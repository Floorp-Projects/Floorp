/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "txRtfHandler.h"
#include "mozilla/Move.h"

using mozilla::Move;

txResultTreeFragment::txResultTreeFragment(nsAutoPtr<txResultBuffer>&& aBuffer)
    : txAExprResult(nullptr),
      mBuffer(Move(aBuffer))
{
}

short txResultTreeFragment::getResultType()
{
    return RESULT_TREE_FRAGMENT;
}

void
txResultTreeFragment::stringValue(nsString& aResult)
{
    if (!mBuffer) {
        return;
    }

    aResult.Append(mBuffer->mStringValue);
}

const nsString*
txResultTreeFragment::stringValuePointer()
{
    return mBuffer ? &mBuffer->mStringValue : nullptr;
}

bool txResultTreeFragment::booleanValue()
{
    return true;
}

double txResultTreeFragment::numberValue()
{
    if (!mBuffer) {
        return 0;
    }

    return txDouble::toDouble(mBuffer->mStringValue);
}

nsresult txResultTreeFragment::flushToHandler(txAXMLEventHandler* aHandler)
{
    if (!mBuffer) {
        return NS_ERROR_FAILURE;
    }

    return mBuffer->flushToHandler(aHandler);
}

nsresult
txRtfHandler::getAsRTF(txAExprResult** aResult)
{
    *aResult = new txResultTreeFragment(Move(mBuffer));
    NS_ENSURE_TRUE(*aResult, NS_ERROR_OUT_OF_MEMORY);

    NS_ADDREF(*aResult);

    return NS_OK;
}

nsresult
txRtfHandler::endDocument(nsresult aResult)
{
    return NS_OK;
}

nsresult
txRtfHandler::startDocument()
{
    return NS_OK;
}
