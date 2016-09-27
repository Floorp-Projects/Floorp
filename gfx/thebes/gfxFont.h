/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 et sw=4 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_FONT_H
#define GFX_FONT_H

#include "gfxTypes.h"
#include "gfxFontEntry.h"
#include "nsString.h"
#include "gfxPoint.h"
#include "gfxPattern.h"
#include "nsTArray.h"
#include "nsTHashtable.h"
#include "nsHashKeys.h"
#include "gfxRect.h"
#include "nsExpirationTracker.h"
#include "gfxPlatform.h"
#include "nsIAtom.h"
#include "mozilla/HashFunctions.h"
#include "nsIMemoryReporter.h"
#include "nsIObserver.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/Attributes.h"
#include <algorithm>
#include "DrawMode.h"
#include "nsDataHashtable.h"
#include "harfbuzz/hb.h"
#include "mozilla/gfx/2D.h"
#include "nsColor.h"

typedef struct _cairo cairo_t;
typedef struct _cairo_scaled_font cairo_scaled_font_t;
//typedef struct gr_face            gr_face;

#ifdef DEBUG
#include <stdio.h>
#endif

class gfxContext;
class gfxTextRun;
class gfxFont;
class gfxGlyphExtents;
class gfxShapedText;
class gfxShapedWord;
class gfxSkipChars;

#define FONT_MAX_SIZE                  2000.0

#define NO_FONT_LANGUAGE_OVERRIDE      0

#define SMALL_CAPS_SCALE_FACTOR        0.8

// The skew factor used for synthetic-italic [oblique] fonts;
// we use a platform-dependent value to harmonize with the platform's own APIs.
#ifdef XP_WIN
#define OBLIQUE_SKEW_FACTOR  0.3
#elif defined(MOZ_WIDGET_GTK)
#define OBLIQUE_SKEW_FACTOR  0.2
#else
#define OBLIQUE_SKEW_FACTOR  0.25
#endif

struct gfxTextRunDrawCallbacks;

namespace mozilla {
class SVGContextPaint;
namespace gfx {
class GlyphRenderingOptions;
} // namespace gfx
} // namespace mozilla

struct gfxFontStyle {
    gfxFontStyle();
    gfxFontStyle(uint8_t aStyle, uint16_t aWeight, int16_t aStretch,
                 gfxFloat aSize, nsIAtom *aLanguage, bool aExplicitLanguage,
                 float aSizeAdjust, bool aSystemFont,
                 bool aPrinterFont,
                 bool aWeightSynthesis, bool aStyleSynthesis,
                 const nsString& aLanguageOverride);
    gfxFontStyle(const gfxFontStyle& aStyle);

    // the language (may be an internal langGroup code rather than an actual
    // language code) specified in the document or element's lang property,
    // or inferred from the charset
    RefPtr<nsIAtom> language;

    // Features are composed of (1) features from style rules (2) features
    // from feature setttings rules and (3) family-specific features.  (1) and
    // (3) are guaranteed to be mutually exclusive

    // custom opentype feature settings
    nsTArray<gfxFontFeature> featureSettings;

    // Some font-variant property values require font-specific settings
    // defined via @font-feature-values rules.  These are resolved after
    // font matching occurs.

    // -- list of value tags for specific alternate features
    nsTArray<gfxAlternateValue> alternateValues;

    // -- object used to look these up once the font is matched
    RefPtr<gfxFontFeatureValueSet> featureValueLookup;

    // The logical size of the font, in pixels
    gfxFloat size;

    // The aspect-value (ie., the ratio actualsize:actualxheight) that any
    // actual physical font created from this font structure must have when
    // rendering or measuring a string. A value of -1.0 means no adjustment
    // needs to be done; otherwise the value must be nonnegative.
    float sizeAdjust;

    // baseline offset, used when simulating sub/superscript glyphs
    float baselineOffset;

    // Language system tag, to override document language;
    // an OpenType "language system" tag represented as a 32-bit integer
    // (see http://www.microsoft.com/typography/otspec/languagetags.htm).
    // Normally 0, so font rendering will use the document or element language
    // (see above) to control any language-specific rendering, but the author
    // can override this for cases where the options implemented in the font
    // do not directly match the actual language. (E.g. lang may be Macedonian,
    // but the font in use does not explicitly support this; the author can
    // use font-language-override to request the Serbian option in the font
    // in order to get correct glyph shapes.)
    uint32_t languageOverride;

    // The weight of the font: 100, 200, ... 900.
    uint16_t weight;

    // The stretch of the font (the sum of various NS_FONT_STRETCH_*
    // constants; see gfxFontConstants.h).
    int8_t stretch;

    // Say that this font is a system font and therefore does not
    // require certain fixup that we do for fonts from untrusted
    // sources.
    bool systemFont : 1;

    // Say that this font is used for print or print preview.
    bool printerFont : 1;

    // Used to imitate -webkit-font-smoothing: antialiased
    bool useGrayscaleAntialiasing : 1;

    // The style of font (normal, italic, oblique)
    uint8_t style : 2;

    // Whether synthetic styles are allowed
    bool allowSyntheticWeight : 1;
    bool allowSyntheticStyle : 1;

    // some variant features require fallback which complicates the shaping
    // code, so set up a bool to indicate when shaping with fallback is needed
    bool noFallbackVariantFeatures : 1;

    // whether the |language| field comes from explicit lang tagging in the
    // document, or was inferred from charset/system locale
    bool explicitLanguage : 1;

    // caps variant (small-caps, petite-caps, etc.)
    uint8_t variantCaps;

    // sub/superscript variant
    uint8_t variantSubSuper;

    // Return the final adjusted font size for the given aspect ratio.
    // Not meant to be called when sizeAdjust = -1.0.
    gfxFloat GetAdjustedSize(gfxFloat aspect) const {
        NS_ASSERTION(sizeAdjust >= 0.0, "Not meant to be called when sizeAdjust = -1.0");
        gfxFloat adjustedSize = std::max(NS_round(size*(sizeAdjust/aspect)), 1.0);
        return std::min(adjustedSize, FONT_MAX_SIZE);
    }

    PLDHashNumber Hash() const {
        return ((style + (systemFont << 7) +
            (weight << 8)) + uint32_t(size*1000) + uint32_t(sizeAdjust*1000)) ^
            nsISupportsHashKey::HashKey(language);
    }

    int8_t ComputeWeight() const;

    // Adjust this style to simulate sub/superscript (as requested in the
    // variantSubSuper field) using size and baselineOffset instead.
    void AdjustForSubSuperscript(int32_t aAppUnitsPerDevPixel);

    bool Equals(const gfxFontStyle& other) const {
        return
            (*reinterpret_cast<const uint64_t*>(&size) ==
             *reinterpret_cast<const uint64_t*>(&other.size)) &&
            (style == other.style) &&
            (variantCaps == other.variantCaps) &&
            (variantSubSuper == other.variantSubSuper) &&
            (allowSyntheticWeight == other.allowSyntheticWeight) &&
            (allowSyntheticStyle == other.allowSyntheticStyle) &&
            (systemFont == other.systemFont) &&
            (printerFont == other.printerFont) &&
            (useGrayscaleAntialiasing == other.useGrayscaleAntialiasing) &&
            (explicitLanguage == other.explicitLanguage) &&
            (weight == other.weight) &&
            (stretch == other.stretch) &&
            (language == other.language) &&
            (baselineOffset == other.baselineOffset) &&
            (*reinterpret_cast<const uint32_t*>(&sizeAdjust) ==
             *reinterpret_cast<const uint32_t*>(&other.sizeAdjust)) &&
            (featureSettings == other.featureSettings) &&
            (languageOverride == other.languageOverride) &&
            (alternateValues == other.alternateValues) &&
            (featureValueLookup == other.featureValueLookup);
    }

    static void ParseFontFeatureSettings(const nsString& aFeatureString,
                                         nsTArray<gfxFontFeature>& aFeatures);

    static uint32_t ParseFontLanguageOverride(const nsString& aLangTag);
};

struct gfxTextRange {
    enum {
        // flags for recording the kind of font-matching that was used
        kFontGroup      = 0x0001,
        kPrefsFallback  = 0x0002,
        kSystemFallback = 0x0004
    };
    gfxTextRange(uint32_t aStart, uint32_t aEnd,
                 gfxFont* aFont, uint8_t aMatchType,
                 uint16_t aOrientation)
        : start(aStart),
          end(aEnd),
          font(aFont),
          matchType(aMatchType),
          orientation(aOrientation)
    { }
    uint32_t Length() const { return end - start; }
    uint32_t start, end;
    RefPtr<gfxFont> font;
    uint8_t matchType;
    uint16_t orientation;
};


/**
 * Font cache design:
 * 
 * The mFonts hashtable contains most fonts, indexed by (gfxFontEntry*, style).
 * It does not add a reference to the fonts it contains.
 * When a font's refcount decreases to zero, instead of deleting it we
 * add it to our expiration tracker.
 * The expiration tracker tracks fonts with zero refcount. After a certain
 * period of time, such fonts expire and are deleted.
 *
 * We're using 3 generations with a ten-second generation interval, so
 * zero-refcount fonts will be deleted 20-30 seconds after their refcount
 * goes to zero, if timer events fire in a timely manner.
 *
 * The font cache also handles timed expiration of cached ShapedWords
 * for "persistent" fonts: it has a repeating timer, and notifies
 * each cached font to "age" its shaped words. The words will be released
 * by the fonts if they get aged three times without being re-used in the
 * meantime.
 *
 * Note that the ShapedWord timeout is much larger than the font timeout,
 * so that in the case of a short-lived font, we'll discard the gfxFont
 * completely, with all its words, and avoid the cost of aging the words
 * individually. That only happens with longer-lived fonts.
 */
struct FontCacheSizes {
    FontCacheSizes()
        : mFontInstances(0), mShapedWords(0)
    { }

    size_t mFontInstances; // memory used by instances of gfxFont subclasses
    size_t mShapedWords; // memory used by the per-font shapedWord caches
};

class gfxFontCache final : public nsExpirationTracker<gfxFont,3> {
public:
    enum {
        FONT_TIMEOUT_SECONDS = 10,
        SHAPED_WORD_TIMEOUT_SECONDS = 60
    };

    gfxFontCache();
    ~gfxFontCache();

    /*
     * Get the global gfxFontCache.  You must call Init() before
     * calling this method --- the result will not be null.
     */
    static gfxFontCache* GetCache() {
        return gGlobalCache;
    }

    static nsresult Init();
    // It's OK to call this even if Init() has not been called.
    static void Shutdown();

    // Look up a font in the cache. Returns an addrefed pointer, or null
    // if there's nothing matching in the cache
    already_AddRefed<gfxFont>
    Lookup(const gfxFontEntry* aFontEntry,
           const gfxFontStyle* aStyle,
           const gfxCharacterMap* aUnicodeRangeMap);

