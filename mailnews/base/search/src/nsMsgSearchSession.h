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

#ifndef nsMsgSearchSession_h___
#define nsMsgSearchSession_h___

#include "nscore.h"
#include "nsIMsgSearchSession.h"

class nsMsgSearchService : public nsIMsgSearchService
{
public:
  NS_DECL_ISUPPORTS

	NS_IMETHOD AddSearchTerm (nsMsgSearchAttribute attrib,    /* attribute for this term                */
			nsMsgSearchOperator op,         /* operator e.g. opContains               */
			nsMsgSearchValue *value,        /* value e.g. "Dogbert"                   */
			PRBool BooleanAND, 		   /*  set to true if associated boolean operator is AND */
			char * arbitraryHeader);      /* user defined arbitrary header. ignored unless attrib = attribOtherHeader */

	NS_IMETHOD CountSearchTerms (PRInt32 *numTerms);

	NS_IMETHOD GetNthSearchTerm (PRInt32 whichTerm,	nsMsgSearchAttribute *attrib, 
		nsMsgSearchOperator *op, nsMsgSearchValue *value);

	NS_IMETHOD MSG_CountSearchScopes (PRInt32 *numScopes);

	NS_IMETHOD MSG_GetNthSearchScope (
		PRInt32 which,
		nsMsgScopeAttribute *scopeId,
		void **scope);

	/* add a scope (e.g. a mail folder) to the search */
	NS_IMETHOD AddScopeTerm (
		nsMsgScopeAttribute attrib,     /* what kind of scope term is this        */
		nsIMsgFolder *folder);       /* which folder to search                 */

/* special cases for LDAP since LDAP isn't really a folderInfo */
NS_IMETHOD AddLdapScope (nsMsgDIRServer *server) ;
//NS_IMETHOD AddAllLdapScopes (XP_List *dirServerList);

/* Call this function everytime the scope changes! It informs the FE if 
   the current scope support custom header use. FEs should not display the
   custom header dialog if custom headers are not supported */

NS_IMETHOD ScopeUsesCustomHeaders(nsMsgScopeAttribute scope,
	void * selection,              /* could be a folder or server based on scope */
	PRBool forFilters, PRBool *result) ;	 

NS_IMETHOD IsStringAttribute(     /* use this to determine if your attribute is a string attrib */
	nsMsgSearchAttribute attrib, PRBool *aResult) ;

/* add all scopes of a given type to the search */
NS_IMETHOD AddAllScopes (nsMsgScopeAttribute attrib) ;    /* what kind of scopes to add             */

/* begin execution of the search */
NS_IMETHOD Search (void) ;
NS_IMETHOD InterruptSearch (void) ;  
NS_IMETHOD GetSearchParam (void **aParam) ;
NS_IMETHOD GetSearchType (nsMsgSearchType *aResult) ;
NS_IMETHOD SetSearchParam (nsMsgSearchType type,          /* type of specialized search to perform   */
			void *param) ;                 /* extended search parameter               */
NS_IMETHOD GetNumResults (PRUint32 *numResults) ;


};

#endif
