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
 * The Original Code is the Mozilla OS/2 libraries.
 *
 * The Initial Developer of the Original Code is
 * John Fairhurst, <john_fairhurst@iname.com>.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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
 * ***** END LICENSE BLOCK *****
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
#include "nsUnicharUtils.h"
#include "nsDrawingSurfaceOS2.h"
#include "nsTHashtable.h"
#include "nsHashKeys.h"

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

// Debug defines
//#define DEBUG_FONT_SELECTION
//#define DEBUG_FONT_STRUCT_ALLOCS

struct nsMiniMetrics
{
  char            szFacename[FACESIZE];
  USHORT          fsType;
  USHORT          fsDefn;
  USHORT          fsSelection;
  nsMiniMetrics*  mNext;
};

// GlobalFontEntry->mStr is an nsString which contains the family name of font.
class GlobalFontEntry : public nsStringHashKey
{
public:
  GlobalFontEntry(KeyTypePointer aStr) : nsStringHashKey(aStr) { }
  GlobalFontEntry(const GlobalFontEntry& aToCopy)
      : nsStringHashKey(aToCopy) { }
  ~GlobalFontEntry()
  {
    nsMiniMetrics* metrics = mMetrics;
    while (metrics) {
      nsMiniMetrics* nextMetrics = metrics->mNext;
      if (metrics)
        delete metrics;
      metrics = nextMetrics;
    }
  }

  // Override, since we want to compare font names as case insensitive
  PRBool KeyEquals(const KeyTypePointer aKey) const
  {
    return GetKeyPointer()->Equals(*aKey, nsCaseInsensitiveStringComparator());
  }
  static PLDHashNumber HashKey(const KeyTypePointer aKey)
  {
    nsAutoString low(*aKey);
    ToLowerCase(low);
    return HashString(low);
  }

  USHORT          mCodePage;
  nsMiniMetrics*  mMetrics;
};

// An nsFontHandle is actually a pointer to one of these.
// It knows how to select itself into a ps.
class nsFontOS2
{
public:
  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  nsFontOS2(void);
  virtual ~nsFontOS2(void);

  inline void SelectIntoPS(HPS hps, long lcid);
  virtual PRInt32 GetWidth(HPS aPS, const char* aString, PRUint32 aLength);
  virtual PRInt32 GetWidth(HPS aPS, const PRUnichar* aString, PRUint32 aLength);
  virtual void DrawString(HPS aPS, nsDrawingSurfaceOS2* aSurface,
                          PRInt32 aX, PRInt32 aY,
                          const char* aString, PRUint32 aLength, INT* aDx0);
  virtual void DrawString(HPS aPS, nsDrawingSurfaceOS2* aSurface,
                          PRInt32 aX, PRInt32 aY,
                          const PRUnichar* aString, PRUint32 aLength);

  FATTRS    mFattrs;
  SIZEF     mCharbox;
  ULONG     mHashMe;
  nscoord   mMaxAscent;
  nscoord   mMaxDescent;
  int       mConvertCodePage;  /* XXX do we need this, or is it just a copy of mFattrs.usCodePage */
#ifdef DEBUG_FONT_STRUCT_ALLOCS
  static unsigned long mRefCount;
#endif
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
  nsFontOS2*          FindUserDefinedFont( HPS aPS );
  nsFontOS2*          FindLocalFont( HPS aPS );
  nsFontOS2*          FindGenericFont( HPS aPS );
  nsFontOS2*          FindPrefFont( HPS aPS );
  nsFontOS2*          FindGlobalFont( HPS aPS );

  nsFontOS2*          LoadFont(HPS aPS, const nsString& aName);
  nsFontOS2*          LoadGenericFont(HPS aPS, const nsString& aName);
  nsFontOS2*          LoadUnicodeFont(HPS aPS, const nsString& aName);
  static nsresult     InitializeGlobalFonts();

  static nsTHashtable<GlobalFontEntry>* gGlobalFonts;
  static PLHashTable*  gFamilyNames;
  static PRBool        gSubstituteVectorFonts;

  nsCOMPtr<nsIAtom>   mLangGroup;
  nsStringArray       mFonts;
  PRUint16            mFontsIndex;
  nsVoidArray         mLoadedFonts;
  nsFontOS2*          mUnicodeFont;
  nsFontOS2*          mWesternFont;

  PRUint16            mGenericIndex;
  nsString            mGeneric;

  nsAutoString        mUserDefined;

  PRBool              mTriedAllGenerics;
  PRBool              mTriedAllPref;
  PRBool              mIsUserDefined;

  int                 mConvertCodePage;

 protected:
  nsresult      RealizeFont(void);
  PRBool        GetVectorSubstitute(HPS aPS, const char* aFacename, char* alias);
  PRBool        GetVectorSubstitute(HPS aPS, const nsString& aFacename, char* alias);
  void          FindUnicodeFont(HPS aPS);
  void          FindWesternFont();
  void          SetFontHandle(HPS aPS, nsFontOS2* aFont);
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

#ifdef DEBUG_FONT_STRUCT_ALLOCS
  static unsigned long mRefCount;
#endif
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
