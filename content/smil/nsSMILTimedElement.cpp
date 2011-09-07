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
 * Portions created by the Initial Developer are Copyright (C) 2006
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

#include "nsSMILTimedElement.h"
#include "nsSMILAnimationFunction.h"
#include "nsSMILTimeValue.h"
#include "nsSMILTimeValueSpec.h"
#include "nsSMILInstanceTime.h"
#include "nsSMILParserUtils.h"
#include "nsSMILTimeContainer.h"
#include "nsGkAtoms.h"
#include "nsGUIEvent.h"
#include "nsEventDispatcher.h"
#include "nsReadableUtils.h"
#include "nsMathUtils.h"
#include "nsThreadUtils.h"
#include "nsIPresShell.h"
#include "prdtoa.h"
#include "plstr.h"
#include "prtime.h"
#include "nsString.h"
#include "mozilla/AutoRestore.h"
#include "mozilla/Util.h"

using namespace mozilla;

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
PRBool
nsSMILTimedElement::InstanceTimeComparator::Equals(
    const nsSMILInstanceTime* aElem1,
    const nsSMILInstanceTime* aElem2) const
{
  NS_ABORT_IF_FALSE(aElem1 && aElem2,
      "Trying to compare null instance time pointers");
  NS_ABORT_IF_FALSE(aElem1->Serial() && aElem2->Serial(),
      "Instance times have not been assigned serial numbers");
  NS_ABORT_IF_FALSE(aElem1 == aElem2 || aElem1->Serial() != aElem2->Serial(),
      "Serial numbers are not unique");

  return aElem1->Serial() == aElem2->Serial();
}

PRBool
nsSMILTimedElement::InstanceTimeComparator::LessThan(
    const nsSMILInstanceTime* aElem1,
    const nsSMILInstanceTime* aElem2) const
{
  NS_ABORT_IF_FALSE(aElem1 && aElem2,
      "Trying to compare null instance time pointers");
  NS_ABORT_IF_FALSE(aElem1->Serial() && aElem2->Serial(),
      "Instance times have not been assigned serial numbers");

  PRInt8 cmp = aElem1->Time().CompareTo(aElem2->Time());
  return cmp == 0 ? aElem1->Serial() < aElem2->Serial() : cmp < 0;
}

//----------------------------------------------------------------------
// Helper class: AsyncTimeEventRunner

namespace
{
  class AsyncTimeEventRunner : public nsRunnable
  {
  protected:
    nsRefPtr<nsIContent> mTarget;
    PRUint32             mMsg;
    PRInt32              mDetail;

  public:
    AsyncTimeEventRunner(nsIContent* aTarget, PRUint32 aMsg, PRInt32 aDetail)
      : mTarget(aTarget), mMsg(aMsg), mDetail(aDetail)
    {
    }

    NS_IMETHOD Run()
    {
      nsUIEvent event(PR_TRUE, mMsg, mDetail);
      event.eventStructType = NS_SMIL_TIME_EVENT;

      nsPresContext* context = nsnull;
      nsIDocument* doc = mTarget->GetCurrentDoc();
      if (doc) {
        nsCOMPtr<nsIPresShell> shell = doc->GetShell();
        if (shell) {
          context = shell->GetPresContext();
        }
      }

      return nsEventDispatcher::Dispatch(mTarget, context, &event);
    }
  };
}

//----------------------------------------------------------------------
// Helper class: AutoIntervalUpdateBatcher

// RAII helper to set the mDeferIntervalUpdates flag on an nsSMILTimedElement
// and perform the UpdateCurrentInterval when the object is destroyed.
//
// If several of these objects are allocated on the stack, the update will not
// be performed until the last object for a given nsSMILTimedElement is
// destroyed.
class NS_STACK_CLASS nsSMILTimedElement::AutoIntervalUpdateBatcher
{
public:
  AutoIntervalUpdateBatcher(nsSMILTimedElement& aTimedElement)
    : mTimedElement(aTimedElement),
      mDidSetFlag(!aTimedElement.mDeferIntervalUpdates)
  {
    mTimedElement.mDeferIntervalUpdates = PR_TRUE;
  }

  ~AutoIntervalUpdateBatcher()
  {
    if (!mDidSetFlag)
      return;

    mTimedElement.mDeferIntervalUpdates = PR_FALSE;

    if (mTimedElement.mDoDeferredUpdate) {
      mTimedElement.mDoDeferredUpdate = PR_FALSE;
      mTimedElement.UpdateCurrentInterval();
    }
  }

private:
  nsSMILTimedElement& mTimedElement;
  PRPackedBool mDidSetFlag;
};

//----------------------------------------------------------------------
// Templated helper functions

// Selectively remove elements from an array of type
// nsTArray<nsRefPtr<nsSMILInstanceTime> > with O(n) performance.
template <class TestFunctor>
void
nsSMILTimedElement::RemoveInstanceTimes(InstanceTimeList& aArray,
                                        TestFunctor& aTest)
{
  InstanceTimeList newArray;
  for (PRUint32 i = 0; i < aArray.Length(); ++i) {
    nsSMILInstanceTime* item = aArray[i].get();
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
      NS_ABORT_IF_FALSE(!GetPreviousInterval() ||
        item != GetPreviousInterval()->End(),
        "Removing end instance time of previous interval");
      item->Unlink();
    } else {
      newArray.AppendElement(item);
    }
  }
  aArray.Clear();
  aArray.SwapElements(newArray);
}

//----------------------------------------------------------------------
// Static members

nsAttrValue::EnumTable nsSMILTimedElement::sFillModeTable[] = {
      {"remove", FILL_REMOVE},
      {"freeze", FILL_FREEZE},
      {nsnull, 0}
};

nsAttrValue::EnumTable nsSMILTimedElement::sRestartModeTable[] = {
      {"always", RESTART_ALWAYS},
      {"whenNotActive", RESTART_WHENNOTACTIVE},
      {"never", RESTART_NEVER},
      {nsnull, 0}
};

const nsSMILMilestone nsSMILTimedElement::sMaxMilestone(LL_MAXINT, PR_FALSE);

// The thresholds at which point we start filtering intervals and instance times
// indiscriminately.
// See FilterIntervals and FilterInstanceTimes.
const PRUint8 nsSMILTimedElement::sMaxNumIntervals = 20;
const PRUint8 nsSMILTimedElement::sMaxNumInstanceTimes = 100;

// Detect if we arrive in some sort of undetected recursive syncbase dependency
// relationship
const PRUint16 nsSMILTimedElement::sMaxUpdateIntervalRecursionDepth = 20;

//----------------------------------------------------------------------
// Ctor, dtor

nsSMILTimedElement::nsSMILTimedElement()
:
  mAnimationElement(nsnull),
  mFillMode(FILL_REMOVE),
  mRestartMode(RESTART_ALWAYS),
  mInstanceSerialIndex(0),
  mClient(nsnull),
  mCurrentInterval(nsnull),
  mCurrentRepeatIteration(0),
  mPrevRegisteredMilestone(sMaxMilestone),
  mElementState(STATE_STARTUP),
  mSeekState(SEEK_NOT_SEEKING),
  mDeferIntervalUpdates(PR_FALSE),
  mDoDeferredUpdate(PR_FALSE),
  mUpdateIntervalRecursionDepth(0)
{
  mSimpleDur.SetIndefinite();
  mMin.SetMillis(0L);
  mMax.SetIndefinite();
  mTimeDependents.Init();
}

