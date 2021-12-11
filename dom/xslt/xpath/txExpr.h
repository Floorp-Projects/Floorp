/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TRANSFRMX_EXPR_H
#define TRANSFRMX_EXPR_H

#include "mozilla/Attributes.h"
#include "mozilla/UniquePtr.h"
#include "txExprResult.h"
#include "txCore.h"
#include "nsString.h"
#include "txOwningArray.h"
#include "nsAtom.h"

#ifdef DEBUG
#  define TX_TO_STRING
#endif

/*
  XPath class definitions.
  Much of this code was ported from XSL:P.
*/

class nsAtom;
class txIMatchContext;
class txIEvalContext;
class txNodeSet;
class txXPathNode;
class txXPathTreeWalker;

/**
 * A Base Class for all XSL Expressions
 **/
class Expr {
 public:
  MOZ_COUNTED_DEFAULT_CTOR(Expr)
  MOZ_COUNTED_DTOR_VIRTUAL(Expr)

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
   * Returns the type of this expression.
   */
  enum ExprType {
    LOCATIONSTEP_EXPR,
    PATH_EXPR,
    UNION_EXPR,
    LITERAL_EXPR,
    OTHER_EXPR
  };
  virtual ExprType getType() { return OTHER_EXPR; }

  /**
   * Returns the type or types of results this Expr return.
   */
  using ResultType = uint16_t;
  enum {
    NODESET_RESULT = 0x01,
    BOOLEAN_RESULT = 0x02,
    NUMBER_RESULT = 0x04,
    STRING_RESULT = 0x08,
    RTF_RESULT = 0x10,
    ANY_RESULT = 0xFFFF
  };
  virtual ResultType getReturnType() = 0;
  bool canReturnType(ResultType aType) {
    return (getReturnType() & aType) != 0;
  }

  using ContextSensitivity = uint16_t;
  enum {
    NO_CONTEXT = 0x00,
    NODE_CONTEXT = 0x01,
    POSITION_CONTEXT = 0x02,
    SIZE_CONTEXT = 0x04,
    NODESET_CONTEXT = POSITION_CONTEXT | SIZE_CONTEXT,
    VARIABLES_CONTEXT = 0x08,
    PRIVATE_CONTEXT = 0x10,
    ANY_CONTEXT = 0xFFFF
  };

  /**
   * Returns true if this expression is sensitive to *any* of
   * the requested contexts in aContexts.
   */
  virtual bool isSensitiveTo(ContextSensitivity aContexts) = 0;

  /**
   * Returns sub-expression at given position
   */
  virtual Expr* getSubExprAt(uint32_t aPos) = 0;

  /**
   * Replace sub-expression at given position. Does not delete the old
   * expression, that is the responsibility of the caller.
   */
  virtual void setSubExprAt(uint32_t aPos, Expr* aExpr) = 0;

  virtual nsresult evaluateToBool(txIEvalContext* aContext, bool& aResult);

  virtual nsresult evaluateToString(txIEvalContext* aContext,
                                    nsString& aResult);

#ifdef TX_TO_STRING
  /**
   * Returns the String representation of this Expr.
   * @param dest the String to use when creating the String
   * representation. The String representation will be appended to
   * any data in the destination String, to allow cascading calls to
   * other #toString() methods for Expressions.
   * @return the String representation of this Expr.
   **/
  virtual void toString(nsAString& str) = 0;
#endif
};  //-- Expr

#ifdef TX_TO_STRING
#  define TX_DECL_TOSTRING void toString(nsAString& aDest) override;
#  define TX_DECL_APPENDNAME void appendName(nsAString& aDest) override;
#else
#  define TX_DECL_TOSTRING
#  define TX_DECL_APPENDNAME
#endif

#define TX_DECL_EXPR_BASE                                              \
  nsresult evaluate(txIEvalContext* aContext, txAExprResult** aResult) \
      override;                                                        \
  ResultType getReturnType() override;                                 \
  bool isSensitiveTo(ContextSensitivity aContexts) override;

#define TX_DECL_EXPR                          \
  TX_DECL_EXPR_BASE                           \
  TX_DECL_TOSTRING                            \
  Expr* getSubExprAt(uint32_t aPos) override; \
  void setSubExprAt(uint32_t aPos, Expr* aExpr) override;

