/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsFontMetricsPh.h"
#include "nsGraphicsStatePh.h"
#include "nsGfxCIID.h"
#include "nsRegionPh.h"
#include "nsRenderingContextPh.h"
#include "nsICharRepresentable.h"
#include "nsDeviceContextPh.h"
#include "prprf.h"
#include "nsDrawingSurfacePh.h"

#include <stdlib.h>
#include <mem.h>
#include "nsReadableUtils.h"

static NS_DEFINE_IID(kIRenderingContextIID, NS_IRENDERING_CONTEXT_IID);
static NS_DEFINE_IID(kIDrawingSurfaceIID, NS_IDRAWING_SURFACE_IID);
static NS_DEFINE_IID(kDrawingSurfaceCID, NS_DRAWING_SURFACE_CID);
static NS_DEFINE_CID(kRegionCID, NS_REGION_CID);

// Macro for converting from nscolor to PtColor_t
// Photon RGB values are stored as 00 RR GG BB
// nscolor RGB values are 00 BB GG RR
#define NS_TO_PH_RGB(ns) (ns & 0xff) << 16 | (ns & 0xff00) | ((ns >> 16) & 0xff)
#define PH_TO_NS_RGB(ns) (ns & 0xff) << 16 | (ns & 0xff00) | ((ns >> 16) & 0xff)

#include <prlog.h>
PRLogModuleInfo *PhGfxLog = PR_NewLogModule("PhGfxLog");
#include "nsPhGfxLog.h"

NS_IMPL_ISUPPORTS1(nsRenderingContextPh, nsIRenderingContext)

/* The default Photon Drawing Context */
PhGC_t *nsRenderingContextPh::mPtGC = nsnull;

nsRenderingContextPh :: nsRenderingContextPh() 
{
	NS_INIT_REFCNT();
	
	mGC               = nsnull;
	mTranMatrix          = nsnull;
	mClipRegion       = nsnull;
	mFontMetrics      = nsnull;
	mSurface          = nsnull;
	mOffscreenSurface = nsnull;
	mDCOwner          = nsnull;
	mContext          = nsnull;
	mP2T              = 1.0f;
	mWidget           = nsnull;
	mPhotonFontName   = nsnull;
	mCurrentColor     = NS_RGB(255, 255, 255);
	mCurrentLineStyle = nsLineStyle_kSolid;
	mStateCache       = new nsVoidArray();
	
	if( mPtGC == nsnull ) mPtGC = PgGetGC();
	
	mInitialized   = PR_FALSE;
	PushState();
}

nsRenderingContextPh :: ~nsRenderingContextPh() 
{
	// Destroy the State Machine
	if( mStateCache ) {
	  PRInt32 cnt = mStateCache->Count();
		
		while( --cnt >= 0 ) {
			PRBool  clipstate;
			PopState( clipstate );
		}
		delete mStateCache;
		mStateCache = nsnull;
	}

	if( mTranMatrix ) 
		delete mTranMatrix;
	
	NS_IF_RELEASE( mOffscreenSurface );		/* this also clears mSurface.. or should */
	NS_IF_RELEASE( mFontMetrics );
	NS_IF_RELEASE( mContext );

	/* Go back to the default Photon DrawContext */
	/* This allows the photon widgets under Viewer to work right */
	PgSetGC( mPtGC );
	PgSetRegion( mPtGC->rid );
	
	if( mGC ) {
		PgDestroyGC( mGC );		/* this causes crashes */
		mGC = nsnull;
	}
	if( mPhotonFontName ) 
		delete [] mPhotonFontName;
}


NS_IMETHODIMP nsRenderingContextPh :: Init( nsIDeviceContext* aContext, nsIWidget *aWindow ) 
{
	nsresult res;
	
	mContext = aContext;
	NS_IF_ADDREF(mContext);
	
	mWidget = (PtWidget_t*) aWindow->GetNativeData( NS_NATIVE_WIDGET );
	
	if( !mWidget ) {
		NS_IF_RELEASE( mContext ); // new
		NS_ASSERTION(mWidget,"nsRenderingContext::Init (with a widget) mWidget is NULL!");
		return NS_ERROR_FAILURE;
	}
	
	PhRid_t rid = PtWidgetRid( mWidget );
	if( rid ) {
		mGC = PgCreateGC(0);
		if( !mGC ) 
			return NS_ERROR_FAILURE;
		PgSetDrawBufferSize(65000);
		
		/* Make sure the new GC is reset to the default settings */  
		PgDefaultGC( mGC );
		
		mSurface = new nsDrawingSurfacePh();
		if( mSurface ) {
			res = mSurface->Init( mGC );
			if( res != NS_OK )
				return NS_ERROR_FAILURE;
			
			mOffscreenSurface = mSurface;
			NS_ADDREF( mSurface );
			
			/* hack up code to setup new GC for on screen drawing */
			PgSetGC( mGC );
			PgSetRegion( rid );
		}
		else 
			return NS_ERROR_FAILURE;
	}
	mInitialized = PR_TRUE;
	return CommonInit();
}