nsSMILTimedElement::~nsSMILTimedElement()
{
  // Unlink all instance times from dependent intervals
  for (PRUint32 i = 0; i < mBeginInstances.Length(); ++i) {
    mBeginInstances[i]->Unlink();
  }
  mBeginInstances.Clear();
  for (PRUint32 i = 0; i < mEndInstances.Length(); ++i) {
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
  NS_ABORT_IF_FALSE(!mDeferIntervalUpdates,
      "Interval updates should no longer be blocked when an nsSMILTimedElement "
      "disappears");
  NS_ABORT_IF_FALSE(!mDoDeferredUpdate,
      "There should no longer be any pending updates when an "
      "nsSMILTimedElement disappears");
}

void
nsSMILTimedElement::SetAnimationElement(nsISMILAnimationElement* aElement)
{
  NS_ABORT_IF_FALSE(aElement, "NULL owner element");
  NS_ABORT_IF_FALSE(!mAnimationElement, "Re-setting owner");
  mAnimationElement = aElement;
}

nsSMILTimeContainer*
nsSMILTimedElement::GetTimeContainer()
{
  return mAnimationElement ? mAnimationElement->GetTimeContainer() : nsnull;
}

//----------------------------------------------------------------------
// nsIDOMElementTimeControl methods
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

nsresult
nsSMILTimedElement::BeginElementAt(double aOffsetSeconds)
{
  nsSMILTimeContainer* container = GetTimeContainer();
  if (!container)
    return NS_ERROR_FAILURE;

  nsSMILTime currentTime = container->GetCurrentTime();
  return AddInstanceTimeFromCurrentTime(currentTime, aOffsetSeconds, PR_TRUE);
}

nsresult
nsSMILTimedElement::EndElementAt(double aOffsetSeconds)
{
  nsSMILTimeContainer* container = GetTimeContainer();
  if (!container)
    return NS_ERROR_FAILURE;

  nsSMILTime currentTime = container->GetCurrentTime();
  return AddInstanceTimeFromCurrentTime(currentTime, aOffsetSeconds, PR_FALSE);
}

//----------------------------------------------------------------------
// nsSVGAnimationElement methods

nsSMILTimeValue
nsSMILTimedElement::GetStartTime() const
{
  return mElementState == STATE_WAITING || mElementState == STATE_ACTIVE
         ? mCurrentInterval->Begin()->Time()
         : nsSMILTimeValue();
}

//----------------------------------------------------------------------
// nsSMILTimedElement

void
nsSMILTimedElement::AddInstanceTime(nsSMILInstanceTime* aInstanceTime,
                                    PRBool aIsBegin)
{
  NS_ABORT_IF_FALSE(aInstanceTime, "Attempting to add null instance time");

  // Event-sensitivity: If an element is not active (but the parent time
  // container is), then events are only handled for begin specifications.
  if (mElementState != STATE_ACTIVE && !aIsBegin &&
      aInstanceTime->IsDynamic())
  {
    // No need to call Unlink here--dynamic instance times shouldn't be linked
    // to anything that's going to miss them
    NS_ABORT_IF_FALSE(!aInstanceTime->GetBaseInterval(),
        "Dynamic instance time has a base interval--we probably need to unlink"
        " it if we're not going to use it");
    return;
  }

  aInstanceTime->SetSerial(++mInstanceSerialIndex);
  InstanceTimeList& instanceList = aIsBegin ? mBeginInstances : mEndInstances;
  nsRefPtr<nsSMILInstanceTime>* inserted =
    instanceList.InsertElementSorted(aInstanceTime, InstanceTimeComparator());
  if (!inserted) {
    NS_WARNING("Insufficient memory to insert instance time");
    return;
  }

  UpdateCurrentInterval();
}

void
nsSMILTimedElement::UpdateInstanceTime(nsSMILInstanceTime* aInstanceTime,
                                       nsSMILTimeValue& aUpdatedTime,
                                       PRBool aIsBegin)
{
  NS_ABORT_IF_FALSE(aInstanceTime, "Attempting to update null instance time");

  // The reason we update the time here and not in the nsSMILTimeValueSpec is
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
  PRBool changedCurrentInterval = mCurrentInterval &&
    (mCurrentInterval->Begin() == aInstanceTime ||
     mCurrentInterval->End() == aInstanceTime);

  UpdateCurrentInterval(changedCurrentInterval);
}

void
nsSMILTimedElement::RemoveInstanceTime(nsSMILInstanceTime* aInstanceTime,
                                       PRBool aIsBegin)
{
  NS_ABORT_IF_FALSE(aInstanceTime, "Attempting to remove null instance time");

  // If the instance time should be kept (because it is or was the fixed end
  // point of an interval) then just disassociate it from the creator.
  if (aInstanceTime->ShouldPreserve()) {
    aInstanceTime->Unlink();
    return;
  }

  InstanceTimeList& instanceList = aIsBegin ? mBeginInstances : mEndInstances;
  mozilla::DebugOnly<PRBool> found =
    instanceList.RemoveElementSorted(aInstanceTime, InstanceTimeComparator());
  NS_ABORT_IF_FALSE(found, "Couldn't find instance time to delete");

  UpdateCurrentInterval();
}

namespace
{
  class NS_STACK_CLASS RemoveByCreator
  {
  public:
    RemoveByCreator(const nsSMILTimeValueSpec* aCreator) : mCreator(aCreator)
    { }

    PRBool operator()(nsSMILInstanceTime* aInstanceTime, PRUint32 /*aIndex*/)
    {
      if (aInstanceTime->GetCreator() != mCreator)
        return PR_FALSE;

      // If the instance time should be kept (because it is or was the fixed end
      // point of an interval) then just disassociate it from the creator.
      if (aInstanceTime->ShouldPreserve()) {
        aInstanceTime->Unlink();
        return PR_FALSE;
      }

      return PR_TRUE;
    }

  private:
    const nsSMILTimeValueSpec* mCreator;
  };
}

void
nsSMILTimedElement::RemoveInstanceTimesForCreator(
    const nsSMILTimeValueSpec* aCreator, PRBool aIsBegin)
{
  NS_ABORT_IF_FALSE(aCreator, "Creator not set");

  InstanceTimeList& instances = aIsBegin ? mBeginInstances : mEndInstances;
  RemoveByCreator removeByCreator(aCreator);
  RemoveInstanceTimes(instances, removeByCreator);

  UpdateCurrentInterval();
}

void
nsSMILTimedElement::SetTimeClient(nsSMILAnimationFunction* aClient)
{
  //
  // No need to check for NULL. A NULL parameter simply means to remove the
  // previous client which we do by setting to NULL anyway.
  //

  mClient = aClient;
}

void
nsSMILTimedElement::SampleAt(nsSMILTime aContainerTime)
{
  // Milestones are cleared before a sample
  mPrevRegisteredMilestone = sMaxMilestone;

  DoSampleAt(aContainerTime, PR_FALSE);
}

void
nsSMILTimedElement::SampleEndAt(nsSMILTime aContainerTime)
{
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
    DoSampleAt(aContainerTime, PR_TRUE); // End sample
  } else {
    // Even if this was an unnecessary milestone sample we want to be sure that
    // our next real milestone is registered.
    RegisterMilestone();
  }
}

void
nsSMILTimedElement::DoSampleAt(nsSMILTime aContainerTime, PRBool aEndOnly)
{
  NS_ABORT_IF_FALSE(mAnimationElement,
      "Got sample before being registered with an animation element");
  NS_ABORT_IF_FALSE(GetTimeContainer(),
      "Got sample without being registered with a time container");

  // This could probably happen if we later implement externalResourcesRequired
  // (bug 277955) and whilst waiting for those resources (and the animation to
  // start) we transfer a node from another document fragment that has already
  // started. In such a case we might receive milestone samples registered with
  // the already active container.
  if (GetTimeContainer()->IsPausedByType(nsSMILTimeContainer::PAUSE_BEGIN))
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
  if (mElementState == STATE_STARTUP && !aEndOnly)
    return;

  PRBool finishedSeek = PR_FALSE;
  if (GetTimeContainer()->IsSeeking() && mSeekState == SEEK_NOT_SEEKING) {
    mSeekState = mElementState == STATE_ACTIVE ?
                 SEEK_FORWARD_FROM_ACTIVE :
                 SEEK_FORWARD_FROM_INACTIVE;
  } else if (mSeekState != SEEK_NOT_SEEKING &&
             !GetTimeContainer()->IsSeeking()) {
    finishedSeek = PR_TRUE;
  }

  PRBool          stateChanged;
  nsSMILTimeValue sampleTime(aContainerTime);

  do {
#ifdef DEBUG
    // Check invariant
    if (mElementState == STATE_STARTUP || mElementState == STATE_POSTACTIVE) {
      NS_ABORT_IF_FALSE(!mCurrentInterval,
          "Shouldn't have current interval in startup or postactive states");
    } else {
      NS_ABORT_IF_FALSE(mCurrentInterval,
          "Should have current interval in waiting and active states");
    }
#endif

    stateChanged = PR_FALSE;

    switch (mElementState)
    {
    case STATE_STARTUP:
      {
        nsSMILInterval firstInterval;
        mElementState = GetNextInterval(nsnull, nsnull, nsnull, firstInterval)
         ? STATE_WAITING
         : STATE_POSTACTIVE;
        stateChanged = PR_TRUE;
        if (mElementState == STATE_WAITING) {
          mCurrentInterval = new nsSMILInterval(firstInterval);
          NotifyNewInterval();
        }
      }
      break;

    case STATE_WAITING:
      {
        if (mCurrentInterval->Begin()->Time() <= sampleTime) {
          mElementState = STATE_ACTIVE;
          mCurrentInterval->FixBegin();
          if (mClient) {
            mClient->Activate(mCurrentInterval->Begin()->Time().GetMillis());
          }
          if (mSeekState == SEEK_NOT_SEEKING) {
            FireTimeEventAsync(NS_SMIL_BEGIN, 0);
          }
          if (HasPlayed()) {
            Reset(); // Apply restart behaviour
            // The call to Reset() may mean that the end point of our current
            // interval should be changed and so we should update the interval
            // now. However, calling UpdateCurrentInterval could result in the
            // interval getting deleted (perhaps through some web of syncbase
            // dependencies) therefore we make updating the interval the last
            // thing we do. There is no guarantee that mCurrentInterval is set
            // after this.
            UpdateCurrentInterval();
          }
          stateChanged = PR_TRUE;
        }
      }
      break;

    case STATE_ACTIVE:
      {
        // Ending early will change the interval but we don't notify dependents
        // of the change until we have closed off the current interval (since we
        // don't want dependencies to un-end our early end).
        PRBool didApplyEarlyEnd = ApplyEarlyEnd(sampleTime);

        if (mCurrentInterval->End()->Time() <= sampleTime) {
          nsSMILInterval newInterval;
          mElementState =
            GetNextInterval(mCurrentInterval, nsnull, nsnull, newInterval)
            ? STATE_WAITING
            : STATE_POSTACTIVE;
          if (mClient) {
            mClient->Inactivate(mFillMode == FILL_FREEZE);
          }
          mCurrentInterval->FixEnd();
          if (mSeekState == SEEK_NOT_SEEKING) {
            FireTimeEventAsync(NS_SMIL_END, 0);
          }
          mCurrentRepeatIteration = 0;
          mOldIntervals.AppendElement(mCurrentInterval.forget());
          SampleFillValue();
          if (mElementState == STATE_WAITING) {
            mCurrentInterval = new nsSMILInterval(newInterval);
          }
          // We are now in a consistent state to dispatch notifications
          if (didApplyEarlyEnd) {
            NotifyChangedInterval(
                mOldIntervals[mOldIntervals.Length() - 1], PR_FALSE, PR_TRUE);
          }
          if (mElementState == STATE_WAITING) {
            NotifyNewInterval();
          }
          FilterHistory();
          stateChanged = PR_TRUE;
        } else {
          NS_ABORT_IF_FALSE(!didApplyEarlyEnd,
              "We got an early end, but didn't end");
          nsSMILTime beginTime = mCurrentInterval->Begin()->Time().GetMillis();
          NS_ASSERTION(aContainerTime >= beginTime,
                       "Sample time should not precede current interval");
          nsSMILTime activeTime = aContainerTime - beginTime;
          SampleSimpleTime(activeTime);
          // We register our repeat times as milestones (except when we're
          // seeking) so we should get a sample at exactly the time we repeat.
          // (And even when we are seeking we want to update
          // mCurrentRepeatIteration so we do that first before testing the seek
          // state.)
          PRUint32 prevRepeatIteration = mCurrentRepeatIteration;
          if (ActiveTimeToSimpleTime(activeTime, mCurrentRepeatIteration)==0 &&
              mCurrentRepeatIteration != prevRepeatIteration &&
              mCurrentRepeatIteration &&
              mSeekState == SEEK_NOT_SEEKING) {
            FireTimeEventAsync(NS_SMIL_REPEAT,
                          static_cast<PRInt32>(mCurrentRepeatIteration));
          }
        }
      }
      break;

    case STATE_POSTACTIVE:
      break;
    }

  // Generally we continue driving the state machine so long as we have changed
  // state. However, for end samples we only drive the state machine as far as
  // the waiting or postactive state because we don't want to commit to any new
  // interval (by transitioning to the active state) until all the end samples
  // have finished and we then have complete information about the available
  // instance times upon which to base our next interval.
  } while (stateChanged && (!aEndOnly || (mElementState != STATE_WAITING &&
                                          mElementState != STATE_POSTACTIVE)));

  if (finishedSeek) {
    DoPostSeek();
  }
  RegisterMilestone();
}

