/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsSMILAnimationController.h"
#include "nsSMILCompositor.h"
#include "nsSMILCSSProperty.h"
#include "nsCSSProps.h"
#include "nsITimer.h"
#include "mozilla/dom/Element.h"
#include "nsIDocument.h"
#include "nsISMILAnimationElement.h"
#include "nsIDOMSVGAnimationElement.h"
#include "nsSMILTimedElement.h"

using namespace mozilla::dom;

//----------------------------------------------------------------------
// nsSMILAnimationController implementation

//----------------------------------------------------------------------
// ctors, dtors, factory methods

nsSMILAnimationController::nsSMILAnimationController(nsIDocument* aDoc)
  : mAvgTimeBetweenSamples(0),
    mResampleNeeded(false),
    mDeferredStartSampling(false),
    mRunningSample(false),
    mDocument(aDoc)
{
  NS_ABORT_IF_FALSE(aDoc, "need a non-null document");

  mAnimationElementTable.Init();
  mChildContainerTable.Init();

  nsRefreshDriver* refreshDriver = GetRefreshDriver();
  if (refreshDriver) {
    mStartTime = refreshDriver->MostRecentRefresh();
  } else {
    mStartTime = mozilla::TimeStamp::Now();
  }
  mCurrentSampleTime = mStartTime;

  Begin();
}

nsSMILAnimationController::~nsSMILAnimationController()
{
  NS_ASSERTION(mAnimationElementTable.Count() == 0,
               "Animation controller shouldn't be tracking any animation"
               " elements when it dies");
}

void
nsSMILAnimationController::Disconnect()
{
  NS_ABORT_IF_FALSE(mDocument, "disconnecting when we weren't connected...?");
  NS_ABORT_IF_FALSE(mRefCnt.get() == 1,
                    "Expecting to disconnect when doc is sole remaining owner");
  NS_ASSERTION(mPauseState & nsSMILTimeContainer::PAUSE_PAGEHIDE,
               "Expecting to be paused for pagehide before disconnect");

  StopSampling(GetRefreshDriver());

  mDocument = nullptr; // (raw pointer)
}

//----------------------------------------------------------------------
// nsSMILTimeContainer methods:

void
nsSMILAnimationController::Pause(PRUint32 aType)
{
  nsSMILTimeContainer::Pause(aType);

  if (mPauseState) {
    mDeferredStartSampling = false;
    StopSampling(GetRefreshDriver());
  }
}

void
nsSMILAnimationController::Resume(PRUint32 aType)
{
  bool wasPaused = (mPauseState != 0);
  // Update mCurrentSampleTime so that calls to GetParentTime--used for
  // calculating parent offsets--are accurate
  mCurrentSampleTime = mozilla::TimeStamp::Now();

  nsSMILTimeContainer::Resume(aType);

  if (wasPaused && !mPauseState && mChildContainerTable.Count()) {
    Sample(); // Run the first sample manually
    MaybeStartSampling(GetRefreshDriver());
  }
}

nsSMILTime
nsSMILAnimationController::GetParentTime() const
{
  return (nsSMILTime)(mCurrentSampleTime - mStartTime).ToMilliseconds();
}

//----------------------------------------------------------------------
// nsARefreshObserver methods:
NS_IMPL_ADDREF(nsSMILAnimationController)
NS_IMPL_RELEASE(nsSMILAnimationController)

