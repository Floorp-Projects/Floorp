/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsString.h"
#include "gfxContext.h"
#include "gfxFontConstants.h"
#include "gfxHarfBuzzShaper.h"
#include "gfxFontUtils.h"
#include "gfxTextRun.h"
#include "mozilla/Sprintf.h"
#include "nsUnicodeProperties.h"
#include "nsUnicodeScriptCodes.h"
#include "nsUnicodeNormalizer.h"

#include "harfbuzz/hb.h"
#include "harfbuzz/hb-ot.h"

#if ENABLE_INTL_API // ICU is available: we'll use it for Unicode composition
                    // and decomposition in preference to nsUnicodeNormalizer.
#include "unicode/unorm.h"
#include "unicode/utext.h"
#define MOZ_HB_SHAPER_USE_ICU_NORMALIZATION 1
static const UNormalizer2 * sNormalizer = nullptr;
#else
#undef MOZ_HB_SHAPER_USE_ICU_NORMALIZATION
#endif

#include <algorithm>

#define FloatToFixed(f) (65536 * (f))
#define FixedToFloat(f) ((f) * (1.0 / 65536.0))
// Right shifts of negative (signed) integers are undefined, as are overflows
// when converting unsigned to negative signed integers.
// (If speed were an issue we could make some 2's complement assumptions.)
#define FixedToIntRound(f) ((f) > 0 ?  ((32768 + (f)) >> 16) \
                                    : -((32767 - (f)) >> 16))

using namespace mozilla; // for AutoSwap_* types
using namespace mozilla::unicode; // for Unicode property lookup

/*
 * Creation and destruction; on deletion, release any font tables we're holding
 */

gfxHarfBuzzShaper::gfxHarfBuzzShaper(gfxFont *aFont)
    : gfxFontShaper(aFont),
      mHBFace(aFont->GetFontEntry()->GetHBFace()),
      mHBFont(nullptr),
      mBuffer(nullptr),
      mKernTable(nullptr),
      mHmtxTable(nullptr),
      mVmtxTable(nullptr),
      mVORGTable(nullptr),
      mLocaTable(nullptr),
      mGlyfTable(nullptr),
      mCmapTable(nullptr),
      mCmapFormat(-1),
      mSubtableOffset(0),
      mUVSTableOffset(0),
      mNumLongHMetrics(0),
      mNumLongVMetrics(0),
      mUseFontGetGlyph(aFont->ProvidesGetGlyph()),
      mUseFontGlyphWidths(false),
      mInitialized(false),
      mVerticalInitialized(false),
      mLoadedLocaGlyf(false),
      mLocaLongOffsets(false)
{
}

gfxHarfBuzzShaper::~gfxHarfBuzzShaper()
{
    if (mCmapTable) {
        hb_blob_destroy(mCmapTable);
    }
    if (mHmtxTable) {
        hb_blob_destroy(mHmtxTable);
    }
    if (mKernTable) {
        hb_blob_destroy(mKernTable);
    }
    if (mVmtxTable) {
        hb_blob_destroy(mVmtxTable);
    }
    if (mVORGTable) {
        hb_blob_destroy(mVORGTable);
    }
    if (mLocaTable) {
        hb_blob_destroy(mLocaTable);
    }
    if (mGlyfTable) {
        hb_blob_destroy(mGlyfTable);
    }
    if (mHBFont) {
        hb_font_destroy(mHBFont);
    }
    if (mHBFace) {
        hb_face_destroy(mHBFace);
    }
    if (mBuffer) {
        hb_buffer_destroy(mBuffer);
    }
}

#define UNICODE_BMP_LIMIT 0x10000

hb_codepoint_t
gfxHarfBuzzShaper::GetNominalGlyph(hb_codepoint_t unicode) const
{
    hb_codepoint_t gid = 0;

    if (mUseFontGetGlyph) {
        gid = mFont->GetGlyph(unicode, 0);
    } else {
        // we only instantiate a harfbuzz shaper if there's a cmap available
        NS_ASSERTION(mFont->GetFontEntry()->HasCmapTable(),
                     "we cannot be using this font!");

        NS_ASSERTION(mCmapTable && (mCmapFormat > 0) && (mSubtableOffset > 0),
                     "cmap data not correctly set up, expect disaster");

        const uint8_t* data =
            (const uint8_t*)hb_blob_get_data(mCmapTable, nullptr);

        switch (mCmapFormat) {
        case 4:
            gid = unicode < UNICODE_BMP_LIMIT ?
                gfxFontUtils::MapCharToGlyphFormat4(data + mSubtableOffset,
                                                    unicode) : 0;
            break;
        case 10:
            gid = gfxFontUtils::MapCharToGlyphFormat10(data + mSubtableOffset,
                                                       unicode);
            break;
        case 12:
        case 13:
            gid =
                gfxFontUtils::MapCharToGlyphFormat12or13(data + mSubtableOffset,
                                                         unicode);
            break;
        default:
            NS_WARNING("unsupported cmap format, glyphs will be missing");
            break;
        }
    }

    if (!gid) {
        // if there's no glyph for &nbsp;, just use the space glyph instead
        if (unicode == 0xA0) {
            gid = mFont->GetSpaceGlyph();
        }
    }

    return gid;
}

hb_codepoint_t
gfxHarfBuzzShaper::GetVariationGlyph(hb_codepoint_t unicode,
                                     hb_codepoint_t variation_selector) const
{
    if (mUseFontGetGlyph) {
        return mFont->GetGlyph(unicode, variation_selector);
    }

    NS_ASSERTION(mFont->GetFontEntry()->HasCmapTable(),
                 "we cannot be using this font!");
    NS_ASSERTION(mCmapTable && (mCmapFormat > 0) && (mSubtableOffset > 0),
                 "cmap data not correctly set up, expect disaster");

    const uint8_t* data =
        (const uint8_t*)hb_blob_get_data(mCmapTable, nullptr);

    if (mUVSTableOffset) {
        hb_codepoint_t gid =
            gfxFontUtils::MapUVSToGlyphFormat14(data + mUVSTableOffset,
                                                unicode, variation_selector);
        if (gid) {
            return gid;
        }
    }

    uint32_t compat =
        gfxFontUtils::GetUVSFallback(unicode, variation_selector);
    if (compat) {
        switch (mCmapFormat) {
        case 4:
            if (compat < UNICODE_BMP_LIMIT) {
                return gfxFontUtils::MapCharToGlyphFormat4(data + mSubtableOffset,
                                                           compat);
            }
            break;
        case 10:
            return gfxFontUtils::MapCharToGlyphFormat10(data + mSubtableOffset,
                                                        compat);
            break;
        case 12:
        case 13:
            return
                gfxFontUtils::MapCharToGlyphFormat12or13(data + mSubtableOffset,
                                                         compat);
            break;
        }
    }

    return 0;
}

static int
VertFormsGlyphCompare(const void* aKey, const void* aElem)
{
    return int(*((hb_codepoint_t*)(aKey))) - int(*((uint16_t*)(aElem)));
}

// Return a vertical presentation-form codepoint corresponding to the
// given Unicode value, or 0 if no such form is available.
static hb_codepoint_t
GetVerticalPresentationForm(hb_codepoint_t unicode)
{
    static const uint16_t sVerticalForms[][2] = {
        { 0x2013, 0xfe32 }, // EN DASH
        { 0x2014, 0xfe31 }, // EM DASH
        { 0x2025, 0xfe30 }, // TWO DOT LEADER
        { 0x2026, 0xfe19 }, // HORIZONTAL ELLIPSIS
        { 0x3001, 0xfe11 }, // IDEOGRAPHIC COMMA
        { 0x3002, 0xfe12 }, // IDEOGRAPHIC FULL STOP
        { 0x3008, 0xfe3f }, // LEFT ANGLE BRACKET
        { 0x3009, 0xfe40 }, // RIGHT ANGLE BRACKET
        { 0x300a, 0xfe3d }, // LEFT DOUBLE ANGLE BRACKET
        { 0x300b, 0xfe3e }, // RIGHT DOUBLE ANGLE BRACKET
        { 0x300c, 0xfe41 }, // LEFT CORNER BRACKET
        { 0x300d, 0xfe42 }, // RIGHT CORNER BRACKET
        { 0x300e, 0xfe43 }, // LEFT WHITE CORNER BRACKET
        { 0x300f, 0xfe44 }, // RIGHT WHITE CORNER BRACKET
        { 0x3010, 0xfe3b }, // LEFT BLACK LENTICULAR BRACKET
        { 0x3011, 0xfe3c }, // RIGHT BLACK LENTICULAR BRACKET
        { 0x3014, 0xfe39 }, // LEFT TORTOISE SHELL BRACKET
        { 0x3015, 0xfe3a }, // RIGHT TORTOISE SHELL BRACKET
        { 0x3016, 0xfe17 }, // LEFT WHITE LENTICULAR BRACKET
        { 0x3017, 0xfe18 }, // RIGHT WHITE LENTICULAR BRACKET
        { 0xfe4f, 0xfe34 }, // WAVY LOW LINE
        { 0xff01, 0xfe15 }, // FULLWIDTH EXCLAMATION MARK
        { 0xff08, 0xfe35 }, // FULLWIDTH LEFT PARENTHESIS
        { 0xff09, 0xfe36 }, // FULLWIDTH RIGHT PARENTHESIS
        { 0xff0c, 0xfe10 }, // FULLWIDTH COMMA
        { 0xff1a, 0xfe13 }, // FULLWIDTH COLON
        { 0xff1b, 0xfe14 }, // FULLWIDTH SEMICOLON
        { 0xff1f, 0xfe16 }, // FULLWIDTH QUESTION MARK
        { 0xff3b, 0xfe47 }, // FULLWIDTH LEFT SQUARE BRACKET
        { 0xff3d, 0xfe48 }, // FULLWIDTH RIGHT SQUARE BRACKET
        { 0xff3f, 0xfe33 }, // FULLWIDTH LOW LINE
        { 0xff5b, 0xfe37 }, // FULLWIDTH LEFT CURLY BRACKET
        { 0xff5d, 0xfe38 }  // FULLWIDTH RIGHT CURLY BRACKET
    };
    const uint16_t* charPair =
        static_cast<const uint16_t*>(bsearch(&unicode,
                                             sVerticalForms,
                                             ArrayLength(sVerticalForms),
                                             sizeof(sVerticalForms[0]),
                                             VertFormsGlyphCompare));
    return charPair ? charPair[1] : 0;
}

