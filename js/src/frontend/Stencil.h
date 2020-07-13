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
#include "vm/JSScript.h"  // GeneratorKind, FunctionAsyncKind, FieldInitializers
#include "vm/Runtime.h"   // ReportOutOfMemory
#include "vm/Scope.h"  // BaseScopeData, FunctionScope, LexicalScope, VarScope, GlobalScope, EvalScope, ModuleScope
#include "vm/ScopeKind.h"      // ScopeKind
#include "vm/SharedStencil.h"  // ImmutableScriptFlags
#include "vm/StencilEnums.h"   // ImmutableScriptFlagsEnum

class JS_PUBLIC_API JSTracer;

namespace js::frontend {

struct CompilationInfo;

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

  // Canonical function if this is a FunctionScope.
  mozilla::Maybe<FunctionIndex> functionIndex_;

  // True if this is a FunctionScope for an arrow function.
  bool isArrow_;

  UniquePtr<BaseScopeData> data_;

 public:
  ScopeCreationData(
      JSContext* cx, ScopeKind kind, Handle<AbstractScopePtr> enclosing,
      Handle<frontend::EnvironmentShapeCreationData> environmentShape,
      UniquePtr<BaseScopeData> data = {},
      mozilla::Maybe<FunctionIndex> functionIndex = mozilla::Nothing(),
      bool isArrow = false)
      : enclosing_(enclosing),
        kind_(kind),
        environmentShape_(environmentShape),  // Copied
        functionIndex_(functionIndex),
        isArrow_(isArrow),
        data_(std::move(data)) {}

  ScopeKind kind() const { return kind_; }
  AbstractScopePtr enclosing() { return enclosing_; }

  Scope* getEnclosingScope(JSContext* cx);

