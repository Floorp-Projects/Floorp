/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

//we need openfilename stuff... MMP
#ifdef WIN32_LEAN_AND_MEAN
#undef WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include "prtypes.h"
#include <stdio.h>
#include "resources.h"
#include "nsIImageManager.h"
#include "nsIImageGroup.h"
#include "nsIImageRequest.h"
#include "nsIImageObserver.h"
#include "nsIRenderingContext.h"
#include "nsIDeviceContext.h"
#include "nsIImage.h"
#include "nsIWidget.h"
#include "nsGUIEvent.h"
#include "nsRect.h"
#include "nsWidgetsCID.h"
#include "nsGfxCIID.h"
#include "nsFont.h"
#include "nsWidgetsCID.h" 
#include "nsITextWidget.h"
#include "nsIBlender.h"

// widget interface
static NS_DEFINE_IID(kITextWidgetIID,     NS_ITEXTWIDGET_IID);
static NS_DEFINE_IID(kCTextFieldCID, NS_TEXTFIELD_CID);


static NS_DEFINE_IID(kIWidgetIID, NS_IWIDGET_IID);
static NS_DEFINE_IID(kIImageObserverIID, NS_IIMAGEREQUESTOBSERVER_IID);
static NS_DEFINE_IID(kCWindowIID, NS_WINDOW_CID);
static NS_DEFINE_IID(kCChildWindowIID, NS_CHILD_CID);
static NS_DEFINE_IID(kCScrollbarIID, NS_VERTSCROLLBAR_CID);

static char* class1Name = "ImageTest";

static HANDLE           gInstance, gPrevInstance;
static nsIImageManager  *gImageManager = nsnull;
static nsIImageGroup    *gImageGroup = nsnull;
static nsIImageRequest  *gImageReq = nsnull;
static HWND             gHwnd;
static nsIWidget        *gWindow = nsnull;
static nsIImage         *gImage = nsnull;
static nsIImage         *gBlendImage = nsnull;
static nsIImage         *gMaskImage = nsnull;
static PRBool           gInstalledColorMap = PR_FALSE;
static PRInt32          gXOff,gYOff;
static nsITextWidget    *gBlendMessage;
static nsITextWidget    *gQualMessage;
static nsIBlender       *gImageblender;
static PRBool           gCanBlend = PR_FALSE;
static HBITMAP          gDobits,gSobits,gSrcbits,gDestbits;
static HDC              gDestdc,gSrcdc,gScreendc;

extern void     Compositetest(nsIBlender *aBlender,nsIImage *aImage,
                              PRInt32 aSX, PRInt32 aSY, PRInt32 aWidth,PRInt32 aHeight,
                              PRInt32 aDX,PRInt32 aDY,
                              float aBlendAmount,PRBool aBuff);
extern void     InterActiveBlend(nsIBlender *aBlender,nsIImage *aImage);
extern PRInt32  speedtest(nsIImage *aTheImage,nsIRenderingContext *aSurface, PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight);
extern PRInt32  drawtest(nsIRenderingContext *aSurface);
extern PRInt32  filltest(nsIRenderingContext *aSurface);
extern PRInt32  arctest(nsIRenderingContext *aSurface);
extern PRBool   IsImageLoaded();
extern void     SetUpBlend();
extern void     CleanUpBlend();
extern nsresult BuildDIB(LPBITMAPINFOHEADER  *aBHead,unsigned char **aBits,PRInt32 aWidth, PRInt32 aHeight, PRInt32 aDepth);
extern PRInt32  CalcBytesSpan(PRUint32  aWidth,PRUint32  aBitsPixel);



//------------------------------------------------------------


class MyBlendObserver : public nsIImageRequestObserver 
{
private:
  nsIImage      **mImage;     // address of pointer to put info into

public:
  MyBlendObserver(nsIImage **aImage);
  ~MyBlendObserver();
 
  NS_DECL_ISUPPORTS

  virtual void Notify(nsIImageRequest *aImageRequest,nsIImage *aImage,
                      nsImageNotification aNotificationType,
                      PRInt32 aParam1, PRInt32 aParam2,void *aParam3);

  virtual void NotifyError(nsIImageRequest *aImageRequest,nsImageError aErrorType);

};

//------------------------------------------------------------

MyBlendObserver::MyBlendObserver(nsIImage **aImage)
{
  mImage = aImage;
}

//------------------------------------------------------------

MyBlendObserver::~MyBlendObserver()
{
}

//------------------------------------------------------------

NS_IMPL_ISUPPORTS(MyBlendObserver, kIImageObserverIID)

