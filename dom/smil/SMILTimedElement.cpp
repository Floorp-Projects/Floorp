/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SMILTimedElement.h"

#include "mozilla/AutoRestore.h"
#include "mozilla/ContentEvents.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/SMILAnimationFunction.h"
#include "mozilla/SMILInstanceTime.h"
#include "mozilla/SMILParserUtils.h"
#include "mozilla/SMILTimeContainer.h"
#include "mozilla/SMILTimeValue.h"
#include "mozilla/SMILTimeValueSpec.h"
#include "mozilla/TaskCategory.h"
#include "mozilla/dom/DocumentInlines.h"
#include "mozilla/dom/SVGAnimationElement.h"
#include "nsAttrValueInlines.h"
#include "nsGkAtoms.h"
#include "nsReadableUtils.h"
#include "nsMathUtils.h"
#include "nsThreadUtils.h"
#include "prdtoa.h"
#include "plstr.h"
#include "prtime.h"
#include "nsString.h"
#include "nsCharSeparatedTokenizer.h"
#include <algorithm>

using namespace mozilla::dom;

namespace mozilla {

//----------------------------------------------------------------------
// Helper class: InstanceTimeComparator

// Upon inserting an instance time into one of our instance time lists we assign
// it a serial number. This allows us to sort the instance times in such a way
// that where we have several equal instance times, the ones added later will
// sort later. This means that when we call UpdateCurrentInterval during the
// waiting state we won't unnecessarily change the begin instance.
//
// The serial number also means that every instance time has an unambiguous
// position in the array so we can use RemoveElementSorted and the like.
bool SMILTimedElement::InstanceTimeComparator::Equals(
    const SMILInstanceTime* aElem1, const SMILInstanceTime* aElem2) const {
  MOZ_ASSERT(aElem1 && aElem2, "Trying to compare null instance time pointers");
  MOZ_ASSERT(aElem1->Serial() && aElem2->Serial(),
             "Instance times have not been assigned serial numbers");
  MOZ_ASSERT(aElem1 == aElem2 || aElem1->Serial() != aElem2->Serial(),
             "Serial numbers are not unique");

  return aElem1->Serial() == aElem2->Serial();
}

bool SMILTimedElement::InstanceTimeComparator::LessThan(
    const SMILInstanceTime* aElem1, const SMILInstanceTime* aElem2) const {
  MOZ_ASSERT(aElem1 && aElem2, "Trying to compare null instance time pointers");
  MOZ_ASSERT(aElem1->Serial() && aElem2->Serial(),
             "Instance times have not been assigned serial numbers");

  int8_t cmp = aElem1->Time().CompareTo(aElem2->Time());
  return cmp == 0 ? aElem1->Serial() < aElem2->Serial() : cmp < 0;
}

//----------------------------------------------------------------------
// Helper class: AsyncTimeEventRunner

namespace {
class AsyncTimeEventRunner : public Runnable {
 protected:
  const RefPtr<nsIContent> mTarget;
  EventMessage mMsg;
  int32_t mDetail;

 public:
  AsyncTimeEventRunner(nsIContent* aTarget, EventMessage aMsg, int32_t aDetail)
      : mozilla::Runnable("AsyncTimeEventRunner"),
        mTarget(aTarget),
        mMsg(aMsg),
        mDetail(aDetail) {}

  // TODO: Convert this to MOZ_CAN_RUN_SCRIPT (bug 1415230, bug 1535398)
  MOZ_CAN_RUN_SCRIPT_BOUNDARY NS_IMETHOD Run() override {
    InternalSMILTimeEvent event(true, mMsg);
    event.mDetail = mDetail;

    RefPtr<nsPresContext> context = nullptr;
    Document* doc = mTarget->GetComposedDoc();
    if (doc) {
      context = doc->GetPresContext();
    }

    return EventDispatcher::Dispatch(mTarget, context, &event);
  }
};
}  // namespace

//----------------------------------------------------------------------
// Helper class: AutoIntervalUpdateBatcher

// Stack-based helper class to set the mDeferIntervalUpdates flag on an
// SMILTimedElement and perform the UpdateCurrentInterval when the object is
// destroyed.
//
// If several of these objects are allocated on the stack, the update will not
// be performed until the last object for a given SMILTimedElement is
// destroyed.
class MOZ_STACK_CLASS SMILTimedElement::AutoIntervalUpdateBatcher {
 public:
  explicit AutoIntervalUpdateBatcher(SMILTimedElement& aTimedElement)
      : mTimedElement(aTimedElement),
        mDidSetFlag(!aTimedElement.mDeferIntervalUpdates) {
    mTimedElement.mDeferIntervalUpdates = true;
  }

  ~AutoIntervalUpdateBatcher() {
    if (!mDidSetFlag) return;

    mTimedElement.mDeferIntervalUpdates = false;

    if (mTimedElement.mDoDeferredUpdate) {
      mTimedElement.mDoDeferredUpdate = false;
      mTimedElement.UpdateCurrentInterval();
    }
  }

 private:
  SMILTimedElement& mTimedElement;
  bool mDidSetFlag;
};

//----------------------------------------------------------------------
// Helper class: AutoIntervalUpdater

// Stack-based helper class to call UpdateCurrentInterval when it is destroyed
// which helps avoid bugs where we forget to call UpdateCurrentInterval in the
// case of early returns (e.g. due to parse errors).
//
// This can be safely used in conjunction with AutoIntervalUpdateBatcher; any
// calls to UpdateCurrentInterval made by this class will simply be deferred if
// there is an AutoIntervalUpdateBatcher on the stack.
class MOZ_STACK_CLASS SMILTimedElement::AutoIntervalUpdater {
 public:
  explicit AutoIntervalUpdater(SMILTimedElement& aTimedElement)
      : mTimedElement(aTimedElement) {}

  ~AutoIntervalUpdater() { mTimedElement.UpdateCurrentInterval(); }

