/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "frontend/BinASTRuntimeSupport.h"

#include <stdint.h>  // uint32_t

#include "gc/Tracer.h"      // TraceManuallyBarrieredEdge
#include "js/Utility.h"     // js_malloc
#include "vm/StringType.h"  // JSAtom

namespace js {
namespace frontend {

BinASTSourceMetadata::~BinASTSourceMetadata() {
  if (isMultipart()) {
    this->asMultipart()->release();
  } else {
    this->asContext()->release();
  }
}

void BinASTSourceMetadata::trace(JSTracer* tracer) {
  if (isMultipart()) {
    this->asMultipart()->trace(tracer);
  } else {
    this->asContext()->trace(tracer);
  }
}

/* static */
BinASTSourceMetadataMultipart* BinASTSourceMetadataMultipart::create(
    const Vector<BinASTKind>& binASTKinds, uint32_t numStrings) {
  uint32_t numBinASTKinds = binASTKinds.length();

  // Given we can perform GC before filling all `JSAtom*` items
  // in the payload, use js_calloc to initialize them with nullptr.
  BinASTSourceMetadataMultipart* data =
      static_cast<BinASTSourceMetadataMultipart*>(
          js_calloc(BinASTSourceMetadataMultipart::totalSize(numBinASTKinds,
                                                             numStrings)));
  if (MOZ_UNLIKELY(!data)) {
    return nullptr;
  }

  new (data) BinASTSourceMetadataMultipart(numBinASTKinds, numStrings);
  memcpy(data->binASTKindBase(), binASTKinds.begin(),
         binASTKinds.length() * sizeof(BinASTKind));

  return data;
}

/* static */
BinASTSourceMetadataMultipart* BinASTSourceMetadataMultipart::create(
    uint32_t numBinASTKinds, uint32_t numStrings) {
  // Given we can perform GC before filling all `JSAtom*` items
  // in the payload, use js_calloc to initialize them with nullptr.
  BinASTSourceMetadataMultipart* data =
      static_cast<BinASTSourceMetadataMultipart*>(
          js_calloc(BinASTSourceMetadataMultipart::totalSize(numBinASTKinds,
                                                             numStrings)));
  if (MOZ_UNLIKELY(!data)) {
    return nullptr;
  }

  new (data) BinASTSourceMetadataMultipart(numBinASTKinds, numStrings);

  return data;
}

void BinASTSourceMetadataMultipart::trace(JSTracer* tracer) {
  JSAtom** base = atomsBase();
  for (uint32_t i = 0; i < numStrings_; i++) {
    if (base[i]) {
      TraceManuallyBarrieredEdge(tracer, &base[i], "BinAST Strings");
    }
  }
}

BinASTSourceMetadataContext* BinASTSourceMetadataContext::create(
    uint32_t numStrings) {
  // Given we can perform GC before filling all `JSAtom*` items
  // in the payload, use js_calloc to initialize them with nullptr.
  BinASTSourceMetadataContext* data = static_cast<BinASTSourceMetadataContext*>(
      js_calloc(BinASTSourceMetadataContext::totalSize(numStrings)));
  if (MOZ_UNLIKELY(!data)) {
    return nullptr;
  }

  new (data) BinASTSourceMetadataContext(numStrings);

  return data;
}

void BinASTSourceMetadataContext::trace(JSTracer* tracer) {
  JSAtom** base = atomsBase();
  for (uint32_t i = 0; i < numStrings_; i++) {
    if (base[i]) {
      TraceManuallyBarrieredEdge(tracer, &base[i], "BinAST Strings");
    }
  }
}

void BinASTSourceMetadataContext::release() {
  if (dictionary_) {
    js_free(dictionary_);
    dictionary_ = nullptr;
  }
}

}  // namespace frontend

}  // namespace js
