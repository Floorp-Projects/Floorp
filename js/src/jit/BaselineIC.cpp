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
#include "vm/JSFunction.h"
#include "vm/Opcodes.h"
#include "vm/SelfHosting.h"
#include "vm/TypedArrayObject.h"
#include "vtune/VTuneWrapper.h"

#include "builtin/Boolean-inl.h"

#include "jit/JitFrames-inl.h"
#include "jit/MacroAssembler-inl.h"
#include "jit/shared/Lowering-shared-inl.h"
#include "jit/SharedICHelpers-inl.h"
#include "jit/VMFunctionList-inl.h"
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
        "Fallback hit for (%s:%u:%u) (pc=%zu,line=%d,uses=%d,stubs=%zu): %s",
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
            "(pc=%zu,line=%d,uses=%d,stubs=%d): %s",
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

/* static */
UniquePtr<ICScript> ICScript::create(JSContext* cx, JSScript* script) {
  MOZ_ASSERT(cx->realm()->jitRealm());
  MOZ_ASSERT(jit::IsBaselineEnabled(cx));

  const uint32_t numICEntries = script->numICEntries();

  // Allocate the ICScript.
  UniquePtr<ICScript> icScript(
      script->zone()->pod_malloc_with_extra<ICScript, ICEntry>(numICEntries));
  if (!icScript) {
    ReportOutOfMemory(cx);
    return nullptr;
  }
  new (icScript.get()) ICScript(numICEntries);

  // We need to call prepareForDestruction on ICScript before we |delete| it.
  auto prepareForDestruction = mozilla::MakeScopeExit(
      [&] { icScript->prepareForDestruction(cx->zone()); });

  FallbackStubAllocator alloc(cx, icScript->fallbackStubSpace_);

  // Index of the next ICEntry to initialize.
  uint32_t icEntryIndex = 0;

  using Kind = BaselineICFallbackKind;

  ICScript* icScriptPtr = icScript.get();
  auto addIC = [cx, icScriptPtr, &icEntryIndex, script](jsbytecode* pc,
                                                        ICStub* stub) {
    if (!stub) {
      MOZ_ASSERT(cx->isExceptionPending());
      mozilla::Unused << cx;  // Silence -Wunused-lambda-capture in opt builds.
      return false;
    }

    // Initialize the ICEntry.
    uint32_t offset = pc ? script->pcToOffset(pc) : ICEntry::ProloguePCOffset;
    ICEntry& entryRef = icScriptPtr->icEntry(icEntryIndex);
    icEntryIndex++;
    new (&entryRef) ICEntry(stub, offset);

    // Fix up pointers from fallback stubs to the ICEntry.
    if (stub->isFallback()) {
      stub->toFallbackStub()->fixupICEntry(&entryRef);
    } else {
      stub->toTypeMonitor_Fallback()->fixupICEntry(&entryRef);
    }

    return true;
  };

  // Add ICEntries and fallback stubs for this/argument type checks.
  // Note: we pass a nullptr pc to indicate this is a non-op IC.
  // See ICEntry::NonOpPCOffset.
  if (JSFunction* fun = script->functionNonDelazifying()) {
    ICStub* stub =
        alloc.newStub<ICTypeMonitor_Fallback>(Kind::TypeMonitor, nullptr, 0);
    if (!addIC(nullptr, stub)) {
      return nullptr;
    }

    for (size_t i = 0; i < fun->nargs(); i++) {
      ICStub* stub = alloc.newStub<ICTypeMonitor_Fallback>(Kind::TypeMonitor,
                                                           nullptr, i + 1);
      if (!addIC(nullptr, stub)) {
        return nullptr;
      }
    }
  }

  jsbytecode const* pcEnd = script->codeEnd();

  // Add ICEntries and fallback stubs for JOF_IC bytecode ops.
  for (jsbytecode* pc = script->code(); pc < pcEnd; pc = GetNextPc(pc)) {
    JSOp op = JSOp(*pc);

    // Assert the frontend stored the correct IC index in jump target ops.
    MOZ_ASSERT_IF(BytecodeIsJumpTarget(op), GET_ICINDEX(pc) == icEntryIndex);

    if (!BytecodeOpHasIC(op)) {
      continue;
    }

    switch (op) {
      case JSOP_NOT:
      case JSOP_AND:
      case JSOP_OR:
      case JSOP_IFEQ:
      case JSOP_IFNE: {
        ICStub* stub = alloc.newStub<ICToBool_Fallback>(Kind::ToBool);
        if (!addIC(pc, stub)) {
          return nullptr;
        }
        break;
      }
      case JSOP_BITNOT:
      case JSOP_NEG:
      case JSOP_INC:
      case JSOP_DEC: {
        ICStub* stub = alloc.newStub<ICUnaryArith_Fallback>(Kind::UnaryArith);
        if (!addIC(pc, stub)) {
          return nullptr;
        }
        break;
      }
      case JSOP_BITOR:
      case JSOP_BITXOR:
      case JSOP_BITAND:
      case JSOP_LSH:
      case JSOP_RSH:
      case JSOP_URSH:
      case JSOP_ADD:
      case JSOP_SUB:
      case JSOP_MUL:
      case JSOP_DIV:
      case JSOP_MOD:
      case JSOP_POW: {
        ICStub* stub = alloc.newStub<ICBinaryArith_Fallback>(Kind::BinaryArith);
        if (!addIC(pc, stub)) {
          return nullptr;
        }
        break;
      }
      case JSOP_EQ:
      case JSOP_NE:
      case JSOP_LT:
      case JSOP_LE:
      case JSOP_GT:
      case JSOP_GE:
      case JSOP_STRICTEQ:
      case JSOP_STRICTNE: {
        ICStub* stub = alloc.newStub<ICCompare_Fallback>(Kind::Compare);
        if (!addIC(pc, stub)) {
          return nullptr;
        }
        break;
      }
      case JSOP_LOOPENTRY: {
        ICStub* stub =
            alloc.newStub<ICWarmUpCounter_Fallback>(Kind::WarmUpCounter);
        if (!addIC(pc, stub)) {
          return nullptr;
        }
        break;
      }
      case JSOP_NEWARRAY: {
        ObjectGroup* group =
            ObjectGroup::allocationSiteGroup(cx, script, pc, JSProto_Array);
        if (!group) {
          return nullptr;
        }
        ICStub* stub =
            alloc.newStub<ICNewArray_Fallback>(Kind::NewArray, group);
        if (!addIC(pc, stub)) {
          return nullptr;
        }
        break;
      }
      case JSOP_NEWOBJECT:
      case JSOP_NEWINIT: {
        ICStub* stub = alloc.newStub<ICNewObject_Fallback>(Kind::NewObject);
        if (!addIC(pc, stub)) {
          return nullptr;
        }
        break;
      }
      case JSOP_INITELEM:
      case JSOP_INITHIDDENELEM:
      case JSOP_INITELEM_ARRAY:
      case JSOP_INITELEM_INC:
      case JSOP_SETELEM:
      case JSOP_STRICTSETELEM: {
        ICStub* stub = alloc.newStub<ICSetElem_Fallback>(Kind::SetElem);
        if (!addIC(pc, stub)) {
          return nullptr;
        }
        break;
      }
      case JSOP_INITPROP:
      case JSOP_INITLOCKEDPROP:
      case JSOP_INITHIDDENPROP:
      case JSOP_INITGLEXICAL:
      case JSOP_SETPROP:
      case JSOP_STRICTSETPROP:
      case JSOP_SETNAME:
      case JSOP_STRICTSETNAME:
      case JSOP_SETGNAME:
      case JSOP_STRICTSETGNAME: {
        ICStub* stub = alloc.newStub<ICSetProp_Fallback>(Kind::SetProp);
        if (!addIC(pc, stub)) {
          return nullptr;
        }
        break;
      }
      case JSOP_GETPROP:
      case JSOP_CALLPROP:
      case JSOP_LENGTH:
      case JSOP_GETBOUNDNAME: {
        ICStub* stub = alloc.newStub<ICGetProp_Fallback>(Kind::GetProp);
        if (!addIC(pc, stub)) {
          return nullptr;
        }
        break;
      }
      case JSOP_GETPROP_SUPER: {
        ICStub* stub = alloc.newStub<ICGetProp_Fallback>(Kind::GetPropSuper);
        if (!addIC(pc, stub)) {
          return nullptr;
        }
        break;
      }
      case JSOP_GETELEM:
      case JSOP_CALLELEM: {
        ICStub* stub = alloc.newStub<ICGetElem_Fallback>(Kind::GetElem);
        if (!addIC(pc, stub)) {
          return nullptr;
        }
        break;
      }
      case JSOP_GETELEM_SUPER: {
        ICStub* stub = alloc.newStub<ICGetElem_Fallback>(Kind::GetElemSuper);
        if (!addIC(pc, stub)) {
          return nullptr;
        }
        break;
      }
      case JSOP_IN: {
        ICStub* stub = alloc.newStub<ICIn_Fallback>(Kind::In);
        if (!addIC(pc, stub)) {
          return nullptr;
        }
        break;
      }
      case JSOP_HASOWN: {
        ICStub* stub = alloc.newStub<ICHasOwn_Fallback>(Kind::HasOwn);
        if (!addIC(pc, stub)) {
          return nullptr;
        }
        break;
      }
      case JSOP_GETNAME:
      case JSOP_GETGNAME: {
        ICStub* stub = alloc.newStub<ICGetName_Fallback>(Kind::GetName);
        if (!addIC(pc, stub)) {
          return nullptr;
        }
        break;
      }
      case JSOP_BINDNAME:
      case JSOP_BINDGNAME: {
        ICStub* stub = alloc.newStub<ICBindName_Fallback>(Kind::BindName);
        if (!addIC(pc, stub)) {
          return nullptr;
        }
        break;
      }
      case JSOP_GETALIASEDVAR:
      case JSOP_GETIMPORT: {
        ICStub* stub =
            alloc.newStub<ICTypeMonitor_Fallback>(Kind::TypeMonitor, nullptr);
        if (!addIC(pc, stub)) {
          return nullptr;
        }
        break;
      }
      case JSOP_GETINTRINSIC: {
        ICStub* stub =
            alloc.newStub<ICGetIntrinsic_Fallback>(Kind::GetIntrinsic);
        if (!addIC(pc, stub)) {
          return nullptr;
        }
        break;
      }
      case JSOP_CALL:
      case JSOP_CALL_IGNORES_RV:
      case JSOP_CALLITER:
      case JSOP_FUNCALL:
      case JSOP_FUNAPPLY:
      case JSOP_EVAL:
      case JSOP_STRICTEVAL: {
        ICStub* stub = alloc.newStub<ICCall_Fallback>(Kind::Call);
        if (!addIC(pc, stub)) {
          return nullptr;
        }
        break;
      }
      case JSOP_SUPERCALL:
      case JSOP_NEW: {
        ICStub* stub = alloc.newStub<ICCall_Fallback>(Kind::CallConstructing);
        if (!addIC(pc, stub)) {
          return nullptr;
        }
        break;
      }
      case JSOP_SPREADCALL:
      case JSOP_SPREADEVAL:
      case JSOP_STRICTSPREADEVAL: {
        ICStub* stub = alloc.newStub<ICCall_Fallback>(Kind::SpreadCall);
        if (!addIC(pc, stub)) {
          return nullptr;
        }
        break;
      }
      case JSOP_SPREADSUPERCALL:
      case JSOP_SPREADNEW: {
        ICStub* stub =
            alloc.newStub<ICCall_Fallback>(Kind::SpreadCallConstructing);
        if (!addIC(pc, stub)) {
          return nullptr;
        }
        break;
      }
      case JSOP_INSTANCEOF: {
        ICStub* stub = alloc.newStub<ICInstanceOf_Fallback>(Kind::InstanceOf);
        if (!addIC(pc, stub)) {
          return nullptr;
        }
        break;
      }
      case JSOP_TYPEOF:
      case JSOP_TYPEOFEXPR: {
        ICStub* stub = alloc.newStub<ICTypeOf_Fallback>(Kind::TypeOf);
        if (!addIC(pc, stub)) {
          return nullptr;
        }
        break;
      }
      case JSOP_ITER: {
        ICStub* stub = alloc.newStub<ICGetIterator_Fallback>(Kind::GetIterator);
        if (!addIC(pc, stub)) {
          return nullptr;
        }
        break;
      }
      case JSOP_REST: {
        ArrayObject* templateObject = ObjectGroup::newArrayObject(
            cx, nullptr, 0, TenuredObject,
            ObjectGroup::NewArrayKind::UnknownIndex);
        if (!templateObject) {
          return nullptr;
        }
        ICStub* stub =
            alloc.newStub<ICRest_Fallback>(Kind::Rest, templateObject);
        if (!addIC(pc, stub)) {
          return nullptr;
        }
        break;
      }
      default:
        MOZ_CRASH("JOF_IC op not handled");
    }
  }

  // Assert all ICEntries have been initialized.
  MOZ_ASSERT(icEntryIndex == numICEntries);

  prepareForDestruction.release();

  return icScript;
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
    case Call_Scripted:
    case Call_AnyScripted:
    case Call_Native:
    case Call_ClassHook:
    case Call_ScriptedApplyArray:
    case Call_ScriptedApplyArguments:
    case Call_ScriptedFunCall:
    case WarmUpCounter_Fallback:
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
    case ICStub::Call_Scripted: {
      ICCall_Scripted* callStub = toCall_Scripted();
      TraceEdge(trc, &callStub->callee(), "baseline-callscripted-callee");
      TraceNullableEdge(trc, &callStub->templateObject(),
                        "baseline-callscripted-template");
      break;
    }
    case ICStub::Call_Native: {
      ICCall_Native* callStub = toCall_Native();
      TraceEdge(trc, &callStub->callee(), "baseline-callnative-callee");
      TraceNullableEdge(trc, &callStub->templateObject(),
                        "baseline-callnative-template");
      break;
    }
    case ICStub::Call_ClassHook: {
      ICCall_ClassHook* callStub = toCall_ClassHook();
      TraceNullableEdge(trc, &callStub->templateObject(),
                        "baseline-callclasshook-template");
      break;
    }
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

//
// WarmUpCounter_Fallback
//

/* clang-format off */
// The following data is kept in a temporary heap-allocated buffer, stored in
// JitRuntime (high memory addresses at top, low at bottom):
//
//     +----->+=================================+  --      <---- High Address
//     |      |                                 |   |
//     |      |     ...BaselineFrame...         |   |-- Copy of BaselineFrame + stack values
//     |      |                                 |   |
//     |      +---------------------------------+   |
//     |      |                                 |   |
//     |      |     ...Locals/Stack...          |   |
//     |      |                                 |   |
//     |      +=================================+  --
//     |      |     Padding(Maybe Empty)        |
//     |      +=================================+  --
//     +------|-- baselineFrame                 |   |-- IonOsrTempData
//            |   jitcode                       |   |
//            +=================================+  --      <---- Low Address
//
// A pointer to the IonOsrTempData is returned.
/* clang-format on */

struct IonOsrTempData {
  void* jitcode;
  uint8_t* baselineFrame;
};

static IonOsrTempData* PrepareOsrTempData(JSContext* cx, BaselineFrame* frame,
                                          void* jitcode) {
  size_t numLocalsAndStackVals = frame->numValueSlots();

  // Calculate the amount of space to allocate:
  //      BaselineFrame space:
  //          (sizeof(Value) * (numLocals + numStackVals))
  //        + sizeof(BaselineFrame)
  //
  //      IonOsrTempData space:
  //          sizeof(IonOsrTempData)

  size_t frameSpace =
      sizeof(BaselineFrame) + sizeof(Value) * numLocalsAndStackVals;
  size_t ionOsrTempDataSpace = sizeof(IonOsrTempData);

  size_t totalSpace = AlignBytes(frameSpace, sizeof(Value)) +
                      AlignBytes(ionOsrTempDataSpace, sizeof(Value));

  IonOsrTempData* info = (IonOsrTempData*)cx->allocateOsrTempData(totalSpace);
  if (!info) {
    ReportOutOfMemory(cx);
    return nullptr;
  }

  memset(info, 0, totalSpace);

  info->jitcode = jitcode;

  // Copy the BaselineFrame + local/stack Values to the buffer. Arguments and
  // |this| are not copied but left on the stack: the Baseline and Ion frame
  // share the same frame prefix and Ion won't clobber these values. Note
  // that info->baselineFrame will point to the *end* of the frame data, like
  // the frame pointer register in baseline frames.
  uint8_t* frameStart =
      (uint8_t*)info + AlignBytes(ionOsrTempDataSpace, sizeof(Value));
  info->baselineFrame = frameStart + frameSpace;

  memcpy(frameStart, (uint8_t*)frame - numLocalsAndStackVals * sizeof(Value),
         frameSpace);

  JitSpew(JitSpew_BaselineOSR, "Allocated IonOsrTempData at %p", (void*)info);
  JitSpew(JitSpew_BaselineOSR, "Jitcode is %p", info->jitcode);

  // All done.
  return info;
}

bool DoWarmUpCounterFallbackOSR(JSContext* cx, BaselineFrame* frame,
                                ICWarmUpCounter_Fallback* stub,
                                IonOsrTempData** infoPtr) {
  MOZ_ASSERT(infoPtr);
  *infoPtr = nullptr;

  RootedScript script(cx, frame->script());
  jsbytecode* pc = stub->icEntry()->pc(script);
  MOZ_ASSERT(JSOp(*pc) == JSOP_LOOPENTRY);

  FallbackICSpew(cx, stub, "WarmUpCounter(%d)", int(script->pcToOffset(pc)));

  if (!IonCompileScriptForBaseline(cx, frame, pc)) {
    return false;
  }

  if (!script->hasIonScript() || script->ionScript()->osrPc() != pc ||
      script->ionScript()->bailoutExpected() || frame->isDebuggee()) {
    return true;
  }

  IonScript* ion = script->ionScript();
  MOZ_ASSERT(cx->runtime()->geckoProfiler().enabled() ==
             ion->hasProfilingInstrumentation());
  MOZ_ASSERT(ion->osrPc() == pc);

  JitSpew(JitSpew_BaselineOSR, "  OSR possible!");
  void* jitcode = ion->method()->raw() + ion->osrEntryOffset();

  // Prepare the temporary heap copy of the fake InterpreterFrame and actual
  // args list.
  JitSpew(JitSpew_BaselineOSR, "Got jitcode.  Preparing for OSR into ion.");
  IonOsrTempData* info = PrepareOsrTempData(cx, frame, jitcode);
  if (!info) {
    return false;
  }
  *infoPtr = info;

  return true;
}

bool FallbackICCodeCompiler::emit_WarmUpCounter() {
  // Push a stub frame so that we can perform a non-tail call.
  enterStubFrame(masm, R1.scratchReg());

  Label noCompiledCode;
  // Call DoWarmUpCounterFallbackOSR to compile/check-for Ion-compiled function
  {
    // Push IonOsrTempData pointer storage
    masm.subFromStackPtr(Imm32(sizeof(void*)));
    masm.push(masm.getStackPointer());

    // Push stub pointer.
    masm.push(ICStubReg);

    pushStubPayload(masm, R0.scratchReg());

    using Fn = bool (*)(JSContext*, BaselineFrame*, ICWarmUpCounter_Fallback*,
                        IonOsrTempData * *infoPtr);
    if (!callVM<Fn, DoWarmUpCounterFallbackOSR>(masm)) {
      return false;
    }

    // Pop IonOsrTempData pointer.
    masm.pop(R0.scratchReg());

    leaveStubFrame(masm);

    // If no JitCode was found, then skip just exit the IC.
    masm.branchPtr(Assembler::Equal, R0.scratchReg(), ImmPtr(nullptr),
                   &noCompiledCode);
  }

  // Get a scratch register.
  AllocatableGeneralRegisterSet regs(availableGeneralRegs(0));
  Register osrDataReg = R0.scratchReg();
  regs.take(osrDataReg);
  regs.takeUnchecked(OsrFrameReg);

  Register scratchReg = regs.takeAny();

  // At this point, stack looks like:
  //  +-> [...Calling-Frame...]
  //  |   [...Actual-Args/ThisV/ArgCount/Callee...]
  //  |   [Descriptor]
  //  |   [Return-Addr]
  //  +---[Saved-FramePtr]            <-- BaselineFrameReg points here.
  //      [...Baseline-Frame...]

  // Restore the stack pointer to point to the saved frame pointer.
  masm.moveToStackPtr(BaselineFrameReg);

  // Discard saved frame pointer, so that the return address is on top of
  // the stack.
  masm.pop(scratchReg);

#ifdef DEBUG
  // If profiler instrumentation is on, ensure that lastProfilingFrame is
  // the frame currently being OSR-ed
  {
    Label checkOk;
    AbsoluteAddress addressOfEnabled(
        cx->runtime()->geckoProfiler().addressOfEnabled());
    masm.branch32(Assembler::Equal, addressOfEnabled, Imm32(0), &checkOk);
    masm.loadPtr(AbsoluteAddress((void*)&cx->jitActivation), scratchReg);
    masm.loadPtr(
        Address(scratchReg, JitActivation::offsetOfLastProfilingFrame()),
        scratchReg);

    // It may be the case that we entered the baseline frame with
    // profiling turned off on, then in a call within a loop (i.e. a
    // callee frame), turn on profiling, then return to this frame,
    // and then OSR with profiling turned on.  In this case, allow for
    // lastProfilingFrame to be null.
    masm.branchPtr(Assembler::Equal, scratchReg, ImmWord(0), &checkOk);

    masm.branchStackPtr(Assembler::Equal, scratchReg, &checkOk);
    masm.assumeUnreachable("Baseline OSR lastProfilingFrame mismatch.");
    masm.bind(&checkOk);
  }
#endif

  // Jump into Ion.
  masm.loadPtr(Address(osrDataReg, offsetof(IonOsrTempData, jitcode)),
               scratchReg);
  masm.loadPtr(Address(osrDataReg, offsetof(IonOsrTempData, baselineFrame)),
               OsrFrameReg);
  masm.jump(scratchReg);

  // No jitcode available, do nothing.
  masm.bind(&noCompiledCode);
  EmitReturnFromIC(masm);
  return true;
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

  if (stub->makesGCCalls() && stub->isMonitored()) {
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
  // In order to silence Coverity - null pointer dereference checker
  MOZ_ASSERT(firstMonitorStub_);
  // If the first monitored stub is a ICTypeMonitor_Fallback stub, then
  // double check that _its_ firstMonitorStub is the same as this one.
  MOZ_ASSERT_IF(
      firstMonitorStub_->isTypeMonitor_Fallback(),
      firstMonitorStub_->toTypeMonitor_Fallback()->firstMonitorStub() ==
          firstMonitorStub_);
}

bool ICMonitoredFallbackStub::initMonitoringChain(JSContext* cx,
                                                  JSScript* script) {
  MOZ_ASSERT(fallbackMonitorStub_ == nullptr);

  ICStubSpace* space = script->icScript()->fallbackStubSpace();
  FallbackStubAllocator alloc(cx, *space);
  auto* stub = alloc.newStub<ICTypeMonitor_Fallback>(
      BaselineICFallbackKind::TypeMonitor, this);
  if (!stub) {
    return false;
  }

  fallbackMonitorStub_ = stub;
  return true;
}

bool ICMonitoredFallbackStub::addMonitorStubForValue(JSContext* cx,
                                                     BaselineFrame* frame,
                                                     StackTypeSet* types,
                                                     HandleValue val) {
  ICTypeMonitor_Fallback* typeMonitorFallback =
      getFallbackMonitorStub(cx, frame->script());
  if (!typeMonitorFallback) {
    return false;
  }
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
    return script->icScript()->fallbackStubSpace();
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
  Linker linker(masm, "getStubCode");
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

void ICScript::noteAccessedGetter(uint32_t pcOffset) {
  ICEntry& entry = icEntryFromPCOffset(pcOffset);
  ICFallbackStub* stub = entry.fallbackStub();

  if (stub->isGetProp_Fallback()) {
    stub->toGetProp_Fallback()->noteAccessedGetter();
  }
}

// TypeMonitor_Fallback
//

bool ICTypeMonitor_Fallback::addMonitorStubForValue(JSContext* cx,
                                                    BaselineFrame* frame,
                                                    StackTypeSet* types,
                                                    HandleValue val) {
  MOZ_ASSERT(types);

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
    if (val.isMagic(JS_UNINITIALIZED_LEXICAL)) {
      return true;
    }
    MOZ_ASSERT(!val.isMagic());
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
  JSScript* script = frame->script();
  jsbytecode* pc = stub->icEntry()->pc(script);
  TypeFallbackICSpew(cx, stub, "TypeMonitor");

  // Copy input value to res.
  res.set(value);

  if (MOZ_UNLIKELY(value.isMagic())) {
    // It's possible that we arrived here from bailing out of Ion, and that
    // Ion proved that the value is dead and optimized out. In such cases,
    // do nothing. However, it's also possible that we have an uninitialized
    // this, in which case we should not look for other magic values.

    if (value.whyMagic() == JS_OPTIMIZED_OUT) {
      MOZ_ASSERT(!stub->monitorsThis());
      return true;
    }

    // In derived class constructors (including nested arrows/eval), the
    // |this| argument or GETALIASEDVAR can return the magic TDZ value.
    MOZ_ASSERT(value.isMagic(JS_UNINITIALIZED_LEXICAL));
    MOZ_ASSERT(frame->isFunctionFrame() || frame->isEvalFrame());
    MOZ_ASSERT(stub->monitorsThis() || *GetNextPc(pc) == JSOP_CHECKTHIS ||
               *GetNextPc(pc) == JSOP_CHECKTHISREINIT ||
               *GetNextPc(pc) == JSOP_CHECKRETURN);
    if (stub->monitorsThis()) {
      TypeScript::SetThis(cx, script, TypeSet::UnknownType());
    } else {
      TypeScript::Monitor(cx, script, pc, TypeSet::UnknownType());
    }
    return true;
  }

  StackTypeSet* types;
  uint32_t argument;
  if (stub->monitorsArgument(&argument)) {
    MOZ_ASSERT(pc == script->code());
    types = TypeScript::ArgTypes(script, argument);
    TypeScript::SetArgument(cx, script, argument, value);
  } else if (stub->monitorsThis()) {
    MOZ_ASSERT(pc == script->code());
    types = TypeScript::ThisTypes(script);
    TypeScript::SetThis(cx, script, value);
  } else {
    types = TypeScript::BytecodeTypes(script, pc);
    TypeScript::Monitor(cx, script, pc, types, value);
  }

  return stub->addMonitorStubForValue(cx, frame, types, value);
}

bool FallbackICCodeCompiler::emit_TypeMonitor() {
  MOZ_ASSERT(R0 == JSReturnOperand);

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
  MOZ_ASSERT(R0 == JSReturnOperand);

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
  StackTypeSet* types = TypeScript::BytecodeTypes(script, pc);

  JSOp op = JSOp(*pc);
  FallbackICSpew(cx, stub, "GetElem(%s)", CodeName[op]);

  MOZ_ASSERT(op == JSOP_GETELEM || op == JSOP_CALLELEM);

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
      TypeScript::Monitor(cx, script, pc, types, res);
    }
  }

  bool attached = TryAttachGetPropStub("GetElem", cx, frame, stub,
                                       CacheKind::GetElem, lhs, rhs, lhs);

  if (!isOptimizedArgs) {
    if (!GetElementOperation(cx, op, lhsCopy, rhs, res)) {
      return false;
    }
    TypeScript::Monitor(cx, script, pc, types, res);
  }

  // Add a type monitor stub for the resulting value.
  if (!stub->addMonitorStubForValue(cx, frame, types, res)) {
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

bool DoGetElemSuperFallback(JSContext* cx, BaselineFrame* frame,
                            ICGetElem_Fallback* stub, HandleValue lhs,
                            HandleValue rhs, HandleValue receiver,
                            MutableHandleValue res) {
  stub->incrementEnteredCount();

  RootedScript script(cx, frame->script());
  jsbytecode* pc = stub->icEntry()->pc(frame->script());
  StackTypeSet* types = TypeScript::BytecodeTypes(script, pc);

  JSOp op = JSOp(*pc);
  FallbackICSpew(cx, stub, "GetElemSuper(%s)", CodeName[op]);

  MOZ_ASSERT(op == JSOP_GETELEM_SUPER);

  bool attached =
      TryAttachGetPropStub("GetElemSuper", cx, frame, stub,
                           CacheKind::GetElemSuper, lhs, rhs, receiver);

  // |lhs| is [[HomeObject]].[[Prototype]] which must be Object
  RootedObject lhsObj(cx, &lhs.toObject());
  if (!GetObjectElementOperation(cx, op, lhsObj, receiver, rhs, res)) {
    return false;
  }
  TypeScript::Monitor(cx, script, pc, types, res);

  // Add a type monitor stub for the resulting value.
  if (!stub->addMonitorStubForValue(cx, frame, types, res)) {
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
  MOZ_ASSERT(R0 == JSReturnOperand);

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
  FallbackICSpew(cx, stub, "SetElem(%s)", CodeName[JSOp(*pc)]);

  MOZ_ASSERT(op == JSOP_SETELEM || op == JSOP_STRICTSETELEM ||
             op == JSOP_INITELEM || op == JSOP_INITHIDDENELEM ||
             op == JSOP_INITELEM_ARRAY || op == JSOP_INITELEM_INC);

  RootedObject obj(cx, ToObjectFromStack(cx, objv));
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

  if (op == JSOP_INITELEM || op == JSOP_INITHIDDENELEM) {
    if (!InitElemOperation(cx, pc, obj, index, rhs)) {
      return false;
    }
  } else if (op == JSOP_INITELEM_ARRAY) {
    MOZ_ASSERT(uint32_t(index.toInt32()) <= INT32_MAX,
               "the bytecode emitter must fail to compile code that would "
               "produce JSOP_INITELEM_ARRAY with an index exceeding "
               "int32_t range");
    MOZ_ASSERT(uint32_t(index.toInt32()) == GET_UINT32(pc));
    if (!InitArrayElemOperation(cx, pc, obj, index.toInt32(), rhs)) {
      return false;
    }
  } else if (op == JSOP_INITELEM_INC) {
    if (!InitArrayElemOperation(cx, pc, obj, index.toInt32(), rhs)) {
      return false;
    }
  } else {
    if (!SetObjectElement(cx, obj, index, rhs, objv,
                          JSOp(*pc) == JSOP_STRICTSETELEM, script, pc)) {
      return false;
    }
  }

  // Don't try to attach stubs that wish to be hidden. We don't know how to
  // have different enumerability in the stubs for the moment.
  if (op == JSOP_INITHIDDENELEM) {
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
  MOZ_ASSERT(R0 == JSReturnOperand);

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

void ICScript::noteHasDenseAdd(uint32_t pcOffset) {
  ICEntry& entry = icEntryFromPCOffset(pcOffset);
  ICFallbackStub* stub = entry.fallbackStub();

  if (stub->isSetElem_Fallback()) {
    stub->toSetElem_Fallback()->noteHasDenseAdd();
  }
}

template <typename T>
void StoreToTypedArray(JSContext* cx, MacroAssembler& masm, Scalar::Type type,
                       const ValueOperand& value, const T& dest,
                       Register scratch, Label* failure) {
  Label done;

  if (type == Scalar::Float32 || type == Scalar::Float64) {
    masm.ensureDouble(value, FloatReg0, failure);
    if (type == Scalar::Float32) {
      ScratchFloat32Scope fpscratch(masm);
      masm.convertDoubleToFloat32(FloatReg0, fpscratch);
      masm.storeToTypedFloatArray(type, fpscratch, dest);
    } else {
      masm.storeToTypedFloatArray(type, FloatReg0, dest);
    }
  } else if (type == Scalar::Uint8Clamped) {
    Label notInt32;
    masm.branchTestInt32(Assembler::NotEqual, value, &notInt32);
    masm.unboxInt32(value, scratch);
    masm.clampIntToUint8(scratch);

    Label clamped;
    masm.bind(&clamped);
    masm.storeToTypedIntArray(type, scratch, dest);
    masm.jump(&done);

    // If the value is a double, clamp to uint8 and jump back.
    // Else, jump to failure.
    masm.bind(&notInt32);
    masm.branchTestDouble(Assembler::NotEqual, value, failure);
    masm.unboxDouble(value, FloatReg0);
    masm.clampDoubleToUint8(FloatReg0, scratch);
    masm.jump(&clamped);
  } else if (type == Scalar::BigInt64 || type == Scalar::BigUint64) {
    // FIXME: https://bugzil.la/1536703
    masm.jump(failure);
  } else {
    Label notInt32;
    masm.branchTestInt32(Assembler::NotEqual, value, &notInt32);
    masm.unboxInt32(value, scratch);

    Label isInt32;
    masm.bind(&isInt32);
    masm.storeToTypedIntArray(type, scratch, dest);
    masm.jump(&done);

    // If the value is a double, truncate and jump back.
    // Else, jump to failure.
    masm.bind(&notInt32);
    masm.branchTestDouble(Assembler::NotEqual, value, failure);
    masm.unboxDouble(value, FloatReg0);
    masm.branchTruncateDoubleMaybeModUint32(FloatReg0, scratch, failure);
    masm.jump(&isInt32);
  }

  masm.bind(&done);
}

template void StoreToTypedArray(JSContext* cx, MacroAssembler& masm,
                                Scalar::Type type, const ValueOperand& value,
                                const Address& dest, Register scratch,
                                Label* failure);

template void StoreToTypedArray(JSContext* cx, MacroAssembler& masm,
                                Scalar::Type type, const ValueOperand& value,
                                const BaseIndex& dest, Register scratch,
                                Label* failure);

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
  FallbackICSpew(cx, stub, "GetName(%s)", CodeName[JSOp(*pc)]);

  MOZ_ASSERT(op == JSOP_GETNAME || op == JSOP_GETGNAME);

  RootedPropertyName name(cx, script->getName(pc));

  TryAttachStub<GetNameIRGenerator>("GetName", cx, frame, stub,
                                    BaselineCacheIRStubKind::Monitored,
                                    envChain, name);

  static_assert(JSOP_GETGNAME_LENGTH == JSOP_GETNAME_LENGTH,
                "Otherwise our check for JSOP_TYPEOF isn't ok");
  if (JSOp(pc[JSOP_GETGNAME_LENGTH]) == JSOP_TYPEOF) {
    if (!GetEnvironmentName<GetNameMode::TypeOf>(cx, envChain, name, res)) {
      return false;
    }
  } else {
    if (!GetEnvironmentName<GetNameMode::Normal>(cx, envChain, name, res)) {
      return false;
    }
  }

  StackTypeSet* types = TypeScript::BytecodeTypes(script, pc);
  TypeScript::Monitor(cx, script, pc, types, res);

  // Add a type monitor stub for the resulting value.
  if (!stub->addMonitorStubForValue(cx, frame, types, res)) {
    return false;
  }

  return true;
}

bool FallbackICCodeCompiler::emit_GetName() {
  MOZ_ASSERT(R0 == JSReturnOperand);

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
  FallbackICSpew(cx, stub, "BindName(%s)", CodeName[JSOp(*pc)]);

  MOZ_ASSERT(op == JSOP_BINDNAME || op == JSOP_BINDGNAME);

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
  MOZ_ASSERT(R0 == JSReturnOperand);

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
  FallbackICSpew(cx, stub, "GetIntrinsic(%s)", CodeName[JSOp(*pc)]);

  MOZ_ASSERT(op == JSOP_GETINTRINSIC);

  if (!GetIntrinsicOperation(cx, script, pc, res)) {
    return false;
  }

  // An intrinsic operation will always produce the same result, so only
  // needs to be monitored once. Attach a stub to load the resulting constant
  // directly.

  TypeScript::Monitor(cx, script, pc, res);

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
    if (op == JSOP_LENGTH) {
      res.setInt32(frame->numActualArgs());
    } else {
      MOZ_ASSERT(name == cx->names().callee);
      MOZ_ASSERT(frame->script()->hasMappedArgsObj());
      res.setObject(*frame->callee());
    }
  } else {
    if (op == JSOP_GETBOUNDNAME) {
      RootedObject env(cx, &val.toObject());
      RootedId id(cx, NameToId(name));
      if (!GetNameBoundInEnvironment(cx, env, id, res)) {
        return false;
      }
    } else {
      MOZ_ASSERT(op == JSOP_GETPROP || op == JSOP_CALLPROP ||
                 op == JSOP_LENGTH);
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
  FallbackICSpew(cx, stub, "GetProp(%s)", CodeName[op]);

  MOZ_ASSERT(op == JSOP_GETPROP || op == JSOP_CALLPROP || op == JSOP_LENGTH ||
             op == JSOP_GETBOUNDNAME);

  RootedPropertyName name(cx, script->getName(pc));
  RootedValue idVal(cx, StringValue(name));

  TryAttachGetPropStub("GetProp", cx, frame, stub, CacheKind::GetProp, val,
                       idVal, val);

  if (!ComputeGetPropResult(cx, frame, op, name, val, res)) {
    return false;
  }

  StackTypeSet* types = TypeScript::BytecodeTypes(script, pc);
  TypeScript::Monitor(cx, script, pc, types, res);

  // Add a type monitor stub for the resulting value.
  if (!stub->addMonitorStubForValue(cx, frame, types, res)) {
    return false;
  }
  return true;
}

bool DoGetPropSuperFallback(JSContext* cx, BaselineFrame* frame,
                            ICGetProp_Fallback* stub, HandleValue receiver,
                            MutableHandleValue val, MutableHandleValue res) {
  stub->incrementEnteredCount();

  RootedScript script(cx, frame->script());
  jsbytecode* pc = stub->icEntry()->pc(script);
  FallbackICSpew(cx, stub, "GetPropSuper(%s)", CodeName[JSOp(*pc)]);

  MOZ_ASSERT(JSOp(*pc) == JSOP_GETPROP_SUPER);

  RootedPropertyName name(cx, script->getName(pc));
  RootedValue idVal(cx, StringValue(name));

  TryAttachGetPropStub("GetPropSuper", cx, frame, stub, CacheKind::GetPropSuper,
                       val, idVal, receiver);

  // |val| is [[HomeObject]].[[Prototype]] which must be Object
  RootedObject valObj(cx, &val.toObject());
  if (!GetProperty(cx, valObj, receiver, name, res)) {
    return false;
  }

  StackTypeSet* types = TypeScript::BytecodeTypes(script, pc);
  TypeScript::Monitor(cx, script, pc, types, res);

  // Add a type monitor stub for the resulting value.
  if (!stub->addMonitorStubForValue(cx, frame, types, res)) {
    return false;
  }

  return true;
}

bool FallbackICCodeCompiler::emitGetProp(bool hasReceiver) {
  MOZ_ASSERT(R0 == JSReturnOperand);

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
  FallbackICSpew(cx, stub, "SetProp(%s)", CodeName[op]);

  MOZ_ASSERT(op == JSOP_SETPROP || op == JSOP_STRICTSETPROP ||
             op == JSOP_SETNAME || op == JSOP_STRICTSETNAME ||
             op == JSOP_SETGNAME || op == JSOP_STRICTSETGNAME ||
             op == JSOP_INITPROP || op == JSOP_INITLOCKEDPROP ||
             op == JSOP_INITHIDDENPROP || op == JSOP_INITGLEXICAL);

  RootedPropertyName name(cx, script->getName(pc));
  RootedId id(cx, NameToId(name));

  RootedObject obj(cx, ToObjectFromStack(cx, lhs));
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

  if (op == JSOP_INITPROP || op == JSOP_INITLOCKEDPROP ||
      op == JSOP_INITHIDDENPROP) {
    if (!InitPropertyOperation(cx, op, obj, name, rhs)) {
      return false;
    }
  } else if (op == JSOP_SETNAME || op == JSOP_STRICTSETNAME ||
             op == JSOP_SETGNAME || op == JSOP_STRICTSETGNAME) {
    if (!SetNameOperation(cx, script, pc, obj, rhs)) {
      return false;
    }
  } else if (op == JSOP_INITGLEXICAL) {
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
    MOZ_ASSERT(op == JSOP_SETPROP || op == JSOP_STRICTSETPROP);

    ObjectOpResult result;
    if (!SetProperty(cx, obj, id, rhs, lhs, result) ||
        !result.checkStrictErrorOrWarning(cx, obj, id,
                                          op == JSOP_STRICTSETPROP)) {
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
  MOZ_ASSERT(R0 == JSReturnOperand);

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

static bool TryAttachFunApplyStub(JSContext* cx, ICCall_Fallback* stub,
                                  HandleScript script, jsbytecode* pc,
                                  HandleValue thisv, uint32_t argc, Value* argv,
                                  ICTypeMonitor_Fallback* typeMonitorFallback,
                                  bool* attached) {
  if (argc != 2) {
    return true;
  }

  if (!thisv.isObject() || !thisv.toObject().is<JSFunction>()) {
    return true;
  }
  RootedFunction target(cx, &thisv.toObject().as<JSFunction>());

  // right now, only handle situation where second argument is |arguments|
  if (argv[1].isMagic(JS_OPTIMIZED_ARGUMENTS) && !script->needsArgsObj()) {
    if (target->hasJitEntry() &&
        !stub->hasStub(ICStub::Call_ScriptedApplyArguments)) {
      JitSpew(JitSpew_BaselineIC,
              "  Generating Call_ScriptedApplyArguments stub");

      ICCall_ScriptedApplyArguments::Compiler compiler(
          cx, typeMonitorFallback->firstMonitorStub(), script->pcToOffset(pc));
      ICStub* newStub = compiler.getStub(compiler.getStubSpace(script));
      if (!newStub) {
        return false;
      }

      stub->addNewStub(newStub);
      *attached = true;
      return true;
    }

    // TODO: handle FUNAPPLY for native targets.
  }

  if (argv[1].isObject() && argv[1].toObject().is<ArrayObject>()) {
    if (target->hasJitEntry() &&
        !stub->hasStub(ICStub::Call_ScriptedApplyArray)) {
      JitSpew(JitSpew_BaselineIC, "  Generating Call_ScriptedApplyArray stub");

      ICCall_ScriptedApplyArray::Compiler compiler(
          cx, typeMonitorFallback->firstMonitorStub(), script->pcToOffset(pc));
      ICStub* newStub = compiler.getStub(compiler.getStubSpace(script));
      if (!newStub) {
        return false;
      }

      stub->addNewStub(newStub);
      *attached = true;
      return true;
    }
  }
  return true;
}

static bool TryAttachFunCallStub(JSContext* cx, ICCall_Fallback* stub,
                                 HandleScript script, jsbytecode* pc,
                                 HandleValue thisv,
                                 ICTypeMonitor_Fallback* typeMonitorFallback,
                                 bool* attached) {
  // Try to attach a stub for Function.prototype.call with scripted |this|.

  *attached = false;
  if (!thisv.isObject() || !thisv.toObject().is<JSFunction>()) {
    return true;
  }
  RootedFunction target(cx, &thisv.toObject().as<JSFunction>());

  // Attach a stub if the script can be Baseline-compiled. We do this also
  // if the script is not yet compiled to avoid attaching a CallNative stub
  // that handles everything, even after the callee becomes hot.
  if (((target->hasScript() && target->nonLazyScript()->canBaselineCompile()) ||
       (target->isNativeWithJitEntry())) &&
      !stub->hasStub(ICStub::Call_ScriptedFunCall)) {
    JitSpew(JitSpew_BaselineIC, "  Generating Call_ScriptedFunCall stub");

    ICCall_ScriptedFunCall::Compiler compiler(
        cx, typeMonitorFallback->firstMonitorStub(), script->pcToOffset(pc));
    ICStub* newStub = compiler.getStub(compiler.getStubSpace(script));
    if (!newStub) {
      return false;
    }

    *attached = true;
    stub->addNewStub(newStub);
    return true;
  }

  return true;
}

static bool GetTemplateObjectForNative(JSContext* cx, HandleFunction target,
                                       const CallArgs& args,
                                       MutableHandleObject res) {
  if (!target->hasJitInfo() ||
      target->jitInfo()->type() != JSJitInfo::InlinableNative) {
    return true;
  }

  // Check for natives to which template objects can be attached. This is
  // done to provide templates to Ion for inlining these natives later on.
  switch (target->jitInfo()->inlinableNative) {
    case InlinableNative::Array: {
      // Note: the template array won't be used if its length is inaccurately
      // computed here.  (We allocate here because compilation may occur on a
      // separate thread where allocation is impossible.)
      size_t count = 0;
      if (args.length() != 1) {
        count = args.length();
      } else if (args.length() == 1 && args[0].isInt32() &&
                 args[0].toInt32() >= 0) {
        count = args[0].toInt32();
      }

      if (count > ArrayObject::EagerAllocationMaxLength) {
        return true;
      }

      // With this and other array templates, analyze the group so that
      // we don't end up with a template whose structure might change later.
      res.set(NewFullyAllocatedArrayForCallingAllocationSite(cx, count,
                                                             TenuredObject));
      return !!res;
    }

    case InlinableNative::ArraySlice: {
      if (!args.thisv().isObject()) {
        return true;
      }

      RootedObject obj(cx, &args.thisv().toObject());
      if (obj->isSingleton()) {
        return true;
      }

      res.set(NewFullyAllocatedArrayTryReuseGroup(cx, obj, 0, TenuredObject));
      return !!res;
    }

    case InlinableNative::String: {
      RootedString emptyString(cx, cx->runtime()->emptyString);
      res.set(StringObject::create(cx, emptyString, /* proto = */ nullptr,
                                   TenuredObject));
      return !!res;
    }

    case InlinableNative::ObjectCreate: {
      if (args.length() != 1 || !args[0].isObjectOrNull()) {
        return true;
      }
      RootedObject proto(cx, args[0].toObjectOrNull());
      res.set(ObjectCreateImpl(cx, proto, TenuredObject));
      return !!res;
    }

    case InlinableNative::IntrinsicNewArrayIterator: {
      res.set(NewArrayIteratorObject(cx, TenuredObject));
      return !!res;
    }

    case InlinableNative::IntrinsicNewStringIterator: {
      res.set(NewStringIteratorObject(cx, TenuredObject));
      return !!res;
    }

    case InlinableNative::IntrinsicNewRegExpStringIterator: {
      res.set(NewRegExpStringIteratorObject(cx, TenuredObject));
      return !!res;
    }

    case InlinableNative::TypedArrayConstructor: {
      return TypedArrayObject::GetTemplateObjectForNative(cx, target->native(),
                                                          args, res);
    }

    default:
      return true;
  }
}

static bool GetTemplateObjectForClassHook(JSContext* cx, JSNative hook,
                                          CallArgs& args,
                                          MutableHandleObject templateObject) {
  if (args.callee().nonCCWRealm() != cx->realm()) {
    return true;
  }

  if (hook == TypedObject::construct) {
    Rooted<TypeDescr*> descr(cx, &args.callee().as<TypeDescr>());
    templateObject.set(TypedObject::createZeroed(cx, descr, gc::TenuredHeap));
    return !!templateObject;
  }

  return true;
}

static bool TryAttachCallStub(JSContext* cx, ICCall_Fallback* stub,
                              HandleScript script, jsbytecode* pc, JSOp op,
                              uint32_t argc, Value* vp, bool constructing,
                              bool isSpread, bool createSingleton,
                              bool* handled) {
  bool isSuper = op == JSOP_SUPERCALL || op == JSOP_SPREADSUPERCALL;

  if (createSingleton || op == JSOP_EVAL || op == JSOP_STRICTEVAL) {
    return true;
  }

  if (stub->numOptimizedStubs() >= ICCall_Fallback::MAX_OPTIMIZED_STUBS) {
    // TODO: Discard all stubs in this IC and replace with inert megamorphic
    // stub. But for now we just bail.
    return true;
  }

  RootedValue callee(cx, vp[0]);
  RootedValue thisv(cx, vp[1]);

  if (!callee.isObject()) {
    return true;
  }

  ICTypeMonitor_Fallback* typeMonitorFallback =
      stub->getFallbackMonitorStub(cx, script);
  if (!typeMonitorFallback) {
    return false;
  }

  RootedObject obj(cx, &callee.toObject());
  if (!obj->is<JSFunction>()) {
    // Try to attach a stub for a call/construct hook on the object.
    if (JSNative hook = constructing ? obj->constructHook() : obj->callHook()) {
      if (op != JSOP_FUNAPPLY && !isSpread && !createSingleton) {
        RootedObject templateObject(cx);
        CallArgs args = CallArgsFromVp(argc, vp);
        if (!GetTemplateObjectForClassHook(cx, hook, args, &templateObject)) {
          return false;
        }

        JitSpew(JitSpew_BaselineIC, "  Generating Call_ClassHook stub");
        ICCall_ClassHook::Compiler compiler(
            cx, typeMonitorFallback->firstMonitorStub(), obj->getClass(), hook,
            templateObject, script->pcToOffset(pc), constructing);
        ICStub* newStub = compiler.getStub(compiler.getStubSpace(script));
        if (!newStub) {
          return false;
        }

        stub->addNewStub(newStub);
        *handled = true;
        return true;
      }
    }
    return true;
  }

  RootedFunction fun(cx, &obj->as<JSFunction>());

  bool nativeWithJitEntry = fun->isNativeWithJitEntry();
  if (fun->isInterpreted() || nativeWithJitEntry) {
    // Never attach optimized scripted call stubs for JSOP_FUNAPPLY.
    // MagicArguments may escape the frame through them.
    if (op == JSOP_FUNAPPLY) {
      return true;
    }

    // If callee is not an interpreted constructor, we have to throw.
    if (constructing && !fun->isConstructor()) {
      return true;
    }

    // Likewise, if the callee is a class constructor, we have to throw.
    if (!constructing && fun->isClassConstructor()) {
      return true;
    }

    if (!fun->hasJitEntry()) {
      // Don't treat this as an unoptimizable case, as we'll add a stub
      // when the callee is delazified.
      *handled = true;
      return true;
    }

    // If we're constructing, require the callee to have JIT code. This
    // isn't required for correctness but avoids allocating a template
    // object below for constructors that aren't hot. See bug 1419758.
    if (constructing && !fun->hasJITCode()) {
      *handled = true;
      return true;
    }

    // Check if this stub chain has already generalized scripted calls.
    if (stub->scriptedStubsAreGeneralized()) {
      JitSpew(JitSpew_BaselineIC,
              "  Chain already has generalized scripted call stub!");
      return true;
    }

    if (stub->state().mode() == ICState::Mode::Megamorphic) {
      // Create a Call_AnyScripted stub.
      JitSpew(JitSpew_BaselineIC,
              "  Generating Call_AnyScripted stub (cons=%s, spread=%s)",
              constructing ? "yes" : "no", isSpread ? "yes" : "no");
      ICCallScriptedCompiler compiler(
          cx, typeMonitorFallback->firstMonitorStub(), constructing, isSpread,
          script->pcToOffset(pc));
      ICStub* newStub = compiler.getStub(compiler.getStubSpace(script));
      if (!newStub) {
        return false;
      }

      // Before adding new stub, unlink all previous Call_Scripted.
      stub->unlinkStubsWithKind(cx, ICStub::Call_Scripted);

      // Add new generalized stub.
      stub->addNewStub(newStub);
      *handled = true;
      return true;
    }

    // Keep track of the function's |prototype| property in type
    // information, for use during Ion compilation.
    if (IsIonEnabled(cx)) {
      EnsureTrackPropertyTypes(cx, fun, NameToId(cx->names().prototype));
    }

    // Remember the template object associated with any script being called
    // as a constructor, for later use during Ion compilation. This is unsound
    // for super(), as a single callsite can have multiple possible prototype
    // object created (via different newTargets)
    RootedObject templateObject(cx);
    if (constructing && !isSuper) {
      // If we are calling a constructor for which the new script
      // properties analysis has not been performed yet, don't attach a
      // stub. After the analysis is performed, CreateThisForFunction may
      // start returning objects with a different type, and the Ion
      // compiler will get confused.

      // Only attach a stub if the function already has a prototype and
      // we can look it up without causing side effects.
      RootedObject newTarget(cx, &vp[2 + argc].toObject());
      RootedValue protov(cx);
      if (!GetPropertyPure(cx, newTarget, NameToId(cx->names().prototype),
                           protov.address())) {
        JitSpew(JitSpew_BaselineIC, "  Can't purely lookup function prototype");
        return true;
      }

      if (protov.isObject()) {
        AutoRealm ar(cx, fun);
        TaggedProto proto(&protov.toObject());
        ObjectGroup* group =
            ObjectGroup::defaultNewGroup(cx, nullptr, proto, newTarget);
        if (!group) {
          return false;
        }

        AutoSweepObjectGroup sweep(group);
        if (group->newScript(sweep) && !group->newScript(sweep)->analyzed()) {
          JitSpew(JitSpew_BaselineIC,
                  "  Function newScript has not been analyzed");

          // This is temporary until the analysis is perfomed, so
          // don't treat this as unoptimizable.
          *handled = true;
          return true;
        }
      }

      JSObject* thisObject =
          CreateThisForFunction(cx, fun, newTarget, TenuredObject);
      if (!thisObject) {
        return false;
      }

      MOZ_ASSERT(thisObject->nonCCWRealm() == fun->realm());

      if (thisObject->is<PlainObject>()) {
        templateObject = thisObject;
      }
    }

    if (nativeWithJitEntry) {
      JitSpew(JitSpew_BaselineIC,
              "  Generating Call_Scripted stub (native=%p with jit entry, "
              "cons=%s, spread=%s)",
              fun->native(), constructing ? "yes" : "no",
              isSpread ? "yes" : "no");
    } else {
      JitSpew(JitSpew_BaselineIC,
              "  Generating Call_Scripted stub (fun=%p, %s:%u:%u, cons=%s, "
              "spread=%s)",
              fun.get(), fun->nonLazyScript()->filename(),
              fun->nonLazyScript()->lineno(), fun->nonLazyScript()->column(),
              constructing ? "yes" : "no", isSpread ? "yes" : "no");
    }

    bool isCrossRealm = cx->realm() != fun->realm();
    ICCallScriptedCompiler compiler(cx, typeMonitorFallback->firstMonitorStub(),
                                    fun, templateObject, constructing, isSpread,
                                    isCrossRealm, script->pcToOffset(pc));
    ICStub* newStub = compiler.getStub(compiler.getStubSpace(script));
    if (!newStub) {
      return false;
    }

    stub->addNewStub(newStub);
    *handled = true;
    return true;
  }

  if (fun->isNative() &&
      (!constructing || (constructing && fun->isConstructor()))) {
    // Generalized native call stubs are not here yet!
    MOZ_ASSERT(!stub->nativeStubsAreGeneralized());

    // Check for JSOP_FUNAPPLY
    if (op == JSOP_FUNAPPLY) {
      if (fun->native() == fun_apply) {
        return TryAttachFunApplyStub(cx, stub, script, pc, thisv, argc, vp + 2,
                                     typeMonitorFallback, handled);
      }

      // Don't try to attach a "regular" optimized call stubs for FUNAPPLY ops,
      // since MagicArguments may escape through them.
      return true;
    }

    if (op == JSOP_FUNCALL && fun->native() == fun_call) {
      if (!TryAttachFunCallStub(cx, stub, script, pc, thisv,
                                typeMonitorFallback, handled)) {
        return false;
      }
      if (*handled) {
        return true;
      }
    }

    if (stub->state().mode() == ICState::Mode::Megamorphic) {
      JitSpew(JitSpew_BaselineIC,
              "  Megamorphic Call_Native stubs. TODO: add Call_AnyNative!");
      return true;
    }

    bool isCrossRealm = cx->realm() != fun->realm();

    RootedObject templateObject(cx);
    if (MOZ_LIKELY(!isSpread && !isSuper)) {
      CallArgs args = CallArgsFromVp(argc, vp);
      AutoRealm ar(cx, fun);
      if (!GetTemplateObjectForNative(cx, fun, args, &templateObject)) {
        return false;
      }
      MOZ_ASSERT_IF(templateObject,
                    !templateObject->group()
                         ->maybePreliminaryObjectsDontCheckGeneration());
    }

    bool ignoresReturnValue =
        op == JSOP_CALL_IGNORES_RV && fun->isNative() && fun->hasJitInfo() &&
        fun->jitInfo()->type() == JSJitInfo::IgnoresReturnValueNative;

    JitSpew(JitSpew_BaselineIC,
            "  Generating Call_Native stub (fun=%p, cons=%s, spread=%s)",
            fun.get(), constructing ? "yes" : "no", isSpread ? "yes" : "no");
    ICCall_Native::Compiler compiler(
        cx, typeMonitorFallback->firstMonitorStub(), fun, templateObject,
        constructing, ignoresReturnValue, isSpread, isCrossRealm,
        script->pcToOffset(pc));
    ICStub* newStub = compiler.getStub(compiler.getStubSpace(script));
    if (!newStub) {
      return false;
    }

    stub->addNewStub(newStub);
    *handled = true;
    return true;
  }

  return true;
}

bool DoCallFallback(JSContext* cx, BaselineFrame* frame, ICCall_Fallback* stub,
                    uint32_t argc, Value* vp, MutableHandleValue res) {
  stub->incrementEnteredCount();

  RootedScript script(cx, frame->script());
  jsbytecode* pc = stub->icEntry()->pc(script);
  JSOp op = JSOp(*pc);
  FallbackICSpew(cx, stub, "Call(%s)", CodeName[op]);

  MOZ_ASSERT(argc == GET_ARGC(pc));
  bool constructing = (op == JSOP_NEW || op == JSOP_SUPERCALL);
  bool ignoresReturnValue = (op == JSOP_CALL_IGNORES_RV);

  // Ensure vp array is rooted - we may GC in here.
  size_t numValues = argc + 2 + constructing;
  AutoArrayRooter vpRoot(cx, numValues, vp);

  CallArgs callArgs = CallArgsFromSp(argc + constructing, vp + numValues,
                                     constructing, ignoresReturnValue);
  RootedValue callee(cx, vp[0]);
  RootedValue newTarget(cx, constructing ? callArgs.newTarget() : NullValue());

  // Handle funapply with JSOP_ARGUMENTS
  if (op == JSOP_FUNAPPLY && argc == 2 &&
      callArgs[1].isMagic(JS_OPTIMIZED_ARGUMENTS)) {
    if (!GuardFunApplyArgumentsOptimization(cx, frame, callArgs)) {
      return false;
    }
  }

  // Transition stub state to megamorphic or generic if warranted.
  if (stub->state().maybeTransition()) {
    stub->discardStubs(cx);
  }

  bool canAttachStub = stub->state().canAttachStub();
  bool handled = false;
  bool deferred = false;

  // Only bother to try optimizing JSOP_CALL with CacheIR if the chain is still
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

    // Try attaching a regular call stub, but only if the CacheIR attempt didn't
    // add any stubs.
    if (!handled && !deferred && JitOptions.disableCacheIRCalls) {
      bool createSingleton =
          ObjectGroup::useSingletonForNewObject(cx, script, pc);
      if (!TryAttachCallStub(cx, stub, script, pc, op, argc, vp, constructing,
                             false, createSingleton, &handled)) {
        return false;
      }
    }
  }

  if (constructing) {
    if (!ConstructFromStack(cx, callArgs)) {
      return false;
    }
    res.set(callArgs.rval());
  } else if ((op == JSOP_EVAL || op == JSOP_STRICTEVAL) &&
             cx->global()->valueIsEval(callee)) {
    if (!DirectEval(cx, callArgs.get(0), res)) {
      return false;
    }
  } else {
    MOZ_ASSERT(op == JSOP_CALL || op == JSOP_CALL_IGNORES_RV ||
               op == JSOP_CALLITER || op == JSOP_FUNCALL ||
               op == JSOP_FUNAPPLY || op == JSOP_EVAL || op == JSOP_STRICTEVAL);
    if (op == JSOP_CALLITER && callee.isPrimitive()) {
      MOZ_ASSERT(argc == 0, "thisv must be on top of the stack");
      ReportValueError(cx, JSMSG_NOT_ITERABLE, -1, callArgs.thisv(), nullptr);
      return false;
    }

    if (!CallFromStack(cx, callArgs)) {
      return false;
    }

    res.set(callArgs.rval());
  }

  StackTypeSet* types = TypeScript::BytecodeTypes(script, pc);
  TypeScript::Monitor(cx, script, pc, types, res);

  // Add a type monitor stub for the resulting value.
  if (!stub->addMonitorStubForValue(cx, frame, types, res)) {
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
  bool constructing = (op == JSOP_SPREADNEW || op == JSOP_SPREADSUPERCALL);
  FallbackICSpew(cx, stub, "SpreadCall(%s)", CodeName[op]);

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
  if (op != JSOP_SPREADEVAL && op != JSOP_STRICTSPREADEVAL &&
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

    // Try attaching a regular call stub, but only if the CacheIR attempt didn't
    // add any stubs.
    if (!handled && JitOptions.disableCacheIRCalls) {
      if (!TryAttachCallStub(cx, stub, script, pc, op, 1, vp, constructing,
                             true, false, &handled)) {
        return false;
      }
    }
  }

  if (!SpreadCallOperation(cx, script, pc, thisv, callee, arr, newTarget,
                           res)) {
    return false;
  }

  // Add a type monitor stub for the resulting value.
  StackTypeSet* types = TypeScript::BytecodeTypes(script, pc);
  if (!stub->addMonitorStubForValue(cx, frame, types, res)) {
    return false;
  }

  return true;
}

void ICStubCompilerBase::pushCallArguments(MacroAssembler& masm,
                                           AllocatableGeneralRegisterSet regs,
                                           Register argcReg, bool isJitCall,
                                           bool isConstructing) {
  MOZ_ASSERT(!regs.has(argcReg));

  // Account for new.target
  Register count = regs.takeAny();

  masm.move32(argcReg, count);

  // If we are setting up for a jitcall, we have to align the stack taking
  // into account the args and newTarget. We could also count callee and |this|,
  // but it's a waste of stack space. Because we want to keep argcReg unchanged,
  // just account for newTarget initially, and add the other 2 after assuring
  // allignment.
  if (isJitCall) {
    if (isConstructing) {
      masm.add32(Imm32(1), count);
    }
  } else {
    masm.add32(Imm32(2 + isConstructing), count);
  }

  // argPtr initially points to the last argument.
  Register argPtr = regs.takeAny();
  masm.moveStackPtrTo(argPtr);

  // Skip 4 pointers pushed on top of the arguments: the frame descriptor,
  // return address, old frame pointer and stub reg.
  masm.addPtr(Imm32(STUB_FRAME_SIZE), argPtr);

  // Align the stack such that the JitFrameLayout is aligned on the
  // JitStackAlignment.
  if (isJitCall) {
    masm.alignJitStackBasedOnNArgs(count, /*countIncludesThis =*/false);

    // Account for callee and |this|, skipped earlier
    masm.add32(Imm32(2), count);
  }

  // Push all values, starting at the last one.
  Label loop, done;
  masm.bind(&loop);
  masm.branchTest32(Assembler::Zero, count, count, &done);
  {
    masm.pushValue(Address(argPtr, 0));
    masm.addPtr(Imm32(sizeof(Value)), argPtr);

    masm.sub32(Imm32(1), count);
    masm.jump(&loop);
  }
  masm.bind(&done);
}

void ICCallStubCompiler::guardSpreadCall(MacroAssembler& masm, Register argcReg,
                                         Label* failure, bool isConstructing) {
  masm.unboxObject(Address(masm.getStackPointer(),
                           isConstructing * sizeof(Value) + ICStackValueOffset),
                   argcReg);
  masm.loadPtr(Address(argcReg, NativeObject::offsetOfElements()), argcReg);
  masm.load32(Address(argcReg, ObjectElements::offsetOfLength()), argcReg);

  // Limit actual argc to something reasonable (huge number of arguments can
  // blow the stack limit).
  static_assert(ICCall_Scripted::MAX_ARGS_SPREAD_LENGTH <= ARGS_LENGTH_MAX,
                "maximum arguments length for optimized stub should be <= "
                "ARGS_LENGTH_MAX");
  masm.branch32(Assembler::Above, argcReg,
                Imm32(ICCall_Scripted::MAX_ARGS_SPREAD_LENGTH), failure);
}

void ICCallStubCompiler::pushSpreadCallArguments(
    MacroAssembler& masm, AllocatableGeneralRegisterSet regs, Register argcReg,
    bool isJitCall, bool isConstructing) {
  // Pull the array off the stack before aligning.
  Register startReg = regs.takeAny();
  masm.unboxObject(Address(masm.getStackPointer(),
                           (isConstructing * sizeof(Value)) + STUB_FRAME_SIZE),
                   startReg);
  masm.loadPtr(Address(startReg, NativeObject::offsetOfElements()), startReg);

  // Align the stack such that the JitFrameLayout is aligned on the
  // JitStackAlignment.
  if (isJitCall) {
    Register alignReg = argcReg;
    if (isConstructing) {
      alignReg = regs.takeAny();
      masm.movePtr(argcReg, alignReg);
      masm.addPtr(Imm32(1), alignReg);
    }
    masm.alignJitStackBasedOnNArgs(alignReg, /*countIncludesThis =*/false);
    if (isConstructing) {
      MOZ_ASSERT(alignReg != argcReg);
      regs.add(alignReg);
    }
  }

  // Push newTarget, if necessary
  if (isConstructing) {
    masm.pushValue(Address(BaselineFrameReg, STUB_FRAME_SIZE));
  }

  // Push arguments: set up endReg to point to &array[argc]
  Register endReg = regs.takeAny();
  masm.movePtr(argcReg, endReg);
  static_assert(sizeof(Value) == 8, "Value must be 8 bytes");
  masm.lshiftPtr(Imm32(3), endReg);
  masm.addPtr(startReg, endReg);

  // Copying pre-decrements endReg by 8 until startReg is reached
  Label copyDone;
  Label copyStart;
  masm.bind(&copyStart);
  masm.branchPtr(Assembler::Equal, endReg, startReg, &copyDone);
  masm.subPtr(Imm32(sizeof(Value)), endReg);
  masm.pushValue(Address(endReg, 0));
  masm.jump(&copyStart);
  masm.bind(&copyDone);

  regs.add(startReg);
  regs.add(endReg);

  // Push the callee and |this|.
  masm.pushValue(
      Address(BaselineFrameReg,
              STUB_FRAME_SIZE + (1 + isConstructing) * sizeof(Value)));
  masm.pushValue(
      Address(BaselineFrameReg,
              STUB_FRAME_SIZE + (2 + isConstructing) * sizeof(Value)));
}

Register ICCallStubCompiler::guardFunApply(MacroAssembler& masm,
                                           AllocatableGeneralRegisterSet regs,
                                           Register argcReg,
                                           FunApplyThing applyThing,
                                           Label* failure) {
  // Ensure argc == 2
  masm.branch32(Assembler::NotEqual, argcReg, Imm32(2), failure);

  // Stack looks like:
  //      [..., CalleeV, ThisV, Arg0V, Arg1V <MaybeReturnReg>]

  Address secondArgSlot(masm.getStackPointer(), ICStackValueOffset);
  if (applyThing == FunApply_MagicArgs) {
    // Ensure that the second arg is magic arguments.
    masm.branchTestMagic(Assembler::NotEqual, secondArgSlot, failure);

    // Ensure that this frame doesn't have an arguments object.
    masm.branchTest32(
        Assembler::NonZero,
        Address(BaselineFrameReg, BaselineFrame::reverseOffsetOfFlags()),
        Imm32(BaselineFrame::HAS_ARGS_OBJ), failure);

    // Limit the length to something reasonable.
    masm.branch32(
        Assembler::Above,
        Address(BaselineFrameReg, BaselineFrame::offsetOfNumActualArgs()),
        Imm32(ICCall_ScriptedApplyArray::MAX_ARGS_ARRAY_LENGTH), failure);
  } else {
    MOZ_ASSERT(applyThing == FunApply_Array);

    AllocatableGeneralRegisterSet regsx = regs;

    // Ensure that the second arg is an array.
    ValueOperand secondArgVal = regsx.takeAnyValue();
    masm.loadValue(secondArgSlot, secondArgVal);

    masm.branchTestObject(Assembler::NotEqual, secondArgVal, failure);
    Register secondArgObj = masm.extractObject(secondArgVal, ExtractTemp1);

    regsx.add(secondArgVal);
    regsx.takeUnchecked(secondArgObj);

    masm.branchTestObjClass(Assembler::NotEqual, secondArgObj,
                            &ArrayObject::class_, regsx.getAny(), secondArgObj,
                            failure);

    // Get the array elements and ensure that initializedLength == length
    masm.loadPtr(Address(secondArgObj, NativeObject::offsetOfElements()),
                 secondArgObj);

    Register lenReg = regsx.takeAny();
    masm.load32(Address(secondArgObj, ObjectElements::offsetOfLength()),
                lenReg);

    masm.branch32(
        Assembler::NotEqual,
        Address(secondArgObj, ObjectElements::offsetOfInitializedLength()),
        lenReg, failure);

    // Limit the length to something reasonable (huge number of arguments can
    // blow the stack limit).
    masm.branch32(Assembler::Above, lenReg,
                  Imm32(ICCall_ScriptedApplyArray::MAX_ARGS_ARRAY_LENGTH),
                  failure);

    // Ensure no holes.  Loop through values in array and make sure none are
    // magic. Start address is secondArgObj, end address is secondArgObj +
    // (lenReg * sizeof(Value))
    static_assert(sizeof(Value) == 8,
                  "shift by 3 below assumes Value is 8 bytes");
    masm.lshiftPtr(Imm32(3), lenReg);
    masm.addPtr(secondArgObj, lenReg);

    Register start = secondArgObj;
    Register end = lenReg;
    Label loop;
    Label endLoop;
    masm.bind(&loop);
    masm.branchPtr(Assembler::AboveOrEqual, start, end, &endLoop);
    masm.branchTestMagic(Assembler::Equal, Address(start, 0), failure);
    masm.addPtr(Imm32(sizeof(Value)), start);
    masm.jump(&loop);
    masm.bind(&endLoop);
  }

  // Stack now confirmed to be like:
  //      [..., CalleeV, ThisV, Arg0V, MagicValue(Arguments), <MaybeReturnAddr>]

  // Load the callee, ensure that it's fun_apply
  ValueOperand val = regs.takeAnyValue();
  Address calleeSlot(masm.getStackPointer(),
                     ICStackValueOffset + (3 * sizeof(Value)));
  masm.loadValue(calleeSlot, val);

  masm.branchTestObject(Assembler::NotEqual, val, failure);
  Register callee = masm.extractObject(val, ExtractTemp1);

  masm.branchTestObjClass(Assembler::NotEqual, callee, &JSFunction::class_,
                          regs.getAny(), callee, failure);
  masm.loadPtr(Address(callee, JSFunction::offsetOfNativeOrEnv()), callee);

  masm.branchPtr(Assembler::NotEqual, callee, ImmPtr(fun_apply), failure);

  // Load the |thisv|, ensure that it's a scripted function with a valid
  // baseline or ion script, or a native function.
  Address thisSlot(masm.getStackPointer(),
                   ICStackValueOffset + (2 * sizeof(Value)));
  masm.loadValue(thisSlot, val);

  masm.branchTestObject(Assembler::NotEqual, val, failure);
  Register target = masm.extractObject(val, ExtractTemp1);
  regs.add(val);
  regs.takeUnchecked(target);

  masm.branchTestObjClass(Assembler::NotEqual, target, &JSFunction::class_,
                          regs.getAny(), target, failure);

  Register temp = regs.takeAny();
  masm.branchIfFunctionHasNoJitEntry(target, /* constructing */ false, failure);
  masm.branchFunctionKind(Assembler::Equal, JSFunction::ClassConstructor,
                          callee, temp, failure);
  regs.add(temp);
  return target;
}

void ICCallStubCompiler::pushCallerArguments(
    MacroAssembler& masm, AllocatableGeneralRegisterSet regs) {
  // Initialize copyReg to point to start caller arguments vector.
  // Initialize argcReg to poitn to the end of it.
  Register startReg = regs.takeAny();
  Register endReg = regs.takeAny();
  masm.loadPtr(Address(BaselineFrameReg, 0), startReg);
  masm.loadPtr(Address(startReg, BaselineFrame::offsetOfNumActualArgs()),
               endReg);
  masm.addPtr(Imm32(BaselineFrame::offsetOfArg(0)), startReg);
  masm.alignJitStackBasedOnNArgs(endReg, /*countIncludesThis =*/false);
  masm.lshiftPtr(Imm32(ValueShift), endReg);
  masm.addPtr(startReg, endReg);

  // Copying pre-decrements endReg by 8 until startReg is reached
  Label copyDone;
  Label copyStart;
  masm.bind(&copyStart);
  masm.branchPtr(Assembler::Equal, endReg, startReg, &copyDone);
  masm.subPtr(Imm32(sizeof(Value)), endReg);
  masm.pushValue(Address(endReg, 0));
  masm.jump(&copyStart);
  masm.bind(&copyDone);
}

void ICCallStubCompiler::pushArrayArguments(
    MacroAssembler& masm, Address arrayVal,
    AllocatableGeneralRegisterSet regs) {
  // Load start and end address of values to copy.
  // guardFunApply has already gauranteed that the array is packed and contains
  // no holes.
  Register startReg = regs.takeAny();
  Register endReg = regs.takeAny();
  masm.unboxObject(arrayVal, startReg);
  masm.loadPtr(Address(startReg, NativeObject::offsetOfElements()), startReg);
  masm.load32(Address(startReg, ObjectElements::offsetOfInitializedLength()),
              endReg);
  masm.alignJitStackBasedOnNArgs(endReg, /*countIncludesThis =*/false);
  masm.lshiftPtr(Imm32(ValueShift), endReg);
  masm.addPtr(startReg, endReg);

  // Copying pre-decrements endReg by 8 until startReg is reached
  Label copyDone;
  Label copyStart;
  masm.bind(&copyStart);
  masm.branchPtr(Assembler::Equal, endReg, startReg, &copyDone);
  masm.subPtr(Imm32(sizeof(Value)), endReg);
  masm.pushValue(Address(endReg, 0));
  masm.jump(&copyStart);
  masm.bind(&copyDone);
}

bool FallbackICCodeCompiler::emitCall(bool isSpread, bool isConstructing) {
  MOZ_ASSERT(R0 == JSReturnOperand);

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

    // SPREADCALL is not yet supported in Ion, so do not generate asmcode for
    // bailout.
    return true;
  }

  // Push a stub frame so that we can perform a non-tail call.
  enterStubFrame(masm, R1.scratchReg());

  regs.take(R0.scratchReg());  // argc.

  pushCallArguments(masm, regs, R0.scratchReg(), /* isJitCall = */ false,
                    isConstructing);

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
    MOZ_ASSERT(JSReturnOperand == R0);
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

bool ICCallScriptedCompiler::generateStubCode(MacroAssembler& masm) {
  Label failure;
  AllocatableGeneralRegisterSet regs(availableGeneralRegs(0));
  bool canUseTailCallReg = regs.has(ICTailCallReg);

  Register argcReg = R0.scratchReg();
  regs.take(argcReg);
  regs.takeUnchecked(ICTailCallReg);

  if (isSpread_) {
    guardSpreadCall(masm, argcReg, &failure, isConstructing_);
  }

  // Load the callee in R1, accounting for newTarget, if necessary
  // Stack Layout:
  //      [ ..., CalleeVal, ThisVal, Arg0Val, ..., ArgNVal, [newTarget],
  //        +ICStackValueOffset+ ]
  if (isSpread_) {
    unsigned skipToCallee = (2 + isConstructing_) * sizeof(Value);
    masm.loadValue(
        Address(masm.getStackPointer(), skipToCallee + ICStackValueOffset), R1);
  } else {
    // Account for newTarget, if necessary
    unsigned nonArgsSkip = (1 + isConstructing_) * sizeof(Value);
    BaseValueIndex calleeSlot(masm.getStackPointer(), argcReg,
                              ICStackValueOffset + nonArgsSkip);
    masm.loadValue(calleeSlot, R1);
  }
  regs.take(R1);

  // Ensure callee is an object.
  masm.branchTestObject(Assembler::NotEqual, R1, &failure);

  // Ensure callee is a function.
  Register callee = masm.extractObject(R1, ExtractTemp0);

  // If calling a specific script, check if the script matches.  Otherwise,
  // ensure that callee function is scripted.  Leave calleeScript in |callee|
  // reg.
  if (callee_) {
    MOZ_ASSERT(kind == ICStub::Call_Scripted);

    // Check if the object matches this callee.
    Address expectedCallee(ICStubReg, ICCall_Scripted::offsetOfCallee());
    masm.branchPtr(Assembler::NotEqual, expectedCallee, callee, &failure);

    // Guard against relazification.
    masm.branchIfFunctionHasNoJitEntry(callee, isConstructing_, &failure);
  } else {
    // Ensure the object is a function.
    masm.branchTestObjClass(Assembler::NotEqual, callee, &JSFunction::class_,
                            regs.getAny(), callee, &failure);
    if (isConstructing_) {
      masm.branchIfNotInterpretedConstructor(callee, regs.getAny(), &failure);
    } else {
      masm.branchIfFunctionHasNoJitEntry(callee, /* constructing */ false,
                                         &failure);
      masm.branchFunctionKind(Assembler::Equal, JSFunction::ClassConstructor,
                              callee, regs.getAny(), &failure);
    }
  }

  // Load the start of the target JitCode.
  Register code;
  if (!isConstructing_) {
    code = regs.takeAny();
    masm.loadJitCodeRaw(callee, code);
  }

  // We no longer need R1.
  regs.add(R1);

  // Push a stub frame so that we can perform a non-tail call.
  enterStubFrame(masm, regs.getAny());
  if (canUseTailCallReg) {
    regs.add(ICTailCallReg);
  }

  if (maybeCrossRealm_) {
    masm.switchToObjectRealm(callee, regs.getAny());
  }

  if (isConstructing_) {
    // Save argc before call.
    masm.push(argcReg);

    // Stack now looks like:
    //      [ ..., Callee, ThisV, Arg0V, ..., ArgNV, NewTarget,
    //        StubFrameHeader, ArgC ]
    masm.loadValue(
        Address(masm.getStackPointer(), STUB_FRAME_SIZE + sizeof(size_t)), R1);
    masm.push(masm.extractObject(R1, ExtractTemp0));

    if (isSpread_) {
      masm.loadValue(Address(masm.getStackPointer(),
                             3 * sizeof(Value) + STUB_FRAME_SIZE +
                                 sizeof(size_t) + sizeof(JSObject*)),
                     R1);
    } else {
      BaseValueIndex calleeSlot2(masm.getStackPointer(), argcReg,
                                 2 * sizeof(Value) + STUB_FRAME_SIZE +
                                     sizeof(size_t) + sizeof(JSObject*));
      masm.loadValue(calleeSlot2, R1);
    }
    masm.push(masm.extractObject(R1, ExtractTemp0));

    using Fn = bool (*)(JSContext * cx, HandleObject callee,
                        HandleObject newTarget, MutableHandleValue rval);
    if (!callVM<Fn, CreateThis>(masm)) {
      return false;
    }

    // Return of CreateThis must be an object or uninitialized.
#ifdef DEBUG
    Label createdThisOK;
    masm.branchTestObject(Assembler::Equal, JSReturnOperand, &createdThisOK);
    masm.branchTestMagic(Assembler::Equal, JSReturnOperand, &createdThisOK);
    masm.assumeUnreachable(
        "The return of CreateThis must be an object or uninitialized.");
    masm.bind(&createdThisOK);
#endif

    // Reset the register set from here on in.
    static_assert(JSReturnOperand == R0, "The code below needs to be adapted.");
    regs = availableGeneralRegs(0);
    regs.take(R0);
    argcReg = regs.takeAny();

    // Restore saved argc so we can use it to calculate the address to save
    // the resulting this object to.
    masm.pop(argcReg);

    // Save "this" value back into pushed arguments on stack. R0 can be
    // clobbered after that.
    //
    // Stack now looks like:
    //      [ ..., Callee, ThisV, Arg0V, ..., ArgNV, [NewTarget],
    //        StubFrameHeader ]
    if (isSpread_) {
      masm.storeValue(
          R0, Address(masm.getStackPointer(),
                      (1 + isConstructing_) * sizeof(Value) + STUB_FRAME_SIZE));
    } else {
      BaseValueIndex thisSlot(
          masm.getStackPointer(), argcReg,
          STUB_FRAME_SIZE + isConstructing_ * sizeof(Value));
      masm.storeValue(R0, thisSlot);
    }

    // Restore the stub register from the baseline stub frame.
    masm.loadPtr(Address(masm.getStackPointer(), STUB_FRAME_SAVED_STUB_OFFSET),
                 ICStubReg);

    // Reload callee script. Note that a GC triggered by CreateThis may
    // have destroyed the callee BaselineScript and IonScript. CreateThis
    // is safely repeatable though, so in this case we just leave the stub
    // frame and jump to the next stub.

    // Just need to load the script now.
    if (isSpread_) {
      unsigned skipForCallee = (2 + isConstructing_) * sizeof(Value);
      masm.loadValue(
          Address(masm.getStackPointer(), skipForCallee + STUB_FRAME_SIZE), R0);
    } else {
      // Account for newTarget, if necessary
      unsigned nonArgsSkip = (1 + isConstructing_) * sizeof(Value);
      BaseValueIndex calleeSlot3(masm.getStackPointer(), argcReg,
                                 nonArgsSkip + STUB_FRAME_SIZE);
      masm.loadValue(calleeSlot3, R0);
    }
    callee = masm.extractObject(R0, ExtractTemp0);
    regs.add(R0);
    regs.takeUnchecked(callee);

    code = regs.takeAny();
    masm.loadJitCodeRaw(callee, code);

    // Release callee register, but don't add ExtractTemp0 back into the pool
    // ExtractTemp0 is used later, and if it's allocated to some other register
    // at that point, it will get clobbered when used.
    if (callee != ExtractTemp0) {
      regs.add(callee);
    }

    if (canUseTailCallReg) {
      regs.addUnchecked(ICTailCallReg);
    }
  }
  Register scratch = regs.takeAny();

  // Values are on the stack left-to-right. Calling convention wants them
  // right-to-left so duplicate them on the stack in reverse order.
  // |this| and callee are pushed last.
  if (isSpread_) {
    pushSpreadCallArguments(masm, regs, argcReg, /* isJitCall = */ true,
                            isConstructing_);
  } else {
    pushCallArguments(masm, regs, argcReg, /* isJitCall = */ true,
                      isConstructing_);
  }

  // The callee is on top of the stack. Pop and unbox it.
  ValueOperand val = regs.takeAnyValue();
  masm.popValue(val);
  callee = masm.extractObject(val, ExtractTemp0);

  EmitBaselineCreateStubFrameDescriptor(masm, scratch, JitFrameLayout::Size());

  // Note that we use Push, not push, so that callJit will align the stack
  // properly on ARM.
  masm.Push(argcReg);
  masm.PushCalleeToken(callee, isConstructing_);
  masm.Push(scratch);

  // Handle arguments underflow.
  Label noUnderflow;
  masm.load16ZeroExtend(Address(callee, JSFunction::offsetOfNargs()), callee);
  masm.branch32(Assembler::AboveOrEqual, argcReg, callee, &noUnderflow);
  {
    // Call the arguments rectifier.
    TrampolinePtr argumentsRectifier =
        cx->runtime()->jitRuntime()->getArgumentsRectifier();
    masm.movePtr(argumentsRectifier, code);
  }

  masm.bind(&noUnderflow);
  masm.callJit(code);

  // If this is a constructing call, and the callee returns a non-object,
  // replace it with the |this| object passed in.
  if (isConstructing_) {
    Label skipThisReplace;
    masm.branchTestObject(Assembler::Equal, JSReturnOperand, &skipThisReplace);

    // Current stack: [ Padding?, ARGVALS..., ThisVal, ActualArgc, Callee,
    //                  Descriptor ]
    // However, we can't use this ThisVal, because it hasn't been traced.
    // We need to use the ThisVal higher up the stack:
    // Current stack: [ ThisVal, ARGVALS..., ...STUB FRAME..., Padding?,
    //                  ARGVALS..., ThisVal, ActualArgc, Callee, Descriptor ]

    // Restore the BaselineFrameReg based on the frame descriptor.
    //
    // BaselineFrameReg = BaselineStackReg
    //                  + sizeof(Descriptor)
    //                  + sizeof(Callee)
    //                  + sizeof(ActualArgc)
    //                  + stubFrameSize(Descriptor)
    //                  - sizeof(ICStubReg)
    //                  - sizeof(BaselineFrameReg)
    Address descriptorAddr(masm.getStackPointer(), 0);
    masm.loadPtr(descriptorAddr, BaselineFrameReg);
    masm.rshiftPtr(Imm32(FRAMESIZE_SHIFT), BaselineFrameReg);
    masm.addPtr(Imm32((3 - 2) * sizeof(size_t)), BaselineFrameReg);
    masm.addStackPtrTo(BaselineFrameReg);

    // Load the number of arguments present before the stub frame.
    Register argcReg = JSReturnOperand.scratchReg();
    if (isSpread_) {
      // Account for the Array object.
      masm.move32(Imm32(1), argcReg);
    } else {
      Address argcAddr(masm.getStackPointer(), 2 * sizeof(size_t));
      masm.loadPtr(argcAddr, argcReg);
    }

    // Current stack:
    //      [ ThisVal, ARGVALS..., ...STUB FRAME..., <-- BaselineFrameReg
    //        Padding?, ARGVALS..., ThisVal, ActualArgc, Callee, Descriptor ]
    //
    // &ThisVal = BaselineFrameReg + argc * sizeof(Value) + STUB_FRAME_SIZE +
    // sizeof(Value) This last sizeof(Value) accounts for the newTarget on the
    // end of the arguments vector which is not reflected in actualArgc
    BaseValueIndex thisSlotAddr(BaselineFrameReg, argcReg,
                                STUB_FRAME_SIZE + sizeof(Value));
    masm.loadValue(thisSlotAddr, JSReturnOperand);
#ifdef DEBUG
    masm.branchTestObject(Assembler::Equal, JSReturnOperand, &skipThisReplace);
    masm.assumeUnreachable("Return of constructing call should be an object.");
#endif
    masm.bind(&skipThisReplace);
  }

  leaveStubFrame(masm, true);

  if (maybeCrossRealm_) {
    masm.switchToBaselineFrameRealm(R1.scratchReg());
  }

  // Enter type monitor IC to type-check result.
  EmitEnterTypeMonitorIC(masm);

  masm.bind(&failure);
  EmitStubGuardFailure(masm);
  return true;
}

bool ICCall_Native::Compiler::generateStubCode(MacroAssembler& masm) {
  Label failure;
  AllocatableGeneralRegisterSet regs(availableGeneralRegs(0));

  Register argcReg = R0.scratchReg();
  regs.take(argcReg);
  regs.takeUnchecked(ICTailCallReg);

  if (isSpread_) {
    guardSpreadCall(masm, argcReg, &failure, isConstructing_);
  }

  // Load the callee in R1.
  if (isSpread_) {
    unsigned skipToCallee = (2 + isConstructing_) * sizeof(Value);
    masm.loadValue(
        Address(masm.getStackPointer(), skipToCallee + ICStackValueOffset), R1);
  } else {
    unsigned nonArgsSlots = (1 + isConstructing_) * sizeof(Value);
    BaseValueIndex calleeSlot(masm.getStackPointer(), argcReg,
                              ICStackValueOffset + nonArgsSlots);
    masm.loadValue(calleeSlot, R1);
  }
  regs.take(R1);

  masm.branchTestObject(Assembler::NotEqual, R1, &failure);

  // Ensure callee matches this stub's callee.
  Register callee = masm.extractObject(R1, ExtractTemp0);
  Address expectedCallee(ICStubReg, ICCall_Native::offsetOfCallee());
  masm.branchPtr(Assembler::NotEqual, expectedCallee, callee, &failure);

  regs.add(R1);
  regs.takeUnchecked(callee);

  // Push a stub frame so that we can perform a non-tail call.
  // Note that this leaves the return address in TailCallReg.
  enterStubFrame(masm, regs.getAny());

  if (isCrossRealm_) {
    masm.switchToObjectRealm(callee, regs.getAny());
  }

  // Values are on the stack left-to-right. Calling convention wants them
  // right-to-left so duplicate them on the stack in reverse order.
  // |this| and callee are pushed last.
  if (isSpread_) {
    pushSpreadCallArguments(masm, regs, argcReg, /* isJitCall = */ false,
                            isConstructing_);
  } else {
    pushCallArguments(masm, regs, argcReg, /* isJitCall = */ false,
                      isConstructing_);
  }

  // Native functions have the signature:
  //
  //    bool (*)(JSContext*, unsigned, Value* vp)
  //
  // Where vp[0] is space for callee/return value, vp[1] is |this|, and vp[2]
  // onward are the function arguments.

  // Initialize vp.
  Register vpReg = regs.takeAny();
  masm.moveStackPtrTo(vpReg);

  // Construct a native exit frame.
  masm.push(argcReg);

  Register scratch = regs.takeAny();
  EmitBaselineCreateStubFrameDescriptor(masm, scratch, ExitFrameLayout::Size());
  masm.push(scratch);
  masm.push(ICTailCallReg);
  masm.loadJSContext(scratch);
  masm.enterFakeExitFrameForNative(scratch, scratch, isConstructing_);

  // Execute call.
  masm.setupUnalignedABICall(scratch);
  masm.loadJSContext(scratch);
  masm.passABIArg(scratch);
  masm.passABIArg(argcReg);
  masm.passABIArg(vpReg);

#ifdef JS_SIMULATOR
  // The simulator requires VM calls to be redirected to a special swi
  // instruction to handle them, so we store the redirected pointer in the
  // stub and use that instead of the original one.
  masm.callWithABI(Address(ICStubReg, ICCall_Native::offsetOfNative()));
#else
  if (ignoresReturnValue_) {
    MOZ_ASSERT(callee_->hasJitInfo());
    masm.loadPtr(Address(callee, JSFunction::offsetOfJitInfo()), callee);
    masm.callWithABI(
        Address(callee, JSJitInfo::offsetOfIgnoresReturnValueNative()));
  } else {
    masm.callWithABI(Address(callee, JSFunction::offsetOfNative()));
  }
#endif

  // Test for failure.
  masm.branchIfFalseBool(ReturnReg, masm.exceptionLabel());

  // Load the return value into R0.
  masm.loadValue(
      Address(masm.getStackPointer(), NativeExitFrameLayout::offsetOfResult()),
      R0);

  leaveStubFrame(masm);

  if (isCrossRealm_) {
    masm.switchToBaselineFrameRealm(R1.scratchReg());
  }

  // Enter type monitor IC to type-check result.
  EmitEnterTypeMonitorIC(masm);

  masm.bind(&failure);
  EmitStubGuardFailure(masm);
  return true;
}

bool ICCall_ClassHook::Compiler::generateStubCode(MacroAssembler& masm) {
  Label failure;
  AllocatableGeneralRegisterSet regs(availableGeneralRegs(0));

  Register argcReg = R0.scratchReg();
  regs.take(argcReg);
  regs.takeUnchecked(ICTailCallReg);

  // Load the callee in R1.
  unsigned nonArgSlots = (1 + isConstructing_) * sizeof(Value);
  BaseValueIndex calleeSlot(masm.getStackPointer(), argcReg,
                            ICStackValueOffset + nonArgSlots);
  masm.loadValue(calleeSlot, R1);
  regs.take(R1);

  masm.branchTestObject(Assembler::NotEqual, R1, &failure);

  // Ensure the callee's class matches the one in this stub.
  // We use |Address(ICStubReg, ICCall_ClassHook::offsetOfNative())| below
  // instead of extracting the hook from callee. As a result the callee
  // register is no longer used and we must use spectreRegToZero := ICStubReg
  // instead.
  Register callee = masm.extractObject(R1, ExtractTemp0);
  Register scratch = regs.takeAny();
  masm.branchTestObjClass(Assembler::NotEqual, callee,
                          Address(ICStubReg, ICCall_ClassHook::offsetOfClass()),
                          scratch, ICStubReg, &failure);
  regs.add(R1);
  regs.takeUnchecked(callee);

  // Push a stub frame so that we can perform a non-tail call.
  // Note that this leaves the return address in TailCallReg.
  enterStubFrame(masm, regs.getAny());

  masm.switchToObjectRealm(callee, regs.getAny());

  regs.add(scratch);
  pushCallArguments(masm, regs, argcReg, /* isJitCall = */ false,
                    isConstructing_);
  regs.take(scratch);

  masm.assertStackAlignment(sizeof(Value), 0);

  // Native functions have the signature:
  //
  //    bool (*)(JSContext*, unsigned, Value* vp)
  //
  // Where vp[0] is space for callee/return value, vp[1] is |this|, and vp[2]
  // onward are the function arguments.

  // Initialize vp.
  Register vpReg = regs.takeAny();
  masm.moveStackPtrTo(vpReg);

  // Construct a native exit frame.
  masm.push(argcReg);

  EmitBaselineCreateStubFrameDescriptor(masm, scratch, ExitFrameLayout::Size());
  masm.push(scratch);
  masm.push(ICTailCallReg);
  masm.loadJSContext(scratch);
  masm.enterFakeExitFrameForNative(scratch, scratch, isConstructing_);

  // Execute call.
  masm.setupUnalignedABICall(scratch);
  masm.loadJSContext(scratch);
  masm.passABIArg(scratch);
  masm.passABIArg(argcReg);
  masm.passABIArg(vpReg);
  masm.callWithABI(Address(ICStubReg, ICCall_ClassHook::offsetOfNative()));

  // Test for failure.
  masm.branchIfFalseBool(ReturnReg, masm.exceptionLabel());

  // Load the return value into R0.
  masm.loadValue(
      Address(masm.getStackPointer(), NativeExitFrameLayout::offsetOfResult()),
      R0);

  leaveStubFrame(masm);

  masm.switchToBaselineFrameRealm(R1.scratchReg());

  // Enter type monitor IC to type-check result.
  EmitEnterTypeMonitorIC(masm);

  masm.bind(&failure);
  EmitStubGuardFailure(masm);
  return true;
}

bool ICCall_ScriptedApplyArray::Compiler::generateStubCode(
    MacroAssembler& masm) {
  Label failure;
  AllocatableGeneralRegisterSet regs(availableGeneralRegs(0));

  Register argcReg = R0.scratchReg();
  regs.take(argcReg);
  regs.takeUnchecked(ICTailCallReg);

  //
  // Validate inputs
  //

  Register target =
      guardFunApply(masm, regs, argcReg, FunApply_Array, &failure);
  if (regs.has(target)) {
    regs.take(target);
  } else {
    // If target is already a reserved reg, take another register for it,
    // because it's probably currently an ExtractTemp, which might get clobbered
    // later.
    Register targetTemp = regs.takeAny();
    masm.movePtr(target, targetTemp);
    target = targetTemp;
  }

  // Push a stub frame so that we can perform a non-tail call.
  enterStubFrame(masm, regs.getAny());

  //
  // Push arguments
  //

  // Stack now looks like:
  //                                      BaselineFrameReg -------------------.
  //                                                                          v
  //      [..., fun_apply, TargetV, TargetThisV, ArgsArrayV, StubFrameHeader]

  // Push all array elements onto the stack:
  Address arrayVal(BaselineFrameReg, STUB_FRAME_SIZE);
  pushArrayArguments(masm, arrayVal, regs);

  // Stack now looks like:
  //                                      BaselineFrameReg -------------------.
  //                                                                          v
  //      [..., fun_apply, TargetV, TargetThisV, ArgsArrayV, StubFrameHeader,
  //       PushedArgN, ..., PushedArg0]
  // Can't fail after this, so it's ok to clobber argcReg.

  // Push actual argument 0 as |thisv| for call.
  masm.pushValue(Address(BaselineFrameReg, STUB_FRAME_SIZE + sizeof(Value)));

  // All pushes after this use Push instead of push to make sure ARM can align
  // stack properly for call.
  Register scratch = regs.takeAny();
  EmitBaselineCreateStubFrameDescriptor(masm, scratch, JitFrameLayout::Size());

  // Reload argc from length of array.
  masm.unboxObject(arrayVal, argcReg);
  masm.loadPtr(Address(argcReg, NativeObject::offsetOfElements()), argcReg);
  masm.load32(Address(argcReg, ObjectElements::offsetOfInitializedLength()),
              argcReg);

  masm.Push(argcReg);
  masm.Push(target);
  masm.Push(scratch);

  masm.switchToObjectRealm(target, scratch);

  // Load nargs into scratch for underflow check, and then load jitcode pointer
  // into target.
  masm.load16ZeroExtend(Address(target, JSFunction::offsetOfNargs()), scratch);
  masm.loadJitCodeRaw(target, target);

  // Handle arguments underflow.
  Label noUnderflow;
  masm.branch32(Assembler::AboveOrEqual, argcReg, scratch, &noUnderflow);
  {
    // Call the arguments rectifier.
    TrampolinePtr argumentsRectifier =
        cx->runtime()->jitRuntime()->getArgumentsRectifier();
    masm.movePtr(argumentsRectifier, target);
  }
  masm.bind(&noUnderflow);
  regs.add(argcReg);

  // Do call.
  masm.callJit(target);
  leaveStubFrame(masm, true);

  masm.switchToBaselineFrameRealm(R1.scratchReg());

  // Enter type monitor IC to type-check result.
  EmitEnterTypeMonitorIC(masm);

  masm.bind(&failure);
  EmitStubGuardFailure(masm);
  return true;
}

bool ICCall_ScriptedApplyArguments::Compiler::generateStubCode(
    MacroAssembler& masm) {
  Label failure;
  AllocatableGeneralRegisterSet regs(availableGeneralRegs(0));

  Register argcReg = R0.scratchReg();
  regs.take(argcReg);
  regs.takeUnchecked(ICTailCallReg);

  //
  // Validate inputs
  //

  Register target =
      guardFunApply(masm, regs, argcReg, FunApply_MagicArgs, &failure);
  if (regs.has(target)) {
    regs.take(target);
  } else {
    // If target is already a reserved reg, take another register for it,
    // because it's probably currently an ExtractTemp, which might get clobbered
    // later.
    Register targetTemp = regs.takeAny();
    masm.movePtr(target, targetTemp);
    target = targetTemp;
  }

  // Push a stub frame so that we can perform a non-tail call.
  enterStubFrame(masm, regs.getAny());

  //
  // Push arguments
  //

  // Stack now looks like:
  //      [..., fun_apply, TargetV, TargetThisV, MagicArgsV, StubFrameHeader]

  // Push all arguments supplied to caller function onto the stack.
  pushCallerArguments(masm, regs);

  // Stack now looks like:
  //                                      BaselineFrameReg -------------------.
  //                                                                          v
  //      [..., fun_apply, TargetV, TargetThisV, MagicArgsV, StubFrameHeader,
  //       PushedArgN, ..., PushedArg0]
  // Can't fail after this, so it's ok to clobber argcReg.

  // Push actual argument 0 as |thisv| for call.
  masm.pushValue(Address(BaselineFrameReg, STUB_FRAME_SIZE + sizeof(Value)));

  // All pushes after this use Push instead of push to make sure ARM can align
  // stack properly for call.
  Register scratch = regs.takeAny();
  EmitBaselineCreateStubFrameDescriptor(masm, scratch, JitFrameLayout::Size());

  masm.loadPtr(Address(BaselineFrameReg, 0), argcReg);
  masm.loadPtr(Address(argcReg, BaselineFrame::offsetOfNumActualArgs()),
               argcReg);
  masm.Push(argcReg);
  masm.Push(target);
  masm.Push(scratch);

  masm.switchToObjectRealm(target, scratch);

  // Load nargs into scratch for underflow check, and then load jitcode pointer
  // into target.
  masm.load16ZeroExtend(Address(target, JSFunction::offsetOfNargs()), scratch);
  masm.loadJitCodeRaw(target, target);

  // Handle arguments underflow.
  Label noUnderflow;
  masm.branch32(Assembler::AboveOrEqual, argcReg, scratch, &noUnderflow);
  {
    // Call the arguments rectifier.
    TrampolinePtr argumentsRectifier =
        cx->runtime()->jitRuntime()->getArgumentsRectifier();
    masm.movePtr(argumentsRectifier, target);
  }
  masm.bind(&noUnderflow);
  regs.add(argcReg);

  // Do call
  masm.callJit(target);
  leaveStubFrame(masm, true);

  masm.switchToBaselineFrameRealm(R1.scratchReg());

  // Enter type monitor IC to type-check result.
  EmitEnterTypeMonitorIC(masm);

  masm.bind(&failure);
  EmitStubGuardFailure(masm);
  return true;
}

bool ICCall_ScriptedFunCall::Compiler::generateStubCode(MacroAssembler& masm) {
  Label failure;
  AllocatableGeneralRegisterSet regs(availableGeneralRegs(0));
  bool canUseTailCallReg = regs.has(ICTailCallReg);

  Register argcReg = R0.scratchReg();
  regs.take(argcReg);
  regs.takeUnchecked(ICTailCallReg);

  // Load the callee in R1.
  // Stack Layout:
  //      [ ..., CalleeVal, ThisVal, Arg0Val, ..., ArgNVal,
  //        +ICStackValueOffset+ ]
  BaseValueIndex calleeSlot(masm.getStackPointer(), argcReg,
                            ICStackValueOffset + sizeof(Value));
  masm.loadValue(calleeSlot, R1);
  regs.take(R1);

  // Ensure callee is fun_call.
  masm.branchTestObject(Assembler::NotEqual, R1, &failure);

  Register callee = masm.extractObject(R1, ExtractTemp0);
  masm.branchTestObjClass(Assembler::NotEqual, callee, &JSFunction::class_,
                          regs.getAny(), callee, &failure);
  masm.loadPtr(Address(callee, JSFunction::offsetOfNativeOrEnv()), callee);
  masm.branchPtr(Assembler::NotEqual, callee, ImmPtr(fun_call), &failure);

  // Ensure |this| is a function with a jit entry.
  BaseIndex thisSlot(masm.getStackPointer(), argcReg, TimesEight,
                     ICStackValueOffset);
  masm.loadValue(thisSlot, R1);

  masm.branchTestObject(Assembler::NotEqual, R1, &failure);
  callee = masm.extractObject(R1, ExtractTemp0);

  masm.branchTestObjClass(Assembler::NotEqual, callee, &JSFunction::class_,
                          regs.getAny(), callee, &failure);
  masm.branchIfFunctionHasNoJitEntry(callee, /* constructing */ false,
                                     &failure);
  masm.branchFunctionKind(Assembler::Equal, JSFunction::ClassConstructor,
                          callee, regs.getAny(), &failure);

  // Load the start of the target JitCode.
  Register code = regs.takeAny();
  masm.loadJitCodeRaw(callee, code);

  // We no longer need R1.
  regs.add(R1);

  // Push a stub frame so that we can perform a non-tail call.
  enterStubFrame(masm, regs.getAny());
  if (canUseTailCallReg) {
    regs.add(ICTailCallReg);
  }

  // Decrement argc if argc > 0. If argc == 0, push |undefined| as |this|.
  Label zeroArgs, done;
  masm.branchTest32(Assembler::Zero, argcReg, argcReg, &zeroArgs);

  // Avoid the copy of the callee (function.call).
  masm.sub32(Imm32(1), argcReg);

  // Values are on the stack left-to-right. Calling convention wants them
  // right-to-left so duplicate them on the stack in reverse order.

  pushCallArguments(masm, regs, argcReg, /* isJitCall = */ true);

  // Pop scripted callee (the original |this|).
  ValueOperand val = regs.takeAnyValue();
  masm.popValue(val);

  masm.jump(&done);
  masm.bind(&zeroArgs);

  // Copy scripted callee (the original |this|).
  Address thisSlotFromStubFrame(BaselineFrameReg, STUB_FRAME_SIZE);
  masm.loadValue(thisSlotFromStubFrame, val);

  // Align the stack.
  masm.alignJitStackBasedOnNArgs(0);

  // Store the new |this|.
  masm.pushValue(UndefinedValue());

  masm.bind(&done);

  // Unbox scripted callee.
  callee = masm.extractObject(val, ExtractTemp0);

  Register scratch = regs.takeAny();
  masm.switchToObjectRealm(callee, scratch);
  EmitBaselineCreateStubFrameDescriptor(masm, scratch, JitFrameLayout::Size());

  // Note that we use Push, not push, so that callJit will align the stack
  // properly on ARM.
  masm.Push(argcReg);
  masm.Push(callee);
  masm.Push(scratch);

  // Handle arguments underflow.
  Label noUnderflow;
  masm.load16ZeroExtend(Address(callee, JSFunction::offsetOfNargs()), callee);
  masm.branch32(Assembler::AboveOrEqual, argcReg, callee, &noUnderflow);
  {
    // Call the arguments rectifier.
    TrampolinePtr argumentsRectifier =
        cx->runtime()->jitRuntime()->getArgumentsRectifier();
    masm.movePtr(argumentsRectifier, code);
  }

  masm.bind(&noUnderflow);
  masm.callJit(code);

  leaveStubFrame(masm, true);

  masm.switchToBaselineFrameRealm(R1.scratchReg());

  // Enter type monitor IC to type-check result.
  EmitEnterTypeMonitorIC(masm);

  masm.bind(&failure);
  EmitStubGuardFailure(masm);
  return true;
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

ICCall_Scripted::ICCall_Scripted(JitCode* stubCode, ICStub* firstMonitorStub,
                                 JSFunction* callee, JSObject* templateObject,
                                 uint32_t pcOffset)
    : ICMonitoredStub(ICStub::Call_Scripted, stubCode, firstMonitorStub),
      callee_(callee),
      templateObject_(templateObject),
      pcOffset_(pcOffset) {}

ICCall_Native::ICCall_Native(JitCode* stubCode, ICStub* firstMonitorStub,
                             JSFunction* callee, JSObject* templateObject,
                             uint32_t pcOffset)
    : ICMonitoredStub(ICStub::Call_Native, stubCode, firstMonitorStub),
      callee_(callee),
      templateObject_(templateObject),
      pcOffset_(pcOffset) {
#ifdef JS_SIMULATOR
  // The simulator requires VM calls to be redirected to a special swi
  // instruction to handle them. To make this work, we store the redirected
  // pointer in the stub.
  native_ = Simulator::RedirectNativeFunction(
      JS_FUNC_TO_DATA_PTR(void*, callee->native()), Args_General3);
#endif
}

ICCall_ClassHook::ICCall_ClassHook(JitCode* stubCode, ICStub* firstMonitorStub,
                                   const Class* clasp, Native native,
                                   JSObject* templateObject, uint32_t pcOffset)
    : ICMonitoredStub(ICStub::Call_ClassHook, stubCode, firstMonitorStub),
      clasp_(clasp),
      native_(JS_FUNC_TO_DATA_PTR(void*, native)),
      templateObject_(templateObject),
      pcOffset_(pcOffset) {
#ifdef JS_SIMULATOR
  // The simulator requires VM calls to be redirected to a special swi
  // instruction to handle them. To make this work, we store the redirected
  // pointer in the stub.
  native_ = Simulator::RedirectNativeFunction(native_, Args_General3);
#endif
}

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
  FallbackICSpew(cx, stub, "UnaryArith(%s)", CodeName[op]);

  // The unary operations take a copied val because the original value is needed
  // below.
  RootedValue valCopy(cx, val);
  switch (op) {
    case JSOP_BITNOT: {
      if (!BitNot(cx, &valCopy, res)) {
        return false;
      }
      break;
    }
    case JSOP_NEG: {
      if (!NegOperation(cx, &valCopy, res)) {
        return false;
      }
      break;
    }
    case JSOP_INC: {
      if (!IncOperation(cx, &valCopy, res)) {
        return false;
      }
      break;
    }
    case JSOP_DEC: {
      if (!DecOperation(cx, &valCopy, res)) {
        return false;
      }
      break;
    }
    default:
      MOZ_CRASH("Unexpected op");
  }

  if (res.isDouble()) {
    stub->setSawDoubleResult();
  }

  TryAttachStub<UnaryArithIRGenerator>("UniryArith", cx, frame, stub,
                                       BaselineCacheIRStubKind::Regular, op,
                                       val, res);
  return true;
}

bool FallbackICCodeCompiler::emit_UnaryArith() {
  MOZ_ASSERT(R0 == JSReturnOperand);

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
      cx, stub, "CacheIRBinaryArith(%s,%d,%d)", CodeName[op],
      int(lhs.isDouble() ? JSVAL_TYPE_DOUBLE : lhs.extractNonDoubleType()),
      int(rhs.isDouble() ? JSVAL_TYPE_DOUBLE : rhs.extractNonDoubleType()));

  // Don't pass lhs/rhs directly, we need the original values when
  // generating stubs.
  RootedValue lhsCopy(cx, lhs);
  RootedValue rhsCopy(cx, rhs);

  // Perform the compare operation.
  switch (op) {
    case JSOP_ADD:
      // Do an add.
      if (!AddValues(cx, &lhsCopy, &rhsCopy, ret)) {
        return false;
      }
      break;
    case JSOP_SUB:
      if (!SubValues(cx, &lhsCopy, &rhsCopy, ret)) {
        return false;
      }
      break;
    case JSOP_MUL:
      if (!MulValues(cx, &lhsCopy, &rhsCopy, ret)) {
        return false;
      }
      break;
    case JSOP_DIV:
      if (!DivValues(cx, &lhsCopy, &rhsCopy, ret)) {
        return false;
      }
      break;
    case JSOP_MOD:
      if (!ModValues(cx, &lhsCopy, &rhsCopy, ret)) {
        return false;
      }
      break;
    case JSOP_POW:
      if (!PowValues(cx, &lhsCopy, &rhsCopy, ret)) {
        return false;
      }
      break;
    case JSOP_BITOR: {
      if (!BitOr(cx, &lhsCopy, &rhsCopy, ret)) {
        return false;
      }
      break;
    }
    case JSOP_BITXOR: {
      if (!BitXor(cx, &lhsCopy, &rhsCopy, ret)) {
        return false;
      }
      break;
    }
    case JSOP_BITAND: {
      if (!BitAnd(cx, &lhsCopy, &rhsCopy, ret)) {
        return false;
      }
      break;
    }
    case JSOP_LSH: {
      if (!BitLsh(cx, &lhsCopy, &rhsCopy, ret)) {
        return false;
      }
      break;
    }
    case JSOP_RSH: {
      if (!BitRsh(cx, &lhsCopy, &rhsCopy, ret)) {
        return false;
      }
      break;
    }
    case JSOP_URSH: {
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
  MOZ_ASSERT(R0 == JSReturnOperand);

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

  FallbackICSpew(cx, stub, "Compare(%s)", CodeName[op]);

  // Don't pass lhs/rhs directly, we need the original values when
  // generating stubs.
  RootedValue lhsCopy(cx, lhs);
  RootedValue rhsCopy(cx, rhs);

  // Perform the compare operation.
  bool out;
  switch (op) {
    case JSOP_LT:
      if (!LessThan(cx, &lhsCopy, &rhsCopy, &out)) {
        return false;
      }
      break;
    case JSOP_LE:
      if (!LessThanOrEqual(cx, &lhsCopy, &rhsCopy, &out)) {
        return false;
      }
      break;
    case JSOP_GT:
      if (!GreaterThan(cx, &lhsCopy, &rhsCopy, &out)) {
        return false;
      }
      break;
    case JSOP_GE:
      if (!GreaterThanOrEqual(cx, &lhsCopy, &rhsCopy, &out)) {
        return false;
      }
      break;
    case JSOP_EQ:
      if (!LooselyEqual<EqualityKind::Equal>(cx, &lhsCopy, &rhsCopy, &out)) {
        return false;
      }
      break;
    case JSOP_NE:
      if (!LooselyEqual<EqualityKind::NotEqual>(cx, &lhsCopy, &rhsCopy, &out)) {
        return false;
      }
      break;
    case JSOP_STRICTEQ:
      if (!StrictlyEqual<EqualityKind::Equal>(cx, &lhsCopy, &rhsCopy, &out)) {
        return false;
      }
      break;
    case JSOP_STRICTNE:
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
  MOZ_ASSERT(R0 == JSReturnOperand);

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

  Linker linker(masm, "BaselineICFallback");
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