 private:
  SMILTimedElement& mTimedElement;
};

//----------------------------------------------------------------------
// Templated helper functions

// Selectively remove elements from an array of type
// nsTArray<RefPtr<SMILInstanceTime> > with O(n) performance.
template <class TestFunctor>
void SMILTimedElement::RemoveInstanceTimes(InstanceTimeList& aArray,
                                           TestFunctor& aTest) {
  InstanceTimeList newArray;
  for (uint32_t i = 0; i < aArray.Length(); ++i) {
    SMILInstanceTime* item = aArray[i].get();
    if (aTest(item, i)) {
      // As per bugs 665334 and 669225 we should be careful not to remove the
      // instance time that corresponds to the previous interval's end time.
      //
      // Most functors supplied here fulfil this condition by checking if the
      // instance time is marked as "ShouldPreserve" and if so, not deleting it.
      //
      // However, when filtering instance times, we sometimes need to drop even
      // instance times marked as "ShouldPreserve". In that case we take special
      // care not to delete the end instance time of the previous interval.
      MOZ_ASSERT(!GetPreviousInterval() || item != GetPreviousInterval()->End(),
                 "Removing end instance time of previous interval");
      item->Unlink();
    } else {
      newArray.AppendElement(item);
    }
  }
  aArray = std::move(newArray);
}

//----------------------------------------------------------------------
// Static members

const nsAttrValue::EnumTable SMILTimedElement::sFillModeTable[] = {
    {"remove", FILL_REMOVE}, {"freeze", FILL_FREEZE}, {nullptr, 0}};

const nsAttrValue::EnumTable SMILTimedElement::sRestartModeTable[] = {
    {"always", RESTART_ALWAYS},
    {"whenNotActive", RESTART_WHENNOTACTIVE},
    {"never", RESTART_NEVER},
    {nullptr, 0}};

const SMILMilestone SMILTimedElement::sMaxMilestone(
    std::numeric_limits<SMILTime>::max(), false);

// The thresholds at which point we start filtering intervals and instance times
// indiscriminately.
// See FilterIntervals and FilterInstanceTimes.
const uint8_t SMILTimedElement::sMaxNumIntervals = 20;
const uint8_t SMILTimedElement::sMaxNumInstanceTimes = 100;

// Detect if we arrive in some sort of undetected recursive syncbase dependency
// relationship
const uint8_t SMILTimedElement::sMaxUpdateIntervalRecursionDepth = 20;

//----------------------------------------------------------------------
// Ctor, dtor

SMILTimedElement::SMILTimedElement()
    : mAnimationElement(nullptr),
      mFillMode(FILL_REMOVE),
      mRestartMode(RESTART_ALWAYS),
      mInstanceSerialIndex(0),
      mClient(nullptr),
      mCurrentInterval(nullptr),
      mCurrentRepeatIteration(0),
      mPrevRegisteredMilestone(sMaxMilestone),
      mElementState(STATE_STARTUP),
      mSeekState(SEEK_NOT_SEEKING),
      mDeferIntervalUpdates(false),
      mDoDeferredUpdate(false),
      mIsDisabled(false),
      mDeleteCount(0),
      mUpdateIntervalRecursionDepth(0) {
  mSimpleDur.SetIndefinite();
  mMin.SetMillis(0L);
  mMax.SetIndefinite();
}

SMILTimedElement::~SMILTimedElement() {
  // Unlink all instance times from dependent intervals
  for (uint32_t i = 0; i < mBeginInstances.Length(); ++i) {
    mBeginInstances[i]->Unlink();
  }
  mBeginInstances.Clear();
  for (uint32_t i = 0; i < mEndInstances.Length(); ++i) {
    mEndInstances[i]->Unlink();
  }
  mEndInstances.Clear();

  // Notify anyone listening to our intervals that they're gone
  // (We shouldn't get any callbacks from this because all our instance times
  // are now disassociated with any intervals)
  ClearIntervals();

  // The following assertions are important in their own right (for checking
  // correct behavior) but also because AutoIntervalUpdateBatcher holds pointers
  // to class so if they fail there's the possibility we might have dangling
  // pointers.
  MOZ_ASSERT(!mDeferIntervalUpdates,
             "Interval updates should no longer be blocked when an "
             "SMILTimedElement disappears");
  MOZ_ASSERT(!mDoDeferredUpdate,
             "There should no longer be any pending updates when an "
             "SMILTimedElement disappears");
}

void SMILTimedElement::SetAnimationElement(SVGAnimationElement* aElement) {
  MOZ_ASSERT(aElement, "NULL owner element");
  MOZ_ASSERT(!mAnimationElement, "Re-setting owner");
  mAnimationElement = aElement;
}

SMILTimeContainer* SMILTimedElement::GetTimeContainer() {
  return mAnimationElement ? mAnimationElement->GetTimeContainer() : nullptr;
}

dom::Element* SMILTimedElement::GetTargetElement() {
  return mAnimationElement ? mAnimationElement->GetTargetElementContent()
                           : nullptr;
}

//----------------------------------------------------------------------
// ElementTimeControl methods
//
// The definition of the ElementTimeControl interface differs between SMIL
// Animation and SVG 1.1. In SMIL Animation all methods have a void return
// type and the new instance time is simply added to the list and restart
// semantics are applied as with any other instance time. In the SVG definition
// the methods return a bool depending on the restart mode.
//
// This inconsistency has now been addressed by an erratum in SVG 1.1:
//
// http://www.w3.org/2003/01/REC-SVG11-20030114-errata#elementtimecontrol-interface
//
// which favours the definition in SMIL, i.e. instance times are just added
// without first checking the restart mode.

nsresult SMILTimedElement::BeginElementAt(double aOffsetSeconds) {
  SMILTimeContainer* container = GetTimeContainer();
  if (!container) return NS_ERROR_FAILURE;

  SMILTime currentTime = container->GetCurrentTimeAsSMILTime();
  return AddInstanceTimeFromCurrentTime(currentTime, aOffsetSeconds, true);
}

nsresult SMILTimedElement::EndElementAt(double aOffsetSeconds) {
  SMILTimeContainer* container = GetTimeContainer();
  if (!container) return NS_ERROR_FAILURE;

  SMILTime currentTime = container->GetCurrentTimeAsSMILTime();
  return AddInstanceTimeFromCurrentTime(currentTime, aOffsetSeconds, false);
}

//----------------------------------------------------------------------
// SVGAnimationElement methods

SMILTimeValue SMILTimedElement::GetStartTime() const {
  return mElementState == STATE_WAITING || mElementState == STATE_ACTIVE
             ? mCurrentInterval->Begin()->Time()
             : SMILTimeValue();
}

//----------------------------------------------------------------------
// Hyperlinking support

SMILTimeValue SMILTimedElement::GetHyperlinkTime() const {
  SMILTimeValue hyperlinkTime;  // Default ctor creates unresolved time

  if (mElementState == STATE_ACTIVE) {
    hyperlinkTime = mCurrentInterval->Begin()->Time();
  } else if (!mBeginInstances.IsEmpty()) {
    hyperlinkTime = mBeginInstances[0]->Time();
  }

  return hyperlinkTime;
}

//----------------------------------------------------------------------
// SMILTimedElement

void SMILTimedElement::AddInstanceTime(SMILInstanceTime* aInstanceTime,
                                       bool aIsBegin) {
  MOZ_ASSERT(aInstanceTime, "Attempting to add null instance time");

  // Event-sensitivity: If an element is not active (but the parent time
  // container is), then events are only handled for begin specifications.
  if (mElementState != STATE_ACTIVE && !aIsBegin &&
      aInstanceTime->IsDynamic()) {
    // No need to call Unlink here--dynamic instance times shouldn't be linked
    // to anything that's going to miss them
    MOZ_ASSERT(!aInstanceTime->GetBaseInterval(),
               "Dynamic instance time has a base interval--we probably need "
               "to unlink it if we're not going to use it");
    return;
  }

  aInstanceTime->SetSerial(++mInstanceSerialIndex);
  InstanceTimeList& instanceList = aIsBegin ? mBeginInstances : mEndInstances;
  RefPtr<SMILInstanceTime>* inserted =
      instanceList.InsertElementSorted(aInstanceTime, InstanceTimeComparator());
  if (!inserted) {
    NS_WARNING("Insufficient memory to insert instance time");
    return;
  }

  UpdateCurrentInterval();
}

void SMILTimedElement::UpdateInstanceTime(SMILInstanceTime* aInstanceTime,
                                          SMILTimeValue& aUpdatedTime,
                                          bool aIsBegin) {
  MOZ_ASSERT(aInstanceTime, "Attempting to update null instance time");

  // The reason we update the time here and not in the SMILTimeValueSpec is
  // that it means we *could* re-sort more efficiently by doing a sorted remove
  // and insert but currently this doesn't seem to be necessary given how
  // infrequently we get these change notices.
  aInstanceTime->DependentUpdate(aUpdatedTime);
  InstanceTimeList& instanceList = aIsBegin ? mBeginInstances : mEndInstances;
  instanceList.Sort(InstanceTimeComparator());

  // Generally speaking, UpdateCurrentInterval makes changes to the current
  // interval and sends changes notices itself. However, in this case because
  // instance times are shared between the instance time list and the intervals
  // we are effectively changing the current interval outside
  // UpdateCurrentInterval so we need to explicitly signal that we've made
  // a change.
  //
  // This wouldn't be necessary if we cloned instance times on adding them to
  // the current interval but this introduces other complications (particularly
  // detecting which instance time is being used to define the begin of the
  // current interval when doing a Reset).
  bool changedCurrentInterval =
      mCurrentInterval && (mCurrentInterval->Begin() == aInstanceTime ||
                           mCurrentInterval->End() == aInstanceTime);

  UpdateCurrentInterval(changedCurrentInterval);
}

void SMILTimedElement::RemoveInstanceTime(SMILInstanceTime* aInstanceTime,
                                          bool aIsBegin) {
  MOZ_ASSERT(aInstanceTime, "Attempting to remove null instance time");

  // If the instance time should be kept (because it is or was the fixed end
  // point of an interval) then just disassociate it from the creator.
  if (aInstanceTime->ShouldPreserve()) {
    aInstanceTime->Unlink();
    return;
  }

  InstanceTimeList& instanceList = aIsBegin ? mBeginInstances : mEndInstances;
  mozilla::DebugOnly<bool> found =
      instanceList.RemoveElementSorted(aInstanceTime, InstanceTimeComparator());
  MOZ_ASSERT(found, "Couldn't find instance time to delete");

  UpdateCurrentInterval();
}

namespace {
class MOZ_STACK_CLASS RemoveByCreator {
 public:
  explicit RemoveByCreator(const SMILTimeValueSpec* aCreator)
      : mCreator(aCreator) {}

  bool operator()(SMILInstanceTime* aInstanceTime, uint32_t /*aIndex*/) {
    if (aInstanceTime->GetCreator() != mCreator) return false;

    // If the instance time should be kept (because it is or was the fixed end
    // point of an interval) then just disassociate it from the creator.
    if (aInstanceTime->ShouldPreserve()) {
      aInstanceTime->Unlink();
      return false;
    }

    return true;
  }

