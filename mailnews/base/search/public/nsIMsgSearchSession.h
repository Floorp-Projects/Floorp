/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef nsIMsgSearchSession_h___
#define nsIMsgSearchSession_h___

#include "nscore.h"
#include "nsISupports.h"

class nsIMsgFolder;

#include "nsMsgBaseCID.h"

/* a819050a-0302-11d3-a50a-0060b0fc04b7 */
#define NS_IMSGSEARCHSESSION_IID                         \
{ 0xa819050a, 0x0302, 0x11d3,                 \
    { 0xa5, 0x0a, 0x0, 0x60, 0xb0, 0xfc, 0x04, 0xb7 } }

////////////////////////////////////////////////////////////////////////////////////////
// The Msg Search Session is an interface designed to make constructing searches
// easier. Clients typically build up search terms, and then run the search
//
////////////////////////////////////////////////////////////////////////////////////////


class nsIMsgSearchService : public nsISupports
{
public:
    static const nsIID& GetIID() { static nsIID iid = NS_IMSGSEARCHSESSION_IID; return iid; }

	NS_IMETHOD AddSearchTerm (nsMsgSearchAttribute attrib,    /* attribute for this term                */
			nsMsgSearchOperator op,         /* operator e.g. opContains               */
			nsMsgSearchValue *value,        /* value e.g. "Dogbert"                   */
			PRBool BooleanAND, 		   /*  set to true if associated boolean operator is AND */
			char * arbitraryHeader) = 0;      /* user defined arbitrary header. ignored unless attrib = attribOtherHeader */

	NS_IMETHOD CountSearchTerms (PRInt32 *numTerms) = 0;

	NS_IMETHOD GetNthSearchTerm (PRInt32 whichTerm,	nsMsgSearchAttribute *attrib, 
		nsMsgSearchOperator *op, nsMsgSearchValue *value) = 0;

	NS_IMETHOD MSG_CountSearchScopes (PRInt32 *numScopes) = 0;

	NS_IMETHOD MSG_GetNthSearchScope (
		PRInt32 which,
		nsMsgScopeAttribute *scopeId,
		void **scope) = 0;

	/* add a scope (e.g. a mail folder) to the search */
	NS_IMETHOD AddScopeTerm (
		nsMsgScopeAttribute attrib,     /* what kind of scope term is this        */
		nsIMsgFolder *folder) = 0;       /* which folder to search                 */

/* special cases for LDAP since LDAP isn't really a folderInfo */
NS_IMETHOD AddLdapScope (nsMsgDIRServer *server) = 0;
//NS_IMETHOD AddAllLdapScopes (XP_List *dirServerList);

/* Call this function everytime the scope changes! It informs the FE if 
   the current scope support custom header use. FEs should not display the
   custom header dialog if custom headers are not supported */

NS_IMETHOD ScopeUsesCustomHeaders(nsMsgScopeAttribute scope,
	void * selection,              /* could be a folder or server based on scope */
	PRBool forFilters, PRBool *result) = 0;	 

NS_IMETHOD IsStringAttribute(     /* use this to determine if your attribute is a string attrib */
	nsMsgSearchAttribute attrib, PRBool *aResult) = 0;

/* add all scopes of a given type to the search */
NS_IMETHOD AddAllScopes (nsMsgScopeAttribute attrib) = 0;    /* what kind of scopes to add             */

/* begin execution of the search */
NS_IMETHOD Search (void) = 0;
NS_IMETHOD InterruptSearch (void) = 0;  
NS_IMETHOD GetSearchParam (void **aParam) = 0;
NS_IMETHOD GetSearchType (nsMsgSearchType *aResult) = 0;
NS_IMETHOD SetSearchParam (nsMsgSearchType type,          /* type of specialized search to perform   */
			void *param) = 0;                 /* extended search parameter               */
NS_IMETHOD GetNumResults (PRUint32 *numResults) = 0;


};

#endif
