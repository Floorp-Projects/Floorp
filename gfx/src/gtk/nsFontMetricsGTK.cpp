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

#include "xp_core.h"
#include "nsQuickSort.h"
#include "nsFontMetricsGTK.h"
#include "nsIServiceManager.h"
#include "nsICharsetConverterManager.h"
#include "nsICharRepresentable.h"
#include "nsCOMPtr.h"
#include "nspr.h"
#include "plhash.h"

#include <gdk/gdk.h>
#include <gdk/gdkx.h>

#include <X11/Xatom.h>

//#undef NOISY_FONTS
//#undef REALLY_NOISY_FONTS

#ifdef DEBUG_pavlov
#define NOISY_FONTS
#define REALLY_NOISY_FONTS
#endif

static NS_DEFINE_IID(kIFontMetricsIID, NS_IFONT_METRICS_IID);

nsFontMetricsGTK::nsFontMetricsGTK()
{
  NS_INIT_REFCNT();
  mDeviceContext = nsnull;
  mFont = nsnull;
  mFontHandle = nsnull;

  mHeight = 0;
  mAscent = 0;
  mDescent = 0;
  mLeading = 0;
  mMaxAscent = 0;
  mMaxDescent = 0;
  mMaxAdvance = 0;
  mXHeight = 0;
  mSuperscriptOffset = 0;
  mSubscriptOffset = 0;
  mStrikeoutSize = 0;
  mStrikeoutOffset = 0;
  mUnderlineSize = 0;
  mUnderlineOffset = 0;
  mSpaceWidth = 0;
}

nsFontMetricsGTK::~nsFontMetricsGTK()
{
  if (nsnull != mFont) {
    delete mFont;
    mFont = nsnull;
  }

#ifdef FONT_SWITCHING

  if (mFonts) {
    delete [] mFonts;
    mFonts = nsnull;
  }

  if (mLoadedFonts) {
    PR_Free(mLoadedFonts);
    mLoadedFonts = nsnull;
  }

  mFontHandle = nsnull;

#else

  if (nsnull != mFontHandle) {
    ::gdk_font_unref (mFontHandle);
  }

#endif

}

NS_IMPL_ISUPPORTS(nsFontMetricsGTK, kIFontMetricsIID)

#ifdef FONT_SWITCHING

static PRBool
FontEnumCallback(const nsString& aFamily, PRBool aGeneric, void *aData)
{
  nsFontMetricsGTK* metrics = (nsFontMetricsGTK*) aData;
  if (metrics->mFontsCount == metrics->mFontsAlloc) {
    int newSize = 2 * (metrics->mFontsAlloc ? metrics->mFontsAlloc : 1);
    nsString* newPointer = new nsString[newSize];
    if (newPointer) {
      for (int i = metrics->mFontsCount - 1; i >= 0; i--) {
        newPointer[i].SetString(metrics->mFonts[i].GetUnicode());
      }
      delete [] metrics->mFonts;
      metrics->mFonts = newPointer;
      metrics->mFontsAlloc = newSize;
    }
    else {
      return PR_FALSE;
    }
  }
  metrics->mFonts[metrics->mFontsCount].SetString(aFamily.GetUnicode());
  metrics->mFonts[metrics->mFontsCount++].ToLowerCase();

  return PR_TRUE;
}

#endif /* FONT_SWITCHING */

NS_IMETHODIMP nsFontMetricsGTK::Init(const nsFont& aFont, nsIDeviceContext* aContext)
{
  NS_ASSERTION(!(nsnull == aContext), "attempt to init fontmetrics with null device context");

#ifdef FONT_SWITCHING

  mFont = new nsFont(aFont);

  mDeviceContext = aContext;

  float app2dev;
  mDeviceContext->GetAppUnitsToDevUnits(app2dev);
  gchar* factorStr = g_getenv("GECKO_FONT_SIZE_FACTOR");
  double factor;
  if (factorStr) {
    factor = atof(factorStr);
  }
  else {
    factor = 1.0;
  }
  mPixelSize = NSToIntRound(app2dev * factor * mFont->size);
  mStretchIndex = 4; // normal
  mStyleIndex = mFont->style;

  mFont->EnumerateFamilies(FontEnumCallback, this);

  nsFontGTK* f = FindFont('a');
  if (!f) {
    return NS_OK; // XXX
  }
  mFontHandle = f->mFont;

  RealizeFont();

#else /* FONT_SWITCHING */

  nsAutoString  firstFace;
  if (NS_OK != aContext->FirstExistingFont(aFont, firstFace)) {
    aFont.GetFirstFamily(firstFace);
  }

  char        **fnames = nsnull;
  PRInt32     namelen = firstFace.Length() + 1;
  char	      *wildstring = (char *)PR_Malloc((namelen << 1) + 200);
  int         numnames = 0;
  char        altitalicization = 0;
  XFontStruct *fonts;
  float       t2d;
  aContext->GetTwipsToDevUnits(t2d);
  PRInt32     dpi = NSToIntRound(t2d * 1440);

  if (nsnull == wildstring)
    return NS_ERROR_NOT_INITIALIZED;

  mFont = new nsFont(aFont);
  mDeviceContext = aContext;
  mFontHandle = nsnull;

  firstFace.ToCString(wildstring, namelen);

  if (abs(dpi - 75) < abs(dpi - 100))
    dpi = 75;
  else
    dpi = 100;

#ifdef NOISY_FONTS
  g_print("looking for font %s (%d)", wildstring, aFont.size / 20);
#endif

  //font properties we care about:
  //name
  //weight (bold, medium)
  //slant (r = normal, i = italic, o = oblique)
  //size in nscoords >> 1

  // XXX oddly enough, enabling font scaling *here* breaks the
  // text-field widgets...
  static PRBool allowFontScaling = PR_FALSE;
#ifdef DEBUG
  static PRBool firstTime = 1;
  if (firstTime) {
    gchar *gsf = g_getenv("GECKO_SCALE_FONTS");
    if (gsf)
    {
      allowFontScaling = PR_TRUE;
//      g_free(gsf);
    }
  }
#endif
  if (allowFontScaling)
  {
    // Try 0,0 dpi first in case we have a scalable font
    PR_snprintf(&wildstring[namelen + 1], namelen + 200,
                "-*-%s-%s-%c-normal-*-*-%d-0-0-*-*-*-*",
                wildstring,
                (aFont.weight <= NS_FONT_WEIGHT_NORMAL) ? "medium" : "bold",
                ((aFont.style == NS_FONT_STYLE_NORMAL)
                 ? 'r'
                 : ((aFont.style == NS_FONT_STYLE_ITALIC) ? 'i' : 'o')),
                aFont.size / 2);
    fnames = ::XListFontsWithInfo(GDK_DISPLAY(), &wildstring[namelen + 1],
                                  200, &numnames, &fonts);
#ifdef NOISY_FONTS
    g_print("  trying %s[%d]", &wildstring[namelen+1], numnames);
#endif
  }

  if (numnames <= 0)
  {
    // If no scalable font, then try using our dpi
    PR_snprintf(&wildstring[namelen + 1], namelen + 200,
                "-*-%s-%s-%c-normal-*-*-*-%d-%d-*-*-*-*",
                wildstring,
                (aFont.weight <= NS_FONT_WEIGHT_NORMAL) ? "medium" : "bold",
                (aFont.style == NS_FONT_STYLE_NORMAL) ? 'r' :
                ((aFont.style == NS_FONT_STYLE_ITALIC) ? 'i' : 'o'), dpi, dpi);
    fnames = ::XListFontsWithInfo(GDK_DISPLAY(), &wildstring[namelen + 1],
                                  200, &numnames, &fonts);
#ifdef NOISY_FONTS
    g_print("  trying %s[%d]", &wildstring[namelen+1], numnames);
#endif

    if (aFont.style == NS_FONT_STYLE_ITALIC)
      altitalicization = 'o';
    else if (aFont.style == NS_FONT_STYLE_OBLIQUE)
      altitalicization = 'i';

    if ((numnames <= 0) && altitalicization)
    {
      PR_snprintf(&wildstring[namelen + 1], namelen + 200,
                  "-*-%s-%s-%c-normal-*-*-*-%d-%d-*-*-*-*",
                  wildstring,
                  (aFont.weight <= NS_FONT_WEIGHT_NORMAL) ? "medium" : "bold",
                  altitalicization, dpi, dpi);

      fnames = ::XListFontsWithInfo(GDK_DISPLAY(), &wildstring[namelen + 1],
                                    200, &numnames, &fonts);
#ifdef NOISY_FONTS
      g_print("  trying %s[%d]", &wildstring[namelen+1], numnames);
#endif
    }


    if (numnames <= 0)
    {
      //we were not able to match the font name at all...

      char *newname = firstFace.ToNewCString();

      PR_snprintf(&wildstring[namelen + 1], namelen + 200,
                  "-*-%s-%s-%c-normal-*-*-*-%d-%d-*-*-*-*",
                  newname,
                  (aFont.weight <= NS_FONT_WEIGHT_NORMAL) ? "medium" : "bold",
                  (aFont.style == NS_FONT_STYLE_NORMAL) ? 'r' :
                  ((aFont.style == NS_FONT_STYLE_ITALIC) ? 'i' : 'o'),
                  dpi, dpi);
      fnames = ::XListFontsWithInfo(GDK_DISPLAY(), &wildstring[namelen + 1],
                                    200, &numnames, &fonts);
#ifdef NOISY_FONTS
      g_print("  trying %s[%d]", &wildstring[namelen+1], numnames);
#endif

      if ((numnames <= 0) && altitalicization)
      {
        PR_snprintf(&wildstring[namelen + 1], namelen + 200,
                    "-*-%s-%s-%c-normal-*-*-*-%d-%d-*-*-*-*",
                    newname,
                    (aFont.weight <= NS_FONT_WEIGHT_NORMAL) ? "medium" : "bold",
                    altitalicization, dpi, dpi);
        fnames = ::XListFontsWithInfo(GDK_DISPLAY(), &wildstring[namelen + 1],
                                      200, &numnames, &fonts);
#ifdef NOISY_FONTS
        g_print("  trying %s[%d]", &wildstring[namelen+1], numnames);
#endif
      }

      delete [] newname;
    }
  }

  if (numnames > 0)
  {
    char *nametouse = PickAppropriateSize(fnames, fonts, numnames, aFont.size);

    mFontHandle = ::gdk_font_load(nametouse);

#ifdef NOISY_FONTS
    g_print(" is: %s\n", nametouse);
#endif

    ::XFreeFontInfo(fnames, fonts, numnames);
  }
  else
  {
    //ack. we're in real trouble, go for fixed...

#ifdef NOISY_FONTS
    g_print(" is: %s\n", "fixed (final fallback)");
#endif

    mFontHandle = ::gdk_font_load("fixed");
  }

  RealizeFont();

  PR_Free(wildstring);

#endif /* FONT_SWITCHING */

  return NS_OK;
}

