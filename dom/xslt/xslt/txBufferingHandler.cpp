/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "txBufferingHandler.h"

using mozilla::MakeUnique;

class txOutputTransaction {
 public:
  enum txTransactionType {
    eAttributeTransaction,
    eAttributeAtomTransaction,
    eCharacterTransaction,
    eCharacterNoOETransaction,
    eCommentTransaction,
    eEndDocumentTransaction,
    eEndElementTransaction,
    ePITransaction,
    eStartDocumentTransaction,
    eStartElementAtomTransaction,
    eStartElementTransaction
  };
  explicit txOutputTransaction(txTransactionType aType) : mType(aType) {
    MOZ_COUNT_CTOR(txOutputTransaction);
  }
  MOZ_COUNTED_DTOR_VIRTUAL(txOutputTransaction)
  txTransactionType mType;
};

class txCharacterTransaction : public txOutputTransaction {
 public:
  txCharacterTransaction(txTransactionType aType, uint32_t aLength)
      : txOutputTransaction(aType), mLength(aLength) {
    MOZ_COUNT_CTOR_INHERITED(txCharacterTransaction, txOutputTransaction);
  }
  virtual ~txCharacterTransaction() {
    MOZ_COUNT_DTOR_INHERITED(txCharacterTransaction, txOutputTransaction);
  }
  uint32_t mLength;
};

class txCommentTransaction : public txOutputTransaction {
 public:
  explicit txCommentTransaction(const nsAString& aValue)
      : txOutputTransaction(eCommentTransaction), mValue(aValue) {
    MOZ_COUNT_CTOR_INHERITED(txCommentTransaction, txOutputTransaction);
  }
  virtual ~txCommentTransaction() {
    MOZ_COUNT_DTOR_INHERITED(txCommentTransaction, txOutputTransaction);
  }
  nsString mValue;
};

class txPITransaction : public txOutputTransaction {
 public:
  txPITransaction(const nsAString& aTarget, const nsAString& aData)
      : txOutputTransaction(ePITransaction), mTarget(aTarget), mData(aData) {
    MOZ_COUNT_CTOR_INHERITED(txPITransaction, txOutputTransaction);
  }
  virtual ~txPITransaction() {
    MOZ_COUNT_DTOR_INHERITED(txPITransaction, txOutputTransaction);
  }
  nsString mTarget;
  nsString mData;
};

class txStartElementAtomTransaction : public txOutputTransaction {
 public:
  txStartElementAtomTransaction(nsAtom* aPrefix, nsAtom* aLocalName,
                                nsAtom* aLowercaseLocalName, int32_t aNsID)
      : txOutputTransaction(eStartElementAtomTransaction),
        mPrefix(aPrefix),
        mLocalName(aLocalName),
        mLowercaseLocalName(aLowercaseLocalName),
        mNsID(aNsID) {
    MOZ_COUNT_CTOR_INHERITED(txStartElementAtomTransaction,
                             txOutputTransaction);
  }
  virtual ~txStartElementAtomTransaction() {
    MOZ_COUNT_DTOR_INHERITED(txStartElementAtomTransaction,
                             txOutputTransaction);
  }
  RefPtr<nsAtom> mPrefix;
  RefPtr<nsAtom> mLocalName;
  RefPtr<nsAtom> mLowercaseLocalName;
  int32_t mNsID;
};

class txStartElementTransaction : public txOutputTransaction {
 public:
  txStartElementTransaction(nsAtom* aPrefix, const nsAString& aLocalName,
                            int32_t aNsID)
      : txOutputTransaction(eStartElementTransaction),
        mPrefix(aPrefix),
        mLocalName(aLocalName),
        mNsID(aNsID) {
    MOZ_COUNT_CTOR_INHERITED(txStartElementTransaction, txOutputTransaction);
  }
  virtual ~txStartElementTransaction() {
    MOZ_COUNT_DTOR_INHERITED(txStartElementTransaction, txOutputTransaction);
  }
  RefPtr<nsAtom> mPrefix;
  nsString mLocalName;
  int32_t mNsID;
};

class txAttributeTransaction : public txOutputTransaction {
 public:
  txAttributeTransaction(nsAtom* aPrefix, const nsAString& aLocalName,
                         int32_t aNsID, const nsString& aValue)
      : txOutputTransaction(eAttributeTransaction),
        mPrefix(aPrefix),
        mLocalName(aLocalName),
        mNsID(aNsID),
        mValue(aValue) {
    MOZ_COUNT_CTOR_INHERITED(txAttributeTransaction, txOutputTransaction);
  }
  virtual ~txAttributeTransaction() {
    MOZ_COUNT_DTOR_INHERITED(txAttributeTransaction, txOutputTransaction);
  }
  RefPtr<nsAtom> mPrefix;
  nsString mLocalName;
  int32_t mNsID;
  nsString mValue;
};

