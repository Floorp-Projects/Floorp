/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "Crypto.h"
#include "jsfriendapi.h"
#include "nsCOMPtr.h"
#include "nsIRandomGenerator.h"
#include "MainThreadUtils.h"
#include "nsXULAppAPI.h"

#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/CryptoBinding.h"
#include "nsServiceManagerUtils.h"

using mozilla::dom::ContentChild;

namespace mozilla {
namespace dom {

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(Crypto)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCrypto)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(Crypto)
NS_IMPL_CYCLE_COLLECTING_RELEASE(Crypto)

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(Crypto, mParent, mSubtle)

Crypto::Crypto()
{
  MOZ_COUNT_CTOR(Crypto);
}

Crypto::~Crypto()
{
  MOZ_COUNT_DTOR(Crypto);
}

void
Crypto::Init(nsIGlobalObject* aParent)
{
  mParent = do_QueryInterface(aParent);
  MOZ_ASSERT(mParent);
}

/* virtual */ JSObject*
Crypto::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return CryptoBinding::Wrap(aCx, this, aGivenProto);
}

void
Crypto::GetRandomValues(JSContext* aCx, const ArrayBufferView& aArray,
                        JS::MutableHandle<JSObject*> aRetval,
                        ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread(), "Called on the wrong thread");

  JS::Rooted<JSObject*> view(aCx, aArray.Obj());

  if (JS_IsTypedArrayObject(view) && JS_GetTypedArraySharedness(view)) {
    // Throw if the object is mapping shared memory (must opt in).
    aRv.ThrowTypeError<MSG_TYPEDARRAY_IS_SHARED>(NS_LITERAL_STRING("Argument of Crypto.getRandomValues"));
    return;
  }

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
      break;
    default:
      aRv.Throw(NS_ERROR_DOM_TYPE_MISMATCH_ERR);
      return;
  }

  aArray.ComputeLengthAndData();
  uint32_t dataLen = aArray.Length();
  if (dataLen == 0) {
    NS_WARNING("ArrayBufferView length is 0, cannot continue");
    aRetval.set(view);
    return;
  } else if (dataLen > 65536) {
    aRv.Throw(NS_ERROR_DOM_QUOTA_EXCEEDED_ERR);
    return;
  }

  uint8_t* data = aArray.Data();

  if (!XRE_IsParentProcess()) {
    InfallibleTArray<uint8_t> randomValues;
    // Tell the parent process to generate random values via PContent
    ContentChild* cc = ContentChild::GetSingleton();
    if (!cc->SendGetRandomValues(dataLen, &randomValues) ||
        randomValues.Length() == 0) {
      aRv.Throw(NS_ERROR_FAILURE);
      return;
    }
    NS_ASSERTION(dataLen == randomValues.Length(),
                 "Invalid length returned from parent process!");
    memcpy(data, randomValues.Elements(), dataLen);
  } else {
    uint8_t *buf = GetRandomValues(dataLen);

    if (!buf) {
      aRv.Throw(NS_ERROR_FAILURE);
      return;
    }

    memcpy(data, buf, dataLen);
    free(buf);
  }

  aRetval.set(view);
}

SubtleCrypto*
Crypto::Subtle()
{
  if(!mSubtle) {
    mSubtle = new SubtleCrypto(GetParentObject());
  }
  return mSubtle;
}

/* static */ uint8_t*
Crypto::GetRandomValues(uint32_t aLength)
{
  nsCOMPtr<nsIRandomGenerator> randomGenerator;
  nsresult rv;
  randomGenerator = do_GetService("@mozilla.org/security/random-generator;1");
  NS_ENSURE_TRUE(randomGenerator, nullptr);

  uint8_t* buf;
  rv = randomGenerator->GenerateRandomBytes(aLength, &buf);

  NS_ENSURE_SUCCESS(rv, nullptr);

  return buf;
}

} // namespace dom
} // namespace mozilla
