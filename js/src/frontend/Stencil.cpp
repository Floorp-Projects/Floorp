/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "frontend/Stencil.h"

#include "mozilla/RefPtr.h"  // RefPtr

#include <memory>  // std::uninitialized_fill

#include "frontend/AbstractScopePtr.h"  // ScopeIndex
#include "frontend/BytecodeSection.h"   // EmitScriptThingsVector
#include "frontend/CompilationInfo.h"  // CompilationInfo, CompilationInfoVector, CompilationGCOutput
#include "frontend/SharedContext.h"
#include "gc/AllocKind.h"    // gc::AllocKind
#include "js/CallArgs.h"     // JSNative
#include "js/RootingAPI.h"   // Rooted
#include "js/Transcoding.h"  // JS::TranscodeBuffer
#include "js/Value.h"        // ObjectValue
#include "js/WasmModule.h"   // JS::WasmModule
#include "vm/EnvironmentObject.h"
#include "vm/GeneratorAndAsyncKind.h"  // GeneratorKind, FunctionAsyncKind
#include "vm/JSContext.h"              // JSContext
#include "vm/JSFunction.h"  // JSFunction, GetFunctionPrototype, NewFunctionWithProto
#include "vm/JSObject.h"      // JSObject
#include "vm/JSONPrinter.h"   // js::JSONPrinter
#include "vm/JSScript.h"      // BaseScript, JSScript
#include "vm/ObjectGroup.h"   // TenuredObject
#include "vm/Printer.h"       // js::Fprinter
#include "vm/Scope.h"         // Scope, ScopeKindString
#include "vm/StencilEnums.h"  // ImmutableScriptFlagsEnum
#include "vm/StringType.h"    // JSAtom, js::CopyChars
#include "vm/Xdr.h"           // XDRMode, XDRResult, XDREncoder
#include "wasm/AsmJS.h"       // InstantiateAsmJS
#include "wasm/WasmModule.h"  // wasm::Module

using namespace js;
using namespace js::frontend;

AbstractScopePtr ScopeStencil::enclosing(
    CompilationInfo& compilationInfo) const {
  if (enclosing_) {
    return AbstractScopePtr(compilationInfo, *enclosing_);
  }

  return AbstractScopePtr(compilationInfo.input.enclosingScope);
}

Scope* ScopeStencil::enclosingExistingScope(
    const CompilationInput& input, const CompilationGCOutput& gcOutput) const {
  if (enclosing_) {
    Scope* result = gcOutput.scopes[*enclosing_];
    MOZ_ASSERT(result, "Scope must already exist to use this method");
    return result;
  }

  return input.enclosingScope;
}

Scope* ScopeStencil::createScope(JSContext* cx, CompilationInput& input,
                                 CompilationGCOutput& gcOutput) const {
  Scope* scope = nullptr;
  switch (kind()) {
    case ScopeKind::Function: {
      scope =
          createSpecificScope<FunctionScope, CallObject>(cx, input, gcOutput);
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
          cx, input, gcOutput);
      break;
    }
    case ScopeKind::FunctionBodyVar: {
      scope = createSpecificScope<VarScope, VarEnvironmentObject>(cx, input,
                                                                  gcOutput);
      break;
    }
    case ScopeKind::Global:
    case ScopeKind::NonSyntactic: {
      scope =
          createSpecificScope<GlobalScope, std::nullptr_t>(cx, input, gcOutput);
      break;
    }
    case ScopeKind::Eval:
    case ScopeKind::StrictEval: {
      scope = createSpecificScope<EvalScope, VarEnvironmentObject>(cx, input,
                                                                   gcOutput);
      break;
    }
    case ScopeKind::Module: {
      scope = createSpecificScope<ModuleScope, ModuleEnvironmentObject>(
          cx, input, gcOutput);
      break;
    }
    case ScopeKind::With: {
      scope =
          createSpecificScope<WithScope, std::nullptr_t>(cx, input, gcOutput);
      break;
    }
    case ScopeKind::WasmFunction:
    case ScopeKind::WasmInstance: {
      MOZ_CRASH("Unexpected deferred type");
    }
  }
  return scope;
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

static bool CreateLazyScript(JSContext* cx, CompilationInput& input,
                             CompilationStencil& stencil,
                             CompilationGCOutput& gcOutput,
                             const ScriptStencil& script,
                             HandleFunction function) {
  Rooted<ScriptSourceObject*> sourceObject(cx, gcOutput.sourceObject);
  size_t ngcthings = script.gcThings.size();

  Rooted<BaseScript*> lazy(
      cx, BaseScript::CreateRawLazy(cx, ngcthings, function, sourceObject,
                                    script.extent, script.immutableFlags));
  if (!lazy) {
    return false;
  }

  if (!EmitScriptThingsVector(cx, input, stencil, gcOutput, script.gcThings,
                              lazy->gcthingsForInit())) {
    return false;
  }

  function->initScript(lazy);

  return true;
}

