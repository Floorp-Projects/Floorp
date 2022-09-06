/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _mozilla_dom_Clients_h
#define _mozilla_dom_Clients_h

#include "nsCOMPtr.h"
#include "nsISupports.h"
#include "nsWrapperCache.h"

class nsIGlobalObject;

namespace mozilla {

class ErrorResult;

namespace dom {

struct ClientQueryOptions;
class Promise;

class Clients final : public nsISupports, public nsWrapperCache {
  nsCOMPtr<nsIGlobalObject> mGlobal;

  ~Clients() = default;

 public:
  explicit Clients(nsIGlobalObject* aGlobal);

  // nsWrapperCache interface methods
  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  // DOM bindings methods
  nsIGlobalObject* GetParentObject() const;

  already_AddRefed<Promise> Get(const nsAString& aClientID, ErrorResult& aRv);

  already_AddRefed<Promise> MatchAll(const ClientQueryOptions& aOptions,
                                     ErrorResult& aRv);

  already_AddRefed<Promise> OpenWindow(const nsAString& aURL, ErrorResult& aRv);

  already_AddRefed<Promise> Claim(ErrorResult& aRv);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(Clients)
};

}  // namespace dom
}  // namespace mozilla

#endif  // _mozilla_dom_Clients_h
