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
 *   Seth Spitzer <sspitzer@netscape.com>
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
#include "nsAbOutlookCard.h"
#include "nsAbWinHelper.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsIAddrBookSession.h"
#include "nsAbBaseCID.h"

#include "prlog.h"

#ifdef PR_LOGGING
static PRLogModuleInfo* gAbOutlookCardLog
    = PR_NewLogModule("nsAbOutlookCardLog");
#endif

#define PRINTF(args) PR_LOG(gAbOutlookCardLog, PR_LOG_DEBUG, args)

extern const char *kOutlookDirectoryScheme ;
extern const char *kOutlookCardScheme ;

nsAbOutlookCard::nsAbOutlookCard(void)
: nsRDFResource(), nsAbCardProperty(), mAbWinType(nsAbWinType_Unknown), mMapiData(nsnull)
{
    mMapiData = new nsMapiEntry ;
}

nsAbOutlookCard::~nsAbOutlookCard(void)
{
    if (mMapiData) { delete mMapiData ; }
}

NS_IMPL_ISUPPORTS_INHERITED1(nsAbOutlookCard, nsRDFResource, nsIAbCard)

static void splitString(nsString& aSource, nsString& aTarget)
{
    aTarget.Truncate() ;
    PRInt32 offset = aSource.FindChar('\n') ;
    
    if (offset >= 0) { 
        const PRUnichar *source = aSource.get() + offset + 1 ;
        
        while (*source) {
            if (*source == '\n' || *source == '\r') { aTarget.Append(PRUnichar(' ')) ; }
            else { aTarget.Append(*source) ; }
            ++ source ;
        }
        aSource.Truncate(offset) ; 
    }
}

static void wordToUnicode(WORD aWord, nsString& aUnicode)
{
    aUnicode.Truncate() ;
    aUnicode.AppendInt((PRInt32) aWord) ;
}

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

static const ULONG OutlookCardMAPIProps [] = 
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

nsresult nsAbOutlookCard::Init(const char *aUri)
{
    nsresult retCode = nsRDFResource::Init(aUri) ;
    
    NS_ENSURE_SUCCESS(retCode, retCode) ;
    nsCAutoString entry ;
    nsCAutoString stub ;

    mAbWinType = getAbWinType(kOutlookCardScheme, mURI.get(), stub, entry) ;
    if (mAbWinType == nsAbWinType_Unknown) {
        PRINTF(("Huge problem URI=%s.\n", mURI.get())) ;
        return NS_ERROR_INVALID_ARG ;
    }
    nsAbWinHelperGuard mapiAddBook (mAbWinType) ;

    if (!mapiAddBook->IsOK()) { return NS_ERROR_FAILURE ; }
    mMapiData->Assign(entry) ;
    nsStringArray unichars ;
    ULONG i = 0 ;

    if (mapiAddBook->GetPropertiesUString(*mMapiData, OutlookCardMAPIProps, index_LastProp, unichars)) {
        SetFirstName(unichars [index_FirstName]->get()) ;
        SetLastName(unichars [index_LastName]->get()) ;
        SetDisplayName(unichars [index_DisplayName]->get()) ;
        SetNickName(unichars [index_NickName]->get()) ;
        SetPrimaryEmail(unichars [index_EmailAddress]->get()) ;
        SetWorkPhone(unichars [index_WorkPhoneNumber]->get()) ;
        SetHomePhone(unichars [index_HomePhoneNumber]->get()) ;
        SetFaxNumber(unichars [index_WorkFaxNumber]->get()) ;
        SetPagerNumber(unichars [index_PagerNumber]->get()) ;
        SetCellularNumber(unichars [index_MobileNumber]->get()) ;
        SetHomeCity(unichars [index_HomeCity]->get()) ;
        SetHomeState(unichars [index_HomeState]->get()) ;
        SetHomeZipCode(unichars [index_HomeZip]->get()) ;
        SetHomeCountry(unichars [index_HomeCountry]->get()) ;
        SetWorkCity(unichars [index_WorkCity]->get()) ;
        SetWorkState(unichars [index_WorkState]->get()) ;
        SetWorkZipCode(unichars [index_WorkZip]->get()) ;
        SetWorkCountry(unichars [index_WorkCountry]->get()) ;
        SetJobTitle(unichars [index_JobTitle]->get()) ;
        SetDepartment(unichars [index_Department]->get()) ;
        SetCompany(unichars [index_Company]->get()) ;
        SetWebPage1(unichars [index_WorkWebPage]->get()) ;
        SetWebPage2(unichars [index_HomeWebPage]->get()) ;
        SetNotes(unichars [index_Comments]->get()) ;
    }
    ULONG cardType = 0 ;
    nsCAutoString normalChars ;
    
    if (mapiAddBook->GetPropertyLong(*mMapiData, PR_OBJECT_TYPE, cardType)) {
        SetIsMailList(cardType == MAPI_DISTLIST) ;
        if (cardType == MAPI_DISTLIST) {
            buildAbWinUri(kOutlookDirectoryScheme, mAbWinType, normalChars) ;
            normalChars.Append(entry) ;
            SetMailListURI(normalChars.get()) ;
        }
    }
    nsAutoString unichar ;
    nsAutoString unicharBis ;

    if (mapiAddBook->GetPropertyUString(*mMapiData, PR_HOME_ADDRESS_STREET_W, unichar)) {
        splitString(unichar, unicharBis) ;
        SetHomeAddress(unichar.get()) ;
        SetHomeAddress2(unicharBis.get()) ;
    }
    if (mapiAddBook->GetPropertyUString(*mMapiData, PR_BUSINESS_ADDRESS_STREET_W, unichar)) {
        splitString(unichar, unicharBis) ;
        SetWorkAddress(unichar.get()) ;
        SetWorkAddress2(unicharBis.get()) ;
    }
    WORD year = 0 ;
    WORD month = 0 ;
    WORD day = 0 ;

    if (mapiAddBook->GetPropertyDate(*mMapiData, PR_BIRTHDAY, year, month, day)) {
        wordToUnicode(year, unichar) ;
        SetBirthYear(unichar.get()) ;
        wordToUnicode(month, unichar) ;
        SetBirthMonth(unichar.get()) ;
        wordToUnicode(day, unichar) ;
        SetBirthDay(unichar.get()) ;
    }
    return retCode ;
}

