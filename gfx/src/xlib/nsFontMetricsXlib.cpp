/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim:ts=2:et:sw=2:
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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   Peter Hartshorn <peter@igelaus.com.au>
 *   Quy Tonthat <quy@igelaus.com.au>
 *   Tony Tsui <tony@igelaus.com.au>
 *   Tim Copperfield <timecop@network.email.ne.jp>
 *   Roland Mainz <roland.mainz@informatik.med.uni-giessen.de>
 */

#include "xp_core.h"
#include "nscore.h"
#include "nsQuickSort.h"
#include "nsFontMetricsXlib.h"
#include "nsIServiceManager.h"
#include "nsICharsetConverterManager.h"
#include "nsICharsetConverterManager2.h"
#include "nsILanguageAtomService.h"
#include "nsISaveAsCharset.h"
#include "nsIPref.h"
#include "nsCOMPtr.h"
#include "nspr.h"
#include "nsHashtable.h"
#include "nsReadableUtils.h"
#include "nsAWritableString.h"
#include "nsXPIDLString.h"

#include <stdlib.h>
#include <X11/Xatom.h>
#include "xlibrgb.h"

/* #define NOISY_FONTS 1 */

#ifdef USE_XPRINT 
/* enable hack "fix" for bug 88554 ("Xprint module should avoid using GFX
 * fonts unless there is no other option...") until bug 93771 ("Mozilla
 * uses low-resolution bitmap fonts on high resolution X11 displays") get's
 * fixed. */
#define XPRINT_FONT_HACK 1
#endif /* USE_XPRINT */

#ifdef PR_LOGGING 
static PRLogModuleInfo * FontMetricsXlibLM = PR_NewLogModule("FontMetricsXlib");
#endif /* PR_LOGGING */

#define UCS2_NOMAPPING 0XFFFD

#undef  USER_DEFINED
#define USER_DEFINED "x-user-def"

/* Prototype some structs */
struct nsFontCharSetMapXlib;
struct nsFontFamilyNameXlib;
struct nsFontPropertyNameXlib;
struct nsFontStyleXlib;
struct nsFontWeightXlib;
struct nsFontLangGroup;

class nsFontNodeArrayXlib : public nsAutoVoidArray
{
public:
  nsFontNodeArrayXlib() {};
  
  nsFontNodeXlib* GetElement(PRInt32 aIndex)
  {
    return (nsFontNodeXlib*) ElementAt(aIndex);
  };
};

struct nsFontCharSetXlibInfo
{
  const char*            mCharSet;
  nsFontCharSetXlibConverter Convert;
  PRUint8                mSpecialUnderline;
  PRUint16*              mCCMap;
  nsIUnicodeEncoder*     mConverter;
  nsIAtom*               mLangGroup;
  PRBool                 mInitedSizeInfo;
  PRInt32                mOutlineScaleMin;
  PRInt32                mBitmapScaleMin;
  double                 mBitmapOversize;
  double                 mBitmapUndersize;
};

struct nsFontCharSetMapXlib
{
  const char*            mName;
  nsFontLangGroup*       mFontLangGroup;
  nsFontCharSetXlibInfo* mInfo;
};

struct nsFontFamilyXlib
{
  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  nsFontNodeArrayXlib mNodes;
};

struct nsFontFamilyNameXlib
{
  const char* mName;
  const char* mXName;
};

struct nsFontNodeXlib
{
  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  void FillStyleHoles(void);

  nsCAutoString          mName;
  nsFontCharSetXlibInfo* mCharSetInfo;
  nsFontStyleXlib*       mStyles[3];
  PRUint8                mHolesFilled;
  PRUint8                mDummy;
};

struct nsFontPropertyNameXlib
{
  const char *mName;
  int         mValue;
};

struct nsFontStretchXlib
{
  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  void SortSizes(void);

  nsFontXlib** mSizes;
  PRUint16     mSizesAlloc;
  PRUint16     mSizesCount;

  char*        mScalable;
  PRBool       mOutlineScaled;
  nsVoidArray  mScaledFonts;
};

struct nsFontStyleXlib
{
  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  void FillWeightHoles(void);

  nsFontWeightXlib* mWeights[9];
};

struct nsFontWeightXlib
{
  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  void FillStretchHoles(void);

  nsFontStretchXlib* mStretches[9];
};

static NS_DEFINE_CID(kCharSetManagerCID, NS_ICHARSETCONVERTERMANAGER_CID);
static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);
static NS_DEFINE_CID(kSaveAsCharsetCID, NS_SAVEASCHARSET_CID);

static int gFontMetricsXlibCount = 0;
static int gInitialized = 0;
static XlibRgbHandle *gXlibRgbHandle = nsnull;
static PRBool gAllowDoubleByteSpecialChars = PR_TRUE;

// XXX many of these statics need to be freed at shutdown time
static nsIPref* gPref = nsnull;
static nsICharsetConverterManager2* gCharSetManager = nsnull;
static nsIUnicodeEncoder* gUserDefinedConverter = nsnull;

static nsHashtable* gAliases = nsnull;
static nsHashtable* gCharSetMaps = nsnull;
static nsHashtable* gFamilies = nsnull;
static nsHashtable* gNodes = nsnull;
/* gCachedFFRESearches holds the "already looked up"
   FFRE (Foundry Family Registry Encoding) font searches */
static nsHashtable* gCachedFFRESearches = nsnull;
static nsHashtable* gSpecialCharSets = nsnull;
static nsHashtable* gStretches = nsnull;
static nsHashtable* gWeights = nsnull;
static nsISaveAsCharset* gFontSubConverter = nsnull;

static nsFontNodeArrayXlib* gGlobalList = nsnull;

static nsIAtom* gUnicode       = nsnull;
static nsIAtom* gUserDefined   = nsnull;
static nsIAtom* gUsersLocale   = nsnull;
static nsIAtom* gWesternLocale = nsnull;

static PRInt32 gOutlineScaleMinimum =  6;
static PRInt32 gBitmapScaleMinimum  = 10;
static double  gBitmapOversize      =  1.1;
static double  gBitmapUndersize     =  0.9;

static int SingleByteConvert(nsFontCharSetXlibInfo* aSelf, XFontStruct* aFont,
                             const PRUnichar* aSrcBuf, PRInt32 aSrcLen,
                             char* aDestBuf, PRInt32 aDestLen);
static int DoubleByteConvert(nsFontCharSetXlibInfo* aSelf, XFontStruct* aFont,
                             const PRUnichar* aSrcBuf, PRInt32 aSrcLen,
                             char* aDestBuf, PRInt32 aDestLen);
static int ISO10646Convert(nsFontCharSetXlibInfo* aSelf, XFontStruct* aFont,
                           const PRUnichar* aSrcBuf, PRInt32 aSrcLen,
                           char* aDestBuf, PRInt32 aDestLen);

static nsFontCharSetXlibInfo Unknown = { nsnull };
static nsFontCharSetXlibInfo Special = { nsnull };

static nsFontCharSetXlibInfo CP1251 =
  { "windows-1251", SingleByteConvert, 0 };
static nsFontCharSetXlibInfo ISO88591 =
  { "ISO-8859-1", SingleByteConvert, 0 };
static nsFontCharSetXlibInfo ISO88592 =
  { "ISO-8859-2", SingleByteConvert, 0 };
static nsFontCharSetXlibInfo ISO88593 =
  { "ISO-8859-3", SingleByteConvert, 0 };
static nsFontCharSetXlibInfo ISO88594 =
  { "ISO-8859-4", SingleByteConvert, 0 };
static nsFontCharSetXlibInfo ISO88595 =
  { "ISO-8859-5", SingleByteConvert, 0 };
static nsFontCharSetXlibInfo ISO88596 =
  { "ISO-8859-6", SingleByteConvert, 0 };
static nsFontCharSetXlibInfo ISO88597 =
  { "ISO-8859-7", SingleByteConvert, 0 };
static nsFontCharSetXlibInfo ISO88598 =
  { "ISO-8859-8", SingleByteConvert, 0 };
// change from  
// { "ISO-8859-8", SingleByteConvertReverse, 0 };
// untill we fix the layout and ensure we only call this with pure RTL text
static nsFontCharSetXlibInfo ISO88599 =
  { "ISO-8859-9", SingleByteConvert, 0 };
static nsFontCharSetXlibInfo ISO885913 =
  { "ISO-8859-13", SingleByteConvert, 0 };
static nsFontCharSetXlibInfo ISO885915 =
  { "ISO-8859-15", SingleByteConvert, 0 };
static nsFontCharSetXlibInfo JISX0201 =
  { "jis_0201", SingleByteConvert, 1 };
static nsFontCharSetXlibInfo KOI8R =
  { "KOI8-R", SingleByteConvert, 0 };
static nsFontCharSetXlibInfo KOI8U =
  { "KOI8-U", SingleByteConvert, 0 };
static nsFontCharSetXlibInfo TIS620 =
  { "TIS-620", SingleByteConvert, 0 };

static nsFontCharSetXlibInfo Big5 =
  { "x-x-big5", DoubleByteConvert, 1 };
static nsFontCharSetXlibInfo CNS116431 =
  { "x-cns-11643-1", DoubleByteConvert, 1 };
static nsFontCharSetXlibInfo CNS116432 =
  { "x-cns-11643-2", DoubleByteConvert, 1 };
static nsFontCharSetXlibInfo CNS116433 =
  { "x-cns-11643-3", DoubleByteConvert, 1 };
static nsFontCharSetXlibInfo CNS116434 =
  { "x-cns-11643-4", DoubleByteConvert, 1 };
static nsFontCharSetXlibInfo CNS116435 =
  { "x-cns-11643-5", DoubleByteConvert, 1 };
static nsFontCharSetXlibInfo CNS116436 =
  { "x-cns-11643-6", DoubleByteConvert, 1 };
static nsFontCharSetXlibInfo CNS116437 =
  { "x-cns-11643-7", DoubleByteConvert, 1 };
static nsFontCharSetXlibInfo GB2312 =
  { "gb_2312-80", DoubleByteConvert, 1 };
static nsFontCharSetXlibInfo GB18030_0 =
  { "gb18030.2000-0", DoubleByteConvert, 1 };
static nsFontCharSetXlibInfo GB18030_1 =
  { "gb18030.2000-1", DoubleByteConvert, 1 };
static nsFontCharSetXlibInfo GBK =
  { "x-gbk-noascii", DoubleByteConvert, 1};
static nsFontCharSetXlibInfo HKSCS =
  { "hkscs-1", DoubleByteConvert, 1 };
static nsFontCharSetXlibInfo JISX0208 =
  { "jis_0208-1983", DoubleByteConvert, 1 };
static nsFontCharSetXlibInfo JISX0212 =
  { "jis_0212-1990", DoubleByteConvert, 1 };
static nsFontCharSetXlibInfo KSC5601 =
  { "ks_c_5601-1987", DoubleByteConvert, 1 };
static nsFontCharSetXlibInfo X11Johab =
  { "x-x11johab", DoubleByteConvert, 1 };
static nsFontCharSetXlibInfo JohabNoAscii =
  { "x-johab-noascii", DoubleByteConvert, 1 };

static nsFontCharSetXlibInfo ISO106461 =
  { nsnull, ISO10646Convert, 1 };

static nsFontCharSetXlibInfo AdobeSymbol =
   { "Adobe-Symbol-Encoding", SingleByteConvert, 0 };

#ifdef MOZ_MATHML
static nsFontCharSetXlibInfo CMCMEX =
   { "x-t1-cmex", SingleByteConvert, 0 };
static nsFontCharSetXlibInfo CMCMSY =
   { "x-t1-cmsy", SingleByteConvert, 0 };
static nsFontCharSetXlibInfo CMCMR =
   { "x-t1-cmr", SingleByteConvert, 0 };
static nsFontCharSetXlibInfo CMCMMI =
   { "x-t1-cmmi", SingleByteConvert, 0 };
static nsFontCharSetXlibInfo Mathematica1 =
   { "x-mathematica1", SingleByteConvert, 0 };
static nsFontCharSetXlibInfo Mathematica2 =
   { "x-mathematica2", SingleByteConvert, 0 };
static nsFontCharSetXlibInfo Mathematica3 =
   { "x-mathematica3", SingleByteConvert, 0 };
static nsFontCharSetXlibInfo Mathematica4 =
   { "x-mathematica4", SingleByteConvert, 0 };
static nsFontCharSetXlibInfo Mathematica5 =
   { "x-mathematica5", SingleByteConvert, 0 };
#endif /* MOZ_MATHML */

/*
 * Font Language Groups
 *
 * These Font Language Groups (FLG) indicate other related
 * encodings to look at when searching for glyphs 
 *
 */
typedef struct nsFontLangGroup {
  const char *mFontLangGroupName;
  nsIAtom*    mFontLangGroupAtom;
} nsFontLangGroup;