void  
MyBlendObserver::Notify(nsIImageRequest *aImageRequest,
                   nsIImage *aImage,
                   nsImageNotification aNotificationType,
                   PRInt32 aParam1, PRInt32 aParam2,
                   void *aParam3)
{
    switch (aNotificationType) 
      {
       case nsImageNotification_kDimensions:
       break;
       case nsImageNotification_kPixmapUpdate:
       case nsImageNotification_kImageComplete:
       case nsImageNotification_kFrameComplete:
        {
        if (*mImage == nsnull && aImage) 
          {
          *mImage = aImage;
          NS_ADDREF(aImage);
          }

        if ( gBlendImage && (aNotificationType == nsImageNotification_kImageComplete) && gImage )
          {
          SetUpBlend();
          }
       }
       break;
    }
}

//------------------------------------------------------------

void 
MyBlendObserver::NotifyError(nsIImageRequest *aImageRequest,
                        nsImageError aErrorType)
{
  ::MessageBox(NULL, "Blend Image loading error!",class1Name, MB_OK);
}

//------------------------------------------------------------

class MyObserver : public nsIImageRequestObserver 
{
public:
  MyObserver();
  ~MyObserver();
 
  NS_DECL_ISUPPORTS

  virtual void Notify(nsIImageRequest *aImageRequest,nsIImage *aImage,
                      nsImageNotification aNotificationType,
                      PRInt32 aParam1, PRInt32 aParam2,void *aParam3);

  virtual void NotifyError(nsIImageRequest *aImageRequest,nsImageError aErrorType);

};

//------------------------------------------------------------

MyObserver::MyObserver()
{
}

//------------------------------------------------------------

MyObserver::~MyObserver()
{
}

//------------------------------------------------------------

NS_IMPL_ISUPPORTS(MyObserver, kIImageObserverIID)

void  
MyObserver::Notify(nsIImageRequest *aImageRequest,
                   nsIImage *aImage,
                   nsImageNotification aNotificationType,
                   PRInt32 aParam1, PRInt32 aParam2,
                   void *aParam3)
{
    switch (aNotificationType) 
      {
       case nsImageNotification_kDimensions:
        {
        char buffer[40];
        sprintf(buffer, "Image:%d x %d", aParam1, aParam2);
        ::SetWindowText(gHwnd, buffer);
        }
       break;

       case nsImageNotification_kPixmapUpdate:
       case nsImageNotification_kImageComplete:
       case nsImageNotification_kFrameComplete:
        {
        if (gImage == nsnull && aImage) 
          {
          gImage = aImage;
          NS_ADDREF(aImage);
          }

        if (!gInstalledColorMap && gImage) 
          {
          nsColorMap *cmap = gImage->GetColorMap();

          if (cmap != nsnull && cmap->NumColors > 0) 
            {
            gWindow->SetColorMap(cmap);
            }
          gInstalledColorMap = PR_TRUE;
          }
        nsRect *rect = (nsRect *)aParam3;
        nsIRenderingContext *drawCtx = gWindow->GetRenderingContext();

        if (gImage) 
          {
          drawCtx->DrawImage(gImage, 0, 0, gImage->GetWidth(), gImage->GetHeight());
          }
       }
       break;
    }
}

//------------------------------------------------------------

void 
MyObserver::NotifyError(nsIImageRequest *aImageRequest,
                        nsImageError aErrorType)
{
  ::MessageBox(NULL, "Image loading error!",class1Name, MB_OK);
}

//------------------------------------------------------------

// this just sets up 2 DC's for blending, 
void
SetUpBlend()
{
void              *bits1,*bits2;
nsresult          rv;

  static NS_DEFINE_IID(kBlenderCID, NS_BLENDER_CID);
  static NS_DEFINE_IID(kBlenderIID, NS_IBLENDER_IID);

  gScreendc = ::GetDC(gHwnd);

  // create everything we need from this DC
  gSrcdc = ::CreateCompatibleDC(gScreendc);
  gDestdc = ::CreateCompatibleDC(gScreendc);

  if(gBlendImage && gImage)
    {
    bits1 = gBlendImage->GetBits();
    bits2 = gImage->GetBits();
    gSrcbits = ::CreateDIBitmap(gScreendc,(BITMAPINFOHEADER*)gBlendImage->GetBitInfo(), CBM_INIT, bits1, (LPBITMAPINFO)gBlendImage->GetBitInfo(), DIB_RGB_COLORS);
    gDestbits = ::CreateDIBitmap(gScreendc,(BITMAPINFOHEADER*)gImage->GetBitInfo(), CBM_INIT, bits2, (LPBITMAPINFO)gImage->GetBitInfo(), DIB_RGB_COLORS);
    gSobits = ::SelectObject(gSrcdc, gSrcbits);
    gDobits = ::SelectObject(gDestdc, gDestbits);

    rv = NSRepository::CreateInstance(kBlenderCID, nsnull, kBlenderIID, (void **)&gImageblender);
    gImageblender->Init(gSrcdc,gDestdc);

  }

}

//------------------------------------------------------------