static hb_bool_t
HBGetNominalGlyph(hb_font_t *font, void *font_data,
                  hb_codepoint_t unicode,
                  hb_codepoint_t *glyph,
                  void *user_data)
{
    const gfxHarfBuzzShaper::FontCallbackData *fcd =
        static_cast<const gfxHarfBuzzShaper::FontCallbackData*>(font_data);

    if (fcd->mShaper->UseVerticalPresentationForms()) {
        hb_codepoint_t verticalForm = GetVerticalPresentationForm(unicode);
        if (verticalForm) {
            *glyph = fcd->mShaper->GetNominalGlyph(verticalForm);
            if (*glyph != 0) {
                return true;
            }
        }
        // fall back to the non-vertical form if we didn't find an alternate
    }

    *glyph = fcd->mShaper->GetNominalGlyph(unicode);
    return *glyph != 0;
}

static hb_bool_t
HBGetVariationGlyph(hb_font_t *font, void *font_data,
                    hb_codepoint_t unicode, hb_codepoint_t variation_selector,
                    hb_codepoint_t *glyph,
                    void *user_data)
{
    const gfxHarfBuzzShaper::FontCallbackData *fcd =
        static_cast<const gfxHarfBuzzShaper::FontCallbackData*>(font_data);

    if (fcd->mShaper->UseVerticalPresentationForms()) {
        hb_codepoint_t verticalForm = GetVerticalPresentationForm(unicode);
        if (verticalForm) {
            *glyph = fcd->mShaper->GetVariationGlyph(verticalForm,
                                                     variation_selector);
            if (*glyph != 0) {
                return true;
            }
        }
        // fall back to the non-vertical form if we didn't find an alternate
    }

    *glyph = fcd->mShaper->GetVariationGlyph(unicode, variation_selector);
    return *glyph != 0;
}

// Glyph metrics structures, shared (with appropriate reinterpretation of
// field names) by horizontal and vertical metrics tables.
struct LongMetric {
    AutoSwap_PRUint16    advanceWidth; // or advanceHeight, when vertical
    AutoSwap_PRInt16     lsb;          // or tsb, when vertical
};

struct GlyphMetrics {
    LongMetric           metrics[1]; // actually numberOfLongMetrics
// the variable-length metrics[] array is immediately followed by:
//  AutoSwap_PRUint16    leftSideBearing[];
};

hb_position_t
gfxHarfBuzzShaper::GetGlyphHAdvance(hb_codepoint_t glyph) const
{
    // font did not implement GetGlyphWidth, so get an unhinted value
    // directly from the font tables

    NS_ASSERTION((mNumLongHMetrics > 0) && mHmtxTable != nullptr,
                 "font is lacking metrics, we shouldn't be here");

    if (glyph >= uint32_t(mNumLongHMetrics)) {
        glyph = mNumLongHMetrics - 1;
    }

    // glyph must be valid now, because we checked during initialization
    // that mNumLongHMetrics is > 0, and that the metrics table is large enough
    // to contain mNumLongHMetrics records
    const ::GlyphMetrics* metrics =
        reinterpret_cast<const ::GlyphMetrics*>(hb_blob_get_data(mHmtxTable,
                                                                 nullptr));
    return FloatToFixed(mFont->FUnitsToDevUnitsFactor() *
                        uint16_t(metrics->metrics[glyph].advanceWidth));
}

hb_position_t
gfxHarfBuzzShaper::GetGlyphVAdvance(hb_codepoint_t glyph) const
{
    if (!mVmtxTable) {
        // Must be a "vertical" font that doesn't actually have vertical metrics;
        // use a fixed advance.
        return FloatToFixed(mFont->GetMetrics(gfxFont::eVertical).aveCharWidth);
    }

    NS_ASSERTION(mNumLongVMetrics > 0,
                 "font is lacking metrics, we shouldn't be here");

    if (glyph >= uint32_t(mNumLongVMetrics)) {
        glyph = mNumLongVMetrics - 1;
    }

    // glyph must be valid now, because we checked during initialization
    // that mNumLongVMetrics is > 0, and that the metrics table is large enough
    // to contain mNumLongVMetrics records
    const ::GlyphMetrics* metrics =
        reinterpret_cast<const ::GlyphMetrics*>(hb_blob_get_data(mVmtxTable,
                                                                 nullptr));
    return FloatToFixed(mFont->FUnitsToDevUnitsFactor() *
                        uint16_t(metrics->metrics[glyph].advanceWidth));
}

/* static */
hb_position_t
gfxHarfBuzzShaper::HBGetGlyphHAdvance(hb_font_t *font, void *font_data,
                                      hb_codepoint_t glyph, void *user_data)
{
    const gfxHarfBuzzShaper::FontCallbackData *fcd =
        static_cast<const gfxHarfBuzzShaper::FontCallbackData*>(font_data);
    gfxFont *gfxfont = fcd->mShaper->GetFont();
    if (gfxfont->ProvidesGlyphWidths()) {
        return gfxfont->GetGlyphWidth(*fcd->mDrawTarget, glyph);
    }
    return fcd->mShaper->GetGlyphHAdvance(glyph);
}

/* static */
hb_position_t
gfxHarfBuzzShaper::HBGetGlyphVAdvance(hb_font_t *font, void *font_data,
                                      hb_codepoint_t glyph, void *user_data)
{
    const gfxHarfBuzzShaper::FontCallbackData *fcd =
        static_cast<const gfxHarfBuzzShaper::FontCallbackData*>(font_data);
    // Currently, we don't offer gfxFont subclasses a method to override this
    // and provide hinted platform-specific vertical advances (analogous to the
    // GetGlyphWidth method for horizontal advances). If that proves necessary,
    // we'll add a new gfxFont method and call it from here.
    return fcd->mShaper->GetGlyphVAdvance(glyph);
}

struct VORG {
    AutoSwap_PRUint16 majorVersion;
    AutoSwap_PRUint16 minorVersion;
    AutoSwap_PRInt16  defaultVertOriginY;
    AutoSwap_PRUint16 numVertOriginYMetrics;
};

struct VORGrec {
    AutoSwap_PRUint16 glyphIndex;
    AutoSwap_PRInt16  vertOriginY;
};

/* static */
hb_bool_t
gfxHarfBuzzShaper::HBGetGlyphVOrigin(hb_font_t *font, void *font_data,
                                     hb_codepoint_t glyph,
                                     hb_position_t *x, hb_position_t *y,
                                     void *user_data)
{
    const gfxHarfBuzzShaper::FontCallbackData *fcd =
        static_cast<const gfxHarfBuzzShaper::FontCallbackData*>(font_data);
    fcd->mShaper->GetGlyphVOrigin(glyph, x, y);
    return true;
}

