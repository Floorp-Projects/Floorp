/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 */

#include "xp_core.h"
#include "nsQuickSort.h"
#include "nsFontMetricsXlib.h"
#include "nsIServiceManager.h"
#include "nsICharsetConverterManager.h"
#include "nsICharsetConverterManager2.h"
#include "nsICharRepresentable.h"
#include "nsISaveAsCharset.h"
#include "nsIPref.h"
#include "nsILocale.h"
#include "nsILocaleService.h"
#include "nsLocaleCID.h"
#include "nsCOMPtr.h"
#include "nspr.h"
#include "plhash.h"

#include "nsHashtable.h"

#include <X11/Xatom.h>
#include <stdlib.h>
#include "xlibrgb.h"

#undef USER_DEFINED
#define USER_DEFINED "x-user-def"
#define NOISY_FONTS 1


// these are in the widget set

static NS_DEFINE_CID(kCharSetManagerCID, NS_ICHARSETCONVERTERMANAGER_CID);
static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);
static NS_DEFINE_CID(kSaveAsCharsetCID, NS_SAVEASCHARSET_CID);

static NS_DEFINE_IID(kIFontMetricsIID, NS_IFONT_METRICS_IID);

static PRLogModuleInfo * FontMetricsXlibLM = PR_NewLogModule("FontMetricsXlib");

static Display *gDisplay = NULL;

static int gFontMetricsXlibCount = 0;
static int gInitialized = 0;

// Prototype some structs
struct nsFontCharSetMap;
struct nsFontFamilyName;
struct nsFontPropertyName;
struct nsFontStyle;
struct nsFontWeight;

class nsFontNodeArray : public nsVoidArray
{
public:
    nsFontNode* GetElement(PRInt32 aIndex)
    {
      return (nsFontNode*) ElementAt(aIndex);
    };
};

struct nsFontCharSetInfo
{
  const char*            mCharSet;
  nsFontCharSetConverter Convert;
  PRUint8                mSpecialUnderline;
  PRUint32*              mMap;
  nsIUnicodeEncoder*     mConverter;
  nsIAtom*               mLangGroup;
};

struct nsFontCharSetMap
{
  char*              mName;
  nsFontCharSetInfo* mInfo;
};

struct nsFontFamily
{
  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  nsFontNodeArray mNodes;
};

struct nsFontFamilyName
{
  char* mName;
  char* mXName;
};

struct nsFontNode
{
  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  void FillStyleHoles(void);

  nsCAutoString       mName;
  nsFontCharSetInfo*  mCharSetInfo;
  nsFontStyle*        mStyles[3];
  PRUint8             mHolesFilled;
  PRUint8             mDummy;
};

struct nsFontPropertyName
{
  char* mName;
  int   mValue;
};

struct nsFontStretch
{
  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  void SortSizes(void);

  nsFontXlib**       mSizes;
  PRUint16           mSizesAlloc;
  PRUint16           mSizesCount;

  char*              mScalable;
  // New code
  // XXX-Tony Must change old way of using mScaled fonts to new way.
  nsVoidArray        mScaledFonts;

};


struct nsFontStyle
{
  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  void FillWeightHoles(void);

  nsFontWeight* mWeights[9];
};

struct nsFontWeight
{
  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  void FillStretchHoles(void);

  nsFontStretch* mStretches[9];
};

// Global variables
// XXX many of these statics need to be freed at shutdown time

static nsIPref* gPref = nsnull;
static nsICharsetConverterManager2* gCharSetManager = nsnull;
static nsIUnicodeEncoder* gUserDefinedConverter = nsnull;

static nsFontNodeArray* gGlobalList = nsnull;
static nsIAtom* gUnicode = nsnull;
static nsIAtom* gUserDefined = nsnull;

static nsHashtable* gAliases = nsnull;
static nsHashtable* gCharSets = nsnull;
static nsHashtable* gFamilies = nsnull;
static nsHashtable* gNodes = nsnull;
static nsHashtable* gSpecialCharSets = nsnull;
static nsHashtable* gStretches = nsnull;
static nsHashtable* gWeights = nsnull;

static nsFontCharSetInfo Unknown = { nsnull };
static nsFontCharSetInfo Special = { nsnull };

static int SingleByteConvert(nsFontCharSetInfo* aSelf, XFontStruct* aFont,
                            const PRUnichar* aSrcBuf, PRInt32 aSrcLen, 
          		              char* aDestBuf, PRInt32 aDestLen);
				      
static int DoubleByteConvert(nsFontCharSetInfo* aSelf, XFontStruct* aFont, 
                            const PRUnichar* aSrcBuf, PRInt32 aSrcLen, char* aDestBuf, 
          		              PRInt32 aDestLen);

static int ISO10646Convert(nsFontCharSetInfo* aSelf, XFontStruct* aFont,
                           const PRUnichar* aSrcBuf, PRInt32 aSrcLen, 
              	           char* aDestBuf, PRInt32 aDestLen);

static nsFontCharSetInfo CP1251 =
  { "windows-1251", SingleByteConvert, 0 };
static nsFontCharSetInfo ISO88591 =
  { "ISO-8859-1", SingleByteConvert, 0 };
static nsFontCharSetInfo ISO88592 =
  { "ISO-8859-2", SingleByteConvert, 0 };
static nsFontCharSetInfo ISO88593 =
  { "ISO-8859-3", SingleByteConvert, 0 };
static nsFontCharSetInfo ISO88594 =
  { "ISO-8859-4", SingleByteConvert, 0 };
static nsFontCharSetInfo ISO88595 =
  { "ISO-8859-5", SingleByteConvert, 0 };
static nsFontCharSetInfo ISO88596 =
  { "ISO-8859-6", SingleByteConvert, 0 };
static nsFontCharSetInfo ISO88597 =
  { "ISO-8859-7", SingleByteConvert, 0 };
static nsFontCharSetInfo ISO88598 =
  { "ISO-8859-8", SingleByteConvert, 0 };
//  change from we fix the layout and ensure we only call this with pure RTL text
//  { "ISO-8859-8", SingleByteConvertReverse, 0 };
//  utill we fix the layout and ensure we only call this with pure RTL text
static nsFontCharSetInfo ISO88599 =
  { "ISO-8859-9", SingleByteConvert, 0 };
static nsFontCharSetInfo ISO885915 =
  { "ISO-8859-15", SingleByteConvert, 0 };
static nsFontCharSetInfo JISX0201 =
  { "jis_0201", SingleByteConvert, 1 };
static nsFontCharSetInfo KOI8R =
  { "KOI8-R", SingleByteConvert, 0 };
static nsFontCharSetInfo KOI8U = 
  { "KOI8-U", SingleByteConvert, 0 };
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
static nsFontCharSetInfo GBK = 
  { "x-gbk", DoubleByteConvert, 1};
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

static nsFontCharSetInfo AdobeSymbol =
   { "Adobe-Symbol-Encoding", SingleByteConvert, 0 };
static nsFontCharSetInfo CMCMEX =
   { "x-cm-cmex", SingleByteConvert, 0 };
static nsFontCharSetInfo CMCMSY =
   { "x-cm-cmsy", SingleByteConvert, 0 };

#ifdef MOZ_MATHML
static nsFontCharSetInfo Mathematica1 =
  { "x-mathematica1", SingleByteConvert, 0 };
static nsFontCharSetInfo Mathematica2 =
  { "x-mathematica2", SingleByteConvert, 0 };
static nsFontCharSetInfo Mathematica3 =
  { "x-mathematica3", SingleByteConvert, 0 };
static nsFontCharSetInfo Mathematica4 =
  { "x-mathematica4", SingleByteConvert, 0 };
static nsFontCharSetInfo Mathematica5 =
  { "x-mathematica5", SingleByteConvert, 0 };
