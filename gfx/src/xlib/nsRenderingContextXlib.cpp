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

#include "nsRenderingContextXlib.h"
#include "prmem.h"

static NS_DEFINE_IID(kIDOMRenderingContextIID, NS_IDOMRENDERINGCONTEXT_IID);
static NS_DEFINE_IID(kIRenderingContextIID, NS_IRENDERING_CONTEXT_IID);
static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);

nsRenderingContextXlib::nsRenderingContextXlib()
{
  printf("nsRenderingContextXlib::nsRenderingContextXlib()\n");
  NS_INIT_REFCNT();
  mRenderingSurface = nsnull;
  mTMatrix = nsnull;
  mFontMetrics = nsnull;
  mContext = nsnull;
  mScriptObject = nsnull;
}

nsRenderingContextXlib::~nsRenderingContextXlib()
{
  printf("nsRenderingContextXlib::~nsRenderingContextXlib()\n");
  NS_IF_RELEASE(mContext);
  NS_IF_RELEASE(mFontMetrics);
}

nsresult
nsRenderingContextXlib::QueryInterface(const nsIID&  aIID, void** aInstancePtr)
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
  if (aIID.Equals(kIScriptObjectOwnerIID))
  {
    nsIScriptObjectOwner* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIDOMRenderingContextIID))
  {
    nsIDOMRenderingContext* tmp = this;
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

NS_IMPL_ADDREF(nsRenderingContextXlib)
NS_IMPL_RELEASE(nsRenderingContextXlib)

NS_IMETHODIMP
nsRenderingContextXlib::Init(nsIDeviceContext* aContext, nsIWidget *aWindow)
{
  printf("nsRenderingContextXlib::Init()\n");
  mContext = aContext;
  NS_IF_ADDREF(mContext);

  mRenderingSurface = (nsDrawingSurfaceXlib *)new nsDrawingSurfaceXlib();

  return CommonInit();
}

NS_IMETHODIMP
nsRenderingContextXlib::Init(nsIDeviceContext* aContext, nsDrawingSurface aSurface)
{
  printf("nsRenderingContxtXbli::Init()\n");

  mContext = aContext;
  NS_IF_ADDREF(mContext);

  mRenderingSurface = (nsDrawingSurfaceXlib *)aSurface;

  if (nsnull != mRenderingSurface) {
    NS_ADDREF(mRenderingSurface);
  }

  return CommonInit();
}

nsresult nsRenderingContextXlib::CommonInit(void)
{
  // put common stuff in here.
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::Reset(void)
{
  printf("nsRenderingContext::Reset()\n");
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::GetDeviceContext(nsIDeviceContext *&aContext)
{
  printf("nsRenderingContext::GetDeviceContext()\n");
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::LockDrawingSurface(PRInt32 aX, PRInt32 aY,
                                           PRUint32 aWidth, PRUint32 aHeight,
                                           void **aBits,
                                           PRInt32 *aStride,
                                           PRInt32 *aWidthBytes,
                                           PRUint32 aFlags)
{
  printf("nsRenderingContextXlib::LockDrawingSurface()\n");
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::UnlockDrawingSurface(void)
{
  printf("nsRenderingContextXlib::UnlockDrawingSurface()\n");
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::SelectOffScreenDrawingSurface(nsDrawingSurface aSurface)
{
  printf("nsRenderingContextXlib::SelectOffScreenDrawingSurface()\n");
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::GetDrawingSurface(nsDrawingSurface *aSurface)
{
  printf("nsRenderingContextXlib::GetDrawingSurface()\n");
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::GetHints(PRUint32& aResult)
{
  printf("nsRenderingContextXlib::GetHints()\n");
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::PushState(void)
{
  printf("nsRenderingContextXlib::PushState()\n");
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::PopState(PRBool &aClipState)
{
  printf("nsRenderingContextXlib::PopState()\n");
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::IsVisibleRect(const nsRect& aRect, PRBool &aClipState)
{
  printf("nsRenderingContextXlib::IsVisibleRect()\n");
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::SetClipRect(const nsRect& aRect, nsClipCombine aCombine, PRBool &aCilpState)
{
  printf("nsRenderingContextXlib::SetClipRect()\n");
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::GetClipRect(nsRect &aRect, PRBool &aClipState)
{
  printf("nsRenderingContextXlib::GetClipRext()\n");
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::SetClipRegion(const nsIRegion& aRegion, nsClipCombine aCombine, PRBool &aClipState)
{
  printf("nsRenderingContextXlib::SetClipRegion()\n");
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::GetClipRegion(nsIRegion **aRegion)
{
  printf("nsRenderingContextXlib::GetClipRegion()\n");
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::SetLineStyle(nsLineStyle aLineStyle)
{
  printf("nsRenderingContextXlib::SetLineStyle()\n");
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::GetLineStyle(nsLineStyle &aLineStyle)
{
  printf("nsRenderingContextXlib::GetLineStyle()\n");
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::SetColor(nscolor aColor)
{
  printf("nsRenderingContextXlib::SetColor()\n");
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::SetColor(nsString const &aColor)
{
  printf("nsRenderingContextXlib::SetColor()\n");
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::GetColor(nscolor &aColor) const
{
  printf("nsRenderingContextXlib::GetColor()\n");
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::GetColor(nsString& aColor)
{
  printf("nsRenderingContextXlib::GetColor()\n");
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::SetFont(const nsFont& aFont)
{
  printf("nsRenderingContextXlib::SetFont()\n");
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::SetFont(nsIFontMetrics *aFontMetrics)
{
  printf("nsRenderingContextXlib::SetFont()\n");
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::GetFontMetrics(nsIFontMetrics *&aFontMetrics)
{
  printf("nsRenderingContextXlib::GetFontMetrics()\n");
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::Translate(nscoord aX, nscoord aY)
{
  printf("nsRenderingContextXlib::Translate()\n");
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::Scale(float aSx, float aSy)
{
  printf("nsRenderingContextXlib::Scale()\n");
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::GetCurrentTransform(nsTransform2D *&aTransform)
{
  printf("nsRenderingContextXlib::GetCurrentTransform()\n");
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::CreateDrawingSurface(nsRect *aBounds, PRUint32 aSurfFlags, nsDrawingSurface &aSurface)
{
  printf("nsRenderingContextXlib::CreateDrawingSurface()\n");
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::DestroyDrawingSurface(nsDrawingSurface aDS)
{
  printf("nsRenderingContextXlib::DestroyDrawingSurface()\n");
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::DrawLine(nscoord aX0, nscoord aY0, nscoord aX1, nscoord aY1)
{
  printf("nsRenderingContextXlib::DrawLine()\n");
  if (nsnull == mTMatrix || nsnull == mRenderingSurface)
    return NS_ERROR_FAILURE;

  mTMatrix->TransformCoord(&aX0,&aY0);
  mTMatrix->TransformCoord(&aX1,&aY1);

  ::XDrawLine(gDisplay, mRenderingSurface->GetDrawable(),
              mRenderingSurface->GetGC(), aX0, aY0, aX1, aY1);

  return NS_OK;
}


NS_IMETHODIMP
nsRenderingContextXlib::DrawLine2(PRInt32 aX0, PRInt32 aY0,
                                  PRInt32 aX1, PRInt32 aY1)
{
  printf("nsRenderingContextXlib::DrawLine2()\n");
  return DrawLine(aX0, aY0, aX1, aY1);
}


NS_IMETHODIMP
nsRenderingContextXlib::DrawPolyline(const nsPoint aPoints[], PRInt32 aNumPoints)
{
  printf("nsRenderingContextXlib::DrawPolyLine()\n");
  if (nsnull == mTMatrix || nsnull == mRenderingSurface) {
    return NS_ERROR_FAILURE;
  }
  PRInt32 i ;
  XPoint * xpoints;
  XPoint * thispoint;

  xpoints = (XPoint *) PR_Malloc(sizeof(XPoint) * aNumPoints);

  for (i = 0; i < aNumPoints; i++){
    thispoint = (xpoints+i);
    thispoint->x = aPoints[i].x;
    thispoint->y = aPoints[i].y;
    mTMatrix->TransformCoord((PRInt32*)&thispoint->x,(PRInt32*)&thispoint->y);
  }

  ::XDrawLines(gDisplay,
               mRenderingSurface->GetDrawable(),
               mRenderingSurface->GetGC(),
               xpoints, aNumPoints, CoordModeOrigin);

  PR_Free((void *)xpoints);

  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::DrawRect(const nsRect& aRect)
{
  printf("nsRenderingContextXlib::DrawRext()\n");
  return DrawRect(aRect.x, aRect.y, aRect.width, aRect.height);
}

NS_IMETHODIMP
nsRenderingContextXlib::DrawRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
  printf("nsRenderingContextXlib::DrawRect()\n");
  if (nsnull == mTMatrix || nsnull == mRenderingSurface) {
    return NS_ERROR_FAILURE;
  }

  nscoord x,y,w,h; 
  
  x = aX;
  y = aY; 
  w = aWidth;
  h = aHeight;
    
  mTMatrix->TransformCoord(&x,&y,&w,&h);
    
  ::XDrawRectangle(gDisplay,
                   mRenderingSurface->GetDrawable(),
                   mRenderingSurface->GetGC(),
                   x,y,w,h);

  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::FillRect(const nsRect& aRect)
{
  printf("nsRenderingContextXlib::FillRect()\n");
  return FillRect(aRect.x, aRect.y, aRect.width, aRect.height);
}

NS_IMETHODIMP
nsRenderingContextXlib::FillRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
  printf("nsRenderingContextXlib::FillRect()\n");
  if (nsnull == mTMatrix || nsnull == mRenderingSurface) {
    return NS_ERROR_FAILURE;
  }
  nscoord x,y,w,h;
  x = aX;
  y = aY;
  w = aWidth;
  h = aHeight;

  mTMatrix->TransformCoord(&x,&y,&w,&h);
  ::XFillRectangle(gDisplay,
                   mRenderingSurface->GetDrawable(),
                   mRenderingSurface->GetGC(),
                   x,y,w,h);
  
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::DrawPolygon(const nsPoint aPoints[], PRInt32 aNumPoints)
{
  printf("nsRenderingContextXlib::DrawPolygon()\n");
  if (nsnull == mTMatrix || nsnull == mRenderingSurface) {
    return NS_ERROR_FAILURE;
  }
  PRInt32 i ;
  XPoint * xpoints;
  XPoint * thispoint;
  
  xpoints = (XPoint *) PR_Malloc(sizeof(XPoint) * aNumPoints);
  
  for (i = 0; i < aNumPoints; i++){
    thispoint = (xpoints+i);
    thispoint->x = aPoints[i].x;
    thispoint->y = aPoints[i].y;
    mTMatrix->TransformCoord((PRInt32*)&thispoint->x,(PRInt32*)&thispoint->y);
  }
  
  ::XDrawLines(gDisplay,
               mRenderingSurface->GetDrawable(),
               mRenderingSurface->GetGC(),
               xpoints, aNumPoints, CoordModeOrigin);

  PR_Free((void *)xpoints);
  
  return NS_OK;    
}

NS_IMETHODIMP
nsRenderingContextXlib::FillPolygon(const nsPoint aPoints[], PRInt32 aNumPoints)
{
  printf("nsRenderingContextXlib::FillPolygon()\n");
  if (nsnull == mTMatrix || nsnull == mRenderingSurface) {
    return NS_ERROR_FAILURE;
  }
  PRInt32 i ;
  XPoint * xpoints;
  XPoint * thispoint;
  nscoord x,y;
   
  xpoints = (XPoint *) PR_Malloc(sizeof(XPoint) * aNumPoints);
  
  for (i = 0; i < aNumPoints; i++){
    thispoint = (xpoints+i);
    x = aPoints[i].x;
    y = aPoints[i].y;
    mTMatrix->TransformCoord(&x,&y);
    thispoint->x = x;
    thispoint->y = y;
  } 
    
  ::XFillPolygon(gDisplay,
                 mRenderingSurface->GetDrawable(),
                 mRenderingSurface->GetGC(),
                 xpoints, aNumPoints, Convex, CoordModeOrigin);
               
  PR_Free((void *)xpoints);

  return NS_OK; 
}

NS_IMETHODIMP
nsRenderingContextXlib::DrawEllipse(const nsRect& aRect)
{
  printf("nsRenderingContextXlib::DrawEllipse()\n");
  return DrawEllipse(aRect.x, aRect.y, aRect.width, aRect.height);
}

NS_IMETHODIMP
nsRenderingContextXlib::DrawEllipse(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
  printf("nsRenderingContextXlib::DrawEllipse()\n");
  if (nsnull == mTMatrix || nsnull == mRenderingSurface) {
    return NS_ERROR_FAILURE;
  }
  nscoord x,y,w,h;
   
  x = aX; 
  y = aY;
  w = aWidth; 
  h = aHeight;
    
  mTMatrix->TransformCoord(&x,&y,&w,&h);
    
  ::XDrawArc(gDisplay,
             mRenderingSurface->GetDrawable(),
             mRenderingSurface->GetGC(),
             x,y,w,h, 0, 360 * 64);
  
  return NS_OK;  
}

NS_IMETHODIMP
nsRenderingContextXlib::FillEllipse(const nsRect& aRect)
{
  printf("nsRenderingContextXlib::FillEllipse()\n");
  return FillEllipse(aRect.x, aRect.y, aRect.width, aRect.height);
}

NS_IMETHODIMP
nsRenderingContextXlib::FillEllipse(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
  printf("nsRenderingContextXlib::FillEllipse()\n");
  if (nsnull == mTMatrix || nsnull == mRenderingSurface) {
    return NS_ERROR_FAILURE;
  }
  nscoord x,y,w,h;
   
  x = aX; 
  y = aY;
  w = aWidth; 
  h = aHeight;
    
  mTMatrix->TransformCoord(&x,&y,&w,&h);
    
  ::XFillArc(gDisplay,
             mRenderingSurface->GetDrawable(),
             mRenderingSurface->GetGC(),
             x,y,w,h, 0, 360 * 64);
  
  return NS_OK;  
}

NS_IMETHODIMP
nsRenderingContextXlib::DrawArc(const nsRect& aRect,
                                float aStartAngle, float aEndAngle)
{
  printf("nsRenderingContextXlib::DrawArc()\n");
  return DrawArc(aRect.x,aRect.y,aRect.width,aRect.height,aStartAngle,aEndAngle); 
}

NS_IMETHODIMP
nsRenderingContextXlib::DrawArc(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight,
                                float aStartAngle, float aEndAngle)
{
  printf("nsRenderingContextXlib::DrawArc()\n");
  if (nsnull == mTMatrix || nsnull == mRenderingSurface) {
    return NS_ERROR_FAILURE;
  }
  nscoord x,y,w,h;
   
  x = aX; 
  y = aY;
  w = aWidth; 
  h = aHeight;
    
  mTMatrix->TransformCoord(&x,&y,&w,&h);
    
  ::XDrawArc(gDisplay,
             mRenderingSurface->GetDrawable(),
             mRenderingSurface->GetGC(),
             x,y,w,h, NSToIntRound(aStartAngle * 64.0f),
             NSToIntRound(aEndAngle * 64.0f));
  
  return NS_OK;  
}

NS_IMETHODIMP
nsRenderingContextXlib::FillArc(const nsRect& aRect,
                                float aStartAngle, float aEndAngle)
{
  printf("nsRenderingContextXlib::FillArc()\n");
  return FillArc(aRect.x, aRect.y, aRect.width, aRect.height, aStartAngle, aEndAngle);
}

NS_IMETHODIMP
nsRenderingContextXlib::FillArc(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight,
                                float aStartAngle, float aEndAngle)
{
  printf("nsRenderingContextXlib::FillArc()\n");
  if (nsnull == mTMatrix || nsnull == mRenderingSurface) {
    return NS_ERROR_FAILURE;
  }
  nscoord x,y,w,h;
   
  x = aX; 
  y = aY;
  w = aWidth; 
  h = aHeight;
    
  mTMatrix->TransformCoord(&x,&y,&w,&h);
    
  ::XFillArc(gDisplay,
             mRenderingSurface->GetDrawable(),
             mRenderingSurface->GetGC(),
             x,y,w,h, NSToIntRound(aStartAngle * 64.0f),
             NSToIntRound(aEndAngle * 64.0f));
  
  return NS_OK;  
}

NS_IMETHODIMP
nsRenderingContextXlib::GetWidth(char aC, nscoord& aWidth)
{
  printf("nsRenderingContextXlib::GetWidth()\n");
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::GetWidth(PRUnichar aC, nscoord& aWidth,
                                 PRInt32 *aFontID)
{
  printf("nsRenderingContextXlib::GetWidth()\n");
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::GetWidth(const nsString& aString, nscoord& aWidth,
                                 PRInt32 *aFontID)
{
  printf("nsRenderingContextXlib::GetWidth()\n");
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::GetWidth(const char* aString, nscoord& aWidth)
{
  printf("nsRenderingContextXlib::GetWidth()\n");
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::GetWidth(const char* aString, PRUint32 aLength, nscoord& aWidth)
{
  printf("nsRenderingContextXlib::GetWidth()\n");
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::GetWidth(const PRUnichar* aString, PRUint32 aLength,
                                 nscoord& aWidth, PRInt32 *aFontID)
{
  printf("nsRenderingContextXlib::GetWidth()\n");
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::DrawString(const char *aString, PRUint32 aLength,
                                   nscoord aX, nscoord aY,
                                   const nscoord* aSpacing)
{
  printf("nsRenderingContextXlib::DrawString()\n");
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::DrawString(const PRUnichar *aString, PRUint32 aLength,
                                   nscoord aX, nscoord aY,
                                   PRInt32 aFontID,
                                   const nscoord* aSpacing)
{
  printf("nsRenderingContextXlib::DrawString()\n");
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextXlib::DrawString(const nsString& aString, nscoord aX, nscoord aY,
                                                 PRInt32 aFontID,
                                                 const nscoord* aSpacing)
{
  printf("nsRenderingContextXlib::DrawString()\n");
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::DrawImage(nsIImage *aImage, nscoord aX, nscoord aY)
{
  printf("nsRenderingContextXlib::DrawImage()\n");
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::DrawImage(nsIImage *aImage, nscoord aX, nscoord aY,
                                  nscoord aWidth, nscoord aHeight)
{
  printf("nsRenderingContextXlib::DrawImage()\n");
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::DrawImage(nsIImage *aImage, const nsRect& aRect)
{
  printf("nsRenderingContextXlib::DrawImage()\n");
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::DrawImage(nsIImage *aImage, const nsRect& aSRect, const nsRect& aDRect)
{
  printf("nsRenderingContextXlib::DrawImage()\n");
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::CopyOffScreenBits(nsDrawingSurface aSrcSurf, PRInt32 aSrcX, PRInt32 aSrcY,
                                          const nsRect &aDestBounds, PRUint32 aCopyFlags)
{
  printf("nsRenderingContextXlib::CopyOffScreenBits()\n");
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::GetScriptObject(nsIScriptContext *aContext, void** aScriptObject)
{
  printf("nsRenderingContextXlib::GetScriptObject()\n");
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::SetScriptObject(void* aScriptObject)
{
  printf("nsRenderingContextXlib::SetScriptObject()\n");
  return NS_OK;
}

