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

#include "nsRenderingContextBeOS.h"
#include "nsRegionBeOS.h"
#include "nsGfxCIID.h"
#include <math.h>

#define NS_TO_GDK_RGB(ns) (ns & 0xff) << 16 | (ns & 0xff00) | ((ns >> 16) & 0xff)

static NS_DEFINE_IID(kRenderingContextIID, NS_IRENDERING_CONTEXT_IID);

class GraphicsState
{
public:
	GraphicsState();
	~GraphicsState();

	nsTransform2D	*mMatrix;
	nsRect			mLocalClip;
	nsRegionBeOS	*mClipRegion;
	nscolor			mColor;
	nsLineStyle		mLineStyle;
	nsIFontMetrics	*mFontMetrics;
};

GraphicsState::GraphicsState()
{
	mMatrix = nsnull;
	mLocalClip.x = mLocalClip.y = mLocalClip.width = mLocalClip.height = 0;
	mClipRegion = nsnull;
	mColor = NS_RGB(0, 0, 0);
	mLineStyle = nsLineStyle_kSolid;
	mFontMetrics = nsnull;
}

GraphicsState::~GraphicsState()
{
	delete mClipRegion;
}

nsRenderingContextBeOS::nsRenderingContextBeOS()
{
	NS_INIT_REFCNT();

	mColor.red = 0; mColor.green = 0; mColor.blue = 0; mColor.alpha = 255;
	mFontMetrics = nsnull;
	mContext = nsnull;
	mSurface = nsnull;
	mMainSurface = nsnull;
	mView = nsnull;
	mMainView = nsnull;
	mCurrentColor = 0;
	mCurrentLineStyle = nsLineStyle_kSolid;
	mCurrentFont = nsnull;
	mTMatrix = nsnull;
	mP2T = 1.0f;
	mStateCache = new nsVoidArray();
	mRegion = new nsRegionBeOS();
	mRegion->Init();

	PushState();
}

nsRenderingContextBeOS::~nsRenderingContextBeOS()
{
	NS_IF_RELEASE(mContext);
	NS_IF_RELEASE(mFontMetrics);
	
	// Destroy the State Machine
	if(nsnull != mStateCache)
	{
		PRInt32 cnt = mStateCache->Count();

		while(--cnt >= 0)
		{
			GraphicsState *state = (GraphicsState *)mStateCache->ElementAt(cnt);
			mStateCache->RemoveElementAt(cnt);

			if(nsnull != state)
				delete state;
		}

		delete mStateCache;
		mStateCache = nsnull;
	}

	if(nsnull != mSurface)
	{
		mSurface->ReleaseView();
		NS_RELEASE(mSurface);
	}

	if(nsnull != mMainSurface)
	{
		mMainSurface->ReleaseView();
		NS_RELEASE(mMainSurface);
	}
	
	if(nsnull != mViewOwner)
		NS_RELEASE(mViewOwner);
}

NS_IMPL_QUERY_INTERFACE(nsRenderingContextBeOS, kRenderingContextIID)
NS_IMPL_ADDREF(nsRenderingContextBeOS)
NS_IMPL_RELEASE(nsRenderingContextBeOS)

NS_IMETHODIMP nsRenderingContextBeOS::Init(nsIDeviceContext* aContext,
										nsIWidget *aWindow)
{
	mContext = aContext;
	NS_IF_ADDREF(mContext);
	
	mSurface = new nsDrawingSurfaceBeOS();
	BView	*view = (BView *)aWindow->GetNativeData(NS_NATIVE_GRAPHIC);
	NS_ADDREF(mSurface);
	mSurface->Init(view);
	mView = view;
	mMainView = mView;
	mMainSurface = mSurface;
    NS_ADDREF(mMainSurface);
	
	mViewOwner = aWindow;
	NS_IF_ADDREF(mViewOwner);

	return CommonInit();
}

NS_IMETHODIMP nsRenderingContextBeOS::Init(nsIDeviceContext* aContext,
                                          nsDrawingSurface aSurface)
{
	mContext = aContext;
	NS_IF_ADDREF(mContext);

	mSurface = (nsDrawingSurfaceBeOS *)aSurface;

	if (nsnull != mSurface)
	{
		NS_ADDREF(mSurface);
		mSurface->GetView(&mView);

		mMainView = mView;
		mMainSurface = mSurface;
		NS_ADDREF(mMainSurface);
	}

	mViewOwner = nsnull;

	return CommonInit();
}

