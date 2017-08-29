/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_AbortController_h
#define mozilla_dom_AbortController_h

#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/AbortSignal.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"

namespace mozilla {
namespace dom {

class AbortController final : public nsISupports
                            , public nsWrapperCache
                            , public AbortSignal::Follower
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(AbortController)

  static bool
  IsEnabled(JSContext* aCx, JSObject* aGlobal);

  static already_AddRefed<AbortController>
  Constructor(const GlobalObject& aGlobal, ErrorResult& aRv);

  explicit AbortController(nsIGlobalObject* aGlobal);

  JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  nsIGlobalObject*
  GetParentObject() const;

  AbortSignal*
  Signal();

  void
  Abort();

  void
  Follow(AbortSignal& aSignal);

  void
  Unfollow(AbortSignal& aSignal);

  AbortSignal*
  Following() const;

  // AbortSignal::Follower

  void Aborted() override;

private:
  ~AbortController() = default;

  nsCOMPtr<nsIGlobalObject> mGlobal;
  RefPtr<AbortSignal> mSignal;

  bool mAborted;
};

} // dom namespace
} // mozilla namespace

#endif // mozilla_dom_AbortController_h
