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
 
/* 
 String

translated to a number via  getfnum()

TextFont(number)
TextSize
TextFace  -- may not play well with new style  
DrawString();  DrawText for cstrings
*/

#include "nsRenderingContextMac.h"
#include "nsDeviceContextMac.h"

#include <math.h>
#include "nspr.h"

#include "nsRegionMac.h"
#include "nsGfxCIID.h"

//#define NO_CLIP

/*
  Some Implementation Notes

  REGIONS:  Regions are clipping rects associated with a GC. Since 
  multiple Drawable's can and do share GC's (they are hardware cached)
  In order to select clip rect's into GC's, they must be writeable. Thus, 
  any consumer of the 'gfx' library must assume that GC's created by them
  will be modified in gfx.

 */

class GraphicsState
{
public:
  GraphicsState();
  ~GraphicsState();

	GrafPtr					*mCurPort;		// port set up on the stack
  nsTransform2D   *mMatrix;
  nsRect          mLocalClip;
  RgnHandle       mClipRegion;
  nscolor         mColor;
  nsIFontMetrics  *mFontMetrics;
  PRInt32        	mFont;
};

//------------------------------------------------------------------------

GraphicsState :: GraphicsState()
{
  mMatrix = nsnull;  
  mLocalClip.x = mLocalClip.y = mLocalClip.width = mLocalClip.height = 0;
  mClipRegion = nsnull;
  mColor = NS_RGB(0, 0, 0);
  mFontMetrics = nsnull;
  mFont = 0;
}

//------------------------------------------------------------------------

GraphicsState :: ~GraphicsState()
{
	mFont = 0;
}

static NS_DEFINE_IID(kRenderingContextIID, NS_IRENDERING_CONTEXT_IID);

//------------------------------------------------------------------------

nsRenderingContextMac :: nsRenderingContextMac()
{
  NS_INIT_REFCNT();

  //mFontCache = nsnull ;
  mFontMetrics = nsnull ;
  mContext = nsnull ;
  mFrontBuffer = nsnull ;
  mRenderingSurface = nsnull ;
  mCurrentColor = 0;
  mTMatrix = nsnull;
  mP2T = 1.0f;
  mStateCache = new nsVoidArray();
  mClipRegion = nsnull;
  mCurrFontHandle = 0;
  PushState();
}

//------------------------------------------------------------------------

nsRenderingContextMac :: ~nsRenderingContextMac()
{

  if (mClipRegion) 
  	{
    ::DisposeRgn(mClipRegion);
    mClipRegion = nsnull;
  	}
  	
  mTMatrix = nsnull;

  // Destroy the State Machine
  if (nsnull != mStateCache)
  	{
    PRInt32 cnt = mStateCache->Count();

    while (--cnt >= 0)
    	{
      GraphicsState *state = (GraphicsState *)mStateCache->ElementAt(cnt);
      mStateCache->RemoveElementAt(cnt);

      if (nsnull != state)
        delete state;
    	}
    delete mStateCache;
    mStateCache = nsnull;
  	}

  // Destroy the front buffer and it's GC if one was allocated for it
  if (nsnull != mFrontBuffer) 
  	{
    if (mFrontBuffer != mRenderingSurface) 
    	{
      //::XFreeGC(mFrontBuffer->display,mFrontBuffer->gc);
    	}
    delete mFrontBuffer;
  	}

  NS_IF_RELEASE(mFontMetrics);
  NS_IF_RELEASE(mFontCache);
  NS_IF_RELEASE(mContext);

}

NS_IMPL_QUERY_INTERFACE(nsRenderingContextMac, kRenderingContextIID)
NS_IMPL_ADDREF(nsRenderingContextMac)
NS_IMPL_RELEASE(nsRenderingContextMac)

//------------------------------------------------------------------------

nsresult nsRenderingContextMac :: Init(nsIDeviceContext* aContext,nsIWidget *aWindow)
{

  if (nsnull == aWindow->GetNativeData(NS_NATIVE_WINDOW))
    return NS_ERROR_NOT_INITIALIZED;

  mContext = aContext;
  NS_IF_ADDREF(mContext);


  mRenderingSurface = (nsDrawingSurfaceMac)aWindow->GetNativeData(NS_NATIVE_DISPLAY);
	mCurrentSurface = mRenderingSurface;  
  mFrontBuffer = mRenderingSurface;
  
  mMainRegion = (RgnHandle)aWindow->GetNativeData(NS_NATIVE_REGION);
  
  return (CommonInit());
}

