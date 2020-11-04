/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef frontend_CompilationInfo_h
#define frontend_CompilationInfo_h

#include "mozilla/Attributes.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Span.h"
#include "mozilla/Variant.h"

#include "builtin/ModuleObject.h"
#include "ds/LifoAlloc.h"
#include "frontend/ParserAtom.h"
#include "frontend/SharedContext.h"
#include "frontend/Stencil.h"
#include "frontend/UsedNameTracker.h"
#include "js/GCVariant.h"
#include "js/GCVector.h"
#include "js/HashTable.h"
#include "js/RealmOptions.h"
#include "js/SourceText.h"
#include "js/Transcoding.h"
#include "js/Vector.h"
#include "js/WasmModule.h"
#include "vm/GlobalObject.h"  // GlobalObject
#include "vm/JSContext.h"
#include "vm/JSFunction.h"  // JSFunction
#include "vm/JSScript.h"    // SourceExtent
#include "vm/Realm.h"

namespace js {

class JSONPrinter;

namespace frontend {

// ScopeContext hold information derivied from the scope and environment chains
// to try to avoid the parser needing to traverse VM structures directly.
struct ScopeContext {
  // Whether the enclosing scope allows certain syntax. Eval and arrow scripts
  // inherit this from their enclosing scipt so we track it here.
  bool allowNewTarget = false;
  bool allowSuperProperty = false;
  bool allowSuperCall = false;
  bool allowArguments = true;

  // The type of binding required for `this` of the top level context, as
  // indicated by the enclosing scopes of this parse.
  ThisBinding thisBinding = ThisBinding::Global;

  // Somewhere on the scope chain this parse is embedded within is a 'With'
  // scope.
  bool inWith = false;

  // Somewhere on the scope chain this parse is embedded within a class scope.
  bool inClass = false;

  // Class field initializer info if we are nested within a class constructor.
  // We may be an combination of arrow and eval context within the constructor.
  mozilla::Maybe<MemberInitializers> memberInitializers = {};

  // If this eval is in response to Debugger.Frame.eval, we may have an
  // incomplete scope chain. In order to determine a better 'this' binding, as
  // well as to ensure we can provide better static error semantics for private
  // names, we use the environment chain to attempt to find a more effective
  // scope than the enclosing scope.
  // If there is no more effective scope, this will just be the scope given in
  // the constructor.
  JS::Rooted<Scope*> effectiveScope;

  explicit ScopeContext(JSContext* cx, Scope* scope,
                        JSObject* enclosingEnv = nullptr)
      : effectiveScope(cx, determineEffectiveScope(scope, enclosingEnv)) {
    computeAllowSyntax(scope);
    computeThisBinding(effectiveScope);
    computeInWith(scope);
    computeExternalInitializers(scope);
    computeInClass(scope);
  }

 private:
  void computeAllowSyntax(Scope* scope);
  void computeThisBinding(Scope* scope);
  void computeInWith(Scope* scope);
  void computeExternalInitializers(Scope* scope);
  void computeInClass(Scope* scope);

  static Scope* determineEffectiveScope(Scope* scope, JSObject* environment);
};

struct CompilationAtomCache {
  // Atoms lowered into or converted from CompilationStencil.parserAtoms.
  //
  // This field is here instead of in CompilationGCOutput because atoms lowered
  // from JSAtom is part of input (enclosing scope bindings, lazy function name,
  // etc), and having 2 vectors in both input/output is error prone.
  JS::GCVector<JSAtom*, 0, js::SystemAllocPolicy> atoms;

  void trace(JSTracer* trc);
} JS_HAZ_GC_POINTER;

// Input of the compilation, including source and enclosing context.
struct CompilationInput {
  const JS::ReadOnlyCompileOptions& options;

  CompilationAtomCache atomCache;

  BaseScript* lazy = nullptr;

  ScriptSourceHolder source_;

  //  * If we're compiling standalone function, the non-null enclosing scope of
  //    the function
  //  * If we're compiling eval, the non-null enclosing scope of the `eval`.
  //  * If we're compiling module, null that means empty global scope
  //    (See EmitterScope::checkEnvironmentChainLength)
  //  * If we're compiling self-hosted JS, an empty global scope.
  //    This scope is also used for EmptyGlobalScopeType in
  //    CompilationStencil.gcThings.
  //    See the comment in initForSelfHostingGlobal.
  //  * Null otherwise
  Scope* enclosingScope = nullptr;

  explicit CompilationInput(const JS::ReadOnlyCompileOptions& options)
      : options(options) {}

