/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_BlobSet_h
#define mozilla_dom_BlobSet_h

#include "jsapi.h"
#include "mozilla/RefPtr.h"
#include "nsString.h"
#include "nsTArray.h"

namespace mozilla::dom {

class BlobImpl;

class BlobSet final {
 public:
  [[nodiscard]] nsresult AppendVoidPtr(const void* aData, uint32_t aLength);

  [[nodiscard]] nsresult AppendVector(Vector<uint8_t>&& aData);

  [[nodiscard]] nsresult AppendUTF8String(const nsACString& aUTF8String,
                                          bool nativeEOL);

  [[nodiscard]] nsresult AppendBlobImpl(BlobImpl* aBlobImpl);

  FallibleTArray<RefPtr<BlobImpl>>& GetBlobImpls() { return mBlobImpls; }

 private:
  FallibleTArray<RefPtr<BlobImpl>> mBlobImpls;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_BlobSet_h
