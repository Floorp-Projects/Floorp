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
 *   Daniel Switkin and Mathias Agopian
 *   Sergei Dolgov           <sergei_d@fi.tartu.ee>
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

#include "nsFontMetricsBeOS.h"
#include "nsRenderingContextBeOS.h"
#include "nsRegionBeOS.h"
#include "nsImageBeOS.h"
#include "nsGraphicsStateBeOS.h"
#include "nsICharRepresentable.h"
#include "prenv.h"
#include <Polygon.h>
#include <math.h>


NS_IMPL_ISUPPORTS1(nsRenderingContextBeOS, nsIRenderingContext)

static NS_DEFINE_CID(kRegionCID, NS_REGION_CID);

nsRenderingContextBeOS::nsRenderingContextBeOS() {
	mOffscreenSurface = nsnull;
	mSurface = nsnull;
	mContext = nsnull;
	mFontMetrics = nsnull;
	mClipRegion = nsnull;
	mStateCache = new nsVoidArray();
	mView = nsnull;	
	mCurrentColor = NS_RGB(255, 255, 255);
	mCurrentFont = nsnull;
	mCurrentLineStyle = nsLineStyle_kSolid;
	mP2T = 1.0f;
	mTranMatrix = nsnull;

	PushState();
}

nsRenderingContextBeOS::~nsRenderingContextBeOS() {
	// Destroy the State Machine
	if (mStateCache) {
		PRInt32 cnt = mStateCache->Count();
		while (--cnt >= 0)
			PopState();
		delete mStateCache;
		mStateCache = nsnull;
	}
	
	delete mTranMatrix;
	NS_IF_RELEASE(mOffscreenSurface);
	NS_IF_RELEASE(mFontMetrics);
	NS_IF_RELEASE(mContext);
}


NS_IMETHODIMP nsRenderingContextBeOS::Init(nsIDeviceContext *aContext, nsIWidget *aWindow) {	
	mContext = aContext;
	NS_IF_ADDREF(mContext);
	
	mSurface = new nsDrawingSurfaceBeOS();
	if (mSurface) {
		if (!aWindow) return NS_ERROR_NULL_POINTER;
		BView *view = (BView *)aWindow->GetNativeData(NS_NATIVE_GRAPHIC);
		mSurface->Init(view);
		mOffscreenSurface = mSurface;
		NS_ADDREF(mSurface);
	}
	
	return (CommonInit());
}

NS_IMETHODIMP nsRenderingContextBeOS::Init(nsIDeviceContext *aContext, nsDrawingSurface aSurface) {
	mContext = aContext;
	NS_IF_ADDREF(mContext);
	mSurface = (nsDrawingSurfaceBeOS *)aSurface;
	NS_ADDREF(mSurface);
	return (CommonInit());
}

NS_IMETHODIMP nsRenderingContextBeOS::CommonInit() {
  if (!mTranMatrix)
    return NS_ERROR_OUT_OF_MEMORY;

  mP2T = mContext->DevUnitsToAppUnits();
  float app2dev;
  app2dev = mContext->AppUnitsToDevUnits();
  mTranMatrix->AddScale(app2dev, app2dev);
  return NS_OK;
}

// We like PRUnichar rendering, hopefully it's not slowing us too much
NS_IMETHODIMP nsRenderingContextBeOS::GetHints(PRUint32 &aResult) {
	aResult = 0;
	if (!PR_GetEnv("MOZILLA_GFX_DISABLE_FAST_MEASURE")) {
		aResult = NS_RENDERING_HINT_FAST_MEASURE;	
	}
	return NS_OK;
}

NS_IMETHODIMP nsRenderingContextBeOS::LockDrawingSurface(PRInt32 aX, PRInt32 aY, PRUint32 aWidth,
	PRUint32 aHeight, void **aBits, PRInt32 *aStride, PRInt32 *aWidthBytes, PRUint32 aFlags) {
	
	PushState();
	return mSurface->Lock(aX, aY, aWidth, aHeight, aBits, aStride, aWidthBytes, aFlags);
}

NS_IMETHODIMP nsRenderingContextBeOS::UnlockDrawingSurface() {
	PopState();
	mSurface->Unlock();
	return NS_OK;
}

NS_IMETHODIMP nsRenderingContextBeOS::SelectOffScreenDrawingSurface(nsDrawingSurface aSurface) {
	if (nsnull == aSurface) mSurface = mOffscreenSurface;
	else mSurface = (nsDrawingSurfaceBeOS *)aSurface;
	return NS_OK;
}

NS_IMETHODIMP nsRenderingContextBeOS::GetDrawingSurface(nsDrawingSurface *aSurface) {
	*aSurface = mSurface;
	return NS_OK;
}

NS_IMETHODIMP nsRenderingContextBeOS::Reset() {
	return NS_OK;
}

NS_IMETHODIMP nsRenderingContextBeOS::GetDeviceContext(nsIDeviceContext *&aContext) {
	NS_IF_ADDREF(mContext);
	aContext = mContext;
	return NS_OK;
}

