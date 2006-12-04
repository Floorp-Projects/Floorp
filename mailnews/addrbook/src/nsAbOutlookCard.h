/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Sun Microsystems, Inc.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Cyrille Moureaux <Cyrille.Moureaux@sun.com>
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
#ifndef nsAbOutlookCard_h___
#define nsAbOutlookCard_h___

#include "nsRDFResource.h"
#include "nsAbCardProperty.h"
#include "nsAbWinHelper.h"

struct nsMapiEntry ;

enum 
{
    index_DisplayName = 0,
    index_EmailAddress,
    index_FirstName,
    index_LastName,
    index_NickName,
    index_WorkPhoneNumber,
    index_HomePhoneNumber,
    index_WorkFaxNumber,
    index_PagerNumber,
    index_MobileNumber,
    index_HomeCity,
    index_HomeState,
    index_HomeZip,
    index_HomeCountry,
    index_WorkCity,
    index_WorkState,
    index_WorkZip,
    index_WorkCountry,
    index_JobTitle,
    index_Department,
    index_Company,
    index_WorkWebPage,
    index_HomeWebPage,
    index_Comments,
    index_LastProp
} ;

static const ULONG OutlookCardMAPIProps[] = 
{
    PR_DISPLAY_NAME_W,
    PR_EMAIL_ADDRESS_W,
    PR_GIVEN_NAME_W,
    PR_SURNAME_W,
    PR_NICKNAME_W,
    PR_BUSINESS_TELEPHONE_NUMBER_W,
    PR_HOME_TELEPHONE_NUMBER_W,
    PR_BUSINESS_FAX_NUMBER_W,
    PR_PAGER_TELEPHONE_NUMBER_W,
    PR_MOBILE_TELEPHONE_NUMBER_W,
    PR_HOME_ADDRESS_CITY_W,
    PR_HOME_ADDRESS_STATE_OR_PROVINCE_W,
    PR_HOME_ADDRESS_POSTAL_CODE_W,
    PR_HOME_ADDRESS_COUNTRY_W,
    PR_BUSINESS_ADDRESS_CITY_W,
    PR_BUSINESS_ADDRESS_STATE_OR_PROVINCE_W,
    PR_BUSINESS_ADDRESS_POSTAL_CODE_W,
    PR_BUSINESS_ADDRESS_COUNTRY_W,
    PR_TITLE_W,
    PR_DEPARTMENT_NAME_W,
    PR_COMPANY_NAME_W,
    PR_BUSINESS_HOME_PAGE_W,
    PR_PERSONAL_HOME_PAGE_W,
    PR_COMMENT_W
} ;

class nsAbOutlookCard : public nsRDFResource, 
                        public nsAbCardProperty
{
public:
    NS_DECL_ISUPPORTS_INHERITED
        
    nsAbOutlookCard();
    virtual ~nsAbOutlookCard();
    
    // nsIRDFResource method
    NS_IMETHOD Init(const char *aUri) ;

protected:
    nsMapiEntry *mMapiData ;
    PRUint32 mAbWinType ;

private:
};

#endif // nsAbOultlookCard_h___