class txAttributeAtomTransaction : public txOutputTransaction {
 public:
  txAttributeAtomTransaction(nsAtom* aPrefix, nsAtom* aLocalName,
                             nsAtom* aLowercaseLocalName, int32_t aNsID,
                             const nsString& aValue)
      : txOutputTransaction(eAttributeAtomTransaction),
        mPrefix(aPrefix),
        mLocalName(aLocalName),
        mLowercaseLocalName(aLowercaseLocalName),
        mNsID(aNsID),
        mValue(aValue) {
    MOZ_COUNT_CTOR_INHERITED(txAttributeAtomTransaction, txOutputTransaction);
  }
  virtual ~txAttributeAtomTransaction() {
    MOZ_COUNT_DTOR_INHERITED(txAttributeAtomTransaction, txOutputTransaction);
  }
  RefPtr<nsAtom> mPrefix;
  RefPtr<nsAtom> mLocalName;
  RefPtr<nsAtom> mLowercaseLocalName;
  int32_t mNsID;
  nsString mValue;
};

txBufferingHandler::txBufferingHandler() : mCanAddAttribute(false) {
  MOZ_COUNT_CTOR(txBufferingHandler);
  mBuffer = MakeUnique<txResultBuffer>();
}

txBufferingHandler::~txBufferingHandler() {
  MOZ_COUNT_DTOR(txBufferingHandler);
}

nsresult txBufferingHandler::attribute(nsAtom* aPrefix, nsAtom* aLocalName,
                                       nsAtom* aLowercaseLocalName,
                                       int32_t aNsID, const nsString& aValue) {
  NS_ENSURE_TRUE(mBuffer, NS_ERROR_OUT_OF_MEMORY);

  if (!mCanAddAttribute) {
    // XXX ErrorReport: Can't add attributes without element
    return NS_OK;
  }

  txOutputTransaction* transaction = new txAttributeAtomTransaction(
      aPrefix, aLocalName, aLowercaseLocalName, aNsID, aValue);
  return mBuffer->addTransaction(transaction);
}

nsresult txBufferingHandler::attribute(nsAtom* aPrefix,
                                       const nsAString& aLocalName,
                                       const int32_t aNsID,
                                       const nsString& aValue) {
  NS_ENSURE_TRUE(mBuffer, NS_ERROR_OUT_OF_MEMORY);

  if (!mCanAddAttribute) {
    // XXX ErrorReport: Can't add attributes without element
    return NS_OK;
  }

  txOutputTransaction* transaction =
      new txAttributeTransaction(aPrefix, aLocalName, aNsID, aValue);
  return mBuffer->addTransaction(transaction);
}

nsresult txBufferingHandler::characters(const nsAString& aData, bool aDOE) {
  NS_ENSURE_TRUE(mBuffer, NS_ERROR_OUT_OF_MEMORY);

  mCanAddAttribute = false;

  txOutputTransaction::txTransactionType type =
      aDOE ? txOutputTransaction::eCharacterNoOETransaction
           : txOutputTransaction::eCharacterTransaction;

  txOutputTransaction* transaction = mBuffer->getLastTransaction();
  if (transaction && transaction->mType == type) {
    mBuffer->mStringValue.Append(aData);
    static_cast<txCharacterTransaction*>(transaction)->mLength +=
        aData.Length();
    return NS_OK;
  }

  transaction = new txCharacterTransaction(type, aData.Length());
  mBuffer->mStringValue.Append(aData);
  return mBuffer->addTransaction(transaction);
}

nsresult txBufferingHandler::comment(const nsString& aData) {
  NS_ENSURE_TRUE(mBuffer, NS_ERROR_OUT_OF_MEMORY);

  mCanAddAttribute = false;

  txOutputTransaction* transaction = new txCommentTransaction(aData);
  return mBuffer->addTransaction(transaction);
}

nsresult txBufferingHandler::endDocument(nsresult aResult) {
  NS_ENSURE_TRUE(mBuffer, NS_ERROR_OUT_OF_MEMORY);

  txOutputTransaction* transaction =
      new txOutputTransaction(txOutputTransaction::eEndDocumentTransaction);
  return mBuffer->addTransaction(transaction);
}

nsresult txBufferingHandler::endElement() {
  NS_ENSURE_TRUE(mBuffer, NS_ERROR_OUT_OF_MEMORY);

  mCanAddAttribute = false;

  txOutputTransaction* transaction =
      new txOutputTransaction(txOutputTransaction::eEndElementTransaction);
  return mBuffer->addTransaction(transaction);
}

nsresult txBufferingHandler::processingInstruction(const nsString& aTarget,
                                                   const nsString& aData) {
  NS_ENSURE_TRUE(mBuffer, NS_ERROR_OUT_OF_MEMORY);

  mCanAddAttribute = false;

  txOutputTransaction* transaction = new txPITransaction(aTarget, aData);
  return mBuffer->addTransaction(transaction);
}

nsresult txBufferingHandler::startDocument() {
  NS_ENSURE_TRUE(mBuffer, NS_ERROR_OUT_OF_MEMORY);

  txOutputTransaction* transaction =
      new txOutputTransaction(txOutputTransaction::eStartDocumentTransaction);
  return mBuffer->addTransaction(transaction);
}

