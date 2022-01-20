/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * JS script operations.
 */

#include "vm/JSScript-inl.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Maybe.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/PodOperations.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/Span.h"  // mozilla::{Span,Span}
#include "mozilla/Sprintf.h"
#include "mozilla/Utf8.h"
#include "mozilla/Vector.h"

#include <algorithm>
#include <new>
#include <string.h>
#include <type_traits>
#include <utility>

#include "jsapi.h"
#include "jstypes.h"

#include "frontend/BytecodeSection.h"
#include "frontend/CompilationStencil.h"  // frontend::CompilationStencil
#include "frontend/ParseContext.h"
#include "frontend/SourceNotes.h"  // SrcNote, SrcNoteType, SrcNoteIterator
#include "frontend/StencilXdr.h"   // XDRStencilEncoder
#include "gc/AllocKind.h"          // gc::InitialHeap
#include "gc/FreeOp.h"
#include "jit/BaselineJIT.h"
#include "jit/CacheIRHealth.h"
#include "jit/Invalidation.h"
#include "jit/Ion.h"
#include "jit/IonScript.h"
#include "jit/JitCode.h"
#include "jit/JitOptions.h"
#include "jit/JitRuntime.h"
#include "js/CompileOptions.h"
#include "js/friend/ErrorMessages.h"  // js::GetErrorMessage, JSMSG_*
#include "js/MemoryMetrics.h"
#include "js/Printf.h"
#include "js/SourceText.h"
#include "js/Transcoding.h"
#include "js/UniquePtr.h"
#include "js/Utility.h"
#include "js/Wrapper.h"
#include "util/Memory.h"
#include "util/Poison.h"
#include "util/StringBuffer.h"
#include "util/Text.h"
#include "vm/ArgumentsObject.h"
#include "vm/BytecodeIterator.h"
#include "vm/BytecodeLocation.h"
#include "vm/BytecodeUtil.h"
#include "vm/Compression.h"
#include "vm/FunctionFlags.h"      // js::FunctionFlags
#include "vm/HelperThreadState.h"  // js::RunPendingSourceCompressions
#include "vm/JSAtom.h"
#include "vm/JSContext.h"
#include "vm/JSFunction.h"
#include "vm/JSObject.h"
#include "vm/Opcodes.h"
#include "vm/PlainObject.h"  // js::PlainObject
#include "vm/SelfHosting.h"
#include "vm/Shape.h"
#include "vm/SharedImmutableStringsCache.h"
#include "vm/Warnings.h"  // js::WarnNumberLatin1
#ifdef MOZ_VTUNE
#  include "vtune/VTuneWrapper.h"
#endif

#include "debugger/DebugAPI-inl.h"
#include "gc/Marking-inl.h"
#include "vm/BytecodeIterator-inl.h"
#include "vm/BytecodeLocation-inl.h"
#include "vm/Compartment-inl.h"
#include "vm/EnvironmentObject-inl.h"
#include "vm/JSFunction-inl.h"
#include "vm/JSObject-inl.h"
#include "vm/NativeObject-inl.h"
#include "vm/SharedImmutableStringsCache-inl.h"
#include "vm/Stack-inl.h"

using namespace js;

using mozilla::CheckedInt;
using mozilla::Maybe;
using mozilla::PodCopy;
using mozilla::PointerRangeSize;
using mozilla::Utf8AsUnsignedChars;
using mozilla::Utf8Unit;

using JS::CompileOptions;
using JS::ReadOnlyCompileOptions;
using JS::SourceText;

bool js::BaseScript::isUsingInterpreterTrampoline(JSRuntime* rt) const {
  return jitCodeRaw() == rt->jitRuntime()->interpreterStub().value;
}

js::ScriptSource* js::BaseScript::maybeForwardedScriptSource() const {
  return MaybeForwarded(sourceObject())->source();
}

void js::BaseScript::setEnclosingScript(BaseScript* enclosingScript) {
  MOZ_ASSERT(enclosingScript);
  warmUpData_.initEnclosingScript(enclosingScript);
}

void js::BaseScript::setEnclosingScope(Scope* enclosingScope) {
  if (warmUpData_.isEnclosingScript()) {
    warmUpData_.clearEnclosingScript();
  }

  MOZ_ASSERT(enclosingScope);
  warmUpData_.initEnclosingScope(enclosingScope);
}

void js::BaseScript::finalize(JSFreeOp* fop) {
  // Scripts with bytecode may have optional data stored in per-runtime or
  // per-zone maps. Note that a failed compilation must not have entries since
  // the script itself will not be marked as having bytecode.
  if (hasBytecode()) {
    JSScript* script = this->asJSScript();

    if (coverage::IsLCovEnabled()) {
      coverage::CollectScriptCoverage(script, true);
    }

    script->destroyScriptCounts();
  }

  fop->runtime()->geckoProfiler().onScriptFinalized(this);

#ifdef MOZ_VTUNE
  if (zone()->scriptVTuneIdMap) {
    // Note: we should only get here if the VTune JIT profiler is running.
    zone()->scriptVTuneIdMap->remove(this);
  }
#endif

  if (warmUpData_.isJitScript()) {
    JSScript* script = this->asJSScript();
#ifdef JS_CACHEIR_SPEW
    maybeUpdateWarmUpCount(script);
#endif
    script->releaseJitScriptOnFinalize(fop);
  }

#ifdef JS_CACHEIR_SPEW
  if (hasBytecode()) {
    maybeSpewScriptFinalWarmUpCount(this->asJSScript());
  }
#endif

  if (data_) {
    // We don't need to triger any barriers here, just free the memory.
    size_t size = data_->allocationSize();
    AlwaysPoison(data_, JS_POISONED_JSSCRIPT_DATA_PATTERN, size,
                 MemCheckKind::MakeNoAccess);
    fop->free_(this, data_, size, MemoryUse::ScriptPrivateData);
  }

  freeSharedData();
}

js::Scope* js::BaseScript::releaseEnclosingScope() {
  Scope* enclosing = warmUpData_.toEnclosingScope();
  warmUpData_.clearEnclosingScope();
  return enclosing;
}

void js::BaseScript::swapData(UniquePtr<PrivateScriptData>& other) {
  PrivateScriptData* tmp = other.release();

  if (data_) {
    // When disconnecting script data from the BaseScript, we must pre-barrier
    // all edges contained in it. Those edges are no longer reachable from
    // current location in the graph.
    PreWriteBarrier(zone(), data_);

    RemoveCellMemory(this, data_->allocationSize(),
                     MemoryUse::ScriptPrivateData);
  }

  std::swap(tmp, data_);

  if (data_) {
    AddCellMemory(this, data_->allocationSize(), MemoryUse::ScriptPrivateData);
  }

  other.reset(tmp);
}

js::Scope* js::BaseScript::enclosingScope() const {
  MOZ_ASSERT(!warmUpData_.isEnclosingScript(),
             "Enclosing scope is not computed yet");

  if (warmUpData_.isEnclosingScope()) {
    return warmUpData_.toEnclosingScope();
  }

  MOZ_ASSERT(data_, "Script doesn't seem to be compiled");

  return gcthings()[js::GCThingIndex::outermostScopeIndex()]
      .as<Scope>()
      .enclosing();
}

size_t JSScript::numAlwaysLiveFixedSlots() const {
  if (bodyScope()->is<js::FunctionScope>()) {
    return bodyScope()->as<js::FunctionScope>().nextFrameSlot();
  }
  if (bodyScope()->is<js::ModuleScope>()) {
    return bodyScope()->as<js::ModuleScope>().nextFrameSlot();
  }
  if (bodyScope()->is<js::EvalScope>() &&
      bodyScope()->kind() == ScopeKind::StrictEval) {
    return bodyScope()->as<js::EvalScope>().nextFrameSlot();
  }
  return 0;
}

unsigned JSScript::numArgs() const {
  if (bodyScope()->is<js::FunctionScope>()) {
    return bodyScope()->as<js::FunctionScope>().numPositionalFormalParameters();
  }
  return 0;
}

bool JSScript::functionHasParameterExprs() const {
  // Only functions have parameters.
  js::Scope* scope = bodyScope();
  if (!scope->is<js::FunctionScope>()) {
    return false;
  }
  return scope->as<js::FunctionScope>().hasParameterExprs();
}

js::ModuleObject* JSScript::module() const {
  if (bodyScope()->is<js::ModuleScope>()) {
    return bodyScope()->as<js::ModuleScope>().module();
  }
  return nullptr;
}

bool JSScript::isGlobalCode() const {
  return bodyScope()->is<js::GlobalScope>();
}

js::VarScope* JSScript::functionExtraBodyVarScope() const {
  MOZ_ASSERT(functionHasExtraBodyVarScope());
  for (JS::GCCellPtr gcThing : gcthings()) {
    if (!gcThing.is<js::Scope>()) {
      continue;
    }
    js::Scope* scope = &gcThing.as<js::Scope>();
    if (scope->kind() == js::ScopeKind::FunctionBodyVar) {
      return &scope->as<js::VarScope>();
    }
  }
  MOZ_CRASH("Function extra body var scope not found");
}

bool JSScript::needsBodyEnvironment() const {
  for (JS::GCCellPtr gcThing : gcthings()) {
    if (!gcThing.is<js::Scope>()) {
      continue;
    }
    js::Scope* scope = &gcThing.as<js::Scope>();
    if (ScopeKindIsInBody(scope->kind()) && scope->hasEnvironment()) {
      return true;
    }
  }
  return false;
}

bool JSScript::isDirectEvalInFunction() const {
  if (!isForEval()) {
    return false;
  }
  return bodyScope()->hasOnChain(js::ScopeKind::Function);
}

// Initialize the optional arrays in the trailing allocation. This is a set of
// offsets that delimit each optional array followed by the arrays themselves.
// See comment before 'ImmutableScriptData' for more details.
void ImmutableScriptData::initOptionalArrays(Offset* pcursor,
                                             uint32_t numResumeOffsets,
                                             uint32_t numScopeNotes,
                                             uint32_t numTryNotes) {
  Offset cursor = (*pcursor);

  // The byte arrays must have already been padded.
  MOZ_ASSERT(isAlignedOffset<CodeNoteAlign>(cursor),
             "Bytecode and source notes should be padded to keep alignment");

  // Each non-empty optional array needs will need an offset to its end.
  unsigned numOptionalArrays = unsigned(numResumeOffsets > 0) +
                               unsigned(numScopeNotes > 0) +
                               unsigned(numTryNotes > 0);

  // Default-initialize the optional-offsets.
  initElements<Offset>(cursor, numOptionalArrays);
  cursor += numOptionalArrays * sizeof(Offset);

  // Offset between optional-offsets table and the optional arrays. This is
  // later used to access the optional-offsets table as well as first optional
  // array.
  optArrayOffset_ = cursor;

  // Each optional array that follows must store an end-offset in the offset
  // table. Assign table entries by using this 'offsetIndex'. The index 0 is
  // reserved for implicit value 'optArrayOffset'.
  int offsetIndex = 0;

  // Default-initialize optional 'resumeOffsets'.
  MOZ_ASSERT(resumeOffsetsOffset() == cursor);
  if (numResumeOffsets > 0) {
    initElements<uint32_t>(cursor, numResumeOffsets);
    cursor += numResumeOffsets * sizeof(uint32_t);
    setOptionalOffset(++offsetIndex, cursor);
  }
  flagsRef().resumeOffsetsEndIndex = offsetIndex;

  // Default-initialize optional 'scopeNotes'.
  MOZ_ASSERT(scopeNotesOffset() == cursor);
  if (numScopeNotes > 0) {
    initElements<ScopeNote>(cursor, numScopeNotes);
    cursor += numScopeNotes * sizeof(ScopeNote);
    setOptionalOffset(++offsetIndex, cursor);
  }
  flagsRef().scopeNotesEndIndex = offsetIndex;

  // Default-initialize optional 'tryNotes'
  MOZ_ASSERT(tryNotesOffset() == cursor);
  if (numTryNotes > 0) {
    initElements<TryNote>(cursor, numTryNotes);
    cursor += numTryNotes * sizeof(TryNote);
    setOptionalOffset(++offsetIndex, cursor);
  }
  flagsRef().tryNotesEndIndex = offsetIndex;

  MOZ_ASSERT(endOffset() == cursor);
  (*pcursor) = cursor;
}

ImmutableScriptData::ImmutableScriptData(uint32_t codeLength,
                                         uint32_t noteLength,
                                         uint32_t numResumeOffsets,
                                         uint32_t numScopeNotes,
                                         uint32_t numTryNotes)
    : codeLength_(codeLength) {
  // Variable-length data begins immediately after ImmutableScriptData itself.
  Offset cursor = sizeof(ImmutableScriptData);

  // The following arrays are byte-aligned with additional padding to ensure
  // that together they maintain uint32_t-alignment.
  {
    MOZ_ASSERT(isAlignedOffset<CodeNoteAlign>(cursor));

    // Zero-initialize 'flags'
    MOZ_ASSERT(isAlignedOffset<Flags>(cursor));
    new (offsetToPointer<void>(cursor)) Flags{};
    cursor += sizeof(Flags);

    initElements<jsbytecode>(cursor, codeLength);
    cursor += codeLength * sizeof(jsbytecode);

    initElements<SrcNote>(cursor, noteLength);
    cursor += noteLength * sizeof(SrcNote);

    MOZ_ASSERT(isAlignedOffset<CodeNoteAlign>(cursor));
  }

  // Initialization for remaining arrays.
  initOptionalArrays(&cursor, numResumeOffsets, numScopeNotes, numTryNotes);

  // Check that we correctly recompute the expected values.
  MOZ_ASSERT(this->codeLength() == codeLength);
  MOZ_ASSERT(this->noteLength() == noteLength);

  // Sanity check
  MOZ_ASSERT(endOffset() == cursor);
}

void js::FillImmutableFlagsFromCompileOptionsForTopLevel(
    const ReadOnlyCompileOptions& options, ImmutableScriptFlags& flags) {
  using ImmutableFlags = ImmutableScriptFlagsEnum;

  js::FillImmutableFlagsFromCompileOptionsForFunction(options, flags);

  flags.setFlag(ImmutableFlags::TreatAsRunOnce, options.isRunOnce);
  flags.setFlag(ImmutableFlags::NoScriptRval, options.noScriptRval);
}