    // We created a new font (presumably because Lookup returned null);
    // put it in the cache. The font's refcount should be nonzero. It is
    // allowable to add a new font even if there is one already in the
    // cache with the same key; we'll forget about the old one.
    void AddNew(gfxFont *aFont);

    // The font's refcount has gone to zero; give ownership of it to
    // the cache. We delete it if it's not acquired again after a certain
    // amount of time.
    void NotifyReleased(gfxFont *aFont);

    // This gets called when the timeout has expired on a zero-refcount
    // font; we just delete it.
    virtual void NotifyExpired(gfxFont *aFont) override;

    // Cleans out the hashtable and removes expired fonts waiting for cleanup.
    // Other gfxFont objects may be still in use but they will be pushed
    // into the expiration queues and removed.
    void Flush() {
        mFonts.Clear();
        AgeAllGenerations();
    }

    void FlushShapedWordCaches();

    void AddSizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf,
                                FontCacheSizes* aSizes) const;
    void AddSizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf,
                                FontCacheSizes* aSizes) const;

protected:
    class MemoryReporter final : public nsIMemoryReporter
    {
        ~MemoryReporter() {}
    public:
        NS_DECL_ISUPPORTS
        NS_DECL_NSIMEMORYREPORTER
    };

    // Observer for notifications that the font cache cares about
    class Observer final
        : public nsIObserver
    {
        ~Observer() {}
    public:
        NS_DECL_ISUPPORTS
        NS_DECL_NSIOBSERVER
    };

    void DestroyFont(gfxFont *aFont);

    static gfxFontCache *gGlobalCache;

    struct Key {
        const gfxFontEntry* mFontEntry;
        const gfxFontStyle* mStyle;
        const gfxCharacterMap* mUnicodeRangeMap;
        Key(const gfxFontEntry* aFontEntry, const gfxFontStyle* aStyle,
            const gfxCharacterMap* aUnicodeRangeMap)
            : mFontEntry(aFontEntry), mStyle(aStyle),
              mUnicodeRangeMap(aUnicodeRangeMap)
        {}
    };

    class HashEntry : public PLDHashEntryHdr {
    public:
        typedef const Key& KeyType;
        typedef const Key* KeyTypePointer;

        // When constructing a new entry in the hashtable, we'll leave this
        // blank. The caller of Put() will fill this in.
        explicit HashEntry(KeyTypePointer aStr) : mFont(nullptr) { }
        HashEntry(const HashEntry& toCopy) : mFont(toCopy.mFont) { }
        ~HashEntry() { }

        bool KeyEquals(const KeyTypePointer aKey) const;
        static KeyTypePointer KeyToPointer(KeyType aKey) { return &aKey; }
        static PLDHashNumber HashKey(const KeyTypePointer aKey) {
            return mozilla::HashGeneric(aKey->mStyle->Hash(), aKey->mFontEntry,
                                        aKey->mUnicodeRangeMap);
        }
        enum { ALLOW_MEMMOVE = true };

        gfxFont* mFont;
    };

    nsTHashtable<HashEntry> mFonts;

    static void WordCacheExpirationTimerCallback(nsITimer* aTimer, void* aCache);
    nsCOMPtr<nsITimer>      mWordCacheExpirationTimer;
};

class gfxTextPerfMetrics {
public:

    struct TextCounts {
        uint32_t    numContentTextRuns;
        uint32_t    numChromeTextRuns;
        uint32_t    numChars;
        uint32_t    maxTextRunLen;
        uint32_t    wordCacheSpaceRules;
        uint32_t    wordCacheLong;
        uint32_t    wordCacheHit;
        uint32_t    wordCacheMiss;
        uint32_t    fallbackPrefs;
        uint32_t    fallbackSystem;
        uint32_t    textrunConst;
        uint32_t    textrunDestr;
        uint32_t    genericLookups;
    };

    uint32_t reflowCount;

    // counts per reflow operation
    TextCounts current;

    // totals for the lifetime of a document
    TextCounts cumulative;

    gfxTextPerfMetrics() {
        memset(this, 0, sizeof(gfxTextPerfMetrics));
    }

    // add current totals to cumulative ones
    void Accumulate() {
        if (current.numChars == 0) {
            return;
        }
        cumulative.numContentTextRuns += current.numContentTextRuns;
        cumulative.numChromeTextRuns += current.numChromeTextRuns;
        cumulative.numChars += current.numChars;
        if (current.maxTextRunLen > cumulative.maxTextRunLen) {
            cumulative.maxTextRunLen = current.maxTextRunLen;
        }
        cumulative.wordCacheSpaceRules += current.wordCacheSpaceRules;
        cumulative.wordCacheLong += current.wordCacheLong;
        cumulative.wordCacheHit += current.wordCacheHit;
        cumulative.wordCacheMiss += current.wordCacheMiss;
        cumulative.fallbackPrefs += current.fallbackPrefs;
        cumulative.fallbackSystem += current.fallbackSystem;
        cumulative.textrunConst += current.textrunConst;
        cumulative.textrunDestr += current.textrunDestr;
        cumulative.genericLookups += current.genericLookups;
        memset(&current, 0, sizeof(current));
    }
};

class gfxTextRunFactory {
    NS_INLINE_DECL_REFCOUNTING(gfxTextRunFactory)

public:
    typedef mozilla::gfx::DrawTarget DrawTarget;

    // Flags in the mask 0xFFFF0000 are reserved for textrun clients
    // Flags in the mask 0x0000F000 are reserved for per-platform fonts
    // Flags in the mask 0x00000FFF are set by the textrun creator.
    enum {
        CACHE_TEXT_FLAGS    = 0xF0000000,
        USER_TEXT_FLAGS     = 0x0FFF0000,
        TEXTRUN_TEXT_FLAGS  = 0x0000FFFF,
        SETTABLE_FLAGS      = CACHE_TEXT_FLAGS | USER_TEXT_FLAGS,

        /**
         * When set, the text string pointer used to create the text run
         * is guaranteed to be available during the lifetime of the text run.
         */
        TEXT_IS_PERSISTENT           = 0x0001,
        /**
         * When set, the text is known to be all-ASCII (< 128).
         */
        TEXT_IS_ASCII                = 0x0002,
        /**
         * When set, the text is RTL.
         */
        TEXT_IS_RTL                  = 0x0004,
        /**
         * When set, spacing is enabled and the textrun needs to call GetSpacing
         * on the spacing provider.
         */
        TEXT_ENABLE_SPACING          = 0x0008,
        /**
         * When set, GetHyphenationBreaks may return true for some character
         * positions, otherwise it will always return false for all characters.
         */
        TEXT_ENABLE_HYPHEN_BREAKS    = 0x0010,
        /**
         * When set, the text has no characters above 255 and it is stored
         * in the textrun in 8-bit format.
         */
        TEXT_IS_8BIT                 = 0x0020,
        /**
         * When set, the RunMetrics::mBoundingBox field will be initialized
         * properly based on glyph extents, in particular, glyph extents that
         * overflow the standard font-box (the box defined by the ascent, descent
         * and advance width of the glyph). When not set, it may just be the
         * standard font-box even if glyphs overflow.
         */
        TEXT_NEED_BOUNDING_BOX       = 0x0040,
        /**
         * When set, optional ligatures are disabled. Ligatures that are
         * required for legible text should still be enabled.
         */
        TEXT_DISABLE_OPTIONAL_LIGATURES = 0x0080,
        /**
         * When set, the textrun should favour speed of construction over
         * quality. This may involve disabling ligatures and/or kerning or
         * other effects.
         */
        TEXT_OPTIMIZE_SPEED          = 0x0100,
        /**
         * For internal use by the memory reporter when accounting for
         * storage used by textruns.
         * Because the reporter may visit each textrun multiple times while
         * walking the frame trees and textrun cache, it needs to mark
         * textruns that have been seen so as to avoid multiple-accounting.
         */
        TEXT_RUN_SIZE_ACCOUNTED      = 0x0200,
        /**
         * When set, the textrun should discard control characters instead of
         * turning them into hexboxes.
         */
        TEXT_HIDE_CONTROL_CHARACTERS = 0x0400,

        /**
         * Field for orientation of the textrun and glyphs within it.
         * Possible values of the TEXT_ORIENT_MASK field:
         *   TEXT_ORIENT_HORIZONTAL
         *   TEXT_ORIENT_VERTICAL_UPRIGHT
         *   TEXT_ORIENT_VERTICAL_SIDEWAYS_RIGHT
         *   TEXT_ORIENT_VERTICAL_SIDEWAYS_LEFT
         *   TEXT_ORIENT_VERTICAL_MIXED
         * For all VERTICAL settings, the x and y coordinates of glyph
         * positions are exchanged, so that simple advances are vertical.
         *
         * The MIXED value indicates vertical textRuns for which the CSS
         * text-orientation property is 'mixed', but is never used for
         * individual glyphRuns; it will be resolved to either UPRIGHT
         * or SIDEWAYS_RIGHT according to the UTR50 properties of the
         * characters, and separate glyphRuns created for the resulting
         * glyph orientations.
         */
        TEXT_ORIENT_MASK                    = 0xF000,
        TEXT_ORIENT_HORIZONTAL              = 0x0000,
        TEXT_ORIENT_VERTICAL_UPRIGHT        = 0x1000,
        TEXT_ORIENT_VERTICAL_SIDEWAYS_RIGHT = 0x2000,
        TEXT_ORIENT_VERTICAL_SIDEWAYS_LEFT  = 0x4000,
        TEXT_ORIENT_VERTICAL_MIXED          = 0x8000,

        /**
         * nsTextFrameThebes sets these, but they're defined here rather than
         * in nsTextFrameUtils.h because ShapedWord creation/caching also needs
         * to check the _INCOMING flag
         */
        TEXT_TRAILING_ARABICCHAR = 0x20000000,
        /**
         * When set, the previous character for this textrun was an Arabic
         * character.  This is used for the context detection necessary for
         * bidi.numeral implementation.
         */
        TEXT_INCOMING_ARABICCHAR = 0x40000000,

        // Set if the textrun should use the OpenType 'math' script.
        TEXT_USE_MATH_SCRIPT = 0x80000000,
    };

    /**
     * This record contains all the parameters needed to initialize a textrun.
     */
    struct Parameters {
        // Shape text params suggesting where the textrun will be rendered
        DrawTarget   *mDrawTarget;
        // Pointer to arbitrary user data (which should outlive the textrun)
        void         *mUserData;
        // A description of which characters have been stripped from the original
        // DOM string to produce the characters in the textrun. May be null
        // if that information is not relevant.
        gfxSkipChars *mSkipChars;
        // A list of where linebreaks are currently placed in the textrun. May
        // be null if mInitialBreakCount is zero.
        uint32_t     *mInitialBreaks;
        uint32_t      mInitialBreakCount;
        // The ratio to use to convert device pixels to application layout units
        int32_t       mAppUnitsPerDevUnit;
    };

protected:
    // Protected destructor, to discourage deletion outside of Release():
    virtual ~gfxTextRunFactory() {}
};

