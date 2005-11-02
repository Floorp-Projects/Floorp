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
 * Larry Fitzpatick, OpenText, lef@opentext.com
 *   -- 19990806
 *     - changed constant short declarations in many of the classes
 *       with enumerations, commented with //--LF
 * Jonas Sicking, sicking@bigfoot.com
 *   -- removal of Patterns and some restructuring
 *      in the class set
 *
 * $Id: txExpr.h,v 1.14 2005/11/02 07:33:39 sicking%bigfoot.com Exp $
 */


#ifndef TRANSFRMX_EXPR_H
#define TRANSFRMX_EXPR_H


#include "TxString.h"
#include "ErrorObserver.h"
#include "NodeSet.h"
#include "Stack.h"
#include "ExprResult.h"
#include "baseutils.h"
#include "TxObject.h"
#include "primitives.h"
#include "NamespaceResolver.h"

/*
  XPath class definitions.
  Much of this code was ported from XSL:P.
  @version $Revision: 1.14 $ $Date: 2005/11/02 07:33:39 $
*/

//necessary prototypes
class FunctionCall;

typedef class Expr Pattern;
typedef class Expr PatternExpr;


/**
 * The expression context and state class used when evaluating XPath Expressions.
**/
class ContextState : public NamespaceResolver, public ErrorObserver {

public:

     /**
      * Returns the value of a given variable binding within the current scope
      * @param the name to which the desired variable value has been bound
      * @return the ExprResult which has been bound to the variable with
      *  the given name
     **/
    virtual ExprResult* getVariable(String& name) = 0;

    /**
     * Returns the Stack of context NodeSets
     * @return the Stack of context NodeSets
    **/
    virtual Stack* getNodeSetStack() = 0;

    /**
     * handles finding the parent of a node, since in DOM some
     * nodes such as Attribute Nodes do not have parents
     * @param node the Node to search for the parent of
     * @return the parent of the given node, or null
    **/
    virtual Node* getParentNode(Node* node) = 0;

    virtual MBool isStripSpaceAllowed(Node* node) = 0;

    /**
     * Returns a call to the function that has the given name.
     * This method is used for XPath Extension Functions.
     * @return the FunctionCall for the function with the given name.
    **/
    virtual FunctionCall* resolveFunctionCall(const String& name) = 0;

    /**
     * Sorts the given NodeSet by DocumentOrder. 
     * @param nodes the NodeSet to sort
     * <BR />
     * <B>Note:</B> I will be moving this functionality elsewhere soon
    **/
    virtual void sortByDocumentOrder(NodeSet* nodes) = 0;

}; //-- ContextState


/**
 * A Base Class for all XSL Expressions
**/
class Expr : public TxObject {

public:

    /**
     * Virtual destructor, important for subclasses
    **/
    virtual ~Expr();

    /**
     * Evaluates this Expr based on the given context node and processor state
     * @param context the context node for evaluation of this Expr
     * @param ps the ContextState containing the stack information needed
     * for evaluation
     * @return the result of the evaluation
    **/
    virtual ExprResult* evaluate(Node* context, ContextState* cs) = 0;

    /**
     * Determines whether this Expr matches the given node within
     * the given context
    **/
    virtual MBool matches(Node* node, Node* context, ContextState* cs);

    /**
     * Returns the default priority of this Expr based on the given Node,
     * context Node, and ContextState.
    **/
    virtual double getDefaultPriority(Node* node, Node* context, ContextState* cs);

    /**
     * Returns the String representation of this Expr.
     * @param dest the String to use when creating the String
     * representation. The String representation will be appended to
     * any data in the destination String, to allow cascading calls to
     * other #toString() methods for Expressions.
     * @return the String representation of this Expr.
    **/
    virtual void toString(String& str) = 0;

}; //-- Expr

/**
 * This class represents a FunctionCall as defined by the XPath 1.0
 * Recommendation.
**/
class FunctionCall : public Expr {

public:

    static const String INVALID_PARAM_COUNT;

    virtual ~FunctionCall();

    /**
     * Virtual methods from Expr 
    **/
    virtual ExprResult* evaluate(Node* context, ContextState* cs) = 0;
    virtual MBool matches(Node* node, Node* context, ContextState* cs);
    virtual double getDefaultPriority(Node* node, Node* context, ContextState* cs);
    virtual void toString(String& dest);

    /**
     * Adds the given parameter to this FunctionCall's parameter list
     * @param expr the Expr to add to this FunctionCall's parameter list
    **/
    void addParam(Expr* expr);

