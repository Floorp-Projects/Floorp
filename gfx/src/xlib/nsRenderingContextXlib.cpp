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

#include "nsRenderingContextXlib.h"
#include "nsFontMetricsXlib.h"
#include "xlibrgb.h"
#include "prprf.h"
#include "prmem.h"

static NS_DEFINE_IID(kIDOMRenderingContextIID, NS_IDOMRENDERINGCONTEXT_IID);
static NS_DEFINE_IID(kIRenderingContextIID, NS_IRENDERING_CONTEXT_IID);
static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);

class GraphicsState
{
public:
  GraphicsState();
  ~GraphicsState();

  nsTransform2D  *mMatrix;
  nsRegionXlib   *mClipRegion;
  nscolor         mColor;
  nsLineStyle     mLineStyle;
  nsIFontMetrics *mFontMetrics;
};

GraphicsState::GraphicsState()
{
  mMatrix = nsnull;
  mClipRegion = nsnull;
  mColor = NS_RGB(0, 0, 0);
  mLineStyle = nsLineStyle_kSolid;
  mFontMetrics = nsnull;
}

GraphicsState::~GraphicsState()
{
  NS_IF_RELEASE(mClipRegion);
  NS_IF_RELEASE(mFontMetrics);
}

nsRenderingContextXlib::nsRenderingContextXlib()
{
  printf("nsRenderingContextXlib::nsRenderingContextXlib()\n");
  NS_INIT_REFCNT();
  mOffscreenSurface = nsnull;
  mRenderingSurface = nsnull;
  mContext = nsnull;
  mFontMetrics = nsnull;
  mTMatrix = nsnull;
  mP2T = 1.0f;
  mScriptObject = nsnull;
  mStateCache = new nsVoidArray();
  mCurrentFont = nsnull;
  mCurrentLineStyle = nsLineStyle_kSolid;
  mCurrentColor = NS_RGB(0, 0, 0);

  PushState();
}

nsRenderingContextXlib::~nsRenderingContextXlib()
{
  printf("nsRenderingContextXlib::~nsRenderingContextXlib()\n");
  NS_IF_RELEASE(mContext);
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
  if (mTMatrix)
    delete mTMatrix;
  NS_IF_RELEASE(mClipRegion);
  NS_IF_RELEASE(mOffscreenSurface);
  NS_IF_RELEASE(mFontMetrics);
  NS_IF_RELEASE(mContext);
}

nsresult
nsRenderingContextXlib::QueryInterface(const nsIID&  aIID, void** aInstancePtr)
{
  if (nsnull == aInstancePtr)
    return NS_ERROR_NULL_POINTER;
  if (aIID.Equals(kIRenderingContextIID))
  {
    nsIRenderingContext* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIScriptObjectOwnerIID))
  {
    nsIScriptObjectOwner* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIDOMRenderingContextIID))
  {
    nsIDOMRenderingContext* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

  if (aIID.Equals(kISupportsIID))
  {
    nsIRenderingContext* tmp = this;
    nsISupports* tmp2 = tmp;
    *aInstancePtr = (void*) tmp2;
    NS_ADDREF_THIS();
    return NS_OK;
  }

  return NS_NOINTERFACE;
}

NS_IMPL_ADDREF(nsRenderingContextXlib)
NS_IMPL_RELEASE(nsRenderingContextXlib)

NS_IMETHODIMP
nsRenderingContextXlib::Init(nsIDeviceContext* aContext, nsIWidget *aWindow)
{
  printf("nsRenderingContextXlib::Init()\n");
  mContext = aContext;
  NS_IF_ADDREF(mContext);

  mRenderingSurface = (nsDrawingSurfaceXlib *)new nsDrawingSurfaceXlib();

  if (mRenderingSurface) {
    Drawable win = (Drawable)aWindow->GetNativeData(NS_NATIVE_WINDOW);
    GC gc        = (GC)aWindow->GetNativeData(NS_NATIVE_GRAPHIC);
    mRenderingSurface->Init(win, gc);
    mOffscreenSurface = mRenderingSurface;
    NS_ADDREF(mRenderingSurface);
  }
  return (CommonInit());
}

NS_IMETHODIMP
nsRenderingContextXlib::Init(nsIDeviceContext* aContext, nsDrawingSurface aSurface)
{
  printf("nsRenderingContxtXbli::Init()\n");

  mContext = aContext;
  NS_IF_ADDREF(mContext);

  mRenderingSurface = (nsDrawingSurfaceXlib *)aSurface;

  if (nsnull != mRenderingSurface) {
    NS_ADDREF(mRenderingSurface);
  }

  return CommonInit();
}

