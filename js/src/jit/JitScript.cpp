/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/JitScript-inl.h"

#include "mozilla/IntegerPrintfMacros.h"
#include "mozilla/Move.h"
#include "mozilla/ScopeExit.h"

#include "jit/BaselineIC.h"
#include "vm/JSScript.h"
#include "vm/TypeInference.h"

#include "vm/JSScript-inl.h"
#include "vm/TypeInference-inl.h"

using namespace js;

static void FillBytecodeTypeMap(JSScript* script, uint32_t* bytecodeMap) {
  uint32_t added = 0;
  for (jsbytecode* pc = script->code(); pc < script->codeEnd();
       pc += GetBytecodeLength(pc)) {
    JSOp op = JSOp(*pc);
    if (CodeSpec[op].format & JOF_TYPESET) {
      bytecodeMap[added++] = script->pcToOffset(pc);
      if (added == script->numBytecodeTypeSets()) {
        break;
      }
    }
  }
  MOZ_ASSERT(added == script->numBytecodeTypeSets());
}

static size_t NumTypeSets(JSScript* script) {
  size_t num = script->numBytecodeTypeSets() + 1 /* this */;
  if (JSFunction* fun = script->functionNonDelazifying()) {
    num += fun->nargs();
  }

  // We rely on |num| being in a safe range to prevent overflow when allocating
  // JitScript.
  static_assert(JSScript::MaxBytecodeTypeSets == UINT16_MAX,
                "JSScript typesets should have safe range to avoid overflow");
  static_assert(JSFunction::NArgsBits == 16,
                "JSFunction nargs should have safe range to avoid overflow");

  return num;
}

JitScript::JitScript(JSScript* script, ICScriptPtr&& icScript,
                     uint32_t numTypeSets)
    : icScript_(std::move(icScript)), numTypeSets_(numTypeSets) {
  setTypesGeneration(script->zone()->types.generation);

  StackTypeSet* array = typeArrayDontCheckGeneration();
  for (unsigned i = 0; i < numTypeSets; i++) {
    new (&array[i]) StackTypeSet();
  }

  FillBytecodeTypeMap(script, bytecodeTypeMap());
}

bool JSScript::createJitScript(JSContext* cx) {
  MOZ_ASSERT(!jitScript_);
  cx->check(this);

  // Scripts that will never run in the Baseline Interpreter or the JITs don't
  // need a JitScript.
  MOZ_ASSERT(!hasForceInterpreterOp());

  AutoEnterAnalysis enter(cx);

  // Run the arguments-analysis if needed. Both the Baseline Interpreter and
  // Compiler rely on this.
  if (!ensureHasAnalyzedArgsUsage(cx)) {
    return false;
  }

  // If ensureHasAnalyzedArgsUsage allocated the JitScript we're done.
  if (jitScript_) {
    return true;
  }

  UniquePtr<jit::ICScript> icScript(jit::ICScript::create(cx, this));
  if (!icScript) {
    return false;
  }

  // We need to call prepareForDestruction on ICScript before we |delete| it.
  auto prepareForDestruction = mozilla::MakeScopeExit(
      [&] { icScript->prepareForDestruction(cx->zone()); });

  size_t numTypeSets = NumTypeSets(this);
  size_t bytecodeTypeMapEntries = numBytecodeTypeSets();

  // Calculate allocation size. This cannot overflow, see comment in
  // NumTypeSets.
  static_assert(sizeof(JitScript) ==
                    sizeof(StackTypeSet) + offsetof(JitScript, typeArray_),
                "typeArray_ must be last member of JitScript");
  size_t allocSize =
      (offsetof(JitScript, typeArray_) + numTypeSets * sizeof(StackTypeSet) +
       bytecodeTypeMapEntries * sizeof(uint32_t));

  auto jitScript =
      reinterpret_cast<JitScript*>(cx->pod_malloc<uint8_t>(allocSize));
  if (!jitScript) {
    return false;
  }

  prepareForDestruction.release();

  MOZ_ASSERT(!jitScript_);
  jitScript_ =
      new (jitScript) JitScript(this, std::move(icScript), numTypeSets);

  // We have a JitScript so we can set the script's jitCodeRaw_ pointer to the
  // Baseline Interpreter code.
  updateJitCodeRaw(cx->runtime());

#ifdef DEBUG
  AutoSweepJitScript sweep(this);
  StackTypeSet* typeArray = jitScript->typeArrayDontCheckGeneration();
  for (unsigned i = 0; i < numBytecodeTypeSets(); i++) {
    InferSpew(ISpewOps, "typeSet: %sT%p%s bytecode%u %p",
              InferSpewColor(&typeArray[i]), &typeArray[i],
              InferSpewColorReset(), i, this);
  }
  StackTypeSet* thisTypes = jitScript->thisTypes(sweep, this);
  InferSpew(ISpewOps, "typeSet: %sT%p%s this %p", InferSpewColor(thisTypes),
            thisTypes, InferSpewColorReset(), this);
  unsigned nargs =
      functionNonDelazifying() ? functionNonDelazifying()->nargs() : 0;
  for (unsigned i = 0; i < nargs; i++) {
    StackTypeSet* types = jitScript->argTypes(sweep, this, i);
    InferSpew(ISpewOps, "typeSet: %sT%p%s arg%u %p", InferSpewColor(types),
              types, InferSpewColorReset(), i, this);
  }
#endif

  return true;
}

