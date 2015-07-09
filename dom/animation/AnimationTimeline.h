/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_AnimationTimeline_h
#define mozilla_dom_AnimationTimeline_h

#include "nsISupports.h"
#include "nsWrapperCache.h"
#include "nsCycleCollectionParticipant.h"
#include "js/TypeDecls.h"
#include "mozilla/AnimationUtils.h"
#include "mozilla/Attributes.h"
#include "nsHashKeys.h"
#include "nsIGlobalObject.h"
#include "nsTHashtable.h"

namespace mozilla {
namespace dom {

class AnimationTimeline
  : public nsISupports
  , public nsWrapperCache
{
public:
  explicit AnimationTimeline(nsIGlobalObject* aWindow)
    : mWindow(aWindow)
  {
    MOZ_ASSERT(mWindow);
  }

protected:
  virtual ~AnimationTimeline() { }

public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(AnimationTimeline)

  nsIGlobalObject* GetParentObject() const { return mWindow; }

  typedef nsTArray<nsRefPtr<Animation>> AnimationSequence;

  // AnimationTimeline methods
  virtual Nullable<TimeDuration> GetCurrentTime() const = 0;
  void GetAnimations(AnimationSequence& aAnimations);

  // Wrapper functions for AnimationTimeline DOM methods when called from
  // script.
  Nullable<double> GetCurrentTimeAsDouble() const {
    return AnimationUtils::TimeDurationToDouble(GetCurrentTime());
  }

  /**
   * Returns true if the times returned by GetCurrentTime() are convertible
   * to and from wallclock-based TimeStamp (e.g. from TimeStamp::Now()) values
   * using ToTimelineTime() and ToTimeStamp().
   *
   * Typically this is true, but it will be false in the case when this
   * timeline has no refresh driver or is tied to a refresh driver under test
   * control.
   */
  virtual bool TracksWallclockTime() const = 0;

  /**
   * Converts a TimeStamp to the equivalent value in timeline time.
   * Note that when TracksWallclockTime() is false, there is no correspondence
   * between timeline time and wallclock time. In such a case, passing a
   * timestamp from TimeStamp::Now() to this method will not return a
   * meaningful result.
   */
  virtual Nullable<TimeDuration> ToTimelineTime(const TimeStamp&
                                                  aTimeStamp) const = 0;

  virtual TimeStamp ToTimeStamp(const TimeDuration& aTimelineTime) const = 0;

  void AddAnimation(Animation& aAnimation);
  void RemoveAnimation(Animation& aAnimation);

protected:
  nsCOMPtr<nsIGlobalObject> mWindow;

  // Animations observing this timeline
  typedef nsTHashtable<nsRefPtrHashKey<dom::Animation>> AnimationSet;
  AnimationSet mAnimations;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_AnimationTimeline_h