#define TX_DECL_OPTIMIZABLE_EXPR \
  TX_DECL_EXPR                   \
  ExprType getType() override;

#define TX_DECL_FUNCTION \
  TX_DECL_APPENDNAME     \
  TX_DECL_EXPR_BASE

#define TX_IMPL_EXPR_STUBS_BASE(_class, _ReturnType) \
  Expr::ResultType _class::getReturnType() { return _ReturnType; }

#define TX_IMPL_EXPR_STUBS_0(_class, _ReturnType)               \
  TX_IMPL_EXPR_STUBS_BASE(_class, _ReturnType)                  \
  Expr* _class::getSubExprAt(uint32_t aPos) { return nullptr; } \
  void _class::setSubExprAt(uint32_t aPos, Expr* aExpr) {       \
    MOZ_ASSERT_UNREACHABLE("setting bad subexpression index");  \
  }

#define TX_IMPL_EXPR_STUBS_1(_class, _ReturnType, _Expr1)      \
  TX_IMPL_EXPR_STUBS_BASE(_class, _ReturnType)                 \
  Expr* _class::getSubExprAt(uint32_t aPos) {                  \
    if (aPos == 0) {                                           \
      return _Expr1.get();                                     \
    }                                                          \
    return nullptr;                                            \
  }                                                            \
  void _class::setSubExprAt(uint32_t aPos, Expr* aExpr) {      \
    NS_ASSERTION(aPos < 1, "setting bad subexpression index"); \
    mozilla::Unused << _Expr1.release();                       \
    _Expr1 = mozilla::WrapUnique(aExpr);                       \
  }

#define TX_IMPL_EXPR_STUBS_2(_class, _ReturnType, _Expr1, _Expr2) \
  TX_IMPL_EXPR_STUBS_BASE(_class, _ReturnType)                    \
  Expr* _class::getSubExprAt(uint32_t aPos) {                     \
    switch (aPos) {                                               \
      case 0:                                                     \
        return _Expr1.get();                                      \
      case 1:                                                     \
        return _Expr2.get();                                      \
      default:                                                    \
        break;                                                    \
    }                                                             \
    return nullptr;                                               \
  }                                                               \
  void _class::setSubExprAt(uint32_t aPos, Expr* aExpr) {         \
    NS_ASSERTION(aPos < 2, "setting bad subexpression index");    \
    if (aPos == 0) {                                              \
      mozilla::Unused << _Expr1.release();                        \
      _Expr1 = mozilla::WrapUnique(aExpr);                        \
    } else {                                                      \
      mozilla::Unused << _Expr2.release();                        \
      _Expr2 = mozilla::WrapUnique(aExpr);                        \
    }                                                             \
  }

#define TX_IMPL_EXPR_STUBS_LIST(_class, _ReturnType, _ExprList) \
  TX_IMPL_EXPR_STUBS_BASE(_class, _ReturnType)                  \
  Expr* _class::getSubExprAt(uint32_t aPos) {                   \
    return _ExprList.SafeElementAt(aPos);                       \
  }                                                             \
  void _class::setSubExprAt(uint32_t aPos, Expr* aExpr) {       \
    NS_ASSERTION(aPos < _ExprList.Length(),                     \
                 "setting bad subexpression index");            \
    _ExprList[aPos] = aExpr;                                    \
  }

/**
 * This class represents a FunctionCall as defined by the XPath 1.0
 * Recommendation.
 **/
class FunctionCall : public Expr {
 public:
  /**
   * Adds the given parameter to this FunctionCall's parameter list.
   * The ownership of the given Expr is passed over to the FunctionCall,
   * even on failure.
   * @param aExpr the Expr to add to this FunctionCall's parameter list
   */
  void addParam(Expr* aExpr) { mParams.AppendElement(aExpr); }

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
  virtual bool requireParams(int32_t aParamCountMin, int32_t aParamCountMax,
                             txIEvalContext* aContext);

  TX_DECL_TOSTRING
  Expr* getSubExprAt(uint32_t aPos) override;
  void setSubExprAt(uint32_t aPos, Expr* aExpr) override;

 protected:
  txOwningArray<Expr> mParams;

