/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 *   Tony Tsui <tony@igelaus.com.au>
 *   Tim Copperfield <timecop@network.email.ne.jp>
 *   Roland Mainz <roland.mainz@informatik.med.uni-giessen.de>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsFontMetricsXlib_h__
#define nsFontMetricsXlib_h__

#include "nsIFontMetrics.h"
#include "nsIFontEnumerator.h"
#include "nsIDeviceContext.h"
#include "nsDeviceContextXlib.h"
#include "nsFont.h"
#include "nsString.h"
#include "nsUnitConversion.h"
#include "nsCRT.h"
#include "nsCOMPtr.h"
#include "nsRenderingContextXlib.h"
#include "nsICharRepresentable.h"
#include "nsCompressedCharMap.h"

/* Undefine some CPP symbols which wrap not-yet-implemented code */
#undef MOZ_ENABLE_FREETYPE2
#undef USE_AASB
#undef USE_X11SHARED_CODE

#ifdef USE_X11SHARED_CODE
/* XXX: I wish I would use the code in gfx/src/x11shared/ - unfortunately
 * it is full of GDK/GTK+ dependices which makes it impossible to use it
 * yet... ;-(
 */ 
#error not implemented yet
#include "nsXFontNormal.h"
#else
class nsX11FontNormal {
public:
  nsX11FontNormal(Display *, XFontStruct *);
  ~nsX11FontNormal();

  void          DrawText8(Drawable Drawable, GC GC, PRInt32, PRInt32,
                          const char *, PRUint32);
  void          DrawText16(Drawable Drawable, GC GC, PRInt32, PRInt32,
                           const XChar2b *, PRUint32);
  PRBool        GetXFontProperty(Atom, unsigned long *);
  XFontStruct  *GetXFontStruct();
  PRBool        LoadFont();
  void          TextExtents8(const char *, PRUint32, PRInt32*, PRInt32*,
                             PRInt32*, PRInt32*, PRInt32*);
  void          TextExtents16(const XChar2b *, PRUint32, PRInt32*, PRInt32*,
                              PRInt32*, PRInt32*, PRInt32*);
  PRInt32       TextWidth8(const char *, PRUint32);
  PRInt32       TextWidth16(const XChar2b *, PRUint32);
  void          UnloadFont();
  inline PRBool IsSingleByte() { return mIsSingleByte; };
protected:
  Display     *mDisplay; /* Track |Display *| for this font 
                          * (Xlib gfx supports multiple displays) */
  XFontStruct *mXFont;
  PRBool       mIsSingleByte;
};

/* XXX: Silly class rename hack to avoid issues with StaticBuild code and to
 * keep the source clean until we can reuse the classes from gfx/src/x11shared/
 */
#define nsXFont       nsX11FontNormal
#define nsXFontNormal nsX11FontNormal
#endif /* USE_X11SHARED_CODE */

class nsFontXlib;
class nsXFont;
class nsRenderingContextXlib;
class nsIDrawingSurfaceXlib;

/* nsFontMetricsXlibContext glue */
class nsFontMetricsXlibContext;
nsresult CreateFontMetricsXlibContext(nsIDeviceContext *aDevice, PRBool printermode, nsFontMetricsXlibContext **aFontMetricsXlibContext);
void     DeleteFontMetricsXlibContext(nsFontMetricsXlibContext *aFontMetricsXlibContext);

#undef FONT_HAS_GLYPH
#define FONT_HAS_GLYPH(map, char) IS_REPRESENTABLE(map, char)
#define WEIGHT_INDEX(weight) (((weight) / 100) - 1)

#define NS_FONT_DEBUG_LOAD_FONT   0x01
#define NS_FONT_DEBUG_CALL_TRACE  0x02
#define NS_FONT_DEBUG_FIND_FONT   0x04
#define NS_FONT_DEBUG_SIZE_FONT   0x08
#define NS_FONT_DEBUG_SCALED_FONT       0x10
#define NS_FONT_DEBUG_BANNED_FONT       0x20
#define NS_FONT_DEBUG_FONT_CATALOG      0x100
#define NS_FONT_DEBUG_FONT_SCAN         0x200
#define NS_FONT_DEBUG_FREETYPE_FONT     0x400
#define NS_FONT_DEBUG_FREETYPE_GRAPHICS 0x800