#endif
                   
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
  { "-ascii",             &Unknown        },
  { "-ibm pc",            &Unknown        },
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
  { "cp1251-1",           &CP1251        },
  { "dec-dectech",        &Unknown       },
  { "dtsymbol-1",         &Unknown       },
  { "fontspecific-0",     &Unknown       },
  { "gb2312.1980-0",      &GB2312        },
  { "gb2312.1980-1",      &GB2312        },
  { "gb13000.1993-1",     &GBK           },  
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
  { "microsoft-cp1251",   &CP1251        },    
  { "misc-fontspecific",  &Unknown       },
  { "sgi-fontspecific",   &Unknown       },
  { "sun-fontspecific",   &Unknown       },
  { "sunolcursor-1",      &Unknown       },
  { "sunolglyph-1",       &Unknown       },
  { "tis620.2529-1",      &TIS620        },
  { "ucs2.cjk-0",         &Unknown       },
  { "ucs2.cjk_japan-0",   &Unknown       },
  { "ucs2.cjk_taiwan-0",  &Unknown       },

  { nsnull,               nsnull         }
};


static nsFontCharSetMap gSpecialCharSetMap[] =
{
  { "symbol-adobe-fontspecific", &AdobeSymbol  },
  { "cmex10-adobe-fontspecific", &CMCMEX  },
  { "cmsy10-adobe-fontspecific", &CMCMSY  },

#ifdef MOZ_MATHML
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

static nsFontFamilyName gFamilyNameTable[] =
{
#ifdef MOZ_MATHML
  { "cmex",             "cmex10" },
  { "cmsy",             "cmsy10" },
  { "-moz-math-text",   "times" },
  { "-moz-math-symbol", "symbol" },
#endif
  { "arial",           "helvetica" },
  { "courier new",     "courier" },
  { "times new roman", "times" },
  { nsnull, nsnull }
};

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

static PRUint32 gUserDefinedMap[2048];

static PRBool
FreeCharSet (nsHashKey* aKey, void* aData, void* aClosure)
{
  nsFontCharSetInfo* charset = (nsFontCharSetInfo*) aData;
  NS_IF_RELEASE(charset->mConverter);
  NS_IF_RELEASE(charset->mLangGroup);
  PR_FREEIF(charset->mMap);

  return PR_TRUE;
}

static PRBool
FreeFamily(nsHashKey* aKey, void* aData, void* aClosure)
{
  delete (nsFontFamily*) aData;

  return PR_TRUE;
}

static void
FreeStretch(nsFontStretch* aStretch)
{
  PR_smprintf_free(aStretch->mScalable);
  for (int i=0; i < aStretch->mSizesCount; i++)
    delete aStretch->mSizes[i];

  delete [] aStretch->mSizes;
  delete aStretch;
}

static void
FreeWeights(nsFontWeight* aWeight)
{
  for (int i=0; i < 9; i++)
  {
    if (aWeight->mStretches[i])
    {
      for (int j=i+1; j < 9; j++)
      {
        if (aWeight->mStretches[j] == aWeight->mStretches[i])
          aWeight->mStretches[j] = nsnull;
      }
      FreeStretch(aWeight->mStretches[i]);
    }
  }
  delete aWeight;
}
  
static void
FreeStyle(nsFontStyle* aStyle)
{
  for (int i=0; i < 9; i++)
  {
    if (aStyle->mWeights[i])
    {
      for (int j = i+1; j < 9; j++)
      {
        if (aStyle->mWeights[j] == aStyle->mWeights[i])
          aStyle->mWeights[j] = nsnull;
      }
      FreeWeights(aStyle->mWeights[i]);
    }
  }
  delete aStyle;
}

static PRBool
FreeNode(nsHashKey* aKey, void* aData, void* aClosure)
{
  nsFontNode* node = (nsFontNode*) aData;
  for (int i=0; i < 3; i++)
  {
    if (node->mStyles[i])
    {
      for (int j=i+1; j < 3; j++)
      {
        if (node->mStyles[j] == node->mStyles[i])
          node->mStyles[j] = nsnull;
      }
      FreeStyle(node->mStyles[i]);
    }
  }
  
  delete node;
  return PR_TRUE;
}

static void
FreeGlobals(void)
{
  gInitialized = 0;
  
  if (gAliases)
  {
    delete gAliases;
    gAliases = nsnull;
  }

  NS_IF_RELEASE(gCharSetManager);

  if (gCharSets)
  {
    gCharSets->Reset(FreeCharSet, nsnull);
    delete gCharSets;
    gCharSets = nsnull;
  }

  if (gFamilies)
  {
    gFamilies->Reset(FreeFamily, nsnull);
    delete gFamilies;
    gFamilies = nsnull;
  }

  if (gGlobalList)
  {
    delete gGlobalList;
    gGlobalList = nsnull;
  }

  if (gNodes)
  {
    gNodes->Reset(FreeNode, nsnull);
    delete gNodes;
    gNodes = nsnull;
  }

  NS_IF_RELEASE(gPref);
  
  if (gSpecialCharSets)
  {
    delete gSpecialCharSets;
    gSpecialCharSets = nsnull;
  }

  if (gStretches)
  {
    delete gStretches;
    gStretches = nsnull;
  }

  NS_IF_RELEASE(gUnicode);
  NS_IF_RELEASE(gUserDefinedConverter);
  NS_IF_RELEASE(gUserDefined);

  if (gWeights)
  {
    delete gWeights;
    gWeights = nsnull;
  }
  
}

/*
 * Initialize all the font lookup hash tables and other globals
 */
static nsresult
InitGlobals(void)
{

  nsServiceManager::GetService(kCharSetManagerCID, 
                              NS_GET_IID(nsICharsetConverterManager2),
                              (nsISupports**) &gCharSetManager);
  if (!gCharSetManager)
  {
    FreeGlobals();
    return NS_ERROR_FAILURE;
   }

  nsServiceManager::GetService(kPrefCID, NS_GET_IID(nsIPref), 
                               (nsISupports**) &gPref);
  if (!gPref)
  {
    FreeGlobals();
    return NS_ERROR_FAILURE;
  }
     
  gNodes = new nsHashtable();
  if (!gNodes) {
    FreeGlobals();
    return NS_ERROR_OUT_OF_MEMORY;
  }

  gFamilies = new nsHashtable();
  if (!gFamilies)
  {
    FreeGlobals();
    return NS_ERROR_OUT_OF_MEMORY;
  }  
  
  gAliases = new nsHashtable();
  if (!gAliases)
  {
    FreeGlobals();
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsFontFamilyName* f = gFamilyNameTable;
  while (f->mName)
  {
    nsCStringKey key(f->mName);
    gAliases->Put(&key, f->mXName);
    f++;
  }

  gWeights = new nsHashtable();
  if (!gWeights)
  {
    FreeGlobals();
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsFontPropertyName* p = gWeightNames;
  while (p->mName)
  {
    nsCStringKey key(p->mName);
    gWeights->Put(&key, (void*) p->mValue);
    p++;
  }
  
  gStretches = new nsHashtable();
  if (!gStretches)
  {
    FreeGlobals();
    return NS_ERROR_OUT_OF_MEMORY;
  }
  p = gStretchNames;
  while (p->mName)
  {
    nsCStringKey key(p->mName);
    gStretches->Put(&key, (void*) p->mValue);
    p++;
  }
    
  gCharSets = new nsHashtable();
  if (!gCharSets)
  {
    FreeGlobals();
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsFontCharSetMap* charSetMap = gCharSetMap;
  while(charSetMap->mName)
  {
    nsCStringKey key(charSetMap->mName);
    gCharSets->Put(&key, charSetMap->mInfo);
    charSetMap++;
  }

  gSpecialCharSets = new nsHashtable();
  if (!gSpecialCharSets)
  {
    FreeGlobals();
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsFontCharSetMap* specialCharSetMap = gSpecialCharSetMap;
  while(specialCharSetMap->mName)
  {
    nsCStringKey key(specialCharSetMap->mName);
    gSpecialCharSets->Put(&key, specialCharSetMap->mInfo);
    specialCharSetMap++;
  }

	gUnicode = NS_NewAtom("x-unicode");
	if (!gUnicode)
	{
	  FreeGlobals();
	  return NS_ERROR_OUT_OF_MEMORY;
  }
		        
  gUserDefined = NS_NewAtom(USER_DEFINED);
  if (!gUserDefined)
  {
    FreeGlobals();
    return NS_ERROR_OUT_OF_MEMORY;
  }

  gInitialized = 1;
  
  return NS_OK;
}

// nsFontMetricsXlib Implementation
nsFontMetricsXlib::nsFontMetricsXlib()
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

  if (mSubstituteFont)
  {
    delete mSubstituteFont;
    mSubstituteFont = nsnull;
  }

  mWesternFont = nsnull;
  mFontHandle = nsnull;

  if (!--gFontMetricsXlibCount) {
    FreeGlobals();
  }
}

NS_IMPL_ISUPPORTS(nsFontMetricsXlib, kIFontMetricsIID)

static PRBool
IsASCIIFontName(const nsString& aName)
{
  PRUint32 len = aName.Length();
  const PRUnichar *str = aName.GetUnicode();
  for (PRUint32 i=0; i<len; i++)
  {
    /*
     * X font names are printable ASCII, ignore others (for now)
     */
    if ((str[i] < 0x20) || (str[i] > 0x7E))
      return PR_FALSE;
  }
  
  return PR_TRUE;
}

static PRBool
FontEnumCallback(const nsString& aFamily, PRBool aGeneric, void *aData)
{
  if (!IsASCIIFontName(aFamily))
    return PR_TRUE;
    
  nsCAutoString name;
  name.AssignWithConversion(aFamily.GetUnicode());
  name.ToLowerCase();
  
  nsFontMetricsXlib* metrics = (nsFontMetricsXlib*) aData;
  metrics->mFonts.AppendCString(name);
  metrics->mFontIsGeneric.AppendElement((void*) aGeneric);
  
  if (aGeneric)
  {
    metrics->mGeneric = metrics->mFonts.CStringAt(metrics->mFonts.Count() - 1);
    return PR_FALSE;
  }
  
  return PR_TRUE;
}

NS_IMETHODIMP
nsFontMetricsXlib::Init(const nsFont& aFont, nsIAtom* aLangGroup,
                        nsIDeviceContext* aContext)
{
  NS_ASSERTION(!(nsnull == aContext), "attempt to init fontmetrics with null device context");

  nsresult res;

  if (!gInitialized) 
  {
    res = InitGlobals();
    if (NS_FAILED(res))
      return res;
  }
    
  mFont = new nsFont(aFont);
  mLangGroup = aLangGroup;
  
  mDeviceContext = aContext;

  float app2dev;
  mDeviceContext->GetAppUnitsToDevUnits(app2dev);
  
  float textZoom = 1.0;
  mDeviceContext->GetTextZoom(textZoom);
 
  mPixelSize = NSToIntRound(app2dev * textZoom * mFont->size);
  mStretchIndex = 4; // Normal
  mStyleIndex = mFont->style;

  mFont->EnumerateFamilies(FontEnumCallback, this);

  char* value = nsnull;
  if (!mGeneric)
  {
    gPref->CopyCharPref("font.default", &value);
    if (value)
    {
      mDefaultFont = value;
      nsMemory::Free(value);
      value = nsnull;
    }
    else
      mDefaultFont = "serif";
    
    mGeneric = &mDefaultFont;
  }   
	       
  if (mLangGroup)
  {
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

    res = gPref->GetIntPref(name.GetBuffer(), &minimum);

    if (NS_FAILED(res))
      gPref->GetDefaultIntPref(name.GetBuffer(), &minimum);
      
    if (minimum < 0)
      minimum = 0;
      
    if (mPixelSize < minimum)
      mPixelSize = minimum;     
  }
    
  if (mLangGroup.get() == gUserDefined)
  {
    if (!gUserDefinedConverter)
    {
      nsCOMPtr<nsIAtom> charset;
      
      res = gCharSetManager->GetCharsetAtom2("x-user-defined", 
                                             getter_AddRefs(charset));
      if (NS_SUCCEEDED(res))
      {
        res = gCharSetManager->GetUnicodeEncoder(charset, 
                                                 &gUserDefinedConverter);
	      if (NS_SUCCEEDED(res))
	      {
	        res = gUserDefinedConverter->SetOutputErrorBehavior(
		                                   gUserDefinedConverter->kOnError_Replace, 
						                           nsnull, '?');

					nsCOMPtr<nsICharRepresentable> mapper = do_QueryInterface(gUserDefinedConverter);
				  if (mapper)
				    res = mapper->FillInfo(gUserDefinedMap);	
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

	  gPref->CopyCharPref(name.GetBuffer(), &value);
	  if (value)
	  {
	    mUserDefined = value;
	    nsMemory::Free(value);
	    value = nsnull;
	    mIsUserDefined = 1;
	  }
	}
  
  mWesternFont = FindFont('a');
  if (!mWesternFont)
    return NS_ERROR_FAILURE;

  mFontHandle = mWesternFont->mFont;

  RealizeFont();

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

  int lineSpacing = mFontHandle->ascent + mFontHandle->descent;
  if (lineSpacing > mWesternFont->mSize)
    mLeading = nscoord((lineSpacing - mWesternFont->mSize) * f);
  else
    mLeading = 0;

  mEmHeight = PR_MAX(1, nscoord(mWesternFont->mSize * f));
  mEmAscent = nscoord(mFontHandle->ascent * mWesternFont->mSize * f / lineSpacing);
  mEmDescent = mEmHeight - mEmAscent;

  mMaxHeight = nscoord((mFontHandle->max_bounds.ascent +
                        mFontHandle->max_bounds.descent) * f);
  mMaxAscent = nscoord(mFontHandle->max_bounds.ascent * f) ;
  mMaxDescent = nscoord(mFontHandle->max_bounds.descent * f);

  mMaxAdvance = nscoord(mFontHandle->max_bounds.width * f);

  // 56% of ascent, best guess for non-true type
  mXHeight = NSToCoordRound((float) mFontHandle->ascent* f * 0.56f);

  int rawWidth = XTextWidth(mFontHandle, " ", 1);
  mSpaceWidth = NSToCoordRound(rawWidth * f);

  unsigned long pr = 0;

  if (::XGetFontProperty(mFontHandle, XA_X_HEIGHT, &pr))
  {
    if (pr < 0x00ffffff)  // Bug 43214: arbitrary to exclude garbage values
    {
      mXHeight = nscoord(pr * f);
      PR_LOG(FontMetricsXlibLM, PR_LOG_DEBUG, ("xHeight=%d\n", mXHeight));
    }
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
  aHandle = (nsFontHandle)mFontHandle;
  return NS_OK;
}

// ===================== new code -- erik ====================

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
SingleByteConvert(nsFontCharSetInfo* aSelf, XFontStruct* aFont,
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
SingleByteConvertReverse(nsFontCharSetInfo* aSelf, const PRUnichar* aSrcBuf,
  PRInt32 aSrcLen, char* aDestBuf, PRInt32 aDestLen)
{
    int count = SingleByteConvert(aSelf, aSrcBuf,
                       aSrcLen, aDestBuf,  aDestLen);
    ReverseBuffer(aDestBuf, count);
    return count;
}
*/

static int
DoubleByteConvert(nsFontCharSetInfo* aSelf, XFontStruct* aFont, 
                  const PRUnichar* aSrcBuf, PRInt32 aSrcLen, char* aDestBuf, 
		              PRInt32 aDestLen)
{
  int count = 0;
  if (aSelf->mConverter) {
    aSelf->mConverter->Convert(aSrcBuf, &aSrcLen, aDestBuf, &aDestLen);
    count = aDestLen;

    if (count>0)
    {
      if ((aDestBuf[0] & 0x80) && (!(aFont->min_byte1 & 0x80)))
      {
        for (PRInt32 i=0; i < aDestLen; i++)
	        aDestBuf[i] &= 0x7F;
		  }
		  else
		  {
		    if ((!(aDestBuf[0] & 0x80)) && (aFont->min_byte1 & 0x80))
		    {
		      for (PRInt32 i=0; i<aDestLen; i++)
		        aDestBuf[i] |= 0x80;
	      }
	    }
	  }
	}
	else
	  count = 0;
	    
  return count;
}

static int
ISO10646Convert(nsFontCharSetInfo* aSelf, XFontStruct* aFont,
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

static void
SetUpFontCharSetInfo(nsFontCharSetInfo* aSelf)
{
  nsCOMPtr<nsIAtom> charset;
  nsresult result = gCharSetManager->GetCharsetAtom2(aSelf->mCharSet,
                                                     getter_AddRefs(charset));
  if (NS_SUCCEEDED(result)) {
    nsIUnicodeEncoder* converter = nsnull;
    result = gCharSetManager->GetUnicodeEncoder(charset, &converter);

    if (NS_SUCCEEDED(result)) {
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
          /* FIXME
           * Original value for comparison was 0x3000, changed it to be inline
           * with GTK version.
           */           
          for (PRUint16 i = 0; i < (0x2200 >> 5); i++) {
            map[i] = 0;
          }
        }
      }
      else 
       fprintf(stderr, "=== nsICharRepresentable %s failed\n", aSelf->mCharSet);
    }
    else
       fprintf(stderr, "=== GetUnicodeEncoder %s failed\n", aSelf->mCharSet);
  }
  else
    fprintf(stderr, "=== GetCharsetAtom2 %s failed\n", aSelf->mCharSet);
}

#undef DEBUG_DUMP_TREE
#ifdef DEBUG_DUMP_TREE

static char* gDumpStyles[3] = { "normal", "italic", "oblique" };

static PRIntn
DumpCharSet(PLHashEntry* he, PRIntn i, void* arg)
{
  PR_LOG(FontMetricsXlibLM, PR_LOG_DEBUG, ("        %s\n", (char*) he->key));
  nsFontCharSet* charSet = (nsFontCharSet*) he->value;
  for (int sizeIndex = 0; sizeIndex < charSet->mSizesCount; sizeIndex++) {
    nsFontXlib* size = &charSet->mSizes[sizeIndex];
    PR_LOG(FontMetricsXlibLM, PR_LOG_DEBUG, ("          %d %s\n", size->mSize, size->mName));
  }
  return HT_ENUMERATE_NEXT;
}

static void
DumpFamily(nsFontFamily* aFamily)
{
  for (int styleIndex = 0; styleIndex < 3; styleIndex++) {
    nsFontStyle* style = aFamily->mStyles[styleIndex];
    if (style) {
      PR_LOG(FontMetricsXlibLM, PR_LOG_DEBUG, ("  style: %s\n", gDumpStyles[styleIndex]));
      for (int weightIndex = 0; weightIndex < 8; weightIndex++) {
        nsFontWeight* weight = style->mWeights[weightIndex];
        if (weight) {
          PR_LOG(FontMetricsXlibLM, PR_LOG_DEBUG, ("    weight: %d\n", (weightIndex + 1) * 100));
          for (int stretchIndex = 0; stretchIndex < 9; stretchIndex++) {
            nsFontStretch* stretch = weight->mStretches[stretchIndex];
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
  nsFontFamily* family = (nsFontFamily*) he->value;
  DumpFamily(family);

  return HT_ENUMERATE_NEXT;
}

static void
DumpTree(void)
{
  PL_HashTableEnumerateEntries(gFamilies_old, DumpFamilyEnum, nsnull);
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

// We need to whack this in the middle of nsFontMetricsXlib implementaion because
// PickASizeAndLoad() needs a nsFontXlibNormal object.

// nsFontXlib Class Implementation

MOZ_DECL_CTOR_COUNTER(nsFontXlib);

nsFontXlib::nsFontXlib()
{
  MOZ_COUNT_CTOR(nsFontXlib);
}

nsFontXlib::~nsFontXlib()
{
  MOZ_COUNT_DTOR(nsFontXlib);
  if(mFont)  
    XFreeFont(xlib_rgb_get_display(), mFont);
    
  if (mCharSetInfo == &ISO106461)
    PR_FREEIF(mMap);

  if (mName)
    PR_smprintf_free(mName);    
}

void
nsFontXlib::LoadFont(void)
{
  Display *aDisplay = xlib_rgb_get_display();
  XFontStruct *xlibFont = XLoadQueryFont(aDisplay, mName);
  
  if (xlibFont) 
  {
    if (mCharSetInfo == &ISO106461)
    {
      mMap = GetMapFor10646Font(xlibFont);
      if (!mMap) {
        XFreeFont(aDisplay, xlibFont);
        return;
      }
    }
  }
  
 mFont = xlibFont;

#if 0
    if (aCharSet->mInfo->mSpecialUnderline && aMetrics->mFontHandle) {
      XFontStruct* asciiXFont =
        (XFontStruct*) GDK_FONT_XFONT(aMetrics->mFontHandle);
      unsigned long positionX2;
      unsigned long thickness;
      GetUnderlineInfo(asciiXFont, &positionX2, &thickness);
      mActualSize += (positionX2 + thickness);
      mBaselineAdjust = (-xlibFont->max_bounds.descent);
    }
#endif /* 0 */
 
}

class nsFontXlibNormal : public nsFontXlib
{
public:
  nsFontXlibNormal();
  virtual ~nsFontXlibNormal();
  
  virtual int GetWidth(const PRUnichar* aString, PRUint32 aLength);
  virtual int DrawString(nsRenderingContextXlib* aContext,
                         nsDrawingSurfaceXlib* aSurface,
			                   nscoord aX, nscoord aY, 
					               const PRUnichar* aString, PRUint32 aLength);
					       						       
#ifdef MOZ_MATHML
  virtual nsresult GetBoudingMetrics(const PRUnichar* aString,
                                     PRUint32         aLength,
				                             nsBoundingMetrics& aBoundingMetrics);
#endif							     						       					   
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
  if (!mFont)
  {
    LoadFont();
    if (!mFont)
      return 0;
  }
  
  XChar2b buf[512];
  int len = mCharSetInfo->Convert(mCharSetInfo, (XFontStruct*) mFont, aString, 
                                  aLength, (char *) buf, sizeof(buf));

  if ((mFont->min_byte1 == 0) && (mFont->max_byte1 == 0))
    return XTextWidth(mFont, (char*) buf, len);
  else
    return XTextWidth16(mFont, buf, len);
}

int
nsFontXlibNormal::DrawString(nsRenderingContextXlib* aContext,
                             nsDrawingSurfaceXlib* aSurface,
			                       nscoord aX, nscoord aY, 
    					               const PRUnichar* aString, PRUint32 aLength)
{
  if (!mFont)
  {
    LoadFont();
    if (!mFont)
      return 0;
  }      
   
  XChar2b buf[512];
  int len = mCharSetInfo->Convert(mCharSetInfo, mFont, aString, aLength, 
                                  (char*) buf, sizeof(buf));   

  if ((mFont->min_byte1 == 0) && (mFont->max_byte1 == 0))    
  {
    XDrawString(aSurface->GetDisplay(),
                aSurface->GetDrawable(),
                aSurface->GetGC(),
                aX, aY + mBaselineAdjust, (char *)buf, len);
    
    return XTextWidth(mFont, (char*) buf, len);  
  }
  else
  {
    XDrawString16(aSurface->GetDisplay(),
                  aSurface->GetDrawable(),
                  aSurface->GetGC(),
                  aX, aY + mBaselineAdjust, buf, len);	      
    
    return XTextWidth16(mFont, buf, len);
  }
}						       

#ifdef MOZ_MATHML
nsresult
nsFontXlibNormal::GetBoundingMetrics(const PRUnichar* aString,
                                     PRUint32         aLength,
				                             nsBoundingMetrics& aBoundingMetrics)
{
  aBoundingMetrics.Clear();

  if (!mFont)
  {
    LoadFont();
    if (!mFont)
      return NS_ERROR_FAILURE;
  }
    
  if (aString && 0 < aLength) {
    XChar2b buf[512]; // XXX watch buffer length !!!
    int len = mCharSetInfo->Convert(mCharSetInfo, mFont, aString,
                                    aLength, (char*) buf, sizeof(buf));
    
    XCharStruct overall;
    int direction, font_ascent, font_descent;

    if ((mFont->min_byte1 == 0) && (mFont->max_byte1 == 0))
      XTextExtents(mFont, (char *)buf, len,
                   &direction, &font_ascent, &font_descent,
                   &overall);
    else
      XTextExtents16(mFont, buf, len/2,
                    &direction, &font_ascent, &font_descent,
                    &overall);

    aBoundingMetrics.leftBearing  =  overall.lbearing;
    aBoundingMetrics.rightBearing =  overall.rbearing;
    aBoundingMetrics.width        =  overall.width;
    aBoundingMetrics.ascent       =  overall.ascent;
    aBoundingMetrics.descent      =  overall.descent;

    unsigned long pr = 0;
    if (::XGetFontProperty(mFont, XA_ITALIC_ANGLE, &pr)) {
      aBoundingMetrics.subItalicCorrection = (int) pr; 
      aBoundingMetrics.supItalicCorrection = (int) pr;
    }
  }

  return NS_OK;
}
#endif 
  
class nsFontXlibSubstitute : public nsFontXlib
{
public:
  nsFontXlibSubstitute(nsFontXlib* aFont);
  virtual ~nsFontXlibSubstitute();
  
  virtual int GetWidth(const PRUnichar* aString, PRUint32 aLength);
  virtual int DrawString(nsRenderingContextXlib* aContext, 
                         nsDrawingSurfaceXlib* aSurface,
                         nscoord aX, nscoord aY, 
			                   const PRUnichar* aString, PRUint32 aLength);
#ifdef MOZ_MATHML
  virtual nsresult GetBoundingMetrics(const PRUnichar* aString,
                                      PRUint32 aLength,
				                              nsBoundingMetrics& aBoundingMetrics);
#endif							      
  virtual PRUint32 Convert(const PRUnichar* aSrc, PRUint32 aSrcLen,
                           PRUnichar* aDest, PRUint32 aDestLen);			    					  

  nsFontXlib* mSubstituteFont;
  
  static int gCount;
  static nsISaveAsCharset* gConverter;
};

int nsFontXlibSubstitute::gCount = 0;
nsISaveAsCharset* nsFontXlibSubstitute::gConverter = nsnull;

nsFontXlibSubstitute::nsFontXlibSubstitute(nsFontXlib* aFont)
{
  gCount++;
  mSubstituteFont = aFont;
}

nsFontXlibSubstitute::~nsFontXlibSubstitute()
{
  if (!--gCount)
    NS_IF_RELEASE(gConverter);
    
  // Do not free mSubstituteFont here. It is owned by somebody else.
}

PRUint32
nsFontXlibSubstitute::Convert(const PRUnichar* aSrc, PRUint32 aSrcLen, 
                              PRUnichar* aDest, PRUint32 aDestLen)
{			      
  nsresult res;
  
  if (!gConverter)
  {
    nsComponentManager::CreateInstance(kSaveAsCharsetCID, nsnull, 
                                        NS_GET_IID(nsISaveAsCharset), 
					                              (void**) &gConverter);
    if (gConverter)								      
    {
      res = gConverter->Init("ISO-88591-1",
                             nsISaveAsCharset::attr_FallbackQuestionMark +
			                       nsISaveAsCharset::attr_EntityBeforeCharsetConv,
					                   nsIEntityConverter::transliterate);
      if (NS_FAILED(res))
        NS_RELEASE(gConverter);
	  }
	}
	
	if (gConverter)
	{
	  nsAutoString tmp(aSrc, aSrcLen);
	  char* conv = nsnull;
	  
	  res = gConverter->Convert(tmp.GetUnicode(), &conv);
	  if (NS_SUCCEEDED(res) && conv)
	  {
	    char* p = conv;
	    PRUint32 i;
	    for (i=0; i < aDestLen; i++)
	    {
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
    
  for (PRUint32 i=0; i < aSrcLen; i++)
    aDest[i] = '?';
    
  return aSrcLen;
}
			      		
int
nsFontXlibSubstitute::GetWidth(const PRUnichar* aString, PRUint32 aLength)
{
  PRUnichar buf[512];
  PRUint32 len = Convert(aString, aLength, buf, sizeof(buf)/2);
 
  return mSubstituteFont->GetWidth(buf, len);
}

int
nsFontXlibSubstitute::DrawString(nsRenderingContextXlib* aContext, 
                                 nsDrawingSurfaceXlib* aSurface,
                                 nscoord aX, nscoord aY, 
				                         const PRUnichar* aString, PRUint32 aLength)
{
  PRUnichar buf[512];
  PRUint32 len = Convert(aString, aLength, buf, sizeof(buf)/2);

  return mSubstituteFont->DrawString(aContext, aSurface, aX, aY, buf, len);
}

#ifdef MOZ_MATHML
// bounding metrics for a string 
// remember returned values are not in app units
nsresult
nsFontXlibSubstitute::GetBoundingMetrics(const PRUnichar*   aString,
                                        PRUint32           aLength,
                                        nsBoundingMetrics& aBoundingMetrics)                                 
{
  PRUnichar buf[512]; // XXX watch buffer length !!!
  PRUint32 len = Convert(aString, aLength, buf, sizeof(buf)/2);
  return mSubstituteFont->GetBoundingMetrics(buf, len, aBoundingMetrics);
}
#endif																      

class nsFontXlibUserDefined : public nsFontXlib
{
public:
  nsFontXlibUserDefined();
  virtual ~nsFontXlibUserDefined();
  
  virtual PRBool Init(nsFontXlib* aFont);
  virtual int GetWidth(const PRUnichar* aString, PRUint32 aLength);
  virtual int DrawString(nsRenderingContextXlib* aContext, 
                         nsDrawingSurfaceXlib* aSurface,
                         nscoord aX, nscoord aY, const PRUnichar* aString, 
 			                   PRUint32 aLength);
#ifdef MOZ_MATHML
  virtual nsresult GetBoundingMetrics(const PRUnichar* aString,
                                      PRUint32 aLength,
				                              nsBoundingMetrics& aBoundingMetrics);
#endif							      
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
  if(!aFont->mFont)
  {
    aFont->LoadFont();
    if (!aFont->mFont)
      return PR_FALSE;
  }
  
  mFont = aFont->mFont;
  mMap = gUserDefinedMap;
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
  PRUint32 len = Convert(aString, aLength, buf, sizeof(buf));
  
  return XTextWidth(mFont, buf, len);
}

int
nsFontXlibUserDefined::DrawString(nsRenderingContextXlib* aContext,
                                  nsDrawingSurfaceXlib* aSurface,
 			                            nscoord aX, nscoord aY,
							                    const PRUnichar* aString, PRUint32 aLength)
{
  char buf[1024];
  PRUint32 len = Convert(aString, aLength, buf, sizeof(buf));

  XDrawString(aSurface->GetDisplay(),
              aSurface->GetDrawable(),
              aSurface->GetGC(),
              aX, aY + mBaselineAdjust, (char *)buf, len);
  
  return XTextWidth(mFont, buf, len);  
}									    

#ifdef MOZ_MATHML
nsresult
nsFontXlibUserDefined::GetBoundingMetrics(const PRUnichar* aString,
                                          PRUint32         aLength,
					                                nsBoundingMetrics& aBoundingMetrics)
{
  aBoundingMetrics.Clear();

  if (aString && 0 < aLength) {
    char buf[1024]; // XXX watch buffer length !!!
    int len = Convert(aString, aLength, buf, sizeof(buf));
    
    XCharStruct overall;
    int direction, font_ascent, font_descent;

    if ((mFont->min_byte1 == 0) && (mFont->max_byte1 == 0))
      XTextExtents(mFont, buf, len,
                   &direction, &font_ascent, &font_descent,
                   &overall);

    aBoundingMetrics.leftBearing  =  overall.lbearing;
    aBoundingMetrics.rightBearing =  overall.rbearing;
    aBoundingMetrics.width        =  overall.width;
    aBoundingMetrics.ascent       =  overall.ascent;
    aBoundingMetrics.descent      =  overall.descent;

    unsigned long pr = 0;
    if (::XGetFontProperty(mFont, XA_ITALIC_ANGLE, &pr)) {
      aBoundingMetrics.subItalicCorrection = (int) pr; 
      aBoundingMetrics.supItalicCorrection = (int) pr;
    }
  }

  return NS_OK
}						
#endif 

// Continue nsFontMetricsXlib Implementation

nsFontXlib*
nsFontMetricsXlib::PickASizeAndLoad(nsFontStretch* aStretch,
                                    nsFontCharSetInfo* aCharSet,
                                    PRUnichar aChar)
{
  nsFontXlib* font = nsnull;
  int scalable = 0;

  if (aStretch->mSizes) {
    nsFontXlib** begin = aStretch->mSizes;
    nsFontXlib** end = &aStretch->mSizes[aStretch->mSizesCount];
    nsFontXlib** s;
    for (s = begin; s < end; s++) {
      if ((*s)->mSize >= mPixelSize) {
        break;
      }
    }
    if (s == end) {
      s--;
    }
    else if (s != begin) {
      if (((*s)->mSize - mPixelSize) >= (mPixelSize - (*(s - 1))->mSize)) {
        s--;
      }
    }
    font = *s;
    
    if (aStretch->mScalable)
    {
      double ratio = ((*s)->mSize / ((double) mPixelSize));

      /*
       * XXX Maybe revisit this. Upper limit deliberately set high (1.8) in
       * order to avoid scaling Japanese fonts (ugly).
       */

      if ((ratio > 1.8) || (ratio < 0.8))
        scalable = 1;
    }
   }
   else
     scalable = 1;
    
    if (scalable)
    {
      PRInt32 i;
      PRInt32 n = aStretch->mScaledFonts.Count();
      nsFontXlib* p;

      for (i=0; i < n; i++)
      {
        p = (nsFontXlib*) aStretch->mScaledFonts.ElementAt(i);
        if (p->mSize == mPixelSize)
          break;
      }

      if (i == n) 
      {
        font = new nsFontXlibNormal;
        if (font)
        {
          /*
           * XXX Instead of passing mPixelSize, we ought to take underline
           * into account. (Extra space for underline for Asian fonts.)
           */
          font->mName = PR_smprintf(aStretch->mScalable, mPixelSize);
          if (!font->mName)
          {
            delete font;
            return nsnull;
          }
          font->mSize = mPixelSize;
          font->mCharSetInfo = aCharSet;
          aStretch->mScaledFonts.AppendElement(font);
        }
        else
          return nsnull;
      }
      else
        font = p;
    }

    if (aCharSet->mCharSet)
    {
      font->mMap = aCharSet->mMap;
      if(FONT_HAS_GLYPH(font->mMap, aChar))
      {
        font->LoadFont();
        if(!font->mFont)
          return nsnull;
      }
    }
    else
    {
      if (aCharSet == &ISO106461)
      {
        font->LoadFont();
        if(!font->mFont)
          return nsnull;
      }
    }

  if (mIsUserDefined)
  {
    if(!font->mUserDefinedFont)
    {
      font->mUserDefinedFont = new nsFontXlibUserDefined();
      if (!font->mUserDefinedFont)
        return nsnull;
      if (!font->mUserDefinedFont->Init(font))
        return nsnull;
    }
    font = font->mUserDefinedFont;
  }
  
  if (mLoadedFontsCount == mLoadedFontsAlloc) {
    int newSize;
    if (mLoadedFontsAlloc) {
      newSize = (2 * mLoadedFontsAlloc);
    }
    else {
      newSize = 1;
    }
    nsFontXlib** newPointer = (nsFontXlib**) PR_Realloc(mLoadedFonts,
      newSize * sizeof(nsFontXlib*));
    if (newPointer) {
      mLoadedFonts = newPointer;
      mLoadedFontsAlloc = newSize;
    }
    else {
      return nsnull;
    }
  }
  mLoadedFonts[mLoadedFontsCount++] = font;

#if 0
  nsFontXlib* result = s;
  for (s = begin; s < end; s++) {
    PR_LOG(FontMetricsXlibLM, PR_LOG_DEBUG, ("%d/%d ", s->mSize, s->mActualSize));
  }
  PR_LOG(FontMetricsXlibLM, PR_LOG_DEBUG, ("desired %d chose %d\n", desiredSize, result->mActualSize));
#endif /* 0 */

  return font;
}

static int
CompareSizes(const void* aArg1, const void* aArg2, void *data)
{
  return (*((nsFontXlib**) aArg1))->mSize - (*((nsFontXlib**) aArg2))->mSize;
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
nsFontNode::FillStyleHoles(void)
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

nsFontXlib*
nsFontMetricsXlib::SearchNode(nsFontNode* aNode, PRUnichar aChar)
{
  if (aNode->mDummy)
    return nsnull;

  nsFontCharSetInfo* charSetInfo = aNode->mCharSetInfo;

  /*
   * mCharSet is set if we know which glyphs will be found in these fonts.
   * If mMap has already been created for this charset, we compare it with
   * the mMaps of the previously loaded fonts. If it is the same as any of
   * the previous ones, we return nsnull because there is no point in
   * loading a font with the same map.
   */  

  if (charSetInfo->mCharSet)
  {
    PRUint32* map = charSetInfo->mMap;
    if (map)
    {
      for (int i=0; i < mLoadedFontsCount; i++)
      {
        if (mLoadedFonts[i]->mMap == map)
          return nsnull;
      }
    }
    else
    {
      map = (PRUint32*) PR_Calloc(2048, 4);
      if (!map)
        return nsnull;

      charSetInfo->mMap = map;
      SetUpFontCharSetInfo(charSetInfo);
    }
  }
  else
  {
    if ((!mIsUserDefined) && (charSetInfo == &Unknown))
      return nsnull;
  }

  aNode->FillStyleHoles();
  nsFontStyle* style = aNode->mStyles[mStyleIndex];

  nsFontWeight** weights = style->mWeights;
  int weight = mFont->weight;
  int steps = (weight % 100);
  int weightIndex;
  if (steps)
  {
    if (steps < 10)
    {
      int base = (weight - steps);
      GET_WEIGHT_INDEX(weightIndex, base);
      while (steps--)
      {
        nsFontWeight* prev = weights[weightIndex];
        for (weightIndex++; weightIndex < 9; weightIndex++)
        {
          if (weights[weightIndex] != prev)
            break;
        }
        if (weightIndex >= 9)
          weightIndex = 8;
      }
    }
    else if (steps > 90)
    {
      steps = (100 - steps);
      int base = (weight + steps);
      GET_WEIGHT_INDEX(weightIndex, base);
      while (steps--)
      {
        nsFontWeight* prev = weights[weightIndex];
        for (weightIndex--; weightIndex >=0; weightIndex--)
        {
          if (weights[weightIndex] != prev)
            break;
        }
        if (weightIndex < 0)
          weightIndex = 0;
      }
    }
    else
      GET_WEIGHT_INDEX(weightIndex, weight);
  }
  else
    GET_WEIGHT_INDEX(weightIndex, weight);

  return PickASizeAndLoad(weights[weightIndex]->mStretches[mStretchIndex],
                          charSetInfo, aChar);
}

static void
GetFontNames(char* aPattern, nsFontNodeArray* aNodes)
{
  nsCAutoString previousNodeName;

  int count;
  //PR_LOG(FontMetricsXlibLM, PR_LOG_DEBUG, ("XListFonts %s\n", aPattern));
  char** list = ::XListFonts(xlib_rgb_get_display(), aPattern, INT_MAX, &count);
  if ((!list) || (count < 1)) {
    return;
  }

  for (int i = 0; i < count; i++) {   
    char* name = nsnull;
    name = list[i];
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
    nsFontCharSetInfo* charSetInfo =
      (nsFontCharSetInfo*) gCharSets->Get(&charSetKey);

    // indirection for font specific charset encoding 
    if (charSetInfo == &Special) 
    {
      nsCAutoString familyCharSetName (familyName);
      familyCharSetName.Append('-');
      familyCharSetName.Append(charSetName);
      nsCStringKey familyCharSetKey(familyCharSetName);
      charSetInfo = (nsFontCharSetInfo*) gSpecialCharSets->Get(&familyCharSetKey);
    }

    if (!charSetInfo) {
      PR_LOG(FontMetricsXlibLM, PR_LOG_DEBUG, ("cannot find charset %s\n", charSetName));
      charSetInfo = &Unknown;
    }

    if (charSetInfo->mCharSet && (!charSetInfo->mLangGroup))
    {
      nsCOMPtr<nsIAtom> charset;
      nsresult res = gCharSetManager->GetCharsetAtom2(charSetInfo->mCharSet,
                                                      getter_AddRefs(charset));
      if (NS_SUCCEEDED(res))
      {
        res = gCharSetManager->GetCharsetLangGroup(charset, 
                                                   &charSetInfo->mLangGroup);
        if (NS_FAILED(res))
        {
          #ifdef NOISY_FONTS
          fprintf(stderr, "=== cannot get lang group for %s\n", charSetInfo->mCharSet);
          #endif
        }
      }
    }
          
    nsCAutoString nodeName(foundry);
    
    nodeName.Append('-');
    nodeName.Append(familyName);
    nodeName.Append('-');
    nodeName.Append(charSetName);

    nsCStringKey key(nodeName);
    nsFontNode* node = (nsFontNode*) gNodes->Get(&key);

    if (!node)
    {
      node = new nsFontNode;
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
      PRInt32 n = aNodes->Count();
      for (PRInt32 j = 0; j < n; j++)
      {
        if (aNodes->GetElement(j) == node)
          found = 1;
      }
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
    nsFontStyle* style = node->mStyles[styleIndex];
    if (!style) {
      style = new nsFontStyle;
      if (!style) {
        continue;
      }
      node->mStyles[styleIndex] = style;
    }

    nsCStringKey weightKey(weightName);
    int weightNumber = (int) gWeights->Get(&weightKey);
    
    if (!weightNumber) {
      PR_LOG(FontMetricsXlibLM, PR_LOG_DEBUG, ("cannot find weight %s\n", weightName));
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

    nsCStringKey setWidthKey(setWidth);
    int stretchIndex = (int) gStretches->Get(&setWidthKey);

   if (!stretchIndex) {
      PR_LOG(FontMetricsXlibLM, PR_LOG_DEBUG, ("cannot find stretch %s\n", setWidth));
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
        stretch->mScalable = PR_smprintf("%s-%s-%s-%s-%s-%s-%%d-*-*-*-%s-*-%s",
          name, familyName, weightName, slant, setWidth, addStyle, spacing, 
          charSetName);
      }
      continue;
    }
  
    int pixels = atoi(pixelSize);
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
     
    stretch->mSizes[stretch->mSizesCount++] = size ;

    size->mName = copy;
    size->mFont = nsnull;
    size->mSize = pixels;
    size->mBaselineAdjust = 0;
    size->mMap = nsnull;
    size->mCharSetInfo = charSetInfo;

  }
  XFreeFontNames(list);

#ifdef DEBUG_DUMP_TREE
  //DumpTree();
#endif

}

static nsFontFamily*
FindFamily(nsCString* aName)
{
  nsCStringKey key(*aName);
  nsFontFamily* family = (nsFontFamily*) gFamilies->Get(&key);
  if (!family) 
  {
    family = new nsFontFamily();
    if (family)
    {
      char pattern[256];
      PR_snprintf(pattern, sizeof(pattern), "-*-%s-*-*-*-*-*-*-*-*-*-*-*-*",
                  aName->GetBuffer());
      GetFontNames(pattern, &family->mNodes);
      gFamilies->Put(&key, family);
    }

  }

  return family;
}

static nsresult
GetAllFontNames(void)
{
  if(!gGlobalList)
  {
    gGlobalList = new nsFontNodeArray();
    if (!gGlobalList) 
    {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    GetFontNames("-*-*-*-*-*-*-*-*-*-*-*-*-*-*", gGlobalList);
  }

  return NS_OK;
}

nsresult
nsFontMetricsXlib::FamilyExists(const nsString& aName)
{
  if (!gInitialized)
  {
    nsresult res = InitGlobals();
    if (NS_FAILED(res))
      return res;
  }

  if (!IsASCIIFontName(aName))
    return NS_ERROR_FAILURE;

  nsCAutoString name;
  name.AssignWithConversion(aName.GetUnicode());
  name.ToLowerCase();
  
  nsFontFamily* family = FindFamily(&name);
  if (family && family->mNodes.Count())
    return NS_OK;

  return NS_ERROR_FAILURE;
}

static void
PrefEnumCallback(const char* aName, void* aClosure)
{
  nsFontSearch* search = (nsFontSearch*) aClosure;
  
  if (search->mFont && search->mFont->SupportsChar(search->mChar))
    return;

  char* value = nsnull;
  gPref->CopyCharPref(aName, &value);
  nsCAutoString name;

  if (value) 
  {
    name = value;
    nsMemory::Free(value);
    value = nsnull;
    search->mFont = search->mMetrics->TryNode(&name, search->mChar);
  }

  if (search->mFont && search->mFont->SupportsChar(search->mChar))
    return;
    
  gPref->CopyDefaultCharPref(aName, &value);

  if (value) 
  {
    name = value;
    nsMemory::Free(value);
    value = nsnull;
    search->mFont = search->mMetrics->TryNode(&name, search->mChar);
  }
  
}

nsFontXlib*
nsFontMetricsXlib::TryNode(nsCString* aName, PRUnichar aChar)
{
  nsCStringKey key(*aName);
  nsFontNode* node = (nsFontNode*) gNodes->Get(&key);
  if (!node)
  {
    nsCAutoString pattern("-");

    pattern.Append(*aName);

    PRInt32 hyphen = pattern.FindChar('-');
    hyphen = pattern.FindChar('-', PR_FALSE, hyphen + 1);
    hyphen = pattern.FindChar('-', PR_FALSE, hyphen + 1);

    pattern.Insert("-*-*-*-*-*-*-*-*-*-*", hyphen);

    nsFontNodeArray nodes;
    GetFontNames(pattern, &nodes);
    if (nodes.Count() > 0)
      node = nodes.GetElement(0);
    else 
    {
      // add a dummy node to the hash table to avoid calling XListFonts again
      node = new nsFontNode();
      if (!node)
        return nsnull;
      gNodes->Put(&key, node);
      node->mDummy = 1;
    }
  }

  if (node)
    return SearchNode(node, aChar);

  return nsnull;
}  

nsFontXlib*
nsFontMetricsXlib::TryAliases(nsCString* aAlias, PRUnichar aChar)
{
  nsCStringKey key(*aAlias);
  char* name = (char*) gAliases->Get(&key);
  
  if (name)
  {
    nsCAutoString str(name);
    return TryFamily(&str, aChar);
  }
  
  return nsnull;  
}

nsFontXlib*
nsFontMetricsXlib::TryFamily(nsCString* aName, PRUnichar aChar)
{
  nsFontFamily* family = FindFamily(aName);
  if (family)
  {
    nsFontNodeArray* nodes = &family->mNodes;
    PRInt32 n = nodes->Count();
    for (PRInt32 i=0; i < n; i++)
    {
      nsFontXlib* font = SearchNode(nodes->GetElement(i), aChar);
      if (font && font->SupportsChar(aChar))
        return font;
	  }
	}
	
	return nsnull;
}

nsFontXlib*
nsFontMetricsXlib::FindGenericFont(PRUnichar aChar)
{
  if (mTriedAllGenerics) {
    return nsnull;
  }

  nsCAutoString prefix("font.name.");
  prefix.Append(*mGeneric);

  if (mLangGroup) 
  {
    nsCAutoString pref = prefix;
    pref.Append('.');
  
    const PRUnichar* langGroup = nsnull;
    mLangGroup->GetUnicode(&langGroup);
    
    pref.AppendWithConversion(langGroup);
    char* value = nsnull;
         
    gPref->CopyCharPref(pref.GetBuffer(), &value);

    nsCAutoString str;
    nsFontXlib* font;
    if (value) {
      str = value;
      nsMemory::Free(value);
      value = nsnull;
      font = TryNode(&str, aChar);
      if (font && font->SupportsChar(aChar))
        return font;
    }
    value = nsnull;
    
    gPref->CopyDefaultCharPref(pref.GetBuffer(), &value);
    if (value) 
    {
      str = value;
      nsMemory::Free(value);
      value = nsnull;
      font = TryNode(&str, aChar);
      if (font && font->SupportsChar(aChar))
        return font;      
    }
  }
  
  nsFontSearch search = { this, aChar, nsnull };
  gPref->EnumerateChildren(prefix.GetBuffer(), PrefEnumCallback, &search);
  if (search.mFont && search.mFont->SupportsChar(aChar))
    return search.mFont;

  mTriedAllGenerics = 1;
  
  return nsnull;
}

nsFontXlib*
nsFontMetricsXlib::FindGlobalFont(PRUnichar aChar)
{  
  // XXX If we get to this point, that means that we have exhausted all the
  // families in the lists. Maybe we should try a list of fonts that are
  // specific to the vendor of the X server here. Because XListFonts for the
  // whole list is very expensive on some Unixes.

  /*
   * Try all the fonts on the system.
   */

  nsresult res = GetAllFontNames();
  if (NS_FAILED(res))
    return nsnull;
    
  PRInt32 n = gGlobalList->Count();
  for (PRInt32 i=0; i < n; i++)
  {
    nsFontXlib* font = SearchNode(gGlobalList->GetElement(i), aChar);
    if (font && font->SupportsChar(aChar))
    {
      // XXX We should probably write this family name out to disk, so that
      // we can use it next time. I.e. prefs file or something.
      return font;
    }
  }
  
  return nsnull;
}

nsFontXlib*
nsFontMetricsXlib::FindLocalFont(PRUnichar aChar)
{
  while (mFontsIndex < mFonts.Count())
  {
    if (mFontIsGeneric[mFontsIndex])
      return nsnull;
    
    nsCString* familyName = mFonts.CStringAt(mFontsIndex++);
    
    /*
     * Count Hyphens
     */
    const char* str = familyName->GetBuffer();
    PRUint32 len = familyName->Length();
    int hyphens = 0;
    for (PRUint32 i=0; i < len; i++)
    {
      if (str[i] == '-')
        hyphens++;
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
     
    nsFontXlib* font;
    if (hyphens == 3)
    {
      font = TryNode(familyName, aChar);
      if (font && font->SupportsChar(aChar))
        return font;
    }
    else
    {
      font = TryFamily(familyName, aChar);
      if (font && font->SupportsChar(aChar))
        return font;

	    font = TryAliases(familyName, aChar);
	    if (font && font->SupportsChar(aChar))
	      return font;
	  }
	}
	
	return nsnull;	  
}

nsFontXlib*
nsFontMetricsXlib::FindSubstituteFont(PRUnichar aChar)
{
  if (!mSubstituteFont)
  {
    for (int i=0; i < mLoadedFontsCount; i++)
    {
      if (FONT_HAS_GLYPH(mLoadedFonts[i]->mMap, 'a'))
      {
        mSubstituteFont = new nsFontXlibSubstitute(mLoadedFonts[i]);
	      break;
      }
    }
  }
	 
 return mSubstituteFont;
}

nsFontXlib*
nsFontMetricsXlib::FindUserDefinedFont(PRUnichar aChar)
{
  if (mIsUserDefined)
  {
    nsFontXlib* font = TryNode(&mUserDefined, aChar);
    if (font && font->SupportsChar(aChar))
      return font;
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

nsFontXlib*
nsFontMetricsXlib::FindFont(PRUnichar aChar)
{
  nsFontXlib* font = FindUserDefinedFont(aChar);
  if (!font)
  {
    font = FindLocalFont(aChar);
    if (!font)
    {
      font = FindGenericFont(aChar);
      if (!font)
      {
        font = FindGlobalFont(aChar);
        if (!font)
        {
          font = FindSubstituteFont(aChar);
        }
      }
    }
  }
  
#ifdef NS_FONT_DEBUG_CALL_TRACE
  if (gDebug & NS_FONT_DEBUG_CALL_TRACE) {
    printf("FindFont(%04X)[", aChar);
     for (PRInt32 i = 0; i < mFonts.Count(); i++) {
       printf("%s, ", mFonts.CStringAt(i)->GetBuffer());
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

nsFontEnumeratorXlib ::nsFontEnumeratorXlib()
{
  NS_INIT_REFCNT();
}

NS_IMPL_ISUPPORTS(nsFontEnumeratorXlib,
                  NS_GET_IID(nsIFontEnumerator));

typedef struct EnumerateNodeInfo
{
  PRUnichar** mArray;
  int         mIndex;
  nsIAtom*    mLangGroup;
} EnumerateNodeInfo;

static PRIntn
EnumerateNode(void* aElement, void* aData)
{
  nsFontNode* node = (nsFontNode*) aElement;
  
  EnumerateNodeInfo* info = (EnumerateNodeInfo*) aData;
  
  if (info->mLangGroup != gUserDefined)
  {
    if (node->mCharSetInfo == &Unknown)
      return PR_TRUE;
  }
  else
  {
    if (info->mLangGroup != gUnicode)
    {
      if (node->mCharSetInfo->mLangGroup != info->mLangGroup)
        return PR_TRUE;
	  }
	}
	
  PRUnichar** array = info->mArray;

  int j = info->mIndex;
  PRUnichar* str = node->mName.ToNewUnicode();
  
  if (!str)
  {
    for (j= j-1; j>=0; j--)
      nsMemory::Free(array[j]);
    info->mIndex = 0;
    return PR_FALSE;
  }
  
  array[j] = str;
  info->mIndex++;
  
  return PR_TRUE;
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

  if (NS_FAILED(res))
    return res;

  PRUnichar** array = 
    (PRUnichar**) nsMemory::Alloc(gGlobalList->Count() * sizeof(PRUnichar*));
    
  if(!array)
    return NS_ERROR_OUT_OF_MEMORY;

  EnumerateNodeInfo info = { array, 0, aLangGroup};
  
  if (!gGlobalList->EnumerateForwards(EnumerateNode, &info))
  {
    nsMemory::Free(array);
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_QuickSort(array, info.mIndex, sizeof(PRUnichar*), CompareFontNames, 
               nsnull);

  *aCount = info.mIndex;
  
  if (*aCount)
    *aResult = array;
  else
    nsMemory::Free(array);

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
      
