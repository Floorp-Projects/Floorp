/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
#include "nsAddrDatabase.h"
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
#include "nsAppShellCIDs.h"
#include "nsIAppShellService.h"
#include "nsIDOMWindowInternal.h"
#include "nsIContentViewer.h"
#include "nsIDocShell.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsICategoryManager.h"
#include "nsIAbUpgrader.h"
#include "nsSpecialSystemDirectory.h"
#include "nsIFilePicker.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsIPrefLocalizedString.h"
#include "nsVoidArray.h"
#include "nsIAbCard.h"
#include "nsIAbMDBCard.h"
#include "plbase64.h"

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

// according to RFC 2849
// SEP = (CR LF / LF)
// so we LF for unix and beos (since that is the natural line ending for
// those platforms, see nsCRT.h)
//
// otherwise we use CR LF (windows linebreak)
#if defined(XP_UNIX) || defined(XP_BEOS)
#define LDIF_LINEBREAK          "\012"
#define LDIF_LINEBREAK_LEN     1
#else
#define LDIF_LINEBREAK          "\015\012"
#define LDIF_LINEBREAK_LEN     2
#endif

// our schema is not fixed yet, but we still want some sort of objectclass
// for now, use obsolete in the class name, hinting that this will change
// see bugs bug #116692 and #118454
#define MOZ_AB_OBJECTCLASS "mozillaAbPersonObsolete"

const ExportAttributesTableStruct EXPORT_ATTRIBUTES_TABLE[EXPORT_ATTRIBUTES_TABLE_COUNT] = { 
  {kFirstNameColumn, "givenName", PR_TRUE},
  {kLastNameColumn, "sn", PR_TRUE},
  {kDisplayNameColumn, "cn", PR_TRUE},
  {kNicknameColumn, "xmozillanickname", PR_TRUE},
  {kPriEmailColumn, "mail", PR_TRUE},
  {k2ndEmailColumn, MOZ_AB_LDIF_PREFIX "SecondEmail", PR_TRUE},
  {kDefaultEmailColumn, MOZ_AB_LDIF_PREFIX "DefaultEmail", PR_FALSE},
  {kCardTypeColumn, MOZ_AB_LDIF_PREFIX "CardType", PR_FALSE},
  {kAimScreenNameColumn, MOZ_AB_LDIF_PREFIX "_AimScreenName", PR_FALSE},
  {kPreferMailFormatColumn, "xmozillausehtmlmail", PR_FALSE},
  {kLastModifiedDateColumn, "modifytimestamp", PR_FALSE},
  {kWorkPhoneColumn, "telephoneNumber", PR_TRUE}, 
  {kWorkPhoneTypeColumn, MOZ_AB_LDIF_PREFIX "WorkPhoneType", PR_FALSE},
  {kHomePhoneColumn, "homePhone", PR_TRUE},
  {kHomePhoneTypeColumn, MOZ_AB_LDIF_PREFIX "HomePhoneType", PR_FALSE},
  {kFaxColumn, "facsimileTelephoneNumber", PR_TRUE},
  {kFaxTypeColumn, MOZ_AB_LDIF_PREFIX "FaxNumberType", PR_FALSE},
  {kPagerColumn, "pager", PR_TRUE},
  {kPagerTypeColumn, MOZ_AB_LDIF_PREFIX "PagerNumberType", PR_FALSE},
  {kCellularColumn, "mobile", PR_TRUE},
  {kCellularTypeColumn, MOZ_AB_LDIF_PREFIX "CellularNumberType", PR_FALSE},
  {kHomeAddressColumn, "homePostalAddress", PR_TRUE},
  {kHomeAddress2Column, MOZ_AB_LDIF_PREFIX "HomePostalAddress2", PR_TRUE},
  {kHomeCityColumn, MOZ_AB_LDIF_PREFIX "HomeLocalityName", PR_TRUE},
  {kHomeStateColumn, MOZ_AB_LDIF_PREFIX "HomeState", PR_TRUE},
  {kHomeZipCodeColumn, MOZ_AB_LDIF_PREFIX "HomePostalCode", PR_TRUE},
  {kHomeCountryColumn, MOZ_AB_LDIF_PREFIX "HomeCountryName", PR_TRUE},
  {kWorkAddressColumn, "postalAddress", PR_TRUE},
  {kWorkAddress2Column, MOZ_AB_LDIF_PREFIX "PostalAddress2", PR_TRUE}, 
  {kWorkCityColumn, "l", PR_TRUE}, 
  {kWorkStateColumn, "st", PR_TRUE}, 
  {kWorkZipCodeColumn, "postalCode", PR_TRUE}, 
  {kWorkCountryColumn, "c", PR_TRUE}, 
  {kJobTitleColumn, "title", PR_TRUE},
  {kDepartmentColumn, "ou", PR_TRUE},
  {kCompanyColumn, "o", PR_TRUE},
  {kWebPage1Column, "workurl", PR_TRUE},
  {kWebPage2Column, "homeurl", PR_TRUE},
  {kBirthYearColumn, nsnull, PR_TRUE}, // unused for now
  {kBirthMonthColumn, nsnull, PR_TRUE}, // unused for now
  {kBirthDayColumn, nsnull, PR_TRUE}, // unused for now
  {kCustom1Column, "custom1", PR_TRUE},
  {kCustom2Column, "custom2", PR_TRUE},
  {kCustom3Column, "custom3", PR_TRUE},
  {kCustom4Column, "custom4", PR_TRUE},
  {kNotesColumn, "description", PR_TRUE},
  {kAnniversaryYearColumn, MOZ_AB_LDIF_PREFIX "AnniversaryYear", PR_FALSE}, 
  {kAnniversaryMonthColumn, MOZ_AB_LDIF_PREFIX "AnniversaryMonth", PR_FALSE}, 
  {kAnniversaryDayColumn, MOZ_AB_LDIF_PREFIX "AnniversaryDay", PR_FALSE}, 
  {kSpouseNameColumn, MOZ_AB_LDIF_PREFIX "SpouseName", PR_FALSE}, 
  {kFamilyNameColumn, MOZ_AB_LDIF_PREFIX "FamilyName", PR_FALSE}, 
  {kDefaultAddressColumn, MOZ_AB_LDIF_PREFIX "DefaultAddress", PR_FALSE}, 
  {kCategoryColumn, MOZ_AB_LDIF_PREFIX "Category", PR_FALSE}, 
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
NS_IMPL_QUERY_INTERFACE4(nsAddressBook, nsIAddressBook, nsICmdLineHandler, nsIContentHandler, nsIStreamLoaderObserver)

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

  nsFileSpec* dbPath;
  rv = abSession->GetUserProfileDirectory(&dbPath);
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
  (*dbPath) += file.get();
  
  nsCOMPtr<nsIAddrDatabase> addrDBFactory = 
    do_GetService(NS_ADDRDATABASE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv,rv);

  rv = addrDBFactory->Open(dbPath, PR_TRUE /* create */, aDB, PR_TRUE);
  delete dbPath;
  NS_ENSURE_SUCCESS(rv,rv);
  
  return NS_OK;
}

