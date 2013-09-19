/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_RangeAnalysis_h
#define jit_RangeAnalysis_h

#include "mozilla/FloatingPoint.h"
#include "mozilla/MathAlgorithms.h"

#include "jit/CompileInfo.h"
#include "jit/IonAnalysis.h"
#include "jit/MIR.h"

namespace js {
namespace jit {

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
    MIRGenerator *mir;
    MIRGraph &graph_;

  public:
    MOZ_CONSTEXPR RangeAnalysis(MIRGenerator *mir, MIRGraph &graph) :
        mir(mir), graph_(graph) {}
    bool addBetaNodes();
    bool analyze();
    bool addRangeAssertions();
    bool removeBetaNodes();
    bool truncate();

  private:
    void analyzeLoop(MBasicBlock *header);
    LoopIterationBound *analyzeLoopIterationCount(MBasicBlock *header,
                                                  MTest *test, BranchDirection direction);
    void analyzeLoopPhi(MBasicBlock *header, LoopIterationBound *loopBound, MPhi *phi);
    bool tryHoistBoundsCheck(MBasicBlock *header, MBoundsCheck *ins);
    void markBlocksInLoopBody(MBasicBlock *header, MBasicBlock *current);
};

class Range : public TempObject {
  public:
    // Int32 are signed, so the value needs 31 bits except for INT_MIN value
    // which needs 32 bits.
    static const uint16_t MaxInt32Exponent = 31;

    // Maximal exponenent under which we have no precission loss on double
    // operations. Double has 52 bits of mantissa, so 2^52+1 cannot be
    // represented without loss.
    static const uint16_t MaxTruncatableExponent = mozilla::DoubleExponentShift;

    // 11 bits of signed exponent, so the max is encoded on 10 bits.
    static const uint16_t MaxDoubleExponent = mozilla::DoubleExponentBias;

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
    //
    // As a second and less precise range analysis, we represent the maximal
    // exponent taken by a value. The exponent is calculated by taking the
    // absolute value and looking at the position of the highest bit.  All
    // exponent computation have to be over-estimations of the actual result. On
    // the Int32 this over approximation is rectified.

    int32_t lower_;
    bool lower_infinite_;

    int32_t upper_;
    bool upper_infinite_;

    bool canHaveFractionalPart_;
    uint16_t max_exponent_;

    // Any symbolic lower or upper bound computed for this term.
    const SymbolicBound *symbolicLower_;
    const SymbolicBound *symbolicUpper_;

  public:
    Range()
        : lower_(JSVAL_INT_MIN),
          lower_infinite_(true),
          upper_(JSVAL_INT_MAX),
          upper_infinite_(true),
          canHaveFractionalPart_(true),
          max_exponent_(MaxDoubleExponent),
          symbolicLower_(NULL),
          symbolicUpper_(NULL)
    {
        JS_ASSERT_IF(lower_infinite_, lower_ == JSVAL_INT_MIN);
        JS_ASSERT_IF(upper_infinite_, upper_ == JSVAL_INT_MAX);
    }

    Range(int64_t l, int64_t h, bool f = false, uint16_t e = MaxInt32Exponent)
        : lower_infinite_(true),
          upper_infinite_(true),
          canHaveFractionalPart_(f),
          max_exponent_(e),
          symbolicLower_(NULL),
          symbolicUpper_(NULL)
    {
        JS_ASSERT(e >= (h == INT64_MIN ? MaxDoubleExponent : mozilla::FloorLog2(mozilla::Abs(h))));
        JS_ASSERT(e >= (l == INT64_MIN ? MaxDoubleExponent : mozilla::FloorLog2(mozilla::Abs(l))));

        setLowerInit(l);
        setUpperInit(h);
        rectifyExponent();
        JS_ASSERT_IF(lower_infinite_, lower_ == JSVAL_INT_MIN);
        JS_ASSERT_IF(upper_infinite_, upper_ == JSVAL_INT_MAX);
    }

    Range(const Range &other)
        : lower_(other.lower_),
          lower_infinite_(other.lower_infinite_),
          upper_(other.upper_),
          upper_infinite_(other.upper_infinite_),
          canHaveFractionalPart_(other.canHaveFractionalPart_),
          max_exponent_(other.max_exponent_),
          symbolicLower_(NULL),
          symbolicUpper_(NULL)
    {
        JS_ASSERT_IF(lower_infinite_, lower_ == JSVAL_INT_MIN);
        JS_ASSERT_IF(upper_infinite_, upper_ == JSVAL_INT_MAX);
    }

    Range(const MDefinition *def);

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
    static Range * add(const Range *lhs, const Range *rhs);
    static Range * sub(const Range *lhs, const Range *rhs);
    static Range * mul(const Range *lhs, const Range *rhs);
    static Range * and_(const Range *lhs, const Range *rhs);
    static Range * or_(const Range *lhs, const Range *rhs);
    static Range * xor_(const Range *lhs, const Range *rhs);
    static Range * not_(const Range *op);
    static Range * lsh(const Range *lhs, int32_t c);
    static Range * rsh(const Range *lhs, int32_t c);
    static Range * ursh(const Range *lhs, int32_t c);
    static Range * lsh(const Range *lhs, const Range *rhs);
    static Range * rsh(const Range *lhs, const Range *rhs);
    static Range * ursh(const Range *lhs, const Range *rhs);
    static Range * abs(const Range *op);
    static Range * min(const Range *lhs, const Range *rhs);
    static Range * max(const Range *lhs, const Range *rhs);

    static bool negativeZeroMul(const Range *lhs, const Range *rhs);

