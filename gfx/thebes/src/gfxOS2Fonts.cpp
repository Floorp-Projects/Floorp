/* vim: set sw=4 sts=4 et cin: */
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
 * The Original Code is OS/2 code in Thebes.
 *
 * The Initial Developer of the Original Code is
 * Peter Weilbacher <mozilla@Weilbacher.org>.
 * Portions created by the Initial Developer are Copyright (C) 2006-2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   authors of code taken from gfxPangoFonts:
 *     Mozilla Foundation
 *     Vladimir Vukicevic <vladimir@mozilla.com>
 *     Masayuki Nakano <masayuki@d-toybox.com>
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

#include "gfxContext.h"

#include "gfxOS2Platform.h"
#include "gfxOS2Surface.h"
#include "gfxOS2Fonts.h"

#include "nsIServiceManager.h"
#include "nsIPlatformCharset.h"

/**********************************************************************
 * class gfxOS2Font
 **********************************************************************/

gfxOS2Font::gfxOS2Font(const nsAString &aName, const gfxFontStyle *aFontStyle)
    : gfxFont(aName, aFontStyle),
      mFontFace(nsnull), mScaledFont(nsnull),
      mMetrics(nsnull), mAdjustedSize(0)
{
#ifdef DEBUG_thebes_2
    printf("gfxOS2Font[%#x]::gfxOS2Font(\"%s\", aFontStyle)\n",
           (unsigned)this, NS_LossyConvertUTF16toASCII(aName).get());
#endif
}

gfxOS2Font::~gfxOS2Font()
{
#ifdef DEBUG_thebes_2
    printf("gfxOS2Font[%#x]::~gfxOS2Font()\n", (unsigned)this);
#endif
    if (mFontFace) {
        cairo_font_face_destroy(mFontFace);
    }
    if (mScaledFont) {
        cairo_scaled_font_destroy(mScaledFont);
    }
    if (mMetrics) {
        delete mMetrics;
    }
    mFontFace = nsnull;
    mScaledFont = nsnull;
    mMetrics = nsnull;
}