void js::FillImmutableFlagsFromCompileOptionsForFunction(
    const ReadOnlyCompileOptions& options, ImmutableScriptFlags& flags) {
  using ImmutableFlags = ImmutableScriptFlagsEnum;

  flags.setFlag(ImmutableFlags::SelfHosted, options.selfHostingMode);
  flags.setFlag(ImmutableFlags::ForceStrict, options.forceStrictMode());
  flags.setFlag(ImmutableFlags::HasNonSyntacticScope,
                options.nonSyntacticScope);
}

// Check if flags matches to compile options for flags set by
// FillImmutableFlagsFromCompileOptionsForTopLevel above.
//
// If isMultiDecode is true, this check minimal set of CompileOptions that is
// shared across multiple scripts in JS::DecodeMultiOffThreadStencils.
// Other options should be checked when getting the decoded script from the
// cache.
bool js::CheckCompileOptionsMatch(const ReadOnlyCompileOptions& options,
                                  ImmutableScriptFlags flags,
                                  bool isMultiDecode) {
  using ImmutableFlags = ImmutableScriptFlagsEnum;

  bool selfHosted = !!(flags & uint32_t(ImmutableFlags::SelfHosted));
  bool forceStrict = !!(flags & uint32_t(ImmutableFlags::ForceStrict));
  bool hasNonSyntacticScope =
      !!(flags & uint32_t(ImmutableFlags::HasNonSyntacticScope));
  bool noScriptRval = !!(flags & uint32_t(ImmutableFlags::NoScriptRval));
  bool treatAsRunOnce = !!(flags & uint32_t(ImmutableFlags::TreatAsRunOnce));

  return options.selfHostingMode == selfHosted &&
         options.noScriptRval == noScriptRval &&
         options.isRunOnce == treatAsRunOnce &&
         (isMultiDecode || (options.forceStrictMode() == forceStrict &&
                            options.nonSyntacticScope == hasNonSyntacticScope));
}

JS_PUBLIC_API bool JS::CheckCompileOptionsMatch(
    const ReadOnlyCompileOptions& options, JSScript* script) {
  return js::CheckCompileOptionsMatch(options, script->immutableFlags(), false);
}

bool JSScript::initScriptCounts(JSContext* cx) {
  MOZ_ASSERT(!hasScriptCounts());

  // Record all pc which are the first instruction of a basic block.
  mozilla::Vector<jsbytecode*, 16, SystemAllocPolicy> jumpTargets;

  js::BytecodeLocation main = mainLocation();
  AllBytecodesIterable iterable(this);
  for (auto& loc : iterable) {
    if (loc.isJumpTarget() || loc == main) {
      if (!jumpTargets.append(loc.toRawBytecode())) {
        ReportOutOfMemory(cx);
        return false;
      }
    }
  }

  // Initialize all PCCounts counters to 0.
  ScriptCounts::PCCountsVector base;
  if (!base.reserve(jumpTargets.length())) {
    ReportOutOfMemory(cx);
    return false;
  }

  for (size_t i = 0; i < jumpTargets.length(); i++) {
    base.infallibleEmplaceBack(pcToOffset(jumpTargets[i]));
  }

  // Create zone's scriptCountsMap if necessary.
  if (!zone()->scriptCountsMap) {
    auto map = cx->make_unique<ScriptCountsMap>();
    if (!map) {
      return false;
    }

    zone()->scriptCountsMap = std::move(map);
  }

  // Allocate the ScriptCounts.
  UniqueScriptCounts sc = cx->make_unique<ScriptCounts>(std::move(base));
  if (!sc) {
    return false;
  }

  MOZ_ASSERT(this->hasBytecode());

  // Register the current ScriptCounts in the zone's map.
  if (!zone()->scriptCountsMap->putNew(this, std::move(sc))) {
    ReportOutOfMemory(cx);
    return false;
  }

  // safe to set this;  we can't fail after this point.
  setHasScriptCounts();

  // Enable interrupts in any interpreter frames running on this script. This
  // is used to let the interpreter increment the PCCounts, if present.
  for (ActivationIterator iter(cx); !iter.done(); ++iter) {
    if (iter->isInterpreter()) {
      iter->asInterpreter()->enableInterruptsIfRunning(this);
    }
  }

  return true;
}

static inline ScriptCountsMap::Ptr GetScriptCountsMapEntry(JSScript* script) {
  MOZ_ASSERT(script->hasScriptCounts());
  ScriptCountsMap::Ptr p = script->zone()->scriptCountsMap->lookup(script);
  MOZ_ASSERT(p);
  return p;
}

ScriptCounts& JSScript::getScriptCounts() {
  ScriptCountsMap::Ptr p = GetScriptCountsMapEntry(this);
  return *p->value();
}

js::PCCounts* ScriptCounts::maybeGetPCCounts(size_t offset) {
  PCCounts searched = PCCounts(offset);
  PCCounts* elem =
      std::lower_bound(pcCounts_.begin(), pcCounts_.end(), searched);
  if (elem == pcCounts_.end() || elem->pcOffset() != offset) {
    return nullptr;
  }
  return elem;
}

const js::PCCounts* ScriptCounts::maybeGetPCCounts(size_t offset) const {
  PCCounts searched = PCCounts(offset);
  const PCCounts* elem =
      std::lower_bound(pcCounts_.begin(), pcCounts_.end(), searched);
  if (elem == pcCounts_.end() || elem->pcOffset() != offset) {
    return nullptr;
  }
  return elem;
}

js::PCCounts* ScriptCounts::getImmediatePrecedingPCCounts(size_t offset) {
  PCCounts searched = PCCounts(offset);
  PCCounts* elem =
      std::lower_bound(pcCounts_.begin(), pcCounts_.end(), searched);
  if (elem == pcCounts_.end()) {
    return &pcCounts_.back();
  }
  if (elem->pcOffset() == offset) {
    return elem;
  }
  if (elem != pcCounts_.begin()) {
    return elem - 1;
  }
  return nullptr;
}

const js::PCCounts* ScriptCounts::maybeGetThrowCounts(size_t offset) const {
  PCCounts searched = PCCounts(offset);
  const PCCounts* elem =
      std::lower_bound(throwCounts_.begin(), throwCounts_.end(), searched);
  if (elem == throwCounts_.end() || elem->pcOffset() != offset) {
    return nullptr;
  }
  return elem;
}

const js::PCCounts* ScriptCounts::getImmediatePrecedingThrowCounts(
    size_t offset) const {
  PCCounts searched = PCCounts(offset);
  const PCCounts* elem =
      std::lower_bound(throwCounts_.begin(), throwCounts_.end(), searched);
  if (elem == throwCounts_.end()) {
    if (throwCounts_.begin() == throwCounts_.end()) {
      return nullptr;
    }
    return &throwCounts_.back();
  }
  if (elem->pcOffset() == offset) {
    return elem;
  }
  if (elem != throwCounts_.begin()) {
    return elem - 1;
  }
  return nullptr;
}

js::PCCounts* ScriptCounts::getThrowCounts(size_t offset) {
  PCCounts searched = PCCounts(offset);
  PCCounts* elem =
      std::lower_bound(throwCounts_.begin(), throwCounts_.end(), searched);
  if (elem == throwCounts_.end() || elem->pcOffset() != offset) {
    elem = throwCounts_.insert(elem, searched);
  }
  return elem;
}

size_t ScriptCounts::sizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) {
  size_t size = mallocSizeOf(this);
  size += pcCounts_.sizeOfExcludingThis(mallocSizeOf);
  size += throwCounts_.sizeOfExcludingThis(mallocSizeOf);
  if (ionCounts_) {
    size += ionCounts_->sizeOfIncludingThis(mallocSizeOf);
  }
  return size;
}

js::PCCounts* JSScript::maybeGetPCCounts(jsbytecode* pc) {
  MOZ_ASSERT(containsPC(pc));
  return getScriptCounts().maybeGetPCCounts(pcToOffset(pc));
}

const js::PCCounts* JSScript::maybeGetThrowCounts(jsbytecode* pc) {
  MOZ_ASSERT(containsPC(pc));
  return getScriptCounts().maybeGetThrowCounts(pcToOffset(pc));
}

js::PCCounts* JSScript::getThrowCounts(jsbytecode* pc) {
  MOZ_ASSERT(containsPC(pc));
  return getScriptCounts().getThrowCounts(pcToOffset(pc));
}

uint64_t JSScript::getHitCount(jsbytecode* pc) {
  MOZ_ASSERT(containsPC(pc));
  if (pc < main()) {
    pc = main();
  }

  ScriptCounts& sc = getScriptCounts();
  size_t targetOffset = pcToOffset(pc);
  const js::PCCounts* baseCount =
      sc.getImmediatePrecedingPCCounts(targetOffset);
  if (!baseCount) {
    return 0;
  }
  if (baseCount->pcOffset() == targetOffset) {
    return baseCount->numExec();
  }
  MOZ_ASSERT(baseCount->pcOffset() < targetOffset);
  uint64_t count = baseCount->numExec();
  do {
    const js::PCCounts* throwCount =
        sc.getImmediatePrecedingThrowCounts(targetOffset);
    if (!throwCount) {
      return count;
    }
    if (throwCount->pcOffset() <= baseCount->pcOffset()) {
      return count;
    }
    count -= throwCount->numExec();
    targetOffset = throwCount->pcOffset() - 1;
  } while (true);
}

void JSScript::addIonCounts(jit::IonScriptCounts* ionCounts) {
  ScriptCounts& sc = getScriptCounts();
  if (sc.ionCounts_) {
    ionCounts->setPrevious(sc.ionCounts_);
  }
  sc.ionCounts_ = ionCounts;
}

jit::IonScriptCounts* JSScript::getIonCounts() {
  return getScriptCounts().ionCounts_;
}

void JSScript::releaseScriptCounts(ScriptCounts* counts) {
  ScriptCountsMap::Ptr p = GetScriptCountsMapEntry(this);
  *counts = std::move(*p->value().get());
  zone()->scriptCountsMap->remove(p);
  clearHasScriptCounts();
}

void JSScript::destroyScriptCounts() {
  if (hasScriptCounts()) {
    ScriptCounts scriptCounts;
    releaseScriptCounts(&scriptCounts);
  }
}

void JSScript::resetScriptCounts() {
  if (!hasScriptCounts()) {
    return;
  }

  ScriptCounts& sc = getScriptCounts();

  for (PCCounts& elem : sc.pcCounts_) {
    elem.numExec() = 0;
  }

  for (PCCounts& elem : sc.throwCounts_) {
    elem.numExec() = 0;
  }
}

void ScriptSourceObject::finalize(JSFreeOp* fop, JSObject* obj) {
  MOZ_ASSERT(fop->onMainThread());
  ScriptSourceObject* sso = &obj->as<ScriptSourceObject>();
  sso->source()->Release();

  // Clear the private value, calling the release hook if necessary.
  sso->setPrivate(fop->runtime(), UndefinedValue());
}

static const JSClassOps ScriptSourceObjectClassOps = {
    nullptr,                       // addProperty
    nullptr,                       // delProperty
    nullptr,                       // enumerate
    nullptr,                       // newEnumerate
    nullptr,                       // resolve
    nullptr,                       // mayResolve
    ScriptSourceObject::finalize,  // finalize
    nullptr,                       // call
    nullptr,                       // hasInstance
    nullptr,                       // construct
    nullptr,                       // trace
};

const JSClass ScriptSourceObject::class_ = {
    "ScriptSource",
    JSCLASS_HAS_RESERVED_SLOTS(RESERVED_SLOTS) | JSCLASS_FOREGROUND_FINALIZE,
    &ScriptSourceObjectClassOps};

ScriptSourceObject* ScriptSourceObject::create(JSContext* cx,
                                               ScriptSource* source) {
  ScriptSourceObject* obj =
      NewObjectWithGivenProto<ScriptSourceObject>(cx, nullptr);
  if (!obj) {
    return nullptr;
  }

  // The matching decref is in ScriptSourceObject::finalize.
  obj->initReservedSlot(SOURCE_SLOT, PrivateValue(do_AddRef(source).take()));

  // The slots below should be populated by a call to initFromOptions. Poison
  // them.
  obj->initReservedSlot(ELEMENT_PROPERTY_SLOT, MagicValue(JS_GENERIC_MAGIC));
  obj->initReservedSlot(INTRODUCTION_SCRIPT_SLOT, MagicValue(JS_GENERIC_MAGIC));

  return obj;
}

[[nodiscard]] static bool MaybeValidateFilename(
    JSContext* cx, HandleScriptSourceObject sso,
    const JS::InstantiateOptions& options) {
  // When parsing off-thread we want to do filename validation on the main
  // thread. This makes off-thread parsing more pure and is simpler because we
  // can't easily throw exceptions off-thread.
  MOZ_ASSERT(!cx->isHelperThreadContext());

  if (!gFilenameValidationCallback) {
    return true;
  }

  const char* filename = sso->source()->filename();
  if (!filename || options.skipFilenameValidation) {
    return true;
  }

  if (gFilenameValidationCallback(filename, cx->realm()->isSystem())) {
    return true;
  }

  const char* utf8Filename;
  if (mozilla::IsUtf8(mozilla::MakeStringSpan(filename))) {
    utf8Filename = filename;
  } else {
    utf8Filename = "(invalid UTF-8 filename)";
  }
  JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr, JSMSG_UNSAFE_FILENAME,
                           utf8Filename);
  return false;
}

/* static */
bool ScriptSourceObject::initFromOptions(
    JSContext* cx, HandleScriptSourceObject source,
    const JS::InstantiateOptions& options) {
  cx->releaseCheck(source);
  MOZ_ASSERT(
      source->getReservedSlot(ELEMENT_PROPERTY_SLOT).isMagic(JS_GENERIC_MAGIC));
  MOZ_ASSERT(source->getReservedSlot(INTRODUCTION_SCRIPT_SLOT)
                 .isMagic(JS_GENERIC_MAGIC));

  if (!MaybeValidateFilename(cx, source, options)) {
    return false;
  }

  if (options.deferDebugMetadata) {
    return true;
  }

  // Initialize the element attribute slot and introduction script slot
  // this marks the SSO as initialized for asserts.

  RootedString elementAttributeName(cx);
  if (!initElementProperties(cx, source, elementAttributeName)) {
    return false;
  }

  RootedValue introductionScript(cx);
  source->setReservedSlot(INTRODUCTION_SCRIPT_SLOT, introductionScript);

  return true;
}