void
gfxHarfBuzzShaper::GetGlyphVOrigin(hb_codepoint_t aGlyph,
                                   hb_position_t *aX, hb_position_t *aY) const
{
    *aX = -0.5 * GetGlyphHAdvance(aGlyph);

    if (mVORGTable) {
        // We checked in Initialize() that the VORG table is safely readable,
        // so no length/bounds-check needed here.
        const VORG* vorg =
            reinterpret_cast<const VORG*>(hb_blob_get_data(mVORGTable, nullptr));

        const VORGrec *lo = reinterpret_cast<const VORGrec*>(vorg + 1);
        const VORGrec *hi = lo + uint16_t(vorg->numVertOriginYMetrics);
        const VORGrec *limit = hi;
        while (lo < hi) {
            const VORGrec *mid = lo + (hi - lo) / 2;
            if (uint16_t(mid->glyphIndex) < aGlyph) {
                lo = mid + 1;
            } else {
                hi = mid;
            }
        }

        if (lo < limit && uint16_t(lo->glyphIndex) == aGlyph) {
            *aY = -FloatToFixed(GetFont()->FUnitsToDevUnitsFactor() *
                                int16_t(lo->vertOriginY));
        } else {
            *aY = -FloatToFixed(GetFont()->FUnitsToDevUnitsFactor() *
                                int16_t(vorg->defaultVertOriginY));
        }
        return;
    }

    if (mVmtxTable) {
        bool emptyGlyf;
        const Glyf *glyf = FindGlyf(aGlyph, &emptyGlyf);
        if (glyf) {
            if (emptyGlyf) {
                *aY = 0;
                return;
            }

            const ::GlyphMetrics* metrics =
                reinterpret_cast<const ::GlyphMetrics*>
                    (hb_blob_get_data(mVmtxTable, nullptr));
            int16_t lsb;
            if (aGlyph < hb_codepoint_t(mNumLongVMetrics)) {
                // Glyph is covered by the first (advance & sidebearing) array
                lsb = int16_t(metrics->metrics[aGlyph].lsb);
            } else {
                // Glyph is covered by the second (sidebearing-only) array
                const AutoSwap_PRInt16* sidebearings =
                    reinterpret_cast<const AutoSwap_PRInt16*>
                        (&metrics->metrics[mNumLongVMetrics]);
                lsb = int16_t(sidebearings[aGlyph - mNumLongVMetrics]);
            }
            *aY = -FloatToFixed(mFont->FUnitsToDevUnitsFactor() *
                                (lsb + int16_t(glyf->yMax)));
            return;
        } else {
            // XXX TODO: not a truetype font; need to get glyph extents
            // via some other API?
            // For now, fall through to default code below.
        }
    }

    // XXX should we consider using OS/2 sTypo* metrics if available?

    gfxFontEntry::AutoTable hheaTable(GetFont()->GetFontEntry(),
                                      TRUETYPE_TAG('h','h','e','a'));
    if (hheaTable) {
        uint32_t len;
        const MetricsHeader* hhea =
            reinterpret_cast<const MetricsHeader*>(hb_blob_get_data(hheaTable,
                                                                    &len));
        if (len >= sizeof(MetricsHeader)) {
            // divide up the default advance we're using (1em) in proportion
            // to ascender:descender from the hhea table
            int16_t a = int16_t(hhea->ascender);
            int16_t d = int16_t(hhea->descender);
            *aY = -FloatToFixed(GetFont()->GetAdjustedSize() * a / (a - d));
            return;
        }
    }

    NS_NOTREACHED("we shouldn't be here!");
    *aY = -FloatToFixed(GetFont()->GetAdjustedSize() / 2);
}

static hb_bool_t
HBGetGlyphExtents(hb_font_t *font, void *font_data,
                  hb_codepoint_t glyph,
                  hb_glyph_extents_t *extents,
                  void *user_data)
{
    const gfxHarfBuzzShaper::FontCallbackData *fcd =
        static_cast<const gfxHarfBuzzShaper::FontCallbackData*>(font_data);
    return fcd->mShaper->GetGlyphExtents(glyph, extents);
}

// Find the data for glyph ID |aGlyph| in the 'glyf' table, if present.
// Returns null if not found, otherwise pointer to the beginning of the
// glyph's data. Sets aEmptyGlyf true if there is no actual data;
// otherwise, it's guaranteed that we can read at least the bounding box.
const gfxHarfBuzzShaper::Glyf*
gfxHarfBuzzShaper::FindGlyf(hb_codepoint_t aGlyph, bool *aEmptyGlyf) const
{
    if (!mLoadedLocaGlyf) {
        mLoadedLocaGlyf = true; // only try this once; if it fails, this
                                // isn't a truetype font
        gfxFontEntry *entry = mFont->GetFontEntry();
        uint32_t len;
        gfxFontEntry::AutoTable headTable(entry,
                                          TRUETYPE_TAG('h','e','a','d'));
        if (!headTable) {
            return nullptr;
        }
        const HeadTable* head =
            reinterpret_cast<const HeadTable*>(hb_blob_get_data(headTable,
                                                                &len));
        if (len < sizeof(HeadTable)) {
            return nullptr;
        }
        mLocaLongOffsets = int16_t(head->indexToLocFormat) > 0;
        mLocaTable = entry->GetFontTable(TRUETYPE_TAG('l','o','c','a'));
        mGlyfTable = entry->GetFontTable(TRUETYPE_TAG('g','l','y','f'));
    }

    if (!mLocaTable || !mGlyfTable) {
        // it's not a truetype font
        return nullptr;
    }

    uint32_t offset; // offset of glyph record in the 'glyf' table
    uint32_t len;
    const char* data = hb_blob_get_data(mLocaTable, &len);
    if (mLocaLongOffsets) {
        if ((aGlyph + 1) * sizeof(AutoSwap_PRUint32) > len) {
            return nullptr;
        }
        const AutoSwap_PRUint32* offsets =
            reinterpret_cast<const AutoSwap_PRUint32*>(data);
        offset = offsets[aGlyph];
        *aEmptyGlyf = (offset == uint16_t(offsets[aGlyph + 1]));
    } else {
        if ((aGlyph + 1) * sizeof(AutoSwap_PRUint16) > len) {
            return nullptr;
        }
        const AutoSwap_PRUint16* offsets =
            reinterpret_cast<const AutoSwap_PRUint16*>(data);
        offset = uint16_t(offsets[aGlyph]);
        *aEmptyGlyf = (offset == uint16_t(offsets[aGlyph + 1]));
        offset *= 2;
    }

    data = hb_blob_get_data(mGlyfTable, &len);
    if (offset + sizeof(Glyf) > len) {
        return nullptr;
    }

    return reinterpret_cast<const Glyf*>(data + offset);
}

hb_bool_t
gfxHarfBuzzShaper::GetGlyphExtents(hb_codepoint_t aGlyph,
                                   hb_glyph_extents_t *aExtents) const
{
    bool emptyGlyf;
    const Glyf *glyf = FindGlyf(aGlyph, &emptyGlyf);
    if (!glyf) {
        // TODO: for non-truetype fonts, get extents some other way?
        return false;
    }

    if (emptyGlyf) {
        aExtents->x_bearing = 0;
        aExtents->y_bearing = 0;
        aExtents->width = 0;
        aExtents->height = 0;
        return true;
    }

    double f = mFont->FUnitsToDevUnitsFactor();
    aExtents->x_bearing = FloatToFixed(int16_t(glyf->xMin) * f);
    aExtents->width =
        FloatToFixed((int16_t(glyf->xMax) - int16_t(glyf->xMin)) * f);

    // Our y-coordinates are positive-downwards, whereas harfbuzz assumes
    // positive-upwards; hence the apparently-reversed subtractions here.
    aExtents->y_bearing =
        FloatToFixed(int16_t(glyf->yMax) * f -
                     mFont->GetHorizontalMetrics().emAscent);
    aExtents->height =
        FloatToFixed((int16_t(glyf->yMin) - int16_t(glyf->yMax)) * f);

    return true;
}

static hb_bool_t
HBGetContourPoint(hb_font_t *font, void *font_data,
                  unsigned int point_index, hb_codepoint_t glyph,
                  hb_position_t *x, hb_position_t *y,
                  void *user_data)
{
    /* not yet implemented - no support for used of hinted contour points
       to fine-tune anchor positions in GPOS AnchorFormat2 */
    return false;
}

struct KernHeaderFmt0 {
    AutoSwap_PRUint16 nPairs;
    AutoSwap_PRUint16 searchRange;
    AutoSwap_PRUint16 entrySelector;
    AutoSwap_PRUint16 rangeShift;
};

struct KernPair {
    AutoSwap_PRUint16 left;
    AutoSwap_PRUint16 right;
    AutoSwap_PRInt16  value;
};