static JSFunction* CreateFunction(JSContext* cx, CompilationInput& input,
                                  CompilationStencil& stencil,
                                  const ScriptStencil& script,
                                  FunctionIndex functionIndex) {
  GeneratorKind generatorKind =
      script.immutableFlags.hasFlag(ImmutableScriptFlagsEnum::IsGenerator)
          ? GeneratorKind::Generator
          : GeneratorKind::NotGenerator;
  FunctionAsyncKind asyncKind =
      script.immutableFlags.hasFlag(ImmutableScriptFlagsEnum::IsAsync)
          ? FunctionAsyncKind::AsyncFunction
          : FunctionAsyncKind::SyncFunction;

  // Determine the new function's proto. This must be done for singleton
  // functions.
  RootedObject proto(cx);
  if (!GetFunctionPrototype(cx, generatorKind, asyncKind, &proto)) {
    return nullptr;
  }

  gc::AllocKind allocKind = script.functionFlags.isExtended()
                                ? gc::AllocKind::FUNCTION_EXTENDED
                                : gc::AllocKind::FUNCTION;
  bool isAsmJS = script.functionFlags.isAsmJSNative();

  JSNative maybeNative = isAsmJS ? InstantiateAsmJS : nullptr;

  RootedAtom displayAtom(cx);
  if (script.functionAtom) {
    displayAtom.set(input.atomCache.getExistingAtomAt(cx, script.functionAtom));
    MOZ_ASSERT(displayAtom);
  }
  RootedFunction fun(
      cx, NewFunctionWithProto(cx, maybeNative, script.nargs,
                               script.functionFlags, nullptr, displayAtom,
                               proto, allocKind, TenuredObject));
  if (!fun) {
    return nullptr;
  }

  if (isAsmJS) {
    RefPtr<const JS::WasmModule> asmJS =
        stencil.asmJS.lookup(functionIndex)->value();

    JSObject* moduleObj = asmJS->createObjectForAsmJS(cx);
    if (!moduleObj) {
      return nullptr;
    }

    fun->setExtendedSlot(FunctionExtended::ASMJS_MODULE_SLOT,
                         ObjectValue(*moduleObj));
  }

  return fun;
}

static bool InstantiateAtoms(JSContext* cx, CompilationInput& input,
                             CompilationStencil& stencil) {
  return InstantiateMarkedAtoms(cx, stencil.parserAtomData, input.atomCache);
}

static bool InstantiateScriptSourceObject(JSContext* cx,
                                          CompilationInput& input,
                                          CompilationGCOutput& gcOutput) {
  MOZ_ASSERT(input.source());

  gcOutput.sourceObject = ScriptSourceObject::create(cx, input.source());
  if (!gcOutput.sourceObject) {
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
    Rooted<ScriptSourceObject*> sourceObject(cx, gcOutput.sourceObject);
    if (!ScriptSourceObject::initFromOptions(cx, sourceObject, input.options)) {
      return false;
    }
  }

  return true;
}

// Instantiate ModuleObject if this is a module compile.
static bool MaybeInstantiateModule(JSContext* cx, CompilationInput& input,
                                   CompilationStencil& stencil,
                                   CompilationGCOutput& gcOutput) {
  if (stencil.scriptData[CompilationInfo::TopLevelIndex].isModule()) {
    gcOutput.module = ModuleObject::create(cx);
    if (!gcOutput.module) {
      return false;
    }

    Rooted<ModuleObject*> module(cx, gcOutput.module);
    if (!stencil.moduleMetadata->initModule(cx, input.atomCache, module)) {
      return false;
    }
  }

  return true;
}

// Instantiate JSFunctions for each FunctionBox.
static bool InstantiateFunctions(JSContext* cx, CompilationInput& input,
                                 CompilationStencil& stencil,
                                 CompilationGCOutput& gcOutput) {
  for (auto item : CompilationInfo::functionScriptStencils(stencil, gcOutput)) {
    auto& scriptStencil = item.script;
    auto functionIndex = item.functionIndex;

    MOZ_ASSERT(!item.function);

    JSFunction* fun =
        CreateFunction(cx, input, stencil, scriptStencil, functionIndex);
    if (!fun) {
      return false;
    }
    gcOutput.functions[functionIndex] = fun;
  }

  return true;
}

// Instantiate Scope for each ScopeStencil.
//
// This should be called after InstantiateFunctions, given FunctionScope needs
// associated JSFunction pointer, and also should be called before
// InstantiateScriptStencils, given JSScript needs Scope pointer in gc things.
static bool InstantiateScopes(JSContext* cx, CompilationInput& input,
                              CompilationStencil& stencil,
                              CompilationGCOutput& gcOutput) {
  // While allocating Scope object from ScopeStencil, Scope object for the
  // enclosing Scope should already be allocated.
  //
  // Enclosing scope of ScopeStencil can be either ScopeStencil or Scope*
  // pointer.
  //
  // If the enclosing scope is ScopeStencil, it's guaranteed to be earlier
  // element in compilationInfo.scopeData, because enclosing_ field holds index
  // into it, and newly created ScopeStencil is pushed back to the vector.
  //
  // If the enclosing scope is Scope*, it's CompilationInput.enclosingScope.

  for (const ScopeStencil& scd : stencil.scopeData) {
    Scope* scope = scd.createScope(cx, input, gcOutput);
    if (!scope) {
      return false;
    }
    gcOutput.scopes.infallibleAppend(scope);
  }

  return true;
}

// Instantiate js::BaseScripts from ScriptStencils for inner functions of the
// compilation. Note that standalone functions and functions being delazified
// are handled below with other top-levels.
static bool InstantiateScriptStencils(JSContext* cx, CompilationInput& input,
                                      CompilationStencil& stencil,
                                      CompilationGCOutput& gcOutput) {
  MOZ_ASSERT(input.lazy == nullptr);

  Rooted<JSFunction*> fun(cx);
  for (auto item : CompilationInfo::functionScriptStencils(stencil, gcOutput)) {
    auto& scriptStencil = item.script;
    fun = item.function;
    if (scriptStencil.sharedData) {
      // If the function was not referenced by enclosing script's bytecode, we
      // do not generate a BaseScript for it. For example, `(function(){});`.
      //
      // `wasFunctionEmitted` is false also for standalone functions. They are
      // handled in InstantiateTopLevel.
      if (!scriptStencil.wasFunctionEmitted) {
        continue;
      }

      RootedScript script(
          cx, JSScript::fromStencil(cx, input, stencil, gcOutput, scriptStencil,
                                    fun));
      if (!script) {
        return false;
      }

      // NOTE: Inner functions can be marked `allowRelazify` after merging
      // a stencil for delazification into the top-level stencil.
      if (scriptStencil.allowRelazify) {
        MOZ_ASSERT(script->isRelazifiable());
        script->setAllowRelazify();
      }
    } else if (scriptStencil.functionFlags.isAsmJSNative()) {
      MOZ_ASSERT(fun->isAsmJSNative());
    } else {
      MOZ_ASSERT(fun->isIncomplete());
      if (!CreateLazyScript(cx, input, stencil, gcOutput, scriptStencil, fun)) {
        return false;
      }
    }
  }

  return true;
}

