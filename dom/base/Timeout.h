/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_timeout_h
#define mozilla_dom_timeout_h

#include "mozilla/dom/PopupBlocker.h"
#include "mozilla/dom/TimeoutHandler.h"
#include "mozilla/LinkedList.h"
#include "mozilla/TimeStamp.h"
#include "nsCOMPtr.h"
#include "nsGlobalWindowInner.h"
#include "nsCycleCollectionParticipant.h"
#include "GeckoProfiler.h"

class nsIEventTarget;
class nsIPrincipal;
class nsIEventTarget;

namespace mozilla {
namespace dom {

/*
 * Timeout struct that holds information about each script
 * timeout.  Holds a strong reference to an nsITimeoutHandler, which
 * abstracts the language specific cruft.
 */
class Timeout final : public LinkedListElement<RefPtr<Timeout>> {
 public:
  Timeout();

  NS_DECL_CYCLE_COLLECTION_NATIVE_CLASS(Timeout)
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(Timeout)

  enum class Reason : uint8_t {
    eTimeoutOrInterval,
    eIdleCallbackTimeout,
  };

  void SetWhenOrTimeRemaining(const TimeStamp& aBaseTime,
                              const TimeDuration& aDelay);

  // Can only be called when not frozen.
  const TimeStamp& When() const;

  const TimeStamp& SubmitTime() const;

  // Can only be called when frozen.
  const TimeDuration& TimeRemaining() const;

#ifdef MOZ_GECKO_PROFILER
  UniqueProfilerBacktrace TakeProfilerBacktrace() { return std::move(mCause); }
#endif

 private:
  // mWhen and mTimeRemaining can't be in a union, sadly, because they
  // have constructors.
  // Nominal time to run this timeout.  Use only when timeouts are not
  // frozen.
  TimeStamp mWhen;

  // Remaining time to wait.  Used only when timeouts are frozen.
  TimeDuration mTimeRemaining;

  // Time that the timeout started, restarted, or was frozen.  Useful for
  // logging time from (virtual) start of a timer until the time it fires
  // (or is cancelled, etc)
  TimeStamp mSubmitTime;

  ~Timeout() = default;

 public:
  // Public member variables in this section.  Please don't add to this list
  // or mix methods with these.  The interleaving public/private sections
  // is necessary as we migrate members to private while still trying to
  // keep decent binary packing.

  // Window for which this timeout fires
  RefPtr<nsGlobalWindowInner> mWindow;

  // The language-specific information about the callback.
  RefPtr<TimeoutHandler> mScriptHandler;

  // Interval
  TimeDuration mInterval;

#ifdef MOZ_GECKO_PROFILER
  UniqueProfilerBacktrace mCause;
#endif

  // Returned as value of setTimeout()
  uint32_t mTimeoutId;

  // Identifies which firing level this Timeout is being processed in
  // when sync loops trigger nested firing.
  uint32_t mFiringId;

#ifdef DEBUG
  int64_t mFiringIndex;
#endif

  // The popup state at timeout creation time if not created from
  // another timeout
  PopupBlocker::PopupControlState mPopupState;

  // Used to allow several reasons for setting a timeout, where each
  // 'Reason' value is using a possibly overlapping set of id:s.
  Reason mReason;

  // Between 0 and DOM_CLAMP_TIMEOUT_NESTING_LEVEL.  Currently we don't
  // care about nesting levels beyond that value.
  uint8_t mNestingLevel;

  // True if the timeout was cleared
  bool mCleared;

  // True if this is one of the timeouts that are currently running
  bool mRunning;

  // True if this is a repeating/interval timer
  bool mIsInterval;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_timeout_h
