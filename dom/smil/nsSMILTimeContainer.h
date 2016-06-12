/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NS_SMILTIMECONTAINER_H_
#define NS_SMILTIMECONTAINER_H_

#include "mozilla/dom/SVGAnimationElement.h"
#include "nscore.h"
#include "nsSMILTypes.h"
#include "nsTPriorityQueue.h"
#include "nsSMILMilestone.h"

class nsSMILTimeValue;

//----------------------------------------------------------------------
// nsSMILTimeContainer
//
// Common base class for a time base that can be paused, resumed, and sampled.
//
class nsSMILTimeContainer
{
public:
  nsSMILTimeContainer();
  virtual ~nsSMILTimeContainer();

  /*
   * Pause request types.
   */
  enum {
    PAUSE_BEGIN    =  1, // Paused because timeline has yet to begin.
    PAUSE_SCRIPT   =  2, // Paused by script.
    PAUSE_PAGEHIDE =  4, // Paused because our doc is hidden.
    PAUSE_USERPREF =  8, // Paused because animations are disabled in prefs.
    PAUSE_IMAGE    = 16  // Paused becuase we're in an image that's suspended.
  };

  /*
   * Cause the time container to record its begin time.
   */
  void Begin();

  /*
   * Pause this time container
   *
   * @param aType The source of the pause request. Successive calls to Pause
   * with the same aType will be ignored. The container will remain paused until
   * each call to Pause of a given aType has been matched by at least one call
   * to Resume with the same aType.
   */
  virtual void Pause(uint32_t aType);

  /*
   * Resume this time container
   *
   * param @aType The source of the resume request. Clears the pause flag for
   * this particular type of pause request. When all pause flags have been
   * cleared the time container will be resumed.
   */
  virtual void Resume(uint32_t aType);

  /**
   * Returns true if this time container is paused by the specified type.
   * Note that the time container may also be paused by other types; this method
   * does not test if aType is the exclusive pause source.
   *
   * @param @aType The pause source to test for.
   * @return true if this container is paused by aType.
   */
  bool IsPausedByType(uint32_t aType) const { return mPauseState & aType; }

  /**
   * Returns true if this time container is paused.
   * Generally you should test for a specific type of pausing using
   * IsPausedByType.
   *
   * @return true if this container is paused, false otherwise.
   */
  bool IsPaused() const { return mPauseState != 0; }

  /*
   * Return the time elapsed since this time container's begin time (expressed
   * in parent time) minus any accumulated offset from pausing.
   */
  nsSMILTime GetCurrentTime() const;

  /*
   * Seek the document timeline to the specified time.
   *
   * @param aSeekTo The time to seek to, expressed in this time container's time
   * base (i.e. the same units as GetCurrentTime).
   */
  void SetCurrentTime(nsSMILTime aSeekTo);

  /*
   * Return the current time for the parent time container if any.
   */
  virtual nsSMILTime GetParentTime() const;

  /*
   * Convert container time to parent time.
   *
   * @param   aContainerTime The container time to convert.
   * @return  The equivalent parent time or indefinite if the container is
   *          paused and the time is in the future.
   */
  nsSMILTimeValue ContainerToParentTime(nsSMILTime aContainerTime) const;

  /*
   * Convert from parent time to container time.
   *
   * @param   aParentTime The parent time to convert.
   * @return  The equivalent container time or indefinite if the container is
   *          paused and aParentTime is after the time when the pause began.
   */
  nsSMILTimeValue ParentToContainerTime(nsSMILTime aParentTime) const;

  /*
   * If the container is paused, causes the pause time to be updated to the
   * current parent time. This should be called before updating
   * cross-container dependencies that will call ContainerToParentTime in order
   * to provide more intuitive results.
   */
  void SyncPauseTime();

  /*
   * Updates the current time of this time container and calls DoSample to
   * perform any sample-operations.
   */
  void Sample();

  /*
   * Return if this time container should be sampled or can be skipped.
   *
   * This is most useful as an optimisation for skipping time containers that
   * don't require a sample.
   */
  bool NeedsSample() const { return !mPauseState || mNeedsPauseSample; }

  /*
   * Indicates if the elements of this time container need to be rewound.
   * This occurs during a backwards seek.
   */
  bool NeedsRewind() const { return mNeedsRewind; }
  void ClearNeedsRewind() { mNeedsRewind = false; }

