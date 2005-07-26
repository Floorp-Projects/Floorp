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

#ifndef __nsSchemaDuration_h__
#define __nsSchemaDuration_h__

#include "nsSchemaValidatorUtils.h"
#include "nsISchemaDuration.h"
#include "nsISchema.h"
#include "nsCOMPtr.h"

/* 85dad673-28ce-414d-b46b-5c3cf4fd18a6 */
#define NS_SCHEMADURATION_CID \
{ 0x85dad673, 0x28ce, 0x414d, \
  {0xb4, 0x6b, 0x5c, 0x3c, 0xf4, 0xfd, 0x18, 0xa6}}

#define NS_SCHEMADURATION_CONTRACTID "@mozilla.org/schemavalidator/schemaduration;1"


class nsSchemaDuration : public nsISchemaDuration
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISCHEMADURATION

  nsSchemaDuration(PRUint32 aYears, PRUint32 aMonths, PRUint32 aDays,
                   PRUint32 aHours, PRUint32 aMinutes,  PRUint32 aSeconds,
                   double aFractionalSeconds, PRBool aNegative);
private:
  ~nsSchemaDuration();
  PRUint32 years;
  PRUint32 months;
  PRUint32 days;
  PRUint32 hours;
  PRUint32 minutes;
  PRUint32 seconds;
  double fractional_seconds;

  PRBool negative;

protected:

};

#endif // __nsSchemaDuration_h__
