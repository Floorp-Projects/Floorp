/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Chris Waterson <waterson@netscape.com>
 */

#ifndef nsTemplateMatch_h__
#define nsTemplateMatch_h__

#include "nsFixedSizeAllocator.h"
#include "nsResourceSet.h"
#include "nsRuleNetwork.h"

/**
 * A "match" is a fully instantiated rule; that is, a complete and
 * consistent set of variable-to-value assignments for all the rule's
 * condition elements.
 *
 * Each match also contains information about the "optional"
 * variable-to-value assignments that can be specified using the
 * <bindings> element in a rule.
 */

class nsConflictSet;
class nsTemplateRule;

class nsTemplateMatch {
protected:
    PRInt32 mRefCnt;

public:
    static void* operator new(size_t aSize, nsFixedSizeAllocator& aAllocator) {
        return aAllocator.Alloc(aSize); }

    static void operator delete(void* aPtr, size_t aSize) {
        nsFixedSizeAllocator::Free(aPtr, aSize); }

    nsTemplateMatch(const nsTemplateRule* aRule,
          const Instantiation& aInstantiation,
          const nsAssignmentSet& aAssignments)
        : mRefCnt(1),
          mRule(aRule),
          mInstantiation(aInstantiation),
          mAssignments(aAssignments)
        { MOZ_COUNT_CTOR(nsTemplateMatch); }

    PRBool operator==(const nsTemplateMatch& aMatch) const {
        return mRule == aMatch.mRule && mInstantiation == aMatch.mInstantiation; }

    PRBool operator!=(const nsTemplateMatch& aMatch) const {
        return !(*this == aMatch); }

    /**
     * Get the assignment for the specified variable, computing the
     * value using the rule's bindings, if necessary.
     * @param aConflictSet
     * @param aVariable the variable for which to determine the assignment
     * @param aValue an out parameter that receives the value assigned to
     *   aVariable.
     * @return PR_TRUE if aVariable has an assignment, PR_FALSE otherwise.
     */
    PRBool GetAssignmentFor(nsConflictSet& aConflictSet, PRInt32 aVariable, Value* aValue);

    /**
     * The rule that this match applies for.
     */
    const nsTemplateRule* mRule;

    /**
     * The fully bound instantiation (variable-to-value assignments, with
     * memory element support) that match the rule's conditions.
     */
    Instantiation mInstantiation;

    /**
     * Any additional assignments that apply because of the rule's
     * bindings. These are computed lazily.
     */
    nsAssignmentSet mAssignments;

    /**
     * The set of resources that the nsTemplateMatch's bindings depend on. Should the
     * assertions relating to these resources change, then the rule will
     * still match (i.e., this match object is still "good"); however, we
     * may need to recompute the assignments that have been made using the
     * rule's bindings.
     */
    nsResourceSet mBindingDependencies;

    PRInt32 AddRef() { return ++mRefCnt; }

    PRInt32 Release() {
        NS_PRECONDITION(mRefCnt > 0, "bad refcnt");
        PRInt32 refcnt = --mRefCnt;
        if (refcnt == 0) delete this;
        return refcnt; }

protected:
    ~nsTemplateMatch() { MOZ_COUNT_DTOR(nsTemplateMatch); }

private:
    nsTemplateMatch(const nsTemplateMatch& aMatch); // not to be implemented
    void operator=(const nsTemplateMatch& aMatch); // not to be implemented
};

#endif // nsConflictSet_h__

