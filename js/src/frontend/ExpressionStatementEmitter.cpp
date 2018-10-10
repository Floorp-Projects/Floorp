/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "frontend/ExpressionStatementEmitter.h"

#include "frontend/BytecodeEmitter.h"
#include "vm/Opcodes.h"

using namespace js;
using namespace js::frontend;

using mozilla::Maybe;

ExpressionStatementEmitter::ExpressionStatementEmitter(BytecodeEmitter* bce,
                                                       ValueUsage valueUsage)
  : bce_(bce),
    valueUsage_(valueUsage)
{}

bool
ExpressionStatementEmitter::prepareForExpr(const Maybe<uint32_t>& beginPos)
{
    MOZ_ASSERT(state_ == State::Start);

    if (beginPos) {
        if (!bce_->updateSourceCoordNotes(*beginPos)) {
            return false;
        }
    }

#ifdef DEBUG
    depth_ = bce_->stackDepth;
    state_ = State::Expr;
#endif
    return true;
}

bool
ExpressionStatementEmitter::emitEnd()
{
    MOZ_ASSERT(state_ == State::Expr);
    MOZ_ASSERT(bce_->stackDepth == depth_ + 1);

    JSOp op = valueUsage_ == ValueUsage::WantValue ? JSOP_SETRVAL : JSOP_POP;
    if (!bce_->emit1(op)) {
        return false;
    }

#ifdef DEBUG
    state_ = State::End;
#endif
    return true;
}
