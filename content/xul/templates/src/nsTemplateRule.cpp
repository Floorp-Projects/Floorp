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

#include "nsTemplateRule.h"
#include "nsTemplateMatch.h"
#include "nsRuleNetwork.h"
#include "nsConflictSet.h"
#include "nsVoidArray.h"

nsTemplateRule::~nsTemplateRule()
{
    MOZ_COUNT_DTOR(nsTemplateRule);

    while (mBindings) {
        Binding* doomed = mBindings;
        mBindings = mBindings->mNext;
        delete doomed;
    }
}


nsresult
nsTemplateRule::GetContent(nsIContent** aResult) const
{
    *aResult = mContent.get();
    NS_IF_ADDREF(*aResult);
    return NS_OK;
}

PRBool
nsTemplateRule::HasBinding(PRInt32 aSourceVariable,
                           nsIRDFResource* aProperty,
                           PRInt32 aTargetVariable) const
{
    for (Binding* binding = mBindings; binding != nsnull; binding = binding->mNext) {
        if ((binding->mSourceVariable == aSourceVariable) &&
            (binding->mProperty == aProperty) &&
            (binding->mTargetVariable == aTargetVariable))
            return PR_TRUE;
    }

    return PR_FALSE;
}

nsresult
nsTemplateRule::AddBinding(PRInt32 aSourceVariable,
                           nsIRDFResource* aProperty,
                           PRInt32 aTargetVariable)
{
    NS_PRECONDITION(aSourceVariable != 0, "no source variable!");
    if (! aSourceVariable)
        return NS_ERROR_INVALID_ARG;

    NS_PRECONDITION(aProperty != nsnull, "null ptr");
    if (! aProperty)
        return NS_ERROR_INVALID_ARG;

    NS_PRECONDITION(aTargetVariable != 0, "no target variable!");
    if (! aTargetVariable)
        return NS_ERROR_INVALID_ARG;

    NS_ASSERTION(! HasBinding(aSourceVariable, aProperty, aTargetVariable),
                 "binding added twice");

    Binding* newbinding = new Binding;
    if (! newbinding)
        return NS_ERROR_OUT_OF_MEMORY;

    newbinding->mSourceVariable = aSourceVariable;
    newbinding->mProperty       = aProperty;
    newbinding->mTargetVariable = aTargetVariable;
    newbinding->mParent         = nsnull;

    Binding* binding = mBindings;
    Binding** link = &mBindings;

    // Insert it at the end, unless we detect that an existing
    // binding's source is dependent on the newbinding's target.
    //
    // XXXwaterson this isn't enough to make sure that we get all of
    // the dependencies worked out right, but it'll do for now. For
    // example, if you have (ab, bc, cd), and insert them in the order
    // (cd, ab, bc), you'll get (bc, cd, ab). The good news is, if the
    // person uses a natural ordering when writing the XUL, it'll all
    // work out ok.
    while (binding) {
        if (binding->mSourceVariable == newbinding->mTargetVariable) {
            binding->mParent = newbinding;
            break;
        }
        else if (binding->mTargetVariable == newbinding->mSourceVariable) {
            newbinding->mParent = binding;
        }

        link = &binding->mNext;
        binding = binding->mNext;
    }

    // Insert the newbinding
    *link = newbinding;
    newbinding->mNext = binding;
    return NS_OK;
}

PRBool
nsTemplateRule::DependsOn(PRInt32 aChildVariable, PRInt32 aParentVariable) const
{
    // Determine whether the value for aChildVariable will depend on
    // the value for aParentVariable by examining the rule's bindings.
    Binding* child = mBindings;
    while ((child != nsnull) && (child->mSourceVariable != aChildVariable))
        child = child->mNext;

    if (! child)
        return PR_FALSE;

    Binding* parent = child->mParent;
    while (parent != nsnull) {
        if (parent->mSourceVariable == aParentVariable)
            return PR_TRUE;

        parent = parent->mParent;
    }

    return PR_FALSE;
}

//----------------------------------------------------------------------
//
// nsTemplateRule
//

nsresult
nsTemplateRule::InitBindings(nsConflictSet& aConflictSet, nsTemplateMatch* aMatch) const
{
    // Initialize a match's binding dependencies, so we can handle
    // updates and queries later.

    for (Binding* binding = mBindings; binding != nsnull; binding = binding->mNext) {
        // Add a dependency for bindings whose source variable comes
        // from one of the <conditions>.
        Value sourceValue;
        PRBool hasBinding =
            aMatch->mInstantiation.mAssignments.GetAssignmentFor(binding->mSourceVariable, &sourceValue);

        if (hasBinding) {
            nsIRDFResource* source = VALUE_TO_IRDFRESOURCE(sourceValue);
            aMatch->mBindingDependencies.Add(source);
            aConflictSet.AddBindingDependency(aMatch, source);
        }

        // If this binding is dependant on another binding, then we
        // need to eagerly compute its source variable's assignment.
        if (binding->mParent) {
            Value value;
            ComputeAssignmentFor(aConflictSet, aMatch, binding->mSourceVariable, &value);
        }
    }

    return NS_OK;
}

