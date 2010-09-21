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
 * The Initial Developer of the Original Code is Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2009-2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "prtypes.h"
#include "prmem.h"
#include "nsString.h"
#include "nsBidiUtils.h"
#include "nsMathUtils.h"

#include "gfxTypes.h"

#include "gfxContext.h"
#include "gfxPlatform.h"
#include "gfxHarfBuzzShaper.h"
#include "gfxFontUtils.h"
#include "gfxUnicodeProperties.h"

#include "harfbuzz/hb-unicode.h"
#include "harfbuzz/hb-ot.h"

#include "nsUnicodeRange.h"
#include "nsCRT.h"

#define FloatToFixed(f) (65536 * (f))
#define FixedToFloat(f) ((f) * (1.0 / 65536.0))

using namespace mozilla; // for AutoSwap_* types

/*
 * Creation and destruction; on deletion, release any font tables we're holding
 */

gfxHarfBuzzShaper::gfxHarfBuzzShaper(gfxFont *aFont)
    : gfxFontShaper(aFont),
      mHBFace(nsnull),
      mHBLanguage(nsnull),
      mKernTable(nsnull),
      mHmtxTable(nsnull),
      mNumLongMetrics(0),
      mCmapTable(nsnull),
      mCmapFormat(-1),
      mSubtableOffset(0),
      mUVSTableOffset(0),
      mUseHintedWidths(aFont->ProvidesHintedWidths())
{
}

gfxHarfBuzzShaper::~gfxHarfBuzzShaper()
{
    hb_blob_destroy(mCmapTable);
    hb_blob_destroy(mHmtxTable);
    hb_blob_destroy(mKernTable);
    hb_face_destroy(mHBFace);
}

/*
 * HarfBuzz callback access to font table data
 */

// callback for HarfBuzz to get a font table (in hb_blob_t form)
// from the shaper (passed as aUserData)
static hb_blob_t *
HBGetTable(hb_tag_t aTag, void *aUserData)
{
    gfxHarfBuzzShaper *shaper = static_cast<gfxHarfBuzzShaper*>(aUserData);
    return shaper->GetFont()->GetFontTable(aTag);
}

/*
 * HarfBuzz font callback functions; user_data is a ptr to a
 * FontCallbackData struct
 */

struct FontCallbackData {
    FontCallbackData(gfxHarfBuzzShaper *aShaper, gfxContext *aContext)
        : mShaper(aShaper), mContext(aContext)
    { }

    gfxHarfBuzzShaper *mShaper;
    gfxContext        *mContext;
};

#define UNICODE_BMP_LIMIT 0x10000

hb_codepoint_t
gfxHarfBuzzShaper::GetGlyph(hb_codepoint_t unicode,
                            hb_codepoint_t variation_selector) const
{
    // we only instantiate a harfbuzz shaper if there's a cmap available
    NS_ASSERTION(mFont->GetFontEntry()->HasCmapTable(),
                 "we cannot be using this font!");

    NS_ASSERTION(mCmapTable && (mCmapFormat > 0) && (mSubtableOffset > 0),
                 "cmap data not correctly set up, expect disaster");

    const PRUint8* data = (const PRUint8*)hb_blob_lock(mCmapTable);

    hb_codepoint_t gid;
    switch (mCmapFormat) {
    case 4:
        gid = unicode < UNICODE_BMP_LIMIT ?
            gfxFontUtils::MapCharToGlyphFormat4(data + mSubtableOffset, unicode) : 0;
        break;
    case 12:
        gid = gfxFontUtils::MapCharToGlyphFormat12(data + mSubtableOffset, unicode);
        break;
    default:
        NS_WARNING("unsupported cmap format, glyphs will be missing");
        gid = 0;
        break;
    }

    if (gid && variation_selector && mUVSTableOffset) {
        hb_codepoint_t varGID =
            gfxFontUtils::MapUVSToGlyphFormat14(data + mUVSTableOffset,
                                                unicode, variation_selector);
        if (varGID) {
            gid = varGID;
        }
        // else the variation sequence was not supported, use default mapping
        // of the character code alone
    }

    hb_blob_unlock(mCmapTable);

    return gid;
}

