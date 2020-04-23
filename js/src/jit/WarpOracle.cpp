/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/WarpOracle.h"

#include "mozilla/IntegerPrintfMacros.h"
#include "mozilla/ScopeExit.h"

#include <algorithm>

#include "jit/CacheIR.h"
#include "jit/CacheIRCompiler.h"
#include "jit/CacheIROpsGenerated.h"
#include "jit/JitScript.h"
#include "jit/JitSpewer.h"
#include "jit/MIRGenerator.h"
#include "jit/WarpBuilder.h"
#include "jit/WarpCacheIRTranspiler.h"
#include "vm/BytecodeIterator.h"
#include "vm/BytecodeLocation.h"
#include "vm/Instrumentation.h"
#include "vm/Opcodes.h"

#include "vm/BytecodeIterator-inl.h"
#include "vm/BytecodeLocation-inl.h"
#include "vm/EnvironmentObject-inl.h"
#include "vm/Interpreter-inl.h"

using namespace js;
using namespace js::jit;

WarpOracle::WarpOracle(JSContext* cx, MIRGenerator& mirGen, HandleScript script)
    : cx_(cx), mirGen_(mirGen), alloc_(mirGen.alloc()), script_(script) {}

mozilla::GenericErrorResult<AbortReason> WarpOracle::abort(AbortReason r) {
  auto res = mirGen_.abort(r);
  JitSpew(JitSpew_IonAbort, "aborted @ %s", script_->filename());
  return res;
}

mozilla::GenericErrorResult<AbortReason> WarpOracle::abort(AbortReason r,
                                                           const char* message,
                                                           ...) {
  va_list ap;
  va_start(ap, message);
  auto res = mirGen_.abortFmt(r, message, ap);
  va_end(ap);
  JitSpew(JitSpew_IonAbort, "aborted @ %s", script_->filename());
  return res;
}

AbortReasonOr<WarpSnapshot*> WarpOracle::createSnapshot() {
  JitSpew(JitSpew_IonScripts,
          "Warp %sompiling script %s:%u:%u (%p) (warmup-counter=%" PRIu32
          ", level=%s)",
          (script_->hasIonScript() ? "Rec" : "C"), script_->filename(),
          script_->lineno(), script_->column(), static_cast<JSScript*>(script_),
          script_->getWarmUpCount(),
          OptimizationLevelString(mirGen_.optimizationInfo().level()));

  WarpScriptSnapshot* scriptSnapshot;
  MOZ_TRY_VAR(scriptSnapshot, createScriptSnapshot(script_));

  auto* snapshot = new (alloc_.fallible()) WarpSnapshot(cx_, scriptSnapshot);
  if (!snapshot) {
    return abort(AbortReason::Alloc);
  }

#ifdef JS_JITSPEW
  if (JitSpewEnabled(JitSpew_WarpSnapshots)) {
    Fprinter& out = JitSpewPrinter();
    snapshot->dump(out);
  }
#endif

  return snapshot;
}

template <typename T, typename... Args>
static MOZ_MUST_USE bool AddOpSnapshot(TempAllocator& alloc,
                                       WarpOpSnapshotList& snapshots,
                                       uint32_t offset, Args&&... args) {
  T* snapshot = new (alloc.fallible()) T(offset, std::forward<Args>(args)...);
  if (!snapshot) {
    return false;
  }

  snapshots.insertBack(snapshot);
  return true;
}

static MOZ_MUST_USE bool AddWarpGetImport(TempAllocator& alloc,
                                          WarpOpSnapshotList& snapshots,
                                          uint32_t offset, JSScript* script,
                                          PropertyName* name) {
  ModuleEnvironmentObject* env = GetModuleEnvironmentForScript(script);
  MOZ_ASSERT(env);

  Shape* shape;
  ModuleEnvironmentObject* targetEnv;
  MOZ_ALWAYS_TRUE(env->lookupImport(NameToId(name), &targetEnv, &shape));

  uint32_t numFixedSlots = shape->numFixedSlots();
  uint32_t slot = shape->slot();

  // In the rare case where this import hasn't been initialized already (we have
  // an import cycle where modules reference each other's imports), we need a
  // check.
  bool needsLexicalCheck =
      targetEnv->getSlot(slot).isMagic(JS_UNINITIALIZED_LEXICAL);

  return AddOpSnapshot<WarpGetImport>(alloc, snapshots, offset, targetEnv,
                                      numFixedSlots, slot, needsLexicalCheck);
}

