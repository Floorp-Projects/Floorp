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
 * Copyright (C) 2000 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "msgCore.h"
#include "nsMsgSearchCore.h"
#include "nsMsgSearchAdapter.h"
#include "nsMsgSearchSession.h"


/* Implementation file */
NS_IMPL_ISUPPORTS1(nsMsgSearchSession, nsIMsgSearchSession)

nsMsgSearchSession::nsMsgSearchSession()
{
  NS_INIT_ISUPPORTS();
  /* member initializers and constructor code */
}

nsMsgSearchSession::~nsMsgSearchSession()
{
  /* destructor code */
}

/* [noscript] void AddSearchTerm (in nsMsgSearchAttribute attrib, in nsMsgSearchOperator op, in nsMsgSearchValue value, in boolean BooleanAND, in string arbitraryHeader); */
NS_IMETHODIMP nsMsgSearchSession::AddSearchTerm(nsMsgSearchAttribute attrib, nsMsgSearchOperator op, nsMsgSearchValue * value, PRBool BooleanAND, const char *arbitraryHeader)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute long numSearchTerms; */
NS_IMETHODIMP nsMsgSearchSession::GetNumSearchTerms(PRInt32 *aNumSearchTerms)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* [noscript] void GetNthSearchTerm (in long whichTerm, in nsMsgSearchAttribute attrib, in nsMsgSearchOperator op, in nsMsgSearchValue value); */
NS_IMETHODIMP nsMsgSearchSession::GetNthSearchTerm(PRInt32 whichTerm, nsMsgSearchAttribute attrib, nsMsgSearchOperator op, nsMsgSearchValue * value)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* long MSG_CountSearchScopes (); */
NS_IMETHODIMP nsMsgSearchSession::MSG_CountSearchScopes(PRInt32 *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* [noscript] voidStar MSG_GetNthSearchScope (in long which, in nsMsgSearchScope scopeId); */
NS_IMETHODIMP nsMsgSearchSession::MSG_GetNthSearchScope(PRInt32 which, nsMsgSearchScope *scopeId, void * *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void AddScopeTerm (in nsMsgSearchScope attrib, in nsIMsgFolder folder); */
NS_IMETHODIMP nsMsgSearchSession::AddScopeTerm(nsMsgSearchScope attrib, nsIMsgFolder *folder)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* [noscript] void AddLdapScope (in nsMsgDIRServer server); */
NS_IMETHODIMP nsMsgSearchSession::AddLdapScope(nsMsgDIRServer * server)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* [noscript] boolean ScopeUsesCustomHeaders (in nsMsgSearchScope scope, in voidStar selection, in boolean forFilters); */
NS_IMETHODIMP nsMsgSearchSession::ScopeUsesCustomHeaders(nsMsgSearchScope scope, void * selection, PRBool forFilters, PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* boolean IsStringAttribute (in nsMsgSearchAttribute attrib); */
NS_IMETHODIMP nsMsgSearchSession::IsStringAttribute(nsMsgSearchAttribute attrib, PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void AddAllScopes (in nsMsgSearchScope attrib); */
NS_IMETHODIMP nsMsgSearchSession::AddAllScopes(nsMsgSearchScope attrib)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void Search (); */
NS_IMETHODIMP nsMsgSearchSession::Search()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void InterruptSearch (); */
NS_IMETHODIMP nsMsgSearchSession::InterruptSearch()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* [noscript] readonly attribute voidStar searchParam; */
NS_IMETHODIMP nsMsgSearchSession::GetSearchParam(void * *aSearchParam)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute nsMsgSearchType searchType; */
NS_IMETHODIMP nsMsgSearchSession::GetSearchType(nsMsgSearchType * *aSearchType)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* [noscript] nsMsgSearchType SetSearchParam (in nsMsgSearchType type, in voidStar param); */
NS_IMETHODIMP nsMsgSearchSession::SetSearchParam(nsMsgSearchType *type, void * param, nsMsgSearchType **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute long numResults; */
NS_IMETHODIMP nsMsgSearchSession::GetNumResults(PRInt32 *aNumResults)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