// gfxOS2Font::GetMetrics()
// return the metrics of the current font using the gfxFont metrics structure.
// If the metrics are not available yet, compute them using the FreeType
// function on the font. (This is partly based on the respective function from
// gfxPangoFonts)
const gfxFont::Metrics& gfxOS2Font::GetMetrics()
{
#ifdef DEBUG_thebes_1
    printf("gfxOS2Font[%#x]::GetMetrics()\n", (unsigned)this);
#endif
    if (!mMetrics) {
        mMetrics = new gfxFont::Metrics;

        mMetrics->emHeight = GetStyle()->size;

        FT_UInt gid; // glyph ID
        FT_Face face = cairo_ft_scaled_font_lock_face(CairoScaledFont());
        if (!face) {
            // Abort here already, otherwise we crash in the following
            // this can happen if the font-size requested is zero.
            // The metrics will be incomplete, but then we don't care.
            return *mMetrics;
        }

        double emUnit = 1.0 * face->units_per_EM;
        double xScale = face->size->metrics.x_ppem / emUnit;
        double yScale = face->size->metrics.y_ppem / emUnit;

        // properties of space
        gid = FT_Get_Char_Index(face, ' ');
        // load glyph into glyph slot, use no_scale to get font units
        FT_Load_Glyph(face, gid, FT_LOAD_NO_SCALE);
        // face->glyph->metrics.width doesn't work for spaces, use advance.x instead
        mMetrics->spaceWidth = face->glyph->advance.x * xScale;
        // save the space glyph
        mSpaceGlyph = gid;

        // properties of 'x', also use its width as average width
        gid = FT_Get_Char_Index(face, 'x'); // select the glyph
        if (gid) {
            FT_Load_Glyph(face, gid, FT_LOAD_NO_SCALE);
            mMetrics->xHeight = face->glyph->metrics.height * yScale;
            mMetrics->aveCharWidth = face->glyph->metrics.width * yScale;
        } else {
            // this font doesn't have an 'x'...
            // fake these metrics using a fraction of the font size
            mMetrics->xHeight = mMetrics->emHeight * 0.5;
            mMetrics->aveCharWidth = mMetrics->emHeight * 0.5;
        }

        // compute an adjusted size if we need to
        if (mAdjustedSize == 0 && GetStyle()->sizeAdjust != 0) {
            gfxFloat aspect = mMetrics->xHeight / GetStyle()->size;
            mAdjustedSize = GetStyle()->GetAdjustedSize(aspect);
            mMetrics->emHeight = mAdjustedSize;
        }

        // now load the OS/2 TrueType table to load access some more properties
        TT_OS2 *os2 = (TT_OS2 *)FT_Get_Sfnt_Table(face, ft_sfnt_os2);
        if (os2 && os2->version != 0xFFFF) { // should be there if not old Mac font
            // if we are here we can improve the avgCharWidth
            mMetrics->aveCharWidth = os2->xAvgCharWidth * xScale;

            mMetrics->superscriptOffset = os2->ySuperscriptYOffset * yScale;
            mMetrics->superscriptOffset = PR_MAX(1, mMetrics->superscriptOffset);
            // some fonts have the incorrect sign (from gfxPangoFonts)
            mMetrics->subscriptOffset   = fabs(os2->ySubscriptYOffset * yScale);
            mMetrics->subscriptOffset   = PR_MAX(1, fabs(mMetrics->subscriptOffset));
            mMetrics->strikeoutOffset   = os2->yStrikeoutPosition * yScale;
            mMetrics->strikeoutSize     = PR_MAX(1, os2->yStrikeoutSize * yScale);
        } else {
            // use fractions of emHeight instead of xHeight for these to be more robust
            mMetrics->superscriptOffset = mMetrics->emHeight * 0.5;
            mMetrics->subscriptOffset   = mMetrics->emHeight * 0.2;
            mMetrics->strikeoutOffset   = mMetrics->emHeight * 0.3;
            mMetrics->strikeoutSize     = face->underline_thickness * yScale;
        }
        // seems that underlineOffset really has to be negative
        mMetrics->underlineOffset = face->underline_position * yScale;
        mMetrics->underlineSize   = PR_MAX(1, face->underline_thickness * yScale);

        // descents are negative in FT but Thebes wants them positive
        mMetrics->emAscent        = face->ascender * yScale;
        mMetrics->emDescent       = -face->descender * yScale;
        mMetrics->maxHeight       = face->height * yScale;
        mMetrics->maxAscent       = face->bbox.yMax * yScale;
        mMetrics->maxDescent      = -face->bbox.yMin * yScale;
        mMetrics->maxAdvance      = face->max_advance_width * xScale;
        // leading are not available directly (only for WinFNTs)
        double lineHeight = mMetrics->maxAscent + mMetrics->maxDescent;
        if (lineHeight > mMetrics->emHeight) {
            mMetrics->internalLeading = lineHeight - mMetrics->emHeight;
        } else {
            mMetrics->internalLeading = 0;
        }
        mMetrics->externalLeading = 0; // normal value for OS/2 fonts, too

#ifdef DEBUG_thebes_1
        printf("gfxOS2Font[%#x]::GetMetrics():\n"
               "  %s (%s)\n"
               "  emHeight=%f == %f=gfxFont::mStyle.size == %f=adjSz\n"
               "  maxHeight=%f  xHeight=%f\n"
               "  aveCharWidth=%f==xWidth  spaceWidth=%f\n"
               "  supOff=%f SubOff=%f   strOff=%f strSz=%f\n"
               "  undOff=%f undSz=%f    intLead=%f extLead=%f\n"
               "  emAsc=%f emDesc=%f maxH=%f\n"
               "  maxAsc=%f maxDes=%f maxAdv=%f\n",
               (unsigned)this,
               NS_LossyConvertUTF16toASCII(mName).get(),
               os2 && os2->version != 0xFFFF ? "has OS/2 table" : "no OS/2 table!",
               mMetrics->emHeight, mStyle.size, mAdjustedSize,
               mMetrics->maxHeight, mMetrics->xHeight,
               mMetrics->aveCharWidth, mMetrics->spaceWidth,
               mMetrics->superscriptOffset, mMetrics->subscriptOffset,
               mMetrics->strikeoutOffset, mMetrics->strikeoutSize,
               mMetrics->underlineOffset, mMetrics->underlineSize,
               mMetrics->internalLeading, mMetrics->externalLeading,
               mMetrics->emAscent, mMetrics->emDescent, mMetrics->maxHeight,
               mMetrics->maxAscent, mMetrics->maxDescent, mMetrics->maxAdvance
              );
#endif
        cairo_ft_scaled_font_unlock_face(CairoScaledFont());
    }
    return *mMetrics;
}