// Find a kern pair in a Format 0 subtable.
// The aSubtable parameter points to the subtable itself, NOT its header,
// as the header structure differs between Windows and Mac (v0 and v1.0)
// versions of the 'kern' table.
// aSubtableLen is the length of the subtable EXCLUDING its header.
// If the pair <aFirstGlyph,aSecondGlyph> is found, the kerning value is
// added to aValue, so that multiple subtables can accumulate a total
// kerning value for a given pair.
static void
GetKernValueFmt0(const void* aSubtable,
                 uint32_t aSubtableLen,
                 uint16_t aFirstGlyph,
                 uint16_t aSecondGlyph,
                 int32_t& aValue,
                 bool     aIsOverride = false,
                 bool     aIsMinimum = false)
{
    const KernHeaderFmt0* hdr =
        reinterpret_cast<const KernHeaderFmt0*>(aSubtable);

    const KernPair *lo = reinterpret_cast<const KernPair*>(hdr + 1);
    const KernPair *hi = lo + uint16_t(hdr->nPairs);
    const KernPair *limit = hi;

    if (reinterpret_cast<const char*>(aSubtable) + aSubtableLen <
        reinterpret_cast<const char*>(hi)) {
        // subtable is not large enough to contain the claimed number
        // of kern pairs, so just ignore it
        return;
    }

#define KERN_PAIR_KEY(l,r) (uint32_t((uint16_t(l) << 16) + uint16_t(r)))

    uint32_t key = KERN_PAIR_KEY(aFirstGlyph, aSecondGlyph);
    while (lo < hi) {
        const KernPair *mid = lo + (hi - lo) / 2;
        if (KERN_PAIR_KEY(mid->left, mid->right) < key) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }

    if (lo < limit && KERN_PAIR_KEY(lo->left, lo->right) == key) {
        if (aIsOverride) {
            aValue = int16_t(lo->value);
        } else if (aIsMinimum) {
            aValue = std::max(aValue, int32_t(lo->value));
        } else {
            aValue += int16_t(lo->value);
        }
    }
}

// Get kerning value from Apple (version 1.0) kern table,
// subtable format 2 (simple N x M array of kerning values)

// See http://developer.apple.com/fonts/TTRefMan/RM06/Chap6kern.html
// for details of version 1.0 format 2 subtable.

struct KernHeaderVersion1Fmt2 {
    KernTableSubtableHeaderVersion1 header;
    AutoSwap_PRUint16 rowWidth;
    AutoSwap_PRUint16 leftOffsetTable;
    AutoSwap_PRUint16 rightOffsetTable;
    AutoSwap_PRUint16 array;
};

struct KernClassTableHdr {
    AutoSwap_PRUint16 firstGlyph;
    AutoSwap_PRUint16 nGlyphs;
    AutoSwap_PRUint16 offsets[1]; // actually an array of nGlyphs entries
};

static int16_t
GetKernValueVersion1Fmt2(const void* aSubtable,
                         uint32_t aSubtableLen,
                         uint16_t aFirstGlyph,
                         uint16_t aSecondGlyph)
{
    if (aSubtableLen < sizeof(KernHeaderVersion1Fmt2)) {
        return 0;
    }

    const char* base = reinterpret_cast<const char*>(aSubtable);
    const char* subtableEnd = base + aSubtableLen;

    const KernHeaderVersion1Fmt2* h =
        reinterpret_cast<const KernHeaderVersion1Fmt2*>(aSubtable);
    uint32_t offset = h->array;

    const KernClassTableHdr* leftClassTable =
        reinterpret_cast<const KernClassTableHdr*>(base +
                                                   uint16_t(h->leftOffsetTable));
    if (reinterpret_cast<const char*>(leftClassTable) +
        sizeof(KernClassTableHdr) > subtableEnd) {
        return 0;
    }
    if (aFirstGlyph >= uint16_t(leftClassTable->firstGlyph)) {
        aFirstGlyph -= uint16_t(leftClassTable->firstGlyph);
        if (aFirstGlyph < uint16_t(leftClassTable->nGlyphs)) {
            if (reinterpret_cast<const char*>(leftClassTable) +
                sizeof(KernClassTableHdr) +
                aFirstGlyph * sizeof(uint16_t) >= subtableEnd) {
                return 0;
            }
            offset = uint16_t(leftClassTable->offsets[aFirstGlyph]);
        }
    }

    const KernClassTableHdr* rightClassTable =
        reinterpret_cast<const KernClassTableHdr*>(base +
                                                   uint16_t(h->rightOffsetTable));
    if (reinterpret_cast<const char*>(rightClassTable) +
        sizeof(KernClassTableHdr) > subtableEnd) {
        return 0;
    }
    if (aSecondGlyph >= uint16_t(rightClassTable->firstGlyph)) {
        aSecondGlyph -= uint16_t(rightClassTable->firstGlyph);
        if (aSecondGlyph < uint16_t(rightClassTable->nGlyphs)) {
            if (reinterpret_cast<const char*>(rightClassTable) +
                sizeof(KernClassTableHdr) +
                aSecondGlyph * sizeof(uint16_t) >= subtableEnd) {
                return 0;
            }
            offset += uint16_t(rightClassTable->offsets[aSecondGlyph]);
        }
    }

    const AutoSwap_PRInt16* pval =
        reinterpret_cast<const AutoSwap_PRInt16*>(base + offset);
    if (reinterpret_cast<const char*>(pval + 1) >= subtableEnd) {
        return 0;
    }
    return *pval;
}

// Get kerning value from Apple (version 1.0) kern table,
// subtable format 3 (simple N x M array of kerning values)

// See http://developer.apple.com/fonts/TTRefMan/RM06/Chap6kern.html
// for details of version 1.0 format 3 subtable.

struct KernHeaderVersion1Fmt3 {
    KernTableSubtableHeaderVersion1 header;
    AutoSwap_PRUint16 glyphCount;
    uint8_t kernValueCount;
    uint8_t leftClassCount;
    uint8_t rightClassCount;
    uint8_t flags;
};

static int16_t
GetKernValueVersion1Fmt3(const void* aSubtable,
                         uint32_t aSubtableLen,
                         uint16_t aFirstGlyph,
                         uint16_t aSecondGlyph)
{
    // check that we can safely read the header fields
    if (aSubtableLen < sizeof(KernHeaderVersion1Fmt3)) {
        return 0;
    }

    const KernHeaderVersion1Fmt3* hdr =
        reinterpret_cast<const KernHeaderVersion1Fmt3*>(aSubtable);
    if (hdr->flags != 0) {
        return 0;
    }

    uint16_t glyphCount = hdr->glyphCount;

    // check that table is large enough for the arrays
    if (sizeof(KernHeaderVersion1Fmt3) +
        hdr->kernValueCount * sizeof(int16_t) +
        glyphCount + glyphCount +
        hdr->leftClassCount * hdr->rightClassCount > aSubtableLen) {
        return 0;
    }
        
    if (aFirstGlyph >= glyphCount || aSecondGlyph >= glyphCount) {
        // glyphs are out of range for the class tables
        return 0;
    }

    // get pointers to the four arrays within the subtable
    const AutoSwap_PRInt16* kernValue =
        reinterpret_cast<const AutoSwap_PRInt16*>(hdr + 1);
    const uint8_t* leftClass =
        reinterpret_cast<const uint8_t*>(kernValue + hdr->kernValueCount);
    const uint8_t* rightClass = leftClass + glyphCount;
    const uint8_t* kernIndex = rightClass + glyphCount;

    uint8_t lc = leftClass[aFirstGlyph];
    uint8_t rc = rightClass[aSecondGlyph];
    if (lc >= hdr->leftClassCount || rc >= hdr->rightClassCount) {
        return 0;
    }

    uint8_t ki = kernIndex[leftClass[aFirstGlyph] * hdr->rightClassCount +
                           rightClass[aSecondGlyph]];
    if (ki >= hdr->kernValueCount) {
        return 0;
    }

    return kernValue[ki];
}

#define KERN0_COVERAGE_HORIZONTAL   0x0001
#define KERN0_COVERAGE_MINIMUM      0x0002
#define KERN0_COVERAGE_CROSS_STREAM 0x0004
#define KERN0_COVERAGE_OVERRIDE     0x0008
#define KERN0_COVERAGE_RESERVED     0x00F0

#define KERN1_COVERAGE_VERTICAL     0x8000
#define KERN1_COVERAGE_CROSS_STREAM 0x4000
#define KERN1_COVERAGE_VARIATION    0x2000
#define KERN1_COVERAGE_RESERVED     0x1F00

