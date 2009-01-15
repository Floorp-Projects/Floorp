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
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brian Birtles <birtles@gmail.com>
 *   Daniel Holbert <dholbert@mozilla.com>
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

#include "nsSMILAnimationController.h"
#include "nsSMILCompositor.h"
#include "nsComponentManagerUtils.h"
#include "nsITimer.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsISMILAnimationElement.h"
#include "nsIDOMSVGAnimationElement.h"
#include "nsSMILTimedElement.h"

//----------------------------------------------------------------------
// nsSMILAnimationController implementation

// In my testing the minimum needed for smooth animation is 36 frames per
// second which seems like a lot (Flash traditionally uses 14fps).
//
// Redrawing is synchronous. This is deliberate so that later we can tune the
// timer based on how long the callback takes. To achieve 36fps we'd need 28ms
// between frames. For now we set the timer interval to be a little less than
// this (to allow for the render itself) and then let performance decay as the
// image gets more complicated and render times increase.
//
const PRUint32 nsSMILAnimationController::kTimerInterval = 22;

//----------------------------------------------------------------------
// ctors, dtors, factory methods

nsSMILAnimationController::nsSMILAnimationController()
  : mDocument(nsnull)
{
  mAnimationElementTable.Init();
  mChildContainerTable.Init();
}

nsSMILAnimationController::~nsSMILAnimationController()
{
  if (mTimer) {
    mTimer->Cancel();
    mTimer = nsnull;
  }

  if (mForceSampleEvent) {
    mForceSampleEvent->Expire();
    mForceSampleEvent = nsnull;
  }

  NS_ASSERTION(mAnimationElementTable.Count() == 0,
               "Animation controller shouldn't be tracking any animation"
               " elements when it dies.");
}

nsSMILAnimationController* NS_NewSMILAnimationController(nsIDocument* aDoc)
{
  nsSMILAnimationController* animationController = 
    new nsSMILAnimationController();
  NS_ENSURE_TRUE(animationController, nsnull);

  nsresult rv = animationController->Init(aDoc);
  if (NS_FAILED(rv)) {
    delete animationController;
    animationController = nsnull;
  }

  return animationController;
}

nsresult
nsSMILAnimationController::Init(nsIDocument* aDoc)
{
  NS_ENSURE_ARG_POINTER(aDoc);

  mTimer = do_CreateInstance("@mozilla.org/timer;1");
  NS_ENSURE_TRUE(mTimer, NS_ERROR_OUT_OF_MEMORY);

  // Keep track of document, so we can traverse its set of animation elements
  mDocument = aDoc;

  Begin();

  return NS_OK;
}

//----------------------------------------------------------------------
// nsSMILTimeContainer methods:

void
nsSMILAnimationController::Pause(PRUint32 aType)
{
  nsSMILTimeContainer::Pause(aType);

  if (mPauseState) {
    StopTimer();
  }
}

void
nsSMILAnimationController::Resume(PRUint32 aType)
{
  PRBool wasPaused = (mPauseState != 0);

  nsSMILTimeContainer::Resume(aType);

  if (wasPaused && !mPauseState) {
    StartTimer();
  }
}

nsSMILTime
nsSMILAnimationController::GetParentTime() const
{
  // Our parent time is wallclock time
  return PR_Now() / PR_USEC_PER_MSEC;
}

//----------------------------------------------------------------------
// Animation element registration methods:

void
nsSMILAnimationController::RegisterAnimationElement(
                                  nsISMILAnimationElement* aAnimationElement)
{
  mAnimationElementTable.PutEntry(aAnimationElement);
}

void
nsSMILAnimationController::UnregisterAnimationElement(
                                  nsISMILAnimationElement* aAnimationElement)
{
  mAnimationElementTable.RemoveEntry(aAnimationElement);
}

//----------------------------------------------------------------------
// nsSMILAnimationController methods:

nsresult
nsSMILAnimationController::OnForceSample()
{
  // Make sure this was a queued call
  NS_ENSURE_TRUE(mForceSampleEvent, NS_ERROR_FAILURE);

  nsresult rv = NS_OK;
  if (!mPauseState) {
    // Stop timer-controlled samples first, to avoid race conditions.
    rv = StopTimer();
    if (NS_SUCCEEDED(rv)) {
      // StartTimer does a synchronous sample before it starts the timer.
      // This is the sample that we're "forcing" here.
      rv = StartTimer();
    }
  }
  mForceSampleEvent = nsnull;
  return rv;
}

