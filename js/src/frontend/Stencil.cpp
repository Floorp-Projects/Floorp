/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "frontend/Stencil.h"

#include "mozilla/RefPtr.h"  // RefPtr

#include "frontend/AbstractScopePtr.h"  // ScopeIndex
#include "frontend/BytecodeSection.h"   // EmitScriptThingsVector
#include "frontend/CompilationInfo.h"   // CompilationInfo
#include "frontend/SharedContext.h"
#include "gc/AllocKind.h"   // gc::AllocKind
#include "js/CallArgs.h"    // JSNative
#include "js/RootingAPI.h"  // Rooted
#include "js/TracingAPI.h"  // TraceEdge
#include "js/Value.h"       // ObjectValue
#include "js/WasmModule.h"  // JS::WasmModule
#include "vm/EnvironmentObject.h"
#include "vm/GeneratorAndAsyncKind.h"  // GeneratorKind, FunctionAsyncKind
#include "vm/JSContext.h"              // JSContext
#include "vm/JSFunction.h"  // JSFunction, GetFunctionPrototype, NewFunctionWithProto
#include "vm/JSObject.h"     // JSObject
#include "vm/JSScript.h"     // BaseScript, JSScript
#include "vm/ObjectGroup.h"  // TenuredObject
#include "vm/Scope.h"  // Scope, CreateEnvironmentShape, EmptyEnvironmentShape
#include "vm/StencilEnums.h"  // ImmutableScriptFlagsEnum
#include "vm/StringType.h"    // JSAtom, js::CopyChars
#include "wasm/AsmJS.h"       // InstantiateAsmJS

using namespace js;
using namespace js::frontend;

bool frontend::RegExpCreationData::init(JSContext* cx, JSAtom* pattern,
                                        JS::RegExpFlags flags) {
  length_ = pattern->length();
  buf_ = cx->make_pod_array<char16_t>(length_);
  if (!buf_) {
    return false;
  }
  js::CopyChars(buf_.get(), *pattern);
  flags_ = flags;
  return true;
}

bool frontend::EnvironmentShapeCreationData::createShape(
    JSContext* cx, MutableHandleShape shape) {
  struct Matcher {
    JSContext* cx;
    MutableHandleShape& shape;

    bool operator()(CreateEnvShapeData& data) {
      shape.set(CreateEnvironmentShape(cx, data.freshBi, data.cls,
                                       data.nextEnvironmentSlot,
                                       data.baseShapeFlags));
      return shape;
    }

    bool operator()(EmptyEnvShapeData& data) {
      shape.set(EmptyEnvironmentShape(cx, data.cls, JSSLOT_FREE(data.cls),
                                      data.baseShapeFlags));
      return shape;
    }

    bool operator()(mozilla::Nothing&) {
      shape.set(nullptr);
      return true;
    }
  };

  Matcher m{cx, shape};
  return data_.match(m);
}

Scope* ScopeCreationData::getEnclosingScope(JSContext* cx) {
  if (enclosing_.isScopeCreationData()) {
    ScopeCreationData& enclosingData = enclosing_.scopeCreationData().get();
    return enclosingData.getScope();
  }

  return enclosing_.scope();
}

JSFunction* ScopeCreationData::function(
    frontend::CompilationInfo& compilationInfo) {
  return compilationInfo.functions[*functionIndex_];
}

Scope* ScopeCreationData::createScope(JSContext* cx,
                                      CompilationInfo& compilationInfo) {
  // If we've already created a scope, best just return it.
  if (scope_) {
    return scope_;
  }

  Scope* scope = nullptr;
  switch (kind()) {
    case ScopeKind::Function: {
      scope = createSpecificScope<FunctionScope>(cx, compilationInfo);
      break;
    }
    case ScopeKind::Lexical:
    case ScopeKind::SimpleCatch:
    case ScopeKind::Catch:
    case ScopeKind::NamedLambda:
    case ScopeKind::StrictNamedLambda:
    case ScopeKind::FunctionLexical:
    case ScopeKind::ClassBody: {
      scope = createSpecificScope<LexicalScope>(cx, compilationInfo);
      break;
    }
    case ScopeKind::FunctionBodyVar: {
      scope = createSpecificScope<VarScope>(cx, compilationInfo);
      break;
    }
    case ScopeKind::Global:
    case ScopeKind::NonSyntactic: {
      scope = createSpecificScope<GlobalScope>(cx, compilationInfo);
      break;
    }
    case ScopeKind::Eval:
    case ScopeKind::StrictEval: {
      scope = createSpecificScope<EvalScope>(cx, compilationInfo);
      break;
    }
    case ScopeKind::Module: {
      scope = createSpecificScope<ModuleScope>(cx, compilationInfo);
      break;
    }
    case ScopeKind::With: {
      scope = createSpecificScope<WithScope>(cx, compilationInfo);
      break;
    }
    case ScopeKind::WasmFunction:
    case ScopeKind::WasmInstance: {
      MOZ_CRASH("Unexpected deferred type");
    }
  }
  return scope;
}