void
CleanUpBlend()
{
  ::SelectObject(gSrcdc, gSobits);
  ::SelectObject(gDestdc,gDobits);

  DeleteDC(gSrcdc);
  DeleteDC(gDestdc);
  DeleteObject(gSrcbits);
  DeleteObject(gDestbits);
  ReleaseDC(gHwnd,gScreendc);

}

//------------------------------------------------------------

void
Restore(nsIBlender *aBlender,nsIImage *aImage)
{
PRUint8             *thebytes,*curbyte,*srcbytes,*cursourcebytes;
PRInt32             w,h,ls,x,y,numbytes,sls;
HDC                 dstdc;
HBITMAP             srcbits,tb1;
BITMAP              srcinfo;
LPBITMAPINFOHEADER  srcbinfo;
nsIRenderingContext *drawCtx = gWindow->GetRenderingContext();

  dstdc = aBlender->GetDstDS();

  aBlender->RestoreImage(dstdc);

  // this takes the Destination DC and copies the information into aImage
  tb1 = CreateCompatibleBitmap(dstdc,3,3);
  srcbits = ::SelectObject(dstdc, tb1);
  numbytes = ::GetObject(srcbits,sizeof(BITMAP),&srcinfo);
  // put into a DIB
  BuildDIB(&srcbinfo,&srcbytes,srcinfo.bmWidth,srcinfo.bmHeight,srcinfo.bmBitsPixel);
  numbytes = ::GetDIBits(dstdc,srcbits,0,srcinfo.bmHeight,srcbytes,(LPBITMAPINFO)srcbinfo,DIB_RGB_COLORS);
  
  ::SelectObject(dstdc,srcbits);
  DeleteObject(tb1);

  // copy the information back into the source
  thebytes = aImage->GetBits();
  h = aImage->GetHeight();
  w = aImage->GetWidth();
  ls = aImage->GetLineStride();
  sls = CalcBytesSpan(srcinfo.bmWidth,srcinfo.bmBitsPixel);
  for(y=0;y<h;y++)
    {
    curbyte = thebytes + (y*ls);
    cursourcebytes = srcbytes + (y*sls);
    for(x=0;x<ls;x++)
      {
      *curbyte = *cursourcebytes;
      curbyte++;
      cursourcebytes++;
      }
    }

  delete [] srcbytes;
  delete srcbinfo;

  drawCtx->DrawImage(aImage, 0, 0, aImage->GetWidth(), aImage->GetHeight());
}

//------------------------------------------------------------

void
InterActiveBlend(nsIBlender *aBlender,nsIImage *aImage)
{
float     blendamount;
nsString  str;
PRInt32   numerror,i,height,width;
POINT     cpos;
DWORD     pos;
MSG       msg;

  if(aBlender && aImage)
    {
    gBlendMessage->GetText(str,3);
    blendamount = (float)(str.ToInteger(&numerror))/100.0f;
    if(blendamount < 0.0)
      blendamount = 0.0f;
    if(blendamount > 1.0)
      blendamount = 1.0f;

    for(i=0;i<200;i++)
      {
      pos = ::GetMessagePos();

      cpos.x = LOWORD(pos);
      cpos.y = HIWORD(pos);
      ::ScreenToClient(gHwnd,&cpos);

      width = gBlendImage->GetWidth();
      height = gBlendImage->GetHeight();
      Compositetest(aBlender,aImage,cpos.x,cpos.y,width,height,0,0,blendamount,PR_TRUE);
      Restore(aBlender,aImage);

      if(GetMessage(&msg,NULL,0,0))
        {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        }
      }
    }
}

//------------------------------------------------------------

void
Compositetest(nsIBlender *aBlender,nsIImage *aImage,
                  PRInt32 aSX, PRInt32 aSY, PRInt32 aWidth,PRInt32 aHeight,
                  PRInt32 aDX, PRInt32 aDY,
                  float aBlendAmount,PRBool aBuff)
{
PRUint8             *thebytes,*curbyte,*srcbytes,*cursourcebytes;
PRInt32             w,h,ls,x,y,numbytes,sls;
HDC                 srcdc,dstdc;
HBITMAP             srcbits,tb1;
BITMAP              srcinfo;
LPBITMAPINFOHEADER  srcbinfo;
nsIRenderingContext *drawCtx = gWindow->GetRenderingContext();


  srcdc = aBlender->GetSrcDS();
  dstdc = aBlender->GetDstDS();

  aBlender->Blend(srcdc,aSX,aSY,aWidth,aHeight,dstdc,0, 0,aBlendAmount,aBuff);

  // this takes the Destination DC and copies the information into aImage
  tb1 = CreateCompatibleBitmap(dstdc,3,3);
  srcbits = ::SelectObject(dstdc, tb1);
  numbytes = ::GetObject(srcbits,sizeof(BITMAP),&srcinfo);
  // put into a DIB
  BuildDIB(&srcbinfo,&srcbytes,srcinfo.bmWidth,srcinfo.bmHeight,srcinfo.bmBitsPixel);
  numbytes = ::GetDIBits(dstdc,srcbits,0,srcinfo.bmHeight,srcbytes,(LPBITMAPINFO)srcbinfo,DIB_RGB_COLORS);

  ::SelectObject(dstdc,srcbits);
  DeleteObject(tb1);


  // copy the information back into the source
  thebytes = aImage->GetBits();
  h = aImage->GetHeight();
  w = aImage->GetWidth();
  ls = aImage->GetLineStride();
  sls = CalcBytesSpan(srcinfo.bmWidth,srcinfo.bmBitsPixel);
  for(y=0;y<h;y++)
    {
    curbyte = thebytes + (y*ls);
    cursourcebytes = srcbytes + (y*sls);
    for(x=0;x<ls;x++)
      {
      *curbyte = *cursourcebytes;
      curbyte++;
      cursourcebytes++;
      }
    }

  delete [] srcbytes;
  delete srcbinfo;

  drawCtx->DrawImage(aImage, 0, 0, aImage->GetWidth(), aImage->GetHeight());
}