nsresult nsRenderingContextBeOS::SetupView(BView *oldview, BView *newview)
{
	bool		havetounlocknew = false;
	bool		havetounlockold = false;
	BFont		fontinstance(be_plain_font);
	rgb_color	prevcolor;
	BFont		*prevfont;
	BRegion		r;
	bool		gotit = false;

	if(newview && newview->LockLooper())
		havetounlocknew = true;

	newview->SetViewColor(B_TRANSPARENT_32_BIT);

	newview->SetDrawingMode(B_OP_COPY);

	prevfont = &fontinstance;
	prevcolor.red = 0; prevcolor.green = 0; prevcolor.blue = 0; prevcolor.alpha = 255;

	if(oldview)
	{
		if(oldview->LockLooper())
			havetounlockold = true;

		prevcolor = oldview->HighColor();
		oldview->GetFont(&fontinstance);
		oldview->GetClippingRegion(&r);
		gotit = true;

		if(havetounlockold)
			oldview->UnlockLooper();
	}

	newview->SetHighColor(mColor);
	prevfont->SetSpacing(B_BITMAP_SPACING);
	newview->SetFont(prevfont);
	if(gotit)
		newview->ConstrainClippingRegion(&r);

	if(havetounlocknew)
		newview->UnlockLooper();

	return NS_OK;
}

NS_IMETHODIMP nsRenderingContextBeOS::CommonInit()
{
	mContext->GetDevUnitsToAppUnits(mP2T);
	float app2dev;
	mContext->GetAppUnitsToDevUnits(app2dev);
	mTMatrix->AddScale(app2dev, app2dev);

	mContext->GetGammaTable(mGammaTable);

	return SetupView(nsnull, mView);
}

NS_IMETHODIMP nsRenderingContextBeOS::GetHints(PRUint32& aResult)
{
	// We like PRUnichar rendering, hopefully it's not slowing us too much
	aResult = 0;
	return NS_OK;
}

NS_IMETHODIMP nsRenderingContextBeOS::LockDrawingSurface(PRInt32 aX, PRInt32 aY,
                                                          PRUint32 aWidth, PRUint32 aHeight,
                                                          void **aBits, PRInt32 *aStride,
                                                          PRInt32 *aWidthBytes, PRUint32 aFlags)
{
	PushState();
	mSurface->ReleaseView();
	return mSurface->Lock(aX, aY, aWidth, aHeight, aBits, aStride, aWidthBytes, aFlags);
}

NS_IMETHODIMP nsRenderingContextBeOS::UnlockDrawingSurface(void)
{
	PRBool  clipstate;

	mSurface->Unlock();
	mSurface->GetView(&mView);
	PopState(clipstate);

	return NS_OK;
}

NS_IMETHODIMP nsRenderingContextBeOS::SelectOffScreenDrawingSurface(nsDrawingSurface aSurface)
{
	nsresult  rv;

	//XXX this should reset the data in the state stack.

	if(aSurface != mSurface)
	{
		if(nsnull != aSurface)
		{
			BView *v;

			// get back a BView
			((nsDrawingSurfaceBeOS *)aSurface)->GetView(&v);

			rv = SetupView(mView, v);

			// kill the BView
			mSurface->ReleaseView();

			NS_IF_RELEASE(mSurface);
			mSurface = (nsDrawingSurfaceBeOS *)aSurface;
		}
		else
		{
			if(NULL != mView)
			{
				rv = SetupView(mView, mMainView);

				// kill the BView
				mSurface->ReleaseView();

				NS_IF_RELEASE(mSurface);
				mSurface = mMainSurface;
			}
		}

		NS_ADDREF(mSurface);
		mSurface->GetView(&mView);
	}
	else
		rv = NS_OK;
	
	return rv;
}

NS_IMETHODIMP nsRenderingContextBeOS::GetDrawingSurface(nsDrawingSurface *aSurface)
{
	*aSurface = mSurface;
	return NS_OK;
}

NS_IMETHODIMP nsRenderingContextBeOS::Reset()
{
	return NS_OK;
}

NS_IMETHODIMP nsRenderingContextBeOS::GetDeviceContext(nsIDeviceContext *&aContext)
{
	NS_IF_ADDREF(mContext);
	aContext = mContext;
	return NS_OK;
}

NS_IMETHODIMP nsRenderingContextBeOS::PushState(void)
{
	GraphicsState * state = new GraphicsState();
	
	// Push into this state object, add to vector
	state->mMatrix = mTMatrix;
	
	mStateCache->AppendElement(state);
	
	if (nsnull == mTMatrix)
		mTMatrix = new nsTransform2D();
	else
		mTMatrix = new nsTransform2D(mTMatrix);

	PRBool clipState;
	GetClipRect(state->mLocalClip, clipState);

	state->mClipRegion = mRegion;

	if(nsnull != state->mClipRegion)
	{
		mRegion = new nsRegionBeOS();
		mRegion->Init();
		mRegion->SetTo(*state->mClipRegion);
	}

	state->mColor = mCurrentColor;
	state->mLineStyle = mCurrentLineStyle;

	return NS_OK;
}