//------------------------------------------------------------------------

nsresult nsRenderingContextMac :: Init(nsIDeviceContext* aContext,
					nsDrawingSurface aSurface)
{

  mContext = aContext;
  NS_IF_ADDREF(mContext);

  mRenderingSurface = (nsDrawingSurfaceMac) aSurface;
  return (CommonInit());
}

//------------------------------------------------------------------------


nsresult nsRenderingContextMac :: CommonInit()
{
	((nsDeviceContextMac *)mContext)->SetDrawingSurface(mRenderingSurface);
  //((nsDeviceContextMac *)mContext)->InstallColormap();

  mFontCache = mContext->GetFontCache();
  mContext->GetDevUnitsToAppUnits(mP2T);
  float app2dev;
  mContext->GetAppUnitsToDevUnits(app2dev);
  mTMatrix->AddScale(app2dev, app2dev);
  return NS_OK;
}

//------------------------------------------------------------------------

nsresult nsRenderingContextMac :: SelectOffScreenDrawingSurface(nsDrawingSurface aSurface)
{  

/*  if (mFrontBuffer == mRenderingSurface) {
    XGCValues values;
    mFrontBuffer->gc = ::XCreateGC(mRenderingSurface->display,
				   mRenderingSurface->drawable,
				   nsnull, &values);
  }
  
  mRenderingSurface = (nsDrawingSurfaceMac *) aSurface;  */
  return NS_OK;
}

//------------------------------------------------------------------------

void nsRenderingContextMac :: Reset()
{
}

//------------------------------------------------------------------------

nsIDeviceContext * nsRenderingContextMac :: GetDeviceContext(void)
{
  NS_IF_ADDREF(mContext);
  return mContext;
}

//------------------------------------------------------------------------

void nsRenderingContextMac :: PushState(void)
{
nsRect 	rect;
Rect		mac_rect;

	
  GraphicsState * state = new GraphicsState();

  // Push into this state object, add to vector
  state->mMatrix = mTMatrix;

  mStateCache->AppendElement(state);

  if (nsnull == mTMatrix)
    mTMatrix = new nsTransform2D();
  else
    mTMatrix = new nsTransform2D(mTMatrix);

  GetClipRect(state->mLocalClip);

  state->mClipRegion = mClipRegion;

  if (nsnull != state->mClipRegion) 
  	{
    mClipRegion = NewRgn();
    
    mac_rect.left = state->mLocalClip.x;
    mac_rect.top = state->mLocalClip.y;
    mac_rect.right = state->mLocalClip.x + state->mLocalClip.width;
    mac_rect.bottom = state->mLocalClip.y + state->mLocalClip.height;
    
    SetRectRgn(mClipRegion,state->mLocalClip.x,state->mLocalClip.y,
    					state->mLocalClip.x + state->mLocalClip.width,state->mLocalClip.y + state->mLocalClip.height);
  	}

  state->mColor = mCurrentColor;
}

//------------------------------------------------------------------------

PRBool nsRenderingContextMac :: PopState(void)
{
PRBool bEmpty = PR_FALSE;

	
  PRUint32 cnt = mStateCache->Count();
  GraphicsState * state;

  if (cnt > 0) 
  	{
    state = (GraphicsState *)mStateCache->ElementAt(cnt - 1);
    mStateCache->RemoveElementAt(cnt - 1);

    // Assign all local attributes from the state object just popped
    if (mTMatrix)
      delete mTMatrix;
    mTMatrix = state->mMatrix;

    if (nsnull != mClipRegion)
      ::DisposeRgn(mClipRegion);

    mClipRegion = state->mClipRegion;

    if (nsnull != mClipRegion && ::EmptyRgn(mClipRegion) == PR_TRUE)
    	{
      bEmpty = PR_TRUE;
    	}
  	else
  		{
      // Select in the old region.  We probably want to set a dirty flag and only 
      // do this IFF we need to draw before the next Pop.  We'd need to check the
      // state flag on every draw operation.
      if (nsnull != mClipRegion)
      	{
      	// set this region as the currentclippig region
      	::SetClip(mClipRegion);
				}
    	}

    if (state->mColor != mCurrentColor)
      SetColor(state->mColor);
    
    // Delete this graphics state object
    delete state;
  	}

  return bEmpty;
  return PR_FALSE;
}

