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
#ifndef nsRenderingContextPh_h___
#define nsRenderingContextPh_h___

#include "nsRenderingContextImpl.h"
#include "nsUnitConversion.h"
#include "nsFont.h"
#include "nsIFontMetrics.h"
#include "nsPoint.h"
#include "nsString.h"
#include "nsCRT.h"
#include "nsTransform2D.h"
#include "nsIViewManager.h"
#include "nsIWidget.h"
#include "nsRect.h"
#include "nsImagePh.h"
#include "nsIDeviceContext.h"
#include "nsVoidArray.h"
#include "nsGfxCIID.h"

#include "nsDrawingSurfacePh.h"
#include "nsRegionPh.h"

// Macro for converting from nscolor to PtColor_t
// Photon RGB values are stored as 00 RR GG BB
// nscolor RGB values are 00 BB GG RR
#define NS_TO_PH_RGB(ns) (ns & 0xff) << 16 | (ns & 0xff00) | ((ns >> 16) & 0xff)
#define PH_TO_NS_RGB(ns) (ns & 0xff) << 16 | (ns & 0xff00) | ((ns >> 16) & 0xff)

class nsRenderingContextPh : public nsRenderingContextImpl
{
public:
   nsRenderingContextPh();
   virtual ~nsRenderingContextPh();
   
   NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW
	   
   NS_DECL_ISUPPORTS
	   
   NS_IMETHOD Init(nsIDeviceContext* aContext, nsIWidget *aWindow);

	 inline
   NS_IMETHODIMP Init(nsIDeviceContext* aContext, nsDrawingSurface aSurface)
		{
		mContext = aContext;
		NS_IF_ADDREF(mContext);

		mSurface = (nsDrawingSurfacePh *) aSurface;
		NS_ADDREF(mSurface);
		mOffscreenSurface = mSurface;

		mGC = mSurface->GetGC();
		mOwner = PR_FALSE;
		mSurfaceDC = ((nsDrawingSurfacePh*)mSurface)->GetDC();

		return CommonInit();
		}
   
   inline NS_IMETHODIMP Reset(void) { return NS_OK; }
   
	 inline
   NS_IMETHODIMP GetDeviceContext(nsIDeviceContext *&aContext)
		{ NS_IF_ADDREF( mContext );
			aContext = mContext;
			return NS_OK;
		}
   
	 inline
   NS_IMETHODIMP LockDrawingSurface(PRInt32 aX, PRInt32 aY, PRUint32 aWidth, PRUint32 aHeight,
								 void **aBits, PRInt32 *aStride, PRInt32 *aWidthBytes,
								 PRUint32 aFlags)
		{
		PushState();
		return mSurface->Lock( aX, aY, aWidth, aHeight, aBits, aStride, aWidthBytes, aFlags );
		}

	 inline
   NS_IMETHODIMP UnlockDrawingSurface(void)
		{	PopState();
			mSurface->Unlock();
			return NS_OK;
		}
   
	 inline
   NS_IMETHODIMP SelectOffScreenDrawingSurface(nsDrawingSurface aSurface)
		{ mSurface = ( nsnull==aSurface ) ? mOffscreenSurface : (nsDrawingSurfacePh *) aSurface;
			mSurfaceDC = mSurface->Select( );
			return NS_OK;
		}

	 inline
   NS_IMETHODIMP GetDrawingSurface(nsDrawingSurface *aSurface) { *aSurface = (void *) mSurface; return NS_OK; }

	 inline
   NS_IMETHODIMP GetHints(PRUint32& aResult)
		{ /* this flag indicates that the system prefers 8bit chars over wide chars */
			/* It may or may not be faster under photon... */
			aResult = NS_RENDERING_HINT_FAST_8BIT_TEXT;
			return NS_OK;
		}
   
   NS_IMETHOD PushState(void);
   NS_IMETHOD PopState(void);
   
	 inline
   NS_IMETHODIMP IsVisibleRect( const nsRect& aRect, PRBool &aVisible )
		{ aVisible = PR_TRUE;
			return NS_OK;
		}
   
