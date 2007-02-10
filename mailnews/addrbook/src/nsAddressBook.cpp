/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Seth Spitzer <sspitzer@netscape.com>
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   Mark Banner <mark@standard8.demon.co.uk>
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

#include "nscore.h"
#include "nsIAddressBook.h"
#include "nsAddressBook.h"
#include "nsAbBaseCID.h"
#include "nsDirPrefs.h"
#include "nsIAddrBookSession.h"
#include "nsIAbLDIFService.h"
#include "nsAddrDatabase.h"
#include "nsIAbMDBDirectory.h"
#include "nsIOutputStream.h"
#include "nsNetUtil.h"
#include "msgCore.h"
#include "nsIImportService.h"
#include "nsIStringBundle.h"

#include "plstr.h"
#include "prmem.h"
#include "prprf.h" 

#include "nsCOMPtr.h"
#include "nsIRDFCompositeDataSource.h"
#include "nsIRDFResource.h"
#include "nsRDFResource.h"
#include "nsIRDFService.h"
#include "nsIServiceManager.h"
#include "nsIDOMWindowInternal.h"
#include "nsIContentViewer.h"
#include "nsIDocShell.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsICategoryManager.h"
#include "nsIFilePicker.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsIPrefLocalizedString.h"
#include "nsIAbCard.h"
#include "nsIAbMDBCard.h"
#include "plbase64.h"
#include "nsIWindowWatcher.h"

#include "nsEscape.h"
#include "nsVCard.h"
#include "nsVCardObj.h"
#include "nsISupportsPrimitives.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIDocShell.h"
#include "nsAutoPtr.h"
#include "nsIMsgVCardService.h"
#include "nsCRT.h"
#include "nsIAbLDAPAttributeMap.h"

#ifdef MOZ_XUL_APP
#include "nsICommandLine.h"
#endif

// our schema is not fixed yet, but we still want some sort of objectclass
// for now, use obsolete in the class name, hinting that this will change
// see bugs bug #116692 and #118454
#define MOZ_AB_OBJECTCLASS "mozillaAbPersonAlpha"

const ExportAttributesTableStruct EXPORT_ATTRIBUTES_TABLE[EXPORT_ATTRIBUTES_TABLE_COUNT] = { 
  {kFirstNameColumn, 2100},
  {kLastNameColumn, 2101},
  {kDisplayNameColumn, 2102},
  {kNicknameColumn, 2103},
  {kPriEmailColumn, 2104},
  {k2ndEmailColumn, 2105},
  {kAimScreenNameColumn},
  {kPreferMailFormatColumn},
  {kLastModifiedDateColumn},
  {kWorkPhoneColumn, 2106},
  {kWorkPhoneTypeColumn},
  {kHomePhoneColumn, 2107},
  {kHomePhoneTypeColumn},
  {kFaxColumn, 2108},
  {kFaxTypeColumn},
  {kPagerColumn, 2109},
  {kPagerTypeColumn},
  {kCellularColumn, 2110},
  {kCellularTypeColumn},
  {kHomeAddressColumn, 2111},
  {kHomeAddress2Column, 2112},
  {kHomeCityColumn, 2113},
  {kHomeStateColumn, 2114},
  {kHomeZipCodeColumn, 2115},
  {kHomeCountryColumn, 2116},
  {kWorkAddressColumn, 2117},
  {kWorkAddress2Column, 2118},
  {kWorkCityColumn, 2119},
  {kWorkStateColumn, 2120},
  {kWorkZipCodeColumn, 2121}, 
  {kWorkCountryColumn, 2122}, 
  {kJobTitleColumn, 2123},
  {kDepartmentColumn, 2124},
  {kCompanyColumn, 2125},
  {kWebPage1Column, 2126},
  {kWebPage2Column, 2127},
  {kBirthYearColumn, 2128}, // unused for now
  {kBirthMonthColumn, 2129}, // unused for now
  {kBirthDayColumn, 2130}, // unused for now
  {kCustom1Column, 2131},
  {kCustom2Column, 2132},
  {kCustom3Column, 2133},
  {kCustom4Column, 2134},
  {kNotesColumn, 2135},
  {kAnniversaryYearColumn},
  {kAnniversaryMonthColumn},
  {kAnniversaryDayColumn},
  {kSpouseNameColumn},
  {kFamilyNameColumn},
  {kDefaultAddressColumn},
  {kCategoryColumn},
};

//
// nsAddressBook
//
nsAddressBook::nsAddressBook()
{
}

nsAddressBook::~nsAddressBook()
{
}

NS_IMPL_THREADSAFE_ADDREF(nsAddressBook)
NS_IMPL_THREADSAFE_RELEASE(nsAddressBook)
NS_IMPL_QUERY_INTERFACE4(nsAddressBook,
                         nsIAddressBook,
                         ICOMMANDLINEHANDLER,
                         nsIContentHandler,
                         nsIStreamLoaderObserver)

//
// nsIAddressBook
//

