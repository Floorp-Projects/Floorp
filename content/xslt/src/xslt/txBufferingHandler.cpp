/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "txBufferingHandler.h"

class txOutputTransaction
{
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
    txOutputTransaction(txTransactionType aType)
        : mType(aType)
    {
        MOZ_COUNT_CTOR(txOutputTransaction);
    }
    virtual ~txOutputTransaction()
    {
        MOZ_COUNT_DTOR(txOutputTransaction);
    }
    txTransactionType mType;
};

class txCharacterTransaction : public txOutputTransaction
{
public:
    txCharacterTransaction(txTransactionType aType, PRUint32 aLength)
        : txOutputTransaction(aType),
          mLength(aLength)
    {
        MOZ_COUNT_CTOR_INHERITED(txCharacterTransaction, txOutputTransaction);
    }
    virtual ~txCharacterTransaction()
    {
        MOZ_COUNT_DTOR_INHERITED(txCharacterTransaction, txOutputTransaction);
    }
    PRUint32 mLength;
};

class txCommentTransaction : public txOutputTransaction
{
public:
    txCommentTransaction(const nsAString& aValue)
        : txOutputTransaction(eCommentTransaction),
          mValue(aValue)
    {
        MOZ_COUNT_CTOR_INHERITED(txCommentTransaction, txOutputTransaction);
    }
    virtual ~txCommentTransaction()
    {
        MOZ_COUNT_DTOR_INHERITED(txCommentTransaction, txOutputTransaction);
    }
    nsString mValue;
};

class txPITransaction : public txOutputTransaction
{
public:
    txPITransaction(const nsAString& aTarget, const nsAString& aData)
        : txOutputTransaction(ePITransaction),
          mTarget(aTarget),
          mData(aData)
    {
        MOZ_COUNT_CTOR_INHERITED(txPITransaction, txOutputTransaction);
    }
    virtual ~txPITransaction()
    {
        MOZ_COUNT_DTOR_INHERITED(txPITransaction, txOutputTransaction);
    }
    nsString mTarget;
    nsString mData;
};

class txStartElementAtomTransaction : public txOutputTransaction
{
public:
    txStartElementAtomTransaction(nsIAtom* aPrefix, nsIAtom* aLocalName,
                                  nsIAtom* aLowercaseLocalName, PRInt32 aNsID)
        : txOutputTransaction(eStartElementAtomTransaction),
          mPrefix(aPrefix),
          mLocalName(aLocalName),
          mLowercaseLocalName(aLowercaseLocalName),
          mNsID(aNsID)
    {
        MOZ_COUNT_CTOR_INHERITED(txStartElementAtomTransaction, txOutputTransaction);
    }
    virtual ~txStartElementAtomTransaction()
    {
        MOZ_COUNT_DTOR_INHERITED(txStartElementAtomTransaction, txOutputTransaction);
    }
    nsCOMPtr<nsIAtom> mPrefix;
    nsCOMPtr<nsIAtom> mLocalName;
    nsCOMPtr<nsIAtom> mLowercaseLocalName;
    PRInt32 mNsID;
};

class txStartElementTransaction : public txOutputTransaction
{
public:
    txStartElementTransaction(nsIAtom* aPrefix,
                              const nsSubstring& aLocalName, PRInt32 aNsID)
        : txOutputTransaction(eStartElementTransaction),
          mPrefix(aPrefix),
          mLocalName(aLocalName),
          mNsID(aNsID)
    {
        MOZ_COUNT_CTOR_INHERITED(txStartElementTransaction, txOutputTransaction);
    }
    virtual ~txStartElementTransaction()
    {
        MOZ_COUNT_DTOR_INHERITED(txStartElementTransaction, txOutputTransaction);
    }
    nsCOMPtr<nsIAtom> mPrefix;
    nsString mLocalName;
    PRInt32 mNsID;
};

