/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NS_SMILTIMEDELEMENT_H_
#define NS_SMILTIMEDELEMENT_H_

#include "mozilla/Move.h"
#include "nsSMILInterval.h"
#include "nsSMILInstanceTime.h"
#include "nsSMILMilestone.h"
#include "nsSMILTimeValueSpec.h"
#include "nsSMILRepeatCount.h"
#include "nsSMILTypes.h"
#include "nsTArray.h"
#include "nsTHashtable.h"
#include "nsHashKeys.h"
#include "nsAutoPtr.h"
#include "nsAttrValue.h"

class nsSMILAnimationFunction;
class nsSMILTimeContainer;
class nsSMILTimeValue;
class nsIAtom;

namespace mozilla {
namespace dom {
class SVGAnimationElement;
}
}

//----------------------------------------------------------------------
// nsSMILTimedElement

class nsSMILTimedElement
{
public:
  nsSMILTimedElement();
  ~nsSMILTimedElement();

  typedef mozilla::dom::Element Element;

  /*
   * Sets the owning animation element which this class uses to convert between
   * container times and to register timebase elements.
   */
  void SetAnimationElement(mozilla::dom::SVGAnimationElement* aElement);

  /*
   * Returns the time container with which this timed element is associated or
   * nullptr if it is not associated with a time container.
   */
  nsSMILTimeContainer* GetTimeContainer();

  /*
   * Returns the element targeted by the animation element. Needed for
   * registering event listeners against the appropriate element.
   */
  mozilla::dom::Element* GetTargetElement();

  /**
   * Methods for supporting the nsIDOMElementTimeControl interface.
   */

  /*
   * Adds a new begin instance time at the current container time plus or minus
   * the specified offset.
   *
   * @param aOffsetSeconds A real number specifying the number of seconds to add
   *                       to the current container time.
   * @return NS_OK if the operation succeeeded, or an error code otherwise.
   */
  nsresult BeginElementAt(double aOffsetSeconds);

  /*
   * Adds a new end instance time at the current container time plus or minus
   * the specified offset.
   *
   * @param aOffsetSeconds A real number specifying the number of seconds to add
   *                       to the current container time.
   * @return NS_OK if the operation succeeeded, or an error code otherwise.
   */
  nsresult EndElementAt(double aOffsetSeconds);

  /**
   * Methods for supporting the nsSVGAnimationElement interface.
   */

  /**
   * According to SVG 1.1 SE this returns
   *
   *   the begin time, in seconds, for this animation element's current
   *   interval, if it exists, regardless of whether the interval has begun yet.
   *
   * @return the start time as defined above in milliseconds or an unresolved
   * time if there is no current interval.
   */
  nsSMILTimeValue GetStartTime() const;

  /**
   * Returns the simple duration of this element.
   *
   * @return the simple duration in milliseconds or INDEFINITE.
   */
  nsSMILTimeValue GetSimpleDuration() const
  {
    return mSimpleDur;
  }

  /**
   * Methods for supporting hyperlinking
   */

  /**
   * Internal SMIL methods
   */

  /**
   * Returns the time to seek the document to when this element is targetted by
   * a hyperlink.
   *
   * The behavior is defined here:
   *   http://www.w3.org/TR/smil-animation/#HyperlinkSemantics
   *
   * It is very similar to GetStartTime() with the exception that when the
   * element is not active, the begin time of the *first* interval is returned.
   *
   * @return the time to seek the documen to in milliseconds or an unresolved
   * time if there is no resolved interval.
   */
  nsSMILTimeValue GetHyperlinkTime() const;

  /**
   * Adds an instance time object this element's list of instance times.
   * These instance times are used when creating intervals.
   *
   * This method is typically called by an nsSMILTimeValueSpec.
   *
   * @param aInstanceTime   The time to add, expressed in container time.
   * @param aIsBegin        true if the time to be added represents a begin
   *                        time or false if it represents an end time.
   */
  void AddInstanceTime(nsSMILInstanceTime* aInstanceTime, bool aIsBegin);

