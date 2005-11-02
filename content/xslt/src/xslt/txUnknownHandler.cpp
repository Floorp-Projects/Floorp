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
 * The Original Code is the TransforMiiX XSLT processor.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "txUnknownHandler.h"

PRUint32 txUnknownHandler::kReasonableTransactions = 8;

txUnknownHandler::txUnknownHandler() : mTotal(0),
                                       mMax(kReasonableTransactions)
{
    mArray = new txOutputTransaction*[kReasonableTransactions];
}

txUnknownHandler::~txUnknownHandler()
{
    PRUint32 counter;
    for (counter = 0; counter < mTotal; ++counter) {
        delete mArray[counter];
    }
    delete [] mArray;
}

void txUnknownHandler::attribute(const String& aName,
                                 const PRInt32 aNsID,
                                 const String& aValue)
{
    NS_ASSERTION(0, "This shouldn't be called")
}

void txUnknownHandler::characters(const String& aData)
{
    txOneStringTransaction* transaction =
        new txOneStringTransaction(txOutputTransaction::eCharacterTransaction,
                                   aData);
    if (!transaction) {
        NS_ASSERTION(0, "Out of memory!")
        return;
    }
    addTransaction(transaction);
}

void txUnknownHandler::charactersNoOutputEscaping(const String& aData)
{
    txOneStringTransaction* transaction =
        new txOneStringTransaction(txOutputTransaction::eCharacterNoOETransaction,
                                   aData);
    if (!transaction) {
        NS_ASSERTION(0, "Out of memory!")
        return;
    }
    addTransaction(transaction);
}

void txUnknownHandler::comment(const String& aData)
{
    txOneStringTransaction* transaction =
        new txOneStringTransaction(txOutputTransaction::eCommentTransaction,
                                   aData);
    if (!transaction) {
        NS_ASSERTION(0, "Out of memory!")
        return;
    }
    addTransaction(transaction);
}

void txUnknownHandler::endDocument()
{
    txOutputTransaction* transaction =
        new txOutputTransaction(txOutputTransaction::eEndDocumentTransaction);
    if (!transaction) {
        NS_ASSERTION(0, "Out of memory!")
        return;
    }
    addTransaction(transaction);
}

void txUnknownHandler::endElement(const String& aName,
                                  const PRInt32 aNsID)
{
    NS_ASSERTION(0, "This shouldn't be called")
}

void txUnknownHandler::processingInstruction(const String& aTarget,
                                             const String& aData)
{
    txTwoStringTransaction* transaction =
        new txTwoStringTransaction(txOutputTransaction::ePITransaction,
                                   aTarget, aData);
    if (!transaction) {
        NS_ASSERTION(0, "Out of memory!")
        return;
    }
    addTransaction(transaction);
}

void txUnknownHandler::startDocument()
{
    txOutputTransaction* transaction =
        new txOutputTransaction(txOutputTransaction::eStartDocumentTransaction);
    if (!transaction) {
        NS_ASSERTION(0, "Out of memory!")
        return;
    }
    addTransaction(transaction);
}

void txUnknownHandler::startElement(const String& aName,
                                    const PRInt32 aNsID)
{
    NS_ASSERTION(0, "This shouldn't be called")
}

void txUnknownHandler::setOutputFormat(txOutputFormat* aOutputFormat)
{
}

void txUnknownHandler::getOutputStream(ostream** aOutputStream)
{
    if (aOutputStream) {
        *aOutputStream = mOut;
    }
}

void txUnknownHandler::setOutputStream(ostream* aOutputStream)
{
    mOut = aOutputStream;
}

void txUnknownHandler::flush(txStreamXMLEventHandler* aHandler)
{
    PRUint32 counter;
    for (counter = 0; counter < mTotal; ++counter) {
        switch (mArray[counter]->mType) {
            case txOutputTransaction::eCharacterTransaction:
            {
                txOneStringTransaction* transaction = (txOneStringTransaction*)mArray[counter];
                aHandler->characters(transaction->mString);
                delete transaction;
                break;
            }
            case txOutputTransaction::eCharacterNoOETransaction:
            {
                txOneStringTransaction* transaction = (txOneStringTransaction*)mArray[counter];
                aHandler->charactersNoOutputEscaping(transaction->mString);
                delete transaction;
                break;
            }
            case txOutputTransaction::eCommentTransaction:
            {
                txOneStringTransaction* transaction = (txOneStringTransaction*)mArray[counter];
                aHandler->comment(transaction->mString);
                delete transaction;
                break;
            }
            case txOutputTransaction::eEndDocumentTransaction:
            {
                aHandler->endDocument();
                delete mArray[counter];
                break;
            }
            case txOutputTransaction::ePITransaction:
            {
                txTwoStringTransaction* transaction = (txTwoStringTransaction*)mArray[counter];
                aHandler->processingInstruction(transaction->mStringOne,
                                                transaction->mStringTwo);
                delete transaction;
                break;
            }
            case txOutputTransaction::eStartDocumentTransaction:
            {
                aHandler->startDocument();
                delete mArray[counter];
                break;
            }
        }
    }
    mTotal = 0;
}

void txUnknownHandler::addTransaction(txOutputTransaction* aTransaction)
{
    if (mTotal == mMax) {
        PRUint32 newMax = mMax * 2;
        txOutputTransaction** newArray = new txOutputTransaction*[newMax];
        if (!newArray) {
            NS_ASSERTION(0, "Out of memory!")
            return;
        }
        mMax = newMax;
        memcpy(newArray, mArray, mTotal * sizeof(txOutputTransaction*));
        delete [] mArray;
        mArray = newArray;
    }
    mArray[mTotal++] = aTransaction;
}