void ScopeCreationData::trace(JSTracer* trc) {
  if (enclosing_) {
    enclosing_.trace(trc);
  }

  environmentShape_.trace(trc);

  if (scope_) {
    TraceEdge(trc, &scope_, "ScopeCreationData Scope");
  }

  // Trace Datas
  if (data_) {
    switch (kind()) {
      case ScopeKind::Function: {
        data<FunctionScope>().trace(trc);
        break;
      }
      case ScopeKind::Lexical:
      case ScopeKind::SimpleCatch:
      case ScopeKind::Catch:
      case ScopeKind::NamedLambda:
      case ScopeKind::StrictNamedLambda:
      case ScopeKind::FunctionLexical:
      case ScopeKind::ClassBody: {
        data<LexicalScope>().trace(trc);
        break;
      }
      case ScopeKind::FunctionBodyVar: {
        data<VarScope>().trace(trc);
        break;
      }
      case ScopeKind::Global:
      case ScopeKind::NonSyntactic: {
        data<GlobalScope>().trace(trc);
        break;
      }
      case ScopeKind::Eval:
      case ScopeKind::StrictEval: {
        data<EvalScope>().trace(trc);
        break;
      }
      case ScopeKind::Module: {
        data<ModuleScope>().trace(trc);
        break;
      }
      case ScopeKind::With:
      default:
        MOZ_CRASH("Unexpected data type");
    }
  }
}

uint32_t ScopeCreationData::nextFrameSlot() const {
  switch (kind()) {
    case ScopeKind::Function:
      return nextFrameSlot<FunctionScope>();
    case ScopeKind::FunctionBodyVar:
      return nextFrameSlot<VarScope>();
    case ScopeKind::Lexical:
    case ScopeKind::SimpleCatch:
    case ScopeKind::Catch:
    case ScopeKind::FunctionLexical:
    case ScopeKind::ClassBody:
      return nextFrameSlot<LexicalScope>();
    case ScopeKind::NamedLambda:
    case ScopeKind::StrictNamedLambda:
      // Named lambda scopes cannot have frame slots.
      return 0;
    case ScopeKind::Eval:
    case ScopeKind::StrictEval:
      return nextFrameSlot<EvalScope>();
    case ScopeKind::Global:
    case ScopeKind::NonSyntactic:
      return 0;
    case ScopeKind::Module:
      return nextFrameSlot<ModuleScope>();
    case ScopeKind::WasmInstance:
    case ScopeKind::WasmFunction:
    case ScopeKind::With:
      MOZ_CRASH(
          "With, WasmInstance and WasmFunction Scopes don't get "
          "nextFrameSlot()");
      return 0;
  }
  MOZ_CRASH("Not an enclosing intra-frame scope");
}

void StencilModuleEntry::trace(JSTracer* trc) {
  if (specifier) {
    TraceManuallyBarrieredEdge(trc, &specifier, "module specifier");
  }
  if (localName) {
    TraceManuallyBarrieredEdge(trc, &localName, "module local name");
  }
  if (importName) {
    TraceManuallyBarrieredEdge(trc, &importName, "module import name");
  }
  if (exportName) {
    TraceManuallyBarrieredEdge(trc, &exportName, "module export name");
  }
}

void StencilModuleMetadata::trace(JSTracer* trc) {
  requestedModules.trace(trc);
  importEntries.trace(trc);
  localExportEntries.trace(trc);
  indirectExportEntries.trace(trc);
  starExportEntries.trace(trc);
}

void ScriptStencil::trace(JSTracer* trc) {
  for (ScriptThingVariant& thing : gcThings) {
    if (thing.is<ScriptAtom>()) {
      JSAtom* atom = thing.as<ScriptAtom>();
      TraceRoot(trc, &atom, "script-atom");
      MOZ_ASSERT(atom == thing.as<ScriptAtom>(), "Atoms should be unmovable");
    }
  }

  if (functionAtom) {
    TraceRoot(trc, &functionAtom, "script-atom");
  }
}

