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
 */

#ifndef __nsAddressBook_h
#define __nsAddressBook_h
 
#include "nsIAddressBook.h"
#include "nsIAbCard.h"
#include "nsCOMPtr.h"
#include "nsIAddrDatabase.h"
#include "nsIDocShell.h"
#include "nsIScriptGlobalObject.h"
#include "nsIFileSpecWithUI.h"
#include "nsICmdLineHandler.h"
#include "nsIComponentManager.h"
#include "nsIFileSpec.h"

#define NC_RDF_NEWABCARD			"http://home.netscape.com/NC-rdf#NewCard"
#define NC_RDF_DELETE				"http://home.netscape.com/NC-rdf#Delete"
#define NC_RDF_DELETECARD			"http://home.netscape.com/NC-rdf#DeleteCards"
#define NC_RDF_NEWDIRECTORY			"http://home.netscape.com/NC-rdf#NewDirectory"

class nsAddressBook : public nsIAddressBook, public nsICmdLineHandler
{
  
public:
	nsAddressBook();
	virtual ~nsAddressBook();

	NS_DECL_ISUPPORTS
 	NS_DECL_NSIADDRESSBOOK
	NS_DECL_NSICMDLINEHANDLER

    CMDLINEHANDLER_REGISTERPROC_DECLS
    
protected:
	nsresult DoCommand(nsIRDFCompositeDataSource *db, char * command, nsISupportsArray *srcArray, 
					   nsISupportsArray *arguments);
	nsresult GetAbDatabaseFromFile(char* pDbFile, nsIAddrDatabase **db);

private:
	 nsIDocShell        *mDocShell;            // weak reference
};

#endif
