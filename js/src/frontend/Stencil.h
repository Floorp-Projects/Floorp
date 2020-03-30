/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef frontend_Stencil_h
#define frontend_Stencil_h

#include "mozilla/Assertions.h"  // MOZ_ASSERT, MOZ_RELEASE_ASSERT
#include "mozilla/CheckedInt.h"  // CheckedUint32
#include "mozilla/Maybe.h"       // mozilla::{Maybe, Nothing}
#include "mozilla/Range.h"       // mozilla::Range
#include "mozilla/Span.h"        // mozilla::Span

#include <stdint.h>  // char16_t, uint8_t, uint32_t
#include <stdlib.h>  // size_t

#include "frontend/AbstractScopePtr.h"   // AbstractScopePtr, ScopeIndex
#include "frontend/NameAnalysisTypes.h"  // {AtomVector, FunctionBoxVector}
#include "frontend/ObjLiteral.h"         // ObjLiteralCreationData
#include "frontend/TypedIndex.h"         // TypedIndex
#include "gc/AllocKind.h"                // gc::AllocKind
#include "gc/Barrier.h"                  // HeapPtr, GCPtrAtom
#include "gc/Rooting.h"  // HandleAtom, HandleModuleObject, HandleScriptSourceObject, MutableHandleScope
#include "js/GCVariant.h"    // GC Support for mozilla::Variant
#include "js/RegExpFlags.h"  // JS::RegExpFlags
#include "js/RootingAPI.h"   // Handle
#include "js/UniquePtr.h"    // UniquePtr
#include "js/Utility.h"      // JS::FreePolicy, UniqueTwoByteChars
#include "js/Vector.h"       // js::Vector
#include "util/Text.h"       // DuplicateString
#include "vm/BigIntType.h"   // ParseBigIntLiteral
#include "vm/JSFunction.h"   // FunctionFlags
#include "vm/JSScript.h"  // GeneratorKind, FunctionAsyncKind, ScopeNote, JSTryNote, FieldInitializers
#include "vm/Runtime.h"  // ReportOutOfMemory
#include "vm/Scope.h"  // BaseScopeData, FunctionScope, LexicalScope, VarScope, GlobalScope, EvalScope, ModuleScope
#include "vm/ScopeKind.h"      // ScopeKind
#include "vm/SharedStencil.h"  // ImmutableScriptFlags

struct JSContext;
class JSAtom;
class JSFunction;
class JSTracer;

namespace js {

class Shape;

namespace frontend {

struct CompilationInfo;
class FunctionBox;

// [SMDOC] Script Stencil (Frontend Representation)
//
// Stencils are GC object free representations of artifacts created during
// parsing and bytecode emission that are being used as part of Project
// Stencil (https://bugzilla.mozilla.org/show_bug.cgi?id=stencil) to revamp
// the frontend.
//
// Renaming to use the term stencil more broadly is still in progress.

enum class FunctionSyntaxKind : uint8_t;

// Arbitrary typename to disambiguate TypedIndexes;
class FunctionIndexType;

// We need to be able to forward declare this type, so make a subclass
// rather than just using.
class FunctionIndex : public TypedIndex<FunctionIndexType> {
  // Delegate constructors;
  using Base = TypedIndex<FunctionIndexType>;
  using Base::Base;
};

// Data used to instantiate the lazy script before script emission.
struct LazyScriptCreationData {
  frontend::AtomVector closedOverBindings;

  // This is traced by the functionbox which owns this LazyScriptCreationData
  Vector<FunctionIndex> innerFunctionIndexes;
  bool forceStrict = false;
  bool strict = false;

  mozilla::Maybe<FieldInitializers> fieldInitializers;

  explicit LazyScriptCreationData(JSContext* cx) : innerFunctionIndexes(cx) {}

  bool init(JSContext* cx, const frontend::AtomVector& COB,
            Vector<FunctionIndex>&& innerIndexes, bool isForceStrict,
            bool isStrict) {
    // Check if we will overflow the `ngcthings` field later.
    mozilla::CheckedUint32 ngcthings =
        mozilla::CheckedUint32(COB.length()) +
        mozilla::CheckedUint32(innerIndexes.length());
    if (!ngcthings.isValid()) {
      ReportAllocationOverflow(cx);
      return false;
    }

    forceStrict = isForceStrict;
    strict = isStrict;
    innerFunctionIndexes = std::move(innerIndexes);

    if (!closedOverBindings.appendAll(COB)) {
      ReportOutOfMemory(cx);  // closedOverBindings uses SystemAllocPolicy.
      return false;
    }
    return true;
  }

