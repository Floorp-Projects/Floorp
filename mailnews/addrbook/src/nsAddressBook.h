/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef __nsAddressBook_h
#define __nsAddressBook_h
 
#include "nsIAddressBook.h"
#include "nsIAbCard.h"
#include "nsCOMPtr.h"
#include "nsIAddrDatabase.h"
#include "nsIWebShell.h"
#include "nsIScriptGlobalObject.h"

#define NC_RDF_NEWABCARD			"http://home.netscape.com/NC-rdf#NewCard"
#define NC_RDF_DELETE				"http://home.netscape.com/NC-rdf#Delete"
#define NC_RDF_NEWDIRECTORY			"http://home.netscape.com/NC-rdf#NewDirectory"

class nsAddressBook : public nsIAddressBook
{
  
public:
	nsAddressBook();
	virtual ~nsAddressBook();

	NS_DECL_ISUPPORTS

	// nsIAddressBook
	NS_IMETHOD DeleteCards(nsIDOMXULElement *tree, nsIDOMXULElement *srcDirectory, nsIDOMNodeList *nodeList);
	NS_IMETHOD NewAddressBook(nsIRDFCompositeDataSource* db, nsIDOMXULElement *srcDirectory, const char *name);
	NS_IMETHOD DeleteAddressBooks(nsIRDFCompositeDataSource* db, nsIDOMXULElement *srcDirectory, nsIDOMNodeList *nodeList);
	NS_IMETHOD PrintCard();
        NS_IMETHOD PrintAddressbook();
	NS_IMETHOD SetWebShellWindow(nsIDOMWindow *win);

protected:
	nsresult DoCommand(nsIRDFCompositeDataSource *db, char * command, nsISupportsArray *srcArray, 
					   nsISupportsArray *arguments);
private:
	 nsIWebShell        *mWebShell;            // weak reference
};

#endif