/**
 * gfxFontShaper
 *
 * This class implements text shaping (character to glyph mapping and
 * glyph layout). There is a gfxFontShaper subclass for each text layout
 * technology (uniscribe, core text, harfbuzz,....) we support.
 *
 * The shaper is responsible for setting up glyph data in gfxTextRuns.
 *
 * A generic, platform-independent shaper relies only on the standard
 * gfxFont interface and can work with any concrete subclass of gfxFont.
 *
 * Platform-specific implementations designed to interface to platform
 * shaping APIs such as Uniscribe or CoreText may rely on features of a
 * specific font subclass to access native font references
 * (such as CTFont, HFONT, DWriteFont, etc).
 */

class gfxFontShaper {
public:
    typedef mozilla::gfx::DrawTarget DrawTarget;
    typedef mozilla::unicode::Script Script;

    explicit gfxFontShaper(gfxFont *aFont)
        : mFont(aFont)
    {
        NS_ASSERTION(aFont, "shaper requires a valid font!");
    }

    virtual ~gfxFontShaper() { }

    // Shape a piece of text and store the resulting glyph data into
    // aShapedText. Parameters aOffset/aLength indicate the range of
    // aShapedText to be updated; aLength is also the length of aText.
    virtual bool ShapeText(DrawTarget     *aDrawTarget,
                           const char16_t *aText,
                           uint32_t        aOffset,
                           uint32_t        aLength,
                           Script          aScript,
                           bool            aVertical,
                           gfxShapedText  *aShapedText) = 0;

    gfxFont *GetFont() const { return mFont; }

    static void
    MergeFontFeatures(const gfxFontStyle *aStyle,
                      const nsTArray<gfxFontFeature>& aFontFeatures,
                      bool aDisableLigatures,
                      const nsAString& aFamilyName,
                      bool aAddSmallCaps,
                      void (*aHandleFeature)(const uint32_t&,
                                             uint32_t&, void*),
                      void* aHandleFeatureData);

protected:
    // Work out whether cairo will snap inter-glyph spacing to pixels.
    static void GetRoundOffsetsToPixels(DrawTarget* aDrawTarget,
                                        bool* aRoundX, bool* aRoundY);

    // the font this shaper is working with. The font owns a UniquePtr reference
    // to this object, and will destroy it before it dies. Thus, mFont will always
    // be valid.
    gfxFont* MOZ_NON_OWNING_REF mFont;
};


/*
 * gfxShapedText is an abstract superclass for gfxShapedWord and gfxTextRun.
 * These are objects that store a list of zero or more glyphs for each character.
 * For each glyph we store the glyph ID, the advance, and possibly x/y-offsets.
 * The idea is that a string is rendered by a loop that draws each glyph
 * at its designated offset from the current point, then advances the current
 * point by the glyph's advance in the direction of the textrun (LTR or RTL).
 * Each glyph advance is always rounded to the nearest appunit; this ensures
 * consistent results when dividing the text in a textrun into multiple text
 * frames (frame boundaries are always aligned to appunits). We optimize
 * for the case where a character has a single glyph and zero xoffset and yoffset,
 * and the glyph ID and advance are in a reasonable range so we can pack all
 * necessary data into 32 bits.
 *
 * gfxFontShaper can shape text into either a gfxShapedWord (cached by a gfxFont)
 * or directly into a gfxTextRun (for cases where we want to shape textruns in
 * their entirety rather than using cached words, because there may be layout
 * features that depend on the inter-word spaces).
 */
class gfxShapedText
{
public:
    typedef mozilla::unicode::Script Script;

    gfxShapedText(uint32_t aLength, uint32_t aFlags,
                  int32_t aAppUnitsPerDevUnit)
        : mLength(aLength)
        , mFlags(aFlags)
        , mAppUnitsPerDevUnit(aAppUnitsPerDevUnit)
    { }

    virtual ~gfxShapedText() { }

    /**
     * This class records the information associated with a character in the
     * input string. It's optimized for the case where there is one glyph
     * representing that character alone.
     * 
     * A character can have zero or more associated glyphs. Each glyph
     * has an advance width and an x and y offset.
     * A character may be the start of a cluster.
     * A character may be the start of a ligature group.
     * A character can be "missing", indicating that the system is unable
     * to render the character.
     * 
     * All characters in a ligature group conceptually share all the glyphs
     * associated with the characters in a group.
     */
    class CompressedGlyph {
    public:
        CompressedGlyph() { mValue = 0; }

        enum {
            // Indicates that a cluster and ligature group starts at this
            // character; this character has a single glyph with a reasonable
            // advance and zero offsets. A "reasonable" advance
            // is one that fits in the available bits (currently 12) (specified
            // in appunits).
            FLAG_IS_SIMPLE_GLYPH  = 0x80000000U,

            // Indicates whether a linebreak is allowed before this character;
            // this is a two-bit field that holds a FLAG_BREAK_TYPE_xxx value
            // indicating the kind of linebreak (if any) allowed here.
            FLAGS_CAN_BREAK_BEFORE = 0x60000000U,

            FLAGS_CAN_BREAK_SHIFT = 29,
            FLAG_BREAK_TYPE_NONE   = 0,
            FLAG_BREAK_TYPE_NORMAL = 1,
            FLAG_BREAK_TYPE_HYPHEN = 2,

            FLAG_CHAR_IS_SPACE     = 0x10000000U,

            // The advance is stored in appunits
            ADVANCE_MASK  = 0x0FFF0000U,
            ADVANCE_SHIFT = 16,

            GLYPH_MASK = 0x0000FFFFU,

            // Non-simple glyphs may or may not have glyph data in the
            // corresponding mDetailedGlyphs entry. They have the following
            // flag bits:

            // When NOT set, indicates that this character corresponds to a
            // missing glyph and should be skipped (or possibly, render the character
            // Unicode value in some special way). If there are glyphs,
            // the mGlyphID is actually the UTF16 character code. The bit is
            // inverted so we can memset the array to zero to indicate all missing.
            FLAG_NOT_MISSING              = 0x01,
            FLAG_NOT_CLUSTER_START        = 0x02,
            FLAG_NOT_LIGATURE_GROUP_START = 0x04,

            FLAG_CHAR_IS_TAB              = 0x08,
            FLAG_CHAR_IS_NEWLINE          = 0x10,
            // Per CSS Text Decoration Module Level 3, emphasis marks are not
            // drawn for any character in Unicode categories Z*, Cc, Cf, and Cn
            // which is not combined with any combining characters. This flag is
            // set for all those characters except 0x20 whitespace.
            FLAG_CHAR_NO_EMPHASIS_MARK    = 0x20,
            CHAR_TYPE_FLAGS_MASK          = 0x38,

            GLYPH_COUNT_MASK = 0x00FFFF00U,
            GLYPH_COUNT_SHIFT = 8
        };

        // "Simple glyphs" have a simple glyph ID, simple advance and their
        // x and y offsets are zero. Also the glyph extents do not overflow
        // the font-box defined by the font ascent, descent and glyph advance width.
        // These case is optimized to avoid storing DetailedGlyphs.

        // Returns true if the glyph ID aGlyph fits into the compressed representation
        static bool IsSimpleGlyphID(uint32_t aGlyph) {
            return (aGlyph & GLYPH_MASK) == aGlyph;
        }
        // Returns true if the advance aAdvance fits into the compressed representation.
        // aAdvance is in appunits.
        static bool IsSimpleAdvance(uint32_t aAdvance) {
            return (aAdvance & (ADVANCE_MASK >> ADVANCE_SHIFT)) == aAdvance;
        }

        bool IsSimpleGlyph() const { return (mValue & FLAG_IS_SIMPLE_GLYPH) != 0; }
        uint32_t GetSimpleAdvance() const { return (mValue & ADVANCE_MASK) >> ADVANCE_SHIFT; }
        uint32_t GetSimpleGlyph() const { return mValue & GLYPH_MASK; }

        bool IsMissing() const { return (mValue & (FLAG_NOT_MISSING|FLAG_IS_SIMPLE_GLYPH)) == 0; }
        bool IsClusterStart() const {
            return (mValue & FLAG_IS_SIMPLE_GLYPH) || !(mValue & FLAG_NOT_CLUSTER_START);
        }
        bool IsLigatureGroupStart() const {
            return (mValue & FLAG_IS_SIMPLE_GLYPH) || !(mValue & FLAG_NOT_LIGATURE_GROUP_START);
        }
        bool IsLigatureContinuation() const {
            return (mValue & FLAG_IS_SIMPLE_GLYPH) == 0 &&
                (mValue & (FLAG_NOT_LIGATURE_GROUP_START | FLAG_NOT_MISSING)) ==
                    (FLAG_NOT_LIGATURE_GROUP_START | FLAG_NOT_MISSING);
        }

        // Return true if the original character was a normal (breakable,
        // trimmable) space (U+0020). Not true for other characters that
        // may happen to map to the space glyph (U+00A0).
        bool CharIsSpace() const {
            return (mValue & FLAG_CHAR_IS_SPACE) != 0;
        }

        bool CharIsTab() const {
            return !IsSimpleGlyph() && (mValue & FLAG_CHAR_IS_TAB) != 0;
        }
        bool CharIsNewline() const {
            return !IsSimpleGlyph() && (mValue & FLAG_CHAR_IS_NEWLINE) != 0;
        }
        bool CharMayHaveEmphasisMark() const {
            return !CharIsSpace() &&
                (IsSimpleGlyph() || !(mValue & FLAG_CHAR_NO_EMPHASIS_MARK));
        }

        uint32_t CharTypeFlags() const {
            return IsSimpleGlyph() ? 0 : (mValue & CHAR_TYPE_FLAGS_MASK);
        }

        void SetClusterStart(bool aIsClusterStart) {
            NS_ASSERTION(!IsSimpleGlyph(),
                         "can't call SetClusterStart on simple glyphs");
            if (aIsClusterStart) {
                mValue &= ~FLAG_NOT_CLUSTER_START;
            } else {
                mValue |= FLAG_NOT_CLUSTER_START;
            }
        }

        uint8_t CanBreakBefore() const {
            return (mValue & FLAGS_CAN_BREAK_BEFORE) >> FLAGS_CAN_BREAK_SHIFT;
        }
        // Returns FLAGS_CAN_BREAK_BEFORE if the setting changed, 0 otherwise
        uint32_t SetCanBreakBefore(uint8_t aCanBreakBefore) {
            NS_ASSERTION(aCanBreakBefore <= 2,
                         "Bogus break-before value!");
            uint32_t breakMask = (uint32_t(aCanBreakBefore) << FLAGS_CAN_BREAK_SHIFT);
            uint32_t toggle = breakMask ^ (mValue & FLAGS_CAN_BREAK_BEFORE);
            mValue ^= toggle;
            return toggle;
        }

