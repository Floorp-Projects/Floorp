/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Initial Developer of the Original Code is
 * mozilla.org.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Stuart Parmenter <pavlov@pavlov.net>
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

#ifndef NSFONTMETRICS__H__
#define NSFONTMETRICS__H__

#include "nsCOMPtr.h"
#include "nsCoord.h"
#include "nsFont.h"
#include "gfxFont.h"
#include "gfxTextRunCache.h"

class gfxFontGroup;
class gfxUserFontSet;
class nsIAtom;
class nsIDeviceContext;
class nsRenderingContext;
class nsString;
class nsThebesDeviceContext;
struct nsBoundingMetrics;
struct nsTextDimensions;

/**
 * A native font handle
 */
typedef void* nsFontHandle;

/**
 * Font metrics
 *
 * This class may be somewhat misnamed. A better name might be
 * nsFontList. The style system uses the nsFont struct for various
 * font properties, one of which is font-family, which can contain a
 * *list* of font names. The nsFont struct is "realized" by asking the
 * device context to cough up an nsFontMetrics object, which contains
 * a list of real font handles, one for each font mentioned in
 * font-family (and for each fallback when we fall off the end of that
 * list).
 *
 * The style system needs to have access to certain metrics, such as
 * the em height (for the CSS "em" unit), and we use the first Western
 * font's metrics for that purpose. The platform-specific
 * implementations are expected to select non-Western fonts that "fit"
 * reasonably well with the Western font that is loaded at Init time.
 */
class nsFontMetrics
{
public:
    nsFontMetrics();
    ~nsFontMetrics();

    NS_INLINE_DECL_REFCOUNTING(nsFontMetrics)

    /**
     * Initialize the font metrics. Call this after creating the font metrics.
     * Font metrics you get from the font cache do NOT need to be initialized
     *
     * @see nsIDeviceContext#GetMetricsFor()
     */
    nsresult Init(const nsFont& aFont, nsIAtom* aLanguage,
                  nsIDeviceContext *aContext,
                  gfxUserFontSet *aUserFontSet = nsnull);
    /**
     * Destroy this font metrics. This breaks the association between
     * the font metrics and the device context.
     */
    nsresult Destroy();
    /**
     * Return the font's xheight property, scaled into app-units.
     */
    nsresult GetXHeight(nscoord& aResult);
    /**
     * Return the font's superscript offset (the distance from the
     * baseline to where a superscript's baseline should be placed).
     * The value returned will be positive.
     */
    nsresult GetSuperscriptOffset(nscoord& aResult);
    /**
     * Return the font's subscript offset (the distance from the
     * baseline to where a subscript's baseline should be placed). The
     * value returned will be a positive value.
     */
    nsresult GetSubscriptOffset(nscoord& aResult);
    /**
     * Return the font's strikeout offset (the distance from the
     * baseline to where a strikeout should be placed) and size.
     * Positive values are above the baseline, negative below.
     */
    nsresult GetStrikeout(nscoord& aOffset, nscoord& aSize);
    /**
     * Return the font's underline offset (the distance from the
     * baseline to where a underline should be placed) and size.
     * Positive values are above the baseline, negative below.
     */
    nsresult GetUnderline(nscoord& aOffset, nscoord& aSize);
    /**
     * Returns the height (in app units) of the font. This is ascent plus descent
     * plus any internal leading
     *
     * This method will be removed once the callers have been moved over to the
     * new GetEmHeight (and possibly GetMaxHeight).
     */
    nsresult GetHeight(nscoord &aHeight);
    /**
     * Returns the amount of internal leading (in app units) for the font. This
     * is computed as the "height  - (ascent + descent)"
     */
    nsresult GetInternalLeading(nscoord &aLeading);
    /**
     * Returns the amount of external leading (in app units) as
     * suggested by font vendor. This value is suggested by font vendor
     * to add to normal line-height beside font height.
     */
    nsresult GetExternalLeading(nscoord &aLeading);
    /**
     * Returns the height (in app units) of the Western font's em square. This is
     * em ascent plus em descent.
     */
    nsresult GetEmHeight(nscoord &aHeight);
    /**
     * Returns, in app units, the ascent part of the Western font's em square.
     */
    nsresult GetEmAscent(nscoord &aAscent);
    /**
     * Returns, in app units, the descent part of the Western font's em square.
     */
    nsresult GetEmDescent(nscoord &aDescent);
    /**
     * Returns the height (in app units) of the Western font's bounding box.
     * This is max ascent plus max descent.
     */
    nsresult GetMaxHeight(nscoord &aHeight);
    /**
     * Returns, in app units, the maximum distance characters in this font extend
     * above the base line.
     */
    nsresult GetMaxAscent(nscoord &aAscent);
    /**
     * Returns, in app units, the maximum distance characters in this font extend
     * below the base line.
     */
    nsresult GetMaxDescent(nscoord &aDescent);
    /**
     * Returns, in app units, the maximum character advance for the font
     */
    nsresult GetMaxAdvance(nscoord &aAdvance);
    /**
     * Returns the font associated with these metrics. The return value
     * is only defined after Init() has been called.
     */
    const nsFont &Font() { return mFont; }
    /**
     * Returns the language associated with these metrics
     */
    nsresult GetLanguage(nsIAtom** aLanguage);
    /**
     * Returns the font handle associated with these metrics
     */
    nsresult GetFontHandle(nsFontHandle &aHandle);
    /**
     * Returns the average character width
     */
    nsresult GetAveCharWidth(nscoord& aAveCharWidth);
    /**
     * Returns the often needed width of the space character
     */
    nsresult GetSpaceWidth(nscoord& aSpaceCharWidth);

