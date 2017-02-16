/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BlobImpl.h"
#include "File.h"
#include "mozilla/CheckedInt.h"

namespace mozilla {
namespace dom {

// Makes sure that aStart and aEnd is less then or equal to aSize and greater
// than 0
static void
ParseSize(int64_t aSize, int64_t& aStart, int64_t& aEnd)
{
  CheckedInt64 newStartOffset = aStart;
  if (aStart < -aSize) {
    newStartOffset = 0;
  }
  else if (aStart < 0) {
    newStartOffset += aSize;
  }
  else if (aStart > aSize) {
    newStartOffset = aSize;
  }

  CheckedInt64 newEndOffset = aEnd;
  if (aEnd < -aSize) {
    newEndOffset = 0;
  }
  else if (aEnd < 0) {
    newEndOffset += aSize;
  }
  else if (aEnd > aSize) {
    newEndOffset = aSize;
  }

  if (!newStartOffset.isValid() || !newEndOffset.isValid() ||
      newStartOffset.value() >= newEndOffset.value()) {
    aStart = aEnd = 0;
  }
  else {
    aStart = newStartOffset.value();
    aEnd = newEndOffset.value();
  }
}

already_AddRefed<BlobImpl>
BlobImpl::Slice(const Optional<int64_t>& aStart,
                const Optional<int64_t>& aEnd,
                const nsAString& aContentType,
                ErrorResult& aRv)
{
  // Truncate aStart and aEnd so that we stay within this file.
  uint64_t thisLength = GetSize(aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  int64_t start = aStart.WasPassed() ? aStart.Value() : 0;
  int64_t end = aEnd.WasPassed() ? aEnd.Value() : (int64_t)thisLength;

  ParseSize((int64_t)thisLength, start, end);

  nsAutoString type(aContentType);
  Blob::MakeValidBlobType(type);
  return CreateSlice((uint64_t)start, (uint64_t)(end - start), type, aRv);
}

NS_IMPL_ISUPPORTS(BlobImpl, BlobImpl)

} // namespace dom
} // namespace mozilla
