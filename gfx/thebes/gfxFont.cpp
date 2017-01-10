/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxFont.h"

#include "mozilla/BinarySearch.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/SVGContextPaint.h"

#include "mozilla/Logging.h"

#include "nsITimer.h"

#include "gfxGlyphExtents.h"
#include "gfxPlatform.h"
#include "gfxTextRun.h"
#include "nsGkAtoms.h"

#include "gfxTypes.h"
#include "gfxContext.h"
#include "gfxFontMissingGlyphs.h"
#include "gfxGraphiteShaper.h"
#include "gfxHarfBuzzShaper.h"
#include "gfxUserFontSet.h"
#include "nsIUGenCategory.h"
#include "nsSpecialCasingData.h"
#include "nsTextRunTransformations.h"
#include "nsUnicodeProperties.h"
#include "nsStyleConsts.h"
#include "mozilla/AppUnits.h"
#include "mozilla/Likely.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
#include "mozilla/Telemetry.h"
#include "gfxMathTable.h"
#include "gfxSVGGlyphs.h"
#include "gfx2DGlue.h"

#include "GreekCasing.h"

#include "cairo.h"

#include "harfbuzz/hb.h"
#include "harfbuzz/hb-ot.h"
#include "graphite2/Font.h"

#include <algorithm>
#include <limits>
#include <cmath>

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::unicode;
using mozilla::services::GetObserverService;

gfxFontCache *gfxFontCache::gGlobalCache = nullptr;

#ifdef DEBUG_roc
#define DEBUG_TEXT_RUN_STORAGE_METRICS
#endif

#ifdef DEBUG_TEXT_RUN_STORAGE_METRICS
uint32_t gTextRunStorageHighWaterMark = 0;
uint32_t gTextRunStorage = 0;
uint32_t gFontCount = 0;
uint32_t gGlyphExtentsCount = 0;
uint32_t gGlyphExtentsWidthsTotalSize = 0;
uint32_t gGlyphExtentsSetupEagerSimple = 0;
uint32_t gGlyphExtentsSetupEagerTight = 0;
uint32_t gGlyphExtentsSetupLazyTight = 0;
uint32_t gGlyphExtentsSetupFallBackToTight = 0;
#endif

#define LOG_FONTINIT(args) MOZ_LOG(gfxPlatform::GetLog(eGfxLog_fontinit), \
                                  LogLevel::Debug, args)
#define LOG_FONTINIT_ENABLED() MOZ_LOG_TEST( \
                                        gfxPlatform::GetLog(eGfxLog_fontinit), \
                                        LogLevel::Debug)


/*
 * gfxFontCache - global cache of gfxFont instances.
 * Expires unused fonts after a short interval;
 * notifies fonts to age their cached shaped-word records;
 * observes memory-pressure notification and tells fonts to clear their
 * shaped-word caches to free up memory.
 */

MOZ_DEFINE_MALLOC_SIZE_OF(FontCacheMallocSizeOf)

NS_IMPL_ISUPPORTS(gfxFontCache::MemoryReporter, nsIMemoryReporter)

NS_IMETHODIMP
gfxFontCache::MemoryReporter::CollectReports(
    nsIHandleReportCallback* aHandleReport, nsISupports* aData, bool aAnonymize)
{
    FontCacheSizes sizes;

    gfxFontCache::GetCache()->AddSizeOfIncludingThis(&FontCacheMallocSizeOf,
                                                     &sizes);

    MOZ_COLLECT_REPORT(
        "explicit/gfx/font-cache", KIND_HEAP, UNITS_BYTES,
        sizes.mFontInstances,
        "Memory used for active font instances.");

    MOZ_COLLECT_REPORT(
        "explicit/gfx/font-shaped-words", KIND_HEAP, UNITS_BYTES,
        sizes.mShapedWords,
        "Memory used to cache shaped glyph data.");

    return NS_OK;
}

NS_IMPL_ISUPPORTS(gfxFontCache::Observer, nsIObserver)

NS_IMETHODIMP
gfxFontCache::Observer::Observe(nsISupports *aSubject,
                                const char *aTopic,
                                const char16_t *someData)
{
    if (!nsCRT::strcmp(aTopic, "memory-pressure")) {
        gfxFontCache *fontCache = gfxFontCache::GetCache();
        if (fontCache) {
            fontCache->FlushShapedWordCaches();
        }
    } else {
        NS_NOTREACHED("unexpected notification topic");
    }
    return NS_OK;
}

nsresult
gfxFontCache::Init()
{
    NS_ASSERTION(!gGlobalCache, "Where did this come from?");
    gGlobalCache = new gfxFontCache();
    if (!gGlobalCache) {
        return NS_ERROR_OUT_OF_MEMORY;
    }
    RegisterStrongMemoryReporter(new MemoryReporter());
    return NS_OK;
}

void
gfxFontCache::Shutdown()
{
    delete gGlobalCache;
    gGlobalCache = nullptr;

#ifdef DEBUG_TEXT_RUN_STORAGE_METRICS
    printf("Textrun storage high water mark=%d\n", gTextRunStorageHighWaterMark);
    printf("Total number of fonts=%d\n", gFontCount);
    printf("Total glyph extents allocated=%d (size %d)\n", gGlyphExtentsCount,
            int(gGlyphExtentsCount*sizeof(gfxGlyphExtents)));
    printf("Total glyph extents width-storage size allocated=%d\n", gGlyphExtentsWidthsTotalSize);
    printf("Number of simple glyph extents eagerly requested=%d\n", gGlyphExtentsSetupEagerSimple);
    printf("Number of tight glyph extents eagerly requested=%d\n", gGlyphExtentsSetupEagerTight);
    printf("Number of tight glyph extents lazily requested=%d\n", gGlyphExtentsSetupLazyTight);
    printf("Number of simple glyph extent setups that fell back to tight=%d\n", gGlyphExtentsSetupFallBackToTight);
#endif
}

gfxFontCache::gfxFontCache()
    : nsExpirationTracker<gfxFont,3>(FONT_TIMEOUT_SECONDS * 1000,
                                     "gfxFontCache")
{
    nsCOMPtr<nsIObserverService> obs = GetObserverService();
    if (obs) {
        obs->AddObserver(new Observer, "memory-pressure", false);
    }

#ifndef RELEASE_OR_BETA
    // Currently disabled for release builds, due to unexplained crashes
    // during expiration; see bug 717175 & 894798.
    mWordCacheExpirationTimer = do_CreateInstance("@mozilla.org/timer;1");
    if (mWordCacheExpirationTimer) {
        mWordCacheExpirationTimer->
            InitWithFuncCallback(WordCacheExpirationTimerCallback, this,
                                 SHAPED_WORD_TIMEOUT_SECONDS * 1000,
                                 nsITimer::TYPE_REPEATING_SLACK);
    }
#endif
}

gfxFontCache::~gfxFontCache()
{
    // Ensure the user font cache releases its references to font entries,
    // so they aren't kept alive after the font instances and font-list
    // have been shut down.
    gfxUserFontSet::UserFontCache::Shutdown();

    if (mWordCacheExpirationTimer) {
        mWordCacheExpirationTimer->Cancel();
        mWordCacheExpirationTimer = nullptr;
    }

    // Expire everything that has a zero refcount, so we don't leak them.
    AgeAllGenerations();
    // All fonts should be gone.
    NS_WARNING_ASSERTION(mFonts.Count() == 0,
                         "Fonts still alive while shutting down gfxFontCache");
    // Note that we have to delete everything through the expiration
    // tracker, since there might be fonts not in the hashtable but in
    // the tracker.
}

bool
gfxFontCache::HashEntry::KeyEquals(const KeyTypePointer aKey) const
{
    const gfxCharacterMap* fontUnicodeRangeMap = mFont->GetUnicodeRangeMap();
    return aKey->mFontEntry == mFont->GetFontEntry() &&
           aKey->mStyle->Equals(*mFont->GetStyle()) &&
           ((!aKey->mUnicodeRangeMap && !fontUnicodeRangeMap) ||
            (aKey->mUnicodeRangeMap && fontUnicodeRangeMap &&
             aKey->mUnicodeRangeMap->Equals(fontUnicodeRangeMap)));
}

already_AddRefed<gfxFont>
gfxFontCache::Lookup(const gfxFontEntry* aFontEntry,
                     const gfxFontStyle* aStyle,
                     const gfxCharacterMap* aUnicodeRangeMap)
{
    Key key(aFontEntry, aStyle, aUnicodeRangeMap);
    HashEntry *entry = mFonts.GetEntry(key);

    Telemetry::Accumulate(Telemetry::FONT_CACHE_HIT, entry != nullptr);
    if (!entry)
        return nullptr;

    RefPtr<gfxFont> font = entry->mFont;
    return font.forget();
}

void
gfxFontCache::AddNew(gfxFont *aFont)
{
    Key key(aFont->GetFontEntry(), aFont->GetStyle(),
            aFont->GetUnicodeRangeMap());
    HashEntry *entry = mFonts.PutEntry(key);
    if (!entry)
        return;
    gfxFont *oldFont = entry->mFont;
    entry->mFont = aFont;
    // Assert that we can find the entry we just put in (this fails if the key
    // has a NaN float value in it, e.g. 'sizeAdjust').
    MOZ_ASSERT(entry == mFonts.GetEntry(key));
    // If someone's asked us to replace an existing font entry, then that's a
    // bit weird, but let it happen, and expire the old font if it's not used.
    if (oldFont && oldFont->GetExpirationState()->IsTracked()) {
        // if oldFont == aFont, recount should be > 0,
        // so we shouldn't be here.
        NS_ASSERTION(aFont != oldFont, "new font is tracked for expiry!");
        NotifyExpired(oldFont);
    }
}

void
gfxFontCache::NotifyReleased(gfxFont *aFont)
{
    nsresult rv = AddObject(aFont);
    if (NS_FAILED(rv)) {
        // We couldn't track it for some reason. Kill it now.
        DestroyFont(aFont);
    }
    // Note that we might have fonts that aren't in the hashtable, perhaps because
    // of OOM adding to the hashtable or because someone did an AddNew where
    // we already had a font. These fonts are added to the expiration tracker
    // anyway, even though Lookup can't resurrect them. Eventually they will
    // expire and be deleted.
}

void
gfxFontCache::NotifyExpired(gfxFont *aFont)
{
    aFont->ClearCachedWords();
    RemoveObject(aFont);
    DestroyFont(aFont);
}

void
gfxFontCache::DestroyFont(gfxFont *aFont)
{
    Key key(aFont->GetFontEntry(), aFont->GetStyle(),
            aFont->GetUnicodeRangeMap());
    HashEntry *entry = mFonts.GetEntry(key);
    if (entry && entry->mFont == aFont) {
        mFonts.RemoveEntry(entry);
    }
    NS_ASSERTION(aFont->GetRefCount() == 0,
                 "Destroying with non-zero ref count!");
    delete aFont;
}

/*static*/
void
gfxFontCache::WordCacheExpirationTimerCallback(nsITimer* aTimer, void* aCache)
{
    gfxFontCache* cache = static_cast<gfxFontCache*>(aCache);
    for (auto it = cache->mFonts.Iter(); !it.Done(); it.Next()) {
        it.Get()->mFont->AgeCachedWords();
    }
}

void
gfxFontCache::FlushShapedWordCaches()
{
    for (auto it = mFonts.Iter(); !it.Done(); it.Next()) {
        it.Get()->mFont->ClearCachedWords();
    }
}

void
gfxFontCache::AddSizeOfExcludingThis(MallocSizeOf aMallocSizeOf,
                                     FontCacheSizes* aSizes) const
{
    // TODO: add the overhead of the expiration tracker (generation arrays)

    aSizes->mFontInstances += mFonts.ShallowSizeOfExcludingThis(aMallocSizeOf);
    for (auto iter = mFonts.ConstIter(); !iter.Done(); iter.Next()) {
        iter.Get()->mFont->AddSizeOfExcludingThis(aMallocSizeOf, aSizes);
    }
}

void
gfxFontCache::AddSizeOfIncludingThis(MallocSizeOf aMallocSizeOf,
                                     FontCacheSizes* aSizes) const
{
    aSizes->mFontInstances += aMallocSizeOf(this);
    AddSizeOfExcludingThis(aMallocSizeOf, aSizes);
}

#define MAX_SSXX_VALUE 99
#define MAX_CVXX_VALUE 99

static void
LookupAlternateValues(gfxFontFeatureValueSet *featureLookup,
                      const nsAString& aFamily,
                      const nsTArray<gfxAlternateValue>& altValue,
                      nsTArray<gfxFontFeature>& aFontFeatures)
{
    uint32_t numAlternates = altValue.Length();
    for (uint32_t i = 0; i < numAlternates; i++) {
        const gfxAlternateValue& av = altValue.ElementAt(i);
        AutoTArray<uint32_t,4> values;

        // map <family, name, feature> ==> <values>
        bool found =
            featureLookup->GetFontFeatureValuesFor(aFamily, av.alternate,
                                                   av.value, values);
        uint32_t numValues = values.Length();

        // nothing defined, skip
        if (!found || numValues == 0) {
            continue;
        }

        gfxFontFeature feature;
        if (av.alternate == NS_FONT_VARIANT_ALTERNATES_CHARACTER_VARIANT) {
            NS_ASSERTION(numValues <= 2,
                         "too many values allowed for character-variant");
            // character-variant(12 3) ==> 'cv12' = 3
            uint32_t nn = values.ElementAt(0);
            // ignore values greater than 99
            if (nn == 0 || nn > MAX_CVXX_VALUE) {
                continue;
            }
            feature.mValue = 1;
            if (numValues > 1) {
                feature.mValue = values.ElementAt(1);
            }
            feature.mTag = HB_TAG('c','v',('0' + nn / 10), ('0' + nn % 10));
            aFontFeatures.AppendElement(feature);

        } else if (av.alternate == NS_FONT_VARIANT_ALTERNATES_STYLESET) {
            // styleset(1 2 7) ==> 'ss01' = 1, 'ss02' = 1, 'ss07' = 1
            feature.mValue = 1;
            for (uint32_t v = 0; v < numValues; v++) {
                uint32_t nn = values.ElementAt(v);
                if (nn == 0 || nn > MAX_SSXX_VALUE) {
                    continue;
                }
                feature.mTag = HB_TAG('s','s',('0' + nn / 10), ('0' + nn % 10));
                aFontFeatures.AppendElement(feature);
            }

        } else {
            NS_ASSERTION(numValues == 1,
                   "too many values for font-specific font-variant-alternates");
            feature.mValue = values.ElementAt(0);

            switch (av.alternate) {
                case NS_FONT_VARIANT_ALTERNATES_STYLISTIC:  // salt
                    feature.mTag = HB_TAG('s','a','l','t');
                    break;
                case NS_FONT_VARIANT_ALTERNATES_SWASH:  // swsh, cswh
                    feature.mTag = HB_TAG('s','w','s','h');
                    aFontFeatures.AppendElement(feature);
                    feature.mTag = HB_TAG('c','s','w','h');
                    break;
                case NS_FONT_VARIANT_ALTERNATES_ORNAMENTS: // ornm
                    feature.mTag = HB_TAG('o','r','n','m');
                    break;
                case NS_FONT_VARIANT_ALTERNATES_ANNOTATION: // nalt
                    feature.mTag = HB_TAG('n','a','l','t');
                    break;
                default:
                    feature.mTag = 0;
                    break;
            }

            NS_ASSERTION(feature.mTag, "unsupported alternate type");
            if (!feature.mTag) {
                continue;
            }
            aFontFeatures.AppendElement(feature);
        }
    }
}

/* static */ void
gfxFontShaper::MergeFontFeatures(
    const gfxFontStyle *aStyle,
    const nsTArray<gfxFontFeature>& aFontFeatures,
    bool aDisableLigatures,
    const nsAString& aFamilyName,
    bool aAddSmallCaps,
    void (*aHandleFeature)(const uint32_t&, uint32_t&, void*),
    void* aHandleFeatureData)
{
    uint32_t numAlts = aStyle->alternateValues.Length();
    const nsTArray<gfxFontFeature>& styleRuleFeatures =
        aStyle->featureSettings;

    // Bail immediately if nothing to do, which is the common case.
    if (styleRuleFeatures.IsEmpty() &&
        aFontFeatures.IsEmpty() &&
        !aDisableLigatures &&
        aStyle->variantCaps == NS_FONT_VARIANT_CAPS_NORMAL &&
        aStyle->variantSubSuper == NS_FONT_VARIANT_POSITION_NORMAL &&
        numAlts == 0) {
        return;
    }

    nsDataHashtable<nsUint32HashKey,uint32_t> mergedFeatures;

    // Ligature features are enabled by default in the generic shaper,
    // so we explicitly turn them off if necessary (for letter-spacing)
    if (aDisableLigatures) {
        mergedFeatures.Put(HB_TAG('l','i','g','a'), 0);
        mergedFeatures.Put(HB_TAG('c','l','i','g'), 0);
    }

    // add feature values from font
    uint32_t i, count;

    count = aFontFeatures.Length();
    for (i = 0; i < count; i++) {
        const gfxFontFeature& feature = aFontFeatures.ElementAt(i);
        mergedFeatures.Put(feature.mTag, feature.mValue);
    }

    // font-variant-caps - handled here due to the need for fallback handling
    // petite caps cases can fallback to appropriate smallcaps
    uint32_t variantCaps = aStyle->variantCaps;
    switch (variantCaps) {
        case NS_FONT_VARIANT_CAPS_NORMAL:
            break;

        case NS_FONT_VARIANT_CAPS_ALLSMALL:
            mergedFeatures.Put(HB_TAG('c','2','s','c'), 1);
            // fall through to the small-caps case
            MOZ_FALLTHROUGH;

        case NS_FONT_VARIANT_CAPS_SMALLCAPS:
            mergedFeatures.Put(HB_TAG('s','m','c','p'), 1);
            break;

        case NS_FONT_VARIANT_CAPS_ALLPETITE:
            mergedFeatures.Put(aAddSmallCaps ? HB_TAG('c','2','s','c') :
                                               HB_TAG('c','2','p','c'), 1);
            // fall through to the petite-caps case
            MOZ_FALLTHROUGH;

        case NS_FONT_VARIANT_CAPS_PETITECAPS:
            mergedFeatures.Put(aAddSmallCaps ? HB_TAG('s','m','c','p') :
                                               HB_TAG('p','c','a','p'), 1);
            break;

        case NS_FONT_VARIANT_CAPS_TITLING:
            mergedFeatures.Put(HB_TAG('t','i','t','l'), 1);
            break;

        case NS_FONT_VARIANT_CAPS_UNICASE:
            mergedFeatures.Put(HB_TAG('u','n','i','c'), 1);
            break;

        default:
            MOZ_ASSERT_UNREACHABLE("Unexpected variantCaps");
            break;
    }

    // font-variant-position - handled here due to the need for fallback
    switch (aStyle->variantSubSuper) {
        case NS_FONT_VARIANT_POSITION_NORMAL:
            break;
        case NS_FONT_VARIANT_POSITION_SUPER:
            mergedFeatures.Put(HB_TAG('s','u','p','s'), 1);
            break;
        case NS_FONT_VARIANT_POSITION_SUB:
            mergedFeatures.Put(HB_TAG('s','u','b','s'), 1);
            break;
        default:
            MOZ_ASSERT_UNREACHABLE("Unexpected variantSubSuper");
            break;
    }

    // add font-specific feature values from style rules
    if (aStyle->featureValueLookup && numAlts > 0) {
        AutoTArray<gfxFontFeature,4> featureList;

        // insert list of alternate feature settings
        LookupAlternateValues(aStyle->featureValueLookup, aFamilyName,
                              aStyle->alternateValues, featureList);

        count = featureList.Length();
        for (i = 0; i < count; i++) {
            const gfxFontFeature& feature = featureList.ElementAt(i);
            mergedFeatures.Put(feature.mTag, feature.mValue);
        }
    }

    // add feature values from style rules
    count = styleRuleFeatures.Length();
    for (i = 0; i < count; i++) {
        const gfxFontFeature& feature = styleRuleFeatures.ElementAt(i);
        mergedFeatures.Put(feature.mTag, feature.mValue);
    }

    if (mergedFeatures.Count() != 0) {
        for (auto iter = mergedFeatures.Iter(); !iter.Done(); iter.Next()) {
            aHandleFeature(iter.Key(), iter.Data(), aHandleFeatureData);
        }
    }
}

