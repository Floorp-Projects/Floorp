/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsTemplateRule_h__
#define nsTemplateRule_h__

#include "nsCOMPtr.h"
#include "nsIRDFDataSource.h"
#include "nsIRDFResource.h"
#include "nsIContent.h"

class nsConflictSet;
class nsTemplateMatch;
class Value;
class VariableSet;

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
 * - Priority, which helps to determine which rule should be
 *   considered "active" if several rules "match".
 * 
 */
class nsTemplateRule
{
public:
    nsTemplateRule(nsIRDFDataSource* aDataSource,
         nsIContent* aContent,
         PRInt32 aPriority)
        : mDataSource(aDataSource),
          mContent(aContent),
          mContainerVariable(0),
          mMemberVariable(0),
          mPriority(aPriority),
          mCount(0),
          mCapacity(0),
          mBindings(nsnull)
        { MOZ_COUNT_CTOR(nsTemplateRule); }

    ~nsTemplateRule();

    /**
     * Return the content node that this rule was constructed from.
     * @param aResult an out parameter, which will contain the content node
     *   that this rule was constructed from
     * @return NS_OK if no errors occur.
     */
    nsresult GetContent(nsIContent** aResult) const;

    /**
     * Set the variable which should be used as the "container
     * variable" in this rule. The container variable will be bound to
     * an RDF resource that is the parent of the member resources.
     * @param aContainerVariable the variable that should be used as the
     *    "container variable" in this rule
     */
    void SetContainerVariable(PRInt32 aContainerVariable) {
        mContainerVariable = aContainerVariable; }

    /**
     * Retrieve the variable that this rule uses as its "container variable".
     * @return the variable that this rule uses as its "container variable".
     */
    PRInt32 GetContainerVariable() const {
        return mContainerVariable; }

    /**
     * Set the variable which should be used as the "member variable"
     * in this rule. The member variable will be bound to an RDF
     * resource that is the child of the container resource.
     * @param aMemberVariable the variable that should be used as the
     *   "member variable" in this rule.
     */
    void SetMemberVariable(PRInt32 aMemberVariable) {
        mMemberVariable = aMemberVariable; }

    /**
     * Retrieve the variable that this rule uses as its "member variable".
     * @return the variable that this rule uses as its "member variable"
     */
    PRInt32 GetMemberVariable() const {
        return mMemberVariable; }

    /**
     * Retrieve the "priority" of the rule with respect to the
     * other rules in the template
     * @return the rule's priority, lower values mean "use this first".
     */
    PRInt32 GetPriority() const {
        return mPriority; }

    /**
     * Determine if the rule has the specified binding
     */
    PRBool
    HasBinding(PRInt32 aSourceVariable,
               nsIRDFResource* aProperty,
               PRInt32 aTargetVariable) const;

    /**
     * Add a binding to the rule. A binding consists of an already-bound
     * source variable, and the RDF property that should be tested to
     * generate a target value. The target value is bound to a target
     * variable.
     *
     * @param aSourceVariable the source variable that will be used in
     *   the RDF query.
     * @param aProperty the RDF property that will be used in the RDF
     *   query.
     * @param aTargetVariable the variable whose value will be bound
     *   to the RDF node that is returned when querying the binding
     * @return NS_OK if no errors occur.
     */
    nsresult AddBinding(PRInt32 aSourceVariable,
                        nsIRDFResource* aProperty,
                        PRInt32 aTargetVariable);

    /**
     * Initialize a match by adding necessary binding dependencies to
     * the conflict set. This will allow us to properly update the
     * match later if a value should change that the match's bindings
     * depend on.
     * @param aConflictSet the conflict set
     * @param aMatch the match we to initialize
     * @return NS_OK if no errors occur.
     */
    nsresult InitBindings(nsConflictSet& aConflictSet, nsTemplateMatch* aMatch) const;

    /**
     * Compute the minimal set of changes to a match's bindings that
     * must occur which the specified change is made to the RDF graph.
     * @param aConflictSet the conflict set, which we may need to manipulate
     *   to update the binding dependencies.
     * @param aMatch the match for which we must recompute the bindings
     * @param aSource the "source" resource in the RDF graph
     * @param aProperty the "property" resource in the RDF graph
     * @param aOldTarget the old "target" node in the RDF graph
     * @param aNewTarget the new "target" node in the RDF graph.
     * @param aModifiedVars a VariableSet, into which this routine
     *   will assign each variable whose value has changed.
     * @return NS_OK if no errors occurred.
     */
    nsresult RecomputeBindings(nsConflictSet& aConflictSet,
                               nsTemplateMatch* aMatch,
                               nsIRDFResource* aSource,
                               nsIRDFResource* aProperty,
                               nsIRDFNode* aOldTarget,
                               nsIRDFNode* aNewTarget,
                               VariableSet& aModifiedVars) const;

    /**
     * Compute the value to assign to an arbitrary variable in a
     * match.  This may require us to work out several dependancies,
     * if there are bindings set up for this rule.
     * @param aConflictSet the conflict set; if necessary, we may add
     *   a "binding dependency" to the conflict set, which will allow us
     *   to correctly recompute the bindings later if they should change.
     * @param aMatch the match that provides the "seed" variable assignments,
     *   which we may need to extend using the rule's bindings.
     * @param aVariable the variable for which we are to compute the
     *   assignment.
     * @param aValue an out parameter that will receive the value that
     *   was assigned to aVariable, if we could find one.
     * @return PR_TRUE if an assignment was found for aVariable, PR_FALSE
     *   otherwise.
     */
    PRBool ComputeAssignmentFor(nsConflictSet& aConflictSet,
                                nsTemplateMatch* aMatch,
                                PRInt32 aVariable,
                                Value* aValue) const;

    /**
     * Determine if one variable depends on another in the rule's
     * bindings.
     * @param aChild the dependent variable, whose value may
     *   depend on the assignment of aParent.
     * @param aParent the variable whose value aChild is depending on.
     * @return PR_TRUE if aChild's assignment depends on the assignment
     *   for aParent, PR_FALSE otherwise.
     */
    PRBool DependsOn(PRInt32 aChild, PRInt32 aParent) const;

protected:
    nsCOMPtr<nsIRDFDataSource> mDataSource;
    nsCOMPtr<nsIContent> mContent;
    PRInt32 mContainerVariable;
    PRInt32 mMemberVariable;
    PRInt32 mPriority;

    PRInt32 mCount;
    PRInt32 mCapacity;

    struct Binding {
        PRInt32                  mSourceVariable;
        nsCOMPtr<nsIRDFResource> mProperty;
        PRInt32                  mTargetVariable;
        Binding*                 mNext;
        Binding*                 mParent;
    };

    Binding* mBindings;
};

#endif // nsTemplateRule_h__