// weight list copied from fontconfig.h
// unfortunately, the OS/2 version so far only supports regular and bold
static const PRInt8 nFcWeight = 2; // 10; // length of weight list
static const int fcWeight[] = {
    //FC_WEIGHT_THIN,
    //FC_WEIGHT_EXTRALIGHT, // == FC_WEIGHT_ULTRALIGHT
    //FC_WEIGHT_LIGHT,
    //FC_WEIGHT_BOOK,
    FC_WEIGHT_REGULAR, // == FC_WEIGHT_NORMAL
    //FC_WEIGHT_MEDIUM,
    //FC_WEIGHT_DEMIBOLD, // == FC_WEIGHT_SEMIBOLD
    FC_WEIGHT_BOLD,
    //FC_WEIGHT_EXTRABOLD, // == FC_WEIGHT_ULTRABOLD
    //FC_WEIGHT_BLACK // == FC_WEIGHT_HEAVY
};

// gfxOS2Font::CairoFontFace()
// return a font face usable by cairo for font rendering
// if none was created yet, use FontConfig patterns based on the current style
// to create a new font face
cairo_font_face_t *gfxOS2Font::CairoFontFace()
{
#ifdef DEBUG_thebes_2
    printf("gfxOS2Font[%#x]::CairoFontFace()\n", (unsigned)this);
#endif
    if (!mFontFace) {
#ifdef DEBUG_thebes
        printf("gfxOS2Font[%#x]::CairoFontFace(): create it for %s, %f\n",
               (unsigned)this, NS_LossyConvertUTF16toASCII(mName).get(), mStyle.size);
#endif
        FcPattern *fcPattern = FcPatternCreate();

        // add (family) name to pattern
        // (the conversion should work, font names don't contain high bit chars)
        FcPatternAddString(fcPattern, FC_FAMILY,
                           (FcChar8 *)NS_LossyConvertUTF16toASCII(mName).get());

        // adjust font weight using the offset
        // The requirements outlined in gfxFont.h are difficult to meet without
        // having a table of available font weights, so we map the gfxFont
        // weight to possible FontConfig weights.
        PRInt8 weight, offset;
        mStyle.ComputeWeightAndOffset(&weight, &offset);
        // gfxFont weight   FC weight
        //    400              80
        //    700             200
        PRInt16 fcW = 40 * weight - 80; // match gfxFont weight to base FC weight
        // find the correct weight in the list
        PRInt8 i = 0;
        while (i < nFcWeight && fcWeight[i] < fcW) {
            i++;
        }
        // add the offset, but observe the available number of weights
        i += offset;
        if (i < 0) {
            i = 0;
        } else if (i >= nFcWeight) {
            i = nFcWeight - 1;
        }
        fcW = fcWeight[i];

        // add weight to pattern
        FcPatternAddInteger(fcPattern, FC_WEIGHT, fcW);

        PRUint8 fcProperty;
        // add style to pattern
        switch (mStyle.style) {
        case FONT_STYLE_ITALIC:
            fcProperty = FC_SLANT_ITALIC;
            break;
        case FONT_STYLE_OBLIQUE:
            fcProperty = FC_SLANT_OBLIQUE;
            break;
        case FONT_STYLE_NORMAL:
        default:
            fcProperty = FC_SLANT_ROMAN;
        }
        FcPatternAddInteger(fcPattern, FC_SLANT, fcProperty);

        // add the size we want
        FcPatternAddDouble(fcPattern, FC_PIXEL_SIZE,
                           mAdjustedSize ? mAdjustedSize : mStyle.size);

        // finally find a matching font
        FcResult fcRes;
        FcPattern *fcMatch = FcFontMatch(NULL, fcPattern, &fcRes);
#ifdef DEBUG_thebes
        FcChar8 *str1, *str2;
        int w1, w2, i1, i2;
        double s1, s2;
        FcPatternGetString(fcPattern, FC_FAMILY, 0, &str1);
        FcPatternGetInteger(fcPattern, FC_WEIGHT, 0, &w1);
        FcPatternGetInteger(fcPattern, FC_SLANT, 0, &i1);
        FcPatternGetDouble(fcPattern, FC_PIXEL_SIZE, 0, &s1);
        FcPatternGetString(fcMatch, FC_FAMILY, 0, &str2);
        FcPatternGetInteger(fcMatch, FC_WEIGHT, 0, &w2);
        FcPatternGetInteger(fcMatch, FC_SLANT, 0, &i2);
        FcPatternGetDouble(fcMatch, FC_PIXEL_SIZE, 0, &s2);
        printf("  input=%s,%d,%d,%f\n  fcPattern=%s,%d,%d,%f\n  fcMatch=%s,%d,%d,%f\n",
               NS_LossyConvertUTF16toASCII(mName).get(), mStyle.weight, mStyle.style, mStyle.size,
               (char *)str1, w1, i1, s1,
               (char *)str2, w2, i2, s2);
#endif
        FcPatternDestroy(fcPattern);
        // and ask cairo to return a font face for this
        mFontFace = cairo_ft_font_face_create_for_pattern(fcMatch);
        FcPatternDestroy(fcMatch);
    }

    NS_ASSERTION(mFontFace, "Failed to make font face");
    return mFontFace;
}

