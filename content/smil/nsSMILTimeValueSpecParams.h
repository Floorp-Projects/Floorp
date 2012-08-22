/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NS_SMILTIMEVALUESPECPARAMS_H_
#define NS_SMILTIMEVALUESPECPARAMS_H_

#include "nsSMILTimeValue.h"
#include "nsAutoPtr.h"
#include "nsIAtom.h"

//----------------------------------------------------------------------
// nsSMILTimeValueSpecParams
//
// A simple data type for storing the result of parsing a single begin or end
// value (e.g. the '5s' in begin="5s; indefinite; a.begin+2s").

class nsSMILTimeValueSpecParams
{
public:
  nsSMILTimeValueSpecParams()
  :
    mType(INDEFINITE),
    mSyncBegin(false),
    mRepeatIterationOrAccessKey(0)
  { }

  // The type of value this specification describes
  enum {
    OFFSET,
    SYNCBASE,
    EVENT,
    REPEAT,
    ACCESSKEY,
    WALLCLOCK,
    INDEFINITE
  } mType;

  // A clock value that is added to:
  // - type OFFSET: the document begin
  // - type SYNCBASE: the timebase's begin or end time
  // - type EVENT: the event time
  // - type REPEAT: the repeat time
  // - type ACCESSKEY: the keypress time
  // It is not used for WALLCLOCK or INDEFINITE times
  nsSMILTimeValue   mOffset;

  // The base element that this specification refers to.
  // For SYNCBASE types, this is the timebase
  // For EVENT and REPEAT types, this is the eventbase
  nsRefPtr<nsIAtom> mDependentElemID;

  // The event to respond to.
  // Only used for EVENT types.
  nsRefPtr<nsIAtom> mEventSymbol;

  // Indicates if this specification refers to the begin or end of the dependent
  // element.
  // Only used for SYNCBASE types.
  bool              mSyncBegin;

  // The repeat iteration (type=REPEAT) or access key (type=ACCESSKEY) to
  // respond to.
  uint32_t          mRepeatIterationOrAccessKey;
};

#endif // NS_SMILTIMEVALUESPECPARAMS_H_