    /**
     * Returns the name of this FunctionCall
     * @return the name of this FunctionCall
    **/
    const String& getName();

    virtual MBool requireParams(int paramCountMin, ContextState* cs);

    virtual MBool requireParams(int paramCountMin,
                                int paramCountMax,
                                ContextState* cs);

    /**
     * Sets the function name of this FunctionCall
     * @param name the name of this Function
    **/
    void setName(const String& name);


protected:

    List params;

    FunctionCall();
    FunctionCall(const String& name);
    FunctionCall(const String& name, List* parameters);

    /**
     * Evaluates the given Expression and converts it's result to a String.
     * The value is appended to the given destination String
    **/
    void evaluateToString
        (Expr* expr, Node* context, ContextState* cs, String& dest);

    /**
     * Evaluates the given Expression and converts it's result to a number.
    **/
    double evaluateToNumber(Expr* expr, Node* context, ContextState* cs);

private:

    String name;
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

    /**
     * Virtual methods from Expr 
    **/
    virtual ExprResult* evaluate(Node* context, ContextState* cs);
    virtual void toString(String& str);

private:
    List expressions;
};


/**
 * This class represents a NodeTestExpr as defined by the XSL
 * Working Draft
**/
class NodeExpr : public Expr {

public:

    //-- NodeExpr Types
    //-- LF - changed from const short to enum
    enum NodeExprType {
        ATTRIBUTE_EXPR =  1,
        ELEMENT_EXPR,
        TEXT_EXPR,
        COMMENT_EXPR,
        PI_EXPR,
        NODE_EXPR,
        WILD_CARD,
        ROOT_EXPR
    };

    virtual ~NodeExpr() {};

      //------------------/
     //- Public Methods -/
    //------------------/

    /**
     * Returns the type of this NodeExpr
     * @return the type of this NodeExpr
    **/
    virtual short getType() = 0;

    /**
     * Virtual methods from Expr 
    **/
    virtual MBool matches(Node* node, Node* context, ContextState* cs) = 0;
    virtual double getDefaultPriority(Node* node, Node* context, ContextState* cs) = 0;
    virtual void toString(String& dest) = 0;

}; //-- NodeExpr

/**
 * This class represents a AttributeExpr as defined by the XSL
 * Working Draft
**/
class AttributeExpr : public NodeExpr {

public:

      //------------------/
     //- Public Methods -/
    //------------------/

    AttributeExpr();
    AttributeExpr(String& name);
    virtual ~AttributeExpr();

    void setWild(MBool isWild);

    /**
     * Returns the name of this AttributeExpr
     * @return the name of this AttributeExpr
    **/
    const String& getName();

    /**
     * Sets the name of this AttributeExpr
     * @param name the name of the element that this AttributeExpr matches
    **/
    void setName(const String& name);

    /**
     * Virtual methods from NodeExpr 
    **/
    virtual ExprResult* evaluate(Node* context, ContextState* cs);
    short getType();
    virtual MBool matches(Node* node, Node* context, ContextState* cs);
    virtual double getDefaultPriority(Node* node, Node* context, ContextState* cs);
    virtual void toString(String& dest);

private:

    static const String WILD_CARD;

    String prefix;
    String name;
    MBool  isNameWild;
    MBool  isNamespaceWild;

}; //-- AttributeExpr

/**
 *
**/
class BasicNodeExpr : public NodeExpr {

public:

      //------------------/
     //- Public Methods -/
    //------------------/

    /**
     * Creates a new BasicNodeExpr of type NodeExpr::NODE_EXPR, which matches
     * any node
    **/
    BasicNodeExpr();

    /**
     * Creates a new BasicNodeExpr of the given type
    **/
    BasicNodeExpr(NodeExprType nodeExprType);

    /**
     * Destroys this BasicNodeExpr
    **/
    virtual ~BasicNodeExpr();
    
    /**
     * Sets the name of the node to match. Only availible for pi nodes
    **/
    void setNodeName(const String& name);

    /**
     * Virtual methods from NodeExpr 
    **/
    virtual ExprResult* evaluate(Node* context, ContextState* cs);
    short getType();
    virtual MBool matches(Node* node, Node* context, ContextState* cs);
    virtual double getDefaultPriority(Node* node, Node* context, ContextState* cs);
    virtual void toString(String& dest);

private:
    NodeExprType type;
    String nodeName;
    MBool nodeNameSet;
}; //-- BasicNodeExpr

/**
 * This class represents a ElementExpr as defined by the XSL
 * Working Draft
**/
class ElementExpr : public NodeExpr {

public:

