/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- 
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
 * The Original Code is TransforMiiX XSLT processor.
 * 
 * The Initial Developer of the Original Code is The MITRE Corporation.
 * Portions created by MITRE are Copyright (C) 1999 The MITRE Corporation.
 *
 * Portions created by Keith Visco as a Non MITRE employee,
 * (C) 1999 Keith Visco. All Rights Reserved.
 * 
 * Contributor(s): 
 * Keith Visco, kvisco@ziplink.net
 *   -- original author.
 *
 */

#include "Expr.h"
#include "XMLDOMUtils.h"

/*
 This class represents a ElementExpr as defined by XPath 1.0
  proposed recommendation
*/


const String ElementExpr::WILD_CARD = "*";


//- Constructors -/
ElementExpr::ElementExpr(String& name)
{
    int idx = name.indexOf(':');
    if (idx >= 0)
       name.subString(0, idx, prefix);
    else
       idx = -1;

    name.subString(idx+1, this->name);

    //-- set flags
    isNameWild = this->name.isEqual(WILD_CARD);
    isNamespaceWild = (isNameWild && prefix.isEmpty());
} //-- ElementExpr

  //------------------/
 //- Public Methods -/
//------------------/

/**
 * Evaluates this Expr based on the given context node and processor state
 * @param context the context node for evaluation of this Expr
 * @param ps the ContextState containing the stack information needed
 * for evaluation
 * @return the result of the evaluation
**/
ExprResult* ElementExpr::evaluate(Node* context, ContextState* cs) {

    NodeSet* nodeSet = new NodeSet();

    if ( !context ) return nodeSet;

    Node* node = context->getFirstChild();
    while (node) {
        if (matches(node, context, cs))
            nodeSet->add(node);
        node = node->getNextSibling();
    }
    return nodeSet;
} //-- evaluate

/**
 * Returns the default priority of this Pattern based on the given Node,
 * context Node, and ContextState.
**/
double ElementExpr::getDefaultPriority(Node* node, Node* context, ContextState* cs) {
    if (!isNameWild)
        return 0;
    if (!isNamespaceWild)
        return -0.25;
    return -0.5;
} //-- getDefaultPriority

  //-----------------------------/
 //- Methods from NodeExpr.cpp -/
//-----------------------------/

/**
 * Determines whether this NodeExpr matches the given node within
 * the given context
**/
MBool ElementExpr::matches(Node* node, Node* context, ContextState* cs) {

    if ((!node) || (node->getNodeType() != Node::ELEMENT_NODE ))
        return MB_FALSE;

    if (isNamespaceWild && isNameWild) return MB_TRUE;

    const String nodeName = node->getNodeName();

    int idx = nodeName.indexOf(':');

    if (!isNamespaceWild) {
        //-- compare namespaces
        String nsURI;
        // use context to get namespace for testing against
        if (!prefix.isEmpty())
            cs->getNameSpaceURIFromPrefix(prefix, nsURI);

        String nsURI2;
        String prefix2;
        if (idx > 0) nodeName.subString(0, idx, prefix2);
        // use source tree to aquire namespace for node
        XMLDOMUtils::getNameSpace(prefix2, (Element*) node, nsURI2);

        if (!nsURI.isEqual(nsURI2)) return MB_FALSE;
    }

    if (this->isNameWild) return MB_TRUE;
    //-- compare local names
    if (idx < 0) return nodeName.isEqual(this->name);
    else {
        String local;
        nodeName.subString(idx+1, local);
        return local.isEqual(this->name);
    }
} //-- matches


/**
 * Returns the String representation of this NodeExpr.
 * @param dest the String to use when creating the String
 * representation. The String representation will be appended to
 * any data in the destination String, to allow cascading calls to
 * other #toString() methods for Expressions.
 * @return the String representation of this NodeExpr.
**/
void ElementExpr::toString(String& dest) {
    dest.append(this->name);
} //-- toString

