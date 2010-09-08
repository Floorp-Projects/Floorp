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

#include "txMozillaXMLOutput.h"

#include "nsIDocument.h"
#include "nsIDocShell.h"
#include "nsScriptLoader.h"
#include "nsIDOMDocument.h"
#include "nsIDOMDocumentType.h"
#include "nsIScriptElement.h"
#include "nsIDOMNSDocument.h"
#include "nsIParser.h"
#include "nsIRefreshURI.h"
#include "nsPIDOMWindow.h"
#include "nsIContent.h"
#include "nsContentCID.h"
#include "nsNetUtil.h"
#include "nsUnicharUtils.h"
#include "txAtoms.h"
#include "txLog.h"
#include "nsIConsoleService.h"
#include "nsIDOMDocumentFragment.h"
#include "nsINameSpaceManager.h"
#include "nsCSSStyleSheet.h"
#include "txStringUtils.h"
#include "txURIUtils.h"
#include "nsIHTMLDocument.h"
#include "nsIStyleSheetLinkingElement.h"
#include "nsIDocumentTransformer.h"
#include "mozilla/css/Loader.h"
#include "nsICharsetAlias.h"
#include "nsIHTMLContentSink.h"
#include "nsContentUtils.h"
#include "txXMLUtils.h"
#include "nsContentSink.h"
#include "nsINode.h"
#include "nsContentCreatorFunctions.h"
#include "txError.h"

#define TX_ENSURE_CURRENTNODE                           \
    NS_ASSERTION(mCurrentNode, "mCurrentNode is NULL"); \
    if (!mCurrentNode)                                  \
        return NS_ERROR_UNEXPECTED

txMozillaXMLOutput::txMozillaXMLOutput(const nsSubstring& aRootName,
                                       PRInt32 aRootNsID,
                                       txOutputFormat* aFormat,
                                       nsIDOMDocument* aSourceDocument,
                                       nsIDOMDocument* aResultDocument,
                                       nsITransformObserver* aObserver)
    : mTreeDepth(0),
      mBadChildLevel(0),
      mTableState(NORMAL),
      mCreatingNewDocument(PR_TRUE),
      mOpenedElementIsHTML(PR_FALSE),
      mRootContentCreated(PR_FALSE),
      mNoFixup(PR_FALSE)
{
    if (aObserver) {
        mNotifier = new txTransformNotifier();
        if (mNotifier) {
            mNotifier->Init(aObserver);
        }
    }

    mOutputFormat.merge(*aFormat);
    mOutputFormat.setFromDefaults();

    createResultDocument(aRootName, aRootNsID, aSourceDocument, aResultDocument);
}

txMozillaXMLOutput::txMozillaXMLOutput(txOutputFormat* aFormat,
                                       nsIDOMDocumentFragment* aFragment,
                                       PRBool aNoFixup)
    : mTreeDepth(0),
      mBadChildLevel(0),
      mTableState(NORMAL),
      mCreatingNewDocument(PR_FALSE),
      mOpenedElementIsHTML(PR_FALSE),
      mRootContentCreated(PR_FALSE),
      mNoFixup(aNoFixup)
{
    mOutputFormat.merge(*aFormat);
    mOutputFormat.setFromDefaults();

    mCurrentNode = do_QueryInterface(aFragment);
    mDocument = mCurrentNode->GetOwnerDoc();
    if (mDocument) {
      mNodeInfoManager = mDocument->NodeInfoManager();
    }
    else {
      mCurrentNode = nsnull;
    }
}

nsresult
txMozillaXMLOutput::attribute(nsIAtom* aPrefix,
                              nsIAtom* aLocalName,
                              nsIAtom* aLowercaseLocalName,
                              const PRInt32 aNsID,
                              const nsString& aValue)
{
    nsCOMPtr<nsIAtom> owner;
    if (mOpenedElementIsHTML && aNsID == kNameSpaceID_None) {
        if (aLowercaseLocalName) {
            aLocalName = aLowercaseLocalName;
        }
        else {
            owner = TX_ToLowerCaseAtom(aLocalName);
            NS_ENSURE_TRUE(owner, NS_ERROR_OUT_OF_MEMORY);

            aLocalName = owner;
        }
    }

    return attributeInternal(aPrefix, aLocalName, aNsID, aValue);
}

