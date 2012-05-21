/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99 ft=cpp:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MatchPairs_h__
#define MatchPairs_h__

/*
 * RegExp match results are succinctly represented by pairs of integer
 * indices delimiting (start, limit] segments of the input string.
 *
 * The pair count for a given RegExp match is the capturing parentheses
 * count plus one for the "0 capturing paren" whole text match.
 */

namespace js {

struct MatchPair
{
    int start;
    int limit;

    MatchPair(int start, int limit) : start(start), limit(limit) {}

    size_t length() const {
        JS_ASSERT(!isUndefined());
        return limit - start;
    }

    bool isUndefined() const {
        return start == -1;
    }

    void check() const {
        JS_ASSERT(limit >= start);
        JS_ASSERT_IF(!isUndefined(), start >= 0);
    }
};

class MatchPairs
{
    size_t  pairCount_;
    int     buffer_[1];

    explicit MatchPairs(size_t pairCount) : pairCount_(pairCount) {
        initPairValues();
    }

    void initPairValues() {
        for (int *it = buffer_; it < buffer_ + 2 * pairCount_; ++it)
            *it = -1;
    }

    static size_t calculateSize(size_t backingPairCount) {
        return sizeof(MatchPairs) - sizeof(int) + sizeof(int) * backingPairCount;
    }

    int *buffer() { return buffer_; }

    friend class RegExpShared;

  public:
    /*
     * |backingPairCount| is necessary because PCRE uses extra space
     * after the actual results in the buffer.
     */
    static MatchPairs *create(LifoAlloc &alloc, size_t pairCount, size_t backingPairCount);

    size_t pairCount() const { return pairCount_; }

    MatchPair pair(size_t i) {
        JS_ASSERT(i < pairCount());
        return MatchPair(buffer_[2 * i], buffer_[2 * i + 1]);
    }

    void displace(size_t amount) {
        if (!amount)
            return;

        for (int *it = buffer_; it < buffer_ + 2 * pairCount_; ++it)
            *it = (*it < 0) ? -1 : *it + amount;
    }

    inline void checkAgainst(size_t length);
};

} /* namespace js */

#endif
