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

#define NEW_GS

#include "nsRenderingContextPh.h"
#include "nsRegionPh.h"
#include <math.h>
#include "libimg.h"
#include "nsDeviceContextPh.h"
#include "prprf.h"
#include "nsDrawingSurfacePh.h"
#include "nsGfxCIID.h"
#ifdef NEW_GS
#include "nsGraphicsStatePh.h"
#endif

#include <stdlib.h>
#include <mem.h>
#include <photon/PhRender.h>
#include <Pt.h>


static NS_DEFINE_IID(kIRenderingContextIID, NS_IRENDERING_CONTEXT_IID);
//static NS_DEFINE_IID(kIRenderingContextPhIID, NS_IRENDERING_CONTEXT_PH_IID);
static NS_DEFINE_IID(kIDrawingSurfaceIID, NS_IDRAWING_SURFACE_IID);
static NS_DEFINE_IID(kDrawingSurfaceCID, NS_DRAWING_SURFACE_CID);
static NS_DEFINE_CID(kRegionCID, NS_REGION_CID);


#define FLAG_CLIP_VALID       0x0001
#define FLAG_CLIP_CHANGED     0x0002
#define FLAG_LOCAL_CLIP_VALID 0x0004

#define FLAGS_ALL             (FLAG_CLIP_VALID | FLAG_CLIP_CHANGED | FLAG_LOCAL_CLIP_VALID)

int cur_color = 0;
char FillColorName[8][20] = {"Pg_BLACK","Pg_BLUE","Pg_RED","Pg_YELLOW","Pg_GREEN","Pg_MAGENTA","Pg_CYAN","Pg_WHITE"};
long FillColorVal[8] = {Pg_BLACK,Pg_BLUE,Pg_RED,Pg_YELLOW,Pg_GREEN,Pg_MAGENTA,Pg_CYAN,Pg_WHITE};

// Macro for creating a palette relative color if you have a COLORREF instead
// of the reg, green, and blue values. The color is color-matches to the nearest
// in the current logical palette. This has no effect on a non-palette device
#define PALETTERGB_COLORREF(c)  (0x02000000 | (c))

// Macro for converting from nscolor to PtColor_t
// Photon RGB values are stored as 00 RR GG BB
// nscolor RGB values are 00 BB GG RR
#define NS_TO_PH_RGB(ns) (ns & 0xff) << 16 | (ns & 0xff00) | ((ns >> 16) & 0xff)
#define PH_TO_NS_RGB(ns) (ns & 0xff) << 16 | (ns & 0xff00) | ((ns >> 16) & 0xff)

#include <prlog.h>
PRLogModuleInfo *PhGfxLog = PR_NewLogModule("PhGfxLog");
#include "nsPhGfxLog.h"

NS_IMPL_ISUPPORTS1(nsRenderingContextPh, nsIRenderingContext)


/* Global Variable for Alpha Blending */
void *Mask = nsnull;

/* The default Photon Drawing Context */
PhGC_t *nsRenderingContextPh::mPtGC = nsnull;

#define SELECT(surf) mBufferIsEmpty = PR_FALSE; if (surf->Select()) ApplyClipping(surf->GetGC());
//#define SELECT(surf) if (surf->Select()) ApplyClipping(surf->GetGC());

#ifndef NEW_GS
class GraphicsState
{
public:
  GraphicsState();
  ~GraphicsState();

  GraphicsState   *mNext;

  /* Members of nsRenderingContextPh object */

//PhGC_t            *mGC;
  nscolor		     mCurrentColor;
  nsTransform2D     *mMatrix;		// transform that all the graphics drawn here will obey
  nsIFontMetrics	*mFontMetrics;
  nsIRegion		    *mClipRegion;
};


GraphicsState :: GraphicsState()
{
  mNext = nsnull;

//  mGC = nsnull;
  mCurrentColor = NS_RGB(0, 0, 0);
  mMatrix = nsnull;
  mFontMetrics = nsnull;
  mClipRegion = nsnull;
}


GraphicsState :: ~GraphicsState()
{
}
#endif

nsRenderingContextPh :: nsRenderingContextPh()
{
  NS_INIT_REFCNT();
  
  mGC = nsnull;
  mTMatrix         = new nsTransform2D();
  mClipRegion          = nsnull ; // new nsRegionPh();
  //mClipRegion->Init();
  mFontMetrics     = nsnull;
  mSurface         = nsnull;
  mMainSurface     = nsnull;
  mDCOwner         = nsnull;
  mContext         = nsnull;
  mP2T             = 1.0f;
  mWidget          = nsnull;
  mPhotonFontName  = nsnull;
  Mask             = nsnull;
  mCurrentLineStyle       = nsLineStyle_kSolid;

  //default objects
  //state management

  mStates          = nsnull;
  mStateCache      = new nsVoidArray();
  mGammaTable      = nsnull;
  
  if( mPtGC == nsnull )
    mPtGC = PgGetGC();

  mInitialized   = PR_FALSE;
  mBufferIsEmpty = PR_TRUE;

  PushState();
}


nsRenderingContextPh :: ~nsRenderingContextPh()
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRenderingContextPh::~nsRenderingContextPh this=<%p> mGC = %p\n", this, mGC ));

#if 1
  // Destroy the State Machine
  if (mStateCache)
  {
    PRInt32 cnt = mStateCache->Count();

    while (--cnt >= 0)
    {
      PRBool  clipstate;
      PopState(clipstate);
    }

    delete mStateCache;
    mStateCache = nsnull;
  }
#else
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
#endif

  if (mTMatrix)
    delete mTMatrix;

  if (!mSurface)
  {
    if( mGC )
    {
      PgSetGC( mPtGC );
      PgSetRegion( mPtGC->rid );
      PgDestroyGC( mGC );
      mGC = nsnull;
    }
  }



  /* We always do this?? */
  PgSetGC( mPtGC );
  PgSetRegion( mPtGC->rid );

  if (mPhotonFontName)
    delete [] mPhotonFontName;

  NS_IF_RELEASE(mClipRegion);  /* do we need to do this? */

//  if (mClipRegion)
//      delete mClipRegion;

  NS_IF_RELEASE(mFontMetrics);
  NS_IF_RELEASE(mContext);
}


