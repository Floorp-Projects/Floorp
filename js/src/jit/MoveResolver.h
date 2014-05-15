/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_MoveResolver_h
#define jit_MoveResolver_h

#include "jit/InlineList.h"
#include "jit/IonAllocPolicy.h"
#include "jit/Registers.h"

namespace js {
namespace jit {

// This is similar to Operand, but carries more information. We're also not
// guaranteed that Operand looks like this on all ISAs.
class MoveOperand
{
  public:
    enum Kind {
        // A register in the "integer", aka "general purpose", class.
        REG,
        // A register in the "float" register class.
        FLOAT_REG,
        // A memory region.
        MEMORY,
        // The address of a memory region.
        EFFECTIVE_ADDRESS
    };

  private:
    Kind kind_;
    uint32_t code_;
    int32_t disp_;

  public:
    MoveOperand()
    { }
    explicit MoveOperand(Register reg) : kind_(REG), code_(reg.code())
    { }
    explicit MoveOperand(FloatRegister reg) : kind_(FLOAT_REG), code_(reg.code())
    { }
    MoveOperand(Register reg, int32_t disp, Kind kind = MEMORY)
        : kind_(kind),
        code_(reg.code()),
        disp_(disp)
    {
        JS_ASSERT(isMemoryOrEffectiveAddress());

        // With a zero offset, this is a plain reg-to-reg move.
        if (disp == 0 && kind_ == EFFECTIVE_ADDRESS)
            kind_ = REG;
    }
    MoveOperand(const MoveOperand &other)
      : kind_(other.kind_),
        code_(other.code_),
        disp_(other.disp_)
    { }
    bool isFloatReg() const {
        return kind_ == FLOAT_REG;
    }
    bool isGeneralReg() const {
        return kind_ == REG;
    }
    bool isMemory() const {
        return kind_ == MEMORY;
    }
    bool isEffectiveAddress() const {
        return kind_ == EFFECTIVE_ADDRESS;
    }
    bool isMemoryOrEffectiveAddress() const {
        return isMemory() || isEffectiveAddress();
    }
    Register reg() const {
        JS_ASSERT(isGeneralReg());
        return Register::FromCode(code_);
    }
    FloatRegister floatReg() const {
        JS_ASSERT(isFloatReg());
        return FloatRegister::FromCode(code_);
    }
    Register base() const {
        JS_ASSERT(isMemoryOrEffectiveAddress());
        return Register::FromCode(code_);
    }
    int32_t disp() const {
        JS_ASSERT(isMemoryOrEffectiveAddress());
        return disp_;
    }

    bool operator ==(const MoveOperand &other) const {
        if (kind_ != other.kind_)
            return false;
        if (code_ != other.code_)
            return false;
        if (isMemoryOrEffectiveAddress())
            return disp_ == other.disp_;
        return true;
    }
    bool operator !=(const MoveOperand &other) const {
        return !operator==(other);
    }
};

// This represents a move operation.
class MoveOp
{
  protected:
    MoveOperand from_;
    MoveOperand to_;
    bool cycleBegin_;
    bool cycleEnd_;

  public:
    enum Type {
        GENERAL,
        INT32,
        FLOAT32,
        DOUBLE
    };

  protected:
    Type type_;

    // If cycleBegin_ is true, endCycleType_ is the type of the move at the end
    // of the cycle. For example, given these moves:
    //       INT32 move a -> b
    //     GENERAL move b -> a
    // the move resolver starts by copying b into a temporary location, so that
    // the last move can read it. This copy needs to use use type GENERAL.
    Type endCycleType_;

  public:
    MoveOp()
    { }
    MoveOp(const MoveOperand &from, const MoveOperand &to, Type type)
      : from_(from),
        to_(to),
        cycleBegin_(false),
        cycleEnd_(false),
        type_(type)
    { }

    bool isCycleBegin() const {
        return cycleBegin_;
    }
    bool isCycleEnd() const {
        return cycleEnd_;
    }
    const MoveOperand &from() const {
        return from_;
    }
    const MoveOperand &to() const {
        return to_;
    }
    Type type() const {
        return type_;
    }
    Type endCycleType() const {
        JS_ASSERT(isCycleBegin());
        return endCycleType_;
    }
};

class MoveResolver
{
  private:
    struct PendingMove
      : public MoveOp,
        public TempObject,
        public InlineListNode<PendingMove>
    {
        PendingMove()
        { }
        PendingMove(const MoveOperand &from, const MoveOperand &to, Type type)
          : MoveOp(from, to, type)
        { }

        void setCycleBegin(Type endCycleType) {
            JS_ASSERT(!isCycleBegin() && !isCycleEnd());
            cycleBegin_ = true;
            endCycleType_ = endCycleType;
        }
        void setCycleEnd() {
            JS_ASSERT(!isCycleBegin() && !isCycleEnd());
            cycleEnd_ = true;
        }
    };

    typedef InlineList<MoveResolver::PendingMove>::iterator PendingMoveIterator;

  private:
    // Moves that are definitely unblocked (constants to registers). These are
    // emitted last.
    js::Vector<MoveOp, 16, SystemAllocPolicy> orderedMoves_;
    bool hasCycles_;

    TempObjectPool<PendingMove> movePool_;

    InlineList<PendingMove> pending_;

    PendingMove *findBlockingMove(const PendingMove *last);

    // Internal reset function. Does not clear lists.
    void resetState();

  public:
    MoveResolver();

    // Resolves a move group into two lists of ordered moves. These moves must
    // be executed in the order provided. Some moves may indicate that they
    // participate in a cycle. For every cycle there are two such moves, and it
    // is guaranteed that cycles do not nest inside each other in the list.
    //
    // After calling addMove() for each parallel move, resolve() performs the
    // cycle resolution algorithm. Calling addMove() again resets the resolver.
    bool addMove(const MoveOperand &from, const MoveOperand &to, MoveOp::Type type);
    bool resolve();

    size_t numMoves() const {
        return orderedMoves_.length();
    }
    const MoveOp &getMove(size_t i) const {
        return orderedMoves_[i];
    }
    bool hasCycles() const {
        return hasCycles_;
    }
    void clearTempObjectPool() {
        movePool_.clear();
    }
    void setAllocator(TempAllocator &alloc) {
        movePool_.setAllocator(alloc);
    }
};

} // namespace jit
} // namespace js

#endif /* jit_MoveResolver_h */
