/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
                  gfxUserFontSet *aUserFontSet = nullptr);

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

    int32_t GetMaxStringLength();

    // Get the width for this string.  aWidth will be updated with the
    // width in points, not twips.  Callers must convert it if they
    // want it in another format.
    nscoord GetWidth(const char* aString, uint32_t aLength,
                     nsRenderingContext *aContext);
    nscoord GetWidth(const PRUnichar* aString, uint32_t aLength,
                     nsRenderingContext *aContext);

    // Draw a string using this font handle on the surface passed in.
    void DrawString(const char *aString, uint32_t aLength,
                    nscoord aX, nscoord aY,
                    nsRenderingContext *aContext);
    void DrawString(const PRUnichar* aString, uint32_t aLength,
                    nscoord aX, nscoord aY,
                    nsRenderingContext *aContext,
                    nsRenderingContext *aTextRunConstructionContext);

    nsBoundingMetrics GetBoundingMetrics(const PRUnichar *aString,
                                         uint32_t aLength,
                                         nsRenderingContext *aContext);

    // Returns the LOOSE_INK_EXTENTS bounds of the text for determing the
    // overflow area of the string.
    nsBoundingMetrics GetInkBoundsForVisualOverflow(const PRUnichar *aString,
                                                    uint32_t aLength,
                                                    nsRenderingContext *aContext);

    void SetTextRunRTL(bool aIsRTL) { mTextRunRTL = aIsRTL; }
    bool GetTextRunRTL() { return mTextRunRTL; }

    gfxFontGroup* GetThebesFontGroup() { return mFontGroup; }
    gfxUserFontSet* GetUserFontSet() { return mFontGroup->GetUserFontSet(); }

    int32_t AppUnitsPerDevPixel() { return mP2A; }

protected:
    const gfxFont::Metrics& GetMetrics() const;

    nsFont mFont;
    nsRefPtr<gfxFontGroup> mFontGroup;
    nsCOMPtr<nsIAtom> mLanguage;
    nsDeviceContext *mDeviceContext;
    int32_t mP2A;
    bool mTextRunRTL;
};

#endif /* NSFONTMETRICS__H__ */