// Work out whether cairo will snap inter-glyph spacing to pixels.
//
// Layout does not align text to pixel boundaries, so, with font drawing
// backends that snap glyph positions to pixels, it is important that
// inter-glyph spacing within words is always an integer number of pixels.
// This ensures that the drawing backend snaps all of the word's glyphs in the
// same direction and so inter-glyph spacing remains the same.
//
/* static */ void
gfxFontShaper::GetRoundOffsetsToPixels(DrawTarget* aDrawTarget,
                                       bool* aRoundX, bool* aRoundY)
{
    *aRoundX = false;
    // Could do something fancy here for ScaleFactors of
    // AxisAlignedTransforms, but we leave things simple.
    // Not much point rounding if a matrix will mess things up anyway.
    // Also return false for non-cairo contexts.
    if (aDrawTarget->GetTransform().HasNonTranslation()) {
        *aRoundY = false;
        return;
    }

    // All raster backends snap glyphs to pixels vertically.
    // Print backends set CAIRO_HINT_METRICS_OFF.
    *aRoundY = true;

    cairo_t* cr = gfxFont::RefCairo(aDrawTarget);
    cairo_scaled_font_t *scaled_font = cairo_get_scaled_font(cr);

    // bug 1198921 - this sometimes fails under Windows for whatver reason
    NS_ASSERTION(scaled_font, "null cairo scaled font should never be returned "
                 "by cairo_get_scaled_font");
    if (!scaled_font) {
        *aRoundX = true; // default to the same as the fallback path below
        return;
    }

    // Sometimes hint metrics gets set for us, most notably for printing.
    cairo_font_options_t *font_options = cairo_font_options_create();
    cairo_scaled_font_get_font_options(scaled_font, font_options);
    cairo_hint_metrics_t hint_metrics =
        cairo_font_options_get_hint_metrics(font_options);
    cairo_font_options_destroy(font_options);

    switch (hint_metrics) {
    case CAIRO_HINT_METRICS_OFF:
        *aRoundY = false;
        return;
    case CAIRO_HINT_METRICS_DEFAULT:
        // Here we mimic what cairo surface/font backends do.  Printing
        // surfaces have already been handled by hint_metrics.  The
        // fallback show_glyphs implementation composites pixel-aligned
        // glyph surfaces, so we just pick surface/font combinations that
        // override this.
        switch (cairo_scaled_font_get_type(scaled_font)) {
#if CAIRO_HAS_DWRITE_FONT // dwrite backend is not in std cairo releases yet
        case CAIRO_FONT_TYPE_DWRITE:
            // show_glyphs is implemented on the font and so is used for
            // all surface types; however, it may pixel-snap depending on
            // the dwrite rendering mode
            if (!cairo_dwrite_scaled_font_get_force_GDI_classic(scaled_font) &&
                gfxWindowsPlatform::GetPlatform()->DWriteMeasuringMode() ==
                    DWRITE_MEASURING_MODE_NATURAL) {
                return;
            }
            MOZ_FALLTHROUGH;
#endif
        case CAIRO_FONT_TYPE_QUARTZ:
            // Quartz surfaces implement show_glyphs for Quartz fonts
            if (cairo_surface_get_type(cairo_get_target(cr)) ==
                CAIRO_SURFACE_TYPE_QUARTZ) {
                return;
            }
            break;
        default:
            break;
        }
        break;
    case CAIRO_HINT_METRICS_ON:
        break;
    }
    *aRoundX = true;
}

void
gfxShapedText::SetupClusterBoundaries(uint32_t        aOffset,
                                      const char16_t *aString,
                                      uint32_t        aLength)
{
    CompressedGlyph *glyphs = GetCharacterGlyphs() + aOffset;

    gfxTextRun::CompressedGlyph extendCluster;
    extendCluster.SetComplex(false, true, 0);

    ClusterIterator iter(aString, aLength);

    // the ClusterIterator won't be able to tell us if the string
    // _begins_ with a cluster-extender, so we handle that here
    if (aLength) {
        uint32_t ch = *aString;
        if (aLength > 1 && NS_IS_HIGH_SURROGATE(ch) &&
            NS_IS_LOW_SURROGATE(aString[1])) {
            ch = SURROGATE_TO_UCS4(ch, aString[1]);
        }
        if (IsClusterExtender(ch)) {
            *glyphs = extendCluster;
        }
    }

    while (!iter.AtEnd()) {
        if (*iter == char16_t(' ')) {
            glyphs->SetIsSpace();
        }
        // advance iter to the next cluster-start (or end of text)
        iter.Next();
        // step past the first char of the cluster
        aString++;
        glyphs++;
        // mark all the rest as cluster-continuations
        while (aString < iter) {
            *glyphs = extendCluster;
            glyphs++;
            aString++;
        }
    }
}

void
gfxShapedText::SetupClusterBoundaries(uint32_t       aOffset,
                                      const uint8_t *aString,
                                      uint32_t       aLength)
{
    CompressedGlyph *glyphs = GetCharacterGlyphs() + aOffset;
    const uint8_t *limit = aString + aLength;

    while (aString < limit) {
        if (*aString == uint8_t(' ')) {
            glyphs->SetIsSpace();
        }
        aString++;
        glyphs++;
    }
}

gfxShapedText::DetailedGlyph *
gfxShapedText::AllocateDetailedGlyphs(uint32_t aIndex, uint32_t aCount)
{
    NS_ASSERTION(aIndex < GetLength(), "Index out of range");

    if (!mDetailedGlyphs) {
        mDetailedGlyphs = MakeUnique<DetailedGlyphStore>();
    }

    return mDetailedGlyphs->Allocate(aIndex, aCount);
}

void
gfxShapedText::SetGlyphs(uint32_t aIndex, CompressedGlyph aGlyph,
                         const DetailedGlyph *aGlyphs)
{
    NS_ASSERTION(!aGlyph.IsSimpleGlyph(), "Simple glyphs not handled here");
    NS_ASSERTION(aIndex > 0 || aGlyph.IsLigatureGroupStart(),
                 "First character can't be a ligature continuation!");

    uint32_t glyphCount = aGlyph.GetGlyphCount();
    if (glyphCount > 0) {
        DetailedGlyph *details = AllocateDetailedGlyphs(aIndex, glyphCount);
        memcpy(details, aGlyphs, sizeof(DetailedGlyph)*glyphCount);
    }
    GetCharacterGlyphs()[aIndex] = aGlyph;
}

#define ZWNJ 0x200C
#define ZWJ  0x200D
static inline bool
IsIgnorable(uint32_t aChar)
{
    return (IsDefaultIgnorable(aChar)) || aChar == ZWNJ || aChar == ZWJ;
}

void
gfxShapedText::SetMissingGlyph(uint32_t aIndex, uint32_t aChar, gfxFont *aFont)
{
    uint8_t category = GetGeneralCategory(aChar);
    if (category >= HB_UNICODE_GENERAL_CATEGORY_SPACING_MARK &&
        category <= HB_UNICODE_GENERAL_CATEGORY_NON_SPACING_MARK)
    {
        GetCharacterGlyphs()[aIndex].SetComplex(false, true, 0);
    }

    DetailedGlyph *details = AllocateDetailedGlyphs(aIndex, 1);

    details->mGlyphID = aChar;
    if (IsIgnorable(aChar)) {
        // Setting advance width to zero will prevent drawing the hexbox
        details->mAdvance = 0;
    } else {
        gfxFloat width =
            std::max(aFont->GetMetrics(gfxFont::eHorizontal).aveCharWidth,
                     gfxFloat(gfxFontMissingGlyphs::GetDesiredMinWidth(aChar,
                                mAppUnitsPerDevUnit)));
        details->mAdvance = uint32_t(width * mAppUnitsPerDevUnit);
    }
    details->mXOffset = 0;
    details->mYOffset = 0;
    GetCharacterGlyphs()[aIndex].SetMissing(1);
}

bool
gfxShapedText::FilterIfIgnorable(uint32_t aIndex, uint32_t aCh)
{
    if (IsIgnorable(aCh)) {
        // There are a few default-ignorables of Letter category (currently,
        // just the Hangul filler characters) that we'd better not discard
        // if they're followed by additional characters in the same cluster.
        // Some fonts use them to carry the width of a whole cluster of
        // combining jamos; see bug 1238243.
        if (GetGenCategory(aCh) == nsIUGenCategory::kLetter &&
            aIndex + 1 < GetLength() &&
            !GetCharacterGlyphs()[aIndex + 1].IsClusterStart()) {
            return false;
        }
        DetailedGlyph *details = AllocateDetailedGlyphs(aIndex, 1);
        details->mGlyphID = aCh;
        details->mAdvance = 0;
        details->mXOffset = 0;
        details->mYOffset = 0;
        GetCharacterGlyphs()[aIndex].SetMissing(1);
        return true;
    }
    return false;
}

void
gfxShapedText::AdjustAdvancesForSyntheticBold(float aSynBoldOffset,
                                              uint32_t aOffset,
                                              uint32_t aLength)
{
    uint32_t synAppUnitOffset = aSynBoldOffset * mAppUnitsPerDevUnit;
    CompressedGlyph *charGlyphs = GetCharacterGlyphs();
    for (uint32_t i = aOffset; i < aOffset + aLength; ++i) {
         CompressedGlyph *glyphData = charGlyphs + i;
         if (glyphData->IsSimpleGlyph()) {
             // simple glyphs ==> just add the advance
             int32_t advance = glyphData->GetSimpleAdvance() + synAppUnitOffset;
             if (CompressedGlyph::IsSimpleAdvance(advance)) {
                 glyphData->SetSimpleGlyph(advance, glyphData->GetSimpleGlyph());
             } else {
                 // rare case, tested by making this the default
                 uint32_t glyphIndex = glyphData->GetSimpleGlyph();
                 glyphData->SetComplex(true, true, 1);
                 DetailedGlyph detail = {glyphIndex, advance, 0, 0};
                 SetGlyphs(i, *glyphData, &detail);
             }
         } else {
             // complex glyphs ==> add offset at cluster/ligature boundaries
             uint32_t detailedLength = glyphData->GetGlyphCount();
             if (detailedLength) {
                 DetailedGlyph *details = GetDetailedGlyphs(i);
                 if (!details) {
                     continue;
                 }
                 if (IsRightToLeft()) {
                     details[0].mAdvance += synAppUnitOffset;
                 } else {
                     details[detailedLength - 1].mAdvance += synAppUnitOffset;
                 }
             }
         }
    }
}

void
gfxFont::RunMetrics::CombineWith(const RunMetrics& aOther, bool aOtherIsOnLeft)
{
    mAscent = std::max(mAscent, aOther.mAscent);
    mDescent = std::max(mDescent, aOther.mDescent);
    if (aOtherIsOnLeft) {
        mBoundingBox =
            (mBoundingBox + gfxPoint(aOther.mAdvanceWidth, 0)).Union(aOther.mBoundingBox);
    } else {
        mBoundingBox =
            mBoundingBox.Union(aOther.mBoundingBox + gfxPoint(mAdvanceWidth, 0));
    }
    mAdvanceWidth += aOther.mAdvanceWidth;
}

gfxFont::gfxFont(gfxFontEntry *aFontEntry, const gfxFontStyle *aFontStyle,
                 AntialiasOption anAAOption, cairo_scaled_font_t *aScaledFont) :
    mScaledFont(aScaledFont),
    mFontEntry(aFontEntry), mIsValid(true),
    mApplySyntheticBold(false),
    mMathInitialized(false),
    mStyle(*aFontStyle),
    mAdjustedSize(0.0),
    mFUnitsConvFactor(-1.0f), // negative to indicate "not yet initialized"
    mAntialiasOption(anAAOption)
{
#ifdef DEBUG_TEXT_RUN_STORAGE_METRICS
    ++gFontCount;
#endif
    mKerningSet = HasFeatureSet(HB_TAG('k','e','r','n'), mKerningEnabled);
}

gfxFont::~gfxFont()
{
    mFontEntry->NotifyFontDestroyed(this);

    if (mGlyphChangeObservers) {
        for (auto it = mGlyphChangeObservers->Iter(); !it.Done(); it.Next()) {
            it.Get()->GetKey()->ForgetFont();
        }
    }
}

gfxFloat
gfxFont::GetGlyphHAdvance(DrawTarget* aDrawTarget, uint16_t aGID)
{
    if (!SetupCairoFont(aDrawTarget)) {
        return 0;
    }
    if (ProvidesGlyphWidths()) {
        return GetGlyphWidth(*aDrawTarget, aGID) / 65536.0;
    }
    if (mFUnitsConvFactor < 0.0f) {
        GetMetrics(eHorizontal);
    }
    NS_ASSERTION(mFUnitsConvFactor >= 0.0f,
                 "missing font unit conversion factor");
    if (!mHarfBuzzShaper) {
        mHarfBuzzShaper = MakeUnique<gfxHarfBuzzShaper>(this);
    }
    gfxHarfBuzzShaper* shaper =
        static_cast<gfxHarfBuzzShaper*>(mHarfBuzzShaper.get());
    if (!shaper->Initialize()) {
        return 0;
    }
    return shaper->GetGlyphHAdvance(aGID) / 65536.0;
}

static void
CollectLookupsByFeature(hb_face_t *aFace, hb_tag_t aTableTag,
                        uint32_t aFeatureIndex, hb_set_t *aLookups)
{
    uint32_t lookups[32];
    uint32_t i, len, offset;

    offset = 0;
    do {
        len = ArrayLength(lookups);
        hb_ot_layout_feature_get_lookups(aFace, aTableTag, aFeatureIndex,
                                         offset, &len, lookups);
        for (i = 0; i < len; i++) {
            hb_set_add(aLookups, lookups[i]);
        }
        offset += len;
    } while (len == ArrayLength(lookups));
}

static void
CollectLookupsByLanguage(hb_face_t *aFace, hb_tag_t aTableTag,
                         const nsTHashtable<nsUint32HashKey>&
                             aSpecificFeatures,
                         hb_set_t *aOtherLookups,
                         hb_set_t *aSpecificFeatureLookups,
                         uint32_t aScriptIndex, uint32_t aLangIndex)
{
    uint32_t reqFeatureIndex;
    if (hb_ot_layout_language_get_required_feature_index(aFace, aTableTag,
                                                         aScriptIndex,
                                                         aLangIndex,
                                                         &reqFeatureIndex)) {
        CollectLookupsByFeature(aFace, aTableTag, reqFeatureIndex,
                                aOtherLookups);
    }

    uint32_t featureIndexes[32];
    uint32_t i, len, offset;

    offset = 0;
    do {
        len = ArrayLength(featureIndexes);
        hb_ot_layout_language_get_feature_indexes(aFace, aTableTag,
                                                  aScriptIndex, aLangIndex,
                                                  offset, &len, featureIndexes);

        for (i = 0; i < len; i++) {
            uint32_t featureIndex = featureIndexes[i];

            // get the feature tag
            hb_tag_t featureTag;
            uint32_t tagLen = 1;
            hb_ot_layout_language_get_feature_tags(aFace, aTableTag,
                                                   aScriptIndex, aLangIndex,
                                                   offset + i, &tagLen,
                                                   &featureTag);

            // collect lookups
            hb_set_t *lookups = aSpecificFeatures.GetEntry(featureTag) ?
                                    aSpecificFeatureLookups : aOtherLookups;
            CollectLookupsByFeature(aFace, aTableTag, featureIndex, lookups);
        }
        offset += len;
    } while (len == ArrayLength(featureIndexes));
}

static bool
HasLookupRuleWithGlyphByScript(hb_face_t *aFace, hb_tag_t aTableTag,
                               hb_tag_t aScriptTag, uint32_t aScriptIndex,
                               uint16_t aGlyph,
                               const nsTHashtable<nsUint32HashKey>&
                                   aDefaultFeatures,
                               bool& aHasDefaultFeatureWithGlyph)
{
    uint32_t numLangs, lang;
    hb_set_t *defaultFeatureLookups = hb_set_create();
    hb_set_t *nonDefaultFeatureLookups = hb_set_create();

    // default lang
    CollectLookupsByLanguage(aFace, aTableTag, aDefaultFeatures,
                             nonDefaultFeatureLookups, defaultFeatureLookups,
                             aScriptIndex,
                             HB_OT_LAYOUT_DEFAULT_LANGUAGE_INDEX);

    // iterate over langs
    numLangs = hb_ot_layout_script_get_language_tags(aFace, aTableTag,
                                                     aScriptIndex, 0,
                                                     nullptr, nullptr);
    for (lang = 0; lang < numLangs; lang++) {
        CollectLookupsByLanguage(aFace, aTableTag, aDefaultFeatures,
                                 nonDefaultFeatureLookups,
                                 defaultFeatureLookups,
                                 aScriptIndex, lang);
    }

    // look for the glyph among default feature lookups
    aHasDefaultFeatureWithGlyph = false;
    hb_set_t *glyphs = hb_set_create();
    hb_codepoint_t index = -1;
    while (hb_set_next(defaultFeatureLookups, &index)) {
        hb_ot_layout_lookup_collect_glyphs(aFace, aTableTag, index,
                                           glyphs, glyphs, glyphs,
                                           nullptr);
        if (hb_set_has(glyphs, aGlyph)) {
            aHasDefaultFeatureWithGlyph = true;
            break;
        }
    }

    // look for the glyph among non-default feature lookups
    // if no default feature lookups contained spaces
    bool hasNonDefaultFeatureWithGlyph = false;
    if (!aHasDefaultFeatureWithGlyph) {
        hb_set_clear(glyphs);
        index = -1;
        while (hb_set_next(nonDefaultFeatureLookups, &index)) {
            hb_ot_layout_lookup_collect_glyphs(aFace, aTableTag, index,
                                               glyphs, glyphs, glyphs,
                                               nullptr);
            if (hb_set_has(glyphs, aGlyph)) {
                hasNonDefaultFeatureWithGlyph = true;
                break;
            }
        }
    }

    hb_set_destroy(glyphs);
    hb_set_destroy(defaultFeatureLookups);
    hb_set_destroy(nonDefaultFeatureLookups);

    return aHasDefaultFeatureWithGlyph || hasNonDefaultFeatureWithGlyph;
}

