/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 * Peter Van der Beken, peterv@netscape.com
 *    -- original author.
 *
 */

#include "XPathProcessor.h"
#include "dom.h"
#include "ProcessorState.h"
#include "Expr.h"
#include "nsNodeSet.h"
#include "nsIDOMClassInfo.h"


// QueryInterface implementation for XPathProcessor
NS_INTERFACE_MAP_BEGIN(XPathProcessor)
  NS_INTERFACE_MAP_ENTRY(nsIXPathNodeSelector)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY_DOM_CLASSINFO(XPathProcessor)
NS_INTERFACE_MAP_END


NS_IMPL_ADDREF(XPathProcessor)
NS_IMPL_RELEASE(XPathProcessor)


/*
 * Creates a new XPathProcessor
 */
XPathProcessor::XPathProcessor()
{
    NS_INIT_ISUPPORTS();
}

/*
 * Default destructor
 */
XPathProcessor::~XPathProcessor()
{
}

/* nsIDOMNodeList selectNodes (in nsIDOMNode aContextNode, in DOMString aPattern); */
NS_IMETHODIMP XPathProcessor::SelectNodes(nsIDOMNode *aContextNode,
                                          const nsAReadableString & aPattern,
                                          nsIDOMNodeList **_retval)
{
    String pattern(PromiseFlatString(aPattern).get());

    nsCOMPtr<nsIDOMDocument> aOwnerDOMDocument;
    aContextNode->GetOwnerDocument(getter_AddRefs(aOwnerDOMDocument));
    nsCOMPtr<nsIDocument> aOwnerDocument = do_QueryInterface(aOwnerDOMDocument);
    Document* aDocument = new Document(aOwnerDOMDocument);
    Node* aNode = aDocument->createWrapper(aContextNode);

    ProcessorState*  aProcessorState = new ProcessorState();
    ExprParser aParser;

    Expr* aExpression = aParser.createExpr(pattern);
    ExprResult* exprResult = aExpression->evaluate(aNode, aProcessorState);
    nsNodeSet* resultSet;
    if (exprResult->getResultType() == ExprResult::NODESET) {
        resultSet = new nsNodeSet((NodeSet*)exprResult);
    }
    else {
        // Return an empty nodeset
        resultSet = new nsNodeSet(nsnull);
    }
    *_retval = resultSet;
    NS_ADDREF(*_retval);

    delete aProcessorState;
    delete exprResult;
    delete aExpression;
    delete aDocument;

    return NS_OK;
}
