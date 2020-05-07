/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_GeneratePlaceholderCanvasData_h
#define mozilla_dom_GeneratePlaceholderCanvasData_h

#include "nsCOMPtr.h"
#include "nsIRandomGenerator.h"
#include "nsServiceManagerUtils.h"
#include "mozilla/StaticPrefs_privacy.h"

#define RANDOM_BYTES_TO_SAMPLE 32

namespace mozilla {
namespace dom {

/**
 * When privacy.resistFingerprinting.randomDataOnCanvasExtract is true, tries
 * to generate random canvas data by sampling RANDOM_BYTES_TO_SAMPLE bytes and
 * then repeating those bytes many times to fill the buffer. If this fails or
 * if privacy.resistFingerprinting.randomDataOnCanvasExtract is false, returns
 * all-white, opaque pixel data.
 *
 * @param[in]   size    Size of output buffer
 * @param[out]  buffer  Output buffer
 */
inline void GeneratePlaceholderCanvasData(uint32_t size, uint8_t** buffer) {
  if (!StaticPrefs::privacy_resistFingerprinting_randomDataOnCanvasExtract()) {
    memset(*buffer, 0xFF, size);
    return;
  }
  nsresult rv;
  nsCOMPtr<nsIRandomGenerator> rg =
      do_GetService("@mozilla.org/security/random-generator;1", &rv);
  if (NS_FAILED(rv)) {
    memset(*buffer, 0xFF, size);
  } else {
    uint8_t* tmp_buffer;
    rv = rg->GenerateRandomBytes(RANDOM_BYTES_TO_SAMPLE, &tmp_buffer);
    if (NS_FAILED(rv)) {
      memset(*buffer, 0xFF, size);
    } else {
      auto remaining_to_fill = size;
      auto index = 0;
      while (remaining_to_fill > 0) {
        auto bytes_to_write = (remaining_to_fill > RANDOM_BYTES_TO_SAMPLE)
                                  ? RANDOM_BYTES_TO_SAMPLE
                                  : remaining_to_fill;
        memcpy((*buffer) + (index * RANDOM_BYTES_TO_SAMPLE), tmp_buffer,
               bytes_to_write);
        remaining_to_fill -= bytes_to_write;
        index++;
      }
      free(tmp_buffer);
    }
  }
}

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_GeneratePlaceholderCanvasData_h