  /**
   * Requests this element update the given instance time.
   *
   * This method is typically called by a child nsSMILTimeValueSpec.
   *
   * @param aInstanceTime   The instance time to update.
   * @param aUpdatedTime    The time to update aInstanceTime with.
   * @param aDependentTime  The instance time upon which aInstanceTime should be
   *                        based.
   * @param aIsBegin        true if the time to be updated represents a begin
   *                        instance time or false if it represents an end
   *                        instance time.
   */
  void UpdateInstanceTime(nsSMILInstanceTime* aInstanceTime,
                          nsSMILTimeValue& aUpdatedTime,
                          bool aIsBegin);

  /**
   * Removes an instance time object from this element's list of instance times.
   *
   * This method is typically called by a child nsSMILTimeValueSpec.
   *
   * @param aInstanceTime   The instance time to remove.
   * @param aIsBegin        true if the time to be removed represents a begin
   *                        time or false if it represents an end time.
   */
  void RemoveInstanceTime(nsSMILInstanceTime* aInstanceTime, bool aIsBegin);

  /**
   * Removes all the instance times associated with the given
   * nsSMILTimeValueSpec object. Used when an ID assignment changes and hence
   * all the previously associated instance times become invalid.
   *
   * @param aSpec    The nsSMILTimeValueSpec object whose created
   *                 nsSMILInstanceTime's should be removed.
   * @param aIsBegin true if the times to be removed represent begin
   *                 times or false if they are end times.
   */
  void RemoveInstanceTimesForCreator(const nsSMILTimeValueSpec* aSpec,
                                     bool aIsBegin);

  /**
   * Sets the object that will be called by this timed element each time it is
   * sampled.
   *
   * In Schmitz's model it is possible to associate several time clients with
   * a timed element but for now we only allow one.
   *
   * @param aClient   The time client to associate. Any previous time client
   *                  will be disassociated and no longer sampled. Setting this
   *                  to nullptr will simply disassociate the previous client, if
   *                  any.
   */
  void SetTimeClient(nsSMILAnimationFunction* aClient);

  /**
   * Samples the object at the given container time. Timing intervals are
   * updated and if this element is active at the given time the associated time
   * client will be sampled with the appropriate simple time.
   *
   * @param aContainerTime The container time at which to sample.
   */
  void SampleAt(nsSMILTime aContainerTime);

  /**
   * Performs a special sample for the end of an interval. Such a sample should
   * only advance the timed element (and any dependent elements) to the waiting
   * or postactive state. It should not cause a transition to the active state.
   * Transition to the active state is only performed on a regular SampleAt.
   *
   * This allows all interval ends at a given time to be processed first and
   * hence the new interval can be established based on full information of the
   * available instance times.
   *
   * @param aContainerTime The container time at which to sample.
   */
  void SampleEndAt(nsSMILTime aContainerTime);

  /**
   * Informs the timed element that its time container has changed time
   * relative to document time. The timed element therefore needs to update its
   * dependent elements (which may belong to a different time container) so they
   * can re-resolve their times.
   */
  void HandleContainerTimeChange();

  /**
   * Resets this timed element's accumulated times and intervals back to start
   * up state.
   *
   * This is used for backwards seeking where rather than accumulating
   * historical timing state and winding it back, we reset the element and seek
   * forwards.
   */
  void Rewind();

  /**
   * Marks this element as disabled or not. If the element is disabled, it
   * will ignore any future samples and discard any accumulated timing state.
   *
   * This is used by SVG to "turn off" timed elements when the associated
   * animation element has failing conditional processing tests.
   *
   * Returns true if the disabled state of the timed element was changed
   * as a result of this call (i.e. it was not a redundant call).
   */
  bool SetIsDisabled(bool aIsDisabled);