hb_position_t
gfxHarfBuzzShaper::GetHKerning(uint16_t aFirstGlyph,
                               uint16_t aSecondGlyph) const
{
    // We want to ignore any kern pairs involving <space>, because we are
    // handling words in isolation, the only space characters seen here are
    // the ones artificially added by the textRun code.
    uint32_t spaceGlyph = mFont->GetSpaceGlyph();
    if (aFirstGlyph == spaceGlyph || aSecondGlyph == spaceGlyph) {
        return 0;
    }

    if (!mKernTable) {
        mKernTable = mFont->GetFontEntry()->GetFontTable(TRUETYPE_TAG('k','e','r','n'));
        if (!mKernTable) {
            mKernTable = hb_blob_get_empty();
        }
    }

    uint32_t len;
    const char* base = hb_blob_get_data(mKernTable, &len);
    if (len < sizeof(KernTableVersion0)) {
        return 0;
    }
    int32_t value = 0;

    // First try to interpret as "version 0" kern table
    // (see http://www.microsoft.com/typography/otspec/kern.htm)
    const KernTableVersion0* kern0 =
        reinterpret_cast<const KernTableVersion0*>(base);
    if (uint16_t(kern0->version) == 0) {
        uint16_t nTables = kern0->nTables;
        uint32_t offs = sizeof(KernTableVersion0);
        for (uint16_t i = 0; i < nTables; ++i) {
            if (offs + sizeof(KernTableSubtableHeaderVersion0) > len) {
                break;
            }
            const KernTableSubtableHeaderVersion0* st0 =
                reinterpret_cast<const KernTableSubtableHeaderVersion0*>
                                (base + offs);
            uint16_t subtableLen = uint16_t(st0->length);
            if (offs + subtableLen > len) {
                break;
            }
            offs += subtableLen;
            uint16_t coverage = st0->coverage;
            if (!(coverage & KERN0_COVERAGE_HORIZONTAL)) {
                // we only care about horizontal kerning (for now)
                continue;
            }
            if (coverage &
                (KERN0_COVERAGE_CROSS_STREAM | KERN0_COVERAGE_RESERVED)) {
                // we don't support cross-stream kerning, and
                // reserved bits should be zero;
                // ignore the subtable if not
                continue;
            }
            uint8_t format = (coverage >> 8);
            switch (format) {
            case 0:
                GetKernValueFmt0(st0 + 1, subtableLen - sizeof(*st0),
                                 aFirstGlyph, aSecondGlyph, value,
                                 (coverage & KERN0_COVERAGE_OVERRIDE) != 0,
                                 (coverage & KERN0_COVERAGE_MINIMUM) != 0);
                break;
            default:
                // TODO: implement support for other formats,
                // if they're ever used in practice
#if DEBUG
                {
                    char buf[1024];
                    SprintfLiteral(buf, "unknown kern subtable in %s: "
                                        "ver 0 format %d\n",
                                   NS_ConvertUTF16toUTF8(mFont->GetName()).get(),
                                   format);
                    NS_WARNING(buf);
                }
#endif
                break;
            }
        }
    } else {
        // It wasn't a "version 0" table; check if it is Apple version 1.0
        // (see http://developer.apple.com/fonts/TTRefMan/RM06/Chap6kern.html)
        const KernTableVersion1* kern1 =
            reinterpret_cast<const KernTableVersion1*>(base);
        if (uint32_t(kern1->version) == 0x00010000) {
            uint32_t nTables = kern1->nTables;
            uint32_t offs = sizeof(KernTableVersion1);
            for (uint32_t i = 0; i < nTables; ++i) {
                if (offs + sizeof(KernTableSubtableHeaderVersion1) > len) {
                    break;
                }
                const KernTableSubtableHeaderVersion1* st1 =
                    reinterpret_cast<const KernTableSubtableHeaderVersion1*>
                                    (base + offs);
                uint32_t subtableLen = uint32_t(st1->length);
                offs += subtableLen;
                uint16_t coverage = st1->coverage;
                if (coverage &
                    (KERN1_COVERAGE_VERTICAL     |
                     KERN1_COVERAGE_CROSS_STREAM |
                     KERN1_COVERAGE_VARIATION    |
                     KERN1_COVERAGE_RESERVED)) {
                    // we only care about horizontal kerning (for now),
                    // we don't support cross-stream kerning,
                    // we don't support variations,
                    // reserved bits should be zero;
                    // ignore the subtable if not
                    continue;
                }
                uint8_t format = (coverage & 0xff);
                switch (format) {
                case 0:
                    GetKernValueFmt0(st1 + 1, subtableLen - sizeof(*st1),
                                     aFirstGlyph, aSecondGlyph, value);
                    break;
                case 2:
                    value = GetKernValueVersion1Fmt2(st1, subtableLen,
                                                     aFirstGlyph, aSecondGlyph);
                    break;
                case 3:
                    value = GetKernValueVersion1Fmt3(st1, subtableLen,
                                                     aFirstGlyph, aSecondGlyph);
                    break;
                default:
                    // TODO: implement support for other formats.
                    // Note that format 1 cannot be supported here,
                    // as it requires the full glyph array to run the FSM,
                    // not just the current glyph pair.
#if DEBUG
                    {
                        char buf[1024];
                        SprintfLiteral(buf, "unknown kern subtable in %s: "
                                            "ver 0 format %d\n",
                                       NS_ConvertUTF16toUTF8(mFont->GetName()).get(),
                                       format);
                        NS_WARNING(buf);
                    }
#endif
                    break;
                }
            }
        }
    }

    if (value != 0) {
        return FloatToFixed(mFont->FUnitsToDevUnitsFactor() * value);
    }
    return 0;
}

static hb_position_t
HBGetHKerning(hb_font_t *font, void *font_data,
              hb_codepoint_t first_glyph, hb_codepoint_t second_glyph,
              void *user_data)
{
    const gfxHarfBuzzShaper::FontCallbackData *fcd =
        static_cast<const gfxHarfBuzzShaper::FontCallbackData*>(font_data);
    return fcd->mShaper->GetHKerning(first_glyph, second_glyph);
}

/*
 * HarfBuzz unicode property callbacks
 */

static hb_codepoint_t
HBGetMirroring(hb_unicode_funcs_t *ufuncs, hb_codepoint_t aCh,
               void *user_data)
{
    return GetMirroredChar(aCh);
}

static hb_unicode_general_category_t
HBGetGeneralCategory(hb_unicode_funcs_t *ufuncs, hb_codepoint_t aCh,
                     void *user_data)
{
    return hb_unicode_general_category_t(GetGeneralCategory(aCh));
}

static hb_script_t
HBGetScript(hb_unicode_funcs_t *ufuncs, hb_codepoint_t aCh, void *user_data)
{
    return hb_script_t(GetScriptTagForCode(GetScriptCode(aCh)));
}

static hb_unicode_combining_class_t
HBGetCombiningClass(hb_unicode_funcs_t *ufuncs, hb_codepoint_t aCh,
                    void *user_data)
{
    return hb_unicode_combining_class_t(GetCombiningClass(aCh));
}

// Hebrew presentation forms with dagesh, for characters 0x05D0..0x05EA;
// note that some letters do not have a dagesh presForm encoded
static const char16_t sDageshForms[0x05EA - 0x05D0 + 1] = {
    0xFB30, // ALEF
    0xFB31, // BET
    0xFB32, // GIMEL
    0xFB33, // DALET
    0xFB34, // HE
    0xFB35, // VAV
    0xFB36, // ZAYIN
    0, // HET
    0xFB38, // TET
    0xFB39, // YOD
    0xFB3A, // FINAL KAF
    0xFB3B, // KAF
    0xFB3C, // LAMED
    0, // FINAL MEM
    0xFB3E, // MEM
    0, // FINAL NUN
    0xFB40, // NUN
    0xFB41, // SAMEKH
    0, // AYIN
    0xFB43, // FINAL PE
    0xFB44, // PE
    0, // FINAL TSADI
    0xFB46, // TSADI
    0xFB47, // QOF
    0xFB48, // RESH
    0xFB49, // SHIN
    0xFB4A // TAV
};

static hb_bool_t
HBUnicodeCompose(hb_unicode_funcs_t *ufuncs,
                 hb_codepoint_t      a,
                 hb_codepoint_t      b,
                 hb_codepoint_t     *ab,
                 void               *user_data)
{
#if MOZ_HB_SHAPER_USE_ICU_NORMALIZATION

    if (sNormalizer) {
        UChar32 ch = unorm2_composePair(sNormalizer, a, b);
        if (ch >= 0) {
            *ab = ch;
            return true;
        }
    }

#else // no ICU available, use the old nsUnicodeNormalizer

    if (nsUnicodeNormalizer::Compose(a, b, ab)) {
        return true;
    }

#endif

    return false;
}

static hb_bool_t
HBUnicodeDecompose(hb_unicode_funcs_t *ufuncs,
                   hb_codepoint_t      ab,
                   hb_codepoint_t     *a,
                   hb_codepoint_t     *b,
                   void               *user_data)
{
#ifdef MOZ_WIDGET_ANDROID
    // Hack for the SamsungDevanagari font, bug 1012365:
    // support U+0972 by decomposing it.
    if (ab == 0x0972) {
        *a = 0x0905;
        *b = 0x0945;
        return true;
    }
#endif

#if MOZ_HB_SHAPER_USE_ICU_NORMALIZATION

    if (!sNormalizer) {
        return false;
    }

    // Canonical decompositions are never more than two characters,
    // or a maximum of 4 utf-16 code units.
    const unsigned MAX_DECOMP_LENGTH = 4;

    UErrorCode error = U_ZERO_ERROR;
    UChar decomp[MAX_DECOMP_LENGTH];
    int32_t len = unorm2_getRawDecomposition(sNormalizer, ab, decomp,
                                             MAX_DECOMP_LENGTH, &error);
    if (U_FAILURE(error) || len < 0) {
        return false;
    }

    UText text = UTEXT_INITIALIZER;
    utext_openUChars(&text, decomp, len, &error);
    NS_ASSERTION(U_SUCCESS(error), "UText failure?");

    UChar32 ch = UTEXT_NEXT32(&text);
    if (ch != U_SENTINEL) {
        *a = ch;
    }
    ch = UTEXT_NEXT32(&text);
    if (ch != U_SENTINEL) {
        *b = ch;
    }
    utext_close(&text);

    return *b != 0 || *a != ab;

#else // no ICU available, use the old nsUnicodeNormalizer

    return nsUnicodeNormalizer::DecomposeNonRecursively(ab, a, b);

#endif
}

