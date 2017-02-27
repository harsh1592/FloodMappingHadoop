#include <iostream>
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include <gdal_priv.h>
#include <gdal_alg.h>
#include "gdal.h"
#include "ogrsf_frmts.h"
#include <ogr_spatialref.h>

using namespace cv;
using namespace std;


void removeSmallBlobs(cv::Mat& im, double size);

Mat imgOriginal, imgThresholded;

int main(int argc, char** argv)
{
	const char *pszFormat = "ESRI Shapefile";
	GDALDriver *gdalDriver = GetGDALDriverManager()->GetDriverByName(pszFormat);

	GDALDataset  *inputDS, *outputDS, *poDstDS, *conversionDS;
	GDALAllRegister();
	OGRRegisterAll();

	imgOriginal = imread("wms3.tiff", IMREAD_LOAD_GDAL | IMREAD_ANYDEPTH | IMREAD_COLOR);

	if (imgOriginal.empty()) //check whether the image is loaded or not
	{
		cout << "Error : Image cannot be loaded..!!" << endl;
		system("pause"); //wait for a key press
		return -1;
	}
	Size size((imgOriginal.cols)/1.5, (imgOriginal.rows)/1.5);
	resize(imgOriginal, imgOriginal, size);
	namedWindow("Control", CV_WINDOW_AUTOSIZE); //create a window called "Control"

	while (true)
	{
		Mat imgHSV;
		cvtColor(imgOriginal, imgHSV, COLOR_BGR2HSV); //Convert the captured frame from BGR to HSV
		
		//Threshold the image based on predifined values
		inRange(imgHSV, Scalar(13, 32, 43), Scalar(53, 126, 135), imgThresholded); 
		
		//morphological opening 
		morphologyEx(imgThresholded, imgThresholded, 2, getStructuringElement(MORPH_ELLIPSE, Size(3, 3)));
		//morphological closing
		morphologyEx(imgThresholded, imgThresholded, 3, getStructuringElement(MORPH_ELLIPSE, Size(7, 7)));

		
		// Area opening on the morphologically processed image
		removeSmallBlobs(imgThresholded, 255);
		//bitwise_not(imgThresholded, imgThresholded);

	
		imwrite("out6.tif", imgThresholded);
		//imshow("Thresholded Image", imgThresholded);
		
		//---------------------GDAL FUNCTIONS-------------------------------

		inputDS = (GDALDataset *)GDALOpen("wms3.tiff", GA_ReadOnly);
		double geoTransform[6];
		double minx, miny, maxx, maxy;
		if (inputDS->GetGeoTransform(geoTransform) == CE_None)
		{
			double width = inputDS->GetRasterXSize();
			double height = inputDS->GetRasterYSize();
			minx = geoTransform[0];
			maxx = geoTransform[0] + width*geoTransform[1] +
				height*geoTransform[2];
			miny = geoTransform[3] + width*geoTransform[4] +
				height*geoTransform[5];
			maxy = geoTransform[3];
			cout << "minx " << minx << endl;
			cout << "miny " << miny << endl;
			cout << "maxx " << maxx << endl;
			cout << "maxy " << maxy << endl;
			cout << "center x " << geoTransform[0] + .5*width*geoTransform[1] +
			.5*height*geoTransform[2] << endl;
			cout << "center y " << geoTransform[3] + 2*width*geoTransform[4] +
			2*height*geoTransform[5] << endl;
		}
		ostringstream translateString, polygonizeString;
		translateString << "gdal_translate -a_nodata 0 -of GTiff -a_srs EPSG:4326 -a_ullr "
			<< minx << " " << maxy << " " << maxx << " " << miny << " out6.tif wms3.tiff";
		string command = translateString.str();
		const char* tCmd = command.c_str();
		system(tCmd);

		outputDS = (GDALDataset *)GDALOpen("output3.tiff", GA_ReadOnly);
		GDALRasterBand *poBandR, *poBandMask;
		poBandR = outputDS->GetRasterBand(1);
		poBandR->CreateMaskBand(GMF_PER_DATASET);
		poBandMask = poBandR->GetMaskBand();
		poBandMask->Fill(10, 0);
		const char *pszDriverName = "ESRI Shapefile";
		OGRSFDriver *poDriver;
		poDriver =
			OGRSFDriverRegistrar::GetRegistrar()->GetDriverByName(pszDriverName);
		
		OGRDataSource* ds = poDriver->CreateDataSource("new shape", NULL);
		OGRLayer* poLayer = ds->CreateLayer("myfile", NULL, wkbMultiPolygon, NULL);
		CPLErr er = GDALPolygonize(poBandR, poBandMask, poLayer
			,-1, NULL, NULL, NULL);

		poLayer->SyncToDisk();
		OGRDataSource::DestroyDataSource(ds);
		
		cout << "done";
		int y;
		cin >> y;
		
		
		//imshow("Thresholded Image", imgThresholded); //show the thresholded image
		//imshow("Original", imgOriginal); //show the original image
		//imwrite("alpha5.jpg", imgThresholded);
		cout << "done.!!" << endl;
		if (waitKey(30) == 27) //wait for 'esc' key press for 30ms. If 'esc' key is pressed, break loop
		{
			cout << "esc key is pressed by user" << endl;
			break;
		}
	}

}

/**
* Replacement for Matlab's bwareaopen()
* Input image must be 8 bits, 1 channel, black and white (objects)
* with values 0 and 255 respectively
*/
void removeSmallBlobs(Mat& im, double size)
{
	// Only accept CV_8UC1
	if (im.channels() != 1 || im.type() != CV_8U)
		return;

	// Find all contours
	vector<vector<Point> > contours;
	findContours(im.clone(), contours, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_SIMPLE);

	for (int i = 0; i < contours.size(); i++)
	{
		// Calculate contour area
		double area = contourArea(contours[i]);

		// Remove small objects by drawing the contour with black color
		if (area > 0 && area <= size)
			drawContours(im, contours, i, CV_RGB(0, 0, 0), -1);
	}
}