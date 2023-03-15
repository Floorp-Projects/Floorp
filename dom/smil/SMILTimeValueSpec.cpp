/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/EventListenerManager.h"
#include "mozilla/SMILInstanceTime.h"
#include "mozilla/SMILInterval.h"
#include "mozilla/SMILParserUtils.h"
#include "mozilla/SMILTimeContainer.h"
#include "mozilla/SMILTimedElement.h"
#include "mozilla/SMILTimeValueSpec.h"
#include "mozilla/SMILTimeValue.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/SVGAnimationElement.h"
#include "mozilla/dom/TimeEvent.h"
#include "nsString.h"
#include <limits>

using namespace mozilla::dom;

namespace mozilla {

//----------------------------------------------------------------------
// Nested class: EventListener

NS_IMPL_ISUPPORTS(SMILTimeValueSpec::EventListener, nsIDOMEventListener)

NS_IMETHODIMP
SMILTimeValueSpec::EventListener::HandleEvent(Event* aEvent) {
  if (mSpec) {
    mSpec->HandleEvent(aEvent);
  }
  return NS_OK;
}

//----------------------------------------------------------------------
// Implementation

SMILTimeValueSpec::SMILTimeValueSpec(SMILTimedElement& aOwner, bool aIsBegin)
    : mOwner(&aOwner), mIsBegin(aIsBegin), mReferencedElement(this) {}

SMILTimeValueSpec::~SMILTimeValueSpec() {
  UnregisterFromReferencedElement(mReferencedElement.get());
  if (mEventListener) {
    mEventListener->Disconnect();
    mEventListener = nullptr;
  }
}

nsresult SMILTimeValueSpec::SetSpec(const nsAString& aStringSpec,
                                    Element& aContextElement) {
  SMILTimeValueSpecParams params;

  if (!SMILParserUtils::ParseTimeValueSpecParams(aStringSpec, params))
    return NS_ERROR_FAILURE;

  mParams = params;

  // According to SMIL 3.0:
  //   The special value "indefinite" does not yield an instance time in the
  //   begin list. It will, however yield a single instance with the value
  //   "indefinite" in an end list. This value is not removed by a reset.
  if (mParams.mType == SMILTimeValueSpecParams::OFFSET ||
      (!mIsBegin && mParams.mType == SMILTimeValueSpecParams::INDEFINITE)) {
    mOwner->AddInstanceTime(new SMILInstanceTime(mParams.mOffset), mIsBegin);
  }

  // Fill in the event symbol to simplify handling later
  if (mParams.mType == SMILTimeValueSpecParams::REPEAT) {
    mParams.mEventSymbol = nsGkAtoms::repeatEvent;
  }

  ResolveReferences(aContextElement);

  return NS_OK;
}

void SMILTimeValueSpec::ResolveReferences(Element& aContextElement) {
  if (mParams.mType != SMILTimeValueSpecParams::SYNCBASE && !IsEventBased()) {
    return;
  }

  // If we're not bound to the document yet, don't worry, we'll get called again
  // when that happens
  if (!aContextElement.IsInComposedDoc()) return;

  // Hold ref to the old element so that it isn't destroyed in between resetting
  // the referenced element and using the pointer to update the referenced
  // element.
  RefPtr<Element> oldReferencedElement = mReferencedElement.get();

  if (mParams.mDependentElemID) {
    mReferencedElement.ResetWithID(aContextElement, mParams.mDependentElemID);
  } else if (mParams.mType == SMILTimeValueSpecParams::EVENT) {
    Element* target = mOwner->GetTargetElement();
    mReferencedElement.ResetWithElement(target);
  } else {
    MOZ_ASSERT(false, "Syncbase or repeat spec without ID");
  }
  UpdateReferencedElement(oldReferencedElement, mReferencedElement.get());
}

bool SMILTimeValueSpec::IsEventBased() const {
  return mParams.mType == SMILTimeValueSpecParams::EVENT ||
         mParams.mType == SMILTimeValueSpecParams::REPEAT;
}

void SMILTimeValueSpec::HandleNewInterval(
    SMILInterval& aInterval, const SMILTimeContainer* aSrcContainer) {
  const SMILInstanceTime& baseInstance =
      mParams.mSyncBegin ? *aInterval.Begin() : *aInterval.End();
  SMILTimeValue newTime =
      ConvertBetweenTimeContainers(baseInstance.Time(), aSrcContainer);

  // Apply offset
  if (!ApplyOffset(newTime)) {
    NS_WARNING("New time overflows SMILTime, ignoring");
    return;
  }

  // Create the instance time and register it with the interval
  RefPtr<SMILInstanceTime> newInstance = new SMILInstanceTime(
      newTime, SMILInstanceTime::SOURCE_SYNCBASE, this, &aInterval);
  mOwner->AddInstanceTime(newInstance, mIsBegin);
}

void SMILTimeValueSpec::HandleTargetElementChange(Element* aNewTarget) {
  if (!IsEventBased() || mParams.mDependentElemID) return;

  mReferencedElement.ResetWithElement(aNewTarget);
}

void SMILTimeValueSpec::HandleChangedInstanceTime(
    const SMILInstanceTime& aBaseTime, const SMILTimeContainer* aSrcContainer,
    SMILInstanceTime& aInstanceTimeToUpdate, bool aObjectChanged) {
  // If the instance time is fixed (e.g. because it's being used as the begin
  // time of an active or postactive interval) we just ignore the change.
  if (aInstanceTimeToUpdate.IsFixedTime()) return;

  SMILTimeValue updatedTime =
      ConvertBetweenTimeContainers(aBaseTime.Time(), aSrcContainer);

  // Apply offset
  if (!ApplyOffset(updatedTime)) {
    NS_WARNING("Updated time overflows SMILTime, ignoring");
    return;
  }

  // The timed element that owns the instance time does the updating so it can
  // re-sort its array of instance times more efficiently
  if (aInstanceTimeToUpdate.Time() != updatedTime || aObjectChanged) {
    mOwner->UpdateInstanceTime(&aInstanceTimeToUpdate, updatedTime, mIsBegin);
  }
}

void SMILTimeValueSpec::HandleDeletedInstanceTime(
    SMILInstanceTime& aInstanceTime) {
  mOwner->RemoveInstanceTime(&aInstanceTime, mIsBegin);
}

bool SMILTimeValueSpec::DependsOnBegin() const { return mParams.mSyncBegin; }

void SMILTimeValueSpec::Traverse(
    nsCycleCollectionTraversalCallback* aCallback) {
  mReferencedElement.Traverse(aCallback);
}

void SMILTimeValueSpec::Unlink() {
  UnregisterFromReferencedElement(mReferencedElement.get());
  mReferencedElement.Unlink();
}

//----------------------------------------------------------------------
// Implementation helpers

void SMILTimeValueSpec::UpdateReferencedElement(Element* aFrom, Element* aTo) {
  if (aFrom == aTo) return;

  UnregisterFromReferencedElement(aFrom);

  switch (mParams.mType) {
    case SMILTimeValueSpecParams::SYNCBASE: {
      SMILTimedElement* to = GetTimedElement(aTo);
      if (to) {
        to->AddDependent(*this);
      }
    } break;

    case SMILTimeValueSpecParams::EVENT:
    case SMILTimeValueSpecParams::REPEAT:
      RegisterEventListener(aTo);
      break;

    default:
      // not a referencing-type
      break;
  }
}

void SMILTimeValueSpec::UnregisterFromReferencedElement(Element* aElement) {
  if (!aElement) return;

  if (mParams.mType == SMILTimeValueSpecParams::SYNCBASE) {
    SMILTimedElement* timedElement = GetTimedElement(aElement);
    if (timedElement) {
      timedElement->RemoveDependent(*this);
    }
    mOwner->RemoveInstanceTimesForCreator(this, mIsBegin);
  } else if (IsEventBased()) {
    UnregisterEventListener(aElement);
  }
}

SMILTimedElement* SMILTimeValueSpec::GetTimedElement(Element* aElement) {
  auto* animationElement = SVGAnimationElement::FromNodeOrNull(aElement);
  return animationElement ? &animationElement->TimedElement() : nullptr;
}

// Indicates whether we're allowed to register an event-listener
// when scripting is disabled.
bool SMILTimeValueSpec::IsEventAllowedWhenScriptingIsDisabled() {
  // The category of (SMIL-specific) "repeat(n)" events are allowed.
  if (mParams.mType == SMILTimeValueSpecParams::REPEAT) {
    return true;
  }

  // A specific list of other SMIL-related events are allowed, too.
  if (mParams.mType == SMILTimeValueSpecParams::EVENT &&
      (mParams.mEventSymbol == nsGkAtoms::repeat ||
       mParams.mEventSymbol == nsGkAtoms::repeatEvent ||
       mParams.mEventSymbol == nsGkAtoms::beginEvent ||
       mParams.mEventSymbol == nsGkAtoms::endEvent)) {
    return true;
  }

  return false;
}

void SMILTimeValueSpec::RegisterEventListener(Element* aTarget) {
  MOZ_ASSERT(IsEventBased(),
             "Attempting to register event-listener for unexpected "
             "SMILTimeValueSpec type");
  MOZ_ASSERT(mParams.mEventSymbol,
             "Attempting to register event-listener but there is no event "
             "name");

  if (!aTarget) return;

  // When script is disabled, only allow registration for limited events.
  if (!aTarget->GetOwnerDocument()->IsScriptEnabled() &&
      !IsEventAllowedWhenScriptingIsDisabled()) {
    return;
  }

  if (!mEventListener) {
    mEventListener = new EventListener(this);
  }

  EventListenerManager* elm = aTarget->GetOrCreateListenerManager();
  if (!elm) {
    return;
  }

  elm->AddEventListenerByType(mEventListener,
                              nsDependentAtomString(mParams.mEventSymbol),
                              AllEventsAtSystemGroupBubble());
}

void SMILTimeValueSpec::UnregisterEventListener(Element* aTarget) {
  if (!aTarget || !mEventListener) {
    return;
  }

  EventListenerManager* elm = aTarget->GetOrCreateListenerManager();
  if (!elm) {
    return;
  }

  elm->RemoveEventListenerByType(mEventListener,
                                 nsDependentAtomString(mParams.mEventSymbol),
                                 AllEventsAtSystemGroupBubble());
}

void SMILTimeValueSpec::HandleEvent(Event* aEvent) {
  MOZ_ASSERT(mEventListener, "Got event without an event listener");
  MOZ_ASSERT(IsEventBased(), "Got event for non-event SMILTimeValueSpec");
  MOZ_ASSERT(aEvent, "No event supplied");

  // XXX In the long run we should get the time from the event itself which will
  // store the time in global document time which we'll need to convert to our
  // time container
  SMILTimeContainer* container = mOwner->GetTimeContainer();
  if (!container) return;

  if (mParams.mType == SMILTimeValueSpecParams::REPEAT &&
      !CheckRepeatEventDetail(aEvent)) {
    return;
  }

  SMILTime currentTime = container->GetCurrentTimeAsSMILTime();
  SMILTimeValue newTime(currentTime);
  if (!ApplyOffset(newTime)) {
    NS_WARNING("New time generated from event overflows SMILTime, ignoring");
    return;
  }

  RefPtr<SMILInstanceTime> newInstance =
      new SMILInstanceTime(newTime, SMILInstanceTime::SOURCE_EVENT);
  mOwner->AddInstanceTime(newInstance, mIsBegin);
}

bool SMILTimeValueSpec::CheckRepeatEventDetail(Event* aEvent) {
  TimeEvent* timeEvent = aEvent->AsTimeEvent();
  if (!timeEvent) {
    NS_WARNING("Received a repeat event that was not a DOMTimeEvent");
    return false;
  }

  int32_t detail = timeEvent->Detail();
  return detail > 0 && (uint32_t)detail == mParams.mRepeatIteration;
}

SMILTimeValue SMILTimeValueSpec::ConvertBetweenTimeContainers(
    const SMILTimeValue& aSrcTime, const SMILTimeContainer* aSrcContainer) {
  // If the source time is either indefinite or unresolved the result is going
  // to be the same
  if (!aSrcTime.IsDefinite()) return aSrcTime;

  // Convert from source time container to our parent time container
  const SMILTimeContainer* dstContainer = mOwner->GetTimeContainer();
  if (dstContainer == aSrcContainer) return aSrcTime;

  // If one of the elements is not attached to a time container then we can't do
  // any meaningful conversion
  if (!aSrcContainer || !dstContainer) return SMILTimeValue();  // unresolved

  SMILTimeValue docTime =
      aSrcContainer->ContainerToParentTime(aSrcTime.GetMillis());

  if (docTime.IsIndefinite())
    // This will happen if the source container is paused and we have a future
    // time. Just return the indefinite time.
    return docTime;

  MOZ_ASSERT(docTime.IsDefinite(),
             "ContainerToParentTime gave us an unresolved or indefinite time");

  return dstContainer->ParentToContainerTime(docTime.GetMillis());
}

bool SMILTimeValueSpec::ApplyOffset(SMILTimeValue& aTime) const {
  // indefinite + offset = indefinite. Likewise for unresolved times.
  if (!aTime.IsDefinite()) {
    return true;
  }

  double resultAsDouble =
      (double)aTime.GetMillis() + mParams.mOffset.GetMillis();
  if (resultAsDouble > double(std::numeric_limits<SMILTime>::max()) ||
      resultAsDouble < double(std::numeric_limits<SMILTime>::min())) {
    return false;
  }
  aTime.SetMillis(aTime.GetMillis() + mParams.mOffset.GetMillis());
  return true;
}

}  // namespace mozilla
