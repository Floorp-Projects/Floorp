extern "C" int verbose=4;	// kedl, need this while using Bobby's test render lib...

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsFontMetricsPh.h"
#include "nsRenderingContextPh.h"
#include "nsRegionPh.h"
#include "nsGraphicsStatePh.h"
#include "nsGfxCIID.h"
#include "nsICharRepresentable.h"
#include <math.h>
#include <Pt.h>
#include <photon/PhRender.h>

#include <errno.h>
#include "libimg.h"
#include "nsDeviceContextPh.h"
#include "prprf.h"
#include "nsDrawingSurfacePh.h"
#include <stdlib.h>
#include <mem.h>

static NS_DEFINE_IID(kIRenderingContextIID, NS_IRENDERING_CONTEXT_IID);
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

#ifdef DEBUG
// By creating "/dev/shmem/grab" this enables a functions that takes the
// offscreen buffer and stuffs it into "grab.bmp" to be viewed by the
// developer for debug purposes...

#include <fcntl.h>
#include <sys/mman.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

int x=0,y=0;
int X,Y,DEPTH;
int real_depth;
int scale=1;

extern "C" {
void do_bmp(unsigned char *ptr,int bpl,int x,int y);
};
#endif

#include <prlog.h>
PRLogModuleInfo *PhGfxLog = PR_NewLogModule("PhGfxLog");
#include "nsPhGfxLog.h"

NS_IMPL_ISUPPORTS1(nsRenderingContextPh, nsIRenderingContext)


/* Global Variable for Alpha Blending */
void *Mask = nsnull;

/* The default Photon Drawing Context */
PhGC_t *nsRenderingContextPh::mPtGC = nsnull;

//#define SELECT(surf) mBufferIsEmpty = PR_FALSE; if (surf->Select()) ApplyClipping(surf->GetGC());
//#define SELECT(surf) mBufferIsEmpty = PR_FALSE; surf->Select();
#define SELECT(surf) mBufferIsEmpty = PR_FALSE;

//#define PgFLUSH() PgFlush()
//#define PgFLUSH() if (mSurface) mSurface->Flush();
#define PgFLUSH()


nsRenderingContextPh :: nsRenderingContextPh()
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRenderingContextPh::nsRenderingContextPh Constructor called this=<%p>\n", this));

  NS_INIT_REFCNT();
  
  mGC               = nsnull;
  mTMatrix          = nsnull;
  mClipRegion       = nsnull;
  mFontMetrics      = nsnull;
  mSurface          = nsnull;
  mOffscreenSurface = nsnull;
  mDCOwner          = nsnull;
  mContext          = nsnull;
  mP2T              = 1.0f;
  mWidget           = nsnull;
  mPhotonFontName   = nsnull;
  Mask              = nsnull;
  mCurrentLineStyle = nsLineStyle_kSolid;
  mStateCache       = new nsVoidArray();
  mGammaTable       = nsnull;
  
  if( mPtGC == nsnull )
    mPtGC = PgGetGC();

  mInitialized   = PR_FALSE;
  mBufferIsEmpty = PR_TRUE;

  PushState();
}


nsRenderingContextPh :: ~nsRenderingContextPh()
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRenderingContextPh::~nsRenderingContextPh this=<%p> mGC = %p\n", this, mGC ));

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

  if (mTMatrix)
    delete mTMatrix;

  NS_IF_RELEASE(mClipRegion);           /* do we need to do this? */
  NS_IF_RELEASE(mOffscreenSurface);		/* this also clears mSurface.. or should */
  NS_IF_RELEASE(mFontMetrics);
  NS_IF_RELEASE(mContext);

  /* Go back to the default Photon DrawContext */
  /* This allows the photon widgets under Viewer to work right */

#if 1 // briane
	PhDrawContext_t *dc;
	dc = PhDCGetCurrent();
	if (mSurface && (dc == mSurface->GetDC()))
	{
		PhDCSetCurrent(NULL);
  		//printf("PhDCSetCurrent (~nsRenderingContextPh): NULL\n");
  	}
#else
  PhDCSetCurrent(NULL);
#endif // briane  
  PgSetGC(mPtGC);
  PgSetRegion(mPtGC->rid);
  
  if( mGC )
  {
    //PgDestroyGC( mGC );		/* this causes crashes */
    mGC = nsnull;
  }
  
  if (mPhotonFontName)
  {
    delete [] mPhotonFontName;
  }
}


