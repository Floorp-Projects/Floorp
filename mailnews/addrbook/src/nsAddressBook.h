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

#ifndef __nsAddressBook_h
#define __nsAddressBook_h
 
#include "nsIAddressBook.h"
#include "nsIAbCard.h"
#include "nsCOMPtr.h"
#include "nsIAddrDatabase.h"
#include "nsIDocShell.h"
#include "nsIScriptGlobalObject.h"
#include "nsICmdLineHandler.h"
#include "nsIComponentManager.h"
#include "nsIContentHandler.h"

class nsILocalFile;
class nsIAbDirectory;

#define NC_RDF_NEWABCARD			"http://home.netscape.com/NC-rdf#NewCard"
#define NC_RDF_MODIFY				"http://home.netscape.com/NC-rdf#Modify"
#define NC_RDF_DELETE				"http://home.netscape.com/NC-rdf#Delete"
#define NC_RDF_DELETECARD			"http://home.netscape.com/NC-rdf#DeleteCards"
#define NC_RDF_NEWDIRECTORY			"http://home.netscape.com/NC-rdf#NewDirectory"

#define   EXPORT_ATTRIBUTES_TABLE_COUNT      53

struct ExportAttributesTableStruct
{
    const char* abColName;
    const char* ldapPropertyName;
    PRBool includeForPlainText;
};

// for now, the oder of the attributes with PR_TRUE for includeForPlainText
// should be in the same order as they are in the import code
// see importMsgProperties and nsImportStringBundle.
// 
// XXX todo, merge with what's in nsAbLDAPProperties.cpp, so we can
// use this for LDAP and LDIF export
//
// here's how we're coming up with the ldapPropertyName values
// if they are specified in RFC 2798, use them
// else use the 4.x LDIF attribute names (for example, "xmozillanickname"
// as we want to allow export from mozilla back to 4.x, and other apps
// are probably out there that can handle 4.x LDIF)
// else use the MOZ_AB_LDIF_PREFIX prefix, see nsIAddrDatabase.idl
static ExportAttributesTableStruct EXPORT_ATTRIBUTES_TABLE[] = { 
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

class nsAddressBook : public nsIAddressBook, public nsICmdLineHandler, public nsIContentHandler
{
  
public:
	nsAddressBook();
	virtual ~nsAddressBook();

	NS_DECL_ISUPPORTS
 	NS_DECL_NSIADDRESSBOOK
	NS_DECL_NSICMDLINEHANDLER
    NS_DECL_NSICONTENTHANDLER

  CMDLINEHANDLER_REGISTERPROC_DECLS
    
protected:
	nsresult DoCommand(nsIRDFDataSource *db, const nsACString& command,
                     nsISupportsArray *srcArray, nsISupportsArray *arguments);
	nsresult GetAbDatabaseFromFile(char* pDbFile, nsIAddrDatabase **db);

private:
  nsIDocShell        *mDocShell;            // weak reference
  nsresult ExportDirectoryToDelimitedText(nsIAbDirectory *aDirectory, const char *aDelim, PRUint32 aDelimLen, nsILocalFile *aLocalFile);
  nsresult ExportDirectoryToLDIF(nsIAbDirectory *aDirectory, nsILocalFile *aLocalFile);
  nsresult AppendLDIFForMailList(nsIAbCard *aCard, nsACString &aResult);
  nsresult AppendDNForCard(const char *aProperty, nsIAbCard *aCard, nsACString &aResult);
  nsresult AppendBasicLDIFForCard(nsIAbCard *aCard, nsACString &aResult);
  nsresult AppendProperty(const char *aProperty, const PRUnichar *aValue, nsACString &aResult);
  PRBool IsSafeLDIFString(const PRUnichar *aStr);
};

#endif