static nsFontLangGroup FLG_WESTERN = { "x-western", nsnull };
static nsFontLangGroup FLG_ZHCN =    { "zh-CN", nsnull };
static nsFontLangGroup FLG_ZHTW =    { "zh-TW", nsnull };
static nsFontLangGroup FLG_JA =      { "ja", nsnull };
static nsFontLangGroup FLG_KO =      { "ko", nsnull };
static nsFontLangGroup FLG_NONE =    { nsnull , nsnull };

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
static nsFontCharSetMapXlib gCharSetMap[] =
{
  { "-ascii",             &FLG_NONE,    &Unknown       },
  { "-ibm pc",            &FLG_NONE,    &Unknown       },
  { "adobe-fontspecific", &FLG_NONE,    &Special       },
  { "big5-0",             &FLG_ZHTW,    &Big5          },
  { "big5-1",             &FLG_ZHTW,    &Big5          },
  { "big5.et-0",          &FLG_ZHTW,    &Big5          },
  { "big5.et.ext-0",      &FLG_ZHTW,    &Big5          },
  { "big5.etext-0",       &FLG_ZHTW,    &Big5          },
  { "big5.hku-0",         &FLG_ZHTW,    &Big5          },
  { "big5.hku-1",         &FLG_ZHTW,    &Big5          },
  { "big5.pc-0",          &FLG_ZHTW,    &Big5          },
  { "big5.shift-0",       &FLG_ZHTW,    &Big5          },
  { "cns11643.1986-1",    &FLG_ZHTW,    &CNS116431     },
  { "cns11643.1986-2",    &FLG_ZHTW,    &CNS116432     },
  { "cns11643.1992-1",    &FLG_ZHTW,    &CNS116431     },
  { "cns11643.1992.1-0",  &FLG_ZHTW,    &CNS116431     },
  { "cns11643.1992-12",   &FLG_NONE,    &Unknown       },
  { "cns11643.1992.2-0",  &FLG_ZHTW,    &CNS116432     },
  { "cns11643.1992-2",    &FLG_ZHTW,    &CNS116432     },
  { "cns11643.1992-3",    &FLG_ZHTW,    &CNS116433     },
  { "cns11643.1992.3-0",  &FLG_ZHTW,    &CNS116433     },
  { "cns11643.1992.4-0",  &FLG_ZHTW,    &CNS116434     },
  { "cns11643.1992-4",    &FLG_ZHTW,    &CNS116434     },
  { "cns11643.1992.5-0",  &FLG_ZHTW,    &CNS116435     },
  { "cns11643.1992-5",    &FLG_ZHTW,    &CNS116435     },
  { "cns11643.1992.6-0",  &FLG_ZHTW,    &CNS116436     },
  { "cns11643.1992-6",    &FLG_ZHTW,    &CNS116436     },
  { "cns11643.1992.7-0",  &FLG_ZHTW,    &CNS116437     },
  { "cns11643.1992-7",    &FLG_ZHTW,    &CNS116437     },
  { "cns11643-1",         &FLG_ZHTW,    &CNS116431     },
  { "cns11643-2",         &FLG_ZHTW,    &CNS116432     },
  { "cns11643-3",         &FLG_ZHTW,    &CNS116433     },
  { "cns11643-4",         &FLG_ZHTW,    &CNS116434     },
  { "cns11643-5",         &FLG_ZHTW,    &CNS116435     },
  { "cns11643-6",         &FLG_ZHTW,    &CNS116436     },
  { "cns11643-7",         &FLG_ZHTW,    &CNS116437     },
  { "cp1251-1",           &FLG_NONE,    &CP1251        },
  { "dec-dectech",        &FLG_NONE,    &Unknown       },
  { "dtsymbol-1",         &FLG_NONE,    &Unknown       },
  { "fontspecific-0",     &FLG_NONE,    &Unknown       },
  { "gb2312.1980-0",      &FLG_ZHCN,    &GB2312        },
  { "gb2312.1980-1",      &FLG_ZHCN,    &GB2312        },
  { "gb13000.1993-1",     &FLG_ZHCN,    &GBK           },
  { "gb18030.2000-0",     &FLG_ZHCN,    &GB18030_0     },
  { "gb18030.2000-1",     &FLG_ZHCN,    &GB18030_1     },
  { "gbk-0",              &FLG_ZHCN,    &GBK           },
  { "gbk1988.1989-0",     &FLG_ZHCN,    &GBK           },
  { "hkscs-1",            &FLG_ZHTW,    &HKSCS         },
  { "hp-japanese15",      &FLG_NONE,    &Unknown       },
  { "hp-japaneseeuc",     &FLG_NONE,    &Unknown       },
  { "hp-roman8",          &FLG_NONE,    &Unknown       },
  { "hp-schinese15",      &FLG_NONE,    &Unknown       },
  { "hp-tchinese15",      &FLG_NONE,    &Unknown       },
  { "hp-tchinesebig5",    &FLG_ZHTW,    &Big5          },
  { "hp-wa",              &FLG_NONE,    &Unknown       },
  { "hpbig5-",            &FLG_ZHTW,    &Big5          },
  { "hproc16-",           &FLG_NONE,    &Unknown       },
  { "ibm-1252",           &FLG_NONE,    &Unknown       },
  { "ibm-850",            &FLG_NONE,    &Unknown       },
  { "ibm-fontspecific",   &FLG_NONE,    &Unknown       },
  { "ibm-sbdcn",          &FLG_NONE,    &Unknown       },
  { "ibm-sbdtw",          &FLG_NONE,    &Unknown       },
  { "ibm-special",        &FLG_NONE,    &Unknown       },
  { "ibm-udccn",          &FLG_NONE,    &Unknown       },
  { "ibm-udcjp",          &FLG_NONE,    &Unknown       },
  { "ibm-udctw",          &FLG_NONE,    &Unknown       },
  { "iso646.1991-irv",    &FLG_NONE,    &Unknown       },
  { "iso8859-1",          &FLG_WESTERN, &ISO88591      },
  { "iso8859-13",         &FLG_WESTERN, &ISO885913     },
  { "iso8859-15",         &FLG_WESTERN, &ISO885915     },
  { "iso8859-1@cn",       &FLG_NONE,    &Unknown       },
  { "iso8859-1@kr",       &FLG_NONE,    &Unknown       },
  { "iso8859-1@tw",       &FLG_NONE,    &Unknown       },
  { "iso8859-1@zh",       &FLG_NONE,    &Unknown       },
  { "iso8859-2",          &FLG_WESTERN, &ISO88592      },
  { "iso8859-3",          &FLG_WESTERN, &ISO88593      },
  { "iso8859-4",          &FLG_WESTERN, &ISO88594      },
  { "iso8859-5",          &FLG_NONE,    &ISO88595      },
  { "iso8859-6",          &FLG_NONE,    &ISO88596      },
  { "iso8859-7",          &FLG_WESTERN, &ISO88597      },
  { "iso8859-8",          &FLG_NONE,    &ISO88598      },
  { "iso8859-9",          &FLG_WESTERN, &ISO88599      },
  { "iso10646-1",         &FLG_NONE,    &ISO106461     },
  { "jisx0201.1976-0",    &FLG_JA,      &JISX0201      },
  { "jisx0201.1976-1",    &FLG_JA,      &JISX0201      },
  { "jisx0208.1983-0",    &FLG_JA,      &JISX0208      },
  { "jisx0208.1990-0",    &FLG_JA,      &JISX0208      },
  { "jisx0212.1990-0",    &FLG_JA,      &JISX0212      },
  { "koi8-r",             &FLG_NONE,    &KOI8R         },
  { "koi8-u",             &FLG_NONE,    &KOI8U         },
  { "johab-1",            &FLG_KO,      &X11Johab      },
  { "johabs-1",           &FLG_KO,      &X11Johab      },
  { "johabsh-1",          &FLG_KO,      &X11Johab      },
  { "ksc5601.1987-0",     &FLG_KO,      &KSC5601       },
  { "ksc5601.1992-3",     &FLG_KO,      &JohabNoAscii  },
  { "microsoft-cp1251",   &FLG_NONE,    &CP1251        },
  { "misc-fontspecific",  &FLG_NONE,    &Unknown       },
  { "sgi-fontspecific",   &FLG_NONE,    &Unknown       },
  { "sun-fontspecific",   &FLG_NONE,    &Unknown       },
  { "sunolcursor-1",      &FLG_NONE,    &Unknown       },
  { "sunolglyph-1",       &FLG_NONE,    &Unknown       },
  { "tis620.2529-1",      &FLG_NONE,    &TIS620        },
  { "tis620.2533-0",      &FLG_NONE,    &TIS620        },
  { "tis620.2533-1",      &FLG_NONE,    &TIS620        },
  { "tis620-0",           &FLG_NONE,    &TIS620        },
  { "iso8859-11",         &FLG_NONE,    &TIS620        },
  { "ucs2.cjk-0",         &FLG_NONE,    &Unknown       },
  { "ucs2.cjk_japan-0",   &FLG_NONE,    &Unknown       },
  { "ucs2.cjk_taiwan-0",  &FLG_NONE,    &Unknown       },

  { nsnull,               nsnull,       nsnull         }
};

static nsFontFamilyNameXlib gFamilyNameTable[] =
{
  { "arial",           "helvetica" },
  { "courier new",     "courier" },
  { "times new roman", "times" },

#ifdef MOZ_MATHML
  { "cmex",             "cmex10" },
  { "cmsy",             "cmsy10" },
  { "-moz-math-text",   "times" },
  { "-moz-math-symbol", "symbol" },
#endif /* MOZ_MATHML */

  { nsnull, nsnull }
};

static nsFontCharSetMapXlib gNoneCharSetMap[] = { { nsnull }, };

static nsFontCharSetMapXlib gSpecialCharSetMap[] =
{
  { "symbol-adobe-fontspecific", &FLG_NONE, &AdobeSymbol },

#ifdef MOZ_MATHML
  { "cmex10-adobe-fontspecific", &FLG_NONE, &CMCMEX },
  { "cmsy10-adobe-fontspecific", &FLG_NONE, &CMCMSY },
  { "cmr10-adobe-fontspecific",  &FLG_NONE, &CMCMR },
  { "cmmi10-adobe-fontspecific", &FLG_NONE, &CMCMMI },

  { "math1-adobe-fontspecific", &FLG_NONE, &Mathematica1 },
  { "math2-adobe-fontspecific", &FLG_NONE, &Mathematica2 },
  { "math3-adobe-fontspecific", &FLG_NONE, &Mathematica3 },
  { "math4-adobe-fontspecific", &FLG_NONE, &Mathematica4 },
  { "math5-adobe-fontspecific", &FLG_NONE, &Mathematica5 },
 
  { "math1mono-adobe-fontspecific", &FLG_NONE, &Mathematica1 },
  { "math2mono-adobe-fontspecific", &FLG_NONE, &Mathematica2 },
  { "math3mono-adobe-fontspecific", &FLG_NONE, &Mathematica3 },
  { "math4mono-adobe-fontspecific", &FLG_NONE, &Mathematica4 },
  { "math5mono-adobe-fontspecific", &FLG_NONE, &Mathematica5 },
#endif /* MOZ_MATHML */

  { nsnull, nsnull }
};

static nsFontPropertyNameXlib gStretchNames[] =
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

static nsFontPropertyNameXlib gWeightNames[] =
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

static PRUint16* gUserDefinedCCMap = nsnull;
static PRUint16* gEmptyCCMap = nsnull;
static PRUint16* gDoubleByteSpecialCharsCCMap = nsnull;

//
// smart quotes (and other special chars) in Asian (double byte)
// fonts are too large to use is western fonts.
// Here we define those characters.
//
static PRUnichar gDoubleByteSpecialChars[] = {
  0x0152, 0x0153, 0x0160, 0x0161, 0x0178, 0x017D, 0x017E, 0x0192,
  0x02C6, 0x02DC, 0x2013, 0x2014, 0x2018, 0x2019, 0x201A, 0x201C,
  0x201D, 0x201E, 0x2020, 0x2021, 0x2022, 0x2026, 0x2030, 0x2039,
  0x203A, 0x20AC, 0x2122,
  0
};


static PRBool
FreeCharSetMap(nsHashKey* aKey, void* aData, void* aClosure)
{
  nsFontCharSetMapXlib* charsetMap = (nsFontCharSetMapXlib*) aData;
  NS_IF_RELEASE(charsetMap->mInfo->mConverter);
  NS_IF_RELEASE(charsetMap->mInfo->mLangGroup);
  FreeCCMap(charsetMap->mInfo->mCCMap);

  return PR_TRUE;
}

static PRBool
FreeFamily(nsHashKey* aKey, void* aData, void* aClosure)
{
  delete (nsFontFamilyXlib*) aData;

  return PR_TRUE;
}

static void
FreeStretch(nsFontStretchXlib* aStretch)
{
  PR_smprintf_free(aStretch->mScalable);

  PRInt32 count;
  while ((count = aStretch->mScaledFonts.Count())) {
    // go backwards to keep nsVoidArray from memmoving everything each time
    count--; // nsVoidArray is zero based
    nsFontXlib *font = (nsFontXlib*)aStretch->mScaledFonts.ElementAt(count);
    aStretch->mScaledFonts.RemoveElementAt(count);
    if (font) delete font;
  }

  for (int i = 0; i < aStretch->mSizesCount; i++) {
    delete aStretch->mSizes[i];
  }
  delete [] aStretch->mSizes;
  delete aStretch;
}

static void
FreeWeight(nsFontWeightXlib* aWeight)
{
  for (int i = 0; i < 9; i++) {
    if (aWeight->mStretches[i]) {
      for (int j = i + 1; j < 9; j++) {
        if (aWeight->mStretches[j] == aWeight->mStretches[i]) {
          aWeight->mStretches[j] = nsnull;
        }
      }
      FreeStretch(aWeight->mStretches[i]);
    }
  }
  delete aWeight;
}

static void
FreeStyle(nsFontStyleXlib* aStyle)
{
  for (int i = 0; i < 9; i++) {
    if (aStyle->mWeights[i]) {
      for (int j = i + 1; j < 9; j++) {
        if (aStyle->mWeights[j] == aStyle->mWeights[i]) {
          aStyle->mWeights[j] = nsnull;
        }
      }
      FreeWeight(aStyle->mWeights[i]);
    }
  }
  delete aStyle;
}

static PRBool
FreeNode(nsHashKey* aKey, void* aData, void* aClosure)
{
  nsFontNodeXlib* node = (nsFontNodeXlib*) aData;
  for (int i = 0; i < 3; i++) {
    if (node->mStyles[i]) {
      for (int j = i + 1; j < 3; j++) {
        if (node->mStyles[j] == node->mStyles[i]) {
          node->mStyles[j] = nsnull;
        }
      }
      FreeStyle(node->mStyles[i]);
    }
  }
  delete node;

  return PR_TRUE;
}

static PRBool
FreeNodeArray(nsHashKey* aKey, void* aData, void* aClosure)
{
  nsFontNodeArrayXlib* nodes = (nsFontNodeArrayXlib*) aData;
  delete nodes;

  return PR_TRUE;
}

void
nsFontMetricsXlib::FreeGlobals(void)
{
  gInitialized = 0;
  
  if (gAliases) {
    delete gAliases;
    gAliases = nsnull;
  }

  NS_IF_RELEASE(gCharSetManager);
  gCharSetManager = nsnull;

  if (gCharSetMaps) {
    gCharSetMaps->Reset(FreeCharSetMap, nsnull);
    delete gCharSetMaps;
    gCharSetMaps = nsnull;
  }

  if (gFamilies) {
    gFamilies->Reset(FreeFamily, nsnull);
    delete gFamilies;
    gFamilies = nsnull;
  }

  if (gGlobalList) {
    delete gGlobalList;
    gGlobalList = nsnull;
  }

  if (gCachedFFRESearches) {
    gCachedFFRESearches->Reset(FreeNodeArray, nsnull);
    delete gCachedFFRESearches;
    gCachedFFRESearches = nsnull;
  }

  if (gNodes) {
    gNodes->Reset(FreeNode, nsnull);
    delete gNodes;
    gNodes = nsnull;
  }

  NS_IF_RELEASE(gPref);
  gPref = nsnull;
  
  if (gSpecialCharSets) {
    delete gSpecialCharSets;
    gSpecialCharSets = nsnull;
  }

  if (gStretches) {
    delete gStretches;
    gStretches = nsnull;
  }

  NS_IF_RELEASE(gUnicode);              gUnicode = nsnull;
  NS_IF_RELEASE(gUserDefined);          gUserDefined = nsnull;
  NS_IF_RELEASE(gUserDefinedConverter); gUserDefinedConverter = nsnull;
  NS_IF_RELEASE(gUsersLocale);          gUsersLocale = nsnull;
  NS_IF_RELEASE(gWesternLocale);        gWesternLocale = nsnull;
  NS_IF_RELEASE(gFontSubConverter);     gFontSubConverter = nsnull;

  if (gWeights) {
    delete gWeights;
    gWeights = nsnull;
  }

  nsFontCharSetMapXlib* charSetMap;
  for (charSetMap = gCharSetMap; charSetMap->mFontLangGroup; charSetMap++) {
    NS_IF_RELEASE(charSetMap->mFontLangGroup->mFontLangGroupAtom);
    charSetMap->mFontLangGroup->mFontLangGroupAtom = nsnull;
  }
  FreeCCMap(gUserDefinedCCMap);
  FreeCCMap(gEmptyCCMap);
  FreeCCMap(gDoubleByteSpecialCharsCCMap);
  
  gXlibRgbHandle = nsnull;
}

/*
 * Initialize all the font lookup hash tables and other globals
 */
