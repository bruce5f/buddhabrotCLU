#include "gd.h"
#include <png.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <math.h>
#include <complex.h>

#define CPXPLN_SIZE (3.0)
#define CPXPLN_xSTART (-2.0)
#define CPXPLN_ySTART (-1.5)
#define CPXPLN_ORIGIN (float _Complex)(0.0 + 0.0*_Complex_I)
#define CHECKS (2500000)
#define DX (CPXPLN_SIZE/sqrt((float)CHECKS))

#define MAX_DEPTH (1000000)
#define MAX_POINTS (10000000)
#define MAX_RES (10000)

struct Buddha {
	int minDepth, maxDepth, pointCount, plotDepth;
	char filename[100];
	float _Complex points[MAX_POINTS];
	int grid[MAX_RES][MAX_RES];
} buddha;

void saveBuddha();
void loadBuddha(char filename[]);
void findPoints(int goal);
void savePointstoFile(char fileName[]);
void generateBuddhabrotPNG(float exponent, float brightness, int resolution);
int alphaToHex(float alpha);
int escapeDepth(double _Complex c, double _Complex z);

int main(int argc, char* argv[]) {
	
	for (int i = 0; i < MAX_POINTS; i++) {
		buddha.points[i] = 0.0 + 0.0*_Complex_I;
	}
	for (int i = 0; i < MAX_RES; i++) {
		for (int j = 0; j < MAX_RES; j++) {
			buddha.grid[i][j] = 0;		
		}	
	}
	
	srandom(time(NULL));
	
	buddha.minDepth = 10000;
	buddha.maxDepth = 100000;
	buddha.pointCount = 0;
	buddha.plotDepth = MAX_DEPTH;
	strcpy(buddha.filename, "defaultBuddha");

	opterr = 0;
	int argi = 0, pointCountGoal = 100;
	float exponent = 1.0, brightness = 0.2, resolution = 1000;
	
	while ((argi = getopt (argc, argv, "d:D:g:j:o:r:e:b:h")) != -1) {
		switch (argi) {
			case 'd':
				buddha.minDepth = atoi(optarg);
			break;
			case 'D':
				buddha.maxDepth = atoi(optarg);
			break;
			case 'j':
				buddha.plotDepth = atoi(optarg);
			break;
			case 'g':
				pointCountGoal = atoi(optarg);
			break;
			case 'o':
				sprintf(buddha.filename, "%s", optarg);
			break;
			case 'r':
				resolution = atoi(optarg);
			break;
			case 'e':
				exponent = atof(optarg);
			break;
			case 'b':
				brightness = atof(optarg);
			break;
			case 'h':
				printf("\n*-------------------------------*\n\tBuddhabrot CLU Options\n\nFor finding deep points...\ng : goal number of points to find\nd : minimum iteration depth\nD : maximum iteration depth\n\nFor generating Buddhabrot...\nj : iteration depth for plotting escape points (default is MAX_DEPTH)\no : output image filename (example : 'buddha')\nr : output image resolution (example : 1000) \ne : exponent (best near 1)\n\t<1 : emphasizes points hit less frequently\n\t>1 : emphasizes points hit more frequently\nb : overall brightness\n*-------------------------------*\n\n");
			return 0;
			break;
			case '?':
				printf("Problem : %s\n", argv[optind]);
				return 1;
			default:
			break;
		}
	}

	printf("Finding deep points\n");

	findPoints(pointCountGoal);
	
	generateBuddhabrotPNG(exponent, brightness, resolution);
	
	return 0;

}