nsresult
nsTemplateRule::RecomputeBindings(nsConflictSet& aConflictSet,
                                  nsTemplateMatch* aMatch,
                                  nsIRDFResource* aSource,
                                  nsIRDFResource* aProperty,
                                  nsIRDFNode* aOldTarget,
                                  nsIRDFNode* aNewTarget,
                                  VariableSet& aModifiedVars) const
{
    // Given a match with a source, property, old target, and new
    // target, compute the minimal changes to the match's bindings.

    // A temporary, mutable collection for holding all of the
    // assignments that comprise the current match.
    nsAutoVoidArray assignments;

    {
        // Collect -all- of the assignments in match into a temporary,
        // mutable collection
        nsAssignmentSet::ConstIterator last = aMatch->mAssignments.Last();
        for (nsAssignmentSet::ConstIterator binding = aMatch->mAssignments.First(); binding != last; ++binding)
            assignments.AppendElement(new nsAssignment(*binding));

        // Truncate the match's assignments to only include
        // assignments made via condition tests. We'll add back
        // assignments as they are recomputed.
        aMatch->mAssignments = aMatch->mInstantiation.mAssignments;
    }

    PRInt32 i;

    // Iterate through each assignment, looking for the assignment
    // whose value corresponds to the source of the assertion that's
    // changing.
    for (i = 0; i < assignments.Count(); ++i) {
        nsAssignment* assignment = NS_STATIC_CAST(nsAssignment*, assignments[i]);
        if ((assignment->mValue.GetType() == Value::eISupports) &&
            (NS_STATIC_CAST(nsISupports*, assignment->mValue) == aSource)) {

            // ...When we find it, look for binding's whose source
            // variable depends on the assignment's variable
            for (Binding* binding = mBindings; binding != nsnull; binding = binding->mNext) {
                if ((binding->mSourceVariable != assignment->mVariable) ||
                    (binding->mProperty.get() != aProperty))
                    continue;

                // Found one. Now we iterate through the assignments,
                // doing fixup.
                for (PRInt32 j = 0; j < assignments.Count(); ++j) {
                    nsAssignment* dependent = NS_STATIC_CAST(nsAssignment*, assignments[j]);
                    if (dependent->mVariable == binding->mTargetVariable) {
                        // The assignment's variable is the target
                        // varible for the binding: we can update it
                        // in-place.
                        dependent->mValue = Value(aNewTarget);
                        aModifiedVars.Add(dependent->mVariable);
                    }
                    else if (DependsOn(dependent->mVariable, binding->mTargetVariable)) {
                        // The assignment's variable depends on the
                        // binding's target variable, which is
                        // changing. Rip it out.
                        nsIRDFResource* target = VALUE_TO_IRDFRESOURCE(dependent->mValue);
                        aMatch->mBindingDependencies.Remove(target);
                        aConflictSet.RemoveBindingDependency(aMatch, target);

                        delete dependent;
                        assignments.RemoveElementAt(j--);

                        aModifiedVars.Add(dependent->mVariable);
                    }
                    else {
                        // The dependent variable is irrelevant. Leave
                        // it alone.
                    }
                }
            }
        }
    }

    // Now our set of assignments will contain the original
    // assignments from the conditions, any unchanged assignments, and
    // the single assignment that was updated by iterating through the
    // bindings.
    //
    // Add these assignments *back* to the match (modulo the ones
    // already in the conditions).
    //
    // The values for any dependent assignments that we've ripped out
    // will be computed the next time that somebody asks us for them.
    for (i = assignments.Count() - 1; i >= 0; --i) {
        nsAssignment* assignment = NS_STATIC_CAST(nsAssignment*, assignments[i]);

        // Only add it if it's not already in the match's conditions
        if (! aMatch->mInstantiation.mAssignments.HasAssignment(*assignment)) {
            aMatch->mAssignments.Add(*assignment);
        }

        delete assignment;
    }

    return NS_OK;
}

PRBool
nsTemplateRule::ComputeAssignmentFor(nsConflictSet& aConflictSet,
                                     nsTemplateMatch* aMatch,
                                     PRInt32 aVariable,
                                     Value* aValue) const
{
    // Compute the value assignment for an arbitrary variable in a
    // match. Potentially fill in dependencies if they haven't been
    // resolved yet.
    for (Binding* binding = mBindings; binding != nsnull; binding = binding->mNext) {
        if (binding->mTargetVariable != aVariable)
            continue;

        // Potentially recur to find the value of the source.
        //
        // XXXwaterson this is sloppy, and could be dealt with more
        // directly by following binding->mParent.
        Value sourceValue;
        PRBool hasSourceAssignment =
            aMatch->GetAssignmentFor(aConflictSet, binding->mSourceVariable, &sourceValue);

        if (! hasSourceAssignment)
            return PR_FALSE;

        nsCOMPtr<nsIRDFNode> target;

        nsIRDFResource* source = VALUE_TO_IRDFRESOURCE(sourceValue);

        if (source) {
            mDataSource->GetTarget(source,
                                   binding->mProperty,
                                   PR_TRUE,
                                   getter_AddRefs(target));

            // Store the assignment in the match so we won't need to
            // retrieve it again.
            nsAssignment assignment(binding->mTargetVariable, Value(target.get()));
            aMatch->mAssignments.Add(assignment);

            // Add a dependency on the source, so we'll recompute the
            // assignment if somebody tweaks it.
            aMatch->mBindingDependencies.Add(source);
            aConflictSet.AddBindingDependency(aMatch, source);
        }

        *aValue = target.get();
        return PR_TRUE;
    }

    return PR_FALSE;
}
