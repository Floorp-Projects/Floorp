/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/JitScript-inl.h"

#include "mozilla/BinarySearch.h"
#include "mozilla/IntegerPrintfMacros.h"
#include "mozilla/Move.h"
#include "mozilla/ScopeExit.h"

#include "jit/BaselineIC.h"
#include "jit/BytecodeAnalysis.h"
#include "vm/BytecodeUtil.h"
#include "vm/JSScript.h"
#include "vm/Stack.h"
#include "vm/TypeInference.h"
#include "wasm/WasmInstance.h"

#include "gc/FreeOp-inl.h"
#include "jit/JSJitFrameIter-inl.h"
#include "vm/JSScript-inl.h"
#include "vm/TypeInference-inl.h"

using namespace js;
using namespace js::jit;

/* static */
size_t JitScript::NumTypeSets(JSScript* script) {
  // We rely on |num| not overflowing below.
  static_assert(JSScript::MaxBytecodeTypeSets == UINT16_MAX,
                "JSScript typesets should have safe range to avoid overflow");
  static_assert(JSFunction::NArgsBits == 16,
                "JSFunction nargs should have safe range to avoid overflow");

  size_t num = script->numBytecodeTypeSets() + 1 /* this */;
  if (JSFunction* fun = script->functionNonDelazifying()) {
    num += fun->nargs();
  }

  return num;
}

JitScript::JitScript(JSScript* script, uint32_t typeSetOffset,
                     uint32_t bytecodeTypeMapOffset, uint32_t allocBytes,
                     const char* profileString)
    : profileString_(profileString),
      typeSetOffset_(typeSetOffset),
      bytecodeTypeMapOffset_(bytecodeTypeMapOffset),
      allocBytes_(allocBytes) {
  setTypesGeneration(script->zone()->types.generation);

  uint8_t* base = reinterpret_cast<uint8_t*>(this);
  DefaultInitializeElements<StackTypeSet>(base + typeSetOffset, numTypeSets());

  // Ensure the baselineScript_ and ionScript_ fields match the BaselineDisabled
  // and IonDisabled script flags.
  if (!script->canBaselineCompile()) {
    setBaselineScriptImpl(script, BaselineDisabledScriptPtr);
  }
  if (!script->canIonCompile()) {
    setIonScriptImpl(script, IonDisabledScriptPtr);
  }
}

