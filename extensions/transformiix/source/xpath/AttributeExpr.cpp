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
 * $Id: AttributeExpr.cpp,v 1.1 2000/04/06 07:44:41 kvisco%ziplink.net Exp $
 */

#include "Expr.h"

/**
 * This class represents a ElementExpr as defined by the XSL
 * Working Draft
 * @author <a href="mailto:kvisco@ziplink.net">Keith Visco</a>
 * @version $Revision: 1.1 $ $Date: 2000/04/06 07:44:41 $
**/

//- Constructors -/

AttributeExpr::AttributeExpr() {
    this->isWild = MB_FALSE;
} //-- AttributeExpr

AttributeExpr::AttributeExpr(String& name) {
    //-- copy name
    this->name = name;
    this->isWild = MB_FALSE;
} //-- AttributeExpr

/**
 * Destructor
**/
AttributeExpr::~AttributeExpr() {
} //-- ~AttributeExpr

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
        for (int i = 0; i < atts->getLength(); i++ ) {
            Attr* attr = (Attr*)atts->item(i);
            if ( isWild ) nodeSet->add(attr);
            else {
                String attName = attr->getName();
                if ( name.isEqual(attName) ){
                    nodeSet->add(attr);
                }
            }
        }
    }
    return nodeSet;
} //-- evaluate

/**
 * Returns the default priority of this Pattern based on the given Node,
 * context Node, and ContextState.
 * If this pattern does not match the given Node under the current context Node and
 * ContextState then Negative Infinity is returned.
**/
double AttributeExpr::getDefaultPriority(Node* node, Node* context, ContextState* cs) {
    return 0.0;
} //-- getDefaultPriority

/**
 * Returns the name of this ElementExpr
 * @return the name of this ElementExpr
**/
const String& AttributeExpr::getName() {
    return (const String&) this->name;
} //-- getName

void AttributeExpr::setName(const String& name) {
    this->name.clear();
    this->name.append(name);
} //-- setName

void AttributeExpr::setWild(MBool isWild) {
    this->isWild = isWild;
} //-- setWild
  //-----------------------------/
 //- Methods from NodeExpr.cpp -/
//-----------------------------/

/**
 * Returns the type of this NodeExpr
 * @return the type of this NodeExpr
**/
short AttributeExpr::getType() {
    return NodeExpr::ATTRIBUTE_EXPR;
} //-- getType

/**
 * Determines whether this NodeExpr matches the given node within
 * the given context
**/
MBool AttributeExpr::matches(Node* node, Node* context, ContextState* cs) {
    if ( (node) && (node->getNodeType() == Node::ATTRIBUTE_NODE) ) {
        if ( isWild ) return MB_TRUE;
        const String nodeName = ((Attr*)node)->getName();
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
    dest.append('@');
    if (isWild) dest.append('*');
    else dest.append(this->name);
} //-- toString