NS_IMETHODIMP nsRenderingContextPh :: Init(nsIDeviceContext* aContext,
                                           nsIWidget *aWindow)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRenderingContextPh::Init with a widget\n"));
  NS_PRECONDITION(PR_FALSE == mInitialized, "double init");

  nsresult res;
  
  mContext = aContext;
  NS_IF_ADDREF(mContext);

  mWidget = (PtWidget_t*) aWindow->GetNativeData( NS_NATIVE_WIDGET );

  if(!mWidget)
  {
    NS_IF_RELEASE(mContext); // new
    NS_ASSERTION(mWidget,"nsRenderingContext::Init (with a widget) mWidget is NULL!");
    return NS_ERROR_FAILURE;
  }

  PhRid_t    rid = PtWidgetRid( mWidget );
  
   PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRenderingContextPh::Init this=<%p> mWidget=<%p> rid=<%d>\n", this, mWidget, rid ));

  if (rid == 0)
  {
    PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRenderingContextPh::Init Widget (%p) does not have a Rid!\n", mWidget ));
    //NS_ASSERTION(rid, "nsRenderingContextPh::Init PtWidgetRid returned 0");
  }
  else
  {
    mGC = PgCreateGC( 4096 );
    if( !mGC )
    {
      PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRenderingContextPh::Init PgCreateGC() failed!\n" ));
    }

    NS_ASSERTION(mGC, "nsRenderingContextPh::Init PgCreateGC() failed!");
  
  PgSetGC( mGC );
  PgDefaultGC( mGC );
  PgSetRegion( rid );

//  PgSetGC( mPtGC );
//  PgSetRegion( mPtGC->rid );

  mSurface = new nsDrawingSurfacePh();
  if (mSurface)
  {
    res = mSurface->Init(mGC);
    if (res != NS_OK)
    {
      PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRenderingContextPh::Init  mSurface->Init(mGC) failed\n"));
      return NS_ERROR_FAILURE;
    }
  }
  else
  {
    NS_ASSERTION(0, "nsRenderingContextPh::Init Failed to new the mSurface");
    return NS_ERROR_FAILURE;
  }
  	
  mOffscreenSurface = mSurface;

//  NS_IF_ADDREF(aWindow);  /* took this out */
  NS_ADDREF(mSurface);
  }
  
  mInitialized = PR_TRUE;
  
  return (CommonInit());
}

NS_IMETHODIMP nsRenderingContextPh::CommonInit()
{
  float app2dev;

  if ( NS_SUCCEEDED(nsComponentManager::CreateInstance(kRegionCID, 0, NS_GET_IID(nsIRegion), (void**)&mClipRegion)) )
  {
    mClipRegion->Init();
    mClipRegion->SetTo(0, 0, 0,0);
    if (mSurface)
	{
      PRUint32 width, height;
	  mSurface->GetDimensions(&width, &height);
      mClipRegion->SetTo(0, 0, width, height);
	}
  }
  
  mContext->GetAppUnitsToDevUnits(app2dev);
  mTMatrix->AddScale(app2dev,app2dev);
  mContext->GetDevUnitsToAppUnits(mP2T);
  mContext->GetGammaTable(mGammaTable);

  return NS_OK;
}


NS_IMETHODIMP nsRenderingContextPh :: Init(nsIDeviceContext* aContext,
                                           nsDrawingSurface aSurface)
{

  printf ("nsRenderingContextPh::Init  with a surface!!!! %p\n",aSurface);

  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRenderingContextPh::Init with a Drawing Surface\n"));

  NS_PRECONDITION(PR_FALSE == mInitialized, "double init");

  mContext = aContext;
  NS_IF_ADDREF(mContext);

  mSurface = (nsDrawingSurfacePh *) aSurface;
  mOffscreenSurface=mSurface;
  NS_ADDREF(mSurface);

  mInitialized = PR_TRUE;

  return (CommonInit());
}


NS_IMETHODIMP nsRenderingContextPh :: LockDrawingSurface(PRInt32 aX, PRInt32 aY,
                                                          PRUint32 aWidth, PRUint32 aHeight,
                                                          void **aBits, PRInt32 *aStride,
                                                          PRInt32 *aWidthBytes, PRUint32 aFlags)
{
  PushState();

  return mSurface->Lock(aX, aY, aWidth, aHeight,
                        aBits, aStride, aWidthBytes, aFlags);
}

NS_IMETHODIMP nsRenderingContextPh::UnlockDrawingSurface(void)
{
  PRBool  clipstate;
  PopState(clipstate);

  mSurface->Unlock();

  return NS_OK;
}


NS_IMETHODIMP nsRenderingContextPh :: SelectOffScreenDrawingSurface(nsDrawingSurface aSurface)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRenderingContextPh::SelectOffScreenDrawingSurface this=<%p> sSurface=<%p>\n", this, aSurface));

  if (nsnull==aSurface)
  {
    PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRenderingContextPh::SelectOffScreenDrawingSurface  selecting offscreen (private)\n"));
    mSurface = mOffscreenSurface;
  }
  else
  {
    PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRenderingContextPh::SelectOffScreenDrawingSurface  selecting passed-in (%p)\n", aSurface));
    mSurface = (nsDrawingSurfacePh *) aSurface;
  }

//  printf ("kedl2: select pixmap %p\n", ((nsDrawingSurfacePh *)mSurface)->mPixmap);
  mSurface->Select();

// to clear the buffer to black to clean up transient rips during redraw....
  PgSetClipping( 0, NULL );
  PgSetMultiClip( 0, NULL );
  PgSetFillColor(FillColorVal[cur_color]);
//  PgDrawIRect( 0, 0, 1024,768, Pg_DRAW_FILL_STROKE ); 
  cur_color++;
  cur_color &= 0x7;

  mBufferIsEmpty = PR_TRUE;

//1  ApplyClipping(mSurface->GetGC());
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPh :: GetDrawingSurface(nsDrawingSurface *aSurface)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRenderingContextPh::GetDrawingSurface\n"));
//  printf ("get drawing surface! %p\n",mSurface);
  *aSurface = (void *) mSurface;
  return NS_OK;
}


NS_IMETHODIMP nsRenderingContextPh :: GetHints(PRUint32& aResult)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRenderingContextPh::GetHints\n"));

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


NS_IMETHODIMP nsRenderingContextPh :: Reset()
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRenderingContextPh::Reset  - Not Implemented\n"));
  return NS_OK;
}


NS_IMETHODIMP nsRenderingContextPh :: GetDeviceContext(nsIDeviceContext *&aContext)
{
//  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRenderingContextPh::GetDeviceContext\n"));

  NS_IF_ADDREF( mContext );
  aContext = mContext;
  return NS_OK;
}


NS_IMETHODIMP nsRenderingContextPh :: PushState(void)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRenderingContextPh::PushState\n"));

#ifdef NEW_GS
/* kirk 9/27/99 stole this from GTK */

  //  Get a new GS
#ifdef USE_GS_POOL
  nsGraphicsState *state = nsGraphicsStatePool::GetNewGS();
#else
  nsGraphicsState *state = new nsGraphicsState;
#endif

  // Push into this state object, add to vector
  if (!state)
  {
    NS_ASSERTION(0, "nsRenderingContextPh::PushState Failed to create a new Graphics State");
    return NS_ERROR_FAILURE;
  }

  state->mMatrix = mTMatrix;

  if (nsnull == mTMatrix)
    mTMatrix = new nsTransform2D();
  else
    mTMatrix = new nsTransform2D(mTMatrix);

  if (mClipRegion)
  {
    // set the state's clip region to a new copy of the current clip region
    GetClipRegion(&state->mClipRegion);
  }

  NS_IF_ADDREF(mFontMetrics);
  state->mFontMetrics = mFontMetrics;

  state->mColor = mCurrentColor;
  state->mLineStyle = mCurrentLineStyle;

  mStateCache->AppendElement(state);