NS_IMETHODIMP nsRenderingContextPh :: Init(nsIDeviceContext* aContext,
                                           nsIWidget *aWindow)
{
  NS_PRECONDITION(PR_FALSE == mInitialized, "double init");


  nsresult res;

  mContext = aContext;

  NS_IF_ADDREF(mContext);


  NS_ASSERTION(mContext,"mContext is NULL 2");
  if (!mContext)
  {
    abort();
  }
  
  mWidget = (PtWidget_t*) aWindow->GetNativeData( NS_NATIVE_WIDGET );

  if(!mWidget)
  {
    NS_IF_RELEASE(mContext); // new
    NS_ASSERTION(mWidget,"nsRenderingContext::Init (with a widget) mWidget is NULL!");
    return NS_ERROR_FAILURE;
  }

  PhRid_t    rid = PtWidgetRid( mWidget );
  

  if (rid == 0)
  {
    PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRenderingContextPh::Init Widget (%p) does not have a Rid!\n", mWidget ));
  }
  else
  {
    mGC = PgCreateGC( 16 * 1024 );		// was 4096
    if( !mGC )
    {
      PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRenderingContextPh::Init PgCreateGC() failed!\n" ));
      NS_ASSERTION(mGC, "nsRenderingContextPh::Init PgCreateGC() failed!");
      abort();
    }

    PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRenderingContextPh::Init new GC=<%p>\n",mGC));

	/* Make sure the new GC is reset to the default settings */  
    PgDefaultGC( mGC );

    mSurface = new nsDrawingSurfacePh();
    if (mSurface)
    {
      res = mSurface->Init(mGC);
      if (res != NS_OK)
      {
        PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRenderingContextPh::Init  mSurface->Init(mGC) failed\n"));
        return NS_ERROR_FAILURE;
      }

      mOffscreenSurface = mSurface;
      NS_ADDREF(mSurface);

	  /* hack up code to setup new GC for on screen drawing */
      PgSetGC( mGC );
      PgSetRegion( rid );
    }
    else
    {
      NS_ASSERTION(0, "nsRenderingContextPh::Init Failed to new the mSurface");
	  abort();
      return NS_ERROR_FAILURE;
    }
  }

  mInitialized = PR_TRUE;

  return (CommonInit());
}

NS_IMETHODIMP nsRenderingContextPh :: Init(nsIDeviceContext* aContext,
                                           nsDrawingSurface aSurface)
{
  NS_PRECONDITION(PR_FALSE == mInitialized, "double init");

  mContext = aContext;
  NS_IF_ADDREF(mContext);

  mSurface = (nsDrawingSurfacePh *) aSurface;
  NS_ADDREF(mSurface);

  mGC = mSurface->GetGC();

  mInitialized = PR_TRUE;
  return (CommonInit());
}


NS_IMETHODIMP nsRenderingContextPh::CommonInit()
{

  if ( NS_SUCCEEDED(nsComponentManager::CreateInstance(kRegionCID, 0, 
          NS_GET_IID(nsIRegion), (void**)&mClipRegion)) )
  {
    mClipRegion->Init();
    if (mSurface)
	{
      PRUint32 width, height;
	  mSurface->GetDimensions(&width, &height);
      mClipRegion->SetTo(0, 0, width, height);
	}
	else
	{
      //mClipRegion->SetTo(0,0,0,0);
      PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRenderingContextPh::CommonInit mSurface is null"));
	  //NS_ASSERTION(mSurface, "nsRenderingContextPh::CommonInit  Error no surface");
	  //abort();
	  //return NS_ERROR_FAILURE;
    }
  }
  else
  {
    // we're going to crash shortly after if we hit this, but we will return NS_ERROR_FAILURE anyways.
    return NS_ERROR_FAILURE;
  }

  if (mContext && mTMatrix)
  {
    mContext->GetDevUnitsToAppUnits(mP2T);
    float app2dev;
    mContext->GetAppUnitsToDevUnits(app2dev);
    mTMatrix->AddScale(app2dev, app2dev);
  }
  else
  {
    NS_ASSERTION(mContext,"nsRenderingContextPh::CommonInit mContext is NULL!");
    NS_ASSERTION(mTMatrix,"nsRenderingContextPh::CommonInit mTMatrix is NULL!");
    //abort();
  }
    
  return NS_OK;
}



NS_IMETHODIMP nsRenderingContextPh :: LockDrawingSurface(PRInt32 aX, PRInt32 aY,
                                                          PRUint32 aWidth, PRUint32 aHeight,
                                                          void **aBits, PRInt32 *aStride,
                                                          PRInt32 *aWidthBytes, PRUint32 aFlags)
{

PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRenderingContextPh::LockDrawingSurface\n"));

  PushState();

  return mSurface->Lock(aX, aY, aWidth, aHeight,
                        aBits, aStride, aWidthBytes, aFlags);
}

NS_IMETHODIMP nsRenderingContextPh::UnlockDrawingSurface(void)
{
PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRenderingContextPh::UnlockDrawingSurface\n"));

  PRBool  clipstate;
  PopState(clipstate);

  mSurface->Unlock();

  return NS_OK;
}