cairo_scaled_font_t *gfxOS2Font::CairoScaledFont()
{
#ifdef DEBUG_thebes_2
    printf("gfxOS2Font[%#x]::CairoScaledFont()\n", (unsigned)this);
#endif
    if (!mScaledFont) {
#ifdef DEBUG_thebes_2
        printf("gfxOS2Font[%#x]::CairoScaledFont(): create it for %s, %f\n",
               (unsigned)this, NS_LossyConvertUTF16toASCII(mName).get(), mStyle.size);
#endif

        double size = mAdjustedSize ? mAdjustedSize : mStyle.size;
        cairo_matrix_t fontMatrix;
        cairo_matrix_init_scale(&fontMatrix, size, size);
        cairo_font_options_t *fontOptions = cairo_font_options_create();
        mScaledFont = cairo_scaled_font_create(CairoFontFace(), &fontMatrix,
                                               reinterpret_cast<cairo_matrix_t*>(&mCTM),
                                               fontOptions);
        cairo_font_options_destroy(fontOptions);
    }

    NS_ASSERTION(cairo_scaled_font_status(mScaledFont) == CAIRO_STATUS_SUCCESS,
                 "Failed to make scaled font");
    return mScaledFont;
}

nsString gfxOS2Font::GetUniqueName()
{
#ifdef DEBUG_thebes
    printf("gfxOS2Font::GetUniqueName()=%s\n", (char *)mName.get());
#endif
    // gfxFont::mName should already be unique enough
    // Atsui uses that, too, while Win appends size, and properties...
    // doesn't seem to get called at all anyway
    return mName;
}