    PRInt32 GetMaxStringLength();

    // Get the width for this string.  aWidth will be updated with the
    // width in points, not twips.  Callers must convert it if they
    // want it in another format.
    nsresult GetWidth(const char* aString, PRUint32 aLength, nscoord& aWidth,
                      nsRenderingContext *aContext);
    nsresult GetWidth(const PRUnichar* aString, PRUint32 aLength,
                      nscoord& aWidth, PRInt32 *aFontID,
                      nsRenderingContext *aContext);

    // Get the text dimensions for this string
    nsresult GetTextDimensions(const PRUnichar* aString,
                               PRUint32 aLength,
                               nsTextDimensions& aDimensions,
                               PRInt32* aFontID);
    nsresult GetTextDimensions(const char*         aString,
                               PRInt32             aLength,
                               PRInt32             aAvailWidth,
                               PRInt32*            aBreaks,
                               PRInt32             aNumBreaks,
                               nsTextDimensions&   aDimensions,
                               PRInt32&            aNumCharsFit,
                               nsTextDimensions&   aLastWordDimensions,
                               PRInt32*            aFontID);
    nsresult GetTextDimensions(const PRUnichar*    aString,
                               PRInt32             aLength,
                               PRInt32             aAvailWidth,
                               PRInt32*            aBreaks,
                               PRInt32             aNumBreaks,
                               nsTextDimensions&   aDimensions,
                               PRInt32&            aNumCharsFit,
                               nsTextDimensions&   aLastWordDimensions,
                               PRInt32*            aFontID);

    // Draw a string using this font handle on the surface passed in.
    nsresult DrawString(const char *aString, PRUint32 aLength,
                        nscoord aX, nscoord aY,
                        const nscoord* aSpacing,
                        nsRenderingContext *aContext);
    nsresult DrawString(const PRUnichar* aString, PRUint32 aLength,
                        nscoord aX, nscoord aY,
                        PRInt32 aFontID,
                        const nscoord* aSpacing,
                        nsRenderingContext *aContext)
    {
        NS_ASSERTION(!aSpacing, "Spacing not supported here");
        return DrawString(aString, aLength, aX, aY, aContext, aContext);
    }
    nsresult DrawString(const PRUnichar* aString, PRUint32 aLength,
                        nscoord aX, nscoord aY,
                        nsRenderingContext *aContext,
                        nsRenderingContext *aTextRunConstructionContext);

#ifdef MOZ_MATHML
    // These two functions get the bounding metrics for this handle,
    // updating the aBoundingMetrics in app units.
    nsresult GetBoundingMetrics(const char *aString, PRUint32 aLength,
                                nsRenderingContext *aContext,
                                nsBoundingMetrics &aBoundingMetrics);
    nsresult GetBoundingMetrics(const PRUnichar *aString,
                                PRUint32 aLength,
                                nsRenderingContext *aContext,
                                nsBoundingMetrics &aBoundingMetrics);
#endif /* MOZ_MATHML */

    // Set the direction of the text rendering
    nsresult SetRightToLeftText(PRBool aIsRTL);
    PRBool GetRightToLeftText();
    void SetTextRunRTL(PRBool aIsRTL) { mTextRunRTL = aIsRTL; }

    gfxFontGroup* GetThebesFontGroup() { return mFontGroup; }

    gfxUserFontSet* GetUserFontSet();

    PRBool GetRightToLeftTextRunMode() {
        return mTextRunRTL;
    }

    PRInt32 AppUnitsPerDevPixel() { return mP2A; }

protected:
    const gfxFont::Metrics& GetMetrics() const;

    nsFont mFont;		// The font for this metrics object.
    nsRefPtr<gfxFontGroup> mFontGroup;
    gfxFontStyle *mFontStyle;
    nsThebesDeviceContext *mDeviceContext;
    nsCOMPtr<nsIAtom> mLanguage;
    PRInt32 mP2A;
    PRPackedBool mIsRightToLeft;
    PRPackedBool mTextRunRTL;
};

#endif /* NSFONTMETRICS__H__ */
