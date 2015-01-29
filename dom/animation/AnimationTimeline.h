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
#include "nsIGlobalObject.h"
#include "js/TypeDecls.h"
#include "nsIDocument.h"
#include "nsRefreshDriver.h"

struct JSContext;

namespace mozilla {
namespace dom {

class AnimationTimeline MOZ_FINAL : public nsWrapperCache
{
public:
  explicit AnimationTimeline(nsIDocument* aDocument)
    : mDocument(aDocument)
    , mWindow(aDocument->GetParentObject())
  {
    MOZ_ASSERT(mWindow);
  }

protected:
  virtual ~AnimationTimeline() { }

public:
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(AnimationTimeline)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(AnimationTimeline)

  nsIGlobalObject* GetParentObject() const
  {
    return mWindow;
  }
  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  // AnimationTimeline methods
  Nullable<TimeDuration> GetCurrentTime() const;

  // Wrapper functions for AnimationTimeline DOM methods when called from
  // script.
  Nullable<double> GetCurrentTimeAsDouble() const;

  // Converts a TimeStamp to the equivalent value in timeline time.
  // Note that when IsUnderTestControl() is true, there is no correspondence
  // between timeline time and wallclock time. In such a case, passing a
  // timestamp from TimeStamp::Now() to this method will not return a
  // meaningful result.
  Nullable<TimeDuration> ToTimelineTime(const TimeStamp& aTimeStamp) const;
  TimeStamp ToTimeStamp(const TimeDuration& aTimelineTime) const;

  nsRefreshDriver* GetRefreshDriver() const;
  // Returns true if this timeline is driven by a refresh driver that is
  // under test control. In such a case, there is no correspondence between
  // TimeStamp values returned by the refresh driver and wallclock time.
  // As a result, passing a value from TimeStamp::Now() to ToTimelineTime()
  // would not return a meaningful result.
  bool IsUnderTestControl() const
  {
    nsRefreshDriver* refreshDriver = GetRefreshDriver();
    return refreshDriver && refreshDriver->IsTestControllingRefreshesEnabled();
  }

protected:
  TimeStamp GetCurrentTimeStamp() const;

  // Sometimes documents can be given a new window, or windows can be given a
  // new document (e.g. document.open()). Since GetParentObject is required to
  // _always_ return the same object it can't get the window from our
  // mDocument, which is why we have pointers to both our document and window.
  nsCOMPtr<nsIDocument> mDocument;
  nsCOMPtr<nsIGlobalObject> mWindow;

  // The most recently used refresh driver time. This is used in cases where
  // we don't have a refresh driver (e.g. because we are in a display:none
  // iframe).
  mutable TimeStamp mLastRefreshDriverTime;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_AnimationTimeline_h
