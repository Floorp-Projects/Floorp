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
#include "nsTHashSet.h"

namespace mozilla::dom {

class Animation;
class Document;
class ScrollTimeline;

class AnimationTimeline : public nsISupports, public nsWrapperCache {
 public:
  explicit AnimationTimeline(nsIGlobalObject* aWindow) : mWindow(aWindow) {
    MOZ_ASSERT(mWindow);
  }

 protected:
  virtual ~AnimationTimeline();

  // Tick animations and may remove them from the list if we don't need to
  // tick it. Return true if any animations need to be ticked.
  bool Tick();

 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(AnimationTimeline)

  nsIGlobalObject* GetParentObject() const { return mWindow; }

  // AnimationTimeline methods
  virtual Nullable<TimeDuration> GetCurrentTimeAsDuration() const = 0;

  // Wrapper functions for AnimationTimeline DOM methods when called from
  // script.
  Nullable<double> GetCurrentTimeAsDouble() const {
    return AnimationUtils::TimeDurationToDouble(GetCurrentTimeAsDuration());
  }

  TimeStamp GetCurrentTimeAsTimeStamp() const {
    Nullable<TimeDuration> currentTime = GetCurrentTimeAsDuration();
    return !currentTime.IsNull() ? ToTimeStamp(currentTime.Value())
                                 : TimeStamp();
  }

  /**
   * Returns true if the times returned by GetCurrentTimeAsDuration() are
   * convertible to and from wallclock-based TimeStamp (e.g. from
   * TimeStamp::Now()) values using ToTimelineTime() and ToTimeStamp().
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
  virtual Nullable<TimeDuration> ToTimelineTime(
      const TimeStamp& aTimeStamp) const = 0;

  virtual TimeStamp ToTimeStamp(const TimeDuration& aTimelineTime) const = 0;

  /**
   * Inform this timeline that |aAnimation| which is or was observing the
   * timeline, has been updated. This serves as both the means to associate
   * AND disassociate animations with a timeline. The timeline itself will
   * determine if it needs to begin, continue or stop tracking this animation.
   */
  virtual void NotifyAnimationUpdated(Animation& aAnimation);

  /**
   * Returns true if any CSS animations, CSS transitions or Web animations are
   * currently associated with this timeline.  As soon as an animation is
   * applied to an element it is associated with the timeline even if it has a
   * delayed start, so this includes animations that may not be active for some
   * time.
   */
  bool HasAnimations() const { return !mAnimations.IsEmpty(); }

  virtual void RemoveAnimation(Animation* aAnimation);

  virtual Document* GetDocument() const = 0;

  virtual bool IsMonotonicallyIncreasing() const = 0;

  virtual bool IsScrollTimeline() const { return false; }
  virtual const ScrollTimeline* AsScrollTimeline() const { return nullptr; }

 protected:
  nsCOMPtr<nsIGlobalObject> mWindow;

  // Animations observing this timeline
  //
  // We store them in (a) a hashset for quick lookup, and (b) an array
  // to maintain a fixed sampling order.
  //
  // The hashset keeps a strong reference to each animation since
  // dealing with addref/release with LinkedList is difficult.
  typedef nsTHashSet<nsRefPtrHashKey<dom::Animation>> AnimationSet;
  AnimationSet mAnimations;
  LinkedList<dom::Animation> mAnimationOrder;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_AnimationTimeline_h
