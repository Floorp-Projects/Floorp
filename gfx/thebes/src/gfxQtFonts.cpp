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
 * The Original Code is Mozilla Foundation code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@mozilla.com>
 *   Masayuki Nakano <masayuki@d-toybox.com>
 *   Behdad Esfahbod <behdad@gnome.org>
 *   Mats Palmgren <mats.palmgren@bredband.net>
 *   Karl Tomlinson <karlt+@karlt.net>, Mozilla Corporation
 *
 * based on nsFontMetricsPango.cpp by
 *   Christopher Blizzard <blizzard@mozilla.org>
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

#include "gfxPlatformQt.h"
#include "gfxTypes.h"
#include "gfxQtFonts.h"
#include "qdebug.h"

/**
 * gfxQtFontGroup
 */

static int
FFRECountHyphens (const nsAString &aFFREName)
{
    int h = 0;
    PRInt32 hyphen = 0;
    while ((hyphen = aFFREName.FindChar('-', hyphen)) >= 0) {
        ++h;
        ++hyphen;
    }
    return h;
}

PRBool
gfxQtFontGroup::FontCallback (const nsAString& fontName,
                                 const nsACString& genericName,
                                 void *closure)
{
    qDebug(">>>>>>Func:%s::%d, fontname:%s, genName:%s\n", __PRETTY_FUNCTION__, __LINE__, NS_ConvertUTF16toUTF8(fontName).get(), nsCString(genericName).get());
    nsStringArray *sa = static_cast<nsStringArray*>(closure);

    // We ignore prefs that have three hypens since they are X style prefs.
    if (genericName.Length() && FFRECountHyphens(fontName) >= 3)
        return PR_TRUE;

    if (sa->IndexOf(fontName) < 0) {
        sa->AppendString(fontName);
    }

    return PR_TRUE;
}

/**
 * Look up the font in the gfxFont cache. If we don't find it, create one.
 * In either case, add a ref, append it to the aFonts array, and return it ---
 * except for OOM in which case we do nothing and return null.
 */
static already_AddRefed<gfxQtFont>
GetOrMakeFont(const nsAString& aName, const gfxFontStyle *aStyle)
{
    qDebug(">>>>>>Func:%s::%d\n", __PRETTY_FUNCTION__, __LINE__);
    nsRefPtr<gfxFont> font = gfxFontCache::GetCache()->Lookup(aName, aStyle);
    if (!font) {
        font = new gfxQtFont(aName, aStyle);
        if (!font)
            return nsnull;
        gfxFontCache::GetCache()->AddNew(font);
    }
    gfxFont *f = nsnull;
    font.swap(f);
    return static_cast<gfxQtFont *>(f);
}


gfxQtFontGroup::gfxQtFontGroup (const nsAString& families,
                                const gfxFontStyle *aStyle)
    : gfxFontGroup(families, aStyle)
{
    qDebug("Func:%s::%d, fam:%s\n\tNeed to create font metrics, otherwise - CRASH\n\n", __PRETTY_FUNCTION__, __LINE__, NS_ConvertUTF16toUTF8(families).get());
    nsStringArray familyArray;

    // Leave non-existing fonts in the list so that fontconfig can get the
    // best match.
    ForEachFontInternal(families, aStyle->langGroup, PR_TRUE, PR_FALSE,
                        FontCallback, &familyArray);

    // Construct a string suitable for fontconfig
    nsAutoString fcFamilies;
    if (familyArray.Count()) {
        qDebug(">>>>>>Func:%s::%d\n", __PRETTY_FUNCTION__, __LINE__);
        int i = 0;
        while (1) {
            fcFamilies.Append(*familyArray[i]);
            ++i;
            if (i >= familyArray.Count())
                break;
            fcFamilies.Append(NS_LITERAL_STRING(","));
        }
    }
    else {
        // XXX If there are no fonts, we should use dummy family.
        // Pango will resolve from this.
        // behdad: yep, looks good.
        // printf("%s(%s)\n", NS_ConvertUTF16toUTF8(families).get(),
        //                    aStyle->langGroup.get());
        fcFamilies.Append(NS_LITERAL_STRING("sans-serif"));
    }

    nsRefPtr<gfxQtFont> font = GetOrMakeFont(fcFamilies, &mStyle);
    if (font) {
        mFonts.AppendElement(font);
    }
}

gfxQtFontGroup::~gfxQtFontGroup()
{
    qDebug(">>>>>>Func:%s::%d\n", __PRETTY_FUNCTION__, __LINE__);
}

gfxFontGroup *
gfxQtFontGroup::Copy(const gfxFontStyle *aStyle)
{
    qDebug(">>>>>>Func:%s::%d\n", __PRETTY_FUNCTION__, __LINE__);
     return new gfxQtFontGroup(mFamilies, aStyle);
}