        CompressedGlyph& SetSimpleGlyph(uint32_t aAdvanceAppUnits, uint32_t aGlyph) {
            NS_ASSERTION(IsSimpleAdvance(aAdvanceAppUnits), "Advance overflow");
            NS_ASSERTION(IsSimpleGlyphID(aGlyph), "Glyph overflow");
            NS_ASSERTION(!CharTypeFlags(), "Char type flags lost");
            mValue = (mValue & (FLAGS_CAN_BREAK_BEFORE | FLAG_CHAR_IS_SPACE)) |
                FLAG_IS_SIMPLE_GLYPH |
                (aAdvanceAppUnits << ADVANCE_SHIFT) | aGlyph;
            return *this;
        }
        CompressedGlyph& SetComplex(bool aClusterStart, bool aLigatureStart,
                uint32_t aGlyphCount) {
            mValue = (mValue & (FLAGS_CAN_BREAK_BEFORE | FLAG_CHAR_IS_SPACE)) |
                FLAG_NOT_MISSING |
                CharTypeFlags() |
                (aClusterStart ? 0 : FLAG_NOT_CLUSTER_START) |
                (aLigatureStart ? 0 : FLAG_NOT_LIGATURE_GROUP_START) |
                (aGlyphCount << GLYPH_COUNT_SHIFT);
            return *this;
        }
        /**
         * Missing glyphs are treated as ligature group starts; don't mess with
         * the cluster-start flag (see bugs 618870 and 619286).
         */
        CompressedGlyph& SetMissing(uint32_t aGlyphCount) {
            mValue = (mValue & (FLAGS_CAN_BREAK_BEFORE | FLAG_NOT_CLUSTER_START |
                                FLAG_CHAR_IS_SPACE)) |
                CharTypeFlags() |
                (aGlyphCount << GLYPH_COUNT_SHIFT);
            return *this;
        }
        uint32_t GetGlyphCount() const {
            NS_ASSERTION(!IsSimpleGlyph(), "Expected non-simple-glyph");
            return (mValue & GLYPH_COUNT_MASK) >> GLYPH_COUNT_SHIFT;
        }

        void SetIsSpace() {
            mValue |= FLAG_CHAR_IS_SPACE;
        }
        void SetIsTab() {
            NS_ASSERTION(!IsSimpleGlyph(), "Expected non-simple-glyph");
            mValue |= FLAG_CHAR_IS_TAB;
        }
        void SetIsNewline() {
            NS_ASSERTION(!IsSimpleGlyph(), "Expected non-simple-glyph");
            mValue |= FLAG_CHAR_IS_NEWLINE;
        }
        void SetNoEmphasisMark() {
            NS_ASSERTION(!IsSimpleGlyph(), "Expected non-simple-glyph");
            mValue |= FLAG_CHAR_NO_EMPHASIS_MARK;
        }

    private:
        uint32_t mValue;
    };

    // Accessor for the array of CompressedGlyph records, which will be in
    // a different place in gfxShapedWord vs gfxTextRun
    virtual const CompressedGlyph *GetCharacterGlyphs() const = 0;
    virtual CompressedGlyph *GetCharacterGlyphs() = 0;

    /**
     * When the glyphs for a character don't fit into a CompressedGlyph record
     * in SimpleGlyph format, we use an array of DetailedGlyphs instead.
     */
    struct DetailedGlyph {
        /** The glyphID, or the Unicode character
         * if this is a missing glyph */
        uint32_t mGlyphID;
        /** The advance, x-offset and y-offset of the glyph, in appunits
         *  mAdvance is in the text direction (RTL or LTR)
         *  mXOffset is always from left to right
         *  mYOffset is always from top to bottom */   
        int32_t  mAdvance;
        float    mXOffset, mYOffset;
    };

    void SetGlyphs(uint32_t aCharIndex, CompressedGlyph aGlyph,
                   const DetailedGlyph *aGlyphs);

    void SetMissingGlyph(uint32_t aIndex, uint32_t aChar, gfxFont *aFont);

    void SetIsSpace(uint32_t aIndex) {
        GetCharacterGlyphs()[aIndex].SetIsSpace();
    }

    bool HasDetailedGlyphs() const {
        return mDetailedGlyphs != nullptr;
    }

    bool IsLigatureGroupStart(uint32_t aPos) {
        NS_ASSERTION(aPos < GetLength(), "aPos out of range");
        return GetCharacterGlyphs()[aPos].IsLigatureGroupStart();
    }

    // NOTE that this must not be called for a character offset that does
    // not have any DetailedGlyph records; callers must have verified that
    // GetCharacterGlyphs()[aCharIndex].GetGlyphCount() is greater than zero.
    DetailedGlyph *GetDetailedGlyphs(uint32_t aCharIndex) const {
        NS_ASSERTION(GetCharacterGlyphs() && HasDetailedGlyphs() &&
                     !GetCharacterGlyphs()[aCharIndex].IsSimpleGlyph() &&
                     GetCharacterGlyphs()[aCharIndex].GetGlyphCount() > 0,
                     "invalid use of GetDetailedGlyphs; check the caller!");
        return mDetailedGlyphs->Get(aCharIndex);
    }

    void AdjustAdvancesForSyntheticBold(float aSynBoldOffset,
                                        uint32_t aOffset, uint32_t aLength);

    // Mark clusters in the CompressedGlyph records, starting at aOffset,
    // based on the Unicode properties of the text in aString.
    // This is also responsible to set the IsSpace flag for space characters.
    void SetupClusterBoundaries(uint32_t         aOffset,
                                const char16_t *aString,
                                uint32_t         aLength);
    // In 8-bit text, there won't actually be any clusters, but we still need
    // the space-marking functionality.
    void SetupClusterBoundaries(uint32_t       aOffset,
                                const uint8_t *aString,
                                uint32_t       aLength);

    uint32_t GetFlags() const {
        return mFlags;
    }

    bool IsVertical() const {
        return (GetFlags() & gfxTextRunFactory::TEXT_ORIENT_MASK) !=
                gfxTextRunFactory::TEXT_ORIENT_HORIZONTAL;
    }

    bool UseCenterBaseline() const {
        uint32_t orient = GetFlags() & gfxTextRunFactory::TEXT_ORIENT_MASK;
        return orient == gfxTextRunFactory::TEXT_ORIENT_VERTICAL_MIXED ||
               orient == gfxTextRunFactory::TEXT_ORIENT_VERTICAL_UPRIGHT;
    }

    bool IsRightToLeft() const {
        return (GetFlags() & gfxTextRunFactory::TEXT_IS_RTL) != 0;
    }

    bool IsSidewaysLeft() const {
        return (GetFlags() & gfxTextRunFactory::TEXT_ORIENT_MASK) ==
               gfxTextRunFactory::TEXT_ORIENT_VERTICAL_SIDEWAYS_LEFT;
    }

    // Return true if the logical inline direction is reversed compared to
    // normal physical coordinates (i.e. if it is leftwards or upwards)
    bool IsInlineReversed() const {
        return IsSidewaysLeft() != IsRightToLeft();
    }

    gfxFloat GetDirection() const {
        return IsInlineReversed() ? -1.0f : 1.0f;
    }

    bool DisableLigatures() const {
        return (GetFlags() &
                gfxTextRunFactory::TEXT_DISABLE_OPTIONAL_LIGATURES) != 0;
    }

    bool TextIs8Bit() const {
        return (GetFlags() & gfxTextRunFactory::TEXT_IS_8BIT) != 0;
    }

    int32_t GetAppUnitsPerDevUnit() const {
        return mAppUnitsPerDevUnit;
    }

    uint32_t GetLength() const {
        return mLength;
    }

    bool FilterIfIgnorable(uint32_t aIndex, uint32_t aCh);

protected:
    // Allocate aCount DetailedGlyphs for the given index
    DetailedGlyph *AllocateDetailedGlyphs(uint32_t aCharIndex,
                                          uint32_t aCount);

    // Ensure the glyph on the given index is complex glyph so that we can use
    // it to record specific characters that layout may need to detect.
    void EnsureComplexGlyph(uint32_t aIndex, CompressedGlyph& aGlyph)
    {
        MOZ_ASSERT(GetCharacterGlyphs() + aIndex == &aGlyph);
        if (aGlyph.IsSimpleGlyph()) {
            DetailedGlyph details = {
                aGlyph.GetSimpleGlyph(),
                (int32_t) aGlyph.GetSimpleAdvance(),
                0, 0
            };
            SetGlyphs(aIndex, CompressedGlyph().SetComplex(true, true, 1),
                      &details);
        }
    }

    // For characters whose glyph data does not fit the "simple" glyph criteria
    // in CompressedGlyph, we use a sorted array to store the association
    // between the source character offset and an index into an array 
    // DetailedGlyphs. The CompressedGlyph record includes a count of
    // the number of DetailedGlyph records that belong to the character,
    // starting at the given index.
    class DetailedGlyphStore {
    public:
        DetailedGlyphStore()
            : mLastUsed(0)
        { }

        // This is optimized for the most common calling patterns:
        // we rarely need random access to the records, access is most commonly
        // sequential through the textRun, so we record the last-used index
        // and check whether the caller wants the same record again, or the
        // next; if not, it's most likely we're starting over from the start
        // of the run, so we check the first entry before resorting to binary
        // search as a last resort.
        // NOTE that this must not be called for a character offset that does
        // not have any DetailedGlyph records; callers must have verified that
        // mCharacterGlyphs[aOffset].GetGlyphCount() is greater than zero
        // before calling this, otherwise the assertions here will fire (in a
        // debug build), and we'll probably crash.
        DetailedGlyph* Get(uint32_t aOffset) {
            NS_ASSERTION(mOffsetToIndex.Length() > 0,
                         "no detailed glyph records!");
            DetailedGlyph* details = mDetails.Elements();
            // check common cases (fwd iteration, initial entry, etc) first
            if (mLastUsed < mOffsetToIndex.Length() - 1 &&
                aOffset == mOffsetToIndex[mLastUsed + 1].mOffset) {
                ++mLastUsed;
            } else if (aOffset == mOffsetToIndex[0].mOffset) {
                mLastUsed = 0;
            } else if (aOffset == mOffsetToIndex[mLastUsed].mOffset) {
                // do nothing
            } else if (mLastUsed > 0 &&
                       aOffset == mOffsetToIndex[mLastUsed - 1].mOffset) {
                --mLastUsed;
            } else {
                mLastUsed =
                    mOffsetToIndex.BinaryIndexOf(aOffset, CompareToOffset());
            }
            NS_ASSERTION(mLastUsed != nsTArray<DGRec>::NoIndex,
                         "detailed glyph record missing!");
            return details + mOffsetToIndex[mLastUsed].mIndex;
        }

        DetailedGlyph* Allocate(uint32_t aOffset, uint32_t aCount) {
            uint32_t detailIndex = mDetails.Length();
            DetailedGlyph *details = mDetails.AppendElements(aCount);
            // We normally set up glyph records sequentially, so the common case
            // here is to append new records to the mOffsetToIndex array;
            // test for that before falling back to the InsertElementSorted
            // method.
            if (mOffsetToIndex.Length() == 0 ||
                aOffset > mOffsetToIndex[mOffsetToIndex.Length() - 1].mOffset) {
                mOffsetToIndex.AppendElement(DGRec(aOffset, detailIndex));
            } else {
                mOffsetToIndex.InsertElementSorted(DGRec(aOffset, detailIndex),
                                                   CompareRecordOffsets());
            }
            return details;
        }