 private:
  const SMILTimeValueSpec* mCreator;
};
}  // namespace

void SMILTimedElement::RemoveInstanceTimesForCreator(
    const SMILTimeValueSpec* aCreator, bool aIsBegin) {
  MOZ_ASSERT(aCreator, "Creator not set");

  InstanceTimeList& instances = aIsBegin ? mBeginInstances : mEndInstances;
  RemoveByCreator removeByCreator(aCreator);
  RemoveInstanceTimes(instances, removeByCreator);

  UpdateCurrentInterval();
}

void SMILTimedElement::SetTimeClient(SMILAnimationFunction* aClient) {
  //
  // No need to check for nullptr. A nullptr parameter simply means to remove
  // the previous client which we do by setting to nullptr anyway.
  //

  mClient = aClient;
}

void SMILTimedElement::SampleAt(SMILTime aContainerTime) {
  if (mIsDisabled) return;

  // Milestones are cleared before a sample
  mPrevRegisteredMilestone = sMaxMilestone;

  DoSampleAt(aContainerTime, false);
}

void SMILTimedElement::SampleEndAt(SMILTime aContainerTime) {
  if (mIsDisabled) return;

  // Milestones are cleared before a sample
  mPrevRegisteredMilestone = sMaxMilestone;

  // If the current interval changes, we don't bother trying to remove any old
  // milestones we'd registered. So it's possible to get a call here to end an
  // interval at a time that no longer reflects the end of the current interval.
  //
  // For now we just check that we're actually in an interval but note that the
  // initial sample we use to initialise the model is an end sample. This is
  // because we want to resolve all the instance times before committing to an
  // initial interval. Therefore an end sample from the startup state is also
  // acceptable.
  if (mElementState == STATE_ACTIVE || mElementState == STATE_STARTUP) {
    DoSampleAt(aContainerTime, true);  // End sample
  } else {
    // Even if this was an unnecessary milestone sample we want to be sure that
    // our next real milestone is registered.
    RegisterMilestone();
  }
}

void SMILTimedElement::DoSampleAt(SMILTime aContainerTime, bool aEndOnly) {
  MOZ_ASSERT(mAnimationElement,
             "Got sample before being registered with an animation element");
  MOZ_ASSERT(GetTimeContainer(),
             "Got sample without being registered with a time container");

  // This could probably happen if we later implement externalResourcesRequired
  // (bug 277955) and whilst waiting for those resources (and the animation to
  // start) we transfer a node from another document fragment that has already
  // started. In such a case we might receive milestone samples registered with
  // the already active container.
  if (GetTimeContainer()->IsPausedByType(SMILTimeContainer::PAUSE_BEGIN))
    return;

  // We use an end-sample to start animation since an end-sample lets us
  // tentatively create an interval without committing to it (by transitioning
  // to the ACTIVE state) and this is necessary because we might have
  // dependencies on other animations that are yet to start. After these
  // other animations start, it may be necessary to revise our initial interval.
  //
  // However, sometimes instead of an end-sample we can get a regular sample
  // during STARTUP state. This can happen, for example, if we register
  // a milestone before time t=0 and are then re-bound to the tree (which sends
  // us back to the STARTUP state). In such a case we should just ignore the
  // sample and wait for our real initial sample which will be an end-sample.
  if (mElementState == STATE_STARTUP && !aEndOnly) return;

  bool finishedSeek = false;
  if (GetTimeContainer()->IsSeeking() && mSeekState == SEEK_NOT_SEEKING) {
    mSeekState = mElementState == STATE_ACTIVE ? SEEK_FORWARD_FROM_ACTIVE
                                               : SEEK_FORWARD_FROM_INACTIVE;
  } else if (mSeekState != SEEK_NOT_SEEKING &&
             !GetTimeContainer()->IsSeeking()) {
    finishedSeek = true;
  }

  bool stateChanged;
  SMILTimeValue sampleTime(aContainerTime);

  do {
#ifdef DEBUG
    // Check invariant
    if (mElementState == STATE_STARTUP || mElementState == STATE_POSTACTIVE) {
      MOZ_ASSERT(!mCurrentInterval,
                 "Shouldn't have current interval in startup or postactive "
                 "states");
    } else {
      MOZ_ASSERT(mCurrentInterval,
                 "Should have current interval in waiting and active states");
    }
#endif

    stateChanged = false;

    switch (mElementState) {
      case STATE_STARTUP: {
        SMILInterval firstInterval;
        mElementState =
            GetNextInterval(nullptr, nullptr, nullptr, firstInterval)
                ? STATE_WAITING
                : STATE_POSTACTIVE;
        stateChanged = true;
        if (mElementState == STATE_WAITING) {
          mCurrentInterval = MakeUnique<SMILInterval>(firstInterval);
          NotifyNewInterval();
        }
      } break;

      case STATE_WAITING: {
        if (mCurrentInterval->Begin()->Time() <= sampleTime) {
          mElementState = STATE_ACTIVE;
          mCurrentInterval->FixBegin();
          if (mClient) {
            mClient->Activate(mCurrentInterval->Begin()->Time().GetMillis());
          }
          if (mSeekState == SEEK_NOT_SEEKING) {
            FireTimeEventAsync(eSMILBeginEvent, 0);
          }
          if (HasPlayed()) {
            Reset();  // Apply restart behaviour
            // The call to Reset() may mean that the end point of our current
            // interval should be changed and so we should update the interval
            // now. However, calling UpdateCurrentInterval could result in the
            // interval getting deleted (perhaps through some web of syncbase
            // dependencies) therefore we make updating the interval the last
            // thing we do. There is no guarantee that mCurrentInterval is set
            // after this.
            UpdateCurrentInterval();
          }
          stateChanged = true;
        }
      } break;

      case STATE_ACTIVE: {
        // Ending early will change the interval but we don't notify dependents
        // of the change until we have closed off the current interval (since we
        // don't want dependencies to un-end our early end).
        bool didApplyEarlyEnd = ApplyEarlyEnd(sampleTime);

        if (mCurrentInterval->End()->Time() <= sampleTime) {
          SMILInterval newInterval;
          mElementState = GetNextInterval(mCurrentInterval.get(), nullptr,
                                          nullptr, newInterval)
                              ? STATE_WAITING
                              : STATE_POSTACTIVE;
          if (mClient) {
            mClient->Inactivate(mFillMode == FILL_FREEZE);
          }
          mCurrentInterval->FixEnd();
          if (mSeekState == SEEK_NOT_SEEKING) {
            FireTimeEventAsync(eSMILEndEvent, 0);
          }
          mCurrentRepeatIteration = 0;
          mOldIntervals.AppendElement(std::move(mCurrentInterval));
          SampleFillValue();
          if (mElementState == STATE_WAITING) {
            mCurrentInterval = MakeUnique<SMILInterval>(newInterval);
          }
          // We are now in a consistent state to dispatch notifications
          if (didApplyEarlyEnd) {
            NotifyChangedInterval(
                mOldIntervals[mOldIntervals.Length() - 1].get(), false, true);
          }
          if (mElementState == STATE_WAITING) {
            NotifyNewInterval();
          }
          FilterHistory();
          stateChanged = true;
        } else if (mCurrentInterval->Begin()->Time() <= sampleTime) {
          MOZ_ASSERT(!didApplyEarlyEnd, "We got an early end, but didn't end");
          SMILTime beginTime = mCurrentInterval->Begin()->Time().GetMillis();
          SMILTime activeTime = aContainerTime - beginTime;

          // The 'min' attribute can cause the active interval to be longer than
          // the 'repeating interval'.
          // In that extended period we apply the fill mode.
          if (GetRepeatDuration() <= SMILTimeValue(activeTime)) {
            if (mClient && mClient->IsActive()) {
              mClient->Inactivate(mFillMode == FILL_FREEZE);
            }
            SampleFillValue();
          } else {
            SampleSimpleTime(activeTime);

            // We register our repeat times as milestones (except when we're
            // seeking) so we should get a sample at exactly the time we repeat.
            // (And even when we are seeking we want to update
            // mCurrentRepeatIteration so we do that first before testing the
            // seek state.)
            uint32_t prevRepeatIteration = mCurrentRepeatIteration;
            if (ActiveTimeToSimpleTime(activeTime, mCurrentRepeatIteration) ==
                    0 &&
                mCurrentRepeatIteration != prevRepeatIteration &&
                mCurrentRepeatIteration && mSeekState == SEEK_NOT_SEEKING) {
              FireTimeEventAsync(eSMILRepeatEvent,
                                 static_cast<int32_t>(mCurrentRepeatIteration));
            }
          }
        }
        // Otherwise |sampleTime| is *before* the current interval. That
        // normally doesn't happen but can happen if we get a stray milestone
        // sample (e.g. if we registered a milestone with a time container that
        // later got re-attached as a child of a more advanced time container).
        // In that case we should just ignore the sample.
      } break;

      case STATE_POSTACTIVE:
        break;
    }

    // Generally we continue driving the state machine so long as we have
    // changed state. However, for end samples we only drive the state machine
    // as far as the waiting or postactive state because we don't want to commit
    // to any new interval (by transitioning to the active state) until all the
    // end samples have finished and we then have complete information about the
    // available instance times upon which to base our next interval.
  } while (stateChanged && (!aEndOnly || (mElementState != STATE_WAITING &&
                                          mElementState != STATE_POSTACTIVE)));

  if (finishedSeek) {
    DoPostSeek();
  }
  RegisterMilestone();
}

void SMILTimedElement::HandleContainerTimeChange() {
  // In future we could possibly introduce a separate change notice for time
  // container changes and only notify those dependents who live in other time
  // containers. For now we don't bother because when we re-resolve the time in
  // the SMILTimeValueSpec we'll check if anything has changed and if not, we
  // won't go any further.
  if (mElementState == STATE_WAITING || mElementState == STATE_ACTIVE) {
    NotifyChangedInterval(mCurrentInterval.get(), false, false);
  }
}

namespace {
bool RemoveNonDynamic(SMILInstanceTime* aInstanceTime) {
  // Generally dynamically-generated instance times (DOM calls, event-based
  // times) are not associated with their creator SMILTimeValueSpec since
  // they may outlive them.
  MOZ_ASSERT(!aInstanceTime->IsDynamic() || !aInstanceTime->GetCreator(),
             "Dynamic instance time should be unlinked from its creator");
  return !aInstanceTime->IsDynamic() && !aInstanceTime->ShouldPreserve();
}
}  // namespace

void SMILTimedElement::Rewind() {
  MOZ_ASSERT(mAnimationElement,
             "Got rewind request before being attached to an animation "
             "element");

  // It's possible to get a rewind request whilst we're already in the middle of
  // a backwards seek. This can happen when we're performing tree surgery and
  // seeking containers at the same time because we can end up requesting
  // a local rewind on an element after binding it to a new container and then
  // performing a rewind on that container as a whole without sampling in
  // between.
  //
  // However, it should currently be impossible to get a rewind in the middle of
  // a forwards seek since forwards seeks are detected and processed within the
  // same (re)sample.
  if (mSeekState == SEEK_NOT_SEEKING) {
    mSeekState = mElementState == STATE_ACTIVE ? SEEK_BACKWARD_FROM_ACTIVE
                                               : SEEK_BACKWARD_FROM_INACTIVE;
  }
  MOZ_ASSERT(mSeekState == SEEK_BACKWARD_FROM_INACTIVE ||
                 mSeekState == SEEK_BACKWARD_FROM_ACTIVE,
             "Rewind in the middle of a forwards seek?");

  ClearTimingState(RemoveNonDynamic);
  RebuildTimingState(RemoveNonDynamic);

  MOZ_ASSERT(!mCurrentInterval, "Current interval is set at end of rewind");
}

namespace {
bool RemoveAll(SMILInstanceTime* aInstanceTime) { return true; }
}  // namespace

bool SMILTimedElement::SetIsDisabled(bool aIsDisabled) {
  if (mIsDisabled == aIsDisabled) return false;

  if (aIsDisabled) {
    mIsDisabled = true;
    ClearTimingState(RemoveAll);
  } else {
    RebuildTimingState(RemoveAll);
    mIsDisabled = false;
  }
  return true;
}

namespace {
bool RemoveNonDOM(SMILInstanceTime* aInstanceTime) {
  return !aInstanceTime->FromDOM() && !aInstanceTime->ShouldPreserve();
}
}  // namespace

bool SMILTimedElement::SetAttr(nsAtom* aAttribute, const nsAString& aValue,
                               nsAttrValue& aResult, Element& aContextElement,
                               nsresult* aParseResult) {
  bool foundMatch = true;
  nsresult parseResult = NS_OK;

  if (aAttribute == nsGkAtoms::begin) {
    parseResult = SetBeginSpec(aValue, aContextElement, RemoveNonDOM);
  } else if (aAttribute == nsGkAtoms::dur) {
    parseResult = SetSimpleDuration(aValue);
  } else if (aAttribute == nsGkAtoms::end) {
    parseResult = SetEndSpec(aValue, aContextElement, RemoveNonDOM);
  } else if (aAttribute == nsGkAtoms::fill) {
    parseResult = SetFillMode(aValue);
  } else if (aAttribute == nsGkAtoms::max) {
    parseResult = SetMax(aValue);
  } else if (aAttribute == nsGkAtoms::min) {
    parseResult = SetMin(aValue);
  } else if (aAttribute == nsGkAtoms::repeatCount) {
    parseResult = SetRepeatCount(aValue);
  } else if (aAttribute == nsGkAtoms::repeatDur) {
    parseResult = SetRepeatDur(aValue);
  } else if (aAttribute == nsGkAtoms::restart) {
    parseResult = SetRestart(aValue);
  } else {
    foundMatch = false;
  }

  if (foundMatch) {
    aResult.SetTo(aValue);
    if (aParseResult) {
      *aParseResult = parseResult;
    }
  }

  return foundMatch;
}

bool SMILTimedElement::UnsetAttr(nsAtom* aAttribute) {
  bool foundMatch = true;

  if (aAttribute == nsGkAtoms::begin) {
    UnsetBeginSpec(RemoveNonDOM);
  } else if (aAttribute == nsGkAtoms::dur) {
    UnsetSimpleDuration();
  } else if (aAttribute == nsGkAtoms::end) {
    UnsetEndSpec(RemoveNonDOM);
  } else if (aAttribute == nsGkAtoms::fill) {
    UnsetFillMode();
  } else if (aAttribute == nsGkAtoms::max) {
    UnsetMax();
  } else if (aAttribute == nsGkAtoms::min) {
    UnsetMin();
  } else if (aAttribute == nsGkAtoms::repeatCount) {
    UnsetRepeatCount();
  } else if (aAttribute == nsGkAtoms::repeatDur) {
    UnsetRepeatDur();
  } else if (aAttribute == nsGkAtoms::restart) {
    UnsetRestart();
  } else {
    foundMatch = false;
  }

  return foundMatch;
}

//----------------------------------------------------------------------
// Setters and unsetters

nsresult SMILTimedElement::SetBeginSpec(const nsAString& aBeginSpec,
                                        Element& aContextElement,
                                        RemovalTestFunction aRemove) {
  return SetBeginOrEndSpec(aBeginSpec, aContextElement, true /*isBegin*/,
                           aRemove);
}

void SMILTimedElement::UnsetBeginSpec(RemovalTestFunction aRemove) {
  ClearSpecs(mBeginSpecs, mBeginInstances, aRemove);
  UpdateCurrentInterval();
}

nsresult SMILTimedElement::SetEndSpec(const nsAString& aEndSpec,
                                      Element& aContextElement,
                                      RemovalTestFunction aRemove) {
  return SetBeginOrEndSpec(aEndSpec, aContextElement, false /*!isBegin*/,
                           aRemove);
}

void SMILTimedElement::UnsetEndSpec(RemovalTestFunction aRemove) {
  ClearSpecs(mEndSpecs, mEndInstances, aRemove);
  UpdateCurrentInterval();
}

nsresult SMILTimedElement::SetSimpleDuration(const nsAString& aDurSpec) {
  // Update the current interval before returning
  AutoIntervalUpdater updater(*this);

  SMILTimeValue duration;
  const nsAString& dur = SMILParserUtils::TrimWhitespace(aDurSpec);

  // SVG-specific: "For SVG's animation elements, if "media" is specified, the
  // attribute will be ignored." (SVG 1.1, section 19.2.6)
  if (dur.EqualsLiteral("media") || dur.EqualsLiteral("indefinite")) {
    duration.SetIndefinite();
  } else {
    if (!SMILParserUtils::ParseClockValue(dur, &duration) ||
        duration.GetMillis() == 0L) {
      mSimpleDur.SetIndefinite();
      return NS_ERROR_FAILURE;
    }
  }
  // mSimpleDur should never be unresolved. ParseClockValue will either set
  // duration to resolved or will return false.
  MOZ_ASSERT(duration.IsResolved(), "Setting unresolved simple duration");

  mSimpleDur = duration;

  return NS_OK;
}

void SMILTimedElement::UnsetSimpleDuration() {
  mSimpleDur.SetIndefinite();
  UpdateCurrentInterval();
}

nsresult SMILTimedElement::SetMin(const nsAString& aMinSpec) {
  // Update the current interval before returning
  AutoIntervalUpdater updater(*this);

  SMILTimeValue duration;
  const nsAString& min = SMILParserUtils::TrimWhitespace(aMinSpec);

  if (min.EqualsLiteral("media")) {
    duration.SetMillis(0L);
  } else {
    if (!SMILParserUtils::ParseClockValue(min, &duration)) {
      mMin.SetMillis(0L);
      return NS_ERROR_FAILURE;
    }
  }

  MOZ_ASSERT(duration.GetMillis() >= 0L, "Invalid duration");

  mMin = duration;

  return NS_OK;
}

void SMILTimedElement::UnsetMin() {
  mMin.SetMillis(0L);
  UpdateCurrentInterval();
}

nsresult SMILTimedElement::SetMax(const nsAString& aMaxSpec) {
  // Update the current interval before returning
  AutoIntervalUpdater updater(*this);

  SMILTimeValue duration;
  const nsAString& max = SMILParserUtils::TrimWhitespace(aMaxSpec);

  if (max.EqualsLiteral("media") || max.EqualsLiteral("indefinite")) {
    duration.SetIndefinite();
  } else {
    if (!SMILParserUtils::ParseClockValue(max, &duration) ||
        duration.GetMillis() == 0L) {
      mMax.SetIndefinite();
      return NS_ERROR_FAILURE;
    }
    MOZ_ASSERT(duration.GetMillis() > 0L, "Invalid duration");
  }

  mMax = duration;

  return NS_OK;
}

void SMILTimedElement::UnsetMax() {
  mMax.SetIndefinite();
  UpdateCurrentInterval();
}

nsresult SMILTimedElement::SetRestart(const nsAString& aRestartSpec) {
  nsAttrValue temp;
  bool parseResult = temp.ParseEnumValue(aRestartSpec, sRestartModeTable, true);
  mRestartMode =
      parseResult ? SMILRestartMode(temp.GetEnumValue()) : RESTART_ALWAYS;
  UpdateCurrentInterval();
  return parseResult ? NS_OK : NS_ERROR_FAILURE;
}

void SMILTimedElement::UnsetRestart() {
  mRestartMode = RESTART_ALWAYS;
  UpdateCurrentInterval();
}

nsresult SMILTimedElement::SetRepeatCount(const nsAString& aRepeatCountSpec) {
  // Update the current interval before returning
  AutoIntervalUpdater updater(*this);

  SMILRepeatCount newRepeatCount;

  if (SMILParserUtils::ParseRepeatCount(aRepeatCountSpec, newRepeatCount)) {
    mRepeatCount = newRepeatCount;
    return NS_OK;
  }
  mRepeatCount.Unset();
  return NS_ERROR_FAILURE;
}

void SMILTimedElement::UnsetRepeatCount() {
  mRepeatCount.Unset();
  UpdateCurrentInterval();
}

nsresult SMILTimedElement::SetRepeatDur(const nsAString& aRepeatDurSpec) {
  // Update the current interval before returning
  AutoIntervalUpdater updater(*this);

  SMILTimeValue duration;

  const nsAString& repeatDur = SMILParserUtils::TrimWhitespace(aRepeatDurSpec);

  if (repeatDur.EqualsLiteral("indefinite")) {
    duration.SetIndefinite();
  } else {
    if (!SMILParserUtils::ParseClockValue(repeatDur, &duration)) {
      mRepeatDur.SetUnresolved();
      return NS_ERROR_FAILURE;
    }
  }

  mRepeatDur = duration;

  return NS_OK;
}

void SMILTimedElement::UnsetRepeatDur() {
  mRepeatDur.SetUnresolved();
  UpdateCurrentInterval();
}

nsresult SMILTimedElement::SetFillMode(const nsAString& aFillModeSpec) {
  uint16_t previousFillMode = mFillMode;

  nsAttrValue temp;
  bool parseResult = temp.ParseEnumValue(aFillModeSpec, sFillModeTable, true);
  mFillMode = parseResult ? SMILFillMode(temp.GetEnumValue()) : FILL_REMOVE;

  // Update fill mode of client
  if (mFillMode != previousFillMode && HasClientInFillRange()) {
    mClient->Inactivate(mFillMode == FILL_FREEZE);
    SampleFillValue();
  }

  return parseResult ? NS_OK : NS_ERROR_FAILURE;
}

void SMILTimedElement::UnsetFillMode() {
  uint16_t previousFillMode = mFillMode;
  mFillMode = FILL_REMOVE;
  if (previousFillMode == FILL_FREEZE && HasClientInFillRange()) {
    mClient->Inactivate(false);
  }
}

void SMILTimedElement::AddDependent(SMILTimeValueSpec& aDependent) {
  // There's probably no harm in attempting to register a dependent
  // SMILTimeValueSpec twice, but we're not expecting it to happen.
  MOZ_ASSERT(!mTimeDependents.GetEntry(&aDependent),
             "SMILTimeValueSpec is already registered as a dependency");
  mTimeDependents.PutEntry(&aDependent);

  // Add current interval. We could add historical intervals too but that would
  // cause unpredictable results since some intervals may have been filtered.
  // SMIL doesn't say what to do here so for simplicity and consistency we
  // simply add the current interval if there is one.
  //
  // It's not necessary to call SyncPauseTime since we're dealing with
  // historical instance times not newly added ones.
  if (mCurrentInterval) {
    aDependent.HandleNewInterval(*mCurrentInterval, GetTimeContainer());
  }
}

void SMILTimedElement::RemoveDependent(SMILTimeValueSpec& aDependent) {
  mTimeDependents.RemoveEntry(&aDependent);
}

bool SMILTimedElement::IsTimeDependent(const SMILTimedElement& aOther) const {
  const SMILInstanceTime* thisBegin = GetEffectiveBeginInstance();
  const SMILInstanceTime* otherBegin = aOther.GetEffectiveBeginInstance();

  if (!thisBegin || !otherBegin) return false;

  return thisBegin->IsDependentOn(*otherBegin);
}

void SMILTimedElement::BindToTree(Element& aContextElement) {
  // Reset previously registered milestone since we may be registering with
  // a different time container now.
  mPrevRegisteredMilestone = sMaxMilestone;

  // If we were already active then clear all our timing information and start
  // afresh
  if (mElementState != STATE_STARTUP) {
    mSeekState = SEEK_NOT_SEEKING;
    Rewind();
  }

  // Scope updateBatcher to last only for the ResolveReferences calls:
  {
    AutoIntervalUpdateBatcher updateBatcher(*this);

    // Resolve references to other parts of the tree
    uint32_t count = mBeginSpecs.Length();
    for (uint32_t i = 0; i < count; ++i) {
      mBeginSpecs[i]->ResolveReferences(aContextElement);
    }

    count = mEndSpecs.Length();
    for (uint32_t j = 0; j < count; ++j) {
      mEndSpecs[j]->ResolveReferences(aContextElement);
    }
  }

  RegisterMilestone();
}

void SMILTimedElement::HandleTargetElementChange(Element* aNewTarget) {
  AutoIntervalUpdateBatcher updateBatcher(*this);

  uint32_t count = mBeginSpecs.Length();
  for (uint32_t i = 0; i < count; ++i) {
    mBeginSpecs[i]->HandleTargetElementChange(aNewTarget);
  }

  count = mEndSpecs.Length();
  for (uint32_t j = 0; j < count; ++j) {
    mEndSpecs[j]->HandleTargetElementChange(aNewTarget);
  }
}

void SMILTimedElement::Traverse(nsCycleCollectionTraversalCallback* aCallback) {
  uint32_t count = mBeginSpecs.Length();
  for (uint32_t i = 0; i < count; ++i) {
    SMILTimeValueSpec* beginSpec = mBeginSpecs[i].get();
    MOZ_ASSERT(beginSpec, "null SMILTimeValueSpec in list of begin specs");
    beginSpec->Traverse(aCallback);
  }

  count = mEndSpecs.Length();
  for (uint32_t j = 0; j < count; ++j) {
    SMILTimeValueSpec* endSpec = mEndSpecs[j].get();
    MOZ_ASSERT(endSpec, "null SMILTimeValueSpec in list of end specs");
    endSpec->Traverse(aCallback);
  }
}

void SMILTimedElement::Unlink() {
  AutoIntervalUpdateBatcher updateBatcher(*this);

  // Remove dependencies on other elements
  uint32_t count = mBeginSpecs.Length();
  for (uint32_t i = 0; i < count; ++i) {
    SMILTimeValueSpec* beginSpec = mBeginSpecs[i].get();
    MOZ_ASSERT(beginSpec, "null SMILTimeValueSpec in list of begin specs");
    beginSpec->Unlink();
  }

  count = mEndSpecs.Length();
  for (uint32_t j = 0; j < count; ++j) {
    SMILTimeValueSpec* endSpec = mEndSpecs[j].get();
    MOZ_ASSERT(endSpec, "null SMILTimeValueSpec in list of end specs");
    endSpec->Unlink();
  }

  ClearIntervals();

  // Make sure we don't notify other elements of new intervals
  mTimeDependents.Clear();
}

//----------------------------------------------------------------------
// Implementation helpers

nsresult SMILTimedElement::SetBeginOrEndSpec(const nsAString& aSpec,
                                             Element& aContextElement,
                                             bool aIsBegin,
                                             RemovalTestFunction aRemove) {
  TimeValueSpecList& timeSpecsList = aIsBegin ? mBeginSpecs : mEndSpecs;
  InstanceTimeList& instances = aIsBegin ? mBeginInstances : mEndInstances;

  ClearSpecs(timeSpecsList, instances, aRemove);

  AutoIntervalUpdateBatcher updateBatcher(*this);

  nsCharSeparatedTokenizer tokenizer(aSpec, ';');
  if (!tokenizer.hasMoreTokens()) {  // Empty list
    return NS_ERROR_FAILURE;
  }

  bool hadFailure = false;
  while (tokenizer.hasMoreTokens()) {
    auto spec = MakeUnique<SMILTimeValueSpec>(*this, aIsBegin);
    nsresult rv = spec->SetSpec(tokenizer.nextToken(), aContextElement);
    if (NS_SUCCEEDED(rv)) {
      timeSpecsList.AppendElement(std::move(spec));
    } else {
      hadFailure = true;
    }
  }

  // The return value from this function is only used to determine if we should
  // print a console message or not, so we return failure if we had one or more
  // failures but we don't need to differentiate between different types of
  // failures or the number of failures.
  return hadFailure ? NS_ERROR_FAILURE : NS_OK;
}

namespace {
// Adaptor functor for RemoveInstanceTimes that allows us to use function
// pointers instead.
// Without this we'd have to either templatize ClearSpecs and all its callers
// or pass bool flags around to specify which removal function to use here.
class MOZ_STACK_CLASS RemoveByFunction {
 public:
  explicit RemoveByFunction(SMILTimedElement::RemovalTestFunction aFunction)
      : mFunction(aFunction) {}
  bool operator()(SMILInstanceTime* aInstanceTime, uint32_t /*aIndex*/) {
    return mFunction(aInstanceTime);
  }

