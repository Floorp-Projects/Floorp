/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "txMozillaTextOutput.h"
#include "nsContentCID.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMDocumentFragment.h"
#include "nsIDocumentTransformer.h"
#include "nsCharsetSource.h"
#include "nsIPrincipal.h"
#include "txURIUtils.h"
#include "nsContentCreatorFunctions.h"
#include "nsContentUtils.h"
#include "nsGkAtoms.h"
#include "mozilla/Encoding.h"
#include "nsTextNode.h"
#include "nsNameSpaceManager.h"

using namespace mozilla::dom;

txMozillaTextOutput::txMozillaTextOutput(nsITransformObserver* aObserver)
{
    MOZ_COUNT_CTOR(txMozillaTextOutput);
    mObserver = do_GetWeakReference(aObserver);
}

txMozillaTextOutput::txMozillaTextOutput(nsIDOMDocumentFragment* aDest)
{
    MOZ_COUNT_CTOR(txMozillaTextOutput);
    mTextParent = do_QueryInterface(aDest);
    mDocument = mTextParent->OwnerDoc();
}

txMozillaTextOutput::~txMozillaTextOutput()
{
    MOZ_COUNT_DTOR(txMozillaTextOutput);
}

nsresult
txMozillaTextOutput::attribute(nsIAtom* aPrefix, nsIAtom* aLocalName,
                               nsIAtom* aLowercaseLocalName,
                               int32_t aNsID, const nsString& aValue)
{
    return NS_OK;
}

nsresult
txMozillaTextOutput::attribute(nsIAtom* aPrefix, const nsSubstring& aName,
                               const int32_t aNsID,
                               const nsString& aValue)
{
    return NS_OK;
}

nsresult
txMozillaTextOutput::characters(const nsSubstring& aData, bool aDOE)
{
    mText.Append(aData);

    return NS_OK;
}

nsresult
txMozillaTextOutput::comment(const nsString& aData)
{
    return NS_OK;
}

nsresult
txMozillaTextOutput::endDocument(nsresult aResult)
{
    NS_ENSURE_TRUE(mDocument && mTextParent, NS_ERROR_FAILURE);

    RefPtr<nsTextNode> text = new nsTextNode(mDocument->NodeInfoManager());
    
    text->SetText(mText, false);
    nsresult rv = mTextParent->AppendChildTo(text, true);
    NS_ENSURE_SUCCESS(rv, rv);

    // This should really be handled by nsIDocument::EndLoad
    MOZ_ASSERT(mDocument->GetReadyStateEnum() ==
               nsIDocument::READYSTATE_LOADING, "Bad readyState");
    mDocument->SetReadyStateInternal(nsIDocument::READYSTATE_INTERACTIVE);

    if (NS_SUCCEEDED(aResult)) {
        nsCOMPtr<nsITransformObserver> observer = do_QueryReferent(mObserver);
        if (observer) {
            observer->OnTransformDone(aResult, mDocument);
        }
    }

    return NS_OK;
}

nsresult
txMozillaTextOutput::endElement()
{
    return NS_OK;
}

nsresult
txMozillaTextOutput::processingInstruction(const nsString& aTarget,
                                           const nsString& aData)
{
    return NS_OK;
}

nsresult
txMozillaTextOutput::startDocument()
{
    return NS_OK;
}

