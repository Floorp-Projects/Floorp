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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Chris Waterson <waterson@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef nsTemplateRule_h__
#define nsTemplateRule_h__

#include "nsCOMPtr.h"
#include "nsIAtom.h"
#include "nsIRDFDataSource.h"
#include "nsIRDFResource.h"
#include "nsIContent.h"
#include "nsIDOMNode.h"
#include "nsVoidArray.h"
#include "nsString.h"
#include "nsIXULTemplateRuleFilter.h"
#include "nsCycleCollectionParticipant.h"

class nsIXULTemplateQueryProcessor;
class nsTemplateQuerySet;

class nsTemplateCondition
{
public:
    // relations that may be used in a rule. They may be negated with the
    // negate flag. Less and Greater are used for numeric comparisons and
    // Before and After are used for string comparisons. For Less, Greater,
    // Before, After, Startswith, Endswith, and Contains, the source is
    // conceptually on the left of the relation and the target is on the
    // right. For example, if the relation is Contains, that means Match if
    // the source contains the target.
    enum ConditionRelation {
        eUnknown,
        eEquals,
        eLess,
        eGreater,
        eBefore,
        eAfter,
        eStartswith,
        eEndswith,
        eContains
    };

    nsTemplateCondition(nsIAtom* aSourceVariable,
                        const nsAString& aRelation,
                        nsIAtom* aTargetVariable,
                        PRBool mIgnoreCase,
                        PRBool mNegate);

    nsTemplateCondition(nsIAtom* aSourceVariable,
                        const nsAString& aRelation,
                        const nsAString& aTargets,
                        PRBool mIgnoreCase,
                        PRBool mNegate,
                        PRBool aIsMultiple);

    nsTemplateCondition(const nsAString& aSource,
                        const nsAString& aRelation,
                        nsIAtom* aTargetVariable,
                        PRBool mIgnoreCase,
                        PRBool mNegate);

    ~nsTemplateCondition() { MOZ_COUNT_DTOR(nsTemplateCondition); }

    nsTemplateCondition* GetNext() { return mNext; }
    void SetNext(nsTemplateCondition* aNext) { mNext = aNext; }

    void SetRelation(const nsAString& aRelation);

    PRBool
    CheckMatch(nsIXULTemplateResult* aResult);

    PRBool
    CheckMatchStrings(const nsAString& aLeftString,
                      const nsAString& aRightString);
protected:

    nsCOMPtr<nsIAtom>   mSourceVariable;
    nsString            mSource;
    ConditionRelation   mRelation;
    nsCOMPtr<nsIAtom>   mTargetVariable;
    nsStringArray       mTargetList;
    PRPackedBool        mIgnoreCase;
    PRPackedBool        mNegate;

   nsTemplateCondition* mNext;
};

/**
 * A rule consists of:
 *
 * - Conditions, a set of unbound variables with consistency
 *   constraints that specify the values that each variable can
 *   assume. The conditions must be completely and consistently
 *   "bound" for the rule to be considered "matched".
 *
 * - Bindings, a set of unbound variables with consistency constraints
 *   that specify the values that each variable can assume. Unlike the
 *   conditions, the bindings need not be bound for the rule to be
 *   considered matched.
 *
 * - Content that should be constructed when the rule is "activated".
 *
 */
class nsTemplateRule
{
public:
    nsTemplateRule(nsIContent* aRuleNode,
                   nsIContent* aAction,
                   nsTemplateQuerySet* aQuerySet);

    ~nsTemplateRule();

    /**
     * Return the <action> node that this rule was constructed from, or its
     * logical equivalent for shorthand syntaxes. That is, the parent node of
     * the content that should be generated for this rule.
     * @param aAction an out parameter, which will contain the content node
     *   that this rule uses to generated content
     * @return NS_OK if no errors occur.
     */
    nsresult GetAction(nsIContent** aAction) const;

    /**
     * Return the <rule> content node that this rule was constructed from.
     * @param aResult an out parameter, which will contain the rule node
     * @return NS_OK if no errors occur.
     */
    nsresult GetRuleNode(nsIDOMNode** aResult) const;

    void SetVars(nsIAtom* aRefVariable, nsIAtom* aMemberVariable)
    {
        mRefVariable = aRefVariable;
        mMemberVariable = aMemberVariable;
    }

    void SetRuleFilter(nsIXULTemplateRuleFilter* aRuleFilter)
    {
        mRuleFilter = aRuleFilter;
    }

    nsIAtom* GetTag() { return mTag; }
    void SetTag(nsIAtom* aTag) { mTag = aTag; }

    nsIAtom* GetMemberVariable() { return mMemberVariable; }

    /**
     * Set the first condition for the rule. Other conditions are linked
     * to it using the condition's SetNext method.
     */
    void SetCondition(nsTemplateCondition* aConditions);

    /**
     * Check if the result matches the rule by first looking at the conditions.
     * If the results is accepted by the conditions, the rule filter, if any
     * was set, is checked. If either check rejects a result, a match cannot
     * occur for this rule and result.
     */
    PRBool
    CheckMatch(nsIXULTemplateResult* aResult) const;

