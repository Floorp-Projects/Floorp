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

static NS_DEFINE_IID(kIDOMRenderingContextIID, NS_IDOMRENDERINGCONTEXT_IID);
static NS_DEFINE_IID(kIRenderingContextIID, NS_IRENDERING_CONTEXT_IID);
static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);

nsRenderingContextXlib::nsRenderingContextXlib()
{
  NS_INIT_REFCNT();
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
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::Init(nsIDeviceContext* aContext, nsDrawingSurface aSurface)
{
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::Reset(void)
{
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::GetDeviceContext(nsIDeviceContext *&aContext)
{
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
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::UnlockDrawingSurface(void)
{
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::SelectOffScreenDrawingSurface(nsDrawingSurface aSurface)
{
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::GetDrawingSurface(nsDrawingSurface *aSurface)
{
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::GetHints(PRUint32& aResult)
{
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::PushState(void)
{
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::PopState(PRBool &aClipState)
{
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::IsVisibleRect(const nsRect& aRect, PRBool &aClipState)
{
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::SetClipRect(const nsRect& aRect, nsClipCombine aCombine, PRBool &aCilpState)
{
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::GetClipRect(nsRect &aRect, PRBool &aClipState)
{
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::SetClipRegion(const nsIRegion& aRegion, nsClipCombine aCombine, PRBool &aClipState)
{
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::GetClipRegion(nsIRegion **aRegion)
{
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::SetLineStyle(nsLineStyle aLineStyle)
{
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::GetLineStyle(nsLineStyle &aLineStyle)
{
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::SetColor(nscolor aColor)
{
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::SetColor(nsString const &aColor)
{
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::GetColor(nscolor &aColor) const
{
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::GetColor(nsString& aColor)
{
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::SetFont(const nsFont& aFont)
{
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::SetFont(nsIFontMetrics *aFontMetrics)
{
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::GetFontMetrics(nsIFontMetrics *&aFontMetrics)
{
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::Translate(nscoord aX, nscoord aY)
{
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::Scale(float aSx, float aSy)
{
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::GetCurrentTransform(nsTransform2D *&aTransform)
{
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::CreateDrawingSurface(nsRect *aBounds, PRUint32 aSurfFlags, nsDrawingSurface &aSurface)
{
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::DestroyDrawingSurface(nsDrawingSurface aDS)
{
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::DrawLine(nscoord aX0, nscoord aY0, nscoord aX1, nscoord aY1)
{
  return NS_OK;
}


NS_IMETHODIMP
nsRenderingContextXlib::DrawLine2(PRInt32 aX0, PRInt32 aY0,
                                  PRInt32 aX1, PRInt32 aY1)
{
  DrawLine(aX0, aY0, aX1, aY1);
  return NS_OK;
}


NS_IMETHODIMP
nsRenderingContextXlib::DrawPolyline(const nsPoint aPoints[], PRInt32 aNumPoints)
{
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::DrawRect(const nsRect& aRect)
{
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::DrawRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::FillRect(const nsRect& aRect)
{
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::FillRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::DrawPolygon(const nsPoint aPoints[], PRInt32 aNumPoints)
{
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::FillPolygon(const nsPoint aPoints[], PRInt32 aNumPoints)
{
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::DrawEllipse(const nsRect& aRect)
{
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::DrawEllipse(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::FillEllipse(const nsRect& aRect)
{
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::FillEllipse(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::DrawArc(const nsRect& aRect,
                                float aStartAngle, float aEndAngle)
{
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::DrawArc(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight,
                                float aStartAngle, float aEndAngle)
{
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::FillArc(const nsRect& aRect,
                                float aStartAngle, float aEndAngle)
{
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::FillArc(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight,
                                float aStartAngle, float aEndAngle)
{
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::GetWidth(char aC, nscoord& aWidth)
{
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::GetWidth(PRUnichar aC, nscoord& aWidth,
                                 PRInt32 *aFontID)
{
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::GetWidth(const nsString& aString, nscoord& aWidth,
                                 PRInt32 *aFontID)
{
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::GetWidth(const char* aString, nscoord& aWidth)
{
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::GetWidth(const char* aString, PRUint32 aLength, nscoord& aWidth)
{
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::GetWidth(const PRUnichar* aString, PRUint32 aLength,
                                 nscoord& aWidth, PRInt32 *aFontID)
{
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::DrawString(const char *aString, PRUint32 aLength,
                                   nscoord aX, nscoord aY,
                                   const nscoord* aSpacing)
{
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::DrawString(const PRUnichar *aString, PRUint32 aLength,
                                   nscoord aX, nscoord aY,
                                   PRInt32 aFontID,
                                   const nscoord* aSpacing)
{
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextXlib::DrawString(const nsString& aString, nscoord aX, nscoord aY,
                                                 PRInt32 aFontID,
                                                 const nscoord* aSpacing)
{
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::DrawImage(nsIImage *aImage, nscoord aX, nscoord aY)
{
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::DrawImage(nsIImage *aImage, nscoord aX, nscoord aY,
                                  nscoord aWidth, nscoord aHeight)
{
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::DrawImage(nsIImage *aImage, const nsRect& aRect)
{
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::DrawImage(nsIImage *aImage, const nsRect& aSRect, const nsRect& aDRect)
{
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::CopyOffScreenBits(nsDrawingSurface aSrcSurf, PRInt32 aSrcX, PRInt32 aSrcY,
                                          const nsRect &aDestBounds, PRUint32 aCopyFlags)
{
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::GetScriptObject(nsIScriptContext *aContext, void** aScriptObject)
{
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::SetScriptObject(void* aScriptObject)
{
  return NS_OK;
}