// Get a new GS
NS_IMETHODIMP nsRenderingContextBeOS::PushState() {
#ifdef USE_GS_POOL
  nsGraphicsState *state = nsGraphicsStatePool::GetNewGS();
#else
  nsGraphicsState *state = new nsGraphicsState;
#endif
  // Push into this state object, add to vector
  if (!state) return NS_ERROR_OUT_OF_MEMORY;

  nsTransform2D *tranMatrix;
  if (nsnull == mTranMatrix)
    tranMatrix = new nsTransform2D();
  else
    tranMatrix = new nsTransform2D(mTranMatrix);

  if (!tranMatrix) {
#ifdef USE_GS_POOL
    nsGraphicsStatePool::ReleaseGS(state);
#else
    delete state;
#endif
    return NS_ERROR_OUT_OF_MEMORY;
  }
  state->mMatrix = mTranMatrix;
  mTranMatrix = tranMatrix;

  // Set state to mClipRegion. SetClip{Rect,Region}() will do copy-on-write stuff
  state->mClipRegion = mClipRegion;

  NS_IF_ADDREF(mFontMetrics);
  state->mFontMetrics = mFontMetrics;
  state->mColor = mCurrentColor;
  state->mLineStyle = mCurrentLineStyle;

  mStateCache->AppendElement(state);
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextBeOS::PopState(void) {
	PRUint32 cnt = mStateCache->Count();
	nsGraphicsState *state;
	
	if (cnt > 0) {
		state = (nsGraphicsState *)mStateCache->ElementAt(cnt - 1);
		mStateCache->RemoveElementAt(cnt - 1);
		
		// Assign all local attributes from the state object just popped
		if (state->mMatrix) {
			delete mTranMatrix;
			mTranMatrix = state->mMatrix;
		}
		
		mClipRegion = state->mClipRegion;
		if (state->mFontMetrics && (mFontMetrics != state->mFontMetrics)) {
			SetFont(state->mFontMetrics);
		}
		if (state->mColor != mCurrentColor) SetColor(state->mColor);
		if (state->mLineStyle != mCurrentLineStyle) SetLineStyle(state->mLineStyle);

		// Delete this graphics state object
#ifdef USE_GS_POOL
		nsGraphicsStatePool::ReleaseGS(state);
#else
		delete state;
#endif
	}
	
	return NS_OK;
}

NS_IMETHODIMP nsRenderingContextBeOS::IsVisibleRect(const nsRect& aRect, PRBool &aVisible) {
	aVisible = PR_TRUE;
	return NS_OK;
}

NS_IMETHODIMP nsRenderingContextBeOS::GetClipRect(nsRect &aRect, PRBool &aClipValid) {
	if (!mClipRegion) return NS_ERROR_FAILURE;
	if (!mClipRegion->IsEmpty()) {
		PRInt32 x, y, w, h;
		mClipRegion->GetBoundingBox(&x, &y, &w, &h);
		aRect.SetRect(x, y, w, h);
		aClipValid = PR_TRUE;
	} else {
		aRect.SetRect(0, 0, 0, 0);
		aClipValid = PR_FALSE;
	}
	return NS_OK;
}

NS_IMETHODIMP nsRenderingContextBeOS::SetClipRect(const nsRect& aRect, nsClipCombine aCombine) {
	
	PRUint32 cnt = mStateCache->Count();
	nsGraphicsState *state = nsnull;
	if (cnt > 0) {
		state = (nsGraphicsState *)mStateCache->ElementAt(cnt - 1);
	}
	
	if (state) {
		if (state->mClipRegion) {
			if (state->mClipRegion == mClipRegion) {
				nsCOMPtr<nsIRegion> region;
				GetClipRegion(getter_AddRefs(region));
				mClipRegion = region;
			}
		}
	}
	
	CreateClipRegion();
	nsRect trect = aRect;
	mTranMatrix->TransformCoord(&trect.x, &trect.y, &trect.width, &trect.height);

	switch (aCombine) {
		case nsClipCombine_kIntersect:
			mClipRegion->Intersect(trect.x, trect.y, trect.width, trect.height);
			break;
		case nsClipCombine_kUnion:
			mClipRegion->Union(trect.x, trect.y, trect.width, trect.height);
			break;
		case nsClipCombine_kSubtract:
			mClipRegion->Subtract(trect.x, trect.y, trect.width, trect.height);
			break;
		case nsClipCombine_kReplace:
			mClipRegion->SetTo(trect.x, trect.y, trect.width, trect.height);
			break;
	}
	
	return NS_OK;
} 

// To reduce locking overhead, the caller must unlock the looper itself.
// TO DO: Locking and unlocking around each graphics primitive is still very lame
// and imposes serious overhead. The entire locking mechanism here needs to be
// revisited, especially for offscreen surfaces whose thread is not locked
// for other purposes, the way a BWindow does for message handling.
bool nsRenderingContextBeOS::LockAndUpdateView() {
	bool rv = false;
	if (mView) mView = nsnull;
	mSurface->AcquireView(&mView);
	
	if (mView && mView->LockLooper()) {
		if (mCurrentFont == nsnull) mCurrentFont = (BFont *)be_plain_font;
		
		mView->SetFont(mCurrentFont);
		mView->SetHighColor(NS_GET_R(mCurrentColor),
                        NS_GET_G(mCurrentColor),
                        NS_GET_B(mCurrentColor),
                        255);

		BRegion *region = nsnull;
		if (mClipRegion) {
			mClipRegion->GetNativeRegion((void *&)region);
			mView->ConstrainClippingRegion(region);
		}
		rv = true;
	}
	return rv;
}

NS_IMETHODIMP nsRenderingContextBeOS::SetClipRegion(const nsIRegion &aRegion,
	nsClipCombine aCombine) {
	
	PRUint32 cnt = mStateCache->Count();
	nsGraphicsState *state = nsnull;
	if (cnt > 0) {
		state = (nsGraphicsState *)mStateCache->ElementAt(cnt - 1);
	}
	
	if (state) {
		if (state->mClipRegion) {
			if (state->mClipRegion == mClipRegion) {
				nsCOMPtr<nsIRegion> region;
				GetClipRegion(getter_AddRefs(region));
				mClipRegion = region;
			}
		}
	}
	
	CreateClipRegion();
	switch (aCombine) {
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
	
	return NS_OK;
}

NS_IMETHODIMP nsRenderingContextBeOS::CopyClipRegion(nsIRegion &aRegion) {
	if (!mClipRegion) return NS_ERROR_FAILURE;
	aRegion.SetTo(*mClipRegion);
	return NS_OK;
}

NS_IMETHODIMP nsRenderingContextBeOS::GetClipRegion(nsIRegion **aRegion) {
	nsresult rv = NS_ERROR_FAILURE;
	if (!aRegion || !mClipRegion) return NS_ERROR_NULL_POINTER;
	
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
#ifdef DEBUG
		printf("null clip region, can't make a valid copy\n");
#endif
		rv = NS_ERROR_FAILURE;
	}
	return rv;
}

NS_IMETHODIMP nsRenderingContextBeOS::SetColor(nscolor aColor) {
	if (nsnull == mContext) return NS_ERROR_FAILURE;
	mCurrentColor = aColor;
	return NS_OK;
}

NS_IMETHODIMP nsRenderingContextBeOS::GetColor(nscolor &aColor) const {
	aColor = mCurrentColor;
	return NS_OK;
}

NS_IMETHODIMP nsRenderingContextBeOS::SetFont(const nsFont &aFont, nsIAtom* aLangGroup) {
	nsCOMPtr<nsIFontMetrics> newMetrics;
	nsresult rv = mContext->GetMetricsFor(aFont, aLangGroup, *getter_AddRefs(newMetrics));
	if (NS_SUCCEEDED(rv)) rv = SetFont(newMetrics);
	return rv;
}

NS_IMETHODIMP nsRenderingContextBeOS::SetFont(nsIFontMetrics *aFontMetrics) {
	NS_IF_RELEASE(mFontMetrics);
	mFontMetrics = aFontMetrics;
	NS_IF_ADDREF(mFontMetrics);
	
	if (mFontMetrics) {
		nsFontHandle fontHandle;
		mFontMetrics->GetFontHandle(fontHandle);
		mCurrentFont = (BFont *)fontHandle;
	}
	return NS_OK;
}

NS_IMETHODIMP nsRenderingContextBeOS::SetLineStyle(nsLineStyle aLineStyle) {
	mCurrentLineStyle = aLineStyle;
	return NS_OK;
}

NS_IMETHODIMP nsRenderingContextBeOS::GetLineStyle(nsLineStyle &aLineStyle) {
	aLineStyle = mCurrentLineStyle;
	return NS_OK;
}

NS_IMETHODIMP nsRenderingContextBeOS::GetFontMetrics(nsIFontMetrics *&aFontMetrics) {
	NS_IF_ADDREF(mFontMetrics);
	aFontMetrics = mFontMetrics;
	return NS_OK;
}

// Add the passed in translation to the current translation
NS_IMETHODIMP nsRenderingContextBeOS::Translate(nscoord aX, nscoord aY) {
	mTranMatrix->AddTranslation((float)aX, (float)aY);
	return NS_OK;
}

// Add the passed in scale to the current scale
NS_IMETHODIMP nsRenderingContextBeOS::Scale(float aSx, float aSy) {
	mTranMatrix->AddScale(aSx, aSy);
	return NS_OK;
}

NS_IMETHODIMP nsRenderingContextBeOS::GetCurrentTransform(nsTransform2D *&aTransform) {
	aTransform = mTranMatrix;
	return NS_OK;
}

NS_IMETHODIMP nsRenderingContextBeOS::CreateDrawingSurface(const nsRect& aBounds, PRUint32 aSurfFlags,
	nsDrawingSurface &aSurface) {
	
	if (nsnull == mSurface) {
		aSurface = nsnull;
		return NS_ERROR_FAILURE;
	}
	
	if ((aBounds.width <= 0) || (aBounds.height <= 0)) {
		return NS_ERROR_FAILURE;
	}
	
	nsDrawingSurfaceBeOS *surf = new nsDrawingSurfaceBeOS();
	if (surf) {
		NS_ADDREF(surf);
		if (!mView) 
		{
		    if(LockAndUpdateView())
		        mView->UnlockLooper();
		}
		surf->Init(mView, aBounds.width, aBounds.height, aSurfFlags);
	}
	aSurface = (nsDrawingSurface)surf;
	return NS_OK;
}

NS_IMETHODIMP nsRenderingContextBeOS::DestroyDrawingSurface(nsDrawingSurface aDS) {
	nsDrawingSurfaceBeOS *surf = (nsDrawingSurfaceBeOS *)aDS;
	if (surf == nsnull) return NS_ERROR_FAILURE;
	NS_IF_RELEASE(surf);
	return NS_OK;
}

NS_IMETHODIMP nsRenderingContextBeOS::DrawLine(nscoord aX0, nscoord aY0, nscoord aX1, nscoord aY1) {
	if (mTranMatrix == nsnull) return NS_ERROR_FAILURE;
	if (mSurface == nsnull) return NS_ERROR_FAILURE;
	
	mTranMatrix->TransformCoord(&aX0, &aY0);
	mTranMatrix->TransformCoord(&aX1, &aY1);
	nscoord diffX = aX1 - aX0;
	nscoord diffY = aY1 - aY0;
	
	if (0 != diffX) {
		diffX = (diffX > 0) ? 1 : -1;
	}
	if (0 != diffY) {
		diffY = (diffY > 0) ? 1 : -1;
	}
	
	if (LockAndUpdateView()) {
		mView->StrokeLine(BPoint(aX0, aY0), BPoint(aX1 - diffX, aY1 - diffY));
		mView->UnlockLooper();
	}
	return NS_OK;
}

NS_IMETHODIMP nsRenderingContextBeOS::DrawPolyline(const nsPoint aPoints[], PRInt32 aNumPoints) {
	if (mTranMatrix == nsnull) return NS_ERROR_FAILURE;
	if (mSurface == nsnull) return NS_ERROR_FAILURE;
	BPoint *pts;
	BPolygon poly;
	BRect r;
	PRInt32 w, h;
	//allocating from stack if amount isn't big
	BPoint bpointbuf[64];
	pts = bpointbuf;
	if(aNumPoints>64)
		pts = new BPoint[aNumPoints];
	for (int i = 0; i < aNumPoints; ++i) {
		nsPoint p = aPoints[i];
		mTranMatrix->TransformCoord(&p.x, &p.y);
		pts[i].x = p.x;
		pts[i].y = p.y;
#ifdef DEBUG
		printf("polyline(%i,%i)\n", p.x, p.y);
#endif
	}
	poly.AddPoints(pts, aNumPoints);
	r = poly.Frame();	
	w = r.IntegerWidth();
	h = r.IntegerHeight();
//	Don't draw empty polygon
	if(w && h)
	{
		if (LockAndUpdateView()) {
			if (1 == h) {
				mView->StrokeLine(BPoint(r.left, r.top), BPoint(r.left + w - 1, r.top));
			} else if (1 == w) {
				mView->StrokeLine(BPoint(r.left, r.top), BPoint(r.left, r.top + h -1));
			} else {
				poly.MapTo(r,BRect(r.left, r.top, r.left + w -1, r.top + h - 1));
				mView->StrokePolygon(&poly, false);
			}
			mView->UnlockLooper();
		}		
	}	
	if(pts!=bpointbuf)
		delete [] pts;
	return NS_OK;
}

NS_IMETHODIMP nsRenderingContextBeOS::DrawRect(const nsRect& aRect) {
	return DrawRect(aRect.x, aRect.y, aRect.width, aRect.height);
}

NS_IMETHODIMP nsRenderingContextBeOS::DrawRect(nscoord aX, nscoord aY, nscoord aWidth,
	nscoord aHeight) {
	
	if (nsnull == mTranMatrix || nsnull == mSurface) return NS_ERROR_FAILURE;

	// After the transform, if the numbers are huge, chop them, because
	// they're going to be converted from 32 bit to 16 bit.
	// It's all way off the screen anyway.
	nscoord x = aX, y = aY, w = aWidth, h = aHeight;
	mTranMatrix->TransformCoord(&x, &y, &w, &h);
	ConditionRect(x, y, w, h);
	
	// Don't draw empty rectangles; also, w/h are adjusted down by one
	// so that the right number of pixels are drawn.
	if (w && h) {
		if (LockAndUpdateView()) {
			// FIXME: add line style
			if (1 == h) {
				mView->StrokeLine(BPoint(x, y), BPoint(x + w - 1, y));
			} else {
				mView->StrokeRect(BRect(x, y, x + w - 1, y + h - 1));
			}
			mView->UnlockLooper();
		}
	}
	return NS_OK;
}

NS_IMETHODIMP nsRenderingContextBeOS::FillRect(const nsRect &aRect) {
	return FillRect(aRect.x, aRect.y, aRect.width, aRect.height);
}

NS_IMETHODIMP nsRenderingContextBeOS::FillRect(nscoord aX, nscoord aY, nscoord aWidth,
	nscoord aHeight) {
	
	if (nsnull == mTranMatrix || nsnull == mSurface) return NS_ERROR_FAILURE;
	
	// After the transform, if the numbers are huge, chop them, because
	// they're going to be converted from 32 bit to 16 bit.
	// It's all way off the screen anyway.
	nscoord x = aX, y = aY, w = aWidth, h = aHeight;
	mTranMatrix->TransformCoord(&x, &y, &w, &h);
	ConditionRect(x, y, w, h);
	
	if (w && h) {
		if (LockAndUpdateView()) {
			// FIXME: add line style
			if (1 == h) {
				mView->StrokeLine(BPoint(x, y), BPoint(x + w - 1, y));
			} else {
				mView->FillRect(BRect(x, y, x + w - 1, y + h - 1), B_SOLID_HIGH);
			}
			mView->UnlockLooper();
		}
	}
	return NS_OK;
}

NS_IMETHODIMP nsRenderingContextBeOS::InvertRect(const nsRect &aRect) {
	return InvertRect(aRect.x, aRect.y, aRect.width, aRect.height);
}

NS_IMETHODIMP nsRenderingContextBeOS::InvertRect(nscoord aX, nscoord aY, nscoord aWidth,
	nscoord aHeight) {
	
	if (nsnull == mTranMatrix || nsnull == mSurface) return NS_ERROR_FAILURE;
	
	// After the transform, if the numbers are huge, chop them, because
	// they're going to be converted from 32 bit to 16 bit.
	// It's all way off the screen anyway.
	nscoord x = aX, y = aY, w = aWidth, h = aHeight;
	mTranMatrix->TransformCoord(&x, &y, &w, &h);
	ConditionRect(x, y, w, h);
	
	if (w && h) {
		if (LockAndUpdateView()) {
			//Mozilla doesn't seem to set clipping for InvertRect, so we do it here - bug 230267
			BRegion tmpClip = BRegion(BRect(x, y, x + w - 1, y + h - 1));
			mView->ConstrainClippingRegion(&tmpClip);
			mView->InvertRect(BRect(x, y, x + w - 1, y + h - 1));
			mView->Sync();
			mView->UnlockLooper();
		}
	}
	return NS_OK;
}

NS_IMETHODIMP nsRenderingContextBeOS::DrawPolygon(const nsPoint aPoints[], PRInt32 aNumPoints) {
	if (nsnull == mTranMatrix || nsnull == mSurface) return NS_ERROR_FAILURE;
	BPoint *pts;
	BPolygon poly;
	BRect r;
	PRInt32 w, h;
	//allocating from stack if amount isn't big
	BPoint bpointbuf[64];
	pts = bpointbuf;
	if(aNumPoints>64)
		pts = new BPoint[aNumPoints];
	for (int i = 0; i < aNumPoints; ++i) {
		nsPoint p = aPoints[i];
		mTranMatrix->TransformCoord(&p.x, &p.y);
		pts[i].x = p.x;
		pts[i].y = p.y;
	}
	poly.AddPoints(pts, aNumPoints);
	r = poly.Frame();	
	w = r.IntegerWidth();
	h = r.IntegerHeight();
//	Don't draw empty polygon
	if(w && h)
	{
		if (LockAndUpdateView()) {
			if (1 == h) {
				mView->StrokeLine(BPoint(r.left, r.top), BPoint(r.left + w - 1, r.top));
			} else if (1 == w) {
				mView->StrokeLine(BPoint(r.left, r.top), BPoint(r.left, r.top + h -1));
			} else {
				poly.MapTo(r,BRect(r.left, r.top, r.left + w -1, r.top + h - 1));
				mView->StrokePolygon(&poly, true, B_SOLID_HIGH);
			}
			mView->UnlockLooper();
		}		
	}		
	if(pts!=bpointbuf)
		delete [] pts;
	return NS_OK;
}

NS_IMETHODIMP nsRenderingContextBeOS::FillPolygon(const nsPoint aPoints[], PRInt32 aNumPoints) {
	if (nsnull == mTranMatrix || nsnull == mSurface) return NS_ERROR_FAILURE;
	
	BPoint *pts;
	BPolygon poly;
	BRect r;
	PRInt32 w, h;
	BPoint bpointbuf[64];
	pts = bpointbuf;
	if(aNumPoints>64)
		pts = new BPoint[aNumPoints];
	for (int i = 0; i < aNumPoints; ++i) {
		nsPoint p = aPoints[i];
		mTranMatrix->TransformCoord(&p.x, &p.y);
		pts[i].x = p.x;
		pts[i].y = p.y;
	}
	poly.AddPoints(pts, aNumPoints);
	r = poly.Frame();
	w = r.IntegerWidth();
	h = r.IntegerHeight();
//	Don't draw empty polygon
	if(w && h)
	{
		if (LockAndUpdateView()) {
			if (1 == h) {
				mView->StrokeLine(BPoint(r.left, r.top), BPoint(r.left + w - 1, r.top));
			} else if (1 == w) {
				mView->StrokeLine(BPoint(r.left, r.top), BPoint(r.left, r.top + h -1));
			} else {
				poly.MapTo(r,BRect(r.left, r.top, r.left + w -1, r.top + h - 1));
				mView->FillPolygon(&poly, B_SOLID_HIGH);
			}
			mView->UnlockLooper();
		}		
	}
	if(pts!=bpointbuf)
		delete [] pts;
	return NS_OK;
}

NS_IMETHODIMP nsRenderingContextBeOS::DrawEllipse(const nsRect &aRect) {
	return DrawEllipse(aRect.x, aRect.y, aRect.width, aRect.height);
}

NS_IMETHODIMP nsRenderingContextBeOS::DrawEllipse(nscoord aX, nscoord aY, nscoord aWidth,
	nscoord aHeight) {
	
	if (nsnull == mTranMatrix || nsnull == mSurface) return NS_ERROR_FAILURE;

	nscoord x = aX, y = aY, w = aWidth, h = aHeight;
	mTranMatrix->TransformCoord(&x, &y, &w, &h);
	
	if (LockAndUpdateView()) {
		mView->StrokeEllipse(BRect(x, y, x + w - 1, y + h - 1));
		mView->UnlockLooper();
	}
	return NS_OK;
}

NS_IMETHODIMP nsRenderingContextBeOS::FillEllipse(const nsRect &aRect) {
	return FillEllipse(aRect.x, aRect.y, aRect.width, aRect.height);
}

NS_IMETHODIMP nsRenderingContextBeOS::FillEllipse(nscoord aX, nscoord aY, nscoord aWidth,
	nscoord aHeight) {
	
	if (nsnull == mTranMatrix || nsnull == mSurface) return NS_ERROR_FAILURE;

	nscoord x = aX, y = aY, w = aWidth, h = aHeight;
	mTranMatrix->TransformCoord(&x, &y, &w, &h);
	
	if (LockAndUpdateView()) {
		mView->FillEllipse(BRect(x, y, x + w - 1, y + h - 1));
		mView->UnlockLooper();
	}
	return NS_OK;
}

NS_IMETHODIMP nsRenderingContextBeOS::DrawArc(const nsRect& aRect, float aStartAngle,
	float aEndAngle) {
	
	return DrawArc(aRect.x, aRect.y, aRect.width, aRect.height, aStartAngle, aEndAngle);
}

NS_IMETHODIMP nsRenderingContextBeOS::DrawArc(nscoord aX, nscoord aY, nscoord aWidth,
	nscoord aHeight, float aStartAngle, float aEndAngle) {
	
	if (nsnull == mTranMatrix || nsnull == mSurface) return NS_ERROR_FAILURE;

	nscoord x = aX, y = aY, w = aWidth, h = aHeight;
	mTranMatrix->TransformCoord(&x, &y, &w, &h);
	
	if (LockAndUpdateView()) {
		// FIXME: add line style
		mView->StrokeArc(BRect(x, y, x + w - 1, y + h - 1), aStartAngle, aEndAngle - aStartAngle);
		mView->UnlockLooper();
	}
	return NS_OK;
}

NS_IMETHODIMP nsRenderingContextBeOS::FillArc(const nsRect &aRect, float aStartAngle,
	float aEndAngle) {
	
	return FillArc(aRect.x, aRect.y, aRect.width, aRect.height, aStartAngle, aEndAngle);
}

NS_IMETHODIMP nsRenderingContextBeOS::FillArc(nscoord aX, nscoord aY, nscoord aWidth,
	nscoord aHeight, float aStartAngle, float aEndAngle) {
	
	if (nsnull == mTranMatrix || nsnull == mSurface) return NS_ERROR_FAILURE;

	nscoord x = aX, y = aY, w = aWidth, h = aHeight;
	mTranMatrix->TransformCoord(&x, &y, &w, &h);
	
	if (LockAndUpdateView()) {
		mView->FillArc(BRect(x, y, x + w - 1, y + h - 1), aStartAngle, aEndAngle - aStartAngle);
		mView->UnlockLooper();
	}
	return NS_OK;
}

// Block of UTF-8 helpers
// Whole block of utf-8 helpers must  be placed before any gfx text method
// in order to allow text handling directly from PRUnichar* methods in future

// From BeNewsLetter #82:
// Get the number of character bytes for a given utf8 byte character
inline uint32 utf8_char_len(uchar byte) 
{
	return (((0xE5000000 >> ((byte >> 3) & 0x1E)) & 3) + 1);
}

// useful UTF-8 utilities 

#define BEGINS_CHAR(byte) ((byte & 0xc0) != 0x80)

inline uint32 utf8_str_len(const char* ustring) 
{
	uint32 cnt = 0;
	while ( *ustring != '\0')
	{
		if ( BEGINS_CHAR( *ustring ) )
			++cnt;
			++ustring;
	}
	return cnt;       
}

// Macro to convert a ushort* uni_string into a char* or uchar* utf8_string,
// one character at a time. Move the pointer. You can check terminaison on
// the uni_string by testing : if (uni_string[0] == 0)
// WARNING : you need to use EXPLICIT POINTERS for both str and unistr.
#define convert_to_utf8(str, uni_str) {\
	if ((uni_str[0]&0xff80) == 0)\
		*str++ = *uni_str++;\
	else if ((uni_str[0]&0xf800) == 0) {\
		str[0] = 0xc0|(uni_str[0]>>6);\
		str[1] = 0x80|((*uni_str++)&0x3f);\
		str += 2;\
	} else if ((uni_str[0]&0xfc00) != 0xd800) {\
		str[0] = 0xe0|(uni_str[0]>>12);\
		str[1] = 0x80|((uni_str[0]>>6)&0x3f);\
		str[2] = 0x80|((*uni_str++)&0x3f);\
		str += 3;\
	} else {\
		int val;\
		val = ((uni_str[0]-0xd7c0)<<10) | (uni_str[1]&0x3ff);\
		str[0] = 0xf0 | (val>>18);\
		str[1] = 0x80 | ((val>>12)&0x3f);\
		str[2] = 0x80 | ((val>>6)&0x3f);\
		str[3] = 0x80 | (val&0x3f);\
		uni_str += 2; str += 4;\
	}\
}