NS_IMETHODIMP nsRenderingContextPh :: SelectOffScreenDrawingSurface(nsDrawingSurface aSurface)
{
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

  mSurface->Select();


  mBufferIsEmpty = PR_TRUE;

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPh :: GetDrawingSurface(nsDrawingSurface *aSurface)
{
  *aSurface = (void *) mSurface;
  return NS_OK;
}


NS_IMETHODIMP nsRenderingContextPh :: GetHints(PRUint32& aResult)
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


NS_IMETHODIMP nsRenderingContextPh :: Reset()
{
  return NS_OK;
}


NS_IMETHODIMP nsRenderingContextPh :: GetDeviceContext(nsIDeviceContext *&aContext)
{
  NS_IF_ADDREF( mContext );
  aContext = mContext;
  return NS_OK;
}


NS_IMETHODIMP nsRenderingContextPh :: PushState(void)
{
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
	
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPh :: PopState( PRBool &aClipEmpty )
{
  //PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRenderingContextPh::PopState\n"));

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
    if (mFontMetrics != state->mFontMetrics)
	{
      SetFont(state->mFontMetrics);
    }

    if (mSurface && mClipRegion)
    {
       ApplyClipping(mGC);
    }

    //ApplyClipping(mGC);

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

  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRenderingContextPh::SetClipRect this=<%p>  mTMatrix=<%p> mClipRegion=<%p> aCombine=<%d> mGC=<%p>\n", this, mTMatrix, mClipRegion, aCombine, mGC ));
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRenderingContextPh::SetClipRect this=<%p>  aRect=<%d,%d,%d,%d>\n", this, aRect.x, aRect.y, aRect.width, aRect.height));

  if ((mTMatrix) && (mClipRegion))
  {
    PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("  prev clip empty = %i\n", mClipRegion->IsEmpty()));

    mTMatrix->TransformCoord(&trect.x, &trect.y,&trect.width, &trect.height);

    PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRenderingContextPh::SetClipRect  rect after transform (%ld,%ld,%ld,%ld)\n", trect.x, trect.y, trect.width, trect.height));

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
    res = NS_OK;
  }
  else
  {
    printf ("no region....\n");
	NS_ASSERTION(mTMatrix, "nsRenderingContextPh::SetClipRect mTMatrix is NULL");
	NS_ASSERTION(mClipRegion, "nsRenderingContextPh::SetClipRect mClipRegionis NULL");
    PR_LOG(PhGfxLog, PR_LOG_ERROR, ("nsRenderingContextPh::SetClipRect  Invalid pointers!\n"));
	abort();
  }
  																			
  return res;
}

NS_IMETHODIMP nsRenderingContextPh :: GetClipRect(nsRect &aRect, PRBool &aClipValid)
{
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

  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRenderingContextPh::GetClipRect aClipValid=<%d> rect=(%d,%d,%d,%d)\n", aClipValid,aRect.x,aRect.y,aRect.width,aRect.height));

  return NS_OK;
}


NS_IMETHODIMP nsRenderingContextPh :: SetClipRegion(PhTile_t *aTileList, nsClipCombine aCombine, PRBool &aClipEmpty)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRenderingContextPh::SetClipRegion with PhTile_t\n"));
  nsRegionPh region(aTileList);

  return SetClipRegion(region, aCombine, aClipEmpty);
}

NS_IMETHODIMP nsRenderingContextPh :: SetClipRegion(const nsIRegion& aRegion, nsClipCombine aCombine, PRBool &aClipEmpty)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRenderingContextPh::SetClipRegion this=<%p>\n", this));

  if(!mClipRegion) {
    return NS_ERROR_FAILURE;
  }
  
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
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRenderingContextPh::CopyClipRegion\n"));
  aRegion.SetTo(*NS_STATIC_CAST(nsIRegion*, mClipRegion));

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsRenderingContextPh :: GetClipRegion(nsIRegion **aRegion)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRenderingContextPh::GetClipRegion this=<%p>\n", this));
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
}

NS_IMETHODIMP nsRenderingContextPh :: SetColor(nscolor aColor)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsRenderingContextPh::SetColor (%i,%i,%i)\n", NS_GET_R(aColor), NS_GET_G(aColor), NS_GET_B(aColor) ));

  if (nsnull == mContext)  
    return NS_ERROR_FAILURE;
	
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
  SetPhLineStyle();
  
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

  nsIFontMetrics* newMetrics;
  nsresult rv = mContext->GetMetricsFor(aFont, newMetrics);
  if (NS_SUCCEEDED(rv))
  {
    rv = SetFont(newMetrics);
    NS_RELEASE(newMetrics);
  }
  return rv;
}


NS_IMETHODIMP nsRenderingContextPh :: SetFont(nsIFontMetrics *aFontMetrics)
{
  nsFontHandle  fontHandle;			/* really a nsString */
  char      *pFontHandle;

  NS_IF_RELEASE(mFontMetrics);
  mFontMetrics = aFontMetrics;
  NS_IF_ADDREF(mFontMetrics);

  if (mFontMetrics == nsnull)
    return NS_OK;

  mFontMetrics->GetFontHandle(fontHandle);
  pFontHandle = (char *) fontHandle;
    
  if (pFontHandle)
  {  
    if( mPhotonFontName )
      free(mPhotonFontName);

    mPhotonFontName = strdup(pFontHandle);
    if (mPhotonFontName)
   	{
      PgSetFont( mPhotonFontName );
    }
	else
	{
	  NS_ASSERTION(mPhotonFontName, "nsRenderingContextPh::SetFont No FOnt to set");
	  return NS_ERROR_FAILURE;
	}
  }
  else
  {
    PR_LOG(PhGfxLog, PR_LOG_ERROR, ("nsRenderingContextPh::SetFont with nsIFontMetrics, INVALID Font Handle\n"));
  }
	
  return NS_OK;
}


