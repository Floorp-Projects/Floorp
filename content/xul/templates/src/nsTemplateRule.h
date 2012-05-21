/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsTemplateRule_h__
#define nsTemplateRule_h__

#include "nsCOMPtr.h"
#include "nsIAtom.h"
#include "nsIRDFDataSource.h"
#include "nsIRDFResource.h"
#include "nsIContent.h"
#include "nsIDOMNode.h"
#include "nsTArray.h"
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
                        bool mIgnoreCase,
                        bool mNegate);

    nsTemplateCondition(nsIAtom* aSourceVariable,
                        const nsAString& aRelation,
                        const nsAString& aTargets,
                        bool mIgnoreCase,
                        bool mNegate,
                        bool aIsMultiple);

    nsTemplateCondition(const nsAString& aSource,
                        const nsAString& aRelation,
                        nsIAtom* aTargetVariable,
                        bool mIgnoreCase,
                        bool mNegate);

    ~nsTemplateCondition() { MOZ_COUNT_DTOR(nsTemplateCondition); }

    nsTemplateCondition* GetNext() { return mNext; }
    void SetNext(nsTemplateCondition* aNext) { mNext = aNext; }

    void SetRelation(const nsAString& aRelation);

    bool
    CheckMatch(nsIXULTemplateResult* aResult);

    bool
    CheckMatchStrings(const nsAString& aLeftString,
                      const nsAString& aRightString);
protected:

    nsCOMPtr<nsIAtom>   mSourceVariable;
    nsString            mSource;
    ConditionRelation   mRelation;
    nsCOMPtr<nsIAtom>   mTargetVariable;
    nsTArray<nsString>  mTargetList;
    bool                mIgnoreCase;
    bool                mNegate;

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
    /**
     * The copy-constructor should only be called from nsTArray when appending
     * a new rule, otherwise things break because the copy constructor expects
     * mBindings and mConditions to be nsnull.
     */
    nsTemplateRule(const nsTemplateRule& aOtherRule);

    ~nsTemplateRule();

    /**
     * Return the <action> node that this rule was constructed from, or its
     * logical equivalent for shorthand syntaxes. That is, the parent node of
     * the content that should be generated for this rule.
     */
    nsIContent* GetAction() const { return mAction; }

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
    bool
    CheckMatch(nsIXULTemplateResult* aResult) const;

    /**
     * Determine if the rule has the specified binding
     */
    bool
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
        cb.NoteXPCOMChild(mRuleNode);
        cb.NoteXPCOMChild(mAction);
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
    nsTArray<nsTemplateRule> mRules;

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
    }

    PRInt32 Priority() const
    {
        return mPriority;
    }

    nsIAtom* GetTag() { return mTag; }
    void SetTag(nsIAtom* aTag) { mTag = aTag; }

    nsTemplateRule* NewRule(nsIContent* aRuleNode,
                            nsIContent* aAction,
                            nsTemplateQuerySet* aQuerySet)
    {
        // nsTemplateMatch stores the index as a 16-bit value,
        // so check to make sure for overflow
        if (mRules.Length() == PR_INT16_MAX)
            return nsnull;

        return mRules.AppendElement(nsTemplateRule(aRuleNode, aAction,
                                    aQuerySet));
    }
    
    void RemoveRule(nsTemplateRule *aRule)
    {
        mRules.RemoveElementAt(aRule - mRules.Elements());
    }

    PRInt16 RuleCount() const
    {
        return mRules.Length();
    }

    nsTemplateRule* GetRuleAt(PRInt16 aIndex)
    {
        if (PRUint32(aIndex) < mRules.Length()) {
            return &mRules[aIndex];
        }
        return nsnull;
    }

    void Clear()
    {
        mRules.Clear();
    }
};

#endif // nsTemplateRule_h__