// End of block of UTF-8 helpers

NS_IMETHODIMP nsRenderingContextBeOS::GetWidth(char aC, nscoord &aWidth) {
	// Check for the very common case of trying to get the width of a single space
	if ((aC == ' ') && (nsnull != mFontMetrics)) {
		return mFontMetrics->GetSpaceWidth(aWidth);
	} else {
		return GetWidth(&aC, 1, aWidth);
	}
}

NS_IMETHODIMP nsRenderingContextBeOS::GetWidth(PRUnichar aC, nscoord &aWidth, PRInt32 *aFontID) {
	return GetWidth(&aC, 1, aWidth, aFontID);
}

NS_IMETHODIMP nsRenderingContextBeOS::GetWidth(const nsString &aString, nscoord& aWidth,
	PRInt32 *aFontID) {
	
	return GetWidth(aString.get(), aString.Length(), aWidth, aFontID);
}

NS_IMETHODIMP nsRenderingContextBeOS::GetWidth(const char *aString, nscoord &aWidth) {
	return GetWidth(aString, strlen(aString), aWidth);
}

NS_IMETHODIMP nsRenderingContextBeOS::GetWidth(const char *aString, PRUint32 aLength,
	nscoord &aWidth) {
	
	if (0 == aLength) {
		aWidth = 0;
	} else {
		if (aString == nsnull) return NS_ERROR_FAILURE;
		PRUint32 rawWidth = (PRUint32)mCurrentFont->StringWidth(aString, aLength);
		aWidth = NSToCoordRound(rawWidth * mP2T);
	}
	return NS_OK;
}


