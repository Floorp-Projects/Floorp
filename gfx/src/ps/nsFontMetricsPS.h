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

#ifndef nsFontMetricsPS_h__
#define nsFontMetricsPS_h__

#include "nsIFontMetrics.h"
#include "nsAFMObject.h"
#include "nsFont.h"
#include "nsString.h"
#include "nsIAtom.h"
#include "nsUnitConversion.h"
#include "nsIDeviceContext.h"
#include "nsCOMPtr.h"
#include "nsCRT.h"
#include "nsCompressedCharMap.h"
#include "nsPostScriptObj.h"

class nsDeviceContextPS;
class nsRenderingContextPS;
class nsFontPS;

class nsFontMetricsPS : public nsIFontMetrics
{
public:
  nsFontMetricsPS();
  virtual ~nsFontMetricsPS();

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
  NS_IMETHOD  GetSpaceWidth(nscoord& aAveCharWidth);
  NS_IMETHOD  GetFont(const nsFont *&aFont);
  NS_IMETHOD  GetLangGroup(nsIAtom** aLangGroup);
  NS_IMETHOD  GetFontHandle(nsFontHandle &aHandle);
  NS_IMETHOD  GetStringWidth(const char *String,nscoord &aWidth,nscoord aLength);
  NS_IMETHOD  GetStringWidth(const PRUnichar *aString,nscoord &aWidth,nscoord aLength);
  
  inline void SetXHeight(nscoord aXHeight) { mXHeight = aXHeight; };
  inline void SetSuperscriptOffset(nscoord aSuperscriptOffset) { mSuperscriptOffset = aSuperscriptOffset; };
  inline void SetSubscriptOffset(nscoord aSubscriptOffset) { mSubscriptOffset = aSubscriptOffset; };
  inline void SetStrikeout(nscoord aOffset, nscoord aSize) { mStrikeoutOffset = aOffset; mStrikeoutSize = aSize; };
  inline void SetUnderline(nscoord aOffset, nscoord aSize) { mUnderlineOffset = aOffset; mUnderlineSize = aSize; };
  inline void SetHeight(nscoord aHeight) { mHeight = aHeight; };
  inline void SetLeading(nscoord aLeading) { mLeading = aLeading; };
  inline void SetAscent(nscoord aAscent) { mAscent = aAscent; };
  inline void SetDescent(nscoord aDescent) { mDescent = aDescent; };
  inline void SetEmHeight(nscoord aEmHeight) { mEmHeight = aEmHeight; };
  inline void SetEmAscent(nscoord aEmAscent) { mEmAscent = aEmAscent; };
  inline void SetEmDescent(nscoord aEmDescent) { mEmDescent = aEmDescent; };
  inline void SetMaxHeight(nscoord aMaxHeight) { mMaxHeight = aMaxHeight; };
  inline void SetMaxAscent(nscoord aMaxAscent) { mMaxAscent = aMaxAscent; };
  inline void SetMaxDescent(nscoord aMaxDescent) { mMaxDescent = aMaxDescent; };
  inline void SetMaxAdvance(nscoord aMaxAdvance) { mMaxAdvance = aMaxAdvance; };
  inline void SetAveCharWidth(nscoord aAveCharWidth) { mAveCharWidth = aAveCharWidth; };
  inline void SetSpaceWidth(nscoord aSpaceWidth) { mSpaceWidth = aSpaceWidth; };

  inline nsFontPS* GetFontPS() { return mFontPS; }
  
#if defined(XP_WIN)
// this routine is defined here so the PostScript module can be debugged
// on the windows platform

  /**
   * Returns the average character width
   */
  NS_IMETHOD  GetAveCharWidth(nscoord& aAveCharWidth){return NS_OK;}
#endif


private:
  nsCOMPtr<nsIAtom> mLangGroup;

protected:
  void RealizeFont();

  nsDeviceContextPS   *mDeviceContext;
  nsFont              *mFont;
  nscoord             mHeight;
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
  nscoord             mAveCharWidth;

  nsFontPS*           mFontPS;
};

class nsFontPS
{
public:
  nsFontPS();
  nsFontPS(const nsFont& aFont, nsIFontMetrics* aFontMetrics);
  virtual ~nsFontPS();
  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  static nsFontPS* FindFont(const nsFont& aFont, nsIFontMetrics* aFontMetrics);

  inline PRInt32 SupportsChar(PRUnichar aChar)
    { return mCCMap && CCMAP_HAS_CHAR(mCCMap, aChar); };

  inline PRInt16 GetFontIndex() { return mFontIndex; };
  inline const nsString& GetFamilyName() { return mFamilyName; };

  virtual nscoord GetWidth(const char* aString, PRUint32 aLength) = 0;
  virtual nscoord GetWidth(const PRUnichar* aString, PRUint32 aLength) = 0;
  virtual nscoord DrawString(nsRenderingContextPS* aContext,
                             nscoord aX, nscoord aY,
                             const char* aString, PRUint32 aLength) = 0;
  virtual nscoord DrawString(nsRenderingContextPS* aContext,
                             nscoord aX, nscoord aY,
                             const PRUnichar* aString, PRUint32 aLength) = 0;
  virtual nsresult RealizeFont(nsFontMetricsPS* aFontMetrics, float dev2app) = 0;

#ifdef MOZ_MATHML
  virtual nsresult
  GetBoundingMetrics(const char*        aString,
                     PRUint32           aLength,
                     nsBoundingMetrics& aBoundingMetrics) = 0;
  virtual nsresult
  GetBoundingMetrics(const PRUnichar*   aString,
                     PRUint32           aLength,
                     nsBoundingMetrics& aBoundingMetrics) = 0;
#endif

protected:
  nsFont*                  mFont;
  PRUint16*                mCCMap;
  nsCOMPtr<nsIFontMetrics> mFontMetrics;
  PRInt16                  mFontIndex;
  nsString                 mFamilyName;
};

class nsFontPSAFM : public nsFontPS
{
public:
  nsFontPSAFM(const nsFont& aFont, nsIFontMetrics* aFontMetrics);
  virtual ~nsFontPSAFM();
  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  nscoord GetWidth(const char* aString, PRUint32 aLength);
  nscoord GetWidth(const PRUnichar* aString, PRUint32 aLength);
  nscoord DrawString(nsRenderingContextPS* aContext,
                     nscoord aX, nscoord aY,
                     const char* aString, PRUint32 aLength);
  nscoord DrawString(nsRenderingContextPS* aContext,
                     nscoord aX, nscoord aY,
                     const PRUnichar* aString, PRUint32 aLength);
  nsresult RealizeFont(nsFontMetricsPS* aFontMetrics, float dev2app);

#ifdef MOZ_MATHML
  nsresult
  GetBoundingMetrics(const char*        aString,
                     PRUint32           aLength,
                     nsBoundingMetrics& aBoundingMetrics);
  nsresult
  GetBoundingMetrics(const PRUnichar*   aString,
                     PRUint32           aLength,
                     nsBoundingMetrics& aBoundingMetrics);
#endif

  nsAFMObject* mAFMInfo;
};

#endif