  /**
   * Attempts to set an attribute on this timed element.
   *
   * @param aAttribute  The name of the attribute to set. The namespace of this
   *                    attribute is not specified as it is checked by the host
   *                    element. Only attributes in the namespace defined for
   *                    SMIL attributes in the host language are passed to the
   *                    timed element.
   * @param aValue      The attribute value.
   * @param aResult     The nsAttrValue object that may be used for storing the
   *                    parsed result.
   * @param aContextNode The element to use for context when resolving
   *                     references to other elements.
   * @param[out] aParseResult The result of parsing the attribute. Will be set
   *                          to NS_OK if parsing is successful.
   *
   * @return true if the given attribute is a timing attribute, false
   * otherwise.
   */
  bool SetAttr(nsIAtom* aAttribute, const nsAString& aValue,
                 nsAttrValue& aResult, Element* aContextNode,
                 nsresult* aParseResult = nullptr);

  /**
   * Attempts to unset an attribute on this timed element.
   *
   * @param aAttribute  The name of the attribute to set. As with SetAttr the
   *                    namespace of the attribute is not specified (see
   *                    SetAttr).
   *
   * @return true if the given attribute is a timing attribute, false
   * otherwise.
   */
  bool UnsetAttr(nsIAtom* aAttribute);

  /**
   * Adds a syncbase dependency to the list of dependents that will be notified
   * when this timed element creates, deletes, or updates its current interval.
   *
   * @param aDependent  The nsSMILTimeValueSpec object to notify. A raw pointer
   *                    to this object will be stored. Therefore it is necessary
   *                    for the object to be explicitly unregistered (with
   *                    RemoveDependent) when it is destroyed.
   */
  void AddDependent(nsSMILTimeValueSpec& aDependent);

  /**
   * Removes a syncbase dependency from the list of dependents that are notified
   * when the current interval is modified.
   *
   * @param aDependent  The nsSMILTimeValueSpec object to unregister.
   */
  void RemoveDependent(nsSMILTimeValueSpec& aDependent);

  /**
   * Determines if this timed element is dependent on the given timed element's
   * begin time for the interval currently in effect. Whilst the element is in
   * the active state this is the current interval and in the postactive or
   * waiting state this is the previous interval if one exists. In all other
   * cases the element is not considered a time dependent of any other element.
   *
   * @param aOther    The potential syncbase element.
   * @return true if this timed element's begin time for the currently
   * effective interval is directly or indirectly derived from aOther, false
   * otherwise.
   */
  bool IsTimeDependent(const nsSMILTimedElement& aOther) const;

  /**
   * Called when the timed element has been bound to the document so that
   * references from this timed element to other elements can be resolved.
   *
   * @param aContextNode  The node which provides the necessary context for
   *                      resolving references. This is typically the element in
   *                      the host language that owns this timed element. Should
   *                      not be null.
   */
  void BindToTree(nsIContent* aContextNode);

  /**
   * Called when the target of the animation has changed so that event
   * registrations can be updated.
   */
  void HandleTargetElementChange(mozilla::dom::Element* aNewTarget);

  /**
   * Called when the timed element has been removed from a document so that
   * references to other elements can be broken.
   */
  void DissolveReferences() { Unlink(); }

  // Cycle collection
  void Traverse(nsCycleCollectionTraversalCallback* aCallback);
  void Unlink();

  typedef bool (*RemovalTestFunction)(nsSMILInstanceTime* aInstance);

protected:
  // Typedefs
  typedef nsTArray<nsAutoPtr<nsSMILTimeValueSpec> > TimeValueSpecList;
  typedef nsTArray<nsRefPtr<nsSMILInstanceTime> >   InstanceTimeList;
  typedef nsTArray<nsAutoPtr<nsSMILInterval> >      IntervalList;
  typedef nsPtrHashKey<nsSMILTimeValueSpec> TimeValueSpecPtrKey;
  typedef nsTHashtable<TimeValueSpecPtrKey> TimeValueSpecHashSet;

  // Helper classes
  class InstanceTimeComparator {
    public:
      bool Equals(const nsSMILInstanceTime* aElem1,
                    const nsSMILInstanceTime* aElem2) const;
      bool LessThan(const nsSMILInstanceTime* aElem1,
                      const nsSMILInstanceTime* aElem2) const;
  };

  struct NotifyTimeDependentsParams {
    nsSMILTimedElement*  mTimedElement;
    nsSMILTimeContainer* mTimeContainer;
  };

