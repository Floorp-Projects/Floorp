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

#ifndef NS_SMILTIMEVALUESPEC_H_
#define NS_SMILTIMEVALUESPEC_H_

#include "nsISupports.h"
#include "nsCOMPtr.h"
#include "nsSMILTypes.h"

class nsAString;
class nsSMILTimeValue;
class nsSMILTimedElement;

//----------------------------------------------------------------------
// nsSMILTimeValueSpec class
//
// An individual element of a 'begin' or 'end' attribute, e.g. '5s', 'a.end'.
// This class handles the parsing of such specifications and performs the
// necessary event handling (for event, repeat, and accesskey specifications)
// and synchronisation (for syncbase specifications).
//
// For an overview of how this class is related to other SMIL time classes see
// the documentstation in nsSMILTimeValue.h

// {39d2f376-6bda-42c0-8510-a93b24828a80}
#define NS_SMILTIMEVALUESPEC_IID \
{ 0x39d2f376, 0x6bda, 0x42c0, { 0x85, 0x10, 0xa9, 0x3b, 0x24, 0x82, 0x8a, 0x80 } }

class nsSMILTimeValueSpec : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_SMILTIMEVALUESPEC_IID)
  NS_DECL_ISUPPORTS

protected:
  nsSMILTimeValueSpec(nsSMILTimedElement* aOwner, PRBool aIsBegin);

  friend already_AddRefed<nsSMILTimeValueSpec>
  NS_NewSMILTimeValueSpec(nsSMILTimedElement* aOwner,
                          PRBool aIsBegin,
                          const nsAString& aStringSpec);

  nsresult SetSpec(const nsAString& aStringSpec);

  nsSMILTimedElement* mOwner;
  PRPackedBool        mIsBegin;
  nsSMILTime          mOffset;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsSMILTimeValueSpec, NS_SMILTIMEVALUESPEC_IID)

////////////////////////////////////////////////////////////////////////
// Factory methods

already_AddRefed<nsSMILTimeValueSpec>
NS_NewSMILTimeValueSpec(nsSMILTimedElement* aOwner,
                        PRBool aIsBegin,
                        const nsAString& aStringSpec);

#endif // NS_SMILTIMEVALUESPEC_H_
