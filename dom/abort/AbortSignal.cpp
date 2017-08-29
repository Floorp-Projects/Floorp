/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AbortSignal.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/AbortSignalBinding.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(AbortSignal)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(AbortSignal,
                                                  DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mController)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(AbortSignal,
                                                DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mController)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(AbortSignal)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(AbortSignal, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(AbortSignal, DOMEventTargetHelper)

AbortSignal::AbortSignal(AbortController* aController,
                         bool aAborted)
  : DOMEventTargetHelper(aController->GetParentObject())
  , mController(aController)
  , mAborted(aAborted)
{}

AbortSignal::AbortSignal(bool aAborted)
  : mAborted(aAborted)
{}

JSObject*
AbortSignal::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return AbortSignalBinding::Wrap(aCx, this, aGivenProto);
}

bool
AbortSignal::Aborted() const
{
  return mAborted;
}

void
AbortSignal::Abort()
{
  MOZ_ASSERT(!mAborted);
  mAborted = true;

  // Let's inform the followers.
  for (uint32_t i = 0; i < mFollowers.Length(); ++i) {
    mFollowers[i]->Aborted();
  }

  EventInit init;
  init.mBubbles = false;
  init.mCancelable = false;

  RefPtr<Event> event =
    Event::Constructor(this, NS_LITERAL_STRING("abort"), init);
  event->SetTrusted(true);

  bool dummy;
  DispatchEvent(event, &dummy);
}

void
AbortSignal::AddFollower(AbortSignal::Follower* aFollower)
{
  MOZ_DIAGNOSTIC_ASSERT(aFollower);
  if (!mFollowers.Contains(aFollower)) {
    mFollowers.AppendElement(aFollower);
  }
}

void
AbortSignal::RemoveFollower(AbortSignal::Follower* aFollower)
{
  MOZ_DIAGNOSTIC_ASSERT(aFollower);
  mFollowers.RemoveElement(aFollower);
}

// AbortSignal::Follower
// ----------------------------------------------------------------------------

AbortSignal::Follower::~Follower()
{
  Unfollow();
}

void
AbortSignal::Follower::Follow(AbortSignal* aSignal)
{
  MOZ_DIAGNOSTIC_ASSERT(aSignal);

  Unfollow();

  mFollowingSignal = aSignal;
  aSignal->AddFollower(this);
}

void
AbortSignal::Follower::Unfollow()
{
  if (mFollowingSignal) {
    mFollowingSignal->RemoveFollower(this);
    mFollowingSignal = nullptr;
  }
}

} // dom namespace
} // mozilla namespace
