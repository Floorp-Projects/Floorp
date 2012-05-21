/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "txTextHandler.h"
#include "nsAString.h"

txTextHandler::txTextHandler(bool aOnlyText) : mLevel(0),
                                                mOnlyText(aOnlyText)
{
}

nsresult
txTextHandler::attribute(nsIAtom* aPrefix, nsIAtom* aLocalName,
                         nsIAtom* aLowercaseLocalName, PRInt32 aNsID,
                         const nsString& aValue)
{
    return NS_OK;
}

nsresult
txTextHandler::attribute(nsIAtom* aPrefix, const nsSubstring& aLocalName,
                         const PRInt32 aNsID,
                         const nsString& aValue)
{
    return NS_OK;
}

nsresult
txTextHandler::characters(const nsSubstring& aData, bool aDOE)
{
    if (mLevel == 0)
        mValue.Append(aData);

    return NS_OK;
}

nsresult
txTextHandler::comment(const nsString& aData)
{
    return NS_OK;
}

nsresult
txTextHandler::endDocument(nsresult aResult)
{
    return NS_OK;
}

nsresult
txTextHandler::endElement()
{
    if (mOnlyText)
        --mLevel;

    return NS_OK;
}

nsresult
txTextHandler::processingInstruction(const nsString& aTarget, const nsString& aData)
{
    return NS_OK;
}

nsresult
txTextHandler::startDocument()
{
    return NS_OK;
}

nsresult
txTextHandler::startElement(nsIAtom* aPrefix, nsIAtom* aLocalName,
                            nsIAtom* aLowercaseLocalName, const PRInt32 aNsID)
{
    if (mOnlyText)
        ++mLevel;

    return NS_OK;
}

nsresult
txTextHandler::startElement(nsIAtom* aPrefix, const nsSubstring& aLocalName,
                            const PRInt32 aNsID)
{
    if (mOnlyText)
        ++mLevel;

    return NS_OK;
}