AbortReasonOr<WarpEnvironment> WarpOracle::createEnvironment(
    HandleScript script) {
  // Don't do anything if the script doesn't use the environment chain.
  // Always make an environment chain if the script needs an arguments object
  // because ArgumentsObject construction requires the environment chain to be
  // passed in.
  if (!script->jitScript()->usesEnvironmentChain() && !script->needsArgsObj()) {
    return WarpEnvironment(NoEnvironment());
  }

  if (ModuleObject* module = script->module()) {
    JSObject* obj = &module->initialEnvironment();
    return WarpEnvironment(ConstantObjectEnvironment(obj));
  }

  JSFunction* fun = script->function();
  if (!fun) {
    // For global scripts without a non-syntactic global scope, the environment
    // chain is the global lexical environment.
    MOZ_ASSERT(!script->isForEval());
    MOZ_ASSERT(!script->hasNonSyntacticScope());
    JSObject* obj = &script->global().lexicalEnvironment();
    return WarpEnvironment(ConstantObjectEnvironment(obj));
  }

  // TODO: Parameter expression-induced extra var environment not
  // yet handled.
  if (fun->needsExtraBodyVarEnvironment()) {
    return abort(AbortReason::Disable, "Extra var environment unsupported");
  }

  JSObject* templateEnv = script->jitScript()->templateEnvironment();

  CallObject* callObjectTemplate = nullptr;
  if (fun->needsCallObject()) {
    callObjectTemplate = &templateEnv->as<CallObject>();
  }

  LexicalEnvironmentObject* namedLambdaTemplate = nullptr;
  if (fun->needsNamedLambdaEnvironment()) {
    if (callObjectTemplate) {
      templateEnv = templateEnv->enclosingEnvironment();
    }
    namedLambdaTemplate = &templateEnv->as<LexicalEnvironmentObject>();
  }

  return WarpEnvironment(
      FunctionEnvironment(callObjectTemplate, namedLambdaTemplate));
}