static void
AddOpenTypeFeature(const uint32_t& aTag, uint32_t& aValue, void *aUserArg)
{
    nsTArray<hb_feature_t>* features = static_cast<nsTArray<hb_feature_t>*> (aUserArg);

    hb_feature_t feat = { 0, 0, 0, UINT_MAX };
    feat.tag = aTag;
    feat.value = aValue;
    features->AppendElement(feat);
}

/*
 * gfxFontShaper override to initialize the text run using HarfBuzz
 */

static hb_font_funcs_t * sHBFontFuncs = nullptr;
static hb_unicode_funcs_t * sHBUnicodeFuncs = nullptr;
static const hb_script_t sMathScript =
    hb_ot_tag_to_script(HB_TAG('m','a','t','h'));

bool
gfxHarfBuzzShaper::Initialize()
{
    if (mInitialized) {
        return mHBFont != nullptr;
    }
    mInitialized = true;
    mCallbackData.mShaper = this;

    mUseFontGlyphWidths = mFont->ProvidesGlyphWidths();

    if (!sHBFontFuncs) {
        // static function callback pointers, initialized by the first
        // harfbuzz shaper used
        sHBFontFuncs = hb_font_funcs_create();
        hb_font_funcs_set_nominal_glyph_func(sHBFontFuncs,
                                             HBGetNominalGlyph,
                                             nullptr, nullptr);
        hb_font_funcs_set_variation_glyph_func(sHBFontFuncs,
                                               HBGetVariationGlyph,
                                               nullptr, nullptr);
        hb_font_funcs_set_glyph_h_advance_func(sHBFontFuncs,
                                               HBGetGlyphHAdvance,
                                               nullptr, nullptr);
        hb_font_funcs_set_glyph_v_advance_func(sHBFontFuncs,
                                               HBGetGlyphVAdvance,
                                               nullptr, nullptr);
        hb_font_funcs_set_glyph_v_origin_func(sHBFontFuncs,
                                              HBGetGlyphVOrigin,
                                              nullptr, nullptr);
        hb_font_funcs_set_glyph_extents_func(sHBFontFuncs,
                                             HBGetGlyphExtents,
                                             nullptr, nullptr);
        hb_font_funcs_set_glyph_contour_point_func(sHBFontFuncs,
                                                   HBGetContourPoint,
                                                   nullptr, nullptr);
        hb_font_funcs_set_glyph_h_kerning_func(sHBFontFuncs,
                                               HBGetHKerning,
                                               nullptr, nullptr);

        sHBUnicodeFuncs =
            hb_unicode_funcs_create(hb_unicode_funcs_get_empty());
        hb_unicode_funcs_set_mirroring_func(sHBUnicodeFuncs,
                                            HBGetMirroring,
                                            nullptr, nullptr);
        hb_unicode_funcs_set_script_func(sHBUnicodeFuncs, HBGetScript,
                                         nullptr, nullptr);
        hb_unicode_funcs_set_general_category_func(sHBUnicodeFuncs,
                                                   HBGetGeneralCategory,
                                                   nullptr, nullptr);
        hb_unicode_funcs_set_combining_class_func(sHBUnicodeFuncs,
                                                  HBGetCombiningClass,
                                                  nullptr, nullptr);
        hb_unicode_funcs_set_compose_func(sHBUnicodeFuncs,
                                          HBUnicodeCompose,
                                          nullptr, nullptr);
        hb_unicode_funcs_set_decompose_func(sHBUnicodeFuncs,
                                            HBUnicodeDecompose,
                                            nullptr, nullptr);

#if MOZ_HB_SHAPER_USE_ICU_NORMALIZATION
        UErrorCode error = U_ZERO_ERROR;
        sNormalizer = unorm2_getNFCInstance(&error);
        NS_ASSERTION(U_SUCCESS(error), "failed to get ICU normalizer");
#endif
    }

    gfxFontEntry *entry = mFont->GetFontEntry();
    if (!mUseFontGetGlyph) {
        // get the cmap table and find offset to our subtable
        mCmapTable = entry->GetFontTable(TRUETYPE_TAG('c','m','a','p'));
        if (!mCmapTable) {
            NS_WARNING("failed to load cmap, glyphs will be missing");
            return false;
        }
        uint32_t len;
        const uint8_t* data = (const uint8_t*)hb_blob_get_data(mCmapTable, &len);
        bool symbol;
        mCmapFormat = gfxFontUtils::
            FindPreferredSubtable(data, len,
                                  &mSubtableOffset, &mUVSTableOffset,
                                  &symbol);
        if (mCmapFormat <= 0) {
            return false;
        }
    }

    if (!mUseFontGlyphWidths) {
        // If font doesn't implement GetGlyphWidth, we will be reading
        // the metrics table directly, so make sure we can load it.
        if (!LoadHmtxTable()) {
            return false;
        }
    }

    mBuffer = hb_buffer_create();
    hb_buffer_set_unicode_funcs(mBuffer, sHBUnicodeFuncs);
    hb_buffer_set_cluster_level(mBuffer,
                                HB_BUFFER_CLUSTER_LEVEL_MONOTONE_CHARACTERS);

    mHBFont = hb_font_create(mHBFace);
    hb_font_set_funcs(mHBFont, sHBFontFuncs, &mCallbackData, nullptr);
    hb_font_set_ppem(mHBFont, mFont->GetAdjustedSize(), mFont->GetAdjustedSize());
    uint32_t scale = FloatToFixed(mFont->GetAdjustedSize()); // 16.16 fixed-point
    hb_font_set_scale(mHBFont, scale, scale);

    return true;
}

bool
gfxHarfBuzzShaper::LoadHmtxTable()
{
    // Read mNumLongHMetrics from metrics-head table without caching its
    // blob, and preload/cache the metrics table.
    gfxFontEntry *entry = mFont->GetFontEntry();
    gfxFontEntry::AutoTable hheaTable(entry, TRUETYPE_TAG('h','h','e','a'));
    if (hheaTable) {
        uint32_t len;
        const MetricsHeader* hhea =
            reinterpret_cast<const MetricsHeader*>
            (hb_blob_get_data(hheaTable, &len));
        if (len >= sizeof(MetricsHeader)) {
            mNumLongHMetrics = hhea->numOfLongMetrics;
            if (mNumLongHMetrics > 0 &&
                int16_t(hhea->metricDataFormat) == 0) {
                // no point reading metrics if number of entries is zero!
                // in that case, we won't be able to use this font
                // (this method will return FALSE below if mHmtxTable
                // is null)
                mHmtxTable = entry->GetFontTable(TRUETYPE_TAG('h','m','t','x'));
                if (mHmtxTable && hb_blob_get_length(mHmtxTable) <
                    mNumLongHMetrics * sizeof(LongMetric)) {
                    // metrics table is not large enough for the claimed
                    // number of entries: invalid, do not use.
                    hb_blob_destroy(mHmtxTable);
                    mHmtxTable = nullptr;
                }
            }
        }
    }
    if (!mHmtxTable) {
        return false;
    }
    return true;
}