//------------------------------------------------------------

// This tests the speed for the bliting,
PRInt32
speedtest(nsIImage *aTheImage,nsIRenderingContext *aSurface, PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight,PRBool aOptimize)
{
PRUint32    min,seconds,milli,i;
SYSTEMTIME  thetime;

  printf("\nSTARTING TIMING TEST\n");
  ::GetSystemTime(&thetime);
  min = thetime.wMinute;
  seconds = thetime.wSecond;
  milli = thetime.wMilliseconds;

  if(aOptimize==PR_TRUE)
    aTheImage->Optimize(aSurface);

  for(i=0;i<200;i++)
    aSurface->DrawImage(aTheImage,aX,aY,aWidth,aHeight);

  ::GetSystemTime(&thetime);
  min = thetime.wMinute-min;
  if(min>0)
    min = min*60;
  seconds = min+thetime.wSecond-seconds;
  if(seconds>0)
    seconds = (seconds*1000)+thetime.wMilliseconds;
  else
    seconds = thetime.wMilliseconds;
  milli=seconds-milli;

  if(aOptimize==PR_TRUE)
    printf("The Optimized Blitting Time was %lu Milliseconds\n",milli);
  else
    printf("The Non-Optimized Blitting Time was %lu Milliseconds\n",milli);

  return(milli);
}

//------------------------------------------------------------

PRInt32
arctest(nsIRenderingContext *aSurface)
{
nsFont  *font;
nscolor white,black;

  font = new nsFont("Times", NS_FONT_STYLE_NORMAL,NS_FONT_VARIANT_NORMAL,NS_FONT_WEIGHT_BOLD,0,12);
  aSurface->SetFont(*font);

    // clear surface
  NS_ColorNameToRGB("white", &white);
  NS_ColorNameToRGB("red", &black);
  aSurface->SetColor(white);
  aSurface->FillRect(0,0,1000,1000);
  aSurface->SetColor(black);

  aSurface->DrawArc(20, 20,50,100,(float)0.0,(float)90.0);
  aSurface->FillArc(70, 20,50,100,(float)0.0,(float)90.0);
  aSurface->DrawArc(150, 20,50,100,(float)90.0,(float)0.0);
  aSurface->DrawString("0 to 90\0",20,8,30);
  aSurface->DrawString("Reverse (eg 90 to 0)\0",120,8,30);

  aSurface->DrawArc(20, 140,100,50,(float)130.0,(float)180.0);
  aSurface->FillArc(70, 140,100,50,(float)130.0,(float)180.0);
  aSurface->DrawArc(150, 140,100,50,(float)180.0,(float)130.0);
  aSurface->DrawString("130 to 180\0",20,130,30);

  aSurface->DrawArc(20, 200,50,100,(float)170.0,(float)300.0);
  aSurface->FillArc(70, 200,50,100,(float)170.0,(float)300.0);
  aSurface->DrawArc(150, 200,50,100,(float)300.0,(float)170.0);
  aSurface->DrawString("170 to 300\0",20,190,30);


  return(30);
}

//------------------------------------------------------------