NS_IMETHODIMP nsRenderingContextPh :: GetFontMetrics(nsIFontMetrics *&aFontMetrics)
{
  NS_IF_ADDREF(mFontMetrics);
  aFontMetrics = mFontMetrics;
  return NS_OK;
}


// add the passed in translation to the current translation
NS_IMETHODIMP nsRenderingContextPh :: Translate(nscoord aX, nscoord aY)
{
  mTMatrix->AddTranslation((float)aX,(float)aY);
  return NS_OK;
}


// add the passed in scale to the current scale
NS_IMETHODIMP nsRenderingContextPh :: Scale(float aSx, float aSy)
{
  mTMatrix->AddScale(aSx, aSy);
  return NS_OK;
}


NS_IMETHODIMP nsRenderingContextPh :: GetCurrentTransform(nsTransform2D *&aTransform)
{
  aTransform = mTMatrix;
  return NS_OK;
}


NS_IMETHODIMP nsRenderingContextPh :: CreateDrawingSurface(nsRect *aBounds, PRUint32 aSurfFlags, nsDrawingSurface &aSurface)
{
	if (nsnull == mSurface)
	{
		aSurface = nsnull;
		return NS_ERROR_FAILURE;
	}

  	nsDrawingSurfacePh *surf = new nsDrawingSurfacePh();
  	if (surf)
  	{
		NS_ADDREF(surf);
		PhGC_t *gc = mSurface->GetGC();
		surf->Init(gc, aBounds->width, aBounds->height, aSurfFlags);
  	}
	else
	{
		NS_ASSERTION(surf, "nsRenderingContextPh::CreateDrawingSurface new nsDrawingSurfacePh is NULL");
		return NS_ERROR_FAILURE;
	}
   
  	aSurface = (nsDrawingSurface) surf;

  	return NS_OK;
}


NS_IMETHODIMP nsRenderingContextPh :: DestroyDrawingSurface(nsDrawingSurface aDS)
{
   nsDrawingSurfacePh *surf = (nsDrawingSurfacePh *) aDS;
   NS_IF_RELEASE(surf);
   return NS_OK;
}


NS_IMETHODIMP nsRenderingContextPh :: DrawLine(nscoord aX0, nscoord aY0, nscoord aX1, nscoord aY1)
{
  int err = 0;

  mTMatrix->TransformCoord(&aX0,&aY0);
  mTMatrix->TransformCoord(&aX1,&aY1);

  if (aY0 != aY1)
    aY1--;
  if (aX0 != aX1)
    aX1--;

  SELECT(mSurface);
  
  err = PgDrawILine(aX0, aY0, aX1, aY1);
  if (err == -1)
  {
	NS_ASSERTION(0, "nsRenderingContextPh::DrawLine failed");
	abort();
	return NS_ERROR_FAILURE;  
  }

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPh :: DrawStdLine(nscoord aX0, nscoord aY0, nscoord aX1, nscoord aY1)
{
  int err = 0;

  if (aY0 != aY1) 
    aY1--;
  if (aX0 != aX1)
    aX1--;

  SELECT(mSurface);
  
  err = PgDrawILine(aX0, aY0, aX1, aY1);
  if (err == -1)
  {
	NS_ASSERTION(0, "nsRenderingContextPh::DrawLine failed");
	abort();
	return NS_ERROR_FAILURE;  
  }

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPh :: DrawPolyline(const nsPoint aPoints[], PRInt32 aNumPoints)
{
  return DrawPolygon(aPoints, aNumPoints);
}


NS_IMETHODIMP nsRenderingContextPh :: DrawRect(const nsRect& aRect)
{
  return DrawRect( aRect.x, aRect.y, aRect.width, aRect.height );
}


NS_IMETHODIMP nsRenderingContextPh :: DrawRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
  nscoord x,y,w,h;

  x = aX;
  y = aY;
  w = aWidth;
  h = aHeight;
  mTMatrix->TransformCoord(&x,&y,&w,&h);

  SELECT(mSurface);

  if (w && h)
    PgDrawIRect( x, y, x + w - 1, y + h - 1, Pg_DRAW_STROKE );
  
  return NS_OK;
}


NS_IMETHODIMP nsRenderingContextPh :: FillRect(const nsRect& aRect)
{
  return FillRect( aRect.x, aRect.y, aRect.width, aRect.height );
}


NS_IMETHODIMP nsRenderingContextPh :: FillRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
  nscoord x,y,w,h;

  x = aX;
  y = aY;
  w = aWidth;
  h = aHeight;

  mTMatrix->TransformCoord(&x,&y,&w,&h);

  SELECT(mSurface);
  if (w && h)
    PgDrawIRect( x, y, x + w - 1, y + h - 1, Pg_DRAW_FILL);

  return NS_OK;
}

