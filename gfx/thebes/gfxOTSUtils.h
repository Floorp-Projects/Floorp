/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_OTS_UTILS_H
#define GFX_OTS_UTILS_H

#include "gfxFontUtils.h"

#include "opentype-sanitiser.h"

struct gfxOTSMozAlloc {
  void* Grow(void* aPtr, size_t aLength) { return moz_xrealloc(aPtr, aLength); }
  void* ShrinkToFit(void* aPtr, size_t aLength) {
    return moz_xrealloc(aPtr, aLength);
  }
  void Free(void* aPtr) { free(aPtr); }
};

// Based on ots::ExpandingMemoryStream from ots-memory-stream.h,
// adapted to use Mozilla allocators and to allow the final
// memory buffer to be adopted by the client.
template <typename AllocT = gfxOTSMozAlloc>
class gfxOTSExpandingMemoryStream : public ots::OTSStream {
 public:
  // limit output/expansion to 256MB by default
  enum { DEFAULT_LIMIT = 256 * 1024 * 1024 };

  explicit gfxOTSExpandingMemoryStream(size_t initial,
                                       size_t limit = DEFAULT_LIMIT)
      : mLength(initial), mLimit(limit), mOff(0) {
    mPtr = mAlloc.Grow(nullptr, mLength);
  }

  ~gfxOTSExpandingMemoryStream() { mAlloc.Free(mPtr); }

  size_t size() override { return mLimit; }

  // Return the buffer, resized to fit its contents (as it may have been
  // over-allocated during growth), and give up ownership of it so the
  // caller becomes responsible to call free() when finished with it.
  auto forget() {
    auto p = mAlloc.ShrinkToFit(mPtr, mOff);
    mPtr = nullptr;
    return p;
  }

  bool WriteRaw(const void* data, size_t length) override {
    if ((mOff + length > mLength) ||
        (mLength > std::numeric_limits<size_t>::max() - mOff)) {
      if (mLength == mLimit) {
        return false;
      }
      size_t newLength = (mLength + 1) * 2;
      if (newLength < mLength) {
        return false;
      }
      if (newLength > mLimit) {
        newLength = mLimit;
      }
      mPtr = mAlloc.Grow(mPtr, newLength);
      mLength = newLength;
      return WriteRaw(data, length);
    }
    std::memcpy(static_cast<char*>(mPtr) + mOff, data, length);
    mOff += length;
    return true;
  }

  bool Seek(off_t position) override {
    if (position < 0) {
      return false;
    }
    if (static_cast<size_t>(position) > mLength) {
      return false;
    }
    mOff = position;
    return true;
  }

  off_t Tell() const override { return mOff; }

 private:
  AllocT mAlloc;
  void* mPtr;
  size_t mLength;
  const size_t mLimit;
  off_t mOff;
};

class MOZ_STACK_CLASS gfxOTSContext : public ots::OTSContext {
 public:
  gfxOTSContext() {
    using namespace mozilla;

    // Whether to apply OTS validation to OpenType Layout tables
    mCheckOTLTables = StaticPrefs::gfx_downloadable_fonts_otl_validation();
    // Whether to preserve Variation tables in downloaded fonts
    mCheckVariationTables =
        StaticPrefs::gfx_downloadable_fonts_validate_variation_tables();
    // Whether to preserve color bitmap glyphs
    mKeepColorBitmaps =
        StaticPrefs::gfx_downloadable_fonts_keep_color_bitmaps();
  }

  virtual ots::TableAction GetTableAction(uint32_t aTag) override {
    // Preserve Graphite, color glyph and SVG tables,
    // and possibly OTL and Variation tables (depending on prefs)
    if ((!mCheckOTLTables && (aTag == TRUETYPE_TAG('G', 'D', 'E', 'F') ||
                              aTag == TRUETYPE_TAG('G', 'P', 'O', 'S') ||
                              aTag == TRUETYPE_TAG('G', 'S', 'U', 'B')))) {
      return ots::TABLE_ACTION_PASSTHRU;
    }
    if (aTag == TRUETYPE_TAG('S', 'V', 'G', ' ') ||
        aTag == TRUETYPE_TAG('C', 'O', 'L', 'R') ||
        aTag == TRUETYPE_TAG('C', 'P', 'A', 'L')) {
      return ots::TABLE_ACTION_PASSTHRU;
    }
    if (mKeepColorBitmaps && (aTag == TRUETYPE_TAG('C', 'B', 'D', 'T') ||
                              aTag == TRUETYPE_TAG('C', 'B', 'L', 'C'))) {
      return ots::TABLE_ACTION_PASSTHRU;
    }
    auto isVariationTable = [](uint32_t aTag) -> bool {
      return aTag == TRUETYPE_TAG('a', 'v', 'a', 'r') ||
             aTag == TRUETYPE_TAG('c', 'v', 'a', 'r') ||
             aTag == TRUETYPE_TAG('f', 'v', 'a', 'r') ||
             aTag == TRUETYPE_TAG('g', 'v', 'a', 'r') ||
             aTag == TRUETYPE_TAG('H', 'V', 'A', 'R') ||
             aTag == TRUETYPE_TAG('M', 'V', 'A', 'R') ||
             aTag == TRUETYPE_TAG('S', 'T', 'A', 'T') ||
             aTag == TRUETYPE_TAG('V', 'V', 'A', 'R');
    };
    if (!mCheckVariationTables && isVariationTable(aTag)) {
      return ots::TABLE_ACTION_PASSTHRU;
    }
    if (!gfxPlatform::HasVariationFontSupport() && isVariationTable(aTag)) {
      return ots::TABLE_ACTION_DROP;
    }
    return ots::TABLE_ACTION_DEFAULT;
  }

  static size_t GuessSanitizedFontSize(size_t aLength,
                                       gfxUserFontType aFontType,
                                       bool aStrict = true) {
    switch (aFontType) {
      case GFX_USERFONT_UNKNOWN:
        // If being permissive of unknown types, make a reasonable guess
        // at how much room the sanitized font may take, if it passes. Just
        // enough extra space to accomodate some growth without excessive
        // bloat in case of large fonts. 1.5x is a reasonable compromise
        // for growable vectors in general.
        return aStrict || !aLength ? 0 : (aLength * 3) / 2;
      case GFX_USERFONT_WOFF:
        return aLength * 2;
      case GFX_USERFONT_WOFF2:
        return aLength * 3;
      default:
        return aLength;
    }
  }

  static size_t GuessSanitizedFontSize(const uint8_t* aData, size_t aLength,
                                       bool aStrict = true) {
    gfxUserFontType fontType =
        gfxFontUtils::DetermineFontDataType(aData, aLength);
    return GuessSanitizedFontSize(aLength, fontType, aStrict);
  }

 private:
  bool mCheckOTLTables;
  bool mCheckVariationTables;
  bool mKeepColorBitmaps;
};

#endif /* GFX_OTS_UTILS_H */
