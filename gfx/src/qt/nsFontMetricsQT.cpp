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
 *		Jean Claude Batista <jcb@macadamian.com>
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

#include "xp_core.h"
#include "nsQuickSort.h"
#include "nsFontMetricsQT.h"
#include "nsFont.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsIServiceManager.h"
#include "nsISaveAsCharset.h"
#include "nsIPref.h"
#include "nsCOMPtr.h"
#include "nspr.h"
#include "nsHashtable.h"
#include "nsDrawingSurfaceQT.h"
#include "nsRenderingContextQT.h"
#include "nsILanguageAtomService.h"

#include <qapplication.h>
#include <qfont.h>
#include <qfontdatabase.h>

#undef USER_DEFINED
#define USER_DEFINED "x-user-def"

#undef NOISY_FONTS
//JCG #define NOISY_FONTS 1
#undef REALLY_NOISY_FONTS
//JCG #define REALLY_NOISY_FONTS 1

//JCG #define DBG_JCG 1

#ifdef DBG_JCG
PRUint32 gQFontCount = 0;
PRUint32 gQFontID = 0;

PRUint32 gFontMetricsCount = 0;
PRUint32 gFontMetricsID = 0;
#endif

static int gFontMetricsQTCount = 0;
static int gInitialized = 0;

// XXX many of these statics need to be freed at shutdown time
 
static nsIPref *gPref = nsnull;
static nsIUnicodeEncoder *gUserDefinedConverter = nsnull;
static nsICharsetConverterManager2 *gCharSetManager = nsnull;
static PRUint32 gUserDefinedMap[2048];

static nsHashtable *gCharSets = nsnull;
static nsHashtable *gSpecialCharSets = nsnull;

static nsIAtom *gUnicode = nsnull;
static nsIAtom *gUserDefined = nsnull;
static nsIAtom *gUsersLocale = nsnull;

static NS_DEFINE_IID(kIFontMetricsIID,NS_IFONT_METRICS_IID);
static NS_DEFINE_CID(kCharSetManagerCID,NS_ICHARSETCONVERTERMANAGER_CID);
static NS_DEFINE_CID(kPrefCID,NS_PREF_CID);
static NS_DEFINE_CID(kSaveAsCharsetCID,NS_SAVEASCHARSET_CID);

#define NSQ_DOUBLEBYTE 1

struct nsFontCharSetInfo
{
  const char *mCharSet;
  nsIAtom *mCharsetAtom;
  nsIAtom *mLangGroup;
  PRUint32 *mMap;
  PRUint8 mType;
};

struct nsFontCharSetMap
{
  char              *mName;
  nsFontCharSetInfo *mInfo;
};

static nsFontCharSetInfo Unknown =
  { nsnull, nsnull, nsnull, nsnull, 0 };
static nsFontCharSetInfo Special =
  { nsnull, nsnull, nsnull, nsnull, 0 };
static nsFontCharSetInfo ISO106461 =
  { nsnull, nsnull, nsnull, nsnull, 0 };
 
static nsFontCharSetInfo CP1251 =
  { "windows-1251", nsnull, nsnull, nsnull, 0 };
static nsFontCharSetInfo ISO88591 =
  { "ISO-8859-1", nsnull, nsnull, nsnull, 0 };
static nsFontCharSetInfo ISO88592 =
  { "ISO-8859-2", nsnull, nsnull, nsnull, 0 };
static nsFontCharSetInfo ISO88593 =
  { "ISO-8859-3", nsnull, nsnull, nsnull, 0 };
static nsFontCharSetInfo ISO88594 =
  { "ISO-8859-4", nsnull, nsnull, nsnull, 0 };
static nsFontCharSetInfo ISO88595 =
  { "ISO-8859-5", nsnull, nsnull, nsnull, 0 };
static nsFontCharSetInfo ISO88596 =
  { "ISO-8859-6", nsnull, nsnull, nsnull, 0 };
static nsFontCharSetInfo ISO88597 =
  { "ISO-8859-7", nsnull, nsnull, nsnull, 0 };
static nsFontCharSetInfo ISO88598 =
  { "ISO-8859-8", nsnull, nsnull, nsnull, 0 };
static nsFontCharSetInfo ISO88599 =
  { "ISO-8859-9", nsnull, nsnull, nsnull, 0 };
static nsFontCharSetInfo ISO885913 =
  { "ISO-8859-13", nsnull, nsnull, nsnull, 0 };
static nsFontCharSetInfo ISO885915 =
  { "ISO-8859-15", nsnull, nsnull, nsnull, 0 };
static nsFontCharSetInfo JISX0201 =
  { "jis_0201", nsnull, nsnull, nsnull, 0 };
static nsFontCharSetInfo KOI8R =
  { "KOI8-R", nsnull, nsnull, nsnull, 0 };
static nsFontCharSetInfo KOI8U =
  { "KOI8-U", nsnull, nsnull, nsnull, 0 };
static nsFontCharSetInfo TIS620 =
  { "TIS-620", nsnull, nsnull, nsnull, 0 };
 
static nsFontCharSetInfo Big5 =
  { "x-x-big5", nsnull, nsnull, nsnull, NSQ_DOUBLEBYTE };
static nsFontCharSetInfo CNS116431 =
  { "x-cns-11643-1", nsnull, nsnull, nsnull, NSQ_DOUBLEBYTE };
static nsFontCharSetInfo CNS116432 =
  { "x-cns-11643-2", nsnull, nsnull, nsnull, NSQ_DOUBLEBYTE };
static nsFontCharSetInfo CNS116433 =
  { "x-cns-11643-3", nsnull, nsnull, nsnull, NSQ_DOUBLEBYTE };
static nsFontCharSetInfo CNS116434 =
  { "x-cns-11643-4", nsnull, nsnull, nsnull, NSQ_DOUBLEBYTE };
static nsFontCharSetInfo CNS116435 =
  { "x-cns-11643-5", nsnull, nsnull, nsnull, NSQ_DOUBLEBYTE };
static nsFontCharSetInfo CNS116436 =
  { "x-cns-11643-6", nsnull, nsnull, nsnull, NSQ_DOUBLEBYTE };
static nsFontCharSetInfo CNS116437 =
  { "x-cns-11643-7", nsnull, nsnull, nsnull, NSQ_DOUBLEBYTE };
static nsFontCharSetInfo GB2312 =
  { "gb_2312-80", nsnull, nsnull, nsnull, NSQ_DOUBLEBYTE };
static nsFontCharSetInfo GB18030_0 =
  { "gb18030.2000-0", nsnull, nsnull, nsnull, NSQ_DOUBLEBYTE };
static nsFontCharSetInfo GB18030_1 =
  { "gb18030.2000-1", nsnull, nsnull, nsnull, NSQ_DOUBLEBYTE };
static nsFontCharSetInfo GBK =
  { "x-gbk", nsnull, nsnull, nsnull, NSQ_DOUBLEBYTE };
static nsFontCharSetInfo HKSCS =
  { "hkscs-1", nsnull, nsnull, nsnull, NSQ_DOUBLEBYTE };
static nsFontCharSetInfo JISX0208 =
  { "jis_0208-1983", nsnull, nsnull, nsnull, NSQ_DOUBLEBYTE };
static nsFontCharSetInfo JISX0212 =
  { "jis_0212-1990", nsnull, nsnull, nsnull, NSQ_DOUBLEBYTE };
static nsFontCharSetInfo KSC5601 =
  { "ks_c_5601-1987", nsnull, nsnull, nsnull, NSQ_DOUBLEBYTE };
static nsFontCharSetInfo X11Johab =
  { "x-x11johab", nsnull, nsnull, nsnull, NSQ_DOUBLEBYTE };
static nsFontCharSetInfo Johab =
  { "x-johab", nsnull, nsnull, nsnull, NSQ_DOUBLEBYTE };
 
static nsFontCharSetInfo AdobeSymbol =
   { "Adobe-Symbol-Encoding", nsnull, nsnull, nsnull, 0 };
 
#ifdef MOZ_MATHML
static nsFontCharSetInfo CMCMEX =
   { "x-t1-cmex", nsnull, nsnull, nsnull, 0 };
static nsFontCharSetInfo CMCMSY =
   { "x-t1-cmsy", nsnull, nsnull, nsnull, 0 };
static nsFontCharSetInfo CMCMR =
   { "x-t1-cmr", nsnull, nsnull, nsnull, 0 };
static nsFontCharSetInfo CMCMMI =
   { "x-t1-cmmi", nsnull, nsnull, nsnull, 0 };
static nsFontCharSetInfo Mathematica1 =
   { "x-mathematica1", nsnull, nsnull, nsnull, 0 };
static nsFontCharSetInfo Mathematica2 =
   { "x-mathematica2", nsnull, nsnull, nsnull, 0 };
static nsFontCharSetInfo Mathematica3 =
   { "x-mathematica3", nsnull, nsnull, nsnull, 0 };
