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

class nsIAtom;
class nsDeviceContext;
class nsRenderingContext;
struct nsBoundingMetrics;

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
     * @see nsDeviceContext#GetMetricsFor()
     */
    nsresult Init(const nsFont& aFont, nsIAtom* aLanguage,
                  nsDeviceContext *aContext,
                  gfxUserFontSet *aUserFontSet = nsnull);

    /**
     * Destroy this font metrics. This breaks the association between
     * the font metrics and the device context.
     */
    void Destroy();

    /**
     * Return the font's x-height.
     */
    nscoord XHeight();

    /**
     * Return the font's superscript offset (the distance from the
     * baseline to where a superscript's baseline should be placed).
     * The value returned will be positive.
     */
    nscoord SuperscriptOffset();

    /**
     * Return the font's subscript offset (the distance from the
     * baseline to where a subscript's baseline should be placed).
     * The value returned will be positive.
     */
    nscoord SubscriptOffset();

    /**
     * Return the font's strikeout offset (the distance from the
     * baseline to where a strikeout should be placed) and size.
     * Positive values are above the baseline, negative below.
     */
    void GetStrikeout(nscoord& aOffset, nscoord& aSize);

    /**
     * Return the font's underline offset (the distance from the
     * baseline to where a underline should be placed) and size.
     * Positive values are above the baseline, negative below.
     */
    void GetUnderline(nscoord& aOffset, nscoord& aSize);

    /**
     * Returns the amount of internal leading for the font.
     * This is normally the difference between the max ascent
     * and the em ascent.
     */
    nscoord InternalLeading();

    /**
     * Returns the amount of external leading for the font.
     * em ascent(?) plus external leading is the font designer's
     * recommended line-height for this font.
     */
    nscoord ExternalLeading();

    /**
     * Returns the height of the em square.
     * This is em ascent plus em descent.
     */
    nscoord EmHeight();

    /**
     * Returns the ascent part of the em square.
     */
    nscoord EmAscent();

    /**
     * Returns the descent part of the em square.
     */
    nscoord EmDescent();

    /**
     * Returns the height of the bounding box.
     * This is max ascent plus max descent.
     */
    nscoord MaxHeight();

    /**
     * Returns the maximum distance characters in this font extend
     * above the base line.
     */
    nscoord MaxAscent();

    /**
     * Returns the maximum distance characters in this font extend
     * below the base line.
     */
    nscoord MaxDescent();

    /**
     * Returns the maximum character advance for the font.
     */
    nscoord MaxAdvance();

    /**
     * Returns the average character width
     */
    nscoord AveCharWidth();

    /**
     * Returns the often needed width of the space character
     */
    nscoord SpaceWidth();

    /**
     * Returns the font associated with these metrics. The return value
     * is only defined after Init() has been called.
     */
    const nsFont &Font() { return mFont; }

    /**
     * Returns the language associated with these metrics
     */
    nsIAtom* Language() { return mLanguage; }

    PRInt32 GetMaxStringLength();

    // Get the width for this string.  aWidth will be updated with the
    // width in points, not twips.  Callers must convert it if they
    // want it in another format.
    nscoord GetWidth(const char* aString, PRUint32 aLength,
                     nsRenderingContext *aContext);
    nscoord GetWidth(const PRUnichar* aString, PRUint32 aLength,
                     nsRenderingContext *aContext);

    // Draw a string using this font handle on the surface passed in.
    void DrawString(const char *aString, PRUint32 aLength,
                    nscoord aX, nscoord aY,
                    nsRenderingContext *aContext);
    void DrawString(const PRUnichar* aString, PRUint32 aLength,
                    nscoord aX, nscoord aY,
                    nsRenderingContext *aContext,
                    nsRenderingContext *aTextRunConstructionContext);

    nsBoundingMetrics GetBoundingMetrics(const PRUnichar *aString,
                                         PRUint32 aLength,
                                         nsRenderingContext *aContext);

    void SetTextRunRTL(bool aIsRTL) { mTextRunRTL = aIsRTL; }
    bool GetTextRunRTL() { return mTextRunRTL; }

    gfxFontGroup* GetThebesFontGroup() { return mFontGroup; }
    gfxUserFontSet* GetUserFontSet() { return mFontGroup->GetUserFontSet(); }

    PRUint32 AppUnitsPerDevPixel() { return mP2A; }

protected:
    const gfxFont::Metrics& GetMetrics() const;

    nsFont mFont;
    nsRefPtr<gfxFontGroup> mFontGroup;
    nsCOMPtr<nsIAtom> mLanguage;
    nsDeviceContext *mDeviceContext;
    PRUint32 mP2A;
    bool mTextRunRTL;
};

#endif /* NSFONTMETRICS__H__ */
