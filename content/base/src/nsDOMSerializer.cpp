/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include "nsDOMSerializer.h"
#include "nsIDOMNode.h"
#include "nsIDOMRange.h"
#include "nsIDOMClassInfo.h"
#include "nsIOutputStream.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDocumentEncoder.h"
#include "nsIComponentManager.h"
#include "nsIContentSerializer.h"
#include "nsString.h"

#include "nsLayoutCID.h" // XXX Need range CID
static NS_DEFINE_CID(kRangeCID,NS_RANGE_CID);

nsDOMSerializer::nsDOMSerializer()
{
  NS_INIT_ISUPPORTS();
}

nsDOMSerializer::~nsDOMSerializer()
{
}


// QueryInterface implementation for nsDOMSerializer
NS_INTERFACE_MAP_BEGIN(nsDOMSerializer)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSerializer)
  NS_INTERFACE_MAP_ENTRY_DOM_CLASSINFO(DOMSerializer)
NS_INTERFACE_MAP_END


NS_IMPL_ADDREF(nsDOMSerializer)
NS_IMPL_RELEASE(nsDOMSerializer)


static nsresult SetUpEncoder(nsIDOMNode *aRoot, const char* aCharset, nsIDocumentEncoder **aEncoder)
{
  *aEncoder = nsnull;
  
  nsresult rv;
  nsCOMPtr<nsIDocumentEncoder> encoder(do_CreateInstance(NS_DOC_ENCODER_CONTRACTID_BASE "text/xml",&rv));
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
  rv = encoder->Init(document,NS_LITERAL_STRING("text/xml"),nsIDocumentEncoder::OutputEncodeEntities);
  if (NS_FAILED(rv))
    return rv;

  nsAutoString charset;
  if (aCharset) {
    charset.AssignWithConversion(aCharset);
  } else {
    rv = document->GetDocumentCharacterSet(charset);
    if (NS_FAILED(rv))
      return rv;
  }
  rv = encoder->SetCharset(charset);
  if (NS_FAILED(rv))
    return rv;

  // If we are working on the entire document we do not need to specify which part to serialize
  if (!entireDocument) {
    nsCOMPtr<nsIDOMRange> range(do_CreateInstance(kRangeCID));
    rv = range->SelectNode(aRoot);
    if (NS_SUCCEEDED(rv)) {
      rv = encoder->SetRange(range);
    }
  }

  if (NS_SUCCEEDED(rv)) {
    *aEncoder = encoder.get();
    NS_ADDREF(*aEncoder);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMSerializer::SerializeToString(nsIDOMNode *root, PRUnichar **_retval)
{
  NS_ENSURE_ARG_POINTER(root);
  NS_ENSURE_ARG_POINTER(_retval);  
  
  *_retval = nsnull;

  nsCOMPtr<nsIDocumentEncoder> encoder;
  nsresult rv = SetUpEncoder(root,nsnull,getter_AddRefs(encoder));
  if (NS_FAILED(rv))
    return rv;

  nsAutoString str;
  rv = encoder->EncodeToString(str);
  if (NS_FAILED(rv))
    return rv;

  *_retval = str.ToNewUnicode();
  if (!*_retval)
    return NS_ERROR_OUT_OF_MEMORY;

  return NS_OK;
}

NS_IMETHODIMP
nsDOMSerializer::SerializeToStream(nsIDOMNode *root, 
                                   nsIOutputStream *stream, 
                                   const char *charset)
{
  NS_ENSURE_ARG_POINTER(root);
  NS_ENSURE_ARG_POINTER(stream);
  NS_ENSURE_ARG_POINTER(charset);

  nsCOMPtr<nsIDocumentEncoder> encoder;
  nsresult rv = SetUpEncoder(root,charset,getter_AddRefs(encoder));
  if (NS_FAILED(rv))
    return rv;

  return encoder->EncodeToStream(stream);
}
