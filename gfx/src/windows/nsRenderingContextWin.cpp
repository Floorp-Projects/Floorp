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
    delete pp0;

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
    delete pp0;

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
    delete pp0;

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
    delete pp0;

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



#ifdef FIX_FOR_BUG_6585
// This function will substitute non-ascii 7-bit chars so that
// the final rendering will look like: char{U+00E5} -- see bug 6585.
// For example, "å" will be render as "å{U+00E5}".

static PRUnichar*
SubstituteNonAsciiChars(const PRUnichar* aString, 
                        PRUint32         aLength,
                        PRUnichar*       aBuffer, 
                        PRUint32         aBufferLength, 
                        PRUint32*        aCount)
{
  // This code is optimized so that no work is done if all
  // chars happen to be 7-bit ascii chars.
  char cbuf[10];
  PRUint32 count = 0;
  PRUnichar* result;
  for (PRUint32 i = 0; i < aLength; i++) {
    if (aString[i] > PRUnichar(0x7F)) {
      if (0 == count) { 
        // A non 7-bit char is encountered for the first time... 
        // So... switch to a separate array, and copy the previous chars there.
        // At worse, each char will eat 9 PRUnichars (1 for the char itself,
        // and 8 for its unicode "{U+NNNN}")
        if (aLength*9 < aBufferLength) { // use the proposed buffer
          result = aBuffer;
        }
        else { // allocated an array --caller will release
          result = new PRUnichar[aLength*9];
          if (!result) break; // do nothing
        }
        for (count = 0; count < i; count++)
          result[count] =  aString[count]; 
      }
      // From now on, add all non-ascii chars followed by their {U+NNNN} ...
      result[count++] = aString[i]; // the char istelf
      PR_snprintf(cbuf, sizeof(cbuf), "{U+%04X}", aString[i]);
      for (PRUint32 j = 0; j < 8; j++) // its unicode
        result[count++] = PRUnichar(cbuf[j]); 
    }
    else if (0 < count) {
      // ... And simply add ascii chars.
      result[count++] = aString[i];
    }
  }
  *aCount = (0 < count)? count : aLength;
  return (0 < count)? result : (PRUnichar *)(const PRUnichar *)aString;
}
#endif

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