 private:
  SMILTimedElement::RemovalTestFunction mFunction;
};
}  // namespace

void SMILTimedElement::ClearSpecs(TimeValueSpecList& aSpecs,
                                  InstanceTimeList& aInstances,
                                  RemovalTestFunction aRemove) {
  AutoIntervalUpdateBatcher updateBatcher(*this);

  for (uint32_t i = 0; i < aSpecs.Length(); ++i) {
    aSpecs[i]->Unlink();
  }
  aSpecs.Clear();

  RemoveByFunction removeByFunction(aRemove);
  RemoveInstanceTimes(aInstances, removeByFunction);
}

void SMILTimedElement::ClearIntervals() {
  if (mElementState != STATE_STARTUP) {
    mElementState = STATE_POSTACTIVE;
  }
  mCurrentRepeatIteration = 0;
  ResetCurrentInterval();

  // Remove old intervals
  for (int32_t i = mOldIntervals.Length() - 1; i >= 0; --i) {
    mOldIntervals[i]->Unlink();
  }
  mOldIntervals.Clear();
}

bool SMILTimedElement::ApplyEarlyEnd(const SMILTimeValue& aSampleTime) {
  // This should only be called within DoSampleAt as a helper function
  MOZ_ASSERT(mElementState == STATE_ACTIVE,
             "Unexpected state to try to apply an early end");

  bool updated = false;

  // Only apply an early end if we're not already ending.
  if (mCurrentInterval->End()->Time() > aSampleTime) {
    SMILInstanceTime* earlyEnd = CheckForEarlyEnd(aSampleTime);
    if (earlyEnd) {
      if (earlyEnd->IsDependent()) {
        // Generate a new instance time for the early end since the
        // existing instance time is part of some dependency chain that we
        // don't want to participate in.
        RefPtr<SMILInstanceTime> newEarlyEnd =
            new SMILInstanceTime(earlyEnd->Time());
        mCurrentInterval->SetEnd(*newEarlyEnd);
      } else {
        mCurrentInterval->SetEnd(*earlyEnd);
      }
      updated = true;
    }
  }
  return updated;
}

namespace {
class MOZ_STACK_CLASS RemoveReset {
 public:
  explicit RemoveReset(const SMILInstanceTime* aCurrentIntervalBegin)
      : mCurrentIntervalBegin(aCurrentIntervalBegin) {}
  bool operator()(SMILInstanceTime* aInstanceTime, uint32_t /*aIndex*/) {
    // SMIL 3.0 section 5.4.3, 'Resetting element state':
    //   Any instance times associated with past Event-values, Repeat-values,
    //   Accesskey-values or added via DOM method calls are removed from the
    //   dependent begin and end instance times lists. In effect, all events
    //   and DOM methods calls in the past are cleared. This does not apply to
    //   an instance time that defines the begin of the current interval.
    return aInstanceTime->IsDynamic() && !aInstanceTime->ShouldPreserve() &&
           (!mCurrentIntervalBegin || aInstanceTime != mCurrentIntervalBegin);
  }