void
nsSMILAnimationController::FireForceSampleEvent()
{
  if (!mForceSampleEvent) {
    mForceSampleEvent = new ForceSampleEvent(*this);
    if (NS_FAILED(NS_DispatchToCurrentThread(mForceSampleEvent))) {
      NS_WARNING("Failed to dispatch force sample event");
      mForceSampleEvent = nsnull;
    }
  }
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
  mLastCompositorTable = nsnull;
}

//----------------------------------------------------------------------
// Timer-related implementation helpers

/*static*/ void
nsSMILAnimationController::Notify(nsITimer* timer, void* aClosure)
{
  nsSMILAnimationController* controller = (nsSMILAnimationController*)aClosure;

  NS_ASSERTION(controller->mTimer == timer,
               "nsSMILAnimationController::Notify called with incorrect timer");

  controller->Sample();
}

nsresult
nsSMILAnimationController::StartTimer()
{
  NS_ENSURE_TRUE(mTimer, NS_ERROR_FAILURE);
  NS_ASSERTION(mPauseState == 0, "Starting timer but controller is paused.");

  // Run the first sample manually
  Sample();

  // 
  // XXX Make this self-tuning. Sounds like control theory to me and not
  // something I'm familiar with.
  //
  return mTimer->InitWithFuncCallback(nsSMILAnimationController::Notify,
                                      this,
                                      kTimerInterval,
                                      nsITimer::TYPE_REPEATING_SLACK);
}

nsresult
nsSMILAnimationController::StopTimer()
{
  NS_ENSURE_TRUE(mTimer, NS_ERROR_FAILURE);

  return mTimer->Cancel();
}

//----------------------------------------------------------------------
// Sample-related methods and callbacks

void
nsSMILAnimationController::DoSample()
{
  // STEP 1: Sample the child time containers
  //
  // When we sample the child time containers they will simply record the sample
  // time in document time.
  TimeContainerHashtable activeContainers;
  activeContainers.Init(mChildContainerTable.Count());
  mChildContainerTable.EnumerateEntries(SampleTimeContainers,
                                        &activeContainers);

  // STEP 2: (i)  Sample the timed elements AND
  //         (ii) Create a table of compositors
  // 
  // (i) Here we sample the timed elements (fetched from the
  // nsISMILAnimationElements) which determine from the active time if the
  // element is active and what it's simple time etc. is. This information is
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
  if (!currentCompositorTable)
    return;
  currentCompositorTable->Init(0);

  SampleAnimationParams params = { &activeContainers, currentCompositorTable };
  nsresult rv = mAnimationElementTable.EnumerateEntries(SampleAnimation,
                                                        &params);
  if (NS_FAILED(rv)) {
    NS_WARNING("SampleAnimationParams failed");
  }
  activeContainers.Clear();

  // STEP 3: Remove animation effects from any no-longer-animated elems/attrs
  if (mLastCompositorTable) {
    // XXX Remove animation effects from no-longer-animated elements
    //  * For each compositor in current sample's hash table:
    //    - Remove entry from *prev sample's* hash table
    //  * For any entries still remaining in prev sample's hash table:
    //    - Remove animation from that entry's attribute.
    //      (For nsSVGLength2, set anim val = base val.  For CSS attribs,
    //      just clear the relevant chunk of OverrideStyle)
  }

  // STEP 4: Compose currently-animated attributes.
  nsSMILCompositor::ComposeAttributes(*currentCompositorTable);

  // Update last compositor table
  mLastCompositorTable = currentCompositorTable.forget();
}

