/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/KeyPair.h"
#include "mozilla/dom/SubtleCryptoBinding.h"
#include "nsContentUtils.h"

namespace mozilla {
namespace dom {


NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(KeyPair, mGlobal, mPublicKey, mPrivateKey)
NS_IMPL_CYCLE_COLLECTING_ADDREF(KeyPair)
NS_IMPL_CYCLE_COLLECTING_RELEASE(KeyPair)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(KeyPair)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

JSObject*
KeyPair::WrapObject(JSContext* aCx)
{
  return KeyPairBinding::Wrap(aCx, this);
}


} // namespace dom
} // namespace mozilla
