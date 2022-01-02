/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef txRtfHandler_h___
#define txRtfHandler_h___

#include "mozilla/Attributes.h"
#include "txBufferingHandler.h"
#include "txExprResult.h"
#include "txXPathNode.h"

class txResultTreeFragment : public txAExprResult {
 public:
  explicit txResultTreeFragment(mozilla::UniquePtr<txResultBuffer>&& aBuffer);

  TX_DECL_EXPRRESULT

  nsresult flushToHandler(txAXMLEventHandler* aHandler);

  void setNode(const txXPathNode* aNode) {
    NS_ASSERTION(!mNode, "Already converted!");

    mNode = mozilla::WrapUnique(aNode);
  }
  const txXPathNode* getNode() const { return mNode.get(); }

 private:
  mozilla::UniquePtr<txResultBuffer> mBuffer;
  mozilla::UniquePtr<const txXPathNode> mNode;
};

class txRtfHandler : public txBufferingHandler {
 public:
  nsresult getAsRTF(txAExprResult** aResult);

  nsresult endDocument(nsresult aResult) override;
  nsresult startDocument() override;
};

#endif /* txRtfHandler_h___ */
