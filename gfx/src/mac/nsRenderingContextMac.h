/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsRenderingContextMac_h___
#define nsRenderingContextMac_h___

#include <QDOffscreen.h>
#include <UnicodeConverter.h>

#include "nsRenderingContextImpl.h"
#include "nsDrawingSurfaceMac.h"
#include "nsUnicodeRenderingToolkit.h"

#include "nsVoidArray.h"

class nsIFontMetrics;
class nsIDeviceContext;
class nsIRegion;
class nsFont;
class nsTransform2D;

class nsGraphicState;
class nsUnicodeFallbackCache;

//------------------------------------------------------------------------


class nsRenderingContextMac : public nsRenderingContextImpl
{
public:
  nsRenderingContextMac();
  ~nsRenderingContextMac();

  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  NS_DECL_ISUPPORTS

  NS_IMETHOD Init(nsIDeviceContext* aContext, nsIWidget *aWindow);
  NS_IMETHOD Init(nsIDeviceContext* aContext, nsIDrawingSurface* aSurface);

  NS_IMETHOD Reset(void);
  NS_IMETHOD GetDeviceContext(nsIDeviceContext *&aContext);
  NS_IMETHOD LockDrawingSurface(PRInt32 aX, PRInt32 aY, PRUint32 aWidth, PRUint32 aHeight,
                                void **aBits, PRInt32 *aStride, PRInt32 *aWidthBytes,
                                PRUint32 aFlags);
  NS_IMETHOD UnlockDrawingSurface(void);
  NS_IMETHOD SelectOffScreenDrawingSurface(nsIDrawingSurface* aSurface);
  NS_IMETHOD GetDrawingSurface(nsIDrawingSurface* *aSurface);
  NS_IMETHOD GetHints(PRUint32& aResult);
  NS_IMETHOD PushState(void);
  NS_IMETHOD PopState(void);
  NS_IMETHOD IsVisibleRect(const nsRect& aRect, PRBool &aVisible);
  NS_IMETHOD SetClipRect(const nsRect& aRect, nsClipCombine aCombine);
  NS_IMETHOD GetClipRect(nsRect &aRect, PRBool &aClipValid);
  NS_IMETHOD SetClipRegion(const nsIRegion& aRegion, nsClipCombine aCombine);
  NS_IMETHOD CopyClipRegion(nsIRegion &aRegion);
  NS_IMETHOD GetClipRegion(nsIRegion **aRegion);
  NS_IMETHOD SetLineStyle(nsLineStyle aLineStyle);
  NS_IMETHOD GetLineStyle(nsLineStyle &aLineStyle);
  NS_IMETHOD SetColor(nscolor aColor);
  NS_IMETHOD GetColor(nscolor &aColor) const;
  NS_IMETHOD SetFont(const nsFont& aFont, nsIAtom* aLangGroup);
  NS_IMETHOD SetFont(nsIFontMetrics *aFontMetrics);
  NS_IMETHOD GetFontMetrics(nsIFontMetrics *&aFontMetrics);
  NS_IMETHOD Translate(nscoord aX, nscoord aY);
  NS_IMETHOD Scale(float aSx, float aSy);
  NS_IMETHOD GetCurrentTransform(nsTransform2D *&aTransform);
  NS_IMETHOD CreateDrawingSurface(const nsRect& aBounds, PRUint32 aSurfFlags, nsIDrawingSurface* &aSurface);
  NS_IMETHOD DestroyDrawingSurface(nsIDrawingSurface* aDS);
  NS_IMETHOD DrawLine(nscoord aX0, nscoord aY0, nscoord aX1, nscoord aY1);
  NS_IMETHOD DrawPolyline(const nsPoint aPoints[], PRInt32 aNumPoints);
  NS_IMETHOD DrawRect(const nsRect& aRect);
  NS_IMETHOD DrawRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight);
  NS_IMETHOD FillRect(const nsRect& aRect);
  NS_IMETHOD FillRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight);
  NS_IMETHOD InvertRect(const nsRect& aRect);
  NS_IMETHOD InvertRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight);
  NS_IMETHOD FlushRect(const nsRect& aRect);
  NS_IMETHOD FlushRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight);
  NS_IMETHOD DrawPolygon(const nsPoint aPoints[], PRInt32 aNumPoints);
  NS_IMETHOD FillPolygon(const nsPoint aPoints[], PRInt32 aNumPoints);
  NS_IMETHOD DrawEllipse(const nsRect& aRect);
  NS_IMETHOD DrawEllipse(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight);
  NS_IMETHOD FillEllipse(const nsRect& aRect);
  NS_IMETHOD FillEllipse(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight);
  NS_IMETHOD DrawArc(const nsRect& aRect,float aStartAngle, float aEndAngle);
  NS_IMETHOD DrawArc(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight,float aStartAngle, float aEndAngle);
  NS_IMETHOD FillArc(const nsRect& aRect,float aStartAngle, float aEndAngle);
  NS_IMETHOD FillArc(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight,float aStartAngle, float aEndAngle);
  NS_IMETHOD GetWidth(char aC, nscoord &aWidth);
  NS_IMETHOD GetWidth(PRUnichar aC, nscoord &aWidth,
                      PRInt32 *aFontID);
  NS_IMETHOD GetWidth(const nsString& aString, nscoord &aWidth,
                      PRInt32 *aFontID);
  NS_IMETHOD GetWidth(const char *aString, nscoord &aWidth);
  NS_IMETHOD GetWidth(const char* aString, PRUint32 aLength, nscoord& aWidth);
  NS_IMETHOD GetWidth(const PRUnichar *aString, PRUint32 aLength, nscoord &aWidth,
                      PRInt32 *aFontID);
  NS_IMETHOD DrawString(const char *aString, PRUint32 aLength,nscoord aX, nscoord aY,const nscoord* aSpacing);
  NS_IMETHOD DrawString(const PRUnichar *aString, PRUint32 aLength, nscoord aX, nscoord aY,
                        PRInt32 aFontID,
                        const nscoord* aSpacing);
  NS_IMETHOD DrawString(const nsString& aString, nscoord aX, nscoord aY,
                        PRInt32 aFontID,
                        const nscoord* aSpacing);

  NS_IMETHOD GetTextDimensions(const char* aString, PRUint32 aLength,
                               nsTextDimensions& aDimensions);
  NS_IMETHOD GetTextDimensions(const PRUnichar *aString, PRUint32 aLength,
                               nsTextDimensions& aDimensions, PRInt32 *aFontID);

  NS_IMETHOD CopyOffScreenBits(nsIDrawingSurface* aSrcSurf, PRInt32 aSrcX, PRInt32 aSrcY,
                               const nsRect &aDestBounds, PRUint32 aCopyFlags);
  NS_IMETHOD RetrieveCurrentNativeGraphicData(PRUint32 * ngd);

  // nsRenderingContextImpl overrides
  NS_IMETHOD ReleaseBackbuffer(void);
  NS_IMETHOD UseBackbuffer(PRBool* aUseBackbuffer);


