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
 * Portions created by the Initial Developer are Copyright (C) 2009
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

#include "prtypes.h"
#include "nsDebug.h"
#include <math.h>

//----------------------------------------------------------------------
// nsSMILRepeatCount
//
// A tri-state non-negative floating point number for representing the number of
// times an animation repeat, i.e. the SMIL repeatCount attribute.
//
// The three states are:
//  1. not-set
//  2. set (with non-negative, non-zero count value)
//  3. indefinite
//
class nsSMILRepeatCount
{
public:
  nsSMILRepeatCount() : mCount(kNotSet) {}
  explicit nsSMILRepeatCount(double aCount)
    : mCount(kNotSet) { SetCount(aCount); }

  operator double() const { return mCount; }
  bool IsDefinite() const {
    return mCount != kNotSet && mCount != kIndefinite;
  }
  bool IsIndefinite() const { return mCount == kIndefinite; }
  bool IsSet() const { return mCount != kNotSet; }

  nsSMILRepeatCount& operator=(double aCount)
  {
    SetCount(aCount);
    return *this;
  }
  void SetCount(double aCount)
  {
    NS_ASSERTION(aCount > 0.0, "Negative or zero repeat count");
    mCount = aCount > 0.0 ? aCount : kNotSet;
  }
  void SetIndefinite() { mCount = kIndefinite; }
  void Unset() { mCount = kNotSet; }

private:
  static const double kNotSet;
  static const double kIndefinite;

  double  mCount;
};