static void
HasLookupRuleWithGlyph(hb_face_t *aFace, hb_tag_t aTableTag, bool& aHasGlyph,
                       hb_tag_t aSpecificFeature, bool& aHasGlyphSpecific,
                       uint16_t aGlyph)
{
    // iterate over the scripts in the font
    uint32_t numScripts, numLangs, script, lang;
    hb_set_t *otherLookups = hb_set_create();
    hb_set_t *specificFeatureLookups = hb_set_create();
    nsTHashtable<nsUint32HashKey> specificFeature;

    specificFeature.PutEntry(aSpecificFeature);

    numScripts = hb_ot_layout_table_get_script_tags(aFace, aTableTag, 0,
                                                    nullptr, nullptr);

    for (script = 0; script < numScripts; script++) {
        // default lang
        CollectLookupsByLanguage(aFace, aTableTag, specificFeature,
                                 otherLookups, specificFeatureLookups,
                                 script, HB_OT_LAYOUT_DEFAULT_LANGUAGE_INDEX);

        // iterate over langs
        numLangs = hb_ot_layout_script_get_language_tags(aFace, HB_OT_TAG_GPOS,
                                                         script, 0,
                                                         nullptr, nullptr);
        for (lang = 0; lang < numLangs; lang++) {
            CollectLookupsByLanguage(aFace, aTableTag, specificFeature,
                                     otherLookups, specificFeatureLookups,
                                     script, lang);
        }
    }

    // look for the glyph among non-specific feature lookups
    hb_set_t *glyphs = hb_set_create();
    hb_codepoint_t index = -1;
    while (hb_set_next(otherLookups, &index)) {
        hb_ot_layout_lookup_collect_glyphs(aFace, aTableTag, index,
                                           glyphs, glyphs, glyphs,
                                           nullptr);
        if (hb_set_has(glyphs, aGlyph)) {
            aHasGlyph = true;
            break;
        }
    }

    // look for the glyph among specific feature lookups
    hb_set_clear(glyphs);
    index = -1;
    while (hb_set_next(specificFeatureLookups, &index)) {
        hb_ot_layout_lookup_collect_glyphs(aFace, aTableTag, index,
                                           glyphs, glyphs, glyphs,
                                           nullptr);
        if (hb_set_has(glyphs, aGlyph)) {
            aHasGlyphSpecific = true;
            break;
        }
    }

    hb_set_destroy(glyphs);
    hb_set_destroy(specificFeatureLookups);
    hb_set_destroy(otherLookups);
}

nsDataHashtable<nsUint32HashKey,Script> *gfxFont::sScriptTagToCode = nullptr;
nsTHashtable<nsUint32HashKey>           *gfxFont::sDefaultFeatures = nullptr;

static inline bool
HasSubstitution(uint32_t *aBitVector, Script aScript) {
    return (aBitVector[static_cast<uint32_t>(aScript) >> 5]
           & (1 << (static_cast<uint32_t>(aScript) & 0x1f))) != 0;
}

// union of all default substitution features across scripts
static const hb_tag_t defaultFeatures[] = {
    HB_TAG('a','b','v','f'),
    HB_TAG('a','b','v','s'),
    HB_TAG('a','k','h','n'),
    HB_TAG('b','l','w','f'),
    HB_TAG('b','l','w','s'),
    HB_TAG('c','a','l','t'),
    HB_TAG('c','c','m','p'),
    HB_TAG('c','f','a','r'),
    HB_TAG('c','j','c','t'),
    HB_TAG('c','l','i','g'),
    HB_TAG('f','i','n','2'),
    HB_TAG('f','i','n','3'),
    HB_TAG('f','i','n','a'),
    HB_TAG('h','a','l','f'),
    HB_TAG('h','a','l','n'),
    HB_TAG('i','n','i','t'),
    HB_TAG('i','s','o','l'),
    HB_TAG('l','i','g','a'),
    HB_TAG('l','j','m','o'),
    HB_TAG('l','o','c','l'),
    HB_TAG('l','t','r','a'),
    HB_TAG('l','t','r','m'),
    HB_TAG('m','e','d','2'),
    HB_TAG('m','e','d','i'),
    HB_TAG('m','s','e','t'),
    HB_TAG('n','u','k','t'),
    HB_TAG('p','r','e','f'),
    HB_TAG('p','r','e','s'),
    HB_TAG('p','s','t','f'),
    HB_TAG('p','s','t','s'),
    HB_TAG('r','c','l','t'),
    HB_TAG('r','l','i','g'),
    HB_TAG('r','k','r','f'),
    HB_TAG('r','p','h','f'),
    HB_TAG('r','t','l','a'),
    HB_TAG('r','t','l','m'),
    HB_TAG('t','j','m','o'),
    HB_TAG('v','a','t','u'),
    HB_TAG('v','e','r','t'),
    HB_TAG('v','j','m','o')
};

void
gfxFont::CheckForFeaturesInvolvingSpace()
{
    mFontEntry->mHasSpaceFeaturesInitialized = true;

    bool log = LOG_FONTINIT_ENABLED();
    TimeStamp start;
    if (MOZ_UNLIKELY(log)) {
        start = TimeStamp::Now();
    }

    bool result = false;

    uint32_t spaceGlyph = GetSpaceGlyph();
    if (!spaceGlyph) {
        return;
    }

    hb_face_t *face = GetFontEntry()->GetHBFace();

    // GSUB lookups - examine per script
    if (hb_ot_layout_has_substitution(face)) {

        // set up the script ==> code hashtable if needed
        if (!sScriptTagToCode) {
            sScriptTagToCode =
                new nsDataHashtable<nsUint32HashKey,
                                    Script>(size_t(Script::NUM_SCRIPT_CODES));
            sScriptTagToCode->Put(HB_TAG('D','F','L','T'), Script::COMMON);
            for (Script s = Script::ARABIC; s < Script::NUM_SCRIPT_CODES;
                 s = Script(static_cast<int>(s) + 1)) {
                hb_script_t scriptTag = hb_script_t(GetScriptTagForCode(s));
                hb_tag_t s1, s2;
                hb_ot_tags_from_script(scriptTag, &s1, &s2);
                sScriptTagToCode->Put(s1, s);
                if (s2 != HB_OT_TAG_DEFAULT_SCRIPT) {
                    sScriptTagToCode->Put(s2, s);
                }
            }

            uint32_t numDefaultFeatures = ArrayLength(defaultFeatures);
            sDefaultFeatures =
                new nsTHashtable<nsUint32HashKey>(numDefaultFeatures);
            for (uint32_t i = 0; i < numDefaultFeatures; i++) {
                sDefaultFeatures->PutEntry(defaultFeatures[i]);
            }
        }

        // iterate over the scripts in the font
        hb_tag_t scriptTags[8];

        uint32_t len, offset = 0;
        do {
            len = ArrayLength(scriptTags);
            hb_ot_layout_table_get_script_tags(face, HB_OT_TAG_GSUB, offset,
                                               &len, scriptTags);
            for (uint32_t i = 0; i < len; i++) {
                bool isDefaultFeature = false;
                Script s;
                if (!HasLookupRuleWithGlyphByScript(face, HB_OT_TAG_GSUB,
                                                    scriptTags[i], offset + i,
                                                    spaceGlyph,
                                                    *sDefaultFeatures,
                                                    isDefaultFeature) ||
                    !sScriptTagToCode->Get(scriptTags[i], &s))
                {
                    continue;
                }
                result = true;
                uint32_t index = static_cast<uint32_t>(s) >> 5;
                uint32_t bit = static_cast<uint32_t>(s) & 0x1f;
                if (isDefaultFeature) {
                    mFontEntry->mDefaultSubSpaceFeatures[index] |= (1 << bit);
                } else {
                    mFontEntry->mNonDefaultSubSpaceFeatures[index] |= (1 << bit);
                }
            }
            offset += len;
        } while (len == ArrayLength(scriptTags));
    }

    // spaces in default features of default script?
    // ==> can't use word cache, skip GPOS analysis
    bool canUseWordCache = true;
    if (HasSubstitution(mFontEntry->mDefaultSubSpaceFeatures,
                        Script::COMMON)) {
        canUseWordCache = false;
    }

    // GPOS lookups - distinguish kerning from non-kerning features
    mFontEntry->mHasSpaceFeaturesKerning = false;
    mFontEntry->mHasSpaceFeaturesNonKerning = false;

    if (canUseWordCache && hb_ot_layout_has_positioning(face)) {
        bool hasKerning = false, hasNonKerning = false;
        HasLookupRuleWithGlyph(face, HB_OT_TAG_GPOS, hasNonKerning,
                               HB_TAG('k','e','r','n'), hasKerning, spaceGlyph);
        if (hasKerning || hasNonKerning) {
            result = true;
        }
        mFontEntry->mHasSpaceFeaturesKerning = hasKerning;
        mFontEntry->mHasSpaceFeaturesNonKerning = hasNonKerning;
    }

    hb_face_destroy(face);
    mFontEntry->mHasSpaceFeatures = result;

    if (MOZ_UNLIKELY(log)) {
        TimeDuration elapsed = TimeStamp::Now() - start;
        LOG_FONTINIT((
            "(fontinit-spacelookups) font: %s - "
            "subst default: %8.8x %8.8x %8.8x %8.8x "
            "subst non-default: %8.8x %8.8x %8.8x %8.8x "
            "kerning: %s non-kerning: %s time: %6.3f\n",
            NS_ConvertUTF16toUTF8(mFontEntry->Name()).get(),
            mFontEntry->mDefaultSubSpaceFeatures[3],
            mFontEntry->mDefaultSubSpaceFeatures[2],
            mFontEntry->mDefaultSubSpaceFeatures[1],
            mFontEntry->mDefaultSubSpaceFeatures[0],
            mFontEntry->mNonDefaultSubSpaceFeatures[3],
            mFontEntry->mNonDefaultSubSpaceFeatures[2],
            mFontEntry->mNonDefaultSubSpaceFeatures[1],
            mFontEntry->mNonDefaultSubSpaceFeatures[0],
            (mFontEntry->mHasSpaceFeaturesKerning ? "true" : "false"),
            (mFontEntry->mHasSpaceFeaturesNonKerning ? "true" : "false"),
            elapsed.ToMilliseconds()
        ));
    }
}

bool
gfxFont::HasSubstitutionRulesWithSpaceLookups(Script aRunScript)
{
    NS_ASSERTION(GetFontEntry()->mHasSpaceFeaturesInitialized,
                 "need to initialize space lookup flags");
    NS_ASSERTION(aRunScript < Script::NUM_SCRIPT_CODES, "weird script code");
    if (aRunScript == Script::INVALID ||
        aRunScript >= Script::NUM_SCRIPT_CODES) {
        return false;
    }

    // default features have space lookups ==> true
    if (HasSubstitution(mFontEntry->mDefaultSubSpaceFeatures,
                        Script::COMMON) ||
        HasSubstitution(mFontEntry->mDefaultSubSpaceFeatures,
                        aRunScript))
    {
        return true;
    }

    // non-default features have space lookups and some type of
    // font feature, in font or style is specified ==> true
    if ((HasSubstitution(mFontEntry->mNonDefaultSubSpaceFeatures,
                         Script::COMMON) ||
         HasSubstitution(mFontEntry->mNonDefaultSubSpaceFeatures,
                         aRunScript)) &&
        (!mStyle.featureSettings.IsEmpty() ||
         !mFontEntry->mFeatureSettings.IsEmpty()))
    {
        return true;
    }

    return false;
}

bool
gfxFont::SpaceMayParticipateInShaping(Script aRunScript)
{
    // avoid checking fonts known not to include default space-dependent features
    if (MOZ_UNLIKELY(mFontEntry->mSkipDefaultFeatureSpaceCheck)) {
        if (!mKerningSet && mStyle.featureSettings.IsEmpty() &&
            mFontEntry->mFeatureSettings.IsEmpty()) {
            return false;
        }
    }

    if (FontCanSupportGraphite()) {
        if (gfxPlatform::GetPlatform()->UseGraphiteShaping()) {
            return mFontEntry->HasGraphiteSpaceContextuals();
        }
    }

    // We record the presence of space-dependent features in the font entry
    // so that subsequent instantiations for the same font face won't
    // require us to re-check the tables; however, the actual check is done
    // by gfxFont because not all font entry subclasses know how to create
    // a harfbuzz face for introspection.
    if (!mFontEntry->mHasSpaceFeaturesInitialized) {
        CheckForFeaturesInvolvingSpace();
    }

    if (!mFontEntry->mHasSpaceFeatures) {
        return false;
    }

    // if font has substitution rules or non-kerning positioning rules
    // that involve spaces, bypass
    if (HasSubstitutionRulesWithSpaceLookups(aRunScript) ||
        mFontEntry->mHasSpaceFeaturesNonKerning) {
        return true;
    }

    // if kerning explicitly enabled/disabled via font-feature-settings or
    // font-kerning and kerning rules use spaces, only bypass when enabled
    if (mKerningSet && mFontEntry->mHasSpaceFeaturesKerning) {
        return mKerningEnabled;
    }

    return false;
}

bool
gfxFont::SupportsFeature(Script aScript, uint32_t aFeatureTag)
{
    if (mGraphiteShaper && gfxPlatform::GetPlatform()->UseGraphiteShaping()) {
        return GetFontEntry()->SupportsGraphiteFeature(aFeatureTag);
    }
    return GetFontEntry()->SupportsOpenTypeFeature(aScript, aFeatureTag);
}

bool
gfxFont::SupportsVariantCaps(Script aScript,
                             uint32_t aVariantCaps,
                             bool& aFallbackToSmallCaps,
                             bool& aSyntheticLowerToSmallCaps,
                             bool& aSyntheticUpperToSmallCaps)
{
    bool ok = true;  // cases without fallback are fine
    aFallbackToSmallCaps = false;
    aSyntheticLowerToSmallCaps = false;
    aSyntheticUpperToSmallCaps = false;
    switch (aVariantCaps) {
        case NS_FONT_VARIANT_CAPS_SMALLCAPS:
            ok = SupportsFeature(aScript, HB_TAG('s','m','c','p'));
            if (!ok) {
                aSyntheticLowerToSmallCaps = true;
            }
            break;
        case NS_FONT_VARIANT_CAPS_ALLSMALL:
            ok = SupportsFeature(aScript, HB_TAG('s','m','c','p')) &&
                 SupportsFeature(aScript, HB_TAG('c','2','s','c'));
            if (!ok) {
                aSyntheticLowerToSmallCaps = true;
                aSyntheticUpperToSmallCaps = true;
            }
            break;
        case NS_FONT_VARIANT_CAPS_PETITECAPS:
            ok = SupportsFeature(aScript, HB_TAG('p','c','a','p'));
            if (!ok) {
                ok = SupportsFeature(aScript, HB_TAG('s','m','c','p'));
                aFallbackToSmallCaps = ok;
            }
            if (!ok) {
                aSyntheticLowerToSmallCaps = true;
            }
            break;
        case NS_FONT_VARIANT_CAPS_ALLPETITE:
            ok = SupportsFeature(aScript, HB_TAG('p','c','a','p')) &&
                 SupportsFeature(aScript, HB_TAG('c','2','p','c'));
            if (!ok) {
                ok = SupportsFeature(aScript, HB_TAG('s','m','c','p')) &&
                     SupportsFeature(aScript, HB_TAG('c','2','s','c'));
                aFallbackToSmallCaps = ok;
            }
            if (!ok) {
                aSyntheticLowerToSmallCaps = true;
                aSyntheticUpperToSmallCaps = true;
            }
            break;
        default:
            break;
    }

    NS_ASSERTION(!(ok && (aSyntheticLowerToSmallCaps ||
                          aSyntheticUpperToSmallCaps)),
                 "shouldn't use synthetic features if we found real ones");

    NS_ASSERTION(!(!ok && aFallbackToSmallCaps),
                 "if we found a usable fallback, that counts as ok");

    return ok;
}

bool
gfxFont::SupportsSubSuperscript(uint32_t aSubSuperscript,
                                const uint8_t *aString,
                                uint32_t aLength, Script aRunScript)
{
    NS_ConvertASCIItoUTF16 unicodeString(reinterpret_cast<const char*>(aString),
                                         aLength);
    return SupportsSubSuperscript(aSubSuperscript, unicodeString.get(),
                                  aLength, aRunScript);
}

bool
gfxFont::SupportsSubSuperscript(uint32_t aSubSuperscript,
                                const char16_t *aString,
                                uint32_t aLength, Script aRunScript)
{
    NS_ASSERTION(aSubSuperscript == NS_FONT_VARIANT_POSITION_SUPER ||
                 aSubSuperscript == NS_FONT_VARIANT_POSITION_SUB,
                 "unknown value of font-variant-position");

    uint32_t feature = aSubSuperscript == NS_FONT_VARIANT_POSITION_SUPER ?
                       HB_TAG('s','u','p','s') : HB_TAG('s','u','b','s');

    if (!SupportsFeature(aRunScript, feature)) {
        return false;
    }

    // xxx - for graphite, don't really know how to sniff lookups so bail
    if (mGraphiteShaper && gfxPlatform::GetPlatform()->UseGraphiteShaping()) {
        return true;
    }

    if (!mHarfBuzzShaper) {
        mHarfBuzzShaper = MakeUnique<gfxHarfBuzzShaper>(this);
    }
    gfxHarfBuzzShaper* shaper =
        static_cast<gfxHarfBuzzShaper*>(mHarfBuzzShaper.get());
    if (!shaper->Initialize()) {
        return false;
    }

    // get the hbset containing input glyphs for the feature
    const hb_set_t *inputGlyphs = mFontEntry->InputsForOpenTypeFeature(aRunScript, feature);

    // create an hbset containing default glyphs for the script run
    hb_set_t *defaultGlyphsInRun = hb_set_create();

    // for each character, get the glyph id
    for (uint32_t i = 0; i < aLength; i++) {
        uint32_t ch = aString[i];

        if ((i + 1 < aLength) && NS_IS_HIGH_SURROGATE(ch) &&
                             NS_IS_LOW_SURROGATE(aString[i + 1])) {
            i++;
            ch = SURROGATE_TO_UCS4(ch, aString[i]);
        }

        if (ch == 0xa0) {
            ch = ' ';
        }

        hb_codepoint_t gid = shaper->GetNominalGlyph(ch);
        hb_set_add(defaultGlyphsInRun, gid);
    }

    // intersect with input glyphs, if size is not the same ==> fallback
    uint32_t origSize = hb_set_get_population(defaultGlyphsInRun);
    hb_set_intersect(defaultGlyphsInRun, inputGlyphs);
    uint32_t intersectionSize = hb_set_get_population(defaultGlyphsInRun);
    hb_set_destroy(defaultGlyphsInRun);

    return origSize == intersectionSize;
}

bool
gfxFont::HasFeatureSet(uint32_t aFeature, bool& aFeatureOn)
{
    aFeatureOn = false;

    if (mStyle.featureSettings.IsEmpty() &&
        GetFontEntry()->mFeatureSettings.IsEmpty()) {
        return false;
    }

    // add feature values from font
    bool featureSet = false;
    uint32_t i, count;

    nsTArray<gfxFontFeature>& fontFeatures = GetFontEntry()->mFeatureSettings;
    count = fontFeatures.Length();
    for (i = 0; i < count; i++) {
        const gfxFontFeature& feature = fontFeatures.ElementAt(i);
        if (feature.mTag == aFeature) {
            featureSet = true;
            aFeatureOn = (feature.mValue != 0);
        }
    }

    // add feature values from style rules
    nsTArray<gfxFontFeature>& styleFeatures = mStyle.featureSettings;
    count = styleFeatures.Length();
    for (i = 0; i < count; i++) {
        const gfxFontFeature& feature = styleFeatures.ElementAt(i);
        if (feature.mTag == aFeature) {
            featureSet = true;
            aFeatureOn = (feature.mValue != 0);
        }
    }

    return featureSet;
}

/**
 * A helper function in case we need to do any rounding or other
 * processing here.
 */
#define ToDeviceUnits(aAppUnits, aDevUnitsPerAppUnit) \
    (double(aAppUnits)*double(aDevUnitsPerAppUnit))

static AntialiasMode Get2DAAMode(gfxFont::AntialiasOption aAAOption) {
  switch (aAAOption) {
  case gfxFont::kAntialiasSubpixel:
    return AntialiasMode::SUBPIXEL;
  case gfxFont::kAntialiasGrayscale:
    return AntialiasMode::GRAY;
  case gfxFont::kAntialiasNone:
    return AntialiasMode::NONE;
  default:
    return AntialiasMode::DEFAULT;
  }
}