// nsRefreshDriver Callback function
void
nsSMILAnimationController::WillRefresh(mozilla::TimeStamp aTime)
{
  // Although we never expect aTime to go backwards, when we initialise the
  // animation controller, if we can't get hold of a refresh driver we
  // initialise mCurrentSampleTime to Now(). It may be possible that after
  // doing so we get sampled by a refresh driver whose most recent refresh time
  // predates when we were initialised, so to be safe we make sure to take the
  // most recent time here.
  aTime = NS_MAX(mCurrentSampleTime, aTime);

  // Sleep detection: If the time between samples is a whole lot greater than we
  // were expecting then we assume the computer went to sleep or someone's
  // messing with the clock. In that case, fiddle our parent offset and use our
  // average time between samples to calculate the new sample time. This
  // prevents us from hanging while trying to catch up on all the missed time.

  // Smoothing of coefficient for the average function. 0.2 should let us track
  // the sample rate reasonably tightly without being overly affected by
  // occasional delays.
  static const double SAMPLE_DUR_WEIGHTING = 0.2;
  // If the elapsed time exceeds our expectation by this number of times we'll
  // initiate special behaviour to basically ignore the intervening time.
  static const double SAMPLE_DEV_THRESHOLD = 200.0;

  nsSMILTime elapsedTime =
    (nsSMILTime)(aTime - mCurrentSampleTime).ToMilliseconds();
  // First sample:
  if (mAvgTimeBetweenSamples == 0) {
    mAvgTimeBetweenSamples = elapsedTime;
  // Unexpectedly long delay between samples:
  } else if (elapsedTime > SAMPLE_DEV_THRESHOLD * mAvgTimeBetweenSamples) {
    NS_WARNING("Detected really long delay between samples, continuing from "
               "previous sample");
    mParentOffset += elapsedTime - mAvgTimeBetweenSamples;
  // Usual case, update moving average:
  } else {
    // Due to truncation here the average will normally be a little less than
    // it should be but that's probably ok
    mAvgTimeBetweenSamples =
      (nsSMILTime)(elapsedTime * SAMPLE_DUR_WEIGHTING +
      mAvgTimeBetweenSamples * (1.0 - SAMPLE_DUR_WEIGHTING));
  }
  mCurrentSampleTime = aTime;

  Sample();
}

//----------------------------------------------------------------------
// Animation element registration methods:

void
nsSMILAnimationController::RegisterAnimationElement(
                                  nsISMILAnimationElement* aAnimationElement)
{
  mAnimationElementTable.PutEntry(aAnimationElement);
  if (mDeferredStartSampling) {
    mDeferredStartSampling = false;
    if (mChildContainerTable.Count()) {
      // mAnimationElementTable was empty, but now we've added its 1st element
      NS_ABORT_IF_FALSE(mAnimationElementTable.Count() == 1,
                        "we shouldn't have deferred sampling if we already had "
                        "animations registered");
      StartSampling(GetRefreshDriver());
      Sample(); // Run the first sample manually
    } // else, don't sample until a time container is registered (via AddChild)
  }
}

void
nsSMILAnimationController::UnregisterAnimationElement(
                                  nsISMILAnimationElement* aAnimationElement)
{
  mAnimationElementTable.RemoveEntry(aAnimationElement);
}

//----------------------------------------------------------------------
// Page show/hide

void
nsSMILAnimationController::OnPageShow()
{
  Resume(nsSMILTimeContainer::PAUSE_PAGEHIDE);
}

void
nsSMILAnimationController::OnPageHide()
{
  Pause(nsSMILTimeContainer::PAUSE_PAGEHIDE);
}

//----------------------------------------------------------------------
// Cycle-collection support

void
nsSMILAnimationController::Traverse(
    nsCycleCollectionTraversalCallback* aCallback)
{
  // Traverse last compositor table
  if (mLastCompositorTable) {
    mLastCompositorTable->EnumerateEntries(CompositorTableEntryTraverse,
                                           aCallback);
  }
}

/*static*/ PR_CALLBACK PLDHashOperator
nsSMILAnimationController::CompositorTableEntryTraverse(
                                      nsSMILCompositor* aCompositor,
                                      void* aArg)
{
  nsCycleCollectionTraversalCallback* cb =
    static_cast<nsCycleCollectionTraversalCallback*>(aArg);
  aCompositor->Traverse(cb);
  return PL_DHASH_NEXT;
}

void
nsSMILAnimationController::Unlink()
{
  mLastCompositorTable = nullptr;
}

//----------------------------------------------------------------------
// Refresh driver lifecycle related methods

void
nsSMILAnimationController::NotifyRefreshDriverCreated(
    nsRefreshDriver* aRefreshDriver)
{
  if (!mPauseState) {
    MaybeStartSampling(aRefreshDriver);
  }
}

void
nsSMILAnimationController::NotifyRefreshDriverDestroying(
    nsRefreshDriver* aRefreshDriver)
{
  if (!mPauseState && !mDeferredStartSampling) {
    StopSampling(aRefreshDriver);
  }
}

//----------------------------------------------------------------------
// Timer-related implementation helpers