static hb_codepoint_t
HBGetGlyph(hb_font_t *font, hb_face_t *face, const void *user_data,
           hb_codepoint_t unicode, hb_codepoint_t variation_selector)
{
    const FontCallbackData *fcd =
        static_cast<const FontCallbackData*>(user_data);
    return fcd->mShaper->GetGlyph(unicode, variation_selector);
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

void
gfxHarfBuzzShaper::GetGlyphMetrics(gfxContext *aContext,
                                   hb_codepoint_t glyph,
                                   hb_glyph_metrics_t *metrics) const
{
    if (mUseHintedWidths) {
        metrics->x_advance = mFont->GetHintedGlyphWidth(aContext, glyph);
        return;
    }

    // font did not implement GetHintedGlyphWidth, so get an unhinted value
    // directly from the font tables

    NS_ASSERTION((mNumLongMetrics > 0) && mHmtxTable != nsnull,
                 "font is lacking metrics, we shouldn't be here");

    if (glyph >= mNumLongMetrics) {
        glyph = mNumLongMetrics - 1;
    }

    // glyph must be valid now, because we checked during initialization
    // that mNumLongMetrics is > 0, and that the hmtx table is large enough
    // to contain mNumLongMetrics records
    const HMetrics* hmtx =
        reinterpret_cast<const HMetrics*>(hb_blob_lock(mHmtxTable));
    metrics->x_advance =
        FloatToFixed(mFont->FUnitsToDevUnitsFactor() *
                     PRUint16(hmtx->metrics[glyph].advanceWidth));
    hb_blob_unlock(mHmtxTable);

    // TODO: set additional metrics if/when harfbuzz needs them
}

static void
HBGetGlyphMetrics(hb_font_t *font, hb_face_t *face, const void *user_data,
                  hb_codepoint_t glyph, hb_glyph_metrics_t *metrics)
{
    const FontCallbackData *fcd =
        static_cast<const FontCallbackData*>(user_data);
    fcd->mShaper->GetGlyphMetrics(fcd->mContext, glyph, metrics);
}

static hb_bool_t
HBGetContourPoint(hb_font_t *font, hb_face_t *face, const void *user_data,
                  unsigned int point_index, hb_codepoint_t glyph,
                  hb_position_t *x, hb_position_t *y)
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
                 PRUint32 aSubtableLen,
                 PRUint16 aFirstGlyph,
                 PRUint16 aSecondGlyph,
                 PRInt32& aValue,
                 PRBool   aIsOverride = PR_FALSE,
                 PRBool   aIsMinimum = PR_FALSE)
{
    const KernHeaderFmt0* hdr =
        reinterpret_cast<const KernHeaderFmt0*>(aSubtable);

    const KernPair *lo = reinterpret_cast<const KernPair*>(hdr + 1);
    const KernPair *hi = lo + PRUint16(hdr->nPairs);

    if (reinterpret_cast<const char*>(aSubtable) + aSubtableLen <
        reinterpret_cast<const char*>(hi)) {
        // subtable is not large enough to contain the claimed number
        // of kern pairs, so just ignore it
        return;
    }

#define KERN_PAIR_KEY(l,r) (PRUint32((PRUint16(l) << 16) + PRUint16(r)))

    PRUint32 key = KERN_PAIR_KEY(aFirstGlyph, aSecondGlyph);
    while (lo < hi) {
        const KernPair *mid = lo + (hi - lo) / 2;
        if (KERN_PAIR_KEY(mid->left, mid->right) < key) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }

    if (KERN_PAIR_KEY(lo->left, lo->right) == key) {
        if (aIsOverride) {
            aValue = PRInt16(lo->value);
        } else if (aIsMinimum) {
            aValue = PR_MAX(aValue, PRInt16(lo->value));
        } else {
            aValue += PRInt16(lo->value);
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

static PRInt16
GetKernValueVersion1Fmt2(const void* aSubtable,
                         PRUint32 aSubtableLen,
                         PRUint16 aFirstGlyph,
                         PRUint16 aSecondGlyph)
{
    if (aSubtableLen < sizeof(KernHeaderVersion1Fmt2)) {
        return 0;
    }

    const char* base = reinterpret_cast<const char*>(aSubtable);
    const char* subtableEnd = base + aSubtableLen;

    const KernHeaderVersion1Fmt2* h =
        reinterpret_cast<const KernHeaderVersion1Fmt2*>(aSubtable);
    PRUint32 offset = h->array;

    const KernClassTableHdr* leftClassTable =
        reinterpret_cast<const KernClassTableHdr*>(base +
                                                   PRUint16(h->leftOffsetTable));
    if (reinterpret_cast<const char*>(leftClassTable) +
        sizeof(KernClassTableHdr) > subtableEnd) {
        return 0;
    }
    if (aFirstGlyph >= PRUint16(leftClassTable->firstGlyph)) {
        aFirstGlyph -= PRUint16(leftClassTable->firstGlyph);
        if (aFirstGlyph < PRUint16(leftClassTable->nGlyphs)) {
            if (reinterpret_cast<const char*>(leftClassTable) +
                sizeof(KernClassTableHdr) +
                aFirstGlyph * sizeof(PRUint16) >= subtableEnd) {
                return 0;
            }
            offset = PRUint16(leftClassTable->offsets[aFirstGlyph]);
        }
    }

    const KernClassTableHdr* rightClassTable =
        reinterpret_cast<const KernClassTableHdr*>(base +
                                                   PRUint16(h->rightOffsetTable));
    if (reinterpret_cast<const char*>(rightClassTable) +
        sizeof(KernClassTableHdr) > subtableEnd) {
        return 0;
    }
    if (aSecondGlyph >= PRUint16(rightClassTable->firstGlyph)) {
        aSecondGlyph -= PRUint16(rightClassTable->firstGlyph);
        if (aSecondGlyph < PRUint16(rightClassTable->nGlyphs)) {
            if (reinterpret_cast<const char*>(rightClassTable) +
                sizeof(KernClassTableHdr) +
                aSecondGlyph * sizeof(PRUint16) >= subtableEnd) {
                return 0;
            }
            offset += PRUint16(rightClassTable->offsets[aSecondGlyph]);
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
    PRUint8 kernValueCount;
    PRUint8 leftClassCount;
    PRUint8 rightClassCount;
    PRUint8 flags;
};

static PRInt16
GetKernValueVersion1Fmt3(const void* aSubtable,
                         PRUint32 aSubtableLen,
                         PRUint16 aFirstGlyph,
                         PRUint16 aSecondGlyph)
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

    PRUint16 glyphCount = hdr->glyphCount;

    // check that table is large enough for the arrays
    if (sizeof(KernHeaderVersion1Fmt3) +
        hdr->kernValueCount * sizeof(PRInt16) +
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
    const PRUint8* leftClass =
        reinterpret_cast<const PRUint8*>(kernValue + hdr->kernValueCount);
    const PRUint8* rightClass = leftClass + glyphCount;
    const PRUint8* kernIndex = rightClass + glyphCount;

    PRUint8 lc = leftClass[aFirstGlyph];
    PRUint8 rc = rightClass[aSecondGlyph];
    if (lc >= hdr->leftClassCount || rc >= hdr->rightClassCount) {
        return 0;
    }

    PRUint8 ki = kernIndex[leftClass[aFirstGlyph] * hdr->rightClassCount +
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
gfxHarfBuzzShaper::GetKerning(PRUint16 aFirstGlyph,
                              PRUint16 aSecondGlyph) const
{
    // We want to ignore any kern pairs involving <space>, because we are
    // handling words in isolation, the only space characters seen here are
    // the ones artificially added by the textRun code.
    PRUint32 spaceGlyph = mFont->GetSpaceGlyph();
    if (aFirstGlyph == spaceGlyph || aSecondGlyph == spaceGlyph) {
        return 0;
    }

    if (!mKernTable) {
        mKernTable = mFont->GetFontTable(TRUETYPE_TAG('k','e','r','n'));
        if (!mKernTable) {
            mKernTable = hb_blob_create_empty();
        }
    }

    PRUint32 len = hb_blob_get_length(mKernTable);
    if (len < sizeof(KernTableVersion0)) {
        return 0;
    }

    PRInt32 value = 0;
    const char* base = reinterpret_cast<const char*>(hb_blob_lock(mKernTable));

    // First try to interpret as "version 0" kern table
    // (see http://www.microsoft.com/typography/otspec/kern.htm)
    const KernTableVersion0* kern0 =
        reinterpret_cast<const KernTableVersion0*>(base);
    if (PRUint16(kern0->version) == 0) {
        PRUint16 nTables = kern0->nTables;
        PRUint32 offs = sizeof(KernTableVersion0);
        for (PRUint16 i = 0; i < nTables; ++i) {
            if (offs + sizeof(KernTableSubtableHeaderVersion0) > len) {
                break;
            }
            const KernTableSubtableHeaderVersion0* st0 =
                reinterpret_cast<const KernTableSubtableHeaderVersion0*>
                                (base + offs);
            PRUint16 subtableLen = PRUint16(st0->length);
            if (offs + subtableLen > len) {
                break;
            }
            offs += subtableLen;
            PRUint16 coverage = st0->coverage;
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
            PRUint8 format = (coverage >> 8);
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
        if (PRUint32(kern1->version) == 0x00010000) {
            PRUint32 nTables = kern1->nTables;
            PRUint32 offs = sizeof(KernTableVersion1);
            for (PRUint32 i = 0; i < nTables; ++i) {
                if (offs + sizeof(KernTableSubtableHeaderVersion1) > len) {
                    break;
                }
                const KernTableSubtableHeaderVersion1* st1 =
                    reinterpret_cast<const KernTableSubtableHeaderVersion1*>
                                    (base + offs);
                PRUint32 subtableLen = PRUint32(st1->length);
                offs += subtableLen;
                PRUint16 coverage = st1->coverage;
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
                PRUint8 format = (coverage & 0xff);
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

    hb_blob_unlock(mKernTable);

    if (value != 0) {
        return FloatToFixed(mFont->FUnitsToDevUnitsFactor() * value);
    }
    return 0;
}

static hb_position_t
HBGetKerning(hb_font_t *font, hb_face_t *face, const void *user_data,
             hb_codepoint_t first_glyph, hb_codepoint_t second_glyph)
{
    const FontCallbackData *fcd =
        static_cast<const FontCallbackData*>(user_data);
    return fcd->mShaper->GetKerning(first_glyph, second_glyph);
}

/*
 * HarfBuzz unicode property callbacks
 */

static hb_codepoint_t
HBGetMirroring(hb_codepoint_t aCh)
{
    return gfxUnicodeProperties::GetMirroredChar(aCh);
}

static hb_category_t
HBGetGeneralCategory(hb_codepoint_t aCh)
{
    return hb_category_t(gfxUnicodeProperties::GetGeneralCategory(aCh));
}

static hb_script_t
HBGetScript(hb_codepoint_t aCh)
{
    return hb_script_t(gfxUnicodeProperties::GetScriptCode(aCh));
}

static unsigned int
HBGetCombiningClass(hb_codepoint_t aCh)
{
    return gfxUnicodeProperties::GetCombiningClass(aCh);
}

static unsigned int
HBGetEastAsianWidth(hb_codepoint_t aCh)
{
    return gfxUnicodeProperties::GetEastAsianWidth(aCh);
}

/*
 * gfxFontShaper override to initialize the text run using HarfBuzz
 */

static hb_font_funcs_t * sHBFontFuncs = nsnull;
static hb_unicode_funcs_t * sHBUnicodeFuncs = nsnull;

PRBool
gfxHarfBuzzShaper::InitTextRun(gfxContext *aContext,
                               gfxTextRun *aTextRun,
                               const PRUnichar *aString,
                               PRUint32 aRunStart,
                               PRUint32 aRunLength,
                               PRInt32 aRunScript)
{
    // some font back-ends require this in order to get proper hinted metrics
    mFont->SetupCairoFont(aContext);

    if (!mHBFace) {
        // set up the harfbuzz face etc the first time we use the font

        if (!sHBFontFuncs) {
            // static function callback pointers, initialized by the first
            // harfbuzz shaper used
            sHBFontFuncs = hb_font_funcs_copy(hb_font_funcs_create());
            hb_font_funcs_set_glyph_func(sHBFontFuncs, HBGetGlyph);
            hb_font_funcs_set_glyph_metrics_func(sHBFontFuncs,
                                                 HBGetGlyphMetrics);
            hb_font_funcs_set_contour_point_func(sHBFontFuncs,
                                                 HBGetContourPoint);
            hb_font_funcs_set_kerning_func(sHBFontFuncs, HBGetKerning);

            sHBUnicodeFuncs = hb_unicode_funcs_copy(hb_unicode_funcs_create());
            hb_unicode_funcs_set_mirroring_func(sHBUnicodeFuncs,
                                                HBGetMirroring);
            hb_unicode_funcs_set_script_func(sHBUnicodeFuncs, HBGetScript);
            hb_unicode_funcs_set_general_category_func(sHBUnicodeFuncs,
                                                       HBGetGeneralCategory);
            hb_unicode_funcs_set_combining_class_func(sHBUnicodeFuncs,
                                                      HBGetCombiningClass);
            hb_unicode_funcs_set_eastasian_width_func(sHBUnicodeFuncs,
                                                      HBGetEastAsianWidth);
        }

        mHBFace = hb_face_create_for_tables(HBGetTable, nsnull, this);

        // get the cmap table and find offset to our subtable
        mCmapTable = mFont->GetFontTable(TRUETYPE_TAG('c','m','a','p'));
        if (!mCmapTable) {
            NS_WARNING("failed to load cmap, glyphs will be missing");
            return PR_FALSE;
        }
        const PRUint8* data = (const PRUint8*)hb_blob_lock(mCmapTable);
        PRBool symbol;
        mCmapFormat =
            gfxFontUtils::FindPreferredSubtable(data,
                                                hb_blob_get_length(mCmapTable),
                                                &mSubtableOffset,
                                                &mUVSTableOffset,
                                                &symbol);
        hb_blob_unlock(mCmapTable);

        if (!mUseHintedWidths) {
            // if font doesn't implement hinted widths, we will be reading
            // the hmtx table directly;
            // read mNumLongMetrics from hhea table without caching its blob,
            // and preload/cache the hmtx table
            hb_blob_t *hheaTable =
                mFont->GetFontTable(TRUETYPE_TAG('h','h','e','a'));
            if (hheaTable &&
                hb_blob_get_length(hheaTable) >= sizeof(HMetricsHeader)) {
                const HMetricsHeader* hhea =
                    reinterpret_cast<const HMetricsHeader*>
                        (hb_blob_lock(hheaTable));
                mNumLongMetrics = hhea->numberOfHMetrics;
                hb_blob_unlock(hheaTable);

                if (mNumLongMetrics > 0 &&
                    PRInt16(hhea->metricDataFormat) == 0) {
                    // no point reading hmtx if number of entries is zero!
                    // in that case, we won't be able to use this font
                    // (this method will return FALSE below if mHmtx is null)
                    mHmtxTable =
                        mFont->GetFontTable(TRUETYPE_TAG('h','m','t','x'));
                    if (hb_blob_get_length(mHmtxTable) <
                        mNumLongMetrics * sizeof(HLongMetric)) {
                        // hmtx table is not large enough for the claimed
                        // number of entries: invalid, do not use.
                        hb_blob_destroy(mHmtxTable);
                        mHmtxTable = nsnull;
                    }
                }
            }
            hb_blob_destroy(hheaTable);
        }

        if (mFont->GetStyle()->languageOverride) {
            mHBLanguage =
                hb_ot_tag_to_language(mFont->GetStyle()->languageOverride);
        } else if (mFont->GetFontEntry()->mLanguageOverride) {
            mHBLanguage =
                hb_ot_tag_to_language(mFont->GetFontEntry()->mLanguageOverride);
        } else {
            nsCString langString;
            mFont->GetStyle()->language->ToUTF8String(langString);
            mHBLanguage = hb_language_from_string(langString.get());
        }
    }

    if (mCmapFormat <= 0 || (!mUseHintedWidths && !mHmtxTable)) {
        // unable to shape with this font
        return PR_FALSE;
    }

    FontCallbackData fcd(this, aContext);
    hb_font_t *font = hb_font_create();
    hb_font_set_funcs(font, sHBFontFuncs, nsnull, &fcd);
    hb_font_set_ppem(font, mFont->GetAdjustedSize(), mFont->GetAdjustedSize());
    PRUint32 scale = FloatToFixed(mFont->GetAdjustedSize()); // 16.16 fixed-point
    hb_font_set_scale(font, scale, scale);

    // aRunStart and aRunLength define the section of the textRun and of
    // aString that is to be drawn with this particular font

    PRBool disableLigatures =
        (aTextRun->GetFlags() &
         gfxTextRunFactory::TEXT_DISABLE_OPTIONAL_LIGATURES) != 0;

    nsAutoTArray<hb_feature_t,20> features;

    // Ligature features are enabled by default in the generic shaper,
    // so we explicitly turn them off if necessary (for letter-spacing)
    if (disableLigatures) {
        hb_feature_t ligaOff = { HB_TAG('l','i','g','a'), 0, 0, -1 };
        hb_feature_t cligOff = { HB_TAG('c','l','i','g'), 0, 0, -1 };
        features.AppendElement(ligaOff);
        features.AppendElement(cligOff);
    }

    // css features need to be merged with the existing ones, if any
    const nsTArray<gfxFontFeature> *cssFeatures =
        mFont->GetStyle()->featureSettings;
    if (!cssFeatures) {
        cssFeatures = mFont->GetFontEntry()->mFeatureSettings;
    }
    if (cssFeatures) {
        for (PRUint32 i = 0; i < cssFeatures->Length(); ++i) {
            PRUint32 j;
            for (j = 0; j < features.Length(); ++j) {
                if (cssFeatures->ElementAt(i).mTag == features[j].tag) {
                    features[j].value = cssFeatures->ElementAt(i).mValue;
                    break;
                }
            }
            if (j == features.Length()) {
                const gfxFontFeature& f = cssFeatures->ElementAt(i);
                hb_feature_t hbf = { f.mTag, f.mValue, 0, -1 };
                features.AppendElement(hbf);
            }
        }
    }

    hb_buffer_t *buffer = hb_buffer_create(aRunLength);
    hb_buffer_set_unicode_funcs(buffer, sHBUnicodeFuncs);
    hb_buffer_set_direction(buffer,
                            aTextRun->IsRightToLeft() ?
                                HB_DIRECTION_RTL : HB_DIRECTION_LTR);
    hb_buffer_set_script(buffer, hb_script_t(aRunScript));
    hb_buffer_set_language(buffer, mHBLanguage);

    hb_buffer_add_utf16(buffer, reinterpret_cast<const uint16_t*>(aString + aRunStart),
                        aRunLength, 0, aRunLength);

    hb_shape(font, mHBFace, buffer, features.Elements(), features.Length());

    if (aTextRun->IsRightToLeft()) {
        hb_buffer_reverse(buffer);
    }

    nsresult rv = SetGlyphsFromRun(aContext, aTextRun, buffer,
                                   aRunStart, aRunLength);
    NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "failed to store glyphs into textrun");
    hb_buffer_destroy(buffer);
    hb_font_destroy(font);

    return PR_TRUE;
}

#define SMALL_GLYPH_RUN 128 // some testing indicates that 90%+ of text runs
                            // will fit without requiring separate allocation
                            // for charToGlyphArray

nsresult
gfxHarfBuzzShaper::SetGlyphsFromRun(gfxContext *aContext,
                                    gfxTextRun *aTextRun,
                                    hb_buffer_t *aBuffer,
                                    PRUint32 aTextRunOffset,
                                    PRUint32 aRunLength)
{
    PRInt32 numGlyphs = hb_buffer_get_length(aBuffer);
    if (numGlyphs == 0)
        return NS_OK;

    const hb_glyph_info_t *ginfo = hb_buffer_get_glyph_infos(aBuffer);

    nsAutoTArray<gfxTextRun::DetailedGlyph,1> detailedGlyphs;

    static const PRInt32 NO_GLYPH = -1;
    nsAutoTArray<PRInt32,SMALL_GLYPH_RUN> charToGlyphArray;
    if (!charToGlyphArray.SetLength(aRunLength))
        return NS_ERROR_OUT_OF_MEMORY;

    PRInt32 *charToGlyph = charToGlyphArray.Elements();
    for (PRUint32 offset = 0; offset < aRunLength; ++offset) {
        charToGlyph[offset] = NO_GLYPH;
    }

    for (PRInt32 i = 0; i < numGlyphs; ++i) {
        PRInt32 loc = ginfo[i].cluster;
        if (loc < aRunLength) {
            charToGlyph[loc] = i;
        }
    }

    PRInt32 glyphStart = 0; // looking for a clump that starts at this glyph
    PRInt32 charStart = 0; // and this char index within the range of the run

    // factor to convert 16.16 fixed-point pixels to app units
    float hb2appUnits = aTextRun->GetAppUnitsPerDevUnit() / 65536.0;

    // keep track of y-position to set glyph offsets if needed
    nscoord yPos = 0;

    const hb_glyph_position_t *posInfo = hb_buffer_get_glyph_positions(aBuffer);

    while (glyphStart < numGlyphs) {

        PRBool inOrder = PR_TRUE;
        PRInt32 charEnd = ginfo[glyphStart].cluster;
        PRInt32 glyphEnd = glyphStart;
        PRInt32 charLimit = aRunLength;
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
            for (PRInt32 i = charStart; i < charEnd; ++i) {
                if (charToGlyph[i] != NO_GLYPH) {
                    glyphEnd = PR_MAX(glyphEnd, charToGlyph[i] + 1);
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
            PRBool allGlyphsAreWithinCluster = PR_TRUE;
            PRInt32 prevGlyphCharIndex = charStart - 1;
            for (PRInt32 i = glyphStart; i < glyphEnd; ++i) {
                PRInt32 glyphCharIndex = ginfo[i].cluster;
                if (glyphCharIndex < charStart || glyphCharIndex >= charEnd) {
                    allGlyphsAreWithinCluster = PR_FALSE;
                    break;
                }
                if (glyphCharIndex <= prevGlyphCharIndex) {
                    inOrder = PR_FALSE;
                }
                prevGlyphCharIndex = glyphCharIndex;
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
        PRInt32 baseCharIndex, endCharIndex;
        while (charEnd < aRunLength && charToGlyph[charEnd] == NO_GLYPH)
            charEnd++;
        baseCharIndex = charStart;
        endCharIndex = charEnd;

        // Then we check if the clump falls outside our actual string range;
        // if so, just go to the next.
        if (baseCharIndex >= aRunLength) {
            glyphStart = glyphEnd;
            charStart = charEnd;
            continue;
        }
        // Ensure we won't try to go beyond the valid length of the textRun's text
        endCharIndex = PR_MIN(endCharIndex, aRunLength);

        // Now we're ready to set the glyph info in the textRun
        PRInt32 glyphsInClump = glyphEnd - glyphStart;

        // Check for default-ignorable char that didn't get filtered, combined,
        // etc by the shaping process, and remove from the run.
        // (This may be done within harfbuzz eventually.)
        if (glyphsInClump == 1 && baseCharIndex + 1 == endCharIndex &&
            aTextRun->FilterIfIgnorable(aTextRunOffset + baseCharIndex)) {
            glyphStart = glyphEnd;
            charStart = charEnd;
            continue;
        }

        // Check if it's a simple one-to-one mapping
        nscoord advance = NS_roundf(hb2appUnits * posInfo[glyphStart].x_advance);
        if (glyphsInClump == 1 &&
            gfxTextRun::CompressedGlyph::IsSimpleGlyphID(ginfo[glyphStart].codepoint) &&
            gfxTextRun::CompressedGlyph::IsSimpleAdvance(advance) &&
            aTextRun->IsClusterStart(aTextRunOffset + baseCharIndex) &&
            posInfo[glyphStart].x_offset == 0 &&
            posInfo[glyphStart].y_offset == 0 && yPos == 0)
        {
            gfxTextRun::CompressedGlyph g;
            aTextRun->SetSimpleGlyph(aTextRunOffset + baseCharIndex,
                                     g.SetSimpleGlyph(advance,
                                         ginfo[glyphStart].codepoint));
        } else {
            // collect all glyphs in a list to be assigned to the first char;
            // there must be at least one in the clump, and we already measured
            // its advance, hence the placement of the loop-exit test and the
            // measurement of the next glyph
            while (1) {
                gfxTextRun::DetailedGlyph* details =
                    detailedGlyphs.AppendElement();
                details->mGlyphID = ginfo[glyphStart].codepoint;
                details->mXOffset = posInfo[glyphStart].x_offset == 0 ?
                                        0 : hb2appUnits * posInfo[glyphStart].x_offset;
                details->mYOffset = yPos - (posInfo[glyphStart].y_offset == 0 ?
                                        0 : hb2appUnits * posInfo[glyphStart].y_offset);
                details->mAdvance = advance;
                if (posInfo[glyphStart].y_advance != 0) {
                    yPos -= hb2appUnits * posInfo[glyphStart].y_advance;
                }
                if (++glyphStart >= glyphEnd) {
                    break;
                }
                advance = NS_roundf(hb2appUnits * posInfo[glyphStart].x_advance);
            }

            gfxTextRun::CompressedGlyph g;
            g.SetComplex(aTextRun->IsClusterStart(aTextRunOffset + baseCharIndex),
                         PR_TRUE, detailedGlyphs.Length());
            aTextRun->SetGlyphs(aTextRunOffset + baseCharIndex,
                                g, detailedGlyphs.Elements());

            detailedGlyphs.Clear();
        }

        // the rest of the chars in the group are ligature continuations,
        // no associated glyphs
        while (++baseCharIndex != endCharIndex && baseCharIndex < aRunLength) {
            gfxTextRun::CompressedGlyph g;
            g.SetComplex(inOrder &&
                         aTextRun->IsClusterStart(aTextRunOffset + baseCharIndex),
                         PR_FALSE, 0);
            aTextRun->SetGlyphs(aTextRunOffset + baseCharIndex, g, nsnull);
        }

        glyphStart = glyphEnd;
        charStart = charEnd;
    }

    return NS_OK;
}
