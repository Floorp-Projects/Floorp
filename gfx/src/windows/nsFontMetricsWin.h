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

#ifdef MOZ_MATHML
struct nsCharacterMap {
  PRUint8* mData;
  PRInt32  mLength;
};
#endif

class nsFontWin
{
public:
  nsFontWin(LOGFONT* aLogFont, HFONT aFont, PRUint32* aMap);
  virtual ~nsFontWin();
  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  virtual PRInt32 GetWidth(HDC aDC, const PRUnichar* aString,
                           PRUint32 aLength) = 0;
  // XXX return width from DrawString
  virtual void DrawString(HDC aDC, PRInt32 aX, PRInt32 aY,
                          const PRUnichar* aString, PRUint32 aLength) = 0;
#ifdef MOZ_MATHML
  virtual nsresult
  GetBoundingMetrics(HDC                aDC, 
                     float              aItalicSlope,
                     const PRUnichar*   aString,
                     PRUint32           aLength,
                     nsBoundingMetrics& aBoundingMetrics) = 0;
#ifdef NS_DEBUG
  virtual void DumpFontInfo() = 0;
#endif // NS_DEBUG
#endif

  char      mName[LF_FACESIZE];
  HFONT     mFont;
  PRUint32* mMap;
#ifdef MOZ_MATHML
  nsCharacterMap* mCMAP;
#endif
};

typedef struct nsGlobalFont
{
  nsString*     name;
  LOGFONT       logFont;
  PRUint32*     map;
  PRUint8       skip;
  FONTSIGNATURE signature;
} nsGlobalFont;

class nsFontMetricsWin : public nsIFontMetrics
{
public:
  nsFontMetricsWin();
  virtual ~nsFontMetricsWin();

  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  NS_DECL_ISUPPORTS

  NS_IMETHOD  Init(const nsFont& aFont, nsIAtom* aLangGroup,
                   nsIDeviceContext* aContext);
  NS_IMETHOD  Destroy();

#ifdef MOZ_MATHML
  NS_IMETHOD  GetItalicSlope(float& aResult);
#endif
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
  NS_IMETHOD  GetLangGroup(nsIAtom** aLangGroup);
  NS_IMETHOD  GetFontHandle(nsFontHandle &aHandle);
  NS_IMETHOD  GetAveCharWidth(nscoord &aAveCharWidth);

  virtual nsresult   GetSpaceWidth(nscoord &aSpaceWidth);
  virtual nsFontWin* FindGlobalFont(HDC aDC, PRUnichar aChar);
  virtual nsFontWin* FindGenericFont(HDC aDC, PRUnichar aChar);
  virtual nsFontWin* FindLocalFont(HDC aDC, PRUnichar aChar);
  nsFontWin*         FindFont(HDC aDC, PRUnichar aChar);
  virtual nsFontWin* LoadGenericFont(HDC aDC, PRUnichar aChar, char** aName);
  virtual nsFontWin* LoadFont(HDC aDC, nsString* aName);

  virtual nsFontWin* LoadSubstituteFont(HDC aDC, nsString* aName);
  virtual nsFontWin* FindSubstituteFont(HDC aDC, PRUnichar aChar);

  int SameAsPreviousMap(int aIndex);

  nsFontWin           **mLoadedFonts;
  PRUint16            mLoadedFontsAlloc;
  PRUint16            mLoadedFontsCount;

  PRInt32             mIndexOfSubstituteFont;

  nsString            *mFonts;
  PRUint16            mFontsAlloc;
  PRUint16            mFontsCount;
  PRUint16            mFontsIndex;

  nsString            *mGeneric;
  int                 mTriedAllGenerics;
  nsCOMPtr<nsIAtom>   mLangGroup;

  nscoord             mSpaceWidth;

  static PLHashTable* gFontMaps;
  static nsGlobalFont* gGlobalFonts;
  static int gGlobalFontsCount;

  static nsGlobalFont* InitializeGlobalFonts(HDC aDC);

  static void SetFontWeight(PRInt32 aWeight, PRUint16* aWeightTable);
  static PRBool IsFontWeightAvailable(PRInt32 aWeight, PRUint16 aWeightTable);

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
  
  void FillLogFont(LOGFONT* aLogFont, PRInt32 aWeight);
  void RealizeFont();

  nsDeviceContextWin  *mDeviceContext;
  nsFont              *mFont;
  nscoord             mHeight;
  nscoord             mAscent;
  nscoord             mDescent;
  nscoord             mLeading;
  nscoord             mMaxAscent;
  nscoord             mMaxDescent;
  nscoord             mMaxAdvance;
  nscoord             mAveCharWidth;
#ifdef MOZ_MATHML
  float               mItalicSlope;
#endif
  nscoord             mXHeight;
  nscoord             mSuperscriptOffset;
  nscoord             mSubscriptOffset;
  nscoord             mStrikeoutSize;
  nscoord             mStrikeoutOffset;
  nscoord             mUnderlineSize;
  nscoord             mUnderlineOffset;
  HFONT               mFontHandle;

  static PLHashTable* InitializeFamilyNames(void);
  static PLHashTable* gFamilyNames;
  static PLHashTable* gFontWeights;

  static PRUint32* GetCMAP(HDC aDC, const char* aShortName, int* aFontType, PRUint8* aCharset);
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
                     float              aItalicSlope,
                     const PRUnichar*   aString,
                     PRUint32           aLength,
                     nsBoundingMetrics& aBoundingMetrics);
#ifdef NS_DEBUG
  virtual void DumpFontInfo();
#endif // NS_DEBUG
#endif

  int Load(nsFontWinA* aFont);

  BYTE     mCharSet;
  PRUint16 mCodePage;
};

class nsFontWinA : public nsFontWin
{
public:
  nsFontWinA(LOGFONT* aLogFont, HFONT aFont, PRUint32* aMap);
  virtual ~nsFontWinA();
  virtual PRInt32 GetWidth(HDC aDC, const PRUnichar* aString,
                           PRUint32 aLength);
  virtual void DrawString(HDC aDC, PRInt32 aX, PRInt32 aY,
                          const PRUnichar* aString, PRUint32 aLength);
#ifdef MOZ_MATHML
  virtual nsresult
  GetBoundingMetrics(HDC                aDC, 
                     float              aItalicSlope,
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

class nsFontMetricsWinA : public nsFontMetricsWin
{
public:
  virtual nsFontWin* FindLocalFont(HDC aDC, PRUnichar aChar);
  virtual nsFontWin* FindGlobalFont(HDC aDC, PRUnichar aChar);
  virtual nsFontWin* LoadGenericFont(HDC aDC, PRUnichar aChar, char** aName);
  virtual nsFontWin* LoadFont(HDC aDC, nsString* aName);

  virtual nsFontWin* LoadSubstituteFont(HDC aDC, nsString* aName);
  virtual nsFontWin* FindSubstituteFont(HDC aDC, PRUnichar aChar);
};

#endif /* nsFontMetricsWin_h__ */
