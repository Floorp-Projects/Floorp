/*
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is the Mozilla OS/2 libraries.
 *
 * The Initial Developer of the Original Code is John Fairhurst,
 * <john_fairhurst@iname.com>.  Portions created by John Fairhurst are
 * Copyright (C) 1999 John Fairhurst. All Rights Reserved.
 *
 * This Original Code has been modified by IBM Corporation.
 * Modifications made by IBM described herein are
 * Copyright (c) International Business Machines
 * Corporation, 2000
 *
 * Modifications to Mozilla code or documentation
 * identified per MPL Section 3.3
 *
 * Date           Modified by     Description of modification
 * 03/28/2000   IBM Corp.        Changes to make os2.h file similar to windows.h file
 */

#ifndef _nsFontMetricsOS2_h
#define _nsFontMetricsOS2_h

#include "nsGfxDefs.h"

#include "plhash.h"
#include "nsIFontMetrics.h"
#include "nsIFontEnumerator.h"
#include "nsFont.h"
#include "nsString.h"
#include "nsUnitConversion.h"
#include "nsIDeviceContext.h"
#include "nsCRT.h"
#include "nsDeviceContextOS2.h"
#include "nsCOMPtr.h"
#include "nsVoidArray.h"

// An nsFontHandle is actually a pointer to one of these.
// It knows how to select itself into a ps.
struct nsFontHandleOS2
{
   FATTRS fattrs;
   SIZEF  charbox;

   nsFontHandleOS2();
   void SelectIntoPS( HPS hps, long lcid);
   ULONG ulHashMe;
};

#ifndef FM_DEFN_LATIN1
#define FM_DEFN_LATIN1          0x0010   /* Base latin character set     */
#define FM_DEFN_PC              0x0020   /* PC characters                */
#define FM_DEFN_LATIN2          0x0040   /* Extended latin character set */
#define FM_DEFN_CYRILLIC        0x0080   /* Cyrillic character set       */
#define FM_DEFN_HEBREW          0x0100   /* Base Hebrew characters       */
#define FM_DEFN_GREEK           0x0200   /* Base Greek characters        */
#define FM_DEFN_ARABIC          0x0400   /* Base Arabic characters       */
#define FM_DEFN_UGLEXT          0x0800   /* Additional UGL chars         */
#define FM_DEFN_KANA            0x1000   /* Katakana and hiragana chars  */
#define FM_DEFN_THAI            0x2000   /* Thai characters              */

#define FM_DEFN_UGL383          0x0070   /* Chars in OS/2 2.1            */
#define FM_DEFN_UGL504          0x00F0   /* Chars in OS/2 Warp 4         */
#define FM_DEFN_UGL767          0x0FF0   /* Chars in ATM fonts           */
#define FM_DEFN_UGL1105         0x3FF0   /* Chars in bitmap fonts        */
#endif

class nsFontOS2
{
public:
//  nsFontWin(FATTRS* aFattrs, nsFontHandleOS2* aFont, PRUint32* aMap);
//  virtual ~nsFontWin();
  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

//  virtual PRInt32 GetWidth(HPS aPS, const PRUnichar* aString,
//                           PRUint32 aLength) = 0;
  // XXX return width from DrawString
//  virtual void DrawString(HDC aDC, PRInt32 aX, PRInt32 aY,
//                          const PRUnichar* aString, PRUint32 aLength) = 0;
#ifdef MOZ_MATHML
  virtual nsresult
  GetBoundingMetrics(HDC                aDC,
                     const PRUnichar*   aString,
                     PRUint32           aLength,
                     nsBoundingMetrics& aBoundingMetrics) = 0;
#endif

  char      mName[FACESIZE];
  char      mFamilyName[FACESIZE];
  nsFontHandleOS2* mFont;
#ifdef MOZ_MATHML
  nsCharacterMap* mCMAP;
#endif
};


typedef struct nsGlobalFont
{
  nsString*     name;
  FONTMETRICS   fontMetrics;
#ifdef OLDCODE
  PRUint32*     map;
  PRUint8       skip;
#endif
  USHORT        signature;
  int           nextFamily;
} nsGlobalFont;

class nsFontMetricsOS2 : public nsIFontMetrics
{
 public:
   nsFontMetricsOS2();
   virtual ~nsFontMetricsOS2();

   NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

   NS_DECL_ISUPPORTS

   NS_IMETHOD Init( const nsFont& aFont, nsIAtom* aLangGroup, nsIDeviceContext *aContext);
   NS_IMETHOD Destroy();

