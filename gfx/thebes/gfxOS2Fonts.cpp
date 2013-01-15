/* vim: set sw=4 sts=4 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxContext.h"

#include "gfxOS2Platform.h"
#include "gfxOS2Surface.h"
#include "gfxOS2Fonts.h"
#include "nsTArray.h"
#include "nsGkAtoms.h"

#include "nsIPlatformCharset.h"

#include "mozilla/Preferences.h"
#include <algorithm>

using namespace mozilla;

/**********************************************************************
 * class gfxOS2Font
 **********************************************************************/

gfxOS2Font::gfxOS2Font(gfxOS2FontEntry *aFontEntry, const gfxFontStyle *aFontStyle)
    : gfxFont(aFontEntry, aFontStyle),
      mFontFace(nullptr),
      mMetrics(nullptr), mAdjustedSize(0),
      mHinting(FC_HINT_MEDIUM), mAntialias(FcTrue)
{
#ifdef DEBUG_thebes_2
    printf("gfxOS2Font[%p]::gfxOS2Font(%p \"%s\", aFontStyle)\n",
           (void *)this, (void *)aFontEntry,
           NS_LossyConvertUTF16toASCII(aFontEntry->Name()).get());
#endif
    // try to get the preferences for hinting, antialias, and embolden options
    int32_t value;
    nsresult rv = Preferences::GetInt("gfx.os2.font.hinting", &value);
    if (NS_SUCCEEDED(rv) && value >= FC_HINT_NONE && value <= FC_HINT_FULL) {
        mHinting = value;
    }

    mAntialias = Preferences::GetBool("gfx.os2.font.antialiasing", mAntialias);

#ifdef DEBUG_thebes_2
    printf("  font display options: hinting=%d, antialiasing=%s\n",
           mHinting, mAntialias ? "on" : "off");
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
    delete mMetrics;
    mFontFace = nullptr;
    mScaledFont = nullptr;
    mMetrics = nullptr;
}

// fill font metrics structure with default values in case of error
static void FillMetricsDefaults(gfxFont::Metrics *aMetrics)
{
    aMetrics->emAscent = 0.8 * aMetrics->emHeight;
    aMetrics->emDescent = 0.2 * aMetrics->emHeight;
    aMetrics->maxAscent = aMetrics->emAscent;
    aMetrics->maxDescent = aMetrics->maxDescent;
    aMetrics->maxHeight = aMetrics->emHeight;
    aMetrics->internalLeading = 0.0;
    aMetrics->externalLeading = 0.2 * aMetrics->emHeight;
    aMetrics->spaceWidth = 0.5 * aMetrics->emHeight;
    aMetrics->maxAdvance = aMetrics->spaceWidth;
    aMetrics->aveCharWidth = aMetrics->spaceWidth;
    aMetrics->zeroOrAveCharWidth = aMetrics->spaceWidth;
    aMetrics->xHeight = 0.5 * aMetrics->emHeight;
    aMetrics->underlineSize = aMetrics->emHeight / 14.0;
    aMetrics->underlineOffset = -aMetrics->underlineSize;
    aMetrics->strikeoutOffset = 0.25 * aMetrics->emHeight;
    aMetrics->strikeoutSize = aMetrics->underlineSize;
    aMetrics->superscriptOffset = aMetrics->xHeight;
    aMetrics->subscriptOffset = aMetrics->xHeight;
}

