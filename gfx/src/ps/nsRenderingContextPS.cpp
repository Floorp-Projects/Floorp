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


#include "nsRenderingContextPS.h"
#include <math.h>
#include "libimg.h"
#include "nsDeviceContextPS.h"
#include "nsIScriptGlobalObject.h"
#include "prprf.h"
#include "nsPSUtil.h"
#include "nsPrintManager.h"
#include "structs.h" //XXX:PS This should be removed
#include "xlate.h"
#include "xlate_i.h"


static NS_DEFINE_IID(kIRenderingContextIID, NS_IRENDERING_CONTEXT_IID);


// Macro to convert from TWIPS (1440 per inch) to POINTS (72 per inch)
#define NS_TWIPS_TO_POINTS(x) ((x / 20))
#define NS_PIXELS_TO_POINTS(x) (x * 10)
#define NS_PS_RED(x) (((float)(NS_GET_R(x))) / 255.0) 
#define NS_PS_GREEN(x) (((float)(NS_GET_G(x))) / 255.0) 
#define NS_PS_BLUE(x) (((float)(NS_GET_B(x))) / 255.0) 
#define NS_IS_BOLD(x) (((x) >= 500) ? 1 : 0) 




static NS_DEFINE_IID(kRenderingContextIID, NS_IRENDERING_CONTEXT_IID);


void nsRenderingContextPS :: PostscriptColor(nscolor aColor)
{
  XP_FilePrintf(mPrintContext->prSetup->out,"%3.2f %3.2f %3.2f setrgbcolor\n", NS_PS_RED(aColor), NS_PS_GREEN(aColor),
		  NS_PS_BLUE(aColor));
}

void nsRenderingContextPS :: PostscriptFillRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
  xl_moveto(mPrintContext, aX, aY);
  xl_box(mPrintContext, aWidth, aHeight);
}

void nsRenderingContextPS :: PostscriptDrawBitmap(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight, IL_Pixmap *aImage, IL_Pixmap *aMask)
{
  //xl_colorimage(mPrintContext, aX, aY, aWidth, aHeight, IL_Pixmap *image,IL_Pixmap *mask);
}

void nsRenderingContextPS :: PostscriptFont(nscoord aHeight, PRUint8 aStyle, 
											 PRUint8 aVariant, PRUint16 aWeight, PRUint8 decorations)
{
int postscriptFont = 0;

  XP_FilePrintf(mPrintContext->prSetup->out,"%d",NS_TWIPS_TO_POINTS(aHeight));
	//XXX:PS Add bold, italic and other settings here



	switch(aStyle){
	  case NS_FONT_STYLE_NORMAL :
		  if (NS_IS_BOLD(aWeight)) {
		    postscriptFont = 1;   // NORMAL BOLD
		  }
		  else {
	        postscriptFont = 0; // NORMAL
		  }
	  break;

	  case NS_FONT_STYLE_ITALIC:
		  if (NS_IS_BOLD(aWeight)) {		  
		    postscriptFont = 3; // BOLD ITALIC
		  }
		  else {			  
		    postscriptFont = 2; // ITALIC
		  }
	  break;

	  case NS_FONT_STYLE_OBLIQUE:
		  if (NS_IS_BOLD(aWeight)) {	
	        postscriptFont = 7;   // COURIER-BOLD OBLIQUE
		  }
		  else {	
	        postscriptFont = 6;   // COURIER OBLIQUE
		  }
	    break;
	}

	 XP_FilePrintf(mPrintContext->prSetup->out, " f%d\n", postscriptFont);

#if 0
     // The style of font (normal, italic, oblique)
  PRUint8 style;

  // The variant of the font (normal, small-caps)
  PRUint8 variant;

  // The weight of the font (0-999)
  PRUint16 weight;

  // The decorations on the font (underline, overline,
  // line-through). The decorations can be binary or'd together.
  PRUint8 decorations;

  // The size of the font, in nscoord units
  nscoord size; 
#endif

}