   // Metrics
   NS_IMETHOD  GetXHeight( nscoord &aResult);
   NS_IMETHOD  GetSuperscriptOffset( nscoord &aResult);
   NS_IMETHOD  GetSubscriptOffset( nscoord &aResult);
   NS_IMETHOD  GetStrikeout( nscoord &aOffset, nscoord &aSize);
   NS_IMETHOD  GetUnderline( nscoord &aOffset, nscoord &aSize);

   NS_IMETHOD  GetHeight( nscoord &aHeight);
   NS_IMETHOD  GetNormalLineHeight(nscoord &aHeight);
   NS_IMETHOD  GetLeading( nscoord &aLeading);
   NS_IMETHOD  GetEmHeight(nscoord &aHeight);
   NS_IMETHOD  GetEmAscent(nscoord &aAscent);
   NS_IMETHOD  GetEmDescent(nscoord &aDescent);
   NS_IMETHOD  GetMaxHeight(nscoord &aHeight);
   NS_IMETHOD  GetMaxAscent( nscoord &aAscent);
   NS_IMETHOD  GetMaxDescent( nscoord &aDescent);
   NS_IMETHOD  GetMaxAdvance( nscoord &aAdvance);
   NS_IMETHOD  GetFont( const nsFont *&aFont);
   NS_IMETHOD  GetLangGroup(nsIAtom** aLangGroup);
   NS_IMETHOD  GetFontHandle( nsFontHandle &aHandle);
  NS_IMETHOD  GetAveCharWidth(nscoord &aAveCharWidth);

  NS_IMETHOD  GetSpaceWidth(nscoord &aSpaceWidth);

  NS_IMETHODIMP SetUnicodeFont( HPS aPS, LONG lcid );

  virtual FATTRS* FindGlobalFont(HPS aPS, BOOL bBold, BOOL bItalic);
  virtual FATTRS* FindGenericFont(HPS aPS, BOOL bBold, BOOL bItalic);
  virtual FATTRS* FindLocalFont(HPS aPS, BOOL bBold, BOOL bItalic);
  virtual FATTRS* FindUserDefinedFont(HPS aPS, BOOL bBold, BOOL bItalic);
  FATTRS*         FindFont(HPS aPS, BOOL bBold, BOOL bItalic);
  virtual FATTRS* LoadGenericFont(HPS aPS, BOOL bBold, BOOL bItalic, char** aName);
  virtual FATTRS* LoadFont(HPS aPS, nsString* aName, BOOL bBold, BOOL bItalic);

  static nsGlobalFont* gGlobalFonts;
  static int gGlobalFontsCount;

  static nsGlobalFont* InitializeGlobalFonts();
  int mCodePage;

 protected:
  nsresult RealizeFont();
  PRBool GetVectorSubstitute( HPS aPS, const char* aFacename, PRBool aIsBold,
                             PRBool aIsItalic, char* alias );

  nsFont   *mFont;
  nscoord  mSuperscriptYOffset;
  nscoord  mSubscriptYOffset;
  nscoord  mStrikeoutPosition;
  nscoord  mStrikeoutSize;
  nscoord  mUnderlinePosition;
  nscoord  mUnderlineSize;
  nscoord  mLeading;
  nscoord  mEmHeight;
  nscoord  mEmAscent;
  nscoord  mEmDescent;
  nscoord  mMaxHeight;
  nscoord  mMaxAscent;
  nscoord  mMaxDescent;
  nscoord  mMaxAdvance;
  nscoord  mSpaceWidth;
  nscoord  mXHeight;
  nscoord  mAveCharWidth;

  nsFontHandleOS2    *mFontHandle;
  nsDeviceContextOS2 *mDeviceContext;

  static PRBool       gSubstituteVectorFonts;

public:
  nsStringArray       mFonts;
  PRUint16            mFontsIndex;
  nsVoidArray         mFontIsGeneric;

  nsAutoString        mDefaultFont;
  nsString            *mGeneric;
  nsCOMPtr<nsIAtom>   mLangGroup;
  nsAutoString        mUserDefined;

  PRUint8 mTriedAllGenerics;
  PRUint8 mIsUserDefined;
  static nscoord      gDPI;
  static long         gSystemRes;
protected:
  static PLHashTable* InitializeFamilyNames(void);
  static PLHashTable* gFamilyNames;
};

class nsFontEnumeratorOS2 : public nsIFontEnumerator
{
public:
  nsFontEnumeratorOS2();
  NS_DECL_ISUPPORTS
  NS_DECL_NSIFONTENUMERATOR

protected:
};

#endif
