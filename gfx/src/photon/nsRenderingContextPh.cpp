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

#include "nsRenderingContextPh.h"
#include "nsRegionPh.h"
#include <math.h>
#include "libimg.h"
#include "nsDeviceContextPh.h"
#include "nsIScriptGlobalObject.h"
#include "prprf.h"
#include "nsDrawingSurfacePh.h"
#include "nsGfxCIID.h"

//#define GFX_DEBUG

#ifdef GFX_DEBUG
  #define BREAK_TO_DEBUGGER           DebugBreak()
#else   
  #define BREAK_TO_DEBUGGER
#endif  

#ifdef GFX_DEBUG
  #define VERIFY(exp)                 ((exp) ? 0: (GetLastError(), BREAK_TO_DEBUGGER))
#else   // !_DEBUG
  #define VERIFY(exp)                 (exp)
#endif  // !_DEBUG

static NS_DEFINE_IID(kIDOMRenderingContextIID, NS_IDOMRENDERINGCONTEXT_IID);
static NS_DEFINE_IID(kIRenderingContextIID, NS_IRENDERING_CONTEXT_IID);
static NS_DEFINE_IID(kIRenderingContextPhIID, NS_IRENDERING_CONTEXT_PH_IID);
static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIDrawingSurfaceIID, NS_IDRAWING_SURFACE_IID);
static NS_DEFINE_IID(kDrawingSurfaceCID, NS_DRAWING_SURFACE_CID);

#define FLAG_CLIP_VALID       0x0001
#define FLAG_CLIP_CHANGED     0x0002
#define FLAG_LOCAL_CLIP_VALID 0x0004

#define FLAGS_ALL             (FLAG_CLIP_VALID | FLAG_CLIP_CHANGED | FLAG_LOCAL_CLIP_VALID)

// Macro for creating a palette relative color if you have a COLORREF instead
// of the reg, green, and blue values. The color is color-matches to the nearest
// in the current logical palette. This has no effect on a non-palette device
#define PALETTERGB_COLORREF(c)  (0x02000000 | (c))

/*
 * This is actually real fun.  Windows does not draw dotted lines with Pen's
 * directly (Go ahead, try it, you'll get dashes).
 *
 * the trick is to install a callback and actually put the pixels in
 * directly. This function will get called for each pixel in the line.
 *
 */


class GraphicsState
{
public:
  GraphicsState();
  GraphicsState(GraphicsState &aState);
  ~GraphicsState();

  GraphicsState   *mNext;
};

GraphicsState :: GraphicsState()
{
  mNext = nsnull;
}

GraphicsState :: GraphicsState(GraphicsState &aState)
{
  mNext = &aState;
}

GraphicsState :: ~GraphicsState()
{
}

#define NOT_SETUP 0x33

nsRenderingContextPh :: nsRenderingContextPh()
{
  NS_INIT_REFCNT();
}

nsRenderingContextPh :: ~nsRenderingContextPh()
{
  NS_IF_RELEASE(mContext);
  NS_IF_RELEASE(mFontMetrics);
}

