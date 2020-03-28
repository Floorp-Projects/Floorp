/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_SharedStencil_h
#define vm_SharedStencil_h

#include <stddef.h>  // size_t
#include <stdint.h>  // uint32_t

#include "jstypes.h"
#include "js/CompileOptions.h"

/*
 * Data shared between the vm and stencil structures.
 */

namespace js {

/*
 * [SMDOC] Try Notes
 *
 * Trynotes are attached to regions that are involved with
 * exception unwinding. They can be broken up into four categories:
 *
 * 1. CATCH and FINALLY: Basic exception handling. A CATCH trynote
 *    covers the range of the associated try. A FINALLY trynote covers
 *    the try and the catch.

 * 2. FOR_IN and DESTRUCTURING: These operations create an iterator
 *    which must be cleaned up (by calling IteratorClose) during
 *    exception unwinding.
 *
 * 3. FOR_OF and FOR_OF_ITERCLOSE: For-of loops handle unwinding using
 *    catch blocks. These trynotes are used for for-of breaks/returns,
 *    which create regions that are lexically within a for-of block,
 *    but logically outside of it. See TryNoteIter::settle for more
 *    details.
 *
 * 4. LOOP: This represents normal for/while/do-while loops. It is
 *    unnecessary for exception unwinding, but storing the boundaries
 *    of loops here is helpful for heuristics that need to know
 *    whether a given op is inside a loop.
 */
enum TryNoteKind {
  JSTRY_CATCH,
  JSTRY_FINALLY,
  JSTRY_FOR_IN,
  JSTRY_DESTRUCTURING,
  JSTRY_FOR_OF,
  JSTRY_FOR_OF_ITERCLOSE,
  JSTRY_LOOP
};

/*
 * Exception handling record.
 */
struct TryNote {
  uint32_t kind;       /* one of TryNoteKind */
  uint32_t stackDepth; /* stack depth upon exception handler entry */
  uint32_t start;      /* start of the try statement or loop relative
                          to script->code() */
  uint32_t length;     /* length of the try statement or loop */

  TryNote(uint32_t kind, uint32_t stackDepth, uint32_t start, uint32_t length)
      : kind(kind), stackDepth(stackDepth), start(start), length(length) {}

  TryNote() = default;

  bool isLoop() const {
    switch (kind) {
      case JSTRY_LOOP:
      case JSTRY_FOR_IN:
      case JSTRY_FOR_OF:
        return true;
      case JSTRY_CATCH:
      case JSTRY_FINALLY:
      case JSTRY_FOR_OF_ITERCLOSE:
      case JSTRY_DESTRUCTURING:
        return false;
    }
    MOZ_CRASH("Unexpected try note kind");
  }
};

// A block scope has a range in bytecode: it is entered at some offset, and left
// at some later offset.  Scopes can be nested.  Given an offset, the
// ScopeNote containing that offset whose with the highest start value
// indicates the block scope.  The block scope list is sorted by increasing
// start value.
//
// It is possible to leave a scope nonlocally, for example via a "break"
// statement, so there may be short bytecode ranges in a block scope in which we
// are popping the block chain in preparation for a goto.  These exits are also
// nested with respect to outer scopes.  The scopes in these exits are indicated
// by the "index" field, just like any other block.  If a nonlocal exit pops the
// last block scope, the index will be NoScopeIndex.
//
struct ScopeNote {
  // Sentinel index for no Scope.
  static const uint32_t NoScopeIndex = UINT32_MAX;

  // Sentinel index for no ScopeNote.
  static const uint32_t NoScopeNoteIndex = UINT32_MAX;

  // Index of the js::Scope in the script's gcthings array, or NoScopeIndex if
  // there is no block scope in this range.
  uint32_t index = 0;

  // Bytecode offset at which this scope starts relative to script->code().
  uint32_t start = 0;

  // Length of bytecode span this scope covers.
  uint32_t length = 0;