void
nsSMILTimedElement::HandleContainerTimeChange()
{
  // In future we could possibly introduce a separate change notice for time
  // container changes and only notify those dependents who live in other time
  // containers. For now we don't bother because when we re-resolve the time in
  // the nsSMILTimeValueSpec we'll check if anything has changed and if not, we
  // won't go any further.
  if (mElementState == STATE_WAITING || mElementState == STATE_ACTIVE) {
    NotifyChangedInterval(mCurrentInterval, PR_FALSE, PR_FALSE);
  }
}

namespace
{
  PRBool
  RemoveNonDynamic(nsSMILInstanceTime* aInstanceTime)
  {
    // Generally dynamically-generated instance times (DOM calls, event-based
    // times) are not associated with their creator nsSMILTimeValueSpec since
    // they may outlive them.
    NS_ABORT_IF_FALSE(!aInstanceTime->IsDynamic() ||
         !aInstanceTime->GetCreator(),
        "Dynamic instance time should be unlinked from its creator");
    return !aInstanceTime->IsDynamic() && !aInstanceTime->ShouldPreserve();
  }
}

void
nsSMILTimedElement::Rewind()
{
  NS_ABORT_IF_FALSE(mAnimationElement,
      "Got rewind request before being attached to an animation element");

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
    mSeekState = mElementState == STATE_ACTIVE ?
                 SEEK_BACKWARD_FROM_ACTIVE :
                 SEEK_BACKWARD_FROM_INACTIVE;
  }
  NS_ABORT_IF_FALSE(mSeekState == SEEK_BACKWARD_FROM_INACTIVE ||
                    mSeekState == SEEK_BACKWARD_FROM_ACTIVE,
                    "Rewind in the middle of a forwards seek?");

  // Putting us in the startup state will ensure we skip doing any interval
  // updates
  mElementState = STATE_STARTUP;
  ClearIntervals();

  UnsetBeginSpec(RemoveNonDynamic);
  UnsetEndSpec(RemoveNonDynamic);

  if (mClient) {
    mClient->Inactivate(PR_FALSE);
  }

  if (mAnimationElement->HasAnimAttr(nsGkAtoms::begin)) {
    nsAutoString attValue;
    mAnimationElement->GetAnimAttr(nsGkAtoms::begin, attValue);
    SetBeginSpec(attValue, &mAnimationElement->AsElement(), RemoveNonDynamic);
  }

  if (mAnimationElement->HasAnimAttr(nsGkAtoms::end)) {
    nsAutoString attValue;
    mAnimationElement->GetAnimAttr(nsGkAtoms::end, attValue);
    SetEndSpec(attValue, &mAnimationElement->AsElement(), RemoveNonDynamic);
  }

  mPrevRegisteredMilestone = sMaxMilestone;
  RegisterMilestone();
  NS_ABORT_IF_FALSE(!mCurrentInterval,
                    "Current interval is set at end of rewind");
}

namespace
{
  PRBool
  RemoveNonDOM(nsSMILInstanceTime* aInstanceTime)
  {
    return !aInstanceTime->FromDOM() && !aInstanceTime->ShouldPreserve();
  }
}

PRBool
nsSMILTimedElement::SetAttr(nsIAtom* aAttribute, const nsAString& aValue,
                            nsAttrValue& aResult,
                            Element* aContextNode,
                            nsresult* aParseResult)
{
  PRBool foundMatch = PR_TRUE;
  nsresult parseResult = NS_OK;

  if (aAttribute == nsGkAtoms::begin) {
    parseResult = SetBeginSpec(aValue, aContextNode, RemoveNonDOM);
  } else if (aAttribute == nsGkAtoms::dur) {
    parseResult = SetSimpleDuration(aValue);
  } else if (aAttribute == nsGkAtoms::end) {
    parseResult = SetEndSpec(aValue, aContextNode, RemoveNonDOM);
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
    foundMatch = PR_FALSE;
  }

  if (foundMatch) {
    aResult.SetTo(aValue);
    if (aParseResult) {
      *aParseResult = parseResult;
    }
  }

  return foundMatch;
}

PRBool
nsSMILTimedElement::UnsetAttr(nsIAtom* aAttribute)
{
  PRBool foundMatch = PR_TRUE;

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
    foundMatch = PR_FALSE;
  }

  return foundMatch;
}

//----------------------------------------------------------------------
// Setters and unsetters

nsresult
nsSMILTimedElement::SetBeginSpec(const nsAString& aBeginSpec,
                                 Element* aContextNode,
                                 RemovalTestFunction aRemove)
{
  return SetBeginOrEndSpec(aBeginSpec, aContextNode, PR_TRUE /*isBegin*/,
                           aRemove);
}

void
nsSMILTimedElement::UnsetBeginSpec(RemovalTestFunction aRemove)
{
  ClearSpecs(mBeginSpecs, mBeginInstances, aRemove);
  UpdateCurrentInterval();
}

nsresult
nsSMILTimedElement::SetEndSpec(const nsAString& aEndSpec,
                               Element* aContextNode,
                               RemovalTestFunction aRemove)
{
  return SetBeginOrEndSpec(aEndSpec, aContextNode, PR_FALSE /*!isBegin*/,
                           aRemove);
}

void
nsSMILTimedElement::UnsetEndSpec(RemovalTestFunction aRemove)
{
  ClearSpecs(mEndSpecs, mEndInstances, aRemove);
  UpdateCurrentInterval();
}