NS_IMETHODIMP  nsFontMetricsGTK::Destroy()
{
//  NS_IF_RELEASE(mDeviceContext);
  return NS_OK;
}

char *
nsFontMetricsGTK::PickAppropriateSize(char **names, XFontStruct *fonts,
                                      int cnt, nscoord desired)
{
  int         idx;
  float       app2dev;
  mDeviceContext->GetAppUnitsToDevUnits(app2dev);
  PRInt32     desiredPix = NSToIntRound(app2dev * desired);
  XFontStruct *curfont;
  PRInt32     closestMin = -1, minIndex = 0;
  PRInt32     closestMax = 1<<30, maxIndex = 0;

  // Find exact match, closest smallest and closest largest. If the
  // largest is too much larger always pick the smallest.
  for (idx = 0, curfont = fonts; idx < cnt; idx++, curfont++)
  {
    PRInt32 height = curfont->ascent + curfont->descent;
    if (height == desiredPix) {
      // Winner. Found an *exact* match
      return names[idx];
    }

    if (height < desiredPix) {
      // If the height is closer to the desired height, remember this font
      if (height > closestMin) {
        closestMin = height;
        minIndex = idx;
      }
    }
    else {
      if (height < closestMax) {
        closestMax = height;
        maxIndex = idx;
      }
    }
  }

  // If the closest smaller font is closer than the closest larger
  // font, use it.
#ifdef NOISY_FONTS
  printf(" *** desiredPix=%d(%d) min=%d max=%d *** ",
         desiredPix, desired, closestMin, closestMax);
#endif
  if (desiredPix - closestMin <= closestMax - desiredPix) {
    return names[minIndex];
  }

  // If the closest larger font is more than 2 pixels too big, use the
  // closest smaller font. This is done to prevent things from being
  // way too large.
  if (closestMax - desiredPix > 2) {
    return names[minIndex];
  }
  return names[maxIndex];
}

void nsFontMetricsGTK::RealizeFont()
{
  XFontStruct *fontInfo;
  
  fontInfo = (XFontStruct *)GDK_FONT_XFONT(mFontHandle);

  float f;
  mDeviceContext->GetDevUnitsToAppUnits(f);

  mAscent = nscoord(fontInfo->ascent * f);
  mDescent = nscoord(fontInfo->descent * f);
  mMaxAscent = nscoord(fontInfo->ascent * f) ;
  mMaxDescent = nscoord(fontInfo->descent * f);

  mHeight = nscoord((fontInfo->ascent + fontInfo->descent) * f);
  mMaxAdvance = nscoord(fontInfo->max_bounds.width * f);

  // 56% of ascent, best guess for non-true type
  mXHeight = NSToCoordRound((float) fontInfo->ascent* f * 0.56f);

  gint rawWidth = gdk_text_width(mFontHandle, " ", 1); 
  mSpaceWidth = NSToCoordRound(rawWidth * f);

  unsigned long pr = 0;

  if (::XGetFontProperty(fontInfo, XA_X_HEIGHT, &pr))
  {
    mXHeight = nscoord(pr * f);
#ifdef REALLY_NOISY_FONTS
    printf("xHeight=%d\n", mXHeight);
#endif
  }

  if (::XGetFontProperty(fontInfo, XA_UNDERLINE_POSITION, &pr))
  {
    /* this will only be provided from adobe .afm fonts */
    mUnderlineOffset = NSToIntRound(pr * f);
#ifdef REALLY_NOISY_FONTS
    printf("underlineOffset=%d\n", mUnderlineOffset);
#endif
  }
  else
  {
    /* this may need to be different than one for those weird asian fonts */
    /* mHeight is already multipled by f */
    float height;
    height = fontInfo->ascent + fontInfo->descent;
    mUnderlineOffset = -NSToIntRound(MAX (1, floor (0.1 * height + 0.5)) * f);
  }

  if (::XGetFontProperty(fontInfo, XA_UNDERLINE_THICKNESS, &pr))
  {
    /* this will only be provided from adobe .afm fonts */
    mUnderlineSize = nscoord(MAX(f, NSToIntRound(pr * f)));
#ifdef REALLY_NOISY_FONTS
    printf("underlineSize=%d\n", mUnderlineSize);
#endif
  }
  else
  {
    /* mHeight is already multipled by f */
    float height;
    height = fontInfo->ascent + fontInfo->descent;
    mUnderlineSize = NSToIntRound(MAX(1, floor (0.05 * height + 0.5)) * f);
  }

  if (::XGetFontProperty(fontInfo, XA_SUPERSCRIPT_Y, &pr))
  {
    mSuperscriptOffset = nscoord(MAX(f, NSToIntRound(pr * f)));
#ifdef REALLY_NOISY_FONTS
    printf("superscriptOffset=%d\n", mSuperscriptOffset);
#endif
  }
  else
  {
    mSuperscriptOffset = mXHeight;
  }

  if (::XGetFontProperty(fontInfo, XA_SUBSCRIPT_Y, &pr))
  {
    mSubscriptOffset = nscoord(MAX(f, NSToIntRound(pr * f)));
#ifdef REALLY_NOISY_FONTS
    printf("subscriptOffset=%d\n", mSubscriptOffset);
#endif
  }
  else
  {
    mSubscriptOffset = mXHeight;
  }

  /* need better way to calculate this */
  mStrikeoutOffset = NSToIntRound((mAscent + 1) / 2);
  mStrikeoutSize = mUnderlineSize;

  mLeading = 0;
}

NS_IMETHODIMP  nsFontMetricsGTK::GetXHeight(nscoord& aResult)
{
  aResult = mXHeight;
  return NS_OK;
}

NS_IMETHODIMP  nsFontMetricsGTK::GetSuperscriptOffset(nscoord& aResult)
{
  aResult = mSuperscriptOffset;
  return NS_OK;
}

NS_IMETHODIMP  nsFontMetricsGTK::GetSubscriptOffset(nscoord& aResult)
{
  aResult = mSubscriptOffset;
  return NS_OK;
}

NS_IMETHODIMP  nsFontMetricsGTK::GetStrikeout(nscoord& aOffset, nscoord& aSize)
{
  aOffset = mStrikeoutOffset;
  aSize = mStrikeoutSize;
  return NS_OK;
}

NS_IMETHODIMP  nsFontMetricsGTK::GetUnderline(nscoord& aOffset, nscoord& aSize)
{
  aOffset = mUnderlineOffset;
  aSize = mUnderlineSize;
  return NS_OK;
}

NS_IMETHODIMP  nsFontMetricsGTK::GetHeight(nscoord &aHeight)
{
  aHeight = mHeight;
  return NS_OK;
}

NS_IMETHODIMP  nsFontMetricsGTK::GetLeading(nscoord &aLeading)
{
  aLeading = mLeading;
  return NS_OK;
}

NS_IMETHODIMP  nsFontMetricsGTK::GetMaxAscent(nscoord &aAscent)
{
  aAscent = mMaxAscent;
  return NS_OK;
}