#else

  GraphicsState * state = new GraphicsState();
  if (state)
  { 
    mStateCache->AppendElement(state);

    // Save current settings to new state...
    state->mCurrentColor = mCurrentColor;
    state->mMatrix = mTMatrix;
    state->mFontMetrics = mFontMetrics;
    NS_IF_ADDREF( state->mFontMetrics );
    state->mClipRegion = mClipRegion;
  
    /* if the mClipRegion is not empty make a copy */
    if (mClipRegion != nsnull)
    {
      mClipRegion = new nsRegionPh();
      if (mClipRegion)
	  {
        mClipRegion->Init();
        mClipRegion->SetTo(*state->mClipRegion);  
      }
      else
	  {
        delete state;
        return NS_ERROR_OUT_OF_MEMORY;
      }
    }
    
    // Make new objects so we dont change the saved ones...
    // Can't make a new FontMetrics since there is no copy constructor
    mTMatrix = new nsTransform2D(mTMatrix);
  }
  else
    return NS_ERROR_OUT_OF_MEMORY;
#endif
	
  return NS_OK;
}


NS_IMETHODIMP nsRenderingContextPh :: PopState( PRBool &aClipEmpty )
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRenderingContextPh::PopState\n"));

#ifdef NEW_GS
/* kirk 9/27/99 stole this code from GTK */

  PRUint32 cnt = mStateCache->Count();
  nsGraphicsState * state;

  if (cnt > 0) {
    state = (nsGraphicsState *)mStateCache->ElementAt(cnt - 1);
    mStateCache->RemoveElementAt(cnt - 1);

    // Assign all local attributes from the state object just popped
    if (mTMatrix)
      delete mTMatrix;
    mTMatrix = state->mMatrix;

    // get rid of the current clip region
    NS_IF_RELEASE(mClipRegion);
    mClipRegion = nsnull;

    // restore everything
    mClipRegion = (nsRegionPh *) state->mClipRegion;
    mFontMetrics = state->mFontMetrics;

    if (mSurface && mClipRegion)
    {
// kirk what does this do?
//      GdkRegion *rgn;
//      mClipRegion->GetNativeRegion((void*&)rgn);
//      ::gdk_gc_set_clip_region (mSurface->GetGC(), rgn);
    }

    ApplyClipping(mGC);
	  
    if (state->mColor != mCurrentColor)
      SetColor(state->mColor);

    if (state->mLineStyle != mCurrentLineStyle)
      SetLineStyle(state->mLineStyle);

    // Delete this graphics state object
#ifdef USE_GS_POOL
    nsGraphicsStatePool::ReleaseGS(state);
#else
    delete state;
#endif
  }

  if (mClipRegion)
    aClipEmpty = mClipRegion->IsEmpty();
  else
    aClipEmpty = PR_TRUE;

  return NS_OK;

#else

  PRUint32 cnt = mStateCache->Count();
  PRBool bEmpty=PR_FALSE;
//kedl ??  PRBool bEmpty=aClipEmpty;
  
  if( cnt > 0)
  {
    GraphicsState * state = (GraphicsState *)mStateCache->ElementAt(cnt - 1);

    mStateCache->RemoveElementAt(cnt - 1);

    // Assign all local attributes from the state object just popped

    if( state->mCurrentColor != mCurrentColor )
      SetColor(state->mCurrentColor);

    if( mTMatrix )
      delete mTMatrix;
    mTMatrix = state->mMatrix;

    NS_IF_RELEASE( mFontMetrics );
    mFontMetrics = state->mFontMetrics;

    if (mClipRegion)
      delete mClipRegion;
	  
    mClipRegion = state->mClipRegion;
    if ((mClipRegion) && (mClipRegion->IsEmpty() == PR_TRUE))
    {
      bEmpty = PR_TRUE;
    }

    ApplyClipping(mGC);

    // Delete this graphics state object
    delete state;
  }

  // REVISIT - fix when we get clipping working
  aClipEmpty = bEmpty;

  return NS_OK;
#endif

}


NS_IMETHODIMP nsRenderingContextPh :: IsVisibleRect(const nsRect& aRect, PRBool &aVisible)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRenderingContextPh::IsVisibleRect - Not Implemented\n"));
  aVisible = PR_TRUE;
  return NS_OK;
}


NS_IMETHODIMP nsRenderingContextPh :: SetClipRect(const nsRect& aRect, nsClipCombine aCombine, PRBool &aClipEmpty)
{
  nsresult   res = NS_ERROR_FAILURE;
  nsRect     trect = aRect;
  PhRect_t  *rgn;

  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRenderingContextPh::SetClipRect  (%ld,%ld,%ld,%ld)\n", aRect.x, aRect.y, aRect.width, aRect.height ));

  if ((mTMatrix) && (mClipRegion))
  {
    PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("  prev clip empty = %i\n", mClipRegion->IsEmpty()));

    mTMatrix->TransformCoord(&trect.x, &trect.y,&trect.width, &trect.height);

    switch(aCombine)
    {
      case nsClipCombine_kIntersect:
   	PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("  combine type = intersect\n"));
        mClipRegion->Intersect(trect.x,trect.y,trect.width,trect.height);
        break;
      case nsClipCombine_kUnion:
   	PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("  combine type = union\n"));
        mClipRegion->Union(trect.x,trect.y,trect.width,trect.height);
        break;
      case nsClipCombine_kSubtract:
   	PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("  combine type = subtract\n"));
        mClipRegion->Subtract(trect.x,trect.y,trect.width,trect.height);
        break;
      case nsClipCombine_kReplace:
   	PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("  combine type = replace\n"));
        mClipRegion->SetTo(trect.x,trect.y,trect.width,trect.height);
        break;
      default:
   	PR_LOG(PhGfxLog, PR_LOG_ERROR, ("nsRenderingContextPh::SetClipRect  Unknown Combine type\n"));
        break;
    }

    aClipEmpty = mClipRegion->IsEmpty();
    PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("  new clip empty = %i\n", aClipEmpty ));

    ApplyClipping(mGC);

// kirk    mClipRegion->GetNativeRegion((void*&)rgn);
// kirk    PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRenderingContextPh::SetClipRect Calling PgSetCliping (%ld,%ld,%ld,%ld)\n", rgn->ul.x, rgn->ul.y, rgn->lr.x, rgn->lr.y));
// kirk    PgSetClipping(1, rgn);
	 
    res = NS_OK;
  }
  else
  {
    printf ("no region....\n");
    PR_LOG(PhGfxLog, PR_LOG_ERROR, ("nsRenderingContextPh::SetClipRect  Invalid pointers!\n"));
  }
  																			
  return res;
}

