/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 *   Pierre Phaneuf <pp@ludusdesign.com>
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
#include "msgCore.h"    // precompiled header...
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsIPref.h"
#include "nsIIOService.h"

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
#include "nsIAbMDBCard.h"
#include "nsIAbDirectory.h"
#include "nsIRDFResource.h"
#include "nsIRDFService.h"
#include "nsRDFCID.h"
#include "prmem.h"

extern const char *kWorkAddressBook;

static NS_DEFINE_CID(kCAddbookUrlCID, NS_ADDBOOKURL_CID);
static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID); 
static NS_DEFINE_CID(kAddrBookSessionCID, NS_ADDRBOOKSESSION_CID);
static NS_DEFINE_CID(kAddressBookDBCID, NS_ADDRDATABASE_CID);
static NS_DEFINE_CID(kRDFServiceCID,  NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);

nsAddbookProtocolHandler::nsAddbookProtocolHandler()
{
  NS_INIT_REFCNT();
  mReportColumns = nsnull;
}

nsAddbookProtocolHandler::~nsAddbookProtocolHandler()
{
  PR_FREEIF(mReportColumns);
  mReportColumns = nsnull;
}

NS_IMPL_ISUPPORTS1(nsAddbookProtocolHandler, nsIProtocolHandler);

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
		*aScheme = nsCRT::strdup("addbook");
	else
		rv = NS_ERROR_NULL_POINTER;
	return rv; 
}

NS_IMETHODIMP nsAddbookProtocolHandler::GetDefaultPort(PRInt32 *aDefaultPort)
{
  return NS_OK;
}

