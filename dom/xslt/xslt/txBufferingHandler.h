/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef txBufferingHandler_h__
#define txBufferingHandler_h__

#include "mozilla/UniquePtr.h"
#include "txXMLEventHandler.h"
#include "nsString.h"
#include "nsTArray.h"

class txOutputTransaction;

class txResultBuffer {
 public:
  txResultBuffer();
  ~txResultBuffer();

  nsresult addTransaction(txOutputTransaction* aTransaction);

  nsresult flushToHandler(txAXMLEventHandler* aHandler);

  txOutputTransaction* getLastTransaction();

  nsString mStringValue;

 private:
  nsTArray<txOutputTransaction*> mTransactions;
};

class txBufferingHandler : public txAXMLEventHandler {
 public:
  txBufferingHandler();
  virtual ~txBufferingHandler();

  TX_DECL_TXAXMLEVENTHANDLER

 protected:
  mozilla::UniquePtr<txResultBuffer> mBuffer;
  bool mCanAddAttribute;
};

#endif /* txBufferingHandler_h__ */
