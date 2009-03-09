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

#ifndef NS_SMILTIMEVALUE_H_
#define NS_SMILTIMEVALUE_H_

#include "prtypes.h"
#include "prlong.h"
#include "nsSMILTypes.h"

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
 * nsSMILInterval      -- a pair of nsSMILTimeValues that defines a begin and an
 *                        end point.
 * nsSMILInstanceTime  -- an nsSMILTimeValue used for constructing intervals. It
 *                        contains additional fields to govern reset behavior.
 *                        It also forms an anchor for establishing syncbase
 *                        relationships.
 * nsSMILTimeValueSpec -- a component of a begin or end attribute, such as the
 *                        '5s' or 'a.end+2m' in begin="5s; a.end+2m". These
 *                        objects produce nsSMILInstanceTimes and are also used
 *                        in managing dependent relationships between
 *                        animations.
 *
 * Objects of this class may be in one of three states:
 *
 * 1) The time is resolved and has a millisecond value
 * 2) The time is indefinite
 * 3) The time in unresolved
 *
 * There is considerable chance for confusion with regards to the indefinite
 * state. Is it resolved? We adopt the convention that it is NOT resolved (but
 * nor is it unresolved). This simplifies implementation as you can then write:
 *
 * if (time.IsResolved())
 *    x = time.GetMillis()
 *
 * instead of:
 *
 * if (time.IsResolved() && !time.IsIndefinite())
 *    x = time.GetMillis()
 *
 * Testing if a time is unresolved becomes more complicated but this is tested
 * much less often.
 *
 * In summary:
 *
 * State         |  GetMillis         |  IsResolved        |  IsIndefinite
 * --------------+--------------------+--------------------+-------------------
 * Resolved      |  The millisecond   |  PR_TRUE           |  PR_FALSE
 *               |  time              |                    |
 * --------------+--------------------+--------------------+-------------------
 * Indefinite    |  LL_MAXINT         |  PR_FALSE          |  PR_TRUE
 * --------------+--------------------+--------------------+-------------------
 * Unresolved    |  LL_MAXINT         |  PR_FALSE          |  PR_FALSE
 *
 */

class nsSMILTimeValue
{
public:
  // Creates an unresolved time value
  nsSMILTimeValue();

  PRBool            IsIndefinite() const { return mState == STATE_INDEFINITE; }
  void              SetIndefinite();

  PRBool            IsResolved() const { return mState == STATE_RESOLVED; }
  void              SetUnresolved();

  nsSMILTime        GetMillis() const;
  void              SetMillis(nsSMILTime aMillis);

  PRInt8            CompareTo(const nsSMILTimeValue& aOther) const;

private:
  PRInt8            Cmp(PRInt64 aA, PRInt64 aB) const;

  static nsSMILTime kUnresolvedSeconds;

  nsSMILTime        mMilliseconds;
  enum {
    STATE_RESOLVED,
    STATE_INDEFINITE,
    STATE_UNRESOLVED
  } mState;
};

#endif // NS_SMILTIMEVALUE_H_