NS_IMETHODIMP 
nsRenderingContextPh :: InvertRect(const nsRect& aRect)
{
  return (InvertRect(aRect.x, aRect.y, aRect.width, aRect.height));
}

NS_IMETHODIMP 
nsRenderingContextPh :: InvertRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
 if (nsnull == mTMatrix || nsnull == mSurface) 
    return NS_ERROR_FAILURE;
  
  nscoord x,y,w,h;

  x = aX;
  y = aY;
  w = aWidth;
  h = aHeight;
  mTMatrix->TransformCoord(&x,&y,&w,&h);

  SELECT(mSurface);

  PgSetFillColor(Pg_INVERT_COLOR);
  PgSetDrawMode(Pg_DRAWMODE_XOR);
  PgDrawIRect( x, y, x + w - 1, y + h - 1, Pg_DRAW_FILL );
  PgSetDrawMode(Pg_DRAWMODE_OPAQUE);

  mSurface->Flush();
  
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPh :: DrawPolygon(const nsPoint aPoints[], PRInt32 aNumPoints)
{
  	PhPoint_t *pts;
  	int err = 0;

  	if (aNumPoints == 0)
  		return NS_OK;

	if ( (pts = new PhPoint_t [aNumPoints]) != NULL )
	{
		PhPoint_t pos = {0,0};
		PRInt32 i;
		int x,y;

		for (i = 0; i < aNumPoints; i++)
		{
			x = aPoints[i].x;
			y = aPoints[i].y;
			mTMatrix->TransformCoord(&x, &y);
			pts[i].x = x;
			pts[i].y = y;
		}
		PgDrawPolygon( pts, aNumPoints, &pos, Pg_DRAW_STROKE );

		delete [] pts;
	}
	
  	return NS_OK;
}


NS_IMETHODIMP nsRenderingContextPh :: FillPolygon(const nsPoint aPoints[], PRInt32 aNumPoints)
{
	PhPoint_t *pts;
	int err = 0;

	if (aNumPoints == 0)
		return NS_OK;

	if ( (pts = new PhPoint_t [aNumPoints]) != NULL )
	{
		PhPoint_t pos = {0,0};
		PRInt32 i;
		int x,y;

		for (i = 0; i < aNumPoints; i++)
		{
			x = aPoints[i].x;
			y = aPoints[i].y;
			mTMatrix->TransformCoord(&x, &y);
			pts[i].x = x;
			pts[i].y = y;
		}
	    err=PgDrawPolygon( pts, aNumPoints, &pos, Pg_DRAW_FILL );
    	if (err == -1)
		{
	  		NS_ASSERTION(NULL,  "nsRenderingContextPh::FillPolygon  Draw Buffer too small");
	  		abort();
	  		return NS_ERROR_FAILURE;
		}
	
    	delete [] pts;
  	}
  	else
  	{
	 	NS_ASSERTION(NULL, "nsRenderingContextPh::FillPolygon Probably out of memory");
	 	abort();
	 	return NS_ERROR_FAILURE;
	}  

  	return NS_OK;
}


NS_IMETHODIMP nsRenderingContextPh :: DrawEllipse(const nsRect& aRect)
{
  DrawEllipse( aRect.x, aRect.y, aRect.width, aRect.height );

  return NS_OK;
}


NS_IMETHODIMP nsRenderingContextPh :: DrawEllipse(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
  nscoord x,y,w,h;
  PhPoint_t center;
  PhPoint_t radii;
  unsigned int flags;

  x = aX;
  y = aY;
  w = aWidth;
  h = aHeight;
  mTMatrix->TransformCoord(&x, &y, &w, &h);

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
  FillEllipse( aRect.x, aRect.y, aRect.width, aRect.height );

  return NS_OK;
}


NS_IMETHODIMP nsRenderingContextPh :: FillEllipse(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
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
  flags = Pg_EXTENT_BASED | Pg_DRAW_FILL;
  SELECT(mSurface);
  PgDrawEllipse( &center, &radii, flags );

  return NS_OK;
}


NS_IMETHODIMP nsRenderingContextPh :: DrawArc(const nsRect& aRect,
                                 float aStartAngle, float aEndAngle)
{
  return DrawArc(aRect.x,aRect.y,aRect.width,aRect.height,aStartAngle,aEndAngle);
}


NS_IMETHODIMP nsRenderingContextPh :: DrawArc(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight,
                                 float aStartAngle, float aEndAngle)
{
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
  PgDrawArc( &center, &radii, aStartAngle, aEndAngle, flags );

  return NS_OK;
}


NS_IMETHODIMP nsRenderingContextPh :: FillArc(const nsRect& aRect,
                                 float aStartAngle, float aEndAngle)
{
  return FillArc(aRect.x,aRect.y,aRect.width,aRect.height,aStartAngle,aEndAngle);
}