 private:
  const SMILInstanceTime* mCurrentIntervalBegin;
};
}  // namespace

void SMILTimedElement::Reset() {
  RemoveReset resetBegin(mCurrentInterval ? mCurrentInterval->Begin()
                                          : nullptr);
  RemoveInstanceTimes(mBeginInstances, resetBegin);

  RemoveReset resetEnd(nullptr);
  RemoveInstanceTimes(mEndInstances, resetEnd);
}

void SMILTimedElement::ClearTimingState(RemovalTestFunction aRemove) {
  mElementState = STATE_STARTUP;
  ClearIntervals();

  UnsetBeginSpec(aRemove);
  UnsetEndSpec(aRemove);

  if (mClient) {
    mClient->Inactivate(false);
  }
}

void SMILTimedElement::RebuildTimingState(RemovalTestFunction aRemove) {
  MOZ_ASSERT(mAnimationElement,
             "Attempting to enable a timed element not attached to an "
             "animation element");
  MOZ_ASSERT(mElementState == STATE_STARTUP,
             "Rebuilding timing state from non-startup state");

  if (mAnimationElement->HasAttr(nsGkAtoms::begin)) {
    nsAutoString attValue;
    mAnimationElement->GetAttr(nsGkAtoms::begin, attValue);
    SetBeginSpec(attValue, *mAnimationElement, aRemove);
  }

  if (mAnimationElement->HasAttr(nsGkAtoms::end)) {
    nsAutoString attValue;
    mAnimationElement->GetAttr(nsGkAtoms::end, attValue);
    SetEndSpec(attValue, *mAnimationElement, aRemove);
  }

  mPrevRegisteredMilestone = sMaxMilestone;
  RegisterMilestone();
}

void SMILTimedElement::DoPostSeek() {
  // Finish backwards seek
  if (mSeekState == SEEK_BACKWARD_FROM_INACTIVE ||
      mSeekState == SEEK_BACKWARD_FROM_ACTIVE) {
    // Previously some dynamic instance times may have been marked to be
    // preserved because they were endpoints of an historic interval (which may
    // or may not have been filtered). Now that we've finished a seek we should
    // clear that flag for those instance times whose intervals are no longer
    // historic.
    UnpreserveInstanceTimes(mBeginInstances);
    UnpreserveInstanceTimes(mEndInstances);

    // Now that the times have been unmarked perform a reset. This might seem
    // counter-intuitive when we're only doing a seek within an interval but
    // SMIL seems to require this. SMIL 3.0, 'Hyperlinks and timing':
    //   Resolved end times associated with events, Repeat-values,
    //   Accesskey-values or added via DOM method calls are cleared when seeking
    //   to time earlier than the resolved end time.
    Reset();
    UpdateCurrentInterval();
  }

  switch (mSeekState) {
    case SEEK_FORWARD_FROM_ACTIVE:
    case SEEK_BACKWARD_FROM_ACTIVE:
      if (mElementState != STATE_ACTIVE) {
        FireTimeEventAsync(eSMILEndEvent, 0);
      }
      break;

    case SEEK_FORWARD_FROM_INACTIVE:
    case SEEK_BACKWARD_FROM_INACTIVE:
      if (mElementState == STATE_ACTIVE) {
        FireTimeEventAsync(eSMILBeginEvent, 0);
      }
      break;

    case SEEK_NOT_SEEKING:
      /* Do nothing */
      break;
  }

  mSeekState = SEEK_NOT_SEEKING;
}

void SMILTimedElement::UnpreserveInstanceTimes(InstanceTimeList& aList) {
  const SMILInterval* prevInterval = GetPreviousInterval();
  const SMILInstanceTime* cutoff = mCurrentInterval ? mCurrentInterval->Begin()
                                   : prevInterval   ? prevInterval->Begin()
                                                    : nullptr;
  uint32_t count = aList.Length();
  for (uint32_t i = 0; i < count; ++i) {
    SMILInstanceTime* instance = aList[i].get();
    if (!cutoff || cutoff->Time().CompareTo(instance->Time()) < 0) {
      instance->UnmarkShouldPreserve();
    }
  }
}

void SMILTimedElement::FilterHistory() {
  // We should filter the intervals first, since instance times still used in an
  // interval won't be filtered.
  FilterIntervals();
  FilterInstanceTimes(mBeginInstances);
  FilterInstanceTimes(mEndInstances);
}

void SMILTimedElement::FilterIntervals() {
  // We can filter old intervals that:
  //
  // a) are not the previous interval; AND
  // b) are not in the middle of a dependency chain; AND
  // c) are not the first interval
  //
  // Condition (a) is necessary since the previous interval is used for applying
  // fill effects and updating the current interval.
  //
  // Condition (b) is necessary since even if this interval itself is not
  // active, it may be part of a dependency chain that includes active
  // intervals. Such chains are used to establish priorities within the
  // animation sandwich.
  //
  // Condition (c) is necessary to support hyperlinks that target animations
  // since in some cases the defined behavior is to seek the document back to
  // the first resolved begin time. Presumably the intention here is not
  // actually to use the first resolved begin time, the
  // _the_first_resolved_begin_time_that_produced_an_interval. That is,
  // if we have begin="-5s; -3s; 1s; 3s" with a duration on 1s, we should seek
  // to 1s. The spec doesn't say this but I'm pretty sure that is the intention.
  // It seems negative times were simply not considered.
  //
  // Although the above conditions allow us to safely filter intervals for most
  // scenarios they do not cover all cases and there will still be scenarios
  // that generate intervals indefinitely. In such a case we simply set
  // a maximum number of intervals and drop any intervals beyond that threshold.

  uint32_t threshold = mOldIntervals.Length() > sMaxNumIntervals
                           ? mOldIntervals.Length() - sMaxNumIntervals
                           : 0;
  IntervalList filteredList;
  for (uint32_t i = 0; i < mOldIntervals.Length(); ++i) {
    SMILInterval* interval = mOldIntervals[i].get();
    if (i != 0 &&                         /*skip first interval*/
        i + 1 < mOldIntervals.Length() && /*skip previous interval*/
        (i < threshold || !interval->IsDependencyChainLink())) {
      interval->Unlink(true /*filtered, not deleted*/);
    } else {
      filteredList.AppendElement(std::move(mOldIntervals[i]));
    }
  }
  mOldIntervals = std::move(filteredList);
}

namespace {
class MOZ_STACK_CLASS RemoveFiltered {
 public:
  explicit RemoveFiltered(SMILTimeValue aCutoff) : mCutoff(aCutoff) {}
  bool operator()(SMILInstanceTime* aInstanceTime, uint32_t /*aIndex*/) {
    // We can filter instance times that:
    // a) Precede the end point of the previous interval; AND
    // b) Are NOT syncbase times that might be updated to a time after the end
    //    point of the previous interval; AND
    // c) Are NOT fixed end points in any remaining interval.
    return aInstanceTime->Time() < mCutoff && aInstanceTime->IsFixedTime() &&
           !aInstanceTime->ShouldPreserve();
  }