  /*
   * Evaluates the given Expression and converts its result to a number.
   */
  static nsresult evaluateToNumber(Expr* aExpr, txIEvalContext* aContext,
                                   double* aResult);

  /*
   * Evaluates the given Expression and converts its result to a NodeSet.
   * If the result is not a NodeSet an error is returned.
   */
  static nsresult evaluateToNodeSet(Expr* aExpr, txIEvalContext* aContext,
                                    txNodeSet** aResult);

  /**
   * Returns true if any argument is sensitive to the given context.
   */
  bool argsSensitiveTo(ContextSensitivity aContexts);

#ifdef TX_TO_STRING
  /*
   * Appends the name of the function to `aStr`.
   */
  virtual void appendName(nsAString& aStr) = 0;
#endif
};

class txCoreFunctionCall : public FunctionCall {
 public:
  // This must be ordered in the same order as descriptTable in
  // txCoreFunctionCall.cpp. If you change one, change the other.
  enum eType {
    COUNT = 0,      // count()
    ID,             // id()
    LAST,           // last()
    LOCAL_NAME,     // local-name()
    NAMESPACE_URI,  // namespace-uri()
    NAME,           // name()
    POSITION,       // position()

    CONCAT,            // concat()
    CONTAINS,          // contains()
    NORMALIZE_SPACE,   // normalize-space()
    STARTS_WITH,       // starts-with()
    STRING,            // string()
    STRING_LENGTH,     // string-length()
    SUBSTRING,         // substring()
    SUBSTRING_AFTER,   // substring-after()
    SUBSTRING_BEFORE,  // substring-before()
    TRANSLATE,         // translate()

    NUMBER,   // number()
    ROUND,    // round()
    FLOOR,    // floor()
    CEILING,  // ceiling()
    SUM,      // sum()

    BOOLEAN,  // boolean()
    _FALSE,   // false()
    LANG,     // lang()
    _NOT,     // not()
    _TRUE     // true()
  };

  /*
   * Creates a txCoreFunctionCall of the given type
   */
  explicit txCoreFunctionCall(eType aType) : mType(aType) {}

  TX_DECL_FUNCTION

  static bool getTypeFromAtom(nsAtom* aName, eType& aType);

 private:
  eType mType;
};

/*
 * This class represents a NodeTest as defined by the XPath spec
 */
class txNodeTest {
 public:
  MOZ_COUNTED_DEFAULT_CTOR(txNodeTest)
  MOZ_COUNTED_DTOR_VIRTUAL(txNodeTest)

  /*
   * Virtual methods
   * pretty much a txPattern, but not supposed to be used
   * standalone. The NodeTest node() is different to the
   * Pattern "node()" (document node isn't matched)
   */
  virtual nsresult matches(const txXPathNode& aNode, txIMatchContext* aContext,
                           bool& aMatched) = 0;
  virtual double getDefaultPriority() = 0;

  /**
   * Returns the type of this nodetest.
   */
  enum NodeTestType { NAME_TEST, NODETYPE_TEST, OTHER_TEST };
  virtual NodeTestType getType() { return OTHER_TEST; }

  /**
   * Returns true if this expression is sensitive to *any* of
   * the requested flags.
   */
  virtual bool isSensitiveTo(Expr::ContextSensitivity aContext) = 0;

#ifdef TX_TO_STRING
  virtual void toString(nsAString& aDest) = 0;
#endif
};

#define TX_DECL_NODE_TEST                                               \
  TX_DECL_TOSTRING                                                      \
  nsresult matches(const txXPathNode& aNode, txIMatchContext* aContext, \
                   bool& aMatched) override;                            \
  double getDefaultPriority() override;                                 \
  bool isSensitiveTo(Expr::ContextSensitivity aContext) override;

/*
 * This class represents a NameTest as defined by the XPath spec
 */
class txNameTest : public txNodeTest {
 public:
  /*
   * Creates a new txNameTest with the given type and the given
   * principal node type
   */
  txNameTest(nsAtom* aPrefix, nsAtom* aLocalName, int32_t aNSID,
             uint16_t aNodeType);

  NodeTestType getType() override;

  TX_DECL_NODE_TEST

