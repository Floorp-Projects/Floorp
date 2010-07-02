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

#include "gfxMacFont.h"
#include "gfxCoreTextShaper.h"
#include "gfxHarfBuzzShaper.h"
#include "gfxPlatformMac.h"
#include "gfxContext.h"

#include "cairo-quartz.h"

gfxMacFont::gfxMacFont(MacOSFontEntry *aFontEntry, const gfxFontStyle *aFontStyle,
                       PRBool aNeedsBold)
    : gfxFont(aFontEntry, aFontStyle),
      mATSFont(aFontEntry->GetFontRef()),
      mCGFont(nsnull),
      mFontFace(nsnull),
      mScaledFont(nsnull)
{
    if (aNeedsBold) {
        mSyntheticBoldOffset = 1;  // devunit offset when double-striking text to fake boldness
    }

    mCGFont = ::CGFontCreateWithPlatformFont(&mATSFont);
    if (!mCGFont) {
        mIsValid = PR_FALSE;
        return;
    }

    // InitMetrics will handle the sizeAdjust factor and set mAdjustedSize
    InitMetrics();
    if (!mIsValid) {
        return;
    }

    mFontFace = cairo_quartz_font_face_create_for_cgfont(mCGFont);

    cairo_status_t cairoerr = cairo_font_face_status(mFontFace);
    if (cairoerr != CAIRO_STATUS_SUCCESS) {
        mIsValid = PR_FALSE;
#ifdef DEBUG
        char warnBuf[1024];
        sprintf(warnBuf, "Failed to create Cairo font face: %s status: %d",
                NS_ConvertUTF16toUTF8(GetName()).get(), cairoerr);
        NS_WARNING(warnBuf);
#endif
        return;
    }

    cairo_matrix_t sizeMatrix, ctm;
    cairo_matrix_init_identity(&ctm);
    cairo_matrix_init_scale(&sizeMatrix, mAdjustedSize, mAdjustedSize);

    // synthetic oblique by skewing via the font matrix
    PRBool needsOblique =
        (mFontEntry != NULL) &&
        (!mFontEntry->IsItalic() && (mStyle.style & (FONT_STYLE_ITALIC | FONT_STYLE_OBLIQUE)));

    if (needsOblique) {
        double skewfactor = (needsOblique ? Fix2X(kATSItalicQDSkew) : 0);

        cairo_matrix_t style;
        cairo_matrix_init(&style,
                          1,                //xx
                          0,                //yx
                          -1 * skewfactor,   //xy
                          1,                //yy
                          0,                //x0
                          0);               //y0
        cairo_matrix_multiply(&sizeMatrix, &sizeMatrix, &style);
    }

    cairo_font_options_t *fontOptions = cairo_font_options_create();

    // turn off font anti-aliasing based on user pref setting
    if (mAdjustedSize <= (float) gfxPlatformMac::GetPlatform()->GetAntiAliasingThreshold()) {
        cairo_font_options_set_antialias(fontOptions, CAIRO_ANTIALIAS_NONE);
    }

    mScaledFont = cairo_scaled_font_create(mFontFace, &sizeMatrix, &ctm, fontOptions);
    cairo_font_options_destroy(fontOptions);

    cairoerr = cairo_scaled_font_status(mScaledFont);
    if (cairoerr != CAIRO_STATUS_SUCCESS) {
        mIsValid = PR_FALSE;
#ifdef DEBUG
        char warnBuf[1024];
        sprintf(warnBuf, "Failed to create scaled font: %s status: %d",
                NS_ConvertUTF16toUTF8(GetName()).get(), cairoerr);
        NS_WARNING(warnBuf);
#endif
    }

    if (FontCanSupportHarfBuzz()) {
        mHarfBuzzShaper = new gfxHarfBuzzShaper(this);
    }
}

gfxMacFont::~gfxMacFont()
{
    if (mScaledFont) {
        cairo_scaled_font_destroy(mScaledFont);
    }
    if (mFontFace) {
        cairo_font_face_destroy(mFontFace);
    }

    // this is documented to be safe if mCGFont is null
    ::CGFontRelease(mCGFont);
}

void
gfxMacFont::CreatePlatformShaper()
{
    mPlatformShaper = new gfxCoreTextShaper(this);
}

