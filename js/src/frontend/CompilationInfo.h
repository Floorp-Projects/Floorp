/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef frontend_CompilationInfo_h
#define frontend_CompilationInfo_h

#include "mozilla/Assertions.h"  // MOZ_ASSERT
#include "mozilla/Attributes.h"
#include "mozilla/RefPtr.h"  // RefPtr
#include "mozilla/Span.h"
#include "mozilla/Variant.h"  // mozilla::Variant, mozilla::AsVariant

#include "builtin/ModuleObject.h"
#include "ds/LifoAlloc.h"
#include "frontend/ParserAtom.h"
#include "frontend/ScriptIndex.h"  // ScriptIndex
#include "frontend/SharedContext.h"
#include "frontend/Stencil.h"
#include "frontend/UsedNameTracker.h"
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
#include "vm/SharedStencil.h"  // SharedImmutableScriptData

namespace js {

class JSONPrinter;

namespace frontend {

// ScopeContext hold information derivied from the scope and environment chains
// to try to avoid the parser needing to traverse VM structures directly.
struct ScopeContext {
  // If this eval is in response to Debugger.Frame.eval, we may have an
  // incomplete scope chain. In order to provide a better debugging experience,
  // we inspect the (optional) environment chain to determine it's enclosing
  // FunctionScope if there is one. If there is no such scope, we use the
  // orignal scope provided.
  //
  // NOTE: This is used to compute the ThisBinding kind and to allow access to
  //       private fields, while other contextual information only uses the
  //       actual scope passed to the compile.
  JS::Rooted<Scope*> effectiveScope;

  // The type of binding required for `this` of the top level context, as
  // indicated by the enclosing scopes of this parse.
  //
  // NOTE: This is computed based on the effective scope (defined above).
  ThisBinding thisBinding = ThisBinding::Global;

  // Eval and arrow scripts inherit certain syntax allowances from their
  // enclosing scripts.
  bool allowNewTarget = false;
  bool allowSuperProperty = false;
  bool allowSuperCall = false;
  bool allowArguments = true;

  // Eval and arrow scripts also inherit the "this" environment -- used by
  // `super` expressions -- from their enclosing script. We count the number of
  // environment hops needed to get from enclosing scope to the nearest
  // appropriate environment. This value is undefined if the script we are
  // compiling is not an eval or arrow-function.
  uint32_t enclosingThisEnvironmentHops = 0;

  // Class field initializer info if we are nested within a class constructor.
  // We may be an combination of arrow and eval context within the constructor.
  mozilla::Maybe<MemberInitializers> memberInitializers = {};

  // Indicates there is a 'class' or 'with' scope on enclosing scope chain.
  bool inClass = false;
  bool inWith = false;

  explicit ScopeContext(JSContext* cx, InheritThis inheritThis, Scope* scope,
                        JSObject* enclosingEnv = nullptr)
      : effectiveScope(cx, determineEffectiveScope(scope, enclosingEnv)) {
    if (inheritThis == InheritThis::Yes) {
      computeThisBinding(effectiveScope);
      computeThisEnvironment(scope);
    }
    computeInScope(scope);
  }

 private:
  void computeThisBinding(Scope* scope);
  void computeThisEnvironment(Scope* scope);
  void computeInScope(Scope* scope);

  static Scope* determineEffectiveScope(Scope* scope, JSObject* environment);
};

struct CompilationAtomCache {
 public:
  using AtomCacheVector = JS::GCVector<JSAtom*, 0, js::SystemAllocPolicy>;

 private:
  // Atoms lowered into or converted from BaseCompilationStencil.parserAtomData.
  //
  // This field is here instead of in CompilationGCOutput because atoms lowered
  // from JSAtom is part of input (enclosing scope bindings, lazy function name,
  // etc), and having 2 vectors in both input/output is error prone.
  AtomCacheVector atoms_;

 public:
  JSAtom* getExistingAtomAt(ParserAtomIndex index) const;
  JSAtom* getExistingAtomAt(JSContext* cx,
                            TaggedParserAtomIndex taggedIndex) const;
  JSAtom* getAtomAt(ParserAtomIndex index) const;
  bool hasAtomAt(ParserAtomIndex index) const;
  bool setAtomAt(JSContext* cx, ParserAtomIndex index, JSAtom* atom);
  bool allocate(JSContext* cx, size_t length);

  void stealBuffer(AtomCacheVector& atoms);
  void returnBuffer(AtomCacheVector& atoms);

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
  //    BaseCompilationStencil.gcThings.
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
    // BaseCompilationStencil.gcThings even though corresponding ScopeStencil
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