 private:
  bool initScriptSource(JSContext* cx);

 public:
  bool initForGlobal(JSContext* cx) { return initScriptSource(cx); }

  bool initForSelfHostingGlobal(JSContext* cx) {
    if (!initScriptSource(cx)) {
      return false;
    }

    // This enclosing scope is also recorded as EmptyGlobalScopeType in
    // CompilationStencil.gcThings even though corresponding ScopeStencil
    // isn't generated.
    //
    // Store the enclosing scope here in order to access it from
    // inner scopes' ScopeStencil::enclosing.
    enclosingScope = &cx->global()->emptyGlobalScope();
    return true;
  }

  bool initForStandaloneFunction(JSContext* cx,
                                 HandleScope functionEnclosingScope) {
    if (!initScriptSource(cx)) {
      return false;
    }
    enclosingScope = functionEnclosingScope;
    return true;
  }

  bool initForEval(JSContext* cx, HandleScope evalEnclosingScope) {
    if (!initScriptSource(cx)) {
      return false;
    }
    enclosingScope = evalEnclosingScope;
    return true;
  }

  bool initForModule(JSContext* cx) {
    if (!initScriptSource(cx)) {
      return false;
    }
    // The `enclosingScope` is the emptyGlobalScope.
    return true;
  }

  void initFromLazy(BaseScript* lazyScript) {
    lazy = lazyScript;
    enclosingScope = lazy->function()->enclosingScope();
  }

  ScriptSource* source() { return source_.get(); }

 private:
  void setSource(ScriptSource* ss) { return source_.reset(ss); }

 public:
  template <typename Unit>
  MOZ_MUST_USE bool assignSource(JSContext* cx,
                                 JS::SourceText<Unit>& sourceBuffer) {
    return source()->assignSource(cx, options, sourceBuffer);
  }

  void trace(JSTracer* trc);
} JS_HAZ_GC_POINTER;

struct MOZ_RAII CompilationState {
  // Until we have dealt with Atoms in the front end, we need to hold
  // onto them.
  Directives directives;

  ScopeContext scopeContext;

  UsedNameTracker usedNames;
  LifoAllocScope& allocScope;

  CompilationState(JSContext* cx, LifoAllocScope& alloc,
                   const JS::ReadOnlyCompileOptions& options,
                   Scope* enclosingScope = nullptr,
                   JSObject* enclosingEnv = nullptr)
      : directives(options.forceStrictMode()),
        scopeContext(cx, enclosingScope, enclosingEnv),
        usedNames(cx),
        allocScope(alloc) {}
};

// The top level struct of stencil.
struct CompilationStencil {
  // This holds allocations that do not require destructors to be run but are
  // live until the stencil is released.
  LifoAlloc alloc;

  // Hold onto the RegExpStencil, BigIntStencil, and ObjLiteralStencil that are
  // allocated during parse to ensure correct destruction.
  Vector<RegExpStencil, 0, js::SystemAllocPolicy> regExpData;
  Vector<BigIntStencil, 0, js::SystemAllocPolicy> bigIntData;
  Vector<ObjLiteralStencil, 0, js::SystemAllocPolicy> objLiteralData;

  // Stencil for all function and non-function scripts. The TopLevelIndex is
  // reserved for the top-level script. This top-level may or may not be a
  // function.
  Vector<ScriptStencil, 0, js::SystemAllocPolicy> scriptData;

  // A rooted list of scopes created during this parse.
  //
  // To ensure that ScopeStencil's destructors fire, and thus our HeapPtr
  // barriers, we store the scopeData at this level so that they
  // can be safely destroyed, rather than LifoAllocing them with the rest of
  // the parser data structures.
  //
  // References to scopes are controlled via AbstractScopePtr, which holds onto
  // an index (and CompilationInfo reference).
  Vector<ScopeStencil, 0, js::SystemAllocPolicy> scopeData;

  // Module metadata if this is a module compile.
  StencilModuleMetadata moduleMetadata;

  // AsmJS modules generated by parsing.
  HashMap<FunctionIndex, RefPtr<const JS::WasmModule>,
          mozilla::DefaultHasher<FunctionIndex>, js::SystemAllocPolicy>
      asmJS;

  // Table of parser atoms for this compilation.
  ParserAtomsTable parserAtoms;

  // Parameterized chunk size to use for LifoAlloc.
  static constexpr size_t LifoAllocChunkSize = 512;

  explicit CompilationStencil(JSRuntime* rt)
      : alloc(LifoAllocChunkSize), parserAtoms(rt, alloc) {}

