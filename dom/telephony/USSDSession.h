/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_USSDSession_h
#define mozilla_dom_USSDSession_h

#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/Promise.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsITelephonyService.h"
#include "nsPIDOMWindow.h"
#include "nsWrapperCache.h"

struct JSContext;

namespace mozilla {
namespace dom {

class USSDSession final : public nsISupports,
                          public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(USSDSession)

  USSDSession(nsPIDOMWindowInner* aWindow, nsITelephonyService* aService,
              uint32_t aServiceId);

  nsPIDOMWindowInner*
  GetParentObject() const;

  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  // WebIDL
  static already_AddRefed<USSDSession>
  Constructor(const GlobalObject& aGlobal, uint32_t aServiceId,
              ErrorResult& aRv);

  already_AddRefed<Promise>
  Send(const nsAString& aUssd, ErrorResult& aRv);

  already_AddRefed<Promise>
  Cancel(ErrorResult& aRv);

private:
  ~USSDSession();

  already_AddRefed<Promise>
  CreatePromise(ErrorResult& aRv);

  nsCOMPtr<nsPIDOMWindowInner> mWindow;
  nsCOMPtr<nsITelephonyService> mService;
  uint32_t mServiceId;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_USSDSession_h