nsresult
txMozillaXMLOutput::attribute(nsIAtom* aPrefix,
                              const nsSubstring& aLocalName,
                              const PRInt32 aNsID,
                              const nsString& aValue)
{
    nsCOMPtr<nsIAtom> lname;

    if (mOpenedElementIsHTML && aNsID == kNameSpaceID_None) {
        nsAutoString lnameStr;
        ToLowerCase(aLocalName, lnameStr);
        lname = do_GetAtom(lnameStr);
    }
    else {
        lname = do_GetAtom(aLocalName);
    }

    NS_ENSURE_TRUE(lname, NS_ERROR_OUT_OF_MEMORY);

    // Check that it's a valid name
    if (!nsContentUtils::IsValidNodeName(lname, aPrefix, aNsID)) {
        // Try without prefix
        aPrefix = nsnull;
        if (!nsContentUtils::IsValidNodeName(lname, aPrefix, aNsID)) {
            // Don't return error here since the callers don't deal
            return NS_OK;
        }
    }

    return attributeInternal(aPrefix, lname, aNsID, aValue);
}

nsresult
txMozillaXMLOutput::attributeInternal(nsIAtom* aPrefix,
                                      nsIAtom* aLocalName,
                                      PRInt32 aNsID,
                                      const nsString& aValue)
{
    if (!mOpenedElement) {
        // XXX Signal this? (can't add attributes after element closed)
        return NS_OK;
    }

    NS_ASSERTION(!mBadChildLevel, "mBadChildLevel set when element is opened");

    return mOpenedElement->SetAttr(aNsID, aLocalName, aPrefix, aValue,
                                   PR_FALSE);
}

nsresult
txMozillaXMLOutput::characters(const nsSubstring& aData, PRBool aDOE)
{
    nsresult rv = closePrevious(PR_FALSE);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!mBadChildLevel) {
        mText.Append(aData);
    }

    return NS_OK;
}

nsresult
txMozillaXMLOutput::comment(const nsString& aData)
{
    nsresult rv = closePrevious(PR_TRUE);
    NS_ENSURE_SUCCESS(rv, rv);

    if (mBadChildLevel) {
        return NS_OK;
    }

    TX_ENSURE_CURRENTNODE;

    nsCOMPtr<nsIContent> comment;
    rv = NS_NewCommentNode(getter_AddRefs(comment), mNodeInfoManager);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = comment->SetText(aData, PR_FALSE);
    NS_ENSURE_SUCCESS(rv, rv);

    return mCurrentNode->AppendChildTo(comment, PR_TRUE);
}

nsresult
txMozillaXMLOutput::endDocument(nsresult aResult)
{
    TX_ENSURE_CURRENTNODE;

    if (NS_FAILED(aResult)) {
        if (mNotifier) {
            mNotifier->OnTransformEnd(aResult);
        }
        
        return NS_OK;
    }

    nsresult rv = closePrevious(PR_TRUE);
    if (NS_FAILED(rv)) {
        if (mNotifier) {
            mNotifier->OnTransformEnd(rv);
        }
        
        return rv;
    }
    // This should really be handled by nsIDocument::EndLoad
    mDocument->SetReadyStateInternal(nsIDocument::READYSTATE_INTERACTIVE);

    if (!mRefreshString.IsEmpty()) {
        nsPIDOMWindow *win = mDocument->GetWindow();
        if (win) {
            nsCOMPtr<nsIRefreshURI> refURI =
                do_QueryInterface(win->GetDocShell());
            if (refURI) {
                refURI->SetupRefreshURIFromHeader(mDocument->GetDocBaseURI(),
                                                  mRefreshString);
            }
        }
    }

    if (mNotifier) {
        mNotifier->OnTransformEnd();
    }

    return NS_OK;
}

