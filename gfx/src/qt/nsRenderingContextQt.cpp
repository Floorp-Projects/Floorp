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
 *   Lars Knoll <knoll@kde.org>
 *   Zack Rusin <zack@kde.org>
 *	 John C. Griggs <jcgriggs@sympatico.ca>
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

#include "nsFontMetricsQt.h"
#include "nsRenderingContextQt.h"
#include "nsRegionQt.h"
#include "nsImageQt.h"
#include "nsGfxCIID.h"
#include "nsICharRepresentable.h"
#include <math.h>

#include <qwidget.h>
#include <qpaintdevicemetrics.h>
#include <qstring.h>
#include "prmem.h"

#include "qtlog.h"

#ifdef DEBUG
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
#ifdef DEBUG
  gGSCount++;
  mID = gGSID++;
  PR_LOG(gQtLogModule, QT_BASIC,
      ("GraphicsState CTOR (%p) ID: %d, Count: %d\n", this, mID, gGSCount));
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
#ifdef DEBUG
  gGSCount--;
  PR_LOG(gQtLogModule, QT_BASIC,
      ("GraphicsState DTOR (%p) ID: %d, Count: %d\n", this, mID, gGSCount));
#endif
}

NS_IMPL_THREADSAFE_ISUPPORTS1(nsRenderingContextQt, nsIRenderingContext)

static NS_DEFINE_CID(kRegionCID, NS_REGION_CID);

nsRenderingContextQt::nsRenderingContextQt()
{
#ifdef DEBUG
  gRCCount++;
  mID = gRCID++;
  PR_LOG(gQtLogModule, QT_BASIC,
      ("nsRenderingContextQt CTOR (%p) ID: %d, Count: %d\n", this, mID, gRCCount));
#endif

  mFontMetrics        = nsnull;
  mContext            = nsnull;
  mSurface            = nsnull;
  mOffscreenSurface   = nsnull;
  mCurrentColor       = NS_RGB(255,255,255);  // set it to white
  mCurrentLineStyle   = nsLineStyle_kSolid;
  mCurrentFont        = nsnull;
  mTranMatrix         = nsnull;
  mP2T                = 1.0f;
  mClipRegion         = nsnull;

  mFunction           = Qt::CopyROP;
  mQLineStyle         = Qt::SolidLine;

  PushState();
}

nsRenderingContextQt::~nsRenderingContextQt()
{
#ifdef DEBUG
  gRCCount--;
  PR_LOG(gQtLogModule, QT_BASIC,
      ("nsRenderingContextQt DTOR (%p) ID: %d, Count: %d\n", this, mID, gRCCount));
#endif
  // Destroy the State Machine
  PRInt32 cnt = mStateCache.Count();

  while (--cnt >= 0) {
      PopState();
  }

  delete mTranMatrix;

  NS_IF_RELEASE(mOffscreenSurface);
  NS_IF_RELEASE(mFontMetrics);
  NS_IF_RELEASE(mContext);
}

NS_IMETHODIMP nsRenderingContextQt::Init(nsIDeviceContext *aContext,
                                         nsIWidget *aWindow)
{
  mContext = aContext;
  NS_IF_ADDREF(mContext);

  mSurface = new nsDrawingSurfaceQt();

  QPaintDevice *pdevice = (QWidget*)aWindow->GetNativeData(NS_NATIVE_WINDOW);
  Q_ASSERT(pdevice);
  QPainter *gc = new QPainter();

  mSurface->Init(pdevice,gc);

  mOffscreenSurface = mSurface;

  NS_ADDREF(mSurface);

  return(CommonInit());
}

NS_IMETHODIMP nsRenderingContextQt::Init(nsIDeviceContext *aContext,
                                         nsIDrawingSurface* aSurface)
{
  mContext = aContext;
  NS_IF_ADDREF(mContext);

  mSurface = (nsDrawingSurfaceQt*)aSurface;
  NS_ADDREF(mSurface);
  return (CommonInit());
}

