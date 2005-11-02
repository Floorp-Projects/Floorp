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
#include <string.h>
#include "ProcessorState.h"

#ifndef TX_EXE
NS_IMPL_ISUPPORTS1(txUnknownHandler, txIOutputXMLEventHandler);
#endif

PRUint32 txUnknownHandler::kReasonableTransactions = 8;

txUnknownHandler::txUnknownHandler(ProcessorState* aPs)
    : mTotal(0), mMax(kReasonableTransactions), 
      mPs(aPs)
{
#ifndef TX_EXE
    NS_INIT_ISUPPORTS();
#endif

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
    // If this is called then the stylesheet is trying to add an attribute
    // without adding an element first. So we'll just ignore it.
    // XXX ErrorReport: Signal this?
}

void txUnknownHandler::characters(const String& aData)
{
    txOneStringTransaction* transaction =
        new txOneStringTransaction(txOutputTransaction::eCharacterTransaction,
                                   aData);
    if (!transaction) {
        NS_ASSERTION(0, "Out of memory!");
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
        NS_ASSERTION(0, "Out of memory!");
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
        NS_ASSERTION(0, "Out of memory!");
        return;
    }
    addTransaction(transaction);
}

void txUnknownHandler::endDocument()
{
    // This is an unusual case, no output method has been set and we
    // didn't create a document element. Switching to XML output mode
    // anyway.

#ifndef TX_EXE
    // Make sure that we don't get deleted while this function is executed and
    // we set a new outputhandler
    nsCOMPtr<txIOutputXMLEventHandler> kungFuDeathGrip(this);
#endif
    nsresult rv = createHandlerAndFlush(eXMLOutput, String(),
                                        kNameSpaceID_None);
    if (NS_FAILED(rv))
        return;

    mPs->mResultHandler->endDocument();

    // in module the outputhandler is refcounted
#ifdef TX_EXE
    delete this;
#endif
}

void txUnknownHandler::endElement(const String& aName,
                                  const PRInt32 aNsID)
{
    NS_ASSERTION(0, "This shouldn't be called");
}

void txUnknownHandler::processingInstruction(const String& aTarget,
                                             const String& aData)
{
    txTwoStringTransaction* transaction =
        new txTwoStringTransaction(txOutputTransaction::ePITransaction,
                                   aTarget, aData);
    if (!transaction) {
        NS_ASSERTION(0, "Out of memory!");
        return;
    }
    addTransaction(transaction);
}

void txUnknownHandler::startDocument()
{
    txOutputTransaction* transaction =
        new txOutputTransaction(txOutputTransaction::eStartDocumentTransaction);
    if (!transaction) {
        NS_ASSERTION(0, "Out of memory!");
        return;
    }
    addTransaction(transaction);
}

void txUnknownHandler::startElement(const String& aName,
                                    const PRInt32 aNsID)
{
#ifndef TX_EXE
    // Make sure that we don't get deleted while this function is executed and
    // we set a new outputhandler
    nsCOMPtr<txIOutputXMLEventHandler> kungFuDeathGrip(this);
#endif

    nsresult rv = NS_OK;
    txOutputFormat* format = mPs->getOutputFormat();
    if (format->mMethod != eMethodNotSet) {
        rv = createHandlerAndFlush(format->mMethod, aName, aNsID);
    }
    else if (aNsID == kNameSpaceID_None &&
             aName.isEqualIgnoreCase(String(NS_LITERAL_STRING("html")))) {
        rv = createHandlerAndFlush(eHTMLOutput, aName, aNsID);
    }
    else {
        rv = createHandlerAndFlush(eXMLOutput, aName, aNsID);
    }
    if (NS_FAILED(rv))
        return;

    mPs->mResultHandler->startElement(aName, aNsID);

    // in module the outputhandler is refcounted
#ifdef TX_EXE
    delete this;
#endif
}

#ifndef TX_EXE
void txUnknownHandler::getOutputDocument(nsIDOMDocument** aDocument)
{
    *aDocument = nsnull;
}
#endif

nsresult txUnknownHandler::createHandlerAndFlush(txOutputMethod aMethod,
                                                 const String& aName,
                                                 const PRInt32 aNsID)
{
    nsresult rv = NS_OK;
    txOutputFormat* format = mPs->getOutputFormat();
    format->mMethod = aMethod;

    txIOutputXMLEventHandler* handler = 0;
    rv = mPs->mOutputHandlerFactory->createHandlerWith(format, aName, aNsID,
                                                       &handler);
    NS_ENSURE_SUCCESS(rv, rv);

    mPs->mOutputHandler = handler;
    mPs->mResultHandler = handler;

    MBool hasDOE = handler->hasDisableOutputEscaping();

    PRUint32 counter;
    for (counter = 0; counter < mTotal; ++counter) {
        switch (mArray[counter]->mType) {
            case txOutputTransaction::eCharacterTransaction:
            {
                txOneStringTransaction* transaction = (txOneStringTransaction*)mArray[counter];
                handler->characters(transaction->mString);
                delete transaction;
                break;
            }
            case txOutputTransaction::eCharacterNoOETransaction:
            {
                txOneStringTransaction* transaction = (txOneStringTransaction*)mArray[counter];
                if (hasDOE) {
                    handler->charactersNoOutputEscaping(transaction->mString);
                }
                else {
                    handler->characters(transaction->mString);
                }
                delete transaction;
                break;
            }
            case txOutputTransaction::eCommentTransaction:
            {
                txOneStringTransaction* transaction = (txOneStringTransaction*)mArray[counter];
                handler->comment(transaction->mString);
                delete transaction;
                break;
            }
            case txOutputTransaction::ePITransaction:
            {
                txTwoStringTransaction* transaction = (txTwoStringTransaction*)mArray[counter];
                handler->processingInstruction(transaction->mStringOne,
                                                transaction->mStringTwo);
                delete transaction;
                break;
            }
            case txOutputTransaction::eStartDocumentTransaction:
            {
                handler->startDocument();
                delete mArray[counter];
                break;
            }
        }
    }
    mTotal = 0;

    return NS_OK;
}

void txUnknownHandler::addTransaction(txOutputTransaction* aTransaction)
{
    if (mTotal == mMax) {
        PRUint32 newMax = mMax * 2;
        txOutputTransaction** newArray = new txOutputTransaction*[newMax];
        if (!newArray) {
            NS_ASSERTION(0, "Out of memory!");
            return;
        }
        mMax = newMax;
        memcpy(newArray, mArray, mTotal * sizeof(txOutputTransaction*));
        delete [] mArray;
        mArray = newArray;
    }
    mArray[mTotal++] = aTransaction;
}
