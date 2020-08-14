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
#include "vm/JSONPrinter.h"  // js::JSONPrinter
#include "vm/JSScript.h"     // BaseScript, JSScript
#include "vm/ObjectGroup.h"  // TenuredObject
#include "vm/Printer.h"      // js::Fprinter
#include "vm/Scope.h"  // Scope, CreateEnvironmentShape, EmptyEnvironmentShape, ScopeKindString
#include "vm/StencilEnums.h"  // ImmutableScriptFlagsEnum
#include "vm/StringType.h"    // JSAtom, js::CopyChars
#include "wasm/AsmJS.h"       // InstantiateAsmJS
#include "wasm/WasmModule.h"  // wasm::Module

using namespace js;
using namespace js::frontend;

bool frontend::RegExpStencil::init(JSContext* cx, JSAtom* pattern,
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

AbstractScopePtr ScopeStencil::enclosing(CompilationInfo& compilationInfo) {
  if (enclosing_) {
    return AbstractScopePtr(compilationInfo, *enclosing_);
  }

  // HACK: The self-hosting script uses the EmptyGlobalScopeType placeholder
  // which does not correspond to a ScopeStencil. This means that the inner
  // scopes may store Nothing as an enclosing ScopeIndex.
  if (compilationInfo.options.selfHostingMode) {
    MOZ_ASSERT(compilationInfo.enclosingScope == nullptr);
    return AbstractScopePtr(&compilationInfo.cx->global()->emptyGlobalScope());
  }

  return AbstractScopePtr(compilationInfo.enclosingScope);
}

Scope* ScopeStencil::createScope(JSContext* cx,
                                 CompilationInfo& compilationInfo) {
  Scope* scope = nullptr;
  switch (kind()) {
    case ScopeKind::Function: {
      scope =
          createSpecificScope<FunctionScope, CallObject>(cx, compilationInfo);
      break;
    }
    case ScopeKind::Lexical:
    case ScopeKind::SimpleCatch:
    case ScopeKind::Catch:
    case ScopeKind::NamedLambda:
    case ScopeKind::StrictNamedLambda:
    case ScopeKind::FunctionLexical:
    case ScopeKind::ClassBody: {
      scope = createSpecificScope<LexicalScope, LexicalEnvironmentObject>(
          cx, compilationInfo);
      break;
    }
    case ScopeKind::FunctionBodyVar: {
      scope = createSpecificScope<VarScope, VarEnvironmentObject>(
          cx, compilationInfo);
      break;
    }
    case ScopeKind::Global:
    case ScopeKind::NonSyntactic: {
      scope =
          createSpecificScope<GlobalScope, std::nullptr_t>(cx, compilationInfo);
      break;
    }
    case ScopeKind::Eval:
    case ScopeKind::StrictEval: {
      scope = createSpecificScope<EvalScope, VarEnvironmentObject>(
          cx, compilationInfo);
      break;
    }
    case ScopeKind::Module: {
      scope = createSpecificScope<ModuleScope, ModuleEnvironmentObject>(
          cx, compilationInfo);
      break;
    }
    case ScopeKind::With: {
      scope =
          createSpecificScope<WithScope, std::nullptr_t>(cx, compilationInfo);
      break;
    }
    case ScopeKind::WasmFunction:
    case ScopeKind::WasmInstance: {
      MOZ_CRASH("Unexpected deferred type");
    }
  }
  return scope;
}

void ScopeStencil::trace(JSTracer* trc) {
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

uint32_t ScopeStencil::nextFrameSlot() const {
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

// Instantiate Scope for each ScopeStencil.
//
// This should be called after InstantiateFunctions, given FunctionScope needs
// associated JSFunction pointer, and also should be called before
// InstantiateScriptStencils, given JSScript needs Scope pointer in gc things.
static bool InstantiateScopes(JSContext* cx, CompilationInfo& compilationInfo) {
  // While allocating Scope object from ScopeStencil, Scope object for the
  // enclosing Scope should already be allocated.
  //
  // Enclosing scope of ScopeStencil can be either ScopeStencil or Scope*
  // pointer, capsulated by AbstractScopePtr.
  //
  // If the enclosing scope is ScopeStencil, it's guaranteed to be earlier
  // element in compilationInfo.scopeData, because AbstractScopePtr holds index
  // into it, and newly created ScopeStencil is pushed back to the vector.

  if (!compilationInfo.scopes.reserve(compilationInfo.scopeData.length())) {
    return false;
  }

  for (auto& scd : compilationInfo.scopeData) {
    Scope* scope = scd.createScope(cx, compilationInfo);
    if (!scope) {
      return false;
    }
    compilationInfo.scopes.infallibleAppend(scope);
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
      Scope* scope = compilationInfo.scopes[index].get();
      script->setEnclosingScope(scope);
      script->initTreatAsRunOnce(stencil.immutableFlags.hasFlag(
          ImmutableScriptFlagsEnum::TreatAsRunOnce));

      if (stencil.memberInitializers) {
        script->setMemberInitializers(*stencil.memberInitializers);
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

#if defined(DEBUG) || defined(JS_JITSPEW)

void RegExpStencil::dump() {
  js::Fprinter out(stderr);
  js::JSONPrinter json(out);
  dump(json);
}

void RegExpStencil::dump(js::JSONPrinter& json) {
  GenericPrinter& out = json.beginString();

  out.put("/");
  for (size_t i = 0; i < length_; i++) {
    char16_t c = buf_[i];
    if (c == '\n') {
      out.put("\\n");
    } else if (c == '\t') {
      out.put("\\t");
    } else if (c >= 32 && c < 127) {
      out.putChar(char(buf_[i]));
    } else if (c <= 255) {
      out.printf("\\x%02x", unsigned(c));
    } else {
      out.printf("\\u%04x", unsigned(c));
    }
  }
  out.put("/");

  if (flags_.global()) {
    out.put("g");
  }
  if (flags_.ignoreCase()) {
    out.put("i");
  }
  if (flags_.multiline()) {
    out.put("m");
  }
  if (flags_.dotAll()) {
    out.put("s");
  }
  if (flags_.unicode()) {
    out.put("u");
  }
  if (flags_.sticky()) {
    out.put("y");
  }

  json.endString();
}

void BigIntStencil::dump() {
  js::Fprinter out(stderr);
  js::JSONPrinter json(out);
  dump(json);
}

void BigIntStencil::dump(js::JSONPrinter& json) {
  GenericPrinter& out = json.beginString();

  for (size_t i = 0; i < length_; i++) {
    out.putChar(char(buf_[i]));
  }

  json.endString();
}

void ScopeStencil::dump() {
  js::Fprinter out(stderr);
  js::JSONPrinter json(out);
  dump(json);
}

void ScopeStencil::dump(js::JSONPrinter& json) {
  json.beginObject();
  dumpFields(json);
  json.endObject();
}

void ScopeStencil::dumpFields(js::JSONPrinter& json) {
  if (enclosing_) {
    json.formatProperty("enclosing", "Some(ScopeIndex(%zu))",
                        size_t(*enclosing_));
  } else {
    json.property("enclosing", "Nothing");
  }

  json.property("kind", ScopeKindString(kind_));

  json.property("firstFrameSlot", firstFrameSlot_);

  if (numEnvironmentSlots_) {
    json.formatProperty("numEnvironmentSlots", "Some(%zu)",
                        size_t(*numEnvironmentSlots_));
  } else {
    json.property("numEnvironmentSlots", "Nothing");
  }

  if (kind_ == ScopeKind::Function) {
    if (functionIndex_.isSome()) {
      json.formatProperty("functionIndex", "FunctionIndex(%zu)",
                          size_t(*functionIndex_));
    } else {
      json.property("functionIndex", "Nothing");
    }

    json.boolProperty("isArrow", isArrow_);
  }

  json.beginObjectProperty("data");

  AbstractTrailingNamesArray<JSAtom>* trailingNames = nullptr;
  uint32_t length = 0;

  switch (kind_) {
    case ScopeKind::Function: {
      auto* data = static_cast<FunctionScope::Data*>(data_.get());
      json.property("nextFrameSlot", data->nextFrameSlot);
      json.property("hasParameterExprs", data->hasParameterExprs);
      json.property("nonPositionalFormalStart", data->nonPositionalFormalStart);
      json.property("varStart", data->varStart);

      trailingNames = &data->trailingNames;
      length = data->length;
      break;
    }

    case ScopeKind::FunctionBodyVar: {
      auto* data = static_cast<VarScope::Data*>(data_.get());
      json.property("nextFrameSlot", data->nextFrameSlot);

      trailingNames = &data->trailingNames;
      length = data->length;
      break;
    }

    case ScopeKind::Lexical:
    case ScopeKind::SimpleCatch:
    case ScopeKind::Catch:
    case ScopeKind::NamedLambda:
    case ScopeKind::StrictNamedLambda:
    case ScopeKind::FunctionLexical:
    case ScopeKind::ClassBody: {
      auto* data = static_cast<LexicalScope::Data*>(data_.get());
      json.property("nextFrameSlot", data->nextFrameSlot);
      json.property("constStart", data->constStart);

      trailingNames = &data->trailingNames;
      length = data->length;
      break;
    }

    case ScopeKind::With: {
      break;
    }

    case ScopeKind::Eval:
    case ScopeKind::StrictEval: {
      auto* data = static_cast<EvalScope::Data*>(data_.get());
      json.property("nextFrameSlot", data->nextFrameSlot);

      trailingNames = &data->trailingNames;
      length = data->length;
      break;
    }

    case ScopeKind::Global:
    case ScopeKind::NonSyntactic: {
      auto* data = static_cast<GlobalScope::Data*>(data_.get());
      json.property("letStart", data->letStart);
      json.property("constStart", data->constStart);

      trailingNames = &data->trailingNames;
      length = data->length;
      break;
    }

    case ScopeKind::Module: {
      auto* data = static_cast<ModuleScope::Data*>(data_.get());
      json.property("nextFrameSlot", data->nextFrameSlot);
      json.property("varStart", data->varStart);
      json.property("letStart", data->letStart);
      json.property("constStart", data->constStart);

      trailingNames = &data->trailingNames;
      length = data->length;
      break;
    }

    case ScopeKind::WasmInstance: {
      auto* data = static_cast<WasmInstanceScope::Data*>(data_.get());
      json.property("nextFrameSlot", data->nextFrameSlot);
      json.property("globalsStart", data->globalsStart);

      trailingNames = &data->trailingNames;
      length = data->length;
      break;
    }

    case ScopeKind::WasmFunction: {
      auto* data = static_cast<WasmFunctionScope::Data*>(data_.get());
      json.property("nextFrameSlot", data->nextFrameSlot);

      trailingNames = &data->trailingNames;
      length = data->length;
      break;
    }

    default: {
      MOZ_CRASH("Unexpected ScopeKind");
      break;
    }
  }

  if (trailingNames) {
    json.beginListProperty("trailingNames");
    for (size_t i = 0; i < length; i++) {
      auto& name = (*trailingNames)[i];
      json.beginObject();

      json.boolProperty("closedOver", name.closedOver());

      json.boolProperty("isTopLevelFunction", name.isTopLevelFunction());

      GenericPrinter& out = json.beginStringProperty("name");
      name.name()->dumpCharsNoQuote(out);
      json.endStringProperty();

      json.endObject();
    }
    json.endList();
  }

  json.endObject();
}

static void DumpModuleEntryVectorItems(
    js::JSONPrinter& json, const StencilModuleMetadata::EntryVector& entries) {
  for (const auto& entry : entries) {
    json.beginObject();
    if (entry.specifier) {
      GenericPrinter& out = json.beginStringProperty("specifier");
      entry.specifier->dumpCharsNoQuote(out);
      json.endStringProperty();
    }
    if (entry.localName) {
      GenericPrinter& out = json.beginStringProperty("localName");
      entry.localName->dumpCharsNoQuote(out);
      json.endStringProperty();
    }
    if (entry.importName) {
      GenericPrinter& out = json.beginStringProperty("importName");
      entry.importName->dumpCharsNoQuote(out);
      json.endStringProperty();
    }
    if (entry.exportName) {
      GenericPrinter& out = json.beginStringProperty("exportName");
      entry.exportName->dumpCharsNoQuote(out);
      json.endStringProperty();
    }
    json.endObject();
  }
}

void StencilModuleMetadata::dump() {
  js::Fprinter out(stderr);
  js::JSONPrinter json(out);
  dump(json);
}

void StencilModuleMetadata::dump(js::JSONPrinter& json) {
  json.beginObject();
  dumpFields(json);
  json.endObject();
}

void StencilModuleMetadata::dumpFields(js::JSONPrinter& json) {
  json.beginListProperty("requestedModules");
  DumpModuleEntryVectorItems(json, requestedModules);
  json.endList();

  json.beginListProperty("importEntries");
  DumpModuleEntryVectorItems(json, importEntries);
  json.endList();

  json.beginListProperty("localExportEntries");
  DumpModuleEntryVectorItems(json, localExportEntries);
  json.endList();

  json.beginListProperty("indirectExportEntries");
  DumpModuleEntryVectorItems(json, indirectExportEntries);
  json.endList();

  json.beginListProperty("starExportEntries");
  DumpModuleEntryVectorItems(json, starExportEntries);
  json.endList();

  json.beginListProperty("functionDecls");
  for (auto& index : functionDecls) {
    json.value("FunctionIndex(%zu)", size_t(index));
  }
  json.endList();
}

static void DumpImmutableScriptFlags(js::JSONPrinter& json,
                                     ImmutableScriptFlags immutableFlags) {
  for (uint32_t i = 1; i; i = i << 1) {
    if (uint32_t(immutableFlags) & i) {
      switch (ImmutableScriptFlagsEnum(i)) {
        case ImmutableScriptFlagsEnum::IsForEval:
          json.value("IsForEval");
          break;
        case ImmutableScriptFlagsEnum::IsModule:
          json.value("IsModule");
          break;
        case ImmutableScriptFlagsEnum::IsFunction:
          json.value("IsFunction");
          break;
        case ImmutableScriptFlagsEnum::SelfHosted:
          json.value("SelfHosted");
          break;
        case ImmutableScriptFlagsEnum::ForceStrict:
          json.value("ForceStrict");
          break;
        case ImmutableScriptFlagsEnum::HasNonSyntacticScope:
          json.value("HasNonSyntacticScope");
          break;
        case ImmutableScriptFlagsEnum::NoScriptRval:
          json.value("NoScriptRval");
          break;
        case ImmutableScriptFlagsEnum::TreatAsRunOnce:
          json.value("TreatAsRunOnce");
          break;
        case ImmutableScriptFlagsEnum::Strict:
          json.value("Strict");
          break;
        case ImmutableScriptFlagsEnum::HasModuleGoal:
          json.value("HasModuleGoal");
          break;
        case ImmutableScriptFlagsEnum::HasInnerFunctions:
          json.value("HasInnerFunctions");
          break;
        case ImmutableScriptFlagsEnum::HasDirectEval:
          json.value("HasDirectEval");
          break;
        case ImmutableScriptFlagsEnum::BindingsAccessedDynamically:
          json.value("BindingsAccessedDynamically");
          break;
        case ImmutableScriptFlagsEnum::HasCallSiteObj:
          json.value("HasCallSiteObj");
          break;
        case ImmutableScriptFlagsEnum::IsAsync:
          json.value("IsAsync");
          break;
        case ImmutableScriptFlagsEnum::IsGenerator:
          json.value("IsGenerator");
          break;
        case ImmutableScriptFlagsEnum::FunHasExtensibleScope:
          json.value("FunHasExtensibleScope");
          break;
        case ImmutableScriptFlagsEnum::FunctionHasThisBinding:
          json.value("FunctionHasThisBinding");
          break;
        case ImmutableScriptFlagsEnum::NeedsHomeObject:
          json.value("NeedsHomeObject");
          break;
        case ImmutableScriptFlagsEnum::IsDerivedClassConstructor:
          json.value("IsDerivedClassConstructor");
          break;
        case ImmutableScriptFlagsEnum::IsFieldInitializer:
          json.value("IsFieldInitializer");
          break;
        case ImmutableScriptFlagsEnum::HasRest:
          json.value("HasRest");
          break;
        case ImmutableScriptFlagsEnum::NeedsFunctionEnvironmentObjects:
          json.value("NeedsFunctionEnvironmentObjects");
          break;
        case ImmutableScriptFlagsEnum::FunctionHasExtraBodyVarScope:
          json.value("FunctionHasExtraBodyVarScope");
          break;
        case ImmutableScriptFlagsEnum::ShouldDeclareArguments:
          json.value("ShouldDeclareArguments");
          break;
        case ImmutableScriptFlagsEnum::ArgumentsHasVarBinding:
          json.value("ArgumentsHasVarBinding");
          break;
        case ImmutableScriptFlagsEnum::AlwaysNeedsArgsObj:
          json.value("AlwaysNeedsArgsObj");
          break;
        case ImmutableScriptFlagsEnum::HasMappedArgsObj:
          json.value("HasMappedArgsObj");
          break;
        case ImmutableScriptFlagsEnum::IsLikelyConstructorWrapper:
          json.value("IsLikelyConstructorWrapper");
          break;
        default:
          json.value("Unknown(%x)", i);
          break;
      }
    }
  }
}

static void DumpFunctionFlagsItems(js::JSONPrinter& json,
                                   FunctionFlags functionFlags) {
  switch (functionFlags.kind()) {
    case FunctionFlags::FunctionKind::NormalFunction:
      json.value("NORMAL_KIND");
      break;
    case FunctionFlags::FunctionKind::AsmJS:
      json.value("ASMJS_KIND");
      break;
    case FunctionFlags::FunctionKind::Wasm:
      json.value("WASM_KIND");
      break;
    case FunctionFlags::FunctionKind::Arrow:
      json.value("ARROW_KIND");
      break;
    case FunctionFlags::FunctionKind::Method:
      json.value("METHOD_KIND");
      break;
    case FunctionFlags::FunctionKind::ClassConstructor:
      json.value("CLASSCONSTRUCTOR_KIND");
      break;
    case FunctionFlags::FunctionKind::Getter:
      json.value("GETTER_KIND");
      break;
    case FunctionFlags::FunctionKind::Setter:
      json.value("SETTER_KIND");
      break;
    default:
      json.value("Unknown(%x)", uint8_t(functionFlags.kind()));
      break;
  }

  static_assert(FunctionFlags::FUNCTION_KIND_MASK == 0x0007,
                "FunctionKind should use the lowest 3 bits");
  for (uint16_t i = 1 << 3; i; i = i << 1) {
    if (functionFlags.toRaw() & i) {
      switch (FunctionFlags::Flags(i)) {
        case FunctionFlags::Flags::EXTENDED:
          json.value("EXTENDED");
          break;
        case FunctionFlags::Flags::SELF_HOSTED:
          json.value("SELF_HOSTED");
          break;
        case FunctionFlags::Flags::BASESCRIPT:
          json.value("BASESCRIPT");
          break;
        case FunctionFlags::Flags::SELFHOSTLAZY:
          json.value("SELFHOSTLAZY");
          break;
        case FunctionFlags::Flags::CONSTRUCTOR:
          json.value("CONSTRUCTOR");
          break;
        case FunctionFlags::Flags::BOUND_FUN:
          json.value("BOUND_FUN");
          break;
        case FunctionFlags::Flags::LAMBDA:
          json.value("LAMBDA");
          break;
        case FunctionFlags::Flags::WASM_JIT_ENTRY:
          json.value("WASM_JIT_ENTRY");
          break;
        case FunctionFlags::Flags::HAS_INFERRED_NAME:
          json.value("HAS_INFERRED_NAME");
          break;
        case FunctionFlags::Flags::ATOM_EXTRA_FLAG:
          json.value("ATOM_EXTRA_FLAG");
          break;
        case FunctionFlags::Flags::RESOLVED_NAME:
          json.value("RESOLVED_NAME");
          break;
        case FunctionFlags::Flags::RESOLVED_LENGTH:
          json.value("RESOLVED_LENGTH");
          break;
        case FunctionFlags::Flags::NEW_SCRIPT_CLEARED:
          json.value("NEW_SCRIPT_CLEARED");
          break;
        default:
          json.value("Unknown(%x)", i);
          break;
      }
    }
  }
}

static void DumpScriptThing(js::JSONPrinter& json, ScriptThingVariant& thing) {
  struct Matcher {
    js::JSONPrinter& json;

    void operator()(ScriptAtom& data) {
      json.beginObject();
      json.property("type", "ScriptAtom");
      JSAtom* atom = data;
      GenericPrinter& out = json.beginStringProperty("value");
      atom->dumpCharsNoQuote(out);
      json.endStringProperty();
      json.endObject();
    }

    void operator()(NullScriptThing& data) { json.nullValue(); }

    void operator()(BigIntIndex& index) {
      json.value("BigIntIndex(%zu)", size_t(index));
    }

    void operator()(RegExpIndex& index) {
      json.value("RegExpIndex(%zu)", size_t(index));
    }

    void operator()(ObjLiteralIndex& index) {
      json.value("ObjLiteralIndex(%zu)", size_t(index));
    }

    void operator()(ScopeIndex& index) {
      json.value("ScopeIndex(%zu)", size_t(index));
    }

    void operator()(FunctionIndex& index) {
      json.value("FunctionIndex(%zu)", size_t(index));
    }

    void operator()(EmptyGlobalScopeType& emptyGlobalScope) {
      json.value("EmptyGlobalScope");
    }
  };

  Matcher m{json};
  thing.match(m);
}

void ScriptStencil::dump() {
  js::Fprinter out(stderr);
  js::JSONPrinter json(out);
  dump(json);
}

void ScriptStencil::dump(js::JSONPrinter& json) {
  json.beginObject();
  dumpFields(json);
  json.endObject();
}

void ScriptStencil::dumpFields(js::JSONPrinter& json) {
  json.beginListProperty("immutableFlags");
  DumpImmutableScriptFlags(json, immutableFlags);
  json.endList();

  if (memberInitializers) {
    json.formatProperty("memberInitializers", "Some(%u)",
                        (*memberInitializers).numMemberInitializers);
  } else {
    json.property("memberInitializers", "Nothing");
  }

  json.beginListProperty("gcThings");
  for (auto& thing : gcThings) {
    DumpScriptThing(json, thing);
  }
  json.endList();

  if (immutableScriptData) {
    json.formatProperty("immutableScriptData", "u8[%zu]",
                        immutableScriptData->immutableData().Length());
  } else {
    json.nullProperty("immutableScriptData");
  }

  json.beginObjectProperty("extent");
  json.property("sourceStart", extent.sourceStart);
  json.property("sourceEnd", extent.sourceEnd);
  json.property("toStringStart", extent.toStringStart);
  json.property("toStringEnd", extent.toStringEnd);
  json.property("lineno", extent.lineno);
  json.property("column", extent.column);
  json.endObject();

  if (isFunction()) {
    if (functionAtom) {
      GenericPrinter& out = json.beginStringProperty("functionAtom");
      functionAtom->dumpCharsNoQuote(out);
      json.endStringProperty();
    } else {
      json.nullProperty("functionAtom");
    }

    json.beginListProperty("functionFlags");
    DumpFunctionFlagsItems(json, functionFlags);
    json.endList();

    json.property("nargs", nargs);

    if (lazyFunctionEnclosingScopeIndex_) {
      json.formatProperty("lazyFunctionEnclosingScopeIndex",
                          "Some(ScopeIndex(%zu))",
                          size_t(*lazyFunctionEnclosingScopeIndex_));
    } else {
      json.property("lazyFunctionEnclosingScopeIndex", "Nothing");
    }

    json.boolProperty("isStandaloneFunction", isStandaloneFunction);
    json.boolProperty("wasFunctionEmitted", wasFunctionEmitted);
    json.boolProperty("isSingletonFunction", isSingletonFunction);
  }
}

void CompilationInfo::dumpStencil() {
  js::Fprinter out(stderr);
  js::JSONPrinter json(out);
  dumpStencil(json);
  out.put("\n");
}

void CompilationInfo::dumpStencil(js::JSONPrinter& json) {
  json.beginObject();

  json.beginObjectProperty("topLevel");
  topLevel.get().dumpFields(json);
  json.endObject();

  // FIXME: dump asmJS

  json.beginListProperty("funcData");
  for (size_t i = 0; i < funcData.length(); i++) {
    funcData[i].get().dump(json);
  }
  json.endList();

  json.beginListProperty("regExpData");
  for (auto& data : regExpData) {
    data.dump(json);
  }
  json.endList();

  json.beginListProperty("bigIntData");
  for (auto& data : bigIntData) {
    data.dump(json);
  }
  json.endList();

  json.beginListProperty("objLiteralData");
  for (auto& data : objLiteralData) {
    data.dump(json);
  }
  json.endList();

  json.beginListProperty("scopeData");
  for (auto& data : scopeData) {
    data.dump(json);
  }
  json.endList();

  if (topLevel.get().isModule()) {
    json.beginObjectProperty("moduleMetadata");
    moduleMetadata.get().dumpFields(json);
    json.endObject();
  }

  json.endObject();
}

#endif  // defined(DEBUG) || defined(JS_JITSPEW)