  // Templated helper functions
  template <class TestFunctor>
  void RemoveInstanceTimes(InstanceTimeList& aArray, TestFunctor& aTest);

  //
  // Implementation helpers
  //

  nsresult          SetBeginSpec(const nsAString& aBeginSpec,
                                 Element* aContextNode,
                                 RemovalTestFunction aRemove);
  nsresult          SetEndSpec(const nsAString& aEndSpec,
                               Element* aContextNode,
                               RemovalTestFunction aRemove);
  nsresult          SetSimpleDuration(const nsAString& aDurSpec);
  nsresult          SetMin(const nsAString& aMinSpec);
  nsresult          SetMax(const nsAString& aMaxSpec);
  nsresult          SetRestart(const nsAString& aRestartSpec);
  nsresult          SetRepeatCount(const nsAString& aRepeatCountSpec);
  nsresult          SetRepeatDur(const nsAString& aRepeatDurSpec);
  nsresult          SetFillMode(const nsAString& aFillModeSpec);

  void              UnsetBeginSpec(RemovalTestFunction aRemove);
  void              UnsetEndSpec(RemovalTestFunction aRemove);
  void              UnsetSimpleDuration();
  void              UnsetMin();
  void              UnsetMax();
  void              UnsetRestart();
  void              UnsetRepeatCount();
  void              UnsetRepeatDur();
  void              UnsetFillMode();

  nsresult          SetBeginOrEndSpec(const nsAString& aSpec,
                                      Element* aContextNode,
                                      bool aIsBegin,
                                      RemovalTestFunction aRemove);
  void              ClearSpecs(TimeValueSpecList& aSpecs,
                               InstanceTimeList& aInstances,
                               RemovalTestFunction aRemove);
  void              ClearIntervals();
  void              DoSampleAt(nsSMILTime aContainerTime, bool aEndOnly);

  /**
   * Helper function to check for an early end and, if necessary, update the
   * current interval accordingly.
   *
   * See SMIL 3.0, section 5.4.5, Element life cycle, "Active Time - Playing an
   * interval" for a description of ending early.
   *
   * @param aSampleTime The current sample time. Early ends should only be
   *                    applied at the last possible moment (i.e. if they are at
   *                    or before the current sample time) and only if the
   *                    current interval is not already ending.
   * @return true if the end time of the current interval was updated,
   *         false otherwise.
   */
  bool ApplyEarlyEnd(const nsSMILTimeValue& aSampleTime);

  /**
   * Clears certain state in response to the element restarting.
   *
   * This state is described in SMIL 3.0, section 5.4.3, Resetting element state
   */
  void Reset();

  /**
   * Clears all accumulated timing state except for those instance times for
   * which aRemove does not return true.
   *
   * Unlike the Reset method which only clears instance times, this clears the
   * element's state, intervals (including current interval), and tells the
   * client animation function to stop applying a result. In effect, it returns
   * the element to its initial state but preserves any instance times excluded
   * by the passed-in function.
   */
  void ClearTimingState(RemovalTestFunction aRemove);

  /**
   * Recreates timing state by re-applying begin/end attributes specified on
   * the associated animation element.
   *
   * Note that this does not completely restore the information cleared by
   * ClearTimingState since it leaves the element in the startup state.
   * The element state will be updated on the next sample.
   */
  void RebuildTimingState(RemovalTestFunction aRemove);

  /**
   * Completes a seek operation by sending appropriate events and, in the case
   * of a backwards seek, updating the state of timing information that was
   * previously considered historical.
   */
  void DoPostSeek();

  /**
   * Unmarks instance times that were previously preserved because they were
   * considered important historical milestones but are no longer such because
   * a backwards seek has been performed.
   */
  void UnpreserveInstanceTimes(InstanceTimeList& aList);

  /**
   * Helper function to iterate through this element's accumulated timing
   * information (specifically old nsSMILIntervals and nsSMILTimeInstanceTimes)
   * and discard items that are no longer needed or exceed some threshold of
   * accumulated state.
   */
  void FilterHistory();

