/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 *		John C. Griggs <jcgriggs@sympatico.ca>
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

#include "nsFontMetricsQT.h"
#include "nsRenderingContextQT.h"
#include "nsRegionQT.h"
#include "nsImageQT.h"
#include "nsGfxCIID.h"
#include "nsICharRepresentable.h"
#include <math.h>
#include <qpaintdevicemetrics.h>
#include <qstring.h>
#include "prmem.h"

static NS_DEFINE_IID(kRenderingContextIID,NS_IRENDERING_CONTEXT_IID);

//JCG #define DBG_JCG 1

#ifdef DBG_JCG
PRUint32 gRCCount = 0;
PRUint32 gRCID = 0;

PRUint32 gGSCount = 0;
PRUint32 gGSID = 0;
#endif

class GraphicsState
{
public:
  GraphicsState();
  ~GraphicsState();

  nsTransform2D       *mMatrix;
  nsRect              mLocalClip;
  nsCOMPtr<nsIRegion> mClipRegion;
  nscolor             mColor;
  nsLineStyle         mLineStyle;
  nsIFontMetrics      *mFontMetrics;

private:
  PRUint32            mID;
};

GraphicsState::GraphicsState()
{
#ifdef DBG_JCG
  gGSCount++;
  mID = gGSID++;
  printf("JCG: GraphicsState CTOR (%p) ID: %d, Count: %d\n",this,mID,gGSCount);
#endif
  mMatrix      = nsnull;
  mLocalClip.x = mLocalClip.y = mLocalClip.width = mLocalClip.height = 0;
  mClipRegion  = nsnull;
  mColor       = NS_RGB(0,0,0);
  mLineStyle   = nsLineStyle_kSolid;
  mFontMetrics = nsnull;
}

GraphicsState::~GraphicsState()
{
#ifdef DBG_JCG
  gGSCount--;
  printf("JCG: GraphicsState DTOR (%p) ID: %d, Count: %d\n",this,mID,gGSCount);
#endif
}

NS_IMPL_THREADSAFE_ISUPPORTS1(nsRenderingContextQT, nsIRenderingContext)

static NS_DEFINE_CID(kRegionCID, NS_REGION_CID);

nsRenderingContextQT::nsRenderingContextQT()
{
#ifdef DBG_JCG
  gRCCount++;
  mID = gRCID++;
  printf("JCG: nsRenderingContextQT CTOR (%p) ID: %d, Count: %d\n",this,mID,gRCCount);
#endif
  NS_INIT_REFCNT();

  mFontMetrics        = nsnull;
  mContext            = nsnull;
  mSurface            = nsnull;
  mOffscreenSurface   = nsnull;
  mCurrentColor       = NS_RGB(255,255,255);  // set it to white
  mCurrentLineStyle   = nsLineStyle_kSolid;
  mCurrentFont        = nsnull;
  mTranMatrix         = nsnull;
  mP2T                = 1.0f;
  mStateCache         = new nsVoidArray();
  mClipRegion         = nsnull;

  mFunction           = Qt::CopyROP;
  mQLineStyle         = Qt::SolidLine;

  mGC                 = nsnull;
  mPaintDev           = nsnull;

  PushState();
}