PRBool gfxOS2Font::SetupCairoFont(gfxContext *aContext)
{
#ifdef DEBUG_thebes_2
    printf("gfxOS2Font[%#x]::SetupCairoFont(%#x)\n",
           (unsigned)this, (unsigned) aContext);
#endif
    // gfxPangoFont checks the CTM but Windows doesn't so leave away here, too

    // this implicitely ensures that mScaledFont is created if NULL
    cairo_scaled_font_t *scaledFont = CairoScaledFont();
    if (cairo_scaled_font_status(scaledFont) != CAIRO_STATUS_SUCCESS) {
        // Don't cairo_set_scaled_font as that would propagate the error to
        // the cairo_t, precluding any further drawing.
        return PR_FALSE;
    }
    cairo_set_scaled_font(aContext->GetCairo(), scaledFont);
    return PR_TRUE;
}

/**********************************************************************
 * class gfxOS2FontGroup
 **********************************************************************/
gfxOS2FontGroup::gfxOS2FontGroup(const nsAString& aFamilies,
                                 const gfxFontStyle* aStyle)
    : gfxFontGroup(aFamilies, aStyle)
{
#ifdef DEBUG_thebes_2
    printf("gfxOS2FontGroup[%#x]::gfxOS2FontGroup(\"%s\", %#x)\n",
           (unsigned)this, NS_LossyConvertUTF16toASCII(aFamilies).get(),
           (unsigned)aStyle);
#endif

    nsStringArray familyArray;
    mFontCache.Init(15);
    ForEachFont(FontCallback, &familyArray);
    FindGenericFontFromStyle(FontCallback, &familyArray);
    if (familyArray.Count() == 0) {
        // Should append default GUI font if there are no available fonts.
        // We use WarpSans as in the default case in nsSystemFontsOS2.
        familyArray.AppendString(NS_LITERAL_STRING("WarpSans"));
    }
    for (int i = 0; i < familyArray.Count(); i++) {
        mFonts.AppendElement(new gfxOS2Font(*familyArray[i], &mStyle));
    }
}

gfxOS2FontGroup::~gfxOS2FontGroup()
{
#ifdef DEBUG_thebes_2
    printf("gfxOS2FontGroup[%#x]::~gfxOS2FontGroup()\n", (unsigned)this);
#endif
}

gfxFontGroup *gfxOS2FontGroup::Copy(const gfxFontStyle *aStyle)
{
    return new gfxOS2FontGroup(mFamilies, aStyle);
}

/**
 * We use this to append an LTR or RTL Override character to the start of the
 * string. This forces Pango to honour our direction even if there are neutral
 * characters in the string.
 */
static PRInt32 AppendDirectionalIndicatorUTF8(PRBool aIsRTL, nsACString& aString)
{
    static const PRUnichar overrides[2][2] = { { 0x202d, 0 }, { 0x202e, 0 }}; // LRO, RLO
    AppendUTF16toUTF8(overrides[aIsRTL], aString);
    return 3; // both overrides map to 3 bytes in UTF8
}

gfxTextRun *gfxOS2FontGroup::MakeTextRun(const PRUnichar* aString, PRUint32 aLength,
                                         const Parameters* aParams, PRUint32 aFlags)
{
    gfxTextRun *textRun = gfxTextRun::Create(aParams, aString, aLength, this, aFlags);
    if (!textRun)
        return nsnull;

    textRun->RecordSurrogates(aString);

    nsCAutoString utf8;
    PRInt32 headerLen = AppendDirectionalIndicatorUTF8(textRun->IsRightToLeft(), utf8);
    AppendUTF16toUTF8(Substring(aString, aString + aLength), utf8);

#ifdef DEBUG_thebes_2
    NS_ConvertUTF8toUTF16 u16(utf8);
    printf("gfxOS2FontGroup[%#x]::MakeTextRun(PRUnichar %s, %d, %#x, %d)\n",
           (unsigned)this, NS_LossyConvertUTF16toASCII(u16).get(), aLength, (unsigned)aParams, aFlags);
#endif

    InitTextRun(textRun, (PRUint8 *)utf8.get(), utf8.Length(), headerLen);

    textRun->FetchGlyphExtents(aParams->mContext);

    return textRun;
}