NS_IMETHODIMP
nsRenderingContextWin::GetWidth(const char *aString,
                                PRInt32     aLength,
                                PRInt32     aAvailWidth,
                                PRInt32*    aBreaks,
                                PRInt32     aNumBreaks,
                                nscoord&    aWidth,
                                PRInt32&    aNumCharsFit,
                                PRInt32*    aFontID = nsnull)
{
  NS_PRECONDITION(aBreaks[aNumBreaks - 1] == aLength, "invalid break array");

  if (nsnull != mFontMetrics) {
    // If we need to back up this state represents the last place we could
    // break. We can use this to avoid remeasuring text
    struct PrevBreakState {
      PRInt32   mBreakIndex;
      nscoord   mWidth;    // accumulated width to this point

      PrevBreakState() {
        mBreakIndex = -1;  // not known (hasn't been computed)
        mWidth = 0;
      }
    };

    // Initialize OUT parameter
    aNumCharsFit = 0;

    // Setup the font and foreground color
    SetupFontAndColor();

    // Iterate each character in the string and determine which font to use
    nsFontMetricsWin* metrics = (nsFontMetricsWin*)mFontMetrics;
    PrevBreakState    prevBreakState;
    nscoord           width = 0;
    PRInt32           start = 0;
    nscoord           aveCharWidth;
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

      } else {
        breakIndex = prevBreakState.mBreakIndex;
        while (((breakIndex + 1) < aNumBreaks) &&
               (aBreaks[breakIndex + 1] <= estimatedBreakOffset)) {
          breakIndex++;
        }
        if (breakIndex == prevBreakState.mBreakIndex) {
          breakIndex++; // make sure we advanced past the previous break index
        }
        numChars = aBreaks[breakIndex] - start;
      }

      // Measure the text
      nscoord twWidth;
      if ((1 == numChars) && (aString[start] == ' ')) {
        metrics->GetSpaceWidth(twWidth);

      } else {
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
        prevBreakState.mBreakIndex = breakIndex;
        prevBreakState.mWidth = width;

      } else {
        // See if we can just back up to the previous saved state and not
        // have to measure any text
        if (prevBreakState.mBreakIndex > 0) {
          // If the previous break index is just before the current break index
          // then we can use it
          if (prevBreakState.mBreakIndex == (breakIndex - 1)) {
            aNumCharsFit = aBreaks[prevBreakState.mBreakIndex];
            width = prevBreakState.mWidth;
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
          start = aBreaks[breakIndex - 1];
          numChars = aBreaks[breakIndex] - start;
          
          if ((1 == numChars) && (aString[start] == ' ')) {
            metrics->GetSpaceWidth(twWidth);

          } else {
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

    aWidth = width;
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsRenderingContextWin :: GetWidth(const nsString& aString, nscoord& aWidth, PRInt32 *aFontID)
{
  return GetWidth(aString.get(), aString.Length(), aWidth, aFontID);
}

NS_IMETHODIMP nsRenderingContextWin :: GetWidth(const PRUnichar *aString,
                                                PRUint32 aLength,
                                                nscoord &aWidth,
                                                PRInt32 *aFontID)
{
  if (nsnull != mFontMetrics)
  {
    PRUnichar* pstr = (PRUnichar *)(const PRUnichar *)aString;
#ifdef FIX_FOR_BUG_6585
    // XXX code is OK - remaining issue: 'substituteNonAsciiChars' has to 
    // be initialized based on the desired rendering mode -- see bug 6585
    PRBool substituteNonAsciiChars = PR_FALSE;
    PRUnichar str[8192];
    if (substituteNonAsciiChars) { 
      pstr = SubstituteNonAsciiChars(aString, aLength, str, 8192, &aLength);
    }
#endif

    nsFontMetricsWin* metrics = (nsFontMetricsWin*) mFontMetrics;
    nsFontWin* prevFont = nsnull;

    SetupFontAndColor();
    HFONT selectedFont = mCurrFont;

    LONG width = 0;
    PRUint32 start = 0;
    for (PRUint32 i = 0; i < aLength; i++) {
      PRUnichar c = pstr[i];
      nsFontWin* currFont = nsnull;
      nsFontWin** font = metrics->mLoadedFonts;
      nsFontWin** end = &metrics->mLoadedFonts[metrics->mLoadedFontsCount];
      while (font < end) {
        if (FONT_HAS_GLYPH((*font)->mMap, c)) {
          currFont = *font;
          goto FoundFont; // for speed -- avoid "if" statement
        }
        font++;
      }
      currFont = metrics->FindFont(mDC, c);
FoundFont:
      // XXX avoid this test by duplicating code
      if (prevFont) {
        if (currFont != prevFont) {
          if (prevFont->mFont != selectedFont) {
            ::SelectObject(mDC, prevFont->mFont);
            selectedFont = prevFont->mFont;
          }
          width += prevFont->GetWidth(mDC, &pstr[start], i - start);
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
      if (prevFont->mFont != selectedFont) {
        ::SelectObject(mDC, prevFont->mFont);
        selectedFont = prevFont->mFont;
      }
      width += prevFont->GetWidth(mDC, &pstr[start], i - start);
    }

    aWidth = NSToCoordRound(float(width) * mP2T);

    if (selectedFont != mCurrFont) {
      // Restore the font
      ::SelectObject(mDC, mCurrFont);
    }

    if (nsnull != aFontID)
      *aFontID = 0;

#ifdef FIX_FOR_BUG_6585
    if (pstr != aString && pstr != str) {
      delete[] pstr;
    }
#endif
    return NS_OK;
  }
  else
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsRenderingContextWin::GetWidth(const PRUnichar *aString,
                                PRInt32          aLength,
                                PRInt32          aAvailWidth,
                                PRInt32*         aBreaks,
                                PRInt32          aNumBreaks,
                                nscoord&         aWidth,
                                PRInt32&         aNumCharsFit,
                                PRInt32*         aFontID)
{
  NS_PRECONDITION(aBreaks[aNumBreaks - 1] == aLength, "invalid break array");

  if (nsnull != mFontMetrics) {
    // If we need to back up this state represents the last place we could
    // break. We can use this to avoid remeasuring text
    struct PrevBreakState {
      PRInt32   mBreakIndex;
      nscoord   mWidth;

      PrevBreakState() {
        mBreakIndex = -1;  // not known (hasn't been computed)
        mWidth = 0;
      }
    };

    PRUnichar*          pstr = (PRUnichar *)(const PRUnichar *)aString;
    nsFontMetricsWin*   metrics = (nsFontMetricsWin*) mFontMetrics;
    nsFontWin*          prevFont = nsnull;
    PrevBreakState      prevBreakState;
    
    // Initialize OUT parameter
    aNumCharsFit = 0;

    // Setup the font and foreground color
    SetupFontAndColor();

    // Iterate each character in the string and determine which font to use
    HFONT     selectedFont = mCurrFont;
    nscoord   width = 0;
    PRInt32   start = 0;
    PRInt32   i = 0;
    PRBool    allDone = PR_FALSE;
    while (!allDone) {
      nsFontWin*        currFont = nsnull;
      nsFontMetricsWin* fontMetricsWin;
      nscoord           aveCharWidth;

      // First search the fonts we already have loaded
      nsFontWin** font = metrics->mLoadedFonts;
      nsFontWin** end = &metrics->mLoadedFonts[metrics->mLoadedFontsCount];
      while (font < end) {
        if (FONT_HAS_GLYPH((*font)->mMap, pstr[i])) {
          currFont = *font;
          goto FoundFont;
        }
        font++;  // try the next font
      }

      // No match in the already loaded fonts. Find a new font
      currFont = metrics->FindFont(mDC, pstr[i]);

     FoundFont:
      // See if this is the same font we found for the previous character.
      // If so, then keep accumulating characters
      if (prevFont) {
        if (currFont != prevFont) {
          // The font has changed. We need to measure the existing text using
          // the previous font
          goto MeasureText;
        }
      }
      else if(currFont) {
        prevFont = currFont;  // remember this font
      } else {
        NS_WARNING("no previous font and cannot find current font");
        // this should never happen, but if it does we will continue walking the text
        // and hope that a later font is found so we can use that to measure
      }

      // Advance to the next character
      if (++i >= aLength) {
        allDone = PR_TRUE;
        goto MeasureText;  // measure any remaining text
      }
      continue;

     MeasureText:
      // Make sure the font is selected
      NS_ASSERTION(prevFont, "must have a font to measure text!");
      if (prevFont &&
          prevFont->mFont != selectedFont ) {
        NS_ASSERTION(mDC && prevFont->mFont, "null args to SelectObject");
        ::SelectObject(mDC, prevFont->mFont);
        selectedFont = prevFont->mFont;
      }

      // Get the average character width
      fontMetricsWin = (nsFontMetricsWin*)mFontMetrics;
      fontMetricsWin->GetAveCharWidth(aveCharWidth);
      while (start < i) {
        // Estimate how many characters will fit. Do that by diving the available
        // space by the average character width
        PRInt32 estimatedNumChars = 0;
        if (aveCharWidth > 0) {
          estimatedNumChars = (aAvailWidth - width) / aveCharWidth;
        }
        // Make sure the estimated number of characters is at least 1
        if (estimatedNumChars < 1) {
          estimatedNumChars = 1;
        }

        // Find the nearest break offset
        PRInt32 estimatedBreakOffset = start + estimatedNumChars;
        PRInt32 breakIndex = -1;  // not computed
        PRBool  inMiddleOfSegment = PR_FALSE;
        nscoord numChars;

        // Avoid scanning the break array in the case where we think all
        // the text should fit
        if (i <= estimatedBreakOffset) {
          // Everything should fit
          numChars = i - start;

        } else {
          // Find the nearest place to break that is less than or equal to
          // the estimated break offset
          breakIndex = prevBreakState.mBreakIndex;
          while (aBreaks[breakIndex + 1] <= estimatedBreakOffset) {
            breakIndex++;
          }

          if (breakIndex == -1)
              breakIndex = 0;
 
          // We found a place to break that is before the estimated break
          // offset. Where we break depends on whether the text crosses a
          // segment boundary
          if (start < aBreaks[breakIndex]) {
            // The text crosses at least one segment boundary so measure to the
            // break point just before the estimated break offset
            numChars = aBreaks[breakIndex] - start;

          } else {
            // See whether there is another segment boundary between this one
            // and the end of the text
            if ((breakIndex < (aNumBreaks - 1)) && (aBreaks[breakIndex] < i)) {
              breakIndex++;
              numChars = aBreaks[breakIndex] - start;

            } else {
              NS_ASSERTION(i != aBreaks[breakIndex], "don't expect to be at segment boundary");

              // The text is all within the same segment
              numChars = i - start;

              // Remember we're in the middle of a segment and not in between
              // two segments
              inMiddleOfSegment = PR_TRUE;
            }
          }
        }

        // Measure the text
        PRInt32 pxWidth;
        nscoord twWidth;
        if ((1 == numChars) && (pstr[start] == ' ')) {
          fontMetricsWin->GetSpaceWidth(twWidth);

        } else {
          if(prevFont)
             pxWidth = prevFont->GetWidth(mDC, &pstr[start], numChars);
          else
             pxWidth = 0;
          twWidth = NSToCoordRound(float(pxWidth) * mP2T);
        }

        // See if the text fits
        PRBool  textFits = (twWidth + width) <= aAvailWidth;

        // If the text fits then update the width and the number of
        // characters that fit
        if (textFits) {
          aNumCharsFit += numChars;
          width += twWidth;

          // If we computed the break index and we're not in the middle
          // of a segment then this is a spot that we can back up to if
          // we need to so remember this state
          if ((breakIndex != -1) && !inMiddleOfSegment) {
            prevBreakState.mBreakIndex = breakIndex;
            prevBreakState.mWidth = width;
          }

        } else {
          // The text didn't fit. If we're out of room then we're all done
          allDone = PR_TRUE;
          
          // See if we can just back up to the previous saved state and not
          // have to measure any text
          if (prevBreakState.mBreakIndex != -1) {
            PRBool  canBackup;

            // If we're in the middle of a word then the break index
            // must be the same if we can use it. If we're at a segment
            // boundary, then if the saved state is for the previous
            // break index then we can use it
            if (inMiddleOfSegment) {
              canBackup = prevBreakState.mBreakIndex == breakIndex;
            } else {
              canBackup = prevBreakState.mBreakIndex == (breakIndex - 1);
            }

            if (canBackup) {
              aNumCharsFit = aBreaks[prevBreakState.mBreakIndex];
              width = prevBreakState.mWidth;
              break;
            }
          }
          
          // We can't just revert to the previous break state. Find the break
          // index just before the end of the text
          i = start + numChars;
          if (breakIndex == -1) {
            breakIndex = 0;
            if (aBreaks[breakIndex] < i)
            {
                while (((breakIndex + 1) < aNumBreaks) && (aBreaks[breakIndex + 1] < i)) {
                  breakIndex++;
                }
            }
          }

          if ((0 == breakIndex) && (i <= aBreaks[0])) {
            // There's no place to back up to so even though the text doesn't fit
            // return it anyway
            aNumCharsFit += numChars;
            width += twWidth;
            break;
          }

          // Repeatedly back up until we get to where the text fits or we're
          // all the way back to the first word
          width += twWidth;
          while ((breakIndex >= 0) && (width > aAvailWidth)) {
            start = aBreaks[breakIndex];
            numChars = i - start;
            if ((1 == numChars) && (pstr[start] == ' ')) {
              fontMetricsWin->GetSpaceWidth(twWidth);

            } else {
              if(prevFont)
                 pxWidth = prevFont->GetWidth(mDC, &pstr[start], numChars);
              else
                 pxWidth =0;
              twWidth = NSToCoordRound(float(pxWidth) * mP2T);
            }

            width -= twWidth;
            aNumCharsFit = start;
            breakIndex--;
            i = start;
          }
        }

        start += numChars;
      }

      // We're done measuring that run
      if (!allDone) {
        prevFont = currFont;
        NS_ASSERTION(start == i, "internal error");
  
        // Advance to the next character
        if (++i >= aLength) {
          allDone = PR_TRUE;
		  goto MeasureText;
        }
      }
    }

    aWidth = width;

    if (selectedFont != mCurrFont) {
      // Restore the font
      ::SelectObject(mDC, mCurrFont);
    }

    if (nsnull != aFontID) {
      *aFontID = 0;
    }

    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsRenderingContextWin :: DrawString(const char *aString, PRUint32 aLength,
                                                  nscoord aX, nscoord aY,
                                                  const nscoord* aSpacing)
{
  NS_PRECONDITION(mFontMetrics,"Something is wrong somewhere");

  // Take care of ascent and specifies the drawing on the baseline
  nscoord ascent;
  mFontMetrics->GetMaxAscent(ascent);
  aY += ascent;

  PRInt32 x = aX;
  PRInt32 y = aY;

  SetupFontAndColor();

  INT dxMem[500];
  INT* dx0;
  if (nsnull != aSpacing) {
    dx0 = dxMem;
    if (aLength > 500) {
      dx0 = new INT[aLength];
    }
    mTranMatrix->ScaleXCoords(aSpacing, aLength, dx0);
  }
  mTranMatrix->TransformCoord(&x, &y);
  ::ExtTextOut(mDC, x, y, 0, NULL, aString, aLength, aSpacing ? dx0 : NULL);

  if ((nsnull != aSpacing) && (dx0 != dxMem)) {
    delete [] dx0;
  }

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextWin :: DrawString(const PRUnichar *aString, PRUint32 aLength,
                                                  nscoord aX, nscoord aY,
                                                  PRInt32 aFontID,
                                                  const nscoord* aSpacing)
{
  if (nsnull != mFontMetrics)
  {
    PRUnichar* pstr = (PRUnichar *)(const PRUnichar *)aString;
#ifdef FIX_FOR_BUG_6585
    // XXX code is OK - remaining issue: 'substituteNonAsciiChars' has to 
    // be initialized based on the rendering mode -- see bug 6585
    PRBool substituteNonAsciiChars = PR_FALSE;
    PRUnichar str[8192];
    if (substituteNonAsciiChars) { 
      pstr = SubstituteNonAsciiChars(aString, aLength, str, 8192, &aLength);
    }
#endif

    // Take care of the ascent since the drawing is on the baseline
    nscoord ascent;
    mFontMetrics->GetMaxAscent(ascent);
    aY += ascent;

    PRInt32 x = aX;
    PRInt32 y = aY;
    mTranMatrix->TransformCoord(&x, &y);
    nsFontMetricsWin* metrics = (nsFontMetricsWin*) mFontMetrics;
    nsFontWin* prevFont = nsnull;

    SetupFontAndColor();
    HFONT selectedFont = mCurrFont;

    PRUint32 start = 0;
    for (PRUint32 i = 0; i < aLength; i++) {
      PRUnichar c = pstr[i];

#ifdef IBMBIDI
      if (mRightToLeftText) {
        c = pstr[aLength - i - 1];
      }
#endif // IBMBIDI

      nsFontWin* currFont = nsnull;
      nsFontWin** font = metrics->mLoadedFonts;
      nsFontWin** end = &metrics->mLoadedFonts[metrics->mLoadedFontsCount];
      while (font < end) {
        if (FONT_HAS_GLYPH((*font)->mMap, c)) {
          currFont = *font;
          goto FoundFont; // for speed -- avoid "if" statement
        }
        font++;
      }
      currFont = metrics->FindFont(mDC, c);
FoundFont:
      if (prevFont) {
        if (currFont != prevFont) {
          if (prevFont->mFont != selectedFont) {
            ::SelectObject(mDC, prevFont->mFont);
            selectedFont = prevFont->mFont;
          }
          if (aSpacing) {
            // XXX Fix path to use a twips transform in the DC and use the
            // spacing values directly and let windows deal with the sub-pixel
            // positioning.

            // Slow, but accurate rendering
            const PRUnichar* str = &pstr[start];
            const PRUnichar* end = &pstr[i];
            while (str < end) {
              // XXX can shave some cycles by inlining a version of transform
              // coord where y is constant and transformed once
              x = aX;
              y = aY;
              mTranMatrix->TransformCoord(&x, &y);
              prevFont->DrawString(mDC, x, y, str, 1);
              aX += *aSpacing++;
              str++;
            }
          }
          else {
#ifdef IBMBIDI
            if (mRightToLeftText) {
              prevFont->DrawString(mDC, x, y, &pstr[aLength - i], i - start);
              x += prevFont->GetWidth(mDC, &pstr[aLength - i], i - start);
            }
            else
#endif // IBMBIDI
            {
              prevFont->DrawString(mDC, x, y, &pstr[start], i - start);
              x += prevFont->GetWidth(mDC, &pstr[start], i - start);
            }
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
      if (prevFont->mFont != selectedFont) {
        ::SelectObject(mDC, prevFont->mFont);
        selectedFont = prevFont->mFont;
      }
      if (aSpacing) {
        // XXX Fix path to use a twips transform in the DC and use the
        // spacing values directly and let windows deal with the sub-pixel
        // positioning.

        // Slow, but accurate rendering
        const PRUnichar* str = &pstr[start];
        const PRUnichar* end = &pstr[i];
        while (str < end) {
          // XXX can shave some cycles by inlining a version of transform
          // coord where y is constant and transformed once
          x = aX;
          y = aY;
          mTranMatrix->TransformCoord(&x, &y);
          prevFont->DrawString(mDC, x, y, str, 1);
          aX += *aSpacing++;
          str++;
        }
      }
      else {
#ifdef IBMBIDI
          if (mRightToLeftText) {
            prevFont->DrawString(mDC, x, y, &pstr[aLength - i], i - start);
          }
          else
#endif // IBMBIDI
        {
          prevFont->DrawString(mDC, x, y, &pstr[start], i - start);
        }
      }
    }

    if (selectedFont != mCurrFont) {
      // Restore the font
      ::SelectObject(mDC, mCurrFont);
    }

#ifdef FIX_FOR_BUG_6585
    if (pstr != aString && pstr != str) {
      delete[] pstr;
    }
#endif
    return NS_OK;
  }
  else
    return NS_ERROR_FAILURE;
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
  if (!mFontMetrics)
    return NS_ERROR_FAILURE;
  else if (aString && 0 < aLength) {
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
    else {
      // flip sign of descent for cross-platform compatibility
      descent = -(nscoord(gm.gmptGlyphOrigin.y) - nscoord(gm.gmBlackBoxY));
      aBoundingMetrics.leftBearing = gm.gmptGlyphOrigin.x;
      aBoundingMetrics.rightBearing = gm.gmptGlyphOrigin.x + gm.gmBlackBoxX;
      aBoundingMetrics.ascent = gm.gmptGlyphOrigin.y;
      aBoundingMetrics.descent = gm.gmptGlyphOrigin.y - gm.gmBlackBoxY;
      aBoundingMetrics.width = gm.gmCellIncX;
    }
    if (1 < aLength) {
      // loop over each glyph to get the ascent and descent
      PRUint32 i;
      for (i = 1; i < aLength; i++) {
        len = GetGlyphOutline(mDC, aString[i], GGO_METRICS, &gm, 0, nsnull, &mat2);
        if (GDI_ERROR == len) {
          return NS_ERROR_UNEXPECTED;
        }
        else {
          // flip sign of descent for cross-platform compatibility
          descent = -(nscoord(gm.gmptGlyphOrigin.y) - nscoord(gm.gmBlackBoxY));
          if (aBoundingMetrics.ascent < gm.gmptGlyphOrigin.y)
            aBoundingMetrics.ascent = gm.gmptGlyphOrigin.y;
          if (aBoundingMetrics.descent > descent)
            aBoundingMetrics.descent = descent;
        }
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
  }
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextWin::GetBoundingMetrics(const PRUnichar*   aString,
                                          PRUint32           aLength,
                                          nsBoundingMetrics& aBoundingMetrics,
                                          PRInt32*           aFontID)
{
  nsresult rv = NS_OK;

  aBoundingMetrics.Clear();
  if (!mFontMetrics)
    return NS_ERROR_FAILURE;
  else if (aString && 0 < aLength) {
    PRUnichar* pstr = (PRUnichar *)(const PRUnichar *)aString;
#ifdef FIX_FOR_BUG_6585
    // XXX code is OK - remaining issue: 'substituteNonAsciiChars' has to 
    // be initialized based on the rendering mode -- see bug 6585
    PRBool substituteNonAsciiChars = PR_FALSE;
    PRUnichar str[8192];
    if (substituteNonAsciiChars) { 
      pstr = SubstituteNonAsciiChars(aString, aLength, str, 8192, &aLength);
    }
#endif

    nsFontMetricsWin* metrics = (nsFontMetricsWin*) mFontMetrics;
    nsFontWin* prevFont = nsnull;

    SetupFontAndColor();
    HFONT selectedFont = mCurrFont;

    nsBoundingMetrics rawbm;
    PRBool firstTime = PR_TRUE;
    PRUint32 start = 0;
    for (PRUint32 i = 0; i < aLength; i++) {
      PRUnichar c = pstr[i];
      nsFontWin* currFont = nsnull;
      nsFontWin** font = metrics->mLoadedFonts;
      nsFontWin** end = &metrics->mLoadedFonts[metrics->mLoadedFontsCount];
      while (font < end) {
        if (FONT_HAS_GLYPH((*font)->mMap, c)) {
          currFont = *font;
          goto FoundFont; // for speed -- avoid "if" statement
        }
        font++;
      }
      currFont = metrics->FindFont(mDC, c);
FoundFont:
      // XXX avoid this test by duplicating code
      if (prevFont) {
        if (currFont != prevFont) {
          if (prevFont->mFont != selectedFont) {
            ::SelectObject(mDC, prevFont->mFont);
            selectedFont = prevFont->mFont;
          }
          rv = prevFont->GetBoundingMetrics(mDC, &pstr[start], i - start, rawbm);
          if (NS_FAILED(rv)) {
#ifdef NS_DEBUG
            printf("GetBoundingMetrics() failed for: 0x%04X...\n", pstr[start]);
            prevFont->DumpFontInfo();
#endif
            if (selectedFont != mCurrFont) {
              // Restore the font
              ::SelectObject(mDC, mCurrFont);
            }
#ifdef FIX_FOR_BUG_6585
            if (pstr != aString && pstr != str) delete[] pstr;
#endif
            return rv;
          }
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
      if (prevFont->mFont != selectedFont) {
        ::SelectObject(mDC, prevFont->mFont);
        selectedFont = prevFont->mFont;
      }
      rv = prevFont->GetBoundingMetrics(mDC, &pstr[start], i - start, rawbm);
      if (NS_FAILED(rv)) {
#ifdef NS_DEBUG
        printf("GetBoundingMetrics() failed for: 0x%04X...\n", pstr[start]);
        prevFont->DumpFontInfo();
#endif
        if (selectedFont != mCurrFont) {
          // Restore the font
          ::SelectObject(mDC, mCurrFont);
        }
#ifdef FIX_FOR_BUG_6585
        if (pstr != aString && pstr != str) delete[] pstr;
#endif
        return rv;
      }
      if (firstTime)
        aBoundingMetrics = rawbm;
      else
        aBoundingMetrics += rawbm;
    }

    // convert to app units
    aBoundingMetrics.leftBearing = NSToCoordRound(float(aBoundingMetrics.leftBearing) * mP2T);
    aBoundingMetrics.rightBearing = NSToCoordRound(float(aBoundingMetrics.rightBearing) * mP2T);
    aBoundingMetrics.width = NSToCoordRound(float(aBoundingMetrics.width) * mP2T);
    aBoundingMetrics.ascent = NSToCoordRound(float(aBoundingMetrics.ascent) * mP2T);
    aBoundingMetrics.descent = NSToCoordRound(float(aBoundingMetrics.descent) * mP2T);

    if (selectedFont != mCurrFont) {
      // Restore the font
      ::SelectObject(mDC, mCurrFont);
    }
#ifdef FIX_FOR_BUG_6585
    if (pstr != aString && pstr != str) delete[] pstr;
#endif
  }
  if (nsnull != aFontID)
    *aFontID = 0;

  return NS_OK;
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


// The following is a workaround for a Japanese Windows 95 problem.

NS_IMETHODIMP nsRenderingContextWinA :: GetWidth(const PRUnichar *aString,
                                                PRUint32 aLength,
                                                nscoord &aWidth,
                                                PRInt32 *aFontID)
{
  if (nsnull != mFontMetrics)
  {
    nsFontMetricsWinA* metrics = (nsFontMetricsWinA*) mFontMetrics;
    nsFontSubset* prevFont = nsnull;

    SetupFontAndColor();

    LONG width = 0;
    PRUint32 start = 0;
    for (PRUint32 i = 0; i < aLength; i++) {
      PRUnichar c = aString[i];
      nsFontSubset* currFont = nsnull;
      nsFontWinA** font = (nsFontWinA**) metrics->mLoadedFonts;
      nsFontWinA** end = (nsFontWinA**) &metrics->mLoadedFonts[metrics->mLoadedFontsCount];
      while (font < end) {
        if (FONT_HAS_GLYPH((*font)->mMap, c)) {
          nsFontSubset** subset = (*font)->mSubsets;
          nsFontSubset** endSubsets = &((*font)->mSubsets[(*font)->mSubsetsCount]);
          while (subset < endSubsets) {
            if (!(*subset)->mMap) {
              if (!(*subset)->Load(*font)) {
                subset++;
                continue;
              }
            }
            if (FONT_HAS_GLYPH((*subset)->mMap, c)) {
              currFont = *subset;
              goto FoundFont; // for speed -- avoid "if" statement
            }
            subset++;
          }
        }
        font++;
      }
      currFont = (nsFontSubset*) metrics->FindFont(mDC, c);
FoundFont:
      // XXX avoid this test by duplicating code
      if (prevFont) {
        if (currFont != prevFont) {
          ::SelectObject(mDC, prevFont->mFont);
          width += prevFont->GetWidth(mDC, &aString[start], i - start);
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
      ::SelectObject(mDC, prevFont->mFont);
      width += prevFont->GetWidth(mDC, &aString[start], i - start);
    }

    aWidth = NSToCoordRound(float(width) * mP2T);

    ::SelectObject(mDC, mCurrFont);

    if (nsnull != aFontID)
      *aFontID = 0;

    return NS_OK;
  }
  else
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsRenderingContextWinA::GetWidth(const PRUnichar *aString,
                                 PRInt32          aLength,
                                 PRInt32          aAvailWidth,
                                 PRInt32*         aBreaks,
                                 PRInt32          aNumBreaks,
                                 nscoord&         aWidth,
                                 PRInt32&         aNumCharsFit,
                                 PRInt32*         aFontID)
{
  NS_PRECONDITION(aBreaks[aNumBreaks - 1] == aLength, "invalid break array");

  if (nsnull != mFontMetrics) {
    // If we need to back up this state represents the last place we could
    // break. We can use this to avoid remeasuring text
    struct PrevBreakState {
      PRInt32   mBreakIndex;
      nscoord   mWidth;

      PrevBreakState() {
        mBreakIndex = -1;  // not known (hasn't been computed)
        mWidth = 0;
      }
    };

    PRUnichar*          pstr = (PRUnichar *)(const PRUnichar *)aString;
    nsFontMetricsWinA*  metrics = (nsFontMetricsWinA*) mFontMetrics;
    nsFontSubset*       prevFont = nsnull;
    PrevBreakState      prevBreakState;
    
    // Initialize OUT parameter
    aNumCharsFit = 0;

    // Setup the font and foreground color
    SetupFontAndColor();

    // Iterate each character in the string and determine which font to use
    HFONT     selectedFont = mCurrFont;
    nscoord   width = 0;
    PRInt32   start = 0;
    PRInt32   i = 0;
    PRBool    allDone = PR_FALSE;
    while (!allDone) {
      nsFontSubset*      currFont = nsnull;
      nsFontMetricsWinA* fontMetricsWin;
      nscoord            aveCharWidth;

      // First search the fonts we already have loaded
      nsFontWinA** font = (nsFontWinA**) metrics->mLoadedFonts;
      nsFontWinA** end = (nsFontWinA**) &metrics->mLoadedFonts[metrics->mLoadedFontsCount];
      while (font < end) {
        if (FONT_HAS_GLYPH((*font)->mMap, pstr[i])) {
          nsFontSubset** subset = (*font)->mSubsets;
          nsFontSubset** endSubsets = &((*font)->mSubsets[(*font)->mSubsetsCount]);
          while (subset < endSubsets) {
            if (!(*subset)->mMap) {
              if (!(*subset)->Load(*font)) {
                subset++;
                continue;
              }
            }
            if (FONT_HAS_GLYPH((*subset)->mMap, pstr[i])) {
              currFont = *subset;
              goto FoundFont;
            }
            subset++;
          }
        }
        font++;  // try the next font
      }

      // No match in the already loaded fonts. Find a new font
      currFont = (nsFontSubset*) metrics->FindFont(mDC, pstr[i]);

     FoundFont:
      // See if this is the same font we found for the previous character.
      // If so, then keep accumulating characters
      if (prevFont) {
        if (currFont != prevFont) {
          // The font has changed. We need to measure the existing text using
          // the previous font
          goto MeasureText;
        }
      }
      else if(currFont) {
        prevFont = currFont;  // remember this font
      } else {
        NS_WARNING("no previous font and cannot find current font");
        // this should never happen, but if it does we will continue walking the text
        // and hope that a later font is found so we can use that to measure
      }

      // Advance to the next character
      if (++i >= aLength) {
        allDone = PR_TRUE;
        goto MeasureText;  // measure any remaining text
      }
      continue;

     MeasureText:
      // Make sure the font is selected
      NS_ASSERTION(prevFont, "must have a font to measure text!");
      if (prevFont &&
          prevFont->mFont != selectedFont ) {
        NS_ASSERTION(mDC && prevFont->mFont, "null args to SelectObject");
        ::SelectObject(mDC, prevFont->mFont);
        selectedFont = prevFont->mFont;
      }

      // Get the average character width
      fontMetricsWin = (nsFontMetricsWinA*)mFontMetrics;
      fontMetricsWin->GetAveCharWidth(aveCharWidth);
      while (start < i) {
        // Estimate how many characters will fit. Do that by diving the available
        // space by the average character width
        PRInt32 estimatedNumChars = 0;
        if (aveCharWidth > 0) {
          estimatedNumChars = (aAvailWidth - width) / aveCharWidth;
        }
        // Make sure the estimated number of characters is at least 1
        if (estimatedNumChars < 1) {
          estimatedNumChars = 1;
        }

        // Find the nearest break offset
        PRInt32 estimatedBreakOffset = start + estimatedNumChars;
        PRInt32 breakIndex = -1;  // not computed
        PRBool  inMiddleOfSegment = PR_FALSE;
        nscoord numChars;

        // Avoid scanning the break array in the case where we think all
        // the text should fit
        if (i <= estimatedBreakOffset) {
          // Everything should fit
          numChars = i - start;

        } else {
          // Find the nearest place to break that is less than or equal to
          // the estimated break offset
          breakIndex = prevBreakState.mBreakIndex;
          while (aBreaks[breakIndex + 1] <= estimatedBreakOffset) {
            breakIndex++;
          }
          if (breakIndex == -1)
              breakIndex = 0;

          // We found a place to break that is before the estimated break
          // offset. Where we break depends on whether the text crosses a
          // segment boundary
          if (start < aBreaks[breakIndex]) {
            // The text crosses at least one segment boundary so measure to the
            // break point just before the estimated break offset
            numChars = aBreaks[breakIndex] - start;

          } else {
            // See whether there is another segment boundary between this one
            // and the end of the text
            if ((breakIndex < (aNumBreaks - 1)) && (aBreaks[breakIndex] < i)) {
              breakIndex++;
              numChars = aBreaks[breakIndex] - start;

            } else {
              NS_ASSERTION(i != aBreaks[breakIndex], "don't expect to be at segment boundary");

              // The text is all within the same segment
              numChars = i - start;

              // Remember we're in the middle of a segment and not in between
              // two segments
              inMiddleOfSegment = PR_TRUE;
            }
          }
        }

        // Measure the text
        PRInt32 pxWidth;
        nscoord twWidth;
        if ((1 == numChars) && (pstr[start] == ' ')) {
          fontMetricsWin->GetSpaceWidth(twWidth);

        } else {
          if(prevFont)
             pxWidth = prevFont->GetWidth(mDC, &pstr[start], numChars);
          else
             pxWidth =0;
          twWidth = NSToCoordRound(float(pxWidth) * mP2T);
        }

        // See if the text fits
        PRBool  textFits = (twWidth + width) <= aAvailWidth;

        // If the text fits then update the width and the number of
        // characters that fit
        if (textFits) {
          aNumCharsFit += numChars;
          width += twWidth;

          // If we computed the break index and we're not in the middle
          // of a segment then this is a spot that we can back up to if
          // we need to so remember this state
          if ((breakIndex != -1) && !inMiddleOfSegment) {
            prevBreakState.mBreakIndex = breakIndex;
            prevBreakState.mWidth = width;
          }

        } else {
          // The text didn't fit. If we're out of room then we're all done
          allDone = PR_TRUE;
          
          // See if we can just back up to the previous saved state and not
          // have to measure any text
          if (prevBreakState.mBreakIndex != -1) {
            PRBool  canBackup;

            // If we're in the middle of a word then the break index
            // must be the same if we can use it. If we're at a segment
            // boundary, then if the saved state is for the previous
            // break index then we can use it
            if (inMiddleOfSegment) {
              canBackup = prevBreakState.mBreakIndex == breakIndex;
            } else {
              canBackup = prevBreakState.mBreakIndex == (breakIndex - 1);
            }

            if (canBackup) {
              aNumCharsFit = aBreaks[prevBreakState.mBreakIndex];
              width = prevBreakState.mWidth;
              break;
            }
          }
          
          // We can't just revert to the previous break state. Find the break
          // index just before the end of the text
          i = start + numChars;
          if (breakIndex == -1) {
            breakIndex = 0;

            if (aBreaks[breakIndex] < i)
            {
                while (((breakIndex + 1) < aNumBreaks) && (aBreaks[breakIndex + 1] < i)) {
                  breakIndex++;
                }
            }
          }

          if ((0 == breakIndex) && (i <= aBreaks[0])) {
            // There's no place to back up to so even though the text doesn't fit
            // return it anyway
            aNumCharsFit += numChars;
            width += twWidth;
            break;
          }

          // Repeatedly back up until we get to where the text fits or we're
          // all the way back to the first word
          width += twWidth;
          while ((breakIndex >= 0) && (width > aAvailWidth)) {
            start = aBreaks[breakIndex];
            numChars = i - start;
            if ((1 == numChars) && (pstr[start] == ' ')) {
              fontMetricsWin->GetSpaceWidth(twWidth);

            } else {
              if(prevFont)
                 pxWidth = prevFont->GetWidth(mDC, &pstr[start], numChars);
              else
                 pxWidth =0;
              twWidth = NSToCoordRound(float(pxWidth) * mP2T);
            }

            width -= twWidth;
            aNumCharsFit = start;
            breakIndex--;
            i = start;
          }
        }

        start += numChars;
      }

      // We're done measuring that run
      if (!allDone) {
        prevFont = currFont;
        NS_ASSERTION(start == i, "internal error");
  
        // Advance to the next character
        if (++i >= aLength) {
          allDone = PR_TRUE;
          goto MeasureText;
        }
      }
    }

    aWidth = width;

    if (selectedFont != mCurrFont) {
      // Restore the font
      ::SelectObject(mDC, mCurrFont);
    }

    if (nsnull != aFontID) {
      *aFontID = 0;
    }

    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsRenderingContextWinA :: DrawString(const PRUnichar *aString, PRUint32 aLength,
                                                  nscoord aX, nscoord aY,
                                                  PRInt32 aFontID,
                                                  const nscoord* aSpacing)
{
  if (nsnull != mFontMetrics)
  {
    // Take care of the ascent and specifies the drawing on the baseline
    nscoord ascent;
    mFontMetrics->GetMaxAscent(ascent);
    aY += ascent;

    PRInt32 x = aX;
    PRInt32 y = aY;
    mTranMatrix->TransformCoord(&x, &y);
    nsFontMetricsWinA* metrics = (nsFontMetricsWinA*) mFontMetrics;
    nsFontSubset* prevFont = nsnull;

    SetupFontAndColor();

    PRUint32 start = 0;
    for (PRUint32 i = 0; i < aLength; i++) {
      PRUnichar c = aString[i];
      nsFontSubset* currFont = nsnull;
      nsFontWinA** font = (nsFontWinA**) metrics->mLoadedFonts;
      nsFontWinA** end = (nsFontWinA**) &metrics->mLoadedFonts[metrics->mLoadedFontsCount];
      while (font < end) {
        if (FONT_HAS_GLYPH((*font)->mMap, c)) {
          nsFontSubset** subset = (*font)->mSubsets;
          nsFontSubset** endSubsets = &((*font)->mSubsets[(*font)->mSubsetsCount]);
          while (subset < endSubsets) {
            if (!(*subset)->mMap) {
              if (!(*subset)->Load(*font)) {
                subset++;
                continue;
              }
            }
            if (FONT_HAS_GLYPH((*subset)->mMap, c)) {
              currFont = *subset;
              goto FoundFont; // for speed -- avoid "if" statement
            }
            subset++;
          }
        }
        font++;
      }
      currFont = (nsFontSubset*) metrics->FindFont(mDC, c);
FoundFont:
      if (prevFont) {
        if (currFont != prevFont) {
          ::SelectObject(mDC, prevFont->mFont);
          if (aSpacing) {
            // XXX Fix path to use a twips transform in the DC and use the
            // spacing values directly and let windows deal with the sub-pixel
            // positioning.

            // Slow, but accurate rendering
            const PRUnichar* str = &aString[start];
            const PRUnichar* end = &aString[i];
            while (str < end) {
              // XXX can shave some cycles by inlining a version of transform
              // coord where y is constant and transformed once
              x = aX;
              y = aY;
              mTranMatrix->TransformCoord(&x, &y);
              prevFont->DrawString(mDC, x, y, str, 1);
              aX += *aSpacing++;
              str++;
            }
          }
          else {
            prevFont->DrawString(mDC, x, y, &aString[start], i - start);
            x += prevFont->GetWidth(mDC, &aString[start], i - start);
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
      ::SelectObject(mDC, prevFont->mFont);
      if (aSpacing) {
        // XXX Fix path to use a twips transform in the DC and use the
        // spacing values directly and let windows deal with the sub-pixel
        // positioning.

        // Slow, but accurate rendering
        const PRUnichar* str = &aString[start];
        const PRUnichar* end = &aString[i];
        while (str < end) {
          // XXX can shave some cycles by inlining a version of transform
          // coord where y is constant and transformed once
          x = aX;
          y = aY;
          mTranMatrix->TransformCoord(&x, &y);
          prevFont->DrawString(mDC, x, y, str, 1);
          aX += *aSpacing++;
          str++;
        }
      }
      else {
        prevFont->DrawString(mDC, x, y, &aString[start], i - start);
      }
    }

    ::SelectObject(mDC, mCurrFont);

    return NS_OK;
  }
  else
    return NS_ERROR_FAILURE;
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
 * by setting the fuOptions parameter to ETO_RTLREADING in calls to
 * ExtTextOut()
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