class GlyphBufferAzure
{
public:
    GlyphBufferAzure(const TextRunDrawParams& aRunParams,
                     const FontDrawParams&    aFontParams)
        : mRunParams(aRunParams)
        , mFontParams(aFontParams)
        , mNumGlyphs(0)
    {
    }

    ~GlyphBufferAzure()
    {
        Flush(true); // flush any remaining buffered glyphs
    }

    void OutputGlyph(uint32_t aGlyphID, const gfxPoint& aPt)
    {
        Glyph *glyph = AppendGlyph();
        glyph->mIndex = aGlyphID;
        glyph->mPosition.x = aPt.x;
        glyph->mPosition.y = aPt.y;
        glyph->mPosition = mFontParams.matInv.TransformPoint(glyph->mPosition);
        Flush(false); // this will flush only if the buffer is full
    }

    const TextRunDrawParams& mRunParams;
    const FontDrawParams& mFontParams;

private:
#define GLYPH_BUFFER_SIZE (2048/sizeof(Glyph))

    Glyph *AppendGlyph()
    {
        return &mGlyphBuffer[mNumGlyphs++];
    }

    static DrawMode
    GetStrokeMode(DrawMode aMode)
    {
        return aMode & (DrawMode::GLYPH_STROKE |
                        DrawMode::GLYPH_STROKE_UNDERNEATH);
    }

    // Render the buffered glyphs to the draw target and clear the buffer.
    // This actually flushes the glyphs only if the buffer is full, or if the
    // aFinish parameter is true; otherwise it simply returns.
    void Flush(bool aFinish)
    {
        // Ensure there's enough room for a glyph to be added to the buffer
        if ((!aFinish && mNumGlyphs < GLYPH_BUFFER_SIZE) || !mNumGlyphs) {
            return;
        }

        if (mRunParams.isRTL) {
            Glyph *begin = &mGlyphBuffer[0];
            Glyph *end = &mGlyphBuffer[mNumGlyphs];
            std::reverse(begin, end);
        }

        gfx::GlyphBuffer buf;
        buf.mGlyphs = mGlyphBuffer;
        buf.mNumGlyphs = mNumGlyphs;

        gfxContext::AzureState state = mRunParams.context->CurrentState();
        if (mRunParams.drawMode & DrawMode::GLYPH_FILL) {
            if (state.pattern || mFontParams.contextPaint) {
                Pattern *pat;

                RefPtr<gfxPattern> fillPattern;
                if (!mFontParams.contextPaint ||
                    !(fillPattern = mFontParams.contextPaint->GetFillPattern(
                                        mRunParams.context->GetDrawTarget(),
                                        mRunParams.context->CurrentMatrix()))) {
                    if (state.pattern) {
                        pat = state.pattern->GetPattern(mRunParams.dt,
                                      state.patternTransformChanged ?
                                          &state.patternTransform : nullptr);
                    } else {
                        pat = nullptr;
                    }
                } else {
                    pat = fillPattern->GetPattern(mRunParams.dt);
                }

                if (pat) {
                    Matrix saved;
                    Matrix *mat = nullptr;
                    if (mFontParams.passedInvMatrix) {
                        // The brush matrix needs to be multiplied with the
                        // inverted matrix as well, to move the brush into the
                        // space of the glyphs.

                        // This relies on the returned Pattern not to be reused
                        // by others, but regenerated on GetPattern calls. This
                        // is true!
                        if (pat->GetType() == PatternType::LINEAR_GRADIENT) {
                            mat = &static_cast<LinearGradientPattern*>(pat)->mMatrix;
                        } else if (pat->GetType() == PatternType::RADIAL_GRADIENT) {
                            mat = &static_cast<RadialGradientPattern*>(pat)->mMatrix;
                        } else if (pat->GetType() == PatternType::SURFACE) {
                            mat = &static_cast<SurfacePattern*>(pat)->mMatrix;
                        }

                        if (mat) {
                            saved = *mat;
                            *mat = (*mat) * (*mFontParams.passedInvMatrix);
                        }
                    }

                    mRunParams.dt->FillGlyphs(mFontParams.scaledFont, buf,
                                              *pat, mFontParams.drawOptions,
                                              mFontParams.renderingOptions);

                    if (mat) {
                        *mat = saved;
                    }
                }
            } else if (state.sourceSurface) {
                mRunParams.dt->FillGlyphs(mFontParams.scaledFont, buf,
                                          SurfacePattern(state.sourceSurface,
                                                         ExtendMode::CLAMP,
                                                         state.surfTransform),
                                          mFontParams.drawOptions,
                                          mFontParams.renderingOptions);
            } else {
                mRunParams.dt->FillGlyphs(mFontParams.scaledFont, buf,
                                          ColorPattern(state.color),
                                          mFontParams.drawOptions,
                                          mFontParams.renderingOptions);
            }
        }
        if (GetStrokeMode(mRunParams.drawMode) == DrawMode::GLYPH_STROKE &&
            mRunParams.strokeOpts) {
            Pattern *pat;
            if (mRunParams.textStrokePattern) {
                pat = mRunParams.textStrokePattern->GetPattern(
                  mRunParams.dt, state.patternTransformChanged
                                   ? &state.patternTransform
                                   : nullptr);

                if (pat) {
                    Matrix saved;
                    Matrix *mat = nullptr;
                    if (mFontParams.passedInvMatrix) {
                        // The brush matrix needs to be multiplied with the
                        // inverted matrix as well, to move the brush into the
                        // space of the glyphs.

                        // This relies on the returned Pattern not to be reused
                        // by others, but regenerated on GetPattern calls. This
                        // is true!
                        if (pat->GetType() == PatternType::LINEAR_GRADIENT) {
                            mat = &static_cast<LinearGradientPattern*>(pat)->mMatrix;
                        } else if (pat->GetType() == PatternType::RADIAL_GRADIENT) {
                            mat = &static_cast<RadialGradientPattern*>(pat)->mMatrix;
                        } else if (pat->GetType() == PatternType::SURFACE) {
                            mat = &static_cast<SurfacePattern*>(pat)->mMatrix;
                        }

                        if (mat) {
                            saved = *mat;
                            *mat = (*mat) * (*mFontParams.passedInvMatrix);
                        }
                    }
                    FlushStroke(buf, *pat);

                    if (mat) {
                        *mat = saved;
                    }
                }
            } else {
                FlushStroke(buf,
                            ColorPattern(
                              Color::FromABGR(mRunParams.textStrokeColor)));
            }
        }
        if (mRunParams.drawMode & DrawMode::GLYPH_PATH) {
            mRunParams.context->EnsurePathBuilder();
            Matrix mat = mRunParams.dt->GetTransform();
            mFontParams.scaledFont->CopyGlyphsToBuilder(
                buf, mRunParams.context->mPathBuilder, &mat);
        }

        mNumGlyphs = 0;
    }

    void FlushStroke(gfx::GlyphBuffer& aBuf, const Pattern& aPattern)
    {
        RefPtr<Path> path =
            mFontParams.scaledFont->GetPathForGlyphs(aBuf, mRunParams.dt);
        mRunParams.dt->Stroke(path, aPattern, *mRunParams.strokeOpts,
                              (mRunParams.drawOpts) ? *mRunParams.drawOpts
                                                    : DrawOptions());
    }

    Glyph        mGlyphBuffer[GLYPH_BUFFER_SIZE];
    unsigned int mNumGlyphs;

#undef GLYPH_BUFFER_SIZE
};

// Bug 674909. When synthetic bolding text by drawing twice, need to
// render using a pixel offset in device pixels, otherwise text
// doesn't appear bolded, it appears as if a bad text shadow exists
// when a non-identity transform exists.  Use an offset factor so that
// the second draw occurs at a constant offset in device pixels.

double
gfxFont::CalcXScale(DrawTarget* aDrawTarget)
{
    // determine magnitude of a 1px x offset in device space
    Size t = aDrawTarget->GetTransform().TransformSize(Size(1.0, 0.0));
    if (t.width == 1.0 && t.height == 0.0) {
        // short-circuit the most common case to avoid sqrt() and division
        return 1.0;
    }

    double m = sqrt(t.width * t.width + t.height * t.height);

    NS_ASSERTION(m != 0.0, "degenerate transform while synthetic bolding");
    if (m == 0.0) {
        return 0.0; // effectively disables offset
    }

    // scale factor so that offsets are 1px in device pixels
    return 1.0 / m;
}

// Draw an individual glyph at a specific location.
// *aPt is the glyph position in appUnits; it is converted to device
// coordinates (devPt) here.
void
gfxFont::DrawOneGlyph(uint32_t aGlyphID, double aAdvance, gfxPoint *aPt,
                      GlyphBufferAzure& aBuffer, bool *aEmittedGlyphs) const
{
    const TextRunDrawParams& runParams(aBuffer.mRunParams);
    const FontDrawParams& fontParams(aBuffer.mFontParams);

    double glyphX, glyphY;
    if (fontParams.isVerticalFont) {
        glyphX = aPt->x;
        if (runParams.isRTL) {
            aPt->y -= aAdvance;
            glyphY = aPt->y;
        } else {
            glyphY = aPt->y;
            aPt->y += aAdvance;
        }
    } else {
        glyphY = aPt->y;
        if (runParams.isRTL) {
            aPt->x -= aAdvance;
            glyphX = aPt->x;
        } else {
            glyphX = aPt->x;
            aPt->x += aAdvance;
        }
    }
    gfxPoint devPt(ToDeviceUnits(glyphX, runParams.devPerApp),
                   ToDeviceUnits(glyphY, runParams.devPerApp));

    if (fontParams.haveSVGGlyphs) {
        if (!runParams.paintSVGGlyphs) {
            return;
        }
        NS_WARNING_ASSERTION(
          runParams.drawMode != DrawMode::GLYPH_PATH,
          "Rendering SVG glyph despite request for glyph path");
        if (RenderSVGGlyph(runParams.context, devPt,
                           aGlyphID, fontParams.contextPaint,
                           runParams.callbacks, *aEmittedGlyphs)) {
            return;
        }
    }

    if (fontParams.haveColorGlyphs &&
        RenderColorGlyph(runParams.dt, runParams.context,
                         fontParams.scaledFont, fontParams.renderingOptions,
                         fontParams.drawOptions,
                         fontParams.matInv.TransformPoint(gfx::Point(devPt.x, devPt.y)),
                         aGlyphID)) {
        return;
    }

    aBuffer.OutputGlyph(aGlyphID, devPt);

    // Synthetic bolding (if required) by multi-striking.
    for (int32_t i = 0; i < fontParams.extraStrikes; ++i) {
        if (fontParams.isVerticalFont) {
            devPt.y += fontParams.synBoldOnePixelOffset;
        } else {
            devPt.x += fontParams.synBoldOnePixelOffset;
        }
        aBuffer.OutputGlyph(aGlyphID, devPt);
    }

    *aEmittedGlyphs = true;
}

// Draw a run of CharacterGlyph records from the given offset in aShapedText.
// Returns true if glyph paths were actually emitted.
bool
gfxFont::DrawGlyphs(const gfxShapedText      *aShapedText,
                    uint32_t                  aOffset, // offset in the textrun
                    uint32_t                  aCount, // length of run to draw
                    gfxPoint                 *aPt,
                    const TextRunDrawParams&  aRunParams,
                    const FontDrawParams&     aFontParams)
{
    bool emittedGlyphs = false;
    GlyphBufferAzure buffer(aRunParams, aFontParams);

    gfxFloat& inlineCoord = aFontParams.isVerticalFont ? aPt->y : aPt->x;

    if (aRunParams.spacing) {
        inlineCoord += aRunParams.isRTL ? -aRunParams.spacing[0].mBefore
                                        : aRunParams.spacing[0].mBefore;
    }

    const gfxShapedText::CompressedGlyph *glyphData =
        &aShapedText->GetCharacterGlyphs()[aOffset];

    for (uint32_t i = 0; i < aCount; ++i, ++glyphData) {
        if (glyphData->IsSimpleGlyph()) {
            DrawOneGlyph(glyphData->GetSimpleGlyph(),
                         glyphData->GetSimpleAdvance(),
                         aPt, buffer, &emittedGlyphs);
        } else {
            uint32_t glyphCount = glyphData->GetGlyphCount();
            if (glyphCount > 0) {
                const gfxShapedText::DetailedGlyph *details =
                    aShapedText->GetDetailedGlyphs(aOffset + i);
                NS_ASSERTION(details, "detailedGlyph should not be missing!");
                for (uint32_t j = 0; j < glyphCount; ++j, ++details) {
                    double advance = details->mAdvance;

                    if (glyphData->IsMissing()) {
                        // Default-ignorable chars will have zero advance width;
                        // we don't have to draw the hexbox for them.
                        if (aRunParams.drawMode != DrawMode::GLYPH_PATH &&
                            advance > 0) {
                            double glyphX = aPt->x;
                            double glyphY = aPt->y;
                            if (aRunParams.isRTL) {
                                if (aFontParams.isVerticalFont) {
                                    glyphY -= advance;
                                } else {
                                    glyphX -= advance;
                                }
                            }
                            Point pt(Float(ToDeviceUnits(glyphX, aRunParams.devPerApp)),
                                     Float(ToDeviceUnits(glyphY, aRunParams.devPerApp)));
                            Float advanceDevUnits =
                                Float(ToDeviceUnits(advance, aRunParams.devPerApp));
                            Float height = GetMetrics(eHorizontal).maxAscent;
                            Rect glyphRect = aFontParams.isVerticalFont ?
                                Rect(pt.x - height / 2, pt.y,
                                     height, advanceDevUnits) :
                                Rect(pt.x, pt.y - height,
                                     advanceDevUnits, height);

                            // If there's a fake-italic skew in effect as part
                            // of the drawTarget's transform, we need to remove
                            // this before drawing the hexbox. (Bug 983985)
                            Matrix oldMat;
                            if (aFontParams.passedInvMatrix) {
                                oldMat = aRunParams.dt->GetTransform();
                                aRunParams.dt->SetTransform(
                                    *aFontParams.passedInvMatrix * oldMat);
                            }

                            gfxFontMissingGlyphs::DrawMissingGlyph(
                                details->mGlyphID, glyphRect, *aRunParams.dt,
                                PatternFromState(aRunParams.context),
                                aShapedText->GetAppUnitsPerDevUnit());

                            // Restore the matrix, if we modified it before
                            // drawing the hexbox.
                            if (aFontParams.passedInvMatrix) {
                                aRunParams.dt->SetTransform(oldMat);
                            }
                        }
                    } else {
                        gfxPoint glyphXY(*aPt);
                        if (aFontParams.isVerticalFont) {
                            glyphXY.x += details->mYOffset;
                            glyphXY.y += details->mXOffset;
                        } else {
                            glyphXY.x += details->mXOffset;
                            glyphXY.y += details->mYOffset;
                        }
                        DrawOneGlyph(details->mGlyphID, advance, &glyphXY,
                                     buffer, &emittedGlyphs);
                    }

                    inlineCoord += aRunParams.isRTL ? -advance : advance;
                }
            }
        }

        if (aRunParams.spacing) {
            double space = aRunParams.spacing[i].mAfter;
            if (i + 1 < aCount) {
                space += aRunParams.spacing[i + 1].mBefore;
            }
            inlineCoord += aRunParams.isRTL ? -space : space;
        }
    }

    return emittedGlyphs;
}

// This method is mostly parallel to DrawGlyphs.
void
gfxFont::DrawEmphasisMarks(const gfxTextRun* aShapedText, gfxPoint* aPt,
                           uint32_t aOffset, uint32_t aCount,
                           const EmphasisMarkDrawParams& aParams)
{
    gfxFloat& inlineCoord = aParams.isVertical ? aPt->y : aPt->x;
    gfxTextRun::Range markRange(aParams.mark);
    gfxTextRun::DrawParams params(aParams.context);

    gfxFloat clusterStart = -std::numeric_limits<gfxFloat>::infinity();
    bool shouldDrawEmphasisMark = false;
    for (uint32_t i = 0, idx = aOffset; i < aCount; ++i, ++idx) {
        if (aParams.spacing) {
            inlineCoord += aParams.direction * aParams.spacing[i].mBefore;
        }
        if (aShapedText->IsClusterStart(idx) ||
            clusterStart == -std::numeric_limits<gfxFloat>::infinity()) {
            clusterStart = inlineCoord;
        }
        if (aShapedText->CharMayHaveEmphasisMark(idx)) {
            shouldDrawEmphasisMark = true;
        }
        inlineCoord += aParams.direction * aShapedText->GetAdvanceForGlyph(idx);
        if (shouldDrawEmphasisMark &&
            (i + 1 == aCount || aShapedText->IsClusterStart(idx + 1))) {
            gfxFloat clusterAdvance = inlineCoord - clusterStart;
            // Move the coord backward to get the needed start point.
            gfxFloat delta = (clusterAdvance + aParams.advance) / 2;
            inlineCoord -= delta;
            aParams.mark->Draw(markRange, *aPt, params);
            inlineCoord += delta;
            shouldDrawEmphasisMark = false;
        }
        if (aParams.spacing) {
            inlineCoord += aParams.direction * aParams.spacing[i].mAfter;
        }
    }
}