static bool CreateLazyScript(JSContext* cx, CompilationInfo& compilationInfo,
                             ScriptStencil& stencil, HandleFunction function) {
  const ScriptThingsVector& gcthings = stencil.gcThings;

  Rooted<BaseScript*> lazy(
      cx, BaseScript::CreateRawLazy(cx, gcthings.length(), function,
                                    compilationInfo.sourceObject,
                                    stencil.extent, stencil.immutableFlags));
  if (!lazy) {
    return false;
  }

  if (!EmitScriptThingsVector(cx, compilationInfo, gcthings,
                              lazy->gcthingsForInit())) {
    return false;
  }

  function->initScript(lazy);

  return true;
}

static JSFunction* CreateFunction(JSContext* cx,
                                  CompilationInfo& compilationInfo,
                                  ScriptStencil& stencil,
                                  FunctionIndex functionIndex) {
  GeneratorKind generatorKind =
      stencil.immutableFlags.hasFlag(ImmutableScriptFlagsEnum::IsGenerator)
          ? GeneratorKind::Generator
          : GeneratorKind::NotGenerator;
  FunctionAsyncKind asyncKind =
      stencil.immutableFlags.hasFlag(ImmutableScriptFlagsEnum::IsAsync)
          ? FunctionAsyncKind::AsyncFunction
          : FunctionAsyncKind::SyncFunction;

  // Determine the new function's proto. This must be done for singleton
  // functions.
  RootedObject proto(cx);
  if (!GetFunctionPrototype(cx, generatorKind, asyncKind, &proto)) {
    return nullptr;
  }

  gc::AllocKind allocKind = stencil.functionFlags.isExtended()
                                ? gc::AllocKind::FUNCTION_EXTENDED
                                : gc::AllocKind::FUNCTION;
  bool isAsmJS = stencil.functionFlags.isAsmJSNative();

  JSNative maybeNative = isAsmJS ? InstantiateAsmJS : nullptr;

  RootedAtom displayAtom(cx, stencil.functionAtom);
  RootedFunction fun(
      cx, NewFunctionWithProto(cx, maybeNative, stencil.nargs,
                               stencil.functionFlags, nullptr, displayAtom,
                               proto, allocKind, TenuredObject));
  if (!fun) {
    return nullptr;
  }

  if (isAsmJS) {
    RefPtr<const JS::WasmModule> asmJS =
        compilationInfo.asmJS.lookup(functionIndex)->value();

    JSObject* moduleObj = asmJS->createObjectForAsmJS(cx);
    if (!moduleObj) {
      return nullptr;
    }

    fun->setExtendedSlot(FunctionExtended::ASMJS_MODULE_SLOT,
                         ObjectValue(*moduleObj));
  }

  return fun;
}

static bool InstantiateScriptSourceObject(JSContext* cx,
                                          CompilationInfo& compilationInfo) {
  MOZ_ASSERT(compilationInfo.source());

  compilationInfo.sourceObject =
      ScriptSourceObject::create(cx, compilationInfo.source());
  if (!compilationInfo.sourceObject) {
    return false;
  }

  // Off-thread compilations do all their GC heap allocation, including the
  // SSO, in a temporary compartment. Hence, for the SSO to refer to the
  // gc-heap-allocated values in |options|, it would need cross-compartment
  // wrappers from the temporary compartment to the real compartment --- which
  // would then be inappropriate once we merged the temporary and real
  // compartments.
  //
  // Instead, we put off populating those SSO slots in off-thread compilations
  // until after we've merged compartments.
  if (!cx->isHelperThreadContext()) {
    if (!ScriptSourceObject::initFromOptions(cx, compilationInfo.sourceObject,
                                             compilationInfo.options)) {
      return false;
    }
  }

  return true;
}

// Instantiate ModuleObject if this is a module compile.
static bool MaybeInstantiateModule(JSContext* cx,
                                   CompilationInfo& compilationInfo) {
  if (compilationInfo.topLevel.get().isModule()) {
    compilationInfo.module = ModuleObject::create(cx);
    if (!compilationInfo.module) {
      return false;
    }

    if (!compilationInfo.moduleMetadata.get().initModule(
            cx, compilationInfo.module)) {
      return false;
    }
  }

  return true;
}

// Instantiate JSFunctions for each FunctionBox.
static bool InstantiateFunctions(JSContext* cx,
                                 CompilationInfo& compilationInfo) {
  for (auto item : compilationInfo.functionScriptStencils()) {
    auto& stencil = item.stencil;
    auto functionIndex = item.functionIndex;

    MOZ_ASSERT(!item.function);

    RootedFunction fun(
        cx, CreateFunction(cx, compilationInfo, stencil, functionIndex));
    if (!fun) {
      return false;
    }
    compilationInfo.functions[functionIndex].set(fun);
  }

  return true;
}

