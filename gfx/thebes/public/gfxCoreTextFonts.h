/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is thebes gfx code.
 *
 * The Initial Developer of the Original Code is Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2006-2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@pobox.com>
 *   Masayuki Nakano <masayuki@d-toybox.com>
 *   John Daggett <jdaggett@mozilla.com>
 *   Jonathan Kew <jfkthame@gmail.com>
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

#ifndef GFX_CORETEXTFONTS_H
#define GFX_CORETEXTFONTS_H

#include "cairo.h"
#include "gfxTypes.h"
#include "gfxFont.h"
#include "gfxFontUtils.h"
#include "gfxPlatform.h"

#include <Carbon/Carbon.h>

class gfxCoreTextFontGroup;

class MacOSFontEntry;

class gfxCoreTextFont : public gfxFont {
public:

    gfxCoreTextFont(MacOSFontEntry *aFontEntry,
                    const gfxFontStyle *fontStyle, PRBool aNeedsBold);

    virtual ~gfxCoreTextFont();

    virtual const gfxFont::Metrics& GetMetrics() {
        NS_ASSERTION(mHasMetrics == PR_TRUE, "metrics not initialized");
        return mMetrics;
    }

    float GetCharWidth(PRUnichar c, PRUint32 *aGlyphID = nsnull);
    float GetCharHeight(PRUnichar c);

    ATSFontRef GetATSFont() {
        return mATSFont;
    }

    CTFontRef GetCTFont() {
        return mCTFont;
    }

    CFDictionaryRef GetAttributesDictionary() {
        return mAttributesDict;
    }

    cairo_font_face_t *CairoFontFace() {
        return mFontFace;
    }

    cairo_scaled_font_t *CairoScaledFont() {
        return mScaledFont;
    }

    virtual nsString GetUniqueName() {
        return GetName();
    }

    virtual PRUint32 GetSpaceGlyph() {
        return mSpaceGlyph;
    }

    PRBool TestCharacterMap(PRUint32 aCh);

    MacOSFontEntry* GetFontEntry();

    PRBool Valid() {
        return mIsValid;
    }

    // clean up static objects that may have been cached
    static void Shutdown();

    static CTFontRef CreateCTFontWithDisabledLigatures(ATSFontRef aFont, CGFloat aSize);

protected:
    const gfxFontStyle *mFontStyle;

    ATSFontRef mATSFont;
    CTFontRef mCTFont;
    CFDictionaryRef mAttributesDict;

    PRBool mHasMetrics;

    nsString mUniqueName;

    cairo_font_face_t *mFontFace;
    cairo_scaled_font_t *mScaledFont;

    gfxFont::Metrics mMetrics;

    gfxFloat mAdjustedSize;
    PRUint32 mSpaceGlyph;    

    void InitMetrics();

    virtual PRBool SetupCairoFont(gfxContext *aContext);

    static void CreateDefaultFeaturesDescriptor();

    static CTFontDescriptorRef GetDefaultFeaturesDescriptor() {
        if (sDefaultFeaturesDescriptor == NULL)
            CreateDefaultFeaturesDescriptor();
        return sDefaultFeaturesDescriptor;
    }

    // cached font descriptor, created the first time it's needed
    static CTFontDescriptorRef    sDefaultFeaturesDescriptor;
    // cached descriptor for adding disable-ligatures setting to a font
    static CTFontDescriptorRef    sDisableLigaturesDescriptor;
};

class THEBES_API gfxCoreTextFontGroup : public gfxFontGroup {
public:
    gfxCoreTextFontGroup(const nsAString& families,
                         const gfxFontStyle *aStyle,
                         gfxUserFontSet *aUserFontSet);
    virtual ~gfxCoreTextFontGroup() {};

    virtual gfxFontGroup *Copy(const gfxFontStyle *aStyle);

    virtual gfxTextRun *MakeTextRun(const PRUnichar* aString, PRUint32 aLength,
                                    const Parameters* aParams, PRUint32 aFlags);
    virtual gfxTextRun *MakeTextRun(const PRUint8* aString, PRUint32 aLength,
                                    const Parameters* aParams, PRUint32 aFlags);
    // When aWrapped is true, the string includes bidi control
    // characters. The first character will be LRO or LRO to force setting the
    // direction for all characters, the last character is PDF, and the
    // second to last character is a non-whitespace character --- to ensure
    // that there is no "trailing whitespace" in the string, see
    // http://weblogs.mozillazine.org/roc/archives/2007/02/superlaser_targ.html#comments
    void MakeTextRunInternal(const PRUnichar *aString, PRUint32 aLength,
                             PRBool aWrapped, gfxTextRun *aTextRun);

    gfxCoreTextFont* GetFontAt(PRInt32 aFontIndex) {
        return static_cast<gfxCoreTextFont*>(static_cast<gfxFont*>(mFonts[aFontIndex]));
    }

    PRBool HasFont(ATSFontRef aFontRef);

    inline gfxCoreTextFont* WhichFontSupportsChar(nsTArray< nsRefPtr<gfxFont> >& aFontList, 
                                                  PRUint32 aCh)
    {
        PRUint32 len = aFontList.Length();
        for (PRUint32 i = 0; i < len; i++) {
            gfxCoreTextFont* font = static_cast<gfxCoreTextFont*>(aFontList.ElementAt(i).get());
            if (font->TestCharacterMap(aCh))
                return font;
        }
        return nsnull;
    }

    // search through pref fonts for a character, return nsnull if no matching pref font
    already_AddRefed<gfxFont> WhichPrefFontSupportsChar(PRUint32 aCh);

    already_AddRefed<gfxFont> WhichSystemFontSupportsChar(PRUint32 aCh);

    void UpdateFontList();

protected:
    static PRBool FindCTFont(const nsAString& aName,
                             const nsACString& aGenericName,
                             void *closure);

    /**
     * @param aTextRun the text run to fill in
     * @param aString the complete text including all wrapper characters
     * @param aTotalLength the length of aString
     * @param aLayoutStart the first "real" character of aString, skipping any dir override
     * @param aLayoutLength the length of the characters that should be actually used
     */
    void InitTextRun(gfxTextRun *aTextRun,
                     const PRUnichar *aString,
                     PRUint32 aTotalLength,
                     PRUint32 aLayoutStart,
                     PRUint32 aLayoutLength);

    nsresult SetGlyphsFromRun(gfxTextRun *aTextRun,
                              CTRunRef aCTRun,
                              const PRPackedBool *aUnmatched,
                              PRInt32 aLayoutStart,
                              PRInt32 aLayoutLength);

    // cache the most recent pref font to avoid general pref font lookup
    nsRefPtr<gfxFontFamily>       mLastPrefFamily;
    nsRefPtr<gfxCoreTextFont>     mLastPrefFont;
    eFontPrefLang                 mLastPrefLang;       // lang group for last pref font
    PRBool                        mLastPrefFirstFont;  // is this the first font in the list of pref fonts for this lang group?
    eFontPrefLang                 mPageLang;
};

#endif /* GFX_CORETEXTFONTS_H */
