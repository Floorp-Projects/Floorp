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

#ifdef NGLAYOUT_DDRAW
#define INITGUID
#endif

#include "nsRenderingContextPS.h"
#include <math.h>
#include "libimg.h"
#include "nsDeviceContextPS.h"
#include "nsIScriptGlobalObject.h"
#include "prprf.h"
#include "nsPSUtil.h"
#include "nsPrintManager.h"
#include "structs.h" //XXX:PS This should be removed
#include "xlate.h"
#include "xlate_i.h"


static NS_DEFINE_IID(kIRenderingContextIID, NS_IRENDERING_CONTEXT_IID);

#define FLAG_CLIP_VALID       0x0001
#define FLAG_CLIP_CHANGED     0x0002
#define FLAG_LOCAL_CLIP_VALID 0x0004

#define FLAGS_ALL             (FLAG_CLIP_VALID | FLAG_CLIP_CHANGED | FLAG_LOCAL_CLIP_VALID)

// Macro for creating a palette relative color if you have a COLORREF instead
// of the reg, green, and blue values. The color is color-matches to the nearest
// in the current logical palette. This has no effect on a non-palette device
#define PALETTERGB_COLORREF(c)  (0x02000000 | (c))

// Macro to convert from TWIPS (1440 per inch) to POINTS (72 per inch)
#define NS_TWIPS_TO_POINTS(x) ((x / 20))
#define NS_PIXELS_TO_POINTS(x) (x * 10)
#define NS_PS_RED(x) (((float)(NS_GET_R(x))) / 255.0) 
#define NS_PS_GREEN(x) (((float)(NS_GET_G(x))) / 255.0) 
#define NS_PS_BLUE(x) (((float)(NS_GET_B(x))) / 255.0) 
#define NS_IS_BOLD(x) (((x) >= 500) ? 1 : 0) 


/*
 * This is actually real fun.  Windows does not draw dotted lines with Pen's
 * directly (Go ahead, try it, you'll get dashes).
 *
 * the trick is to install a callback and actually put the pixels in
 * directly. This function will get called for each pixel in the line.
 *
 */


class GraphicState
{
public:
  GraphicState();
  GraphicState(GraphicState &aState);
  ~GraphicState();

  GraphicState   *mNext;
  nsTransform2D   mMatrix;
  nsRect          mLocalClip;
  HRGN            mClipRegion;
  nscolor         mBrushColor;
  HBRUSH          mSolidBrush;
  nsIFontMetrics  *mFontMetrics;
  HFONT           mFont;
  nscolor         mPenColor;
  HPEN            mSolidPen;
  HPEN            mDashedPen;
  HPEN            mDottedPen;
  PRInt32         mFlags;
  nscolor         mTextColor;
  nsLineStyle     mLineStyle;
};

GraphicState :: GraphicState()
{
  mNext = nsnull;
  mMatrix.SetToIdentity(); 
  mLocalClip.x = mLocalClip.y = mLocalClip.width = mLocalClip.height = 0;
  mClipRegion = NULL;
  mBrushColor = NS_RGB(0, 0, 0);
  mSolidBrush = NULL;
  mFontMetrics = nsnull;
  mFont = NULL;
  mPenColor = NS_RGB(0, 0, 0);
  mSolidPen = NULL;
  mDashedPen = NULL;
  mDottedPen = NULL;
  mFlags = ~FLAGS_ALL;
  mTextColor = RGB(0, 0, 0);
  mLineStyle = nsLineStyle_kSolid;
}

GraphicState :: GraphicState(GraphicState &aState) :
                               mMatrix(&aState.mMatrix),
                               mLocalClip(aState.mLocalClip)
{
  mNext = &aState;
  mClipRegion = NULL;
  mBrushColor = aState.mBrushColor;
  mSolidBrush = NULL;
  mFontMetrics = nsnull;
  mFont = NULL;
  mPenColor = aState.mPenColor;
  mSolidPen = NULL;
  mDashedPen = NULL;
  mDottedPen = NULL;
  mFlags = ~FLAGS_ALL;
  mTextColor = aState.mTextColor;
  mLineStyle = aState.mLineStyle;
}

GraphicState :: ~GraphicState()
{
  if (NULL != mClipRegion)
  {
    ::DeleteObject(mClipRegion);
    mClipRegion = NULL;
  }

  if (NULL != mSolidBrush)
  {
    ::DeleteObject(mSolidBrush);
    mSolidBrush = NULL;
  }

  //don't delete this because it lives in the font metrics
  mFont = NULL;

  if (NULL != mSolidPen)
  {
    ::DeleteObject(mSolidPen);
    mSolidPen = NULL;
  }

  if (NULL != mDashedPen)
  {
    ::DeleteObject(mDashedPen);
    mDashedPen = NULL;
  }

  if (NULL != mDottedPen)
  {
    ::DeleteObject(mDottedPen);
    mDottedPen = NULL;
  }
}


static NS_DEFINE_IID(kRenderingContextIID, NS_IRENDERING_CONTEXT_IID);

#ifdef NGLAYOUT_DDRAW
IDirectDraw *nsRenderingContextPS::mDDraw = NULL;
IDirectDraw2 *nsRenderingContextPS::mDDraw2 = NULL;
nsresult nsRenderingContextPS::mDDrawResult = NS_OK;
#endif

#define NOT_SETUP 0x33
static PRBool gIsWIN95 = NOT_SETUP;

void nsRenderingContextPS :: PostscriptColor(nscolor aColor)
{
  XP_FilePrintf(mPrintContext->prSetup->out,"%3.2f %3.2f %3.2f setrgbcolor\n", NS_PS_RED(aColor), NS_PS_GREEN(aColor),
		  NS_PS_BLUE(aColor));
}

void nsRenderingContextPS :: PostscriptFillRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
  xl_moveto(mPrintContext, aX, aY);
  xl_box(mPrintContext, aWidth, aHeight);
}

void nsRenderingContextPS :: PostscriptDrawBitmap(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight, IL_Pixmap *aImage, IL_Pixmap *aMask)
{
  //xl_colorimage(mPrintContext, aX, aY, aWidth, aHeight, IL_Pixmap *image,IL_Pixmap *mask);
}