nsRenderingContextQT::~nsRenderingContextQT()
{
#ifdef DBG_JCG
  gRCCount--;
  printf("JCG: nsRenderingContextQT DTOR (%p) ID: %d, Count: %d\n",this,mID,gRCCount);
#endif
  // Destroy the State Machine
  if (nsnull != mStateCache) {
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

  NS_IF_RELEASE(mOffscreenSurface);
  NS_IF_RELEASE(mFontMetrics);
  NS_IF_RELEASE(mContext);
}

NS_IMETHODIMP nsRenderingContextQT::Init(nsIDeviceContext *aContext,
                                         nsIWidget *aWindow)
{
  mContext = aContext;
  NS_IF_ADDREF(mContext);

  mSurface = new nsDrawingSurfaceQT();

  QPaintDevice *pdevice = (QPaintDevice*)aWindow->GetNativeData(NS_NATIVE_WINDOW);
  QPainter *gc = new QPainter();

  mSurface->Init(pdevice,gc);

  mOffscreenSurface = mSurface;

  NS_ADDREF(mSurface);

  return(CommonInit());
}

NS_IMETHODIMP nsRenderingContextQT::Init(nsIDeviceContext *aContext,
                                         nsDrawingSurface aSurface)
{
  mContext = aContext;
  NS_IF_ADDREF(mContext);

  mSurface = (nsDrawingSurfaceQT*)aSurface;
  NS_ADDREF(mSurface);
  return (CommonInit());
}

NS_IMETHODIMP nsRenderingContextQT::CommonInit()
{
  float app2dev;

  mContext->GetDevUnitsToAppUnits(mP2T);
  mContext->GetAppUnitsToDevUnits(app2dev);
  mTranMatrix->AddScale(app2dev,app2dev);
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextQT::GetHints(PRUint32 &aResult)
{
  PRUint32 result = 0;

  // Most X servers implement 8 bit text rendering alot faster than
  // XChar2b rendering. In addition, we can avoid the PRUnichar to
  // XChar2b conversion. So we set this bit...
  result |= NS_RENDERING_HINT_FAST_8BIT_TEXT;

  // XXX see if we are rendering to the local display or to a remote
  // dispaly and set the NS_RENDERING_HINT_REMOTE_RENDERING accordingly
  aResult = result;
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextQT::LockDrawingSurface(PRInt32 aX, 
                                                       PRInt32 aY,
                                                       PRUint32 aWidth, 
                                                       PRUint32 aHeight,
                                                       void **aBits, 
                                                       PRInt32 *aStride,
                                                       PRInt32 *aWidthBytes, 
                                                       PRUint32 aFlags)
{
  PushState();
  return mSurface->Lock(aX,aY,aWidth,aHeight,aBits,aStride,
                        aWidthBytes,aFlags);
}

NS_IMETHODIMP nsRenderingContextQT::UnlockDrawingSurface(void)
{
  PRBool clipstate;

  PopState(clipstate);
  mSurface->Unlock();
  return NS_OK;
}

NS_IMETHODIMP 
nsRenderingContextQT::SelectOffScreenDrawingSurface(nsDrawingSurface aSurface)
{
  if (nsnull == aSurface)
    mSurface = mOffscreenSurface;
  else
    mSurface = (nsDrawingSurfaceQT*)aSurface;
  return NS_OK;
}

NS_IMETHODIMP 
nsRenderingContextQT::GetDrawingSurface(nsDrawingSurface *aSurface)
{
  *aSurface = mSurface;
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextQT::Reset()
{
  return NS_OK;
}

NS_IMETHODIMP 
nsRenderingContextQT::GetDeviceContext(nsIDeviceContext *&aContext)
{
  NS_IF_ADDREF(mContext);
  aContext = mContext;
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextQT::PushState(void)
{
#ifdef USE_GS_POOL
  nsGraphicsState *state = nsGraphicsStatePool::GetNewGS();
#else
  GraphicsState *state = new GraphicsState();
#endif
  if (!state)
    return NS_ERROR_FAILURE;

  // Push into this state object, add to vector
  state->mMatrix = mTranMatrix;

  if (nsnull == mTranMatrix)
    mTranMatrix = new nsTransform2D();
  else
    mTranMatrix = new nsTransform2D(mTranMatrix);

  state->mClipRegion = mClipRegion;

  NS_IF_ADDREF(mFontMetrics);
  state->mFontMetrics = mFontMetrics;

  state->mColor = mCurrentColor;
  state->mLineStyle = mCurrentLineStyle;

  mStateCache->AppendElement(state);
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextQT::PopState(PRBool &aClipEmpty)
{
  GraphicsState *state;
  PRUint32 cnt = mStateCache->Count();

  if (cnt > 0) {
    state = (GraphicsState*)mStateCache->ElementAt(cnt - 1);
    mStateCache->RemoveElementAt(cnt - 1);

    // Assign all local attributes from the state object just popped
    if (state->mMatrix) {
      if (mTranMatrix)
        delete mTranMatrix;
      mTranMatrix = state->mMatrix;
    }
    mClipRegion = state->mClipRegion;

    if (state->mColor != mCurrentColor)
      SetColor(state->mColor);

    if (state->mLineStyle != mCurrentLineStyle)
      SetLineStyle(state->mLineStyle);

    if (state->mFontMetrics && (mFontMetrics != state->mFontMetrics))
      SetFont(state->mFontMetrics);

    // Delete this graphics state object
#ifdef USE_GS_POOL
    nsGraphicsStatePool::ReleaseGS(state);
#else
    delete state;
#endif
  }
  if (mClipRegion) {
    aClipEmpty = mClipRegion->IsEmpty();
  }
  else {
    aClipEmpty = PR_TRUE;
  }
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextQT::IsVisibleRect(const nsRect &aRect,
                                                  PRBool &aVisible)
{
  aVisible = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextQT::GetClipRect(nsRect &aRect, 
                                                PRBool &aClipValid)
{
  PRInt32 x,y,w,h;

  if (!mClipRegion)
    return NS_ERROR_FAILURE;

  if (!mClipRegion->IsEmpty()) {
    mClipRegion->GetBoundingBox(&x,&y,&w,&h);
    aRect.SetRect(x,y,w,h);
    aClipValid = PR_TRUE;
  } 
  else {
    aRect.SetRect(0,0,0,0);
    aClipValid = PR_FALSE;
  }
  return NS_OK;
}

/**
 * Fills in |aRegion| with a copy of the current clip region.
 */
NS_IMETHODIMP nsRenderingContextQT::CopyClipRegion(nsIRegion &aRegion)
{
  if (!mClipRegion)
    return NS_ERROR_FAILURE;

  aRegion.SetTo(*mClipRegion);
  return NS_OK;
}

void nsRenderingContextQT::CreateClipRegion()
{
  if (mClipRegion)
    return;

  mClipRegion = do_CreateInstance(kRegionCID);
  if (mClipRegion) {
    PRUint32 w,h;

    mSurface->GetDimensions(&w,&h);
    mClipRegion->Init();
    mClipRegion->SetTo(0,0,w,h);
  }
}

NS_IMETHODIMP nsRenderingContextQT::SetClipRect(const nsRect &aRect,
                                                nsClipCombine aCombine,
                                                PRBool &aClipEmpty)
{
  GraphicsState *state = nsnull;
  PRUint32 cnt = mStateCache->Count();

  if (cnt > 0) {
    state = (GraphicsState*)mStateCache->ElementAt(cnt - 1);
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

  nsRect trect = aRect;

  mTranMatrix->TransformCoord(&trect.x,&trect.y,
                              &trect.width,&trect.height);

  switch (aCombine) {
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
  aClipEmpty = mClipRegion->IsEmpty();
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextQT::SetClipRegion(const nsIRegion &aRegion,
                                                  nsClipCombine aCombine,
                                                  PRBool &aClipEmpty)
{
  PRUint32 cnt = mStateCache->Count();
  GraphicsState *state = nsnull;

  if (cnt > 0) {
    state = (GraphicsState *)mStateCache->ElementAt(cnt - 1);
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

  switch(aCombine) {
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
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextQT::GetClipRegion(nsIRegion **aRegion)
{
  nsresult  rv = NS_ERROR_FAILURE;

  if (!aRegion || !mClipRegion)
    return NS_ERROR_NULL_POINTER;

  if (*aRegion) {
    (*aRegion)->SetTo(*mClipRegion);
    rv = NS_OK;
  }
  else {
    nsCOMPtr<nsIRegion> newRegion = do_CreateInstance(kRegionCID,&rv);
    if (NS_SUCCEEDED(rv)) {
      newRegion->Init();
      newRegion->SetTo(*mClipRegion);
      NS_ADDREF(*aRegion = newRegion);
    }
  }
  return rv;
}

NS_IMETHODIMP nsRenderingContextQT::SetColor(nscolor aColor)
{
  if (nsnull == mContext)
    return NS_ERROR_FAILURE;

  mCurrentColor = aColor;
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextQT::GetColor(nscolor &aColor) const
{
  aColor = mCurrentColor;
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextQT::SetFont(const nsFont &aFont)
{
  nsCOMPtr<nsIFontMetrics> newMetrics;
  nsresult rv = mContext->GetMetricsFor(aFont,*getter_AddRefs(newMetrics));
  if (NS_SUCCEEDED(rv)) {
    rv = SetFont(newMetrics);
  }
  return rv;
}

NS_IMETHODIMP nsRenderingContextQT::SetFont(nsIFontMetrics *aFontMetrics)
{
  NS_IF_RELEASE(mFontMetrics);
  mFontMetrics = aFontMetrics;
  NS_IF_ADDREF(mFontMetrics);

  if (mFontMetrics) {
    nsFontHandle fontHandle;

    mFontMetrics->GetFontHandle(fontHandle);
    mCurrentFont = (nsFontQT*)fontHandle;
  }
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextQT::SetLineStyle(nsLineStyle aLineStyle)
{
  if (aLineStyle != mCurrentLineStyle) {
    switch (aLineStyle) { 
      case nsLineStyle_kSolid:
        mQLineStyle = QPen::SolidLine;
        break;
            
      case nsLineStyle_kDashed: 
        mQLineStyle = QPen::DashLine;
        break;
        
      case nsLineStyle_kDotted: 
        mQLineStyle = QPen::DotLine;
        break;

      default:
        break;
    }
    mCurrentLineStyle = aLineStyle ;
  }
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextQT::GetLineStyle(nsLineStyle &aLineStyle)
{
  aLineStyle = mCurrentLineStyle;
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextQT::GetFontMetrics(nsIFontMetrics *&aFontMetrics)
{
  NS_IF_ADDREF(mFontMetrics);
  aFontMetrics = mFontMetrics;
  return NS_OK;
}

// add the passed in translation to the current translation
NS_IMETHODIMP nsRenderingContextQT::Translate(nscoord aX,nscoord aY)
{
  mTranMatrix->AddTranslation((float)aX,(float)aY);
  return NS_OK;
}

// add the passed in scale to the current scale
NS_IMETHODIMP nsRenderingContextQT::Scale(float aSx,float aSy)
{
  mTranMatrix->AddScale(aSx,aSy);
  return NS_OK;
}

NS_IMETHODIMP 
nsRenderingContextQT::GetCurrentTransform(nsTransform2D *&aTransform)
{
  aTransform = mTranMatrix;
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextQT::CreateDrawingSurface(nsRect *aBounds,
                                           PRUint32 aSurfFlags,
                                           nsDrawingSurface &aSurface)
{
  if (nsnull == mSurface) {
    aSurface = nsnull;
    return NS_ERROR_FAILURE;
  }
  if (aBounds == NULL || aBounds->width <= 0 || aBounds->height <= 0)
    return NS_ERROR_FAILURE;

  nsDrawingSurfaceQT *surface = new nsDrawingSurfaceQT();

  if (surface) {
    QPainter *gc = new QPainter();
    NS_ADDREF(surface);
    surface->Init(gc,aBounds->width,aBounds->height,aSurfFlags);
  }
  else {
    aSurface = nsnull;
    return NS_ERROR_FAILURE;
  }
  aSurface = (nsDrawingSurface)surface;
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextQT::DestroyDrawingSurface(nsDrawingSurface aDS)
{
  nsDrawingSurfaceQT *surface = (nsDrawingSurfaceQT*)aDS;

  if (surface == NULL)
    return NS_ERROR_FAILURE;

  NS_IF_RELEASE(surface);
  return NS_OK;
}

void nsRenderingContextQT::UpdateGC()
{
  QPainter *pGC;
  QColor color(NS_GET_R(mCurrentColor),
               NS_GET_G(mCurrentColor),
               NS_GET_B(mCurrentColor));
  QPen   pen(color,0,mQLineStyle);
  QBrush brush(color);

  pGC = mSurface->GetGC();
  pGC->setPen(pen);
  pGC->setBrush(brush);
  pGC->setRasterOp(mFunction);
  if (mCurrentFont)
    pGC->setFont(*(mCurrentFont->GetQFont()));
  if (mClipRegion) {
     QRegion *rgn = nsnull;

     pGC->setClipping(TRUE);
     mClipRegion->GetNativeRegion((void*&)rgn);
     pGC->setClipRegion(*rgn);
  }
  else {
    pGC->setClipping(FALSE);
  }
}

NS_IMETHODIMP nsRenderingContextQT::DrawLine(nscoord aX0,nscoord aY0,
                                             nscoord aX1,nscoord aY1)
{
  nscoord diffX,diffY;

  if (nsnull == mTranMatrix || nsnull == mSurface
      || nsnull == mSurface->GetGC() || nsnull == mSurface->GetPaintDevice()) 
    return NS_ERROR_FAILURE;

  mTranMatrix->TransformCoord(&aX0,&aY0);
  mTranMatrix->TransformCoord(&aX1,&aY1);

  diffX = aX1 - aX0;
  diffY = aY1 - aY0;

  if (0 != diffX) {
    diffX = (diffX > 0 ? 1 : -1);
  }
  if (0 != diffY) {
    diffY = (diffY > 0 ? 1 : -1);
  }
  UpdateGC();
  mSurface->GetGC()->drawLine(aX0,aY0,aX1 - diffX,aY1 - diffY);
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextQT::DrawStdLine(nscoord aX0,nscoord aY0,
                                                nscoord aX1,nscoord aY1)
{
  nscoord diffX,diffY;

  if (nsnull == mTranMatrix || nsnull == mSurface
      || nsnull == mSurface->GetGC() || nsnull == mSurface->GetPaintDevice()) 
    return NS_ERROR_FAILURE;

  mTranMatrix->TransformCoord(&aX0,&aY0);
  mTranMatrix->TransformCoord(&aX1,&aY1);

  diffX = aX1 - aX0;
  diffY = aY1 - aY0;

  if (0 != diffX) {
    diffX = (diffX > 0 ? 1 : -1);
  }
  if (0 != diffY) {
    diffY = (diffY > 0 ? 1 : -1);
  }
  UpdateGC();
  mSurface->GetGC()->drawLine(aX0,aY0,aX1 - diffX,aY1 - diffY);
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextQT::DrawPolyline(const nsPoint aPoints[], 
                                                 PRInt32 aNumPoints)
{
  PRInt32 i;

  if (nsnull == mTranMatrix || nsnull == mSurface
      || nsnull == mSurface->GetGC() || nsnull == mSurface->GetPaintDevice()) 
    return NS_ERROR_FAILURE;

  QPointArray *pts = new QPointArray(aNumPoints);
  for (i = 0; i < aNumPoints; i++) {
    nsPoint p = aPoints[i];

    mTranMatrix->TransformCoord(&p.x,&p.y);
    pts->setPoint(i,p.x,p.y);
  }
  UpdateGC();

  mSurface->GetGC()->drawPolyline(*pts);
  delete pts;
  return NS_OK;
}

void nsRenderingContextQT::ConditionRect(nscoord &x,nscoord &y,
                                         nscoord &w,nscoord &h)
{
  if (y < -32766) {
    y = -32766;
  }
  if (y + h > 32766) {
    h  = 32766 - y;   
  }
  if (x < -32766) {
    x = -32766;
  }
  if (x + w > 32766) {
    w  = 32766 - x;
  }
}

NS_IMETHODIMP nsRenderingContextQT::DrawRect(const nsRect &aRect)
{
  return DrawRect(aRect.x,aRect.y,aRect.width,aRect.height);
}

NS_IMETHODIMP nsRenderingContextQT::DrawRect(nscoord aX,nscoord aY, 
                                             nscoord aWidth,nscoord aHeight)
{
  if (nsnull == mTranMatrix || nsnull == mSurface
      || nsnull == mSurface->GetGC() || nsnull == mSurface->GetPaintDevice()) 
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

  if (w && h) {
    UpdateGC();
    mSurface->GetGC()->drawRect(x,y,w,h);
  }
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextQT::FillRect(const nsRect &aRect)
{
  return FillRect(aRect.x,aRect.y,aRect.width,aRect.height);
}

NS_IMETHODIMP nsRenderingContextQT::FillRect(nscoord aX,nscoord aY, 
                                             nscoord aWidth,nscoord aHeight)
{
  if (nsnull == mTranMatrix || nsnull == mSurface
      || nsnull == mSurface->GetGC() || nsnull == mSurface->GetPaintDevice()) 
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
  UpdateGC();

  QColor color(NS_GET_R(mCurrentColor),NS_GET_G(mCurrentColor),
               NS_GET_B(mCurrentColor));

  mSurface->GetGC()->fillRect(x,y,w,h,color);
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextQT::InvertRect(const nsRect &aRect)
{
  return InvertRect(aRect.x,aRect.y,aRect.width,aRect.height);
}

NS_IMETHODIMP nsRenderingContextQT::InvertRect(nscoord aX,nscoord aY, 
                                               nscoord aWidth,nscoord aHeight)
{
  if (nsnull == mTranMatrix || nsnull == mSurface
      || nsnull == mSurface->GetGC() || nsnull == mSurface->GetPaintDevice()) 
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

  // Set XOR drawing mode
  mFunction = Qt::XorROP;
  UpdateGC();

  // Fill the rect
  QColor color(NS_GET_R(mCurrentColor),NS_GET_G(mCurrentColor),
               NS_GET_B(mCurrentColor));

  mSurface->GetGC()->fillRect(x,y,w,h,color);
    
  // Back to normal copy drawing mode
  mFunction = Qt::CopyROP;
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextQT::DrawPolygon(const nsPoint aPoints[],
                                                PRInt32 aNumPoints)
{
  if (nsnull == mTranMatrix || nsnull == mSurface
      || nsnull == mSurface->GetGC() || nsnull == mSurface->GetPaintDevice()) 
    return NS_ERROR_FAILURE;

  QPointArray *pts = new QPointArray(aNumPoints);
  for (PRInt32 i = 0; i < aNumPoints; i++) {
    nsPoint p = aPoints[i];

    mTranMatrix->TransformCoord(&p.x,&p.y);
    pts->setPoint(i,p.x,p.y);
  }
  UpdateGC();

  mSurface->GetGC()->drawPolyline(*pts);
  delete pts;
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextQT::FillPolygon(const nsPoint aPoints[], 
                                                PRInt32 aNumPoints)
{
  if (nsnull == mTranMatrix || nsnull == mSurface
      || nsnull == mSurface->GetGC() || nsnull == mSurface->GetPaintDevice()) 
    return NS_ERROR_FAILURE;

  QPointArray *pts = new QPointArray(aNumPoints);
  for (PRInt32 i = 0; i < aNumPoints; i++) {
    nsPoint p = aPoints[i];

    mTranMatrix->TransformCoord(&p.x,&p.y);
    pts->setPoint(i,p.x,p.y);
  }
  UpdateGC();

  mSurface->GetGC()->drawPolygon(*pts);
  delete pts;
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextQT::DrawEllipse(const nsRect &aRect)
{
  return DrawEllipse(aRect.x,aRect.y,aRect.width,aRect.height);
}

NS_IMETHODIMP nsRenderingContextQT::DrawEllipse(nscoord aX,nscoord aY, 
                                                nscoord aWidth,nscoord aHeight)
{
  if (nsnull == mTranMatrix || nsnull == mSurface
      || nsnull == mSurface->GetGC() || nsnull == mSurface->GetPaintDevice()) 
    return NS_ERROR_FAILURE;

  nscoord x,y,w,h;

  x = aX;
  y = aY;
  w = aWidth;
  h = aHeight;
  mTranMatrix->TransformCoord(&x,&y,&w,&h);
  UpdateGC();

  mSurface->GetGC()->drawEllipse(x,y,w,h);
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextQT::FillEllipse(const nsRect &aRect)
{
  return FillEllipse(aRect.x,aRect.y,aRect.width,aRect.height);
}

NS_IMETHODIMP nsRenderingContextQT::FillEllipse(nscoord aX,nscoord aY, 
                                                nscoord aWidth,nscoord aHeight)
{
  if (nsnull == mTranMatrix || nsnull == mSurface
      || nsnull == mSurface->GetGC() || nsnull == mSurface->GetPaintDevice()) 
    return NS_ERROR_FAILURE;

  nscoord x,y,w,h;

  x = aX;
  y = aY;
  w = aWidth;
  h = aHeight;
  mTranMatrix->TransformCoord(&x,&y,&w,&h);
  UpdateGC();

  mSurface->GetGC()->drawChord(x,y,w,h,0,16 * 360);
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextQT::DrawArc(const nsRect &aRect,
                                            float aStartAngle,float aEndAngle)
{
  return DrawArc(aRect.x,aRect.y,aRect.width,aRect.height,
                 aStartAngle,aEndAngle);
}

NS_IMETHODIMP nsRenderingContextQT::DrawArc(nscoord aX,nscoord aY,
                                            nscoord aWidth,nscoord aHeight,
                                            float aStartAngle,float aEndAngle)
{
  if (nsnull == mTranMatrix || nsnull == mSurface
      || nsnull == mSurface->GetGC() || nsnull == mSurface->GetPaintDevice()) 
    return NS_ERROR_FAILURE;

  nscoord x,y,w,h;

  x = aX;
  y = aY;
  w = aWidth;
  h = aHeight;
  mTranMatrix->TransformCoord(&x,&y,&w,&h);
  UpdateGC();

   mSurface->GetGC()->drawArc(x,y,w,h,NSToIntRound(aStartAngle * 16.0f),
                              NSToIntRound(aEndAngle * 16.0f));
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextQT::FillArc(const nsRect &aRect,
                                            float aStartAngle,float aEndAngle)
{
  return FillArc(aRect.x,aRect.y,aRect.width,aRect.height,
                 aStartAngle,aEndAngle);
}

NS_IMETHODIMP nsRenderingContextQT::FillArc(nscoord aX,nscoord aY,
                                            nscoord aWidth,nscoord aHeight,
                                            float aStartAngle,float aEndAngle)
{
  if (nsnull == mTranMatrix || nsnull == mSurface
      || nsnull == mSurface->GetGC() || nsnull == mSurface->GetPaintDevice()) 
    return NS_ERROR_FAILURE;

  nscoord x,y,w,h;

  x = aX;
  y = aY;
  w = aWidth;
  h = aHeight;
  mTranMatrix->TransformCoord(&x,&y,&w,&h);
  UpdateGC();

  mSurface->GetGC()->drawPie(x,y,w,h,NSToIntRound(aStartAngle * 16.0f),
                             NSToIntRound(aEndAngle * 16.0f));    
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextQT::GetWidth(char aC,nscoord &aWidth)
{
  if (aC == ' ' && nsnull != mFontMetrics) {
    nsFontMetricsQT *fontMetrics = (nsFontMetricsQT*)mFontMetrics;
    return fontMetrics->GetSpaceWidth(aWidth);
  }
  return GetWidth(&aC,1,aWidth);
}

NS_IMETHODIMP nsRenderingContextQT::GetWidth(PRUnichar aC,nscoord &aWidth,
                                             PRInt32 *aFontID)
{
  return GetWidth(&aC,1,aWidth,aFontID);
}

NS_IMETHODIMP nsRenderingContextQT::GetWidth(const nsString &aString,
                                             nscoord &aWidth,PRInt32 *aFontID)
{
  return GetWidth(aString.get(),aString.Length(),aWidth,aFontID);
}

NS_IMETHODIMP nsRenderingContextQT::GetWidth(const char *aString,
                                             nscoord &aWidth)
{
  return GetWidth(aString,strlen(aString),aWidth);
}

NS_IMETHODIMP nsRenderingContextQT::GetWidth(const char *aString,
                                             PRUint32 aLength,nscoord &aWidth)
{
  if (0 == aLength) 
    aWidth = 0;
  else if (nsnull == aString || nsnull == mCurrentFont)
    return NS_ERROR_FAILURE;
  else {
    QFontMetrics curFontMetrics(*(mCurrentFont->GetQFont()));
    int rawWidth = curFontMetrics.width(aString,aLength);

    aWidth = NSToCoordRound(rawWidth * mP2T);
  }
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextQT::GetWidth(const PRUnichar *aString,
                                             PRUint32 aLength,nscoord &aWidth,
                                             PRInt32 *aFontID)
{
  if (0 == aLength) 
    aWidth = 0;
  else if (nsnull == aString)
    return NS_ERROR_FAILURE;
  else {
    nsFontMetricsQT *metrics = (nsFontMetricsQT*)mFontMetrics;
    nsFontQT *prevFont = nsnull;
    int rawWidth = 0;
    PRUint32 start = 0;
    PRUint32 i;

    for (i = 0; i < aLength; i++) {
      PRUnichar c = aString[i];
      nsFontQT* currFont = nsnull;
      if (mCurrentFont->SupportsChar(c)) {
        currFont = mCurrentFont;
        goto FoundFont;
      }
      else {
        nsFontQT **font = metrics->mLoadedFonts;
        nsFontQT **lastFont = &metrics->mLoadedFonts[metrics->mLoadedFontsCount];
        while (font < lastFont) {
          if ((*font)->SupportsChar(c)) {
            currFont = *font;
            goto FoundFont; // for speed -- avoid "if" statement
          }
          font++;
        }
        currFont = metrics->FindFont(c);
      }
FoundFont:
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
      rawWidth += prevFont->GetWidth(&aString[start], i - start);
    }
    aWidth = NSToCoordRound(rawWidth * mP2T);
  }
  if (nsnull != aFontID)
    *aFontID = 0;

  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextQT::GetTextDimensions(const char* aString, PRUint32 aLength,
                                        nsTextDimensions& aDimensions)
{
  mFontMetrics->GetMaxAscent(aDimensions.ascent);
  mFontMetrics->GetMaxDescent(aDimensions.descent);
  return GetWidth(aString, aLength, aDimensions.width);
}

NS_IMETHODIMP
nsRenderingContextQT::GetTextDimensions(const PRUnichar *aString,
                                        PRUint32 aLength,
                                        nsTextDimensions &aDimensions,
                                        PRInt32 *aFontID)
{
  aDimensions.Clear();
  if (0 == aLength) 
    return NS_OK;
  if (nsnull == aString)
    return NS_ERROR_FAILURE;

  nsFontMetricsQT *metrics = (nsFontMetricsQT*)mFontMetrics;
  nsFontQT *prevFont = nsnull;
  int rawWidth = 0, rawAscent = 0, rawDescent = 0;
  PRUint32 start = 0;
  PRUint32 i;

  for (i = 0; i < aLength; i++) {
    PRUnichar c = aString[i];
    nsFontQT* currFont = nsnull;
    if (mCurrentFont->SupportsChar(c)) {
      currFont = mCurrentFont;
      goto FoundFont;
    }
    else {
      nsFontQT **font = metrics->mLoadedFonts;
      nsFontQT **lastFont = &metrics->mLoadedFonts[metrics->mLoadedFontsCount];
      while (font < lastFont) {
        if ((*font)->SupportsChar(c)) {
          currFont = *font;
          goto FoundFont; // for speed -- avoid "if" statement
        }
        font++;
      }
      currFont = metrics->FindFont(c);
    }
FoundFont:
    if (prevFont) {
      if (currFont != prevFont) {
        rawWidth += prevFont->GetWidth(&aString[start], i - start);
        if (rawAscent < prevFont->mFontMetrics->ascent())
          rawAscent = prevFont->mFontMetrics->ascent();
        if (rawDescent < prevFont->mFontMetrics->descent())
          rawDescent = prevFont->mFontMetrics->descent();
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
    rawWidth += prevFont->GetWidth(&aString[start], i - start);
    if (rawAscent < prevFont->mFontMetrics->ascent())
      rawAscent = prevFont->mFontMetrics->ascent();
    if (rawDescent < prevFont->mFontMetrics->descent())
      rawDescent = prevFont->mFontMetrics->descent();
  }

  aDimensions.width = NSToCoordRound(rawWidth * mP2T);
  aDimensions.ascent = NSToCoordRound(rawAscent * mP2T);
  aDimensions.descent = NSToCoordRound(rawDescent * mP2T);

  if (nsnull != aFontID)
    *aFontID = 0;

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextQT::DrawString(const char *aString,
                                               PRUint32 aLength,
                                               nscoord aX, nscoord aY,
                                               const nscoord *aSpacing)
{
  if (0 != aLength) {
    if (nsnull == mTranMatrix || nsnull == mSurface
        || nsnull == mSurface->GetGC() || nsnull == mSurface->GetPaintDevice()
         || nsnull == aString)
      return NS_ERROR_FAILURE;

    nscoord x = aX;
    nscoord y = aY;

    UpdateGC();

    if (nsnull != aSpacing) {
      // Render the string, one character at a time...
      const char *end = aString + aLength;
      while (aString < end) {
        char ch = *aString++;
        nscoord xx = x;
        nscoord yy = y;
        mTranMatrix->TransformCoord(&xx,&yy);
        QString str;
        str = ch;
        mSurface->GetGC()->drawText(xx,yy,str,1);
        x += *aSpacing++;
      }
    }
    else {
      QString qStr(aString);
      qStr[aLength] = '\0';
      mTranMatrix->TransformCoord(&x,&y);
      mSurface->GetGC()->drawText(x,y,qStr,aLength);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextQT::DrawString(const PRUnichar *aString,
                                               PRUint32 aLength,
                                               nscoord aX,nscoord aY,
                                               PRInt32 aFontID,
                                               const nscoord *aSpacing)
{
  if (aLength && mFontMetrics) {
    if (nsnull == mTranMatrix || nsnull == mSurface
        || nsnull == mSurface->GetGC()
         || nsnull == mSurface->GetPaintDevice() || nsnull == aString)
      return NS_ERROR_FAILURE;

    nscoord x = aX;
    nscoord y = aY;

    mTranMatrix->TransformCoord(&x,&y);
 
    nsFontMetricsQT *metrics = (nsFontMetricsQT*)mFontMetrics;
    nsFontQT *prevFont = nsnull;
    PRUint32 start = 0;
    PRUint32 i;
    for (i = 0; i < aLength; i++) {
      PRUnichar c = aString[i];
      nsFontQT *currFont = nsnull;

      if (mCurrentFont->SupportsChar(c)) {
        currFont = mCurrentFont;
        goto FoundFont;
      }
      else {
        nsFontQT **font = metrics->mLoadedFonts;
        nsFontQT **lastFont = &metrics->mLoadedFonts[metrics->mLoadedFontsCount];
        while (font < lastFont) {
          if ((*font)->SupportsChar(c)) {
            currFont = *font;
            goto FoundFont; // for speed -- avoid "if" statement
          }
          font++;
        }
        currFont = metrics->FindFont(c);
      }
FoundFont:
      // XXX avoid this test by duplicating code -- erik
      if (prevFont) {
        if (currFont != prevFont) {
          if (aSpacing) {
            const PRUnichar *str = &aString[start];
            const PRUnichar *end = &aString[i];
 
            // save mCurrentFont and set it so that we cache correctly
            nsFontQT *oldFont = mCurrentFont;
            mCurrentFont = prevFont;
            UpdateGC();
            while (str < end) {
              x = aX;
              y = aY;
              mTranMatrix->TransformCoord(&x,&y);
              prevFont->DrawString(this,mSurface,x,y,str,1);
              aX += *aSpacing++;
              str++;
            }
            mCurrentFont = oldFont;
          }
          else {
            nsFontQT *oldFont = mCurrentFont;
            mCurrentFont = prevFont;
            UpdateGC();
            x += prevFont->DrawString(this,mSurface,x,y,&aString[start],
                                      i - start);
            mCurrentFont = oldFont;
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
      nsFontQT *oldFont = mCurrentFont;
      mCurrentFont = prevFont;
      UpdateGC();
      if (aSpacing) {
        const PRUnichar *str = &aString[start];
        const PRUnichar *end = &aString[i];
        while (str < end) {
          x = aX;
          y = aY;
          mTranMatrix->TransformCoord(&x,&y);
          prevFont->DrawString(this,mSurface,x,y,str,1);
          aX += *aSpacing++;
          str++;
        }
      }
      else {
        prevFont->DrawString(this,mSurface,x,y,&aString[start],i - start);
      }
      mCurrentFont = oldFont;
    }
  }
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextQT::DrawString(const nsString &aString,
                                               nscoord aX,nscoord aY,
                                               PRInt32 aFontID,
                                               const nscoord *aSpacing)
{
  return DrawString(aString.get(),aString.Length(),aX,aY,aFontID,
                    aSpacing);
}

void nsRenderingContextQT::MyDrawString(QPainter *aGC,
                                        nscoord aX,nscoord aY,
                                        QString &aText)
{
  if (nsnull == aGC || aText.isEmpty() || aText.isNull()) {
    return;
  }
  aGC->drawText(aX,aY,aText);
}

NS_IMETHODIMP nsRenderingContextQT::DrawImage(nsIImage *aImage,nscoord aX,
                                              nscoord aY)
{
  nscoord width  = NSToCoordRound(mP2T * aImage->GetWidth());
  nscoord height = NSToCoordRound(mP2T * aImage->GetHeight());

  return DrawImage(aImage,aX,aY,width,height);
}

NS_IMETHODIMP nsRenderingContextQT::DrawImage(nsIImage *aImage,
                                              nscoord aX,nscoord aY,
                                              nscoord aWidth,nscoord aHeight)
{
  nsRect tr;

  tr.x      = aX;
  tr.y      = aY;
  tr.width  = aWidth;
  tr.height = aHeight;

  return DrawImage(aImage,tr);
}

NS_IMETHODIMP nsRenderingContextQT::DrawImage(nsIImage *aImage, 
                                              const nsRect &aRect)
{
  nsRect tr;

  tr = aRect;
  mTranMatrix->TransformCoord(&tr.x,&tr.y,&tr.width,&tr.height);
  UpdateGC();

  return aImage->Draw(*this,mSurface,tr.x,tr.y,tr.width,tr.height);
}

NS_IMETHODIMP nsRenderingContextQT::DrawImage(nsIImage *aImage, 
                                              const nsRect &aSRect, 
                                              const nsRect &aDRect)
{
  nsRect sr,dr;

  sr = aSRect;
  mTranMatrix ->TransformCoord(&sr.x,&sr.y,&sr.width,&sr.height);
  sr.x = aSRect.x;
  sr.y = aSRect.y;
  mTranMatrix->TransformNoXLateCoord(&sr.x, &sr.y);

  dr = aDRect;
  mTranMatrix->TransformCoord(&dr.x,&dr.y,&dr.width,&dr.height);
  UpdateGC();

  return aImage->Draw(*this,mSurface,sr.x,sr.y,sr.width,sr.height,
                      dr.x,dr.y,dr.width,dr.height);
}

/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 *	@update 3/16/00 dwc
 */
NS_IMETHODIMP 
nsRenderingContextQT::DrawTile(nsIImage *aImage,
                               nscoord aX0,nscoord aY0,
                               nscoord aX1,nscoord aY1,
                               nscoord aWidth,nscoord aHeight)
{
  nsImageQT* image = (nsImageQT*)aImage;

  mTranMatrix->TransformCoord(&aX0,&aY0,&aWidth,&aHeight);
  mTranMatrix->TransformCoord(&aX1,&aY1);
 
  nsRect srcRect(0,0,aWidth,aHeight);
  nsRect tileRect(aX0,aY0,aX1 - aX0,aY1 - aY0);
 
  if (tileRect.width > 0 && tileRect.height > 0)
    image->DrawTile(*this,mSurface,srcRect,tileRect);
 
  return NS_OK; 
}

NS_IMETHODIMP nsRenderingContextQT::DrawTile(nsIImage *aImage,
                                             nscoord aSrcXOffset,
                                             nscoord aSrcYOffset,
                                             const nsRect &aTileRect)
{
  nsImageQT* image = (nsImageQT*)aImage;
  nsRect tileRect(aTileRect);
  nsRect srcRect(0,0,aSrcXOffset,aSrcYOffset);

  mTranMatrix->TransformCoord(&srcRect.x,&srcRect.y,
                              &srcRect.width,&srcRect.height);
  mTranMatrix->TransformCoord(&tileRect.x,&tileRect.y,
                              &tileRect.width,&tileRect.height);
 
  if (tileRect.width > 0 && tileRect.height > 0)
    image->DrawTile(*this,mSurface,srcRect.width,srcRect.height,
                    tileRect);
  return NS_OK;
}

NS_IMETHODIMP 
nsRenderingContextQT::CopyOffScreenBits(nsDrawingSurface aSrcSurf,
                                        PRInt32 aSrcX, 
                                        PRInt32 aSrcY,
                                        const nsRect &aDestBounds,
                                        PRUint32 aCopyFlags)
{
  PRInt32            x = aSrcX;
  PRInt32            y = aSrcY;
  nsRect             drect = aDestBounds;
  nsDrawingSurfaceQT *destsurf;

  if (nsnull == aSrcSurf || nsnull == mTranMatrix || nsnull == mSurface
      || nsnull == mSurface->GetPaintDevice() || nsnull == mSurface->GetGC())
    return NS_ERROR_FAILURE;

  if (aCopyFlags & NS_COPYBITS_TO_BACK_BUFFER) {
    NS_ASSERTION(!(nsnull == mSurface), "no back buffer");
    destsurf = mSurface;
  }
  else
    destsurf = mOffscreenSurface;

  if (aCopyFlags & NS_COPYBITS_XFORM_SOURCE_VALUES)
    mTranMatrix->TransformCoord(&x,&y);

  if (aCopyFlags & NS_COPYBITS_XFORM_DEST_VALUES)
    mTranMatrix->TransformCoord(&drect.x,&drect.y,
                                &drect.width,&drect.height);

  //XXX flags are unused. that would seem to mean that there is
  //inefficiency somewhere... MMP
  UpdateGC();

  destsurf->GetGC()->drawPixmap(x,y, 
                                *(QPixmap*)((nsDrawingSurfaceQT*)aSrcSurf)->GetPaintDevice(), 
                                drect.x,drect.y,drect.width,drect.height);
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextQT::RetrieveCurrentNativeGraphicData(PRUint32 *ngd)
{
  return NS_OK;
}

#ifdef MOZ_MATHML
NS_IMETHODIMP
nsRenderingContextQT::GetBoundingMetrics(const char *aString,PRUint32 aLength,
                                         nsBoundingMetrics &aBoundingMetrics)
{
  aBoundingMetrics.Clear();
  if (0 >= aLength || !aString || !mCurrentFont) {
    return(NS_ERROR_FAILURE);
  }
  else {
    QChar *buf = new QChar[aLength];

    for (int i = 0; i < aLength; i++) {
      buf[i] = QChar(aString[i]);
    }
    QString qStr(buf,aLength);
    QFontMetrics qFM(*(mCurrentFont->mFont));

    aBoundingMetrics.width = qFM.width(qStr);
    aBoundingMetrics.leftBearing = qFM.leftBearing(buf[0]);
    aBoundingMetrics.rightBearing = qFM.rightBearing(buf[aLength - 1]);
    aBoundingMetrics.ascent = qFM.ascent();
    aBoundingMetrics.descent = qFM.descent();
    aBoundingMetrics.leftBearing = NSToCoordRound(aBoundingMetrics.leftBearing
                                                  * mP2T);
    aBoundingMetrics.rightBearing
       = NSToCoordRound(aBoundingMetrics.rightBearing * mP2T);
    aBoundingMetrics.width = NSToCoordRound(aBoundingMetrics.width * mP2T);
    aBoundingMetrics.ascent = NSToCoordRound(aBoundingMetrics.ascent * mP2T);
    aBoundingMetrics.descent = NSToCoordRound(aBoundingMetrics.descent * mP2T);
    delete [] buf;
  }
  return NS_OK;
}
 
NS_IMETHODIMP
nsRenderingContextQT::GetBoundingMetrics(const PRUnichar *aString,
                                         PRUint32 aLength,
                                         nsBoundingMetrics &aBoundingMetrics,
                                         PRInt32 *aFontID)
{
  aBoundingMetrics.Clear();
  if (0 < aLength) {
    if (aString == NULL)
      return NS_ERROR_FAILURE;
 
    nsFontMetricsQT *metrics = (nsFontMetricsQT*)mFontMetrics;
    nsFontQT *prevFont = nsnull;
 
    nsBoundingMetrics rawbm;
    PRBool firstTime = PR_TRUE;
    PRUint32 start = 0;
    PRUint32 i;
    for (i = 0; i < aLength; i++) {
      PRUnichar c = aString[i];
      nsFontQT *currFont = nsnull;
      nsFontQT **font = metrics->mLoadedFonts;
      nsFontQT **end = &metrics->mLoadedFonts[metrics->mLoadedFontsCount];
      while (font < end) {
        if ((*font)->SupportsChar( c)) {
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
          prevFont->GetBoundingMetrics((const PRUnichar*)&aString[start],
                                       i - start,rawbm);
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
      prevFont->GetBoundingMetrics((const PRUnichar*)&aString[start],
                                   i - start,rawbm);
      if (firstTime) {
        aBoundingMetrics = rawbm;
      }
      else {
        aBoundingMetrics += rawbm;
      }
    }
    // convert to app units
    aBoundingMetrics.leftBearing
       = NSToCoordRound(aBoundingMetrics.leftBearing * mP2T);
    aBoundingMetrics.rightBearing
       = NSToCoordRound(aBoundingMetrics.rightBearing * mP2T);
    aBoundingMetrics.width = NSToCoordRound(aBoundingMetrics.width * mP2T);
    aBoundingMetrics.ascent = NSToCoordRound(aBoundingMetrics.ascent * mP2T);
    aBoundingMetrics.descent = NSToCoordRound(aBoundingMetrics.descent * mP2T);
  }
  if (nsnull != aFontID)
    *aFontID = 0;
 
  return NS_OK;
}
#endif /* MOZ_MATHML */