class txAttributeTransaction : public txOutputTransaction
{
public:
    txAttributeTransaction(nsIAtom* aPrefix,
                           const nsSubstring& aLocalName, PRInt32 aNsID,
                           const nsString& aValue)
        : txOutputTransaction(eAttributeTransaction),
          mPrefix(aPrefix),
          mLocalName(aLocalName),
          mNsID(aNsID),
          mValue(aValue)
    {
        MOZ_COUNT_CTOR_INHERITED(txAttributeTransaction, txOutputTransaction);
    }
    virtual ~txAttributeTransaction()
    {
        MOZ_COUNT_DTOR_INHERITED(txAttributeTransaction, txOutputTransaction);
    }
    nsCOMPtr<nsIAtom> mPrefix;
    nsString mLocalName;
    PRInt32 mNsID;
    nsString mValue;
};

class txAttributeAtomTransaction : public txOutputTransaction
{
public:
    txAttributeAtomTransaction(nsIAtom* aPrefix, nsIAtom* aLocalName,
                               nsIAtom* aLowercaseLocalName,
                               PRInt32 aNsID, const nsString& aValue)
        : txOutputTransaction(eAttributeAtomTransaction),
          mPrefix(aPrefix),
          mLocalName(aLocalName),
          mLowercaseLocalName(aLowercaseLocalName),
          mNsID(aNsID),
          mValue(aValue)
    {
        MOZ_COUNT_CTOR_INHERITED(txAttributeAtomTransaction, txOutputTransaction);
    }
    virtual ~txAttributeAtomTransaction()
    {
        MOZ_COUNT_DTOR_INHERITED(txAttributeAtomTransaction, txOutputTransaction);
    }
    nsCOMPtr<nsIAtom> mPrefix;
    nsCOMPtr<nsIAtom> mLocalName;
    nsCOMPtr<nsIAtom> mLowercaseLocalName;
    PRInt32 mNsID;
    nsString mValue;
};

txBufferingHandler::txBufferingHandler() : mCanAddAttribute(false)
{
    MOZ_COUNT_CTOR(txBufferingHandler);
    mBuffer = new txResultBuffer();
}

txBufferingHandler::~txBufferingHandler()
{
    MOZ_COUNT_DTOR(txBufferingHandler);
}

nsresult
txBufferingHandler::attribute(nsIAtom* aPrefix, nsIAtom* aLocalName,
                              nsIAtom* aLowercaseLocalName, PRInt32 aNsID,
                              const nsString& aValue)
{
    NS_ENSURE_TRUE(mBuffer, NS_ERROR_OUT_OF_MEMORY);

    if (!mCanAddAttribute) {
        // XXX ErrorReport: Can't add attributes without element
        return NS_OK;
    }

    txOutputTransaction* transaction =
        new txAttributeAtomTransaction(aPrefix, aLocalName,
                                       aLowercaseLocalName, aNsID,
                                       aValue);
    NS_ENSURE_TRUE(transaction, NS_ERROR_OUT_OF_MEMORY);

    return mBuffer->addTransaction(transaction);
}

nsresult
txBufferingHandler::attribute(nsIAtom* aPrefix, const nsSubstring& aLocalName,
                              const PRInt32 aNsID, const nsString& aValue)
{
    NS_ENSURE_TRUE(mBuffer, NS_ERROR_OUT_OF_MEMORY);

    if (!mCanAddAttribute) {
        // XXX ErrorReport: Can't add attributes without element
        return NS_OK;
    }

    txOutputTransaction* transaction =
        new txAttributeTransaction(aPrefix, aLocalName, aNsID, aValue);
    NS_ENSURE_TRUE(transaction, NS_ERROR_OUT_OF_MEMORY);

    return mBuffer->addTransaction(transaction);
}

nsresult
txBufferingHandler::characters(const nsSubstring& aData, bool aDOE)
{
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
    NS_ENSURE_TRUE(transaction, NS_ERROR_OUT_OF_MEMORY);

    mBuffer->mStringValue.Append(aData);
    return mBuffer->addTransaction(transaction);
}