    /**
     * Determine if the rule has the specified binding
     */
    PRBool
    HasBinding(nsIAtom* aSourceVariable,
               nsAString& aExpr,
               nsIAtom* aTargetVariable) const;

    /**
     * Add a binding to the rule. A binding consists of an already-bound
     * source variable, and the RDF property that should be tested to
     * generate a target value. The target value is bound to a target
     * variable.
     *
     * @param aSourceVariable the source variable that will be used in
     *   the RDF query.
     * @param aExpr the expression that will be used in the query.
     * @param aTargetVariable the variable whose value will be bound
     *   to the RDF node that is returned when querying the binding
     * @return NS_OK if no errors occur.
     */
    nsresult AddBinding(nsIAtom* aSourceVariable,
                        nsAString& aExpr,
                        nsIAtom* aTargetVariable);

    /**
     * Inform the query processor of the bindings that are set for a rule.
     * This should be called after all the bindings for a rule are compiled.
     */
    nsresult
    AddBindingsToQueryProcessor(nsIXULTemplateQueryProcessor* aProcessor);

    void Traverse(nsCycleCollectionTraversalCallback &cb) const
    {
        if (mRuleNode) {
            cb.NoteXPCOMChild(mRuleNode);
        }
        if (mAction) {
            cb.NoteXPCOMChild(mAction);
        }
    }

protected:

    struct Binding {
        nsCOMPtr<nsIAtom>        mSourceVariable;
        nsCOMPtr<nsIAtom>        mTargetVariable;
        nsString                 mExpr;
        Binding*                 mNext;
        Binding*                 mParent;
    };

    // backreference to the query set which owns this rule
    nsTemplateQuerySet* mQuerySet;

    // the <rule> node, or the <template> node if there is no <rule>
    nsCOMPtr<nsIDOMNode> mRuleNode;

    // the <action> node, or, if there is no <action>, the container node
    // which contains the content to generate
    nsCOMPtr<nsIContent> mAction;

    // the rule filter set by the builder's SetRuleFilter function
    nsCOMPtr<nsIXULTemplateRuleFilter> mRuleFilter;

    // indicates that the rule will only match when generating content 
    // to be inserted into a container with this tag
    nsCOMPtr<nsIAtom> mTag;

    // linked-list of the bindings for the rule, owned by the rule.
    Binding* mBindings;

    nsCOMPtr<nsIAtom> mRefVariable;
    nsCOMPtr<nsIAtom> mMemberVariable;

    nsTemplateCondition* mConditions; // owned by nsTemplateRule
};

/** nsTemplateQuerySet
 *
 *  A single <queryset> which holds the query node and the rules for it.
 *  All builders have at least one queryset, which may be created with an
 *  explicit <queryset> tag or implied if the tag is not used.
 *
 *  These queryset objects are created and owned by the builder in its
 *  mQuerySets array.
 */
class nsTemplateQuerySet
{
protected:
    nsVoidArray mRules; // rules owned by nsTemplateQuerySet

    // a number which increments for each successive queryset. It is stored so
    // it can be used as an optimization when updating results so that it is
    // known where to insert them into a match.
    PRInt32 mPriority;

public:

    // <query> node
    nsCOMPtr<nsIContent> mQueryNode;

    // compiled opaque query object returned by the query processor's
    // CompileQuery call
    nsCOMPtr<nsISupports> mCompiledQuery;

    // indicates that the query will only generate content to be inserted into
    // a container with this tag
    nsCOMPtr<nsIAtom> mTag;

    nsTemplateQuerySet(PRInt32 aPriority)
        : mPriority(aPriority)
    {
        MOZ_COUNT_CTOR(nsTemplateQuerySet);
    }

    ~nsTemplateQuerySet()
    {
        MOZ_COUNT_DTOR(nsTemplateQuerySet);
        Clear();
    }

    PRInt32 Priority() const
    {
        return mPriority;
    }

    nsIAtom* GetTag() { return mTag; }
    void SetTag(nsIAtom* aTag) { mTag = aTag; }

    nsresult AddRule(nsTemplateRule *aChild)
    {
        // nsTemplateMatch stores the index as a 16-bit value,
        // so check to make sure for overflow
        if (mRules.Count() == PR_INT16_MAX)
            return NS_ERROR_FAILURE;

        if (!mRules.AppendElement(aChild))
            return NS_ERROR_OUT_OF_MEMORY;
        return NS_OK;
    }

    PRInt16 RuleCount() const
    {
        return mRules.Count();
    }

    nsTemplateRule* GetRuleAt(PRInt16 aIndex)
    {
        return NS_STATIC_CAST(nsTemplateRule*, mRules[aIndex]);
    }

    void Clear()
    {
        for (PRInt32 r = mRules.Count() - 1; r >= 0; r--) {
            nsTemplateRule* rule = NS_STATIC_CAST(nsTemplateRule* , mRules[r]);
            delete rule;
        }
        mRules.Clear();
    }
};

#endif // nsTemplateRule_h__
