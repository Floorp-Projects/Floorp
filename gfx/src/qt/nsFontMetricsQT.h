/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#ifndef nsFontMetricsQT_h__
#define nsFontMetricsQT_h__

#include "nsIFontMetrics.h"
#include "nsFont.h"
#include "nsString.h"
#include "nsUnitConversion.h"
#include "nsIDeviceContext.h"
#include "nsCRT.h"
#include "nsDeviceContextQT.h"

#include <qfont.h>

class nsFontMetricsQT : public nsIFontMetrics
{
public:
    nsFontMetricsQT();
    virtual ~nsFontMetricsQT();

    NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

    NS_DECL_ISUPPORTS

    NS_IMETHOD  Init(const nsFont& aFont, nsIDeviceContext* aContext);
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
    NS_IMETHOD  GetFont(const nsFont *&aFont);
    NS_IMETHOD  GetFontHandle(nsFontHandle &aHandle);

protected:
#if 0
    char *PickAppropriateSize(char **names, 
                              XFontStruct *fonts, 
                              int cnt, 
                              nscoord desired);
#endif
    void RealizeFont();

    nsIDeviceContext * mDeviceContext;
    nsFont           * mFont;
    QFont            * mFontHandle;

    nscoord            mHeight;
    nscoord            mAscent;
    nscoord            mDescent;
    nscoord            mLeading;
    nscoord            mMaxAscent;
    nscoord            mMaxDescent;
    nscoord            mMaxAdvance;
    nscoord            mXHeight;
    nscoord            mSuperscriptOffset;
    nscoord            mSubscriptOffset;
    nscoord            mStrikeoutSize;
    nscoord            mStrikeoutOffset;
    nscoord            mUnderlineSize;
    nscoord            mUnderlineOffset;
};

#endif
