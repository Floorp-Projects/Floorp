/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ScopedCredential.h"
#include "mozilla/dom/WebAuthenticationBinding.h"

namespace mozilla {
namespace dom {

// Only needed for refcounted objects.
NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(ScopedCredential, mParent)
NS_IMPL_CYCLE_COLLECTING_ADDREF(ScopedCredential)
NS_IMPL_CYCLE_COLLECTING_RELEASE(ScopedCredential)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ScopedCredential)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

ScopedCredential::ScopedCredential(nsPIDOMWindowInner* aParent)
  : mParent(aParent)
  , mType(ScopedCredentialType::ScopedCred)
{}

ScopedCredential::~ScopedCredential()
{}

JSObject*
ScopedCredential::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return ScopedCredentialBinding::Wrap(aCx, this, aGivenProto);
}

ScopedCredentialType
ScopedCredential::Type() const
{
  return mType;
}

void
ScopedCredential::GetId(JSContext* aCx,
                        JS::MutableHandle<JSObject*> aRetVal) const
{
  aRetVal.set(mIdBuffer.ToUint8Array(aCx));
}

nsresult
ScopedCredential::SetType(ScopedCredentialType aType)
{
  mType = aType;
  return NS_OK;
}

nsresult
ScopedCredential::SetId(CryptoBuffer& aBuffer)
{
  if (!mIdBuffer.Assign(aBuffer)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}

} // namespace dom
} // namespace mozilla
