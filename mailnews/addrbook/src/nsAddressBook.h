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

#ifndef __nsAddressBook_h
#define __nsAddressBook_h
 
#include "nsIAddressBook.h"
#include "nsIAbCard.h"
#include "nsCOMPtr.h"
#include "nsIAddrDatabase.h"
#include "nsIComponentManager.h"
#include "nsIContentHandler.h"
#include "nsIStreamLoader.h"
#include "rdf.h"

#ifdef MOZ_XUL_APP
#include "nsICommandLineHandler.h"
#define ICOMMANDLINEHANDLER nsICommandLineHandler
#else
#include "nsICmdLineHandler.h"
#define ICOMMANDLINEHANDLER nsICmdLineHandler
#endif

class nsILocalFile;
class nsIAbDirectory;
class nsIAbLDAPAttributeMap;

#define NC_RDF_NEWABCARD            NC_NAMESPACE_URI "NewCard"
#define NC_RDF_MODIFY               NC_NAMESPACE_URI "Modify"
#define NC_RDF_DELETE               NC_NAMESPACE_URI "Delete"
#define NC_RDF_DELETECARD           NC_NAMESPACE_URI "DeleteCards"
#define NC_RDF_NEWDIRECTORY         NC_NAMESPACE_URI "NewDirectory"

#define   EXPORT_ATTRIBUTES_TABLE_COUNT      51

struct ExportAttributesTableStruct
{
    const char* abColName;
    PRUint32 plainTextStringID;
};

const extern ExportAttributesTableStruct EXPORT_ATTRIBUTES_TABLE[EXPORT_ATTRIBUTES_TABLE_COUNT];

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

class nsAddressBook : public nsIAddressBook,
                      public ICOMMANDLINEHANDLER,
                      public nsIContentHandler,
                      public nsIStreamLoaderObserver
{
  
public:
	nsAddressBook();
	virtual ~nsAddressBook();

	NS_DECL_ISUPPORTS
 	NS_DECL_NSIADDRESSBOOK
    NS_DECL_NSICONTENTHANDLER
    NS_DECL_NSISTREAMLOADEROBSERVER

#ifdef MOZ_XUL_APP
    NS_DECL_NSICOMMANDLINEHANDLER
#else
	NS_DECL_NSICMDLINEHANDLER
    CMDLINEHANDLER_REGISTERPROC_DECLS
#endif
    
protected:
	nsresult DoCommand(nsIRDFDataSource *db, const nsACString& command,
                     nsISupportsArray *srcArray, nsISupportsArray *arguments);
	nsresult GetAbDatabaseFromFile(char* pDbFile, nsIAddrDatabase **db);

private:
  nsresult ExportDirectoryToDelimitedText(nsIAbDirectory *aDirectory, const char *aDelim, PRUint32 aDelimLen, nsILocalFile *aLocalFile);
  nsresult ExportDirectoryToLDIF(nsIAbDirectory *aDirectory, nsILocalFile *aLocalFile);
  nsresult AppendLDIFForMailList(nsIAbCard *aCard, nsIAbLDAPAttributeMap *aAttrMap, nsACString &aResult);
  nsresult AppendDNForCard(const char *aProperty, nsIAbCard *aCard, nsIAbLDAPAttributeMap *aAttrMap, nsACString &aResult);
  nsresult AppendBasicLDIFForCard(nsIAbCard *aCard, nsIAbLDAPAttributeMap *aAttrMap, nsACString &aResult);
  nsresult AppendProperty(const char *aProperty, const PRUnichar *aValue, nsACString &aResult);
  PRBool IsSafeLDIFString(const PRUnichar *aStr);
};

#endif