PRInt32
drawtest(nsIRenderingContext *aSurface)
{
nsFont  *font;
nsPoint *pointlist;
nscolor white,black;

  font = new nsFont("Times", NS_FONT_STYLE_NORMAL,NS_FONT_VARIANT_NORMAL,NS_FONT_WEIGHT_BOLD,0,12);
  aSurface->SetFont(*font);
            

  // clear surface
  NS_ColorNameToRGB("white", &white);
  NS_ColorNameToRGB("black", &black);
  aSurface->SetColor(white);
  aSurface->FillRect(0,0,1000,1000);
  aSurface->SetColor(black);

  aSurface->DrawRect(20, 20, 100, 100);
  aSurface->DrawString("This is a Rectangle\0",20,5,30);

  aSurface->DrawLine(0,0,300,400);

  pointlist = new nsPoint[6];
  pointlist[0].x = 150;pointlist[0].y = 150;
  pointlist[1].x = 200;pointlist[1].y = 150;
  pointlist[2].x = 190;pointlist[2].y = 170;
  pointlist[3].x = 210;pointlist[3].y = 190;
  pointlist[4].x = 175;pointlist[4].y = 175;
  pointlist[5].x = 150;pointlist[5].y = 150;
  aSurface->DrawPolygon(pointlist,6);
  aSurface->DrawString("This is a closed Polygon\0",210,150,30);
  delete [] pointlist;

#ifdef WINDOWSBROKEN
  pointlist = new nsPoint[5];
  pointlist[0].x = 200;pointlist[0].y = 200;
  pointlist[1].x = 250;pointlist[1].y = 200;
  pointlist[2].x = 240;pointlist[2].y = 220;
  pointlist[3].x = 260;pointlist[3].y = 240;
  pointlist[4].x = 225;pointlist[4].y = 225;
  aSurface->DrawPolygon(pointlist,6);
  aSurface->DrawString("This is an open Polygon\0",250,200,30);
  delete [] pointlist;
#endif  

  aSurface->DrawEllipse(30, 150,50,100);
  aSurface->DrawString("This is an Ellipse\0",30,140,30);


  return(30);
}

//------------------------------------------------------------

PRInt32
filltest(nsIRenderingContext *aSurface)
{
nsFont  *font;
nsPoint *pointlist;
nscolor white,black;

  font = new nsFont("Times", NS_FONT_STYLE_NORMAL,NS_FONT_VARIANT_NORMAL,NS_FONT_WEIGHT_BOLD,0,12);
  aSurface->SetFont(*font);

    // clear surface
  NS_ColorNameToRGB("white", &white);
  NS_ColorNameToRGB("black", &black);
  aSurface->SetColor(white);
  aSurface->FillRect(0,0,1000,1000);
  aSurface->SetColor(black);

  aSurface->FillRect(20, 20, 100, 100);
  aSurface->DrawString("This is a Rectangle\0",20,5,30);

  pointlist = new nsPoint[6];
  pointlist[0].x = 150;pointlist[0].y = 150;
  pointlist[1].x = 200;pointlist[1].y = 150;
  pointlist[2].x = 190;pointlist[2].y = 170;
  pointlist[3].x = 210;pointlist[3].y = 190;
  pointlist[4].x = 175;pointlist[4].y = 175;
  pointlist[5].x = 150;pointlist[5].y = 150;
  aSurface->FillPolygon(pointlist,6);
  aSurface->DrawString("This is a closed Polygon\0",210,150,30);
  delete [] pointlist;


#ifdef WINDOWSBROKEN
  pointlist = new nsPoint[5];
  pointlist[0].x = 200;pointlist[0].y = 200;
  pointlist[1].x = 250;pointlist[1].y = 200;
  pointlist[2].x = 240;pointlist[2].y = 220;
  pointlist[3].x = 260;pointlist[3].y = 240;
  pointlist[4].x = 225;pointlist[4].y = 225;
  aSurface->FillPolygon(pointlist,6);
  aSurface->DrawString("This is an open Polygon\0",250,200,30);
  delete [] pointlist;
#endif

  aSurface->FillEllipse(30, 150,50,100);
  aSurface->DrawString("This is an Ellipse\0",30,140,30);

  return(30);
}

//------------------------------------------------------------

nsEventStatus PR_CALLBACK
MyHandleEvent(nsGUIEvent *aEvent)
{
  nsEventStatus result = nsEventStatus_eConsumeNoDefault;
  switch(aEvent->message) 
    {
      case NS_PAINT:
        {
        // paint the background
        nsIRenderingContext *drawCtx = ((nsPaintEvent*)aEvent)->renderingContext;
        drawCtx->SetColor(aEvent->widget->GetBackgroundColor());
        drawCtx->FillRect(*(((nsPaintEvent*)aEvent)->rect));
        if (gImage) 
          {
          drawCtx->DrawImage(gImage, 0, 0, gImage->GetWidth(), gImage->GetHeight());
          }
        return nsEventStatus_eConsumeNoDefault;
        }
      break;
    }

  return nsEventStatus_eIgnore;
}

//------------------------------------------------------------

void
MyReleaseImages()
{
  if (gImageReq) 
    {
    NS_RELEASE(gImageReq);
    gImageReq = NULL;
    }
  if (gImage) 
    {
    NS_RELEASE(gImage);
    gImage = NULL;
    }

  gInstalledColorMap = PR_FALSE;
}

//------------------------------------------------------------

void
MyInterrupt()
{
  if (gImageGroup) 
    {
    gImageGroup->Interrupt();
    }
}

//------------------------------------------------------------

#define FILE_URL_PREFIX "file://"

