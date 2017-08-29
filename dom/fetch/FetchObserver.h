/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FetchObserver_h
#define mozilla_dom_FetchObserver_h

#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/dom/FetchObserverBinding.h"
#include "mozilla/dom/AbortSignal.h"

namespace mozilla {
namespace dom {

class FetchObserver final : public DOMEventTargetHelper
                          , public AbortFollower
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(FetchObserver, DOMEventTargetHelper)

  static bool
  IsEnabled(JSContext* aCx, JSObject* aGlobal);

  FetchObserver(nsIGlobalObject* aGlobal, AbortSignal* aSignal);

  JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  FetchState
  State() const;

  IMPL_EVENT_HANDLER(statechange);
  IMPL_EVENT_HANDLER(requestprogress);
  IMPL_EVENT_HANDLER(responseprogress);

  void
  Abort() override;

  void
  SetState(FetchState aState);

private:
  ~FetchObserver() = default;

  FetchState mState;
};

} // dom namespace
} // mozilla namespace

#endif // mozilla_dom_FetchObserver_h
