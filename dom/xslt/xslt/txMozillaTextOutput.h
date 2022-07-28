/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TRANSFRMX_MOZILLA_TEXT_OUTPUT_H
#define TRANSFRMX_MOZILLA_TEXT_OUTPUT_H

#include "txXMLEventHandler.h"
#include "nsCOMPtr.h"
#include "nsIWeakReferenceUtils.h"
#include "txOutputFormat.h"

class nsITransformObserver;
class nsIContent;

namespace mozilla::dom {
class Document;
class DocumentFragment;
class Element;
}  // namespace mozilla::dom

class txMozillaTextOutput : public txAOutputXMLEventHandler {
 public:
  explicit txMozillaTextOutput(mozilla::dom::Document* aSourceDocument,
                               nsITransformObserver* aObserver);
  explicit txMozillaTextOutput(mozilla::dom::DocumentFragment* aDest);
  virtual ~txMozillaTextOutput();

  TX_DECL_TXAXMLEVENTHANDLER
  TX_DECL_TXAOUTPUTXMLEVENTHANDLER

  nsresult createResultDocument(bool aLoadedAsData);

 private:
  nsresult createXHTMLElement(nsAtom* aName, mozilla::dom::Element** aResult);

  nsCOMPtr<mozilla::dom::Document> mSourceDocument;
  nsCOMPtr<nsIContent> mTextParent;
  nsWeakPtr mObserver;
  RefPtr<mozilla::dom::Document> mDocument;
  txOutputFormat mOutputFormat;
  nsString mText;
  bool mCreatedDocument;
};

#endif