  // We need a move-constructor to work with Rooted, but must be explicit in
  // order to steal the LifoAlloc data.
  CompilationStencil(CompilationStencil&& other) noexcept
      : alloc(LifoAllocChunkSize),
        regExpData(std::move(other.regExpData)),
        bigIntData(std::move(other.bigIntData)),
        objLiteralData(std::move(other.objLiteralData)),
        scriptData(std::move(other.scriptData)),
        scopeData(std::move(other.scopeData)),
        moduleMetadata(std::move(other.moduleMetadata)),
        asmJS(std::move(other.asmJS)),
        parserAtoms(std::move(other.parserAtoms)) {
    // Steal the data from the LifoAlloc. Anything that holds a reference to
    // this will need to be updated.
    alloc.steal(&other.alloc);
    parserAtoms.updateLifoAlloc(alloc);
  }

#if defined(DEBUG) || defined(JS_JITSPEW)
  void dump();
  void dump(js::JSONPrinter& json);
#endif
};

// The output of GC allocation from stencil.
struct CompilationGCOutput {
  // The resulting outermost script for the compilation powered
  // by this CompilationInfo.
  JSScript* script = nullptr;

  // The resulting module object if there is one.
  ModuleObject* module = nullptr;

  // A Rooted vector to handle tracing of JSFunction* and Atoms within.
  //
  // If the top level script isn't a function, the item at TopLevelIndex is
  // nullptr.
  JS::GCVector<JSFunction*, 0, js::SystemAllocPolicy> functions;

  // References to scopes are controlled via AbstractScopePtr, which holds onto
  // an index (and CompilationInfo reference).
  JS::GCVector<js::Scope*, 0, js::SystemAllocPolicy> scopes;

  // The result ScriptSourceObject. This is unused in delazifying parses.
  ScriptSourceObject* sourceObject = nullptr;

  CompilationGCOutput() = default;

  void trace(JSTracer* trc);
} JS_HAZ_GC_POINTER;

class ScriptStencilIterable {
 public:
  class ScriptAndFunction {
   public:
    const ScriptStencil& script;
    JSFunction* function;
    FunctionIndex functionIndex;

    ScriptAndFunction() = delete;
    ScriptAndFunction(const ScriptStencil& script, JSFunction* function,
                      FunctionIndex functionIndex)
        : script(script), function(function), functionIndex(functionIndex) {}
  };

  class Iterator {
    size_t index_ = 0;
    const CompilationStencil& stencil_;
    CompilationGCOutput& gcOutput_;

    Iterator(const CompilationStencil& stencil, CompilationGCOutput& gcOutput,
             size_t index)
        : index_(index), stencil_(stencil), gcOutput_(gcOutput) {
      skipNonFunctions();
    }

   public:
    explicit Iterator(const CompilationStencil& stencil,
                      CompilationGCOutput& gcOutput)
        : stencil_(stencil), gcOutput_(gcOutput) {
      skipNonFunctions();
    }

    Iterator operator++() {
      next();
      skipNonFunctions();
      return *this;
    }

    void next() {
      MOZ_ASSERT(index_ < stencil_.scriptData.length());
      index_++;
    }

    void skipNonFunctions() {
      size_t length = stencil_.scriptData.length();
      while (index_ < length) {
        if (stencil_.scriptData[index_].isFunction()) {
          return;
        }

        index_++;
      }
    }

    bool operator!=(const Iterator& other) const {
      return index_ != other.index_;
    }

    ScriptAndFunction operator*() {
      const ScriptStencil& script = stencil_.scriptData[index_];

      FunctionIndex functionIndex = FunctionIndex(index_);
      return ScriptAndFunction(script, gcOutput_.functions[functionIndex],
                               functionIndex);
    }

    static Iterator end(const CompilationStencil& stencil,
                        CompilationGCOutput& gcOutput) {
      return Iterator(stencil, gcOutput, stencil.scriptData.length());
    }
  };

  const CompilationStencil& stencil_;
  CompilationGCOutput& gcOutput_;

  explicit ScriptStencilIterable(const CompilationStencil& stencil,
                                 CompilationGCOutput& gcOutput)
      : stencil_(stencil), gcOutput_(gcOutput) {}

  Iterator begin() const { return Iterator(stencil_, gcOutput_); }

  Iterator end() const { return Iterator::end(stencil_, gcOutput_); }
};

// Input and output of compilation to stencil.
struct CompilationInfo {
  static constexpr FunctionIndex TopLevelIndex = FunctionIndex(0);

