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
#include "IonAnalysis.h"

namespace js {
namespace ion {

class MBasicBlock;
class MIRGraph;

// An upper bound computed on the number of backedges a loop will take.
// This count only includes backedges taken while running Ion code: for OSR
// loops, this will exclude iterations that executed in the interpreter or in
// baseline compiled code.
struct LoopIterationBound : public TempObject
{
    // Loop for which this bound applies.
    MBasicBlock *header;

    // Test from which this bound was derived. Code in the loop body which this
    // test dominates (will include the backedge) will execute at most 'bound'
    // times. Other code in the loop will execute at most '1 + Max(bound, 0)'
    // times.
    MTest *test;

    // Symbolic bound computed for the number of backedge executions.
    LinearSum sum;

    LoopIterationBound(MBasicBlock *header, MTest *test, LinearSum sum)
      : header(header), test(test), sum(sum)
    {
    }
};

// A symbolic upper or lower bound computed for a term.
struct SymbolicBound : public TempObject
{
    // Any loop iteration bound from which this was derived.
    //
    // If non-NULL, then 'sum' is only valid within the loop body, at points
    // dominated by the loop bound's test (see LoopIterationBound).
    //
    // If NULL, then 'sum' is always valid.
    LoopIterationBound *loop;

    // Computed symbolic bound, see above.
    LinearSum sum;

    SymbolicBound(LoopIterationBound *loop, LinearSum sum)
      : loop(loop), sum(sum)
    {
    }

    void print(Sprinter &sp) const;
};

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

  private:
    void analyzeLoop(MBasicBlock *header);
    LoopIterationBound *analyzeLoopIterationCount(MBasicBlock *header,
                                                  MTest *test, BranchDirection direction);
    void analyzeLoopPhi(MBasicBlock *header, LoopIterationBound *loopBound, MPhi *phi);
    bool tryHoistBoundsCheck(MBasicBlock *header, MBoundsCheck *ins);
    void markBlocksInLoopBody(MBasicBlock *header, MBasicBlock *current);
};

class Range : public TempObject {
  private:
    // Absolute ranges.
    //
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
    int32_t lower_;
    bool lower_infinite_;
    int32_t upper_;
    bool upper_infinite_;

    // Any symbolic lower or upper bound computed for this term.
    const SymbolicBound *symbolicLower_;
    const SymbolicBound *symbolicUpper_;

  public:
    Range()
        : lower_(JSVAL_INT_MIN),
          lower_infinite_(true),
          upper_(JSVAL_INT_MAX),
          upper_infinite_(true),
          symbolicLower_(NULL),
          symbolicUpper_(NULL)
    {}

    Range(int64_t l, int64_t h)
        : symbolicLower_(NULL),
          symbolicUpper_(NULL)
    {
        setLower(l);
        setUpper(h);
    }

    Range(const Range &other)
        : lower_(other.lower_),
          lower_infinite_(other.lower_infinite_),
          upper_(other.upper_),
          upper_infinite_(other.upper_infinite_),
          symbolicLower_(NULL),
          symbolicUpper_(NULL)
    {}

    static Range *Truncate(int64_t l, int64_t h);

    static int64_t abs64(int64_t x) {
#ifdef WTF_OS_WINDOWS
        return _abs64(x);
#else
        return llabs(x);
#endif
    }

    void print(Sprinter &sp) const;
    bool update(const Range *other);
    bool update(const Range &other) {
        return update(&other);
    }

    // Unlike the other operations, unionWith is an in-place
    // modification. This is to avoid a bunch of useless extra
    // copying when chaining together unions when handling Phi
    // nodes.
    void unionWith(const Range *other);
    static Range * intersect(const Range *lhs, const Range *rhs, bool *emptyRange);
    static Range * addTruncate(const Range *lhs, const Range *rhs);
    static Range * subTruncate(const Range *lhs, const Range *rhs);
    static Range * add(const Range *lhs, const Range *rhs);
    static Range * sub(const Range *lhs, const Range *rhs);
    static Range * mul(const Range *lhs, const Range *rhs);
    static Range * and_(const Range *lhs, const Range *rhs);
    static Range * shl(const Range *lhs, int32_t c);
    static Range * shr(const Range *lhs, int32_t c);

    static bool precisionLossMul(const Range *lhs, const Range *rhs);
    static bool negativeZeroMul(const Range *lhs, const Range *rhs);

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

    inline int32_t lower() const {
        return lower_;
    }

    inline int32_t upper() const {
        return upper_;
    }

    inline void setLower(int64_t x) {
        if (x > JSVAL_INT_MAX) { // c.c
            lower_ = JSVAL_INT_MAX;
        } else if (x < JSVAL_INT_MIN) {
            makeLowerInfinite();
        } else {
            lower_ = (int32_t)x;
            lower_infinite_ = false;
        }
    }
    inline void setUpper(int64_t x) {
        if (x > JSVAL_INT_MAX) {
            makeUpperInfinite();
        } else if (x < JSVAL_INT_MIN) { // c.c
            upper_ = JSVAL_INT_MIN;
        } else {
            upper_ = (int32_t)x;
            upper_infinite_ = false;
        }
    }
    void set(int64_t l, int64_t h) {
        setLower(l);
        setUpper(h);
    }

    const SymbolicBound *symbolicLower() const {
        return symbolicLower_;
    }
    const SymbolicBound *symbolicUpper() const {
        return symbolicUpper_;
    }

    void setSymbolicLower(SymbolicBound *bound) {
        symbolicLower_ = bound;
    }
    void setSymbolicUpper(SymbolicBound *bound) {
        symbolicUpper_ = bound;
    }
};

} // namespace ion
} // namespace js

#endif // jsion_range_analysis_h__