NS_IMETHODIMP nsRenderingContextBeOS::PopState(PRBool &aClipEmpty)
{
	PRBool bEmpty = PR_FALSE;

	PRUint32 cnt = mStateCache->Count();
	GraphicsState * state;
	
	if(cnt > 0)
	{
		state = (GraphicsState *)mStateCache->ElementAt(cnt - 1);
		mStateCache->RemoveElementAt(cnt - 1);
		
		// Assign all local attributes from the state object just popped
		if (mTMatrix)
			delete mTMatrix;
		mTMatrix = state->mMatrix;

		if(nsnull != mRegion)
			delete mRegion;
		
		mRegion = state->mClipRegion;
		state->mClipRegion = 0;

		if(nsnull != mRegion && mRegion->IsEmpty() == PR_TRUE)
		{
			bEmpty = PR_TRUE;
		}
		else
		{
			// Select in the old region.  We probably want to set a dirty flag and only
			// do this IFF we need to draw before the next Pop.  We'd need to check the
			// state flag on every draw operation.
			if(nsnull != mRegion)
			{
				BRegion *rgn;
				mRegion->GetNativeRegion((void*&)rgn);
				if(mView != mMainView && mView && mView->LockLooper())
				{
					mView->ConstrainClippingRegion(rgn);
					mView->UnlockLooper();
				}
			}
		}

		if(state->mColor != mCurrentColor)
			SetColor(state->mColor);

		if(state->mLineStyle != mCurrentLineStyle)
			SetLineStyle(state->mLineStyle);

		// Delete this graphics state object
		delete state;
	}

	aClipEmpty = bEmpty;

	return NS_OK;
}

NS_IMETHODIMP nsRenderingContextBeOS::IsVisibleRect(const nsRect& aRect,
                                                   PRBool &aVisible)
{
	aVisible = PR_TRUE;
	return NS_OK;
}

NS_IMETHODIMP nsRenderingContextBeOS::GetClipRect(nsRect &aRect, PRBool &aClipValid)
{
	PRInt32 x, y, w, h;
	if(!mRegion->IsEmpty())
	{
		mRegion->GetBoundingBox(&x,&y,&w,&h);
		aRect.SetRect(x,y,w,h);
		aClipValid = PR_TRUE;
	}
	else
	{
		aRect.SetRect(0,0,0,0);
		aClipValid = PR_FALSE;
	}
	
	return NS_OK;
}

NS_IMETHODIMP nsRenderingContextBeOS::SetClipRect(const nsRect& aRect,
                                                 nsClipCombine aCombine,
                                                 PRBool &aClipEmpty)
{
	nsRect trect = aRect;
	BRegion *rgn;

	mTMatrix->TransformCoord(&trect.x, &trect.y, &trect.width, &trect.height);

	switch(aCombine)
	{
		case nsClipCombine_kIntersect:
			mRegion->Intersect(trect.x,trect.y,trect.width,trect.height);
			break;
		case nsClipCombine_kUnion:
			mRegion->Union(trect.x,trect.y,trect.width,trect.height);
			break;
		case nsClipCombine_kSubtract:
			mRegion->Subtract(trect.x,trect.y,trect.width,trect.height);
			break;
		case nsClipCombine_kReplace:
			mRegion->SetTo(trect.x,trect.y,trect.width,trect.height);
			break;
	}

	aClipEmpty = mRegion->IsEmpty();

	mRegion->GetNativeRegion((void*&)rgn);
	if(mView && mView->LockLooper())
	{
		mView->ConstrainClippingRegion(rgn);
		mView->UnlockLooper();
	}

	return NS_OK;
}

NS_IMETHODIMP nsRenderingContextBeOS::SetClipRegion(const nsIRegion& aRegion,
                                                   nsClipCombine aCombine,
                                                   PRBool &aClipEmpty)
{
	BRegion *rgn;
	
	switch(aCombine)
	{
		case nsClipCombine_kIntersect:
			mRegion->Intersect(aRegion);
			break;
		case nsClipCombine_kUnion:
			mRegion->Union(aRegion);
			break;
		case nsClipCombine_kSubtract:
			mRegion->Subtract(aRegion);
			break;
		case nsClipCombine_kReplace:
			mRegion->SetTo(aRegion);
			break;
	}
	
	aClipEmpty = mRegion->IsEmpty();
	mRegion->GetNativeRegion((void*&)rgn);
	if(mView && mView->LockLooper())
	{
		mView->ConstrainClippingRegion(rgn);
		mView->UnlockLooper();
	}
	
	return NS_OK;
}

NS_IMETHODIMP nsRenderingContextBeOS::GetClipRegion(nsIRegion **aRegion)
{
	nsresult  rv = NS_OK;

	NS_ASSERTION(!(nsnull == aRegion), "no region ptr");

	if (nsnull == *aRegion)
	{
		nsRegionBeOS *rgn = new nsRegionBeOS();

		if(nsnull != rgn)
		{
			NS_ADDREF(rgn);

			rv = rgn->Init();

			if (NS_OK == rv)
				*aRegion = rgn;
			else
				NS_RELEASE(rgn);
		}
		else
			rv = NS_ERROR_OUT_OF_MEMORY;
	}

	if (rv == NS_OK)
		(*aRegion)->SetTo(*mRegion);

	return rv;
}