NS_IMETHODIMP nsAddbookProtocolHandler::GetProtocolFlags(PRUint32 *aUritype)
{
  *aUritype = URI_STD;
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
nsAddbookProtocolHandler::AllowPort(PRInt32 port, const char *scheme, PRBool *_retval)
{
    // don't override anything.  
    *_retval = PR_FALSE;
    return NS_OK;
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

  rv = NS_NewStringInputStream(getter_AddRefs(s), NS_ConvertASCIItoUCS2(aHtmlOutput));
  NS_ENSURE_SUCCESS(rv, rv);
  
  inStr = do_QueryInterface(s, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = NS_NewInputStreamChannel(&channel, aURI, inStr, "text/html", aHtmlOutputSize);
  NS_ENSURE_SUCCESS(rv, rv);
  
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

	nsCOMPtr<nsIAddrBookSession> abSession = 
	         do_GetService(kAddrBookSessionCID, &rv); 
	if(NS_SUCCEEDED(rv))
		abSession->GetUserProfileDirectory(&dbPath);
	
	if (dbPath)
	{
    if (!aAbName)
      (*dbPath) += kPersonalAddressbook;
    else
      (*dbPath) += aAbName;

		nsCOMPtr<nsIAddrDatabase> addrDBFactory = 
		         do_GetService(kAddressBookDBCID, &rv);

		if (NS_SUCCEEDED(rv) && addrDBFactory)
			rv = addrDBFactory->Open(dbPath, PR_TRUE, aDatabase, PR_TRUE);

    delete dbPath;
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
    aString.AppendWithConversion("<tr>");

    aString.AppendWithConversion("<td><b>");
    // RICHIE - Should really convert this to some string bundled thing? 
    aString.AppendWithConversion(aColumn);
    aString.AppendWithConversion("</b></td>");

    aString.AppendWithConversion("<td>");
    aString.Append(aName);
    aString.AppendWithConversion("</td>");

    aString.AppendWithConversion("</tr>");
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

  nsCOMPtr<nsIAbMDBCard> dbaCard(do_QueryInterface(aCard, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  if (NS_SUCCEEDED(dbaCard->GetAnonymousStrAttrubutesList(&attrlist)) && attrlist)
  {
    if (NS_SUCCEEDED(dbaCard->GetAnonymousStrValuesList(&valuelist)) && valuelist)
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
            *retName = ToNewUnicode(nsDependentCString(val));
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
  nsString        workBuffer;
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
  nsCOMPtr<nsIRDFService> rdfService(do_GetService(kRDFServiceCID, &rv));
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
    charEmail = ToNewCString(nsDependentString(workEmail));
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
    charAb = ToNewCString(nsDependentString(workAb));
    if (!charAb)
      goto EarlyExit;

    nsCOMPtr<nsIPref> pPref(do_GetService(kPrefCID, &rv)); 
    if (NS_FAILED(rv) || !pPref) 
		  goto EarlyExit;

    nsCString prefId("ldap_2.servers.");
    prefId.Append(charAb);
    prefId.Append(".filename");

    rv = pPref->CopyCharPref(prefId.get(), &abFileName);
	  if (NS_FAILED(rv))
      abFileName = nsnull;
  }

  // Now, open the database...for now, make it the default
  rv = OpenAB(abFileName, &aDatabase);
  NS_ENSURE_SUCCESS(rv, rv);

  // RICHIE - this works for any address book...not sure why
  rv = rdfService->GetResource(kPersonalAddressbookUri, getter_AddRefs(resource));
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

  *outBuf = ToNewUTF8String(workBuffer);

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

  if (NS_FAILED(InitPrintColumns()))
    return NS_ERROR_FAILURE;

  nsresult rv = aDatabase->GetCardForEmailAddress(directory, charEmail, getter_AddRefs(workCard));
  NS_ENSURE_SUCCESS(rv, rv);
  if (!workCard) 
    return NS_ERROR_FAILURE;

  // Ok, build a little HTML for output...
  workBuffer.AppendWithConversion("<HTML><BODY>");
  workBuffer.AppendWithConversion("<CENTER>");
  workBuffer.AppendWithConversion("<TABLE BORDER>");

  if (NS_SUCCEEDED(workCard->GetName(&aName)) && (aName))
  {
    workBuffer.AppendWithConversion("<caption><b>");
    workBuffer.Append(aName);
    workBuffer.AppendWithConversion("</b></caption>");
  }

  for (PRInt32 i=0; i<kMaxReportColumns; i++)
    AddIndividualUserAttribPair(workBuffer,  mReportColumns[i].abField, workCard);

  workBuffer.AppendWithConversion("</TABLE>");
  workBuffer.AppendWithConversion("<CENTER>");
  workBuffer.AppendWithConversion("</BODY></HTML>");
  return rv;
}

NS_IMETHODIMP    
nsAddbookProtocolHandler::BuildAllHTML(nsIAddrDatabase *aDatabase, nsIAbDirectory *directory, 
                                       nsString &workBuffer)
{
  nsresult                rv = NS_OK;

  if (NS_FAILED(InitPrintColumns()))
    return NS_ERROR_FAILURE;

  nsIEnumerator     *cardEnum = nsnull;
  rv = aDatabase->EnumerateCards(directory, &cardEnum);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!cardEnum)
    return NS_ERROR_FAILURE;

  InitPrintColumns();

  // Ok, we make 2 passes at this enumerator. The first is to
  // check the columns that we should output and the second is
  // to actually generate the output
  //
  nsCOMPtr<nsISupports>   obj = nsnull;
  cardEnum->First();
  do
  {
    if (NS_FAILED(cardEnum->CurrentItem(getter_AddRefs(obj))))
      break;
    else
    {
      nsCOMPtr<nsIAbCard> card;
      card = do_QueryInterface(obj, &rv);
      if ( NS_SUCCEEDED(rv) && (card) )
      {
        CheckColumnValidity(card);
      }
    }

  } while (NS_SUCCEEDED(cardEnum->Next()));

  // Now, we need to generate some fun output!
  // 
  // Ok, build a little HTML for output...
  workBuffer.AppendWithConversion("<HTML><BODY>");
  workBuffer.AppendWithConversion("<CENTER>");
  workBuffer.AppendWithConversion("<TABLE BORDER>");

  GenerateColumnHeadings(workBuffer);

  cardEnum->First();
  do
  {
    if (NS_FAILED(cardEnum->CurrentItem(getter_AddRefs(obj))))
      break;
    else
    {
      nsCOMPtr<nsIAbCard> card;
      card = do_QueryInterface(obj, &rv);
      if ( NS_SUCCEEDED(rv) && (card) )
      {
        GenerateRowForCard(workBuffer, card);
      }
    }

  } while (NS_SUCCEEDED(cardEnum->Next()));

  delete cardEnum;

  // Finish up and get out!
  //
  workBuffer.AppendWithConversion("</TABLE>");
  workBuffer.AppendWithConversion("<CENTER>");
  workBuffer.AppendWithConversion("</BODY></HTML>");
  return rv;
}

PRBool
ValidColumn(nsIAbCard *aCard, const char *aColumn)
{
  PRUnichar     *aName = nsnull;

  if (NS_SUCCEEDED(aCard->GetCardValue(aColumn, &aName)) && (aName) && (*aName))
    return PR_TRUE;
  else
    return PR_FALSE;
}

nsresult
TackOnColumn(nsIAbCard *aCard, const char *aColumn, nsString &aString)
{
  PRUnichar     *aName = nsnull;

  aString.AppendWithConversion("<td>");

  if (NS_SUCCEEDED(aCard->GetCardValue(aColumn, &aName)) && (aName) && (*aName))
  {
    aString.Append(aName);
  }

  aString.AppendWithConversion("</td>");
  return NS_OK;
}

NS_IMETHODIMP    
nsAddbookProtocolHandler::CheckColumnValidity(nsIAbCard *aCard)
{
  for (PRInt32 i=0; i<kMaxReportColumns; i++)
  {
    if (!mReportColumns[i].includeIt)
      mReportColumns[i].includeIt = ValidColumn(aCard, mReportColumns[i].abField);
  }

  return NS_OK;
}

NS_IMETHODIMP    
nsAddbookProtocolHandler::GenerateColumnHeadings(nsString           &aString)
{
  aString.AppendWithConversion("<tr>");

  for (PRInt32 i=0; i<kMaxReportColumns; i++)
  {
    if (mReportColumns[i].includeIt)
    {
      aString.AppendWithConversion("<td>");
      aString.AppendWithConversion("<B>");
      // RICHIE - Should really convert this to some string bundled thing? 
      aString.AppendWithConversion(mReportColumns[i].abField);
      aString.AppendWithConversion("</B>");
      aString.AppendWithConversion("</td>");
    }
  }

  aString.AppendWithConversion("</tr>");
  return NS_OK;
}

NS_IMETHODIMP    
nsAddbookProtocolHandler::GenerateRowForCard(nsString           &aString, 
                                             nsIAbCard          *aCard)
{
  aString.AppendWithConversion("<tr>");

  for (PRInt32 i=0; i<kMaxReportColumns; i++)
  {
    if (mReportColumns[i].includeIt)
      TackOnColumn(aCard, mReportColumns[i].abField, aString);
  }

  aString.AppendWithConversion("</tr>");
  return NS_OK;
}

NS_IMETHODIMP
nsAddbookProtocolHandler::InitPrintColumns()
{
  if (!mReportColumns)
  {
    mReportColumns = (reportColumnStruct *) PR_Malloc(sizeof(reportColumnStruct) * kMaxReportColumns);
    if (mReportColumns == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;

    mReportColumns[0].abField = kFirstNameColumn;
    mReportColumns[1].abField = kLastNameColumn;
    mReportColumns[2].abField = kDisplayNameColumn;
    mReportColumns[3].abField = kNicknameColumn;
    mReportColumns[4].abField = kPriEmailColumn;
    mReportColumns[5].abField = k2ndEmailColumn;
    mReportColumns[6].abField = kPreferMailFormatColumn;
    mReportColumns[7].abField = kWorkPhoneColumn;
    mReportColumns[8].abField = kHomePhoneColumn;
    mReportColumns[9].abField = kFaxColumn;
    mReportColumns[10].abField = kPagerColumn;
    mReportColumns[11].abField = kCellularColumn;
    mReportColumns[12].abField = kHomeAddressColumn;
    mReportColumns[13].abField = kHomeAddress2Column;
    mReportColumns[14].abField = kHomeCityColumn;
    mReportColumns[15].abField = kHomeStateColumn;
    mReportColumns[16].abField = kHomeZipCodeColumn;
    mReportColumns[17].abField = kHomeCountryColumn;
    mReportColumns[18].abField = kWorkAddressColumn;
    mReportColumns[19].abField = kWorkAddress2Column;
    mReportColumns[20].abField = kWorkCityColumn;
    mReportColumns[21].abField = kWorkStateColumn;
    mReportColumns[22].abField = kWorkZipCodeColumn;
    mReportColumns[23].abField = kWorkCountryColumn;
    mReportColumns[24].abField = kJobTitleColumn;
    mReportColumns[25].abField = kDepartmentColumn;
    mReportColumns[26].abField = kCompanyColumn;
    mReportColumns[27].abField = kWebPage1Column;
    mReportColumns[28].abField = kWebPage2Column;
    mReportColumns[29].abField = kBirthYearColumn;
    mReportColumns[30].abField = kBirthMonthColumn;
    mReportColumns[31].abField = kBirthDayColumn;
    mReportColumns[32].abField = kCustom1Column;
    mReportColumns[33].abField = kCustom2Column;
    mReportColumns[34].abField = kCustom3Column;
    mReportColumns[35].abField = kCustom4Column;
    mReportColumns[36].abField = kNotesColumn;
    mReportColumns[37].abField = kLastModifiedDateColumn;
    mReportColumns[38].abField = nsnull;
  }

  for (PRInt32 i=0; i<kMaxReportColumns; i++)
  {
    mReportColumns[i].includeIt = PR_FALSE;
  }

  return NS_OK;
}
