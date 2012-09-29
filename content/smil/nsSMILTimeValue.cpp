/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsSMILTimeValue.h"

nsSMILTime nsSMILTimeValue::kUnresolvedMillis = INT64_MAX;

//----------------------------------------------------------------------
// nsSMILTimeValue methods:

static inline int8_t
Cmp(int64_t aA, int64_t aB)
{
  return aA == aB ? 0 : (aA > aB ? 1 : -1);
}

int8_t
nsSMILTimeValue::CompareTo(const nsSMILTimeValue& aOther) const
{
  int8_t result;

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
