/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/BaselineIC.h"

#include "mozilla/Casting.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/IntegerPrintfMacros.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/Sprintf.h"
#include "mozilla/TemplateLib.h"
#include "mozilla/Unused.h"

#include "jsfriendapi.h"
#include "jslibmath.h"
#include "jstypes.h"

#include "builtin/Eval.h"
#include "gc/Policy.h"
#include "jit/BaselineCacheIRCompiler.h"
#include "jit/BaselineDebugModeOSR.h"
#include "jit/BaselineJIT.h"
#include "jit/InlinableNatives.h"
#include "jit/JitSpewer.h"
#include "jit/Linker.h"
#include "jit/Lowering.h"
#ifdef JS_ION_PERF
#  include "jit/PerfSpewer.h"
#endif
#include "jit/SharedICHelpers.h"
#include "jit/VMFunctions.h"
#include "js/Conversions.h"
#include "js/GCVector.h"
#include "vm/BytecodeIterator.h"
#include "vm/BytecodeLocation.h"
#include "vm/BytecodeUtil.h"
#include "vm/JSFunction.h"
#include "vm/JSScript.h"
#include "vm/Opcodes.h"
#include "vm/SelfHosting.h"
#include "vm/TypedArrayObject.h"
#ifdef MOZ_VTUNE
#  include "vtune/VTuneWrapper.h"
#endif

#include "builtin/Boolean-inl.h"

#include "jit/JitFrames-inl.h"
#include "jit/MacroAssembler-inl.h"
#include "jit/shared/Lowering-shared-inl.h"
#include "jit/SharedICHelpers-inl.h"
#include "jit/VMFunctionList-inl.h"
#include "vm/BytecodeIterator-inl.h"
#include "vm/BytecodeLocation-inl.h"
#include "vm/EnvironmentObject-inl.h"
#include "vm/Interpreter-inl.h"
#include "vm/JSScript-inl.h"
#include "vm/StringObject-inl.h"

using mozilla::DebugOnly;

namespace js {
namespace jit {

// Class used to emit all Baseline IC fallback code when initializing the
// JitRuntime.
class MOZ_RAII FallbackICCodeCompiler final : public ICStubCompilerBase {
  BaselineICFallbackCode& code;
  MacroAssembler& masm;

  MOZ_MUST_USE bool emitCall(bool isSpread, bool isConstructing);
  MOZ_MUST_USE bool emitGetElem(bool hasReceiver);
  MOZ_MUST_USE bool emitGetProp(bool hasReceiver);

 public:
  FallbackICCodeCompiler(JSContext* cx, BaselineICFallbackCode& code,
                         MacroAssembler& masm)
      : ICStubCompilerBase(cx), code(code), masm(masm) {}

#define DEF_METHOD(kind) MOZ_MUST_USE bool emit_##kind();
  IC_BASELINE_FALLBACK_CODE_KIND_LIST(DEF_METHOD)
#undef DEF_METHOD
};

#ifdef JS_JITSPEW
void FallbackICSpew(JSContext* cx, ICFallbackStub* stub, const char* fmt, ...) {
  if (JitSpewEnabled(JitSpew_BaselineICFallback)) {
    RootedScript script(cx, GetTopJitJSScript(cx));
    jsbytecode* pc = stub->icEntry()->pc(script);

    char fmtbuf[100];
    va_list args;
    va_start(args, fmt);
    (void)VsprintfLiteral(fmtbuf, fmt, args);
    va_end(args);

    JitSpew(
        JitSpew_BaselineICFallback,
        "Fallback hit for (%s:%u:%u) (pc=%zu,line=%u,uses=%u,stubs=%zu): %s",
        script->filename(), script->lineno(), script->column(),
        script->pcToOffset(pc), PCToLineNumber(script, pc),
        script->getWarmUpCount(), stub->numOptimizedStubs(), fmtbuf);
  }
}

void TypeFallbackICSpew(JSContext* cx, ICTypeMonitor_Fallback* stub,
                        const char* fmt, ...) {
  if (JitSpewEnabled(JitSpew_BaselineICFallback)) {
    RootedScript script(cx, GetTopJitJSScript(cx));
    jsbytecode* pc = stub->icEntry()->pc(script);

    char fmtbuf[100];
    va_list args;
    va_start(args, fmt);
    (void)VsprintfLiteral(fmtbuf, fmt, args);
    va_end(args);

    JitSpew(JitSpew_BaselineICFallback,
            "Type monitor fallback hit for (%s:%u:%u) "
            "(pc=%zu,line=%u,uses=%u,stubs=%d): %s",
            script->filename(), script->lineno(), script->column(),
            script->pcToOffset(pc), PCToLineNumber(script, pc),
            script->getWarmUpCount(), (int)stub->numOptimizedMonitorStubs(),
            fmtbuf);
  }
}
#endif  // JS_JITSPEW

ICFallbackStub* ICEntry::fallbackStub() const {
  return firstStub()->getChainFallback();
}

void ICEntry::trace(JSTracer* trc) {
#ifdef JS_64BIT
  // If we have filled our padding with a magic value, check it now.
  MOZ_DIAGNOSTIC_ASSERT(traceMagic_ == EXPECTED_TRACE_MAGIC);
#endif
  for (ICStub* stub = firstStub(); stub; stub = stub->next()) {
    stub->trace(trc);
  }
}

// Allocator for Baseline IC fallback stubs. These stubs use trampoline code
// stored in JitRuntime.
class MOZ_RAII FallbackStubAllocator {
  JSContext* cx_;
  ICStubSpace& stubSpace_;
  const BaselineICFallbackCode& code_;

 public:
  FallbackStubAllocator(JSContext* cx, ICStubSpace& stubSpace)
      : cx_(cx),
        stubSpace_(stubSpace),
        code_(cx->runtime()->jitRuntime()->baselineICFallbackCode()) {}