NS_IMETHODIMP nsRenderingContextBeOS::GetWidth(const PRUnichar *aString, PRUint32 aLength,
	nscoord &aWidth, PRInt32 *aFontID) {
	
	// max UTF-8 string length
	uint8 *utf8str = new uint8[aLength * 4 + 1];
	uint8 *utf8ptr = utf8str;
	const PRUnichar *uniptr = aString;
	
	for (PRUint32 i = 0; i < aLength; i++) {
		convert_to_utf8(utf8ptr, uniptr);
	}
	
	*utf8ptr = '\0';
	uint32 utf8str_len = strlen((char *)utf8str);
	GetWidth((char *)utf8str, utf8str_len, aWidth);
	delete [] utf8str;
	return NS_OK;
}

NS_IMETHODIMP nsRenderingContextBeOS::GetTextDimensions(const char *aString,
	PRUint32 aLength, nsTextDimensions &aDimensions) {
	
	if (mFontMetrics) {
		mFontMetrics->GetMaxAscent(aDimensions.ascent);
		mFontMetrics->GetMaxDescent(aDimensions.descent);
	}	
	return GetWidth(aString, aLength, aDimensions.width);
}

NS_IMETHODIMP nsRenderingContextBeOS::GetTextDimensions(const PRUnichar *aString,
	PRUint32 aLength, nsTextDimensions &aDimensions, PRInt32 *aFontID) {
	
	if (mFontMetrics) {
		mFontMetrics->GetMaxAscent(aDimensions.ascent);
		mFontMetrics->GetMaxDescent(aDimensions.descent);
	}	
	return GetWidth(aString, aLength, aDimensions.width, aFontID);
}

