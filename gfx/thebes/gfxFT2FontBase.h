/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_FT2FONTBASE_H
#define GFX_FT2FONTBASE_H

#include "gfxContext.h"
#include "gfxFont.h"
#include "gfxFontEntry.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/UnscaledFontFreeType.h"
#include "nsTHashMap.h"
#include "nsHashKeys.h"

class gfxFT2FontEntryBase : public gfxFontEntry {
 public:
  explicit gfxFT2FontEntryBase(const nsACString& aName) : gfxFontEntry(aName) {}

  struct CmapCacheSlot {
    CmapCacheSlot() : mCharCode(0), mGlyphIndex(0) {}

    uint32_t mCharCode;
    uint32_t mGlyphIndex;
  };

  CmapCacheSlot* GetCmapCacheSlot(uint32_t aCharCode);

  static bool FaceHasTable(mozilla::gfx::SharedFTFace*, uint32_t aTableTag);
  static nsresult CopyFaceTable(mozilla::gfx::SharedFTFace*, uint32_t aTableTag,
                                nsTArray<uint8_t>&);

 private:
  enum { kNumCmapCacheSlots = 256 };

  mozilla::UniquePtr<CmapCacheSlot[]> mCmapCache;
};

class gfxFT2FontBase : public gfxFont {
 public:
  gfxFT2FontBase(
      const RefPtr<mozilla::gfx::UnscaledFontFreeType>& aUnscaledFont,
      RefPtr<mozilla::gfx::SharedFTFace>&& aFTFace, gfxFontEntry* aFontEntry,
      const gfxFontStyle* aFontStyle, int aLoadFlags, bool aEmbolden);
  virtual ~gfxFT2FontBase();

  uint32_t GetGlyph(uint32_t aCharCode);
  bool ProvidesGetGlyph() const override { return true; }
  virtual uint32_t GetGlyph(uint32_t unicode,
                            uint32_t variation_selector) override;
  bool ProvidesGlyphWidths() const override { return true; }
  int32_t GetGlyphWidth(uint16_t aGID) override;
  bool GetGlyphBounds(uint16_t aGID, gfxRect* aBounds, bool aTight) override;

  FontType GetType() const override { return FONT_TYPE_FT2; }

  bool ShouldRoundXOffset(cairo_t* aCairo) const override;

  static void SetupVarCoords(FT_MM_Var* aMMVar,
                             const nsTArray<gfxFontVariation>& aVariations,
                             FT_Face aFTFace);

  FT_Face LockFTFace();
  void UnlockFTFace();

 private:
  uint32_t GetCharExtents(char aChar, gfxFloat* aWidth,
                          gfxRect* aBounds = nullptr);

  // Get advance (and optionally bounds) of a single glyph from FreeType,
  // and return true, or return false if we failed.
  bool GetFTGlyphExtents(uint16_t aGID, int32_t* aWidth,
                         mozilla::gfx::IntRect* aBounds = nullptr);

 protected:
  void InitMetrics();
  const Metrics& GetHorizontalMetrics() override;
  FT_Vector GetEmboldenStrength(FT_Face aFace);

  RefPtr<mozilla::gfx::SharedFTFace> mFTFace;

  Metrics mMetrics;
  int mFTLoadFlags;
  bool mEmbolden;
  gfxFloat mFTSize;

  // For variation/multiple-master fonts, this will be an array of the values
  // for each axis, as specified by mStyle.variationSettings (or the font's
  // default for axes not present in variationSettings). Values here are in
  // freetype's 16.16 fixed-point format, and clamped to the valid min/max
  // range reported by the face.
  nsTArray<FT_Fixed> mCoords;

  // Store cached glyph metrics for reasonably small glyph sizes. The bounds
  // are stored unscaled to losslessly compress 26.6 fixed point to an int16_t.
  // Larger glyphs are handled directly via GetFTGlyphExtents.
  struct GlyphMetrics {
    // Set the X coord to INT16_MIN to signal the bounds are invalid, or
    // INT16_MAX to signal that the bounds would overflow an int16_t.
    enum { INVALID = INT16_MIN, LARGE = INT16_MAX };

    GlyphMetrics() : mAdvance(0), mX(INVALID), mY(0), mWidth(0), mHeight(0) {}

    bool HasValidBounds() const { return mX != INVALID; }
    bool HasCachedBounds() const { return mX != LARGE; }

    // If the bounds can fit in an int16_t, set them. Otherwise, leave the
    // bounds invalid to signal that GetFTGlyphExtents should be queried
    // directly.
    void SetBounds(const mozilla::gfx::IntRect& aBounds) {
      if (aBounds.x > INT16_MIN && aBounds.x < INT16_MAX &&
          aBounds.y > INT16_MIN && aBounds.y < INT16_MAX &&
          aBounds.width <= UINT16_MAX && aBounds.height <= UINT16_MAX) {
        mX = aBounds.x;
        mY = aBounds.y;
        mWidth = aBounds.width;
        mHeight = aBounds.height;
      } else {
        mX = LARGE;
      }
    }

    mozilla::gfx::IntRect GetBounds() const {
      return mozilla::gfx::IntRect(mX, mY, mWidth, mHeight);
    }

    int32_t mAdvance;
    int16_t mX;
    int16_t mY;
    uint16_t mWidth;
    uint16_t mHeight;
  };

  const GlyphMetrics& GetCachedGlyphMetrics(
      uint16_t aGID, mozilla::gfx::IntRect* aBounds = nullptr);

  mozilla::UniquePtr<nsTHashMap<nsUint32HashKey, GlyphMetrics>> mGlyphMetrics;
};

// Helper classes used for clearing out user font data when FT font
// face is destroyed. Since multiple faces may use the same data, be
// careful to assure that the data is only cleared out when all uses
// expire. The font entry object contains a refptr to FTUserFontData and
// each FT face created from that font entry contains a refptr to that
// same FTUserFontData object.

class FTUserFontData final
    : public mozilla::gfx::SharedFTFaceRefCountedData<FTUserFontData> {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(FTUserFontData)

  FTUserFontData(const uint8_t* aData, uint32_t aLength)
      : mFontData(aData), mLength(aLength) {}

  const uint8_t* FontData() const { return mFontData; }

  already_AddRefed<mozilla::gfx::SharedFTFace> CloneFace(
      int aFaceIndex = 0) override;

 private:
  ~FTUserFontData() {
    if (mFontData) {
      free((void*)mFontData);
    }
  }

  const uint8_t* mFontData;
  uint32_t mLength;
};

#endif /* GFX_FT2FONTBASE_H */
