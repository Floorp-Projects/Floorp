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
char *kFirstNameColumn = "FirstName";
char *kLastNameColumn = "LastName";
char *kDisplayNameColumn = "DisplayName";
char *kNicknameColumn = "NickName";
char *kPriEmailColumn = "PrimaryEmail";
char *k2ndEmailColumn = "SecondEmail";
char *kPreferMailFormatColumn = "PreferMailFormat";
char *kWorkPhoneColumn = "WorkPhone";
char *kHomePhoneColumn = "HomePhone";
char *kFaxColumn = "FaxNumber";
char *kPagerColumn = "PagerNumber";
char *kCellularColumn = "CellularNumber";
char *kHomeAddressColumn = "HomeAddress";
char *kHomeAddress2Column = "HomeAddress2";
char *kHomeCityColumn = "HomeCity";
char *kHomeStateColumn = "HomeState";
char *kHomeZipCodeColumn = "HomeZipCode";
char *kHomeCountryColumn = "HomeCountry";
char *kWorkAddressColumn = "WorkAddress";
char *kWorkAddress2Column = "WorkAddress2";
char *kWorkCityColumn = "WorkCity";
char *kWorkStateColumn = "WorkState";
char *kWorkZipCodeColumn = "WorkZipCode";
char *kWorkCountryColumn = "WorkCountry";
char *kJobTitleColumn = "JobTitle";
char *kDepartmentColumn = "Department";
char *kCompanyColumn = "Company";
char *kWebPage1Column = "WebPage1";
char *kWebPage2Column = "WebPage2";
char *kBirthYearColumn = "BirthYear";
char *kBirthMonthColumn = "BirthMonth";
char *kBirthDayColumn = "BirthDay";
char *kCustom1Column = "Custom1";
char *kCustom2Column = "Custom2";
char *kCustom3Column = "Custom3";
char *kCustom4Column = "Custom4";
char *kNotesColumn = "Notes";
char *kLastModifiedDateColumn = "LastModifiedDate";

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

// So far, we aren't really doing anything with these!
char *kAddressCharSetColumn = "AddrCharSet";
char *kMailListName = "ListName";
char *kMailListNickName = "ListNickName";
char *kMailListDescription = "ListDescription";
char *kMailListTotalAddresses = "ListTotalAddresses";
// So far, we aren't really doing anything with these!

nsSyncDecoderRing::nsSyncDecoderRing()
{
}

nsSyncDecoderRing::~nsSyncDecoderRing()
{
}