void
gfxFont::Draw(const gfxTextRun *aTextRun, uint32_t aStart, uint32_t aEnd,
              gfxPoint *aPt, const TextRunDrawParams& aRunParams,
              uint16_t aOrientation)
{
    NS_ASSERTION(aRunParams.drawMode == DrawMode::GLYPH_PATH ||
                 !(int(aRunParams.drawMode) & int(DrawMode::GLYPH_PATH)),
                 "GLYPH_PATH cannot be used with GLYPH_FILL, GLYPH_STROKE or GLYPH_STROKE_UNDERNEATH");

    if (aStart >= aEnd) {
        return;
    }

    FontDrawParams fontParams;

    if (aRunParams.drawOpts) {
        fontParams.drawOptions = *aRunParams.drawOpts;
    }

    fontParams.scaledFont = GetScaledFont(aRunParams.dt);
    if (!fontParams.scaledFont) {
        return;
    }

    fontParams.haveSVGGlyphs = GetFontEntry()->TryGetSVGData(this);
    fontParams.haveColorGlyphs = GetFontEntry()->TryGetColorGlyphs();
    fontParams.contextPaint = aRunParams.runContextPaint;
    fontParams.isVerticalFont =
        aOrientation == gfxTextRunFactory::TEXT_ORIENT_VERTICAL_UPRIGHT;

    bool sideways = false;
    gfxPoint origPt = *aPt;
    if (aRunParams.isVerticalRun && !fontParams.isVerticalFont) {
        sideways = true;
        aRunParams.context->Save();
        gfxPoint p(aPt->x * aRunParams.devPerApp,
                   aPt->y * aRunParams.devPerApp);
        const Metrics& metrics = GetMetrics(eHorizontal);
        // Get a matrix we can use to draw the (horizontally-shaped) textrun
        // with 90-degree CW rotation.
        const gfxFloat
            rotation = (aOrientation ==
                        gfxTextRunFactory::TEXT_ORIENT_VERTICAL_SIDEWAYS_LEFT)
                       ? -M_PI / 2.0 : M_PI / 2.0;
        gfxMatrix mat =
            aRunParams.context->CurrentMatrix().
            Translate(p).     // translate origin for rotation
            Rotate(rotation). // turn 90deg CCW (sideways-left) or CW (*-right)
            Translate(-p);    // undo the translation

        // If we're drawing rotated horizontal text for an element styled
        // text-orientation:mixed, the dominant baseline will be vertical-
        // centered. So in this case, we need to adjust the position so that
        // the rotated horizontal text (which uses an alphabetic baseline) will
        // look OK when juxtaposed with upright glyphs (rendered on a centered
        // vertical baseline). The adjustment here is somewhat ad hoc; we
        // should eventually look for baseline tables[1] in the fonts and use
        // those if available.
        // [1] See http://www.microsoft.com/typography/otspec/base.htm
        if (aTextRun->UseCenterBaseline()) {
            gfxPoint baseAdj(0, (metrics.emAscent - metrics.emDescent) / 2);
            mat.Translate(baseAdj);
        }

        aRunParams.context->SetMatrix(mat);
    }

    UniquePtr<SVGContextPaint> contextPaint;
    if (fontParams.haveSVGGlyphs && !fontParams.contextPaint) {
        // If no pattern is specified for fill, use the current pattern
        NS_ASSERTION((int(aRunParams.drawMode) & int(DrawMode::GLYPH_STROKE)) == 0,
                     "no pattern supplied for stroking text");
        RefPtr<gfxPattern> fillPattern = aRunParams.context->GetPattern();
        contextPaint.reset(
            new SimpleTextContextPaint(fillPattern, nullptr,
                                       aRunParams.context->CurrentMatrix()));
        fontParams.contextPaint = contextPaint.get();
    }

    // Synthetic-bold strikes are each offset one device pixel in run direction.
    // (these values are only needed if IsSyntheticBold() is true)
    if (IsSyntheticBold()) {
        double xscale = CalcXScale(aRunParams.context->GetDrawTarget());
        fontParams.synBoldOnePixelOffset = aRunParams.direction * xscale;
        if (xscale != 0.0) {
            // use as many strikes as needed for the the increased advance
            fontParams.extraStrikes =
                std::max(1, NS_lroundf(GetSyntheticBoldOffset() / xscale));
        }
    } else {
        fontParams.synBoldOnePixelOffset = 0;
        fontParams.extraStrikes = 0;
    }

    bool oldSubpixelAA = aRunParams.dt->GetPermitSubpixelAA();
    if (!AllowSubpixelAA()) {
        aRunParams.dt->SetPermitSubpixelAA(false);
    }

    Matrix mat;
    Matrix oldMat = aRunParams.dt->GetTransform();

    // This is nullptr when we have inverse-transformed glyphs and we need
    // to transform the Brush inside flush.
    fontParams.passedInvMatrix = nullptr;

    fontParams.renderingOptions = GetGlyphRenderingOptions(&aRunParams);
    fontParams.drawOptions.mAntialiasMode = Get2DAAMode(mAntialiasOption);

    // The cairo DrawTarget backend uses the cairo_scaled_font directly
    // and so has the font skew matrix applied already.
    if (mScaledFont &&
        aRunParams.dt->GetBackendType() != BackendType::CAIRO) {
        cairo_matrix_t matrix;
        cairo_scaled_font_get_font_matrix(mScaledFont, &matrix);
        if (matrix.xy != 0) {
            // If this matrix applies a skew, which can happen when drawing
            // oblique fonts, we will set the DrawTarget matrix to apply the
            // skew. We'll need to move the glyphs by the inverse of the skew to
            // get the glyphs positioned correctly in the new device space
            // though, since the font matrix should only be applied to drawing
            // the glyphs, and not to their position.
            mat = Matrix(matrix.xx, matrix.yx,
                         matrix.xy, matrix.yy,
                         matrix.x0, matrix.y0);

            mat._11 = mat._22 = 1.0;
            mat._21 /= GetAdjustedSize();

            aRunParams.dt->SetTransform(mat * oldMat);

            fontParams.matInv = mat;
            fontParams.matInv.Invert();

            fontParams.passedInvMatrix = &fontParams.matInv;
        }
    }

    gfxFloat& baseline = fontParams.isVerticalFont ? aPt->x : aPt->y;
    gfxFloat origBaseline = baseline;
    if (mStyle.baselineOffset != 0.0) {
        baseline +=
            mStyle.baselineOffset * aTextRun->GetAppUnitsPerDevUnit();
    }

    bool emittedGlyphs =
        DrawGlyphs(aTextRun, aStart, aEnd - aStart, aPt,
                   aRunParams, fontParams);

    baseline = origBaseline;

    if (aRunParams.callbacks && emittedGlyphs) {
        aRunParams.callbacks->NotifyGlyphPathEmitted();
    }

    aRunParams.dt->SetTransform(oldMat);
    aRunParams.dt->SetPermitSubpixelAA(oldSubpixelAA);

    if (sideways) {
        aRunParams.context->Restore();
        // adjust updated aPt to account for the transform we were using
        gfxFloat advance = aPt->x - origPt.x;
        if (aOrientation ==
            gfxTextRunFactory::TEXT_ORIENT_VERTICAL_SIDEWAYS_LEFT) {
            *aPt = gfxPoint(origPt.x, origPt.y - advance);
        } else {
            *aPt = gfxPoint(origPt.x, origPt.y + advance);
        }
    }
}

bool
gfxFont::RenderSVGGlyph(gfxContext *aContext, gfxPoint aPoint,
                        uint32_t aGlyphId, SVGContextPaint* aContextPaint) const
{
    if (!GetFontEntry()->HasSVGGlyph(aGlyphId)) {
        return false;
    }

    const gfxFloat devUnitsPerSVGUnit =
        GetAdjustedSize() / GetFontEntry()->UnitsPerEm();
    gfxContextMatrixAutoSaveRestore matrixRestore(aContext);

    aContext->Save();
    aContext->SetMatrix(
      aContext->CurrentMatrix().Translate(aPoint.x, aPoint.y).
                                Scale(devUnitsPerSVGUnit, devUnitsPerSVGUnit));

    aContextPaint->InitStrokeGeometry(aContext, devUnitsPerSVGUnit);

    bool rv = GetFontEntry()->RenderSVGGlyph(aContext, aGlyphId,
                                             aContextPaint);
    aContext->Restore();
    aContext->NewPath();
    return rv;
}

bool
gfxFont::RenderSVGGlyph(gfxContext *aContext, gfxPoint aPoint,
                        uint32_t aGlyphId, SVGContextPaint* aContextPaint,
                        gfxTextRunDrawCallbacks *aCallbacks,
                        bool& aEmittedGlyphs) const
{
    if (aCallbacks && aEmittedGlyphs) {
        aCallbacks->NotifyGlyphPathEmitted();
        aEmittedGlyphs = false;
    }
    return RenderSVGGlyph(aContext, aPoint, aGlyphId, aContextPaint);
}

bool
gfxFont::RenderColorGlyph(DrawTarget* aDrawTarget,
                          gfxContext* aContext,
                          mozilla::gfx::ScaledFont* scaledFont,
                          GlyphRenderingOptions* aRenderingOptions,
                          mozilla::gfx::DrawOptions aDrawOptions,
                          const mozilla::gfx::Point& aPoint,
                          uint32_t aGlyphId) const
{
    AutoTArray<uint16_t, 8> layerGlyphs;
    AutoTArray<mozilla::gfx::Color, 8> layerColors;

    mozilla::gfx::Color defaultColor;
    if (!aContext->GetDeviceColor(defaultColor)) {
        defaultColor = mozilla::gfx::Color(0, 0, 0);
    }
    if (!GetFontEntry()->GetColorLayersInfo(aGlyphId, defaultColor,
                                            layerGlyphs, layerColors)) {
        return false;
    }

    for (uint32_t layerIndex = 0; layerIndex < layerGlyphs.Length();
         layerIndex++) {
        Glyph glyph;
        glyph.mIndex = layerGlyphs[layerIndex];
        glyph.mPosition = aPoint;

        mozilla::gfx::GlyphBuffer buffer;
        buffer.mGlyphs = &glyph;
        buffer.mNumGlyphs = 1;

        aDrawTarget->FillGlyphs(scaledFont, buffer,
                                ColorPattern(layerColors[layerIndex]),
                                aDrawOptions, aRenderingOptions);
    }
    return true;
}

static void
UnionRange(gfxFloat aX, gfxFloat* aDestMin, gfxFloat* aDestMax)
{
    *aDestMin = std::min(*aDestMin, aX);
    *aDestMax = std::max(*aDestMax, aX);
}

// We get precise glyph extents if the textrun creator requested them, or
// if the font is a user font --- in which case the author may be relying
// on overflowing glyphs.
static bool
NeedsGlyphExtents(gfxFont *aFont, const gfxTextRun *aTextRun)
{
    return (aTextRun->GetFlags() & gfxTextRunFactory::TEXT_NEED_BOUNDING_BOX) ||
        aFont->GetFontEntry()->IsUserFont();
}

bool
gfxFont::IsSpaceGlyphInvisible(DrawTarget* aRefDrawTarget,
                               const gfxTextRun* aTextRun)
{
    if (!mFontEntry->mSpaceGlyphIsInvisibleInitialized &&
        GetAdjustedSize() >= 1.0) {
        gfxGlyphExtents *extents =
            GetOrCreateGlyphExtents(aTextRun->GetAppUnitsPerDevUnit());
        gfxRect glyphExtents;
        mFontEntry->mSpaceGlyphIsInvisible =
            extents->GetTightGlyphExtentsAppUnits(this, aRefDrawTarget,
                GetSpaceGlyph(), &glyphExtents) &&
            glyphExtents.IsEmpty();
        mFontEntry->mSpaceGlyphIsInvisibleInitialized = true;
    }
    return mFontEntry->mSpaceGlyphIsInvisible;
}

gfxFont::RunMetrics
gfxFont::Measure(const gfxTextRun *aTextRun,
                 uint32_t aStart, uint32_t aEnd,
                 BoundingBoxType aBoundingBoxType,
                 DrawTarget* aRefDrawTarget,
                 Spacing *aSpacing,
                 uint16_t aOrientation)
{
    // If aBoundingBoxType is TIGHT_HINTED_OUTLINE_EXTENTS
    // and the underlying cairo font may be antialiased,
    // we need to create a copy in order to avoid getting cached extents.
    // This is only used by MathML layout at present.
    if (aBoundingBoxType == TIGHT_HINTED_OUTLINE_EXTENTS &&
        mAntialiasOption != kAntialiasNone) {
        if (!mNonAAFont) {
            mNonAAFont = Move(CopyWithAntialiasOption(kAntialiasNone));
        }
        // if font subclass doesn't implement CopyWithAntialiasOption(),
        // it will return null and we'll proceed to use the existing font
        if (mNonAAFont) {
            return mNonAAFont->Measure(aTextRun, aStart, aEnd,
                                       TIGHT_HINTED_OUTLINE_EXTENTS,
                                       aRefDrawTarget, aSpacing, aOrientation);
        }
    }

    const int32_t appUnitsPerDevUnit = aTextRun->GetAppUnitsPerDevUnit();
    // Current position in appunits
    gfxFont::Orientation orientation =
        aOrientation == gfxTextRunFactory::TEXT_ORIENT_VERTICAL_UPRIGHT
        ? eVertical : eHorizontal;
    const gfxFont::Metrics& fontMetrics = GetMetrics(orientation);

    gfxFloat baselineOffset = 0;
    if (aTextRun->UseCenterBaseline() && orientation == eHorizontal) {
        // For a horizontal font being used in vertical writing mode with
        // text-orientation:mixed, the overall metrics we're accumulating
        // will be aimed at a center baseline. But this font's metrics were
        // based on the alphabetic baseline. So we compute a baseline offset
        // that will be applied to ascent/descent values and glyph rects
        // to effectively shift them relative to the baseline.
        // XXX Eventually we should probably use the BASE table, if present.
        // But it usually isn't, so we need an ad hoc adjustment for now.
        baselineOffset = appUnitsPerDevUnit *
            (fontMetrics.emAscent - fontMetrics.emDescent) / 2;
    }

    RunMetrics metrics;
    metrics.mAscent = fontMetrics.maxAscent * appUnitsPerDevUnit;
    metrics.mDescent = fontMetrics.maxDescent * appUnitsPerDevUnit;

    if (aStart == aEnd) {
        // exit now before we look at aSpacing[0], which is undefined
        metrics.mAscent -= baselineOffset;
        metrics.mDescent += baselineOffset;
        metrics.mBoundingBox = gfxRect(0, -metrics.mAscent,
                                       0, metrics.mAscent + metrics.mDescent);
        return metrics;
    }

    gfxFloat advanceMin = 0, advanceMax = 0;
    const gfxTextRun::CompressedGlyph *charGlyphs = aTextRun->GetCharacterGlyphs();
    bool isRTL = aTextRun->IsRightToLeft();
    double direction = aTextRun->GetDirection();
    bool needsGlyphExtents = NeedsGlyphExtents(this, aTextRun);
    gfxGlyphExtents *extents =
        ((aBoundingBoxType == LOOSE_INK_EXTENTS &&
            !needsGlyphExtents &&
            !aTextRun->HasDetailedGlyphs()) ||
         (MOZ_UNLIKELY(GetStyle()->sizeAdjust == 0.0)) ||
         (MOZ_UNLIKELY(GetStyle()->size == 0))) ? nullptr
        : GetOrCreateGlyphExtents(aTextRun->GetAppUnitsPerDevUnit());
    double x = 0;
    if (aSpacing) {
        x += direction*aSpacing[0].mBefore;
    }
    uint32_t spaceGlyph = GetSpaceGlyph();
    bool allGlyphsInvisible = true;
    uint32_t i;
    for (i = aStart; i < aEnd; ++i) {
        const gfxTextRun::CompressedGlyph *glyphData = &charGlyphs[i];
        if (glyphData->IsSimpleGlyph()) {
            double advance = glyphData->GetSimpleAdvance();
            uint32_t glyphIndex = glyphData->GetSimpleGlyph();
            if (glyphIndex != spaceGlyph ||
                !IsSpaceGlyphInvisible(aRefDrawTarget, aTextRun)) {
                allGlyphsInvisible = false;
            }
            // Only get the real glyph horizontal extent if we were asked
            // for the tight bounding box or we're in quality mode
            if ((aBoundingBoxType != LOOSE_INK_EXTENTS || needsGlyphExtents) &&
                extents){
                uint16_t extentsWidth = extents->GetContainedGlyphWidthAppUnits(glyphIndex);
                if (extentsWidth != gfxGlyphExtents::INVALID_WIDTH &&
                    aBoundingBoxType == LOOSE_INK_EXTENTS) {
                    UnionRange(x, &advanceMin, &advanceMax);
                    UnionRange(x + direction*extentsWidth, &advanceMin, &advanceMax);
                } else {
                    gfxRect glyphRect;
                    if (!extents->GetTightGlyphExtentsAppUnits(this,
                            aRefDrawTarget, glyphIndex, &glyphRect)) {
                        glyphRect = gfxRect(0, metrics.mBoundingBox.Y(),
                            advance, metrics.mBoundingBox.Height());
                    }
                    if (orientation == eVertical) {
                        Swap(glyphRect.x, glyphRect.y);
                        Swap(glyphRect.width, glyphRect.height);
                    }
                    if (isRTL) {
                        glyphRect -= gfxPoint(advance, 0);
                    }
                    glyphRect += gfxPoint(x, 0);
                    metrics.mBoundingBox = metrics.mBoundingBox.Union(glyphRect);
                }
            }
            x += direction*advance;
        } else {
            allGlyphsInvisible = false;
            uint32_t glyphCount = glyphData->GetGlyphCount();
            if (glyphCount > 0) {
                const gfxTextRun::DetailedGlyph *details =
                    aTextRun->GetDetailedGlyphs(i);
                NS_ASSERTION(details != nullptr,
                             "detailedGlyph record should not be missing!");
                uint32_t j;
                for (j = 0; j < glyphCount; ++j, ++details) {
                    uint32_t glyphIndex = details->mGlyphID;
                    gfxPoint glyphPt(x + details->mXOffset, details->mYOffset);
                    double advance = details->mAdvance;
                    gfxRect glyphRect;
                    if (glyphData->IsMissing() || !extents ||
                        !extents->GetTightGlyphExtentsAppUnits(this,
                                aRefDrawTarget, glyphIndex, &glyphRect)) {
                        // We might have failed to get glyph extents due to
                        // OOM or something
                        glyphRect = gfxRect(0, -metrics.mAscent,
                            advance, metrics.mAscent + metrics.mDescent);
                    }
                    if (orientation == eVertical) {
                        Swap(glyphRect.x, glyphRect.y);
                        Swap(glyphRect.width, glyphRect.height);
                    }
                    if (isRTL) {
                        glyphRect -= gfxPoint(advance, 0);
                    }
                    glyphRect += glyphPt;
                    metrics.mBoundingBox = metrics.mBoundingBox.Union(glyphRect);
                    x += direction*advance;
                }
            }
        }
        // Every other glyph type is ignored
        if (aSpacing) {
            double space = aSpacing[i - aStart].mAfter;
            if (i + 1 < aEnd) {
                space += aSpacing[i + 1 - aStart].mBefore;
            }
            x += direction*space;
        }
    }

    if (allGlyphsInvisible) {
        metrics.mBoundingBox.SetEmpty();
    } else {
        if (aBoundingBoxType == LOOSE_INK_EXTENTS) {
            UnionRange(x, &advanceMin, &advanceMax);
            gfxRect fontBox(advanceMin, -metrics.mAscent,
                            advanceMax - advanceMin, metrics.mAscent + metrics.mDescent);
            metrics.mBoundingBox = metrics.mBoundingBox.Union(fontBox);
        }
        if (isRTL) {
            metrics.mBoundingBox -= gfxPoint(x, 0);
        }
    }

    // If the font may be rendered with a fake-italic effect, we need to allow
    // for the top-right of the glyphs being skewed to the right, and the
    // bottom-left being skewed further left.
    if (mStyle.style != NS_FONT_STYLE_NORMAL &&
        mFontEntry->IsUpright() &&
        mStyle.allowSyntheticStyle) {
        gfxFloat extendLeftEdge =
            ceil(OBLIQUE_SKEW_FACTOR * metrics.mBoundingBox.YMost());
        gfxFloat extendRightEdge =
            ceil(OBLIQUE_SKEW_FACTOR * -metrics.mBoundingBox.y);
        metrics.mBoundingBox.width += extendLeftEdge + extendRightEdge;
        metrics.mBoundingBox.x -= extendLeftEdge;
    }

    if (baselineOffset != 0) {
        metrics.mAscent -= baselineOffset;
        metrics.mDescent += baselineOffset;
        metrics.mBoundingBox.y += baselineOffset;
    }

    metrics.mAdvanceWidth = x*direction;
    return metrics;
}

void
gfxFont::AgeCachedWords()
{
    if (mWordCache) {
        for (auto it = mWordCache->Iter(); !it.Done(); it.Next()) {
            CacheHashEntry *entry = it.Get();
            if (!entry->mShapedWord) {
                NS_ASSERTION(entry->mShapedWord,
                             "cache entry has no gfxShapedWord!");
                it.Remove();
            } else if (entry->mShapedWord->IncrementAge() ==
                       kShapedWordCacheMaxAge) {
                it.Remove();
            }
        }
    }
}

void
gfxFont::NotifyGlyphsChanged()
{
    uint32_t i, count = mGlyphExtentsArray.Length();
    for (i = 0; i < count; ++i) {
        // Flush cached extents array
        mGlyphExtentsArray[i]->NotifyGlyphsChanged();
    }

    if (mGlyphChangeObservers) {
        for (auto it = mGlyphChangeObservers->Iter(); !it.Done(); it.Next()) {
            it.Get()->GetKey()->NotifyGlyphsChanged();
        }
    }
}

// If aChar is a "word boundary" for shaped-word caching purposes, return it;
// else return 0.
static char16_t
IsBoundarySpace(char16_t aChar, char16_t aNextChar)
{
    if ((aChar == ' ' || aChar == 0x00A0) && !IsClusterExtender(aNextChar)) {
        return aChar;
    }
    return 0;
}

#ifdef __GNUC__
#define GFX_MAYBE_UNUSED __attribute__((unused))
#else
#define GFX_MAYBE_UNUSED
#endif