void nsRenderingContextPS :: PostscriptFont(nscoord aHeight, PRUint8 aStyle, 
											 PRUint8 aVariant, PRUint16 aWeight, PRUint8 decorations)
{
	XP_FilePrintf(mPrintContext->prSetup->out,"%d",NS_TWIPS_TO_POINTS(aHeight));
	//XXX:PS Add bold, italic and other settings here


	int postscriptFont = 0;

	switch(aStyle)
	{
	  case NS_FONT_STYLE_NORMAL :
		if (NS_IS_BOLD(aWeight)) {
			// NORMAL BOLD
		  postscriptFont = 1;
		}
		else {
		    // NORMAL
	      postscriptFont = 0;
		}
	  break;

	  case NS_FONT_STYLE_ITALIC:
		if (NS_IS_BOLD(aWeight)) {
			// BOLD ITALIC
		  postscriptFont = 3;
		}
		else {
			// ITALIC
		  postscriptFont = 2;
		}
	  break;

	  case NS_FONT_STYLE_OBLIQUE:
		if (NS_IS_BOLD(aWeight)) {
			// COURIER-BOLD OBLIQUE
	      postscriptFont = 7;
		}
		else {
			// COURIER OBLIQUE
	      postscriptFont = 6;
		}
	  break;
	}

	 XP_FilePrintf(mPrintContext->prSetup->out, " f%d\n", postscriptFont);

#if 0
     // The style of font (normal, italic, oblique)
  PRUint8 style;

  // The variant of the font (normal, small-caps)
  PRUint8 variant;

  // The weight of the font (0-999)
  PRUint16 weight;

  // The decorations on the font (underline, overline,
  // line-through). The decorations can be binary or'd together.
  PRUint8 decorations;

  // The size of the font, in nscoord units
  nscoord size; 
#endif

}

void nsRenderingContextPS :: PostscriptTextOut(const char *aString, PRUint32 aLength,
                                    nscoord aX, nscoord aY, nscoord aWidth,
                                    const nscoord* aSpacing, PRBool aIsUnicode)
{
  nscoord fontHeight = 0;
  mFontMetrics->GetHeight(fontHeight);
  nsFont *font;
  mFontMetrics->GetFont(font);

   //XXX: NGLAYOUT expects font to be positioned based on center.
   // fontHeight / 2 is a crude approximation of this. TODO: use the correct
   // postscript command to offset from center of the text.
  xl_moveto(mPrintContext, aX, aY + (fontHeight / 2));
  if (PR_TRUE == aIsUnicode) {
    //XXX: Investigate how to really do unicode with Postscript
	// Just remove the extra byte per character and draw that instead
    char *buf = new char[aLength];
	int ptr = 0;
	unsigned int i;
	for (i = 0; i < aLength; i++) {
      buf[i] = aString[ptr];
	  ptr+=2;
	}
	xl_show(mPrintContext, buf, aLength, "");
	delete buf;
  }
  else
    xl_show(mPrintContext, (char *)aString, aLength, "");	
}

nsRenderingContextPS :: nsRenderingContextPS()
{
  NS_INIT_REFCNT();
  
  mPrintContext = nsnull;

  // The first time in we initialize gIsWIN95 flag & gFastDDASupport flag
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
    }
  }

  mFontMetrics = nsnull;
  mOrigFont = NULL;
  mDefFont = NULL;
  mBlackPen = NULL;
  mCurrBrushColor = NULL;
  mCurrFontMetrics = nsnull;
  mCurrPenColor = NULL;
  mNullPen = NULL;
  mCurrTextColor = RGB(0, 0, 0);
  mCurrLineStyle = nsLineStyle_kSolid;

  mStateCache = new nsVoidArray();

  //create an initial GraphicState
  PushState();

  mP2T = 1.0f;
}

nsRenderingContextPS :: ~nsRenderingContextPS()
{
	//XXX:PS Temporary for postscript output
  //TestFinalize();

  NS_IF_RELEASE(mContext);
  NS_IF_RELEASE(mFontMetrics);

  //destroy the initial GraphicState

  PRBool clipState;
  PopState(clipState);

  //cleanup the DC so that we can just destroy objects
  //in the graphics state without worrying that we are
  //ruining the dc

#ifdef DC
  if (NULL != mDC){
    if (NULL != mOrigSolidBrush){
      ::SelectObject(mDC, mOrigSolidBrush);
      mOrigSolidBrush = NULL;
    }

    if (NULL != mOrigFont){
      ::SelectObject(mDC, mOrigFont);
      mOrigFont = NULL;
    }

    if (NULL != mDefFont){
      ::DeleteObject(mDefFont);
      mDefFont = NULL;
    }

    if (NULL != mOrigSolidPen){
      ::SelectObject(mDC, mOrigSolidPen);
      mOrigSolidPen = NULL;
    }

    if (NULL != mCurrBrush)
      ::DeleteObject(mCurrBrush);

    if ((NULL != mBlackBrush) && (mBlackBrush != mCurrBrush))
      ::DeleteObject(mBlackBrush);

    mCurrBrush = NULL;
    mBlackBrush = NULL;

    //don't kill the font because the font cache/metrics owns it
    mCurrFont = NULL;

    if (NULL != mCurrPen)
      ::DeleteObject(mCurrPen);

    if ((NULL != mBlackPen) && (mBlackPen != mCurrPen))
      ::DeleteObject(mBlackPen);
    if ((NULL != mNullPen) && (mNullPen != mCurrPen))

      ::DeleteObject(mNullPen);

    mCurrPen = NULL;
    mBlackPen = NULL;
    mNullPen = NULL;
  }
#endif

  if (nsnull != mStateCache){
    PRInt32 cnt = mStateCache->Count();

    while (--cnt >= 0){
      GraphicState *state = (GraphicState *)mStateCache->ElementAt(cnt);
      mStateCache->RemoveElementAt(cnt);

      if (nsnull != state)
        delete state;
    }

    delete mStateCache;
    mStateCache = nsnull;
  }

#ifdef DC
 if (nsnull != mSurface){
    NS_RELEASE(mSurface);
  }

  NS_IF_RELEASE(mMainSurface);
#endif

  //NS_IF_RELEASE(mDCOwner);

  mTMatrix = nsnull;
  //mDC = NULL;
  //mMainDC = NULL;

}

