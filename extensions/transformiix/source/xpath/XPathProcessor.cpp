/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Peter Van der Beken, peterv@netscape.com
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

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