/* static */
bool ScriptSourceObject::initElementProperties(JSContext* cx,
                                               HandleScriptSourceObject source,
                                               HandleString elementAttrName) {
  RootedValue nameValue(cx);
  if (elementAttrName) {
    nameValue = StringValue(elementAttrName);
  }
  if (!cx->compartment()->wrap(cx, &nameValue)) {
    return false;
  }

  source->setReservedSlot(ELEMENT_PROPERTY_SLOT, nameValue);

  return true;
}

void ScriptSourceObject::setPrivate(JSRuntime* rt, const Value& value) {
  // Update the private value, calling addRef/release hooks if necessary
  // to allow the embedding to maintain a reference count for the
  // private data.
  JS::AutoSuppressGCAnalysis nogc;
  Value prevValue = getReservedSlot(PRIVATE_SLOT);
  rt->releaseScriptPrivate(prevValue);
  setReservedSlot(PRIVATE_SLOT, value);
  rt->addRefScriptPrivate(value);
}

JSObject* ScriptSourceObject::unwrappedElement(JSContext* cx) const {
  JS::RootedValue privateValue(cx, getPrivate());
  if (privateValue.isUndefined()) {
    return nullptr;
  }

  if (cx->runtime()->sourceElementCallback) {
    return (*cx->runtime()->sourceElementCallback)(cx, privateValue);
  }

  return nullptr;
}

class ScriptSource::LoadSourceMatcher {
  JSContext* const cx_;
  ScriptSource* const ss_;
  bool* const loaded_;

 public:
  explicit LoadSourceMatcher(JSContext* cx, ScriptSource* ss, bool* loaded)
      : cx_(cx), ss_(ss), loaded_(loaded) {}

  template <typename Unit, SourceRetrievable CanRetrieve>
  bool operator()(const Compressed<Unit, CanRetrieve>&) const {
    *loaded_ = true;
    return true;
  }

  template <typename Unit, SourceRetrievable CanRetrieve>
  bool operator()(const Uncompressed<Unit, CanRetrieve>&) const {
    *loaded_ = true;
    return true;
  }

  template <typename Unit>
  bool operator()(const Retrievable<Unit>&) {
    if (!cx_->runtime()->sourceHook.ref()) {
      *loaded_ = false;
      return true;
    }

    size_t length;

    // The first argument is just for overloading -- its value doesn't matter.
    if (!tryLoadAndSetSource(Unit('0'), &length)) {
      return false;
    }

    return true;
  }

  bool operator()(const Missing&) const {
    *loaded_ = false;
    return true;
  }

 private:
  bool tryLoadAndSetSource(const Utf8Unit&, size_t* length) const {
    char* utf8Source;
    if (!cx_->runtime()->sourceHook->load(cx_, ss_->filename(), nullptr,
                                          &utf8Source, length)) {
      return false;
    }

    if (!utf8Source) {
      *loaded_ = false;
      return true;
    }

    if (!ss_->setRetrievedSource(
            cx_, EntryUnits<Utf8Unit>(reinterpret_cast<Utf8Unit*>(utf8Source)),
            *length)) {
      return false;
    }

    *loaded_ = true;
    return true;
  }

  bool tryLoadAndSetSource(const char16_t&, size_t* length) const {
    char16_t* utf16Source;
    if (!cx_->runtime()->sourceHook->load(cx_, ss_->filename(), &utf16Source,
                                          nullptr, length)) {
      return false;
    }

    if (!utf16Source) {
      *loaded_ = false;
      return true;
    }

    if (!ss_->setRetrievedSource(cx_, EntryUnits<char16_t>(utf16Source),
                                 *length)) {
      return false;
    }

    *loaded_ = true;
    return true;
  }
};

/* static */
bool ScriptSource::loadSource(JSContext* cx, ScriptSource* ss, bool* loaded) {
  return ss->data.match(LoadSourceMatcher(cx, ss, loaded));
}

/* static */
JSLinearString* JSScript::sourceData(JSContext* cx, HandleScript script) {
  MOZ_ASSERT(script->scriptSource()->hasSourceText());
  return script->scriptSource()->substring(cx, script->sourceStart(),
                                           script->sourceEnd());
}

bool BaseScript::appendSourceDataForToString(JSContext* cx, StringBuffer& buf) {
  MOZ_ASSERT(scriptSource()->hasSourceText());
  return scriptSource()->appendSubstring(cx, buf, toStringStart(),
                                         toStringEnd());
}

void UncompressedSourceCache::holdEntry(AutoHoldEntry& holder,
                                        const ScriptSourceChunk& ssc) {
  MOZ_ASSERT(!holder_);
  holder.holdEntry(this, ssc);
  holder_ = &holder;
}

void UncompressedSourceCache::releaseEntry(AutoHoldEntry& holder) {
  MOZ_ASSERT(holder_ == &holder);
  holder_ = nullptr;
}

template <typename Unit>
const Unit* UncompressedSourceCache::lookup(const ScriptSourceChunk& ssc,
                                            AutoHoldEntry& holder) {
  MOZ_ASSERT(!holder_);
  MOZ_ASSERT(ssc.ss->isCompressed<Unit>());

  if (!map_) {
    return nullptr;
  }

  if (Map::Ptr p = map_->lookup(ssc)) {
    holdEntry(holder, ssc);
    return static_cast<const Unit*>(p->value().get());
  }

  return nullptr;
}

bool UncompressedSourceCache::put(const ScriptSourceChunk& ssc, SourceData data,
                                  AutoHoldEntry& holder) {
  MOZ_ASSERT(!holder_);

  if (!map_) {
    map_ = MakeUnique<Map>();
    if (!map_) {
      return false;
    }
  }

  if (!map_->put(ssc, std::move(data))) {
    return false;
  }

  holdEntry(holder, ssc);
  return true;
}

void UncompressedSourceCache::purge() {
  if (!map_) {
    return;
  }

  for (Map::Range r = map_->all(); !r.empty(); r.popFront()) {
    if (holder_ && r.front().key() == holder_->sourceChunk()) {
      holder_->deferDelete(std::move(r.front().value()));
      holder_ = nullptr;
    }
  }

  map_ = nullptr;
}

size_t UncompressedSourceCache::sizeOfExcludingThis(
    mozilla::MallocSizeOf mallocSizeOf) {
  size_t n = 0;
  if (map_ && !map_->empty()) {
    n += map_->shallowSizeOfIncludingThis(mallocSizeOf);
    for (Map::Range r = map_->all(); !r.empty(); r.popFront()) {
      n += mallocSizeOf(r.front().value().get());
    }
  }
  return n;
}

template <typename Unit>
const Unit* ScriptSource::chunkUnits(
    JSContext* cx, UncompressedSourceCache::AutoHoldEntry& holder,
    size_t chunk) {
  const CompressedData<Unit>& c = *compressedData<Unit>();

  ScriptSourceChunk ssc(this, chunk);
  if (const Unit* decompressed =
          cx->caches().uncompressedSourceCache.lookup<Unit>(ssc, holder)) {
    return decompressed;
  }

  size_t totalLengthInBytes = length() * sizeof(Unit);
  size_t chunkBytes = Compressor::chunkSize(totalLengthInBytes, chunk);

  MOZ_ASSERT((chunkBytes % sizeof(Unit)) == 0);
  const size_t chunkLength = chunkBytes / sizeof(Unit);
  EntryUnits<Unit> decompressed(js_pod_malloc<Unit>(chunkLength));
  if (!decompressed) {
    JS_ReportOutOfMemory(cx);
    return nullptr;
  }

  // Compression treats input and output memory as plain ol' bytes. These
  // reinterpret_cast<>s accord exactly with that.
  if (!DecompressStringChunk(
          reinterpret_cast<const unsigned char*>(c.raw.chars()), chunk,
          reinterpret_cast<unsigned char*>(decompressed.get()), chunkBytes)) {
    JS_ReportOutOfMemory(cx);
    return nullptr;
  }

  const Unit* ret = decompressed.get();
  if (!cx->caches().uncompressedSourceCache.put(
          ssc, ToSourceData(std::move(decompressed)), holder)) {
    JS_ReportOutOfMemory(cx);
    return nullptr;
  }
  return ret;
}

template <typename Unit>
void ScriptSource::convertToCompressedSource(SharedImmutableString compressed,
                                             size_t uncompressedLength) {
  MOZ_ASSERT(isUncompressed<Unit>());
  MOZ_ASSERT(uncompressedData<Unit>()->length() == uncompressedLength);

  if (data.is<Uncompressed<Unit, SourceRetrievable::Yes>>()) {
    data = SourceType(Compressed<Unit, SourceRetrievable::Yes>(
        std::move(compressed), uncompressedLength));
  } else {
    data = SourceType(Compressed<Unit, SourceRetrievable::No>(
        std::move(compressed), uncompressedLength));
  }
}

template <typename Unit>
void ScriptSource::performDelayedConvertToCompressedSource() {
  // There might not be a conversion to compressed source happening at all.
  if (pendingCompressed_.empty()) {
    return;
  }

  CompressedData<Unit>& pending =
      pendingCompressed_.ref<CompressedData<Unit>>();

  convertToCompressedSource<Unit>(std::move(pending.raw),
                                  pending.uncompressedLength);

  pendingCompressed_.destroy();
}

template <typename Unit>
ScriptSource::PinnedUnits<Unit>::~PinnedUnits() {
  if (units_) {
    MOZ_ASSERT(*stack_ == this);
    *stack_ = prev_;
    if (!prev_) {
      source_->performDelayedConvertToCompressedSource<Unit>();
    }
  }
}

template <typename Unit>
const Unit* ScriptSource::units(JSContext* cx,
                                UncompressedSourceCache::AutoHoldEntry& holder,
                                size_t begin, size_t len) {
  MOZ_ASSERT(begin <= length());
  MOZ_ASSERT(begin + len <= length());

  if (isUncompressed<Unit>()) {
    const Unit* units = uncompressedData<Unit>()->units();
    if (!units) {
      return nullptr;
    }
    return units + begin;
  }

  if (data.is<Missing>()) {
    MOZ_CRASH("ScriptSource::units() on ScriptSource with missing source");
  }

  if (data.is<Retrievable<Unit>>()) {
    MOZ_CRASH("ScriptSource::units() on ScriptSource with retrievable source");
  }

  MOZ_ASSERT(isCompressed<Unit>());

  // Determine first/last chunks, the offset (in bytes) into the first chunk
  // of the requested units, and the number of bytes in the last chunk.
  //
  // Note that first and last chunk sizes are miscomputed and *must not be
  // used* when the first chunk is the last chunk.
  size_t firstChunk, firstChunkOffset, firstChunkSize;
  size_t lastChunk, lastChunkSize;
  Compressor::rangeToChunkAndOffset(
      begin * sizeof(Unit), (begin + len) * sizeof(Unit), &firstChunk,
      &firstChunkOffset, &firstChunkSize, &lastChunk, &lastChunkSize);
  MOZ_ASSERT(firstChunk <= lastChunk);
  MOZ_ASSERT(firstChunkOffset % sizeof(Unit) == 0);
  MOZ_ASSERT(firstChunkSize % sizeof(Unit) == 0);

  size_t firstUnit = firstChunkOffset / sizeof(Unit);

  // Directly return units within a single chunk.  UncompressedSourceCache
  // and |holder| will hold the units alive past function return.
  if (firstChunk == lastChunk) {
    const Unit* units = chunkUnits<Unit>(cx, holder, firstChunk);
    if (!units) {
      return nullptr;
    }

    return units + firstUnit;
  }

  // Otherwise the units span multiple chunks.  Copy successive chunks'
  // decompressed units into freshly-allocated memory to return.
  EntryUnits<Unit> decompressed(js_pod_malloc<Unit>(len));
  if (!decompressed) {
    JS_ReportOutOfMemory(cx);
    return nullptr;
  }

  Unit* cursor;

  {
    // |AutoHoldEntry| is single-shot, and a holder successfully filled in
    // by |chunkUnits| must be destroyed before another can be used.  Thus
    // we can't use |holder| with |chunkUnits| when |chunkUnits| is used
    // with multiple chunks, and we must use and destroy distinct, fresh
    // holders for each chunk.
    UncompressedSourceCache::AutoHoldEntry firstHolder;
    const Unit* units = chunkUnits<Unit>(cx, firstHolder, firstChunk);
    if (!units) {
      return nullptr;
    }

    cursor = std::copy_n(units + firstUnit, firstChunkSize / sizeof(Unit),
                         decompressed.get());
  }

  for (size_t i = firstChunk + 1; i < lastChunk; i++) {
    UncompressedSourceCache::AutoHoldEntry chunkHolder;
    const Unit* units = chunkUnits<Unit>(cx, chunkHolder, i);
    if (!units) {
      return nullptr;
    }

    cursor = std::copy_n(units, Compressor::CHUNK_SIZE / sizeof(Unit), cursor);
  }

  {
    UncompressedSourceCache::AutoHoldEntry lastHolder;
    const Unit* units = chunkUnits<Unit>(cx, lastHolder, lastChunk);
    if (!units) {
      return nullptr;
    }

    cursor = std::copy_n(units, lastChunkSize / sizeof(Unit), cursor);
  }

  MOZ_ASSERT(PointerRangeSize(decompressed.get(), cursor) == len);

  // Transfer ownership to |holder|.
  const Unit* ret = decompressed.get();
  holder.holdUnits(std::move(decompressed));
  return ret;
}

template <typename Unit>
ScriptSource::PinnedUnits<Unit>::PinnedUnits(
    JSContext* cx, ScriptSource* source,
    UncompressedSourceCache::AutoHoldEntry& holder, size_t begin, size_t len)
    : PinnedUnitsBase(source) {
  MOZ_ASSERT(source->hasSourceType<Unit>(), "must pin units of source's type");

  units_ = source->units<Unit>(cx, holder, begin, len);
  if (units_) {
    stack_ = &source->pinnedUnitsStack_;
    prev_ = *stack_;
    *stack_ = this;
  }
}

template class ScriptSource::PinnedUnits<Utf8Unit>;
template class ScriptSource::PinnedUnits<char16_t>;

JSLinearString* ScriptSource::substring(JSContext* cx, size_t start,
                                        size_t stop) {
  MOZ_ASSERT(start <= stop);

  size_t len = stop - start;
  if (!len) {
    return cx->emptyString();
  }
  UncompressedSourceCache::AutoHoldEntry holder;

  // UTF-8 source text.
  if (hasSourceType<Utf8Unit>()) {
    PinnedUnits<Utf8Unit> units(cx, this, holder, start, len);
    if (!units.asChars()) {
      return nullptr;
    }

    const char* str = units.asChars();
    return NewStringCopyUTF8N<CanGC>(cx, JS::UTF8Chars(str, len));
  }

  // UTF-16 source text.
  PinnedUnits<char16_t> units(cx, this, holder, start, len);
  if (!units.asChars()) {
    return nullptr;
  }

  return NewStringCopyN<CanGC>(cx, units.asChars(), len);
}

