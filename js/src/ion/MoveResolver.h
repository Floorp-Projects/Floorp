/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=79:
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   David Anderson <dvander@alliedmods.net>
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

#ifndef jsion_move_group_resolver_h__
#define jsion_move_group_resolver_h__

#include "IonRegisters.h"
#include "InlineList.h"
#include "IonAllocPolicy.h"

namespace js {
namespace ion {

class MoveResolver
{
  public:
    // This is similar to Operand, but carries more information. We're also not
    // guaranteed that Operand looks like this on all ISAs.
    class MoveOperand
    {
        enum Kind {
            REG,
            FLOAT_REG,
            ADDRESS
        };

        Kind kind_;
        uint32 code_;
        int32 disp_;

      public:
        MoveOperand()
        { }
        explicit MoveOperand(const Register &reg) : kind_(REG), code_(reg.code())
        { }
        explicit MoveOperand(const FloatRegister &reg) : kind_(FLOAT_REG), code_(reg.code())
        { }
        MoveOperand(const Register &reg, int32 disp)
          : kind_(ADDRESS),
            code_(reg.code()),
            disp_(disp)
        { }
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
        bool isDouble() const {
            return kind_ == FLOAT_REG;
        }
        bool isMemory() const {
            return kind_ == ADDRESS;
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
            JS_ASSERT(isMemory());
            return Register::FromCode(code_);
        }
        int32 disp() const {
            return disp_;
        }

        bool operator ==(const MoveOperand &other) const {
            if (kind_ != other.kind_)
                return false;
            if (code_ != other.code_)
                return false;
            if (isMemory())
                return disp_ == other.disp_;
            return true;
        }
    };

    class Move
    {
      protected:
        MoveOperand from_;
        MoveOperand to_;
        bool cycle_;

      public:
        enum Kind {
            GENERAL,
            DOUBLE
        };

      protected:
        Kind kind_;

      public:
        Move()
        { }
        Move(const Move &other)
          : from_(other.from_),
            to_(other.to_),
            cycle_(other.cycle_),
            kind_(other.kind_)
        { }
        Move(const MoveOperand &from, const MoveOperand &to, Kind kind, bool cycle = false)
          : from_(from),
            to_(to),
            cycle_(cycle),
            kind_(kind)
        { }

        bool inCycle() const {
            return cycle_;
        }
        const MoveOperand &from() const {
            return from_;
        }
        const MoveOperand &to() const {
            return to_;
        }
        Kind kind() const {
            return kind_;
        }
    };

  private:
    struct PendingMove
      : public Move,
        public TempObject,
        public InlineListNode<PendingMove>
    {
        PendingMove()
        { }
        PendingMove(const MoveOperand &from, const MoveOperand &to, Kind kind)
          : Move(from, to, kind, false)
        { }
        
        void setInCycle() {
            JS_ASSERT(!inCycle());
            cycle_ = true;
        }

    };

    typedef InlineList<MoveResolver::PendingMove>::iterator PendingMoveIterator;

  private:
    // Moves that are definitely unblocked (constants to registers). These are
    // emitted last.
    js::Vector<Move, 16, SystemAllocPolicy> orderedMoves_;
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
    bool addMove(const MoveOperand &from, const MoveOperand &to, Move::Kind kind);
    bool resolve();

    size_t numMoves() const {
        return orderedMoves_.length();
    }
    const Move &getMove(size_t i) const {
        return orderedMoves_[i];
    }
    bool hasCycles() const {
        return hasCycles_;
    }
};

} // namespace ion
} // namespace js

#endif // jsion_move_group_resolver_h__

