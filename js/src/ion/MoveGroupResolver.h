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

#include "IonLIR.h"

namespace js {
namespace ion {

class MoveGroupResolver
{
  public:
    class Move
    {
        const LMove *move_;
        bool cycle_;

      public:
        Move()
        { }
        Move(const LMove *move, bool cycle = false)
          : move_(move),
            cycle_(cycle)
        { }

        bool inCycle() const {
            return cycle_;
        }
        const LAllocation *from() const {
            return move_->from();
        }
        const LAllocation *to() const {
            return move_->to();
        }
    };

  private:
    struct PendingMove
      : public TempObject,
        public InlineListNode<PendingMove>
    {
        const LMove *move;
        bool cycle;

        PendingMove()
        { }

        PendingMove(const LMove *move)
          : move(move),
            cycle(false)
        { }
    };

    typedef InlineList<MoveGroupResolver::PendingMove>::iterator PendingMoveIterator;

  private:
    // Moves that are definitely unblocked (constants to registers). These are
    // emitted last.
    js::Vector<Move, 16, SystemAllocPolicy> unorderedMoves_;
    js::Vector<Move, 16, SystemAllocPolicy> orderedMoves_;
    bool hasCycles_;

    TempObjectPool<PendingMove> movePool_;

    InlineList<PendingMove> pending_;

    bool buildWorklist(LMoveGroup *group);
    PendingMove *findBlockingMove(const PendingMove *last);

  public:
    MoveGroupResolver();

    // Resolves a move group into two lists of ordered moves. These moves must
    // be executed in the order provided. Some moves may indicate that they
    // participate in a cycle. For every cycle there are two such moves, and it
    // is guaranteed that cycles do not nest inside each other in the list.
    bool resolve(LMoveGroup *group);

    size_t numMoves() const {
        return orderedMoves_.length() + unorderedMoves_.length();
    }

    const Move &getMove(size_t i) const {
        if (i < orderedMoves_.length())
            return orderedMoves_[i];
        return unorderedMoves_[i - orderedMoves_.length()];
    }
    bool hasCycles() const {
        return hasCycles_;
    }
};

} // namespace ion
} // namespace js

#endif // jsion_move_group_resolver_h__

