/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsMsgSearchSession_h___
#define nsMsgSearchSession_h___

#include "nscore.h"
#include "nsMsgSearchCore.h"
#include "nsIMsgSearchSession.h"

/* Use the code below as a template for the implementation class for this interface. */

/* Header file */
class nsMsgSearchSession : public nsIMsgSearchSession
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIMSGSEARCHSESSION

  nsMsgSearchSession();
  virtual ~nsMsgSearchSession();
  /* additional members */


	NS_IMETHOD AddSearchTerm (nsMsgSearchAttribute attrib,    /* attribute for this term                */
			nsMsgSearchOperator op,         /* operator e.g. opContains               */
			nsMsgSearchValue *value,        /* value e.g. "Dogbert"                   */
			PRBool BooleanAND, 		   /*  set to true if associated boolean operator is AND */
			char * arbitraryHeader);      /* user defined arbitrary header. ignored unless attrib = attribOtherHeader */

	/* add a scope (e.g. a mail folder) to the search */
	NS_IMETHOD AddScopeTerm (
		nsMsgSearchScope attrib,     /* what kind of scope term is this        */
		nsIMsgFolder *folder);       /* which folder to search                 */

/* Call this function everytime the scope changes! It informs the FE if 
   the current scope support custom header use. FEs should not display the
   custom header dialog if custom headers are not supported */

NS_IMETHOD ScopeUsesCustomHeaders(nsMsgSearchScope scope,
	void * selection,              /* could be a folder or server based on scope */
	PRBool forFilters, PRBool *result) ;	 

/* add all scopes of a given type to the search */
NS_IMETHOD AddAllScopes (nsMsgSearchScope attrib) ;    /* what kind of scopes to add             */

NS_IMETHOD GetSearchType (nsMsgSearchType *aResult) ;
NS_IMETHOD SetSearchParam (nsMsgSearchType type,          /* type of specialized search to perform   */
			void *param) ;                 /* extended search parameter               */
NS_IMETHOD GetNumResults (PRUint32 *numResults) ;


};

#endif
