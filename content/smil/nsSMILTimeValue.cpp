/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsSMILTimeValue.h"

nsSMILTime nsSMILTimeValue::kUnresolvedMillis = LL_MAXINT;

//----------------------------------------------------------------------
// nsSMILTimeValue methods:

static inline PRInt8
Cmp(PRInt64 aA, PRInt64 aB)
{
  return aA == aB ? 0 : (aA > aB ? 1 : -1);
}

PRInt8
nsSMILTimeValue::CompareTo(const nsSMILTimeValue& aOther) const
{
  PRInt8 result;

  if (mState == STATE_DEFINITE) {
    result = (aOther.mState == STATE_DEFINITE)
           ? Cmp(mMilliseconds, aOther.mMilliseconds)
           : -1;
  } else if (mState == STATE_INDEFINITE) {
    if (aOther.mState == STATE_DEFINITE)
      result = 1;
    else if (aOther.mState == STATE_INDEFINITE)
      result = 0;
    else
      result = -1;
  } else {
    result = (aOther.mState != STATE_UNRESOLVED) ? 1 : 0;
  }

  return result;
}