JSLinearString* ScriptSource::substringDontDeflate(JSContext* cx, size_t start,
                                                   size_t stop) {
  MOZ_ASSERT(start <= stop);

  size_t len = stop - start;
  if (!len) {
    return cx->emptyString();
  }
  UncompressedSourceCache::AutoHoldEntry holder;

  // UTF-8 source text.
  if (hasSourceType<Utf8Unit>()) {
    PinnedUnits<Utf8Unit> units(cx, this, holder, start, len);
    if (!units.asChars()) {
      return nullptr;
    }

    const char* str = units.asChars();

    // There doesn't appear to be a non-deflating UTF-8 string creation
    // function -- but then again, it's not entirely clear how current
    // callers benefit from non-deflation.
    return NewStringCopyUTF8N<CanGC>(cx, JS::UTF8Chars(str, len));
  }

  // UTF-16 source text.
  PinnedUnits<char16_t> units(cx, this, holder, start, len);
  if (!units.asChars()) {
    return nullptr;
  }

  return NewStringCopyNDontDeflate<CanGC>(cx, units.asChars(), len);
}

bool ScriptSource::appendSubstring(JSContext* cx, StringBuffer& buf,
                                   size_t start, size_t stop) {
  MOZ_ASSERT(start <= stop);

  size_t len = stop - start;
  UncompressedSourceCache::AutoHoldEntry holder;

  if (hasSourceType<Utf8Unit>()) {
    PinnedUnits<Utf8Unit> pinned(cx, this, holder, start, len);
    if (!pinned.get()) {
      return false;
    }
    if (len > SourceDeflateLimit && !buf.ensureTwoByteChars()) {
      return false;
    }

    const Utf8Unit* units = pinned.get();
    return buf.append(units, len);
  } else {
    PinnedUnits<char16_t> pinned(cx, this, holder, start, len);
    if (!pinned.get()) {
      return false;
    }
    if (len > SourceDeflateLimit && !buf.ensureTwoByteChars()) {
      return false;
    }

    const char16_t* units = pinned.get();
    return buf.append(units, len);
  }
}

JSLinearString* ScriptSource::functionBodyString(JSContext* cx) {
  MOZ_ASSERT(isFunctionBody());

  size_t start =
      parameterListEnd_ + (sizeof(FunctionConstructorMedialSigils) - 1);
  size_t stop = length() - (sizeof(FunctionConstructorFinalBrace) - 1);
  return substring(cx, start, stop);
}

template <typename Unit>
[[nodiscard]] bool ScriptSource::setUncompressedSourceHelper(
    JSContext* cx, EntryUnits<Unit>&& source, size_t length,
    SourceRetrievable retrievable) {
  auto& cache = cx->runtime()->sharedImmutableStrings();

  auto uniqueChars = SourceTypeTraits<Unit>::toCacheable(std::move(source));
  auto deduped = cache.getOrCreate(std::move(uniqueChars), length);
  if (!deduped) {
    ReportOutOfMemory(cx);
    return false;
  }

  if (retrievable == SourceRetrievable::Yes) {
    data = SourceType(
        Uncompressed<Unit, SourceRetrievable::Yes>(std::move(deduped)));
  } else {
    data = SourceType(
        Uncompressed<Unit, SourceRetrievable::No>(std::move(deduped)));
  }
  return true;
}

template <typename Unit>
[[nodiscard]] bool ScriptSource::setRetrievedSource(JSContext* cx,
                                                    EntryUnits<Unit>&& source,
                                                    size_t length) {
  MOZ_ASSERT(data.is<Retrievable<Unit>>(),
             "retrieved source can only overwrite the corresponding "
             "retrievable source");
  return setUncompressedSourceHelper(cx, std::move(source), length,
                                     SourceRetrievable::Yes);
}

bool js::IsOffThreadSourceCompressionEnabled() {
  // If we don't have concurrent execution compression will contend with
  // main-thread execution, in which case we disable. Similarly we don't want to
  // block the thread pool if it is too small.
  return GetHelperThreadCPUCount() > 1 && GetHelperThreadCount() > 1 &&
         CanUseExtraThreads();
}

bool ScriptSource::tryCompressOffThread(JSContext* cx) {
  // Beware: |js::SynchronouslyCompressSource| assumes that this function is
  // only called once, just after a script has been compiled, and it's never
  // called at some random time after that.  If multiple calls of this can ever
  // occur, that function may require changes.

  // The SourceCompressionTask needs to record the major GC number for
  // scheduling. This cannot be accessed off-thread and must be handle in
  // ParseTask::finish instead.
  MOZ_ASSERT(!cx->isHelperThreadContext());
  MOZ_ASSERT(CurrentThreadCanAccessRuntime(cx->runtime()));

  // If source compression was already attempted, do not queue a new task.
  if (hadCompressionTask_) {
    return true;
  }

  if (!hasUncompressedSource()) {
    // This excludes compressed, missing, and retrievable source.
    return true;
  }

  // There are several cases where source compression is not a good idea:
  //  - If the script is tiny, then compression will save little or no space.
  //  - If there is only one core, then compression will contend with JS
  //    execution (which hurts benchmarketing).
  //
  // Otherwise, enqueue a compression task to be processed when a major
  // GC is requested.

  if (length() < ScriptSource::MinimumCompressibleLength ||
      !IsOffThreadSourceCompressionEnabled()) {
    return true;
  }

  // Heap allocate the task. It will be freed upon compression
  // completing in AttachFinishedCompressedSources.
  auto task = MakeUnique<SourceCompressionTask>(cx->runtime(), this);
  if (!task) {
    ReportOutOfMemory(cx);
    return false;
  }
  return EnqueueOffThreadCompression(cx, std::move(task));
}

template <typename Unit>
void ScriptSource::triggerConvertToCompressedSource(
    SharedImmutableString compressed, size_t uncompressedLength) {
  MOZ_ASSERT(isUncompressed<Unit>(),
             "should only be triggering compressed source installation to "
             "overwrite identically-encoded uncompressed source");
  MOZ_ASSERT(uncompressedData<Unit>()->length() == uncompressedLength);

  // If units aren't pinned -- and they probably won't be, we'd have to have a
  // GC in the small window of time where a |PinnedUnits| was live -- then we
  // can immediately convert.
  if (MOZ_LIKELY(!pinnedUnitsStack_)) {
    convertToCompressedSource<Unit>(std::move(compressed), uncompressedLength);
    return;
  }

  // Otherwise, set aside the compressed-data info.  The conversion is performed
  // when the last |PinnedUnits| dies.
  MOZ_ASSERT(pendingCompressed_.empty(),
             "shouldn't be multiple conversions happening");
  pendingCompressed_.construct<CompressedData<Unit>>(std::move(compressed),
                                                     uncompressedLength);
}

template <typename Unit>
[[nodiscard]] bool ScriptSource::initializeWithUnretrievableCompressedSource(
    JSContext* cx, UniqueChars&& compressed, size_t rawLength,
    size_t sourceLength) {
  MOZ_ASSERT(data.is<Missing>(), "shouldn't be double-initializing");
  MOZ_ASSERT(compressed != nullptr);

  auto& cache = cx->runtime()->sharedImmutableStrings();
  auto deduped = cache.getOrCreate(std::move(compressed), rawLength);
  if (!deduped) {
    ReportOutOfMemory(cx);
    return false;
  }

  MOZ_ASSERT(pinnedUnitsStack_ == nullptr,
             "shouldn't be initializing a ScriptSource while its characters "
             "are pinned -- that only makes sense with a ScriptSource actively "
             "being inspected");

  data = SourceType(Compressed<Unit, SourceRetrievable::No>(std::move(deduped),
                                                            sourceLength));

  return true;
}

template bool ScriptSource::initializeWithUnretrievableCompressedSource<
    Utf8Unit>(JSContext* cx, UniqueChars&& compressed, size_t rawLength,
              size_t sourceLength);
template bool ScriptSource::initializeWithUnretrievableCompressedSource<
    char16_t>(JSContext* cx, UniqueChars&& compressed, size_t rawLength,
              size_t sourceLength);

template <typename Unit>
bool ScriptSource::assignSource(JSContext* cx,
                                const ReadOnlyCompileOptions& options,
                                SourceText<Unit>& srcBuf) {
  MOZ_ASSERT(data.is<Missing>(),
             "source assignment should only occur on fresh ScriptSources");

  if (options.discardSource) {
    return true;
  }

  if (options.sourceIsLazy) {
    data = SourceType(Retrievable<Unit>());
    return true;
  }

  JSRuntime* runtime = cx->runtime();
  auto& cache = runtime->sharedImmutableStrings();
  auto deduped = cache.getOrCreate(srcBuf.get(), srcBuf.length(), [&srcBuf]() {
    using CharT = typename SourceTypeTraits<Unit>::CharT;
    return srcBuf.ownsUnits()
               ? UniquePtr<CharT[], JS::FreePolicy>(srcBuf.takeChars())
               : DuplicateString(srcBuf.get(), srcBuf.length());
  });
  if (!deduped) {
    ReportOutOfMemory(cx);
    return false;
  }

  data =
      SourceType(Uncompressed<Unit, SourceRetrievable::No>(std::move(deduped)));
  return true;
}

template bool ScriptSource::assignSource(JSContext* cx,
                                         const ReadOnlyCompileOptions& options,
                                         SourceText<char16_t>& srcBuf);
template bool ScriptSource::assignSource(JSContext* cx,
                                         const ReadOnlyCompileOptions& options,
                                         SourceText<Utf8Unit>& srcBuf);

[[nodiscard]] static bool reallocUniquePtr(UniqueChars& unique, size_t size) {
  auto newPtr = static_cast<char*>(js_realloc(unique.get(), size));
  if (!newPtr) {
    return false;
  }

  // Since the realloc succeeded, unique is now holding a freed pointer.
  (void)unique.release();
  unique.reset(newPtr);
  return true;
}

template <typename Unit>
void SourceCompressionTask::workEncodingSpecific() {
  MOZ_ASSERT(source_->isUncompressed<Unit>());

  // Try to keep the maximum memory usage down by only allocating half the
  // size of the string, first.
  size_t inputBytes = source_->length() * sizeof(Unit);
  size_t firstSize = inputBytes / 2;
  UniqueChars compressed(js_pod_malloc<char>(firstSize));
  if (!compressed) {
    return;
  }

  const Unit* chars = source_->uncompressedData<Unit>()->units();
  Compressor comp(reinterpret_cast<const unsigned char*>(chars), inputBytes);
  if (!comp.init()) {
    return;
  }

  comp.setOutput(reinterpret_cast<unsigned char*>(compressed.get()), firstSize);
  bool cont = true;
  bool reallocated = false;
  while (cont) {
    if (shouldCancel()) {
      return;
    }

    switch (comp.compressMore()) {
      case Compressor::CONTINUE:
        break;
      case Compressor::MOREOUTPUT: {
        if (reallocated) {
          // The compressed string is longer than the original string.
          return;
        }

        // The compressed output is greater than half the size of the
        // original string. Reallocate to the full size.
        if (!reallocUniquePtr(compressed, inputBytes)) {
          return;
        }

        comp.setOutput(reinterpret_cast<unsigned char*>(compressed.get()),
                       inputBytes);
        reallocated = true;
        break;
      }
      case Compressor::DONE:
        cont = false;
        break;
      case Compressor::OOM:
        return;
    }
  }

  size_t totalBytes = comp.totalBytesNeeded();

  // Shrink the buffer to the size of the compressed data.
  if (!reallocUniquePtr(compressed, totalBytes)) {
    return;
  }

  comp.finish(compressed.get(), totalBytes);

  if (shouldCancel()) {
    return;
  }

  auto& strings = runtime_->sharedImmutableStrings();
  resultString_ = strings.getOrCreate(std::move(compressed), totalBytes);
}

struct SourceCompressionTask::PerformTaskWork {
  SourceCompressionTask* const task_;

  explicit PerformTaskWork(SourceCompressionTask* task) : task_(task) {}

  template <typename Unit, SourceRetrievable CanRetrieve>
  void operator()(const ScriptSource::Uncompressed<Unit, CanRetrieve>&) {
    task_->workEncodingSpecific<Unit>();
  }

  template <typename T>
  void operator()(const T&) {
    MOZ_CRASH(
        "why are we compressing missing, missing-but-retrievable, "
        "or already-compressed source?");
  }
};

void ScriptSource::performTaskWork(SourceCompressionTask* task) {
  MOZ_ASSERT(hasUncompressedSource());
  data.match(SourceCompressionTask::PerformTaskWork(task));
}

void SourceCompressionTask::runTask() {
  if (shouldCancel()) {
    return;
  }

  TraceLoggerThread* logger = TraceLoggerForCurrentThread();
  AutoTraceLog logCompile(logger, TraceLogger_CompressSource);

  MOZ_ASSERT(source_->hasUncompressedSource());

  source_->performTaskWork(this);
}

void SourceCompressionTask::runHelperThreadTask(
    AutoLockHelperThreadState& locked) {
  {
    AutoUnlockHelperThreadState unlock(locked);
    this->runTask();
  }

  {
    AutoEnterOOMUnsafeRegion oomUnsafe;
    if (!HelperThreadState().compressionFinishedList(locked).append(this)) {
      oomUnsafe.crash("SourceCompressionTask::runHelperThreadTask");
    }
  }
}

void ScriptSource::triggerConvertToCompressedSourceFromTask(
    SharedImmutableString compressed) {
  data.match(TriggerConvertToCompressedSourceFromTask(this, compressed));
}

void SourceCompressionTask::complete() {
  if (!shouldCancel() && resultString_) {
    source_->triggerConvertToCompressedSourceFromTask(std::move(resultString_));
  }
}