  // Helper functions for FilterHistory to clear old nsSMILIntervals and
  // nsSMILInstanceTimes respectively.
  void FilterIntervals();
  void FilterInstanceTimes(InstanceTimeList& aList);

  /**
   * Calculates the next acceptable interval for this element after the
   * specified interval, or, if no previous interval is specified, it will be
   * the first interval with an end time after t=0.
   *
   * @see SMILANIM 3.6.8
   *
   * @param aPrevInterval   The previous interval used. If supplied, the first
   *                        interval that begins after aPrevInterval will be
   *                        returned. May be nullptr.
   * @param aReplacedInterval The interval that is being updated (if any). This
   *                        used to ensure we don't return interval endpoints
   *                        that are dependent on themselves. May be nullptr.
   * @param aFixedBeginTime The time to use for the start of the interval. This
   *                        is used when only the endpoint of the interval
   *                        should be updated such as when the animation is in
   *                        the ACTIVE state. May be nullptr.
   * @param[out] aResult    The next interval. Will be unchanged if no suitable
   *                        interval was found (in which case false will be
   *                        returned).
   * @return  true if a suitable interval was found, false otherwise.
   */
  bool              GetNextInterval(const nsSMILInterval* aPrevInterval,
                                    const nsSMILInterval* aReplacedInterval,
                                    const nsSMILInstanceTime* aFixedBeginTime,
                                    nsSMILInterval& aResult) const;
  nsSMILInstanceTime* GetNextGreater(const InstanceTimeList& aList,
                                     const nsSMILTimeValue& aBase,
                                     int32_t& aPosition) const;
  nsSMILInstanceTime* GetNextGreaterOrEqual(const InstanceTimeList& aList,
                                            const nsSMILTimeValue& aBase,
                                            int32_t& aPosition) const;
  nsSMILTimeValue   CalcActiveEnd(const nsSMILTimeValue& aBegin,
                                  const nsSMILTimeValue& aEnd) const;
  nsSMILTimeValue   GetRepeatDuration() const;
  nsSMILTimeValue   ApplyMinAndMax(const nsSMILTimeValue& aDuration) const;
  nsSMILTime        ActiveTimeToSimpleTime(nsSMILTime aActiveTime,
                                           uint32_t& aRepeatIteration);
  nsSMILInstanceTime* CheckForEarlyEnd(
                        const nsSMILTimeValue& aContainerTime) const;
  void              UpdateCurrentInterval(bool aForceChangeNotice = false);
  void              SampleSimpleTime(nsSMILTime aActiveTime);
  void              SampleFillValue();
  nsresult          AddInstanceTimeFromCurrentTime(nsSMILTime aCurrentTime,
                        double aOffsetSeconds, bool aIsBegin);
  void              RegisterMilestone();
  bool              GetNextMilestone(nsSMILMilestone& aNextMilestone) const;

  // Notification methods. Note that these notifications can result in nested
  // calls to this same object. Therefore,
  // (i)  we should not perform notification until this object is in
  //      a consistent state to receive callbacks, and
  // (ii) after calling these methods we must assume that the state of the
  //      element may have changed.
  void              NotifyNewInterval();
  void              NotifyChangedInterval(nsSMILInterval* aInterval,
                                          bool aBeginObjectChanged,
                                          bool aEndObjectChanged);

  void              FireTimeEventAsync(uint32_t aMsg, int32_t aDetail);
  const nsSMILInstanceTime* GetEffectiveBeginInstance() const;
  const nsSMILInterval* GetPreviousInterval() const;
  bool              HasPlayed() const { return !mOldIntervals.IsEmpty(); }
  bool              HasClientInFillRange() const;
  bool              EndHasEventConditions() const;
  bool              AreEndTimesDependentOn(
                      const nsSMILInstanceTime* aBase) const;

  // Reset the current interval by first passing ownership to a temporary
  // variable so that if Unlink() results in us receiving a callback,
  // mCurrentInterval will be nullptr and we will be in a consistent state.
  void ResetCurrentInterval()
  {
    if (mCurrentInterval) {
      // Transfer ownership to temp var. (This sets mCurrentInterval to null.)
      nsAutoPtr<nsSMILInterval> interval(mozilla::Move(mCurrentInterval));
      interval->Unlink();
    }
  }