  // Index of parent block scope in notes, or NoScopeNote.
  uint32_t parent = 0;
};

template <typename EnumType>
class ScriptFlagBase {
  // To allow cross-checking offsetof assert.
  friend class js::BaseScript;

 protected:
  // Stored as a uint32_t to make access more predictable from
  // JIT code.
  uint32_t flags_ = 0;

 public:
  MOZ_MUST_USE bool hasFlag(EnumType flag) const {
    return flags_ & static_cast<uint32_t>(flag);
  }
  void setFlag(EnumType flag) { flags_ |= static_cast<uint32_t>(flag); }
  void clearFlag(EnumType flag) { flags_ &= ~static_cast<uint32_t>(flag); }
  void setFlag(EnumType flag, bool b) {
    if (b) {
      setFlag(flag);
    } else {
      clearFlag(flag);
    }
  }

  operator uint32_t() const { return flags_; }
};

enum class ImmutableScriptFlagsEnum : uint32_t {
  // Input Flags
  //
  // Flags that come from CompileOptions.
  // ----

  // No need for result value of last expression statement.
  NoScriptRval = 1 << 0,

  // See Parser::selfHostingMode.
  SelfHosted = 1 << 1,

  // Script is a lambda to treat as running once or a global or eval script
  // that will only run once.  Which one it is can be disambiguated by
  // checking whether function() is null.
  TreatAsRunOnce = 1 << 2,

  // Code was forced into strict mode using CompileOptions.
  ForceStrict = 1 << 3,
  // ----

  // Parser Flags
  //
  // Flags that come from the parser.
  // ----

  // Code is in strict mode.
  Strict = 1 << 4,

  // See FunctionBox.
  BindingsAccessedDynamically = 1 << 5,
  FunHasExtensibleScope = 1 << 6,

  // Bytecode contains JSOp::CallSiteObj
  // (We don't relazify functions with template strings, due to observability)
  HasCallSiteObj = 1 << 7,

  // Script is parsed with a top-level goal of Module. This may be a top-level
  // or an inner-function script.
  HasModuleGoal = 1 << 8,

  FunctionHasThisBinding = 1 << 9,

  // Whether the arguments object for this script, if it needs one, should be
  // mapped (alias formal parameters).
  HasMappedArgsObj = 1 << 10,

  // Script contains inner functions. Used to check if we can relazify the
  // script.
  HasInnerFunctions = 1 << 11,

  NeedsHomeObject = 1 << 12,
  IsDerivedClassConstructor = 1 << 13,

  // 'this', 'arguments' and f.apply() are used. This is likely to be a
  // wrapper.
  IsLikelyConstructorWrapper = 1 << 14,

  // Set if this function is a generator function or async generator.
  IsGenerator = 1 << 15,

  // Set if this function is an async function or async generator.
  IsAsync = 1 << 16,

  // Set if this function has a rest parameter.
  HasRest = 1 << 17,

  // See comments below.
  ArgumentsHasVarBinding = 1 << 18,

  // Script came from eval().
  IsForEval = 1 << 19,

  // Whether this is a top-level module script.
  IsModule = 1 << 20,

  // Whether the Parser declared 'arguments'.
  ShouldDeclareArguments = 1 << 21,

  // Script is for function.
  IsFunction = 1 << 22,

  // Whether this script contains a direct eval statement.
  HasDirectEval = 1 << 23,
  // ----

  // Bytecode Emitter Flags
  //
  // Flags that are initialized by the BCE.
  // ----

  // True if the script has a non-syntactic scope on its dynamic scope chain.
  // That is, there are objects about which we know nothing between the
  // outermost syntactic scope and the global.
  HasNonSyntacticScope = 1 << 24,

  FunctionHasExtraBodyVarScope = 1 << 25,

