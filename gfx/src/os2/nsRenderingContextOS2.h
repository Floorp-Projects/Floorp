/*
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is the Mozilla OS/2 libraries.
 *
 * The Initial Developer of the Original Code is John Fairhurst,
 * <john_fairhurst@iname.com>.  Portions created by John Fairhurst are
 * Copyright (C) 1999 John Fairhurst. All Rights Reserved.
 *
 * Contributor(s): 
 *
 */

#ifndef _nsRenderingContextOS2_h
#define _nsRenderingContextOS2_h

#include "nsIRenderingContext.h"
#include "nsTransform2D.h"
#include "nscoord.h"
#include "nscolor.h"
#include "nsCRT.h"

class nsIDeviceContext;
class nsIFontMetrics;
class nsIPaletteOS2;
class nsString;
class nsIWidget;
class nsPoint;
class nsFont;
class nsRect;

struct GraphicsState;

#include "nsDrawingSurfaceOS2.h"

class nsRenderingContextOS2 : public nsIRenderingContext
{
 public:
   nsRenderingContextOS2();
   virtual ~nsRenderingContextOS2();

   void *operator new( size_t sz) {
      void *rv = new char[ sz];
      nsCRT::zero( rv, sz);
      return rv;
   }

   NS_DECL_ISUPPORTS

   NS_IMETHOD Init( nsIDeviceContext* aContext, nsIWidget *aWindow);
   NS_IMETHOD Init( nsIDeviceContext* aContext, nsDrawingSurface aSurface);

   NS_IMETHOD Reset();

   NS_IMETHOD GetDeviceContext( nsIDeviceContext *&aDeviceContext);

   NS_IMETHOD LockDrawingSurface( PRInt32 aX, PRInt32 aY,
                                  PRUint32 aWidth, PRUint32 aHeight,
                                  void **aBits,
                                  PRInt32 *aStride, PRInt32 *aWidthBytes,
                                  PRUint32 aFlags);
   NS_IMETHOD UnlockDrawingSurface();

   NS_IMETHOD SelectOffScreenDrawingSurface( nsDrawingSurface aSurface);
   NS_IMETHOD GetDrawingSurface( nsDrawingSurface *aSurface);

   NS_IMETHOD PushState();
   NS_IMETHOD PopState( PRBool &aClipEmpty);

   NS_IMETHOD IsVisibleRect( const nsRect& aRect, PRBool &aIsVisible);

   NS_IMETHOD SetClipRect( const nsRect& aRect, nsClipCombine aCombine, PRBool &aClipEmpty);
   NS_IMETHOD GetClipRect( nsRect &aRect, PRBool &aHasLocalClip);
   NS_IMETHOD SetClipRegion( const nsIRegion& aRegion, nsClipCombine aCombine, PRBool &aClipEmpty);
   NS_IMETHOD GetClipRegion( nsIRegion **aRegion);

   NS_IMETHOD SetColor( nscolor aColor);
   NS_IMETHOD GetColor( nscolor &aColor) const;

   NS_IMETHOD SetFont( const nsFont& aFont);
   NS_IMETHOD SetFont( nsIFontMetrics *aFontMetrics);

   NS_IMETHOD GetFontMetrics( nsIFontMetrics*& aFontMetrics);

   NS_IMETHOD Translate( nscoord aX, nscoord aY);
   NS_IMETHOD Scale( float aSx, float aSy);
   NS_IMETHOD GetCurrentTransform( nsTransform2D *&aTransform);

   NS_IMETHOD SetLineStyle( nsLineStyle aLineStyle);
   NS_IMETHOD GetLineStyle( nsLineStyle &aLineStyle);

   NS_IMETHOD CreateDrawingSurface( nsRect *aBounds, PRUint32 aSurfFlags, nsDrawingSurface &aSurface);
   NS_IMETHOD DestroyDrawingSurface( nsDrawingSurface aDS);

   NS_IMETHOD DrawLine( nscoord aX0, nscoord aY0, nscoord aX1, nscoord aY1);
   NS_IMETHOD DrawPolyline( const nsPoint aPoints[], PRInt32 aNumPoints);

   NS_IMETHOD DrawRect( const nsRect& aRect);
   NS_IMETHOD DrawRect( nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight);
   NS_IMETHOD FillRect( const nsRect& aRect);
   NS_IMETHOD FillRect( nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight);