// Snap a line to pixels while keeping the center and size of the
// line as close to the original position as possible.
static void SnapLineToPixels(gfxFloat& aOffset, gfxFloat& aSize)
{
    gfxFloat snappedSize = std::max(floor(aSize + 0.5), 1.0);
    // Correct offset for change in size
    gfxFloat offset = aOffset - 0.5 * (aSize - snappedSize);
    // Snap offset
    aOffset = floor(offset + 0.5);
    aSize = snappedSize;
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
    if (mMetrics) {
        return *mMetrics;
    }

    // whatever happens below, we can always create the metrics
    mMetrics = new gfxFont::Metrics;
    mSpaceGlyph = 0;

    // round size to integer pixels, this is to get full pixels for layout
    // together with internal/external leading (see below)
    mMetrics->emHeight = floor(GetStyle()->size + 0.5);

    cairo_scaled_font_t* scaledFont = CairoScaledFont();
    if (!scaledFont) {
        FillMetricsDefaults(mMetrics);
        return *mMetrics;
    }

    FT_Face face = cairo_ft_scaled_font_lock_face(scaledFont);
    if (!face) {
        // Abort here already, otherwise we crash in the following
        // this can happen if the font-size requested is zero.
        FillMetricsDefaults(mMetrics);
        return *mMetrics;
    }
    if (!face->charmap) {
        // Also abort, if the charmap isn't loaded; then the char
        // lookups won't work. This happens for fonts without Unicode
        // charmap.
        cairo_ft_scaled_font_unlock_face(scaledFont);
        FillMetricsDefaults(mMetrics);
        return *mMetrics;
    }

    // compute font scaling factors
    gfxFloat emUnit = 1.0 * face->units_per_EM;
    gfxFloat xScale = face->size->metrics.x_ppem / emUnit;
    gfxFloat yScale = face->size->metrics.y_ppem / emUnit;

    FT_UInt gid; // glyph ID

    // properties of space
    gid = FT_Get_Char_Index(face, ' ');
    if (gid) {
        // Load glyph into glyph slot. Use load_default here to get results in
        // 26.6 fractional pixel format which is what is used for all other
        // characters in gfxOS2FontGroup::CreateGlyphRunsFT.
        FT_Load_Glyph(face, gid, FT_LOAD_DEFAULT);
        // glyph width doesn't work for spaces, use advance instead
        mMetrics->spaceWidth = face->glyph->advance.x >> 6;
        // save the space glyph
        mSpaceGlyph = gid;
    } else {
        NS_ASSERTION(gid, "this font doesn't have a space glyph!");
        mMetrics->spaceWidth = face->max_advance_width * xScale;
    }

    // properties of 'x', also use its width as average width
    gid = FT_Get_Char_Index(face, 'x'); // select the glyph
    if (gid) {
        // Load glyph into glyph slot. Here, use no_scale to get font units.
        FT_Load_Glyph(face, gid, FT_LOAD_NO_SCALE);
        mMetrics->xHeight = face->glyph->metrics.height * yScale;
        mMetrics->aveCharWidth = face->glyph->metrics.horiAdvance * xScale;
    } else {
        // this font doesn't have an 'x'...
        // fake these metrics using a fraction of the font size
        mMetrics->xHeight = mMetrics->emHeight * 0.5;
        mMetrics->aveCharWidth = mMetrics->emHeight * 0.5;
    }

    // properties of '0', for 'ch' units
    gid = FT_Get_Char_Index(face, '0');
    if (gid) {
        FT_Load_Glyph(face, gid, FT_LOAD_NO_SCALE);
        mMetrics->zeroOrAveCharWidth = face->glyph->metrics.horiAdvance * xScale;
    } else {
        // this font doesn't have a '0'
        mMetrics->zeroOrAveCharWidth = mMetrics->aveCharWidth;
    }

    // compute an adjusted size if we need to
    if (mAdjustedSize == 0 && GetStyle()->sizeAdjust != 0) {
        gfxFloat aspect = mMetrics->xHeight / GetStyle()->size;
        mAdjustedSize = GetStyle()->GetAdjustedSize(aspect);
        mMetrics->emHeight = mAdjustedSize;
    }

    // now load the OS/2 TrueType table to access some more properties
    TT_OS2 *os2 = (TT_OS2 *)FT_Get_Sfnt_Table(face, ft_sfnt_os2);
    if (os2 && os2->version != 0xFFFF) { // should be there if not old Mac font
        // if we are here we can improve the avgCharWidth
        mMetrics->aveCharWidth = std::max(mMetrics->aveCharWidth,
                                        os2->xAvgCharWidth * xScale);

        mMetrics->superscriptOffset = std::max(os2->ySuperscriptYOffset * yScale, 1.0);
        // some fonts have the incorrect sign (from gfxPangoFonts)
        mMetrics->subscriptOffset   = std::max(fabs(os2->ySubscriptYOffset * yScale),
                                             1.0);
        mMetrics->strikeoutOffset   = os2->yStrikeoutPosition * yScale;
        mMetrics->strikeoutSize     = os2->yStrikeoutSize * yScale;
    } else {
        // use fractions of emHeight instead of xHeight for these to be more robust
        mMetrics->superscriptOffset = mMetrics->emHeight * 0.5;
        mMetrics->subscriptOffset   = mMetrics->emHeight * 0.2;
        mMetrics->strikeoutOffset   = mMetrics->emHeight * 0.3;
        mMetrics->strikeoutSize     = face->underline_thickness * yScale;
    }
    SnapLineToPixels(mMetrics->strikeoutOffset, mMetrics->strikeoutSize);

    // seems that underlineOffset really has to be negative
    mMetrics->underlineOffset = face->underline_position * yScale;
    mMetrics->underlineSize   = face->underline_thickness * yScale;

    // descents are negative in FT but Thebes wants them positive
    mMetrics->emAscent        = face->ascender * yScale;
    mMetrics->emDescent       = -face->descender * yScale;
    mMetrics->maxHeight       = face->height * yScale;
    // the max units determine frame heights, better be generous
    mMetrics->maxAscent       = std::max(face->bbox.yMax * yScale,
                                       mMetrics->emAscent);
    mMetrics->maxDescent      = std::max(-face->bbox.yMin * yScale,
                                       mMetrics->emDescent);
    mMetrics->maxAdvance      = std::max(face->max_advance_width * xScale,
                                       mMetrics->aveCharWidth);

    // leadings are not available directly (only for WinFNTs);
    // better compute them on our own, to get integer values and make
    // layout happy (see // LockedFTFace::GetMetrics in gfxPangoFonts.cpp)
    mMetrics->internalLeading = floor(mMetrics->maxHeight
                                         - mMetrics->emHeight + 0.5);
    gfxFloat lineHeight = floor(mMetrics->maxHeight + 0.5);
    mMetrics->externalLeading = lineHeight
                              - mMetrics->internalLeading - mMetrics->emHeight;

    SanitizeMetrics(mMetrics, false);

#ifdef DEBUG_thebes_1
    printf("gfxOS2Font[%#x]::GetMetrics():\n"
           "  %s (%s)\n"
           "  emHeight=%f == %f=gfxFont::style.size == %f=adjSz\n"
           "  maxHeight=%f  xHeight=%f\n"
           "  aveCharWidth=%f(x) zeroOrAveWidth=%f(0) spaceWidth=%f\n"
           "  supOff=%f SubOff=%f   strOff=%f strSz=%f\n"
           "  undOff=%f undSz=%f    intLead=%f extLead=%f\n"
           "  emAsc=%f emDesc=%f maxH=%f\n"
           "  maxAsc=%f maxDes=%f maxAdv=%f\n",
           (unsigned)this,
           NS_LossyConvertUTF16toASCII(GetName()).get(),
           os2 && os2->version != 0xFFFF ? "has OS/2 table" : "no OS/2 table!",
           mMetrics->emHeight, GetStyle()->size, mAdjustedSize,
           mMetrics->maxHeight, mMetrics->xHeight,
           mMetrics->aveCharWidth, mMetrics->zeroOrAveCharWidth, mMetrics->spaceWidth,
           mMetrics->superscriptOffset, mMetrics->subscriptOffset,
           mMetrics->strikeoutOffset, mMetrics->strikeoutSize,
           mMetrics->underlineOffset, mMetrics->underlineSize,
           mMetrics->internalLeading, mMetrics->externalLeading,
           mMetrics->emAscent, mMetrics->emDescent, mMetrics->maxHeight,
           mMetrics->maxAscent, mMetrics->maxDescent, mMetrics->maxAdvance
          );
#endif
    cairo_ft_scaled_font_unlock_face(scaledFont);
    return *mMetrics;
}