AbortReasonOr<WarpScriptSnapshot*> WarpOracle::createScriptSnapshot(
    HandleScript script) {
  MOZ_ASSERT(script->hasJitScript());

  if (!script->jitScript()->ensureHasCachedIonData(cx_, script)) {
    return abort(AbortReason::Error);
  }

  if (script->jitScript()->hasTryFinally()) {
    return abort(AbortReason::Disable, "Try-finally not supported");
  }

  WarpEnvironment environment{NoEnvironment()};
  MOZ_TRY_VAR(environment, createEnvironment(script));

  // Unfortunately LinkedList<> asserts the list is empty in its destructor.
  // Clear the list if we abort compilation.
  WarpOpSnapshotList opSnapshots;
  auto autoClearOpSnapshots =
      mozilla::MakeScopeExit([&] { opSnapshots.clear(); });

  ModuleObject* moduleObject = nullptr;

  mozilla::Maybe<bool> instrumentationActive;
  mozilla::Maybe<int32_t> instrumentationScriptId;
  JSObject* instrumentationCallback = nullptr;

  // Analyze the bytecode. Abort compilation for unsupported ops and create
  // WarpOpSnapshots.
  for (BytecodeLocation loc : AllBytecodesIterable(script)) {
    JSOp op = loc.getOp();
    uint32_t offset = loc.bytecodeToOffset(script);
    switch (op) {
      case JSOp::Arguments:
        if (script->needsArgsObj()) {
          bool mapped = script->hasMappedArgsObj();
          ArgumentsObject* templateObj =
              script->realm()->maybeArgumentsTemplateObject(mapped);
          if (!AddOpSnapshot<WarpArguments>(alloc_, opSnapshots, offset,
                                            templateObj)) {
            return abort(AbortReason::Alloc);
          }
        }
        break;

      case JSOp::RegExp: {
        bool hasShared = loc.getRegExp(script)->hasShared();
        if (!AddOpSnapshot<WarpRegExp>(alloc_, opSnapshots, offset,
                                       hasShared)) {
          return abort(AbortReason::Alloc);
        }
        break;
      }

      case JSOp::FunctionThis:
        if (!script->strict() && script->hasNonSyntacticScope()) {
          // Abort because MComputeThis doesn't support non-syntactic scopes
          // (a deprecated SpiderMonkey mechanism). If this becomes an issue we
          // could support it by refactoring GetFunctionThis to not take a frame
          // pointer and then call that.
          return abort(AbortReason::Disable,
                       "JSOp::FunctionThis with non-syntactic scope");
        }
        break;

      case JSOp::GlobalThis:
        if (script->hasNonSyntacticScope()) {
          // We don't compile global scripts with a non-syntactic scope, but
          // we can end up here when we're compiling an arrow function.
          return abort(AbortReason::Disable,
                       "JSOp::GlobalThis with non-syntactic scope");
        }
        break;

      case JSOp::FunctionProto: {
        // If we already resolved this proto we can bake it in.
        if (JSObject* proto =
                cx_->global()->maybeGetPrototype(JSProto_Function)) {
          if (!AddOpSnapshot<WarpFunctionProto>(alloc_, opSnapshots, offset,
                                                proto)) {
            return abort(AbortReason::Alloc);
          }
        }
        break;
      }

      case JSOp::GetIntrinsic: {
        // If we already cloned this intrinsic we can bake it in.
        PropertyName* name = loc.getPropertyName(script);
        Value val;
        if (cx_->global()->maybeExistingIntrinsicValue(name, &val)) {
          if (!AddOpSnapshot<WarpGetIntrinsic>(alloc_, opSnapshots, offset,
                                               val)) {
            return abort(AbortReason::Alloc);
          }
        }
        break;
      }

      case JSOp::ImportMeta: {
        if (!moduleObject) {
          moduleObject = GetModuleObjectForScript(script);
          MOZ_ASSERT(moduleObject->isTenured());
        }
        break;
      }

      case JSOp::CallSiteObj: {
        // Prepare the object so that WarpBuilder can just push it as constant.
        if (!ProcessCallSiteObjOperation(cx_, script, loc.toRawBytecode())) {
          return abort(AbortReason::Error);
        }
        break;
      }

      case JSOp::NewArrayCopyOnWrite: {
        MOZ_CRASH("Bug 1626854: COW arrays disabled without TI for now");

        // Fix up the copy-on-write ArrayObject if needed.
        jsbytecode* pc = loc.toRawBytecode();
        if (!ObjectGroup::getOrFixupCopyOnWriteObject(cx_, script, pc)) {
          return abort(AbortReason::Error);
        }
        break;
      }

      case JSOp::Object: {
        if (!mirGen_.options.cloneSingletons()) {
          cx_->realm()->behaviors().setSingletonsAsValues();
        }
        break;
      }

      case JSOp::GetImport: {
        PropertyName* name = loc.getPropertyName(script);
        if (!AddWarpGetImport(alloc_, opSnapshots, offset, script, name)) {
          return abort(AbortReason::Alloc);
        }
        break;
      }

      case JSOp::Lambda:
      case JSOp::LambdaArrow: {
        JSFunction* fun = loc.getFunction(script);
        if (IsAsmJSModule(fun)) {
          return abort(AbortReason::Disable, "asm.js module function lambda");
        }

        // WarpBuilder relies on these conditions.
        MOZ_ASSERT(!fun->isSingleton());
        MOZ_ASSERT(!ObjectGroup::useSingletonForClone(fun));

        if (!AddOpSnapshot<WarpLambda>(alloc_, opSnapshots, offset,
                                       fun->baseScript(), fun->flags(),
                                       fun->nargs())) {
          return abort(AbortReason::Alloc);
        }
        break;
      }

      case JSOp::GetElemSuper: {
#if defined(JS_CODEGEN_X86)
        // x86 does not have enough registers if profiling is enabled.
        if (mirGen_.instrumentedProfiling()) {
          return abort(AbortReason::Disable,
                       "GetElemSuper with profiling is not supported on x86");
        }
#endif
        break;
      }

      case JSOp::InstrumentationActive: {
        // All IonScripts in the realm are discarded when instrumentation
        // activity changes, so we can treat the value we get as a constant.
        if (instrumentationActive.isNothing()) {
          bool active = RealmInstrumentation::isActive(cx_->global());
          instrumentationActive.emplace(active);
        }
        break;
      }

      case JSOp::InstrumentationCallback: {
        if (!instrumentationCallback) {
          JSObject* obj = RealmInstrumentation::getCallback(cx_->global());
          if (IsInsideNursery(obj)) {
            // Unfortunately the callback can be nursery allocated. If this
            // becomes an issue we should consider triggering a minor GC after
            // installing it.
            return abort(AbortReason::Disable,
                         "Nursery-allocated instrumentation callback");
          }
          instrumentationCallback = obj;
        }
        break;
      }

      case JSOp::InstrumentationScriptId: {
        // Getting the script ID requires interacting with the Debugger used for
        // instrumentation, but cannot run script.
        if (instrumentationScriptId.isNothing()) {
          int32_t id = 0;
          if (!RealmInstrumentation::getScriptId(cx_, cx_->global(), script,
                                                 &id)) {
            return abort(AbortReason::Error);
          }
          instrumentationScriptId.emplace(id);
        }
        break;
      }

      case JSOp::Rest: {
        const ICEntry& entry = script->jitScript()->icEntryFromPCOffset(offset);
        ICRest_Fallback* stub = entry.fallbackStub()->toRest_Fallback();
        if (!AddOpSnapshot<WarpRest>(alloc_, opSnapshots, offset,
                                     stub->templateObject())) {
          return abort(AbortReason::Alloc);
        }
        break;
      }

      case JSOp::GetName:
      case JSOp::GetGName:
      case JSOp::GetProp:
      case JSOp::CallProp:
      case JSOp::Length:
      case JSOp::GetElem:
      case JSOp::CallElem:
        MOZ_TRY(maybeInlineIC(opSnapshots, script, loc));
        break;

      case JSOp::Nop:
      case JSOp::NopDestructuring:
      case JSOp::TryDestructuring:
      case JSOp::Lineno:
      case JSOp::DebugLeaveLexicalEnv:
      case JSOp::Undefined:
      case JSOp::Void:
      case JSOp::Null:
      case JSOp::Hole:
      case JSOp::Uninitialized:
      case JSOp::IsConstructing:
      case JSOp::False:
      case JSOp::True:
      case JSOp::Zero:
      case JSOp::One:
      case JSOp::Int8:
      case JSOp::Uint16:
      case JSOp::Uint24:
      case JSOp::Int32:
      case JSOp::Double:
      case JSOp::ResumeIndex:
      case JSOp::BigInt:
      case JSOp::String:
      case JSOp::Symbol:
      case JSOp::Pop:
      case JSOp::PopN:
      case JSOp::Dup:
      case JSOp::Dup2:
      case JSOp::DupAt:
      case JSOp::Swap:
      case JSOp::Pick:
      case JSOp::Unpick:
      case JSOp::GetLocal:
      case JSOp::SetLocal:
      case JSOp::InitLexical:
      case JSOp::GetArg:
      case JSOp::SetArg:
      case JSOp::ToNumeric:
      case JSOp::Pos:
      case JSOp::Inc:
      case JSOp::Dec:
      case JSOp::Neg:
      case JSOp::BitNot:
      case JSOp::Add:
      case JSOp::Sub:
      case JSOp::Mul:
      case JSOp::Div:
      case JSOp::Mod:
      case JSOp::Pow:
      case JSOp::BitAnd:
      case JSOp::BitOr:
      case JSOp::BitXor:
      case JSOp::Lsh:
      case JSOp::Rsh:
      case JSOp::Ursh:
      case JSOp::Eq:
      case JSOp::Ne:
      case JSOp::Lt:
      case JSOp::Le:
      case JSOp::Gt:
      case JSOp::Ge:
      case JSOp::StrictEq:
      case JSOp::StrictNe:
      case JSOp::JumpTarget:
      case JSOp::LoopHead:
      case JSOp::IfEq:
      case JSOp::IfNe:
      case JSOp::And:
      case JSOp::Or:
      case JSOp::Case:
      case JSOp::Default:
      case JSOp::Coalesce:
      case JSOp::Goto:
      case JSOp::DebugCheckSelfHosted:
      case JSOp::DynamicImport:
      case JSOp::Not:
      case JSOp::ToString:
      case JSOp::DefVar:
      case JSOp::DefLet:
      case JSOp::DefConst:
      case JSOp::DefFun:
      case JSOp::CheckGlobalOrEvalDecl:
      case JSOp::BindVar:
      case JSOp::MutateProto:
      case JSOp::Callee:
      case JSOp::ClassConstructor:
      case JSOp::DerivedConstructor:
      case JSOp::ToAsyncIter:
      case JSOp::ToId:
      case JSOp::Typeof:
      case JSOp::TypeofExpr:
      case JSOp::ObjWithProto:
      case JSOp::GetAliasedVar:
      case JSOp::SetAliasedVar:
      case JSOp::InitAliasedLexical:
      case JSOp::EnvCallee:
      case JSOp::Iter:
      case JSOp::IterNext:
      case JSOp::MoreIter:
      case JSOp::EndIter:
      case JSOp::IsNoIter:
      case JSOp::Call:
      case JSOp::CallIgnoresRv:
      case JSOp::CallIter:
      case JSOp::FunCall:
      case JSOp::FunApply:
      case JSOp::New:
      case JSOp::SuperCall:
      case JSOp::BindName:
      case JSOp::BindGName:
      case JSOp::SetProp:
      case JSOp::StrictSetProp:
      case JSOp::SetName:
      case JSOp::StrictSetName:
      case JSOp::SetGName:
      case JSOp::StrictSetGName:
      case JSOp::InitGLexical:
      case JSOp::SetElem:
      case JSOp::StrictSetElem:
      case JSOp::DelProp:
      case JSOp::StrictDelProp:
      case JSOp::DelElem:
      case JSOp::StrictDelElem:
      case JSOp::SetFunName:
      case JSOp::PushLexicalEnv:
      case JSOp::PopLexicalEnv:
      case JSOp::FreshenLexicalEnv:
      case JSOp::RecreateLexicalEnv:
      case JSOp::ImplicitThis:
      case JSOp::GImplicitThis:
      case JSOp::CheckClassHeritage:
      case JSOp::CheckThis:
      case JSOp::CheckThisReinit:
      case JSOp::CheckReturn:
      case JSOp::CheckLexical:
      case JSOp::CheckAliasedLexical:
      case JSOp::InitHomeObject:
      case JSOp::SuperBase:
      case JSOp::SuperFun:
      case JSOp::NewArray:
      case JSOp::NewObject:
      case JSOp::NewObjectWithGroup:
      case JSOp::NewInit:
      case JSOp::InitPropGetter:
      case JSOp::InitPropSetter:
      case JSOp::InitHiddenPropGetter:
      case JSOp::InitHiddenPropSetter:
      case JSOp::InitElemGetter:
      case JSOp::InitElemSetter:
      case JSOp::InitHiddenElemGetter:
      case JSOp::InitHiddenElemSetter:
      case JSOp::In:
      case JSOp::HasOwn:
      case JSOp::Instanceof:
      case JSOp::NewTarget:
      case JSOp::CheckIsObj:
      case JSOp::CheckIsCallable:
      case JSOp::CheckObjCoercible:
      case JSOp::GetPropSuper:
      case JSOp::InitProp:
      case JSOp::InitLockedProp:
      case JSOp::InitHiddenProp:
      case JSOp::InitElem:
      case JSOp::InitHiddenElem:
      case JSOp::InitElemArray:
      case JSOp::InitElemInc:
      case JSOp::FunWithProto:
      case JSOp::SpreadCall:
      case JSOp::SpreadNew:
      case JSOp::SpreadSuperCall:
      case JSOp::OptimizeSpreadCall:
      case JSOp::Debugger:
      case JSOp::TableSwitch:
      case JSOp::Try:
      case JSOp::Throw:
      case JSOp::ThrowSetConst:
      case JSOp::SetRval:
      case JSOp::Return:
      case JSOp::RetRval:
        // Supported by WarpBuilder. Nothing to do.
        break;

        // Unsupported ops. Don't use a 'default' here, we want to trigger a
        // compiler warning when adding a new JSOp.
#define DEF_CASE(OP) case JSOp::OP:
        WARP_UNSUPPORTED_OPCODE_LIST(DEF_CASE)
#undef DEF_CASE
#ifdef DEBUG
        return abort(AbortReason::Disable, "Unsupported opcode: %s",
                     CodeName(op));
#else
        return abort(AbortReason::Disable, "Unsupported opcode: %u",
                     uint8_t(op));
#endif
    }
  }

  auto* scriptSnapshot = new (alloc_.fallible()) WarpScriptSnapshot(
      script, environment, std::move(opSnapshots), moduleObject,
      instrumentationCallback, instrumentationScriptId, instrumentationActive);
  if (!scriptSnapshot) {
    return abort(AbortReason::Alloc);
  }

  autoClearOpSnapshots.release();

  return scriptSnapshot;
}