nsresult nsRenderingContextXlib::CommonInit(void)
{
  // put common stuff in here.
  int x, y;
  unsigned int width, height, border, depth;
  Window root_win;

  XGetGeometry(gDisplay, mRenderingSurface->GetDrawable(), &root_win,
               &x, &y, &width, &height, &border, &depth);
  mClipRegion = new nsRegionXlib();
  mClipRegion->Init();
  mClipRegion->SetTo(0, 0, width, height);

  mContext->GetDevUnitsToAppUnits(mP2T);
  float app2dev;
  mContext->GetAppUnitsToDevUnits(app2dev);
  mTMatrix->AddScale(app2dev, app2dev);
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::Reset(void)
{
  printf("nsRenderingContext::Reset()\n");
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::GetDeviceContext(nsIDeviceContext *&aContext)
{
  printf("nsRenderingContext::GetDeviceContext()\n");
  NS_IF_ADDREF(mContext);
  aContext = mContext;
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::LockDrawingSurface(PRInt32 aX, PRInt32 aY,
                                           PRUint32 aWidth, PRUint32 aHeight,
                                           void **aBits,
                                           PRInt32 *aStride,
                                           PRInt32 *aWidthBytes,
                                           PRUint32 aFlags)
{
  printf("nsRenderingContextXlib::LockDrawingSurface()\n");
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::UnlockDrawingSurface(void)
{
  printf("nsRenderingContextXlib::UnlockDrawingSurface()\n");
  PRBool clipstate;
  PopState(clipstate);
  mRenderingSurface->Unlock();
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::SelectOffScreenDrawingSurface(nsDrawingSurface aSurface)
{
  printf("nsRenderingContextXlib::SelectOffScreenDrawingSurface()\n");
  if (nsnull == aSurface)
    mRenderingSurface = mOffscreenSurface;
  else 
    mRenderingSurface = (nsDrawingSurfaceXlib *)aSurface;
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::GetDrawingSurface(nsDrawingSurface *aSurface)
{
  printf("nsRenderingContextXlib::GetDrawingSurface()\n");
  *aSurface = mRenderingSurface;
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::GetHints(PRUint32& aResult)
{
  printf("nsRenderingContextXlib::GetHints()\n");
  PRUint32 result = 0;
  // Most X servers implement 8 bit text rendering alot faster than
  // XChar2b rendering. In addition, we can avoid the PRUnichar to
  // XChar2b conversion. So we set this bit...
  result |= NS_RENDERING_HINT_FAST_8BIT_TEXT;
  aResult = result;
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::PushState(void)
{
  printf("nsRenderingContextXlib::PushState()\n");
  GraphicsState *state = new GraphicsState();

  state->mMatrix = mTMatrix;

  if (nsnull == mTMatrix)
    mTMatrix = new nsTransform2D();
  else
    mTMatrix = new nsTransform2D(mTMatrix);

  if (mClipRegion) {
    NS_IF_ADDREF(mClipRegion);
    state->mClipRegion = mClipRegion;
    mClipRegion = new nsRegionXlib();
    mClipRegion->Init();
    mClipRegion->SetTo((const nsIRegion &)*(state->mClipRegion));
  }

  NS_IF_ADDREF(mFontMetrics);
  state->mFontMetrics = mFontMetrics;
  state->mColor = mCurrentColor;
  state->mLineStyle = mCurrentLineStyle;

  mStateCache->AppendElement(state);
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::PopState(PRBool &aClipState)
{
  printf("nsRenderingContextXlib::PopState()\n");

  PRUint32 cnt = mStateCache->Count();
  GraphicsState *state;
  
  if (cnt > 0) {
    state = (GraphicsState *)mStateCache->ElementAt(cnt - 1);
    mStateCache->RemoveElementAt(cnt - 1);
    
    if (mTMatrix)
      delete mTMatrix;
    mTMatrix = state->mMatrix;
    
    NS_IF_RELEASE(mClipRegion);
    
    mClipRegion = state->mClipRegion;
    mFontMetrics = state->mFontMetrics;

    if (mRenderingSurface && mClipRegion) {
      Region region;
      mClipRegion->GetNativeRegion((void *&)region);
      XSetRegion(gDisplay, mRenderingSurface->GetGC(), region);
    }

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

NS_IMETHODIMP
nsRenderingContextXlib::IsVisibleRect(const nsRect& aRect, PRBool &aVisible)
{
  printf("nsRenderingContextXlib::IsVisibleRect()\n");
  aVisible = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::SetClipRect(const nsRect& aRect, nsClipCombine aCombine, PRBool &aClipState)
{
  printf("nsRenderingContextXlib::SetClipRect()\n");
  nsRect trect = aRect;
  Region rgn;
  mTMatrix->TransformCoord(&trect.x, &trect.y,
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

  mClipRegion->GetNativeRegion((void*&)rgn);
  XSetRegion(gDisplay, mRenderingSurface->GetGC(), rgn);

  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::GetClipRect(nsRect &aRect, PRBool &aClipState)
{
  printf("nsRenderingContextXlib::GetClipRext()\n");
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
nsRenderingContextXlib::SetClipRegion(const nsIRegion& aRegion, nsClipCombine aCombine, PRBool &aClipState)
{
  printf("nsRenderingContextXlib::SetClipRegion()\n");
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
  mClipRegion->GetNativeRegion((void*&)rgn);
  XSetRegion(gDisplay, mRenderingSurface->GetGC(),rgn);

  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::GetClipRegion(nsIRegion **aRegion)
{
  printf("nsRenderingContextXlib::GetClipRegion()\n");
  nsresult  rv = NS_OK;
  
  NS_ASSERTION(!(nsnull == aRegion), "no region ptr");
  
  if (nsnull == *aRegion) {
    nsRegionXlib *rgn = new nsRegionXlib();
    
    if (nsnull != rgn) {
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
    (*aRegion)->SetTo(*mClipRegion);

  return rv;
}

NS_IMETHODIMP
nsRenderingContextXlib::SetLineStyle(nsLineStyle aLineStyle)
{
  printf("nsRenderingContextXlib::SetLineStyle()\n");
  if (aLineStyle != mCurrentLineStyle) {
    switch(aLineStyle)
      { 
      case nsLineStyle_kSolid:
        XSetLineAttributes(gDisplay, mRenderingSurface->GetGC(),
                           1, // width
                           LineSolid, // line style
                           CapNotLast,// cap style
                           JoinMiter);// join style
        break;
      case nsLineStyle_kDashed:
        {
          static char dashed[2] = {4,4};
          XSetDashes(gDisplay, mRenderingSurface->GetGC(),
                     0, dashed, 2);
        }
        break;

      case nsLineStyle_kDotted:
        {
          static char dotted[2] = {3,1};
          XSetDashes(gDisplay, mRenderingSurface->GetGC(),
                     0, dotted, 2);
        }
        break;

    default:
        break;

    }
    
    mCurrentLineStyle = aLineStyle ;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::GetLineStyle(nsLineStyle &aLineStyle)
{
  printf("nsRenderingContextXlib::GetLineStyle()\n");
  aLineStyle = mCurrentLineStyle;
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::SetColor(nscolor aColor)
{
  printf("nsRenderingContextXlib::SetColor(nscolor)\n");
  if (nsnull == mContext)
    return NS_ERROR_FAILURE;

  mCurrentColor = aColor;
  xlib_rgb_gc_set_foreground(mRenderingSurface->GetGC(), NS_RGB(NS_GET_R(aColor),
                                                                NS_GET_G(aColor),
                                                                NS_GET_B(aColor)));
  printf("Setting color to %d %d %d\n", NS_GET_R(aColor), NS_GET_G(aColor), NS_GET_B(aColor));
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::SetColor(const nsString &aColor)
{
  nscolor rgb;
  char cbuf[40];
  aColor.ToCString(cbuf, sizeof(cbuf));
  if (NS_ColorNameToRGB(cbuf, &rgb)) {
    SetColor(rgb);
  }
  else if (NS_HexToRGB(cbuf, &rgb)) {
    SetColor(rgb);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::GetColor(nscolor &aColor) const
{
  printf("nsRenderingContextXlib::GetColor()\n");
  aColor = mCurrentColor;
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::GetColor(nsString &aColor)
{
  char cbuf[40];
  PR_snprintf(cbuf, sizeof(cbuf), "#%02x%02x%02x",
              NS_GET_R(mCurrentColor),
              NS_GET_G(mCurrentColor),
              NS_GET_B(mCurrentColor));
  aColor = cbuf;
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::SetFont(const nsFont& aFont)
{
  printf("nsRenderingContextXlib::SetFont(nsFont)\n");
  NS_IF_RELEASE(mFontMetrics);
  mContext->GetMetricsFor(aFont, mFontMetrics);
  return SetFont(mFontMetrics);
}

NS_IMETHODIMP
nsRenderingContextXlib::SetFont(nsIFontMetrics *aFontMetrics)
{
  printf("nsRenderingContextXlib::SetFont(nsIFontMetrics)\n");
  NS_IF_RELEASE(mFontMetrics);
  mFontMetrics = aFontMetrics;
  NS_IF_ADDREF(mFontMetrics);

  if (mFontMetrics)
  {
    nsFontHandle  fontHandle;
    mFontMetrics->GetFontHandle(fontHandle);
    mCurrentFont = (XFontStruct *)fontHandle;
    XSetFont(gDisplay, mRenderingSurface->GetGC(), mCurrentFont->fid);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::GetFontMetrics(nsIFontMetrics *&aFontMetrics)
{
  printf("nsRenderingContextXlib::GetFontMetrics()\n");
  NS_IF_ADDREF(mFontMetrics);
  aFontMetrics = mFontMetrics;
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::Translate(nscoord aX, nscoord aY)
{
  printf("nsRenderingContextXlib::Translate()\n");
  mTMatrix->AddTranslation((float)aX,(float)aY);
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::Scale(float aSx, float aSy)
{
  printf("nsRenderingContextXlib::Scale()\n");
  mTMatrix->AddScale(aSx, aSy);
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::GetCurrentTransform(nsTransform2D *&aTransform)
{
  printf("nsRenderingContextXlib::GetCurrentTransform()\n");
  aTransform = mTMatrix;
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::CreateDrawingSurface(nsRect *aBounds, PRUint32 aSurfFlags, nsDrawingSurface &aSurface)
{
  printf("nsRenderingContextXlib::CreateDrawingSurface()\n");
  if (nsnull == mRenderingSurface) {
    aSurface = nsnull;
    return NS_ERROR_FAILURE;
  }
 
  nsDrawingSurfaceXlib *surf = new nsDrawingSurfaceXlib();

  if (surf) {
    NS_ADDREF(surf);
    surf->Init(mRenderingSurface->GetGC(),
               aBounds->width, aBounds->height, aSurfFlags);
  }
  
  aSurface = (nsDrawingSurface)surf;

  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::DestroyDrawingSurface(nsDrawingSurface aDS)
{
  printf("nsRenderingContextXlib::DestroyDrawingSurface()\n");
  nsDrawingSurfaceXlib *surf = (nsDrawingSurfaceXlib *) aDS;

  NS_IF_RELEASE(surf);

  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::DrawLine(nscoord aX0, nscoord aY0, nscoord aX1, nscoord aY1)
{
  printf("nsRenderingContextXlib::DrawLine()\n");
  if (nsnull == mTMatrix || nsnull == mRenderingSurface)
    return NS_ERROR_FAILURE;

  mTMatrix->TransformCoord(&aX0,&aY0);
  mTMatrix->TransformCoord(&aX1,&aY1);
  
  ::XDrawLine(gDisplay, mRenderingSurface->GetDrawable(),
              mRenderingSurface->GetGC(), aX0, aY0, aX1, aY1);

  return NS_OK;
}


NS_IMETHODIMP
nsRenderingContextXlib::DrawLine2(PRInt32 aX0, PRInt32 aY0,
                                  PRInt32 aX1, PRInt32 aY1)
{
  printf("nsRenderingContextXlib::DrawLine2()\n");
  return DrawLine(aX0, aY0, aX1, aY1);
}


NS_IMETHODIMP
nsRenderingContextXlib::DrawPolyline(const nsPoint aPoints[], PRInt32 aNumPoints)
{
  printf("nsRenderingContextXlib::DrawPolyLine()\n");
  if (nsnull == mTMatrix || nsnull == mRenderingSurface) {
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
    mTMatrix->TransformCoord((PRInt32*)&thispoint->x,(PRInt32*)&thispoint->y);
  }

  ::XDrawLines(gDisplay,
               mRenderingSurface->GetDrawable(),
               mRenderingSurface->GetGC(),
               xpoints, aNumPoints, CoordModeOrigin);

  PR_Free((void *)xpoints);

  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::DrawRect(const nsRect& aRect)
{
  printf("nsRenderingContextXlib::DrawRext()\n");
  return DrawRect(aRect.x, aRect.y, aRect.width, aRect.height);
}

NS_IMETHODIMP
nsRenderingContextXlib::DrawRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
  printf("nsRenderingContextXlib::DrawRect()\n");
  if (nsnull == mTMatrix || nsnull == mRenderingSurface) {
    return NS_ERROR_FAILURE;
  }

  nscoord x,y,w,h; 
  
  x = aX;
  y = aY; 
  w = aWidth;
  h = aHeight;
    
  mTMatrix->TransformCoord(&x,&y,&w,&h);
    
  ::XDrawRectangle(gDisplay,
                   mRenderingSurface->GetDrawable(),
                   mRenderingSurface->GetGC(),
                   x,y,w,h);

  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::FillRect(const nsRect& aRect)
{
  printf("nsRenderingContextXlib::FillRect()\n");
  printf("About to fill rect %d %d %d %d\n",
         aRect.x, aRect.y, aRect.width, aRect.height);
  return FillRect(aRect.x, aRect.y, aRect.width, aRect.height);
}

NS_IMETHODIMP
nsRenderingContextXlib::FillRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
  printf("nsRenderingContextXlib::FillRect()\n");
  if (nsnull == mTMatrix || nsnull == mRenderingSurface) {
    return NS_ERROR_FAILURE;
  }
  nscoord x,y,w,h;
  x = aX;
  y = aY;
  w = aWidth;
  h = aHeight;

  mTMatrix->TransformCoord(&x,&y,&w,&h);
  ::XFillRectangle(gDisplay,
                   mRenderingSurface->GetDrawable(),
                   mRenderingSurface->GetGC(),
                   x,y,w,h);
  
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::DrawPolygon(const nsPoint aPoints[], PRInt32 aNumPoints)
{
  printf("nsRenderingContextXlib::DrawPolygon()\n");
  if (nsnull == mTMatrix || nsnull == mRenderingSurface) {
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
    mTMatrix->TransformCoord((PRInt32*)&thispoint->x,(PRInt32*)&thispoint->y);
  }
  
  ::XDrawLines(gDisplay,
               mRenderingSurface->GetDrawable(),
               mRenderingSurface->GetGC(),
               xpoints, aNumPoints, CoordModeOrigin);

  PR_Free((void *)xpoints);
  
  return NS_OK;    
}

NS_IMETHODIMP
nsRenderingContextXlib::FillPolygon(const nsPoint aPoints[], PRInt32 aNumPoints)
{
  printf("nsRenderingContextXlib::FillPolygon()\n");
  if (nsnull == mTMatrix || nsnull == mRenderingSurface) {
    return NS_ERROR_FAILURE;
  }
  PRInt32 i ;
  XPoint * xpoints;
  XPoint * thispoint;
  nscoord x,y;
   
  xpoints = (XPoint *) PR_Malloc(sizeof(XPoint) * aNumPoints);
  
  for (i = 0; i < aNumPoints; i++){
    thispoint = (xpoints+i);
    x = aPoints[i].x;
    y = aPoints[i].y;
    mTMatrix->TransformCoord(&x,&y);
    thispoint->x = x;
    thispoint->y = y;
  } 
    
  ::XFillPolygon(gDisplay,
                 mRenderingSurface->GetDrawable(),
                 mRenderingSurface->GetGC(),
                 xpoints, aNumPoints, Convex, CoordModeOrigin);
               
  PR_Free((void *)xpoints);

  return NS_OK; 
}

NS_IMETHODIMP
nsRenderingContextXlib::DrawEllipse(const nsRect& aRect)
{
  printf("nsRenderingContextXlib::DrawEllipse()\n");
  return DrawEllipse(aRect.x, aRect.y, aRect.width, aRect.height);
}

NS_IMETHODIMP
nsRenderingContextXlib::DrawEllipse(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
  printf("nsRenderingContextXlib::DrawEllipse()\n");
  if (nsnull == mTMatrix || nsnull == mRenderingSurface) {
    return NS_ERROR_FAILURE;
  }
  nscoord x,y,w,h;
   
  x = aX; 
  y = aY;
  w = aWidth; 
  h = aHeight;
    
  mTMatrix->TransformCoord(&x,&y,&w,&h);
    
  ::XDrawArc(gDisplay,
             mRenderingSurface->GetDrawable(),
             mRenderingSurface->GetGC(),
             x,y,w,h, 0, 360 * 64);
  
  return NS_OK;  
}

NS_IMETHODIMP
nsRenderingContextXlib::FillEllipse(const nsRect& aRect)
{
  printf("nsRenderingContextXlib::FillEllipse()\n");
  return FillEllipse(aRect.x, aRect.y, aRect.width, aRect.height);
}

NS_IMETHODIMP
nsRenderingContextXlib::FillEllipse(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
  printf("nsRenderingContextXlib::FillEllipse()\n");
  if (nsnull == mTMatrix || nsnull == mRenderingSurface) {
    return NS_ERROR_FAILURE;
  }
  nscoord x,y,w,h;
   
  x = aX; 
  y = aY;
  w = aWidth; 
  h = aHeight;
    
  mTMatrix->TransformCoord(&x,&y,&w,&h);
    
  ::XFillArc(gDisplay,
             mRenderingSurface->GetDrawable(),
             mRenderingSurface->GetGC(),
             x,y,w,h, 0, 360 * 64);
  
  return NS_OK;  
}

NS_IMETHODIMP
nsRenderingContextXlib::DrawArc(const nsRect& aRect,
                                float aStartAngle, float aEndAngle)
{
  printf("nsRenderingContextXlib::DrawArc()\n");
  return DrawArc(aRect.x,aRect.y,aRect.width,aRect.height,aStartAngle,aEndAngle); 
}

NS_IMETHODIMP
nsRenderingContextXlib::DrawArc(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight,
                                float aStartAngle, float aEndAngle)
{
  printf("nsRenderingContextXlib::DrawArc()\n");
  if (nsnull == mTMatrix || nsnull == mRenderingSurface) {
    return NS_ERROR_FAILURE;
  }
  nscoord x,y,w,h;
   
  x = aX; 
  y = aY;
  w = aWidth; 
  h = aHeight;
    
  mTMatrix->TransformCoord(&x,&y,&w,&h);
    
  ::XDrawArc(gDisplay,
             mRenderingSurface->GetDrawable(),
             mRenderingSurface->GetGC(),
             x,y,w,h, NSToIntRound(aStartAngle * 64.0f),
             NSToIntRound(aEndAngle * 64.0f));
  
  return NS_OK;  
}

NS_IMETHODIMP
nsRenderingContextXlib::FillArc(const nsRect& aRect,
                                float aStartAngle, float aEndAngle)
{
  printf("nsRenderingContextXlib::FillArc()\n");
  return FillArc(aRect.x, aRect.y, aRect.width, aRect.height, aStartAngle, aEndAngle);
}

NS_IMETHODIMP
nsRenderingContextXlib::FillArc(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight,
                                float aStartAngle, float aEndAngle)
{
  printf("nsRenderingContextXlib::FillArc()\n");
  if (nsnull == mTMatrix || nsnull == mRenderingSurface) {
    return NS_ERROR_FAILURE;
  }
  nscoord x,y,w,h;
   
  x = aX; 
  y = aY;
  w = aWidth; 
  h = aHeight;
    
  mTMatrix->TransformCoord(&x,&y,&w,&h);
    
  ::XFillArc(gDisplay,
             mRenderingSurface->GetDrawable(),
             mRenderingSurface->GetGC(),
             x,y,w,h, NSToIntRound(aStartAngle * 64.0f),
             NSToIntRound(aEndAngle * 64.0f));
  
  return NS_OK;  
}

NS_IMETHODIMP
nsRenderingContextXlib::GetWidth(char aC, nscoord& aWidth)
{
  printf("nsRenderingContextXlib::GetWidth()\n");
  return GetWidth(&aC, 1, aWidth);
}

NS_IMETHODIMP
nsRenderingContextXlib::GetWidth(PRUnichar aC, nscoord& aWidth,
                                 PRInt32 *aFontID)
{
  printf("nsRenderingContextXlib::GetWidth()\n");
  return GetWidth(&aC, 1, aWidth, aFontID);
}

NS_IMETHODIMP
nsRenderingContextXlib::GetWidth(const nsString& aString, nscoord& aWidth,
                                 PRInt32 *aFontID)
{
  printf("nsRenderingContextXlib::GetWidth()\n");
  return GetWidth(aString.GetUnicode(), aString.Length(), aWidth, aFontID);
}

NS_IMETHODIMP
nsRenderingContextXlib::GetWidth(const char* aString, nscoord& aWidth)
{
  printf("nsRenderingContextXlib::GetWidth()\n");
  return GetWidth(aString, strlen(aString), aWidth);
}

NS_IMETHODIMP
nsRenderingContextXlib::GetWidth(const char* aString, PRUint32 aLength, nscoord& aWidth)
{
  printf("nsRenderingContextXlib::GetWidth()\n");
  if (0 == aLength) {
    aWidth = 0;
  }
  else {
    // XXX fix this...
    int rawWidth = XTextWidth16(mCurrentFont, (XChar2b *)aString, aLength / 2);
    aWidth = NSToCoordRound(rawWidth * mP2T);
  }  
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::GetWidth(const PRUnichar* aString, PRUint32 aLength,
                                 nscoord& aWidth, PRInt32 *aFontID)
{
  printf("nsRenderingContextXlib::GetWidth()\n");
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
        if (FONT_HAS_GLYPH((*font)->mMap, c)) {
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
          rawWidth += nsFontMetricsXlib::GetWidth(prevFont, &aString[start],
                                                  i - start);
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
      rawWidth += nsFontMetricsXlib::GetWidth(prevFont, &aString[start],
                                              i - start);
    }

    aWidth = NSToCoordRound(rawWidth * mP2T);
  }
  if (nsnull != aFontID)
    *aFontID = 0;

  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::DrawString(const char *aString, PRUint32 aLength,
                                   nscoord aX, nscoord aY,
                                   const nscoord* aSpacing)
{
  printf("nsRenderingContextXlib::DrawString()\n");
  if (0 != aLength) {
    if (mTMatrix == nsnull)
      return NS_ERROR_FAILURE;
    if (mRenderingSurface == nsnull)
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

    if (nsnull != aSpacing) {
      // Render the string, one character at a time...
      const char* end = aString + aLength;
      while (aString < end) {
        char ch = *aString++;
        nscoord xx = x;
        nscoord yy = y;
        mTMatrix->TransformCoord(&xx, &yy);
        XDrawString(gDisplay,
                    mRenderingSurface->GetDrawable(),
                    mRenderingSurface->GetGC(),
                    xx, yy, &ch, 1);
        x += *aSpacing++;
      }
    }
    else {
      mTMatrix->TransformCoord(&x, &y);
      XDrawString(gDisplay,
                  mRenderingSurface->GetDrawable(),
                  mRenderingSurface->GetGC(),
                  x, y, aString, aLength);
    }
  }

#if 0
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
#endif


  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::DrawString(const PRUnichar *aString, PRUint32 aLength,
                                   nscoord aX, nscoord aY,
                                   PRInt32 aFontID,
                                   const nscoord* aSpacing)
{
  printf("nsRenderingContextXlib::DrawString()\n");
    if (aLength && mFontMetrics) {
      if (mTMatrix == nsnull)
        return NS_ERROR_FAILURE;
      if (mRenderingSurface == nsnull)
        return NS_ERROR_FAILURE;
      if (aString == nsnull)
        return NS_ERROR_FAILURE;

    nscoord x = aX;
    nscoord y;

    // Substract xFontStruct ascent since drawing specifies baseline
    mFontMetrics->GetMaxAscent(y);
    y += aY;
    aY = y;

    mTMatrix->TransformCoord(&x, &y);

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
        if (FONT_HAS_GLYPH((*font)->mMap, c)) {
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
	    while (str < end) {
	      x = aX;
	      y = aY;
              mTMatrix->TransformCoord(&x, &y);
              nsFontMetricsXlib::DrawString(mRenderingSurface, prevFont, x, y, str, 1);
	      aX += *aSpacing++;
	      str++;
	    }
	  }
	  else {
            nsFontMetricsXlib::DrawString(mRenderingSurface, prevFont, x, y,
	      &aString[start], i - start);
            x += nsFontMetricsXlib::GetWidth(prevFont, &aString[start],
	      i - start);
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
      if (aSpacing) {
	const PRUnichar* str = &aString[start];
	const PRUnichar* end = &aString[i];
	while (str < end) {
	  x = aX;
	  y = aY;
          mTMatrix->TransformCoord(&x, &y);
          nsFontMetricsXlib::DrawString(mRenderingSurface, prevFont, x, y, str, 1);
	  aX += *aSpacing++;
	  str++;
	}
      }
      else {
        nsFontMetricsXlib::DrawString(mRenderingSurface, prevFont, x, y, &aString[start],
	  i - start);
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextXlib::DrawString(const nsString& aString, nscoord aX, nscoord aY,
                                                 PRInt32 aFontID,
                                                 const nscoord* aSpacing)
{
  printf("nsRenderingContextXlib::DrawString()\n");
  return DrawString(aString.GetUnicode(), aString.Length(),
                    aX, aY, aFontID, aSpacing);
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::DrawImage(nsIImage *aImage, nscoord aX, nscoord aY)
{
  printf("nsRenderingContextXlib::DrawImage()\n");
  nscoord width, height;

  // we have to do this here because we are doing a transform below
  width = NSToCoordRound(mP2T * aImage->GetWidth());
  height = NSToCoordRound(mP2T * aImage->GetHeight());
  
  return DrawImage(aImage, aX, aY, width, height);
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::DrawImage(nsIImage *aImage, nscoord aX, nscoord aY,
                                  nscoord aWidth, nscoord aHeight)
{
  printf("nsRenderingContextXlib::DrawImage()\n");
  nscoord x, y, w, h;
  
  x = aX;
  y = aY;
  w = aWidth;
  h = aHeight;

  mTMatrix->TransformCoord(&x, &y, &w, &h);
  
  return aImage->Draw(*this, mRenderingSurface,
                      x, y, w, h);
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::DrawImage(nsIImage *aImage, const nsRect& aRect)
{
  printf("nsRenderingContextXlib::DrawImage()\n");
  return DrawImage(aImage,
                   aRect.x,
                   aRect.y,
                   aRect.width,
                   aRect.height);
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::DrawImage(nsIImage *aImage, const nsRect& aSRect, const nsRect& aDRect)
{
  printf("nsRenderingContextXlib::DrawImage()\n");
  nsRect	sr,dr;
  
  sr = aSRect;
  mTMatrix->TransformCoord(&sr.x, &sr.y,
                           &sr.width, &sr.height);
  
  dr = aDRect;
  mTMatrix->TransformCoord(&dr.x, &dr.y,
                           &dr.width, &dr.height);
  
  return aImage->Draw(*this, mRenderingSurface,
                      sr.x, sr.y,
                      sr.width, sr.height,
                      dr.x, dr.y,
                      dr.width, dr.height);
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::CopyOffScreenBits(nsDrawingSurface aSrcSurf, PRInt32 aSrcX, PRInt32 aSrcY,
                                          const nsRect &aDestBounds, PRUint32 aCopyFlags)
{
  printf("nsRenderingContextXlib::CopyOffScreenBits()\n");
  PRInt32               srcX = aSrcX;
  PRInt32               srcY = aSrcY;
  nsRect                drect = aDestBounds;
  nsDrawingSurfaceXlib  *destsurf;

  if (aSrcSurf == nsnull)
    return NS_ERROR_FAILURE;
  if (mTMatrix == nsnull)
    return NS_ERROR_FAILURE;
  if (mRenderingSurface == nsnull)
    return NS_ERROR_FAILURE;

  if (aCopyFlags & NS_COPYBITS_TO_BACK_BUFFER) {
    NS_ASSERTION(!(nsnull == mRenderingSurface), "no back buffer");
    destsurf = mRenderingSurface;
  }
  else
    destsurf = mOffscreenSurface;
  
  if (aCopyFlags & NS_COPYBITS_XFORM_SOURCE_VALUES)
    mTMatrix->TransformCoord(&srcX, &srcY);
  
  if (aCopyFlags & NS_COPYBITS_XFORM_DEST_VALUES)
    mTMatrix->TransformCoord(&drect.x, &drect.y, &drect.width, &drect.height);
  
  //XXX flags are unused. that would seem to mean that there is
  //inefficiency somewhere... MMP

  XCopyArea(gDisplay,
            ((nsDrawingSurfaceXlib *)aSrcSurf)->GetDrawable(),
            destsurf->GetDrawable(),
            ((nsDrawingSurfaceXlib *)aSrcSurf)->GetGC(),
            srcX, srcY,
            drect.width, drect.height,
            drect.x, drect.height);

  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::GetScriptObject(nsIScriptContext *aContext, void** aScriptObject)
{
  printf("nsRenderingContextXlib::GetScriptObject()\n");
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::SetScriptObject(void* aScriptObject)
{
  printf("nsRenderingContextXlib::SetScriptObject()\n");
  return NS_OK;
}

