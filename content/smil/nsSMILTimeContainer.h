/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Mozilla SMIL module.
 *
 * The Initial Developer of the Original Code is Brian Birtles.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brian Birtles <birtles@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef NS_SMILTIMECONTAINER_H_
#define NS_SMILTIMECONTAINER_H_

#include "nscore.h"
#include "nsSMILTypes.h"
#include "nsTPriorityQueue.h"
#include "nsAutoPtr.h"
#include "nsISMILAnimationElement.h"
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
  virtual void Pause(PRUint32 aType);

  /*
   * Resume this time container
   *
   * param @aType The source of the resume request. Clears the pause flag for
   * this particular type of pause request. When all pause flags have been
   * cleared the time container will be resumed.
   */
  virtual void Resume(PRUint32 aType);

  /**
   * Returns true if this time container is paused by the specified type.
   * Note that the time container may also be paused by other types; this method
   * does not test if aType is the exclusive pause source.
   *
   * @param @aType The pause source to test for.
   * @return PR_TRUE if this container is paused by aType.
   */
  bool IsPausedByType(PRUint32 aType) const { return mPauseState & aType; }

  /**
   * Returns true if this time container is paused.
   * Generally you should test for a specific type of pausing using
   * IsPausedByType.
   *
   * @return PR_TRUE if this container is paused, PR_FALSE otherwise.
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
  void ClearNeedsRewind() { mNeedsRewind = PR_FALSE; }

  /*
   * Indicates the time container is currently processing a SetCurrentTime
   * request and appropriate seek behaviour should be applied by child elements
   * (e.g. not firing time events).
   */
  bool IsSeeking() const { return mIsSeeking; }
  void MarkSeekFinished() { mIsSeeking = PR_FALSE; }

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
   * @return  PR_TRUE if the element was successfully added, PR_FALSE otherwise.
   */
  bool AddMilestone(const nsSMILMilestone& aMilestone,
                      nsISMILAnimationElement& aElement);

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
   * @return PR_TRUE if there exists another milestone, PR_FALSE otherwise in
   * which case aNextMilestone will be unmodified.
   */
  bool GetNextMilestoneInParentTime(nsSMILMilestone& aNextMilestone) const;

  typedef nsTArray<nsRefPtr<nsISMILAnimationElement> > AnimElemArray;

  /*
   * Removes and returns the timebase elements from the start of the list of
   * timebase elements that match the given time.
   *
   * @param      aMilestone  The milestone time to match in parent time. This
   *                         must be <= GetNextMilestoneInParentTime.
   * @param[out] aMatchedElements The array to which matching elements will be
   *                              appended.
   * @return PR_TRUE if one or more elements match, PR_FALSE otherwise.
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
  PRUint32 mPauseState;

  struct MilestoneEntry
  {
    MilestoneEntry(nsSMILMilestone aMilestone,
                   nsISMILAnimationElement& aElement)
      : mMilestone(aMilestone), mTimebase(&aElement)
    { }

    bool operator<(const MilestoneEntry& aOther) const
    {
      return mMilestone < aOther.mMilestone;
    }

    nsSMILMilestone mMilestone; // In container time.
    nsRefPtr<nsISMILAnimationElement> mTimebase;
  };

  // Queue of elements with registered milestones. Used to update the model with
  // significant transitions that occur between two samples. Since timed element
  // re-register their milestones when they're sampled this is reset once we've
  // taken care of the milestones before the current sample time but before we
  // actually do the full sample.
  nsTPriorityQueue<MilestoneEntry> mMilestoneEntries;
};

#endif // NS_SMILTIMECONTAINER_H_
