/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Larry Fitzpatick, OpenText, lef@opentext.com
 *   -- 19990806
 *     - changed constant short declarations in many of the classes
 *       with enumerations, commented with //--LF
 * Jonas Sicking, sicking@bigfoot.com
 *   -- removal of Patterns and some restructuring
 *      in the class set
 *
 */


#ifndef TRANSFRMX_EXPR_H
#define TRANSFRMX_EXPR_H

#include "baseutils.h"
#include "dom.h"
#include "List.h"
#include "nsString.h"
#include "nsIAtom.h"
#include "TxObject.h"
#include "nsAutoPtr.h"
#include "ExprResult.h"

/*
  XPath class definitions.
  Much of this code was ported from XSL:P.
*/

class NodeSet;
class txIParseContext;
class txIMatchContext;
class txIEvalContext;

/**
 * A Base Class for all XSL Expressions
**/
class Expr : public TxObject {

public:

    /**
     * Virtual destructor, important for subclasses
    **/
    virtual ~Expr()
    {
    }

    /**
     * Evaluates this Expr based on the given context node and processor state
     * @param context the context node for evaluation of this Expr
     * @param ps the ContextState containing the stack information needed
     * for evaluation
     * @return the result of the evaluation
    **/
    virtual nsresult evaluate(txIEvalContext* aContext,
                              txAExprResult** aResult) = 0;

    /**
     * Returns the String representation of this Expr.
     * @param dest the String to use when creating the String
     * representation. The String representation will be appended to
     * any data in the destination String, to allow cascading calls to
     * other #toString() methods for Expressions.
     * @return the String representation of this Expr.
    **/
    virtual void toString(nsAString& str) = 0;

}; //-- Expr

#define TX_DECL_EVALUATE \
    nsresult evaluate(txIEvalContext* aContext, txAExprResult** aResult)

#define TX_DECL_EXPR \
    TX_DECL_EVALUATE; \
    void toString(nsAString& aDest)

#define TX_DECL_FUNCTION \
    TX_DECL_EVALUATE; \
    nsresult getNameAtom(nsIAtom** aAtom)

/**
 * This class represents a FunctionCall as defined by the XPath 1.0
 * Recommendation.
**/
class FunctionCall : public Expr {

public:
    virtual ~FunctionCall();

    /**
     * Virtual methods from Expr 
    **/
    void toString(nsAString& aDest);

    /**
     * Adds the given parameter to this FunctionCall's parameter list.
     * The ownership of the given Expr is passed over to the FunctionCall,
     * even on failure.
     * @param aExpr the Expr to add to this FunctionCall's parameter list
     * @return nsresult indicating out of memory
     */
    nsresult addParam(Expr* aExpr);

    /**
     * Check if the number of parameters falls within a range.
     *
     * @param aParamCountMin minimum number of required parameters.
     * @param aParamCountMax maximum number of parameters. If aParamCountMax
     *                       is negative the maximum number is not checked.
     * @return boolean representing whether the number of parameters falls
     *         within the expected range or not.
     *
     * XXX txIEvalContext should be txIParseContest, bug 143291
     */
    virtual PRBool requireParams(PRInt32 aParamCountMin,
                                 PRInt32 aParamCountMax,
                                 txIEvalContext* aContext);

protected:

    txList params;

    FunctionCall();

    /*
     * Evaluates the given Expression and converts its result to a String.
     * The value is appended to the given destination String
     */
    void evaluateToString(Expr* aExpr, txIEvalContext* aContext,
                          nsAString& aDest);

    /*
     * Evaluates the given Expression and converts its result to a number.
     */
    double evaluateToNumber(Expr* aExpr, txIEvalContext* aContext);

    /*
     * Evaluates the given Expression and converts its result to a boolean.
     */
    MBool evaluateToBoolean(Expr* aExpr, txIEvalContext* aContext);

    /*
     * Evaluates the given Expression and converts its result to a NodeSet.
     * If the result is not a NodeSet an error is returned.
     */
    nsresult evaluateToNodeSet(Expr* aExpr, txIEvalContext* aContext,
                               NodeSet** aResult);

    /*
     * Returns the name of the function as an atom.
     */
    virtual nsresult getNameAtom(nsIAtom** aAtom) = 0;
}; //-- FunctionCall