  void setSource(ScriptSource* ss) { return source_.reset(ss); }

  template <typename Unit>
  MOZ_MUST_USE bool assignSource(JSContext* cx,
                                 JS::SourceText<Unit>& sourceBuffer) {
    return source()->assignSource(cx, options, sourceBuffer);
  }

  void trace(JSTracer* trc);
} JS_HAZ_GC_POINTER;

struct CompilationStencil;

struct MOZ_RAII CompilationState {
  // Until we have dealt with Atoms in the front end, we need to hold
  // onto them.
  Directives directives;

  ScopeContext scopeContext;

  UsedNameTracker usedNames;
  LifoAllocScope& allocScope;

  CompilationInput& input;

  // Temporary space to accumulate stencil data.
  // Copied to BaseCompilationStencil by `finish` method.
  //
  // See corresponding BaseCompilationStencil fields for desription.
  Vector<RegExpStencil, 0, js::SystemAllocPolicy> regExpData;
  Vector<ScriptStencil, 0, js::SystemAllocPolicy> scriptData;
  Vector<ScriptStencilExtra, 0, js::SystemAllocPolicy> scriptExtra;
  Vector<ScopeStencil, 0, js::SystemAllocPolicy> scopeData;
  Vector<BaseParserScopeData*, 0, js::SystemAllocPolicy> scopeNames;
  Vector<TaggedScriptThingIndex, 0, js::SystemAllocPolicy> gcThingData;

  // Table of parser atoms for this compilation.
  ParserAtomsTable parserAtoms;

  // The number of functions that *will* have bytecode.
  // This doesn't count top-level non-function script.
  //
  // This should be counted while parsing, and should be passed to
  // BaseCompilationStencil.prepareStorageFor *before* start emitting bytecode.
  size_t nonLazyFunctionCount = 0;

  CompilationState(JSContext* cx, LifoAllocScope& frontendAllocScope,
                   const JS::ReadOnlyCompileOptions& options,
                   CompilationStencil& stencil,
                   InheritThis inheritThis = InheritThis::No,
                   Scope* enclosingScope = nullptr,
                   JSObject* enclosingEnv = nullptr);

  bool finish(JSContext* cx, CompilationStencil& stencil);

  const ParserAtom* getParserAtomAt(JSContext* cx,
                                    TaggedParserAtomIndex taggedIndex) const;

  // Allocate space for `length` gcthings, and return the address of the
  // first element to `cursor` to initialize on the caller.
  bool allocateGCThingsUninitialized(JSContext* cx, ScriptIndex scriptIndex,
                                     size_t length,
                                     TaggedScriptThingIndex** cursor);

  bool appendGCThings(JSContext* cx, ScriptIndex scriptIndex,
                      mozilla::Span<const TaggedScriptThingIndex> things);
};

// Store shared data for non-lazy script.
struct SharedDataContainer {
  using SingleSharedData = RefPtr<js::SharedImmutableScriptData>;
  using SharedDataVector =
      Vector<RefPtr<js::SharedImmutableScriptData>, 0, js::SystemAllocPolicy>;
  using SharedDataMap =
      HashMap<ScriptIndex, RefPtr<js::SharedImmutableScriptData>,
              mozilla::DefaultHasher<ScriptIndex>, js::SystemAllocPolicy>;

  mozilla::Variant<SingleSharedData, SharedDataVector, SharedDataMap> storage;

  // Defaults to SingleSharedData for delazification vector.
  SharedDataContainer() : storage(mozilla::AsVariant(SingleSharedData())) {}

  bool prepareStorageFor(JSContext* cx, size_t nonLazyScriptCount,
                         size_t allScriptCount);

  // Returns index-th script's shared data, or nullptr if it doesn't have.
  js::SharedImmutableScriptData* get(ScriptIndex index);

  // Add data for index-th script and share it with VM.
  bool addAndShare(JSContext* cx, ScriptIndex index,
                   js::SharedImmutableScriptData* data);

#if defined(DEBUG) || defined(JS_JITSPEW)
  void dump();
  void dump(js::JSONPrinter& json);
  void dumpFields(js::JSONPrinter& json);
#endif
};

// The top level struct of stencil.
struct BaseCompilationStencil {
  // Hold onto the RegExpStencil, BigIntStencil, and ObjLiteralStencil that are
  // allocated during parse to ensure correct destruction.
  mozilla::Span<RegExpStencil> regExpData;
  Vector<BigIntStencil, 0, js::SystemAllocPolicy> bigIntData;
  Vector<ObjLiteralStencil, 0, js::SystemAllocPolicy> objLiteralData;