nsresult
txMozillaXMLOutput::endElement()
{
    TX_ENSURE_CURRENTNODE;

    if (mBadChildLevel) {
        --mBadChildLevel;
        PR_LOG(txLog::xslt, PR_LOG_DEBUG,
               ("endElement, mBadChildLevel = %d\n", mBadChildLevel));
        return NS_OK;
    }
    
    --mTreeDepth;

    nsresult rv = closePrevious(PR_TRUE);
    NS_ENSURE_SUCCESS(rv, rv);

    NS_ASSERTION(mCurrentNode->IsElement(), "borked mCurrentNode");
    NS_ENSURE_TRUE(mCurrentNode->IsElement(), NS_ERROR_UNEXPECTED);

    nsIContent* element = static_cast<nsIContent*>
                                     (static_cast<nsINode*>
                                                 (mCurrentNode));

    // Handle html-elements
    if (!mNoFixup) {
        if (element->IsHTML()) {
            rv = endHTMLElement(element);
            NS_ENSURE_SUCCESS(rv, rv);
        }

        // Handle script elements
        if (element->Tag() == nsGkAtoms::script &&
            (element->IsHTML() ||
            element->GetNameSpaceID() == kNameSpaceID_SVG)) {

            rv = element->DoneAddingChildren(PR_TRUE);

            // If the act of insertion evaluated the script, we're fine.
            // Else, add this script element to the array of loading scripts.
            if (rv == NS_ERROR_HTMLPARSER_BLOCK) {
                nsCOMPtr<nsIScriptElement> sele = do_QueryInterface(element);
                rv = mNotifier->AddScriptElement(sele);
                NS_ENSURE_SUCCESS(rv, rv);
            }
        }
    }

    if (mCreatingNewDocument) {
        // Handle all sorts of stylesheets
        nsCOMPtr<nsIStyleSheetLinkingElement> ssle =
            do_QueryInterface(mCurrentNode);
        if (ssle) {
            ssle->SetEnableUpdates(PR_TRUE);
            PRBool willNotify;
            PRBool isAlternate;
            nsresult rv = ssle->UpdateStyleSheet(mNotifier, &willNotify,
                                                 &isAlternate);
            if (mNotifier && NS_SUCCEEDED(rv) && willNotify && !isAlternate) {
                mNotifier->AddPendingStylesheet();
            }
        }
    }

    // Add the element to the tree if it wasn't added before and take one step
    // up the tree
    PRUint32 last = mCurrentNodeStack.Count() - 1;
    NS_ASSERTION(last != (PRUint32)-1, "empty stack");

    nsCOMPtr<nsINode> parent = mCurrentNodeStack.SafeObjectAt(last);
    mCurrentNodeStack.RemoveObjectAt(last);

    if (mCurrentNode == mNonAddedNode) {
        if (parent == mDocument) {
            NS_ASSERTION(!mRootContentCreated,
                         "Parent to add to shouldn't be a document if we "
                         "have a root content");
            mRootContentCreated = PR_TRUE;
        }

        // Check to make sure that script hasn't inserted the node somewhere
        // else in the tree
        if (!mCurrentNode->GetNodeParent()) {
            parent->AppendChildTo(mNonAddedNode, PR_TRUE);
        }
        mNonAddedNode = nsnull;
    }

    mCurrentNode = parent;

    mTableState =
        static_cast<TableState>(NS_PTR_TO_INT32(mTableStateStack.pop()));

    return NS_OK;
}

void txMozillaXMLOutput::getOutputDocument(nsIDOMDocument** aDocument)
{
    CallQueryInterface(mDocument, aDocument);
}

nsresult
txMozillaXMLOutput::processingInstruction(const nsString& aTarget, const nsString& aData)
{
    nsresult rv = closePrevious(PR_TRUE);
    NS_ENSURE_SUCCESS(rv, rv);

    if (mOutputFormat.mMethod == eHTMLOutput)
        return NS_OK;

    TX_ENSURE_CURRENTNODE;

    rv = nsContentUtils::CheckQName(aTarget, PR_FALSE);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIContent> pi;
    rv = NS_NewXMLProcessingInstruction(getter_AddRefs(pi),
                                        mNodeInfoManager, aTarget, aData);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIStyleSheetLinkingElement> ssle;
    if (mCreatingNewDocument) {
        ssle = do_QueryInterface(pi);
        if (ssle) {
            ssle->InitStyleLinkElement(PR_FALSE);
            ssle->SetEnableUpdates(PR_FALSE);
        }
    }

    rv = mCurrentNode->AppendChildTo(pi, PR_TRUE);
    NS_ENSURE_SUCCESS(rv, rv);

    if (ssle) {
        ssle->SetEnableUpdates(PR_TRUE);
        PRBool willNotify;
        PRBool isAlternate;
        rv = ssle->UpdateStyleSheet(mNotifier, &willNotify, &isAlternate);
        if (mNotifier && NS_SUCCEEDED(rv) && willNotify && !isAlternate) {
            mNotifier->AddPendingStylesheet();
        }
    }

    return NS_OK;
}

nsresult
txMozillaXMLOutput::startDocument()
{
    if (mNotifier) {
        mNotifier->OnTransformStart();
    }

    return NS_OK;
}