#undef NS_FONT_DEBUG
#define NS_FONT_DEBUG 1
#ifdef NS_FONT_DEBUG

# define DEBUG_PRINTF(x) \
         DEBUG_PRINTF_MACRO(x, 0xFFFF)

# define DEBUG_PRINTF_MACRO(x, type) \
            PR_BEGIN_MACRO \
              if (gFontDebug & (type)) { \
                printf x ; \
                printf(", %s %d\n", __FILE__, __LINE__); \
              } \
            PR_END_MACRO
#else
# define DEBUG_PRINTF_MACRO(x, type) \
            PR_BEGIN_MACRO \
            PR_END_MACRO
#endif

#define FIND_FONT_PRINTF(x) \
         DEBUG_PRINTF_MACRO(x, NS_FONT_DEBUG_FIND_FONT)

#define SIZE_FONT_PRINTF(x) \
         DEBUG_PRINTF_MACRO(x, NS_FONT_DEBUG_SIZE_FONT)

#define SCALED_FONT_PRINTF(x) \
         DEBUG_PRINTF_MACRO(x, NS_FONT_DEBUG_SCALED_FONT)

#define BANNED_FONT_PRINTF(x) \
         DEBUG_PRINTF_MACRO(x, NS_FONT_DEBUG_BANNED_FONT)

#define FONT_CATALOG_PRINTF(x) \
         DEBUG_PRINTF_MACRO(x, NS_FONT_DEBUG_FONT_CATALOG)

#define FONT_SCAN_PRINTF(x) \
            PR_BEGIN_MACRO \
              if (gFontDebug & NS_FONT_DEBUG_FONT_SCAN) { \
                printf x ; \
                fflush(stdout); \
              } \
            PR_END_MACRO

#define FREETYPE_FONT_PRINTF(x) \
         DEBUG_PRINTF_MACRO(x, NS_FONT_DEBUG_FREETYPE_FONT)

typedef struct nsFontCharSetInfoXlib nsFontCharSetInfoXlib;

typedef int (*nsFontCharSetConverterXlib)(nsFontCharSetInfoXlib* aSelf,
  XFontStruct* aFont, const PRUnichar* aSrcBuf, PRInt32 aSrcLen,
  char* aDestBuf, PRInt32 aDestLen);

struct nsFontCharSetXlib;
struct nsFontFamilyXlib;
struct nsFontNodeXlib;
struct nsFontStretchXlib;
struct nsFontWeightXlib;

class nsFontXlibUserDefined;
class nsFontMetricsXlib;
#ifdef MOZ_ENABLE_FREETYPE2
class nsFreeTypeFace;
#endif /* MOZ_ENABLE_FREETYPE2 */

struct nsFontStretchXlib
{
  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  void SortSizes(void);

  nsFontXlib       **mSizes;
  PRUint16           mSizesAlloc;
  PRUint16           mSizesCount;

  char*              mScalable;
  PRBool             mOutlineScaled;
  nsVoidArray        mScaledFonts;
#ifdef MOZ_ENABLE_FREETYPE2
  nsFreeTypeFace    *mFreeTypeFaceID;
#endif /* MOZ_ENABLE_FREETYPE2 */
};

struct nsFontStyleXlib
{
  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  void FillWeightHoles(void);

  nsFontWeightXlib *mWeights[9];
};

struct nsFontWeightXlib
{
  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  void FillStretchHoles(void);

  nsFontStretchXlib *mStretches[9];
};

struct nsFontNodeXlib
{
  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  void FillStyleHoles(void);

  nsCAutoString          mName;
  nsFontCharSetInfoXlib *mCharSetInfo;
  nsFontStyleXlib       *mStyles[3];
  PRUint8                mHolesFilled;
  PRUint8                mDummy;
};

class nsFontNodeArrayXlib : public nsAutoVoidArray
{
public:
  nsFontNodeArrayXlib() {};

  nsFontNodeXlib *GetElement(PRInt32 aIndex)
  {
    return (nsFontNodeXlib *) ElementAt(aIndex);
  };
};

/*
 * Font Language Groups
 *
 * These Font Language Groups (FLG) indicate other related
 * encodings to look at when searching for glyphs
 *
 */
typedef struct nsFontLangGroupXlib {
  const char *mFontLangGroupName;
  nsIAtom*    mFontLangGroupAtom;
} nsFontLangGroup;

