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

txUnknownHandler::txUnknownHandler(txExecutionState* aEs)
    : mEs(aEs)
{
}

txUnknownHandler::~txUnknownHandler()
{
}

void txUnknownHandler::attribute(const nsAString& aName,
                                 const PRInt32 aNsID,
                                 const nsAString& aValue)
{
    // If this is called then the stylesheet is trying to add an attribute
    // without adding an element first. So we'll just ignore it.
    // XXX ErrorReport: Signal this?
}

void txUnknownHandler::endDocument()
{
    // This is an unusual case, no output method has been set and we
    // didn't create a document element. Switching to XML output mode
    // anyway.

    nsresult rv = createHandlerAndFlush(eXMLOutput, EmptyString(),
                                        kNameSpaceID_None);
    if (NS_FAILED(rv))
        return;

    mEs->mResultHandler->endDocument();

    delete this;
}

void txUnknownHandler::startElement(const nsAString& aName,
                                    const PRInt32 aNsID)
{
    nsresult rv = NS_OK;
    txOutputFormat* format = mEs->mStylesheet->getOutputFormat();
    if (format->mMethod != eMethodNotSet) {
        rv = createHandlerAndFlush(format->mMethod, aName, aNsID);
    }
    else if (aNsID == kNameSpaceID_None &&
             aName.Equals(NS_LITERAL_STRING("html"),
                          txCaseInsensitiveStringComparator())) {
        rv = createHandlerAndFlush(eHTMLOutput, aName, aNsID);
    }
    else {
        rv = createHandlerAndFlush(eXMLOutput, aName, aNsID);
    }
    if (NS_FAILED(rv))
        return;

    mEs->mResultHandler->startElement(aName, aNsID);

    delete this;
}

nsresult txUnknownHandler::createHandlerAndFlush(txOutputMethod aMethod,
                                                 const nsAString& aName,
                                                 const PRInt32 aNsID)
{
    NS_ENSURE_TRUE(mBuffer, NS_ERROR_NOT_INITIALIZED);

    txOutputFormat format;
    format.merge(*mEs->mStylesheet->getOutputFormat());
    format.mMethod = aMethod;

    txAXMLEventHandler* handler = 0;
    nsresult rv = mEs->mOutputHandlerFactory->createHandlerWith(&format, aName,
                                                                aNsID,
                                                                &handler);
    NS_ENSURE_SUCCESS(rv, rv);

    mEs->mOutputHandler = handler;
    mEs->mResultHandler = handler;

    return mBuffer->flushToHandler(handler);
}
