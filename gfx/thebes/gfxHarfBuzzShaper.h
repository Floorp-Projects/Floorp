/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_HARFBUZZSHAPER_H
#define GFX_HARFBUZZSHAPER_H

#include "gfxFont.h"

#include "harfbuzz/hb.h"
#include "nsUnicodeProperties.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/MruCache.h"
#include "mozilla/Mutex.h"

class gfxHarfBuzzShaper : public gfxFontShaper {
 public:
  explicit gfxHarfBuzzShaper(gfxFont* aFont);
  virtual ~gfxHarfBuzzShaper();

  /*
   * For HarfBuzz font callback functions, font_data is a ptr to a
   * FontCallbackData struct
   */
  struct FontCallbackData {
    gfxHarfBuzzShaper* mShaper;
  };

  // Initializes the shaper and returns whether this was successful.
  bool Initialize();

  // Returns whether the shaper has been successfully initialized.
  bool IsInitialized() const { return mHBFont != nullptr; }

  bool ShapeText(DrawTarget* aDrawTarget, const char16_t* aText,
                 uint32_t aOffset, uint32_t aLength, Script aScript,
                 nsAtom* aLanguage, bool aVertical, RoundingFlags aRounding,
                 gfxShapedText* aShapedText) override;

  // get a given font table in harfbuzz blob form
  hb_blob_t* GetFontTable(hb_tag_t aTag) const;

  // map unicode character to glyph ID
  hb_codepoint_t GetNominalGlyph(hb_codepoint_t unicode) const;
  unsigned int GetNominalGlyphs(unsigned int count,
                                const hb_codepoint_t* first_unicode,
                                unsigned int unicode_stride,
                                hb_codepoint_t* first_glyph,
                                unsigned int glyph_stride);
  hb_codepoint_t GetVariationGlyph(hb_codepoint_t unicode,
                                   hb_codepoint_t variation_selector) const;

  // get harfbuzz glyph advance, in font design units
  hb_position_t GetGlyphHAdvance(hb_codepoint_t glyph) const;
  void GetGlyphHAdvances(unsigned int count, const hb_codepoint_t* first_glyph,
                         unsigned int glyph_stride,
                         hb_position_t* first_advance,
                         unsigned int advance_stride) const;

  // Get vertical glyph advance, or -1 if not available; caller should check
  // for a negative result and provide a fallback or fail, as appropriate.
  hb_position_t GetGlyphVAdvance(hb_codepoint_t glyph);

  void GetGlyphVOrigin(hb_codepoint_t aGlyph, hb_position_t* aX,
                       hb_position_t* aY) const;

  hb_position_t GetHKerning(uint16_t aFirstGlyph, uint16_t aSecondGlyph) const;

  hb_bool_t GetGlyphExtents(hb_codepoint_t aGlyph,
                            hb_glyph_extents_t* aExtents) const;

  bool UseVerticalPresentationForms() const {
    return mUseVerticalPresentationForms;
  }

  static hb_script_t GetHBScriptUsedForShaping(Script aScript) {
    // Decide what harfbuzz script code will be used for shaping
    hb_script_t hbScript;
    if (aScript <= Script::INHERITED) {
      // For unresolved "common" or "inherited" runs,
      // default to Latin for now.
      hbScript = HB_SCRIPT_LATIN;
    } else {
      hbScript = hb_script_t(mozilla::unicode::GetScriptTagForCode(aScript));
    }
    return hbScript;
  }

  static hb_codepoint_t GetVerticalPresentationForm(hb_codepoint_t aUnicode);

  // Create an hb_font corresponding to the given gfxFont instance, with size
  // and variations set appropriately. If aFontFuncs and aCallbackData are
  // provided, they may be used as harfbuzz font callbacks for advances, glyph
  // bounds, etc; if not, the built-in hb_ot font functions will be used.
  static hb_font_t* CreateHBFont(gfxFont* aFont,
                                 hb_font_funcs_t* aFontFuncs = nullptr,
                                 FontCallbackData* aCallbackData = nullptr);

  hb_font_t* GetHBFont() const { return mHBFont; }
  hb_face_t* GetHBFace() const { return hb_font_get_face(mHBFont); }

 protected:
  // This is called with the cache locked, but if mUseFontGetGlyph is true, it
  // may unlock it temporarily. So in this case, it may invalidate an earlier
  // cache entry reference.
  hb_codepoint_t GetGlyphUncached(hb_codepoint_t unicode) const
      MOZ_REQUIRES(mCacheLock);

  hb_position_t GetGlyphHAdvanceUncached(hb_codepoint_t gid) const;

  nsresult SetGlyphsFromRun(gfxShapedText* aShapedText, uint32_t aOffset,
                            uint32_t aLength, const char16_t* aText,
                            bool aVertical, RoundingFlags aRounding);

  // retrieve glyph positions, applying advance adjustments and attachments
  // returns results in appUnits
  nscoord GetGlyphPositions(gfxContext* aContext, nsTArray<nsPoint>& aPositions,
                            uint32_t aAppUnitsPerDevUnit);