NS_IMETHODIMP nsAddressBook::NewAddressBook(nsIAbDirectoryProperties *aProperties)
{
  NS_ENSURE_ARG_POINTER(aProperties);

  nsresult rv;

  nsCOMPtr<nsIRDFService> rdfService = do_GetService (NS_RDF_CONTRACTID "/rdf-service;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIRDFResource> parentResource;
  rv = rdfService->GetResource(NS_LITERAL_CSTRING(kAllDirectoryRoot),
                               getter_AddRefs(parentResource));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIAbDirectory> parentDir = do_QueryInterface(parentResource, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = parentDir->CreateNewDirectory(aProperties);
  return rv;
}

NS_IMETHODIMP nsAddressBook::ModifyAddressBook
(nsIRDFDataSource* aDS, nsIAbDirectory *aParentDir, nsIAbDirectory *aDirectory, nsIAbDirectoryProperties *aProperties)
{
  NS_ENSURE_ARG_POINTER(aDS);
  NS_ENSURE_ARG_POINTER(aParentDir);
  NS_ENSURE_ARG_POINTER(aDirectory);
  NS_ENSURE_ARG_POINTER(aProperties);

  nsresult rv;
  nsCOMPtr<nsISupportsArray> parentArray (do_CreateInstance(NS_SUPPORTSARRAY_CONTRACTID, &rv));
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsISupportsArray> resourceElement (do_CreateInstance(NS_SUPPORTSARRAY_CONTRACTID, &rv));
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsISupportsArray> resourceArray (do_CreateInstance(NS_SUPPORTSARRAY_CONTRACTID, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  parentArray->AppendElement(aParentDir);

  nsCOMPtr<nsIRDFResource> dirSource(do_QueryInterface(aDirectory, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  resourceElement->AppendElement(dirSource);
  resourceElement->AppendElement(aProperties);
  resourceArray->AppendElement(resourceElement);

  return DoCommand(aDS, NS_LITERAL_CSTRING(NC_RDF_MODIFY), parentArray, resourceArray);
}

NS_IMETHODIMP nsAddressBook::DeleteAddressBooks
(nsIRDFDataSource* aDS, nsISupportsArray *aParentDir, nsISupportsArray *aResourceArray)
{
  NS_ENSURE_ARG_POINTER(aDS);
  NS_ENSURE_ARG_POINTER(aParentDir);
  NS_ENSURE_ARG_POINTER(aResourceArray);
  
  return DoCommand(aDS, NS_LITERAL_CSTRING(NC_RDF_DELETE), aParentDir, aResourceArray);
}

nsresult nsAddressBook::DoCommand(nsIRDFDataSource* db,
                                  const nsACString& command,
                                  nsISupportsArray *srcArray,
                                  nsISupportsArray *argumentArray)
{
  nsresult rv = NS_OK;

  nsCOMPtr<nsIRDFService> rdfService = do_GetService (NS_RDF_CONTRACTID "/rdf-service;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIRDFResource> commandResource;
  rv = rdfService->GetResource(command, getter_AddRefs(commandResource));
  if(NS_SUCCEEDED(rv))
  {
    rv = db->DoCommand(srcArray, commandResource, argumentArray);
  }

  return rv;
}

NS_IMETHODIMP nsAddressBook::GetAbDatabaseFromURI(const char *aURI, nsIAddrDatabase **aDB)
{
  NS_ENSURE_ARG_POINTER(aURI);
  NS_ENSURE_ARG_POINTER(aDB);
  
  nsresult rv;
  
  nsCOMPtr<nsIAddrBookSession> abSession = 
    do_GetService(NS_ADDRBOOKSESSION_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv,rv);

  nsCOMPtr<nsILocalFile> dbPath;
  rv = abSession->GetUserProfileDirectory(getter_AddRefs(dbPath));
  NS_ENSURE_SUCCESS(rv,rv);

 /* directory URIs are of the form
  * moz-abmdbdirectory://foo
  * mailing list URIs are of the form
  * moz-abmdbdirectory://foo/bar
  *
  * if we are passed a mailing list URI, we want the db for the parent.
  */
  if (strlen(aURI) < kMDBDirectoryRootLen)
    return NS_ERROR_UNEXPECTED;

  nsCAutoString file(aURI + kMDBDirectoryRootLen);
  PRInt32 pos = file.Find("/");
  if (pos != kNotFound)
    file.Truncate(pos);
  rv = dbPath->AppendNative(file);
  NS_ENSURE_SUCCESS(rv,rv);
  
  nsCOMPtr<nsIAddrDatabase> addrDBFactory = 
    do_GetService(NS_ADDRDATABASE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv,rv);

  /* Don't create otherwise we end up re-opening a deleted address book */
  /* bug 66410 */
  rv = addrDBFactory->Open(dbPath, PR_FALSE /* no create */, PR_TRUE, aDB);
  
  return rv;
}

nsresult nsAddressBook::GetAbDatabaseFromFile(char* pDbFile, nsIAddrDatabase **db)
{
    nsresult rv = NS_OK;
    nsCOMPtr<nsIAddrDatabase> database;
    if (pDbFile)
    {
        nsCOMPtr<nsILocalFile> dbPath;

        nsCOMPtr<nsIAddrBookSession> abSession = 
                 do_GetService(NS_ADDRBOOKSESSION_CONTRACTID, &rv); 
        if(NS_SUCCEEDED(rv))
        {
          rv = abSession->GetUserProfileDirectory(getter_AddRefs(dbPath));
          NS_ENSURE_SUCCESS(rv, rv);
        }
        
        nsCAutoString file(pDbFile);
        rv = dbPath->AppendNative(file);
        NS_ENSURE_SUCCESS(rv,rv);

        nsCOMPtr<nsIAddrDatabase> addrDBFactory = 
                 do_GetService(NS_ADDRDATABASE_CONTRACTID, &rv);
        if (NS_SUCCEEDED(rv) && addrDBFactory)
            rv = addrDBFactory->Open(dbPath, PR_TRUE, PR_TRUE, getter_AddRefs(database));

        if (NS_SUCCEEDED(rv) && database)
        {
            NS_IF_ADDREF(*db = database);
        }
        else
            rv = NS_ERROR_NULL_POINTER;

    }
    return NS_OK;
}

NS_IMETHODIMP nsAddressBook::MailListNameExists(const PRUnichar *name, PRBool *exist)
{
  *exist = PR_FALSE;
  nsVoidArray* pDirectories = DIR_GetDirectories();
  if (pDirectories)
  {
    PRInt32 count = pDirectories->Count();
    /* check: only show personal address book for now */
    /* not showing 4.x address book unitl we have the converting done */
    PRInt32 i;
    for (i = 0; i < count; i++)
    {
      DIR_Server *server = (DIR_Server *)pDirectories->ElementAt(i);
      if (server->dirType == PABDirectory)
      {
        /* check: this is a 4.x file, remove when conversion is done */
        PRUint32 fileNameLen = strlen(server->fileName);
        if ((fileNameLen > kABFileName_PreviousSuffixLen) && 
          strcmp(server->fileName + fileNameLen - kABFileName_PreviousSuffixLen, kABFileName_PreviousSuffix) == 0)
          continue;
        
        nsCOMPtr<nsIAddrDatabase> database;
        nsresult rv = GetAbDatabaseFromFile(server->fileName, getter_AddRefs(database));                
        if (NS_SUCCEEDED(rv) && database)
        {
          database->FindMailListbyUnicodeName(name, exist);
          if (*exist)
            return NS_OK;
        }
      }
    }
  }
  return NS_OK;
}

#define CSV_DELIM ","
#define CSV_DELIM_LEN 1
#define TAB_DELIM "\t"
#define TAB_DELIM_LEN 1

#define CSV_FILE_EXTENSION ".csv"
#define TAB_FILE_EXTENSION ".tab"
#define TXT_FILE_EXTENSION ".txt"
#define LDIF_FILE_EXTENSION ".ldi"
#define LDIF_FILE_EXTENSION2 ".ldif"

enum ADDRESSBOOK_EXPORT_FILE_TYPE 
{
 LDIF_EXPORT_TYPE =  0,
 CSV_EXPORT_TYPE = 1,
 TAB_EXPORT_TYPE = 2
};

NS_IMETHODIMP nsAddressBook::ExportAddressBook(nsIDOMWindow *aParentWin, nsIAbDirectory *aDirectory)
{
  NS_ENSURE_ARG_POINTER(aParentWin);

  nsresult rv;
  nsCOMPtr<nsIFilePicker> filePicker = do_CreateInstance("@mozilla.org/filepicker;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIStringBundleService> bundleService = do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIStringBundle> bundle;
  rv = bundleService->CreateBundle("chrome://messenger/locale/addressbook/addressBook.properties", getter_AddRefs(bundle));
  NS_ENSURE_SUCCESS(rv, rv);
 
  nsXPIDLString title;
  rv = bundle->GetStringFromName(NS_LITERAL_STRING("ExportAddressBookTitle").get(), getter_Copies(title));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = filePicker->Init(aParentWin, title, nsIFilePicker::modeSave);
  NS_ENSURE_SUCCESS(rv, rv);
 
  nsXPIDLString filterString;
  rv = bundle->GetStringFromName(NS_LITERAL_STRING("LDIFFiles").get(), getter_Copies(filterString));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = filePicker->AppendFilter(filterString, NS_LITERAL_STRING("*.ldi; *.ldif"));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = bundle->GetStringFromName(NS_LITERAL_STRING("CSVFiles").get(), getter_Copies(filterString));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = filePicker->AppendFilter(filterString, NS_LITERAL_STRING("*.csv"));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = bundle->GetStringFromName(NS_LITERAL_STRING("TABFiles").get(), getter_Copies(filterString));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = filePicker->AppendFilter(filterString, NS_LITERAL_STRING("*.tab; *.txt"));
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt16 dialogResult;
  filePicker->Show(&dialogResult);

  if (dialogResult == nsIFilePicker::returnCancel)
    return rv;

  nsCOMPtr<nsILocalFile> localFile;
  rv = filePicker->GetFile(getter_AddRefs(localFile));
  NS_ENSURE_SUCCESS(rv, rv);
  
  if (dialogResult == nsIFilePicker::returnReplace) {
    // be extra safe and only delete when the file is really a file
    PRBool isFile;
    rv = localFile->IsFile(&isFile);
    if (NS_SUCCEEDED(rv) && isFile) {
      rv = localFile->Remove(PR_FALSE /* recursive delete */);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  // The type of export is determined by the drop-down in
  // the file picker dialog.
  PRInt32 exportType;
  rv = filePicker->GetFilterIndex(&exportType);
  NS_ENSURE_SUCCESS(rv,rv);

  nsAutoString fileName;
  rv = localFile->GetLeafName(fileName);
  NS_ENSURE_SUCCESS(rv, rv);
    
  switch ( exportType )
  {
    default:
    case LDIF_EXPORT_TYPE: // ldif
      // If filename does not have the correct ext, add one.
      if ((fileName.RFind(LDIF_FILE_EXTENSION, PR_TRUE, -1, sizeof(LDIF_FILE_EXTENSION)-1) == kNotFound) &&
       (fileName.RFind(LDIF_FILE_EXTENSION2, PR_TRUE, -1, sizeof(LDIF_FILE_EXTENSION2)-1) == kNotFound)) {

       // Add the extenstion and build a new localFile.
       fileName.AppendLiteral(LDIF_FILE_EXTENSION2);
       localFile->SetLeafName(fileName);
    }
      rv = ExportDirectoryToLDIF(aDirectory, localFile);
      break;

    case CSV_EXPORT_TYPE: // csv
      // If filename does not have the correct ext, add one.
      if (fileName.RFind(CSV_FILE_EXTENSION, PR_TRUE, -1, sizeof(CSV_FILE_EXTENSION)-1) == kNotFound) {

       // Add the extenstion and build a new localFile.
       fileName.AppendLiteral(CSV_FILE_EXTENSION);
       localFile->SetLeafName(fileName);
    }
      rv = ExportDirectoryToDelimitedText(aDirectory, CSV_DELIM, CSV_DELIM_LEN, localFile);
      break;

    case TAB_EXPORT_TYPE: // tab & text
      // If filename does not have the correct ext, add one.
      if ( (fileName.RFind(TXT_FILE_EXTENSION, PR_TRUE, -1, sizeof(TXT_FILE_EXTENSION)-1) == kNotFound) &&
          (fileName.RFind(TAB_FILE_EXTENSION, PR_TRUE, -1, sizeof(TAB_FILE_EXTENSION)-1) == kNotFound) ) {

       // Add the extenstion and build a new localFile.
       fileName.AppendLiteral(TXT_FILE_EXTENSION);
       localFile->SetLeafName(fileName);
  }
      rv = ExportDirectoryToDelimitedText(aDirectory, TAB_DELIM, TAB_DELIM_LEN, localFile);
      break;
  };
 
  return rv;
}

nsresult
nsAddressBook::ExportDirectoryToDelimitedText(nsIAbDirectory *aDirectory, const char *aDelim, PRUint32 aDelimLen, nsILocalFile *aLocalFile)
{
  nsCOMPtr <nsISimpleEnumerator> cardsEnumerator;
  nsCOMPtr <nsIAbCard> card;

  nsresult rv;

  nsCOMPtr <nsIImportService> importService = do_GetService(NS_IMPORTSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv,rv);

  nsCOMPtr <nsIOutputStream> outputStream;
  rv = NS_NewLocalFileOutputStream(getter_AddRefs(outputStream),
                                   aLocalFile,
                                   PR_CREATE_FILE | PR_WRONLY | PR_TRUNCATE,
                                   0664);

  // the desired file may be read only
  if (NS_FAILED(rv))
    return rv;

  PRUint32 i;
  PRUint32 writeCount;
  PRUint32 length;

  nsCOMPtr<nsIStringBundleService> bundleService = do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIStringBundle> bundle;
  rv = bundleService->CreateBundle("chrome://messenger/locale/importMsgs.properties", getter_AddRefs(bundle));
  NS_ENSURE_SUCCESS(rv, rv);

  nsXPIDLCString revisedName;
  nsXPIDLString columnName;

  for (i = 0; i < EXPORT_ATTRIBUTES_TABLE_COUNT; i++) {
    if (EXPORT_ATTRIBUTES_TABLE[i].plainTextStringID != 0) {

      // We don't need to truncate the string here as getter_Copies will
      // do that for us.
      if (NS_FAILED(bundle->GetStringFromID(EXPORT_ATTRIBUTES_TABLE[i].plainTextStringID, getter_Copies(columnName))))
        columnName.AppendInt(EXPORT_ATTRIBUTES_TABLE[i].plainTextStringID);

      importService->SystemStringFromUnicode(columnName.get(), revisedName);

      rv = outputStream->Write(revisedName.get(),
                               revisedName.Length(),
                               &writeCount);
      NS_ENSURE_SUCCESS(rv,rv);

      if (revisedName.Length() != writeCount)
        return NS_ERROR_FAILURE;
      
      if (i < EXPORT_ATTRIBUTES_TABLE_COUNT - 1) {
        rv = outputStream->Write(aDelim, aDelimLen, &writeCount);
        NS_ENSURE_SUCCESS(rv,rv);

        if (aDelimLen != writeCount)
          return NS_ERROR_FAILURE;
      }
    }
  }
  rv = outputStream->Write(NS_LINEBREAK, NS_LINEBREAK_LEN, &writeCount);
  NS_ENSURE_SUCCESS(rv,rv);
  if (NS_LINEBREAK_LEN != writeCount)
    return NS_ERROR_FAILURE;

  rv = aDirectory->GetChildCards(getter_AddRefs(cardsEnumerator));
  if (NS_SUCCEEDED(rv) && cardsEnumerator) {
    nsCOMPtr<nsISupports> item;
    PRBool more;
    while (NS_SUCCEEDED(cardsEnumerator->HasMoreElements(&more)) && more) {
      rv = cardsEnumerator->GetNext(getter_AddRefs(item));
      if (NS_SUCCEEDED(rv)) {
        nsCOMPtr <nsIAbCard> card = do_QueryInterface(item, &rv);
        NS_ENSURE_SUCCESS(rv,rv);
         
        PRBool isMailList;
        rv = card->GetIsMailList(&isMailList);
        NS_ENSURE_SUCCESS(rv,rv);
       
         
        if (isMailList) {
          // .tab, .txt and .csv aren't able to export mailing lists
          // use LDIF for that.
        }
        else {
          nsXPIDLString value;          
          nsCString valueCStr;

          for (i = 0; i < EXPORT_ATTRIBUTES_TABLE_COUNT; i++) {
            if (EXPORT_ATTRIBUTES_TABLE[i].plainTextStringID != 0) {
              rv = card->GetCardValue(EXPORT_ATTRIBUTES_TABLE[i].abColName, getter_Copies(value));
              NS_ENSURE_SUCCESS(rv,rv);

              // If a string contains at least one comma, tab or double quote then
              // we need to quote the entire string. Also if double quote is part
              // of the string we need to quote the double quote(s) as well.  
              nsAutoString newValue(value);
              PRBool needsQuotes = PR_FALSE;
              if(newValue.FindChar('"') != kNotFound)
              {
                needsQuotes = PR_TRUE;
                newValue.ReplaceSubstring(NS_LITERAL_STRING("\"").get(), NS_LITERAL_STRING("\"\"").get());
              }
              if (!needsQuotes && (newValue.FindChar(',') != kNotFound || newValue.FindChar('\x09') != kNotFound))
                needsQuotes = PR_TRUE;

              // Make sure we quote if containing CR/LF.
              if (newValue.FindChar(nsCRT::CR) != kNotFound ||
                  newValue.FindChar(nsCRT::LF) != kNotFound)
                  needsQuotes = PR_TRUE;

              if (needsQuotes)
              {
                newValue.Insert(NS_LITERAL_STRING("\""), 0);
                newValue.AppendLiteral("\"");
              }

              rv = importService->SystemStringFromUnicode(newValue.get(), valueCStr);

              if (NS_FAILED(rv)) {
                NS_ASSERTION(0, "failed to convert string to system charset.  use LDIF");
                valueCStr = "?";
              }
              
              length = valueCStr.Length();
              if (length) {
                rv = outputStream->Write(valueCStr.get(), length, &writeCount);
                NS_ENSURE_SUCCESS(rv,rv);
                if (length != writeCount)
                  return NS_ERROR_FAILURE;
              }
              valueCStr = "";
            }
            else {
              // something we don't support for the current export
              // for example, .tab doesn't export preferred html format
              continue; // go to next field
            }

            if (i < EXPORT_ATTRIBUTES_TABLE_COUNT - 1) {
              rv = outputStream->Write(aDelim, aDelimLen, &writeCount);
              NS_ENSURE_SUCCESS(rv,rv);
              if (aDelimLen != writeCount)
                return NS_ERROR_FAILURE;
            }
          }

          // write out the linebreak that separates the cards
          rv = outputStream->Write(NS_LINEBREAK, NS_LINEBREAK_LEN, &writeCount);
          NS_ENSURE_SUCCESS(rv,rv);
          if (NS_LINEBREAK_LEN != writeCount)
            return NS_ERROR_FAILURE;
        }
      }
    }
  }

  rv = outputStream->Flush();
  NS_ENSURE_SUCCESS(rv,rv);

  rv = outputStream->Close();
  NS_ENSURE_SUCCESS(rv,rv);
  return NS_OK;
}

nsresult
nsAddressBook::ExportDirectoryToLDIF(nsIAbDirectory *aDirectory, nsILocalFile *aLocalFile)
{
  nsCOMPtr <nsISimpleEnumerator> cardsEnumerator;
  nsCOMPtr <nsIAbCard> card;

  nsresult rv;

  nsCOMPtr <nsIOutputStream> outputStream;
  rv = NS_NewLocalFileOutputStream(getter_AddRefs(outputStream),
                                   aLocalFile,
                                   PR_CREATE_FILE | PR_WRONLY | PR_TRUNCATE,
                                   0664);

  // the desired file may be read only
  if (NS_FAILED(rv))
    return rv;

  // Get the default attribute map for ldap. We use the default attribute
  // map rather than one for a specific server because if people want an
  // ldif export using a servers specific schema, then they can use ldapsearch
  nsCOMPtr<nsIAbLDAPAttributeMapService> mapSrv = 
    do_GetService("@mozilla.org/addressbook/ldap-attribute-map-service;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIAbLDAPAttributeMap> attrMap;
  rv = mapSrv->GetMapForPrefBranch(NS_LITERAL_CSTRING("ldap_2.servers.default.attrmap"),
                                   getter_AddRefs(attrMap));
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint32 i;
  PRUint32 writeCount;
  PRUint32 length;
   
  rv = aDirectory->GetChildCards(getter_AddRefs(cardsEnumerator));
  if (NS_SUCCEEDED(rv) && cardsEnumerator) {
    nsCOMPtr<nsISupports> item;
    PRBool more;
    while (NS_SUCCEEDED(cardsEnumerator->HasMoreElements(&more)) && more) {
      rv = cardsEnumerator->GetNext(getter_AddRefs(item));
      if (NS_SUCCEEDED(rv)) {
        nsCOMPtr <nsIAbCard> card = do_QueryInterface(item, &rv);
        NS_ENSURE_SUCCESS(rv,rv);
         
        PRBool isMailList;
        rv = card->GetIsMailList(&isMailList);
        NS_ENSURE_SUCCESS(rv,rv);
       
        if (isMailList) {
          nsCString mailListCStr;

          rv = AppendLDIFForMailList(card, attrMap, mailListCStr);
          NS_ENSURE_SUCCESS(rv,rv);

          length = mailListCStr.Length();
          rv = outputStream->Write(mailListCStr.get(), length, &writeCount);
          NS_ENSURE_SUCCESS(rv,rv);
          if (length != writeCount)
            return NS_ERROR_FAILURE;
        }
        else {
          nsXPIDLString value;          
          nsCString valueCStr;
    
          rv = AppendBasicLDIFForCard(card, attrMap, valueCStr);
          NS_ENSURE_SUCCESS(rv,rv);
          
          length = valueCStr.Length();
          rv = outputStream->Write(valueCStr.get(), length, &writeCount);
          NS_ENSURE_SUCCESS(rv,rv);
          if (length != writeCount)
            return NS_ERROR_FAILURE;
          
          valueCStr.Truncate();

          nsCAutoString ldapAttribute;

          for (i = 0; i < EXPORT_ATTRIBUTES_TABLE_COUNT; i++) {
            if (NS_SUCCEEDED(attrMap->GetFirstAttribute(nsDependentCString(EXPORT_ATTRIBUTES_TABLE[i].abColName),
                                                        ldapAttribute) &&
                !ldapAttribute.IsEmpty())) {

              rv = card->GetCardValue(EXPORT_ATTRIBUTES_TABLE[i].abColName, getter_Copies(value));
              NS_ENSURE_SUCCESS(rv,rv);
 
              if (!PL_strcmp(EXPORT_ATTRIBUTES_TABLE[i].abColName, kPreferMailFormatColumn)) {
                if (value.Equals(NS_LITERAL_STRING("html").get()))
                  value.AssignLiteral("true");
                else if (value.Equals(NS_LITERAL_STRING("plaintext").get()))
                  value.AssignLiteral("false");
                else
                  value.Truncate(); // unknown.
              }

              if (!value.IsEmpty()) {
                rv = AppendProperty(ldapAttribute.get(), value.get(), valueCStr);
                NS_ENSURE_SUCCESS(rv,rv);
                
                valueCStr += NS_LINEBREAK;
              }
              else
                valueCStr.Truncate();
              
              length = valueCStr.Length();
              if (length) {
                rv = outputStream->Write(valueCStr.get(), length, &writeCount);
                NS_ENSURE_SUCCESS(rv,rv);
                if (length != writeCount)
                  return NS_ERROR_FAILURE;
              }
              valueCStr.Truncate();
            }
            else {
              // something we don't support yet
              // ldif doesn't export mutliple addresses
            }
          }
        
          // write out the linebreak that separates the cards
          rv = outputStream->Write(NS_LINEBREAK, NS_LINEBREAK_LEN, &writeCount);
          NS_ENSURE_SUCCESS(rv,rv);
          if (NS_LINEBREAK_LEN != writeCount)
            return NS_ERROR_FAILURE;
        }
      }
    }
  }

  rv = outputStream->Flush();
  NS_ENSURE_SUCCESS(rv,rv);

  rv = outputStream->Close();
  NS_ENSURE_SUCCESS(rv,rv);
  return NS_OK;
}

nsresult nsAddressBook::AppendLDIFForMailList(nsIAbCard *aCard, nsIAbLDAPAttributeMap *aAttrMap, nsACString &aResult)
{
  nsresult rv;
  nsXPIDLString attrValue;
  
  rv = AppendDNForCard("dn", aCard, aAttrMap, aResult);
  NS_ENSURE_SUCCESS(rv,rv);

  aResult += NS_LINEBREAK \
             "objectclass: top" NS_LINEBREAK \
             "objectclass: groupOfNames" NS_LINEBREAK;

  rv = aCard->GetCardValue(kDisplayNameColumn, getter_Copies(attrValue));
  NS_ENSURE_SUCCESS(rv,rv);

  nsCAutoString ldapAttributeName;

  rv = aAttrMap->GetFirstAttribute(NS_LITERAL_CSTRING(kDisplayNameColumn),
                                  ldapAttributeName);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = AppendProperty(ldapAttributeName.get(), attrValue.get(), aResult);
  NS_ENSURE_SUCCESS(rv,rv);
  aResult += NS_LINEBREAK;

  rv = aAttrMap->GetFirstAttribute(NS_LITERAL_CSTRING(kNicknameColumn),
                                  ldapAttributeName);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aCard->GetCardValue(kNicknameColumn, getter_Copies(attrValue));
  NS_ENSURE_SUCCESS(rv,rv);

  if (!attrValue.IsEmpty()) {
    rv = AppendProperty(ldapAttributeName.get(), attrValue.get(), aResult);
    NS_ENSURE_SUCCESS(rv,rv);
    aResult += NS_LINEBREAK;
  }

  rv = aAttrMap->GetFirstAttribute(NS_LITERAL_CSTRING(kNotesColumn),
                                  ldapAttributeName);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aCard->GetCardValue(kNotesColumn, getter_Copies(attrValue));
  NS_ENSURE_SUCCESS(rv,rv);

  if (!attrValue.IsEmpty()) {
    rv = AppendProperty(ldapAttributeName.get(), attrValue.get(), aResult);
    NS_ENSURE_SUCCESS(rv,rv);
    aResult += NS_LINEBREAK;
  }

  nsCOMPtr<nsIRDFService> rdfService = do_GetService("@mozilla.org/rdf/rdf-service;1", &rv);
  NS_ENSURE_SUCCESS(rv,rv);
  
  nsXPIDLCString mailListURI;
  rv = aCard->GetMailListURI(getter_Copies(mailListURI));
  NS_ENSURE_SUCCESS(rv,rv);
  
  nsCOMPtr <nsIRDFResource> resource;
  rv = rdfService->GetResource(mailListURI, getter_AddRefs(resource));
  NS_ENSURE_SUCCESS(rv,rv);
    
  nsCOMPtr <nsIAbDirectory> mailList = do_QueryInterface(resource, &rv);
  NS_ENSURE_SUCCESS(rv,rv);
    
  nsCOMPtr<nsISupportsArray> addresses;
  rv = mailList->GetAddressLists(getter_AddRefs(addresses));
  if (addresses) {
    PRUint32 total = 0;
    addresses->Count(&total);
    if (total) {
      PRUint32 i;
      for (i = 0; i < total; i++) {
        nsCOMPtr <nsIAbCard> listCard = do_QueryElementAt(addresses, i, &rv);
        NS_ENSURE_SUCCESS(rv,rv);

        rv = AppendDNForCard("member", listCard, aAttrMap, aResult);
        NS_ENSURE_SUCCESS(rv,rv);

        aResult += NS_LINEBREAK;
      }
    }
  }
  
  aResult += NS_LINEBREAK;
  return NS_OK;
}

nsresult nsAddressBook::AppendDNForCard(const char *aProperty, nsIAbCard *aCard, nsIAbLDAPAttributeMap *aAttrMap, nsACString &aResult)
{
  nsXPIDLString email;
  nsXPIDLString displayName;
  nsCAutoString ldapAttributeName;

  nsresult rv = aCard->GetCardValue(kPriEmailColumn, getter_Copies(email));
  NS_ENSURE_SUCCESS(rv,rv);

  rv = aCard->GetCardValue(kDisplayNameColumn, getter_Copies(displayName));
  NS_ENSURE_SUCCESS(rv,rv);

  nsString cnStr;

  rv = aAttrMap->GetFirstAttribute(NS_LITERAL_CSTRING(kDisplayNameColumn),
                                   ldapAttributeName);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!displayName.IsEmpty()) {
    cnStr += NS_ConvertUTF8toUTF16(ldapAttributeName).get();
    cnStr += NS_LITERAL_STRING("=") + displayName;
    if (!email.IsEmpty()) {
      cnStr.AppendLiteral(",");
    }
  }

  rv = aAttrMap->GetFirstAttribute(NS_LITERAL_CSTRING(kPriEmailColumn),
                                   ldapAttributeName);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!email.IsEmpty()) {
    cnStr += NS_ConvertUTF8toUTF16(ldapAttributeName).get();
    cnStr += NS_LITERAL_STRING("=") + email;
  }

  rv = AppendProperty(aProperty, cnStr.get(), aResult);
  NS_ENSURE_SUCCESS(rv,rv);
  return rv;
}

nsresult nsAddressBook::AppendBasicLDIFForCard(nsIAbCard *aCard, nsIAbLDAPAttributeMap *aAttrMap, nsACString &aResult)
{
  nsresult rv = AppendDNForCard("dn", aCard, aAttrMap, aResult);
  NS_ENSURE_SUCCESS(rv,rv);
  aResult += NS_LINEBREAK \
    "objectclass: top" NS_LINEBREAK \
    "objectclass: person" NS_LINEBREAK \
    "objectclass: organizationalPerson" NS_LINEBREAK \
    "objectclass: inetOrgPerson" NS_LINEBREAK \
    "objectclass: " MOZ_AB_OBJECTCLASS NS_LINEBREAK;  


  return rv;
}

PRBool nsAddressBook::IsSafeLDIFString(const PRUnichar *aStr)
{
  // follow RFC 2849 to determine if something is safe "as is" for LDIF
  if (aStr[0] == PRUnichar(' ') ||
      aStr[0] == PRUnichar(':') ||
      aStr[0] == PRUnichar('<')) 
    return PR_FALSE;

  PRUint32 i;
  PRUint32 len = nsCRT::strlen(aStr);
  for (i=0; i<len; i++) {
    // If string contains CR or LF, it is not safe for LDIF
    // and MUST be base64 encoded
    if ((aStr[i] == PRUnichar('\n')) ||
        (aStr[i] == PRUnichar('\r')) ||
        (!nsCRT::IsAscii(aStr[i])))
      return PR_FALSE;
  }
  return PR_TRUE;
}

nsresult nsAddressBook::AppendProperty(const char *aProperty, const PRUnichar *aValue, nsACString &aResult)
{
  NS_ENSURE_ARG_POINTER(aValue);

  aResult += aProperty;
 
  // if the string is not safe "as is", base64 encode it
  if (IsSafeLDIFString(aValue)) {
    aResult.AppendLiteral(": ");
    LossyAppendUTF16toASCII(aValue, aResult);
  }
  else {
    char *base64Str = PL_Base64Encode(NS_ConvertUTF16toUTF8(aValue).get(), 0, nsnull);
    if (!base64Str)
      return NS_ERROR_OUT_OF_MEMORY;

    aResult += NS_LITERAL_CSTRING(":: ") + nsDependentCString(base64Str);
    PR_Free(base64Str);
  }

  return NS_OK;
}

char *getCString(VObject *vObj)
{
    if (VALUE_TYPE(vObj) == VCVT_USTRINGZ)
        return fakeCString(vObjectUStringZValue(vObj));
    if (VALUE_TYPE(vObj) == VCVT_STRINGZ)
        return PL_strdup(vObjectStringZValue(vObj));
    return NULL;
}

static void convertNameValue(VObject *vObj, nsIAbCard *aCard)
{
  const char *cardColName = NULL;

  // if the vCard property is not a root property then we need to determine its exact property.
  // a good example of this is VCTelephoneProp, this prop has four objects underneath it:
  // fax, work and home and cellular.
  if (PL_strcasecmp(VCCityProp, vObjectName(vObj)) == 0)
      cardColName = kWorkCityColumn;
  else if (PL_strcasecmp(VCTelephoneProp, vObjectName(vObj)) == 0)
  {
      if (isAPropertyOf(vObj, VCFaxProp))
          cardColName = kFaxColumn;
      else if (isAPropertyOf(vObj, VCWorkProp))
          cardColName = kWorkPhoneColumn; 
      else if (isAPropertyOf(vObj, VCHomeProp))
          cardColName = kHomePhoneColumn;
      else if (isAPropertyOf(vObj, VCCellularProp))
          cardColName = kCellularColumn;
      else if (isAPropertyOf(vObj, VCPagerProp))
          cardColName = kPagerColumn;
      else
          return;
  }
  else if (PL_strcasecmp(VCEmailAddressProp, vObjectName(vObj)) == 0)
      cardColName = kPriEmailColumn;
  else if (PL_strcasecmp(VCFamilyNameProp, vObjectName(vObj)) == 0) 
      cardColName = kLastNameColumn;
  else if (PL_strcasecmp(VCFullNameProp, vObjectName(vObj)) == 0)
      cardColName = kDisplayNameColumn;
  else if (PL_strcasecmp(VCGivenNameProp, vObjectName(vObj)) == 0)
      cardColName = kFirstNameColumn;
  else if (PL_strcasecmp(VCOrgNameProp, vObjectName(vObj)) == 0) 
      cardColName = kCompanyColumn;
  else if (PL_strcasecmp(VCOrgUnitProp, vObjectName(vObj)) == 0)
      cardColName = kDepartmentColumn;
  else if (PL_strcasecmp(VCPostalCodeProp, vObjectName(vObj)) == 0) 
      cardColName = kWorkZipCodeColumn;
  else if (PL_strcasecmp(VCRegionProp, vObjectName(vObj)) == 0)
      cardColName = kWorkStateColumn;
  else if (PL_strcasecmp(VCStreetAddressProp, vObjectName(vObj)) == 0)
      cardColName = kWorkAddressColumn;
  else if (PL_strcasecmp(VCPostalBoxProp, vObjectName(vObj)) == 0) 
      cardColName = kWorkAddress2Column;
  else if (PL_strcasecmp(VCCountryNameProp, vObjectName(vObj)) == 0)
      cardColName = kWorkCountryColumn;
  else if (PL_strcasecmp(VCTitleProp, vObjectName(vObj)) == 0) 
      cardColName = kJobTitleColumn;
  else if (PL_strcasecmp(VCUseHTML, vObjectName(vObj)) == 0) 
      cardColName = kPreferMailFormatColumn;
  else if (PL_strcasecmp(VCNoteProp, vObjectName(vObj)) == 0) 
      cardColName = kNotesColumn;
  else if (PL_strcasecmp(VCURLProp, vObjectName(vObj)) == 0)
      cardColName = kWebPage1Column;
  else
      return;
  
  if (!VALUE_TYPE(vObj))
      return;

  char *cardColValue = getCString(vObj);
  aCard->SetCardValue(cardColName, NS_ConvertUTF8toUTF16(cardColValue).get());
  PR_FREEIF(cardColValue);
  return;
}

static void convertFromVObject(VObject *vObj, nsIAbCard *aCard)
{
    if (vObj)
    {
        VObjectIterator t;

        convertNameValue(vObj, aCard);
        
        initPropIterator(&t, vObj);
        while (moreIteration(&t))
        {
            VObject * nextObject = nextVObject(&t);
            convertFromVObject(nextObject, aCard);
        }
    }
    return;
}

NS_IMETHODIMP nsAddressBook::HandleContent(const char * aContentType,
                                           nsIInterfaceRequestor * aWindowContext,
                                           nsIRequest *request)
{
  NS_ENSURE_ARG_POINTER(request);
  
  nsresult rv = NS_OK;

  // First of all, get the content type and make sure it is a content type we know how to handle!
  if (nsCRT::strcasecmp(aContentType, "application/x-addvcard") == 0) {
    nsCOMPtr<nsIURI> uri;
    nsCOMPtr<nsIChannel> aChannel = do_QueryInterface(request);
    if (!aChannel) return NS_ERROR_FAILURE;

    rv = aChannel->GetURI(getter_AddRefs(uri));
    if (uri)
    {
        nsCAutoString path;
        rv = uri->GetPath(path);
        NS_ENSURE_SUCCESS(rv,rv);

        const char *startOfVCard = strstr(path.get(), "add?vcard=");
        if (startOfVCard)
        {
            char *unescapedData = PL_strdup(startOfVCard + strlen("add?vcard="));
            
            // XXX todo, explain why we is escaped twice
            nsUnescape(unescapedData);
            
            if (!aWindowContext) 
                return NS_ERROR_FAILURE;

            nsCOMPtr<nsIDOMWindowInternal> parentWindow = do_GetInterface(aWindowContext);
            if (!parentWindow) 
                return NS_ERROR_FAILURE;
            
            nsCOMPtr <nsIAbCard> cardFromVCard;
            rv = EscapedVCardToAbCard((const char *)unescapedData, getter_AddRefs(cardFromVCard));
            NS_ENSURE_SUCCESS(rv, rv);

            nsCOMPtr<nsISupportsInterfacePointer> ifptr =
                do_CreateInstance(NS_SUPPORTS_INTERFACE_POINTER_CONTRACTID, &rv);
            NS_ENSURE_SUCCESS(rv, rv);
            
            ifptr->SetData(cardFromVCard);
            ifptr->SetDataIID(&NS_GET_IID(nsIAbCard));

            nsCOMPtr<nsIDOMWindow> dialogWindow;

            rv = parentWindow->OpenDialog(
                NS_LITERAL_STRING("chrome://messenger/content/addressbook/abNewCardDialog.xul"),
                EmptyString(),
                NS_LITERAL_STRING("chrome,resizable=no,titlebar,modal,centerscreen"),
                ifptr, getter_AddRefs(dialogWindow));
            NS_ENSURE_SUCCESS(rv, rv);

            PL_strfree(unescapedData);
        }
        rv = NS_OK;
    }
  } 
  else if (nsCRT::strcasecmp(aContentType, "text/x-vcard") == 0) {
    // create a vcard stream listener that can parse the data stream
    // and bring up the appropriate UI

    // (1) cancel the current load operation. We'll restart it
    request->Cancel(NS_ERROR_ABORT); 
    // get the url we were trying to open
    nsCOMPtr<nsIURI> uri;
    nsCOMPtr<nsIChannel> channel = do_QueryInterface(request);
    NS_ENSURE_TRUE(channel, NS_ERROR_FAILURE);

    rv = channel->GetURI(getter_AddRefs(uri));
    NS_ENSURE_SUCCESS(rv, rv);

    // create a stream loader to handle the v-card data
    nsCOMPtr<nsIStreamLoader> streamLoader;
    rv = NS_NewStreamLoader(getter_AddRefs(streamLoader), uri, this, aWindowContext);
    NS_ENSURE_SUCCESS(rv, rv);

  }
  else // The content-type was not application/x-addvcard...
    return NS_ERROR_WONT_HANDLE_CONTENT;

  return rv;
}

NS_IMETHODIMP nsAddressBook::OnStreamComplete(nsIStreamLoader *aLoader, nsISupports *aContext, nsresult aStatus, PRUint32 datalen, const PRUint8 *data)
{
  NS_ENSURE_ARG_POINTER(aContext);
  NS_ENSURE_SUCCESS(aStatus, aStatus); // don't process the vcard if we got a status error
  nsresult rv = NS_OK;

  // take our vCard string and open up an address book window based on it
  nsCOMPtr<nsIMsgVCardService> vCardService = do_GetService(NS_MSGVCARDSERVICE_CONTRACTID);
  if (vCardService)
  {
    nsAutoPtr<VObject> vObj(vCardService->Parse_MIME((const char *)data, datalen));
    if (vObj)
    {
      PRInt32 len = 0;
      nsAdoptingCString vCard(vCardService->WriteMemoryVObjects(0, &len, vObj, PR_FALSE));

      nsCOMPtr <nsIAbCard> cardFromVCard;
      rv = EscapedVCardToAbCard(vCard.get(), getter_AddRefs(cardFromVCard));
      NS_ENSURE_SUCCESS(rv, rv);

      nsCOMPtr<nsIDOMWindowInternal> parentWindow = do_GetInterface(aContext);
      NS_ENSURE_TRUE(parentWindow, NS_ERROR_FAILURE);

      nsCOMPtr<nsIDOMWindow> dialogWindow;
      rv = parentWindow->OpenDialog(
           NS_LITERAL_STRING("chrome://messenger/content/addressbook/abNewCardDialog.xul"),
           EmptyString(),
           NS_LITERAL_STRING("chrome,resizable=no,titlebar,modal,centerscreen"),
           cardFromVCard, getter_AddRefs(dialogWindow));      
    }
  }

  return rv;
}

NS_IMETHODIMP nsAddressBook::EscapedVCardToAbCard(const char *aEscapedVCardStr, nsIAbCard **aCard)
{
    NS_ENSURE_ARG_POINTER(aEscapedVCardStr);
    NS_ENSURE_ARG_POINTER(aCard);
    
    nsCOMPtr <nsIAbCard> cardFromVCard = do_CreateInstance(NS_ABCARDPROPERTY_CONTRACTID);
    if (!cardFromVCard)
        return NS_ERROR_FAILURE;

    // aEscapedVCardStr will be "" the first time, before you have a vCard
    if (*aEscapedVCardStr != '\0') {   
        char *unescapedData = PL_strdup(aEscapedVCardStr);
        if (!unescapedData)
            return NS_ERROR_OUT_OF_MEMORY;
        
        nsUnescape(unescapedData);
        VObject *vObj = parse_MIME(unescapedData, strlen(unescapedData));
        PL_strfree(unescapedData);
        NS_ASSERTION(vObj, "Parse of vCard failed");

        convertFromVObject(vObj, cardFromVCard);
        
        if (vObj)
            cleanVObject(vObj);
    }
    
    NS_IF_ADDREF(*aCard = cardFromVCard);
    return NS_OK;
}

NS_IMETHODIMP nsAddressBook::AbCardToEscapedVCard(nsIAbCard *aCard, char **aEscapedVCardStr)
{
    NS_ENSURE_ARG_POINTER(aCard);
    NS_ENSURE_ARG_POINTER(aEscapedVCardStr);
    
    nsresult rv = aCard->ConvertToEscapedVCard(aEscapedVCardStr);
    NS_ENSURE_SUCCESS(rv,rv);
    return rv;
}

static nsresult addProperty(char **currentVCard, const char *currentRoot, const char *mask)
{
    // keep in mind as we add properties that we want to filter out any begin and end vcard types....because
    // we add those automatically...
    
    const char *beginPhrase = "begin";
    
    nsCOMPtr<nsIPrefBranch> prefBranch = do_GetService(NS_PREFSERVICE_CONTRACTID);
    if (currentVCard && prefBranch)
    {
        PRUint32 childCount;
        char **childArray;
        nsresult rv = prefBranch->GetChildList(currentRoot, &childCount, &childArray);
        NS_ENSURE_SUCCESS(rv, rv);
        
        for (PRUint32 i = 0; i < childCount; ++i) 
        {
            char *child = childArray[i];

            if (!strcmp(child, currentRoot))
                continue;
                
            // first iterate over the child in case the child has children    
            addProperty(currentVCard, child, mask);
               
            // child length should be greater than the mask....
            if (strlen(child) > strlen(mask) + 1)  // + 1 for the '.' in .property
            {
                nsXPIDLCString value;
                prefBranch->GetCharPref(child, getter_Copies(value));
                if (mask)
                    child += strlen(mask) + 1;  // eat up the "mail.identity.vcard" part...
                // turn all '.' into ';' which is what vcard format uses
                char * marker = strchr(child, '.');
                while (marker)
                {
                    *marker = ';';
                    marker = strchr(child, '.');
                }
                
                // filter property to make sure it is one we want to add.....
                if ((PL_strncasecmp(child, beginPhrase, strlen(beginPhrase)) != 0) && (PL_strncasecmp(child, VCEndProp, strlen(VCEndProp)) != 0))
                {
                    if (!value.IsEmpty()) // only add the value is not an empty string...
                        if (*currentVCard)
                        {
                            char * tempString = *currentVCard;
                            *currentVCard = PR_smprintf ("%s%s:%s%s", tempString, child, value.get(), "\n");
                            PR_FREEIF(tempString);
                        }
                        else
                            *currentVCard = PR_smprintf ("%s:%s%s", child, value.get(), "\n");
                }
            }
            else {
                NS_ASSERTION(0, "child length should be greater than the mask");
            }
        }     
        NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(childCount, childArray);
    } 
    return NS_OK;
}

NS_IMETHODIMP nsAddressBook::Convert4xVCardPrefs(const char *prefRoot, char **escapedVCardStr)
{
    NS_ENSURE_ARG_POINTER(prefRoot);
    NS_ENSURE_ARG_POINTER(escapedVCardStr);
    
    char *vCardString = nsnull;
    vCardString = PL_strdup("begin:vcard \n");
    
    nsresult rv = addProperty(&vCardString, prefRoot, prefRoot);
    NS_ENSURE_SUCCESS(rv,rv);
    
    char *vcard = PR_smprintf("%send:vcard\n", vCardString);
    PR_FREEIF(vCardString);
    
    VObject *vObj = parse_MIME(vcard, strlen(vcard));
    PR_FREEIF(vcard);
    
    nsCOMPtr<nsIAbCard> cardFromVCard = do_CreateInstance(NS_ABCARDPROPERTY_CONTRACTID);
    convertFromVObject(vObj, cardFromVCard);
    
    if (vObj)
        cleanVObject(vObj);
    
    rv = cardFromVCard->ConvertToEscapedVCard(escapedVCardStr);
    NS_ENSURE_SUCCESS(rv,rv);
    return rv;
}

#ifdef MOZ_XUL_APP

NS_IMETHODIMP
nsAddressBook::Handle(nsICommandLine* aCmdLine)
{
  nsresult rv; 
  PRBool found;

  rv = aCmdLine->HandleFlag(NS_LITERAL_STRING("addressbook"), PR_FALSE, &found);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!found)
    return NS_OK;

  nsCOMPtr<nsIWindowWatcher> wwatch (do_GetService(NS_WINDOWWATCHER_CONTRACTID));
  NS_ENSURE_TRUE(wwatch, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDOMWindow> opened;
  wwatch->OpenWindow(nsnull, "chrome://messenger/content/addressbook/addressbook.xul",
                     "_blank", "chrome,dialog=no,all", nsnull, getter_AddRefs(opened));
  aCmdLine->SetPreventDefault(PR_TRUE);
  return NS_OK;
}

NS_IMETHODIMP
nsAddressBook::GetHelpInfo(nsACString& aResult)
{
  aResult.Assign(NS_LITERAL_CSTRING("  -addressbook         Open the address book at startup.\n"));
  return NS_OK;
}

#else // !MOZ_XUL_APP

CMDLINEHANDLER_IMPL(nsAddressBook,"-addressbook","general.startup.addressbook","chrome://messenger/content/addressbook/addressbook.xul","Start with the addressbook.",NS_ADDRESSBOOKSTARTUPHANDLER_CONTRACTID,"Addressbook Startup Handler",PR_FALSE,"", PR_TRUE)

#endif