nsresult
txMozillaXMLOutput::startElement(nsIAtom* aPrefix, nsIAtom* aLocalName,
                                 nsIAtom* aLowercaseLocalName,
                                 const PRInt32 aNsID)
{
    NS_PRECONDITION(aNsID != kNameSpaceID_None || !aPrefix,
                    "Can't have prefix without namespace");

    if (mOutputFormat.mMethod == eHTMLOutput && aNsID == kNameSpaceID_None) {
        nsCOMPtr<nsIAtom> owner;
        if (!aLowercaseLocalName) {
            owner = TX_ToLowerCaseAtom(aLocalName);
            NS_ENSURE_TRUE(owner, NS_ERROR_OUT_OF_MEMORY);

            aLowercaseLocalName = owner;
        }
        return startElementInternal(nsnull, 
                                    aLowercaseLocalName, 
                                    kNameSpaceID_XHTML);
    }

    return startElementInternal(aPrefix, aLocalName, aNsID);
}

nsresult
txMozillaXMLOutput::startElement(nsIAtom* aPrefix,
                                 const nsSubstring& aLocalName,
                                 const PRInt32 aNsID)
{
    PRInt32 nsId = aNsID;
    nsCOMPtr<nsIAtom> lname;

    if (mOutputFormat.mMethod == eHTMLOutput && aNsID == kNameSpaceID_None) {
        nsId = kNameSpaceID_XHTML;

        nsAutoString lnameStr;
        ToLowerCase(aLocalName, lnameStr);
        lname = do_GetAtom(lnameStr);
    }
    else {
        lname = do_GetAtom(aLocalName);
    }

    // No biggie if we lose the prefix due to OOM
    NS_ENSURE_TRUE(lname, NS_ERROR_OUT_OF_MEMORY);

    // Check that it's a valid name
    if (!nsContentUtils::IsValidNodeName(lname, aPrefix, nsId)) {
        // Try without prefix
        aPrefix = nsnull;
        if (!nsContentUtils::IsValidNodeName(lname, aPrefix, nsId)) {
            return NS_ERROR_XSLT_BAD_NODE_NAME;
        }
    }

    return startElementInternal(aPrefix, lname, nsId);
}

nsresult
txMozillaXMLOutput::startElementInternal(nsIAtom* aPrefix,
                                         nsIAtom* aLocalName,
                                         PRInt32 aNsID)
{
    TX_ENSURE_CURRENTNODE;

    if (mBadChildLevel) {
        ++mBadChildLevel;
        PR_LOG(txLog::xslt, PR_LOG_DEBUG,
               ("startElement, mBadChildLevel = %d\n", mBadChildLevel));
        return NS_OK;
    }

    nsresult rv = closePrevious(PR_TRUE);
    NS_ENSURE_SUCCESS(rv, rv);

    // Push and init state
    if (mTreeDepth == MAX_REFLOW_DEPTH) {
        // eCloseElement couldn't add the parent so we fail as well or we've
        // reached the limit of the depth of the tree that we allow.
        ++mBadChildLevel;
        PR_LOG(txLog::xslt, PR_LOG_DEBUG,
               ("startElement, mBadChildLevel = %d\n", mBadChildLevel));
        return NS_OK;
    }

    ++mTreeDepth;

    rv = mTableStateStack.push(NS_INT32_TO_PTR(mTableState));
    NS_ENSURE_SUCCESS(rv, rv);

    if (!mCurrentNodeStack.AppendObject(mCurrentNode)) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    mTableState = NORMAL;
    mOpenedElementIsHTML = PR_FALSE;

    // Create the element
    nsCOMPtr<nsINodeInfo> ni;
    ni = mNodeInfoManager->GetNodeInfo(aLocalName, aPrefix, aNsID);
    NS_ENSURE_TRUE(ni, NS_ERROR_OUT_OF_MEMORY);

    NS_NewElement(getter_AddRefs(mOpenedElement), aNsID, ni.forget(), PR_FALSE);

    // Set up the element and adjust state
    if (!mNoFixup) {
        if (aNsID == kNameSpaceID_XHTML) {
            mOpenedElementIsHTML = (mOutputFormat.mMethod == eHTMLOutput);
            rv = startHTMLElement(mOpenedElement, mOpenedElementIsHTML);
            NS_ENSURE_SUCCESS(rv, rv);

        }
    }

    if (mCreatingNewDocument) {
        // Handle all sorts of stylesheets
        nsCOMPtr<nsIStyleSheetLinkingElement> ssle =
            do_QueryInterface(mOpenedElement);
        if (ssle) {
            ssle->InitStyleLinkElement(PR_FALSE);
            ssle->SetEnableUpdates(PR_FALSE);
        }
    }

    return NS_OK;
}

