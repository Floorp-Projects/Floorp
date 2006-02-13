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

#ifndef nsTemplateMatch_h__
#define nsTemplateMatch_h__

#include "nsFixedSizeAllocator.h"
#include "nsIContent.h"
#include "nsIXULTemplateQueryProcessor.h"
#include "nsIXULTemplateResult.h"
#include "nsRuleNetwork.h"
#include NEW_H

/**
 * A match object, where each match object is associated with one result.
 * There will be one match list for each unique id generated. However, since
 * there are multiple querysets and each may generate results with the same
 * id, they are all chained together in a linked list, ordered in the same
 * order as the respective <queryset> elements they were generated from.
 * Only one match is active at a time, but which match is active may change
 * as new results are added or removed. When a match is active, content is
 * generated for that match.
 *
 * Matches are stored and owned by the mMatchToMap hash in the template
 * builder.
 */

class nsTemplateRule;
class nsTemplateQuerySet;

class nsTemplateMatch {
private:
    // Hide so that only Create() and Destroy() can be used to
    // allocate and deallocate from the heap
    void* operator new(size_t) CPP_THROW_NEW { return 0; }
    void operator delete(void*, size_t) {}

public:
    nsTemplateMatch(nsTemplateQuerySet* aQuerySet,
                    nsIXULTemplateResult* aResult)
        : mRule(nsnull),
          mQuerySet(aQuerySet),
          mResult(aResult),
          mNext(nsnull) {}

    ~nsTemplateMatch() {}

    static nsTemplateMatch*
    Create(nsFixedSizeAllocator& aPool,
           nsTemplateQuerySet* aQuerySet,
           nsIXULTemplateResult* aResult) {
        void* place = aPool.Alloc(sizeof(nsTemplateMatch));
        return place ? ::new (place) nsTemplateMatch(aQuerySet, aResult) : nsnull; }

    static void
    Destroy(nsFixedSizeAllocator& aPool, nsTemplateMatch* aMatch) {
        aMatch->~nsTemplateMatch();
        aPool.Free(aMatch, sizeof(*aMatch)); }

    PRBool operator==(const nsTemplateMatch& aMatch) const {
        return mRule == aMatch.mRule && mResult == aMatch.mResult; }

    PRBool operator!=(const nsTemplateMatch& aMatch) const {
        return !(*this == aMatch); }

    nsresult RuleMatched(nsTemplateQuerySet* aQuerySet,
                         nsTemplateRule* aRule,
                         nsIXULTemplateResult* aResult);

    /**
     * The rule that this match applies to. If the rule is null, the result
     * has not matched a rule and no content has been generated. However, a
     * later queryset may still have matched.
     */
    nsTemplateRule* mRule; // not owned

    /**
     * The queryset for this rule
     */
    nsTemplateQuerySet* mQuerySet; // not owned

    /**
     * The result associated with this match
     */
    nsCOMPtr<nsIXULTemplateResult> mResult;

    /**
     * Matches are stored in a linked list, in priority order. This first
     * match that has a rule set (mRule) is the active match and generates
     * content. The next match is owned by the builder, which will delete
     * template matches when needed.
     */
    nsTemplateMatch *mNext;

private:
    nsTemplateMatch(const nsTemplateMatch& aMatch); // not to be implemented
    void operator=(const nsTemplateMatch& aMatch); // not to be implemented
};

#endif // nsTemplateMatch_h__

