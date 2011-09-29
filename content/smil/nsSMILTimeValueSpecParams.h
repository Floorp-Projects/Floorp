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
    mSyncBegin(PR_FALSE),
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
  PRUint32          mRepeatIterationOrAccessKey;
};

#endif // NS_SMILTIMEVALUESPECPARAMS_H_
