/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code Christopher Blizzard
 * <blizzard@mozilla.org>.  Portions created by the Initial Developer
 * are Copyright (C) 2002 the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsIFontMetrics.h"
#include "nsIFontEnumerator.h"
#include "nsCRT.h"
#include "nsIAtom.h"
#include "nsString.h"
#include "nsVoidArray.h"
#include "nsIFontMetricsGTK.h"

#include <pango/pango.h>

class nsFontMetricsPango : public nsIFontMetricsGTK
{
public:
    nsFontMetricsPango();
    virtual ~nsFontMetricsPango();

    NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

    // nsISupports
    NS_DECL_ISUPPORTS

    // nsIFontMetrics
    NS_IMETHOD  Init                 (const nsFont& aFont, nsIAtom* aLangGroup,
                                      nsIDeviceContext *aContext);
    NS_IMETHOD  Destroy();
    NS_IMETHOD  GetFont              (const nsFont *&aFont);
    NS_IMETHOD  GetLangGroup         (nsIAtom** aLangGroup);
    NS_IMETHOD  GetFontHandle        (nsFontHandle &aHandle);

    NS_IMETHOD  GetXHeight           (nscoord& aResult)
                                     { aResult = mXHeight; return NS_OK; };

    NS_IMETHOD GetSuperscriptOffset  (nscoord& aResult)
                                     { aResult = mSuperscriptOffset;
                                       return NS_OK; };

    NS_IMETHOD GetSubscriptOffset    (nscoord& aResult)
                                     { aResult = mSubscriptOffset;
                                       return NS_OK; };
                              
    NS_IMETHOD GetStrikeout          (nscoord& aOffset, nscoord& aSize)
                                     { aOffset = mStrikeoutOffset;
                                       aSize = mStrikeoutSize; 
                                       return NS_OK; };

    NS_IMETHOD GetUnderline          (nscoord& aOffset, nscoord& aSize)
                                     { aOffset = mUnderlineOffset;
                                       aSize = mUnderlineSize; 
                                       return NS_OK; };

    NS_IMETHOD GetHeight             (nscoord &aHeight)
                                     { aHeight = mMaxHeight; 
                                       return NS_OK; };

    NS_IMETHOD GetNormalLineHeight   (nscoord &aHeight)
                                     { aHeight = mEmHeight + mLeading;
                                       return NS_OK; };

    NS_IMETHOD GetLeading            (nscoord &aLeading)
                                     { aLeading = mLeading; 
                                       return NS_OK; };

    NS_IMETHOD GetEmHeight           (nscoord &aHeight)
                                     { aHeight = mEmHeight; 
                                       return NS_OK; };

    NS_IMETHOD GetEmAscent           (nscoord &aAscent)
                                     { aAscent = mEmAscent;
                                       return NS_OK; };

    NS_IMETHOD GetEmDescent          (nscoord &aDescent)
                                     { aDescent = mEmDescent;
                                       return NS_OK; };

    NS_IMETHOD GetMaxHeight          (nscoord &aHeight)
                                     { aHeight = mMaxHeight;
                                       return NS_OK; };

    NS_IMETHOD GetMaxAscent          (nscoord &aAscent)
                                     { aAscent = mMaxAscent;
                                       return NS_OK; };

    NS_IMETHOD GetMaxDescent         (nscoord &aDescent)
                                     { aDescent = mMaxDescent;
                                       return NS_OK; };

    NS_IMETHOD GetMaxAdvance         (nscoord &aAdvance)
                                     { aAdvance = mMaxAdvance;
                                       return NS_OK; };

    NS_IMETHOD GetSpaceWidth         (nscoord &aSpaceCharWidth)
                                     { aSpaceCharWidth = mSpaceWidth;
                                       return NS_OK; };

    NS_IMETHOD GetAveCharWidth       (nscoord &aAveCharWidth)
                                     { aAveCharWidth = mAveCharWidth;
                                       return NS_OK; };

    // nsIFontMetricsGTK (calls from the font rendering layer)
    virtual nsresult GetWidth(const char* aString, PRUint32 aLength,
                              nscoord& aWidth,
                              nsRenderingContextGTK *aContext);
    virtual nsresult GetWidth(const PRUnichar* aString, PRUint32 aLength,
                              nscoord& aWidth, PRInt32 *aFontID,
                              nsRenderingContextGTK *aContext);