// weight list copied from fontconfig.h
// unfortunately, the OS/2 version so far only supports regular and bold
static const int8_t nFcWeight = 2; // 10; // length of weight list
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
               (unsigned)this, NS_LossyConvertUTF16toASCII(GetName()).get(), GetStyle()->size);
#endif
        FcPattern *fcPattern = FcPatternCreate();

        // add (family) name to pattern
        // convert name because FC stores it in UTF8 while we have it in UTF16
        FcPatternAddString(fcPattern, FC_FAMILY,
                           (FcChar8 *)NS_ConvertUTF16toUTF8(GetName()).get());

        // The requirements outlined in gfxFont.h are difficult to meet without
        // having a table of available font weights, so we map the gfxFont
        // weight to possible FontConfig weights.
        int8_t weight = GetStyle()->ComputeWeight();
        // gfxFont weight   FC weight
        //    400              80
        //    700             200
        int16_t fcW = 40 * weight - 80; // match gfxFont weight to base FC weight
        // find the correct weight in the list
        int8_t i = 0;
        while (i < nFcWeight && fcWeight[i] < fcW) {
            i++;
        }
        if (i < 0) {
            i = 0;
        } else if (i >= nFcWeight) {
            i = nFcWeight - 1;
        }
        fcW = fcWeight[i];

        // add weight to pattern
        FcPatternAddInteger(fcPattern, FC_WEIGHT, fcW);

        uint8_t fcProperty;
        // add style to pattern
        switch (GetStyle()->style) {
        case NS_FONT_STYLE_ITALIC:
            fcProperty = FC_SLANT_ITALIC;
            break;
        case NS_FONT_STYLE_OBLIQUE:
            fcProperty = FC_SLANT_OBLIQUE;
            break;
        case NS_FONT_STYLE_NORMAL:
        default:
            fcProperty = FC_SLANT_ROMAN;
        }
        FcPatternAddInteger(fcPattern, FC_SLANT, fcProperty);

        // add the size we want
        FcPatternAddDouble(fcPattern, FC_PIXEL_SIZE,
                           mAdjustedSize ? mAdjustedSize : GetStyle()->size);

        // finally find a matching font
        FcResult fcRes;
        FcPattern *fcMatch = FcFontMatch(NULL, fcPattern, &fcRes);

        // Most code that depends on FcFontMatch() assumes it won't fail,
        // then crashes when it does.  For now, at least, substitute the
        // default serif font when it fails to avoid those crashes.
        if (!fcMatch) {
//#ifdef DEBUG
            printf("Could not match font for:\n"
                   "  family=%s, weight=%d, slant=%d, size=%f\n",
                   NS_LossyConvertUTF16toASCII(GetName()).get(),
                   GetStyle()->weight, GetStyle()->style, GetStyle()->size);
//#endif
            // FcPatternAddString() will free the existing FC_FAMILY string
            FcPatternAddString(fcPattern, FC_FAMILY, (FcChar8*)"SERIF");
            fcMatch = FcFontMatch(NULL, fcPattern, &fcRes);
//#ifdef DEBUG
            printf("Attempt to substitute default SERIF font %s\n",
                   fcMatch ? "succeeded" : "failed");
//#endif
        }
        FcPatternDestroy(fcPattern);

        if (fcMatch) {
            int w = FC_WEIGHT_REGULAR;
            FcPatternGetInteger(fcMatch, FC_WEIGHT, 0, &w);
            if (fcW >= FC_WEIGHT_DEMIBOLD && w < FC_WEIGHT_DEMIBOLD) {
                // if we want a bold font, but the selected font doesn't have a
                // bold counterpart, artificially embolden it
                FcPatternAddBool(fcMatch, FC_EMBOLDEN, FcTrue);
            }
            FcPatternAddBool(fcMatch, FC_ANTIALIAS, mAntialias);
            FcPatternAddInteger(fcMatch, FC_HINT_STYLE, mHinting);

            // and ask cairo to return a font face for this
            mFontFace = cairo_ft_font_face_create_for_pattern(fcMatch);

            FcPatternDestroy(fcMatch);
        }
    }

    NS_ASSERTION(mFontFace, "Failed to make font face");
    return mFontFace;
}