void
MyLoadImage(char *aFileName,PRBool  aGenLoad,nsIImage **aTheImage)
{
char fileURL[256];
char *str;

    MyInterrupt();
    if(aGenLoad!=PR_TRUE)
      MyReleaseImages();

    if (gImageGroup == NULL) 
      {
      nsIRenderingContext *drawCtx = gWindow->GetRenderingContext();
      if (NS_NewImageGroup(&gImageGroup) != NS_OK || gImageGroup->Init(drawCtx) != NS_OK) 
        {
        ::MessageBox(NULL, "Couldn't create image group",class1Name, MB_OK);
        NS_RELEASE(drawCtx);
        return;
        }
      NS_RELEASE(drawCtx);
      }

    strcpy(fileURL, FILE_URL_PREFIX);
    strcpy(fileURL + strlen(FILE_URL_PREFIX), aFileName);

    str = fileURL;
    while ((str = strchr(str, '\\')) != NULL)
        *str = '/';

    nscolor white;

    // set up which image gets loaded
    if(aGenLoad == PR_TRUE)
      {
      MyBlendObserver *observer = new MyBlendObserver(aTheImage);
      NS_ColorNameToRGB("white", &white);
      gImageReq = gImageGroup->GetImage(fileURL,observer,white, 0, 0, 0);
      }
    else
      {
      MyObserver *observer = new MyObserver();
      NS_ColorNameToRGB("white", &white);
      gImageReq = gImageGroup->GetImage(fileURL,observer,white, 0, 0, 0);
      }
            
    if (gImageReq == NULL) 
      {
      ::MessageBox(NULL, "Couldn't create image request",class1Name, MB_OK);
      }
}

//------------------------------------------------------------

PRBool
OpenFileDialog(char *aBuffer, int32 aBufLen)
{
    BOOL result = FALSE;
    OPENFILENAME ofn;

    // *.js is the standard File Name on the Save/Open Dialog
    ::strcpy(aBuffer, "*.gif;*.png;*.jpg;*.jpeg");

    // fill the OPENFILENAME sruct
    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = gHwnd;
    ofn.hInstance = gInstance;
    ofn.lpstrFilter = "All Images (*.gif,*.png,*.jpg,*.jpeg)\0*.gif;*png;*.jpg;*.jpeg\0GIF Files (*.gif)\0*.gif\0PNG Files (*.png)\0*.png\0JPEG Files (*.jpg,*.jpeg)\0*.jpg;*.jpeg\0All Files\0*.*\0\0";
    ofn.lpstrCustomFilter = NULL; 
    ofn.nMaxCustFilter = 0;
    ofn.nFilterIndex = 1; // the first one in lpstrFilter
    ofn.lpstrFile = aBuffer; // contains the file path name on return
    ofn.nMaxFile = aBufLen;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL; // use default
    ofn.lpstrTitle = NULL; // use default
    ofn.Flags = OFN_CREATEPROMPT | OFN_HIDEREADONLY | OFN_PATHMUSTEXIST;
    ofn.nFileOffset = 0; 
    ofn.nFileExtension = 0;
    ofn.lpstrDefExt = "gif"; // default extension is .js
    ofn.lCustData = NULL; 
    ofn.lpfnHook = NULL;
    ofn.lpTemplateName = NULL;

    // call the open file dialog or the save file dialog according to aIsOpenDialog
    result = ::GetOpenFileName(&ofn);

    return (PRBool)result;
}

//------------------------------------------------------------

PRBool 
IsImageLoaded()
{

  if(gImageGroup == NULL)  // go get an image for the window
    {
    char szFile[256];

    ::MessageBox(NULL, "Need To Open an Image",NULL, MB_OK);

    if (!OpenFileDialog(szFile, 256))
        return PR_FALSE;
    MyLoadImage(szFile,PR_FALSE,NULL);
    }
  return PR_TRUE;
}

//------------------------------------------------------------