#ifdef MOZ_MATHML
  /**
   * Returns metrics (in app units) of an 8-bit character string
   */
  NS_IMETHOD GetBoundingMetrics(const char*        aString,
                                PRUint32           aLength,
                                nsBoundingMetrics& aBoundingMetrics);
  
  /**
   * Returns metrics (in app units) of a Unicode character string
   */
  NS_IMETHOD GetBoundingMetrics(const PRUnichar*   aString,
                                PRUint32           aLength,
                                nsBoundingMetrics& aBoundingMetrics,
                                PRInt32*           aFontID);
#endif /* MOZ_MATHML */
  /**
   * Let the device context know whether we want text reordered with
   * right-to-left base direction
   */
  NS_IMETHOD SetRightToLeftText(PRBool aIsRTL);
  //locals
  nsresult   SetPortTextState();
  nsresult   Init(nsIDeviceContext* aContext, CGrafPtr aPort);

  // for determining if we're running on at least Mac OS X 10.2
  static PRBool OnJaguar();

protected:
  enum GraphicStateChanges {
		kFontChanged	= (1 << 0),
		kColorChanged	= (1 << 1),
		kClippingChanged = (1 << 2),
		
		kEverythingChanged = 0xFFFFFFFF
	};

	void 			SelectDrawingSurface(nsDrawingSurfaceMac* aSurface, PRUint32 aChanges = kEverythingChanged);
  void      SetupPortState();

protected:
    float                   mP2T;               // Pixel to Twip conversion factor
    nsIDeviceContext *      mContext;

    nsDrawingSurfaceMac*    mFrontSurface;
    nsDrawingSurfaceMac*    mCurrentSurface;    // pointer to the current surface

    CGrafPtr                mPort;              // current grafPort - shortcut for mCurrentSurface->GetPort()
    nsGraphicState *        mGS;                // current graphic state - shortcut for mCurrentSurface->GetGS()
    nsUnicodeRenderingToolkit mUnicodeRenderingToolkit;
    nsAutoVoidArray         mGSStack;           // GraphicStates stack, used for PushState/PopState
    PRUint32                mChanges;           // bit mask of attributes that have changed since last Push().
    PRBool                  mRightToLeftText;
};

#endif /* nsRenderingContextMac_h___ */
