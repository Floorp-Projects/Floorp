/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is mozilla.org.
 * Portions created by the Initial Developer are
 * Copyright (C) 2004 the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *    Stuart Parmenter <pavlov@netscape.com>
 *    Joe Hewitt <hewitt@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 */

#ifndef NSCAIROFONTMETRICS__H__
#define NSCAIROFONTMETRICS__H__

#include "nsIFontMetrics.h"
#include "nsCOMPtr.h"
#include "nsIDeviceContext.h"
#include "nsIAtom.h"

#include <ft2build.h>
#include FT_FREETYPE_H

class nsCairoFontMetrics : public nsIFontMetrics
{
public:
    nsCairoFontMetrics();
    virtual ~nsCairoFontMetrics();

    NS_DECL_ISUPPORTS

    NS_IMETHOD  Init(const nsFont& aFont, nsIAtom* aLangGroup,
                     nsIDeviceContext *aContext);
    NS_IMETHOD  Destroy();
    NS_IMETHOD  GetXHeight(nscoord& aResult);
    NS_IMETHOD  GetSuperscriptOffset(nscoord& aResult);
    NS_IMETHOD  GetSubscriptOffset(nscoord& aResult);
    NS_IMETHOD  GetStrikeout(nscoord& aOffset, nscoord& aSize);
    NS_IMETHOD  GetUnderline(nscoord& aOffset, nscoord& aSize);
    NS_IMETHOD  GetHeight(nscoord &aHeight);
    NS_IMETHOD  GetInternalLeading(nscoord &aLeading);
    NS_IMETHOD  GetExternalLeading(nscoord &aLeading);
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
    NS_IMETHOD  GetAveCharWidth(nscoord& aAveCharWidth);
    NS_IMETHOD  GetSpaceWidth(nscoord& aSpaceCharWidth);
    NS_IMETHOD  GetLeading(nscoord& aLeading);
    NS_IMETHOD  GetNormalLineHeight(nscoord& aLineHeight);

/* local methods */
    nscoord MeasureString(const char *aString, PRUint32 aLength);
    nscoord MeasureString(const PRUnichar *aString, PRUint32 aLength);

private:
    FT_Face mFace;
    nsFont *mFont;
    nsCOMPtr<nsIDeviceContext> mDeviceContext;
    nsCOMPtr<nsIAtom> mLangGroup;
    float mDev2App;

    long mMaxAscent;
    long mMaxDescent;
    short mMaxAdvance;

    long mUnderlineOffset;
    long mUnderlineHeight;
};

#endif /* NSCAIROFONTMETRICS__H__ */
