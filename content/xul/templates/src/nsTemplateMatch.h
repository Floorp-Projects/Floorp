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

#ifndef nsTemplateMatch_h__
#define nsTemplateMatch_h__

#include "nsFixedSizeAllocator.h"
#include "nsResourceSet.h"
#include "nsRuleNetwork.h"
#include <new.h>

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
private:
    // Hide so that only Create() and Destroy() can be used to
    // allocate and deallocate from the heap
    void* operator new(size_t) { return 0; }
    void operator delete(void*, size_t) {}

protected:
    PRInt32 mRefCnt;

    nsTemplateMatch(const nsTemplateRule* aRule,
                    const Instantiation& aInstantiation,
                    const nsAssignmentSet& aAssignments)
        : mRefCnt(0),
          mRule(aRule),
          mInstantiation(aInstantiation),
          mAssignments(aAssignments) {}

public:
    static nsTemplateMatch*
    Create(nsFixedSizeAllocator& aPool,
           const nsTemplateRule* aRule,
           const Instantiation& aInstantiation,
           const nsAssignmentSet& aAssignments) {
        void* place = aPool.Alloc(sizeof(nsTemplateMatch));
        return place ? ::new (place) nsTemplateMatch(aRule, aInstantiation, aAssignments) : nsnull; }

    static void
    Destroy(nsFixedSizeAllocator& aPool, nsTemplateMatch* aMatch) {
        aMatch->~nsTemplateMatch();
        aPool.Free(aMatch, sizeof(*aMatch)); }

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

    PRInt32 AddRef() {
        ++mRefCnt;
        NS_LOG_ADDREF(this, mRefCnt, "nsTemplateMatch", sizeof(*this));
        return mRefCnt; }

    PRInt32 Release(nsFixedSizeAllocator& aPool) {
        NS_PRECONDITION(mRefCnt > 0, "bad refcnt");
        PRInt32 refcnt = --mRefCnt;
        NS_LOG_RELEASE(this, mRefCnt, "nsTemplateMatch");
        if (refcnt == 0)
            Destroy(aPool, this);
        return refcnt; }

private:
    nsTemplateMatch(const nsTemplateMatch& aMatch); // not to be implemented
    void operator=(const nsTemplateMatch& aMatch); // not to be implemented
};

#endif // nsConflictSet_h__

