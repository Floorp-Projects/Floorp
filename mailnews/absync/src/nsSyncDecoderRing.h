/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef __nsSyncDecoderRing_h__
#define __nsSyncDecoderRing_h__

#include "nsIAbSync.h"
#include "nsIAbSyncPostListener.h"
#include "nsIAbSyncPostEngine.h"
#include "nsCOMPtr.h"
#include "nsIAddrDatabase.h"
#include "nsIAbDirectory.h"
#include "nsAbSyncCRCModel.h"

// Address book fields!
extern char *kFirstNameColumn;
extern char *kLastNameColumn;
extern char *kDisplayNameColumn; 
extern char *kNicknameColumn;
extern char *kPriEmailColumn;
extern char *k2ndEmailColumn;
extern char *kPreferMailFormatColumn;
extern char *kWorkPhoneColumn;
extern char *kHomePhoneColumn;
extern char *kFaxColumn;
extern char *kPagerColumn;
extern char *kCellularColumn;
extern char *kHomeAddressColumn;
extern char *kHomeAddress2Column;
extern char *kHomeCityColumn;
extern char *kHomeStateColumn;
extern char *kHomeZipCodeColumn; 
extern char *kHomeCountryColumn;
extern char *kWorkAddressColumn; 
extern char *kWorkAddress2Column;
extern char *kWorkCityColumn;
extern char *kWorkStateColumn; 
extern char *kWorkZipCodeColumn; 
extern char *kWorkCountryColumn; 
extern char *kJobTitleColumn;
extern char *kDepartmentColumn;
extern char *kCompanyColumn;
extern char *kWebPage1Column;
extern char *kWebPage2Column;
extern char *kBirthYearColumn; 
extern char *kBirthMonthColumn;
extern char *kBirthDayColumn;
extern char *kCustom1Column;
extern char *kCustom2Column;
extern char *kCustom3Column;
extern char *kCustom4Column;
extern char *kNotesColumn;
extern char *kLastModifiedDateColumn;

// Server record fields!
extern char *kServerFirstNameColumn;
extern char *kServerLastNameColumn;
extern char *kServerDisplayNameColumn;
extern char *kServerNicknameColumn;
extern char *kServerPriEmailColumn;
extern char *kServer2ndEmailColumn;
extern char *kServerPlainTextColumn;
extern char *kServerWorkPhoneColumn;
extern char *kServerHomePhoneColumn;
extern char *kServerFaxColumn;
extern char *kServerPagerColumn;
extern char *kServerCellularColumn;
extern char *kServerHomeAddressColumn;
extern char *kServerHomeAddress2Column;
extern char *kServerHomeCityColumn;
extern char *kServerHomeStateColumn;
extern char *kServerHomeZipCodeColumn;
extern char *kServerHomeCountryColumn;
extern char *kServerWorkAddressColumn;
extern char *kServerWorkAddress2Column;
extern char *kServerWorkCityColumn;
extern char *kServerWorkStateColumn;
extern char *kServerWorkZipCodeColumn;
extern char *kServerWorkCountryColumn;
extern char *kServerJobTitleColumn;
extern char *kServerDepartmentColumn;
extern char *kServerCompanyColumn;
extern char *kServerWebPage1Column;
extern char *kServerWebPage2Column;
extern char *kServerBirthYearColumn;
extern char *kServerBirthMonthColumn;
extern char *kServerBirthDayColumn;
extern char *kServerCustom1Column;
extern char *kServerCustom2Column;
extern char *kServerCustom3Column;
extern char *kServerCustom4Column;
extern char *kServerNotesColumn;
extern char *kServerLastModifiedDateColumn;

// So far, we aren't really doing anything with these!
extern char *kAddressCharSetColumn;
extern char *kMailListName;
extern char *kMailListNickName;
extern char *kMailListDescription;
extern char *kMailListTotalAddresses;
// So far, we aren't really doing anything with these!

class nsSyncDecoderRing
{
public:

  nsSyncDecoderRing();
  virtual ~nsSyncDecoderRing();
};

#endif /* __nsSyncDecoderRing_h__ */