  CompilationInput input;
  CompilationStencil stencil;

  // Set to true once prepareForInstantiate is called.
  // NOTE: This field isn't XDR-encoded.
  bool preparationIsPerformed = false;

  // Track the state of key allocations and roll them back as parts of parsing
  // get retried. This ensures iteration during stencil instantiation does not
  // encounter discarded frontend state.
  struct RewindToken {
    size_t scriptDataLength = 0;
    size_t asmJSCount = 0;
  };

  RewindToken getRewindToken();
  void rewind(const RewindToken& pos);

  // Construct a CompilationInfo
  CompilationInfo(JSContext* cx, const JS::ReadOnlyCompileOptions& options)
      : input(options), stencil(cx->runtime()) {}

  MOZ_MUST_USE bool prepareInputAndStencilForInstantiate(JSContext* cx);
  MOZ_MUST_USE bool prepareGCOutputForInstantiate(
      JSContext* cx, CompilationGCOutput& gcOutput);

  MOZ_MUST_USE bool prepareForInstantiate(JSContext* cx,
                                          CompilationGCOutput& gcOutput);
  MOZ_MUST_USE bool instantiateStencils(JSContext* cx,
                                        CompilationGCOutput& gcOutput);
  MOZ_MUST_USE bool instantiateStencilsAfterPreparation(
      JSContext* cx, CompilationGCOutput& gcOutput);

  MOZ_MUST_USE bool serializeStencils(JSContext* cx, JS::TranscodeBuffer& buf,
                                      bool* succeededOut = nullptr);

  const ParserAtom* lowerJSAtomToParserAtom(JSContext* cx, JSAtom* atom) {
    auto result = stencil.parserAtoms.internJSAtom(cx, *this, atom);
    return result.unwrapOr(nullptr);
  }

  // Move constructor is necessary to use Rooted.
  CompilationInfo(CompilationInfo&&) = default;

  // To avoid any misuses, make sure this is neither copyable or assignable.
  CompilationInfo(const CompilationInfo&) = delete;
  CompilationInfo& operator=(const CompilationInfo&) = delete;
  CompilationInfo& operator=(CompilationInfo&&) = delete;

  ScriptStencilIterable functionScriptStencils(CompilationGCOutput& gcOutput) {
    return ScriptStencilIterable(stencil, gcOutput);
  }

  void trace(JSTracer* trc);
};

// A set of CompilationInfo, for XDR purpose.
// This contains the initial compilation, and a vector of delazification.
struct CompilationInfoVector {
 private:
  using FunctionKey = uint64_t;
  using FunctionMap = HashMap<FunctionKey, FunctionIndex>;

  static FunctionKey toFunctionKey(const SourceExtent& extent) {
    return (FunctionKey)extent.sourceStart << 32 | extent.sourceEnd;
  }

  MOZ_MUST_USE bool buildDelazificationStencilMap(FunctionMap& functionMap);

 public:
  frontend::CompilationInfo initial;
  GCVector<frontend::CompilationInfo, 0, js::SystemAllocPolicy> delazifications;

  CompilationInfoVector(JSContext* cx,
                        const JS::ReadOnlyCompileOptions& options)
      : initial(cx, options) {}

  // Move constructor is necessary to use Rooted.
  CompilationInfoVector(CompilationInfoVector&&) = default;

  // To avoid any misuses, make sure this is neither copyable or assignable.
  CompilationInfoVector(const CompilationInfoVector&) = delete;
  CompilationInfoVector& operator=(const CompilationInfoVector&) = delete;
  CompilationInfoVector& operator=(CompilationInfoVector&&) = delete;

  MOZ_MUST_USE bool prepareForInstantiate(JSContext* cx,
                                          CompilationGCOutput& gcOutput);
  MOZ_MUST_USE bool instantiateStencils(JSContext* cx,
                                        CompilationGCOutput& gcOutput);
  MOZ_MUST_USE bool instantiateStencilsAfterPreparation(
      JSContext* cx, CompilationGCOutput& gcOutput);

  MOZ_MUST_USE bool deserializeStencils(JSContext* cx,
                                        const JS::TranscodeRange& range,
                                        bool* succeededOut);

  void trace(JSTracer* trc);
};

// Allocate an uninitialized script-things array using the Stencil's allocator.
mozilla::Span<ScriptThingVariant> NewScriptThingSpanUninitialized(
    JSContext* cx, LifoAlloc& alloc, uint32_t ngcthings);

}  // namespace frontend
}  // namespace js
#endif
