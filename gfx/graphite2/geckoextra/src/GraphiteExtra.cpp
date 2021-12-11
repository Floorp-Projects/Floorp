/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "graphite2/Font.h"
#include "graphite2/Segment.h"
#include "graphite2/GraphiteExtra.h"

#include <stdlib.h>
#include <memory>
#include <limits>

#define CHECK(cond, str) \
  do {                   \
    if (!(cond)) {       \
      return false;      \
    }                    \
  } while (false)

// High surrogates are in the range 0xD800 -- OxDBFF
#define NS_IS_HIGH_SURROGATE(u) ((uint32_t(u) & 0xFFFFFC00) == 0xD800)
// Low surrogates are in the range 0xDC00 -- 0xDFFF
#define NS_IS_LOW_SURROGATE(u) ((uint32_t(u) & 0xFFFFFC00) == 0xDC00)

#define IS_POWER_OF_2(n) (n & (n - 1)) == 0

typedef gr_glyph_to_char_cluster Cluster;

// returns whether successful
static bool LoopThrough(gr_segment* aSegment, const uint32_t aLength,
                        const uint32_t aGlyphCount, const char16_t* aText,
                        uint32_t& aCIndex, Cluster* aClusters, uint16_t* aGids,
                        float* aXLocs, float* aYLocs) {
  // walk through the glyph slots and check which original character
  // each is associated with
  uint32_t gIndex = 0;  // glyph slot index
  for (const gr_slot* slot = gr_seg_first_slot(aSegment); slot != nullptr;
       slot = gr_slot_next_in_segment(slot), gIndex++) {
    CHECK(gIndex < aGlyphCount, "iterating past glyphcount");
    uint32_t before =
        gr_cinfo_base(gr_seg_cinfo(aSegment, gr_slot_before(slot)));
    uint32_t after = gr_cinfo_base(gr_seg_cinfo(aSegment, gr_slot_after(slot)));
    aGids[gIndex] = gr_slot_gid(slot);
    aXLocs[gIndex] = gr_slot_origin_X(slot);
    aYLocs[gIndex] = gr_slot_origin_Y(slot);

    // if this glyph has a "before" character index that precedes the
    // current cluster's char index, we need to merge preceding
    // aClusters until it gets included
    while (before < aClusters[aCIndex].baseChar && aCIndex > 0) {
      aClusters[aCIndex - 1].nChars += aClusters[aCIndex].nChars;
      aClusters[aCIndex - 1].nGlyphs += aClusters[aCIndex].nGlyphs;
      --aCIndex;
    }

    // if there's a gap between the current cluster's base character and
    // this glyph's, extend the cluster to include the intervening chars
    if (gr_slot_can_insert_before(slot) && aClusters[aCIndex].nChars &&
        before >= aClusters[aCIndex].baseChar + aClusters[aCIndex].nChars) {
      CHECK(aCIndex < aLength - 1, "aCIndex at end of word");
      Cluster& c = aClusters[aCIndex + 1];
      c.baseChar = aClusters[aCIndex].baseChar + aClusters[aCIndex].nChars;
      c.nChars = before - c.baseChar;
      c.baseGlyph = gIndex;
      c.nGlyphs = 0;
      ++aCIndex;
    }

    // increment cluster's glyph count to include current slot
    CHECK(aCIndex < aLength, "aCIndex beyond word length");
    ++aClusters[aCIndex].nGlyphs;

    // bump |after| index if it falls in the middle of a surrogate pair
    if (NS_IS_HIGH_SURROGATE(aText[after]) && after < aLength - 1 &&
        NS_IS_LOW_SURROGATE(aText[after + 1])) {
      after++;
    }

    // extend cluster if necessary to reach the glyph's "after" index
    if (aClusters[aCIndex].baseChar + aClusters[aCIndex].nChars < after + 1) {
      aClusters[aCIndex].nChars = after + 1 - aClusters[aCIndex].baseChar;
    }
  }

  // Succeeded
  return true;
}

static gr_glyph_to_char_association* calloc_glyph_to_char_association(
    uint32_t aGlyphCount, uint32_t aLength) {
  using Type1 = gr_glyph_to_char_association;
  using Type2 = gr_glyph_to_char_cluster;
  using Type3 = float;
  using Type4 = float;
  using Type5 = uint16_t;

  // We are allocating memory in a pool. To avoid dealing with thorny alignment
  // issues, we allocate from most to least aligned
  static_assert(
      alignof(Type1) >= alignof(Type2) && alignof(Type2) >= alignof(Type3) &&
          alignof(Type3) >= alignof(Type4) && alignof(Type4) >= alignof(Type5),
      "Unexpected alignments of types");

  const uint64_t size1 = sizeof(Type1) * static_cast<uint64_t>(1);
  const uint64_t size2 = sizeof(Type2) * static_cast<uint64_t>(aLength);
  const uint64_t size3 = sizeof(Type3) * static_cast<uint64_t>(aGlyphCount);
  const uint64_t size4 = sizeof(Type4) * static_cast<uint64_t>(aGlyphCount);
  const uint64_t size5 = sizeof(Type5) * static_cast<uint64_t>(aGlyphCount);

  uint64_t totalSize = size1 + size2 + size3 + size4 + size5;

  if (totalSize > std::numeric_limits<uint32_t>::max()) {
    // allocation request got too large
    return nullptr;
  }

  char* const memoryPool = static_cast<char*>(calloc(1, totalSize));
  if (!memoryPool) {
    return nullptr;
  }

  char* currentPoolFront = memoryPool;

  auto data = reinterpret_cast<Type1*>(currentPoolFront);
  currentPoolFront += size1;
  data->clusters = reinterpret_cast<Type2*>(currentPoolFront);
  currentPoolFront += size2;
  data->xLocs = reinterpret_cast<Type3*>(currentPoolFront);
  currentPoolFront += size3;
  data->yLocs = reinterpret_cast<Type4*>(currentPoolFront);
  currentPoolFront += size4;
  data->gids = reinterpret_cast<Type5*>(currentPoolFront);

  return data;
}

// returns nullptr on failure and the glyph to char association on success
gr_glyph_to_char_association* gr_get_glyph_to_char_association(
    gr_segment* aSegment, uint32_t aLength, const char16_t* aText) {
  uint32_t glyphCount = gr_seg_n_slots(aSegment);

  gr_glyph_to_char_association* data =
      calloc_glyph_to_char_association(glyphCount, aLength);
  if (!data) {
    return nullptr;
  }

  bool succeeded =
      LoopThrough(aSegment, aLength, glyphCount, aText, data->cIndex,
                  data->clusters, data->gids, data->xLocs, data->yLocs);
  if (!succeeded) {
    gr_free_char_association(data);
    return nullptr;
  }
  return data;
}

void gr_free_char_association(gr_glyph_to_char_association* aData) {
  free(aData);
}

#undef CHECK
#undef NS_IS_HIGH_SURROGATE
#undef NS_IS_LOW_SURROGATE
#undef IS_POWER_OF_2