// Instantiate the Stencil for the top-level script of the compilation. This
// includes standalone functions and functions being delazified.
static bool InstantiateTopLevel(JSContext* cx, CompilationInput& input,
                                CompilationStencil& stencil,
                                CompilationGCOutput& gcOutput) {
  const ScriptStencil& scriptStencil =
      stencil.scriptData[CompilationInfo::TopLevelIndex];
  RootedFunction fun(cx);
  if (scriptStencil.isFunction()) {
    fun = gcOutput.functions[CompilationInfo::TopLevelIndex];
  }

  // Top-level asm.js does not generate a JSScript.
  if (scriptStencil.functionFlags.isAsmJSNative()) {
    return true;
  }

  MOZ_ASSERT(scriptStencil.sharedData);

  if (input.lazy) {
    gcOutput.script = JSScript::CastFromLazy(input.lazy);

    Rooted<JSScript*> script(cx, gcOutput.script);
    if (!JSScript::fullyInitFromStencil(cx, input, stencil, gcOutput, script,
                                        scriptStencil, fun)) {
      return false;
    }

    if (scriptStencil.allowRelazify) {
      MOZ_ASSERT(gcOutput.script->isRelazifiable());
      gcOutput.script->setAllowRelazify();
    }

    return true;
  }

  gcOutput.script =
      JSScript::fromStencil(cx, input, stencil, gcOutput, scriptStencil, fun);
  if (!gcOutput.script) {
    return false;
  }

  if (scriptStencil.allowRelazify) {
    MOZ_ASSERT(gcOutput.script->isRelazifiable());
    gcOutput.script->setAllowRelazify();
  }

  // Finish initializing the ModuleObject if needed.
  if (scriptStencil.isModule()) {
    Rooted<JSScript*> script(cx, gcOutput.script);

    gcOutput.module->initScriptSlots(script);
    gcOutput.module->initStatusSlot();

    Rooted<ModuleObject*> module(cx, gcOutput.module);
    if (!ModuleObject::createEnvironment(cx, module)) {
      return false;
    }

    // Off-thread compilation with parseGlobal will freeze the module object
    // in GlobalHelperThreadState::finishSingleParseTask instead.
    if (!cx->isHelperThreadContext()) {
      if (!ModuleObject::Freeze(cx, module)) {
        return false;
      }
    }
  }

  return true;
}

// When a function is first referenced by enclosing script's bytecode, we need
// to update it with information determined by the BytecodeEmitter. This applies
// to both initial and delazification parses. The functions being update may or
// may not have bytecode at this point.
static void UpdateEmittedInnerFunctions(JSContext* cx, CompilationInput& input,
                                        CompilationStencil& stencil,
                                        CompilationGCOutput& gcOutput) {
  for (auto item : CompilationInfo::functionScriptStencils(stencil, gcOutput)) {
    auto& scriptStencil = item.script;
    auto& fun = item.function;
    if (!scriptStencil.wasFunctionEmitted) {
      continue;
    }

    if (scriptStencil.functionFlags.isAsmJSNative() ||
        fun->baseScript()->hasBytecode()) {
      // Non-lazy inner functions don't use the enclosingScope_ field.
      MOZ_ASSERT(scriptStencil.lazyFunctionEnclosingScopeIndex_.isNothing());
    } else {
      // Apply updates from FunctionEmitter::emitLazy().
      BaseScript* script = fun->baseScript();

      ScopeIndex index = *scriptStencil.lazyFunctionEnclosingScopeIndex_;
      Scope* scope = gcOutput.scopes[index];
      script->setEnclosingScope(scope);

      if (scriptStencil.memberInitializers) {
        script->setMemberInitializers(*scriptStencil.memberInitializers);
      }

      // Inferred and Guessed names are computed by BytecodeEmitter and so may
      // need to be applied to existing JSFunctions during delazification.
      if (fun->displayAtom() == nullptr) {
        JSAtom* funcAtom = nullptr;
        if (scriptStencil.functionFlags.hasInferredName() ||
            scriptStencil.functionFlags.hasGuessedAtom()) {
          funcAtom =
              input.atomCache.getExistingAtomAt(cx, scriptStencil.functionAtom);
          MOZ_ASSERT(funcAtom);
        }
        if (scriptStencil.functionFlags.hasInferredName()) {
          fun->setInferredName(funcAtom);
        }
        if (scriptStencil.functionFlags.hasGuessedAtom()) {
          fun->setGuessedAtom(funcAtom);
        }
      }
    }
  }
}