NS_IMETHODIMP nsRenderingContextPh :: Init( nsIDeviceContext* aContext, nsDrawingSurface aSurface ) 
{
	mContext = aContext;
	NS_IF_ADDREF(mContext);
	
	mSurface = (nsDrawingSurfacePh *) aSurface;
	NS_ADDREF(mSurface);
	
	mGC = mSurface->GetGC();
	
	mInitialized = PR_TRUE;
	return CommonInit();
}

NS_IMETHODIMP nsRenderingContextPh::CommonInit() 
{
	if( mContext && mTranMatrix ) {
		mContext->GetDevUnitsToAppUnits(mP2T);
		float app2dev;
		mContext->GetAppUnitsToDevUnits(app2dev);
		mTranMatrix->AddScale(app2dev, app2dev);
	}
	return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPh :: GetHints( PRUint32& aResult ) 
{
	PRUint32 result = 0;
	
	// Most X servers implement 8 bit text rendering alot faster than
	// XChar2b rendering. In addition, we can avoid the PRUnichar to
	// XChar2b conversion. So we set this bit...
	result |= NS_RENDERING_HINT_FAST_8BIT_TEXT;
	
	/* this flag indicates that the system prefers 8bit chars over wide chars */
	/* It may or may not be faster under photon... */
	aResult = result;
	return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPh :: LockDrawingSurface( PRInt32 aX, PRInt32 aY,
                                                          PRUint32 aWidth, PRUint32 aHeight,
                                                          void **aBits, PRInt32 *aStride,
                                                          PRInt32 *aWidthBytes, PRUint32 aFlags ) 
{
	PushState();
	return mSurface->Lock( aX, aY, aWidth, aHeight, aBits, aStride, aWidthBytes, aFlags );
}

NS_IMETHODIMP nsRenderingContextPh::UnlockDrawingSurface( void ) 
{
	PRBool  clipstate;
	PopState( clipstate );
	
	mSurface->Unlock();
	
	return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPh :: SelectOffScreenDrawingSurface( nsDrawingSurface aSurface ) 
{
	
	if( nsnull==aSurface ) 
		mSurface = mOffscreenSurface;
	else 
		mSurface = (nsDrawingSurfacePh *) aSurface;
	
	mSurface->Select( );
	return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPh :: GetDrawingSurface( nsDrawingSurface *aSurface ) 
{
	*aSurface = (void *) mSurface;
	return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPh :: Reset( ) { return NS_OK; }

NS_IMETHODIMP nsRenderingContextPh :: GetDeviceContext( nsIDeviceContext *&aContext ) 
{
	NS_IF_ADDREF( mContext );
	aContext = mContext;
	return NS_OK;
}


NS_IMETHODIMP nsRenderingContextPh :: PushState( void ) 
{
	//  Get a new GS
#ifdef USE_GS_POOL
	nsGraphicsState *state = nsGraphicsStatePool::GetNewGS();
#else
	nsGraphicsState *state = new nsGraphicsState;
#endif

	// Push into this state object, add to vector
	if( !state ) {
		NS_ASSERTION(0, "nsRenderingContextPh::PushState Failed to create a new Graphics State");
		return NS_ERROR_FAILURE;
	}
	
	state->mMatrix = mTranMatrix;
	
	if( nsnull == mTranMatrix ) 
		mTranMatrix = new nsTransform2D();
	else 
		mTranMatrix = new nsTransform2D(mTranMatrix);
	
	// set the state's clip region to a new copy of the current clip region
	//  if( mClipRegion ) GetClipRegion( &state->mClipRegion );
	state->mClipRegion = mClipRegion;

	NS_IF_ADDREF(mFontMetrics);
	state->mFontMetrics = mFontMetrics;
	
	state->mColor = mCurrentColor;
	state->mLineStyle = mCurrentLineStyle;
	
	mStateCache->AppendElement(state);
	
	return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPh :: PopState( PRBool &aClipEmpty ) 
{
	PRUint32 cnt = mStateCache->Count();
	nsGraphicsState * state;
	
	if (cnt > 0) {
		state = (nsGraphicsState *)mStateCache->ElementAt(cnt - 1);
		mStateCache->RemoveElementAt(cnt - 1);
		
		// Assign all local attributes from the state object just popped
		if ( state->mMatrix) {
			if (mTranMatrix)
				delete mTranMatrix;
			mTranMatrix = state->mMatrix;
		}
		
		// restore everything
		mClipRegion = state->mClipRegion;

		if( state->mFontMetrics && mFontMetrics != state->mFontMetrics ) 
			SetFont( state->mFontMetrics );
		
		//    if( mSurface && mClipRegion ) ApplyClipping( mGC );
		if( state->mColor != mCurrentColor ) 
			SetColor( state->mColor );
		
		if( state->mLineStyle != mCurrentLineStyle ) 
			SetLineStyle( state->mLineStyle );
		
		// Delete this graphics state object
#ifdef USE_GS_POOL
		nsGraphicsStatePool::ReleaseGS(state);
#else
		delete state;
#endif
	}
	
	if( mClipRegion ) 
		aClipEmpty = mClipRegion->IsEmpty();
	else
		aClipEmpty = PR_TRUE;
	return NS_OK;
}


NS_IMETHODIMP nsRenderingContextPh :: IsVisibleRect( const nsRect& aRect, PRBool &aVisible ) 
{
	aVisible = PR_TRUE;
	return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPh :: GetClipRect( nsRect &aRect, PRBool &aClipValid ) 
{
	PRInt32 x, y, w, h;
	
	if ( !mClipRegion )
		return NS_ERROR_FAILURE;
	
	if( !mClipRegion->IsEmpty() ) {
		mClipRegion->GetBoundingBox( &x, &y, &w, &h );
		aRect.SetRect( x, y, w, h );
		aClipValid = PR_TRUE;
	}
	else {
		aRect.SetRect(0,0,0,0);
		aClipValid = PR_FALSE;
	}
	return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPh :: SetClipRect( const nsRect& aRect, nsClipCombine aCombine, PRBool &aClipEmpty ) 
{
	nsresult   res = NS_ERROR_FAILURE;
	nsRect     trect = aRect;
    PRUint32 cnt = mStateCache->Count();
	nsGraphicsState *state = nsnull;
	
	if (cnt > 0) {
		state = (nsGraphicsState *)mStateCache->ElementAt(cnt - 1);
	}
	
	if (state) {
		if (state->mClipRegion) {
			if (state->mClipRegion == mClipRegion) {
				nsCOMPtr<nsIRegion> tmpRgn;
				GetClipRegion(getter_AddRefs(tmpRgn));
				mClipRegion = tmpRgn;
			}
		}
	}
	
	CreateClipRegion();
	
	if( mTranMatrix && mClipRegion ) {
		mTranMatrix->TransformCoord( &trect.x, &trect.y,&trect.width, &trect.height );
		switch( aCombine ) {
			case nsClipCombine_kIntersect:
			mClipRegion->Intersect(trect.x,trect.y,trect.width,trect.height);
			break;
			case nsClipCombine_kUnion:
			mClipRegion->Union(trect.x,trect.y,trect.width,trect.height);
			break;
			case nsClipCombine_kSubtract:
			mClipRegion->Subtract(trect.x,trect.y,trect.width,trect.height);
			break;
			case nsClipCombine_kReplace:
			mClipRegion->SetTo(trect.x,trect.y,trect.width,trect.height);
			break;
			default:
			break;
		}
		
		aClipEmpty = mClipRegion->IsEmpty();
		res = NS_OK;
	}
	
	return res;
}

#if 0
NS_IMETHODIMP nsRenderingContextPh :: SetClipRegion( PhTile_t *aTileList, nsClipCombine aCombine, PRBool &aClipEmpty ) {
  nsRegionPh region( aTileList );
  return SetClipRegion( region, aCombine, aClipEmpty );
	}
#endif

NS_IMETHODIMP nsRenderingContextPh :: SetClipRegion( const nsIRegion& aRegion, nsClipCombine aCombine, PRBool &aClipEmpty ) 
{
	PRUint32 cnt = mStateCache->Count();
	nsGraphicsState *state = nsnull;
	
	if (cnt > 0) {
		state = (nsGraphicsState *)mStateCache->ElementAt(cnt - 1);
	}
	
	if (state) {
		if (state->mClipRegion) {
			if (state->mClipRegion == mClipRegion) {
				nsCOMPtr<nsIRegion> tmpRgn;
				GetClipRegion(getter_AddRefs(tmpRgn));
				mClipRegion = tmpRgn;
			}
		}
	}
	
	CreateClipRegion();
	
	switch( aCombine ) {
		case nsClipCombine_kIntersect:
		mClipRegion->Intersect(aRegion);
		break;
		case nsClipCombine_kUnion:
		mClipRegion->Union(aRegion);
		break;
		case nsClipCombine_kSubtract:
		mClipRegion->Subtract(aRegion);
		break;
		case nsClipCombine_kReplace:
		mClipRegion->SetTo(aRegion);
		break;
	}
	
	aClipEmpty = mClipRegion->IsEmpty();
//	ApplyClipping(mGC);
	return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPh :: CopyClipRegion( nsIRegion &aRegion ) 
{
	aRegion.SetTo(*NS_STATIC_CAST(nsIRegion*, mClipRegion));
	return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsRenderingContextPh :: GetClipRegion( nsIRegion **aRegion ) 
{
	nsresult rv = NS_ERROR_FAILURE;
	
	if (!aRegion || !mClipRegion)
		return NS_ERROR_NULL_POINTER;
	
	if (mClipRegion) {
		if (*aRegion) { // copy it, they should be using CopyClipRegion
			  (*aRegion)->SetTo(*mClipRegion);
			  rv = NS_OK;
		  } else {
				  nsCOMPtr<nsIRegion> newRegion = do_CreateInstance(kRegionCID, &rv);
				  if (NS_SUCCEEDED(rv)) {
					  newRegion->Init();
					  newRegion->SetTo(*mClipRegion);
					  NS_ADDREF(*aRegion = newRegion);
				  }
			  }
	} else {
		rv = NS_ERROR_FAILURE;
	}
	return rv;
}

NS_IMETHODIMP nsRenderingContextPh :: SetColor( nscolor aColor ) 
{
	if( nsnull == mContext ) 
		return NS_ERROR_FAILURE;
	mCurrentColor = aColor;
	return NS_OK;
}


NS_IMETHODIMP nsRenderingContextPh :: GetColor( nscolor &aColor ) const 
{
	aColor = mCurrentColor;
	return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPh :: SetLineStyle( nsLineStyle aLineStyle ) 
{
	mCurrentLineStyle = aLineStyle;
	switch( mCurrentLineStyle ) {
		case nsLineStyle_kSolid:
		mLineStyle[0] = 0;
		break;
		case nsLineStyle_kDashed:
		mLineStyle[0] = 10;
		mLineStyle[1] = 4;
		break;
		case nsLineStyle_kDotted:
		mLineStyle[0] = 1;
		mLineStyle[1] = 0;
		break;
		case nsLineStyle_kNone:
		default:
		break;
	}
	return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPh :: GetLineStyle( nsLineStyle &aLineStyle ) 
{
	aLineStyle = mCurrentLineStyle;
	return NS_OK;
}


NS_IMETHODIMP nsRenderingContextPh :: SetFont( const nsFont& aFont ) 
{
	nsIFontMetrics* newMetrics;
	nsresult rv = mContext->GetMetricsFor( aFont, newMetrics );
	if( NS_SUCCEEDED( rv ) ) {
		rv = SetFont( newMetrics );
		NS_RELEASE( newMetrics );
	}
	return rv;
}


NS_IMETHODIMP nsRenderingContextPh :: SetFont( nsIFontMetrics *aFontMetrics ) 
{
	if( mFontMetrics == aFontMetrics ) return NS_OK;
	
	nsFontHandle  fontHandle;			/* really a nsString */
	char      *pFontHandle;
	
	NS_IF_RELEASE(mFontMetrics);
	mFontMetrics = aFontMetrics;
	NS_IF_ADDREF(mFontMetrics);
	
  if( mFontMetrics == nsnull ) return NS_OK;
	
	mFontMetrics->GetFontHandle( fontHandle );
	pFontHandle = (char *) fontHandle;
    
	if( pFontHandle ) {
		if( mPhotonFontName ) free( mPhotonFontName );
		mPhotonFontName = strdup( pFontHandle );
	}
	
	return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPh :: GetFontMetrics( nsIFontMetrics *&aFontMetrics ) 
{
	NS_IF_ADDREF(mFontMetrics);
	aFontMetrics = mFontMetrics;
	return NS_OK;
}

// add the passed in translation to the current translation
NS_IMETHODIMP nsRenderingContextPh :: Translate( nscoord aX, nscoord aY ) 
{
	mTranMatrix->AddTranslation((float)aX,(float)aY);
	return NS_OK;
}

// add the passed in scale to the current scale
NS_IMETHODIMP nsRenderingContextPh :: Scale( float aSx, float aSy ) 
{
	mTranMatrix->AddScale(aSx, aSy);
	return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPh :: GetCurrentTransform( nsTransform2D *&aTransform ) 
{
	aTransform = mTranMatrix;
	return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPh :: CreateDrawingSurface( nsRect *aBounds, PRUint32 aSurfFlags, nsDrawingSurface &aSurface ) 
{
	if( nsnull == mSurface ) {
		aSurface = nsnull;
		return NS_ERROR_FAILURE;
	}
	
	nsDrawingSurfacePh *surf = new nsDrawingSurfacePh();
	if( surf ) {
		NS_ADDREF(surf);
		PhGC_t *gc = mSurface->GetGC();
		surf->Init( gc, aBounds->width, aBounds->height, aSurfFlags );
	}
	else 
		return NS_ERROR_FAILURE;
	
	aSurface = (nsDrawingSurface) surf;
	
	return NS_OK;
}


NS_IMETHODIMP nsRenderingContextPh :: DestroyDrawingSurface( nsDrawingSurface aDS ) 
{
	nsDrawingSurfacePh *surf = (nsDrawingSurfacePh *) aDS;
	NS_IF_RELEASE(surf);
	return NS_OK;
}


NS_IMETHODIMP nsRenderingContextPh :: DrawLine( nscoord aX0, nscoord aY0, nscoord aX1, nscoord aY1 ) 
{
	mTranMatrix->TransformCoord( &aX0, &aY0 );
	mTranMatrix->TransformCoord( &aX1, &aY1 );
	
	if( aY0 != aY1 ) aY1--;
	if( aX0 != aX1 ) aX1--;
	
	UpdateGC();
	
	PgDrawILine( aX0, aY0, aX1, aY1 );
	return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPh :: DrawStdLine( nscoord aX0, nscoord aY0, nscoord aX1, nscoord aY1 ) 
{
	
	if( aY0 != aY1 ) aY1--;
	if( aX0 != aX1 ) aX1--;
	
	UpdateGC();
	
	PgDrawILine( aX0, aY0, aX1, aY1 );
	return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPh :: DrawPolyline( const nsPoint aPoints[], PRInt32 aNumPoints ) 
{
	return DrawPolygon( aPoints, aNumPoints );
}

NS_IMETHODIMP nsRenderingContextPh :: DrawRect( const nsRect& aRect ) 
{
	return DrawRect( aRect.x, aRect.y, aRect.width, aRect.height );
}

NS_IMETHODIMP nsRenderingContextPh :: DrawRect( nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight ) 
{
	nscoord x,y,w,h;
	
	x = aX;
	y = aY;
	w = aWidth;
	h = aHeight;
	mTranMatrix->TransformCoord( &x, &y, &w, &h );
	
	UpdateGC();	
	ConditionRect(x,y,w,h);	
	if( w && h )
		PgDrawIRect( x, y, x + w - 1, y + h - 1, Pg_DRAW_STROKE );
	return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPh :: FillRect( const nsRect& aRect ) 
{
	return FillRect( aRect.x, aRect.y, aRect.width, aRect.height );
}

NS_IMETHODIMP nsRenderingContextPh :: FillRect( nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight ) 
{
	nscoord x,y,w,h;
	
	x = aX;
	y = aY;
	w = aWidth;
	h = aHeight;
	
	mTranMatrix->TransformCoord( &x, &y, &w, &h );
	
	UpdateGC();
	ConditionRect(x,y,w,h);	
	if( w && h ) {
		int y2 = y + h - 1;
		if( y < SHRT_MIN ) y = SHRT_MIN;			/* on very large documents, the PgDrawIRect will take only the short part from the int, which could lead to randomly, hazardous results see PR: 5864 */
		if( y2 >= SHRT_MAX ) y2 = SHRT_MAX;		/* on very large documents, the PgDrawIRect will take only the short part from the int, which could lead to randomly, hazardous results see PR: 5864 */
		
		PgDrawIRect( x, y, x + w - 1, y2, Pg_DRAW_FILL );
	}
	return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPh :: InvertRect( const nsRect& aRect ) 
{
	return InvertRect( aRect.x, aRect.y, aRect.width, aRect.height );
}

NS_IMETHODIMP nsRenderingContextPh :: InvertRect( nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight ) 
{
	if( nsnull == mTranMatrix || nsnull == mSurface ) return NS_ERROR_FAILURE; 
	nscoord x,y,w,h;
	
	x = aX;
	y = aY;
	w = aWidth;
	h = aHeight;
	mTranMatrix->TransformCoord(&x,&y,&w,&h);
	
	if( !w || !h ) return NS_OK;
	
	UpdateGC();	
	ConditionRect(x,y,w,h);
	PgSetFillColor(Pg_INVERT_COLOR);
	PgSetDrawMode(Pg_DRAWMODE_XOR);
	PgDrawIRect( x, y, x + w - 1, y + h - 1, Pg_DRAW_FILL );
	PgSetDrawMode(Pg_DRAWMODE_OPAQUE);
//	mSurface->Flush();
	return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPh :: DrawPolygon( const nsPoint aPoints[], PRInt32 aNumPoints ) 
{
	PhPoint_t *pts;
	
	if( !aNumPoints ) return NS_OK;
	
	if( (pts = new PhPoint_t [aNumPoints]) != NULL ) {
		PhPoint_t pos = {0,0};
		PRInt32 i;
		int x,y;
		
		for( i = 0; i < aNumPoints; i++ ) {
			x = aPoints[i].x;
			y = aPoints[i].y;
			mTranMatrix->TransformCoord(&x, &y);
			pts[i].x = x;
			pts[i].y = y;
		}
		UpdateGC();
		PgDrawPolygon( pts, aNumPoints, &pos, Pg_DRAW_STROKE );
		delete [] pts;
	}
	return NS_OK;
}


NS_IMETHODIMP nsRenderingContextPh :: FillPolygon( const nsPoint aPoints[], PRInt32 aNumPoints ) 
{
	PhPoint_t *pts;
	
	if( !aNumPoints ) return NS_OK;
	
	if( (pts = new PhPoint_t [aNumPoints]) != NULL ) {
		PhPoint_t pos = {0,0};
		PRInt32 i;
		int x,y;
		
		for( i = 0; i < aNumPoints; i++ ) {
			x = aPoints[i].x;
			y = aPoints[i].y;
			mTranMatrix->TransformCoord(&x, &y);
			pts[i].x = x;
			pts[i].y = y;
		}
		UpdateGC();
		PgDrawPolygon( pts, aNumPoints, &pos, Pg_DRAW_FILL );
		delete [] pts;
	}
	return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPh :: DrawEllipse( const nsRect& aRect ) 
{
	DrawEllipse( aRect.x, aRect.y, aRect.width, aRect.height );
	return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPh :: DrawEllipse( nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight ) 
{
	nscoord x,y,w,h;
	PhPoint_t center;
	PhPoint_t radii;
	
	x = aX;
	y = aY;
	w = aWidth;
	h = aHeight;
	mTranMatrix->TransformCoord( &x, &y, &w, &h );
	
	center.x = x;
	center.y = y;
	radii.x = x+w-1;
	radii.y = y+h-1;
	UpdateGC();
	PgDrawEllipse( &center, &radii, Pg_EXTENT_BASED | Pg_DRAW_STROKE );
	return NS_OK;
}


NS_IMETHODIMP nsRenderingContextPh :: FillEllipse( const nsRect& aRect ) 
{
	FillEllipse( aRect.x, aRect.y, aRect.width, aRect.height );
	return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPh :: FillEllipse( nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight ) 
{
	nscoord x,y,w,h;
	PhPoint_t center;
	PhPoint_t radii;
	
	x = aX;
	y = aY;
	w = aWidth;
	h = aHeight;
	mTranMatrix->TransformCoord(&x,&y,&w,&h);
	
	center.x = x;
	center.y = y;
	radii.x = x+w-1;
	radii.y = y+h-1;
	
	UpdateGC();
	PgDrawEllipse( &center, &radii, Pg_EXTENT_BASED | Pg_DRAW_FILL );
	return NS_OK;
}


NS_IMETHODIMP nsRenderingContextPh :: DrawArc( const nsRect& aRect, float aStartAngle, float aEndAngle ) 
{
	return DrawArc(aRect.x,aRect.y,aRect.width,aRect.height,aStartAngle,aEndAngle);
}


NS_IMETHODIMP nsRenderingContextPh :: DrawArc(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight, float aStartAngle, float aEndAngle ) 
{
	nscoord x,y,w,h;
	PhPoint_t center;
	PhPoint_t radii;
	
	x = aX;
	y = aY;
	w = aWidth;
	h = aHeight;
	
	mTranMatrix->TransformCoord(&x,&y,&w,&h);
	
	center.x = x;
	center.y = y;
	radii.x = x+w-1;
	radii.y = y+h-1;
	
	UpdateGC();
	PgDrawArc( &center, &radii, (unsigned int)aStartAngle, (unsigned int)aEndAngle, Pg_EXTENT_BASED | Pg_DRAW_STROKE );
	return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPh :: FillArc( const nsRect& aRect, float aStartAngle, float aEndAngle ) 
{
	return FillArc(aRect.x,aRect.y,aRect.width,aRect.height,aStartAngle,aEndAngle);
}

NS_IMETHODIMP nsRenderingContextPh :: FillArc( nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight, float aStartAngle, float aEndAngle ) 
{
	nscoord x,y,w,h;
	PhPoint_t center;
	PhPoint_t radii;
	
	x = aX;
	y = aY;
	w = aWidth;
	h = aHeight;
	mTranMatrix->TransformCoord(&x,&y,&w,&h);
	
	center.x = x;
	center.y = y;
	radii.x = x+w-1;
	radii.y = y+h-1;
	
	UpdateGC();
	PgDrawArc( &center, &radii, (unsigned int)aStartAngle, (unsigned int)aEndAngle, Pg_EXTENT_BASED | Pg_DRAW_FILL );
	return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPh :: GetWidth( char ch, nscoord& aWidth ) 
{
	// Check for the very common case of trying to get the width of a single
	// space.
	if(ch == ' ' && nsnull != mFontMetrics ) {
		return mFontMetrics->GetSpaceWidth(aWidth);
	}
	return GetWidth( &ch, 1, aWidth );
}


NS_IMETHODIMP nsRenderingContextPh :: GetWidth( PRUnichar ch, nscoord &aWidth, PRInt32 *aFontID ) 
{
	PRUnichar buf[2];
	
	/* turn it into a string */
	buf[0] = ch;
	buf[1] = nsnull;
	return GetWidth( buf, 1, aWidth, aFontID );
}

NS_IMETHODIMP nsRenderingContextPh :: GetWidth( const char* aString, nscoord& aWidth ) 
{
	return GetWidth( aString, strlen( aString ), aWidth );
}

NS_IMETHODIMP nsRenderingContextPh :: GetWidth(const char* aString, PRUint32 aLength, nscoord& aWidth ) 
{
	PhRect_t extent;
	nsresult ret_code = NS_ERROR_FAILURE;
	
	aWidth = 0;	// Initialize to zero in case we fail.
	
	if( nsnull != mFontMetrics ) 
	{
#if 0
		PhRect_t      extentTail;
		
		//using "M" to get rid of the right bearing of the right most char of the string.
		
		nsCString strTail("M");
		char* tail=(char*)strTail.ToNewUnicode();
		PRUint32 tailLength=strlen(tail);	
		char* text = (char*) nsMemory::Alloc(aLength + tailLength + 2);
		
		PRUint32 i;
		for(i=0;i<aLength;i++)
			text[i]=aString[i];
		for(i=0;i<tailLength;i++)
			text[aLength+i] = tail[i];
		text[aLength+tailLength]='\0';
		text[aLength+tailLength+1]='\0';
		
		if( PfExtentText( &extent, NULL, mPhotonFontName, text, aLength+tailLength ) &&
		   PfExtentText( &extentTail, NULL, mPhotonFontName, tail, tailLength)) 
		{
			aWidth = NSToCoordRound((int) ((extent.lr.x - extent.ul.x -extentTail.lr.x + extentTail.ul.x) * mP2T));
			ret_code = NS_OK;
		}
		nsMemory::Free(text);
#else
		if( PfExtentText( &extent, NULL, mPhotonFontName, aString, aLength ) )
		{
			aWidth = NSToCoordRound((int) ((extent.lr.x - extent.ul.x + 1) * mP2T));
			ret_code = NS_OK;
		}
#endif		
	}
	else 
		ret_code = NS_ERROR_FAILURE;
	
	return ret_code;
}

NS_IMETHODIMP nsRenderingContextPh :: GetWidth( const nsString& aString, nscoord& aWidth, PRInt32 *aFontID ) 
{
	return GetWidth( aString.get(), aString.Length(), aWidth, aFontID );  
}


NS_IMETHODIMP nsRenderingContextPh :: GetWidth( const PRUnichar *aString, PRUint32 aLength, nscoord &aWidth, PRInt32 *aFontID ) 
{
	nsresult ret_code = NS_ERROR_FAILURE;
	
	aWidth = 0;	// Initialize to zero in case we fail.
	if( nsnull != mFontMetrics ) {
		NS_ConvertUCS2toUTF8    theUnicodeString (aString, aLength);
		ret_code = GetWidth( theUnicodeString.get(), strlen(theUnicodeString.get()), aWidth );
	}
	if( nsnull != aFontID ) 
		*aFontID = 0;
	return ret_code;
}

NS_IMETHODIMP nsRenderingContextPh::GetTextDimensions(const char* aString, PRUint32 aLength, nsTextDimensions& aDimensions)
{
	aDimensions.Clear();
	mFontMetrics->GetMaxAscent(aDimensions.ascent);
	mFontMetrics->GetMaxDescent(aDimensions.descent);
	return GetWidth(aString, aLength, aDimensions.width);
}

NS_IMETHODIMP nsRenderingContextPh::GetTextDimensions(const PRUnichar* aString, PRUint32 aLength,
													  nsTextDimensions& aDimensions, PRInt32* aFontID)
{
	nsresult ret_code = NS_ERROR_FAILURE;

	aDimensions.Clear();
	if( nsnull != mFontMetrics ) {
		mFontMetrics->GetMaxAscent(aDimensions.ascent);
		mFontMetrics->GetMaxDescent(aDimensions.descent);
		
		NS_ConvertUCS2toUTF8    theUnicodeString (aString, aLength);
		ret_code = GetWidth( theUnicodeString.get(), strlen(theUnicodeString.get()), aDimensions.width );
	}
	if (nsnull != aFontID)
		*aFontID = 0;
	return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPh :: DrawString( const nsString& aString, nscoord aX, nscoord aY, PRInt32 aFontID, const nscoord* aSpacing ) 
{
	NS_ConvertUCS2toUTF8 theUnicodeString( aString.get(), aString.Length() );
	const char *p = theUnicodeString.get();
	return DrawString( p, strlen( p ), aX, aY, aSpacing );
}

NS_IMETHODIMP nsRenderingContextPh::DrawString(const char *aString, PRUint32 aLength,
												nscoord aX, nscoord aY,
												const nscoord* aSpacing)
{
	if ( aLength == 0 )
		return NS_OK;

	UpdateGC();
	
	PgSetFont( mPhotonFontName );

	if( !aSpacing ) {
		mTranMatrix->TransformCoord( &aX, &aY );
		PhPoint_t pos = { aX, aY };
		PgDrawTextChars( aString, aLength, &pos, Pg_TEXT_LEFT);
		}
	else {
		nscoord x = aX;
		nscoord y = aY;
		const char* end = aString + aLength;
		while( aString < end ) {
			char ch = *aString++;
			nscoord xx = x;
			nscoord yy = y;
			mTranMatrix->TransformCoord(&xx, &yy);
			PhPoint_t pos = { xx, yy };
			PgDrawText( &ch, 1, &pos, Pg_TEXT_LEFT);
			x += *aSpacing++;
		}
	}
	return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPh::DrawString(const PRUnichar* aString, PRUint32 aLength,
												 nscoord aX, nscoord aY,
												 PRInt32 aFontID,
												const nscoord* aSpacing)
{
	NS_ConvertUCS2toUTF8 theUnicodeString( aString, aLength );
	const char *p = theUnicodeString.get( );
	return DrawString( p, strlen( p ), aX, aY, aSpacing );
}

NS_IMETHODIMP nsRenderingContextPh::DrawImage( nsIImage *aImage, nscoord aX, nscoord aY ) 
{
	nscoord width, height;
	
	// we have to do this here because we are doing a transform below
	width = NSToCoordRound(mP2T * aImage->GetWidth());
	height = NSToCoordRound(mP2T * aImage->GetHeight());
	
	return DrawImage( aImage, aX, aY, width, height );
}

NS_IMETHODIMP nsRenderingContextPh::DrawImage( nsIImage *aImage, const nsRect& aRect ) 
{
	return DrawImage( aImage, aRect.x, aRect.y, aRect.width, aRect.height );
}

NS_IMETHODIMP nsRenderingContextPh::DrawImage( nsIImage *aImage, nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight ) 
{
	nscoord x, y, w, h;
	
	x = aX;
	y = aY;
	w = aWidth;
	h = aHeight;
	mTranMatrix->TransformCoord(&x, &y, &w, &h);
	return (aImage->Draw(*this, mSurface, x, y, w, h));
}

NS_IMETHODIMP nsRenderingContextPh::DrawImage( nsIImage *aImage, const nsRect& aSRect, const nsRect& aDRect ) 
{
	nsRect    sr,dr;
	
	sr = aSRect;
	mTranMatrix->TransformCoord(&sr.x, &sr.y, &sr.width, &sr.height);
	sr.x -= mTranMatrix->GetXTranslationCoord();
	sr.y -= mTranMatrix->GetYTranslationCoord();
	
	dr = aDRect;
	mTranMatrix->TransformCoord(&dr.x, &dr.y, &dr.width, &dr.height);
	
	return aImage->Draw(*this, mSurface,
						sr.x, sr.y,
						sr.width, sr.height,
						dr.x, dr.y,
						dr.width, dr.height);
}

/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 *	@update 3/16/00 dwc
 */
NS_IMETHODIMP nsRenderingContextPh::DrawTile( nsIImage *aImage,nscoord aX0,nscoord aY0,nscoord aX1,nscoord aY1, nscoord aWidth,nscoord aHeight ) 
{
	mTranMatrix->TransformCoord(&aX0,&aY0,&aWidth,&aHeight);
	mTranMatrix->TransformCoord(&aX1,&aY1);
	
	nsRect srcRect (0, 0, aWidth,  aHeight);
	nsRect tileRect(aX0, aY0, aX1-aX0, aY1-aY0);
	
	((nsImagePh*)aImage)->DrawTile(*this, mSurface, srcRect, tileRect);
	return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPh::DrawTile( nsIImage *aImage, nscoord aSrcXOffset, nscoord aSrcYOffset, const nsRect &aTileRect ) 
{
	nsRect tileRect( aTileRect );
	nsRect srcRect(0, 0, aSrcXOffset, aSrcYOffset);
	mTranMatrix->TransformCoord(&srcRect.x, &srcRect.y, &srcRect.width, &srcRect.height);
	mTranMatrix->TransformCoord(&tileRect.x, &tileRect.y, &tileRect.width, &tileRect.height);
	
	if( tileRect.width > 0 && tileRect.height > 0 )
		((nsImagePh*)aImage)->DrawTile(*this, mSurface, srcRect.width, srcRect.height, tileRect);
	else
		NS_ASSERTION(aTileRect.width > 0 && aTileRect.height > 0,
			   "You can't draw an image with a 0 width or height!");
	return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPh :: CopyOffScreenBits( nsDrawingSurface aSrcSurf, PRInt32 aSrcX, PRInt32 aSrcY, const nsRect &aDestBounds, PRUint32 aCopyFlags ) 
{
	PhArea_t              darea, sarea;
	PRInt32               srcX = aSrcX;
	PRInt32               srcY = aSrcY;
	nsRect                drect = aDestBounds;
	nsDrawingSurfacePh    *destsurf;

	if( !aSrcSurf || !mTranMatrix || !mSurface ) return NS_ERROR_FAILURE;
	
	if( aCopyFlags & NS_COPYBITS_TO_BACK_BUFFER ) {
		NS_ASSERTION(!(nsnull == mSurface), "no back buffer");
		destsurf = mSurface;
	}
	else 
		destsurf = mOffscreenSurface;
	
	if( aCopyFlags & NS_COPYBITS_XFORM_SOURCE_VALUES )	
		mTranMatrix->TransformCoord( &srcX, &srcY );
	if( aCopyFlags & NS_COPYBITS_XFORM_DEST_VALUES )
		mTranMatrix->TransformCoord( &drect.x, &drect.y, &drect.width, &drect.height );

	destsurf->Select();
	UpdateGC();
	(PgGetGC())->target_rid = 0;  // kedl, fix the animations showing thru all regions

	sarea.pos.x = srcX;
	sarea.pos.y = srcY;
	sarea.size.w = drect.width;
	sarea.size.h = drect.height;
	darea.pos.x = drect.x;
	darea.pos.y = drect.y;
	darea.size.w = sarea.size.w;
	darea.size.h = sarea.size.h;
	PgContextBlitArea( (PdOffscreenContext_t *) ((nsDrawingSurfacePh *)aSrcSurf)->GetDC(), &sarea,  
					   (PdOffscreenContext_t *) ((nsDrawingSurfacePh *)destsurf)->GetDC(), &darea );
	PgFlush();
	return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPh::RetrieveCurrentNativeGraphicData( PRUint32 * ngd ) 
{
	if( ngd != nsnull ) *ngd = nsnull;
	return NS_OK;
}

void nsRenderingContextPh::UpdateGC()
{
	PgSetGC(mGC);	/* new */
	PgSetStrokeColor(NS_TO_PH_RGB(mCurrentColor));
	PgSetTextColor(NS_TO_PH_RGB(mCurrentColor));
	PgSetFillColor(NS_TO_PH_RGB(mCurrentColor));
	PgSetStrokeDash(mLineStyle, strlen((char *)mLineStyle), 0x10000);
	
//	  valuesMask = GdkGCValuesMask(valuesMask | GDK_GC_FUNCTION);
//	  values.function = mFunction;
	
	ApplyClipping(mGC);
}

void nsRenderingContextPh::ApplyClipping( PhGC_t *gc ) 
{
	
	if( mClipRegion ) {
		PhTile_t    *tiles = nsnull;
		PhRect_t    *rects = nsnull;
		int         rect_count;
		
		/* no offset needed use the normal tile list */
		mClipRegion->GetNativeRegion((void*&)tiles);
		
		if( tiles != nsnull ) {
			rects = PhTilesToRects( tiles, &rect_count );
			PgSetMultiClip( rect_count, rects );
//			int i;
//			for(i = 0;i < rect_count;i++) {			
//				PgSetStrokeColor(Pg_RED);
//				PgDrawRect(&rects[i],Pg_DRAW_STROKE);
//			}
			
			
			free( rects );
		}
		else PgSetMultiClip( 0, NULL );
	}
}
