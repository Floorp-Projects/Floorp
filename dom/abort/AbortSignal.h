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

class AbortSignal;

// This class must be implemented by objects who want to follow a AbortSignal.
class AbortFollower
{
public:
  virtual void Abort() = 0;

  void
  Follow(AbortSignal* aSignal);

  void
  Unfollow();

  bool
  IsFollowing() const;

protected:
  virtual ~AbortFollower();

  RefPtr<AbortSignal> mFollowingSignal;
};

class AbortSignal final : public DOMEventTargetHelper
                        , public AbortFollower
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(AbortSignal, DOMEventTargetHelper)

  AbortSignal(nsIGlobalObject* aGlobalObject, bool aAborted);
  explicit AbortSignal(bool aAborted);

  JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  bool
  Aborted() const;

  void
  Abort() override;

  IMPL_EVENT_HANDLER(abort);

  void
  AddFollower(AbortFollower* aFollower);

  void
  RemoveFollower(AbortFollower* aFollower);

private:
  ~AbortSignal() = default;

  // Raw pointers. AbortFollower unregisters itself in the DTOR.
  nsTArray<AbortFollower*> mFollowers;

  bool mAborted;
};

} // dom namespace
} // mozilla namespace

#endif // mozilla_dom_AbortSignal_h