gfxTextRun *gfxOS2FontGroup::MakeTextRun(const PRUint8* aString, PRUint32 aLength,
                                         const Parameters* aParams, PRUint32 aFlags)
{
#ifdef DEBUG_thebes_2
    const char *cStr = reinterpret_cast<const char *>(aString);
    NS_ConvertASCIItoUTF16 us(cStr, aLength);
    printf("gfxOS2FontGroup[%#x]::MakeTextRun(PRUint8 %s, %d, %#x, %d)\n",
           (unsigned)this, NS_LossyConvertUTF16toASCII(us).get(), aLength, (unsigned)aParams, aFlags);
#endif
    NS_ASSERTION(aFlags & TEXT_IS_8BIT, "8bit should have been set");
    gfxTextRun *textRun = gfxTextRun::Create(aParams, aString, aLength, this, aFlags);
    if (!textRun)
        return nsnull;

    const char *chars = reinterpret_cast<const char *>(aString);
    PRBool isRTL = textRun->IsRightToLeft();
    if ((aFlags & TEXT_IS_ASCII) && !isRTL) {
        // We don't need to send an override character here, the characters must be all
        // LTR
        InitTextRun(textRun, (PRUint8 *)chars, aLength, 0);
    } else {
        // Although chars in not necessarily ASCII (as it may point to the low
        // bytes of any UCS-2 characters < 256), NS_ConvertASCIItoUTF16 seems
        // to DTRT.
        NS_ConvertASCIItoUTF16 unicodeString(chars, aLength);
        nsCAutoString utf8;
        PRInt32 headerLen = AppendDirectionalIndicatorUTF8(isRTL, utf8);
        AppendUTF16toUTF8(unicodeString, utf8);
        InitTextRun(textRun, (PRUint8 *)utf8.get(), utf8.Length(), headerLen);
    }

    textRun->FetchGlyphExtents(aParams->mContext);

    return textRun;
}

void gfxOS2FontGroup::InitTextRun(gfxTextRun *aTextRun, const PRUint8 *aUTF8Text,
                                  PRUint32 aUTF8Length,
                                  PRUint32 aUTF8HeaderLength)
{
    CreateGlyphRunsFT(aTextRun, aUTF8Text + aUTF8HeaderLength,
                      aUTF8Length - aUTF8HeaderLength);
}

// Helper function to return the leading UTF-8 character in a char pointer
// as 32bit number. Also sets the length of the current character (i.e. the
// offset to the next one) in the second argument
PRUint32 getUTF8CharAndNext(const PRUint8 *aString, PRUint8 *aLength)
{
    *aLength = 1;
    if (aString[0] < 0x80) { // normal 7bit ASCII char
        return aString[0];
    }
    if ((aString[0] >> 5) == 6) { // two leading ones -> two bytes
        *aLength = 2;
        return ((aString[0] & 0x1F) << 6) + (aString[1] & 0x3F);
    }
    if ((aString[0] >> 4) == 14) { // three leading ones -> three bytes
        *aLength = 3;
        return ((aString[0] & 0x0F) << 12) + ((aString[1] & 0x3F) << 6) +
               (aString[2] & 0x3F);
    }
    if ((aString[0] >> 4) == 15) { // four leading ones -> four bytes
        *aLength = 4;
        return ((aString[0] & 0x07) << 18) + ((aString[1] & 0x3F) << 12) +
               ((aString[2] & 0x3F) <<  6) + (aString[3] & 0x3F);
    }
    return aString[0];
}