/**
 * Represents an AttributeValueTemplate
**/
class AttributeValueTemplate: public Expr {

public:

    AttributeValueTemplate();

    virtual ~AttributeValueTemplate();

    /**
     * Adds the given Expr to this AttributeValueTemplate
    **/
    void addExpr(Expr* expr);

    TX_DECL_EXPR;

private:
    List expressions;
};


/*
 * This class represents a NodeTest as defined by the XPath spec
 */
class txNodeTest
{
public:
    virtual ~txNodeTest() {}
    /*
     * Virtual methods
     * pretty much a txPattern, but not supposed to be used 
     * standalone. The NodeTest node() is different to the
     * Pattern "node()" (document node isn't matched)
     */
    virtual MBool matches(Node* aNode, txIMatchContext* aContext) = 0;
    virtual double getDefaultPriority() = 0;
    virtual void toString(nsAString& aDest) = 0;
};

#define TX_DECL_NODE_TEST \
    MBool matches(Node* aNode, txIMatchContext* aContext); \
    double getDefaultPriority(); \
    void toString(nsAString& aDest)

/*
 * This class represents a NameTest as defined by the XPath spec
 */
class txNameTest : public txNodeTest
{
public:
    /*
     * Creates a new txNameTest with the given type and the given
     * principal node type
     */
    txNameTest(nsIAtom* aPrefix, nsIAtom* aLocalName, PRInt32 aNSID,
               Node::NodeType aNodeType);

    ~txNameTest();

    TX_DECL_NODE_TEST;

private:
    nsCOMPtr<nsIAtom> mPrefix;
    nsCOMPtr<nsIAtom> mLocalName;
    PRInt32 mNamespace;
    Node::NodeType mNodeType;
};

/*
 * This class represents a NodeType as defined by the XPath spec
 */
class txNodeTypeTest : public txNodeTest
{
public:
    enum NodeType {
        COMMENT_TYPE,
        TEXT_TYPE,
        PI_TYPE,
        NODE_TYPE
    };

    /*
     * Creates a new txNodeTypeTest of the given type
     */
    txNodeTypeTest(NodeType aNodeType);

    ~txNodeTypeTest();

    /*
     * Sets the name of the node to match. Only availible for pi nodes
     */
    void setNodeName(const nsAString& aName);

    TX_DECL_NODE_TEST;

private:
    NodeType mNodeType;
    nsCOMPtr<nsIAtom> mNodeName;
};

/**
 * Represents an ordered list of Predicates,
 * for use with Step and Filter Expressions
**/
class PredicateList  {

public:

    /**
     * Creates a new PredicateList
    **/
    PredicateList();
    /**
     * Destructor, will delete all Expressions in the list, so remove
     * any you may need
    **/
    virtual ~PredicateList();

    /**
     * Adds the given Expr to the list.
     * The ownership of the given Expr is passed over the PredicateList,
     * even on failure.
     * @param aExpr the Expr to add to the list
     * @return nsresult indicating out of memory
     */
    nsresult add(Expr* aExpr);

    nsresult evaluatePredicates(NodeSet* aNodes, txIMatchContext* aContext);

    /**
     * returns true if this predicate list is empty
    **/
    MBool isEmpty();

    /**
     * Returns the String representation of this PredicateList.
     * @param dest the String to use when creating the String
     * representation. The String representation will be appended to
     * any data in the destination String, to allow cascading calls to
     * other #toString() methods for Expressions.
     * @return the String representation of this PredicateList.
    **/
    virtual void toString(nsAString& dest);

protected:
    //-- list of predicates
    List predicates;
}; //-- PredicateList

class LocationStep : public PredicateList, public Expr {

public:

    // Axis Identifier Types
    //-- LF changed from static const short to enum
    enum LocationStepType {
        ANCESTOR_AXIS = 0,
        ANCESTOR_OR_SELF_AXIS,
        ATTRIBUTE_AXIS,
        CHILD_AXIS,
        DESCENDANT_AXIS,
        DESCENDANT_OR_SELF_AXIS,
        FOLLOWING_AXIS,
        FOLLOWING_SIBLING_AXIS,
        NAMESPACE_AXIS,
        PARENT_AXIS,
        PRECEDING_AXIS,
        PRECEDING_SIBLING_AXIS,
        SELF_AXIS
    };

