/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NS_SMILTIMEVALUESPECPARAMS_H_
#define NS_SMILTIMEVALUESPECPARAMS_H_

#include "nsSMILTimeValue.h"
#include "nsAtom.h"

//----------------------------------------------------------------------
// nsSMILTimeValueSpecParams
//
// A simple data type for storing the result of parsing a single begin or end
// value (e.g. the '5s' in begin="5s; indefinite; a.begin+2s").

class nsSMILTimeValueSpecParams
{
public:
  nsSMILTimeValueSpecParams()
    : mType(INDEFINITE)
    , mSyncBegin(false)
    , mRepeatIteration(0)
  { }

  // The type of value this specification describes
  enum {
    OFFSET,
    SYNCBASE,
    EVENT,
    REPEAT,
    WALLCLOCK,
    INDEFINITE
  } mType;

  // A clock value that is added to:
  // - type OFFSET: the document begin
  // - type SYNCBASE: the timebase's begin or end time
  // - type EVENT: the event time
  // - type REPEAT: the repeat time
  // It is not used for WALLCLOCK or INDEFINITE times
  nsSMILTimeValue   mOffset;

  // The base element that this specification refers to.
  // For SYNCBASE types, this is the timebase
  // For EVENT and REPEAT types, this is the eventbase
  RefPtr<nsAtom> mDependentElemID;

  // The event to respond to.
  // Only used for EVENT types.
  RefPtr<nsAtom> mEventSymbol;

  // Indicates if this specification refers to the begin or end of the dependent
  // element.
  // Only used for SYNCBASE types.
  bool              mSyncBegin;

  // The repeat iteration to respond to.
  // Only used for mType=REPEAT.
  uint32_t mRepeatIteration;
};

#endif // NS_SMILTIMEVALUESPECPARAMS_H_