long PASCAL
WndProc(HWND hWnd, UINT msg, WPARAM param, LPARAM lparam)
{
HMENU     hMenu;
char      szFile[256];
float     blendamount;
nsString  str;
PRInt32   numerror;

  switch (msg) 
    {
    case WM_COMMAND:
      hMenu = GetMenu(hWnd);
      switch (LOWORD(param)) 
        {
        case TIMER_OPEN: 
          if (OpenFileDialog(szFile, 256))
            MyLoadImage(szFile,PR_FALSE,NULL);
        case OPENBLD:
          if (OpenFileDialog(szFile, 256))
            {
            if (gBlendImage!=nsnull) 
              {
              NS_RELEASE(gBlendImage);
              gBlendImage = nsnull;
              }
            MyLoadImage(szFile,PR_TRUE,&gBlendImage);
            }
          break;          
        case TIMER_EXIT:
          ::DestroyWindow(hWnd);
          exit(0);
          break;
        case RDMSK:           // read mask
            if (gMaskImage!=nsnull) 
              {
              NS_RELEASE(gMaskImage);
              gMaskImage = nsnull;
              }
            if (OpenFileDialog(szFile, 256))
              MyLoadImage(szFile,PR_TRUE,&gMaskImage);
          break;
        case COMPRESET:
            Restore(gImageblender,gImage);
          break;
        case COMPINT:         // compostite interactive
          InterActiveBlend(gImageblender,gImage);
          break;
        case COMPTST:         // composite
        case COMPTSTSPEED:    // composit speed test
          IsImageLoaded();
          gBlendMessage->GetText(str,3);
          blendamount = (float)(str.ToInteger(&numerror))/100.0f;
          if(blendamount < 0.0)
            blendamount = 0.0f;
          if(blendamount > 1.0)
            blendamount = 1.0f;

          Compositetest(gImageblender,gImage,0,0,0,0,0,0,blendamount,PR_TRUE);
          break;
        case BSTNOOPT:
        case BSTOPT:
          IsImageLoaded();
          if(gImage)
            {
            PRBool opt;
            nsIRenderingContext *drawCtx = gWindow->GetRenderingContext();

            if(LOWORD(param) == BSTNOOPT)
              opt = PR_FALSE;
            else
              opt = PR_TRUE;
            speedtest(gImage,drawCtx, 0, 0, gImage->GetWidth(), gImage->GetHeight(),opt);
            }
          break;
        case DRAWTEST:
            if(gWindow)
              {
              nsIRenderingContext *drawCtx = gWindow->GetRenderingContext();
              drawtest(drawCtx);
              }
          break;
        case FILLTEST:
            if(gWindow)
              {
              nsIRenderingContext *drawCtx = gWindow->GetRenderingContext();
              filltest(drawCtx);
              }
          break;
        case ARCTEST:
            if(gWindow)
              {
              nsIRenderingContext *drawCtx = gWindow->GetRenderingContext();
              arctest(drawCtx);
              }
          break;
        default:
            break;
      }
      break;

    case WM_CREATE:
      // Initialize image library
      if (NS_NewImageManager(&gImageManager) != NS_OK||gImageManager->Init() != NS_OK) 
        {
        ::MessageBox(NULL, "Can't initialize the image library",class1Name, MB_OK);
        }	  
      gImageManager->SetCacheSize(1024*1024);
      break;
    case WM_DESTROY:
      MyInterrupt();
      MyReleaseImages();
      CleanUpBlend();
      
      if (gImageGroup != nsnull) 
        {
        NS_RELEASE(gImageGroup);
        }
      if (gImageManager != nsnull) 
        {
        NS_RELEASE(gImageManager);
        }
      if (gBlendImage) 
        {
        NS_RELEASE(gBlendImage);
        gBlendImage = NULL;
        }

      PostQuitMessage(0);
      break;

    default:
      break;
    }

  return DefWindowProc(hWnd, msg, param, lparam);
}

//------------------------------------------------------------

static HWND CreateTopLevel(const char* clazz, const char* title,int aWidth, int aHeight)
{

  // Create a simple top level window
  HWND window = ::CreateWindowEx(WS_EX_CLIENTEDGE,
                                 clazz, title,
                                 WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,
                                 CW_USEDEFAULT, CW_USEDEFAULT,
                                 aWidth, aHeight,
                                 HWND_DESKTOP,
                                 NULL,
                                 gInstance,
                                 NULL);

  nsRect rect(0, 0, aWidth, aHeight);  

  nsresult rv = NSRepository::CreateInstance(kCChildWindowIID, NULL, kIWidgetIID, (void**)&gWindow);


  if (NS_OK == rv) 
    {
    gWindow->Create((nsNativeWidget)window, rect, MyHandleEvent, NULL);
    }

  // something for input
  NSRepository::RegisterFactory(kCTextFieldCID, "raptorwidget.dll", PR_FALSE, PR_FALSE);
  rect.SetRect(25, 370, 40, 25);  
  NSRepository::CreateInstance(kCTextFieldCID, nsnull, kITextWidgetIID, (void**)&gBlendMessage);
  gBlendMessage->Create(gWindow, rect, nsnull, nsnull);
  gBlendMessage->SetText("50");

  rect.SetRect(70,370,40,25);
  NSRepository::CreateInstance(kCTextFieldCID, nsnull, kITextWidgetIID, (void**)&gQualMessage);
  gQualMessage->Create(gWindow, rect, nsnull, nsnull);
  gQualMessage->SetText("3");

  ::ShowWindow(window, SW_SHOW);
  ::UpdateWindow(window);
  return window;
}

#define WIDGET_DLL "raptorwidget.dll"
#define GFXWIN_DLL "raptorgfxwin.dll"

//------------------------------------------------------------