  // Stencil for all function and non-function scripts. The TopLevelIndex is
  // reserved for the top-level script. This top-level may or may not be a
  // function.
  mozilla::Span<ScriptStencil> scriptData;
  SharedDataContainer sharedData;
  mozilla::Span<TaggedScriptThingIndex> gcThingData;

  // scopeData and scopeNames have the same size, and i-th scopeNames contains
  // the names for the bindings contained in the slot defined by i-th scopeData.
  mozilla::Span<ScopeStencil> scopeData;
  mozilla::Span<BaseParserScopeData*> scopeNames;

  // List of parser atoms for this compilation.
  // This may contain nullptr entries when round-tripping with XDR if the atom
  // was generated in original parse but not used by stencil.
  ParserAtomSpan parserAtomData;

  // FunctionKey is an identifier that identifies a function within the source
  // next in a reproducible way. It allows us to match delazification data with
  // initial parse data, even across different runs. This is only used for
  // delazification stencils.
  using FunctionKey = uint64_t;

  static constexpr FunctionKey NullFunctionKey = 0;

  FunctionKey functionKey = NullFunctionKey;

  BaseCompilationStencil() = default;

  // We need a move-constructor to work with Rooted.
  BaseCompilationStencil(BaseCompilationStencil&& other) = default;

  const ParserAtom* getParserAtomAt(JSContext* cx,
                                    TaggedParserAtomIndex taggedIndex) const;

  bool prepareStorageFor(JSContext* cx, CompilationState& compilationState) {
    // NOTE: At this point CompilationState shouldn't be finished, and
    // BaseCompilationStencil.scriptData field should be empty.
    // Use CompilationState.scriptData as data source.
    MOZ_ASSERT(scriptData.empty());
    size_t allScriptCount = compilationState.scriptData.length();
    size_t nonLazyScriptCount = compilationState.nonLazyFunctionCount;
    if (!compilationState.scriptData[0].isFunction()) {
      nonLazyScriptCount++;
    }
    return sharedData.prepareStorageFor(cx, nonLazyScriptCount, allScriptCount);
  }

  static FunctionKey toFunctionKey(const SourceExtent& extent) {
    auto result = static_cast<FunctionKey>(extent.sourceStart) << 32 |
                  static_cast<FunctionKey>(extent.sourceEnd);
    MOZ_ASSERT(result != NullFunctionKey);
    return result;
  }

  bool isInitialStencil() const { return functionKey == NullFunctionKey; }

  bool isCompilationStencil() const { return isInitialStencil(); }
  inline CompilationStencil& asCompilationStencil();
  inline const CompilationStencil& asCompilationStencil() const;

#if defined(DEBUG) || defined(JS_JITSPEW)
  void dump();
  void dump(js::JSONPrinter& json);
  void dumpFields(js::JSONPrinter& json);
#endif
};

// The output of GC allocation from stencil.
struct CompilationGCOutput {
  // The resulting outermost script for the compilation powered
  // by this CompilationStencil.
  JSScript* script = nullptr;

  // The resulting module object if there is one.
  ModuleObject* module = nullptr;

  // A Rooted vector to handle tracing of JSFunction* and Atoms within.
  //
  // If the top level script isn't a function, the item at TopLevelIndex is
  // nullptr.
  JS::GCVector<JSFunction*, 0, js::SystemAllocPolicy> functions;

  // References to scopes are controlled via AbstractScopePtr, which holds onto
  // an index (and CompilationStencil reference).
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
    const ScriptStencilExtra* scriptExtra;
    JSFunction* function;
    ScriptIndex index;

    ScriptAndFunction() = delete;
    ScriptAndFunction(const ScriptStencil& script,
                      const ScriptStencilExtra* scriptExtra,
                      JSFunction* function, ScriptIndex index)
        : script(script),
          scriptExtra(scriptExtra),
          function(function),
          index(index) {}
  };

  class Iterator {
    size_t index_ = 0;
    const BaseCompilationStencil& stencil_;
    CompilationGCOutput& gcOutput_;

    Iterator(const BaseCompilationStencil& stencil,
             CompilationGCOutput& gcOutput, size_t index)
        : index_(index), stencil_(stencil), gcOutput_(gcOutput) {
      MOZ_ASSERT(index == stencil.scriptData.size());
    }

