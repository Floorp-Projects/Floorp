/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/MoveResolver.h"

using namespace js;
using namespace js::jit;

MoveResolver::MoveResolver()
  : hasCycles_(false)
{
}

void
MoveResolver::resetState()
{
    hasCycles_ = false;
}

bool
MoveResolver::addMove(const MoveOperand &from, const MoveOperand &to, MoveOp::Type type)
{
    // Assert that we're not doing no-op moves.
    JS_ASSERT(!(from == to));
    PendingMove *pm = movePool_.allocate();
    if (!pm)
        return false;
    new (pm) PendingMove(from, to, type);
    pending_.pushBack(pm);
    return true;
}

// Given move (A -> B), this function attempts to find any move (B -> *) in the
// pending move list, and returns the first one.
MoveResolver::PendingMove *
MoveResolver::findBlockingMove(const PendingMove *last)
{
    for (PendingMoveIterator iter = pending_.begin(); iter != pending_.end(); iter++) {
        PendingMove *other = *iter;

        if (other->from() == last->to()) {
            // We now have pairs in the form (A -> X) (X -> y). The second pair
            // blocks the move in the first pair, so return it.
            return other;
        }
    }

    // No blocking moves found.
    return nullptr;
}

bool
MoveResolver::resolve()
{
    resetState();
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
    //                  Add M to S.
    //                  do not Add M to O, since M may have other conflictors in P that have not yet been processed.
    //              Otherwise,
    //                  Add M to S.
    //         Otherwise,
    //              Remove L from S.
    //              Add L to O.
    //
    while (!pending_.empty()) {
        PendingMove *pm = pending_.popBack();

        // Add this pending move to the cycle detection stack.
        stack.pushBack(pm);

        while (!stack.empty()) {
            PendingMove *blocking = findBlockingMove(stack.peekBack());

            if (blocking) {
                if (blocking->to() == pm->from()) {
                    // We annotate cycles at each move in the cycle, and
                    // assert that we do not find two cycles in one move chain
                    // traversal (which would indicate two moves to the same
                    // destination).
                    pm->setCycleEnd();
                    blocking->setCycleBegin(pm->type());
                    hasCycles_ = true;
                    pending_.remove(blocking);
                    stack.pushBack(blocking);
                } else {
                    // This is a new link in the move chain, so keep
                    // searching for a cycle.
                    pending_.remove(blocking);
                    stack.pushBack(blocking);
                }
            } else {
                // Otherwise, pop the last move on the search stack because it's
                // complete and not participating in a cycle. The resulting
                // move can safely be added to the ordered move list.
                PendingMove *done = stack.popBack();
                if (!orderedMoves_.append(*done))
                    return false;
                movePool_.free(done);
            }
        }
    }

    return true;
}