// FAST TEXT MEASURE methods
// Implementation is simplicistic in comparison with other platforms - we follow in this method
// generic BeOS-port approach for string methods in GFX - convert from PRUnichar* to (UTF-8) char*
// and call (UTF-8) char* version of the method.
// It works well with current nsFontMetricsBeOS which is, again, simpler in comparison with
// other platforms, partly due to UTF-8 nature of BeOS, partly due unimplemented font fallbacks
// and other tricks.

NS_IMETHODIMP nsRenderingContextBeOS::GetTextDimensions(const PRUnichar*  aString,
														PRInt32           aLength,
														PRInt32           aAvailWidth,
														PRInt32*          aBreaks,
														PRInt32           aNumBreaks,
														nsTextDimensions& aDimensions,
														PRInt32&          aNumCharsFit,
														nsTextDimensions& aLastWordDimensions,
														PRInt32*          aFontID = nsnull)
{
	nsresult ret_code = NS_ERROR_FAILURE;	
	uint8 utf8buf[1024];
	uint8* utf8str = nsnull;
	// max UTF-8 string length
	PRUint32 slength = aLength * 4 + 1;
	// Allocating char* array rather from stack than from heap for speed.
	//  1024 char array forms  e.g.  256 == 32*8 frame for CJK glyphs, which may be
	// insufficient for purpose of this method, but until we implement storage
	//in nsSurface, i think it is good compromise.
	if (slength < 1024) 
		utf8str = utf8buf;
	else 
		utf8str = new uint8[slength];

	uint8 *utf8ptr = utf8str;
	const PRUnichar *uniptr = aString;
	
	for (PRUint32 i = 0; i < aLength; ++i) 
		convert_to_utf8(utf8ptr, uniptr);
	
	*utf8ptr = '\0';
	ret_code = GetTextDimensions((char *)utf8str, utf8ptr-utf8str, aAvailWidth, aBreaks, aNumBreaks,
                               aDimensions, aNumCharsFit, aLastWordDimensions, aFontID);
	// deallocating if got from heap
	if (utf8str != utf8buf)
			delete [] utf8str;
	return ret_code;
}

