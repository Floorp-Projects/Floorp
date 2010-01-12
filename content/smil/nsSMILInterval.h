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

#ifndef NS_SMILINTERVAL_H_
#define NS_SMILINTERVAL_H_

#include "nsSMILInstanceTime.h"

//----------------------------------------------------------------------
// nsSMILInterval class
//
// A structure consisting of a begin and end time. The begin time must be
// resolved (i.e. not indefinite or unresolved).
//
// For an overview of how this class is related to other SMIL time classes see
// the documentation in nsSMILTimeValue.h

class nsSMILInterval
{
public:
  void Set(nsSMILInstanceTime& aBegin, nsSMILInstanceTime& aEnd)
  {
    NS_ABORT_IF_FALSE(aBegin.Time().IsResolved(),
        "Attempting to set unresolved begin time on an interval");
    mBegin = &aBegin;
    mEnd = &aEnd;
  }

  PRBool IsSet() const
  {
    NS_ABORT_IF_FALSE(!mBegin == !mEnd, "Bad interval: only one endpoint set");
    return !!mBegin;
  }

  void Reset()
  {
    mBegin = nsnull;
    mEnd = nsnull;
  }

  // Begin() and End() will be non-null so long as IsSet() is true. Otherwise,
  // they probably shouldn't be called.

  const nsSMILInstanceTime* Begin() const
  {
    NS_ABORT_IF_FALSE(mBegin, "Calling Begin() on un-set interval");
    return mBegin;
  }

  nsSMILInstanceTime* Begin()
  {
    NS_ABORT_IF_FALSE(mBegin, "Calling Begin() on un-set interval");
    return mBegin;
  }

  const nsSMILInstanceTime* End() const
  {
    NS_ABORT_IF_FALSE(mEnd, "Calling End() on un-set interval");
    return mEnd;
  }

  nsSMILInstanceTime* End()
  {
    NS_ABORT_IF_FALSE(mEnd, "Calling End() on un-set interval");
    return mEnd;
  }

  void SetBegin(nsSMILInstanceTime& aBegin)
  {
    NS_ABORT_IF_FALSE(mBegin, "Calling SetBegin() on un-set interval");
    NS_ABORT_IF_FALSE(aBegin.Time().IsResolved(),
        "Attempting to set unresolved begin time on interval");
    mBegin = &aBegin;
  }

  void SetEnd(nsSMILInstanceTime& aEnd)
  {
    NS_ABORT_IF_FALSE(mEnd, "Calling SetEnd() on un-set interval");
    mEnd = &aEnd;
  }

  void FreezeBegin()
  {
    NS_ABORT_IF_FALSE(mBegin, "Calling FreezeBegin() on un-set interval");
    mBegin->MarkNoLongerUpdating();
  }

  void FreezeEnd()
  {
    NS_ABORT_IF_FALSE(mEnd, "Calling FreezeEnd() on un-set interval");
    NS_ABORT_IF_FALSE(!mBegin->MayUpdate(),
        "Freezing the end of an interval without a fixed begin");
    mEnd->MarkNoLongerUpdating();
  }

private:
  nsRefPtr<nsSMILInstanceTime> mBegin;
  nsRefPtr<nsSMILInstanceTime> mEnd;
};

#endif // NS_SMILINTERVAL_H_