template<typename T>
gfxShapedWord*
gfxFont::GetShapedWord(DrawTarget *aDrawTarget,
                       const T    *aText,
                       uint32_t    aLength,
                       uint32_t    aHash,
                       Script      aRunScript,
                       bool        aVertical,
                       int32_t     aAppUnitsPerDevUnit,
                       uint32_t    aFlags,
                       gfxTextPerfMetrics *aTextPerf GFX_MAYBE_UNUSED)
{
    // if the cache is getting too big, flush it and start over
    uint32_t wordCacheMaxEntries =
        gfxPlatform::GetPlatform()->WordCacheMaxEntries();
    if (mWordCache->Count() > wordCacheMaxEntries) {
        NS_WARNING("flushing shaped-word cache");
        ClearCachedWords();
    }

    // if there's a cached entry for this word, just return it
    CacheHashKey key(aText, aLength, aHash,
                     aRunScript,
                     aAppUnitsPerDevUnit,
                     aFlags);

    CacheHashEntry *entry = mWordCache->PutEntry(key);
    if (!entry) {
        NS_WARNING("failed to create word cache entry - expect missing text");
        return nullptr;
    }
    gfxShapedWord* sw = entry->mShapedWord.get();

    bool isContent = !mStyle.systemFont;

    if (sw) {
        sw->ResetAge();
        Telemetry::Accumulate((isContent ? Telemetry::WORD_CACHE_HITS_CONTENT :
                                   Telemetry::WORD_CACHE_HITS_CHROME),
                              aLength);
#ifndef RELEASE_OR_BETA
        if (aTextPerf) {
            aTextPerf->current.wordCacheHit++;
        }
#endif
        return sw;
    }

    Telemetry::Accumulate((isContent ? Telemetry::WORD_CACHE_MISSES_CONTENT :
                               Telemetry::WORD_CACHE_MISSES_CHROME),
                          aLength);
#ifndef RELEASE_OR_BETA
    if (aTextPerf) {
        aTextPerf->current.wordCacheMiss++;
    }
#endif

    sw = gfxShapedWord::Create(aText, aLength, aRunScript, aAppUnitsPerDevUnit,
                               aFlags);
    entry->mShapedWord.reset(sw);
    if (!sw) {
        NS_WARNING("failed to create gfxShapedWord - expect missing text");
        return nullptr;
    }

    DebugOnly<bool> ok =
        ShapeText(aDrawTarget, aText, 0, aLength, aRunScript, aVertical, sw);

    NS_WARNING_ASSERTION(ok, "failed to shape word - expect garbled text");

    return sw;
}

bool
gfxFont::CacheHashEntry::KeyEquals(const KeyTypePointer aKey) const
{
    const gfxShapedWord* sw = mShapedWord.get();
    if (!sw) {
        return false;
    }
    if (sw->GetLength() != aKey->mLength ||
        sw->GetFlags() != aKey->mFlags ||
        sw->GetAppUnitsPerDevUnit() != aKey->mAppUnitsPerDevUnit ||
        sw->GetScript() != aKey->mScript) {
        return false;
    }
    if (sw->TextIs8Bit()) {
        if (aKey->mTextIs8Bit) {
            return (0 == memcmp(sw->Text8Bit(), aKey->mText.mSingle,
                                aKey->mLength * sizeof(uint8_t)));
        }
        // The key has 16-bit text, even though all the characters are < 256,
        // so the TEXT_IS_8BIT flag was set and the cached ShapedWord we're
        // comparing with will have 8-bit text.
        const uint8_t   *s1 = sw->Text8Bit();
        const char16_t *s2 = aKey->mText.mDouble;
        const char16_t *s2end = s2 + aKey->mLength;
        while (s2 < s2end) {
            if (*s1++ != *s2++) {
                return false;
            }
        }
        return true;
    }
    NS_ASSERTION((aKey->mFlags & gfxTextRunFactory::TEXT_IS_8BIT) == 0 &&
                 !aKey->mTextIs8Bit, "didn't expect 8-bit text here");
    return (0 == memcmp(sw->TextUnicode(), aKey->mText.mDouble,
                        aKey->mLength * sizeof(char16_t)));
}

bool
gfxFont::ShapeText(DrawTarget    *aDrawTarget,
                   const uint8_t *aText,
                   uint32_t       aOffset,
                   uint32_t       aLength,
                   Script         aScript,
                   bool           aVertical,
                   gfxShapedText *aShapedText)
{
    nsDependentCSubstring ascii((const char*)aText, aLength);
    nsAutoString utf16;
    AppendASCIItoUTF16(ascii, utf16);
    if (utf16.Length() != aLength) {
        return false;
    }
    return ShapeText(aDrawTarget, utf16.BeginReading(), aOffset, aLength,
                     aScript, aVertical, aShapedText);
}

bool
gfxFont::ShapeText(DrawTarget      *aDrawTarget,
                   const char16_t *aText,
                   uint32_t         aOffset,
                   uint32_t         aLength,
                   Script           aScript,
                   bool             aVertical,
                   gfxShapedText   *aShapedText)
{
    bool ok = false;

    // XXX Currently, we do all vertical shaping through harfbuzz.
    // Vertical graphite support may be wanted as a future enhancement.
    if (FontCanSupportGraphite() && !aVertical) {
        if (gfxPlatform::GetPlatform()->UseGraphiteShaping()) {
            if (!mGraphiteShaper) {
                mGraphiteShaper = MakeUnique<gfxGraphiteShaper>(this);
            }
            ok = mGraphiteShaper->ShapeText(aDrawTarget, aText, aOffset, aLength,
                                            aScript, aVertical, aShapedText);
        }
    }

    if (!ok) {
        if (!mHarfBuzzShaper) {
            mHarfBuzzShaper = MakeUnique<gfxHarfBuzzShaper>(this);
        }
        ok = mHarfBuzzShaper->ShapeText(aDrawTarget, aText, aOffset, aLength,
                                        aScript, aVertical, aShapedText);
    }

    NS_WARNING_ASSERTION(ok, "shaper failed, expect scrambled or missing text");

    PostShapingFixup(aDrawTarget, aText, aOffset, aLength,
                     aVertical, aShapedText);

    return ok;
}

void
gfxFont::PostShapingFixup(DrawTarget*     aDrawTarget,
                          const char16_t* aText,
                          uint32_t        aOffset,
                          uint32_t        aLength,
                          bool            aVertical,
                          gfxShapedText*  aShapedText)
{
    if (IsSyntheticBold()) {
        const Metrics& metrics =
            GetMetrics(aVertical ? eVertical : eHorizontal);
        if (metrics.maxAdvance > metrics.aveCharWidth) {
            float synBoldOffset =
                    GetSyntheticBoldOffset() * CalcXScale(aDrawTarget);
            aShapedText->AdjustAdvancesForSyntheticBold(synBoldOffset,
                                                        aOffset, aLength);
        }
    }
}

#define MAX_SHAPING_LENGTH  32760 // slightly less than 32K, trying to avoid
                                  // over-stressing platform shapers
#define BACKTRACK_LIMIT     16 // backtrack this far looking for a good place
                               // to split into fragments for separate shaping

template<typename T>
bool
gfxFont::ShapeFragmentWithoutWordCache(DrawTarget *aDrawTarget,
                                       const T    *aText,
                                       uint32_t    aOffset,
                                       uint32_t    aLength,
                                       Script      aScript,
                                       bool        aVertical,
                                       gfxTextRun *aTextRun)
{
    aTextRun->SetupClusterBoundaries(aOffset, aText, aLength);

    bool ok = true;

    while (ok && aLength > 0) {
        uint32_t fragLen = aLength;

        // limit the length of text we pass to shapers in a single call
        if (fragLen > MAX_SHAPING_LENGTH) {
            fragLen = MAX_SHAPING_LENGTH;

            // in the 8-bit case, there are no multi-char clusters,
            // so we don't need to do this check
            if (sizeof(T) == sizeof(char16_t)) {
                uint32_t i;
                for (i = 0; i < BACKTRACK_LIMIT; ++i) {
                    if (aTextRun->IsClusterStart(aOffset + fragLen - i)) {
                        fragLen -= i;
                        break;
                    }
                }
                if (i == BACKTRACK_LIMIT) {
                    // if we didn't find any cluster start while backtracking,
                    // just check that we're not in the middle of a surrogate
                    // pair; back up by one code unit if we are.
                    if (NS_IS_LOW_SURROGATE(aText[fragLen]) &&
                        NS_IS_HIGH_SURROGATE(aText[fragLen - 1])) {
                        --fragLen;
                    }
                }
            }
        }

        ok = ShapeText(aDrawTarget, aText, aOffset, fragLen, aScript, aVertical,
                       aTextRun);

        aText += fragLen;
        aOffset += fragLen;
        aLength -= fragLen;
    }

    return ok;
}

// Check if aCh is an unhandled control character that should be displayed
// as a hexbox rather than rendered by some random font on the system.
// We exclude \r as stray &#13;s are rather common (bug 941940).
// Note that \n and \t don't come through here, as they have specific
// meanings that have already been handled.
static bool
IsInvalidControlChar(uint32_t aCh)
{
    return aCh != '\r' && ((aCh & 0x7f) < 0x20 || aCh == 0x7f);
}

template<typename T>
bool
gfxFont::ShapeTextWithoutWordCache(DrawTarget *aDrawTarget,
                                   const T    *aText,
                                   uint32_t    aOffset,
                                   uint32_t    aLength,
                                   Script      aScript,
                                   bool        aVertical,
                                   gfxTextRun *aTextRun)
{
    uint32_t fragStart = 0;
    bool ok = true;

    for (uint32_t i = 0; i <= aLength && ok; ++i) {
        T ch = (i < aLength) ? aText[i] : '\n';
        bool invalid = gfxFontGroup::IsInvalidChar(ch);
        uint32_t length = i - fragStart;

        // break into separate fragments when we hit an invalid char
        if (!invalid) {
            continue;
        }

        if (length > 0) {
            ok = ShapeFragmentWithoutWordCache(aDrawTarget, aText + fragStart,
                                               aOffset + fragStart, length,
                                               aScript, aVertical, aTextRun);
        }

        if (i == aLength) {
            break;
        }

        // fragment was terminated by an invalid char: skip it,
        // unless it's a control char that we want to show as a hexbox,
        // but record where TAB or NEWLINE occur
        if (ch == '\t') {
            aTextRun->SetIsTab(aOffset + i);
        } else if (ch == '\n') {
            aTextRun->SetIsNewline(aOffset + i);
        } else if (IsInvalidControlChar(ch) &&
            !(aTextRun->GetFlags() & gfxTextRunFactory::TEXT_HIDE_CONTROL_CHARACTERS)) {
            if (GetFontEntry()->IsUserFont() && HasCharacter(ch)) {
                ShapeFragmentWithoutWordCache(aDrawTarget, aText + i,
                                              aOffset + i, 1,
                                              aScript, aVertical, aTextRun);
            } else {
                aTextRun->SetMissingGlyph(aOffset + i, ch, this);
            }
        }
        fragStart = i + 1;
    }

    NS_WARNING_ASSERTION(ok, "failed to shape text - expect garbled text");
    return ok;
}

#ifndef RELEASE_OR_BETA
#define TEXT_PERF_INCR(tp, m) (tp ? (tp)->current.m++ : 0)
#else
#define TEXT_PERF_INCR(tp, m)
#endif

inline static bool IsChar8Bit(uint8_t /*aCh*/) { return true; }
inline static bool IsChar8Bit(char16_t aCh) { return aCh < 0x100; }

inline static bool HasSpaces(const uint8_t *aString, uint32_t aLen)
{
    return memchr(aString, 0x20, aLen) != nullptr;
}

inline static bool HasSpaces(const char16_t *aString, uint32_t aLen)
{
    for (const char16_t *ch = aString; ch < aString + aLen; ch++) {
        if (*ch == 0x20) {
            return true;
        }
    }
    return false;
}

template<typename T>
bool
gfxFont::SplitAndInitTextRun(DrawTarget *aDrawTarget,
                             gfxTextRun *aTextRun,
                             const T *aString, // text for this font run
                             uint32_t aRunStart, // position in the textrun
                             uint32_t aRunLength,
                             Script aRunScript,
                             bool aVertical)
{
    if (aRunLength == 0) {
        return true;
    }

    gfxTextPerfMetrics *tp = nullptr;

#ifndef RELEASE_OR_BETA
    tp = aTextRun->GetFontGroup()->GetTextPerfMetrics();
    if (tp) {
        if (mStyle.systemFont) {
            tp->current.numChromeTextRuns++;
        } else {
            tp->current.numContentTextRuns++;
        }
        tp->current.numChars += aRunLength;
        if (aRunLength > tp->current.maxTextRunLen) {
            tp->current.maxTextRunLen = aRunLength;
        }
    }
#endif

    uint32_t wordCacheCharLimit =
        gfxPlatform::GetPlatform()->WordCacheCharLimit();

    // If spaces can participate in shaping (e.g. within lookups for automatic
    // fractions), need to shape without using the word cache which segments
    // textruns on space boundaries. Word cache can be used if the textrun
    // is short enough to fit in the word cache and it lacks spaces.
    if (SpaceMayParticipateInShaping(aRunScript)) {
        if (aRunLength > wordCacheCharLimit ||
            HasSpaces(aString, aRunLength)) {
            TEXT_PERF_INCR(tp, wordCacheSpaceRules);
            return ShapeTextWithoutWordCache(aDrawTarget, aString,
                                             aRunStart, aRunLength,
                                             aRunScript, aVertical,
                                             aTextRun);
        }
    }

    InitWordCache();

    // the only flags we care about for ShapedWord construction/caching
    uint32_t flags = aTextRun->GetFlags();
    flags &= (gfxTextRunFactory::TEXT_IS_RTL |
              gfxTextRunFactory::TEXT_DISABLE_OPTIONAL_LIGATURES |
              gfxTextRunFactory::TEXT_USE_MATH_SCRIPT |
              gfxTextRunFactory::TEXT_ORIENT_MASK);
    if (sizeof(T) == sizeof(uint8_t)) {
        flags |= gfxTextRunFactory::TEXT_IS_8BIT;
    }

    uint32_t wordStart = 0;
    uint32_t hash = 0;
    bool wordIs8Bit = true;
    int32_t appUnitsPerDevUnit = aTextRun->GetAppUnitsPerDevUnit();

    T nextCh = aString[0];
    for (uint32_t i = 0; i <= aRunLength; ++i) {
        T ch = nextCh;
        nextCh = (i < aRunLength - 1) ? aString[i + 1] : '\n';
        T boundary = IsBoundarySpace(ch, nextCh);
        bool invalid = !boundary && gfxFontGroup::IsInvalidChar(ch);
        uint32_t length = i - wordStart;

        // break into separate ShapedWords when we hit an invalid char,
        // or a boundary space (always handled individually),
        // or the first non-space after a space
        if (!boundary && !invalid) {
            if (!IsChar8Bit(ch)) {
                wordIs8Bit = false;
            }
            // include this character in the hash, and move on to next
            hash = gfxShapedWord::HashMix(hash, ch);
            continue;
        }

        // We've decided to break here (i.e. we're at the end of a "word");
        // shape the word and add it to the textrun.
        // For words longer than the limit, we don't use the
        // font's word cache but just shape directly into the textrun.
        if (length > wordCacheCharLimit) {
            TEXT_PERF_INCR(tp, wordCacheLong);
            bool ok = ShapeFragmentWithoutWordCache(aDrawTarget,
                                                    aString + wordStart,
                                                    aRunStart + wordStart,
                                                    length,
                                                    aRunScript,
                                                    aVertical,
                                                    aTextRun);
            if (!ok) {
                return false;
            }
        } else if (length > 0) {
            uint32_t wordFlags = flags;
            // in the 8-bit version of this method, TEXT_IS_8BIT was
            // already set as part of |flags|, so no need for a per-word
            // adjustment here
            if (sizeof(T) == sizeof(char16_t)) {
                if (wordIs8Bit) {
                    wordFlags |= gfxTextRunFactory::TEXT_IS_8BIT;
                }
            }
            gfxShapedWord* sw = GetShapedWord(aDrawTarget,
                                              aString + wordStart, length,
                                              hash, aRunScript, aVertical,
                                              appUnitsPerDevUnit,
                                              wordFlags, tp);
            if (sw) {
                aTextRun->CopyGlyphDataFrom(sw, aRunStart + wordStart);
            } else {
                return false; // failed, presumably out of memory?
            }
        }

        if (boundary) {
            // word was terminated by a space: add that to the textrun
            uint16_t orientation = flags & gfxTextRunFactory::TEXT_ORIENT_MASK;
            if (orientation == gfxTextRunFactory::TEXT_ORIENT_VERTICAL_MIXED) {
                orientation = aVertical ?
                    gfxTextRunFactory::TEXT_ORIENT_VERTICAL_UPRIGHT :
                    gfxTextRunFactory::TEXT_ORIENT_VERTICAL_SIDEWAYS_RIGHT;
            }
            if (boundary != ' ' ||
                !aTextRun->SetSpaceGlyphIfSimple(this, aRunStart + i, ch,
                                                 orientation)) {
                // Currently, the only "boundary" characters we recognize are
                // space and no-break space, which are both 8-bit, so we force
                // that flag (below). If we ever change IsBoundarySpace, we
                // may need to revise this.
                // Avoid tautological-constant-out-of-range-compare in 8-bit:
                DebugOnly<char16_t> boundary16 = boundary;
                NS_ASSERTION(boundary16 < 256, "unexpected boundary!");
                gfxShapedWord *sw =
                    GetShapedWord(aDrawTarget, &boundary, 1,
                                  gfxShapedWord::HashMix(0, boundary),
                                  aRunScript, aVertical, appUnitsPerDevUnit,
                                  flags | gfxTextRunFactory::TEXT_IS_8BIT, tp);
                if (sw) {
                    aTextRun->CopyGlyphDataFrom(sw, aRunStart + i);
                } else {
                    return false;
                }
            }
            hash = 0;
            wordStart = i + 1;
            wordIs8Bit = true;
            continue;
        }

        if (i == aRunLength) {
            break;
        }

        NS_ASSERTION(invalid,
                     "how did we get here except via an invalid char?");

        // word was terminated by an invalid char: skip it,
        // unless it's a control char that we want to show as a hexbox,
        // but record where TAB or NEWLINE occur
        if (ch == '\t') {
            aTextRun->SetIsTab(aRunStart + i);
        } else if (ch == '\n') {
            aTextRun->SetIsNewline(aRunStart + i);
        } else if (IsInvalidControlChar(ch) &&
            !(aTextRun->GetFlags() & gfxTextRunFactory::TEXT_HIDE_CONTROL_CHARACTERS)) {
            if (GetFontEntry()->IsUserFont() && HasCharacter(ch)) {
                ShapeFragmentWithoutWordCache(aDrawTarget, aString + i,
                                              aRunStart + i, 1,
                                              aRunScript, aVertical, aTextRun);
            } else {
                aTextRun->SetMissingGlyph(aRunStart + i, ch, this);
            }
        }

        hash = 0;
        wordStart = i + 1;
        wordIs8Bit = true;
    }

    return true;
}

// Explicit instantiations of SplitAndInitTextRun, to avoid libxul link failure
template bool
gfxFont::SplitAndInitTextRun(DrawTarget *aDrawTarget,
                             gfxTextRun *aTextRun,
                             const uint8_t *aString,
                             uint32_t aRunStart,
                             uint32_t aRunLength,
                             Script aRunScript,
                             bool aVertical);
template bool
gfxFont::SplitAndInitTextRun(DrawTarget *aDrawTarget,
                             gfxTextRun *aTextRun,
                             const char16_t *aString,
                             uint32_t aRunStart,
                             uint32_t aRunLength,
                             Script aRunScript,
                             bool aVertical);