    /**
     * Creates a new LocationStep using the given NodeExpr and Axis Identifier
     * @param nodeExpr the NodeExpr to use when matching Nodes
     * @param axisIdentifier the Axis Identifier in which to search for nodes
    **/
    LocationStep(nsAutoPtr<txNodeTest>& aNodeTest,
                 LocationStepType aAxisIdentifier)
        : mNodeTest(aNodeTest),
          mAxisIdentifier(aAxisIdentifier)
    {
    }

    TX_DECL_EXPR;

private:

    nsAutoPtr<txNodeTest> mNodeTest;
    LocationStepType mAxisIdentifier;

    void fromDescendants(Node* node, txIMatchContext* aContext,
                         NodeSet* nodes);
    void fromDescendantsRev(Node* node, txIMatchContext* aContext,
                            NodeSet* nodes);

}; //-- LocationStep


class FilterExpr : public PredicateList, public Expr {

public:

    /**
     * Creates a new FilterExpr using the given Expr
     * @param expr the Expr to use for evaluation
     */
    FilterExpr(nsAutoPtr<Expr>& aExpr)
        : expr(aExpr)
    {
    }

    TX_DECL_EXPR;

private:
    nsAutoPtr<Expr> expr;

}; //-- FilterExpr


class txLiteralExpr : public Expr {
public:
    txLiteralExpr(double aDbl);
    txLiteralExpr(const nsAString& aStr);

    TX_DECL_EXPR;

private:
    nsRefPtr<txAExprResult> mValue;
};

/**
 * Represents an AdditiveExpr, a binary expression that
 * performs an additive operation between it's lvalue and rvalue:
 *  +   : add
 *  -   : subtract
**/
class AdditiveExpr : public Expr {

public:

    //-- AdditiveExpr Types
    //-- LF, changed from static const short to enum
    enum _AdditiveExprType { ADDITION = 1, SUBTRACTION };

     AdditiveExpr(nsAutoPtr<Expr>& aLeftExpr, nsAutoPtr<Expr>& aRightExpr,
                  short aOp)
         : op(aOp),
           leftExpr(aLeftExpr),
           rightExpr(aRightExpr)
    {
    }

    TX_DECL_EXPR;

private:
    short op;
    nsAutoPtr<Expr> leftExpr, rightExpr;
}; //-- AdditiveExpr

/**
 * Represents an UnaryExpr. Returns the negative value of it's expr.
**/
class UnaryExpr : public Expr {

public:

    UnaryExpr(nsAutoPtr<Expr>& aExpr)
        : expr(aExpr)
    {
    }

    TX_DECL_EXPR;

private:
    nsAutoPtr<Expr> expr;
}; //-- UnaryExpr

/**
 * Represents a BooleanExpr, a binary expression that
 * performs a boolean operation between it's lvalue and rvalue.
**/
class BooleanExpr : public Expr {

public:

    //-- BooleanExpr Types
    enum _BooleanExprType { AND = 1, OR };

     BooleanExpr(nsAutoPtr<Expr>& aLeftExpr, nsAutoPtr<Expr>& aRightExpr,
                 short aOp)
         : op(aOp),
           leftExpr(aLeftExpr),
           rightExpr(aRightExpr)
    {
    }

    TX_DECL_EXPR;

private:
    short op;
    nsAutoPtr<Expr> leftExpr, rightExpr;
}; //-- BooleanExpr

/**
 * Represents a MultiplicativeExpr, a binary expression that
 * performs a multiplicative operation between it's lvalue and rvalue:
 *  *   : multiply
 * mod  : modulus
 * div  : divide
 *
**/
class MultiplicativeExpr : public Expr {

public:

    //-- MultiplicativeExpr Types
    //-- LF, changed from static const short to enum
    enum _MultiplicativeExprType { DIVIDE = 1, MULTIPLY, MODULUS };

     MultiplicativeExpr(nsAutoPtr<Expr>& aLeftExpr,
                        nsAutoPtr<Expr>& aRightExpr,
                        short aOp)
         : op(aOp),
           leftExpr(aLeftExpr),
           rightExpr(aRightExpr)
    {
    }