void JSScript::maybeReleaseJitScript() {
  if (!jitScript_ || zone()->types.keepJitScripts || hasBaselineScript() ||
      jitScript_->active()) {
    return;
  }

  MOZ_ASSERT(!hasIonScript());

  jitScript_->destroy(zone());
  jitScript_ = nullptr;
  updateJitCodeRaw(runtimeFromMainThread());
}

void JitScript::destroy(Zone* zone) {
  icScript_->prepareForDestruction(zone);

  js_delete(this);
}

#ifdef DEBUG
void JitScript::printTypes(JSContext* cx, HandleScript script) {
  AutoSweepJitScript sweep(script);
  MOZ_ASSERT(script->jitScript() == this);

  AutoEnterAnalysis enter(nullptr, script->zone());
  Fprinter out(stderr);

  if (script->functionNonDelazifying()) {
    fprintf(stderr, "Function");
  } else if (script->isForEval()) {
    fprintf(stderr, "Eval");
  } else {
    fprintf(stderr, "Main");
  }
  fprintf(stderr, " %#" PRIxPTR " %s:%u ", uintptr_t(script.get()),
          script->filename(), script->lineno());

  if (script->functionNonDelazifying()) {
    if (JSAtom* name = script->functionNonDelazifying()->explicitName()) {
      name->dumpCharsNoNewline(out);
    }
  }

  fprintf(stderr, "\n    this:");
  thisTypes(sweep, script)->print();

  for (unsigned i = 0; script->functionNonDelazifying() &&
                       i < script->functionNonDelazifying()->nargs();
       i++) {
    fprintf(stderr, "\n    arg%u:", i);
    argTypes(sweep, script, i)->print();
  }
  fprintf(stderr, "\n");

  for (jsbytecode* pc = script->code(); pc < script->codeEnd();
       pc += GetBytecodeLength(pc)) {
    {
      fprintf(stderr, "%p:", script.get());
      Sprinter sprinter(cx);
      if (!sprinter.init()) {
        return;
      }
      Disassemble1(cx, script, pc, script->pcToOffset(pc), true, &sprinter);
      fprintf(stderr, "%s", sprinter.string());
    }

    if (CodeSpec[*pc].format & JOF_TYPESET) {
      StackTypeSet* types = bytecodeTypes(sweep, script, pc);
      fprintf(stderr, "  typeset %u:", unsigned(types - typeArray(sweep)));
      types->print();
      fprintf(stderr, "\n");
    }
  }

  fprintf(stderr, "\n");
}
#endif /* DEBUG */