  void InitializeVertical();
  bool LoadHmtxTable();

  struct Glyf {  // we only need the bounding-box at the beginning
                 // of the glyph record, not the actual outline data
    mozilla::AutoSwap_PRInt16 numberOfContours;
    mozilla::AutoSwap_PRInt16 xMin;
    mozilla::AutoSwap_PRInt16 yMin;
    mozilla::AutoSwap_PRInt16 xMax;
    mozilla::AutoSwap_PRInt16 yMax;
  };

  const Glyf* FindGlyf(hb_codepoint_t aGlyph, bool* aEmptyGlyf) const;

  // size-specific font object, owned by the gfxHarfBuzzShaper
  hb_font_t* mHBFont;

  // harfbuzz buffer for the shaping process
  hb_buffer_t* mBuffer;

  mutable mozilla::Mutex mCacheLock = mozilla::Mutex("shaperCacheMutex");

  struct CmapCacheData {
    uint32_t mCodepoint;
    uint32_t mGlyphId;
  };

  struct CmapCache
      : public mozilla::MruCache<uint32_t, CmapCacheData, CmapCache, 251> {
    static mozilla::HashNumber Hash(const uint32_t& aKey) { return aKey; }
    static bool Match(const uint32_t& aKey, const CmapCacheData& aData) {
      return aKey == aData.mCodepoint;
    }
  };

  mutable mozilla::UniquePtr<CmapCache> mCmapCache MOZ_GUARDED_BY(mCacheLock);

  struct WidthCacheData {
    hb_codepoint_t mGlyphId;
    hb_position_t mAdvance;
  };

  struct WidthCache
      : public mozilla::MruCache<uint32_t, WidthCacheData, WidthCache, 251> {
    static mozilla::HashNumber Hash(const hb_codepoint_t& aKey) { return aKey; }
    static bool Match(const uint32_t& aKey, const WidthCacheData& aData) {
      return aKey == aData.mGlyphId;
    }
  };

  mutable mozilla::UniquePtr<WidthCache> mWidthCache MOZ_GUARDED_BY(mCacheLock);

  FontCallbackData mCallbackData;

  // Following table references etc are declared "mutable" because the
  // harfbuzz callback functions take a const ptr to the shaper, but
  // wish to cache tables here to avoid repeatedly looking them up
  // in the font.

  // Old-style TrueType kern table, if we're not doing GPOS kerning
  mutable hb_blob_t* mKernTable;

  // Cached copy of the hmtx table.
  mutable hb_blob_t* mHmtxTable;

  // For vertical fonts, cached vmtx and VORG table, if present.
  mutable hb_blob_t* mVmtxTable;
  mutable hb_blob_t* mVORGTable;
  // And for vertical TrueType (not CFF) fonts that have vmtx,
  // we also use loca and glyf to get glyph bounding boxes.
  mutable hb_blob_t* mLocaTable;
  mutable hb_blob_t* mGlyfTable;

  // Cached pointer to cmap subtable to be used for char-to-glyph mapping.
  // This comes from GetFontTablePtr; if it is non-null, our destructor
  // must call ReleaseFontTablePtr to avoid permanently caching the table.
  mutable hb_blob_t* mCmapTable;
  mutable int32_t mCmapFormat;
  mutable uint32_t mSubtableOffset;
  mutable uint32_t mUVSTableOffset;

  // Cached copy of numLongMetrics field from the hhea table,
  // for use when looking up glyph metrics; initialized to 0 by the
  // constructor so we can tell it hasn't been set yet.
  // This is a signed value so that we can use -1 to indicate
  // an error (if the hhea table was not available).
  mutable int32_t mNumLongHMetrics;
  // Similarly for vhea if it's a vertical font.
  mutable int32_t mNumLongVMetrics;

  // Default y-coordinate for glyph vertical origin, used if the font
  // does not actually have vertical-layout metrics.
  mutable gfxFloat mDefaultVOrg;

  // Whether the font implements GetGlyph, or we should read tables
  // directly
  bool mUseFontGetGlyph;

  // Whether the font is an MS Symbol-encoded font, in which case we will
  // try remapping U+0020..00FF to U+F020..F0FF for characters in the U+00xx
  // range that are otherwise unsupported.
  bool mIsSymbolFont;

  // Whether the font implements GetGlyphWidth, or we should read tables
  // directly to get ideal widths
  bool mUseFontGlyphWidths;

  bool mInitialized;
  bool mVerticalInitialized;

  // Whether to use vertical presentation forms for CJK characters
  // when available (only set if the 'vert' feature is not available).
  bool mUseVerticalPresentationForms;

  // these are set from the FindGlyf callback on first use of the glyf data
  mutable bool mLoadedLocaGlyf;
  mutable bool mLocaLongOffsets;
};

#endif /* GFX_HARFBUZZSHAPER_H */