PRBool
gfxMacFont::SetupCairoFont(gfxContext *aContext)
{
    if (cairo_scaled_font_status(mScaledFont) != CAIRO_STATUS_SUCCESS) {
        // Don't cairo_set_scaled_font as that would propagate the error to
        // the cairo_t, precluding any further drawing.
        return PR_FALSE;
    }
    cairo_set_scaled_font(aContext->GetCairo(), mScaledFont);
    return PR_TRUE;
}

static double
RoundToNearestMultiple(double aValue, double aFraction)
{
    return floor(aValue/aFraction + 0.5) * aFraction;
}

void
gfxMacFont::InitMetrics()
{
    gfxFloat size =
        PR_MAX(((mAdjustedSize != 0.0f) ? mAdjustedSize : mStyle.size), 1.0f);
    PRUint32 upem = ::CGFontGetUnitsPerEm(mCGFont);
    if (!upem) {
        mIsValid = PR_FALSE;
#ifdef DEBUG
        char warnBuf[1024];
        sprintf(warnBuf, "Bad font metrics for: %s (no unitsPerEm value)",
                NS_ConvertUTF16toUTF8(mFontEntry->Name()).get());
        NS_WARNING(warnBuf);
#endif
        return;
    }

    ATSFontMetrics atsMetrics;
    OSStatus err;

    err = ::ATSFontGetHorizontalMetrics(mATSFont, kATSOptionFlagsDefault,
                                        &atsMetrics);
    if (err != noErr) {
        mIsValid = PR_FALSE;

#ifdef DEBUG
        char warnBuf[1024];
        sprintf(warnBuf, "Bad font metrics for: %s err: %8.8x",
                NS_ConvertUTF16toUTF8(mFontEntry->Name()).get(), PRUint32(err));
        NS_WARNING(warnBuf);
#endif
        return;
    }

    if (atsMetrics.xHeight > 0)
        mMetrics.xHeight = atsMetrics.xHeight * size;
    else
        mMetrics.xHeight = ::CGFontGetXHeight(mCGFont) * size / upem;

    if (mAdjustedSize == 0.0f) {
        if (mMetrics.xHeight != 0.0f && mStyle.sizeAdjust != 0.0f) {
            gfxFloat aspect = mMetrics.xHeight / size;
            mAdjustedSize = mStyle.GetAdjustedSize(aspect);

            // the recursive call to InitMetrics will see the adjusted size,
            // and set up the rest of the metrics fields accordingly
            InitMetrics();
            return;
        }
        mAdjustedSize = size;
    }

    mMetrics.superscriptOffset = mMetrics.xHeight;
    mMetrics.subscriptOffset = mMetrics.xHeight;
    mMetrics.underlineOffset = atsMetrics.underlinePosition * size;
    mMetrics.underlineSize = atsMetrics.underlineThickness * size;
    mMetrics.strikeoutSize = mMetrics.underlineSize;
    mMetrics.strikeoutOffset = mMetrics.xHeight / 2;

    mMetrics.externalLeading = atsMetrics.leading * size;
    mMetrics.emHeight = size;
    mMetrics.maxAscent =
      NS_ceil(RoundToNearestMultiple(atsMetrics.ascent * size, 1/1024.0));
    mMetrics.maxDescent =
      NS_ceil(-RoundToNearestMultiple(atsMetrics.descent * size, 1/1024.0));

    mMetrics.maxHeight = mMetrics.maxAscent + mMetrics.maxDescent;
    if (mMetrics.maxHeight - mMetrics.emHeight > 0.0)
        mMetrics.internalLeading = mMetrics.maxHeight - mMetrics.emHeight;
    else
        mMetrics.internalLeading = 0.0;

    mMetrics.maxAdvance = atsMetrics.maxAdvanceWidth * size + mSyntheticBoldOffset;

    mMetrics.emAscent = mMetrics.maxAscent * mMetrics.emHeight / mMetrics.maxHeight;
    mMetrics.emDescent = mMetrics.emHeight - mMetrics.emAscent;

    CFDataRef cmap =
        ::CGFontCopyTableForTag(mCGFont, TRUETYPE_TAG('c','m','a','p'));

    PRUint32 glyphID;
    gfxFloat xWidth = GetCharWidth(cmap, upem, size, 'x', &glyphID);
    if (atsMetrics.avgAdvanceWidth != 0.0)
        mMetrics.aveCharWidth = PR_MIN(atsMetrics.avgAdvanceWidth * size, xWidth);
    else if (glyphID != 0)
        mMetrics.aveCharWidth = xWidth;
    else
        mMetrics.aveCharWidth = mMetrics.maxAdvance;
    mMetrics.aveCharWidth += mSyntheticBoldOffset;

    if (mFontEntry->IsFixedPitch()) {
        // Some Quartz fonts are fixed pitch, but there's some glyph with a bigger
        // advance than the average character width... this forces
        // those fonts to be recognized like fixed pitch fonts by layout.
        mMetrics.maxAdvance = mMetrics.aveCharWidth;
    }

    mMetrics.spaceWidth = GetCharWidth(cmap, upem, size, ' ', &glyphID);
    mSpaceGlyph = glyphID;

    mMetrics.zeroOrAveCharWidth = GetCharWidth(cmap, upem, size, '0', &glyphID);
    if (glyphID == 0)
        mMetrics.zeroOrAveCharWidth = mMetrics.aveCharWidth;

    if (cmap) {
        ::CFRelease(cmap);
    }

    mFUnitsConvFactor = mAdjustedSize / upem;

    SanitizeMetrics(&mMetrics, mFontEntry->mIsBadUnderlineFont);

    mIsValid = PR_TRUE;

#if 0
    fprintf (stderr, "Font: %p (%s) size: %f\n", this,
             NS_ConvertUTF16toUTF8(GetName()).get(), mStyle.size);
//    fprintf (stderr, "    fbounds.origin.x %f y %f size.width %f height %f\n", fbounds.origin.x, fbounds.origin.y, fbounds.size.width, fbounds.size.height);
    fprintf (stderr, "    emHeight: %f emAscent: %f emDescent: %f\n", mMetrics.emHeight, mMetrics.emAscent, mMetrics.emDescent);
    fprintf (stderr, "    maxAscent: %f maxDescent: %f maxAdvance: %f\n", mMetrics.maxAscent, mMetrics.maxDescent, mMetrics.maxAdvance);
    fprintf (stderr, "    internalLeading: %f externalLeading: %f\n", mMetrics.internalLeading, mMetrics.externalLeading);
    fprintf (stderr, "    spaceWidth: %f aveCharWidth: %f xHeight: %f\n", mMetrics.spaceWidth, mMetrics.aveCharWidth, mMetrics.xHeight);
    fprintf (stderr, "    uOff: %f uSize: %f stOff: %f stSize: %f supOff: %f subOff: %f\n", mMetrics.underlineOffset, mMetrics.underlineSize, mMetrics.strikeoutOffset, mMetrics.strikeoutSize, mMetrics.superscriptOffset, mMetrics.subscriptOffset);
#endif
}