NS_IMETHODIMP nsRenderingContextPh :: GetClipRect(nsRect &aRect, PRBool &aClipValid)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRenderingContextPh::GetClipRect\n"));
  PRInt32 x, y, w, h;

  if (!mClipRegion->IsEmpty())
  {
    mClipRegion->GetBoundingBox(&x,&y,&w,&h);
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


NS_IMETHODIMP nsRenderingContextPh :: SetClipRegion(const nsIRegion& aRegion, nsClipCombine aCombine, PRBool &aClipEmpty)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRenderingContextPh::SetClipRegion\n"));

  switch(aCombine)
  {
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
  ApplyClipping(mGC);

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPh :: CopyClipRegion(nsIRegion &aRegion)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRenderingContextPh::CopyClipRegion  - Not Implemented\n"));
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsRenderingContextPh :: GetClipRegion(nsIRegion **aRegion)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRenderingContextPh::GetClipRegion\n"));

#ifdef NEW_GS
/* kirk 9/27/99 stole this from GTK */
  nsresult rv = NS_ERROR_FAILURE;

  if (!aRegion)
    return NS_ERROR_NULL_POINTER;

  if (*aRegion) // copy it, they should be using CopyClipRegion
  {
    // printf("you should be calling CopyClipRegion()\n");
    (*aRegion)->SetTo(*mClipRegion);
    rv = NS_OK;
  }
  else
  {
    if ( NS_SUCCEEDED(nsComponentManager::CreateInstance(kRegionCID, 0, NS_GET_IID(nsIRegion), 
                                                         (void**)aRegion )) )
    {
      if (mClipRegion)
      {
        (*aRegion)->Init();
        (*aRegion)->SetTo(*mClipRegion);
        NS_ADDREF(*aRegion);
        rv = NS_OK;
      }
      else
      {
        printf("null clip region, can't make a valid copy\n");
        NS_RELEASE(*aRegion);
        rv = NS_ERROR_FAILURE;
      }
    } 
  }

  return rv;
#else

  nsresult  rv = NS_OK;
  
  NS_ASSERTION(!(nsnull == aRegion), "no region ptr");

  if (nsnull == *aRegion)
  {
   nsRegionPh *rgn = new nsRegionPh();
   if (nsnull != rgn)
   {
     NS_ADDREF(rgn);
     rv = rgn->Init();
     if (NS_OK == rv)
       *aRegion = rgn;
     else
       NS_RELEASE(rgn);
   }
   else
   {
     rv = NS_ERROR_OUT_OF_MEMORY;
   }
  }

  if (rv == NS_OK)
  {
    (*aRegion)->SetTo(*mClipRegion);
  }																				  

  return rv;
#endif
}


NS_IMETHODIMP nsRenderingContextPh :: SetColor(nscolor aColor)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRenderingContextPh::SetColor (%i,%i,%i)\n", NS_GET_R(aColor), NS_GET_G(aColor), NS_GET_B(aColor) ));

  mCurrentColor = aColor;
  PgSetStrokeColor( NS_TO_PH_RGB( aColor ));
  PgSetFillColor( NS_TO_PH_RGB( aColor ));
  PgSetTextColor( NS_TO_PH_RGB( aColor ));

  return NS_OK;
}


NS_IMETHODIMP nsRenderingContextPh :: GetColor(nscolor &aColor) const
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRenderingContextPh::GetColor\n"));
  aColor = mCurrentColor;
  return NS_OK;
}


NS_IMETHODIMP nsRenderingContextPh :: SetLineStyle(nsLineStyle aLineStyle)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRenderingContextPh::SetLineStyle\n"));
  mCurrentLineStyle = aLineStyle;
  return NS_OK;
}


NS_IMETHODIMP nsRenderingContextPh :: GetLineStyle(nsLineStyle &aLineStyle)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRenderingContextPh::GetLineStyle  - Not Implemented\n"));
  aLineStyle = mCurrentLineStyle;
  return NS_OK;
}


NS_IMETHODIMP nsRenderingContextPh :: SetFont(const nsFont& aFont)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRenderingContextPh::SetFont with nsFont\n"));

  if (mFontMetrics)
    NS_IF_RELEASE(mFontMetrics);

  if (mContext)
  {
    mContext->GetMetricsFor(aFont, mFontMetrics);
    return SetFont(mFontMetrics);
  }
  else
    return NS_ERROR_FAILURE;
}


NS_IMETHODIMP nsRenderingContextPh :: SetFont(nsIFontMetrics *aFontMetrics)
{
//  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRenderingContextPh::SetFont with nsIFontMetrics mFontMetrics=<%p> aFontMetrics=<%p>\n", mFontMetrics, aFontMetrics));
	  
  nsFontHandle  fontHandle;			/* really a nsString */
  nsString      *pFontHandle;

  if (mFontMetrics)
    NS_IF_RELEASE(mFontMetrics);

//PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRenderingContextPh::SetFont after NS_IF_RELEASE(mFontMetrics)\n"));

  mFontMetrics = aFontMetrics;
  NS_IF_ADDREF(mFontMetrics);

//PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRenderingContextPh::SetFont after NS_IF_ADDREF(mFontMetrics)\n"));
  
  mFontMetrics->GetFontHandle(fontHandle);

//PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRenderingContextPh::SetFont after GetFontHandle <%p>\n", fontHandle));

  pFontHandle = (nsString *) fontHandle;
    
  if (pFontHandle)
  {  
    if( mPhotonFontName )
      delete [] mPhotonFontName;

    mPhotonFontName = pFontHandle->ToNewCString();

	/* Cache the Font metrics locally, costs ~1400 bytes per font */
    PfLoadMetrics( mPhotonFontName );

    PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRenderingContextPh::SetFont with nsIFontMetrics Photon Font Name is <%s>\n", mPhotonFontName));

    PgSetFont( mPhotonFontName );
  }
  else
  {
    PR_LOG(PhGfxLog, PR_LOG_ERROR, ("nsRenderingContextPh::SetFont with nsIFontMetrics, INVALID Font Handle\n"));
  }
	
  return NS_OK;
}


NS_IMETHODIMP nsRenderingContextPh :: GetFontMetrics(nsIFontMetrics *&aFontMetrics)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRenderingContextPh::GetFontMetrics mFontMetrics=<%p>\n", mFontMetrics));

  NS_IF_ADDREF(mFontMetrics);
  aFontMetrics = mFontMetrics;
  return NS_OK;
}


// add the passed in translation to the current translation
NS_IMETHODIMP nsRenderingContextPh :: Translate(nscoord aX, nscoord aY)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRenderingContextPh::Translate (%i,%i)\n", aX, aY));

//  printf("nsRenderingContextPh::Translate (%i,%i)\n", aX, aY);

/*
  PtArg_t arg;
  PhPoint_t *pos;

  PtSetArg(&arg,Pt_ARG_POS,&pos,0);
  PtGetResources(mWidget,1,&arg);
*/

//printf ("translate widget: %p %d %d\n",mWidget,pos->x,pos->y);
//aX += pos->x*15;
//aY += pos->y*15;
  mTMatrix->AddTranslation((float)aX,(float)aY);
  return NS_OK;
}


// add the passed in scale to the current scale
NS_IMETHODIMP nsRenderingContextPh :: Scale(float aSx, float aSy)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRenderingContextPh::Scale (%f,%f)\n", aSx, aSy ));
  mTMatrix->AddScale(aSx, aSy);
  return NS_OK;
}


NS_IMETHODIMP nsRenderingContextPh :: GetCurrentTransform(nsTransform2D *&aTransform)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRenderingContextPh::GetCurrentTransform\n"));
  aTransform = mTMatrix;
  return NS_OK;
}


NS_IMETHODIMP nsRenderingContextPh :: CreateDrawingSurface(nsRect *aBounds, PRUint32 aSurfFlags, nsDrawingSurface &aSurface)
{
// REVISIT; what are the flags???

  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRenderingContextPh::CreateDrawingSurface\n"));

  if (nsnull==mSurface) {
    aSurface = nsnull;
    PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("  mSurface is NULL - failure!\n"));
    return NS_ERROR_FAILURE;
  }

 nsDrawingSurfacePh *surf = new nsDrawingSurfacePh();