NS_IMETHODIMP  nsFontMetricsGTK::GetMaxDescent(nscoord &aDescent)
{
  aDescent = mMaxDescent;
  return NS_OK;
}

NS_IMETHODIMP  nsFontMetricsGTK::GetMaxAdvance(nscoord &aAdvance)
{
  aAdvance = mMaxAdvance;
  return NS_OK;
}

NS_IMETHODIMP  nsFontMetricsGTK::GetFont(const nsFont*& aFont)
{
  aFont = mFont;
  return NS_OK;
}

NS_IMETHODIMP  nsFontMetricsGTK::GetFontHandle(nsFontHandle &aHandle)
{
  aHandle = (nsFontHandle)mFontHandle;
  return NS_OK;
}

// ===================== new code -- erik ====================

#ifdef FONT_SWITCHING

/*
 * CSS2 "font properties":
 *   font-family
 *   font-style
 *   font-variant
 *   font-weight
 *   font-stretch
 *   font-size
 *   font-size-adjust
 *   font
 */

/*
 * CSS2 "font descriptors":
 *   font-family
 *   font-style
 *   font-variant
 *   font-weight
 *   font-stretch
 *   font-size
 *   unicode-range
 *   units-per-em
 *   src
 *   panose-1
 *   stemv
 *   stemh
 *   slope
 *   cap-height
 *   x-height
 *   ascent
 *   descent
 *   widths
 *   bbox
 *   definition-src
 *   baseline
 *   centerline
 *   mathline
 *   topline
 */

/*
 * XLFD 1.5 "FontName fields":
 *   FOUNDRY
 *   FAMILY_NAME
 *   WEIGHT_NAME
 *   SLANT
 *   SETWIDTH_NAME
 *   ADD_STYLE_NAME
 *   PIXEL_SIZE
 *   POINT_SIZE
 *   RESOLUTION_X
 *   RESOLUTION_Y
 *   SPACING
 *   AVERAGE_WIDTH
 *   CHARSET_REGISTRY
 *   CHARSET_ENCODING
 * XLFD example:
 *   -adobe-times-medium-r-normal--17-120-100-100-p-84-iso8859-1
 */

/*
 * XLFD 1.5 "font properties":
 *   FOUNDRY
 *   FAMILY_NAME
 *   WEIGHT_NAME
 *   SLANT
 *   SETWIDTH_NAME
 *   ADD_STYLE_NAME
 *   PIXEL_SIZE
 *   POINT_SIZE
 *   RESOLUTION_X
 *   RESOLUTION_Y
 *   SPACING
 *   AVERAGE_WIDTH
 *   CHARSET_REGISTRY
 *   CHARSET_ENCODING
 *   MIN_SPACE
 *   NORM_SPACE
 *   MAX_SPACE
 *   END_SPACE
 *   AVG_CAPITAL_WIDTH
 *   AVG_LOWERCASE_WIDTH
 *   QUAD_WIDTH
 *   FIGURE_WIDTH
 *   SUPERSCRIPT_X
 *   SUPERSCRIPT_Y
 *   SUBSCRIPT_X
 *   SUBSCRIPT_Y
 *   SUPERSCRIPT_SIZE
 *   SUBSCRIPT_SIZE
 *   SMALL_CAP_SIZE
 *   UNDERLINE_POSITION
 *   UNDERLINE_THICKNESS
 *   STRIKEOUT_ASCENT
 *   STRIKEOUT_DESCENT
 *   ITALIC_ANGLE
 *   CAP_HEIGHT
 *   X_HEIGHT
 *   RELATIVE_SETWIDTH
 *   RELATIVE_WEIGHT
 *   WEIGHT
 *   RESOLUTION
 *   FONT
 *   FACE_NAME
 *   FULL_NAME
 *   COPYRIGHT
 *   NOTICE
 *   DESTINATION
 *   FONT_TYPE
 *   FONT_VERSION
 *   RASTERIZER_NAME
 *   RASTERIZER_VERSION
 *   RAW_ASCENT
 *   RAW_DESCENT
 *   RAW_*
 *   AXIS_NAMES
 *   AXIS_LIMITS
 *   AXIS_TYPES
 */

/*
 * XLFD 1.5 BDF 2.1 properties:
 *   FONT_ASCENT
 *   FONT_DESCENT
 *   DEFAULT_CHAR
 */

/*
 * CSS2 algorithm, in the following order:
 *   font-family:  FAMILY_NAME (and FOUNDRY? (XXX))
 *   font-style:   SLANT (XXX: XLFD's RI and RO)
 *   font-variant: implemented in mozilla/layout/html/base/src/nsTextFrame.cpp
 *   font-weight:  RELATIVE_WEIGHT (XXX), WEIGHT (XXX), WEIGHT_NAME
 *   font-size:    XFontStruct.max_bounds.ascent + descent
 *
 * The following property is not specified in the algorithm spec. It will be
 * inserted between the font-weight and font-size steps for now:
 *   font-stretch: RELATIVE_SETWIDTH (XXX), SETWIDTH_NAME
 */

/*
 * XXX: Things to investigate in the future:
 *   ADD_STYLE_NAME font-family's serif and sans-serif
 *   SPACING        font-family's monospace; however, there are very few
 *                  proportional fonts in non-Latin-1 charsets, so beware in
 *                  font prefs dialog
 *   AVERAGE_WIDTH  none (see SETWIDTH_NAME)
 */

struct nsFontCharSetInfo
{
  const char*            mCharSet;
  nsFontCharSetConverter Convert;
  PRUint8                mSpecialUnderline;
  PRUint32*              mMap;
  nsIUnicodeEncoder*     mConverter;
};

struct nsFontStretch
{
  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  void SortSizes(void);

  nsFontGTK*         mSizes;
  PRUint16           mSizesAlloc;
  PRUint16           mSizesCount;

  char*              mScalable;
  nsFontGTK**        mScaledFonts;
  PRUint16           mScaledFontsAlloc;
  PRUint16           mScaledFontsCount;
};

struct nsFontWeight
{
  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  void FillStretchHoles(void);

  nsFontStretch* mStretches[9];
};

struct nsFontStyle
{
  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  void FillWeightHoles(void);

  nsFontWeight* mWeights[9];
};

struct nsFontCharSet
{
  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  void FillStyleHoles(void);

  nsFontCharSetInfo* mInfo;
  nsFontStyle*       mStyles[3];
  PRUint8            mHolesFilled;
};

struct nsFontFamily
{
  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  PLHashTable* mCharSets;
};

typedef struct nsFontFamilyName
{
  char* mName;
  char* mXName;
} nsFontFamilyName;

typedef struct nsFontPropertyName
{
  char* mName;
  int   mValue;
} nsFontPropertyName;

typedef struct nsFontCharSetMap
{
  char*              mName;
  nsFontCharSetInfo* mInfo;
} nsFontCharSetMap;

static PLHashTable* gFamilies = nsnull;

static PLHashTable* gFamilyNames = nsnull;

static nsFontFamilyName gFamilyNameTable[] =
{
  { "arial",           "helvetica" },
  { "courier new",     "courier" },
  { "times new roman", "times" },

  { "serif",           "times" },
  { "sans-serif",      "helvetica" },
  { "fantasy",         "courier" },
  { "cursive",         "courier" },
  { "monospace",       "courier" },
  { "-moz-fixed",      "courier" },

  { nsnull, nsnull }
};

static PLHashTable* gWeights = nsnull;

static nsFontPropertyName gWeightNames[] =
{
  { "black",    900 },
  { "bold",     700 },
  { "book",     400 },
  { "demi",     600 },
  { "demibold", 600 },
  { "light",    300 },
  { "medium",   400 },
  { "regular",  400 },
  
  { nsnull,     0 }
};

static PLHashTable* gStretches = nsnull;

static nsFontPropertyName gStretchNames[] =
{
  { "block",         5 }, // XXX
  { "bold",          7 }, // XXX
  { "double wide",   9 },
  { "medium",        5 },
  { "narrow",        3 },
  { "normal",        5 },
  { "semicondensed", 4 },
  { "wide",          7 },

  { nsnull,          0 }
};

static PLHashTable* gCharSets = nsnull;

static nsFontCharSetInfo Ignore = { nsnull };

static gint
SingleByteConvert(nsFontCharSetInfo* aSelf, const PRUnichar* aSrcBuf,
  PRInt32 aSrcLen, char* aDestBuf, PRInt32 aDestLen)
{
  gint count = 0;
  if (aSelf->mConverter) {
    aSelf->mConverter->Convert(aSrcBuf, &aSrcLen, aDestBuf, &aDestLen);
    count = aDestLen;
  }

  return count;
}
static void 
ReverseBuffer(char* aBuf, gint count)
{
    char *head, *tail, *med;
    head = aBuf;
    tail = &aBuf[count-1];
    med = &aBuf[count/2];

    while(head < med)
    {
       char tmp = *head;
       *head++ = *tail;
       *tail-- = tmp;
    }
}

