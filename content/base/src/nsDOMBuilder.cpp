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
 * Netscape Communiactions Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *      Boris Zbarsky <bzbarsky@mit.edu>  (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsDOMBuilder.h"
#include "nsDOMError.h"
#include "nsContentUtils.h"  // for NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO
#include "nsIDOMDocument.h"

static const char* kLoadAsData = "loadAsData";

nsresult
NS_NewDOMBuilder(nsIDOMDOMBuilder** aResult,
                 PRUint16 aMode,
                 const nsAString & aSchemaType,
		 nsIDOMDOMImplementation* aDOMImplementation)
{
  NS_PRECONDITION(aResult, "Null out ptr?  Who do you think you are, flouting XPCOM contract?");
  NS_PRECONDITION(aDOMImplementation, "How are we supposed to create documents without a DOMImplementation?");

  nsDOMBuilder* it = new nsDOMBuilder(aMode, aSchemaType, aDOMImplementation);
  if (!it)
    return NS_ERROR_OUT_OF_MEMORY;

  return CallQueryInterface(it, aResult);
}

nsDOMBuilder::nsDOMBuilder(PRUint16 aMode,
			   const nsAString& aSchemaType,
			   nsIDOMDOMImplementation* aDOMImplementation)
{
  mDOMImplementation = aDOMImplementation;
}

nsDOMBuilder::~nsDOMBuilder()
{
}

NS_IMPL_ADDREF(nsDOMBuilder)
NS_IMPL_RELEASE(nsDOMBuilder)

NS_INTERFACE_MAP_BEGIN(nsDOMBuilder)
  NS_INTERFACE_MAP_ENTRY(nsIDOMDOMBuilder)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMDOMBuilder)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(DOMBuilder)
NS_INTERFACE_MAP_END

// nsIDOMDOMBuilder
NS_IMETHODIMP
nsDOMBuilder::GetEntityResolver(nsIDOMDOMEntityResolver** aEntityResolver)
{
  *aEntityResolver = mEntityResolver;
  NS_IF_ADDREF(*aEntityResolver);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMBuilder::SetEntityResolver(nsIDOMDOMEntityResolver* aEntityResolver)
{
  mEntityResolver = aEntityResolver;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMBuilder::GetErrorHandler(nsIDOMDOMErrorHandler** aErrorHandler)
{
  *aErrorHandler = mErrorHandler;
  NS_IF_ADDREF(*aErrorHandler);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMBuilder::SetErrorHandler(nsIDOMDOMErrorHandler* aErrorHandler)
{
  mErrorHandler = aErrorHandler;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMBuilder::GetFilter(nsIDOMDOMBuilderFilter** aFilter)
{
  *aFilter = mFilter;
  NS_IF_ADDREF(*aFilter);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMBuilder::SetFilter(nsIDOMDOMBuilderFilter* aFilter)
{
  mFilter = aFilter;
  return NS_OK;
}


NS_IMETHODIMP
nsDOMBuilder::SetFeature(const nsAString& aName, PRBool aState)
{
  // XXX We don't know about any features yet
  return NS_ERROR_DOM_NOT_FOUND_ERR;
}

NS_IMETHODIMP
nsDOMBuilder::CanSetFeature(const nsAString& aName, PRBool aState,
			    PRBool* aCanSet)
{
  // XXX We can't set anything
  *aCanSet = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMBuilder::GetFeature(const nsAString& aName, PRBool* aIsOn)
{
  // XXX We don't know about any features yet
  return NS_ERROR_DOM_NOT_FOUND_ERR;
}
    
NS_IMETHODIMP
nsDOMBuilder::ParseURI(const nsAString& aURI, nsIDOMDocument** aDocument)
{
  *aDocument = nsnull;

  nsCOMPtr<nsIDOMDocument> domDoc;

  NS_NAMED_LITERAL_STRING(emptyStr, "");
  mDOMImplementation->CreateDocument(emptyStr,
				     emptyStr,
				     nsnull,
				     getter_AddRefs(domDoc));

  if (!domDoc)
    return NS_ERROR_FAILURE;

  // XXX synchronous loading?  We'd have to do something right about now.

  
  
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDOMBuilder::Parse(nsIDOMDOMInputSource* aInputSource,
		    nsIDOMDocument** aDocument)
{
  *aDocument = nsnull;
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDOMBuilder::ParseWithContext(nsIDOMDOMInputSource* aInputSource,
			       nsIDOMNode* aContextNode,
			       PRUint16 aAction)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}