nsresult
txMozillaTextOutput::createResultDocument(nsIDOMDocument* aSourceDocument,
                                          bool aLoadedAsData)
{
    /*
     * Create an XHTML document to hold the text.
     *
     * <html>
     *   <head />
     *   <body>
     *     <pre id="transformiixResult"> * The text comes here * </pre>
     *   <body>
     * </html>
     *
     * Except if we are transforming into a non-displayed document we create
     * the following DOM
     *
     * <transformiix:result> * The text comes here * </transformiix:result>
     */

    // Create the document
    nsresult rv = NS_NewXMLDocument(getter_AddRefs(mDocument),
                                    aLoadedAsData);
    NS_ENSURE_SUCCESS(rv, rv);
    // This should really be handled by nsIDocument::BeginLoad
    MOZ_ASSERT(mDocument->GetReadyStateEnum() ==
               nsIDocument::READYSTATE_UNINITIALIZED, "Bad readyState");
    mDocument->SetReadyStateInternal(nsIDocument::READYSTATE_LOADING);
    nsCOMPtr<nsIDocument> source = do_QueryInterface(aSourceDocument);
    NS_ENSURE_STATE(source);
    bool hasHadScriptObject = false;
    nsIScriptGlobalObject* sgo =
      source->GetScriptHandlingObject(hasHadScriptObject);
    NS_ENSURE_STATE(sgo || !hasHadScriptObject);

    NS_ASSERTION(mDocument, "Need document");

    // Reset and set up document
    URIUtils::ResetWithSource(mDocument, aSourceDocument);
    // Only do this after resetting the document to ensure we have the
    // correct principal.
    mDocument->SetScriptHandlingObject(sgo);

    // Set the charset
    if (!mOutputFormat.mEncoding.IsEmpty()) {
        const Encoding* encoding = Encoding::ForLabel(mOutputFormat.mEncoding);
        if (encoding) {
            mDocument->SetDocumentCharacterSetSource(kCharsetFromOtherComponent);
            nsAutoCString canonicalCharset;
            encoding->Name(canonicalCharset);
            mDocument->SetDocumentCharacterSet(canonicalCharset);
        }
    }

    // Notify the contentsink that the document is created
    nsCOMPtr<nsITransformObserver> observer = do_QueryReferent(mObserver);
    if (observer) {
        rv = observer->OnDocumentCreated(mDocument);
        NS_ENSURE_SUCCESS(rv, rv);
    }

    // Create the content

    // When transforming into a non-displayed document (i.e. when there is no
    // observer) we only create a transformiix:result root element.
    if (!observer) {
        int32_t namespaceID;
        rv = nsContentUtils::NameSpaceManager()->
            RegisterNameSpace(NS_LITERAL_STRING(kTXNameSpaceURI), namespaceID);
        NS_ENSURE_SUCCESS(rv, rv);

        mTextParent =
          mDocument->CreateElem(nsDependentAtomString(nsGkAtoms::result),
                                nsGkAtoms::transformiix, namespaceID);


        rv = mDocument->AppendChildTo(mTextParent, true);
        NS_ENSURE_SUCCESS(rv, rv);
    }
    else {
        nsCOMPtr<nsIContent> html, head, body;
        rv = createXHTMLElement(nsGkAtoms::html, getter_AddRefs(html));
        NS_ENSURE_SUCCESS(rv, rv);

        rv = createXHTMLElement(nsGkAtoms::head, getter_AddRefs(head));
        NS_ENSURE_SUCCESS(rv, rv);

        rv = html->AppendChildTo(head, false);
        NS_ENSURE_SUCCESS(rv, rv);

        rv = createXHTMLElement(nsGkAtoms::body, getter_AddRefs(body));
        NS_ENSURE_SUCCESS(rv, rv);

        rv = html->AppendChildTo(body, false);
        NS_ENSURE_SUCCESS(rv, rv);

        rv = createXHTMLElement(nsGkAtoms::pre, getter_AddRefs(mTextParent));
        NS_ENSURE_SUCCESS(rv, rv);

        rv = mTextParent->SetAttr(kNameSpaceID_None, nsGkAtoms::id,
                                  NS_LITERAL_STRING("transformiixResult"),
                                  false);
        NS_ENSURE_SUCCESS(rv, rv);

        rv = body->AppendChildTo(mTextParent, false);
        NS_ENSURE_SUCCESS(rv, rv);

        rv = mDocument->AppendChildTo(html, true);
        NS_ENSURE_SUCCESS(rv, rv);
    }

    return NS_OK;
}

nsresult
txMozillaTextOutput::startElement(nsIAtom* aPrefix, nsIAtom* aLocalName,
                                  nsIAtom* aLowercaseLocalName, int32_t aNsID)
{
    return NS_OK;
}

nsresult
txMozillaTextOutput::startElement(nsIAtom* aPrefix, const nsSubstring& aName,
                                  const int32_t aNsID)
{
    return NS_OK;
}

void txMozillaTextOutput::getOutputDocument(nsIDOMDocument** aDocument)
{
    CallQueryInterface(mDocument, aDocument);
}

nsresult
txMozillaTextOutput::createXHTMLElement(nsIAtom* aName,
                                        nsIContent** aResult)
{
    nsCOMPtr<Element> element = mDocument->CreateHTMLElement(aName);
    element.forget(aResult);
    return NS_OK;
}