//printf ("create2: %p %d\n",surf,aSurfFlags);

 if (surf)
 {
   NS_ADDREF(surf);
   surf->Init(mSurface->GetGC(), aBounds->width, aBounds->height, aSurfFlags);
//   surf->Init(mGC, aBounds->width, aBounds->height, aSurfFlags);
//2   ApplyClipping(mSurface->GetGC());
 }

  aSurface = (nsDrawingSurface)surf;

  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("  new surface = %p\n", aSurface));

  return NS_OK;
}


NS_IMETHODIMP nsRenderingContextPh :: DestroyDrawingSurface(nsDrawingSurface aDS)
{
  PhImage_t *image;
  void *gc;

   PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRenderingContextPh::DestroyDrawingSurface - Not Implemented\n"));

   nsDrawingSurfacePh *surf = (nsDrawingSurfacePh *) aDS;
   NS_IF_RELEASE(surf);

   return NS_OK;
}


NS_IMETHODIMP nsRenderingContextPh :: DrawLine(nscoord aX0, nscoord aY0, nscoord aX1, nscoord aY1)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRenderingContextPh::DrawLine (%ld,%ld,%ld,%ld)\n", aX0, aY0, aX1, aY1 ));

  if( nsLineStyle_kNone == mCurrentLineStyle )
    return NS_OK;

  nscoord x0,y0,x1,y1;

  x0 = aX0;
  y0 = aY0;
  x1 = aX1;
  y1 = aY1;

  mTMatrix->TransformCoord(&x0,&y0);
  mTMatrix->TransformCoord(&x1,&y1);

  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRenderingContextPh::DrawLine (%ld,%ld,%ld,%ld)\n", x0, y0, x1, y1 ));

  SELECT(mSurface);
  SetPhLineStyle();
  PgDrawILine( x0, y0, x1, y1 );

  return NS_OK;
}


NS_IMETHODIMP nsRenderingContextPh :: DrawPolyline(const nsPoint aPoints[], PRInt32 aNumPoints)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("untested nsRenderingContextPh::DrawPolyLine\n"));

  if( nsLineStyle_kNone == mCurrentLineStyle )
    return NS_OK;

  PhPoint_t *pts;

  if(( pts = new PhPoint_t [aNumPoints] ) != NULL )
  {
    PhPoint_t pos = {0,0};
    PRInt32 i;

    for(i=0;i<aNumPoints;i++)
    {
    int x,y;
      x = aPoints[i].x;
      y = aPoints[i].y;
      mTMatrix->TransformCoord(&x,&y);
      pts[i].x = x;
      pts[i].y = y;
    }

    SELECT(mSurface);
    SetPhLineStyle();
    PgDrawPolygon( pts, aNumPoints, &pos, Pg_DRAW_STROKE );

    delete [] pts;
  }
  return NS_OK;
}


NS_IMETHODIMP nsRenderingContextPh :: DrawRect(const nsRect& aRect)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("untested nsRenderingContextPh::DrawRect 1 \n"));

  DrawRect( aRect.x, aRect.y, aRect.width, aRect.height );

  return NS_OK;
}


NS_IMETHODIMP nsRenderingContextPh :: DrawRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("untested nsRenderingContextPh::DrawRect 2 \n"));

  nscoord x,y,w,h;

  x = aX;
  y = aY;
  w = aWidth;
  h = aHeight;
  mTMatrix->TransformCoord(&x,&y,&w,&h);

  SELECT(mSurface);
  PgDrawIRect( x, y, x + w - 1, y + h - 1, Pg_DRAW_STROKE );

  return NS_OK;
}


NS_IMETHODIMP nsRenderingContextPh :: FillRect(const nsRect& aRect)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRenderingContextPh::FillRect 1 (%i,%i,%i,%i)\n", aRect.x, aRect.y, aRect.width, aRect.height ));

  FillRect( aRect.x, aRect.y, aRect.width, aRect.height );

  return NS_OK;
}


NS_IMETHODIMP nsRenderingContextPh :: FillRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRenderingContextPh::FillRect 2 (%i,%i,%i,%i)\n", aX, aY, aWidth, aHeight ));
  nscoord x,y,w,h;

  x = aX;
  y = aY;
  w = aWidth;
  h = aHeight;

  mTMatrix->TransformCoord(&x,&y,&w,&h);
  SELECT(mSurface);
  PgDrawIRect( x, y, x + w - 1, y + h - 1, Pg_DRAW_FILL_STROKE );

  return NS_OK;
}

NS_IMETHODIMP 
nsRenderingContextPh :: InvertRect(const nsRect& aRect)
{
  InvertRect( aRect.x, aRect.y, aRect.width, aRect.height );

  return NS_OK;
}

// kedl,july 21, 1999
// looks like we crashe on test12 when u try to select; but so does linux
// and windows rips on test12.... yippeeeee; otherwise we look great!
NS_IMETHODIMP 
nsRenderingContextPh :: InvertRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
  nscoord x,y,w,h;

  x = aX;
  y = aY;
  w = aWidth;
  h = aHeight;

  mTMatrix->TransformCoord(&x,&y,&w,&h);
  SELECT(mSurface);
//  printf ("invert rect: %d %d %d %d\n",x,y,w,h);

  PgSetFillColor(Pg_INVERT_COLOR);
  PgSetDrawMode(Pg_DRAWMODE_XOR);
  PgDrawIRect( x, y, x + w - 1, y + h - 1, Pg_DRAW_FILL );
  PgSetDrawMode(Pg_DRAWMODE_OPAQUE);

//good  mSurface->XOR(x,y,w,h);

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPh :: DrawPolygon(const nsPoint aPoints[], PRInt32 aNumPoints)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("untested nsRenderingContextPh::DrawPolygon\n"));

  PhPoint_t *pts;

  if(( pts = new PhPoint_t [aNumPoints] ) != NULL )
  {
    PhPoint_t pos = {0,0};
    PRInt32 i;

    for(i=0;i<aNumPoints;i++)
    {
    int x,y;
      x = aPoints[i].x;
      y = aPoints[i].y;
      mTMatrix->TransformCoord(&x,&y);
      pts[i].x = x;
      pts[i].y = y;
    }

    SELECT(mSurface);
    PgDrawPolygon( pts, aNumPoints, &pos, Pg_DRAW_STROKE | Pg_CLOSED );

    delete [] pts;
  }
  return NS_OK;
}


NS_IMETHODIMP nsRenderingContextPh :: FillPolygon(const nsPoint aPoints[], PRInt32 aNumPoints)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("untested nsRenderingContextPh::FillPolygon\n"));

  PhPoint_t *pts;

  if(( pts = new PhPoint_t [aNumPoints] ) != NULL )
  {
    PhPoint_t pos = {0,0};
    PRInt32 i;

    for(i=0;i<aNumPoints;i++)
    {
    int x,y;
      x = aPoints[i].x;
      y = aPoints[i].y;
      mTMatrix->TransformCoord(&x,&y);
      pts[i].x = x;
      pts[i].y = y;
    }

    SELECT(mSurface);
    PgDrawPolygon( pts, aNumPoints, &pos, Pg_DRAW_FILL_STROKE | Pg_CLOSED );

    delete [] pts;
  }
  return NS_OK;
}