   NS_IMETHOD InvertRect(const nsRect& aRect);
   NS_IMETHOD InvertRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight);

   NS_IMETHOD DrawPolygon( const nsPoint aPoints[], PRInt32 aNumPoints);
   NS_IMETHOD FillPolygon( const nsPoint aPoints[], PRInt32 aNumPoints);

   NS_IMETHOD DrawEllipse( const nsRect& aRect);
   NS_IMETHOD DrawEllipse( nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight);
   NS_IMETHOD FillEllipse( const nsRect& aRect);
   NS_IMETHOD FillEllipse( nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight);

   NS_IMETHOD DrawArc( const nsRect& aRect,
                       float aStartAngle, float aEndAngle);
   NS_IMETHOD DrawArc( nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight,
                       float aStartAngle, float aEndAngle);
   NS_IMETHOD FillArc( const nsRect& aRect,
                       float aStartAngle, float aEndAngle);
   NS_IMETHOD FillArc( nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight,
                       float aStartAngle, float aEndAngle);
   // String widths
   NS_IMETHOD GetWidth( char aC, nscoord &aWidth);
   NS_IMETHOD GetWidth( PRUnichar aC, nscoord &aWidth,
                        PRInt32 *aFontID = nsnull);
   NS_IMETHOD GetWidth( const nsString &aString, nscoord &aWidth,
                        PRInt32 *aFontID = nsnull);
   NS_IMETHOD GetWidth( const char* aString, nscoord &aWidth);
   NS_IMETHOD GetWidth( const char* aString, PRUint32 aLength,
                        nscoord &aWidth);
   NS_IMETHOD GetWidth( const PRUnichar *aString, PRUint32 aLength,
                        nscoord &aWidth, PRInt32 *aFontID = nsnull);
 
   NS_IMETHOD DrawString( const char *aString, PRUint32 aLength,
                          nscoord aX, nscoord aY,
                          const nscoord* aSpacing = nsnull);
   NS_IMETHOD DrawString( const PRUnichar *aString, PRUint32 aLength,
                          nscoord aX, nscoord aY,
                          PRInt32 aFontID = -1,
                          const nscoord* aSpacing = nsnull);
   NS_IMETHOD DrawString( const nsString& aString,
                          nscoord aX, nscoord aY,
                          PRInt32 aFontID = -1,
                          const nscoord* aSpacing = nsnull);
 
   NS_IMETHOD DrawImage( nsIImage *aImage, nscoord aX, nscoord aY);
   NS_IMETHOD DrawImage( nsIImage *aImage, nscoord aX, nscoord aY,
                         nscoord aWidth, nscoord aHeight); 
   NS_IMETHOD DrawImage( nsIImage *aImage, const nsRect& aRect);
   NS_IMETHOD DrawImage( nsIImage *aImage, const nsRect& aSRect, const nsRect& aDRect);
 
   NS_IMETHOD CopyOffScreenBits( nsDrawingSurface aSrcSurf,
                                 PRInt32 aSrcX, PRInt32 aSrcY,
                                 const nsRect &aDestBounds,
                                 PRUint32 aCopyFlags);

   NS_IMETHOD GetHints( PRUint32 &aResult);

   // Convert XP-rects to OS/2 space.
   // World coordinates given & required, double inclusive rcl wanted.
   void NS2PM_ININ( const nsRect &in, RECTL &rcl);
   // World coordinates given & required, inclusive-exclusive rcl wanted.
   void NS2PM_INEX( const nsRect &in, RECTL &rcl);

   // Convert OS/2 rects to XP space.
   // World coords given & required, double-inclusive rcl wanted
   void PM2NS_ININ( const RECTL &in, nsRect &out);

   // Convert XP points to OS/2 space.
   void NS2PM( PPOINTL aPointl, ULONG cPointls);

 private:
   nsresult CommonInit();
   nsresult SetupPS( HPS oldPS, HPS newPS);
   void     GetTargetHeight( PRUint32 &ht);

   // Colour/font setting; call before drawing things.
   void SetupDrawingColor( BOOL bForce = FALSE);
   void SetupFontAndColor( BOOL bForce = FALSE);

   // Primitive draw-ers
   void PMDrawRect( nsRect &rect, BOOL fill);
   void PMDrawPoly( const nsPoint aPoints[], PRInt32 aNumPoints, PRBool bFilled);
   void PMDrawArc( nsRect &rect, PRBool bFilled, PRBool bFull, PRInt32 start=0, PRInt32 end=0);

 protected:
   nsIDeviceContext    *mContext;         // device context
   nsIPaletteOS2       *mPalette;         // palette from the dc
   nsDrawingSurfaceOS2 *mSurface;         // draw things here
   nsDrawingSurfaceOS2 *mFrontSurface;    // if offscreen, this is onscreen
   nscolor              mColor;           // current colour
   nsLineStyle          mLineStyle;       // current line style
   nsTransform2D        mTMatrix;         // current xform matrix
   float                mP2T;             // cache pix-2-app factor from DC
   GraphicsState       *mStateStack;      // stack of graphics states
   nsIFontMetrics      *mFontMetrics;     // current font
   nsIFontMetrics      *mCurrFontMetrics; // currently selected font
   nscolor              mCurrDrawingColor;// currently selected drawing color
   nscolor              mCurrTextColor;   // currently selected text color
   nsLineStyle          mCurrLineStyle;   // currently selected line style
};

inline void nsRenderingContextOS2::GetTargetHeight( PRUint32 &ht)
{
   PRUint32 on, dummy, off;
   mSurface->GetDimensions( &dummy, &on);
   if( mSurface != mFrontSurface)
   {
      mFrontSurface->GetDimensions( &dummy, &off);
      if( off < on) on = off;
   }
   ht = on;
}

#endif