nsresult
nsRenderingContextPS :: QueryInterface(REFNSIID aIID, void** aInstancePtr)
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

NS_IMPL_ADDREF(nsRenderingContextPS)
NS_IMPL_RELEASE(nsRenderingContextPS)

NS_IMETHODIMP
nsRenderingContextPS :: Init(nsIDeviceContext* aContext,
                              nsIWidget *aWindow)
{
  //NS_PRECONDITION(PR_FALSE == mInitialized, "double init");

  mContext = aContext;
  mPrintContext = ((nsDeviceContextPS*)mContext)->GetPrintContext();
  NS_IF_ADDREF(mContext);

  mSurface = (nsDrawingSurfacePS *)new nsDrawingSurfacePS();

  if (nsnull != mSurface)
  {
    //NS_ADDREF(mSurface);
    //mSurface->Init(aWindow);
    //mDC = mSurface->mDC;

    //mMainDC = mDC;
    //mMainSurface = mSurface;
    //NS_ADDREF(mMainSurface);
  }

  //mDCOwner = aWindow;

  //NS_IF_ADDREF(mDCOwner);

  return CommonInit();
}

NS_IMETHODIMP
nsRenderingContextPS :: Init(nsIDeviceContext* aContext,
                              nsDrawingSurface aSurface)
{
  //NS_PRECONDITION(PR_FALSE == mInitialized, "double init");

  mContext = aContext;
  NS_IF_ADDREF(mContext);

  mSurface = (nsDrawingSurfacePS *)aSurface;

  //if (nsnull != mSurface)
  //{
    //NS_ADDREF(mSurface);
    //mDC = mSurface->mDC;

    //mMainDC = mDC;
    //mMainSurface = mSurface;
    //NS_ADDREF(mMainSurface);
  //}

  //mDCOwner = nsnull;

  return CommonInit();
}

#ifdef NOTNOW
nsresult nsRenderingContextPS :: SetupDC(HDC aOldDC, HDC aNewDC)
{
  ::SetTextColor(aNewDC, RGB(0, 0, 0));
  ::SetBkMode(aNewDC, TRANSPARENT);
  ::SetPolyFillMode(aNewDC, WINDING);
  ::SetStretchBltMode(aNewDC, COLORONCOLOR);

  if (nsnull != aOldDC)
  {
    if (nsnull != mOrigSolidBrush)
      ::SelectObject(aOldDC, mOrigSolidBrush);

    if (nsnull != mOrigFont)
      ::SelectObject(aOldDC, mOrigFont);

    if (nsnull != mOrigSolidPen)
      ::SelectObject(aOldDC, mOrigSolidPen);

    if (nsnull != mOrigPalette)
      ::SelectPalette(aOldDC, mOrigPalette, TRUE);
  }

  mOrigSolidBrush = (HBRUSH)::SelectObject(aNewDC, mBlackBrush);
  mOrigFont = (HFONT)::SelectObject(aNewDC, mDefFont);
  mOrigSolidPen = (HPEN)::SelectObject(aNewDC, mBlackPen);

  // If this is a palette device, then select and realize the palette
  nsPaletteInfo palInfo;
  mContext->GetPaletteInfo(palInfo);

  if (palInfo.isPaletteDevice && palInfo.palette)
  {
    // Select the palette in the background
    mOrigPalette = ::SelectPalette(aNewDC, (HPALETTE)palInfo.palette, TRUE);
    // Don't do the realization for an off-screen memory DC
    if (nsnull == aOldDC)
      ::RealizePalette(aNewDC);
  }

  if (GetDeviceCaps(aNewDC, RASTERCAPS) & (RC_BITBLT))
    gFastDDASupport = PR_TRUE;

  return NS_OK;
}
#endif

