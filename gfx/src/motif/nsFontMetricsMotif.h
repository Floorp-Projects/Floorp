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

#ifndef nsFontMetricsMotif_h__
#define nsFontMetricsMotif_h__

#include "nsIFontMetrics.h"
#include "nsFont.h"
#include "nsString.h"
#include "nsUnitConversion.h"
#include "nsIDeviceContext.h"
#include "nsCRT.h"

#include "X11/Xlib.h"

class nsFontMetricsMotif : public nsIFontMetrics
{
public:
  nsFontMetricsMotif();
  virtual ~nsFontMetricsMotif();

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
  NS_IMETHOD  GetLeading(nscoord &aLeading);
  NS_IMETHOD  GetMaxAscent(nscoord &aAscent);
  NS_IMETHOD  GetMaxDescent(nscoord &aDescent);
  NS_IMETHOD  GetMaxAdvance(nscoord &aAdvance);

  NS_IMETHOD  GetFont(const nsFont*& aFont);
  NS_IMETHOD  GetFontHandle(nsFontHandle &aHandle);
  NS_IMETHOD  GetLangGroup(nsIAtom** aLangGroup);

protected:
  void RealizeFont();

protected:
  void QueryFont();
  char *PickAppropriateSize(char **names, XFontStruct *fonts, int cnt, nscoord desired);

  nsFont            *mFont;
  nsIDeviceContext  *mContext;
  nsCOMPtr<nsIAtom> mLangGroup;
  XFontStruct       *mFontInfo;
  Font              mFontHandle;
  nscoord           mCharWidths[256];
  nscoord           mHeight;
  nscoord           mAscent;
  nscoord           mDescent;
  nscoord           mLeading;
  nscoord           mMaxAscent;
  nscoord           mMaxDescent;
  nscoord           mMaxAdvance;
};

#endif // nsFontMetricsMotif_h__