nsresult
txBufferingHandler::comment(const nsString& aData)
{
    NS_ENSURE_TRUE(mBuffer, NS_ERROR_OUT_OF_MEMORY);

    mCanAddAttribute = false;

    txOutputTransaction* transaction = new txCommentTransaction(aData);
    NS_ENSURE_TRUE(transaction, NS_ERROR_OUT_OF_MEMORY);

    return mBuffer->addTransaction(transaction);
}

nsresult
txBufferingHandler::endDocument(nsresult aResult)
{
    NS_ENSURE_TRUE(mBuffer, NS_ERROR_OUT_OF_MEMORY);

    txOutputTransaction* transaction =
        new txOutputTransaction(txOutputTransaction::eEndDocumentTransaction);
    NS_ENSURE_TRUE(transaction, NS_ERROR_OUT_OF_MEMORY);

    return mBuffer->addTransaction(transaction);
}

nsresult
txBufferingHandler::endElement()
{
    NS_ENSURE_TRUE(mBuffer, NS_ERROR_OUT_OF_MEMORY);

    mCanAddAttribute = false;

    txOutputTransaction* transaction =
        new txOutputTransaction(txOutputTransaction::eEndElementTransaction);
    NS_ENSURE_TRUE(transaction, NS_ERROR_OUT_OF_MEMORY);

    return mBuffer->addTransaction(transaction);
}

nsresult
txBufferingHandler::processingInstruction(const nsString& aTarget,
                                          const nsString& aData)
{
    NS_ENSURE_TRUE(mBuffer, NS_ERROR_OUT_OF_MEMORY);

    mCanAddAttribute = false;

    txOutputTransaction* transaction =
        new txPITransaction(aTarget, aData);
    NS_ENSURE_TRUE(transaction, NS_ERROR_OUT_OF_MEMORY);

    return mBuffer->addTransaction(transaction);
}

nsresult
txBufferingHandler::startDocument()
{
    NS_ENSURE_TRUE(mBuffer, NS_ERROR_OUT_OF_MEMORY);

    txOutputTransaction* transaction =
        new txOutputTransaction(txOutputTransaction::eStartDocumentTransaction);
    NS_ENSURE_TRUE(transaction, NS_ERROR_OUT_OF_MEMORY);

    return mBuffer->addTransaction(transaction);
}

nsresult
txBufferingHandler::startElement(nsIAtom* aPrefix, nsIAtom* aLocalName,
                                 nsIAtom* aLowercaseLocalName,
                                 PRInt32 aNsID)
{
    NS_ENSURE_TRUE(mBuffer, NS_ERROR_OUT_OF_MEMORY);

    mCanAddAttribute = true;

    txOutputTransaction* transaction =
        new txStartElementAtomTransaction(aPrefix, aLocalName,
                                          aLowercaseLocalName, aNsID);
    NS_ENSURE_TRUE(transaction, NS_ERROR_OUT_OF_MEMORY);

    return mBuffer->addTransaction(transaction);
}

nsresult
txBufferingHandler::startElement(nsIAtom* aPrefix,
                                 const nsSubstring& aLocalName,
                                 const PRInt32 aNsID)
{
    NS_ENSURE_TRUE(mBuffer, NS_ERROR_OUT_OF_MEMORY);

    mCanAddAttribute = true;

    txOutputTransaction* transaction =
        new txStartElementTransaction(aPrefix, aLocalName, aNsID);
    NS_ENSURE_TRUE(transaction, NS_ERROR_OUT_OF_MEMORY);

    return mBuffer->addTransaction(transaction);
}

txResultBuffer::txResultBuffer()
{
    MOZ_COUNT_CTOR(txResultBuffer);
}

txResultBuffer::~txResultBuffer()
{
    MOZ_COUNT_DTOR(txResultBuffer);
    for (PRUint32 i = 0, len = mTransactions.Length(); i < len; ++i) {
        delete mTransactions[i];
    }
}

