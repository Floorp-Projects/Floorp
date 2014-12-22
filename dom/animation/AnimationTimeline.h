/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_AnimationTimeline_h
#define mozilla_dom_AnimationTimeline_h

#include "nsWrapperCache.h"
#include "nsCycleCollectionParticipant.h"
#include "mozilla/Attributes.h"
#include "mozilla/TimeStamp.h"
#include "js/TypeDecls.h"
#include "nsIDocument.h"

struct JSContext;
class nsRefreshDriver;

namespace mozilla {
namespace dom {

class AnimationTimeline MOZ_FINAL : public nsWrapperCache
{
public:
  explicit AnimationTimeline(nsIDocument* aDocument)
    : mDocument(aDocument)
  {
  }

protected:
  virtual ~AnimationTimeline() { }

public:
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(AnimationTimeline)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(AnimationTimeline)

  nsIGlobalObject* GetParentObject() const
  {
    return mDocument->GetParentObject();
  }
  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  // AnimationTimeline methods
  Nullable<TimeDuration> GetCurrentTime() const;

  // Wrapper functions for AnimationTimeline DOM methods when called from
  // script.
  Nullable<double> GetCurrentTimeAsDouble() const;

  Nullable<TimeDuration> ToTimelineTime(const TimeStamp& aTimeStamp) const;
  TimeStamp ToTimeStamp(const TimeDuration& aTimelineTime) const;

  // Force the timeline to advance to |aTimeStamp|.
  //
  // Normally the timeline uses the refresh driver time but when we have
  // animations that are timed from when their first frame is rendered we need
  // to bring the timeline forward to that moment. If we don't, calling
  // IsRunning() will incorrectly return false (because GetCurrentTime() will
  // return a negative time) until the next refresh driver tick causes the
  // timeline to catch up.
  //
  // |aTimeStamp| must be greater or equal to the current refresh driver
  // time for the document with which this timeline is associated unless the
  // refresh driver is under test control, in which case this method will
  // be a no-op.
  void FastForward(const TimeStamp& aTimeStamp);

protected:
  TimeStamp GetCurrentTimeStamp() const;
  nsRefreshDriver* GetRefreshDriver() const;

  nsCOMPtr<nsIDocument> mDocument;

  // The most recently used refresh driver time. This is used in cases where
  // we don't have a refresh driver (e.g. because we are in a display:none
  // iframe).
  mutable TimeStamp mLastRefreshDriverTime;

  // The time to which the timeline has been forced-to in order to account for
  // animations that are started in-between frames.
  mutable TimeStamp mFastForwardTime;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_AnimationTimeline_h