      //------------------/
     //- Public Methods -/
    //------------------/

    ElementExpr();
    ElementExpr(String& name);
    virtual ~ElementExpr();

    /**
     * Returns the name of this ElementExpr
     * @return the name of this ElementExpr
    **/
    const String& getName();

    /**
     * Sets the name of this ElementExpr
     * @param name the name of the element that this ElementExpr matches
    **/
    void setName(const String& name);

    /**
     * Virtual methods from NodeExpr 
    **/
    virtual ExprResult* evaluate(Node* context, ContextState* cs);
    short getType();
    virtual double getDefaultPriority(Node* node, Node* context, ContextState* cs);
    virtual MBool matches(Node* node, Node* context, ContextState* cs);
    virtual void toString(String& dest);

private:

    static const String WILD_CARD;

    String name;

    MBool isNamespaceWild;

    MBool isNameWild;

    String prefix;

}; //-- ElementExpr

/**
 * This class represents a TextExpr, which only matches any text node
**/
class TextExpr : public NodeExpr {

public:

      //------------------/
     //- Public Methods -/
    //------------------/

    /**
     * Virtual methods from NodeExpr 
    **/
    virtual ExprResult* evaluate(Node* context, ContextState* cs);
    virtual short getType();
    virtual MBool matches(Node* node, Node* context, ContextState* cs);
    virtual double getDefaultPriority(Node* node, Node* context, ContextState* cs);
    virtual void toString(String& dest);

}; //-- TextExpr

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
     * Adds the given Expr to the list
     * @param expr the Expr to add to the list
    **/
    void add(Expr* expr);


    void evaluatePredicates(NodeSet* nodes, ContextState* cs);

    /**
     * returns true if this predicate list is empty
    **/
    MBool isEmpty();

    /**
     * Removes the given Expr from the list
     * @param expr the Expr to remove from the list
    **/
    Expr* remove(Expr* expr);

    /**
     * Returns the String representation of this PredicateList.
     * @param dest the String to use when creating the String
     * representation. The String representation will be appended to
     * any data in the destination String, to allow cascading calls to
     * other #toString() methods for Expressions.
     * @return the String representation of this PredicateList.
    **/
    virtual void toString(String& dest);

private:
    //-- list of predicates
    List predicates;
}; //-- PredicateList

class LocationStep : public PredicateList, public Expr {

public:

    // Axis Identifier Types
    //-- LF changed from static const short to enum
    enum _LocationStepType {
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
     * Creates a new LocationStep using the default Axis Identifier and no
     * NodeExpr (which matches nothing)
    **/
    LocationStep();

    /**
     * Creates a new LocationStep using the default Axis Identifier and
     * the given NodeExpr
     * @param nodeExpr the NodeExpr to use when matching Nodes
    **/
    LocationStep(NodeExpr* nodeExpr);

    /**
     * Creates a new LocationStep using the given NodeExpr and Axis Identifier
     * @param nodeExpr the NodeExpr to use when matching Nodes
     * @param axisIdentifier the Axis Identifier in which to search for nodes
    **/
    LocationStep(NodeExpr* nodeExpr, short axisIdentifier);

    /**
     * Destructor, will delete all predicates and the given NodeExpr
    **/
    virtual ~LocationStep();

    /**
     * Sets the Axis Identifier for this LocationStep
     * @param axisIdentifier the Axis in which to search for nodes
    **/
    void setAxisIdentifier(short axisIdentifier);

    /**
     * Sets the NodeExpr of this LocationStep for use when matching nodes
     * @param nodeExpr the NodeExpr to use when matching nodes
    **/
    void setNodeExpr(NodeExpr* nodeExpr);

    /**
     * Virtual methods from Expr 
    **/
    virtual ExprResult* evaluate(Node* context, ContextState* cs);
    virtual MBool matches(Node* node, Node* context, ContextState* cs);
    virtual double getDefaultPriority(Node* node, Node* context, ContextState* cs);
    virtual void toString(String& dest);

private:

    NodeExpr* nodeExpr;
    short     axisIdentifier;

    void fromDescendants(Node* context, ContextState* cs, NodeSet* nodes);
    void fromDescendantsRev(Node* context, ContextState* cs, NodeSet* nodes);

}; //-- LocationStep


class FilterExpr : public PredicateList, public Expr {

public:

    /**
     * Creates a new FilterExpr using the default no default Expr
     * (which evaluates to nothing)
    **/
    FilterExpr();

    /**
     * Creates a new FilterExpr using the given Expr
     * @param expr the Expr to use for evaluation
    **/
    FilterExpr(Expr* expr);