bool
gfxHarfBuzzShaper::InitializeVertical()
{
    // We only try this once. If we don't have a mHmtxTable after that,
    // this font can't handle vertical shaping, so return false.
    if (mVerticalInitialized) {
        return mHmtxTable != nullptr;
    }
    mVerticalInitialized = true;

    if (!mHmtxTable) {
        if (!LoadHmtxTable()) {
            return false;
        }
    }

    // Load vertical metrics if present in the font; if not, we'll synthesize
    // vertical glyph advances based on (horizontal) ascent/descent metrics.
    gfxFontEntry *entry = mFont->GetFontEntry();
    gfxFontEntry::AutoTable vheaTable(entry, TRUETYPE_TAG('v','h','e','a'));
    if (vheaTable) {
        uint32_t len;
        const MetricsHeader* vhea =
            reinterpret_cast<const MetricsHeader*>
            (hb_blob_get_data(vheaTable, &len));
        if (len >= sizeof(MetricsHeader)) {
            mNumLongVMetrics = vhea->numOfLongMetrics;
            gfxFontEntry::AutoTable
                maxpTable(entry, TRUETYPE_TAG('m','a','x','p'));
            int numGlyphs = -1; // invalid if we fail to read 'maxp'
            if (maxpTable &&
                hb_blob_get_length(maxpTable) >= sizeof(MaxpTableHeader)) {
                const MaxpTableHeader* maxp =
                    reinterpret_cast<const MaxpTableHeader*>
                    (hb_blob_get_data(maxpTable, nullptr));
                numGlyphs = uint16_t(maxp->numGlyphs);
            }
            if (mNumLongVMetrics > 0 && mNumLongVMetrics <= numGlyphs &&
                int16_t(vhea->metricDataFormat) == 0) {
                mVmtxTable = entry->GetFontTable(TRUETYPE_TAG('v','m','t','x'));
                if (mVmtxTable && hb_blob_get_length(mVmtxTable) <
                    mNumLongVMetrics * sizeof(LongMetric) +
                    (numGlyphs - mNumLongVMetrics) * sizeof(int16_t)) {
                    // metrics table is not large enough for the claimed
                    // number of entries: invalid, do not use.
                    hb_blob_destroy(mVmtxTable);
                    mVmtxTable = nullptr;
                }
            }
        }
    }

    // For CFF fonts only, load a VORG table if present.
    if (entry->HasFontTable(TRUETYPE_TAG('C','F','F',' '))) {
        mVORGTable = entry->GetFontTable(TRUETYPE_TAG('V','O','R','G'));
        if (mVORGTable) {
            uint32_t len;
            const VORG* vorg =
                reinterpret_cast<const VORG*>(hb_blob_get_data(mVORGTable,
                                                               &len));
            if (len < sizeof(VORG) ||
                uint16_t(vorg->majorVersion) != 1 ||
                uint16_t(vorg->minorVersion) != 0 ||
                len < sizeof(VORG) + uint16_t(vorg->numVertOriginYMetrics) *
                              sizeof(VORGrec)) {
                // VORG table is an unknown version, or not large enough
                // to be valid -- discard it.
                NS_WARNING("discarding invalid VORG table");
                hb_blob_destroy(mVORGTable);
                mVORGTable = nullptr;
            }
        }
    }

    return true;
}

bool
gfxHarfBuzzShaper::ShapeText(DrawTarget      *aDrawTarget,
                             const char16_t *aText,
                             uint32_t         aOffset,
                             uint32_t         aLength,
                             Script           aScript,
                             bool             aVertical,
                             RoundingFlags    aRounding,
                             gfxShapedText   *aShapedText)
{
    // some font back-ends require this in order to get proper hinted metrics
    if (!mFont->SetupCairoFont(aDrawTarget)) {
        return false;
    }

    mCallbackData.mDrawTarget = aDrawTarget;
    mUseVerticalPresentationForms = false;

    if (!Initialize()) {
        return false;
    }

    if (aVertical) {
        if (!InitializeVertical()) {
            return false;
        }
        if (!mFont->GetFontEntry()->
            SupportsOpenTypeFeature(aScript, HB_TAG('v','e','r','t'))) {
            mUseVerticalPresentationForms = true;
        }
    }

    const gfxFontStyle *style = mFont->GetStyle();

    // determine whether petite-caps falls back to small-caps
    bool addSmallCaps = false;
    if (style->variantCaps != NS_FONT_VARIANT_CAPS_NORMAL) {
        switch (style->variantCaps) {
            case NS_FONT_VARIANT_CAPS_ALLPETITE:
            case NS_FONT_VARIANT_CAPS_PETITECAPS:
                bool synLower, synUpper;
                mFont->SupportsVariantCaps(aScript, style->variantCaps,
                                           addSmallCaps, synLower, synUpper);
                break;
            default:
                break;
        }
    }

    gfxFontEntry *entry = mFont->GetFontEntry();

    // insert any merged features into hb_feature array
    AutoTArray<hb_feature_t,20> features;
    MergeFontFeatures(style,
                      entry->mFeatureSettings,
                      aShapedText->DisableLigatures(),
                      entry->FamilyName(),
                      addSmallCaps,
                      AddOpenTypeFeature,
                      &features);

    bool isRightToLeft = aShapedText->IsRightToLeft();
    hb_buffer_t *buffer = hb_buffer_create();
    hb_buffer_set_unicode_funcs(buffer, sHBUnicodeFuncs);

    hb_buffer_set_direction(mBuffer,
                            aVertical ? HB_DIRECTION_TTB :
                                        (isRightToLeft ? HB_DIRECTION_RTL :
                                                         HB_DIRECTION_LTR));
    hb_script_t scriptTag;
    if (aShapedText->GetFlags() & gfx::ShapedTextFlags::TEXT_USE_MATH_SCRIPT) {
        scriptTag = sMathScript;
    } else {
        scriptTag = GetHBScriptUsedForShaping(aScript);
    }
    hb_buffer_set_script(mBuffer, scriptTag);

    hb_language_t language;
    if (style->languageOverride) {
        language = hb_ot_tag_to_language(style->languageOverride);
    } else if (entry->mLanguageOverride) {
        language = hb_ot_tag_to_language(entry->mLanguageOverride);
    } else if (style->explicitLanguage) {
        nsCString langString;
        style->language->ToUTF8String(langString);
        language =
            hb_language_from_string(langString.get(), langString.Length());
    } else {
        language = hb_ot_tag_to_language(HB_OT_TAG_DEFAULT_LANGUAGE);
    }
    hb_buffer_set_language(mBuffer, language);

    uint32_t length = aLength;
    hb_buffer_add_utf16(mBuffer,
                        reinterpret_cast<const uint16_t*>(aText),
                        length, 0, length);

    hb_shape(mHBFont, mBuffer, features.Elements(), features.Length());

    if (isRightToLeft) {
        hb_buffer_reverse(mBuffer);
    }

    nsresult rv = SetGlyphsFromRun(aShapedText, aOffset, aLength,
                                   aText, aVertical, aRounding);

    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "failed to store glyphs into gfxShapedWord");
    hb_buffer_clear_contents(mBuffer);

    return NS_SUCCEEDED(rv);
}

#define SMALL_GLYPH_RUN 128 // some testing indicates that 90%+ of text runs
                            // will fit without requiring separate allocation
                            // for charToGlyphArray

