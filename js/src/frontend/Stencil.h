/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef frontend_Stencil_h
#define frontend_Stencil_h

#include "frontend/AbstractScope.h"
#include "frontend/TypedIndex.h"
#include "gc/AllocKind.h"
#include "gc/Rooting.h"
#include "js/RegExpFlags.h"
#include "vm/BigIntType.h"
#include "vm/JSFunction.h"
#include "vm/JSScript.h"
#include "vm/Scope.h"

namespace js {
namespace frontend {

// [SMDOC] Script Stencil (Frontend Representation)
//
// Stencils are GC object free representations of artifacts created during
// parsing and bytecode emission that are being used as part of Project
// Stencil (https://bugzilla.mozilla.org/show_bug.cgi?id=stencil) to revamp
// the frontend.
//
// Renaming to use the term stencil more broadly is still in progress.

enum class FunctionSyntaxKind : uint8_t;

// Data used to instantiate the lazy script before script emission.
struct LazyScriptCreationData {
  frontend::AtomVector closedOverBindings;

  // This is traced by the functionbox which owns this LazyScriptCreationData
  FunctionBoxVector innerFunctionBoxes;
  bool strict = false;

  mozilla::Maybe<FieldInitializers> fieldInitializers;

  explicit LazyScriptCreationData(JSContext* cx) : innerFunctionBoxes(cx) {}

  bool init(JSContext* cx, const frontend::AtomVector& COB,
            FunctionBoxVector& innerBoxes, bool isStrict) {
    strict = isStrict;
    // Copy out of the stack allocated vectors.
    if (!innerFunctionBoxes.appendAll(innerBoxes)) {
      return false;
    }

    if (!closedOverBindings.appendAll(COB)) {
      ReportOutOfMemory(cx);  // closedOverBindings uses SystemAllocPolicy.
      return false;
    }
    return true;
  }

  bool create(JSContext* cx, FunctionBox* funbox,
              HandleScriptSourceObject sourceObject);
};

// Metadata that can be used to allocate a JSFunction object.
//
// Keeping metadata separate allows the parser to generate
// metadata without requiring immediate access to the garbage
// collector.
struct FunctionCreationData {
  FunctionCreationData(HandleAtom atom, FunctionSyntaxKind kind,
                       GeneratorKind generatorKind, FunctionAsyncKind asyncKind,
                       bool isSelfHosting = false, bool inFunctionBox = false);

  // Custom Copy constructor to ensure we never copy a FunctionCreationData
  // that has a LazyScriptData.
  //
  // We want to be able to copy FunctionCreationData into a Functionbox as part
  // of our attempts to syntax parse an inner function, however, because
  // trySyntaxParseInnerFunction is fallible, for example, if a new directive
  // like "use asmjs" is encountered, we don't want to -move- it into the
  // FunctionBox, because we may need it again if the syntax parse fails.
  //
  // To ensure that we never lose a lazyScriptData however, we guarantee that
  // when this copy constructor is run, it doesn't have any lazyScriptData.
  FunctionCreationData(const FunctionCreationData& data)
      : atom(data.atom),
        kind(data.kind),
        generatorKind(data.generatorKind),
        asyncKind(data.asyncKind),
        allocKind(data.allocKind),
        flags(data.flags),
        isSelfHosting(data.isSelfHosting),
        lazyScriptData(mozilla::Nothing()) {
    MOZ_RELEASE_ASSERT(!data.lazyScriptData);
  }

  FunctionCreationData(FunctionCreationData&& data) = default;

  // The Parser uses KeepAtoms to prevent GC from collecting atoms
  JSAtom* atom = nullptr;
  FunctionSyntaxKind kind;  // can't field-initialize and forward declare
  GeneratorKind generatorKind = GeneratorKind::NotGenerator;
  FunctionAsyncKind asyncKind = FunctionAsyncKind::SyncFunction;

