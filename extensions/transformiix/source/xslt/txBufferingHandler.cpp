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
 * The Original Code is TransforMiiX XSLT processor.
 *
 * The Initial Developer of the Original Code is
 * Jonas Sicking.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * Jonas Sicking. All Rights Reserved.
 *
 * Contributor(s):
 *   Jonas Sicking <jonas@sicking.cc>
 *   Peter Van der Beken <peterv@netscape.com>
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
        eCharacterTransaction,
        eCharacterNoOETransaction,
        eCommentTransaction,
        eEndDocumentTransaction,
        eEndElementTransaction,
        ePITransaction,
        eStartDocumentTransaction,
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

class txElementTransaction : public txOutputTransaction
{
public:
    txElementTransaction(txTransactionType aType, const nsAString& aName,
                         PRInt32 aNsID)
        : txOutputTransaction(aType),
          mName(aName),
          mNsID(aNsID)
    {
    }
    nsString mName;
    PRInt32 mNsID;
};

class txAttributeTransaction : public txOutputTransaction
{
public:
    txAttributeTransaction(const nsAString& aName, PRInt32 aNsID,
                           const nsAString& aValue)
        : txOutputTransaction(eAttributeTransaction),
          mName(aName),
          mNsID(aNsID),
          mValue(aValue)
    {
    }
    nsString mName;
    PRInt32 mNsID;
    nsString mValue;
};

txBufferingHandler::txBufferingHandler() : mCanAddAttribute(PR_FALSE)
{
    mBuffer = new txResultBuffer();
}

txBufferingHandler::~txBufferingHandler()
{
}

void
txBufferingHandler::attribute(const nsAString& aName, const PRInt32 aNsID,
                              const nsAString& aValue)
{
    if (!mBuffer) {
        return;
    }

    if (!mCanAddAttribute) {
        // XXX ErrorReport: Can't add attributes without element
        return;
    }

    txOutputTransaction* transaction =
        new txAttributeTransaction(aName, aNsID, aValue);
    if (!transaction) {
        NS_ASSERTION(0, "Out of memory!");
        return;
    }
    mBuffer->addTransaction(transaction);
}

void
txBufferingHandler::characters(const nsAString& aData, PRBool aDOE)
{
    if (!mBuffer) {
        return;
    }

    mCanAddAttribute = PR_FALSE;

    txOutputTransaction::txTransactionType type =
         aDOE ? txOutputTransaction::eCharacterNoOETransaction
              : txOutputTransaction::eCharacterTransaction;

    txOutputTransaction* transaction = mBuffer->getLastTransaction();
    if (transaction && transaction->mType == type) {
        mBuffer->mStringValue.Append(aData);
        NS_STATIC_CAST(txCharacterTransaction*, transaction)->mLength +=
            aData.Length();
        return;
    }

    transaction = new txCharacterTransaction(type, aData.Length());
    if (!transaction) {
        NS_ASSERTION(0, "Out of memory!");
        return;
    }

    mBuffer->mStringValue.Append(aData);
    mBuffer->addTransaction(transaction);
}

void
txBufferingHandler::comment(const nsAString& aData)
{
    if (!mBuffer) {
        return;
    }

    mCanAddAttribute = PR_FALSE;

    txOutputTransaction* transaction = new txCommentTransaction(aData);
    if (!transaction) {
        NS_ASSERTION(0, "Out of memory!");
        return;
    }
    mBuffer->addTransaction(transaction);
}

void
txBufferingHandler::endDocument()
{
    if (!mBuffer) {
        return;
    }

    txOutputTransaction* transaction =
        new txOutputTransaction(txOutputTransaction::eEndDocumentTransaction);
    if (!transaction) {
        NS_ASSERTION(0, "Out of memory!");
        return;
    }
    mBuffer->addTransaction(transaction);
}

void
txBufferingHandler::endElement(const nsAString& aName, const PRInt32 aNsID)
{
    if (!mBuffer) {
        return;
    }

    mCanAddAttribute = PR_FALSE;

    txOutputTransaction* transaction =
        new txElementTransaction(txOutputTransaction::eEndElementTransaction,
                                 aName, aNsID);
    if (!transaction) {
        NS_ASSERTION(0, "Out of memory!");
        return;
    }
    mBuffer->addTransaction(transaction);
}