nsresult nsAddressBook::GetAbDatabaseFromFile(char* pDbFile, nsIAddrDatabase **db)
{
    nsresult rv = NS_OK;
    nsCOMPtr<nsIAddrDatabase> database;
    if (pDbFile)
    {
        nsFileSpec* dbPath = nsnull;

        nsCOMPtr<nsIAddrBookSession> abSession = 
                 do_GetService(NS_ADDRBOOKSESSION_CONTRACTID, &rv); 
        if(NS_SUCCEEDED(rv))
            abSession->GetUserProfileDirectory(&dbPath);
        
        nsCAutoString file(pDbFile);
        (*dbPath) += file.get();

        nsCOMPtr<nsIAddrDatabase> addrDBFactory = 
                 do_GetService(NS_ADDRDATABASE_CONTRACTID, &rv);
        if (NS_SUCCEEDED(rv) && addrDBFactory)
            rv = addrDBFactory->Open(dbPath, PR_TRUE, getter_AddRefs(database), PR_TRUE);

      delete dbPath;

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

class AddressBookParser 
{
protected:

    nsCAutoString    mLine;
    nsCOMPtr <nsIFileSpec> mFileSpec;
    char*            mDbUri;
    nsCOMPtr<nsIAddrDatabase> mDatabase;  
    PRBool mMigrating;
    PRBool mStoreLocAsHome;
    PRBool mDeleteDB;
    PRBool mImportingComm4x;
    PRInt32 mLFCount;
    PRInt32 mCRCount;

    nsresult ParseLDIFFile();
    void AddLdifRowToDatabase(PRBool bIsList);
    void AddLdifColToDatabase(nsIMdbRow* newRow, char* typeSlot, char* valueSlot, PRBool bIsList);
    void ClearLdifRecordBuffer();

    nsresult GetLdifStringRecord(char* buf, PRInt32 len, PRInt32& stopPos);
    nsresult str_parse_line(char *line, char    **type, char **value, int *vlen);
    char * str_getline( char **next );

public:
    AddressBookParser(nsIFileSpec *fileSpec, PRBool migrating, nsIAddrDatabase *db, PRBool bStoreLocAsHome, PRBool bImportingComm4x);
    ~AddressBookParser();

    nsresult ParseFile();
};

AddressBookParser::AddressBookParser(nsIFileSpec * fileSpec, PRBool migrating, nsIAddrDatabase *db, PRBool bStoreLocAsHome, PRBool bImportingComm4x)
{
  mFileSpec = fileSpec;
  mDbUri = nsnull;
  mMigrating = migrating;
  mDatabase = db;
  if (mDatabase)
    mDeleteDB = PR_FALSE;
  else
    mDeleteDB = PR_TRUE;
  mStoreLocAsHome = bStoreLocAsHome;
  mImportingComm4x = bImportingComm4x;
  mLFCount = 0;                                                              
  mCRCount = 0;
}

AddressBookParser::~AddressBookParser(void)
{
    if(mDbUri)
        PR_smprintf_free(mDbUri);
    if (mDatabase && mDeleteDB)
    {
        mDatabase->Close(PR_TRUE);
        mDatabase = nsnull;
    }
}

nsresult AddressBookParser::ParseFile()
{
  // Initialize the parser for a run...
  mLine.Truncate();

  // this is converting 4.x to 6.0
  if (mImportingComm4x && mDatabase) 
  {
    return ParseLDIFFile();
  }
 
  // We are migrating 4.x profile
    /* Get database file name */
    char *leafName = nsnull;
    if (mFileSpec) {
        mFileSpec->GetLeafName(&leafName);

        PRInt32 i = 0;
        while (leafName[i] != '\0')
        {
            if (leafName[i] == '.')
            {
                leafName[i] = '\0';
                break;
            }
            else
                i++;
        }
        if (leafName)
            mDbUri = PR_smprintf("%s%s.mab", kMDBDirectoryRoot, leafName);
    }

    // to do: we should use only one "return rv;" at the very end, instead of this
    //  multi return structure
    nsresult rv = NS_OK;
    nsFileSpec* dbPath = nsnull;
    char* fileName = PR_smprintf("%s.mab", leafName);

    nsCOMPtr<nsIAddrBookSession> abSession = 
             do_GetService(NS_ADDRBOOKSESSION_CONTRACTID, &rv); 
    if(NS_SUCCEEDED(rv))
        abSession->GetUserProfileDirectory(&dbPath);
    
    /* create address book database  */
    if (dbPath)
    {
        (*dbPath) += fileName;
        nsCOMPtr<nsIAddrDatabase> addrDBFactory = 
                 do_GetService(NS_ADDRDATABASE_CONTRACTID, &rv);
        if (NS_SUCCEEDED(rv) && addrDBFactory)
            rv = addrDBFactory->Open(dbPath, PR_TRUE, getter_AddRefs(mDatabase), PR_TRUE);
    }
    NS_ENSURE_SUCCESS(rv, rv);

    delete dbPath;

    nsCOMPtr<nsIRDFService> rdfService = do_GetService (NS_RDF_CONTRACTID "/rdf-service;1", &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    nsCOMPtr<nsIRDFResource> parentResource;
    rv = rdfService->GetResource(NS_LITERAL_CSTRING(kAllDirectoryRoot),
                                 getter_AddRefs(parentResource));
    nsCOMPtr<nsIAbDirectory> parentDir = do_QueryInterface(parentResource);
    if (!parentDir)
        return NS_ERROR_NULL_POINTER;

    // Get Pretty name from prefs.
    nsCOMPtr<nsIPrefBranch> pPref(do_GetService(NS_PREFSERVICE_CONTRACTID, &rv));
    if (NS_FAILED(rv)) 
        return nsnull;

    nsXPIDLString dirName;
    nsCOMPtr<nsIPrefLocalizedString> locString;
    nsCAutoString prefName;
    if (strcmp(fileName, kPersonalAddressbook) == 0)
        prefName.AssignLiteral("ldap_2.servers.pab.description");
    else
        prefName = NS_LITERAL_CSTRING("ldap_2.servers.") + nsDependentCString(leafName) + NS_LITERAL_CSTRING(".description");

    rv = pPref->GetComplexValue(prefName.get(), NS_GET_IID(nsIPrefLocalizedString), getter_AddRefs(locString));

    if (NS_SUCCEEDED(rv))
       rv = locString->ToString(getter_Copies(dirName));

    // If a name is found then use it, otherwise use the filename as last resort.
    if (NS_FAILED(rv) || dirName.IsEmpty())
      dirName.AssignASCII(leafName);
    parentDir->CreateDirectoryByURI(dirName, mDbUri, mMigrating);
        
    rv = ParseLDIFFile();

    if (leafName)
        nsCRT::free(leafName);
    if (fileName)
        PR_smprintf_free(fileName);

    return rv;
}

#define RIGHT2            0x03
#define RIGHT4            0x0f
#define CONTINUED_LINE_MARKER    '\001'

static unsigned char b642nib[0x80] = {
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0x3e, 0xff, 0xff, 0xff, 0x3f,
    0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b,
    0x3c, 0x3d, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
    0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e,
    0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,
    0x17, 0x18, 0x19, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20,
    0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
    0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30,
    0x31, 0x32, 0x33, 0xff, 0xff, 0xff, 0xff, 0xff
};

/*
 * str_parse_line - takes a line of the form "type:[:] value" and splits it
 * into components "type" and "value".  if a double colon separates type from
 * value, then value is encoded in base 64, and parse_line un-decodes it
 * (in place) before returning.
 */

nsresult AddressBookParser::str_parse_line(
    char    *line,
    char    **type,
    char    **value,
    int        *vlen
)
{
    char    *p, *s, *d, *byte, *stop;
    char    nib;
    int    i, b64;

    /* skip any leading space */
    while ( IS_SPACE( *line ) ) {
        line++;
    }
    *type = line;

    for ( s = line; *s && *s != ':'; s++ )
        ;    /* NULL */
    if ( *s == '\0' ) {
        return NS_ERROR_FAILURE;
    }

    /* trim any space between type and : */
    for ( p = s - 1; p > line && nsCRT::IsAsciiSpace( *p ); p-- ) {
        *p = '\0';
    }
    *s++ = '\0';

    /* check for double : - indicates base 64 encoded value */
    if ( *s == ':' ) {
        s++;
        b64 = 1;

    /* single : - normally encoded value */
    } else {
        b64 = 0;
    }

    /* skip space between : and value */
    while ( IS_SPACE( *s ) ) {
        s++;
    }

    /* if no value is present, error out */
    if ( *s == '\0' ) {
        return NS_ERROR_FAILURE;
    }

    /* check for continued line markers that should be deleted */
    for ( p = s, d = s; *p; p++ ) {
        if ( *p != CONTINUED_LINE_MARKER )
            *d++ = *p;
    }
    *d = '\0';

    *value = s;
    if ( b64 ) {
        stop = PL_strchr( s, '\0' );
        byte = s;
        for ( p = s, *vlen = 0; p < stop; p += 4, *vlen += 3 ) {
            for ( i = 0; i < 3; i++ ) {
                if ( p[i] != '=' && (p[i] & 0x80 ||
                    b642nib[ p[i] & 0x7f ] > 0x3f) ) {
                    return NS_ERROR_FAILURE;
                }
            }

            /* first digit */
            nib = b642nib[ p[0] & 0x7f ];
            byte[0] = nib << 2;
            /* second digit */
            nib = b642nib[ p[1] & 0x7f ];
            byte[0] |= nib >> 4;
            byte[1] = (nib & RIGHT4) << 4;
            /* third digit */
            if ( p[2] == '=' ) {
                *vlen += 1;
                break;
            }
            nib = b642nib[ p[2] & 0x7f ];
            byte[1] |= nib >> 2;
            byte[2] = (nib & RIGHT2) << 6;
            /* fourth digit */
            if ( p[3] == '=' ) {
                *vlen += 2;
                break;
            }
            nib = b642nib[ p[3] & 0x7f ];
            byte[2] |= nib;

            byte += 3;
        }
        s[ *vlen ] = '\0';
    } else {
        *vlen = (int) (d - s);
    }

    return NS_OK;
}

/*
 * str_getline - return the next "line" (minus newline) of input from a
 * string buffer of lines separated by newlines, terminated by \n\n
 * or \0.  this routine handles continued lines, bundling them into
 * a single big line before returning.  if a line begins with a white
 * space character, it is a continuation of the previous line. the white
 * space character (nb: only one char), and preceeding newline are changed
 * into CONTINUED_LINE_MARKER chars, to be deleted later by the
 * str_parse_line() routine above.
 *
 * it takes a pointer to a pointer to the buffer on the first call,
 * which it updates and must be supplied on subsequent calls.
 */

char * AddressBookParser::str_getline( char **next )
{
    char    *lineStr;
    char    c;

    if ( *next == NULL || **next == '\n' || **next == '\0' ) {
        return( NULL );
    }

    lineStr = *next;
    while ( (*next = PL_strchr( *next, '\n' )) != NULL ) {
        c = *(*next + 1);
        if ( IS_SPACE ( c ) && c != '\n' ) {
            **next = CONTINUED_LINE_MARKER;
            *(*next+1) = CONTINUED_LINE_MARKER;
        } else {
            *(*next)++ = '\0';
            break;
        }
    }

    return( lineStr );
}

/*
 * get one ldif record
 * 
 */
nsresult AddressBookParser::GetLdifStringRecord(char* buf, PRInt32 len, PRInt32& stopPos)
{
    for (; stopPos < len; stopPos++) 
    {
        char c = buf[stopPos];

        if (c == 0xA)
        {
            mLFCount++;
        }
        else if (c == 0xD)
        {
            mCRCount++;
        }
        else if ( c != 0xA && c != 0xD)
        {
            if (mLFCount == 0 && mCRCount == 0)
                 mLine.Append(c);
            else if (( mLFCount > 1) || ( mCRCount > 2 && mLFCount ) ||
                ( !mLFCount && mCRCount > 1 ))
            {
                return NS_OK;
            }
            else if ((mLFCount == 1 || mCRCount == 1))
            {
                 mLine.Append('\n');
                 mLine.Append(c);
                mLFCount = 0;
                mCRCount = 0;
            }
        }
    }

    if ((stopPos == len) && (mLFCount > 1) || (mCRCount > 2 && mLFCount) ||
        (!mLFCount && mCRCount > 1))
        return NS_OK;
    else
        return NS_ERROR_FAILURE;
}

nsresult AddressBookParser::ParseLDIFFile()
{
    char buf[1024];
    char* pBuf = &buf[0];
    PRInt32 startPos = 0;
    PRInt32 len = 0;
    PRBool bEof = PR_FALSE;
    nsVoidArray listPosArray;   // where each list/group starts in ldif file
    nsVoidArray listSizeArray;  // size of the list/group info
    PRInt32 savedStartPos = 0;
    PRInt32 filePos = 0;

    while (NS_SUCCEEDED(mFileSpec->Eof(&bEof)) && !bEof)
    {
        if (NS_SUCCEEDED(mFileSpec->Read(&pBuf, (PRInt32)sizeof(buf), &len)) && len > 0)
        {
            startPos = 0;

            while (NS_SUCCEEDED(GetLdifStringRecord(buf, len, startPos)))
            {
                if (mLine.Find("groupOfNames") == kNotFound)
                    AddLdifRowToDatabase(PR_FALSE);
                else
                {
                    //keep file position for mailing list
                    listPosArray.AppendElement((void*)savedStartPos);
                    listSizeArray.AppendElement((void*)(filePos + startPos-savedStartPos));
                    ClearLdifRecordBuffer();
                }
                savedStartPos = filePos + startPos;
            }
            filePos += len;
        }
    }
    //last row
    if (!mLine.IsEmpty() && mLine.Find("groupOfNames") == kNotFound)
        AddLdifRowToDatabase(PR_FALSE);

    // mail Lists
    PRInt32 i, pos, size;
    PRInt32 listTotal = listPosArray.Count();
    char *listBuf;
    ClearLdifRecordBuffer();  // make sure the buffer is clean
    for (i = 0; i < listTotal; i++)
    {
        pos  = NS_PTR_TO_INT32(listPosArray.ElementAt(i));
        size = NS_PTR_TO_INT32(listSizeArray.ElementAt(i));
        if (NS_SUCCEEDED(mFileSpec->Seek(pos)))
        {

            // Allocate enough space for the lists/groups as the size varies.
            listBuf = (char *) PR_Malloc(size);
            if (!listBuf)
              continue;
            if (NS_SUCCEEDED(mFileSpec->Read(&listBuf, size, &len)) && len > 0)
            {
                startPos = 0;

                while (NS_SUCCEEDED(GetLdifStringRecord(listBuf, len, startPos)))
                {
                    if (mLine.Find("groupOfNames") != kNotFound)
                    {
                        AddLdifRowToDatabase(PR_TRUE);
                        if (NS_SUCCEEDED(mFileSpec->Seek(0)))
                            break;
                    }
                }
            }
            PR_FREEIF(listBuf);
        }
    }
    return NS_OK;
}

void AddressBookParser::AddLdifRowToDatabase(PRBool bIsList)
{
    // If no data to process then reset CR/LF counters and return.
    if (mLine.IsEmpty())
    {
      mLFCount = 0;
      mCRCount = 0;
      return;
    }

    nsCOMPtr <nsIMdbRow> newRow;
    if (mDatabase)
    {
        if (bIsList)
            mDatabase->GetNewListRow(getter_AddRefs(newRow)); 
        else
            mDatabase->GetNewRow(getter_AddRefs(newRow)); 

        if (!newRow)
            return;
    }
    else
        return;

    char* cursor = ToNewCString(mLine); 
    char* saveCursor = cursor;  /* keep for deleting */ 
    char* line = 0; 
    char* typeSlot = 0; 
    char* valueSlot = 0; 
    int length = 0;  // the length  of an ldif attribute
    while ( (line = str_getline(&cursor)) != NULL)
    {
        if ( str_parse_line(line, &typeSlot, &valueSlot, &length) == 0 )
        {
            AddLdifColToDatabase(newRow, typeSlot, valueSlot, bIsList);
        }
        else
            continue; // parse error: continue with next loop iteration
    }
    nsMemory::Free(saveCursor);
    mDatabase->AddCardRowToDB(newRow);    

    if (bIsList)
        mDatabase->AddListDirNode(newRow);

    // Clear buffer for next record
    ClearLdifRecordBuffer();
}

void AddressBookParser::ClearLdifRecordBuffer()
{
  if (!mLine.IsEmpty())
  {
      mLine.Truncate();
      mLFCount = 0;
      mCRCount = 0;
  }
}


// We have two copies of this function in the code, one here for import and 
// the other one in addrbook/src/nsAddressBook.cpp for migrating.  If ths 
// function need modification, make sure change in both places until we resolve
// this problem.
static void SplitCRLFAddressField(nsCString &inputAddress, nsCString &outputLine1, nsCString &outputLine2)
{
  PRInt32 crlfPos = inputAddress.Find("\r\n");
  if (crlfPos != kNotFound)
  {
    inputAddress.Left(outputLine1, crlfPos);
    inputAddress.Right(outputLine2, inputAddress.Length() - (crlfPos + 2));
  }
  else
    outputLine1.Assign(inputAddress);
}


void AddressBookParser::AddLdifColToDatabase(nsIMdbRow* newRow, char* typeSlot, char* valueSlot, PRBool bIsList)
{
    nsCAutoString colType(typeSlot);
    nsCAutoString column(valueSlot);

    mdb_u1 firstByte = (mdb_u1)(colType.get())[0];
    switch ( firstByte )
    {
    case 'b':
      if ( kNotFound != colType.Find("birthyear") )
        mDatabase->AddBirthYear(newRow, column.get());
      break; // 'b'

    case 'c':
      if ( kNotFound != colType.Find("cn") || kNotFound != colType.Find("commonname") )
      {
        if (bIsList)
          mDatabase->AddListName(newRow, column.get());
        else
          mDatabase->AddDisplayName(newRow, column.get());
      }
      else if ( kNotFound != colType.Find("countryname") )
       {
      if (mStoreLocAsHome )
        mDatabase->AddHomeCountry(newRow, column.get());
      else
        mDatabase->AddWorkCountry(newRow, column.get());
      }

      // else if ( kNotFound != colType.Find("charset") )
      //   ioRow->AddColumn(ev, this->ColCharset(), yarn);

      else if ( kNotFound != colType.Find("cellphone") )
        mDatabase->AddCellularNumber(newRow, column.get());

//          else if ( kNotFound != colType.Find("calendar") )
//            ioRow->AddColumn(ev, this->ColCalendar(), yarn);

//          else if ( kNotFound != colType.Find("car") )
//            ioRow->AddColumn(ev, this->ColCar(), yarn);

      else if ( kNotFound != colType.Find("carphone") )
        mDatabase->AddCellularNumber(newRow, column.get());
//            ioRow->AddColumn(ev, this->ColCarPhone(), yarn);

//          else if ( kNotFound != colType.Find("carlicense") )
//            ioRow->AddColumn(ev, this->ColCarLicense(), yarn);
        
      else if ( kNotFound != colType.Find("custom1") )
        mDatabase->AddCustom1(newRow, column.get());
        
      else if ( kNotFound != colType.Find("custom2") )
        mDatabase->AddCustom2(newRow, column.get());
        
      else if ( kNotFound != colType.Find("custom3") )
        mDatabase->AddCustom3(newRow, column.get());
        
      else if ( kNotFound != colType.Find("custom4") )
        mDatabase->AddCustom4(newRow, column.get());
        
      else if ( kNotFound != colType.Find("company") )
        mDatabase->AddCompany(newRow, column.get());
      break; // 'c'

    case 'd':
      if ( kNotFound != colType.Find("description") )
      {
        if (bIsList)
          mDatabase->AddListDescription(newRow, column.get());
        else
          mDatabase->AddNotes(newRow, column.get());
      }
//          else if ( kNotFound != colType.Find("dn") ) // distinuished name
//            ioRow->AddColumn(ev, this->ColDistName(), yarn);

      else if ( kNotFound != colType.Find("department") )
        mDatabase->AddDepartment(newRow, column.get());

//          else if ( kNotFound != colType.Find("departmentnumber") )
//            ioRow->AddColumn(ev, this->ColDepartmentNumber(), yarn);

//          else if ( kNotFound != colType.Find("date") )
//            ioRow->AddColumn(ev, this->ColDate(), yarn);
      break; // 'd'

    case 'e':

//          if ( kNotFound != colType.Find("employeeid") )
//            ioRow->AddColumn(ev, this->ColEmployeeId(), yarn);

//          else if ( kNotFound != colType.Find("employeetype") )
//            ioRow->AddColumn(ev, this->ColEmployeeType(), yarn);
      break; // 'e'

    case 'f':

      if ( kNotFound != colType.Find("fax") ||
        kNotFound != colType.Find("facsimiletelephonenumber") )
        mDatabase->AddFaxNumber(newRow, column.get());
      break; // 'f'

    case 'g':
      if ( kNotFound != colType.Find("givenname") )
        mDatabase->AddFirstName(newRow, column.get());

//          else if ( kNotFound != colType.Find("gif") )
//            ioRow->AddColumn(ev, this->ColGif(), yarn);

//          else if ( kNotFound != colType.Find("geo") )
//            ioRow->AddColumn(ev, this->ColGeo(), yarn);

      break; // 'g'

    case 'h':
      if ( kNotFound != colType.Find("homephone") )
        mDatabase->AddHomePhone(newRow, column.get());

      else if ( kNotFound != colType.Find("homeurl") )
        mDatabase->AddWebPage2(newRow, column.get());
      break; // 'h'

    case 'i':
//          if ( kNotFound != colType.Find("imapurl") )
//            ioRow->AddColumn(ev, this->ColImapUrl(), yarn);
      break; // 'i'

    case 'j':
//          if ( kNotFound != colType.Find("jpeg") || kNotFound != colType.Find("jpegfile") )
//            ioRow->AddColumn(ev, this->ColJpegFile(), yarn);

      break; // 'j'

    case 'k':
//          if ( kNotFound != colType.Find("key") )
//            ioRow->AddColumn(ev, this->ColKey(), yarn);

//          else if ( kNotFound != colType.Find("keywords") )
//            ioRow->AddColumn(ev, this->ColKeywords(), yarn);

      break; // 'k'

    case 'l':
      if ( kNotFound != colType.Find("l") || kNotFound != colType.Find("locality") )
       {
      if (mStoreLocAsHome )
          mDatabase->AddHomeCity(newRow, column.get());
      else
          mDatabase->AddWorkCity(newRow, column.get());
      }

//          else if ( kNotFound != colType.Find("language") )
//            ioRow->AddColumn(ev, this->ColLanguage(), yarn);

//          else if ( kNotFound != colType.Find("logo") )
//            ioRow->AddColumn(ev, this->ColLogo(), yarn);

//          else if ( kNotFound != colType.Find("location") )
//            ioRow->AddColumn(ev, this->ColLocation(), yarn);

      break; // 'l'

    case 'm':
      if ( kNotFound != colType.Find("mail") )
        mDatabase->AddPrimaryEmail(newRow, column.get());

      else if ( kNotFound != colType.Find("member") && bIsList )
        mDatabase->AddLdifListMember(newRow, column.get());

//          else if ( kNotFound != colType.Find("manager") )
//            ioRow->AddColumn(ev, this->ColManager(), yarn);

//          else if ( kNotFound != colType.Find("modem") )
//            ioRow->AddColumn(ev, this->ColModem(), yarn);

//          else if ( kNotFound != colType.Find("msgphone") )
//            ioRow->AddColumn(ev, this->ColMessagePhone(), yarn);

      break; // 'm'

    case 'n':
//          if ( kNotFound != colType.Find("note") )
//            ioRow->AddColumn(ev, this->ColNote(), yarn);

      if ( kNotFound != colType.Find("notes") )
        mDatabase->AddNotes(newRow, column.get());

//          else if ( kNotFound != colType.Find("n") )
//            ioRow->AddColumn(ev, this->ColN(), yarn);

//          else if ( kNotFound != colType.Find("notifyurl") )
//            ioRow->AddColumn(ev, this->ColNotifyUrl(), yarn);

      break; // 'n'

    case 'o':
      if ( kNotFound != colType.Find("objectclass"))
        break;

      else if ( kNotFound != colType.Find("ou") || kNotFound != colType.Find("orgunit") )
        mDatabase->AddDepartment(newRow, column.get());

      else if ( kNotFound != colType.Find("o") ) // organization
        mDatabase->AddCompany(newRow, column.get());

      break; // 'o'

    case 'p':
      if ( kNotFound != colType.Find("postalcode") )
      {
      if (mStoreLocAsHome )
          mDatabase->AddHomeZipCode(newRow, column.get());
      else
          mDatabase->AddWorkZipCode(newRow, column.get());
      }

      else if ( kNotFound != colType.Find("postOfficeBox") )
      {
        nsCAutoString workAddr1, workAddr2;
        SplitCRLFAddressField(column, workAddr1, workAddr2);
        mDatabase->AddWorkAddress(newRow, workAddr1.get());
        mDatabase->AddWorkAddress2(newRow, workAddr2.get());
      }
      else if ( kNotFound != colType.Find("pager") ||
        kNotFound != colType.Find("pagerphone") )
        mDatabase->AddPagerNumber(newRow, column.get());
                    
//          else if ( kNotFound != colType.Find("photo") )
//            ioRow->AddColumn(ev, this->ColPhoto(), yarn);

//          else if ( kNotFound != colType.Find("parentphone") )
//            ioRow->AddColumn(ev, this->ColParentPhone(), yarn);

//          else if ( kNotFound != colType.Find("pageremail") )
//            ioRow->AddColumn(ev, this->ColPagerEmail(), yarn);

//          else if ( kNotFound != colType.Find("prefurl") )
//            ioRow->AddColumn(ev, this->ColPrefUrl(), yarn);

//          else if ( kNotFound != colType.Find("priority") )
//            ioRow->AddColumn(ev, this->ColPriority(), yarn);

      break; // 'p'

    case 'r':
      if ( kNotFound != colType.Find("region") )
       {
      if (mStoreLocAsHome )
        mDatabase->AddWorkState(newRow, column.get());
      else
        mDatabase->AddWorkState(newRow, column.get());
    }

//          else if ( kNotFound != colType.Find("rfc822mailbox") )
//            ioRow->AddColumn(ev, this->ColPrimaryEmail(), yarn);

//          else if ( kNotFound != colType.Find("rev") )
//            ioRow->AddColumn(ev, this->ColRev(), yarn);

//          else if ( kNotFound != colType.Find("role") )
//            ioRow->AddColumn(ev, this->ColRole(), yarn);
      break; // 'r'

    case 's':
      if ( kNotFound != colType.Find("sn") || kNotFound != colType.Find("surname") )
        mDatabase->AddLastName(newRow, column.get());

      else if ( kNotFound != colType.Find("streetaddress") )
       {
          nsCAutoString addr1, addr2;
          SplitCRLFAddressField(column, addr1, addr2);
      if (mStoreLocAsHome )
          {
              mDatabase->AddHomeAddress(newRow, addr1.get());
              mDatabase->AddHomeAddress2(newRow, addr2.get());
          }
      else
          {
              mDatabase->AddWorkAddress(newRow, addr1.get());
              mDatabase->AddWorkAddress2(newRow, addr2.get());
          }
      }
      else if ( kNotFound != colType.Find("st") )
      {
      if (mStoreLocAsHome )
        mDatabase->AddHomeState(newRow, column.get());
      else
        mDatabase->AddWorkState(newRow, column.get());
      }

//          else if ( kNotFound != colType.Find("secretary") )
//            ioRow->AddColumn(ev, this->ColSecretary(), yarn);

//          else if ( kNotFound != colType.Find("sound") )
//            ioRow->AddColumn(ev, this->ColSound(), yarn);

//          else if ( kNotFound != colType.Find("sortstring") )
//            ioRow->AddColumn(ev, this->ColSortString(), yarn);
        
      break; // 's'

    case 't':
      if ( kNotFound != colType.Find("title") )
        mDatabase->AddJobTitle(newRow, column.get());

      else if ( kNotFound != colType.Find("telephonenumber") )
      {
        mDatabase->AddWorkPhone(newRow, column.get());
      }

//          else if ( kNotFound != colType.Find("tiff") )
//            ioRow->AddColumn(ev, this->ColTiff(), yarn);

//          else if ( kNotFound != colType.Find("tz") )
//            ioRow->AddColumn(ev, this->ColTz(), yarn);
      break; // 't'

    case 'u':

        if ( kNotFound != colType.Find("uniquemember") && bIsList )
            mDatabase->AddLdifListMember(newRow, column.get());

//          else if ( kNotFound != colType.Find("uid") )
//            ioRow->AddColumn(ev, this->ColUid(), yarn);

      break; // 'u'

    case 'v':
//          if ( kNotFound != colType.Find("version") )
//            ioRow->AddColumn(ev, this->ColVersion(), yarn);

//          else if ( kNotFound != colType.Find("voice") )
//            ioRow->AddColumn(ev, this->ColVoice(), yarn);

      break; // 'v'

    case 'w':
      if ( kNotFound != colType.Find("workurl") )
        mDatabase->AddWebPage1(newRow, column.get());

      break; // 'w'

    case 'x':
      if ( kNotFound != colType.Find("xmozillanickname") )
      {
        if (bIsList)
          mDatabase->AddListNickName(newRow, column.get());
        else
          mDatabase->AddNickName(newRow, column.get());
      }

      else if ( kNotFound != colType.Find("xmozillausehtmlmail") )
      {
        ToLowerCase(column);
        if (kNotFound != column.Find("true"))
            mDatabase->AddPreferMailFormat(newRow, nsIAbPreferMailFormat::html);
        else if (kNotFound != column.Find("false"))
            mDatabase->AddPreferMailFormat(newRow, nsIAbPreferMailFormat::plaintext);
        else
            mDatabase->AddPreferMailFormat(newRow, nsIAbPreferMailFormat::unknown);
      }

      break; // 'x'

    case 'z':
      if ( kNotFound != colType.Find("zip") ) // alias for postalcode
       {
      if (mStoreLocAsHome )
        mDatabase->AddHomeZipCode(newRow, column.get());
      else
        mDatabase->AddWorkZipCode(newRow, column.get());
      }

      break; // 'z'

    default:
      break; // default
    }
}

NS_IMETHODIMP nsAddressBook::ConvertNA2toLDIF(nsIFileSpec *srcFileSpec, nsIFileSpec *dstFileSpec)
{
  nsresult rv = NS_OK;
  if (!srcFileSpec || !dstFileSpec) return NS_ERROR_NULL_POINTER;
  
  nsCOMPtr <nsIAbUpgrader> abUpgrader = do_GetService(NS_AB4xUPGRADER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!abUpgrader) return NS_ERROR_FAILURE;

  rv = abUpgrader->StartUpgrade4xAddrBook(srcFileSpec, dstFileSpec);
  if (NS_SUCCEEDED(rv)) {
    PRBool done = PR_FALSE;
    
    do {
      rv = abUpgrader->ContinueExport(&done);
      // XXX todo 
      // put this in the msg status
      printf("converting na2 to ldif...\n");
    } while (NS_SUCCEEDED(rv) && !done);
  }
  return rv;  
}

NS_IMETHODIMP nsAddressBook::ConvertLDIFtoMAB(nsIFileSpec *fileSpec, PRBool migrating, nsIAddrDatabase *db, PRBool bStoreLocAsHome, PRBool bImportingComm4x)
{
    nsresult rv;
    if (!fileSpec) return NS_ERROR_FAILURE;

    rv = fileSpec->OpenStreamForReading();
    NS_ENSURE_SUCCESS(rv, rv);

    AddressBookParser abParser(fileSpec, migrating, db, bStoreLocAsHome, bImportingComm4x);

    rv = abParser.ParseFile();
    NS_ENSURE_SUCCESS(rv, rv);

    fileSpec->CloseStream();

    // Commit the changes if 'db' is set.
    if (db)
      rv = db->Commit(nsAddrDBCommitType::kLargeCommit);
    return rv;
}

#define CSV_DELIM ","
#define CSV_DELIM_LEN 1
#define TAB_DELIM "\t"
#define TAB_DELIM_LEN 1
#define LDIF_DELIM (nsnull)
#define LDIF_DELIM_LEN 0

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

NS_IMETHODIMP nsAddressBook::ExportAddressBook(nsIDOMWindowInternal *aParentWin, nsIAbDirectory *aDirectory)
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
 
  nsCOMPtr<nsIScriptGlobalObject> globalObj = do_QueryInterface(aParentWin, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDocShell> docShell = globalObj->GetDocShell();
  if (!docShell)
    return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIDOMWindow> parentWindow = do_GetInterface(docShell);
  if (!parentWindow)
    return NS_NOINTERFACE;

  nsXPIDLString title;
  rv = bundle->GetStringFromName(NS_LITERAL_STRING("ExportAddressBookTitle").get(), getter_Copies(title));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = filePicker->Init(parentWindow, title, nsIFilePicker::modeSave);
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
  nsCOMPtr <nsIEnumerator> cardsEnumerator;
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
   
  // XXX TODO
  // when Outlook Express exports to .csv, the first line of the file is the column descriptions.
  // and I think we should too.  (otherwise, this data is just data, who knows what is what.)
  // but we have to fix the import code to skip the first card before we can do that.
  // the problem is Netscape 4.x didn't this, so if we always skip the first card
  // we'll lose data when importing from 4.x
  // for now, do it how 4.x does it and how the current import code expect it.
  // note, these are more fields that 4.x expects, so Mozilla -> 4.x will not work right.
#ifdef EXPORT_LIKE_OE_DOES
  for (i = 0; i < EXPORT_ATTRIBUTES_TABLE_COUNT; i++) {
    if (EXPORT_ATTRIBUTES_TABLE[i].includeForPlainText) {
      // XXX localize this?
      length = strlen(EXPORT_ATTRIBUTES_TABLE[i].abColName);
      rv = outputStream->Write(EXPORT_ATTRIBUTES_TABLE_COUNT[i].abColName, length, &writeCount);
      NS_ENSURE_SUCCESS(rv,rv);
      if (length != writeCount)
        return NS_ERROR_FAILURE;
      
      if (i < EXPORT_ATTRIBUTES_TABLE_COUNT - 1) {
        rv = outputStream->Write(aDelim, aDelimLen, &writeCount);
        NS_ENSURE_SUCCESS(rv,rv);
        if (aDelimLen != writeCount)
          return NS_ERROR_FAILURE;
      }
    }
  }
  rv = outputStream->Write(MSG_LINEBREAK, MSG_LINEBREAK_LEN, &writeCount);
  NS_ENSURE_SUCCESS(rv,rv);
  if (MSG_LINEBREAK_LEN != writeCount)
    return NS_ERROR_FAILURE;
#endif

  rv = aDirectory->GetChildCards(getter_AddRefs(cardsEnumerator));
  if (NS_SUCCEEDED(rv) && cardsEnumerator) {
    nsCOMPtr<nsISupports> item;
    for (rv = cardsEnumerator->First(); NS_SUCCEEDED(rv); rv = cardsEnumerator->Next()) {
      rv = cardsEnumerator->CurrentItem(getter_AddRefs(item));
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
            if (EXPORT_ATTRIBUTES_TABLE[i].includeForPlainText) {
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

              if (needsQuotes)
              {
                newValue.Insert(NS_LITERAL_STRING("\""), 0);
                newValue.AppendLiteral("\"");
              }

              // For notes, make sure CR/LF is converted to spaces 
              // to avoid creating multiple lines for a single card.
              //
              // the import code expects .txt, .tab, .csv files to
              // have non-ASCII data in the system charset
              //
              // note, this means if you machine is set to US-ASCII
              // but you have cards with Japanese characters
              // you will lose data when exporting.
              //
              // the solution is to export / import as LDIF.
              // non-ASCII data is treated as base64 encoded UTF-8 in LDIF
              if (!strcmp(EXPORT_ATTRIBUTES_TABLE[i].abColName, kNotesColumn))
              {
                if (!newValue.IsEmpty())
                {
                  newValue.ReplaceChar(nsCRT::CR, ' ');
                  newValue.ReplaceChar(nsCRT::LF, ' ');
                }
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
          rv = outputStream->Write(MSG_LINEBREAK, MSG_LINEBREAK_LEN, &writeCount);
          NS_ENSURE_SUCCESS(rv,rv);
          if (MSG_LINEBREAK_LEN != writeCount)
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
  nsCOMPtr <nsIEnumerator> cardsEnumerator;
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

  PRUint32 i;
  PRUint32 writeCount;
  PRUint32 length;
   
  rv = aDirectory->GetChildCards(getter_AddRefs(cardsEnumerator));
  if (NS_SUCCEEDED(rv) && cardsEnumerator) {
    nsCOMPtr<nsISupports> item;
    for (rv = cardsEnumerator->First(); NS_SUCCEEDED(rv); rv = cardsEnumerator->Next()) {
      rv = cardsEnumerator->CurrentItem(getter_AddRefs(item));
      if (NS_SUCCEEDED(rv)) {
        nsCOMPtr <nsIAbCard> card = do_QueryInterface(item, &rv);
        NS_ENSURE_SUCCESS(rv,rv);
         
        PRBool isMailList;
        rv = card->GetIsMailList(&isMailList);
        NS_ENSURE_SUCCESS(rv,rv);
       
        if (isMailList) {
          nsCString mailListCStr;

          rv = AppendLDIFForMailList(card, mailListCStr);
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
    
          rv = AppendBasicLDIFForCard(card, valueCStr);
          NS_ENSURE_SUCCESS(rv,rv);
          
          length = valueCStr.Length();
          rv = outputStream->Write(valueCStr.get(), length, &writeCount);
          NS_ENSURE_SUCCESS(rv,rv);
          if (length != writeCount)
            return NS_ERROR_FAILURE;
          
          valueCStr.Truncate();

          for (i = 0; i < EXPORT_ATTRIBUTES_TABLE_COUNT; i++) {
            if (EXPORT_ATTRIBUTES_TABLE[i].ldapPropertyName) {
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
                rv = AppendProperty(EXPORT_ATTRIBUTES_TABLE[i].ldapPropertyName, value.get(), valueCStr);
                NS_ENSURE_SUCCESS(rv,rv);
                
                valueCStr += LDIF_LINEBREAK;
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
          rv = outputStream->Write(LDIF_LINEBREAK, LDIF_LINEBREAK_LEN, &writeCount);
          NS_ENSURE_SUCCESS(rv,rv);
          if (LDIF_LINEBREAK_LEN != writeCount)
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

nsresult nsAddressBook::AppendLDIFForMailList(nsIAbCard *aCard, nsACString &aResult)
{
  nsresult rv;
  nsXPIDLString attrValue;
  
  rv = aCard->GetCardValue(kDisplayNameColumn, getter_Copies(attrValue));
  NS_ENSURE_SUCCESS(rv,rv);

  rv = AppendDNForCard("dn", aCard, aResult);
  NS_ENSURE_SUCCESS(rv,rv);

  aResult += LDIF_LINEBREAK \
             "objectclass: top" LDIF_LINEBREAK \
             "objectclass: groupOfNames" LDIF_LINEBREAK;

  rv = AppendProperty("cn", attrValue.get(), aResult);
  NS_ENSURE_SUCCESS(rv,rv);
  aResult += LDIF_LINEBREAK;

  rv = aCard->GetCardValue(kNicknameColumn, getter_Copies(attrValue));
  NS_ENSURE_SUCCESS(rv,rv);

  if (!attrValue.IsEmpty()) {
    rv = AppendProperty("xmozillanickname", attrValue.get(), aResult);
    NS_ENSURE_SUCCESS(rv,rv);
    aResult += LDIF_LINEBREAK;
  }

  rv = aCard->GetCardValue(kNotesColumn, getter_Copies(attrValue));
  NS_ENSURE_SUCCESS(rv,rv);

  if (!attrValue.IsEmpty()) {
    rv = AppendProperty("description", attrValue.get(), aResult);
    NS_ENSURE_SUCCESS(rv,rv);
    aResult += LDIF_LINEBREAK;
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

        rv = AppendDNForCard("member", listCard, aResult);
        NS_ENSURE_SUCCESS(rv,rv);

        aResult += LDIF_LINEBREAK;
      }
    }
  }
  
  aResult += LDIF_LINEBREAK;
  return NS_OK;
}

nsresult nsAddressBook::AppendDNForCard(const char *aProperty, nsIAbCard *aCard, nsACString &aResult)
{
  nsXPIDLString email;
  nsXPIDLString displayName;

  nsresult rv = aCard->GetCardValue(kPriEmailColumn, getter_Copies(email));
  NS_ENSURE_SUCCESS(rv,rv);

  rv = aCard->GetCardValue(kDisplayNameColumn, getter_Copies(displayName));
  NS_ENSURE_SUCCESS(rv,rv);

  nsString cnStr;

  if (!displayName.IsEmpty()) {
    cnStr += NS_LITERAL_STRING("cn=") + displayName;
    if (!email.IsEmpty()) {
      cnStr.AppendLiteral(",");
    }
  }

  if (!email.IsEmpty()) {
    cnStr += NS_LITERAL_STRING("mail=") + email;
  }

  rv = AppendProperty(aProperty, cnStr.get(), aResult);
  NS_ENSURE_SUCCESS(rv,rv);
  return rv;
}

nsresult nsAddressBook::AppendBasicLDIFForCard(nsIAbCard *aCard, nsACString &aResult)
{
  nsresult rv = AppendDNForCard("dn", aCard, aResult);
  NS_ENSURE_SUCCESS(rv,rv);
  aResult += LDIF_LINEBREAK \
    "objectclass: top" LDIF_LINEBREAK \
    "objectclass: person" LDIF_LINEBREAK \
    "objectclass: organizationalPerson" LDIF_LINEBREAK \
    "objectclass: inetOrgPerson" LDIF_LINEBREAK \
    "objectclass: " MOZ_AB_OBJECTCLASS LDIF_LINEBREAK;  


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
    char *base64Str = PL_Base64Encode(NS_ConvertUCS2toUTF8(aValue).get(), 0, nsnull);
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
  {       
      // only treat it as a match if it is an internet property
      VObject* iprop = isAPropertyOf(vObj, VCInternetProp);
      if (iprop)
          cardColName = kPriEmailColumn;
      else
          return;
  }
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
  aCard->SetCardValue(cardColName, NS_ConvertUTF8toUCS2(cardColValue).get());
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
  if (nsCRT::strcasecmp(aContentType, "x-application-addvcard") == 0) {
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
  else // The content-type was not x-application-addvcard...
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

CMDLINEHANDLER_IMPL(nsAddressBook,"-addressbook","general.startup.addressbook","chrome://messenger/content/addressbook/addressbook.xul","Start with the addressbook.",NS_ADDRESSBOOKSTARTUPHANDLER_CONTRACTID,"Addressbook Startup Handler",PR_FALSE,"", PR_TRUE)