// Instantiate Scope for each ScopeCreationData.
//
// This should be called after InstantiateFunctions, given FunctionScope needs
// associated JSFunction pointer, and also should be called before
// InstantiateScriptStencils, given JSScript needs Scope pointer in gc things.
static bool InstantiateScopes(JSContext* cx, CompilationInfo& compilationInfo) {
  // While allocating Scope object from ScopeCreationData, Scope object
  // for the enclosing Scope should already be allocated.
  //
  // Enclosing scope of ScopeCreationData can be either ScopeCreationData or
  // Scope* pointer, capsulated by AbstractScopePtr.
  //
  // If the enclosing scope is ScopeCreationData, it's guaranteed to be
  // earlier element in compilationInfo.scopeCreationData, because
  // AbstractScopePtr holds index into it, and newly created ScopeCreaationData
  // is pushed back to the vector.
  for (auto& scd : compilationInfo.scopeCreationData) {
    if (!scd.createScope(cx, compilationInfo)) {
      return false;
    }
  }

  return true;
}

// JSFunctions have a default ObjectGroup when they are created. Once their
// enclosing script is compiled, we have more precise heuristic information and
// now compute their final group. These functions have not been exposed to
// script before this point.
static bool SetTypeForExposedFunctions(JSContext* cx,
                                       CompilationInfo& compilationInfo) {
  for (auto item : compilationInfo.functionScriptStencils()) {
    auto& stencil = item.stencil;
    auto& fun = item.function;
    if (!stencil.functionFlags.hasBaseScript()) {
      continue;
    }

    // If the function was not referenced by enclosing script's bytecode, we do
    // not generate a BaseScript for it. For example, `(function(){});`.
    if (!stencil.wasFunctionEmitted && !stencil.isStandaloneFunction) {
      continue;
    }

    if (!JSFunction::setTypeForScriptedFunction(cx, fun,
                                                stencil.isSingletonFunction)) {
      return false;
    }
  }

  return true;
}

// Instantiate js::BaseScripts from ScriptStencils for inner functions of the
// compilation. Note that standalone functions and functions being delazified
// are handled below with other top-levels.
static bool InstantiateScriptStencils(JSContext* cx,
                                      CompilationInfo& compilationInfo) {
  for (auto item : compilationInfo.functionScriptStencils()) {
    auto& stencil = item.stencil;
    auto& fun = item.function;
    if (stencil.immutableScriptData) {
      // If the function was not referenced by enclosing script's bytecode, we
      // do not generate a BaseScript for it. For example, `(function(){});`.
      if (!stencil.wasFunctionEmitted) {
        continue;
      }

      RootedScript script(
          cx, JSScript::fromStencil(cx, compilationInfo, stencil, fun));
      if (!script) {
        return false;
      }
    } else if (stencil.functionFlags.isAsmJSNative()) {
      MOZ_ASSERT(fun->isAsmJSNative());
    } else if (fun->isIncomplete()) {
      // Lazy functions are generally only allocated in the initial parse.
      MOZ_ASSERT(compilationInfo.lazy == nullptr);

      if (!CreateLazyScript(cx, compilationInfo, stencil, fun)) {
        return false;
      }
    }
  }

  return true;
}

// Instantiate the Stencil for the top-level script of the compilation. This
// includes standalone functions and functions being delazified.
static bool InstantiateTopLevel(JSContext* cx,
                                CompilationInfo& compilationInfo) {
  ScriptStencil& stencil = compilationInfo.topLevel.get();
  RootedFunction fun(cx);
  if (stencil.isFunction()) {
    fun = compilationInfo.functions[CompilationInfo::TopLevelFunctionIndex];
  }

  // Top-level asm.js does not generate a JSScript.
  if (stencil.functionFlags.isAsmJSNative()) {
    return true;
  }

  MOZ_ASSERT(stencil.immutableScriptData);

  if (compilationInfo.lazy) {
    compilationInfo.script = JSScript::CastFromLazy(compilationInfo.lazy);
    return JSScript::fullyInitFromStencil(cx, compilationInfo,
                                          compilationInfo.script, stencil, fun);
  }

  compilationInfo.script =
      JSScript::fromStencil(cx, compilationInfo, stencil, fun);
  if (!compilationInfo.script) {
    return false;
  }

  // Finish initializing the ModuleObject if needed.
  if (stencil.isModule()) {
    compilationInfo.module->initScriptSlots(compilationInfo.script);
    compilationInfo.module->initStatusSlot();

    if (!ModuleObject::createEnvironment(cx, compilationInfo.module)) {
      return false;
    }
  }

  return true;
}