   public:
    explicit Iterator(const BaseCompilationStencil& stencil,
                      CompilationGCOutput& gcOutput)
        : stencil_(stencil), gcOutput_(gcOutput) {
      skipTopLevelNonFunction();
    }

    Iterator operator++() {
      next();
      assertFunction();
      return *this;
    }

    void next() {
      MOZ_ASSERT(index_ < stencil_.scriptData.size());
      index_++;
    }

    void assertFunction() {
      if (index_ < stencil_.scriptData.size()) {
        MOZ_ASSERT(stencil_.scriptData[index_].isFunction());
      }
    }

    void skipTopLevelNonFunction() {
      MOZ_ASSERT(index_ == 0);
      if (stencil_.scriptData.size()) {
        if (!stencil_.scriptData[0].isFunction()) {
          next();
          assertFunction();
        }
      }
    }

    bool operator!=(const Iterator& other) const {
      return index_ != other.index_;
    }

    inline ScriptAndFunction operator*();

    static Iterator end(const BaseCompilationStencil& stencil,
                        CompilationGCOutput& gcOutput) {
      return Iterator(stencil, gcOutput, stencil.scriptData.size());
    }
  };

  const BaseCompilationStencil& stencil_;
  CompilationGCOutput& gcOutput_;

  explicit ScriptStencilIterable(const BaseCompilationStencil& stencil,
                                 CompilationGCOutput& gcOutput)
      : stencil_(stencil), gcOutput_(gcOutput) {}

  Iterator begin() const { return Iterator(stencil_, gcOutput_); }

  Iterator end() const { return Iterator::end(stencil_, gcOutput_); }
};

// Input and output of compilation to stencil.
struct CompilationStencil : public BaseCompilationStencil {
  static constexpr ScriptIndex TopLevelIndex = ScriptIndex(0);

  // This holds allocations that do not require destructors to be run but are
  // live until the stencil is released.
  LifoAlloc alloc;

  // Parameterized chunk size to use for LifoAlloc.
  static constexpr size_t LifoAllocChunkSize = 512;

  CompilationInput input;

  // Initial-compilation-specific data for each script.
  mozilla::Span<ScriptStencilExtra> scriptExtra;

  // Module metadata if this is a module compile.
  mozilla::Maybe<StencilModuleMetadata> moduleMetadata;

  // AsmJS modules generated by parsing.
  HashMap<ScriptIndex, RefPtr<const JS::WasmModule>,
          mozilla::DefaultHasher<ScriptIndex>, js::SystemAllocPolicy>
      asmJS;

  // Set to true once prepareForInstantiate is called.
  // NOTE: This field isn't XDR-encoded.
  bool preparationIsPerformed = false;

  // Track the state of key allocations and roll them back as parts of parsing
  // get retried. This ensures iteration during stencil instantiation does not
  // encounter discarded frontend state.
  struct RewindToken {
    // Temporarily share this token struct with CompilationState.
    size_t scriptDataLength = 0;

    size_t asmJSCount = 0;
  };

  RewindToken getRewindToken(CompilationState& state);
  void rewind(CompilationState& state, const RewindToken& pos);

  // Construct a CompilationStencil
  CompilationStencil(JSContext* cx, const JS::ReadOnlyCompileOptions& options)
      : alloc(LifoAllocChunkSize), input(options) {}

  static MOZ_MUST_USE bool prepareInputAndStencilForInstantiate(
      JSContext* cx, CompilationInput& input, BaseCompilationStencil& stencil);
  static MOZ_MUST_USE bool prepareGCOutputForInstantiate(
      JSContext* cx, BaseCompilationStencil& stencil,
      CompilationGCOutput& gcOutput);

  static MOZ_MUST_USE bool prepareForInstantiate(JSContext* cx,
                                                 CompilationStencil& stencil,
                                                 CompilationGCOutput& gcOutput);
  static MOZ_MUST_USE bool instantiateStencils(JSContext* cx,
                                               CompilationStencil& stencil,
                                               CompilationGCOutput& gcOutput);
  static MOZ_MUST_USE bool instantiateStencilsAfterPreparation(
      JSContext* cx, CompilationInput& input, BaseCompilationStencil& stencil,
      CompilationGCOutput& gcOutput);

  MOZ_MUST_USE bool serializeStencils(JSContext* cx, JS::TranscodeBuffer& buf,
                                      bool* succeededOut = nullptr);