nsresult
txMozillaXMLOutput::closePrevious(PRBool aFlushText)
{
    TX_ENSURE_CURRENTNODE;

    nsresult rv;
    if (mOpenedElement) {
        PRBool currentIsDoc = mCurrentNode == mDocument;
        if (currentIsDoc && mRootContentCreated) {
            // We already have a document element, but the XSLT spec allows this.
            // As a workaround, create a wrapper object and use that as the
            // document element.
            
            rv = createTxWrapper();
            NS_ENSURE_SUCCESS(rv, rv);
        }

        rv = mCurrentNode->AppendChildTo(mOpenedElement, PR_TRUE);
        NS_ENSURE_SUCCESS(rv, rv);

        if (currentIsDoc) {
            mRootContentCreated = PR_TRUE;
            nsContentSink::NotifyDocElementCreated(mDocument);
        }

        mCurrentNode = mOpenedElement;
        mOpenedElement = nsnull;
    }
    else if (aFlushText && !mText.IsEmpty()) {
        // Text can't appear in the root of a document
        if (mDocument == mCurrentNode) {
            if (XMLUtils::isWhitespace(mText)) {
                mText.Truncate();
                
                return NS_OK;
            }

            rv = createTxWrapper();
            NS_ENSURE_SUCCESS(rv, rv);
        }
        nsCOMPtr<nsIContent> text;
        rv = NS_NewTextNode(getter_AddRefs(text), mNodeInfoManager);
        NS_ENSURE_SUCCESS(rv, rv);

        rv = text->SetText(mText, PR_FALSE);
        NS_ENSURE_SUCCESS(rv, rv);

        rv = mCurrentNode->AppendChildTo(text, PR_TRUE);
        NS_ENSURE_SUCCESS(rv, rv);

        mText.Truncate();
    }

    return NS_OK;
}

nsresult
txMozillaXMLOutput::createTxWrapper()
{
    NS_ASSERTION(mDocument == mCurrentNode,
                 "creating wrapper when document isn't parent");

    PRInt32 namespaceID;
    nsresult rv = nsContentUtils::NameSpaceManager()->
        RegisterNameSpace(NS_LITERAL_STRING(kTXNameSpaceURI), namespaceID);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIContent> wrapper;
    rv = mDocument->CreateElem(nsDependentAtomString(nsGkAtoms::result),
                               nsGkAtoms::transformiix, namespaceID,
                               PR_FALSE, getter_AddRefs(wrapper));
    NS_ENSURE_SUCCESS(rv, rv);

    PRUint32 i, j, childCount = mDocument->GetChildCount();
#ifdef DEBUG
    // Keep track of the location of the current documentElement, if there is
    // one, so we can verify later
    PRUint32 rootLocation = 0;
#endif
    for (i = 0, j = 0; i < childCount; ++i) {
        nsCOMPtr<nsIContent> childContent = mDocument->GetChildAt(j);

#ifdef DEBUG
        if (childContent->IsElement()) {
            rootLocation = j;
        }
#endif

        if (childContent->Tag() == nsGkAtoms::documentTypeNodeName) {
#ifdef DEBUG
            // The new documentElement should go after the document type.
            // This is needed for cases when there is no existing
            // documentElement in the document.
            rootLocation = PR_MAX(rootLocation, j + 1);
#endif
            ++j;
        }
        else {
            rv = mDocument->RemoveChildAt(j, PR_TRUE);
            NS_ENSURE_SUCCESS(rv, rv);

            rv = wrapper->AppendChildTo(childContent, PR_TRUE);
            NS_ENSURE_SUCCESS(rv, rv);
            break;
        }
    }

    if (!mCurrentNodeStack.AppendObject(wrapper)) {
        return NS_ERROR_OUT_OF_MEMORY;
    }
    mCurrentNode = wrapper;
    mRootContentCreated = PR_TRUE;
    NS_ASSERTION(rootLocation == mDocument->GetChildCount(),
                 "Incorrect root location");
    return mDocument->AppendChildTo(wrapper, PR_TRUE);
}