nsresult txBufferingHandler::startElement(nsAtom* aPrefix, nsAtom* aLocalName,
                                          nsAtom* aLowercaseLocalName,
                                          int32_t aNsID) {
  NS_ENSURE_TRUE(mBuffer, NS_ERROR_OUT_OF_MEMORY);

  mCanAddAttribute = true;

  txOutputTransaction* transaction = new txStartElementAtomTransaction(
      aPrefix, aLocalName, aLowercaseLocalName, aNsID);
  return mBuffer->addTransaction(transaction);
}

nsresult txBufferingHandler::startElement(nsAtom* aPrefix,
                                          const nsAString& aLocalName,
                                          const int32_t aNsID) {
  NS_ENSURE_TRUE(mBuffer, NS_ERROR_OUT_OF_MEMORY);

  mCanAddAttribute = true;

  txOutputTransaction* transaction =
      new txStartElementTransaction(aPrefix, aLocalName, aNsID);
  return mBuffer->addTransaction(transaction);
}

txResultBuffer::txResultBuffer() { MOZ_COUNT_CTOR(txResultBuffer); }

txResultBuffer::~txResultBuffer() {
  MOZ_COUNT_DTOR(txResultBuffer);
  for (uint32_t i = 0, len = mTransactions.Length(); i < len; ++i) {
    delete mTransactions[i];
  }
}

nsresult txResultBuffer::addTransaction(txOutputTransaction* aTransaction) {
  // XXX(Bug 1631371) Check if this should use a fallible operation as it
  // pretended earlier, or change the return type to void.
  mTransactions.AppendElement(aTransaction);
  return NS_OK;
}

static nsresult flushTransaction(txOutputTransaction* aTransaction,
                                 txAXMLEventHandler* aHandler,
                                 nsString::const_char_iterator& aIter) {
  switch (aTransaction->mType) {
    case txOutputTransaction::eAttributeAtomTransaction: {
      txAttributeAtomTransaction* transaction =
          static_cast<txAttributeAtomTransaction*>(aTransaction);
      return aHandler->attribute(transaction->mPrefix, transaction->mLocalName,
                                 transaction->mLowercaseLocalName,
                                 transaction->mNsID, transaction->mValue);
    }
    case txOutputTransaction::eAttributeTransaction: {
      txAttributeTransaction* attrTransaction =
          static_cast<txAttributeTransaction*>(aTransaction);
      return aHandler->attribute(
          attrTransaction->mPrefix, attrTransaction->mLocalName,
          attrTransaction->mNsID, attrTransaction->mValue);
    }
    case txOutputTransaction::eCharacterTransaction:
    case txOutputTransaction::eCharacterNoOETransaction: {
      txCharacterTransaction* charTransaction =
          static_cast<txCharacterTransaction*>(aTransaction);
      nsString::const_char_iterator start = aIter;
      nsString::const_char_iterator end = start + charTransaction->mLength;
      aIter = end;
      return aHandler->characters(
          Substring(start, end),
          aTransaction->mType ==
              txOutputTransaction::eCharacterNoOETransaction);
    }
    case txOutputTransaction::eCommentTransaction: {
      txCommentTransaction* commentTransaction =
          static_cast<txCommentTransaction*>(aTransaction);
      return aHandler->comment(commentTransaction->mValue);
    }
    case txOutputTransaction::eEndElementTransaction: {
      return aHandler->endElement();
    }
    case txOutputTransaction::ePITransaction: {
      txPITransaction* piTransaction =
          static_cast<txPITransaction*>(aTransaction);
      return aHandler->processingInstruction(piTransaction->mTarget,
                                             piTransaction->mData);
    }
    case txOutputTransaction::eStartDocumentTransaction: {
      return aHandler->startDocument();
    }
    case txOutputTransaction::eStartElementAtomTransaction: {
      txStartElementAtomTransaction* transaction =
          static_cast<txStartElementAtomTransaction*>(aTransaction);
      return aHandler->startElement(
          transaction->mPrefix, transaction->mLocalName,
          transaction->mLowercaseLocalName, transaction->mNsID);
    }
    case txOutputTransaction::eStartElementTransaction: {
      txStartElementTransaction* transaction =
          static_cast<txStartElementTransaction*>(aTransaction);
      return aHandler->startElement(
          transaction->mPrefix, transaction->mLocalName, transaction->mNsID);
    }
    default: {
      MOZ_ASSERT_UNREACHABLE("Unexpected transaction type");
    }
  }

  return NS_ERROR_UNEXPECTED;
}

nsresult txResultBuffer::flushToHandler(txAXMLEventHandler* aHandler) {
  nsString::const_char_iterator iter;
  mStringValue.BeginReading(iter);

  for (uint32_t i = 0, len = mTransactions.Length(); i < len; ++i) {
    nsresult rv = flushTransaction(mTransactions[i], aHandler, iter);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

txOutputTransaction* txResultBuffer::getLastTransaction() {
  int32_t last = mTransactions.Length() - 1;
  if (last < 0) {
    return nullptr;
  }
  return mTransactions[last];
}