    TX_DECL_EXPR;

private:
    short op;
    nsAutoPtr<Expr> leftExpr, rightExpr;
}; //-- MultiplicativeExpr

/**
 * Represents a RelationalExpr, an expression that compares it's lvalue
 * to it's rvalue using:
 * =  : equal to
 * <  : less than
 * >  : greater than
 * <= : less than or equal to
 * >= : greater than or equal to
 *
**/
class RelationalExpr : public Expr {

public:
    enum RelationalExprType {
        EQUAL,
        NOT_EQUAL,
        LESS_THAN,
        GREATER_THAN,
        LESS_OR_EQUAL,
        GREATER_OR_EQUAL
    };

    RelationalExpr(nsAutoPtr<Expr>& aLeftExpr, nsAutoPtr<Expr>& aRightExpr,
                   RelationalExprType aOp)
        : mLeftExpr(aLeftExpr),
          mRightExpr(aRightExpr),
          mOp(aOp)
    {
    }


    TX_DECL_EXPR;

private:
    PRBool compareResults(txIEvalContext* aContext, txAExprResult* aLeft,
                          txAExprResult* aRight);

    nsAutoPtr<Expr> mLeftExpr;
    nsAutoPtr<Expr> mRightExpr;
    RelationalExprType mOp;
};

/**
 * VariableRefExpr
 * Represents a variable reference ($refname)
**/
class VariableRefExpr : public Expr {

public:

    VariableRefExpr(nsIAtom* aPrefix, nsIAtom* aLocalName, PRInt32 aNSID);
    ~VariableRefExpr();

    TX_DECL_EXPR;

private:
    nsCOMPtr<nsIAtom> mPrefix;
    nsCOMPtr<nsIAtom> mLocalName;
    PRInt32 mNamespace;
};

/**
 *  Represents a PathExpr
**/
class PathExpr : public Expr {

public:

    //-- Path Operators
    //-- RELATIVE_OP is the default
    //-- LF, changed from static const short to enum
    enum PathOperator { RELATIVE_OP, DESCENDANT_OP };

    /**
     * Creates a new PathExpr
    **/
    PathExpr();

    /**
     * Destructor, will delete all Expressions
    **/
    virtual ~PathExpr();

    /**
     * Adds the Expr to this PathExpr
     * The ownership of the given Expr is passed over the PathExpr,
     * even on failure.
     * @param aExpr the Expr to add to this PathExpr
     * @return nsresult indicating out of memory
     */
    nsresult addExpr(Expr* aExpr, PathOperator pathOp);

    TX_DECL_EXPR;

private:
    class PathExprItem {
    public:
        PathExprItem(Expr* aExpr, PathOperator aOp)
            : expr(aExpr),
              pathOp(aOp)
        {}
        nsAutoPtr<Expr> expr;
        PathOperator pathOp;
    };

    List expressions;

    /*
     * Selects from the descendants of the context node
     * all nodes that match the Expr
     */
    nsresult evalDescendants(Expr* aStep, Node* aNode,
                             txIMatchContext* aContext,
                             NodeSet* resNodes);

}; //-- PathExpr

/**
 * This class represents a RootExpr, which only matches the Document node
**/
class RootExpr : public Expr {

public:

    /**
     * Creates a new RootExpr
     * @param aSerialize should this RootExpr be serialized
     */
    RootExpr(MBool aSerialize);

    TX_DECL_EXPR;

private:
    // When a RootExpr is used in a PathExpr it shouldn't be serialized
    MBool mSerialize;

}; //-- RootExpr

/**
 *  Represents a UnionExpr
**/
class UnionExpr : public Expr {

public:

    /**
     * Creates a new UnionExpr
    **/
    UnionExpr();

    /**
     * Destructor, will delete all Path Expressions
    **/
    virtual ~UnionExpr();

    /**
     * Adds the PathExpr to this UnionExpr
     * The ownership of the given Expr is passed over the UnionExpr,
     * even on failure.
     * @param aExpr the Expr to add to this UnionExpr
     * @return nsresult indicating out of memory
     */
    nsresult addExpr(Expr* aExpr);

    TX_DECL_EXPR;

private:

   List expressions;

}; //-- UnionExpr

#endif