static nsFontCharSetInfo Mathematica4 =
   { "x-mathematica4", nsnull, nsnull, nsnull, 0 };
static nsFontCharSetInfo Mathematica5 =
   { "x-mathematica5", nsnull, nsnull, nsnull, 0 };
#endif

static nsFontCharSetMap gCharSetMap[] =
{
  { "-ascii",             &Unknown       },
  { "-ibm pc",            &Unknown       },
  { "adobe-fontspecific", &Special       },
  { "big5-0",             &Big5          },
  { "big5-1",             &Big5          },
  { "big5.et-0",          &Big5          },
  { "big5.et.ext-0",      &Big5          },
  { "big5.etext-0",       &Big5          },
  { "big5.hku-0",         &Big5          },
  { "big5.hku-1",         &Big5          },
  { "big5.pc-0",          &Big5          },
  { "big5.shift-0",       &Big5          },
  { "cns11643.1986-1",    &CNS116431     },
  { "cns11643.1986-2",    &CNS116432     },
  { "cns11643.1992-1",    &CNS116431     },
  { "cns11643.1992.1-0",  &CNS116431     },
  { "cns11643.1992-12",   &Unknown       },
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
  { "cns11643-1",         &CNS116431     },
  { "cns11643-2",         &CNS116432     },
  { "cns11643-3",         &CNS116433     },
  { "cns11643-4",         &CNS116434     },
  { "cns11643-5",         &CNS116435     },
  { "cns11643-6",         &CNS116436     },
  { "cns11643-7",         &CNS116437     },
  { "cp1251-1",           &CP1251        },
  { "dec-dectech",        &Unknown       },
  { "dtsymbol-1",         &Unknown       },
  { "fontspecific-0",     &Unknown       },
  { "gb2312.1980-0",      &GB2312        },
  { "gb2312.1980-1",      &GB2312        },
  { "gb13000.1993-1",     &GBK           },
  { "gb18030.2000-0",     &GB18030_0     },
  { "gb18030.2000-1",     &GB18030_1     },
  { "gbk-0",     	  &GBK           },
  { "hkscs-1",     	  &HKSCS         },
  { "hp-japanese15",      &Unknown       },
  { "hp-japaneseeuc",     &Unknown       },
  { "hp-roman8",          &Unknown       },
  { "hp-schinese15",      &Unknown       },
  { "hp-tchinese15",      &Unknown       },
  { "hp-tchinesebig5",    &Big5          },
  { "hp-wa",              &Unknown       },
  { "hpbig5-",            &Big5          },
  { "hproc16-",           &Unknown       },
  { "ibm-1252",           &Unknown       },
  { "ibm-850",            &Unknown       },
  { "ibm-fontspecific",   &Unknown       },
  { "ibm-sbdcn",          &Unknown       },
  { "ibm-sbdtw",          &Unknown       },
  { "ibm-special",        &Unknown       },
  { "ibm-udccn",          &Unknown       },
  { "ibm-udcjp",          &Unknown       },
  { "ibm-udctw",          &Unknown       },
  { "iso646.1991-irv",    &Unknown       },
  { "iso8859-1",          &ISO88591      },
  { "iso8859-13",         &ISO885913     },
  { "iso8859-15",         &ISO885915     },
  { "iso8859-1@cn",       &Unknown       },
  { "iso8859-1@kr",       &Unknown       },
  { "iso8859-1@tw",       &Unknown       },
  { "iso8859-1@zh",       &Unknown       },
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
  { "koi8-u",             &KOI8U         },
  { "johab-1",            &X11Johab      },
  { "johabs-1",           &X11Johab      },
  { "johabsh-1",          &X11Johab      },
  { "ksc5601.1987-0",     &KSC5601       },
  { "ksc5601.1992-3",     &Johab         },
  { "microsoft-cp1251",   &CP1251        },
  { "misc-fontspecific",  &Unknown       },
  { "sgi-fontspecific",   &Unknown       },
  { "sun-fontspecific",   &Unknown       },
  { "sunolcursor-1",      &Unknown       },
  { "sunolglyph-1",       &Unknown       },
  { "tis620.2529-1",      &TIS620        },
  { "tis620.2533-0",      &TIS620        },
  { "tis620.2533-1",      &TIS620        },
  { "tis620-0",           &TIS620        },
  { "iso8859-11",         &TIS620        },
  { "ucs2.cjk-0",         &Unknown       },
  { "ucs2.cjk_japan-0",   &Unknown       },
  { "ucs2.cjk_taiwan-0",  &Unknown       },
 
  { nsnull,               nsnull         }
};

static nsFontCharSetMap gSpecialCharSetMap[] =
{
  { "symbol-adobe-fontspecific", &AdobeSymbol  },
 
#ifdef MOZ_MATHML
  { "cmex10-adobe-fontspecific", &CMCMEX  },
  { "cmsy10-adobe-fontspecific", &CMCMSY  },
  { "cmr10-adobe-fontspecific", &CMCMR  },
  { "cmmi10-adobe-fontspecific", &CMCMMI  },

  { "math1-adobe-fontspecific", &Mathematica1 },
  { "math2-adobe-fontspecific", &Mathematica2 },
  { "math3-adobe-fontspecific", &Mathematica3 },
  { "math4-adobe-fontspecific", &Mathematica4 },
  { "math5-adobe-fontspecific", &Mathematica5 },
 
  { "math1mono-adobe-fontspecific", &Mathematica1 },
  { "math2mono-adobe-fontspecific", &Mathematica2 },
  { "math3mono-adobe-fontspecific", &Mathematica3 },
  { "math4mono-adobe-fontspecific", &Mathematica4 },
  { "math5mono-adobe-fontspecific", &Mathematica5 },
#endif
 
  { nsnull,                      nsnull        }
};

struct nsFontFamilyName
{
  char *mName;
  char *mXName;
}; 

static struct nsFontFamilyName gFamilyNameTable[] =
{
  { "arial",           "helvetica" },
  { "courier new",     "courier" },
  { "times new roman", "times" },
 
#ifdef MOZ_MATHML
  { "cmex",             "cmex10" },
  { "cmsy",             "cmsy10" },
  { "-moz-math-text",   "times" },
  { "-moz-math-symbol", "symbol" },
#endif
 
  { nsnull, nsnull }
};

struct nsFontSearch
{
  nsFontMetricsQT *mMetrics;
  PRUnichar       mChar;
  nsFontQT        *mFont;
};

class nsFontQTNormal : public nsFontQT
{
public:
  nsFontQTNormal();
  nsFontQTNormal(QFont *aQFont);
  virtual ~nsFontQTNormal();
 
  virtual int GetWidth(const PRUnichar *aString,PRUint32 aLength);
  virtual int DrawString(nsRenderingContextQT *aContext,
                         nsDrawingSurfaceQT *aSurface,nscoord aX,
                         nscoord aY,const PRUnichar *aString,
                         PRUint32 aLength);
#ifdef MOZ_MATHML
  virtual nsresult GetBoundingMetrics(const PRUnichar *aString,
                                      PRUint32 aLength,
                                      nsBoundingMetrics &aBoundingMetrics);
#endif
};
 
class nsFontQTSubstitute : public nsFontQT
{
public:
  nsFontQTSubstitute(nsFontQT *aFont);
  virtual ~nsFontQTSubstitute();
 
  virtual QFont *GetQFont(void);
   virtual int SupportsChar(PRUnichar aChar);
  virtual int GetWidth(const PRUnichar *aString,PRUint32 aLength);
  virtual int DrawString(nsRenderingContextQT *aContext,
                         nsDrawingSurfaceQT *aSurface,nscoord aX,
                         nscoord aY,const PRUnichar *aString,
                         PRUint32 aLength);
#ifdef MOZ_MATHML
  virtual nsresult GetBoundingMetrics(const PRUnichar *aString,
                                      PRUint32 aLength,
                                      nsBoundingMetrics &aBoundingMetrics);
#endif
  virtual PRUint32 Convert(const PRUnichar *aSrc,PRUint32 aSrcLen,
                           PRUnichar *aDest,PRUint32 aDestLen);

  nsFontQT *mSubstituteFont;
 
  static int gCount;
  static nsISaveAsCharset *gConverter;
};

int nsFontQTSubstitute::gCount = 0;
nsISaveAsCharset *nsFontQTSubstitute::gConverter = nsnull;

class nsFontQTUserDefined : public nsFontQT
{
public:
  nsFontQTUserDefined();
  virtual ~nsFontQTUserDefined();
 
  virtual PRUint32 *GetCharSetMap() { return mMap; };
  virtual PRBool Init(nsFontQT *aFont);
  virtual int GetWidth(const PRUnichar *aString,PRUint32 aLength);
  virtual int DrawString(nsRenderingContextQT *aContext,
                         nsDrawingSurfaceQT *aSurface,nscoord aX,
                         nscoord aY,const PRUnichar *aString,
                         PRUint32 aLength);
#ifdef MOZ_MATHML
  virtual nsresult GetBoundingMetrics(const PRUnichar *aString,
                                      PRUint32 aLength,
                                      nsBoundingMetrics &aBoundingMetrics);
#endif
  virtual PRUint32 Convert(const PRUnichar *aSrc,PRInt32 aSrcLen,
                           char *aDest,PRInt32 aDestLen);