// During initial parse we must link lazy-functions-inside-lazy-functions to
// their enclosing script.
static void LinkEnclosingLazyScript(CompilationStencil& stencil,
                                    CompilationGCOutput& gcOutput) {
  for (auto item : CompilationInfo::functionScriptStencils(stencil, gcOutput)) {
    auto& scriptStencil = item.script;
    auto& fun = item.function;
    if (!scriptStencil.functionFlags.hasBaseScript()) {
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

#ifdef DEBUG
// Some fields aren't used in delazification, given the target functions and
// scripts are already instantiated, but they still should match.
static void AssertDelazificationFieldsMatch(CompilationStencil& stencil,
                                            CompilationGCOutput& gcOutput) {
  for (auto item : CompilationInfo::functionScriptStencils(stencil, gcOutput)) {
    auto& scriptStencil = item.script;
    auto& fun = item.function;

    BaseScript* script = fun->baseScript();

    MOZ_ASSERT(script->immutableFlags() == scriptStencil.immutableFlags);

    MOZ_ASSERT(script->extent().sourceStart ==
               scriptStencil.extent.sourceStart);
    MOZ_ASSERT(script->extent().sourceEnd == scriptStencil.extent.sourceEnd);
    MOZ_ASSERT(script->extent().toStringStart ==
               scriptStencil.extent.toStringStart);
    MOZ_ASSERT(script->extent().toStringEnd ==
               scriptStencil.extent.toStringEnd);
    MOZ_ASSERT(script->extent().lineno == scriptStencil.extent.lineno);
    MOZ_ASSERT(script->extent().column == scriptStencil.extent.column);

    // Names are updated by UpdateInnerFunctions.
    constexpr uint16_t HAS_INFERRED_NAME =
        uint16_t(FunctionFlags::Flags::HAS_INFERRED_NAME);
    constexpr uint16_t HAS_GUESSED_ATOM =
        uint16_t(FunctionFlags::Flags::HAS_GUESSED_ATOM);
    constexpr uint16_t MUTABLE_FLAGS =
        uint16_t(FunctionFlags::Flags::MUTABLE_FLAGS);
    constexpr uint16_t acceptableDifferenceForFunction =
        HAS_INFERRED_NAME | HAS_GUESSED_ATOM | MUTABLE_FLAGS;

    MOZ_ASSERT((fun->flags().toRaw() | acceptableDifferenceForFunction) ==
               (scriptStencil.functionFlags.toRaw() |
                acceptableDifferenceForFunction));

    // Delazification shouldn't delazify inner scripts.
    MOZ_ASSERT_IF(item.functionIndex == 0, scriptStencil.sharedData);
    MOZ_ASSERT_IF(item.functionIndex > 0, !scriptStencil.sharedData);

    // FIXME: If this function is lazily parsed again, nargs isn't set to
    //        correct value (bug 1666978).
    MOZ_ASSERT_IF(scriptStencil.sharedData,
                  fun->nargs() == scriptStencil.nargs);
  }
}
#endif  // DEBUG

// When delazifying, use the existing JSFunctions. The initial and delazifying
// parse are required to generate the same sequence of functions for lazy
// parsing to work at all.
static void FunctionsFromExistingLazy(CompilationInput& input,
                                      CompilationGCOutput& gcOutput) {
  MOZ_ASSERT(input.lazy);

  size_t idx = 0;
  gcOutput.functions[idx++] = input.lazy->function();

  for (JS::GCCellPtr elem : input.lazy->gcthings()) {
    if (!elem.is<JSObject>()) {
      continue;
    }
    gcOutput.functions[idx++] = &elem.as<JSObject>().as<JSFunction>();
  }

  MOZ_ASSERT(idx == gcOutput.functions.length());
}

/* static */
bool CompilationInfo::instantiateStencils(JSContext* cx,
                                          CompilationInfo& compilationInfo,
                                          CompilationGCOutput& gcOutput) {
  if (!compilationInfo.preparationIsPerformed) {
    if (!prepareForInstantiate(cx, compilationInfo, gcOutput)) {
      return false;
    }
  }

  return instantiateStencilsAfterPreparation(cx, compilationInfo.input,
                                             compilationInfo.stencil, gcOutput);
}

/* static */
bool CompilationInfo::instantiateStencilsAfterPreparation(
    JSContext* cx, CompilationInput& input, CompilationStencil& stencil,
    CompilationGCOutput& gcOutput) {
  if (!gcOutput.functions.resize(stencil.scriptData.length())) {
    ReportOutOfMemory(cx);
    return false;
  }

  if (!InstantiateAtoms(cx, input, stencil)) {
    return false;
  }

  if (input.lazy) {
    MOZ_ASSERT(!stencil.scriptData[CompilationInfo::TopLevelIndex].isModule());

    FunctionsFromExistingLazy(input, gcOutput);

#ifdef DEBUG
    AssertDelazificationFieldsMatch(stencil, gcOutput);
#endif
  } else {
    if (stencil.scriptData[CompilationInfo::TopLevelIndex].isModule()) {
      MOZ_ASSERT(input.enclosingScope == nullptr);
      input.enclosingScope = &cx->global()->emptyGlobalScope();
      MOZ_ASSERT(input.enclosingScope->environmentChainLength() ==
                 ModuleScope::EnclosingEnvironmentChainLength);
    }

    if (!InstantiateScriptSourceObject(cx, input, gcOutput)) {
      return false;
    }

    if (!MaybeInstantiateModule(cx, input, stencil, gcOutput)) {
      return false;
    }

    if (!InstantiateFunctions(cx, input, stencil, gcOutput)) {
      return false;
    }
  }

  if (!InstantiateScopes(cx, input, stencil, gcOutput)) {
    return false;
  }

  if (!input.lazy) {
    if (!InstantiateScriptStencils(cx, input, stencil, gcOutput)) {
      return false;
    }
  }

  if (!InstantiateTopLevel(cx, input, stencil, gcOutput)) {
    return false;
  }

  // Must be infallible from here forward.

  UpdateEmittedInnerFunctions(cx, input, stencil, gcOutput);

  if (!input.lazy) {
    LinkEnclosingLazyScript(stencil, gcOutput);
  }

  return true;
}

bool CompilationInfoVector::buildDelazificationIndices(JSContext* cx) {
  // Standalone-functions are not supported by XDR.
  MOZ_ASSERT(!initial.stencil.scriptData[0].isFunction());

  // If no delazifications, we are done.
  if (delazifications.empty()) {
    return true;
  }

  if (!delazificationIndices.resize(delazifications.length())) {
    ReportOutOfMemory(cx);
    return false;
  }

  HashMap<FunctionKey, size_t> keyToIndex(cx);
  if (!keyToIndex.reserve(delazifications.length())) {
    return false;
  }

  for (size_t i = 0; i < delazifications.length(); i++) {
    const auto& delazification = delazifications[i];
    auto key = toFunctionKey(delazification.scriptData[0].extent);
    keyToIndex.putNewInfallible(key, i);
  }

  MOZ_ASSERT(keyToIndex.count() == delazifications.length());

  for (size_t i = 1; i < initial.stencil.scriptData.length(); i++) {
    auto key = toFunctionKey(initial.stencil.scriptData[i].extent);
    auto ptr = keyToIndex.lookup(key);
    if (!ptr) {
      continue;
    }
    delazificationIndices[ptr->value()] = FunctionIndex(i);
  }

  return true;
}

bool CompilationInfoVector::instantiateStencils(
    JSContext* cx, CompilationGCOutput& gcOutput,
    CompilationGCOutput& gcOutputForDelazification) {
  if (!prepareForInstantiate(cx, gcOutput, gcOutputForDelazification)) {
    return false;
  }

  return instantiateStencilsAfterPreparation(cx, gcOutput,
                                             gcOutputForDelazification);
}

bool CompilationInfoVector::instantiateStencilsAfterPreparation(
    JSContext* cx, CompilationGCOutput& gcOutput,
    CompilationGCOutput& gcOutputForDelazification) {
  if (!CompilationInfo::instantiateStencilsAfterPreparation(
          cx, initial.input, initial.stencil, gcOutput)) {
    return false;
  }

  for (size_t i = 0; i < delazifications.length(); i++) {
    auto& delazification = delazifications[i];
    auto index = delazificationIndices[i];

    JSFunction* fun = gcOutput.functions[index];
    MOZ_ASSERT(fun);

    BaseScript* lazy = fun->baseScript();
    MOZ_ASSERT(!lazy->hasBytecode());

    Rooted<CompilationInput> input(cx, CompilationInput(initial.input.options));
    input.get().initFromLazy(lazy);

    input.get().atomCache.stealBuffer(delazificationAtomCache);

    if (!CompilationInfo::prepareGCOutputForInstantiate(
            cx, delazification, gcOutputForDelazification)) {
      return false;
    }
    if (!CompilationInfo::instantiateStencilsAfterPreparation(
            cx, input.get(), delazification, gcOutputForDelazification)) {
      return false;
    }

    // Destroy elements, without unreserving.
    gcOutputForDelazification.functions.clear();
    gcOutputForDelazification.scopes.clear();

    input.get().atomCache.returnBuffer(delazificationAtomCache);
  }

  return true;
}

/* static */
bool CompilationInfo::prepareInputAndStencilForInstantiate(
    JSContext* cx, CompilationInput& input, CompilationStencil& stencil) {
  if (!input.atomCache.allocate(cx, stencil.parserAtomData.length())) {
    return false;
  }

  return true;
}

/* static */
bool CompilationInfo::prepareGCOutputForInstantiate(
    JSContext* cx, CompilationStencil& stencil, CompilationGCOutput& gcOutput) {
  if (!gcOutput.functions.reserve(stencil.scriptData.length())) {
    ReportOutOfMemory(cx);
    return false;
  }
  if (!gcOutput.scopes.reserve(stencil.scopeData.length())) {
    ReportOutOfMemory(cx);
    return false;
  }

  return true;
}

/* static */
bool CompilationInfo::prepareForInstantiate(JSContext* cx,
                                            CompilationInfo& compilationInfo,
                                            CompilationGCOutput& gcOutput) {
  auto& input = compilationInfo.input;
  auto& stencil = compilationInfo.stencil;

  if (!prepareInputAndStencilForInstantiate(cx, input, stencil)) {
    return false;
  }
  if (!prepareGCOutputForInstantiate(cx, stencil, gcOutput)) {
    return false;
  }

  compilationInfo.preparationIsPerformed = true;
  return true;
}

bool CompilationInfoVector::prepareForInstantiate(
    JSContext* cx, CompilationGCOutput& gcOutput,
    CompilationGCOutput& gcOutputForDelazification) {
  if (!CompilationInfo::prepareForInstantiate(cx, initial, gcOutput)) {
    return false;
  }

  size_t maxScriptDataLength = 0;
  size_t maxScopeDataLength = 0;
  size_t maxParserAtomDataLength = 0;
  for (auto& delazification : delazifications) {
    if (maxParserAtomDataLength < delazification.parserAtomData.length()) {
      maxParserAtomDataLength = delazification.parserAtomData.length();
    }
    if (maxScriptDataLength < delazification.scriptData.length()) {
      maxScriptDataLength = delazification.scriptData.length();
    }
    if (maxScopeDataLength < delazification.scopeData.length()) {
      maxScopeDataLength = delazification.scopeData.length();
    }
  }

  if (!delazificationAtomCache.resize(maxParserAtomDataLength)) {
    ReportOutOfMemory(cx);
    return false;
  }
  if (!gcOutput.functions.reserve(maxScriptDataLength)) {
    ReportOutOfMemory(cx);
    return false;
  }
  if (!gcOutput.scopes.reserve(maxScopeDataLength)) {
    ReportOutOfMemory(cx);
    return false;
  }

  if (!buildDelazificationIndices(cx)) {
    return false;
  }

  return true;
}

bool CompilationInfo::serializeStencils(JSContext* cx, JS::TranscodeBuffer& buf,
                                        bool* succeededOut) {
  if (succeededOut) {
    *succeededOut = false;
  }
  XDRIncrementalStencilEncoder encoder(cx);

  XDRResult res = encoder.codeStencil(*this);
  if (res.isErr()) {
    if (res.unwrapErr() & JS::TranscodeResult_Failure) {
      buf.clear();
      return true;
    }
    MOZ_ASSERT(res.unwrapErr() == JS::TranscodeResult_Throw);

    return false;
  }

  // Lineareize the endcoder, return empty buffer on failure.
  res = encoder.linearize(buf);
  if (res.isErr()) {
    MOZ_ASSERT(cx->isThrowingOutOfMemory());
    buf.clear();
    return false;
  }

  if (succeededOut) {
    *succeededOut = true;
  }
  return true;
}

bool CompilationInfoVector::deserializeStencils(JSContext* cx,
                                                const JS::TranscodeRange& range,
                                                bool* succeededOut) {
  if (succeededOut) {
    *succeededOut = false;
  }
  MOZ_ASSERT(initial.stencil.parserAtomData.empty());
  XDRStencilDecoder decoder(cx, &initial.input.options, range);

  XDRResult res = decoder.codeStencils(*this);
  if (res.isErr()) {
    if (res.unwrapErr() & JS::TranscodeResult_Failure) {
      return true;
    }
    MOZ_ASSERT(res.unwrapErr() == JS::TranscodeResult_Throw);

    return false;
  }

  if (succeededOut) {
    *succeededOut = true;
  }
  return true;
}

CompilationState::CompilationState(JSContext* cx,
                                   LifoAllocScope& frontendAllocScope,
                                   const JS::ReadOnlyCompileOptions& options,
                                   CompilationInfo& compilationInfo,
                                   Scope* enclosingScope /* = nullptr */,
                                   JSObject* enclosingEnv /* = nullptr */)
    : directives(options.forceStrictMode()),
      scopeContext(cx, enclosingScope, enclosingEnv),
      usedNames(cx),
      allocScope(frontendAllocScope),
      parserAtoms(cx->runtime(), compilationInfo.alloc,
                  compilationInfo.stencil.parserAtomData) {}

#if defined(DEBUG) || defined(JS_JITSPEW)

void frontend::DumpTaggedParserAtomIndex(
    js::JSONPrinter& json, TaggedParserAtomIndex taggedIndex,
    CompilationStencil* compilationStencil) {
  if (taggedIndex.isParserAtomIndex()) {
    json.property("tag", "AtomIndex");
    auto index = taggedIndex.toParserAtomIndex();
    if (compilationStencil && compilationStencil->parserAtomData[index]) {
      GenericPrinter& out = json.beginStringProperty("atom");
      compilationStencil->parserAtomData[index]->dumpCharsNoQuote(out);
      json.endString();
    } else {
      json.property("index", size_t(index));
    }
    return;
  }

  if (taggedIndex.isWellKnownAtomId()) {
    json.property("tag", "WellKnown");
    auto index = taggedIndex.toWellKnownAtomId();
    switch (index) {
      case WellKnownAtomId::empty:
        json.property("atom", "");
        break;

#  define CASE_(_, name, _2)                                  \
    case WellKnownAtomId::name: {                             \
      GenericPrinter& out = json.beginStringProperty("atom"); \
      WellKnownParserAtoms::rom_.name.dumpCharsNoQuote(out);  \
      json.endString();                                       \
      break;                                                  \
    }
        FOR_EACH_NONTINY_COMMON_PROPERTYNAME(CASE_)
#  undef CASE_

#  define CASE_(name, _)                                      \
    case WellKnownAtomId::name: {                             \
      GenericPrinter& out = json.beginStringProperty("atom"); \
      WellKnownParserAtoms::rom_.name.dumpCharsNoQuote(out);  \
      json.endString();                                       \
      break;                                                  \
    }
        JS_FOR_EACH_PROTOTYPE(CASE_)
#  undef CASE_

      default:
        // This includes tiny WellKnownAtomId atoms, which is invalid.
        json.property("index", size_t(index));
        break;
    }
    return;
  }

  if (taggedIndex.isStaticParserString1()) {
    json.property("tag", "Static1");
    auto index = taggedIndex.toStaticParserString1();
    GenericPrinter& out = json.beginStringProperty("atom");
    WellKnownParserAtoms::getStatic1(index)->dumpCharsNoQuote(out);
    json.endString();
    return;
  }

  if (taggedIndex.isStaticParserString2()) {
    json.property("tag", "Static2");
    auto index = taggedIndex.toStaticParserString2();
    GenericPrinter& out = json.beginStringProperty("atom");
    WellKnownParserAtoms::getStatic2(index)->dumpCharsNoQuote(out);
    json.endString();
    return;
  }

  MOZ_ASSERT(taggedIndex.isNull());
  json.property("tag", "null");
}

void RegExpStencil::dump() {
  js::Fprinter out(stderr);
  js::JSONPrinter json(out);
  dump(json, nullptr);
}

void RegExpStencil::dump(js::JSONPrinter& json,
                         CompilationStencil* compilationStencil) {
  json.beginObject();
  dumpFields(json, compilationStencil);
  json.endObject();
}

void RegExpStencil::dumpFields(js::JSONPrinter& json,
                               CompilationStencil* compilationStencil) {
  json.beginObjectProperty("pattern");
  DumpTaggedParserAtomIndex(json, atom_, compilationStencil);
  json.endObject();

  GenericPrinter& out = json.beginStringProperty("flags");

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

  json.endStringProperty();
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

  AbstractTrailingNamesArray<const ParserAtom>* trailingNames = nullptr;
  uint32_t length = 0;

  switch (kind_) {
    case ScopeKind::Function: {
      auto* data = static_cast<ParserFunctionScopeData*>(data_);
      json.property("nextFrameSlot", data->nextFrameSlot);
      json.property("hasParameterExprs", data->hasParameterExprs);
      json.property("nonPositionalFormalStart", data->nonPositionalFormalStart);
      json.property("varStart", data->varStart);

      trailingNames = &data->trailingNames;
      length = data->length;
      break;
    }

    case ScopeKind::FunctionBodyVar: {
      auto* data = static_cast<ParserVarScopeData*>(data_);
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
      auto* data = static_cast<ParserLexicalScopeData*>(data_);
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
      auto* data = static_cast<ParserEvalScopeData*>(data_);
      json.property("nextFrameSlot", data->nextFrameSlot);

      trailingNames = &data->trailingNames;
      length = data->length;
      break;
    }

    case ScopeKind::Global:
    case ScopeKind::NonSyntactic: {
      auto* data = static_cast<ParserGlobalScopeData*>(data_);
      json.property("letStart", data->letStart);
      json.property("constStart", data->constStart);

      trailingNames = &data->trailingNames;
      length = data->length;
      break;
    }

    case ScopeKind::Module: {
      auto* data = static_cast<ParserModuleScopeData*>(data_);
      json.property("nextFrameSlot", data->nextFrameSlot);
      json.property("varStart", data->varStart);
      json.property("letStart", data->letStart);
      json.property("constStart", data->constStart);

      trailingNames = &data->trailingNames;
      length = data->length;
      break;
    }

    case ScopeKind::WasmInstance: {
      auto* data =
          static_cast<AbstractScopeData<WasmInstanceScope, const ParserAtom>*>(
              data_);
      json.property("nextFrameSlot", data->nextFrameSlot);
      json.property("globalsStart", data->globalsStart);

      trailingNames = &data->trailingNames;
      length = data->length;
      break;
    }

    case ScopeKind::WasmFunction: {
      auto* data =
          static_cast<AbstractScopeData<WasmFunctionScope, const ParserAtom>*>(
              data_);
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
    js::JSONPrinter& json, const StencilModuleMetadata::EntryVector& entries,
    CompilationStencil* compilationStencil) {
  for (const auto& entry : entries) {
    json.beginObject();
    if (entry.specifier) {
      json.beginObjectProperty("specifier");
      DumpTaggedParserAtomIndex(json, entry.specifier, compilationStencil);
      json.endObject();
    }
    if (entry.localName) {
      json.beginObjectProperty("localName");
      DumpTaggedParserAtomIndex(json, entry.localName, compilationStencil);
      json.endObject();
    }
    if (entry.importName) {
      json.beginObjectProperty("importName");
      DumpTaggedParserAtomIndex(json, entry.importName, compilationStencil);
      json.endObject();
    }
    if (entry.exportName) {
      json.beginObjectProperty("exportName");
      DumpTaggedParserAtomIndex(json, entry.exportName, compilationStencil);
      json.endObject();
    }
    json.endObject();
  }
}

void StencilModuleMetadata::dump() {
  js::Fprinter out(stderr);
  js::JSONPrinter json(out);
  dump(json, nullptr);
}

void StencilModuleMetadata::dump(js::JSONPrinter& json,
                                 CompilationStencil* compilationStencil) {
  json.beginObject();
  dumpFields(json, compilationStencil);
  json.endObject();
}

void StencilModuleMetadata::dumpFields(js::JSONPrinter& json,
                                       CompilationStencil* compilationStencil) {
  json.beginListProperty("requestedModules");
  DumpModuleEntryVectorItems(json, requestedModules, compilationStencil);
  json.endList();

  json.beginListProperty("importEntries");
  DumpModuleEntryVectorItems(json, importEntries, compilationStencil);
  json.endList();

  json.beginListProperty("localExportEntries");
  DumpModuleEntryVectorItems(json, localExportEntries, compilationStencil);
  json.endList();

  json.beginListProperty("indirectExportEntries");
  DumpModuleEntryVectorItems(json, indirectExportEntries, compilationStencil);
  json.endList();

  json.beginListProperty("starExportEntries");
  DumpModuleEntryVectorItems(json, starExportEntries, compilationStencil);
  json.endList();

  json.beginListProperty("functionDecls");
  for (auto& index : functionDecls) {
    json.value("FunctionIndex(%zu)", size_t(index));
  }
  json.endList();

  json.boolProperty("isAsync", isAsync);
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
        default:
          json.value("Unknown(%x)", i);
          break;
      }
    }
  }
}

static void DumpScriptThing(js::JSONPrinter& json,
                            CompilationStencil* compilationStencil,
                            TaggedScriptThingIndex& thing) {
  switch (thing.tag()) {
    case TaggedScriptThingIndex::Kind::ParserAtomIndex:
    case TaggedScriptThingIndex::Kind::WellKnown:
      json.beginObject();
      json.property("type", "Atom");
      DumpTaggedParserAtomIndex(json, thing.toAtom(), compilationStencil);
      json.endObject();
      break;
    case TaggedScriptThingIndex::Kind::Null:
      json.nullValue();
      break;
    case TaggedScriptThingIndex::Kind::BigInt:
      json.value("BigIntIndex(%zu)", size_t(thing.toBigInt()));
      break;
    case TaggedScriptThingIndex::Kind::ObjLiteral:
      json.value("ObjLiteralIndex(%zu)", size_t(thing.toObjLiteral()));
      break;
    case TaggedScriptThingIndex::Kind::RegExp:
      json.value("RegExpIndex(%zu)", size_t(thing.toRegExp()));
      break;
    case TaggedScriptThingIndex::Kind::Scope:
      json.value("ScopeIndex(%zu)", size_t(thing.toScope()));
      break;
    case TaggedScriptThingIndex::Kind::Function:
      json.value("FunctionIndex(%zu)", size_t(thing.toFunction()));
      break;
    case TaggedScriptThingIndex::Kind::EmptyGlobalScope:
      json.value("EmptyGlobalScope");
      break;
  }
}

void ScriptStencil::dump() {
  js::Fprinter out(stderr);
  js::JSONPrinter json(out);
  dump(json, nullptr);
}

void ScriptStencil::dump(js::JSONPrinter& json,
                         CompilationStencil* compilationStencil) {
  json.beginObject();
  dumpFields(json, compilationStencil);
  json.endObject();
}

void ScriptStencil::dumpFields(js::JSONPrinter& json,
                               CompilationStencil* compilationStencil) {
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
    DumpScriptThing(json, compilationStencil, thing);
  }
  json.endList();

  if (sharedData) {
    json.formatProperty("sharedData", "u8[%zu]",
                        sharedData->immutableDataLength());
  } else {
    json.nullProperty("sharedData");
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
    json.beginObjectProperty("functionAtom");
    DumpTaggedParserAtomIndex(json, functionAtom, compilationStencil);
    json.endObject();

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

    json.boolProperty("wasFunctionEmitted", wasFunctionEmitted);
    json.boolProperty("allowRelazify", allowRelazify);
  }
}

void CompilationStencil::dump() {
  js::Fprinter out(stderr);
  js::JSONPrinter json(out);
  dump(json);
  out.put("\n");
}

void CompilationStencil::dump(js::JSONPrinter& json) {
  json.beginObject();

  // FIXME: dump asmJS

  json.beginListProperty("scriptData");
  for (auto& data : scriptData) {
    data.dump(json, this);
  }
  json.endList();

  json.beginListProperty("regExpData");
  for (auto& data : regExpData) {
    data.dump(json, this);
  }
  json.endList();

  json.beginListProperty("bigIntData");
  for (auto& data : bigIntData) {
    data.dump(json);
  }
  json.endList();

  json.beginListProperty("objLiteralData");
  for (auto& data : objLiteralData) {
    data.dump(json, this);
  }
  json.endList();

  json.beginListProperty("scopeData");
  for (auto& data : scopeData) {
    data.dump(json);
  }
  json.endList();

  if (scriptData[CompilationInfo::TopLevelIndex].isModule()) {
    json.beginObjectProperty("moduleMetadata");
    moduleMetadata->dumpFields(json, this);
    json.endObject();
  }

  json.endObject();
}

#endif  // defined(DEBUG) || defined(JS_JITSPEW)

mozilla::Span<TaggedScriptThingIndex>
js::frontend::NewScriptThingSpanUninitialized(JSContext* cx, LifoAlloc& alloc,
                                              uint32_t ngcthings) {
  MOZ_ASSERT(ngcthings > 0);

  TaggedScriptThingIndex* stencilThings =
      alloc.newArrayUninitialized<TaggedScriptThingIndex>(ngcthings);
  if (!stencilThings) {
    js::ReportOutOfMemory(cx);
    return mozilla::Span<TaggedScriptThingIndex>();
  }

  return mozilla::Span<TaggedScriptThingIndex>(stencilThings, ngcthings);
}

JSAtom* CompilationAtomCache::getExistingAtomAt(ParserAtomIndex index) const {
  return atoms_[index];
}

JSAtom* CompilationAtomCache::getExistingAtomAt(
    JSContext* cx, TaggedParserAtomIndex taggedIndex) const {
  if (taggedIndex.isParserAtomIndex()) {
    auto index = taggedIndex.toParserAtomIndex();
    return getExistingAtomAt(index);
  }

  if (taggedIndex.isWellKnownAtomId()) {
    auto index = taggedIndex.toWellKnownAtomId();
    return GetWellKnownAtom(cx, index);
  }

  if (taggedIndex.isStaticParserString1()) {
    auto index = taggedIndex.toStaticParserString1();
    return cx->staticStrings().getUnit(char16_t(index));
  }

  MOZ_ASSERT(taggedIndex.isStaticParserString2());
  auto index = taggedIndex.toStaticParserString2();
  return cx->staticStrings().getLength2FromIndex(size_t(index));
}

JSAtom* CompilationAtomCache::getAtomAt(ParserAtomIndex index) const {
  if (size_t(index) >= atoms_.length()) {
    return nullptr;
  }
  return atoms_[index];
}

bool CompilationAtomCache::hasAtomAt(ParserAtomIndex index) const {
  if (size_t(index) >= atoms_.length()) {
    return false;
  }
  return !!atoms_[index];
}

bool CompilationAtomCache::setAtomAt(JSContext* cx, ParserAtomIndex index,
                                     JSAtom* atom) {
  if (size_t(index) < atoms_.length()) {
    atoms_[index] = atom;
    return true;
  }

  if (!atoms_.resize(size_t(index) + 1)) {
    ReportOutOfMemory(cx);
    return false;
  }

  atoms_[index] = atom;
  return true;
}

bool CompilationAtomCache::allocate(JSContext* cx, size_t length) {
  MOZ_ASSERT(length >= atoms_.length());
  if (length == atoms_.length()) {
    return true;
  }

  if (!atoms_.resize(length)) {
    ReportOutOfMemory(cx);
    return false;
  }

  return true;
}

void CompilationAtomCache::stealBuffer(AtomCacheVector& atoms) {
  atoms_ = std::move(atoms);
  // Destroy elements, without unreserving.
  atoms_.clear();
}

void CompilationAtomCache::returnBuffer(AtomCacheVector& atoms) {
  atoms = std::move(atoms_);
}

const ParserAtom* CompilationStencil::getParserAtomAt(
    JSContext* cx, TaggedParserAtomIndex taggedIndex) const {
  if (taggedIndex.isParserAtomIndex()) {
    auto index = taggedIndex.toParserAtomIndex();
    MOZ_ASSERT(index < parserAtomData.length());
    return parserAtomData[index]->asAtom();
  }

  if (taggedIndex.isWellKnownAtomId()) {
    auto index = taggedIndex.toWellKnownAtomId();
    return cx->runtime()->commonParserNames->getWellKnown(index);
  }

  if (taggedIndex.isStaticParserString1()) {
    auto index = taggedIndex.toStaticParserString1();
    return WellKnownParserAtoms::getStatic1(index);
  }

  MOZ_ASSERT(taggedIndex.isStaticParserString2());
  auto index = taggedIndex.toStaticParserString2();
  return WellKnownParserAtoms::getStatic2(index);
}