static void UnicodeToWord(const PRUnichar *aUnicode, WORD& aWord)
{
    aWord = 0 ;
    if (aUnicode == nsnull || *aUnicode == 0) { return ; }
    PRInt32 errorCode = 0 ;
    nsAutoString unichar (aUnicode) ;

    aWord = NS_STATIC_CAST(WORD, unichar.ToInteger(&errorCode)) ;
    if (errorCode != 0) {
        PRINTF(("Error conversion string %S: %08x.\n", unichar.get(), errorCode)) ;
    }
}

#define PREF_MAIL_ADDR_BOOK_LASTNAMEFIRST "mail.addr_book.lastnamefirst"

NS_IMETHODIMP nsAbOutlookCard::EditCardToDatabase(const char *aUru)
{
    nsresult retCode = NS_OK ;
    nsXPIDLString *properties = nsnull ;
    nsAutoString utility ;
    nsAbWinHelperGuard mapiAddBook (mAbWinType) ;

    if (!mapiAddBook->IsOK()) { return NS_ERROR_FAILURE ; }
    // First, all the standard properties in one go
    properties = new nsXPIDLString [index_LastProp] ;
    if (!properties) {
        return NS_ERROR_OUT_OF_MEMORY ;
    }
    GetFirstName(getter_Copies(properties [index_FirstName])) ;
    GetLastName(getter_Copies(properties [index_LastName])) ;
    // This triple search for something to put in the name
    // is because in the case of a mailing list edition in 
    // Mozilla, the display name will not be provided, and 
    // MAPI doesn't allow that, so we fall back on an optional
    // name, and when all fails, on the email address.
    GetDisplayName(getter_Copies(properties [index_DisplayName])) ;
    if (*properties [index_DisplayName].get() == 0) {
      nsresult rv;
      nsCOMPtr<nsIPrefBranch> prefBranch = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
      NS_ENSURE_SUCCESS(rv,rv);
      
      PRInt32 format;
      rv = prefBranch->GetIntPref(PREF_MAIL_ADDR_BOOK_LASTNAMEFIRST, &format);
      NS_ENSURE_SUCCESS(rv,rv);
      
      nsCOMPtr<nsIAddrBookSession> abSession = do_GetService(NS_ADDRBOOKSESSION_CONTRACTID, &rv);
      NS_ENSURE_SUCCESS(rv,rv);
      
      rv = abSession->GenerateNameFromCard(this, format, getter_Copies(properties [index_DisplayName]));
      NS_ENSURE_SUCCESS(rv,rv);
      
        if (*properties[index_DisplayName].get() == 0) {
            GetPrimaryEmail(getter_Copies(properties [index_DisplayName])) ;
        }
    }
    SetDisplayName(properties [index_DisplayName]) ;
    GetNickName(getter_Copies(properties [index_NickName])) ;
    GetPrimaryEmail(getter_Copies(properties [index_EmailAddress])) ;
    GetWorkPhone(getter_Copies(properties [index_WorkPhoneNumber])) ;
    GetHomePhone(getter_Copies(properties [index_HomePhoneNumber])) ;
    GetFaxNumber(getter_Copies(properties [index_WorkFaxNumber])) ;
    GetPagerNumber(getter_Copies(properties [index_PagerNumber])) ;
    GetCellularNumber(getter_Copies(properties [index_MobileNumber])) ;
    GetHomeCity(getter_Copies(properties [index_HomeCity])) ;
    GetHomeState(getter_Copies(properties [index_HomeState])) ;
    GetHomeZipCode(getter_Copies(properties [index_HomeZip])) ;
    GetHomeCountry(getter_Copies(properties [index_HomeCountry])) ;
    GetWorkCity(getter_Copies(properties [index_WorkCity])) ;
    GetWorkState(getter_Copies(properties [index_WorkState])) ;
    GetWorkZipCode(getter_Copies(properties [index_WorkZip])) ;
    GetWorkCountry(getter_Copies(properties [index_WorkCountry])) ;
    GetJobTitle(getter_Copies(properties [index_JobTitle])) ;
    GetDepartment(getter_Copies(properties [index_Department])) ;
    GetCompany(getter_Copies(properties [index_Company])) ;
    GetWebPage1(getter_Copies(properties [index_WorkWebPage])) ;
    GetWebPage2(getter_Copies(properties [index_HomeWebPage])) ;
    GetNotes(getter_Copies(properties [index_Comments])) ;
    if (!mapiAddBook->SetPropertiesUString(*mMapiData, OutlookCardMAPIProps, 
        index_LastProp, properties)) {
        PRINTF(("Cannot set general properties.\n")) ;
    }
    delete [] properties ;
    nsXPIDLString unichar ;
    nsXPIDLString unichar2 ;
    WORD year = 0 ; 
    WORD month = 0 ;
    WORD day = 0 ;
    
    GetHomeAddress(getter_Copies(unichar)) ;
    GetHomeAddress2(getter_Copies(unichar2)) ;
    utility.Assign(unichar.get()) ;
    if (!utility.IsEmpty()) { utility.AppendWithConversion(CRLF) ; }
    utility.Append(unichar2.get()) ;
    if (!mapiAddBook->SetPropertyUString(*mMapiData, PR_HOME_ADDRESS_STREET_W, utility.get())) {
        PRINTF(("Cannot set home address.\n")) ;
    }
    GetWorkAddress(getter_Copies(unichar)) ;
    GetWorkAddress2(getter_Copies(unichar2)) ;
    utility.Assign(unichar.get()) ;
    if (!utility.IsEmpty()) { utility.AppendWithConversion(CRLF) ; }
    utility.Append(unichar2.get()) ;
    if (!mapiAddBook->SetPropertyUString(*mMapiData, PR_BUSINESS_ADDRESS_STREET_W, utility.get())) {
        PRINTF(("Cannot set work address.\n")) ;
    }
    GetBirthYear(getter_Copies(unichar)) ;
    UnicodeToWord(unichar.get(), year) ;
    GetBirthMonth(getter_Copies(unichar)) ;
    UnicodeToWord(unichar.get(), month) ;
    GetBirthDay(getter_Copies(unichar)) ;
    UnicodeToWord(unichar.get(), day) ;
    if (!mapiAddBook->SetPropertyDate(*mMapiData, PR_BIRTHDAY, year, month, day)) {
        PRINTF(("Cannot set date.\n")) ;
    }
    return retCode ;
}






