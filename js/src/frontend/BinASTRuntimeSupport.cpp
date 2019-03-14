/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "frontend/BinASTRuntimeSupport.h"

#include "gc/Tracer.h"
#include "js/Vector.h"
#include "vm/StringType.h"

namespace js {
namespace frontend {

/* static */
BinASTSourceMetadata* BinASTSourceMetadata::Create(
    const Vector<BinASTKind>& binASTKinds, uint32_t numStrings) {
  uint32_t numBinASTKinds = binASTKinds.length();

  BinASTSourceMetadata* data = static_cast<BinASTSourceMetadata*>(
      js_malloc(BinASTSourceMetadata::totalSize(numBinASTKinds, numStrings)));
  if (!data) {
    return nullptr;
  }

  new (data) BinASTSourceMetadata(numBinASTKinds, numStrings);
  memcpy(data->binASTKindBase(), binASTKinds.begin(),
         binASTKinds.length() * sizeof(BinASTKind));

  return data;
}

void BinASTSourceMetadata::trace(JSTracer* tracer) {
  JSAtom** base = atomsBase();
  for (uint32_t i = 0; i < numStrings_; i++) {
    if (base[i]) {
      TraceManuallyBarrieredEdge(tracer, &base[i], "BinAST Strings");
    }
  }
}

}  // namespace frontend
}  // namespace js
