/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/CryptoKeyPair.h"
#include "mozilla/dom/SubtleCryptoBinding.h"
#include "nsContentUtils.h"

namespace mozilla {
namespace dom {


NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(CryptoKeyPair, mGlobal, mPublicKey, mPrivateKey)
NS_IMPL_CYCLE_COLLECTING_ADDREF(CryptoKeyPair)
NS_IMPL_CYCLE_COLLECTING_RELEASE(CryptoKeyPair)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(CryptoKeyPair)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

JSObject*
CryptoKeyPair::WrapObject(JSContext* aCx)
{
  return CryptoKeyPairBinding::Wrap(aCx, this);
}


} // namespace dom
} // namespace mozilla
