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
#define kServerFirstNameColumn NS_LITERAL_STRING("fname")
#define kServerLastNameColumn NS_LITERAL_STRING("lname")
#define kServerDisplayNameColumn NS_LITERAL_STRING("Display_name")
#define kServerNicknameColumn NS_LITERAL_STRING("nick_name")
#define kServerPriEmailColumn NS_LITERAL_STRING("email1")
#define kServer2ndEmailColumn NS_LITERAL_STRING("email2")
#define kServerPlainTextColumn NS_LITERAL_STRING("Pref_rich_email")
#define kServerWorkPhoneColumn NS_LITERAL_STRING("work_phone")
#define kServerHomePhoneColumn NS_LITERAL_STRING("home_phone")
#define kServerFaxColumn NS_LITERAL_STRING("fax")
#define kServerPagerColumn NS_LITERAL_STRING("pager")
#define kServerCellularColumn NS_LITERAL_STRING("cell_phone")
#define kServerHomeAddressColumn NS_LITERAL_STRING("home_add1")
#define kServerHomeAddress2Column NS_LITERAL_STRING("home_add2")
#define kServerHomeCityColumn NS_LITERAL_STRING("home_city")
#define kServerHomeStateColumn NS_LITERAL_STRING("home_state")
#define kServerHomeZipCodeColumn NS_LITERAL_STRING("home_zip")
#define kServerHomeCountryColumn NS_LITERAL_STRING("home_country")
#define kServerWorkAddressColumn NS_LITERAL_STRING("work_add1")
#define kServerWorkAddress2Column NS_LITERAL_STRING("work_add2")
#define kServerWorkCityColumn NS_LITERAL_STRING("work_city")
#define kServerWorkStateColumn NS_LITERAL_STRING("work_state")
#define kServerWorkZipCodeColumn NS_LITERAL_STRING("work_zip")
#define kServerWorkCountryColumn NS_LITERAL_STRING("work_country")
#define kServerJobTitleColumn NS_LITERAL_STRING("job_title")
#define kServerDepartmentColumn NS_LITERAL_STRING("department")
#define kServerCompanyColumn NS_LITERAL_STRING("company")
#define kServerWebPage1Column NS_LITERAL_STRING("Work_web_page")
#define kServerWebPage2Column NS_LITERAL_STRING("web_page")
#define kServerBirthYearColumn NS_LITERAL_STRING("OMIT:BirthYear")
#define kServerBirthMonthColumn NS_LITERAL_STRING("OMIT:BirthMonth")
#define kServerBirthDayColumn NS_LITERAL_STRING("OMIT:BirthDay")
#define kServerCustom1Column NS_LITERAL_STRING("Custom_1")
#define kServerCustom2Column NS_LITERAL_STRING("Custom_2")
#define kServerCustom3Column NS_LITERAL_STRING("Custom_3")
#define kServerCustom4Column NS_LITERAL_STRING("Custom_4")
#define kServerNotesColumn NS_LITERAL_STRING("addl_info")
#define kServerLastModifiedDateColumn NS_LITERAL_STRING("OMIT:LastModifiedDate")

class nsSyncDecoderRing
{
public:

  nsSyncDecoderRing();
  virtual ~nsSyncDecoderRing();
};

#endif /* __nsSyncDecoderRing_h__ */