NS_IMETHODIMP nsRenderingContextBeOS::SetColor(nscolor aColor)
{
	mCurrentColor = aColor;
	mColor.red = mGammaTable[NS_GET_R(aColor)];
	mColor.green = mGammaTable[NS_GET_G(aColor)];
	mColor.blue = mGammaTable[NS_GET_B(aColor)];
	mColor.alpha = 255;	// solid
	if(mView)
	{
		if(mView->LockLooper())
		{
			mView->SetHighColor(mColor);
			mView->UnlockLooper();
		}
		else
			mView->SetHighColor(mColor);
	}
	return NS_OK;
}

NS_IMETHODIMP nsRenderingContextBeOS::GetColor(nscolor &aColor) const
{
	aColor = mCurrentColor;
	return NS_OK;
}

NS_IMETHODIMP nsRenderingContextBeOS::SetFont(const nsFont& aFont)
{
	NS_IF_RELEASE(mFontMetrics);
	mContext->GetMetricsFor(aFont, mFontMetrics);
	return SetFont(mFontMetrics);
}

NS_IMETHODIMP nsRenderingContextBeOS::SetFont(nsIFontMetrics *aFontMetrics)
{
	NS_IF_RELEASE(mFontMetrics);
	mFontMetrics = aFontMetrics;
	NS_IF_ADDREF(mFontMetrics);
	
	if(mFontMetrics)
	{
		nsFontHandle  fontHandle;
		mFontMetrics->GetFontHandle(fontHandle);
		mCurrentFont = (BFont *)fontHandle;
		
		if(mView && mView->LockLooper())
		{
			mView->SetFont(mCurrentFont);
			mView->UnlockLooper();
		}
	}
	
	return NS_OK;
}

// #pragma mark checkme
NS_IMETHODIMP nsRenderingContextBeOS::SetLineStyle(nsLineStyle aLineStyle)
{
  if (aLineStyle != mCurrentLineStyle)
  {
printf("nsRenderingContextBeOS::SetLineStyle not implemented!\n");
#if 0
    switch(aLineStyle)
    { 
      case nsLineStyle_kSolid:
        ::gdk_gc_set_line_attributes(mSurface->gc,
                          1, GDK_LINE_SOLID, (GdkCapStyle)0, (GdkJoinStyle)0);
        break;

      case nsLineStyle_kDashed: {
        static char dashed[2] = {4,4};

        ::gdk_gc_set_dashes(mSurface->gc, 
                     0, dashed, 2);
        } break;

      case nsLineStyle_kDotted: {
        static char dotted[2] = {3,1};

        ::gdk_gc_set_dashes(mSurface->gc, 
                     0, dotted, 2);
         }break;

      default:
        break;

    }
#endif

    mCurrentLineStyle = aLineStyle;
  }

  return NS_OK;

}

NS_IMETHODIMP nsRenderingContextBeOS::GetLineStyle(nsLineStyle &aLineStyle)
{
	aLineStyle = mCurrentLineStyle;
	return NS_OK;
}

NS_IMETHODIMP nsRenderingContextBeOS::GetFontMetrics(nsIFontMetrics *&aFontMetrics)
{
	NS_IF_ADDREF(mFontMetrics);
	aFontMetrics = mFontMetrics;
	return NS_OK;
}

// add the passed in translation to the current translation
NS_IMETHODIMP nsRenderingContextBeOS::Translate(nscoord aX, nscoord aY)
{
	mTMatrix->AddTranslation((float)aX,(float)aY);
	return NS_OK;
}

// add the passed in scale to the current scale
NS_IMETHODIMP nsRenderingContextBeOS::Scale(float aSx, float aSy)
{
	mTMatrix->AddScale(aSx, aSy);
	return NS_OK;
}

NS_IMETHODIMP nsRenderingContextBeOS::GetCurrentTransform(nsTransform2D *&aTransform)
{
	aTransform = mTMatrix;
	return NS_OK;
}

NS_IMETHODIMP nsRenderingContextBeOS::CreateDrawingSurface(nsRect *aBounds,
                                                          PRUint32 aSurfFlags,
                                                          nsDrawingSurface &aSurface)
{
	nsDrawingSurfaceBeOS *surf = new nsDrawingSurfaceBeOS();

	if(nsnull != surf)
	{
		NS_ADDREF(surf);

		if (nsnull != aBounds)
			surf->Init(mMainView, aBounds->width, aBounds->height, aSurfFlags);
		else
			surf->Init(mMainView, 0, 0, aSurfFlags);
	}

	aSurface = (nsDrawingSurface)surf;

	return NS_OK;
}