  RefPtr<nsAtom> mPrefix;
  RefPtr<nsAtom> mLocalName;
  int32_t mNamespace;

 private:
  uint16_t mNodeType;
};

/*
 * This class represents a NodeType as defined by the XPath spec
 */
class txNodeTypeTest : public txNodeTest {
 public:
  enum NodeType { COMMENT_TYPE, TEXT_TYPE, PI_TYPE, NODE_TYPE };

  /*
   * Creates a new txNodeTypeTest of the given type
   */
  explicit txNodeTypeTest(NodeType aNodeType) : mNodeType(aNodeType) {}

  /*
   * Sets the name of the node to match. Only availible for pi nodes
   */
  void setNodeName(const nsAString& aName) { mNodeName = NS_Atomize(aName); }

  NodeType getNodeTestType() { return mNodeType; }

  NodeTestType getType() override;

  TX_DECL_NODE_TEST

 private:
  NodeType mNodeType;
  RefPtr<nsAtom> mNodeName;
};

/**
 * Class representing a nodetest combined with a predicate. May only be used
 * if the predicate is not sensitive to the context-nodelist.
 */
class txPredicatedNodeTest : public txNodeTest {
 public:
  txPredicatedNodeTest(txNodeTest* aNodeTest, Expr* aPredicate);
  TX_DECL_NODE_TEST

 private:
  mozilla::UniquePtr<txNodeTest> mNodeTest;
  mozilla::UniquePtr<Expr> mPredicate;
};

/**
 * Represents an ordered list of Predicates,
 * for use with Step and Filter Expressions
 **/
class PredicateList {
 public:
  /**
   * Adds the given Expr to the list.
   * The ownership of the given Expr is passed over the PredicateList,
   * even on failure.
   * @param aExpr the Expr to add to the list
   */
  void add(Expr* aExpr) {
    NS_ASSERTION(aExpr, "missing expression");
    mPredicates.AppendElement(aExpr);
  }

  nsresult evaluatePredicates(txNodeSet* aNodes, txIMatchContext* aContext);

  /**
   * Drops the first predicate without deleting it.
   */
  void dropFirst() { mPredicates.RemoveElementAt(0); }

  /**
   * returns true if this predicate list is empty
   **/
  bool isEmpty() { return mPredicates.IsEmpty(); }

#ifdef TX_TO_STRING
  /**
   * Returns the String representation of this PredicateList.
   * @param dest the String to use when creating the String
   * representation. The String representation will be appended to
   * any data in the destination String, to allow cascading calls to
   * other #toString() methods for Expressions.
   * @return the String representation of this PredicateList.
   **/
  void toString(nsAString& dest);
#endif

 protected:
  bool isSensitiveTo(Expr::ContextSensitivity aContext);
  Expr* getSubExprAt(uint32_t aPos) { return mPredicates.SafeElementAt(aPos); }
  void setSubExprAt(uint32_t aPos, Expr* aExpr) {
    NS_ASSERTION(aPos < mPredicates.Length(),
                 "setting bad subexpression index");
    mPredicates[aPos] = aExpr;
  }

  //-- list of predicates
  txOwningArray<Expr> mPredicates;
};  //-- PredicateList

class LocationStep : public Expr, public PredicateList {
 public:
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
  LocationStep(txNodeTest* aNodeTest, LocationStepType aAxisIdentifier)
      : mNodeTest(aNodeTest), mAxisIdentifier(aAxisIdentifier) {}

  TX_DECL_OPTIMIZABLE_EXPR

  txNodeTest* getNodeTest() { return mNodeTest.get(); }
  void setNodeTest(txNodeTest* aNodeTest) {
    mozilla::Unused << mNodeTest.release();
    mNodeTest = mozilla::WrapUnique(aNodeTest);
  }
  LocationStepType getAxisIdentifier() { return mAxisIdentifier; }
  void setAxisIdentifier(LocationStepType aAxisIdentifier) {
    mAxisIdentifier = aAxisIdentifier;
  }

 private:
  /**
   * Append the current position of aWalker to aNodes if it matches mNodeTest,
   * using aContext as the context for matching.
   */
  nsresult appendIfMatching(const txXPathTreeWalker& aWalker,
                            txIMatchContext* aContext, txNodeSet* aNodes);

