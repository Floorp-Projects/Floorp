/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsBidi.h"

nsresult
nsBidi::CountRuns(int32_t* aRunCount)
{
  UErrorCode errorCode = U_ZERO_ERROR;
  *aRunCount = ubidi_countRuns(mBiDi, &errorCode);
  if (U_SUCCESS(errorCode)) {
    mLength = ubidi_getProcessedLength(mBiDi);
    mLevels = mLength > 0 ? ubidi_getLevels(mBiDi, &errorCode) : nullptr;
  }
  return ICUUtils::UErrorToNsResult(errorCode);
}

void
nsBidi::GetLogicalRun(int32_t aLogicalStart,
                      int32_t* aLogicalLimit, nsBidiLevel* aLevel)
{
  MOZ_ASSERT(mLevels, "CountRuns hasn't been run?");
  MOZ_RELEASE_ASSERT(aLogicalStart < mLength, "Out of bound");
  // This function implements an alternative approach to get logical
  // run that is based on levels of characters, which would avoid O(n^2)
  // performance issue when used in a loop over runs.
  // Per comment in ubidi_getLogicalRun, that function doesn't use this
  // approach because levels have special interpretation when reordering
  // mode is UBIDI_REORDER_RUNS_ONLY. Since we don't use this mode in
  // Gecko, it should be safe to just use levels for this function.
  MOZ_ASSERT(ubidi_getReorderingMode(mBiDi) != UBIDI_REORDER_RUNS_ONLY,
             "Don't support UBIDI_REORDER_RUNS_ONLY mode");

  nsBidiLevel level = mLevels[aLogicalStart];
  int32_t limit;
  for (limit = aLogicalStart + 1; limit < mLength; limit++) {
    if (mLevels[limit] != level) {
      break;
    }
  }
  *aLogicalLimit = limit;
  *aLevel = level;
}