NS_IMETHODIMP nsRenderingContextBeOS::DestroyDrawingSurface(nsDrawingSurface aDS)
{
	nsDrawingSurfaceBeOS *surf = (nsDrawingSurfaceBeOS *)aDS;
	
	// are we using the surface that we want to kill?
	if(surf == mSurface)
	{
		// remove our local ref to the surface
		NS_IF_RELEASE(mSurface);

		mView = mMainView;
		mSurface = mMainSurface;

		// two pointers: two refs
		NS_IF_ADDREF(mSurface);
	}

	// release it...
	NS_IF_RELEASE(surf);

	return NS_OK;
}

NS_IMETHODIMP nsRenderingContextBeOS::DrawLine(nscoord aX0, nscoord aY0, nscoord aX1, nscoord aY1)
{
	if(nsLineStyle_kNone == mCurrentLineStyle)
		return NS_OK;
	
	mTMatrix->TransformCoord(&aX0, &aY0);
	mTMatrix->TransformCoord(&aX1, &aY1);

	if(mView && mView->LockLooper())
	{
		mView->StrokeLine(BPoint(aX0, aY0), BPoint(aX1, aY1));	// FIXME: add line style
		mView->UnlockLooper();
	}
	
	return NS_OK;
}

NS_IMETHODIMP nsRenderingContextBeOS::DrawPolyline(const nsPoint aPoints[], PRInt32 aNumPoints)
{
	if(nsLineStyle_kNone == mCurrentLineStyle)
		return NS_OK;
	
	// First transform nsPoint's into BPoint's; perform coordinate space
	// transformation at the same time
	BPoint pts[20];
	BPoint *pp0 = pts;
	
	if(aNumPoints > 20)
		pp0 = new BPoint[aNumPoints];
	
	BPoint *pp = pp0;
	const nsPoint *np = &aPoints[0];
	
	for(PRInt32 i = 0; i < aNumPoints; i++, pp++, np++)
	{
		int x = np->x;
		int y = np->y;
		mTMatrix->TransformCoord(&x, &y);
		pp->x = x;
		pp->y = y;
	}
	
	// Draw the polyline
	if(mView && mView->LockLooper())
	{
		mView->StrokePolygon(pp0, aNumPoints, false);	// FIXME: add line style
		mView->UnlockLooper();
	}

	// Release temporary storage if necessary
	if(pp0 != pts)
		delete pp0;
	
	return NS_OK;
}

NS_IMETHODIMP nsRenderingContextBeOS::DrawRect(const nsRect& aRect)
{
	return DrawRect(aRect.x, aRect.y, aRect.width, aRect.height);
}

NS_IMETHODIMP nsRenderingContextBeOS::DrawRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
	nscoord x,y,w,h;
	
	x = aX;
	y = aY;
	w = aWidth;
	h = aHeight;
	
	mTMatrix->TransformCoord(&x, &y, &w, &h);

	// Draw the rect
	if(mView && mView->LockLooper())
	{
		if( 1 == h )
		{
			mView->StrokeLine(BPoint(x, y), BPoint(x + w-1, y));	// FIXME: add line style
		}
		else
			mView->StrokeRect(BRect(x, y, x + w - 1, y + h - 1));	// FIXME: add line style
		mView->UnlockLooper();
	}

	return NS_OK;
}

NS_IMETHODIMP nsRenderingContextBeOS::FillRect(const nsRect& aRect)
{
	return FillRect(aRect.x, aRect.y, aRect.width, aRect.height);
}

NS_IMETHODIMP nsRenderingContextBeOS::FillRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
	nscoord x,y,w,h;
	
	x = aX;
	y = aY;
	w = aWidth;
	h = aHeight;
	
	mTMatrix->TransformCoord(&x, &y, &w, &h);
	
	// Fill the rect
	if(mView && mView->LockLooper())
	{
		if( 1 == h )
		{
			mView->StrokeLine(BPoint(x, y), BPoint(x + w-1, y));	// FIXME: add line style
		}
		else
			mView->FillRect(BRect(x, y, x + w - 1, y + h - 1), B_SOLID_HIGH);
		mView->UnlockLooper();
	}

	return NS_OK;
}

NS_IMETHODIMP 
nsRenderingContextBeOS :: InvertRect(const nsRect& aRect)
{
	NS_NOTYETIMPLEMENTED("nsRenderingContextBeOS::InvertRect");

  return NS_OK;
}

NS_IMETHODIMP 
nsRenderingContextBeOS :: InvertRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
	NS_NOTYETIMPLEMENTED("nsRenderingContextBeOS::InvertRect");

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextBeOS::DrawPolygon(const nsPoint aPoints[], PRInt32 aNumPoints)
{
	// First transform nsPoint's into BPoint's; perform coordinate space
	// transformation at the same time
	BPoint pts[20];
	BPoint *pp0 = pts;
	
	if(aNumPoints > 20)
		pp0 = new BPoint[aNumPoints];
	
	BPoint *pp = pp0;
	const nsPoint *np = &aPoints[0];
	
	for(PRInt32 i = 0; i < aNumPoints; i++, pp++, np++)
	{
		int x = np->x;
		int y = np->y;
		mTMatrix->TransformCoord(&x, &y);
		pp->x = x;
		pp->y = y;
	}
	
	// Draw the polyline
	if(mView && mView->LockLooper())
	{
		// FIXME: ignore linestyle, like Windows???
		mView->StrokePolygon(pp0, aNumPoints, true, B_SOLID_HIGH);
		mView->UnlockLooper();
	}

	// Release temporary storage if necessary
	if(pp0 != pts)
		delete pp0;
	
	return NS_OK;
}