        size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) {
            return aMallocSizeOf(this) +
                mDetails.ShallowSizeOfExcludingThis(aMallocSizeOf) +
                mOffsetToIndex.ShallowSizeOfExcludingThis(aMallocSizeOf);
        }

    private:
        struct DGRec {
            DGRec(const uint32_t& aOffset, const uint32_t& aIndex)
                : mOffset(aOffset), mIndex(aIndex) { }
            uint32_t mOffset; // source character offset in the textrun
            uint32_t mIndex;  // index where this char's DetailedGlyphs begin
        };

        struct CompareToOffset {
            bool Equals(const DGRec& a, const uint32_t& b) const {
                return a.mOffset == b;
            }
            bool LessThan(const DGRec& a, const uint32_t& b) const {
                return a.mOffset < b;
            }
        };

        struct CompareRecordOffsets {
            bool Equals(const DGRec& a, const DGRec& b) const {
                return a.mOffset == b.mOffset;
            }
            bool LessThan(const DGRec& a, const DGRec& b) const {
                return a.mOffset < b.mOffset;
            }
        };

        // Concatenated array of all the DetailedGlyph records needed for the
        // textRun; individual character offsets are associated with indexes
        // into this array via the mOffsetToIndex table.
        nsTArray<DetailedGlyph>     mDetails;

        // For each character offset that needs DetailedGlyphs, we record the
        // index in mDetails where the list of glyphs begins. This array is
        // sorted by mOffset.
        nsTArray<DGRec>             mOffsetToIndex;

        // Records the most recently used index into mOffsetToIndex, so that
        // we can support sequential access more quickly than just doing
        // a binary search each time.
        nsTArray<DGRec>::index_type mLastUsed;
    };

    mozilla::UniquePtr<DetailedGlyphStore>   mDetailedGlyphs;

    // Number of char16_t characters and CompressedGlyph glyph records
    uint32_t                        mLength;

    // Shaping flags (direction, ligature-suppression)
    uint32_t                        mFlags;

    int32_t                         mAppUnitsPerDevUnit;
};

/*
 * gfxShapedWord: an individual (space-delimited) run of text shaped with a
 * particular font, without regard to external context.
 *
 * The glyph data is copied into gfxTextRuns as needed from the cache of
 * ShapedWords associated with each gfxFont instance.
 */
class gfxShapedWord final : public gfxShapedText
{
public:
    typedef mozilla::unicode::Script Script;

    // Create a ShapedWord that can hold glyphs for aLength characters,
    // with mCharacterGlyphs sized appropriately.
    //
    // Returns null on allocation failure (does NOT use infallible alloc)
    // so caller must check for success.
    //
    // This does NOT perform shaping, so the returned word contains no
    // glyph data; the caller must call gfxFont::ShapeText() with appropriate
    // parameters to set up the glyphs.
    static gfxShapedWord* Create(const uint8_t *aText, uint32_t aLength,
                                 Script aRunScript,
                                 int32_t aAppUnitsPerDevUnit,
                                 uint32_t aFlags) {
        NS_ASSERTION(aLength <= gfxPlatform::GetPlatform()->WordCacheCharLimit(),
                     "excessive length for gfxShapedWord!");

        // Compute size needed including the mCharacterGlyphs array
        // and a copy of the original text
        uint32_t size =
            offsetof(gfxShapedWord, mCharGlyphsStorage) +
            aLength * (sizeof(CompressedGlyph) + sizeof(uint8_t));
        void *storage = malloc(size);
        if (!storage) {
            return nullptr;
        }

        // Construct in the pre-allocated storage, using placement new
        return new (storage) gfxShapedWord(aText, aLength, aRunScript,
                                           aAppUnitsPerDevUnit, aFlags);
    }

    static gfxShapedWord* Create(const char16_t *aText, uint32_t aLength,
                                 Script aRunScript,
                                 int32_t aAppUnitsPerDevUnit,
                                 uint32_t aFlags) {
        NS_ASSERTION(aLength <= gfxPlatform::GetPlatform()->WordCacheCharLimit(),
                     "excessive length for gfxShapedWord!");

        // In the 16-bit version of Create, if the TEXT_IS_8BIT flag is set,
        // then we convert the text to an 8-bit version and call the 8-bit
        // Create function instead.
        if (aFlags & gfxTextRunFactory::TEXT_IS_8BIT) {
            nsAutoCString narrowText;
            LossyAppendUTF16toASCII(nsDependentSubstring(aText, aLength),
                                    narrowText);
            return Create((const uint8_t*)(narrowText.BeginReading()),
                          aLength, aRunScript, aAppUnitsPerDevUnit, aFlags);
        }

        uint32_t size =
            offsetof(gfxShapedWord, mCharGlyphsStorage) +
            aLength * (sizeof(CompressedGlyph) + sizeof(char16_t));
        void *storage = malloc(size);
        if (!storage) {
            return nullptr;
        }

        return new (storage) gfxShapedWord(aText, aLength, aRunScript,
                                           aAppUnitsPerDevUnit, aFlags);
    }

    // Override operator delete to properly free the object that was
    // allocated via malloc.
    void operator delete(void* p) {
        free(p);
    }

    virtual const CompressedGlyph *GetCharacterGlyphs() const override {
        return &mCharGlyphsStorage[0];
    }
    virtual CompressedGlyph *GetCharacterGlyphs() override {
        return &mCharGlyphsStorage[0];
    }

    const uint8_t* Text8Bit() const {
        NS_ASSERTION(TextIs8Bit(), "invalid use of Text8Bit()");
        return reinterpret_cast<const uint8_t*>(mCharGlyphsStorage + GetLength());
    }

    const char16_t* TextUnicode() const {
        NS_ASSERTION(!TextIs8Bit(), "invalid use of TextUnicode()");
        return reinterpret_cast<const char16_t*>(mCharGlyphsStorage + GetLength());
    }

    char16_t GetCharAt(uint32_t aOffset) const {
        NS_ASSERTION(aOffset < GetLength(), "aOffset out of range");
        return TextIs8Bit() ?
            char16_t(Text8Bit()[aOffset]) : TextUnicode()[aOffset];
    }

    Script GetScript() const {
        return mScript;
    }

    void ResetAge() {
        mAgeCounter = 0;
    }
    uint32_t IncrementAge() {
        return ++mAgeCounter;
    }

    // Helper used when hashing a word for the shaped-word caches
    static uint32_t HashMix(uint32_t aHash, char16_t aCh)
    {
        return (aHash >> 28) ^ (aHash << 4) ^ aCh;
    }

private:
    // so that gfxTextRun can share our DetailedGlyphStore class
    friend class gfxTextRun;

    // Construct storage for a ShapedWord, ready to receive glyph data
    gfxShapedWord(const uint8_t *aText, uint32_t aLength,
                  Script aRunScript,
                  int32_t aAppUnitsPerDevUnit, uint32_t aFlags)
        : gfxShapedText(aLength, aFlags | gfxTextRunFactory::TEXT_IS_8BIT,
                        aAppUnitsPerDevUnit)
        , mScript(aRunScript)
        , mAgeCounter(0)
    {
        memset(mCharGlyphsStorage, 0, aLength * sizeof(CompressedGlyph));
        uint8_t *text = reinterpret_cast<uint8_t*>(&mCharGlyphsStorage[aLength]);
        memcpy(text, aText, aLength * sizeof(uint8_t));
    }

    gfxShapedWord(const char16_t *aText, uint32_t aLength,
                  Script aRunScript,
                  int32_t aAppUnitsPerDevUnit, uint32_t aFlags)
        : gfxShapedText(aLength, aFlags, aAppUnitsPerDevUnit)
        , mScript(aRunScript)
        , mAgeCounter(0)
    {
        memset(mCharGlyphsStorage, 0, aLength * sizeof(CompressedGlyph));
        char16_t *text = reinterpret_cast<char16_t*>(&mCharGlyphsStorage[aLength]);
        memcpy(text, aText, aLength * sizeof(char16_t));
        SetupClusterBoundaries(0, aText, aLength);
    }

    Script           mScript;

    uint32_t         mAgeCounter;

    // The mCharGlyphsStorage array is actually a variable-size member;
    // when the ShapedWord is created, its size will be increased as necessary
    // to allow the proper number of glyphs to be stored.
    // The original text, in either 8-bit or 16-bit form, will be stored
    // immediately following the CompressedGlyphs.
    CompressedGlyph  mCharGlyphsStorage[1];
};

class GlyphBufferAzure;
struct TextRunDrawParams;
struct FontDrawParams;
struct EmphasisMarkDrawParams;

class gfxFont {

    friend class gfxHarfBuzzShaper;
    friend class gfxGraphiteShaper;

protected:
    typedef mozilla::gfx::DrawTarget DrawTarget;
    typedef mozilla::unicode::Script Script;
    typedef mozilla::SVGContextPaint SVGContextPaint;

public:
    nsrefcnt AddRef(void) {
        NS_PRECONDITION(int32_t(mRefCnt) >= 0, "illegal refcnt");
        if (mExpirationState.IsTracked()) {
            gfxFontCache::GetCache()->RemoveObject(this);
        }
        ++mRefCnt;
        NS_LOG_ADDREF(this, mRefCnt, "gfxFont", sizeof(*this));
        return mRefCnt;
    }
    nsrefcnt Release(void) {
        NS_PRECONDITION(0 != mRefCnt, "dup release");
        --mRefCnt;
        NS_LOG_RELEASE(this, mRefCnt, "gfxFont");
        if (mRefCnt == 0) {
            NotifyReleased();
            // |this| may have been deleted.
            return 0;
        }
        return mRefCnt;
    }

    int32_t GetRefCount() { return mRefCnt; }

    // options to specify the kind of AA to be used when creating a font
    typedef enum {
        kAntialiasDefault,
        kAntialiasNone,
        kAntialiasGrayscale,
        kAntialiasSubpixel
    } AntialiasOption;

protected:
    nsAutoRefCnt mRefCnt;
    cairo_scaled_font_t *mScaledFont;

    void NotifyReleased() {
        gfxFontCache *cache = gfxFontCache::GetCache();
        if (cache) {
            // Don't delete just yet; return the object to the cache for
            // possibly recycling within some time limit
            cache->NotifyReleased(this);
        } else {
            // The cache may have already been shut down.
            delete this;
        }
    }

    gfxFont(gfxFontEntry *aFontEntry, const gfxFontStyle *aFontStyle,
            AntialiasOption anAAOption = kAntialiasDefault,
            cairo_scaled_font_t *aScaledFont = nullptr);

public:
    virtual ~gfxFont();