nsresult
txMozillaXMLOutput::startHTMLElement(nsIContent* aElement, PRBool aIsHTML)
{
    nsresult rv = NS_OK;
    nsIAtom *atom = aElement->Tag();

    if ((atom != txHTMLAtoms::tr || !aIsHTML) &&
        NS_PTR_TO_INT32(mTableStateStack.peek()) == ADDED_TBODY) {
        PRUint32 last = mCurrentNodeStack.Count() - 1;
        NS_ASSERTION(last != (PRUint32)-1, "empty stack");

        mCurrentNode = mCurrentNodeStack.SafeObjectAt(last);
        mCurrentNodeStack.RemoveObjectAt(last);
        mTableStateStack.pop();
    }

    if (atom == txHTMLAtoms::table && aIsHTML) {
        mTableState = TABLE;
    }
    else if (atom == txHTMLAtoms::tr && aIsHTML &&
             NS_PTR_TO_INT32(mTableStateStack.peek()) == TABLE) {
        nsCOMPtr<nsIContent> tbody;
        rv = createHTMLElement(nsGkAtoms::tbody, getter_AddRefs(tbody));
        NS_ENSURE_SUCCESS(rv, rv);

        rv = mCurrentNode->AppendChildTo(tbody, PR_TRUE);
        NS_ENSURE_SUCCESS(rv, rv);

        rv = mTableStateStack.push(NS_INT32_TO_PTR(ADDED_TBODY));
        NS_ENSURE_SUCCESS(rv, rv);

        if (!mCurrentNodeStack.AppendObject(tbody)) {
            return NS_ERROR_OUT_OF_MEMORY;
        }

        mCurrentNode = tbody;
    }
    else if (atom == txHTMLAtoms::head &&
             mOutputFormat.mMethod == eHTMLOutput) {
        // Insert META tag, according to spec, 16.2, like
        // <META http-equiv="Content-Type" content="text/html; charset=EUC-JP">
        nsCOMPtr<nsIContent> meta;
        rv = createHTMLElement(nsGkAtoms::meta, getter_AddRefs(meta));
        NS_ENSURE_SUCCESS(rv, rv);

        rv = meta->SetAttr(kNameSpaceID_None, nsGkAtoms::httpEquiv,
                           NS_LITERAL_STRING("Content-Type"), PR_FALSE);
        NS_ENSURE_SUCCESS(rv, rv);

        nsAutoString metacontent;
        metacontent.Append(mOutputFormat.mMediaType);
        metacontent.AppendLiteral("; charset=");
        metacontent.Append(mOutputFormat.mEncoding);
        rv = meta->SetAttr(kNameSpaceID_None, nsGkAtoms::content,
                           metacontent, PR_FALSE);
        NS_ENSURE_SUCCESS(rv, rv);

        // No need to notify since aElement hasn't been inserted yet
        NS_ASSERTION(!aElement->IsInDoc(), "should not be in doc");
        rv = aElement->AppendChildTo(meta, PR_FALSE);
        NS_ENSURE_SUCCESS(rv, rv);
    }

    return NS_OK;
}

nsresult
txMozillaXMLOutput::endHTMLElement(nsIContent* aElement)
{
    nsIAtom *atom = aElement->Tag();

    if (mTableState == ADDED_TBODY) {
        NS_ASSERTION(atom == txHTMLAtoms::tbody,
                     "Element flagged as added tbody isn't a tbody");
        PRUint32 last = mCurrentNodeStack.Count() - 1;
        NS_ASSERTION(last != (PRUint32)-1, "empty stack");

        mCurrentNode = mCurrentNodeStack.SafeObjectAt(last);
        mCurrentNodeStack.RemoveObjectAt(last);
        mTableState = static_cast<TableState>
                                 (NS_PTR_TO_INT32(mTableStateStack.pop()));

        return NS_OK;
    }
    else if (mCreatingNewDocument && atom == txHTMLAtoms::meta) {
        // handle HTTP-EQUIV data
        nsAutoString httpEquiv;
        aElement->GetAttr(kNameSpaceID_None, txHTMLAtoms::httpEquiv, httpEquiv);
        if (!httpEquiv.IsEmpty()) {
            nsAutoString value;
            aElement->GetAttr(kNameSpaceID_None, txHTMLAtoms::content, value);
            if (!value.IsEmpty()) {
                ToLowerCase(httpEquiv);
                nsCOMPtr<nsIAtom> header = do_GetAtom(httpEquiv);
                processHTTPEquiv(header, value);
            }
        }
    }
    
    return NS_OK;
}

void txMozillaXMLOutput::processHTTPEquiv(nsIAtom* aHeader, const nsString& aValue)
{
    // For now we only handle "refresh". There's a longer list in
    // HTMLContentSink::ProcessHeaderData
    if (aHeader == txHTMLAtoms::refresh)
        LossyCopyUTF16toASCII(aValue, mRefreshString);
}