  /**
   * Append the descendants of the current position of aWalker to aNodes if
   * they match mNodeTest, using aContext as the context for matching.
   */
  nsresult appendMatchingDescendants(const txXPathTreeWalker& aWalker,
                                     txIMatchContext* aContext,
                                     txNodeSet* aNodes);

  /**
   * Append the descendants of the current position of aWalker to aNodes in
   * reverse order if they match mNodeTest, using aContext as the context for
   * matching.
   */
  nsresult appendMatchingDescendantsRev(const txXPathTreeWalker& aWalker,
                                        txIMatchContext* aContext,
                                        txNodeSet* aNodes);

  mozilla::UniquePtr<txNodeTest> mNodeTest;
  LocationStepType mAxisIdentifier;
};

class FilterExpr : public Expr, public PredicateList {
 public:
  /**
   * Creates a new FilterExpr using the given Expr
   * @param expr the Expr to use for evaluation
   */
  explicit FilterExpr(Expr* aExpr) : expr(aExpr) {}

  TX_DECL_EXPR

 private:
  mozilla::UniquePtr<Expr> expr;

};  //-- FilterExpr

class txLiteralExpr : public Expr {
 public:
  explicit txLiteralExpr(double aDbl)
      : mValue(new NumberResult(aDbl, nullptr)) {}
  explicit txLiteralExpr(const nsAString& aStr)
      : mValue(new StringResult(aStr, nullptr)) {}
  explicit txLiteralExpr(txAExprResult* aValue) : mValue(aValue) {}

  TX_DECL_EXPR

 private:
  RefPtr<txAExprResult> mValue;
};

/**
 * Represents an UnaryExpr. Returns the negative value of its expr.
 **/
class UnaryExpr : public Expr {
 public:
  explicit UnaryExpr(Expr* aExpr) : expr(aExpr) {}

  TX_DECL_EXPR

 private:
  mozilla::UniquePtr<Expr> expr;
};  //-- UnaryExpr

/**
 * Represents a BooleanExpr, a binary expression that
 * performs a boolean operation between its lvalue and rvalue.
 **/
class BooleanExpr : public Expr {
 public:
  //-- BooleanExpr Types
  enum _BooleanExprType { AND = 1, OR };

  BooleanExpr(Expr* aLeftExpr, Expr* aRightExpr, short aOp)
      : leftExpr(aLeftExpr), rightExpr(aRightExpr), op(aOp) {}

  TX_DECL_EXPR

 private:
  mozilla::UniquePtr<Expr> leftExpr, rightExpr;
  short op;
};  //-- BooleanExpr

/**
 * Represents a MultiplicativeExpr, a binary expression that
 * performs a multiplicative operation between its lvalue and rvalue:
 *  *   : multiply
 * mod  : modulus
 * div  : divide
 *
 **/
class txNumberExpr : public Expr {
 public:
  enum eOp { ADD, SUBTRACT, DIVIDE, MULTIPLY, MODULUS };

  txNumberExpr(Expr* aLeftExpr, Expr* aRightExpr, eOp aOp)
      : mLeftExpr(aLeftExpr), mRightExpr(aRightExpr), mOp(aOp) {}

  TX_DECL_EXPR

 private:
  mozilla::UniquePtr<Expr> mLeftExpr, mRightExpr;
  eOp mOp;
};  //-- MultiplicativeExpr

/**
 * Represents a RelationalExpr, an expression that compares its lvalue
 * to its rvalue using:
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

  RelationalExpr(Expr* aLeftExpr, Expr* aRightExpr, RelationalExprType aOp)
      : mLeftExpr(aLeftExpr), mRightExpr(aRightExpr), mOp(aOp) {}

  TX_DECL_EXPR

 private:
  bool compareResults(txIEvalContext* aContext, txAExprResult* aLeft,
                      txAExprResult* aRight);

  mozilla::UniquePtr<Expr> mLeftExpr;
  mozilla::UniquePtr<Expr> mRightExpr;
  RelationalExprType mOp;
};

/**
 * VariableRefExpr
 * Represents a variable reference ($refname)
 **/
class VariableRefExpr : public Expr {
 public:
  VariableRefExpr(nsAtom* aPrefix, nsAtom* aLocalName, int32_t aNSID);