  // Whether this function needs a call object or named lambda environment.
  NeedsFunctionEnvironmentObjects = 1 << 26,
};

class ImmutableScriptFlags : public ScriptFlagBase<ImmutableScriptFlagsEnum> {
  // Immutable flags should not be modified after the JSScript that contains
  // them has been initialized. These flags should likely be preserved when
  // serializing (XDR) or copying (CopyScript) the script. This is only public
  // for the JITs.
  //
  // Specific accessors for flag values are defined with
  // IMMUTABLE_FLAG_* macros below.
 public:
  ImmutableScriptFlags() = default;

  void static_asserts() {
    static_assert(sizeof(ImmutableScriptFlags) == sizeof(flags_),
                  "No extra members allowed");
    static_assert(offsetof(ImmutableScriptFlags, flags_) == 0,
                  "Required for JIT flag access");
  }

  void operator=(uint32_t flag) { flags_ = flag; }

  static ImmutableScriptFlags fromCompileOptions(
      const JS::ReadOnlyCompileOptions& options) {
    ImmutableScriptFlags isf;
    isf.setFlag(ImmutableScriptFlagsEnum::NoScriptRval, options.noScriptRval);
    isf.setFlag(ImmutableScriptFlagsEnum::SelfHosted, options.selfHostingMode);
    isf.setFlag(ImmutableScriptFlagsEnum::TreatAsRunOnce, options.isRunOnce);
    isf.setFlag(ImmutableScriptFlagsEnum::ForceStrict,
                options.forceStrictMode());
    return isf;
  };

  static ImmutableScriptFlags fromCompileOptions(
      const JS::TransitiveCompileOptions& options) {
    ImmutableScriptFlags isf;
    isf.setFlag(ImmutableScriptFlagsEnum::NoScriptRval,
                /* noScriptRval (non-transitive compile option) = */ false);
    isf.setFlag(ImmutableScriptFlagsEnum::SelfHosted, options.selfHostingMode);
    isf.setFlag(ImmutableScriptFlagsEnum::TreatAsRunOnce,
                /* isRunOnce (non-transitive compile option) = */ false);
    isf.setFlag(ImmutableScriptFlagsEnum::ForceStrict,
                options.forceStrictMode());
    return isf;
  };
};

enum class MutableScriptFlagsEnum : uint32_t {
  // Number of times the |warmUpCount| was forcibly discarded. The counter is
  // reset when a script is successfully jit-compiled.
  WarmupResets_MASK = 0xFF,

  // (1 << 8) is unused

  // If treatAsRunOnce, whether script has executed.
  HasRunOnce = 1 << 9,

  // Script has been reused for a clone.
  HasBeenCloned = 1 << 10,

  // Script has an entry in Realm::scriptCountsMap.
  HasScriptCounts = 1 << 12,

  // Script has an entry in Realm::debugScriptMap.
  HasDebugScript = 1 << 13,

  // Script supports relazification where it releases bytecode and gcthings to
  // save memory. This process is opt-in since various complexities may disallow
  // this for some scripts.
  // NOTE: Must check for isRelazifiable() before setting this flag.
  AllowRelazify = 1 << 14,

  // IonMonkey compilation hints.

  // Script has had hoisted bounds checks fail.
  FailedBoundsCheck = 1 << 15,

  // Script has had hoisted shape guard fail.
  FailedShapeGuard = 1 << 16,

  HadFrequentBailouts = 1 << 17,
  HadOverflowBailout = 1 << 18,

  // Whether Baseline or Ion compilation has been disabled for this script.
  // IonDisabled is equivalent to |jitScript->canIonCompile() == false| but
  // JitScript can be discarded on GC and we don't want this to affect
  // observable behavior (see ArgumentsGetterImpl comment).
  BaselineDisabled = 1 << 19,
  IonDisabled = 1 << 20,

  // Explicitly marked as uninlineable.
  Uninlineable = 1 << 21,

  // Idempotent cache has triggered invalidation.
  InvalidatedIdempotentCache = 1 << 22,

  // Lexical check did fail and bail out.
  FailedLexicalCheck = 1 << 23,

  // See comments below.
  NeedsArgsAnalysis = 1 << 24,
  NeedsArgsObj = 1 << 25,