void
nsSMILAnimationController::StartSampling(nsRefreshDriver* aRefreshDriver)
{
  NS_ASSERTION(mPauseState == 0, "Starting sampling but controller is paused");
  NS_ASSERTION(!mDeferredStartSampling,
               "Started sampling but the deferred start flag is still set");
  if (aRefreshDriver) {
    NS_ABORT_IF_FALSE(!GetRefreshDriver() ||
                      aRefreshDriver == GetRefreshDriver(),
                      "Starting sampling with wrong refresh driver");
    // We're effectively resuming from a pause so update our current sample time
    // or else it will confuse our "average time between samples" calculations.
    mCurrentSampleTime = mozilla::TimeStamp::Now();
    aRefreshDriver->AddRefreshObserver(this, Flush_Style);
  }
}

void
nsSMILAnimationController::StopSampling(nsRefreshDriver* aRefreshDriver)
{
  if (aRefreshDriver) {
    // NOTE: The document might already have been detached from its PresContext
    // (and RefreshDriver), which would make GetRefreshDriverForDoc return null.
    NS_ABORT_IF_FALSE(!GetRefreshDriver() ||
                      aRefreshDriver == GetRefreshDriver(),
                      "Stopping sampling with wrong refresh driver");
    aRefreshDriver->RemoveRefreshObserver(this, Flush_Style);
  }
}

void
nsSMILAnimationController::MaybeStartSampling(nsRefreshDriver* aRefreshDriver)
{
  if (mDeferredStartSampling) {
    // We've received earlier 'MaybeStartSampling' calls, and we're
    // deferring until we get a registered animation.
    return;
  }

  if (mAnimationElementTable.Count()) {
    StartSampling(aRefreshDriver);
  } else {
    mDeferredStartSampling = true;
  }
}

//----------------------------------------------------------------------
// Sample-related methods and callbacks

PR_CALLBACK PLDHashOperator
TransferCachedBaseValue(nsSMILCompositor* aCompositor,
                        void* aData)
{
  nsSMILCompositorTable* lastCompositorTable =
    static_cast<nsSMILCompositorTable*>(aData);
  nsSMILCompositor* lastCompositor =
    lastCompositorTable->GetEntry(aCompositor->GetKey());

  if (lastCompositor) {
    aCompositor->StealCachedBaseValue(lastCompositor);
  }

  return PL_DHASH_NEXT;  
}

PR_CALLBACK PLDHashOperator
RemoveCompositorFromTable(nsSMILCompositor* aCompositor,
                          void* aData)
{
  nsSMILCompositorTable* lastCompositorTable =
    static_cast<nsSMILCompositorTable*>(aData);
  lastCompositorTable->RemoveEntry(aCompositor->GetKey());
  return PL_DHASH_NEXT;
}

PR_CALLBACK PLDHashOperator
DoClearAnimationEffects(nsSMILCompositor* aCompositor,
                        void* /*aData*/)
{
  aCompositor->ClearAnimationEffects();
  return PL_DHASH_NEXT;
}

PR_CALLBACK PLDHashOperator
DoComposeAttribute(nsSMILCompositor* aCompositor,
                   void* /*aData*/)
{
  aCompositor->ComposeAttribute();
  return PL_DHASH_NEXT;
}

void
nsSMILAnimationController::DoSample()
{
  DoSample(true); // Skip unchanged time containers
}