  PRUint32 *mMap;
};
 
QFontDatabase *nsFontMetricsQT::mQFontDB = nsnull;

static PRBool
FreeCharSet(nsHashKey *aKey,void *aData,void *aClosure)
{
  nsFontCharSetInfo *charset = (nsFontCharSetInfo*)aData;

  NS_IF_RELEASE(charset->mCharsetAtom);
  NS_IF_RELEASE(charset->mLangGroup);
  return PR_TRUE;
}

static void
FreeGlobals(void)
{
  // XXX complete this
  gInitialized = 0;
  NS_IF_RELEASE(gCharSetManager);
  if (gCharSets) {
    gCharSets->Reset(FreeCharSet,nsnull);
    delete gCharSets;
    gCharSets = nsnull;
  }
  if (gSpecialCharSets) {
    delete gSpecialCharSets;
    gSpecialCharSets = nsnull;
  }
  NS_IF_RELEASE(gPref);
  NS_IF_RELEASE(gUnicode);
  NS_IF_RELEASE(gUserDefined);
  NS_IF_RELEASE(gUserDefinedConverter);
  NS_IF_RELEASE(gUsersLocale);
}

static nsresult
InitGlobals(void)
{
  nsServiceManager::GetService(kCharSetManagerCID,
                               NS_GET_IID(nsICharsetConverterManager2),
                               (nsISupports**)&gCharSetManager);
  if (!gCharSetManager) {
    FreeGlobals();
    return NS_ERROR_FAILURE;
  }
  nsServiceManager::GetService(kPrefCID,NS_GET_IID(nsIPref),
                               (nsISupports**)&gPref);
  if (!gPref) {
    FreeGlobals();
    return NS_ERROR_FAILURE;
  }
  nsFontFamilyName *f = gFamilyNameTable;
  while (f->mName) {
    QFont::insertSubstitution(QString(f->mName),QString(f->mXName));
    f++;
  }
  gCharSets = new nsHashtable();
  if (!gCharSets) {
    FreeGlobals();
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsFontCharSetMap *charSetMap = gCharSetMap;
  while (charSetMap->mName) {
    nsCStringKey key(charSetMap->mName);
    gCharSets->Put(&key,charSetMap->mInfo);
    charSetMap++;
  } 
  gSpecialCharSets = new nsHashtable();
  if (!gSpecialCharSets) {
    FreeGlobals();
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsFontCharSetMap *specialCharSetMap = gSpecialCharSetMap;
  while (specialCharSetMap->mName) {
    nsCStringKey key(specialCharSetMap->mName);
    gSpecialCharSets->Put(&key,specialCharSetMap->mInfo);
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
  memset(gUserDefinedMap,0,sizeof(gUserDefinedMap)); 

  nsCOMPtr<nsILanguageAtomService> langService;
  langService = do_GetService(NS_LANGUAGEATOMSERVICE_CONTRACTID);
  if (langService) {
    langService->GetLocaleLanguageGroup(&gUsersLocale);
  }
  if (!gUsersLocale) {
    gUsersLocale = NS_NewAtom("x-western");
  }
  if (!gUsersLocale) {
    FreeGlobals();
    return NS_ERROR_OUT_OF_MEMORY;
  }
  gInitialized = 1;
  return NS_OK;
}

static void ParseFontPref(nsCAutoString &aPrefValue,QString &aFontName,
                          QString &aFontFoundryName,QString &aCharSet)
{
  nsCAutoString fontName,fontFoundryName,fontCharSetName;
 
  /* count hyphens */
  const char *str = aPrefValue.get();
  PRUint32 len = aPrefValue.Length();
  int hyphens = 0;
  for (PRUint32 i = 0; i < len; i++) {
    if (str[i] == '-') {
      hyphens++;
    }
  }
  /*
   * if there are 3 hyphens, the name is something like
   *
   *   adobe-times-iso8859-1
   *
   * otherwise it is something like
   *
   *   times new roman
   */
  if (hyphens == 3) {
    PRInt32 start,end;
 
    /* Try both foundry-name and name */
    start = aPrefValue.FindChar('-');
    end = aPrefValue.FindChar('-',PR_FALSE,start + 1);
    aPrefValue.Left(fontFoundryName,end);
    aPrefValue.Right(fontCharSetName,len - end - 1);
    aPrefValue.Mid(fontName,start + 1,end - start - 1);
    aFontName = fontName.get();
    aFontFoundryName = fontFoundryName.get();
    aCharSet = fontCharSetName.get();
  }
  else {
    aFontFoundryName = aPrefValue.get();
  } 
}

//
// There is a bug in QT 2.x and 3.0 when using the QFontDatabase
// font method to obtain a specific QFont. Some QFont::CharSet
// do not have a string mapping. Use this function after calling
// QFontDatabase::font().
//
// Note: This should be fixed in QT 3.1 - JCB 2001-03-09
//
static QFont::CharSet getExtendedCharSet(const QString &name)
{
  if (name == "jisx0208.1983-0")
    return QFont::JIS_X_0208;
  if (name == "jisx0201.1976-0")
    return QFont::JIS_X_0201;
  if (name == "ksc5601.1987-0")
    return QFont::KSC_5601;
  if (name == "gb2312.1980-0")
    return QFont::GB_2312;
  if (name == "big5-0")
    return QFont::Big5;
 
  return QFont::AnyCharSet;
}

nsFontMetricsQT::nsFontMetricsQT()
{
#ifdef DBG_JCG
  gFontMetricsCount++;
  mID = gFontMetricsID++;
  printf("JCG: nsFontMetricsQT CTOR (%p) ID: %d, Count: %d\n",this,mID,gFontMetricsCount);
#endif
  NS_INIT_REFCNT();
  gFontMetricsQTCount++;
  mDeviceContext = nsnull;
  mFont = nsnull;
  mQStyle = nsnull;

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
  mLoadedFonts = nsnull;
  mLoadedFontsCount = 0;
  mLoadedFontsAlloc = 0;
  mUserDefinedFont = nsnull;
}

nsFontMetricsQT::~nsFontMetricsQT()
{
#ifdef DBG_JCG
  gFontMetricsCount--;
  printf("JCG: nsFontMetricsQT DTOR (%p) ID: %d, Count: %d\n",this,mID,gFontMetricsCount);
#endif
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
  if (mQStyle) {
    delete mQStyle;
    mQStyle = nsnull;
  }
  if (mUserDefinedFont) {
    delete mUserDefinedFont;
    mUserDefinedFont = nsnull;
  } 
  if (!--gFontMetricsQTCount) {
    FreeGlobals();
    if (mQFontDB) {
      delete mQFontDB;
      mQFontDB = nsnull;
    }
  }
}

NS_IMPL_ISUPPORTS1(nsFontMetricsQT,nsIFontMetrics)

static PRBool
IsASCIIFontName(const nsString &aName)
{
  PRUint32 len = aName.Length();
  const PRUnichar *str = aName.get();
  for (PRUint32 i = 0; i < len; i++) {
    /*
     * X font names are printable ASCII, ignore others (for now)
     */
    if (str[i] < 0x20 || str[i] > 0x7E) {
      return PR_FALSE;
    }
  }
  return PR_TRUE;
}
 
static PRBool
FontEnumCallback(const nsString &aFamily,PRBool aGeneric,void *aData)
{
#ifdef REALLY_NOISY_FONTS
#ifdef DEBUG
  printf("font = '");
  fputs(aFamily, stdout);
  printf("'\n");
#endif
#endif
 
  if (!IsASCIIFontName(aFamily)) {
    return PR_TRUE; // skip and continue
  }
  nsCAutoString name;
  nsFontMetricsQT *metrics = (nsFontMetricsQT*)aData;

  name.AssignWithConversion(aFamily.get());
  name.ToLowerCase();
  metrics->mFonts.AppendCString(name);
  metrics->mFontIsGeneric.AppendElement((void*)aGeneric);
  if (aGeneric) {
    metrics->mGeneric = metrics->mFonts.CStringAt(metrics->mFonts.Count() - 1);
    return PR_FALSE; // stop
  }
  return PR_TRUE; // continue
}

#if (QT_VERSION >= 230)
static int use_qt_xft()
{
  static int use_xft = 0;
  static int chkd_env = 0;

  if (!chkd_env) {
    char *e = getenv ("QT_XFT");
    if (e && (*e == '1' || *e == 'y' || *e == 'Y'
              || *e == 't' || *e == 'T' ))
      use_xft = 1;
    chkd_env = 1;
  }
  return use_xft;

}
#endif

NS_IMETHODIMP nsFontMetricsQT::Init(const nsFont &aFont,nsIAtom *aLangGroup,
                                    nsIDeviceContext *aContext)
{
  NS_ASSERTION(!(nsnull == aContext), 
               "attempt to init fontmetrics with null device context");
  nsresult res;

  if (!gInitialized) {
    res = InitGlobals();
    if (NS_FAILED(res)) {
      return res;
    }
  }
  mFont = new nsFont(aFont);
  mLangGroup = aLangGroup;
  mDeviceContext = aContext;

  float a2d;
  float textZoom = 1.0;
 
  aContext->GetAppUnitsToDevUnits(a2d);
  aContext->GetTextZoom(textZoom);
  mPixelSize = NSToIntRound(a2d * textZoom * mFont->size); 
  if (mFont->style == NS_FONT_STYLE_NORMAL) {
#if (QT_VERSION >= 230)
    if (use_qt_xft())
      mQStyle = new QString("Regular");
    else
#endif
    mQStyle = new QString("Normal");
  }
  else if (mFont->style == NS_FONT_STYLE_OBLIQUE) {
    mQStyle = new QString("Oblique");
  }
  else {
    mQStyle = new QString("Italic");
  }
  mWeight = mFont->weight;

  mFont->EnumerateFamilies(FontEnumCallback,this);
  char *value = nsnull;
  if (!mGeneric) {
    gPref->CopyCharPref("font.default",&value);
    if (value) {
      mDefaultFont = value;
      nsMemory::Free(value);
      value = nsnull;
    }
    else {
      mDefaultFont = "serif";
    }
    mGeneric = &mDefaultFont;
  }
  if (mLangGroup) {
    nsCAutoString name("font.min-size.");
    if (mGeneric->Equals("monospace")) {
      name.Append("fixed");
    }
    else {
      name.Append("variable");
    }
    name.Append(char('.'));

    const PRUnichar *langGroup = nsnull;
    mLangGroup->GetUnicode(&langGroup);
    name.AppendWithConversion(langGroup);

    PRInt32 minimum = 0;
    res = gPref->GetIntPref(name.get(),&minimum);
    if (NS_FAILED(res)) {
      gPref->GetDefaultIntPref(name.get(),&minimum);
    }
    if (minimum < 0) {
      minimum = 0;
    }
    if (mPixelSize < minimum) {
      mPixelSize = minimum;
    }
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
          res
           = gUserDefinedConverter->SetOutputErrorBehavior(gUserDefinedConverter->kOnError_Replace,
                                                           nsnull, '?');
          nsCOMPtr<nsICharRepresentable> mapper
           = do_QueryInterface(gUserDefinedConverter);
          if (mapper) {
            res = mapper->FillInfo(gUserDefinedMap);
          }
        }
        else {
          return res;
        }
      }
      else {
        return res;
      }
    }
    nsCAutoString name("font.name.");
    name.Append(*mGeneric);
    name.Append(char('.'));
    name.Append(USER_DEFINED);
    gPref->CopyCharPref(name.get(),&value);
    if (value) {
      mUserDefined = value;
      nsMemory::Free(value);
      value = nsnull;
      mIsUserDefined = 1;
    }
  }
  mWesternFont = FindFont('a');
  if (!mWesternFont) {
    return NS_ERROR_FAILURE;
  }
  RealizeFont();
  return NS_OK;
}