    /**
     * Destructor, will delete all predicates and the given Expr
    **/
    virtual ~FilterExpr();

    /**
     * Sets the Expr of this FilterExpr for evaluation
     * @param expr the Expr to use for evaluation
    **/
    void setExpr(Expr* expr);

    /**
     * Virtual methods from Expr 
    **/
    virtual ExprResult* evaluate(Node* context, ContextState* cs);
    virtual MBool matches(Node* node, Node* context, ContextState* cs);
    virtual double getDefaultPriority(Node* node, Node* context, ContextState* cs);
    virtual void toString(String& dest);

private:

    Expr* expr;

}; //-- FilterExpr


class NumberExpr : public Expr {

public:

    NumberExpr();
    NumberExpr(double dbl);
    ~NumberExpr();

    /**
     * Virtual methods from Expr 
    **/
    virtual ExprResult* evaluate(Node* context, ContextState* cs);
    virtual void toString(String& str);

private:

    double _value;
};

/**
 * Represents a String expression
**/
class StringExpr : public Expr {

public:

    StringExpr();
    StringExpr(String& value);
    StringExpr(const String& value);
    StringExpr(const char* value);
    ~StringExpr();

    /**
     * Virtual methods from Expr 
    **/
    virtual ExprResult* evaluate(Node* context, ContextState* cs);
    virtual void toString(String& str);

private:

    String value;
}; //-- StringExpr


/**
 * Represents an AdditiveExpr, a binary expression that
 * performs an additive operation between it's lvalue and rvalue:<BR/>
 *  +   : add
 *  -   : subtract
**/
class AdditiveExpr : public Expr {

public:

    //-- AdditiveExpr Types
    //-- LF, changed from static const short to enum
    enum _AdditiveExprType { ADDITION = 1, SUBTRACTION };

     AdditiveExpr();
     AdditiveExpr(Expr* leftExpr, Expr* rightExpr, short op);
     ~AdditiveExpr();

    /**
     * Sets the left side of this AdditiveExpr
    **/
    void setLeftExpr(Expr* leftExpr);

    /**
     * Sets the right side of this AdditiveExpr
    **/
    void setRightExpr(Expr* rightExpr);


    /**
     * Virtual methods from Expr 
    **/
    virtual ExprResult* evaluate(Node* context, ContextState* cs);
    virtual void toString(String& str);

private:
    short op;
    Expr* leftExpr;
    Expr* rightExpr;
}; //-- AdditiveExpr

/**
 * Represents an UnaryExpr. Returns the negative value of it's expr.
**/
class UnaryExpr : public Expr {

public:

     UnaryExpr();
     UnaryExpr(Expr* expr);
     ~UnaryExpr();

    /**
     * Sets the expression to negate
    **/
    void setExpr(Expr* expr);

    /**
     * Virtual methods from Expr 
    **/
    virtual ExprResult* evaluate(Node* context, ContextState* cs);
    virtual void toString(String& str);

private:
    Expr* expr;
}; //-- UnaryExpr

/**
 * Represents a BooleanExpr, a binary expression that
 * performs a boolean operation between it's lvalue and rvalue:<BR/>
**/
class BooleanExpr : public Expr {

public:

    //-- BooleanExpr Types
    enum _BooleanExprType { AND = 1, OR };

     BooleanExpr();
     BooleanExpr(Expr* leftExpr, Expr* rightExpr, short op);
     ~BooleanExpr();

    /**
     * Sets the left side of this BooleanExpr
    **/
    void setLeftExpr(Expr* leftExpr);

    /**
     * Sets the right side of this BooleanExpr
    **/
    void setRightExpr(Expr* rightExpr);


    /**
     * Virtual methods from Expr 
    **/
    virtual ExprResult* evaluate(Node* context, ContextState* cs);
    virtual void toString(String& str);

private:
    short op;
    Expr* leftExpr;
    Expr* rightExpr;
}; //-- BooleanExpr

/**
 * Represents a MultiplicativeExpr, a binary expression that
 * performs a multiplicative operation between it's lvalue and rvalue:<BR/>
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

     MultiplicativeExpr();
     MultiplicativeExpr(Expr* leftExpr, Expr* rightExpr, short op);
     ~MultiplicativeExpr();

    /**
     * Sets the left side of this MultiplicativeExpr
    **/
    void setLeftExpr(Expr* leftExpr);

    /**
     * Sets the right side of this MultiplicativeExpr
    **/
    void setRightExpr(Expr* rightExpr);

