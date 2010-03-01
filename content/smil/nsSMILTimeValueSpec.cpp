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

#include "nsSMILTimeValueSpec.h"
#include "nsSMILInterval.h"
#include "nsSMILTimeContainer.h"
#include "nsSMILTimeValue.h"
#include "nsSMILTimedElement.h"
#include "nsSMILInstanceTime.h"
#include "nsSMILParserUtils.h"
#include "nsISMILAnimationElement.h"
#include "nsContentUtils.h"
#include "nsString.h"

//----------------------------------------------------------------------
// Implementation

#ifdef _MSC_VER
// Disable "warning C4355: 'this' : used in base member initializer list".
// We can ignore that warning because we know that mTimebase's constructor
// doesn't dereference the pointer passed to it.
#pragma warning(push)
#pragma warning(disable:4355)
#endif
nsSMILTimeValueSpec::nsSMILTimeValueSpec(nsSMILTimedElement& aOwner,
                                         PRBool aIsBegin)
  : mOwner(&aOwner),
    mIsBegin(aIsBegin),
    mTimebase(this)
#ifdef _MSC_VER
#pragma warning(pop)
#endif
{
}

nsSMILTimeValueSpec::~nsSMILTimeValueSpec()
{
  UnregisterFromTimebase(GetTimebaseElement());
}

nsresult
nsSMILTimeValueSpec::SetSpec(const nsAString& aStringSpec,
                             nsIContent* aContextNode)
{
  nsSMILTimeValueSpecParams params;
  nsresult rv =
    nsSMILParserUtils::ParseTimeValueSpecParams(aStringSpec, params);

  if (NS_FAILED(rv))
    return rv;

  mParams = params;

  // According to SMIL 3.0:
  //   The special value "indefinite" does not yield an instance time in the
  //   begin list. It will, however yield a single instance with the value
  //   "indefinite" in an end list. This value is not removed by a reset.
  if (mParams.mType == nsSMILTimeValueSpecParams::OFFSET ||
      (!mIsBegin && mParams.mType == nsSMILTimeValueSpecParams::INDEFINITE)) {
    nsRefPtr<nsSMILInstanceTime> instance =
      new nsSMILInstanceTime(mParams.mOffset);
    if (!instance)
      return NS_ERROR_OUT_OF_MEMORY;
    mOwner->AddInstanceTime(instance, mIsBegin);
  }

  ResolveReferences(aContextNode);

  return rv;
}

void
nsSMILTimeValueSpec::ResolveReferences(nsIContent* aContextNode)
{
  if (mParams.mType != nsSMILTimeValueSpecParams::SYNCBASE)
    return;

  NS_ABORT_IF_FALSE(aContextNode,
      "null context node for resolving timing references against");

  // If we're not bound to the document yet, don't worry, we'll get called again
  // when that happens
  if (!aContextNode->IsInDoc())
    return;

  // Hold ref to the old content so that it isn't destroyed in between resetting
  // the timebase and using the pointer to update the timebase.
  nsRefPtr<nsIContent> oldTimebaseContent = mTimebase.get();

  NS_ABORT_IF_FALSE(mParams.mDependentElemID, "NULL syncbase element id");
  nsString idStr;
  mParams.mDependentElemID->ToString(idStr);
  mTimebase.ResetWithID(aContextNode, idStr);
  UpdateTimebase(oldTimebaseContent, mTimebase.get());
}

void
nsSMILTimeValueSpec::HandleNewInterval(nsSMILInterval& aInterval,
                                       const nsSMILTimeContainer* aSrcContainer)
{
  const nsSMILInstanceTime& baseInstance = mParams.mSyncBegin
    ? *aInterval.Begin() : *aInterval.End();
  nsSMILTimeValue newTime =
    ConvertBetweenTimeContainers(baseInstance.Time(), aSrcContainer);

  // Apply offset
  if (newTime.IsResolved()) {
    newTime.SetMillis(newTime.GetMillis() + mParams.mOffset.GetMillis());
  }

  // Create the instance time and register it with the interval
  nsRefPtr<nsSMILInstanceTime> newInstance =
    new nsSMILInstanceTime(newTime, nsSMILInstanceTime::SOURCE_SYNCBASE, this,
                           &aInterval);
  if (!newInstance)
    return;

  // If we are a begin spec but the time we've got is not resolved, we won't add
  // it to the owner just yet. When the time later becomes resolved we'll add it
  // at that point.
  if (mIsBegin && !newTime.IsResolved())
    return;

  mOwner->AddInstanceTime(newInstance, mIsBegin);
}

