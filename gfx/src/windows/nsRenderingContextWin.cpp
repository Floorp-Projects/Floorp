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
 *   Roger B. Sidje <rbs@maths.uq.edu.au>
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

#include "nsRenderingContextWin.h"
#include "nsICharRepresentable.h"
#include "nsFontMetricsWin.h"
#include "nsRegionWin.h"
#include <math.h>
#include "nsDeviceContextWin.h"
#include "prprf.h"
#include "nsDrawingSurfaceWin.h"
#include "nsGfxCIID.h"

//#define GFX_DEBUG

#ifdef GFX_DEBUG
  #define BREAK_TO_DEBUGGER           DebugBreak()
#else   
  #define BREAK_TO_DEBUGGER
#endif  

#ifdef GFX_DEBUG
  #define VERIFY(exp)                 ((exp) ? 0: (GetLastError(), BREAK_TO_DEBUGGER))
#else   // !_DEBUG
  #define VERIFY(exp)                 (exp)
#endif  // !_DEBUG


static NS_DEFINE_IID(kIRenderingContextIID, NS_IRENDERING_CONTEXT_IID);
static NS_DEFINE_IID(kIRenderingContextWinIID, NS_IRENDERING_CONTEXT_WIN_IID);
static NS_DEFINE_IID(kIDrawingSurfaceIID, NS_IDRAWING_SURFACE_IID);
static NS_DEFINE_IID(kDrawingSurfaceCID, NS_DRAWING_SURFACE_CID);

#define FLAG_CLIP_VALID       0x0001
#define FLAG_CLIP_CHANGED     0x0002
#define FLAG_LOCAL_CLIP_VALID 0x0004

#define FLAGS_ALL             (FLAG_CLIP_VALID | FLAG_CLIP_CHANGED | FLAG_LOCAL_CLIP_VALID)

// Macro for creating a palette relative color if you have a COLORREF instead
// of the reg, green, and blue values. The color is color-matches to the nearest
// in the current logical palette. This has no effect on a non-palette device
#define PALETTERGB_COLORREF(c)  (0x02000000 | (c))

typedef struct lineddastructtag
{
   int   nDottedPixel;
   HDC   dc;
   COLORREF crColor;
} lineddastruct;

void CALLBACK LineDDAFunc(int x,int y,LONG lData)
{
  lineddastruct * dda_struct = (lineddastruct *) lData;
  
  dda_struct->nDottedPixel ^= 1; 

  if (dda_struct->nDottedPixel)
    SetPixel(dda_struct->dc, x, y, dda_struct->crColor);
}   



class GraphicsState
{
public:
  GraphicsState();
  GraphicsState(GraphicsState &aState);
  ~GraphicsState();

  GraphicsState   *mNext;
  nsTransform2D   mMatrix;
  nsRect          mLocalClip;
  HRGN            mClipRegion;
  nscolor         mColor;
  COLORREF        mColorREF;
  nsIFontMetrics  *mFontMetrics;
  HPEN            mSolidPen;
  HPEN            mDashedPen;
  HPEN            mDottedPen;
  PRInt32         mFlags;
  nsLineStyle     mLineStyle;
};

GraphicsState :: GraphicsState()
{
  mNext = nsnull;
  mMatrix.SetToIdentity();  
  mLocalClip.x = mLocalClip.y = mLocalClip.width = mLocalClip.height = 0;
  mClipRegion = NULL;
  mColor = NS_RGB(0, 0, 0);
  mColorREF = RGB(0, 0, 0);
  mFontMetrics = nsnull;
  mSolidPen = NULL;
  mDashedPen = NULL;
  mDottedPen = NULL;
  mFlags = ~FLAGS_ALL;
  mLineStyle = nsLineStyle_kSolid;
}

GraphicsState :: GraphicsState(GraphicsState &aState) :
                               mMatrix(&aState.mMatrix),
                               mLocalClip(aState.mLocalClip)
{
  mNext = &aState;
  mClipRegion = NULL;
  mColor = NS_RGB(0, 0, 0);
  mColorREF = RGB(0, 0, 0);
  mFontMetrics = nsnull;
  mSolidPen = NULL;
  mDashedPen = NULL;
  mDottedPen = NULL;
  mFlags = ~FLAGS_ALL;
  mLineStyle = aState.mLineStyle;
}

GraphicsState :: ~GraphicsState()
{
  if (NULL != mClipRegion)
  {
    VERIFY(::DeleteObject(mClipRegion));
    mClipRegion = NULL;
  }

  //these are killed by the rendering context...
  mSolidPen = NULL;
  mDashedPen = NULL;
  mDottedPen = NULL;
}

#define NOT_SETUP 0x33
static PRBool gIsWIN95 = NOT_SETUP;

#ifdef IBMBIDI
#define DONT_INIT 0
static DWORD gBidiInfo = NOT_SETUP;
#endif // IBMBIDI

// A few of the stock objects are needed all the time, so we just get them
// once
static HFONT  gStockSystemFont = (HFONT)::GetStockObject(SYSTEM_FONT);
static HBRUSH gStockWhiteBrush = (HBRUSH)::GetStockObject(WHITE_BRUSH);
static HPEN   gStockBlackPen = (HPEN)::GetStockObject(BLACK_PEN);
static HPEN   gStockWhitePen = (HPEN)::GetStockObject(WHITE_PEN);

nsRenderingContextWin :: nsRenderingContextWin()
{
  NS_INIT_REFCNT();

  // The first time in we initialize gIsWIN95 flag
  if (NOT_SETUP == gIsWIN95) {
    OSVERSIONINFO os;
    os.dwOSVersionInfoSize = sizeof(os);
    ::GetVersionEx(&os);
    // XXX This may need tweaking for win98
    if (VER_PLATFORM_WIN32_NT == os.dwPlatformId) {
      gIsWIN95 = PR_FALSE;
    }
    else {
      gIsWIN95 = PR_TRUE;

#ifdef IBMBIDI
      if ( (os.dwMajorVersion < 4)
           || ( (os.dwMajorVersion == 4) && (os.dwMinorVersion == 0) ) ) {
        // Windows 95 or earlier: assume it's not Bidi
        gBidiInfo = DONT_INIT;
      }
      else if (os.dwMajorVersion >= 4) {
        // Windows 98 or later
        UINT cp = ::GetACP();
        if (1256 == cp) {
          gBidiInfo = GCP_REORDER | GCP_GLYPHSHAPE;
        }
        else if (1255 == cp) {
          gBidiInfo = GCP_REORDER;
        }
      }
#endif // IBMBIDI
    }
  }

  mDC = NULL;
  mMainDC = NULL;
  mDCOwner = nsnull;
  mFontMetrics = nsnull;
  mOrigSolidBrush = NULL;
  mOrigFont = NULL;
  mOrigSolidPen = NULL;
  mCurrBrushColor = RGB(255, 255, 255);
  mCurrFontMetrics = nsnull;
  mCurrPenColor = NULL;
  mCurrPen = NULL;
  mNullPen = NULL;
  mCurrTextColor = RGB(0, 0, 0);
  mCurrLineStyle = nsLineStyle_kSolid;
#ifdef NS_DEBUG
  mInitialized = PR_FALSE;
#endif
  mSurface = nsnull;
  mMainSurface = nsnull;

  mStateCache = new nsVoidArray();
#ifdef IBMBIDI
  mRightToLeftText = PR_FALSE;
#endif

  //create an initial GraphicsState

  PushState();

  mP2T = 1.0f;
}

nsRenderingContextWin :: ~nsRenderingContextWin()
{
  NS_IF_RELEASE(mContext);
  NS_IF_RELEASE(mFontMetrics);

  //destroy the initial GraphicsState

  PRBool clipState;
  PopState(clipState);

  //cleanup the DC so that we can just destroy objects
  //in the graphics state without worrying that we are
  //ruining the dc

  if (NULL != mDC)
  {
    if (NULL != mOrigSolidBrush)
    {
      ::SelectObject(mDC, mOrigSolidBrush);
      mOrigSolidBrush = NULL;
    }

    if (NULL != mOrigFont)
    {
      ::SelectObject(mDC, mOrigFont);
      mOrigFont = NULL;
    }

    if (NULL != mOrigSolidPen)
    {
      ::SelectObject(mDC, mOrigSolidPen);
      mOrigSolidPen = NULL;
    }
  }

  mCurrBrush = NULL;   // don't delete - owned by brush cache

  //don't kill the font because the font cache/metrics owns it
  mCurrFont = NULL;

  if (mCurrPen && (mCurrPen != gStockBlackPen) && (mCurrPen != gStockWhitePen)) {
    VERIFY(::DeleteObject(mCurrPen));
  }

  if ((NULL != mNullPen) && (mNullPen != mCurrPen)) {
    VERIFY(::DeleteObject(mNullPen));
  }

  mCurrPen = NULL;
  mNullPen = NULL;

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

  if (nsnull != mSurface)
  {
    mSurface->ReleaseDC();
    NS_RELEASE(mSurface);
  }

  if (nsnull != mMainSurface)
  {
    mMainSurface->ReleaseDC();
    NS_RELEASE(mMainSurface);
  }

  if (nsnull != mDCOwner)
  {
    ::ReleaseDC((HWND)mDCOwner->GetNativeData(NS_NATIVE_WINDOW), mMainDC);
    NS_RELEASE(mDCOwner);
  }

  mTranMatrix = nsnull;
  mDC = NULL;
  mMainDC = NULL;
}

