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
 * Roland Mainz <roland.mainz@informatik.med.uni-giessen.de>
 *
 */

#include <math.h>
#include <strings.h>
#include "nsRenderingContextXP.h"
#include "nsFontMetricsXlib.h"
#include "libimg.h"
#include "nsDeviceContextXP.h"
#include "nsXPrintContext.h"
#include "nsICharRepresentable.h"
#include "xlibrgb.h"
#include "nsIRegion.h"      
#include "prmem.h"

static NS_DEFINE_IID(kIRenderingContextIID, NS_IRENDERING_CONTEXT_IID);

#ifdef PR_LOGGING 
static PRLogModuleInfo *RenderingContextXpLM = PR_NewLogModule("nsRenderingContextXp");
#endif /* PR_LOGGING */ 

static nsGCCacheXlib *gcCache = nsnull;

class GraphicsState
{
public:
  GraphicsState();
  ~GraphicsState();

  nsTransform2D  *mMatrix;
  nsCOMPtr<nsIRegion> mClipRegion;
  nscolor         mColor;
  nsLineStyle     mLineStyle;
  nsIFontMetrics *mFontMetrics;
};

MOZ_DECL_CTOR_COUNTER(GraphicsState)

GraphicsState::GraphicsState()
{
  MOZ_COUNT_CTOR(GraphicsState);
  mMatrix = nsnull;
  mColor = NS_RGB(0, 0, 0);
  mLineStyle = nsLineStyle_kSolid;
  mFontMetrics = nsnull;
}

GraphicsState::~GraphicsState()
{
  MOZ_COUNT_DTOR(GraphicsState);
  NS_IF_RELEASE(mFontMetrics);
}

// This is not being used
static NS_DEFINE_IID(kRenderingContextIID, NS_IRENDERING_CONTEXT_IID);


/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 */
nsRenderingContextXp :: nsRenderingContextXp()
{
  PR_LOG(RenderingContextXpLM, PR_LOG_DEBUG, ("nsRenderingContextXp::nsRenderingContextXp()\n"));
  
  NS_INIT_REFCNT();
  mPrintContext =    nsnull;
  mContext          = nsnull;
  mFontMetrics      = nsnull;
  mTranMatrix       = nsnull;
  mP2T              = 1.0f;
  mStateCache       = new nsVoidArray();
  mCurrentFont      = nsnull;
  mCurrentLineStyle = nsLineStyle_kSolid;
  mCurrentColor     = NS_RGB(0, 0, 0); /* X11 intial bg color is always _black_...
                                        * ...but we should query that from 
                                        * Xserver instead of guessing that...
                                        */
  mDisplay          = nsnull;
  mScreen           = nsnull;
  mVisual           = nsnull;
  mDepth            = 0;
  mGC               = nsnull;

  mFunction         = GXcopy;

  PushState();
}

/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 */
nsRenderingContextXp :: ~nsRenderingContextXp()
{
  PR_LOG(RenderingContextXpLM, PR_LOG_DEBUG, ("nsRenderingContextXp::~nsRenderingContextXp()\n"));

  NS_IF_RELEASE(mFontMetrics);
  if (mStateCache) {
    PRInt32 cnt = mStateCache->Count();

    while (--cnt >= 0) {
      PRBool clipstate;
      PopState(clipstate);
    }
    delete mStateCache;
    mStateCache = nsnull;
  }

  if (mTranMatrix)
    delete mTranMatrix;
  NS_IF_RELEASE(mFontMetrics);
  
  if(nsnull != mGC)
    mGC->Release();
}

/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 */
nsresult
nsRenderingContextXp :: QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (nsnull == aInstancePtr)
    return NS_ERROR_NULL_POINTER;

  if (aIID.Equals(kIRenderingContextIID)){
    nsIRenderingContext* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }

  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

  if (aIID.Equals(kISupportsIID)){
    nsIRenderingContext* tmp = this;
    nsISupports* tmp2 = tmp;
    *aInstancePtr = (void*) tmp2;
    NS_ADDREF_THIS();
    return NS_OK;
  }

  return NS_NOINTERFACE;
}

NS_IMPL_ADDREF(nsRenderingContextXp)
NS_IMPL_RELEASE(nsRenderingContextXp)

/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 */
NS_IMETHODIMP
nsRenderingContextXp :: Init(nsIDeviceContext* aContext)
{
  PR_LOG(RenderingContextXpLM, PR_LOG_DEBUG, ("nsRenderingContextXp::Init(nsIDeviceContext *)\n"));

  mContext = do_QueryInterface(aContext);
  if (mContext) {
     mContext->GetPrintContext(mPrintContext);
  }
  NS_ASSERTION(nsnull != mContext, "Device context is null.");

  mDisplay = mPrintContext->GetDisplay();
  mScreen  = mPrintContext->GetScreen();
  mVisual  = mPrintContext->GetVisual();
  mDepth   = mPrintContext->GetDepth();

  return CommonInit();
}

/*static*/ nsresult
nsRenderingContextXp::Shutdown()
{
  delete gcCache;
  gcCache = nsnull;
  return NS_OK;
}