    bool Valid() const {
        return mIsValid;
    }

    // options for the kind of bounding box to return from measurement
    typedef enum {
        LOOSE_INK_EXTENTS,
            // A box that encloses all the painted pixels, and may
            // include sidebearings and/or additional ascent/descent
            // within the glyph cell even if the ink is smaller.
        TIGHT_INK_EXTENTS,
            // A box that tightly encloses all the painted pixels
            // (although actually on Windows, at least, it may be
            // slightly larger than strictly necessary because
            // we can't get precise extents with ClearType).
        TIGHT_HINTED_OUTLINE_EXTENTS
            // A box that tightly encloses the glyph outline,
            // ignoring possible antialiasing pixels that extend
            // beyond this.
            // NOTE: The default implementation of gfxFont::Measure(),
            // which works with the glyph extents cache, does not
            // differentiate between this and TIGHT_INK_EXTENTS.
            // Whether the distinction is important depends on the
            // antialiasing behavior of the platform; currently the
            // distinction is only implemented in the gfxWindowsFont
            // subclass, because of ClearType's tendency to paint
            // outside the hinted outline.
            // Also NOTE: it is relatively expensive to request this,
            // as it does not use cached glyph extents in the font.
    } BoundingBoxType;

    const nsString& GetName() const { return mFontEntry->Name(); }
    const gfxFontStyle *GetStyle() const { return &mStyle; }

    virtual cairo_scaled_font_t* GetCairoScaledFont() { return mScaledFont; }

    virtual gfxFont* CopyWithAntialiasOption(AntialiasOption anAAOption) {
        // platforms where this actually matters should override
        return nullptr;
    }

    gfxFloat GetAdjustedSize() const {
        return mAdjustedSize > 0.0
                 ? mAdjustedSize
                 : (mStyle.sizeAdjust == 0.0 ? 0.0 : mStyle.size);
    }

    float FUnitsToDevUnitsFactor() const {
        // check this was set up during font initialization
        NS_ASSERTION(mFUnitsConvFactor >= 0.0f, "mFUnitsConvFactor not valid");
        return mFUnitsConvFactor;
    }

    // check whether this is an sfnt we can potentially use with harfbuzz
    bool FontCanSupportHarfBuzz() {
        return mFontEntry->HasCmapTable();
    }

    // check whether this is an sfnt we can potentially use with Graphite
    bool FontCanSupportGraphite() {
        return mFontEntry->HasGraphiteTables();
    }

    // Whether this is a font that may be doing full-color rendering,
    // and therefore needs us to use a mask for text-shadow even when
    // we're not actually blurring.
    bool AlwaysNeedsMaskForShadow() {
        return mFontEntry->TryGetColorGlyphs() ||
               mFontEntry->TryGetSVGData(this) ||
               mFontEntry->HasFontTable(TRUETYPE_TAG('C','B','D','T')) ||
               mFontEntry->HasFontTable(TRUETYPE_TAG('s','b','i','x'));
    }

    // whether a feature is supported by the font (limited to a small set
    // of features for which some form of fallback needs to be implemented)
    bool SupportsFeature(Script aScript, uint32_t aFeatureTag);

    // whether the font supports "real" small caps, petite caps etc.
    // aFallbackToSmallCaps true when petite caps should fallback to small caps
    bool SupportsVariantCaps(Script aScript, uint32_t aVariantCaps,
                             bool& aFallbackToSmallCaps,
                             bool& aSyntheticLowerToSmallCaps,
                             bool& aSyntheticUpperToSmallCaps);

    // whether the font supports subscript/superscript feature
    // for fallback, need to verify that all characters in the run
    // have variant substitutions
    bool SupportsSubSuperscript(uint32_t aSubSuperscript,
                                const uint8_t *aString,
                                uint32_t aLength,
                                Script aRunScript);

    bool SupportsSubSuperscript(uint32_t aSubSuperscript,
                                const char16_t *aString,
                                uint32_t aLength,
                                Script aRunScript);

    // Subclasses may choose to look up glyph ids for characters.
    // If they do not override this, gfxHarfBuzzShaper will fetch the cmap
    // table and use that.
    virtual bool ProvidesGetGlyph() const {
        return false;
    }
    // Map unicode character to glyph ID.
    // Only used if ProvidesGetGlyph() returns true.
    virtual uint32_t GetGlyph(uint32_t unicode, uint32_t variation_selector) {
        return 0;
    }
    // Return the horizontal advance of a glyph.
    gfxFloat GetGlyphHAdvance(DrawTarget* aDrawTarget, uint16_t aGID);

    // Return Azure GlyphRenderingOptions for drawing this font.
    virtual already_AddRefed<mozilla::gfx::GlyphRenderingOptions>
      GetGlyphRenderingOptions(const TextRunDrawParams* aRunParams = nullptr)
    { return nullptr; }

    gfxFloat SynthesizeSpaceWidth(uint32_t aCh);

    // Font metrics
    struct Metrics {
        gfxFloat capHeight;
        gfxFloat xHeight;
        gfxFloat strikeoutSize;
        gfxFloat strikeoutOffset;
        gfxFloat underlineSize;
        gfxFloat underlineOffset;

        gfxFloat internalLeading;
        gfxFloat externalLeading;

        gfxFloat emHeight;
        gfxFloat emAscent;
        gfxFloat emDescent;
        gfxFloat maxHeight;
        gfxFloat maxAscent;
        gfxFloat maxDescent;
        gfxFloat maxAdvance;

        gfxFloat aveCharWidth;
        gfxFloat spaceWidth;
        gfxFloat zeroOrAveCharWidth;  // width of '0', or if there is
                                      // no '0' glyph in this font,
                                      // equal to .aveCharWidth
    };

    enum Orientation {
        eHorizontal,
        eVertical
    };

    const Metrics& GetMetrics(Orientation aOrientation)
    {
        if (aOrientation == eHorizontal) {
            return GetHorizontalMetrics();
        }
        if (!mVerticalMetrics) {
            mVerticalMetrics.reset(CreateVerticalMetrics());
        }
        return *mVerticalMetrics;
    }

    /**
     * We let layout specify spacing on either side of any
     * character. We need to specify both before and after
     * spacing so that substring measurement can do the right things.
     * These values are in appunits. They're always an integral number of
     * appunits, but we specify them in floats in case very large spacing
     * values are required.
     */
    struct Spacing {
        gfxFloat mBefore;
        gfxFloat mAfter;
    };
    /**
     * Metrics for a particular string
     */
    struct RunMetrics {
        RunMetrics() {
            mAdvanceWidth = mAscent = mDescent = 0.0;
        }

        void CombineWith(const RunMetrics& aOther, bool aOtherIsOnLeft);

        // can be negative (partly due to negative spacing).
        // Advance widths should be additive: the advance width of the
        // (offset1, length1) plus the advance width of (offset1 + length1,
        // length2) should be the advance width of (offset1, length1 + length2)
        gfxFloat mAdvanceWidth;
        
        // For zero-width substrings, these must be zero!
        gfxFloat mAscent;  // always non-negative
        gfxFloat mDescent; // always non-negative
        
        // Bounding box that is guaranteed to include everything drawn.
        // If a tight boundingBox was requested when these metrics were
        // generated, this will tightly wrap the glyphs, otherwise it is
        // "loose" and may be larger than the true bounding box.
        // Coordinates are relative to the baseline left origin, so typically
        // mBoundingBox.y == -mAscent
        gfxRect  mBoundingBox;
    };

    /**
     * Draw a series of glyphs to aContext. The direction of aTextRun must
     * be honoured.
     * @param aStart the first character to draw
     * @param aEnd draw characters up to here
     * @param aPt the baseline origin; the left end of the baseline
     * for LTR textruns, the right end for RTL textruns.
     * On return, this will be updated to the other end of the baseline.
     * In application units, really!
     * @param aRunParams record with drawing parameters, see TextRunDrawParams.
     * Particular fields of interest include
     * .spacing  spacing to insert before and after characters (for RTL
     *   glyphs, before-spacing is inserted to the right of characters). There
     *   are aEnd - aStart elements in this array, unless it's null to indicate
     *   that there is no spacing.
     * .drawMode  specifies whether the fill or stroke of the glyph should be
     *   drawn, or if it should be drawn into the current path
     * .contextPaint  information about how to construct the fill and
     *   stroke pattern. Can be nullptr if we are not stroking the text, which
     *   indicates that the current source from context should be used for fill
     * .context  the Thebes graphics context to which we're drawing
     * .dt  Moz2D DrawTarget to which we're drawing
     *
     * Callers guarantee:
     * -- aStart and aEnd are aligned to cluster and ligature boundaries
     * -- all glyphs use this font
     */
    void Draw(const gfxTextRun *aTextRun, uint32_t aStart, uint32_t aEnd,
              gfxPoint *aPt, const TextRunDrawParams& aRunParams,
              uint16_t aOrientation);

    /**
     * Draw the emphasis marks for the given text run. Its prerequisite
     * and output are similiar to the method Draw().
     * @param aPt the baseline origin of the emphasis marks.
     * @param aParams some drawing parameters, see EmphasisMarkDrawParams.
     */
    void DrawEmphasisMarks(const gfxTextRun* aShapedText, gfxPoint* aPt,
                           uint32_t aOffset, uint32_t aCount,
                           const EmphasisMarkDrawParams& aParams);

    /**
     * Measure a run of characters. See gfxTextRun::Metrics.
     * @param aTight if false, then return the union of the glyph extents
     * with the font-box for the characters (the rectangle with x=0,width=
     * the advance width for the character run,y=-(font ascent), and height=
     * font ascent + font descent). Otherwise, we must return as tight as possible
     * an approximation to the area actually painted by glyphs.
     * @param aDrawTargetForTightBoundingBox when aTight is true, this must
     * be non-null.
     * @param aSpacing spacing to insert before and after glyphs. The bounding box
     * need not include the spacing itself, but the spacing affects the glyph
     * positions. null if there is no spacing.
     * 
     * Callers guarantee:
     * -- aStart and aEnd are aligned to cluster and ligature boundaries
     * -- all glyphs use this font
     * 
     * The default implementation just uses font metrics and aTextRun's
     * advances, and assumes no characters fall outside the font box. In
     * general this is insufficient, because that assumption is not always true.
     */
    virtual RunMetrics Measure(const gfxTextRun *aTextRun,
                               uint32_t aStart, uint32_t aEnd,
                               BoundingBoxType aBoundingBoxType,
                               DrawTarget* aDrawTargetForTightBoundingBox,
                               Spacing *aSpacing, uint16_t aOrientation);
    /**
     * Line breaks have been changed at the beginning and/or end of a substring
     * of the text. Reshaping may be required; glyph updating is permitted.
     * @return true if anything was changed, false otherwise
     */
    bool NotifyLineBreaksChanged(gfxTextRun *aTextRun,
                                   uint32_t aStart, uint32_t aLength)
    { return false; }

    // Expiration tracking
    nsExpirationState *GetExpirationState() { return &mExpirationState; }