nsresult
nsSMILTimedElement::SetSimpleDuration(const nsAString& aDurSpec)
{
  nsSMILTimeValue duration;
  PRBool isMedia;
  nsresult rv;

  rv = nsSMILParserUtils::ParseClockValue(aDurSpec, &duration,
          nsSMILParserUtils::kClockValueAllowIndefinite, &isMedia);

  if (NS_FAILED(rv)) {
    mSimpleDur.SetIndefinite();
    return NS_ERROR_FAILURE;
  }

  if (duration.IsDefinite() && duration.GetMillis() == 0L) {
    mSimpleDur.SetIndefinite();
    return NS_ERROR_FAILURE;
  }

  //
  // SVG-specific: "For SVG's animation elements, if "media" is specified, the
  // attribute will be ignored." (SVG 1.1, section 19.2.6)
  //
  if (isMedia)
    duration.SetIndefinite();

  // mSimpleDur should never be unresolved. ParseClockValue will either set
  // duration to resolved/indefinite/media or will return a failure code.
  NS_ASSERTION(duration.IsDefinite() || duration.IsIndefinite(),
    "Setting unresolved simple duration");

  mSimpleDur = duration;
  UpdateCurrentInterval();

  return NS_OK;
}

void
nsSMILTimedElement::UnsetSimpleDuration()
{
  mSimpleDur.SetIndefinite();
  UpdateCurrentInterval();
}

nsresult
nsSMILTimedElement::SetMin(const nsAString& aMinSpec)
{
  nsSMILTimeValue duration;
  PRBool isMedia;
  nsresult rv;

  rv = nsSMILParserUtils::ParseClockValue(aMinSpec, &duration, 0, &isMedia);

  if (isMedia) {
    duration.SetMillis(0L);
  }

  if (NS_FAILED(rv) || !duration.IsDefinite()) {
    mMin.SetMillis(0L);
    return NS_ERROR_FAILURE;
  }

  if (duration.GetMillis() < 0L) {
    mMin.SetMillis(0L);
    return NS_ERROR_FAILURE;
  }

  mMin = duration;
  UpdateCurrentInterval();

  return NS_OK;
}

void
nsSMILTimedElement::UnsetMin()
{
  mMin.SetMillis(0L);
  UpdateCurrentInterval();
}

nsresult
nsSMILTimedElement::SetMax(const nsAString& aMaxSpec)
{
  nsSMILTimeValue duration;
  PRBool isMedia;
  nsresult rv;

  rv = nsSMILParserUtils::ParseClockValue(aMaxSpec, &duration,
          nsSMILParserUtils::kClockValueAllowIndefinite, &isMedia);

  if (isMedia)
    duration.SetIndefinite();

  if (NS_FAILED(rv) || (!duration.IsDefinite() && !duration.IsIndefinite())) {
    mMax.SetIndefinite();
    return NS_ERROR_FAILURE;
  }

  if (duration.IsDefinite() && duration.GetMillis() <= 0L) {
    mMax.SetIndefinite();
    return NS_ERROR_FAILURE;
  }

  mMax = duration;
  UpdateCurrentInterval();

  return NS_OK;
}

void
nsSMILTimedElement::UnsetMax()
{
  mMax.SetIndefinite();
  UpdateCurrentInterval();
}

nsresult
nsSMILTimedElement::SetRestart(const nsAString& aRestartSpec)
{
  nsAttrValue temp;
  PRBool parseResult
    = temp.ParseEnumValue(aRestartSpec, sRestartModeTable, PR_TRUE);
  mRestartMode = parseResult
               ? nsSMILRestartMode(temp.GetEnumValue())
               : RESTART_ALWAYS;
  UpdateCurrentInterval();
  return parseResult ? NS_OK : NS_ERROR_FAILURE;
}

void
nsSMILTimedElement::UnsetRestart()
{
  mRestartMode = RESTART_ALWAYS;
  UpdateCurrentInterval();
}

nsresult
nsSMILTimedElement::SetRepeatCount(const nsAString& aRepeatCountSpec)
{
  nsSMILRepeatCount newRepeatCount;
  nsresult rv =
    nsSMILParserUtils::ParseRepeatCount(aRepeatCountSpec, newRepeatCount);

  if (NS_SUCCEEDED(rv)) {
    mRepeatCount = newRepeatCount;
  } else {
    mRepeatCount.Unset();
  }

  UpdateCurrentInterval();

  return rv;
}

void
nsSMILTimedElement::UnsetRepeatCount()
{
  mRepeatCount.Unset();
  UpdateCurrentInterval();
}

nsresult
nsSMILTimedElement::SetRepeatDur(const nsAString& aRepeatDurSpec)
{
  nsresult rv;
  nsSMILTimeValue duration;

  rv = nsSMILParserUtils::ParseClockValue(aRepeatDurSpec, &duration,
          nsSMILParserUtils::kClockValueAllowIndefinite);

  if (NS_FAILED(rv) || (!duration.IsDefinite() && !duration.IsIndefinite())) {
    mRepeatDur.SetUnresolved();
    return NS_ERROR_FAILURE;
  }

  mRepeatDur = duration;
  UpdateCurrentInterval();

  return NS_OK;
}

void
nsSMILTimedElement::UnsetRepeatDur()
{
  mRepeatDur.SetUnresolved();
  UpdateCurrentInterval();
}

nsresult
nsSMILTimedElement::SetFillMode(const nsAString& aFillModeSpec)
{
  PRUint16 previousFillMode = mFillMode;

  nsAttrValue temp;
  PRBool parseResult =
    temp.ParseEnumValue(aFillModeSpec, sFillModeTable, PR_TRUE);
  mFillMode = parseResult
            ? nsSMILFillMode(temp.GetEnumValue())
            : FILL_REMOVE;

  // Check if we're in a fill-able state: i.e. we've played at least one
  // interval and are now between intervals or at the end of all intervals
  PRBool isFillable = HasPlayed() &&
    (mElementState == STATE_WAITING || mElementState == STATE_POSTACTIVE);

  if (mClient && mFillMode != previousFillMode && isFillable) {
    mClient->Inactivate(mFillMode == FILL_FREEZE);
    SampleFillValue();
  }

  return parseResult ? NS_OK : NS_ERROR_FAILURE;
}

void
nsSMILTimedElement::UnsetFillMode()
{
  PRUint16 previousFillMode = mFillMode;
  mFillMode = FILL_REMOVE;
  if ((mElementState == STATE_WAITING || mElementState == STATE_POSTACTIVE) &&
      previousFillMode == FILL_FREEZE && mClient && HasPlayed())
    mClient->Inactivate(PR_FALSE);
}

void
nsSMILTimedElement::AddDependent(nsSMILTimeValueSpec& aDependent)
{
  // There's probably no harm in attempting to register a dependent
  // nsSMILTimeValueSpec twice, but we're not expecting it to happen.
  NS_ABORT_IF_FALSE(!mTimeDependents.GetEntry(&aDependent),
      "nsSMILTimeValueSpec is already registered as a dependency");
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

void
nsSMILTimedElement::RemoveDependent(nsSMILTimeValueSpec& aDependent)
{
  mTimeDependents.RemoveEntry(&aDependent);
}

PRBool
nsSMILTimedElement::IsTimeDependent(const nsSMILTimedElement& aOther) const
{
  const nsSMILInstanceTime* thisBegin = GetEffectiveBeginInstance();
  const nsSMILInstanceTime* otherBegin = aOther.GetEffectiveBeginInstance();

  if (!thisBegin || !otherBegin)
    return PR_FALSE;

  return thisBegin->IsDependentOn(*otherBegin);
}

void
nsSMILTimedElement::BindToTree(nsIContent* aContextNode)
{
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
    PRUint32 count = mBeginSpecs.Length();
    for (PRUint32 i = 0; i < count; ++i) {
      mBeginSpecs[i]->ResolveReferences(aContextNode);
    }

    count = mEndSpecs.Length();
    for (PRUint32 j = 0; j < count; ++j) {
      mEndSpecs[j]->ResolveReferences(aContextNode);
    }
  }

  RegisterMilestone();
}

void
nsSMILTimedElement::HandleTargetElementChange(Element* aNewTarget)
{
  AutoIntervalUpdateBatcher updateBatcher(*this);

  PRUint32 count = mBeginSpecs.Length();
  for (PRUint32 i = 0; i < count; ++i) {
    mBeginSpecs[i]->HandleTargetElementChange(aNewTarget);
  }

  count = mEndSpecs.Length();
  for (PRUint32 j = 0; j < count; ++j) {
    mEndSpecs[j]->HandleTargetElementChange(aNewTarget);
  }
}

void
nsSMILTimedElement::Traverse(nsCycleCollectionTraversalCallback* aCallback)
{
  PRUint32 count = mBeginSpecs.Length();
  for (PRUint32 i = 0; i < count; ++i) {
    nsSMILTimeValueSpec* beginSpec = mBeginSpecs[i];
    NS_ABORT_IF_FALSE(beginSpec,
        "null nsSMILTimeValueSpec in list of begin specs");
    beginSpec->Traverse(aCallback);
  }

  count = mEndSpecs.Length();
  for (PRUint32 j = 0; j < count; ++j) {
    nsSMILTimeValueSpec* endSpec = mEndSpecs[j];
    NS_ABORT_IF_FALSE(endSpec, "null nsSMILTimeValueSpec in list of end specs");
    endSpec->Traverse(aCallback);
  }
}