bool JSScript::createJitScript(JSContext* cx) {
  MOZ_ASSERT(!jitScript_);
  cx->check(this);

  // Scripts with a JitScript can run in the Baseline Interpreter. Make sure
  // we don't create a JitScript for scripts we shouldn't Baseline interpret.
  MOZ_ASSERT_IF(IsBaselineInterpreterEnabled(),
                CanBaselineInterpretScript(this));

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

  // Store the profile string in the JitScript if the profiler is enabled.
  const char* profileString = nullptr;
  if (cx->runtime()->geckoProfiler().enabled()) {
    profileString = cx->runtime()->geckoProfiler().profileString(cx, this);
    if (!profileString) {
      return false;
    }
  }

  size_t numTypeSets = JitScript::NumTypeSets(this);

  static_assert(sizeof(JitScript) % sizeof(uintptr_t) == 0,
                "Trailing arrays must be aligned properly");
  static_assert(sizeof(ICEntry) % sizeof(uintptr_t) == 0,
                "Trailing arrays must be aligned properly");
  static_assert(sizeof(StackTypeSet) % sizeof(uintptr_t) == 0,
                "Trailing arrays must be aligned properly");

  // Calculate allocation size.
  CheckedInt<uint32_t> allocSize = sizeof(JitScript);
  allocSize += CheckedInt<uint32_t>(numICEntries()) * sizeof(ICEntry);
  allocSize += CheckedInt<uint32_t>(numTypeSets) * sizeof(StackTypeSet);
  allocSize += CheckedInt<uint32_t>(numBytecodeTypeSets()) * sizeof(uint32_t);
  if (!allocSize.isValid()) {
    ReportAllocationOverflow(cx);
    return false;
  }

  void* raw = cx->pod_malloc<uint8_t>(allocSize.value());
  MOZ_ASSERT(uintptr_t(raw) % alignof(JitScript) == 0);
  if (!raw) {
    return false;
  }

  uint32_t typeSetOffset = sizeof(JitScript) + numICEntries() * sizeof(ICEntry);
  uint32_t bytecodeTypeMapOffset =
      typeSetOffset + numTypeSets * sizeof(StackTypeSet);
  UniquePtr<JitScript> jitScript(
      new (raw) JitScript(this, typeSetOffset, bytecodeTypeMapOffset,
                          allocSize.value(), profileString));

  // Sanity check the length computations.
  MOZ_ASSERT(jitScript->numICEntries() == numICEntries());
  MOZ_ASSERT(jitScript->numTypeSets() == numTypeSets);

  // We need to call prepareForDestruction on JitScript before we |delete| it.
  auto prepareForDestruction = mozilla::MakeScopeExit(
      [&] { jitScript->prepareForDestruction(cx->zone()); });

  if (!jitScript->initICEntriesAndBytecodeTypeMap(cx, this)) {
    return false;
  }

  MOZ_ASSERT(!jitScript_);
  prepareForDestruction.release();
  jitScript_ = jitScript.release();
  AddCellMemory(this, allocSize.value(), MemoryUse::JitScript);

  // We have a JitScript so we can set the script's jitCodeRaw_ pointer to the
  // Baseline Interpreter code.
  updateJitCodeRaw(cx->runtime());

#ifdef DEBUG
  AutoSweepJitScript sweep(this);
  StackTypeSet* typeArray = jitScript_->typeArrayDontCheckGeneration();
  for (unsigned i = 0; i < numBytecodeTypeSets(); i++) {
    InferSpew(ISpewOps, "typeSet: %sT%p%s bytecode%u %p",
              InferSpewColor(&typeArray[i]), &typeArray[i],
              InferSpewColorReset(), i, this);
  }
  StackTypeSet* thisTypes = jitScript_->thisTypes(sweep, this);
  InferSpew(ISpewOps, "typeSet: %sT%p%s this %p", InferSpewColor(thisTypes),
            thisTypes, InferSpewColorReset(), this);
  unsigned nargs =
      functionNonDelazifying() ? functionNonDelazifying()->nargs() : 0;
  for (unsigned i = 0; i < nargs; i++) {
    StackTypeSet* types = jitScript_->argTypes(sweep, this, i);
    InferSpew(ISpewOps, "typeSet: %sT%p%s arg%u %p", InferSpewColor(types),
              types, InferSpewColorReset(), i, this);
  }
#endif

  return true;
}

void JSScript::maybeReleaseJitScript(JSFreeOp* fop) {
  MOZ_ASSERT(hasJitScript());

  if (zone()->types.keepJitScripts || jitScript_->hasBaselineScript() ||
      jitScript_->active()) {
    return;
  }

  releaseJitScript(fop);
}

void JSScript::releaseJitScript(JSFreeOp* fop) {
  MOZ_ASSERT(hasJitScript());
  MOZ_ASSERT(!hasBaselineScript());
  MOZ_ASSERT(!hasIonScript());

  fop->removeCellMemory(this, jitScript_->allocBytes(), MemoryUse::JitScript);

  JitScript::Destroy(zone(), jitScript_);
  jitScript_ = nullptr;
  updateJitCodeRaw(fop->runtime());
}

void JSScript::releaseJitScriptOnFinalize(JSFreeOp* fop) {
  MOZ_ASSERT(hasJitScript());

  if (hasIonScript()) {
    IonScript* ion = jitScript()->clearIonScript(fop, this);
    jit::IonScript::Destroy(fop, ion);
  }

  if (hasBaselineScript()) {
    BaselineScript* baseline = jitScript()->clearBaselineScript(fop, this);
    jit::BaselineScript::Destroy(fop, baseline);
  }

  releaseJitScript(fop);
}

void JitScript::CachedIonData::trace(JSTracer* trc) {
  TraceNullableEdge(trc, &templateEnv, "jitscript-iondata-template-env");
}

void JitScript::trace(JSTracer* trc) {
  if (hasBaselineScript()) {
    baselineScript()->trace(trc);
  }

  if (hasIonScript()) {
    ionScript()->trace(trc);
  }

  if (hasCachedIonData()) {
    cachedIonData().trace(trc);
  }

  // Mark all IC stub codes hanging off the IC stub entries.
  for (size_t i = 0; i < numICEntries(); i++) {
    ICEntry& ent = icEntry(i);
    ent.trace(trc);
  }
}

