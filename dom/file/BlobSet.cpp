/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/BlobSet.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/dom/File.h"
#include "MemoryBlobImpl.h"
#include "MultipartBlobImpl.h"

namespace mozilla {
namespace dom {

nsresult
BlobSet::AppendVoidPtr(const void* aData, uint32_t aLength)
{
  NS_ENSURE_ARG_POINTER(aData);
  if (!aLength) {
    return NS_OK;
  }

  void* data = malloc(aLength);
  if (!data) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  memcpy((char*)data, aData, aLength);

  RefPtr<BlobImpl> blobImpl = new MemoryBlobImpl(data, aLength, EmptyString());
  mBlobImpls.AppendElement(blobImpl);

  return NS_OK;
}

nsresult
BlobSet::AppendString(const nsAString& aString, bool nativeEOL)
{
  nsCString utf8Str;
  if (NS_WARN_IF(!AppendUTF16toUTF8(aString, utf8Str, mozilla::fallible))) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  if (nativeEOL) {
    if (utf8Str.Contains('\r')) {
      utf8Str.ReplaceSubstring("\r\n", "\n");
      utf8Str.ReplaceSubstring("\r", "\n");
    }
#ifdef XP_WIN
    utf8Str.ReplaceSubstring("\n", "\r\n");
#endif
  }

  RefPtr<StringBlobImpl> blobImpl =
    StringBlobImpl::Create(utf8Str, EmptyString());
  return AppendBlobImpl(blobImpl);
}

nsresult
BlobSet::AppendBlobImpl(BlobImpl* aBlobImpl)
{
  NS_ENSURE_ARG_POINTER(aBlobImpl);
  mBlobImpls.AppendElement(aBlobImpl);
  return NS_OK;
}

} // dom namespace
} // mozilla namespace