QFontDatabase *nsFontMetricsQT::GetQFontDB()
{
  if (!mQFontDB) {
    mQFontDB = new QFontDatabase();
  }
  return mQFontDB;
}
 
NS_IMETHODIMP nsFontMetricsQT::Destroy()
{
  return NS_OK;
}

void nsFontMetricsQT::RealizeFont()
{
  QFontMetrics fm(*(mWesternFont->GetQFont()));

  float f;
  mDeviceContext->GetDevUnitsToAppUnits(f);

  mMaxAscent = nscoord(fm.ascent() * f) ;
  mMaxDescent = nscoord(fm.descent() * f);
  mMaxHeight = mMaxAscent + mMaxDescent;

  mEmHeight = nscoord(fm.height() * f);
  mMaxAdvance = nscoord(fm.maxWidth() * f);

  mEmAscent = mMaxAscent;  
  mEmDescent = mMaxDescent;
 
  // 56% of ascent, best guess for non-true type
  mXHeight = NSToCoordRound((float) fm.ascent() * f * 0.56f);

  mUnderlineOffset = - nscoord(fm.underlinePos() * f);

  mUnderlineSize = NSToIntRound(fm.lineWidth() * f);

  mSuperscriptOffset = mXHeight;
  mSubscriptOffset = mXHeight;

  mStrikeoutOffset = nscoord(fm.strikeOutPos() * f);
  mStrikeoutSize = mUnderlineSize;

  mLeading = nscoord(fm.leading() * f);
  mSpaceWidth = nscoord(fm.width(QChar(' ')) * f);
}

NS_IMETHODIMP nsFontMetricsQT::GetXHeight(nscoord &aResult)
{
  aResult = mXHeight;
  return NS_OK;
}

NS_IMETHODIMP nsFontMetricsQT::GetSuperscriptOffset(nscoord &aResult)
{
  aResult = mSuperscriptOffset;
  return NS_OK;
}

NS_IMETHODIMP nsFontMetricsQT::GetSubscriptOffset(nscoord &aResult)
{
  aResult = mSubscriptOffset;
  return NS_OK;
}

NS_IMETHODIMP nsFontMetricsQT::GetStrikeout(nscoord &aOffset,nscoord &aSize)
{
  aOffset = mStrikeoutOffset;
  aSize = mStrikeoutSize;
  return NS_OK;
}

NS_IMETHODIMP nsFontMetricsQT::GetUnderline(nscoord &aOffset,nscoord &aSize)
{
  aOffset = mUnderlineOffset;
  aSize = mUnderlineSize;
  return NS_OK;
}

NS_IMETHODIMP nsFontMetricsQT::GetHeight(nscoord &aHeight)
{
  aHeight = mMaxHeight;
  return NS_OK;
}

NS_IMETHODIMP nsFontMetricsQT::GetNormalLineHeight(nscoord &aHeight)
{
  aHeight = mEmHeight + mLeading;
  return NS_OK;
}
 
NS_IMETHODIMP nsFontMetricsQT::GetLeading(nscoord &aLeading)
{
  aLeading = mLeading;
  return NS_OK;
}

NS_IMETHODIMP nsFontMetricsQT::GetEmHeight(nscoord &aHeight)
{
  aHeight = mEmHeight;
  return NS_OK;
}
 
NS_IMETHODIMP nsFontMetricsQT::GetEmAscent(nscoord &aAscent)
{
  aAscent = mEmAscent;
  return NS_OK;
}
 
NS_IMETHODIMP nsFontMetricsQT::GetEmDescent(nscoord &aDescent)
{
  aDescent = mEmDescent;
  return NS_OK;
}
 
NS_IMETHODIMP nsFontMetricsQT::GetMaxHeight(nscoord &aHeight)
{
  aHeight = mMaxHeight;
  return NS_OK;
}
 
NS_IMETHODIMP nsFontMetricsQT::GetMaxAscent(nscoord &aAscent)
{
  aAscent = mMaxAscent;
  return NS_OK;
}

NS_IMETHODIMP nsFontMetricsQT::GetMaxDescent(nscoord &aDescent)
{
  aDescent = mMaxDescent;
  return NS_OK;
}

NS_IMETHODIMP nsFontMetricsQT::GetMaxAdvance(nscoord &aAdvance)
{
  aAdvance = mMaxAdvance;
  return NS_OK;
}

NS_IMETHODIMP nsFontMetricsQT::GetFont(const nsFont *&aFont)
{
  aFont = mFont;
  return NS_OK;
}

NS_IMETHODIMP nsFontMetricsQT::GetLangGroup(nsIAtom **aLangGroup)
{
  if (!aLangGroup) {
    return NS_ERROR_NULL_POINTER;
  }
  *aLangGroup = mLangGroup;
  NS_IF_ADDREF(*aLangGroup);
  return NS_OK;
}


NS_IMETHODIMP nsFontMetricsQT::GetFontHandle(nsFontHandle &aHandle)
{
  aHandle = (nsFontHandle)mWesternFont;
  return NS_OK;
}

nsFontQT*
nsFontMetricsQT::LookUpFontPref(nsCAutoString &aName,PRUnichar aChar)
{
  QString fontName,foundryName,charSetName;
  nsFontQT *font = nsnull;

  ParseFontPref(aName,fontName,foundryName,charSetName);
  if (!fontName.isEmpty()) {
    if (!charSetName.isEmpty()) {
      font = LoadFont(foundryName,charSetName,aChar);
      if (font) {
        return font;
      }
    }
    else {
      font = LoadFont(foundryName,aChar);
      if (font) {
        return font;
      }
    }
  }
  else {
    font = LoadFont(foundryName,aChar);
    if (font) {
      return font;
    }
  }
  return font;
}

