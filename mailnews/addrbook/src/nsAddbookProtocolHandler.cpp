/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */
#include "msgCore.h"    // precompiled header...
#include "nsXPIDLString.h"
#include "nsIPref.h"
#include "nsIIOService.h"

#include "nsIStreamObserver.h"
#include "nsIStreamListener.h"
#include "nsAddbookProtocolHandler.h"

#include "nsAddbookUrl.h"
#include "nsAddbookProtocolHandler.h"
#include "nsIFileSpec.h"
#include "nsCOMPtr.h"
#include "nsIMsgIdentity.h"
#include "nsAbBaseCID.h"
#include "nsNetUtil.h"
#include "nsIStringStream.h"
#include "nsIAddrBookSession.h"
#include "nsIAbDirectory.h"
#include "nsIRDFResource.h"
#include "nsIRDFService.h"
#include "nsRDFCID.h"

/* The definition is nsAddrDatabase.cpp */
extern const char *kWorkAddressBook;
extern const char *kFirstNameColumn;
extern const char *kLastNameColumn;
extern const char *kDisplayNameColumn;
extern const char *kNicknameColumn;
extern const char *kPriEmailColumn;
extern const char *k2ndEmailColumn;
extern const char *kPlainTextColumn;
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
/* end */

static NS_DEFINE_CID(kCAddbookUrlCID, NS_ADDBOOKURL_CID);
static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID); 
static NS_DEFINE_CID(kAddrBookSessionCID, NS_ADDRBOOKSESSION_CID);
static NS_DEFINE_CID(kAddressBookDBCID, NS_ADDRDATABASE_CID);
static NS_DEFINE_CID(kRDFServiceCID,  NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);

nsAddbookProtocolHandler::nsAddbookProtocolHandler()
{
    NS_INIT_REFCNT();

}

nsAddbookProtocolHandler::~nsAddbookProtocolHandler()
{
}

NS_IMPL_ISUPPORTS(nsAddbookProtocolHandler, NS_GET_IID(nsIProtocolHandler));

NS_METHOD
nsAddbookProtocolHandler::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    if (aOuter)
        return NS_ERROR_NO_AGGREGATION;

    nsAddbookProtocolHandler* ph = new nsAddbookProtocolHandler();
    if (ph == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    return ph->QueryInterface(aIID, aResult);
}

NS_IMETHODIMP nsAddbookProtocolHandler::GetScheme(char * *aScheme)
{
	nsresult rv = NS_OK;
	if (aScheme)
		*aScheme = PL_strdup("addbook");
	else
		rv = NS_ERROR_NULL_POINTER;
	return rv; 
}

NS_IMETHODIMP nsAddbookProtocolHandler::GetDefaultPort(PRInt32 *aDefaultPort)
{
  return NS_OK;
}

NS_IMETHODIMP nsAddbookProtocolHandler::NewURI(const char *aSpec, nsIURI *aBaseURI, nsIURI **_retval)
{
  // get a new smtp url
  nsresult rv = NS_OK;
	nsCOMPtr <nsIURI> addbookUrl;

	rv = nsComponentManager::CreateInstance(kCAddbookUrlCID, NULL, NS_GET_IID(nsIURI), getter_AddRefs(addbookUrl));

	if (NS_SUCCEEDED(rv))
	{
    rv = addbookUrl->SetSpec(aSpec);
    if (NS_SUCCEEDED(rv))
    {
  		rv = addbookUrl->QueryInterface(NS_GET_IID(nsIURI), (void **) _retval);
    }
	}

  return rv;
}

NS_IMETHODIMP
nsAddbookProtocolHandler::GenerateHTMLOutputChannel( char *aHtmlOutput,
                                                     PRInt32  aHtmlOutputSize,
                                                     nsIAddbookUrl *addbookUrl,
                                                     nsIURI *aURI, 
                                                     nsIChannel **_retval)
{
  nsresult                  rv = NS_OK;
  nsIChannel                *channel;
  nsCOMPtr<nsIInputStream>  inStr;
  nsCOMPtr<nsISupports>     s;
  
  if (!aHtmlOutput)
    return NS_ERROR_FAILURE;

  rv = NS_NewStringInputStream(getter_AddRefs(s), aHtmlOutput);
  if (NS_FAILED(rv)) 
    return rv;
  
  inStr = do_QueryInterface(s, &rv);
  if (NS_FAILED(rv)) 
    return rv;
  rv = NS_NewInputStreamChannel(&channel, aURI, inStr, "text/html", aHtmlOutputSize);
  if (NS_FAILED(rv)) 
    return rv;
  
  *_retval = channel;
  return rv;
}