NS_IMETHODIMP nsRenderingContextPh :: DrawEllipse(const nsRect& aRect)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRenderingContextPh::DrawEllipse.\n"));

  DrawEllipse( aRect.x, aRect.y, aRect.width, aRect.height );

  return NS_OK;
}


NS_IMETHODIMP nsRenderingContextPh :: DrawEllipse(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRenderingContextPh::DrawEllipse.\n"));
  nscoord x,y,w,h;
  PhPoint_t center;
  PhPoint_t radii;
  unsigned int flags;

  x = aX;
  y = aY;
  w = aWidth;
  h = aHeight;

  mTMatrix->TransformCoord(&x,&y,&w,&h);

  center.x = x;
  center.y = y;
  radii.x = x+w-1;
  radii.y = y+h-1;
  flags = Pg_EXTENT_BASED | Pg_DRAW_STROKE;
  SELECT(mSurface);
  PgDrawEllipse( &center, &radii, flags );

  return NS_OK;
}


NS_IMETHODIMP nsRenderingContextPh :: FillEllipse(const nsRect& aRect)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRenderingContextPh::FillEllipse.\n"));

  FillEllipse( aRect.x, aRect.y, aRect.width, aRect.height );

  return NS_OK;
}


NS_IMETHODIMP nsRenderingContextPh :: FillEllipse(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRenderingContextPh::FillEllipse.\n"));
  nscoord x,y,w,h;
  PhPoint_t center;
  PhPoint_t radii;
  unsigned int flags;

  x = aX;
  y = aY;
  w = aWidth;
  h = aHeight;

  mTMatrix->TransformCoord(&x,&y,&w,&h);

  center.x = x;
  center.y = y;
  radii.x = x+w-1;
  radii.y = y+h-1;
  flags = Pg_EXTENT_BASED | Pg_DRAW_FILL_STROKE;
  SELECT(mSurface);
  PgDrawEllipse( &center, &radii, flags );

  return NS_OK;
}


NS_IMETHODIMP nsRenderingContextPh :: DrawArc(const nsRect& aRect,
                                 float aStartAngle, float aEndAngle)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRenderingContextPh::DrawArc - Not implemented.\n"));

  return NS_OK;
}


NS_IMETHODIMP nsRenderingContextPh :: DrawArc(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight,
                                 float aStartAngle, float aEndAngle)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRenderingContextPh::DrawArc - Not implemented.\n"));

  return NS_OK;
}


NS_IMETHODIMP nsRenderingContextPh :: FillArc(const nsRect& aRect,
                                 float aStartAngle, float aEndAngle)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRenderingContextPh::FillArc - Not implemented.\n"));

  return NS_OK;
}


NS_IMETHODIMP nsRenderingContextPh :: FillArc(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight,
                                 float aStartAngle, float aEndAngle)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRenderingContextPh::FillArc - Not implemented.\n"));

  return NS_OK;
}


NS_IMETHODIMP nsRenderingContextPh :: GetWidth(char ch, nscoord& aWidth)
{
  char buf[2];
  nsresult ret_code;

  /* turn it into a string */
  buf[0] = ch;
  buf[1] = nsnull;

  ret_code = GetWidth(buf, 1, aWidth);  
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRenderingContextPh::GetWidth1 for <%c> aWidth=<%d> ret_code=<%d>\n", ch, aWidth, ret_code));

  return ret_code;
}


NS_IMETHODIMP nsRenderingContextPh :: GetWidth(PRUnichar ch, nscoord &aWidth, PRInt32 *aFontID)
{
  PRUnichar buf[2];
  nsresult ret_code;

  /* turn it into a string */
  buf[0] = ch;
  buf[1] = nsnull;

  ret_code = GetWidth(buf, 1, aWidth, aFontID);  
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRenderingContextPh::GetWidth2 for <%c> aWidth=<%d> aFontId=<%p> ret_code=<%d>\n", (char) ch, aWidth, aFontID, ret_code));
  return ret_code;
}


NS_IMETHODIMP nsRenderingContextPh :: GetWidth(const char* aString, nscoord& aWidth)
{
  nsresult ret_code;

  ret_code = GetWidth(aString, strlen(aString), aWidth);  
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRenderingContextPh::GetWidth3 for <%s> aWidth=<%d> ret_code=<%d>\n", aString, aWidth, ret_code));

  return ret_code;
}


NS_IMETHODIMP nsRenderingContextPh :: GetWidth(const char* aString,
                                                PRUint32 aLength,
                                                nscoord& aWidth)
{
  nsresult ret_code = NS_ERROR_FAILURE;
  
  aWidth = 0;	// Initialize to zero in case we fail.

  if (nsnull != mFontMetrics)
  {
    PhRect_t      extent;
	
    if (PfExtentText(&extent, NULL, mPhotonFontName, aString, aLength))
    {
      aWidth = (int) ((extent.lr.x - extent.ul.x + 1) * mP2T);

      ret_code = NS_OK;
    }
  }
  else
  {
    PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRenderingContextPh::GetWidth4 FAILED = a NULL mFontMetrics detected\n"));
    ret_code = NS_ERROR_FAILURE;
  }  

  return ret_code;
}


NS_IMETHODIMP nsRenderingContextPh :: GetWidth(const nsString& aString, nscoord& aWidth, PRInt32 *aFontID)
{
#if 0
  /* DEBUG ONLY */
  char *str = aString.ToNewCString();
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRenderingContextPh::GetWidth5 aString=<%s>\n", str));
  delete [] str;
#endif

  nsresult ret_code;

  ret_code = GetWidth(aString.GetUnicode(), aString.Length(), aWidth, aFontID);  

  /* What the heck? I copied this from Windows */
  if (nsnull != aFontID)
    *aFontID = 0;

  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRenderingContextPh::GetWidth5  aWidth=<%d> ret_code=<%d>\n", aWidth, ret_code));
  return ret_code;
}


NS_IMETHODIMP nsRenderingContextPh :: GetWidth(const PRUnichar *aString,
                                                PRUint32 aLength,
                                                nscoord &aWidth,
                                                PRInt32 *aFontID)
{
  nsresult ret_code = NS_ERROR_FAILURE;
  nscoord photonWidth;
  
  aWidth = 0;	// Initialize to zero in case we fail.

  if (nsnull != mFontMetrics)
  {
    PhRect_t      extent;
//    nsFontHandle  fontHandle;			/* really a (nsString  *) */
//    nsString      *pFontHandle = nsnull;
//    char          *PhotonFontName =  nsnull;

//    mFontMetrics->GetFontHandle(fontHandle);
//    pFontHandle = (nsString *) fontHandle;
//    PhotonFontName =  pFontHandle->ToNewCString();
	
    if (PfExtentWideText(&extent, NULL, mPhotonFontName, (wchar_t *) aString, (aLength*2)))
    {
//	  photonWidth = (extent.lr.x - extent.ul.x + 1);
// 	  aWidth = (int) ((float) photonWidth * mP2T);
      aWidth = (int) ((extent.lr.x - extent.ul.x + 1) * mP2T);
     
//      PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRenderingContextPh::GetWidth4 PhotonWidth=<%d> aWidth=<%d> PhotonFontName=<%s>\n",photonWidth, aWidth, PhotonFontName));

      ret_code = NS_OK;
//	  delete [] PhotonFontName;
    }
  }
  else
  {
    PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRenderingContextPh::GetWidth6 FAILED = a NULL mFontMetrics detected\n"));
    ret_code = NS_ERROR_FAILURE;
  }  

  if (nsnull != aFontID)
  {
    *aFontID = 0;
  }
  	

  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRenderingContextPh::GetWidth6  aLength=<%d> aWidth=<%d> ret_code=<%d>\n", aLength, aWidth, ret_code));

  return ret_code;
}


