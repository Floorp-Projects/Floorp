/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TX_XSLT_PATTERNS_H
#define TX_XSLT_PATTERNS_H

#include "mozilla/Attributes.h"
#include "txExpandedName.h"
#include "txExpr.h"
#include "txXMLUtils.h"

class txPattern {
 public:
  MOZ_COUNTED_DEFAULT_CTOR(txPattern)
  MOZ_COUNTED_DTOR_VIRTUAL(txPattern)

  /*
   * Determines whether this Pattern matches the given node.
   */
  virtual nsresult matches(const txXPathNode& aNode, txIMatchContext* aContext,
                           bool& aMatched) = 0;

  /*
   * Returns the default priority of this Pattern.
   *
   * Simple Patterns return the values as specified in XPath 5.5.
   * Returns -Inf for union patterns, as it shouldn't be called on them.
   */
  virtual double getDefaultPriority() = 0;

  /**
   * Returns the type of this pattern.
   */
  enum Type { STEP_PATTERN, UNION_PATTERN, OTHER_PATTERN };
  virtual Type getType() { return OTHER_PATTERN; }

  /**
   * Returns sub-expression at given position
   */
  virtual Expr* getSubExprAt(uint32_t aPos) = 0;

  /**
   * Replace sub-expression at given position. Does not delete the old
   * expression, that is the responsibility of the caller.
   */
  virtual void setSubExprAt(uint32_t aPos, Expr* aExpr) = 0;

  /**
   * Returns sub-pattern at given position
   */
  virtual txPattern* getSubPatternAt(uint32_t aPos) = 0;

  /**
   * Replace sub-pattern at given position. Does not delete the old
   * pattern, that is the responsibility of the caller.
   */
  virtual void setSubPatternAt(uint32_t aPos, txPattern* aPattern) = 0;

#ifdef TX_TO_STRING
  /*
   * Returns the String representation of this Pattern.
   * @param dest the String to use when creating the String
   * representation. The String representation will be appended to
   * any data in the destination String, to allow cascading calls to
   * other #toString() methods for Patterns.
   * @return the String representation of this Pattern.
   */
  virtual void toString(nsAString& aDest) = 0;
#endif
};

#define TX_DECL_PATTERN_BASE                                            \
  nsresult matches(const txXPathNode& aNode, txIMatchContext* aContext, \
                   bool& aMatched) override;                            \
  double getDefaultPriority() override;                                 \
  virtual Expr* getSubExprAt(uint32_t aPos) override;                   \
  virtual void setSubExprAt(uint32_t aPos, Expr* aExpr) override;       \
  virtual txPattern* getSubPatternAt(uint32_t aPos) override;           \
  virtual void setSubPatternAt(uint32_t aPos, txPattern* aPattern) override

#ifndef TX_TO_STRING
#  define TX_DECL_PATTERN TX_DECL_PATTERN_BASE
#else
#  define TX_DECL_PATTERN \
    TX_DECL_PATTERN_BASE; \
    void toString(nsAString& aDest) override
#endif

#define TX_IMPL_PATTERN_STUBS_NO_SUB_EXPR(_class)               \
  Expr* _class::getSubExprAt(uint32_t aPos) { return nullptr; } \
  void _class::setSubExprAt(uint32_t aPos, Expr* aExpr) {       \
    MOZ_ASSERT_UNREACHABLE("setting bad subexpression index");  \
  }

#define TX_IMPL_PATTERN_STUBS_NO_SUB_PATTERN(_class)                    \
  txPattern* _class::getSubPatternAt(uint32_t aPos) { return nullptr; } \
  void _class::setSubPatternAt(uint32_t aPos, txPattern* aPattern) {    \
    MOZ_ASSERT_UNREACHABLE("setting bad subexpression index");          \
  }

class txUnionPattern : public txPattern {
 public:
  void addPattern(txPattern* aPattern) {
    mLocPathPatterns.AppendElement(aPattern);
  }

  TX_DECL_PATTERN;
  Type getType() override;

 private:
  txOwningArray<txPattern> mLocPathPatterns;
};

class txLocPathPattern : public txPattern {
 public:
  void addStep(txPattern* aPattern, bool isChild);

  TX_DECL_PATTERN;

 private:
  class Step {
   public:
    mozilla::UniquePtr<txPattern> pattern;
    bool isChild;
  };

  nsTArray<Step> mSteps;
};

class txRootPattern : public txPattern {
 public:
#ifdef TX_TO_STRING
  txRootPattern() : mSerialize(true) {}
#endif

  TX_DECL_PATTERN;

#ifdef TX_TO_STRING
 public:
  void setSerialize(bool aSerialize) { mSerialize = aSerialize; }

 private:
  // Don't serialize txRootPattern if it's used in a txLocPathPattern
  bool mSerialize;
#endif
};

class txIdPattern : public txPattern {
 public:
  explicit txIdPattern(const nsAString& aString);

  TX_DECL_PATTERN;

 private:
  nsTArray<RefPtr<nsAtom>> mIds;
};

class txKeyPattern : public txPattern {
 public:
  txKeyPattern(nsAtom* aPrefix, nsAtom* aLocalName, int32_t aNSID,
               const nsAString& aValue)
      : mName(aNSID, aLocalName),
#ifdef TX_TO_STRING
        mPrefix(aPrefix),
#endif
        mValue(aValue) {
  }

  TX_DECL_PATTERN;

 private:
  txExpandedName mName;
#ifdef TX_TO_STRING
  RefPtr<nsAtom> mPrefix;
#endif
  nsString mValue;
};

class txStepPattern : public txPattern, public PredicateList {
 public:
  txStepPattern(txNodeTest* aNodeTest, bool isAttr)
      : mNodeTest(aNodeTest), mIsAttr(isAttr) {}

  TX_DECL_PATTERN;
  Type getType() override;

  txNodeTest* getNodeTest() { return mNodeTest.get(); }
  void setNodeTest(txNodeTest* aNodeTest) {
    mozilla::Unused << mNodeTest.release();
    mNodeTest = mozilla::WrapUnique(aNodeTest);
  }

 private:
  mozilla::UniquePtr<txNodeTest> mNodeTest;
  bool mIsAttr;
};

#endif  // TX_XSLT_PATTERNS_H
