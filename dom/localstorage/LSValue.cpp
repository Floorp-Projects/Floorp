/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LSValue.h"

namespace mozilla {
namespace dom {

bool LSValue::InitFromString(const nsAString& aBuffer) {
  MOZ_ASSERT(mBuffer.IsVoid());
  MOZ_ASSERT(!mUTF16Length);
  MOZ_ASSERT(!mCompressed);

  if (aBuffer.IsVoid()) {
    return true;
  }

  nsCString converted;
  if (NS_WARN_IF(!CopyUTF16toUTF8(aBuffer, converted, fallible))) {
    return false;
  }

  nsCString convertedAndCompressed;
  if (NS_WARN_IF(!SnappyCompress(converted, convertedAndCompressed))) {
    return false;
  }

  if (convertedAndCompressed.IsVoid()) {
    mBuffer = converted;
    mUTF16Length = aBuffer.Length();
  } else {
    mBuffer = convertedAndCompressed;
    mUTF16Length = aBuffer.Length();
    mCompressed = true;
  }

  return true;
}

nsresult LSValue::InitFromStatement(mozIStorageStatement* aStatement,
                                    uint32_t aIndex) {
  MOZ_ASSERT(aStatement);
  MOZ_ASSERT(mBuffer.IsVoid());
  MOZ_ASSERT(!mUTF16Length);
  MOZ_ASSERT(!mCompressed);

  nsCString buffer;
  nsresult rv = aStatement->GetUTF8String(aIndex, buffer);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  int32_t utf16Length;
  rv = aStatement->GetInt32(aIndex + 1, &utf16Length);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  int32_t compressed;
  rv = aStatement->GetInt32(aIndex + 2, &compressed);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  mBuffer = buffer;
  mUTF16Length = utf16Length;
  mCompressed = compressed;

  return NS_OK;
}

const LSValue& VoidLSValue() {
  static const LSValue sVoidLSValue;

  return sVoidLSValue;
}

}  // namespace dom
}  // namespace mozilla