struct nsFontCharSetMapXlib
{
  const char*            mName;
  nsFontLangGroupXlib*   mFontLangGroup;
  nsFontCharSetInfoXlib* mInfo;
};

class nsFontXlib
{
public:
  nsFontXlib();
  nsFontXlib(nsFontXlib *);
  virtual ~nsFontXlib();
  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  nsFontMetricsXlibContext *mFontMetricsContext;

  void LoadFont(void);
  PRBool IsEmptyFont(XFontStruct*);

  inline int SupportsChar(PRUnichar aChar)
    { return mCCMap && CCMAP_HAS_CHAR(mCCMap, aChar); };

  virtual XFontStruct *GetXFontStruct(void);
  virtual nsXFont     *GetXFont(void);
  virtual PRBool       GetXFontIs10646(void);
#ifdef MOZ_ENABLE_FREETYPE2
  virtual PRBool       IsFreeTypeFont(void);
#endif /* MOZ_ENABLE_FREETYPE2 */
  virtual int          GetWidth(const PRUnichar* aString, PRUint32 aLength) = 0;
  virtual int          DrawString(nsRenderingContextXlib *aContext,
                                  nsIDrawingSurfaceXlib *aSurface, nscoord aX,
                                  nscoord aY, const PRUnichar* aString,
                                  PRUint32 aLength) = 0;
#ifdef MOZ_MATHML
  // bounding metrics for a string 
  // remember returned values are not in app units 
  // - to emulate GetWidth () above
  virtual nsresult
  GetBoundingMetrics(const PRUnichar*   aString,
                     PRUint32           aLength,
                     nsBoundingMetrics& aBoundingMetrics) = 0;
#endif /* MOZ_MATHML */

  PRUint16              *mCCMap;
  nsFontCharSetInfoXlib *mCharSetInfo;
  char                  *mName;
  nsFontXlibUserDefined *mUserDefinedFont;
  PRUint16               mSize;
#ifdef USE_AASB
  PRUint16               mAABaseSize;
#endif /* USE_AASB */
  PRInt16                mBaselineAdjust;

  // these values are not in app units, they need to be scaled with 
  // nsIDeviceContext::DevUnitsToAppUnits()
  PRInt16                mMaxAscent;
  PRInt16                mMaxDescent;

protected:
  XFontStruct           *mFont;
  XFontStruct           *mFontHolder;
  nsXFont               *mXFont;
  PRPackedBool           mAlreadyCalledLoadFont;
};

struct nsFontSwitchXlib {
  // Simple wrapper on top of nsFontXlib for the moment
  // Could hold other attributes of the font
  nsFontXlib *mFontXlib;
};

typedef PRBool (*PR_CALLBACK nsFontSwitchCallbackXlib)
               (const nsFontSwitchXlib *aFontSwitch,
                const PRUnichar*        aSubstring,
                PRUint32                aSubstringLength,
                void*                   aData);

class nsFontMetricsXlib : public nsIFontMetrics
{
public:
  nsFontMetricsXlib();
  virtual ~nsFontMetricsXlib();

  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  NS_DECL_ISUPPORTS

  NS_IMETHOD  Init(const nsFont& aFont, nsIAtom* aLangGroup,
                   nsIDeviceContext* aContext);
  NS_IMETHOD  Destroy();

  NS_IMETHOD  GetXHeight(nscoord& aResult);
  NS_IMETHOD  GetSuperscriptOffset(nscoord& aResult);
  NS_IMETHOD  GetSubscriptOffset(nscoord& aResult);
  NS_IMETHOD  GetStrikeout(nscoord& aOffset, nscoord& aSize);
  NS_IMETHOD  GetUnderline(nscoord& aOffset, nscoord& aSize);

  NS_IMETHOD  GetHeight(nscoord &aHeight);
  NS_IMETHOD  GetNormalLineHeight(nscoord &aHeight);
  NS_IMETHOD  GetLeading(nscoord &aLeading);
  NS_IMETHOD  GetEmHeight(nscoord &aHeight);
  NS_IMETHOD  GetEmAscent(nscoord &aAscent);
  NS_IMETHOD  GetEmDescent(nscoord &aDescent);
  NS_IMETHOD  GetMaxHeight(nscoord &aHeight);
  NS_IMETHOD  GetMaxAscent(nscoord &aAscent);
  NS_IMETHOD  GetMaxDescent(nscoord &aDescent);
  NS_IMETHOD  GetMaxAdvance(nscoord &aAdvance);
  NS_IMETHOD  GetAveCharWidth(nscoord &aAveCharWidth);
  NS_IMETHOD  GetFont(const nsFont *&aFont);
  NS_IMETHOD  GetLangGroup(nsIAtom** aLangGroup);
  NS_IMETHOD  GetFontHandle(nsFontHandle &aHandle);
  
