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

#ifndef nsFontMetricsGTK_h__
#define nsFontMetricsGTK_h__

#include "nsIFontMetrics.h"
#include "nsFont.h"
#include "nsString.h"
#include "nsUnitConversion.h"
#include "nsIDeviceContext.h"
#include "nsCRT.h"
#include "nsDeviceContextGTK.h"

#include <gdk/gdk.h>
#include <gdk/gdkx.h>

#undef FONT_SWITCHING
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

typedef gint (*nsFontCharSetConverter)(nsFontCharSetInfo* aSelf,
  const PRUnichar* aSrcBuf, PRUint32 aSrcLen, PRUint8* aDestBuf,
  PRUint32 aDestLen);

typedef struct nsFontGTK
{
  GdkFont*               mFont;
  PRUint8*               mMap;
  nsFontCharSetInfo*     mCharSetInfo;
  char*                  mName;
} nsFontGTK;

struct nsFontCharSet;
struct nsFontFamily;
typedef struct nsFontSearch nsFontSearch;

#endif /* FONT_SWITCHING */

class nsFontMetricsGTK : public nsIFontMetrics
{
public:
  nsFontMetricsGTK();
  virtual ~nsFontMetricsGTK();

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

#ifdef FONT_SWITCHING

  nsFontGTK*  FindFont(PRUnichar aChar);
  static gint GetWidth(GdkFont* aFont, nsFontCharSetInfo* aInfo,
                       const PRUnichar* aString, PRUint32 aLength);
  static void DrawString(nsDrawingSurfaceGTK* aSurface, GdkFont* aFont,
                         nsFontCharSetInfo* aInfo, nscoord aX, nscoord aY,
			 const PRUnichar* aString, PRUint32 aLength);
  static void InitFonts(void);

  friend void TryCharSet(nsFontSearch* aSearch, nsFontCharSet* aCharSet);
  friend void TryFamily(nsFontSearch* aSearch, nsFontFamily* aFamily);

  nsFontGTK   *mLoadedFonts;
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

  nsIDeviceContext    *mDeviceContext;
  nsFont              *mFont;
  GdkFont             *mFontHandle;

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

#ifdef FONT_SWITCHING

  PRUint16            mPixelSize;
  PRUint8             mStretchIndex;
  PRUint8             mStyleIndex;

#endif /* FONT_SWITCHING */
};

#endif
