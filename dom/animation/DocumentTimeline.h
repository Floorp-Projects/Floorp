/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_DocumentTimeline_h
#define mozilla_dom_DocumentTimeline_h

#include "mozilla/dom/DocumentTimelineBinding.h"
#include "mozilla/TimeStamp.h"
#include "AnimationTimeline.h"
#include "nsIDocument.h"
#include "nsDOMNavigationTiming.h" // for DOMHighResTimeStamp
#include "nsRefreshDriver.h"

struct JSContext;

// GetCurrentTime is defined in winbase.h as zero argument macro forwarding to
// GetTickCount().
#ifdef GetCurrentTime
#undef GetCurrentTime
#endif

namespace mozilla {
namespace dom {

class DocumentTimeline final
  : public AnimationTimeline
  , public nsARefreshObserver
{
public:
  DocumentTimeline(nsIDocument* aDocument, const TimeDuration& aOriginTime)
    : AnimationTimeline(aDocument->GetParentObject())
    , mDocument(aDocument)
    , mIsObservingRefreshDriver(false)
    , mOriginTime(aOriginTime)
  {
  }

protected:
  virtual ~DocumentTimeline()
  {
    MOZ_ASSERT(!mIsObservingRefreshDriver, "Timeline should have disassociated"
               " from the refresh driver before being destroyed");
  }

public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(DocumentTimeline,
                                                         AnimationTimeline)

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  static already_AddRefed<DocumentTimeline>
  Constructor(const GlobalObject& aGlobal,
              const DocumentTimelineOptions& aOptions,
              ErrorResult& aRv);

  // AnimationTimeline methods
  virtual Nullable<TimeDuration> GetCurrentTime() const override;

  bool TracksWallclockTime() const override
  {
    nsRefreshDriver* refreshDriver = GetRefreshDriver();
    return !refreshDriver ||
           !refreshDriver->IsTestControllingRefreshesEnabled();
  }
  Nullable<TimeDuration> ToTimelineTime(const TimeStamp& aTimeStamp) const
                                                                     override;
  TimeStamp ToTimeStamp(const TimeDuration& aTimelineTime) const override;

  void NotifyAnimationUpdated(Animation& aAnimation) override;

  void RemoveAnimation(Animation* aAnimation) override;

  // nsARefreshObserver methods
  void WillRefresh(TimeStamp aTime) override;

  void NotifyRefreshDriverCreated(nsRefreshDriver* aDriver);
  void NotifyRefreshDriverDestroying(nsRefreshDriver* aDriver);

protected:
  TimeStamp GetCurrentTimeStamp() const;
  nsRefreshDriver* GetRefreshDriver() const;
  void UnregisterFromRefreshDriver();

  nsCOMPtr<nsIDocument> mDocument;

  // The most recently used refresh driver time. This is used in cases where
  // we don't have a refresh driver (e.g. because we are in a display:none
  // iframe).
  mutable TimeStamp mLastRefreshDriverTime;
  bool mIsObservingRefreshDriver;

  TimeDuration mOriginTime;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_DocumentTimeline_h
