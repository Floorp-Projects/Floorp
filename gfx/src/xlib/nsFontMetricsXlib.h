/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *   Tony Tsui <tony@igelaus.com.au>
 *   Tim Copperfield <timecop@network.email.ne.jp>
 *   Roland Mainz <roland.mainz@informatik.med.uni-giessen.de> 
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

#ifndef nsFontMetricsXlib_h__
#define nsFontMetricsXlib_h__

#include "nsICharRepresentable.h"
#include "nsCompressedCharMap.h"
#include "nsIDeviceContext.h"
#include "nsIFontMetrics.h"
#include "nsIFontEnumerator.h"

#include "nsCRT.h"
#include "nsCOMPtr.h"
#include "nsDrawingSurfaceXlib.h"
#ifdef USE_XPRINT
#include "nsDeviceContextXP.h"
#endif /* USE_XPRINT */
#include "nsFont.h"
#include "nsRenderingContextXlib.h"
#include "nsString.h"
#include "nsUnitConversion.h"
#include "xlibrgb.h"

#undef FONT_HAS_GLYPH
#define FONT_HAS_GLYPH(map, char) IS_REPRESENTABLE(map, char)

typedef struct nsFontCharSetXlibInfo nsFontCharSetXlibInfo;

typedef int (*nsFontCharSetXlibConverter)(nsFontCharSetXlibInfo* aSelf,
             XFontStruct* aFont, const PRUnichar* aSrcBuf, PRInt32 aSrcLen, 
             char* aDestBuf, PRInt32 aDestLen);

struct nsFontCharSetXlib;
struct nsFontFamilyXlib;
struct nsFontNodeXlib;
struct nsFontStretchXlib;

class nsFontXlibUserDefined;
class nsFontMetricsXlib;

typedef XFontStruct *XFontStructPtr;

class nsFontXlib
{
public:
  nsFontXlib();
  virtual ~nsFontXlib();
  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW
  
  operator const XFontStructPtr() { return (const XFontStructPtr)mFont; }

  void LoadFont(void);
  PRBool IsEmptyFont(XFontStruct*);

  inline int SupportsChar(PRUnichar aChar)
    { return mFont && CCMAP_HAS_CHAR(mCCMap, aChar); };
    
  virtual PRBool GetXlibFontIs10646(void);
  virtual int GetWidth(const PRUnichar* aString, PRUint32 aLength) = 0;
  virtual int DrawString(nsRenderingContextXlib* aContext,
                         nsIDrawingSurfaceXlib* aSurface,
                         nscoord aX, nscoord aY,
                         const PRUnichar* aString, PRUint32 aLength) = 0;

#ifdef MOZ_MATHML
  // bounding metrics for a string 
  // remember returned values are not in app units 
  // - to emulate GetWidth () above
  virtual nsresult
  GetBoundingMetrics(const PRUnichar*   aString,
                     PRUint32           aLength,
                     nsBoundingMetrics& aBoundingMetrics) = 0;
#endif /* MOZ_MATHML */

  XFontStruct           *mFont;
  PRUint16              *mCCMap;
  nsFontCharSetXlibInfo *mCharSetInfo;
  char                  *mName;
  nsFontXlibUserDefined *mUserDefinedFont;
  PRUint16               mSize;
  PRInt16                mBaselineAdjust;
  PRBool                 mAlreadyCalledLoadFont;

  // these values are not in app units, they need to be scaled with 
  // nsIDeviceContext::GetDevUnitsToAppUnits()
  PRInt16                mMaxAscent;
  PRInt16                mMaxDescent;
};

class nsFontMetricsXlib : public nsIFontMetrics
{
public:
  nsFontMetricsXlib();
  virtual ~nsFontMetricsXlib();

#ifdef USE_XPRINT
  static void     EnablePrinterMode(PRBool printermode);
#endif /* USE_XPRINT */

  static nsresult InitGlobals(nsIDeviceContext *);
  static void     FreeGlobals(void);
  
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
  NS_IMETHOD  GetFont(const nsFont *&aFont);
  NS_IMETHOD  GetLangGroup(nsIAtom** aLangGroup);
  NS_IMETHOD  GetFontHandle(nsFontHandle &aHandle);
  
  virtual nsresult GetSpaceWidth(nscoord &aSpaceWidth);

  nsFontXlib* FindFont(PRUnichar aChar);
  nsFontXlib* FindUserDefinedFont(PRUnichar aChar);
  nsFontXlib* FindStyleSheetSpecificFont(PRUnichar aChar);
  nsFontXlib* FindStyleSheetGenericFont(PRUnichar aChar);
  nsFontXlib* FindLangGroupPrefFont(nsIAtom* aLangGroup, PRUnichar aChar);
  nsFontXlib* FindLangGroupFont(nsIAtom* aLangGroup, PRUnichar aChar, nsCString* aName);
  nsFontXlib* FindAnyFont(PRUnichar aChar);
  nsFontXlib* FindSubstituteFont(PRUnichar aChar);

  nsFontXlib* SearchNode(nsFontNodeXlib* aNode, PRUnichar aChar);
  nsFontXlib* TryAliases(nsCString* aName, PRUnichar aChar);
  nsFontXlib* TryFamily(nsCString* aName, PRUnichar aChar);
  nsFontXlib* TryNode(nsCString* aName, PRUnichar aChar);
  nsFontXlib* TryNodes(nsAWritableCString &aFFREName, PRUnichar aChar);
  nsFontXlib* TryLangGroup(nsIAtom* aLangGroup, nsCString* aName, PRUnichar aChar);

  nsFontXlib* AddToLoadedFontsList(nsFontXlib* aFont);
  nsFontXlib* PickASizeAndLoad(nsFontStretchXlib* aStretch,
                               nsFontCharSetXlibInfo* aCharSet,
                               PRUnichar aChar);

  static nsresult FamilyExists(nsIDeviceContext*, const nsString& aFontName);

  nsCAutoString      mDefaultFont;
  nsCString          *mGeneric;
  PRUint8            mIsUserDefined;
  nsCOMPtr<nsIAtom>  mLangGroup;
  nsCAutoString      mUserDefined;

  nsCStringArray     mFonts;
  PRUint16           mFontsAlloc;
  PRUint16           mFontsCount;
  PRUint16           mFontsIndex;
  nsAutoVoidArray    mFontIsGeneric;

  nsFontXlib         **mLoadedFonts;
  PRUint16           mLoadedFontsAlloc;
  PRUint16           mLoadedFontsCount;

  PRUint8            mTriedAllGenerics;
  nsFontXlib         *mSubstituteFont;

   
protected:
  void RealizeFont();

  nsIDeviceContext    *mDeviceContext;

  nsFont              *mFont;
  XFontStruct         *mFontHandle;
  XFontStruct         *mFontStruct;
  nsFontXlib          *mWesternFont;

  nscoord             mAscent;
  nscoord             mDescent;
  
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

  PRUint16            mPixelSize;
  PRUint8             mStretchIndex;
  PRUint8             mStyleIndex;
  nsFontCharSetXlibConverter mDocConverterType;
#ifdef USE_XPRINT
public:  
  static PRPackedBool mPrinterMode;
#endif /* USE_XPRINT */  
};

class nsFontEnumeratorXlib : public nsIFontEnumerator
{
public:
  nsFontEnumeratorXlib();
  NS_DECL_ISUPPORTS
  NS_DECL_NSIFONTENUMERATOR
};

#endif /* !nsFontMetricsXlib_h__ */