void
nsSMILAnimationController::DoSample(bool aSkipUnchangedContainers)
{
  if (!mDocument) {
    NS_ERROR("Shouldn't be sampling after document has disconnected");
    return;
  }

  mResampleNeeded = false;
  // Set running sample flag -- do this before flushing styles so that when we
  // flush styles we don't end up requesting extra samples
  mRunningSample = true;
  nsCOMPtr<nsIDocument> kungFuDeathGrip(mDocument);  // keeps 'this' alive too
  mDocument->FlushPendingNotifications(Flush_Style);

  // WARNING: 
  // WARNING: the above flush may have destroyed the pres shell and/or
  // WARNING: frames and other layout related objects.
  // WARNING:
  
  // STEP 1: Bring model up to date
  // (i)  Rewind elements where necessary
  // (ii) Run milestone samples
  RewindElements();
  DoMilestoneSamples();

  // STEP 2: Sample the child time containers
  //
  // When we sample the child time containers they will simply record the sample
  // time in document time.
  TimeContainerHashtable activeContainers;
  activeContainers.Init(mChildContainerTable.Count());
  SampleTimeContainerParams tcParams = { &activeContainers,
                                         aSkipUnchangedContainers };
  mChildContainerTable.EnumerateEntries(SampleTimeContainer, &tcParams);

  // STEP 3: (i)  Sample the timed elements AND
  //         (ii) Create a table of compositors
  //
  // (i) Here we sample the timed elements (fetched from the
  // nsISMILAnimationElements) which determine from the active time if the
  // element is active and what its simple time etc. is. This information is
  // then passed to its time client (nsSMILAnimationFunction).
  //
  // (ii) During the same loop we also build up a table that contains one
  // compositor for each animated attribute and which maps animated elements to
  // the corresponding compositor for their target attribute.
  //
  // Note that this compositor table needs to be allocated on the heap so we can
  // store it until the next sample. This lets us find out which elements were
  // animated in sample 'n-1' but not in sample 'n' (and hence need to have
  // their animation effects removed in sample 'n').
  //
  // Parts (i) and (ii) are not functionally related but we combine them here to
  // save iterating over the animation elements twice.

  // Create the compositor table
  nsAutoPtr<nsSMILCompositorTable>
    currentCompositorTable(new nsSMILCompositorTable());
  currentCompositorTable->Init(0);

  SampleAnimationParams saParams = { &activeContainers,
                                     currentCompositorTable };
  mAnimationElementTable.EnumerateEntries(SampleAnimation,
                                          &saParams);
  activeContainers.Clear();

  // STEP 4: Compare previous sample's compositors against this sample's.
  // (Transfer cached base values across, & remove animation effects from 
  // no-longer-animated targets.)
  if (mLastCompositorTable) {
    // * Transfer over cached base values, from last sample's compositors
    currentCompositorTable->EnumerateEntries(TransferCachedBaseValue,
                                             mLastCompositorTable);

    // * For each compositor in current sample's hash table, remove entry from
    // prev sample's hash table -- we don't need to clear animation
    // effects of those compositors, since they're still being animated.
    currentCompositorTable->EnumerateEntries(RemoveCompositorFromTable,
                                             mLastCompositorTable);

    // * For each entry that remains in prev sample's hash table (i.e. for
    // every target that's no longer animated), clear animation effects.
    mLastCompositorTable->EnumerateEntries(DoClearAnimationEffects, nullptr);
  }

  // STEP 5: Compose currently-animated attributes.
  // XXXdholbert: This step traverses our animation targets in an effectively
  // random order. For animation from/to 'inherit' values to work correctly
  // when the inherited value is *also* being animated, we really should be
  // traversing our animated nodes in an ancestors-first order (bug 501183)
  currentCompositorTable->EnumerateEntries(DoComposeAttribute, nullptr);
  mRunningSample = false;

  // Update last compositor table
  mLastCompositorTable = currentCompositorTable.forget();

  NS_ASSERTION(!mResampleNeeded, "Resample dirty flag set during sample!");
}

void
nsSMILAnimationController::RewindElements()
{
  bool rewindNeeded = false;
  mChildContainerTable.EnumerateEntries(RewindNeeded, &rewindNeeded);
  if (!rewindNeeded)
    return;

  mAnimationElementTable.EnumerateEntries(RewindAnimation, nullptr);
  mChildContainerTable.EnumerateEntries(ClearRewindNeeded, nullptr);
}

/*static*/ PR_CALLBACK PLDHashOperator
nsSMILAnimationController::RewindNeeded(TimeContainerPtrKey* aKey,
                                        void* aData)
{
  NS_ABORT_IF_FALSE(aData,
      "Null data pointer during time container enumeration");
  bool* rewindNeeded = static_cast<bool*>(aData);

  nsSMILTimeContainer* container = aKey->GetKey();
  if (container->NeedsRewind()) {
    *rewindNeeded = true;
    return PL_DHASH_STOP;
  }

  return PL_DHASH_NEXT;
}

/*static*/ PR_CALLBACK PLDHashOperator
nsSMILAnimationController::RewindAnimation(AnimationElementPtrKey* aKey,
                                           void* aData)
{
  nsISMILAnimationElement* animElem = aKey->GetKey();
  nsSMILTimeContainer* timeContainer = animElem->GetTimeContainer();
  if (timeContainer && timeContainer->NeedsRewind()) {
    animElem->TimedElement().Rewind();
  }

  return PL_DHASH_NEXT;
}

/*static*/ PR_CALLBACK PLDHashOperator
nsSMILAnimationController::ClearRewindNeeded(TimeContainerPtrKey* aKey,
                                             void* aData)
{
  aKey->GetKey()->ClearNeedsRewind();
  return PL_DHASH_NEXT;
}