NS_IMETHODIMP nsRenderingContextBeOS::GetTextDimensions(const char*       aString,
														PRInt32           aLength,
														PRInt32           aAvailWidth,
														PRInt32*          aBreaks,
														PRInt32           aNumBreaks,
														nsTextDimensions& aDimensions,
														PRInt32&          aNumCharsFit,
														nsTextDimensions& aLastWordDimensions,
														PRInt32*          aFontID = nsnull)
{
	// Code is borrowed from win32 implementation including comments.
	// Minor changes are introduced due multibyte/utf-8 nature of char* strings handling in BeOS.

	NS_PRECONDITION(aBreaks[aNumBreaks - 1] == aLength, "invalid break array");
	// If we need to back up this state represents the last place we could
	// break. We can use this to avoid remeasuring text
	PRInt32 prevBreakState_BreakIndex = -1; // not known (hasn't been computed)
	nscoord prevBreakState_Width = 0; // accumulated width to this point
	mFontMetrics->GetMaxAscent(aLastWordDimensions.ascent);
	mFontMetrics->GetMaxDescent(aLastWordDimensions.descent);
	aLastWordDimensions.width = -1;
	aNumCharsFit = 0;
	// Iterate each character in the string and determine which font to use
	nscoord width = 0;
	PRInt32 start = 0;
	nscoord aveCharWidth;
	PRInt32 numBytes = 0;
	PRInt32 num_of_glyphs = 0; // Number of glyphs isn't equal to number of bytes in case of UTF-8
	PRInt32 *utf8pos =0;
	// allocating  array for positions of first bytes of utf-8 chars in utf-8 string
	// from stack if possible
	PRInt32 utf8posbuf[1025];
	if (aLength < 1025) 
		utf8pos = utf8posbuf;
	else 
		utf8pos = new PRInt32[aLength + 1];
	
	char * utf8ptr = (char *)aString;
	// counting number of glyphs (not bytes) in utf-8 string 
	//and recording positions of first byte for each utf-8 char
	PRInt32 i = 0;
	while (i < aLength)
	{
		if ( BEGINS_CHAR( utf8ptr[i] ) )
		{
			utf8pos[num_of_glyphs] = i;
			++num_of_glyphs;
		}
		++i;
	}

	mFontMetrics->GetAveCharWidth(aveCharWidth);
	utf8pos[num_of_glyphs] = i; // IMPORTANT for non-ascii strings for proper last-string-in-block.

	while (start < num_of_glyphs) 
	{
		// Estimate how many characters will fit. Do that by diving the available
		// space by the average character width. Make sure the estimated number
		// of characters is at least 1
		PRInt32 estimatedNumChars = 0;
		if (aveCharWidth > 0) 
			estimatedNumChars = (aAvailWidth - width) / aveCharWidth;

		if (estimatedNumChars < 1) 
			estimatedNumChars = 1;

		// Find the nearest break offset
		PRInt32 estimatedBreakOffset = start + estimatedNumChars;
		PRInt32 breakIndex;
		nscoord numChars;
		if (num_of_glyphs <= estimatedBreakOffset) 
		{
			// All the characters should fit
			numChars = num_of_glyphs - start;
			// BeOS specifics - getting number of bytes from position array. Same for all remaining numBytes occurencies.
			numBytes = aLength - utf8pos[start];
			breakIndex = aNumBreaks - 1;
		}
		else 
		{
			breakIndex = prevBreakState_BreakIndex;
			while (((breakIndex + 1) < aNumBreaks) 
					&& (aBreaks[breakIndex + 1] <= estimatedBreakOffset)) 
			{
				++breakIndex;
			}
			
			if (breakIndex == prevBreakState_BreakIndex) 
				++breakIndex; // make sure we advanced past the previous break index

			numChars = aBreaks[breakIndex] - start;
			numBytes = utf8pos[aBreaks[breakIndex]] - utf8pos[start];
		}
		nscoord twWidth = 0;
		if ((1 == numChars) && (aString[utf8pos[start]] == ' ')) 
		{
			mFontMetrics->GetSpaceWidth(twWidth);
		} 
		else if (numChars > 0) 
		{
			float  size = mCurrentFont->StringWidth(&aString[utf8pos[start]], numBytes);
			twWidth = NSToCoordRound(size * mP2T);
		} 

		// See if the text fits
		PRBool  textFits = (twWidth + width) <= aAvailWidth;
		// If the text fits then update the width and the number of
		// characters that fit
		if (textFits) 
		{
			aNumCharsFit += numChars;
			width += twWidth;
			start += numChars;

			// This is a good spot to back up to if we need to so remember
			// this state
			prevBreakState_BreakIndex = breakIndex;
			prevBreakState_Width = width;
		}
		else 
		{
			// See if we can just back up to the previous saved state and not
			// have to measure any text
			if (prevBreakState_BreakIndex > 0) 
			{
				// If the previous break index is just before the current break index
				// then we can use it
				if (prevBreakState_BreakIndex == (breakIndex - 1)) 
				{
					aNumCharsFit = aBreaks[prevBreakState_BreakIndex];
					width = prevBreakState_Width;
					break;
				}
			}
			// We can't just revert to the previous break state
			if (0 == breakIndex)
			{
				// There's no place to back up to, so even though the text doesn't fit
				// return it anyway
				aNumCharsFit += numChars;
				width += twWidth;
				break;
			}       
			// Repeatedly back up until we get to where the text fits or we're all
			// the way back to the first word
			width += twWidth;
			while ((breakIndex >= 1) && (width > aAvailWidth)) 
			{
				twWidth = 0;
				start = aBreaks[breakIndex - 1];
				numChars = aBreaks[breakIndex] - start;
				numBytes = utf8pos[aBreaks[breakIndex]] - utf8pos[start];
				if ((1 == numChars) && (aString[utf8pos[start]] == ' ')) 
				{
					mFontMetrics->GetSpaceWidth(twWidth);
				}
				else if (numChars > 0) 
				{
					float size = mCurrentFont->StringWidth(&aString[utf8pos[start]], numBytes);
					twWidth = NSToCoordRound(size * mP2T);
				}
				width -= twWidth;
				aNumCharsFit = start;
				--breakIndex;
			}
		break;   
		}	       
	}
	aDimensions.width = width;
	mFontMetrics->GetMaxAscent(aDimensions.ascent);
	mFontMetrics->GetMaxDescent(aDimensions.descent);
	// deallocating if got from heap
	if(utf8pos != utf8posbuf) 
		delete utf8pos;
	return NS_OK;
}

