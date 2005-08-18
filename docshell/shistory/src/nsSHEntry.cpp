/* -*- Mode: IDL; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Mozilla browser.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications, Inc.  Portions created by Netscape are
 * Copyright (C) 1999, Mozilla.  All Rights Reserved.
 * 
 * Contributor(s):
 *   Radha Kulkarni <radha@netscape.com>
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

// Local Includes
#include "nsSHEntry.h"

//*****************************************************************************
//***    nsSHEntry: Object Management
//*****************************************************************************

nsSHEntry::nsSHEntry() 
{
   NS_INIT_REFCNT();
}

nsSHEntry::~nsSHEntry() 
{
}

//*****************************************************************************
//    nsSHEntry: nsISupports
//*****************************************************************************

NS_IMPL_ADDREF(nsSHEntry)
NS_IMPL_RELEASE(nsSHEntry)

NS_INTERFACE_MAP_BEGIN(nsSHEntry)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsISHEntry)
   NS_INTERFACE_MAP_ENTRY(nsISHEntry)
NS_INTERFACE_MAP_END

//*****************************************************************************
//    nsSHEntry: nsISHEntry
//*****************************************************************************

NS_IMETHODIMP nsSHEntry::GetURI(nsIURI** aURI)
{
   NS_ENSURE_ARG_POINTER(aURI);
  
   *aURI = mURI;
   NS_IF_ADDREF(*aURI);
   return NS_OK;
}

NS_IMETHODIMP nsSHEntry::SetURI(nsIURI* aURI)
{
   mURI = aURI;
   return NS_OK;
}

NS_IMETHODIMP nsSHEntry::SetDocument(nsIDOMDocument* aDocument)
{
   mDocument = aDocument;
   return NS_OK;
}

NS_IMETHODIMP nsSHEntry::GetDocument(nsIDOMDocument** aResult)
{
	NS_ENSURE_ARG_POINTER(aResult);

   *aResult = mDocument;
   NS_IF_ADDREF(*aResult);
   return NS_OK;
}

NS_IMETHODIMP nsSHEntry::GetTitle(PRUnichar** aTitle)
{
   NS_ENSURE_ARG_POINTER(aTitle);

   *aTitle = mTitle.ToNewUnicode();
   return NS_OK;
}

NS_IMETHODIMP nsSHEntry::SetTitle(const PRUnichar* aTitle)
{
   mTitle = aTitle;
   return NS_OK;
}

NS_IMETHODIMP nsSHEntry::GetPostData(nsIInputStream** aResult)
{
   NS_ENSURE_ARG_POINTER(aResult);

   *aResult = mPostData;
   NS_IF_ADDREF(*aResult);
   return NS_OK;
}

NS_IMETHODIMP nsSHEntry::SetPostData(nsIInputStream* aPostData)
{
   mPostData = aPostData;
   return NS_OK;
}

NS_IMETHODIMP nsSHEntry::GetLayoutHistoryState(nsILayoutHistoryState** aResult)
{
   NS_ENSURE_ARG_POINTER(aResult);
   
   *aResult = mLayoutHistoryState;
   NS_IF_ADDREF(*aResult);
   return NS_OK;
}

NS_IMETHODIMP nsSHEntry::SetLayoutHistoryState(nsILayoutHistoryState* aState)
{
   mLayoutHistoryState = aState;
   return NS_OK;
}

NS_IMETHODIMP
nsSHEntry::Create(nsIURI * aURI, const PRUnichar * aTitle, nsIDOMDocument * aDOMDocument,
			         nsIInputStream * aInputStream, nsILayoutHistoryState * aHistoryLayoutState)
{
   SetURI(aURI);
	SetTitle(aTitle);
	SetDocument(aDOMDocument);
	SetPostData(aInputStream);
	SetLayoutHistoryState(aHistoryLayoutState);
	return NS_OK;
	
}
