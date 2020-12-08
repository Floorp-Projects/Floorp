/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef frontend_Stencil_h
#define frontend_Stencil_h

#include "mozilla/Assertions.h"  // MOZ_ASSERT
#include "mozilla/Attributes.h"  // MOZ_MUST_USE
#include "mozilla/Maybe.h"       // mozilla::{Maybe, Nothing}
#include "mozilla/Range.h"       // mozilla::Range
#include "mozilla/Span.h"        // mozilla::Span
#include "mozilla/Variant.h"     // mozilla::Variant

#include <stddef.h>  // size_t
#include <stdint.h>  // char16_t, uint8_t, uint16_t, uint32_t

#include "frontend/AbstractScopePtr.h"    // AbstractScopePtr, ScopeIndex
#include "frontend/FunctionSyntaxKind.h"  // FunctionSyntaxKind
#include "frontend/ObjLiteral.h"          // ObjLiteralStencil
#include "frontend/ParserAtom.h"          // ParserAtom, TaggedParserAtomIndex
#include "frontend/TypedIndex.h"          // TypedIndex
#include "js/AllocPolicy.h"               // SystemAllocPolicy
#include "js/RegExpFlags.h"               // JS::RegExpFlags
#include "js/RootingAPI.h"                // Handle
#include "js/TypeDecls.h"                 // JSContext
#include "js/UniquePtr.h"                 // js::UniquePtr
#include "js/Utility.h"                   // UniqueTwoByteChars
#include "js/Vector.h"                    // js::Vector
#include "util/Text.h"                    // DuplicateString
#include "vm/BigIntType.h"                // ParseBigIntLiteral
#include "vm/FunctionFlags.h"             // FunctionFlags
#include "vm/GeneratorAndAsyncKind.h"     // GeneratorKind, FunctionAsyncKind
#include "vm/Scope.h"  // Scope, BaseScopeData, FunctionScope, LexicalScope, VarScope, GlobalScope, EvalScope, ModuleScope
#include "vm/ScopeKind.h"      // ScopeKind
#include "vm/SharedStencil.h"  // ImmutableScriptFlags, GCThingIndex, js::SharedImmutableScriptData, MemberInitializers, SourceExtent
#include "vm/StencilEnums.h"   // ImmutableScriptFlagsEnum

namespace js {

class JSONPrinter;
class RegExpObject;

namespace frontend {

struct CompilationInfo;
struct CompilationAtomCache;
struct CompilationStencil;
struct CompilationGCOutput;
class ScriptStencil;
class RegExpStencil;
class BigIntStencil;
class StencilXDR;

using BaseParserScopeData = AbstractBaseScopeData<const ParserAtom>;

template <typename Scope>
using ParserScopeData = typename Scope::template AbstractData<const ParserAtom>;
using ParserGlobalScopeData = ParserScopeData<GlobalScope>;
using ParserEvalScopeData = ParserScopeData<EvalScope>;
using ParserLexicalScopeData = ParserScopeData<LexicalScope>;
using ParserFunctionScopeData = ParserScopeData<FunctionScope>;
using ParserModuleScopeData = ParserScopeData<ModuleScope>;
using ParserVarScopeData = ParserScopeData<VarScope>;

using ParserBindingIter = AbstractBindingIter<const ParserAtom>;

// [SMDOC] Script Stencil (Frontend Representation)
//
// Stencils are GC object free representations of artifacts created during
// parsing and bytecode emission that are being used as part of Project
// Stencil (https://bugzilla.mozilla.org/show_bug.cgi?id=stencil) to revamp
// the frontend.
//
// Renaming to use the term stencil more broadly is still in progress.

// Typed indices for the different stencil elements in the compilation result.
using RegExpIndex = TypedIndex<RegExpStencil>;
using BigIntIndex = TypedIndex<BigIntStencil>;
using ObjLiteralIndex = TypedIndex<ObjLiteralStencil>;
using FunctionIndex = TypedIndex<ScriptStencil>;

FunctionFlags InitialFunctionFlags(FunctionSyntaxKind kind,
                                   GeneratorKind generatorKind,
                                   FunctionAsyncKind asyncKind,
                                   bool isSelfHosting = false,
                                   bool hasUnclonedName = false);

// A syntax-checked regular expression string.
class RegExpStencil {
  friend class StencilXDR;

  TaggedParserAtomIndex atom_;
  JS::RegExpFlags flags_;

 public:
  RegExpStencil() = default;

  RegExpStencil(TaggedParserAtomIndex atom, JS::RegExpFlags flags)
      : atom_(atom), flags_(flags) {}

