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
#include "txAtoms.h"

txUnknownHandler::txUnknownHandler(txExecutionState* aEs)
    : mEs(aEs)
{
}

nsresult
txUnknownHandler::endDocument(nsresult aResult)
{
    if (NS_FAILED(aResult)) {
        return NS_OK;
    }

    // This is an unusual case, no output method has been set and we
    // didn't create a document element. Switching to XML output mode
    // anyway.

    // Make sure that mEs->mResultHandler == this is true, otherwise we'll
    // leak mEs->mResultHandler in createHandlerAndFlush and we'll crash on
    // the last line (delete this).
    NS_ASSERTION(mEs->mResultHandler == this,
                 "We're leaking mEs->mResultHandler and are going to crash.");

    nsresult rv = createHandlerAndFlush(PR_FALSE, EmptyString(),
                                        kNameSpaceID_None);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mEs->mResultHandler->endDocument(aResult);

    delete this;

    return rv;
}

nsresult
txUnknownHandler::startElement(nsIAtom* aPrefix, nsIAtom* aLocalName,
                               nsIAtom* aLowercaseLocalName, PRInt32 aNsID)
{
    // Make sure that mEs->mResultHandler == this is true, otherwise we'll
    // leak mEs->mResultHandler in createHandlerAndFlush and we may crash
    // later on trying to delete this handler again.
    NS_ASSERTION(mEs->mResultHandler == this,
                 "We're leaking mEs->mResultHandler.");

    nsCOMPtr<nsIAtom> owner;
    if (!aLowercaseLocalName) {
        owner = TX_ToLowerCaseAtom(aLocalName);
        NS_ENSURE_TRUE(owner, NS_ERROR_OUT_OF_MEMORY);
        
        aLowercaseLocalName = owner;
    }

    PRBool htmlRoot = aNsID == kNameSpaceID_None && !aPrefix &&
                      aLowercaseLocalName == txHTMLAtoms::html;

    // Use aLocalName and not aLowercaseLocalName in case the output
    // handler cares about case. For eHTMLOutput the handler will hardcode
    // to 'html' anyway.
    nsAutoString name;
    aLocalName->ToString(name);
    nsresult rv = createHandlerAndFlush(htmlRoot, name, aNsID);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mEs->mResultHandler->startElement(aPrefix, aLocalName,
                                           aLowercaseLocalName, aNsID);

    delete this;

    return rv;
}

nsresult
txUnknownHandler::startElement(nsIAtom* aPrefix, const nsSubstring& aLocalName,
                               const PRInt32 aNsID)
{
    // Make sure that mEs->mResultHandler == this is true, otherwise we'll
    // leak mEs->mResultHandler in createHandlerAndFlush and we may crash
    // later on trying to delete this handler again.
    NS_ASSERTION(mEs->mResultHandler == this,
                 "We're leaking mEs->mResultHandler.");

    PRBool htmlRoot = aNsID == kNameSpaceID_None && !aPrefix &&
                      aLocalName.Equals(NS_LITERAL_STRING("html"),
                                        txCaseInsensitiveStringComparator());
    nsresult rv = createHandlerAndFlush(htmlRoot, aLocalName, aNsID);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mEs->mResultHandler->startElement(aPrefix, aLocalName, aNsID);

    delete this;

    return rv;
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

    txAXMLEventHandler *handler = nsnull;
    nsresult rv = mEs->mOutputHandlerFactory->createHandlerWith(&format, aName,
                                                                aNsID,
                                                                &handler);
    NS_ENSURE_SUCCESS(rv, rv);

    mEs->mOutputHandler = handler;
    mEs->mResultHandler = handler;

    return mBuffer->flushToHandler(&handler);
}