cairo_scaled_font_t *gfxOS2Font::CairoScaledFont()
{
#ifdef DEBUG_thebes_2
    printf("gfxOS2Font[%#x]::CairoScaledFont()\n", (unsigned)this);
#endif
    if (mScaledFont) {
        return mScaledFont;
    }
#ifdef DEBUG_thebes_2
    printf("gfxOS2Font[%#x]::CairoScaledFont(): create it for %s, %f\n",
           (unsigned)this, NS_LossyConvertUTF16toASCII(GetName()).get(), GetStyle()->size);
#endif

    double size = mAdjustedSize ? mAdjustedSize : GetStyle()->size;
    cairo_matrix_t identityMatrix;
    cairo_matrix_init_identity(&identityMatrix);
    cairo_matrix_t fontMatrix;
    // synthetic oblique by skewing via the font matrix
    if (!mFontEntry->mItalic &&
        (mStyle.style & (NS_FONT_STYLE_ITALIC | NS_FONT_STYLE_OBLIQUE)))
    {
        const double kSkewFactor = 0.2126; // 12 deg skew as used in e.g. ftview
        cairo_matrix_init(&fontMatrix, size, 0, -kSkewFactor*size, size, 0, 0);
    } else {
        cairo_matrix_init_scale(&fontMatrix, size, size);
    }

    cairo_font_face_t * face = CairoFontFace();
    if (!face)
        return nullptr;

    cairo_font_options_t *fontOptions = cairo_font_options_create();
    mScaledFont = cairo_scaled_font_create(face, &fontMatrix,
                                           &identityMatrix, fontOptions);
    cairo_font_options_destroy(fontOptions);

    NS_ASSERTION(cairo_scaled_font_status(mScaledFont) == CAIRO_STATUS_SUCCESS,
                 "Failed to make scaled font");
    return mScaledFont;
}