int PASCAL
WinMain(HANDLE instance, HANDLE prevInstance, LPSTR cmdParam, int nCmdShow)
{
  gInstance = instance;

  NSRepository::RegisterFactory(kCWindowIID, WIDGET_DLL, PR_FALSE, PR_FALSE);
  NSRepository::RegisterFactory(kCChildWindowIID, WIDGET_DLL, PR_FALSE, PR_FALSE);
  NSRepository::RegisterFactory(kCScrollbarIID, WIDGET_DLL, PR_FALSE, PR_FALSE);

  static NS_DEFINE_IID(kCRenderingContextIID, NS_RENDERING_CONTEXT_CID);
  static NS_DEFINE_IID(kCDeviceContextIID, NS_DEVICE_CONTEXT_CID);
  static NS_DEFINE_IID(kCFontMetricsIID, NS_FONT_METRICS_CID);
  static NS_DEFINE_IID(kCImageIID, NS_IMAGE_CID);
  static NS_DEFINE_IID(kCBlenderIID, NS_BLENDER_CID);

  NSRepository::RegisterFactory(kCRenderingContextIID, GFXWIN_DLL, PR_FALSE, PR_FALSE);
  NSRepository::RegisterFactory(kCDeviceContextIID, GFXWIN_DLL, PR_FALSE, PR_FALSE);
  NSRepository::RegisterFactory(kCFontMetricsIID, GFXWIN_DLL, PR_FALSE, PR_FALSE);
  NSRepository::RegisterFactory(kCImageIID, GFXWIN_DLL, PR_FALSE, PR_FALSE);
  NSRepository::RegisterFactory(kCBlenderIID, GFXWIN_DLL, PR_FALSE, PR_FALSE);

  if (!prevInstance) {
    WNDCLASS wndClass;
    wndClass.style = 0;
    wndClass.lpfnWndProc = WndProc;
    wndClass.cbClsExtra = 0;
    wndClass.cbWndExtra = 0;
    wndClass.hInstance = gInstance;
    wndClass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
    wndClass.hbrBackground = (HBRUSH) GetStockObject(LTGRAY_BRUSH);
    wndClass.lpszMenuName = class1Name;
    wndClass.lpszClassName = class1Name;
    RegisterClass(&wndClass);
  }

  // Create our first top level window
  gHwnd = CreateTopLevel(class1Name, "Graphics tester", 620, 400);

  // Process messages
  MSG msg;
  while (GetMessage(&msg, NULL, 0, 0)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
  }
  return msg.wParam;
}

//------------------------------------------------------------

void main(int argc, char **argv)
{
  WinMain(GetModuleHandle(NULL), NULL, 0, SW_SHOW);
}

//------------------------------------------------------------


PRInt32  
CalcBytesSpan(PRUint32  aWidth,PRUint32  aBitsPixel)
{
PRInt32 spanbytes;

  spanbytes = (aWidth * aBitsPixel) >> 5;

	if ((aWidth * aBitsPixel) & 0x1F) 
		spanbytes++;

	spanbytes <<= 2;

  return(spanbytes);
}

//------------------------------------------------------------

nsresult
BuildDIB(LPBITMAPINFOHEADER  *aBHead,unsigned char **aBits,PRInt32 aWidth, PRInt32 aHeight, PRInt32 aDepth)
{
PRInt32   numpalletcolors,imagesize,spanbytes;
PRUint8   *colortable;


	switch (aDepth) 
    {
		case 8:
			numpalletcolors = 256;
      break;
		case 24:
			numpalletcolors = 0;
      break;
		default:
			numpalletcolors = -1;
      break;
    }

  if (numpalletcolors >= 0)
    {
	  (*aBHead) = (LPBITMAPINFOHEADER) new char[sizeof(BITMAPINFOHEADER) + sizeof(RGBQUAD) * numpalletcolors];
    (*aBHead)->biSize = sizeof(BITMAPINFOHEADER);
	  (*aBHead)->biWidth = aWidth;
	  (*aBHead)->biHeight = aHeight;
	  (*aBHead)->biPlanes = 1;
	  (*aBHead)->biBitCount = (unsigned short) aDepth;
	  (*aBHead)->biCompression = BI_RGB;
	  (*aBHead)->biSizeImage = 0;            // not compressed, so we dont need this to be set
	  (*aBHead)->biXPelsPerMeter = 0;
	  (*aBHead)->biYPelsPerMeter = 0;
	  (*aBHead)->biClrUsed = numpalletcolors;
	  (*aBHead)->biClrImportant = numpalletcolors;

    spanbytes = CalcBytesSpan(aWidth,aDepth);

    imagesize = spanbytes * (*aBHead)->biHeight;    // no compression

    // set the color table in the info header
	  colortable = (PRUint8 *)(*aBHead) + sizeof(BITMAPINFOHEADER);

	  memset(colortable, 0, sizeof(RGBQUAD) * numpalletcolors);
    *aBits = new unsigned char[imagesize];
    memset(*aBits, 128, imagesize);
  }

  return NS_OK;
}
