/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/intl/Bidi.h"
#include "mozilla/Casting.h"
#include "mozilla/intl/ICU4CGlue.h"

#if !USE_RUST_UNICODE_BIDI
#  include "unicode/ubidi.h"
#endif

namespace mozilla::intl {

#if USE_RUST_UNICODE_BIDI
using namespace ffi;

Bidi::Bidi() = default;
Bidi::~Bidi() = default;
#else
Bidi::Bidi() { mBidi = ubidi_open(); }
Bidi::~Bidi() { ubidi_close(mBidi.GetMut()); }
#endif

ICUResult Bidi::SetParagraph(Span<const char16_t> aParagraph,
                             BidiEmbeddingLevel aLevel) {
#if USE_RUST_UNICODE_BIDI
  const auto* text = reinterpret_cast<const uint16_t*>(aParagraph.Elements());
  mBidi.reset(bidi_new(text, aParagraph.Length(), aLevel));

  return ToICUResult(U_ZERO_ERROR);
#else
  // Do not allow any reordering of the runs, as this can change the
  // performance characteristics of working with runs. In the default mode,
  // the levels can be iterated over directly, rather than relying on computing
  // logical runs on the fly. This can have negative performance characteristics
  // compared to iterating over the levels.
  //
  // In the UBIDI_REORDER_RUNS_ONLY the levels are encoded with additional
  // information which can be safely ignored in this Bidi implementation.
  // Note that this check is here since setting the mode must be done before
  // calls to setting the paragraph.
  MOZ_ASSERT(ubidi_getReorderingMode(mBidi.GetMut()) == UBIDI_REORDER_DEFAULT);

  UErrorCode status = U_ZERO_ERROR;
  ubidi_setPara(mBidi.GetMut(), aParagraph.Elements(),
                AssertedCast<int32_t>(aParagraph.Length()), aLevel, nullptr,
                &status);

  mLevels = nullptr;

  return ToICUResult(status);
#endif
}

Bidi::ParagraphDirection Bidi::GetParagraphDirection() const {
#if USE_RUST_UNICODE_BIDI
  auto dir = bidi_get_direction(mBidi.get());
  switch (dir) {
    case -1:
      return Bidi::ParagraphDirection::RTL;
    case 0:
      return Bidi::ParagraphDirection::Mixed;
    case 1:
      return Bidi::ParagraphDirection::LTR;
    default:
      MOZ_ASSERT_UNREACHABLE("Bad direction value");
      return Bidi::ParagraphDirection::Mixed;
  }
#else
  switch (ubidi_getDirection(mBidi.GetConst())) {
    case UBIDI_LTR:
      return Bidi::ParagraphDirection::LTR;
    case UBIDI_RTL:
      return Bidi::ParagraphDirection::RTL;
    case UBIDI_MIXED:
      return Bidi::ParagraphDirection::Mixed;
    case UBIDI_NEUTRAL:
      // This is only used in `ubidi_getBaseDirection` which is unused in this
      // API.
      MOZ_ASSERT_UNREACHABLE("Unexpected UBiDiDirection value.");
  };
  return Bidi::ParagraphDirection::Mixed;
#endif
}

/* static */
void Bidi::ReorderVisual(const BidiEmbeddingLevel* aLevels, int32_t aLength,
                         int32_t* aIndexMap) {
#if USE_RUST_UNICODE_BIDI
  bidi_reorder_visual(reinterpret_cast<const uint8_t*>(aLevels), aLength,
                      aIndexMap);
#else
  ubidi_reorderVisual(reinterpret_cast<const uint8_t*>(aLevels), aLength,
                      aIndexMap);
#endif
}

/* static */
Bidi::BaseDirection Bidi::GetBaseDirection(Span<const char16_t> aText) {
#if USE_RUST_UNICODE_BIDI
  const auto* text = reinterpret_cast<const uint16_t*>(aText.Elements());
  switch (bidi_get_base_direction(text, aText.Length(), false)) {
    case -1:
      return Bidi::BaseDirection::RTL;
    case 0:
      return Bidi::BaseDirection::Neutral;
    case 1:
      return Bidi::BaseDirection::LTR;
    default:
      MOZ_ASSERT_UNREACHABLE("Bad base direction value");
      return Bidi::BaseDirection::Neutral;
  }
#else
  UBiDiDirection direction = ubidi_getBaseDirection(
      aText.Elements(), AssertedCast<int32_t>(aText.Length()));
  switch (direction) {
    case UBIDI_LTR:
      return Bidi::BaseDirection::LTR;
    case UBIDI_RTL:
      return Bidi::BaseDirection::RTL;
    case UBIDI_NEUTRAL:
      return Bidi::BaseDirection::Neutral;
    case UBIDI_MIXED:
      MOZ_ASSERT_UNREACHABLE("Unexpected UBiDiDirection value.");
  }
  return Bidi::BaseDirection::Neutral;
#endif
}

#if !USE_RUST_UNICODE_BIDI
static BidiDirection ToBidiDirection(UBiDiDirection aDirection) {
  switch (aDirection) {
    case UBIDI_LTR:
      return BidiDirection::LTR;
    case UBIDI_RTL:
      return BidiDirection::RTL;
    case UBIDI_MIXED:
    case UBIDI_NEUTRAL:
      MOZ_ASSERT_UNREACHABLE("Unexpected UBiDiDirection value.");
  }
  return BidiDirection::LTR;
}
#endif

Result<int32_t, ICUError> Bidi::CountRuns() {
#if USE_RUST_UNICODE_BIDI
  return bidi_count_runs(mBidi.get());
#else
  UErrorCode status = U_ZERO_ERROR;
  int32_t runCount = ubidi_countRuns(mBidi.GetMut(), &status);
  if (U_FAILURE(status)) {
    return Err(ToICUError(status));
  }

  mLength = ubidi_getProcessedLength(mBidi.GetConst());
  mLevels = mLength > 0 ? reinterpret_cast<const BidiEmbeddingLevel*>(
                              ubidi_getLevels(mBidi.GetMut(), &status))
                        : nullptr;
  if (U_FAILURE(status)) {
    return Err(ToICUError(status));
  }

  return runCount;
#endif
}

void Bidi::GetLogicalRun(int32_t aLogicalStart, int32_t* aLogicalLimitOut,
                         BidiEmbeddingLevel* aLevelOut) {
#if USE_RUST_UNICODE_BIDI
  const int32_t length = bidi_get_length(mBidi.get());
  MOZ_DIAGNOSTIC_ASSERT(aLogicalStart < length);
  const auto* levels = bidi_get_levels(mBidi.get());
#else
  MOZ_ASSERT(mLevels, "CountRuns hasn't been run?");
  MOZ_RELEASE_ASSERT(aLogicalStart < mLength, "Out of bound");
  const int32_t length = mLength;
  const auto* levels = mLevels;
#endif
  const uint8_t level = levels[aLogicalStart];
  int32_t limit;
  for (limit = aLogicalStart + 1; limit < length; limit++) {
    if (levels[limit] != level) {
      break;
    }
  }
  *aLogicalLimitOut = limit;
  *aLevelOut = BidiEmbeddingLevel(level);
}

BidiEmbeddingLevel Bidi::GetParagraphEmbeddingLevel() const {
#if USE_RUST_UNICODE_BIDI
  return BidiEmbeddingLevel(bidi_get_paragraph_level(mBidi.get()));
#else
  return BidiEmbeddingLevel(ubidi_getParaLevel(mBidi.GetConst()));
#endif
}

BidiDirection Bidi::GetVisualRun(int32_t aRunIndex, int32_t* aLogicalStart,
                                 int32_t* aLength) {
#if USE_RUST_UNICODE_BIDI
  auto run = bidi_get_visual_run(mBidi.get(), aRunIndex);
  *aLogicalStart = run.start;
  *aLength = run.length;
  return BidiEmbeddingLevel(run.level).Direction();
#else
  return ToBidiDirection(
      ubidi_getVisualRun(mBidi.GetMut(), aRunIndex, aLogicalStart, aLength));
#endif
}

}  // namespace mozilla::intl