static gint
SingleByteConvertReverse(nsFontCharSetInfo* aSelf, const PRUnichar* aSrcBuf,
  PRInt32 aSrcLen, char* aDestBuf, PRInt32 aDestLen)
{
    gint count = SingleByteConvert(aSelf, aSrcBuf,
                       aSrcLen, aDestBuf,  aDestLen);
    ReverseBuffer(aDestBuf, count);
    return count;
}

static gint
DoubleByteConvert(nsFontCharSetInfo* aSelf, const PRUnichar* aSrcBuf,
  PRInt32 aSrcLen, char* aDestBuf, PRInt32 aDestLen)
{
  gint count = 0;
  if (aSelf->mConverter) {
    aSelf->mConverter->Convert(aSrcBuf, &aSrcLen, aDestBuf, &aDestLen);
    count = aDestLen;
  }
  // XXX do high-bit if font requires it

  return count;
}

static gint
ISO10646Convert(nsFontCharSetInfo* aSelf, const PRUnichar* aSrcBuf,
  PRInt32 aSrcLen, char* aDestBuf, PRInt32 aDestLen)
{
  aDestLen /= 2;
  if (aSrcLen > aDestLen) {
    aSrcLen = aDestLen;
  }
  if (aSrcLen < 0) {
    aSrcLen = 0;
  }
  XChar2b* dest = (XChar2b*) aDestBuf;
  for (PRInt32 i = 0; i < aSrcLen; i++) {
    dest[i].byte1 = (aSrcBuf[i] >> 8);
    dest[i].byte2 = (aSrcBuf[i] & 0xFF);
  }

  return (gint) aSrcLen * 2;
}

static void
SetUpFontCharSetInfo(nsFontCharSetInfo* aSelf)
{
  nsresult result;
  NS_WITH_SERVICE(nsICharsetConverterManager, manager,
    NS_CHARSETCONVERTERMANAGER_PROGID, &result);
  if (manager && NS_SUCCEEDED(result)) {
    nsAutoString charset(aSelf->mCharSet);
    nsIUnicodeEncoder* converter = nsnull;
    result = manager->GetUnicodeEncoder(&charset, &converter);
    if (converter && NS_SUCCEEDED(result)) {
      aSelf->mConverter = converter;
      result = converter->SetOutputErrorBehavior(converter->kOnError_Replace,
        nsnull, '?');
      nsCOMPtr<nsICharRepresentable> mapper = do_QueryInterface(converter);
      if (mapper) {
        result = mapper->FillInfo(aSelf->mMap);

        /*
         * XXX This is a bit of a hack. Documents containing the CP1252
         * extensions of Latin-1 (e.g. smart quotes) will display with those
         * special characters way too large. This is because they happen to
         * be in these large double byte fonts. So, we disable those
         * characters here. Revisit this decision later.
         */
        if (aSelf->Convert == DoubleByteConvert) {
          PRUint32* map = aSelf->mMap;
          for (PRUint16 i = 0; i < (0x3000 >> 5); i++) {
            map[i] = 0;
          }
        }
      }
    }
  }
}

static nsFontCharSetInfo CP1251 =
  { "windows-1251", SingleByteConvert, 0 };
static nsFontCharSetInfo ISO88591 =
  { "iso-8859-1", SingleByteConvert, 0 };
static nsFontCharSetInfo ISO88592 =
  { "iso-8859-2", SingleByteConvert, 0 };
static nsFontCharSetInfo ISO88593 =
  { "iso-8859-3", SingleByteConvert, 0 };
static nsFontCharSetInfo ISO88594 =
  { "iso-8859-4", SingleByteConvert, 0 };
static nsFontCharSetInfo ISO88595 =
  { "iso-8859-5", SingleByteConvert, 0 };
static nsFontCharSetInfo ISO88596 =
  { "iso-8859-6", SingleByteConvert, 0 };
static nsFontCharSetInfo ISO88597 =
  { "iso-8859-7", SingleByteConvert, 0 };
static nsFontCharSetInfo ISO88598 =
  { "iso-8859-8", SingleByteConvertReverse, 0 };
static nsFontCharSetInfo ISO88599 =
  { "iso-8859-9", SingleByteConvert, 0 };
static nsFontCharSetInfo ISO885915 =
  { "iso-8859-15", SingleByteConvert, 0 };
static nsFontCharSetInfo JISX0201 =
  { "jis_0201", SingleByteConvert, 1 };
static nsFontCharSetInfo KOI8R =
  { "KOI8-R", SingleByteConvert, 0 };
static nsFontCharSetInfo TIS620 =
  { "TIS-620", SingleByteConvert, 0 };

static nsFontCharSetInfo Big5 =
  { "x-x-big5", DoubleByteConvert, 1 };
static nsFontCharSetInfo CNS116431 =
  { "x-cns-11643-1", DoubleByteConvert, 1 };
static nsFontCharSetInfo CNS116432 =
  { "x-cns-11643-2", DoubleByteConvert, 1 };
static nsFontCharSetInfo CNS116433 =
  { "x-cns-11643-3", DoubleByteConvert, 1 };
static nsFontCharSetInfo CNS116434 =
  { "x-cns-11643-4", DoubleByteConvert, 1 };
static nsFontCharSetInfo CNS116435 =
  { "x-cns-11643-5", DoubleByteConvert, 1 };
static nsFontCharSetInfo CNS116436 =
  { "x-cns-11643-6", DoubleByteConvert, 1 };
static nsFontCharSetInfo CNS116437 =
  { "x-cns-11643-7", DoubleByteConvert, 1 };
static nsFontCharSetInfo GB2312 =
  { "gb_2312-80", DoubleByteConvert, 1 };
static nsFontCharSetInfo JISX0208 =
  { "jis_0208-1983", DoubleByteConvert, 1 };
static nsFontCharSetInfo JISX0212 =
  { "jis_0212-1990", DoubleByteConvert, 1 };
static nsFontCharSetInfo KSC5601 =
  { "ks_c_5601-1987", DoubleByteConvert, 1 };
static nsFontCharSetInfo X11Johab =
  { "x-x11johab", DoubleByteConvert, 1 };

static nsFontCharSetInfo ISO106461 =
  { nsnull, ISO10646Convert, 1 };

/*
 * Normally, the charset of an X font can be determined simply by looking at
 * the last 2 fields of the long XLFD font name (CHARSET_REGISTRY and
 * CHARSET_ENCODING). However, there are a number of special cases:
 *
 * Sometimes, X server vendors use the same name to mean different things. For
 * example, IRIX uses "cns11643-1" to mean the 2nd plane of CNS 11643, while
 * Solaris uses that name for the 1st plane.
 *
 * Some X server vendors use certain names for something completely different.
 * For example, some Solaris fonts say "gb2312.1980-0" but are actually ASCII
 * fonts. These cases can be detected by looking at the POINT_SIZE and
 * AVERAGE_WIDTH fields. If the average width is half the point size, this is
 * an ASCII font, not GB 2312.
 *
 * Some fonts say "fontspecific" in the CHARSET_ENCODING field. Their charsets
 * depend on the FAMILY_NAME. For example, the following is a "Symbol" font:
 *
 *   -adobe-symbol-medium-r-normal--17-120-100-100-p-95-adobe-fontspecific
 *
 * Some vendors use one name to mean 2 different things, depending on the font.
 * For example, AIX has some "ksc5601.1987-0" fonts that require the 8th bit of
 * both bytes to be zero, while other fonts require them to be set to one.
 * These cases can be distinguished by looking at the FOUNDRY field, but a
 * better way is to look at XFontStruct.min_byte1.
 */