bool js::SynchronouslyCompressSource(JSContext* cx,
                                     JS::Handle<BaseScript*> script) {
  MOZ_ASSERT(!cx->isHelperThreadContext(),
             "should only sync-compress on the main thread");

  // Finish all pending source compressions, including the single compression
  // task that may have been created (by |ScriptSource::tryCompressOffThread|)
  // just after the script was compiled.  Because we have flushed this queue,
  // no code below needs to synchronize with an off-thread parse task that
  // assumes the immutability of a |ScriptSource|'s data.
  //
  // This *may* end up compressing |script|'s source.  If it does -- we test
  // this below -- that takes care of things.  But if it doesn't, we will
  // synchronously compress ourselves (and as noted above, this won't race
  // anything).
  RunPendingSourceCompressions(cx->runtime());

  ScriptSource* ss = script->scriptSource();
  MOZ_ASSERT(!ss->pinnedUnitsStack_,
             "can't synchronously compress while source units are in use");

  // In principle a previously-triggered compression on a helper thread could
  // have already completed.  If that happens, there's nothing more to do.
  if (ss->hasCompressedSource()) {
    return true;
  }

  MOZ_ASSERT(ss->hasUncompressedSource(),
             "shouldn't be compressing uncompressible source");

  // Use an explicit scope to delineate the lifetime of |task|, for simplicity.
  {
#ifdef DEBUG
    uint32_t sourceRefs = ss->refs;
#endif
    MOZ_ASSERT(sourceRefs > 0, "at least |script| here should have a ref");

    // |SourceCompressionTask::shouldCancel| can periodically result in source
    // compression being canceled if we're not careful.  Guarantee that two refs
    // to |ss| are always live in this function (at least one preexisting and
    // one held by the task) so that compression is never canceled.
    auto task = MakeUnique<SourceCompressionTask>(cx->runtime(), ss);
    if (!task) {
      ReportOutOfMemory(cx);
      return false;
    }

    MOZ_ASSERT(ss->refs > sourceRefs, "must have at least two refs now");

    // Attempt to compress.  This may not succeed if OOM happens, but (because
    // it ordinarily happens on a helper thread) no error will ever be set here.
    MOZ_ASSERT(!cx->isExceptionPending());
    ss->performTaskWork(task.get());
    MOZ_ASSERT(!cx->isExceptionPending());

    // Convert |ss| from uncompressed to compressed data.
    task->complete();

    MOZ_ASSERT(!cx->isExceptionPending());
  }

  // The only way source won't be compressed here is if OOM happened.
  return ss->hasCompressedSource();
}

void ScriptSource::addSizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf,
                                          JS::ScriptSourceInfo* info) const {
  info->misc += mallocSizeOf(this);
  info->numScripts++;
}

bool ScriptSource::startIncrementalEncoding(
    JSContext* cx,
    UniquePtr<frontend::ExtensibleCompilationStencil>&& initial) {
  // We don't support asm.js in XDR.
  // Encoding failures are reported by the xdrFinalizeEncoder function.
  if (initial->asmJS) {
    return true;
  }

  // Remove the reference to the source, to avoid the circular reference.
  initial->source = nullptr;

  AutoIncrementalTimer timer(cx->realm()->timers.xdrEncodingTime);
  auto failureCase = mozilla::MakeScopeExit([&] { xdrEncoder_.reset(); });

  if (!xdrEncoder_.setInitial(
          cx, std::forward<UniquePtr<frontend::ExtensibleCompilationStencil>>(
                  initial))) {
    // On encoding failure, let failureCase destroy encoder and return true
    // to avoid failing any currently executing script.
    return false;
  }

  failureCase.release();
  return true;
}

bool ScriptSource::addDelazificationToIncrementalEncoding(
    JSContext* cx, const frontend::CompilationStencil& stencil) {
  MOZ_ASSERT(hasEncoder());
  AutoIncrementalTimer timer(cx->realm()->timers.xdrEncodingTime);
  auto failureCase = mozilla::MakeScopeExit([&] { xdrEncoder_.reset(); });

  if (!xdrEncoder_.addDelazification(cx, stencil)) {
    // On encoding failure, let failureCase destroy encoder and return true
    // to avoid failing any currently executing script.
    return false;
  }

  failureCase.release();
  return true;
}

bool ScriptSource::xdrFinalizeEncoder(JSContext* cx,
                                      JS::TranscodeBuffer& buffer) {
  if (!hasEncoder()) {
    JS_ReportErrorASCII(cx, "XDR encoding failure");
    return false;
  }

  auto cleanup = mozilla::MakeScopeExit([&] { xdrEncoder_.reset(); });

  XDRStencilEncoder encoder(cx, buffer);

  frontend::BorrowingCompilationStencil borrowingStencil(
      xdrEncoder_.merger_->getResult());
  XDRResult res = encoder.codeStencil(this, borrowingStencil);
  if (res.isErr()) {
    if (JS::IsTranscodeFailureResult(res.unwrapErr())) {
      JS_ReportErrorASCII(cx, "XDR encoding failure");
    }
    return false;
  }
  return true;
}

template <typename Unit>
[[nodiscard]] bool ScriptSource::initializeUnretrievableUncompressedSource(
    JSContext* cx, EntryUnits<Unit>&& source, size_t length) {
  MOZ_ASSERT(data.is<Missing>(), "must be initializing a fresh ScriptSource");
  return setUncompressedSourceHelper(cx, std::move(source), length,
                                     SourceRetrievable::No);
}

template bool ScriptSource::initializeUnretrievableUncompressedSource(
    JSContext* cx, EntryUnits<Utf8Unit>&& source, size_t length);
template bool ScriptSource::initializeUnretrievableUncompressedSource(
    JSContext* cx, EntryUnits<char16_t>&& source, size_t length);

// Format and return a cx->pod_malloc'ed URL for a generated script like:
//   {filename} line {lineno} > {introducer}
// For example:
//   foo.js line 7 > eval
// indicating code compiled by the call to 'eval' on line 7 of foo.js.
UniqueChars js::FormatIntroducedFilename(JSContext* cx, const char* filename,
                                         unsigned lineno,
                                         const char* introducer) {
  // Compute the length of the string in advance, so we can allocate a
  // buffer of the right size on the first shot.
  //
  // (JS_smprintf would be perfect, as that allocates the result
  // dynamically as it formats the string, but it won't allocate from cx,
  // and wants us to use a special free function.)
  char linenoBuf[15];
  size_t filenameLen = strlen(filename);
  size_t linenoLen = SprintfLiteral(linenoBuf, "%u", lineno);
  size_t introducerLen = strlen(introducer);
  size_t len = filenameLen + 6 /* == strlen(" line ") */ + linenoLen +
               3 /* == strlen(" > ") */ + introducerLen + 1 /* \0 */;
  UniqueChars formatted(cx->pod_malloc<char>(len));
  if (!formatted) {
    return nullptr;
  }

  mozilla::DebugOnly<size_t> checkLen = snprintf(
      formatted.get(), len, "%s line %s > %s", filename, linenoBuf, introducer);
  MOZ_ASSERT(checkLen == len - 1);

  return formatted;
}

bool ScriptSource::initFromOptions(JSContext* cx,
                                   const ReadOnlyCompileOptions& options) {
  MOZ_ASSERT(!filename_);
  MOZ_ASSERT(!introducerFilename_);

  mutedErrors_ = options.mutedErrors();

  startLine_ = options.lineno;
  introductionType_ = options.introductionType;
  setIntroductionOffset(options.introductionOffset);
  // The parameterListEnd_ is initialized later by setParameterListEnd, before
  // we expose any scripts that use this ScriptSource to the debugger.

  if (options.hasIntroductionInfo) {
    MOZ_ASSERT(options.introductionType != nullptr);
    const char* filename =
        options.filename() ? options.filename() : "<unknown>";
    UniqueChars formatted = FormatIntroducedFilename(
        cx, filename, options.introductionLineno, options.introductionType);
    if (!formatted) {
      return false;
    }
    if (!setFilename(cx, std::move(formatted))) {
      return false;
    }
  } else if (options.filename()) {
    if (!setFilename(cx, options.filename())) {
      return false;
    }
  }

  if (options.introducerFilename()) {
    if (!setIntroducerFilename(cx, options.introducerFilename())) {
      return false;
    }
  }

  return true;
}

// Use the SharedImmutableString map to deduplicate input string. The input
// string must be null-terminated.
template <typename SharedT, typename CharT>
static SharedT GetOrCreateStringZ(JSContext* cx,
                                  UniquePtr<CharT[], JS::FreePolicy>&& str) {
  JSRuntime* rt = cx->runtime();
  size_t lengthWithNull = std::char_traits<CharT>::length(str.get()) + 1;
  auto res =
      rt->sharedImmutableStrings().getOrCreate(std::move(str), lengthWithNull);
  if (!res) {
    ReportOutOfMemory(cx);
  }
  return res;
}

SharedImmutableString ScriptSource::getOrCreateStringZ(JSContext* cx,
                                                       UniqueChars&& str) {
  return GetOrCreateStringZ<SharedImmutableString>(cx, std::move(str));
}

SharedImmutableTwoByteString ScriptSource::getOrCreateStringZ(
    JSContext* cx, UniqueTwoByteChars&& str) {
  return GetOrCreateStringZ<SharedImmutableTwoByteString>(cx, std::move(str));
}

bool ScriptSource::setFilename(JSContext* cx, const char* filename) {
  UniqueChars owned = DuplicateString(cx, filename);
  if (!owned) {
    return false;
  }
  return setFilename(cx, std::move(owned));
}

bool ScriptSource::setFilename(JSContext* cx, UniqueChars&& filename) {
  MOZ_ASSERT(!filename_);
  filename_ = getOrCreateStringZ(cx, std::move(filename));
  return bool(filename_);
}

bool ScriptSource::setIntroducerFilename(JSContext* cx, const char* filename) {
  UniqueChars owned = DuplicateString(cx, filename);
  if (!owned) {
    return false;
  }
  return setIntroducerFilename(cx, std::move(owned));
}

bool ScriptSource::setIntroducerFilename(JSContext* cx,
                                         UniqueChars&& filename) {
  MOZ_ASSERT(!introducerFilename_);
  introducerFilename_ = getOrCreateStringZ(cx, std::move(filename));
  return bool(introducerFilename_);
}

bool ScriptSource::setDisplayURL(JSContext* cx, const char16_t* url) {
  UniqueTwoByteChars owned = DuplicateString(cx, url);
  if (!owned) {
    return false;
  }
  return setDisplayURL(cx, std::move(owned));
}

bool ScriptSource::setDisplayURL(JSContext* cx, UniqueTwoByteChars&& url) {
  if (hasDisplayURL()) {
    // FIXME: filename() should be UTF-8 (bug 987069).
    if (!cx->isHelperThreadContext() &&
        !WarnNumberLatin1(cx, JSMSG_ALREADY_HAS_PRAGMA, filename(),
                          "//# sourceURL")) {
      return false;
    }
  }

  MOZ_ASSERT(url);
  if (url[0] == '\0') {
    return true;
  }

  displayURL_ = getOrCreateStringZ(cx, std::move(url));
  return bool(displayURL_);
}

bool ScriptSource::setSourceMapURL(JSContext* cx, const char16_t* url) {
  UniqueTwoByteChars owned = DuplicateString(cx, url);
  if (!owned) {
    return false;
  }
  return setSourceMapURL(cx, std::move(owned));
}

bool ScriptSource::setSourceMapURL(JSContext* cx, UniqueTwoByteChars&& url) {
  MOZ_ASSERT(url);
  if (url[0] == '\0') {
    return true;
  }

  sourceMapURL_ = getOrCreateStringZ(cx, std::move(url));
  return bool(sourceMapURL_);
}

/* static */ mozilla::Atomic<uint32_t, mozilla::SequentiallyConsistent>
    ScriptSource::idCount_;

/*
 * [SMDOC] JSScript data layout (immutable)
 *
 * Script data that shareable across processes. There are no pointers (GC or
 * otherwise) and the data is relocatable.
 *
 * Array elements   Pointed to by         Length
 * --------------   -------------         ------
 * jsbytecode       code()                codeLength()
 * jsscrnote        notes()               noteLength()
 * uint32_t         resumeOffsets()
 * ScopeNote        scopeNotes()
 * TryNote          tryNotes()
 */

/* static */ CheckedInt<uint32_t> ImmutableScriptData::sizeFor(
    uint32_t codeLength, uint32_t noteLength, uint32_t numResumeOffsets,
    uint32_t numScopeNotes, uint32_t numTryNotes) {
  // Take a count of which optional arrays will be used and need offset info.
  unsigned numOptionalArrays = unsigned(numResumeOffsets > 0) +
                               unsigned(numScopeNotes > 0) +
                               unsigned(numTryNotes > 0);

  // Compute size including trailing arrays.
  CheckedInt<uint32_t> size = sizeof(ImmutableScriptData);
  size += sizeof(Flags);
  size += CheckedInt<uint32_t>(codeLength) * sizeof(jsbytecode);
  size += CheckedInt<uint32_t>(noteLength) * sizeof(SrcNote);
  size += CheckedInt<uint32_t>(numOptionalArrays) * sizeof(Offset);
  size += CheckedInt<uint32_t>(numResumeOffsets) * sizeof(uint32_t);
  size += CheckedInt<uint32_t>(numScopeNotes) * sizeof(ScopeNote);
  size += CheckedInt<uint32_t>(numTryNotes) * sizeof(TryNote);

  return size;
}

js::UniquePtr<ImmutableScriptData> js::ImmutableScriptData::new_(
    JSContext* cx, uint32_t codeLength, uint32_t noteLength,
    uint32_t numResumeOffsets, uint32_t numScopeNotes, uint32_t numTryNotes) {
  auto size = sizeFor(codeLength, noteLength, numResumeOffsets, numScopeNotes,
                      numTryNotes);
  if (!size.isValid()) {
    ReportAllocationOverflow(cx);
    return nullptr;
  }

  // Allocate contiguous raw buffer.
  void* raw = cx->pod_malloc<uint8_t>(size.value());
  MOZ_ASSERT(uintptr_t(raw) % alignof(ImmutableScriptData) == 0);
  if (!raw) {
    return nullptr;
  }

  // Constuct the ImmutableScriptData. Trailing arrays are uninitialized but
  // GCPtrs are put into a safe state.
  UniquePtr<ImmutableScriptData> result(new (raw) ImmutableScriptData(
      codeLength, noteLength, numResumeOffsets, numScopeNotes, numTryNotes));
  if (!result) {
    return nullptr;
  }

  // Sanity check
  MOZ_ASSERT(result->endOffset() == size.value());

  return result;
}