NS_IMETHODIMP 
nsAddbookProtocolHandler::NewChannel(nsIURI *aURI, nsIChannel **_retval)
{
  nsresult      rv = NS_OK;
  char          *outBuf = nsnull;

  //
  // Ok, now that we are here, we need to figure out what oprations we
  // are going to perform...create a stream of buffered data if necessary
  // or launch the UI dialog window and go from there.
  //
  mAddbookOperation = nsIAddbookUrlOperation::InvalidUrl;
  nsCOMPtr <nsIAddbookUrl> addbookUrl = do_QueryInterface(aURI);
  if (!addbookUrl)
    return NS_ERROR_ABORT;

  //
  // Ok, first, lets see what we need to do here and then call the appropriate
  // method to handle the operation
  //
  addbookUrl->GetAddbookOperation(&mAddbookOperation);
  switch (mAddbookOperation)
  {
    case nsIAddbookUrlOperation::PrintIndividual:
    case nsIAddbookUrlOperation::PrintAddressBook:
      rv = GeneratePrintOutput(addbookUrl, &outBuf);
      if ((NS_FAILED(rv) || (!outBuf)))
      {
        char          *eMsg = "Unsupported format/operation requested for \"addbook:\" URL.";
        PRInt32       eSize = nsCRT::strlen(eMsg);
        rv = GenerateHTMLOutputChannel(eMsg, eSize, addbookUrl, aURI, _retval);
        break;
      }
      else
      {
        rv = GenerateHTMLOutputChannel(outBuf, nsCRT::strlen(outBuf), addbookUrl, aURI, _retval);
        PR_FREEIF(outBuf);
      }
      break;

    case nsIAddbookUrlOperation::ImportCards:
    case nsIAddbookUrlOperation::ImportMailingLists:
    case nsIAddbookUrlOperation::ExportCards:
    case nsIAddbookUrlOperation::AddToAddressBook:
    case nsIAddbookUrlOperation::ExportTitle:
    case nsIAddbookUrlOperation::ImportTitle: 
    case nsIAddbookUrlOperation::InvalidUrl:
    default:
      char          *eMsg = "Unsupported format/operation requested for \"addbook:\" URL.";
      PRInt32       eSize = nsCRT::strlen(eMsg);

      rv = GenerateHTMLOutputChannel(eMsg, eSize, addbookUrl, aURI, _retval);
      break;
  }

  return rv;
}

NS_IMETHODIMP 
nsAddbookProtocolHandler::OpenAB(char *aAbName, nsIAddrDatabase **aDatabase)
{
	if (!aDatabase)
    return NS_ERROR_FAILURE;

	nsresult rv = NS_OK;
	nsFileSpec* dbPath = nsnull;

	NS_WITH_SERVICE(nsIAddrBookSession, abSession, kAddrBookSessionCID, &rv); 
	if(NS_SUCCEEDED(rv))
		abSession->GetUserProfileDirectory(&dbPath);
	
	if (dbPath)
	{
    if (!aAbName)
      (*dbPath) += "abook.mab";
    else
      (*dbPath) += aAbName;

		NS_WITH_SERVICE(nsIAddrDatabase, addrDBFactory, kAddressBookDBCID, &rv);

		if (NS_SUCCEEDED(rv) && addrDBFactory)
			rv = addrDBFactory->Open(dbPath, PR_TRUE, getter_AddRefs(aDatabase), PR_TRUE);
	}
  else
    rv = NS_ERROR_FAILURE;

	return rv;
}

NS_IMETHODIMP    
nsAddbookProtocolHandler::AddIndividualUserAttribPair(nsString &aString, const char *aColumn, nsIAbCard *aCard)
{
  PRUnichar     *aName = nsnull;

  if (NS_SUCCEEDED(aCard->GetCardValue(aColumn, &aName)) && (aName) && (*aName))
  {
    aString.Append("<tr>");

    aString.Append("<td><b>");
    aString.Append(aColumn);
    aString.Append("</b></td>");

    aString.Append("<td>");
    aString.Append(aName);
    aString.Append("</td>");

    aString.Append("</tr>");
  }

  return NS_OK;
}

