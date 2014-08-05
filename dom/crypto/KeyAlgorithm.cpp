/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/KeyAlgorithm.h"
#include "mozilla/dom/WebCryptoCommon.h"
#include "mozilla/dom/AesKeyAlgorithm.h"
#include "mozilla/dom/HmacKeyAlgorithm.h"
#include "mozilla/dom/RsaKeyAlgorithm.h"
#include "mozilla/dom/RsaHashedKeyAlgorithm.h"
#include "mozilla/dom/SubtleCryptoBinding.h"
#include "mozilla/dom/WebCryptoCommon.h"

namespace mozilla {
namespace dom {


NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(KeyAlgorithm, mGlobal)
NS_IMPL_CYCLE_COLLECTING_ADDREF(KeyAlgorithm)
NS_IMPL_CYCLE_COLLECTING_RELEASE(KeyAlgorithm)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(KeyAlgorithm)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

KeyAlgorithm::KeyAlgorithm(nsIGlobalObject* aGlobal, const nsString& aName)
  : mGlobal(aGlobal)
  , mName(aName)
{
  SetIsDOMBinding();

  // Set mechanism based on algorithm name
  mMechanism = MapAlgorithmNameToMechanism(aName);

  // HMAC not handled here, since it requires extra info
}

KeyAlgorithm::~KeyAlgorithm()
{
}

JSObject*
KeyAlgorithm::WrapObject(JSContext* aCx)
{
  return KeyAlgorithmBinding::Wrap(aCx, this);
}

nsString
KeyAlgorithm::ToJwkAlg() const
{
  return nsString();
}

void
KeyAlgorithm::GetName(nsString& aRetVal) const
{
  aRetVal.Assign(mName);
}

bool
KeyAlgorithm::WriteStructuredClone(JSStructuredCloneWriter* aWriter) const
{
  return WriteString(aWriter, mName);
}

KeyAlgorithm*
KeyAlgorithm::Create(nsIGlobalObject* aGlobal, JSStructuredCloneReader* aReader)
{
  uint32_t tag, zero;
  bool read = JS_ReadUint32Pair( aReader, &tag, &zero );
  if (!read) {
    return nullptr;
  }

  KeyAlgorithm* algorithm = nullptr;
  switch (tag) {
    case SCTAG_KEYALG: {
      nsString name;
      read = ReadString(aReader, name);
      if (!read) {
        return nullptr;
      }
      algorithm = new KeyAlgorithm(aGlobal, name);
      break;
    }
    case SCTAG_AESKEYALG: {
      algorithm = AesKeyAlgorithm::Create(aGlobal, aReader);
      break;
    }
    case SCTAG_HMACKEYALG: {
      algorithm = HmacKeyAlgorithm::Create(aGlobal, aReader);
      break;
    }
    case SCTAG_RSAKEYALG: {
      algorithm = RsaKeyAlgorithm::Create(aGlobal, aReader);
      break;
    }
    case SCTAG_RSAHASHEDKEYALG: {
      algorithm = RsaHashedKeyAlgorithm::Create(aGlobal, aReader);
      break;
    }
    // No default, algorithm is already nullptr
  }

  return algorithm;
}

} // namespace dom
} // namespace mozilla
