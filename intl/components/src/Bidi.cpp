/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/intl/Bidi.h"
#include "mozilla/Casting.h"
#include "mozilla/intl/ICU4CGlue.h"

#include "unicode/ubidi.h"

namespace mozilla::intl {

Bidi::Bidi() { mBidi = ubidi_open(); }
Bidi::~Bidi() { ubidi_close(mBidi.GetMut()); }

ICUResult Bidi::SetParagraph(Span<const char16_t> aParagraph,
                             BidiEmbeddingLevel aLevel) {
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
}

Bidi::ParagraphDirection Bidi::GetParagraphDirection() const {
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
}

/* static */
void Bidi::ReorderVisual(const BidiEmbeddingLevel* aLevels, int32_t aLength,
                         int32_t* aIndexMap) {
  ubidi_reorderVisual(reinterpret_cast<const uint8_t*>(aLevels), aLength,
                      aIndexMap);
}

/* static */
Bidi::BaseDirection Bidi::GetBaseDirection(Span<const char16_t> aParagraph) {
  UBiDiDirection direction = ubidi_getBaseDirection(
      aParagraph.Elements(), AssertedCast<int32_t>(aParagraph.Length()));

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
}

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

Result<int32_t, ICUError> Bidi::CountRuns() {
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
}

void Bidi::GetLogicalRun(int32_t aLogicalStart, int32_t* aLogicalLimitOut,
                         BidiEmbeddingLevel* aLevelOut) {
  MOZ_ASSERT(mLevels, "CountRuns hasn't been run?");
  MOZ_RELEASE_ASSERT(aLogicalStart < mLength, "Out of bound");
  BidiEmbeddingLevel level = mLevels[aLogicalStart];
  int32_t limit;
  for (limit = aLogicalStart + 1; limit < mLength; limit++) {
    if (mLevels[limit] != level) {
      break;
    }
  }
  *aLogicalLimitOut = limit;
  *aLevelOut = level;
}

BidiEmbeddingLevel Bidi::GetParagraphEmbeddingLevel() const {
  return BidiEmbeddingLevel(ubidi_getParaLevel(mBidi.GetConst()));
}

BidiDirection Bidi::GetVisualRun(int32_t aRunIndex, int32_t* aLogicalStart,
                                 int32_t* aLength) {
  return ToBidiDirection(
      ubidi_getVisualRun(mBidi.GetMut(), aRunIndex, aLogicalStart, aLength));
}

}  // namespace mozilla::intl