void JitScript::ensureProfileString(JSContext* cx, JSScript* script) {
  MOZ_ASSERT(cx->runtime()->geckoProfiler().enabled());

  if (profileString_) {
    return;
  }

  AutoEnterOOMUnsafeRegion oomUnsafe;
  profileString_ = cx->runtime()->geckoProfiler().profileString(cx, script);
  if (!profileString_) {
    oomUnsafe.crash("Failed to allocate profile string");
  }
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

    if (BytecodeOpHasTypeSet(JSOp(*pc))) {
      StackTypeSet* types = bytecodeTypes(sweep, script, pc);
      fprintf(stderr, "  typeset %u:", unsigned(types - typeArray(sweep)));
      types->print();
      fprintf(stderr, "\n");
    }
  }

  fprintf(stderr, "\n");
}
#endif /* DEBUG */

/* static */
void JitScript::Destroy(Zone* zone, JitScript* script) {
  script->unlinkDependentWasmImports();
  script->prepareForDestruction(zone);

  js_delete(script);
}

struct ICEntries {
  JitScript* const jitScript_;

  explicit ICEntries(JitScript* jitScript) : jitScript_(jitScript) {}

  size_t numEntries() const { return jitScript_->numICEntries(); }
  ICEntry& operator[](size_t index) const { return jitScript_->icEntry(index); }
};

static bool ComputeBinarySearchMid(ICEntries entries, uint32_t pcOffset,
                                   size_t* loc) {
  return mozilla::BinarySearchIf(
      entries, 0, entries.numEntries(),
      [pcOffset](const ICEntry& entry) {
        uint32_t entryOffset = entry.pcOffset();
        if (pcOffset < entryOffset) {
          return -1;
        }
        if (entryOffset < pcOffset) {
          return 1;
        }
        if (entry.isForPrologue()) {
          // Prologue ICEntries are used for function argument type checks.
          // Ignore these entries and return 1 because these entries appear in
          // the ICEntry list before the other ICEntry (if any) at offset 0.
          MOZ_ASSERT(entryOffset == 0);
          return 1;
        }
        return 0;
      },
      loc);
}

ICEntry* JitScript::maybeICEntryFromPCOffset(uint32_t pcOffset) {
  // This method ignores prologue IC entries. There can be at most one
  // non-prologue IC per bytecode op.

  size_t mid;
  if (!ComputeBinarySearchMid(ICEntries(this), pcOffset, &mid)) {
    return nullptr;
  }

  MOZ_ASSERT(mid < numICEntries());

  ICEntry& entry = icEntry(mid);
  MOZ_ASSERT(!entry.isForPrologue());
  MOZ_ASSERT(entry.pcOffset() == pcOffset);
  return &entry;
}

ICEntry& JitScript::icEntryFromPCOffset(uint32_t pcOffset) {
  ICEntry* entry = maybeICEntryFromPCOffset(pcOffset);
  MOZ_RELEASE_ASSERT(entry);
  return *entry;
}

ICEntry* JitScript::maybeICEntryFromPCOffset(uint32_t pcOffset,
                                             ICEntry* prevLookedUpEntry) {
  // Do a linear forward search from the last queried PC offset, or fallback to
  // a binary search if the last offset is too far away.
  if (prevLookedUpEntry && pcOffset >= prevLookedUpEntry->pcOffset() &&
      (pcOffset - prevLookedUpEntry->pcOffset()) <= 10) {
    ICEntry* firstEntry = &icEntry(0);
    ICEntry* lastEntry = &icEntry(numICEntries() - 1);
    ICEntry* curEntry = prevLookedUpEntry;
    while (curEntry >= firstEntry && curEntry <= lastEntry) {
      if (curEntry->pcOffset() == pcOffset && !curEntry->isForPrologue()) {
        return curEntry;
      }
      curEntry++;
    }
    return nullptr;
  }

  return maybeICEntryFromPCOffset(pcOffset);
}

ICEntry& JitScript::icEntryFromPCOffset(uint32_t pcOffset,
                                        ICEntry* prevLookedUpEntry) {
  ICEntry* entry = maybeICEntryFromPCOffset(pcOffset, prevLookedUpEntry);
  MOZ_RELEASE_ASSERT(entry);
  return *entry;
}

