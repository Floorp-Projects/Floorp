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

#ifndef nsIRenderingContext_h___
#define nsIRenderingContext_h___

#include "nscore.h"
#include "nsISupports.h"
#include "nsColor.h"
#include "nsCoord.h"

class nsIWidget;
class nsIFontMetrics;
class nsIImage;
class nsTransform2D;
class nsString;
class nsIFontCache;
class nsIDeviceContext;

struct nsFont;
struct nsPoint;
struct nsRect;

// IID for the nsIRenderingContext interface
#define NS_IRENDERING_CONTEXT_IID    \
{ 0x7fd8c0f0, 0xa265, 0x11d1, \
{ 0xa8, 0x24, 0x00, 0x40, 0x95, 0x9a, 0x28, 0xc9 } }

//----------------------------------------------------------------------

//a cross platform way of specifying a navite rendering context
typedef void * nsDrawingSurface;

// RenderingContext interface
class nsIRenderingContext : public nsISupports
{
public:
  //TBD: bind/unbind, transformation of scalars (hacky), 
  //potential drawmode for selection, polygons. MMP

  virtual nsresult Init(nsIDeviceContext* aContext,nsIWidget *aWidget) = 0;
  virtual nsresult Init(nsIDeviceContext* aContext,nsDrawingSurface aSurface) = 0;

  virtual void Reset() = 0;
  virtual nsresult SelectOffScreenDrawingSurface(nsDrawingSurface aSurface) = 0;

  virtual void PushState() = 0;
  virtual void PopState() = 0;

  virtual PRBool IsVisibleRect(const nsRect& aRect) = 0;

  //set aIntersect to true to cause the new cliprect to be intersected
  //with any other cliprects already in the renderingcontext.
  virtual void SetClipRect(const nsRect& aRect, PRBool aIntersect) = 0;
  virtual const nsRect& GetClipRect() = 0;

  virtual void SetColor(nscolor aColor) = 0;
  virtual nscolor GetColor() const = 0;

  virtual void SetFont(const nsFont& aFont) = 0;
  virtual const nsFont& GetFont() = 0;

  virtual nsIFontMetrics* GetFontMetrics() = 0;

  virtual void Translate(nscoord aX, nscoord aY) = 0;
  virtual void Scale(float aSx, float aSy) = 0;
  virtual nsTransform2D * GetCurrentTransform() = 0;

  //create and destroy offscreen renderingsurface compatible with this RC
  virtual nsDrawingSurface CreateDrawingSurface(nsRect &aBounds) = 0;
  virtual void DestroyDrawingSurface(nsDrawingSurface aDS) = 0;

  virtual nsDrawingSurface CreateOptimizeSurface() = 0;

  virtual nsDrawingSurface getDrawingSurface() = 0;

  virtual void DrawLine(nscoord aX0, nscoord aY0, nscoord aX1, nscoord aY1) = 0;

  virtual void DrawRect(const nsRect& aRect) = 0;
  virtual void DrawRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight) = 0;
  virtual void FillRect(const nsRect& aRect) = 0;
  virtual void FillRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight) = 0;

  virtual void DrawPolygon(nsPoint aPoints[], PRInt32 aNumPoints) = 0;
  virtual void FillPolygon(nsPoint aPoints[], PRInt32 aNumPoints) = 0;

  virtual void DrawEllipse(const nsRect& aRect) = 0;
  virtual void DrawEllipse(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight) = 0;
  virtual void FillEllipse(const nsRect& aRect) = 0;
  virtual void FillEllipse(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight) = 0;

  virtual void DrawArc(const nsRect& aRect,
                       float aStartAngle, float aEndAngle) = 0;
  virtual void DrawArc(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight,
                       float aStartAngle, float aEndAngle) = 0;
  virtual void FillArc(const nsRect& aRect,
                       float aStartAngle, float aEndAngle) = 0;
  virtual void FillArc(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight,
                       float aStartAngle, float aEndAngle) = 0;

  virtual void DrawString(const char *aString, PRUint32 aLength,
                          nscoord aX, nscoord aY,
                          nscoord aWidth) = 0;
  virtual void DrawString(const PRUnichar *aString, PRUint32 aLength,
                          nscoord aX, nscoord aY,
                          nscoord aWidth) = 0;
  virtual void DrawString(const nsString& aString, nscoord aX, nscoord aY,
                          nscoord aWidth) = 0;

  virtual void DrawImage(nsIImage *aImage, nscoord aX, nscoord aY) = 0;
  virtual void DrawImage(nsIImage *aImage, nscoord aX, nscoord aY,
                         nscoord aWidth, nscoord aHeight) = 0; 
  virtual void DrawImage(nsIImage *aImage, const nsRect& aRect) = 0;
  virtual void DrawImage(nsIImage *aImage, const nsRect& aSRect, const nsRect& aDRect)=0;

  virtual nsresult CopyOffScreenBits(nsRect &aBounds) = 0;
};

//modifiers for text rendering

#define NS_DRAWSTRING_NORMAL            0x0
#define NS_DRAWSTRING_UNDERLINE         0x1
#define NS_DRAWSTRING_OVERLINE          0x2
#define NS_DRAWSTRING_LINE_THROUGH      0x4

#endif /* nsIRenderingContext_h___ */