  /*
   * Indicates the time container is currently processing a SetCurrentTime
   * request and appropriate seek behaviour should be applied by child elements
   * (e.g. not firing time events).
   */
  bool IsSeeking() const { return mIsSeeking; }
  void MarkSeekFinished() { mIsSeeking = false; }

  /*
   * Sets the parent time container.
   *
   * The callee still retains ownership of the time container.
   */
  nsresult SetParent(nsSMILTimeContainer* aParent);

  /*
   * Registers an element for a sample at the given time.
   *
   * @param   aMilestone  The milestone to register in container time.
   * @param   aElement    The timebase element that needs a sample at
   *                      aMilestone.
   * @return  true if the element was successfully added, false otherwise.
   */
  bool AddMilestone(const nsSMILMilestone& aMilestone,
                    mozilla::dom::SVGAnimationElement& aElement);

  /*
   * Resets the list of milestones.
   */
  void ClearMilestones();

  /*
   * Returns the next significant transition from amongst the registered
   * milestones.
   *
   * @param[out] aNextMilestone The next milestone with time in parent time.
   *
   * @return true if there exists another milestone, false otherwise in
   * which case aNextMilestone will be unmodified.
   */
  bool GetNextMilestoneInParentTime(nsSMILMilestone& aNextMilestone) const;

  typedef nsTArray<RefPtr<mozilla::dom::SVGAnimationElement> > AnimElemArray;

  /*
   * Removes and returns the timebase elements from the start of the list of
   * timebase elements that match the given time.
   *
   * @param      aMilestone  The milestone time to match in parent time. This
   *                         must be <= GetNextMilestoneInParentTime.
   * @param[out] aMatchedElements The array to which matching elements will be
   *                              appended.
   * @return true if one or more elements match, false otherwise.
   */
  bool PopMilestoneElementsAtMilestone(const nsSMILMilestone& aMilestone,
                                         AnimElemArray& aMatchedElements);

  // Cycle-collection support
  void Traverse(nsCycleCollectionTraversalCallback* aCallback);
  void Unlink();

protected:
  /*
   * Per-sample operations to be performed whenever Sample() is called and
   * NeedsSample() is true. Called after updating mCurrentTime;
   */
  virtual void DoSample() { }

  /*
   * Adding and removing child containers is not implemented in the base class
   * because not all subclasses need this.
   */

  /*
   * Adds a child time container.
   */
  virtual nsresult AddChild(nsSMILTimeContainer& aChild)
  {
    return NS_ERROR_FAILURE;
  }

  /*
   * Removes a child time container.
   */
  virtual void RemoveChild(nsSMILTimeContainer& aChild) { }

  /*
   * Implementation helper to update the current time.
   */
  void UpdateCurrentTime();

  /*
   * Implementation helper to notify timed elements with dependencies that the
   * container time has changed with respect to the document time.
   */
  void NotifyTimeChange();

  // The parent time container, if any
  nsSMILTimeContainer* mParent;

  // The current time established at the last call to Sample()
  nsSMILTime mCurrentTime;

  // The number of milliseconds for which the container has been paused
  // (excluding the current pause interval if the container is currently
  // paused).
  //
  //  Current time = parent time - mParentOffset
  //
  nsSMILTime mParentOffset;

  // The timestamp in parent time when the container was paused
  nsSMILTime mPauseStart;

  // Whether or not a pause sample is required
  bool mNeedsPauseSample;

  bool mNeedsRewind; // Backwards seek performed
  bool mIsSeeking; // Currently in the middle of a seek operation

  // A bitfield of the pause state for all pause requests
  uint32_t mPauseState;

  struct MilestoneEntry
  {
    MilestoneEntry(nsSMILMilestone aMilestone,
                   mozilla::dom::SVGAnimationElement& aElement)
      : mMilestone(aMilestone), mTimebase(&aElement)
    { }

    bool operator<(const MilestoneEntry& aOther) const
    {
      return mMilestone < aOther.mMilestone;
    }

    nsSMILMilestone mMilestone; // In container time.
    RefPtr<mozilla::dom::SVGAnimationElement> mTimebase;
  };

  // Queue of elements with registered milestones. Used to update the model with
  // significant transitions that occur between two samples. Since timed element
  // re-register their milestones when they're sampled this is reset once we've
  // taken care of the milestones before the current sample time but before we
  // actually do the full sample.
  nsTPriorityQueue<MilestoneEntry> mMilestoneEntries;
};

#endif // NS_SMILTIMECONTAINER_H_