nsresult
txResultBuffer::addTransaction(txOutputTransaction* aTransaction)
{
    if (mTransactions.AppendElement(aTransaction) == nullptr) {
        return NS_ERROR_OUT_OF_MEMORY;
    }
    return NS_OK;
}

static nsresult
flushTransaction(txOutputTransaction* aTransaction,
                 txAXMLEventHandler* aHandler,
                 nsAFlatString::const_char_iterator& aIter)
{
    switch (aTransaction->mType) {
        case txOutputTransaction::eAttributeAtomTransaction:
        {
            txAttributeAtomTransaction* transaction =
                static_cast<txAttributeAtomTransaction*>(aTransaction);
            return aHandler->attribute(transaction->mPrefix,
                                       transaction->mLocalName,
                                       transaction->mLowercaseLocalName,
                                       transaction->mNsID,
                                       transaction->mValue);
        }
        case txOutputTransaction::eAttributeTransaction:
        {
            txAttributeTransaction* attrTransaction =
                static_cast<txAttributeTransaction*>(aTransaction);
            return aHandler->attribute(attrTransaction->mPrefix,
                                       attrTransaction->mLocalName,
                                       attrTransaction->mNsID,
                                       attrTransaction->mValue);
        }
        case txOutputTransaction::eCharacterTransaction:
        case txOutputTransaction::eCharacterNoOETransaction:
        {
            txCharacterTransaction* charTransaction =
                static_cast<txCharacterTransaction*>(aTransaction);
            nsAFlatString::const_char_iterator start = aIter;
            nsAFlatString::const_char_iterator end =
                start + charTransaction->mLength;
            aIter = end;
            return aHandler->characters(Substring(start, end),
                                        aTransaction->mType ==
                                        txOutputTransaction::eCharacterNoOETransaction);
        }
        case txOutputTransaction::eCommentTransaction:
        {
            txCommentTransaction* commentTransaction =
                static_cast<txCommentTransaction*>(aTransaction);
            return aHandler->comment(commentTransaction->mValue);
        }
        case txOutputTransaction::eEndElementTransaction:
        {
            return aHandler->endElement();
        }
        case txOutputTransaction::ePITransaction:
        {
            txPITransaction* piTransaction =
                static_cast<txPITransaction*>(aTransaction);
            return aHandler->processingInstruction(piTransaction->mTarget,
                                                   piTransaction->mData);
        }
        case txOutputTransaction::eStartDocumentTransaction:
        {
            return aHandler->startDocument();
        }
        case txOutputTransaction::eStartElementAtomTransaction:
        {
            txStartElementAtomTransaction* transaction =
                static_cast<txStartElementAtomTransaction*>(aTransaction);
            return aHandler->startElement(transaction->mPrefix,
                                          transaction->mLocalName,
                                          transaction->mLowercaseLocalName,
                                          transaction->mNsID);
        }
        case txOutputTransaction::eStartElementTransaction:
        {
            txStartElementTransaction* transaction =
                static_cast<txStartElementTransaction*>(aTransaction);
            return aHandler->startElement(transaction->mPrefix,
                                          transaction->mLocalName,
                                          transaction->mNsID);
        }
        default:
        {
            NS_NOTREACHED("Unexpected transaction type");
        }
    }

    return NS_ERROR_UNEXPECTED;
}

nsresult
txResultBuffer::flushToHandler(txAXMLEventHandler* aHandler)
{
    nsAFlatString::const_char_iterator iter;
    mStringValue.BeginReading(iter);

    for (PRUint32 i = 0, len = mTransactions.Length(); i < len; ++i) {
        nsresult rv = flushTransaction(mTransactions[i], aHandler, iter);
        NS_ENSURE_SUCCESS(rv, rv);
    }

    return NS_OK;
}

txOutputTransaction*
txResultBuffer::getLastTransaction()
{
    PRInt32 last = mTransactions.Length() - 1;
    if (last < 0) {
        return nullptr;
    }
    return mTransactions[last];
}
