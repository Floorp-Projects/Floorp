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
 */

#ifndef nsFontMetricsMac_h__
#define nsFontMetricsMac_h__

#include <TextEdit.h>	// for TextStyle

#include "nsIFontMetrics.h"
#include "nsFont.h"
#include "nsString.h"
#include "nsUnitConversion.h"
#include "nsIDeviceContext.h"
#include "nsCRT.h"
#include "nsIAtom.h"
#include "nsCOMPtr.h"
class nsUnicodeFontMappingMac;

class nsFontMetricsMac : public nsIFontMetrics
{
public:
  nsFontMetricsMac();
  virtual ~nsFontMetricsMac();

  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  NS_DECL_ISUPPORTS

  NS_IMETHOD  Init(const nsFont& aFont, nsIAtom* aLangGroup, nsIDeviceContext* aContext);
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
  NS_IMETHOD  GetWidths(const nscoord *&aWidths);
  NS_IMETHOD  GetFont(const nsFont *&aFont);
  NS_IMETHOD  GetLangGroup(nsIAtom** aLangGroup);
  NS_IMETHOD  GetFontHandle(nsFontHandle& aHandle);
  virtual nsresult  GetSpaceWidth(nscoord &aSpaceWidth);
	// fill a native TextStyle record with the font, size and style (not color)
	static void 	GetNativeTextStyle(nsIFontMetrics& inMetrics,
																		const nsIDeviceContext& inDevContext, TextStyle &outStyle);


  nsUnicodeFontMappingMac* GetUnicodeFontMapping();
	
protected:
	void				RealizeFont();

	short							mFontNum;
  nsUnicodeFontMappingMac *mFontMapping;
  nscoord           mEmHeight;
  nscoord           mEmAscent;
  nscoord           mEmDescent;
  nscoord           mMaxHeight;
  nscoord           mLeading;
  nscoord           mMaxAscent;
  nscoord           mMaxDescent;
  nscoord           mMaxAdvance;
  nscoord						mSpaceWidth;
  nsCOMPtr<nsIAtom> mLangGroup;
  nsFont            *mFont;
  nsIDeviceContext  *mContext;
};

#endif




