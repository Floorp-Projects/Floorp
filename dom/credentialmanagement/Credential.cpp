/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/Credential.h"
#include "mozilla/dom/CredentialManagementBinding.h"
#include "nsCycleCollectionParticipant.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(Credential, mParent)

NS_IMPL_CYCLE_COLLECTING_ADDREF(Credential)
NS_IMPL_CYCLE_COLLECTING_RELEASE(Credential)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(Credential)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

Credential::Credential(nsPIDOMWindowInner* aParent)
  : mParent(aParent)
{}

Credential::~Credential()
{}

JSObject*
Credential::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return Credential_Binding::Wrap(aCx, this, aGivenProto);
}

void
Credential::GetId(nsAString& aId) const
{
  aId.Assign(mId);
}

void
Credential::GetType(nsAString& aType) const
{
  aType.Assign(mType);
}

void
Credential::SetId(const nsAString& aId)
{
  mId.Assign(aId);
}

void
Credential::SetType(const nsAString& aType)
{
  mType.Assign(aType);
}

} // namespace dom
} // namespace mozilla
