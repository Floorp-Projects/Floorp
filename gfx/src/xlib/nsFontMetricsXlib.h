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

#ifndef nsFontMetricsXlib_h__
#define nsFontMetricsXlib_h__

#include "nsIFontMetrics.h"
#include "nsFont.h"
#include "nsString.h"
#include "nsUnitConversion.h"
#include "nsIDeviceContext.h"
#include "nsCRT.h"
#include "nsDeviceContextXlib.h"
#include "nsDrawingSurfaceXlib.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>

#define FONT_SWITCHING
#ifdef FONT_SWITCHING

#ifdef ADD_GLYPH
#undef ADD_GLYPH
#endif
#define ADD_GLYPH(map, g) (map)[(g) >> 3] |= (1 << ((g) & 7))

#ifdef FONT_HAS_GLYPH
#undef FONT_HAS_GLYPH
#endif
#define FONT_HAS_GLYPH(map, g) (((map)[(g) >> 3] >> ((g) & 7)) & 1)

typedef struct nsFontCharSetInfo nsFontCharSetInfo;

typedef int (*nsFontCharSetConverter)(nsFontCharSetInfo* aSelf,
             const PRUnichar* aSrcBuf, PRInt32 aSrcLen, char* aDestBuf,
             PRInt32 aDestLen);

struct nsFontCharSet;
class nsFontMetricsXlib;

struct nsFontXlib
{
  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  void LoadFont(nsFontCharSet* aCharSet, nsFontMetricsXlib* aMetrics);

  XFontStruct           *mFont;
  PRUint32*              mMap;
  nsFontCharSetInfo*     mCharSetInfo;
  char*                  mName;
  PRUint16               mSize;
  PRUint16               mActualSize;
  PRInt16                mBaselineAdjust;
};

struct nsFontStretch;
struct nsFontFamily;
typedef struct nsFontSearch nsFontSearch;

#endif /* FONT_SWITCHING */

class nsFontMetricsXlib : public nsIFontMetrics
{
public:
  nsFontMetricsXlib();
  virtual ~nsFontMetricsXlib();

  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  NS_DECL_ISUPPORTS

  NS_IMETHOD  Init(const nsFont& aFont, nsIDeviceContext* aContext);
  NS_IMETHOD  Destroy();

  NS_IMETHOD  GetXHeight(nscoord& aResult);
  NS_IMETHOD  GetSuperscriptOffset(nscoord& aResult);
  NS_IMETHOD  GetSubscriptOffset(nscoord& aResult);
  NS_IMETHOD  GetStrikeout(nscoord& aOffset, nscoord& aSize);
  NS_IMETHOD  GetUnderline(nscoord& aOffset, nscoord& aSize);

  NS_IMETHOD  GetHeight(nscoord &aHeight);
  NS_IMETHOD  GetLeading(nscoord &aLeading);
  NS_IMETHOD  GetMaxAscent(nscoord &aAscent);
  NS_IMETHOD  GetMaxDescent(nscoord &aDescent);
  NS_IMETHOD  GetMaxAdvance(nscoord &aAdvance);
  NS_IMETHOD  GetFont(const nsFont *&aFont);
  NS_IMETHOD  GetFontHandle(nsFontHandle &aHandle);

  virtual nsresult GetSpaceWidth(nscoord &aSpaceWidth);

#ifdef FONT_SWITCHING

  nsFontXlib*  FindFont(PRUnichar aChar);
  static  int GetWidth(nsFontXlib* aFont, const PRUnichar* aString,
                       PRUint32 aLength);
  static void DrawString(nsDrawingSurfaceXlib* aSurface, nsFontXlib* aFont,
                         nscoord aX, nscoord aY, const PRUnichar* aString,
                         PRUint32 aLength);
  static void InitFonts(void);

  friend void PickASizeAndLoad(nsFontSearch* aSearch, nsFontStretch* aStretch,
                               nsFontCharSet* aCharSet);
  friend void TryCharSet(nsFontSearch* aSearch, nsFontCharSet* aCharSet);
  friend void TryFamily(nsFontSearch* aSearch, nsFontFamily* aFamily);
  friend struct nsFontXlib;

  nsFontXlib   **mLoadedFonts;
  PRUint16    mLoadedFontsAlloc;
  PRUint16    mLoadedFontsCount;

  nsString    *mFonts;
  PRUint16    mFontsAlloc;
  PRUint16    mFontsCount;
  PRUint16    mFontsIndex;

#endif /* FONT_SWITCHING */

protected:
  char *PickAppropriateSize(char **names, XFontStruct *fonts, int cnt, nscoord desired);
  void RealizeFont();

  Display             *mDisplay;
  nsIDeviceContext    *mDeviceContext;
  nsFont              *mFont;
  XFontStruct         *mFontHandle;
  XFontStruct         *mFontStruct;

  nscoord             mHeight;
  nscoord             mAscent;
  nscoord             mDescent;
  nscoord             mLeading;
  nscoord             mMaxAscent;
  nscoord             mMaxDescent;
  nscoord             mMaxAdvance;
  nscoord             mXHeight;
  nscoord             mSuperscriptOffset;
  nscoord             mSubscriptOffset;
  nscoord             mStrikeoutSize;
  nscoord             mStrikeoutOffset;
  nscoord             mUnderlineSize;
  nscoord             mUnderlineOffset;
  nscoord             mSpaceWidth;

#ifdef FONT_SWITCHING

  PRUint16            mPixelSize;
  PRUint8             mStretchIndex;
  PRUint8             mStyleIndex;

#endif /* FONT_SWITCHING */
};

#endif
