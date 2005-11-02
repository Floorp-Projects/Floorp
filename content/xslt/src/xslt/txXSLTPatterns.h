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
 * The Original Code is TransforMiiX XSLT Processor.
 *
 * The Initial Developer of the Original Code is
 * Axel Hecht.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Axel Hecht <axel@pike.org>
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

#include "Expr.h"
#include "XMLUtils.h"
#include "nsVoidArray.h"

class ProcessorState;

class txPattern : public TxObject
{
public:
    virtual ~txPattern();

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

    /*
     * Returns the String representation of this Pattern.
     * @param dest the String to use when creating the String
     * representation. The String representation will be appended to
     * any data in the destination String, to allow cascading calls to
     * other #toString() methods for Patterns.
     * @return the String representation of this Pattern.
     */
    virtual void toString(nsAString& aDest) = 0;

    /*
     * Adds the simple Patterns to the List.
     * For union patterns, add all sub patterns,
     * all other (simple) patterns just add themselves.
     * This cuts the ownership of the union pattern and it's
     * simple patterns, leaving union patterns empty after a call
     * to this function.
     */
    virtual nsresult getSimplePatterns(txList &aList);
};

#define TX_DECL_PATTERN \
    MBool matches(const txXPathNode& aNode, txIMatchContext* aContext); \
    double getDefaultPriority(); \
    void toString(nsAString& aDest)
#define TX_DECL_PATTERN2 \
    TX_DECL_PATTERN; \
    nsresult getSimplePatterns(txList &aList)


class txUnionPattern : public txPattern
{
public:
    txUnionPattern()
    {
    }

    ~txUnionPattern();

    nsresult addPattern(txPattern* aPattern);

    TX_DECL_PATTERN2;

private:
    txList mLocPathPatterns;
};

class txLocPathPattern : public txPattern
{
public:
    txLocPathPattern()
    {
    }

    ~txLocPathPattern();

    nsresult addStep(txPattern* aPattern, MBool isChild);

    TX_DECL_PATTERN;

private:
    class Step {
    public:
        Step(txPattern* aPattern, MBool aIsChild)
            : pattern(aPattern), isChild(aIsChild)
        {
        }

        ~Step()
        {
            delete pattern;
        }

        txPattern* pattern;
        MBool isChild;
    };

    txList mSteps;
};

class txRootPattern : public txPattern
{
public:
    txRootPattern(MBool aSerialize) : mSerialize(aSerialize)
    {
    }

    ~txRootPattern();

    TX_DECL_PATTERN;
private:
    // Don't serialize txRootPattern if it's used in a txLocPathPattern
    MBool mSerialize;
};

class txIdPattern : public txPattern
{
public:
    txIdPattern(const nsAString& aString);

    ~txIdPattern();

    TX_DECL_PATTERN;

private:
    nsStringArray mIds;
};

class txKeyPattern : public txPattern
{
public:
    txKeyPattern(nsIAtom* aPrefix, nsIAtom* aLocalName,
                 PRInt32 aNSID, const nsAString& aValue)
        : mName(aNSID, aLocalName), mPrefix(aPrefix), mValue(aValue)
    {
    }

    ~txKeyPattern();

    TX_DECL_PATTERN;

private:
    txExpandedName mName;
    nsIAtom* mPrefix;
    nsString mValue;
};

class txStepPattern : public PredicateList, public txPattern
{
public:
    txStepPattern(txNodeTest* aNodeTest, MBool isAttr)
        :mNodeTest(aNodeTest), mIsAttr(isAttr)
    {
    }

    ~txStepPattern();

    TX_DECL_PATTERN;

private:
    txNodeTest* mNodeTest;
    MBool mIsAttr;
};

#endif // TX_XSLT_PATTERNS_H