//------------------------------------------------------------------------

PRBool nsRenderingContextMac :: IsVisibleRect(const nsRect& aRect)
{
  return PR_TRUE;
}

//------------------------------------------------------------------------

PRBool nsRenderingContextMac :: SetClipRectInPixels(const nsRect& aRect, nsClipCombine aCombine)
{
PRBool 		bEmpty = PR_FALSE;
nsRect  	trect = aRect;
RgnHandle	theregion,tregion;

  theregion = ::NewRgn();
  SetRectRgn(theregion,trect.x,trect.y,trect.x+trect.width,trect.y+trect.height);

  if (aCombine == nsClipCombine_kIntersect)
  	{
    if (nsnull != mClipRegion) 
    	{
    	tregion = ::NewRgn();
    	::SectRgn(theregion,mClipRegion,tregion);
      ::DisposeRgn(theregion);
      ::DisposeRgn(mClipRegion);
      mClipRegion = tregion;
    	} 
  	}
  else 
  	if (aCombine == nsClipCombine_kUnion)
  		{
	    if (nsnull != mClipRegion) 
	    	{
	      tregion = ::NewRgn();
	      ::UnionRgn(theregion, mClipRegion, tregion);
	      ::DisposeRgn(mClipRegion);
	      ::DisposeRgn(theregion);
	      mClipRegion = tregion;
	    	} 
  		}
  	else 
  		if (aCombine == nsClipCombine_kSubtract)
  			{
		    if (nsnull != mClipRegion) 
		    	{
		      tregion = ::NewRgn();
		      ::DiffRgn(mClipRegion, theregion, tregion);
		      ::DisposeRgn(mClipRegion);
		      ::DisposeRgn(theregion);
		      mClipRegion = tregion;
		    	} 
  			}
  	else 
  		if (aCombine == nsClipCombine_kReplace)
  			{
		    if (nsnull != mClipRegion)
		      ::DisposeRgn(mClipRegion);

		    mClipRegion = theregion;
  			}
  		else
    		NS_ASSERTION(PR_FALSE, "illegal clip combination");

  if (nsnull == mClipRegion)
  	{
    bEmpty = PR_TRUE;
    // clip to the size of the window
    ::SetPort(mRenderingSurface);
    ::ClipRect(&mRenderingSurface->portRect);
  	} 
 	else 
 		{
 		// set the clipping area of this windowptr
 		::SetPort(mRenderingSurface);
    ::SetClip(mClipRegion);
  	}

  return bEmpty;
}

//------------------------------------------------------------------------

PRBool nsRenderingContextMac :: SetClipRect(const nsRect& aRect, nsClipCombine aCombine)
{
nsRect  trect = aRect;

  mTMatrix->TransformCoord(&trect.x, &trect.y,&trect.width, &trect.height);
  return(SetClipRectInPixels(trect,aCombine));
}

//------------------------------------------------------------------------

PRBool nsRenderingContextMac :: GetClipRect(nsRect &aRect)
{
Rect	cliprect;

  if (mClipRegion != nsnull) 
  	{
  	cliprect = (**mClipRegion).rgnBBox;
    aRect.SetRect(cliprect.left, cliprect.top, cliprect.right-cliprect.left, cliprect.bottom-cliprect.top);
    return(PR_FALSE);
  	} 
 	else 
 		{
    aRect.SetRect(0,0,0,0);
    return (PR_TRUE);
  	}
}

//------------------------------------------------------------------------



//------------------------------------------------------------------------

PRBool nsRenderingContextMac :: SetClipRegion(const nsIRegion& aRegion, nsClipCombine aCombine)
{
nsRect 			rect;
Rect			 	mrect;
RgnHandle		mregion;

  nsRegionMac *pRegion = (nsRegionMac *)&aRegion;
  
  
  mregion = pRegion->GetRegion();
  mrect = (**mClipRegion).rgnBBox;
  
  rect.x = mrect.left;
  rect.y = mrect.right;
  rect.width = mrect.right-mrect.left;
  rect.height = mrect.bottom-mrect.top;

  SetClipRectInPixels(rect, aCombine);

  if (::EmptyRgn(mClipRegion) == PR_TRUE)
    return PR_TRUE;
  else
    return PR_FALSE;
}