static nsFontCharSetMap gCharSetMap[] =
{
  { "-ascii",             &Ignore        },
  { "-ibm pc",            &Ignore        },
  { "adobe-fontspecific", &Ignore        },
  { "cns11643.1986-1",    &CNS116431     },
  { "cns11643.1986-2",    &CNS116432     },
  { "cns11643.1992-1",    &CNS116431     },
  { "cns11643.1992.1-0",  &CNS116431     },
  { "cns11643.1992-12",   &Ignore        },
  { "cns11643.1992.2-0",  &CNS116432     },
  { "cns11643.1992-2",    &CNS116432     },
  { "cns11643.1992-3",    &CNS116433     },
  { "cns11643.1992.3-0",  &CNS116433     },
  { "cns11643.1992.4-0",  &CNS116434     },
  { "cns11643.1992-4",    &CNS116434     },
  { "cns11643.1992.5-0",  &CNS116435     },
  { "cns11643.1992-5",    &CNS116435     },
  { "cns11643.1992.6-0",  &CNS116436     },
  { "cns11643.1992-6",    &CNS116436     },
  { "cns11643.1992.7-0",  &CNS116437     },
  { "cns11643.1992-7",    &CNS116437     },
  { "cp1251-1",           &CP1251        },
  { "dec-dectech",        &Ignore        },
  { "dtsymbol-1",         &Ignore        },
  { "fontspecific-0",     &Ignore        },
  { "gb2312.1980-0",      &GB2312        },
  { "gb2312.1980-1",      &GB2312        },
  { "hp-japanese15",      &Ignore        },
  { "hp-japaneseeuc",     &Ignore        },
  { "hp-roman8",          &Ignore        },
  { "hp-schinese15",      &Ignore        },
  { "hp-tchinese15",      &Ignore        },
  { "hp-tchinesebig5",    &Big5          },
  { "hp-wa",              &Ignore        },
  { "hpbig5-",            &Big5          },
  { "hproc16-",           &Ignore        },
  { "ibm-1252",           &Ignore        },
  { "ibm-850",            &Ignore        },
  { "ibm-fontspecific",   &Ignore        },
  { "ibm-sbdcn",          &Ignore        },
  { "ibm-sbdtw",          &Ignore        },
  { "ibm-special",        &Ignore        },
  { "ibm-udccn",          &Ignore        },
  { "ibm-udcjp",          &Ignore        },
  { "ibm-udctw",          &Ignore        },
  { "iso646.1991-irv",    &Ignore        },
  { "iso8859-1",          &ISO88591      },
  { "iso8859-15",         &ISO885915     },
  { "iso8859-1@cn",       &Ignore        },
  { "iso8859-1@kr",       &Ignore        },
  { "iso8859-1@tw",       &Ignore        },
  { "iso8859-1@zh",       &Ignore        },
  { "iso8859-2",          &ISO88592      },
  { "iso8859-3",          &ISO88593      },
  { "iso8859-4",          &ISO88594      },
  { "iso8859-5",          &ISO88595      },
  { "iso8859-6",          &ISO88596      },
  { "iso8859-7",          &ISO88597      },
  { "iso8859-8",          &ISO88598      },
  { "iso8859-9",          &ISO88599      },
  { "iso10646-1",         &ISO106461     },
  { "jisx0201.1976-0",    &JISX0201      },
  { "jisx0201.1976-1",    &JISX0201      },
  { "jisx0208.1983-0",    &JISX0208      },
  { "jisx0208.1990-0",    &JISX0208      },
  { "jisx0212.1990-0",    &JISX0212      },
  { "koi8-r",             &KOI8R         },
  { "johab-1",            &X11Johab      },
  { "johabs-1",           &X11Johab      },
  { "johabsh-1",          &X11Johab      },
  { "ksc5601.1987-0",     &KSC5601       },
  { "misc-fontspecific",  &Ignore        },
  { "sgi-fontspecific",   &Ignore        },
  { "sun-fontspecific",   &Ignore        },
  { "sunolcursor-1",      &Ignore        },
  { "sunolglyph-1",       &Ignore        },
  { "tis620.2529-1",      &TIS620        },
  { "ucs2.cjk-0",         &Ignore        },
  { "ucs2.cjk_japan-0",   &Ignore        },
  { "ucs2.cjk_taiwan-0",  &Ignore        },

  { nsnull,               nsnull         }
};

#undef DEBUG_DUMP_TREE
#ifdef DEBUG_DUMP_TREE

static char* gDumpStyles[3] = { "normal", "italic", "oblique" };

static PRIntn
DumpCharSet(PLHashEntry* he, PRIntn i, void* arg)
{
  printf("        %s\n", (char*) he->key);
  nsFontCharSet* charSet = (nsFontCharSet*) he->value;
  for (int sizeIndex = 0; sizeIndex < charSet->mSizesCount; sizeIndex++) {
    nsFontGTK* size = &charSet->mSizes[sizeIndex];
    printf("          %d %s\n", size->mSize, size->mName);
  }
  return HT_ENUMERATE_NEXT;
}

static void
DumpFamily(nsFontFamily* aFamily)
{
  for (int styleIndex = 0; styleIndex < 3; styleIndex++) {
    nsFontStyle* style = aFamily->mStyles[styleIndex];
    if (style) {
      printf("  style: %s\n", gDumpStyles[styleIndex]);
      for (int weightIndex = 0; weightIndex < 8; weightIndex++) {
        nsFontWeight* weight = style->mWeights[weightIndex];
        if (weight) {
          printf("    weight: %d\n", (weightIndex + 1) * 100);
          for (int stretchIndex = 0; stretchIndex < 9; stretchIndex++) {
            nsFontStretch* stretch = weight->mStretches[stretchIndex];
            if (stretch) {
              printf("      stretch: %d\n", stretchIndex + 1);
              PL_HashTableEnumerateEntries(stretch->mCharSets, DumpCharSet,
                nsnull);
            }
          }
        }
      }
    }
  }
}

static PRIntn
DumpFamilyEnum(PLHashEntry* he, PRIntn i, void* arg)
{
  char buf[256];
  ((nsString*) he->key)->ToCString(buf, sizeof(buf));
  printf("family: %s\n", buf);
  nsFontFamily* family = (nsFontFamily*) he->value;
  DumpFamily(family);

  return HT_ENUMERATE_NEXT;
}

static void
DumpTree(void)
{
  PL_HashTableEnumerateEntries(gFamilies, DumpFamilyEnum, nsnull);
}

#endif /* DEBUG_DUMP_TREE */

static PLHashNumber
HashKey(const void* aString)
{
  return (PLHashNumber)
    nsCRT::HashValue(((const nsString*) aString)->GetUnicode());
}

static PRIntn
CompareKeys(const void* aStr1, const void* aStr2)
{
  return nsCRT::strcmp(((const nsString*) aStr1)->GetUnicode(),
    ((const nsString*) aStr2)->GetUnicode()) == 0;
}

struct nsFontSearch
{
  nsFontMetricsGTK* mMetrics;
  PRUnichar         mChar;
  nsFontGTK*        mFont;
};

static void
GetUnderlineInfo(XFontStruct* aFont, unsigned long* aPositionX2,
  unsigned long* aThickness)
{
  /*
   * XLFD 1.5 says underline position defaults descent/2.
   * Hence we return position*2 to avoid rounding error.
   */
  if (::XGetFontProperty(aFont, XA_UNDERLINE_POSITION, aPositionX2)) {
    *aPositionX2 *= 2;
  }
  else {
    *aPositionX2 = aFont->max_bounds.descent;
  }

  /*
   * XLFD 1.5 says underline thickness defaults to cap stem width.
   * We don't know what that is, so we just take the thickness of "_".
   * This way, we get thicker underlines for bold fonts.
   */
  if (!::XGetFontProperty(aFont, XA_UNDERLINE_THICKNESS, aThickness)) {
    int dir, ascent, descent;
    XCharStruct overall;
    XTextExtents(aFont, "_", 1, &dir, &ascent, &descent, &overall);
    *aThickness = (overall.ascent + overall.descent);
  }
}

static PRUint32*
GetMapFor10646Font(XFontStruct* aFont)
{
  PRUint32* map = (PRUint32*) PR_Calloc(2048, 4);
  if (map) {
    if (aFont->per_char) {
      PRInt32 minByte1 = aFont->min_byte1;
      PRInt32 maxByte1 = aFont->max_byte1;
      PRInt32 minByte2 = aFont->min_char_or_byte2;
      PRInt32 maxByte2 = aFont->max_char_or_byte2;
      PRInt32 charsPerRow = maxByte2 - minByte2 + 1;
      for (PRInt32 row = minByte1; row <= maxByte1; row++) {
        PRInt32 offset = (((row - minByte1) * charsPerRow) - minByte2);
        for (PRInt32 cell = minByte2; cell <= maxByte2; cell++) {
          XCharStruct* bounds = &aFont->per_char[offset + cell];
          if ((!bounds->ascent) && (!bounds->descent)) {
            SET_REPRESENTABLE(map, (row << 8) | cell);
          }
        }
      }
    }
    else {
      // XXX look at glyph ranges property, if any
      PR_Free(map);
      map = nsnull;
    }
  }

  return map;
}

void
nsFontGTK::LoadFont(nsFontCharSet* aCharSet, nsFontMetricsGTK* aMetrics)
{
  GdkFont* gdkFont = ::gdk_font_load(mName);
  if (gdkFont) {
    XFontStruct* xFont = (XFontStruct*) GDK_FONT_XFONT(gdkFont);
    if (aCharSet->mInfo->mCharSet) {
      mMap = aCharSet->mInfo->mMap;
    }
    else {
      mMap = GetMapFor10646Font(xFont);
      if (!mMap) {
        ::gdk_font_unref(gdkFont);
        return;
      }
    }
    mFont = gdkFont;
    mActualSize = xFont->max_bounds.ascent + xFont->max_bounds.descent;
    if (aCharSet->mInfo->mSpecialUnderline) {
      XFontStruct* asciiXFont =
        (XFontStruct*) GDK_FONT_XFONT(aMetrics->mFontHandle);
      unsigned long positionX2;
      unsigned long thickness;
      GetUnderlineInfo(asciiXFont, &positionX2, &thickness);
      mActualSize += (positionX2 + thickness);
      mBaselineAdjust = (-xFont->max_bounds.descent);
    }
  }
}

