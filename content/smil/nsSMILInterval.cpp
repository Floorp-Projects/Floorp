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
 * Portions created by the Initial Developer are Copyright (C) 2009
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

#include "nsSMILInterval.h"

nsSMILInterval::nsSMILInterval()
:
  mBeginObjectChanged(PR_FALSE),
  mEndObjectChanged(PR_FALSE)
{
}

nsSMILInterval::nsSMILInterval(const nsSMILInterval& aOther)
:
  mBegin(aOther.mBegin),
  mEnd(aOther.mEnd),
  mBeginObjectChanged(PR_FALSE),
  mEndObjectChanged(PR_FALSE)
{
  NS_ABORT_IF_FALSE(aOther.mDependentTimes.IsEmpty(),
      "Attempting to copy-construct an interval with dependent times, "
      "this will lead to instance times being shared between intervals.");
}

nsSMILInterval::~nsSMILInterval()
{
  NS_ABORT_IF_FALSE(mDependentTimes.IsEmpty(),
      "Destroying interval without disassociating dependent instance times. "
      "NotifyDeleting was not called.");
}

void
nsSMILInterval::NotifyChanged(const nsSMILTimeContainer* aContainer)
{
  for (PRInt32 i = mDependentTimes.Length() - 1; i >= 0; --i) {
    mDependentTimes[i]->HandleChangedInterval(aContainer,
                                              mBeginObjectChanged,
                                              mEndObjectChanged);
  }
  mBeginObjectChanged = PR_FALSE;
  mEndObjectChanged = PR_FALSE;
}

void
nsSMILInterval::NotifyDeleting()
{
  for (PRInt32 i = mDependentTimes.Length() - 1; i >= 0; --i) {
    mDependentTimes[i]->HandleDeletedInterval();
  }
  mDependentTimes.Clear();
}

nsSMILInstanceTime*
nsSMILInterval::Begin()
{
  NS_ABORT_IF_FALSE(mBegin && mEnd,
      "Requesting Begin() on un-initialized instance time.");
  return mBegin;
}

nsSMILInstanceTime*
nsSMILInterval::End()
{
  NS_ABORT_IF_FALSE(mBegin && mEnd,
      "Requesting End() on un-initialized instance time.");
  return mEnd;
}

void
nsSMILInterval::SetBegin(nsSMILInstanceTime& aBegin)
{
  NS_ABORT_IF_FALSE(aBegin.Time().IsResolved(),
      "Attempting to set unresolved begin time on interval.");

  if (mBegin == &aBegin)
    return;

  mBegin = &aBegin;
  mBeginObjectChanged = PR_TRUE;
}

void
nsSMILInterval::SetEnd(nsSMILInstanceTime& aEnd)
{
  if (mEnd == &aEnd)
    return;

  mEnd = &aEnd;
  mEndObjectChanged = PR_TRUE;
}

void
nsSMILInterval::AddDependentTime(nsSMILInstanceTime& aTime)
{
  nsRefPtr<nsSMILInstanceTime>* inserted =
    mDependentTimes.InsertElementSorted(&aTime);
  if (!inserted) {
    NS_WARNING("Insufficient memory to insert instance time.");
  }
}

void
nsSMILInterval::RemoveDependentTime(const nsSMILInstanceTime& aTime)
{
#ifdef DEBUG
  PRBool found =
#endif
    mDependentTimes.RemoveElementSorted(&aTime);
  NS_ABORT_IF_FALSE(found, "Couldn't find instance time to delete.");
}