 private:
  SMILTimeValue mCutoff;
};

class MOZ_STACK_CLASS RemoveBelowThreshold {
 public:
  RemoveBelowThreshold(uint32_t aThreshold,
                       nsTArray<const SMILInstanceTime*>& aTimesToKeep)
      : mThreshold(aThreshold), mTimesToKeep(aTimesToKeep) {}
  bool operator()(SMILInstanceTime* aInstanceTime, uint32_t aIndex) {
    return aIndex < mThreshold && !mTimesToKeep.Contains(aInstanceTime);
  }

 private:
  uint32_t mThreshold;
  nsTArray<const SMILInstanceTime*>& mTimesToKeep;
};
}  // namespace

void SMILTimedElement::FilterInstanceTimes(InstanceTimeList& aList) {
  if (GetPreviousInterval()) {
    RemoveFiltered removeFiltered(GetPreviousInterval()->End()->Time());
    RemoveInstanceTimes(aList, removeFiltered);
  }

  // As with intervals it is possible to create a document that, even despite
  // our most aggressive filtering, will generate instance times indefinitely
  // (e.g. cyclic dependencies with TimeEvents---we can't filter such times as
  // they're unpredictable due to the possibility of seeking the document which
  // may prevent some events from being generated). Therefore we introduce
  // a hard cutoff at which point we just drop the oldest instance times.
  if (aList.Length() > sMaxNumInstanceTimes) {
    uint32_t threshold = aList.Length() - sMaxNumInstanceTimes;
    // There are a few instance times we should keep though, notably:
    // - the current interval begin time,
    // - the previous interval end time (see note in RemoveInstanceTimes)
    // - the first interval begin time (see note in FilterIntervals)
    nsTArray<const SMILInstanceTime*> timesToKeep;
    if (mCurrentInterval) {
      timesToKeep.AppendElement(mCurrentInterval->Begin());
    }
    const SMILInterval* prevInterval = GetPreviousInterval();
    if (prevInterval) {
      timesToKeep.AppendElement(prevInterval->End());
    }
    if (!mOldIntervals.IsEmpty()) {
      timesToKeep.AppendElement(mOldIntervals[0]->Begin());
    }
    RemoveBelowThreshold removeBelowThreshold(threshold, timesToKeep);
    RemoveInstanceTimes(aList, removeBelowThreshold);
  }
}

//
// This method is based on the pseudocode given in the SMILANIM spec.
//
// See:
// http://www.w3.org/TR/2001/REC-smil-animation-20010904/#Timing-BeginEnd-LC-Start
//
bool SMILTimedElement::GetNextInterval(const SMILInterval* aPrevInterval,
                                       const SMILInterval* aReplacedInterval,
                                       const SMILInstanceTime* aFixedBeginTime,
                                       SMILInterval& aResult) const {
  MOZ_ASSERT(!aFixedBeginTime || aFixedBeginTime->Time().IsDefinite(),
             "Unresolved or indefinite begin time given for interval start");
  static const SMILTimeValue zeroTime(0L);

  if (mRestartMode == RESTART_NEVER && aPrevInterval) return false;

  // Calc starting point
  SMILTimeValue beginAfter;
  bool prevIntervalWasZeroDur = false;
  if (aPrevInterval) {
    beginAfter = aPrevInterval->End()->Time();
    prevIntervalWasZeroDur =
        aPrevInterval->End()->Time() == aPrevInterval->Begin()->Time();
  } else {
    beginAfter.SetMillis(INT64_MIN);
  }

  RefPtr<SMILInstanceTime> tempBegin;
  RefPtr<SMILInstanceTime> tempEnd;

  while (true) {
    // Calculate begin time
    if (aFixedBeginTime) {
      if (aFixedBeginTime->Time() < beginAfter) {
        return false;
      }
      // our ref-counting is not const-correct
      tempBegin = const_cast<SMILInstanceTime*>(aFixedBeginTime);
    } else if ((!mAnimationElement ||
                !mAnimationElement->HasAttr(nsGkAtoms::begin)) &&
               beginAfter <= zeroTime) {
      tempBegin = new SMILInstanceTime(SMILTimeValue(0));
    } else {
      int32_t beginPos = 0;
      do {
        tempBegin =
            GetNextGreaterOrEqual(mBeginInstances, beginAfter, beginPos);
        if (!tempBegin || !tempBegin->Time().IsDefinite()) {
          return false;
        }
        // If we're updating the current interval then skip any begin time that
        // is dependent on the current interval's begin time. e.g.
        //   <animate id="a" begin="b.begin; a.begin+2s"...
        // If b's interval disappears whilst 'a' is in the waiting state the
        // begin time at "a.begin+2s" should be skipped since 'a' never begun.
      } while (aReplacedInterval &&
               tempBegin->GetBaseTime() == aReplacedInterval->Begin());
    }
    MOZ_ASSERT(tempBegin && tempBegin->Time().IsDefinite() &&
                   tempBegin->Time() >= beginAfter,
               "Got a bad begin time while fetching next interval");

    // Calculate end time
    {
      int32_t endPos = 0;
      do {
        tempEnd =
            GetNextGreaterOrEqual(mEndInstances, tempBegin->Time(), endPos);

        // SMIL doesn't allow for coincident zero-duration intervals, so if the
        // previous interval was zero-duration, and tempEnd is going to give us
        // another zero duration interval, then look for another end to use
        // instead.
        if (tempEnd && prevIntervalWasZeroDur &&
            tempEnd->Time() == beginAfter) {
          tempEnd = GetNextGreater(mEndInstances, tempBegin->Time(), endPos);
        }
        // As above with begin times, avoid creating self-referential loops
        // between instance times by checking that the newly found end instance
        // time is not already dependent on the end of the current interval.
      } while (tempEnd && aReplacedInterval &&
               tempEnd->GetBaseTime() == aReplacedInterval->End());

      if (!tempEnd) {
        // If all the ends are before the beginning we have a bad interval
        // UNLESS:
        // a) We never had any end attribute to begin with (the SMIL pseudocode
        //    places this condition earlier in the flow but that fails to allow
        //    for DOM calls when no "indefinite" condition is given), OR
        // b) We never had any end instance times to begin with, OR
        // c) We have end events which leave the interval open-ended.
        bool openEndedIntervalOk = mEndSpecs.IsEmpty() ||
                                   mEndInstances.IsEmpty() ||
                                   EndHasEventConditions();

        // The above conditions correspond with the SMIL pseudocode but SMIL
        // doesn't address self-dependent instance times which we choose to
        // ignore.
        //
        // Therefore we add a qualification of (b) above that even if
        // there are end instance times but they all depend on the end of the
        // current interval we should act as if they didn't exist and allow the
        // open-ended interval.
        //
        // In the following condition we don't use |= because it doesn't provide
        // short-circuit behavior.
        openEndedIntervalOk =
            openEndedIntervalOk ||
            (aReplacedInterval &&
             AreEndTimesDependentOn(aReplacedInterval->End()));

        if (!openEndedIntervalOk) {
          return false;  // Bad interval
        }
      }

      SMILTimeValue intervalEnd = tempEnd ? tempEnd->Time() : SMILTimeValue();
      SMILTimeValue activeEnd = CalcActiveEnd(tempBegin->Time(), intervalEnd);

      if (!tempEnd || intervalEnd != activeEnd) {
        tempEnd = new SMILInstanceTime(activeEnd);
      }
    }
    MOZ_ASSERT(tempEnd, "Failed to get end point for next interval");

    // When we choose the interval endpoints, we don't allow coincident
    // zero-duration intervals, so if we arrive here and we have a zero-duration
    // interval starting at the same point as a previous zero-duration interval,
    // then it must be because we've applied constraints to the active duration.
    // In that case, we will potentially run into an infinite loop, so we break
    // it by searching for the next interval that starts AFTER our current
    // zero-duration interval.
    if (prevIntervalWasZeroDur && tempEnd->Time() == beginAfter) {
      beginAfter.SetMillis(tempBegin->Time().GetMillis() + 1);
      prevIntervalWasZeroDur = false;
      continue;
    }
    prevIntervalWasZeroDur = tempBegin->Time() == tempEnd->Time();

    // Check for valid interval
    if (tempEnd->Time() > zeroTime ||
        (tempBegin->Time() == zeroTime && tempEnd->Time() == zeroTime)) {
      aResult.Set(*tempBegin, *tempEnd);
      return true;
    }

    if (mRestartMode == RESTART_NEVER) {
      // tempEnd <= 0 so we're going to loop which effectively means restarting
      return false;
    }

    beginAfter = tempEnd->Time();
  }
  MOZ_ASSERT_UNREACHABLE("Hmm... we really shouldn't be here");

  return false;
}

SMILInstanceTime* SMILTimedElement::GetNextGreater(
    const InstanceTimeList& aList, const SMILTimeValue& aBase,
    int32_t& aPosition) const {
  SMILInstanceTime* result = nullptr;
  while ((result = GetNextGreaterOrEqual(aList, aBase, aPosition)) &&
         result->Time() == aBase) {
  }
  return result;
}

SMILInstanceTime* SMILTimedElement::GetNextGreaterOrEqual(
    const InstanceTimeList& aList, const SMILTimeValue& aBase,
    int32_t& aPosition) const {
  SMILInstanceTime* result = nullptr;
  int32_t count = aList.Length();

  for (; aPosition < count && !result; ++aPosition) {
    SMILInstanceTime* val = aList[aPosition].get();
    MOZ_ASSERT(val, "NULL instance time in list");
    if (val->Time() >= aBase) {
      result = val;
    }
  }

  return result;
}

/**
 * @see SMILANIM 3.3.4
 */
SMILTimeValue SMILTimedElement::CalcActiveEnd(const SMILTimeValue& aBegin,
                                              const SMILTimeValue& aEnd) const {
  SMILTimeValue result;

  MOZ_ASSERT(mSimpleDur.IsResolved(),
             "Unresolved simple duration in CalcActiveEnd");
  MOZ_ASSERT(aBegin.IsDefinite(),
             "Indefinite or unresolved begin time in CalcActiveEnd");

  result = GetRepeatDuration();

  if (aEnd.IsDefinite()) {
    SMILTime activeDur = aEnd.GetMillis() - aBegin.GetMillis();

    if (result.IsDefinite()) {
      result.SetMillis(std::min(result.GetMillis(), activeDur));
    } else {
      result.SetMillis(activeDur);
    }
  }

  result = ApplyMinAndMax(result);

  if (result.IsDefinite()) {
    SMILTime activeEnd = result.GetMillis() + aBegin.GetMillis();
    result.SetMillis(activeEnd);
  }

  return result;
}

SMILTimeValue SMILTimedElement::GetRepeatDuration() const {
  SMILTimeValue multipliedDuration;
  if (mRepeatCount.IsDefinite() && mSimpleDur.IsDefinite()) {
    if (mRepeatCount * double(mSimpleDur.GetMillis()) <
        double(std::numeric_limits<SMILTime>::max())) {
      multipliedDuration.SetMillis(
          SMILTime(mRepeatCount * mSimpleDur.GetMillis()));
    }
  } else {
    multipliedDuration.SetIndefinite();
  }

  SMILTimeValue repeatDuration;

  if (mRepeatDur.IsResolved()) {
    repeatDuration = std::min(multipliedDuration, mRepeatDur);
  } else if (mRepeatCount.IsSet()) {
    repeatDuration = multipliedDuration;
  } else {
    repeatDuration = mSimpleDur;
  }

  return repeatDuration;
}

SMILTimeValue SMILTimedElement::ApplyMinAndMax(
    const SMILTimeValue& aDuration) const {
  if (!aDuration.IsResolved()) {
    return aDuration;
  }

  if (mMax < mMin) {
    return aDuration;
  }

  SMILTimeValue result;

  if (aDuration > mMax) {
    result = mMax;
  } else if (aDuration < mMin) {
    result = mMin;
  } else {
    result = aDuration;
  }

  return result;
}

SMILTime SMILTimedElement::ActiveTimeToSimpleTime(SMILTime aActiveTime,
                                                  uint32_t& aRepeatIteration) {
  SMILTime result;

  MOZ_ASSERT(mSimpleDur.IsResolved(),
             "Unresolved simple duration in ActiveTimeToSimpleTime");
  MOZ_ASSERT(aActiveTime >= 0, "Expecting non-negative active time");
  // Note that a negative aActiveTime will give us a negative value for
  // aRepeatIteration, which is bad because aRepeatIteration is unsigned

  if (mSimpleDur.IsIndefinite() || mSimpleDur.GetMillis() == 0L) {
    aRepeatIteration = 0;
    result = aActiveTime;
  } else {
    result = aActiveTime % mSimpleDur.GetMillis();
    aRepeatIteration = (uint32_t)(aActiveTime / mSimpleDur.GetMillis());
  }

  return result;
}

//
// Although in many cases it would be possible to check for an early end and
// adjust the current interval well in advance the SMIL Animation spec seems to
// indicate that we should only apply an early end at the latest possible
// moment. In particular, this paragraph from section 3.6.8:
//
// 'If restart  is set to "always", then the current interval will end early if
// there is an instance time in the begin list that is before (i.e. earlier
// than) the defined end for the current interval. Ending in this manner will
// also send a changed time notice to all time dependents for the current
// interval end.'
//
SMILInstanceTime* SMILTimedElement::CheckForEarlyEnd(
    const SMILTimeValue& aContainerTime) const {
  MOZ_ASSERT(mCurrentInterval,
             "Checking for an early end but the current interval is not set");
  if (mRestartMode != RESTART_ALWAYS) return nullptr;

  int32_t position = 0;
  SMILInstanceTime* nextBegin = GetNextGreater(
      mBeginInstances, mCurrentInterval->Begin()->Time(), position);

  if (nextBegin && nextBegin->Time() > mCurrentInterval->Begin()->Time() &&
      nextBegin->Time() < mCurrentInterval->End()->Time() &&
      nextBegin->Time() <= aContainerTime) {
    return nextBegin;
  }

  return nullptr;
}

void SMILTimedElement::UpdateCurrentInterval(bool aForceChangeNotice) {
  // Check if updates are currently blocked (batched)
  if (mDeferIntervalUpdates) {
    mDoDeferredUpdate = true;
    return;
  }

  // We adopt the convention of not resolving intervals until the first
  // sample. Otherwise, every time each attribute is set we'll re-resolve the
  // current interval and notify all our time dependents of the change.
  //
  // The disadvantage of deferring resolving the interval is that DOM calls to
  // to getStartTime will throw an INVALID_STATE_ERR exception until the
  // document timeline begins since the start time has not yet been resolved.
  if (mElementState == STATE_STARTUP) return;

  // Although SMIL gives rules for detecting cycles in change notifications,
  // some configurations can lead to create-delete-create-delete-etc. cycles
  // which SMIL does not consider.
  //
  // In order to provide consistent behavior in such cases, we detect two
  // deletes in a row and then refuse to create any further intervals. That is,
  // we say the configuration is invalid.
  if (mDeleteCount > 1) {
    // When we update the delete count we also set the state to post active, so
    // if we're not post active here then something other than
    // UpdateCurrentInterval has updated the element state in between and all
    // bets are off.
    MOZ_ASSERT(mElementState == STATE_POSTACTIVE,
               "Expected to be in post-active state after performing double "
               "delete");
    return;
  }

  // Check that we aren't stuck in infinite recursion updating some syncbase
  // dependencies. Generally such situations should be detected in advance and
  // the chain broken in a sensible and predictable manner, so if we're hitting
  // this assertion we need to work out how to detect the case that's causing
  // it. In release builds, just bail out before we overflow the stack.
  AutoRestore<uint8_t> depthRestorer(mUpdateIntervalRecursionDepth);
  if (++mUpdateIntervalRecursionDepth > sMaxUpdateIntervalRecursionDepth) {
    MOZ_ASSERT(false,
               "Update current interval recursion depth exceeded threshold");
    return;
  }

  // If the interval is active the begin time is fixed.
  const SMILInstanceTime* beginTime =
      mElementState == STATE_ACTIVE ? mCurrentInterval->Begin() : nullptr;
  SMILInterval updatedInterval;
  if (GetNextInterval(GetPreviousInterval(), mCurrentInterval.get(), beginTime,
                      updatedInterval)) {
    if (mElementState == STATE_POSTACTIVE) {
      MOZ_ASSERT(!mCurrentInterval,
                 "In postactive state but the interval has been set");
      mCurrentInterval = MakeUnique<SMILInterval>(updatedInterval);
      mElementState = STATE_WAITING;
      NotifyNewInterval();

    } else {
      bool beginChanged = false;
      bool endChanged = false;

      if (mElementState != STATE_ACTIVE &&
          !updatedInterval.Begin()->SameTimeAndBase(
              *mCurrentInterval->Begin())) {
        mCurrentInterval->SetBegin(*updatedInterval.Begin());
        beginChanged = true;
      }

      if (!updatedInterval.End()->SameTimeAndBase(*mCurrentInterval->End())) {
        mCurrentInterval->SetEnd(*updatedInterval.End());
        endChanged = true;
      }

      if (beginChanged || endChanged || aForceChangeNotice) {
        NotifyChangedInterval(mCurrentInterval.get(), beginChanged, endChanged);
      }
    }

    // There's a chance our next milestone has now changed, so update the time
    // container
    RegisterMilestone();
  } else {  // GetNextInterval failed: Current interval is no longer valid
    if (mElementState == STATE_ACTIVE) {
      // The interval is active so we can't just delete it, instead trim it so
      // that begin==end.
      if (!mCurrentInterval->End()->SameTimeAndBase(
              *mCurrentInterval->Begin())) {
        mCurrentInterval->SetEnd(*mCurrentInterval->Begin());
        NotifyChangedInterval(mCurrentInterval.get(), false, true);
      }
      // The transition to the postactive state will take place on the next
      // sample (along with firing end events, clearing intervals etc.)
      RegisterMilestone();
    } else if (mElementState == STATE_WAITING) {
      AutoRestore<uint8_t> deleteCountRestorer(mDeleteCount);
      ++mDeleteCount;
      mElementState = STATE_POSTACTIVE;
      ResetCurrentInterval();
    }
  }
}

void SMILTimedElement::SampleSimpleTime(SMILTime aActiveTime) {
  if (mClient) {
    uint32_t repeatIteration;
    SMILTime simpleTime = ActiveTimeToSimpleTime(aActiveTime, repeatIteration);
    mClient->SampleAt(simpleTime, mSimpleDur, repeatIteration);
  }
}

void SMILTimedElement::SampleFillValue() {
  if (mFillMode != FILL_FREEZE || !mClient) return;

  SMILTime activeTime;

  if (mElementState == STATE_WAITING || mElementState == STATE_POSTACTIVE) {
    const SMILInterval* prevInterval = GetPreviousInterval();
    MOZ_ASSERT(prevInterval,
               "Attempting to sample fill value but there is no previous "
               "interval");
    MOZ_ASSERT(prevInterval->End()->Time().IsDefinite() &&
                   prevInterval->End()->IsFixedTime(),
               "Attempting to sample fill value but the endpoint of the "
               "previous interval is not resolved and fixed");

    activeTime = prevInterval->End()->Time().GetMillis() -
                 prevInterval->Begin()->Time().GetMillis();

    // If the interval's repeat duration was shorter than its active duration,
    // use the end of the repeat duration to determine the frozen animation's
    // state.
    SMILTimeValue repeatDuration = GetRepeatDuration();
    if (repeatDuration.IsDefinite()) {
      activeTime = std::min(repeatDuration.GetMillis(), activeTime);
    }
  } else {
    MOZ_ASSERT(
        mElementState == STATE_ACTIVE,
        "Attempting to sample fill value when we're in an unexpected state "
        "(probably STATE_STARTUP)");

    // If we are being asked to sample the fill value while active we *must*
    // have a repeat duration shorter than the active duration so use that.
    MOZ_ASSERT(GetRepeatDuration().IsDefinite(),
               "Attempting to sample fill value of an active animation with "
               "an indefinite repeat duration");
    activeTime = GetRepeatDuration().GetMillis();
  }

  uint32_t repeatIteration;
  SMILTime simpleTime = ActiveTimeToSimpleTime(activeTime, repeatIteration);

  if (simpleTime == 0L && repeatIteration) {
    mClient->SampleLastValue(--repeatIteration);
  } else {
    mClient->SampleAt(simpleTime, mSimpleDur, repeatIteration);
  }
}

nsresult SMILTimedElement::AddInstanceTimeFromCurrentTime(SMILTime aCurrentTime,
                                                          double aOffsetSeconds,
                                                          bool aIsBegin) {
  double offset = NS_round(aOffsetSeconds * PR_MSEC_PER_SEC);

  // Check we won't overflow the range of SMILTime
  if (aCurrentTime + offset > double(std::numeric_limits<SMILTime>::max()))
    return NS_ERROR_ILLEGAL_VALUE;

  SMILTimeValue timeVal(aCurrentTime + int64_t(offset));

  RefPtr<SMILInstanceTime> instanceTime =
      new SMILInstanceTime(timeVal, SMILInstanceTime::SOURCE_DOM);

  AddInstanceTime(instanceTime, aIsBegin);

  return NS_OK;
}

void SMILTimedElement::RegisterMilestone() {
  SMILTimeContainer* container = GetTimeContainer();
  if (!container) return;
  MOZ_ASSERT(mAnimationElement,
             "Got a time container without an owning animation element");

  SMILMilestone nextMilestone;
  if (!GetNextMilestone(nextMilestone)) return;

  // This method is called every time we might possibly have updated our
  // current interval, but since SMILTimeContainer makes no attempt to filter
  // out redundant milestones we do some rudimentary filtering here. It's not
  // perfect, but unnecessary samples are fairly cheap.
  if (nextMilestone >= mPrevRegisteredMilestone) return;

  container->AddMilestone(nextMilestone, *mAnimationElement);
  mPrevRegisteredMilestone = nextMilestone;
}

bool SMILTimedElement::GetNextMilestone(SMILMilestone& aNextMilestone) const {
  // Return the next key moment in our lifetime.
  //
  // XXX It may be possible in future to optimise this so that we only register
  // for milestones if:
  // a) We have time dependents, or
  // b) We are dependent on events or syncbase relationships, or
  // c) There are registered listeners for our events
  //
  // Then for the simple case where everything uses offset values we could
  // ignore milestones altogether.
  //
  // We'd need to be careful, however, that if one of those conditions became
  // true in between samples that we registered our next milestone at that
  // point.

  switch (mElementState) {
    case STATE_STARTUP:
      // All elements register for an initial end sample at t=0 where we resolve
      // our initial interval.
      aNextMilestone.mIsEnd = true;  // Initial sample should be an end sample
      aNextMilestone.mTime = 0;
      return true;

    case STATE_WAITING:
      MOZ_ASSERT(mCurrentInterval,
                 "In waiting state but the current interval has not been set");
      aNextMilestone.mIsEnd = false;
      aNextMilestone.mTime = mCurrentInterval->Begin()->Time().GetMillis();
      return true;

    case STATE_ACTIVE: {
      // Work out what comes next: the interval end or the next repeat iteration
      SMILTimeValue nextRepeat;
      if (mSeekState == SEEK_NOT_SEEKING && mSimpleDur.IsDefinite()) {
        SMILTime nextRepeatActiveTime =
            (mCurrentRepeatIteration + 1) * mSimpleDur.GetMillis();
        // Check that the repeat fits within the repeat duration
        if (SMILTimeValue(nextRepeatActiveTime) < GetRepeatDuration()) {
          nextRepeat.SetMillis(mCurrentInterval->Begin()->Time().GetMillis() +
                               nextRepeatActiveTime);
        }
      }
      SMILTimeValue nextMilestone =
          std::min(mCurrentInterval->End()->Time(), nextRepeat);

      // Check for an early end before that time
      SMILInstanceTime* earlyEnd = CheckForEarlyEnd(nextMilestone);
      if (earlyEnd) {
        aNextMilestone.mIsEnd = true;
        aNextMilestone.mTime = earlyEnd->Time().GetMillis();
        return true;
      }

      // Apply the previously calculated milestone
      if (nextMilestone.IsDefinite()) {
        aNextMilestone.mIsEnd = nextMilestone != nextRepeat;
        aNextMilestone.mTime = nextMilestone.GetMillis();
        return true;
      }

      return false;
    }

    case STATE_POSTACTIVE:
      return false;
  }
  MOZ_CRASH("Invalid element state");
}

void SMILTimedElement::NotifyNewInterval() {
  MOZ_ASSERT(mCurrentInterval,
             "Attempting to notify dependents of a new interval but the "
             "interval is not set");

  SMILTimeContainer* container = GetTimeContainer();
  if (container) {
    container->SyncPauseTime();
  }

  for (SMILTimeValueSpec* spec : mTimeDependents.Keys()) {
    SMILInterval* interval = mCurrentInterval.get();
    // It's possible that in notifying one new time dependent of a new interval
    // that a chain reaction is triggered which results in the original
    // interval disappearing. If that's the case we can skip sending further
    // notifications.
    if (!interval) {
      break;
    }
    spec->HandleNewInterval(*interval, container);
  }
}

void SMILTimedElement::NotifyChangedInterval(SMILInterval* aInterval,
                                             bool aBeginObjectChanged,
                                             bool aEndObjectChanged) {
  MOZ_ASSERT(aInterval, "Null interval for change notification");

  SMILTimeContainer* container = GetTimeContainer();
  if (container) {
    container->SyncPauseTime();
  }

  // Copy the instance times list since notifying the instance times can result
  // in a chain reaction whereby our own interval gets deleted along with its
  // instance times.
  InstanceTimeList times;
  aInterval->GetDependentTimes(times);

  for (uint32_t i = 0; i < times.Length(); ++i) {
    times[i]->HandleChangedInterval(container, aBeginObjectChanged,
                                    aEndObjectChanged);
  }
}

void SMILTimedElement::FireTimeEventAsync(EventMessage aMsg, int32_t aDetail) {
  if (!mAnimationElement) return;

  nsCOMPtr<nsIRunnable> event =
      new AsyncTimeEventRunner(mAnimationElement, aMsg, aDetail);
  mAnimationElement->OwnerDoc()->Dispatch(TaskCategory::Other, event.forget());
}

const SMILInstanceTime* SMILTimedElement::GetEffectiveBeginInstance() const {
  switch (mElementState) {
    case STATE_STARTUP:
      return nullptr;

    case STATE_ACTIVE:
      return mCurrentInterval->Begin();

    case STATE_WAITING:
    case STATE_POSTACTIVE: {
      const SMILInterval* prevInterval = GetPreviousInterval();
      return prevInterval ? prevInterval->Begin() : nullptr;
    }
  }
  MOZ_CRASH("Invalid element state");
}

const SMILInterval* SMILTimedElement::GetPreviousInterval() const {
  return mOldIntervals.IsEmpty()
             ? nullptr
             : mOldIntervals[mOldIntervals.Length() - 1].get();
}

bool SMILTimedElement::HasClientInFillRange() const {
  // Returns true if we have a client that is in the range where it will fill
  return mClient && ((mElementState != STATE_ACTIVE && HasPlayed()) ||
                     (mElementState == STATE_ACTIVE && !mClient->IsActive()));
}

bool SMILTimedElement::EndHasEventConditions() const {
  for (uint32_t i = 0; i < mEndSpecs.Length(); ++i) {
    if (mEndSpecs[i]->IsEventBased()) return true;
  }
  return false;
}

bool SMILTimedElement::AreEndTimesDependentOn(
    const SMILInstanceTime* aBase) const {
  if (mEndInstances.IsEmpty()) return false;

  for (uint32_t i = 0; i < mEndInstances.Length(); ++i) {
    if (mEndInstances[i]->GetBaseTime() != aBase) {
      return false;
    }
  }
  return true;
}

}  // namespace mozilla