NS_IMETHODIMP nsRenderingContextBeOS::DrawString(const nsString &aString,
	nscoord aX, nscoord aY, PRInt32 aFontID, const nscoord *aSpacing) {
	
	return DrawString(aString.get(), aString.Length(), aX, aY, aFontID, aSpacing);
}


// TO DO: A better solution is needed for both antialiasing as noted below and
// character spacing - these are both suboptimal.
NS_IMETHODIMP nsRenderingContextBeOS::DrawString(const char *aString, PRUint32 aLength,
	nscoord aX, nscoord aY, const nscoord *aSpacing) {
	
	if (0 != aLength) {
		if (mTranMatrix == nsnull) return NS_ERROR_FAILURE;
		if (mSurface == nsnull) return NS_ERROR_FAILURE;
		if (aString == nsnull) return NS_ERROR_FAILURE;
		nscoord xx = aX, yy = aY, y=aY;
		PRBool doEmulateBold = PR_FALSE;
		
		// Subtract xFontStruct ascent since drawing specifies baseline
		if (mFontMetrics) {
			nsFontMetricsBeOS *metrics = NS_STATIC_CAST(nsFontMetricsBeOS *, mFontMetrics);
			doEmulateBold = metrics->GetEmulateBold();
		}
		
		if (LockAndUpdateView())  
		{
			// XXX: the following maybe isn't  most efficient for text rendering,
			// but it's the easy way to render antialiased text correctly
			mView->SetDrawingMode(B_OP_OVER);
			if (nsnull == aSpacing || utf8_char_len((uchar)aString[0])==aLength) {
				mTranMatrix->TransformCoord(&xx, &yy);
				mView->DrawString(aString, aLength, BPoint(xx, yy));
				if (doEmulateBold) {
					mView->DrawString(aString, aLength, BPoint(xx+1.0, yy));
				}
			} else {
					char *wpoint =0;
					int32 unichnum=0,  position=aX, ch_len=0;
					for (PRUint32 i =0; i <= aLength; i += ch_len)
					{
						ch_len = utf8_char_len((uchar)aString[i]);
						wpoint = (char *)(aString + i);
						xx = position; 
						yy = y;
						mTranMatrix->TransformCoord(&xx, &yy);
						// yy++; DrawString quirk!
						mView->DrawString((char *)(wpoint), ch_len, BPoint(xx, yy)); 
						if (doEmulateBold) 	
							mView->DrawString((char *)(wpoint), ch_len, BPoint(xx+1.0, yy));
						position += aSpacing[unichnum++];
					}
			}
			mView->SetDrawingMode(B_OP_COPY);
				mView->UnlockLooper();
		}
	}
	return NS_OK;
}

NS_IMETHODIMP nsRenderingContextBeOS::DrawString(const PRUnichar *aString, PRUint32 aLength,
	nscoord aX, nscoord aY, PRInt32 aFontID, const nscoord *aSpacing) {
	
	// max UTF-8 string length
	uint8 *utf8str = new uint8[aLength * 4 + 1];
	uint8 *utf8ptr = utf8str;
	const PRUnichar *uniptr = aString;
	
	for (PRUint32 i = 0; i < aLength; i++) {
		convert_to_utf8(utf8ptr, uniptr);
	}
	
	*utf8ptr = '\0';
	uint32 utf8str_len = strlen((char *)utf8str);
	DrawString((char *)utf8str, utf8str_len, aX, aY, aSpacing);
	delete [] utf8str;
	return NS_OK;
}

