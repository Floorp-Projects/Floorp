/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_GeneratePlaceholderCanvasData_h
#define mozilla_dom_GeneratePlaceholderCanvasData_h

#include "mozilla/StaticPrefs_privacy.h"
#include "nsCOMPtr.h"
#include "nsIRandomGenerator.h"
#include "nsServiceManagerUtils.h"

#define RANDOM_BYTES_TO_SAMPLE 32

namespace mozilla::dom {

/**
 * When privacy.resistFingerprinting.randomDataOnCanvasExtract is true, tries
 * to generate random data for placeholder canvas by sampling
 * RANDOM_BYTES_TO_SAMPLE bytes and then returning it. If this fails or if
 * privacy.resistFingerprinting.randomDataOnCanvasExtract is false, returns
 * nullptr.
 *
 * @return uint8_t*  output buffer
 */
inline uint8_t* TryToGenerateRandomDataForPlaceholderCanvasData() {
  if (!StaticPrefs::privacy_resistFingerprinting_randomDataOnCanvasExtract()) {
    return nullptr;
  }
  nsresult rv;
  nsCOMPtr<nsIRandomGenerator> rg =
      do_GetService("@mozilla.org/security/random-generator;1", &rv);
  if (NS_FAILED(rv)) {
    return nullptr;
  }
  uint8_t* randomData;
  rv = rg->GenerateRandomBytes(RANDOM_BYTES_TO_SAMPLE, &randomData);
  if (NS_FAILED(rv)) {
    return nullptr;
  }
  return randomData;
}

/**
 * If randomData not nullptr, repeats those bytes many times to fill buffer. If
 * randomData is nullptr, returns all-white pixel data.
 *
 * @param[in]   randomData  Buffer of RANDOM_BYTES_TO_SAMPLE bytes of random
 *                          data, or nullptr
 * @param[in]   size        Size of output buffer
 * @param[out]  buffer      Output buffer
 */
inline void FillPlaceholderCanvas(uint8_t* randomData, uint32_t size,
                                  uint8_t* buffer) {
  if (!randomData) {
    memset(buffer, 0xFF, size);
    return;
  }
  auto remaining_to_fill = size;
  auto index = 0;
  while (remaining_to_fill > 0) {
    auto bytes_to_write = (remaining_to_fill > RANDOM_BYTES_TO_SAMPLE)
                              ? RANDOM_BYTES_TO_SAMPLE
                              : remaining_to_fill;
    memcpy(buffer + (index * RANDOM_BYTES_TO_SAMPLE), randomData,
           bytes_to_write);
    remaining_to_fill -= bytes_to_write;
    index++;
  }
  free(randomData);
}

/**
 * When privacy.resistFingerprinting.randomDataOnCanvasExtract is true, tries
 * to generate random canvas data by sampling RANDOM_BYTES_TO_SAMPLE bytes and
 * then repeating those bytes many times to fill the buffer. If this fails or
 * if privacy.resistFingerprinting.randomDataOnCanvasExtract is false, returns
 * all-white, opaque pixel data.
 *
 * It is recommended that callers call this function instead of individually
 * calling TryToGenerateRandomDataForPlaceholderCanvasData and
 * FillPlaceholderCanvas unless there are requirements, like NoGC, that prevent
 * them from calling the RNG service inside this function.
 *
 * @param[in]   size    Size of output buffer
 * @param[out]  buffer  Output buffer
 */
inline void GeneratePlaceholderCanvasData(uint32_t size, uint8_t* buffer) {
  uint8_t* randomData = TryToGenerateRandomDataForPlaceholderCanvasData();
  FillPlaceholderCanvas(randomData, size, buffer);
}

}  // namespace mozilla::dom

#endif  // mozilla_dom_GeneratePlaceholderCanvasData_h