NS_IMETHODIMP  
nsAddbookProtocolHandler::FindPossibleAbName(nsIAbCard  *aCard,
                                             PRUnichar  **retName)
{
  nsresult    rv = NS_ERROR_FAILURE;
  nsVoidArray *attrlist = nsnull;
  nsVoidArray *valuelist = nsnull;

  if (NS_SUCCEEDED(aCard->GetAnonymousStrAttrubutesList(&attrlist)) && attrlist)
  {
    if (NS_SUCCEEDED(aCard->GetAnonymousStrValuesList(&valuelist)) && valuelist)
    {
      char    *attr = nsnull;

      for (PRInt32 i = 0; i<attrlist->Count(); i++)
      {
        attr = (char *)attrlist->ElementAt(i);

        if ((attr) && (!nsCRT::strcasecmp(kWorkAddressBook, attr)))
        {
          char *val = (char *)valuelist->ElementAt(i);
          if ( (val) && (*val) )
          {
            *retName = nsString(val).ToNewUnicode();
            rv = NS_OK;
          }
        }
      }
    }
  }

  return rv;
}

NS_IMETHODIMP    
nsAddbookProtocolHandler::GeneratePrintOutput(nsIAddbookUrl *addbookUrl, 
                                              char          **outBuf)
{
  nsresult        rv = NS_OK;
  nsString        workBuffer = "";
  nsIAddrDatabase *aDatabase = nsnull;
 
  if (!outBuf)
    return NS_ERROR_OUT_OF_MEMORY;

  // Get the address book entry
  nsCOMPtr <nsIRDFResource>     resource = nsnull;
  nsCOMPtr <nsIAbDirectory>     directory = nsnull;
  nsIAbCard                     *urlArgCard;
  PRUnichar                     *workEmail = nsnull;
  char                          *charEmail = nsnull;
  PRUnichar                     *workAb = nsnull;
  char                          *charAb = nsnull;
  char                          *abFileName = nsnull;

  rv = NS_OK;
  // Get the RDF service...
  NS_WITH_SERVICE(nsIRDFService, rdfService, kRDFServiceCID, &rv);
  if (NS_FAILED(rv)) 
    goto EarlyExit;

  // Get the AB card that has all of the URL arguments
  rv = addbookUrl->GetAbCard(&urlArgCard);
  if (NS_FAILED(rv)) 
    goto EarlyExit;

  // Get the email of interest if this is for a specific email message
  if (mAddbookOperation == nsIAddbookUrlOperation::PrintIndividual)
  {
    rv = urlArgCard->GetCardValue(kPriEmailColumn, &workEmail);
    if ( (NS_FAILED(rv)) || (!workEmail) || (!*workEmail)) 
      goto EarlyExit;

    // Make it a char *
    charEmail = nsString(workEmail).ToNewCString();
    if (!charEmail)
      goto EarlyExit;
  }

  // Ok, we need to see if a particular address book was passed in on the 
  // URL string. If not, then we will use the default, but if there was one
  // specified, we need to do a prefs lookup and get the file name of interest
  // The pref format is: user_pref("ldap_2.servers.Richie.filename", "abook-1.mab");
  //
  rv = FindPossibleAbName(urlArgCard, &workAb);
  if ( (NS_SUCCEEDED(rv)) && (workAb) && (*workAb)) 
  {
    // Make it a char *
    charAb = nsString(workAb).ToNewCString();
    if (!charAb)
      goto EarlyExit;

    NS_WITH_SERVICE(nsIPref, pPref, kPrefCID, &rv); 
    if (NS_FAILED(rv) || !pPref) 
		  goto EarlyExit;

    nsCString prefId = "ldap_2.servers.";
    prefId.Append(charAb);
    prefId.Append(".filename");

    rv = pPref->CopyCharPref(prefId, &abFileName);
	  if (NS_FAILED(rv))
      abFileName = nsnull;
  }

  // Now, open the database...for now, make it the default
  rv = OpenAB(abFileName, &aDatabase);
  if (NS_FAILED(rv))
    return rv;

  // this should not be hardcoded to abook.mab
  // RICHIE - this works for any address book...not sure why
  rv = rdfService->GetResource("abdirectory://abook.mab", getter_AddRefs(resource));
  if (NS_FAILED(rv)) 
    goto EarlyExit;
  
  // query interface 
  directory = do_QueryInterface(resource, &rv);
  if (NS_FAILED(rv)) 
    goto EarlyExit;

  // Ok, this is the place where we need to generate output for either a single entry
  // or the entire table...
  //
  if (mAddbookOperation == nsIAddbookUrlOperation::PrintIndividual)
    rv = BuildSingleHTML(aDatabase, directory, charEmail, workBuffer);
  else
    rv = BuildAllHTML(aDatabase, directory, workBuffer);

  *outBuf = workBuffer.ToNewCString();

EarlyExit:
  // Database is open...make sure to close it
  if (aDatabase)
  {
    aDatabase->Close(PR_TRUE);
    // aDatabase->RemoveListener(??? listeners are hanging...I think);
  }
  NS_IF_RELEASE(aDatabase);
  
  NS_IF_RELEASE(urlArgCard);
  PR_FREEIF(charEmail);
  PR_FREEIF(charAb);
  PR_FREEIF(abFileName);
  return rv;
}

