/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MatchPairs_h__
#define MatchPairs_h__

#include "ds/LifoAlloc.h"

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

    MatchPair()
      : start(-1), limit(-1)
    { }

    MatchPair(int start, int limit)
      : start(start), limit(limit)
    { }

    size_t length()      const { JS_ASSERT(!isUndefined()); return limit - start; }
    bool isEmpty()       const { return length() == 0; }
    bool isUndefined()   const { return start < 0; }

    void displace(size_t amount) {
        start += (start < 0) ? 0 : amount;
        limit += (limit < 0) ? 0 : amount;
    }

    inline bool check() const {
        JS_ASSERT(limit >= start);
        JS_ASSERT_IF(start < 0, start == -1);
        JS_ASSERT_IF(limit < 0, limit == -1);
        return true;
    }
};

/* Base class for RegExp execution output. */
class MatchPairs
{
  protected:
    size_t     pairCount_;   /* Length of pairs_. */
    MatchPair *pairs_;       /* Raw pointer into an allocated MatchPair buffer. */

  protected:
    /* Not used directly: use ScopedMatchPairs or VectorMatchPairs. */
    MatchPairs()
      : pairCount_(0), pairs_(NULL)
    { }

  protected:
    /* Functions used by friend classes. */
    friend class RegExpShared;
    friend class RegExpStatics;

    /* MatchPair buffer allocator: set pairs_ and pairCount_. */
    virtual bool allocOrExpandArray(size_t pairCount) = 0;

    bool initArray(size_t pairCount);
    bool initArrayFrom(MatchPairs &copyFrom);
    void forgetArray() { pairs_ = NULL; }

    void displace(size_t disp);
    inline void checkAgainst(size_t length);

  public:
    /* Querying functions in the style of RegExpStatics. */
    bool   empty() const           { return pairCount_ == 0; }
    size_t pairCount() const       { JS_ASSERT(pairCount_ > 0); return pairCount_; }
    size_t parenCount() const      { return pairCount_ - 1; }

  public:
    unsigned *rawBuf() const { return reinterpret_cast<unsigned *>(pairs_); }
    size_t length() const { return pairCount_; }

    /* Pair accessors. */
    const MatchPair &pair(size_t i) const {
        JS_ASSERT(pairCount_ && i < pairCount_);
        JS_ASSERT(pairs_);
        return pairs_[i];
    }

    const MatchPair &operator[](size_t i) const { return pair(i); }
};

/* MatchPairs allocated into temporary storage, removed when out of scope. */
class ScopedMatchPairs : public MatchPairs
{
    LifoAllocScope lifoScope_;

  public:
    /* Constructs an implicit LifoAllocScope. */
    ScopedMatchPairs(LifoAlloc *lifoAlloc)
      : lifoScope_(lifoAlloc)
    { }

    const MatchPair &operator[](size_t i) const { return pair(i); }

  protected:
    bool allocOrExpandArray(size_t pairCount);
};

/*
 * MatchPairs allocated into permanent storage, for RegExpStatics.
 * The Vector of MatchPairs is reusable by Vector expansion.
 */
class VectorMatchPairs : public MatchPairs
{
    Vector<MatchPair, 10, SystemAllocPolicy> vec_;

  public:
    VectorMatchPairs() {
        vec_.clear();
    }

    const MatchPair &operator[](size_t i) const { return pair(i); }

  protected:
    friend class RegExpStatics;
    bool allocOrExpandArray(size_t pairCount);
};

/*
 * Passes either MatchPair or MatchPairs through ExecuteRegExp()
 * to avoid duplication of generic code.
 */
struct MatchConduit
{
    union {
        MatchPair  *pair;
        MatchPairs *pairs;
    } u;
    bool isPair;

    explicit MatchConduit(MatchPair *pair) {
        isPair = true;
        u.pair = pair;
    }
    explicit MatchConduit(MatchPairs *pairs) {
        isPair = false;
        u.pairs = pairs;
    }
};

} /* namespace js */

#endif /* MatchPairs_h__ */