  // FunctionScope
  static bool create(JSContext* cx, frontend::CompilationInfo& compilationInfo,
                     Handle<FunctionScope::Data*> dataArg,
                     bool hasParameterExprs, bool needsEnvironment,
                     FunctionIndex functionIndex, bool isArrow,
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
  JSFunction* function(frontend::CompilationInfo& compilationInfo);

  bool isArrow() const { return isArrow_; }

  bool hasScope() const { return scope_ != nullptr; }

  Scope* getScope() const {
    MOZ_ASSERT(hasScope());
    return scope_;
  }

  Scope* createScope(JSContext* cx, CompilationInfo& compilationInfo);

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
  UniquePtr<typename SpecificScopeType::Data> releaseData(
      CompilationInfo& compilationInfo);

  template <typename SpecificScopeType>
  Scope* createSpecificScope(JSContext* cx, CompilationInfo& compilationInfo);

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

// See JSOp::Lambda for interepretation of this index.
using FunctionDeclaration = uint32_t;
using FunctionDeclarationVector = Vector<FunctionDeclaration>;

// Common type for ImportEntry / ExportEntry / ModuleRequest within frontend. We
// use a shared stencil class type to simplify serialization.
//
// https://tc39.es/ecma262/#importentry-record
// https://tc39.es/ecma262/#exportentry-record
//
// Note: We subdivide the spec's ExportEntry into ExportAs / ExportFrom forms
//       for readability.
class StencilModuleEntry {
 public:
  //              | ModuleRequest | ImportEntry | ExportAs | ExportFrom |
  //              |-----------------------------------------------------|
  // specifier    | required      | required    | nullptr  | required   |
  // localName    | null          | required    | required | nullptr    |
  // importName   | null          | required    | nullptr  | required   |
  // exportName   | null          | null        | required | optional   |
  JSAtom* specifier = nullptr;
  JSAtom* localName = nullptr;
  JSAtom* importName = nullptr;
  JSAtom* exportName = nullptr;

  // Location used for error messages. If this is for a module request entry
  // then it is the module specifier string, otherwise the import/export spec
  // that failed. Exports may not fill these fields if an error cannot be
  // generated such as `export let x;`.
  uint32_t lineno = 0;
  uint32_t column = 0;

 private:
  StencilModuleEntry(uint32_t lineno, uint32_t column)
      : lineno(lineno), column(column) {}

 public:
  static StencilModuleEntry moduleRequest(JSAtom* specifier, uint32_t lineno,
                                          uint32_t column) {
    MOZ_ASSERT(specifier);
    StencilModuleEntry entry(lineno, column);
    entry.specifier = specifier;
    return entry;
  }

  static StencilModuleEntry importEntry(JSAtom* specifier, JSAtom* localName,
                                        JSAtom* importName, uint32_t lineno,
                                        uint32_t column) {
    MOZ_ASSERT(specifier && localName && importName);
    StencilModuleEntry entry(lineno, column);
    entry.specifier = specifier;
    entry.localName = localName;
    entry.importName = importName;
    return entry;
  }

  static StencilModuleEntry exportAsEntry(JSAtom* localName, JSAtom* exportName,
                                          uint32_t lineno, uint32_t column) {
    MOZ_ASSERT(localName && exportName);
    StencilModuleEntry entry(lineno, column);
    entry.localName = localName;
    entry.exportName = exportName;
    return entry;
  }

  static StencilModuleEntry exportFromEntry(JSAtom* specifier,
                                            JSAtom* importName,
                                            JSAtom* exportName, uint32_t lineno,
                                            uint32_t column) {
    // NOTE: The `export * from "mod";` syntax generates nullptr exportName.
    MOZ_ASSERT(specifier && importName);
    StencilModuleEntry entry(lineno, column);
    entry.specifier = specifier;
    entry.importName = importName;
    entry.exportName = exportName;
    return entry;
  }

  // This traces the JSAtoms. This will be removed once atoms are deferred from
  // parsing.
  void trace(JSTracer* trc);
};

// Metadata generated by parsing module scripts, including import/export tables.
class StencilModuleMetadata {
 public:
  using EntryVector = JS::GCVector<StencilModuleEntry>;

  EntryVector requestedModules;
  EntryVector importEntries;
  EntryVector localExportEntries;
  EntryVector indirectExportEntries;
  EntryVector starExportEntries;
  FunctionDeclarationVector functionDecls;

  explicit StencilModuleMetadata(JSContext* cx)
      : requestedModules(cx),
        importEntries(cx),
        localExportEntries(cx),
        indirectExportEntries(cx),
        starExportEntries(cx),
        functionDecls(cx) {}

  bool initModule(JSContext* cx, JS::Handle<ModuleObject*> module);

  void trace(JSTracer* trc);
};

// The lazy closed-over-binding info is represented by these types that will
// convert to a GCCellPtr(nullptr), GCCellPtr(JSAtom*).
class NullScriptThing {};
using ScriptAtom = JSAtom*;

// These types all end up being baked into GC things as part of stencil
// instantiation.
using ScriptThingVariant =
    mozilla::Variant<ScriptAtom, NullScriptThing, BigIntIndex,
                     ObjLiteralCreationData, RegExpIndex, ScopeIndex,
                     FunctionIndex, EmptyGlobalScopeType>;

// A vector of things destined to be converted to GC things.
using ScriptThingsVector = Vector<ScriptThingVariant>;

// Data generated by frontend that will be used to create a js::BaseScript.
class ScriptStencil {
 public:
  // Fields for BaseScript.
  // Used by:
  //   * Global script
  //   * Eval
  //   * Module
  //   * non-lazy Function (except asm.js module)
  //   * lazy Function (cannot be asm.js module)

  // See `BaseScript::immutableFlags_`.
  ImmutableScriptFlags immutableFlags;

  // See `BaseScript::data_`.
  mozilla::Maybe<FieldInitializers> fieldInitializers;
  ScriptThingsVector gcThings;

  // See `BaseScript::sharedData_`.
  js::UniquePtr<js::ImmutableScriptData> immutableScriptData = nullptr;

  // The location of this script in the source.
  SourceExtent extent = {};

  // Fields for JSFunction.
  // Used by:
  //   * non-lazy Function
  //   * lazy Function
  //   * asm.js module

  // The explicit or implicit name of the function. The FunctionFlags indicate
  // the kind of name.
  JSAtom* functionAtom = nullptr;

  // See: `FunctionFlags`.
  FunctionFlags functionFlags = {};

  // See `JSFunction::nargs_`.
  uint16_t nargs = 0;

  // If this ScriptStencil refers to a lazy child of the function being
  // compiled, this field holds the child's immediately enclosing scope's index.
  // Once compilation succeeds, we will store the scope pointed by this in the
  // child's BaseScript.  (Debugger may become confused if lazy scripts refer to
  // partially initialized enclosing scopes, so we must avoid storing the
  // scope in the BaseScript until compilation has completed
  // successfully.)
  mozilla::Maybe<ScopeIndex> lazyFunctionEnclosingScopeIndex_;

  // This function is a standalone function that is not syntactically part of
  // another script. Eg. Created by `new Function("")`.
  bool isStandaloneFunction : 1;

  // This is set by the BytecodeEmitter of the enclosing script when a reference
  // to this function is generated.
  bool wasFunctionEmitted : 1;

  // This function should be marked as a singleton. It is expected to be defined
  // at most once. This is a heuristic only and does not affect correctness.
  bool isSingletonFunction : 1;

  // End of fields.

  explicit ScriptStencil(JSContext* cx)
      : gcThings(cx),
        isStandaloneFunction(false),
        wasFunctionEmitted(false),
        isSingletonFunction(false) {}

  // This traces any JSAtoms in the gcThings array. This will be removed once
  // atoms are deferred from parsing.
  void trace(JSTracer* trc);

  bool isFunction() const {
    bool result = functionFlags.toRaw() != 0x0000;
    MOZ_ASSERT_IF(
        result, functionFlags.isAsmJSNative() || functionFlags.hasBaseScript());
    return result;
  }

  bool isModule() const {
    bool result = immutableFlags.hasFlag(ImmutableScriptFlagsEnum::IsModule);
    MOZ_ASSERT_IF(result, !isFunction());
    return result;
  }
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
