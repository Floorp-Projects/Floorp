/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsBidi_ICU.h"
#include "ICUUtils.h"

nsBidi::nsBidi()
{
  mBiDi = ubidi_open();
}

nsBidi::~nsBidi()
{
  ubidi_close(mBiDi);
}

nsresult nsBidi::SetPara(const char16_t *aText, int32_t aLength,
                         nsBidiLevel aParaLevel)
{
  UErrorCode error = U_ZERO_ERROR;
  ubidi_setPara(mBiDi, reinterpret_cast<const UChar*>(aText), aLength,
                aParaLevel, nullptr, &error);
  return ICUUtils::UErrorToNsResult(error);
}

nsresult nsBidi::GetDirection(nsBidiDirection* aDirection)
{
  *aDirection = nsBidiDirection(ubidi_getDirection(mBiDi));
  return NS_OK;
}

nsresult nsBidi::GetParaLevel(nsBidiLevel* aParaLevel)
{
  *aParaLevel = ubidi_getParaLevel(mBiDi);
  return NS_OK;
}

nsresult nsBidi::GetLogicalRun(int32_t aLogicalStart, int32_t* aLogicalLimit,
                               nsBidiLevel* aLevel)
{
  ubidi_getLogicalRun(mBiDi, aLogicalStart, aLogicalLimit, aLevel);
  return NS_OK;
}

nsresult nsBidi::CountRuns(int32_t* aRunCount)
{
  UErrorCode errorCode = U_ZERO_ERROR;
  *aRunCount = ubidi_countRuns(mBiDi, &errorCode);
  return ICUUtils::UErrorToNsResult(errorCode);
}

nsresult nsBidi::GetVisualRun(int32_t aRunIndex, int32_t* aLogicalStart,
                              int32_t* aLength, nsBidiDirection* aDirection)
{
  *aDirection = nsBidiDirection(ubidi_getVisualRun(mBiDi, aRunIndex,
                                                   aLogicalStart, aLength));
  return NS_OK;
}

nsresult nsBidi::ReorderVisual(const nsBidiLevel* aLevels, int32_t aLength,
                               int32_t* aIndexMap)
{
  ubidi_reorderVisual(aLevels, aLength, aIndexMap);
  return NS_OK;
}