NS_IMETHODIMP nsRenderingContextPh :: DrawString(const char *aString, PRUint32 aLength,
                                                  nscoord aX, nscoord aY,
                                                  const nscoord* aSpacing)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRenderingContextPh::DrawString1 first aString=<%s> of %d at (%d,%d) aSpacing=<%p>.\n",aString, aLength, aX, aY, aSpacing));

  int err;
  
  nscoord x = aX;
  nscoord y = aY;

  if (nsnull != aSpacing)
  {
/* REVISIT this code will break with an ACTUAL multi-byte multi-byte string */
    // Render the string, one character at a time...
    const char* end = aString + aLength;
	while (aString < end)
	{
	  char ch = *aString++;
      nscoord xx = x;
	  nscoord yy = y;
	  mTMatrix->TransformCoord(&xx, &yy);
      PhPoint_t pos = { xx, yy };
      SELECT(mSurface);
      PgDrawText( &ch, 1, &pos, (Pg_TEXT_LEFT | Pg_TEXT_TOP));
      x += *aSpacing++;
    }
  }
  else
  {
    mTMatrix->TransformCoord(&x,&y);
    PhPoint_t pos = { x, y };
 	
    SELECT(mSurface);

  /* HACK to see if we have a clipping problem */
  //PgSetClipping(0,NULL);

#if 1
    err=PgDrawTextChars( aString, aLength, &pos, (Pg_TEXT_LEFT | Pg_TEXT_TOP));
#else
	/* This is garbage and doesn't work */
    int char_count, byte_count; 
    char *new_str;
    byte_count = mbstrnlen(aString, aLength, 0, &char_count);
	printf("nsRenderingContextPh::DrawString1 aLength=<%d> char_count=<%d> byte_count=<%d>\n", aLength, char_count, byte_count);
    new_str = malloc(byte_count+1);
	memcpy(new_str, aString, byte_count+1);
    err=PgDrawTextmx( new_str, byte_count, &pos, (Pg_TEXT_LEFT | Pg_TEXT_TOP));
#endif

    if ( err == -1)
	{
	  printf("nsRenderingContextPh::DrawString1 returned error code\n");
	}
  }

  return NS_OK;
}


NS_IMETHODIMP nsRenderingContextPh :: DrawString(const PRUnichar *aString, PRUint32 aLength,
                                                  nscoord aX, nscoord aY,
                                                  PRInt32 aFontID,
                                                  const nscoord* aSpacing)
{
  const int BUFFER_SIZE = (aLength * 3);
  char buffer[BUFFER_SIZE];
  int len;
  
  len = wcstombs(buffer, (wchar_t *) aString, BUFFER_SIZE);
  return DrawString( (char *) buffer, aLength, aX, aY, aSpacing);
}


NS_IMETHODIMP nsRenderingContextPh :: DrawString(const nsString& aString,
                                                  nscoord aX, nscoord aY,
                                                  PRInt32 aFontID,
                                                  const nscoord* aSpacing)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRenderingContextPh::DrawString3 at (%d,%d) aSpacing=<%p>.\n", aX, aY, aSpacing));

  return DrawString(aString.GetUnicode(), aString.Length(),
                      aX, aY, aFontID, aSpacing);
}


NS_IMETHODIMP nsRenderingContextPh :: DrawImage(nsIImage *aImage, nscoord aX, nscoord aY)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRenderingContextPh::DrawImage1\n"));

  nsresult res;
  nscoord x,y,w,h;

  x = aX;
  y = aY;
  w = NSToCoordRound( mP2T * aImage->GetWidth());
  h = NSToCoordRound( mP2T * aImage->GetHeight());

  mTMatrix->TransformCoord(&x,&y,&w,&h);
  SELECT(mSurface);
  res = aImage->Draw( *this, mSurface, x, y, w, h );

  Mask = aImage->GetAlphaBits();
  return res;
}


NS_IMETHODIMP nsRenderingContextPh :: DrawImage(nsIImage *aImage, nscoord aX, nscoord aY,
                                        nscoord aWidth, nscoord aHeight) 
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRenderingContextPh::DrawImage2\n"));

  nsresult res;
  nscoord x,y,w,h;

  x = aX;
  y = aY;
  w = aWidth;
  h = aHeight;

  mTMatrix->TransformCoord(&x,&y,&w,&h);

  SELECT(mSurface);
  res = aImage->Draw( *this, mSurface, x, y, w, h );
  Mask = aImage->GetAlphaBits();

  return res;
}


NS_IMETHODIMP nsRenderingContextPh :: DrawImage(nsIImage *aImage, const nsRect& aSRect, const nsRect& aDRect)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRenderingContextPh::DrawImage3\n"));

  nsresult res;
  nsRect	sr,dr;

  sr = aSRect;
  mTMatrix->TransformCoord(&sr.x,&sr.y,&sr.width,&sr.height);

  dr = aDRect;
  mTMatrix->TransformCoord(&dr.x,&dr.y,&dr.width,&dr.height);

  SELECT(mSurface);
  res = aImage->Draw(*this,mSurface,sr.x,sr.y,sr.width,sr.height, dr.x,dr.y,dr.width,dr.height);
  Mask = aImage->GetAlphaBits();

  return res;
}


NS_IMETHODIMP nsRenderingContextPh :: DrawImage(nsIImage *aImage, const nsRect& aRect)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRenderingContextPh::DrawImage4\n"));

  nsresult res;
  nsRect	tr;

  tr = aRect;
  mTMatrix->TransformCoord(&tr.x,&tr.y,&tr.width,&tr.height);

  SELECT(mSurface);
  res = aImage->Draw(*this,mSurface,tr.x,tr.y,tr.width,tr.height);
  Mask = aImage->GetAlphaBits();

  return res;
}

static int count=0;

NS_IMETHODIMP nsRenderingContextPh :: CopyOffScreenBits(nsDrawingSurface aSrcSurf,
                                                         PRInt32 aSrcX, PRInt32 aSrcY,
                                                         const nsRect &aDestBounds,
                                                         PRUint32 aCopyFlags)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRenderingContextPh::CopyOffScreenBits this=<%p> aSrcSurf=<%p> aSrcPt=(%d,%d) aCopyFlags=<%d> DestRect=<%d,%d,%d,%d>\n",
     this, aSrcSurf, aSrcX, aSrcY, aCopyFlags, aDestBounds.x, aDestBounds.y, aDestBounds.width, aDestBounds.height));

