/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SnappyUtils.h"

#include <stddef.h>
#include "mozilla/Assertions.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/fallible.h"
#include "nsDebug.h"
#include "nsString.h"
#include "snappy/snappy.h"

namespace mozilla::dom {

static_assert(SNAPPY_VERSION == 0x010109);

bool SnappyCompress(const nsACString& aSource, nsACString& aDest) {
  MOZ_ASSERT(!aSource.IsVoid());

  size_t uncompressedLength = aSource.Length();

  if (uncompressedLength <= 16) {
    aDest.SetIsVoid(true);
    return true;
  }

  size_t compressedLength = snappy::MaxCompressedLength(uncompressedLength);

  if (NS_WARN_IF(!aDest.SetLength(compressedLength, fallible))) {
    return false;
  }

  snappy::RawCompress(aSource.BeginReading(), uncompressedLength,
                      aDest.BeginWriting(), &compressedLength);

  if (compressedLength >= uncompressedLength) {
    aDest.SetIsVoid(true);
    return true;
  }

  if (NS_WARN_IF(!aDest.SetLength(compressedLength, fallible))) {
    return false;
  }

  return true;
}

bool SnappyUncompress(const nsACString& aSource, nsACString& aDest) {
  MOZ_ASSERT(!aSource.IsVoid());

  const char* compressed = aSource.BeginReading();

  auto compressedLength = static_cast<size_t>(aSource.Length());

  size_t uncompressedLength = 0u;
  if (!snappy::GetUncompressedLength(compressed, compressedLength,
                                     &uncompressedLength)) {
    return false;
  }

  CheckedUint32 checkedLength(uncompressedLength);
  if (!checkedLength.isValid()) {
    return false;
  }

  aDest.SetLength(checkedLength.value());

  if (!snappy::RawUncompress(compressed, compressedLength,
                             aDest.BeginWriting())) {
    return false;
  }

  return true;
}

}  // namespace mozilla::dom
