// Copyright (c) 2018 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OTS_VARIATIONS_H_
#define OTS_VARIATIONS_H_

#include <vector>
#include "ots.h"

// Utility functions for OpenType variations common table formats.

namespace ots {

bool ParseItemVariationStore(const Font* font,
                             const uint8_t* data, const size_t length,
                             std::vector<uint16_t>* out_region_index_count = NULL);

bool ParseDeltaSetIndexMap(const Font* font, const uint8_t* data, const size_t length);

bool ParseVariationData(const Font* font, const uint8_t* data, size_t length,
                        size_t axisCount, size_t sharedTupleCount);

}  // namespace ots

#endif  // OTS_VARIATIONS_H_