gfxFloat
gfxMacFont::GetCharWidth(CFDataRef aCmap, PRUint32 aUpem, gfxFloat aSize,
                         PRUnichar aUniChar, PRUint32 *aGlyphID)
{
    CGGlyph glyph = 0;
    
    if (aCmap) {
        glyph = gfxFontUtils::MapCharToGlyph(::CFDataGetBytePtr(aCmap),
                                             ::CFDataGetLength(aCmap),
                                             aUniChar);
    }

    if (aGlyphID) {
        *aGlyphID = glyph;
    }

    if (glyph) {
        int advance;
        if (::CGFontGetGlyphAdvances(mCGFont, &glyph, 1, &advance)) {
            return advance * aSize / aUpem;
        }
    }

    return 0;
}

/*static*/ void
gfxMacFont::DestroyBlobFunc(void* aUserData)
{
    ::CFRelease((CFDataRef)aUserData);
}

hb_blob_t *
gfxMacFont::GetFontTable(PRUint32 aTag)
{
    CFDataRef dataRef = ::CGFontCopyTableForTag(mCGFont, aTag);
    if (dataRef) {
        return hb_blob_create((const char*)::CFDataGetBytePtr(dataRef),
                              ::CFDataGetLength(dataRef),
                              HB_MEMORY_MODE_READONLY,
                              DestroyBlobFunc, (void*)dataRef);
    }

    return nsnull;
}