void
PickASizeAndLoad(nsFontSearch* aSearch, nsFontStretch* aStretch,
  nsFontCharSet* aCharSet)
{
  nsFontGTK* s = nsnull;
  nsFontGTK* begin;
  nsFontGTK* end;
  nsFontMetricsGTK* m = aSearch->mMetrics;
  int desiredSize = m->mPixelSize;
  int scalable = 0;

  if (aStretch->mSizes) {
    begin = aStretch->mSizes;
    end = &aStretch->mSizes[aStretch->mSizesCount];
    for (s = begin; s < end; s++) {
      if (s->mSize >= desiredSize) {
        break;
      }
    }
    if (s == end) {
      s--;
    }
    else if (s != begin) {
      if ((s->mSize - desiredSize) > (desiredSize - (s - 1)->mSize)) {
        s--;
      }
    }
  
    if (!s->mFont) {
      s->LoadFont(aCharSet, m);
      if (!s->mFont) {
        return;
      }
    }
    if (s->mActualSize > desiredSize) {
      for (; s >= begin; s--) {
        if (!s->mFont) {
          s->LoadFont(aCharSet, m);
          if (!s->mFont) {
            return;
          }
        }
        if (s->mActualSize <= desiredSize) {
          if (((s + 1)->mActualSize - desiredSize) <=
              (desiredSize - s->mActualSize)) {
            s++;
          }
          break;
        }
      }
      if (s < begin) {
        s = begin;
      }
    }
    else if (s->mActualSize < desiredSize) {
      for (; s < end; s++) {
        if (!s->mFont) {
          s->LoadFont(aCharSet, m);
          if (!s->mFont) {
            return;
          }
        }
        if (s->mActualSize >= desiredSize) {
          if ((s->mActualSize - desiredSize) >
              (desiredSize - (s - 1)->mActualSize)) {
            s--;
          }
          break;
        }
      }
      if (s == end) {
        s--;
      }
    }

    if (aStretch->mScalable) {
      double ratio = (s->mActualSize / ((double) desiredSize));

      /*
       * XXX Maybe revisit this. Upper limit deliberately set high (1.8) in
       * order to avoid scaling Japanese fonts (ugly).
       */
      if ((ratio > 1.8) || (ratio < 0.8)) {
        scalable = 1;
      }
    }
  }
  else {
    scalable = 1;
  }

  if (scalable) {
    nsFontGTK* closestBitmapSize = s;
    nsFontGTK** beginScaled = aStretch->mScaledFonts;
    nsFontGTK** endScaled =
      &aStretch->mScaledFonts[aStretch->mScaledFontsCount];
    nsFontGTK** p;
    for (p = beginScaled; p < endScaled; p++) {
      if ((*p)->mSize == desiredSize) {
        break;
      }
    }
    if (p == endScaled) {
      s = new nsFontGTK;
      if (s) {
        /*
         * XXX Instead of passing desiredSize, we ought to take underline
         * into account. (Extra space for underline for Asian fonts.)
         */
        s->mName = PR_smprintf(aStretch->mScalable, desiredSize);
        if (!s->mName) {
          delete s;
          return;
        }
        s->mSize = desiredSize;
        s->mCharSetInfo = aCharSet->mInfo;
        s->LoadFont(aCharSet, m);
        if (s->mFont) {
          if (aStretch->mScaledFontsCount == aStretch->mScaledFontsAlloc) {
            int newSize = 2 *
              (aStretch->mScaledFontsAlloc ? aStretch->mScaledFontsAlloc : 1);
            nsFontGTK** newPointer = (nsFontGTK**)
              PR_Realloc(aStretch->mScaledFonts, newSize * sizeof(nsFontGTK*));
            if (newPointer) {
              aStretch->mScaledFontsAlloc = newSize;
              aStretch->mScaledFonts = newPointer;
            }
            else {
              delete s;
              return;
            }
          }
          aStretch->mScaledFonts[aStretch->mScaledFontsCount++] = s;
        }
        else {
          delete s;
          s = nsnull;
        }
      }
      if (!s) {
        if (closestBitmapSize) {
          s = closestBitmapSize;
        }
        else {
          return;
        }
      }
    }
    else {
      s = *p;
    }
  }

  if (!aCharSet->mInfo->mCharSet) {
    if (!IS_REPRESENTABLE(s->mMap, aSearch->mChar)) {
      return;
    }
  }

  if (m->mLoadedFontsCount == m->mLoadedFontsAlloc) {
    int newSize;
    if (m->mLoadedFontsAlloc) {
      newSize = (2 * m->mLoadedFontsAlloc);
    }
    else {
      newSize = 1;
    }
    nsFontGTK** newPointer = (nsFontGTK**) PR_Realloc(m->mLoadedFonts,
      newSize * sizeof(nsFontGTK*));
    if (newPointer) {
      m->mLoadedFonts = newPointer;
      m->mLoadedFontsAlloc = newSize;
    }
    else {
      return;
    }
  }
  m->mLoadedFonts[m->mLoadedFontsCount++] = s;
  aSearch->mFont = s;

#if 0
  nsFontGTK* result = s;
  for (s = begin; s < end; s++) {
    printf("%d/%d ", s->mSize, s->mActualSize);
  }
  printf("desired %d chose %d\n", desiredSize, result->mActualSize);
#endif /* 0 */
}

static int
CompareSizes(const void* aArg1, const void* aArg2, void *data)
{
  return ((nsFontGTK*) aArg1)->mSize - ((nsFontGTK*) aArg2)->mSize;
}

void
nsFontStretch::SortSizes(void)
{
  NS_QuickSort(mSizes, mSizesCount, sizeof(*mSizes), CompareSizes, NULL);
}

void
nsFontWeight::FillStretchHoles(void)
{
  int i, j;

  for (i = 0; i < 9; i++) {
    if (mStretches[i]) {
      mStretches[i]->SortSizes();
    }
  }

  if (!mStretches[4]) {
    for (i = 5; i < 9; i++) {
      if (mStretches[i]) {
        mStretches[4] = mStretches[i];
        break;
      }
    }
    if (!mStretches[4]) {
      for (i = 3; i >= 0; i--) {
        if (mStretches[i]) {
          mStretches[4] = mStretches[i];
          break;
        }
      }
    }
  }

  for (i = 5; i < 9; i++) {
    if (!mStretches[i]) {
      for (j = i + 1; j < 9; j++) {
        if (mStretches[j]) {
          mStretches[i] = mStretches[j];
          break;
        }
      }
      if (!mStretches[i]) {
        for (j = i - 1; j >= 0; j--) {
          if (mStretches[j]) {
            mStretches[i] = mStretches[j];
            break;
          }
        }
      }
    }
  }
  for (i = 3; i >= 0; i--) {
    if (!mStretches[i]) {
      for (j = i - 1; j >= 0; j--) {
        if (mStretches[j]) {
          mStretches[i] = mStretches[j];
          break;
        }
      }
      if (!mStretches[i]) {
        for (j = i + 1; j < 9; j++) {
          if (mStretches[j]) {
            mStretches[i] = mStretches[j];
            break;
          }
        }
      }
    }
  }
}

void
nsFontStyle::FillWeightHoles(void)
{
  int i, j;

  for (i = 0; i < 9; i++) {
    if (mWeights[i]) {
      mWeights[i]->FillStretchHoles();
    }
  }

  if (!mWeights[3]) {
    for (i = 4; i < 9; i++) {
      if (mWeights[i]) {
        mWeights[3] = mWeights[i];
        break;
      }
    }
    if (!mWeights[3]) {
      for (i = 2; i >= 0; i--) {
        if (mWeights[i]) {
          mWeights[3] = mWeights[i];
          break;
        }
      }
    }
  }

  // CSS2, section 15.5.1
  if (!mWeights[4]) {
    mWeights[4] = mWeights[3];
  }
  for (i = 5; i < 9; i++) {
    if (!mWeights[i]) {
      for (j = i + 1; j < 9; j++) {
        if (mWeights[j]) {
          mWeights[i] = mWeights[j];
          break;
        }
      }
      if (!mWeights[i]) {
        for (j = i - 1; j >= 0; j--) {
          if (mWeights[j]) {
            mWeights[i] = mWeights[j];
            break;
          }
        }
      }
    }
  }
  for (i = 2; i >= 0; i--) {
    if (!mWeights[i]) {
      for (j = i - 1; j >= 0; j--) {
        if (mWeights[j]) {
          mWeights[i] = mWeights[j];
          break;
        }
      }
      if (!mWeights[i]) {
        for (j = i + 1; j < 9; j++) {
          if (mWeights[j]) {
            mWeights[i] = mWeights[j];
            break;
          }
        }
      }
    }
  }
}