NS_IMETHODIMP nsRenderingContextQt::CommonInit()
{
  mP2T = mContext->DevUnitsToAppUnits();
  float app2dev;

  app2dev = mContext->AppUnitsToDevUnits();
  mTranMatrix->SetToIdentity();
  mTranMatrix->AddScale(app2dev,app2dev);

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextQt::GetHints(PRUint32 &aResult)
{
  PRUint32 result = 0;

  // Most X servers implement 8 bit text rendering alot faster than
  // XChar2b rendering. In addition, we can avoid the PRUnichar to
  // XChar2b conversion. So we set this bit...
  //result |= NS_RENDERING_HINT_FAST_8BIT_TEXT;

  // XXX see if we are rendering to the local display or to a remote
  // dispaly and set the NS_RENDERING_HINT_REMOTE_RENDERING accordingly
  aResult = result;
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextQt::LockDrawingSurface(PRInt32 aX,
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

NS_IMETHODIMP nsRenderingContextQt::UnlockDrawingSurface(void)
{
  PopState();
  mSurface->Unlock();
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextQt::SelectOffScreenDrawingSurface(nsIDrawingSurface* aSurface)
{
  if (nsnull == aSurface)
    mSurface = mOffscreenSurface;
  else
    mSurface = (nsDrawingSurfaceQt*)aSurface;
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextQt::GetDrawingSurface(nsIDrawingSurface* *aSurface)
{
  *aSurface = mSurface;
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextQt::Reset()
{
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextQt::GetDeviceContext(nsIDeviceContext *&aContext)
{
  NS_IF_ADDREF(mContext);
  aContext = mContext;
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextQt::PushState(void)
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

  mStateCache.AppendElement(state);
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextQt::PopState()
{
  GraphicsState *state;
  PRUint32 cnt = mStateCache.Count();

  if (cnt > 0) {
    state = (GraphicsState*)mStateCache.ElementAt(cnt - 1);
    mStateCache.RemoveElementAt(cnt - 1);

    // Assign all local attributes from the state object just popped
    if (state->mMatrix) {
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

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextQt::IsVisibleRect(const nsRect &aRect,
                                                  PRBool &aVisible)
{
  aVisible = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextQt::GetClipRect(nsRect &aRect,
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
NS_IMETHODIMP nsRenderingContextQt::CopyClipRegion(nsIRegion &aRegion)
{
  if (!mClipRegion)
    return NS_ERROR_FAILURE;

  aRegion.SetTo(*mClipRegion);
  return NS_OK;
}

void nsRenderingContextQt::CreateClipRegion()
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

NS_IMETHODIMP nsRenderingContextQt::SetClipRect(const nsRect &aRect,
                                                nsClipCombine aCombine)
{
  GraphicsState *state = nsnull;
  PRUint32 cnt = mStateCache.Count();

  if (cnt > 0) {
    state = (GraphicsState*)mStateCache.ElementAt(cnt - 1);
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

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextQt::SetClipRegion(const nsIRegion &aRegion,
                                                  nsClipCombine aCombine)
{
  PRUint32 cnt = mStateCache.Count();
  GraphicsState *state = nsnull;

  if (cnt > 0) {
    state = (GraphicsState *)mStateCache.ElementAt(cnt - 1);
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

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextQt::GetClipRegion(nsIRegion **aRegion)
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

NS_IMETHODIMP nsRenderingContextQt::SetColor(nscolor aColor)
{
  if (nsnull == mContext)
    return NS_ERROR_FAILURE;

  mCurrentColor = aColor;
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextQt::GetColor(nscolor &aColor) const
{
  aColor = mCurrentColor;
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextQt::SetFont(const nsFont &aFont, nsIAtom* aLangGroup)
{
  nsCOMPtr<nsIFontMetrics> newMetrics;
  nsresult rv = mContext->GetMetricsFor(aFont, aLangGroup, *getter_AddRefs(newMetrics));

  if (NS_SUCCEEDED(rv)) {
    rv = SetFont(newMetrics);
  }
  return rv;
}

NS_IMETHODIMP nsRenderingContextQt::SetFont(nsIFontMetrics *aFontMetrics)
{
  NS_IF_RELEASE(mFontMetrics);
  mFontMetrics = aFontMetrics;
  NS_IF_ADDREF(mFontMetrics);

  if (mFontMetrics) {
    nsFontHandle fontHandle;

    mFontMetrics->GetFontHandle(fontHandle);
    mCurrentFont = (nsFontQt*)fontHandle;
  }
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextQt::SetLineStyle(nsLineStyle aLineStyle)
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

NS_IMETHODIMP nsRenderingContextQt::GetLineStyle(nsLineStyle &aLineStyle)
{
  aLineStyle = mCurrentLineStyle;
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextQt::GetFontMetrics(nsIFontMetrics *&aFontMetrics)
{
  NS_IF_ADDREF(mFontMetrics);
  aFontMetrics = mFontMetrics;
  return NS_OK;
}

// add the passed in translation to the current translation
NS_IMETHODIMP nsRenderingContextQt::Translate(nscoord aX,nscoord aY)
{
    mTranMatrix->AddTranslation((float)aX,(float)aY);
    return NS_OK;
}

// add the passed in scale to the current scale
NS_IMETHODIMP nsRenderingContextQt::Scale(float aSx,float aSy)
{
    mTranMatrix->AddScale(aSx,aSy);
    return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextQt::GetCurrentTransform(nsTransform2D *&aTransform)
{
  aTransform = mTranMatrix;
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextQt::CreateDrawingSurface(const nsRect& aBounds,
                                           PRUint32 aSurfFlags,
                                           nsIDrawingSurface* &aSurface)
{
  if (nsnull == mSurface) {
    aSurface = nsnull;
    return NS_ERROR_FAILURE;
  }
  if (aBounds.width <= 0 || aBounds.height <= 0)
    return NS_ERROR_FAILURE;

  nsDrawingSurfaceQt *surface = new nsDrawingSurfaceQt();

  if (surface) {
    //QPainter *gc = mSurface->GetGC();
    QPainter *gc = new QPainter();
    NS_ADDREF(surface);
    //PushState();
    surface->Init(gc,aBounds.width,aBounds.height,aSurfFlags);
    //PopState();
  }
  else {
    aSurface = nsnull;
    return NS_ERROR_FAILURE;
  }
  aSurface = surface;
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextQt::DestroyDrawingSurface(nsIDrawingSurface* aDS)
{
  nsDrawingSurfaceQt *surface = (nsDrawingSurfaceQt*)aDS;

  if (surface == NULL)
    return NS_ERROR_FAILURE;

  NS_IF_RELEASE(surface);
  return NS_OK;
}

void nsRenderingContextQt::UpdateGC()
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
    pGC->setFont(mCurrentFont->font);
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

NS_IMETHODIMP nsRenderingContextQt::DrawLine(nscoord aX0,nscoord aY0,
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

NS_IMETHODIMP nsRenderingContextQt::DrawStdLine(nscoord aX0,nscoord aY0,
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

NS_IMETHODIMP nsRenderingContextQt::DrawPolyline(const nsPoint aPoints[],
                                                 PRInt32 aNumPoints)
{
  PRInt32 i;

  if (nsnull == mTranMatrix || nsnull == mSurface
      || nsnull == mSurface->GetGC() || nsnull == mSurface->GetPaintDevice())
    return NS_ERROR_FAILURE;

  QPointArray pts(aNumPoints);
  for (i = 0; i < aNumPoints; i++) {
    nsPoint p = aPoints[i];

    mTranMatrix->TransformCoord(&p.x,&p.y);
    pts.setPoint(i,p.x,p.y);
  }
  UpdateGC();

  mSurface->GetGC()->drawPolyline(pts);

  return NS_OK;
}

void nsRenderingContextQt::ConditionRect(nscoord &x,nscoord &y,
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

NS_IMETHODIMP nsRenderingContextQt::DrawRect(const nsRect &aRect)
{
  return DrawRect(aRect.x,aRect.y,aRect.width,aRect.height);
}

NS_IMETHODIMP nsRenderingContextQt::DrawRect(nscoord aX,nscoord aY,
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

NS_IMETHODIMP nsRenderingContextQt::FillRect(const nsRect &aRect)
{
  return FillRect(aRect.x,aRect.y,aRect.width,aRect.height);
}

NS_IMETHODIMP nsRenderingContextQt::FillRect(nscoord aX,nscoord aY,
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

  QColor color(NS_GET_R(mCurrentColor),
               NS_GET_G(mCurrentColor),
               NS_GET_B(mCurrentColor));

  mSurface->GetGC()->fillRect(x,y,w,h,color);
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextQt::InvertRect(const nsRect &aRect)
{
  return InvertRect(aRect.x,aRect.y,aRect.width,aRect.height);
}

NS_IMETHODIMP nsRenderingContextQt::InvertRect(nscoord aX,nscoord aY,
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
  QColor color(NS_GET_R(mCurrentColor),
               NS_GET_G(mCurrentColor),
               NS_GET_B(mCurrentColor));

  mSurface->GetGC()->fillRect(x,y,w,h,color);

  // Back to normal copy drawing mode
  mFunction = Qt::CopyROP;
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextQt::DrawPolygon(const nsPoint aPoints[],
                                                PRInt32 aNumPoints)
{
  if (nsnull == mTranMatrix || nsnull == mSurface
      || nsnull == mSurface->GetGC() || nsnull == mSurface->GetPaintDevice())
    return NS_ERROR_FAILURE;

  QPointArray pts(aNumPoints);
  for (PRInt32 i = 0; i < aNumPoints; i++) {
    nsPoint p = aPoints[i];

    mTranMatrix->TransformCoord(&p.x,&p.y);
    pts.setPoint(i,p.x,p.y);
  }
  UpdateGC();

  mSurface->GetGC()->drawPolyline(pts);

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextQt::FillPolygon(const nsPoint aPoints[],
                                                PRInt32 aNumPoints)
{
  if (nsnull == mTranMatrix || nsnull == mSurface
      || nsnull == mSurface->GetGC() || nsnull == mSurface->GetPaintDevice())
    return NS_ERROR_FAILURE;

  QPointArray pts(aNumPoints);
  for (PRInt32 i = 0; i < aNumPoints; i++) {
    nsPoint p = aPoints[i];

    mTranMatrix->TransformCoord(&p.x,&p.y);
    pts.setPoint(i,p.x,p.y);
  }
  UpdateGC();

  mSurface->GetGC()->drawPolygon(pts);

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextQt::DrawEllipse(const nsRect &aRect)
{
  return DrawEllipse(aRect.x,aRect.y,aRect.width,aRect.height);
}

NS_IMETHODIMP nsRenderingContextQt::DrawEllipse(nscoord aX,nscoord aY,
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

NS_IMETHODIMP nsRenderingContextQt::FillEllipse(const nsRect &aRect)
{
  return FillEllipse(aRect.x,aRect.y,aRect.width,aRect.height);
}

NS_IMETHODIMP nsRenderingContextQt::FillEllipse(nscoord aX,nscoord aY,
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

NS_IMETHODIMP nsRenderingContextQt::DrawArc(const nsRect &aRect,
                                            float aStartAngle,float aEndAngle)
{
  return DrawArc(aRect.x,aRect.y,aRect.width,aRect.height,
                 aStartAngle,aEndAngle);
}

NS_IMETHODIMP nsRenderingContextQt::DrawArc(nscoord aX,nscoord aY,
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

NS_IMETHODIMP nsRenderingContextQt::FillArc(const nsRect &aRect,
                                            float aStartAngle,float aEndAngle)
{
  return FillArc(aRect.x,aRect.y,aRect.width,aRect.height,
                 aStartAngle,aEndAngle);
}

NS_IMETHODIMP nsRenderingContextQt::FillArc(nscoord aX,nscoord aY,
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

NS_IMETHODIMP nsRenderingContextQt::GetWidth(char aC, nscoord &aWidth)
{
    if (!mFontMetrics)
        return NS_ERROR_FAILURE;
    if (aC == ' ') {
        aWidth = mCurrentFont->mSpaceWidth;
    } else {
        QFontMetrics fm(mCurrentFont->font);
        aWidth = NSToCoordRound(fm.width(aC) * mP2T);
    }
    return NS_OK;
}

NS_IMETHODIMP nsRenderingContextQt::GetWidth(PRUnichar aC,nscoord &aWidth,
                                             PRInt32 *aFontID)
{
    if (!mFontMetrics)
        return NS_ERROR_FAILURE;
    if (aC == ' ') {
        aWidth = mCurrentFont->mSpaceWidth;
    } else {
        QFontMetrics fm(mCurrentFont->font);
        aWidth = NSToCoordRound(fm.width(QChar(aC)) * mP2T);
    }
    return NS_OK;
}

NS_IMETHODIMP nsRenderingContextQt::GetWidth(const nsString &aString, nscoord &aWidth,PRInt32 *aFontID)
{
  return GetWidth(aString.get(), aString.Length(), aWidth, aFontID);
}

NS_IMETHODIMP nsRenderingContextQt::GetWidth(const char *aString, nscoord &aWidth)
{
  return GetWidth(aString,strlen(aString),aWidth);
}

NS_IMETHODIMP nsRenderingContextQt::GetWidth(const char *aString, PRUint32 aLength,nscoord &aWidth)
{
    if (0 == aLength) {
    aWidth = 0;
    return NS_OK;
    }
    if (nsnull == aString || nsnull == mCurrentFont)
        return NS_ERROR_FAILURE;

    QFontMetrics curFontMetrics(mCurrentFont->font);
    int rawWidth = curFontMetrics.width(QString::fromLatin1(aString, aLength));
    aWidth = NSToCoordRound(rawWidth * mP2T);
    return NS_OK;
}

NS_IMETHODIMP nsRenderingContextQt::GetWidth(const PRUnichar *aString,
                                             PRUint32 aLength,nscoord &aWidth,
                                             PRInt32 *aFontID)
{
    if (aFontID)
        *aFontID = 0;
    if (0 == aLength) {
        aWidth = 0;
        return NS_OK;
    }
    if (!aString || !mFontMetrics)
        return NS_ERROR_FAILURE;

    QConstString cstr((const QChar *)aString, aLength);
    QFontMetrics curFontMetrics(mCurrentFont->font);
    int rawWidth = curFontMetrics.width(cstr.string());
    aWidth = NSToCoordRound(rawWidth * mP2T);
    return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextQt::GetTextDimensions(const char* aString, PRUint32 aLength,
                                        nsTextDimensions& aDimensions)
{
    aDimensions.Clear();
    if (aLength == 0)
        return NS_OK;
    if (nsnull == aString)
        return NS_ERROR_FAILURE;

    QString str = QString::fromLatin1(aString, aLength);
    QFontMetrics fm(mCurrentFont->font);
    aDimensions.ascent = NSToCoordRound(fm.ascent()*mP2T);
    aDimensions.descent = NSToCoordRound(fm.descent()*mP2T);
    aDimensions.width = NSToCoordRound(fm.width(str)*mP2T);
    return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextQt::GetTextDimensions(const PRUnichar *aString,
                                        PRUint32 aLength,
                                        nsTextDimensions &aDimensions,
                                        PRInt32 *aFontID)
{
    aDimensions.Clear();
    if (0 == aLength)
        return NS_OK;
    if (nsnull == aString)
        return NS_ERROR_FAILURE;

    QConstString str((const QChar *)aString, aLength);
    QFontMetrics fm(mCurrentFont->font);
    aDimensions.ascent = NSToCoordRound(fm.ascent()*mP2T);
    aDimensions.descent = NSToCoordRound(fm.descent()*mP2T);
    aDimensions.width = NSToCoordRound(fm.width(str.string())*mP2T);

    if (nsnull != aFontID)
        *aFontID = 0;

    return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextQt::GetTextDimensions(const char*       aString,
                                        PRInt32           aLength,
                                        PRInt32           aAvailWidth,
                                        PRInt32*          aBreaks,
                                        PRInt32           aNumBreaks,
                                        nsTextDimensions& aDimensions,
                                        PRInt32&          aNumCharsFit,
                                        nsTextDimensions& aLastWordDimensions,
                                        PRInt32*          aFontID)
{
    NS_NOTREACHED("nsRenderingContextQt::GetTextDimensions not implemented\n");
    return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP
nsRenderingContextQt::GetTextDimensions(const PRUnichar*  aString,
                                        PRInt32           aLength,
                                        PRInt32           aAvailWidth,
                                        PRInt32*          aBreaks,
                                        PRInt32           aNumBreaks,
                                        nsTextDimensions& aDimensions,
                                        PRInt32&          aNumCharsFit,
                                        nsTextDimensions& aLastWordDimensions,
                                        PRInt32*          aFontID)
{
    NS_NOTREACHED("nsRenderingContextQt::GetTextDimensions not implemented\n");
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP nsRenderingContextQt::DrawString(const char *aString,
                                               PRUint32 aLength,
                                               nscoord aX, nscoord aY,
                                               const nscoord *aSpacing)
{
    if (0 == aLength)
        return NS_OK;

    if (!mCurrentFont || nsnull == mTranMatrix || nsnull == mSurface
        || nsnull == mSurface->GetGC() || nsnull == mSurface->GetPaintDevice()
        || nsnull == aString)
        return NS_ERROR_FAILURE;

    nscoord x = aX;
    nscoord y = aY;

    UpdateGC();

    QString str = QString::fromLatin1(aString, aLength);

    if (nsnull != aSpacing) {
        // Render the string, one character at a time...
        for (uint i = 0; i < aLength; ++i) {
            nscoord xx = x;
            nscoord yy = y;
            mTranMatrix->TransformCoord(&xx,&yy);
            mSurface->GetGC()->drawText(xx, yy, str, i, 1);
            x += *aSpacing++;
        }
    } else {
        mTranMatrix->TransformCoord(&x,&y);
        mSurface->GetGC()->drawText(x, y, str, aLength, QPainter::LTR);
    }
    return NS_OK;
}

NS_IMETHODIMP nsRenderingContextQt::DrawString(const PRUnichar *aString,
                                               PRUint32 aLength,
                                               nscoord aX,nscoord aY,
                                               PRInt32 aFontID,
                                               const nscoord *aSpacing)
{
    if (!aLength)
        return NS_OK;

    if (!mCurrentFont || nsnull == mTranMatrix || nsnull == mSurface
        || nsnull == mSurface->GetGC()
        || nsnull == mSurface->GetPaintDevice() || nsnull == aString)
        return NS_ERROR_FAILURE;

    nscoord x = aX;
    nscoord y = aY;

    UpdateGC();
    QConstString str((const QChar *)aString, aLength);

    if (nsnull != aSpacing) {
        // Render the string, one character at a time...
        for (uint i = 0; i < aLength; ++i) {
            nscoord xx = x;
            nscoord yy = y;
            mTranMatrix->TransformCoord(&xx,&yy);
            mSurface->GetGC()->drawText(xx, yy, str.string(), i, 1);
            x += *aSpacing++;
        }
    }
    else {
        mTranMatrix->TransformCoord(&x,&y);
        mSurface->GetGC()->drawText(x, y, str.string(), aLength, QPainter::LTR);
    }
    return NS_OK;
}

NS_IMETHODIMP nsRenderingContextQt::DrawString(const nsString &aString,
                                               nscoord aX,nscoord aY,
                                               PRInt32 aFontID,
                                               const nscoord *aSpacing)
{
  return DrawString(aString.get(), aString.Length(), aX, aY, aFontID, aSpacing);
}

NS_IMETHODIMP
nsRenderingContextQt::CopyOffScreenBits(nsIDrawingSurface* aSrcSurf,
                                        PRInt32 aSrcX,
                                        PRInt32 aSrcY,
                                        const nsRect &aDestBounds,
                                        PRUint32 aCopyFlags)
{
    PRInt32            x = aSrcX;
    PRInt32            y = aSrcY;
    nsRect             drect = aDestBounds;
    nsDrawingSurfaceQt *destsurf;

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
    QPixmap pm = *(QPixmap*)((nsDrawingSurfaceQt*)aSrcSurf)->GetPaintDevice();

    QPainter *p = destsurf->GetGC();
    UpdateGC();

//     qDebug("copy offscreen bits: aSrcX=%d, aSrcY=%d, w/h = %d/%d aCopyFlags=%d",
//            aSrcX, aSrcY, pm.width(), pm.height(), aCopyFlags);

    //XXX flags are unused. that would seem to mean that there is
    //inefficiency somewhere... MMP

    p->drawPixmap(drect.x,drect.y, pm, x, y, drect.width, drect.height);
    return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextQt::RetrieveCurrentNativeGraphicData(PRUint32 *ngd)
{
  return NS_OK;
}

#ifdef MOZ_MATHML
NS_IMETHODIMP
nsRenderingContextQt::GetBoundingMetrics(const char *aString,PRUint32 aLength,
                                         nsBoundingMetrics &aBoundingMetrics)
{
    aBoundingMetrics.Clear();
    if (0 >= aLength || !aString || !mCurrentFont)
        return(NS_ERROR_FAILURE);

    QString str = QString::fromLatin1(aString, aLength);
    QFontMetrics fm(mCurrentFont->font);
    QRect br = fm.boundingRect(str);
    aBoundingMetrics.width = NSToCoordRound(br.width() * mP2T);
    aBoundingMetrics.ascent = NSToCoordRound(-br.y() * mP2T);
    aBoundingMetrics.descent = NSToCoordRound(br.bottom() * mP2T);
    aBoundingMetrics.leftBearing = NSToCoordRound(br.x() * mP2T);
    aBoundingMetrics.rightBearing = NSToCoordRound(fm.rightBearing(str.at(aLength - 1)) * mP2T);
    return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextQt::GetBoundingMetrics(const PRUnichar *aString,
                                         PRUint32 aLength,
                                         nsBoundingMetrics &aBoundingMetrics,
                                         PRInt32 *aFontID)
{
    aBoundingMetrics.Clear();
    if (0 >= aLength || !aString || !mCurrentFont)
        return(NS_ERROR_FAILURE);

    QConstString str((const QChar *)aString, aLength);
    QFontMetrics fm(mCurrentFont->font);
    QRect br = fm.boundingRect(str.string());
    aBoundingMetrics.width = NSToCoordRound(br.width() * mP2T);
    aBoundingMetrics.ascent = NSToCoordRound(-br.y() * mP2T);
    aBoundingMetrics.descent = NSToCoordRound(br.bottom() * mP2T);
    aBoundingMetrics.leftBearing = NSToCoordRound(br.x() * mP2T);
    aBoundingMetrics.rightBearing = NSToCoordRound(fm.rightBearing(str.string().at(aLength - 1)) * mP2T);
    return NS_OK;
}
#endif /* MOZ_MATHML */
