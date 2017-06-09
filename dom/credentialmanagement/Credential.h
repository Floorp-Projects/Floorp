/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_Credential_h
#define mozilla_dom_Credential_h

#include "mozilla/dom/CredentialManagementBinding.h"
#include "nsCycleCollectionParticipant.h"
#include "nsPIDOMWindow.h"
#include "nsWrapperCache.h"

namespace mozilla {
namespace dom {

class Credential : public nsISupports
                 , public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(Credential)

public:
  explicit Credential(nsPIDOMWindowInner* aParent);

protected:
  virtual ~Credential();

public:
  nsISupports*
  GetParentObject() const
  {
    return mParent;
  }

  JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  void
  GetId(nsAString& aId) const;

  void
  GetType(nsAString& aType) const;

private:
  nsCOMPtr<nsPIDOMWindowInner> mParent;
  nsAutoString mId;
  nsAutoString mType;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_Credential_h