   NS_IMETHOD SetClipRect(const nsRect& aRect, nsClipCombine aCombine);
   NS_IMETHOD GetClipRect(nsRect &aRect, PRBool &aClipState);
   NS_IMETHOD SetClipRegion(const nsIRegion& aRegion, nsClipCombine aCombine);

	 inline
   NS_IMETHODIMP CopyClipRegion(nsIRegion &aRegion)
		{ if( !mClipRegion ) return NS_ERROR_FAILURE;
			aRegion.SetTo(*NS_STATIC_CAST(nsIRegion*, mClipRegion));
			return NS_OK;
		}

   NS_IMETHOD GetClipRegion(nsIRegion **aRegion);
   
   inline
	 NS_IMETHODIMP SetLineStyle(nsLineStyle aLineStyle)
   { mCurrentLineStyle = aLineStyle;
   	 return NS_OK;
	 }

	 inline
   NS_IMETHODIMP GetLineStyle(nsLineStyle &aLineStyle)
		{ aLineStyle = mCurrentLineStyle;
			return NS_OK;
		}
   
   inline
	 NS_IMETHODIMP SetColor(nscolor aColor) { mCurrentColor = NS_TO_PH_RGB( aColor ); return NS_OK; }
	 inline
   NS_IMETHODIMP GetColor(nscolor &aColor) const { aColor = PH_TO_NS_RGB( mCurrentColor ); return NS_OK; }
   
	 inline
	 NS_IMETHODIMP SetFont(const nsFont& aFont, nsIAtom* aLangGroup)
		{ nsIFontMetrics* newMetrics;
			nsresult rv = mContext->GetMetricsFor( aFont, aLangGroup, newMetrics );
			if( NS_SUCCEEDED( rv ) ) {
			  rv = SetFont( newMetrics );
			  NS_RELEASE( newMetrics );
				}
			return rv;
		}

   NS_IMETHOD SetFont(nsIFontMetrics *aFontMetrics);
   
	 inline	
   NS_IMETHODIMP GetFontMetrics(nsIFontMetrics *&aFontMetrics)
		{ NS_IF_ADDREF(mFontMetrics);
			aFontMetrics = mFontMetrics;
			return NS_OK;
		}
   
	 inline
   NS_IMETHODIMP Translate(nscoord aX, nscoord aY) { mTranMatrix->AddTranslation((float)aX,(float)aY); return NS_OK; }

	 inline
   NS_IMETHODIMP Scale(float aSx, float aSy) { mTranMatrix->AddScale(aSx, aSy); return NS_OK; }

	 inline
   NS_IMETHODIMP GetCurrentTransform(nsTransform2D *&aTransform) { aTransform = mTranMatrix; return NS_OK; }

   
   NS_IMETHOD CreateDrawingSurface(const nsRect &aBounds, PRUint32 aSurfFlags, nsDrawingSurface &aSurface);

	 inline
   NS_IMETHODIMP DestroyDrawingSurface(nsDrawingSurface aDS)
		{ nsDrawingSurfacePh *surf = (nsDrawingSurfacePh *) aDS;
			NS_IF_RELEASE(surf);
			return NS_OK;
		}
   
   NS_IMETHOD DrawLine(nscoord aX0, nscoord aY0, nscoord aX1, nscoord aY1);

	 inline
   NS_IMETHODIMP DrawPolyline(const nsPoint aPoints[], PRInt32 aNumPoints) { return DrawPolygon( aPoints, aNumPoints ); }
   
	 inline
   NS_IMETHODIMP DrawRect(const nsRect& aRect) { return DrawRect( aRect.x, aRect.y, aRect.width, aRect.height ); }