//------------------------------------------------------------------------

void nsRenderingContextMac :: GetClipRegion(nsIRegion **aRegion)
{
nsIRegion * pRegion ;

  static NS_DEFINE_IID(kCRegionCID, NS_REGION_CID);
  static NS_DEFINE_IID(kIRegionIID, NS_IREGION_IID);

  nsresult rv = NSRepository::CreateInstance(kCRegionCID,nsnull,  kIRegionIID, (void **)aRegion);

  if (NS_OK == rv) 
  	{
    nsRect rect;
    pRegion = (nsIRegion *)&aRegion;
    pRegion->Init();    
    this->GetClipRect(rect);
    pRegion->Union(rect.x,rect.y,rect.width,rect.height);
  	}
}

//------------------------------------------------------------------------

void nsRenderingContextMac :: SetColor(nscolor aColor)
{
RGBColor	thecolor;
 // mCurrentColor = ((nsDeviceContextMac *)mContext)->ConvertPixel(aColor);

	
	thecolor.red = NS_GET_R(aColor);
	thecolor.red = NS_GET_G(aColor);
	thecolor.red = NS_GET_B(aColor);
	::RGBForeColor(&thecolor);
  mCurrentColor = aColor ;
 
}

//------------------------------------------------------------------------

nscolor nsRenderingContextMac :: GetColor() const
{
  return mCurrentColor;
}

//------------------------------------------------------------------------

void nsRenderingContextMac :: SetFont(const nsFont& aFont)
{
/*
  NS_IF_RELEASE(mFontMetrics);
  mFontMetrics = mFontCache->GetMetricsFor(aFont);

  if (mFontMetrics)
  {  
//    mCurrFontHandle = ::XLoadFont(mRenderingSurface->display, (char *)mFontMetrics->GetFontHandle());
    mCurrFontHandle = (Font)mFontMetrics->GetFontHandle();
    
    ::XSetFont(mRenderingSurface->display,
	             mRenderingSurface->gc,
	             mCurrFontHandle);
      
//    ::XFlushGC(mRenderingSurface->display,
//	             mRenderingSurface->gc);
  }
*/
}

//------------------------------------------------------------------------

const nsFont& nsRenderingContextMac :: GetFont()
{
  return mFontMetrics->GetFont();
}

//------------------------------------------------------------------------

nsIFontMetrics* nsRenderingContextMac :: GetFontMetrics()
{
  return mFontMetrics;
}

//------------------------------------------------------------------------

// add the passed in translation to the current translation
void nsRenderingContextMac :: Translate(nscoord aX, nscoord aY)
{

  mTMatrix->AddTranslation((float)aX,(float)aY);

}

//------------------------------------------------------------------------

// add the passed in scale to the current scale
void nsRenderingContextMac :: Scale(float aSx, float aSy)
{

  mTMatrix->AddScale(aSx, aSy);

}

//------------------------------------------------------------------------

nsTransform2D * nsRenderingContextMac :: GetCurrentTransform()
{

  return mTMatrix;
}

//------------------------------------------------------------------------

nsDrawingSurface nsRenderingContextMac :: CreateDrawingSurface(nsRect *aBounds)
{
/*
  // Must make sure this code never gets called when nsRenderingSurface is nsnull
  PRUint32 depth = DefaultDepth(mRenderingSurface->display,
				DefaultScreen(mRenderingSurface->display));
  Pixmap p;
  
  if (aBounds != nsnull) 
  	{
    p  = ::XCreatePixmap(mRenderingSurface->display,
			 mRenderingSurface->drawable,
			 aBounds->width, aBounds->height, depth);
  	} 
 	else 
 		{
    p  = ::XCreatePixmap(mRenderingSurface->display,
			 mRenderingSurface->drawable,
			 2, 2, depth);
  	}

  nsDrawingSurfaceMac * surface = new nsDrawingSurfaceMac();

  surface->drawable = p ;
  surface->display  = mRenderingSurface->display;
  surface->gc       = mFrontBuffer->gc;
  surface->visual   = mRenderingSurface->visual;
  surface->depth    = mRenderingSurface->depth;

  return ((nsDrawingSurface)surface);
*/
  return nsnull;
}

//------------------------------------------------------------------------

