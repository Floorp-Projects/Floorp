/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TRANSFRMX_MOZILLA_XML_OUTPUT_H
#define TRANSFRMX_MOZILLA_XML_OUTPUT_H

#include "txXMLEventHandler.h"
#include "nsAutoPtr.h"
#include "nsIScriptLoaderObserver.h"
#include "txOutputFormat.h"
#include "nsCOMArray.h"
#include "nsICSSLoaderObserver.h"
#include "txStack.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/Element.h"

class nsIContent;
class nsIDOMDocument;
class nsIAtom;
class nsIDOMDocumentFragment;
class nsITransformObserver;
class nsNodeInfoManager;
class nsIDocument;
class nsINode;

class txTransformNotifier final : public nsIScriptLoaderObserver,
                                  public nsICSSLoaderObserver
{
public:
    txTransformNotifier();

    NS_DECL_ISUPPORTS
    NS_DECL_NSISCRIPTLOADEROBSERVER
    
    // nsICSSLoaderObserver
    NS_IMETHOD StyleSheetLoaded(mozilla::StyleSheet* aSheet,
                                bool aWasAlternate,
                                nsresult aStatus) override;

    void Init(nsITransformObserver* aObserver);
    nsresult AddScriptElement(nsIScriptElement* aElement);
    void AddPendingStylesheet();
    void OnTransformEnd(nsresult aResult = NS_OK);
    void OnTransformStart();
    nsresult SetOutputDocument(nsIDocument* aDocument);

private:
    ~txTransformNotifier();
    void SignalTransformEnd(nsresult aResult = NS_OK);

    nsCOMPtr<nsIDocument> mDocument;
    nsCOMPtr<nsITransformObserver> mObserver;
    nsCOMArray<nsIScriptElement> mScriptElements;
    uint32_t mPendingStylesheetCount;
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

    nsresult createResultDocument(const nsAString& aName, int32_t aNsID,
                                  nsIDOMDocument* aSourceDocument,
                                  bool aLoadedAsData);

private:
    nsresult createTxWrapper();
    nsresult startHTMLElement(nsIContent* aElement, bool aXHTML);
    nsresult endHTMLElement(nsIContent* aElement);
    void processHTTPEquiv(nsIAtom* aHeader, const nsString& aValue);
    nsresult createHTMLElement(nsIAtom* aName,
                               nsIContent** aResult);

    nsresult attributeInternal(nsIAtom* aPrefix, nsIAtom* aLocalName,
                               int32_t aNsID, const nsString& aValue);
    nsresult startElementInternal(nsIAtom* aPrefix, nsIAtom* aLocalName,
                                  int32_t aNsID);

    nsCOMPtr<nsIDocument> mDocument;
    nsCOMPtr<nsINode> mCurrentNode;     // This is updated once an element is
                                        // 'closed' (i.e. once we're done
                                        // adding attributes to it).
                                        // until then the opened element is
                                        // kept in mOpenedElement
    nsCOMPtr<mozilla::dom::Element> mOpenedElement;
    RefPtr<nsNodeInfoManager> mNodeInfoManager;

    nsCOMArray<nsINode> mCurrentNodeStack;

    nsCOMPtr<nsIContent> mNonAddedNode;

    RefPtr<txTransformNotifier> mNotifier;

    uint32_t mTreeDepth, mBadChildLevel;
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