js::UniquePtr<ImmutableScriptData> js::ImmutableScriptData::new_(
    JSContext* cx, uint32_t totalSize) {
  void* raw = cx->pod_malloc<uint8_t>(totalSize);
  MOZ_ASSERT(uintptr_t(raw) % alignof(ImmutableScriptData) == 0);
  UniquePtr<ImmutableScriptData> result(
      reinterpret_cast<ImmutableScriptData*>(raw));
  return result;
}

bool js::ImmutableScriptData::validateLayout(uint32_t expectedSize) {
  constexpr size_t HeaderSize = sizeof(js::ImmutableScriptData);
  constexpr size_t OptionalOffsetsMaxSize = 3 * sizeof(Offset);

  // Check that the optional-offsets array lies within the allocation before we
  // try to read from it while computing sizes. Remember that the array *ends*
  // at the `optArrayOffset_`.
  static_assert(OptionalOffsetsMaxSize <= HeaderSize);
  if (HeaderSize > optArrayOffset_) {
    return false;
  }
  if (optArrayOffset_ > expectedSize) {
    return false;
  }

  // Round-trip the size computation using `CheckedInt` to detect overflow. This
  // should indirectly validate most alignment, size, and ordering requirments.
  auto size = sizeFor(codeLength(), noteLength(), resumeOffsets().size(),
                      scopeNotes().size(), tryNotes().size());
  return size.isValid() && (size.value() == expectedSize);
}

/* static */
SharedImmutableScriptData* SharedImmutableScriptData::create(JSContext* cx) {
  return cx->new_<SharedImmutableScriptData>();
}

/* static */
SharedImmutableScriptData* SharedImmutableScriptData::createWith(
    JSContext* cx, js::UniquePtr<ImmutableScriptData>&& isd) {
  MOZ_ASSERT(isd.get());
  SharedImmutableScriptData* sisd = create(cx);
  if (!sisd) {
    return nullptr;
  }

  sisd->setOwn(std::move(isd));
  return sisd;
}

void JSScript::relazify(JSRuntime* rt) {
  js::Scope* scope = enclosingScope();
  UniquePtr<PrivateScriptData> scriptData;

#ifndef JS_CODEGEN_NONE
  // Any JIT compiles should have been released, so we already point to the
  // interpreter trampoline which supports lazy scripts.
  MOZ_ASSERT(isUsingInterpreterTrampoline(rt));
#endif

  // Without bytecode, the script counts are invalid so destroy them if they
  // still exist.
  destroyScriptCounts();

  // Release the bytecode and gcthings list.
  // NOTE: We clear the PrivateScriptData to nullptr. This is fine because we
  //       only allowed relazification (via AllowRelazify) if the original lazy
  //       script we compiled from had a nullptr PrivateScriptData.
  swapData(scriptData);
  freeSharedData();

  // We should not still be in any side-tables for the debugger or
  // code-coverage. The finalizer will not be able to clean them up once
  // bytecode is released. We check in JSFunction::maybeRelazify() for these
  // conditions before requesting relazification.
  MOZ_ASSERT(!coverage::IsLCovEnabled());
  MOZ_ASSERT(!hasScriptCounts());
  MOZ_ASSERT(!hasDebugScript());

  // Rollback warmUpData_ to have enclosingScope.
  MOZ_ASSERT(warmUpData_.isWarmUpCount(),
             "JitScript should already be released");
  warmUpData_.resetWarmUpCount(0);
  warmUpData_.initEnclosingScope(scope);

  MOZ_ASSERT(isReadyForDelazification());
}

// Takes ownership of the passed SharedImmutableScriptData and either adds it
// into the runtime's SharedImmutableScriptDataTable, or frees it if a matching
// entry already exists and replaces the passed RefPtr with the existing entry.
/* static */
bool SharedImmutableScriptData::shareScriptData(
    JSContext* cx, RefPtr<SharedImmutableScriptData>& sisd) {
  MOZ_ASSERT(sisd);
  MOZ_ASSERT(sisd->refCount() == 1);

  SharedImmutableScriptData* data = sisd.get();

  // Calculate the hash before taking the lock. Because the data is reference
  // counted, it also will be freed after releasing the lock if necessary.
  SharedImmutableScriptData::Hasher::Lookup lookup(data);

  AutoLockScriptData lock(cx->runtime());

  SharedImmutableScriptDataTable::AddPtr p =
      cx->scriptDataTable(lock).lookupForAdd(lookup);
  if (p) {
    MOZ_ASSERT(data != *p);
    sisd = *p;
  } else {
    if (!cx->scriptDataTable(lock).add(p, data)) {
      ReportOutOfMemory(cx);
      return false;
    }

    // Being in the table counts as a reference on the script data.
    data->AddRef();
  }

  // Refs: sisd argument, SharedImmutableScriptDataTable
  MOZ_ASSERT(sisd->refCount() >= 2);

  return true;
}

void js::SweepScriptData(JSRuntime* rt) {
  // Entries are removed from the table when their reference count is one,
  // i.e. when the only reference to them is from the table entry.

  AutoLockScriptData lock(rt);
  SharedImmutableScriptDataTable& table = rt->scriptDataTable(lock);

  for (SharedImmutableScriptDataTable::Enum e(table); !e.empty();
       e.popFront()) {
    SharedImmutableScriptData* sharedData = e.front();
    if (sharedData->refCount() == 1) {
      sharedData->Release();
      e.removeFront();
    }
  }
}

inline size_t PrivateScriptData::allocationSize() const { return endOffset(); }

// Initialize and placement-new the trailing arrays.
PrivateScriptData::PrivateScriptData(uint32_t ngcthings)
    : ngcthings(ngcthings) {
  // Variable-length data begins immediately after PrivateScriptData itself.
  // NOTE: Alignment is computed using cursor/offset so the alignment of
  // PrivateScriptData must be stricter than any trailing array type.
  Offset cursor = sizeof(PrivateScriptData);

  // Layout and initialize the gcthings array.
  {
    initElements<JS::GCCellPtr>(cursor, ngcthings);
    cursor += ngcthings * sizeof(JS::GCCellPtr);
  }

  // Sanity check.
  MOZ_ASSERT(endOffset() == cursor);
}

/* static */
PrivateScriptData* PrivateScriptData::new_(JSContext* cx, uint32_t ngcthings) {
  // Compute size including trailing arrays.
  CheckedInt<Offset> size = sizeof(PrivateScriptData);
  size += CheckedInt<Offset>(ngcthings) * sizeof(JS::GCCellPtr);
  if (!size.isValid()) {
    ReportAllocationOverflow(cx);
    return nullptr;
  }

  // Allocate contiguous raw buffer for the trailing arrays.
  void* raw = cx->pod_malloc<uint8_t>(size.value());
  MOZ_ASSERT(uintptr_t(raw) % alignof(PrivateScriptData) == 0);
  if (!raw) {
    return nullptr;
  }

  // Constuct the PrivateScriptData. Trailing arrays are uninitialized but
  // GCPtrs are put into a safe state.
  PrivateScriptData* result = new (raw) PrivateScriptData(ngcthings);
  if (!result) {
    return nullptr;
  }

  // Sanity check.
  MOZ_ASSERT(result->endOffset() == size.value());

  return result;
}

/* static */
bool PrivateScriptData::InitFromStencil(
    JSContext* cx, js::HandleScript script,
    const js::frontend::CompilationAtomCache& atomCache,
    const js::frontend::CompilationStencil& stencil,
    js::frontend::CompilationGCOutput& gcOutput,
    const js::frontend::ScriptIndex scriptIndex) {
  js::frontend::ScriptStencil& scriptStencil = stencil.scriptData[scriptIndex];
  uint32_t ngcthings = scriptStencil.gcThingsLength;

  MOZ_ASSERT(ngcthings <= INDEX_LIMIT);

  // Create and initialize PrivateScriptData
  if (!JSScript::createPrivateScriptData(cx, script, ngcthings)) {
    return false;
  }

  js::PrivateScriptData* data = script->data_;
  if (ngcthings) {
    if (!EmitScriptThingsVector(cx, atomCache, stencil, gcOutput,
                                scriptStencil.gcthings(stencil),
                                data->gcthings())) {
      return false;
    }
  }

  return true;
}

void PrivateScriptData::trace(JSTracer* trc) {
  for (JS::GCCellPtr& elem : gcthings()) {
    gc::Cell* thing = elem.asCell();
    TraceManuallyBarrieredGenericPointerEdge(trc, &thing, "script-gcthing");
    if (MOZ_UNLIKELY(!thing)) {
      // NOTE: If we are clearing edges, also erase the type. This can happen
      // due to OOM triggering the ClearEdgesTracer.
      elem = JS::GCCellPtr();
    } else if (thing != elem.asCell()) {
      elem = JS::GCCellPtr(thing, elem.kind());
    }
  }
}

/*static*/
JSScript* JSScript::Create(JSContext* cx, JS::Handle<JSFunction*> function,
                           js::HandleScriptSourceObject sourceObject,
                           const SourceExtent& extent,
                           js::ImmutableScriptFlags flags) {
  return static_cast<JSScript*>(
      BaseScript::New(cx, function, sourceObject, extent, flags));
}

#ifdef MOZ_VTUNE
uint32_t JSScript::vtuneMethodID() {
  if (!zone()->scriptVTuneIdMap) {
    auto map = MakeUnique<ScriptVTuneIdMap>();
    if (!map) {
      MOZ_CRASH("Failed to allocate ScriptVTuneIdMap");
    }

    zone()->scriptVTuneIdMap = std::move(map);
  }

  ScriptVTuneIdMap::AddPtr p = zone()->scriptVTuneIdMap->lookupForAdd(this);
  if (p) {
    return p->value();
  }

  MOZ_ASSERT(this->hasBytecode());

  uint32_t id = vtune::GenerateUniqueMethodID();
  if (!zone()->scriptVTuneIdMap->add(p, this, id)) {
    MOZ_CRASH("Failed to add vtune method id");
  }

  return id;
}
#endif

/* static */
bool JSScript::createPrivateScriptData(JSContext* cx, HandleScript script,
                                       uint32_t ngcthings) {
  cx->check(script);

  UniquePtr<PrivateScriptData> data(PrivateScriptData::new_(cx, ngcthings));
  if (!data) {
    return false;
  }

  script->swapData(data);
  MOZ_ASSERT(!data);

  return true;
}

/* static */
bool JSScript::fullyInitFromStencil(
    JSContext* cx, const js::frontend::CompilationAtomCache& atomCache,
    const js::frontend::CompilationStencil& stencil,
    frontend::CompilationGCOutput& gcOutput, HandleScript script,
    const js::frontend::ScriptIndex scriptIndex) {
  MutableScriptFlags lazyMutableFlags;
  RootedScope lazyEnclosingScope(cx);

  // A holder for the lazy PrivateScriptData that we must keep around in case
  // this process fails and we must return the script to its original state.
  //
  // This is initialized by BaseScript::swapData() which will run pre-barriers
  // for us. On successful conversion to non-lazy script, the old script data
  // here will be released by the UniquePtr.
  Rooted<UniquePtr<PrivateScriptData>> lazyData(cx);

#ifndef JS_CODEGEN_NONE
  // Whether we are a newborn script or an existing lazy script, we should
  // already be pointing to the interpreter trampoline.
  MOZ_ASSERT(script->isUsingInterpreterTrampoline(cx->runtime()));
#endif

  // If we are using an existing lazy script, record enough info to be able to
  // rollback on failure.
  if (script->isReadyForDelazification()) {
    lazyMutableFlags = script->mutableFlags_;
    lazyEnclosingScope = script->releaseEnclosingScope();
    script->swapData(lazyData.get());
    MOZ_ASSERT(script->sharedData_ == nullptr);
  }

  // Restore the script to lazy state on failure. If this was a fresh script, we
  // just need to clear bytecode to mark script as incomplete.
  auto rollbackGuard = mozilla::MakeScopeExit([&] {
    if (lazyEnclosingScope) {
      script->mutableFlags_ = lazyMutableFlags;
      script->warmUpData_.initEnclosingScope(lazyEnclosingScope);
      script->swapData(lazyData.get());
      script->sharedData_ = nullptr;

      MOZ_ASSERT(script->isReadyForDelazification());
    } else {
      script->sharedData_ = nullptr;
    }
  });

  // The counts of indexed things must be checked during code generation.
  MOZ_ASSERT(stencil.scriptData[scriptIndex].gcThingsLength <= INDEX_LIMIT);

  // Note: These flags should already be correct when the BaseScript was
  // allocated.
  MOZ_ASSERT_IF(stencil.isInitialStencil(),
                script->immutableFlags() ==
                    stencil.scriptExtra[scriptIndex].immutableFlags);

  // Create and initialize PrivateScriptData
  if (!PrivateScriptData::InitFromStencil(cx, script, atomCache, stencil,
                                          gcOutput, scriptIndex)) {
    return false;
  }

  // Member-initializer data is computed in initial parse only. If we are
  // delazifying, make sure to copy it off the `lazyData` before we throw it
  // away.
  if (script->useMemberInitializers()) {
    if (stencil.isInitialStencil()) {
      MemberInitializers initializers(
          stencil.scriptExtra[scriptIndex].memberInitializers());
      script->setMemberInitializers(initializers);
    } else {
      script->setMemberInitializers(lazyData.get()->getMemberInitializers());
    }
  }

  auto* scriptData = stencil.sharedData.get(scriptIndex);
  MOZ_ASSERT_IF(
      script->isGenerator() || script->isAsync(),
      scriptData->nfixed() <= frontend::ParseContext::Scope::FixedSlotLimit);

  script->initSharedData(scriptData);

  // NOTE: JSScript is now constructed and should be linked in.
  rollbackGuard.release();

  // Link Scope -> JSFunction -> BaseScript.
  if (script->isFunction()) {
    JSFunction* fun = gcOutput.getFunction(scriptIndex);
    script->bodyScope()->as<FunctionScope>().initCanonicalFunction(fun);
    if (fun->isIncomplete()) {
      fun->initScript(script);
    } else if (fun->hasSelfHostedLazyScript()) {
      fun->clearSelfHostedLazyScript();
      fun->initScript(script);
    } else {
      // We are delazifying in-place.
      MOZ_ASSERT(fun->baseScript() == script);
    }
  }

  // NOTE: The caller is responsible for linking ModuleObjects if this is a
  //       module script.

#ifdef JS_STRUCTURED_SPEW
  // We want this to happen after line number initialization to allow filtering
  // to work.
  script->setSpewEnabled(cx->spewer().enabled(script));
#endif

#ifdef DEBUG
  script->assertValidJumpTargets();
#endif

  if (coverage::IsLCovEnabled()) {
    if (!coverage::InitScriptCoverage(cx, script)) {
      return false;
    }
  }

  return true;
}