  // Set if the script has opted into spew
  SpewEnabled = 1 << 26,
};

class MutableScriptFlags : public ScriptFlagBase<MutableScriptFlagsEnum> {
 public:
  MutableScriptFlags() = default;

  void static_asserts() {
    static_assert(sizeof(MutableScriptFlags) == sizeof(flags_),
                  "No extra members allowed");
    static_assert(offsetof(MutableScriptFlags, flags_) == 0,
                  "Required for JIT flag access");
  }

  MutableScriptFlags& operator&=(const uint32_t rhs) {
    flags_ &= rhs;
    return *this;
  }

  MutableScriptFlags& operator|=(const uint32_t rhs) {
    flags_ |= rhs;
    return *this;
  }
};

// [SMDOC] JSScript data layout (immutable)
//
// ImmutableScriptData stores variable-length script data that may be shared
// between scripts with the same bytecode, even across different GC Zones.
// Abstractly this structure consists of multiple (optional) arrays that are
// exposed as mozilla::Span<T>. These arrays exist in a single heap allocation.
//
// Under the hood, ImmutableScriptData is a fixed-size header class followed
// the various array bodies interleaved with metadata to compactly encode the
// bounds. These arrays have varying requirements for alignment, performance,
// and jit-friendliness which leads to the complex indexing system below.
//
// Note: The '----' separators are for readability only.
//
// ----
//   <ImmutableScriptData itself>
// ----
//   (REQUIRED) Flags structure
//   (REQUIRED) Array of jsbytecode constituting code()
//   (REQUIRED) Array of jssrcnote constituting notes()
// ----
//   (OPTIONAL) Array of uint32_t optional-offsets
//  optArrayOffset:
// ----
//  L0:
//   (OPTIONAL) Array of uint32_t constituting resumeOffsets()
//  L1:
//   (OPTIONAL) Array of ScopeNote constituting scopeNotes()
//  L2:
//   (OPTIONAL) Array of TryNote constituting tryNotes()
//  L3:
// ----
//
// NOTE: The notes() array must have been null-padded such that
//       flags/code/notes together have uint32_t alignment.
//
// The labels shown are recorded as byte-offsets relative to 'this'. This is to
// reduce memory as well as make ImmutableScriptData easier to share across
// processes.
//
// The L0/L1/L2/L3 labels indicate the start and end of the optional arrays.
// Some of these labels may refer to the same location if the array between
// them is empty. Each unique label position has an offset stored in the
// optional-offsets table. Note that we also avoid entries for labels that
// match 'optArrayOffset'. This saves memory when arrays are empty.
//
// The flags() data indicates (for each optional array) which entry from the
// optional-offsets table marks the *end* of array. The array starts where the
// previous array ends (with the first array beginning at 'optArrayOffset').
// The optional-offset table is addressed at negative indices from
// 'optArrayOffset'.
//
// In general, the length of each array is computed from subtracting the start
// offset of the array from the start offset of the subsequent array. The
// notable exception is that bytecode length is stored explicitly.
class alignas(uint32_t) ImmutableScriptData final {
  // Offsets are measured in bytes relative to 'this'.
  using Offset = uint32_t;

  Offset optArrayOffset_ = 0;

  // Length of bytecode
  uint32_t codeLength_ = 0;

  // Offset of main entry point from code, after predef'ing prologue.
  uint32_t mainOffset = 0;

  // Fixed frame slots.
  uint32_t nfixed = 0;

  // Slots plus maximum stack depth.
  uint32_t nslots = 0;

  // Index into the gcthings array of the body scope.
  uint32_t bodyScopeIndex = 0;

  // Number of IC entries to allocate in JitScript for Baseline ICs.
  uint32_t numICEntries = 0;

  // ES6 function length.
  uint16_t funLength = 0;

  // Number of type sets used in this script for dynamic type monitoring.
  uint16_t numBytecodeTypeSets = 0;

  // NOTE: The raw bytes of this structure are used for hashing so use explicit
  // padding values as needed for predicatable results across compilers.

