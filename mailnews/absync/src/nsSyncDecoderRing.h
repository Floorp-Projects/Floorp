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
extern const char *kFirstNameColumn;
extern const char *kLastNameColumn;
extern const char *kDisplayNameColumn; 
extern const char *kNicknameColumn;
extern const char *kPriEmailColumn;
extern const char *k2ndEmailColumn;
extern const char *kPreferMailFormatColumn;
extern const char *kWorkPhoneColumn;
extern const char *kHomePhoneColumn;
extern const char *kFaxColumn;
extern const char *kPagerColumn;
extern const char *kCellularColumn;
extern const char *kHomeAddressColumn;
extern const char *kHomeAddress2Column;
extern const char *kHomeCityColumn;
extern const char *kHomeStateColumn;
extern const char *kHomeZipCodeColumn; 
extern const char *kHomeCountryColumn;
extern const char *kWorkAddressColumn; 
extern const char *kWorkAddress2Column;
extern const char *kWorkCityColumn;
extern const char *kWorkStateColumn; 
extern const char *kWorkZipCodeColumn; 
extern const char *kWorkCountryColumn; 
extern const char *kJobTitleColumn;
extern const char *kDepartmentColumn;
extern const char *kCompanyColumn;
extern const char *kWebPage1Column;
extern const char *kWebPage2Column;
extern const char *kBirthYearColumn; 
extern const char *kBirthMonthColumn;
extern const char *kBirthDayColumn;
extern const char *kCustom1Column;
extern const char *kCustom2Column;
extern const char *kCustom3Column;
extern const char *kCustom4Column;
extern const char *kNotesColumn;
extern const char *kLastModifiedDateColumn;

// So far, we aren't really doing anything with these!
extern const char *kAddressCharSetColumn;
extern const char *kMailListName;
extern const char *kMailListNickName;
extern const char *kMailListDescription;
extern const char *kMailListTotalAddresses;
// So far, we aren't really doing anything with these!

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

class nsSyncDecoderRing
{
public:

  nsSyncDecoderRing();
  virtual ~nsSyncDecoderRing();
};

#endif /* __nsSyncDecoderRing_h__ */
