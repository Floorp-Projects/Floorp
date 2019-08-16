/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_JitScript_inl_h
#define jit_JitScript_inl_h

#include "jit/JitScript.h"

#include "mozilla/BinarySearch.h"

#include "vm/BytecodeUtil.h"
#include "vm/JSScript.h"
#include "vm/TypeInference.h"

#include "vm/JSContext-inl.h"

namespace js {
namespace jit {

inline StackTypeSet* JitScript::thisTypes(const AutoSweepJitScript& sweep,
                                          JSScript* script) {
  return typeArray(sweep) + script->numBytecodeTypeSets();
}

/*
 * Note: for non-escaping arguments, argTypes reflect only the initial type of
 * the variable (e.g. passed values for argTypes, or undefined for localTypes)
 * and not types from subsequent assignments.
 */

inline StackTypeSet* JitScript::argTypes(const AutoSweepJitScript& sweep,
                                         JSScript* script, unsigned i) {
  MOZ_ASSERT(i < script->functionNonDelazifying()->nargs());
  return typeArray(sweep) + script->numBytecodeTypeSets() + 1 /* this */ + i;
}

template <typename TYPESET>
/* static */ inline TYPESET* JitScript::BytecodeTypes(JSScript* script,
                                                      jsbytecode* pc,
                                                      uint32_t* bytecodeMap,
                                                      uint32_t* hint,
                                                      TYPESET* typeArray) {
  MOZ_ASSERT(BytecodeOpHasTypeSet(JSOp(*pc)));
  uint32_t offset = script->pcToOffset(pc);

  // See if this pc is the next typeset opcode after the last one looked up.
  size_t numBytecodeTypeSets = script->numBytecodeTypeSets();
  if ((*hint + 1) < numBytecodeTypeSets && bytecodeMap[*hint + 1] == offset) {
    (*hint)++;
    return typeArray + *hint;
  }

  // See if this pc is the same as the last one looked up.
  if (bytecodeMap[*hint] == offset) {
    return typeArray + *hint;
  }

  // Fall back to a binary search.  We'll either find the exact offset, or
  // there are more JOF_TYPESET opcodes than nTypeSets in the script (as can
  // happen if the script is very long) and we'll use the last location.
  size_t loc;
  bool found =
      mozilla::BinarySearch(bytecodeMap, 0, numBytecodeTypeSets, offset, &loc);
  if (found) {
    MOZ_ASSERT(bytecodeMap[loc] == offset);
  } else {
    MOZ_ASSERT(numBytecodeTypeSets == JSScript::MaxBytecodeTypeSets);
    loc = numBytecodeTypeSets - 1;
  }

  *hint = mozilla::AssertedCast<uint32_t>(loc);
  return typeArray + *hint;
}

inline StackTypeSet* JitScript::bytecodeTypes(const AutoSweepJitScript& sweep,
                                              JSScript* script,
                                              jsbytecode* pc) {
  MOZ_ASSERT(CurrentThreadCanAccessZone(script->zone()));
  return BytecodeTypes(script, pc, bytecodeTypeMap(), bytecodeTypeMapHint(),
                       typeArray(sweep));
}

inline AutoKeepJitScripts::AutoKeepJitScripts(JSContext* cx)
    : zone_(cx->zone()->types), prev_(zone_.keepJitScripts) {
  zone_.keepJitScripts = true;
}

inline AutoKeepJitScripts::~AutoKeepJitScripts() {
  MOZ_ASSERT(zone_.keepJitScripts);
  zone_.keepJitScripts = prev_;
}

inline bool JitScript::typesNeedsSweep(Zone* zone) const {
  MOZ_ASSERT(!js::TlsContext.get()->inUnsafeCallWithABI);
  return typesGeneration() != zone->types.generation;
}

}  // namespace jit
}  // namespace js

inline bool JSScript::ensureHasJitScript(JSContext* cx,
                                         js::jit::AutoKeepJitScripts&) {
  if (hasJitScript()) {
    return true;
  }
  return createJitScript(cx);
}

#endif /* jit_JitScript_inl_h */
