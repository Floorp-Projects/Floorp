/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsString.h"
#include "gfxContext.h"
#include "gfxHarfBuzzShaper.h"
#include "gfxFontUtils.h"
#include "nsUnicodeProperties.h"
#include "nsUnicodeScriptCodes.h"
#include "nsUnicodeNormalizer.h"

#include "harfbuzz/hb.h"
#include "harfbuzz/hb-ot.h"

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
      mKernTable(nullptr),
      mHmtxTable(nullptr),
      mNumLongMetrics(0),
      mCmapTable(nullptr),
      mCmapFormat(-1),
      mSubtableOffset(0),
      mUVSTableOffset(0),
      mUseFontGetGlyph(aFont->ProvidesGetGlyph()),
      mUseFontGlyphWidths(false),
      mInitialized(false)
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
    if (mHBFont) {
        hb_font_destroy(mHBFont);
    }
    if (mHBFace) {
        hb_face_destroy(mHBFace);
    }
}

#define UNICODE_BMP_LIMIT 0x10000

hb_codepoint_t
gfxHarfBuzzShaper::GetGlyph(hb_codepoint_t unicode,
                            hb_codepoint_t variation_selector) const
{
    hb_codepoint_t gid = 0;

    if (mUseFontGetGlyph) {
        gid = mFont->GetGlyph(unicode, variation_selector);
    } else {
        // we only instantiate a harfbuzz shaper if there's a cmap available
        NS_ASSERTION(mFont->GetFontEntry()->HasCmapTable(),
                     "we cannot be using this font!");

        NS_ASSERTION(mCmapTable && (mCmapFormat > 0) && (mSubtableOffset > 0),
                     "cmap data not correctly set up, expect disaster");

        const uint8_t* data =
            (const uint8_t*)hb_blob_get_data(mCmapTable, nullptr);

        if (variation_selector) {
            if (mUVSTableOffset) {
                gid =
                    gfxFontUtils::MapUVSToGlyphFormat14(data + mUVSTableOffset,
                                                        unicode,
                                                        variation_selector);
            }
            // If the variation sequence was not supported, return zero here;
            // harfbuzz will call us again for the base character alone
            return gid;
        }

        switch (mCmapFormat) {
        case 4:
            gid = unicode < UNICODE_BMP_LIMIT ?
                gfxFontUtils::MapCharToGlyphFormat4(data + mSubtableOffset,
                                                    unicode) : 0;
            break;
        case 12:
            gid = gfxFontUtils::MapCharToGlyphFormat12(data + mSubtableOffset,
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

static hb_bool_t
HBGetGlyph(hb_font_t *font, void *font_data,
           hb_codepoint_t unicode, hb_codepoint_t variation_selector,
           hb_codepoint_t *glyph,
           void *user_data)
{
    const gfxHarfBuzzShaper::FontCallbackData *fcd =
        static_cast<const gfxHarfBuzzShaper::FontCallbackData*>(font_data);
    *glyph = fcd->mShaper->GetGlyph(unicode, variation_selector);
    return *glyph != 0;
}

struct HMetricsHeader {
    AutoSwap_PRUint32    tableVersionNumber;
    AutoSwap_PRInt16     ascender;
    AutoSwap_PRInt16     descender;
    AutoSwap_PRInt16     lineGap;
    AutoSwap_PRUint16    advanceWidthMax;
    AutoSwap_PRInt16     minLeftSideBearing;
    AutoSwap_PRInt16     minRightSideBearing;
    AutoSwap_PRInt16     xMaxExtent;
    AutoSwap_PRInt16     caretSlopeRise;
    AutoSwap_PRInt16     caretSlopeRun;
    AutoSwap_PRInt16     caretOffset;
    AutoSwap_PRInt16     reserved[4];
    AutoSwap_PRInt16     metricDataFormat;
    AutoSwap_PRUint16    numberOfHMetrics;
};

struct HLongMetric {
    AutoSwap_PRUint16    advanceWidth;
    AutoSwap_PRInt16     lsb;
};

struct HMetrics {
    HLongMetric          metrics[1]; // actually numberOfHMetrics
// the variable-length metrics[] array is immediately followed by:
//  AutoSwap_PRUint16    leftSideBearing[];
};

hb_position_t
gfxHarfBuzzShaper::GetGlyphHAdvance(gfxContext *aContext,
                                    hb_codepoint_t glyph) const
{
    if (mUseFontGlyphWidths) {
        return mFont->GetGlyphWidth(aContext, glyph);
    }

    // font did not implement GetHintedGlyphWidth, so get an unhinted value
    // directly from the font tables

    NS_ASSERTION((mNumLongMetrics > 0) && mHmtxTable != nullptr,
                 "font is lacking metrics, we shouldn't be here");

    if (glyph >= uint32_t(mNumLongMetrics)) {
        glyph = mNumLongMetrics - 1;
    }

    // glyph must be valid now, because we checked during initialization
    // that mNumLongMetrics is > 0, and that the hmtx table is large enough
    // to contain mNumLongMetrics records
    const HMetrics* hmtx =
        reinterpret_cast<const HMetrics*>(hb_blob_get_data(mHmtxTable, nullptr));
    return FloatToFixed(mFont->FUnitsToDevUnitsFactor() *
                        uint16_t(hmtx->metrics[glyph].advanceWidth));
}

static hb_position_t
HBGetGlyphHAdvance(hb_font_t *font, void *font_data,
                   hb_codepoint_t glyph, void *user_data)
{
    const gfxHarfBuzzShaper::FontCallbackData *fcd =
        static_cast<const gfxHarfBuzzShaper::FontCallbackData*>(font_data);
    return fcd->mShaper->GetGlyphHAdvance(fcd->mContext, glyph);
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
                    sprintf(buf, "unknown kern subtable in %s: "
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
                        sprintf(buf, "unknown kern subtable in %s: "
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

static unsigned int
HBGetEastAsianWidth(hb_unicode_funcs_t *ufuncs, hb_codepoint_t aCh,
                    void *user_data)
{
    return GetEastAsianWidth(aCh);
}

// Hebrew presentation forms with dagesh, for characters 0x05D0..0x05EA;
// note that some letters do not have a dagesh presForm encoded
static const PRUnichar sDageshForms[0x05EA - 0x05D0 + 1] = {
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
    hb_bool_t found = nsUnicodeNormalizer::Compose(a, b, ab);

    if (!found && (b & 0x1fff80) == 0x0580) {
        // special-case Hebrew presentation forms that are excluded from
        // standard normalization, but wanted for old fonts
        switch (b) {
        case 0x05B4: // HIRIQ
            if (a == 0x05D9) { // YOD
                *ab = 0xFB1D;
                found = true;
            }
            break;
        case 0x05B7: // patah
            if (a == 0x05F2) { // YIDDISH YOD YOD
                *ab = 0xFB1F;
                found = true;
            } else if (a == 0x05D0) { // ALEF
                *ab = 0xFB2E;
                found = true;
            }
            break;
        case 0x05B8: // QAMATS
            if (a == 0x05D0) { // ALEF
                *ab = 0xFB2F;
                found = true;
            }
            break;
        case 0x05B9: // HOLAM
            if (a == 0x05D5) { // VAV
                *ab = 0xFB4B;
                found = true;
            }
            break;
        case 0x05BC: // DAGESH
            if (a >= 0x05D0 && a <= 0x05EA) {
                *ab = sDageshForms[a - 0x05D0];
                found = (*ab != 0);
            } else if (a == 0xFB2A) { // SHIN WITH SHIN DOT
                *ab = 0xFB2C;
                found = true;
            } else if (a == 0xFB2B) { // SHIN WITH SIN DOT
                *ab = 0xFB2D;
                found = true;
            }
            break;
        case 0x05BF: // RAFE
            switch (a) {
            case 0x05D1: // BET
                *ab = 0xFB4C;
                found = true;
                break;
            case 0x05DB: // KAF
                *ab = 0xFB4D;
                found = true;
                break;
            case 0x05E4: // PE
                *ab = 0xFB4E;
                found = true;
                break;
            }
            break;
        case 0x05C1: // SHIN DOT
            if (a == 0x05E9) { // SHIN
                *ab = 0xFB2A;
                found = true;
            } else if (a == 0xFB49) { // SHIN WITH DAGESH
                *ab = 0xFB2C;
                found = true;
            }
            break;
        case 0x05C2: // SIN DOT
            if (a == 0x05E9) { // SHIN
                *ab = 0xFB2B;
                found = true;
            } else if (a == 0xFB49) { // SHIN WITH DAGESH
                *ab = 0xFB2D;
                found = true;
            }
            break;
        }
    }

    return found;
}

static hb_bool_t
HBUnicodeDecompose(hb_unicode_funcs_t *ufuncs,
                   hb_codepoint_t      ab,
                   hb_codepoint_t     *a,
                   hb_codepoint_t     *b,
                   void               *user_data)
{
    return nsUnicodeNormalizer::DecomposeNonRecursively(ab, a, b);
}

static PLDHashOperator
AddFeature(const uint32_t& aTag, uint32_t& aValue, void *aUserArg)
{
    nsTArray<hb_feature_t>* features = static_cast<nsTArray<hb_feature_t>*> (aUserArg);

    hb_feature_t feat = { 0, 0, 0, UINT_MAX };
    feat.tag = aTag;
    feat.value = aValue;
    features->AppendElement(feat);
    return PL_DHASH_NEXT;
}

/*
 * gfxFontShaper override to initialize the text run using HarfBuzz
 */

static hb_font_funcs_t * sHBFontFuncs = nullptr;
static hb_unicode_funcs_t * sHBUnicodeFuncs = nullptr;

bool
gfxHarfBuzzShaper::ShapeText(gfxContext      *aContext,
                             const PRUnichar *aText,
                             uint32_t         aOffset,
                             uint32_t         aLength,
                             int32_t          aScript,
                             gfxShapedText   *aShapedText)
{
    // some font back-ends require this in order to get proper hinted metrics
    if (!mFont->SetupCairoFont(aContext)) {
        return false;
    }

    mCallbackData.mContext = aContext;
    gfxFontEntry *entry = mFont->GetFontEntry();

    if (!mInitialized) {
        mInitialized = true;
        mCallbackData.mShaper = this;

        mUseFontGlyphWidths = mFont->ProvidesGlyphWidths();

        if (!sHBFontFuncs) {
            // static function callback pointers, initialized by the first
            // harfbuzz shaper used
            sHBFontFuncs = hb_font_funcs_create();
            hb_font_funcs_set_glyph_func(sHBFontFuncs, HBGetGlyph,
                                         nullptr, nullptr);
            hb_font_funcs_set_glyph_h_advance_func(sHBFontFuncs,
                                                   HBGetGlyphHAdvance,
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
            hb_unicode_funcs_set_eastasian_width_func(sHBUnicodeFuncs,
                                                      HBGetEastAsianWidth,
                                                      nullptr, nullptr);
            hb_unicode_funcs_set_compose_func(sHBUnicodeFuncs,
                                              HBUnicodeCompose,
                                              nullptr, nullptr);
            hb_unicode_funcs_set_decompose_func(sHBUnicodeFuncs,
                                                HBUnicodeDecompose,
                                                nullptr, nullptr);
        }

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
        }

        if (!mUseFontGlyphWidths) {
            // if font doesn't implement GetGlyphWidth, we will be reading
            // the hmtx table directly;
            // read mNumLongMetrics from hhea table without caching its blob,
            // and preload/cache the hmtx table
            gfxFontEntry::AutoTable hheaTable(entry, TRUETYPE_TAG('h','h','e','a'));
            if (hheaTable) {
                uint32_t len;
                const HMetricsHeader* hhea =
                    reinterpret_cast<const HMetricsHeader*>
                        (hb_blob_get_data(hheaTable, &len));
                if (len >= sizeof(HMetricsHeader)) {
                    mNumLongMetrics = hhea->numberOfHMetrics;
                    if (mNumLongMetrics > 0 &&
                        int16_t(hhea->metricDataFormat) == 0) {
                        // no point reading hmtx if number of entries is zero!
                        // in that case, we won't be able to use this font
                        // (this method will return FALSE below if mHmtx is null)
                        mHmtxTable =
                            entry->GetFontTable(TRUETYPE_TAG('h','m','t','x'));
                        if (hb_blob_get_length(mHmtxTable) <
                            mNumLongMetrics * sizeof(HLongMetric)) {
                            // hmtx table is not large enough for the claimed
                            // number of entries: invalid, do not use.
                            hb_blob_destroy(mHmtxTable);
                            mHmtxTable = nullptr;
                        }
                    }
                }
            }
        }

        mHBFont = hb_font_create(mHBFace);
        hb_font_set_funcs(mHBFont, sHBFontFuncs, &mCallbackData, nullptr);
        hb_font_set_ppem(mHBFont, mFont->GetAdjustedSize(), mFont->GetAdjustedSize());
        uint32_t scale = FloatToFixed(mFont->GetAdjustedSize()); // 16.16 fixed-point
        hb_font_set_scale(mHBFont, scale, scale);
    }

    if ((!mUseFontGetGlyph && mCmapFormat <= 0) ||
        (!mUseFontGlyphWidths && !mHmtxTable)) {
        // unable to shape with this font
        return false;
    }

    const gfxFontStyle *style = mFont->GetStyle();

    nsAutoTArray<hb_feature_t,20> features;
    nsDataHashtable<nsUint32HashKey,uint32_t> mergedFeatures;

    if (MergeFontFeatures(style,
                          entry->mFeatureSettings,
                          aShapedText->DisableLigatures(),
                          entry->FamilyName(),
                          mergedFeatures))
    {
        // enumerate result and insert into hb_feature array
        mergedFeatures.Enumerate(AddFeature, &features);
    }

    bool isRightToLeft = aShapedText->IsRightToLeft();
    hb_buffer_t *buffer = hb_buffer_create();
    hb_buffer_set_unicode_funcs(buffer, sHBUnicodeFuncs);
    hb_buffer_set_direction(buffer, isRightToLeft ? HB_DIRECTION_RTL :
                                                    HB_DIRECTION_LTR);
    // For unresolved "common" or "inherited" runs, default to Latin for now.
    // (Should we somehow use the language or locale to try and infer
    // a better default?)
    hb_script_t scriptTag = (aScript <= MOZ_SCRIPT_INHERITED) ?
        HB_SCRIPT_LATIN :
        hb_script_t(GetScriptTagForCode(aScript));
    hb_buffer_set_script(buffer, scriptTag);

    hb_language_t language;
    if (style->languageOverride) {
        language = hb_ot_tag_to_language(style->languageOverride);
    } else if (entry->mLanguageOverride) {
        language = hb_ot_tag_to_language(entry->mLanguageOverride);
    } else {
        nsCString langString;
        style->language->ToUTF8String(langString);
        language =
            hb_language_from_string(langString.get(), langString.Length());
    }
    hb_buffer_set_language(buffer, language);

    uint32_t length = aLength;
    hb_buffer_add_utf16(buffer,
                        reinterpret_cast<const uint16_t*>(aText),
                        length, 0, length);

    hb_shape(mHBFont, buffer, features.Elements(), features.Length());

    if (isRightToLeft) {
        hb_buffer_reverse(buffer);
    }

    nsresult rv = SetGlyphsFromRun(aContext, aShapedText, aOffset, aLength,
                                   aText, buffer);

    NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "failed to store glyphs into gfxShapedWord");
    hb_buffer_destroy(buffer);

    return NS_SUCCEEDED(rv);
}

#define SMALL_GLYPH_RUN 128 // some testing indicates that 90%+ of text runs
                            // will fit without requiring separate allocation
                            // for charToGlyphArray

nsresult
gfxHarfBuzzShaper::SetGlyphsFromRun(gfxContext      *aContext,
                                    gfxShapedText   *aShapedText,
                                    uint32_t         aOffset,
                                    uint32_t         aLength,
                                    const PRUnichar *aText,
                                    hb_buffer_t     *aBuffer)
{
    uint32_t numGlyphs;
    const hb_glyph_info_t *ginfo = hb_buffer_get_glyph_infos(aBuffer, &numGlyphs);
    if (numGlyphs == 0) {
        return NS_OK;
    }

    nsAutoTArray<gfxTextRun::DetailedGlyph,1> detailedGlyphs;

    uint32_t wordLength = aLength;
    static const int32_t NO_GLYPH = -1;
    nsAutoTArray<int32_t,SMALL_GLYPH_RUN> charToGlyphArray;
    if (!charToGlyphArray.SetLength(wordLength)) {
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

    bool roundX;
    bool roundY;
    aContext->GetRoundOffsetsToPixels(&roundX, &roundY);

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
    hb_position_t x_residual = 0;

    // keep track of y-position to set glyph offsets if needed
    nscoord yPos = 0;

    const hb_glyph_position_t *posInfo =
        hb_buffer_get_glyph_positions(aBuffer, nullptr);

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

        hb_position_t x_offset = posInfo[glyphStart].x_offset;
        hb_position_t x_advance = posInfo[glyphStart].x_advance;
        nscoord xOffset, advance;
        if (roundX) {
            xOffset =
                appUnitsPerDevUnit * FixedToIntRound(x_offset + x_residual);
            // Desired distance from the base glyph to the next reference point.
            hb_position_t width = x_advance - x_offset;
            int intWidth = FixedToIntRound(width);
            x_residual = width - FloatToFixed(intWidth);
            advance = appUnitsPerDevUnit * intWidth + xOffset;
        } else {
            xOffset = floor(hb2appUnits * x_offset + 0.5);
            advance = floor(hb2appUnits * x_advance + 0.5);
        }
        // Check if it's a simple one-to-one mapping
        if (glyphsInClump == 1 &&
            gfxTextRun::CompressedGlyph::IsSimpleGlyphID(ginfo[glyphStart].codepoint) &&
            gfxTextRun::CompressedGlyph::IsSimpleAdvance(advance) &&
            charGlyphs[baseCharIndex].IsClusterStart() &&
            xOffset == 0 &&
            posInfo[glyphStart].y_offset == 0 && yPos == 0)
        {
            charGlyphs[baseCharIndex].SetSimpleGlyph(advance,
                                                     ginfo[glyphStart].codepoint);
        } else {
            // collect all glyphs in a list to be assigned to the first char;
            // there must be at least one in the clump, and we already measured
            // its advance, hence the placement of the loop-exit test and the
            // measurement of the next glyph
            while (1) {
                gfxTextRun::DetailedGlyph* details =
                    detailedGlyphs.AppendElement();
                details->mGlyphID = ginfo[glyphStart].codepoint;

                details->mXOffset = xOffset;
                details->mAdvance = advance;

                hb_position_t y_offset = posInfo[glyphStart].y_offset;
                details->mYOffset = yPos -
                    (roundY ? appUnitsPerDevUnit * FixedToIntRound(y_offset)
                     : floor(hb2appUnits * y_offset + 0.5));

                hb_position_t y_advance = posInfo[glyphStart].y_advance;
                if (y_advance != 0) {
                    yPos -=
                        roundY ? appUnitsPerDevUnit * FixedToIntRound(y_advance)
                        : floor(hb2appUnits * y_advance + 0.5);
                }
                if (++glyphStart >= glyphEnd) {
                    break;
                }

                x_offset = posInfo[glyphStart].x_offset;
                x_advance = posInfo[glyphStart].x_advance;
                if (roundX) {
                    xOffset = appUnitsPerDevUnit *
                        FixedToIntRound(x_offset + x_residual);
                    // Desired distance to the next reference point.  The
                    // residual is considered here, and includes the residual
                    // from the base glyph offset and subsequent advances, so
                    // that the distance from the base glyph is optimized
                    // rather than the distance from combining marks.
                    x_advance += x_residual;
                    int intAdvance = FixedToIntRound(x_advance);
                    x_residual = x_advance - FloatToFixed(intAdvance);
                    advance = appUnitsPerDevUnit * intAdvance;
                } else {
                    xOffset = floor(hb2appUnits * x_offset + 0.5);
                    advance = floor(hb2appUnits * x_advance + 0.5);
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
