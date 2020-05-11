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

#include "frontend/AbstractScopePtr.h"    // AbstractScopePtr, ScopeIndex
#include "frontend/FunctionSyntaxKind.h"  // FunctionSyntaxKind
#include "frontend/NameAnalysisTypes.h"   // AtomVector
#include "frontend/ObjLiteral.h"          // ObjLiteralCreationData
#include "frontend/TypedIndex.h"          // TypedIndex
#include "gc/Barrier.h"                   // HeapPtr, GCPtrAtom
#include "gc/Rooting.h"  // HandleAtom, HandleModuleObject, HandleScriptSourceObject, MutableHandleScope
#include "js/GCVariant.h"              // GC Support for mozilla::Variant
#include "js/RegExpFlags.h"            // JS::RegExpFlags
#include "js/RootingAPI.h"             // Handle
#include "js/TypeDecls.h"              // JSContext,JSAtom,JSFunction
#include "js/UniquePtr.h"              // js::UniquePtr
#include "js/Utility.h"                // JS::FreePolicy, UniqueTwoByteChars
#include "js/Vector.h"                 // js::Vector
#include "util/Text.h"                 // DuplicateString
#include "vm/BigIntType.h"             // ParseBigIntLiteral
#include "vm/FunctionFlags.h"          // FunctionFlags
#include "vm/GeneratorAndAsyncKind.h"  // GeneratorKind, FunctionAsyncKind
#include "vm/JSFunction.h"             // FunctionFlags
#include "vm/JSScript.h"  // GeneratorKind, FunctionAsyncKind, FieldInitializers
#include "vm/Runtime.h"   // ReportOutOfMemory
#include "vm/Scope.h"  // BaseScopeData, FunctionScope, LexicalScope, VarScope, GlobalScope, EvalScope, ModuleScope
#include "vm/ScopeKind.h"      // ScopeKind
#include "vm/SharedStencil.h"  // ImmutableScriptFlags

class JS_PUBLIC_API JSTracer;

namespace js::frontend {

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

// Arbitrary typename to disambiguate TypedIndexes;
class FunctionIndexType;

// We need to be able to forward declare this type, so make a subclass
// rather than just using.
class FunctionIndex : public TypedIndex<FunctionIndexType> {
  // Delegate constructors;
  using Base = TypedIndex<FunctionIndexType>;
  using Base::Base;
};

FunctionFlags InitialFunctionFlags(FunctionSyntaxKind kind,
                                   GeneratorKind generatorKind,
                                   FunctionAsyncKind asyncKind,
                                   bool isSelfHosting = false,
                                   bool hasUnclonedName = false);

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
  bool isClassConstructor() const;
  const FieldInitializers& fieldInitializers() const;

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
  UniquePtr<typename SpecificScopeType::Data> releaseData();

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

// The lazy closed-over-binding info is represented by these types that will
// convert to a GCCellPtr(nullptr), GCCellPtr(JSAtom*).
class NullScriptThing {};
using ClosedOverBinding = JSAtom*;

// These types all end up being baked into GC things as part of stencil
// instantiation.
using ScriptThingVariant =
    mozilla::Variant<ClosedOverBinding, NullScriptThing, BigIntIndex,
                     ObjLiteralCreationData, RegExpIndex, ScopeIndex,
                     FunctionIndex, EmptyGlobalScopeType>;

// A vector of things destined to be converted to GC things.
using ScriptThingsVector = Vector<ScriptThingVariant>;

// Non-virtual base class for ScriptStencil.
class ScriptStencilBase {
 public:
  // See `BaseScript::functionOrGlobal_`.
  mozilla::Maybe<FunctionIndex> functionIndex;

  // See `BaseScript::immutableFlags_`.
  ImmutableScriptFlags immutableFlags;

  // See `BaseScript::data_`.
  mozilla::Maybe<FieldInitializers> fieldInitializers;
  ScriptThingsVector gcThings;

  // See `BaseScript::sharedData_`.
  uint32_t natoms = 0;
  js::UniquePtr<js::ImmutableScriptData> immutableScriptData = nullptr;

  explicit ScriptStencilBase(JSContext* cx) : gcThings(cx) {}

  // This traces any JSAtoms in the gcThings array. This will be removed once
  // atoms are deferred from parsing.
  void trace(JSTracer* trc);
};

// Data used to instantiate the non-lazy script.
class ScriptStencil : public ScriptStencilBase {
 public:
  using ImmutableFlags = ImmutableScriptFlagsEnum;

  explicit ScriptStencil(JSContext* cx) : ScriptStencilBase(cx) {}

  bool isFunction() const {
    return immutableFlags.hasFlag(ImmutableFlags::IsFunction);
  }

  // Allocate a JSScript and initialize it with bytecode. This consumes
  // allocations within this stencil.
  JSScript* intoScript(JSContext* cx, CompilationInfo& compilationInfo,
                       SourceExtent extent);

  // Store all atoms into `atoms`
  // `atoms` is the pointer to `this.natoms`-length array of `GCPtrAtom`.
  virtual void initAtomMap(GCPtrAtom* atoms) const = 0;
};

} /* namespace js::frontend */

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