ICEntry* JitScript::interpreterICEntryFromPCOffset(uint32_t pcOffset) {
  // We have to return the entry to store in BaselineFrame::interpreterICEntry
  // when resuming in the Baseline Interpreter at pcOffset. The bytecode op at
  // pcOffset does not necessarily have an ICEntry, so we want to return the
  // first ICEntry for which the following is true:
  //
  //    !entry.isForPrologue() && entry.pcOffset() >= pcOffset
  //
  // Fortunately, ComputeBinarySearchMid returns exactly this entry.

  size_t mid;
  ComputeBinarySearchMid(ICEntries(this), pcOffset, &mid);

  if (mid < numICEntries()) {
    ICEntry& entry = icEntry(mid);
    MOZ_ASSERT(!entry.isForPrologue());
    MOZ_ASSERT(entry.pcOffset() >= pcOffset);
    return &entry;
  }

  // Resuming at a pc after the last ICEntry. Just return nullptr:
  // BaselineFrame::interpreterICEntry will never be used in this case.
  return nullptr;
}

void JitScript::purgeOptimizedStubs(JSScript* script) {
  MOZ_ASSERT(script->jitScript() == this);

  Zone* zone = script->zone();
  if (zone->isGCSweeping() && IsAboutToBeFinalizedDuringSweep(*script)) {
    // We're sweeping and the script is dead. Don't purge optimized stubs
    // because (1) accessing CacheIRStubInfo pointers in ICStubs is invalid
    // because we may have swept them already when we started (incremental)
    // sweeping and (2) it's unnecessary because this script will be finalized
    // soon anyway.
    return;
  }

  JitSpew(JitSpew_BaselineIC, "Purging optimized stubs");

  for (size_t i = 0; i < numICEntries(); i++) {
    ICEntry& entry = icEntry(i);
    ICStub* lastStub = entry.firstStub();
    while (lastStub->next()) {
      lastStub = lastStub->next();
    }

    if (lastStub->isFallback()) {
      // Unlink all stubs allocated in the optimized space.
      ICStub* stub = entry.firstStub();
      ICStub* prev = nullptr;

      while (stub->next()) {
        if (!stub->allocatedInFallbackSpace()) {
          lastStub->toFallbackStub()->unlinkStub(zone, prev, stub);
          stub = stub->next();
          continue;
        }

        prev = stub;
        stub = stub->next();
      }

      if (lastStub->isMonitoredFallback()) {
        // Monitor stubs can't make calls, so are always in the
        // optimized stub space.
        ICTypeMonitor_Fallback* lastMonStub =
            lastStub->toMonitoredFallbackStub()->maybeFallbackMonitorStub();
        if (lastMonStub) {
          lastMonStub->resetMonitorStubChain(zone);
        }
      }
    } else if (lastStub->isTypeMonitor_Fallback()) {
      lastStub->toTypeMonitor_Fallback()->resetMonitorStubChain(zone);
    } else {
      MOZ_CRASH("Unknown fallback stub");
    }
  }

#ifdef DEBUG
  // All remaining stubs must be allocated in the fallback space.
  for (size_t i = 0; i < numICEntries(); i++) {
    ICEntry& entry = icEntry(i);
    ICStub* stub = entry.firstStub();
    while (stub->next()) {
      MOZ_ASSERT(stub->allocatedInFallbackSpace());
      stub = stub->next();
    }
  }
#endif
}

void JitScript::noteAccessedGetter(uint32_t pcOffset) {
  ICEntry& entry = icEntryFromPCOffset(pcOffset);
  ICFallbackStub* stub = entry.fallbackStub();

  if (stub->isGetProp_Fallback()) {
    stub->toGetProp_Fallback()->noteAccessedGetter();
  }
}

void JitScript::noteHasDenseAdd(uint32_t pcOffset) {
  ICEntry& entry = icEntryFromPCOffset(pcOffset);
  ICFallbackStub* stub = entry.fallbackStub();

  if (stub->isSetElem_Fallback()) {
    stub->toSetElem_Fallback()->noteHasDenseAdd();
  }
}