printf("nsRenderingContextPh::CopyOffScreenBits 0\n");

  PhArea_t              area;
  PRInt32               srcX = aSrcX;
  PRInt32               srcY = aSrcY;
  nsRect                drect = aDestBounds;
  nsDrawingSurfacePh    *destsurf;

  if ( (aSrcSurf==NULL) || (mTMatrix==NULL) || (mSurface==NULL))
  {
    NS_ASSERTION(0, "nsRenderingContextPh::CopyOffScreenBits Started with NULL pointer");
	printf("nsRenderingContextPh::CopyOffScreenBits Started with NULL pointer\n");
    return NS_ERROR_FAILURE;  
  }
  
  PhGC_t *saveGC = PgGetGC();

  if (aCopyFlags & NS_COPYBITS_TO_BACK_BUFFER)
  {
    NS_ASSERTION(!(nsnull == mSurface), "no back buffer");
    destsurf = mSurface;
  }
  else
    destsurf = mOffscreenSurface;

  if( mBufferIsEmpty )
  {
    PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRenderingContextPh::CopyOffScreenBits Buffer empty, skipping.\n"));
    printf("nsRenderingContextPh::CopyOffScreenBits Buffer empty, skipping.\n");

    SELECT( destsurf );
    PgSetGC(saveGC);

    return NS_OK;
  }

  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("  flags=%X\n", aCopyFlags ));

#if 1
  printf("nsRenderingContextPh::CopyOffScreenBits() flags=\n");

  if (aCopyFlags & NS_COPYBITS_USE_SOURCE_CLIP_REGION)
    printf("NS_COPYBITS_USE_SOURCE_CLIP_REGION\n");

  if (aCopyFlags & NS_COPYBITS_XFORM_SOURCE_VALUES)
    printf("NS_COPYBITS_XFORM_SOURCE_VALUES\n");

  if (aCopyFlags & NS_COPYBITS_XFORM_DEST_VALUES)
    printf("NS_COPYBITS_XFORM_DEST_VALUES\n");

  if (aCopyFlags & NS_COPYBITS_TO_BACK_BUFFER)
    printf("NS_COPYBITS_TO_BACK_BUFFER\n");

  printf("\n");
#endif

  if (aCopyFlags & NS_COPYBITS_XFORM_SOURCE_VALUES)
    mTMatrix->TransformCoord(&srcX, &srcY);

  if (aCopyFlags & NS_COPYBITS_XFORM_DEST_VALUES)
    mTMatrix->TransformCoord(&drect.x, &drect.y, &drect.width, &drect.height);

  area.pos.x=drect.x;
  area.pos.y=drect.y;
  area.size.w=drect.width;
  area.size.h=drect.height;

  printf ("nsRenderingContextPh::CopyOffScreenBits 1 CopyFlags=<%d>, SrcSurf=<%p> DestSurf=<%p> Src=(%d,%d) Area=(%d,%d,%d,%d)\n",
    aCopyFlags,aSrcSurf,destsurf,srcX,srcY,area.pos.x,area.pos.y,area.size.w,area.size.h);

  nsRect rect;
  PRBool valid;
  GetClipRect(rect,valid);

#if 0
/* this is shit */
  if (valid)
  {
    printf ("nsRenderingContextPh::CopyOffScreenBits clip rect=<%d,%d,%d,%d>\n",rect.x,rect.y,rect.width,rect.height);
    area.size.w = rect.width; 
    area.size.h = rect.height; 
  }
#endif
  
  ((nsDrawingSurfacePh *)aSrcSurf)->Stop();
  PhImage_t *image;
  image = ((nsDrawingSurfacePh *)aSrcSurf)->mPixmap;
  SELECT(destsurf);

if (aSrcSurf==destsurf)
{
  if (image==0)
  {
	printf ("nsRenderingContextPh::CopyOffScreenBits: Unsupported onscreen to onscreen copy!!\n");
  }
  else
  {
    PhPoint_t pos = { area.pos.x,area.pos.y };
    PhDim_t size = { area.size.w,area.size.h };
    unsigned char *ptr;
    ptr = image->image;
    ptr += image->bpl * srcY + srcX*3 ;
    //PgDrawImagemx( ptr, image->type , &pos, &size, image->bpl, 0); 
    int err = PgDrawImagemx( ptr, image->type , &pos, &size, image->bpl, 0); 
    if (err == -1)
	{
	  printf ("nsRenderingContextPh::CopyOffScreenBits Error calling PgDrawImage\n");
	}
  }
}
  else
  {
  PhPoint_t pos = { 0,0 };
  if (aCopyFlags == 12) 	// oh god, super hack.. ==12
  {
   pos.x=area.pos.x;
   pos.y=area.pos.y;
  }
  PhDim_t size = { area.size.w,area.size.h };
  unsigned char *ptr;
  ptr = image->image;
//  ptr += image->bpl * srcY + srcX*3 ;
  PgDrawImagemx( ptr, image->type , &pos, &size, image->bpl, 0); 

//    PgSetGC( mPtGC );
//    PgSetRegion( mPtGC->rid );
  }

  PgSetGC(saveGC);
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPh::RetrieveCurrentNativeGraphicData(PRUint32 * ngd)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRenderingContextPh::RetrieveCurrentNativeGraphicData - Not implemented.\n"));
  if (ngd != nsnull)
    *ngd = nsnull;
	
  return NS_OK;
}

void nsRenderingContextPh :: PushClipState(void)
{
printf ("unimp pushclipstate\n");
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRenderingContextPh::PushClipState - Not implemented.\n"));
}


void nsRenderingContextPh::ApplyClipping( PhGC_t *gc )
{
int rid;

if (!gc)
{
//	PgSetClipping(0,0);
//	PgSetMultiClip( 0, NULL );
	return;
}

rid = gc->rid;

//PtArg_t arg;
//PhPoint_t *pos;

  if (mClipRegion)
  {
    PhRegion_t my_region;
    PhRect_t rect = {{0,0},{0,0}};
    int offset_x=0,offset_y=0;
    int err;
    nsRegionPh tmp_region;
    PhTile_t *tiles;
    PhRect_t *rects;
    int rect_count;

     err = PhRegionQuery(rid, &my_region, &rect, NULL, 0);
	   
//PtSetArg(&arg,Pt_ARG_POS,&pos,0);
//PtGetResources(mWidget,1,&arg);
//printf ("clip widget: %p %d %d\n",mWidget,pos->x,pos->y);

     /* no offset needed use the normal tile list */
     mClipRegion->GetNativeRegion((void*&)tiles);

    if (tiles != nsnull)
    {
      rects = PhTilesToRects(tiles, &rect_count);
//      PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRenderingContextPh::SetGC Calling PgSetClipping with %d rects\n", rect_count));
      PgSetMultiClip(rect_count,rects);
      free(rects);
    }
    else
    {
//	printf ("hmmm, no tiles!\n");
//	PgSetMultiClip( 0, NULL );
    }
  }
//  PgSetMultiClip( 0, NULL );
}


void nsRenderingContextPh::SetPhLineStyle()
{
  switch( mCurrentLineStyle )
  {
  case nsLineStyle_kSolid:
    PgSetStrokeDash( nsnull, 0, 0x10000 );
    break;

  case nsLineStyle_kDashed:
    PgSetStrokeDash( "\10\4", 2, 0x10000 );
    break;

  case nsLineStyle_kDotted:
    PgSetStrokeDash( "\1", 1, 0x10000 );
    break;

  case nsLineStyle_kNone:
  default:
    break;
  }
}


