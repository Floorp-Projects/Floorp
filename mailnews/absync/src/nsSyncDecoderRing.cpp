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
#include "nsAbSync.h"
#include "prmem.h"
#include "nsAbSyncCID.h"
#include "nsIPref.h"
#include "nsIServiceManager.h"
#include "nsEscape.h"
#include "nsSyncDecoderRing.h"

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
char *kServerFirstNameColumn = "fname";
char *kServerLastNameColumn = "lname";
char *kServerDisplayNameColumn = "Display_name";
char *kServerNicknameColumn = "nick_name";
char *kServerPriEmailColumn = "email1";
char *kServer2ndEmailColumn = "email2";
char *kServerPlainTextColumn = "Pref_rich_email";
char *kServerWorkPhoneColumn = "work_phone";
char *kServerHomePhoneColumn = "home_phone";
char *kServerFaxColumn = "fax";
char *kServerPagerColumn = "pager";
char *kServerCellularColumn = "cell_phone";
char *kServerHomeAddressColumn = "home_add1";
char *kServerHomeAddress2Column = "home_add2";
char *kServerHomeCityColumn = "home_city";
char *kServerHomeStateColumn = "home_state";
char *kServerHomeZipCodeColumn = "home_zip";
char *kServerHomeCountryColumn = "home_country";
char *kServerWorkAddressColumn = "work_add1";
char *kServerWorkAddress2Column = "work_add2";
char *kServerWorkCityColumn = "work_city";
char *kServerWorkStateColumn = "work_state";
char *kServerWorkZipCodeColumn = "work_zip";
char *kServerWorkCountryColumn = "work_country";
char *kServerJobTitleColumn = "job_title";
char *kServerDepartmentColumn = "department";
char *kServerCompanyColumn = "company";
char *kServerWebPage1Column = "Work_web_page";
char *kServerWebPage2Column = "web_page";
char *kServerBirthYearColumn = "OMIT:BirthYear";
char *kServerBirthMonthColumn = "OMIT:BirthMonth";
char *kServerBirthDayColumn = "OMIT:BirthDay";
char *kServerCustom1Column = "Custom_1";
char *kServerCustom2Column = "Custom_2";
char *kServerCustom3Column = "Custom_3";
char *kServerCustom4Column = "Custom_4";
char *kServerNotesColumn = "addl_info";
char *kServerLastModifiedDateColumn = "OMIT:LastModifiedDate";

nsSyncDecoderRing::nsSyncDecoderRing()
{
}

nsSyncDecoderRing::~nsSyncDecoderRing()
{
}