nsresult
gfxHarfBuzzShaper::SetGlyphsFromRun(gfxShapedText  *aShapedText,
                                    uint32_t        aOffset,
                                    uint32_t        aLength,
                                    const char16_t *aText,
                                    bool            aVertical,
                                    RoundingFlags   aRounding)
{
    uint32_t numGlyphs;
    const hb_glyph_info_t *ginfo = hb_buffer_get_glyph_infos(mBuffer, &numGlyphs);
    if (numGlyphs == 0) {
        return NS_OK;
    }

    AutoTArray<gfxTextRun::DetailedGlyph,1> detailedGlyphs;

    uint32_t wordLength = aLength;
    static const int32_t NO_GLYPH = -1;
    AutoTArray<int32_t,SMALL_GLYPH_RUN> charToGlyphArray;
    if (!charToGlyphArray.SetLength(wordLength, fallible)) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    int32_t *charToGlyph = charToGlyphArray.Elements();
    for (uint32_t offset = 0; offset < wordLength; ++offset) {
        charToGlyph[offset] = NO_GLYPH;
    }

    for (uint32_t i = 0; i < numGlyphs; ++i) {
        uint32_t loc = ginfo[i].cluster;
        if (loc < wordLength) {
            charToGlyph[loc] = i;
        }
    }

    int32_t glyphStart = 0; // looking for a clump that starts at this glyph
    int32_t charStart = 0; // and this char index within the range of the run

    bool roundI, roundB;
    if (aVertical) {
        roundI = bool(aRounding & RoundingFlags::kRoundY);
        roundB = bool(aRounding & RoundingFlags::kRoundX);
    } else {
        roundI = bool(aRounding & RoundingFlags::kRoundX);
        roundB = bool(aRounding & RoundingFlags::kRoundY);
    }

    int32_t appUnitsPerDevUnit = aShapedText->GetAppUnitsPerDevUnit();
    gfxShapedText::CompressedGlyph *charGlyphs =
        aShapedText->GetCharacterGlyphs() + aOffset;

    // factor to convert 16.16 fixed-point pixels to app units
    // (only used if not rounding)
    double hb2appUnits = FixedToFloat(aShapedText->GetAppUnitsPerDevUnit());

    // Residual from rounding of previous advance, for use in rounding the
    // subsequent offset or advance appropriately.  16.16 fixed-point
    //
    // When rounding, the goal is to make the distance between glyphs and
    // their base glyph equal to the integral number of pixels closest to that
    // suggested by that shaper.
    // i.e. posInfo[n].x_advance - posInfo[n].x_offset + posInfo[n+1].x_offset
    //
    // The value of the residual is the part of the desired distance that has
    // not been included in integer offsets.
    hb_position_t residual = 0;

    // keep track of y-position to set glyph offsets if needed
    nscoord bPos = 0;

    const hb_glyph_position_t *posInfo =
        hb_buffer_get_glyph_positions(mBuffer, nullptr);

    while (glyphStart < int32_t(numGlyphs)) {

        int32_t charEnd = ginfo[glyphStart].cluster;
        int32_t glyphEnd = glyphStart;
        int32_t charLimit = wordLength;
        while (charEnd < charLimit) {
            // This is normally executed once for each iteration of the outer loop,
            // but in unusual cases where the character/glyph association is complex,
            // the initial character range might correspond to a non-contiguous
            // glyph range with "holes" in it. If so, we will repeat this loop to
            // extend the character range until we have a contiguous glyph sequence.
            charEnd += 1;
            while (charEnd != charLimit && charToGlyph[charEnd] == NO_GLYPH) {
                charEnd += 1;
            }

            // find the maximum glyph index covered by the clump so far
            for (int32_t i = charStart; i < charEnd; ++i) {
                if (charToGlyph[i] != NO_GLYPH) {
                    glyphEnd = std::max(glyphEnd, charToGlyph[i] + 1);
                    // update extent of glyph range
                }
            }

            if (glyphEnd == glyphStart + 1) {
                // for the common case of a single-glyph clump,
                // we can skip the following checks
                break;
            }

            if (glyphEnd == glyphStart) {
                // no glyphs, try to extend the clump
                continue;
            }

            // check whether all glyphs in the range are associated with the characters
            // in our clump; if not, we have a discontinuous range, and should extend it
            // unless we've reached the end of the text
            bool allGlyphsAreWithinCluster = true;
            for (int32_t i = glyphStart; i < glyphEnd; ++i) {
                int32_t glyphCharIndex = ginfo[i].cluster;
                if (glyphCharIndex < charStart || glyphCharIndex >= charEnd) {
                    allGlyphsAreWithinCluster = false;
                    break;
                }
            }
            if (allGlyphsAreWithinCluster) {
                break;
            }
        }

        NS_ASSERTION(glyphStart < glyphEnd,
                     "character/glyph clump contains no glyphs!");
        NS_ASSERTION(charStart != charEnd,
                     "character/glyph clump contains no characters!");

        // Now charStart..charEnd is a ligature clump, corresponding to glyphStart..glyphEnd;
        // Set baseCharIndex to the char we'll actually attach the glyphs to (1st of ligature),
        // and endCharIndex to the limit (position beyond the last char),
        // adjusting for the offset of the stringRange relative to the textRun.
        int32_t baseCharIndex, endCharIndex;
        while (charEnd < int32_t(wordLength) && charToGlyph[charEnd] == NO_GLYPH)
            charEnd++;
        baseCharIndex = charStart;
        endCharIndex = charEnd;

        // Then we check if the clump falls outside our actual string range;
        // if so, just go to the next.
        if (baseCharIndex >= int32_t(wordLength)) {
            glyphStart = glyphEnd;
            charStart = charEnd;
            continue;
        }
        // Ensure we won't try to go beyond the valid length of the textRun's text
        endCharIndex = std::min<int32_t>(endCharIndex, wordLength);

        // Now we're ready to set the glyph info in the textRun
        int32_t glyphsInClump = glyphEnd - glyphStart;

        // Check for default-ignorable char that didn't get filtered, combined,
        // etc by the shaping process, and remove from the run.
        // (This may be done within harfbuzz eventually.)
        if (glyphsInClump == 1 && baseCharIndex + 1 == endCharIndex &&
            aShapedText->FilterIfIgnorable(aOffset + baseCharIndex,
                                           aText[baseCharIndex])) {
            glyphStart = glyphEnd;
            charStart = charEnd;
            continue;
        }

        // HarfBuzz gives us physical x- and y-coordinates, but we will store
        // them as logical inline- and block-direction values in the textrun.

        hb_position_t i_offset, i_advance; // inline-direction offset/advance
        hb_position_t b_offset, b_advance; // block-direction offset/advance
        if (aVertical) {
            i_offset = posInfo[glyphStart].y_offset;
            i_advance = posInfo[glyphStart].y_advance;
            b_offset = posInfo[glyphStart].x_offset;
            b_advance = posInfo[glyphStart].x_advance;
        } else {
            i_offset = posInfo[glyphStart].x_offset;
            i_advance = posInfo[glyphStart].x_advance;
            b_offset = posInfo[glyphStart].y_offset;
            b_advance = posInfo[glyphStart].y_advance;
        }

        nscoord iOffset, advance;
        if (roundI) {
            iOffset =
                appUnitsPerDevUnit * FixedToIntRound(i_offset + residual);
            // Desired distance from the base glyph to the next reference point.
            hb_position_t width = i_advance - i_offset;
            int intWidth = FixedToIntRound(width);
            residual = width - FloatToFixed(intWidth);
            advance = appUnitsPerDevUnit * intWidth + iOffset;
        } else {
            iOffset = floor(hb2appUnits * i_offset + 0.5);
            advance = floor(hb2appUnits * i_advance + 0.5);
        }
        // Check if it's a simple one-to-one mapping
        if (glyphsInClump == 1 &&
            gfxTextRun::CompressedGlyph::IsSimpleGlyphID(ginfo[glyphStart].codepoint) &&
            gfxTextRun::CompressedGlyph::IsSimpleAdvance(advance) &&
            charGlyphs[baseCharIndex].IsClusterStart() &&
            iOffset == 0 && b_offset == 0 &&
            b_advance == 0 && bPos == 0)
        {
            charGlyphs[baseCharIndex].SetSimpleGlyph(advance,
                                                     ginfo[glyphStart].codepoint);
        } else {
            // Collect all glyphs in a list to be assigned to the first char;
            // there must be at least one in the clump, and we already measured
            // its advance, hence the placement of the loop-exit test and the
            // measurement of the next glyph.
            // For vertical orientation, we add a "base offset" to compensate
            // for the positioning within the cluster being based on horizontal
            // glyph origin/offset.
            hb_position_t baseIOffset, baseBOffset;
            if (aVertical) {
                baseIOffset = 2 * (i_offset - i_advance);
                baseBOffset = GetGlyphHAdvance(ginfo[glyphStart].codepoint);
            }
            while (1) {
                gfxTextRun::DetailedGlyph* details =
                    detailedGlyphs.AppendElement();
                details->mGlyphID = ginfo[glyphStart].codepoint;

                details->mXOffset = iOffset;
                details->mAdvance = advance;

                details->mYOffset = bPos -
                    (roundB ? appUnitsPerDevUnit * FixedToIntRound(b_offset)
                     : floor(hb2appUnits * b_offset + 0.5));

                if (b_advance != 0) {
                    bPos -=
                        roundB ? appUnitsPerDevUnit * FixedToIntRound(b_advance)
                        : floor(hb2appUnits * b_advance + 0.5);
                }
                if (++glyphStart >= glyphEnd) {
                    break;
                }

                if (aVertical) {
                    i_offset = baseIOffset - posInfo[glyphStart].y_offset;
                    i_advance = posInfo[glyphStart].y_advance;
                    b_offset = baseBOffset - posInfo[glyphStart].x_offset;
                    b_advance = posInfo[glyphStart].x_advance;
                } else {
                    i_offset = posInfo[glyphStart].x_offset;
                    i_advance = posInfo[glyphStart].x_advance;
                    b_offset = posInfo[glyphStart].y_offset;
                    b_advance = posInfo[glyphStart].y_advance;
                }

                if (roundI) {
                    iOffset = appUnitsPerDevUnit *
                        FixedToIntRound(i_offset + residual);
                    // Desired distance to the next reference point.  The
                    // residual is considered here, and includes the residual
                    // from the base glyph offset and subsequent advances, so
                    // that the distance from the base glyph is optimized
                    // rather than the distance from combining marks.
                    i_advance += residual;
                    int intAdvance = FixedToIntRound(i_advance);
                    residual = i_advance - FloatToFixed(intAdvance);
                    advance = appUnitsPerDevUnit * intAdvance;
                } else {
                    iOffset = floor(hb2appUnits * i_offset + 0.5);
                    advance = floor(hb2appUnits * i_advance + 0.5);
                }
            }

            gfxShapedText::CompressedGlyph g;
            g.SetComplex(charGlyphs[baseCharIndex].IsClusterStart(),
                         true, detailedGlyphs.Length());
            aShapedText->SetGlyphs(aOffset + baseCharIndex,
                                   g, detailedGlyphs.Elements());

            detailedGlyphs.Clear();
        }

        // the rest of the chars in the group are ligature continuations,
        // no associated glyphs
        while (++baseCharIndex != endCharIndex &&
               baseCharIndex < int32_t(wordLength)) {
            gfxShapedText::CompressedGlyph &g = charGlyphs[baseCharIndex];
            NS_ASSERTION(!g.IsSimpleGlyph(), "overwriting a simple glyph");
            g.SetComplex(g.IsClusterStart(), false, 0);
        }

        glyphStart = glyphEnd;
        charStart = charEnd;
    }

    return NS_OK;
}
