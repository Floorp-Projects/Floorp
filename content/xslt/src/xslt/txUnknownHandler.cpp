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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "txUnknownHandler.h"
#include "txExecutionState.h"
#include "txStringUtils.h"
#include "txStylesheet.h"
#include "nsGkAtoms.h"

txUnknownHandler::txUnknownHandler(txExecutionState* aEs)
    : mEs(aEs),
      mFlushed(PR_FALSE)
{
    MOZ_COUNT_CTOR_INHERITED(txUnknownHandler, txBufferingHandler);
}

txUnknownHandler::~txUnknownHandler()
{
    MOZ_COUNT_DTOR_INHERITED(txUnknownHandler, txBufferingHandler);
}

nsresult
txUnknownHandler::attribute(nsIAtom* aPrefix, nsIAtom* aLocalName,
                            nsIAtom* aLowercaseLocalName, PRInt32 aNsID,
                            const nsString& aValue)
{
    return mFlushed ?
           mEs->mResultHandler->attribute(aPrefix, aLocalName,
                                          aLowercaseLocalName, aNsID, aValue) :
           txBufferingHandler::attribute(aPrefix, aLocalName,
                                         aLowercaseLocalName, aNsID, aValue);
}

nsresult
txUnknownHandler::attribute(nsIAtom* aPrefix, const nsSubstring& aLocalName,
                            const PRInt32 aNsID, const nsString& aValue)
{
    return mFlushed ?
           mEs->mResultHandler->attribute(aPrefix, aLocalName, aNsID, aValue) :
           txBufferingHandler::attribute(aPrefix, aLocalName, aNsID, aValue);
}

nsresult
txUnknownHandler::characters(const nsSubstring& aData, PRBool aDOE)
{
    return mFlushed ?
           mEs->mResultHandler->characters(aData, aDOE) :
           txBufferingHandler::characters(aData, aDOE);
}

nsresult
txUnknownHandler::comment(const nsString& aData)
{
    return mFlushed ?
           mEs->mResultHandler->comment(aData) :
           txBufferingHandler::comment(aData);
}

nsresult
txUnknownHandler::endDocument(nsresult aResult)
{
    if (!mFlushed) {
        if (NS_FAILED(aResult)) {
            return NS_OK;
        }

        // This is an unusual case, no output method has been set and we
        // didn't create a document element. Switching to XML output mode
        // anyway.

        // Make sure that mEs->mResultHandler == this is true, otherwise we'll
        // leak mEs->mResultHandler in createHandlerAndFlush.
        NS_ASSERTION(mEs->mResultHandler == this,
                     "We're leaking mEs->mResultHandler.");

        nsresult rv = createHandlerAndFlush(PR_FALSE, EmptyString(),
                                            kNameSpaceID_None);
        NS_ENSURE_SUCCESS(rv, rv);
    }

    return mEs->mResultHandler->endDocument(aResult);
}

nsresult
txUnknownHandler::endElement()
{
    return mFlushed ?
           mEs->mResultHandler->endElement() :
           txBufferingHandler::endElement();
}

nsresult
txUnknownHandler::processingInstruction(const nsString& aTarget,
                                        const nsString& aData)
{
    return mFlushed ?
           mEs->mResultHandler->processingInstruction(aTarget, aData) :
           txBufferingHandler::processingInstruction(aTarget, aData);
}

nsresult
txUnknownHandler::startDocument()
{
    return mFlushed ?
           mEs->mResultHandler->startDocument() :
           txBufferingHandler::startDocument();
}

nsresult
txUnknownHandler::startElement(nsIAtom* aPrefix, nsIAtom* aLocalName,
                               nsIAtom* aLowercaseLocalName, PRInt32 aNsID)
{
    if (!mFlushed) {
        // Make sure that mEs->mResultHandler == this is true, otherwise we'll
        // leak mEs->mResultHandler in createHandlerAndFlush.
        NS_ASSERTION(mEs->mResultHandler == this,
                     "We're leaking mEs->mResultHandler.");

        nsCOMPtr<nsIAtom> owner;
        if (!aLowercaseLocalName) {
            owner = TX_ToLowerCaseAtom(aLocalName);
            NS_ENSURE_TRUE(owner, NS_ERROR_OUT_OF_MEMORY);

            aLowercaseLocalName = owner;
        }

        PRBool htmlRoot = aNsID == kNameSpaceID_None && !aPrefix &&
                          aLowercaseLocalName == nsGkAtoms::html;

        // Use aLocalName and not aLowercaseLocalName in case the output
        // handler cares about case. For eHTMLOutput the handler will hardcode
        // to 'html' anyway.
        nsresult rv = createHandlerAndFlush(htmlRoot,
                                            nsDependentAtomString(aLocalName),
                                            aNsID);
        NS_ENSURE_SUCCESS(rv, rv);
    }

    return mEs->mResultHandler->startElement(aPrefix, aLocalName,
                                             aLowercaseLocalName, aNsID);
}

nsresult
txUnknownHandler::startElement(nsIAtom* aPrefix, const nsSubstring& aLocalName,
                               const PRInt32 aNsID)
{
    if (!mFlushed) {
        // Make sure that mEs->mResultHandler == this is true, otherwise we'll
        // leak mEs->mResultHandler in createHandlerAndFlush.
        NS_ASSERTION(mEs->mResultHandler == this,
                     "We're leaking mEs->mResultHandler.");

        PRBool htmlRoot = aNsID == kNameSpaceID_None && !aPrefix &&
                          aLocalName.Equals(NS_LITERAL_STRING("html"),
                                            txCaseInsensitiveStringComparator());
        nsresult rv = createHandlerAndFlush(htmlRoot, aLocalName, aNsID);
        NS_ENSURE_SUCCESS(rv, rv);
    }

    return mEs->mResultHandler->startElement(aPrefix, aLocalName, aNsID);
}

nsresult txUnknownHandler::createHandlerAndFlush(PRBool aHTMLRoot,
                                                 const nsSubstring& aName,
                                                 const PRInt32 aNsID)
{
    NS_ENSURE_TRUE(mBuffer, NS_ERROR_NOT_INITIALIZED);

    txOutputFormat format;
    format.merge(*(mEs->mStylesheet->getOutputFormat()));
    if (format.mMethod == eMethodNotSet) {
        format.mMethod = aHTMLRoot ? eHTMLOutput : eXMLOutput;
    }

    nsAutoPtr<txAXMLEventHandler> handler;
    nsresult rv = mEs->mOutputHandlerFactory->createHandlerWith(&format, aName,
                                                                aNsID,
                                                                getter_Transfers(handler));
    NS_ENSURE_SUCCESS(rv, rv);

    mEs->mOutputHandler = handler;
    mEs->mResultHandler = handler.forget();
    // Let the executionstate delete us. We need to stay alive because we might
    // need to forward hooks to mEs->mResultHandler if someone is currently
    // flushing a buffer to mEs->mResultHandler.
    mEs->mObsoleteHandler = this;

    mFlushed = PR_TRUE;

    // Let go of out buffer as soon as we're done flushing it, we're not going
    // to need it anymore from this point on (all hooks get forwarded to
    // mEs->mResultHandler.
    nsAutoPtr<txResultBuffer> buffer(mBuffer);
    return buffer->flushToHandler(mEs->mResultHandler);
}
