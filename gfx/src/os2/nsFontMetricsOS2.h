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

#define INCL_WIN
#define INCL_GPI
#include <os2.h>

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
  char      mName[FACESIZE];
};


typedef struct nsGlobalFont
{
  nsString*     name;
  FONTMETRICS   fontMetrics;
  PRUint32*     map;
  PRUint8       skip;
  USHORT        signature;
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

#ifndef XP_OS2
  virtual nsFontOS2* FindGlobalFont(HPS aPS, PRUnichar aChar);
#endif
  virtual nsFontOS2* FindGenericFont(HPS aPS, PRUnichar aChar);
  virtual nsFontOS2* FindLocalFont(HPS aPS, PRUnichar aChar);
  virtual nsFontOS2* FindUserDefinedFont(HPS aPS, PRUnichar aChar);
  nsFontOS2*         FindFont(HPS aPS, PRUnichar aChar);
  virtual nsFontOS2* LoadGenericFont(HPS aPS, PRUnichar aChar, char** aName);
  virtual nsFontOS2* LoadFont(HPS aPS, nsString* aName);

   // for drawing text
   PRUint32 GetDevMaxAscender() const { return mDevMaxAscent; }
   nscoord  GetSpaceWidth( nsIRenderingContext *aRContext);

  static PLHashTable* gFontMaps;
  static nsGlobalFont* gGlobalFonts;
  static int gGlobalFontsCount;

  static nsGlobalFont* InitializeGlobalFonts(HPS aPS);
   int mCodePage;
 
 protected:
   nsresult RealizeFont();
 
   nsFont  *mFont;
   nscoord  mSuperscriptYOffset;
   nscoord  mSubscriptYOffset;
   nscoord  mStrikeoutPosition;
   nscoord  mStrikeoutSize;
   nscoord  mUnderlinePosition;
   nscoord  mUnderlineSize;
   nscoord  mHeight;
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

   PRUint32 mDevMaxAscent;

   nsFontHandleOS2    *mFontHandle;
   nsDeviceContextOS2 *mDeviceContext;

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
protected:
  static PLHashTable* InitializeFamilyNames(void);
  static PLHashTable* gFamilyNames;
  static PLHashTable* gFontWeights;
};

class nsFontEnumeratorOS2 : public nsIFontEnumerator
{
public:
  nsFontEnumeratorOS2();
  NS_DECL_ISUPPORTS
  NS_DECL_NSIFONTENUMERATOR
};

#endif