  RegExpObject* createRegExp(JSContext* cx,
                             CompilationAtomCache& atomCache) const;

  // This is used by `Reflect.parse` when we need the RegExpObject but are not
  // doing a complete instantiation of the CompilationStencil.
  RegExpObject* createRegExpAndEnsureAtom(JSContext* cx,
                                          CompilationAtomCache& atomCache,
                                          CompilationStencil& stencil) const;

#if defined(DEBUG) || defined(JS_JITSPEW)
  void dump();
  void dump(JSONPrinter& json, CompilationStencil* compilationStencil);
  void dumpFields(JSONPrinter& json, CompilationStencil* compilationStencil);
#endif
};

// This owns a set of characters guaranteed to parse into a BigInt via
// ParseBigIntLiteral. Used to avoid allocating the BigInt on the
// GC heap during parsing.
class BigIntStencil {
  friend class StencilXDR;

  UniqueTwoByteChars buf_;
  size_t length_ = 0;

 public:
  BigIntStencil() = default;

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

  BigInt* createBigInt(JSContext* cx) const {
    mozilla::Range<const char16_t> source(buf_.get(), length_);

    return js::ParseBigIntLiteral(cx, source);
  }

  bool isZero() const {
    mozilla::Range<const char16_t> source(buf_.get(), length_);
    return js::BigIntLiteralIsZero(source);
  }

#if defined(DEBUG) || defined(JS_JITSPEW)
  void dump();
  void dump(JSONPrinter& json);
#endif
};

class ScopeStencil {
  friend class StencilXDR;

  // The enclosing scope. If Nothing, then the enclosing scope of the
  // compilation applies.
  mozilla::Maybe<ScopeIndex> enclosing_;

  // The kind determines data_.
  ScopeKind kind_{UINT8_MAX};

  // First frame slot to use, or LOCALNO_LIMIT if none are allowed.
  uint32_t firstFrameSlot_ = UINT32_MAX;

  // If Some, then an environment Shape must be created. The shape itself may
  // have no slots if the environment may be extensible later.
  mozilla::Maybe<uint32_t> numEnvironmentSlots_;

  // Canonical function if this is a FunctionScope.
  mozilla::Maybe<FunctionIndex> functionIndex_;

  // True if this is a FunctionScope for an arrow function.
  bool isArrow_ = false;

  // The list of binding and scope-specific data.
  // Note: The back pointers to the owning JSFunction / ModuleObject are not set
  //       until Stencils are converted to GC allocations.
  // Note: This allocation is owned by CompilationStencil.
  BaseParserScopeData* data_ = nullptr;

 public:
  // For XDR only.
  ScopeStencil() = default;

  ScopeStencil(ScopeKind kind, mozilla::Maybe<ScopeIndex> enclosing,
               uint32_t firstFrameSlot,
               mozilla::Maybe<uint32_t> numEnvironmentSlots,
               BaseParserScopeData* data = {},
               mozilla::Maybe<FunctionIndex> functionIndex = mozilla::Nothing(),
               bool isArrow = false)
      : enclosing_(enclosing),
        kind_(kind),
        firstFrameSlot_(firstFrameSlot),
        numEnvironmentSlots_(numEnvironmentSlots),
        functionIndex_(functionIndex),
        isArrow_(isArrow),
        data_(data) {}

  static bool createForFunctionScope(JSContext* cx, CompilationStencil& stencil,
                                     ParserFunctionScopeData* dataArg,
                                     bool hasParameterExprs,
                                     bool needsEnvironment,
                                     FunctionIndex functionIndex, bool isArrow,
                                     mozilla::Maybe<ScopeIndex> enclosing,
                                     ScopeIndex* index);

  static bool createForLexicalScope(JSContext* cx, CompilationStencil& stencil,
                                    ScopeKind kind,
                                    ParserLexicalScopeData* dataArg,
                                    uint32_t firstFrameSlot,
                                    mozilla::Maybe<ScopeIndex> enclosing,
                                    ScopeIndex* index);

  static bool createForVarScope(JSContext* cx,
                                frontend::CompilationStencil& stencil,
                                ScopeKind kind, ParserVarScopeData* dataArg,
                                uint32_t firstFrameSlot, bool needsEnvironment,
                                mozilla::Maybe<ScopeIndex> enclosing,
                                ScopeIndex* index);

  static bool createForGlobalScope(JSContext* cx, CompilationStencil& stencil,
                                   ScopeKind kind,
                                   ParserGlobalScopeData* dataArg,
                                   ScopeIndex* index);

  static bool createForEvalScope(JSContext* cx, CompilationStencil& stencil,
                                 ScopeKind kind, ParserEvalScopeData* dataArg,
                                 mozilla::Maybe<ScopeIndex> enclosing,
                                 ScopeIndex* index);

