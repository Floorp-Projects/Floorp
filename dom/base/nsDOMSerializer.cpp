/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDOMSerializer.h"

#include "mozilla/Encoding.h"
#include "nsIDocument.h"
#include "nsIDocumentEncoder.h"
#include "nsIDOMDocument.h"
#include "nsComponentManagerUtils.h"
#include "nsContentCID.h"
#include "nsContentUtils.h"
#include "nsError.h"
#include "nsINode.h"

using namespace mozilla;

nsDOMSerializer::nsDOMSerializer()
{
}

nsDOMSerializer::~nsDOMSerializer()
{
}

// QueryInterface implementation for nsDOMSerializer
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsDOMSerializer)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSerializer)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(nsDOMSerializer, mOwner)

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsDOMSerializer)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsDOMSerializer)


static nsresult
SetUpEncoder(nsIDOMNode *aRoot, const nsACString& aCharset,
             nsIDocumentEncoder **aEncoder)
{
  *aEncoder = nullptr;

  nsresult rv;
  nsCOMPtr<nsIDocumentEncoder> encoder =
    do_CreateInstance(NS_DOC_ENCODER_CONTRACTID_BASE "application/xhtml+xml", &rv);
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsINode> root = do_QueryInterface(aRoot);
  MOZ_ASSERT(root);

  nsIDocument* doc = root->OwnerDoc();
  bool entireDocument = (doc == root);
  nsCOMPtr<nsIDOMDocument> domDoc(do_QueryInterface(doc));

  // This method will fail if no document
  rv = encoder->Init(domDoc, NS_LITERAL_STRING("application/xhtml+xml"),
                     nsIDocumentEncoder::OutputRaw |
                     nsIDocumentEncoder::OutputDontRewriteEncodingDeclaration);

  if (NS_FAILED(rv))
    return rv;

  nsAutoCString charset(aCharset);
  if (charset.IsEmpty()) {
    nsCOMPtr<nsIDocument> doc = do_QueryInterface(domDoc);
    NS_ASSERTION(doc, "Need a document");
    doc->GetDocumentCharacterSet()->Name(charset);
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
    encoder.forget(aEncoder);
  }

  return rv;
}

void
nsDOMSerializer::SerializeToString(nsINode& aRoot, nsAString& aStr,
                                   ErrorResult& rv)
{
  rv = nsDOMSerializer::SerializeToString(aRoot.AsDOMNode(), aStr);
}

NS_IMETHODIMP
nsDOMSerializer::SerializeToString(nsIDOMNode *aRoot, nsAString& _retval)
{
  NS_ENSURE_ARG_POINTER(aRoot);

  _retval.Truncate();

  if (!nsContentUtils::CanCallerAccess(aRoot)) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  nsCOMPtr<nsIDocumentEncoder> encoder;
  nsresult rv = SetUpEncoder(aRoot, EmptyCString(), getter_AddRefs(encoder));
  if (NS_FAILED(rv))
    return rv;

  return encoder->EncodeToString(_retval);
}

void
nsDOMSerializer::SerializeToStream(nsINode& aRoot, nsIOutputStream* aStream,
                                   const nsAString& aCharset,
                                   ErrorResult& aRv)
{
  if (NS_WARN_IF(!aStream)) {
    aRv.Throw(NS_ERROR_INVALID_ARG);
    return;
  }

  // The charset arg can be empty, in which case we get the document's
  // charset and use that when serializing.

  // No point doing a CanCallerAccess check, because we can only be
  // called by system JS or C++.
  nsCOMPtr<nsIDocumentEncoder> encoder;
  nsresult rv = SetUpEncoder(aRoot.AsDOMNode(), NS_ConvertUTF16toUTF8(aCharset),
                             getter_AddRefs(encoder));
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return;
  }

  rv = encoder->EncodeToStream(aStream);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
  }
}
