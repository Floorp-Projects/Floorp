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
#include "MoveGroupResolver.h"

using namespace js;
using namespace js::ion;

MoveGroupResolver::MoveGroupResolver()
  : hasCycles_(false)
{
}

typedef InlineList<MoveGroupResolver::PendingMove>::iterator PendingMoveIterator;

bool
MoveGroupResolver::buildWorklist(LMoveGroup *group)
{
    // Add the moves to an initial worklist.
    for (size_t i = 0; i < group->numMoves(); i++) {
        const LMove &move = group->getMove(i);

        // No bogus moves.
        JS_ASSERT(*move.from() != *move.to());

        // Ignore constants, they're always unblocked.
        if (move.from()->isConstant()) {
            if (!unorderedMoves_.append(Move(&move)))
                return false;
            continue;
        }

        PendingMove *pm = movePool_.allocate();
        new (pm) PendingMove(&move);
        pending_.insert(pm);
    }

    return true;
}

// Given move (A -> B), this function attempts to find any move (B -> *) in the
// pending move list, and returns the first one.
MoveGroupResolver::PendingMove *
MoveGroupResolver::findBlockingMove(const PendingMove *last)
{
    for (PendingMoveIterator iter = pending_.begin(); iter != pending_.end(); iter++) {
        PendingMove *other = *iter;

        if (*other->move->from() == *last->move->to()) {
            // We now have pairs in the form (A -> X) (X -> y). The second pair
            // blocks the move in the first pair, so return it.
            return other;
        }
    }

    // No blocking moves found.
    return NULL;
}

bool
MoveGroupResolver::resolve(LMoveGroup *group)
{
    if (!buildWorklist(group))
        return false;

    unorderedMoves_.clear();
    orderedMoves_.clear();

    InlineList<PendingMove> stack;

    // This is a depth-first-search without recursion, which tries to find
    // cycles in a list of moves. The output is not entirely optimal for cases
    // where a source has multiple destinations, i.e.:
    //      [stack0] -> A
    //      [stack0] -> B
    //
    // These moves may not occur next to each other in the list, making it
    // harder for the emitter to optimize memory to memory traffic. However, we
    // expect duplicate sources to be rare in greedy allocation, and indicative
    // of an error in LSRA.
    //
    // Algorithm.
    //
    // S = Traversal stack.
    // P = Pending move list.
    // O = Ordered list of moves.
    //
    // As long as there are pending moves in P:
    //      Let |root| be any pending move removed from P
    //      Add |root| to the traversal stack.
    //      As long as S is not empty:
    //          Let |L| be the most recent move added to S.
    //
    //          Find any pending move M whose source is L's destination, thus
    //          preventing L's move until M has completed.
    //
    //          If a move M was found,
    //              Remove M from the pending list.
    //              If M's destination is |root|,
    //                  Annotate M and |root| as cycles.
    //                  Add M to O.
    //              Otherwise,
    //                  Add M to S.
    //         Otherwise,
    //              Remove L from S.
    //              Add L to O.
    //
    while (!pending_.empty()) {
        PendingMove *pm = pending_.pop();

        // Add this pending move to the cycle detection stack.
        stack.insert(pm);

        while (!stack.empty()) {
            PendingMove *blocking = findBlockingMove(stack.peekBack());

            if (blocking) {
                if (*blocking->move->to() == *pm->move->from()) {
                    // We annotate cycles at each move in the cycle, and
                    // assert that we do not find two cycles in one move chain
                    // traversal (which would indicate two moves to the same
                    // destination).
                    JS_ASSERT(!pm->cycle);
                    if (!orderedMoves_.append(Move(blocking->move, true)));
                        return true;
                    pm->cycle = true;
                    hasCycles_ = true;
                    pending_.remove(blocking);
                    movePool_.free(blocking);
                } else {
                    // This is a new link in the move chain, so keep
                    // searching for a cycle.
                    pending_.remove(blocking);
                    stack.insert(blocking);
                }
            } else {
                // Otherwise, pop the last move on the search stack because it's
                // complete and not participating in a cycle. The resulting
                // move can safely be added to the ordered move list.
                PendingMove *done = stack.pop();
                if (!orderedMoves_.append(Move(done->move, done->cycle)))
                    return false;
                movePool_.free(done);
            }
        }
    }

    return true;
}