nsresult nsRenderingContextPS :: CommonInit(void)
{
  float app2dev;
  mContext->GetAppUnitsToDevUnits(app2dev);
	mTMatrix->AddScale(app2dev, app2dev);
  mContext->GetDevUnitsToAppUnits(mP2T);

#ifdef NS_DEBUG
  //mInitialized = PR_TRUE;
#endif

  mBlackBrush = (HBRUSH)::GetStockObject(BLACK_BRUSH);
  mDefFont = ::CreateFont(12, 0, 0, 0, FW_DONTCARE, FALSE, FALSE, FALSE,
                          ANSI_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                          DEFAULT_QUALITY, FF_ROMAN | VARIABLE_PITCH, "Times New Roman");
  mBlackPen = ::CreatePen(PS_SOLID, 0, RGB(0, 0, 0));

  mContext->GetGammaTable(mGammaTable);

  //return SetupDC(nsnull, mDC);
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextPS :: SelectOffScreenDrawingSurface(nsDrawingSurface aSurface)
{
  nsresult  rv;
#ifdef DC
  //XXX this should reset the data in the state stack.

  if (nsnull != aSurface){

    rv = SetupDC(mDC, ((nsDrawingSurfacePS *)aSurface)->mDC);
    NS_IF_RELEASE(mSurface);
    mSurface = (nsDrawingSurfacePS *)aSurface;
  }
  else
  {
    rv = SetupDC(mDC, mMainDC);
    NS_IF_RELEASE(mSurface);
    mSurface = mMainSurface;
  }

  NS_ADDREF(mSurface);
  mDC = mSurface->mDC;
#endif

  return rv;
}

NS_IMETHODIMP
nsRenderingContextPS :: GetHints(PRUint32& aResult)
{
  PRUint32 result = 0;

  if (gIsWIN95)
    result |= NS_RENDERING_HINT_FAST_8BIT_TEXT;

  aResult = result;

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPS :: Reset()
{
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPS :: GetDeviceContext(nsIDeviceContext *&aContext)
{
  NS_IF_ADDREF(mContext);
  aContext = mContext;
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPS :: PushState(void)
{
  PRInt32 cnt = mStateCache->Count();

  if (cnt == 0)
  {
    if (nsnull == mStates)
      mStates = new GraphicState();
    else
      mStates = new GraphicState(*mStates);
  }
  else
  {
    GraphicState *state = (GraphicState *)mStateCache->ElementAt(cnt - 1);
    mStateCache->RemoveElementAt(cnt - 1);

    state->mNext = mStates;

    //clone state info

    state->mMatrix = mStates->mMatrix;

    state->mLocalClip = mStates->mLocalClip;
// we don't want to NULL this out since we reuse the region
// from state to state. if we NULL it, we need to also delete it,
// which means we'll just re-create it when we push the clip state. MMP
//    state->mClipRegion = NULL;
    state->mBrushColor = mStates->mBrushColor;
    state->mSolidBrush = NULL;
    state->mFontMetrics = mStates->mFontMetrics;
    state->mFont = NULL;
    state->mPenColor = mStates->mPenColor;
    state->mSolidPen = NULL;
    state->mDashedPen = NULL;
    state->mDottedPen = NULL;
    state->mFlags = ~FLAGS_ALL;
    state->mTextColor = mStates->mTextColor;
    state->mLineStyle = mStates->mLineStyle;

    mStates = state;
  }

  mTMatrix = &mStates->mMatrix;

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPS :: PopState(PRBool &aClipEmpty)
{
  PRBool  retval = PR_FALSE;

  if (nsnull == mStates)
  {
    NS_ASSERTION(!(nsnull == mStates), "state underflow");
  }
  else
  {
    GraphicState *oldstate = mStates;

    mStates = mStates->mNext;

    mStateCache->AppendElement(oldstate);

    if (nsnull != mStates)
    {
      mTMatrix = &mStates->mMatrix;

      GraphicState *pstate;

      if (oldstate->mFlags & FLAG_CLIP_CHANGED)
      {
        pstate = mStates;

        //the clip rect has changed from state to state, so
        //install the previous clip rect

        while ((nsnull != pstate) && !(pstate->mFlags & FLAG_CLIP_VALID))
          pstate = pstate->mNext;

        if (nsnull != pstate)
        {
          //int cliptype = ::SelectClipRgn(mDC, pstate->mClipRegion);

          //if (cliptype == NULLREGION)
            //retval = PR_TRUE;
        }
      }

      oldstate->mFlags &= ~FLAGS_ALL;
      oldstate->mSolidBrush = NULL;
      oldstate->mFont = NULL;
      oldstate->mSolidPen = NULL;
      oldstate->mDashedPen = NULL;
      oldstate->mDottedPen = NULL;

      SetLineStyle(mStates->mLineStyle);
    }
    else
      mTMatrix = nsnull;
  }

  aClipEmpty = retval;

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPS :: IsVisibleRect(const nsRect& aRect, PRBool &aVisible)
{
  aVisible = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPS :: SetClipRect(const nsRect& aRect, nsClipCombine aCombine, PRBool &aClipEmpty)
{
  nsRect  trect = aRect;
  int     cliptype;

  mStates->mLocalClip = aRect;

	mTMatrix->TransformCoord(&trect.x, &trect.y,
                           &trect.width, &trect.height);

  mStates->mFlags |= FLAG_LOCAL_CLIP_VALID;

  //how we combine the new rect with the previous?

  if (aCombine == nsClipCombine_kIntersect)
  {
    PushClipState();

    //cliptype = ::IntersectClipRect(mDC, trect.x,trect.y,trect.XMost(),trect.YMost());
  }
  else if (aCombine == nsClipCombine_kUnion)
  {
    PushClipState();

    HRGN  tregion = ::CreateRectRgn(trect.x,
                                    trect.y,
                                    trect.XMost(),
                                    trect.YMost());

    //cliptype = ::ExtSelectClipRgn(mDC, tregion, RGN_OR);
    ::DeleteObject(tregion);
  }
  else if (aCombine == nsClipCombine_kSubtract)
  {
    PushClipState();

    //cliptype = ::ExcludeClipRect(mDC, trect.x,trect.y,trect.XMost(),trect.YMost());
  }
  else if (aCombine == nsClipCombine_kReplace)
  {
    PushClipState();

    HRGN  tregion = ::CreateRectRgn(trect.x,
                                    trect.y,
                                    trect.XMost(),
                                    trect.YMost());
    //cliptype = ::SelectClipRgn(mDC, tregion);
    ::DeleteObject(tregion);
  }
  else
    NS_ASSERTION(FALSE, "illegal clip combination");

  if (cliptype == NULLREGION)
    aClipEmpty = PR_TRUE;
  else
    aClipEmpty = PR_FALSE;

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPS :: GetClipRect(nsRect &aRect, PRBool &aClipValid)
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

NS_IMETHODIMP nsRenderingContextPS :: SetClipRegion(const nsIRegion& aRegion, nsClipCombine aCombine, PRBool &aClipEmpty)
{
#ifdef NOTNOW
  nsRegionPS *pRegion = (nsRegionPS *)&aRegion;
  HRGN        hrgn = pRegion->GetHRGN();
  int         cmode, cliptype;

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
    //cliptype = ::ExtSelectClipRgn(mDC, hrgn, cmode);
  }
  else
    return PR_FALSE;

  if (cliptype == NULLREGION)
    aClipEmpty = PR_TRUE;
  else
    aClipEmpty = PR_FALSE;
#endif

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPS :: GetClipRegion(nsIRegion **aRegion)
{
  //XXX wow, needs to do something.
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPS :: SetColor(nscolor aColor)
{
  mCurrentColor = aColor;
  mColor = RGB(mGammaTable[NS_GET_R(aColor)],
               mGammaTable[NS_GET_G(aColor)],
               mGammaTable[NS_GET_B(aColor)]);
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPS :: GetColor(nscolor &aColor) const
{
  aColor = mCurrentColor;
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPS :: SetLineStyle(nsLineStyle aLineStyle)
{
  mCurrLineStyle = aLineStyle;
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPS :: GetLineStyle(nsLineStyle &aLineStyle)
{
  aLineStyle = mCurrLineStyle;
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPS :: SetFont(const nsFont& aFont)
{
  NS_IF_RELEASE(mFontMetrics);
  mContext->GetMetricsFor(aFont, mFontMetrics);
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPS :: SetFont(nsIFontMetrics *aFontMetrics)
{
  NS_IF_RELEASE(mFontMetrics);
  mFontMetrics = aFontMetrics;
  NS_IF_ADDREF(mFontMetrics);

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPS :: GetFontMetrics(nsIFontMetrics *&aFontMetrics)
{
  NS_IF_ADDREF(mFontMetrics);
  aFontMetrics = mFontMetrics;
  return NS_OK;
}

// add the passed in translation to the current translation
NS_IMETHODIMP nsRenderingContextPS :: Translate(nscoord aX, nscoord aY)
{
	mTMatrix->AddTranslation((float)aX,(float)aY);
  return NS_OK;
}

// add the passed in scale to the current scale
NS_IMETHODIMP nsRenderingContextPS :: Scale(float aSx, float aSy)
{
	mTMatrix->AddScale(aSx, aSy);
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPS :: GetCurrentTransform(nsTransform2D *&aTransform)
{
  aTransform = mTMatrix;
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPS :: CreateDrawingSurface(nsRect *aBounds, PRUint32 aSurfFlags, nsDrawingSurface &aSurface)
{
  nsDrawingSurfacePS *surf = new nsDrawingSurfacePS();
  aSurface = (nsDrawingSurface)surf;

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPS :: DestroyDrawingSurface(nsDrawingSurface aDS)
{
#ifdef DC
  nsDrawingSurfacePS *surf = (nsDrawingSurfacePS *)aDS;

  if (surf->mDC == mDC)
  {
    mDC = mMainDC;
    mSurface = mMainSurface;
    NS_IF_ADDREF(mSurface);
  }

  NS_IF_RELEASE(surf);
#endif

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPS :: DrawLine(nscoord aX0, nscoord aY0, nscoord aX1, nscoord aY1)
{
  if (nsLineStyle_kNone == mCurrLineStyle)
    return NS_OK;

	mTMatrix->TransformCoord(&aX0,&aY0);
	mTMatrix->TransformCoord(&aX1,&aY1);

  //SetupPen();

  // support dashed lines here
  //::MoveToEx(mDC, (int)(aX0), (int)(aY0), NULL);
  //::LineTo(mDC, (int)(aX1), (int)(aY1));

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPS :: DrawPolyline(const nsPoint aPoints[], PRInt32 aNumPoints)
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
		mTMatrix->TransformCoord((int*)&pp->x,(int*)&pp->y);
	}

  // Draw the polyline
  //SetupPen();
  //::Polyline(mDC, pp0, int(aNumPoints));

  // Release temporary storage if necessary
  if (pp0 != pts)
    delete pp0;

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPS :: DrawRect(const nsRect& aRect)
{
  RECT nr;
	nsRect	tr;

	tr = aRect;
	mTMatrix->TransformCoord(&tr.x,&tr.y,&tr.width,&tr.height);
	nr.left = tr.x;
	nr.top = tr.y;
	nr.right = tr.x+tr.width;
	nr.bottom = tr.y+tr.height;

  //::FrameRect(mDC, &nr, SetupSolidBrush());

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPS :: DrawRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
  RECT nr;

	mTMatrix->TransformCoord(&aX,&aY,&aWidth,&aHeight);
	nr.left = aX;
	nr.top = aY;
	nr.right = aX+aWidth;
	nr.bottom = aY+aHeight;

  //::FrameRect(mDC, &nr, SetupSolidBrush());

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPS :: FillRect(const nsRect& aRect)
{
  RECT nr;
	nsRect	tr;

	tr = aRect;
	mTMatrix->TransformCoord(&tr.x,&tr.y,&tr.width,&tr.height);
	nr.left = tr.x;
	nr.top = tr.y;
	nr.right = tr.x+tr.width;
	nr.bottom = tr.y+tr.height;

	PostscriptFillRect(NS_PIXELS_TO_POINTS(tr.x), NS_PIXELS_TO_POINTS(tr.y), 
		 NS_PIXELS_TO_POINTS(tr.width), NS_PIXELS_TO_POINTS(tr.height));
	//XXX: Remove fillrect call bellow when working

  //::FillRect(mDC, &nr, SetupSolidBrush());

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPS :: FillRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
  RECT nr;
	nsRect	tr;

	mTMatrix->TransformCoord(&aX,&aY,&aWidth,&aHeight);
	nr.left = aX;
	nr.top = aY;
	nr.right = aX+aWidth;
	nr.bottom = aY+aHeight;

  //::FillRect(mDC, &nr, SetupSolidBrush());

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPS :: DrawPolygon(const nsPoint aPoints[], PRInt32 aNumPoints)
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
		mTMatrix->TransformCoord((int*)&pp->x,(int*)&pp->y);
	}

  // Outline the polygon - note we are implicitly ignoring the linestyle here
  LOGBRUSH lb;
  lb.lbStyle = BS_NULL;
  lb.lbColor = 0;
  lb.lbHatch = 0;
  //SetupSolidPen();
  //HBRUSH brush = ::CreateBrushIndirect(&lb);
  //HBRUSH oldBrush = (HBRUSH)::SelectObject(mDC, brush);
  //::Polygon(mDC, pp0, int(aNumPoints));
  //::SelectObject(mDC, oldBrush);
  //::DeleteObject(brush);

  // Release temporary storage if necessary
  if (pp0 != pts)
    delete pp0;

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPS :: FillPolygon(const nsPoint aPoints[], PRInt32 aNumPoints)
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
		mTMatrix->TransformCoord((int*)&pp->x,(int*)&pp->y);
	}

  // Fill the polygon
  //SetupSolidBrush();

  //if (NULL == mNullPen)
    //mNullPen = ::CreatePen(PS_NULL, 0, 0);

  //HPEN oldPen = (HPEN)::SelectObject(mDC, mNullPen);
  //::Polygon(mDC, pp0, int(aNumPoints));
  //::SelectObject(mDC, oldPen);

  // Release temporary storage if necessary
  if (pp0 != pts)
    delete pp0;

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPS :: DrawEllipse(const nsRect& aRect)
{
  return DrawEllipse(aRect.x, aRect.y, aRect.width, aRect.height);
}

NS_IMETHODIMP nsRenderingContextPS :: DrawEllipse(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
  if (nsLineStyle_kNone == mCurrLineStyle)
    return NS_OK;

  mTMatrix->TransformCoord(&aX, &aY, &aWidth, &aHeight);

  //SetupPen();

  //HBRUSH oldBrush = (HBRUSH)::SelectObject(mDC, ::GetStockObject(NULL_BRUSH));
  
  //::Ellipse(mDC, aX, aY, aX + aWidth, aY + aHeight);
  //::SelectObject(mDC, oldBrush);

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPS :: FillEllipse(const nsRect& aRect)
{
  return FillEllipse(aRect.x, aRect.y, aRect.width, aRect.height);
}

NS_IMETHODIMP nsRenderingContextPS :: FillEllipse(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
  mTMatrix->TransformCoord(&aX, &aY, &aWidth, &aHeight);

  //SetupSolidPen();
  //SetupSolidBrush();
  
  //:Ellipse(mDC, aX, aY, aX + aWidth, aY + aHeight);

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPS :: DrawArc(const nsRect& aRect,
                                 float aStartAngle, float aEndAngle)
{
  return DrawArc(aRect.x,aRect.y,aRect.width,aRect.height,aStartAngle,aEndAngle);
}

NS_IMETHODIMP nsRenderingContextPS :: DrawArc(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight,
                                 float aStartAngle, float aEndAngle)
{
  if (nsLineStyle_kNone == mCurrLineStyle)
    return NS_OK;

  PRInt32 quad1, quad2, sx, sy, ex, ey, cx, cy;
  float   anglerad, distance;

  mTMatrix->TransformCoord(&aX, &aY, &aWidth, &aHeight);

  //SetupPen();
  //SetupSolidBrush();

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
  //::SetArcDirection(mDC, AD_COUNTERCLOCKWISE);

  //::Arc(mDC, aX, aY, aX + aWidth, aY + aHeight, sx, sy, ex, ey); 

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPS :: FillArc(const nsRect& aRect,
                                 float aStartAngle, float aEndAngle)
{
  return FillArc(aRect.x, aRect.y, aRect.width, aRect.height, aStartAngle, aEndAngle);
}

NS_IMETHODIMP nsRenderingContextPS :: FillArc(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight,
                                 float aStartAngle, float aEndAngle)
{
  PRInt32 quad1, quad2, sx, sy, ex, ey, cx, cy;
  float   anglerad, distance;

  mTMatrix->TransformCoord(&aX, &aY, &aWidth, &aHeight);

  //SetupSolidPen();
  //SetupSolidBrush();

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
  //::SetArcDirection(mDC, AD_COUNTERCLOCKWISE);

  //::Pie(mDC, aX, aY, aX + aWidth, aY + aHeight, sx, sy, ex, ey); 

  return NS_OK;
}


NS_IMETHODIMP nsRenderingContextPS :: GetWidth(char ch, nscoord& aWidth)
{
  char buf[1];
  buf[0] = ch;
  return GetWidth(buf, 1, aWidth);
}

NS_IMETHODIMP nsRenderingContextPS :: GetWidth(PRUnichar ch, nscoord &aWidth)
{
  PRUnichar buf[1];
  buf[0] = ch;
  return GetWidth(buf, 1, aWidth);
}

NS_IMETHODIMP nsRenderingContextPS :: GetWidth(const char* aString, nscoord& aWidth)
{
  return GetWidth(aString, strlen(aString), aWidth);
}

NS_IMETHODIMP nsRenderingContextPS :: GetWidth(const char* aString,
                                  PRUint32 aLength,
                                  nscoord& aWidth)
{
  if (nsnull != mFontMetrics)
  {
    SIZE  size;

    SetupFontAndColor();
    //::GetTextExtentPoint32(mDC, aString, aLength, &size);
    aWidth = NSToCoordRound(float(size.cx) * mP2T);

    return NS_OK;
  }
  else
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsRenderingContextPS :: GetWidth(const nsString& aString, nscoord& aWidth)
{
  return GetWidth(aString.GetUnicode(), aString.Length(), aWidth);
}

NS_IMETHODIMP nsRenderingContextPS :: GetWidth(const PRUnichar *aString,
                                  PRUint32 aLength,
                                  nscoord &aWidth)
{
  if (nsnull != mFontMetrics)
  {
    SIZE  size;

    SetupFontAndColor();
    //::GetTextExtentPoint32W(mDC, aString, aLength, &size);
    aWidth = NSToCoordRound(float(size.cx) * mP2T);

    return NS_OK;
  }
  else
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsRenderingContextPS :: DrawString(const char *aString, PRUint32 aLength,
                        nscoord aX, nscoord aY,
                        nscoord aWidth,
                        const nscoord* aSpacing)
{
	PRInt32	x = aX;
  PRInt32 y = aY;

  SetupFontAndColor();

  INT dxMem[500];
  INT* dx0;
  if (nsnull != aSpacing) {
    dx0 = dxMem;
    if (aLength > 500) {
      dx0 = new INT[aLength];
    }
    mTMatrix->ScaleXCoords(aSpacing, aLength, dx0);
  }

	mTMatrix->TransformCoord(&x, &y);
  //::ExtTextOut(mDC, x, y, 0, NULL, aString, aLength, aSpacing ? dx0 : NULL);
   //XXX: Remove ::ExtTextOut later
  PostscriptTextOut(aString, aLength, NS_PIXELS_TO_POINTS(x), NS_PIXELS_TO_POINTS(y), aLength, aSpacing ? dx0 : NULL, FALSE);

  if ((nsnull != aSpacing) && (dx0 != dxMem)) {
    delete [] dx0;
  }

  if (nsnull != mFontMetrics)
  {
    nsFont *font;
    mFontMetrics->GetFont(font);
    PRUint8 decorations = font->decorations;

    if (decorations & NS_FONT_DECORATION_OVERLINE)
    {
      nscoord offset;
      nscoord size;
      mFontMetrics->GetUnderline(offset, size);
      FillRect(aX, aY, aWidth, size);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPS :: DrawString(const PRUnichar *aString, PRUint32 aLength,
                                    nscoord aX, nscoord aY, nscoord aWidth,
                                    const nscoord* aSpacing)
{
  PRInt32 x = aX;
  PRInt32 y = aY;

  SetupFontAndColor();

  if (nsnull != aSpacing)
  {
    // XXX Fix path to use a twips transform in the DC and use the
    // spacing values directly and let windows deal with the sub-pixel
    // positioning.

    // Slow, but accurate rendering
    const PRUnichar* end = aString + aLength;
    while (aString < end)
    {
      // XXX can shave some cycles by inlining a version of transform
      // coord where y is constant and transformed once
      x = aX;
      y = aY;
      mTMatrix->TransformCoord(&x, &y);
      //::ExtTextOutW(mDC, x, y, 0, NULL, aString, 1, NULL);
	  //XXX:Remove ::ExtTextOutW above
	  PostscriptTextOut((const char *)aString, 1, NS_PIXELS_TO_POINTS(x), NS_PIXELS_TO_POINTS(y), aWidth, aSpacing, PR_TRUE);
      aX += *aSpacing++;
      aString++;
    }
  }
  else
  {
    mTMatrix->TransformCoord(&x, &y);
    //::ExtTextOutW(mDC, x, y, 0, NULL, aString, aLength, NULL);
	//XXX: Remove ::ExtTextOutW above
	PostscriptTextOut((const char *)aString, aLength, NS_PIXELS_TO_POINTS(x), NS_PIXELS_TO_POINTS(y), aWidth, aSpacing, PR_TRUE);
  }

  if (nsnull != mFontMetrics)
  {
    nsFont *font;
    mFontMetrics->GetFont(font);
    PRUint8 decorations = font->decorations;

    if (decorations & NS_FONT_DECORATION_OVERLINE)
    {
      nscoord offset;
      nscoord size;
      mFontMetrics->GetUnderline(offset, size);
      FillRect(aX, aY, aWidth, size);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPS :: DrawString(const nsString& aString,
                                    nscoord aX, nscoord aY, nscoord aWidth,
                                    const nscoord* aSpacing)
{
  return DrawString(aString.GetUnicode(), aString.Length(), aX, aY, aWidth, aSpacing);
}

NS_IMETHODIMP nsRenderingContextPS :: DrawImage(nsIImage *aImage, nscoord aX, nscoord aY)
{
  //NS_PRECONDITION(PR_TRUE == mInitialized, "!initialized");

  nscoord width, height;

  width = NSToCoordRound(mP2T * aImage->GetWidth());
  height = NSToCoordRound(mP2T * aImage->GetHeight());

  return DrawImage(aImage, aX, aY, width, height);
}

NS_IMETHODIMP nsRenderingContextPS :: DrawImage(nsIImage *aImage, nscoord aX, nscoord aY,
                                        nscoord aWidth, nscoord aHeight) 
{
  nsRect  tr;

  tr.x = aX;
  tr.y = aY;
  tr.width = aWidth;
  tr.height = aHeight;

  return DrawImage(aImage, tr);
}

NS_IMETHODIMP nsRenderingContextPS :: DrawImage(nsIImage *aImage, const nsRect& aSRect, const nsRect& aDRect)
{
  nsRect	sr,dr;

	sr = aSRect;
	mTMatrix->TransformCoord(&sr.x, &sr.y, &sr.width, &sr.height);

  dr = aDRect;
	mTMatrix->TransformCoord(&dr.x, &dr.y, &dr.width, &dr.height);

  //return aImage->Draw(*this, mSurface, sr.x, sr.y, sr.width, sr.height, dr.x, dr.y, dr.width, dr.height);
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPS :: DrawImage(nsIImage *aImage, const nsRect& aRect)
{
  nsRect	tr;

	tr = aRect;
	mTMatrix->TransformCoord(&tr.x, &tr.y, &tr.width, &tr.height);

  //return aImage->Draw(*this, mSurface, tr.x, tr.y, tr.width, tr.height);
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextPS :: CopyOffScreenBits(nsDrawingSurface aSrcSurf,
                                                         PRInt32 aSrcX, PRInt32 aSrcY,
                                                         const nsRect &aDestBounds,
                                                         PRUint32 aCopyFlags)
{
#ifdef NOTNOW
  if ((nsnull != aSrcSurf) && (nsnull != mMainDC))
  {
    PRInt32 x = aSrcX;
    PRInt32 y = aSrcY;
    nsRect  drect = aDestBounds;
    HDC     destdc;

    if (nsnull != ((nsDrawingSurfacePS *)aSrcSurf)->mDC){
      if (aCopyFlags & NS_COPYBITS_TO_BACK_BUFFER){
        NS_ASSERTION(!(nsnull == mDC), "no back buffer");
        destdc = mDC;
      }
      else
        destdc = mMainDC;

      if (aCopyFlags & NS_COPYBITS_USE_SOURCE_CLIP_REGION)
      {
        HRGN  tregion = ::CreateRectRgn(0, 0, 0, 0);

        //if (::GetClipRgn(((nsDrawingSurfacePS *)aSrcSurf)->mDC, tregion) == 1)
          //::SelectClipRgn(destdc, tregion);

        //::DeleteObject(tregion);
      }

      // If there's a palette make sure it's selected.
      // XXX This doesn't seem like the best place to be doing this...

      nsPaletteInfo palInfo;
      HPALETTE      oldPalette;

      mContext->GetPaletteInfo(palInfo);

      if (palInfo.isPaletteDevice && palInfo.palette)
        oldPalette = ::SelectPalette(destdc, (HPALETTE)palInfo.palette, TRUE);

      if (aCopyFlags & NS_COPYBITS_XFORM_SOURCE_VALUES)
        mTMatrix->TransformCoord(&x, &y);

      if (aCopyFlags & NS_COPYBITS_XFORM_DEST_VALUES)
        mTMatrix->TransformCoord(&drect.x, &drect.y, &drect.width, &drect.height);

      ::BitBlt(destdc, drect.x, drect.y,
               drect.width, drect.height,
               ((nsDrawingSurfacePS *)aSrcSurf)->mDC,
               x, y, SRCCOPY);

	  //XXX: Remove BitBlt above when working.
	  PostscriptDrawBitmap(NS_PIXELS_TO_POINTS(drect.x), 
		                   NS_PIXELS_TO_POINTS(drect.y),
						   NS_PIXELS_TO_POINTS(drect.width),
						   NS_PIXELS_TO_POINTS(drect.height),
						   IL_Pixmap *aImage, IL_Pixmap *aMask);
	  

      //if (palInfo.isPaletteDevice && palInfo.palette)
        //::SelectPalette(destdc, oldPalette, TRUE);

    }
    else
      NS_ASSERTION(0, "attempt to blit with bad DCs");
  }
  else
    NS_ASSERTION(0, "attempt to blit with bad DCs");

#endif
  return NS_OK;
}

#ifdef NS_DEBUG
//these are used with the routines below
//to see how our state caching is working... MMP
static numpen = 0;
static numbrush = 0;
static numfont = 0;
#endif

#ifdef DC
HBRUSH nsRenderingContextPS :: SetupSolidBrush(void)
{
  if ((mCurrentColor != mCurrBrushColor) || (NULL == mCurrBrush))
  {
    // XXX In 16-bit mode we need to use GetNearestColor() to get a solid
    // color; otherwise, we'll end up with a dithered brush...
    HBRUSH  tbrush = ::CreateSolidBrush(PALETTERGB_COLORREF(mColor));

    ::SelectObject(mDC, tbrush);

    if (NULL != mCurrBrush)
      ::DeleteObject(mCurrBrush);

    mStates->mSolidBrush = mCurrBrush = tbrush;
    mStates->mBrushColor = mCurrBrushColor = mCurrentColor;
//printf("brushes: %d\n", ++numbrush);
  }

  return mCurrBrush;
}
#endif

void nsRenderingContextPS :: SetupFontAndColor(void)
{
  if (((mFontMetrics != mCurrFontMetrics) || (NULL == mCurrFontMetrics)) &&
      (nsnull != mFontMetrics))
  {
    nsFontHandle  fontHandle;
    mFontMetrics->GetFontHandle(fontHandle);
    HFONT         tfont = (HFONT)fontHandle;

	//XXX:PS Remove SelectObject below
	nscoord fontHeight = 0;
	mFontMetrics->GetHeight(fontHeight);
	nsFont *font;
	mFontMetrics->GetFont(font);
	PostscriptFont(fontHeight, font->style, font->variant, font->weight, font->decorations);
	//XXX:PS Add bold, italic and other settings here

   // ::SelectObject(mDC, tfont);

    mStates->mFont = mCurrFont = tfont;
    mStates->mFontMetrics = mCurrFontMetrics = mFontMetrics;
//printf("fonts: %d\n", ++numfont);
  }

  if (mCurrentColor != mCurrTextColor)
  {
    //::SetTextColor(mDC, PALETTERGB_COLORREF(mColor));
	  // XXX:PS COLOR Remove SetTextColor Above, once it is working correctly
	PostscriptColor(mCurrentColor);

    mStates->mTextColor = mCurrTextColor = mCurrentColor;
  }
}

#ifdef DC
HPEN nsRenderingContextPS :: SetupPen()
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

HPEN nsRenderingContextPS :: SetupSolidPen(void)
{
  if ((mCurrentColor != mCurrPenColor) || (NULL == mCurrPen) || (mCurrPen != mStates->mSolidPen))
  {
    HPEN  tpen = ::CreatePen(PS_SOLID, 0, PALETTERGB_COLORREF(mColor));

    //::SelectObject(mDC, tpen);

    if (NULL != mCurrPen)
      ::DeleteObject(mCurrPen);

    mStates->mSolidPen = mCurrPen = tpen;
    mStates->mPenColor = mCurrPenColor = mCurrentColor;
//printf("pens: %d\n", ++numpen);
  }

  return mCurrPen;
}

HPEN nsRenderingContextPS :: SetupDashedPen(void)
{
  if ((mCurrentColor != mCurrPenColor) || (NULL == mCurrPen) || (mCurrPen != mStates->mDashedPen))
  {
    HPEN  tpen = ::CreatePen(PS_DASH, 0, PALETTERGB_COLORREF(mColor));

    //::SelectObject(mDC, tpen);

    if (NULL != mCurrPen)
      ::DeleteObject(mCurrPen);

    mStates->mDashedPen = mCurrPen = tpen;
    mStates->mPenColor  = mCurrPenColor = mCurrentColor;
//printf("pens: %d\n", ++numpen);
  }

  return mCurrPen;
}

HPEN nsRenderingContextPS :: SetupDottedPen(void)
{
  if ((mCurrentColor != mCurrPenColor) || (NULL == mCurrPen) || (mCurrPen != mStates->mDottedPen))
  {
    HPEN  tpen = ::CreatePen(PS_DOT, 0, PALETTERGB_COLORREF(mColor));

    //::SelectObject(mDC, tpen);

    if (NULL != mCurrPen)
      ::DeleteObject(mCurrPen);

    mStates->mDottedPen = mCurrPen = tpen;
    mStates->mPenColor = mCurrPenColor = mCurrentColor;

//printf("pens: %d\n", ++numpen);
  }

  return mCurrPen;
}

#endif

void nsRenderingContextPS :: PushClipState(void)
{
  if (!(mStates->mFlags & FLAG_CLIP_CHANGED))
  {
    GraphicState *tstate = mStates->mNext;

    //we have never set a clip on this state before, so
    //remember the current clip state in the next state on the
    //stack. kind of wacky, but avoids selecting stuff in the DC
    //all the damned time.

    if (nsnull != tstate)
    {
      if (NULL == tstate->mClipRegion)
        tstate->mClipRegion = ::CreateRectRgn(0, 0, 0, 0);

      //if (::GetClipRgn(mDC, tstate->mClipRegion) == 1)
        //tstate->mFlags |= FLAG_CLIP_VALID;
      //else
        //tstate->mFlags &= ~FLAG_CLIP_VALID;
    }
  
    mStates->mFlags |= FLAG_CLIP_CHANGED;
  }
}

#ifdef DC
NS_IMETHODIMP
nsRenderingContextPS::GetColor(nsString& aColor)
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
nsRenderingContextPS::SetColor(const nsString& aColor)
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
#endif