template<>
bool
gfxFont::InitFakeSmallCapsRun(DrawTarget     *aDrawTarget,
                              gfxTextRun     *aTextRun,
                              const char16_t *aText,
                              uint32_t        aOffset,
                              uint32_t        aLength,
                              uint8_t         aMatchType,
                              uint16_t        aOrientation,
                              Script          aScript,
                              bool            aSyntheticLower,
                              bool            aSyntheticUpper)
{
    bool ok = true;

    RefPtr<gfxFont> smallCapsFont = GetSmallCapsFont();
    if (!smallCapsFont) {
        NS_WARNING("failed to get reduced-size font for smallcaps!");
        smallCapsFont = this;
    }

    enum RunCaseAction {
        kNoChange,
        kUppercaseReduce,
        kUppercase
    };

    RunCaseAction runAction = kNoChange;
    uint32_t runStart = 0;
    bool vertical =
        aOrientation == gfxTextRunFactory::TEXT_ORIENT_VERTICAL_UPRIGHT;

    for (uint32_t i = 0; i <= aLength; ++i) {
        uint32_t extraCodeUnits = 0; // Will be set to 1 if we need to consume
                                     // a trailing surrogate as well as the
                                     // current code unit.
        RunCaseAction chAction = kNoChange;
        // Unless we're at the end, figure out what treatment the current
        // character will need.
        if (i < aLength) {
            uint32_t ch = aText[i];
            if (NS_IS_HIGH_SURROGATE(ch) && i < aLength - 1 &&
                NS_IS_LOW_SURROGATE(aText[i + 1])) {
                ch = SURROGATE_TO_UCS4(ch, aText[i + 1]);
                extraCodeUnits = 1;
            }
            // Characters that aren't the start of a cluster are ignored here.
            // They get added to whatever lowercase/non-lowercase run we're in.
            if (IsClusterExtender(ch)) {
                chAction = runAction;
            } else {
                if (ch != ToUpperCase(ch) || SpecialUpper(ch)) {
                    // ch is lower case
                    chAction = (aSyntheticLower ? kUppercaseReduce : kNoChange);
                } else if (ch != ToLowerCase(ch)) {
                    // ch is upper case
                    chAction = (aSyntheticUpper ? kUppercaseReduce : kNoChange);
                    if (mStyle.explicitLanguage &&
                        mStyle.language == nsGkAtoms::el) {
                        // In Greek, check for characters that will be modified by
                        // the GreekUpperCase mapping - this catches accented
                        // capitals where the accent is to be removed (bug 307039).
                        // These are handled by using the full-size font with the
                        // uppercasing transform.
                        mozilla::GreekCasing::State state;
                        bool markEta, updateEta;
                        uint32_t ch2 =
                            mozilla::GreekCasing::UpperCase(ch, state, markEta,
                                                            updateEta);
                        if ((ch != ch2 || markEta) && !aSyntheticUpper) {
                            chAction = kUppercase;
                        }
                    }
                }
            }
        }

        // At the end of the text or when the current character needs different
        // casing treatment from the current run, finish the run-in-progress
        // and prepare to accumulate a new run.
        // Note that we do not look at any source data for offset [i] here,
        // as that would be invalid in the case where i==length.
        if ((i == aLength || runAction != chAction) && runStart < i) {
            uint32_t runLength = i - runStart;
            gfxFont* f = this;
            switch (runAction) {
            case kNoChange:
                // just use the current font and the existing string
                aTextRun->AddGlyphRun(f, aMatchType, aOffset + runStart, true,
                                      aOrientation);
                if (!f->SplitAndInitTextRun(aDrawTarget, aTextRun,
                                            aText + runStart,
                                            aOffset + runStart, runLength,
                                            aScript, vertical)) {
                    ok = false;
                }
                break;

            case kUppercaseReduce:
                // use reduced-size font, then fall through to uppercase the text
                f = smallCapsFont;
                MOZ_FALLTHROUGH;

            case kUppercase:
                // apply uppercase transform to the string
                nsDependentSubstring origString(aText + runStart, runLength);
                nsAutoString convertedString;
                AutoTArray<bool,50> charsToMergeArray;
                AutoTArray<bool,50> deletedCharsArray;

                bool mergeNeeded = nsCaseTransformTextRunFactory::
                    TransformString(origString,
                                    convertedString,
                                    true,
                                    mStyle.explicitLanguage
                                      ? mStyle.language.get() : nullptr,
                                    charsToMergeArray,
                                    deletedCharsArray);

                if (mergeNeeded) {
                    // This is the hard case: the transformation caused chars
                    // to be inserted or deleted, so we can't shape directly
                    // into the destination textrun but have to handle the
                    // mismatch of character positions.
                    gfxTextRunFactory::Parameters params = {
                        aDrawTarget, nullptr, nullptr, nullptr, 0,
                        aTextRun->GetAppUnitsPerDevUnit()
                    };
                    RefPtr<gfxTextRun> tempRun(
                        gfxTextRun::Create(&params, convertedString.Length(),
                                           aTextRun->GetFontGroup(), 0));
                    tempRun->AddGlyphRun(f, aMatchType, 0, true, aOrientation);
                    if (!f->SplitAndInitTextRun(aDrawTarget, tempRun.get(),
                                                convertedString.BeginReading(),
                                                0, convertedString.Length(),
                                                aScript, vertical)) {
                        ok = false;
                    } else {
                        RefPtr<gfxTextRun> mergedRun(
                            gfxTextRun::Create(&params, runLength,
                                               aTextRun->GetFontGroup(), 0));
                        MergeCharactersInTextRun(mergedRun.get(), tempRun.get(),
                                                 charsToMergeArray.Elements(),
                                                 deletedCharsArray.Elements());
                        gfxTextRun::Range runRange(0, runLength);
                        aTextRun->CopyGlyphDataFrom(mergedRun.get(), runRange,
                                                    aOffset + runStart);
                    }
                } else {
                    aTextRun->AddGlyphRun(f, aMatchType, aOffset + runStart,
                                          true, aOrientation);
                    if (!f->SplitAndInitTextRun(aDrawTarget, aTextRun,
                                                convertedString.BeginReading(),
                                                aOffset + runStart, runLength,
                                                aScript, vertical)) {
                        ok = false;
                    }
                }
                break;
            }

            runStart = i;
        }

        i += extraCodeUnits;
        if (i < aLength) {
            runAction = chAction;
        }
    }

    return ok;
}

template<>
bool
gfxFont::InitFakeSmallCapsRun(DrawTarget     *aDrawTarget,
                              gfxTextRun     *aTextRun,
                              const uint8_t  *aText,
                              uint32_t        aOffset,
                              uint32_t        aLength,
                              uint8_t         aMatchType,
                              uint16_t        aOrientation,
                              Script          aScript,
                              bool            aSyntheticLower,
                              bool            aSyntheticUpper)
{
    NS_ConvertASCIItoUTF16 unicodeString(reinterpret_cast<const char*>(aText),
                                         aLength);
    return InitFakeSmallCapsRun(aDrawTarget, aTextRun, static_cast<const char16_t*>(unicodeString.get()),
                                aOffset, aLength, aMatchType, aOrientation,
                                aScript, aSyntheticLower, aSyntheticUpper);
}

already_AddRefed<gfxFont>
gfxFont::GetSmallCapsFont()
{
    gfxFontStyle style(*GetStyle());
    style.size *= SMALL_CAPS_SCALE_FACTOR;
    style.variantCaps = NS_FONT_VARIANT_CAPS_NORMAL;
    gfxFontEntry* fe = GetFontEntry();
    bool needsBold = style.weight >= 600 && !fe->IsBold();
    return fe->FindOrMakeFont(&style, needsBold, mUnicodeRangeMap);
}

already_AddRefed<gfxFont>
gfxFont::GetSubSuperscriptFont(int32_t aAppUnitsPerDevPixel)
{
    gfxFontStyle style(*GetStyle());
    style.AdjustForSubSuperscript(aAppUnitsPerDevPixel);
    gfxFontEntry* fe = GetFontEntry();
    bool needsBold = style.weight >= 600 && !fe->IsBold();
    return fe->FindOrMakeFont(&style, needsBold, mUnicodeRangeMap);
}

static void
DestroyRefCairo(void* aData)
{
  cairo_t* refCairo = static_cast<cairo_t*>(aData);
  MOZ_ASSERT(refCairo);
  cairo_destroy(refCairo);
}

/* static */ cairo_t *
gfxFont::RefCairo(DrawTarget* aDT)
{
  // DrawTargets that don't use a Cairo backend can be given a 1x1 "reference"
  // |cairo_t*|, stored in the DrawTarget's user data, for doing font-related
  // operations.
  static UserDataKey sRefCairo;

  cairo_t* refCairo = nullptr;
  if (aDT->GetBackendType() == BackendType::CAIRO) {
    refCairo = static_cast<cairo_t*>
      (aDT->GetNativeSurface(NativeSurfaceType::CAIRO_CONTEXT));
    if (refCairo) {
      return refCairo;
    }
  }

  refCairo = static_cast<cairo_t*>(aDT->GetUserData(&sRefCairo));
  if (!refCairo) {
    refCairo = cairo_create(gfxPlatform::GetPlatform()->ScreenReferenceSurface()->CairoSurface());
    aDT->AddUserData(&sRefCairo, refCairo, DestroyRefCairo);
  }

  return refCairo;
}

gfxGlyphExtents *
gfxFont::GetOrCreateGlyphExtents(int32_t aAppUnitsPerDevUnit) {
    uint32_t i, count = mGlyphExtentsArray.Length();
    for (i = 0; i < count; ++i) {
        if (mGlyphExtentsArray[i]->GetAppUnitsPerDevUnit() == aAppUnitsPerDevUnit)
            return mGlyphExtentsArray[i].get();
    }
    gfxGlyphExtents *glyphExtents = new gfxGlyphExtents(aAppUnitsPerDevUnit);
    if (glyphExtents) {
        mGlyphExtentsArray.AppendElement(glyphExtents);
        // Initialize the extents of a space glyph, assuming that spaces don't
        // render anything!
        glyphExtents->SetContainedGlyphWidthAppUnits(GetSpaceGlyph(), 0);
    }
    return glyphExtents;
}

void
gfxFont::SetupGlyphExtents(DrawTarget* aDrawTarget, uint32_t aGlyphID,
                           bool aNeedTight, gfxGlyphExtents *aExtents)
{
    gfxRect svgBounds;
    if (mFontEntry->TryGetSVGData(this) && mFontEntry->HasSVGGlyph(aGlyphID) &&
        mFontEntry->GetSVGGlyphExtents(aDrawTarget, aGlyphID, &svgBounds)) {
        gfxFloat d2a = aExtents->GetAppUnitsPerDevUnit();
        aExtents->SetTightGlyphExtents(aGlyphID,
                                       gfxRect(svgBounds.x * d2a,
                                               svgBounds.y * d2a,
                                               svgBounds.width * d2a,
                                               svgBounds.height * d2a));
        return;
    }

    cairo_glyph_t glyph;
    glyph.index = aGlyphID;
    glyph.x = 0;
    glyph.y = 0;
    cairo_text_extents_t extents;
    cairo_glyph_extents(gfxFont::RefCairo(aDrawTarget), &glyph, 1, &extents);

    const Metrics& fontMetrics = GetMetrics(eHorizontal);
    int32_t appUnitsPerDevUnit = aExtents->GetAppUnitsPerDevUnit();
    if (!aNeedTight && extents.x_bearing >= 0 &&
        extents.y_bearing >= -fontMetrics.maxAscent &&
        extents.height + extents.y_bearing <= fontMetrics.maxDescent) {
        uint32_t appUnitsWidth =
            uint32_t(ceil((extents.x_bearing + extents.width)*appUnitsPerDevUnit));
        if (appUnitsWidth < gfxGlyphExtents::INVALID_WIDTH) {
            aExtents->SetContainedGlyphWidthAppUnits(aGlyphID, uint16_t(appUnitsWidth));
            return;
        }
    }
#ifdef DEBUG_TEXT_RUN_STORAGE_METRICS
    if (!aNeedTight) {
        ++gGlyphExtentsSetupFallBackToTight;
    }
#endif

    gfxFloat d2a = appUnitsPerDevUnit;
    gfxRect bounds(extents.x_bearing*d2a, extents.y_bearing*d2a,
                   extents.width*d2a, extents.height*d2a);
    aExtents->SetTightGlyphExtents(aGlyphID, bounds);
}

// Try to initialize font metrics by reading sfnt tables directly;
// set mIsValid=TRUE and return TRUE on success.
// Return FALSE if the gfxFontEntry subclass does not
// implement GetFontTable(), or for non-sfnt fonts where tables are
// not available.
// If this returns TRUE without setting the mIsValid flag, then we -did-
// apparently find an sfnt, but it was too broken to be used.
bool
gfxFont::InitMetricsFromSfntTables(Metrics& aMetrics)
{
    mIsValid = false; // font is NOT valid in case of early return

    const uint32_t kHheaTableTag = TRUETYPE_TAG('h','h','e','a');
    const uint32_t kPostTableTag = TRUETYPE_TAG('p','o','s','t');
    const uint32_t kOS_2TableTag = TRUETYPE_TAG('O','S','/','2');

    uint32_t len;

    if (mFUnitsConvFactor < 0.0) {
        // If the conversion factor from FUnits is not yet set,
        // get the unitsPerEm from the 'head' table via the font entry
        uint16_t unitsPerEm = GetFontEntry()->UnitsPerEm();
        if (unitsPerEm == gfxFontEntry::kInvalidUPEM) {
            return false;
        }
        mFUnitsConvFactor = GetAdjustedSize() / unitsPerEm;
    }

    // 'hhea' table is required to get vertical extents
    gfxFontEntry::AutoTable hheaTable(mFontEntry, kHheaTableTag);
    if (!hheaTable) {
        return false; // no 'hhea' table -> not an sfnt
    }
    const MetricsHeader* hhea =
        reinterpret_cast<const MetricsHeader*>
            (hb_blob_get_data(hheaTable, &len));
    if (len < sizeof(MetricsHeader)) {
        return false;
    }

#define SET_UNSIGNED(field,src) aMetrics.field = uint16_t(src) * mFUnitsConvFactor
#define SET_SIGNED(field,src)   aMetrics.field = int16_t(src) * mFUnitsConvFactor

    SET_UNSIGNED(maxAdvance, hhea->advanceWidthMax);
    SET_SIGNED(maxAscent, hhea->ascender);
    SET_SIGNED(maxDescent, -int16_t(hhea->descender));
    SET_SIGNED(externalLeading, hhea->lineGap);

    // 'post' table is required for underline metrics
    gfxFontEntry::AutoTable postTable(mFontEntry, kPostTableTag);
    if (!postTable) {
        return true; // no 'post' table -> sfnt is not valid
    }
    const PostTable *post =
        reinterpret_cast<const PostTable*>(hb_blob_get_data(postTable, &len));
    if (len < offsetof(PostTable, underlineThickness) + sizeof(uint16_t)) {
        return true; // bad post table -> sfnt is not valid
    }

    SET_SIGNED(underlineOffset, post->underlinePosition);
    SET_UNSIGNED(underlineSize, post->underlineThickness);

    // 'OS/2' table is optional, if not found we'll estimate xHeight
    // and aveCharWidth by measuring glyphs
    gfxFontEntry::AutoTable os2Table(mFontEntry, kOS_2TableTag);
    if (os2Table) {
        const OS2Table *os2 =
            reinterpret_cast<const OS2Table*>(hb_blob_get_data(os2Table, &len));
        // although sxHeight and sCapHeight are signed fields, we consider
        // negative values to be erroneous and just ignore them
        if (uint16_t(os2->version) >= 2) {
            // version 2 and later includes the x-height and cap-height fields
            if (len >= offsetof(OS2Table, sxHeight) + sizeof(int16_t) &&
                int16_t(os2->sxHeight) > 0) {
                SET_SIGNED(xHeight, os2->sxHeight);
            }
            if (len >= offsetof(OS2Table, sCapHeight) + sizeof(int16_t) &&
                int16_t(os2->sCapHeight) > 0) {
                SET_SIGNED(capHeight, os2->sCapHeight);
            }
        }
        // this should always be present in any valid OS/2 of any version
        if (len >= offsetof(OS2Table, sTypoLineGap) + sizeof(int16_t)) {
            SET_SIGNED(aveCharWidth, os2->xAvgCharWidth);
            SET_SIGNED(strikeoutSize, os2->yStrikeoutSize);
            SET_SIGNED(strikeoutOffset, os2->yStrikeoutPosition);

            // for fonts with USE_TYPO_METRICS set in the fsSelection field,
            // let the OS/2 sTypo* metrics override those from the hhea table
            // (see http://www.microsoft.com/typography/otspec/os2.htm#fss)
            const uint16_t kUseTypoMetricsMask = 1 << 7;
            if (uint16_t(os2->fsSelection) & kUseTypoMetricsMask) {
                SET_SIGNED(maxAscent, os2->sTypoAscender);
                SET_SIGNED(maxDescent, - int16_t(os2->sTypoDescender));
                SET_SIGNED(externalLeading, os2->sTypoLineGap);
            }
        }
    }

#undef SET_SIGNED
#undef SET_UNSIGNED

    mIsValid = true;

    return true;
}

static double
RoundToNearestMultiple(double aValue, double aFraction)
{
    return floor(aValue/aFraction + 0.5) * aFraction;
}

void gfxFont::CalculateDerivedMetrics(Metrics& aMetrics)
{
    aMetrics.maxAscent =
        ceil(RoundToNearestMultiple(aMetrics.maxAscent, 1/1024.0));
    aMetrics.maxDescent =
        ceil(RoundToNearestMultiple(aMetrics.maxDescent, 1/1024.0));

    if (aMetrics.xHeight <= 0) {
        // only happens if we couldn't find either font metrics
        // or a char to measure;
        // pick an arbitrary value that's better than zero
        aMetrics.xHeight = aMetrics.maxAscent * DEFAULT_XHEIGHT_FACTOR;
    }

    // If we have a font that doesn't provide a capHeight value, use maxAscent
    // as a reasonable fallback.
    if (aMetrics.capHeight <= 0) {
        aMetrics.capHeight = aMetrics.maxAscent;
    }

    aMetrics.maxHeight = aMetrics.maxAscent + aMetrics.maxDescent;

    if (aMetrics.maxHeight - aMetrics.emHeight > 0.0) {
        aMetrics.internalLeading = aMetrics.maxHeight - aMetrics.emHeight;
    } else {
        aMetrics.internalLeading = 0.0;
    }

    aMetrics.emAscent = aMetrics.maxAscent * aMetrics.emHeight
                            / aMetrics.maxHeight;
    aMetrics.emDescent = aMetrics.emHeight - aMetrics.emAscent;

    if (GetFontEntry()->IsFixedPitch()) {
        // Some Quartz fonts are fixed pitch, but there's some glyph with a bigger
        // advance than the average character width... this forces
        // those fonts to be recognized like fixed pitch fonts by layout.
        aMetrics.maxAdvance = aMetrics.aveCharWidth;
    }

    if (!aMetrics.strikeoutOffset) {
        aMetrics.strikeoutOffset = aMetrics.xHeight * 0.5;
    }
    if (!aMetrics.strikeoutSize) {
        aMetrics.strikeoutSize = aMetrics.underlineSize;
    }
}