    // Get the glyphID of a space
    virtual uint32_t GetSpaceGlyph() = 0;

    gfxGlyphExtents *GetOrCreateGlyphExtents(int32_t aAppUnitsPerDevUnit);

    // You need to call SetupCairoFont on aDrawTarget just before calling this.
    virtual void SetupGlyphExtents(DrawTarget* aDrawTarget, uint32_t aGlyphID,
                                   bool aNeedTight, gfxGlyphExtents *aExtents);

    // This is called by the default Draw() implementation above.
    virtual bool SetupCairoFont(DrawTarget* aDrawTarget) = 0;

    virtual bool AllowSubpixelAA() { return true; }

    bool IsSyntheticBold() { return mApplySyntheticBold; }

    // Amount by which synthetic bold "fattens" the glyphs:
    // For size S up to a threshold size T, we use (0.25 + 3S / 4T),
    // so that the result ranges from 0.25 to 1.0; thereafter,
    // simply use (S / T).
    gfxFloat GetSyntheticBoldOffset() {
        gfxFloat size = GetAdjustedSize();
        const gfxFloat threshold = 48.0;
        return size < threshold ? (0.25 + 0.75 * size / threshold) :
                                  (size / threshold);
    }

    gfxFontEntry *GetFontEntry() const { return mFontEntry.get(); }
    bool HasCharacter(uint32_t ch) {
        if (!mIsValid ||
            (mUnicodeRangeMap && !mUnicodeRangeMap->test(ch))) {
            return false;
        }
        return mFontEntry->HasCharacter(ch); 
    }

    const gfxCharacterMap* GetUnicodeRangeMap() const {
        return mUnicodeRangeMap.get();
    }

    void SetUnicodeRangeMap(gfxCharacterMap* aUnicodeRangeMap) {
        mUnicodeRangeMap = aUnicodeRangeMap;
    }

    uint16_t GetUVSGlyph(uint32_t aCh, uint32_t aVS) {
        if (!mIsValid) {
            return 0;
        }
        return mFontEntry->GetUVSGlyph(aCh, aVS); 
    }

    template<typename T>
    bool InitFakeSmallCapsRun(DrawTarget *aDrawTarget,
                              gfxTextRun *aTextRun,
                              const T    *aText,
                              uint32_t    aOffset,
                              uint32_t    aLength,
                              uint8_t     aMatchType,
                              uint16_t    aOrientation,
                              Script      aScript,
                              bool        aSyntheticLower,
                              bool        aSyntheticUpper);

    // call the (virtual) InitTextRun method to do glyph generation/shaping,
    // limiting the length of text passed by processing the run in multiple
    // segments if necessary
    template<typename T>
    bool SplitAndInitTextRun(DrawTarget *aDrawTarget,
                             gfxTextRun *aTextRun,
                             const T *aString,
                             uint32_t aRunStart,
                             uint32_t aRunLength,
                             Script aRunScript,
                             bool aVertical);

    // Get a ShapedWord representing the given text (either 8- or 16-bit)
    // for use in setting up a gfxTextRun.
    template<typename T>
    gfxShapedWord* GetShapedWord(DrawTarget *aDrawTarget,
                                 const T *aText,
                                 uint32_t aLength,
                                 uint32_t aHash,
                                 Script aRunScript,
                                 bool aVertical,
                                 int32_t aAppUnitsPerDevUnit,
                                 uint32_t aFlags,
                                 gfxTextPerfMetrics *aTextPerf);

    // Ensure the ShapedWord cache is initialized. This MUST be called before
    // any attempt to use GetShapedWord().
    void InitWordCache() {
        if (!mWordCache) {
            mWordCache = mozilla::MakeUnique<nsTHashtable<CacheHashEntry>>();
        }
    }

    // Called by the gfxFontCache timer to increment the age of all the words,
    // so that they'll expire after a sufficient period of non-use
    void AgeCachedWords();

    // Discard all cached word records; called on memory-pressure notification.
    void ClearCachedWords() {
        if (mWordCache) {
            mWordCache->Clear();
        }
    }

    // Glyph rendering/geometry has changed, so invalidate data as necessary.
    void NotifyGlyphsChanged();

    virtual void AddSizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf,
                                        FontCacheSizes* aSizes) const;
    virtual void AddSizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf,
                                        FontCacheSizes* aSizes) const;

    typedef enum {
        FONT_TYPE_DWRITE,
        FONT_TYPE_GDI,
        FONT_TYPE_FT2,
        FONT_TYPE_MAC,
        FONT_TYPE_OS2,
        FONT_TYPE_CAIRO,
        FONT_TYPE_FONTCONFIG
    } FontType;

    virtual FontType GetType() const = 0;

    virtual already_AddRefed<mozilla::gfx::ScaledFont> GetScaledFont(DrawTarget* aTarget)
    { return gfxPlatform::GetPlatform()->GetScaledFontForFont(aTarget, this); }

    bool KerningDisabled() {
        return mKerningSet && !mKerningEnabled;
    }

    /**
     * Subclass this object to be notified of glyph changes. Delete the object
     * when no longer needed.
     */
    class GlyphChangeObserver {
    public:
        virtual ~GlyphChangeObserver()
        {
            if (mFont) {
                mFont->RemoveGlyphChangeObserver(this);
            }
        }
        // This gets called when the gfxFont dies.
        void ForgetFont() { mFont = nullptr; }
        virtual void NotifyGlyphsChanged() = 0;
    protected:
        explicit GlyphChangeObserver(gfxFont *aFont) : mFont(aFont)
        {
            mFont->AddGlyphChangeObserver(this);
        }
        // This pointer is nulled by ForgetFont in the gfxFont's
        // destructor. Before the gfxFont dies.
        gfxFont* MOZ_NON_OWNING_REF mFont;
    };
    friend class GlyphChangeObserver;

    bool GlyphsMayChange()
    {
        // Currently only fonts with SVG glyphs can have animated glyphs
        return mFontEntry->TryGetSVGData(this);
    }

    static void DestroySingletons() {
        delete sScriptTagToCode;
        delete sDefaultFeatures;
    }

    // Get a font dimension from the MATH table, scaled to appUnits;
    // may only be called if mFontEntry->TryGetMathTable has succeeded
    // (i.e. the font is known to be a valid OpenType math font).
    nscoord GetMathConstant(gfxFontEntry::MathConstant aConstant,
                            uint32_t aAppUnitsPerDevPixel)
    {
        return NSToCoordRound(mFontEntry->GetMathConstant(aConstant) *
                              GetAdjustedSize() * aAppUnitsPerDevPixel);
    }

    // Get a dimensionless math constant (e.g. a percentage);
    // may only be called if mFontEntry->TryGetMathTable has succeeded
    // (i.e. the font is known to be a valid OpenType math font).
    float GetMathConstant(gfxFontEntry::MathConstant aConstant)
    {
        return mFontEntry->GetMathConstant(aConstant);
    }

    // return a cloned font resized and offset to simulate sub/superscript glyphs
    virtual already_AddRefed<gfxFont>
    GetSubSuperscriptFont(int32_t aAppUnitsPerDevPixel);

    /**
     * Return the reference cairo_t object from aDT.
     */
    static cairo_t* RefCairo(mozilla::gfx::DrawTarget* aDT);