nsresult
nsFontMetricsXlib::InitGlobals(nsIDeviceContext *aDevice)
{
  nsresult rv = NS_OK;

  NS_ASSERTION(aDevice!=nsnull, "calling InitGlobals() without a device"); 
#ifdef USE_XPRINT 
  if (mPrinterMode)
    gXlibRgbHandle =  NS_STATIC_CAST(nsDeviceContextXp   *,aDevice)->GetXlibRgbHandle();
  else  
#endif /* USE_XPRINT */  
    gXlibRgbHandle =  NS_STATIC_CAST(nsDeviceContextXlib *,aDevice)->GetXlibRgbHandle();

  nsServiceManager::GetService(kCharSetManagerCID,
                               NS_GET_IID(nsICharsetConverterManager2),
                               (nsISupports**) &gCharSetManager);
  if (!gCharSetManager) {
    FreeGlobals();
    return NS_ERROR_FAILURE;
  }

  nsServiceManager::GetService(kPrefCID, NS_GET_IID(nsIPref),
                               (nsISupports**) &gPref);
  if (!gPref) {
    FreeGlobals();
    return NS_ERROR_FAILURE;
  }

  nsCompressedCharMap empty_ccmapObj;
  gEmptyCCMap = empty_ccmapObj.NewCCMap();
  if (!gEmptyCCMap)
    return NS_ERROR_OUT_OF_MEMORY;

  // get the "disable double byte font special chars" setting
  PRBool val = PR_TRUE;
  rv = gPref->GetBoolPref("font.allow_double_byte_special_chars", &val);
  if (NS_SUCCEEDED(rv))
    gAllowDoubleByteSpecialChars = val;

  // setup the double byte font special chars glyph map
  nsCompressedCharMap specialchars_ccmapObj;
  for (int i=0; gDoubleByteSpecialChars[i]; i++) {
    specialchars_ccmapObj.SetChar(gDoubleByteSpecialChars[i]);
  }
  gDoubleByteSpecialCharsCCMap = specialchars_ccmapObj.NewCCMap();
  if (!gDoubleByteSpecialCharsCCMap)
    return NS_ERROR_OUT_OF_MEMORY;    

  PRInt32 scale_minimum = 0;
  rv = gPref->GetIntPref("font.scale.outline.min", &scale_minimum);
  if (NS_SUCCEEDED(rv))
    gOutlineScaleMinimum = scale_minimum;
    
  rv = gPref->GetIntPref("font.scale.bitmap.min", &scale_minimum);
  if (NS_SUCCEEDED(rv))
    gBitmapScaleMinimum = scale_minimum;

  PRInt32 percent = 0;
  gPref->GetIntPref("font.scale.bitmap.oversize", &percent);
  if (percent)
    gBitmapOversize = percent / 100.0;
  
  percent = 0;
  gPref->GetIntPref("font.scale.bitmap.undersize", &percent);
  if (percent)
    gBitmapUndersize = percent / 100.0;

  gNodes = new nsHashtable();
  if (!gNodes) {
    FreeGlobals();
    return NS_ERROR_OUT_OF_MEMORY;
  }

  gCachedFFRESearches = new nsHashtable();
  if (!gCachedFFRESearches) {
    FreeGlobals();
    return NS_ERROR_OUT_OF_MEMORY;
  }

  gFamilies = new nsHashtable();
  if (!gFamilies) {
    FreeGlobals();
    return NS_ERROR_OUT_OF_MEMORY;
  }

  gAliases = new nsHashtable();
  if (!gAliases) {
    FreeGlobals();
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsFontFamilyNameXlib* f = gFamilyNameTable;
  while (f->mName) {
    nsCStringKey key(f->mName);
    gAliases->Put(&key, (void *)f->mXName);
    f++;
  }

  gWeights = new nsHashtable();
  if (!gWeights) {
    FreeGlobals();
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsFontPropertyNameXlib* p = gWeightNames;
  while (p->mName) {
    nsCStringKey key(p->mName);
    gWeights->Put(&key, (void*) p->mValue);
    p++;
  }

  gStretches = new nsHashtable();
  if (!gStretches) {
    FreeGlobals();
    return NS_ERROR_OUT_OF_MEMORY;
  }
  p = gStretchNames;
  while (p->mName) {
    nsCStringKey key(p->mName);
    gStretches->Put(&key, (void*) p->mValue);
    p++;
  }

  gCharSetMaps = new nsHashtable();
  if (!gCharSetMaps) {
    FreeGlobals();
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsFontCharSetMapXlib* charSetMap = gCharSetMap;
  while (charSetMap->mName) {
    nsCStringKey key(charSetMap->mName);
    gCharSetMaps->Put(&key, charSetMap);
    charSetMap++;
  }

  gSpecialCharSets = new nsHashtable();
  if (!gSpecialCharSets) {
    FreeGlobals();
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsFontCharSetMapXlib* specialCharSetMap = gSpecialCharSetMap;
  while (specialCharSetMap->mName) {
    nsCStringKey key(specialCharSetMap->mName);
    gSpecialCharSets->Put(&key, specialCharSetMap);
    specialCharSetMap++;
  }

  gUnicode = NS_NewAtom("x-unicode");
  if (!gUnicode) {
    FreeGlobals();
    return NS_ERROR_OUT_OF_MEMORY;
  }

  gUserDefined = NS_NewAtom(USER_DEFINED);
  if (!gUserDefined) {
    FreeGlobals();
    return NS_ERROR_OUT_OF_MEMORY;
  }

  /* get the user's locale */
  nsCOMPtr <nsILanguageAtomService> langService;
  langService = do_GetService(NS_LANGUAGEATOMSERVICE_CONTRACTID);
  if (langService)
    langService->GetLocaleLanguageGroup(&gUsersLocale);
  if (!gUsersLocale)
    gUsersLocale = NS_NewAtom("x-western");

  gWesternLocale = NS_NewAtom("x-western");

  if (!gUsersLocale) {
    FreeGlobals();
    return NS_ERROR_OUT_OF_MEMORY;
  }

  if (!gWesternLocale) {
    FreeGlobals();
    return NS_ERROR_OUT_OF_MEMORY;
  }

  gInitialized = 1;
  
  return NS_OK;
}

// nsFontMetricsXlib Implementation
nsFontMetricsXlib::nsFontMetricsXlib()
  : mFonts() // I'm not sure what the common size is here - I generally
  // see 2-5 entries.  For now, punt and let it be allocated later.  We can't
  // make it an nsAutoVoidArray since it's a cString array.
  // XXX mFontIsGeneric will generally need to be the same size; right now
  // it's an nsAutoVoidArray.  If the average is under 8, that's ok.
{
  NS_INIT_REFCNT();
  gFontMetricsXlibCount++;
}

nsFontMetricsXlib::~nsFontMetricsXlib()
{
  // do not free mGeneric here since it's a pointer into the mFonts list

  if (nsnull != mFont) {
    delete mFont;
    mFont = nsnull;
  }

  if (mLoadedFonts) {
    PR_Free(mLoadedFonts);
    mLoadedFonts = nsnull;
  }

  if (mSubstituteFont) {
    delete mSubstituteFont;
    mSubstituteFont = nsnull;
  }

  mWesternFont = nsnull;
  mFontHandle = nsnull;

  if (!--gFontMetricsXlibCount) {
    FreeGlobals();
  }
}

NS_IMPL_ISUPPORTS1(nsFontMetricsXlib, nsIFontMetrics)

static PRBool
IsASCIIFontName(const nsString& aName)
{
  PRUint32 len = aName.Length();
  const PRUnichar* str = aName.get();
  for (PRUint32 i = 0; i < len; i++) {
    /*
     * X font names are printable ASCII, ignore others (for now)
     */
    if ((str[i] < 0x20) || (str[i] > 0x7E)) {
      return PR_FALSE;
    }
  }

  return PR_TRUE;
}

static PRBool
FontEnumCallback(const nsString& aFamily, PRBool aGeneric, void *aData)
{
  if (!IsASCIIFontName(aFamily))
    return PR_TRUE;
    
  nsCAutoString name;
  name.AssignWithConversion(aFamily.get());
  name.ToLowerCase();
  
  nsFontMetricsXlib* metrics = (nsFontMetricsXlib*) aData;
  metrics->mFonts.AppendCString(name);
  metrics->mFontIsGeneric.AppendElement((void*) aGeneric);
  
  if (aGeneric) {
    metrics->mGeneric = metrics->mFonts.CStringAt(metrics->mFonts.Count() - 1);
    return PR_FALSE;
  }

  return PR_TRUE;
}


#ifdef USE_XPRINT
/* does this nsFont(Metrics)Xlib class operate on a Xprt (X11 print server) ? */
PRPackedBool nsFontMetricsXlib::mPrinterMode = PR_FALSE;

void 
nsFontMetricsXlib::EnablePrinterMode(PRBool printermode)
{
  mPrinterMode = printermode;
}
#endif /* USE_XPRINT */

NS_IMETHODIMP
nsFontMetricsXlib::Init(const nsFont& aFont, nsIAtom* aLangGroup,
                        nsIDeviceContext* aContext)
{
  NS_ASSERTION(!(nsnull == aContext), "attempt to init fontmetrics with null device context");

  nsresult res;
  mDocConverterType = nsnull;

  if (!gInitialized) {
    res = InitGlobals(aContext);
    if (NS_FAILED(res))
      return res;
  }

  mFont = new nsFont(aFont);
  mLangGroup = aLangGroup;

#ifdef USE_XPRINT 
#ifdef XPRINT_FONT_HACK
  nsString *savedName;

  if(mPrinterMode)
  {
    savedName = &mFont->name;
    mFont->name = NS_LITERAL_STRING("serif");
  }  
#endif  /* XPRINT_FONT_HACK */
#endif /* USE_XPRINT */

  mDeviceContext = aContext;

  float app2dev;
  mDeviceContext->GetAppUnitsToDevUnits(app2dev);

  float textZoom = 1.0;
  mDeviceContext->GetTextZoom(textZoom);
 
  mPixelSize = NSToIntRound(app2dev * textZoom * mFont->size);
  mStretchIndex = 4; // Normal
  mStyleIndex = mFont->style;

  mFont->EnumerateFamilies(FontEnumCallback, this);

  nsXPIDLCString value;
  if (!mGeneric) {
    gPref->CopyCharPref("font.default", getter_Copies(value));
    if (value.get()) {
      mDefaultFont = value.get();
    }
    else
      mDefaultFont = "serif";
    
    mGeneric = &mDefaultFont;
  }

  if (mLangGroup) {
    nsCAutoString name("font.min-size.");
    if (mGeneric->Equals("monospace"))
      name.Append("fixed");
    else
      name.Append("variable");

    name.Append(char('.'));

    const PRUnichar* langGroup = nsnull;
    mLangGroup->GetUnicode(&langGroup);

    name.AppendWithConversion(langGroup);
    PRInt32 minimum = 0;

    res = gPref->GetIntPref(name.get(), &minimum);

    if (NS_FAILED(res))
      gPref->GetDefaultIntPref(name.get(), &minimum);

    if (minimum < 0)
      minimum = 0;

    if (mPixelSize < minimum)
      mPixelSize = minimum;
  }

  if (mLangGroup.get() == gUserDefined) {
    if (!gUserDefinedConverter) {
      nsCOMPtr<nsIAtom> charset;
      res = gCharSetManager->GetCharsetAtom2("x-user-defined",
                                             getter_AddRefs(charset));
      if (NS_SUCCEEDED(res)) {
        res = gCharSetManager->GetUnicodeEncoder(charset,
                                                 &gUserDefinedConverter);
        if (NS_SUCCEEDED(res)) {
          res = gUserDefinedConverter->SetOutputErrorBehavior(
              gUserDefinedConverter->kOnError_Replace, nsnull, '?');

          nsCOMPtr<nsICharRepresentable> mapper =
            do_QueryInterface(gUserDefinedConverter);
          if (mapper) {
            gUserDefinedCCMap = MapperToCCMap(mapper);
            if (!gUserDefinedCCMap)
              return NS_ERROR_OUT_OF_MEMORY;
          }  
        }
        else
          return res;
      }
      else
        return res;
    }

    nsCAutoString name("font.name.");
    name.Append(*mGeneric);
    name.Append(char('.'));
    name.Append(USER_DEFINED);

    gPref->CopyCharPref(name.get(), getter_Copies(value));
    if (value.get()) {
      mUserDefined = value.get();
      mIsUserDefined = 1;
    }
  }

  mWesternFont = FindFont('a');
  if (!mWesternFont)
    return NS_ERROR_FAILURE;

  mFontHandle = mWesternFont->mFont;

  RealizeFont();

#ifdef USE_XPRINT
#ifdef XPRINT_FONT_HACK
  if(mPrinterMode)
    mFont->name = *savedName;
#endif /* XPRINT_FONT_HACK */
#endif /* USE_XPRINT */

  return NS_OK;
}

NS_IMETHODIMP  nsFontMetricsXlib::Destroy()
{
//  NS_IF_RELEASE(mDeviceContext);
  return NS_OK;
}

void nsFontMetricsXlib::RealizeFont()
{
  float f;
  mDeviceContext->GetDevUnitsToAppUnits(f);

  nscoord lineSpacing = nscoord((mFontHandle->ascent + mFontHandle->descent) * f);
  // XXXldb Shouldn't we get mEmHeight from the metrics?
  mEmHeight = PR_MAX(1, nscoord(mWesternFont->mSize * f));
  if (lineSpacing > mEmHeight)
    mLeading = lineSpacing - mEmHeight;
  else
    mLeading = 0;

  mMaxHeight = nscoord((mFontHandle->ascent + mFontHandle->descent) * f);
  mMaxAscent = nscoord(mFontHandle->ascent * f);
  mMaxDescent = nscoord(mFontHandle->descent * f);

  mEmAscent = nscoord(mMaxAscent * mEmHeight / lineSpacing);
  mEmDescent = mEmHeight - mEmAscent;

  mMaxAdvance = nscoord(mFontHandle->max_bounds.width * f);

  int rawWidth;
  if ((mFontHandle->min_byte1 == 0) && (mFontHandle->max_byte1 == 0)) {
    rawWidth = ::XTextWidth(mFontHandle, " ", 1);
  }
  else {
    XChar2b my16bit_space;
    my16bit_space.byte1 = 0;
    my16bit_space.byte2 = ' ';
    rawWidth = ::XTextWidth16(mFontHandle, &my16bit_space, 1);
  }

  mSpaceWidth = NSToCoordRound(rawWidth * f);

  unsigned long pr = 0;
  if (::XGetFontProperty(mFontHandle, XA_X_HEIGHT, &pr) &&
      pr < 0x00ffffff)  // Bug 43214: arbitrary to exclude garbage values
  {
    mXHeight = nscoord(pr * f);
    PR_LOG(FontMetricsXlibLM, PR_LOG_DEBUG, ("xHeight=%d\n", mXHeight));
  }
  else 
  {
    // 56% of ascent, best guess for non-true type
    mXHeight = NSToCoordRound((float) mFontHandle->ascent* f * 0.56f);
  }
    
  if (::XGetFontProperty(mFontHandle, XA_UNDERLINE_POSITION, &pr))
  {
    /* this will only be provided from adobe .afm fonts and TrueType
     * fonts served by xfsft (not xfstt!) */
    mUnderlineOffset = -NSToIntRound(pr * f);
    PR_LOG(FontMetricsXlibLM, PR_LOG_DEBUG, ("underlineOffset=%d\n", mUnderlineOffset));
  }
  else
  {
    /* this may need to be different than one for those weird asian fonts */
    /* mHeight is already multipled by f */
    float height;
    height = mFontHandle->ascent + mFontHandle->descent;
    mUnderlineOffset = -NSToIntRound(PR_MAX (1, floor (0.1 * height + 0.5)) * f);
  }

  if (::XGetFontProperty(mFontHandle, XA_UNDERLINE_THICKNESS, &pr))
  {
    /* this will only be provided from adobe .afm fonts */
    mUnderlineSize = nscoord(PR_MAX(f, NSToIntRound(pr * f)));
    PR_LOG(FontMetricsXlibLM, PR_LOG_DEBUG, ("underlineSize=%d\n", mUnderlineSize));
  }
  else
  {
    /* mHeight is already multipled by f */
    float height;
    height = mFontHandle->ascent + mFontHandle->descent;
    mUnderlineSize = NSToIntRound(PR_MAX(1, floor (0.05 * height + 0.5)) * f);
  }

  if (::XGetFontProperty(mFontHandle, XA_SUPERSCRIPT_Y, &pr))
  {
    mSuperscriptOffset = nscoord(PR_MAX(f, NSToIntRound(pr * f)));
    PR_LOG(FontMetricsXlibLM, PR_LOG_DEBUG, ("superscriptOffset=%d\n", mSuperscriptOffset));
  }
  else
  {
    mSuperscriptOffset = mXHeight;
  }

  if (::XGetFontProperty(mFontHandle, XA_SUBSCRIPT_Y, &pr))
  {
    mSubscriptOffset = nscoord(PR_MAX(f, NSToIntRound(pr * f)));
    PR_LOG(FontMetricsXlibLM, PR_LOG_DEBUG, ("subscriptOffset=%d\n", mSubscriptOffset));
  }
  else
  {
    mSubscriptOffset = mXHeight;
  }

  /* need better way to calculate this */
  mStrikeoutOffset = NSToCoordRound(mXHeight / 2.0);
  mStrikeoutSize = mUnderlineSize;
}

NS_IMETHODIMP  nsFontMetricsXlib::GetXHeight(nscoord& aResult)
{
  aResult = mXHeight;
  return NS_OK;
}

NS_IMETHODIMP  nsFontMetricsXlib::GetSuperscriptOffset(nscoord& aResult)
{
  aResult = mSuperscriptOffset;
  return NS_OK;
}

NS_IMETHODIMP  nsFontMetricsXlib::GetSubscriptOffset(nscoord& aResult)
{
  aResult = mSubscriptOffset;
  return NS_OK;
}

NS_IMETHODIMP  nsFontMetricsXlib::GetStrikeout(nscoord& aOffset, nscoord& aSize)
{
  aOffset = mStrikeoutOffset;
  aSize = mStrikeoutSize;
  return NS_OK;
}

NS_IMETHODIMP  nsFontMetricsXlib::GetUnderline(nscoord& aOffset, nscoord& aSize)
{
  aOffset = mUnderlineOffset;
  aSize = mUnderlineSize;
  return NS_OK;
}

NS_IMETHODIMP  nsFontMetricsXlib::GetHeight(nscoord &aHeight)
{
  aHeight = mMaxHeight;
  return NS_OK;
}

NS_IMETHODIMP  nsFontMetricsXlib::GetNormalLineHeight(nscoord &aHeight)
{
  aHeight = mEmHeight + mLeading;
  return NS_OK;
}

NS_IMETHODIMP  nsFontMetricsXlib::GetLeading(nscoord &aLeading)
{
  aLeading = mLeading;
  return NS_OK;
}

NS_IMETHODIMP  nsFontMetricsXlib::GetEmHeight(nscoord &aHeight)
{
  aHeight = mEmHeight;
  return NS_OK;
}

NS_IMETHODIMP  nsFontMetricsXlib::GetEmAscent(nscoord &aAscent)
{
  aAscent = mEmAscent;
  return NS_OK;
}

NS_IMETHODIMP  nsFontMetricsXlib::GetEmDescent(nscoord &aDescent)
{
  aDescent = mEmDescent;
  return NS_OK;
}

NS_IMETHODIMP  nsFontMetricsXlib::GetMaxHeight(nscoord &aHeight)
{
  aHeight = mMaxHeight;
  return NS_OK;
}

NS_IMETHODIMP  nsFontMetricsXlib::GetMaxAscent(nscoord &aAscent)
{
  aAscent = mMaxAscent;
  return NS_OK;
}

NS_IMETHODIMP  nsFontMetricsXlib::GetMaxDescent(nscoord &aDescent)
{
  aDescent = mMaxDescent;
  return NS_OK;
}

NS_IMETHODIMP  nsFontMetricsXlib::GetMaxAdvance(nscoord &aAdvance)
{
  aAdvance = mMaxAdvance;
  return NS_OK;
}

NS_IMETHODIMP  nsFontMetricsXlib::GetFont(const nsFont*& aFont)
{
  aFont = mFont;
  return NS_OK;
}

NS_IMETHODIMP  nsFontMetricsXlib::GetLangGroup(nsIAtom** aLangGroup)
{
  if (!aLangGroup) {
    return NS_ERROR_NULL_POINTER;
  }

  *aLangGroup = mLangGroup;
  NS_IF_ADDREF(*aLangGroup);

  return NS_OK;
}

NS_IMETHODIMP  nsFontMetricsXlib::GetFontHandle(nsFontHandle &aHandle)
{
  aHandle = (nsFontHandle)mWesternFont;
  return NS_OK;
}

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

static int
SingleByteConvert(nsFontCharSetXlibInfo* aSelf, XFontStruct* aFont,
                  const PRUnichar* aSrcBuf, PRInt32 aSrcLen, 
                  char* aDestBuf, PRInt32 aDestLen)
{
  int count = 0;
  if (aSelf->mConverter) {
    aSelf->mConverter->Convert(aSrcBuf, &aSrcLen, aDestBuf, &aDestLen);
    count = aDestLen;
  }

  return count;
}

/*
static void 
ReverseBuffer(char* aBuf, int count)
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

static int
SingleByteConvertReverse(nsFontCharSetXlibInfo* aSelf, const PRUnichar* aSrcBuf,
  PRInt32 aSrcLen, char* aDestBuf, PRInt32 aDestLen)
{
    int count = SingleByteConvert(aSelf, aSrcBuf,
                       aSrcLen, aDestBuf,  aDestLen);
    ReverseBuffer(aDestBuf, count);
    return count;
}
*/

static int
DoubleByteConvert(nsFontCharSetXlibInfo* aSelf, XFontStruct* aFont, 
                  const PRUnichar* aSrcBuf, PRInt32 aSrcLen, char* aDestBuf, 
                  PRInt32 aDestLen)
{
  int count = 0;
  if (aSelf->mConverter) {
    aSelf->mConverter->Convert(aSrcBuf, &aSrcLen, aDestBuf, &aDestLen);
    count = aDestLen;
    if (count > 0) {
      if ((aDestBuf[0] & 0x80) && (!(aFont->max_byte1 & 0x80))) {
        for (PRInt32 i = 0; i < aDestLen; i++)
          aDestBuf[i] &= 0x7F;
      }
      else if ((!(aDestBuf[0] & 0x80)) && (aFont->min_byte1 & 0x80)) {
        for (PRInt32 i = 0; i < aDestLen; i++)
            aDestBuf[i] |= 0x80;
      }
    }
  }
  else
    count = 0;

  return count;
}

static int
ISO10646Convert(nsFontCharSetXlibInfo* aSelf, XFontStruct* aFont,
                const PRUnichar* aSrcBuf, PRInt32 aSrcLen,
                char* aDestBuf, PRInt32 aDestLen)
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

  return (int) aSrcLen * 2;
}

#ifdef DEBUG
static void
CheckMap(nsFontCharSetMapXlib* aEntry)
{
  while (aEntry->mName) {
    if (aEntry->mInfo->mCharSet) {
      nsresult res;
      nsCOMPtr<nsIAtom> charset =
        getter_AddRefs(NS_NewAtom(aEntry->mInfo->mCharSet));
      if (charset) {
        nsCOMPtr<nsIUnicodeEncoder> converter;
        res = gCharSetManager->GetUnicodeEncoder(charset,
          getter_AddRefs(converter));
        if (NS_FAILED(res)) {
          printf("\a===ERROR: === %s failed (%s)\n", aEntry->mInfo->mCharSet, __FILE__);
        }
      }
    }
    aEntry++;
  }
}

static void
CheckSelf(void)
{
  CheckMap(gCharSetMap);

#ifdef MOZ_MATHML
  // For this to pass, the ucvmath module must be built as well
  CheckMap(gSpecialCharSetMap);
#endif /* MOZ_MATHML */
}
#endif /* DEBUG */

static void
SetUpFontCharSetInfo(nsFontCharSetXlibInfo* aSelf)
{
#ifdef DEBUG
  static int checkedSelf = 0;
  if (!checkedSelf) {
    CheckSelf();
    checkedSelf = 1;
  }
#endif /* DEBUG */

  nsresult res;
  nsCOMPtr<nsIAtom> charset = getter_AddRefs(NS_NewAtom(aSelf->mCharSet));
  if (charset) {
    nsIUnicodeEncoder* converter = nsnull;
    res = gCharSetManager->GetUnicodeEncoder(charset, &converter);
    if (NS_SUCCEEDED(res)) {
      aSelf->mConverter = converter;
      res = converter->SetOutputErrorBehavior(converter->kOnError_Replace,
          nsnull, '?');
      nsCOMPtr<nsICharRepresentable> mapper = do_QueryInterface(converter);
      if (mapper) {
        aSelf->mCCMap = MapperToCCMap(mapper);
        if (!aSelf->mCCMap)
          return;

        /*
         * We used to disable special characters like smart quotes
         * in CJK fonts because if they are quite a bit larger than
         * western glyphs and we did not want glyph fill-in to use them
         * in single byte documents.
         *
         * Now, single byte documents find these special chars before
         * the CJK fonts are searched so this is no longer needed
         * and should be removed, as requested in bug 100233
          */
        if ((aSelf->Convert == DoubleByteConvert)
            && (!gAllowDoubleByteSpecialChars)) {
          PRUint16* ccmap = aSelf->mCCMap;
          for (int i=0; gDoubleByteSpecialChars[i]; i++)
            CCMAP_UNSET_CHAR(ccmap, gDoubleByteSpecialChars[i]);
        }
      }
      else
       NS_WARNING("cannot get nsICharRepresentable");
    }
    else
      NS_WARNING("cannot get Unicode converter");
  }
  else
    NS_WARNING("cannot get atom");
}

#undef DEBUG_DUMP_TREE
#ifdef DEBUG_DUMP_TREE

static const char* gDumpStyles[3] = { "normal", "italic", "oblique" };

static PRIntn
DumpCharSet(PLHashEntry* he, PRIntn i, void* arg)
{
  PR_LOG(FontMetricsXlibLM, PR_LOG_DEBUG, ("        %s\n", (char*) he->key));
  nsFontCharSetXlib* charSet = (nsFontCharSetXlib*) he->value;
  for (int sizeIndex = 0; sizeIndex < charSet->mSizesCount; sizeIndex++) {
    nsFontXlib* size = &charSet->mSizes[sizeIndex];
    PR_LOG(FontMetricsXlibLM, PR_LOG_DEBUG, ("          %d %s\n", size->mSize, size->mName));
  }
  return HT_ENUMERATE_NEXT;
}

static void
DumpFamily(nsFontFamilyXlib* aFamily)
{
  for (int styleIndex = 0; styleIndex < 3; styleIndex++) {
    nsFontStyleXlib* style = aFamily->mStyles[styleIndex];
    if (style) {
      PR_LOG(FontMetricsXlibLM, PR_LOG_DEBUG, ("  style: %s\n", gDumpStyles[styleIndex]));
      for (int weightIndex = 0; weightIndex < 8; weightIndex++) {
        nsFontWeightXlib* weight = style->mWeights[weightIndex];
        if (weight) {
          PR_LOG(FontMetricsXlibLM, PR_LOG_DEBUG, ("    weight: %d\n", (weightIndex + 1) * 100));
          for (int stretchIndex = 0; stretchIndex < 9; stretchIndex++) {
            nsFontStretchXlib* stretch = weight->mStretches[stretchIndex];
            if (stretch) {
              PR_LOG(FontMetricsXlibLM, PR_LOG_DEBUG, ("      stretch: %d\n", stretchIndex + 1));
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
  PR_LOG(FontMetricsXlibLM, PR_LOG_DEBUG, ("family: %s\n", buf));
  nsFontFamilyXlib* family = (nsFontFamilyXlib*) he->value;
  DumpFamily(family);

  return HT_ENUMERATE_NEXT;
}

static void
DumpTree(void)
{
  PL_HashTableEnumerateEntries(gFamilies, DumpFamilyEnum, nsnull);
}

#endif /* DEBUG_DUMP_TREE */

struct nsFontSearch
{
  nsFontMetricsXlib* mMetrics;
  PRUnichar          mChar;
  nsFontXlib*        mFont;
};

#if 0
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
#endif /* 0 */

static PRUint16*
GetMapFor10646Font(XFontStruct* aFont)
{
  if (!aFont->per_char)
    return nsnull;

  nsCompressedCharMap ccmapObj;
  PRInt32 minByte1 = aFont->min_byte1;
  PRInt32 maxByte1 = aFont->max_byte1;
  PRInt32 minByte2 = aFont->min_char_or_byte2;
  PRInt32 maxByte2 = aFont->max_char_or_byte2;
  PRInt32 charsPerRow = maxByte2 - minByte2 + 1;
  for (PRInt32 row = minByte1; row <= maxByte1; row++) {
    PRInt32 offset = (((row - minByte1) * charsPerRow) - minByte2);
    for (PRInt32 cell = minByte2; cell <= maxByte2; cell++) {
      XCharStruct* bounds = &aFont->per_char[offset + cell];
      if (bounds->ascent || bounds->descent) {
        ccmapObj.SetChar((row << 8) | cell);
      }
    }
  }    

  PRUint16 *ccmap = ccmapObj.NewCCMap();
  return ccmap;
}

PRBool
nsFontXlib::IsEmptyFont(XFontStruct* xFont)
{
  if (!xFont)
    return PR_TRUE;

  //
  // scan and see if we can find at least one glyph
  //
  if (xFont->per_char) {
    PRInt32 minByte1 = xFont->min_byte1;
    PRInt32 maxByte1 = xFont->max_byte1;
    PRInt32 minByte2 = xFont->min_char_or_byte2;
    PRInt32 maxByte2 = xFont->max_char_or_byte2;
    PRInt32 charsPerRow = maxByte2 - minByte2 + 1;
    for (PRInt32 row = minByte1; row <= maxByte1; row++) {
      PRInt32 offset = (((row - minByte1) * charsPerRow) - minByte2);
      for (PRInt32 cell = minByte2; cell <= maxByte2; cell++) {
        XCharStruct* bounds = &xFont->per_char[offset + cell];
        if (bounds->ascent || bounds->descent) {
          return PR_FALSE;
        }
      }
    }
  }

  return PR_TRUE;
}

void
nsFontXlib::LoadFont(void)
{
  if (mAlreadyCalledLoadFont)
    return;

  Display *aDisplay = xxlib_rgb_get_display(gXlibRgbHandle);

#ifdef USE_XPRINT
  if (nsFontMetricsXlib::mPrinterMode)
  {
    if (XpGetContext(aDisplay) == None)
    {
      /* applications must not make any assumptions about fonts _before_ XpSetContext() !!! */
      NS_ERROR("Obtaining font information without a valid print context (XLoadQueryFont()) _before_ XpSetContext()\n");
#ifdef DEBUG
      abort();
#else
      return;
#endif /* DEBUG */      
    }
  }
#endif /* USE_XPRINT */

  mAlreadyCalledLoadFont = PR_TRUE;

  XFontStruct *xlibFont = ::XLoadQueryFont(aDisplay, mName);

#ifdef DEBUG
  if(!xlibFont)
  {
    printf("nsFontXlib::LoadFont(): loading of font '%s' failed\n", mName);
  }
#endif /* DEBUG */

  PR_LOG(FontMetricsXlibLM, PR_LOG_DEBUG, 
         ("nsFontXlib::LoadFont(): XLoadQueryFont(aDisplay='%s',mName='%s') returned %lx\n", 
         ::XDisplayString(aDisplay), mName, (long)xlibFont));

  if (xlibFont) {
    if (mCharSetInfo == &ISO106461) {
      mCCMap = GetMapFor10646Font(xlibFont);
      if (!mCCMap) {
        ::XFreeFont(aDisplay, xlibFont);
        return;
      }
    }

    // Redhat 6.2 Japanese has invalid jisx201 fonts
    // Solaris 2.6 has invalid cns11643 fonts for planes 4-7
    if ((mCharSetInfo == &JISX0201)
        || (mCharSetInfo == &CNS116434)
        || (mCharSetInfo == &CNS116435)
        || (mCharSetInfo == &CNS116436)
        || (mCharSetInfo == &CNS116437)
       ) {
      if (IsEmptyFont(xlibFont)) {
        NS_WARNING("empty font.\n");
        ::XFreeFont(aDisplay, xlibFont);
        return;
      }
    }
  }

  mFont = xlibFont;

#if 0
    if (aCharSet->mInfo->mSpecialUnderline && aMetrics->mFontHandle) {
      XFontStruct* asciiXFont = aMetrics->mFontHandle;
      unsigned long positionX2;
      unsigned long thickness;
      GetUnderlineInfo(asciiXFont, &positionX2, &thickness);
      mActualSize += (positionX2 + thickness);
      mBaselineAdjust = (-xlibFont->max_bounds.descent);
    }
#endif
}


PRBool
nsFontXlib::GetXlibFontIs10646(void)
{
  return ((PRBool) (mCharSetInfo == &ISO106461));
}

// nsFontXlib Class Implementation
MOZ_DECL_CTOR_COUNTER(nsFontXlib)

nsFontXlib::nsFontXlib()
{
  MOZ_COUNT_CTOR(nsFontXlib);
  mFont                  = nsnull;
  mCCMap                 = nsnull;
  mCharSetInfo           = nsnull;
  mName                  = nsnull;
  mUserDefinedFont       = nsnull;
  mAlreadyCalledLoadFont = PR_FALSE;
}

nsFontXlib::~nsFontXlib()
{
  MOZ_COUNT_DTOR(nsFontXlib);
  if (mFont)
    XFreeFont(xxlib_rgb_get_display(gXlibRgbHandle), mFont);

  if (mCharSetInfo == &ISO106461)
    FreeCCMap(mCCMap);

  if (mName)
    PR_smprintf_free(mName);
}

class nsFontXlibNormal : public nsFontXlib
{
public:
  nsFontXlibNormal();
  virtual ~nsFontXlibNormal();
  
  virtual int GetWidth(const PRUnichar* aString, PRUint32 aLength);
  virtual int DrawString(nsRenderingContextXlib* aContext,
                         nsIDrawingSurfaceXlib* aSurface,
                         nscoord aX, nscoord aY,
                         const PRUnichar* aString, PRUint32 aLength);

#ifdef MOZ_MATHML
  virtual nsresult GetBoundingMetrics(const PRUnichar*   aString,
                                      PRUint32           aLength,
                                      nsBoundingMetrics& aBoundingMetrics);
#endif /* MOZ_MATHML */
};

nsFontXlibNormal::nsFontXlibNormal()
{
}

nsFontXlibNormal::~nsFontXlibNormal()
{
}

int
nsFontXlibNormal::GetWidth(const PRUnichar* aString, PRUint32 aLength)
{
  if (!mFont) {
    LoadFont();
    if (!mFont)
      return 0;
  }

  XChar2b buf[512];
  char *p;
  PRInt32 bufLen;
  int textWidth;

  ENCODER_BUFFER_ALLOC_IF_NEEDED(p, mCharSetInfo->mConverter,
      aString, aLength, buf, sizeof(buf), bufLen);

  int len = mCharSetInfo->Convert(mCharSetInfo, (XFontStruct*) mFont, aString,
                                  aLength, p, bufLen);

  if ((mFont->min_byte1 == 0) && (mFont->max_byte1 == 0))
    textWidth = XTextWidth(mFont, (char *) buf, len);
  else
    textWidth = XTextWidth16(mFont, buf, len / 2);

  ENCODER_BUFFER_FREE_IF_NEEDED(p, buf);
  return textWidth;
}

int
nsFontXlibNormal::DrawString(nsRenderingContextXlib* aContext,
                             nsIDrawingSurfaceXlib* aSurface,
                             nscoord aX, nscoord aY,
                             const PRUnichar* aString, PRUint32 aLength)
{
  if (!mFont) {
    LoadFont();
    if (!mFont)
      return 0;
  }

  XChar2b buf[512];
  char *p;
  PRInt32 bufLen;
  int textWidth;

  ENCODER_BUFFER_ALLOC_IF_NEEDED(p, mCharSetInfo->mConverter,
      aString, aLength, buf, sizeof(buf), bufLen);
  
  int len = mCharSetInfo->Convert(mCharSetInfo, mFont, aString, aLength,
                                  p, bufLen);

  if ((mFont->min_byte1 == 0) && (mFont->max_byte1 == 0)) {
    xGC *gc = aContext->GetGC();
    Drawable drawable; aSurface->GetDrawable(drawable);
    XlibRgbHandle *xrgbh; aSurface->GetXlibRgbHandle(xrgbh);
    
    XDrawString(xxlib_rgb_get_display(xrgbh),
                drawable,
                *gc,
                aX, aY + mBaselineAdjust, p, len);

    gc->Release();
    textWidth = XTextWidth(mFont, p, len);
  }
  else
  {
    nsFontXlib *savedFont = aContext->GetCurrentFont();
    aContext->SetCurrentFont(this);
    aContext->UpdateGC();

    xGC *gc = aContext->GetGC();
    
    /* note the length must be divided by 2 for X*16 functions */
    Drawable drawable; aSurface->GetDrawable(drawable);
    XlibRgbHandle *xrgbh; aSurface->GetXlibRgbHandle(xrgbh);
    XDrawString16(xxlib_rgb_get_display(xrgbh),
                  drawable,
                  *gc,
                  aX, aY + mBaselineAdjust, (XChar2b *)p, len / 2);
                  
    gc->Release();
    
    textWidth = XTextWidth16(mFont, (XChar2b *)p, len / 2);

    aContext->SetCurrentFont(savedFont);
    aContext->UpdateGC();
  }

  ENCODER_BUFFER_FREE_IF_NEEDED(p, buf);
  return textWidth;
}

#ifdef MOZ_MATHML
nsresult
nsFontXlibNormal::GetBoundingMetrics(const PRUnichar*   aString,
                                     PRUint32           aLength,
                                     nsBoundingMetrics& aBoundingMetrics)
{
  aBoundingMetrics.Clear();

  if (!mFont) {
    LoadFont();
    if (!mFont)
      return NS_ERROR_FAILURE;
  }

  if (aString && 0 < aLength) {
    XChar2b buf[512]; // XXX watch buffer length !!!
    char *p;
    PRInt32 bufLen;

    ENCODER_BUFFER_ALLOC_IF_NEEDED(p, mCharSetInfo->mConverter,
        aString, aLength, buf, sizeof(buf), bufLen);

    int len = mCharSetInfo->Convert(mCharSetInfo, mFont, aString, aLength,
                                    p, bufLen);

    XCharStruct overall;
    int direction, font_ascent, font_descent;

    if ((mFont->min_byte1 == 0) && (mFont->max_byte1 == 0))
      XTextExtents(mFont, p, len,
                   &direction, &font_ascent, &font_descent,
                   &overall);
    else
      XTextExtents16(mFont, (XChar2b *)p, len / 2,
                     &direction, &font_ascent, &font_descent,
                     &overall);

    aBoundingMetrics.leftBearing  =  overall.lbearing;
    aBoundingMetrics.rightBearing =  overall.rbearing;
    aBoundingMetrics.width        =  overall.width;
    aBoundingMetrics.ascent       =  overall.ascent;
    aBoundingMetrics.descent      =  overall.descent;

    ENCODER_BUFFER_FREE_IF_NEEDED(p, buf);
  }

  return NS_OK;
}
#endif /* MOZ_MATHML */
  
class nsFontXlibSubstitute : public nsFontXlib
{
public:
  nsFontXlibSubstitute(nsFontXlib* aFont);
  virtual ~nsFontXlibSubstitute();
  
  virtual PRBool GetXlibFontIs10646(void);
  virtual int GetWidth(const PRUnichar* aString, PRUint32 aLength);
  virtual int DrawString(nsRenderingContextXlib* aContext,
                         nsIDrawingSurfaceXlib* aSurface,
                         nscoord aX, nscoord aY,
                         const PRUnichar* aString, PRUint32 aLength);

#ifdef MOZ_MATHML
  virtual nsresult GetBoundingMetrics(const PRUnichar*   aString,
                                      PRUint32           aLength,
                                      nsBoundingMetrics& aBoundingMetrics);
#endif /* MOZ_MATHML */
  virtual PRUint32 Convert(const PRUnichar* aSrc, PRUint32 aSrcLen,
                           PRUnichar* aDest, PRUint32 aDestLen);

  nsFontXlib* mSubstituteFont;
};

nsFontXlibSubstitute::nsFontXlibSubstitute(nsFontXlib* aFont)
{
  mSubstituteFont = aFont;
}

nsFontXlibSubstitute::~nsFontXlibSubstitute()
{
  // Do not free mSubstituteFont here. It is owned by somebody else.
}

PRUint32
nsFontXlibSubstitute::Convert(const PRUnichar* aSrc, PRUint32 aSrcLen,
                              PRUnichar* aDest, PRUint32 aDestLen)
{
  nsresult res;

  if (!gFontSubConverter) {
    nsComponentManager::CreateInstance(kSaveAsCharsetCID, nsnull,
                                       NS_GET_IID(nsISaveAsCharset),
                                       (void**) &gFontSubConverter);
    if (gFontSubConverter) {
      res = gFontSubConverter->Init("ISO-88591-1",
                             nsISaveAsCharset::attr_FallbackQuestionMark +
                             nsISaveAsCharset::attr_EntityBeforeCharsetConv,
                             nsIEntityConverter::transliterate);
      if (NS_FAILED(res))
        NS_RELEASE(gFontSubConverter);
    }
  }

  if (gFontSubConverter) {
    nsAutoString tmp(aSrc, aSrcLen);
    char* conv = nsnull;

    /* timecop revisit with nsXPIDLCString */
    res = gFontSubConverter->Convert(tmp.get(), &conv);
    if (NS_SUCCEEDED(res) && conv) {
      char* p = conv;
      PRUint32 i;
      for (i = 0; i < aDestLen; i++) {
        if (*p)
          aDest[i] = *p;
        else
          break;

        p++;
      }
      nsMemory::Free(conv);
      conv = nsnull;
      return i;
    }
  }

  if (aSrcLen > aDestLen)
    aSrcLen = aDestLen;

  for (PRUint32 i = 0; i < aSrcLen; i++)
    aDest[i] = '?';

  return aSrcLen;
}

int
nsFontXlibSubstitute::GetWidth(const PRUnichar* aString, PRUint32 aLength)
{
  PRUnichar buf[512];
  PRUnichar *p = buf;
  PRUint32 bufLen = sizeof(buf) / sizeof(PRUnichar);

  if ((aLength * 2) > bufLen) {
    PRUnichar *tmp;
    tmp = (PRUnichar*)nsMemory::Alloc(sizeof(PRUnichar) * (aLength * 2));
    if (tmp) {
      p = tmp;
      bufLen = (aLength * 2);
    }
  }
  PRUint32 len = Convert(aString, aLength, p, bufLen);
  int textWidth = mSubstituteFont->GetWidth(p, len);
  if (p != buf)
    nsMemory::Free(p);

  return textWidth;
}

int
nsFontXlibSubstitute::DrawString(nsRenderingContextXlib* aContext, 
                                 nsIDrawingSurfaceXlib* aSurface,
                                 nscoord aX, nscoord aY,
                                 const PRUnichar* aString, PRUint32 aLength)
{
  PRUnichar buf[512];
  PRUnichar *p = buf;
  PRUint32 bufLen = sizeof(buf) / sizeof(PRUnichar);
  if ((aLength * 2) > bufLen) {
    PRUnichar *tmp;
    tmp = (PRUnichar*)nsMemory::Alloc(sizeof(PRUnichar) * (aLength * 2));
    if (tmp) {
      p = tmp;
      bufLen = (aLength * 2);
    }
  }
  
  PRUint32 len = Convert(aString, aLength, p, bufLen);
  int textWidth = mSubstituteFont->DrawString(aContext, aSurface,
                                              aX, aY, p, len);
  if (p != buf)
    nsMemory::Free(p);
  return textWidth;
}

#ifdef MOZ_MATHML
// bounding metrics for a string 
// remember returned values are not in app units
nsresult
nsFontXlibSubstitute::GetBoundingMetrics(const PRUnichar*   aString,
                                         PRUint32           aLength,
                                         nsBoundingMetrics& aBoundingMetrics)
{
  PRUnichar buf[512];
  PRUnichar *p = buf;
  PRUint32 bufLen = sizeof(buf) / sizeof(PRUnichar);
  
  if ((aLength * 2) > bufLen) {
    PRUnichar *tmp;
    tmp = (PRUnichar *)nsMemory::Alloc(sizeof(PRUnichar) * (aLength * 2));
    if (tmp) {
      p = tmp;
      bufLen = (aLength * 2);
    }
  }
  
  PRUint32 len = Convert(aString, aLength, p, bufLen);
  nsresult res = mSubstituteFont->GetBoundingMetrics(buf, len,
                                                     aBoundingMetrics);

  if (p != buf)
    nsMemory::Free(p);

  return res;
}
#endif /* MOZ_MATHML */

PRBool
nsFontXlibSubstitute::GetXlibFontIs10646(void)
{
  return mSubstituteFont->GetXlibFontIs10646();
}

class nsFontXlibUserDefined : public nsFontXlib
{
public:
  nsFontXlibUserDefined();
  virtual ~nsFontXlibUserDefined();

  virtual PRBool Init(nsFontXlib* aFont);
  virtual int GetWidth(const PRUnichar* aString, PRUint32 aLength);
  virtual int DrawString(nsRenderingContextXlib* aContext,
                         nsIDrawingSurfaceXlib* aSurface,
                         nscoord aX, nscoord aY, const PRUnichar* aString, 
                         PRUint32 aLength);

#ifdef MOZ_MATHML
  virtual nsresult GetBoundingMetrics(const PRUnichar*   aString,
                                      PRUint32           aLength,
                                      nsBoundingMetrics& aBoundingMetrics);
#endif /* MOZ_MATHML */
  virtual PRUint32 Convert(const PRUnichar* aSrc, PRInt32 aSrcLen,
                           char* aDest, PRInt32 aDestLen);
};

nsFontXlibUserDefined::nsFontXlibUserDefined()
{
}

nsFontXlibUserDefined::~nsFontXlibUserDefined()
{
  // Do not free mFont here. It is owned by somebody else.
}

PRBool
nsFontXlibUserDefined::Init(nsFontXlib* aFont)
{
  if(!aFont->mFont) {
    aFont->LoadFont();
    if (!aFont->mFont) {
      mCCMap = gEmptyCCMap;
      return PR_FALSE;
    }  
  }

  mFont = aFont->mFont;
  mCCMap = gUserDefinedCCMap;
  mName = aFont->mName;

  return PR_TRUE;
}

PRUint32
nsFontXlibUserDefined::Convert(const PRUnichar* aSrc, PRInt32 aSrcLen,
                               char* aDest, PRInt32 aDestLen)
{
  if (aSrcLen > aDestLen)
    aSrcLen = aDestLen;

  gUserDefinedConverter->Convert(aSrc, &aSrcLen, aDest, &aDestLen);

  return aSrcLen;
}

int
nsFontXlibUserDefined::GetWidth(const PRUnichar* aString, PRUint32 aLength)
{
  char buf[1024];
  char *p;
  PRInt32 bufLen;

  ENCODER_BUFFER_ALLOC_IF_NEEDED(p, gUserDefinedConverter,
      aString, aLength, buf, sizeof(buf), bufLen);
  
  PRUint32 len = Convert(aString, aLength, p, bufLen);

  NS_ASSERTION(((mFont->min_byte1 == 0) && (mFont->max_byte1 == 0)) || 
               GetXlibFontIs10646(), "using 8bit instead of 16bit text data");
  int textWidth = XTextWidth(mFont, p, len);
  
  ENCODER_BUFFER_FREE_IF_NEEDED(p, buf);
  return textWidth;
}

int
nsFontXlibUserDefined::DrawString(nsRenderingContextXlib* aContext,
                                  nsIDrawingSurfaceXlib* aSurface,
                                  nscoord aX, nscoord aY,
                                  const PRUnichar* aString, PRUint32 aLength)
{
  char buf[1024];
  char *p;
  PRInt32 bufLen;

  ENCODER_BUFFER_ALLOC_IF_NEEDED(p, gUserDefinedConverter,
      aString, aLength, buf, sizeof(buf), bufLen);

  PRUint32 len = Convert(aString, aLength, p, bufLen);

  xGC *gc = aContext->GetGC();
  NS_ASSERTION(((mFont->min_byte1 == 0) && (mFont->max_byte1 == 0)) || 
               GetXlibFontIs10646(), "drawing 8bit instead of 16bit text data");
  Drawable drawable; aSurface->GetDrawable(drawable);
  XlibRgbHandle *xrgbh; aSurface->GetXlibRgbHandle(xrgbh);
  XDrawString(xxlib_rgb_get_display(xrgbh),
              drawable,
              *gc,
              aX, aY + mBaselineAdjust, p, len);

  gc->Release();
  int textWidth = XTextWidth(mFont, p, len);

  ENCODER_BUFFER_FREE_IF_NEEDED(p, buf);
  return textWidth;
}

#ifdef MOZ_MATHML
nsresult
nsFontXlibUserDefined::GetBoundingMetrics(const PRUnichar*   aString,
                                          PRUint32           aLength,
                                          nsBoundingMetrics& aBoundingMetrics)
{
  aBoundingMetrics.Clear();

  if (aString && 0 < aLength) {
    char buf[1024];
    char *p;
    PRInt32 bufLen;
    
    ENCODER_BUFFER_ALLOC_IF_NEEDED(p, gUserDefinedConverter,
        aString, aLength, buf, sizeof(buf), bufLen);
    
    int len = Convert(aString, aLength, p, bufLen);

    XCharStruct overall;
    int direction, font_ascent, font_descent;

    if ((mFont->min_byte1 == 0) && (mFont->max_byte1 == 0)) {
      XTextExtents(mFont, p, len,
                   &direction, &font_ascent, &font_descent,
                   &overall);
    } else {
      XTextExtents16(mFont, (XChar2b *)p, len / 2,
                     &direction, &font_ascent, &font_descent,
                     &overall);
    }

    aBoundingMetrics.leftBearing  =  overall.lbearing;
    aBoundingMetrics.rightBearing =  overall.rbearing;
    aBoundingMetrics.width        =  overall.width;
    aBoundingMetrics.ascent       =  overall.ascent;
    aBoundingMetrics.descent      =  overall.descent;
    ENCODER_BUFFER_FREE_IF_NEEDED(p, buf);
  }

  return NS_OK;
}
#endif /* MOZ_MATHML */

nsFontXlib*
nsFontMetricsXlib::AddToLoadedFontsList(nsFontXlib* aFont)
{
  if (mLoadedFontsCount == mLoadedFontsAlloc) {
    int newSize;
    if (mLoadedFontsAlloc)
      newSize = (2 * mLoadedFontsAlloc);
    else
      newSize = 1;

    nsFontXlib** newPointer = (nsFontXlib**) 
      PR_Realloc(mLoadedFonts, newSize * sizeof(nsFontXlib*));
    
    if (newPointer) {
      mLoadedFonts = newPointer;
      mLoadedFontsAlloc = newSize;
    }
    else {
      return nsnull;
    }
  }
  mLoadedFonts[mLoadedFontsCount++] = aFont;
  return aFont;
}

nsFontXlib*
nsFontMetricsXlib::PickASizeAndLoad(nsFontStretchXlib* aStretch,
                                    nsFontCharSetXlibInfo* aCharSet,
                                    PRUnichar aChar)
{
  nsFontXlib *font            = nsnull;
  PRBool      use_scaled_font = PR_FALSE;
  
#ifdef USE_XPRINT
/* gisburn: Small hack for Xprint:
 * Xprint usually operates at resolutions >= 300DPI. There are 
 * usually no "normal" bitmap fonts at those resolutions - only 
 * "scaleable outline fonts" and "printer buildin fonts" (which 
 * usually look like scaleable bitmap fonts) are available.
 * Therefore: force use of scaleable fonts to get rid of 
 * manually scaled bitmap fonts...
 */
 if (mPrinterMode)
 {
   use_scaled_font = PR_TRUE;
 }
#endif /* USE_XPRINT */

// define a size such that a scaled font would always be closer
// to the desired size than this
#define NOT_FOUND_FONT_SIZE (1000*1000*1000)
  PRInt32 bitmap_size = NOT_FOUND_FONT_SIZE;
  PRInt32 scale_size = mPixelSize;

  if (aStretch->mSizes) {
    nsFontXlib** begin = aStretch->mSizes;
    nsFontXlib** end = &aStretch->mSizes[aStretch->mSizesCount];
    nsFontXlib** s;
    // scan the list of sizes
    for (s = begin; s < end; s++) {
      if ((*s)->mSize >= mPixelSize) {
        break;
      }
    }
    // backup if we hit the end of the list
    if (s == end) {
      s--;
    }
    else if (s != begin) {
      // if we overshot pick the closest size
      if (((*s)->mSize - mPixelSize) >= (mPixelSize - (*(s - 1))->mSize)) {
        s--;
      }
    }
    // this is the nearest bitmap font
    font = *s;
    bitmap_size = (*s)->mSize;
  }

#ifndef ABS
#define ABS(a)     (((a) < 0) ? -(a) : (a))
#endif /* !ABS */
  // if we do not have the correct size 
  // check if we can use a scaled font
  if ((mPixelSize != bitmap_size) && (aStretch->mScalable)) {
    // if we have an outline font then use that
    // if it is allowed to be closer than the bitmap
    if (aStretch->mOutlineScaled) {
      scale_size = PR_MAX(mPixelSize, aCharSet->mOutlineScaleMin);
      if (ABS(mPixelSize - scale_size) < ABS(mPixelSize - bitmap_size)) {
        use_scaled_font = PR_TRUE;
      }
    }
    else {
      // if we do not have any similarly sized font
      // use a bitmap scaled font (ugh!)
      scale_size = PR_MAX(mPixelSize, aCharSet->mBitmapScaleMin);
      double ratio = (bitmap_size / ((double) mPixelSize));
      if ((ratio < aCharSet->mBitmapUndersize)
          || (ratio > aCharSet->mBitmapOversize)) {
        if ((ABS(mPixelSize - scale_size) < ABS(mPixelSize - bitmap_size))) {
          use_scaled_font = PR_TRUE;
        }
      }
    }
  }

  NS_ASSERTION((bitmap_size < NOT_FOUND_FONT_SIZE) || use_scaled_font, "did not find font size");

  if (use_scaled_font) {
    PRInt32 i;
    PRInt32 n = aStretch->mScaledFonts.Count();
    nsFontXlib *p = nsnull;
    for (i = 0; i < n; i++) {
      p = (nsFontXlib*) aStretch->mScaledFonts.ElementAt(i);
      if (p->mSize == scale_size)
        break;
    }

    if (i == n) {
      font = new nsFontXlibNormal;
      if (font) {
        /*
         * XXX Instead of passing pixel size, we ought to take underline
         * into account. (Extra space for underline for Asian fonts.)
         */
        font->mName = PR_smprintf(aStretch->mScalable, scale_size);
        if (!font->mName) {
          delete font;
          return nsnull;
        }
        font->mSize = scale_size;
        font->mCharSetInfo = aCharSet;
        aStretch->mScaledFonts.AppendElement(font);
      }
      else {
        return nsnull;
      }
    }
    else {
      font = p;
    }
  }

  if (aCharSet->mCharSet) {
    font->mCCMap = aCharSet->mCCMap;
    if (CCMAP_HAS_CHAR(font->mCCMap, aChar)) {
      font->LoadFont();
        if (!font->mFont)
          return nsnull;
    }
  }
  else {
    if (aCharSet == &ISO106461) {
      font->LoadFont();
      if (!font->mFont)
        return nsnull;
    }
  }

  if (mIsUserDefined) {
    if (!font->mUserDefinedFont) {
      font->mUserDefinedFont = new nsFontXlibUserDefined();
      if (!font->mUserDefinedFont)
        return nsnull;
      if (!font->mUserDefinedFont->Init(font))
        return nsnull;
    }
    font = font->mUserDefinedFont;
  }

  return AddToLoadedFontsList(font);
}

static int
CompareSizes(const void* aArg1, const void* aArg2, void *data)
{
  return (*((nsFontXlib**) aArg1))->mSize - (*((nsFontXlib**) aArg2))->mSize;
}

void
nsFontStretchXlib::SortSizes(void)
{
  NS_QuickSort(mSizes, mSizesCount, sizeof(*mSizes), CompareSizes, NULL);
}

void
nsFontWeightXlib::FillStretchHoles(void)
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
nsFontStyleXlib::FillWeightHoles(void)
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
nsFontNodeXlib::FillStyleHoles(void)
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

static void
SetCharsetLangGroup(nsFontCharSetXlibInfo* aCharSetInfo)
{
  if (!aCharSetInfo->mCharSet || aCharSetInfo->mLangGroup)
    return;

  nsCOMPtr<nsIAtom> charset;
  nsresult res = gCharSetManager->GetCharsetAtom2(aCharSetInfo->mCharSet,
                                                  getter_AddRefs(charset));
                                             
  NS_ASSERTION(NS_SUCCEEDED(res), "gCharSetManager->GetCharsetAtom2() failed.");
                                             
  if (NS_SUCCEEDED(res)) {
    res = gCharSetManager->GetCharsetLangGroup(charset,
                                               &aCharSetInfo->mLangGroup);

    // NS_ASSERTION(NS_SUCCEEDED(res), "gCharSetManager->GetCharsetLangGroup() failed.");

    if (NS_FAILED(res)) {
      aCharSetInfo->mLangGroup = NS_NewAtom("");
#ifdef DEBUG
      printf("=== cannot get lang group for %s\n", aCharSetInfo->mCharSet);
#endif
    }
  }
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

nsFontXlib*
nsFontMetricsXlib::SearchNode(nsFontNodeXlib* aNode, PRUnichar aChar)
{
  if (aNode->mDummy)
    return nsnull;

  nsFontCharSetXlibInfo* charSetInfo = aNode->mCharSetInfo;

  /*
   * mCharSet is set if we know which glyphs will be found in these fonts.
   * If mCCMap has already been created for this charset, we compare it with
   * the mCCMaps of the previously loaded fonts. If it is the same as any of
   * the previous ones, we return nsnull because there is no point in
   * loading a font with the same map.
   */
  if (charSetInfo->mCharSet) {
    PRUint16* ccmap = charSetInfo->mCCMap;
    if (ccmap) {
      for (int i = 0; i < mLoadedFontsCount; i++) {
        if (mLoadedFonts[i]->mCCMap == ccmap) {
          return nsnull;
        }
      }
    }
    else {
      SetUpFontCharSetInfo(charSetInfo);
      if (!charSetInfo->mCCMap)
        return nsnull;
    }
  }
  else {
    if ((!mIsUserDefined) && (charSetInfo == &Unknown)) {
      return nsnull;
    }
  }

  aNode->FillStyleHoles();
  nsFontStyleXlib* style = aNode->mStyles[mStyleIndex];

  nsFontWeightXlib** weights = style->mWeights;
  int weight = mFont->weight;
  int steps = (weight % 100);
  int weightIndex;
  if (steps) {
    if (steps < 10) {
      int base = (weight - steps);
      GET_WEIGHT_INDEX(weightIndex, base);
      while (steps--) {
        nsFontWeightXlib* prev = weights[weightIndex];
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
      int base = (weight + steps);
      GET_WEIGHT_INDEX(weightIndex, base);
      while (steps--) {
        nsFontWeightXlib* prev = weights[weightIndex];
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

  return PickASizeAndLoad(weights[weightIndex]->mStretches[mStretchIndex],
    charSetInfo, aChar);
}

static void 
SetFontLangGroupInfo(nsFontCharSetMapXlib* aCharSetMap)
{
  nsFontLangGroup *fontLangGroup = aCharSetMap->mFontLangGroup;
  if (!fontLangGroup)
    return;

  // get the atom for mFontLangGroup->mFontLangGroupName so we can
  // apply fontLangGroup operations to it
  // eg: search for related groups, check for scaling prefs
    const char *langGroup = fontLangGroup->mFontLangGroupName;
    if (!langGroup)
      langGroup = "";
    if (!fontLangGroup->mFontLangGroupAtom) {
      fontLangGroup->mFontLangGroupAtom = NS_NewAtom(langGroup);
    }
  
    // get the font scaling controls
    nsFontCharSetXlibInfo *charSetInfo = aCharSetMap->mInfo;
    if (!charSetInfo->mInitedSizeInfo) {
      charSetInfo->mInitedSizeInfo = PR_TRUE;

      nsCAutoString name;
      name.Assign("font.scale.outline.min.");
      name.Append(langGroup);
      gPref->GetIntPref(name.get(), &charSetInfo->mOutlineScaleMin);
      if (!charSetInfo->mOutlineScaleMin)
        charSetInfo->mOutlineScaleMin = gOutlineScaleMinimum;

      name.Assign("font.scale.bitmap.min.");
      name.Append(langGroup);
      gPref->GetIntPref(name.get(), &charSetInfo->mBitmapScaleMin);
      if (!charSetInfo->mBitmapScaleMin)
        charSetInfo->mBitmapScaleMin = gBitmapScaleMinimum;

      PRInt32 percent = 0;
      name.Assign("font.scale.bitmap.oversize.");
      name.Append(langGroup);
      gPref->GetIntPref(name.get(), &percent);
      if (percent)
        charSetInfo->mBitmapOversize = percent / 100.0;
      else
        charSetInfo->mBitmapOversize = gBitmapOversize;

      percent = 0;
      name.Assign("font.scale.bitmap.undersize.");
      name.Append(langGroup);
      gPref->GetIntPref(name.get(), &percent);
      if (percent)
        charSetInfo->mBitmapUndersize = percent / 100.0;
      else
        charSetInfo->mBitmapUndersize = gBitmapUndersize;
    }
}

static void
GetFontNames(const char* aPattern, nsFontNodeArrayXlib* aNodes)
{
  nsCAutoString previousNodeName;
  Display       *dpy = xxlib_rgb_get_display(gXlibRgbHandle);

#ifdef USE_XPRINT
#ifdef DEBUG
  if(nsFontMetricsXlib::mPrinterMode)
  {
    if (!dpy)
    {
      NS_ERROR("Obtaining font information without having a |Display *|");
      abort(); /* DIE!! */
    }
  
    if (XpGetContext(dpy) == None)
    {
      /* applications must not make any assumptions about fonts _before_ XpSetContext() !!! */
      NS_ERROR("Obtaining font information without valid print context (XListFonts()) _before_ XpSetContext()");
      abort(); /* DIE!! */
    }    
  }               
#endif /* DEBUG */
#endif /* USE_XPRINT */
  int count;
  char** list = ::XListFonts(dpy, aPattern, INT_MAX, &count);
  if ((!list) || (count < 1)) {
    return;
  }

  for (int i = 0; i < count; i++) {
    char* name = list[i];
    if ((!name) || (name[0] != '-')) {
      continue;
    }
    char* p = name + 1;
    int scalable = 0;
    PRBool outline_scaled = PR_FALSE;

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

    FIND_FIELD(foundry);
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
    // check if bitmap non-scaled font
    if ((pixelSize[0] != '0') || (pointSize[0] != '0')) {
      /* ugh */
    }
    // check if bitmap scaled font
    else if ((pixelSize[0] == '0') && (pointSize[0] == '0')
        && (resolutionX[0] != '0') && (resolutionY[0] != '0')) {
      /* ugh */
    }
    // check if outline scaled font
    else if ((pixelSize[0] == '0') && (pointSize[0] == '0')
        && (resolutionX[0] == '0') && (resolutionY[0] == '0')) {
      outline_scaled = PR_TRUE;
    }
    else {
      static PRBool already_complained = PR_FALSE;
      if (!already_complained) {
        already_complained = PR_TRUE;
        NS_ASSERTION(pixelSize[0] == '0', "font scaler type test failed");
        NS_ASSERTION(pointSize[0] == '0', "font scaler type test failed");
        NS_ASSERTION(resolutionX[0] == '0', "font scaler type test failed");
        NS_ASSERTION(resolutionY[0] == '0', "font scaler type test failed");
      }
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
    
    nsCStringKey charSetKey(charSetName);
    nsFontCharSetMapXlib *charSetMap =
      (nsFontCharSetMapXlib *) gCharSetMaps->Get(&charSetKey);
    if (!charSetMap)
      charSetMap = gNoneCharSetMap;

    nsFontCharSetXlibInfo* charSetInfo = charSetMap->mInfo;
    // indirection for font specific charset encoding
    if (charSetInfo == &Special) {
      nsCAutoString familyCharSetName(familyName);
      familyCharSetName.Append('-');
      familyCharSetName.Append(charSetName);
      nsCStringKey familyCharSetKey(familyCharSetName);
      charSetMap = NS_STATIC_CAST(nsFontCharSetMapXlib*, gSpecialCharSets->Get(&familyCharSetKey));
      if (!charSetMap)
        charSetMap = gNoneCharSetMap;
      charSetInfo = charSetMap->mInfo;
    }

    if (!charSetInfo) {
      PR_LOG(FontMetricsXlibLM, PR_LOG_DEBUG, ("cannot find charset %s\n", charSetName));
      charSetInfo = &Unknown;
    }
    SetCharsetLangGroup(charSetInfo);
    SetFontLangGroupInfo(charSetMap);

    nsCAutoString nodeName(foundry);
    nodeName.Append('-');
    nodeName.Append(familyName);
    nodeName.Append('-');
    nodeName.Append(charSetName);

    nsCStringKey key(nodeName);
    nsFontNodeXlib* node = (nsFontNodeXlib*) gNodes->Get(&key);

    if (!node) {
      node = new nsFontNodeXlib;
      if (!node)
        continue;
      gNodes->Put(&key, node);
      node->mName = nodeName;
      node->mCharSetInfo = charSetInfo;
    }

    int found = 0;
    if (nodeName == previousNodeName)
      found = 1;
    else
    {
      found = (aNodes->IndexOf(node) >= 0);
    }

    previousNodeName = nodeName;
    if (!found)
      aNodes->AppendElement(node);

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
    nsFontStyleXlib* style = node->mStyles[styleIndex];
    if (!style) {
      style = new nsFontStyleXlib;
      if (!style) {
        continue;
      }
      node->mStyles[styleIndex] = style;
    }

    nsCStringKey weightKey(weightName);
    int weightNumber = NS_PTR_TO_INT32(gWeights->Get(&weightKey));

    if (!weightNumber) {
      PR_LOG(FontMetricsXlibLM, PR_LOG_DEBUG, ("cannot find weight %s\n", weightName));
      weightNumber = NS_FONT_WEIGHT_NORMAL;
    }
    int weightIndex = WEIGHT_INDEX(weightNumber);
    nsFontWeightXlib* weight = style->mWeights[weightIndex];
    if (!weight) {
      weight = new nsFontWeightXlib;
      if (!weight) {
        continue;
      }
      style->mWeights[weightIndex] = weight;
    }

    nsCStringKey setWidthKey(setWidth);
    int stretchIndex = NS_PTR_TO_INT32(gStretches->Get(&setWidthKey));

    if (!stretchIndex) {
      PR_LOG(FontMetricsXlibLM, PR_LOG_DEBUG, ("cannot find stretch %s\n", setWidth));
      stretchIndex = 5;
    }
    stretchIndex--;
    nsFontStretchXlib* stretch = weight->mStretches[stretchIndex];
    if (!stretch) {
      stretch = new nsFontStretchXlib;
      if (!stretch) {
        continue;
      }
      weight->mStretches[stretchIndex] = stretch;
    }
    if (scalable) {
/* gisburn: disabled for Xprint - this kills printer buildin fonts
 * Xprint printer-buildin fonts look like bitmap scaled fonts but are 
 * (scaleable) printer-buildin fonts in reality.
 */
#ifdef USE_XPRINT
      if(!nsFontMetricsXlib::mPrinterMode)
#endif /* USE_XPRINT */      
      {
        // if we have both an outline scaled font and a bitmap 
        // scaled font pick the outline scaled font
        if ((stretch->mScalable) && (!stretch->mOutlineScaled)
            && (outline_scaled)) {
          PR_smprintf_free(stretch->mScalable);
          stretch->mScalable = nsnull;
        }
      }
      
      if (!stretch->mScalable) {
        stretch->mOutlineScaled = outline_scaled;
        if (outline_scaled) {
          stretch->mScalable = PR_smprintf("%s-%s-%s-%s-%s-%s-%%d-*-0-0-%s-*-%s",
          name, familyName, weightName, slant, setWidth, addStyle, spacing, charSetName);
        }
        else {
          stretch->mScalable =
            PR_smprintf("%s-%s-%s-%s-%s-%s-%%d-*-*-*-%s-*-%s",
            name, familyName, weightName, slant, setWidth, addStyle,
            spacing, charSetName);
        }
      }
      continue;
    }

    int pixels = atoi(pixelSize);
    NS_ASSERTION((pixels > 0), "unexpected pixel size");
    
    if (stretch->mSizesCount) {
      nsFontXlib** end = &stretch->mSizes[stretch->mSizesCount];
      nsFontXlib** s;
      for (s = stretch->mSizes; s < end; s++) {
        if ((*s)->mSize == pixels) {
          break;
        }
      }
      if (s != end) {
        continue;
      }
    }
    if (stretch->mSizesCount == stretch->mSizesAlloc) {
      int newSize = 2 * (stretch->mSizesAlloc ? stretch->mSizesAlloc : 1);
      nsFontXlib** newPointer = new nsFontXlib*[newSize];
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

    char* copy = PR_smprintf("%s", name);

    if (!copy) {
      continue;
    }
    nsFontXlib* size = new nsFontXlibNormal();
    if (!size)
      continue;

    stretch->mSizes[stretch->mSizesCount++] = size;

    size->mName = copy;
    size->mFont = nsnull;
    size->mSize = pixels;
    size->mBaselineAdjust = 0;
    size->mCCMap = nsnull;
    size->mCharSetInfo = charSetInfo;
  }
  XFreeFontNames(list);

#ifdef DEBUG_DUMP_TREE
  DumpTree();
#endif
}

static nsresult
GetAllFontNames(void)
{
  if (!gGlobalList) {
    // This may well expand further (families * sizes * styles?), but it's
    // only created once
    gGlobalList = new nsFontNodeArrayXlib;
    if (!gGlobalList) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    GetFontNames("-*-*-*-*-*-*-*-*-*-*-*-*-*-*", gGlobalList);
  }

  return NS_OK;
}

static nsFontFamilyXlib*
FindFamily(nsCString* aName)
{
  nsCStringKey key(*aName);
  nsFontFamilyXlib* family = (nsFontFamilyXlib*) gFamilies->Get(&key);
  if (!family) {
    family = new nsFontFamilyXlib();
    if (family) {
      char pattern[256];
      PR_snprintf(pattern, sizeof(pattern), "-*-%s-*-*-*-*-*-*-*-*-*-*-*-*",
        aName->get());
      GetFontNames(pattern, &family->mNodes);
      gFamilies->Put(&key, family);
    }
  }

  return family;
}

nsresult
nsFontMetricsXlib::FamilyExists(nsIDeviceContext *aDevice, const nsString& aName)
{
  if (!gInitialized) {
    nsresult res = InitGlobals(aDevice);
    if (NS_FAILED(res))
      return res;
  }
  
  if (!IsASCIIFontName(aName))
    return NS_ERROR_FAILURE;

  nsCAutoString name;
  name.AssignWithConversion(aName.get());
  name.ToLowerCase();
  
  nsFontFamilyXlib* family = FindFamily(&name);
  if (family && family->mNodes.Count())
    return NS_OK;

  return NS_ERROR_FAILURE;
}

/* convert a FFRE (Foundry-Family-Registry-Encoding) To XLFD Pattern */
static void
FFREToXLFDPattern(nsAWritableCString &aFFREName, nsAWritableCString &oPattern)
{
  oPattern.Append("-");
  oPattern.Append(aFFREName);
  PRInt32 charsetHyphen = oPattern.FindChar('-');
  charsetHyphen = oPattern.FindChar('-', charsetHyphen + 1);
  charsetHyphen = oPattern.FindChar('-', charsetHyphen + 1);
  oPattern.Insert("-*-*-*-*-*-*-*-*-*-*", charsetHyphen);
}

/* substitute the charset in a FFRE (Foundry-Family-Registry-Encoding) */
static void
FFRESubstituteCharset(nsAWritableCString &aFFREName, const char *aReplacementCharset)
{
  PRInt32 charsetHyphen = aFFREName.FindChar('-');
  charsetHyphen = aFFREName.FindChar('-', charsetHyphen + 1);
  aFFREName.Truncate(charsetHyphen+1);
  aFFREName.Append(aReplacementCharset);
}

/* substitute the encoding in a FFRE (Foundry-Family-Registry-Encoding) */
static void
FFRESubstituteEncoding(nsAWritableCString &aFFREName, const char *aReplacementEncoding)
{
  PRInt32 encodingHyphen = aFFREName.FindChar('-');
  encodingHyphen = aFFREName.FindChar('-', encodingHyphen + 1);
  encodingHyphen = aFFREName.FindChar('-', encodingHyphen + 1);
  aFFREName.Truncate(encodingHyphen+1);
  aFFREName.Append(aReplacementEncoding);
}

nsFontXlib*
nsFontMetricsXlib::TryNodes(nsAWritableCString &aFFREName, PRUnichar aChar)
{
  nsCStringKey key(PromiseFlatCString(aFFREName).get());
  nsFontNodeArrayXlib* nodes = (nsFontNodeArrayXlib*) gCachedFFRESearches->Get(&key);
  if (!nodes) {
    nsCAutoString pattern;
    FFREToXLFDPattern(aFFREName, pattern);
    nodes = new nsFontNodeArrayXlib;
    if (!nodes)
      return nsnull;
    GetFontNames(pattern.get(), nodes);
    gCachedFFRESearches->Put(&key, nodes);
  }
  int i, cnt = nodes->Count();
  for (i=0; i<cnt; i++) {
    nsFontNodeXlib* node = nodes->GetElement(i);
    nsFontXlib* font;
    font = SearchNode(node, aChar);
    if (font && font->SupportsChar(aChar))
      return font;
  }
  return nsnull;
}

nsFontXlib*
nsFontMetricsXlib::TryNode(nsCString* aName, PRUnichar aChar)
{
  /* check the specified font (foundry-family-registry-encoding) */
  nsFontXlib* font;
 
  nsCStringKey key(*aName);
  nsFontNodeXlib* node = (nsFontNodeXlib*) gNodes->Get(&key);
  if (!node) {
    nsCAutoString pattern;
    FFREToXLFDPattern(*aName, pattern);
    nsFontNodeArrayXlib nodes;
    GetFontNames(pattern.get(), &nodes);
    // no need to call gNodes->Put() since GetFontNames already did
    if (nodes.Count() > 0) {
      // XXX This assertion may be spurious; you can have more than
      // -*-courier-iso8859-1 font, for example, from different
      // foundries.    
      NS_ASSERTION((nodes.Count() == 1), "unexpected number of nodes");
      node = nodes.GetElement(0);
    }
    else {
      // add a dummy node to the hash table to avoid calling XListFonts again
      node = new nsFontNodeXlib();
      if (!node) {
        return nsnull;
      }
      gNodes->Put(&key, node);
      node->mDummy = 1;
    }
  }

  if (node) {
    font = SearchNode(node, aChar);
    if (font && font->SupportsChar(aChar))
      return font;
  }

  /* do not check related sub-planes for UserDefined */
  if (mIsUserDefined) {
    return nsnull;
  }
  /* check related sub-planes (wild-card the encoding) */
  nsCAutoString ffreName(*aName);
  FFRESubstituteEncoding(ffreName, "*");
  font = TryNodes(ffreName, aChar);
  if (font) {
    NS_ASSERTION(font->SupportsChar(aChar), "font supposed to support this char");
    return font;
  }
  return nsnull;
}

nsFontXlib* 
nsFontMetricsXlib::TryLangGroup(nsIAtom* aLangGroup, nsCString* aName, PRUnichar aChar)
{
  /* for this family check related registry-encoding (for the language) */
  nsFontXlib* font = FindLangGroupFont(aLangGroup, aChar, aName);
  return font;
}

nsFontXlib*
nsFontMetricsXlib::TryFamily(nsCString* aName, PRUnichar aChar)
{
  //
  // check the patterh "*-familyname-registry-encoding" for language
  //
  nsFontFamilyXlib* family = FindFamily(aName);
  if (family) {
    /* try family name of language group first */
    nsCAutoString FFREName("*-");
    FFREName.Append(*aName);
    FFREName.Append("-*-*");
    nsFontXlib* font = TryLangGroup(mLangGroup, &FFREName, aChar);
    if (font)
      return font;

    /* then try family name regardless of language group */
    nsFontNodeArrayXlib* nodes = &family->mNodes;
    PRInt32 n = nodes->Count();
    for (PRInt32 i = 0; i < n; i++) {
      nsFontXlib* font = SearchNode(nodes->GetElement(i), aChar);
      if (font && font->SupportsChar(aChar)) {
        return font;
      }
    }
  }

  return nsnull;
}

nsFontXlib*
nsFontMetricsXlib::TryAliases(nsCString* aAlias, PRUnichar aChar)
{
  nsCStringKey key(*aAlias);
  char* name = (char*) gAliases->Get(&key);
  if (name) {
    nsCAutoString str(name);
    return TryFamily(&str, aChar);
  }

  return nsnull;
}

nsFontXlib*
nsFontMetricsXlib::FindUserDefinedFont(PRUnichar aChar)
{
  if (mIsUserDefined) {
    nsFontXlib* font = TryNode(&mUserDefined, aChar);
    if (font) {
      NS_ASSERTION(font->SupportsChar(aChar), "font supposed to support this char");
      return font;
    }
  }

  return nsnull;
}

nsFontXlib*
nsFontMetricsXlib::FindStyleSheetSpecificFont(PRUnichar aChar)
{
  while (mFontsIndex < mFonts.Count()) {
    if (mFontIsGeneric[mFontsIndex]) {
      return nsnull;
    }
    nsCString* familyName = mFonts.CStringAt(mFontsIndex);

    /*
     * count hyphens
     * XXX It might be good to try to pre-cache this information instead
     * XXX of recalculating it on every font access!
     */
    const char* str = familyName->get();
    PRUint32 len = familyName->Length();
    int hyphens = 0;
    for (PRUint32 i = 0; i < len; i++) {
      if (str[i] == '-') {
        hyphens++;
      }
    }

    /*
     * if there are 3 hyphens, the name is in FFRE form
     * (foundry-family-registry-encoding)
     * ie: something like this:
     *
     *   adobe-times-iso8859-1
     *
     * otherwise it is something like
     *
     *   times new roman
     */
    nsFontXlib* font;
    if (hyphens == 3) {
      font = TryNode(familyName, aChar);
      if (font) {
        NS_ASSERTION(font->SupportsChar(aChar), "font supposed to support this char");
        return font;
      }
      font = TryLangGroup(mLangGroup, familyName, aChar);
      if (font) {
        NS_ASSERTION(font->SupportsChar(aChar), "font supposed to support this char");
        return font;
      }
    }
    else {
      font = TryFamily(familyName, aChar);
      if (font) {
        NS_ASSERTION(font->SupportsChar(aChar), "font supposed to support this char");
        return font;
      }
      font = TryAliases(familyName, aChar);
      if (font) {
        NS_ASSERTION(font->SupportsChar(aChar), "font supposed to support this char");
        return font;
      }
    }
    // bug 42917: increment only after all of the above fails
    mFontsIndex++;
  }

  return nsnull;
}

static void
PrefEnumCallback(const char* aName, void* aClosure)
{
  nsFontSearch* s = (nsFontSearch*) aClosure;
  if (s->mFont) {
    NS_ASSERTION(s->mFont->SupportsChar(s->mChar), "font supposed to support this char");
    return;
  }
  nsXPIDLCString value;
  gPref->CopyCharPref(aName, getter_Copies(value));
  nsCAutoString name;
  if (value) {
    name = value;
    s->mFont = s->mMetrics->TryNode(&name, s->mChar);
    if (s->mFont) {
      NS_ASSERTION(s->mFont->SupportsChar(s->mChar), "font supposed to support this char");
      return;
    }
  }
  s->mFont = s->mMetrics->TryLangGroup(s->mMetrics->mLangGroup, &name, s->mChar);
  if (s->mFont) {
    NS_ASSERTION(s->mFont->SupportsChar(s->mChar), "font supposed to support this char");
    return;
  }
  gPref->CopyDefaultCharPref(aName, getter_Copies(value));
  if ((value) && (!name.Equals(value))) {
    name = value;
    s->mFont = s->mMetrics->TryNode(&name, s->mChar);
    if (s->mFont) {
      NS_ASSERTION(s->mFont->SupportsChar(s->mChar), "font supposed to support this char");
      return;
    }
    s->mFont = s->mMetrics->TryLangGroup(s->mMetrics->mLangGroup, &name, s->mChar);
    NS_ASSERTION(s->mFont ? s->mFont->SupportsChar(s->mChar) : 1, "font supposed to support this char");
  }
}

nsFontXlib*
nsFontMetricsXlib::FindStyleSheetGenericFont(PRUnichar aChar)
{
  nsFontXlib* font;

  if (mTriedAllGenerics) {
    return nsnull;
  }

  /* find font based on document's lang group */
  font = FindLangGroupPrefFont(mLangGroup, aChar);
  if (font) {
    NS_ASSERTION(font->SupportsChar(aChar), "font supposed to support this char");
    return font;
  }

  //
  // Asian smart quote glyphs are much too large for western
  // documents so if this is a single byte document add a
  // special "font" to tranliterate those chars rather than
  // possibly find them in double byte fonts
  //
  // (risk management: since we are close to a ship point we have a 
  //  control (gAllowDoubleByteSpecialChars) to disable this new feature)
  //
  if (gAllowDoubleByteSpecialChars) {
    if (!mDocConverterType) {
      if (mLoadedFontsCount) {
        nsFontXlib* first_font = mLoadedFonts[0];
        if (first_font->mCharSetInfo) {
          mDocConverterType = first_font->mCharSetInfo->Convert;
        }
      }
      if (!mDocConverterType) {
        NS_ASSERTION(mDocConverterType!=nsnull, "failed to get converter type");
        mDocConverterType = SingleByteConvert;
      }
      if (mDocConverterType == SingleByteConvert) {
        // before we put in the transliterator to disable double byte special chars
        // add the x-western font before the early transliterator
        // to get the EURO sign (hack)
        nsFontXlib* western_font = nsnull;
        if (mLangGroup != gWesternLocale)
          western_font = FindLangGroupPrefFont(gWesternLocale, aChar);
        // add the symbol font before the early transliterator
        // to get the bullet (hack)
        nsCAutoString ffre("*-symbol-adobe-fontspecific");
        nsFontXlib* symbol_font = TryNodes(ffre, 0x0030);

        // add the early transliterator
        // to avoid getting Japanese "special chars" such as smart
        // since they are very oversized compared to western fonts
        nsFontXlib* sub_font = FindSubstituteFont(aChar);
        NS_ASSERTION(sub_font, "failed to get a special chars substitute font");
        if (sub_font) {
          sub_font->mCCMap = gDoubleByteSpecialCharsCCMap;
          AddToLoadedFontsList(sub_font);
        }
        if (western_font && CCMAP_HAS_CHAR(western_font->mCCMap, aChar)) {
          return western_font;
        }
        else if (symbol_font && CCMAP_HAS_CHAR(symbol_font->mCCMap, aChar)) {
          return symbol_font;
        }
        else if (sub_font && CCMAP_HAS_CHAR(sub_font->mCCMap, aChar)) {
          return sub_font;
        }
      }
    }
  }

  /* find font based on user's locale's lang group
   * if different from documents locale */
  if (gUsersLocale != mLangGroup) {
    font = FindLangGroupPrefFont(gUsersLocale, aChar);
    if (font) {
      NS_ASSERTION(font->SupportsChar(aChar), "font supposed to support this char");
      return font;
    }
  }

  // If this is is the 'unknown' char (ie: converter could not
  // convert it) there is no sense in searching any further for
  // a font.  This test shows up in several locations; if we did
  // this test earlier in the search we would only need to do it
  // once but we don't want to slow down the typical search.
  if (aChar == UCS2_NOMAPPING) {
    return nsnull;
  }

  /* Search all font prefs for generic */
  nsCAutoString prefix("font.name.");
  prefix.Append(*mGeneric);
  nsFontSearch search = { this, aChar, nsnull };
  gPref->EnumerateChildren(prefix.get(), PrefEnumCallback, &search);
  if (search.mFont) {
    NS_ASSERTION(search.mFont->SupportsChar(aChar), "font supposed to support this char");
    return search.mFont;
  }

  /* Search all font prefs
   * find based on all prefs (no generic part (eg: sans-serif)) */
  nsCAutoString allPrefs("font.name.");
  search.mFont = nsnull;
  gPref->EnumerateChildren(allPrefs.get(), PrefEnumCallback, &search);
  if (search.mFont) {
    NS_ASSERTION(search.mFont->SupportsChar(aChar), "font supposed to support this char");
    return search.mFont;
  }

  mTriedAllGenerics = 1;
  return nsnull;
}

nsFontXlib*
nsFontMetricsXlib::FindAnyFont(PRUnichar aChar)
{
  // If this is is the 'unknown' char (ie: converter could not 
  // convert it) there is no sense in searching any further for 
  // a font.  This test shows up in several locations; if we did
  // this test earlier in the search we would only need to do it
  // once but we don't want to slow down the typical search.
  if (aChar == UCS2_NOMAPPING) {
    return nsnull;
  }

  // XXX If we get to this point, that means that we have exhausted all the
  // families in the lists. Maybe we should try a list of fonts that are
  // specific to the vendor of the X server here. Because XListFonts for the
  // whole list is very expensive on some Unixes.

  /* Try all the fonts on the system. */
  nsresult res = GetAllFontNames();
  if (NS_FAILED(res)) {
    return nsnull;
  }

  PRInt32 n = gGlobalList->Count();
  for (PRInt32 i = 0; i < n; i++) {
    nsFontXlib* font = SearchNode(gGlobalList->GetElement(i), aChar);
    if (font && font->SupportsChar(aChar)) {
      // XXX We should probably write this family name out to disk, so that
      // we can use it next time. I.e. prefs file or something.
      return font;
    }
  }

  // future work:
  // to properly support the substitute font we
  // need to indicate here that all fonts have been tried
  return nsnull;
}

nsFontXlib*
nsFontMetricsXlib::FindSubstituteFont(PRUnichar aChar)
{
  if (!mSubstituteFont) {
    for (int i = 0; i < mLoadedFontsCount; i++) {
      if (CCMAP_HAS_CHAR(mLoadedFonts[i]->mCCMap, 'a')) {
        mSubstituteFont = new nsFontXlibSubstitute(mLoadedFonts[i]);
        break;
      }
    }
    // Currently the substitute font does not have a glyph map.
    // This means that even if we have already checked all fonts
    // for a particular character the mLoadedFonts will not know it.
    // Thus we reparse *all* font glyph maps every time we see
    // a character that ends up using a substitute font.
    // future work:
    // create an empty mCCMap and every time we determine a
    // character will get its "glyph" from the substitute font
    // mark that character in the mCCMap.
   }
  // mark the mCCMap to indicate that this character has a "glyph"

  // If we know that mLoadedFonts has every font's glyph map loaded
  // then we can now set all the bit in the substitute font's glyph map
  // and thus direct all umapped characters to the substitute
  // font (without the font search).
  // if tried all glyphs {
  //   create a substitute font with all bits set
  //   set all bits in mCCMap
  // }

  return mSubstituteFont;
}

nsFontXlib* 
nsFontMetricsXlib::FindLangGroupPrefFont(nsIAtom* aLangGroup, PRUnichar aChar)
{ 
  nsFontXlib* font;

  /* get the font specified in prefs */
  nsCAutoString prefix("font.name."); 
  prefix.Append(*mGeneric); 
  if (aLangGroup) { 
    // check user set pref
    nsCAutoString pref = prefix;
    pref.Append(char('.'));
    const PRUnichar* langGroup = nsnull;
    aLangGroup->GetUnicode(&langGroup);
    pref.AppendWithConversion(langGroup);
    nsXPIDLCString value;
    gPref->CopyCharPref(pref.get(), getter_Copies(value));
    nsCAutoString str;
    nsCAutoString str_user;
    if (value.get()) {
      str = value.get();
      str_user = value.get();
      font = TryNode(&str, aChar);
      if (font) {
        NS_ASSERTION(font->SupportsChar(aChar), "font supposed to support this char");
        return font;
      }
      font = TryLangGroup(aLangGroup, &str, aChar);
      if (font) {
        NS_ASSERTION(font->SupportsChar(aChar), "font supposed to support this char");
        return font;
      }
    }
    // check factory set pref
    gPref->CopyDefaultCharPref(pref.get(), getter_Copies(value));
    if (value.get()) {
      str = value.get();
      // check if we already tried this name
      if (str != str_user) {
        font = TryNode(&str, aChar);
        if (font) {
          NS_ASSERTION(font->SupportsChar(aChar), "font supposed to support this char");
          return font;
        }
        font = TryLangGroup(aLangGroup, &str, aChar);
        if (font) {
          NS_ASSERTION(font->SupportsChar(aChar), "font supposed to support this char");
          return font;
        }
      }
    }
  }

  //
  // find any style font based on lang group
  //
  font = FindLangGroupFont(aLangGroup, aChar, nsnull);
  if (font) {
    NS_ASSERTION(font->SupportsChar(aChar), "font supposed to support this char");
    return font;
  }

  return nsnull;
}

nsFontXlib*
nsFontMetricsXlib::FindLangGroupFont(nsIAtom* aLangGroup, PRUnichar aChar, nsCString *aName)
{
  nsFontXlib* font;

  /* scan gCharSetMap for encodings with matching lang groups */
  nsFontCharSetMapXlib* charSetMap;
  for (charSetMap = gCharSetMap; charSetMap->mName; charSetMap++) {
    nsFontLangGroup *mFontLangGroup = charSetMap->mFontLangGroup;

    if ((!mFontLangGroup) || (!mFontLangGroup->mFontLangGroupName))
      continue;

    if (!charSetMap->mInfo->mLangGroup) {
      SetCharsetLangGroup(charSetMap->mInfo);
    }

    if (!mFontLangGroup->mFontLangGroupAtom)
      SetFontLangGroupInfo(charSetMap);

    if ((aLangGroup != mFontLangGroup->mFontLangGroupAtom)
        && (aLangGroup != charSetMap->mInfo->mLangGroup))
      continue;

    // look for a font with this charset (registry-encoding) & char
    //
    nsCAutoString ffreName("");
    if (aName) {
      // if aName was specified so call TryNode() not TryNodes()
      ffreName.Append(*aName);
      FFRESubstituteCharset(ffreName, charSetMap->mName); 
      if(aName->First() == '*') {
         // called from TryFamily()
         font = TryNodes(ffreName, aChar);
      } else {
         font = TryNode(&ffreName, aChar);
      }      
      NS_ASSERTION(font ? font->SupportsChar(aChar) : 1, "font supposed to support this char");
    } else {
      // no name was specified so call TryNodes() for this charset
      ffreName.Append("*-*-*-*");
      FFRESubstituteCharset(ffreName, charSetMap->mName); 
      font = TryNodes(ffreName, aChar);
      NS_ASSERTION(font ? font->SupportsChar(aChar) : 1, "font supposed to support this char");
    }
    if (font) {
      NS_ASSERTION(font->SupportsChar(aChar), "font supposed to support this char");
      return font;
    }
  }

  return nsnull;
}

/*
 * First we try to load the user-defined font, if the user-defined charset
 * has been selected in the menu.
 *
 * Next, we try the fonts listed in the font-family property (FindStyleSheetSpecificFont).
 *
 * Next, we try any CSS generic font encountered in the font-family list and
 * all of the fonts specified by the user for the generic (FindStyleSheetGenericFont).
 *
 * Next, we try all of the fonts on the system (FindAnyFont). This is
 * expensive on some Unixes.
 *
 * Finally, we try to create a substitute font that offers substitute glyphs
 * for the characters (FindSubstituteFont).
 */

nsFontXlib*
nsFontMetricsXlib::FindFont(PRUnichar aChar)
{
  nsFontXlib* font = FindUserDefinedFont(aChar);

  if (!font) {
    font = FindStyleSheetSpecificFont(aChar);
    if (!font) {
      font = FindStyleSheetGenericFont(aChar);
      if (!font) {
        font = FindAnyFont(aChar);
        if (!font) {
          font = FindSubstituteFont(aChar);
        }
      }
    }
  }

#ifdef NS_FONT_DEBUG_CALL_TRACE
  if (gDebug & NS_FONT_DEBUG_CALL_TRACE) {
    printf("FindFont(%04X)[", aChar);
    for (PRInt32 i = 0; i < mFonts.Count(); i++) {
      printf("%s, ", mFonts.CStringAt(i)->get());
    }
    printf("]\nreturns ");
    if (font) {
      printf("%s\n", font->mName ? font->mName : "(substitute)");
    }
    else {
      printf("NULL\n");
    }
  }
#endif

  return font;
}

nsresult
nsFontMetricsXlib::GetSpaceWidth(nscoord &aSpaceWidth)
{
  aSpaceWidth = mSpaceWidth;
  return NS_OK;
}

// The Font Enumerator

nsFontEnumeratorXlib::nsFontEnumeratorXlib()
{
  NS_INIT_REFCNT();
}

NS_IMPL_ISUPPORTS1(nsFontEnumeratorXlib, nsIFontEnumerator)

typedef struct EnumerateNodeInfo
{
  PRUnichar** mArray;
  int         mIndex;
  nsIAtom*    mLangGroup;
} EnumerateNodeInfo;

static PRIntn
EnumerateNode(void* aElement, void* aData)
{
  nsFontNodeXlib* node = (nsFontNodeXlib*) aElement;
  EnumerateNodeInfo* info = (EnumerateNodeInfo*) aData;
  if (info->mLangGroup != gUserDefined) {
    if (node->mCharSetInfo == &Unknown) {
      return PR_TRUE; // continue
    }
    else if (info->mLangGroup != gUnicode) {
      if (node->mCharSetInfo->mLangGroup != info->mLangGroup) {
        return PR_TRUE; // continue
      }
    }
    // else {
    //   if (lang == add-style-field) {
    //     consider it part of the lang group
    //   }
    //   else if (a Unicode font reports its lang group) {
    //     consider it part of the lang group
    //   }
    //   else if (lang's ranges in list of ranges) {
    //     consider it part of the lang group
    //     // Note: at present we have no way to do this test but we 
    //     // could in the future and this would be the place to enable
    //     // to make the font show up in the preferences dialog
    //   }
    // }

  }
  PRUnichar** array = info->mArray;
  int j = info->mIndex;
  PRUnichar* str = node->mName.ToNewUnicode();
  if (!str) {
    for (j = j - 1; j >= 0; j--) {
      nsMemory::Free(array[j]);
    }
    info->mIndex = 0;
    return PR_FALSE; // stop
  }
  array[j] = str;
  info->mIndex++;

  return PR_TRUE; // continue
}

static int
CompareFontNames(const void* aArg1, const void* aArg2, void* aClosure)
{
  const PRUnichar* str1 = *((const PRUnichar**) aArg1);
  const PRUnichar* str2 = *((const PRUnichar**) aArg2);

  // XXX add nsICollation stuff

  return nsCRT::strcmp(str1, str2);
}

static nsresult
EnumFonts(nsIAtom* aLangGroup, const char* aGeneric, PRUint32* aCount,
  PRUnichar*** aResult)
{
  nsresult res = GetAllFontNames();
  if (NS_FAILED(res)) {
    return res;
  }

  PRUnichar** array =
    (PRUnichar**) nsMemory::Alloc(gGlobalList->Count() * sizeof(PRUnichar*));
  if (!array) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  EnumerateNodeInfo info = { array, 0, aLangGroup };
  if (!gGlobalList->EnumerateForwards(EnumerateNode, &info)) {
    nsMemory::Free(array);
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_QuickSort(array, info.mIndex, sizeof(PRUnichar*), CompareFontNames,
               nsnull);

  *aCount = info.mIndex;
  if (*aCount) {
    *aResult = array;
  }
  else {
    nsMemory::Free(array);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsFontEnumeratorXlib::EnumerateAllFonts(PRUint32* aCount, PRUnichar*** aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = nsnull;
  NS_ENSURE_ARG_POINTER(aCount);
  *aCount = 0;

  return EnumFonts(nsnull, nsnull, aCount, aResult);
}

NS_IMETHODIMP
nsFontEnumeratorXlib::EnumerateFonts(const char* aLangGroup,
                                     const char* aGeneric, PRUint32* aCount,
                                     PRUnichar*** aResult)
{   
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = nsnull;
  NS_ENSURE_ARG_POINTER(aCount);
  *aCount = 0;
  NS_ENSURE_ARG_POINTER(aGeneric);
  NS_ENSURE_ARG_POINTER(aLangGroup);
  
  nsCOMPtr<nsIAtom> langGroup = getter_AddRefs(NS_NewAtom(aLangGroup));
  
  // XXX still need to implement aLangGroup and aGeneric
  return EnumFonts(langGroup, aGeneric, aCount, aResult);
}

NS_IMETHODIMP
nsFontEnumeratorXlib::HaveFontFor(const char* aLangGroup, PRBool* aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = PR_FALSE;
  NS_ENSURE_ARG_POINTER(aLangGroup);

  *aResult = PR_TRUE; // always return true for now.
  // Finish me - ftang
  return NS_OK;
}

NS_IMETHODIMP
nsFontEnumeratorXlib::UpdateFontList(PRBool *updateFontList)
{
  *updateFontList = PR_FALSE; // always return false for now
  return NS_OK;
}