nsFontQT*
nsFontMetricsQT::LoadFont(QString &aName,PRUnichar aChar)
{
  QFontDatabase *qFontDB = GetQFontDB();
  QStringList qCharsets;
  nsFontQT *font;

  qCharsets = qFontDB->charSets(aName,FALSE);
  qCharsets.sort();
  for (QStringList::Iterator csIt = qCharsets.begin();
       csIt != qCharsets.end(); ++csIt) {
    font = LoadFont(aName,*csIt,aChar);
    if (font) {
      return font;
    }
  }
  return nsnull;
}

nsFontQT*
nsFontMetricsQT::LoadFont(QString &aName,const QString &aCharSet,
                          PRUnichar aChar)
{
  nsFontQT *newFont = nsnull;
  QFont *qFont;
  nsIUnicodeEncoder *converter;

  nsCStringKey charSetKey(aCharSet.latin1());

  nsFontCharSetInfo *charSetInfo
     = (nsFontCharSetInfo*)gCharSets->Get(&charSetKey);
 
  // indirection for font specific charset encoding
  if (charSetInfo == &Special) {
    nsCAutoString familyCharSetName(aName.latin1());
    familyCharSetName.Append('-');
    familyCharSetName.Append(aCharSet.latin1());
    nsCStringKey familyCharSetKey(familyCharSetName);
    charSetInfo = (nsFontCharSetInfo*)gSpecialCharSets->Get(&familyCharSetKey);
  }
  if (charSetInfo == &Unknown) {
    return nsnull;
  }
  if (!charSetInfo) {
    if (mIsUserDefined && FONT_HAS_GLYPH(gUserDefinedMap,aChar)) {
      charSetInfo = &Unknown;
    }
    else {
#ifdef NOISY_FONTS
#ifdef DEBUG
      printf("cannot find charset %s-%s\n",aName.latin1(),
             aCharSet.latin1());
#endif
#endif
      return nsnull;
    }
  }
  if (charSetInfo->mCharSet) {
    if (charSetInfo->mMap) {
      for (int i = 0; i < mLoadedFontsCount; i++) {
        if (mLoadedFonts[i]->GetCharSetMap() == charSetInfo->mMap) {
          return nsnull;
        }
      }
    }
    else {
      nsresult res;
      if (!charSetInfo->mCharsetAtom) {
        res
         = gCharSetManager->GetCharsetAtom2(charSetInfo->mCharSet,
                                            &charSetInfo->mCharsetAtom);
        if (NS_FAILED(res)) {
#ifdef NOISY_FONTS
#ifdef DEBUG
        printf("=== cannot get Charset Atom for %s\n",
               charSetInfo->mCharSet);
#endif
#endif
          return nsnull;
        }
      }
      if (!charSetInfo->mLangGroup) {
        res
         = gCharSetManager->GetCharsetLangGroup(charSetInfo->mCharsetAtom,
                                                &charSetInfo->mLangGroup);
        if (NS_FAILED(res)) {
#ifdef NOISY_FONTS
#ifdef DEBUG
          printf("=== cannot get lang group for %s\n",
                 charSetInfo->mCharSet);
#endif
#endif
          return nsnull;
        }
      }
      nsCOMPtr<nsIAtom> charsetAtom
         = getter_AddRefs(NS_NewAtom(charSetInfo->mCharSet));
      if (!charsetAtom) {
        NS_WARNING("cannot get atom");
        return nsnull;
      }
      res = gCharSetManager->GetUnicodeEncoder(charsetAtom,&converter);
      if (NS_FAILED(res)) {
        NS_WARNING("cannot get Unicode converter");
        return nsnull;
      }
      res = converter->SetOutputErrorBehavior(converter->kOnError_Replace,
                                              nsnull,'?');
      nsCOMPtr<nsICharRepresentable> mapper
                                      = do_QueryInterface(converter);
      if (!mapper) {
        NS_WARNING("cannot get nsICharRepresentable");
        return nsnull;
      }
      charSetInfo->mMap = (PRUint32*)PR_Calloc(2048,4);
      if (nsnull == charSetInfo->mMap) {
        return nsnull;
      }
      res = mapper->FillInfo(charSetInfo->mMap);
 
      /*
       * XXX This is a bit of a hack. Documents containing the CP1252
       * extensions of Latin-1 (e.g. smart quotes) will display with
       * those special characters way too large. This is because they
       * happen to be in these large double byte fonts. So, we disable
       * those characters here. Revisit this decision later.
       */
      if (charSetInfo->mType == NSQ_DOUBLEBYTE) {
        PRUint32 *map = charSetInfo->mMap;
#undef REMOVE_CHAR
#define REMOVE_CHAR(map,c) (map)[(c) >> 5] &= ~(1L << ((c) & 0x1f))
        REMOVE_CHAR(map,0x20AC);
        REMOVE_CHAR(map,0x201A);
        REMOVE_CHAR(map,0x0192);
        REMOVE_CHAR(map,0x201E);
        REMOVE_CHAR(map,0x2026);
        REMOVE_CHAR(map,0x2020);
        REMOVE_CHAR(map,0x2021);
        REMOVE_CHAR(map,0x02C6);
        REMOVE_CHAR(map,0x2030);
        REMOVE_CHAR(map,0x0160);
        REMOVE_CHAR(map,0x2039);
        REMOVE_CHAR(map,0x0152);
        REMOVE_CHAR(map,0x017D);
        REMOVE_CHAR(map,0x2018);
        REMOVE_CHAR(map,0x2019);
        REMOVE_CHAR(map,0x201C);
        REMOVE_CHAR(map,0x201D);
        REMOVE_CHAR(map,0x2022);
        REMOVE_CHAR(map,0x2013);
        REMOVE_CHAR(map,0x2014);
        REMOVE_CHAR(map,0x02DC);
        REMOVE_CHAR(map,0x2122);
        REMOVE_CHAR(map,0x0161);
        REMOVE_CHAR(map,0x203A);
        REMOVE_CHAR(map,0x0153);
        REMOVE_CHAR(map,0x017E);
        REMOVE_CHAR(map,0x0178);
      }
    }
    if (!FONT_HAS_GLYPH(charSetInfo->mMap,aChar)) {
      return nsnull;
    }
  }
  qFont = LoadQFont(aName,aCharSet);
  if (qFont) {
    newFont = new nsFontQTNormal(qFont);
    if (!newFont) {
      delete qFont;
      return nsnull;
    }
    if (charSetInfo == &ISO106461) {
      if (!newFont->HasChar(aChar)) {
        delete newFont;
        return nsnull;
      }
    }
    if (mLoadedFontsCount == mLoadedFontsAlloc) {
      int newSize;
      if (mLoadedFontsAlloc) {
        newSize = (2 * mLoadedFontsAlloc);
      }
      else {
        newSize = 1;
      }
      nsFontQT **newPointer
                  = (nsFontQT**)PR_Realloc(mLoadedFonts,
                                           newSize * sizeof(nsFontQT*));
      if (newPointer) {
        mLoadedFonts = newPointer;
        mLoadedFontsAlloc = newSize;
      }
      else {
        delete newFont;
        return nsnull;
      }
    }
    if (mIsUserDefined) {
      mUserDefinedFont = new nsFontQTUserDefined();
      if (nsnull == mUserDefinedFont) {
        delete newFont;
        return nsnull;
      }
      mUserDefinedFont->Init(newFont);
      newFont = mUserDefinedFont;
    }
    else {
      newFont->mCharSetInfo = charSetInfo;
    }
    mLoadedFonts[mLoadedFontsCount++] = newFont; 
  }
  return newFont;
}