  static bool createForModuleScope(JSContext* cx, CompilationStencil& stencil,
                                   ParserModuleScopeData* dataArg,
                                   mozilla::Maybe<ScopeIndex> enclosing,
                                   ScopeIndex* index);

  static bool createForWithScope(JSContext* cx, CompilationStencil& stencil,
                                 mozilla::Maybe<ScopeIndex> enclosing,
                                 ScopeIndex* index);

  AbstractScopePtr enclosing(CompilationInfo& compilationInfo) const;
  js::Scope* enclosingExistingScope(const CompilationInput& input,
                                    const CompilationGCOutput& gcOutput) const;

  ScopeKind kind() const { return kind_; }

  bool hasEnvironment() const {
    // Check if scope kind alone means we have an env shape, and
    // otherwise check if we have one created.
    bool hasEnvironmentShape = numEnvironmentSlots_.isSome();
    return Scope::hasEnvironment(kind(), hasEnvironmentShape);
  }

  bool isArrow() const { return isArrow_; }

  Scope* createScope(JSContext* cx, CompilationInput& input,
                     CompilationGCOutput& gcOutput) const;

  uint32_t nextFrameSlot() const;

#if defined(DEBUG) || defined(JS_JITSPEW)
  void dump();
  void dump(JSONPrinter& json);
  void dumpFields(JSONPrinter& json);
#endif

 private:
  // Non owning reference to data
  template <typename SpecificScopeType>
  typename SpecificScopeType::template AbstractData<const ParserAtom>& data()
      const {
    using Data =
        typename SpecificScopeType ::template AbstractData<const ParserAtom>;

    MOZ_ASSERT(data_);
    return *static_cast<Data*>(data_);
  }

  // Transfer ownership into a new UniquePtr.
  template <typename SpecificScopeType>
  UniquePtr<typename SpecificScopeType::Data> createSpecificScopeData(
      JSContext* cx, CompilationAtomCache& atomCache,
      CompilationGCOutput& gcOutput) const;

  template <typename SpecificScopeType>
  uint32_t nextFrameSlot() const {
    // If a scope has been allocated for the ScopeStencil we no longer own data,
    // so defer to scope
    return data<SpecificScopeType>().nextFrameSlot;
  }

  template <typename SpecificEnvironmentType>
  MOZ_MUST_USE bool createSpecificShape(JSContext* cx, ScopeKind kind,
                                        BaseScopeData* scopeData,
                                        MutableHandleShape shape) const;

  template <typename SpecificScopeType, typename SpecificEnvironmentType>
  Scope* createSpecificScope(JSContext* cx, CompilationInput& input,
                             CompilationGCOutput& gcOutput) const;
};

// See JSOp::Lambda for interepretation of this index.
using FunctionDeclaration = GCThingIndex;
using FunctionDeclarationVector =
    Vector<FunctionDeclaration, 0, js::SystemAllocPolicy>;

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
  TaggedParserAtomIndex specifier;
  TaggedParserAtomIndex localName;
  TaggedParserAtomIndex importName;
  TaggedParserAtomIndex exportName;

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
  // For XDR only.
  StencilModuleEntry() = default;

  static StencilModuleEntry moduleRequest(TaggedParserAtomIndex specifier,
                                          uint32_t lineno, uint32_t column) {
    MOZ_ASSERT(!!specifier);
    StencilModuleEntry entry(lineno, column);
    entry.specifier = specifier;
    return entry;
  }

  static StencilModuleEntry importEntry(TaggedParserAtomIndex specifier,
                                        TaggedParserAtomIndex localName,
                                        TaggedParserAtomIndex importName,
                                        uint32_t lineno, uint32_t column) {
    MOZ_ASSERT(specifier && localName && importName);
    StencilModuleEntry entry(lineno, column);
    entry.specifier = specifier;
    entry.localName = localName;
    entry.importName = importName;
    return entry;
  }

  static StencilModuleEntry exportAsEntry(TaggedParserAtomIndex localName,
                                          TaggedParserAtomIndex exportName,
                                          uint32_t lineno, uint32_t column) {
    MOZ_ASSERT(localName && exportName);
    StencilModuleEntry entry(lineno, column);
    entry.localName = localName;
    entry.exportName = exportName;
    return entry;
  }

