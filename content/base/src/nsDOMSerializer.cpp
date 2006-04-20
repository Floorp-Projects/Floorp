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

#include "nsDOMSerializer.h"
#include "nsIDOMNode.h"
#include "nsIDOMClassInfo.h"
#include "nsIOutputStream.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDocumentEncoder.h"
#include "nsIComponentManager.h"
#include "nsIContentSerializer.h"
#include "nsString.h"
#include "nsReadableUtils.h"

#include "nsIJSContextStack.h"
#include "nsIScriptSecurityManager.h"
#include "nsIURI.h"

nsDOMSerializer::nsDOMSerializer()
{
}

nsDOMSerializer::~nsDOMSerializer()
{
}


// QueryInterface implementation for nsDOMSerializer
NS_INTERFACE_MAP_BEGIN(nsDOMSerializer)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSerializer)
  NS_INTERFACE_MAP_ENTRY_EXTERNAL_DOM_CLASSINFO(XMLSerializer)
NS_INTERFACE_MAP_END


NS_IMPL_ADDREF(nsDOMSerializer)
NS_IMPL_RELEASE(nsDOMSerializer)


static nsresult
SetUpEncoder(nsIDOMNode *aRoot, const nsACString& aCharset,
             nsIDocumentEncoder **aEncoder)
{
  *aEncoder = nsnull;
   
  nsresult rv;
  nsCOMPtr<nsIDocumentEncoder> encoder =
    do_CreateInstance(NS_DOC_ENCODER_CONTRACTID_BASE "text/xml", &rv);
  if (NS_FAILED(rv))
    return rv;

  PRBool entireDocument = PR_TRUE;
  nsCOMPtr<nsIDocument> document(do_QueryInterface(aRoot));
  if (!document) {
    entireDocument = PR_FALSE;
    nsCOMPtr<nsIDOMDocument> domDoc;
    rv = aRoot->GetOwnerDocument(getter_AddRefs(domDoc));
    if (NS_FAILED(rv))
      return rv;
    document = do_QueryInterface(domDoc);
  }

  // This method will fail if no document
  rv = encoder->Init(document, NS_LITERAL_STRING("text/xml"),
                     nsIDocumentEncoder::OutputEncodeBasicEntities);
  if (NS_FAILED(rv))
    return rv;

  nsCAutoString charset(aCharset);
  if (charset.IsEmpty()) {
    charset = document->GetDocumentCharacterSet();
  }
  rv = encoder->SetCharset(charset);
  if (NS_FAILED(rv))
    return rv;

  // If we are working on the entire document we do not need to
  // specify which part to serialize
  if (!entireDocument) {
    rv = encoder->SetNode(aRoot);
  }

  if (NS_SUCCEEDED(rv)) {
    *aEncoder = encoder.get();
    NS_ADDREF(*aEncoder);
  }

  return rv;
}

static nsresult
CheckSameOrigin(nsIDOMNode *aRoot)
{
  // Make sure that the caller has permission to access the root

  // Be sure to QI to nsINode to make sure we're passed a native
  // object.

  nsCOMPtr<nsINode> node(do_QueryInterface(aRoot));

  if (NS_UNLIKELY(!node)) {
    // We got a non-native object.

    return NS_ERROR_INVALID_POINTER;
  }

  
  nsIPrincipal *principal = node->GetNodePrincipal();

  if (principal) {
    nsresult rv;
    nsCOMPtr<nsIScriptSecurityManager> secMan = 
      do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    PRBool ubrEnabled = PR_FALSE;
    rv = secMan->IsCapabilityEnabled("UniversalBrowserRead", &ubrEnabled);
    NS_ENSURE_SUCCESS(rv, rv);

    if (ubrEnabled) {
      // UniversalBrowserRead is enabled (or we're not called from
      // script), permit access.
      return NS_OK;
    }

    nsCOMPtr<nsIPrincipal> subject;
    rv = secMan->GetSubjectPrincipal(getter_AddRefs(subject));
    NS_ENSURE_SUCCESS(rv, rv);

    // XXXbz can we happen to not have a subject principal here?
    // nsScriptSecurityManager::IsCapabilityEnabled doesn't actually use
    // GetSubjectPrincipal, so not sure...
    // In any case, no subject principal means access is allowed.
    if (subject) {
      // Check if the caller is from the same origin that the root is from.
      return secMan->CheckSameOriginPrincipal(subject, principal);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMSerializer::SerializeToString(nsIDOMNode *aRoot, nsAString& _retval)
{
  NS_ENSURE_ARG_POINTER(aRoot);
  
  _retval.Truncate();

  nsresult rv = CheckSameOrigin(aRoot);
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIDocumentEncoder> encoder;
  rv = SetUpEncoder(aRoot, EmptyCString(), getter_AddRefs(encoder));
  if (NS_FAILED(rv))
    return rv;

  return encoder->EncodeToString(_retval);
}

NS_IMETHODIMP
nsDOMSerializer::SerializeToStream(nsIDOMNode *aRoot, 
                                   nsIOutputStream *aStream, 
                                   const nsACString& aCharset)
{
  NS_ENSURE_ARG_POINTER(aRoot);
  NS_ENSURE_ARG_POINTER(aStream);
  // The charset arg can be null, in which case we get the document's
  // charset and use that when serializing.

  nsresult rv = CheckSameOrigin(aRoot);
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIDocumentEncoder> encoder;
  rv = SetUpEncoder(aRoot, aCharset, getter_AddRefs(encoder));
  if (NS_FAILED(rv))
    return rv;

  return encoder->EncodeToStream(aStream);
}