AbortReasonOr<Ok> WarpOracle::maybeInlineIC(WarpOpSnapshotList& snapshots,
                                            HandleScript script,
                                            BytecodeLocation loc) {
  // Add a WarpCacheIR snapshot if the Baseline IC has a single ICStub we can
  // inline.

  MOZ_ASSERT(loc.opHasIC());

  uint32_t offset = loc.bytecodeToOffset(script);

  // TODO: slow. Should traverse ICEntries as we go, like BaselineCompiler.
  const ICEntry& entry = script->jitScript()->icEntryFromPCOffset(offset);

  ICStub* stub = entry.firstStub();

  if (stub->isFallback()) {
    // No optimized stubs.
    // TODO: add logging for failure/success cases.
    return Ok();
  }

  if (!stub->next()->isFallback()) {
    // More than one optimized stub.
    return Ok();
  }

  // TODO: check stub's hit count if we're not doing eager compilation.
  // TODO: check stub data for nursery pointers.
  // TODO: don't inline if the IC had unhandled cases => CacheIR is incomplete.
  // TOOD: have a consistent bailout => invalidate story. Set a flag on the IC?

  const CacheIRStubInfo* stubInfo = nullptr;
  const uint8_t* stubData = nullptr;
  switch (stub->kind()) {
    case ICStub::CacheIR_Regular:
      stubInfo = stub->toCacheIR_Regular()->stubInfo();
      stubData = stub->toCacheIR_Regular()->stubDataStart();
      break;
    case ICStub::CacheIR_Monitored:
      stubInfo = stub->toCacheIR_Monitored()->stubInfo();
      stubData = stub->toCacheIR_Monitored()->stubDataStart();
      break;
    case ICStub::CacheIR_Updated:
      stubInfo = stub->toCacheIR_Updated()->stubInfo();
      stubData = stub->toCacheIR_Updated()->stubDataStart();
      break;
    default:
      MOZ_CRASH("Unexpected stub");
  }

  // Only create a snapshots if all opcodes are supported by the transpiler.
  CacheIRReader reader(stubInfo);
  while (reader.more()) {
    CacheOp op = reader.readOp();
    uint32_t argLength = CacheIROpArgLengths[size_t(op)];
    reader.skip(argLength);

    switch (op) {
#define DEFINE_OP(op, ...) \
  case CacheOp::op:        \
    break;
      CACHE_IR_TRANSPILER_OPS(DEFINE_OP)
#undef DEFINE_OP

      default:
        // Unsupported opcode.
        JitSpew(JitSpew_WarpTranspiler, "unsupported CacheIR opcode: %s",
                CacheIROpNames[size_t(op)]);
        return Ok();
    }
  }

  // Copy the ICStub data to protect against the stub being unlinked or mutated.
  // We don't need to copy the CacheIRStubInfo: because we store and trace the
  // stub's JitCode*, the baselineCacheIRStubCodes_ map in JitZone will keep it
  // alive.
  size_t bytesNeeded = stubInfo->stubDataSize();
  uint8_t* stubDataCopy = alloc_.allocateArray<uint8_t>(bytesNeeded);
  if (!stubDataCopy) {
    return abort(AbortReason::Alloc);
  }

  // We don't need any GC barriers because the stub data does not contain
  // nursery pointers (checked above) so we can do a bitwise copy.
  std::copy_n(stubData, bytesNeeded, stubDataCopy);

  JitCode* jitCode = stub->jitCode();

  if (!AddOpSnapshot<WarpCacheIR>(alloc_, snapshots, offset, jitCode, stubInfo,
                                  stubDataCopy)) {
    return abort(AbortReason::Alloc);
  }

  return Ok();
}
