/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* ReadableStream.prototype.pipeTo state. */

#include "builtin/streams/PipeToState.h"

#include "js/Class.h"       // JSClass, JSCLASS_HAS_RESERVED_SLOTS
#include "js/RootingAPI.h"  // JS::Rooted

#include "vm/JSObject-inl.h"  // js::NewBuiltinClassInstance

using JS::Int32Value;
using JS::Rooted;

using js::PipeToState;

/* static */ PipeToState* PipeToState::create(JSContext* cx) {
  Rooted<PipeToState*> state(cx, NewBuiltinClassInstance<PipeToState>(cx));
  if (!state) {
    return nullptr;
  }

  state->setFixedSlot(Slot_Flags, Int32Value(0));

  return state;
}

const JSClass PipeToState::class_ = {"PipeToState",
                                     JSCLASS_HAS_RESERVED_SLOTS(SlotCount)};
