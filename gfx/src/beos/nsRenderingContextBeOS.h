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
 *   Daniel Switkin, Mathias Agopian, Sergei Dolgov
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

#ifndef nsRenderingContextBeOS_h___
#define nsRenderingContextBeOS_h___

#include "nsRenderingContextImpl.h"
#include "nsUnitConversion.h"
#include "nsFont.h"
#include "nsIFontMetrics.h"
#include "nsPoint.h"
#include "nsString.h"
#include "nsCRT.h"
#include "nsTransform2D.h"
#include "nsIWidget.h"
#include "nsRect.h"
#include "nsIDeviceContext.h"
#include "nsVoidArray.h"
#include "nsGfxCIID.h"
#include "nsDrawingSurfaceBeOS.h"
#include "nsRegionBeOS.h"

class nsRenderingContextBeOS : public nsRenderingContextImpl
{
public:
	nsRenderingContextBeOS();
	virtual ~nsRenderingContextBeOS();
	
	NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW	
	NS_DECL_ISUPPORTS
	
	NS_IMETHOD Init(nsIDeviceContext *aContext, nsIWidget *aWindow);
	NS_IMETHOD Init(nsIDeviceContext *aContext, nsDrawingSurface aSurface);
	
	NS_IMETHOD Reset();
	NS_IMETHOD GetDeviceContext(nsIDeviceContext *&aContext);
	
	NS_IMETHOD LockDrawingSurface(PRInt32 aX, PRInt32 aY, PRUint32 aWidth, PRUint32 aHeight,
		void **aBits, PRInt32 *aStride, PRInt32 *aWidthBytes, PRUint32 aFlags);
	NS_IMETHOD UnlockDrawingSurface();
	
	NS_IMETHOD SelectOffScreenDrawingSurface(nsDrawingSurface aSurface);
	NS_IMETHOD GetDrawingSurface(nsDrawingSurface *aSurface);
	NS_IMETHOD GetHints(PRUint32 &aResult);
	
	NS_IMETHOD PushState();
	NS_IMETHOD PopState();
	
	NS_IMETHOD IsVisibleRect(const nsRect &aRect, PRBool &aVisible);
	
	NS_IMETHOD SetClipRect(const nsRect &aRect, nsClipCombine aCombine);
	NS_IMETHOD GetClipRect(nsRect &aRect, PRBool &aClipValid);
	NS_IMETHOD SetClipRegion(const nsIRegion &aRegion, nsClipCombine aCombine);
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
	
	NS_IMETHOD CreateDrawingSurface(const nsRect& aBounds, PRUint32 aSurfFlags, nsDrawingSurface &aSurface);
	NS_IMETHOD DestroyDrawingSurface(nsDrawingSurface aDS);
	
	NS_IMETHOD DrawLine(nscoord aX0, nscoord aY0, nscoord aX1, nscoord aY1);
	NS_IMETHOD DrawPolyline(const nsPoint aPoints[], PRInt32 aNumPoints);
	