#if defined(ENABLE_FAST_PATH_8BIT)
PRBool
gfxQtFontGroup::CanTakeFastPath(PRUint32 aFlags)
{
    // Can take fast path only if OPTIMIZE_SPEED is set and IS_RTL isn't.
    // We need to always use Pango for RTL text, in case glyph mirroring is
    // required.
    PRBool speed = aFlags & gfxTextRunFactory::TEXT_OPTIMIZE_SPEED;
    PRBool isRTL = aFlags & gfxTextRunFactory::TEXT_IS_RTL;
    return speed && !isRTL
           //&& PANGO_IS_FC_FONT(GetFontAt(0)->GetPangoFont())
           ;
}
#endif

void
gfxQtFontGroup::InitTextRun(gfxTextRun *aTextRun, const char *aUTF8Text,
                             PRUint32 aUTF8Length, PRUint32 aUTF8HeaderLength,
                             PRBool aTake8BitPath)
{
    qDebug(">>>>>>Func:%s::%d, Text:'%s'\n", __PRETTY_FUNCTION__, __LINE__, aUTF8Text);
#if defined(ENABLE_FAST_PATH_ALWAYS)
//    CreateGlyphRunsFast(aTextRun, aUTF8Text + aUTF8HeaderLength, aUTF8Length - aUTF8HeaderLength);
#else
#if defined(ENABLE_FAST_PATH_8BIT)
    if (aTake8BitPath && CanTakeFastPath(aTextRun->GetFlags())) {
//        nsresult rv = CreateGlyphRunsFast(aTextRun, aUTF8Text + aUTF8HeaderLength, aUTF8Length - aUTF8HeaderLength);
        if (NS_SUCCEEDED(rv))
            return;
    }
#endif

//    CreateGlyphRunsItemizing(aTextRun, aUTF8Text, aUTF8Length, aUTF8HeaderLength);
#endif
}

/**
 * We use this to append an LTR or RTL Override character to the start of the
 * string. This forces Pango to honour our direction even if there are neutral characters
 * in the string.
 */
static PRInt32 AppendDirectionalIndicatorUTF8(PRBool aIsRTL, nsACString& aString)
{
    static const PRUnichar overrides[2][2] =
      { { 0x202d, 0 }, { 0x202e, 0 }}; // LRO, RLO
    AppendUTF16toUTF8(overrides[aIsRTL], aString);
    return 3; // both overrides map to 3 bytes in UTF8
}

gfxTextRun *
gfxQtFontGroup::MakeTextRun(const PRUint8 *aString, PRUint32 aLength,
                               const Parameters *aParams, PRUint32 aFlags)
{
    qDebug(">>>>>>Func:%s::%d\n", __PRETTY_FUNCTION__, __LINE__);
    NS_ASSERTION(aFlags & TEXT_IS_8BIT, "8bit should have been set");
    gfxTextRun *run = gfxTextRun::Create(aParams, aString, aLength, this, aFlags);
    if (!run)
        return nsnull;

    PRBool isRTL = run->IsRightToLeft();
    if ((aFlags & TEXT_IS_ASCII) && !isRTL) {
        // We don't need to send an override character here, the characters must be all LTR
        const char *utf8Chars = reinterpret_cast<const char*>(aString);
        InitTextRun(run, utf8Chars, aLength, 0, PR_TRUE);
    } else {
        // this is really gross...
        const char *chars = reinterpret_cast<const char*>(aString);
        NS_ConvertASCIItoUTF16 unicodeString(chars, aLength);
        nsCAutoString utf8;
        PRInt32 headerLen = AppendDirectionalIndicatorUTF8(isRTL, utf8);
        AppendUTF16toUTF8(unicodeString, utf8);
        InitTextRun(run, utf8.get(), utf8.Length(), headerLen, PR_TRUE);
    }
    run->FetchGlyphExtents(aParams->mContext);
    return run;
}

gfxTextRun *
gfxQtFontGroup::MakeTextRun(const PRUnichar *aString, PRUint32 aLength,
                               const Parameters *aParams, PRUint32 aFlags)
{
    qDebug(">>>>>>Func:%s::%d\n", __PRETTY_FUNCTION__, __LINE__);
    gfxTextRun *run = gfxTextRun::Create(aParams, aString, aLength, this, aFlags);
    if (!run)
        return nsnull;

    run->RecordSurrogates(aString);

    nsCAutoString utf8;
    PRInt32 headerLen = AppendDirectionalIndicatorUTF8(run->IsRightToLeft(), utf8);
    AppendUTF16toUTF8(Substring(aString, aString + aLength), utf8);
    PRBool is8Bit = PR_FALSE;

#if defined(ENABLE_FAST_PATH_8BIT)
    if (CanTakeFastPath(aFlags)) {
        PRUint32 allBits = 0;
        PRUint32 i;
        for (i = 0; i < aLength; ++i) {
            allBits |= aString[i];
        }
        is8Bit = (allBits & 0xFF00) == 0;
    }
#endif
    InitTextRun(run, utf8.get(), utf8.Length(), headerLen, is8Bit);
    run->FetchGlyphExtents(aParams->mContext);
    return run;
}

