/*
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
 * $Id: ElementExpr.cpp,v 1.2 2000/11/07 10:49:07 kvisco%ziplink.net Exp $
 */

#include "Expr.h"

/*
 This class represents a ElementExpr as defined by XPath 1.0
  proposed recommendation
*/


const String ElementExpr::WILD_CARD = "*";


//- Constructors -/
ElementExpr::ElementExpr() {
    //-- do nothing
} //-- ElementExpr

ElementExpr::ElementExpr(String& name) {
    //-- copy name
    int idx = name.indexOf(':');
    if ( idx >= 0 )
       name.subString(0,idx, this->prefix);
    else
       idx = -1;
    name.subString(idx+1, this->name);

    //-- set flags
    this->isNamespaceWild = (this->prefix.length() == 0);

    //-- I know...this should be done is WildCardExpr or someplace
    //-- else but I need a fix for now.
    this->isNameWild = this->name.isEqual(WILD_CARD);

} //-- ElementExpr

/**
 * Destructor
**/
ElementExpr::~ElementExpr() {
} //-- ~ElementExpr

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

    NodeList* nl = context->getChildNodes();

    for (int i = 0; i < nl->getLength(); i++ ) {
        Node* node = nl->item(i);
        if (matches(node, context, cs)) nodeSet->add(node);
    }
    return nodeSet;
} //-- evaluate

/**
 * Returns the default priority of this Pattern based on the given Node,
 * context Node, and ContextState.
 * If this pattern does not match the given Node under the current context Node and
 * ContextState then Negative Infinity is returned.
**/
double ElementExpr::getDefaultPriority(Node* node, Node* context, ContextState* cs) {
    return 0.0;
} //-- getDefaultPriority

/**
 * Returns the name of this ElementExpr
 * @return the name of this ElementExpr
**/
const String& ElementExpr::getName() {
    return (const String&) this->name;
} //-- getName

void ElementExpr::setName(const String& name) {
    this->name.clear();
    this->name.append(name);
} //-- setName


  //-----------------------------/
 //- Methods from NodeExpr.cpp -/
//-----------------------------/

/**
 * Returns the type of this NodeExpr
 * @return the type of this NodeExpr
**/
short ElementExpr::getType() {
    return NodeExpr::ELEMENT_EXPR;
} //-- getType

/**
 * Determines whether this NodeExpr matches the given node within
 * the given context
**/
MBool ElementExpr::matches(Node* node, Node* context, ContextState* cs) {
    if ( node) {
        if ( node->getNodeType() == Node::ELEMENT_NODE ) {

            const String nodeName = node->getNodeName();
            int idx = nodeName.indexOf(':');

            if (!isNamespaceWild) {
               //-- compare namespaces
               String nsURI;
               cs->getNameSpaceURIFromPrefix(this->prefix, nsURI);

               String nsURI2;
               String prefix2;
               if (idx > 0) nodeName.subString(0, idx, prefix2);
               cs->getNameSpaceURIFromPrefix(prefix2, nsURI2);

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
        }
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
void ElementExpr::toString(String& dest) {
    dest.append(this->name);
} //-- toString

