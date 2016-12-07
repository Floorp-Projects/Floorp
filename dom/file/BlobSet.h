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

namespace mozilla {
namespace dom {

class BlobImpl;

class BlobSet final
{
public:
  nsresult AppendVoidPtr(const void* aData, uint32_t aLength);

  nsresult AppendString(const nsAString& aString, bool nativeEOL,
                        JSContext* aCx);

  nsresult AppendBlobImpl(BlobImpl* aBlobImpl);

  nsTArray<RefPtr<BlobImpl>>& GetBlobImpls() { return mBlobImpls; }

private:
  nsTArray<RefPtr<BlobImpl>> mBlobImpls;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_BlobSet_h