bool gfxOS2Font::SetupCairoFont(gfxContext *aContext)
{
#ifdef DEBUG_thebes_2
    printf("gfxOS2Font[%#x]::SetupCairoFont(%#x)\n",
           (unsigned)this, (unsigned) aContext);
#endif
    // gfxPangoFont checks the CTM but Windows doesn't so leave away here, too

    // this implicitely ensures that mScaledFont is created if NULL
    cairo_scaled_font_t *scaledFont = CairoScaledFont();
    if (!scaledFont || cairo_scaled_font_status(scaledFont) != CAIRO_STATUS_SUCCESS) {
        // Don't cairo_set_scaled_font as that would propagate the error to
        // the cairo_t, precluding any further drawing.
        return false;
    }
    cairo_set_scaled_font(aContext->GetCairo(), scaledFont);
    return true;
}

/**
 * Look up the font in the gfxFont cache. If we don't find it, create one.
 * In either case, add a ref and return it ---
 * except for OOM in which case we do nothing and return null.
 */
already_AddRefed<gfxOS2Font> gfxOS2Font::GetOrMakeFont(const nsAString& aName,
                                                       const gfxFontStyle *aStyle)
{
    nsRefPtr<gfxOS2FontEntry> fe = new gfxOS2FontEntry(aName);
    nsRefPtr<gfxFont> font =
      gfxFontCache::GetCache()->Lookup(static_cast<gfxFontEntry *>(fe), aStyle);
    if (!font) {
        font = new gfxOS2Font(fe, aStyle);
        if (!font)
            return nullptr;
        gfxFontCache::GetCache()->AddNew(font);
    }
    gfxFont *f = nullptr;
    font.swap(f);
    return static_cast<gfxOS2Font *>(f);
}

/**********************************************************************
 * class gfxOS2FontGroup
 **********************************************************************/