  TX_DECL_EXPR

 private:
  RefPtr<nsAtom> mPrefix;
  RefPtr<nsAtom> mLocalName;
  int32_t mNamespace;
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
   * Adds the Expr to this PathExpr
   * The ownership of the given Expr is passed over the PathExpr,
   * even on failure.
   * @param aExpr the Expr to add to this PathExpr
   */
  void addExpr(Expr* aExpr, PathOperator pathOp);

  /**
   * Removes and deletes the expression at the given index.
   */
  void deleteExprAt(uint32_t aPos) {
    NS_ASSERTION(aPos < mItems.Length(), "killing bad expression index");
    mItems.RemoveElementAt(aPos);
  }

  TX_DECL_OPTIMIZABLE_EXPR

  PathOperator getPathOpAt(uint32_t aPos) {
    NS_ASSERTION(aPos < mItems.Length(), "getting bad pathop index");
    return mItems[aPos].pathOp;
  }
  void setPathOpAt(uint32_t aPos, PathOperator aPathOp) {
    NS_ASSERTION(aPos < mItems.Length(), "setting bad pathop index");
    mItems[aPos].pathOp = aPathOp;
  }

 private:
  class PathExprItem {
   public:
    mozilla::UniquePtr<Expr> expr;
    PathOperator pathOp;
  };

  nsTArray<PathExprItem> mItems;

  /*
   * Selects from the descendants of the context node
   * all nodes that match the Expr
   */
  nsresult evalDescendants(Expr* aStep, const txXPathNode& aNode,
                           txIMatchContext* aContext, txNodeSet* resNodes);
};

/**
 * This class represents a RootExpr, which only matches the Document node
 **/
class RootExpr : public Expr {
 public:
  /**
   * Creates a new RootExpr
   */
  RootExpr()
#ifdef TX_TO_STRING
      : mSerialize(true)
#endif
  {
  }

  TX_DECL_EXPR

#ifdef TX_TO_STRING
 public:
  void setSerialize(bool aSerialize) { mSerialize = aSerialize; }

 private:
  // When a RootExpr is used in a PathExpr it shouldn't be serialized
  bool mSerialize;
#endif
};  //-- RootExpr

/**
 *  Represents a UnionExpr
 **/
class UnionExpr : public Expr {
 public:
  /**
   * Adds the PathExpr to this UnionExpr
   * The ownership of the given Expr is passed over the UnionExpr,
   * even on failure.
   * @param aExpr the Expr to add to this UnionExpr
   */
  void addExpr(Expr* aExpr) { mExpressions.AppendElement(aExpr); }

  /**
   * Removes and deletes the expression at the given index.
   */
  void deleteExprAt(uint32_t aPos) {
    NS_ASSERTION(aPos < mExpressions.Length(), "killing bad expression index");

    delete mExpressions[aPos];
    mExpressions.RemoveElementAt(aPos);
  }

  TX_DECL_OPTIMIZABLE_EXPR

 private:
  txOwningArray<Expr> mExpressions;

};  //-- UnionExpr

/**
 * Class specializing in executing expressions like "@foo" where we are
 * interested in different result-types, and expressions like "@foo = 'hi'"
 */
class txNamedAttributeStep : public Expr {
 public:
  txNamedAttributeStep(int32_t aNsID, nsAtom* aPrefix, nsAtom* aLocalName);

  TX_DECL_EXPR

 private:
  int32_t mNamespace;
  RefPtr<nsAtom> mPrefix;
  RefPtr<nsAtom> mLocalName;
};

/**
 *
 */
class txUnionNodeTest : public txNodeTest {
 public:
  void addNodeTest(txNodeTest* aNodeTest) {
    mNodeTests.AppendElement(aNodeTest);
  }

  TX_DECL_NODE_TEST

 private:
  txOwningArray<txNodeTest> mNodeTests;
};

/**
 *  Expression that failed to parse
 */
class txErrorExpr : public Expr {
 public:
#ifdef TX_TO_STRING
  explicit txErrorExpr(const nsAString& aStr) : mStr(aStr) {}
#endif

  TX_DECL_EXPR

#ifdef TX_TO_STRING
 private:
  nsString mStr;
#endif
};

#endif
