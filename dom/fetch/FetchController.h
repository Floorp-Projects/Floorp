/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FetchController_h
#define mozilla_dom_FetchController_h

#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/FetchSignal.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"

namespace mozilla {
namespace dom {

class FetchController final : public nsISupports
                            , public nsWrapperCache
                            , public FetchSignal::Follower
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(FetchController)

  static bool
  IsEnabled(JSContext* aCx, JSObject* aGlobal);

  static already_AddRefed<FetchController>
  Constructor(const GlobalObject& aGlobal, ErrorResult& aRv);

  explicit FetchController(nsIGlobalObject* aGlobal);

  JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  nsIGlobalObject*
  GetParentObject() const;

  FetchSignal*
  Signal();

  void
  Abort();

  void
  Follow(FetchSignal& aSignal);

  void
  Unfollow(FetchSignal& aSignal);

  FetchSignal*
  Following() const;

  // FetchSignal::Follower

  void Aborted() override;

private:
  ~FetchController() = default;

  nsCOMPtr<nsIGlobalObject> mGlobal;
  RefPtr<FetchSignal> mSignal;

  bool mAborted;
};

} // dom namespace
} // mozilla namespace

#endif // mozilla_dom_FetchController_h
