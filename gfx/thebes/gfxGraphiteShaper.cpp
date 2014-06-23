/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxGraphiteShaper.h"
#include "nsString.h"
#include "gfxContext.h"
#include "gfxFontConstants.h"

#include "graphite2/Font.h"
#include "graphite2/Segment.h"

#include "harfbuzz/hb.h"

#define FloatToFixed(f) (65536 * (f))
#define FixedToFloat(f) ((f) * (1.0 / 65536.0))
// Right shifts of negative (signed) integers are undefined, as are overflows
// when converting unsigned to negative signed integers.
// (If speed were an issue we could make some 2's complement assumptions.)
#define FixedToIntRound(f) ((f) > 0 ?  ((32768 + (f)) >> 16) \
                                    : -((32767 - (f)) >> 16))

using namespace mozilla; // for AutoSwap_* types

/*
 * Creation and destruction; on deletion, release any font tables we're holding
 */

gfxGraphiteShaper::gfxGraphiteShaper(gfxFont *aFont)
    : gfxFontShaper(aFont),
      mGrFace(mFont->GetFontEntry()->GetGrFace()),
      mGrFont(nullptr), mFallbackToSmallCaps(false)
{
    mCallbackData.mFont = aFont;
    mCallbackData.mShaper = this;
}

gfxGraphiteShaper::~gfxGraphiteShaper()
{
    if (mGrFont) {
        gr_font_destroy(mGrFont);
    }
    mFont->GetFontEntry()->ReleaseGrFace(mGrFace);
}

/*static*/ float
gfxGraphiteShaper::GrGetAdvance(const void* appFontHandle, uint16_t glyphid)
{
    const CallbackData *cb =
        static_cast<const CallbackData*>(appFontHandle);
    return FixedToFloat(cb->mFont->GetGlyphWidth(cb->mContext, glyphid));
}

static inline uint32_t
MakeGraphiteLangTag(uint32_t aTag)
{
    uint32_t grLangTag = aTag;
    // replace trailing space-padding with NULs for graphite
    uint32_t mask = 0x000000FF;
    while ((grLangTag & mask) == ' ') {
        grLangTag &= ~mask;
        mask <<= 8;
    }
    return grLangTag;
}

struct GrFontFeatures {
    gr_face        *mFace;
    gr_feature_val *mFeatures;
};

static PLDHashOperator
AddFeature(const uint32_t& aTag, uint32_t& aValue, void *aUserArg)
{
    GrFontFeatures *f = static_cast<GrFontFeatures*>(aUserArg);

    const gr_feature_ref* fref = gr_face_find_fref(f->mFace, aTag);
    if (fref) {
        gr_fref_set_feature_value(fref, aValue, f->mFeatures);
    }
    return PL_DHASH_NEXT;
}

bool
gfxGraphiteShaper::ShapeText(gfxContext      *aContext,
                             const char16_t *aText,
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

    const gfxFontStyle *style = mFont->GetStyle();

    if (!mGrFont) {
        if (!mGrFace) {
            return false;
        }

        if (mFont->ProvidesGlyphWidths()) {
            gr_font_ops ops = {
                sizeof(gr_font_ops),
                &GrGetAdvance,
                nullptr // vertical text not yet implemented
            };
            mGrFont = gr_make_font_with_ops(mFont->GetAdjustedSize(),
                                            &mCallbackData, &ops, mGrFace);
        } else {
            mGrFont = gr_make_font(mFont->GetAdjustedSize(), mGrFace);
        }

        if (!mGrFont) {
            return false;
        }

        // determine whether petite-caps falls back to small-caps
        if (style->variantCaps != NS_FONT_VARIANT_CAPS_NORMAL) {
            switch (style->variantCaps) {
                case NS_FONT_VARIANT_CAPS_ALLPETITE:
                case NS_FONT_VARIANT_CAPS_PETITECAPS:
                    bool synLower, synUpper;
                    mFont->SupportsVariantCaps(aScript, style->variantCaps,
                                               mFallbackToSmallCaps, synLower,
                                               synUpper);
                    break;
                default:
                    break;
            }
        }
    }

    gfxFontEntry *entry = mFont->GetFontEntry();
    uint32_t grLang = 0;
    if (style->languageOverride) {
        grLang = MakeGraphiteLangTag(style->languageOverride);
    } else if (entry->mLanguageOverride) {
        grLang = MakeGraphiteLangTag(entry->mLanguageOverride);
    } else {
        nsAutoCString langString;
        style->language->ToUTF8String(langString);
        grLang = GetGraphiteTagForLang(langString);
    }
    gr_feature_val *grFeatures = gr_face_featureval_for_lang(mGrFace, grLang);

    // if style contains font-specific features
    nsDataHashtable<nsUint32HashKey,uint32_t> mergedFeatures;

    if (MergeFontFeatures(style,
                          mFont->GetFontEntry()->mFeatureSettings,
                          aShapedText->DisableLigatures(),
                          mFont->GetFontEntry()->FamilyName(),
                          mFallbackToSmallCaps,
                          mergedFeatures))
    {
        // enumerate result and insert into Graphite feature list
        GrFontFeatures f = {mGrFace, grFeatures};
        mergedFeatures.Enumerate(AddFeature, &f);
    }

    size_t numChars = gr_count_unicode_characters(gr_utf16,
                                                  aText, aText + aLength,
                                                  nullptr);
    gr_segment *seg = gr_make_seg(mGrFont, mGrFace, 0, grFeatures,
                                  gr_utf16, aText, numChars,
                                  aShapedText->IsRightToLeft());

    gr_featureval_destroy(grFeatures);

    if (!seg) {
        return false;
    }

    nsresult rv = SetGlyphsFromSegment(aContext, aShapedText, aOffset, aLength,
                                       aText, seg);

    gr_seg_destroy(seg);

    return NS_SUCCEEDED(rv);
}

