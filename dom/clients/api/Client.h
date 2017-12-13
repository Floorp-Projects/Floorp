/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _mozilla_dom_Client_h
#define _mozilla_dom_Client_h

#include "mozilla/dom/ClientBinding.h"
#include "nsCOMPtr.h"
#include "nsContentUtils.h"
#include "nsISupports.h"
#include "nsWrapperCache.h"

class nsIGlobalObject;

namespace mozilla {

class ErrorResult;

namespace dom {

class ClientHandle;
class ClientInfoAndState;
class Promise;

template <typename t> class Sequence;

class Client final : public nsISupports
                   , public nsWrapperCache
{
  nsCOMPtr<nsIGlobalObject> mGlobal;
  UniquePtr<ClientInfoAndState> mData;
  RefPtr<ClientHandle> mHandle;

  ~Client() = default;

  void
  EnsureHandle();

public:
  Client(nsIGlobalObject* aGlobal, const ClientInfoAndState& aData);

  TimeStamp
  CreationTime() const;

  TimeStamp
  LastFocusTime() const;

  nsContentUtils::StorageAccess
  GetStorageAccess() const;

  // nsWrapperCache interface methods
  JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  // DOM bindings methods
  nsIGlobalObject*
  GetParentObject() const;

  // Client Bindings
  void
  GetUrl(nsAString& aUrlOut) const;

  void
  GetId(nsAString& aIdOut) const;

  ClientType
  Type() const;

  FrameType
  GetFrameType() const;

  // WindowClient bindings
  VisibilityState
  GetVisibilityState() const;

  bool
  Focused() const;

  already_AddRefed<Promise>
  Focus(ErrorResult& aRv);

  already_AddRefed<Promise>
  Navigate(const nsAString& aURL, ErrorResult& aRv);

  void
  PostMessage(JSContext* aCx, JS::Handle<JS::Value> aMessage,
              const Sequence<JSObject*>& aTransferrable,
              ErrorResult& aRv);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(mozilla::dom::Client)
};

} // namespace dom
} // namespace mozilla

#endif // _mozilla_dom_Client_h