  static StencilModuleEntry exportFromEntry(TaggedParserAtomIndex specifier,
                                            TaggedParserAtomIndex importName,
                                            TaggedParserAtomIndex exportName,
                                            uint32_t lineno, uint32_t column) {
    // NOTE: The `export * from "mod";` syntax generates nullptr exportName.
    MOZ_ASSERT(specifier && importName);
    StencilModuleEntry entry(lineno, column);
    entry.specifier = specifier;
    entry.importName = importName;
    entry.exportName = exportName;
    return entry;
  }
};

// Metadata generated by parsing module scripts, including import/export tables.
class StencilModuleMetadata {
 public:
  using EntryVector = Vector<StencilModuleEntry, 0, js::SystemAllocPolicy>;

  EntryVector requestedModules;
  EntryVector importEntries;
  EntryVector localExportEntries;
  EntryVector indirectExportEntries;
  EntryVector starExportEntries;
  FunctionDeclarationVector functionDecls;
  // Set to true if the module has a top-level await keyword.
  bool isAsync = false;

  StencilModuleMetadata() = default;

  bool initModule(JSContext* cx, CompilationAtomCache& atomCache,
                  JS::Handle<ModuleObject*> module) const;

#if defined(DEBUG) || defined(JS_JITSPEW)
  void dump();
  void dump(JSONPrinter& json, CompilationStencil* compilationStencil);
  void dumpFields(JSONPrinter& json, CompilationStencil* compilationStencil);
#endif
};

// As an alternative to a ScopeIndex (which references a ScopeStencil), we may
// instead refer to an existing scope from GlobalObject::emptyGlobalScope().
//
// NOTE: This is only used for the self-hosting global.
class EmptyGlobalScopeType {};

// Things pointed by this index all end up being baked into GC things as part
// of stencil instantiation.
//
// 0x0000_0000  Null
// 0x1YYY_YYYY  28-bit ParserAtom
// 0x2YYY_YYYY  Well-known/static atom (See TaggedParserAtomIndex)
// 0x3YYY_YYYY  28-bit BigInt
// 0x4YYY_YYYY  28-bit ObjLiteral
// 0x5YYY_YYYY  28-bit RegExp
// 0x6YYY_YYYY  28-bit Scope
// 0x7YYY_YYYY  28-bit Function
// 0x8000_0000  EmptyGlobalScope
class TaggedScriptThingIndex {
  uint32_t data_;

  static constexpr size_t IndexBit = TaggedParserAtomIndex::IndexBit;
  static constexpr size_t IndexMask = TaggedParserAtomIndex::IndexMask;

  static constexpr size_t TagShift = TaggedParserAtomIndex::TagShift;
  static constexpr size_t TagBit = TaggedParserAtomIndex::TagBit;
  static constexpr size_t TagMask = TaggedParserAtomIndex::TagMask;

 public:
  enum class Kind : uint32_t {
    Null = uint32_t(TaggedParserAtomIndex::Kind::Null),
    ParserAtomIndex = uint32_t(TaggedParserAtomIndex::Kind::ParserAtomIndex),
    WellKnown = uint32_t(TaggedParserAtomIndex::Kind::WellKnown),
    BigInt,
    ObjLiteral,
    RegExp,
    Scope,
    Function,
    EmptyGlobalScope,
  };

 private:
  static constexpr uint32_t NullTag = uint32_t(Kind::Null) << TagShift;
  static_assert(NullTag == TaggedParserAtomIndex::NullTag);
  static constexpr uint32_t ParserAtomIndexTag = uint32_t(Kind::ParserAtomIndex)
                                                 << TagShift;
  static_assert(ParserAtomIndexTag ==
                TaggedParserAtomIndex::ParserAtomIndexTag);
  static constexpr uint32_t WellKnownTag = uint32_t(Kind::WellKnown)
                                           << TagShift;
  static_assert(WellKnownTag == TaggedParserAtomIndex::WellKnownTag);

  static constexpr uint32_t BigIntTag = uint32_t(Kind::BigInt) << TagShift;
  static constexpr uint32_t ObjLiteralTag = uint32_t(Kind::ObjLiteral)
                                            << TagShift;
  static constexpr uint32_t RegExpTag = uint32_t(Kind::RegExp) << TagShift;
  static constexpr uint32_t ScopeTag = uint32_t(Kind::Scope) << TagShift;
  static constexpr uint32_t FunctionTag = uint32_t(Kind::Function) << TagShift;
  static constexpr uint32_t EmptyGlobalScopeTag =
      uint32_t(Kind::EmptyGlobalScope) << TagShift;

 public:
  static constexpr uint32_t IndexLimit = Bit(IndexBit);

  TaggedScriptThingIndex() : data_(NullTag) {}

