/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef frontend_CompilationInfo_h
#define frontend_CompilationInfo_h

#include "mozilla/Attributes.h"
#include "mozilla/RefPtr.h"
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
#include "js/Vector.h"
#include "js/WasmModule.h"
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

struct CompilationInfo;

class ScriptStencilIterable {
 public:
  class ScriptAndFunction {
   public:
    ScriptStencil& stencil;
    HandleFunction function;
    FunctionIndex functionIndex;

    ScriptAndFunction() = delete;
    ScriptAndFunction(ScriptStencil& stencil, HandleFunction function,
                      FunctionIndex functionIndex)
        : stencil(stencil), function(function), functionIndex(functionIndex) {}
  };

  class Iterator {
    enum class State {
      TopLevel,
      Functions,
    };
    State state_ = State::TopLevel;
    size_t index_ = 0;
    CompilationInfo* compilationInfo_;

    Iterator(CompilationInfo* compilationInfo, State state, size_t index)
        : state_(state), index_(index), compilationInfo_(compilationInfo) {
      skipNonFunctions();
    }

   public:
    explicit Iterator(CompilationInfo* compilationInfo)
        : compilationInfo_(compilationInfo) {
      skipNonFunctions();
    }

    Iterator operator++() {
      next();
      skipNonFunctions();
      return *this;
    }

    inline void next();

    inline void skipNonFunctions();

    bool operator!=(const Iterator& other) const {
      return state_ != other.state_ || index_ != other.index_;
    }

    inline ScriptAndFunction operator*();

    static inline Iterator end(CompilationInfo* compilationInfo);
  };

  CompilationInfo* compilationInfo_;

  explicit ScriptStencilIterable(CompilationInfo* compilationInfo)
      : compilationInfo_(compilationInfo) {}

  Iterator begin() const { return Iterator(compilationInfo_); }

  Iterator end() const { return Iterator::end(compilationInfo_); }
};

// CompilationInfo owns a number of pieces of information about script
// compilation as well as controls the lifetime of parse nodes and other data by
// controling the mark and reset of the LifoAlloc.
struct MOZ_RAII CompilationInfo {
  static constexpr FunctionIndex TopLevelFunctionIndex = FunctionIndex(0);

  JSContext* cx;
  const JS::ReadOnlyCompileOptions& options;

  // Until we have dealt with Atoms in the front end, we need to hold
  // onto them.
  AutoKeepAtoms keepAtoms;

  // Table of parser atoms for this compilation.
  ParserAtomsTable parserAtoms;

  Directives directives;

  ScopeContext scopeContext;

  // The resulting outermost script for the compilation powered
  // by this CompilationInfo.
  JS::Rooted<JSScript*> script;
  JS::Rooted<BaseScript*> lazy;

  // The resulting module object if there is one.
  JS::Rooted<ModuleObject*> module;

  UsedNameTracker usedNames;
  LifoAllocScope& allocScope;

  // Hold onto the RegExpStencil, BigIntStencil, and ObjLiteralStencil that are
  // allocated during parse to ensure correct destruction.
  Vector<RegExpStencil> regExpData;
  Vector<BigIntStencil> bigIntData;
  Vector<ObjLiteralStencil> objLiteralData;

  // A Rooted vector to handle tracing of JSFunction*
  // and Atoms within.
  JS::RootedVector<JSFunction*> functions;
  Vector<ScriptStencil> funcData;

  // The enclosing scope of the function if we're compiling standalone function.
  // The enclosing scope of the `eval` if we're compiling eval.
  // Null otherwise.
  JS::Rooted<Scope*> enclosingScope;

  // Stencil for top-level script. This includes standalone functions and
  // functions being delazified.
  ScriptStencil topLevel;

  // A rooted list of scopes created during this parse.
  //
  // To ensure that ScopeStencil's destructors fire, and thus our HeapPtr
  // barriers, we store the scopeData at this level so that they
  // can be safely destroyed, rather than LifoAllocing them with the rest of
  // the parser data structures.
  //
  // References to scopes are controlled via AbstractScopePtr, which holds onto
  // an index (and CompilationInfo reference).
  JS::RootedVector<js::Scope*> scopes;
  Vector<ScopeStencil> scopeData;

  // Module metadata if this is a module compile.
  StencilModuleMetadata moduleMetadata;

  // AsmJS modules generated by parsing.
  HashMap<FunctionIndex, RefPtr<const JS::WasmModule>> asmJS;

  // The result ScriptSourceObject. This is unused in delazifying parses.
  JS::Rooted<ScriptSourceHolder> source_;
  JS::Rooted<ScriptSourceObject*> sourceObject;

