/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is TransforMiiX XSLT processor code.
 *
 * The Initial Developer of the Original Code is
 * Axel Hecht.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Axel Hecht <axel@pike.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef TX_XSLT_PATTERNS_H
#define TX_XSLT_PATTERNS_H

#include "txExpr.h"
#include "txXMLUtils.h"

class ProcessorState;

class txPattern
{
public:
    txPattern()
    {
        MOZ_COUNT_CTOR(txPattern);
    }
    virtual ~txPattern()
    {
        MOZ_COUNT_DTOR(txPattern);
    }

    /*
     * Determines whether this Pattern matches the given node.
     */
    virtual MBool matches(const txXPathNode& aNode,
                          txIMatchContext* aContext) = 0;

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
    enum Type {
        STEP_PATTERN,
        UNION_PATTERN,
        OTHER_PATTERN
    };
    virtual Type getType()
    {
      return OTHER_PATTERN;
    }

    /**
     * Returns sub-expression at given position
     */
    virtual Expr* getSubExprAt(PRUint32 aPos) = 0;

    /**
     * Replace sub-expression at given position. Does not delete the old
     * expression, that is the responsibility of the caller.
     */
    virtual void setSubExprAt(PRUint32 aPos, Expr* aExpr) = 0;

    /**
     * Returns sub-pattern at given position
     */
    virtual txPattern* getSubPatternAt(PRUint32 aPos) = 0;

    /**
     * Replace sub-pattern at given position. Does not delete the old
     * pattern, that is the responsibility of the caller.
     */
    virtual void setSubPatternAt(PRUint32 aPos, txPattern* aPattern) = 0;

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

#define TX_DECL_PATTERN_BASE \
    MBool matches(const txXPathNode& aNode, txIMatchContext* aContext); \
    double getDefaultPriority(); \
    virtual Expr* getSubExprAt(PRUint32 aPos); \
    virtual void setSubExprAt(PRUint32 aPos, Expr* aExpr); \
    virtual txPattern* getSubPatternAt(PRUint32 aPos); \
    virtual void setSubPatternAt(PRUint32 aPos, txPattern* aPattern)

#ifndef TX_TO_STRING
#define TX_DECL_PATTERN TX_DECL_PATTERN_BASE
#else
#define TX_DECL_PATTERN \
    TX_DECL_PATTERN_BASE; \
    void toString(nsAString& aDest)
#endif

#define TX_IMPL_PATTERN_STUBS_NO_SUB_EXPR(_class)             \
Expr*                                                         \
_class::getSubExprAt(PRUint32 aPos)                           \
{                                                             \
    return nsnull;                                            \
}                                                             \
void                                                          \
_class::setSubExprAt(PRUint32 aPos, Expr* aExpr)              \
{                                                             \
    NS_NOTREACHED("setting bad subexpression index");         \
}

#define TX_IMPL_PATTERN_STUBS_NO_SUB_PATTERN(_class)          \
txPattern*                                                    \
_class::getSubPatternAt(PRUint32 aPos)                        \
{                                                             \
    return nsnull;                                            \
}                                                             \
void                                                          \
_class::setSubPatternAt(PRUint32 aPos, txPattern* aPattern)   \
{                                                             \
    NS_NOTREACHED("setting bad subexpression index");         \
}

class txUnionPattern : public txPattern
{
public:
    nsresult addPattern(txPattern* aPattern)
    {
        return mLocPathPatterns.AppendElement(aPattern) ? 
            NS_OK : NS_ERROR_OUT_OF_MEMORY;
    }

    TX_DECL_PATTERN;
    Type getType();

private:
    txOwningArray<txPattern> mLocPathPatterns;
};

class txLocPathPattern : public txPattern
{
public:
    nsresult addStep(txPattern* aPattern, bool isChild);

    TX_DECL_PATTERN;

private:
    class Step {
    public:
        nsAutoPtr<txPattern> pattern;
        bool isChild;
    };

    nsTArray<Step> mSteps;
};

class txRootPattern : public txPattern
{
public:
#ifdef TX_TO_STRING
    txRootPattern()
        : mSerialize(PR_TRUE)
    {
    }
#endif

    TX_DECL_PATTERN;

#ifdef TX_TO_STRING
public:
    void setSerialize(bool aSerialize)
    {
        mSerialize = aSerialize;
    }

private:
    // Don't serialize txRootPattern if it's used in a txLocPathPattern
    bool mSerialize;
#endif
};

class txIdPattern : public txPattern
{
public:
    txIdPattern(const nsSubstring& aString);

    TX_DECL_PATTERN;

private:
    nsCOMArray<nsIAtom> mIds;
};

class txKeyPattern : public txPattern
{
public:
    txKeyPattern(nsIAtom* aPrefix, nsIAtom* aLocalName,
                 PRInt32 aNSID, const nsAString& aValue)
        : mName(aNSID, aLocalName),
#ifdef TX_TO_STRING
          mPrefix(aPrefix),
#endif
          mValue(aValue)
    {
    }

    TX_DECL_PATTERN;

private:
    txExpandedName mName;
#ifdef TX_TO_STRING
    nsCOMPtr<nsIAtom> mPrefix;
#endif
    nsString mValue;
};

class txStepPattern : public txPattern,
                      public PredicateList
{
public:
    txStepPattern(txNodeTest* aNodeTest, bool isAttr)
        : mNodeTest(aNodeTest), mIsAttr(isAttr)
    {
    }

    TX_DECL_PATTERN;
    Type getType();

    txNodeTest* getNodeTest()
    {
      return mNodeTest;
    }
    void setNodeTest(txNodeTest* aNodeTest)
    {
      mNodeTest.forget();
      mNodeTest = aNodeTest;
    }

private:
    nsAutoPtr<txNodeTest> mNodeTest;
    bool mIsAttr;
};

#endif // TX_XSLT_PATTERNS_H
