/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef frontend_TDZCheckCache_h
#define frontend_TDZCheckCache_h

#include "mozilla/Attributes.h"
#include "mozilla/Maybe.h"

#include "ds/Nestable.h"
#include "frontend/NameCollections.h"
#include "js/TypeDecls.h"
#include "vm/Stack.h"

namespace js {
namespace frontend {

struct BytecodeEmitter;

// A cache that tracks Temporal Dead Zone (TDZ) checks, so that any use of a
// lexical variable that's dominated by an earlier use, or by evaluation of its
// declaration (which will initialize it, perhaps to |undefined|), doesn't have
// to redundantly check that the lexical variable has been initialized
//
// Each basic block should have a TDZCheckCache in scope. Some NestableControl
// subclasses contain a TDZCheckCache.
//
// When a scope containing lexical variables is entered, all such variables are
// marked as CheckTDZ.  When a lexical variable is accessed, its entry is
// checked.  If it's CheckTDZ, a JSOP_CHECKLEXICAL is emitted and then the
// entry is marked DontCheckTDZ.  If it's DontCheckTDZ, no check is emitted
// because a prior check would have already failed.  Finally, because
// evaluating a lexical variable declaration initializes it (after any
// initializer is evaluated), evaluating a lexical declaration marks its entry
// as DontCheckTDZ.
class TDZCheckCache : public Nestable<TDZCheckCache>
{
    PooledMapPtr<CheckTDZMap> cache_;

    MOZ_MUST_USE bool ensureCache(BytecodeEmitter* bce);

  public:
    explicit TDZCheckCache(BytecodeEmitter* bce);

    mozilla::Maybe<MaybeCheckTDZ> needsTDZCheck(BytecodeEmitter* bce, JSAtom* name);
    MOZ_MUST_USE bool noteTDZCheck(BytecodeEmitter* bce, JSAtom* name, MaybeCheckTDZ check);
};

} /* namespace frontend */
} /* namespace js */

#endif /* frontend_TDZCheckCache_h */
