/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "txUnknownHandler.h"
#include "txExecutionState.h"
#include "txStringUtils.h"
#include "txStylesheet.h"
#include "nsGkAtoms.h"

txUnknownHandler::txUnknownHandler(txExecutionState* aEs)
    : mEs(aEs),
      mFlushed(false)
{
    MOZ_COUNT_CTOR_INHERITED(txUnknownHandler, txBufferingHandler);
}

txUnknownHandler::~txUnknownHandler()
{
    MOZ_COUNT_DTOR_INHERITED(txUnknownHandler, txBufferingHandler);
}

nsresult
txUnknownHandler::attribute(nsIAtom* aPrefix, nsIAtom* aLocalName,
                            nsIAtom* aLowercaseLocalName, int32_t aNsID,
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
                            const int32_t aNsID, const nsString& aValue)
{
    return mFlushed ?
           mEs->mResultHandler->attribute(aPrefix, aLocalName, aNsID, aValue) :
           txBufferingHandler::attribute(aPrefix, aLocalName, aNsID, aValue);
}

nsresult
txUnknownHandler::characters(const nsSubstring& aData, bool aDOE)
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

        nsresult rv = createHandlerAndFlush(false, EmptyString(),
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
                               nsIAtom* aLowercaseLocalName, int32_t aNsID)
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

        bool htmlRoot = aNsID == kNameSpaceID_None && !aPrefix &&
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
                               const int32_t aNsID)
{
    if (!mFlushed) {
        // Make sure that mEs->mResultHandler == this is true, otherwise we'll
        // leak mEs->mResultHandler in createHandlerAndFlush.
        NS_ASSERTION(mEs->mResultHandler == this,
                     "We're leaking mEs->mResultHandler.");

        bool htmlRoot = aNsID == kNameSpaceID_None && !aPrefix &&
                          aLocalName.Equals(NS_LITERAL_STRING("html"),
                                            txCaseInsensitiveStringComparator());
        nsresult rv = createHandlerAndFlush(htmlRoot, aLocalName, aNsID);
        NS_ENSURE_SUCCESS(rv, rv);
    }

    return mEs->mResultHandler->startElement(aPrefix, aLocalName, aNsID);
}

nsresult txUnknownHandler::createHandlerAndFlush(bool aHTMLRoot,
                                                 const nsSubstring& aName,
                                                 const int32_t aNsID)
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

    mFlushed = true;

    // Let go of out buffer as soon as we're done flushing it, we're not going
    // to need it anymore from this point on (all hooks get forwarded to
    // mEs->mResultHandler.
    nsAutoPtr<txResultBuffer> buffer(mBuffer);
    return buffer->flushToHandler(mEs->mResultHandler);
}
