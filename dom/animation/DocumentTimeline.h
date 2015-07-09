/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_DocumentTimeline_h
#define mozilla_dom_DocumentTimeline_h

#include "mozilla/TimeStamp.h"
#include "AnimationTimeline.h"
#include "nsIDocument.h"
#include "nsRefreshDriver.h"

struct JSContext;

namespace mozilla {
namespace dom {

class DocumentTimeline final : public AnimationTimeline
{
public:
  explicit DocumentTimeline(nsIDocument* aDocument)
    : AnimationTimeline(aDocument->GetParentObject())
    , mDocument(aDocument)
  {
  }

protected:
  virtual ~DocumentTimeline() { }

public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(DocumentTimeline,
                                                         AnimationTimeline)

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

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

protected:
  TimeStamp GetCurrentTimeStamp() const;
  nsRefreshDriver* GetRefreshDriver() const;

  nsCOMPtr<nsIDocument> mDocument;

  // The most recently used refresh driver time. This is used in cases where
  // we don't have a refresh driver (e.g. because we are in a display:none
  // iframe).
  mutable TimeStamp mLastRefreshDriverTime;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_DocumentTimeline_h