void gfxOS2FontGroup::CreateGlyphRunsFT(gfxTextRun *aTextRun, const PRUint8 *aUTF8,
                                        PRUint32 aUTF8Length)
{
#ifdef DEBUG_thebes_2
    printf("gfxOS2FontGroup::CreateGlyphRunsFT(%#x, _aUTF8_, %d)\n",
           (unsigned)aTextRun, /*aUTF8,*/ aUTF8Length);
    for (PRUint32 i = 0; i < FontListLength(); i++) {
        gfxOS2Font *font = GetFontAt(i);
        printf("  i=%d, name=%s, size=%f\n", i, NS_LossyConvertUTF16toASCII(font->GetName()).get(),
               font->GetStyle()->size);
    }
#endif
    const PRUint8 *p = aUTF8;
    gfxOS2Font *font = GetFontAt(0);
    PRUint32 utf16Offset = 0;
    gfxTextRun::CompressedGlyph g;
    const PRUint32 appUnitsPerDevUnit = aTextRun->GetAppUnitsPerDevUnit();

    aTextRun->AddGlyphRun(font, 0);
    // a textRun should have the same font, so we can lock it before the loop
    FT_Face face = cairo_ft_scaled_font_lock_face(font->CairoScaledFont());
    while (p < aUTF8 + aUTF8Length) {
        // convert UTF-8 character and step to the next one in line
        PRUint8 chLen;
        PRUint32 ch = getUTF8CharAndNext(p, &chLen);
        p += chLen; // move to next char
#ifdef DEBUG_thebes_2
        printf("\'%c\' (%d, %#x, %s):", (char)ch, ch, ch, ch >=0x10000 ? "non-BMP!" : "BMP");
#endif

        if (ch == 0) {
            // treat this null byte as a missing glyph, don't create a glyph for it
            aTextRun->SetMissingGlyph(utf16Offset, 0);
        } else {
            NS_ASSERTION(!IsInvalidChar(ch), "Invalid char detected");
            FT_UInt gid = FT_Get_Char_Index(face, ch); // find the glyph id
            PRInt32 advance = 0;
            if (gid == font->GetSpaceGlyph()) {
                advance = (int)(font->GetMetrics().spaceWidth * appUnitsPerDevUnit);
            } else if (gid == 0) {
                advance = -1; // trigger the missing glyphs case below
            } else {
                FT_Load_Glyph(face, gid, FT_LOAD_DEFAULT); // load glyph into the slot
                advance = (face->glyph->advance.x >> 6) * appUnitsPerDevUnit;
            }
#ifdef DEBUG_thebes_2
            printf(" gid=%d, advance=%d (%s)\n", gid, advance,
                   NS_LossyConvertUTF16toASCII(font->GetName()).get());
#endif

            if (advance >= 0 &&
                gfxTextRun::CompressedGlyph::IsSimpleAdvance(advance) &&
                gfxTextRun::CompressedGlyph::IsSimpleGlyphID(gid))
            {
                aTextRun->SetSimpleGlyph(utf16Offset,
                                         g.SetSimpleGlyph(advance, gid));
            } else if (gid == 0) {
                // gid = 0 only happens when the glyph is missing from the font
                aTextRun->SetMissingGlyph(utf16Offset, ch);
            } else {
                gfxTextRun::DetailedGlyph details;
                details.mGlyphID = gid;
                NS_ASSERTION(details.mGlyphID == gid, "Seriously weird glyph ID detected!");
                details.mAdvance = advance;
                details.mXOffset = 0;
                details.mYOffset = 0;
                g.SetComplex(aTextRun->IsClusterStart(utf16Offset), PR_TRUE, 1);
                aTextRun->SetGlyphs(utf16Offset, g, &details);
            }

            NS_ASSERTION(!IS_SURROGATE(ch), "Surrogates shouldn't appear in UTF8");
            if (ch >= 0x10000) {
                // This character is a surrogate pair in UTF16
                ++utf16Offset;
            }
        }
        ++utf16Offset;
    }
    cairo_ft_scaled_font_unlock_face(font->CairoScaledFont());
}

PRBool gfxOS2FontGroup::FontCallback(const nsAString& aFontName,
                                     const nsACString& aGenericName,
                                     void *aClosure)
{
    nsStringArray *sa = static_cast<nsStringArray*>(aClosure);
    if (sa->IndexOf(aFontName) < 0) {
        sa->AppendString(aFontName);
    }
    return PR_TRUE;
}
