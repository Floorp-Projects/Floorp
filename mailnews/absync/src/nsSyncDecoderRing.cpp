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
#include "nsAbSync.h"
#include "prmem.h"
#include "nsAbSyncCID.h"
#include "nsIPref.h"
#include "nsIServiceManager.h"
#include "nsEscape.h"
#include "nsSyncDecoderRing.h"

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


