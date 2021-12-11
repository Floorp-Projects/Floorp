/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Crypto.h"
#include "js/ScalarType.h"
#include "js/experimental/TypedData.h"  // JS_GetArrayBufferViewType
#include "nsCOMPtr.h"
#include "nsIRandomGenerator.h"
#include "nsReadableUtils.h"

#include "mozilla/dom/CryptoBinding.h"
#include "mozilla/dom/SubtleCrypto.h"
#include "nsServiceManagerUtils.h"

namespace mozilla::dom {

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(Crypto)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(Crypto)
NS_IMPL_CYCLE_COLLECTING_RELEASE(Crypto)

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(Crypto, mParent, mSubtle)

Crypto::Crypto(nsIGlobalObject* aParent) : mParent(aParent) {}

Crypto::~Crypto() = default;

/* virtual */
JSObject* Crypto::WrapObject(JSContext* aCx,
                             JS::Handle<JSObject*> aGivenProto) {
  return Crypto_Binding::Wrap(aCx, this, aGivenProto);
}

void Crypto::GetRandomValues(JSContext* aCx, const ArrayBufferView& aArray,
                             JS::MutableHandle<JSObject*> aRetval,
                             ErrorResult& aRv) {
  JS::Rooted<JSObject*> view(aCx, aArray.Obj());

  // Throw if the wrong type of ArrayBufferView is passed in
  // (Part of the Web Crypto API spec)
  switch (JS_GetArrayBufferViewType(view)) {
    case js::Scalar::Int8:
    case js::Scalar::Uint8:
    case js::Scalar::Uint8Clamped:
    case js::Scalar::Int16:
    case js::Scalar::Uint16:
    case js::Scalar::Int32:
    case js::Scalar::Uint32:
    case js::Scalar::BigInt64:
    case js::Scalar::BigUint64:
      break;
    default:
      aRv.Throw(NS_ERROR_DOM_TYPE_MISMATCH_ERR);
      return;
  }

  aArray.ComputeState();
  uint32_t dataLen = aArray.Length();
  if (dataLen == 0) {
    NS_WARNING("ArrayBufferView length is 0, cannot continue");
    aRetval.set(view);
    return;
  } else if (dataLen > 65536) {
    aRv.Throw(NS_ERROR_DOM_QUOTA_EXCEEDED_ERR);
    return;
  }

  nsCOMPtr<nsIRandomGenerator> randomGenerator =
      do_GetService("@mozilla.org/security/random-generator;1");
  if (!randomGenerator) {
    aRv.Throw(NS_ERROR_DOM_OPERATION_ERR);
    return;
  }

  uint8_t* buf;
  nsresult rv = randomGenerator->GenerateRandomBytes(dataLen, &buf);
  if (NS_FAILED(rv) || !buf) {
    aRv.Throw(NS_ERROR_DOM_OPERATION_ERR);
    return;
  }

  // Copy random bytes to ABV.
  memcpy(aArray.Data(), buf, dataLen);
  free(buf);

  aRetval.set(view);
}

void Crypto::RandomUUID(nsAString& aRetVal) {
  // NSID_LENGTH == 39 == 36 UUID chars + 2 curly braces + 1 NUL byte
  static_assert(NSID_LENGTH == 39);

  nsIDToCString uuidString(nsID::GenerateUUID());
  MOZ_ASSERT(strlen(uuidString.get()) == NSID_LENGTH - 1);

  // Convert UUID chars to UTF-16 retval, omitting the curly braces and NUL.
  CopyASCIItoUTF16(Substring(uuidString.get() + 1, NSID_LENGTH - 3), aRetVal);
  MOZ_ASSERT(aRetVal.Length() == NSID_LENGTH - 3);
}

SubtleCrypto* Crypto::Subtle() {
  if (!mSubtle) {
    mSubtle = new SubtleCrypto(GetParentObject());
  }
  return mSubtle;
}

}  // namespace mozilla::dom