  NS_IMETHOD  GetSpaceWidth(nscoord &aSpaceWidth);
  NS_IMETHOD  ResolveForwards(const PRUnichar* aString, PRUint32 aLength,
                              nsFontSwitchCallbackXlib aFunc, void* aData);
  nsFontXlib*  FindFont(PRUnichar aChar);
  nsFontXlib*  FindUserDefinedFont(PRUnichar aChar);
  nsFontXlib*  FindStyleSheetSpecificFont(PRUnichar aChar);
  nsFontXlib*  FindStyleSheetGenericFont(PRUnichar aChar);
  nsFontXlib*  FindLangGroupPrefFont(nsIAtom* aLangGroup, PRUnichar aChar);
  nsFontXlib*  FindLangGroupFont(nsIAtom* aLangGroup, PRUnichar aChar, nsCString* aName);
  nsFontXlib*  FindAnyFont(PRUnichar aChar);
  nsFontXlib*  FindSubstituteFont(PRUnichar aChar);

  nsFontXlib*  SearchNode(nsFontNodeXlib* aNode, PRUnichar aChar);
  nsFontXlib*  TryAliases(nsCString* aName, PRUnichar aChar);
  nsFontXlib*  TryFamily(nsCString* aName, PRUnichar aChar);
  nsFontXlib*  TryNode(nsCString* aName, PRUnichar aChar);
  nsFontXlib*  TryNodes(nsACString &aFFREName, PRUnichar aChar);
  nsFontXlib*  TryLangGroup(nsIAtom* aLangGroup, nsCString* aName, PRUnichar aChar);

  nsFontXlib*  AddToLoadedFontsList(nsFontXlib* aFont);
  nsFontXlib*  FindNearestSize(nsFontStretchXlib* aStretch, PRUint16 aSize);
#ifdef USE_AASB
  nsFontXlib*  GetAASBBaseFont(nsFontStretchXlib *aStretch, 
                               nsFontCharSetInfoXlib* aCharSet);
#endif /* USE_AASB */
  nsFontXlib*  PickASizeAndLoad(nsFontStretchXlib *aStretch,
                               nsFontCharSetInfoXlib *aCharSet, 
                               PRUnichar aChar,
                               const char *aName);

  static nsresult FamilyExists(nsFontMetricsXlibContext *aFontMetricsContext, const nsString& aName);

  //friend struct nsFontXlib;

  nsFontXlib **mLoadedFonts;
  PRUint16     mLoadedFontsAlloc;
  PRUint16     mLoadedFontsCount;

  nsFontXlib  *mSubstituteFont;

  nsCStringArray    mFonts;
  PRUint16          mFontsIndex;
  nsAutoVoidArray   mFontIsGeneric;

  nsCAutoString     mDefaultFont;
  nsCString        *mGeneric;
  nsCOMPtr<nsIAtom> mLangGroup;
  nsCAutoString     mUserDefined;

  PRUint8           mTriedAllGenerics;
  PRUint8           mIsUserDefined;

  nsFontMetricsXlibContext *mFontMetricsContext;
  nsIDeviceContext         *mDeviceContext;

protected:
  void RealizeFont();
  nsFontXlib *LocateFont(PRUint32 aChar, PRInt32 & aCount);

  nsFont             *mFont;
  nsFontXlib         *mWesternFont;

  nscoord             mLeading;
  nscoord             mEmHeight;
  nscoord             mEmAscent;
  nscoord             mEmDescent;
  nscoord             mMaxHeight;
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
  nscoord             mAveCharWidth;

  PRUint16            mPixelSize;
  PRUint8             mStretchIndex;
  PRUint8             mStyleIndex;
  nsFontCharSetConverterXlib mDocConverterType;
};

class nsFontEnumeratorXlib : public nsIFontEnumerator
{
public:
  nsFontEnumeratorXlib();
  NS_DECL_ISUPPORTS
  NS_DECL_NSIFONTENUMERATOR
};

class nsHashKey;