  // Hashtable callback methods
  static PLDHashOperator NotifyNewIntervalCallback(
      TimeValueSpecPtrKey* aKey, void* aData);

  //
  // Members
  //
  mozilla::dom::SVGAnimationElement* mAnimationElement; // [weak] won't outlive
                                                        // owner
  TimeValueSpecList               mBeginSpecs; // [strong]
  TimeValueSpecList               mEndSpecs; // [strong]

  nsSMILTimeValue                 mSimpleDur;

  nsSMILRepeatCount               mRepeatCount;
  nsSMILTimeValue                 mRepeatDur;

  nsSMILTimeValue                 mMin;
  nsSMILTimeValue                 mMax;

  enum nsSMILFillMode
  {
    FILL_REMOVE,
    FILL_FREEZE
  };
  nsSMILFillMode                  mFillMode;
  static nsAttrValue::EnumTable   sFillModeTable[];

  enum nsSMILRestartMode
  {
    RESTART_ALWAYS,
    RESTART_WHENNOTACTIVE,
    RESTART_NEVER
  };
  nsSMILRestartMode               mRestartMode;
  static nsAttrValue::EnumTable   sRestartModeTable[];

  InstanceTimeList                mBeginInstances;
  InstanceTimeList                mEndInstances;
  uint32_t                        mInstanceSerialIndex;

  nsSMILAnimationFunction*        mClient;
  nsAutoPtr<nsSMILInterval>       mCurrentInterval;
  IntervalList                    mOldIntervals;
  uint32_t                        mCurrentRepeatIteration;
  nsSMILMilestone                 mPrevRegisteredMilestone;
  static const nsSMILMilestone    sMaxMilestone;
  static const uint8_t            sMaxNumIntervals;
  static const uint8_t            sMaxNumInstanceTimes;

  // Set of dependent time value specs to be notified when establishing a new
  // current interval. Change notifications and delete notifications are handled
  // by the interval.
  //
  // [weak] The nsSMILTimeValueSpec objects register themselves and unregister
  // on destruction. Likewise, we notify them when we are destroyed.
  TimeValueSpecHashSet mTimeDependents;

  /**
   * The state of the element in its life-cycle. These states are based on the
   * element life-cycle described in SMILANIM 3.6.8
   */
  enum nsSMILElementState
  {
    STATE_STARTUP,
    STATE_WAITING,
    STATE_ACTIVE,
    STATE_POSTACTIVE
  };
  nsSMILElementState              mElementState;

  enum nsSMILSeekState
  {
    SEEK_NOT_SEEKING,
    SEEK_FORWARD_FROM_ACTIVE,
    SEEK_FORWARD_FROM_INACTIVE,
    SEEK_BACKWARD_FROM_ACTIVE,
    SEEK_BACKWARD_FROM_INACTIVE
  };
  nsSMILSeekState                 mSeekState;

  // Used to batch updates to the timing model
  class AutoIntervalUpdateBatcher;
  bool mDeferIntervalUpdates;
  bool mDoDeferredUpdate; // Set if an update to the current interval was
                          // requested while mDeferIntervalUpdates was set
  bool mIsDisabled;

  // Stack-based helper class to call UpdateCurrentInterval when it is destroyed
  class AutoIntervalUpdater;

  // Recursion depth checking
  uint8_t              mDeleteCount;
  uint8_t              mUpdateIntervalRecursionDepth;
  static const uint8_t sMaxUpdateIntervalRecursionDepth;
};

inline void
ImplCycleCollectionUnlink(nsSMILTimedElement& aField)
{
  aField.Unlink();
}

inline void
ImplCycleCollectionTraverse(nsCycleCollectionTraversalCallback& aCallback,
                            nsSMILTimedElement& aField,
                            const char* aName,
                            uint32_t aFlags = 0)
{
  aField.Traverse(&aCallback);
}

#endif // NS_SMILTIMEDELEMENT_H_
