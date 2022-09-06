/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_Credential_h
#define mozilla_dom_Credential_h

#include "mozilla/dom/CredentialManagementBinding.h"
#include "nsCycleCollectionParticipant.h"
#include "nsPIDOMWindow.h"
#include "nsWrapperCache.h"

namespace mozilla::dom {

class Credential : public nsISupports, public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(Credential)

 public:
  explicit Credential(nsPIDOMWindowInner* aParent);

 protected:
  virtual ~Credential();

 public:
  nsISupports* GetParentObject() const { return mParent; }

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  void GetId(nsAString& aId) const;

  void GetType(nsAString& aType) const;

  void SetId(const nsAString& aId);

  void SetType(const nsAString& aType);

 private:
  nsCOMPtr<nsPIDOMWindowInner> mParent;
  nsAutoString mId;
  nsAutoString mType;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_Credential_h
