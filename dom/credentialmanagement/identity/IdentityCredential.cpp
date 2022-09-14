/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/Promise.h"
#include "mozilla/dom/IdentityCredential.h"
#include "nsCycleCollectionParticipant.h"

namespace mozilla::dom {

NS_IMPL_ADDREF_INHERITED(IdentityCredential, Credential)
NS_IMPL_RELEASE_INHERITED(IdentityCredential, Credential)

NS_IMPL_CYCLE_COLLECTION_INHERITED(IdentityCredential, Credential)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(IdentityCredential)
NS_INTERFACE_MAP_END_INHERITING(Credential)

JSObject* IdentityCredential::WrapObject(JSContext* aCx,
                                         JS::Handle<JSObject*> aGivenProto) {
  return IdentityCredential_Binding::Wrap(aCx, this, aGivenProto);
}

void IdentityCredential::GetToken(nsAString& aToken) const {
  aToken.Assign(mToken);
}
void IdentityCredential::SetToken(const nsAString& aToken) {
  mToken.Assign(aToken);
}

}  // namespace mozilla::dom