  struct Flags {
    uint8_t resumeOffsetsEndIndex : 2;
    uint8_t scopeNotesEndIndex : 2;
    uint8_t tryNotesEndIndex : 2;
    uint8_t _unused : 2;
  };
  static_assert(sizeof(Flags) == sizeof(uint8_t),
                "Structure packing is broken");

  friend class ::JSScript;

 private:
  // Offsets (in bytes) from 'this' to each component array. The delta between
  // each offset and the next offset is the size of each array and is defined
  // even if an array is empty.
  size_t flagOffset() const { return offsetOfCode() - sizeof(Flags); }
  size_t codeOffset() const { return offsetOfCode(); }
  size_t noteOffset() const { return offsetOfCode() + codeLength_; }
  size_t optionalOffsetsOffset() const {
    // Determine the location to beginning of optional-offsets array by looking
    // at index for try-notes.
    //
    //   optionalOffsetsOffset():
    //     (OPTIONAL) tryNotesEndOffset
    //     (OPTIONAL) scopeNotesEndOffset
    //     (OPTIONAL) resumeOffsetsEndOffset
    //   optArrayOffset_:
    //     ....
    unsigned numOffsets = flags().tryNotesEndIndex;
    MOZ_ASSERT(numOffsets >= flags().scopeNotesEndIndex);
    MOZ_ASSERT(numOffsets >= flags().resumeOffsetsEndIndex);

    return optArrayOffset_ - (numOffsets * sizeof(Offset));
  }
  size_t resumeOffsetsOffset() const { return optArrayOffset_; }
  size_t scopeNotesOffset() const {
    return getOptionalOffset(flags().resumeOffsetsEndIndex);
  }
  size_t tryNotesOffset() const {
    return getOptionalOffset(flags().scopeNotesEndIndex);
  }
  size_t endOffset() const {
    return getOptionalOffset(flags().tryNotesEndIndex);
  }

  // Size to allocate
  static size_t AllocationSize(uint32_t codeLength, uint32_t noteLength,
                               uint32_t numResumeOffsets,
                               uint32_t numScopeNotes, uint32_t numTryNotes);

  // Translate an offset into a concrete pointer.
  template <typename T>
  T* offsetToPointer(size_t offset) {
    uintptr_t base = reinterpret_cast<uintptr_t>(this);
    return reinterpret_cast<T*>(base + offset);
  }

  template <typename T>
  void initElements(size_t offset, size_t length);

  void initOptionalArrays(size_t* cursor, Flags* flags,
                          uint32_t numResumeOffsets, uint32_t numScopeNotes,
                          uint32_t numTryNotes);

  // Initialize to GC-safe state
  ImmutableScriptData(uint32_t codeLength, uint32_t noteLength,
                      uint32_t numResumeOffsets, uint32_t numScopeNotes,
                      uint32_t numTryNotes);

  void setOptionalOffset(int index, Offset offset) {
    MOZ_ASSERT(index > 0);
    MOZ_ASSERT(offset != optArrayOffset_, "Do not store implicit offset");
    offsetToPointer<Offset>(optArrayOffset_)[-index] = offset;
  }
  Offset getOptionalOffset(int index) const {
    // The index 0 represents (implicitly) the offset 'optArrayOffset_'.
    if (index == 0) {
      return optArrayOffset_;
    }

    ImmutableScriptData* this_ = const_cast<ImmutableScriptData*>(this);
    return this_->offsetToPointer<Offset>(optArrayOffset_)[-index];
  }

 public:
  static js::UniquePtr<ImmutableScriptData> new_(
      JSContext* cx, uint32_t mainOffset, uint32_t nfixed, uint32_t nslots,
      uint32_t bodyScopeIndex, uint32_t numICEntries,
      uint32_t numBytecodeTypeSets, bool isFunction, uint16_t funLength,
      mozilla::Span<const jsbytecode> code,
      mozilla::Span<const jssrcnote> notes,
      mozilla::Span<const uint32_t> resumeOffsets,
      mozilla::Span<const ScopeNote> scopeNotes,
      mozilla::Span<const TryNote> tryNotes);

