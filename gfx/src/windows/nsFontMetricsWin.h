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

#ifndef nsFontMetricsWin_h__
#define nsFontMetricsWin_h__

#include <windows.h>

#include "plhash.h"
#include "nsIFontMetrics.h"
#include "nsIFontEnumerator.h"
#include "nsFont.h"
#include "nsString.h"
#include "nsUnitConversion.h"
#include "nsIDeviceContext.h"
#include "nsCRT.h"
#include "nsDeviceContextWin.h"
#include "nsCOMPtr.h"
#include "nsVoidArray.h"
#include "nsICharRepresentable.h"
#include "nsCompressedCharMap.h"

#ifdef FONT_HAS_GLYPH
#undef FONT_HAS_GLYPH
#endif
//#define FONT_HAS_GLYPH(map, g) (((map)[(g) >> 3] >> ((g) & 7)) & 1)
#define FONT_HAS_GLYPH(map, g) IS_REPRESENTABLE(map, g)

#ifdef ADD_GLYPH
#undef ADD_GLYPH
#endif
//#define ADD_GLYPH(map, g) (map)[(g) >> 3] |= (1 << ((g) & 7))
#define ADD_GLYPH(map, g) SET_REPRESENTABLE(map, g)

enum eFontType {
 eFontType_UNKNOWN = -1,
 eFontType_Unicode,
 eFontType_NonUnicode,
};

struct nsCharacterMap {
  PRUint8* mData;
  PRInt32  mLength;
};

struct nsGlobalFont
{
  nsString      name;
  LOGFONT       logFont;
  PRUint16*     ccmap;
  FONTSIGNATURE signature;
  eFontType     fonttype;
  PRUint32      flags;
};

// Bits used for nsGlobalFont.flags
// If this bit is set, then the font is to be ignored
#define NS_GLOBALFONT_SKIP      0x80000000L
// If this bit is set, then the font is a TrueType font
#define NS_GLOBALFONT_TRUETYPE  0x40000000L
// If this bit is set, then the font is a Symbol font (SYMBOL_CHARSET)
#define NS_GLOBALFONT_SYMBOL    0x20000000L

class nsFontWin
{
public:
  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  nsFontWin(LOGFONT* aLogFont, HFONT aFont, PRUint16* aCCMap);
  virtual ~nsFontWin();

  virtual PRInt32 GetWidth(HDC aDC, const PRUnichar* aString,
                           PRUint32 aLength) = 0;

  // XXX return width from DrawString
  virtual void DrawString(HDC aDC, PRInt32 aX, PRInt32 aY,
                          const PRUnichar* aString, PRUint32 aLength) = 0;

  virtual PRBool HasGlyph(PRUnichar ch) {return CCMAP_HAS_CHAR(mCCMap, ch);};
#ifdef MOZ_MATHML
  virtual nsresult
  GetBoundingMetrics(HDC                aDC, 
                     const PRUnichar*   aString,
                     PRUint32           aLength,
                     nsBoundingMetrics& aBoundingMetrics) = 0;
#ifdef NS_DEBUG
  virtual void DumpFontInfo() = 0;
#endif // NS_DEBUG
#endif

  char            mName[LF_FACESIZE];
  HFONT           mFont;
  PRUint16*       mCCMap;
#ifdef MOZ_MATHML
  nsCharacterMap* mCMAP;
#endif

  nscoord         mMaxAscent;
  nscoord         mMaxDescent;
};

// A "substitute font" to deal with missing glyphs -- see bug 6585
// We now use transliteration+fallback to the REPLACEMENT CHAR + 
// HEX representation to handle this issue.
class nsFontWinSubstitute : public nsFontWin
{
public:
  nsFontWinSubstitute(LOGFONT* aLogFont, HFONT aFont, PRUint16* aCCMap, PRBool aDisplayUnicode);
  virtual ~nsFontWinSubstitute();

