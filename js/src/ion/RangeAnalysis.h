/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsion_range_analysis_h__
#define jsion_range_analysis_h__

#include "wtf/Platform.h"
#include "MIR.h"
#include "CompileInfo.h"

namespace js {
namespace ion {

class MBasicBlock;
class MIRGraph;

class RangeAnalysis
{
  protected:
    bool blockDominates(MBasicBlock *b, MBasicBlock *b2);
    void replaceDominatedUsesWith(MDefinition *orig, MDefinition *dom,
                                  MBasicBlock *block);

  protected:
    MIRGraph &graph_;

  public:
    RangeAnalysis(MIRGraph &graph);
    bool addBetaNobes();
    bool analyze();
    bool removeBetaNobes();
};

struct RangeChangeCount;
class Range {
  private:
    // :TODO: we should do symbolic range evaluation, where we have
    // information of the form v1 < v2 for arbitrary defs v1 and v2, not
    // just constants of type int32.
    // (Bug 766592)

    // We represent ranges where the endpoints can be in the set:
    // {-infty} U [INT_MIN, INT_MAX] U {infty}.  A bound of +/-
    // infty means that the value may have overflowed in that
    // direction. When computing the range of an integer
    // instruction, the ranges of the operands can be clamped to
    // [INT_MIN, INT_MAX], since if they had overflowed they would
    // no longer be integers. This is important for optimizations
    // and somewhat subtle.
    //
    // N.B.: All of the operations that compute new ranges based
    // on existing ranges will ignore the _infinite_ flags of the
    // input ranges; that is, they implicitly clamp the ranges of
    // the inputs to [INT_MIN, INT_MAX]. Therefore, while our range might
    // be infinite (and could overflow), when using this information to
    // propagate through other ranges, we disregard this fact; if that code
    // executes, then the overflow did not occur, so we may safely assume
    // that the range is [INT_MIN, INT_MAX] instead.
    //
    // To facilitate this trick, we maintain the invariants that:
    // 1) lower_infinite == true implies lower_ == JSVAL_INT_MIN
    // 2) upper_infinite == true implies upper_ == JSVAL_INT_MAX
    int32 lower_;
    bool lower_infinite_;
    int32 upper_;
    bool upper_infinite_;

  public:
    Range()
        : lower_(JSVAL_INT_MIN),
          lower_infinite_(true),
          upper_(JSVAL_INT_MAX),
          upper_infinite_(true)
    {}

    Range(int64_t l, int64_t h) {
        setLower(l);
        setUpper(h);
    }

    Range(const Range &other)
    : lower_(other.lower_),
      lower_infinite_(other.lower_infinite_),
      upper_(other.upper_),
      upper_infinite_(other.upper_infinite_)
    {}

    static int64_t abs64(int64_t x) {
#ifdef WTF_OS_WINDOWS
        return _abs64(x);
#else
        return llabs(x);
#endif
    }

    void printRange(FILE *fp);
    bool update(const Range *other);
    bool update(const Range &other) {
        return update(&other);
    }

    // Unlike the other operations, unionWith is an in-place
    // modification. This is to avoid a bunch of useless extra
    // copying when chaining together unions when handling Phi
    // nodes.
    void unionWith(const Range *other);
    void unionWith(RangeChangeCount *other);
    static Range intersect(const Range *lhs, const Range *rhs, bool *nullRange);
    static Range add(const Range *lhs, const Range *rhs);
    static Range sub(const Range *lhs, const Range *rhs);
    static Range mul(const Range *lhs, const Range *rhs);
    static Range and_(const Range *lhs, const Range *rhs);
    static Range shl(const Range *lhs, int32 c);
    static Range shr(const Range *lhs, int32 c);

    inline void makeLowerInfinite() {
        lower_infinite_ = true;
        lower_ = JSVAL_INT_MIN;
    }
    inline void makeUpperInfinite() {
        upper_infinite_ = true;
        upper_ = JSVAL_INT_MAX;
    }
    inline void makeRangeInfinite() {
        makeLowerInfinite();
        makeUpperInfinite();
    }

    inline bool isLowerInfinite() const {
        return lower_infinite_;
    }
    inline bool isUpperInfinite() const {
        return upper_infinite_;
    }

    inline bool isFinite() const {
        return !isLowerInfinite() && !isUpperInfinite();
    }

    inline int32 lower() const {
        return lower_;
    }

    inline int32 upper() const {
        return upper_;
    }

    inline void setLower(int64_t x) {
        if (x > JSVAL_INT_MAX) { // c.c
            lower_ = JSVAL_INT_MAX;
        } else if (x < JSVAL_INT_MIN) {
            makeLowerInfinite();
        } else {
            lower_ = (int32)x;
            lower_infinite_ = false;
        }
    }
    inline void setUpper(int64_t x) {
        if (x > JSVAL_INT_MAX) {
            makeUpperInfinite();
        } else if (x < JSVAL_INT_MIN) { // c.c
            upper_ = JSVAL_INT_MIN;
        } else {
            upper_ = (int32)x;
            upper_infinite_ = false;
        }
    }
    void set(int64_t l, int64_t h) {
        setLower(l);
        setUpper(h);
    }
};

struct RangeChangeCount {
    Range oldRange;
    unsigned char lowerCount_ : 4;
    unsigned char upperCount_ : 4;

    void updateRange(Range *newRange) {
        JS_ASSERT(newRange->lower() >= oldRange.lower());
        if (newRange->lower() != oldRange.lower())
            lowerCount_ = lowerCount_ < 15 ? lowerCount_ + 1 : lowerCount_;
        JS_ASSERT(newRange->upper() <= oldRange.upper());
        if (newRange->upper() != oldRange.upper())
            upperCount_ = upperCount_ < 15 ? upperCount_ + 1 : upperCount_;
        oldRange = *newRange;
    }
};

} // namespace ion
} // namespace js

#endif // jsion_range_analysis_h__