void nsRenderingContextPS :: PostscriptTextOut(const char *aString, PRUint32 aLength,
                                    nscoord aX, nscoord aY, nscoord aWidth,
                                    const nscoord* aSpacing, PRBool aIsUnicode)
{
int             ptr = 0;
unsigned int    i;
char            *buf = 0;
nscoord         fontHeight = 0;
nsFont          *font;
nsIFontMetrics  *fMetrics;

  mDelRenderingContext->GetFontMetrics(fMetrics);
  fMetrics->GetHeight(fontHeight);
  fMetrics->GetFont(font);

   //XXX: NGLAYOUT expects font to be positioned based on center.
   // fontHeight / 2 is a crude approximation of this. TODO: use the correct
   // postscript command to offset from center of the text.
  xl_moveto(mPrintContext, aX, aY + (fontHeight / 2));
  if (PR_TRUE == aIsUnicode) {
    //XXX: Investigate how to really do unicode with Postscript
	  // Just remove the extra byte per character and draw that instead
    buf = new char[aLength];

    for (i = 0; i < aLength; i++) {
      buf[i] = aString[ptr];
	  ptr+=2;
	  }
	xl_show(mPrintContext, buf, aLength, "");
	delete buf;
  }
  else{
    xl_show(mPrintContext, (char *)aString, aLength, "");
  }
}

nsRenderingContextPS :: nsRenderingContextPS()
{
  NS_INIT_REFCNT();
  
  mPrintContext = nsnull;     // local copy of printcontext, will be set on the init process
}

nsRenderingContextPS :: ~nsRenderingContextPS()
{


}

nsresult
nsRenderingContextPS :: QueryInterface(REFNSIID aIID, void** aInstancePtr)
{

  if (nsnull == aInstancePtr)
    return NS_ERROR_NULL_POINTER;

  if (aIID.Equals(kIRenderingContextIID))
  {
    nsIRenderingContext* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }

  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

  if (aIID.Equals(kISupportsIID))
  {
    nsIRenderingContext* tmp = this;
    nsISupports* tmp2 = tmp;
    *aInstancePtr = (void*) tmp2;
    NS_ADDREF_THIS();
    return NS_OK;
  }

  return NS_NOINTERFACE;
}

NS_IMPL_ADDREF(nsRenderingContextPS)
NS_IMPL_RELEASE(nsRenderingContextPS)

NS_IMETHODIMP
nsRenderingContextPS :: Init(nsIDeviceContext* aContext,nsIRenderingContext *aDelRenderingContext)
{

  mContext = aContext;
  mDelRenderingContext = aDelRenderingContext;
  mPrintContext = ((nsDeviceContextPS*)mContext)->GetPrintContext();
  NS_IF_ADDREF(mContext);
  NS_IF_ADDREF(mDelRenderingContext);
  return NS_OK;
}


NS_IMETHODIMP
nsRenderingContextPS :: SelectOffScreenDrawingSurface(nsDrawingSurface aSurface)
{
  return mDelRenderingContext->SelectOffScreenDrawingSurface(aSurface);
}

NS_IMETHODIMP
nsRenderingContextPS :: GetHints(PRUint32& aResult)
{

  return (mDelRenderingContext->GetHints(aResult));
}

NS_IMETHODIMP nsRenderingContextPS :: Reset()
{
  return (mDelRenderingContext->Reset());
}