  template <typename T, typename... Args>
  T* newStub(BaselineICFallbackKind kind, Args&&... args) {
    TrampolinePtr addr = code_.addr(kind);
    return ICStub::NewFallback<T>(cx_, &stubSpace_, addr,
                                  std::forward<Args>(args)...);
  }
};

// Helper method called by lambda expressions `addIC` and `addPrologueIC` in
// `JitScript::initICEntriesAndBytecodeTypeMap`.
static bool AddICImpl(JSContext* cx, JitScript* jitScript, uint32_t offset,
                      ICStub* stub, uint32_t& icEntryIndex) {
  if (!stub) {
    MOZ_ASSERT(cx->isExceptionPending());
    mozilla::Unused << cx;  // Silence -Wunused-lambda-capture in opt builds.
    return false;
  }

  // Initialize the ICEntry.
  ICEntry& entryRef = jitScript->icEntry(icEntryIndex);
  icEntryIndex++;
  new (&entryRef) ICEntry(stub, offset);

  // Fix up pointers from fallback stubs to the ICEntry.
  if (stub->isFallback()) {
    stub->toFallbackStub()->fixupICEntry(&entryRef);
  } else {
    stub->toTypeMonitor_Fallback()->fixupICEntry(&entryRef);
  }

  return true;
}

bool JitScript::initICEntriesAndBytecodeTypeMap(JSContext* cx,
                                                JSScript* script) {
  MOZ_ASSERT(cx->realm()->jitRealm());
  MOZ_ASSERT(jit::IsBaselineInterpreterEnabled());

  MOZ_ASSERT(numICEntries() == script->numICEntries());

  FallbackStubAllocator alloc(cx, fallbackStubSpace_);

  // Index of the next ICEntry to initialize.
  uint32_t icEntryIndex = 0;

  using Kind = BaselineICFallbackKind;

  auto addIC = [cx, this, script, &icEntryIndex](BytecodeLocation loc,
                                                 ICStub* stub) {
    uint32_t offset = loc.bytecodeToOffset(script);
    return AddICImpl(cx, this, offset, stub, icEntryIndex);
  };

  // Lambda expression for adding ICs for non-op ICs
  auto addPrologueIC = [cx, this, &icEntryIndex](ICStub* stub) {
    return AddICImpl(cx, this, ICEntry::ProloguePCOffset, stub, icEntryIndex);
  };

  if (IsTypeInferenceEnabled()) {
    // Add ICEntries and fallback stubs for this/argument type checks.
    // Note: we pass a nullptr pc to indicate this is a non-op IC.
    // See ICEntry::NonOpPCOffset.
    if (JSFunction* fun = script->function()) {
      ICStub* stub =
          alloc.newStub<ICTypeMonitor_Fallback>(Kind::TypeMonitor, nullptr, 0);
      if (!addPrologueIC(stub)) {
        return false;
      }

      for (size_t i = 0; i < fun->nargs(); i++) {
        ICStub* stub = alloc.newStub<ICTypeMonitor_Fallback>(Kind::TypeMonitor,
                                                             nullptr, i + 1);
        if (!addPrologueIC(stub)) {
          return false;
        }
      }
    }
  }

  // Index of the next bytecode type map entry to initialize.
  uint32_t typeMapIndex = 0;
  uint32_t* const typeMap =
      IsTypeInferenceEnabled() ? bytecodeTypeMap() : nullptr;

  // For JOF_IC ops: initialize ICEntries and fallback stubs.
  // For JOF_TYPESET ops: initialize bytecode type map entries.
  for (BytecodeLocation loc : js::AllBytecodesIterable(script)) {
    JSOp op = loc.getOp();
    // Note: if the script is very large there will be more JOF_TYPESET ops
    // than bytecode type sets. See JSScript::MaxBytecodeTypeSets.
    if (BytecodeOpHasTypeSet(op) && typeMap &&
        typeMapIndex < JSScript::MaxBytecodeTypeSets) {
      typeMap[typeMapIndex] = loc.bytecodeToOffset(script);
      typeMapIndex++;
    }

    // Assert the frontend stored the correct IC index in jump target ops.
    MOZ_ASSERT_IF(BytecodeIsJumpTarget(op), loc.icIndex() == icEntryIndex);

    if (!BytecodeOpHasIC(op)) {
      continue;
    }

    switch (op) {
      case JSOp::Not:
      case JSOp::And:
      case JSOp::Or:
      case JSOp::IfEq:
      case JSOp::IfNe: {
        ICStub* stub = alloc.newStub<ICToBool_Fallback>(Kind::ToBool);
        if (!addIC(loc, stub)) {
          return false;
        }
        break;
      }
      case JSOp::BitNot:
      case JSOp::Pos:
      case JSOp::Neg:
      case JSOp::Inc:
      case JSOp::Dec: {
        ICStub* stub = alloc.newStub<ICUnaryArith_Fallback>(Kind::UnaryArith);
        if (!addIC(loc, stub)) {
          return false;
        }
        break;
      }
      case JSOp::BitOr:
      case JSOp::BitXor:
      case JSOp::BitAnd:
      case JSOp::Lsh:
      case JSOp::Rsh:
      case JSOp::Ursh:
      case JSOp::Add:
      case JSOp::Sub:
      case JSOp::Mul:
      case JSOp::Div:
      case JSOp::Mod:
      case JSOp::Pow: {
        ICStub* stub = alloc.newStub<ICBinaryArith_Fallback>(Kind::BinaryArith);
        if (!addIC(loc, stub)) {
          return false;
        }
        break;
      }
      case JSOp::Eq:
      case JSOp::Ne:
      case JSOp::Lt:
      case JSOp::Le:
      case JSOp::Gt:
      case JSOp::Ge:
      case JSOp::StrictEq:
      case JSOp::StrictNe: {
        ICStub* stub = alloc.newStub<ICCompare_Fallback>(Kind::Compare);
        if (!addIC(loc, stub)) {
          return false;
        }
        break;
      }
      case JSOp::NewArray: {
        ObjectGroup* group = ObjectGroup::allocationSiteGroup(
            cx, script, loc.toRawBytecode(), JSProto_Array);
        if (!group) {
          return false;
        }
        ICStub* stub =
            alloc.newStub<ICNewArray_Fallback>(Kind::NewArray, group);
        if (!addIC(loc, stub)) {
          return false;
        }
        break;
      }
      case JSOp::NewObject:
      case JSOp::NewObjectWithGroup:
      case JSOp::NewInit: {
        ICStub* stub = alloc.newStub<ICNewObject_Fallback>(Kind::NewObject);
        if (!addIC(loc, stub)) {
          return false;
        }
        break;
      }
      case JSOp::InitElem:
      case JSOp::InitHiddenElem:
      case JSOp::InitElemArray:
      case JSOp::InitElemInc:
      case JSOp::SetElem:
      case JSOp::StrictSetElem: {
        ICStub* stub = alloc.newStub<ICSetElem_Fallback>(Kind::SetElem);
        if (!addIC(loc, stub)) {
          return false;
        }
        break;
      }
      case JSOp::InitProp:
      case JSOp::InitLockedProp:
      case JSOp::InitHiddenProp:
      case JSOp::InitGLexical:
      case JSOp::SetProp:
      case JSOp::StrictSetProp:
      case JSOp::SetName:
      case JSOp::StrictSetName:
      case JSOp::SetGName:
      case JSOp::StrictSetGName: {
        ICStub* stub = alloc.newStub<ICSetProp_Fallback>(Kind::SetProp);
        if (!addIC(loc, stub)) {
          return false;
        }
        break;
      }
      case JSOp::GetProp:
      case JSOp::CallProp:
      case JSOp::Length:
      case JSOp::GetBoundName: {
        ICStub* stub = alloc.newStub<ICGetProp_Fallback>(Kind::GetProp);
        if (!addIC(loc, stub)) {
          return false;
        }
        break;
      }
      case JSOp::GetPropSuper: {
        ICStub* stub = alloc.newStub<ICGetProp_Fallback>(Kind::GetPropSuper);
        if (!addIC(loc, stub)) {
          return false;
        }
        break;
      }
      case JSOp::GetElem:
      case JSOp::CallElem: {
        ICStub* stub = alloc.newStub<ICGetElem_Fallback>(Kind::GetElem);
        if (!addIC(loc, stub)) {
          return false;
        }
        break;
      }
      case JSOp::GetElemSuper: {
        ICStub* stub = alloc.newStub<ICGetElem_Fallback>(Kind::GetElemSuper);
        if (!addIC(loc, stub)) {
          return false;
        }
        break;
      }
      case JSOp::In: {
        ICStub* stub = alloc.newStub<ICIn_Fallback>(Kind::In);
        if (!addIC(loc, stub)) {
          return false;
        }
        break;
      }
      case JSOp::HasOwn: {
        ICStub* stub = alloc.newStub<ICHasOwn_Fallback>(Kind::HasOwn);
        if (!addIC(loc, stub)) {
          return false;
        }
        break;
      }
      case JSOp::GetName:
      case JSOp::GetGName: {
        ICStub* stub = alloc.newStub<ICGetName_Fallback>(Kind::GetName);
        if (!addIC(loc, stub)) {
          return false;
        }
        break;
      }
      case JSOp::BindName:
      case JSOp::BindGName: {
        ICStub* stub = alloc.newStub<ICBindName_Fallback>(Kind::BindName);
        if (!addIC(loc, stub)) {
          return false;
        }
        break;
      }
      case JSOp::GetAliasedVar:
      case JSOp::GetImport: {
        ICStub* stub =
            alloc.newStub<ICTypeMonitor_Fallback>(Kind::TypeMonitor, nullptr);
        if (!addIC(loc, stub)) {
          return false;
        }
        break;
      }
      case JSOp::GetIntrinsic: {
        ICStub* stub =
            alloc.newStub<ICGetIntrinsic_Fallback>(Kind::GetIntrinsic);
        if (!addIC(loc, stub)) {
          return false;
        }
        break;
      }
      case JSOp::Call:
      case JSOp::CallIgnoresRv:
      case JSOp::CallIter:
      case JSOp::FunCall:
      case JSOp::FunApply:
      case JSOp::Eval:
      case JSOp::StrictEval: {
        ICStub* stub = alloc.newStub<ICCall_Fallback>(Kind::Call);
        if (!addIC(loc, stub)) {
          return false;
        }
        break;
      }
      case JSOp::SuperCall:
      case JSOp::New: {
        ICStub* stub = alloc.newStub<ICCall_Fallback>(Kind::CallConstructing);
        if (!addIC(loc, stub)) {
          return false;
        }
        break;
      }
      case JSOp::SpreadCall:
      case JSOp::SpreadEval:
      case JSOp::StrictSpreadEval: {
        ICStub* stub = alloc.newStub<ICCall_Fallback>(Kind::SpreadCall);
        if (!addIC(loc, stub)) {
          return false;
        }
        break;
      }
      case JSOp::SpreadSuperCall:
      case JSOp::SpreadNew: {
        ICStub* stub =
            alloc.newStub<ICCall_Fallback>(Kind::SpreadCallConstructing);
        if (!addIC(loc, stub)) {
          return false;
        }
        break;
      }
      case JSOp::Instanceof: {
        ICStub* stub = alloc.newStub<ICInstanceOf_Fallback>(Kind::InstanceOf);
        if (!addIC(loc, stub)) {
          return false;
        }
        break;
      }
      case JSOp::Typeof:
      case JSOp::TypeofExpr: {
        ICStub* stub = alloc.newStub<ICTypeOf_Fallback>(Kind::TypeOf);
        if (!addIC(loc, stub)) {
          return false;
        }
        break;
      }
      case JSOp::Iter: {
        ICStub* stub = alloc.newStub<ICGetIterator_Fallback>(Kind::GetIterator);
        if (!addIC(loc, stub)) {
          return false;
        }
        break;
      }
      case JSOp::Rest: {
        ArrayObject* templateObject = ObjectGroup::newArrayObject(
            cx, nullptr, 0, TenuredObject,
            ObjectGroup::NewArrayKind::UnknownIndex);
        if (!templateObject) {
          return false;
        }
        ICStub* stub =
            alloc.newStub<ICRest_Fallback>(Kind::Rest, templateObject);
        if (!addIC(loc, stub)) {
          return false;
        }
        break;
      }
      default:
        MOZ_CRASH("JOF_IC op not handled");
    }
  }

  // Assert all ICEntries and type map entries have been initialized.
  MOZ_ASSERT(icEntryIndex == numICEntries());
  MOZ_ASSERT_IF(IsTypeInferenceEnabled(),
                typeMapIndex == script->numBytecodeTypeSets());

  return true;
}

ICStubConstIterator& ICStubConstIterator::operator++() {
  MOZ_ASSERT(currentStub_ != nullptr);
  currentStub_ = currentStub_->next();
  return *this;
}

ICStubIterator::ICStubIterator(ICFallbackStub* fallbackStub, bool end)
    : icEntry_(fallbackStub->icEntry()),
      fallbackStub_(fallbackStub),
      previousStub_(nullptr),
      currentStub_(end ? fallbackStub : icEntry_->firstStub()),
      unlinked_(false) {}

ICStubIterator& ICStubIterator::operator++() {
  MOZ_ASSERT(currentStub_->next() != nullptr);
  if (!unlinked_) {
    previousStub_ = currentStub_;
  }
  currentStub_ = currentStub_->next();
  unlinked_ = false;
  return *this;
}

void ICStubIterator::unlink(JSContext* cx) {
  MOZ_ASSERT(currentStub_->next() != nullptr);
  MOZ_ASSERT(currentStub_ != fallbackStub_);
  MOZ_ASSERT(!unlinked_);

  fallbackStub_->unlinkStub(cx->zone(), previousStub_, currentStub_);

  // Mark the current iterator position as unlinked, so operator++ works
  // properly.
  unlinked_ = true;
}

/* static */
bool ICStub::NonCacheIRStubMakesGCCalls(Kind kind) {
  MOZ_ASSERT(IsValidKind(kind));
  MOZ_ASSERT(!IsCacheIRKind(kind));

  switch (kind) {
    case Call_Fallback:
    // These three fallback stubs don't actually make non-tail calls,
    // but the fallback code for the bailout path needs to pop the stub frame
    // pushed during the bailout.
    case GetProp_Fallback:
    case SetProp_Fallback:
    case GetElem_Fallback:
      return true;
    default:
      return false;
  }
}

bool ICStub::makesGCCalls() const {
  switch (kind()) {
    case CacheIR_Regular:
      return toCacheIR_Regular()->stubInfo()->makesGCCalls();
    case CacheIR_Monitored:
      return toCacheIR_Monitored()->stubInfo()->makesGCCalls();
    case CacheIR_Updated:
      return toCacheIR_Updated()->stubInfo()->makesGCCalls();
    default:
      return NonCacheIRStubMakesGCCalls(kind());
  }
}

void ICStub::updateCode(JitCode* code) {
  // Write barrier on the old code.
  JitCode::writeBarrierPre(jitCode());
  stubCode_ = code->raw();
}

/* static */
void ICStub::trace(JSTracer* trc) {
#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
  checkTraceMagic();
#endif
  // Fallback stubs use runtime-wide trampoline code we don't need to trace.
  if (!usesTrampolineCode()) {
    JitCode* stubJitCode = jitCode();
    TraceManuallyBarrieredEdge(trc, &stubJitCode, "baseline-ic-stub-code");
  }

  // If the stub is a monitored fallback stub, then trace the monitor ICs
  // hanging off of that stub.  We don't need to worry about the regular
  // monitored stubs, because the regular monitored stubs will always have a
  // monitored fallback stub that references the same stub chain.
  if (isMonitoredFallback()) {
    ICTypeMonitor_Fallback* lastMonStub =
        toMonitoredFallbackStub()->maybeFallbackMonitorStub();
    if (lastMonStub) {
      for (ICStubConstIterator iter(lastMonStub->firstMonitorStub());
           !iter.atEnd(); iter++) {
        MOZ_ASSERT_IF(iter->next() == nullptr, *iter == lastMonStub);
        iter->trace(trc);
      }
    }
  }

  if (isUpdated()) {
    for (ICStubConstIterator iter(toUpdatedStub()->firstUpdateStub());
         !iter.atEnd(); iter++) {
      MOZ_ASSERT_IF(iter->next() == nullptr, iter->isTypeUpdate_Fallback());
      iter->trace(trc);
    }
  }

  switch (kind()) {
    case ICStub::TypeMonitor_SingleObject: {
      ICTypeMonitor_SingleObject* monitorStub = toTypeMonitor_SingleObject();
      TraceEdge(trc, &monitorStub->object(), "baseline-monitor-singleton");
      break;
    }
    case ICStub::TypeMonitor_ObjectGroup: {
      ICTypeMonitor_ObjectGroup* monitorStub = toTypeMonitor_ObjectGroup();
      TraceEdge(trc, &monitorStub->group(), "baseline-monitor-group");
      break;
    }
    case ICStub::TypeUpdate_SingleObject: {
      ICTypeUpdate_SingleObject* updateStub = toTypeUpdate_SingleObject();
      TraceEdge(trc, &updateStub->object(), "baseline-update-singleton");
      break;
    }
    case ICStub::TypeUpdate_ObjectGroup: {
      ICTypeUpdate_ObjectGroup* updateStub = toTypeUpdate_ObjectGroup();
      TraceEdge(trc, &updateStub->group(), "baseline-update-group");
      break;
    }
    case ICStub::NewArray_Fallback: {
      ICNewArray_Fallback* stub = toNewArray_Fallback();
      TraceNullableEdge(trc, &stub->templateObject(),
                        "baseline-newarray-template");
      TraceEdge(trc, &stub->templateGroup(),
                "baseline-newarray-template-group");
      break;
    }
    case ICStub::NewObject_Fallback: {
      ICNewObject_Fallback* stub = toNewObject_Fallback();
      TraceNullableEdge(trc, &stub->templateObject(),
                        "baseline-newobject-template");
      break;
    }
    case ICStub::Rest_Fallback: {
      ICRest_Fallback* stub = toRest_Fallback();
      TraceEdge(trc, &stub->templateObject(), "baseline-rest-template");
      break;
    }
    case ICStub::CacheIR_Regular:
      TraceCacheIRStub(trc, this, toCacheIR_Regular()->stubInfo());
      break;
    case ICStub::CacheIR_Monitored:
      TraceCacheIRStub(trc, this, toCacheIR_Monitored()->stubInfo());
      break;
    case ICStub::CacheIR_Updated: {
      ICCacheIR_Updated* stub = toCacheIR_Updated();
      TraceNullableEdge(trc, &stub->updateStubGroup(),
                        "baseline-update-stub-group");
      TraceEdge(trc, &stub->updateStubId(), "baseline-update-stub-id");
      TraceCacheIRStub(trc, this, stub->stubInfo());
      break;
    }
    default:
      break;
  }
}

// This helper handles ICState updates/transitions while attaching CacheIR
// stubs.
template <typename IRGenerator, typename... Args>
static void TryAttachStub(const char* name, JSContext* cx, BaselineFrame* frame,
                          ICFallbackStub* stub, BaselineCacheIRStubKind kind,
                          Args&&... args) {
  if (stub->state().maybeTransition()) {
    stub->discardStubs(cx);
  }

  if (stub->state().canAttachStub()) {
    RootedScript script(cx, frame->script());
    jsbytecode* pc = stub->icEntry()->pc(script);

    bool attached = false;
    IRGenerator gen(cx, script, pc, stub->state().mode(),
                    std::forward<Args>(args)...);
    switch (gen.tryAttachStub()) {
      case AttachDecision::Attach: {
        ICStub* newStub =
            AttachBaselineCacheIRStub(cx, gen.writerRef(), gen.cacheKind(),
                                      kind, script, stub, &attached);
        if (newStub) {
          JitSpew(JitSpew_BaselineIC, "  Attached %s CacheIR stub", name);
        }
      } break;
      case AttachDecision::NoAction:
        break;
      case AttachDecision::TemporarilyUnoptimizable:
      case AttachDecision::Deferred:
        MOZ_ASSERT_UNREACHABLE("Not expected in generic TryAttachStub");
        break;
    }
    if (!attached) {
      stub->state().trackNotAttached();
    }
  }
}

void ICFallbackStub::unlinkStub(Zone* zone, ICStub* prev, ICStub* stub) {
  MOZ_ASSERT(stub->next());

  // If stub is the last optimized stub, update lastStubPtrAddr.
  if (stub->next() == this) {
    MOZ_ASSERT(lastStubPtrAddr_ == stub->addressOfNext());
    if (prev) {
      lastStubPtrAddr_ = prev->addressOfNext();
    } else {
      lastStubPtrAddr_ = icEntry()->addressOfFirstStub();
    }
    *lastStubPtrAddr_ = this;
  } else {
    if (prev) {
      MOZ_ASSERT(prev->next() == stub);
      prev->setNext(stub->next());
    } else {
      MOZ_ASSERT(icEntry()->firstStub() == stub);
      icEntry()->setFirstStub(stub->next());
    }
  }

  state_.trackUnlinkedStub();

  if (zone->needsIncrementalBarrier()) {
    // We are removing edges from ICStub to gcthings. Perform one final trace
    // of the stub for incremental GC, as it must know about those edges.
    stub->trace(zone->barrierTracer());
  }

  if (IsTypeInferenceEnabled() && stub->makesGCCalls() && stub->isMonitored()) {
    // This stub can make calls so we can return to it if it's on the stack.
    // We just have to reset its firstMonitorStub_ field to avoid a stale
    // pointer when purgeOptimizedStubs destroys all optimized monitor
    // stubs (unlinked stubs won't be updated).
    ICTypeMonitor_Fallback* monitorFallback =
        toMonitoredFallbackStub()->maybeFallbackMonitorStub();
    MOZ_ASSERT(monitorFallback);
    stub->toMonitoredStub()->resetFirstMonitorStub(monitorFallback);
  }

#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
  stub->checkTraceMagic();
#endif
#ifdef DEBUG
  // Poison stub code to ensure we don't call this stub again. However, if
  // this stub can make calls, a pointer to it may be stored in a stub frame
  // on the stack, so we can't touch the stubCode_ or GC will crash when
  // tracing this pointer.
  if (!stub->makesGCCalls()) {
    stub->stubCode_ = (uint8_t*)0xbad;
  }
#endif
}

void ICFallbackStub::unlinkStubsWithKind(JSContext* cx, ICStub::Kind kind) {
  for (ICStubIterator iter = beginChain(); !iter.atEnd(); iter++) {
    if (iter->kind() == kind) {
      iter.unlink(cx);
    }
  }
}

void ICFallbackStub::discardStubs(JSContext* cx) {
  for (ICStubIterator iter = beginChain(); !iter.atEnd(); iter++) {
    iter.unlink(cx);
  }
}

void ICTypeMonitor_Fallback::resetMonitorStubChain(Zone* zone) {
  if (zone->needsIncrementalBarrier()) {
    // We are removing edges from monitored stubs to gcthings (JitCode).
    // Perform one final trace of all monitor stubs for incremental GC,
    // as it must know about those edges.
    for (ICStub* s = firstMonitorStub_; !s->isTypeMonitor_Fallback();
         s = s->next()) {
      s->trace(zone->barrierTracer());
    }
  }

  firstMonitorStub_ = this;
  numOptimizedMonitorStubs_ = 0;

  if (hasFallbackStub_) {
    lastMonitorStubPtrAddr_ = nullptr;

    // Reset firstMonitorStub_ field of all monitored stubs.
    for (ICStubConstIterator iter = mainFallbackStub_->beginChainConst();
         !iter.atEnd(); iter++) {
      if (!iter->isMonitored()) {
        continue;
      }
      iter->toMonitoredStub()->resetFirstMonitorStub(this);
    }
  } else {
    icEntry_->setFirstStub(this);
    lastMonitorStubPtrAddr_ = icEntry_->addressOfFirstStub();
  }
}

void ICCacheIR_Updated::resetUpdateStubChain(Zone* zone) {
  while (!firstUpdateStub_->isTypeUpdate_Fallback()) {
    if (zone->needsIncrementalBarrier()) {
      // We are removing edges from update stubs to gcthings (JitCode).
      // Perform one final trace of all update stubs for incremental GC,
      // as it must know about those edges.
      firstUpdateStub_->trace(zone->barrierTracer());
    }
    firstUpdateStub_ = firstUpdateStub_->next();
  }

  numOptimizedStubs_ = 0;
}

ICMonitoredStub::ICMonitoredStub(Kind kind, JitCode* stubCode,
                                 ICStub* firstMonitorStub)
    : ICStub(kind, ICStub::Monitored, stubCode),
      firstMonitorStub_(firstMonitorStub) {
  if (IsTypeInferenceEnabled()) {
    // In order to silence Coverity - null pointer dereference checker
    MOZ_ASSERT(firstMonitorStub_);
    // If the first monitored stub is a ICTypeMonitor_Fallback stub, then
    // double check that _its_ firstMonitorStub is the same as this one.
    MOZ_ASSERT_IF(
        firstMonitorStub_->isTypeMonitor_Fallback(),
        firstMonitorStub_->toTypeMonitor_Fallback()->firstMonitorStub() ==
            firstMonitorStub_);
  } else {
    MOZ_ASSERT(!firstMonitorStub_);
  }
}

bool ICMonitoredFallbackStub::initMonitoringChain(JSContext* cx,
                                                  JSScript* script) {
  MOZ_ASSERT(fallbackMonitorStub_ == nullptr);
  MOZ_ASSERT(IsTypeInferenceEnabled());

  ICStubSpace* space = script->jitScript()->fallbackStubSpace();
  FallbackStubAllocator alloc(cx, *space);
  auto* stub = alloc.newStub<ICTypeMonitor_Fallback>(
      BaselineICFallbackKind::TypeMonitor, this);
  if (!stub) {
    return false;
  }

  fallbackMonitorStub_ = stub;
  return true;
}

bool TypeMonitorResult(JSContext* cx, ICMonitoredFallbackStub* stub,
                       BaselineFrame* frame, HandleScript script,
                       jsbytecode* pc, HandleValue val) {
  if (!IsTypeInferenceEnabled()) {
    return true;
  }

  ICTypeMonitor_Fallback* typeMonitorFallback =
      stub->getFallbackMonitorStub(cx, script);
  if (!typeMonitorFallback) {
    return false;
  }

  AutoSweepJitScript sweep(script);
  StackTypeSet* types = script->jitScript()->bytecodeTypes(sweep, script, pc);
  JitScript::MonitorBytecodeType(cx, script, pc, types, val);

  return typeMonitorFallback->addMonitorStubForValue(cx, frame, types, val);
}

bool ICCacheIR_Updated::initUpdatingChain(JSContext* cx, ICStubSpace* space) {
  MOZ_ASSERT(firstUpdateStub_ == nullptr);

  FallbackStubAllocator alloc(cx, *space);
  auto* stub =
      alloc.newStub<ICTypeUpdate_Fallback>(BaselineICFallbackKind::TypeUpdate);
  if (!stub) {
    return false;
  }

  firstUpdateStub_ = stub;
  return true;
}

/* static */
ICStubSpace* ICStubCompiler::StubSpaceForStub(bool makesGCCalls,
                                              JSScript* script) {
  if (makesGCCalls) {
    return script->jitScript()->fallbackStubSpace();
  }
  return script->zone()->jitZone()->optimizedStubSpace();
}

static void InitMacroAssemblerForICStub(StackMacroAssembler& masm) {
#ifndef JS_USE_LINK_REGISTER
  // The first value contains the return addres,
  // which we pull into ICTailCallReg for tail calls.
  masm.adjustFrame(sizeof(intptr_t));
#endif
#ifdef JS_CODEGEN_ARM
  masm.setSecondScratchReg(BaselineSecondScratchReg);
#endif
}

JitCode* ICStubCompiler::getStubCode() {
  JitRealm* realm = cx->realm()->jitRealm();

  // Check for existing cached stubcode.
  uint32_t stubKey = getKey();
  JitCode* stubCode = realm->getStubCode(stubKey);
  if (stubCode) {
    return stubCode;
  }

  // Compile new stubcode.
  JitContext jctx(cx, nullptr);
  StackMacroAssembler masm;
  InitMacroAssemblerForICStub(masm);

  if (!generateStubCode(masm)) {
    return nullptr;
  }
  Linker linker(masm);
  Rooted<JitCode*> newStubCode(cx, linker.newCode(cx, CodeKind::Baseline));
  if (!newStubCode) {
    return nullptr;
  }

  // Cache newly compiled stubcode.
  if (!realm->putStubCode(cx, stubKey, newStubCode)) {
    return nullptr;
  }

  MOZ_ASSERT(entersStubFrame_ == ICStub::NonCacheIRStubMakesGCCalls(kind));
  MOZ_ASSERT(!inStubFrame_);

#ifdef JS_ION_PERF
  writePerfSpewerJitCodeProfile(newStubCode, "BaselineIC");
#endif

  return newStubCode;
}

bool ICStubCompilerBase::tailCallVMInternal(MacroAssembler& masm,
                                            TailCallVMFunctionId id) {
  TrampolinePtr code = cx->runtime()->jitRuntime()->getVMWrapper(id);
  const VMFunctionData& fun = GetVMFunction(id);
  MOZ_ASSERT(fun.expectTailCall == TailCall);
  uint32_t argSize = fun.explicitStackSlots() * sizeof(void*);
  EmitBaselineTailCallVM(code, masm, argSize);
  return true;
}

bool ICStubCompilerBase::callVMInternal(MacroAssembler& masm, VMFunctionId id) {
  MOZ_ASSERT(inStubFrame_);

  TrampolinePtr code = cx->runtime()->jitRuntime()->getVMWrapper(id);
  MOZ_ASSERT(GetVMFunction(id).expectTailCall == NonTailCall);

  EmitBaselineCallVM(code, masm);
  return true;
}

template <typename Fn, Fn fn>
bool ICStubCompilerBase::callVM(MacroAssembler& masm) {
  VMFunctionId id = VMFunctionToId<Fn, fn>::id;
  return callVMInternal(masm, id);
}

template <typename Fn, Fn fn>
bool ICStubCompilerBase::tailCallVM(MacroAssembler& masm) {
  TailCallVMFunctionId id = TailCallVMFunctionToId<Fn, fn>::id;
  return tailCallVMInternal(masm, id);
}

void ICStubCompilerBase::enterStubFrame(MacroAssembler& masm,
                                        Register scratch) {
  EmitBaselineEnterStubFrame(masm, scratch);
#ifdef DEBUG
  framePushedAtEnterStubFrame_ = masm.framePushed();
#endif

  MOZ_ASSERT(!inStubFrame_);
  inStubFrame_ = true;

#ifdef DEBUG
  entersStubFrame_ = true;
#endif
}

void ICStubCompilerBase::assumeStubFrame() {
  MOZ_ASSERT(!inStubFrame_);
  inStubFrame_ = true;

#ifdef DEBUG
  entersStubFrame_ = true;

  // |framePushed| isn't tracked precisely in ICStubs, so simply assume it to
  // be STUB_FRAME_SIZE so that assertions don't fail in leaveStubFrame.
  framePushedAtEnterStubFrame_ = STUB_FRAME_SIZE;
#endif
}

void ICStubCompilerBase::leaveStubFrame(MacroAssembler& masm,
                                        bool calledIntoIon) {
  MOZ_ASSERT(entersStubFrame_ && inStubFrame_);
  inStubFrame_ = false;

#ifdef DEBUG
  masm.setFramePushed(framePushedAtEnterStubFrame_);
  if (calledIntoIon) {
    masm.adjustFrame(sizeof(intptr_t));  // Calls into ion have this extra.
  }
#endif
  EmitBaselineLeaveStubFrame(masm, calledIntoIon);
}

void ICStubCompilerBase::pushStubPayload(MacroAssembler& masm,
                                         Register scratch) {
  if (inStubFrame_) {
    masm.loadPtr(Address(BaselineFrameReg, 0), scratch);
    masm.pushBaselineFramePtr(scratch, scratch);
  } else {
    masm.pushBaselineFramePtr(BaselineFrameReg, scratch);
  }
}

void ICStubCompilerBase::PushStubPayload(MacroAssembler& masm,
                                         Register scratch) {
  pushStubPayload(masm, scratch);
  masm.adjustFrame(sizeof(intptr_t));
}

// TypeMonitor_Fallback
//

bool ICTypeMonitor_Fallback::addMonitorStubForValue(JSContext* cx,
                                                    BaselineFrame* frame,
                                                    StackTypeSet* types,
                                                    HandleValue val) {
  MOZ_ASSERT(types);

  if (MOZ_UNLIKELY(val.isMagic())) {
    return true;
  }

  // Don't attach too many SingleObject/ObjectGroup stubs. If the value is a
  // primitive or if we will attach an any-object stub, we can handle this
  // with a single PrimitiveSet or AnyValue stub so we always optimize.
  if (numOptimizedMonitorStubs_ >= MAX_OPTIMIZED_STUBS && val.isObject() &&
      !types->unknownObject()) {
    return true;
  }

  bool wasDetachedMonitorChain = lastMonitorStubPtrAddr_ == nullptr;
  MOZ_ASSERT_IF(wasDetachedMonitorChain, numOptimizedMonitorStubs_ == 0);

  if (types->unknown()) {
    // The TypeSet got marked as unknown so attach a stub that always
    // succeeds.

    // Check for existing TypeMonitor_AnyValue stubs.
    for (ICStubConstIterator iter(firstMonitorStub()); !iter.atEnd(); iter++) {
      if (iter->isTypeMonitor_AnyValue()) {
        return true;
      }
    }

    // Discard existing stubs.
    resetMonitorStubChain(cx->zone());
    wasDetachedMonitorChain = (lastMonitorStubPtrAddr_ == nullptr);

    ICTypeMonitor_AnyValue::Compiler compiler(cx);
    ICStub* stub = compiler.getStub(compiler.getStubSpace(frame->script()));
    if (!stub) {
      ReportOutOfMemory(cx);
      return false;
    }

    JitSpew(JitSpew_BaselineIC, "  Added TypeMonitor stub %p for any value",
            stub);
    addOptimizedMonitorStub(stub);

  } else if (val.isPrimitive() || types->unknownObject()) {
    ValueType type = val.type();

    // Check for existing TypeMonitor stub.
    ICTypeMonitor_PrimitiveSet* existingStub = nullptr;
    for (ICStubConstIterator iter(firstMonitorStub()); !iter.atEnd(); iter++) {
      if (iter->isTypeMonitor_PrimitiveSet()) {
        existingStub = iter->toTypeMonitor_PrimitiveSet();
        if (existingStub->containsType(type)) {
          return true;
        }
      }
    }

    if (val.isObject()) {
      // Check for existing SingleObject/ObjectGroup stubs and discard
      // stubs if we find one. Ideally we would discard just these stubs,
      // but unlinking individual type monitor stubs is somewhat
      // complicated.
      MOZ_ASSERT(types->unknownObject());
      bool hasObjectStubs = false;
      for (ICStubConstIterator iter(firstMonitorStub()); !iter.atEnd();
           iter++) {
        if (iter->isTypeMonitor_SingleObject() ||
            iter->isTypeMonitor_ObjectGroup()) {
          hasObjectStubs = true;
          break;
        }
      }
      if (hasObjectStubs) {
        resetMonitorStubChain(cx->zone());
        wasDetachedMonitorChain = (lastMonitorStubPtrAddr_ == nullptr);
        existingStub = nullptr;
      }
    }

    ICTypeMonitor_PrimitiveSet::Compiler compiler(cx, existingStub, type);
    ICStub* stub =
        existingStub ? compiler.updateStub()
                     : compiler.getStub(compiler.getStubSpace(frame->script()));
    if (!stub) {
      ReportOutOfMemory(cx);
      return false;
    }

    JitSpew(JitSpew_BaselineIC,
            "  %s TypeMonitor stub %p for primitive type %u",
            existingStub ? "Modified existing" : "Created new", stub,
            static_cast<uint8_t>(type));

    if (!existingStub) {
      MOZ_ASSERT(!hasStub(TypeMonitor_PrimitiveSet));
      addOptimizedMonitorStub(stub);
    }

  } else if (val.toObject().isSingleton()) {
    RootedObject obj(cx, &val.toObject());

    // Check for existing TypeMonitor stub.
    for (ICStubConstIterator iter(firstMonitorStub()); !iter.atEnd(); iter++) {
      if (iter->isTypeMonitor_SingleObject() &&
          iter->toTypeMonitor_SingleObject()->object() == obj) {
        return true;
      }
    }

    ICTypeMonitor_SingleObject::Compiler compiler(cx, obj);
    ICStub* stub = compiler.getStub(compiler.getStubSpace(frame->script()));
    if (!stub) {
      ReportOutOfMemory(cx);
      return false;
    }

    JitSpew(JitSpew_BaselineIC, "  Added TypeMonitor stub %p for singleton %p",
            stub, obj.get());

    addOptimizedMonitorStub(stub);

  } else {
    RootedObjectGroup group(cx, val.toObject().group());

    // Check for existing TypeMonitor stub.
    for (ICStubConstIterator iter(firstMonitorStub()); !iter.atEnd(); iter++) {
      if (iter->isTypeMonitor_ObjectGroup() &&
          iter->toTypeMonitor_ObjectGroup()->group() == group) {
        return true;
      }
    }

    ICTypeMonitor_ObjectGroup::Compiler compiler(cx, group);
    ICStub* stub = compiler.getStub(compiler.getStubSpace(frame->script()));
    if (!stub) {
      ReportOutOfMemory(cx);
      return false;
    }

    JitSpew(JitSpew_BaselineIC,
            "  Added TypeMonitor stub %p for ObjectGroup %p", stub,
            group.get());

    addOptimizedMonitorStub(stub);
  }

  bool firstMonitorStubAdded =
      wasDetachedMonitorChain && (numOptimizedMonitorStubs_ > 0);

  if (firstMonitorStubAdded) {
    // Was an empty monitor chain before, but a new stub was added.  This is the
    // only time that any main stubs' firstMonitorStub fields need to be updated
    // to refer to the newly added monitor stub.
    ICStub* firstStub = mainFallbackStub_->icEntry()->firstStub();
    for (ICStubConstIterator iter(firstStub); !iter.atEnd(); iter++) {
      // Non-monitored stubs are used if the result has always the same type,
      // e.g. a StringLength stub will always return int32.
      if (!iter->isMonitored()) {
        continue;
      }

      // Since we just added the first optimized monitoring stub, any
      // existing main stub's |firstMonitorStub| MUST be pointing to the
      // fallback monitor stub (i.e. this stub).
      MOZ_ASSERT(iter->toMonitoredStub()->firstMonitorStub() == this);
      iter->toMonitoredStub()->updateFirstMonitorStub(firstMonitorStub_);
    }
  }

  return true;
}

bool DoTypeMonitorFallback(JSContext* cx, BaselineFrame* frame,
                           ICTypeMonitor_Fallback* stub, HandleValue value,
                           MutableHandleValue res) {
  MOZ_ASSERT(IsTypeInferenceEnabled());

  JSScript* script = frame->script();
  jsbytecode* pc = stub->icEntry()->pc(script);
  TypeFallbackICSpew(cx, stub, "TypeMonitor");

  // Copy input value to res.
  res.set(value);

  JitScript* jitScript = script->jitScript();
  AutoSweepJitScript sweep(script);

  StackTypeSet* types;
  uint32_t argument;
  if (stub->monitorsArgument(&argument)) {
    MOZ_ASSERT(pc == script->code());
    types = jitScript->argTypes(sweep, script, argument);
    JitScript::MonitorArgType(cx, script, argument, value);
  } else if (stub->monitorsThis()) {
    MOZ_ASSERT(pc == script->code());
    types = jitScript->thisTypes(sweep, script);
    JitScript::MonitorThisType(cx, script, value);
  } else {
    types = jitScript->bytecodeTypes(sweep, script, pc);
    JitScript::MonitorBytecodeType(cx, script, pc, types, value);
  }

  return stub->addMonitorStubForValue(cx, frame, types, value);
}

bool FallbackICCodeCompiler::emit_TypeMonitor() {
  static_assert(R0 == JSReturnOperand);

  // Restore the tail call register.
  EmitRestoreTailCallReg(masm);

  masm.pushValue(R0);
  masm.push(ICStubReg);
  masm.pushBaselineFramePtr(BaselineFrameReg, R0.scratchReg());

  using Fn = bool (*)(JSContext*, BaselineFrame*, ICTypeMonitor_Fallback*,
                      HandleValue, MutableHandleValue);
  return tailCallVM<Fn, DoTypeMonitorFallback>(masm);
}

bool ICTypeMonitor_PrimitiveSet::Compiler::generateStubCode(
    MacroAssembler& masm) {
  Label success;
  if ((flags_ & TypeToFlag(ValueType::Int32)) &&
      !(flags_ & TypeToFlag(ValueType::Double))) {
    masm.branchTestInt32(Assembler::Equal, R0, &success);
  }

  if (flags_ & TypeToFlag(ValueType::Double)) {
    masm.branchTestNumber(Assembler::Equal, R0, &success);
  }

  if (flags_ & TypeToFlag(ValueType::Undefined)) {
    masm.branchTestUndefined(Assembler::Equal, R0, &success);
  }

  if (flags_ & TypeToFlag(ValueType::Boolean)) {
    masm.branchTestBoolean(Assembler::Equal, R0, &success);
  }

  if (flags_ & TypeToFlag(ValueType::String)) {
    masm.branchTestString(Assembler::Equal, R0, &success);
  }

  if (flags_ & TypeToFlag(ValueType::Symbol)) {
    masm.branchTestSymbol(Assembler::Equal, R0, &success);
  }

  if (flags_ & TypeToFlag(ValueType::BigInt)) {
    masm.branchTestBigInt(Assembler::Equal, R0, &success);
  }

  if (flags_ & TypeToFlag(ValueType::Object)) {
    masm.branchTestObject(Assembler::Equal, R0, &success);
  }

  if (flags_ & TypeToFlag(ValueType::Null)) {
    masm.branchTestNull(Assembler::Equal, R0, &success);
  }

  EmitStubGuardFailure(masm);

  masm.bind(&success);
  EmitReturnFromIC(masm);
  return true;
}

static void MaybeWorkAroundAmdBug(MacroAssembler& masm) {
  // Attempt to work around an AMD bug (see bug 1034706 and bug 1281759), by
  // inserting 32-bytes of NOPs.
#if defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_X64)
  if (CPUInfo::NeedAmdBugWorkaround()) {
    masm.nop(9);
    masm.nop(9);
    masm.nop(9);
    masm.nop(5);
  }
#endif
}

bool ICTypeMonitor_SingleObject::Compiler::generateStubCode(
    MacroAssembler& masm) {
  Label failure;
  masm.branchTestObject(Assembler::NotEqual, R0, &failure);
  MaybeWorkAroundAmdBug(masm);

  // Guard on the object's identity.
  Register obj = masm.extractObject(R0, ExtractTemp0);
  Address expectedObject(ICStubReg,
                         ICTypeMonitor_SingleObject::offsetOfObject());
  masm.branchPtr(Assembler::NotEqual, expectedObject, obj, &failure);
  MaybeWorkAroundAmdBug(masm);

  EmitReturnFromIC(masm);
  MaybeWorkAroundAmdBug(masm);

  masm.bind(&failure);
  EmitStubGuardFailure(masm);
  return true;
}

bool ICTypeMonitor_ObjectGroup::Compiler::generateStubCode(
    MacroAssembler& masm) {
  Label failure;
  masm.branchTestObject(Assembler::NotEqual, R0, &failure);
  MaybeWorkAroundAmdBug(masm);

  // Guard on the object's ObjectGroup. No Spectre mitigations are needed
  // here: we're just recording type information for Ion compilation and
  // it's safe to speculatively return.
  Register obj = masm.extractObject(R0, ExtractTemp0);
  Address expectedGroup(ICStubReg, ICTypeMonitor_ObjectGroup::offsetOfGroup());
  masm.branchTestObjGroupNoSpectreMitigations(
      Assembler::NotEqual, obj, expectedGroup, R1.scratchReg(), &failure);
  MaybeWorkAroundAmdBug(masm);

  EmitReturnFromIC(masm);
  MaybeWorkAroundAmdBug(masm);

  masm.bind(&failure);
  EmitStubGuardFailure(masm);
  return true;
}

bool ICTypeMonitor_AnyValue::Compiler::generateStubCode(MacroAssembler& masm) {
  EmitReturnFromIC(masm);
  return true;
}

bool ICCacheIR_Updated::addUpdateStubForValue(JSContext* cx,
                                              HandleScript outerScript,
                                              HandleObject obj,
                                              HandleObjectGroup group,
                                              HandleId id, HandleValue val) {
  MOZ_ASSERT(IsTypeInferenceEnabled());

  EnsureTrackPropertyTypes(cx, obj, id);

  // Make sure that undefined values are explicitly included in the property
  // types for an object if generating a stub to write an undefined value.
  if (val.isUndefined() && CanHaveEmptyPropertyTypesForOwnProperty(obj)) {
    MOZ_ASSERT(obj->group() == group);
    AddTypePropertyId(cx, obj, id, val);
  }

  bool unknown = false, unknownObject = false;
  AutoSweepObjectGroup sweep(group);
  if (group->unknownProperties(sweep)) {
    unknown = unknownObject = true;
  } else {
    if (HeapTypeSet* types = group->maybeGetProperty(sweep, id)) {
      unknown = types->unknown();
      unknownObject = types->unknownObject();
    } else {
      // We don't record null/undefined types for certain TypedObject
      // properties. In these cases |types| is allowed to be nullptr
      // without implying unknown types. See DoTypeUpdateFallback.
      MOZ_ASSERT(obj->is<TypedObject>());
      MOZ_ASSERT(val.isNullOrUndefined());
    }
  }
  MOZ_ASSERT_IF(unknown, unknownObject);

  // Don't attach too many SingleObject/ObjectGroup stubs unless we can
  // replace them with a single PrimitiveSet or AnyValue stub.
  if (numOptimizedStubs_ >= MAX_OPTIMIZED_STUBS && val.isObject() &&
      !unknownObject) {
    return true;
  }

  if (unknown) {
    // Attach a stub that always succeeds. We should not have a
    // TypeUpdate_AnyValue stub yet.
    MOZ_ASSERT(!hasTypeUpdateStub(TypeUpdate_AnyValue));

    // Discard existing stubs.
    resetUpdateStubChain(cx->zone());

    ICTypeUpdate_AnyValue::Compiler compiler(cx);
    ICStub* stub = compiler.getStub(compiler.getStubSpace(outerScript));
    if (!stub) {
      return false;
    }

    JitSpew(JitSpew_BaselineIC, "  Added TypeUpdate stub %p for any value",
            stub);
    addOptimizedUpdateStub(stub);

  } else if (val.isPrimitive() || unknownObject) {
    ValueType type = val.type();

    // Check for existing TypeUpdate stub.
    ICTypeUpdate_PrimitiveSet* existingStub = nullptr;
    for (ICStubConstIterator iter(firstUpdateStub_); !iter.atEnd(); iter++) {
      if (iter->isTypeUpdate_PrimitiveSet()) {
        existingStub = iter->toTypeUpdate_PrimitiveSet();
        MOZ_ASSERT(!existingStub->containsType(type));
      }
    }

    if (val.isObject()) {
      // Discard existing ObjectGroup/SingleObject stubs.
      resetUpdateStubChain(cx->zone());
      if (existingStub) {
        addOptimizedUpdateStub(existingStub);
      }
    }

    ICTypeUpdate_PrimitiveSet::Compiler compiler(cx, existingStub, type);
    ICStub* stub = existingStub
                       ? compiler.updateStub()
                       : compiler.getStub(compiler.getStubSpace(outerScript));
    if (!stub) {
      return false;
    }
    if (!existingStub) {
      MOZ_ASSERT(!hasTypeUpdateStub(TypeUpdate_PrimitiveSet));
      addOptimizedUpdateStub(stub);
    }

    JitSpew(JitSpew_BaselineIC, "  %s TypeUpdate stub %p for primitive type %d",
            existingStub ? "Modified existing" : "Created new", stub,
            static_cast<uint8_t>(type));

  } else if (val.toObject().isSingleton()) {
    RootedObject obj(cx, &val.toObject());

#ifdef DEBUG
    // We should not have a stub for this object.
    for (ICStubConstIterator iter(firstUpdateStub_); !iter.atEnd(); iter++) {
      MOZ_ASSERT_IF(iter->isTypeUpdate_SingleObject(),
                    iter->toTypeUpdate_SingleObject()->object() != obj);
    }
#endif

    ICTypeUpdate_SingleObject::Compiler compiler(cx, obj);
    ICStub* stub = compiler.getStub(compiler.getStubSpace(outerScript));
    if (!stub) {
      return false;
    }

    JitSpew(JitSpew_BaselineIC, "  Added TypeUpdate stub %p for singleton %p",
            stub, obj.get());

    addOptimizedUpdateStub(stub);

  } else {
    RootedObjectGroup group(cx, val.toObject().group());

#ifdef DEBUG
    // We should not have a stub for this group.
    for (ICStubConstIterator iter(firstUpdateStub_); !iter.atEnd(); iter++) {
      MOZ_ASSERT_IF(iter->isTypeUpdate_ObjectGroup(),
                    iter->toTypeUpdate_ObjectGroup()->group() != group);
    }
#endif

    ICTypeUpdate_ObjectGroup::Compiler compiler(cx, group);
    ICStub* stub = compiler.getStub(compiler.getStubSpace(outerScript));
    if (!stub) {
      return false;
    }

    JitSpew(JitSpew_BaselineIC, "  Added TypeUpdate stub %p for ObjectGroup %p",
            stub, group.get());

    addOptimizedUpdateStub(stub);
  }

  return true;
}

//
// TypeUpdate_Fallback
//
bool DoTypeUpdateFallback(JSContext* cx, BaselineFrame* frame,
                          ICCacheIR_Updated* stub, HandleValue objval,
                          HandleValue value) {
  // This can get called from optimized stubs. Therefore it is not allowed to
  // gc.
  JS::AutoCheckCannotGC nogc;

  MOZ_ASSERT(IsTypeInferenceEnabled());

  FallbackICSpew(cx, stub->getChainFallback(), "TypeUpdate(%s)",
                 ICStub::KindString(stub->kind()));

  MOZ_ASSERT(stub->isCacheIR_Updated());

  RootedScript script(cx, frame->script());
  RootedObject obj(cx, &objval.toObject());

  RootedId id(cx, stub->toCacheIR_Updated()->updateStubId());
  MOZ_ASSERT(id.get() != JSID_EMPTY);

  // The group should match the object's group.
  RootedObjectGroup group(cx, stub->toCacheIR_Updated()->updateStubGroup());
#ifdef DEBUG
  MOZ_ASSERT(obj->group() == group);
#endif

  // If we're storing null/undefined to a typed object property, check if
  // we want to include it in this property's type information.
  bool addType = true;
  if (MOZ_UNLIKELY(obj->is<TypedObject>()) && value.isNullOrUndefined()) {
    StructTypeDescr* structDescr =
        &obj->as<TypedObject>().typeDescr().as<StructTypeDescr>();
    size_t fieldIndex;
    MOZ_ALWAYS_TRUE(structDescr->fieldIndex(id, &fieldIndex));

    TypeDescr* fieldDescr = &structDescr->fieldDescr(fieldIndex);
    ReferenceType type = fieldDescr->as<ReferenceTypeDescr>().type();
    if (type == ReferenceType::TYPE_ANY) {
      // Ignore undefined values, which are included implicitly in type
      // information for this property.
      if (value.isUndefined()) {
        addType = false;
      }
    } else {
      MOZ_ASSERT(type == ReferenceType::TYPE_OBJECT ||
                 type == ReferenceType::TYPE_WASM_ANYREF);

      // Ignore null values being written here. Null is included
      // implicitly in type information for this property. Note that
      // non-object, non-null values are not possible here, these
      // should have been filtered out by the IR emitter.
      if (value.isNull()) {
        addType = false;
      }
    }
  }

  if (MOZ_LIKELY(addType)) {
    JSObject* maybeSingleton = obj->isSingleton() ? obj.get() : nullptr;
    AddTypePropertyId(cx, group, maybeSingleton, id, value);
  }

  if (MOZ_UNLIKELY(
          !stub->addUpdateStubForValue(cx, script, obj, group, id, value))) {
    // The calling JIT code assumes this function is infallible (for
    // instance we may reallocate dynamic slots before calling this),
    // so ignore OOMs if we failed to attach a stub.
    cx->recoverFromOutOfMemory();
  }

  return true;
}

bool FallbackICCodeCompiler::emit_TypeUpdate() {
  // Just store false into R1.scratchReg() and return.
  masm.move32(Imm32(0), R1.scratchReg());
  EmitReturnFromIC(masm);
  return true;
}

bool ICTypeUpdate_PrimitiveSet::Compiler::generateStubCode(
    MacroAssembler& masm) {
  Label success;
  if ((flags_ & TypeToFlag(ValueType::Int32)) &&
      !(flags_ & TypeToFlag(ValueType::Double))) {
    masm.branchTestInt32(Assembler::Equal, R0, &success);
  }

  if (flags_ & TypeToFlag(ValueType::Double)) {
    masm.branchTestNumber(Assembler::Equal, R0, &success);
  }

  if (flags_ & TypeToFlag(ValueType::Undefined)) {
    masm.branchTestUndefined(Assembler::Equal, R0, &success);
  }

  if (flags_ & TypeToFlag(ValueType::Boolean)) {
    masm.branchTestBoolean(Assembler::Equal, R0, &success);
  }

  if (flags_ & TypeToFlag(ValueType::String)) {
    masm.branchTestString(Assembler::Equal, R0, &success);
  }

  if (flags_ & TypeToFlag(ValueType::Symbol)) {
    masm.branchTestSymbol(Assembler::Equal, R0, &success);
  }

  if (flags_ & TypeToFlag(ValueType::BigInt)) {
    masm.branchTestBigInt(Assembler::Equal, R0, &success);
  }

  if (flags_ & TypeToFlag(ValueType::Object)) {
    masm.branchTestObject(Assembler::Equal, R0, &success);
  }

  if (flags_ & TypeToFlag(ValueType::Null)) {
    masm.branchTestNull(Assembler::Equal, R0, &success);
  }

  EmitStubGuardFailure(masm);

  // Type matches, load true into R1.scratchReg() and return.
  masm.bind(&success);
  masm.mov(ImmWord(1), R1.scratchReg());
  EmitReturnFromIC(masm);

  return true;
}

bool ICTypeUpdate_SingleObject::Compiler::generateStubCode(
    MacroAssembler& masm) {
  Label failure;
  masm.branchTestObject(Assembler::NotEqual, R0, &failure);

  // Guard on the object's identity.
  Register obj = masm.extractObject(R0, R1.scratchReg());
  Address expectedObject(ICStubReg,
                         ICTypeUpdate_SingleObject::offsetOfObject());
  masm.branchPtr(Assembler::NotEqual, expectedObject, obj, &failure);

  // Identity matches, load true into R1.scratchReg() and return.
  masm.mov(ImmWord(1), R1.scratchReg());
  EmitReturnFromIC(masm);

  masm.bind(&failure);
  EmitStubGuardFailure(masm);
  return true;
}

bool ICTypeUpdate_ObjectGroup::Compiler::generateStubCode(
    MacroAssembler& masm) {
  Label failure;
  masm.branchTestObject(Assembler::NotEqual, R0, &failure);

  // Guard on the object's ObjectGroup.
  Address expectedGroup(ICStubReg, ICTypeUpdate_ObjectGroup::offsetOfGroup());
  Register scratch1 = R1.scratchReg();
  masm.unboxObject(R0, scratch1);
  masm.branchTestObjGroup(Assembler::NotEqual, scratch1, expectedGroup,
                          scratch1, R0.payloadOrValueReg(), &failure);

  // Group matches, load true into R1.scratchReg() and return.
  masm.mov(ImmWord(1), R1.scratchReg());
  EmitReturnFromIC(masm);

  masm.bind(&failure);
  EmitStubGuardFailure(masm);
  return true;
}

bool ICTypeUpdate_AnyValue::Compiler::generateStubCode(MacroAssembler& masm) {
  // AnyValue always matches so return true.
  masm.mov(ImmWord(1), R1.scratchReg());
  EmitReturnFromIC(masm);
  return true;
}

//
// ToBool_Fallback
//

bool DoToBoolFallback(JSContext* cx, BaselineFrame* frame,
                      ICToBool_Fallback* stub, HandleValue arg,
                      MutableHandleValue ret) {
  stub->incrementEnteredCount();
  FallbackICSpew(cx, stub, "ToBool");

  MOZ_ASSERT(!arg.isBoolean());

  TryAttachStub<ToBoolIRGenerator>("ToBool", cx, frame, stub,
                                   BaselineCacheIRStubKind::Regular, arg);

  bool cond = ToBoolean(arg);
  ret.setBoolean(cond);

  return true;
}

bool FallbackICCodeCompiler::emit_ToBool() {
  static_assert(R0 == JSReturnOperand);

  // Restore the tail call register.
  EmitRestoreTailCallReg(masm);

  // Push arguments.
  masm.pushValue(R0);
  masm.push(ICStubReg);
  pushStubPayload(masm, R0.scratchReg());

  using Fn = bool (*)(JSContext*, BaselineFrame*, ICToBool_Fallback*,
                      HandleValue, MutableHandleValue);
  return tailCallVM<Fn, DoToBoolFallback>(masm);
}

static void StripPreliminaryObjectStubs(JSContext* cx, ICFallbackStub* stub) {
  // Before the new script properties analysis has been performed on a type,
  // all instances of that type have the maximum number of fixed slots.
  // Afterwards, the objects (even the preliminary ones) might be changed
  // to reduce the number of fixed slots they have. If we generate stubs for
  // both the old and new number of fixed slots, the stub will look
  // polymorphic to IonBuilder when it is actually monomorphic. To avoid
  // this, strip out any stubs for preliminary objects before attaching a new
  // stub which isn't on a preliminary object.

  for (ICStubIterator iter = stub->beginChain(); !iter.atEnd(); iter++) {
    if (iter->isCacheIR_Regular() &&
        iter->toCacheIR_Regular()->hasPreliminaryObject()) {
      iter.unlink(cx);
    } else if (iter->isCacheIR_Monitored() &&
               iter->toCacheIR_Monitored()->hasPreliminaryObject()) {
      iter.unlink(cx);
    } else if (iter->isCacheIR_Updated() &&
               iter->toCacheIR_Updated()->hasPreliminaryObject()) {
      iter.unlink(cx);
    }
  }
}

static bool TryAttachGetPropStub(const char* name, JSContext* cx,
                                 BaselineFrame* frame, ICFallbackStub* stub,
                                 CacheKind kind, HandleValue val,
                                 HandleValue idVal, HandleValue receiver) {
  bool attached = false;

  if (stub->state().maybeTransition()) {
    stub->discardStubs(cx);
  }

  if (stub->state().canAttachStub()) {
    RootedScript script(cx, frame->script());
    jsbytecode* pc = stub->icEntry()->pc(script);

    GetPropIRGenerator gen(cx, script, pc, stub->state().mode(), kind, val,
                           idVal, receiver, GetPropertyResultFlags::All);
    switch (gen.tryAttachStub()) {
      case AttachDecision::Attach: {
        ICStub* newStub = AttachBaselineCacheIRStub(
            cx, gen.writerRef(), gen.cacheKind(),
            BaselineCacheIRStubKind::Monitored, script, stub, &attached);
        if (newStub) {
          JitSpew(JitSpew_BaselineIC, "  Attached %s CacheIR stub", name);
          if (gen.shouldNotePreliminaryObjectStub()) {
            newStub->toCacheIR_Monitored()->notePreliminaryObject();
          } else if (gen.shouldUnlinkPreliminaryObjectStubs()) {
            StripPreliminaryObjectStubs(cx, stub);
          }
        }
      } break;
      case AttachDecision::NoAction:
        break;
      case AttachDecision::TemporarilyUnoptimizable:
        attached = true;
        break;
      case AttachDecision::Deferred:
        MOZ_ASSERT_UNREACHABLE("No deferred GetProp stubs");
        break;
    }
  }
  return attached;
}

//
// GetElem_Fallback
//

bool DoGetElemFallback(JSContext* cx, BaselineFrame* frame,
                       ICGetElem_Fallback* stub, HandleValue lhs,
                       HandleValue rhs, MutableHandleValue res) {
  stub->incrementEnteredCount();

  RootedScript script(cx, frame->script());
  jsbytecode* pc = stub->icEntry()->pc(frame->script());

  JSOp op = JSOp(*pc);
  FallbackICSpew(cx, stub, "GetElem(%s)", CodeName(op));

  MOZ_ASSERT(op == JSOp::GetElem || op == JSOp::CallElem);

  // Don't pass lhs directly, we need it when generating stubs.
  RootedValue lhsCopy(cx, lhs);

  bool isOptimizedArgs = false;
  if (lhs.isMagic(JS_OPTIMIZED_ARGUMENTS)) {
    // Handle optimized arguments[i] access.
    if (!GetElemOptimizedArguments(cx, frame, &lhsCopy, rhs, res,
                                   &isOptimizedArgs)) {
      return false;
    }
    if (isOptimizedArgs) {
      if (!TypeMonitorResult(cx, stub, frame, script, pc, res)) {
        return false;
      }
    }
  }

  bool attached = TryAttachGetPropStub("GetElem", cx, frame, stub,
                                       CacheKind::GetElem, lhs, rhs, lhs);

  if (!isOptimizedArgs) {
    if (!GetElementOperation(cx, op, lhsCopy, rhs, res)) {
      return false;
    }

    if (!TypeMonitorResult(cx, stub, frame, script, pc, res)) {
      return false;
    }
  }

  if (attached) {
    return true;
  }

  // GetElem operations which could access negative indexes generally can't
  // be optimized without the potential for bailouts, as we can't statically
  // determine that an object has no properties on such indexes.
  if (rhs.isNumber() && rhs.toNumber() < 0) {
    stub->noteNegativeIndex();
  }

  // GetElem operations which could access non-integer indexes generally can't
  // be optimized without the potential for bailouts.
  int32_t representable;
  if (rhs.isNumber() && rhs.isDouble() &&
      !mozilla::NumberEqualsInt32(rhs.toDouble(), &representable)) {
    stub->setSawNonIntegerIndex();
  }

  return true;
}

bool DoGetElemSuperFallback(JSContext* cx, BaselineFrame* frame,
                            ICGetElem_Fallback* stub, HandleValue lhs,
                            HandleValue rhs, HandleValue receiver,
                            MutableHandleValue res) {
  stub->incrementEnteredCount();

  RootedScript script(cx, frame->script());
  jsbytecode* pc = stub->icEntry()->pc(frame->script());

  JSOp op = JSOp(*pc);
  FallbackICSpew(cx, stub, "GetElemSuper(%s)", CodeName(op));

  MOZ_ASSERT(op == JSOp::GetElemSuper);

  bool attached =
      TryAttachGetPropStub("GetElemSuper", cx, frame, stub,
                           CacheKind::GetElemSuper, lhs, rhs, receiver);

  // |lhs| is [[HomeObject]].[[Prototype]] which must be Object
  RootedObject lhsObj(cx, &lhs.toObject());
  if (!GetObjectElementOperation(cx, op, lhsObj, receiver, rhs, res)) {
    return false;
  }

  if (!TypeMonitorResult(cx, stub, frame, script, pc, res)) {
    return false;
  }

  if (attached) {
    return true;
  }

  // GetElem operations which could access negative indexes generally can't
  // be optimized without the potential for bailouts, as we can't statically
  // determine that an object has no properties on such indexes.
  if (rhs.isNumber() && rhs.toNumber() < 0) {
    stub->noteNegativeIndex();
  }

  // GetElem operations which could access non-integer indexes generally can't
  // be optimized without the potential for bailouts.
  int32_t representable;
  if (rhs.isNumber() && rhs.isDouble() &&
      !mozilla::NumberEqualsInt32(rhs.toDouble(), &representable)) {
    stub->setSawNonIntegerIndex();
  }

  return true;
}

bool FallbackICCodeCompiler::emitGetElem(bool hasReceiver) {
  static_assert(R0 == JSReturnOperand);

  // Restore the tail call register.
  EmitRestoreTailCallReg(masm);

  // Super property getters use a |this| that differs from base object
  if (hasReceiver) {
    // State: receiver in R0, index in R1, obj on the stack

    // Ensure stack is fully synced for the expression decompiler.
    // We need: receiver, index, obj
    masm.pushValue(R0);
    masm.pushValue(R1);
    masm.pushValue(Address(masm.getStackPointer(), sizeof(Value) * 2));

    // Push arguments.
    masm.pushValue(R0);  // Receiver
    masm.pushValue(R1);  // Index
    masm.pushValue(Address(masm.getStackPointer(), sizeof(Value) * 5));  // Obj
    masm.push(ICStubReg);
    masm.pushBaselineFramePtr(BaselineFrameReg, R0.scratchReg());

    using Fn =
        bool (*)(JSContext*, BaselineFrame*, ICGetElem_Fallback*, HandleValue,
                 HandleValue, HandleValue, MutableHandleValue);
    if (!tailCallVM<Fn, DoGetElemSuperFallback>(masm)) {
      return false;
    }
  } else {
    // Ensure stack is fully synced for the expression decompiler.
    masm.pushValue(R0);
    masm.pushValue(R1);

    // Push arguments.
    masm.pushValue(R1);
    masm.pushValue(R0);
    masm.push(ICStubReg);
    masm.pushBaselineFramePtr(BaselineFrameReg, R0.scratchReg());

    using Fn = bool (*)(JSContext*, BaselineFrame*, ICGetElem_Fallback*,
                        HandleValue, HandleValue, MutableHandleValue);
    if (!tailCallVM<Fn, DoGetElemFallback>(masm)) {
      return false;
    }
  }

  // This is the resume point used when bailout rewrites call stack to undo
  // Ion inlined frames. The return address pushed onto reconstructed stack
  // will point here.
  assumeStubFrame();
  if (hasReceiver) {
    code.initBailoutReturnOffset(BailoutReturnKind::GetElemSuper,
                                 masm.currentOffset());
  } else {
    code.initBailoutReturnOffset(BailoutReturnKind::GetElem,
                                 masm.currentOffset());
  }

  leaveStubFrame(masm, true);

  // When we get here, ICStubReg contains the ICGetElem_Fallback stub,
  // which we can't use to enter the TypeMonitor IC, because it's a
  // MonitoredFallbackStub instead of a MonitoredStub. So, we cheat. Note that
  // we must have a non-null fallbackMonitorStub here because InitFromBailout
  // delazifies.
  masm.loadPtr(Address(ICStubReg,
                       ICMonitoredFallbackStub::offsetOfFallbackMonitorStub()),
               ICStubReg);
  EmitEnterTypeMonitorIC(masm,
                         ICTypeMonitor_Fallback::offsetOfFirstMonitorStub());

  return true;
}

bool FallbackICCodeCompiler::emit_GetElem() {
  return emitGetElem(/* hasReceiver = */ false);
}

bool FallbackICCodeCompiler::emit_GetElemSuper() {
  return emitGetElem(/* hasReceiver = */ true);
}

static void SetUpdateStubData(ICCacheIR_Updated* stub,
                              const PropertyTypeCheckInfo* info) {
  if (info->isSet()) {
    stub->updateStubGroup() = info->group();
    stub->updateStubId() = info->id();
  }
}

bool DoSetElemFallback(JSContext* cx, BaselineFrame* frame,
                       ICSetElem_Fallback* stub, Value* stack, HandleValue objv,
                       HandleValue index, HandleValue rhs) {
  using DeferType = SetPropIRGenerator::DeferType;

  stub->incrementEnteredCount();

  RootedScript script(cx, frame->script());
  RootedScript outerScript(cx, script);
  jsbytecode* pc = stub->icEntry()->pc(script);
  JSOp op = JSOp(*pc);
  FallbackICSpew(cx, stub, "SetElem(%s)", CodeName(JSOp(*pc)));

  MOZ_ASSERT(op == JSOp::SetElem || op == JSOp::StrictSetElem ||
             op == JSOp::InitElem || op == JSOp::InitHiddenElem ||
             op == JSOp::InitElemArray || op == JSOp::InitElemInc);

  int objvIndex = -3;
  RootedObject obj(
      cx, ToObjectFromStackForPropertyAccess(cx, objv, objvIndex, index));
  if (!obj) {
    return false;
  }

  RootedShape oldShape(cx, obj->shape());
  RootedObjectGroup oldGroup(cx, JSObject::getGroup(cx, obj));
  if (!oldGroup) {
    return false;
  }

  DeferType deferType = DeferType::None;
  bool attached = false;

  if (stub->state().maybeTransition()) {
    stub->discardStubs(cx);
  }

  if (stub->state().canAttachStub()) {
    SetPropIRGenerator gen(cx, script, pc, CacheKind::SetElem,
                           stub->state().mode(), objv, index, rhs);
    switch (gen.tryAttachStub()) {
      case AttachDecision::Attach: {
        ICStub* newStub = AttachBaselineCacheIRStub(
            cx, gen.writerRef(), gen.cacheKind(),
            BaselineCacheIRStubKind::Updated, frame->script(), stub, &attached);
        if (newStub) {
          JitSpew(JitSpew_BaselineIC, "  Attached SetElem CacheIR stub");

          SetUpdateStubData(newStub->toCacheIR_Updated(), gen.typeCheckInfo());

          if (gen.shouldNotePreliminaryObjectStub()) {
            newStub->toCacheIR_Updated()->notePreliminaryObject();
          } else if (gen.shouldUnlinkPreliminaryObjectStubs()) {
            StripPreliminaryObjectStubs(cx, stub);
          }

          if (gen.attachedTypedArrayOOBStub()) {
            stub->noteHasTypedArrayOOB();
          }
        }
      } break;
      case AttachDecision::NoAction:
        break;
      case AttachDecision::TemporarilyUnoptimizable:
        attached = true;
        break;
      case AttachDecision::Deferred:
        deferType = gen.deferType();
        MOZ_ASSERT(deferType != DeferType::None);
        break;
    }
  }

  if (op == JSOp::InitElem || op == JSOp::InitHiddenElem) {
    if (!InitElemOperation(cx, pc, obj, index, rhs)) {
      return false;
    }
  } else if (op == JSOp::InitElemArray) {
    MOZ_ASSERT(uint32_t(index.toInt32()) <= INT32_MAX,
               "the bytecode emitter must fail to compile code that would "
               "produce JSOp::InitElemArray with an index exceeding "
               "int32_t range");
    MOZ_ASSERT(uint32_t(index.toInt32()) == GET_UINT32(pc));
    if (!InitArrayElemOperation(cx, pc, obj, index.toInt32(), rhs)) {
      return false;
    }
  } else if (op == JSOp::InitElemInc) {
    if (!InitArrayElemOperation(cx, pc, obj, index.toInt32(), rhs)) {
      return false;
    }
  } else {
    if (!SetObjectElement(cx, obj, index, rhs, objv,
                          JSOp(*pc) == JSOp::StrictSetElem, script, pc)) {
      return false;
    }
  }

  // Don't try to attach stubs that wish to be hidden. We don't know how to
  // have different enumerability in the stubs for the moment.
  if (op == JSOp::InitHiddenElem) {
    return true;
  }

  // Overwrite the object on the stack (pushed for the decompiler) with the rhs.
  MOZ_ASSERT(stack[2] == objv);
  stack[2] = rhs;

  if (attached) {
    return true;
  }

  // The SetObjectElement call might have entered this IC recursively, so try
  // to transition.
  if (stub->state().maybeTransition()) {
    stub->discardStubs(cx);
  }

  bool canAttachStub = stub->state().canAttachStub();

  if (deferType != DeferType::None && canAttachStub) {
    SetPropIRGenerator gen(cx, script, pc, CacheKind::SetElem,
                           stub->state().mode(), objv, index, rhs);

    MOZ_ASSERT(deferType == DeferType::AddSlot);
    AttachDecision decision = gen.tryAttachAddSlotStub(oldGroup, oldShape);

    switch (decision) {
      case AttachDecision::Attach: {
        ICStub* newStub = AttachBaselineCacheIRStub(
            cx, gen.writerRef(), gen.cacheKind(),
            BaselineCacheIRStubKind::Updated, frame->script(), stub, &attached);
        if (newStub) {
          JitSpew(JitSpew_BaselineIC, "  Attached SetElem CacheIR stub");

          SetUpdateStubData(newStub->toCacheIR_Updated(), gen.typeCheckInfo());

          if (gen.shouldNotePreliminaryObjectStub()) {
            newStub->toCacheIR_Updated()->notePreliminaryObject();
          } else if (gen.shouldUnlinkPreliminaryObjectStubs()) {
            StripPreliminaryObjectStubs(cx, stub);
          }
        }
      } break;
      case AttachDecision::NoAction:
        gen.trackAttached(IRGenerator::NotAttached);
        break;
      case AttachDecision::TemporarilyUnoptimizable:
      case AttachDecision::Deferred:
        MOZ_ASSERT_UNREACHABLE("Invalid attach result");
        break;
    }
  }
  if (!attached && canAttachStub) {
    stub->state().trackNotAttached();
  }
  return true;
}

bool FallbackICCodeCompiler::emit_SetElem() {
  static_assert(R0 == JSReturnOperand);

  EmitRestoreTailCallReg(masm);

  // State: R0: object, R1: index, stack: rhs.
  // For the decompiler, the stack has to be: object, index, rhs,
  // so we push the index, then overwrite the rhs Value with R0
  // and push the rhs value.
  masm.pushValue(R1);
  masm.loadValue(Address(masm.getStackPointer(), sizeof(Value)), R1);
  masm.storeValue(R0, Address(masm.getStackPointer(), sizeof(Value)));
  masm.pushValue(R1);

  // Push arguments.
  masm.pushValue(R1);  // RHS

  // Push index. On x86 and ARM two push instructions are emitted so use a
  // separate register to store the old stack pointer.
  masm.moveStackPtrTo(R1.scratchReg());
  masm.pushValue(Address(R1.scratchReg(), 2 * sizeof(Value)));
  masm.pushValue(R0);  // Object.

  // Push pointer to stack values, so that the stub can overwrite the object
  // (pushed for the decompiler) with the rhs.
  masm.computeEffectiveAddress(
      Address(masm.getStackPointer(), 3 * sizeof(Value)), R0.scratchReg());
  masm.push(R0.scratchReg());

  masm.push(ICStubReg);
  pushStubPayload(masm, R0.scratchReg());

  using Fn = bool (*)(JSContext*, BaselineFrame*, ICSetElem_Fallback*, Value*,
                      HandleValue, HandleValue, HandleValue);
  return tailCallVM<Fn, DoSetElemFallback>(masm);
}

//
// In_Fallback
//

bool DoInFallback(JSContext* cx, BaselineFrame* frame, ICIn_Fallback* stub,
                  HandleValue key, HandleValue objValue,
                  MutableHandleValue res) {
  stub->incrementEnteredCount();

  FallbackICSpew(cx, stub, "In");

  if (!objValue.isObject()) {
    ReportInNotObjectError(cx, key, -2, objValue, -1);
    return false;
  }

  TryAttachStub<HasPropIRGenerator>("In", cx, frame, stub,
                                    BaselineCacheIRStubKind::Regular,
                                    CacheKind::In, key, objValue);

  RootedObject obj(cx, &objValue.toObject());
  bool cond = false;
  if (!OperatorIn(cx, key, obj, &cond)) {
    return false;
  }
  res.setBoolean(cond);

  return true;
}

bool FallbackICCodeCompiler::emit_In() {
  EmitRestoreTailCallReg(masm);

  // Sync for the decompiler.
  masm.pushValue(R0);
  masm.pushValue(R1);

  // Push arguments.
  masm.pushValue(R1);
  masm.pushValue(R0);
  masm.push(ICStubReg);
  pushStubPayload(masm, R0.scratchReg());

  using Fn = bool (*)(JSContext*, BaselineFrame*, ICIn_Fallback*, HandleValue,
                      HandleValue, MutableHandleValue);
  return tailCallVM<Fn, DoInFallback>(masm);
}

//
// HasOwn_Fallback
//

bool DoHasOwnFallback(JSContext* cx, BaselineFrame* frame,
                      ICHasOwn_Fallback* stub, HandleValue keyValue,
                      HandleValue objValue, MutableHandleValue res) {
  stub->incrementEnteredCount();

  FallbackICSpew(cx, stub, "HasOwn");

  TryAttachStub<HasPropIRGenerator>("HasOwn", cx, frame, stub,
                                    BaselineCacheIRStubKind::Regular,
                                    CacheKind::HasOwn, keyValue, objValue);

  bool found;
  if (!HasOwnProperty(cx, objValue, keyValue, &found)) {
    return false;
  }

  res.setBoolean(found);
  return true;
}

bool FallbackICCodeCompiler::emit_HasOwn() {
  EmitRestoreTailCallReg(masm);

  // Sync for the decompiler.
  masm.pushValue(R0);
  masm.pushValue(R1);

  // Push arguments.
  masm.pushValue(R1);
  masm.pushValue(R0);
  masm.push(ICStubReg);
  pushStubPayload(masm, R0.scratchReg());

  using Fn = bool (*)(JSContext*, BaselineFrame*, ICHasOwn_Fallback*,
                      HandleValue, HandleValue, MutableHandleValue);
  return tailCallVM<Fn, DoHasOwnFallback>(masm);
}

//
// GetName_Fallback
//

bool DoGetNameFallback(JSContext* cx, BaselineFrame* frame,
                       ICGetName_Fallback* stub, HandleObject envChain,
                       MutableHandleValue res) {
  stub->incrementEnteredCount();

  RootedScript script(cx, frame->script());
  jsbytecode* pc = stub->icEntry()->pc(script);
  mozilla::DebugOnly<JSOp> op = JSOp(*pc);
  FallbackICSpew(cx, stub, "GetName(%s)", CodeName(JSOp(*pc)));

  MOZ_ASSERT(op == JSOp::GetName || op == JSOp::GetGName);

  RootedPropertyName name(cx, script->getName(pc));

  TryAttachStub<GetNameIRGenerator>("GetName", cx, frame, stub,
                                    BaselineCacheIRStubKind::Monitored,
                                    envChain, name);

  static_assert(JSOpLength_GetGName == JSOpLength_GetName,
                "Otherwise our check for JSOp::Typeof isn't ok");
  if (JSOp(pc[JSOpLength_GetGName]) == JSOp::Typeof) {
    if (!GetEnvironmentName<GetNameMode::TypeOf>(cx, envChain, name, res)) {
      return false;
    }
  } else {
    if (!GetEnvironmentName<GetNameMode::Normal>(cx, envChain, name, res)) {
      return false;
    }
  }

  return TypeMonitorResult(cx, stub, frame, script, pc, res);
}

bool FallbackICCodeCompiler::emit_GetName() {
  static_assert(R0 == JSReturnOperand);

  EmitRestoreTailCallReg(masm);

  masm.push(R0.scratchReg());
  masm.push(ICStubReg);
  pushStubPayload(masm, R0.scratchReg());

  using Fn = bool (*)(JSContext*, BaselineFrame*, ICGetName_Fallback*,
                      HandleObject, MutableHandleValue);
  return tailCallVM<Fn, DoGetNameFallback>(masm);
}

//
// BindName_Fallback
//

bool DoBindNameFallback(JSContext* cx, BaselineFrame* frame,
                        ICBindName_Fallback* stub, HandleObject envChain,
                        MutableHandleValue res) {
  stub->incrementEnteredCount();

  jsbytecode* pc = stub->icEntry()->pc(frame->script());
  mozilla::DebugOnly<JSOp> op = JSOp(*pc);
  FallbackICSpew(cx, stub, "BindName(%s)", CodeName(JSOp(*pc)));

  MOZ_ASSERT(op == JSOp::BindName || op == JSOp::BindGName);

  RootedPropertyName name(cx, frame->script()->getName(pc));

  TryAttachStub<BindNameIRGenerator>("BindName", cx, frame, stub,
                                     BaselineCacheIRStubKind::Regular, envChain,
                                     name);

  RootedObject scope(cx);
  if (!LookupNameUnqualified(cx, name, envChain, &scope)) {
    return false;
  }

  res.setObject(*scope);
  return true;
}

bool FallbackICCodeCompiler::emit_BindName() {
  static_assert(R0 == JSReturnOperand);

  EmitRestoreTailCallReg(masm);

  masm.push(R0.scratchReg());
  masm.push(ICStubReg);
  pushStubPayload(masm, R0.scratchReg());

  using Fn = bool (*)(JSContext*, BaselineFrame*, ICBindName_Fallback*,
                      HandleObject, MutableHandleValue);
  return tailCallVM<Fn, DoBindNameFallback>(masm);
}

//
// GetIntrinsic_Fallback
//

bool DoGetIntrinsicFallback(JSContext* cx, BaselineFrame* frame,
                            ICGetIntrinsic_Fallback* stub,
                            MutableHandleValue res) {
  stub->incrementEnteredCount();

  RootedScript script(cx, frame->script());
  jsbytecode* pc = stub->icEntry()->pc(script);
  mozilla::DebugOnly<JSOp> op = JSOp(*pc);
  FallbackICSpew(cx, stub, "GetIntrinsic(%s)", CodeName(JSOp(*pc)));

  MOZ_ASSERT(op == JSOp::GetIntrinsic);

  if (!GetIntrinsicOperation(cx, script, pc, res)) {
    return false;
  }

  // An intrinsic operation will always produce the same result, so only
  // needs to be monitored once. Attach a stub to load the resulting constant
  // directly.

  JitScript::MonitorBytecodeType(cx, script, pc, res);

  TryAttachStub<GetIntrinsicIRGenerator>("GetIntrinsic", cx, frame, stub,
                                         BaselineCacheIRStubKind::Regular, res);

  return true;
}

bool FallbackICCodeCompiler::emit_GetIntrinsic() {
  EmitRestoreTailCallReg(masm);

  masm.push(ICStubReg);
  pushStubPayload(masm, R0.scratchReg());

  using Fn = bool (*)(JSContext*, BaselineFrame*, ICGetIntrinsic_Fallback*,
                      MutableHandleValue);
  return tailCallVM<Fn, DoGetIntrinsicFallback>(masm);
}

//
// GetProp_Fallback
//

static bool ComputeGetPropResult(JSContext* cx, BaselineFrame* frame, JSOp op,
                                 HandlePropertyName name,
                                 MutableHandleValue val,
                                 MutableHandleValue res) {
  // Handle arguments.length and arguments.callee on optimized arguments, as
  // it is not an object.
  if (val.isMagic(JS_OPTIMIZED_ARGUMENTS) && IsOptimizedArguments(frame, val)) {
    if (op == JSOp::Length) {
      res.setInt32(frame->numActualArgs());
    } else {
      MOZ_ASSERT(name == cx->names().callee);
      MOZ_ASSERT(frame->script()->hasMappedArgsObj());
      res.setObject(*frame->callee());
    }
  } else {
    if (op == JSOp::GetBoundName) {
      RootedObject env(cx, &val.toObject());
      RootedId id(cx, NameToId(name));
      if (!GetNameBoundInEnvironment(cx, env, id, res)) {
        return false;
      }
    } else {
      MOZ_ASSERT(op == JSOp::GetProp || op == JSOp::CallProp ||
                 op == JSOp::Length);
      if (!GetProperty(cx, val, name, res)) {
        return false;
      }
    }
  }

  return true;
}

bool DoGetPropFallback(JSContext* cx, BaselineFrame* frame,
                       ICGetProp_Fallback* stub, MutableHandleValue val,
                       MutableHandleValue res) {
  stub->incrementEnteredCount();

  RootedScript script(cx, frame->script());
  jsbytecode* pc = stub->icEntry()->pc(script);
  JSOp op = JSOp(*pc);
  FallbackICSpew(cx, stub, "GetProp(%s)", CodeName(op));

  MOZ_ASSERT(op == JSOp::GetProp || op == JSOp::CallProp ||
             op == JSOp::Length || op == JSOp::GetBoundName);

  RootedPropertyName name(cx, script->getName(pc));
  RootedValue idVal(cx, StringValue(name));

  TryAttachGetPropStub("GetProp", cx, frame, stub, CacheKind::GetProp, val,
                       idVal, val);

  if (!ComputeGetPropResult(cx, frame, op, name, val, res)) {
    return false;
  }

  return TypeMonitorResult(cx, stub, frame, script, pc, res);
}

bool DoGetPropSuperFallback(JSContext* cx, BaselineFrame* frame,
                            ICGetProp_Fallback* stub, HandleValue receiver,
                            MutableHandleValue val, MutableHandleValue res) {
  stub->incrementEnteredCount();

  RootedScript script(cx, frame->script());
  jsbytecode* pc = stub->icEntry()->pc(script);
  FallbackICSpew(cx, stub, "GetPropSuper(%s)", CodeName(JSOp(*pc)));

  MOZ_ASSERT(JSOp(*pc) == JSOp::GetPropSuper);

  RootedPropertyName name(cx, script->getName(pc));
  RootedValue idVal(cx, StringValue(name));

  TryAttachGetPropStub("GetPropSuper", cx, frame, stub, CacheKind::GetPropSuper,
                       val, idVal, receiver);

  // |val| is [[HomeObject]].[[Prototype]] which must be Object
  RootedObject valObj(cx, &val.toObject());
  if (!GetProperty(cx, valObj, receiver, name, res)) {
    return false;
  }

  return TypeMonitorResult(cx, stub, frame, script, pc, res);
}

bool FallbackICCodeCompiler::emitGetProp(bool hasReceiver) {
  static_assert(R0 == JSReturnOperand);

  EmitRestoreTailCallReg(masm);

  // Super property getters use a |this| that differs from base object
  if (hasReceiver) {
    // Push arguments.
    masm.pushValue(R0);
    masm.pushValue(R1);
    masm.push(ICStubReg);
    masm.pushBaselineFramePtr(BaselineFrameReg, R0.scratchReg());

    using Fn = bool (*)(JSContext*, BaselineFrame*, ICGetProp_Fallback*,
                        HandleValue, MutableHandleValue, MutableHandleValue);
    if (!tailCallVM<Fn, DoGetPropSuperFallback>(masm)) {
      return false;
    }
  } else {
    // Ensure stack is fully synced for the expression decompiler.
    masm.pushValue(R0);

    // Push arguments.
    masm.pushValue(R0);
    masm.push(ICStubReg);
    masm.pushBaselineFramePtr(BaselineFrameReg, R0.scratchReg());

    using Fn = bool (*)(JSContext*, BaselineFrame*, ICGetProp_Fallback*,
                        MutableHandleValue, MutableHandleValue);
    if (!tailCallVM<Fn, DoGetPropFallback>(masm)) {
      return false;
    }
  }

  // This is the resume point used when bailout rewrites call stack to undo
  // Ion inlined frames. The return address pushed onto reconstructed stack
  // will point here.
  assumeStubFrame();
  if (hasReceiver) {
    code.initBailoutReturnOffset(BailoutReturnKind::GetPropSuper,
                                 masm.currentOffset());
  } else {
    code.initBailoutReturnOffset(BailoutReturnKind::GetProp,
                                 masm.currentOffset());
  }

  leaveStubFrame(masm, true);

  // When we get here, ICStubReg contains the ICGetProp_Fallback stub,
  // which we can't use to enter the TypeMonitor IC, because it's a
  // MonitoredFallbackStub instead of a MonitoredStub. So, we cheat. Note that
  // we must have a non-null fallbackMonitorStub here because InitFromBailout
  // delazifies.
  masm.loadPtr(Address(ICStubReg,
                       ICMonitoredFallbackStub::offsetOfFallbackMonitorStub()),
               ICStubReg);
  EmitEnterTypeMonitorIC(masm,
                         ICTypeMonitor_Fallback::offsetOfFirstMonitorStub());

  return true;
}

bool FallbackICCodeCompiler::emit_GetProp() {
  return emitGetProp(/* hasReceiver = */ false);
}

bool FallbackICCodeCompiler::emit_GetPropSuper() {
  return emitGetProp(/* hasReceiver = */ true);
}

//
// SetProp_Fallback
//

bool DoSetPropFallback(JSContext* cx, BaselineFrame* frame,
                       ICSetProp_Fallback* stub, Value* stack, HandleValue lhs,
                       HandleValue rhs) {
  using DeferType = SetPropIRGenerator::DeferType;

  stub->incrementEnteredCount();

  RootedScript script(cx, frame->script());
  jsbytecode* pc = stub->icEntry()->pc(script);
  JSOp op = JSOp(*pc);
  FallbackICSpew(cx, stub, "SetProp(%s)", CodeName(op));

  MOZ_ASSERT(op == JSOp::SetProp || op == JSOp::StrictSetProp ||
             op == JSOp::SetName || op == JSOp::StrictSetName ||
             op == JSOp::SetGName || op == JSOp::StrictSetGName ||
             op == JSOp::InitProp || op == JSOp::InitLockedProp ||
             op == JSOp::InitHiddenProp || op == JSOp::InitGLexical);

  RootedPropertyName name(cx, script->getName(pc));
  RootedId id(cx, NameToId(name));

  int lhsIndex = -2;
  RootedObject obj(cx,
                   ToObjectFromStackForPropertyAccess(cx, lhs, lhsIndex, id));
  if (!obj) {
    return false;
  }
  RootedShape oldShape(cx, obj->shape());
  RootedObjectGroup oldGroup(cx, JSObject::getGroup(cx, obj));
  if (!oldGroup) {
    return false;
  }

  DeferType deferType = DeferType::None;
  bool attached = false;
  if (stub->state().maybeTransition()) {
    stub->discardStubs(cx);
  }

  if (stub->state().canAttachStub()) {
    RootedValue idVal(cx, StringValue(name));
    SetPropIRGenerator gen(cx, script, pc, CacheKind::SetProp,
                           stub->state().mode(), lhs, idVal, rhs);
    switch (gen.tryAttachStub()) {
      case AttachDecision::Attach: {
        ICStub* newStub = AttachBaselineCacheIRStub(
            cx, gen.writerRef(), gen.cacheKind(),
            BaselineCacheIRStubKind::Updated, frame->script(), stub, &attached);
        if (newStub) {
          JitSpew(JitSpew_BaselineIC, "  Attached SetProp CacheIR stub");

          SetUpdateStubData(newStub->toCacheIR_Updated(), gen.typeCheckInfo());

          if (gen.shouldNotePreliminaryObjectStub()) {
            newStub->toCacheIR_Updated()->notePreliminaryObject();
          } else if (gen.shouldUnlinkPreliminaryObjectStubs()) {
            StripPreliminaryObjectStubs(cx, stub);
          }
        }
      } break;
      case AttachDecision::NoAction:
        break;
      case AttachDecision::TemporarilyUnoptimizable:
        attached = true;
        break;
      case AttachDecision::Deferred:
        deferType = gen.deferType();
        MOZ_ASSERT(deferType != DeferType::None);
        break;
    }
  }

  if (op == JSOp::InitProp || op == JSOp::InitLockedProp ||
      op == JSOp::InitHiddenProp) {
    if (!InitPropertyOperation(cx, op, obj, name, rhs)) {
      return false;
    }
  } else if (op == JSOp::SetName || op == JSOp::StrictSetName ||
             op == JSOp::SetGName || op == JSOp::StrictSetGName) {
    if (!SetNameOperation(cx, script, pc, obj, rhs)) {
      return false;
    }
  } else if (op == JSOp::InitGLexical) {
    RootedValue v(cx, rhs);
    LexicalEnvironmentObject* lexicalEnv;
    if (script->hasNonSyntacticScope()) {
      lexicalEnv = &NearestEnclosingExtensibleLexicalEnvironment(
          frame->environmentChain());
    } else {
      lexicalEnv = &cx->global()->lexicalEnvironment();
    }
    InitGlobalLexicalOperation(cx, lexicalEnv, script, pc, v);
  } else {
    MOZ_ASSERT(op == JSOp::SetProp || op == JSOp::StrictSetProp);

    ObjectOpResult result;
    if (!SetProperty(cx, obj, id, rhs, lhs, result) ||
        !result.checkStrictModeError(cx, obj, id, op == JSOp::StrictSetProp)) {
      return false;
    }
  }

  // Overwrite the LHS on the stack (pushed for the decompiler) with the RHS.
  MOZ_ASSERT(stack[1] == lhs);
  stack[1] = rhs;

  if (attached) {
    return true;
  }

  // The SetProperty call might have entered this IC recursively, so try
  // to transition.
  if (stub->state().maybeTransition()) {
    stub->discardStubs(cx);
  }

  bool canAttachStub = stub->state().canAttachStub();

  if (deferType != DeferType::None && canAttachStub) {
    RootedValue idVal(cx, StringValue(name));
    SetPropIRGenerator gen(cx, script, pc, CacheKind::SetProp,
                           stub->state().mode(), lhs, idVal, rhs);

    MOZ_ASSERT(deferType == DeferType::AddSlot);
    AttachDecision decision = gen.tryAttachAddSlotStub(oldGroup, oldShape);

    switch (decision) {
      case AttachDecision::Attach: {
        ICStub* newStub = AttachBaselineCacheIRStub(
            cx, gen.writerRef(), gen.cacheKind(),
            BaselineCacheIRStubKind::Updated, frame->script(), stub, &attached);
        if (newStub) {
          JitSpew(JitSpew_BaselineIC, "  Attached SetElem CacheIR stub");

          SetUpdateStubData(newStub->toCacheIR_Updated(), gen.typeCheckInfo());

          if (gen.shouldNotePreliminaryObjectStub()) {
            newStub->toCacheIR_Updated()->notePreliminaryObject();
          } else if (gen.shouldUnlinkPreliminaryObjectStubs()) {
            StripPreliminaryObjectStubs(cx, stub);
          }
        }
      } break;
      case AttachDecision::NoAction:
        gen.trackAttached(IRGenerator::NotAttached);
        break;
      case AttachDecision::TemporarilyUnoptimizable:
      case AttachDecision::Deferred:
        MOZ_ASSERT_UNREACHABLE("Invalid attach result");
        break;
    }
  }
  if (!attached && canAttachStub) {
    stub->state().trackNotAttached();
  }

  return true;
}

bool FallbackICCodeCompiler::emit_SetProp() {
  static_assert(R0 == JSReturnOperand);

  EmitRestoreTailCallReg(masm);

  // Ensure stack is fully synced for the expression decompiler.
  // Overwrite the RHS value on top of the stack with the object, then push
  // the RHS in R1 on top of that.
  masm.storeValue(R0, Address(masm.getStackPointer(), 0));
  masm.pushValue(R1);

  // Push arguments.
  masm.pushValue(R1);
  masm.pushValue(R0);

  // Push pointer to stack values, so that the stub can overwrite the object
  // (pushed for the decompiler) with the RHS.
  masm.computeEffectiveAddress(
      Address(masm.getStackPointer(), 2 * sizeof(Value)), R0.scratchReg());
  masm.push(R0.scratchReg());

  masm.push(ICStubReg);
  pushStubPayload(masm, R0.scratchReg());

  using Fn = bool (*)(JSContext*, BaselineFrame*, ICSetProp_Fallback*, Value*,
                      HandleValue, HandleValue);
  if (!tailCallVM<Fn, DoSetPropFallback>(masm)) {
    return false;
  }

  // This is the resume point used when bailout rewrites call stack to undo
  // Ion inlined frames. The return address pushed onto reconstructed stack
  // will point here.
  assumeStubFrame();
  code.initBailoutReturnOffset(BailoutReturnKind::SetProp,
                               masm.currentOffset());

  leaveStubFrame(masm, true);
  EmitReturnFromIC(masm);

  return true;
}

//
// Call_Fallback
//

bool DoCallFallback(JSContext* cx, BaselineFrame* frame, ICCall_Fallback* stub,
                    uint32_t argc, Value* vp, MutableHandleValue res) {
  stub->incrementEnteredCount();

  RootedScript script(cx, frame->script());
  jsbytecode* pc = stub->icEntry()->pc(script);
  JSOp op = JSOp(*pc);
  FallbackICSpew(cx, stub, "Call(%s)", CodeName(op));

  MOZ_ASSERT(argc == GET_ARGC(pc));
  bool constructing = (op == JSOp::New || op == JSOp::SuperCall);
  bool ignoresReturnValue = (op == JSOp::CallIgnoresRv);

  // Ensure vp array is rooted - we may GC in here.
  size_t numValues = argc + 2 + constructing;
  AutoArrayRooter vpRoot(cx, numValues, vp);

  CallArgs callArgs = CallArgsFromSp(argc + constructing, vp + numValues,
                                     constructing, ignoresReturnValue);
  RootedValue callee(cx, vp[0]);
  RootedValue newTarget(cx, constructing ? callArgs.newTarget() : NullValue());

  // Handle funapply with JSOp::Arguments
  if (op == JSOp::FunApply && argc == 2 &&
      callArgs[1].isMagic(JS_OPTIMIZED_ARGUMENTS)) {
    GuardFunApplyArgumentsOptimization(cx, frame, callArgs);
  }

  // Transition stub state to megamorphic or generic if warranted.
  if (stub->state().maybeTransition()) {
    stub->discardStubs(cx);
  }

  bool canAttachStub = stub->state().canAttachStub();
  bool handled = false;
  bool deferred = false;

  // Only bother to try optimizing JSOp::Call with CacheIR if the chain is still
  // allowed to attach stubs.
  if (canAttachStub) {
    HandleValueArray args = HandleValueArray::fromMarkedLocation(argc, vp + 2);
    CallIRGenerator gen(cx, script, pc, op, stub->state().mode(), argc, callee,
                        callArgs.thisv(), newTarget, args);
    switch (gen.tryAttachStub()) {
      case AttachDecision::NoAction:
        break;
      case AttachDecision::Attach: {
        ICStub* newStub = AttachBaselineCacheIRStub(
            cx, gen.writerRef(), gen.cacheKind(), gen.cacheIRStubKind(), script,
            stub, &handled);
        if (newStub) {
          JitSpew(JitSpew_BaselineIC, "  Attached Call CacheIR stub");

          // If it's an updated stub, initialize it.
          if (gen.cacheIRStubKind() == BaselineCacheIRStubKind::Updated) {
            SetUpdateStubData(newStub->toCacheIR_Updated(),
                              gen.typeCheckInfo());
          }
        }
      } break;
      case AttachDecision::TemporarilyUnoptimizable:
        handled = true;
        break;
      case AttachDecision::Deferred:
        deferred = true;
    }
  }

  if (constructing) {
    if (!ConstructFromStack(cx, callArgs)) {
      return false;
    }
    res.set(callArgs.rval());
  } else if ((op == JSOp::Eval || op == JSOp::StrictEval) &&
             cx->global()->valueIsEval(callee)) {
    if (!DirectEval(cx, callArgs.get(0), res)) {
      return false;
    }
  } else {
    MOZ_ASSERT(op == JSOp::Call || op == JSOp::CallIgnoresRv ||
               op == JSOp::CallIter || op == JSOp::FunCall ||
               op == JSOp::FunApply || op == JSOp::Eval ||
               op == JSOp::StrictEval);
    if (op == JSOp::CallIter && callee.isPrimitive()) {
      MOZ_ASSERT(argc == 0, "thisv must be on top of the stack");
      ReportValueError(cx, JSMSG_NOT_ITERABLE, -1, callArgs.thisv(), nullptr);
      return false;
    }

    if (!CallFromStack(cx, callArgs)) {
      return false;
    }

    res.set(callArgs.rval());
  }

  if (!TypeMonitorResult(cx, stub, frame, script, pc, res)) {
    return false;
  }

  // Try to transition again in case we called this IC recursively.
  if (stub->state().maybeTransition()) {
    stub->discardStubs(cx);
  }
  canAttachStub = stub->state().canAttachStub();

  if (deferred && canAttachStub) {
    HandleValueArray args = HandleValueArray::fromMarkedLocation(argc, vp + 2);
    CallIRGenerator gen(cx, script, pc, op, stub->state().mode(), argc, callee,
                        callArgs.thisv(), newTarget, args);
    switch (gen.tryAttachDeferredStub(res)) {
      case AttachDecision::Attach: {
        ICStub* newStub = AttachBaselineCacheIRStub(
            cx, gen.writerRef(), gen.cacheKind(), gen.cacheIRStubKind(), script,
            stub, &handled);
        if (newStub) {
          JitSpew(JitSpew_BaselineIC, "  Attached Call CacheIR stub");

          // If it's an updated stub, initialize it.
          if (gen.cacheIRStubKind() == BaselineCacheIRStubKind::Updated) {
            SetUpdateStubData(newStub->toCacheIR_Updated(),
                              gen.typeCheckInfo());
          }
        }
      } break;
      case AttachDecision::NoAction:
        break;
      case AttachDecision::TemporarilyUnoptimizable:
      case AttachDecision::Deferred:
        MOZ_ASSERT_UNREACHABLE("Impossible attach decision");
        break;
    }
  }

  if (!handled && canAttachStub) {
    stub->state().trackNotAttached();
  }
  return true;
}

bool DoSpreadCallFallback(JSContext* cx, BaselineFrame* frame,
                          ICCall_Fallback* stub, Value* vp,
                          MutableHandleValue res) {
  stub->incrementEnteredCount();

  RootedScript script(cx, frame->script());
  jsbytecode* pc = stub->icEntry()->pc(script);
  JSOp op = JSOp(*pc);
  bool constructing = (op == JSOp::SpreadNew || op == JSOp::SpreadSuperCall);
  FallbackICSpew(cx, stub, "SpreadCall(%s)", CodeName(op));

  // Ensure vp array is rooted - we may GC in here.
  AutoArrayRooter vpRoot(cx, 3 + constructing, vp);

  RootedValue callee(cx, vp[0]);
  RootedValue thisv(cx, vp[1]);
  RootedValue arr(cx, vp[2]);
  RootedValue newTarget(cx, constructing ? vp[3] : NullValue());

  // Transition stub state to megamorphic or generic if warranted.
  if (stub->state().maybeTransition()) {
    stub->discardStubs(cx);
  }

  // Try attaching a call stub.
  bool handled = false;
  if (op != JSOp::SpreadEval && op != JSOp::StrictSpreadEval &&
      stub->state().canAttachStub()) {
    // Try CacheIR first:
    RootedArrayObject aobj(cx, &arr.toObject().as<ArrayObject>());
    MOZ_ASSERT(aobj->length() == aobj->getDenseInitializedLength());

    HandleValueArray args = HandleValueArray::fromMarkedLocation(
        aobj->length(), aobj->getDenseElements());
    CallIRGenerator gen(cx, script, pc, op, stub->state().mode(), 1, callee,
                        thisv, newTarget, args);
    switch (gen.tryAttachStub()) {
      case AttachDecision::NoAction:
        break;
      case AttachDecision::Attach: {
        ICStub* newStub = AttachBaselineCacheIRStub(
            cx, gen.writerRef(), gen.cacheKind(), gen.cacheIRStubKind(), script,
            stub, &handled);

        if (newStub) {
          JitSpew(JitSpew_BaselineIC, "  Attached Spread Call CacheIR stub");

          // If it's an updated stub, initialize it.
          if (gen.cacheIRStubKind() == BaselineCacheIRStubKind::Updated) {
            SetUpdateStubData(newStub->toCacheIR_Updated(),
                              gen.typeCheckInfo());
          }
        }
      } break;
      case AttachDecision::TemporarilyUnoptimizable:
        handled = true;
        break;
      case AttachDecision::Deferred:
        MOZ_ASSERT_UNREACHABLE("No deferred optimizations for spread calls");
        break;
    }
  }

  if (!SpreadCallOperation(cx, script, pc, thisv, callee, arr, newTarget,
                           res)) {
    return false;
  }

  return TypeMonitorResult(cx, stub, frame, script, pc, res);
}

void ICStubCompilerBase::pushCallArguments(MacroAssembler& masm,
                                           AllocatableGeneralRegisterSet regs,
                                           Register argcReg,
                                           bool isConstructing) {
  MOZ_ASSERT(!regs.has(argcReg));

  // argPtr initially points to the last argument.
  Register argPtr = regs.takeAny();
  masm.moveStackPtrTo(argPtr);

  // Skip 4 pointers pushed on top of the arguments: the frame descriptor,
  // return address, old frame pointer and stub reg.
  size_t valueOffset = STUB_FRAME_SIZE;

  // We have to push |this|, callee, new.target (if constructing) and argc
  // arguments. Handle the number of Values we know statically first.

  size_t numNonArgValues = 2 + isConstructing;
  for (size_t i = 0; i < numNonArgValues; i++) {
    masm.pushValue(Address(argPtr, valueOffset));
    valueOffset += sizeof(Value);
  }

  // If there are no arguments we're done.
  Label done;
  masm.branchTest32(Assembler::Zero, argcReg, argcReg, &done);

  // Push argc Values.
  Label loop;
  Register count = regs.takeAny();
  masm.addPtr(Imm32(valueOffset), argPtr);
  masm.move32(argcReg, count);
  masm.bind(&loop);
  {
    masm.pushValue(Address(argPtr, 0));
    masm.addPtr(Imm32(sizeof(Value)), argPtr);

    masm.branchSub32(Assembler::NonZero, Imm32(1), count, &loop);
  }
  masm.bind(&done);
}

bool FallbackICCodeCompiler::emitCall(bool isSpread, bool isConstructing) {
  static_assert(R0 == JSReturnOperand);

  // Values are on the stack left-to-right. Calling convention wants them
  // right-to-left so duplicate them on the stack in reverse order.
  // |this| and callee are pushed last.

  AllocatableGeneralRegisterSet regs(availableGeneralRegs(0));

  if (MOZ_UNLIKELY(isSpread)) {
    // Push a stub frame so that we can perform a non-tail call.
    enterStubFrame(masm, R1.scratchReg());

    // Use BaselineFrameReg instead of BaselineStackReg, because
    // BaselineFrameReg and BaselineStackReg hold the same value just after
    // calling enterStubFrame.

    // newTarget
    uint32_t valueOffset = 0;
    if (isConstructing) {
      masm.pushValue(Address(BaselineFrameReg, STUB_FRAME_SIZE));
      valueOffset++;
    }

    // array
    masm.pushValue(Address(BaselineFrameReg,
                           valueOffset * sizeof(Value) + STUB_FRAME_SIZE));
    valueOffset++;

    // this
    masm.pushValue(Address(BaselineFrameReg,
                           valueOffset * sizeof(Value) + STUB_FRAME_SIZE));
    valueOffset++;

    // callee
    masm.pushValue(Address(BaselineFrameReg,
                           valueOffset * sizeof(Value) + STUB_FRAME_SIZE));
    valueOffset++;

    masm.push(masm.getStackPointer());
    masm.push(ICStubReg);

    PushStubPayload(masm, R0.scratchReg());

    using Fn = bool (*)(JSContext*, BaselineFrame*, ICCall_Fallback*, Value*,
                        MutableHandleValue);
    if (!callVM<Fn, DoSpreadCallFallback>(masm)) {
      return false;
    }

    leaveStubFrame(masm);
    EmitReturnFromIC(masm);

    // SpreadCall is not yet supported in Ion, so do not generate asmcode for
    // bailout.
    return true;
  }

  // Push a stub frame so that we can perform a non-tail call.
  enterStubFrame(masm, R1.scratchReg());

  regs.take(R0.scratchReg());  // argc.

  pushCallArguments(masm, regs, R0.scratchReg(), isConstructing);

  masm.push(masm.getStackPointer());
  masm.push(R0.scratchReg());
  masm.push(ICStubReg);

  PushStubPayload(masm, R0.scratchReg());

  using Fn = bool (*)(JSContext*, BaselineFrame*, ICCall_Fallback*, uint32_t,
                      Value*, MutableHandleValue);
  if (!callVM<Fn, DoCallFallback>(masm)) {
    return false;
  }

  leaveStubFrame(masm);
  EmitReturnFromIC(masm);

  // This is the resume point used when bailout rewrites call stack to undo
  // Ion inlined frames. The return address pushed onto reconstructed stack
  // will point here.
  assumeStubFrame();

  MOZ_ASSERT(!isSpread);

  if (isConstructing) {
    code.initBailoutReturnOffset(BailoutReturnKind::New, masm.currentOffset());
  } else {
    code.initBailoutReturnOffset(BailoutReturnKind::Call, masm.currentOffset());
  }

  // Load passed-in ThisV into R1 just in case it's needed.  Need to do this
  // before we leave the stub frame since that info will be lost.
  // Current stack:  [...., ThisV, ActualArgc, CalleeToken, Descriptor ]
  masm.loadValue(Address(masm.getStackPointer(), 3 * sizeof(size_t)), R1);

  leaveStubFrame(masm, true);

  // If this is a |constructing| call, if the callee returns a non-object, we
  // replace it with the |this| object passed in.
  if (isConstructing) {
    static_assert(JSReturnOperand == R0);
    Label skipThisReplace;

    masm.branchTestObject(Assembler::Equal, JSReturnOperand, &skipThisReplace);
    masm.moveValue(R1, R0);
#ifdef DEBUG
    masm.branchTestObject(Assembler::Equal, JSReturnOperand, &skipThisReplace);
    masm.assumeUnreachable("Failed to return object in constructing call.");
#endif
    masm.bind(&skipThisReplace);
  }

  // At this point, ICStubReg points to the ICCall_Fallback stub, which is NOT
  // a MonitoredStub, but rather a MonitoredFallbackStub.  To use
  // EmitEnterTypeMonitorIC, first load the ICTypeMonitor_Fallback stub into
  // ICStubReg.  Then, use EmitEnterTypeMonitorIC with a custom struct offset.
  // Note that we must have a non-null fallbackMonitorStub here because
  // InitFromBailout delazifies.
  masm.loadPtr(Address(ICStubReg,
                       ICMonitoredFallbackStub::offsetOfFallbackMonitorStub()),
               ICStubReg);
  EmitEnterTypeMonitorIC(masm,
                         ICTypeMonitor_Fallback::offsetOfFirstMonitorStub());

  return true;
}

bool FallbackICCodeCompiler::emit_Call() {
  return emitCall(/* isSpread = */ false, /* isConstructing = */ false);
}

bool FallbackICCodeCompiler::emit_CallConstructing() {
  return emitCall(/* isSpread = */ false, /* isConstructing = */ true);
}

bool FallbackICCodeCompiler::emit_SpreadCall() {
  return emitCall(/* isSpread = */ true, /* isConstructing = */ false);
}

bool FallbackICCodeCompiler::emit_SpreadCallConstructing() {
  return emitCall(/* isSpread = */ true, /* isConstructing = */ true);
}

//
// GetIterator_Fallback
//

bool DoGetIteratorFallback(JSContext* cx, BaselineFrame* frame,
                           ICGetIterator_Fallback* stub, HandleValue value,
                           MutableHandleValue res) {
  stub->incrementEnteredCount();
  FallbackICSpew(cx, stub, "GetIterator");

  TryAttachStub<GetIteratorIRGenerator>(
      "GetIterator", cx, frame, stub, BaselineCacheIRStubKind::Regular, value);

  JSObject* iterobj = ValueToIterator(cx, value);
  if (!iterobj) {
    return false;
  }

  res.setObject(*iterobj);
  return true;
}

bool FallbackICCodeCompiler::emit_GetIterator() {
  EmitRestoreTailCallReg(masm);

  // Sync stack for the decompiler.
  masm.pushValue(R0);

  masm.pushValue(R0);
  masm.push(ICStubReg);
  pushStubPayload(masm, R0.scratchReg());

  using Fn = bool (*)(JSContext*, BaselineFrame*, ICGetIterator_Fallback*,
                      HandleValue, MutableHandleValue);
  return tailCallVM<Fn, DoGetIteratorFallback>(masm);
}

//
// InstanceOf_Fallback
//

bool DoInstanceOfFallback(JSContext* cx, BaselineFrame* frame,
                          ICInstanceOf_Fallback* stub, HandleValue lhs,
                          HandleValue rhs, MutableHandleValue res) {
  stub->incrementEnteredCount();

  FallbackICSpew(cx, stub, "InstanceOf");

  if (!rhs.isObject()) {
    ReportValueError(cx, JSMSG_BAD_INSTANCEOF_RHS, -1, rhs, nullptr);
    return false;
  }

  RootedObject obj(cx, &rhs.toObject());
  bool cond = false;
  if (!HasInstance(cx, obj, lhs, &cond)) {
    return false;
  }

  res.setBoolean(cond);

  if (!obj->is<JSFunction>()) {
    // ensure we've recorded at least one failure, so we can detect there was a
    // non-optimizable case
    if (!stub->state().hasFailures()) {
      stub->state().trackNotAttached();
    }
    return true;
  }

  // For functions, keep track of the |prototype| property in type information,
  // for use during Ion compilation.
  EnsureTrackPropertyTypes(cx, obj, NameToId(cx->names().prototype));

  TryAttachStub<InstanceOfIRGenerator>("InstanceOf", cx, frame, stub,
                                       BaselineCacheIRStubKind::Regular, lhs,
                                       obj);
  return true;
}

bool FallbackICCodeCompiler::emit_InstanceOf() {
  EmitRestoreTailCallReg(masm);

  // Sync stack for the decompiler.
  masm.pushValue(R0);
  masm.pushValue(R1);

  masm.pushValue(R1);
  masm.pushValue(R0);
  masm.push(ICStubReg);
  pushStubPayload(masm, R0.scratchReg());

  using Fn = bool (*)(JSContext*, BaselineFrame*, ICInstanceOf_Fallback*,
                      HandleValue, HandleValue, MutableHandleValue);
  return tailCallVM<Fn, DoInstanceOfFallback>(masm);
}

//
// TypeOf_Fallback
//

bool DoTypeOfFallback(JSContext* cx, BaselineFrame* frame,
                      ICTypeOf_Fallback* stub, HandleValue val,
                      MutableHandleValue res) {
  stub->incrementEnteredCount();
  FallbackICSpew(cx, stub, "TypeOf");

  TryAttachStub<TypeOfIRGenerator>("TypeOf", cx, frame, stub,
                                   BaselineCacheIRStubKind::Regular, val);

  JSType type = js::TypeOfValue(val);
  RootedString string(cx, TypeName(type, cx->names()));
  res.setString(string);
  return true;
}

bool FallbackICCodeCompiler::emit_TypeOf() {
  EmitRestoreTailCallReg(masm);

  masm.pushValue(R0);
  masm.push(ICStubReg);
  pushStubPayload(masm, R0.scratchReg());

  using Fn = bool (*)(JSContext*, BaselineFrame*, ICTypeOf_Fallback*,
                      HandleValue, MutableHandleValue);
  return tailCallVM<Fn, DoTypeOfFallback>(masm);
}

ICTypeMonitor_SingleObject::ICTypeMonitor_SingleObject(JitCode* stubCode,
                                                       JSObject* obj)
    : ICStub(TypeMonitor_SingleObject, stubCode), obj_(obj) {}

ICTypeMonitor_ObjectGroup::ICTypeMonitor_ObjectGroup(JitCode* stubCode,
                                                     ObjectGroup* group)
    : ICStub(TypeMonitor_ObjectGroup, stubCode), group_(group) {}

ICTypeUpdate_SingleObject::ICTypeUpdate_SingleObject(JitCode* stubCode,
                                                     JSObject* obj)
    : ICStub(TypeUpdate_SingleObject, stubCode), obj_(obj) {}

ICTypeUpdate_ObjectGroup::ICTypeUpdate_ObjectGroup(JitCode* stubCode,
                                                   ObjectGroup* group)
    : ICStub(TypeUpdate_ObjectGroup, stubCode), group_(group) {}

//
// Rest_Fallback
//

bool DoRestFallback(JSContext* cx, BaselineFrame* frame, ICRest_Fallback* stub,
                    MutableHandleValue res) {
  unsigned numFormals = frame->numFormalArgs() - 1;
  unsigned numActuals = frame->numActualArgs();
  unsigned numRest = numActuals > numFormals ? numActuals - numFormals : 0;
  Value* rest = frame->argv() + numFormals;

  ArrayObject* obj =
      ObjectGroup::newArrayObject(cx, rest, numRest, GenericObject,
                                  ObjectGroup::NewArrayKind::UnknownIndex);
  if (!obj) {
    return false;
  }
  res.setObject(*obj);
  return true;
}

bool FallbackICCodeCompiler::emit_Rest() {
  EmitRestoreTailCallReg(masm);

  masm.push(ICStubReg);
  pushStubPayload(masm, R0.scratchReg());

  using Fn = bool (*)(JSContext*, BaselineFrame*, ICRest_Fallback*,
                      MutableHandleValue);
  return tailCallVM<Fn, DoRestFallback>(masm);
}

//
// UnaryArith_Fallback
//

bool DoUnaryArithFallback(JSContext* cx, BaselineFrame* frame,
                          ICUnaryArith_Fallback* stub, HandleValue val,
                          MutableHandleValue res) {
  stub->incrementEnteredCount();

  RootedScript script(cx, frame->script());
  jsbytecode* pc = stub->icEntry()->pc(script);
  JSOp op = JSOp(*pc);
  FallbackICSpew(cx, stub, "UnaryArith(%s)", CodeName(op));

  switch (op) {
    case JSOp::BitNot: {
      res.set(val);
      if (!BitNot(cx, res, res)) {
        return false;
      }
      break;
    }
    case JSOp::Pos: {
      res.set(val);
      if (!ToNumber(cx, res)) {
        return false;
      }
      break;
    }
    case JSOp::Neg: {
      res.set(val);
      if (!NegOperation(cx, res, res)) {
        return false;
      }
      break;
    }
    case JSOp::Inc: {
      if (!IncOperation(cx, val, res)) {
        return false;
      }
      break;
    }
    case JSOp::Dec: {
      if (!DecOperation(cx, val, res)) {
        return false;
      }
      break;
    }
    default:
      MOZ_CRASH("Unexpected op");
  }
  MOZ_ASSERT(res.isNumeric());

  if (res.isDouble()) {
    stub->setSawDoubleResult();
  }

  TryAttachStub<UnaryArithIRGenerator>("UnaryArith", cx, frame, stub,
                                       BaselineCacheIRStubKind::Regular, op,
                                       val, res);
  return true;
}

bool FallbackICCodeCompiler::emit_UnaryArith() {
  static_assert(R0 == JSReturnOperand);

  // Restore the tail call register.
  EmitRestoreTailCallReg(masm);

  // Ensure stack is fully synced for the expression decompiler.
  masm.pushValue(R0);

  // Push arguments.
  masm.pushValue(R0);
  masm.push(ICStubReg);
  pushStubPayload(masm, R0.scratchReg());

  using Fn = bool (*)(JSContext*, BaselineFrame*, ICUnaryArith_Fallback*,
                      HandleValue, MutableHandleValue);
  return tailCallVM<Fn, DoUnaryArithFallback>(masm);
}

//
// BinaryArith_Fallback
//

bool DoBinaryArithFallback(JSContext* cx, BaselineFrame* frame,
                           ICBinaryArith_Fallback* stub, HandleValue lhs,
                           HandleValue rhs, MutableHandleValue ret) {
  stub->incrementEnteredCount();

  RootedScript script(cx, frame->script());
  jsbytecode* pc = stub->icEntry()->pc(script);
  JSOp op = JSOp(*pc);
  FallbackICSpew(
      cx, stub, "CacheIRBinaryArith(%s,%d,%d)", CodeName(op),
      int(lhs.isDouble() ? JSVAL_TYPE_DOUBLE : lhs.extractNonDoubleType()),
      int(rhs.isDouble() ? JSVAL_TYPE_DOUBLE : rhs.extractNonDoubleType()));

  // Don't pass lhs/rhs directly, we need the original values when
  // generating stubs.
  RootedValue lhsCopy(cx, lhs);
  RootedValue rhsCopy(cx, rhs);

  // Perform the compare operation.
  switch (op) {
    case JSOp::Add:
      // Do an add.
      if (!AddValues(cx, &lhsCopy, &rhsCopy, ret)) {
        return false;
      }
      break;
    case JSOp::Sub:
      if (!SubValues(cx, &lhsCopy, &rhsCopy, ret)) {
        return false;
      }
      break;
    case JSOp::Mul:
      if (!MulValues(cx, &lhsCopy, &rhsCopy, ret)) {
        return false;
      }
      break;
    case JSOp::Div:
      if (!DivValues(cx, &lhsCopy, &rhsCopy, ret)) {
        return false;
      }
      break;
    case JSOp::Mod:
      if (!ModValues(cx, &lhsCopy, &rhsCopy, ret)) {
        return false;
      }
      break;
    case JSOp::Pow:
      if (!PowValues(cx, &lhsCopy, &rhsCopy, ret)) {
        return false;
      }
      break;
    case JSOp::BitOr: {
      if (!BitOr(cx, &lhsCopy, &rhsCopy, ret)) {
        return false;
      }
      break;
    }
    case JSOp::BitXor: {
      if (!BitXor(cx, &lhsCopy, &rhsCopy, ret)) {
        return false;
      }
      break;
    }
    case JSOp::BitAnd: {
      if (!BitAnd(cx, &lhsCopy, &rhsCopy, ret)) {
        return false;
      }
      break;
    }
    case JSOp::Lsh: {
      if (!BitLsh(cx, &lhsCopy, &rhsCopy, ret)) {
        return false;
      }
      break;
    }
    case JSOp::Rsh: {
      if (!BitRsh(cx, &lhsCopy, &rhsCopy, ret)) {
        return false;
      }
      break;
    }
    case JSOp::Ursh: {
      if (!UrshOperation(cx, &lhsCopy, &rhsCopy, ret)) {
        return false;
      }
      break;
    }
    default:
      MOZ_CRASH("Unhandled baseline arith op");
  }

  if (ret.isDouble()) {
    stub->setSawDoubleResult();
  }

  TryAttachStub<BinaryArithIRGenerator>("BinaryArith", cx, frame, stub,
                                        BaselineCacheIRStubKind::Regular, op,
                                        lhs, rhs, ret);
  return true;
}

bool FallbackICCodeCompiler::emit_BinaryArith() {
  static_assert(R0 == JSReturnOperand);

  // Restore the tail call register.
  EmitRestoreTailCallReg(masm);

  // Ensure stack is fully synced for the expression decompiler.
  masm.pushValue(R0);
  masm.pushValue(R1);

  // Push arguments.
  masm.pushValue(R1);
  masm.pushValue(R0);
  masm.push(ICStubReg);
  pushStubPayload(masm, R0.scratchReg());

  using Fn = bool (*)(JSContext*, BaselineFrame*, ICBinaryArith_Fallback*,
                      HandleValue, HandleValue, MutableHandleValue);
  return tailCallVM<Fn, DoBinaryArithFallback>(masm);
}

//
// Compare_Fallback
//
bool DoCompareFallback(JSContext* cx, BaselineFrame* frame,
                       ICCompare_Fallback* stub, HandleValue lhs,
                       HandleValue rhs, MutableHandleValue ret) {
  stub->incrementEnteredCount();

  RootedScript script(cx, frame->script());
  jsbytecode* pc = stub->icEntry()->pc(script);
  JSOp op = JSOp(*pc);

  FallbackICSpew(cx, stub, "Compare(%s)", CodeName(op));

  // Don't pass lhs/rhs directly, we need the original values when
  // generating stubs.
  RootedValue lhsCopy(cx, lhs);
  RootedValue rhsCopy(cx, rhs);

  // Perform the compare operation.
  bool out;
  switch (op) {
    case JSOp::Lt:
      if (!LessThan(cx, &lhsCopy, &rhsCopy, &out)) {
        return false;
      }
      break;
    case JSOp::Le:
      if (!LessThanOrEqual(cx, &lhsCopy, &rhsCopy, &out)) {
        return false;
      }
      break;
    case JSOp::Gt:
      if (!GreaterThan(cx, &lhsCopy, &rhsCopy, &out)) {
        return false;
      }
      break;
    case JSOp::Ge:
      if (!GreaterThanOrEqual(cx, &lhsCopy, &rhsCopy, &out)) {
        return false;
      }
      break;
    case JSOp::Eq:
      if (!LooselyEqual<EqualityKind::Equal>(cx, &lhsCopy, &rhsCopy, &out)) {
        return false;
      }
      break;
    case JSOp::Ne:
      if (!LooselyEqual<EqualityKind::NotEqual>(cx, &lhsCopy, &rhsCopy, &out)) {
        return false;
      }
      break;
    case JSOp::StrictEq:
      if (!StrictlyEqual<EqualityKind::Equal>(cx, &lhsCopy, &rhsCopy, &out)) {
        return false;
      }
      break;
    case JSOp::StrictNe:
      if (!StrictlyEqual<EqualityKind::NotEqual>(cx, &lhsCopy, &rhsCopy,
                                                 &out)) {
        return false;
      }
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("Unhandled baseline compare op");
      return false;
  }

  ret.setBoolean(out);

  TryAttachStub<CompareIRGenerator>("Compare", cx, frame, stub,
                                    BaselineCacheIRStubKind::Regular, op, lhs,
                                    rhs);
  return true;
}

bool FallbackICCodeCompiler::emit_Compare() {
  static_assert(R0 == JSReturnOperand);

  // Restore the tail call register.
  EmitRestoreTailCallReg(masm);

  // Ensure stack is fully synced for the expression decompiler.
  masm.pushValue(R0);
  masm.pushValue(R1);

  // Push arguments.
  masm.pushValue(R1);
  masm.pushValue(R0);
  masm.push(ICStubReg);
  pushStubPayload(masm, R0.scratchReg());

  using Fn = bool (*)(JSContext*, BaselineFrame*, ICCompare_Fallback*,
                      HandleValue, HandleValue, MutableHandleValue);
  return tailCallVM<Fn, DoCompareFallback>(masm);
}

//
// NewArray_Fallback
//

bool DoNewArrayFallback(JSContext* cx, BaselineFrame* frame,
                        ICNewArray_Fallback* stub, uint32_t length,
                        MutableHandleValue res) {
  stub->incrementEnteredCount();
  FallbackICSpew(cx, stub, "NewArray");

  RootedObject obj(cx);
  if (stub->templateObject()) {
    RootedObject templateObject(cx, stub->templateObject());
    obj = NewArrayOperationWithTemplate(cx, templateObject);
    if (!obj) {
      return false;
    }
  } else {
    RootedScript script(cx, frame->script());
    jsbytecode* pc = stub->icEntry()->pc(script);

    obj = NewArrayOperation(cx, script, pc, length);
    if (!obj) {
      return false;
    }

    if (!obj->isSingleton()) {
      JSObject* templateObject =
          NewArrayOperation(cx, script, pc, length, TenuredObject);
      if (!templateObject) {
        return false;
      }
      stub->setTemplateObject(templateObject);
    }
  }

  res.setObject(*obj);
  return true;
}

bool FallbackICCodeCompiler::emit_NewArray() {
  EmitRestoreTailCallReg(masm);

  masm.push(R0.scratchReg());  // length
  masm.push(ICStubReg);        // stub.
  masm.pushBaselineFramePtr(BaselineFrameReg, R0.scratchReg());

  using Fn = bool (*)(JSContext*, BaselineFrame*, ICNewArray_Fallback*,
                      uint32_t, MutableHandleValue);
  return tailCallVM<Fn, DoNewArrayFallback>(masm);
}

//
// NewObject_Fallback
//
bool DoNewObjectFallback(JSContext* cx, BaselineFrame* frame,
                         ICNewObject_Fallback* stub, MutableHandleValue res) {
  stub->incrementEnteredCount();
  FallbackICSpew(cx, stub, "NewObject");

  RootedObject obj(cx);

  RootedObject templateObject(cx, stub->templateObject());
  if (templateObject) {
    MOZ_ASSERT(
        !templateObject->group()->maybePreliminaryObjectsDontCheckGeneration());
    obj = NewObjectOperationWithTemplate(cx, templateObject);
  } else {
    RootedScript script(cx, frame->script());
    jsbytecode* pc = stub->icEntry()->pc(script);
    obj = NewObjectOperation(cx, script, pc);

    if (obj && !obj->isSingleton() &&
        !obj->group()->maybePreliminaryObjectsDontCheckGeneration()) {
      templateObject = NewObjectOperation(cx, script, pc, TenuredObject);
      if (!templateObject) {
        return false;
      }

      TryAttachStub<NewObjectIRGenerator>("NewObject", cx, frame, stub,
                                          BaselineCacheIRStubKind::Regular,
                                          JSOp(*pc), templateObject);

      stub->setTemplateObject(templateObject);
    }
  }

  if (!obj) {
    return false;
  }

  res.setObject(*obj);
  return true;
}

bool FallbackICCodeCompiler::emit_NewObject() {
  EmitRestoreTailCallReg(masm);

  masm.push(ICStubReg);  // stub.
  pushStubPayload(masm, R0.scratchReg());

  using Fn = bool (*)(JSContext*, BaselineFrame*, ICNewObject_Fallback*,
                      MutableHandleValue);
  return tailCallVM<Fn, DoNewObjectFallback>(masm);
}

bool JitRuntime::generateBaselineICFallbackCode(JSContext* cx) {
  StackMacroAssembler masm;

  BaselineICFallbackCode& fallbackCode = baselineICFallbackCode_.ref();
  FallbackICCodeCompiler compiler(cx, fallbackCode, masm);

  JitSpew(JitSpew_Codegen, "# Emitting Baseline IC fallback code");

#define EMIT_CODE(kind)                                            \
  {                                                                \
    uint32_t offset = startTrampolineCode(masm);                   \
    InitMacroAssemblerForICStub(masm);                             \
    if (!compiler.emit_##kind()) {                                 \
      return false;                                                \
    }                                                              \
    fallbackCode.initOffset(BaselineICFallbackKind::kind, offset); \
  }
  IC_BASELINE_FALLBACK_CODE_KIND_LIST(EMIT_CODE)
#undef EMIT_CODE

  Linker linker(masm);
  JitCode* code = linker.newCode(cx, CodeKind::Other);
  if (!code) {
    return false;
  }

#ifdef JS_ION_PERF
  writePerfSpewerJitCodeProfile(code, "BaselineICFallback");
#endif
#ifdef MOZ_VTUNE
  vtune::MarkStub(code, "BaselineICFallback");
#endif

  fallbackCode.initCode(code);
  return true;
}

}  // namespace jit
}  // namespace js
