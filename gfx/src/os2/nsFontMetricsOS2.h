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
#include "nsDrawingSurfaceOS2.h"
#include "nsICollation.h"

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


// An nsFontHandle is actually a pointer to one of these.
// It knows how to select itself into a ps.
class nsFontOS2
{
 public:
            nsFontOS2(void);
  void      SelectIntoPS( HPS hps, long lcid );
  PRInt32   GetWidth( HPS aPS, const PRUnichar* aString,
                      PRUint32 aLength );
  void      DrawString( HPS aPS, nsDrawingSurfaceOS2* aSurface,
                        PRInt32 aX, PRInt32 aY,
                        const PRUnichar* aString, PRUint32 aLength );

  FATTRS    mFattrs;
  SIZEF     mCharbox;
  ULONG     mHashMe;
  nscoord   mMaxAscent;
  nscoord   mMaxDescent;
  int       mConvertCodePage;
};

struct nsMiniFontMetrics
{
  char    szFamilyname[FACESIZE];
  char    szFacename[FACESIZE];
  USHORT  fsType;
  USHORT  fsDefn;
  USHORT  fsSelection;
};

class nsGlobalFont
{
 public:
      nsGlobalFont(void)  { key = nsnull; };
     ~nsGlobalFont(void)  { if(key) nsMemory::Free(key); };
     
  nsAutoString        name;
  nsMiniFontMetrics   metrics;
  int                 nextFamily;
  PRUint8*            key;
  PRUint32            len;
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
  nsFontOS2* mFont;
};

typedef PRBool (*PR_CALLBACK nsFontSwitchCallback)
               (const nsFontSwitch* aFontSwitch,
                const PRUnichar*    aSubstring,
                PRUint32            aSubstringLength,
                void*               aData);


class nsFontMetricsOS2 : public nsIFontMetrics
{
 public:
  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW
  NS_DECL_ISUPPORTS

  nsFontMetricsOS2();
  virtual ~nsFontMetricsOS2();

  NS_IMETHOD Init( const nsFont& aFont, nsIAtom* aLangGroup, nsIDeviceContext *aContext);
  NS_IMETHOD Destroy();

  // Metrics
  NS_IMETHOD  GetXHeight( nscoord &aResult);
  NS_IMETHOD  GetSuperscriptOffset( nscoord &aResult);
  NS_IMETHOD  GetSubscriptOffset( nscoord &aResult);
  NS_IMETHOD  GetStrikeout( nscoord &aOffset, nscoord &aSize);
  NS_IMETHOD  GetUnderline( nscoord &aOffset, nscoord &aSize);

  NS_IMETHOD  GetHeight( nscoord &aHeight);
#ifdef FONT_LEADING_APIS_V2
  NS_IMETHOD  GetInternalLeading(nscoord &aLeading);
  NS_IMETHOD  GetExternalLeading(nscoord &aLeading);
#else
  NS_IMETHOD  GetLeading(nscoord &aLeading);
  NS_IMETHOD  GetNormalLineHeight(nscoord &aHeight);
#endif //FONT_LEADING_APIS_V2
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

  virtual nsresult
  ResolveForwards(HPS                  aPS,
                  const PRUnichar*     aString,
                  PRUint32             aLength,
                  nsFontSwitchCallback aFunc, 
                  void*                aData);

  virtual nsresult
  ResolveBackwards(HPS                  aPS,
                   const PRUnichar*     aString,
                   PRUint32             aLength,
                   nsFontSwitchCallback aFunc, 
                   void*                aData);

  nsFontOS2*          FindFont( HPS aPS );
  nsFontOS2*          FindGlobalFont( HPS aPS );
  nsFontOS2*          FindGenericFont( HPS aPS );
  nsFontOS2*          FindPrefFont( HPS aPS );
  nsFontOS2*          FindLocalFont( HPS aPS );
  nsFontOS2*          FindUserDefinedFont( HPS aPS );
  nsFontOS2*          LoadFont (HPS aPS, nsString* aName );
  static nsresult     InitializeGlobalFonts();

  static nsVoidArray*  gGlobalFonts;
  static PLHashTable*  gFamilyNames;
  static nsICollation* gCollation;
  
  nsStringArray       mFonts;
  PRUint16            mFontsIndex;
  nsVoidArray         mFontIsGeneric;

  nsString            mGeneric;
  nsCOMPtr<nsIAtom>   mLangGroup;
  nsAutoString        mUserDefined;

  PRUint8             mTriedAllGenerics;
  PRUint8             mIsUserDefined;

  int                 mConvertCodePage;


 protected:
  nsresult      RealizeFont(void);
  PRBool        GetVectorSubstitute( HPS aPS, const char* aFacename, char* alias );
  PRBool        GetVectorSubstitute( HPS aPS, nsString* aFacename, char* alias );
  nsFontOS2*    GetUnicodeFont( HPS aPS );
  void          SetFontHandle( HPS aPS, nsFontOS2* aFH );
  PLHashTable*  InitializeFamilyNames(void);


  nsFont   mFont;
  nscoord  mSuperscriptYOffset;
  nscoord  mSubscriptYOffset;
  nscoord  mStrikeoutPosition;
  nscoord  mStrikeoutSize;
  nscoord  mUnderlinePosition;
  nscoord  mUnderlineSize;
  nscoord  mExternalLeading;
  nscoord  mInternalLeading;
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

  nsFontOS2          *mFontHandle;
  nsDeviceContextOS2 *mDeviceContext;

  static PRBool       gSubstituteVectorFonts;
  static int          gCachedIndex;
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