   NS_IMETHOD DrawRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight);
   
	 inline
   NS_IMETHODIMP FillRect(const nsRect& aRect) { return FillRect( aRect.x, aRect.y, aRect.width, aRect.height ); }

   NS_IMETHOD FillRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight);
   
	 inline
   NS_IMETHODIMP InvertRect(const nsRect& aRect) {  return InvertRect( aRect.x, aRect.y, aRect.width, aRect.height ); }

   NS_IMETHOD InvertRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight);
   
   NS_IMETHOD DrawPolygon(const nsPoint aPoints[], PRInt32 aNumPoints);
   NS_IMETHOD FillPolygon(const nsPoint aPoints[], PRInt32 aNumPoints);
   
	 inline
   NS_IMETHODIMP DrawEllipse(const nsRect& aRect) { return DrawEllipse( aRect.x, aRect.y, aRect.width, aRect.height ); }

   NS_IMETHOD DrawEllipse(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight);

	 inline
   NS_IMETHODIMP FillEllipse(const nsRect& aRect) { return FillEllipse( aRect.x, aRect.y, aRect.width, aRect.height ); }

   NS_IMETHOD FillEllipse(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight);
   
	 inline
   NS_IMETHODIMP DrawArc(const nsRect& aRect, float aStartAngle, float aEndAngle) { return DrawArc(aRect.x,aRect.y,aRect.width,aRect.height,aStartAngle,aEndAngle); }

   NS_IMETHOD DrawArc(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight,
					  float aStartAngle, float aEndAngle);

	 inline
   NS_IMETHODIMP FillArc(const nsRect& aRect, float aStartAngle, float aEndAngle) { return FillArc(aRect.x,aRect.y,aRect.width,aRect.height,aStartAngle,aEndAngle); }

   NS_IMETHOD FillArc(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight,
					  float aStartAngle, float aEndAngle);

	 inline
   NS_IMETHODIMP GetWidth(char aC, nscoord& aWidth)
		{ // Check for the very common case of trying to get the width of a single space
			if(aC == ' ')
			  return mFontMetrics->GetSpaceWidth(aWidth);
			return GetWidth( &aC, 1, aWidth );
		}

	 inline
   NS_IMETHODIMP GetWidth(PRUnichar aC, nscoord& aWidth, PRInt32 *aFontID)
		{ /* turn it into a string */
		PRUnichar buf[2];
		buf[0] = aC;
		buf[1] = nsnull;
		return GetWidth( buf, 1, aWidth, aFontID );
		}

	 inline
   NS_IMETHODIMP GetWidth(const nsString& aString, nscoord& aWidth, PRInt32 *aFontID) { return GetWidth( aString.get(), aString.Length(), aWidth, aFontID ); }

	 inline
   NS_IMETHODIMP GetWidth(const char* aString, nscoord& aWidth) { return GetWidth( aString, strlen( aString ), aWidth ); }

   NS_IMETHOD GetWidth(const char* aString, PRUint32 aLength, nscoord& aWidth);
   NS_IMETHOD GetWidth(const PRUnichar* aString, PRUint32 aLength,
					   nscoord& aWidth, PRInt32 *aFontID);
   
   NS_IMETHOD DrawString(const char *aString, PRUint32 aLength,
						 nscoord aX, nscoord aY,
						 const nscoord* aSpacing);

	 inline
   NS_IMETHODIMP DrawString(const PRUnichar *aString, PRUint32 aLength,
						 nscoord aX, nscoord aY,
						 PRInt32 aFontID,
						 const nscoord* aSpacing)
		{
		NS_ConvertUCS2toUTF8 theUnicodeString( aString, aLength );
		const char *p = theUnicodeString.get( );
		return DrawString( p, strlen( p ), aX, aY, aSpacing );
		}

	 inline
   NS_IMETHODIMP DrawString(const nsString& aString, nscoord aX, nscoord aY, PRInt32 aFontID, const nscoord* aSpacing)
		{
		NS_ConvertUCS2toUTF8 theUnicodeString( aString.get(), aString.Length() );
		const char *p = theUnicodeString.get();
		return DrawString( p, strlen( p ), aX, aY, aSpacing );
		}

	 inline
   NS_IMETHODIMP GetTextDimensions(const char* aString, PRUint32 aLength, nsTextDimensions& aDimensions)
		{
		mFontMetrics->GetMaxAscent(aDimensions.ascent);
		mFontMetrics->GetMaxDescent(aDimensions.descent);
		return GetWidth(aString, aLength, aDimensions.width);
		}

   NS_IMETHOD GetTextDimensions(const PRUnichar *aString, PRUint32 aLength,
								nsTextDimensions& aDimensions, PRInt32 *aFontID);

	 inline
   NS_IMETHODIMP DrawImage(nsIImage *aImage, nscoord aX, nscoord aY)
		{
  	// we have to do this here because we are doing a transform below
  	return DrawImage( aImage, aX, aY,
  	                  NSToCoordRound(mP2T * aImage->GetWidth()),
  	                  NSToCoordRound(mP2T * aImage->GetHeight())
  	                );
		}

	 inline
   NS_IMETHODIMP DrawImage(nsIImage *aImage, nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
		{
		nscoord x, y, w, h;
		x = aX;
		y = aY;
		w = aWidth;
		h = aHeight;
		mTranMatrix->TransformCoord(&x, &y, &w, &h);
		return (aImage->Draw(*this, mSurface, x, y, w, h));
		}

	 inline
   NS_IMETHODIMP DrawImage(nsIImage *aImage, const nsRect& aRect) { return DrawImage( aImage, aRect.x, aRect.y, aRect.width, aRect.height ); }

   NS_IMETHOD DrawImage(nsIImage *aImage, const nsRect& aSRect, const nsRect& aDRect);
   
   NS_IMETHOD CopyOffScreenBits(nsDrawingSurface aSrcSurf, PRInt32 aSrcX, PRInt32 aSrcY,
								const nsRect &aDestBounds, PRUint32 aCopyFlags);

	 inline
   NS_IMETHODIMP RetrieveCurrentNativeGraphicData(PRUint32 * ngd)
		{
		if( ngd != nsnull ) *ngd = nsnull;
		return NS_OK;
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

   
   
private:
	inline NS_IMETHODIMP CommonInit()
		{
		if( mContext && mTranMatrix ) {
		  mP2T = mContext->DevUnitsToAppUnits();
		  float app2dev;
		  app2dev = mContext->AppUnitsToDevUnits();
		  mTranMatrix->AddScale(app2dev, app2dev);
		  }
		return NS_OK;
		}

	void ApplyClipping( PhGC_t * );
	void CreateClipRegion( );
  inline void UpdateGC( )
		{
		PgSetGCCx( mSurfaceDC, mGC ); /* new */
		if( mRegionID ) mSurfaceDC->gin.rid = mRegionID;
		ApplyClipping( mGC );
		}

   // ConditionRect is used to fix coordinate overflow problems for
   // rectangles after they are transformed to screen coordinates
   inline void ConditionRect(nscoord &x, nscoord &y, nscoord &w, nscoord &h) {
	   if ( y < -32766 )
		   y = -32766;
	   if ( y + h > 32766 )
		   h  = 32766 - y;
	   if ( x < -32766 )
		   x = -32766;
	   if ( x + w > 32766 ) 
		   w  = 32766 - x;
   }

   PhGC_t             *mGC;
	 PhDrawContext_t		*mSurfaceDC; /* the DC of the mSurface - keep this in sync with mSurface */
	 PhRid_t						mRegionID;
   nscolor            mCurrentColor;
   nsLineStyle        mCurrentLineStyle;
   nsIFontMetrics     *mFontMetrics;
   nsDrawingSurfacePh *mOffscreenSurface;
   nsDrawingSurfacePh *mSurface;
   nsIDeviceContext   *mContext;
   float              mP2T;
   nsCOMPtr<nsIRegion>mClipRegion;
   char               *mPhotonFontName;
	 PRBool							mOwner;
   
   //state management
   nsVoidArray       *mStateCache;
};

#endif /* nsRenderingContextPh_h___ */