QFont*
nsFontMetricsQT::LoadQFont(QString &aName,const QString &aCharSet)
{
  QFontDatabase *qFontDB;
  QFont *qFont;
  QFont::CharSet charset;

  qFontDB = GetQFontDB();
  if (qFontDB->isSmoothlyScalable(aName,*mQStyle,aCharSet)) {
    qFont = new QFont(qFontDB->font(aName,*mQStyle,(int)mPixelSize,aCharSet));
    if (qFont->charSet() == QFont::AnyCharSet) {
      charset = getExtendedCharSet(aCharSet);
      if (charset == QFont::AnyCharSet) {
        delete qFont;
        return nsnull;
      }
      qFont->setCharSet(charset);
    }
  }
  else {
    typedef QValueList<int> QFSList;
    QFSList qSizes;
    PRUint16 curSz = mPixelSize, loSz = 0;
    PRBool exactMatch = PR_FALSE, nameFound = PR_FALSE;
 
    qSizes = qFontDB->smoothSizes(aName,*mQStyle,aCharSet);
    for (QFSList::Iterator szIt = qSizes.begin(); szIt != qSizes.end();
         ++szIt) {
      nameFound = PR_TRUE;
      curSz = (PRUint16)*szIt;
 
      if (curSz == mPixelSize) {
        exactMatch = PR_TRUE;
        break;
      }
      else if (curSz < mPixelSize) {
        loSz = curSz;
      }
      else {
        break;
      }
    }
    if (nameFound) {
      if (exactMatch) {
        qFont = new QFont(qFontDB->font(aName,*mQStyle,(int)mPixelSize,aCharSet));
        if (qFont->charSet() == QFont::AnyCharSet) {
          charset = getExtendedCharSet(aCharSet);
          if (charset == QFont::AnyCharSet) {
            delete qFont;
            return nsnull;
          }
          qFont->setCharSet(charset);
        }
      }
      else {
        PRUint16 loDiff,hiDiff,pixSz;
 
        loDiff = mPixelSize - loSz;
        hiDiff = curSz - mPixelSize;
        if (loSz != 0 && loDiff <= hiDiff) {
          pixSz = loSz;
        }
        else {
          pixSz = curSz;
        }
        qFont = new QFont(qFontDB->font(aName,*mQStyle,(int)pixSz,aCharSet));
        if (qFont->charSet() == QFont::AnyCharSet) {
          charset = getExtendedCharSet(aCharSet);
          if (charset == QFont::AnyCharSet) {
            delete qFont;
            return nsnull;
          }
          qFont->setCharSet(charset);
        }
      }
    }
    else {
      return nsnull;
    }
  }
  if (qFont->family() == aName || qFont->family() == QFont::substitute(aName)) {
    // We need to map CSS2 font weight (ranging from [100..900]) to
    // qt font weight (ranging from [0..99]) - JCB 2001-03-09
    qFont->setWeight((int)((((mWeight - 100) * 99) + 400) / 800));
  }
  else {
    delete qFont;
    return nsnull;
  }
  return qFont;
}

nsresult
nsFontMetricsQT::FamilyExists(const nsString &aName)
{ 
  if (!IsASCIIFontName(aName)) {
    return NS_ERROR_FAILURE;
  }
  QFontDatabase *qFontDB = GetQFontDB();
  QStringList qCharsets;

  nsCAutoString name;
  name.AssignWithConversion(aName.get());
  qCharsets = qFontDB->charSets(QString((char*)name.get()),FALSE);
  if (!qCharsets.isEmpty()) {
    return NS_OK;  
  } 
  return NS_ERROR_FAILURE;
}

static void
PrefEnumCallback(const char *aName,void *aClosure)
{
  nsFontSearch *s = (nsFontSearch*)aClosure;
  if (s->mFont) {
      return;
  }
  QString fontName,foundryName,charSetName;
  char *value = nsnull;
  gPref->CopyCharPref(aName,&value);
  nsCAutoString name;
  if (value) {
    name = value;
    nsMemory::Free(value);
    value = nsnull;

    s->mFont = s->mMetrics->LookUpFontPref(name,s->mChar);
  }
  if (!s->mFont) {
    gPref->CopyDefaultCharPref(aName,&value);
    if (value) {
      name = value;
      nsMemory::Free(value);
      value = nsnull;
      s->mFont = s->mMetrics->LookUpFontPref(name,s->mChar);
    }
  }
}

nsFontQT*
nsFontMetricsQT::FindLangGroupPrefFont(nsIAtom *aLangGroup,
                                       PRUnichar aChar)
{
  nsFontQT *font = nsnull;
  nsCAutoString prefix("font.name.");

  prefix.Append(*mGeneric);
  if (aLangGroup) {
    // check user set pref
    nsCAutoString pref = prefix;
    const PRUnichar *langGroup = nsnull;
    char *value = nsnull;
    nsCAutoString str;
    nsCAutoString str_user;

    pref.Append(char('.'));
    aLangGroup->GetUnicode(&langGroup);
    pref.AppendWithConversion(langGroup);
    gPref->CopyCharPref(pref.get(), &value);
    if (value) {
      str = value;
      str_user = value;
      nsMemory::Free(value);
      value = nsnull;
      font = LookUpFontPref(str,aChar);
      if (font) {
        NS_ASSERTION(font->SupportsChar(aChar), "font supposed to support this char");
        return font;
      }
    }
    value = nsnull;
    // check factory set pref
    gPref->CopyDefaultCharPref(pref.get(), &value);
    if (value) {
      str = value;
      // check if we already tried this name
      if (str != str_user) {
        nsMemory::Free(value);
        value = nsnull;
        font = LookUpFontPref(str,aChar);
        if (font) {
          NS_ASSERTION(font->SupportsChar(aChar), "font supposed to support this char");
          return font;
        }
      }
    }
  }
  return nsnull;
}