void
nsSMILTimeValueSpec::HandleChangedInstanceTime(
    const nsSMILInstanceTime& aBaseTime,
    const nsSMILTimeContainer* aSrcContainer,
    nsSMILInstanceTime& aInstanceTimeToUpdate,
    PRBool aObjectChanged)
{
  // If the instance time is fixed (e.g. because it's being used as the begin
  // time of an active interval) we just ignore the change.
  if (!aInstanceTimeToUpdate.MayUpdate())
    return;

  nsSMILTimeValue updatedTime =
    ConvertBetweenTimeContainers(aBaseTime.Time(), aSrcContainer);

  // Apply offset
  if (updatedTime.IsResolved()) {
    updatedTime.SetMillis(updatedTime.GetMillis() +
                          mParams.mOffset.GetMillis());
  }

  // Since we never add unresolved begin times to the owner we must detect if
  // this change requires adding a newly-resolved time, removing
  // a previously-resolved time, or doing nothing
  if (mIsBegin) {
    // Add newly-resolved time
    if (!aInstanceTimeToUpdate.Time().IsResolved() &&
        updatedTime.IsResolved()) {
      aInstanceTimeToUpdate.DependentUpdate(updatedTime);
      mOwner->AddInstanceTime(&aInstanceTimeToUpdate, mIsBegin);
      return;
    }
    // Remove previously-resolved time
    if (aInstanceTimeToUpdate.Time().IsResolved() &&
        !updatedTime.IsResolved()) {
      aInstanceTimeToUpdate.DependentUpdate(updatedTime);
      mOwner->RemoveInstanceTime(&aInstanceTimeToUpdate, mIsBegin);
      return;
    }
    // Do nothing (but update in case we're updating an 'unresolved' time to an
    // 'indefinite' time or vice versa, both of which return PR_FALSE for
    // IsResolved() and neither of which should be added to the owner).
    if (!aInstanceTimeToUpdate.Time().IsResolved() &&
        !updatedTime.IsResolved()) {
      aInstanceTimeToUpdate.DependentUpdate(updatedTime);
      return;
    }
  }

  // The timed element that owns the instance time does the updating so it can
  // re-sort its array of instance times more efficiently
  if (aInstanceTimeToUpdate.Time() != updatedTime || aObjectChanged) {
    mOwner->UpdateInstanceTime(&aInstanceTimeToUpdate, updatedTime, mIsBegin);
  }
}

void
nsSMILTimeValueSpec::HandleDeletedInstanceTime(
    nsSMILInstanceTime &aInstanceTime)
{
  // If it's an unresolved begin time then we won't have added it
  if (mIsBegin && !aInstanceTime.Time().IsResolved())
    return;

  mOwner->RemoveInstanceTime(&aInstanceTime, mIsBegin);
}

PRBool
nsSMILTimeValueSpec::DependsOnBegin() const
{
  return mParams.mSyncBegin;
}

void
nsSMILTimeValueSpec::Traverse(nsCycleCollectionTraversalCallback* aCallback)
{
  mTimebase.Traverse(aCallback);
}

void
nsSMILTimeValueSpec::Unlink()
{
  UnregisterFromTimebase(GetTimebaseElement());
  mTimebase.Unlink();
}

//----------------------------------------------------------------------
// Implementation helpers

void
nsSMILTimeValueSpec::UpdateTimebase(nsIContent* aFrom, nsIContent* aTo)
{
  if (aFrom == aTo)
    return;

  UnregisterFromTimebase(GetTimedElementFromContent(aFrom));

  nsSMILTimedElement* to = GetTimedElementFromContent(aTo);
  if (to) {
    to->AddDependent(*this);
  }
}

void
nsSMILTimeValueSpec::UnregisterFromTimebase(nsSMILTimedElement* aTimedElement)
{
  if (!aTimedElement)
    return;

  aTimedElement->RemoveDependent(*this);
  mOwner->RemoveInstanceTimesForCreator(this, mIsBegin);
}

nsSMILTimedElement*
nsSMILTimeValueSpec::GetTimedElementFromContent(nsIContent* aContent)
{
  if (!aContent)
    return nsnull;

  nsCOMPtr<nsISMILAnimationElement> animElement = do_QueryInterface(aContent);
  if (!animElement)
    return nsnull;

  return &animElement->TimedElement();
}

nsSMILTimedElement*
nsSMILTimeValueSpec::GetTimebaseElement()
{
  return GetTimedElementFromContent(mTimebase.get());
}

nsSMILTimeValue
nsSMILTimeValueSpec::ConvertBetweenTimeContainers(
    const nsSMILTimeValue& aSrcTime,
    const nsSMILTimeContainer* aSrcContainer)
{
  // If the source time is either indefinite or unresolved the result is going
  // to be the same
  if (!aSrcTime.IsResolved())
    return aSrcTime;

  // Convert from source time container to our parent time container
  const nsSMILTimeContainer* dstContainer = mOwner->GetTimeContainer();
  if (dstContainer == aSrcContainer)
    return aSrcTime;

  // If one of the elements is not attached to a time container then we can't do
  // any meaningful conversion
  if (!aSrcContainer || !dstContainer)
    return nsSMILTimeValue(); // unresolved

  nsSMILTimeValue docTime =
    aSrcContainer->ContainerToParentTime(aSrcTime.GetMillis());

  if (docTime.IsIndefinite())
    // This will happen if the source container is paused and we have a future
    // time. Just return the indefinite time.
    return docTime;

   NS_ABORT_IF_FALSE(docTime.IsResolved(),
       "ContainerToParentTime gave us an unresolved time");

  return dstContainer->ParentToContainerTime(docTime.GetMillis());
}