/*static*/ PR_CALLBACK PLDHashOperator
nsSMILAnimationController::SampleTimeContainers(TimeContainerPtrKey* aKey,
                                                void* aData)
{ 
  NS_ENSURE_TRUE(aKey, PL_DHASH_NEXT);
  NS_ENSURE_TRUE(aKey->GetKey(), PL_DHASH_NEXT);
  NS_ENSURE_TRUE(aData, PL_DHASH_NEXT);

  TimeContainerHashtable* activeContainers
    = static_cast<TimeContainerHashtable*>(aData);

  nsSMILTimeContainer* container = aKey->GetKey();
  if (container->NeedsSample()) {
    container->Sample();
    activeContainers->PutEntry(container);
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
  SampleAnimationParams* params = static_cast<SampleAnimationParams*>(aData);

  SampleTimedElement(animElem, params->mActiveContainers);
  AddAnimationToCompositorTable(animElem, params->mCompositorTable);

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
  // step (SampleTimeContainers) and then test here if the container for this
  // timed element is in the list.
  if (!aActiveContainers->GetEntry(timeContainer))
    return;

  nsSMILTime containerTime = timeContainer->GetCurrentTime();

  aElement->TimedElement().SampleAt(containerTime);
}

/*static*/ void
nsSMILAnimationController::AddAnimationToCompositorTable(
  nsISMILAnimationElement* aElement, nsSMILCompositorTable* aCompositorTable)
{
  // Add a compositor to the hash table if there's not already one there
  nsSMILCompositorKey key;
  if (!GetCompositorKeyForAnimation(aElement, key))
    // Something's wrong/missing about animation's target; skip this animation
    return;

  nsSMILCompositor* result = aCompositorTable->PutEntry(key);
  
  // Add this animationElement's animation function to the compositor's list of
  // animation functions.
  result->AddAnimationFunction(&aElement->AnimationFunction());
}

// Helper function that, given a nsISMILAnimationElement, looks up its target
// element & target attribute and returns a newly-constructed nsSMILCompositor
// for this target.
/*static*/ PRBool
nsSMILAnimationController::GetCompositorKeyForAnimation(
    nsISMILAnimationElement* aAnimElem, nsSMILCompositorKey& aResult)
{
  // Look up target (animated) element
  nsIContent* targetElem = aAnimElem->GetTargetElementContent();
  if (!targetElem)
    // Animation has no target elem -- skip it.
    return PR_FALSE;

  // Look up target (animated) attribute
  //
  // XXXdholbert As mentioned in SMILANIM section 3.1, attributeName may
  // have an XMLNS prefix to indicate the XML namespace. Need to parse
  // that somewhere.
  nsIAtom* attributeName = aAnimElem->GetTargetAttributeName();
  if (!attributeName)
    // Animation has no target attr -- skip it.
    return PR_FALSE;

  // Look up target (animated) attribute-type
  nsSMILTargetAttrType attributeType = aAnimElem->GetTargetAttributeType();

  // Check if an 'auto' attributeType refers to a CSS property or XML attribute.
  // Note that SMIL requires we search for CSS properties first. So if they
  // overlap, 'auto' = 'CSS'. (SMILANIM 3.1)
  //
  // XXX This doesn't really work for CSS properties that aren't mapped
  // attributes
  if (attributeType == eSMILTargetAttrType_auto) {
    attributeType = (targetElem->IsAttributeMapped(attributeName))
                  ? eSMILTargetAttrType_CSS
                  : eSMILTargetAttrType_XML;
  }
  PRBool isCSS = (attributeType == eSMILTargetAttrType_CSS);

  // Construct the key
  aResult.mElement = targetElem;
  aResult.mAttributeName = attributeName;
  aResult.mIsCSS = isCSS;

  return PR_TRUE;
}

//----------------------------------------------------------------------
// Add/remove child time containers

nsresult
nsSMILAnimationController::AddChild(nsSMILTimeContainer& aChild)
{
  TimeContainerPtrKey* key = mChildContainerTable.PutEntry(&aChild);
  NS_ENSURE_TRUE(key,NS_ERROR_OUT_OF_MEMORY);

  if (!mPauseState && mChildContainerTable.Count() == 1) {
    StartTimer();
  }
    
  return NS_OK;
}

void
nsSMILAnimationController::RemoveChild(nsSMILTimeContainer& aChild)
{
  mChildContainerTable.RemoveEntry(&aChild);

  if (!mPauseState && mChildContainerTable.Count() == 0) {
    StopTimer();
  }
}