void
nsFontCharSet::FillStyleHoles(void)
{
  if (mHolesFilled) {
    return;
  }
  mHolesFilled = 1;

#ifdef DEBUG_DUMP_TREE
  DumpFamily(this);
#endif

  for (int i = 0; i < 3; i++) {
    if (mStyles[i]) {
      mStyles[i]->FillWeightHoles();
    }
  }

  // XXX If both italic and oblique exist, there is probably something
  // wrong. Try counting the fonts, and removing the one that has less.
  if (!mStyles[NS_FONT_STYLE_NORMAL]) {
    if (mStyles[NS_FONT_STYLE_ITALIC]) {
      mStyles[NS_FONT_STYLE_NORMAL] = mStyles[NS_FONT_STYLE_ITALIC];
    }
    else {
      mStyles[NS_FONT_STYLE_NORMAL] = mStyles[NS_FONT_STYLE_OBLIQUE];
    }
  }
  if (!mStyles[NS_FONT_STYLE_ITALIC]) {
    if (mStyles[NS_FONT_STYLE_OBLIQUE]) {
      mStyles[NS_FONT_STYLE_ITALIC] = mStyles[NS_FONT_STYLE_OBLIQUE];
    }
    else {
      mStyles[NS_FONT_STYLE_ITALIC] = mStyles[NS_FONT_STYLE_NORMAL];
    }
  }
  if (!mStyles[NS_FONT_STYLE_OBLIQUE]) {
    if (mStyles[NS_FONT_STYLE_ITALIC]) {
      mStyles[NS_FONT_STYLE_OBLIQUE] = mStyles[NS_FONT_STYLE_ITALIC];
    }
    else {
      mStyles[NS_FONT_STYLE_OBLIQUE] = mStyles[NS_FONT_STYLE_NORMAL];
    }
  }

#ifdef DEBUG_DUMP_TREE
  DumpFamily(this);
#endif
}

#define WEIGHT_INDEX(weight) (((weight) / 100) - 1)

#define GET_WEIGHT_INDEX(index, weight) \
  do {                                  \
    (index) = WEIGHT_INDEX(weight);     \
    if ((index) < 0) {                  \
      (index) = 0;                      \
    }                                   \
    else if ((index) > 8) {             \
      (index) = 8;                      \
    }                                   \
  } while (0)

void
TryCharSet(nsFontSearch* aSearch, nsFontCharSet* aCharSet)
{
  aCharSet->FillStyleHoles();
  nsFontMetricsGTK* f = aSearch->mMetrics;
  nsFontStyle* style = aCharSet->mStyles[f->mStyleIndex];
  if (!style) {
    return; // skip dummy entries
  }

  nsFontWeight** weights = style->mWeights;
  int weight = f->mFont->weight;
  int steps = (weight % 100);
  int weightIndex;
  if (steps) {
    if (steps < 10) {
      int base = (weight - (steps * 101));
      GET_WEIGHT_INDEX(weightIndex, base);
      while (steps--) {
        nsFontWeight* prev = weights[weightIndex];
        for (weightIndex++; weightIndex < 9; weightIndex++) {
          if (weights[weightIndex] != prev) {
            break;
          }
        }
        if (weightIndex >= 9) {
          weightIndex = 8;
        }
      }
    }
    else if (steps > 90) {
      steps = (100 - steps);
      int base = (weight + (steps * 101));
      GET_WEIGHT_INDEX(weightIndex, base);
      while (steps--) {
        nsFontWeight* prev = weights[weightIndex];
        for (weightIndex--; weightIndex >= 0; weightIndex--) {
          if (weights[weightIndex] != prev) {
            break;
          }
        }
        if (weightIndex < 0) {
          weightIndex = 0;
        }
      }
    }
    else {
      GET_WEIGHT_INDEX(weightIndex, weight);
    }
  }
  else {
    GET_WEIGHT_INDEX(weightIndex, weight);
  }

  PickASizeAndLoad(aSearch, weights[weightIndex]->mStretches[f->mStretchIndex],
    aCharSet);
}

static PRIntn
SearchCharSet(PLHashEntry* he, PRIntn i, void* arg)
{
  nsFontCharSet* charSet = (nsFontCharSet*) he->value;
  nsFontCharSetInfo* charSetInfo = charSet->mInfo;
  PRUint32* map = charSetInfo->mMap;
  nsFontSearch* search = (nsFontSearch*) arg;
  PRUnichar c = search->mChar;
  if (charSetInfo->mCharSet) {
    if (!map) {
      map = (PRUint32*) PR_Calloc(2048, 4);
      if (!map) {
        return HT_ENUMERATE_NEXT;
      }
      charSetInfo->mMap = map;
      SetUpFontCharSetInfo(charSetInfo);
    }
    if (!IS_REPRESENTABLE(map, c)) {
      return HT_ENUMERATE_NEXT;
    }
  }

  TryCharSet(search, charSet);
  if (search->mFont) {
    return HT_ENUMERATE_STOP;
  }

  return HT_ENUMERATE_NEXT;
}

void
TryFamily(nsFontSearch* aSearch, nsFontFamily* aFamily)
{
  // XXX Should process charsets in reasonable order, instead of randomly
  // enumerating the hash table.
  if (aFamily->mCharSets) {
    PL_HashTableEnumerateEntries(aFamily->mCharSets, SearchCharSet, aSearch);
  }
}

static PRIntn
SearchFamily(PLHashEntry* he, PRIntn i, void* arg)
{
  nsFontFamily* family = (nsFontFamily*) he->value;
  nsFontSearch* search = (nsFontSearch*) arg;
  TryFamily(search, family);
  if (search->mFont) {
    return HT_ENUMERATE_STOP;
  }

  return HT_ENUMERATE_NEXT;
}

