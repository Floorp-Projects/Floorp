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

NS_IMPL_ISUPPORTS2(XPathProcessor,
                   nsIXPathNodeSelector,
                   nsISecurityCheckedComponent)

/**
 * Creates a new XPathProcessor
**/
XPathProcessor::XPathProcessor() {

    NS_INIT_ISUPPORTS();
} //-- XPathProcessor

/**
 * Default destructor
**/
XPathProcessor::~XPathProcessor() {
} //-- ~XPathProcessor

/* nsIDOMNodeList selectNodes (in nsIDOMNode aContextNode, in string aPattern); */
NS_IMETHODIMP XPathProcessor::SelectNodes(nsIDOMNode *aContextNode, const char *aPattern, nsIDOMNodeList **_retval)
{
    nsCOMPtr<nsIDOMDocument> aOwnerDOMDocument;
    aContextNode->GetOwnerDocument(getter_AddRefs(aOwnerDOMDocument));
    nsCOMPtr<nsIDocument> aOwnerDocument = do_QueryInterface(aOwnerDOMDocument);
    Document* aDocument = new Document(aOwnerDOMDocument);
    Node* aNode = new Node(aContextNode, aDocument);

    ProcessorState*  aProcessorState = new ProcessorState(*aDocument, *aDocument);
    ExprParser* aParser = new ExprParser;

    Expr* aExpression = aParser->createExpr(aPattern);
    ExprResult* exprResult = aExpression->evaluate(aNode, aProcessorState);
    if ( exprResult->getResultType() == ExprResult::NODESET ) {
        *_retval = (NodeSet*)exprResult;
    }
    else {
        // Return an empty nodeset
        *_retval = new NodeSet(0);
    }
    NS_ADDREF(*_retval);

    delete aExpression;
    delete aNode;
    delete aDocument;

    return NS_OK;
}

/* 
 * nsISecurityCheckedComponent
 */

static const char* kAllAccess = "AllAccess";

/* string canCreateWrapper (in nsIIDPtr iid); */
NS_IMETHODIMP 
XPathProcessor::CanCreateWrapper(const nsIID * iid, char **_retval)
{
    if (iid->Equals(NS_GET_IID(nsIXPathNodeSelector))) {
        *_retval = nsCRT::strdup(kAllAccess);
    }

    return NS_OK;
}

/* string canCallMethod (in nsIIDPtr iid, in wstring methodName); */
NS_IMETHODIMP 
XPathProcessor::CanCallMethod(const nsIID * iid, const PRUnichar *methodName, char **_retval)
{
    if (iid->Equals(NS_GET_IID(nsIXPathNodeSelector))) {
        *_retval = nsCRT::strdup(kAllAccess);
    }

    return NS_OK;
}

/* string canGetProperty (in nsIIDPtr iid, in wstring propertyName); */
NS_IMETHODIMP 
XPathProcessor::CanGetProperty(const nsIID * iid, const PRUnichar *propertyName, char **_retval)
{
    if (iid->Equals(NS_GET_IID(nsIXPathNodeSelector))) {
        *_retval = nsCRT::strdup(kAllAccess);
    }

    return NS_OK;
}

/* string canSetProperty (in nsIIDPtr iid, in wstring propertyName); */
NS_IMETHODIMP 
XPathProcessor::CanSetProperty(const nsIID * iid, const PRUnichar *propertyName, char **_retval)
{
    if (iid->Equals(NS_GET_IID(nsIXPathNodeSelector))) {
        *_retval = nsCRT::strdup(kAllAccess);
    }

    return NS_OK;
}