    /**
     * Virtual methods from Expr 
    **/
    virtual ExprResult* evaluate(Node* context, ContextState* cs);
    virtual void toString(String& str);

private:
    short op;
    Expr* leftExpr;
    Expr* rightExpr;
}; //-- MultiplicativeExpr

/**
 * Represents a RelationalExpr, an expression that compares it's lvalue
 * to it's rvalue using:<BR/>
 * =  : equal to
 * <  : less than
 * >  : greater than
 * <= : less than or equal to
 * >= : greater than or equal to
 *
**/
class RelationalExpr : public Expr {

public:

    //-- RelationalExpr Types
    //-- LF, changed from static const short to enum
    enum _RelationalExprType {
        EQUAL = 1,
        NOT_EQUAL,
        LESS_THAN,
        GREATER_THAN,
        LESS_OR_EQUAL,
        GREATER_OR_EQUAL
    };

     RelationalExpr();
     RelationalExpr(Expr* leftExpr, Expr* rightExpr, short op);
     ~RelationalExpr();

    /**
     * Virtual methods from Expr 
    **/
    virtual ExprResult* evaluate(Node* context, ContextState* cs);
    virtual void toString(String& str);

private:
    short op;
    Expr* leftExpr;
    Expr* rightExpr;

    MBool compareResults(ExprResult* left, ExprResult* right);
}; //-- RelationalExpr

/**
 * VariableRefExpr<BR>
 * Represents a variable reference ($refname)
**/
class VariableRefExpr : public Expr {

public:

    VariableRefExpr();
    VariableRefExpr(const String& name);
    VariableRefExpr(String& name);
    ~VariableRefExpr();

    /**
     * Sets the name of the variable of reference
    **/
    void setName(const String& name);

    /**
     * Virtual methods from Expr 
    **/
    virtual ExprResult* evaluate(Node* context, ContextState* cs);
    virtual void toString(String& str);

private:
    String name;

}; //-- VariableRefExpr

/**
 *  Represents a PathExpr
**/
class PathExpr : public Expr {

public:

    //-- Path Operators
    //-- RELATIVE_OP is the default
    //-- LF, changed from static const short to enum
    enum _PathOperator { ANCESTOR_OP=1, PARENT_OP, RELATIVE_OP} ;

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
     * @param expr the Expr to add to this PathExpr
     * @param index the index at which to add the given Expr
    **/
    void addExpr(int index, Expr* expr, short ancestryOp);

    /**
     * Adds the Expr to this PathExpr
     * @param expr the Expr to add to this PathExpr
    **/
    void addExpr(Expr* expr, short ancestryOp);

    virtual MBool isAbsolute();

    /**
     * Virtual methods from Expr 
    **/
    virtual ExprResult* evaluate(Node* context, ContextState* cs);
    virtual MBool matches(Node* node, Node* context, ContextState* cs);
    virtual double getDefaultPriority(Node* node, Node* context, ContextState* cs);
    virtual void toString(String& dest);

private:

    struct PathExprItem {
        Expr* expr;
        short ancestryOp;
    };

   List expressions;

   /**
    * Selects from the descendants of the context node
    * all nodes that match the Expr
    * -- this will be moving to a Utility class
   **/
   void evalDescendants(Expr* expr,
                        Node* context,
                        ContextState* cs,
                        NodeSet* resNodes);

}; //-- PathExpr

/**
 * This class represents a RootExpr, which only matches the Document node
**/
class RootExpr : public PathExpr {

public:

      //------------------/
     //- Public Methods -/
    //------------------/

    /**
     * Virtual methods from Expr 
    **/
    virtual ExprResult* evaluate(Node* context, ContextState* cs);
    virtual MBool matches(Node* node, Node* context, ContextState* cs);
    virtual double getDefaultPriority(Node* node, Node* context, ContextState* cs);
    virtual void toString(String& dest);

    virtual MBool isAbsolute();


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
     * @param expr the Expr to add to this UnionExpr
    **/
    void addExpr(Expr* expr);

    /**
     * Adds the PathExpr to this UnionExpr at the specified index
     * @param expr the Expr to add to this UnionExpr
    **/
    void addExpr(int index, Expr* expr);

    /**
     * Virtual methods from Expr 
    **/
    virtual ExprResult* evaluate(Node* context, ContextState* cs);
    virtual MBool matches(Node* node, Node* context, ContextState* cs);
    virtual double getDefaultPriority(Node* node, Node* context, ContextState* cs);
    virtual void toString(String& dest);

private:

   List expressions;

}; //-- UnionExpr

/* */
#endif