void
nsSMILTimedElement::Unlink()
{
  AutoIntervalUpdateBatcher updateBatcher(*this);

  // Remove dependencies on other elements
  PRUint32 count = mBeginSpecs.Length();
  for (PRUint32 i = 0; i < count; ++i) {
    nsSMILTimeValueSpec* beginSpec = mBeginSpecs[i];
    NS_ABORT_IF_FALSE(beginSpec,
        "null nsSMILTimeValueSpec in list of begin specs");
    beginSpec->Unlink();
  }

  count = mEndSpecs.Length();
  for (PRUint32 j = 0; j < count; ++j) {
    nsSMILTimeValueSpec* endSpec = mEndSpecs[j];
    NS_ABORT_IF_FALSE(endSpec, "null nsSMILTimeValueSpec in list of end specs");
    endSpec->Unlink();
  }

  ClearIntervals();

  // Make sure we don't notify other elements of new intervals
  mTimeDependents.Clear();
}

//----------------------------------------------------------------------
// Implementation helpers

nsresult
nsSMILTimedElement::SetBeginOrEndSpec(const nsAString& aSpec,
                                      Element* aContextNode,
                                      PRBool aIsBegin,
                                      RemovalTestFunction aRemove)
{
  PRInt32 start;
  PRInt32 end = -1;
  PRInt32 length;
  nsresult rv = NS_OK;
  TimeValueSpecList& timeSpecsList = aIsBegin ? mBeginSpecs : mEndSpecs;
  InstanceTimeList& instances = aIsBegin ? mBeginInstances : mEndInstances;

  ClearSpecs(timeSpecsList, instances, aRemove);

  AutoIntervalUpdateBatcher updateBatcher(*this);

  do {
    start = end + 1;
    end = aSpec.FindChar(';', start);
    length = (end == -1) ? -1 : end - start;
    nsAutoPtr<nsSMILTimeValueSpec>
      spec(new nsSMILTimeValueSpec(*this, aIsBegin));
    rv = spec->SetSpec(Substring(aSpec, start, length), aContextNode);
    if (NS_SUCCEEDED(rv)) {
      timeSpecsList.AppendElement(spec.forget());
    }
  } while (end != -1 && NS_SUCCEEDED(rv));

  if (NS_FAILED(rv)) {
    ClearSpecs(timeSpecsList, instances, aRemove);
  }

  return rv;
}

namespace
{
  // Adaptor functor for RemoveInstanceTimes that allows us to use function
  // pointers instead.
  // Without this we'd have to either templatize ClearSpecs and all its callers
  // or pass bool flags around to specify which removal function to use here.
  class NS_STACK_CLASS RemoveByFunction
  {
  public:
    RemoveByFunction(nsSMILTimedElement::RemovalTestFunction aFunction)
      : mFunction(aFunction) { }
    PRBool operator()(nsSMILInstanceTime* aInstanceTime, PRUint32 /*aIndex*/)
    {
      return mFunction(aInstanceTime);
    }

  private:
    nsSMILTimedElement::RemovalTestFunction mFunction;
  };
}

void
nsSMILTimedElement::ClearSpecs(TimeValueSpecList& aSpecs,
                               InstanceTimeList& aInstances,
                               RemovalTestFunction aRemove)
{
  aSpecs.Clear();
  RemoveByFunction removeByFunction(aRemove);
  RemoveInstanceTimes(aInstances, removeByFunction);
}

void
nsSMILTimedElement::ClearIntervals()
{
  if (mElementState != STATE_STARTUP) {
    mElementState = STATE_POSTACTIVE;
  }
  mCurrentRepeatIteration = 0;
  ResetCurrentInterval();

  // Remove old intervals
  for (PRInt32 i = mOldIntervals.Length() - 1; i >= 0; --i) {
    mOldIntervals[i]->Unlink();
  }
  mOldIntervals.Clear();
}

PRBool
nsSMILTimedElement::ApplyEarlyEnd(const nsSMILTimeValue& aSampleTime)
{
  // This should only be called within DoSampleAt as a helper function
  NS_ABORT_IF_FALSE(mElementState == STATE_ACTIVE,
      "Unexpected state to try to apply an early end");

  PRBool updated = PR_FALSE;

  // Only apply an early end if we're not already ending.
  if (mCurrentInterval->End()->Time() > aSampleTime) {
    nsSMILInstanceTime* earlyEnd = CheckForEarlyEnd(aSampleTime);
    if (earlyEnd) {
      if (earlyEnd->IsDependent()) {
        // Generate a new instance time for the early end since the
        // existing instance time is part of some dependency chain that we
        // don't want to participate in.
        nsRefPtr<nsSMILInstanceTime> newEarlyEnd =
          new nsSMILInstanceTime(earlyEnd->Time());
        mCurrentInterval->SetEnd(*newEarlyEnd);
      } else {
        mCurrentInterval->SetEnd(*earlyEnd);
      }
      updated = PR_TRUE;
    }
  }
  return updated;
}

namespace
{
  class NS_STACK_CLASS RemoveReset
  {
  public:
    RemoveReset(const nsSMILInstanceTime* aCurrentIntervalBegin)
      : mCurrentIntervalBegin(aCurrentIntervalBegin) { }
    PRBool operator()(nsSMILInstanceTime* aInstanceTime, PRUint32 /*aIndex*/)
    {
      // SMIL 3.0 section 5.4.3, 'Resetting element state':
      //   Any instance times associated with past Event-values, Repeat-values,
      //   Accesskey-values or added via DOM method calls are removed from the
      //   dependent begin and end instance times lists. In effect, all events
      //   and DOM methods calls in the past are cleared. This does not apply to
      //   an instance time that defines the begin of the current interval.
      return aInstanceTime->IsDynamic() &&
             !aInstanceTime->ShouldPreserve() &&
             (!mCurrentIntervalBegin || aInstanceTime != mCurrentIntervalBegin);
    }

  private:
    const nsSMILInstanceTime* mCurrentIntervalBegin;
  };
}

void
nsSMILTimedElement::Reset()
{
  RemoveReset resetBegin(mCurrentInterval ? mCurrentInterval->Begin() : nsnull);
  RemoveInstanceTimes(mBeginInstances, resetBegin);

  RemoveReset resetEnd(nsnull);
  RemoveInstanceTimes(mEndInstances, resetEnd);
}

void
nsSMILTimedElement::DoPostSeek()
{
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

  switch (mSeekState)
  {
  case SEEK_FORWARD_FROM_ACTIVE:
  case SEEK_BACKWARD_FROM_ACTIVE:
    if (mElementState != STATE_ACTIVE) {
      FireTimeEventAsync(NS_SMIL_END, 0);
    }
    break;

  case SEEK_FORWARD_FROM_INACTIVE:
  case SEEK_BACKWARD_FROM_INACTIVE:
    if (mElementState == STATE_ACTIVE) {
      FireTimeEventAsync(NS_SMIL_BEGIN, 0);
    }
    break;

  case SEEK_NOT_SEEKING:
    /* Do nothing */
    break;
  }

  mSeekState = SEEK_NOT_SEEKING;
}

void
nsSMILTimedElement::UnpreserveInstanceTimes(InstanceTimeList& aList)
{
  const nsSMILInterval* prevInterval = GetPreviousInterval();
  const nsSMILInstanceTime* cutoff = mCurrentInterval ?
      mCurrentInterval->Begin() :
      prevInterval ? prevInterval->Begin() : nsnull;
  PRUint32 count = aList.Length();
  for (PRUint32 i = 0; i < count; ++i) {
    nsSMILInstanceTime* instance = aList[i].get();
    if (!cutoff || cutoff->Time().CompareTo(instance->Time()) < 0) {
      instance->UnmarkShouldPreserve();
    }
  }
}

void
nsSMILTimedElement::FilterHistory()
{
  // We should filter the intervals first, since instance times still used in an
  // interval won't be filtered.
  FilterIntervals();
  FilterInstanceTimes(mBeginInstances);
  FilterInstanceTimes(mEndInstances);
}

void
nsSMILTimedElement::FilterIntervals()
{
  // We can filter old intervals that:
  //
  // a) are not the previous interval; AND
  // b) are not in the middle of a dependency chain
  //
  // Condition (a) is necessary since the previous interval is used for applying
  // fill effects and updating the current interval.
  //
  // Condition (b) is necessary since even if this interval itself is not
  // active, it may be part of a dependency chain that includes active
  // intervals. Such chains are used to establish priorities within the
  // animation sandwich.
  //
  // Although the above conditions allow us to safely filter intervals for most
  // scenarios they do not cover all cases and there will still be scenarios
  // that generate intervals indefinitely. In such a case we simply set
  // a maximum number of intervals and drop any intervals beyond that threshold.

  PRUint32 threshold = mOldIntervals.Length() > sMaxNumIntervals ?
                       mOldIntervals.Length() - sMaxNumIntervals :
                       0;
  IntervalList filteredList;
  for (PRUint32 i = 0; i < mOldIntervals.Length(); ++i)
  {
    nsSMILInterval* interval = mOldIntervals[i].get();
    if (i + 1 < mOldIntervals.Length() /*skip previous interval*/ &&
        (i < threshold || !interval->IsDependencyChainLink())) {
      interval->Unlink(PR_TRUE /*filtered, not deleted*/);
    } else {
      filteredList.AppendElement(mOldIntervals[i].forget());
    }
  }
  mOldIntervals.Clear();
  mOldIntervals.SwapElements(filteredList);
}

