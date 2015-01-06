/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsTemplateMatch_h__
#define nsTemplateMatch_h__

#include "mozilla/Attributes.h"
#include "nsIContent.h"
#include "nsIXULTemplateQueryProcessor.h"
#include "nsIXULTemplateResult.h"
#include "nsRuleNetwork.h"

/**
 * A match object, where each match object is associated with one result.
 * There will be one match list for each unique id generated. However, since
 * there are multiple querysets and each may generate results with the same
 * id, they are all chained together in a linked list, ordered in the same
 * order as the respective <queryset> elements they were generated from.
 * A match can be identified by the container and id. The id is retrievable
 * from the result.
 *
 * Only one match per container and id pair is active at a time, but which
 * match is active may change as new results are added or removed. When a
 * match is active, content is generated for that match.
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
    void* operator new(size_t) CPP_THROW_NEW { MOZ_ASSERT(0); return nullptr; }
    void operator delete(void*, size_t) { MOZ_ASSERT(0); }

public:
    nsTemplateMatch(uint16_t aQuerySetPriority,
                    nsIXULTemplateResult* aResult,
                    nsIContent* aContainer)
        : mRuleIndex(-1),
          mQuerySetPriority(aQuerySetPriority),
          mContainer(aContainer),
          mResult(aResult),
          mNext(nullptr)
    {
      MOZ_COUNT_CTOR(nsTemplateMatch);
    }

    ~nsTemplateMatch()
    {
      MOZ_COUNT_DTOR(nsTemplateMatch);
    }

    static nsTemplateMatch*
    Create(uint16_t aQuerySetPriority,
           nsIXULTemplateResult* aResult,
           nsIContent* aContainer) {
        return ::new nsTemplateMatch(aQuerySetPriority, aResult, aContainer);
    }

    static void Destroy(nsTemplateMatch*& aMatch, bool aRemoveResult);

    // return true if the the match is active, and has generated output
    bool IsActive() {
        return mRuleIndex >= 0;
    }

    // indicate that a rule is no longer active, used when a query with a
    // lower priority has overriden the match
    void SetInactive() {
        mRuleIndex = -1;
    }

    // return matching rule index
    int16_t RuleIndex() {
        return mRuleIndex;
    }

    // return priority of query set
    uint16_t QuerySetPriority() {
        return mQuerySetPriority;
    }

    // return container, not addrefed. May be null.
    nsIContent* GetContainer() {
        return mContainer;
    }

    nsresult RuleMatched(nsTemplateQuerySet* aQuerySet,
                         nsTemplateRule* aRule,
                         int16_t aRuleIndex,
                         nsIXULTemplateResult* aResult);

private:

    /**
     * The index of the rule that matched, or -1 if the match is not active.
     */
    int16_t mRuleIndex;

    /**
     * The priority of the queryset for this rule
     */
    uint16_t mQuerySetPriority;

    /**
     * The container the content generated for the match is inside.
     */
    nsCOMPtr<nsIContent> mContainer;

public:

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

    nsTemplateMatch(const nsTemplateMatch& aMatch) = delete;
    void operator=(const nsTemplateMatch& aMatch) = delete;
};

#endif // nsTemplateMatch_h__

