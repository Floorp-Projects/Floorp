/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FetchSignal.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/FetchSignalBinding.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(FetchSignal)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(FetchSignal,
                                                  DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mController)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(FetchSignal,
                                                DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mController)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(FetchSignal)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(FetchSignal, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(FetchSignal, DOMEventTargetHelper)

FetchSignal::FetchSignal(FetchController* aController,
                         bool aAborted)
  : DOMEventTargetHelper(aController->GetParentObject())
  , mController(aController)
  , mAborted(aAborted)
{}

FetchSignal::FetchSignal(bool aAborted)
  : mAborted(aAborted)
{}

JSObject*
FetchSignal::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return FetchSignalBinding::Wrap(aCx, this, aGivenProto);
}

bool
FetchSignal::Aborted() const
{
  return mAborted;
}

void
FetchSignal::Abort()
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

  // TODO which kind of event should we dispatch here?

  RefPtr<Event> event =
    Event::Constructor(this, NS_LITERAL_STRING("abort"), init);
  event->SetTrusted(true);

  bool dummy;
  DispatchEvent(event, &dummy);
}

void
FetchSignal::AddFollower(FetchSignal::Follower* aFollower)
{
  MOZ_DIAGNOSTIC_ASSERT(aFollower);
  if (!mFollowers.Contains(aFollower)) {
    mFollowers.AppendElement(aFollower);
  }
}

void
FetchSignal::RemoveFollower(FetchSignal::Follower* aFollower)
{
  MOZ_DIAGNOSTIC_ASSERT(aFollower);
  mFollowers.RemoveElement(aFollower);
}

bool
FetchSignal::CanAcceptFollower(FetchSignal::Follower* aFollower) const
{
  MOZ_DIAGNOSTIC_ASSERT(aFollower);

  if (!mController) {
    return true;
  }

  if (aFollower == mController) {
    return false;
  }

  FetchSignal* following = mController->Following();
  if (!following) {
    return true;
  }

  return following->CanAcceptFollower(aFollower);
}

// FetchSignal::Follower
// ----------------------------------------------------------------------------

FetchSignal::Follower::~Follower()
{
  Unfollow();
}

void
FetchSignal::Follower::Follow(FetchSignal* aSignal)
{
  MOZ_DIAGNOSTIC_ASSERT(aSignal);

  if (!aSignal->CanAcceptFollower(this)) {
    return;
  }

  Unfollow();

  mFollowingSignal = aSignal;
  aSignal->AddFollower(this);
}

void
FetchSignal::Follower::Unfollow()
{
  if (mFollowingSignal) {
    mFollowingSignal->RemoveFollower(this);
    mFollowingSignal = nullptr;
  }
}

} // dom namespace
} // mozilla namespace
