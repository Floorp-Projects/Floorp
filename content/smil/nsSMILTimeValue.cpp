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