  // Track the state of key allocations and roll them back as parts of parsing
  // get retried. This ensures iteration during stencil instantiation does not
  // encounter discarded frontend state.
  struct RewindToken {
    size_t funcDataLength = 0;
    size_t asmJSCount = 0;
  };

  RewindToken getRewindToken();
  void rewind(const RewindToken& pos);

  // Construct a CompilationInfo
  CompilationInfo(JSContext* cx, LifoAllocScope& alloc,
                  const JS::ReadOnlyCompileOptions& options,
                  Scope* enclosingScope = nullptr,
                  JSObject* enclosingEnv = nullptr)
      : cx(cx),
        options(options),
        keepAtoms(cx),
        parserAtoms(cx),
        directives(options.forceStrictMode()),
        scopeContext(cx, enclosingScope, enclosingEnv),
        script(cx),
        lazy(cx),
        module(cx),
        usedNames(cx),
        allocScope(alloc),
        regExpData(cx),
        bigIntData(cx),
        objLiteralData(cx),
        functions(cx),
        funcData(cx),
        enclosingScope(cx),
        topLevel(cx),
        scopes(cx),
        scopeData(cx),
        moduleMetadata(cx),
        asmJS(cx),
        source_(cx),
        sourceObject(cx) {}

  bool init(JSContext* cx);

  bool initForStandaloneFunction(JSContext* cx, HandleScope enclosingScope) {
    if (!init(cx)) {
      return false;
    }
    this->enclosingScope = enclosingScope;
    return true;
  }

  void initFromLazy(BaseScript* lazy) {
    this->lazy = lazy;
    this->enclosingScope = lazy->function()->enclosingScope();
  }

  void setEnclosingScope(Scope* scope) { enclosingScope = scope; }

  ScriptSource* source() { return source_.get().get(); }
  void setSource(ScriptSource* ss) { return source_.get().reset(ss); }

  template <typename Unit>
  MOZ_MUST_USE bool assignSource(JS::SourceText<Unit>& sourceBuffer) {
    return source()->assignSource(cx, options, sourceBuffer);
  }

  MOZ_MUST_USE bool instantiateStencils();

  JSAtom* liftParserAtomToJSAtom(const ParserAtom* parserAtom) {
    return parserAtom->toJSAtom(cx).unwrapOr(nullptr);
  }
  const ParserAtom* lowerJSAtomToParserAtom(JSAtom* atom) {
    auto result = parserAtoms.internJSAtom(cx, atom);
    return result.unwrapOr(nullptr);
  }

  // To avoid any misuses, make sure this is neither copyable,
  // movable or assignable.
  CompilationInfo(const CompilationInfo&) = delete;
  CompilationInfo(CompilationInfo&&) = delete;
  CompilationInfo& operator=(const CompilationInfo&) = delete;
  CompilationInfo& operator=(CompilationInfo&&) = delete;

  ScriptStencilIterable functionScriptStencils() {
    return ScriptStencilIterable(this);
  }

#if defined(DEBUG) || defined(JS_JITSPEW)
  void dumpStencil();
  void dumpStencil(js::JSONPrinter& json);
#endif
};

inline void ScriptStencilIterable::Iterator::next() {
  if (state_ == State::TopLevel) {
    state_ = State::Functions;
  } else {
    MOZ_ASSERT(index_ < compilationInfo_->funcData.length());
    index_++;
  }
}

inline void ScriptStencilIterable::Iterator::skipNonFunctions() {
  if (state_ == State::TopLevel) {
    if (compilationInfo_->topLevel.isFunction()) {
      return;
    }

    next();
  }

  size_t length = compilationInfo_->funcData.length();
  while (index_ < length) {
    // NOTE: If topLevel is a function, funcData can contain unused item,
    //       and the item isn't marked as a function.
    if (compilationInfo_->funcData[index_].isFunction()) {
      return;
    }

    index_++;
  }
}

inline ScriptStencilIterable::ScriptAndFunction
ScriptStencilIterable::Iterator::operator*() {
  ScriptStencil& stencil = state_ == State::TopLevel
                               ? compilationInfo_->topLevel
                               : compilationInfo_->funcData[index_];

  FunctionIndex functionIndex = FunctionIndex(
      state_ == State::TopLevel ? CompilationInfo::TopLevelFunctionIndex
                                : index_);
  return ScriptAndFunction(stencil, compilationInfo_->functions[functionIndex],
                           functionIndex);
}

/* static */ inline ScriptStencilIterable::Iterator
ScriptStencilIterable::Iterator::end(CompilationInfo* compilationInfo) {
  return Iterator(compilationInfo, State::Functions,
                  compilationInfo->funcData.length());
}

}  // namespace frontend
}  // namespace js
#endif
