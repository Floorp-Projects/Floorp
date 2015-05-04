/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NS_SMILTIMEVALUE_H_
#define NS_SMILTIMEVALUE_H_

#include "nsSMILTypes.h"
#include "nsDebug.h"

/*----------------------------------------------------------------------
 * nsSMILTimeValue class
 *
 * A tri-state time value.
 *
 * First a quick overview of the SMIL time data types:
 *
 * nsSMILTime          -- a timestamp in milliseconds.
 * nsSMILTimeValue     -- (this class) a timestamp that can take the additional
 *                        states 'indefinite' and 'unresolved'
 * nsSMILInstanceTime  -- an nsSMILTimeValue used for constructing intervals. It
 *                        contains additional fields to govern reset behavior
 *                        and track timing dependencies (e.g. syncbase timing).
 * nsSMILInterval      -- a pair of nsSMILInstanceTimes that defines a begin and
 *                        an end time for animation.
 * nsSMILTimeValueSpec -- a component of a begin or end attribute, such as the
 *                        '5s' or 'a.end+2m' in begin="5s; a.end+2m". Acts as
 *                        a broker between an nsSMILTimedElement and its
 *                        nsSMILInstanceTimes by generating new instance times
 *                        and handling changes to existing times.
 *
 * Objects of this class may be in one of three states:
 *
 * 1) The time is resolved and has a definite millisecond value
 * 2) The time is resolved and indefinite
 * 3) The time is unresolved
 *
 * In summary:
 *
 * State      | GetMillis       | IsDefinite | IsIndefinite | IsResolved
 * -----------+-----------------+------------+--------------+------------
 * Definite   | nsSMILTimeValue | true       | false        | true
 * -----------+-----------------+------------+--------------+------------
 * Indefinite | --              | false      | true         | true
 * -----------+-----------------+------------+--------------+------------
 * Unresolved | --              | false      | false        | false
 *
 */

class nsSMILTimeValue
{
public:
  // Creates an unresolved time value
  nsSMILTimeValue()
  : mMilliseconds(kUnresolvedMillis),
    mState(STATE_UNRESOLVED)
  { }

  // Creates a resolved time value
  explicit nsSMILTimeValue(nsSMILTime aMillis)
  : mMilliseconds(aMillis),
    mState(STATE_DEFINITE)
  { }

  // Named constructor to create an indefinite time value
  static nsSMILTimeValue Indefinite()
  {
    nsSMILTimeValue value;
    value.SetIndefinite();
    return value;
  }

  bool IsIndefinite() const { return mState == STATE_INDEFINITE; }
  void SetIndefinite()
  {
    mState = STATE_INDEFINITE;
    mMilliseconds = kUnresolvedMillis;
  }

  bool IsResolved() const { return mState != STATE_UNRESOLVED; }
  void SetUnresolved()
  {
    mState = STATE_UNRESOLVED;
    mMilliseconds = kUnresolvedMillis;
  }

  bool IsDefinite() const { return mState == STATE_DEFINITE; }
  nsSMILTime GetMillis() const
  {
    MOZ_ASSERT(mState == STATE_DEFINITE,
               "GetMillis() called for unresolved or indefinite time");

    return mState == STATE_DEFINITE ? mMilliseconds : kUnresolvedMillis;
  }

  void SetMillis(nsSMILTime aMillis)
  {
    mState = STATE_DEFINITE;
    mMilliseconds = aMillis;
  }

  int8_t CompareTo(const nsSMILTimeValue& aOther) const;

  bool operator==(const nsSMILTimeValue& aOther) const
  { return CompareTo(aOther) == 0; }

  bool operator!=(const nsSMILTimeValue& aOther) const
  { return CompareTo(aOther) != 0; }

  bool operator<(const nsSMILTimeValue& aOther) const
  { return CompareTo(aOther) < 0; }

  bool operator>(const nsSMILTimeValue& aOther) const
  { return CompareTo(aOther) > 0; }

  bool operator<=(const nsSMILTimeValue& aOther) const
  { return CompareTo(aOther) <= 0; }

  bool operator>=(const nsSMILTimeValue& aOther) const
  { return CompareTo(aOther) >= 0; }

private:
  static nsSMILTime kUnresolvedMillis;

  nsSMILTime mMilliseconds;
  enum {
    STATE_DEFINITE,
    STATE_INDEFINITE,
    STATE_UNRESOLVED
  } mState;
};

#endif // NS_SMILTIMEVALUE_H_
