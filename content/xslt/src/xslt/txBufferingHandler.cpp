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
 * Jonas Sicking.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jonas Sicking <jonas@sicking.cc>
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
    }
    virtual ~txOutputTransaction()
    {
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
    }
    nsCOMPtr<nsIAtom> mPrefix;
    nsCOMPtr<nsIAtom> mLocalName;
    nsCOMPtr<nsIAtom> mLowercaseLocalName;
    PRInt32 mNsID;
    nsString mValue;
};

txBufferingHandler::txBufferingHandler() : mCanAddAttribute(PR_FALSE)
{
    mBuffer = new txResultBuffer();
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
txBufferingHandler::characters(const nsSubstring& aData, PRBool aDOE)
{
    NS_ENSURE_TRUE(mBuffer, NS_ERROR_OUT_OF_MEMORY);

    mCanAddAttribute = PR_FALSE;

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

    mCanAddAttribute = PR_FALSE;

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

    mCanAddAttribute = PR_FALSE;

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

    mCanAddAttribute = PR_FALSE;

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

    mCanAddAttribute = PR_TRUE;

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

    mCanAddAttribute = PR_TRUE;

    txOutputTransaction* transaction =
        new txStartElementTransaction(aPrefix, aLocalName, aNsID);
    NS_ENSURE_TRUE(transaction, NS_ERROR_OUT_OF_MEMORY);

    return mBuffer->addTransaction(transaction);
}

PR_STATIC_CALLBACK(PRBool)
deleteTransaction(void* aElement, void *aData)
{
    delete static_cast<txOutputTransaction*>(aElement);
    return PR_TRUE;
}

txResultBuffer::~txResultBuffer()
{
    mTransactions.EnumerateForwards(deleteTransaction, nsnull);
}

nsresult
txResultBuffer::addTransaction(txOutputTransaction* aTransaction)
{
    if (!mTransactions.AppendElement(aTransaction)) {
        return NS_ERROR_OUT_OF_MEMORY;
    }
    return NS_OK;
}

struct Holder
{
    txAXMLEventHandler** mHandler;
    nsresult mResult;
    nsAFlatString::const_char_iterator mIter;
};

PR_STATIC_CALLBACK(PRBool)
flushTransaction(void* aElement, void *aData)
{
    Holder* holder = static_cast<Holder*>(aData);
    txAXMLEventHandler* handler = *holder->mHandler;
    txOutputTransaction* transaction =
        static_cast<txOutputTransaction*>(aElement);

    nsresult rv;
    switch (transaction->mType) {
        case txOutputTransaction::eAttributeAtomTransaction:
        {
            txAttributeAtomTransaction* transaction =
                static_cast<txAttributeAtomTransaction*>(aElement);
            rv = handler->attribute(transaction->mPrefix,
                                    transaction->mLocalName,
                                    transaction->mLowercaseLocalName,
                                    transaction->mNsID,
                                    transaction->mValue);
            break;
        }
        case txOutputTransaction::eAttributeTransaction:
        {
            txAttributeTransaction* attrTransaction =
                static_cast<txAttributeTransaction*>(aElement);
            rv = handler->attribute(attrTransaction->mPrefix,
                                    attrTransaction->mLocalName,
                                    attrTransaction->mNsID,
                                    attrTransaction->mValue);
            break;
        }
        case txOutputTransaction::eCharacterTransaction:
        case txOutputTransaction::eCharacterNoOETransaction:
        {
            txCharacterTransaction* charTransaction =
                static_cast<txCharacterTransaction*>(aElement);
            nsAFlatString::const_char_iterator& start =
                holder->mIter;
            nsAFlatString::const_char_iterator end =
                start + charTransaction->mLength;
            rv = handler->characters(Substring(start, end),
                                     transaction->mType ==
                                     txOutputTransaction::eCharacterNoOETransaction);
            start = end;
            break;
        }
        case txOutputTransaction::eCommentTransaction:
        {
            txCommentTransaction* commentTransaction =
                static_cast<txCommentTransaction*>(aElement);
            rv = handler->comment(commentTransaction->mValue);
            break;
        }
        case txOutputTransaction::eEndElementTransaction:
        {
            rv = handler->endElement();
            break;
        }
        case txOutputTransaction::ePITransaction:
        {
            txPITransaction* piTransaction =
                static_cast<txPITransaction*>(aElement);
            rv = handler->processingInstruction(piTransaction->mTarget,
                                                piTransaction->mData);
            break;
        }
        case txOutputTransaction::eStartDocumentTransaction:
        {
            rv = handler->startDocument();
            break;
        }
        case txOutputTransaction::eStartElementAtomTransaction:
        {
            txStartElementAtomTransaction* transaction =
                static_cast<txStartElementAtomTransaction*>(aElement);
            rv = handler->startElement(transaction->mPrefix,
                                       transaction->mLocalName,
                                       transaction->mLowercaseLocalName,
                                       transaction->mNsID);
            break;
        }
        case txOutputTransaction::eStartElementTransaction:
        {
            txStartElementTransaction* transaction =
                static_cast<txStartElementTransaction*>(aElement);
            rv = handler->startElement(transaction->mPrefix,
                                       transaction->mLocalName,
                                       transaction->mNsID);
            break;
        }
    }

    holder->mResult = rv;

    return NS_SUCCEEDED(rv);
}

nsresult
txResultBuffer::flushToHandler(txAXMLEventHandler** aHandler)
{
    Holder data = { aHandler, NS_OK };
    mStringValue.BeginReading(data.mIter);

    mTransactions.EnumerateForwards(flushTransaction, &data);

    return data.mResult;
}

txOutputTransaction*
txResultBuffer::getLastTransaction()
{
    PRInt32 last = mTransactions.Count() - 1;
    if (last < 0) {
        return nsnull;
    }
    return static_cast<txOutputTransaction*>(mTransactions[last]);
}