void
nsSMILAnimationController::DoMilestoneSamples()
{
  // We need to sample the timing model but because SMIL operates independently
  // of the frame-rate, we can get one sample at t=0s and the next at t=10min.
  //
  // In between those two sample times a whole string of significant events
  // might be expected to take place: events firing, new interdependencies
  // between animations resolved and dissolved, etc.
  //
  // Furthermore, at any given time, we want to sample all the intervals that
  // end at that time BEFORE any that begin. This behaviour is implied by SMIL's
  // endpoint-exclusive timing model.
  //
  // So we have the animations (specifically the timed elements) register the
  // next significant moment (called a milestone) in their lifetime and then we
  // step through the model at each of these moments and sample those animations
  // registered for those times. This way events can fire in the correct order,
  // dependencies can be resolved etc.

  nsSMILTime sampleTime = LL_MININT;

  while (true) {
    // We want to find any milestones AT OR BEFORE the current sample time so we
    // initialise the next milestone to the moment after (1ms after, to be
    // precise) the current sample time and see if there are any milestones
    // before that. Any other milestones will be dealt with in a subsequent
    // sample.
    nsSMILMilestone nextMilestone(GetCurrentTime() + 1, true);
    mChildContainerTable.EnumerateEntries(GetNextMilestone, &nextMilestone);

    if (nextMilestone.mTime > GetCurrentTime()) {
      break;
    }

    GetMilestoneElementsParams params;
    params.mMilestone = nextMilestone;
    mChildContainerTable.EnumerateEntries(GetMilestoneElements, &params);
    PRUint32 length = params.mElements.Length();

    // During the course of a sampling we don't want to actually go backwards.
    // Due to negative offsets, early ends and the like, a timed element might
    // register a milestone that is actually in the past. That's fine, but it's
    // still only going to get *sampled* with whatever time we're up to and no
    // earlier.
    //
    // Because we're only performing this clamping at the last moment, the
    // animations will still all get sampled in the correct order and
    // dependencies will be appropriately resolved.
    sampleTime = NS_MAX(nextMilestone.mTime, sampleTime);

    for (PRUint32 i = 0; i < length; ++i) {
      nsISMILAnimationElement* elem = params.mElements[i].get();
      NS_ABORT_IF_FALSE(elem, "NULL animation element in list");
      nsSMILTimeContainer* container = elem->GetTimeContainer();
      if (!container)
        // The container may be nullptr if the element has been detached from its
        // parent since registering a milestone.
        continue;

      nsSMILTimeValue containerTimeValue =
        container->ParentToContainerTime(sampleTime);
      if (!containerTimeValue.IsDefinite())
        continue;

      // Clamp the converted container time to non-negative values.
      nsSMILTime containerTime = NS_MAX<nsSMILTime>(0, containerTimeValue.GetMillis());

      if (nextMilestone.mIsEnd) {
        elem->TimedElement().SampleEndAt(containerTime);
      } else {
        elem->TimedElement().SampleAt(containerTime);
      }
    }
  }
}

/*static*/ PR_CALLBACK PLDHashOperator
nsSMILAnimationController::GetNextMilestone(TimeContainerPtrKey* aKey,
                                            void* aData)
{
  NS_ABORT_IF_FALSE(aKey, "Null hash key for time container hash table");
  NS_ABORT_IF_FALSE(aKey->GetKey(), "Null time container key in hash table");
  NS_ABORT_IF_FALSE(aData,
      "Null data pointer during time container enumeration");

  nsSMILMilestone* nextMilestone = static_cast<nsSMILMilestone*>(aData);

  nsSMILTimeContainer* container = aKey->GetKey();
  if (container->IsPausedByType(nsSMILTimeContainer::PAUSE_BEGIN))
    return PL_DHASH_NEXT;

  nsSMILMilestone thisMilestone;
  bool didGetMilestone =
    container->GetNextMilestoneInParentTime(thisMilestone);
  if (didGetMilestone && thisMilestone < *nextMilestone) {
    *nextMilestone = thisMilestone;
  }

  return PL_DHASH_NEXT;
}