  static js::UniquePtr<ImmutableScriptData> new_(
      JSContext* cx, uint32_t codeLength, uint32_t noteLength,
      uint32_t numResumeOffsets, uint32_t numScopeNotes, uint32_t numTryNotes);

  // The code() and note() arrays together maintain an target alignment by
  // padding the source notes with null. This allows arrays with stricter
  // alignment requirements to follow them.
  static constexpr size_t CodeNoteAlign = sizeof(uint32_t);

  // Compute number of null notes to pad out source notes with.
  static uint32_t ComputeNotePadding(uint32_t codeLength, uint32_t noteLength) {
    uint32_t flagLength = sizeof(Flags);
    uint32_t nullLength =
        CodeNoteAlign - (flagLength + codeLength + noteLength) % CodeNoteAlign;

    // The source notes must have at least one null-terminator.
    MOZ_ASSERT(nullLength >= 1);

    return nullLength;
  }

  // Span over all raw bytes in this struct and its trailing arrays.
  mozilla::Span<const uint8_t> immutableData() const {
    size_t allocSize = endOffset();
    return mozilla::MakeSpan(reinterpret_cast<const uint8_t*>(this), allocSize);
  }

  Flags& flagsRef() { return *offsetToPointer<Flags>(flagOffset()); }
  const Flags& flags() const {
    return const_cast<ImmutableScriptData*>(this)->flagsRef();
  }

  uint32_t codeLength() const { return codeLength_; }
  jsbytecode* code() { return offsetToPointer<jsbytecode>(codeOffset()); }
  mozilla::Span<jsbytecode> codeSpan() { return {code(), codeLength()}; }

  uint32_t noteLength() const { return optionalOffsetsOffset() - noteOffset(); }
  jssrcnote* notes() { return offsetToPointer<jssrcnote>(noteOffset()); }
  mozilla::Span<jssrcnote> notesSpan() { return {notes(), noteLength()}; }

  mozilla::Span<uint32_t> resumeOffsets() {
    return mozilla::MakeSpan(offsetToPointer<uint32_t>(resumeOffsetsOffset()),
                             offsetToPointer<uint32_t>(scopeNotesOffset()));
  }
  mozilla::Span<ScopeNote> scopeNotes() {
    return mozilla::MakeSpan(offsetToPointer<ScopeNote>(scopeNotesOffset()),
                             offsetToPointer<ScopeNote>(tryNotesOffset()));
  }
  mozilla::Span<TryNote> tryNotes() {
    return mozilla::MakeSpan(offsetToPointer<TryNote>(tryNotesOffset()),
                             offsetToPointer<TryNote>(endOffset()));
  }

  static constexpr size_t offsetOfCode() {
    return sizeof(ImmutableScriptData) + sizeof(Flags);
  }
  static constexpr size_t offsetOfResumeOffsetsOffset() {
    // Resume-offsets are the first optional array if they exist. Locate the
    // array with the 'optArrayOffset_' field.
    return offsetof(ImmutableScriptData, optArrayOffset_);
  }
  static constexpr size_t offsetOfNfixed() {
    return offsetof(ImmutableScriptData, nfixed);
  }
  static constexpr size_t offsetOfNslots() {
    return offsetof(ImmutableScriptData, nslots);
  }
  static constexpr size_t offsetOfFunLength() {
    return offsetof(ImmutableScriptData, funLength);
  }

  template <XDRMode mode>
  static MOZ_MUST_USE XDRResult XDR(js::XDRState<mode>* xdr,
                                    js::UniquePtr<ImmutableScriptData>& script);

  // ImmutableScriptData has trailing data so isn't copyable or movable.
  ImmutableScriptData(const ImmutableScriptData&) = delete;
  ImmutableScriptData& operator=(const ImmutableScriptData&) = delete;
};

}  // namespace js

#endif /* vm_SharedStencil_h */