nsresult
txMozillaXMLOutput::createResultDocument(const nsSubstring& aName, PRInt32 aNsID,
                                         nsIDOMDocument* aSourceDocument,
                                         nsIDOMDocument* aResultDocument)
{
    nsresult rv;

    if (!aResultDocument) {
        // Create the document
        if (mOutputFormat.mMethod == eHTMLOutput) {
            rv = NS_NewHTMLDocument(getter_AddRefs(mDocument));
            NS_ENSURE_SUCCESS(rv, rv);
        }
        else {
            // We should check the root name/namespace here and create the
            // appropriate document
            rv = NS_NewXMLDocument(getter_AddRefs(mDocument));
            NS_ENSURE_SUCCESS(rv, rv);
        }
        // This should really be handled by nsIDocument::BeginLoad
        mDocument->SetReadyStateInternal(nsIDocument::READYSTATE_LOADING);
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

    mCurrentNode = mDocument;
    mNodeInfoManager = mDocument->NodeInfoManager();

    // Reset and set up the document
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

    // Set the mime-type
    if (!mOutputFormat.mMediaType.IsEmpty()) {
        mDocument->SetContentType(mOutputFormat.mMediaType);
    }
    else if (mOutputFormat.mMethod == eHTMLOutput) {
        mDocument->SetContentType(NS_LITERAL_STRING("text/html"));
    }
    else {
        mDocument->SetContentType(NS_LITERAL_STRING("application/xml"));
    }

    if (mOutputFormat.mMethod == eXMLOutput &&
        mOutputFormat.mOmitXMLDeclaration != eTrue) {
        PRInt32 standalone;
        if (mOutputFormat.mStandalone == eNotSet) {
          standalone = -1;
        }
        else if (mOutputFormat.mStandalone == eFalse) {
          standalone = 0;
        }
        else {
          standalone = 1;
        }

        // Could use mOutputFormat.mVersion.get() when we support
        // versions > 1.0.
        static const PRUnichar kOneDotZero[] = { '1', '.', '0', '\0' };
        mDocument->SetXMLDeclaration(kOneDotZero, mOutputFormat.mEncoding.get(),
                                     standalone);
    }

    // Set up script loader of the result document.
    nsScriptLoader *loader = mDocument->ScriptLoader();
    if (mNotifier) {
        loader->AddObserver(mNotifier);
    }
    else {
        // Don't load scripts, we can't notify the caller when they're loaded.
        loader->SetEnabled(PR_FALSE);
    }

    if (mNotifier) {
        rv = mNotifier->SetOutputDocument(mDocument);
        NS_ENSURE_SUCCESS(rv, rv);
    }

    // Do this after calling OnDocumentCreated to ensure that the
    // PresShell/PresContext has been hooked up and get notified.
    nsCOMPtr<nsIHTMLDocument> htmlDoc = do_QueryInterface(mDocument);
    if (htmlDoc) {
        htmlDoc->SetCompatibilityMode(eCompatibility_FullStandards);
    }

    // Add a doc-type if requested
    if (!mOutputFormat.mSystemId.IsEmpty()) {
        nsAutoString qName;
        if (mOutputFormat.mMethod == eHTMLOutput) {
            qName.AssignLiteral("html");
        }
        else {
            qName.Assign(aName);
        }

        nsCOMPtr<nsIDOMDocumentType> documentType;

        nsresult rv = nsContentUtils::CheckQName(qName);
        if (NS_SUCCEEDED(rv)) {
            nsCOMPtr<nsIAtom> doctypeName = do_GetAtom(qName);
            if (!doctypeName) {
                return NS_ERROR_OUT_OF_MEMORY;
            }

            // Indicate that there is no internal subset (not just an empty one)
            nsAutoString voidString;
            voidString.SetIsVoid(PR_TRUE);
            rv = NS_NewDOMDocumentType(getter_AddRefs(documentType),
                                       mNodeInfoManager, nsnull,
                                       doctypeName, nsnull, nsnull,
                                       mOutputFormat.mPublicId,
                                       mOutputFormat.mSystemId,
                                       voidString);
            NS_ENSURE_SUCCESS(rv, rv);

            nsCOMPtr<nsIContent> docType = do_QueryInterface(documentType);
            rv = mDocument->AppendChildTo(docType, PR_TRUE);
            NS_ENSURE_SUCCESS(rv, rv);
        }
    }

    return NS_OK;
}

nsresult
txMozillaXMLOutput::createHTMLElement(nsIAtom* aName,
                                      nsIContent** aResult)
{
    NS_ASSERTION(mOutputFormat.mMethod == eHTMLOutput,
                 "need to adjust createHTMLElement");

    *aResult = nsnull;

    nsCOMPtr<nsINodeInfo> ni;
    ni = mNodeInfoManager->GetNodeInfo(aName, nsnull,
                                       kNameSpaceID_XHTML);
    NS_ENSURE_TRUE(ni, NS_ERROR_OUT_OF_MEMORY);

    return NS_NewHTMLElement(aResult, ni.forget(), PR_FALSE);
}

txTransformNotifier::txTransformNotifier()
    : mPendingStylesheetCount(0),
      mInTransform(PR_FALSE)      
{
}

NS_IMPL_ISUPPORTS2(txTransformNotifier,
                   nsIScriptLoaderObserver,
                   nsICSSLoaderObserver)

NS_IMETHODIMP
txTransformNotifier::ScriptAvailable(nsresult aResult, 
                                     nsIScriptElement *aElement, 
                                     PRBool aIsInline,
                                     nsIURI *aURI, 
                                     PRInt32 aLineNo)
{
    if (NS_FAILED(aResult) &&
        mScriptElements.RemoveObject(aElement)) {
        SignalTransformEnd();
    }

    return NS_OK;
}

NS_IMETHODIMP 
txTransformNotifier::ScriptEvaluated(nsresult aResult, 
                                     nsIScriptElement *aElement,
                                     PRBool aIsInline)
{
    if (mScriptElements.RemoveObject(aElement)) {
        SignalTransformEnd();
    }

    return NS_OK;
}

NS_IMETHODIMP 
txTransformNotifier::StyleSheetLoaded(nsCSSStyleSheet* aSheet,
                                      PRBool aWasAlternate,
                                      nsresult aStatus)
{
    if (mPendingStylesheetCount == 0) {
        // We weren't waiting on this stylesheet anyway.  This can happen if
        // SignalTransformEnd got called with an error aResult.  See
        // http://bugzilla.mozilla.org/show_bug.cgi?id=215465.
        return NS_OK;
    }

    // We're never waiting for alternate stylesheets
    if (!aWasAlternate) {
        --mPendingStylesheetCount;
        SignalTransformEnd();
    }
    
    return NS_OK;
}

void
txTransformNotifier::Init(nsITransformObserver* aObserver)
{
    mObserver = aObserver;
}

nsresult
txTransformNotifier::AddScriptElement(nsIScriptElement* aElement)
{
    return mScriptElements.AppendObject(aElement) ? NS_OK :
                                                    NS_ERROR_OUT_OF_MEMORY;
}

void
txTransformNotifier::AddPendingStylesheet()
{
    ++mPendingStylesheetCount;
}

void
txTransformNotifier::OnTransformEnd(nsresult aResult)
{
    mInTransform = PR_FALSE;
    SignalTransformEnd(aResult);
}

void
txTransformNotifier::OnTransformStart()
{
    mInTransform = PR_TRUE;
}

nsresult
txTransformNotifier::SetOutputDocument(nsIDocument* aDocument)
{
    mDocument = aDocument;

    // Notify the contentsink that the document is created
    return mObserver->OnDocumentCreated(mDocument);
}

void
txTransformNotifier::SignalTransformEnd(nsresult aResult)
{
    if (mInTransform || (NS_SUCCEEDED(aResult) &&
        mScriptElements.Count() > 0 || mPendingStylesheetCount > 0)) {
        return;
    }

    // mPendingStylesheetCount is nonzero at this point only if aResult is an
    // error.  Set it to 0 so we won't reenter this code when we stop the
    // CSSLoader.
    mPendingStylesheetCount = 0;
    mScriptElements.Clear();

    // Make sure that we don't get deleted while this function is executed and
    // we remove ourselfs from the scriptloader
    nsCOMPtr<nsIScriptLoaderObserver> kungFuDeathGrip(this);

    if (mDocument) {
        mDocument->ScriptLoader()->RemoveObserver(this);
        // XXX Maybe we want to cancel script loads if NS_FAILED(rv)?

        if (NS_FAILED(aResult)) {
            mDocument->CSSLoader()->Stop();
        }
    }

    if (NS_SUCCEEDED(aResult)) {
        mObserver->OnTransformDone(aResult, mDocument);
    }
}
