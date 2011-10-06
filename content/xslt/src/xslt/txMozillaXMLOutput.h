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

#ifndef TRANSFRMX_MOZILLA_XML_OUTPUT_H
#define TRANSFRMX_MOZILLA_XML_OUTPUT_H

#include "txXMLEventHandler.h"
#include "nsAutoPtr.h"
#include "nsIScriptLoaderObserver.h"
#include "txOutputFormat.h"
#include "nsCOMArray.h"
#include "nsICSSLoaderObserver.h"
#include "txStack.h"

class nsIContent;
class nsIDOMDocument;
class nsIAtom;
class nsIDOMDocumentFragment;
class nsIDOMElement;
class nsIStyleSheet;
class nsIDOMNode;
class nsITransformObserver;
class nsNodeInfoManager;
class nsIDocument;
class nsINode;

class txTransformNotifier : public nsIScriptLoaderObserver,
                            public nsICSSLoaderObserver
{
public:
    txTransformNotifier();

    NS_DECL_ISUPPORTS
    NS_DECL_NSISCRIPTLOADEROBSERVER
    
    // nsICSSLoaderObserver
    NS_IMETHOD StyleSheetLoaded(nsCSSStyleSheet* aSheet,
                                bool aWasAlternate,
                                nsresult aStatus);

    void Init(nsITransformObserver* aObserver);
    nsresult AddScriptElement(nsIScriptElement* aElement);
    void AddPendingStylesheet();
    void OnTransformEnd(nsresult aResult = NS_OK);
    void OnTransformStart();
    nsresult SetOutputDocument(nsIDocument* aDocument);

private:
    void SignalTransformEnd(nsresult aResult = NS_OK);

    nsCOMPtr<nsIDocument> mDocument;
    nsCOMPtr<nsITransformObserver> mObserver;
    nsCOMArray<nsIScriptElement> mScriptElements;
    PRUint32 mPendingStylesheetCount;
    bool mInTransform;
};

class txMozillaXMLOutput : public txAOutputXMLEventHandler
{
public:
    txMozillaXMLOutput(txOutputFormat* aFormat,
                       nsITransformObserver* aObserver);
    txMozillaXMLOutput(txOutputFormat* aFormat,
                       nsIDOMDocumentFragment* aFragment,
                       bool aNoFixup);
    ~txMozillaXMLOutput();

    TX_DECL_TXAXMLEVENTHANDLER
    TX_DECL_TXAOUTPUTXMLEVENTHANDLER

    nsresult closePrevious(bool aFlushText);

    nsresult createResultDocument(const nsSubstring& aName, PRInt32 aNsID,
                                  nsIDOMDocument* aSourceDocument);

private:
    nsresult createTxWrapper();
    nsresult startHTMLElement(nsIContent* aElement, bool aXHTML);
    nsresult endHTMLElement(nsIContent* aElement);
    void processHTTPEquiv(nsIAtom* aHeader, const nsString& aValue);
    nsresult createHTMLElement(nsIAtom* aName,
                               nsIContent** aResult);

    nsresult attributeInternal(nsIAtom* aPrefix, nsIAtom* aLocalName,
                               PRInt32 aNsID, const nsString& aValue);
    nsresult startElementInternal(nsIAtom* aPrefix, nsIAtom* aLocalName,
                                  PRInt32 aNsID);

    nsCOMPtr<nsIDocument> mDocument;
    nsCOMPtr<nsINode> mCurrentNode;     // This is updated once an element is
                                        // 'closed' (i.e. once we're done
                                        // adding attributes to it).
                                        // until then the opened element is
                                        // kept in mOpenedElement
    nsCOMPtr<nsIContent> mOpenedElement;
    nsRefPtr<nsNodeInfoManager> mNodeInfoManager;

    nsCOMArray<nsINode> mCurrentNodeStack;

    nsCOMPtr<nsIContent> mNonAddedNode;

    nsRefPtr<txTransformNotifier> mNotifier;

    PRUint32 mTreeDepth, mBadChildLevel;
    nsCString mRefreshString;

    txStack mTableStateStack;
    enum TableState {
        NORMAL,      // An element needing no special treatment
        TABLE,       // A HTML table element
        ADDED_TBODY  // An inserted tbody not coming from the stylesheet
    };
    TableState mTableState;

    nsAutoString mText;

    txOutputFormat mOutputFormat;

    bool mCreatingNewDocument;

    bool mOpenedElementIsHTML;

    // Set to true when we know there's a root content in our document.
    bool mRootContentCreated;

    bool mNoFixup;

    enum txAction { eCloseElement = 1, eFlushText = 2 };
};

#endif
