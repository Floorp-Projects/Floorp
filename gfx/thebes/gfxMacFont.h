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
 * The Original Code is Mozilla Corporation code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2006-2010
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

#ifndef GFX_MACFONT_H
#define GFX_MACFONT_H

#include "gfxFont.h"
#include "gfxMacPlatformFontList.h"

#include "cairo.h"

class gfxMacFont : public gfxFont
{
public:
    gfxMacFont(MacOSFontEntry *aFontEntry, const gfxFontStyle *aFontStyle,
               bool aNeedsBold);

    virtual ~gfxMacFont();

    CGFontRef GetCGFontRef() const { return mCGFont; }

    /* overrides for the pure virtual methods in gfxFont */
    virtual const gfxFont::Metrics& GetMetrics() {
        return mMetrics;
    }

    virtual PRUint32 GetSpaceGlyph() {
        return mSpaceGlyph;
    }

    virtual bool SetupCairoFont(gfxContext *aContext);

    /* override Measure to add padding for antialiasing */
    virtual RunMetrics Measure(gfxTextRun *aTextRun,
                               PRUint32 aStart, PRUint32 aEnd,
                               BoundingBoxType aBoundingBoxType,
                               gfxContext *aContextForTightBoundingBox,
                               Spacing *aSpacing);

    // override gfxFont table access function to bypass gfxFontEntry cache,
    // use CGFontRef API to get direct access to system font data
    virtual hb_blob_t *GetFontTable(PRUint32 aTag);

protected:
    virtual void CreatePlatformShaper();

    // override to prefer CoreText shaping with fonts that depend on AAT
    virtual bool InitTextRun(gfxContext *aContext,
                               gfxTextRun *aTextRun,
                               const PRUnichar *aString,
                               PRUint32 aRunStart,
                               PRUint32 aRunLength,
                               PRInt32 aRunScript,
                               bool aPreferPlatformShaping = false);

    void InitMetrics();
    void InitMetricsFromPlatform();
    void InitMetricsFromATSMetrics(ATSFontRef aFontRef);

    // Get width and glyph ID for a character; uses aConvFactor
    // to convert font units as returned by CG to actual dimensions
    gfxFloat GetCharWidth(CFDataRef aCmap, PRUnichar aUniChar,
                          PRUint32 *aGlyphID, gfxFloat aConvFactor);

    static void DestroyBlobFunc(void* aUserData);

    // a weak reference to the CoreGraphics font: this is owned by the
    // MacOSFontEntry, it is not retained or released by gfxMacFont
    CGFontRef             mCGFont;

    cairo_font_face_t    *mFontFace;
    cairo_scaled_font_t  *mScaledFont;

    Metrics               mMetrics;
    PRUint32              mSpaceGlyph;
};

#endif /* GFX_MACFONT_H */
