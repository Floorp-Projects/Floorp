/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <algorithm>

#include "mozilla/dom/IdleDeadline.h"
#include "mozilla/dom/IdleDeadlineBinding.h"
#include "mozilla/dom/Performance.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsDOMNavigationTiming.h"
#include "nsPIDOMWindow.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(IdleDeadline, mWindow, mGlobal)
NS_IMPL_CYCLE_COLLECTING_ADDREF(IdleDeadline)
NS_IMPL_CYCLE_COLLECTING_RELEASE(IdleDeadline)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(IdleDeadline)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

IdleDeadline::IdleDeadline(nsPIDOMWindowInner* aWindow, bool aDidTimeout,
                           DOMHighResTimeStamp aDeadline)
  : mWindow(aWindow)
  , mDidTimeout(aDidTimeout)
  , mDeadline(aDeadline)
{
  bool hasHadSHO;
  mGlobal = aWindow->GetDoc()->GetScriptHandlingObject(hasHadSHO);
}

IdleDeadline::IdleDeadline(nsIGlobalObject* aGlobal, bool aDidTimeout,
                           DOMHighResTimeStamp aDeadline)
  : mWindow(nullptr)
  , mGlobal(aGlobal)
  , mDidTimeout(aDidTimeout)
  , mDeadline(aDeadline)
{
}

IdleDeadline::~IdleDeadline()
{
}

JSObject*
IdleDeadline::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return IdleDeadline_Binding::Wrap(aCx, this, aGivenProto);
}

DOMHighResTimeStamp
IdleDeadline::TimeRemaining()
{
  if (mDidTimeout) {
    return 0.0;
  }

  if (mWindow) {
    RefPtr<Performance> performance = mWindow->GetPerformance();
    if (!performance) {
      // If there is no performance object the window is partially torn
      // down, so we can safely say that there is no time remaining.
      return 0.0;
    }

    return std::max(mDeadline - performance->Now(), 0.0);
  }

  // If there's no window, we're in a system scope, and can just use
  // a high-resolution TimeStamp::Now();
  auto timestamp = TimeStamp::Now() - TimeStamp::ProcessCreation();
  return std::max(mDeadline - timestamp.ToMilliseconds(), 0.0);
}

bool
IdleDeadline::DidTimeout() const
{
  return mDidTimeout;
}

} // namespace dom
} // namespace mozilla
