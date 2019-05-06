/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SnappyUtils.h"

#include "snappy/snappy.h"

namespace mozilla {
namespace dom {

bool SnappyCompress(const nsACString& aSource, nsACString& aDest) {
  MOZ_ASSERT(!aSource.IsVoid());

  size_t uncompressedLength = aSource.Length();

  if (uncompressedLength <= 16) {
    return false;
  }

  size_t compressedLength = snappy::MaxCompressedLength(uncompressedLength);

  aDest.SetLength(compressedLength);

  snappy::RawCompress(aSource.BeginReading(), uncompressedLength,
                      aDest.BeginWriting(), &compressedLength);

  if (compressedLength >= uncompressedLength) {
    return false;
  }

  aDest.SetLength(compressedLength);

  return true;
}

bool SnappyUncompress(const nsACString& aSource, nsACString& aDest) {
  MOZ_ASSERT(!aSource.IsVoid());

  const char* compressed = aSource.BeginReading();

  size_t compressedLength = aSource.Length();

  size_t uncompressedLength;
  if (!snappy::GetUncompressedLength(compressed, compressedLength,
                                     &uncompressedLength)) {
    return false;
  }

  aDest.SetLength(uncompressedLength);

  if (!snappy::RawUncompress(compressed, compressedLength,
                             aDest.BeginWriting())) {
    return false;
  }

  return true;
}

}  // namespace dom
}  // namespace mozilla