/**
 * gfxQtFont
 */

gfxQtFont::gfxQtFont(const nsAString &aName,
                     const gfxFontStyle *aFontStyle)
    : gfxFont(aName, aFontStyle),
      mQFont(nsnull), mCairoFont(nsnull),
      mHasMetrics(PR_FALSE), mAdjustedSize(0)
{
    qDebug(">>>>>>Func:%s::%d, name:%s\n", __PRETTY_FUNCTION__, __LINE__, NS_ConvertUTF16toUTF8(aName).get());
}

gfxQtFont::~gfxQtFont()
{
    qDebug(">>>>>>Func:%s::%d\n", __PRETTY_FUNCTION__, __LINE__);
}

const gfxFont::Metrics&
gfxQtFont::GetMetrics()
{
    if (mHasMetrics)
        return mMetrics;

    qDebug(">>>>>>Func:%s::%d\n", __PRETTY_FUNCTION__, __LINE__);
#if 0
    //    printf("font name: %s %f %f\n", NS_ConvertUTF16toUTF8(mName).get(), GetStyle()->size, mAdjustedSize);
    //    printf ("pango font %s\n", pango_font_description_to_string (pango_font_describe (font)));

    fprintf (stderr, "Font: %s\n", NS_ConvertUTF16toUTF8(mName).get());
    fprintf (stderr, "    emHeight: %f emAscent: %f emDescent: %f\n", mMetrics.emHeight, mMetrics.emAscent, mMetrics.emDescent);
    fprintf (stderr, "    maxAscent: %f maxDescent: %f\n", mMetrics.maxAscent, mMetrics.maxDescent);
    fprintf (stderr, "    internalLeading: %f externalLeading: %f\n", mMetrics.externalLeading, mMetrics.internalLeading);
    fprintf (stderr, "    spaceWidth: %f aveCharWidth: %f xHeight: %f\n", mMetrics.spaceWidth, mMetrics.aveCharWidth, mMetrics.xHeight);
    fprintf (stderr, "    uOff: %f uSize: %f stOff: %f stSize: %f suOff: %f suSize: %f\n", mMetrics.underlineOffset, mMetrics.underlineSize, mMetrics.strikeoutOffset, mMetrics.strikeoutSize, mMetrics.superscriptOffset, mMetrics.subscriptOffset);
#endif

    mHasMetrics = PR_TRUE;
    return mMetrics;
}


nsString
gfxQtFont::GetUniqueName()
{
    qDebug(">>>>>>Func:%s::%d\n", __PRETTY_FUNCTION__, __LINE__);
    nsString result;
/*
    PangoFont *font = GetPangoFont();
    PangoFontDescription *desc = pango_font_describe(font);
    pango_font_description_unset_fields (desc, PANGO_FONT_MASK_SIZE);
    char *str = pango_font_description_to_string(desc);
    pango_font_description_free (desc);

    CopyUTF8toUTF16(str, result);
    g_free(str);*/
    return result;
}

/* static */ void
gfxQtFont::Shutdown()
{
    qDebug(">>>>>>Func:%s::%d\n", __PRETTY_FUNCTION__, __LINE__);
}

PRBool
gfxQtFont::SetupCairoFont(gfxContext *aContext)
{
    qDebug(">>>>>>Func:%s::%d\n", __PRETTY_FUNCTION__, __LINE__);
    cairo_t *cr = aContext->GetCairo();
    cairo_matrix_t currentCTM;
    cairo_get_matrix(cr, &currentCTM);

    if (mCairoFont) {
        // Need to validate that its CTM is OK
        cairo_matrix_t fontCTM;
        cairo_scaled_font_get_ctm(mCairoFont, &fontCTM);
        if (fontCTM.xx != currentCTM.xx || fontCTM.yy != currentCTM.yy ||
            fontCTM.xy != currentCTM.xy || fontCTM.yx != currentCTM.yx) {
            // Just recreate it from scratch, simplest way
            cairo_scaled_font_destroy(mCairoFont);
            mCairoFont = nsnull;
        }
    }
    if (!mCairoFont) {
        //mCairoFont = CreateScaledFont(cr, &currentCTM, GetPangoFont());
    }
    if (cairo_scaled_font_status(mCairoFont) != CAIRO_STATUS_SUCCESS) {
        // Don't cairo_set_scaled_font as that would propagate the error to
        // the cairo_t, precluding any further drawing.
        return PR_FALSE;
    }
    cairo_set_scaled_font(cr, mCairoFont);
    return PR_TRUE;
}