NS_IMETHODIMP nsRenderingContextBeOS::CopyOffScreenBits(nsDrawingSurface aSrcSurf,
	PRInt32 aSrcX, PRInt32 aSrcY, const nsRect &aDestBounds, PRUint32 aCopyFlags) {
	
	PRInt32 srcX = aSrcX;
	PRInt32 srcY = aSrcY;
	nsRect drect = aDestBounds;
	
	if (aSrcSurf == nsnull) return NS_ERROR_FAILURE;
	if (mTranMatrix == nsnull) return NS_ERROR_FAILURE;
	if (mSurface == nsnull) return NS_ERROR_FAILURE;
	
	bool mustunlock = LockAndUpdateView();	
	BBitmap *srcbitmap;
	((nsDrawingSurfaceBeOS *)aSrcSurf)->AcquireBitmap(&srcbitmap);
	BView *srcview;
	((nsDrawingSurfaceBeOS *)aSrcSurf)->AcquireView(&srcview);
	
	if (srcbitmap) {
		if (srcview && srcview->LockLooper()) {
			// Since all the drawing in this class is asynchronous, we must synchronize with
			// the app_server before drawing its contents anywhere else
			srcview->Sync();
			
			nsDrawingSurfaceBeOS *destsurf;
			if (aCopyFlags & NS_COPYBITS_TO_BACK_BUFFER) {
				NS_ASSERTION(nsnull != mSurface, "no back buffer");
				destsurf = mSurface;
			} else {
				destsurf = mOffscreenSurface;
			}
			
			BView *destview;
			destsurf->AcquireView(&destview);
			if (destview) {
				if (destview != mView) destview->LockLooper();
				if (aCopyFlags & NS_COPYBITS_XFORM_SOURCE_VALUES) {
					mTranMatrix->TransformCoord(&srcX, &srcY);
				}
				if (aCopyFlags & NS_COPYBITS_XFORM_DEST_VALUES) {
					mTranMatrix->TransformCoord(&drect.x, &drect.y, &drect.width, &drect.height);
				}
				if (aCopyFlags & NS_COPYBITS_USE_SOURCE_CLIP_REGION) {
					BRegion r;
					srcview->GetClippingRegion(&r);
					destview->ConstrainClippingRegion(&r);
				}
				
				// Draw to destination synchronously to make sure srcbitmap doesn't change
				// before the blit is finished.
				destview->DrawBitmap(srcbitmap, BRect(srcX, srcY, srcX + drect.width - 1, srcY + drect.height - 1),
					BRect(drect.x, drect.y, drect.x + drect.width - 1, drect.y + drect.height - 1));
				destview->UnlockLooper();
				if (destview != mView && mustunlock) mView->UnlockLooper();
				destsurf->ReleaseView();
			}
			srcview->UnlockLooper();
		}
	} else {
#ifdef DEBUG
		printf("nsRenderingContextBeOS::CopyOffScreenBits - FIXME: should render from surface without bitmap!?!?!\n");
#endif
	}
	
	// Release the source bitmap
	((nsDrawingSurfaceBeOS *)aSrcSurf)->ReleaseBitmap();	
	((nsDrawingSurfaceBeOS *)aSrcSurf)->ReleaseView();
	return NS_OK;
}

NS_IMETHODIMP nsRenderingContextBeOS::RetrieveCurrentNativeGraphicData(PRUint32 *ngd) {
	return NS_OK;
}



#ifdef MOZ_MATHML
  /**
   * Returns metrics (in app units) of an 8-bit character string
   */
NS_IMETHODIMP 
nsRenderingContextBeOS::GetBoundingMetrics(const char*        aString,
                                         PRUint32           aLength,
                                         nsBoundingMetrics& aBoundingMetrics) {
	aBoundingMetrics.Clear();
	if (0 >= aLength || !aString || !mCurrentFont)
		return NS_ERROR_FAILURE;

	BRect rect;
	escapement_delta delta;
	delta.nonspace = 0;
	delta.space = 0;
	// Use the printing metric to get more detail
	mCurrentFont->GetBoundingBoxesForStrings(&aString, 1, B_PRINTING_METRIC, &delta, &rect);


	aBoundingMetrics.width = NSToCoordRound(mCurrentFont->StringWidth(aString) * mP2T);
	
	aBoundingMetrics.leftBearing = NSToCoordRound(rect.left * mP2T);
	aBoundingMetrics.rightBearing = NSToCoordRound(rect.right * mP2T);

	// The pen position for DrawString is at the baseline of the font for BeOS.  
	// The orientation for drawing moves downward for vertical metrics.
	// MathML expects what X Windows does: the orientation of the ascent moves upward from the baseline.
	// So, we need to negate the top value returned by GetBoundingBoxesForStrings, to have that value
	// move up in BeOS.
	aBoundingMetrics.ascent = NSToCoordRound((-rect.top) * mP2T);
	aBoundingMetrics.descent = NSToCoordRound(rect.bottom * mP2T);

	return NS_OK;
}

  /**
   * Returns metrics (in app units) of a Unicode character string
   */
NS_IMETHODIMP 
nsRenderingContextBeOS::GetBoundingMetrics(const PRUnichar*   aString,
                                         PRUint32           aLength,
                                         nsBoundingMetrics& aBoundingMetrics,
                                         PRInt32*           aFontID) {
  aBoundingMetrics.Clear();
  nsresult r = NS_OK;
  if (0 < aLength) {
  	if (aString == NULL)
  		return NS_ERROR_FAILURE;

		// Since more often than not, single char strings are passed to this function
		// we will try keep this on the stack, instead of the heap
		uint8 utf8buf[1024];
		uint8* utf8str = (uint8*)&utf8buf;
		if (aLength * 4 + 1 > 1024)
			utf8str = new uint8[aLength * 4 + 1];
		uint8 *utf8ptr = utf8str;
		const PRUnichar *uniptr = aString;
	
		for (PRUint32 i = 0; i < aLength; i++) {
			convert_to_utf8(utf8ptr, uniptr);
		}
	
		*utf8ptr = '\0';
		uint32 utf8str_len = strlen((char *)utf8str);
		r = GetBoundingMetrics((char *)utf8str, utf8str_len, aBoundingMetrics);
		if (utf8str != utf8buf)
			delete [] utf8str;
		if (nsnull != aFontID)
			*aFontID = 0;
	}		
	return r;
}
#endif /* MOZ_MATHML */