  explicit TaggedScriptThingIndex(TaggedParserAtomIndex index)
      : data_(*index.rawData()) {}
  explicit TaggedScriptThingIndex(BigIntIndex index)
      : data_(uint32_t(index) | BigIntTag) {
    MOZ_ASSERT(uint32_t(index) < IndexLimit);
  }
  explicit TaggedScriptThingIndex(ObjLiteralIndex index)
      : data_(uint32_t(index) | ObjLiteralTag) {
    MOZ_ASSERT(uint32_t(index) < IndexLimit);
  }
  explicit TaggedScriptThingIndex(RegExpIndex index)
      : data_(uint32_t(index) | RegExpTag) {
    MOZ_ASSERT(uint32_t(index) < IndexLimit);
  }
  explicit TaggedScriptThingIndex(ScopeIndex index)
      : data_(uint32_t(index) | ScopeTag) {
    MOZ_ASSERT(uint32_t(index) < IndexLimit);
  }
  explicit TaggedScriptThingIndex(FunctionIndex index)
      : data_(uint32_t(index) | FunctionTag) {
    MOZ_ASSERT(uint32_t(index) < IndexLimit);
  }
  explicit TaggedScriptThingIndex(EmptyGlobalScopeType t)
      : data_(EmptyGlobalScopeTag) {}

  bool isAtom() const {
    return (data_ & TagMask) == ParserAtomIndexTag ||
           (data_ & TagMask) == WellKnownTag;
  }
  bool isNull() const {
    bool result = !data_;
    MOZ_ASSERT_IF(result, (data_ & TagMask) == NullTag);
    return result;
  }
  bool isBigInt() const { return (data_ & TagMask) == BigIntTag; }
  bool isObjLiteral() const { return (data_ & TagMask) == ObjLiteralTag; }
  bool isRegExp() const { return (data_ & TagMask) == RegExpTag; }
  bool isScope() const { return (data_ & TagMask) == ScopeTag; }
  bool isFunction() const { return (data_ & TagMask) == FunctionTag; }
  bool isEmptyGlobalScope() const {
    return (data_ & TagMask) == EmptyGlobalScopeTag;
  }

  TaggedParserAtomIndex toAtom() const {
    MOZ_ASSERT(isAtom());
    return TaggedParserAtomIndex::fromRaw(data_);
  }
  BigIntIndex toBigInt() const { return BigIntIndex(data_ & IndexMask); }
  ObjLiteralIndex toObjLiteral() const {
    return ObjLiteralIndex(data_ & IndexMask);
  }
  RegExpIndex toRegExp() const { return RegExpIndex(data_ & IndexMask); }
  ScopeIndex toScope() const { return ScopeIndex(data_ & IndexMask); }
  FunctionIndex toFunction() const { return FunctionIndex(data_ & IndexMask); }

  uint32_t* rawData() { return &data_; }

  Kind tag() const { return Kind((data_ & TagMask) >> TagShift); }

  bool operator==(const TaggedScriptThingIndex& rhs) const {
    return data_ == rhs.data_;
  }
};

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
  // NOTE: The backing memory is owned by the CompilationStencil.
  mozilla::Maybe<MemberInitializers> memberInitializers;
  mozilla::Span<TaggedScriptThingIndex> gcThings;

  // See `BaseScript::sharedData_`.
  RefPtr<js::SharedImmutableScriptData> sharedData = {};

  // The location of this script in the source.
  SourceExtent extent = {};

  // Fields for JSFunction.
  // Used by:
  //   * non-lazy Function
  //   * lazy Function
  //   * asm.js module

  // The explicit or implicit name of the function. The FunctionFlags indicate
  // the kind of name.
  TaggedParserAtomIndex functionAtom;

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

  // This is set by the BytecodeEmitter of the enclosing script when a reference
  // to this function is generated.
  bool wasFunctionEmitted : 1;

  // If this is for the root of delazification, this represents
  // MutableScriptFlagsEnum::AllowRelazify value of the script *after*
  // delazification.
  // False otherwise.
  bool allowRelazify : 1;

  // End of fields.

  ScriptStencil() : wasFunctionEmitted(false), allowRelazify(false) {}

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

#if defined(DEBUG) || defined(JS_JITSPEW)
  void dump();
  void dump(JSONPrinter& json, CompilationStencil* compilationStencil);
  void dumpFields(JSONPrinter& json, CompilationStencil* compilationStencil);
#endif
};

#if defined(DEBUG) || defined(JS_JITSPEW)
void DumpTaggedParserAtomIndex(js::JSONPrinter& json,
                               TaggedParserAtomIndex taggedIndex,
                               CompilationStencil* compilationStencil);
#endif

} /* namespace frontend */
} /* namespace js */

#endif /* frontend_Stencil_h */