NS_IMETHODIMP nsRenderingContextBeOS::FillPolygon(const nsPoint aPoints[], PRInt32 aNumPoints)
{
	// First transform nsPoint's into BPoint's; perform coordinate space
	// transformation at the same time
	BPoint pts[20];
	BPoint *pp0 = pts;
	
	if(aNumPoints > 20)
		pp0 = new BPoint[aNumPoints];
	
	BPoint *pp = pp0;
	const nsPoint *np = &aPoints[0];
	
	for(PRInt32 i = 0; i < aNumPoints; i++, pp++, np++)
	{
		int x = np->x;
		int y = np->y;
		mTMatrix->TransformCoord(&x, &y);
		pp->x = x;
		pp->y = y;
	}
	
	// Fill the polygon
	if(mView && mView->LockLooper())
	{
		mView->FillPolygon(pp0, aNumPoints, B_SOLID_HIGH);
		mView->UnlockLooper();
	}

	// Release temporary storage if necessary
	if(pp0 != pts)
		delete pp0;
	
	return NS_OK;
}

NS_IMETHODIMP nsRenderingContextBeOS::DrawEllipse(const nsRect& aRect)
{
	return DrawEllipse(aRect.x, aRect.y, aRect.width, aRect.height);
}

NS_IMETHODIMP nsRenderingContextBeOS::DrawEllipse(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
	nscoord x,y,w,h;
	
	x = aX;
	y = aY;
	w = aWidth;
	h = aHeight;
	
	mTMatrix->TransformCoord(&x,&y,&w,&h);
	
	// Draw the ellipse
	if(mView && mView->LockLooper())
	{
		mView->StrokeEllipse(BRect(x, y, x + w - 1, y + h - 1));	// FIXME: add line style
		mView->UnlockLooper();
	}
	
	return NS_OK;
}

NS_IMETHODIMP nsRenderingContextBeOS::FillEllipse(const nsRect& aRect)
{
	return FillEllipse(aRect.x, aRect.y, aRect.width, aRect.height);
}

NS_IMETHODIMP nsRenderingContextBeOS::FillEllipse(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
	nscoord x,y,w,h;

	x = aX;
	y = aY;
	w = aWidth;
	h = aHeight;

	mTMatrix->TransformCoord(&x,&y,&w,&h);

	// Fill the ellipse
	if(mView && mView->LockLooper())
	{
		mView->FillEllipse(BRect(x, y, x + w - 1, y + h - 1));	// FIXME: add line style
		mView->UnlockLooper();
	}

	return NS_OK;
}

NS_IMETHODIMP nsRenderingContextBeOS::DrawArc(const nsRect& aRect,
                                             float aStartAngle, float aEndAngle)
{
	return DrawArc(aRect.x,aRect.y,aRect.width,aRect.height,aStartAngle,aEndAngle);
}

NS_IMETHODIMP nsRenderingContextBeOS::DrawArc(nscoord aX, nscoord aY,
                                             nscoord aWidth, nscoord aHeight,
                                             float aStartAngle, float aEndAngle)
{
	nscoord x,y,w,h;

	x = aX;
	y = aY;
	w = aWidth;
	h = aHeight;

	mTMatrix->TransformCoord(&x,&y,&w,&h);

	// Draw the arc
	if(mView && mView->LockLooper())
	{
		mView->StrokeArc(BRect(x, y, x + w - 1, y + h - 1), aStartAngle, aEndAngle - aStartAngle);	// FIXME: add line style
		mView->UnlockLooper();
	}

	return NS_OK;
}

NS_IMETHODIMP nsRenderingContextBeOS::FillArc(const nsRect& aRect,
                                             float aStartAngle, float aEndAngle)
{
	return FillArc(aRect.x,aRect.y,aRect.width,aRect.height,aStartAngle,aEndAngle);
}


NS_IMETHODIMP nsRenderingContextBeOS::FillArc(nscoord aX, nscoord aY,
                                             nscoord aWidth, nscoord aHeight,
                                             float aStartAngle, float aEndAngle)
{
	nscoord x,y,w,h;

	x = aX;
	y = aY;
	w = aWidth;
	h = aHeight;

	mTMatrix->TransformCoord(&x,&y,&w,&h);

	// Fill the arc
	if(mView && mView->LockLooper())
	{
		mView->FillArc(BRect(x, y, x + w - 1, y + h - 1), aStartAngle, aEndAngle - aStartAngle);
		mView->UnlockLooper();
	}

	return NS_OK;
}