  virtual PRBool HasGlyph(PRUnichar ch) {return IS_REPRESENTABLE(mRepresentableCharMap, ch);};
  virtual void SetRepresentable(PRUnichar ch) {SET_REPRESENTABLE(mRepresentableCharMap, ch);};
  virtual PRInt32 GetWidth(HDC aDC, const PRUnichar* aString, PRUint32 aLength);
  virtual void DrawString(HDC aDC, PRInt32 aX, PRInt32 aY,
                          const PRUnichar* aString, PRUint32 aLength);
#ifdef MOZ_MATHML
  virtual nsresult
  GetBoundingMetrics(HDC                aDC,
                     const PRUnichar*   aString,
                     PRUint32           aLength,
                     nsBoundingMetrics& aBoundingMetrics);
#ifdef NS_DEBUG
  virtual void DumpFontInfo();
#endif // NS_DEBUG
#endif
private:
  PRBool mDisplayUnicode;

  //We need to have a easily operatable charmap for substitute font
  PRUint32 mRepresentableCharMap[UCS2_MAP_LEN];
};

/**
 * nsFontSwitchCallback
 *
 * Font-switching callback function. Used by ResolveForwards() and
 * ResolveBackwards(). aFontSwitch points to a structure that gives
 * details about the current font needed to represent the current
 * substring. In particular, this struct contains a handler to the font
 * and some metrics of the font. These metrics may be different from
 * the metrics obtained via nsIFontMetrics.
 * Return PR_FALSE to stop the resolution of the remaining substrings.
 */

struct nsFontSwitch {
  // Simple wrapper on top of nsFontWin for the moment
  // Could hold other attributes of the font
  nsFontWin* mFontWin;
};

typedef PRBool (*PR_CALLBACK nsFontSwitchCallback)
               (const nsFontSwitch* aFontSwitch,
                const PRUnichar*    aSubstring,
                PRUint32            aSubstringLength,
                void*               aData);

class nsFontMetricsWin : public nsIFontMetrics
{
public:
  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW
  NS_DECL_ISUPPORTS

  nsFontMetricsWin();
  virtual ~nsFontMetricsWin();

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
  NS_IMETHOD  GetFont(const nsFont *&aFont);
  NS_IMETHOD  GetLangGroup(nsIAtom** aLangGroup);
  NS_IMETHOD  GetFontHandle(nsFontHandle &aHandle);
  NS_IMETHOD  GetAveCharWidth(nscoord &aAveCharWidth);
  NS_IMETHOD  GetSpaceWidth(nscoord &aSpaceWidth);

  virtual nsresult
  ResolveForwards(HDC                  aDC,
                  const PRUnichar*     aString,
                  PRUint32             aLength,
                  nsFontSwitchCallback aFunc, 
                  void*                aData);

  virtual nsresult
  ResolveBackwards(HDC                  aDC,
                   const PRUnichar*     aString,
                   PRUint32             aLength,
                   nsFontSwitchCallback aFunc, 
                   void*                aData);

  nsFontWin*         FindFont(HDC aDC, PRUnichar aChar);
  virtual nsFontWin* FindUserDefinedFont(HDC aDC, PRUnichar aChar);
  virtual nsFontWin* FindLocalFont(HDC aDC, PRUnichar aChar);
  virtual nsFontWin* FindGenericFont(HDC aDC, PRUnichar aChar);
  virtual nsFontWin* FindGlobalFont(HDC aDC, PRUnichar aChar);
  virtual nsFontWin* FindSubstituteFont(HDC aDC, PRUnichar aChar);

  virtual nsFontWin* LoadFont(HDC aDC, nsString* aName);
  virtual nsFontWin* LoadGenericFont(HDC aDC, PRUnichar aChar, nsString* aName);
  virtual nsFontWin* LoadGlobalFont(HDC aDC, nsGlobalFont* aGlobalFontItem);
  virtual nsFontWin* LoadSubstituteFont(HDC aDC, nsString* aName);

  virtual nsFontWin* GetFontFor(HFONT aHFONT);

  nsCOMPtr<nsIAtom>   mLangGroup;
  nsStringArray       mFonts;
  PRUint16            mFontsIndex;
  nsVoidArray         mLoadedFonts;
  nsFontWin          *mSubstituteFont;

  PRUint16            mGenericIndex;
  nsString            mGeneric;

  nsString            mUserDefined;

  PRBool              mTriedAllGenerics;
  PRBool              mIsUserDefined;

