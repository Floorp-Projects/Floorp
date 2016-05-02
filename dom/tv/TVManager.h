/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TVManager_h
#define mozilla_dom_TVManager_h

#include "mozilla/DOMEventTargetHelper.h"

class nsITVService;

namespace mozilla {
namespace dom {

class Promise;
class TVTuner;

class TVManager final : public DOMEventTargetHelper
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(TVManager, DOMEventTargetHelper)

  static already_AddRefed<TVManager> Create(nsPIDOMWindowInner* aWindow);

  // WebIDL (internal functions)

  virtual JSObject* WrapObject(JSContext *aCx, JS::Handle<JSObject*> aGivenProto) override;

  nsresult SetTuners(const nsTArray<RefPtr<TVTuner>>& aTuners);

  void RejectPendingGetTunersPromises(nsresult aRv);

  nsresult DispatchTVEvent(nsIDOMEvent* aEvent);

  // WebIDL (public APIs)

  already_AddRefed<Promise> GetTuners(ErrorResult& aRv);

private:
  explicit TVManager(nsPIDOMWindowInner* aWindow);

  ~TVManager();

  bool Init();

  nsCOMPtr<nsITVService> mTVService;
  nsTArray<RefPtr<TVTuner>> mTuners;
  bool mIsReady;
  nsTArray<RefPtr<Promise>> mPendingGetTunersPromises;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_TVManager_h
