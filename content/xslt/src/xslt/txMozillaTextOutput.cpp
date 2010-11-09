/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is TransforMiiX XSLT processor code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Peter Van der Beken <peterv@propagandism.org>
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

#include "txMozillaTextOutput.h"
#include "nsContentCID.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMDocumentFragment.h"
#include "nsIDOMElement.h"
#include "nsIDOMHTMLElement.h"
#include "nsIDOMText.h"
#include "nsIDocumentTransformer.h"
#include "nsNetUtil.h"
#include "nsIDOMNSDocument.h"
#include "nsIParser.h"
#include "nsICharsetAlias.h"
#include "nsIPrincipal.h"
#include "txURIUtils.h"
#include "nsContentCreatorFunctions.h"
#include "nsContentUtils.h"
#include "nsGkAtoms.h"

using namespace mozilla::dom;

txMozillaTextOutput::txMozillaTextOutput(nsIDOMDocument* aSourceDocument,
                                         nsIDOMDocument* aResultDocument,
                                         nsITransformObserver* aObserver)
{
    mObserver = do_GetWeakReference(aObserver);
    createResultDocument(aSourceDocument, aResultDocument);
}

txMozillaTextOutput::txMozillaTextOutput(nsIDOMDocumentFragment* aDest)
{
    mTextParent = do_QueryInterface(aDest);
    mDocument = mTextParent->GetOwnerDoc();
}

nsresult
txMozillaTextOutput::attribute(nsIAtom* aPrefix, nsIAtom* aLocalName,
                               nsIAtom* aLowercaseLocalName,
                               PRInt32 aNsID, const nsString& aValue)
{
    return NS_OK;
}

nsresult
txMozillaTextOutput::attribute(nsIAtom* aPrefix, const nsSubstring& aName,
                               const PRInt32 aNsID,
                               const nsString& aValue)
{
    return NS_OK;
}

nsresult
txMozillaTextOutput::characters(const nsSubstring& aData, PRBool aDOE)
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

    nsCOMPtr<nsIContent> text;
    nsresult rv = NS_NewTextNode(getter_AddRefs(text),
                                 mDocument->NodeInfoManager());
    NS_ENSURE_SUCCESS(rv, rv);
    
    text->SetText(mText, PR_FALSE);
    rv = mTextParent->AppendChildTo(text, PR_TRUE);
    NS_ENSURE_SUCCESS(rv, rv);

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
                                          nsIDOMDocument* aResultDocument)
{
    nsresult rv = NS_OK;

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

    if (!aResultDocument) {
        // Create the document
        rv = NS_NewXMLDocument(getter_AddRefs(mDocument));
        NS_ENSURE_SUCCESS(rv, rv);
        nsCOMPtr<nsIDocument> source = do_QueryInterface(aSourceDocument);
        NS_ENSURE_STATE(source);
        PRBool hasHadScriptObject = PR_FALSE;
        nsIScriptGlobalObject* sgo =
          source->GetScriptHandlingObject(hasHadScriptObject);
        NS_ENSURE_STATE(sgo || !hasHadScriptObject);
        mDocument->SetScriptHandlingObject(sgo);
    }
    else {
        mDocument = do_QueryInterface(aResultDocument);
    }

    NS_ASSERTION(mDocument, "Need document");

    // Reset and set up document
    URIUtils::ResetWithSource(mDocument, aSourceDocument);

    // Set the charset
    if (!mOutputFormat.mEncoding.IsEmpty()) {
        NS_LossyConvertUTF16toASCII charset(mOutputFormat.mEncoding);
        nsCAutoString canonicalCharset;
        nsCOMPtr<nsICharsetAlias> calias =
            do_GetService("@mozilla.org/intl/charsetalias;1");

        if (calias &&
            NS_SUCCEEDED(calias->GetPreferred(charset, canonicalCharset))) {
            mDocument->SetDocumentCharacterSetSource(kCharsetFromOtherComponent);
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
    // Don't do this when called through nsIXSLTProcessorObsolete (i.e. when
    // aResultDocument is set) for compability reasons
    if (!aResultDocument && !observer) {
        PRInt32 namespaceID;
        rv = nsContentUtils::NameSpaceManager()->
            RegisterNameSpace(NS_LITERAL_STRING(kTXNameSpaceURI), namespaceID);
        NS_ENSURE_SUCCESS(rv, rv);

        rv = mDocument->CreateElem(nsDependentAtomString(nsGkAtoms::result),
                                   nsGkAtoms::transformiix, namespaceID,
                                   PR_FALSE, getter_AddRefs(mTextParent));
        NS_ENSURE_SUCCESS(rv, rv);


        rv = mDocument->AppendChildTo(mTextParent, PR_TRUE);
        NS_ENSURE_SUCCESS(rv, rv);
    }
    else {
        nsCOMPtr<nsIContent> html, head, body;
        rv = createXHTMLElement(nsGkAtoms::html, getter_AddRefs(html));
        NS_ENSURE_SUCCESS(rv, rv);

        rv = createXHTMLElement(nsGkAtoms::head, getter_AddRefs(head));
        NS_ENSURE_SUCCESS(rv, rv);

        rv = html->AppendChildTo(head, PR_FALSE);
        NS_ENSURE_SUCCESS(rv, rv);

        rv = createXHTMLElement(nsGkAtoms::body, getter_AddRefs(body));
        NS_ENSURE_SUCCESS(rv, rv);

        rv = html->AppendChildTo(body, PR_FALSE);
        NS_ENSURE_SUCCESS(rv, rv);

        rv = createXHTMLElement(nsGkAtoms::pre, getter_AddRefs(mTextParent));
        NS_ENSURE_SUCCESS(rv, rv);

        rv = mTextParent->SetAttr(kNameSpaceID_None, nsGkAtoms::id,
                                  NS_LITERAL_STRING("transformiixResult"),
                                  PR_FALSE);
        NS_ENSURE_SUCCESS(rv, rv);

        rv = body->AppendChildTo(mTextParent, PR_FALSE);
        NS_ENSURE_SUCCESS(rv, rv);

        rv = mDocument->AppendChildTo(html, PR_TRUE);
        NS_ENSURE_SUCCESS(rv, rv);
    }

    return NS_OK;
}

nsresult
txMozillaTextOutput::startElement(nsIAtom* aPrefix, nsIAtom* aLocalName,
                                  nsIAtom* aLowercaseLocalName, PRInt32 aNsID)
{
    return NS_OK;
}

nsresult
txMozillaTextOutput::startElement(nsIAtom* aPrefix, const nsSubstring& aName,
                                  const PRInt32 aNsID)
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
    *aResult = nsnull;

    nsCOMPtr<nsINodeInfo> ni;
    ni = mDocument->NodeInfoManager()->
        GetNodeInfo(aName, nsnull, kNameSpaceID_XHTML);
    NS_ENSURE_TRUE(ni, NS_ERROR_OUT_OF_MEMORY);

    return NS_NewHTMLElement(aResult, ni.forget(), NOT_FROM_PARSER);
}
