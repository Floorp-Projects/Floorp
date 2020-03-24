/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/WarpOracle.h"

#include "mozilla/ScopeExit.h"

#include "jit/JitScript.h"
#include "jit/MIRGenerator.h"
#include "jit/WarpBuilder.h"
#include "vm/BytecodeIterator.h"
#include "vm/BytecodeLocation.h"

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
  WarpScriptSnapshot* scriptSnapshot;
  MOZ_TRY_VAR(scriptSnapshot, createScriptSnapshot(script_));

  auto* snapshot = new (alloc_.fallible()) WarpSnapshot(cx_, scriptSnapshot);
  if (!snapshot) {
    return abort(AbortReason::Alloc);
  }

  return snapshot;
}

WarpSnapshot::WarpSnapshot(JSContext* cx, WarpScriptSnapshot* script)
    : script_(script),
      globalLexicalEnv_(&cx->global()->lexicalEnvironment()),
      globalLexicalEnvThis_(globalLexicalEnv_->thisValue()) {}

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
  WarpEnvironment env;

  // Don't do anything if the script doesn't use the environment chain.
  // Always make an environment chain if the script needs an arguments object
  // because ArgumentsObject construction requires the environment chain to be
  // passed in.
  if (!script->jitScript()->usesEnvironmentChain() && !script->needsArgsObj()) {
    MOZ_ASSERT(env.kind() == WarpEnvironment::Kind::None);
    return env;
  }

  if (ModuleObject* module = script->module()) {
    env.initConstantObject(&module->initialEnvironment());
    return env;
  }

  JSFunction* fun = script->function();
  if (!fun) {
    // For global scripts without a non-syntactic global scope, the environment
    // chain is the global lexical environment.
    MOZ_ASSERT(!script->isForEval());
    MOZ_ASSERT(!script->hasNonSyntacticScope());
    env.initConstantObject(&script->global().lexicalEnvironment());
    return env;
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

  env.initFunction(callObjectTemplate, namedLambdaTemplate);
  return env;
}

WarpScriptSnapshot::WarpScriptSnapshot(JSScript* script,
                                       const WarpEnvironment& env,
                                       WarpOpSnapshotList&& opSnapshots,
                                       ModuleObject* moduleObject)
    : script_(script),
      environment_(env),
      opSnapshots_(std::move(opSnapshots)),
      moduleObject_(moduleObject),
      isArrowFunction_(script->isFunction() && script->function()->isArrow()) {}

AbortReasonOr<WarpScriptSnapshot*> WarpOracle::createScriptSnapshot(
    HandleScript script) {
  MOZ_ASSERT(script->hasJitScript());

  if (!script->jitScript()->ensureHasCachedIonData(cx_, script)) {
    return abort(AbortReason::Error);
  }

  WarpEnvironment environment;
  MOZ_TRY_VAR(environment, createEnvironment(script));

  // Unfortunately LinkedList<> asserts the list is empty in its destructor.
  // Clear the list if we abort compilation.
  WarpOpSnapshotList opSnapshots;
  auto autoClearOpSnapshots =
      mozilla::MakeScopeExit([&] { opSnapshots.clear(); });

  ModuleObject* moduleObject = nullptr;

  // Analyze the bytecode to look for opcodes we can't compile yet. Eventually
  // this loop will also be responsible for copying IC data.
  for (BytecodeLocation loc : AllBytecodesIterable(script)) {
    JSOp op = loc.getOp();
    switch (op) {
#define OP_CASE(OP) case JSOp::OP:
      WARP_OPCODE_LIST(OP_CASE)
#undef OP_CASE
      break;

      default:
#ifdef DEBUG
        return abort(AbortReason::Disable, "Unsupported opcode: %s",
                     CodeName(op));
#else
        return abort(AbortReason::Disable, "Unsupported opcode: %u",
                     uint8_t(op));
#endif
    }

    uint32_t offset = loc.bytecodeToOffset(script);

    // Allocate op snapshots for data we'll need off-thread.
    // TODO: merge this switch-statement with the previous one when we overhaul
    // WARP_OPCODE_LIST.
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

      case JSOp::BuiltinProto: {
        // If we already resolved this proto we can bake it in.
        JSProtoKey key = loc.getProtoKey();
        if (JSObject* proto = cx_->global()->maybeGetPrototype(key)) {
          if (!AddOpSnapshot<WarpBuiltinProto>(alloc_, opSnapshots, offset,
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

      default:
        break;
    }
  }

  auto* scriptSnapshot = new (alloc_.fallible()) WarpScriptSnapshot(
      script, environment, std::move(opSnapshots), moduleObject);
  if (!scriptSnapshot) {
    return abort(AbortReason::Alloc);
  }

  autoClearOpSnapshots.release();

  return scriptSnapshot;
}