NS_IMETHODIMP nsRenderingContextBeOS::GetWidth(char aC, nscoord &aWidth)
{
	char buf[1];
	buf[0] = aC;
	return GetWidth(buf, 1, aWidth);
}

NS_IMETHODIMP nsRenderingContextBeOS::GetWidth(PRUnichar aC, nscoord& aWidth,
                                PRInt32* aFontID)
{
	PRUnichar buf[1];
	buf[0] = aC;
	return GetWidth(buf, 1, aWidth, aFontID);
}

NS_IMETHODIMP nsRenderingContextBeOS::GetWidth(const nsString& aString,
                                nscoord& aWidth, PRInt32* aFontID)
{
	return GetWidth(aString.GetUnicode(), aString.Length(), aWidth, aFontID);
}

NS_IMETHODIMP
nsRenderingContextBeOS::GetWidth(const char* aString, nscoord& aWidth)
{
	return GetWidth(aString, strlen(aString), aWidth);
}

NS_IMETHODIMP nsRenderingContextBeOS::GetWidth(const char* aString, PRUint32 aLength,
                                nscoord& aWidth)
{
	if(mView && mView->LockLooper())
	{
		aWidth = nscoord(mView->StringWidth(aString, aLength) * mP2T);
		mView->UnlockLooper();
	}  
	return NS_OK;
}


/* macro to convert a ushort* uni_string into a char* or uchar* utf8_string,
   one character at a time. Move the pointer. You can check terminaison on
   the uni_string by testing : if (uni_string[0] == 0)
   WARNING : you need to use EXPLICIT POINTERS for both str and unistr. */
#define convert_to_utf8(str, uni_str)\
{\
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
		int   val;\
		val = ((uni_str[0]-0xd7c0)<<10) | (uni_str[1]&0x3ff);\
		str[0] = 0xf0 | (val>>18);\
		str[1] = 0x80 | ((val>>12)&0x3f);\
		str[2] = 0x80 | ((val>>6)&0x3f);\
		str[3] = 0x80 | (val&0x3f);\
		uni_str += 2; str += 4;\
	}\
}	

NS_IMETHODIMP nsRenderingContextBeOS::GetWidth(const PRUnichar* aString, PRUint32 aLength,
                                nscoord& aWidth, PRInt32* aFontID)
{
	uint8 *utf8str = new uint8 [aLength * 4 + 1];	// max UTF-8 string length
	uint8 *utf8ptr = utf8str;
	const uint16 *uniptr = aString;
	for(PRUint32 i = 0; i < aLength; i++)
		convert_to_utf8(utf8ptr, uniptr);
	GetWidth((char *)utf8str, aLength, aWidth);
	delete [] utf8str;

	return NS_OK;
}

NS_IMETHODIMP nsRenderingContextBeOS::DrawString(const char *aString, PRUint32 aLength,
                                  nscoord aX, nscoord aY,
                                  const nscoord* aSpacing)
{
	if(! aLength)
		return NS_OK;

	PRInt32	x = aX;
	PRInt32 y = aY;

	// Substract xFontStruct ascent since drawing specifies baseline
	if(mFontMetrics)
	{
		mFontMetrics->GetMaxAscent(y);
		y += aY;
	}

	mTMatrix->TransformCoord(&x, &y);
	y++;	// BView::DrawString quirk

	if(mView && mView->LockLooper())
	{
		// FIXME: the following is *very* inefficient for text rendering,
		// but it's the easiest way to render antialiased text correctly
		mView->SetDrawingMode(B_OP_OVER);

		if(nsnull != aSpacing)
		{
			PRIntn dxMem[500];
			PRIntn *dx0;

			dx0 = dxMem;
			if(aLength > 500)
				dx0 = new PRIntn[aLength];
			mTMatrix->ScaleXCoords(aSpacing, aLength, dx0);

			for(PRUint32 i = 0; i < aLength; i++)
				mView->DrawString(&aString[i], 1L, BPoint(dx0[i], y));
	
			if((nsnull != aSpacing) && (dx0 != dxMem))
				delete [] dx0;
		}
		else
			mView->DrawString(aString, aLength, BPoint(x, y));

		mView->SetDrawingMode(B_OP_COPY);

		mView->UnlockLooper();
	}
	
	return NS_OK;
}

NS_IMETHODIMP nsRenderingContextBeOS::DrawString(const PRUnichar* aString, PRUint32 aLength,
                                  nscoord aX, nscoord aY,
                                  PRInt32 aFontID,
                                  const nscoord* aSpacing)
{
	uint8 *utf8str = new uint8 [aLength * 4 + 1];	// max UTF-8 string length
	uint8 *utf8ptr = utf8str;
	const uint16 *uniptr = aString;
	for(PRUint32 i = 0; i < aLength; i++)
		convert_to_utf8(utf8ptr, uniptr);
	DrawString((char *)utf8str, aLength, aX, aY, aSpacing);
	delete [] utf8str;
	return NS_OK;
}