void nsRenderingContextMac :: DestroyDrawingSurface(nsDrawingSurface aDS)
{
/*
  nsDrawingSurfaceMac * surface = (nsDrawingSurfaceMac *) aDS;

  // XXX - Could this be a GC? If so, store the type of surface in nsDrawingSurfaceMac
  ::XFreePixmap(surface->display, surface->drawable);

  //XXX greg, this seems bad. MMP
    if (mRenderingSurface == surface)
      mRenderingSurface = nsnull;

  delete aDS;
*/
}

//------------------------------------------------------------------------

void nsRenderingContextMac :: DrawLine(nscoord aX0, nscoord aY0, nscoord aX1, nscoord aY1)
{
  mTMatrix->TransformCoord(&aX0,&aY0);
  mTMatrix->TransformCoord(&aX1,&aY1);

	SetPort(mCurrentSurface);
	::SetClip(mMainRegion);
	::MoveTo(aX0, aY0);
	::LineTo(aX1, aY1);
}

//------------------------------------------------------------------------

void nsRenderingContextMac :: DrawPolyline(const nsPoint aPoints[], PRInt32 aNumPoints)
{
}

//------------------------------------------------------------------------

void nsRenderingContextMac :: DrawRect(const nsRect& aRect)
{
  DrawRect(aRect.x, aRect.y, aRect.width, aRect.height);
}

//------------------------------------------------------------------------

void nsRenderingContextMac :: DrawRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
nscoord x,y,w,h;
Rect		therect;

  x = aX;
  y = aY;
  w = aWidth;
  h = aHeight;

	SetPort(mCurrentSurface);
  mTMatrix->TransformCoord(&x,&y,&w,&h);

	SetPort(mCurrentSurface);
	::SetRect(&therect,x,y,x+w,y+h);
	::FrameRect(&therect);

}

//------------------------------------------------------------------------

void nsRenderingContextMac :: FillRect(const nsRect& aRect)
{
  FillRect(aRect.x, aRect.y, aRect.width, aRect.height);
}

//------------------------------------------------------------------------

void nsRenderingContextMac :: FillRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
nscoord x,y,w,h;
Rect		therect;

  x = aX;
  y = aY;
  w = aWidth;
  h = aHeight;

  mTMatrix->TransformCoord(&x,&y,&w,&h);
	SetPort(mCurrentSurface);
	::SetRect(&therect,aX,aY,aX+aWidth,aY+aHeight);
	::PaintRect(&therect);
  
}

//------------------------------------------------------------------------

void nsRenderingContextMac::DrawPolygon(const nsPoint aPoints[], PRInt32 aNumPoints)
{
PRUint32 		i ;
PolyHandle	thepoly;
PRInt32			x,y;

	SetPort(mCurrentSurface);
	thepoly = ::OpenPoly();
	
	x = aPoints[0].x;
	y = aPoints[0].y;
	mTMatrix->TransformCoord((PRInt32*)&x,(PRInt32*)&y);
	::MoveTo(x,y);
	for (i = 1; i < aNumPoints; i++)
		{
    x = aPoints[i].x;
    y = aPoints[i].y;
		mTMatrix->TransformCoord((PRInt32*)&x,(PRInt32*)&y);
		::LineTo(x,y);
		}
	ClosePoly();
	
	::FramePoly(thepoly);
}

//------------------------------------------------------------------------

void nsRenderingContextMac::FillPolygon(const nsPoint aPoints[], PRInt32 aNumPoints)
{
PRUint32 		i ;
PolyHandle	thepoly;
PRInt32			x,y;

	SetPort(mCurrentSurface);
	thepoly = ::OpenPoly();
	
	x = aPoints[0].x;
	y = aPoints[0].y;
	mTMatrix->TransformCoord((PRInt32*)&x,(PRInt32*)&y);
	::MoveTo(x,y);
	for (i = 1; i < aNumPoints; i++)
		{
    x = aPoints[i].x;
    y = aPoints[i].y;
		mTMatrix->TransformCoord((PRInt32*)&x,(PRInt32*)&y);
		::LineTo(x,y);
		}
	ClosePoly();
	
	::PaintPoly(thepoly);
}

//------------------------------------------------------------------------

void nsRenderingContextMac :: DrawEllipse(const nsRect& aRect)
{
  DrawEllipse(aRect.x, aRect.y, aRect.width, aRect.height);
}