namespace
{
  class NS_STACK_CLASS RemoveFiltered
  {
  public:
    RemoveFiltered(nsSMILTimeValue aCutoff) : mCutoff(aCutoff) { }
    PRBool operator()(nsSMILInstanceTime* aInstanceTime, PRUint32 /*aIndex*/)
    {
      // We can filter instance times that:
      // a) Precede the end point of the previous interval; AND
      // b) Are NOT syncbase times that might be updated to a time after the end
      //    point of the previous interval; AND
      // c) Are NOT fixed end points in any remaining interval.
      return aInstanceTime->Time() < mCutoff &&
             aInstanceTime->IsFixedTime() &&
             !aInstanceTime->ShouldPreserve();
    }

  private:
    nsSMILTimeValue mCutoff;
  };

  class NS_STACK_CLASS RemoveBelowThreshold
  {
  public:
    RemoveBelowThreshold(PRUint32 aThreshold,
                         nsTArray<const nsSMILInstanceTime *>& aTimesToKeep)
      : mThreshold(aThreshold),
        mTimesToKeep(aTimesToKeep) { }
    PRBool operator()(nsSMILInstanceTime* aInstanceTime, PRUint32 aIndex)
    {
      return aIndex < mThreshold && !mTimesToKeep.Contains(aInstanceTime);
    }

  private:
    PRUint32 mThreshold;
    nsTArray<const nsSMILInstanceTime *>& mTimesToKeep;
  };
}

