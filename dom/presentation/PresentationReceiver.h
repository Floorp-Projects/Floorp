/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PresentationReceiver_h
#define mozilla_dom_PresentationReceiver_h

#include "mozilla/ErrorResult.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIPresentationListener.h"
#include "nsWrapperCache.h"
#include "nsString.h"

class nsPIDOMWindowInner;

namespace mozilla {
namespace dom {

class PresentationConnection;
class PresentationConnectionList;
class Promise;

class PresentationReceiver final : public nsIPresentationRespondingListener
                                 , public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(PresentationReceiver)
  NS_DECL_NSIPRESENTATIONRESPONDINGLISTENER

  static already_AddRefed<PresentationReceiver> Create(nsPIDOMWindowInner* aWindow);

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  nsPIDOMWindowInner* GetParentObject() const
  {
    return mOwner;
  }

  // WebIDL (public APIs)
  already_AddRefed<Promise> GetConnectionList(ErrorResult& aRv);

private:
  explicit PresentationReceiver(nsPIDOMWindowInner* aWindow);

  virtual ~PresentationReceiver();

  MOZ_IS_CLASS_INIT bool Init();

  void Shutdown();

  void CreateConnectionList();

  // Store the inner window ID for |UnregisterRespondingListener| call in
  // |Shutdown| since the inner window may not exist at that moment.
  uint64_t mWindowId;

  nsCOMPtr<nsPIDOMWindowInner> mOwner;
  nsString mUrl;
  RefPtr<Promise> mGetConnectionListPromise;
  RefPtr<PresentationConnectionList> mConnectionList;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_PresentationReceiver_h