    virtual nsresult GetTextDimensions(const PRUnichar* aString,
                                       PRUint32 aLength,
                                       nsTextDimensions& aDimensions, 
                                       PRInt32* aFontID,
                                       nsRenderingContextGTK *aContext);
    virtual nsresult GetTextDimensions(const char*         aString,
                                       PRInt32             aLength,
                                       PRInt32             aAvailWidth,
                                       PRInt32*            aBreaks,
                                       PRInt32             aNumBreaks,
                                       nsTextDimensions&   aDimensions,
                                       PRInt32&            aNumCharsFit,
                                       nsTextDimensions&   aLastWordDimensions,
                                       PRInt32*            aFontID,
                                       nsRenderingContextGTK *aContext);
    virtual nsresult GetTextDimensions(const PRUnichar*    aString,
                                       PRInt32             aLength,
                                       PRInt32             aAvailWidth,
                                       PRInt32*            aBreaks,
                                       PRInt32             aNumBreaks,
                                       nsTextDimensions&   aDimensions,
                                       PRInt32&            aNumCharsFit,
                                       nsTextDimensions&   aLastWordDimensions,
                                       PRInt32*            aFontID,
                                       nsRenderingContextGTK *aContext);

    virtual nsresult DrawString(const char *aString, PRUint32 aLength,
                                nscoord aX, nscoord aY,
                                const nscoord* aSpacing,
                                nsRenderingContextGTK *aContext,
                                nsDrawingSurfaceGTK *aSurface);
    virtual nsresult DrawString(const PRUnichar* aString, PRUint32 aLength,
                                nscoord aX, nscoord aY,
                                PRInt32 aFontID,
                                const nscoord* aSpacing,
                                nsRenderingContextGTK *aContext,
                                nsDrawingSurfaceGTK *aSurface);

#ifdef MOZ_MATHML
    virtual nsresult GetBoundingMetrics(const char *aString, PRUint32 aLength,
                                        nsBoundingMetrics &aBoundingMetrics,
                                        nsRenderingContextGTK *aContext);
    virtual nsresult GetBoundingMetrics(const PRUnichar *aString,
                                        PRUint32 aLength,
                                        nsBoundingMetrics &aBoundingMetrics,
                                        PRInt32 *aFontID,
                                        nsRenderingContextGTK *aContext);
#endif /* MOZ_MATHML */

    virtual GdkFont* GetCurrentGDKFont(void);

    virtual nsresult SetRightToLeftText(PRBool aIsRTL);

    // get hints for the font
    static PRUint32    GetHints     (void);

    // drawing surface methods
    static nsresult FamilyExists    (nsIDeviceContext *aDevice,
                                     const nsString &aName);

private:

    // generic font metrics class bits
    nsCStringArray       mFontList;
    nsAutoVoidArray      mFontIsGeneric;

    nsIDeviceContext    *mDeviceContext;
    nsCOMPtr<nsIAtom>    mLangGroup;
    nsCString           *mGenericFont;
    nsFont              *mFont;
    float                mPointSize;

    nsCAutoString        mDefaultFont;

    // Pango-related items
    PangoFontDescription *mPangoFontDesc;
    PangoContext         *mPangoContext;
    PangoContext         *mLTRPangoContext;
    PangoContext         *mRTLPangoContext;
    PangoAttrList        *mPangoAttrList;
    PRBool                mIsRTL;

    // Cached font metrics
    nscoord                  mXHeight;
    nscoord                  mSuperscriptOffset;
    nscoord                  mSubscriptOffset;
    nscoord                  mStrikeoutOffset;
    nscoord                  mStrikeoutSize;
    nscoord                  mUnderlineOffset;
    nscoord                  mUnderlineSize;
    nscoord                  mMaxHeight;
    nscoord                  mLeading;
    nscoord                  mEmHeight;
    nscoord                  mEmAscent;
    nscoord                  mEmDescent;
    nscoord                  mMaxAscent;
    nscoord                  mMaxDescent;
    nscoord                  mMaxAdvance;
    nscoord                  mSpaceWidth;
    nscoord                  mAveCharWidth;

    // Private methods
    nsresult RealizeFont(void);
    nsresult CacheFontMetrics(void);

    static PRBool EnumFontCallback(const nsString &aFamily,
                                   PRBool aIsGeneric, void *aData);

    void     DrawStringSlowly(const gchar *aText,
                              const PRUnichar *aOrigString,
                              PRUint32 aLength,
                              GdkDrawable *aDrawable,
                              GdkGC *aGC, gint aX, gint aY,
                              PangoLayoutLine *aLine,
                              const nscoord *aSpacing);

    nsresult GetTextDimensionsInternal(const gchar*        aString,
                                       PRInt32             aLength,
                                       PRInt32             aAvailWidth,
                                       PRInt32*            aBreaks,
                                       PRInt32             aNumBreaks,
                                       nsTextDimensions&   aDimensions,
                                       PRInt32&            aNumCharsFit,
                                       nsTextDimensions&   aLastWordDimensions,
                                       nsRenderingContextGTK *aContext);
};

class nsFontEnumeratorPango : public nsIFontEnumerator
{
public:
    nsFontEnumeratorPango();
    NS_DECL_ISUPPORTS
    NS_DECL_NSIFONTENUMERATOR
};