nsresult
nsRenderingContextWin :: QueryInterface(REFNSIID aIID, void** aInstancePtr)
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

  if (aIID.Equals(kIRenderingContextWinIID))
  {
    nsIRenderingContextWin* tmp = this;
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

NS_IMPL_ADDREF(nsRenderingContextWin)
NS_IMPL_RELEASE(nsRenderingContextWin)

NS_IMETHODIMP
nsRenderingContextWin :: Init(nsIDeviceContext* aContext,
                              nsIWidget *aWindow)
{
  NS_PRECONDITION(PR_FALSE == mInitialized, "double init");

  mContext = aContext;
  NS_IF_ADDREF(mContext);

  mSurface = (nsDrawingSurfaceWin *)new nsDrawingSurfaceWin();

  if (nsnull != mSurface)
  {
    HDC tdc = (HDC)aWindow->GetNativeData(NS_NATIVE_GRAPHIC);

    NS_ADDREF(mSurface);
    mSurface->Init(tdc);
    mDC = tdc;

    mMainDC = mDC;
    mMainSurface = mSurface;
    NS_ADDREF(mMainSurface);
  }

  mDCOwner = aWindow;

  NS_IF_ADDREF(mDCOwner);

  return CommonInit();
}

NS_IMETHODIMP
nsRenderingContextWin :: Init(nsIDeviceContext* aContext,
                              nsDrawingSurface aSurface)
{
  NS_PRECONDITION(PR_FALSE == mInitialized, "double init");

  mContext = aContext;
  NS_IF_ADDREF(mContext);

  mSurface = (nsDrawingSurfaceWin *)aSurface;

  if (nsnull != mSurface)
  {
    NS_ADDREF(mSurface);
    mSurface->GetDC(&mDC);

    mMainDC = mDC;
    mMainSurface = mSurface;
    NS_ADDREF(mMainSurface);
  }

  mDCOwner = nsnull;

  return CommonInit();
}

nsresult nsRenderingContextWin :: SetupDC(HDC aOldDC, HDC aNewDC)
{
  HBRUSH  prevbrush;
  HFONT   prevfont;
  HPEN    prevpen;

  ::SetTextColor(aNewDC, PALETTERGB_COLORREF(mColor));
  mCurrTextColor = mCurrentColor;
  ::SetBkMode(aNewDC, TRANSPARENT);
  ::SetPolyFillMode(aNewDC, WINDING);
  ::SetStretchBltMode(aNewDC, COLORONCOLOR);
  ::SetTextAlign(aNewDC, TA_BASELINE);

  if (nsnull != aOldDC)
  {
    if (nsnull != mOrigSolidBrush)
      prevbrush = (HBRUSH)::SelectObject(aOldDC, mOrigSolidBrush);

    if (nsnull != mOrigFont)
      prevfont = (HFONT)::SelectObject(aOldDC, mOrigFont);

    if (nsnull != mOrigSolidPen)
      prevpen = (HPEN)::SelectObject(aOldDC, mOrigSolidPen);

  }
  else
  {
    prevbrush = gStockWhiteBrush;
    prevfont = gStockSystemFont;
    prevpen = gStockBlackPen;
  }

  mOrigSolidBrush = (HBRUSH)::SelectObject(aNewDC, prevbrush);
  mOrigFont = (HFONT)::SelectObject(aNewDC, prevfont);
  mOrigSolidPen = (HPEN)::SelectObject(aNewDC, prevpen);

#if 0
  GraphicsState *pstate = mStates;

  while ((nsnull != pstate) && !(pstate->mFlags & FLAG_CLIP_VALID))
    pstate = pstate->mNext;

  if (nsnull != pstate)
    ::SelectClipRgn(aNewDC, pstate->mClipRegion);
#endif

  return NS_OK;
}

nsresult nsRenderingContextWin :: CommonInit(void)
{
  float app2dev;

  mContext->GetAppUnitsToDevUnits(app2dev);
	mTranMatrix->AddScale(app2dev, app2dev);
  mContext->GetDevUnitsToAppUnits(mP2T);

#ifdef NS_DEBUG
  mInitialized = PR_TRUE;
#endif

  mContext->GetGammaTable(mGammaTable);

  return SetupDC(nsnull, mDC);
}

NS_IMETHODIMP nsRenderingContextWin :: LockDrawingSurface(PRInt32 aX, PRInt32 aY,
                                                          PRUint32 aWidth, PRUint32 aHeight,
                                                          void **aBits, PRInt32 *aStride,
                                                          PRInt32 *aWidthBytes, PRUint32 aFlags)
{
  PRBool        destructive;

  PushState();

  mSurface->IsReleaseDCDestructive(&destructive);

  if (destructive)
  {
    PushClipState();

    if (nsnull != mOrigSolidBrush)
      mCurrBrush = (HBRUSH)::SelectObject(mDC, mOrigSolidBrush);

    if (nsnull != mOrigFont)
      mCurrFont = (HFONT)::SelectObject(mDC, mOrigFont);

    if (nsnull != mOrigSolidPen)
      mCurrPen = (HPEN)::SelectObject(mDC, mOrigSolidPen);
  }

  mSurface->ReleaseDC();

  return mSurface->Lock(aX, aY, aWidth, aHeight, aBits, aStride, aWidthBytes, aFlags);
}

NS_IMETHODIMP nsRenderingContextWin :: UnlockDrawingSurface(void)
{
  PRBool  clipstate;

  mSurface->Unlock();
  mSurface->GetDC(&mDC);

  PopState(clipstate);

  mSurface->IsReleaseDCDestructive(&clipstate);

  if (clipstate)
  {
    ::SetTextColor(mDC, PALETTERGB_COLORREF(mColor));
    mCurrTextColor = mCurrentColor;

    ::SetBkMode(mDC, TRANSPARENT);
    ::SetPolyFillMode(mDC, WINDING);
    ::SetStretchBltMode(mDC, COLORONCOLOR);

    mOrigSolidBrush = (HBRUSH)::SelectObject(mDC, mCurrBrush);
    mOrigFont = (HFONT)::SelectObject(mDC, mCurrFont);
    mOrigSolidPen = (HPEN)::SelectObject(mDC, mCurrPen);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextWin :: SelectOffScreenDrawingSurface(nsDrawingSurface aSurface)
{
  nsresult  rv;

  //XXX this should reset the data in the state stack.

  if (aSurface != mSurface)
  {
    if (nsnull != aSurface)
    {
      HDC tdc;

      //get back a DC
      ((nsDrawingSurfaceWin *)aSurface)->GetDC(&tdc);

      rv = SetupDC(mDC, tdc);

      //kill the DC
      mSurface->ReleaseDC();

      NS_IF_RELEASE(mSurface);
      mSurface = (nsDrawingSurfaceWin *)aSurface;
    }
    else
    {
      if (NULL != mDC)
      {
        rv = SetupDC(mDC, mMainDC);

        //kill the DC
        mSurface->ReleaseDC();

        NS_IF_RELEASE(mSurface);
        mSurface = mMainSurface;
      }
    }

    NS_ADDREF(mSurface);
    mSurface->GetDC(&mDC);
  }
  else
    rv = NS_OK;

  return rv;
}

NS_IMETHODIMP
nsRenderingContextWin :: GetDrawingSurface(nsDrawingSurface *aSurface)
{
  *aSurface = mSurface;
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextWin :: GetHints(PRUint32& aResult)
{
  PRUint32 result = 0;

  if (gIsWIN95)
    result |= NS_RENDERING_HINT_FAST_8BIT_TEXT;

#ifdef IBMBIDI
  if (NOT_SETUP == gBidiInfo) {
    InitBidiInfo();
  }
  if (GCP_REORDER == (gBidiInfo & GCP_REORDER) )
    result |= NS_RENDERING_HINT_BIDI_REORDERING;
  if (GCP_GLYPHSHAPE == (gBidiInfo & GCP_GLYPHSHAPE) )
    result |= NS_RENDERING_HINT_ARABIC_SHAPING;
#endif // IBMBIDI
  
  aResult = result;

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextWin :: Reset()
{
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextWin :: GetDeviceContext(nsIDeviceContext *&aContext)
{
  NS_IF_ADDREF(mContext);
  aContext = mContext;
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextWin :: PushState(void)
{
  PRInt32 cnt = mStateCache->Count();

  if (cnt == 0)
  {
    if (nsnull == mStates)
      mStates = new GraphicsState();
    else
      mStates = new GraphicsState(*mStates);
  }
  else
  {
    GraphicsState *state = (GraphicsState *)mStateCache->ElementAt(cnt - 1);
    mStateCache->RemoveElementAt(cnt - 1);

    state->mNext = mStates;

    //clone state info

    state->mMatrix = mStates->mMatrix;
    state->mLocalClip = mStates->mLocalClip;
// we don't want to NULL this out since we reuse the region
// from state to state. if we NULL it, we need to also delete it,
// which means we'll just re-create it when we push the clip state. MMP
//    state->mClipRegion = NULL;
    state->mSolidPen = NULL;
    state->mDashedPen = NULL;
    state->mDottedPen = NULL;
    state->mFlags = ~FLAGS_ALL;
    state->mLineStyle = mStates->mLineStyle;

    mStates = state;
  }

  if (nsnull != mStates->mNext)
  {
    mStates->mNext->mColor = mCurrentColor;
    mStates->mNext->mColorREF = mColor;
    mStates->mNext->mFontMetrics = mFontMetrics;
    NS_IF_ADDREF(mStates->mNext->mFontMetrics);
  }

  mTranMatrix = &mStates->mMatrix;

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextWin :: PopState(PRBool &aClipEmpty)
{
  PRBool  retval = PR_FALSE;

  if (nsnull == mStates)
  {
    NS_ASSERTION(!(nsnull == mStates), "state underflow");
  }
  else
  {
    GraphicsState *oldstate = mStates;

    mStates = mStates->mNext;

    mStateCache->AppendElement(oldstate);

    if (nsnull != mStates)
    {
      mTranMatrix = &mStates->mMatrix;

      GraphicsState *pstate;

      if (oldstate->mFlags & FLAG_CLIP_CHANGED)
      {
        pstate = mStates;

        //the clip rect has changed from state to state, so
        //install the previous clip rect

        while ((nsnull != pstate) && !(pstate->mFlags & FLAG_CLIP_VALID))
          pstate = pstate->mNext;

        if (nsnull != pstate)
        {
          int cliptype = ::SelectClipRgn(mDC, pstate->mClipRegion);

          if (cliptype == NULLREGION)
            retval = PR_TRUE;
        }
      }

      oldstate->mFlags &= ~FLAGS_ALL;
      oldstate->mSolidPen = NULL;
      oldstate->mDashedPen = NULL;
      oldstate->mDottedPen = NULL;

      NS_IF_RELEASE(mFontMetrics);
      mFontMetrics = mStates->mFontMetrics;

      mCurrentColor = mStates->mColor;
      mColor = mStates->mColorREF;

      SetLineStyle(mStates->mLineStyle);
    }
    else
      mTranMatrix = nsnull;
  }

  aClipEmpty = retval;

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextWin :: IsVisibleRect(const nsRect& aRect, PRBool &aVisible)
{
  aVisible = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextWin :: SetClipRect(const nsRect& aRect, nsClipCombine aCombine, PRBool &aClipEmpty)
{
  nsRect  trect = aRect;
  int     cliptype;

  mStates->mLocalClip = aRect;

	mTranMatrix->TransformCoord(&trect.x, &trect.y,
                           &trect.width, &trect.height);

  RECT nr;
  ConditionRect(trect, nr);

  mStates->mFlags |= FLAG_LOCAL_CLIP_VALID;

  //how we combine the new rect with the previous?

  if (aCombine == nsClipCombine_kIntersect)
  {
    PushClipState();

    cliptype = ::IntersectClipRect(mDC, nr.left,
                                   nr.top,
                                   nr.right,
                                   nr.bottom);
  }
  else if (aCombine == nsClipCombine_kUnion)
  {
    PushClipState();

    HRGN  tregion = ::CreateRectRgn(nr.left,
                                    nr.top,
                                    nr.right,
                                    nr.bottom);

    cliptype = ::ExtSelectClipRgn(mDC, tregion, RGN_OR);
    ::DeleteObject(tregion);
  }
  else if (aCombine == nsClipCombine_kSubtract)
  {
    PushClipState();

    cliptype = ::ExcludeClipRect(mDC, nr.left,
                                 nr.top,
                                 nr.right,
                                 nr.bottom);
  }
  else if (aCombine == nsClipCombine_kReplace)
  {
    PushClipState();

    HRGN  tregion = ::CreateRectRgn(nr.left,
                                    nr.top,
                                    nr.right,
                                    nr.bottom);
    cliptype = ::SelectClipRgn(mDC, tregion);
    ::DeleteObject(tregion);
  }
  else
    NS_ASSERTION(PR_FALSE, "illegal clip combination");

  if (cliptype == NULLREGION)
    aClipEmpty = PR_TRUE;
  else
    aClipEmpty = PR_FALSE;

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextWin :: GetClipRect(nsRect &aRect, PRBool &aClipValid)
{
  if (mStates->mFlags & FLAG_LOCAL_CLIP_VALID)
  {
    aRect = mStates->mLocalClip;
    aClipValid = PR_TRUE;
  }
  else
    aClipValid = PR_FALSE;

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextWin :: SetClipRegion(const nsIRegion& aRegion, nsClipCombine aCombine, PRBool &aClipEmpty)
{
  HRGN        hrgn;
  int         cmode, cliptype;

  aRegion.GetNativeRegion((void *&)hrgn);

  switch (aCombine)
  {
    case nsClipCombine_kIntersect:
      cmode = RGN_AND;
      break;

    case nsClipCombine_kUnion:
      cmode = RGN_OR;
      break;

    case nsClipCombine_kSubtract:
      cmode = RGN_DIFF;
      break;

    default:
    case nsClipCombine_kReplace:
      cmode = RGN_COPY;
      break;
  }

  if (NULL != hrgn)
  {
    mStates->mFlags &= ~FLAG_LOCAL_CLIP_VALID;
    PushClipState();
    cliptype = ::ExtSelectClipRgn(mDC, hrgn, cmode);
  }
  else
    return PR_FALSE;

  if (cliptype == NULLREGION)
    aClipEmpty = PR_TRUE;
  else
    aClipEmpty = PR_FALSE;

  return NS_OK;
}

/**
 * Fills in |aRegion| with a copy of the current clip region.
 */
NS_IMETHODIMP nsRenderingContextWin::CopyClipRegion(nsIRegion &aRegion)
{
  int rstat = ::GetClipRgn(mDC, ((nsRegionWin *)&aRegion)->mRegion);

  if (rstat == 1)
  {
    //i can't find a way to get the actual complexity
    //of the region without actually messing with it, so
    //if the region is non-empty, we'll call it complex. MMP

    ((nsRegionWin *)&aRegion)->mRegionType = eRegionComplexity_complex;
  }
 
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextWin :: GetClipRegion(nsIRegion **aRegion)
{
  nsresult  rv = NS_OK;

  NS_ASSERTION(!(nsnull == aRegion), "no region ptr");

  if (nsnull == *aRegion)
  {
    nsRegionWin *rgn = new nsRegionWin();

    if (nsnull != rgn)
    {
      NS_ADDREF(rgn);

      rv = rgn->Init();

      if (NS_OK != rv)
        NS_RELEASE(rgn);
      else
        *aRegion = rgn;
    }
    else
      rv = NS_ERROR_OUT_OF_MEMORY;
  }

  if (rv == NS_OK) {
    rv = CopyClipRegion(**aRegion);
  }

  return rv;
}

NS_IMETHODIMP nsRenderingContextWin :: SetColor(nscolor aColor)
{
  mCurrentColor = aColor;
  mColor = RGB(mGammaTable[NS_GET_R(aColor)],
               mGammaTable[NS_GET_G(aColor)],
               mGammaTable[NS_GET_B(aColor)]);
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextWin :: GetColor(nscolor &aColor) const
{
  aColor = mCurrentColor;
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextWin :: SetLineStyle(nsLineStyle aLineStyle)
{
  mCurrLineStyle = aLineStyle;
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextWin :: GetLineStyle(nsLineStyle &aLineStyle)
{
  aLineStyle = mCurrLineStyle;
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextWin :: SetFont(const nsFont& aFont)
{
  NS_IF_RELEASE(mFontMetrics);
  mContext->GetMetricsFor(aFont, mFontMetrics);

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextWin :: SetFont(nsIFontMetrics *aFontMetrics)
{
  NS_IF_RELEASE(mFontMetrics);
  mFontMetrics = aFontMetrics;
  NS_IF_ADDREF(mFontMetrics);

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextWin :: GetFontMetrics(nsIFontMetrics *&aFontMetrics)
{
  NS_IF_ADDREF(mFontMetrics);
  aFontMetrics = mFontMetrics;

  return NS_OK;
}

// add the passed in translation to the current translation
NS_IMETHODIMP nsRenderingContextWin :: Translate(nscoord aX, nscoord aY)
{
	mTranMatrix->AddTranslation((float)aX,(float)aY);
  return NS_OK;
}

// add the passed in scale to the current scale
NS_IMETHODIMP nsRenderingContextWin :: Scale(float aSx, float aSy)
{
	mTranMatrix->AddScale(aSx, aSy);
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextWin :: GetCurrentTransform(nsTransform2D *&aTransform)
{
  aTransform = mTranMatrix;
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextWin :: CreateDrawingSurface(nsRect *aBounds, PRUint32 aSurfFlags, nsDrawingSurface &aSurface)
{
  nsDrawingSurfaceWin *surf = new nsDrawingSurfaceWin();

  if (nsnull != surf)
  {
    NS_ADDREF(surf);

    if (nsnull != aBounds)
      surf->Init(mMainDC, aBounds->width, aBounds->height, aSurfFlags);
    else
      surf->Init(mMainDC, 0, 0, aSurfFlags);
  }

  aSurface = (nsDrawingSurface)surf;

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextWin :: DestroyDrawingSurface(nsDrawingSurface aDS)
{
  nsDrawingSurfaceWin *surf = (nsDrawingSurfaceWin *)aDS;

  //are we using the surface that we want to kill?
  if (surf == mSurface)
  {
    //remove our local ref to the surface
    NS_IF_RELEASE(mSurface);

    mDC = mMainDC;
    mSurface = mMainSurface;

    //two pointers: two refs
    NS_IF_ADDREF(mSurface);
  }

  //release it...
  NS_IF_RELEASE(surf);

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextWin :: DrawLine(nscoord aX0, nscoord aY0, nscoord aX1, nscoord aY1)
{
  if (nsLineStyle_kNone == mCurrLineStyle)
    return NS_OK;

	mTranMatrix->TransformCoord(&aX0,&aY0);
	mTranMatrix->TransformCoord(&aX1,&aY1);

  SetupPen();

  if (nsLineStyle_kDotted == mCurrLineStyle)
  {
    lineddastruct dda_struct;

    dda_struct.nDottedPixel = 1;
    dda_struct.dc = mDC;
    dda_struct.crColor = mColor;

    LineDDA((int)(aX0),(int)(aY0),(int)(aX1),(int)(aY1),(LINEDDAPROC) LineDDAFunc,(long)&dda_struct);
  }
  else
  {
    ::MoveToEx(mDC, (int)(aX0), (int)(aY0), NULL);
    ::LineTo(mDC, (int)(aX1), (int)(aY1));
  }

  return NS_OK;
}

  /** ---------------------------------------------------
   *  See documentation in nsIRenderingContextImpl.h
   *	@update 5/01/00 dwc
   */
NS_IMETHODIMP nsRenderingContextWin :: DrawStdLine(nscoord aX0, nscoord aY0, nscoord aX1, nscoord aY1)
{

  if (nsLineStyle_kNone == mCurrLineStyle)
    return NS_OK;

  SetupPen();

  if (nsLineStyle_kDotted == mCurrLineStyle)
  {
    lineddastruct dda_struct;

    dda_struct.nDottedPixel = 1;
    dda_struct.dc = mDC;
    dda_struct.crColor = mColor;

    LineDDA((int)(aX0),(int)(aY0),(int)(aX1),(int)(aY1),(LINEDDAPROC) LineDDAFunc,(long)&dda_struct);
  }
  else
  {
    ::MoveToEx(mDC, (int)(aX0), (int)(aY0), NULL);
    ::LineTo(mDC, (int)(aX1), (int)(aY1));
  }

  return NS_OK;

}



NS_IMETHODIMP nsRenderingContextWin :: DrawPolyline(const nsPoint aPoints[], PRInt32 aNumPoints)
{
  if (nsLineStyle_kNone == mCurrLineStyle)
    return NS_OK;

  // First transform nsPoint's into POINT's; perform coordinate space
  // transformation at the same time
  POINT pts[20];
  POINT* pp0 = pts;

  if (aNumPoints > 20)
    pp0 = new POINT[aNumPoints];

  POINT* pp = pp0;
  const nsPoint* np = &aPoints[0];

	for (PRInt32 i = 0; i < aNumPoints; i++, pp++, np++)
  {
		pp->x = np->x;
		pp->y = np->y;
		mTranMatrix->TransformCoord((int*)&pp->x,(int*)&pp->y);
	}

  // Draw the polyline
  SetupPen();
  ::Polyline(mDC, pp0, int(aNumPoints));

  // Release temporary storage if necessary
  if (pp0 != pts)
    delete [] pp0;

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextWin :: DrawRect(const nsRect& aRect)
{
  RECT nr;
	nsRect	tr;

	tr = aRect;
	mTranMatrix->TransformCoord(&tr.x,&tr.y,&tr.width,&tr.height);
	nr.left = tr.x;
	nr.top = tr.y;
	nr.right = tr.x+tr.width;
	nr.bottom = tr.y+tr.height;

  ::FrameRect(mDC, &nr, SetupSolidBrush());

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextWin :: DrawRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
  RECT nr;

	mTranMatrix->TransformCoord(&aX,&aY,&aWidth,&aHeight);
	nr.left = aX;
	nr.top = aY;
	nr.right = aX+aWidth;
	nr.bottom = aY+aHeight;

  ::FrameRect(mDC, &nr, SetupSolidBrush());

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextWin :: FillRect(const nsRect& aRect)
{
  RECT nr;
	nsRect tr;

	tr = aRect;
	mTranMatrix->TransformCoord(&tr.x,&tr.y,&tr.width,&tr.height);
  ConditionRect(tr, nr);
  ::FillRect(mDC, &nr, SetupSolidBrush());

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextWin :: FillRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
  RECT nr;
	nsRect	tr;

	mTranMatrix->TransformCoord(&aX,&aY,&aWidth,&aHeight);
	nr.left = aX;
	nr.top = aY;
	nr.right = aX+aWidth;
	nr.bottom = aY+aHeight;

  ::FillRect(mDC, &nr, SetupSolidBrush());

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextWin :: InvertRect(const nsRect& aRect)
{
  RECT nr;
	nsRect tr;

	tr = aRect;
	mTranMatrix->TransformCoord(&tr.x,&tr.y,&tr.width,&tr.height);
  ConditionRect(tr, nr);
  ::InvertRect(mDC, &nr);

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextWin :: InvertRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
  RECT nr;
	nsRect	tr;

	mTranMatrix->TransformCoord(&aX,&aY,&aWidth,&aHeight);
	nr.left = aX;
	nr.top = aY;
	nr.right = aX+aWidth;
	nr.bottom = aY+aHeight;

  ::InvertRect(mDC, &nr);

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextWin :: DrawPolygon(const nsPoint aPoints[], PRInt32 aNumPoints)
{
  // First transform nsPoint's into POINT's; perform coordinate space
  // transformation at the same time
  POINT pts[20];
  POINT* pp0 = pts;

  if (aNumPoints > 20)
    pp0 = new POINT[aNumPoints];

  POINT* pp = pp0;
  const nsPoint* np = &aPoints[0];

	for (PRInt32 i = 0; i < aNumPoints; i++, pp++, np++)
  {
		pp->x = np->x;
		pp->y = np->y;
		mTranMatrix->TransformCoord((int*)&pp->x,(int*)&pp->y);
	}

  // Outline the polygon - note we are implicitly ignoring the linestyle here
  SetupSolidPen();
  HBRUSH oldBrush = (HBRUSH)::SelectObject(mDC, ::GetStockObject(NULL_BRUSH));
  ::Polygon(mDC, pp0, int(aNumPoints));
  ::SelectObject(mDC, oldBrush);

  // Release temporary storage if necessary
  if (pp0 != pts)
    delete [] pp0;

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextWin :: FillPolygon(const nsPoint aPoints[], PRInt32 aNumPoints)
{
  // First transform nsPoint's into POINT's; perform coordinate space
  // transformation at the same time

  POINT pts[20];
  POINT* pp0 = pts;

  if (aNumPoints > 20)
    pp0 = new POINT[aNumPoints];

  POINT* pp = pp0;
  const nsPoint* np = &aPoints[0];

	for (PRInt32 i = 0; i < aNumPoints; i++, pp++, np++)
	{
		pp->x = np->x;
		pp->y = np->y;
		mTranMatrix->TransformCoord((int*)&pp->x,(int*)&pp->y);
	}

  // Fill the polygon
  SetupSolidBrush();

  if (NULL == mNullPen)
    mNullPen = ::CreatePen(PS_NULL, 0, 0);

  HPEN oldPen = (HPEN)::SelectObject(mDC, mNullPen);
  ::Polygon(mDC, pp0, int(aNumPoints));
  ::SelectObject(mDC, oldPen);

  // Release temporary storage if necessary
  if (pp0 != pts)
    delete [] pp0;

  return NS_OK;
}


NS_IMETHODIMP nsRenderingContextWin :: FillStdPolygon(const nsPoint aPoints[], PRInt32 aNumPoints)
{
  // First transform nsPoint's into POINT's;
  POINT pts[20];
  POINT* pp0 = pts;

  if (aNumPoints > 20)
    pp0 = new POINT[aNumPoints];

  POINT* pp = pp0;
  const nsPoint* np = &aPoints[0];

	for (PRInt32 i = 0; i < aNumPoints; i++, pp++, np++){
		pp->x = np->x;
		pp->y = np->y;
	}

  // Fill the polygon
  SetupSolidBrush();

  if (NULL == mNullPen)
    mNullPen = ::CreatePen(PS_NULL, 0, 0);

  HPEN oldPen = (HPEN)::SelectObject(mDC, mNullPen);
  ::Polygon(mDC, pp0, int(aNumPoints));
  ::SelectObject(mDC, oldPen);

  // Release temporary storage if necessary
  if (pp0 != pts)
    delete [] pp0;

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextWin :: DrawEllipse(const nsRect& aRect)
{
  return DrawEllipse(aRect.x, aRect.y, aRect.width, aRect.height);
}

NS_IMETHODIMP nsRenderingContextWin :: DrawEllipse(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
  if (nsLineStyle_kNone == mCurrLineStyle)
    return NS_OK;

  mTranMatrix->TransformCoord(&aX, &aY, &aWidth, &aHeight);

  SetupPen();

  HBRUSH oldBrush = (HBRUSH)::SelectObject(mDC, ::GetStockObject(NULL_BRUSH));
  
  ::Ellipse(mDC, aX, aY, aX + aWidth, aY + aHeight);
  ::SelectObject(mDC, oldBrush);

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextWin :: FillEllipse(const nsRect& aRect)
{
  return FillEllipse(aRect.x, aRect.y, aRect.width, aRect.height);
}

NS_IMETHODIMP nsRenderingContextWin :: FillEllipse(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
  mTranMatrix->TransformCoord(&aX, &aY, &aWidth, &aHeight);

  SetupSolidPen();
  SetupSolidBrush();
  
  ::Ellipse(mDC, aX, aY, aX + aWidth, aY + aHeight);

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextWin :: DrawArc(const nsRect& aRect,
                                 float aStartAngle, float aEndAngle)
{
  return DrawArc(aRect.x,aRect.y,aRect.width,aRect.height,aStartAngle,aEndAngle);
}

NS_IMETHODIMP nsRenderingContextWin :: DrawArc(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight,
                                 float aStartAngle, float aEndAngle)
{
  if (nsLineStyle_kNone == mCurrLineStyle)
    return NS_OK;

  PRInt32 quad1, quad2, sx, sy, ex, ey, cx, cy;
  float   anglerad, distance;

  mTranMatrix->TransformCoord(&aX, &aY, &aWidth, &aHeight);

  SetupPen();
  SetupSolidBrush();

  // figure out the the coordinates of the arc from the angle
  distance = (float)sqrt((float)(aWidth * aWidth + aHeight * aHeight));
  cx = aX + aWidth / 2;
  cy = aY + aHeight / 2;

  anglerad = (float)(aStartAngle / (180.0 / 3.14159265358979323846));
  quad1 = (PRInt32)(aStartAngle / 90.0);
  sx = (PRInt32)(distance * cos(anglerad) + cx);
  sy = (PRInt32)(cy - distance * sin(anglerad));

  anglerad = (float)(aEndAngle / (180.0 / 3.14159265358979323846));
  quad2 = (PRInt32)(aEndAngle / 90.0);
  ex = (PRInt32)(distance * cos(anglerad) + cx);
  ey = (PRInt32)(cy - distance * sin(anglerad));

  // this just makes it consitent, on windows 95 arc will always draw CC, nt this sets direction
  ::SetArcDirection(mDC, AD_COUNTERCLOCKWISE);

  ::Arc(mDC, aX, aY, aX + aWidth, aY + aHeight, sx, sy, ex, ey); 

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextWin :: FillArc(const nsRect& aRect,
                                 float aStartAngle, float aEndAngle)
{
  return FillArc(aRect.x, aRect.y, aRect.width, aRect.height, aStartAngle, aEndAngle);
}

NS_IMETHODIMP nsRenderingContextWin :: FillArc(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight,
                                 float aStartAngle, float aEndAngle)
{
  PRInt32 quad1, quad2, sx, sy, ex, ey, cx, cy;
  float   anglerad, distance;

  mTranMatrix->TransformCoord(&aX, &aY, &aWidth, &aHeight);

  SetupSolidPen();
  SetupSolidBrush();

  // figure out the the coordinates of the arc from the angle
  distance = (float)sqrt((float)(aWidth * aWidth + aHeight * aHeight));
  cx = aX + aWidth / 2;
  cy = aY + aHeight / 2;

  anglerad = (float)(aStartAngle / (180.0 / 3.14159265358979323846));
  quad1 = (PRInt32)(aStartAngle / 90.0);
  sx = (PRInt32)(distance * cos(anglerad) + cx);
  sy = (PRInt32)(cy - distance * sin(anglerad));

  anglerad = (float)(aEndAngle / (180.0 / 3.14159265358979323846));
  quad2 = (PRInt32)(aEndAngle / 90.0);
  ex = (PRInt32)(distance * cos(anglerad) + cx);
  ey = (PRInt32)(cy - distance * sin(anglerad));

  // this just makes it consistent, on windows 95 arc will always draw CC,
  // on NT this sets direction
  ::SetArcDirection(mDC, AD_COUNTERCLOCKWISE);

  ::Pie(mDC, aX, aY, aX + aWidth, aY + aHeight, sx, sy, ex, ey); 

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextWin :: GetWidth(char ch, nscoord& aWidth)
{
  char buf[1];
  buf[0] = ch;
  return GetWidth(buf, 1, aWidth);
}

NS_IMETHODIMP nsRenderingContextWin :: GetWidth(PRUnichar ch, nscoord &aWidth, PRInt32 *aFontID)
{
  PRUnichar buf[1];
  buf[0] = ch;
#ifdef IBMBIDI
  WORD charType=0;
  if (::GetStringTypeW(CT_CTYPE3, &ch, 1, &charType) && (charType & C3_DIACRITIC) && !(charType & C3_ALPHA)) {
//    aWidth = 0;
    GetWidth(buf, 1, aWidth, aFontID);
    aWidth *=-1;
    return NS_OK;
  }
#endif
  return GetWidth(buf, 1, aWidth, aFontID);
}

NS_IMETHODIMP nsRenderingContextWin :: GetWidth(const char* aString, nscoord& aWidth)
{
  return GetWidth(aString, strlen(aString), aWidth);
}

NS_IMETHODIMP nsRenderingContextWin :: GetWidth(const char* aString,
                                                PRUint32 aLength,
                                                nscoord& aWidth)
{

  if (nsnull != mFontMetrics)
  {
    // Check for the very common case of trying to get the width of a single
    // space.
    if ((1 == aLength) && (aString[0] == ' '))
    {
      nsFontMetricsWin* fontMetricsWin = (nsFontMetricsWin*)mFontMetrics;
      return fontMetricsWin->GetSpaceWidth(aWidth);
    }

    SIZE  size;

    SetupFontAndColor();
    ::GetTextExtentPoint32(mDC, aString, aLength, &size);
    aWidth = NSToCoordRound(float(size.cx) * mP2T);

    return NS_OK;
  }
  else
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsRenderingContextWin :: GetWidth(const nsString& aString, nscoord& aWidth, PRInt32 *aFontID)
{
  return GetWidth(aString.get(), aString.Length(), aWidth, aFontID);
}

struct GetWidthData {
  HDC   mDC;    // IN
  HFONT mFont;  // IN/OUT (running)
  LONG  mWidth; // IN/OUT (running, accumulated width so far)
};

static PRBool PR_CALLBACK
do_GetWidth(const nsFontSwitch* aFontSwitch,
            const PRUnichar*    aSubstring,
            PRUint32            aSubstringLength,
            void*               aData)
{
  nsFontWin* fontWin = aFontSwitch->mFontWin;

  GetWidthData* data = (GetWidthData*)aData;
  if (data->mFont != fontWin->mFont) {
    // the desired font is not the current font in the DC
    data->mFont = fontWin->mFont;
    ::SelectObject(data->mDC, data->mFont);
  }
  data->mWidth += fontWin->GetWidth(data->mDC, aSubstring, aSubstringLength);
  return PR_TRUE; // don't stop till the end
}

NS_IMETHODIMP nsRenderingContextWin :: GetWidth(const PRUnichar *aString,
                                                PRUint32 aLength,
                                                nscoord &aWidth,
                                                PRInt32 *aFontID)
{
  if (!mFontMetrics) return NS_ERROR_FAILURE;

  SetupFontAndColor();

  nsFontMetricsWin* metrics = (nsFontMetricsWin*)mFontMetrics;
  GetWidthData data = {mDC, mCurrFont, 0};

  metrics->ResolveForwards(mDC, aString, aLength, do_GetWidth, &data);
  aWidth = NSToCoordRound(float(data.mWidth) * mP2T);

  if (mCurrFont != data.mFont) {
    // If the font was changed along the way, restore our font
    ::SelectObject(mDC, mCurrFont);
  }

  if (aFontID) *aFontID = 0;

  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextWin::GetTextDimensions(const char*       aString,
                                         PRInt32           aLength,
                                         PRInt32           aAvailWidth,
                                         PRInt32*          aBreaks,
                                         PRInt32           aNumBreaks,
                                         nsTextDimensions& aDimensions,
                                         PRInt32&          aNumCharsFit,
                                         nsTextDimensions& aLastWordDimensions,
                                         PRInt32*          aFontID)
{
  NS_PRECONDITION(aBreaks[aNumBreaks - 1] == aLength, "invalid break array");

  if (nsnull != mFontMetrics) {
    // Setup the font and foreground color
    SetupFontAndColor();

    // If we need to back up this state represents the last place we could
    // break. We can use this to avoid remeasuring text
    PRInt32 prevBreakState_BreakIndex = -1; // not known (hasn't been computed)
    nscoord prevBreakState_Width = 0; // accumulated width to this point

    // Initialize OUT parameters
    aLastWordDimensions.ascent = mCurrFontWin->mMaxAscent;
    aLastWordDimensions.descent = mCurrFontWin->mMaxDescent;
    aLastWordDimensions.width = -1;
    aNumCharsFit = 0;

    // Iterate each character in the string and determine which font to use
    nsFontMetricsWin* metrics = (nsFontMetricsWin*)mFontMetrics;
    nscoord width = 0;
    PRInt32 start = 0;
    nscoord aveCharWidth;
    metrics->GetAveCharWidth(aveCharWidth);

    while (start < aLength) {
      // Estimate how many characters will fit. Do that by diving the available
      // space by the average character width. Make sure the estimated number
      // of characters is at least 1
      PRInt32 estimatedNumChars = 0;
      if (aveCharWidth > 0) {
        estimatedNumChars = (aAvailWidth - width) / aveCharWidth;
      }
      if (estimatedNumChars < 1) {
        estimatedNumChars = 1;
      }

      // Find the nearest break offset
      PRInt32 estimatedBreakOffset = start + estimatedNumChars;
      PRInt32 breakIndex;
      nscoord numChars;

      // Find the nearest place to break that is less than or equal to
      // the estimated break offset
      if (aLength < estimatedBreakOffset) {
        // All the characters should fit
        numChars = aLength - start;
        breakIndex = aNumBreaks - 1;
      } 
      else {
        breakIndex = prevBreakState_BreakIndex;
        while (((breakIndex + 1) < aNumBreaks) &&
               (aBreaks[breakIndex + 1] <= estimatedBreakOffset)) {
          ++breakIndex;
        }
        if (breakIndex == prevBreakState_BreakIndex) {
          ++breakIndex; // make sure we advanced past the previous break index
        }
        numChars = aBreaks[breakIndex] - start;
      }

      // Measure the text
      nscoord twWidth = 0;
      if ((1 == numChars) && (aString[start] == ' ')) {
        metrics->GetSpaceWidth(twWidth);
      } 
      else if (numChars > 0) {
        SIZE  size;
        ::GetTextExtentPoint32(mDC, &aString[start], numChars, &size);
        twWidth = NSToCoordRound(float(size.cx) * mP2T);
      }

      // See if the text fits
      PRBool  textFits = (twWidth + width) <= aAvailWidth;

      // If the text fits then update the width and the number of
      // characters that fit
      if (textFits) {
        aNumCharsFit += numChars;
        width += twWidth;
        start += numChars;

        // This is a good spot to back up to if we need to so remember
        // this state
        prevBreakState_BreakIndex = breakIndex;
        prevBreakState_Width = width;
      }
      else {
        // See if we can just back up to the previous saved state and not
        // have to measure any text
        if (prevBreakState_BreakIndex > 0) {
          // If the previous break index is just before the current break index
          // then we can use it
          if (prevBreakState_BreakIndex == (breakIndex - 1)) {
            aNumCharsFit = aBreaks[prevBreakState_BreakIndex];
            width = prevBreakState_Width;
            break;
          }
        }

        // We can't just revert to the previous break state
        if (0 == breakIndex) {
          // There's no place to back up to so even though the text doesn't fit
          // return it anyway
          aNumCharsFit += numChars;
          width += twWidth;
          break;
        }

        // Repeatedly back up until we get to where the text fits or we're all
        // the way back to the first word
        width += twWidth;
        while ((breakIndex >= 1) && (width > aAvailWidth)) {
          twWidth = 0;
          start = aBreaks[breakIndex - 1];
          numChars = aBreaks[breakIndex] - start;
          
          if ((1 == numChars) && (aString[start] == ' ')) {
            metrics->GetSpaceWidth(twWidth);
          } 
          else if (numChars > 0) {
            SIZE  size;
            ::GetTextExtentPoint32(mDC, &aString[start], numChars, &size);
            twWidth = NSToCoordRound(float(size.cx) * mP2T);
          }

          width -= twWidth;
          aNumCharsFit = start;
          breakIndex--;
        }
        break;
      }
    }

    aDimensions.width = width;
    aDimensions.ascent = mCurrFontWin->mMaxAscent;
    aDimensions.descent = mCurrFontWin->mMaxDescent;

    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

struct BreakGetTextDimensionsData {
  HDC      mDC;                // IN
  HFONT    mFont;              // IN/OUT (running)
  float    mP2T;               // IN
  PRInt32  mAvailWidth;        // IN
  PRInt32* mBreaks;            // IN
  PRInt32  mNumBreaks;         // IN
  nscoord  mSpaceWidth;        // IN
  nscoord  mAveCharWidth;      // IN
  PRInt32  mEstimatedNumChars; // IN (running -- to handle the edge case of one word)

  PRInt32  mNumCharsFit;  // IN/OUT -- accumulated number of chars that fit so far
  nscoord  mWidth;        // IN/OUT -- accumulated width so far

  // If we need to back up, this state represents the last place
  // we could break. We can use this to avoid remeasuring text
  PRInt32 mPrevBreakState_BreakIndex; // IN/OUT, initialized as -1, i.e., not yet computed
  nscoord mPrevBreakState_Width;      // IN/OUT, initialized as  0

  // Remember the fonts that we use so that we can deal with
  // line-breaking in-between fonts later. mOffsets[0] is also used
  // to initialize the current offset from where to start measuring
  nsVoidArray* mFonts;   // OUT
  nsVoidArray* mOffsets; // IN/OUT

};

static PRBool PR_CALLBACK
do_BreakGetTextDimensions(const nsFontSwitch* aFontSwitch,
                          const PRUnichar*    aSubstring,
                          PRUint32            aSubstringLength,
                          void*               aData)
{
  nsFontWin* fontWin = aFontSwitch->mFontWin;

  // Make sure the font is selected
  BreakGetTextDimensionsData* data = (BreakGetTextDimensionsData*)aData;
  if (data->mFont != fontWin->mFont) {
    data->mFont = fontWin->mFont;
    ::SelectObject(data->mDC, data->mFont);
  }

  // Our current state relatively to the _full_ string...
  // This allows emulating the previous code...
  const PRUnichar* pstr = (const PRUnichar*)data->mOffsets->ElementAt(0);
  PRInt32 numCharsFit = data->mNumCharsFit;
  nscoord width = data->mWidth;
  PRInt32 start = (PRInt32)(aSubstring - pstr);
  PRInt32 i = start + aSubstringLength;
  PRBool allDone = PR_FALSE;

  while (start < i) {
    // Estimate how many characters will fit. Do that by dividing the
    // available space by the average character width
    PRInt32 estimatedNumChars = data->mEstimatedNumChars;
    if (!estimatedNumChars && data->mAveCharWidth > 0) {
      estimatedNumChars = (data->mAvailWidth - width) / data->mAveCharWidth;
    }
    // Make sure the estimated number of characters is at least 1
    if (estimatedNumChars < 1) {
      estimatedNumChars = 1;
    }

    // Find the nearest break offset
    PRInt32 estimatedBreakOffset = start + estimatedNumChars;
    PRInt32 breakIndex = -1; // not yet computed
    PRBool  inMiddleOfSegment = PR_FALSE;
    nscoord numChars;

    // Avoid scanning the break array in the case where we think all
    // the text should fit
    if (i <= estimatedBreakOffset) {
      // Everything should fit
      numChars = i - start;
    }
    else {
      // Find the nearest place to break that is less than or equal to
      // the estimated break offset
      breakIndex = data->mPrevBreakState_BreakIndex;
      while (data->mBreaks[breakIndex + 1] <= estimatedBreakOffset) {
        ++breakIndex;
      }

      if (breakIndex == -1)
        breakIndex = 0;

      // We found a place to break that is before the estimated break
      // offset. Where we break depends on whether the text crosses a
      // segment boundary
      if (start < data->mBreaks[breakIndex]) {
        // The text crosses at least one segment boundary so measure to the
        // break point just before the estimated break offset
        numChars = PR_MIN(data->mBreaks[breakIndex] - start, (PRInt32)aSubstringLength);
      } 
      else {
        // See whether there is another segment boundary between this one
        // and the end of the text
        if ((breakIndex < (data->mNumBreaks - 1)) && (data->mBreaks[breakIndex] < i)) {
          ++breakIndex;
          numChars = data->mBreaks[breakIndex] - start;
        }
        else {
          NS_ASSERTION(i != data->mBreaks[breakIndex], "don't expect to be at segment boundary");

          // The text is all within the same segment
          numChars = i - start;

          // Remember we're in the middle of a segment and not in between
          // two segments
          inMiddleOfSegment = PR_TRUE;
        }
      }
    }

    // Measure the text
    nscoord twWidth, pxWidth;
    if ((1 == numChars) && (pstr[start] == ' ')) {
      twWidth = data->mSpaceWidth;
    }
    else {
      pxWidth = fontWin->GetWidth(data->mDC, &pstr[start], numChars);
      twWidth = NSToCoordRound(float(pxWidth) * data->mP2T);
    }

    // See if the text fits
    PRBool textFits = (twWidth + width) <= data->mAvailWidth;

    // If the text fits then update the width and the number of
    // characters that fit
    if (textFits) {
      numCharsFit += numChars;
      width += twWidth;

      // If we computed the break index and we're not in the middle
      // of a segment then this is a spot that we can back up to if
      // we need to so remember this state
      if ((breakIndex != -1) && !inMiddleOfSegment) {
        data->mPrevBreakState_BreakIndex = breakIndex;
        data->mPrevBreakState_Width = width;
      }
    }
    else {
      // The text didn't fit. If we're out of room then we're all done
      allDone = PR_TRUE;

      // See if we can just back up to the previous saved state and not
      // have to measure any text
      if (data->mPrevBreakState_BreakIndex != -1) {
        PRBool canBackup;

        // If we're in the middle of a word then the break index
        // must be the same if we can use it. If we're at a segment
        // boundary, then if the saved state is for the previous
        // break index then we can use it
        if (inMiddleOfSegment) {
          canBackup = data->mPrevBreakState_BreakIndex == breakIndex;
        } else {
          canBackup = data->mPrevBreakState_BreakIndex == (breakIndex - 1);
        }

        if (canBackup) {
          numCharsFit = data->mBreaks[data->mPrevBreakState_BreakIndex];
          width = data->mPrevBreakState_Width;
          break;
        }
      }

      // We can't just revert to the previous break state. Find the break
      // index just before the end of the text
      i = start + numChars;
      if (breakIndex == -1) {
        breakIndex = 0;
        if (data->mBreaks[breakIndex] < i) {
          while ((breakIndex + 1 < data->mNumBreaks) && (data->mBreaks[breakIndex + 1] < i)) {
            ++breakIndex;
          }
        }
      }

      if ((0 == breakIndex) && (i <= data->mBreaks[0])) {
        // There's no place to back up to so even though the text doesn't fit
        // return it anyway
        numCharsFit += numChars;
        width += twWidth;

        // Edge case of one word: it could be that we just measured a fragment of the
        // first word and its remainder involves other fonts, so we want to keep going
        // until we at least measure the first word entirely
        if (numCharsFit < data->mBreaks[0]) {
          allDone = PR_FALSE;
          // from now on, the estimated number of characters is what we want to measure
          data->mEstimatedNumChars = data->mBreaks[0] - numCharsFit;
          start += numChars;
        }

        break;
      }

      // Repeatedly back up until we get to where the text fits or we're
      // all the way back to the first word
      width += twWidth;
      while ((breakIndex >= 0) && (width > data->mAvailWidth)) {
        twWidth = 0;
        start = data->mBreaks[breakIndex];
        numChars = i - start;
        if ((1 == numChars) && (pstr[start] == ' ')) {
          twWidth = data->mSpaceWidth;
        }
        else if (numChars > 0) {
          pxWidth = fontWin->GetWidth(data->mDC, &pstr[start], numChars);
          twWidth = NSToCoordRound(float(pxWidth) * data->mP2T);
        }

        width -= twWidth;
        numCharsFit = start;
        --breakIndex;
        i = start;
      }
    }

    start += numChars;
  }

#ifdef DEBUG_rbs
  NS_ASSERTION(allDone || start == i, "internal error");
  NS_ASSERTION(allDone || data->mNumCharsFit != numCharsFit, "internal error");
#endif

  if (data->mNumCharsFit != numCharsFit) {
    // some text was actually retained
    data->mWidth = width;
    data->mNumCharsFit = numCharsFit;
    data->mFonts->AppendElement(fontWin);
    data->mOffsets->AppendElement((void*)&pstr[numCharsFit]);
  }

  if (allDone) {
    // stop now
    return PR_FALSE;
  }

  return PR_TRUE; // don't stop if we still need to measure more characters
}

NS_IMETHODIMP
nsRenderingContextWin::GetTextDimensions(const PRUnichar*  aString,
                                         PRInt32           aLength,
                                         PRInt32           aAvailWidth,
                                         PRInt32*          aBreaks,
                                         PRInt32           aNumBreaks,
                                         nsTextDimensions& aDimensions,
                                         PRInt32&          aNumCharsFit,
                                         nsTextDimensions& aLastWordDimensions,
                                         PRInt32*          aFontID)
{
  if (!mFontMetrics) return NS_ERROR_FAILURE;

  SetupFontAndColor();

  nsFontMetricsWin* metrics = (nsFontMetricsWin*)mFontMetrics;

  nscoord spaceWidth, aveCharWidth;
  metrics->GetSpaceWidth(spaceWidth);
  metrics->GetAveCharWidth(aveCharWidth);

  // Note: aBreaks[] is supplied to us so that the first word is located
  // at aString[0 .. aBreaks[0]-1] and more generally, the k-th word is
  // located at aString[aBreaks[k-1] .. aBreaks[k]-1]. Whitespace can
  // be included and each of them counts as a word in its own right.

  // Upon completion of glyph resolution, characters that can be
  // represented with fonts[i] are at offsets[i] .. offsets[i+1]-1

  nsAutoVoidArray fonts, offsets;
  offsets.AppendElement((void*)aString);

  BreakGetTextDimensionsData data = {mDC, mCurrFont, mP2T, 
    aAvailWidth, aBreaks, aNumBreaks, spaceWidth, aveCharWidth,
    0, 0, 0, -1, 0, &fonts, &offsets 
  };

  metrics->ResolveForwards(mDC, aString, aLength, do_BreakGetTextDimensions, &data);

  if (mCurrFont != data.mFont) {
    // If the font was changed along the way, restore our font
    ::SelectObject(mDC, mCurrFont);
  }

  if (aFontID) *aFontID = 0;

  aNumCharsFit = data.mNumCharsFit;
  aDimensions.width = data.mWidth;

  ///////////////////
  // Post-processing for the ascent and descent:
  //
  // The width of the last word is included in the final width, but its
  // ascent and descent are kept aside for the moment. The problem is that
  // line-breaking may occur _before_ the last word, and we don't want its
  // ascent and descent to interfere. We can re-measure the last word and
  // substract its width later. However, we need a special care for the ascent
  // and descent at the break-point. The idea is to keep the ascent and descent
  // of the last word separate, and let layout consider them later when it has
  // determined that line-breaking doesn't occur before the last word.
  //
  // Therefore, there are two things to do:
  // 1. Determine the ascent and descent up to where line-breaking may occur.
  // 2. Determine the ascent and descent of the remainder.
  //    For efficiency however, it is okay to bail out early if there is only
  //    one font (in this case, the height of the last word has no special
  //    effect on the total height).

  // aLastWordDimensions.width should be set to -1 to reply that we don't
  // know the width of the last word since we measure multiple words
  aLastWordDimensions.Clear();
  aLastWordDimensions.width = -1;

  nsFontWin* fontWin = (nsFontWin*)fonts[0];
  aDimensions.ascent = fontWin->mMaxAscent;
  aDimensions.descent = fontWin->mMaxDescent;

  // fast path - normal case, quick return if there is only one font
  if (fonts.Count() == 1)
    return NS_OK;

  // get the last break index.
  // If there is only one word, we end up with lastBreakIndex = 0. We don't
  // need to worry about aLastWordDimensions in this case too. But if we didn't
  // return earlier, it would mean that the unique word needs several fonts
  // and we will still have to loop over the fonts to return the final height
  PRInt32 lastBreakIndex = 0;
  while (aBreaks[lastBreakIndex] < aNumCharsFit)
    ++lastBreakIndex;

  const PRUnichar* lastWord = (lastBreakIndex > 0) 
    ? aString + aBreaks[lastBreakIndex-1]
    : aString + aNumCharsFit; // let it point outside to play nice with the loop

  // now get the desired ascent and descent information... this is however
  // a very fast loop of the order of the number of additional fonts

  PRInt32 currFont = 0;
  const PRUnichar* pstr = aString;
  const PRUnichar* last = aString + aNumCharsFit;

  while (pstr < last) {
    fontWin = (nsFontWin*)fonts[currFont];
    PRUnichar* nextOffset = (PRUnichar*)offsets[++currFont]; 

    // For consistent word-wrapping, we are going to handle the whitespace
    // character with special care because a whitespace character can come
    // from a font different from that of the previous word. If 'x', 'y', 'z',
    // are Unicode points that require different fonts, we want 'xyz <br>'
    // and 'xyz<br>' to have the same height because it gives a more stable
    // rendering, especially when the window is resized at the edge of the word.
    // If we don't do this, a 'tall' trailing whitespace, i.e., if the whitespace
    // happens to come from a font with a bigger ascent and/or descent than all
    // current fonts on the line, this can cause the next lines to be shifted
    // down the window is slowly resized to fit that whitespace.
    if (*pstr == ' ') {
      // skip pass the whitespace to ignore the height that it may contribute
      ++pstr;
      // get out if we reached the end
      if (pstr == last) {
        break;
      }
      // switch to the next font if we just passed the current font 
      if (pstr == nextOffset) {
        fontWin = (nsFontWin*)fonts[currFont];
        nextOffset = (PRUnichar*)offsets[++currFont];
      } 
    }

    // see if the last word intersects with the current font
    // (we are testing for 'nextOffset-1 >= lastWord' since the
    // current font ends at nextOffset-1)
    if (nextOffset > lastWord) {
      if (aLastWordDimensions.ascent < fontWin->mMaxAscent) {
        aLastWordDimensions.ascent = fontWin->mMaxAscent;
      }
      if (aLastWordDimensions.descent < fontWin->mMaxDescent) {
        aLastWordDimensions.descent = fontWin->mMaxDescent;
      }
    }

    // see we have not reached the last word yet
    if (pstr < lastWord) {
      if (aDimensions.ascent < fontWin->mMaxAscent) {
        aDimensions.ascent = fontWin->mMaxAscent;
      }
      if (aDimensions.descent < fontWin->mMaxDescent) {
        aDimensions.descent = fontWin->mMaxDescent;
      }
    }

    // advance to where the next font starts
    pstr = nextOffset;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextWin::GetTextDimensions(const char*       aString,
                                         PRUint32          aLength,
                                         nsTextDimensions& aDimensions)
{
  SetupFontAndColor(); // need this for the case it is space ' '
  GetWidth(aString, aLength, aDimensions.width);
  aDimensions.ascent = mCurrFontWin->mMaxAscent;
  aDimensions.descent = mCurrFontWin->mMaxDescent;
  return NS_OK;
}

struct GetTextDimensionsData {
  HDC     mDC;      // IN
  HFONT   mFont;    // IN/OUT (running)
  LONG    mWidth;   // IN/OUT (running)
  nscoord mAscent;  // IN/OUT (running)
  nscoord mDescent; // IN/OUT (running)
};

static PRBool PR_CALLBACK
do_GetTextDimensions(const nsFontSwitch* aFontSwitch,
                     const PRUnichar*    aSubstring,
                     PRUint32            aSubstringLength,
                     void*               aData)
{
  nsFontWin* fontWin = aFontSwitch->mFontWin;
  GetTextDimensionsData* data = (GetTextDimensionsData*)aData;
  if (data->mFont != fontWin->mFont) {
    data->mFont = fontWin->mFont;
    ::SelectObject(data->mDC, data->mFont);
  }
  data->mWidth += fontWin->GetWidth(data->mDC, aSubstring, aSubstringLength);
  if (data->mAscent < fontWin->mMaxAscent) {
    data->mAscent = fontWin->mMaxAscent;
  }
  if (data->mDescent < fontWin->mMaxDescent) {
    data->mDescent = fontWin->mMaxDescent;
  }

  return PR_TRUE; // don't stop till the end
}

NS_IMETHODIMP
nsRenderingContextWin::GetTextDimensions(const PRUnichar*  aString,
                                         PRUint32          aLength,
                                         nsTextDimensions& aDimensions,
                                         PRInt32*          aFontID)
{
  aDimensions.Clear();
  if (!mFontMetrics) return NS_ERROR_FAILURE;

  SetupFontAndColor();

  nsFontMetricsWin* metrics = (nsFontMetricsWin*)mFontMetrics;
  GetTextDimensionsData data = {mDC, mCurrFont, 0, 0, 0};

  metrics->ResolveForwards(mDC, aString, aLength, do_GetTextDimensions, &data);
  aDimensions.width = NSToCoordRound(float(data.mWidth) * mP2T);
  aDimensions.ascent = data.mAscent;
  aDimensions.descent = data.mDescent;

  if (mCurrFont != data.mFont) {
    // If the font was changed on the way, restore our font
    ::SelectObject(mDC, mCurrFont);
  }

  if (aFontID) *aFontID = 0;

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextWin :: DrawString(const char *aString, PRUint32 aLength,
                                                  nscoord aX, nscoord aY,
                                                  const nscoord* aSpacing)
{
  NS_PRECONDITION(mFontMetrics,"Something is wrong somewhere");

  PRInt32 x = aX;
  PRInt32 y = aY;

  SetupFontAndColor();

  INT dxMem[500];
  INT* dx0 = NULL;
  if (aSpacing) {
    dx0 = dxMem;
    if (aLength > 500) {
      dx0 = new INT[aLength];
    }
    mTranMatrix->ScaleXCoords(aSpacing, aLength, dx0);
  }
  mTranMatrix->TransformCoord(&x, &y);
  ::ExtTextOut(mDC, x, y, 0, NULL, aString, aLength, dx0);

  if (dx0 && (dx0 != dxMem)) {
    delete [] dx0;
  }

  return NS_OK;
}

struct DrawStringData {
  HDC            mDC;         // IN
  HFONT          mFont;       // IN/OUT (running)
  nsTransform2D* mTranMatrix; // IN
  nscoord        mX;          // IN/OUT (running)
  nscoord        mY;          // IN
  const nscoord* mSpacing;    // IN
  nscoord        mMaxLength;  // IN (length of the full string)
  nscoord        mLength;     // IN/OUT (running, current length already rendered)
};

static PRBool PR_CALLBACK
do_DrawString(const nsFontSwitch* aFontSwitch,
              const PRUnichar*    aSubstring,
              PRUint32            aSubstringLength,
              void*               aData)
{
  nsFontWin* fontWin = aFontSwitch->mFontWin;

  PRInt32 x, y;
  DrawStringData* data = (DrawStringData*)aData;
  if (data->mFont != fontWin->mFont) {
    data->mFont = fontWin->mFont;
    ::SelectObject(data->mDC, data->mFont);
  }

  data->mLength += aSubstringLength;
  if (data->mSpacing) {
    // XXX Fix path to use a twips transform in the DC and use the
    // spacing values directly and let windows deal with the sub-pixel
    // positioning.

    // Slow, but accurate rendering
    const PRUnichar* str = aSubstring;
    const PRUnichar* end = aSubstring + aSubstringLength;
    while (str < end) {
      // XXX can shave some cycles by inlining a version of transform
      // coord where y is constant and transformed once
      x = data->mX;
      y = data->mY;
      data->mTranMatrix->TransformCoord(&x, &y);
      fontWin->DrawString(data->mDC, x, y, str, 1);
      data->mX += *data->mSpacing++;
      ++str;
    }
  }
  else {
    fontWin->DrawString(data->mDC, data->mX, data->mY, aSubstring, aSubstringLength);
    // be ready if there is more to come
    if (data->mLength < data->mMaxLength) {
      data->mX += fontWin->GetWidth(data->mDC, aSubstring, aSubstringLength);
    }
  }
  return PR_TRUE; // don't stop till the end
}

NS_IMETHODIMP nsRenderingContextWin :: DrawString(const PRUnichar *aString, PRUint32 aLength,
                                                  nscoord aX, nscoord aY,
                                                  PRInt32 aFontID,
                                                  const nscoord* aSpacing)
{
  if (!mFontMetrics) return NS_ERROR_FAILURE;

  SetupFontAndColor();

  nsFontMetricsWin* metrics = (nsFontMetricsWin*)mFontMetrics;
  DrawStringData data = {mDC, mCurrFont, mTranMatrix, 
    aX, aY, aSpacing, aLength, 0
  };
  if (!aSpacing) { // @see do_DrawString for the spacing case
    mTranMatrix->TransformCoord(&data.mX, &data.mY);
  }

#ifdef IBMBIDI
  if (mRightToLeftText) {
    metrics->ResolveBackwards(mDC, aString, aLength, do_DrawString, &data);
  }
  else
#endif // IBMBIDI
  {
    metrics->ResolveForwards(mDC, aString, aLength, do_DrawString, &data);
  }

  if (mCurrFont != data.mFont) {
    // If the font was changed along the way, restore our font
    ::SelectObject(mDC, mCurrFont);
  }

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextWin :: DrawString(const nsString& aString,
                                                  nscoord aX, nscoord aY,
                                                  PRInt32 aFontID,
                                                  const nscoord* aSpacing)
{
  return DrawString(aString.get(), aString.Length(), aX, aY, aFontID, aSpacing);
}

#ifdef MOZ_MATHML
NS_IMETHODIMP 
nsRenderingContextWin::GetBoundingMetrics(const char*        aString,
                                          PRUint32           aLength,
                                          nsBoundingMetrics& aBoundingMetrics)
{
  NS_PRECONDITION(mFontMetrics,"Something is wrong somewhere");

  aBoundingMetrics.Clear();
  if (!mFontMetrics) return NS_ERROR_FAILURE;

  SetupFontAndColor();

  // set glyph transform matrix to identity
  MAT2 mat2;
  FIXED zero, one;
  zero.fract = 0; one.fract = 0;
  zero.value = 0; one.value = 1; 
  mat2.eM12 = mat2.eM21 = zero; 
  mat2.eM11 = mat2.eM22 = one; 

  // measure the string
  nscoord descent;
  GLYPHMETRICS gm;
  DWORD len = GetGlyphOutline(mDC, aString[0], GGO_METRICS, &gm, 0, nsnull, &mat2);
  if (GDI_ERROR == len) {
    return NS_ERROR_UNEXPECTED;
  }
  // flip sign of descent for cross-platform compatibility
  descent = -(nscoord(gm.gmptGlyphOrigin.y) - nscoord(gm.gmBlackBoxY));
  aBoundingMetrics.leftBearing = gm.gmptGlyphOrigin.x;
  aBoundingMetrics.rightBearing = gm.gmptGlyphOrigin.x + gm.gmBlackBoxX;
  aBoundingMetrics.ascent = gm.gmptGlyphOrigin.y;
  aBoundingMetrics.descent = gm.gmptGlyphOrigin.y - gm.gmBlackBoxY;
  aBoundingMetrics.width = gm.gmCellIncX;

  if (1 < aLength) {
    // loop over each glyph to get the ascent and descent
    PRUint32 i;
    for (i = 1; i < aLength; i++) {
      len = GetGlyphOutline(mDC, aString[i], GGO_METRICS, &gm, 0, nsnull, &mat2);
      if (GDI_ERROR == len) {
        return NS_ERROR_UNEXPECTED;
      }
      // flip sign of descent for cross-platform compatibility
      descent = -(nscoord(gm.gmptGlyphOrigin.y) - nscoord(gm.gmBlackBoxY));
      if (aBoundingMetrics.ascent < gm.gmptGlyphOrigin.y)
        aBoundingMetrics.ascent = gm.gmptGlyphOrigin.y;
      if (aBoundingMetrics.descent > descent)
        aBoundingMetrics.descent = descent;
    }
    // get the final rightBearing and width. Possible kerning is taken into account.
    SIZE size;
    ::GetTextExtentPoint32(mDC, aString, aLength, &size);
    aBoundingMetrics.width = size.cx;
    aBoundingMetrics.rightBearing = size.cx - gm.gmCellIncX + gm.gmBlackBoxX;
  }

  // convert to app units
  aBoundingMetrics.leftBearing = NSToCoordRound(float(aBoundingMetrics.leftBearing) * mP2T);
  aBoundingMetrics.rightBearing = NSToCoordRound(float(aBoundingMetrics.rightBearing) * mP2T);
  aBoundingMetrics.width = NSToCoordRound(float(aBoundingMetrics.width) * mP2T);
  aBoundingMetrics.ascent = NSToCoordRound(float(aBoundingMetrics.ascent) * mP2T);
  aBoundingMetrics.descent = NSToCoordRound(float(aBoundingMetrics.descent) * mP2T);

  return NS_OK;
}

struct GetBoundingMetricsData {
  HDC                mDC;              // IN
  HFONT              mFont;            // IN/OUT (running)
  nsBoundingMetrics* mBoundingMetrics; // IN/OUT (running)
  PRBool             mFirstTime;       // IN/OUT (set once)
  nsresult           mStatus;          // OUT
};

static PRBool PR_CALLBACK
do_GetBoundingMetrics(const nsFontSwitch* aFontSwitch,
                      const PRUnichar*    aSubstring,
                      PRUint32            aSubstringLength,
                      void*               aData)
{
  nsFontWin* fontWin = aFontSwitch->mFontWin;

  GetBoundingMetricsData* data = (GetBoundingMetricsData*)aData;
  if (data->mFont != fontWin->mFont) {
    data->mFont = fontWin->mFont;
    ::SelectObject(data->mDC, data->mFont);
  }

  nsBoundingMetrics rawbm;
  data->mStatus = fontWin->GetBoundingMetrics(data->mDC, aSubstring, aSubstringLength, rawbm);
  if (NS_FAILED(data->mStatus)) {
    return PR_FALSE; // stop now
  }

  if (data->mFirstTime) {
    data->mFirstTime = PR_FALSE;
    *data->mBoundingMetrics = rawbm;
  }
  else {
    *data->mBoundingMetrics += rawbm;
  }

  return PR_TRUE; // don't stop till the end
}

NS_IMETHODIMP
nsRenderingContextWin::GetBoundingMetrics(const PRUnichar*   aString,
                                          PRUint32           aLength,
                                          nsBoundingMetrics& aBoundingMetrics,
                                          PRInt32*           aFontID)
{
  aBoundingMetrics.Clear();
  if (!mFontMetrics) return NS_ERROR_FAILURE;

  SetupFontAndColor();

  nsFontMetricsWin* metrics = (nsFontMetricsWin*)mFontMetrics;
  GetBoundingMetricsData data = {mDC, mCurrFont, &aBoundingMetrics, PR_TRUE, NS_OK};

  nsresult rv = metrics->ResolveForwards(mDC, aString, aLength, do_GetBoundingMetrics, &data);
  if (NS_SUCCEEDED(rv)) {
    rv = data.mStatus;
  }

  // convert to app units
  aBoundingMetrics.leftBearing = NSToCoordRound(float(aBoundingMetrics.leftBearing) * mP2T);
  aBoundingMetrics.rightBearing = NSToCoordRound(float(aBoundingMetrics.rightBearing) * mP2T);
  aBoundingMetrics.width = NSToCoordRound(float(aBoundingMetrics.width) * mP2T);
  aBoundingMetrics.ascent = NSToCoordRound(float(aBoundingMetrics.ascent) * mP2T);
  aBoundingMetrics.descent = NSToCoordRound(float(aBoundingMetrics.descent) * mP2T);

  if (mCurrFont != data.mFont) {
    // If the font was changed along the way, restore our font
    ::SelectObject(mDC, mCurrFont);
  }

  if (aFontID) *aFontID = 0;

  return rv;
}
#endif // MOZ_MATHML

NS_IMETHODIMP nsRenderingContextWin :: DrawImage(nsIImage *aImage, nscoord aX, nscoord aY)
{
  NS_PRECONDITION(PR_TRUE == mInitialized, "!initialized");

  nscoord width, height;

  width = NSToCoordRound(mP2T * aImage->GetWidth());
  height = NSToCoordRound(mP2T * aImage->GetHeight());

  return DrawImage(aImage, aX, aY, width, height);
}

NS_IMETHODIMP nsRenderingContextWin :: DrawImage(nsIImage *aImage, nscoord aX, nscoord aY,
                                        nscoord aWidth, nscoord aHeight) 
{
  nsRect  tr;

  tr.x = aX;
  tr.y = aY;
  tr.width = aWidth;
  tr.height = aHeight;

  return DrawImage(aImage, tr);
}

NS_IMETHODIMP nsRenderingContextWin :: DrawImage(nsIImage *aImage, const nsRect& aSRect, const nsRect& aDRect)
{
  nsRect	sr,dr;

	sr = aSRect;
	mTranMatrix->TransformCoord(&sr.x, &sr.y, &sr.width, &sr.height);
	sr.x = aSRect.x;
	sr.y = aSRect.y;
	mTranMatrix->TransformNoXLateCoord(&sr.x, &sr.y);

  dr = aDRect;
	mTranMatrix->TransformCoord(&dr.x, &dr.y, &dr.width, &dr.height);

  return aImage->Draw(*this, mSurface, sr.x, sr.y, sr.width, sr.height, dr.x, dr.y, dr.width, dr.height);
}

NS_IMETHODIMP nsRenderingContextWin :: DrawImage(nsIImage *aImage, const nsRect& aRect)
{
  nsRect	tr;

	tr = aRect;
	mTranMatrix->TransformCoord(&tr.x, &tr.y, &tr.width, &tr.height);

  return aImage->Draw(*this, mSurface, tr.x, tr.y, tr.width, tr.height);
}


/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 *	@update 3/16/00 dwc
 */
PRBool 
nsRenderingContextWin::CanTile(nscoord aWidth,nscoord aHeight)
{
PRInt32 canRaster;

  // find out if the surface is a printer.
  ((nsDrawingSurfaceWin *)mSurface)->GetTECHNOLOGY(&canRaster);
  if(canRaster != DT_RASPRINTER){
    // XXX This may need tweaking for win98
    if (PR_TRUE == gIsWIN95) {
      // windows 98
      if((aWidth<8)&&(aHeight<8)){
        return PR_FALSE;    // this does not seem to work on win 98
      }else{
        return PR_FALSE;
      }
    }
    else {
      // windows NT
      return PR_FALSE;
    }
  } else {
    return PR_FALSE;
  }
  

}

NS_IMETHODIMP nsRenderingContextWin :: CopyOffScreenBits(nsDrawingSurface aSrcSurf,
                                                         PRInt32 aSrcX, PRInt32 aSrcY,
                                                         const nsRect &aDestBounds,
                                                         PRUint32 aCopyFlags)
{

  if ((nsnull != aSrcSurf) && (nsnull != mMainDC))
  {
    PRInt32 x = aSrcX;
    PRInt32 y = aSrcY;
    nsRect  drect = aDestBounds;
    HDC     destdc, srcdc;

    //get back a DC

    ((nsDrawingSurfaceWin *)aSrcSurf)->GetDC(&srcdc);

    if (nsnull != srcdc)
    {
      if (aCopyFlags & NS_COPYBITS_TO_BACK_BUFFER)
      {
        NS_ASSERTION(!(nsnull == mDC), "no back buffer");
        destdc = mDC;
      }
      else
        destdc = mMainDC;

      if (aCopyFlags & NS_COPYBITS_USE_SOURCE_CLIP_REGION)
      {
        HRGN  tregion = ::CreateRectRgn(0, 0, 0, 0);

        if (::GetClipRgn(srcdc, tregion) == 1)
          ::SelectClipRgn(destdc, tregion);

        ::DeleteObject(tregion);
      }

      if (aCopyFlags & NS_COPYBITS_XFORM_SOURCE_VALUES)
        mTranMatrix->TransformCoord(&x, &y);

      if (aCopyFlags & NS_COPYBITS_XFORM_DEST_VALUES)
        mTranMatrix->TransformCoord(&drect.x, &drect.y, &drect.width, &drect.height);

      ::BitBlt(destdc, drect.x, drect.y,
               drect.width, drect.height,
               srcdc, x, y, SRCCOPY);


      //kill the DC
      ((nsDrawingSurfaceWin *)aSrcSurf)->ReleaseDC();
    }
    else
      NS_ASSERTION(0, "attempt to blit with bad DCs");
  }
  else
    NS_ASSERTION(0, "attempt to blit with bad DCs");

  return NS_OK;
}

//~~~
NS_IMETHODIMP nsRenderingContextWin::RetrieveCurrentNativeGraphicData(PRUint32 * ngd)
{
  if(ngd != nsnull)
    *ngd = (PRUint32)mDC;
  return NS_OK;
}

// Small cache of HBRUSH objects
// Note: the current assumption is that there is only one UI thread so
// we do not lock, and we do not use TLS
static const int  BRUSH_CACHE_SIZE = 17;  // 2 stock plus 15

class SolidBrushCache {
public:
  SolidBrushCache();
  ~SolidBrushCache();

  HBRUSH  GetSolidBrush(COLORREF aColor);

private:
  struct CacheEntry {
    HBRUSH   mBrush;
    COLORREF mBrushColor;
  };

  CacheEntry  mCache[BRUSH_CACHE_SIZE];
  int         mIndexOldest;  // index of oldest entry in cache
};

SolidBrushCache::SolidBrushCache()
  : mIndexOldest(2)
{
  // First two entries are stock objects
  mCache[0].mBrush = gStockWhiteBrush;
  mCache[0].mBrushColor = RGB(255, 255, 255);
  mCache[1].mBrush = (HBRUSH)::GetStockObject(BLACK_BRUSH);
  mCache[1].mBrushColor = RGB(0, 0, 0);
}

SolidBrushCache::~SolidBrushCache()
{
  // No need to delete the stock objects
  for (int i = 2; i < BRUSH_CACHE_SIZE; i++) {
    if (mCache[i].mBrush) {
      ::DeleteObject(mCache[i].mBrush);
    }
  }
}

HBRUSH
SolidBrushCache::GetSolidBrush(COLORREF aColor)
{
  int     i;
  HBRUSH  result = NULL;
  
  // See if it's already in the cache
  for (i = 0; (i < BRUSH_CACHE_SIZE) && mCache[i].mBrush; i++) {
    if (mCache[i].mBrush && (mCache[i].mBrushColor == aColor)) {
      // Found an existing brush
      result = mCache[i].mBrush;
      break;
    }
  }

  if (!result) {
    // We didn't find it in the set of existing brushes, so create a
    // new brush
    result = (HBRUSH)::CreateSolidBrush(PALETTERGB_COLORREF(aColor));

    // If there's an empty slot in the cache, then just add it there
    if (i >= BRUSH_CACHE_SIZE) {
      // Nope. The cache is full so we need to replace the oldest entry
      // in the cache
      ::DeleteObject(mCache[mIndexOldest].mBrush);
      i = mIndexOldest;
      if (++mIndexOldest >= BRUSH_CACHE_SIZE) {
        mIndexOldest = 2;
      }
    }

    // Add the new entry
    mCache[i].mBrush = result;
    mCache[i].mBrushColor = aColor;
  }

  return result;
}

static SolidBrushCache  gSolidBrushCache;

HBRUSH nsRenderingContextWin :: SetupSolidBrush(void)
{
  if ((mCurrentColor != mCurrBrushColor) || (NULL == mCurrBrush))
  {
    HBRUSH tbrush = gSolidBrushCache.GetSolidBrush(mColor);
    
    ::SelectObject(mDC, tbrush);
    mCurrBrush = tbrush;
    mCurrBrushColor = mCurrentColor;
  }

  return mCurrBrush;
}

void nsRenderingContextWin :: SetupFontAndColor(void)
{
  if (((mFontMetrics != mCurrFontMetrics) || (NULL == mCurrFontMetrics)) &&
      (nsnull != mFontMetrics))
  {
    nsFontHandle  fontHandle;
    mFontMetrics->GetFontHandle(fontHandle);
    HFONT         tfont = (HFONT)fontHandle;
    
    ::SelectObject(mDC, tfont);

    mCurrFont = tfont;

    nsFontMetricsWin* metrics = (nsFontMetricsWin*)mFontMetrics;
    mCurrFontWin = metrics->GetFontFor(mCurrFont);

    mCurrFontMetrics = mFontMetrics;
  }

  if (mCurrentColor != mCurrTextColor)
  {
    ::SetTextColor(mDC, PALETTERGB_COLORREF(mColor));
    mCurrTextColor = mCurrentColor;
  }
}

HPEN nsRenderingContextWin :: SetupPen()
{
  HPEN pen;

  switch(mCurrLineStyle)
  {
    case nsLineStyle_kSolid:
      pen = SetupSolidPen();
      break;

    case nsLineStyle_kDashed:
      pen = SetupDashedPen();
      break;

    case nsLineStyle_kDotted:
      pen = SetupDottedPen();
      break;

    case nsLineStyle_kNone:
      pen = NULL;
      break;

    default:
      pen = SetupSolidPen();
      break;
  }

  return pen;
}


HPEN nsRenderingContextWin :: SetupSolidPen(void)
{
  if ((mCurrentColor != mCurrPenColor) || (NULL == mCurrPen) || (mCurrPen != mStates->mSolidPen))
  {
    HPEN  tpen;
     
    if (RGB(0, 0, 0) == mColor) {
      tpen = gStockBlackPen;
    } else if (RGB(255, 255, 255) == mColor) {
      tpen = gStockWhitePen;
    } else {
      tpen = ::CreatePen(PS_SOLID, 0, PALETTERGB_COLORREF(mColor));
    }

    ::SelectObject(mDC, tpen);

    if (mCurrPen && (mCurrPen != gStockBlackPen) && (mCurrPen != gStockWhitePen)) {
      VERIFY(::DeleteObject(mCurrPen));
    }

    mStates->mSolidPen = mCurrPen = tpen;
    mCurrPenColor = mCurrentColor;
  }

  return mCurrPen;
}

HPEN nsRenderingContextWin :: SetupDashedPen(void)
{
  if ((mCurrentColor != mCurrPenColor) || (NULL == mCurrPen) || (mCurrPen != mStates->mDashedPen))
  {
    HPEN  tpen = ::CreatePen(PS_DASH, 0, PALETTERGB_COLORREF(mColor));

    ::SelectObject(mDC, tpen);

    if (NULL != mCurrPen)
      VERIFY(::DeleteObject(mCurrPen));

    mStates->mDashedPen = mCurrPen = tpen;
    mCurrPenColor = mCurrentColor;
  }

  return mCurrPen;
}

HPEN nsRenderingContextWin :: SetupDottedPen(void)
{
  if ((mCurrentColor != mCurrPenColor) || (NULL == mCurrPen) || (mCurrPen != mStates->mDottedPen))
  {
    HPEN  tpen = ::CreatePen(PS_DOT, 0, PALETTERGB_COLORREF(mColor));

    ::SelectObject(mDC, tpen);

    if (NULL != mCurrPen)
      VERIFY(::DeleteObject(mCurrPen));

    mStates->mDottedPen = mCurrPen = tpen;
    mCurrPenColor = mCurrentColor;
  }

  return mCurrPen;
}

//========================================================

NS_IMETHODIMP 
nsRenderingContextWin::SetPenMode(nsPenMode aPenMode)
{

  switch(aPenMode){
  case nsPenMode_kNone:
    ::SetROP2(mDC,R2_COPYPEN);
    mPenMode = nsPenMode_kNone;
    break;
  case nsPenMode_kInvert:
    ::SetROP2(mDC,R2_NOT);
    mPenMode = nsPenMode_kInvert;
    break;
  }

  return NS_OK;
}

//========================================================

NS_IMETHODIMP 
nsRenderingContextWin::GetPenMode(nsPenMode &aPenMode)
{
  // can use the ::GetROP2(mDC); for debugging, see if windows is in the correct mode
  aPenMode = mPenMode;

  return NS_OK;
}

//========================================================

void nsRenderingContextWin :: PushClipState(void)
{
  if (!(mStates->mFlags & FLAG_CLIP_CHANGED))
  {
    GraphicsState *tstate = mStates->mNext;

    //we have never set a clip on this state before, so
    //remember the current clip state in the next state on the
    //stack. kind of wacky, but avoids selecting stuff in the DC
    //all the damned time.

    if (nsnull != tstate)
    {
      if (NULL == tstate->mClipRegion)
        tstate->mClipRegion = ::CreateRectRgn(0, 0, 0, 0);

      if (::GetClipRgn(mDC, tstate->mClipRegion) == 1)
        tstate->mFlags |= FLAG_CLIP_VALID;
      else
        tstate->mFlags &= ~FLAG_CLIP_VALID;
    }
  
    mStates->mFlags |= FLAG_CLIP_CHANGED;
  }
}

NS_IMETHODIMP nsRenderingContextWin :: CreateDrawingSurface(HDC aDC, nsDrawingSurface &aSurface)
{
  nsDrawingSurfaceWin *surf = new nsDrawingSurfaceWin();

  if (nsnull != surf)
  {
    NS_ADDREF(surf);
    surf->Init(aDC);
  }

  aSurface = (nsDrawingSurface)surf;

  return NS_OK;
}

/**
 * ConditionRect is used to fix a coordinate overflow problem under WIN95/WIN98.
 * Some operations fail for rectangles whose coordinates have very large
 * absolute values.  Since these values are off the screen, they can be
 * truncated to reasonable ones.
 *
 * @param aSrcRect input rectangle
 * @param aDestRect output rectangle, same as input except with large
 *                  coordinates changed so they are acceptable to WIN95/WIN98
 */
// XXX: TODO find all calls under WIN95 that will fail if passed large coordinate values
// and make calls to this method to fix them.

void 
nsRenderingContextWin::ConditionRect(nsRect& aSrcRect, RECT& aDestRect)
{
   // XXX: TODO find the exact values for the and bottom limits. These limits were determined by
   // printing out the RECT coordinates and noticing when they failed. There must be an offical
   // document that describes what the coordinate limits are for calls
   // such as ::FillRect and ::IntersectClipRect under WIN95 which fail when large negative and
   // position values are passed.

   // The following is for WIN95. If range of legal values for the rectangles passed for 
   // clipping and drawing is smaller on WIN95 than under WINNT.                              
  const nscoord kBottomRightLimit = 16384;
  const nscoord kTopLeftLimit = -8192;

  aDestRect.top = (aSrcRect.y < kTopLeftLimit)
                      ? kTopLeftLimit
                      : aSrcRect.y;
  aDestRect.bottom = ((aSrcRect.y + aSrcRect.height) > kBottomRightLimit)
                      ? kBottomRightLimit
                      : (aSrcRect.y+aSrcRect.height);
  aDestRect.left = (aSrcRect.x < kTopLeftLimit)
                      ? kTopLeftLimit
                      : aSrcRect.x;
  aDestRect.right = ((aSrcRect.x + aSrcRect.width) > kBottomRightLimit)
                      ? kBottomRightLimit
                      : (aSrcRect.x+aSrcRect.width);
}

#ifdef IBMBIDI
/**
 * Let the device context know whether we want text reordered with
 * right-to-left base direction. The Windows implementation does this
 * by calling SetTextAlign() to set or clear the flag TA_RTLREADING
 */
NS_IMETHODIMP
nsRenderingContextWin::SetRightToLeftText(PRBool aIsRTL)
{
  // Only call SetTextAlign if the new value is different from the
  // current value
  if (aIsRTL != mRightToLeftText) {
    UINT flags = ::GetTextAlign(mDC);
    if (aIsRTL) {
      flags |= TA_RTLREADING;
    }
    else {
      flags &= (~TA_RTLREADING);
    }
    ::SetTextAlign(mDC, flags);
  }

  mRightToLeftText = aIsRTL;
  return NS_OK;
}

/**
 * Init <code>gBidiInfo</code> with reordering and shaping 
 * capabilities of the system
 */
void
nsRenderingContextWin::InitBidiInfo()
{
  if (NOT_SETUP == gBidiInfo) {
    gBidiInfo = DONT_INIT;

    const PRUnichar araAin  = 0x0639;
    const PRUnichar one     = 0x0031;

    int distanceArray[2];
    PRUnichar glyphArray[2];
    PRUnichar outStr[] = {0, 0};

    GCP_RESULTSW gcpResult;
    gcpResult.lStructSize = sizeof(GCP_RESULTS);
    gcpResult.lpOutString = outStr;     // Output string
    gcpResult.lpOrder = nsnull;         // Ordering indices
    gcpResult.lpDx = distanceArray;     // Distances between character cells
    gcpResult.lpCaretPos = nsnull;      // Caret positions
    gcpResult.lpClass = nsnull;         // Character classifications
    gcpResult.lpGlyphs = glyphArray;    // Character glyphs
    gcpResult.nGlyphs = 2;              // Array size

    PRUnichar inStr[] = {araAin, one};

    if (::GetCharacterPlacementW(mDC, inStr, 2, 0, &gcpResult, GCP_REORDER) 
        && (inStr[0] == outStr[1]) ) {
      gBidiInfo = GCP_REORDER | GCP_GLYPHSHAPE;
#ifdef NS_DEBUG
      printf("System has shaping\n");
#endif
    }
    else {
      const PRUnichar hebAlef = 0x05D0;
      inStr[0] = hebAlef;
      inStr[1] = one;
      if (::GetCharacterPlacementW(mDC, inStr, 2, 0, &gcpResult, GCP_REORDER) 
          && (inStr[0] == outStr[1]) ) {
        gBidiInfo = GCP_REORDER;
#ifdef NS_DEBUG
        printf("System has Bidi\n");
#endif
      }
    }
  }
}
#endif // IBMBIDI