// When a function is first referenced by enclosing script's bytecode, we need
// to update it with information determined by the BytecodeEmitter. This applies
// to both initial and delazification parses. The functions being update may or
// may not have bytecode at this point.
static void UpdateEmittedInnerFunctions(CompilationInfo& compilationInfo) {
  for (auto item : compilationInfo.functionScriptStencils()) {
    auto& stencil = item.stencil;
    auto& fun = item.function;
    if (!stencil.wasFunctionEmitted) {
      continue;
    }

    if (stencil.functionFlags.isAsmJSNative() ||
        fun->baseScript()->hasBytecode()) {
      // Non-lazy inner functions don't use the enclosingScope_ field.
      MOZ_ASSERT(stencil.lazyFunctionEnclosingScopeIndex_.isNothing());
    } else {
      // Apply updates from FunctionEmitter::emitLazy().
      BaseScript* script = fun->baseScript();

      ScopeIndex index = *stencil.lazyFunctionEnclosingScopeIndex_;
      Scope* scope = compilationInfo.scopeCreationData[index].get().getScope();
      script->setEnclosingScope(scope);
      script->initTreatAsRunOnce(stencil.immutableFlags.hasFlag(
          ImmutableScriptFlagsEnum::TreatAsRunOnce));

      if (stencil.fieldInitializers) {
        script->setFieldInitializers(*stencil.fieldInitializers);
      }
    }

    // Inferred and Guessed names are computed by BytecodeEmitter and so may
    // need to be applied to existing JSFunctions during delazification.
    if (fun->displayAtom() == nullptr) {
      if (stencil.functionFlags.hasInferredName()) {
        fun->setInferredName(stencil.functionAtom);
      }

      if (stencil.functionFlags.hasGuessedAtom()) {
        fun->setGuessedAtom(stencil.functionAtom);
      }
    }
  }
}

// During initial parse we must link lazy-functions-inside-lazy-functions to
// their enclosing script.
static void LinkEnclosingLazyScript(CompilationInfo& compilationInfo) {
  for (auto item : compilationInfo.functionScriptStencils()) {
    auto& stencil = item.stencil;
    auto& fun = item.function;
    if (!stencil.functionFlags.hasBaseScript()) {
      continue;
    }

    if (fun->baseScript()->hasBytecode()) {
      continue;
    }

    BaseScript* script = fun->baseScript();
    MOZ_ASSERT(!script->hasBytecode());

    for (auto inner : script->gcthings()) {
      if (!inner.is<JSObject>()) {
        continue;
      }
      inner.as<JSObject>().as<JSFunction>().setEnclosingLazyScript(script);
    }
  }
}

// When delazifying, use the existing JSFunctions. The initial and delazifying
// parse are required to generate the same sequence of functions for lazy
// parsing to work at all.
static void FunctionsFromExistingLazy(CompilationInfo& compilationInfo) {
  MOZ_ASSERT(compilationInfo.lazy);

  size_t idx = 0;
  compilationInfo.functions[idx++].set(compilationInfo.lazy->function());

  for (JS::GCCellPtr elem : compilationInfo.lazy->gcthings()) {
    if (!elem.is<JSObject>()) {
      continue;
    }
    compilationInfo.functions[idx++].set(&elem.as<JSObject>().as<JSFunction>());
  }

  MOZ_ASSERT(idx == compilationInfo.functions.length());
}

bool CompilationInfo::instantiateStencils() {
  if (!functions.resize(funcData.length())) {
    return false;
  }

  if (lazy) {
    FunctionsFromExistingLazy(*this);
  } else {
    if (!InstantiateScriptSourceObject(cx, *this)) {
      return false;
    }

    if (!MaybeInstantiateModule(cx, *this)) {
      return false;
    }

    if (!InstantiateFunctions(cx, *this)) {
      return false;
    }
  }

  if (!InstantiateScopes(cx, *this)) {
    return false;
  }

  if (!SetTypeForExposedFunctions(cx, *this)) {
    return false;
  }

  if (!InstantiateScriptStencils(cx, *this)) {
    return false;
  }

  if (!InstantiateTopLevel(cx, *this)) {
    return false;
  }

  // Must be infallible from here forward.

  UpdateEmittedInnerFunctions(*this);

  if (lazy == nullptr) {
    LinkEnclosingLazyScript(*this);
  }

  return true;
}