static nsFontFamily*
GetFontNames(char* aPattern)
{
  nsFontFamily* family = nsnull;

  int count;
  char** list = ::XListFonts(GDK_DISPLAY(), aPattern, INT_MAX, &count);
  if ((!list) || (count < 1)) {
    return nsnull;
  }
  for (int i = 0; i < count; i++) {
    char* name = list[i];
    if ((!name) || (name[0] != '-')) {
      continue;
    }
    char* p = name + 1;
    int scalable = 0;

#ifdef FIND_FIELD
#undef FIND_FIELD
#endif
#define FIND_FIELD(var)           \
  char* var = p;                  \
  while ((*p) && ((*p) != '-')) { \
    p++;                          \
  }                               \
  if (*p) {                       \
    *p++ = 0;                     \
  }                               \
  else {                          \
    continue;                     \
  }

#ifdef SKIP_FIELD
#undef SKIP_FIELD
#endif
#define SKIP_FIELD(var)           \
  while ((*p) && ((*p) != '-')) { \
    p++;                          \
  }                               \
  if (*p) {                       \
    p++;                          \
  }                               \
  else {                          \
    continue;                     \
  }

    SKIP_FIELD(foundry);
    // XXX What to do about the many Applix fonts that start with "ax"?
    FIND_FIELD(familyName);
    FIND_FIELD(weightName);
    FIND_FIELD(slant);
    FIND_FIELD(setWidth);
    FIND_FIELD(addStyle);
    FIND_FIELD(pixelSize);
    if (pixelSize[0] == '0') {
      scalable = 1;
    }
    FIND_FIELD(pointSize);
    if (pointSize[0] == '0') {
      scalable = 1;
    }
    FIND_FIELD(resolutionX);
    if (resolutionX[0] == '0') {
      scalable = 1;
    }
    FIND_FIELD(resolutionY);
    if (resolutionY[0] == '0') {
      scalable = 1;
    }
    FIND_FIELD(spacing);
    FIND_FIELD(averageWidth);
    if (averageWidth[0] == '0') {
      scalable = 1;
    }
    char* charSetName = p; // CHARSET_REGISTRY & CHARSET_ENCODING
    if (!*charSetName) {
      continue;
    }
    nsFontCharSetInfo* charSetInfo =
      (nsFontCharSetInfo*) PL_HashTableLookup(gCharSets, charSetName);
    if (!charSetInfo) {
#ifdef NOISY_FONTS
      printf("cannot find charset %s\n", charSetName);
#endif
      continue;
    }
    if (charSetInfo == &Ignore) {
      // XXX printf("ignoring %s\n", charSetName);
      continue;
    }

    nsAutoString familyName2(familyName);
    family =
      (nsFontFamily*) PL_HashTableLookup(gFamilies, (nsString*) &familyName2);
    if (!family) {
      family = new nsFontFamily;
      if (!family) {
        continue;
      }
      nsString* copy = new nsString(familyName);
      if (!copy) {
        delete family;
        continue;
      }
      PL_HashTableAdd(gFamilies, copy, family);
    }

    if (!family->mCharSets) {
      family->mCharSets = PL_NewHashTable(0, PL_HashString, PL_CompareStrings,
        NULL, NULL, NULL);
      if (!family->mCharSets) {
        continue;
      }
    }
    nsFontCharSet* charSet =
      (nsFontCharSet*) PL_HashTableLookup(family->mCharSets, charSetName);
    if (!charSet) {
      charSet = new nsFontCharSet;
      if (!charSet) {
        continue;
      }
      char* copy = strdup(charSetName);
      if (!copy) {
        delete charSet;
        continue;
      }
      charSet->mInfo = charSetInfo;
      PL_HashTableAdd(family->mCharSets, copy, charSet);
    }

    int styleIndex;
    // XXX This does not cover the full XLFD spec for SLANT.
    switch (slant[0]) {
    case 'i':
      styleIndex = NS_FONT_STYLE_ITALIC;
      break;
    case 'o':
      styleIndex = NS_FONT_STYLE_OBLIQUE;
      break;
    case 'r':
    default:
      styleIndex = NS_FONT_STYLE_NORMAL;
      break;
    }
    nsFontStyle* style = charSet->mStyles[styleIndex];
    if (!style) {
      style = new nsFontStyle;
      if (!style) {
        continue;
      }
      charSet->mStyles[styleIndex] = style;
    }

    int weightNumber = (int) PL_HashTableLookup(gWeights, weightName);
    if (!weightNumber) {
#ifdef NOISY_FONTS
      printf("cannot find weight %s\n", weightName);
#endif
      weightNumber = NS_FONT_WEIGHT_NORMAL;
    }
    int weightIndex = WEIGHT_INDEX(weightNumber);
    nsFontWeight* weight = style->mWeights[weightIndex];
    if (!weight) {
      weight = new nsFontWeight;
      if (!weight) {
        continue;
      }
      style->mWeights[weightIndex] = weight;
    }
  
    int stretchIndex = (int) PL_HashTableLookup(gStretches, setWidth);
    if (!stretchIndex) {
#ifdef NOISY_FONTS
      printf("cannot find stretch %s\n", setWidth);
#endif
      stretchIndex = 5;
    }
    stretchIndex--;
    nsFontStretch* stretch = weight->mStretches[stretchIndex];
    if (!stretch) {
      stretch = new nsFontStretch;
      if (!stretch) {
        continue;
      }
      weight->mStretches[stretchIndex] = stretch;
    }
    if (scalable) {
      if (!stretch->mScalable) {
        stretch->mScalable = PR_smprintf("%s-%s-%s-%s-%s-%%d-*-*-*-%s-*-%s",
          name, weightName, slant, setWidth, addStyle, spacing, charSetName);
      }
      continue;
    }
  
    int pixels = atoi(pixelSize);
    if (stretch->mSizesCount) {
      nsFontGTK* end = &stretch->mSizes[stretch->mSizesCount];
      nsFontGTK* s;
      for (s = stretch->mSizes; s < end; s++) {
        if (s->mSize == pixels) {
          break;
        }
      }
      if (s != end) {
        continue;
      }
    }
    if (stretch->mSizesCount == stretch->mSizesAlloc) {
      int newSize = 2 * (stretch->mSizesAlloc ? stretch->mSizesAlloc : 1);
      nsFontGTK* newPointer = new nsFontGTK[newSize];
      if (newPointer) {
        for (int j = stretch->mSizesAlloc - 1; j >= 0; j--) {
          newPointer[j] = stretch->mSizes[j];
        }
        stretch->mSizesAlloc = newSize;
        delete [] stretch->mSizes;
        stretch->mSizes = newPointer;
      }
      else {
        continue;
      }
    }
    p = name;
    while (p < charSetName) {
      if (!*p) {
        *p = '-';
      }
      p++;
    }
    char* copy = strdup(name);
    if (!copy) {
      continue;
    }
    nsFontGTK* size = &stretch->mSizes[stretch->mSizesCount++];
    size->mName = copy;
    size->mFont = nsnull;
    size->mSize = pixels;
    size->mActualSize = 0;
    size->mBaselineAdjust = 0;
    size->mMap = nsnull;
    size->mCharSetInfo = charSetInfo;
  }
  XFreeFontNames(list);

#ifdef DEBUG_DUMP_TREE
  //DumpTree();
#endif

  return family;
}

/*
 * XListFonts(*) is expensive. Delay this till the last possible moment.
 * XListFontsWithInfo is expensive. Use XLoadQueryFont instead.
 */
nsFontGTK*
nsFontMetricsGTK::FindFont(PRUnichar aChar)
{
  static int gInitialized = 0;
  if (!gInitialized) {
    gInitialized = 1;
    gFamilies = PL_NewHashTable(0, HashKey, CompareKeys, NULL, NULL, NULL);
    gFamilyNames = PL_NewHashTable(0, HashKey, CompareKeys, NULL, NULL, NULL);
    nsFontFamilyName* f = gFamilyNameTable;
    while (f->mName) {
      nsString* name = new nsString(f->mName);
      nsString* xName = new nsString(f->mXName);
      if (name && xName) {
        PL_HashTableAdd(gFamilyNames, name, (void*) xName);
      }
      f++;
    }
    gWeights = PL_NewHashTable(0, PL_HashString, PL_CompareStrings, NULL, NULL,
      NULL);
    nsFontPropertyName* p = gWeightNames;
    while (p->mName) {
      PL_HashTableAdd(gWeights, p->mName, (void*) p->mValue);
      p++;
    }
    gStretches = PL_NewHashTable(0, PL_HashString, PL_CompareStrings, NULL,
      NULL, NULL);
    p = gStretchNames;
    while (p->mName) {
      PL_HashTableAdd(gStretches, p->mName, (void*) p->mValue);
      p++;
    }
    gCharSets = PL_NewHashTable(0, PL_HashString, PL_CompareStrings, NULL, NULL,
      NULL);
    nsFontCharSetMap* charSetMap = gCharSetMap;
    while (charSetMap->mName) {
      PL_HashTableAdd(gCharSets, charSetMap->mName, (void*) charSetMap->mInfo);
      charSetMap++;
    }
  }

  nsFontSearch search = { this, aChar, nsnull };

  while (mFontsIndex < mFontsCount) {
    nsString* familyName = &mFonts[mFontsIndex++];
    nsString* xName = (nsString*) PL_HashTableLookup(gFamilyNames, familyName);
    if (!xName) {
      xName = familyName;
    }
    nsFontFamily* family =
      (nsFontFamily*) PL_HashTableLookup(gFamilies, xName);
    if (!family) {
      char name[128];
      xName->ToCString(name, sizeof(name));
      char buf[256];
      PR_snprintf(buf, sizeof(buf), "-*-%s-*-*-*-*-*-*-*-*-*-*-*-*", name);
      family = GetFontNames(buf);
      if (!family) {
        family = new nsFontFamily; // dummy entry to avoid calling X again
        if (family) {
          nsString* copy = new nsString(*xName);
          if (copy) {
            PL_HashTableAdd(gFamilies, copy, family);
          }
          else {
            delete family;
          }
        }
        continue;
      }
    }
    TryFamily(&search, family);
    if (search.mFont) {
      return search.mFont;
    }
  }

  // XXX If we get to this point, that means that we have exhausted all the
  // families in the lists. Maybe we should try a list of fonts that are
  // specific to the vendor of the X server here. Because XListFonts for the
  // whole list is very expensive on some Unixes.

  static int gGotAllFontNames = 0;
  if (!gGotAllFontNames) {
    gGotAllFontNames = 1;
    GetFontNames("-*-*-*-*-*-*-*-*-*-*-*-*-*-*");
  }

  PL_HashTableEnumerateEntries(gFamilies, SearchFamily, &search);
  if (search.mFont) {
    // XXX We should probably write this family name out to disk, so that we
    // can use it next time. I.e. prefs file or something.
    return search.mFont;
  }

  // XXX Just return nsnull for now.
  // Need to draw boxes eventually. Or pop up dialog like plug-in dialog.
  return nsnull;
}

gint
nsFontMetricsGTK::GetWidth(nsFontGTK* aFont, const PRUnichar* aString,
  PRUint32 aLength)
{
  XChar2b buf[512];
  gint len = aFont->mCharSetInfo->Convert(aFont->mCharSetInfo, aString, aLength,
    (char*) buf, sizeof(buf));
  return gdk_text_width(aFont->mFont, (char*) buf, len);
}

void
nsFontMetricsGTK::DrawString(nsDrawingSurfaceGTK* aSurface, nsFontGTK* aFont,
  nscoord aX, nscoord aY, const PRUnichar* aString, PRUint32 aLength)
{
  XChar2b buf[512];
  gint len = aFont->mCharSetInfo->Convert(aFont->mCharSetInfo, aString, aLength,
    (char*) buf, sizeof(buf));
  ::gdk_draw_text(aSurface->GetDrawable(), aFont->mFont, aSurface->GetGC(), aX,
    aY + aFont->mBaselineAdjust, (char*) buf, len);
}

nsresult
nsFontMetricsGTK::GetSpaceWidth(nscoord &aSpaceWidth)
{
  aSpaceWidth = mSpaceWidth;
  return NS_OK;
}

#endif /* FONT_SWITCHING */