  gc::AllocKind allocKind = gc::AllocKind::FUNCTION;
  FunctionFlags flags = {};

  bool isSelfHosting = false;

  mozilla::Maybe<LazyScriptCreationData> lazyScriptData;

  HandleAtom getAtom(JSContext* cx) const;

  void trace(JSTracer* trc) {
    TraceNullableRoot(trc, &atom, "FunctionCreationData atom");
  }
};

// This owns a set of characters, previously syntax checked as a RegExp. Used
// to avoid allocating the RegExp on the GC heap during parsing.
class RegExpCreationData {
  UniquePtr<char16_t[], JS::FreePolicy> buf_;
  size_t length_ = 0;
  JS::RegExpFlags flags_;

 public:
  RegExpCreationData() = default;

  MOZ_MUST_USE bool init(JSContext* cx, mozilla::Range<const char16_t> range,
                         JS::RegExpFlags flags) {
    length_ = range.length();
    buf_ = js::DuplicateString(cx, range.begin().get(), range.length());
    if (!buf_) {
      return false;
    }
    flags_ = flags;
    return true;
  }

  RegExpObject* createRegExp(JSContext* cx) const;
};

using RegExpIndex = TypedIndex<RegExpCreationData>;

// This owns a set of characters guaranteed to parse into a BigInt via
// ParseBigIntLiteral. Used to avoid allocating the BigInt on the
// GC heap during parsing.
class BigIntCreationData {
  UniqueTwoByteChars buf_;
  size_t length_ = 0;

 public:
  BigIntCreationData() = default;

  MOZ_MUST_USE bool init(JSContext* cx, const Vector<char16_t, 32>& buf) {
#ifdef DEBUG
    // Assert we have no separators; if we have a separator then the algorithm
    // used in BigInt::literalIsZero will be incorrect.
    for (char16_t c : buf) {
      MOZ_ASSERT(c != '_');
    }
#endif
    length_ = buf.length();
    buf_ = js::DuplicateString(cx, buf.begin(), buf.length());
    return buf_ != nullptr;
  }

  BigInt* createBigInt(JSContext* cx) {
    mozilla::Range<const char16_t> source(buf_.get(), length_);

    return js::ParseBigIntLiteral(cx, source);
  }

  bool isZero() {
    mozilla::Range<const char16_t> source(buf_.get(), length_);
    return js::BigIntLiteralIsZero(source);
  }
};

using BigIntIndex = TypedIndex<BigIntCreationData>;

} /* namespace frontend */

class ScopeCreationData {
  friend class AbstractScope;
  friend class GCMarker;

  // The enclosing scope if it exists
  AbstractScope enclosing_;

  // The kind determines data_.
  ScopeKind kind_;

  // If there are any aliased bindings, the shape for the
  // EnvironmentObject. Otherwise nullptr.
  HeapPtr<Shape*> environmentShape_ = {};

  // Once we've produced a scope from a scope creation data, there may still be
  // AbstractScopes refering to this ScopeCreationData, and if reification is
  // requested multiple times, we should return the same scope rather than
  // creating multiple sopes.
  //
  // As well, any queries that require data() to answer must be redirected to
  // the scope once the scope has been reified, as the ScopeCreationData loses
  // ownership of the data on reification.
  HeapPtr<Scope*> scope_ = {};

  // For FunctionScopes we need the funbox; nullptr otherwise.
  frontend::FunctionBox* funbox_ = nullptr;

  UniquePtr<BaseScopeData> data_;

 public:
  ScopeCreationData(JSContext* cx, ScopeKind kind,
                    Handle<AbstractScope> enclosing,
                    UniquePtr<BaseScopeData> data = {},
                    Shape* environmentShape = nullptr,
                    frontend::FunctionBox* funbox = nullptr)
      : enclosing_(enclosing),
        kind_(kind),
        environmentShape_(environmentShape),
        funbox_(funbox),
        data_(std::move(data)) {}

