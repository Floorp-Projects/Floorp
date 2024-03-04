/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDOMSerializer.h"

#include "mozilla/Encoding.h"
#include "mozilla/dom/Document.h"
#include "nsIDocumentEncoder.h"
#include "nsComponentManagerUtils.h"
#include "nsContentUtils.h"
#include "nsError.h"
#include "nsINode.h"

using namespace mozilla;

nsDOMSerializer::nsDOMSerializer() = default;

static already_AddRefed<nsIDocumentEncoder> SetUpEncoder(
    nsINode& aRoot, const nsAString& aCharset, ErrorResult& aRv) {
  nsCOMPtr<nsIDocumentEncoder> encoder =
      do_createDocumentEncoder("application/xhtml+xml");
  if (!encoder) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  dom::Document* doc = aRoot.OwnerDoc();
  bool entireDocument = (doc == &aRoot);

  // This method will fail if no document
  nsresult rv = encoder->NativeInit(
      doc, u"application/xhtml+xml"_ns,
      nsIDocumentEncoder::OutputRaw |
          nsIDocumentEncoder::OutputDontRewriteEncodingDeclaration);

  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  NS_ConvertUTF16toUTF8 charset(aCharset);
  if (charset.IsEmpty()) {
    doc->GetDocumentCharacterSet()->Name(charset);
  }
  rv = encoder->SetCharset(charset);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  // If we are working on the entire document we do not need to
  // specify which part to serialize
  if (!entireDocument) {
    rv = encoder->SetNode(&aRoot);
  }

  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  return encoder.forget();
}

void nsDOMSerializer::SerializeToString(nsINode& aRoot, nsAString& aStr,
                                        ErrorResult& aRv) {
  aStr.Truncate();

  if (!nsContentUtils::CanCallerAccess(&aRoot)) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  nsCOMPtr<nsIDocumentEncoder> encoder = SetUpEncoder(aRoot, u""_ns, aRv);
  if (aRv.Failed()) {
    return;
  }

  nsresult rv = encoder->EncodeToString(aStr);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
  }
}

void nsDOMSerializer::SerializeToStream(nsINode& aRoot,
                                        nsIOutputStream* aStream,
                                        const nsAString& aCharset,
                                        ErrorResult& aRv) {
  if (NS_WARN_IF(!aStream)) {
    aRv.Throw(NS_ERROR_INVALID_ARG);
    return;
  }

  // The charset arg can be empty, in which case we get the document's
  // charset and use that when serializing.

  // No point doing a CanCallerAccess check, because we can only be
  // called by system JS or C++.
  nsCOMPtr<nsIDocumentEncoder> encoder = SetUpEncoder(aRoot, aCharset, aRv);
  if (aRv.Failed()) {
    return;
  }

  nsresult rv = encoder->EncodeToStream(aStream);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
  }
}