nsresult
nsRenderingContextPh :: QueryInterface(REFNSIID aIID, void** aInstancePtr)
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

  if (aIID.Equals(kIRenderingContextPhIID))
  {
    nsIRenderingContextPh* tmp = this;
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

NS_IMPL_ADDREF(nsRenderingContextPh)
NS_IMPL_RELEASE(nsRenderingContextPh)

NS_IMETHODIMP
nsRenderingContextPh :: Init(nsIDeviceContext* aContext,
                              nsIWidget *aWindow)
{
  NS_PRECONDITION(PR_FALSE == mInitialized, "double init");

  mContext = aContext;
  NS_IF_ADDREF(mContext);

  mSurface = (nsDrawingSurfacePh *)new nsDrawingSurfacePh();

  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextPh :: Init(nsIDeviceContext* aContext,
                              nsDrawingSurface aSurface)
{
  NS_PRECONDITION(PR_FALSE == mInitialized, "double init");

  mContext = aContext;
  NS_IF_ADDREF(mContext);

  mSurface = (nsDrawingSurfacePh *)aSurface;

  return NS_OK;
}


NS_IMETHODIMP nsRenderingContextPh :: LockDrawingSurface(PRInt32 aX, PRInt32 aY,
                                                          PRUint32 aWidth, PRUint32 aHeight,
                                                          void **aBits, PRInt32 *aStride,
                                                          PRInt32 *aWidthBytes, PRUint32 aFlags)
{
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPh :: UnlockDrawingSurface(void)
{
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextPh :: SelectOffScreenDrawingSurface(nsDrawingSurface aSurface)
{
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextPh :: GetDrawingSurface(nsDrawingSurface *aSurface)
{
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextPh :: GetHints(PRUint32& aResult)
{
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPh :: Reset()
{
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPh :: GetDeviceContext(nsIDeviceContext *&aContext)
{
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPh :: PushState(void)
{
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPh :: PopState(PRBool &aClipEmpty)
{
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPh :: IsVisibleRect(const nsRect& aRect, PRBool &aVisible)
{
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPh :: SetClipRect(const nsRect& aRect, nsClipCombine aCombine, PRBool &aClipEmpty)
{
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPh :: GetClipRect(nsRect &aRect, PRBool &aClipValid)
{
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPh :: SetClipRegion(const nsIRegion& aRegion, nsClipCombine aCombine, PRBool &aClipEmpty)
{
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPh :: GetClipRegion(nsIRegion **aRegion)
{

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPh :: SetColor(nscolor aColor)
{
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPh :: GetColor(nscolor &aColor) const
{
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPh :: SetLineStyle(nsLineStyle aLineStyle)
{
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPh :: GetLineStyle(nsLineStyle &aLineStyle)
{
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPh :: SetFont(const nsFont& aFont)
{
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPh :: SetFont(nsIFontMetrics *aFontMetrics)
{
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPh :: GetFontMetrics(nsIFontMetrics *&aFontMetrics)
{
  return NS_OK;
}

// add the passed in translation to the current translation
NS_IMETHODIMP nsRenderingContextPh :: Translate(nscoord aX, nscoord aY)
{
  return NS_OK;
}

// add the passed in scale to the current scale
NS_IMETHODIMP nsRenderingContextPh :: Scale(float aSx, float aSy)
{
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPh :: GetCurrentTransform(nsTransform2D *&aTransform)
{
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPh :: CreateDrawingSurface(nsRect *aBounds, PRUint32 aSurfFlags, nsDrawingSurface &aSurface)
{
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPh :: DestroyDrawingSurface(nsDrawingSurface aDS)
{
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPh :: DrawLine(nscoord aX0, nscoord aY0, nscoord aX1, nscoord aY1)
{
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPh :: DrawPolyline(const nsPoint aPoints[], PRInt32 aNumPoints)
{
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPh :: DrawRect(const nsRect& aRect)
{
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPh :: DrawRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPh :: FillRect(const nsRect& aRect)
{
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPh :: FillRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPh :: DrawPolygon(const nsPoint aPoints[], PRInt32 aNumPoints)
{
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPh :: FillPolygon(const nsPoint aPoints[], PRInt32 aNumPoints)
{
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPh :: DrawEllipse(const nsRect& aRect)
{
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPh :: DrawEllipse(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPh :: FillEllipse(const nsRect& aRect)
{
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPh :: FillEllipse(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPh :: DrawArc(const nsRect& aRect,
                                 float aStartAngle, float aEndAngle)
{
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPh :: DrawArc(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight,
                                 float aStartAngle, float aEndAngle)
{
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPh :: FillArc(const nsRect& aRect,
                                 float aStartAngle, float aEndAngle)
{
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPh :: FillArc(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight,
                                 float aStartAngle, float aEndAngle)
{
  return NS_OK;
}


NS_IMETHODIMP nsRenderingContextPh :: GetWidth(char ch, nscoord& aWidth)
{
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPh :: GetWidth(PRUnichar ch, nscoord &aWidth, PRInt32 *aFontID)
{
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPh :: GetWidth(const char* aString, nscoord& aWidth)
{
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPh :: GetWidth(const char* aString,
                                                PRUint32 aLength,
                                                nscoord& aWidth)
{
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPh :: GetWidth(const nsString& aString, nscoord& aWidth, PRInt32 *aFontID)
{
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPh :: GetWidth(const PRUnichar *aString,
                                                PRUint32 aLength,
                                                nscoord &aWidth,
                                                PRInt32 *aFontID)
{
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPh :: DrawString(const char *aString, PRUint32 aLength,
                                                  nscoord aX, nscoord aY,
                                                  const nscoord* aSpacing)
{
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPh :: DrawString(const PRUnichar *aString, PRUint32 aLength,
                                                  nscoord aX, nscoord aY,
                                                  PRInt32 aFontID,
                                                  const nscoord* aSpacing)
{
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPh :: DrawString(const nsString& aString,
                                                  nscoord aX, nscoord aY,
                                                  PRInt32 aFontID,
                                                  const nscoord* aSpacing)
{
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPh :: DrawImage(nsIImage *aImage, nscoord aX, nscoord aY)
{
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPh :: DrawImage(nsIImage *aImage, nscoord aX, nscoord aY,
                                        nscoord aWidth, nscoord aHeight) 
{
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPh :: DrawImage(nsIImage *aImage, const nsRect& aSRect, const nsRect& aDRect)
{
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPh :: DrawImage(nsIImage *aImage, const nsRect& aRect)
{
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPh :: CopyOffScreenBits(nsDrawingSurface aSrcSurf,
                                                         PRInt32 aSrcX, PRInt32 aSrcY,
                                                         const nsRect &aDestBounds,
                                                         PRUint32 aCopyFlags)
{
  return NS_OK;
}


void nsRenderingContextPh :: PushClipState(void)
{
}

NS_IMETHODIMP nsRenderingContextPh::GetScriptObject(nsIScriptContext* aContext,
                                       void** aScriptObject)
{
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPh::SetScriptObject(void* aScriptObject)
{
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPh::GetColor(nsString& aColor)
{
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPh::SetColor(const nsString& aColor)
{
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPh::DrawLine2(PRInt32 aX0, PRInt32 aY0,
                                 PRInt32 aX1, PRInt32 aY1)
{
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPh :: CreateDrawingSurface(PhGC_t &aGC, nsDrawingSurface &aSurface)
{
  return NS_OK;
}