protected:
    virtual const Metrics& GetHorizontalMetrics() = 0;

    const Metrics* CreateVerticalMetrics();

    // Output a single glyph at *aPt, which is updated by the glyph's advance.
    // Normal glyphs are simply accumulated in aBuffer until it is full and
    // gets flushed, but SVG or color-font glyphs will instead be rendered
    // directly to the destination (found from the buffer's parameters).
    void DrawOneGlyph(uint32_t           aGlyphID,
                      double             aAdvance,
                      gfxPoint          *aPt,
                      GlyphBufferAzure&  aBuffer,
                      bool              *aEmittedGlyphs) const;

    // Output a run of glyphs at *aPt, which is updated to follow the last glyph
    // in the run. This method also takes account of any letter-spacing provided
    // in aRunParams.
    bool DrawGlyphs(const gfxShapedText      *aShapedText,
                    uint32_t                  aOffset, // offset in the textrun
                    uint32_t                  aCount, // length of run to draw
                    gfxPoint                 *aPt,
                    const TextRunDrawParams&  aRunParams,
                    const FontDrawParams&     aFontParams);

    // set the font size and offset used for
    // synthetic subscript/superscript glyphs
    void CalculateSubSuperSizeAndOffset(int32_t aAppUnitsPerDevPixel,
                                        gfxFloat& aSubSuperSizeRatio,
                                        float& aBaselineOffset);

    // Return a font that is a "clone" of this one, but reduced to 80% size
    // (and with variantCaps set to normal).
    // Default implementation relies on gfxFontEntry::CreateFontInstance;
    // backends that don't implement that will need to override this and use
    // an alternative technique. (gfxFontconfigFonts, I'm looking at you...)
    virtual already_AddRefed<gfxFont> GetSmallCapsFont();

    // subclasses may provide (possibly hinted) glyph widths (in font units);
    // if they do not override this, harfbuzz will use unhinted widths
    // derived from the font tables
    virtual bool ProvidesGlyphWidths() const {
        return false;
    }

    // The return value is interpreted as a horizontal advance in 16.16 fixed
    // point format.
    virtual int32_t GetGlyphWidth(DrawTarget& aDrawTarget, uint16_t aGID) {
        return -1;
    }

    bool IsSpaceGlyphInvisible(DrawTarget* aRefDrawTarget,
                               const gfxTextRun* aTextRun);

    void AddGlyphChangeObserver(GlyphChangeObserver *aObserver);
    void RemoveGlyphChangeObserver(GlyphChangeObserver *aObserver);

    // whether font contains substitution lookups containing spaces
    bool HasSubstitutionRulesWithSpaceLookups(Script aRunScript);

    // do spaces participate in shaping rules? if so, can't used word cache
    bool SpaceMayParticipateInShaping(Script aRunScript);

    // For 8-bit text, expand to 16-bit and then call the following method.
    bool ShapeText(DrawTarget    *aContext,
                   const uint8_t *aText,
                   uint32_t       aOffset, // dest offset in gfxShapedText
                   uint32_t       aLength,
                   Script         aScript,
                   bool           aVertical,
                   gfxShapedText *aShapedText); // where to store the result

    // Call the appropriate shaper to generate glyphs for aText and store
    // them into aShapedText.
    virtual bool ShapeText(DrawTarget      *aContext,
                           const char16_t *aText,
                           uint32_t         aOffset,
                           uint32_t         aLength,
                           Script           aScript,
                           bool             aVertical,
                           gfxShapedText   *aShapedText);

    // Helper to adjust for synthetic bold and set character-type flags
    // in the shaped text; implementations of ShapeText should call this
    // after glyph shaping has been completed.
    void PostShapingFixup(DrawTarget*     aContext,
                          const char16_t* aText,
                          uint32_t        aOffset, // position within aShapedText
                          uint32_t        aLength,
                          bool            aVertical,
                          gfxShapedText*  aShapedText);

    // Shape text directly into a range within a textrun, without using the
    // font's word cache. Intended for use when the font has layout features
    // that involve space, and therefore require shaping complete runs rather
    // than isolated words, or for long strings that are inefficient to cache.
    // This will split the text on "invalid" characters (tab/newline) that are
    // not handled via normal shaping, but does not otherwise divide up the
    // text.
    template<typename T>
    bool ShapeTextWithoutWordCache(DrawTarget *aDrawTarget,
                                   const T    *aText,
                                   uint32_t    aOffset,
                                   uint32_t    aLength,
                                   Script      aScript,
                                   bool        aVertical,
                                   gfxTextRun *aTextRun);

    // Shape a fragment of text (a run that is known to contain only
    // "valid" characters, no newlines/tabs/other control chars).
    // All non-wordcache shaping goes through here; this is the function
    // that will ensure we don't pass excessively long runs to the various
    // platform shapers.
    template<typename T>
    bool ShapeFragmentWithoutWordCache(DrawTarget *aDrawTarget,
                                       const T    *aText,
                                       uint32_t    aOffset,
                                       uint32_t    aLength,
                                       Script      aScript,
                                       bool        aVertical,
                                       gfxTextRun *aTextRun);

    void CheckForFeaturesInvolvingSpace();

    // whether a given feature is included in feature settings from both the
    // font and the style. aFeatureOn set if resolved feature value is non-zero
    bool HasFeatureSet(uint32_t aFeature, bool& aFeatureOn);

    // used when analyzing whether a font has space contextual lookups
    static nsDataHashtable<nsUint32HashKey,Script> *sScriptTagToCode;
    static nsTHashtable<nsUint32HashKey>           *sDefaultFeatures;

    RefPtr<gfxFontEntry> mFontEntry;

    struct CacheHashKey {
        union {
            const uint8_t   *mSingle;
            const char16_t *mDouble;
        }                mText;
        uint32_t         mLength;
        uint32_t         mFlags;
        Script           mScript;
        int32_t          mAppUnitsPerDevUnit;
        PLDHashNumber    mHashKey;
        bool             mTextIs8Bit;

        CacheHashKey(const uint8_t *aText, uint32_t aLength,
                     uint32_t aStringHash,
                     Script aScriptCode, int32_t aAppUnitsPerDevUnit,
                     uint32_t aFlags)
            : mLength(aLength),
              mFlags(aFlags),
              mScript(aScriptCode),
              mAppUnitsPerDevUnit(aAppUnitsPerDevUnit),
              mHashKey(aStringHash + static_cast<int32_t>(aScriptCode) +
                  aAppUnitsPerDevUnit * 0x100 + aFlags * 0x10000),
              mTextIs8Bit(true)
        {
            NS_ASSERTION(aFlags & gfxTextRunFactory::TEXT_IS_8BIT,
                         "8-bit flag should have been set");
            mText.mSingle = aText;
        }

        CacheHashKey(const char16_t *aText, uint32_t aLength,
                     uint32_t aStringHash,
                     Script aScriptCode, int32_t aAppUnitsPerDevUnit,
                     uint32_t aFlags)
            : mLength(aLength),
              mFlags(aFlags),
              mScript(aScriptCode),
              mAppUnitsPerDevUnit(aAppUnitsPerDevUnit),
              mHashKey(aStringHash + static_cast<int32_t>(aScriptCode) +
                  aAppUnitsPerDevUnit * 0x100 + aFlags * 0x10000),
              mTextIs8Bit(false)
        {
            // We can NOT assert that TEXT_IS_8BIT is false in aFlags here,
            // because this might be an 8bit-only word from a 16-bit textrun,
            // in which case the text we're passed is still in 16-bit form,
            // and we'll have to use an 8-to-16bit comparison in KeyEquals.
            mText.mDouble = aText;
        }
    };

    class CacheHashEntry : public PLDHashEntryHdr {
    public:
        typedef const CacheHashKey &KeyType;
        typedef const CacheHashKey *KeyTypePointer;

        // When constructing a new entry in the hashtable, the caller of Put()
        // will fill us in.
        explicit CacheHashEntry(KeyTypePointer aKey) { }
        CacheHashEntry(const CacheHashEntry& toCopy) { NS_ERROR("Should not be called"); }
        ~CacheHashEntry() { }

        bool KeyEquals(const KeyTypePointer aKey) const;

        static KeyTypePointer KeyToPointer(KeyType aKey) { return &aKey; }

        static PLDHashNumber HashKey(const KeyTypePointer aKey) {
            return aKey->mHashKey;
        }

        size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
        {
            return aMallocSizeOf(mShapedWord.get());
        }

        enum { ALLOW_MEMMOVE = true };

        mozilla::UniquePtr<gfxShapedWord> mShapedWord;
    };

    mozilla::UniquePtr<nsTHashtable<CacheHashEntry> > mWordCache;

    static const uint32_t  kShapedWordCacheMaxAge = 3;

    bool                       mIsValid;

    // use synthetic bolding for environments where this is not supported
    // by the platform
    bool                       mApplySyntheticBold;

    bool                       mKerningSet;     // kerning explicitly set?
    bool                       mKerningEnabled; // if set, on or off?

    nsExpirationState          mExpirationState;
    gfxFontStyle               mStyle;
    nsTArray<mozilla::UniquePtr<gfxGlyphExtents>> mGlyphExtentsArray;
    mozilla::UniquePtr<nsTHashtable<nsPtrHashKey<GlyphChangeObserver>>>
                               mGlyphChangeObservers;

    gfxFloat                   mAdjustedSize;

    // Conversion factor from font units to dev units; note that this may be
    // zero (in the degenerate case where mAdjustedSize has become zero).
    // This is OK because we only multiply by this factor, never divide.
    float                      mFUnitsConvFactor;

    // the AA setting requested for this font - may affect glyph bounds
    AntialiasOption            mAntialiasOption;

    // a copy of the font without antialiasing, if needed for separate
    // measurement by mathml code
    mozilla::UniquePtr<gfxFont>         mNonAAFont;

    // we create either or both of these shapers when needed, depending
    // whether the font has graphite tables, and whether graphite shaping
    // is actually enabled
    mozilla::UniquePtr<gfxFontShaper>   mHarfBuzzShaper;
    mozilla::UniquePtr<gfxFontShaper>   mGraphiteShaper;

    // if a userfont with unicode-range specified, contains map of *possible*
    // ranges supported by font
    RefPtr<gfxCharacterMap> mUnicodeRangeMap;

    RefPtr<mozilla::gfx::ScaledFont> mAzureScaledFont;

    // For vertical metrics, created on demand.
    mozilla::UniquePtr<const Metrics> mVerticalMetrics;

    // Helper for subclasses that want to initialize standard metrics from the
    // tables of sfnt (TrueType/OpenType) fonts.
    // This will use mFUnitsConvFactor if it is already set, else compute it
    // from mAdjustedSize and the unitsPerEm in the font's 'head' table.
    // Returns TRUE and sets mIsValid=TRUE if successful;
    // Returns TRUE but leaves mIsValid=FALSE if the font seems to be broken.
    // Returns FALSE if the font does not appear to be an sfnt at all,
    // and should be handled (if possible) using other APIs.
    bool InitMetricsFromSfntTables(Metrics& aMetrics);

    // Helper to calculate various derived metrics from the results of
    // InitMetricsFromSfntTables or equivalent platform code
    void CalculateDerivedMetrics(Metrics& aMetrics);

    // some fonts have bad metrics, this method sanitize them.
    // if this font has bad underline offset, aIsBadUnderlineFont should be true.
    void SanitizeMetrics(Metrics *aMetrics, bool aIsBadUnderlineFont);

    bool RenderSVGGlyph(gfxContext *aContext, gfxPoint aPoint,
                        uint32_t aGlyphId, SVGContextPaint* aContextPaint) const;
    bool RenderSVGGlyph(gfxContext *aContext, gfxPoint aPoint,
                        uint32_t aGlyphId, SVGContextPaint* aContextPaint,
                        gfxTextRunDrawCallbacks *aCallbacks,
                        bool& aEmittedGlyphs) const;

    bool RenderColorGlyph(DrawTarget* aDrawTarget,
                          gfxContext* aContext,
                          mozilla::gfx::ScaledFont* scaledFont,
                          mozilla::gfx::GlyphRenderingOptions* renderingOptions,
                          mozilla::gfx::DrawOptions drawOptions,
                          const mozilla::gfx::Point& aPoint,
                          uint32_t aGlyphId) const;

    // Bug 674909. When synthetic bolding text by drawing twice, need to
    // render using a pixel offset in device pixels, otherwise text
    // doesn't appear bolded, it appears as if a bad text shadow exists
    // when a non-identity transform exists.  Use an offset factor so that
    // the second draw occurs at a constant offset in device pixels.
    // This helper calculates the scale factor we need to apply to the
    // synthetic-bold offset.
    static double CalcXScale(DrawTarget* aDrawTarget);
};

// proportion of ascent used for x-height, if unable to read value from font
#define DEFAULT_XHEIGHT_FACTOR 0.56f

// Parameters passed to gfxFont methods for drawing glyphs from a textrun.
// The TextRunDrawParams are set up once per textrun; the FontDrawParams
// are dependent on the specific font, so they are set per GlyphRun.

struct TextRunDrawParams {
    RefPtr<mozilla::gfx::DrawTarget> dt;
    gfxContext              *context;
    gfxFont::Spacing        *spacing;
    gfxTextRunDrawCallbacks *callbacks;
    mozilla::SVGContextPaint *runContextPaint;
    mozilla::gfx::Color      fontSmoothingBGColor;
    gfxFloat                 direction;
    double                   devPerApp;
    nscolor                  textStrokeColor;
    gfxPattern              *textStrokePattern;
    const mozilla::gfx::StrokeOptions *strokeOpts;
    const mozilla::gfx::DrawOptions   *drawOpts;
    DrawMode                 drawMode;
    bool                     isVerticalRun;
    bool                     isRTL;
    bool                     paintSVGGlyphs;
};

struct FontDrawParams {
    RefPtr<mozilla::gfx::ScaledFont>            scaledFont;
    RefPtr<mozilla::gfx::GlyphRenderingOptions> renderingOptions;
    mozilla::SVGContextPaint *contextPaint;
    mozilla::gfx::Matrix     *passedInvMatrix;
    mozilla::gfx::Matrix      matInv;
    double                    synBoldOnePixelOffset;
    int32_t                   extraStrikes;
    mozilla::gfx::DrawOptions drawOptions;
    bool                      isVerticalFont;
    bool                      haveSVGGlyphs;
    bool                      haveColorGlyphs;
};

struct EmphasisMarkDrawParams {
    gfxContext* context;
    gfxFont::Spacing* spacing;
    gfxTextRun* mark;
    gfxFloat advance;
    gfxFloat direction;
    bool isVertical;
};

#endif
