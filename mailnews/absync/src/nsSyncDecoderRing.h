/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef __nsSyncDecoderRing_h__
#define __nsSyncDecoderRing_h__

#include "nsIAbSync.h"
#include "nsIAbSyncPostListener.h"
#include "nsIAbSyncPostEngine.h"
#include "nsCOMPtr.h"
#include "nsIAddrDatabase.h"
#include "nsIAbDirectory.h"
#include "nsAbSyncCRCModel.h"


// Server record fields!
extern const char kServerFirstNameColumn[];
extern const char kServerLastNameColumn[];
extern const char kServerDisplayNameColumn[];
extern const char kServerNicknameColumn[];
extern const char kServerPriEmailColumn[];
extern const char kServer2ndEmailColumn[];
extern const char kServerPlainTextColumn[];
extern const char kServerWorkPhoneColumn[];
extern const char kServerHomePhoneColumn[];
extern const char kServerFaxColumn[];
extern const char kServerPagerColumn[];
extern const char kServerCellularColumn[];
extern const char kServerHomeAddressColumn[];
extern const char kServerHomeAddress2Column[];
extern const char kServerHomeCityColumn[];
extern const char kServerHomeStateColumn[];
extern const char kServerHomeZipCodeColumn[];
extern const char kServerHomeCountryColumn[];
extern const char kServerWorkAddressColumn[];
extern const char kServerWorkAddress2Column[];
extern const char kServerWorkCityColumn[];
extern const char kServerWorkStateColumn[];
extern const char kServerWorkZipCodeColumn[];
extern const char kServerWorkCountryColumn[];
extern const char kServerJobTitleColumn[];
extern const char kServerDepartmentColumn[];
extern const char kServerCompanyColumn[];
extern const char kServerWebPage1Column[];
extern const char kServerWebPage2Column[];
extern const char kServerBirthYearColumn[];
extern const char kServerBirthMonthColumn[];
extern const char kServerBirthDayColumn[];
extern const char kServerCustom1Column[];
extern const char kServerCustom2Column[];
extern const char kServerCustom3Column[];
extern const char kServerCustom4Column[];
extern const char kServerNotesColumn[];
extern const char kServerLastModifiedDateColumn[];

class nsSyncDecoderRing
{
public:

  nsSyncDecoderRing();
  virtual ~nsSyncDecoderRing();
};

#endif /* __nsSyncDecoderRing_h__ */