NS_IMETHODIMP    
nsAddbookProtocolHandler::BuildSingleHTML(nsIAddrDatabase *aDatabase, nsIAbDirectory *directory, 
                                          char *charEmail, nsString &workBuffer)
{
  PRUnichar                     *aName = nsnull;
  nsCOMPtr <nsIAbCard>          workCard;

  nsresult rv = aDatabase->GetCardForEmailAddress(directory, charEmail, getter_AddRefs(workCard));
  if (NS_FAILED(rv) || (!workCard)) 
    return NS_ERROR_FAILURE;

  // Ok, build a little HTML for output...
  workBuffer.Append("<HTML><BODY>");
  workBuffer.Append("<CENTER>");
  workBuffer.Append("<TABLE BORDER>");

  if (NS_SUCCEEDED(workCard->GetName(&aName)) && (aName))
  {
    workBuffer.Append("<caption><b>");
    workBuffer.Append(aName);
    workBuffer.Append("</b></caption>");
  }
  
  AddIndividualUserAttribPair(workBuffer, kWorkAddressBook, workCard);
  AddIndividualUserAttribPair(workBuffer, kFirstNameColumn, workCard);
  AddIndividualUserAttribPair(workBuffer, kLastNameColumn, workCard);
  AddIndividualUserAttribPair(workBuffer, kDisplayNameColumn, workCard);
  AddIndividualUserAttribPair(workBuffer, kNicknameColumn, workCard);
  AddIndividualUserAttribPair(workBuffer, kPriEmailColumn, workCard);
  AddIndividualUserAttribPair(workBuffer, k2ndEmailColumn, workCard);
  AddIndividualUserAttribPair(workBuffer, kPlainTextColumn, workCard);
  AddIndividualUserAttribPair(workBuffer, kWorkPhoneColumn, workCard);
  AddIndividualUserAttribPair(workBuffer, kHomePhoneColumn, workCard);
  AddIndividualUserAttribPair(workBuffer, kFaxColumn, workCard);
  AddIndividualUserAttribPair(workBuffer, kPagerColumn, workCard);
  AddIndividualUserAttribPair(workBuffer, kCellularColumn, workCard);
  AddIndividualUserAttribPair(workBuffer, kHomeAddressColumn, workCard);
  AddIndividualUserAttribPair(workBuffer, kHomeAddress2Column, workCard);
  AddIndividualUserAttribPair(workBuffer, kHomeCityColumn, workCard);
  AddIndividualUserAttribPair(workBuffer, kHomeStateColumn, workCard);
  AddIndividualUserAttribPair(workBuffer, kHomeZipCodeColumn, workCard);
  AddIndividualUserAttribPair(workBuffer, kHomeCountryColumn, workCard);
  AddIndividualUserAttribPair(workBuffer, kWorkAddressColumn, workCard);
  AddIndividualUserAttribPair(workBuffer, kWorkAddress2Column, workCard);
  AddIndividualUserAttribPair(workBuffer, kWorkCityColumn, workCard);
  AddIndividualUserAttribPair(workBuffer, kWorkStateColumn, workCard);
  AddIndividualUserAttribPair(workBuffer, kWorkZipCodeColumn, workCard);
  AddIndividualUserAttribPair(workBuffer, kWorkCountryColumn, workCard);
  AddIndividualUserAttribPair(workBuffer, kJobTitleColumn, workCard);
  AddIndividualUserAttribPair(workBuffer, kDepartmentColumn, workCard);
  AddIndividualUserAttribPair(workBuffer, kCompanyColumn, workCard);
  AddIndividualUserAttribPair(workBuffer, kWebPage1Column, workCard);
  AddIndividualUserAttribPair(workBuffer, kWebPage2Column, workCard);
  AddIndividualUserAttribPair(workBuffer, kBirthYearColumn, workCard);
  AddIndividualUserAttribPair(workBuffer, kBirthMonthColumn, workCard);
  AddIndividualUserAttribPair(workBuffer, kBirthDayColumn, workCard);
  AddIndividualUserAttribPair(workBuffer, kCustom1Column, workCard);
  AddIndividualUserAttribPair(workBuffer, kCustom2Column, workCard);
  AddIndividualUserAttribPair(workBuffer, kCustom3Column, workCard);
  AddIndividualUserAttribPair(workBuffer, kCustom4Column, workCard);
  AddIndividualUserAttribPair(workBuffer, kNotesColumn, workCard);
  AddIndividualUserAttribPair(workBuffer, kLastModifiedDateColumn, workCard);

  workBuffer.Append("</TABLE>");
  workBuffer.Append("<CENTER>");
  workBuffer.Append("</BODY></HTML>");
  return rv;
}

