/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/InterpreterEntryTrampoline.h"
#include "jit/JitRuntime.h"

#include "gc/Marking-inl.h"

using namespace js;
using namespace js::jit;

void js::ClearInterpreterEntryMap(JSRuntime* runtime) {
  if (runtime->hasJitRuntime() &&
      runtime->jitRuntime()->hasInterpreterEntryMap()) {
    runtime->jitRuntime()->getInterpreterEntryMap()->clear();
  }
}

void EntryTrampolineMap::traceTrampolineCode(JSTracer* trc) {
  for (jit::EntryTrampolineMap::Enum e(*this); !e.empty(); e.popFront()) {
    EntryTrampoline& trampoline = e.front().value();
    trampoline.trace(trc);
  }
}

void EntryTrampolineMap::updateScriptsAfterMovingGC(void) {
  for (jit::EntryTrampolineMap::Enum e(*this); !e.empty(); e.popFront()) {
    JSScript* script = e.front().key();
    if (IsForwarded(script)) {
      script = Forwarded(script);
      e.rekeyFront(script);
    }
  }
}

#ifdef JSGC_HASH_TABLE_CHECKS
void EntryTrampoline::checkTrampolineAfterMovingGC() {
  JitCode* trampoline = entryTrampoline_;
  CheckGCThingAfterMovingGC(trampoline);
}

void EntryTrampolineMap::checkScriptsAfterMovingGC() {
  for (jit::EntryTrampolineMap::Enum r(*this); !r.empty(); r.popFront()) {
    JSScript* script = r.front().key();
    CheckGCThingAfterMovingGC(script);
    r.front().value().checkTrampolineAfterMovingGC();
    auto ptr = lookup(script);
    MOZ_RELEASE_ASSERT(ptr.found() && &*ptr == &r.front());
  }
}
#endif
