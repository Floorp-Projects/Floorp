/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_AbortSignal_h
#define mozilla_dom_AbortSignal_h

#include "mozilla/DOMEventTargetHelper.h"

namespace mozilla {
namespace dom {

class AbortController;
class AbortSignal;

class AbortSignal final : public DOMEventTargetHelper
{
public:
  // This class must be implemented by objects who want to follow a AbortSignal.
  class Follower
  {
  public:
    virtual void Aborted() = 0;

  protected:
    virtual ~Follower();

    void
    Follow(AbortSignal* aSignal);

    void
    Unfollow();

    RefPtr<AbortSignal> mFollowingSignal;
  };

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(AbortSignal, DOMEventTargetHelper)

  AbortSignal(AbortController* aController, bool aAborted);
  explicit AbortSignal(bool aAborted);

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
  ~AbortSignal() = default;

  RefPtr<AbortController> mController;

  // Raw pointers. Follower unregisters itself in the DTOR.
  nsTArray<Follower*> mFollowers;

  bool mAborted;
};

} // dom namespace
} // mozilla namespace

#endif // mozilla_dom_AbortSignal_h