NS_IMETHODIMP nsRenderingContextPh :: FillArc(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight,
                                 float aStartAngle, float aEndAngle)
{
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
  flags = Pg_EXTENT_BASED | Pg_DRAW_FILL;
  SELECT(mSurface);
  PgDrawArc( &center, &radii, aStartAngle, aEndAngle, flags );

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPh :: GetWidth(char ch, nscoord& aWidth)
{
  nsresult ret_code;

  // Check for the very common case of trying to get the width of a single
  // space.
   if ((ch == ' ') && (nsnull != mFontMetrics))
   {
    nsFontMetricsPh* fontMetricsPh = (nsFontMetricsPh*)mFontMetrics;
    return fontMetricsPh->GetSpaceWidth(aWidth);
  }

  return (GetWidth(&ch, 1, aWidth));
}


NS_IMETHODIMP nsRenderingContextPh :: GetWidth(PRUnichar ch, nscoord &aWidth, PRInt32 *aFontID)
{
  PRUnichar buf[2];
  nsresult ret_code;

  /* turn it into a string */
  buf[0] = ch;
  buf[1] = nsnull;
  return (GetWidth(buf, 1, aWidth, aFontID));
}


NS_IMETHODIMP nsRenderingContextPh :: GetWidth(const char* aString, nscoord& aWidth)
{
  return (GetWidth(aString, strlen(aString), aWidth));
}


NS_IMETHODIMP nsRenderingContextPh :: GetWidth(const char* aString,
                                                PRUint32 aLength,
                                                nscoord& aWidth)
{
  nsresult ret_code = NS_ERROR_FAILURE;
  FontQueryInfo tsInfo;
  
  aWidth = 0;	// Initialize to zero in case we fail.

  if (nsnull != mFontMetrics)
  {
    PhRect_t      extent;

    if (PfExtentText(&extent, NULL, mPhotonFontName, aString, aLength))
    {
    	aWidth = NSToCoordRound((int) ((extent.lr.x - extent.ul.x + 1) * mP2T));
      	ret_code = NS_OK;
    }
  }
  else
    	ret_code = NS_ERROR_FAILURE;

  return ret_code;
}


NS_IMETHODIMP nsRenderingContextPh :: GetWidth(const nsString& aString, nscoord& aWidth, PRInt32 *aFontID)
{
  nsresult ret_code;

  ret_code = GetWidth(aString.GetUnicode(), aString.Length(), aWidth, aFontID);  

  return ret_code;
}


NS_IMETHODIMP nsRenderingContextPh :: GetWidth(const PRUnichar *aString,
                                                PRUint32 aLength,
                                                nscoord &aWidth,
                                                PRInt32 *aFontID)
{
  	nsresult ret_code = NS_ERROR_FAILURE;

  	aWidth = 0;	// Initialize to zero in case we fail.
  	if (nsnull != mFontMetrics)
  	{
		PhRect_t      extent;

		NS_ConvertUCS2toUTF8    theUnicodeString (aString, aLength);
		ret_code = GetWidth(theUnicodeString.GetBuffer(), strlen(theUnicodeString.GetBuffer()), aWidth);
    }
	if (nsnull != aFontID)
	  *aFontID = 0;

  	return ret_code;
}


NS_IMETHODIMP nsRenderingContextPh :: DrawString(const char *aString, PRUint32 aLength,
                                                  nscoord aX, nscoord aY,
                                                  const nscoord* aSpacing)
{
	int err = 0;
	nscoord x = aX;
	nscoord y = aY;

	if (nsnull != aSpacing)
	{
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
		err=PgDrawTextChars( aString, aLength, &pos, Pg_TEXT_LEFT | Pg_TEXT_TOP);
		PgColor_t cc = PgSetTextColor(Pg_BLACK);
		PgSetTextColor(cc);
		if ( err == -1)
	   		abort();
	}

  	return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPh :: DrawString(const PRUnichar *aString, PRUint32 aLength,
                                                  nscoord aX, nscoord aY,
                                                  PRInt32 aFontID,
                                                  const nscoord* aSpacing)
{
  	nsresult ret = NS_ERROR_FAILURE;
  	char *str;
	NS_ConvertUCS2toUTF8    theUnicodeString (aString, aLength);
	ret = DrawString(theUnicodeString.GetBuffer(), strlen(theUnicodeString.GetBuffer()),
		aX, aY, aSpacing);
	return ret;
}

NS_IMETHODIMP nsRenderingContextPh :: DrawString(const nsString& aString,
                                                  nscoord aX, nscoord aY,
                                                  PRInt32 aFontID,
                                                  const nscoord* aSpacing)
{
  	nsresult ret = NS_ERROR_FAILURE;
	NS_ConvertUCS2toUTF8    theUnicodeString (aString.GetUnicode(), aString.Length());
	ret = DrawString(theUnicodeString.GetBuffer(), strlen(theUnicodeString.GetBuffer()),
			aX, aY, aSpacing);
	return ret;
}


NS_IMETHODIMP nsRenderingContextPh::DrawImage(nsIImage *aImage,
                                               nscoord aX, nscoord aY)
{
  nscoord width, height;

  // we have to do this here because we are doing a transform below
  width = NSToCoordRound(mP2T * aImage->GetWidth());
  height = NSToCoordRound(mP2T * aImage->GetHeight());

  return (DrawImage(aImage, aX, aY, width, height));
}

NS_IMETHODIMP nsRenderingContextPh::DrawImage(nsIImage *aImage, const nsRect& aRect)
{
  return (DrawImage(aImage, aRect.x, aRect.y, aRect.width, aRect.height));
}

NS_IMETHODIMP nsRenderingContextPh::DrawImage(nsIImage *aImage,
                                               nscoord aX, nscoord aY,
                                               nscoord aWidth, nscoord aHeight)
{
  	nscoord x, y, w, h;
  	nsresult res = NS_OK;
	int ww,hh;

#if 0 // printing
  if (mClipRegion->IsEmpty())
  {
    // this is bad!
    //    printf("drawing image with empty clip region\n");
    //printf("nsRenderingContextPh::DrawImage2 drawing image with empty clip region\n");
    return NS_ERROR_FAILURE;
  }
#endif 

	x = aX;
	y = aY;
	w = aWidth;
	h = aHeight;
	mTMatrix->TransformCoord(&x, &y, &w, &h);

  SELECT(mSurface);

	ww = mSurface->mWidth;
	hh = mSurface->mHeight;

    nsDeviceContextPh  *phContext = (nsDeviceContextPh *)mContext;
    if (!phContext || !(phContext->IsPrinting()))
    {
		if (x >= ww || y >=hh)
			return NS_OK;
	}

	if (x + w > ww) 
		w = ww - x;
	if (y + h > hh) 
		h = hh - y;

  	return (aImage->Draw(*this, mSurface, x, y, w, h));
}


NS_IMETHODIMP nsRenderingContextPh::DrawImage(nsIImage *aImage,
                                               const nsRect& aSRect,
                                               const nsRect& aDRect)
{
  nsRect	sr,dr;
  nsresult  res = NS_OK;

  if (mClipRegion->IsEmpty())
  {
    // this is bad!
    //    printf("drawing image with empty clip region\n");
    return NS_ERROR_FAILURE;
  }

	sr = aSRect;
	mTMatrix->TransformCoord(&sr.x, &sr.y, &sr.width, &sr.height);
	dr = aDRect;
	mTMatrix->TransformCoord(&dr.x, &dr.y, &dr.width, &dr.height);

  SELECT(mSurface);

  	return (aImage->Draw(*this, mSurface, sr.x, sr.y, sr.width, sr.height, \
  		dr.x, dr.y, dr.width, dr.height));
}

/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 *	@update 3/16/00 dwc
 */
NS_IMETHODIMP 
nsRenderingContextPh::DrawTile(nsIImage *aImage,nscoord aX0,nscoord aY0,nscoord aX1,nscoord aY1,
                                                    nscoord aWidth,nscoord aHeight)
{
	mTranMatrix->TransformCoord(&aX0,&aY0,&aWidth,&aHeight);
	mTranMatrix->TransformCoord(&aX1,&aY1);

	nsRect srcRect (0, 0, aWidth,  aHeight);
	nsRect tileRect(aX0, aY0, aX1-aX0, aY1-aY0);

	((nsImagePh*)aImage)->DrawTile(*this, mSurface, srcRect, tileRect);

  	return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextPh::DrawTile(nsIImage *aImage, nscoord aSrcXOffset, nscoord aSrcYOffset,
                                const nsRect &aTileRect)
{
	nsRect tileRect(aTileRect);
	nsRect srcRect(0, 0, aSrcXOffset, aSrcYOffset);
	mTMatrix->TransformCoord(&srcRect.x, &srcRect.y, &srcRect.width, &srcRect.height);
	mTMatrix->TransformCoord(&tileRect.x, &tileRect.y, &tileRect.width, &tileRect.height);

	if ((tileRect.width > 0) && (tileRect.height > 0))
  		((nsImagePh*)aImage)->DrawTile(*this, mSurface, srcRect.width, srcRect.height, tileRect);

  	return NS_OK;
}


NS_IMETHODIMP nsRenderingContextPh :: CopyOffScreenBits(nsDrawingSurface aSrcSurf,
                                                         PRInt32 aSrcX, PRInt32 aSrcY,
                                                         const nsRect &aDestBounds,
                                                         PRUint32 aCopyFlags)
{

  PhArea_t              darea, sarea;
  PRInt32               srcX = aSrcX;
  PRInt32               srcY = aSrcY;
  nsRect                drect = aDestBounds;
  nsDrawingSurfacePh    *destsurf;
  int                   err;
  unsigned char         *ptr;

  if ( (aSrcSurf==NULL) || (mTMatrix==NULL) || (mSurface==NULL))
  {
    NS_ASSERTION(0, "nsRenderingContextPh::CopyOffScreenBits Started with NULL pointer");
    return NS_ERROR_FAILURE;  
  }
  
  if (aCopyFlags & NS_COPYBITS_TO_BACK_BUFFER)
  {
    NS_ASSERTION(!(nsnull == mSurface), "no back buffer");
    destsurf = mSurface;
  }
  else
    destsurf = mOffscreenSurface;

  if (aCopyFlags & NS_COPYBITS_XFORM_SOURCE_VALUES)
    mTMatrix->TransformCoord(&srcX, &srcY);

  if (aCopyFlags & NS_COPYBITS_XFORM_DEST_VALUES)
    mTMatrix->TransformCoord(&drect.x, &drect.y, &drect.width, &drect.height);

  darea.pos.x=drect.x;
  darea.pos.y=drect.y;
  darea.size.w=drect.width;
  darea.size.h=drect.height;

  nsRect rect;
  PRBool valid;

  /* Is this really needed?? */
  GetClipRect(rect,valid);

  /* Flush the Source buffer, Really need this  */
  ((nsDrawingSurfacePh *)aSrcSurf)->Flush();

  if (aSrcSurf == destsurf)
    abort();

   if (aSrcSurf != destsurf)
   {
     destsurf->Select();
   }

{//xyz
PhRect_t rsrc,rdst;
PdOffscreenContext_t *d;
int off;
int oldrid;
PhGC_t *oldgc;
int ww,hh;
int rid;

	oldgc = PgGetGC();
	oldrid=(PgGetGC())->rid;

	destsurf->IsOffscreen(&off);
	if (off)
		d = (PdOffscreenContext_t *) destsurf->GetDC();
	else
	{
		d=NULL;
 		PhDCSetCurrent(NULL);
		(PgGetGC())->target_rid = 0;	// kedl, fix the animations showing thru all regions
	}

	ww = destsurf->mWidth;
	hh = destsurf->mHeight;
	if (d && (darea.pos.x >= ww || darea.pos.y >=hh))
		return NS_OK;
	
	PhArea_t sarea;
	sarea.pos.x = srcX;
	sarea.pos.y = srcY;
	sarea.size.w = darea.size.w;
	sarea.size.h = darea.size.h;

	PhDCSetCurrent(d);
	rid = PtWidgetRid(mWidget);
	PgSetRegion(rid);

	PgContextBlitArea((PdOffscreenContext_t *) ((nsDrawingSurfacePh *)aSrcSurf)->GetDC(), &sarea, d, &darea);
	
	PgSetRegion(oldrid);
	PgSetGC(oldgc);
	ApplyClipping(oldgc);
}

   if (err == -1)
   {
     printf ("nsRenderingContextPh::CopyOffScreenBits Error calling PgDrawImage\n");
	 abort();
   }

   destsurf->Flush();
	  
  mBufferIsEmpty = PR_TRUE;
  
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPh::RetrieveCurrentNativeGraphicData(PRUint32 * ngd)
{
  if (ngd != nsnull)
    *ngd = nsnull;
  	
  return NS_OK;
}

void nsRenderingContextPh :: PushClipState(void)
{
}


void nsRenderingContextPh::ApplyClipping( PhGC_t *gc )
{
#if 0 // briane
	if (!gc)
	{
		NS_ASSERTION(0,"nsRenderingContextPh::ApplyClipping gc is NULL");
		abort(); /* Is this an error? Try Test10 */
		return;
	}
#endif	

	PgSetGC(mGC);	/* new */
  
	if (mClipRegion)
	{
	int         err;
	PhTile_t    *tiles = nsnull;
	PhRect_t    *rects = nsnull;
	int         rect_count;

		/* no offset needed use the normal tile list */
		mClipRegion->GetNativeRegion((void*&)tiles);

		if (tiles != nsnull)
		{
			rects = PhTilesToRects(tiles, &rect_count);
			err=PgSetMultiClip(rect_count,rects);
	
			if (err == -1)
			{
				NS_ASSERTION(0,"nsRenderingContextPh::ApplyClipping Error in PgSetMultiClip probably not enough memory");
				abort();
			}
	   
			free(rects);
		}
		else
		{
			PgSetMultiClip( 0, NULL );
		}
	}
	else
		printf("nsRenderingContextPh::ApplyClipping  mClipRegion is NULL");
}


void nsRenderingContextPh::SetPhLineStyle()
{
	switch( mCurrentLineStyle )
	{
		case nsLineStyle_kSolid:
			PgSetStrokeDash( nsnull, 0, 0x10000 );
			break;

		case nsLineStyle_kDashed:
			PgSetStrokeDash( (const unsigned char *)"\10\4", 2, 0x10000 );
			break;

		case nsLineStyle_kDotted:
			PgSetStrokeDash( (const unsigned char *)"\1", 1, 0x10000 );
			break;

		case nsLineStyle_kNone:
			default:
			break;
	}
}
