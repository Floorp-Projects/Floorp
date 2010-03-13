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

nsSMILInstanceTime::nsSMILInstanceTime(const nsSMILTimeValue& aTime,
                                       nsSMILInstanceTimeSource aSource,
                                       nsSMILTimeValueSpec* aCreator,
                                       nsSMILInterval* aBaseInterval)
  : mTime(aTime),
    mFlags(0),
    mSerial(0),
    mVisited(PR_FALSE),
    mChainEnd(PR_FALSE),
    mCreator(aCreator),
    mBaseInterval(nsnull) // This will get set to aBaseInterval in a call to
                          // SetBaseInterval() at end of constructor
{
  switch (aSource) {
    case SOURCE_NONE:
      // No special flags
      break;

    case SOURCE_DOM:
      mFlags = kClearOnReset | kFromDOM;
      break;

    case SOURCE_SYNCBASE:
      mFlags = kMayUpdate;
      break;

    case SOURCE_EVENT:
      mFlags = kClearOnReset;
      break;
  }

  SetBaseInterval(aBaseInterval);
}

nsSMILInstanceTime::~nsSMILInstanceTime()
{
  NS_ABORT_IF_FALSE(!mBaseInterval && !mCreator,
      "Destroying instance time without first calling Unlink()");
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
    PRBool aBeginObjectChanged,
    PRBool aEndObjectChanged)
{
  NS_ABORT_IF_FALSE(mBaseInterval,
      "Got call to HandleChangedInterval on an independent instance time.");
  NS_ABORT_IF_FALSE(mCreator, "Base interval is set but creator is not.");

  if (mVisited || mChainEnd) {
    // We're breaking the cycle here but we need to ensure that if we later
    // receive a change notice in a different context (e.g. due to a time
    // container change) that we don't end up following the chain further and so
    // we set a flag to that effect.
    mChainEnd = PR_TRUE;
    return;
  }

  PRBool objectChanged = mCreator->DependsOnBegin() ? aBeginObjectChanged :
                                                      aEndObjectChanged;

  AutoBoolSetter setVisited(mVisited);

  nsRefPtr<nsSMILInstanceTime> deathGrip(this);
  mCreator->HandleChangedInstanceTime(*GetBaseTime(), aSrcContainer, *this,
                                      objectChanged);
}

void
nsSMILInstanceTime::HandleDeletedInterval()
{
  NS_ABORT_IF_FALSE(mBaseInterval,
      "Got call to HandleDeletedInterval on an independent instance time.");
  NS_ABORT_IF_FALSE(mCreator, "Base interval is set but creator is not.");

  mBaseInterval = nsnull;

  nsRefPtr<nsSMILInstanceTime> deathGrip(this);
  mCreator->HandleDeletedInstanceTime(*this);
  mCreator = nsnull;
}

PRBool
nsSMILInstanceTime::IsDependent(const nsSMILInstanceTime& aOther,
                                PRUint32 aRecursionDepth) const
{
  NS_ABORT_IF_FALSE(aRecursionDepth < 1000,
      "We seem to have created a cycle between instance times");

  const nsSMILInstanceTime* myBaseTime = GetBaseTime();
  if (!myBaseTime)
    return PR_FALSE;

  if (myBaseTime == &aOther)
    return PR_TRUE;

  return myBaseTime->IsDependent(aOther, ++aRecursionDepth);
}

void
nsSMILInstanceTime::SetBaseInterval(nsSMILInterval* aBaseInterval)
{
  NS_ABORT_IF_FALSE(!mBaseInterval,
      "Attepting to reassociate an instance time with a different interval.");

  // Make sure we don't end up creating a cycle between the dependent time
  // pointers.
  if (aBaseInterval) {
    NS_ABORT_IF_FALSE(mCreator,
        "Attempting to create a dependent instance time without reference "
        "to the creating nsSMILTimeValueSpec object.");
    if (!mCreator)
      return;

    const nsSMILInstanceTime* dependentTime = mCreator->DependsOnBegin() ?
                                              aBaseInterval->Begin() :
                                              aBaseInterval->End();
    dependentTime->BreakPotentialCycle(this);
    aBaseInterval->AddDependentTime(*this);
  }

  mBaseInterval = aBaseInterval;
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
nsSMILInstanceTime::BreakPotentialCycle(
    const nsSMILInstanceTime* aNewTail) const
{
  const nsSMILInstanceTime* myBaseTime = GetBaseTime();
  if (!myBaseTime)
    return;

  if (myBaseTime == aNewTail) {
    // Making aNewTail the new tail of the chain would create a cycle so we
    // prevent this by unlinking the pointer to aNewTail.
    mBaseInterval->RemoveDependentTime(*this);
    return;
  }

  myBaseTime->BreakPotentialCycle(aNewTail);
}
