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

#ifndef TRANSFRMX_UNKNOWN_HANDLER_H
#define TRANSFRMX_UNKNOWN_HANDLER_H

#include "txXMLEventHandler.h"
#include "TxString.h"
#include "txOutputFormat.h"

class ProcessorState;

class txOutputTransaction
{
public:
    enum txTransactionType {
        eCharacterTransaction,
        eCharacterNoOETransaction,
        eCommentTransaction,
        ePITransaction,
        eStartDocumentTransaction
    };
    txOutputTransaction(txTransactionType aType)
        : mType(aType)
    {
    };
    virtual ~txOutputTransaction()
    {
    };
    txTransactionType mType;
};

class txOneStringTransaction : public txOutputTransaction
{
public:
    txOneStringTransaction(txTransactionType aType,
                           String aString)
        : txOutputTransaction(aType),
          mString(aString)
    {
    };
    String mString;
};

class txTwoStringTransaction : public txOutputTransaction
{
public:
    txTwoStringTransaction(txTransactionType aType,
                           String aStringOne,
                           String aStringTwo)
        : txOutputTransaction(aType),
          mStringOne(aStringOne),
          mStringTwo(aStringTwo)
    {
    };
    String mStringOne;
    String mStringTwo;
};

class txUnknownHandler : public txIOutputXMLEventHandler
{
public:
    txUnknownHandler(ProcessorState* aPs);
    virtual ~txUnknownHandler();

#ifndef TX_EXE
    NS_DECL_ISUPPORTS
#endif

    /*
     * Signals to receive the start of an attribute.
     *
     * @param aName the name of the attribute
     * @param aNsID the namespace ID of the attribute
     * @param aValue the value of the attribute
     */
    void attribute(const String& aName,
                   const PRInt32 aNsID,
                   const String& aValue);

    /*
     * Signals to receive characters.
     *
     * @param aData the characters to receive
     */
    void characters(const String& aData);

    /*
     * Signals to receive characters that don't need output escaping.
     *
     * @param aData the characters to receive
     */
    void charactersNoOutputEscaping(const String& aData);

    /*
     * Signals to receive data that should be treated as a comment.
     *
     * @param data the comment data to receive
     */
    void comment(const String& aData);

    /*
     * Signals the end of a document. It is an error to call
     * this method more than once.
     */
    void endDocument();

    /*
     * Signals to receive the end of an element.
     *
     * @param aName the name of the element
     * @param aNsID the namespace ID of the element
     */
    void endElement(const String& aName,
                    const PRInt32 aNsID);

    /**
     * Returns whether the output handler supports
     * disable-output-escaping.
     *
     * @return MB_TRUE if this handler supports
     *                 disable-output-escaping
     */
    MBool hasDisableOutputEscaping()
    {
        return MB_TRUE;
    }

    /*
     * Signals to receive a processing instruction.
     *
     * @param aTarget the target of the processing instruction
     * @param aData the data of the processing instruction
     */
    void processingInstruction(const String& aTarget, 
                               const String& aData);

    /*
     * Signals the start of a document.
     */
    void startDocument();

    /*
     * Signals to receive the start of an element.
     *
     * @param aName the name of the element
     * @param aNsID the namespace ID of the element
     */
    void startElement(const String& aName,
                      const PRInt32 aNsID);

#ifndef TX_EXE
    /**
     * Gets the Mozilla output document
     *
     * @param aDocument the Mozilla output document
     */
    void getOutputDocument(nsIDOMDocument** aDocument);
#endif

private:
    nsresult createHandlerAndFlush(txOutputMethod aMethod,
                                   const String& aName,
                                   const PRInt32 aNsID);
    void addTransaction(txOutputTransaction* aTransaction);

    PRUint32 mTotal, mMax;
    /*
     * XXX we shouldn't hold to the ProcessorState, as we're supposed
     * to live without it. But as a standalone handler, we don't.
     * The right fix may need a txOutputFormat here.
     */
    ProcessorState* mPs;
    txOutputTransaction** mArray;

    static PRUint32 kReasonableTransactions;
};

#endif
