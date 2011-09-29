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

#include "nsSMILInstanceTime.h"
#include "nsSMILInterval.h"
#include "nsSMILTimeValueSpec.h"
#include "mozilla/AutoRestore.h"

//----------------------------------------------------------------------
// Implementation

nsSMILInstanceTime::nsSMILInstanceTime(const nsSMILTimeValue& aTime,
                                       nsSMILInstanceTimeSource aSource,
                                       nsSMILTimeValueSpec* aCreator,
                                       nsSMILInterval* aBaseInterval)
  : mTime(aTime),
    mFlags(0),
    mVisited(PR_FALSE),
    mFixedEndpointRefCnt(0),
    mSerial(0),
    mCreator(aCreator),
    mBaseInterval(nsnull) // This will get set to aBaseInterval in a call to
                          // SetBaseInterval() at end of constructor
{
  switch (aSource) {
    case SOURCE_NONE:
      // No special flags
      break;

    case SOURCE_DOM:
      mFlags = kDynamic | kFromDOM;
      break;

    case SOURCE_SYNCBASE:
      mFlags = kMayUpdate;
      break;

    case SOURCE_EVENT:
      mFlags = kDynamic;
      break;
  }

  SetBaseInterval(aBaseInterval);
}

nsSMILInstanceTime::~nsSMILInstanceTime()
{
  NS_ABORT_IF_FALSE(!mBaseInterval,
      "Destroying instance time without first calling Unlink()");
  NS_ABORT_IF_FALSE(mFixedEndpointRefCnt == 0,
      "Destroying instance time that is still used as the fixed endpoint of an "
      "interval");
}

void
nsSMILInstanceTime::Unlink()
{
  nsRefPtr<nsSMILInstanceTime> deathGrip(this);
  if (mBaseInterval) {
    mBaseInterval->RemoveDependentTime(*this);
    mBaseInterval = nsnull;
  }
  mCreator = nsnull;
}

void
nsSMILInstanceTime::HandleChangedInterval(
    const nsSMILTimeContainer* aSrcContainer,
    bool aBeginObjectChanged,
    bool aEndObjectChanged)
{
  // It's possible a sequence of notifications might cause our base interval to
  // be updated and then deleted. Furthermore, the delete might happen whilst
  // we're still in the queue to be notified of the change. In any case, if we
  // don't have a base interval, just ignore the change.
  if (!mBaseInterval)
    return;

  NS_ABORT_IF_FALSE(mCreator, "Base interval is set but creator is not.");

  if (mVisited) {
    // Break the cycle here
    Unlink();
    return;
  }

  bool objectChanged = mCreator->DependsOnBegin() ? aBeginObjectChanged :
                                                      aEndObjectChanged;

  mozilla::AutoRestore<bool> setVisited(mVisited);
  mVisited = PR_TRUE;

  nsRefPtr<nsSMILInstanceTime> deathGrip(this);
  mCreator->HandleChangedInstanceTime(*GetBaseTime(), aSrcContainer, *this,
                                      objectChanged);
}

void
nsSMILInstanceTime::HandleDeletedInterval()
{
  NS_ABORT_IF_FALSE(mBaseInterval,
      "Got call to HandleDeletedInterval on an independent instance time");
  NS_ABORT_IF_FALSE(mCreator, "Base interval is set but creator is not");

  mBaseInterval = nsnull;
  mFlags &= ~kMayUpdate; // Can't update without a base interval

  nsRefPtr<nsSMILInstanceTime> deathGrip(this);
  mCreator->HandleDeletedInstanceTime(*this);
  mCreator = nsnull;
}

void
nsSMILInstanceTime::HandleFilteredInterval()
{
  NS_ABORT_IF_FALSE(mBaseInterval,
      "Got call to HandleFilteredInterval on an independent instance time");

  mBaseInterval = nsnull;
  mFlags &= ~kMayUpdate; // Can't update without a base interval
  mCreator = nsnull;
}

bool
nsSMILInstanceTime::ShouldPreserve() const
{
  return mFixedEndpointRefCnt > 0 || (mFlags & kWasDynamicEndpoint);
}

void
nsSMILInstanceTime::UnmarkShouldPreserve()
{
  mFlags &= ~kWasDynamicEndpoint;
}

void
nsSMILInstanceTime::AddRefFixedEndpoint()
{
  NS_ABORT_IF_FALSE(mFixedEndpointRefCnt < PR_UINT16_MAX,
      "Fixed endpoint reference count upper limit reached");
  ++mFixedEndpointRefCnt;
  mFlags &= ~kMayUpdate; // Once fixed, always fixed
}

void
nsSMILInstanceTime::ReleaseFixedEndpoint()
{
  NS_ABORT_IF_FALSE(mFixedEndpointRefCnt > 0, "Duplicate release");
  --mFixedEndpointRefCnt;
  if (mFixedEndpointRefCnt == 0 && IsDynamic()) {
    mFlags |= kWasDynamicEndpoint;
  }
}

bool
nsSMILInstanceTime::IsDependentOn(const nsSMILInstanceTime& aOther) const
{
  if (mVisited)
    return PR_FALSE;

  const nsSMILInstanceTime* myBaseTime = GetBaseTime();
  if (!myBaseTime)
    return PR_FALSE;

  if (myBaseTime == &aOther)
    return PR_TRUE;

  // mVisited is mutable
  mozilla::AutoRestore<bool> setVisited(const_cast<nsSMILInstanceTime*>(this)->mVisited);
  const_cast<nsSMILInstanceTime*>(this)->mVisited = PR_TRUE;
  return myBaseTime->IsDependentOn(aOther);
}

const nsSMILInstanceTime*
nsSMILInstanceTime::GetBaseTime() const
{
  if (!mBaseInterval) {
    return nsnull;
  }

  NS_ABORT_IF_FALSE(mCreator, "Base interval is set but there is no creator.");
  if (!mCreator) {
    return nsnull;
  }

  return mCreator->DependsOnBegin() ? mBaseInterval->Begin() :
                                      mBaseInterval->End();
}

void
nsSMILInstanceTime::SetBaseInterval(nsSMILInterval* aBaseInterval)
{
  NS_ABORT_IF_FALSE(!mBaseInterval,
      "Attempting to reassociate an instance time with a different interval.");

  if (aBaseInterval) {
    NS_ABORT_IF_FALSE(mCreator,
        "Attempting to create a dependent instance time without reference "
        "to the creating nsSMILTimeValueSpec object.");
    if (!mCreator)
      return;

    aBaseInterval->AddDependentTime(*this);
  }

  mBaseInterval = aBaseInterval;
}
