/*-*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
  This class represents a Attribute Expression as defined by the XPath
  1.0 Recommendation
*/

const String AttributeExpr::WILD_CARD = "*";

//- Constructors -/

AttributeExpr::AttributeExpr(String& name)
{
    if (name.isEqual(WILD_CARD)) {
        isNameWild      = MB_TRUE;
        isNamespaceWild = MB_TRUE;
        return;
    }

    int idx = name.indexOf(':');
    if (idx >= 0)
       name.subString(0, idx, prefix);
    else
       idx = -1;

    name.subString(idx+1, this->name);

    //-- set flags
    isNamespaceWild = MB_FALSE;
    isNameWild      = this->name.isEqual(WILD_CARD);
} //-- AttributeExpr

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
ExprResult* AttributeExpr::evaluate(Node* context, ContextState* cs) {

    NodeSet* nodeSet = new NodeSet();
    if ( !context ) return nodeSet;
    NamedNodeMap* atts = context->getAttributes();
    if ( atts ) {
        PRUint32 i = 0;
        if ( isNameWild && isNamespaceWild ) {
            for ( ; i < atts->getLength(); i++ )
                nodeSet->add(atts->item(i));
        }
        else {
            for ( ; i < atts->getLength(); i++ ) {
                Node* attr = atts->item(i);
                if (matches(attr, context, cs)) {
                    nodeSet->add(attr);
                    if (!isNameWild) break;
                }
            }
        }
    }
    return nodeSet;
} //-- evaluate

/**
 * Returns the default priority of this Pattern based on the given Node,
 * context Node, and ContextState.
**/
double AttributeExpr::getDefaultPriority(Node* node, Node* context, ContextState* cs) {
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
MBool AttributeExpr::matches(Node* node, Node* context, ContextState* cs) {

    //XXX need to filter out namespace-declaration attributes!

    if ( (!node) || (node->getNodeType() != Node::ATTRIBUTE_NODE) )
        return  MB_FALSE;

    if ( isNameWild && isNamespaceWild ) return MB_TRUE;

    const String nodeName = ((Attr*)node)->getName();
    int idx = nodeName.indexOf(':');
    if (idx >= 0) {
        String prefixForNode;
        nodeName.subString(0,idx,prefixForNode);
        String localName;
        nodeName.subString(idx+1, localName);
        if (isNamespaceWild) return localName.isEqual(this->name);
        String nsForNode;
        Node* parent = node->getXPathParent();
        if (parent) 
            XMLDOMUtils::getNameSpace(prefixForNode, (Element*)parent,
                                      nsForNode);
        String nsForTest;
        if (!prefix.isEmpty())
            cs->getNameSpaceURIFromPrefix(prefix, nsForTest);
        if (!nsForTest.isEqual(nsForNode)) return MB_FALSE;
        return localName.isEqual(this->name);
    }
    else {
        if (isNamespaceWild) return nodeName.isEqual(this->name);
        String nsForTest;
        if (!prefix.isEmpty())
            cs->getNameSpaceURIFromPrefix(prefix, nsForTest);
        if (!nsForTest.isEmpty())
            return MB_FALSE;
        return nodeName.isEqual(this->name);
    }
    return MB_FALSE;
} //-- matches


/**
 * Returns the String representation of this NodeExpr.
 * @param dest the String to use when creating the String
 * representation. The String representation will be appended to
 * any data in the destination String, to allow cascading calls to
 * other #toString() methods for Expressions.
 * @return the String representation of this NodeExpr.
**/
void AttributeExpr::toString(String& dest) {
    if (isNameWild && isNamespaceWild) dest.append('*');
    else {
       dest.append(this->prefix);
       dest.append(':');
       dest.append(this->name);
    }
} //-- toString