gfxOS2FontGroup::gfxOS2FontGroup(const nsAString& aFamilies,
                                 const gfxFontStyle* aStyle,
                                 gfxUserFontSet *aUserFontSet)
    : gfxFontGroup(aFamilies, aStyle, aUserFontSet)
{
#ifdef DEBUG_thebes_2
    printf("gfxOS2FontGroup[%#x]::gfxOS2FontGroup(\"%s\", %#x)\n",
           (unsigned)this, NS_LossyConvertUTF16toASCII(aFamilies).get(),
           (unsigned)aStyle);
#endif

    // check for WarpSans and as we cannot display that (yet), replace
    // it with Workplace Sans
    int pos = 0;
    if ((pos = mFamilies.Find("WarpSans", false, 0, -1)) > -1) {
        mFamilies.Replace(pos, 8, NS_LITERAL_STRING("Workplace Sans"));
    }

    nsTArray<nsString> familyArray;
    ForEachFont(FontCallback, &familyArray);

    // To be able to easily search for glyphs in other fonts, append a few good
    // replacement candidates to the list. The best ones are the Unicode fonts that
    // are set up, and if the user was so clever to set up the User Defined fonts,
    // then these are probable candidates, too.
    nsString fontString;
    gfxPlatform::GetPlatform()->GetPrefFonts(nsGkAtoms::Unicode, fontString, false);
    ForEachFont(fontString, nsGkAtoms::Unicode, FontCallback, &familyArray);
    gfxPlatform::GetPlatform()->GetPrefFonts(nsGkAtoms::x_user_def, fontString, false);
    ForEachFont(fontString, nsGkAtoms::x_user_def, FontCallback, &familyArray);

    // Should append some default font if there are no available fonts.
    // Let's use Helv which should be available on any OS/2 system; if
    // it's not there, Fontconfig replaces it with something else...
    if (familyArray.Length() == 0) {
        familyArray.AppendElement(NS_LITERAL_STRING("Helv"));
    }

    for (uint32_t i = 0; i < familyArray.Length(); i++) {
        nsRefPtr<gfxOS2Font> font = gfxOS2Font::GetOrMakeFont(familyArray[i], &mStyle);
        if (font) {
            mFonts.AppendElement(font);
        }
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
    return new gfxOS2FontGroup(mFamilies, aStyle, mUserFontSet);
}

/**
 * We use this to append an LTR or RTL Override character to the start of the
 * string. This forces Pango to honour our direction even if there are neutral
 * characters in the string.
 */
static int32_t AppendDirectionalIndicatorUTF8(bool aIsRTL, nsACString& aString)
{
    static const PRUnichar overrides[2][2] = { { 0x202d, 0 }, { 0x202e, 0 }}; // LRO, RLO
    AppendUTF16toUTF8(overrides[aIsRTL], aString);
    return 3; // both overrides map to 3 bytes in UTF8
}

gfxTextRun *gfxOS2FontGroup::MakeTextRun(const PRUnichar* aString, uint32_t aLength,
                                         const Parameters* aParams, uint32_t aFlags)
{
    NS_ASSERTION(aLength > 0, "should use MakeEmptyTextRun for zero-length text");
    gfxTextRun *textRun = gfxTextRun::Create(aParams, aLength, this, aFlags);
    if (!textRun)
        return nullptr;

    mEnableKerning = !(aFlags & gfxTextRunFactory::TEXT_OPTIMIZE_SPEED);

    nsAutoCString utf8;
    int32_t headerLen = AppendDirectionalIndicatorUTF8(textRun->IsRightToLeft(), utf8);
    AppendUTF16toUTF8(Substring(aString, aString + aLength), utf8);

#ifdef DEBUG_thebes_2
    NS_ConvertUTF8toUTF16 u16(utf8);
    printf("gfxOS2FontGroup[%#x]::MakeTextRun(PRUnichar %s, %d, %#x, %d)\n",
           (unsigned)this, NS_LossyConvertUTF16toASCII(u16).get(), aLength, (unsigned)aParams, aFlags);
#endif

    InitTextRun(textRun, (uint8_t *)utf8.get(), utf8.Length(), headerLen);

    textRun->FetchGlyphExtents(aParams->mContext);

    return textRun;
}

gfxTextRun *gfxOS2FontGroup::MakeTextRun(const uint8_t* aString, uint32_t aLength,
                                         const Parameters* aParams, uint32_t aFlags)
{
#ifdef DEBUG_thebes_2
    const char *cStr = reinterpret_cast<const char *>(aString);
    NS_ConvertASCIItoUTF16 us(cStr, aLength);
    printf("gfxOS2FontGroup[%#x]::MakeTextRun(uint8_t %s, %d, %#x, %d)\n",
           (unsigned)this, NS_LossyConvertUTF16toASCII(us).get(), aLength, (unsigned)aParams, aFlags);
#endif
    NS_ASSERTION(aLength > 0, "should use MakeEmptyTextRun for zero-length text");
    NS_ASSERTION(aFlags & TEXT_IS_8BIT, "8bit should have been set");
    gfxTextRun *textRun = gfxTextRun::Create(aParams, aLength, this, aFlags);
    if (!textRun)
        return nullptr;

    mEnableKerning = !(aFlags & gfxTextRunFactory::TEXT_OPTIMIZE_SPEED);

    const char *chars = reinterpret_cast<const char *>(aString);
    bool isRTL = textRun->IsRightToLeft();
    if ((aFlags & TEXT_IS_ASCII) && !isRTL) {
        // We don't need to send an override character here, the characters must be all
        // LTR
        InitTextRun(textRun, (uint8_t *)chars, aLength, 0);
    } else {
        // Although chars in not necessarily ASCII (as it may point to the low
        // bytes of any UCS-2 characters < 256), NS_ConvertASCIItoUTF16 seems
        // to DTRT.
        NS_ConvertASCIItoUTF16 unicodeString(chars, aLength);
        nsAutoCString utf8;
        int32_t headerLen = AppendDirectionalIndicatorUTF8(isRTL, utf8);
        AppendUTF16toUTF8(unicodeString, utf8);
        InitTextRun(textRun, (uint8_t *)utf8.get(), utf8.Length(), headerLen);
    }

    textRun->FetchGlyphExtents(aParams->mContext);

    return textRun;
}

void gfxOS2FontGroup::InitTextRun(gfxTextRun *aTextRun, const uint8_t *aUTF8Text,
                                  uint32_t aUTF8Length,
                                  uint32_t aUTF8HeaderLength)
{
    CreateGlyphRunsFT(aTextRun, aUTF8Text + aUTF8HeaderLength,
                      aUTF8Length - aUTF8HeaderLength);
}

// Helper function to return the leading UTF-8 character in a char pointer
// as 32bit number. Also sets the length of the current character (i.e. the
// offset to the next one) in the second argument
uint32_t getUTF8CharAndNext(const uint8_t *aString, uint8_t *aLength)
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

void gfxOS2FontGroup::CreateGlyphRunsFT(gfxTextRun *aTextRun, const uint8_t *aUTF8,
                                        uint32_t aUTF8Length)
{
#ifdef DEBUG_thebes_2
    printf("gfxOS2FontGroup::CreateGlyphRunsFT(%#x, _aUTF8_, %d)\n",
           (unsigned)aTextRun, /*aUTF8,*/ aUTF8Length);
    for (uint32_t i = 0; i < FontListLength(); i++) {
        gfxOS2Font *font = GetFontAt(i);
        printf("  i=%d, name=%s, size=%f\n", i, NS_LossyConvertUTF16toASCII(font->GetName()).get(),
               font->GetStyle()->size);
    }
#endif
    uint32_t lastFont = FontListLength()-1;
    gfxOS2Font *font0 = GetFontAt(0);
    const uint8_t *p = aUTF8;
    uint32_t utf16Offset = 0;
    gfxTextRun::CompressedGlyph g;
    const uint32_t appUnitsPerDevUnit = aTextRun->GetAppUnitsPerDevUnit();
    gfxOS2Platform *platform = gfxOS2Platform::GetPlatform();

    aTextRun->AddGlyphRun(font0, gfxTextRange::kFontGroup, 0, false);
    // a textRun likely has the same font for most of the characters, so we can
    // lock it before the loop for efficiency
    FT_Face face0 = cairo_ft_scaled_font_lock_face(font0->CairoScaledFont());
    while (p < aUTF8 + aUTF8Length) {
        bool glyphFound = false;
        // convert UTF-8 character and step to the next one in line
        uint8_t chLen;
        uint32_t ch = getUTF8CharAndNext(p, &chLen);
        p += chLen; // move to next char
#ifdef DEBUG_thebes_2
        printf("\'%c\' (%d, %#x, %s) [%#x %#x]:", (char)ch, ch, ch, ch >=0x10000 ? "non-BMP!" : "BMP", ch >=0x10000 ? H_SURROGATE(ch) : 0, ch >=0x10000 ? L_SURROGATE(ch) : 0);
#endif

        if (ch == 0 || platform->noFontWithChar(ch)) {
            // null bytes or missing characters cannot be displayed
            aTextRun->SetMissingGlyph(utf16Offset, ch);
        } else {
            // Try to get a glyph from all fonts available to us.
            // Once we found it in one of the fonts we quit the loop early.
            // If we don't find the glyph even in the last font, we will fall
            // back to searching all fonts on the system and finally set the
            // missing glyph symbol after trying the last font.
            for (uint32_t i = 0; i <= lastFont; i++) {
                gfxOS2Font *font = font0;
                FT_Face face = face0;
                if (i > 0) {
                    font = GetFontAt(i);
                    face = cairo_ft_scaled_font_lock_face(font->CairoScaledFont());
#ifdef DEBUG_thebes_2
                    if (i == lastFont) {
                        printf("Last font %d (%s) for ch=%#x (pos=%d)",
                               i, NS_LossyConvertUTF16toASCII(font->GetName()).get(), ch, utf16Offset);
                    }
#endif
                }
                if (!face || !face->charmap) { // don't try to use fonts with non-Unicode charmaps
                    if (face && face != face0)
                        cairo_ft_scaled_font_unlock_face(font->CairoScaledFont());
                    continue; // next font
                }

                NS_ASSERTION(!IsInvalidChar(ch), "Invalid char detected");
                FT_UInt gid = FT_Get_Char_Index(face, ch); // find the glyph id

                if (gid == 0 && i == lastFont) {
                    // missing glyph, try to find a replacement in another font
                    nsRefPtr<gfxOS2Font> fontX = platform->FindFontForChar(ch, font0);
                    if (fontX) {
                        font = fontX; // replace current font
                        cairo_ft_scaled_font_unlock_face(font->CairoScaledFont());
                        face = cairo_ft_scaled_font_lock_face(fontX->CairoScaledFont());
                        gid = FT_Get_Char_Index(face, ch);
                        // likely to find more chars in this font, append it
                        // to the font list to find it quicker next time
                        mFonts.AppendElement(fontX);
                        lastFont = FontListLength()-1;
                    }
                }

                // select the current font into the text run
                aTextRun->AddGlyphRun(font, gfxTextRange::kFontGroup, utf16Offset, false);

                int32_t advance = 0;
                if (gid == font->GetSpaceGlyph()) {
                    advance = (int)(font->GetMetrics().spaceWidth * appUnitsPerDevUnit);
                } else if (gid == 0) {
                    advance = -1; // trigger the missing glyphs case below
                } else {
                    // find next character and its glyph -- in case they exist
                    // and exist in the current font face -- to compute kerning
                    uint32_t chNext = 0;
                    FT_UInt gidNext = 0;
                    FT_Pos lsbDeltaNext = 0;
#ifdef DEBUG_thebes_2
                    printf("(kerning=%s/%s)", mEnableKerning ? "enable" : "disable", FT_HAS_KERNING(face) ? "yes" : "no");
#endif
                    if (mEnableKerning && FT_HAS_KERNING(face) && p < aUTF8 + aUTF8Length) {
                        chNext = getUTF8CharAndNext(p, &chLen);
                        if (chNext) {
                            gidNext = FT_Get_Char_Index(face, chNext);
                            if (gidNext && gidNext != font->GetSpaceGlyph()) {
                                FT_Load_Glyph(face, gidNext, FT_LOAD_DEFAULT);
                                lsbDeltaNext = face->glyph->lsb_delta;
                            }
                        }
                    }

                    // now load the current glyph
                    FT_Load_Glyph(face, gid, FT_LOAD_DEFAULT); // load glyph into the slot
                    advance = face->glyph->advance.x;

                    // now add kerning to the current glyph's advance
                    if (chNext && gidNext) {
                        FT_Vector kerning;
                        FT_Get_Kerning(face, gid, gidNext, FT_KERNING_DEFAULT, &kerning);
                        advance += kerning.x;
                        if (face->glyph->rsb_delta - lsbDeltaNext >= 32) {
                            advance -= 64;
                        } else if (face->glyph->rsb_delta - lsbDeltaNext < -32) {
                            advance += 64;
                        }
                    }

                    // now apply unit conversion and scaling
                    advance = (advance >> 6) * appUnitsPerDevUnit;
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
                    glyphFound = true;
                } else if (gid == 0) {
                    // gid = 0 only happens when the glyph is missing from the font
                    if (i == lastFont) {
                        // set the missing glyph only when it's missing from the very
                        // last font
                        aTextRun->SetMissingGlyph(utf16Offset, ch);
                    }
                    glyphFound = false;
                } else {
                    gfxTextRun::DetailedGlyph details;
                    details.mGlyphID = gid;
                    NS_ASSERTION(details.mGlyphID == gid, "Seriously weird glyph ID detected!");
                    details.mAdvance = advance;
                    details.mXOffset = 0;
                    details.mYOffset = 0;
                    g.SetComplex(aTextRun->IsClusterStart(utf16Offset), true, 1);
                    aTextRun->SetGlyphs(utf16Offset, g, &details);
                    glyphFound = true;
                }

                if (i > 0) {
                    cairo_ft_scaled_font_unlock_face(font->CairoScaledFont());
                }

                if (glyphFound) {
                    break;
                }
            }
        } // for all fonts

        NS_ASSERTION(!IS_SURROGATE(ch), "Surrogates shouldn't appear in UTF8");
        if (ch >= 0x10000) {
            // This character is a surrogate pair in UTF16
            ++utf16Offset;
        }

        ++utf16Offset;
    }
    cairo_ft_scaled_font_unlock_face(font0->CairoScaledFont());
}

// append aFontName to aClosure string array, if not already present
bool gfxOS2FontGroup::FontCallback(const nsAString& aFontName,
                                     const nsACString& aGenericName,
                                     bool aUseFontSet,
                                     void *aClosure)
{
    nsTArray<nsString> *sa = static_cast<nsTArray<nsString>*>(aClosure);
    if (!aFontName.IsEmpty() && !sa->Contains(aFontName)) {
        sa->AppendElement(aFontName);
    }
    return true;
}