//------------------------------------------------------------------------

void nsRenderingContextMac :: DrawEllipse(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
nscoord x,y,w,h;
Rect		therect;

  x = aX;
  y = aY;
  w = aWidth;
  h = aHeight;

	SetPort(mCurrentSurface);
  mTMatrix->TransformCoord(&x,&y,&w,&h);
  SetRect(&therect,x,y,x+w,x+h);
  ::FrameOval(&therect);
}

//------------------------------------------------------------------------

void nsRenderingContextMac :: FillEllipse(const nsRect& aRect)
{
  FillEllipse(aRect.x, aRect.y, aRect.width, aRect.height);
}

//------------------------------------------------------------------------

void nsRenderingContextMac :: FillEllipse(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
nscoord x,y,w,h;
Rect		therect;

  x = aX;
  y = aY;
  w = aWidth;
  h = aHeight;

	SetPort(mCurrentSurface);
  mTMatrix->TransformCoord(&x,&y,&w,&h);
  SetRect(&therect,x,y,x+w,x+h);
  ::PaintOval(&therect);
}

//------------------------------------------------------------------------

void nsRenderingContextMac :: DrawArc(const nsRect& aRect,
                                 float aStartAngle, float aEndAngle)
{
  this->DrawArc(aRect.x,aRect.y,aRect.width,aRect.height,aStartAngle,aEndAngle);
}

//------------------------------------------------------------------------

void nsRenderingContextMac :: DrawArc(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight,
                                 float aStartAngle, float aEndAngle)
{
nscoord x,y,w,h;
Rect		therect;

  x = aX;
  y = aY;
  w = aWidth;
  h = aHeight;

	SetPort(mCurrentSurface);
  mTMatrix->TransformCoord(&x,&y,&w,&h);
  SetRect(&therect,x,y,x+w,x+h);
  ::FrameArc(&therect,aStartAngle,aEndAngle);

}

//------------------------------------------------------------------------

void nsRenderingContextMac :: FillArc(const nsRect& aRect,
                                 float aStartAngle, float aEndAngle)
{
  this->FillArc(aRect.x, aRect.y, aRect.width, aRect.height, aStartAngle, aEndAngle);
}

//------------------------------------------------------------------------

void nsRenderingContextMac :: FillArc(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight,
                                 float aStartAngle, float aEndAngle)
{
nscoord x,y,w,h;
Rect		therect;

  x = aX;
  y = aY;
  w = aWidth;
  h = aHeight;

	SetPort(mCurrentSurface);
  mTMatrix->TransformCoord(&x,&y,&w,&h);
  SetRect(&therect,x,y,x+w,x+h);
  ::PaintArc(&therect,aStartAngle,aEndAngle);
}

//------------------------------------------------------------------------

void nsRenderingContextMac :: DrawString(const char *aString, PRUint32 aLength,
                                    nscoord aX, nscoord aY,
                                    nscoord aWidth)
{
PRInt32 x = aX;
PRInt32 y = aY;

	SetPort(mCurrentSurface);
  // Substract xFontStruct ascent since drawing specifies baseline
  if (mFontMetrics)
      y += mFontMetrics->GetMaxAscent();

  mTMatrix->TransformCoord(&x,&y);
	
	::MoveTo(x,y);
	DrawText(aString,0,aLength);


  if (mFontMetrics)
  {
    PRUint8 deco = mFontMetrics->GetFont().decorations;

    if (deco & NS_FONT_DECORATION_OVERLINE)
      DrawLine(aX, aY, aX + aWidth, aY);

    if (deco & NS_FONT_DECORATION_UNDERLINE)
    {
      nscoord ascent = mFontMetrics->GetMaxAscent();
      nscoord descent = mFontMetrics->GetMaxDescent();

      DrawLine(aX, aY + ascent + (descent >> 1),
               aX + aWidth, aY + ascent + (descent >> 1));
    }

    if (deco & NS_FONT_DECORATION_LINE_THROUGH)
    {
      nscoord height = mFontMetrics->GetHeight();

      DrawLine(aX, aY + (height >> 1), aX + aWidth, aY + (height >> 1));
    }
  }

}

//------------------------------------------------------------------------

