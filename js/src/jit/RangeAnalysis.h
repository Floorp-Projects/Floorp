/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_RangeAnalysis_h
#define jit_RangeAnalysis_h

#include "mozilla/FloatingPoint.h"
#include "mozilla/MathAlgorithms.h"

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
    // If non-nullptr, then 'sum' is only valid within the loop body, at
    // points dominated by the loop bound's test (see LoopIterationBound).
    //
    // If nullptr, then 'sum' is always valid.
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
    bool analyzeLoop(MBasicBlock *header);
    LoopIterationBound *analyzeLoopIterationCount(MBasicBlock *header,
                                                  MTest *test, BranchDirection direction);
    void analyzeLoopPhi(MBasicBlock *header, LoopIterationBound *loopBound, MPhi *phi);
    bool tryHoistBoundsCheck(MBasicBlock *header, MBoundsCheck *ins);
    bool markBlocksInLoopBody(MBasicBlock *header, MBasicBlock *current);
};

class Range : public TempObject {
  public:
    // Int32 are signed, so the value needs 31 bits except for INT_MIN value
    // which needs 32 bits.
    static const uint16_t MaxInt32Exponent = 31;

    // UInt32 are unsigned. UINT32_MAX is pow(2,32)-1, so it's the greatest
    // value that has an exponent of 31.
    static const uint16_t MaxUInt32Exponent = 31;

    // Maximal exponenent under which we have no precission loss on double
    // operations. Double has 52 bits of mantissa, so 2^52+1 cannot be
    // represented without loss.
    static const uint16_t MaxTruncatableExponent = mozilla::DoubleExponentShift;

    // 11 bits of signed exponent, so the max is encoded on 10 bits.
    static const uint16_t MaxDoubleExponent = mozilla::DoubleExponentBias;

    // This range class uses int32_t ranges, but has several interfaces which
    // use int64_t, which either holds an int32_t value, or one of the following
    // special values which mean a value which is beyond the int32 range,
    // potentially including infinity or NaN. These special values are
    // guaranteed to compare greater, and less than, respectively, any int32_t
    // value.
    static const int64_t NoInt32UpperBound = int64_t(JSVAL_INT_MAX) + 1;
    static const int64_t NoInt32LowerBound = int64_t(JSVAL_INT_MIN) - 1;

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
    // on existing ranges will ignore the hasInt32*Bound_ flags of the
    // input ranges; that is, they implicitly clamp the ranges of
    // the inputs to [INT_MIN, INT_MAX]. Therefore, while our range might
    // be unbounded (and could overflow), when using this information to
    // propagate through other ranges, we disregard this fact; if that code
    // executes, then the overflow did not occur, so we may safely assume
    // that the range is [INT_MIN, INT_MAX] instead.
    //
    // To facilitate this trick, we maintain the invariants that:
    // 1) hasInt32LowerBound_ == false implies lower_ == JSVAL_INT_MIN
    // 2) hasInt32UpperBound_ == false implies upper_ == JSVAL_INT_MAX
    //
    // As a second and less precise range analysis, we represent the maximal
    // exponent taken by a value. The exponent is calculated by taking the
    // absolute value and looking at the position of the highest bit.  All
    // exponent computation have to be over-estimations of the actual result. On
    // the Int32 this over approximation is rectified.

    int32_t lower_;
    bool hasInt32LowerBound_;

    int32_t upper_;
    bool hasInt32UpperBound_;

    bool canHaveFractionalPart_;
    uint16_t max_exponent_;

    // Any symbolic lower or upper bound computed for this term.
    const SymbolicBound *symbolicLower_;
    const SymbolicBound *symbolicUpper_;

    void setLowerInit(int64_t x) {
        if (x > JSVAL_INT_MAX) {
            lower_ = JSVAL_INT_MAX;
            hasInt32LowerBound_ = true;
        } else if (x < JSVAL_INT_MIN) {
            dropInt32LowerBound();
        } else {
            lower_ = int32_t(x);
            hasInt32LowerBound_ = true;
        }
    }
    void setUpperInit(int64_t x) {
        if (x > JSVAL_INT_MAX) {
            dropInt32UpperBound();
        } else if (x < JSVAL_INT_MIN) {
            upper_ = JSVAL_INT_MIN;
            hasInt32UpperBound_ = true;
        } else {
            upper_ = int32_t(x);
            hasInt32UpperBound_ = true;
        }
    }

  public:
    Range()
        : lower_(JSVAL_INT_MIN),
          hasInt32LowerBound_(false),
          upper_(JSVAL_INT_MAX),
          hasInt32UpperBound_(false),
          canHaveFractionalPart_(true),
          max_exponent_(MaxDoubleExponent),
          symbolicLower_(nullptr),
          symbolicUpper_(nullptr)
    {
        JS_ASSERT_IF(!hasInt32LowerBound_, lower_ == JSVAL_INT_MIN);
        JS_ASSERT_IF(!hasInt32UpperBound_, upper_ == JSVAL_INT_MAX);
    }

    Range(int64_t l, int64_t h, bool f = false, uint16_t e = MaxInt32Exponent)
        : hasInt32LowerBound_(false),
          hasInt32UpperBound_(false),
          canHaveFractionalPart_(f),
          max_exponent_(e),
          symbolicLower_(nullptr),
          symbolicUpper_(nullptr)
    {
        JS_ASSERT(e >= (h == INT64_MIN ? MaxDoubleExponent : mozilla::FloorLog2(mozilla::Abs(h))));
        JS_ASSERT(e >= (l == INT64_MIN ? MaxDoubleExponent : mozilla::FloorLog2(mozilla::Abs(l))));

        setLowerInit(l);
        setUpperInit(h);
        rectifyExponent();
        JS_ASSERT_IF(!hasInt32LowerBound_, lower_ == JSVAL_INT_MIN);
        JS_ASSERT_IF(!hasInt32UpperBound_, upper_ == JSVAL_INT_MAX);
    }