void
txBufferingHandler::processingInstruction(const nsAString& aTarget,
                                          const nsAString& aData)
{
    if (!mBuffer) {
        return;
    }

    mCanAddAttribute = PR_FALSE;

    txOutputTransaction* transaction =
        new txPITransaction(aTarget, aData);
    if (!transaction) {
        NS_ASSERTION(0, "Out of memory!");
        return;
    }
    mBuffer->addTransaction(transaction);
}

void txBufferingHandler::startDocument()
{
    if (!mBuffer) {
        return;
    }

    txOutputTransaction* transaction =
        new txOutputTransaction(txOutputTransaction::eStartDocumentTransaction);
    if (!transaction) {
        NS_ASSERTION(0, "Out of memory!");
        return;
    }
    mBuffer->addTransaction(transaction);
}

void
txBufferingHandler::startElement(const nsAString& aName, const PRInt32 aNsID)
{
    if (!mBuffer) {
        return;
    }

    mCanAddAttribute = PR_TRUE;

    txOutputTransaction* transaction =
        new txElementTransaction(txOutputTransaction::eStartElementTransaction,
                                 aName, aNsID);
    if (!transaction) {
        NS_ASSERTION(0, "Out of memory!");
        return;
    }
    mBuffer->addTransaction(transaction);
}

PR_STATIC_CALLBACK(PRBool)
deleteTransaction(void* aElement, void *aData)
{
    delete NS_STATIC_CAST(txOutputTransaction*, aElement);
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
    txAXMLEventHandler* mHandler;
    nsAFlatString::const_char_iterator mIter;
};

PR_STATIC_CALLBACK(PRBool)
flushTransaction(void* aElement, void *aData)
{
    txAXMLEventHandler* handler = NS_STATIC_CAST(Holder*, aData)->mHandler;
    txOutputTransaction* transaction =
        NS_STATIC_CAST(txOutputTransaction*, aElement);

    switch (transaction->mType) {
        case txOutputTransaction::eAttributeTransaction:
        {
            txAttributeTransaction* attrTransaction =
                NS_STATIC_CAST(txAttributeTransaction*, aElement);
            handler->attribute(attrTransaction->mName,
                               attrTransaction->mNsID,
                               attrTransaction->mValue);
            break;
        }
        case txOutputTransaction::eCharacterTransaction:
        case txOutputTransaction::eCharacterNoOETransaction:
        {
            txCharacterTransaction* charTransaction =
                NS_STATIC_CAST(txCharacterTransaction*, aElement);
            nsAFlatString::const_char_iterator& start =
                NS_STATIC_CAST(Holder*, aData)->mIter;
            nsAFlatString::const_char_iterator end =
                start + charTransaction->mLength;
            handler->characters(Substring(start, end),
                                transaction->mType ==
                                txOutputTransaction::eCharacterNoOETransaction);
            start = end;
            break;
        }
        case txOutputTransaction::eCommentTransaction:
        {
            txCommentTransaction* commentTransaction =
                NS_STATIC_CAST(txCommentTransaction*, aElement);
            handler->comment(commentTransaction->mValue);
            break;
        }
        case txOutputTransaction::eEndElementTransaction:
        {
            txElementTransaction* elementTransaction =
                NS_STATIC_CAST(txElementTransaction*, aElement);
            handler->endElement(elementTransaction->mName,
                                elementTransaction->mNsID);
            break;
        }
        case txOutputTransaction::ePITransaction:
        {
            txPITransaction* piTransaction =
                NS_STATIC_CAST(txPITransaction*, aElement);
            handler->processingInstruction(piTransaction->mTarget,
                                           piTransaction->mData);
            break;
        }
        case txOutputTransaction::eStartDocumentTransaction:
        {
            handler->startDocument();
            break;
        }
        case txOutputTransaction::eStartElementTransaction:
        {
            txElementTransaction* elementTransaction =
                NS_STATIC_CAST(txElementTransaction*, aElement);
            handler->startElement(elementTransaction->mName,
                                  elementTransaction->mNsID);
            break;
        }
    }

    return PR_TRUE;
}

nsresult
txResultBuffer::flushToHandler(txAXMLEventHandler* aHandler)
{
    Holder data;
    data.mHandler = aHandler;
    mStringValue.BeginReading(data.mIter);
    mTransactions.EnumerateForwards(flushTransaction, &data);
    return NS_OK;
}

txOutputTransaction*
txResultBuffer::getLastTransaction()
{
    PRInt32 last = mTransactions.Count() - 1;
    if (last < 0) {
        return nsnull;
    }
    return NS_STATIC_CAST(txOutputTransaction*, mTransactions[last]);
}