#define SMALL_GLYPH_RUN 256 // avoid heap allocation of per-glyph data arrays
                            // for short (typical) runs up to this length

struct Cluster {
    uint32_t baseChar; // in UTF16 code units, not Unicode character indices
    uint32_t baseGlyph;
    uint32_t nChars; // UTF16 code units
    uint32_t nGlyphs;
    Cluster() : baseChar(0), baseGlyph(0), nChars(0), nGlyphs(0) { }
};

nsresult
gfxGraphiteShaper::SetGlyphsFromSegment(gfxContext      *aContext,
                                        gfxShapedText   *aShapedText,
                                        uint32_t         aOffset,
                                        uint32_t         aLength,
                                        const char16_t *aText,
                                        gr_segment      *aSegment)
{
    int32_t dev2appUnits = aShapedText->GetAppUnitsPerDevUnit();
    bool rtl = aShapedText->IsRightToLeft();

    uint32_t glyphCount = gr_seg_n_slots(aSegment);

    // identify clusters; graphite may have reordered/expanded/ligated glyphs.
    AutoFallibleTArray<Cluster,SMALL_GLYPH_RUN> clusters;
    AutoFallibleTArray<uint16_t,SMALL_GLYPH_RUN> gids;
    AutoFallibleTArray<float,SMALL_GLYPH_RUN> xLocs;
    AutoFallibleTArray<float,SMALL_GLYPH_RUN> yLocs;

    if (!clusters.SetLength(aLength) ||
        !gids.SetLength(glyphCount) ||
        !xLocs.SetLength(glyphCount) ||
        !yLocs.SetLength(glyphCount))
    {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    // walk through the glyph slots and check which original character
    // each is associated with
    uint32_t gIndex = 0; // glyph slot index
    uint32_t cIndex = 0; // current cluster index
    for (const gr_slot *slot = gr_seg_first_slot(aSegment);
         slot != nullptr;
         slot = gr_slot_next_in_segment(slot), gIndex++)
    {
        uint32_t before =
            gr_cinfo_base(gr_seg_cinfo(aSegment, gr_slot_before(slot)));
        uint32_t after =
            gr_cinfo_base(gr_seg_cinfo(aSegment, gr_slot_after(slot)));
        gids[gIndex] = gr_slot_gid(slot);
        xLocs[gIndex] = gr_slot_origin_X(slot);
        yLocs[gIndex] = gr_slot_origin_Y(slot);

        // if this glyph has a "before" character index that precedes the
        // current cluster's char index, we need to merge preceding
        // clusters until it gets included
        while (before < clusters[cIndex].baseChar && cIndex > 0) {
            clusters[cIndex-1].nChars += clusters[cIndex].nChars;
            clusters[cIndex-1].nGlyphs += clusters[cIndex].nGlyphs;
            --cIndex;
        }

        // if there's a gap between the current cluster's base character and
        // this glyph's, extend the cluster to include the intervening chars
        if (gr_slot_can_insert_before(slot) && clusters[cIndex].nChars &&
            before >= clusters[cIndex].baseChar + clusters[cIndex].nChars)
        {
            NS_ASSERTION(cIndex < aLength - 1, "cIndex at end of word");
            Cluster& c = clusters[cIndex + 1];
            c.baseChar = clusters[cIndex].baseChar + clusters[cIndex].nChars;
            c.nChars = before - c.baseChar;
            c.baseGlyph = gIndex;
            c.nGlyphs = 0;
            ++cIndex;
        }

        // increment cluster's glyph count to include current slot
        NS_ASSERTION(cIndex < aLength, "cIndex beyond word length");
        ++clusters[cIndex].nGlyphs;

        // extend cluster if necessary to reach the glyph's "after" index
        if (clusters[cIndex].baseChar + clusters[cIndex].nChars < after + 1) {
            clusters[cIndex].nChars = after + 1 - clusters[cIndex].baseChar;
        }
    }

    bool roundX;
    bool roundY;
    aContext->GetRoundOffsetsToPixels(&roundX, &roundY);

    gfxShapedText::CompressedGlyph *charGlyphs =
        aShapedText->GetCharacterGlyphs() + aOffset;

    // now put glyphs into the textrun, one cluster at a time
    for (uint32_t i = 0; i <= cIndex; ++i) {
        const Cluster& c = clusters[i];

        float adv; // total advance of the cluster
        if (rtl) {
            if (i == 0) {
                adv = gr_seg_advance_X(aSegment) - xLocs[c.baseGlyph];
            } else {
                adv = xLocs[clusters[i-1].baseGlyph] - xLocs[c.baseGlyph];
            }
        } else {
            if (i == cIndex) {
                adv = gr_seg_advance_X(aSegment) - xLocs[c.baseGlyph];
            } else {
                adv = xLocs[clusters[i+1].baseGlyph] - xLocs[c.baseGlyph];
            }
        }

        // Check for default-ignorable char that didn't get filtered, combined,
        // etc by the shaping process, and skip it.
        uint32_t offs = c.baseChar;
        NS_ASSERTION(offs < aLength, "unexpected offset");
        if (c.nGlyphs == 1 && c.nChars == 1 &&
            aShapedText->FilterIfIgnorable(aOffset + offs, aText[offs])) {
            continue;
        }

        uint32_t appAdvance = roundX ? NSToIntRound(adv) * dev2appUnits :
                                       NSToIntRound(adv * dev2appUnits);
        if (c.nGlyphs == 1 &&
            gfxShapedText::CompressedGlyph::IsSimpleGlyphID(gids[c.baseGlyph]) &&
            gfxShapedText::CompressedGlyph::IsSimpleAdvance(appAdvance) &&
            charGlyphs[offs].IsClusterStart() &&
            yLocs[c.baseGlyph] == 0)
        {
            charGlyphs[offs].SetSimpleGlyph(appAdvance, gids[c.baseGlyph]);
        } else {
            // not a one-to-one mapping with simple metrics: use DetailedGlyph
            nsAutoTArray<gfxShapedText::DetailedGlyph,8> details;
            float clusterLoc;
            for (uint32_t j = c.baseGlyph; j < c.baseGlyph + c.nGlyphs; ++j) {
                gfxShapedText::DetailedGlyph* d = details.AppendElement();
                d->mGlyphID = gids[j];
                d->mYOffset = roundY ? NSToIntRound(-yLocs[j]) * dev2appUnits :
                              -yLocs[j] * dev2appUnits;
                if (j == c.baseGlyph) {
                    d->mXOffset = 0;
                    d->mAdvance = appAdvance;
                    clusterLoc = xLocs[j];
                } else {
                    float dx = rtl ? (xLocs[j] - clusterLoc) :
                                     (xLocs[j] - clusterLoc - adv);
                    d->mXOffset = roundX ? NSToIntRound(dx) * dev2appUnits :
                                           dx * dev2appUnits;
                    d->mAdvance = 0;
                }
            }
            gfxShapedText::CompressedGlyph g;
            g.SetComplex(charGlyphs[offs].IsClusterStart(),
                         true, details.Length());
            aShapedText->SetGlyphs(aOffset + offs, g, details.Elements());
        }

        for (uint32_t j = c.baseChar + 1; j < c.baseChar + c.nChars; ++j) {
            NS_ASSERTION(j < aLength, "unexpected offset");
            gfxShapedText::CompressedGlyph &g = charGlyphs[j];
            NS_ASSERTION(!g.IsSimpleGlyph(), "overwriting a simple glyph");
            g.SetComplex(g.IsClusterStart(), false, 0);
        }
    }

    return NS_OK;
}

#undef SMALL_GLYPH_RUN

// for language tag validation - include list of tags from the IANA registry
#include "gfxLanguageTagList.cpp"

nsTHashtable<nsUint32HashKey> *gfxGraphiteShaper::sLanguageTags;

/*static*/ uint32_t
gfxGraphiteShaper::GetGraphiteTagForLang(const nsCString& aLang)
{
    int len = aLang.Length();
    if (len < 2) {
        return 0;
    }

    // convert primary language subtag to a left-packed, NUL-padded integer
    // for the Graphite API
    uint32_t grLang = 0;
    for (int i = 0; i < 4; ++i) {
        grLang <<= 8;
        if (i < len) {
            uint8_t ch = aLang[i];
            if (ch == '-') {
                // found end of primary language subtag, truncate here
                len = i;
                continue;
            }
            if (ch < 'a' || ch > 'z') {
                // invalid character in tag, so ignore it completely
                return 0;
            }
            grLang += ch;
        }
    }

    // valid tags must have length = 2 or 3
    if (len < 2 || len > 3) {
        return 0;
    }

    if (!sLanguageTags) {
        // store the registered IANA tags in a hash for convenient validation
        sLanguageTags = new nsTHashtable<nsUint32HashKey>(ArrayLength(sLanguageTagList));
        for (const uint32_t *tag = sLanguageTagList; *tag != 0; ++tag) {
            sLanguageTags->PutEntry(*tag);
        }
    }

    // only accept tags known in the IANA registry
    if (sLanguageTags->GetEntry(grLang)) {
        return grLang;
    }

    return 0;
}

/*static*/ void
gfxGraphiteShaper::Shutdown()
{
#ifdef NS_FREE_PERMANENT_DATA
    if (sLanguageTags) {
        sLanguageTags->Clear();
        delete sLanguageTags;
        sLanguageTags = nullptr;
    }
#endif
}