JSScript* JSScript::fromStencil(JSContext* cx,
                                frontend::CompilationAtomCache& atomCache,
                                const frontend::CompilationStencil& stencil,
                                frontend::CompilationGCOutput& gcOutput,
                                frontend::ScriptIndex scriptIndex) {
  js::frontend::ScriptStencil& scriptStencil = stencil.scriptData[scriptIndex];
  js::frontend::ScriptStencilExtra& scriptExtra =
      stencil.scriptExtra[scriptIndex];
  MOZ_ASSERT(scriptStencil.hasSharedData(),
             "Need generated bytecode to use JSScript::fromStencil");

  Rooted<JSFunction*> function(cx);
  if (scriptStencil.isFunction()) {
    function = gcOutput.getFunction(scriptIndex);
  }

  Rooted<ScriptSourceObject*> sourceObject(cx, gcOutput.sourceObject);
  RootedScript script(cx, Create(cx, function, sourceObject, scriptExtra.extent,
                                 scriptExtra.immutableFlags));
  if (!script) {
    return nullptr;
  }

  if (!fullyInitFromStencil(cx, atomCache, stencil, gcOutput, script,
                            scriptIndex)) {
    return nullptr;
  }

  return script;
}

#ifdef DEBUG
void JSScript::assertValidJumpTargets() const {
  BytecodeLocation mainLoc = mainLocation();
  BytecodeLocation endLoc = endLocation();
  AllBytecodesIterable iter(this);
  for (BytecodeLocation loc : iter) {
    // Check jump instructions' target.
    if (loc.isJump()) {
      BytecodeLocation target = loc.getJumpTarget();
      MOZ_ASSERT(mainLoc <= target && target < endLoc);
      MOZ_ASSERT(target.isJumpTarget());

      // All backward jumps must be to a JSOp::LoopHead op. This is an invariant
      // we want to maintain to simplify JIT compilation and bytecode analysis.
      MOZ_ASSERT_IF(target < loc, target.is(JSOp::LoopHead));
      MOZ_ASSERT_IF(target < loc, IsBackedgePC(loc.toRawBytecode()));

      // All forward jumps must be to a JSOp::JumpTarget op.
      MOZ_ASSERT_IF(target > loc, target.is(JSOp::JumpTarget));

      // Jumps must not cross scope boundaries.
      MOZ_ASSERT(loc.innermostScope(this) == target.innermostScope(this));

      // Check fallthrough of conditional jump instructions.
      if (loc.fallsThrough()) {
        BytecodeLocation fallthrough = loc.next();
        MOZ_ASSERT(mainLoc <= fallthrough && fallthrough < endLoc);
        MOZ_ASSERT(fallthrough.isJumpTarget());
      }
    }

    // Check table switch case labels.
    if (loc.is(JSOp::TableSwitch)) {
      BytecodeLocation target = loc.getTableSwitchDefaultTarget();

      // Default target.
      MOZ_ASSERT(mainLoc <= target && target < endLoc);
      MOZ_ASSERT(target.is(JSOp::JumpTarget));

      int32_t low = loc.getTableSwitchLow();
      int32_t high = loc.getTableSwitchHigh();

      for (int i = 0; i < high - low + 1; i++) {
        BytecodeLocation switchCase = loc.getTableSwitchCaseTarget(this, i);
        MOZ_ASSERT(mainLoc <= switchCase && switchCase < endLoc);
        MOZ_ASSERT(switchCase.is(JSOp::JumpTarget));
      }
    }
  }

  // Check catch/finally blocks as jump targets.
  for (const TryNote& tn : trynotes()) {
    if (tn.kind() != TryNoteKind::Catch && tn.kind() != TryNoteKind::Finally) {
      continue;
    }

    jsbytecode* tryStart = offsetToPC(tn.start);
    jsbytecode* tryPc = tryStart - JSOpLength_Try;
    MOZ_ASSERT(JSOp(*tryPc) == JSOp::Try);

    jsbytecode* tryTarget = tryStart + tn.length;
    MOZ_ASSERT(main() <= tryTarget && tryTarget < codeEnd());
    MOZ_ASSERT(BytecodeIsJumpTarget(JSOp(*tryTarget)));
  }
}
#endif

void JSScript::addSizeOfJitScript(mozilla::MallocSizeOf mallocSizeOf,
                                  size_t* sizeOfJitScript,
                                  size_t* sizeOfBaselineFallbackStubs) const {
  if (!hasJitScript()) {
    return;
  }

  jitScript()->addSizeOfIncludingThis(mallocSizeOf, sizeOfJitScript,
                                      sizeOfBaselineFallbackStubs);
}

js::GlobalObject& JSScript::uninlinedGlobal() const { return global(); }

static const uint32_t GSN_CACHE_THRESHOLD = 100;

void GSNCache::purge() {
  code = nullptr;
  map.clearAndCompact();
}

const js::SrcNote* js::GetSrcNote(GSNCache& cache, JSScript* script,
                                  jsbytecode* pc) {
  size_t target = pc - script->code();
  if (target >= script->length()) {
    return nullptr;
  }

  if (cache.code == script->code()) {
    GSNCache::Map::Ptr p = cache.map.lookup(pc);
    return p ? p->value() : nullptr;
  }

  size_t offset = 0;
  const js::SrcNote* result;
  for (SrcNoteIterator iter(script->notes());; ++iter) {
    auto sn = *iter;
    if (sn->isTerminator()) {
      result = nullptr;
      break;
    }
    offset += sn->delta();
    if (offset == target && sn->isGettable()) {
      result = sn;
      break;
    }
  }

  if (cache.code != script->code() && script->length() >= GSN_CACHE_THRESHOLD) {
    unsigned nsrcnotes = 0;
    for (SrcNoteIterator iter(script->notes()); !iter.atEnd(); ++iter) {
      auto sn = *iter;
      if (sn->isGettable()) {
        ++nsrcnotes;
      }
    }
    if (cache.code) {
      cache.map.clear();
      cache.code = nullptr;
    }
    if (cache.map.reserve(nsrcnotes)) {
      pc = script->code();
      for (SrcNoteIterator iter(script->notes()); !iter.atEnd(); ++iter) {
        auto sn = *iter;
        pc += sn->delta();
        if (sn->isGettable()) {
          cache.map.putNewInfallible(pc, sn);
        }
      }
      cache.code = script->code();
    }
  }

  return result;
}

const js::SrcNote* js::GetSrcNote(JSContext* cx, JSScript* script,
                                  jsbytecode* pc) {
  return GetSrcNote(cx->caches().gsnCache, script, pc);
}

unsigned js::PCToLineNumber(unsigned startLine, unsigned startCol,
                            SrcNote* notes, jsbytecode* code, jsbytecode* pc,
                            unsigned* columnp) {
  unsigned lineno = startLine;
  unsigned column = startCol;

  /*
   * Walk through source notes accumulating their deltas, keeping track of
   * line-number notes, until we pass the note for pc's offset within
   * script->code.
   */
  ptrdiff_t offset = 0;
  ptrdiff_t target = pc - code;
  for (SrcNoteIterator iter(notes); !iter.atEnd(); ++iter) {
    auto sn = *iter;
    offset += sn->delta();
    if (offset > target) {
      break;
    }

    SrcNoteType type = sn->type();
    if (type == SrcNoteType::SetLine) {
      lineno = SrcNote::SetLine::getLine(sn, startLine);
      column = 0;
    } else if (type == SrcNoteType::NewLine) {
      lineno++;
      column = 0;
    } else if (type == SrcNoteType::ColSpan) {
      ptrdiff_t colspan = SrcNote::ColSpan::getSpan(sn);
      MOZ_ASSERT(ptrdiff_t(column) + colspan >= 0);
      column += colspan;
    }
  }

  if (columnp) {
    *columnp = column;
  }

  return lineno;
}

unsigned js::PCToLineNumber(JSScript* script, jsbytecode* pc,
                            unsigned* columnp) {
  /* Cope with InterpreterFrame.pc value prior to entering Interpret. */
  if (!pc) {
    return 0;
  }

  return PCToLineNumber(script->lineno(), script->column(), script->notes(),
                        script->code(), pc, columnp);
}

jsbytecode* js::LineNumberToPC(JSScript* script, unsigned target) {
  ptrdiff_t offset = 0;
  ptrdiff_t best = -1;
  unsigned lineno = script->lineno();
  unsigned bestdiff = SrcNote::MaxOperand;
  for (SrcNoteIterator iter(script->notes()); !iter.atEnd(); ++iter) {
    auto sn = *iter;
    /*
     * Exact-match only if offset is not in the prologue; otherwise use
     * nearest greater-or-equal line number match.
     */
    if (lineno == target && offset >= ptrdiff_t(script->mainOffset())) {
      goto out;
    }
    if (lineno >= target) {
      unsigned diff = lineno - target;
      if (diff < bestdiff) {
        bestdiff = diff;
        best = offset;
      }
    }
    offset += sn->delta();
    SrcNoteType type = sn->type();
    if (type == SrcNoteType::SetLine) {
      lineno = SrcNote::SetLine::getLine(sn, script->lineno());
    } else if (type == SrcNoteType::NewLine) {
      lineno++;
    }
  }
  if (best >= 0) {
    offset = best;
  }
out:
  return script->offsetToPC(offset);
}

JS_PUBLIC_API unsigned js::GetScriptLineExtent(JSScript* script) {
  unsigned lineno = script->lineno();
  unsigned maxLineNo = lineno;
  for (SrcNoteIterator iter(script->notes()); !iter.atEnd(); ++iter) {
    auto sn = *iter;
    SrcNoteType type = sn->type();
    if (type == SrcNoteType::SetLine) {
      lineno = SrcNote::SetLine::getLine(sn, script->lineno());
    } else if (type == SrcNoteType::NewLine) {
      lineno++;
    }

    if (maxLineNo < lineno) {
      maxLineNo = lineno;
    }
  }

  return 1 + maxLineNo - script->lineno();
}

#ifdef JS_CACHEIR_SPEW
void js::maybeUpdateWarmUpCount(JSScript* script) {
  if (script->needsFinalWarmUpCount()) {
    ScriptFinalWarmUpCountMap* map =
        script->zone()->scriptFinalWarmUpCountMap.get();
    // If needsFinalWarmUpCount is true, ScriptFinalWarmUpCountMap must have
    // already been created and thus must be asserted.
    MOZ_ASSERT(map);
    ScriptFinalWarmUpCountMap::Ptr p = map->lookup(script);
    MOZ_ASSERT(p);

    mozilla::Get<0>(p->value()) += script->jitScript()->warmUpCount();
  }
}

void js::maybeSpewScriptFinalWarmUpCount(JSScript* script) {
  if (script->needsFinalWarmUpCount()) {
    ScriptFinalWarmUpCountMap* map =
        script->zone()->scriptFinalWarmUpCountMap.get();
    // If needsFinalWarmUpCount is true, ScriptFinalWarmUpCountMap must have
    // already been created and thus must be asserted.
    MOZ_ASSERT(map);
    ScriptFinalWarmUpCountMap::Ptr p = map->lookup(script);
    MOZ_ASSERT(p);
    auto& tuple = p->value();
    uint32_t warmUpCount = mozilla::Get<0>(tuple);
    SharedImmutableString& scriptName = mozilla::Get<1>(tuple);

    JSContext* cx = TlsContext.get();
    cx->spewer().enableSpewing();

    // In the case that we care about a script's final warmup count but the
    // spewer is not enabled, AutoSpewChannel automatically sets and unsets
    // the proper channel for the duration of spewing a health report's warm
    // up count.
    AutoSpewChannel channel(cx, SpewChannel::CacheIRHealthReport, script);
    jit::CacheIRHealth cih;
    cih.spewScriptFinalWarmUpCount(cx, scriptName.chars(), script, warmUpCount);

    script->zone()->scriptFinalWarmUpCountMap->remove(script);
    script->setNeedsFinalWarmUpCount(false);
  }
}
#endif

void js::DescribeScriptedCallerForDirectEval(JSContext* cx, HandleScript script,
                                             jsbytecode* pc, const char** file,
                                             unsigned* linenop,
                                             uint32_t* pcOffset,
                                             bool* mutedErrors) {
  MOZ_ASSERT(script->containsPC(pc));

  static_assert(JSOpLength_SpreadEval == JSOpLength_StrictSpreadEval,
                "next op after a spread must be at consistent offset");
  static_assert(JSOpLength_Eval == JSOpLength_StrictEval,
                "next op after a direct eval must be at consistent offset");

  MOZ_ASSERT(JSOp(*pc) == JSOp::Eval || JSOp(*pc) == JSOp::StrictEval ||
             JSOp(*pc) == JSOp::SpreadEval ||
             JSOp(*pc) == JSOp::StrictSpreadEval);

  bool isSpread =
      (JSOp(*pc) == JSOp::SpreadEval || JSOp(*pc) == JSOp::StrictSpreadEval);
  jsbytecode* nextpc =
      pc + (isSpread ? JSOpLength_SpreadEval : JSOpLength_Eval);
  MOZ_ASSERT(JSOp(*nextpc) == JSOp::Lineno);

  *file = script->filename();
  *linenop = GET_UINT32(nextpc);
  *pcOffset = script->pcToOffset(pc);
  *mutedErrors = script->mutedErrors();
}

void js::DescribeScriptedCallerForCompilation(
    JSContext* cx, MutableHandleScript maybeScript, const char** file,
    unsigned* linenop, uint32_t* pcOffset, bool* mutedErrors) {
  NonBuiltinFrameIter iter(cx, cx->realm()->principals());

  if (iter.done()) {
    maybeScript.set(nullptr);
    *file = nullptr;
    *linenop = 0;
    *pcOffset = 0;
    *mutedErrors = false;
    return;
  }

  *file = iter.filename();
  *linenop = iter.computeLine();
  *mutedErrors = iter.mutedErrors();

  // These values are only used for introducer fields which are debugging
  // information and can be safely left null for wasm frames.
  if (iter.hasScript()) {
    maybeScript.set(iter.script());
    *pcOffset = iter.pc() - maybeScript->code();
  } else {
    maybeScript.set(nullptr);
    *pcOffset = 0;
  }
}

template <typename SourceSpan, typename TargetSpan>
void CopySpan(const SourceSpan& source, TargetSpan target) {
  MOZ_ASSERT(source.size() == target.size());
  std::copy(source.cbegin(), source.cend(), target.begin());
}