nsresult nsRenderingContextXp::CommonInit(void)
{
  // put common stuff in here.
  int x, y;
  unsigned int width, height, border, depth;
  Window root_win;

  Drawable drawable = mPrintContext->GetDrawable();

  XGetGeometry(mDisplay, 
               drawable, 
               &root_win,
               &x, 
               &y, 
               &width, 
               &height, 
               &border, 
               &depth);

  mClipRegion = do_QueryInterface(new nsRegionXlib());
  if (!mClipRegion) return NS_ERROR_OUT_OF_MEMORY;
  mClipRegion->Init();
  mClipRegion->SetTo(0, 0, width, height);

  mContext->GetDevUnitsToAppUnits(mP2T);
  float app2dev;
  mContext->GetAppUnitsToDevUnits(app2dev);
  mTranMatrix->AddScale(app2dev, app2dev);
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXp::Reset(void)
{
  PR_LOG(RenderingContextXpLM, PR_LOG_DEBUG, ("nsRenderingContextXp::Reset()\n"));
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXp::GetDeviceContext(nsIDeviceContext *&aContext)
{
  PR_LOG(RenderingContextXpLM, PR_LOG_DEBUG, ("nsRenderingContextXp::GetDeviceContext()\n"));
  aContext = mContext;
  NS_IF_ADDREF(aContext);
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 */
NS_IMETHODIMP nsRenderingContextXp :: LockDrawingSurface(
                                             PRInt32 aX, PRInt32 aY,
                                             PRUint32 aWidth, PRUint32 aHeight,
                                             void **aBits, PRInt32 *aStride,
                                             PRInt32 *aWidthBytes, 
                                             PRUint32 aFlags)
{
  PR_LOG(RenderingContextXpLM, PR_LOG_DEBUG, ("nsRenderingContextXp::LockDrawingSurface()\n"));
  PushState();
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 */
NS_IMETHODIMP nsRenderingContextXp :: UnlockDrawingSurface(void)
{
  PR_LOG(RenderingContextXpLM, PR_LOG_DEBUG, ("nsRenderingContextXp::UnlockDrawingSurface()\n"));
  PRBool clipstate;
  PopState(clipstate);
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 */
NS_IMETHODIMP
nsRenderingContextXp :: SelectOffScreenDrawingSurface(nsDrawingSurface aSurface)
{
  PR_LOG(RenderingContextXpLM, PR_LOG_DEBUG, ("nsRenderingContextXp::SelectOffScreenDrawingSurface()\n"));
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 */
NS_IMETHODIMP
nsRenderingContextXp :: GetDrawingSurface(nsDrawingSurface *aSurface)
{
  PR_LOG(RenderingContextXpLM, PR_LOG_DEBUG, ("nsRenderingContextXp::GetDrawingSurface()\n"));
  *aSurface = nsnull;
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 */
NS_IMETHODIMP
nsRenderingContextXp :: GetHints(PRUint32& aResult)
{
  PR_LOG(RenderingContextXpLM, PR_LOG_DEBUG, ("nsRenderingContextXp::GetHints()\n"));

  PRUint32 result = 0;
  // Most X servers implement 8 bit text rendering alot faster than
  // XChar2b rendering. In addition, we can avoid the PRUnichar to
  // XChar2b conversion. So we set this bit...
  result |= NS_RENDERING_HINT_FAST_8BIT_TEXT;
  aResult = result;
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 */
NS_IMETHODIMP
nsRenderingContextXp::PushState(void)
{
  PR_LOG(RenderingContextXpLM, PR_LOG_DEBUG, ("nsRenderingContextXp::PushState()\n"));
  GraphicsState *state = new GraphicsState();

  if (state == 0) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  state->mMatrix = mTranMatrix;
  
  mStateCache->AppendElement(state);
  
  if (nsnull == mTranMatrix)
    mTranMatrix = new nsTransform2D();
  else
    mTranMatrix = new nsTransform2D(mTranMatrix);
  
  if (mClipRegion) {
    state->mClipRegion = mClipRegion;
    mClipRegion = do_QueryInterface(new nsRegionXlib());
    if (!mClipRegion) return NS_ERROR_OUT_OF_MEMORY;
    mClipRegion->Init();
    mClipRegion->SetTo(*state->mClipRegion);
  }
  
  NS_IF_ADDREF(mFontMetrics);
  state->mFontMetrics = mFontMetrics;
  state->mColor = mCurrentColor;
  state->mLineStyle = mCurrentLineStyle;
  
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXp::PopState(PRBool &aClipState)
{
  PR_LOG(RenderingContextXpLM, PR_LOG_DEBUG, ("nsRenderingContextXp::PopState()\n"));

  PRUint32 cnt = mStateCache->Count();
  GraphicsState *state;
  
  if (cnt > 0) {
    state = (GraphicsState *)mStateCache->ElementAt(cnt - 1);
    mStateCache->RemoveElementAt(cnt - 1);
    
    if (mTranMatrix)
      delete mTranMatrix;
    mTranMatrix = state->mMatrix;
    
    mClipRegion = state->mClipRegion;
    if (mFontMetrics != state->mFontMetrics)
      SetFont(state->mFontMetrics);

    if (state->mColor != mCurrentColor)
      SetColor(state->mColor);
    if (state->mLineStyle != mCurrentLineStyle)
      SetLineStyle(state->mLineStyle);

    delete state;
  }

  if (mClipRegion)
    aClipState = mClipRegion->IsEmpty();
  else 
    aClipState = PR_TRUE;

  return NS_OK;
}

void nsRenderingContextXp::UpdateGC()
{
   XGCValues     values;
   unsigned long valuesMask;
   unsigned long color;
 
   if (mGC)
     mGC->Release();
 
   memset(&values, 0, sizeof(XGCValues));
 
 
   color = xlib_rgb_xpixel_from_rgb (NS_RGB(NS_GET_B(mCurrentColor),
                                            NS_GET_G(mCurrentColor),
                                            NS_GET_R(mCurrentColor)));

   values.foreground = color;
   valuesMask = GCForeground;
 
   if (mCurrentFont) {
     valuesMask |= GCFont;
     values.font = mCurrentFont->fid;
   }
 
   values.line_style = mLineStyle;
   valuesMask |= GCLineStyle;
 
   values.function = mFunction;
   valuesMask |= GCFunction;

   Region rgn = 0;
   if (mClipRegion) { 
     mClipRegion->GetNativeRegion((void*&)rgn);
   }
 
   if (!gcCache) {
     gcCache = new nsGCCacheXlib();
     if (!gcCache) return;
   }

   mGC = gcCache->GetGC(mDisplay, mPrintContext->GetDrawable(), 
                        valuesMask, &values,
                        rgn);
}

NS_IMETHODIMP
nsRenderingContextXp::IsVisibleRect(const nsRect& aRect, PRBool &aVisible)
{
  PR_LOG(RenderingContextXpLM, PR_LOG_DEBUG, ("nsRenderingContextXp::IsVisibleRect()\n"));
  aVisible = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXp::SetClipRect(const nsRect& aRect, nsClipCombine aCombine, PRBool &aClipState)
{
  PR_LOG(RenderingContextXpLM, PR_LOG_DEBUG, ("nsRenderingContextXp::SetClipRect()\n"));
  nsRect trect = aRect;
  Region rgn;
  mTranMatrix->TransformCoord(&trect.x, &trect.y,
                           &trect.width, &trect.height);
  switch(aCombine) {
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
  }
  aClipState = mClipRegion->IsEmpty();

  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXp::GetClipRect(nsRect &aRect, PRBool &aClipState)
{
  PR_LOG(RenderingContextXpLM, PR_LOG_DEBUG, ("nsRenderingContextXp::GetClipRext()\n"));
  PRInt32 x, y, w, h;
  if (!mClipRegion->IsEmpty()) {
    mClipRegion->GetBoundingBox(&x,&y,&w,&h);
    aRect.SetRect(x,y,w,h);
    aClipState = PR_TRUE;
  } else {
    aRect.SetRect(0,0,0,0);
    aClipState = PR_FALSE;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXp::SetClipRegion(const nsIRegion& aRegion, nsClipCombine aCombine, PRBool &aClipState)
{
  PR_LOG(RenderingContextXpLM, PR_LOG_DEBUG, ("nsRenderingContextXp::SetClipRegion()\n"));
  Region rgn;
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

  aClipState = mClipRegion->IsEmpty();

  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXp::CopyClipRegion(nsIRegion &aRegion)
{
  PR_LOG(RenderingContextXpLM, PR_LOG_DEBUG, ("nsRenderingContextXp::CopyClipRegion()\n"));
  aRegion.SetTo(*mClipRegion);
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXp::GetClipRegion(nsIRegion **aRegion)
{
  PR_LOG(RenderingContextXpLM, PR_LOG_DEBUG, ("nsRenderingContextXp::GetClipRegion()\n"));
  nsresult  rv = NS_OK;
  
  NS_ASSERTION(!(nsnull == aRegion), "no region ptr");
  
  if (*aRegion) {
    (*aRegion)->SetTo(*mClipRegion);
  }
  return rv;
}

NS_IMETHODIMP
nsRenderingContextXp::SetLineStyle(nsLineStyle aLineStyle)
{
  PR_LOG(RenderingContextXpLM, PR_LOG_DEBUG, ("nsRenderingContextXp::SetLineStyle()\n"));
  if (aLineStyle != mCurrentLineStyle) {
    /* XXX this isnt done in gtk, copy from there when ready
    switch(aLineStyle)
      { 
      case nsLineStyle_kSolid:
        mLineStyle = LineSolid;
        mDashes = 0;
        break;
      case nsLineStyle_kDashed:
          static char dashed[2] = {4,4};
        mDashList = dashed;
        mDashes = 2;
        break;
      case nsLineStyle_kDotted:
          static char dotted[2] = {3,1};
        mDashList = dotted;
        mDashes = 2;
        break;
    default:
        break;

    }
    */
    mCurrentLineStyle = aLineStyle ;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXp::GetLineStyle(nsLineStyle &aLineStyle)
{
  PR_LOG(RenderingContextXpLM, PR_LOG_DEBUG, ("nsRenderingContextXp::GetLineStyle()\n"));
  aLineStyle = mCurrentLineStyle;
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXp::SetColor(nscolor aColor)
{
  PR_LOG(RenderingContextXpLM, PR_LOG_DEBUG, ("nsRenderingContextXp::SetColor(nscolor)\n"));
  if (nsnull == mContext)
    return NS_ERROR_FAILURE;

  mCurrentColor = aColor;

  PR_LOG(RenderingContextXpLM, PR_LOG_DEBUG, ("Setting color to %d %d %d\n", NS_GET_R(aColor), NS_GET_G(aColor), NS_GET_B(aColor)));
  return NS_OK;
}


NS_IMETHODIMP
nsRenderingContextXp::GetColor(nscolor &aColor) const
{
  PR_LOG(RenderingContextXpLM, PR_LOG_DEBUG, ("nsRenderingContextXp::GetColor()\n"));
  aColor = mCurrentColor;
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXp::SetFont(const nsFont& aFont)
{
  PR_LOG(RenderingContextXpLM, PR_LOG_DEBUG, ("nsRenderingContextXp::SetFont(nsFont)\n"));
  NS_IF_RELEASE(mFontMetrics);
  mContext->GetMetricsFor(aFont, mFontMetrics);
  return SetFont(mFontMetrics);
}

NS_IMETHODIMP
nsRenderingContextXp::SetFont(nsIFontMetrics *aFontMetrics)
{
  PR_LOG(RenderingContextXpLM, PR_LOG_DEBUG, ("nsRenderingContextXp::SetFont(nsIFontMetrics)\n"));
  NS_IF_RELEASE(mFontMetrics);
  mFontMetrics = aFontMetrics;
  NS_IF_ADDREF(mFontMetrics);

  if (mFontMetrics)
  {
    nsFontHandle  fontHandle;
    mFontMetrics->GetFontHandle(fontHandle);
    mCurrentFont = (XFontStruct *)fontHandle;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXp::GetFontMetrics(nsIFontMetrics *&aFontMetrics)
{
  PR_LOG(RenderingContextXpLM, PR_LOG_DEBUG, ("nsRenderingContextXp::GetFontMetrics()\n"));
  NS_IF_ADDREF(mFontMetrics);
  aFontMetrics = mFontMetrics;
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXp::Translate(nscoord aX, nscoord aY)
{
  PR_LOG(RenderingContextXpLM, PR_LOG_DEBUG, ("nsRenderingContextXp::Translate()\n"));
  mTranMatrix->AddTranslation((float)aX,(float)aY);
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXp::Scale(float aSx, float aSy)
{
  PR_LOG(RenderingContextXpLM, PR_LOG_DEBUG, ("nsRenderingContextXp::Scale()\n"));
  mTranMatrix->AddScale(aSx, aSy);
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXp::GetCurrentTransform(nsTransform2D *&aTransform)
{
  PR_LOG(RenderingContextXpLM, PR_LOG_DEBUG, ("nsRenderingContextXp::GetCurrentTransform()\n"));
  aTransform = mTranMatrix;
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXp::CreateDrawingSurface(nsRect *aBounds, PRUint32 aSurfFlags, nsDrawingSurface &aSurface)
{
  PR_LOG(RenderingContextXpLM, PR_LOG_DEBUG, ("nsRenderingContextXp::CreateDrawingSurface()\n"));

  aSurface = nsnull;

  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXp::DestroyDrawingSurface(nsDrawingSurface aDS)
{
  PR_LOG(RenderingContextXpLM, PR_LOG_DEBUG, ("nsRenderingContextXp::DestroyDrawingSurface()\n"));

  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXp::DrawLine(nscoord aX0, nscoord aY0, nscoord aX1, nscoord aY1)
{
  nscoord diffX, diffY;

  PR_LOG(RenderingContextXpLM, PR_LOG_DEBUG, ("nsRenderingContextXp::DrawLine()\n"));
  if (nsnull == mTranMatrix || nsnull == mPrintContext)
    return NS_ERROR_FAILURE;

  mTranMatrix->TransformCoord(&aX0,&aY0);
  mTranMatrix->TransformCoord(&aX1,&aY1);
  
  diffX = aX1-aX0;
  diffY = aY1-aY0;

  if (0!=diffX) {
    diffX = (diffX>0?1:-1);
  }
  if (0!=diffY) {
    diffY = (diffY>0?1:-1);
  }

  UpdateGC();

  ::XDrawLine(mDisplay, mPrintContext->GetDrawable(),
              *mGC, aX0, aY0, aX1 - diffX, aY1 - diffY);

  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXp::DrawStdLine(nscoord aX0, nscoord aY0, nscoord aX1, nscoord aY1)
{
  nscoord diffX, diffY;

  PR_LOG(RenderingContextXpLM, PR_LOG_DEBUG, ("nsRenderingContextXp::DrawStdLine()\n"));
  if (nsnull == mTranMatrix || nsnull == mPrintContext)
    return NS_ERROR_FAILURE;

  mTranMatrix->TransformCoord(&aX0,&aY0);
  mTranMatrix->TransformCoord(&aX1,&aY1);
  
  diffX = aX1-aX0;
  diffY = aY1-aY0;

  if (0!=diffX) {
    diffX = (diffX>0?1:-1);
  }
  if (0!=diffY) {
    diffY = (diffY>0?1:-1);
  }

  UpdateGC();
  ::XDrawLine(mDisplay, mPrintContext->GetDrawable(),
              *mGC, aX0, aY0, aX1 - diffX, aY1 - diffY);

  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXp::DrawPolyline(const nsPoint aPoints[], PRInt32 aNumPoints)
{
  PR_LOG(RenderingContextXpLM, PR_LOG_DEBUG, ("nsRenderingContextXp::DrawPolyLine()\n"));
  if (nsnull == mTranMatrix || nsnull == mPrintContext) {
    return NS_ERROR_FAILURE;
  }
  PRInt32 i ;
  XPoint * xpoints;
  XPoint * thispoint;

  xpoints = (XPoint *) PR_Malloc(sizeof(XPoint) * aNumPoints);

  for (i = 0; i < aNumPoints; i++){
    thispoint = (xpoints+i);
    thispoint->x = aPoints[i].x;
    thispoint->y = aPoints[i].y;
    mTranMatrix->TransformCoord((PRInt32*)&thispoint->x,(PRInt32*)&thispoint->y);
  }

  UpdateGC();
  ::XDrawLines(mDisplay,
               mPrintContext->GetDrawable(),
               *mGC,
               xpoints, aNumPoints, CoordModeOrigin);

  PR_Free((void *)xpoints);

  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXp::DrawRect(const nsRect& aRect)
{
  PR_LOG(RenderingContextXpLM, PR_LOG_DEBUG, ("nsRenderingContextXp::DrawRext()\n"));
  return DrawRect(aRect.x, aRect.y, aRect.width, aRect.height);
}

NS_IMETHODIMP
nsRenderingContextXp::DrawRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
  PR_LOG(RenderingContextXpLM, PR_LOG_DEBUG, ("nsRenderingContextXp::DrawRect()\n"));
  if (nsnull == mTranMatrix || nsnull == mPrintContext) {
    return NS_ERROR_FAILURE;
  }

  nscoord x,y,w,h; 
  
  x = aX;
  y = aY; 
  w = aWidth;
  h = aHeight;
    
  mTranMatrix->TransformCoord(&x,&y,&w,&h);
    
  // After the transform, if the numbers are huge, chop them, because
  // they're going to be converted from 32 bit to 16 bit.
  // It's all way off the screen anyway.
  ConditionRect(x,y,w,h);
   
  // Don't draw empty rectangles; also, w/h are adjusted down by one
  // so that the right number of pixels are drawn.
  if (w && h) 
  {
    UpdateGC();
    ::XDrawRectangle(mDisplay,
                     mPrintContext->GetDrawable(),
                     *mGC,
                     x,
                     y,
                     w - 1,
                     h - 1);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXp::FillRect(const nsRect& aRect)
{
  PR_LOG(RenderingContextXpLM, PR_LOG_DEBUG, ("nsRenderingContextXp::FillRect(aRect)\n"));
  return FillRect(aRect.x, aRect.y, aRect.width, aRect.height);
}

NS_IMETHODIMP
nsRenderingContextXp::FillRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
  PR_LOG(RenderingContextXpLM, PR_LOG_DEBUG, ("nsRenderingContextXp::FillRect(aX,aY,aWidth,aHeight)\n"));
  if (nsnull == mTranMatrix || nsnull == mPrintContext) {
    return NS_ERROR_FAILURE;
  }
  nscoord x,y,w,h;
  x = aX;
  y = aY;
  w = aWidth;
  h = aHeight;

  mTranMatrix->TransformCoord(&x,&y,&w,&h);

  // After the transform, if the numbers are huge, chop them, because
  // they're going to be converted from 32 bit to 16 bit.
  // It's all way off the screen anyway.
  ConditionRect(x,y,w,h);

  PR_LOG(RenderingContextXpLM, PR_LOG_DEBUG, ("About to fill window 0x%lxd with rect %d %d %d %d\n",
                                                mPrintContext->GetDrawable(), x, y, w, h));
  UpdateGC();

/* the following code is disabled for two reasons:
 * 1. It does not work because it does not at the real coordinates instead of
 *    the translated ones.
 * 2. Such an optimisation should be done in Xprt (server side/DDX layer) -
 *    not in the client... 
 */ 
#ifdef XPRINT_NOT_NOW  
  // Hack for background page
  if ((x == 0) && (y == 0)) {
    PR_LOG(RenderingContextXpLM, PR_LOG_DEBUG, 
           ("nsRenderingContextXp::FillRect() - background page fillrect supressed\n"));
    return NS_OK;
  }
#endif /* XPRINT_NOT_NOW */  
  
  ::XFillRectangle(mDisplay,
                   mPrintContext->GetDrawable(),
                   *mGC,
                   x,y,w,h);
  
  return NS_OK;
}

NS_IMETHODIMP 
nsRenderingContextXp :: InvertRect(const nsRect& aRect)
{
  return InvertRect(aRect.x, aRect.y, aRect.width, aRect.height);
}

NS_IMETHODIMP 
nsRenderingContextXp :: InvertRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
  if (nsnull == mTranMatrix || nsnull == mPrintContext) 
    return NS_ERROR_FAILURE;
  
  nscoord x,y,w,h;

  x = aX;
  y = aY;
  w = aWidth;
  h = aHeight;

  mTranMatrix->TransformCoord(&x,&y,&w,&h);

  // After the transform, if the numbers are huge, chop them, because
  // they're going to be converted from 32 bit to 16 bit.
  // It's all way off the screen anyway.
  ConditionRect(x,y,w,h);

  mFunction = GXxor;
  
  UpdateGC();
  
  ::XFillRectangle(mDisplay,
                   mPrintContext->GetDrawable(),
                   *mGC,
                   x,
                   y,
                   w,
                   h);
  
  mFunction = GXcopy;

  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXp::DrawPolygon(const nsPoint aPoints[], PRInt32 aNumPoints)
{
  PR_LOG(RenderingContextXpLM, PR_LOG_DEBUG, ("nsRenderingContextXp::DrawPolygon()\n"));
  if (nsnull == mTranMatrix || nsnull == mPrintContext) {
    return NS_ERROR_FAILURE;
  }
  PRInt32 i ;
  XPoint * xpoints;
  XPoint * thispoint;
  
  xpoints = (XPoint *) PR_Malloc(sizeof(XPoint) * aNumPoints);
  
  for (i = 0; i < aNumPoints; i++){
    thispoint = (xpoints+i);
    thispoint->x = aPoints[i].x;
    thispoint->y = aPoints[i].y;
    mTranMatrix->TransformCoord((PRInt32*)&thispoint->x,(PRInt32*)&thispoint->y);
  }
  
  UpdateGC();
  ::XDrawLines(mDisplay,
               mPrintContext->GetDrawable(),
               *mGC,
               xpoints, aNumPoints, CoordModeOrigin);

  PR_Free((void *)xpoints);
  
  return NS_OK;    
}

NS_IMETHODIMP
nsRenderingContextXp::FillPolygon(const nsPoint aPoints[], PRInt32 aNumPoints)
{
  PR_LOG(RenderingContextXpLM, PR_LOG_DEBUG, ("nsRenderingContextXp::FillPolygon()\n"));
  if (nsnull == mTranMatrix || nsnull == mPrintContext) {
    return NS_ERROR_FAILURE;
  }
  PRInt32 i ;
  XPoint * xpoints;
   
  xpoints = (XPoint *) PR_Malloc(sizeof(XPoint) * aNumPoints);
  
  for (i = 0; i < aNumPoints; ++i) {
    nsPoint p = aPoints[i];
    mTranMatrix->TransformCoord(&p.x, &p.y);
    xpoints[i].x = p.x;
    xpoints[i].y = p.y;
  } 
    
  UpdateGC();
  ::XFillPolygon(mDisplay,
                 mPrintContext->GetDrawable(),
                 *mGC,
                 xpoints, aNumPoints, Complex, CoordModeOrigin);
               
  PR_Free((void *)xpoints);

  return NS_OK; 
}

NS_IMETHODIMP
nsRenderingContextXp::DrawEllipse(const nsRect& aRect)
{
  PR_LOG(RenderingContextXpLM, PR_LOG_DEBUG, ("nsRenderingContextXp::DrawEllipse()\n"));
  return DrawEllipse(aRect.x, aRect.y, aRect.width, aRect.height);
}

NS_IMETHODIMP
nsRenderingContextXp::DrawEllipse(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
  PR_LOG(RenderingContextXpLM, PR_LOG_DEBUG, ("nsRenderingContextXp::DrawEllipse()\n"));
  if (nsnull == mTranMatrix || nsnull == mPrintContext) {
    return NS_ERROR_FAILURE;
  }
  nscoord x,y,w,h;
   
  x = aX; 
  y = aY;
  w = aWidth; 
  h = aHeight;
    
  mTranMatrix->TransformCoord(&x,&y,&w,&h);
    
  UpdateGC();
  ::XDrawArc(mDisplay,
             mPrintContext->GetDrawable(),
             *mGC,
             x,y,w,h, 0, 360 * 64);
  
  return NS_OK;  
}

NS_IMETHODIMP
nsRenderingContextXp::FillEllipse(const nsRect& aRect)
{
  PR_LOG(RenderingContextXpLM, PR_LOG_DEBUG, ("nsRenderingContextXp::FillEllipse()\n"));
  return FillEllipse(aRect.x, aRect.y, aRect.width, aRect.height);
}

NS_IMETHODIMP
nsRenderingContextXp::FillEllipse(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
  PR_LOG(RenderingContextXpLM, PR_LOG_DEBUG, ("nsRenderingContextXp::FillEllipse()\n"));
  if (nsnull == mTranMatrix || nsnull == mPrintContext) {
    return NS_ERROR_FAILURE;
  }
  nscoord x,y,w,h;
   
  x = aX; 
  y = aY;
  w = aWidth; 
  h = aHeight;
    
  mTranMatrix->TransformCoord(&x,&y,&w,&h);
    
  UpdateGC();

  ::XFillArc(mDisplay,
             mPrintContext->GetDrawable(),
             *mGC,
             x,y,w,h, 0, 360 * 64);
  
  return NS_OK;  
}

NS_IMETHODIMP
nsRenderingContextXp::DrawArc(const nsRect& aRect,
                                float aStartAngle, float aEndAngle)
{
  PR_LOG(RenderingContextXpLM, PR_LOG_DEBUG, ("nsRenderingContextXp::DrawArc()\n"));
  return DrawArc(aRect.x,aRect.y,aRect.width,aRect.height,aStartAngle,aEndAngle); 
}

NS_IMETHODIMP
nsRenderingContextXp::DrawArc(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight,
                                float aStartAngle, float aEndAngle)
{
  PR_LOG(RenderingContextXpLM, PR_LOG_DEBUG, ("nsRenderingContextXp::DrawArc()\n"));
  if (nsnull == mTranMatrix || nsnull == mPrintContext) {
    return NS_ERROR_FAILURE;
  }
  nscoord x,y,w,h;
   
  x = aX; 
  y = aY;
  w = aWidth; 
  h = aHeight;
    
  mTranMatrix->TransformCoord(&x,&y,&w,&h);
    
  UpdateGC();
  ::XDrawArc(mDisplay,
             mPrintContext->GetDrawable(),
             *mGC,
             x,y,w,h, NSToIntRound(aStartAngle * 64.0f),
             NSToIntRound(aEndAngle * 64.0f));
  
  return NS_OK;  
}

NS_IMETHODIMP
nsRenderingContextXp::FillArc(const nsRect& aRect,
                                float aStartAngle, float aEndAngle)
{
  PR_LOG(RenderingContextXpLM, PR_LOG_DEBUG, ("nsRenderingContextXp::FillArc()\n"));
  return FillArc(aRect.x, aRect.y, aRect.width, aRect.height, aStartAngle, aEndAngle);
}

NS_IMETHODIMP
nsRenderingContextXp::FillArc(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight,
                                float aStartAngle, float aEndAngle)
{
  PR_LOG(RenderingContextXpLM, PR_LOG_DEBUG, ("nsRenderingContextXp::FillArc()\n"));
  if (nsnull == mTranMatrix || nsnull == mPrintContext) {
    return NS_ERROR_FAILURE;
  }
  nscoord x,y,w,h;
   
  x = aX; 
  y = aY;
  w = aWidth; 
  h = aHeight;
    
  mTranMatrix->TransformCoord(&x,&y,&w,&h);
    
  UpdateGC();
  ::XFillArc(mDisplay,
             mPrintContext->GetDrawable(),
             *mGC,
             x,y,w,h, NSToIntRound(aStartAngle * 64.0f),
             NSToIntRound(aEndAngle * 64.0f));
  
  return NS_OK;  
}

NS_IMETHODIMP
nsRenderingContextXp::GetWidth(char aC, nscoord& aWidth)
{
  PR_LOG(RenderingContextXpLM, PR_LOG_DEBUG, ("nsRenderingContextXp::GetWidth()\n"));
  // Check for the very common case of trying to get the width of a single
  // space.
  if ((aC == ' ') && (nsnull != mFontMetrics)) {
    nsFontMetricsXlib* fontMetrics = (nsFontMetricsXlib*)mFontMetrics;
    return fontMetrics->GetSpaceWidth(aWidth);
  }
  return GetWidth(&aC, 1, aWidth);
}

NS_IMETHODIMP
nsRenderingContextXp::GetWidth(PRUnichar aC, nscoord& aWidth,
                                 PRInt32 *aFontID)
{
  PR_LOG(RenderingContextXpLM, PR_LOG_DEBUG, ("nsRenderingContextXp::GetWidth()\n"));
  return GetWidth(&aC, 1, aWidth, aFontID);
}

NS_IMETHODIMP
nsRenderingContextXp::GetWidth(const nsString& aString, nscoord& aWidth,
                                 PRInt32 *aFontID)
{
  PR_LOG(RenderingContextXpLM, PR_LOG_DEBUG, ("nsRenderingContextXp::GetWidth()\n"));
  return GetWidth(aString.get(), aString.Length(), aWidth, aFontID);
}

NS_IMETHODIMP
nsRenderingContextXp::GetWidth(const char* aString, nscoord& aWidth)
{
  PR_LOG(RenderingContextXpLM, PR_LOG_DEBUG, ("nsRenderingContextXp::GetWidth()\n"));
  return GetWidth(aString, strlen(aString), aWidth);
}

NS_IMETHODIMP
nsRenderingContextXp::GetWidth(const char* aString, PRUint32 aLength, nscoord& aWidth)
{
  PR_LOG(RenderingContextXpLM, PR_LOG_DEBUG, ("nsRenderingContextXp::GetWidth()\n"));
  if (0 == aLength) {
    aWidth = 0;
  }
  else {
    int rawWidth;

    if ((mCurrentFont->min_byte1 == 0) && (mCurrentFont->max_byte1 == 0))
      rawWidth = XTextWidth(mCurrentFont, (char *)aString, aLength);
    else
      rawWidth = XTextWidth16(mCurrentFont, (XChar2b *)aString, aLength / 2);

    aWidth = NSToCoordRound(rawWidth * mP2T);
  }  
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXp::GetWidth(const PRUnichar* aString, PRUint32 aLength,
                               nscoord& aWidth, PRInt32 *aFontID)
{
  PR_LOG(RenderingContextXpLM, PR_LOG_DEBUG, ("nsRenderingContextXp::GetWidth()\n"));
  if (0 == aLength) {
    aWidth = 0;
  }
  else {
    nsFontMetricsXlib* metrics = (nsFontMetricsXlib*) mFontMetrics;
    nsFontXlib *prevFont = nsnull;
    int rawWidth = 0;
    PRUint32 start = 0;
    PRUint32 i;
    for (i = 0; i < aLength; i++) {
      PRUnichar c = aString[i];
      nsFontXlib* currFont = nsnull;
      nsFontXlib** font = metrics->mLoadedFonts;
      nsFontXlib** end = &metrics->mLoadedFonts[metrics->mLoadedFontsCount];
      while (font < end) {
        if (IS_REPRESENTABLE((*font)->mMap, c)) {
          currFont = *font;
          goto FoundFont; // for speed -- avoid "if" statement
        }
        font++;
      }
      currFont = metrics->FindFont(c);
FoundFont:
      // XXX avoid this test by duplicating code -- erik
      if (prevFont) {
        if (currFont != prevFont) {
          rawWidth += prevFont->GetWidth(&aString[start], i - start);
          prevFont = currFont;
          start = i;
        }
      }
      else {
        prevFont = currFont;
        start = i;
      }
    }

    if (prevFont) {
      //                                              
      rawWidth += prevFont->GetWidth(&aString[start], i - start);
    }

    aWidth = NSToCoordRound(rawWidth * mP2T);
  }
  if (nsnull != aFontID)
    *aFontID = 0;

  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXp::DrawString(const char *aString, PRUint32 aLength,
                                   nscoord aX, nscoord aY,
                                   const nscoord* aSpacing)
{
  PR_LOG(RenderingContextXpLM, PR_LOG_DEBUG, ("nsRenderingContextXp::DrawString()\n"));
  if (0 != aLength) {
    if (mTranMatrix == nsnull)
      return NS_ERROR_FAILURE;
    if (aString == nsnull)
      return NS_ERROR_FAILURE;
    
    nscoord x = aX;
    nscoord y = aY;
    
    // Substract xFontStruct ascent since drawing specifies baseline
    if (mFontMetrics) {
      mFontMetrics->GetMaxAscent(y);
      y += aY;
    }

    UpdateGC();

    if (nsnull != aSpacing) {
      // Render the string, one character at a time...
      const char* end = aString + aLength;
      while (aString < end) {
        char ch = *aString++;
        nscoord xx = x;
        nscoord yy = y;
        mTranMatrix->TransformCoord(&xx, &yy);
        ::XDrawString(mDisplay,
                    mPrintContext->GetDrawable(),
                    *mGC,
                    xx, yy, &ch, 1);
        x += *aSpacing++;
      }
    }
    else {
      mTranMatrix->TransformCoord(&x, &y);
      XDrawString(mDisplay,
                  mPrintContext->GetDrawable(),
                  *mGC,
                  x, y, aString, aLength);
    }
  }

#ifdef XPRINT_DISABLED_CODE
  //this is no longer to be done by this API, but another
  //will take it's place that will need this code again. MMP
  if (mFontMetrics)
  {
    const nsFont *font;
    mFontMetrics->GetFont(font);
    PRUint8 deco = font->decorations;

    if (deco & NS_FONT_DECORATION_OVERLINE)
      DrawLine(aX, aY, aX + aWidth, aY);

    if (deco & NS_FONT_DECORATION_UNDERLINE)
    {
      nscoord ascent,descent;

      mFontMetrics->GetMaxAscent(ascent);
      mFontMetrics->GetMaxDescent(descent);

      DrawLine(aX, aY + ascent + (descent >> 1),
               aX + aWidth, aY + ascent + (descent >> 1));
    }

    if (deco & NS_FONT_DECORATION_LINE_THROUGH)
    {
      nscoord height;

      mFontMetrics->GetHeight(height);

      DrawLine(aX, aY + (height >> 1), aX + aWidth, aY + (height >> 1));
    }
  }
#endif /* XPRINT_DISABLED_CODE */

  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXp::DrawString(const PRUnichar *aString, PRUint32 aLength,
                                   nscoord aX, nscoord aY,
                                   PRInt32 aFontID,
                                   const nscoord* aSpacing)
{
  PR_LOG(RenderingContextXpLM, PR_LOG_DEBUG, ("nsRenderingContextXp::DrawString()\n"));
    if (aLength && mFontMetrics) {
      if (mTranMatrix == nsnull)
        return NS_ERROR_FAILURE;
      if (mPrintContext == nsnull)
        return NS_ERROR_FAILURE;
      if (aString == nsnull)
        return NS_ERROR_FAILURE;

    nscoord x = aX;
    nscoord y;

    // Substract xFontStruct ascent since drawing specifies baseline
    mFontMetrics->GetMaxAscent(y);
    y += aY;
    aY = y;

    mTranMatrix->TransformCoord(&x, &y);

    nsFontMetricsXlib* metrics = (nsFontMetricsXlib*) mFontMetrics;
    nsFontXlib* prevFont = nsnull;
    PRUint32 start = 0;
    PRUint32 i;
    for (i = 0; i < aLength; i++) {
      PRUnichar c = aString[i];
      nsFontXlib* currFont = nsnull;
      nsFontXlib** font = metrics->mLoadedFonts;
      nsFontXlib** lastFont = &metrics->mLoadedFonts[metrics->mLoadedFontsCount];
      while (font < lastFont) {
        if (IS_REPRESENTABLE((*font)->mMap, c)) {
          currFont = *font;
          goto FoundFont; // for speed -- avoid "if" statement
        }
        font++;
      }
      currFont = metrics->FindFont(c);
FoundFont:
      // XXX avoid this test by duplicating code -- erik
      if (prevFont) {
        if (currFont != prevFont) {
          if (aSpacing) {
            const PRUnichar* str = &aString[start];
            const PRUnichar* end = &aString[i];
            UpdateGC();
            while (str < end) {
              x = aX;
              y = aY;
              mTranMatrix->TransformCoord(&x, &y);
              prevFont->DrawString(this, mPrintContext, x, y, str, 1);
              aX += *aSpacing++;
              str++;
            }
          }
          else {
      UpdateGC();
      prevFont->DrawString(this, mPrintContext, x, y, &aString[start], i - start);
              x += prevFont->GetWidth(&aString[start], i -start);

          }
          prevFont = currFont;
          start = i;
        }
      }
      else {
        prevFont = currFont;
        start = i;
      }
    }

    if (prevFont) {
    UpdateGC();
      if (aSpacing) {
        const PRUnichar* str = &aString[start];
        const PRUnichar* end = &aString[i];
        while (str < end) {
          x = aX;
          y = aY;
          mTranMatrix->TransformCoord(&x, &y);
          prevFont->DrawString(this, mPrintContext, x, y, str, 1);
          aX += *aSpacing++;
          str++;
        }
      }
      else {
        prevFont->DrawString(this, mPrintContext, x, y, &aString[start], i - start);
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextXp::DrawString(const nsString& aString, nscoord aX, nscoord aY,
                                                 PRInt32 aFontID,
                                                 const nscoord* aSpacing)
{
  PR_LOG(RenderingContextXpLM, PR_LOG_DEBUG, ("nsRenderingContextXp::DrawString()\n"));
  return DrawString(aString.get(), aString.Length(),
                    aX, aY, aFontID, aSpacing);
}

NS_IMETHODIMP
nsRenderingContextXp::DrawImage(nsIImage *aImage, nscoord aX, nscoord aY)
{
  PR_LOG(RenderingContextXpLM, PR_LOG_DEBUG, ("nsRenderingContextXp::DrawImage(%d/%d)\n", 
         (int)aX, (int)aY));
  nscoord width, height;

  // we have to do this here because we are doing a transform below
  width  = NSToCoordRound(mP2T * aImage->GetWidth());
  height = NSToCoordRound(mP2T * aImage->GetHeight());
  
  return DrawImage(aImage, aX, aY, width, height);
}

NS_IMETHODIMP
nsRenderingContextXp::DrawImage(nsIImage *aImage, nscoord aX, nscoord aY,
                                  nscoord aWidth, nscoord aHeight)
{
  PR_LOG(RenderingContextXpLM, PR_LOG_DEBUG, ("nsRenderingContextXp::DrawImage(%d/%d/%d/%d)\n", 
         (int)aX, (int)aY, (int)aWidth, (int)aHeight));
  nsRect tr;
  
  tr.x      = aX;
  tr.y      = aY;
  tr.width  = aWidth;
  tr.height = aHeight;
  return DrawImage(aImage, tr);
}

NS_IMETHODIMP
nsRenderingContextXp::DrawImage(nsIImage *aImage, const nsRect& aRect)
{
  PR_LOG(RenderingContextXpLM, PR_LOG_DEBUG, ("nsRenderingContextXp::DrawImage(aRect)\n"));

  nsRect tr;
  tr = aRect;
  mTranMatrix->TransformCoord(&tr.x, &tr.y, &tr.width, &tr.height);
  UpdateGC();
  return mPrintContext->DrawImage(mGC, aImage, tr.x, tr.y, tr.width, tr.height);
}

NS_IMETHODIMP
nsRenderingContextXp::DrawImage(nsIImage *aImage, const nsRect& aSRect, const nsRect& aDRect)
{
  PR_LOG(RenderingContextXpLM, PR_LOG_DEBUG, ("nsRenderingContextXp::DrawImage(aSRect,aDRect)\n"));
  nsRect sr,dr;
  
  sr = aSRect;
  mTranMatrix->TransformCoord(&sr.x, &sr.y,
                              &sr.width, &sr.height);
  sr.x = aSRect.x;
  sr.y = aSRect.y;
  mTranMatrix->TransformNoXLateCoord(&sr.x, &sr.y);
  
  dr = aDRect;
  mTranMatrix->TransformCoord(&dr.x, &dr.y,
                              &dr.width, &dr.height);
  UpdateGC();
  
  return mPrintContext->DrawImage(mGC, aImage,
                                  sr.x, sr.y,
                                  sr.width, sr.height,
                                  dr.x, dr.y,
                                  dr.width, dr.height);
}

#ifdef USE_IMG2
#include "imgIContainer.h"
#include "gfxIImageFrame.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"

/* [noscript] void drawImage (in imgIContainer aImage, [const] in nsRect aSrcRect, [const] in nsPoint aDestPoint); */
NS_IMETHODIMP nsRenderingContextXp::DrawImage(imgIContainer *aImage, const nsRect * aSrcRect, const nsPoint * aDestPoint)
{
  PR_LOG(RenderingContextXpLM, PR_LOG_DEBUG, ("nsRenderingContextXp::DrawImage()\n"));
  nsPoint pt;
  nsRect  sr;

  pt = *aDestPoint;
  mTranMatrix->TransformCoord(&pt.x, &pt.y);

  sr = *aSrcRect;
  mTranMatrix->TransformNoXLateCoord(&sr.x, &sr.y);

  nsCOMPtr<gfxIImageFrame> iframe;
  aImage->GetCurrentFrame(getter_AddRefs(iframe));
  if (!iframe) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIImage> img(do_GetInterface(iframe));
  if (!img) return NS_ERROR_FAILURE;

  UpdateGC();

  // doesn't it seem like we should use more of the params here?
  // img->Draw(*this, surface, sr.x, sr.y, sr.width, sr.height,
  //           pt.x + sr.x, pt.y + sr.y, sr.width, sr.height);
  return mPrintContext->DrawImage(mGC, img, pt.x, pt.y, sr.width, sr.height);
}

/* [noscript] void drawScaledImage (in imgIContainer aImage, [const] in nsRect aSrcRect, [const] in nsRect aDestRect); */
NS_IMETHODIMP nsRenderingContextXp::DrawScaledImage(imgIContainer *aImage, const nsRect * aSrcRect, const nsRect * aDestRect)
{
  PR_LOG(RenderingContextXpLM, PR_LOG_DEBUG, ("nsRenderingContextXp::DrawScaledImage()\n"));
  nsRect dr;

  dr = *aDestRect;
  mTranMatrix->TransformCoord(&dr.x, &dr.y, &dr.width, &dr.height);

  nsCOMPtr<gfxIImageFrame> iframe;
  aImage->GetCurrentFrame(getter_AddRefs(iframe));
  if (!iframe) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIImage> img(do_GetInterface(iframe));
  if (!img) return NS_ERROR_FAILURE;

  // doesn't it seem like we should use more of the params here?
  // img->Draw(*this, surface, sr.x, sr.y, sr.width, sr.height, dr.x, dr.y, dr.width, dr.height);

  nsRect sr;

  sr = *aSrcRect;
  mTranMatrix->TransformCoord(&sr.x, &sr.y, &sr.width, &sr.height);
  sr.x = aSrcRect->x;
  sr.y = aSrcRect->y;
  mTranMatrix->TransformNoXLateCoord(&sr.x, &sr.y);

  UpdateGC();

  return mPrintContext->DrawImage(mGC, img,
                                  sr.x, sr.y,
                                  sr.width, sr.height,
                                  dr.x, dr.y,
                                  dr.width, dr.height);
}
#endif /* USE_IMG2 */

NS_IMETHODIMP
nsRenderingContextXp::DrawTile(nsIImage *aImage,
                                nscoord aSrcXOffset, nscoord aSrcYOffset,
                                const nsRect &aTileRect)
{
  NS_NOTREACHED("nsRenderingContextXp::DrawTile() not yet implemented");

  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXp::CopyOffScreenBits(nsDrawingSurface aSrcSurf, PRInt32 aSrcX, PRInt32 aSrcY,
                                          const nsRect &aDestBounds, PRUint32 aCopyFlags)
{
  PR_LOG(RenderingContextXpLM, PR_LOG_DEBUG, ("nsRenderingContextXp::CopyOffScreenBits()\n"));

  NS_NOTREACHED("nsRenderingContextXp::CopyOffScreenBits() not yet implemented");

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextXp::RetrieveCurrentNativeGraphicData(PRUint32 * ngd)
{
  NS_NOTREACHED("nsRenderingContextXp::RetrieveCurrentNativeGraphicData() not yet implemented");
  
  return NS_OK;
}

#ifdef MOZ_MATHML
#ifdef FONT_SWITCHING

NS_IMETHODIMP
nsRenderingContextXp::GetBoundingMetrics(const char*        aString, 
                                         PRUint32           aLength,
                                         nsBoundingMetrics& aBoundingMetrics)
{
  aBoundingMetrics.Clear();
  if (aString && 0 < aLength) {

    XCharStruct overall;
    int direction, font_ascent, font_descent;

    if ((mCurrentFont->min_byte1 == 0) && (mCurrentFont->max_byte1 == 0))
      XTextExtents(mCurrentFont, aString, aLength,
                    &direction, &font_ascent, &font_descent,
                    &overall);
    else
      XTextExtents16(mCurrentFont, (XChar2b *)aString, aLength/2,
                    &direction, &font_ascent, &font_descent,
                    &overall);

    aBoundingMetrics.leftBearing  = NSToCoordRound(float(overall.lbearing) * mP2T);
    aBoundingMetrics.rightBearing = NSToCoordRound(float(overall.rbearing) * mP2T);
    aBoundingMetrics.width        = NSToCoordRound(float(overall.width)    * mP2T);
    aBoundingMetrics.ascent       = NSToCoordRound(float(overall.ascent)   * mP2T);
    aBoundingMetrics.descent      = NSToCoordRound(float(overall.descent)  * mP2T);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXp::GetBoundingMetrics(const PRUnichar*   aString, 
                                         PRUint32           aLength,
                                         nsBoundingMetrics& aBoundingMetrics,
                                         PRInt32*           aFontID)
{
  aBoundingMetrics.Clear(); 
  if (aString && 0 < aLength) {

    nsFontMetricsXlib* metrics = (nsFontMetricsXlib*) mFontMetrics;
    nsFontXlib* prevFont = nsnull;

    nsBoundingMetrics rawbm;
    PRBool firstTime = PR_TRUE;
    PRUint32 start = 0;
    PRUint32 i;
    for (i = 0; i < aLength; i++) {
      PRUnichar c = aString[i];
      nsFontXlib* currFont = nsnull;
      nsFontXlib** font = metrics->mLoadedFonts;
      nsFontXlib** end = &metrics->mLoadedFonts[metrics->mLoadedFontsCount];
      while (font < end) {
        if (IS_REPRESENTABLE((*font)->mMap, c)) {
          currFont = *font;
          goto FoundFont; // for speed -- avoid "if" statement
        }
        font++;
      }
      currFont = metrics->FindFont(c);
    FoundFont:
      // XXX avoid this test by duplicating code -- erik
      if (prevFont) {
        if (currFont != prevFont) {
          prevFont->GetBoundingMetrics((const PRUnichar*) &aString[start],
                                       i - start, rawbm);
          if (firstTime) {
            firstTime = PR_FALSE;
            aBoundingMetrics = rawbm;
          } 
          else {
            aBoundingMetrics += rawbm;
          }
          prevFont = currFont;
          start = i;
        }
      }
      else {
        prevFont = currFont;
        start = i;
      }
    }
    
    if (prevFont) {
      prevFont->GetBoundingMetrics((const PRUnichar*) &aString[start],
                                   i - start, rawbm);
      if (firstTime) {
        aBoundingMetrics = rawbm;
      }
      else {
        aBoundingMetrics += rawbm;
      }
    }
    // convert to app units
    aBoundingMetrics.leftBearing  = NSToCoordRound(aBoundingMetrics.leftBearing  * mP2T);
    aBoundingMetrics.rightBearing = NSToCoordRound(aBoundingMetrics.rightBearing * mP2T);
    aBoundingMetrics.width        = NSToCoordRound(aBoundingMetrics.width   * mP2T);
    aBoundingMetrics.ascent       = NSToCoordRound(aBoundingMetrics.ascent  * mP2T);
    aBoundingMetrics.descent      = NSToCoordRound(aBoundingMetrics.descent * mP2T);
  }
  if (nsnull != aFontID)
    *aFontID = 0;

  return NS_OK;
}
#endif /* FONT_SWITCHING */
#endif /* MOZ_MATHML */