  ScopeKind kind() const { return kind_; }
  AbstractScope enclosing() { return enclosing_; }
  bool getOrCreateEnclosingScope(JSContext* cx, MutableHandleScope scope) {
    return enclosing_.getOrCreateScope(cx, scope);
  }

  // FunctionScope
  static bool create(JSContext* cx, frontend::ParseInfo& parseInfo,
                     Handle<FunctionScope::Data*> dataArg,
                     bool hasParameterExprs, bool needsEnvironment,
                     frontend::FunctionBox* funbox,
                     Handle<AbstractScope> enclosing, ScopeIndex* index);

  // LexicalScope
  static bool create(JSContext* cx, frontend::ParseInfo& parseInfo,
                     ScopeKind kind, Handle<LexicalScope::Data*> dataArg,
                     uint32_t firstFrameSlot, Handle<AbstractScope> enclosing,
                     ScopeIndex* index);
  // VarScope
  static bool create(JSContext* cx, frontend::ParseInfo& parseInfo,
                     ScopeKind kind, Handle<VarScope::Data*> dataArg,
                     uint32_t firstFrameSlot, bool needsEnvironment,
                     Handle<AbstractScope> enclosing, ScopeIndex* index);

  // GlobalScope
  static bool create(JSContext* cx, frontend::ParseInfo& parseInfo,
                     ScopeKind kind, Handle<GlobalScope::Data*> dataArg,
                     ScopeIndex* index);

  // EvalScope
  static bool create(JSContext* cx, frontend::ParseInfo& parseInfo,
                     ScopeKind kind, Handle<EvalScope::Data*> dataArg,
                     Handle<AbstractScope> enclosing, ScopeIndex* index);

  // ModuleScope
  static bool create(JSContext* cx, frontend::ParseInfo& parseInfo,
                     Handle<ModuleScope::Data*> dataArg,
                     HandleModuleObject module, Handle<AbstractScope> enclosing,
                     ScopeIndex* index);

  // WithScope
  static bool create(JSContext* cx, frontend::ParseInfo& parseInfo,
                     Handle<AbstractScope> enclosing, ScopeIndex* index);

  bool hasEnvironment() const {
    return Scope::hasEnvironment(kind(), environmentShape_);
  }

  // Valid for functions;
  bool isArrow() const;
  JSFunction* canonicalFunction() const;

  bool hasScope() const { return scope_ != nullptr; }

  Scope* getScope() const {
    MOZ_ASSERT(hasScope());
    return scope_;
  }

  Scope* createScope(JSContext* cx);

  void trace(JSTracer* trc);

  uint32_t nextFrameSlot() const;

 private:
  // Non owning reference to data
  template <typename SpecificScopeType>
  typename SpecificScopeType::Data& data() const {
    MOZ_ASSERT(data_.get());
    return *static_cast<typename SpecificScopeType::Data*>(data_.get());
  }

  // Transfer ownership into a new UniquePtr.
  template <typename SpecificScopeType>
  UniquePtr<typename SpecificScopeType::Data> releaseData() {
    return UniquePtr<typename SpecificScopeType::Data>(
        static_cast<typename SpecificScopeType::Data*>(data_.release()));
  }

  template <typename SpecificScopeType>
  Scope* createSpecificScope(JSContext* cx);

  template <typename SpecificScopeType>
  uint32_t nextFrameSlot() const {
    // If a scope has been allocated for the ScopeCreationData we no longer own
    // data, so defer to scope
    if (hasScope()) {
      return getScope()->template as<SpecificScopeType>().nextFrameSlot();
    }
    return data<SpecificScopeType>().nextFrameSlot;
  }
};

} /* namespace js */

namespace JS {
template <>
struct GCPolicy<js::ScopeCreationData*> {
  static void trace(JSTracer* trc, js::ScopeCreationData** data,
                    const char* name) {
    (*data)->trace(trc);
  }
};

}  // namespace JS
#endif /* frontend_Stencil_h */