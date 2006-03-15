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
 * The Original Code is Mozilla Schema Validation.
 *
 * The Initial Developer of the Original Code is
 * IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * IBM Corporation. All Rights Reserved.
 *
 * Contributor(s):
 *   Doron Rosenberg <doronr@us.ibm.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsSchemaDuration.h"

// string includes
#include "nsReadableUtils.h"
#include "nsString.h"
#include "nsUnicharUtils.h"

// XPCOM includes
#include "nsMemory.h"
#include "nsIServiceManager.h"
#include "nsIComponentManager.h"
#include "nsIClassInfoImpl.h"

#include <stdlib.h>
#include <math.h>
#include "prprf.h"
#include "prtime.h"
#include "plbase64.h"

NS_IMPL_ISUPPORTS1_CI(nsSchemaDuration, nsISchemaDuration)

nsSchemaDuration::nsSchemaDuration(PRUint32 aYears, PRUint32 aMonths,
                                   PRUint32 aDays, PRUint32 aHours,
                                   PRUint32 aMinutes,  PRUint32 aSeconds,
                                   double aFractionalSeconds, PRBool aNegative)
{
  years = aYears;
  months = aMonths;
  days = aDays;
  hours = aHours;
  minutes = aMinutes;
  seconds = aSeconds;
  fractional_seconds = aFractionalSeconds;
  negative = aNegative;
}

nsSchemaDuration::~nsSchemaDuration()
{
}

////////////////////////////////////////////////////////////
//
// nsSchemaDuration implementation
//
////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsSchemaDuration::GetYears(PRUint32 *aResult)
{
  *aResult = years;
  return NS_OK;
}

NS_IMETHODIMP
nsSchemaDuration::GetMonths(PRUint32 *aResult)
{
  *aResult = months;
  return NS_OK;
}

NS_IMETHODIMP
nsSchemaDuration::GetDays(PRUint32 *aResult)
{
  *aResult = days;
  return NS_OK;
}

NS_IMETHODIMP
nsSchemaDuration::GetHours(PRUint32 *aResult)
{
  *aResult = hours;
  return NS_OK;
}

NS_IMETHODIMP
nsSchemaDuration::GetMinutes(PRUint32 *aResult)
{
  *aResult = minutes;
  return NS_OK;
}

NS_IMETHODIMP
nsSchemaDuration::GetSeconds(PRUint32 *aResult)
{
  *aResult = seconds;
  return NS_OK;
}

NS_IMETHODIMP
nsSchemaDuration::GetFractionSeconds(double *aResult)
{
  *aResult = fractional_seconds;
  return NS_OK;
}

NS_IMETHODIMP
nsSchemaDuration::GetNegative(PRBool *aResult)
{
  *aResult = negative;
  return NS_OK;
}