NS_IMETHODIMP nsRenderingContextPS :: GetDeviceContext(nsIDeviceContext *&aContext)
{
  NS_IF_ADDREF(mContext);
  aContext = mContext;
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPS :: PushState(void)
{
  return (mDelRenderingContext->PushState());
}

NS_IMETHODIMP nsRenderingContextPS :: PopState(PRBool &aClipEmpty)
{
  return (mDelRenderingContext->PopState(aClipEmpty));
}

NS_IMETHODIMP nsRenderingContextPS :: IsVisibleRect(const nsRect& aRect, PRBool &aVisible)
{
  return (mDelRenderingContext->IsVisibleRect(aRect,aVisible));
}

NS_IMETHODIMP nsRenderingContextPS :: SetClipRect(const nsRect& aRect, nsClipCombine aCombine, PRBool &aClipEmpty)
{

  return (mDelRenderingContext->SetClipRect(aRect,aCombine,aClipEmpty));
}

NS_IMETHODIMP nsRenderingContextPS :: GetClipRect(nsRect &aRect, PRBool &aClipValid)
{
  return (mDelRenderingContext->GetClipRect(aRect,aClipValid));
}

NS_IMETHODIMP nsRenderingContextPS :: SetClipRegion(const nsIRegion& aRegion, nsClipCombine aCombine, PRBool &aClipEmpty)
{
  return (mDelRenderingContext->SetClipRegion(aRegion,aCombine,aClipEmpty));
}

NS_IMETHODIMP nsRenderingContextPS :: GetClipRegion(nsIRegion **aRegion)
{
  //XXX wow, needs to do something.
  return (mDelRenderingContext->GetClipRegion(aRegion));
}

NS_IMETHODIMP nsRenderingContextPS :: SetColor(nscolor aColor)
{
  return (mDelRenderingContext->SetColor(aColor));
}

NS_IMETHODIMP nsRenderingContextPS :: GetColor(nscolor &aColor) const
{
  
  return (mDelRenderingContext->GetColor(aColor));
}

NS_IMETHODIMP nsRenderingContextPS :: SetLineStyle(nsLineStyle aLineStyle)
{
  return (mDelRenderingContext->SetLineStyle(aLineStyle));
}

NS_IMETHODIMP nsRenderingContextPS :: GetLineStyle(nsLineStyle &aLineStyle)
{
  return (mDelRenderingContext->GetLineStyle(aLineStyle));
}

NS_IMETHODIMP nsRenderingContextPS :: SetFont(const nsFont& aFont)
{
  return (mDelRenderingContext->SetFont(aFont));
}

NS_IMETHODIMP nsRenderingContextPS :: SetFont(nsIFontMetrics *aFontMetrics)
{
  return (mDelRenderingContext->SetFont(aFontMetrics));
}

NS_IMETHODIMP nsRenderingContextPS :: GetFontMetrics(nsIFontMetrics *&aFontMetrics)
{
  return (mDelRenderingContext->GetFontMetrics(aFontMetrics));
}

// add the passed in translation to the current translation
NS_IMETHODIMP nsRenderingContextPS :: Translate(nscoord aX, nscoord aY)
{
  return (mDelRenderingContext->Translate(aX,aY));
}

// add the passed in scale to the current scale
NS_IMETHODIMP nsRenderingContextPS :: Scale(float aSx, float aSy)
{
  return (mDelRenderingContext->Scale(aSx,aSy));
}

NS_IMETHODIMP nsRenderingContextPS :: GetCurrentTransform(nsTransform2D *&aTransform)
{
  return (mDelRenderingContext->GetCurrentTransform(aTransform));
}

NS_IMETHODIMP nsRenderingContextPS :: CreateDrawingSurface(nsRect *aBounds, PRUint32 aSurfFlags, nsDrawingSurface &aSurface)
{
  return (mDelRenderingContext->CreateDrawingSurface(aBounds,aSurfFlags,aSurface));
}

NS_IMETHODIMP nsRenderingContextPS :: DestroyDrawingSurface(nsDrawingSurface aDS)
{
  return (mDelRenderingContext->DestroyDrawingSurface(aDS));
}

NS_IMETHODIMP nsRenderingContextPS :: DrawLine(nscoord aX0, nscoord aY0, nscoord aX1, nscoord aY1)
{
  //if (nsLineStyle_kNone == mCurrLineStyle)
    //return NS_OK;

	//mTMatrix->TransformCoord(&aX0,&aY0);
	//mTMatrix->TransformCoord(&aX1,&aY1);

  //SetupPen();

  // support dashed lines here
  //::MoveToEx(mDC, (int)(aX0), (int)(aY0), NULL);
  //::LineTo(mDC, (int)(aX1), (int)(aY1));

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPS :: DrawPolyline(const nsPoint aPoints[], PRInt32 aNumPoints)
{

  //if (nsLineStyle_kNone == mCurrLineStyle)
    //return NS_OK;

  // First transform nsPoint's into POINT's; perform coordinate space
  // transformation at the same time
  POINT pts[20];
  POINT* pp0 = pts;

  if (aNumPoints > 20)
    pp0 = new POINT[aNumPoints];

  POINT* pp = pp0;
  const nsPoint* np = &aPoints[0];

	for (PRInt32 i = 0; i < aNumPoints; i++, pp++, np++)
  {
		pp->x = np->x;
		pp->y = np->y;
		//mTMatrix->TransformCoord((int*)&pp->x,(int*)&pp->y);
	}

  // Draw the polyline
  //SetupPen();
  //::Polyline(mDC, pp0, int(aNumPoints));

  // Release temporary storage if necessary
  if (pp0 != pts)
    delete pp0;

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPS :: DrawRect(const nsRect& aRect)
{
RECT nr;
nsRect	tr;

	tr = aRect;
	//mTMatrix->TransformCoord(&tr.x,&tr.y,&tr.width,&tr.height);
	nr.left = tr.x;
	nr.top = tr.y;
	nr.right = tr.x+tr.width;
	nr.bottom = tr.y+tr.height;

  //::FrameRect(mDC, &nr, SetupSolidBrush());

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPS :: DrawRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
RECT nr;

	//mTMatrix->TransformCoord(&aX,&aY,&aWidth,&aHeight);
	nr.left = aX;
	nr.top = aY;
	nr.right = aX+aWidth;
	nr.bottom = aY+aHeight;

  //::FrameRect(mDC, &nr, SetupSolidBrush());

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPS :: FillRect(const nsRect& aRect)
{
  RECT nr;
	nsRect	tr;

	tr = aRect;
	//mTMatrix->TransformCoord(&tr.x,&tr.y,&tr.width,&tr.height);
	nr.left = tr.x;
	nr.top = tr.y;
	nr.right = tr.x+tr.width;
	nr.bottom = tr.y+tr.height;

	PostscriptFillRect(NS_PIXELS_TO_POINTS(tr.x), NS_PIXELS_TO_POINTS(tr.y), 
		 NS_PIXELS_TO_POINTS(tr.width), NS_PIXELS_TO_POINTS(tr.height));
	//XXX: Remove fillrect call bellow when working

  //::FillRect(mDC, &nr, SetupSolidBrush());

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPS :: FillRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
  RECT nr;
	nsRect	tr;

	//mTMatrix->TransformCoord(&aX,&aY,&aWidth,&aHeight);
	nr.left = aX;
	nr.top = aY;
	nr.right = aX+aWidth;
	nr.bottom = aY+aHeight;

  //::FillRect(mDC, &nr, SetupSolidBrush());

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPS :: DrawPolygon(const nsPoint aPoints[], PRInt32 aNumPoints)
{
  // First transform nsPoint's into POINT's; perform coordinate space
  // transformation at the same time
  POINT pts[20];
  POINT* pp0 = pts;

  if (aNumPoints > 20)
    pp0 = new POINT[aNumPoints];

  POINT* pp = pp0;
  const nsPoint* np = &aPoints[0];

	for (PRInt32 i = 0; i < aNumPoints; i++, pp++, np++){
		pp->x = np->x;
		pp->y = np->y;
		//mTMatrix->TransformCoord((int*)&pp->x,(int*)&pp->y);
	}

  // Outline the polygon - note we are implicitly ignoring the linestyle here
  LOGBRUSH lb;
  lb.lbStyle = BS_NULL;
  lb.lbColor = 0;
  lb.lbHatch = 0;
  //SetupSolidPen();
  //HBRUSH brush = ::CreateBrushIndirect(&lb);
  //HBRUSH oldBrush = (HBRUSH)::SelectObject(mDC, brush);
  //::Polygon(mDC, pp0, int(aNumPoints));
  //::SelectObject(mDC, oldBrush);
  //::DeleteObject(brush);

  // Release temporary storage if necessary
  if (pp0 != pts)
    delete pp0;

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPS :: FillPolygon(const nsPoint aPoints[], PRInt32 aNumPoints)
{
  // First transform nsPoint's into POINT's; perform coordinate space
  // transformation at the same time

  POINT pts[20];
  POINT* pp0 = pts;

  if (aNumPoints > 20)
    pp0 = new POINT[aNumPoints];

  POINT* pp = pp0;
  const nsPoint* np = &aPoints[0];

	for (PRInt32 i = 0; i < aNumPoints; i++, pp++, np++){
		pp->x = np->x;
		pp->y = np->y;
		//mTMatrix->TransformCoord((int*)&pp->x,(int*)&pp->y);
	}

  // Fill the polygon
  //SetupSolidBrush();

  //if (NULL == mNullPen)
    //mNullPen = ::CreatePen(PS_NULL, 0, 0);

  //HPEN oldPen = (HPEN)::SelectObject(mDC, mNullPen);
  //::Polygon(mDC, pp0, int(aNumPoints));
  //::SelectObject(mDC, oldPen);

  // Release temporary storage if necessary
  if (pp0 != pts)
    delete pp0;

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPS :: DrawEllipse(const nsRect& aRect)
{
  return DrawEllipse(aRect.x, aRect.y, aRect.width, aRect.height);
}

NS_IMETHODIMP nsRenderingContextPS :: DrawEllipse(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
  //if (nsLineStyle_kNone == mCurrLineStyle)
    //return NS_OK;

  //mTMatrix->TransformCoord(&aX, &aY, &aWidth, &aHeight);

  //SetupPen();

  //HBRUSH oldBrush = (HBRUSH)::SelectObject(mDC, ::GetStockObject(NULL_BRUSH));
  
  //::Ellipse(mDC, aX, aY, aX + aWidth, aY + aHeight);
  //::SelectObject(mDC, oldBrush);

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPS :: FillEllipse(const nsRect& aRect)
{
  return FillEllipse(aRect.x, aRect.y, aRect.width, aRect.height);
}

NS_IMETHODIMP nsRenderingContextPS :: FillEllipse(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
  //mTMatrix->TransformCoord(&aX, &aY, &aWidth, &aHeight);

  //SetupSolidPen();
  //SetupSolidBrush();
  
  //:Ellipse(mDC, aX, aY, aX + aWidth, aY + aHeight);

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPS :: DrawArc(const nsRect& aRect,
                                 float aStartAngle, float aEndAngle)
{
  return DrawArc(aRect.x,aRect.y,aRect.width,aRect.height,aStartAngle,aEndAngle);
}

NS_IMETHODIMP nsRenderingContextPS :: DrawArc(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight,
                                 float aStartAngle, float aEndAngle)
{
  //if (nsLineStyle_kNone == mCurrLineStyle)
    //return NS_OK;

  PRInt32 quad1, quad2, sx, sy, ex, ey, cx, cy;
  float   anglerad, distance;

  //mTMatrix->TransformCoord(&aX, &aY, &aWidth, &aHeight);

  //SetupPen();
  //SetupSolidBrush();

  // figure out the the coordinates of the arc from the angle
  distance = (float)sqrt((float)(aWidth * aWidth + aHeight * aHeight));
  cx = aX + aWidth / 2;
  cy = aY + aHeight / 2;

  anglerad = (float)(aStartAngle / (180.0 / 3.14159265358979323846));
  quad1 = (PRInt32)(aStartAngle / 90.0);
  sx = (PRInt32)(distance * cos(anglerad) + cx);
  sy = (PRInt32)(cy - distance * sin(anglerad));

  anglerad = (float)(aEndAngle / (180.0 / 3.14159265358979323846));
  quad2 = (PRInt32)(aEndAngle / 90.0);
  ex = (PRInt32)(distance * cos(anglerad) + cx);
  ey = (PRInt32)(cy - distance * sin(anglerad));

  // this just makes it consitent, on windows 95 arc will always draw CC, nt this sets direction
  //::SetArcDirection(mDC, AD_COUNTERCLOCKWISE);

  //::Arc(mDC, aX, aY, aX + aWidth, aY + aHeight, sx, sy, ex, ey); 

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPS :: FillArc(const nsRect& aRect,
                                 float aStartAngle, float aEndAngle)
{
  return FillArc(aRect.x, aRect.y, aRect.width, aRect.height, aStartAngle, aEndAngle);
}

NS_IMETHODIMP nsRenderingContextPS :: FillArc(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight,
                                 float aStartAngle, float aEndAngle)
{
  PRInt32 quad1, quad2, sx, sy, ex, ey, cx, cy;
  float   anglerad, distance;

  //mTMatrix->TransformCoord(&aX, &aY, &aWidth, &aHeight);

  //SetupSolidPen();
  //SetupSolidBrush();

  // figure out the the coordinates of the arc from the angle
  distance = (float)sqrt((float)(aWidth * aWidth + aHeight * aHeight));
  cx = aX + aWidth / 2;
  cy = aY + aHeight / 2;

  anglerad = (float)(aStartAngle / (180.0 / 3.14159265358979323846));
  quad1 = (PRInt32)(aStartAngle / 90.0);
  sx = (PRInt32)(distance * cos(anglerad) + cx);
  sy = (PRInt32)(cy - distance * sin(anglerad));

  anglerad = (float)(aEndAngle / (180.0 / 3.14159265358979323846));
  quad2 = (PRInt32)(aEndAngle / 90.0);
  ex = (PRInt32)(distance * cos(anglerad) + cx);
  ey = (PRInt32)(cy - distance * sin(anglerad));

  // this just makes it consistent, on windows 95 arc will always draw CC,
  // on NT this sets direction
  //::SetArcDirection(mDC, AD_COUNTERCLOCKWISE);

  //::Pie(mDC, aX, aY, aX + aWidth, aY + aHeight, sx, sy, ex, ey); 

  return NS_OK;
}


NS_IMETHODIMP nsRenderingContextPS :: GetWidth(char ch, nscoord& aWidth)
{

  return mDelRenderingContext->GetWidth(ch,aWidth);

}

NS_IMETHODIMP nsRenderingContextPS :: GetWidth(PRUnichar ch, nscoord &aWidth)
{
  return mDelRenderingContext->GetWidth(ch,aWidth);
}

NS_IMETHODIMP nsRenderingContextPS :: GetWidth(const char* aString, nscoord& aWidth)
{
  return mDelRenderingContext->GetWidth(aString,aWidth);
}

NS_IMETHODIMP nsRenderingContextPS :: GetWidth(const char* aString,PRUint32 aLength,nscoord& aWidth)
{
  return mDelRenderingContext->GetWidth(aString,aLength,aWidth);
}

NS_IMETHODIMP nsRenderingContextPS :: GetWidth(const nsString& aString, nscoord& aWidth)
{
  return mDelRenderingContext->GetWidth(aString,aWidth);
}

NS_IMETHODIMP nsRenderingContextPS :: GetWidth(const PRUnichar *aString,PRUint32 aLength,nscoord &aWidth)
{
  return mDelRenderingContext->GetWidth(aString,aLength,aWidth);
}

NS_IMETHODIMP nsRenderingContextPS :: DrawString(const char *aString, PRUint32 aLength,
                        nscoord aX, nscoord aY,
                        nscoord aWidth,
                        const nscoord* aSpacing)
{
PRInt32	      x = aX;
PRInt32       y = aY;
nsTransform2D *theMatrix;

  SetupFontAndColor();

  INT dxMem[500];
  INT* dx0;
  if (nsnull != aSpacing) {
    dx0 = dxMem;
    if (aLength > 500) {
      dx0 = new INT[aLength];
    }
    mDelRenderingContext->GetCurrentTransform(theMatrix);
    theMatrix->ScaleXCoords(aSpacing, aLength, dx0);
  }

	theMatrix->TransformCoord(&x, &y);
  //::ExtTextOut(mDC, x, y, 0, NULL, aString, aLength, aSpacing ? dx0 : NULL);
   //XXX: Remove ::ExtTextOut later
  PostscriptTextOut(aString, aLength, NS_PIXELS_TO_POINTS(x), NS_PIXELS_TO_POINTS(y), aLength, aSpacing ? dx0 : NULL, FALSE);

  if ((nsnull != aSpacing) && (dx0 != dxMem)) {
    delete [] dx0;
  }

#ifdef NOTNOW
  if (nsnull != mFontMetrics){
    nsFont *font;
    mFontMetrics->GetFont(font);
    PRUint8 decorations = font->decorations;

    if (decorations & NS_FONT_DECORATION_OVERLINE){
      nscoord offset;
      nscoord size;
      //mFontMetrics->GetUnderline(offset, size);
      FillRect(aX, aY, aWidth, size);
    }
  }


#endif

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPS :: DrawString(const PRUnichar *aString, PRUint32 aLength,
                                    nscoord aX, nscoord aY, nscoord aWidth,
                                    const nscoord* aSpacing)
{
nsTransform2D   *theMatrix;
PRInt32         x = aX;
PRInt32         y = aY;
nsIFontMetrics  *fMetrics;

  SetupFontAndColor();
  mDelRenderingContext->GetCurrentTransform(theMatrix);

  if (nsnull != aSpacing){
    // XXX Fix path to use a twips transform in the DC and use the
    // spacing values directly and let windows deal with the sub-pixel
    // positioning.

    // Slow, but accurate rendering
    const PRUnichar* end = aString + aLength;
    while (aString < end){
      x = aX;
      y = aY;
      theMatrix->TransformCoord(&x, &y);
	    PostscriptTextOut((const char *)aString, 1, NS_PIXELS_TO_POINTS(x), NS_PIXELS_TO_POINTS(y), aWidth, aSpacing, PR_TRUE);
      aX += *aSpacing++;
      aString++;
    }
  } else {
    theMatrix->TransformCoord(&x, &y);
	  PostscriptTextOut((const char *)aString, aLength, NS_PIXELS_TO_POINTS(x), NS_PIXELS_TO_POINTS(y), aWidth, aSpacing, PR_TRUE);
  }


  mDelRenderingContext->GetFontMetrics(fMetrics);

  if (nsnull != fMetrics){
    nsFont *font;
    fMetrics->GetFont(font);
    PRUint8 decorations = font->decorations;

    if (decorations & NS_FONT_DECORATION_OVERLINE){
      nscoord offset;
      nscoord size;
      fMetrics->GetUnderline(offset, size);
      FillRect(aX, aY, aWidth, size);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPS :: DrawString(const nsString& aString,
                                    nscoord aX, nscoord aY, nscoord aWidth,
                                    const nscoord* aSpacing)
{
  return DrawString(aString.GetUnicode(), aString.Length(), aX, aY, aWidth, aSpacing);
}

NS_IMETHODIMP nsRenderingContextPS :: DrawImage(nsIImage *aImage, nscoord aX, nscoord aY)
{
  //NS_PRECONDITION(PR_TRUE == mInitialized, "!initialized");

  nscoord width, height;

  //width = NSToCoordRound(mP2T * aImage->GetWidth());
  //height = NSToCoordRound(mP2T * aImage->GetHeight());

  return DrawImage(aImage, aX, aY, width, height);
}

NS_IMETHODIMP nsRenderingContextPS :: DrawImage(nsIImage *aImage, nscoord aX, nscoord aY,
                                        nscoord aWidth, nscoord aHeight) 
{
  nsRect  tr;

  tr.x = aX;
  tr.y = aY;
  tr.width = aWidth;
  tr.height = aHeight;

  return DrawImage(aImage, tr);
}

NS_IMETHODIMP nsRenderingContextPS :: DrawImage(nsIImage *aImage, const nsRect& aSRect, const nsRect& aDRect)
{
  nsRect	sr,dr;

	sr = aSRect;
	//mTMatrix->TransformCoord(&sr.x, &sr.y, &sr.width, &sr.height);

  dr = aDRect;
	//mTMatrix->TransformCoord(&dr.x, &dr.y, &dr.width, &dr.height);

  //return aImage->Draw(*this, mSurface, sr.x, sr.y, sr.width, sr.height, dr.x, dr.y, dr.width, dr.height);
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPS :: DrawImage(nsIImage *aImage, const nsRect& aRect)
{
  nsRect	tr;

	tr = aRect;
	//mTMatrix->TransformCoord(&tr.x, &tr.y, &tr.width, &tr.height);

  //return aImage->Draw(*this, mSurface, tr.x, tr.y, tr.width, tr.height);
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPS :: CopyOffScreenBits(nsDrawingSurface aSrcSurf,
                                                         PRInt32 aSrcX, PRInt32 aSrcY,
                                                         const nsRect &aDestBounds,
                                                         PRUint32 aCopyFlags)
{
#ifdef NOTNOW
  if ((nsnull != aSrcSurf) && (nsnull != mMainDC))
  {
    PRInt32 x = aSrcX;
    PRInt32 y = aSrcY;
    nsRect  drect = aDestBounds;
    HDC     destdc;

    if (nsnull != ((nsDrawingSurfacePS *)aSrcSurf)->mDC){
      if (aCopyFlags & NS_COPYBITS_TO_BACK_BUFFER){
        NS_ASSERTION(!(nsnull == mDC), "no back buffer");
        destdc = mDC;
      }
      else
        destdc = mMainDC;

      if (aCopyFlags & NS_COPYBITS_USE_SOURCE_CLIP_REGION)
      {
        HRGN  tregion = ::CreateRectRgn(0, 0, 0, 0);

        //if (::GetClipRgn(((nsDrawingSurfacePS *)aSrcSurf)->mDC, tregion) == 1)
          //::SelectClipRgn(destdc, tregion);

        //::DeleteObject(tregion);
      }

      // If there's a palette make sure it's selected.
      // XXX This doesn't seem like the best place to be doing this...

      nsPaletteInfo palInfo;
      HPALETTE      oldPalette;

      mContext->GetPaletteInfo(palInfo);

      if (palInfo.isPaletteDevice && palInfo.palette)
        oldPalette = ::SelectPalette(destdc, (HPALETTE)palInfo.palette, TRUE);

      if (aCopyFlags & NS_COPYBITS_XFORM_SOURCE_VALUES)
        mTMatrix->TransformCoord(&x, &y);

      if (aCopyFlags & NS_COPYBITS_XFORM_DEST_VALUES)
        mTMatrix->TransformCoord(&drect.x, &drect.y, &drect.width, &drect.height);

      ::BitBlt(destdc, drect.x, drect.y,
               drect.width, drect.height,
               ((nsDrawingSurfacePS *)aSrcSurf)->mDC,
               x, y, SRCCOPY);

	  //XXX: Remove BitBlt above when working.
	  PostscriptDrawBitmap(NS_PIXELS_TO_POINTS(drect.x), 
		                   NS_PIXELS_TO_POINTS(drect.y),
						   NS_PIXELS_TO_POINTS(drect.width),
						   NS_PIXELS_TO_POINTS(drect.height),
						   IL_Pixmap *aImage, IL_Pixmap *aMask);
	  

      //if (palInfo.isPaletteDevice && palInfo.palette)
        //::SelectPalette(destdc, oldPalette, TRUE);

    }
    else
      NS_ASSERTION(0, "attempt to blit with bad DCs");
  }
  else
    NS_ASSERTION(0, "attempt to blit with bad DCs");

#endif
  return NS_OK;
}


void nsRenderingContextPS :: SetupFontAndColor(void)
{
nscoord         fontHeight = 0;
nsFont          *font;
nsIFontMetrics  *fMetrics;

  mDelRenderingContext->GetFontMetrics(fMetrics);
  fMetrics->GetHeight(fontHeight);
  fMetrics->GetFont(font);

  PostscriptFont(fontHeight,font->style,font->variant,font->weight,font->decorations);
}

#ifdef NOTNOW
HPEN nsRenderingContextPS :: SetupSolidPen(void)
{
  return mCurrPen;
}

HPEN nsRenderingContextPS :: SetupDashedPen(void)
{
  return mCurrPen;
}

HPEN nsRenderingContextPS :: SetupDottedPen(void)
{
  return mCurrPen;
}

#endif


#ifdef DC
NS_IMETHODIMP
nsRenderingContextPS::GetColor(nsString& aColor)
{
  char cbuf[40];
  PR_snprintf(cbuf, sizeof(cbuf), "#%02x%02x%02x",
              NS_GET_R(mCurrentColor),
              NS_GET_G(mCurrentColor),
              NS_GET_B(mCurrentColor));
  aColor = cbuf;
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextPS::SetColor(const nsString& aColor)
{
  nscolor rgb;
  char cbuf[40];
  aColor.ToCString(cbuf, sizeof(cbuf));
  if (NS_ColorNameToRGB(cbuf, &rgb)) {
    SetColor(rgb);
  }
  else if (NS_HexToRGB(cbuf, &rgb)) {
    SetColor(rgb);
  }
  return NS_OK;
}
#endif