void JitScript::unlinkDependentWasmImports() {
  // Remove any links from wasm::Instances that contain optimized FFI calls into
  // this JitScript.
  if (dependentWasmImports_) {
    for (DependentWasmImport& dep : *dependentWasmImports_) {
      dep.instance->deoptimizeImportExit(dep.importIndex);
    }
    dependentWasmImports_.reset();
  }
}

bool JitScript::addDependentWasmImport(JSContext* cx, wasm::Instance& instance,
                                       uint32_t idx) {
  if (!dependentWasmImports_) {
    dependentWasmImports_ = cx->make_unique<Vector<DependentWasmImport>>(cx);
    if (!dependentWasmImports_) {
      return false;
    }
  }
  return dependentWasmImports_->emplaceBack(instance, idx);
}

void JitScript::removeDependentWasmImport(wasm::Instance& instance,
                                          uint32_t idx) {
  if (!dependentWasmImports_) {
    return;
  }

  for (DependentWasmImport& dep : *dependentWasmImports_) {
    if (dep.instance == &instance && dep.importIndex == idx) {
      dependentWasmImports_->erase(&dep);
      break;
    }
  }
}

JitScript::CachedIonData::CachedIonData(EnvironmentObject* templateEnv,
                                        IonBytecodeInfo bytecodeInfo)
    : templateEnv(templateEnv), bytecodeInfo(bytecodeInfo) {}

bool JitScript::ensureHasCachedIonData(JSContext* cx, HandleScript script) {
  MOZ_ASSERT(script->jitScript() == this);

  if (hasCachedIonData()) {
    return true;
  }

  Rooted<EnvironmentObject*> templateEnv(cx);
  if (script->functionNonDelazifying()) {
    RootedFunction fun(cx, script->functionNonDelazifying());

    if (fun->needsNamedLambdaEnvironment()) {
      templateEnv =
          NamedLambdaObject::createTemplateObject(cx, fun, gc::TenuredHeap);
      if (!templateEnv) {
        return false;
      }
    }

    if (fun->needsCallObject()) {
      templateEnv = CallObject::createTemplateObject(cx, script, templateEnv,
                                                     gc::TenuredHeap);
      if (!templateEnv) {
        return false;
      }
    }
  }

  IonBytecodeInfo bytecodeInfo = AnalyzeBytecodeForIon(cx, script);

  UniquePtr<CachedIonData> data =
      cx->make_unique<CachedIonData>(templateEnv, bytecodeInfo);
  if (!data) {
    return false;
  }

  cachedIonData_ = std::move(data);
  return true;
}

void JitScript::setBaselineScriptImpl(JSScript* script,
                                      BaselineScript* baselineScript) {
  JSRuntime* rt = script->runtimeFromMainThread();
  setBaselineScriptImpl(rt->defaultFreeOp(), script, baselineScript);
}

void JitScript::setBaselineScriptImpl(JSFreeOp* fop, JSScript* script,
                                      BaselineScript* baselineScript) {
  if (hasBaselineScript()) {
    BaselineScript::writeBarrierPre(script->zone(), baselineScript_);
    fop->removeCellMemory(script, baselineScript_->allocBytes(),
                          MemoryUse::BaselineScript);
    baselineScript_ = nullptr;
  }

  MOZ_ASSERT(ionScript_ == nullptr || ionScript_ == IonDisabledScriptPtr);

  baselineScript_ = baselineScript;
  if (hasBaselineScript()) {
    AddCellMemory(script, baselineScript_->allocBytes(),
                  MemoryUse::BaselineScript);
  }

  script->resetWarmUpResetCounter();
  script->updateJitCodeRaw(fop->runtime());
}

void JitScript::setIonScriptImpl(JSScript* script, IonScript* ionScript) {
  JSRuntime* rt = script->runtimeFromMainThread();
  setIonScriptImpl(rt->defaultFreeOp(), script, ionScript);
}

void JitScript::setIonScriptImpl(JSFreeOp* fop, JSScript* script,
                                 IonScript* ionScript) {
  MOZ_ASSERT_IF(ionScript != IonDisabledScriptPtr,
                !baselineScript()->hasPendingIonBuilder());

  if (hasIonScript()) {
    IonScript::writeBarrierPre(script->zone(), ionScript_);
    fop->removeCellMemory(script, ionScript_->allocBytes(),
                          MemoryUse::IonScript);
    ionScript_ = nullptr;
  }

  ionScript_ = ionScript;
  MOZ_ASSERT_IF(hasIonScript(), hasBaselineScript());
  if (hasIonScript()) {
    AddCellMemory(script, ionScript_->allocBytes(), MemoryUse::IonScript);
  }

  script->updateJitCodeRaw(fop->runtime());
}