void nsRenderingContextMac :: DrawString(const PRUnichar *aString, PRUint32 aLength,
                                         nscoord aX, nscoord aY, nscoord aWidth)
{

PRInt32 x = aX;
PRInt32 y = aY;

	SetPort(mCurrentSurface);
	
  // Substract xFontStruct ascent since drawing specifies baseline
  if (mFontMetrics)
      y += mFontMetrics->GetMaxAscent();

  mTMatrix->TransformCoord(&x, &y);

	::MoveTo(x,y);
	DrawText(aString,0,aLength);

  if (mFontMetrics)
  {
    PRUint8 deco = mFontMetrics->GetFont().decorations;

    if (deco & NS_FONT_DECORATION_OVERLINE)
      DrawLine(aX, aY, aX + aWidth, aY);

    if (deco & NS_FONT_DECORATION_UNDERLINE)
    {
      nscoord ascent = mFontMetrics->GetMaxAscent();
      nscoord descent = mFontMetrics->GetMaxDescent();

      DrawLine(aX, aY + ascent + (descent >> 1),
               aX + aWidth, aY + ascent + (descent >> 1));
    }

    if (deco & NS_FONT_DECORATION_LINE_THROUGH)
    {
      nscoord height = mFontMetrics->GetHeight();

      DrawLine(aX, aY + (height >> 1), aX + aWidth, aY + (height >> 1));
    }
  }

}

//------------------------------------------------------------------------

void nsRenderingContextMac :: DrawString(const nsString& aString,
                                         nscoord aX, nscoord aY, nscoord aWidth)
{
  DrawString(aString.GetUnicode(), aString.Length(), aX, aY, aWidth);
}

//------------------------------------------------------------------------

void nsRenderingContextMac :: DrawImage(nsIImage *aImage, nscoord aX, nscoord aY)
{
nscoord width,height;

  width = NSToCoordRound(mP2T * aImage->GetWidth());
  height = NSToCoordRound(mP2T * aImage->GetHeight());
  
  this->DrawImage(aImage,aX,aY,width,height);
}

//------------------------------------------------------------------------

void nsRenderingContextMac :: DrawImage(nsIImage *aImage, nscoord aX, nscoord aY,
                                        nscoord aWidth, nscoord aHeight) 
{
nsRect	tr;

  tr.x = aX;
  tr.y = aY;
  tr.width = aWidth;
  tr.height = aHeight;
  this->DrawImage(aImage,tr);
}

//------------------------------------------------------------------------

void nsRenderingContextMac :: DrawImage(nsIImage *aImage, const nsRect& aSRect, const nsRect& aDRect)
{
nsRect	sr,dr;
  
  sr = aSRect;
  mTMatrix ->TransformCoord(&sr.x,&sr.y,&sr.width,&sr.height);

  dr = aDRect;
  mTMatrix->TransformCoord(&dr.x,&dr.y,&dr.width,&dr.height);
  
  ((nsImageMac*)aImage)->Draw(*this,mRenderingSurface,sr.x,sr.y,sr.width,sr.height,
                                 dr.x,dr.y,dr.width,dr.height);
}

//------------------------------------------------------------------------

void nsRenderingContextMac :: DrawImage(nsIImage *aImage, const nsRect& aRect)
{
nsRect	tr;

  tr = aRect;
  mTMatrix->TransformCoord(&tr.x,&tr.y,&tr.width,&tr.height);
  
  if (aImage != nsnull) 
  	{
    ((nsImageMac*)aImage)->Draw(*this,mRenderingSurface,tr.x,tr.y,tr.width,tr.height);
  	} 
  else 
  	{
    printf("Image is NULL!\n");
  	}
}

//------------------------------------------------------------------------

nsresult nsRenderingContextMac :: CopyOffScreenBits(nsRect &aBounds)
{
/*

  ::XSetClipMask(mFrontBuffer->display,
		 mFrontBuffer->gc,
		 None);
  
  
  ::XCopyArea(mRenderingSurface->display, 
	      mRenderingSurface->drawable,
	      mFrontBuffer->drawable,
	      mFrontBuffer->gc,
	      aBounds.x, aBounds.y, aBounds.width, aBounds.height, 0, 0);
  
  if (nsnull != mRegion)
    ::XSetRegion(mRenderingSurface->display,
		 mRenderingSurface->gc,
		 mRegion);
*/
  return NS_OK;
}