  static PRUint16*    gEmptyCCMap;
  static PLHashTable* gFontMaps;
  static PLHashTable* gFamilyNames;
  static PLHashTable* gFontWeights;
  static nsVoidArray* gGlobalFonts;

  static nsVoidArray* InitializeGlobalFonts(HDC aDC);

  static void SetFontWeight(PRInt32 aWeight, PRUint16* aWeightTable);
  static PRBool IsFontWeightAvailable(PRInt32 aWeight, PRUint16 aWeightTable);

  static PRUint16* GetFontCCMAP(HDC aDC, const char* aShortName, eFontType& aFontType, PRUint8& aCharset);
  static PRUint16* GetCCMAP(HDC aDC, const char* aShortName, eFontType* aFontType, PRUint8* aCharset);

  static int SameAsPreviousMap(int aIndex);

  // This function creates a possibly adjusted font
  HFONT CreateFontHandle(HDC aDC, LOGFONT* aLogFont);
  void InitMetricsFor(HDC aDC, nsFontWin* aFontWin);

protected:
  // @description Font Weights
  // Each available font weight is stored as as single bit inside a PRUint16.
  // e.g. The binary value 0000000000001000 indcates font weight 400 is available.
  // while the binary value 0000000000001001 indicates both font weight 100 and 400 are available
  // The font weights which will be represented include {100, 200, 300, 400, 500, 600, 700, 800, 900}
  // The font weight specified in the mFont->weight may include values which are not an even multiple of 100.
  // If so, the font weight mod 100 indicates the number steps to lighten are make bolder.
  // This corresponds to the CSS lighter and bolder property values. If bolder is applied twice to the font which has
  // a font weight of 400 then the mFont->weight will contain the value 402.
  // If lighter is applied twice to a font of weight 400 then the mFont->weight will contain the value 398.
  // Only nine steps of bolder or lighter are allowed by the CSS XPCODE.
  // The font weight table is used in conjuction with the mFont->weight to determine
  // what font weight to pass in the LOGFONT structure.

  // Utility methods for managing font weights.
  PRUint16 LookForFontWeightTable(HDC aDc, nsString* aName);
  PRInt32  GetBolderWeight(PRInt32 aWeight, PRInt32 aDistance, PRUint16 aWeightTable);
  PRInt32  GetLighterWeight(PRInt32 aWeight, PRInt32 aDistance, PRUint16 aWeightTable);
  PRInt32  GetFontWeight(PRInt32 aWeight, PRUint16 aWeightTable);
  PRInt32  GetClosestWeight(PRInt32 aWeight, PRUint16 aWeightTable);
  PRUint16 GetFontWeightTable(HDC aDC, nsString* aFontName);

  nsresult RealizeFont();
  void FillLogFont(LOGFONT* aLogFont, PRInt32 aWeight,
                   PRBool aSizeOnly=PR_FALSE, PRBool aSkipZoom=PR_FALSE);
  static PLHashTable* InitializeFamilyNames(void);

  nsDeviceContextWin *mDeviceContext;
  nsFont              mFont;

  HFONT               mFontHandle;

  nscoord             mLeading;
  nscoord             mEmHeight;
  nscoord             mEmAscent;
  nscoord             mEmDescent;
  nscoord             mMaxHeight;
  nscoord             mMaxAscent;
  nscoord             mMaxDescent;
  nscoord             mMaxAdvance;
  nscoord             mAveCharWidth;
  nscoord             mXHeight;
  nscoord             mSuperscriptOffset;
  nscoord             mSubscriptOffset;
  nscoord             mStrikeoutSize;
  nscoord             mStrikeoutOffset;
  nscoord             mUnderlineSize;
  nscoord             mUnderlineOffset;
  nscoord             mSpaceWidth;
};


class nsFontEnumeratorWin : public nsIFontEnumerator
{
public:
  nsFontEnumeratorWin();
  NS_DECL_ISUPPORTS
  NS_DECL_NSIFONTENUMERATOR
};


// The following is a workaround for a Japanse Windows 95 problem.

class nsFontWinA;
class nsFontMetricsWinA;

class nsFontSubset : public nsFontWin
{
public:
  nsFontSubset();
  virtual ~nsFontSubset();

