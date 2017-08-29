/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FetchSignal_h
#define mozilla_dom_FetchSignal_h

#include "mozilla/DOMEventTargetHelper.h"

namespace mozilla {
namespace dom {

class FetchController;
class FetchSignal;

class FetchSignal final : public DOMEventTargetHelper
{
public:
  // This class must be implemented by objects who want to follow a FetchSignal.
  class Follower
  {
  public:
    virtual void Aborted() = 0;

  protected:
    virtual ~Follower();

    void
    Follow(FetchSignal* aSignal);

    void
    Unfollow();

    RefPtr<FetchSignal> mFollowingSignal;
  };

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(FetchSignal, DOMEventTargetHelper)

  FetchSignal(FetchController* aController, bool aAborted);
  explicit FetchSignal(bool aAborted);

  JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  bool
  Aborted() const;

  void
  Abort();

  IMPL_EVENT_HANDLER(abort);

  void
  AddFollower(Follower* aFollower);

  void
  RemoveFollower(Follower* aFollower);

  bool
  CanAcceptFollower(Follower* aFollower) const;

private:
  ~FetchSignal() = default;

  RefPtr<FetchController> mController;

  // Raw pointers. Follower unregisters itself in the DTOR.
  nsTArray<Follower*> mFollowers;

  bool mAborted;
};

} // dom namespace
} // mozilla namespace

#endif // mozilla_dom_FetchSignal_h
