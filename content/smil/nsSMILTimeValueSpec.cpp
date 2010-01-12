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
// Helper classes

namespace
{
  // Utility class to set a PRPackedBool value to PR_TRUE whilst it is in scope.
  // Saves us having to remember to clear the flag at every possible return.
  class AutoBoolSetter
  {
  public:
    AutoBoolSetter(PRPackedBool& aValue)
    : mValue(aValue)
    {
      mValue = PR_TRUE;
    }

    ~AutoBoolSetter()
    {
      mValue = PR_FALSE;
    }

  private:
    PRPackedBool&   mValue;
  };
}

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
    mVisited(PR_FALSE),
    mChainEnd(PR_FALSE),
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

  // Currently we don't allow nsSMILTimeValueSpec objects to be re-used. When
  // the 'begin' or 'end' attribute on an nsSMILTimedElement is set,
  // nsSMILTimedElement will just throw away all the old spec objects and create
  // new ones.
  NS_ABORT_IF_FALSE(!mLatestInstanceTime,
      "Attempting to re-use nsSMILTimeValueSpec object. "
      "Last instance time is non-null");

  mParams = params;

  // According to SMIL 3.0:
  //   The special value "indefinite" does not yield an instance time in the
  //   begin list. It will, however yield a single instance with the value
  //   "indefinite" in an end list. This value is not removed by a reset.
  if (mParams.mType == nsSMILTimeValueSpecParams::OFFSET ||
      (!mIsBegin && mParams.mType == nsSMILTimeValueSpecParams::INDEFINITE)) {
    nsRefPtr<nsSMILInstanceTime> instance =
      new nsSMILInstanceTime(mParams.mOffset, nsnull);
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
nsSMILTimeValueSpec::HandleNewInterval(const nsSMILInterval& aInterval,
                                       const nsSMILTimeContainer* aSrcContainer)
{
  NS_ABORT_IF_FALSE(aInterval.IsSet(),
      "Received notification of new interval that is not set");

  const nsSMILInstanceTime& baseInstance = mParams.mSyncBegin
    ? *aInterval.Begin() : *aInterval.End();
  nsSMILTimeValue newTime =
    ConvertBetweenTimeContainers(baseInstance.Time(), aSrcContainer);

  // If we're a begin spec but the time we've been given is not resolved we
  // won't make an instance time. If the time becomes resolved later we'll
  // create the instance time when we get the change notice.
  if (mIsBegin && !newTime.IsResolved())
    return;

  // Apply offset
  if (newTime.IsResolved()) {
    newTime.SetMillis(newTime.GetMillis() + mParams.mOffset.GetMillis());
  }

  nsRefPtr<nsSMILInstanceTime> newInstance =
    new nsSMILInstanceTime(newTime, &baseInstance,
                           nsSMILInstanceTime::SOURCE_SYNCBASE);
  if (!newInstance)
    return;

  if (mLatestInstanceTime) {
    mLatestInstanceTime->MarkNoLongerUpdating();
  }

  mLatestInstanceTime = newInstance;
  mChainEnd = PR_FALSE;
  mOwner->AddInstanceTime(newInstance, mIsBegin);
}

void
nsSMILTimeValueSpec::HandleChangedInterval(const nsSMILInterval& aInterval,
   const nsSMILTimeContainer* aSrcContainer)
{
  NS_ABORT_IF_FALSE(aInterval.IsSet(),
      "Received notification of changed interval that is not set");

  if (mVisited || mChainEnd) {
    // We're breaking the cycle here but we need to ensure that if we later
    // receive a change notice in a different context (e.g. due to a time
    // container change) that we don't end up following the chain further and so
    // we set a flag to that effect.
    mChainEnd = PR_TRUE;
    return;
  }

  AutoBoolSetter setVisited(mVisited);

  // If there's no latest interval to update it must mean that we decided not to
  // make one when the got the new interval notification (because we're a begin
  // spec and the time wasn't resolved) or we deleted it because the source time
  // container was paused. So now we just act like this was a new interval
  // notification.
  if (!mLatestInstanceTime) {
    HandleNewInterval(aInterval, aSrcContainer);
    return;
  }

  const nsSMILInstanceTime& baseInstance = mParams.mSyncBegin
    ? *aInterval.Begin() : *aInterval.End();
  NS_ABORT_IF_FALSE(mLatestInstanceTime != &baseInstance,
      "Instance time is dependent on itself");

  nsSMILTimeValue updatedTime =
    ConvertBetweenTimeContainers(baseInstance.Time(), aSrcContainer);

  // If we're a begin spec but the time is now unresolved, delete the interval.
  if (mIsBegin && !updatedTime.IsResolved()) {
    HandleDeletedInterval();
    return;
  }

  // Apply offset
  if (updatedTime.IsResolved()) {
    updatedTime.SetMillis(updatedTime.GetMillis() +
                          mParams.mOffset.GetMillis());
  }

  // Note that if the instance time is fixed (e.g. because it's being used as
  // the begin time of an active interval) we just ignore the change.
  // See SMIL 3 section 5.4.5:
  //
  //   "In contrast, when an instance time in the begin list changes because the
  //   syncbase (current interval) time moves, this does not invoke restart
  //   semantics, but may change the current begin time: If the current interval
  //   has not yet begun, a change to an instance time in the begin list will
  //   cause a re-evaluation of the begin instance lists, which may cause the
  //   interval begin time to change."
  //
  if (!mLatestInstanceTime->MayUpdate())
    return;

  // The timed element that owns the instance time does the updating so it can
  // re-sort its array of instance times more efficiently
  if (mLatestInstanceTime->Time() != updatedTime ||
      mLatestInstanceTime->GetDependentTime() != &baseInstance) {
    mOwner->UpdateInstanceTime(mLatestInstanceTime, updatedTime,
                               &baseInstance, mIsBegin);
  }
}

void
nsSMILTimeValueSpec::HandleDeletedInterval()
{
  // If we don't have an instance time it must mean we decided not to create one
  // when we got a new interval notice (because we're a begin spec and the time
  // was unresolved).
  if (!mLatestInstanceTime)
    return;

  // Since we don't know if calling RemoveInstanceTime will result in further
  // calls to us, we ensure that we're in a consistent state before handing over
  // control.
  nsRefPtr<nsSMILInstanceTime> oldInstanceTime = mLatestInstanceTime;
  mLatestInstanceTime = nsnull;
  mChainEnd = PR_FALSE;

  mOwner->RemoveInstanceTime(oldInstanceTime, mIsBegin);
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
  HandleDeletedInterval();
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