  bool create(JSContext* cx, CompilationInfo& compilationInfo,
              HandleFunction function, FunctionBox* funbox,
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

  // If Some(), calls JSFunction::setTypeForScriptedFunction when
  // this Function is created.
  mozilla::Maybe<bool> typeForScriptedFunction;

  mozilla::Maybe<LazyScriptCreationData> lazyScriptData;

  HandleAtom getAtom(JSContext* cx) const;

  void setInferredName(JSAtom* name) {
    MOZ_ASSERT(!atom);
    MOZ_ASSERT(name);
    MOZ_ASSERT(!flags.hasGuessedAtom());
    atom = name;
    flags.setInferredName();
  }

  JSAtom* inferredName() const {
    MOZ_ASSERT(flags.hasInferredName());
    MOZ_ASSERT(atom);
    return atom;
  }

  bool hasInferredName() const { return flags.hasInferredName(); }

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

  MOZ_MUST_USE bool init(JSContext* cx, JSAtom* pattern, JS::RegExpFlags flags);

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

class EnvironmentShapeCreationData {
  // Data used to call CreateEnvShapeData
  struct CreateEnvShapeData {
    BindingIter freshBi;
    const JSClass* cls;
    uint32_t nextEnvironmentSlot;
    uint32_t baseShapeFlags;

    void trace(JSTracer* trc) { freshBi.trace(trc); }
  };

  // Data used to call EmptyEnvironmentShape
  struct EmptyEnvShapeData {
    const JSClass* cls;
    uint32_t baseShapeFlags;
    void trace(JSTracer* trc){
        // Rather than having to expose this type as public to provide
        // an IgnoreGCPolicy, just have an empty trace.
    };
  };

  // Three different paths to creating an environment shape: Either we produce
  // nullptr directly (represented by storing Nothing in the variant), or we
  // call CreateEnvironmentShape, or we call EmptyEnvironmentShape.
  mozilla::Variant<mozilla::Nothing, CreateEnvShapeData, EmptyEnvShapeData>
      data_ = mozilla::AsVariant(mozilla::Nothing());

 public:
  explicit operator bool() const { return !data_.is<mozilla::Nothing>(); }

  // Setup for calling CreateEnvironmentShape
  void set(const BindingIter& freshBi, const JSClass* cls,
           uint32_t nextEnvironmentSlot, uint32_t baseShapeFlags) {
    data_ = mozilla::AsVariant(
        CreateEnvShapeData{freshBi, cls, nextEnvironmentSlot, baseShapeFlags});
  }

  // Setup for calling EmptyEnviornmentShape
  void set(const JSClass* cls, uint32_t shapeFlags) {
    data_ = mozilla::AsVariant(EmptyEnvShapeData{cls, shapeFlags});
  }

  // Reifiy this into an actual shape.
  MOZ_MUST_USE bool createShape(JSContext* cx, MutableHandleShape shape);

  void trace(JSTracer* trc) {
    using DataGCPolicy = JS::GCPolicy<decltype(data_)>;
    DataGCPolicy::trace(trc, &data_, "data_");
  }
};

class ScopeCreationData {
  friend class js::AbstractScopePtr;
  friend class js::GCMarker;

  // The enclosing scope if it exists
  AbstractScopePtr enclosing_;

  // The kind determines data_.
  ScopeKind kind_;

  // Data to reify an environment shape at creation time.
  EnvironmentShapeCreationData environmentShape_;

  // Once we've produced a scope from a scope creation data, there may still be
  // AbstractScopePtrs refering to this ScopeCreationData, and if reification is
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
  ScopeCreationData(
      JSContext* cx, ScopeKind kind, Handle<AbstractScopePtr> enclosing,
      Handle<frontend::EnvironmentShapeCreationData> environmentShape,
      UniquePtr<BaseScopeData> data = {},
      frontend::FunctionBox* funbox = nullptr)
      : enclosing_(enclosing),
        kind_(kind),
        environmentShape_(environmentShape),  // Copied
        funbox_(funbox),
        data_(std::move(data)) {}

  ScopeKind kind() const { return kind_; }
  AbstractScopePtr enclosing() { return enclosing_; }
  bool getOrCreateEnclosingScope(JSContext* cx, MutableHandleScope scope) {
    return enclosing_.getOrCreateScope(cx, scope);
  }

  // FunctionScope
  static bool create(JSContext* cx, frontend::CompilationInfo& compilationInfo,
                     Handle<FunctionScope::Data*> dataArg,
                     bool hasParameterExprs, bool needsEnvironment,
                     frontend::FunctionBox* funbox,
                     Handle<AbstractScopePtr> enclosing, ScopeIndex* index);

  // LexicalScope
  static bool create(JSContext* cx, frontend::CompilationInfo& compilationInfo,
                     ScopeKind kind, Handle<LexicalScope::Data*> dataArg,
                     uint32_t firstFrameSlot,
                     Handle<AbstractScopePtr> enclosing, ScopeIndex* index);
  // VarScope
  static bool create(JSContext* cx, frontend::CompilationInfo& compilationInfo,
                     ScopeKind kind, Handle<VarScope::Data*> dataArg,
                     uint32_t firstFrameSlot, bool needsEnvironment,
                     Handle<AbstractScopePtr> enclosing, ScopeIndex* index);

  // GlobalScope
  static bool create(JSContext* cx, frontend::CompilationInfo& compilationInfo,
                     ScopeKind kind, Handle<GlobalScope::Data*> dataArg,
                     ScopeIndex* index);

  // EvalScope
  static bool create(JSContext* cx, frontend::CompilationInfo& compilationInfo,
                     ScopeKind kind, Handle<EvalScope::Data*> dataArg,
                     Handle<AbstractScopePtr> enclosing, ScopeIndex* index);

  // ModuleScope
  static bool create(JSContext* cx, frontend::CompilationInfo& compilationInfo,
                     Handle<ModuleScope::Data*> dataArg,
                     HandleModuleObject module,
                     Handle<AbstractScopePtr> enclosing, ScopeIndex* index);

  // WithScope
  static bool create(JSContext* cx, frontend::CompilationInfo& compilationInfo,
                     Handle<AbstractScopePtr> enclosing, ScopeIndex* index);

  bool hasEnvironment() const {
    // Check if scope kind alone means we have an env shape, and
    // otherwise check if we have one created.
    return Scope::hasEnvironment(kind(), !!environmentShape_);
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

class EmptyGlobalScopeType {};

// These types all end up being baked into GC things as part of stencil
// instantiation.
using ScriptThingVariant =
    mozilla::Variant<BigIntIndex, ObjLiteralCreationData, RegExpIndex,
                     ScopeIndex, FunctionIndex, EmptyGlobalScopeType>;

// A vector of things destined to be converted to GC things.
using ScriptThingsVector = Vector<ScriptThingVariant>;

// Data used to instantiate the non-lazy script.
class ScriptStencil {
 public:
  js::UniquePtr<js::ImmutableScriptData> immutableScriptData = nullptr;

  // See `BaseScript::{lineno_,column_}`.
  unsigned lineno = 0;
  unsigned column = 0;

  // See `initAtomMap` method.
  uint32_t natoms = 0;

  // See `finishGCThings` method.
  uint32_t ngcthings = 0;

  // The flags that will be added to the script when initializing it.
  ImmutableScriptFlags immutableFlags;

  ScriptThingsVector gcThings;

  js::frontend::FunctionBox* functionBox = nullptr;

  ScriptStencil(JSContext* cx,
                UniquePtr<js::ImmutableScriptData> immutableScriptData)
      : immutableScriptData(std::move(immutableScriptData)), gcThings(cx) {}

  bool isFunction() const {
    return immutableFlags.hasFlag(ImmutableScriptFlagsEnum::IsFunction);
  }

  // Store all GC things into `gcthings`.
  // `gcthings.Length()` is `this.ngcthings`.
  virtual bool finishGCThings(JSContext* cx,
                              mozilla::Span<JS::GCCellPtr> output) const = 0;

  // Store all atoms into `atoms`
  // `atoms` is the pointer to `this.natoms`-length array of `GCPtrAtom`.
  virtual void initAtomMap(GCPtrAtom* atoms) const = 0;

  // Call `FunctionBox::finish` for all inner functions.
  virtual void finishInnerFunctions() const = 0;
};

} /* namespace frontend */
} /* namespace js */

namespace JS {
template <>
struct GCPolicy<js::frontend::ScopeCreationData*> {
  static void trace(JSTracer* trc, js::frontend::ScopeCreationData** data,
                    const char* name) {
    (*data)->trace(trc);
  }
};
}  // namespace JS
#endif /* frontend_Stencil_h */