/*static*/ PR_CALLBACK PLDHashOperator
nsSMILAnimationController::GetMilestoneElements(TimeContainerPtrKey* aKey,
                                                void* aData)
{
  NS_ABORT_IF_FALSE(aKey, "Null hash key for time container hash table");
  NS_ABORT_IF_FALSE(aKey->GetKey(), "Null time container key in hash table");
  NS_ABORT_IF_FALSE(aData,
      "Null data pointer during time container enumeration");

  GetMilestoneElementsParams* params =
    static_cast<GetMilestoneElementsParams*>(aData);

  nsSMILTimeContainer* container = aKey->GetKey();
  if (container->IsPausedByType(nsSMILTimeContainer::PAUSE_BEGIN))
    return PL_DHASH_NEXT;

  container->PopMilestoneElementsAtMilestone(params->mMilestone,
                                             params->mElements);

  return PL_DHASH_NEXT;
}

/*static*/ PR_CALLBACK PLDHashOperator
nsSMILAnimationController::SampleTimeContainer(TimeContainerPtrKey* aKey,
                                               void* aData)
{
  NS_ENSURE_TRUE(aKey, PL_DHASH_NEXT);
  NS_ENSURE_TRUE(aKey->GetKey(), PL_DHASH_NEXT);
  NS_ENSURE_TRUE(aData, PL_DHASH_NEXT);

  SampleTimeContainerParams* params =
    static_cast<SampleTimeContainerParams*>(aData);

  nsSMILTimeContainer* container = aKey->GetKey();
  if (!container->IsPausedByType(nsSMILTimeContainer::PAUSE_BEGIN) &&
      (container->NeedsSample() || !params->mSkipUnchangedContainers)) {
    container->ClearMilestones();
    container->Sample();
    container->MarkSeekFinished();
    params->mActiveContainers->PutEntry(container);
  }

  return PL_DHASH_NEXT;
}

/*static*/ PR_CALLBACK PLDHashOperator
nsSMILAnimationController::SampleAnimation(AnimationElementPtrKey* aKey,
                                           void* aData)
{
  NS_ENSURE_TRUE(aKey, PL_DHASH_NEXT);
  NS_ENSURE_TRUE(aKey->GetKey(), PL_DHASH_NEXT);
  NS_ENSURE_TRUE(aData, PL_DHASH_NEXT);

  nsISMILAnimationElement* animElem = aKey->GetKey();
  if (animElem->PassesConditionalProcessingTests()) {
    SampleAnimationParams* params = static_cast<SampleAnimationParams*>(aData);

    SampleTimedElement(animElem, params->mActiveContainers);
    AddAnimationToCompositorTable(animElem, params->mCompositorTable);
  }

  return PL_DHASH_NEXT;
}

/*static*/ void
nsSMILAnimationController::SampleTimedElement(
  nsISMILAnimationElement* aElement, TimeContainerHashtable* aActiveContainers)
{
  nsSMILTimeContainer* timeContainer = aElement->GetTimeContainer();
  if (!timeContainer)
    return;

  // We'd like to call timeContainer->NeedsSample() here and skip all timed
  // elements that belong to paused time containers that don't need a sample,
  // but that doesn't work because we've already called Sample() on all the time
  // containers so the paused ones don't need a sample any more and they'll
  // return false.
  //
  // Instead we build up a hashmap of active time containers during the previous
  // step (SampleTimeContainer) and then test here if the container for this
  // timed element is in the list.
  if (!aActiveContainers->GetEntry(timeContainer))
    return;

  nsSMILTime containerTime = timeContainer->GetCurrentTime();

  NS_ABORT_IF_FALSE(!timeContainer->IsSeeking(),
      "Doing a regular sample but the time container is still seeking");
  aElement->TimedElement().SampleAt(containerTime);
}

/*static*/ void
nsSMILAnimationController::AddAnimationToCompositorTable(
  nsISMILAnimationElement* aElement, nsSMILCompositorTable* aCompositorTable)
{
  // Add a compositor to the hash table if there's not already one there
  nsSMILTargetIdentifier key;
  if (!GetTargetIdentifierForAnimation(aElement, key))
    // Something's wrong/missing about animation's target; skip this animation
    return;

  nsSMILAnimationFunction& func = aElement->AnimationFunction();

  // Only add active animation functions. If there are no active animations
  // targeting an attribute, no compositor will be created and any previously
  // applied animations will be cleared.
  if (func.IsActiveOrFrozen()) {
    // Look up the compositor for our target, & add our animation function
    // to its list of animation functions.
    nsSMILCompositor* result = aCompositorTable->PutEntry(key);
    result->AddAnimationFunction(&func);

  } else if (func.HasChanged()) {
    // Look up the compositor for our target, and force it to skip the
    // "nothing's changed so don't bother compositing" optimization for this
    // sample. |func| is inactive, but it's probably *newly* inactive (since
    // it's got HasChanged() == true), so we need to make sure to recompose
    // its target.
    nsSMILCompositor* result = aCompositorTable->PutEntry(key);
    result->ToggleForceCompositing();

    // We've now made sure that |func|'s inactivity will be reflected as of
    // this sample. We need to clear its HasChanged() flag so that it won't
    // trigger this same clause in future samples (until it changes again).
    func.ClearHasChanged();
  }
}