  // Move constructor is necessary to use Rooted, but must be explicit in
  // order to steal the LifoAlloc data
  CompilationStencil(CompilationStencil&& other) noexcept
      : BaseCompilationStencil(std::move(other)),
        alloc(LifoAllocChunkSize),
        input(std::move(other.input)) {
    // Steal the data from the LifoAlloc.
    alloc.steal(&other.alloc);
  }

  // To avoid any misuses, make sure this is neither copyable or assignable.
  CompilationStencil(const CompilationStencil&) = delete;
  CompilationStencil& operator=(const CompilationStencil&) = delete;
  CompilationStencil& operator=(CompilationStencil&&) = delete;

  static ScriptStencilIterable functionScriptStencils(
      BaseCompilationStencil& stencil, CompilationGCOutput& gcOutput) {
    return ScriptStencilIterable(stencil, gcOutput);
  }

  void trace(JSTracer* trc);

#if defined(DEBUG) || defined(JS_JITSPEW)
  void dump();
  void dump(js::JSONPrinter& json);
  void dumpFields(js::JSONPrinter& json);
#endif
};

inline CompilationStencil& BaseCompilationStencil::asCompilationStencil() {
  MOZ_ASSERT(isCompilationStencil());
  return *static_cast<CompilationStencil*>(this);
}

inline const CompilationStencil& BaseCompilationStencil::asCompilationStencil()
    const {
  MOZ_ASSERT(isCompilationStencil());
  return *reinterpret_cast<const CompilationStencil*>(this);
}

inline ScriptStencilIterable::ScriptAndFunction
ScriptStencilIterable::Iterator::operator*() {
  ScriptIndex index = ScriptIndex(index_);
  const ScriptStencil& script = stencil_.scriptData[index];
  const ScriptStencilExtra* scriptExtra = nullptr;
  if (stencil_.isInitialStencil()) {
    scriptExtra = &stencil_.asCompilationStencil().scriptExtra[index];
  }
  return ScriptAndFunction(script, scriptExtra, gcOutput_.functions[index],
                           index);
}

// A set of stencils, for XDR purpose.
// This contains the initial compilation, and a vector of delazification.
struct CompilationStencilSet : public CompilationStencil {
 private:
  using ScriptIndexVector = Vector<ScriptIndex, 0, js::SystemAllocPolicy>;

  MOZ_MUST_USE bool buildDelazificationIndices(JSContext* cx);

  // Parameterized chunk size to use for LifoAlloc.
  static constexpr size_t LifoAllocChunkSize = 512;

 public:
  LifoAlloc allocForDelazifications;
  Vector<BaseCompilationStencil, 0, js::SystemAllocPolicy> delazifications;
  ScriptIndexVector delazificationIndices;
  CompilationAtomCache::AtomCacheVector delazificationAtomCache;

  CompilationStencilSet(JSContext* cx,
                        const JS::ReadOnlyCompileOptions& options)
      : CompilationStencil(cx, options),
        allocForDelazifications(LifoAllocChunkSize) {}

  // Move constructor is necessary to use Rooted.
  CompilationStencilSet(CompilationStencilSet&& other) noexcept
      : CompilationStencil(std::move(other)),
        allocForDelazifications(LifoAllocChunkSize),
        delazifications(std::move(other.delazifications)),
        delazificationAtomCache(std::move(other.delazificationAtomCache)) {
    // Steal the data from the LifoAlloc.
    allocForDelazifications.steal(&other.allocForDelazifications);
  }

  // To avoid any misuses, make sure this is neither copyable or assignable.
  CompilationStencilSet(const CompilationStencilSet&) = delete;
  CompilationStencilSet& operator=(const CompilationStencilSet&) = delete;
  CompilationStencilSet& operator=(CompilationStencilSet&&) = delete;

  MOZ_MUST_USE bool prepareForInstantiate(
      JSContext* cx, CompilationGCOutput& gcOutput,
      CompilationGCOutput& gcOutputForDelazification);
  MOZ_MUST_USE bool instantiateStencils(
      JSContext* cx, CompilationGCOutput& gcOutput,
      CompilationGCOutput& gcOutputForDelazification);
  MOZ_MUST_USE bool instantiateStencilsAfterPreparation(
      JSContext* cx, CompilationGCOutput& gcOutput,
      CompilationGCOutput& gcOutputForDelazification);

  MOZ_MUST_USE bool deserializeStencils(JSContext* cx,
                                        const JS::TranscodeRange& range,
                                        bool* succeededOut);

  void trace(JSTracer* trc);
};

}  // namespace frontend
}  // namespace js

#endif  // frontend_CompilationInfo_h