void
nsSMILTimedElement::FilterInstanceTimes(InstanceTimeList& aList)
{
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
    PRUint32 threshold = aList.Length() - sMaxNumInstanceTimes;
    // There are a few instance times we should keep though, notably:
    // - the current interval begin time,
    // - the previous interval end time (see note in RemoveInstanceTimes)
    nsTArray<const nsSMILInstanceTime *> timesToKeep;
    if (mCurrentInterval) {
      timesToKeep.AppendElement(mCurrentInterval->Begin());
    }
    const nsSMILInterval* prevInterval = GetPreviousInterval();
    if (prevInterval) {
      timesToKeep.AppendElement(prevInterval->End());
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
PRBool
nsSMILTimedElement::GetNextInterval(const nsSMILInterval* aPrevInterval,
                                    const nsSMILInterval* aReplacedInterval,
                                    const nsSMILInstanceTime* aFixedBeginTime,
                                    nsSMILInterval& aResult) const
{
  NS_ABORT_IF_FALSE(!aFixedBeginTime || aFixedBeginTime->Time().IsDefinite(),
      "Unresolved begin time specified for interval start");
  static const nsSMILTimeValue zeroTime(0L);

  if (mRestartMode == RESTART_NEVER && aPrevInterval)
    return PR_FALSE;

  // Calc starting point
  nsSMILTimeValue beginAfter;
  PRBool prevIntervalWasZeroDur = PR_FALSE;
  if (aPrevInterval) {
    beginAfter = aPrevInterval->End()->Time();
    prevIntervalWasZeroDur
      = aPrevInterval->End()->Time() == aPrevInterval->Begin()->Time();
    if (aFixedBeginTime) {
      prevIntervalWasZeroDur &=
        aPrevInterval->Begin()->Time() == aFixedBeginTime->Time();
    }
  } else {
    beginAfter.SetMillis(LL_MININT);
  }

  nsRefPtr<nsSMILInstanceTime> tempBegin;
  nsRefPtr<nsSMILInstanceTime> tempEnd;

  while (PR_TRUE) {
    // Calculate begin time
    if (aFixedBeginTime) {
      if (aFixedBeginTime->Time() < beginAfter) {
        return PR_FALSE;
      }
      // our ref-counting is not const-correct
      tempBegin = const_cast<nsSMILInstanceTime*>(aFixedBeginTime);
    } else if ((!mAnimationElement ||
                !mAnimationElement->HasAnimAttr(nsGkAtoms::begin)) &&
               beginAfter <= zeroTime) {
      tempBegin = new nsSMILInstanceTime(nsSMILTimeValue(0));
    } else {
      PRInt32 beginPos = 0;
      // If we're updating the current interval then skip any begin time that is
      // dependent on the current interval's begin time. e.g.
      //   <animate id="a" begin="b.begin; a.begin+2s"...
      // If b's interval disappears whilst 'a' is in the waiting state the begin
      // time at "a.begin+2s" should be skipped since 'a' never begun.
      do {
        tempBegin =
          GetNextGreaterOrEqual(mBeginInstances, beginAfter, beginPos);
        if (!tempBegin || !tempBegin->Time().IsDefinite()) {
          return PR_FALSE;
        }
      } while (aReplacedInterval &&
               tempBegin->GetBaseTime() == aReplacedInterval->Begin());
    }
    NS_ABORT_IF_FALSE(tempBegin && tempBegin->Time().IsDefinite() &&
        tempBegin->Time() >= beginAfter,
        "Got a bad begin time while fetching next interval");

    // Calculate end time
    {
      PRInt32 endPos = 0;
      // As above with begin times, avoid creating self-referential loops
      // between instance times by checking that the newly found end instance
      // time is not already dependent on the end of the current interval.
      do {
        tempEnd =
          GetNextGreaterOrEqual(mEndInstances, tempBegin->Time(), endPos);
      } while (tempEnd && aReplacedInterval &&
               tempEnd->GetBaseTime() == aReplacedInterval->End());

      // If the last interval ended at the same point and was zero-duration and
      // this one is too, look for another end to use instead
      if (tempEnd && tempEnd->Time() == tempBegin->Time() &&
          prevIntervalWasZeroDur) {
        tempEnd = GetNextGreater(mEndInstances, tempBegin->Time(), endPos);
      }

      // If all the ends are before the beginning we have a bad interval UNLESS:
      // a) We never had any end attribute to begin with (and hence we should
      //    just use the active duration after allowing for the possibility of
      //    an end instance provided by a DOM call), OR
      // b) We have no resolved (not incl. indefinite) end instances
      //    (SMIL only says "if the instance list is empty"--but if we have
      //    indefinite/unresolved instance times then there must be a good
      //    reason we haven't used them (since they'll be >= tempBegin) such as
      //    avoiding creating a self-referential loop. In any case, the interval
      //    should be allowed to be open.), OR
      // c) We have end events which leave the interval open-ended.
      PRBool openEndedIntervalOk = mEndSpecs.IsEmpty() ||
                                   !HaveResolvedEndTimes() ||
                                   EndHasEventConditions();
      if (!tempEnd && !openEndedIntervalOk)
        return PR_FALSE; // Bad interval

      nsSMILTimeValue intervalEnd = tempEnd
                                  ? tempEnd->Time() : nsSMILTimeValue();
      nsSMILTimeValue activeEnd = CalcActiveEnd(tempBegin->Time(), intervalEnd);

      if (!tempEnd || intervalEnd != activeEnd) {
        tempEnd = new nsSMILInstanceTime(activeEnd);
      }
    }
    NS_ABORT_IF_FALSE(tempEnd, "Failed to get end point for next interval");

    // If we get two zero-length intervals in a row we will potentially have an
    // infinite loop so we break it here by searching for the next begin time
    // greater than tempEnd on the next time around.
    if (tempEnd->Time().IsDefinite() && tempBegin->Time() == tempEnd->Time()) {
      if (prevIntervalWasZeroDur) {
        beginAfter.SetMillis(tempEnd->Time().GetMillis() + 1);
        prevIntervalWasZeroDur = PR_FALSE;
        continue;
      }
      prevIntervalWasZeroDur = PR_TRUE;
    }

    // Check for valid interval
    if (tempEnd->Time() > zeroTime ||
       (tempBegin->Time() == zeroTime && tempEnd->Time() == zeroTime)) {
      aResult.Set(*tempBegin, *tempEnd);
      return PR_TRUE;
    }

    if (mRestartMode == RESTART_NEVER) {
      // tempEnd <= 0 so we're going to loop which effectively means restarting
      return PR_FALSE;
    }

    beginAfter = tempEnd->Time();
  }
  NS_NOTREACHED("Hmm... we really shouldn't be here");

  return PR_FALSE;
}

nsSMILInstanceTime*
nsSMILTimedElement::GetNextGreater(const InstanceTimeList& aList,
                                   const nsSMILTimeValue& aBase,
                                   PRInt32& aPosition) const
{
  nsSMILInstanceTime* result = nsnull;
  while ((result = GetNextGreaterOrEqual(aList, aBase, aPosition)) &&
         result->Time() == aBase);
  return result;
}

nsSMILInstanceTime*
nsSMILTimedElement::GetNextGreaterOrEqual(const InstanceTimeList& aList,
                                          const nsSMILTimeValue& aBase,
                                          PRInt32& aPosition) const
{
  nsSMILInstanceTime* result = nsnull;
  PRInt32 count = aList.Length();

  for (; aPosition < count && !result; ++aPosition) {
    nsSMILInstanceTime* val = aList[aPosition].get();
    NS_ABORT_IF_FALSE(val, "NULL instance time in list");
    if (val->Time() >= aBase) {
      result = val;
    }
  }

  return result;
}

/**
 * @see SMILANIM 3.3.4
 */
nsSMILTimeValue
nsSMILTimedElement::CalcActiveEnd(const nsSMILTimeValue& aBegin,
                                  const nsSMILTimeValue& aEnd) const
{
  nsSMILTimeValue result;

  NS_ABORT_IF_FALSE(mSimpleDur.IsDefinite() || mSimpleDur.IsIndefinite(),
    "Unresolved simple duration in CalcActiveEnd");
  NS_ABORT_IF_FALSE(aBegin.IsDefinite(),
    "Unresolved begin time in CalcActiveEnd");

  if (mRepeatDur.IsIndefinite()) {
    result.SetIndefinite();
  } else {
    result = GetRepeatDuration();
  }

  if (aEnd.IsDefinite()) {
    nsSMILTime activeDur = aEnd.GetMillis() - aBegin.GetMillis();

    if (result.IsDefinite()) {
      result.SetMillis(NS_MIN(result.GetMillis(), activeDur));
    } else {
      result.SetMillis(activeDur);
    }
  }

  result = ApplyMinAndMax(result);

  if (result.IsDefinite()) {
    nsSMILTime activeEnd = result.GetMillis() + aBegin.GetMillis();
    result.SetMillis(activeEnd);
  }

  return result;
}

nsSMILTimeValue
nsSMILTimedElement::GetRepeatDuration() const
{
  nsSMILTimeValue result;

  if (mRepeatCount.IsDefinite() && mRepeatDur.IsDefinite()) {
    if (mSimpleDur.IsDefinite()) {
      nsSMILTime activeDur =
        nsSMILTime(mRepeatCount * double(mSimpleDur.GetMillis()));
      result.SetMillis(NS_MIN(activeDur, mRepeatDur.GetMillis()));
    } else {
      result = mRepeatDur;
    }
  } else if (mRepeatCount.IsDefinite() && mSimpleDur.IsDefinite()) {
    nsSMILTime activeDur =
      nsSMILTime(mRepeatCount * double(mSimpleDur.GetMillis()));
    result.SetMillis(activeDur);
  } else if (mRepeatDur.IsDefinite()) {
    result = mRepeatDur;
  } else if (mRepeatCount.IsIndefinite()) {
    result.SetIndefinite();
  } else {
    result = mSimpleDur;
  }

  return result;
}

nsSMILTimeValue
nsSMILTimedElement::ApplyMinAndMax(const nsSMILTimeValue& aDuration) const
{
  if (!aDuration.IsDefinite() && !aDuration.IsIndefinite()) {
    return aDuration;
  }

  if (mMax < mMin) {
    return aDuration;
  }

  nsSMILTimeValue result;

  if (aDuration > mMax) {
    result = mMax;
  } else if (aDuration < mMin) {
    nsSMILTimeValue repeatDur = GetRepeatDuration();
    result = mMin > repeatDur ? repeatDur : mMin;
  } else {
    result = aDuration;
  }

  return result;
}

nsSMILTime
nsSMILTimedElement::ActiveTimeToSimpleTime(nsSMILTime aActiveTime,
                                           PRUint32& aRepeatIteration)
{
  nsSMILTime result;

  NS_ASSERTION(mSimpleDur.IsDefinite() || mSimpleDur.IsIndefinite(),
      "Unresolved simple duration in ActiveTimeToSimpleTime");
  NS_ASSERTION(aActiveTime >= 0, "Expecting non-negative active time");
  // Note that a negative aActiveTime will give us a negative value for
  // aRepeatIteration, which is bad because aRepeatIteration is unsigned

  if (mSimpleDur.IsIndefinite() || mSimpleDur.GetMillis() == 0L) {
    aRepeatIteration = 0;
    result = aActiveTime;
  } else {
    result = aActiveTime % mSimpleDur.GetMillis();
    aRepeatIteration = (PRUint32)(aActiveTime / mSimpleDur.GetMillis());
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
nsSMILInstanceTime*
nsSMILTimedElement::CheckForEarlyEnd(
    const nsSMILTimeValue& aContainerTime) const
{
  NS_ABORT_IF_FALSE(mCurrentInterval,
      "Checking for an early end but the current interval is not set");
  if (mRestartMode != RESTART_ALWAYS)
    return nsnull;

  PRInt32 position = 0;
  nsSMILInstanceTime* nextBegin =
    GetNextGreater(mBeginInstances, mCurrentInterval->Begin()->Time(),
                   position);

  if (nextBegin &&
      nextBegin->Time() > mCurrentInterval->Begin()->Time() &&
      nextBegin->Time() < mCurrentInterval->End()->Time() &&
      nextBegin->Time() <= aContainerTime) {
    return nextBegin;
  }

  return nsnull;
}

void
nsSMILTimedElement::UpdateCurrentInterval(PRBool aForceChangeNotice)
{
  // Check if updates are currently blocked (batched)
  if (mDeferIntervalUpdates) {
    mDoDeferredUpdate = PR_TRUE;
    return;
  }

  // We adopt the convention of not resolving intervals until the first
  // sample. Otherwise, every time each attribute is set we'll re-resolve the
  // current interval and notify all our time dependents of the change.
  //
  // The disadvantage of deferring resolving the interval is that DOM calls to
  // to getStartTime will throw an INVALID_STATE_ERR exception until the
  // document timeline begins since the start time has not yet been resolved.
  if (mElementState == STATE_STARTUP)
    return;

  // Check that we aren't stuck in infinite recursion updating some syncbase
  // dependencies. Generally such situations should be detected in advance and
  // the chain broken in a sensible and predictable manner, so if we're hitting
  // this assertion we need to work out how to detect the case that's causing
  // it. In release builds, just bail out before we overflow the stack.
  AutoRestore<PRUint16> depthRestorer(mUpdateIntervalRecursionDepth);
  if (++mUpdateIntervalRecursionDepth > sMaxUpdateIntervalRecursionDepth) {
    NS_ABORT_IF_FALSE(PR_FALSE,
        "Update current interval recursion depth exceeded threshold");
    return;
  }

  // If the interval is active the begin time is fixed.
  const nsSMILInstanceTime* beginTime = mElementState == STATE_ACTIVE
                                      ? mCurrentInterval->Begin()
                                      : nsnull;
  nsSMILInterval updatedInterval;
  if (GetNextInterval(GetPreviousInterval(), mCurrentInterval,
                      beginTime, updatedInterval)) {

    if (mElementState == STATE_POSTACTIVE) {

      NS_ABORT_IF_FALSE(!mCurrentInterval,
          "In postactive state but the interval has been set");
      mCurrentInterval = new nsSMILInterval(updatedInterval);
      mElementState = STATE_WAITING;
      NotifyNewInterval();

    } else {

      PRBool beginChanged = PR_FALSE;
      PRBool endChanged   = PR_FALSE;

      if (mElementState != STATE_ACTIVE &&
          !updatedInterval.Begin()->SameTimeAndBase(
            *mCurrentInterval->Begin())) {
        mCurrentInterval->SetBegin(*updatedInterval.Begin());
        beginChanged = PR_TRUE;
      }

      if (!updatedInterval.End()->SameTimeAndBase(*mCurrentInterval->End())) {
        mCurrentInterval->SetEnd(*updatedInterval.End());
        endChanged = PR_TRUE;
      }

      if (beginChanged || endChanged || aForceChangeNotice) {
        NotifyChangedInterval(mCurrentInterval, beginChanged, endChanged);
      }
    }

    // There's a chance our next milestone has now changed, so update the time
    // container
    RegisterMilestone();
  } else { // GetNextInterval failed: Current interval is no longer valid
    if (mElementState == STATE_ACTIVE) {
      // The interval is active so we can't just delete it, instead trim it so
      // that begin==end.
      if (!mCurrentInterval->End()->SameTimeAndBase(*mCurrentInterval->Begin()))
      {
        mCurrentInterval->SetEnd(*mCurrentInterval->Begin());
        NotifyChangedInterval(mCurrentInterval, PR_FALSE, PR_TRUE);
      }
      // The transition to the postactive state will take place on the next
      // sample (along with firing end events, clearing intervals etc.)
      RegisterMilestone();
    } else if (mElementState == STATE_WAITING) {
      mElementState = STATE_POSTACTIVE;
      ResetCurrentInterval();
    }
  }
}

void
nsSMILTimedElement::SampleSimpleTime(nsSMILTime aActiveTime)
{
  if (mClient) {
    PRUint32 repeatIteration;
    nsSMILTime simpleTime =
      ActiveTimeToSimpleTime(aActiveTime, repeatIteration);
    mClient->SampleAt(simpleTime, mSimpleDur, repeatIteration);
  }
}

void
nsSMILTimedElement::SampleFillValue()
{
  if (mFillMode != FILL_FREEZE || !mClient)
    return;

  const nsSMILInterval* prevInterval = GetPreviousInterval();
  NS_ABORT_IF_FALSE(prevInterval,
      "Attempting to sample fill value but there is no previous interval");
  NS_ABORT_IF_FALSE(prevInterval->End()->Time().IsDefinite() &&
      prevInterval->End()->IsFixedTime(),
      "Attempting to sample fill value but the endpoint of the previous "
      "interval is not resolved and fixed");

  nsSMILTime activeTime = prevInterval->End()->Time().GetMillis() -
                          prevInterval->Begin()->Time().GetMillis();

  PRUint32 repeatIteration;
  nsSMILTime simpleTime =
    ActiveTimeToSimpleTime(activeTime, repeatIteration);

  if (simpleTime == 0L && repeatIteration) {
    mClient->SampleLastValue(--repeatIteration);
  } else {
    mClient->SampleAt(simpleTime, mSimpleDur, repeatIteration);
  }
}

nsresult
nsSMILTimedElement::AddInstanceTimeFromCurrentTime(nsSMILTime aCurrentTime,
    double aOffsetSeconds, PRBool aIsBegin)
{
  double offset = aOffsetSeconds * PR_MSEC_PER_SEC;

  // Check we won't overflow the range of nsSMILTime
  if (aCurrentTime + NS_round(offset) > LL_MAXINT)
    return NS_ERROR_ILLEGAL_VALUE;

  nsSMILTimeValue timeVal(aCurrentTime + PRInt64(NS_round(offset)));

  nsRefPtr<nsSMILInstanceTime> instanceTime =
    new nsSMILInstanceTime(timeVal, nsSMILInstanceTime::SOURCE_DOM);

  AddInstanceTime(instanceTime, aIsBegin);

  return NS_OK;
}

void
nsSMILTimedElement::RegisterMilestone()
{
  nsSMILTimeContainer* container = GetTimeContainer();
  if (!container)
    return;
  NS_ABORT_IF_FALSE(mAnimationElement,
      "Got a time container without an owning animation element");

  nsSMILMilestone nextMilestone;
  if (!GetNextMilestone(nextMilestone))
    return;

  // This method is called every time we might possibly have updated our
  // current interval, but since nsSMILTimeContainer makes no attempt to filter
  // out redundant milestones we do some rudimentary filtering here. It's not
  // perfect, but unnecessary samples are fairly cheap.
  if (nextMilestone >= mPrevRegisteredMilestone)
    return;

  container->AddMilestone(nextMilestone, *mAnimationElement);
  mPrevRegisteredMilestone = nextMilestone;
}

PRBool
nsSMILTimedElement::GetNextMilestone(nsSMILMilestone& aNextMilestone) const
{
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

  switch (mElementState)
  {
  case STATE_STARTUP:
    // All elements register for an initial end sample at t=0 where we resolve
    // our initial interval.
    aNextMilestone.mIsEnd = PR_TRUE; // Initial sample should be an end sample
    aNextMilestone.mTime = 0;
    return PR_TRUE;

  case STATE_WAITING:
    NS_ABORT_IF_FALSE(mCurrentInterval,
        "In waiting state but the current interval has not been set");
    aNextMilestone.mIsEnd = PR_FALSE;
    aNextMilestone.mTime = mCurrentInterval->Begin()->Time().GetMillis();
    return PR_TRUE;

  case STATE_ACTIVE:
    {
      // Work out what comes next: the interval end or the next repeat iteration
      nsSMILTimeValue nextRepeat;
      if (mSeekState == SEEK_NOT_SEEKING && mSimpleDur.IsDefinite()) {
        nextRepeat.SetMillis(mCurrentInterval->Begin()->Time().GetMillis() +
            (mCurrentRepeatIteration + 1) * mSimpleDur.GetMillis());
      }
      nsSMILTimeValue nextMilestone =
        NS_MIN(mCurrentInterval->End()->Time(), nextRepeat);

      // Check for an early end before that time
      nsSMILInstanceTime* earlyEnd = CheckForEarlyEnd(nextMilestone);
      if (earlyEnd) {
        aNextMilestone.mIsEnd = PR_TRUE;
        aNextMilestone.mTime = earlyEnd->Time().GetMillis();
        return PR_TRUE;
      }

      // Apply the previously calculated milestone
      if (nextMilestone.IsDefinite()) {
        aNextMilestone.mIsEnd = nextMilestone != nextRepeat;
        aNextMilestone.mTime = nextMilestone.GetMillis();
        return PR_TRUE;
      }

      return PR_FALSE;
    }

  case STATE_POSTACTIVE:
    return PR_FALSE;

  default:
    NS_ABORT_IF_FALSE(PR_FALSE, "Invalid element state");
    return PR_FALSE;
  }
}

void
nsSMILTimedElement::NotifyNewInterval()
{
  NS_ABORT_IF_FALSE(mCurrentInterval,
      "Attempting to notify dependents of a new interval but the interval "
      "is not set");

  nsSMILTimeContainer* container = GetTimeContainer();
  if (container) {
    container->SyncPauseTime();
  }

  NotifyTimeDependentsParams params = { mCurrentInterval, container };
  mTimeDependents.EnumerateEntries(NotifyNewIntervalCallback, &params);
}

void
nsSMILTimedElement::NotifyChangedInterval(nsSMILInterval* aInterval,
                                          PRBool aBeginObjectChanged,
                                          PRBool aEndObjectChanged)
{
  NS_ABORT_IF_FALSE(aInterval, "Null interval for change notification");

  nsSMILTimeContainer* container = GetTimeContainer();
  if (container) {
    container->SyncPauseTime();
  }

  // Copy the instance times list since notifying the instance times can result
  // in a chain reaction whereby our own interval gets deleted along with its
  // instance times.
  InstanceTimeList times;
  aInterval->GetDependentTimes(times);

  for (PRUint32 i = 0; i < times.Length(); ++i) {
    times[i]->HandleChangedInterval(container, aBeginObjectChanged,
                                    aEndObjectChanged);
  }
}

void
nsSMILTimedElement::FireTimeEventAsync(PRUint32 aMsg, PRInt32 aDetail)
{
  if (!mAnimationElement)
    return;

  nsCOMPtr<nsIRunnable> event =
    new AsyncTimeEventRunner(&mAnimationElement->AsElement(), aMsg, aDetail);
  NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);
}

const nsSMILInstanceTime*
nsSMILTimedElement::GetEffectiveBeginInstance() const
{
  switch (mElementState)
  {
  case STATE_STARTUP:
    return nsnull;

  case STATE_ACTIVE:
    return mCurrentInterval->Begin();

  case STATE_WAITING:
  case STATE_POSTACTIVE:
    {
      const nsSMILInterval* prevInterval = GetPreviousInterval();
      return prevInterval ? prevInterval->Begin() : nsnull;
    }

  default:
    NS_NOTREACHED("Invalid element state");
    return nsnull;
  }
}

const nsSMILInterval*
nsSMILTimedElement::GetPreviousInterval() const
{
  return mOldIntervals.IsEmpty()
    ? nsnull
    : mOldIntervals[mOldIntervals.Length()-1].get();
}

PRBool
nsSMILTimedElement::HaveResolvedEndTimes() const
{
  if (mEndInstances.IsEmpty())
    return PR_FALSE;

  // mEndInstances is sorted so if the first time is not resolved then none of
  // them are
  return mEndInstances[0]->Time().IsDefinite();
}

PRBool
nsSMILTimedElement::EndHasEventConditions() const
{
  for (PRUint32 i = 0; i < mEndSpecs.Length(); ++i) {
    if (mEndSpecs[i]->IsEventBased())
      return PR_TRUE;
  }
  return PR_FALSE;
}

//----------------------------------------------------------------------
// Hashtable callback functions

/* static */ PR_CALLBACK PLDHashOperator
nsSMILTimedElement::NotifyNewIntervalCallback(TimeValueSpecPtrKey* aKey,
                                              void* aData)
{
  NS_ABORT_IF_FALSE(aKey, "Null hash key for time container hash table");
  NS_ABORT_IF_FALSE(aKey->GetKey(),
                    "null nsSMILTimeValueSpec in set of time dependents");

  NotifyTimeDependentsParams* params =
    static_cast<NotifyTimeDependentsParams*>(aData);
  NS_ABORT_IF_FALSE(params, "null data ptr while enumerating hashtable");
  NS_ABORT_IF_FALSE(params->mCurrentInterval, "null current-interval ptr");

  nsSMILTimeValueSpec* spec = aKey->GetKey();
  spec->HandleNewInterval(*params->mCurrentInterval, params->mTimeContainer);
  return PL_DHASH_NEXT;
}