#define       MAX_FIELDS  39

NS_IMETHODIMP    
nsAddbookProtocolHandler::BuildAllHTML(nsIAddrDatabase *aDatabase, nsIAbDirectory *directory, 
                                       nsString &workBuffer)
{
  nsresult                rv = NS_OK;
  reportColumnStruct      reportColumns[MAX_FIELDS];
  PRUnichar               *aName = nsnull;
  nsCOMPtr <nsIAbCard>    workCard;

  reportColumns[0].abField = kFirstNameColumn;
  reportColumns[1].abField = kLastNameColumn;
  reportColumns[2].abField = kDisplayNameColumn;
  reportColumns[3].abField = kNicknameColumn;
  reportColumns[4].abField = kPriEmailColumn;
  reportColumns[5].abField = k2ndEmailColumn;
  reportColumns[6].abField = kPlainTextColumn;
  reportColumns[7].abField = kWorkPhoneColumn;
  reportColumns[8].abField = kHomePhoneColumn;
  reportColumns[9].abField = kFaxColumn;
  reportColumns[10].abField = kPagerColumn;
  reportColumns[11].abField = kCellularColumn;
  reportColumns[12].abField = kHomeAddressColumn;
  reportColumns[13].abField = kHomeAddress2Column;
  reportColumns[14].abField = kHomeCityColumn;
  reportColumns[15].abField = kHomeStateColumn;
  reportColumns[16].abField = kHomeZipCodeColumn;
  reportColumns[17].abField = kHomeCountryColumn;
  reportColumns[18].abField = kWorkAddressColumn;
  reportColumns[19].abField = kWorkAddress2Column;
  reportColumns[20].abField = kWorkCityColumn;
  reportColumns[21].abField = kWorkStateColumn;
  reportColumns[22].abField = kWorkZipCodeColumn;
  reportColumns[23].abField = kWorkCountryColumn;
  reportColumns[24].abField = kJobTitleColumn;
  reportColumns[25].abField = kDepartmentColumn;
  reportColumns[26].abField = kCompanyColumn;
  reportColumns[27].abField = kWebPage1Column;
  reportColumns[28].abField = kWebPage2Column;
  reportColumns[29].abField = kBirthYearColumn;
  reportColumns[30].abField = kBirthMonthColumn;
  reportColumns[31].abField = kBirthDayColumn;
  reportColumns[32].abField = kCustom1Column;
  reportColumns[33].abField = kCustom2Column;
  reportColumns[34].abField = kCustom3Column;
  reportColumns[35].abField = kCustom4Column;
  reportColumns[36].abField = kNotesColumn;
  reportColumns[37].abField = kLastModifiedDateColumn;
  reportColumns[38].abField = nsnull;

  for (PRInt32 i=0; i<MAX_FIELDS; i++)
  {
    reportColumns[i].includeIt = PR_FALSE;
  }

  // Ok, build a little HTML for output...
  workBuffer.Append("<HTML><BODY>");
  workBuffer.Append("<CENTER>");
  workBuffer.Append("<TABLE BORDER>");

  // RICHIE - need more work here
  // NS_IMETHODIMP nsAddrDatabase::EnumerateCards(nsIAbDirectory *directory, nsIEnumerator **result)

  if (NS_SUCCEEDED(workCard->GetName(&aName)) && (aName))
  {
    workBuffer.Append("<caption><b>");
    workBuffer.Append(aName);
    workBuffer.Append("</b></caption>");
  }

  workBuffer.Append("</TABLE>");
  workBuffer.Append("<CENTER>");
  workBuffer.Append("</BODY></HTML>");
  return rv;
}