/* static */
js::UniquePtr<ImmutableScriptData> ImmutableScriptData::new_(
    JSContext* cx, uint32_t mainOffset, uint32_t nfixed, uint32_t nslots,
    GCThingIndex bodyScopeIndex, uint32_t numICEntries, bool isFunction,
    uint16_t funLength, mozilla::Span<const jsbytecode> code,
    mozilla::Span<const SrcNote> notes,
    mozilla::Span<const uint32_t> resumeOffsets,
    mozilla::Span<const ScopeNote> scopeNotes,
    mozilla::Span<const TryNote> tryNotes) {
  MOZ_RELEASE_ASSERT(code.Length() <= frontend::MaxBytecodeLength);

  // There are 1-4 copies of SrcNoteType::Null appended after the source
  // notes. These are a combination of sentinel and padding values.
  static_assert(frontend::MaxSrcNotesLength <= UINT32_MAX - CodeNoteAlign,
                "Length + CodeNoteAlign shouldn't overflow UINT32_MAX");
  size_t noteLength = notes.Length();
  MOZ_RELEASE_ASSERT(noteLength <= frontend::MaxSrcNotesLength);

  size_t nullLength = ComputeNotePadding(code.Length(), noteLength);

  // Allocate ImmutableScriptData
  js::UniquePtr<ImmutableScriptData> data(ImmutableScriptData::new_(
      cx, code.Length(), noteLength + nullLength, resumeOffsets.Length(),
      scopeNotes.Length(), tryNotes.Length()));
  if (!data) {
    return data;
  }

  // Initialize POD fields
  data->mainOffset = mainOffset;
  data->nfixed = nfixed;
  data->nslots = nslots;
  data->bodyScopeIndex = bodyScopeIndex;
  data->numICEntries = numICEntries;

  if (isFunction) {
    data->funLength = funLength;
  }

  // Initialize trailing arrays
  CopySpan(code, data->codeSpan());
  CopySpan(notes, data->notesSpan().To(noteLength));
  std::fill_n(data->notes() + noteLength, nullLength, SrcNote::terminator());
  CopySpan(resumeOffsets, data->resumeOffsets());
  CopySpan(scopeNotes, data->scopeNotes());
  CopySpan(tryNotes, data->tryNotes());

  return data;
}

void ScriptWarmUpData::trace(JSTracer* trc) {
  uintptr_t tag = data_ & TagMask;
  switch (tag) {
    case EnclosingScriptTag: {
      BaseScript* enclosingScript = toEnclosingScript();
      TraceManuallyBarrieredEdge(trc, &enclosingScript, "enclosingScript");
      setTaggedPtr<EnclosingScriptTag>(enclosingScript);
      break;
    }

    case EnclosingScopeTag: {
      Scope* enclosingScope = toEnclosingScope();
      TraceManuallyBarrieredEdge(trc, &enclosingScope, "enclosingScope");
      setTaggedPtr<EnclosingScopeTag>(enclosingScope);
      break;
    }

    case JitScriptTag: {
      toJitScript()->trace(trc);
      break;
    }

    default: {
      MOZ_ASSERT(isWarmUpCount());
      break;
    }
  }
}

size_t JSScript::calculateLiveFixed(jsbytecode* pc) {
  size_t nlivefixed = numAlwaysLiveFixedSlots();

  if (nfixed() != nlivefixed) {
    Scope* scope = lookupScope(pc);
    if (scope) {
      scope = MaybeForwarded(scope);
    }

    // Find the nearest LexicalScope in the same script.
    while (scope && scope->is<WithScope>()) {
      scope = scope->enclosing();
      if (scope) {
        scope = MaybeForwarded(scope);
      }
    }

    if (scope) {
      if (scope->is<LexicalScope>()) {
        nlivefixed = scope->as<LexicalScope>().nextFrameSlot();
      } else if (scope->is<VarScope>()) {
        nlivefixed = scope->as<VarScope>().nextFrameSlot();
      } else if (scope->is<ClassBodyScope>()) {
        nlivefixed = scope->as<ClassBodyScope>().nextFrameSlot();
      }
    }
  }

  MOZ_ASSERT(nlivefixed <= nfixed());
  MOZ_ASSERT(nlivefixed >= numAlwaysLiveFixedSlots());

  return nlivefixed;
}

Scope* JSScript::lookupScope(const jsbytecode* pc) const {
  MOZ_ASSERT(containsPC(pc));

  size_t offset = pc - code();

  auto notes = scopeNotes();
  Scope* scope = nullptr;

  // Find the innermost block chain using a binary search.
  size_t bottom = 0;
  size_t top = notes.size();

  while (bottom < top) {
    size_t mid = bottom + (top - bottom) / 2;
    const ScopeNote* note = &notes[mid];
    if (note->start <= offset) {
      // Block scopes are ordered in the list by their starting offset, and
      // since blocks form a tree ones earlier in the list may cover the pc even
      // if later blocks end before the pc. This only happens when the earlier
      // block is a parent of the later block, so we need to check parents of
      // |mid| in the searched range for coverage.
      size_t check = mid;
      while (check >= bottom) {
        const ScopeNote* checkNote = &notes[check];
        MOZ_ASSERT(checkNote->start <= offset);
        if (offset < checkNote->start + checkNote->length) {
          // We found a matching block chain but there may be inner ones
          // at a higher block chain index than mid. Continue the binary search.
          if (checkNote->index == ScopeNote::NoScopeIndex) {
            scope = nullptr;
          } else {
            scope = getScope(checkNote->index);
          }
          break;
        }
        if (checkNote->parent == UINT32_MAX) {
          break;
        }
        check = checkNote->parent;
      }
      bottom = mid + 1;
    } else {
      top = mid;
    }
  }

  return scope;
}

Scope* JSScript::innermostScope(const jsbytecode* pc) const {
  if (Scope* scope = lookupScope(pc)) {
    return scope;
  }
  return bodyScope();
}

void js::SetFrameArgumentsObject(JSContext* cx, AbstractFramePtr frame,
                                 HandleScript script, JSObject* argsobj) {
  /*
   * If the arguments object was optimized out by scalar replacement,
   * we must recreate it when we bail out. Because 'arguments' may have
   * already been overwritten, we must check to see if the slot already
   * contains a value.
   */

  Rooted<BindingIter> bi(cx, BindingIter(script));
  while (bi && bi.name() != cx->names().arguments) {
    bi++;
  }
  if (!bi) {
    return;
  }

  if (bi.location().kind() == BindingLocation::Kind::Environment) {
#ifdef DEBUG
    /*
     * If |arguments| lives in the call object, we should not have
     * optimized it. Scan the script to find the slot in the call
     * object that |arguments| is assigned to and verify that it
     * already exists.
     */
    jsbytecode* pc = script->code();
    while (JSOp(*pc) != JSOp::Arguments) {
      pc += GetBytecodeLength(pc);
    }
    pc += JSOpLength_Arguments;
    MOZ_ASSERT(JSOp(*pc) == JSOp::SetAliasedVar);

    EnvironmentObject& env = frame.callObj().as<EnvironmentObject>();
    MOZ_ASSERT(!env.aliasedBinding(bi).isMagic(JS_OPTIMIZED_OUT));
#endif
    return;
  }

  MOZ_ASSERT(bi.location().kind() == BindingLocation::Kind::Frame);
  uint32_t frameSlot = bi.location().slot();
  if (frame.unaliasedLocal(frameSlot).isMagic(JS_OPTIMIZED_OUT)) {
    frame.unaliasedLocal(frameSlot) = ObjectValue(*argsobj);
  }
}

bool JSScript::formalIsAliased(unsigned argSlot) {
  if (functionHasParameterExprs()) {
    return false;
  }

  for (PositionalFormalParameterIter fi(this); fi; fi++) {
    if (fi.argumentSlot() == argSlot) {
      return fi.closedOver();
    }
  }
  MOZ_CRASH("Argument slot not found");
}

// Returns true if any formal argument is mapped by the arguments
// object, but lives in the call object.
bool JSScript::anyFormalIsForwarded() {
  if (!argsObjAliasesFormals()) {
    return false;
  }

  for (PositionalFormalParameterIter fi(this); fi; fi++) {
    if (fi.closedOver()) {
      return true;
    }
  }
  return false;
}

bool JSScript::formalLivesInArgumentsObject(unsigned argSlot) {
  return argsObjAliasesFormals() && !formalIsAliased(argSlot);
}

BaseScript::BaseScript(uint8_t* stubEntry, JSFunction* function,
                       ScriptSourceObject* sourceObject,
                       const SourceExtent& extent, uint32_t immutableFlags)
    : TenuredCellWithNonGCPointer(stubEntry),
      function_(function),
      sourceObject_(sourceObject),
      extent_(extent),
      immutableFlags_(immutableFlags) {
  MOZ_ASSERT(extent_.toStringStart <= extent_.sourceStart);
  MOZ_ASSERT(extent_.sourceStart <= extent_.sourceEnd);
  MOZ_ASSERT(extent_.sourceEnd <= extent_.toStringEnd);
}

/* static */
BaseScript* BaseScript::New(JSContext* cx, JS::Handle<JSFunction*> function,
                            HandleScriptSourceObject sourceObject,
                            const SourceExtent& extent,
                            uint32_t immutableFlags) {
  void* script = Allocate<BaseScript>(cx);
  if (!script) {
    return nullptr;
  }

#ifndef JS_CODEGEN_NONE
  uint8_t* stubEntry = cx->runtime()->jitRuntime()->interpreterStub().value;
#else
  uint8_t* stubEntry = nullptr;
#endif

  MOZ_ASSERT_IF(function,
                function->compartment() == sourceObject->compartment());
  MOZ_ASSERT_IF(function, function->realm() == sourceObject->realm());

  return new (script)
      BaseScript(stubEntry, function, sourceObject, extent, immutableFlags);
}

/* static */
BaseScript* BaseScript::CreateRawLazy(JSContext* cx, uint32_t ngcthings,
                                      HandleFunction fun,
                                      HandleScriptSourceObject sourceObject,
                                      const SourceExtent& extent,
                                      uint32_t immutableFlags) {
  cx->check(fun);

  BaseScript* lazy = New(cx, fun, sourceObject, extent, immutableFlags);
  if (!lazy) {
    return nullptr;
  }

  // Allocate a PrivateScriptData if it will not be empty. Lazy class
  // constructors that use member initializers also need PrivateScriptData for
  // field data.
  //
  // This condition is implicit in BaseScript::hasPrivateScriptData, and should
  // be mirrored on InputScript::hasPrivateScriptData.
  if (ngcthings || lazy->useMemberInitializers()) {
    UniquePtr<PrivateScriptData> data(PrivateScriptData::new_(cx, ngcthings));
    if (!data) {
      return nullptr;
    }
    lazy->swapData(data);
    MOZ_ASSERT(!data);
  }

  return lazy;
}

void JSScript::updateJitCodeRaw(JSRuntime* rt) {
  MOZ_ASSERT(rt);
  if (hasBaselineScript() && baselineScript()->hasPendingIonCompileTask()) {
    MOZ_ASSERT(!isIonCompilingOffThread());
    setJitCodeRaw(rt->jitRuntime()->lazyLinkStub().value);
  } else if (hasIonScript()) {
    jit::IonScript* ion = ionScript();
    setJitCodeRaw(ion->method()->raw());
  } else if (hasBaselineScript()) {
    setJitCodeRaw(baselineScript()->method()->raw());
  } else if (hasJitScript() && js::jit::IsBaselineInterpreterEnabled()) {
    setJitCodeRaw(rt->jitRuntime()->baselineInterpreter().codeRaw());
  } else {
    setJitCodeRaw(rt->jitRuntime()->interpreterStub().value);
  }
  MOZ_ASSERT(jitCodeRaw());
}

bool JSScript::hasLoops() {
  for (const TryNote& tn : trynotes()) {
    if (tn.isLoop()) {
      return true;
    }
  }
  return false;
}

bool JSScript::mayReadFrameArgsDirectly() {
  return needsArgsObj() || hasRest();
}

void JSScript::resetWarmUpCounterToDelayIonCompilation() {
  // Reset the warm-up count only if it's greater than the BaselineCompiler
  // threshold. We do this to ensure this has no effect on Baseline compilation
  // because we don't want scripts to get stuck in the (Baseline) interpreter in
  // pathological cases.

  if (getWarmUpCount() > jit::JitOptions.baselineJitWarmUpThreshold) {
    incWarmUpResetCounter();
    uint32_t newCount = jit::JitOptions.baselineJitWarmUpThreshold;
    if (warmUpData_.isWarmUpCount()) {
      warmUpData_.resetWarmUpCount(newCount);
    } else {
      warmUpData_.toJitScript()->resetWarmUpCount(newCount);
    }
  }
}

gc::AllocSite* JSScript::createAllocSite() {
  return jitScript()->createAllocSite(this);
}

void JSScript::AutoDelazify::holdScript(JS::HandleFunction fun) {
  if (fun) {
    JSAutoRealm ar(cx_, fun);
    script_ = JSFunction::getOrCreateScript(cx_, fun);
    if (script_) {
      oldAllowRelazify_ = script_->allowRelazify();
      script_->clearAllowRelazify();
    }
  }
}

void JSScript::AutoDelazify::dropScript() {
  if (script_) {
    script_->setAllowRelazify(oldAllowRelazify_);
  }
  script_ = nullptr;
}

JS::ubi::Base::Size JS::ubi::Concrete<BaseScript>::size(
    mozilla::MallocSizeOf mallocSizeOf) const {
  BaseScript* base = &get();

  Size size = gc::Arena::thingSize(base->getAllocKind());
  size += base->sizeOfExcludingThis(mallocSizeOf);

  // Include any JIT data if it exists.
  if (base->hasJitScript()) {
    JSScript* script = base->asJSScript();

    size_t jitScriptSize = 0;
    size_t fallbackStubSize = 0;
    script->addSizeOfJitScript(mallocSizeOf, &jitScriptSize, &fallbackStubSize);
    size += jitScriptSize;
    size += fallbackStubSize;

    size_t baselineSize = 0;
    jit::AddSizeOfBaselineData(script, mallocSizeOf, &baselineSize);
    size += baselineSize;

    size += jit::SizeOfIonData(script, mallocSizeOf);
  }

  MOZ_ASSERT(size > 0);
  return size;
}

const char* JS::ubi::Concrete<BaseScript>::scriptFilename() const {
  return get().filename();
}