  virtual PRInt32 GetWidth(HDC aDC, const PRUnichar* aString,
                           PRUint32 aLength);
  virtual void DrawString(HDC aDC, PRInt32 aX, PRInt32 aY,
                          const PRUnichar* aString, PRUint32 aLength);
#ifdef MOZ_MATHML
  virtual nsresult
  GetBoundingMetrics(HDC                aDC, 
                     const PRUnichar*   aString,
                     PRUint32           aLength,
                     nsBoundingMetrics& aBoundingMetrics);
#ifdef NS_DEBUG
  virtual void DumpFontInfo();
#endif // NS_DEBUG
#endif

  int Load(HDC aDC, nsFontMetricsWinA* aFontMetricsWin, nsFontWinA* aFont);

  // convert a Unicode string to ANSI within our codepage
  virtual void Convert(const PRUnichar* aString, PRUint32 aLength,
                       char** aResult /*IN/OUT*/, int* aResultLength /*IN/OUT*/);

  BYTE     mCharset;
  PRUint16 mCodePage;
};

class nsFontWinA : public nsFontWin
{
public:
  nsFontWinA(LOGFONT* aLogFont, HFONT aFont, PRUint16* aCCMap);
  virtual ~nsFontWinA();

  virtual PRInt32 GetWidth(HDC aDC, const PRUnichar* aString,
                           PRUint32 aLength);
  virtual void DrawString(HDC aDC, PRInt32 aX, PRInt32 aY,
                          const PRUnichar* aString, PRUint32 aLength);
  virtual nsFontSubset* FindSubset(HDC aDC, PRUnichar aChar, nsFontMetricsWinA* aFontMetrics);
#ifdef MOZ_MATHML
  virtual nsresult
  GetBoundingMetrics(HDC                aDC, 
                     const PRUnichar*   aString,
                     PRUint32           aLength,
                     nsBoundingMetrics& aBoundingMetrics);
#ifdef NS_DEBUG
  virtual void DumpFontInfo();
#endif // NS_DEBUG
#endif

  int GetSubsets(HDC aDC);

  LOGFONT        mLogFont;
  nsFontSubset** mSubsets;
  PRUint16       mSubsetsCount;
};

class nsFontWinSubstituteA : public nsFontWinA
{
public:
  nsFontWinSubstituteA(LOGFONT* aLogFont, HFONT aFont, PRUint16* aCCMap);
  virtual ~nsFontWinSubstituteA();

  virtual PRBool HasGlyph(PRUnichar ch) {return IS_REPRESENTABLE(mRepresentableCharMap, ch);};
  virtual void SetRepresentable(PRUnichar ch) {SET_REPRESENTABLE(mRepresentableCharMap, ch);};
  virtual nsFontSubset* FindSubset(HDC aDC, PRUnichar aChar, nsFontMetricsWinA* aFontMetrics) {return mSubsets[0];};

  //We need to have a easily operatable charmap for substitute font
  PRUint32 mRepresentableCharMap[UCS2_MAP_LEN];
};


class nsFontMetricsWinA : public nsFontMetricsWin
{
public:
  virtual nsFontWin* FindLocalFont(HDC aDC, PRUnichar aChar);
  virtual nsFontWin* LoadGenericFont(HDC aDC, PRUnichar aChar, nsString* aName);
  virtual nsFontWin* FindGlobalFont(HDC aDC, PRUnichar aChar);
  virtual nsFontWin* FindSubstituteFont(HDC aDC, PRUnichar aChar);

  virtual nsFontWin* LoadFont(HDC aDC, nsString* aName);
  virtual nsFontWin* LoadGlobalFont(HDC aDC, nsGlobalFont* aGlobalFontItem);
  virtual nsFontWin* LoadSubstituteFont(HDC aDC, nsString* aName);

  virtual nsFontWin* GetFontFor(HFONT aHFONT);

  virtual nsresult
  ResolveForwards(HDC                  aDC,
                  const PRUnichar*     aString,
                  PRUint32             aLength,
                  nsFontSwitchCallback aFunc, 
                  void*                aData);

  virtual nsresult
  ResolveBackwards(HDC                  aDC,
                   const PRUnichar*     aString,
                   PRUint32             aLength,
                   nsFontSwitchCallback aFunc, 
                   void*                aData);
};

#endif /* nsFontMetricsWin_h__ */