#ifdef JS_STRUCTURED_SPEW
static bool GetStubEnteredCount(ICStub* stub, uint32_t* count) {
  switch (stub->kind()) {
    case ICStub::CacheIR_Regular:
      *count = stub->toCacheIR_Regular()->enteredCount();
      return true;
    case ICStub::CacheIR_Updated:
      *count = stub->toCacheIR_Updated()->enteredCount();
      return true;
    case ICStub::CacheIR_Monitored:
      *count = stub->toCacheIR_Monitored()->enteredCount();
      return true;
    default:
      return false;
  }
}

static bool HasEnteredCounters(ICEntry& entry) {
  ICStub* stub = entry.firstStub();
  while (stub && !stub->isFallback()) {
    uint32_t count;
    if (GetStubEnteredCount(stub, &count)) {
      return true;
    }
    stub = stub->next();
  }
  return false;
}

void jit::JitSpewBaselineICStats(JSScript* script, const char* dumpReason) {
  MOZ_ASSERT(script->hasJitScript());
  JSContext* cx = TlsContext.get();
  AutoStructuredSpewer spew(cx, SpewChannel::BaselineICStats, script);
  if (!spew) {
    return;
  }

  JitScript* jitScript = script->jitScript();
  spew->property("reason", dumpReason);
  spew->beginListProperty("entries");
  for (size_t i = 0; i < jitScript->numICEntries(); i++) {
    ICEntry& entry = jitScript->icEntry(i);
    if (!HasEnteredCounters(entry)) {
      continue;
    }

    uint32_t pcOffset = entry.pcOffset();
    jsbytecode* pc = entry.pc(script);

    unsigned column;
    unsigned int line = PCToLineNumber(script, pc, &column);

    spew->beginObject();
    spew->property("op", CodeName[*pc]);
    spew->property("pc", pcOffset);
    spew->property("line", line);
    spew->property("column", column);

    spew->beginListProperty("counts");
    ICStub* stub = entry.firstStub();
    while (stub && !stub->isFallback()) {
      uint32_t count;
      if (GetStubEnteredCount(stub, &count)) {
        spew->value(count);
      } else {
        spew->value("?");
      }
      stub = stub->next();
    }
    spew->endList();
    spew->property("fallback_count", entry.fallbackStub()->enteredCount());
    spew->endObject();
  }
  spew->endList();
}
#endif

static void MarkActiveJitScripts(JSContext* cx,
                                 const JitActivationIterator& activation) {
  for (OnlyJSJitFrameIter iter(activation); !iter.done(); ++iter) {
    const JSJitFrameIter& frame = iter.frame();
    switch (frame.type()) {
      case FrameType::BaselineJS:
        frame.script()->jitScript()->setActive();
        break;
      case FrameType::Exit:
        if (frame.exitFrame()->is<LazyLinkExitFrameLayout>()) {
          LazyLinkExitFrameLayout* ll =
              frame.exitFrame()->as<LazyLinkExitFrameLayout>();
          JSScript* script =
              ScriptFromCalleeToken(ll->jsFrame()->calleeToken());
          script->jitScript()->setActive();
        }
        break;
      case FrameType::Bailout:
      case FrameType::IonJS: {
        // Keep the JitScript and BaselineScript around, since bailouts from
        // the ion jitcode need to re-enter into the Baseline code.
        frame.script()->jitScript()->setActive();
        for (InlineFrameIterator inlineIter(cx, &frame); inlineIter.more();
             ++inlineIter) {
          inlineIter.script()->jitScript()->setActive();
        }
        break;
      }
      default:;
    }
  }
}

void jit::MarkActiveJitScripts(Zone* zone) {
  if (zone->isAtomsZone()) {
    return;
  }
  JSContext* cx = TlsContext.get();
  for (JitActivationIterator iter(cx); !iter.done(); ++iter) {
    if (iter->compartment()->zone() == zone) {
      MarkActiveJitScripts(cx, iter);
    }
  }
}