    inline void makeLowerInfinite() {
        lower_infinite_ = true;
        lower_ = JSVAL_INT_MIN;
        if (max_exponent_ < MaxInt32Exponent)
            max_exponent_ = MaxInt32Exponent;
    }
    inline void makeUpperInfinite() {
        upper_infinite_ = true;
        upper_ = JSVAL_INT_MAX;
        if (max_exponent_ < MaxInt32Exponent)
            max_exponent_ = MaxInt32Exponent;
    }
    inline void makeRangeInfinite() {
        makeLowerInfinite();
        makeUpperInfinite();
        max_exponent_ = MaxDoubleExponent;
    }

    inline bool isLowerInfinite() const {
        return lower_infinite_;
    }
    inline bool isUpperInfinite() const {
        return upper_infinite_;
    }

    inline bool isInt32() const {
        return !isLowerInfinite() && !isUpperInfinite();
    }
    inline bool isBoolean() const {
        return lower() >= 0 && upper() <= 1;
    }

    inline bool hasRoundingErrors() const {
        return canHaveFractionalPart() || exponent() >= MaxTruncatableExponent;
    }

    inline bool isInfinite() const {
        return exponent() >= MaxDoubleExponent;
    }

    inline bool canHaveFractionalPart() const {
        return canHaveFractionalPart_;
    }

    inline uint16_t exponent() const {
        return max_exponent_;
    }

    inline uint16_t numBits() const {
        return max_exponent_ + 1; // 2^0 -> 1
    }

    inline int32_t lower() const {
        return lower_;
    }

    inline int32_t upper() const {
        return upper_;
    }

    inline void setLowerInit(int64_t x) {
        if (x > JSVAL_INT_MAX) { // c.c
            lower_ = JSVAL_INT_MAX;
            lower_infinite_ = false;
        } else if (x < JSVAL_INT_MIN) {
            makeLowerInfinite();
        } else {
            lower_ = (int32_t)x;
            lower_infinite_ = false;
        }
    }
    inline void setLower(int64_t x) {
        setLowerInit(x);
        rectifyExponent();
        JS_ASSERT_IF(lower_infinite_, lower_ == JSVAL_INT_MIN);
    }
    inline void setUpperInit(int64_t x) {
        if (x > JSVAL_INT_MAX) {
            makeUpperInfinite();
        } else if (x < JSVAL_INT_MIN) { // c.c
            upper_ = JSVAL_INT_MIN;
            upper_infinite_ = false;
        } else {
            upper_ = (int32_t)x;
            upper_infinite_ = false;
        }
    }
    inline void setUpper(int64_t x) {
        setUpperInit(x);
        rectifyExponent();
        JS_ASSERT_IF(upper_infinite_, upper_ == JSVAL_INT_MAX);
    }

    inline void setInt32() {
        lower_infinite_ = false;
        upper_infinite_ = false;
        canHaveFractionalPart_ = false;
        max_exponent_ = MaxInt32Exponent;
    }

    inline void set(int64_t l, int64_t h, bool f = false, uint16_t e = MaxInt32Exponent) {
        max_exponent_ = e;
        setLowerInit(l);
        setUpperInit(h);
        canHaveFractionalPart_ = f;
        rectifyExponent();
        JS_ASSERT_IF(lower_infinite_, lower_ == JSVAL_INT_MIN);
        JS_ASSERT_IF(upper_infinite_, upper_ == JSVAL_INT_MAX);
    }

    // Make the lower end of this range at least INT32_MIN, and make
    // the upper end of this range at most INT32_MAX.
    void clampToInt32();

    // If this range exceeds int32_t range, at either or both ends, change
    // it to int32_t range.  Otherwise do nothing.
    void wrapAroundToInt32();

    // If this range exceeds [0, 32) range, at either or both ends, change
    // it to the [0, 32) range.  Otherwise do nothing.
    void wrapAroundToShiftCount();

    // If this range exceeds [0, 1] range, at either or both ends, change
    // it to the [0, 1] range.  Otherwise do nothing.
    void wrapAroundToBoolean();

    // As we lack support of MIRType_UInt32, we need to work around the int32
    // representation by doing an overflow while keeping the upper infinity to
    // repesent the fact that the value might reach bigger numbers.
    void extendUInt32ToInt32Min() {
        JS_ASSERT(isUpperInfinite());
        lower_ = JSVAL_INT_MIN;
    }

    // Set the exponent by using the precise range analysis on the full
    // range of Int32 values. This might shrink the exponent after some
    // operations.
    //
    // Note:
    //     exponent of JSVAL_INT_MIN == 32
    //     exponent of JSVAL_INT_MAX == 31
    inline void rectifyExponent() {
        if (!isInt32()) {
            JS_ASSERT(max_exponent_ >= MaxInt32Exponent);
            return;
        }

        uint32_t max = Max(mozilla::Abs<int64_t>(lower()), mozilla::Abs<int64_t>(upper()));
        JS_ASSERT_IF(lower() == JSVAL_INT_MIN, max == (uint32_t) JSVAL_INT_MIN);
        JS_ASSERT(max <= (uint32_t) JSVAL_INT_MIN);
        // The number of bits needed to encode |max| is the power of 2 plus one.
        max_exponent_ = max ? mozilla::FloorLog2Size(max) : max;
    }

    const SymbolicBound *symbolicLower() const {
        return symbolicLower_;
    }
    const SymbolicBound *symbolicUpper() const {
        return symbolicUpper_;
    }

    inline void setSymbolicLower(SymbolicBound *bound) {
        symbolicLower_ = bound;
    }
    inline void setSymbolicUpper(SymbolicBound *bound) {
        symbolicUpper_ = bound;
    }
};

} // namespace jit
} // namespace js

#endif /* jit_RangeAnalysis_h */
