/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IndexedDBCommon.h"

#include "js/StructuredClone.h"
#include "mozilla/SnappyUncompressInputStream.h"

namespace mozilla::dom::indexedDB {

// aStructuredCloneData is a parameter rather than a return value because one
// caller preallocates it on the heap not immediately before calling for some
// reason. Maybe this could be changed.
nsresult SnappyUncompressStructuredCloneData(
    nsIInputStream& aInputStream, JSStructuredCloneData& aStructuredCloneData) {
  const auto snappyInputStream =
      MakeRefPtr<SnappyUncompressInputStream>(&aInputStream);

  char buffer[kFileCopyBufferSize];

  QM_TRY(CollectEach(
      [&snappyInputStream = *snappyInputStream, &buffer] {
        QM_TRY_RETURN(MOZ_TO_RESULT_INVOKE(snappyInputStream, Read, buffer,
                                           sizeof(buffer)));
      },
      [&aStructuredCloneData,
       &buffer](const uint32_t& numRead) -> Result<Ok, nsresult> {
        QM_TRY(OkIf(aStructuredCloneData.AppendBytes(buffer, numRead)),
               Err(NS_ERROR_OUT_OF_MEMORY));

        return Ok{};
      }));

  return NS_OK;
}

}  // namespace mozilla::dom::indexedDB