void
gfxFont::SanitizeMetrics(gfxFont::Metrics *aMetrics, bool aIsBadUnderlineFont)
{
    // Even if this font size is zero, this font is created with non-zero size.
    // However, for layout and others, we should return the metrics of zero size font.
    if (mStyle.size == 0.0 || mStyle.sizeAdjust == 0.0) {
        memset(aMetrics, 0, sizeof(gfxFont::Metrics));
        return;
    }

    aMetrics->underlineSize = std::max(1.0, aMetrics->underlineSize);
    aMetrics->strikeoutSize = std::max(1.0, aMetrics->strikeoutSize);

    aMetrics->underlineOffset = std::min(aMetrics->underlineOffset, -1.0);

    if (aMetrics->maxAscent < 1.0) {
        // We cannot draw strikeout line and overline in the ascent...
        aMetrics->underlineSize = 0;
        aMetrics->underlineOffset = 0;
        aMetrics->strikeoutSize = 0;
        aMetrics->strikeoutOffset = 0;
        return;
    }

    /**
     * Some CJK fonts have bad underline offset. Therefore, if this is such font,
     * we need to lower the underline offset to bottom of *em* descent.
     * However, if this is system font, we should not do this for the rendering compatibility with
     * another application's UI on the platform.
     * XXX Should not use this hack if the font size is too small?
     *     Such text cannot be read, this might be used for tight CSS rendering? (E.g., Acid2)
     */
    if (!mStyle.systemFont && aIsBadUnderlineFont) {
        // First, we need 2 pixels between baseline and underline at least. Because many CJK characters
        // put their glyphs on the baseline, so, 1 pixel is too close for CJK characters.
        aMetrics->underlineOffset = std::min(aMetrics->underlineOffset, -2.0);

        // Next, we put the underline to bottom of below of the descent space.
        if (aMetrics->internalLeading + aMetrics->externalLeading > aMetrics->underlineSize) {
            aMetrics->underlineOffset = std::min(aMetrics->underlineOffset, -aMetrics->emDescent);
        } else {
            aMetrics->underlineOffset = std::min(aMetrics->underlineOffset,
                                               aMetrics->underlineSize - aMetrics->emDescent);
        }
    }
    // If underline positioned is too far from the text, descent position is preferred so that underline
    // will stay within the boundary.
    else if (aMetrics->underlineSize - aMetrics->underlineOffset > aMetrics->maxDescent) {
        if (aMetrics->underlineSize > aMetrics->maxDescent)
            aMetrics->underlineSize = std::max(aMetrics->maxDescent, 1.0);
        // The max underlineOffset is 1px (the min underlineSize is 1px, and min maxDescent is 0px.)
        aMetrics->underlineOffset = aMetrics->underlineSize - aMetrics->maxDescent;
    }

    // If strikeout line is overflowed from the ascent, the line should be resized and moved for
    // that being in the ascent space.
    // Note that the strikeoutOffset is *middle* of the strikeout line position.
    gfxFloat halfOfStrikeoutSize = floor(aMetrics->strikeoutSize / 2.0 + 0.5);
    if (halfOfStrikeoutSize + aMetrics->strikeoutOffset > aMetrics->maxAscent) {
        if (aMetrics->strikeoutSize > aMetrics->maxAscent) {
            aMetrics->strikeoutSize = std::max(aMetrics->maxAscent, 1.0);
            halfOfStrikeoutSize = floor(aMetrics->strikeoutSize / 2.0 + 0.5);
        }
        gfxFloat ascent = floor(aMetrics->maxAscent + 0.5);
        aMetrics->strikeoutOffset = std::max(halfOfStrikeoutSize, ascent / 2.0);
    }

    // If overline is larger than the ascent, the line should be resized.
    if (aMetrics->underlineSize > aMetrics->maxAscent) {
        aMetrics->underlineSize = aMetrics->maxAscent;
    }
}

// Create a Metrics record to be used for vertical layout. This should never
// fail, as we've already decided this is a valid font. We do not have the
// option of marking it invalid (as can happen if we're unable to read
// horizontal metrics), because that could break a font that we're already
// using for horizontal text.
// So we will synthesize *something* usable here even if there aren't any of the
// usual font tables (which can happen in the case of a legacy bitmap or Type1
// font for which the platform-specific backend used platform APIs instead of
// sfnt tables to create the horizontal metrics).
const gfxFont::Metrics*
gfxFont::CreateVerticalMetrics()
{
    const uint32_t kHheaTableTag = TRUETYPE_TAG('h','h','e','a');
    const uint32_t kVheaTableTag = TRUETYPE_TAG('v','h','e','a');
    const uint32_t kPostTableTag = TRUETYPE_TAG('p','o','s','t');
    const uint32_t kOS_2TableTag = TRUETYPE_TAG('O','S','/','2');
    uint32_t len;

    Metrics* metrics = new Metrics;
    ::memset(metrics, 0, sizeof(Metrics));

    // Some basic defaults, in case the font lacks any real metrics tables.
    // TODO: consider what rounding (if any) we should apply to these.
    metrics->emHeight = GetAdjustedSize();
    metrics->emAscent = metrics->emHeight / 2;
    metrics->emDescent = metrics->emHeight - metrics->emAscent;

    metrics->maxAscent = metrics->emAscent;
    metrics->maxDescent = metrics->emDescent;

    const float UNINITIALIZED_LEADING = -10000.0f;
    metrics->externalLeading = UNINITIALIZED_LEADING;

    if (mFUnitsConvFactor < 0.0) {
        uint16_t upem = GetFontEntry()->UnitsPerEm();
        if (upem != gfxFontEntry::kInvalidUPEM) {
            mFUnitsConvFactor = GetAdjustedSize() / upem;
        }
    }

#define SET_UNSIGNED(field,src) metrics->field = uint16_t(src) * mFUnitsConvFactor
#define SET_SIGNED(field,src)   metrics->field = int16_t(src) * mFUnitsConvFactor

    gfxFontEntry::AutoTable os2Table(mFontEntry, kOS_2TableTag);
    if (os2Table && mFUnitsConvFactor >= 0.0) {
        const OS2Table *os2 =
            reinterpret_cast<const OS2Table*>(hb_blob_get_data(os2Table, &len));
        // These fields should always be present in any valid OS/2 table
        if (len >= offsetof(OS2Table, sTypoLineGap) + sizeof(int16_t)) {
            SET_SIGNED(strikeoutSize, os2->yStrikeoutSize);
            // Use ascent+descent from the horizontal metrics as the default
            // advance (aveCharWidth) in vertical mode
            gfxFloat ascentDescent = gfxFloat(mFUnitsConvFactor) *
                (int16_t(os2->sTypoAscender) - int16_t(os2->sTypoDescender));
            metrics->aveCharWidth =
                std::max(metrics->emHeight, ascentDescent);
            // Use xAvgCharWidth from horizontal metrics as minimum font extent
            // for vertical layout, applying half of it to ascent and half to
            // descent (to work with a default centered baseline).
            gfxFloat halfCharWidth =
                int16_t(os2->xAvgCharWidth) * gfxFloat(mFUnitsConvFactor) / 2;
            metrics->maxAscent = std::max(metrics->maxAscent, halfCharWidth);
            metrics->maxDescent = std::max(metrics->maxDescent, halfCharWidth);
        }
    }

    // If we didn't set aveCharWidth from OS/2, try to read 'hhea' metrics
    // and use the line height from its ascent/descent.
    if (!metrics->aveCharWidth) {
        gfxFontEntry::AutoTable hheaTable(mFontEntry, kHheaTableTag);
        if (hheaTable && mFUnitsConvFactor >= 0.0) {
            const MetricsHeader* hhea =
                reinterpret_cast<const MetricsHeader*>
                    (hb_blob_get_data(hheaTable, &len));
            if (len >= sizeof(MetricsHeader)) {
                SET_SIGNED(aveCharWidth, int16_t(hhea->ascender) -
                                         int16_t(hhea->descender));
                metrics->maxAscent = metrics->aveCharWidth / 2;
                metrics->maxDescent =
                    metrics->aveCharWidth - metrics->maxAscent;
            }
        }
    }

    // Read real vertical metrics if available.
    gfxFontEntry::AutoTable vheaTable(mFontEntry, kVheaTableTag);
    if (vheaTable && mFUnitsConvFactor >= 0.0) {
        const MetricsHeader* vhea =
            reinterpret_cast<const MetricsHeader*>
                (hb_blob_get_data(vheaTable, &len));
        if (len >= sizeof(MetricsHeader)) {
            SET_UNSIGNED(maxAdvance, vhea->advanceWidthMax);
            // Redistribute space between ascent/descent because we want a
            // centered vertical baseline by default.
            gfxFloat halfExtent = 0.5 * gfxFloat(mFUnitsConvFactor) *
                (int16_t(vhea->ascender) + std::abs(int16_t(vhea->descender)));
            // Some bogus fonts have ascent and descent set to zero in 'vhea'.
            // In that case we just ignore them and keep our synthetic values
            // from above.
            if (halfExtent > 0) {
                metrics->maxAscent = halfExtent;
                metrics->maxDescent = halfExtent;
                SET_SIGNED(externalLeading, vhea->lineGap);
            }
        }
    }

    // If we didn't set aveCharWidth above, we must be dealing with a non-sfnt
    // font of some kind (Type1, bitmap, vector, ...), so fall back to using
    // whatever the platform backend figured out for horizontal layout.
    // And if we haven't set externalLeading yet, then copy that from the
    // horizontal metrics as well, to help consistency of CSS line-height.
    if (!metrics->aveCharWidth ||
        metrics->externalLeading == UNINITIALIZED_LEADING) {
        const Metrics& horizMetrics = GetHorizontalMetrics();
        if (!metrics->aveCharWidth) {
            metrics->aveCharWidth = horizMetrics.maxAscent + horizMetrics.maxDescent;
        }
        if (metrics->externalLeading == UNINITIALIZED_LEADING) {
            metrics->externalLeading = horizMetrics.externalLeading;
        }
    }

    // Get underline thickness from the 'post' table if available.
    gfxFontEntry::AutoTable postTable(mFontEntry, kPostTableTag);
    if (postTable) {
        const PostTable *post =
            reinterpret_cast<const PostTable*>(hb_blob_get_data(postTable,
                                                                &len));
        if (len >= offsetof(PostTable, underlineThickness) +
                       sizeof(uint16_t)) {
            SET_UNSIGNED(underlineSize, post->underlineThickness);
            // Also use for strikeout if we didn't find that in OS/2 above.
            if (!metrics->strikeoutSize) {
                metrics->strikeoutSize = metrics->underlineSize;
            }
        }
    }

#undef SET_UNSIGNED
#undef SET_SIGNED

    // If we didn't read this from a vhea table, it will still be zero.
    // In any case, let's make sure it is not less than the value we've
    // come up with for aveCharWidth.
    metrics->maxAdvance = std::max(metrics->maxAdvance, metrics->aveCharWidth);

    // Thickness of underline and strikeout may have been read from tables,
    // but in case they were not present, ensure a minimum of 1 pixel.
    // We synthesize our own positions, as font metrics don't provide these
    // for vertical layout.
    metrics->underlineSize = std::max(1.0, metrics->underlineSize);
    metrics->underlineOffset = - metrics->maxDescent - metrics->underlineSize;

    metrics->strikeoutSize = std::max(1.0, metrics->strikeoutSize);
    metrics->strikeoutOffset = - 0.5 * metrics->strikeoutSize;

    // Somewhat arbitrary values for now, subject to future refinement...
    metrics->spaceWidth = metrics->aveCharWidth;
    metrics->zeroOrAveCharWidth = metrics->aveCharWidth;
    metrics->maxHeight = metrics->maxAscent + metrics->maxDescent;
    metrics->xHeight = metrics->emHeight / 2;
    metrics->capHeight = metrics->maxAscent;

    return metrics;
}

gfxFloat
gfxFont::SynthesizeSpaceWidth(uint32_t aCh)
{
    // return an appropriate width for various Unicode space characters
    // that we "fake" if they're not actually present in the font;
    // returns negative value if the char is not a known space.
    switch (aCh) {
    case 0x2000:                                 // en quad
    case 0x2002: return GetAdjustedSize() / 2;   // en space
    case 0x2001:                                 // em quad
    case 0x2003: return GetAdjustedSize();       // em space
    case 0x2004: return GetAdjustedSize() / 3;   // three-per-em space
    case 0x2005: return GetAdjustedSize() / 4;   // four-per-em space
    case 0x2006: return GetAdjustedSize() / 6;   // six-per-em space
    case 0x2007: return GetMetrics(eHorizontal).zeroOrAveCharWidth; // figure space
    case 0x2008: return GetMetrics(eHorizontal).spaceWidth; // punctuation space
    case 0x2009: return GetAdjustedSize() / 5;   // thin space
    case 0x200a: return GetAdjustedSize() / 10;  // hair space
    case 0x202f: return GetAdjustedSize() / 5;   // narrow no-break space
    default: return -1.0;
    }
}

void
gfxFont::AddSizeOfExcludingThis(MallocSizeOf aMallocSizeOf,
                                FontCacheSizes* aSizes) const
{
    for (uint32_t i = 0; i < mGlyphExtentsArray.Length(); ++i) {
        aSizes->mFontInstances +=
            mGlyphExtentsArray[i]->SizeOfIncludingThis(aMallocSizeOf);
    }
    if (mWordCache) {
        aSizes->mShapedWords += mWordCache->SizeOfIncludingThis(aMallocSizeOf);
    }
}

void
gfxFont::AddSizeOfIncludingThis(MallocSizeOf aMallocSizeOf,
                                FontCacheSizes* aSizes) const
{
    aSizes->mFontInstances += aMallocSizeOf(this);
    AddSizeOfExcludingThis(aMallocSizeOf, aSizes);
}

void
gfxFont::AddGlyphChangeObserver(GlyphChangeObserver *aObserver)
{
    if (!mGlyphChangeObservers) {
        mGlyphChangeObservers.reset(
            new nsTHashtable<nsPtrHashKey<GlyphChangeObserver>>);
    }
    mGlyphChangeObservers->PutEntry(aObserver);
}

void
gfxFont::RemoveGlyphChangeObserver(GlyphChangeObserver *aObserver)
{
    NS_ASSERTION(mGlyphChangeObservers, "No observers registered");
    NS_ASSERTION(mGlyphChangeObservers->Contains(aObserver), "Observer not registered");
    mGlyphChangeObservers->RemoveEntry(aObserver);
}


#define DEFAULT_PIXEL_FONT_SIZE 16.0f

/*static*/ uint32_t
gfxFontStyle::ParseFontLanguageOverride(const nsString& aLangTag)
{
  if (!aLangTag.Length() || aLangTag.Length() > 4) {
    return NO_FONT_LANGUAGE_OVERRIDE;
  }
  uint32_t index, result = 0;
  for (index = 0; index < aLangTag.Length(); ++index) {
    char16_t ch = aLangTag[index];
    if (!nsCRT::IsAscii(ch)) { // valid tags are pure ASCII
      return NO_FONT_LANGUAGE_OVERRIDE;
    }
    result = (result << 8) + ch;
  }
  while (index++ < 4) {
    result = (result << 8) + 0x20;
  }
  return result;
}

gfxFontStyle::gfxFontStyle() :
    language(nsGkAtoms::x_western),
    size(DEFAULT_PIXEL_FONT_SIZE), sizeAdjust(-1.0f), baselineOffset(0.0f),
    languageOverride(NO_FONT_LANGUAGE_OVERRIDE),
    weight(NS_FONT_WEIGHT_NORMAL), stretch(NS_FONT_STRETCH_NORMAL),
    style(NS_FONT_STYLE_NORMAL),
    variantCaps(NS_FONT_VARIANT_CAPS_NORMAL),
    variantSubSuper(NS_FONT_VARIANT_POSITION_NORMAL),
    systemFont(true), printerFont(false), useGrayscaleAntialiasing(false),
    allowSyntheticWeight(true), allowSyntheticStyle(true),
    noFallbackVariantFeatures(true),
    explicitLanguage(false)
{
}

gfxFontStyle::gfxFontStyle(uint8_t aStyle, uint16_t aWeight, int16_t aStretch,
                           gfxFloat aSize,
                           nsIAtom *aLanguage, bool aExplicitLanguage,
                           float aSizeAdjust, bool aSystemFont,
                           bool aPrinterFont,
                           bool aAllowWeightSynthesis,
                           bool aAllowStyleSynthesis,
                           const nsString& aLanguageOverride):
    language(aLanguage),
    size(aSize), sizeAdjust(aSizeAdjust), baselineOffset(0.0f),
    languageOverride(ParseFontLanguageOverride(aLanguageOverride)),
    weight(aWeight), stretch(aStretch),
    style(aStyle),
    variantCaps(NS_FONT_VARIANT_CAPS_NORMAL),
    variantSubSuper(NS_FONT_VARIANT_POSITION_NORMAL),
    systemFont(aSystemFont), printerFont(aPrinterFont),
    useGrayscaleAntialiasing(false),
    allowSyntheticWeight(aAllowWeightSynthesis),
    allowSyntheticStyle(aAllowStyleSynthesis),
    noFallbackVariantFeatures(true),
    explicitLanguage(aExplicitLanguage)
{
    MOZ_ASSERT(!mozilla::IsNaN(size));
    MOZ_ASSERT(!mozilla::IsNaN(sizeAdjust));

    if (weight > 900)
        weight = 900;
    if (weight < 100)
        weight = 100;

    if (size >= FONT_MAX_SIZE) {
        size = FONT_MAX_SIZE;
        sizeAdjust = -1.0f;
    } else if (size < 0.0) {
        NS_WARNING("negative font size");
        size = 0.0;
    }

    if (!language) {
        NS_WARNING("null language");
        language = nsGkAtoms::x_western;
    }
}

int8_t
gfxFontStyle::ComputeWeight() const
{
    int8_t baseWeight = (weight + 50) / 100;

    if (baseWeight < 0)
        baseWeight = 0;
    if (baseWeight > 9)
        baseWeight = 9;

    return baseWeight;
}

void
gfxFontStyle::AdjustForSubSuperscript(int32_t aAppUnitsPerDevPixel)
{
    NS_PRECONDITION(variantSubSuper != NS_FONT_VARIANT_POSITION_NORMAL &&
                    baselineOffset == 0,
                    "can't adjust this style for sub/superscript");

    // calculate the baseline offset (before changing the size)
    if (variantSubSuper == NS_FONT_VARIANT_POSITION_SUPER) {
        baselineOffset = size * -NS_FONT_SUPERSCRIPT_OFFSET_RATIO;
    } else {
        baselineOffset = size * NS_FONT_SUBSCRIPT_OFFSET_RATIO;
    }

    // calculate reduced size, roughly mimicing behavior of font-size: smaller
    float cssSize = size * aAppUnitsPerDevPixel / AppUnitsPerCSSPixel();
    if (cssSize < NS_FONT_SUB_SUPER_SMALL_SIZE) {
        size *= NS_FONT_SUB_SUPER_SIZE_RATIO_SMALL;
    } else if (cssSize >= NS_FONT_SUB_SUPER_LARGE_SIZE) {
        size *= NS_FONT_SUB_SUPER_SIZE_RATIO_LARGE;
    } else {
        gfxFloat t = (cssSize - NS_FONT_SUB_SUPER_SMALL_SIZE) /
                         (NS_FONT_SUB_SUPER_LARGE_SIZE -
                          NS_FONT_SUB_SUPER_SMALL_SIZE);
        size *= (1.0 - t) * NS_FONT_SUB_SUPER_SIZE_RATIO_SMALL +
                    t * NS_FONT_SUB_SUPER_SIZE_RATIO_LARGE;
    }

    // clear the variant field
    variantSubSuper = NS_FONT_VARIANT_POSITION_NORMAL;
}

bool
gfxFont::TryGetMathTable()
{
    if (!mMathInitialized) {
        mMathInitialized = true;

        hb_face_t *face = GetFontEntry()->GetHBFace();
        if (face) {
            if (hb_ot_math_has_data(face)) {
                mMathTable = MakeUnique<gfxMathTable>(face, GetAdjustedSize());
            }
            hb_face_destroy(face);
        }
    }

    return !!mMathTable;
}
