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

#ifndef NS_SMILINSTANCETIME_H_
#define NS_SMILINSTANCETIME_H_

#include "nsISupports.h"
#include "nsSMILTimeValue.h"
#include "nsWeakReference.h"
#include "nsAutoPtr.h"

class nsSMILTimeValueSpec;

//----------------------------------------------------------------------
// nsSMILInstanceTime
//
// An instant in document simple time that may be used in creating a new
// interval
//
// For an overview of how this class is related to other SMIL time classes see
// the documentstation in nsSMILTimeValue.h

class nsSMILInstanceTime
{
public:
  nsSMILInstanceTime(const nsSMILTimeValue& aTime,
                     nsSMILTimeValueSpec* aCreator,
                     PRBool aClearOnReset = PR_FALSE);

  ~nsSMILInstanceTime();

  const nsSMILTimeValue& Time() const { return mTime; }

  PRBool                 ClearOnReset() const { return mClearOnReset; }

  // void DependentUpdate(const nsSMILTimeValue& aNewTime); -- NOT YET IMPL.

  // Used by nsTArray::Sort
  class Comparator {
    public:
      PRBool Equals(const nsSMILInstanceTime& aElem1,
                    const nsSMILInstanceTime& aElem2) const {
        return (aElem1.Time().CompareTo(aElem2.Time()) == 0);
      }
      PRBool LessThan(const nsSMILInstanceTime& aElem1,
                      const nsSMILInstanceTime& aElem2) const {
        return (aElem1.Time().CompareTo(aElem2.Time()) < 0);
      }
  };

protected:
  nsSMILTimeValue     mTime;

  /**
   * Indicates if this instance time should be removed when the owning timed
   * element is reset. True for events and DOM calls.
   */
  PRPackedBool        mClearOnReset;
};

#endif // NS_SMILINSTANCETIME_H_