NS_IMETHODIMP nsRenderingContextBeOS::DrawString(const nsString& aString,
                                  nscoord aX, nscoord aY,
                                  PRInt32 aFontID,
                                  const nscoord* aSpacing)
{
	return DrawString(aString.GetUnicode(), aString.Length(), aX, aY, aFontID, aSpacing);
}

NS_IMETHODIMP nsRenderingContextBeOS::DrawImage(nsIImage *aImage, nscoord aX, nscoord aY)
{
	nscoord width,height;
	width = NSToCoordRound(mP2T * aImage->GetWidth());
	height = NSToCoordRound(mP2T * aImage->GetHeight());
	
	return DrawImage(aImage, aX, aY, width, height);
}

NS_IMETHODIMP nsRenderingContextBeOS::DrawImage(nsIImage *aImage, nscoord aX, nscoord aY,
                     nscoord aWidth, nscoord aHeight)
{
	nsRect	tr;
	
	tr.x = aX;
	tr.y = aY;
	tr.width = aWidth;
	tr.height = aHeight;
	
	return DrawImage(aImage,tr);
}

NS_IMETHODIMP nsRenderingContextBeOS::DrawImage(nsIImage *aImage, const nsRect& aRect)
{
	nsRect	tr;
	
	tr = aRect;
	mTMatrix->TransformCoord(&tr.x,&tr.y,&tr.width,&tr.height);
	
	return aImage->Draw(*this,mSurface,tr.x,tr.y,tr.width,tr.height);
}

NS_IMETHODIMP nsRenderingContextBeOS::DrawImage(nsIImage *aImage, const nsRect& aSRect, const nsRect& aDRect)
{
	nsRect	sr,dr;
	
	sr = aSRect;
	mTMatrix ->TransformCoord(&sr.x,&sr.y,&sr.width,&sr.height);
	
	dr = aDRect;
	mTMatrix->TransformCoord(&dr.x,&dr.y,&dr.width,&dr.height);
	
	return aImage->Draw(*this,mSurface,sr.x,sr.y,sr.width,sr.height,
		dr.x,dr.y,dr.width,dr.height);
}

// #pragma mark checkme
NS_IMETHODIMP
nsRenderingContextBeOS::CopyOffScreenBits(nsDrawingSurface aSrcSurf,
                                         PRInt32 aSrcX, PRInt32 aSrcY,
                                         const nsRect &aDestBounds,
                                         PRUint32 aCopyFlags)
{
	if((nsnull != aSrcSurf) && (nsnull != mMainView))
	{
		PRInt32 x = aSrcX;
		PRInt32 y = aSrcY;
		nsRect	drect = aDestBounds;
		BBitmap	*srcbitmap;
		BView	*srcview;
		BView	*destview;

		// get back a BBitmap
		((nsDrawingSurfaceBeOS *)aSrcSurf)->GetBitmap(&srcbitmap);
		((nsDrawingSurfaceBeOS *)aSrcSurf)->GetView(&srcview);

		if(srcview->LockLooper())
		{
			if(nsnull != srcbitmap)
			{
				if(aCopyFlags & NS_COPYBITS_TO_BACK_BUFFER)
				{
					NS_ASSERTION(!(nsnull == mView), "no back buffer");
					destview = mView;
				}
				else
					destview = mMainView;

				if(destview->LockLooper())
				{
					if(aCopyFlags & NS_COPYBITS_USE_SOURCE_CLIP_REGION)
					{
						BRegion r;
						srcview->GetClippingRegion(&r);
						destview->ConstrainClippingRegion(&r);
					}

					if(aCopyFlags & NS_COPYBITS_XFORM_SOURCE_VALUES)
						mTMatrix->TransformCoord(&x, &y);

					if(aCopyFlags & NS_COPYBITS_XFORM_DEST_VALUES)
						mTMatrix->TransformCoord(&drect.x, &drect.y, &drect.width, &drect.height);

					destview->DrawBitmapAsync(srcbitmap, BRect(x, y, x + drect.width - 1, y + drect.height - 1), BRect(drect.x, drect.y, drect.x + drect.width - 1, drect.y + drect.height - 1));
					destview->Sync();
					destview->UnlockLooper();
				}
			}
			else
				printf("nsRenderingContextBeOS::CopyOffScreenBits - FIXME: should render from surface without bitmap!?!?!\n");
//				NS_ASSERTION(0, "attempt to blit with bad BViews");

			srcview->UnlockLooper();

			// kill the source bitmap
			((nsDrawingSurfaceBeOS *)aSrcSurf)->ReleaseBitmap();
			((nsDrawingSurfaceBeOS *)aSrcSurf)->ReleaseView();
		}
	}
	else
		NS_ASSERTION(0, "attempt to blit with bad BViews");

	return NS_OK;
}

NS_IMETHODIMP nsRenderingContextBeOS::RetrieveCurrentNativeGraphicData(PRUint32 * ngd)
{
  return NS_OK;
}