	NS_IMETHOD DrawRect(const nsRect &aRect);
	NS_IMETHOD DrawRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight);
	
	NS_IMETHOD FillRect(const nsRect &aRect);
	NS_IMETHOD FillRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight);
	
	NS_IMETHOD InvertRect(const nsRect &aRect);
	NS_IMETHOD InvertRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight);
	
	NS_IMETHOD DrawPolygon(const nsPoint aPoints[], PRInt32 aNumPoints);
	NS_IMETHOD FillPolygon(const nsPoint aPoints[], PRInt32 aNumPoints);
	
	NS_IMETHOD DrawEllipse(const nsRect &aRect);
	NS_IMETHOD DrawEllipse(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight);
	NS_IMETHOD FillEllipse(const nsRect &aRect);
	NS_IMETHOD FillEllipse(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight);
	
	NS_IMETHOD DrawArc(const nsRect &aRect, float aStartAngle, float aEndAngle);
	NS_IMETHOD DrawArc(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight,
		float aStartAngle, float aEndAngle);
	NS_IMETHOD FillArc(const nsRect &aRect, float aStartAngle, float aEndAngle);
	NS_IMETHOD FillArc(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight,
		float aStartAngle, float aEndAngle);
	
	NS_IMETHOD GetWidth(char aC, nscoord &aWidth);
	NS_IMETHOD GetWidth(PRUnichar aC, nscoord &aWidth, PRInt32 *aFontID);
	NS_IMETHOD GetWidth(const nsString &aString, nscoord &aWidth, PRInt32 *aFontID);
	NS_IMETHOD GetWidth(const char *aString, nscoord &aWidth);
	NS_IMETHOD GetWidth(const char *aString, PRUint32 aLength, nscoord &aWidth);
	NS_IMETHOD GetWidth(const PRUnichar *aString, PRUint32 aLength, nscoord &aWidth,
		PRInt32 *aFontID);
	
	NS_IMETHOD GetTextDimensions(const char *aString, PRUint32 aLength,
		nsTextDimensions &aDimensions);
	NS_IMETHOD GetTextDimensions(const PRUnichar *aString, PRUint32 aLength,
		nsTextDimensions &aDimensions, PRInt32 *aFontID);
	NS_IMETHOD GetTextDimensions(const char*       aString,
								PRInt32           aLength,
								PRInt32           aAvailWidth,
								PRInt32*          aBreaks,
								PRInt32           aNumBreaks,
								nsTextDimensions& aDimensions,
								PRInt32&          aNumCharsFit,
								nsTextDimensions& aLastWordDimensions,
								PRInt32*          aFontID = nsnull);
	NS_IMETHOD GetTextDimensions(const PRUnichar*  aString,
								PRInt32           aLength,
								PRInt32           aAvailWidth,
								PRInt32*          aBreaks,
								PRInt32           aNumBreaks,
								nsTextDimensions& aDimensions,
								PRInt32&          aNumCharsFit,
								nsTextDimensions& aLastWordDimensions,
								PRInt32*          aFontID = nsnull);
                               	
	NS_IMETHOD DrawString(const char *aString, PRUint32 aLength, nscoord aX, nscoord aY,
		const nscoord *aSpacing);
	NS_IMETHOD DrawString(const PRUnichar *aString, PRUint32 aLength, nscoord aX, nscoord aY,
		PRInt32 aFontID, const nscoord *aSpacing);
	NS_IMETHOD DrawString(const nsString &aString, nscoord aX, nscoord aY, PRInt32 aFontID,
		const nscoord *aSpacing);
	
	NS_IMETHOD CopyOffScreenBits(nsDrawingSurface aSrcSurf, PRInt32 aSrcX, PRInt32 aSrcY,
		const nsRect &aDestBounds, PRUint32 aCopyFlags);
	NS_IMETHOD RetrieveCurrentNativeGraphicData(PRUint32 *ngd);
	
	void CreateClipRegion() {
		static NS_DEFINE_CID(kRegionCID, NS_REGION_CID);
		if (mClipRegion) return;
		PRUint32 w, h;
		mSurface->GetSize(&w, &h);
		mClipRegion = do_CreateInstance(kRegionCID);
		if (mClipRegion) {
			mClipRegion->Init();
			mClipRegion->SetTo(0, 0, w, h);
		}
	}

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
                                PRInt32*           aFontID = nsnull);
#endif /* MOZ_MATHML */
  //LockAndUpdateView() - method, similar to UpdateGC (from gtk gfx).
  //Acquires "fresh" drawable mView (BView) from drawing surface, locks it (BeOS specifics),
  //updates font, color and sets clipping region. 
  //In if() statement actually replaces (mView && mView->LockLooper);
  //Each LockAndUpdateView() statement must have UnlockLooper() counterpart somewhere, 
  //if returned true.
  bool LockAndUpdateView();	

protected:
	NS_IMETHOD CommonInit();
	
	// ConditionRect is used to fix coordinate overflow problems for
	// rectangles after they are transformed to screen coordinates
	void ConditionRect(nscoord &x, nscoord &y, nscoord &w, nscoord &h) {
		if (y < -32766) y = -32766;
		if (y + h > 32766) h = 32766 - y;
		if (x < -32766) x = -32766;
		if (x + w > 32766) w = 32766 - x;
	}
	
	nsDrawingSurfaceBeOS *mOffscreenSurface;
	nsDrawingSurfaceBeOS *mSurface;
	nsIDeviceContext *mContext;
	nsIFontMetrics *mFontMetrics;
	nsCOMPtr<nsIRegion> mClipRegion;
	nsVoidArray *mStateCache;
	BView *mView;
	nscolor mCurrentColor;
	BFont *mCurrentFont;
	nsLineStyle mCurrentLineStyle;
	float mP2T;
};

#endif /* nsRenderingContextBeOS_h___ */