// Helper function that, given a nsISMILAnimationElement, looks up its target
// element & target attribute and populates a nsSMILTargetIdentifier
// for this target.
/*static*/ bool
nsSMILAnimationController::GetTargetIdentifierForAnimation(
    nsISMILAnimationElement* aAnimElem, nsSMILTargetIdentifier& aResult)
{
  // Look up target (animated) element
  Element* targetElem = aAnimElem->GetTargetElementContent();
  if (!targetElem)
    // Animation has no target elem -- skip it.
    return false;

  // Look up target (animated) attribute
  // SMILANIM section 3.1, attributeName may
  // have an XMLNS prefix to indicate the XML namespace.
  nsCOMPtr<nsIAtom> attributeName;
  PRInt32 attributeNamespaceID;
  if (!aAnimElem->GetTargetAttributeName(&attributeNamespaceID,
                                         getter_AddRefs(attributeName)))
    // Animation has no target attr -- skip it.
    return false;

  // Look up target (animated) attribute-type
  nsSMILTargetAttrType attributeType = aAnimElem->GetTargetAttributeType();

  // Check if an 'auto' attributeType refers to a CSS property or XML attribute.
  // Note that SMIL requires we search for CSS properties first. So if they
  // overlap, 'auto' = 'CSS'. (SMILANIM 3.1)
  bool isCSS = false;
  if (attributeType == eSMILTargetAttrType_auto) {
    if (attributeNamespaceID == kNameSpaceID_None) {
      // width/height are special as they may be attributes or for
      // outer-<svg> elements, mapped into style.
      if (attributeName == nsGkAtoms::width ||
          attributeName == nsGkAtoms::height) {
        isCSS = targetElem->GetNameSpaceID() != kNameSpaceID_SVG;
      } else {
        nsCSSProperty prop =
          nsCSSProps::LookupProperty(nsDependentAtomString(attributeName),
                                     nsCSSProps::eEnabled);
        isCSS = nsSMILCSSProperty::IsPropertyAnimatable(prop);
      }
    }
  } else {
    isCSS = (attributeType == eSMILTargetAttrType_CSS);
  }

  // Construct the key
  aResult.mElement = targetElem;
  aResult.mAttributeName = attributeName;
  aResult.mAttributeNamespaceID = attributeNamespaceID;
  aResult.mIsCSS = isCSS;

  return true;
}

//----------------------------------------------------------------------
// Add/remove child time containers

nsresult
nsSMILAnimationController::AddChild(nsSMILTimeContainer& aChild)
{
  TimeContainerPtrKey* key = mChildContainerTable.PutEntry(&aChild);
  NS_ENSURE_TRUE(key, NS_ERROR_OUT_OF_MEMORY);

  if (!mPauseState && mChildContainerTable.Count() == 1) {
    MaybeStartSampling(GetRefreshDriver());
    Sample(); // Run the first sample manually
  }

  return NS_OK;
}

void
nsSMILAnimationController::RemoveChild(nsSMILTimeContainer& aChild)
{
  mChildContainerTable.RemoveEntry(&aChild);

  if (!mPauseState && mChildContainerTable.Count() == 0) {
    StopSampling(GetRefreshDriver());
  }
}

// Helper method
nsRefreshDriver*
nsSMILAnimationController::GetRefreshDriver()
{
  if (!mDocument) {
    NS_ERROR("Requesting refresh driver after document has disconnected!");
    return nullptr;
  }

  nsIPresShell* shell = mDocument->GetShell();
  if (!shell) {
    return nullptr;
  }

  nsPresContext* context = shell->GetPresContext();
  return context ? context->RefreshDriver() : nullptr;
}

void
nsSMILAnimationController::FlagDocumentNeedsFlush()
{
  mDocument->SetNeedStyleFlush();
}