void generateBuddhabrotPNG(float exponent, float brightness, int resolution) {
	
	printf("Generating Buddhabrot image\n");


	gdImagePtr im;
		FILE *fp;
	im = gdImageCreateTrueColor(resolution, resolution);
	
	//populate grid
	for (int i = 0; i < buddha.pointCount; i++) {
		float _Complex c = buddha.points[i], zNext = 0.0+0.0*_Complex_I;
		for (int j = 0; j < buddha.plotDepth; j++) {
			
			zNext = zNext*zNext + c;
			if (cabs(zNext) > 2.0) break;
			
			//map next z to point on output grid
			int xMapped = (int)((creal(zNext) - CPXPLN_xSTART)/CPXPLN_SIZE * resolution);
			int yMapped = (int)((cimag(zNext) - CPXPLN_ySTART)/CPXPLN_SIZE * resolution);

			if (xMapped > 0 && xMapped < resolution && yMapped > 0 && yMapped < resolution)
				buddha.grid[xMapped][yMapped]++;
		}
	}
	
	//collect data about grid
	float total = 0.0, average = 0.0, max = 0.0, greaterThan1 = 0.0;
	for (int x = 0; x < resolution; x++) {
		for (int y = 0; y < resolution; y++) {
			int current = buddha.grid[x][y];
			if (current > 0) {
				greaterThan1++;
				total += buddha.grid[x][y];
				if (buddha.grid[x][y] > max)
					max = buddha.grid[x][y];
			}
		}
	}
	average = total/greaterThan1;
	
	//translate grid to image
	for (int x = 0; x < resolution; x++) {
		for (int y = 0; y < resolution; y++) {
			
			//Average values are normalized to 1, that way lower than average values 
			float value = alphaToHex(1.0 - pow(buddha.grid[x][y]/average, exponent)*brightness);
			
			gdImageSetPixel(im, y, x, value);

		}
	}
	
	
	
	char pngFileName[105];
	sprintf(pngFileName, "%s.png", buddha.filename);
	fp = fopen(pngFileName, "wb");
	if (!fp) {
		fprintf(stderr, "Can't save png image.\n");
		gdImageDestroy(im);
		return;
	}
	gdImagePng(im, fp);

	fclose(fp);

	gdImageDestroy(im);
}


int alphaToHex(float alpha) {
	if (alpha > 1.0) alpha = 1.0;
	else if (alpha < 0.0) alpha = 0.0;
	int val = 0xFF*alpha;
	char hex[15];
	sprintf(hex, "0x00%02X%02X%02X", val,val,val);
	return strtol(hex, NULL, 0);
}


//sweep plane. Works faster than total randomness and ensures even distribution of points.
void findPoints(int goal) {
	
	int count = buddha.pointCount;
	float _Complex centerCircleBulb = -1.0 + 0.0*_Complex_I;
	for (int i = 0; i < INT_MAX && count < goal; i++) {
		
		for (float a = CPXPLN_xSTART; a < (CPXPLN_xSTART + CPXPLN_SIZE); a+=DX) {	
			//only check half of the complex axis since BB is symmetrical across the real axis
          		  for (float bi = CPXPLN_ySTART; bi < (CPXPLN_ySTART + CPXPLN_SIZE); bi+=DX) {
			
				//give it a little randomness
				double randomAngle = 2*M_PI*((float)random()/RAND_MAX), randomRadius = DX*((float)random()/RAND_MAX);
			
				float _Complex c = ( a + randomRadius*cos(randomAngle) ) + ( bi + randomRadius*sin(randomAngle) )*_Complex_I, zinit = 0.0 + 0.0*_Complex_I;
	
				int cTest = ((cabs(c - centerCircleBulb) > 0.25) && (cabs(1.0-csqrt(1-(4*c))) > 1.0));
				int ed = escapeDepth(c,zinit);
		                int onBorder = (cTest && ed >= buddha.minDepth && ed <= buddha.maxDepth );
			        if (onBorder) {
					buddha.points[count] = c;
					count++;
					if (count >= goal) break;
               			}
            		}
			printf("\rFound %i/%i points (%i%%)", count, goal, 100*(count)/goal);

			if (count >= goal) break;
		}	
	}
       


	buddha.pointCount = count;
    	printf("Border points saved to array. %i numbers found\n", count);

}


int escapeDepth(double _Complex c, double _Complex z) {

	double _Complex zPrevPow2 = c;
	int i = 0, checkStep = 1;
	for (; i < MAX_DEPTH; i++) {

		
		//check if its in the main cardiod or circle
		float _Complex centerCircleBulb = -1.0 + 0.0*_Complex_I;
		if ((cabs(c - centerCircleBulb) < 0.25) || (cabs(1.0-csqrt(1-(4*c))) < 1.0)) {
			return -1;
		}

		if (cabs(z) > 2.0) {
			return i;
		}


		//cycle search to make it faster
		if (i > checkStep) {
			if (cabs(z-zPrevPow2) < 0.00000001) {
				return -1;
			}

			
			if (i == checkStep*2) {
				checkStep*=2;
				zPrevPow2 = z;
			}
		}

		z = z*z + c;
	}

	return -1;
}