nsFontQT*
nsFontMetricsQT::FindGenericFont(PRUnichar aChar)
{
  nsFontQT *font = nsnull;

  if (mTriedAllGenerics) {
    return nsnull;
  }
  if (mLangGroup) {
    font = FindLangGroupPrefFont(mLangGroup,aChar);
    if (font) {
      NS_ASSERTION(font->SupportsChar(aChar), "font supposed to support this char");
      return(font);
    }
  }
  if (mLangGroup != gUsersLocale) {
    font = FindLangGroupPrefFont(gUsersLocale,aChar);
    if (font) {
      NS_ASSERTION(font->SupportsChar(aChar), "font supposed to support this char");
      return(font);
    }
  }
  nsCAutoString prefix("font.name.");
  nsFontSearch search = { this,aChar,nsnull };

  prefix.Append(*mGeneric);
  gPref->EnumerateChildren(prefix.get(),PrefEnumCallback,&search);
  if (search.mFont) {
    NS_ASSERTION(search.mFont->SupportsChar(aChar), "font supposed to support this char");
    return search.mFont;
  }
  //
  // Search all font prefs
  //
  // find based on all prefs (no generic part (eg: sans-serif))
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

nsFontQT*
nsFontMetricsQT::FindLocalFont(PRUnichar aChar)
{
  while (mFontsIndex < mFonts.Count()) {
    if (mFontIsGeneric[mFontsIndex]) {
      return nsnull;
    }
    QString qName(*(mFonts.CStringAt(mFontsIndex++))); 
    nsFontQT *font;

    font = LoadFont(qName,aChar);
    if (font) {
      return font;
    }
  }
  return nsnull;
}

nsFontQT*
nsFontMetricsQT::FindGlobalFont(PRUnichar aChar)
{
  QStringList qFamilies,qCharsets;
  QFontDatabase *qFontDB = GetQFontDB();
  nsFontQT *font;

  qFamilies = qFontDB->families(FALSE);
  qFamilies.sort();
  for (QStringList::Iterator famIt = qFamilies.begin(); famIt != qFamilies.end(); ++famIt) {
    qCharsets = nsFontMetricsQT::GetQFontDB()->charSets(*famIt,FALSE);
    qCharsets.sort();
    for (QStringList::Iterator csIt = qCharsets.begin();
         csIt != qCharsets.end(); ++csIt) {
      font = LoadFont(*famIt,*csIt,aChar);
      if (font) {
        return font;
      }
    }
  }
  return nsnull;
}

nsFontQT*
nsFontMetricsQT::FindSubstituteFont(PRUnichar aChar)
{ 
  if (!mSubstituteFont) {
    for (int i = 0; i < mLoadedFontsCount; i++) {
      if (mLoadedFonts[i]->SupportsChar('a')) {
        mSubstituteFont = new nsFontQTSubstitute(mLoadedFonts[i]);
        break;
      }
    }
  }
  return mSubstituteFont; 
}
 

nsFontQT*
nsFontMetricsQT::FindUserDefinedFont(PRUnichar aChar)
{ 
  if (mIsUserDefined) {
    if (!mUserDefinedFont) {
      QString fontName(mUserDefined.get());
      LoadFont(fontName,aChar);
    }
    return mUserDefinedFont;
  }
  return nsnull;
}

/*
 * First we try to load the user-defined font, if the user-defined charset
 * has been selected in the menu.
 *
 * Next, we try the fonts listed in the font-family property (FindLocalFont).
 *
 * Next, we try any CSS generic font encountered in the font-family list and
 * all of the fonts specified by the user for the generic (FindGenericFont).
 *
 * Next, we try all of the fonts on the system (FindGlobalFont). This is
 * expensive on some Unixes.
 *
 * Finally, we try to create a substitute font that offers substitute glyphs
 * for the characters (FindSubstituteFont).
 */
nsFontQT*
nsFontMetricsQT::FindFont(PRUnichar aChar)
{
  if (!mCharSubst.isEmpty()) {
    if (mCharSubst.find((long)aChar)) {
      return mSubstituteFont;
    }
  }
  nsFontQT *font = FindUserDefinedFont(aChar);
  if (!font) {
    font = FindLocalFont(aChar);
    if (!font) {
      font = FindGenericFont(aChar);
      if (!font) {
        font = FindGlobalFont(aChar);
        if (!font) {
          font = FindSubstituteFont(aChar);
          if (font) {
            mCharSubst.insert((long)aChar,"ok");
          }
        }
      }
    }
  }
  return font;
}
 
nsresult
nsFontMetricsQT::GetSpaceWidth(nscoord &aSpaceWidth)
{
  aSpaceWidth = mSpaceWidth;
  return NS_OK;
}

MOZ_DECL_CTOR_COUNTER(nsFontQT);
 
nsFontQT::nsFontQT()
{
  MOZ_COUNT_CTOR(nsFontQT);
  mFont = nsnull;
  mFontInfo = nsnull;
  mFontMetrics = nsnull;
}
 
nsFontQT::nsFontQT(QFont *aQFont)
{
#ifdef DBG_JCG
  gQFontCount++;
  mID = gQFontID++;
  printf("JCG: nsFontQT CTOR (%p) ID: %d, Count: %d\n",this,mID,gQFontCount);
#endif
  MOZ_COUNT_CTOR(nsFontQT);
  mFont = aQFont;
  mFontInfo = new QFontInfo(*mFont);
  mFontMetrics = new QFontMetrics(*mFont);
}
 
nsFontQT::~nsFontQT()
{
#ifdef DBG_JCG
  gQFontCount--;
  printf("JCG: nsFontQT DTOR (%p) ID: %d, Count: %d\n",this,mID,gQFontCount);
#endif
  MOZ_COUNT_DTOR(nsFontQT);
  if (mFont) {
    delete mFont;
    mFont = nsnull;
  }
  if (mFontInfo) {
    delete mFontInfo;
    mFontInfo = nsnull;
  }
  if (mFontMetrics) {
    delete mFontMetrics;
    mFontMetrics = nsnull;
  }
}

PRUint32 *nsFontQT::GetCharSetMap()
{
  return mCharSetInfo->mMap;
}
 
QFont*
nsFontQT::GetQFont(void)
{
  return mFont;
}

int nsFontQT::SupportsChar(PRUnichar aChar)
{
  if (IsUnicodeFont())
    return HasChar(aChar);
  else
    return (mFont && FONT_HAS_GLYPH(GetCharSetMap(),aChar));
}

PRBool nsFontQT::IsUnicodeFont()
{
  return(mCharSetInfo == &ISO106461);
}
 
nsFontQTNormal::nsFontQTNormal() 
{
}
 
nsFontQTNormal::nsFontQTNormal(QFont *aQFont) 
 : nsFontQT(aQFont)
{
}
 
nsFontQTNormal::~nsFontQTNormal()
{
}
 
int
nsFontQTNormal::GetWidth(const PRUnichar *aString,PRUint32 aLength)
{
  int result = 0;

  if (mFontMetrics) {
    QChar *buf = new QChar[aLength + 1];

    for (PRUint32 i = 0; i < aLength; i++) {
      buf[i] = QChar(*aString++); 
    }
    result = mFontMetrics->width(QString(buf,aLength));
    delete [] buf;
  }
  return result;
}
 
int
nsFontQTNormal::DrawString(nsRenderingContextQT *aContext,
                           nsDrawingSurfaceQT *aSurface,
                           nscoord aX,nscoord aY,
                           const PRUnichar *aString,PRUint32 aLength)
{
  int result = 0; 
  if (mFontMetrics) {
    QChar *buf = new QChar[aLength + 1];
 
    for (PRUint32 i = 0; i < aLength; i++) {
      buf[i] = QChar(*aString++); 
    }
    QString qStr(buf,aLength);
    aContext->MyDrawString(aSurface->GetGC(),aX,aY,qStr);
    result = mFontMetrics->width(qStr);
    delete [] buf;
  }
  return result;
}
 
#ifdef MOZ_MATHML
// bounding metrics for a string
// remember returned values are not in app units
nsresult
nsFontQTNormal::GetBoundingMetrics(const PRUnichar *aString,
                                   PRUint32 aLength,
                                   nsBoundingMetrics &aBoundingMetrics)
{
  aBoundingMetrics.Clear();
  if (!mFont) {
    return NS_ERROR_FAILURE;
  }
  if (aString && 0 < aLength) {
    QChar *buf = new QChar[aLength + 1];
 
    for (int i = 0; i < aLength; i++) {
      buf[i] = QChar(*aString++);
    } 
    QString qStr(buf,aLength);

    aBoundingMetrics.width = mFontMetrics->width(qStr);
    aBoundingMetrics.leftBearing
      = mFontMetrics->leftBearing(buf[0]);
    aBoundingMetrics.rightBearing
      = mFontMetrics->rightBearing(buf[aLength - 1]);
    aBoundingMetrics.ascent = mFontMetrics->ascent();
    aBoundingMetrics.descent = mFontMetrics->descent();
    delete [] buf;
  }
  return NS_OK;
}
#endif

nsFontQTSubstitute::nsFontQTSubstitute(nsFontQT *aFont)
{
  gCount++;
  mSubstituteFont = aFont;
}
 
nsFontQTSubstitute::~nsFontQTSubstitute()
{
  if (!--gCount) {
    NS_IF_RELEASE(gConverter);
  }
  // Do not free mSubstituteFont here. It is owned by somebody else.
}
 
PRUint32
nsFontQTSubstitute::Convert(const PRUnichar *aSrc,PRUint32 aSrcLen,
                            PRUnichar *aDest,PRUint32 aDestLen)
{
  nsresult res;

  if (!gConverter) {
    nsComponentManager::CreateInstance(kSaveAsCharsetCID,nsnull,
                                       NS_GET_IID(nsISaveAsCharset),
                                       (void**)&gConverter);
    if (gConverter) {
      res = gConverter->Init("ISO-8859-1",
                             nsISaveAsCharset::attr_FallbackQuestionMark
                              + nsISaveAsCharset::attr_EntityBeforeCharsetConv,
                             nsIEntityConverter::transliterate);
      if (NS_FAILED(res)) {
        NS_RELEASE(gConverter);
      }
    }
  }
  if (gConverter) {
    nsAutoString tmp(aSrc,aSrcLen);
    char *conv = nsnull;
    res = gConverter->Convert(tmp.get(),&conv);
    if (NS_SUCCEEDED(res) && conv) {
      char *p = conv;
      PRUint32 i;

      for (i = 0; i < aDestLen; i++) {
        if (*p) {
          aDest[i] = *p;
        }
        else {
          break;
        }
        p++;
      }
      nsMemory::Free(conv);
      conv = nsnull;
      return i;
    }
  }
  if (aSrcLen > aDestLen) {
    aSrcLen = aDestLen;
  }
  for (PRUint32 i = 0; i < aSrcLen; i++) {
    aDest[i] = '?';
  }
  return aSrcLen;
}
 
int
nsFontQTSubstitute::GetWidth(const PRUnichar *aString,PRUint32 aLength)
{
  PRUnichar buf[512];
  PRUint32 len = Convert(aString,aLength,buf,sizeof(buf) / 2);

  return mSubstituteFont->GetWidth(buf,len);
}
 
int
nsFontQTSubstitute::DrawString(nsRenderingContextQT *aContext,
                               nsDrawingSurfaceQT *aSurface,
                               nscoord aX,nscoord aY,
                               const PRUnichar *aString,PRUint32 aLength)
{
  PRUnichar buf[512];
  PRUint32 len = Convert(aString,aLength,buf,sizeof(buf) / 2);

  return mSubstituteFont->DrawString(aContext,aSurface,aX,aY,buf,len);
}
 
#ifdef MOZ_MATHML
// bounding metrics for a string
// remember returned values are not in app units
nsresult
nsFontQTSubstitute::GetBoundingMetrics(const PRUnichar *aString,
                                       PRUint32 aLength,
                                       nsBoundingMetrics &aBoundingMetrics)
{
  PRUnichar buf[512]; // XXX watch buffer length !!!
  PRUint32 len = Convert(aString,aLength,buf,sizeof(buf) / 2);

  return mSubstituteFont->GetBoundingMetrics(buf,len,aBoundingMetrics);
}
#endif
 
QFont*
nsFontQTSubstitute::GetQFont(void)
{
  return mSubstituteFont->GetQFont();
}

int nsFontQTSubstitute::SupportsChar(PRUnichar aChar)
{
  return mSubstituteFont->SupportsChar(aChar);
}
 
nsFontQTUserDefined::nsFontQTUserDefined()
{
  mMap = nsnull;
}
 
nsFontQTUserDefined::~nsFontQTUserDefined()
{
  // Do not free mFont here. It is owned by somebody else.
}
 
PRBool
nsFontQTUserDefined::Init(nsFontQT *aFont)
{
  mFont = aFont->GetQFont();
  mFontInfo = new QFontInfo(*mFont);
  mFontMetrics = new QFontMetrics(*mFont);
  mMap = gUserDefinedMap;
  return PR_TRUE;
} 

PRUint32
nsFontQTUserDefined::Convert(const PRUnichar *aSrc,PRInt32 aSrcLen,
                             char *aDest,PRInt32 aDestLen)
{
  if (aSrcLen > aDestLen) {
    aSrcLen = aDestLen;
  }
  gUserDefinedConverter->Convert(aSrc,&aSrcLen,aDest,&aDestLen);
  return aSrcLen;
}
 
int
nsFontQTUserDefined::GetWidth(const PRUnichar *aString,PRUint32 aLength)
{
  int result = 0;

  if (mFontMetrics) {
    QChar *buf = new QChar[aLength + 1];

    for (PRUint32 i = 0; i < aLength; i++) {
      buf[i] = QChar(*aString++);
    }
    result = mFontMetrics->width(QString(buf,aLength));
    delete [] buf;
  } 
  return result;
}
 
int
nsFontQTUserDefined::DrawString(nsRenderingContextQT *aContext,
                                nsDrawingSurfaceQT *aSurface,
                                nscoord aX,nscoord aY,
                                const PRUnichar *aString,PRUint32 aLength)
{
  int result = 0;
 
  if (mFontMetrics) {
    QChar *buf = new QChar[aLength + 1];
 
    for (PRUint32 i = 0; i < aLength; i++) {
      buf[i] = QChar(*aString++);
    }
    QString qStr(buf,aLength);

    aContext->MyDrawString(aSurface->GetGC(),aX,aY,qStr);
    result = mFontMetrics->width(qStr);
    delete [] buf;
  }
  return result;
}
 
#ifdef MOZ_MATHML
// bounding metrics for a string
// remember returned values are not in app units
nsresult
nsFontQTUserDefined::GetBoundingMetrics(const PRUnichar *aString,
                                        PRUint32 aLength,
                                        nsBoundingMetrics &aBoundingMetrics)
{
  aBoundingMetrics.Clear();
  if (!mFontMetrics) {
    return NS_ERROR_FAILURE;
  }
  if (aString && 0 < aLength) {
    QChar *buf = new QChar[aLength + 1];
 
    for (int i = 0; i < aLength; i++) {
      buf[i] = QChar(*aString++);
    } 
    QString qStr(buf,aLength);

    aBoundingMetrics.width = mFontMetrics->width(qStr);
    aBoundingMetrics.leftBearing = mFontMetrics->leftBearing(buf[0]);
    aBoundingMetrics.rightBearing = mFontMetrics->rightBearing(buf[aLength - 1]);
    aBoundingMetrics.ascent = mFontMetrics->ascent();
    aBoundingMetrics.descent = mFontMetrics->descent();
    delete [] buf;
  }
  return NS_OK;
} 
#endif

// The Font Enumerator
nsFontEnumeratorQT::nsFontEnumeratorQT()
{
  NS_INIT_REFCNT();
}
 
NS_IMPL_ISUPPORTS1(nsFontEnumeratorQT, nsIFontEnumerator)

static int CompareFontNames(const void *aArg1,const void *aArg2,void *aClosure)
{
  const PRUnichar *str1 = *((const PRUnichar**)aArg1);
  const PRUnichar *str2 = *((const PRUnichar**)aArg2);
 
  // XXX add nsICollation stuff
  return nsCRT::strcmp(str1,str2);
}

struct FontEnumNode
{
  PRUnichar *name;
  struct FontEnumNode *next;
};

typedef struct FontEnumNode FontEnumNode;

static nsresult EnumFonts(nsIAtom *aLangGroup,const char *aGeneric,
                          PRUint32 *aCount,PRUnichar ***aResult)
{
  QFontDatabase *qFontDB = nsFontMetricsQT::GetQFontDB();
  QStringList qCharsets,qFamilies;
  FontEnumNode *head = nsnull,*tail = nsnull;
  int count = 0;

  /* Get list of all fonts */
  qFamilies = qFontDB->families(FALSE);
  qFamilies.sort();
  for (QStringList::Iterator famIt = qFamilies.begin();
       famIt != qFamilies.end(); ++famIt) {
    qCharsets = qFontDB->charSets(*famIt,FALSE);
    qCharsets.sort();
    for (QStringList::Iterator csIt = qCharsets.begin();
         csIt != qCharsets.end(); ++csIt) {
      nsCStringKey charSetKey((*csIt).latin1());
 
      nsFontCharSetInfo *charSetInfo
       = (nsFontCharSetInfo*)gCharSets->Get(&charSetKey);

      // indirection for font specific charset encoding
      if (charSetInfo == &Special) {
        nsCAutoString familyCharSetName((*famIt).latin1());
        familyCharSetName.Append('-');
        familyCharSetName.Append((*csIt).latin1());
        nsCStringKey familyCharSetKey(familyCharSetName);
        charSetInfo
          = (nsFontCharSetInfo*)gSpecialCharSets->Get(&familyCharSetKey);
      }
      if (!charSetInfo) {
        continue;
      }
      if (aLangGroup != gUserDefined && charSetInfo == &Unknown) {
        continue;
      }
      if (aLangGroup != gUnicode) {
        nsresult res;

        if (!charSetInfo->mCharsetAtom) {
          res = gCharSetManager->GetCharsetAtom2(charSetInfo->mCharSet,
                                                 &charSetInfo->mCharsetAtom);
          if (NS_FAILED(res)) {
            continue;
          }
        }
        if (!charSetInfo->mLangGroup) {
          res = gCharSetManager->GetCharsetLangGroup(charSetInfo->mCharsetAtom,
                                                     &charSetInfo->mLangGroup);
          if (NS_FAILED(res)) {
            continue;
          }
        }
        if (aLangGroup != charSetInfo->mLangGroup) {
          continue;
        }
      }
      nsCAutoString name((*famIt).latin1());
      FontEnumNode *node = (FontEnumNode*)nsMemory::Alloc(sizeof(FontEnumNode));

      if (!node) {
        FontEnumNode *ptr = head,*tmp;
        while (ptr) {
          tmp = ptr;
          nsMemory::Free(ptr->name);
          ptr = ptr->next;
          nsMemory::Free(tmp);
        }
        return NS_ERROR_OUT_OF_MEMORY;
      }
      node->name = ToNewUnicode(name);
      if (!node->name) {
        FontEnumNode *ptr = head,*tmp;
        while (ptr) {
          tmp = ptr;
          nsMemory::Free(ptr->name);
          ptr = ptr->next;
          nsMemory::Free(tmp);
        }
        return NS_ERROR_OUT_OF_MEMORY;
      }
      node->next = nsnull;
      if (!head) {
        head = node;
      }
      else {
        tail->next = node;
      }
      tail = node;
      count++;
    }
  }
  PRUnichar **array = (PRUnichar**)nsMemory::Alloc(count * sizeof(PRUnichar*));
  if (!array) {
    FontEnumNode *ptr = head,*tmp;
    while (ptr) {
      tmp = ptr;
      nsMemory::Free(ptr->name);
      ptr = ptr->next;
      nsMemory::Free(tmp);
    }
    return NS_ERROR_OUT_OF_MEMORY;
  }
  FontEnumNode *ptr = head,*tmp;
  for (int i = 0; i < count; i++) {
    tmp = ptr;
    array[i] = ptr->name;
    ptr = ptr->next;
    nsMemory::Free(tmp);
  }
  NS_QuickSort(array,count,sizeof(PRUnichar*),CompareFontNames,nsnull);
 
  *aCount = count;
  *aResult = array;
  return NS_OK;
}

NS_IMETHODIMP
nsFontEnumeratorQT::EnumerateAllFonts(PRUint32 *aCount,PRUnichar ***aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = nsnull;
  NS_ENSURE_ARG_POINTER(aCount);
  *aCount = 0;

  return EnumFonts(nsnull,nsnull,aCount,aResult);
}

NS_IMETHODIMP
nsFontEnumeratorQT::EnumerateFonts(const char *aLangGroup,const char *aGeneric,
                                   PRUint32 *aCount,PRUnichar ***aResult)
{ 
  nsresult res;

  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = nsnull;
  NS_ENSURE_ARG_POINTER(aCount);
  *aCount = 0;
  NS_ENSURE_ARG_POINTER(aGeneric);
  NS_ENSURE_ARG_POINTER(aLangGroup);

  nsIAtom *langGroup = NS_NewAtom(aLangGroup);

  res = EnumFonts(langGroup,aGeneric,aCount,aResult);
  NS_IF_RELEASE(langGroup);
  return(res);
}

NS_IMETHODIMP
nsFontEnumeratorQT::HaveFontFor(const char *aLangGroup,PRBool *aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  NS_ENSURE_ARG_POINTER(aLangGroup);

  *aResult = PR_TRUE; // always return true for now.
  // Finish me
  return NS_OK;
}

NS_IMETHODIMP
nsFontEnumeratorQT::UpdateFontList(PRBool *updateFontList)
{
  *updateFontList = PR_FALSE; // always return false for now
  return NS_OK;
}