/* XXX: We can't include gfx/src/x11shared/nsFreeType.h because it relies on
 * GDK/GTK+ includes which are not available in Xlib builds (fix is to remove
 * the GDK/GTK+ dependicy from the code in gfx/src/x11shared/ ...)
 */
#ifndef MOZ_ENABLE_FREETYPE2
/*
 * Defines for the TrueType codepage bits.
 * Used as a hint for the languages supported in a TrueType font.
 */

/*
 * ulCodePageRange1
 */
#define TT_OS2_CPR1_LATIN1       (0x00000001) /* Latin 1                     */
#define TT_OS2_CPR1_LATIN2       (0x00000002) /* Latin 2: Eastern Europe     */
#define TT_OS2_CPR1_CYRILLIC     (0x00000004) /* Cyrillic                    */
#define TT_OS2_CPR1_GREEK        (0x00000008) /* Greek                       */
#define TT_OS2_CPR1_TURKISH      (0x00000010) /* Turkish                     */
#define TT_OS2_CPR1_HEBREW       (0x00000020) /* Hebrew                      */
#define TT_OS2_CPR1_ARABIC       (0x00000040) /* Arabic                      */
#define TT_OS2_CPR1_BALTIC       (0x00000080) /* Windows Baltic              */
#define TT_OS2_CPR1_VIETNAMESE   (0x00000100) /* Vietnamese                  */
                                 /* 9-15     Reserved for Alternate ANSI     */
#define TT_OS2_CPR1_THAI         (0x00010000) /* Thai                        */
#define TT_OS2_CPR1_JAPANESE     (0x00020000) /* JIS/Japan                   */
#define TT_OS2_CPR1_CHINESE_SIMP (0x00040000) /* Chinese: Simplified         */
#define TT_OS2_CPR1_KO_WANSUNG   (0x00080000) /* Korean Wansung              */
#define TT_OS2_CPR1_CHINESE_TRAD (0x00100000) /* Chinese: Traditional        */
#define TT_OS2_CPR1_KO_JOHAB     (0x00200000) /* Korean Johab                */
                                 /* 22-28    Reserved for Alternate ANSI&OEM */
#define TT_OS2_CPR1_MAC_ROMAN    (0x20000000) /* Mac (US Roman)              */
#define TT_OS2_CPR1_OEM          (0x40000000) /* OEM Character Set           */
#define TT_OS2_CPR1_SYMBOL       (0x80000000) /* Symbol Character Set        */

/*
 * ulCodePageRange2
 */                              /* 32-47    Reserved for OEM                */
#define TT_OS2_CPR2_GREEK        (0x00010000) /* IBM Greek                   */
#define TT_OS2_CPR2_RUSSIAN      (0x00020000) /* MS-DOS Russian              */
#define TT_OS2_CPR2_NORDIC       (0x00040000) /* MS-DOS Nordic               */
#define TT_OS2_CPR2_ARABIC       (0x00080000) /* Arabic                      */
#define TT_OS2_CPR2_CA_FRENCH    (0x00100000) /* MS-DOS Canadian French      */
#define TT_OS2_CPR2_HEBREW       (0x00200000) /* Hebrew                      */
#define TT_OS2_CPR2_ICELANDIC    (0x00400000) /* MS-DOS Icelandic            */
#define TT_OS2_CPR2_PORTUGESE    (0x00800000) /* MS-DOS Portuguese           */
#define TT_OS2_CPR2_TURKISH      (0x01000000) /* IBM Turkish                 */
#define TT_OS2_CPR2_CYRILLIC     (0x02000000)/*IBM Cyrillic; primarily Russian*/
#define TT_OS2_CPR2_LATIN2       (0x04000000) /* Latin 2                     */
#define TT_OS2_CPR2_BALTIC       (0x08000000) /* MS-DOS Baltic               */
#define TT_OS2_CPR2_GREEK_437G   (0x10000000) /* Greek; former 437 G         */
#define TT_OS2_CPR2_ARABIC_708   (0x20000000) /* Arabic; ASMO 708            */
#define TT_OS2_CPR2_WE_LATIN1    (0x40000000) /* WE/Latin 1                  */
#define TT_OS2_CPR2_US           (0x80000000) /* US                          */
#endif /* !MOZ_ENABLE_FREETYPE2 */

#endif /* !nsFontMetricsXlib_h__ */

