/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TRANSFRMX_MOZILLA_XML_OUTPUT_H
#define TRANSFRMX_MOZILLA_XML_OUTPUT_H

#include "txXMLEventHandler.h"
#include "nsIScriptLoaderObserver.h"
#include "txOutputFormat.h"
#include "nsTArray.h"
#include "nsCOMPtr.h"
#include "nsICSSLoaderObserver.h"
#include "txStack.h"
#include "mozilla/Attributes.h"

class nsIContent;
class nsAtom;
class nsITransformObserver;
class nsNodeInfoManager;
class nsINode;

namespace mozilla {
namespace dom {
class Document;
class DocumentFragment;
class Element;
}  // namespace dom
}  // namespace mozilla

class txTransformNotifier final : public nsIScriptLoaderObserver,
                                  public nsICSSLoaderObserver {
 public:
  txTransformNotifier();

  NS_DECL_ISUPPORTS
  NS_DECL_NSISCRIPTLOADEROBSERVER

  // nsICSSLoaderObserver
  NS_IMETHOD StyleSheetLoaded(mozilla::StyleSheet* aSheet, bool aWasDeferred,
                              nsresult aStatus) override;

  void Init(nsITransformObserver* aObserver);
  void AddScriptElement(nsIScriptElement* aElement);
  void AddPendingStylesheet();
  void OnTransformEnd(nsresult aResult = NS_OK);
  void OnTransformStart();
  nsresult SetOutputDocument(mozilla::dom::Document* aDocument);

 private:
  ~txTransformNotifier();
  void SignalTransformEnd(nsresult aResult = NS_OK);

  nsCOMPtr<mozilla::dom::Document> mDocument;
  nsCOMPtr<nsITransformObserver> mObserver;
  nsTArray<nsCOMPtr<nsIScriptElement>> mScriptElements;
  uint32_t mPendingStylesheetCount;
  bool mInTransform;
};

class txMozillaXMLOutput : public txAOutputXMLEventHandler {
 public:
  txMozillaXMLOutput(txOutputFormat* aFormat, nsITransformObserver* aObserver);
  txMozillaXMLOutput(txOutputFormat* aFormat,
                     mozilla::dom::DocumentFragment* aFragment, bool aNoFixup);
  ~txMozillaXMLOutput();

  TX_DECL_TXAXMLEVENTHANDLER
  TX_DECL_TXAOUTPUTXMLEVENTHANDLER

  nsresult closePrevious(bool aFlushText);

  nsresult createResultDocument(const nsAString& aName, int32_t aNsID,
                                mozilla::dom::Document* aSourceDocument,
                                bool aLoadedAsData);

 private:
  nsresult createTxWrapper();
  nsresult startHTMLElement(nsIContent* aElement, bool aXHTML);
  void endHTMLElement(nsIContent* aElement);
  nsresult createHTMLElement(nsAtom* aName, mozilla::dom::Element** aResult);

  nsresult attributeInternal(nsAtom* aPrefix, nsAtom* aLocalName, int32_t aNsID,
                             const nsString& aValue);
  nsresult startElementInternal(nsAtom* aPrefix, nsAtom* aLocalName,
                                int32_t aNsID);

  RefPtr<mozilla::dom::Document> mDocument;
  nsCOMPtr<nsINode> mCurrentNode;  // This is updated once an element is
                                   // 'closed' (i.e. once we're done
                                   // adding attributes to it).
                                   // until then the opened element is
                                   // kept in mOpenedElement
  nsCOMPtr<mozilla::dom::Element> mOpenedElement;
  RefPtr<nsNodeInfoManager> mNodeInfoManager;

  nsTArray<nsCOMPtr<nsINode>> mCurrentNodeStack;

  nsCOMPtr<nsIContent> mNonAddedNode;

  RefPtr<txTransformNotifier> mNotifier;

  uint32_t mTreeDepth, mBadChildLevel;

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