    Range(const Range &other)
        : lower_(other.lower_),
          hasInt32LowerBound_(other.hasInt32LowerBound_),
          upper_(other.upper_),
          hasInt32UpperBound_(other.hasInt32UpperBound_),
          canHaveFractionalPart_(other.canHaveFractionalPart_),
          max_exponent_(other.max_exponent_),
          symbolicLower_(nullptr),
          symbolicUpper_(nullptr)
    {
        JS_ASSERT_IF(!hasInt32LowerBound_, lower_ == JSVAL_INT_MIN);
        JS_ASSERT_IF(!hasInt32UpperBound_, upper_ == JSVAL_INT_MAX);
    }

    Range(const MDefinition *def);

    static Range *NewInt32Range(int32_t l, int32_t h) {
        return new Range(l, h, false, MaxInt32Exponent);
    }

    static Range *NewUInt32Range(uint32_t l, uint32_t h) {
        // For now, just pass them to the constructor as int64_t values.
        // They'll become unbounded if they're not in the int32_t range.
        return new Range(l, h, false, MaxUInt32Exponent);
    }

    static Range *NewDoubleRange(int64_t l, int64_t h, uint16_t e = MaxDoubleExponent) {
        return new Range(l, h, true, e);
    }

    static Range *NewSingleValueRange(int64_t v) {
        return new Range(v, v, false, MaxDoubleExponent);
    }

    void print(Sprinter &sp) const;
    bool update(const Range *other);

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

    void dropInt32LowerBound() {
        hasInt32LowerBound_ = false;
        lower_ = JSVAL_INT_MIN;
        if (max_exponent_ < MaxInt32Exponent)
            max_exponent_ = MaxInt32Exponent;
    }
    void dropInt32UpperBound() {
        hasInt32UpperBound_ = false;
        upper_ = JSVAL_INT_MAX;
        if (max_exponent_ < MaxInt32Exponent)
            max_exponent_ = MaxInt32Exponent;
    }

    bool hasInt32LowerBound() const {
        return hasInt32LowerBound_;
    }
    bool hasInt32UpperBound() const {
        return hasInt32UpperBound_;
    }

    // Test whether the value is known to be within [INT32_MIN,INT32_MAX].
    // Note that this does not necessarily mean the value is an integer.
    bool hasInt32Bounds() const {
        return hasInt32LowerBound() && hasInt32UpperBound();
    }

    // Test whether the value is known to be representable as an int32.
    bool isInt32() const {
        return hasInt32Bounds() && !canHaveFractionalPart();
    }

    // Test whether the given value is known to be either 0 or 1.
    bool isBoolean() const {
        return lower() >= 0 && upper() <= 1 && !canHaveFractionalPart();
    }

    bool canHaveRoundingErrors() const {
        return canHaveFractionalPart() || exponent() >= MaxTruncatableExponent;
    }

    bool canBeInfiniteOrNaN() const {
        return exponent() >= MaxDoubleExponent;
    }

    bool canHaveFractionalPart() const {
        return canHaveFractionalPart_;
    }

    uint16_t exponent() const {
        return max_exponent_;
    }

    uint16_t numBits() const {
        return max_exponent_ + 1; // 2^0 -> 1
    }

    int32_t lower() const {
        return lower_;
    }

    int32_t upper() const {
        return upper_;
    }

    void setLower(int64_t x) {
        setLowerInit(x);
        rectifyExponent();
        JS_ASSERT_IF(!hasInt32LowerBound_, lower_ == JSVAL_INT_MIN);
    }
    void setUpper(int64_t x) {
        setUpperInit(x);
        rectifyExponent();
        JS_ASSERT_IF(!hasInt32UpperBound_, upper_ == JSVAL_INT_MAX);
    }

    void setInt32() {
        hasInt32LowerBound_ = true;
        hasInt32UpperBound_ = true;
        canHaveFractionalPart_ = false;
        max_exponent_ = MaxInt32Exponent;
    }

    void set(int64_t l, int64_t h, bool f = false, uint16_t e = MaxInt32Exponent) {
        max_exponent_ = e;
        setLowerInit(l);
        setUpperInit(h);
        canHaveFractionalPart_ = f;
        rectifyExponent();
        JS_ASSERT_IF(!hasInt32LowerBound_, lower_ == JSVAL_INT_MIN);
        JS_ASSERT_IF(!hasInt32UpperBound_, upper_ == JSVAL_INT_MAX);
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
        JS_ASSERT(!hasInt32UpperBound());
        lower_ = JSVAL_INT_MIN;
    }

    // Set the exponent by using the precise range analysis on the full
    // range of Int32 values. This might shrink the exponent after some
    // operations.
    //
    // Note:
    //     exponent of JSVAL_INT_MIN == 32
    //     exponent of JSVAL_INT_MAX == 31
    void rectifyExponent() {
        if (!hasInt32Bounds()) {
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

    void setSymbolicLower(SymbolicBound *bound) {
        symbolicLower_ = bound;
    }
    void setSymbolicUpper(SymbolicBound *bound) {
        symbolicUpper_ = bound;
    }
};

} // namespace jit
} // namespace js

#endif /* jit_RangeAnalysis_h */
