/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef frontend_BytecodeSection_h
#define frontend_BytecodeSection_h

#include "mozilla/Attributes.h"  // MOZ_MUST_USE, MOZ_STACK_CLASS
#include "mozilla/Maybe.h"       // mozilla::Maybe
#include "mozilla/Span.h"        // mozilla::Span

#include <stddef.h>  // ptrdiff_t, size_t
#include <stdint.h>  // uint16_t, int32_t, uint32_t

#include "jstypes.h"           // JS_PUBLIC_API
#include "NamespaceImports.h"  // ValueVector

#include "frontend/AbstractScopePtr.h"  // AbstractScopePtr
#include "frontend/BytecodeOffset.h"    // BytecodeOffset
#include "frontend/CompilationInfo.h"   // CompilationInfo
#include "frontend/JumpList.h"          // JumpTarget
#include "frontend/NameCollections.h"   // AtomIndexMap, PooledMapPtr
#include "frontend/ObjLiteral.h"        // ObjLiteralCreationData
#include "frontend/ParseNode.h"         // BigIntLiteral
#include "frontend/SourceNotes.h"       // SrcNote
#include "frontend/Stencil.h"           // Stencils
#include "gc/Barrier.h"                 // GCPtrObject, GCPtrScope, GCPtrValue
#include "gc/Rooting.h"                 // JS::Rooted
#include "js/GCVariant.h"               // GCPolicy<mozilla::Variant>
#include "js/GCVector.h"                // GCVector
#include "js/TypeDecls.h"               // jsbytecode, JSContext
#include "js/Value.h"                   // JS::Vector
#include "js/Vector.h"                  // Vector
#include "vm/Opcodes.h"                 // JSOpLength_JumpTarget
#include "vm/SharedStencil.h"           // TryNote, ScopeNote
#include "vm/StencilEnums.h"            // TryNoteKind

namespace js {

class Scope;

namespace frontend {

class FunctionBox;

struct MOZ_STACK_CLASS GCThingList {
  CompilationInfo& compilationInfo;
  ScriptThingsVector vector;

  // Last emitted function.
  FunctionBox* lastbox = nullptr;

  // Index of the first scope in the vector.
  mozilla::Maybe<uint32_t> firstScopeIndex;

  explicit GCThingList(JSContext* cx, CompilationInfo& compilationInfo)
      : compilationInfo(compilationInfo), vector(cx) {}

  MOZ_MUST_USE bool append(ScopeIndex scope, uint32_t* index) {
    *index = vector.length();
    if (!vector.append(mozilla::AsVariant(scope))) {
      return false;
    }
    if (!firstScopeIndex) {
      firstScopeIndex.emplace(*index);
    }
    return true;
  }
  MOZ_MUST_USE bool append(BigIntLiteral* literal, uint32_t* index) {
    *index = vector.length();
    return vector.append(mozilla::AsVariant(literal->index()));
  }
  MOZ_MUST_USE bool append(RegExpLiteral* literal, uint32_t* index) {
    *index = vector.length();
    return vector.append(mozilla::AsVariant(literal->index()));
  }
  MOZ_MUST_USE bool append(ObjLiteralCreationData&& objlit, uint32_t* index) {
    *index = vector.length();
    return vector.append(mozilla::AsVariant(std::move(objlit)));
  }
  MOZ_MUST_USE bool append(FunctionBox* funbox, uint32_t* index);

  MOZ_MUST_USE bool appendEmptyGlobalScope(uint32_t* index) {
    *index = vector.length();
    EmptyGlobalScopeType emptyGlobalScope;
    if (!vector.append(mozilla::AsVariant(emptyGlobalScope))) {
      return false;
    }
    if (!firstScopeIndex) {
      firstScopeIndex.emplace(*index);
    }
    return true;
  }

  uint32_t length() const { return vector.length(); }

  const ScriptThingsVector& objects() { return vector; }

  void finishInnerFunctions();

  AbstractScopePtr getScope(size_t index) const;

  AbstractScopePtr firstScope() const {
    MOZ_ASSERT(firstScopeIndex.isSome());
    return getScope(*firstScopeIndex);
  }

  ScriptThingsVector stealGCThings() { return std::move(vector); }
};

MOZ_MUST_USE bool EmitScriptThingsVector(JSContext* cx,
                                         CompilationInfo& compilationInfo,
                                         const ScriptThingsVector& objects,
                                         mozilla::Span<JS::GCCellPtr> output);

struct CGTryNoteList {
  Vector<TryNote> list;
  explicit CGTryNoteList(JSContext* cx) : list(cx) {}

  MOZ_MUST_USE bool append(TryNoteKind kind, uint32_t stackDepth,
                           BytecodeOffset start, BytecodeOffset end);
  mozilla::Span<const TryNote> span() const {
    return {list.begin(), list.length()};
  }
  size_t length() const { return list.length(); }
};

struct CGScopeNoteList {
  Vector<ScopeNote> list;
  explicit CGScopeNoteList(JSContext* cx) : list(cx) {}

  MOZ_MUST_USE bool append(uint32_t scopeIndex, BytecodeOffset offset,
                           uint32_t parent);
  void recordEnd(uint32_t index, BytecodeOffset offset);
  void recordEndFunctionBodyVar(uint32_t index);
  mozilla::Span<const ScopeNote> span() const {
    return {list.begin(), list.length()};
  }
  size_t length() const { return list.length(); }

 private:
  void recordEndImpl(uint32_t index, uint32_t offset);
};

struct CGResumeOffsetList {
  Vector<uint32_t> list;
  explicit CGResumeOffsetList(JSContext* cx) : list(cx) {}

  MOZ_MUST_USE bool append(uint32_t offset) { return list.append(offset); }
  mozilla::Span<const uint32_t> span() const {
    return {list.begin(), list.length()};
  }
  size_t length() const { return list.length(); }
};

static constexpr size_t MaxBytecodeLength = INT32_MAX;
static constexpr size_t MaxSrcNotesLength = INT32_MAX;

// Have a few inline elements, so as to avoid heap allocation for tiny
// sequences.  See bug 1390526.
typedef Vector<jsbytecode, 64> BytecodeVector;
typedef Vector<js::SrcNote, 64> SrcNotesVector;

// Bytecode and all data directly associated with specific opcode/index inside
// bytecode is stored in this class.
class BytecodeSection {
 public:
  BytecodeSection(JSContext* cx, uint32_t lineNum);

  // ---- Bytecode ----

  BytecodeVector& code() { return code_; }
  const BytecodeVector& code() const { return code_; }

  jsbytecode* code(BytecodeOffset offset) {
    return code_.begin() + offset.value();
  }
  BytecodeOffset offset() const {
    return BytecodeOffset(code_.end() - code_.begin());
  }

  // ---- Source notes ----

  SrcNotesVector& notes() { return notes_; }
  const SrcNotesVector& notes() const { return notes_; }

  BytecodeOffset lastNoteOffset() const { return lastNoteOffset_; }
  void setLastNoteOffset(BytecodeOffset offset) { lastNoteOffset_ = offset; }

  // ---- Jump ----

  BytecodeOffset lastTargetOffset() const { return lastTarget_.offset; }
  void setLastTargetOffset(BytecodeOffset offset) {
    lastTarget_.offset = offset;
  }

  // Check if the last emitted opcode is a jump target.
  bool lastOpcodeIsJumpTarget() const {
    return lastTarget_.offset.valid() &&
           offset() - lastTarget_.offset ==
               BytecodeOffsetDiff(JSOpLength_JumpTarget);
  }

  // JumpTarget should not be part of the emitted statement, as they can be
  // aliased by multiple statements. If we included the jump target as part of
  // the statement we might have issues where the enclosing statement might
  // not contain all the opcodes of the enclosed statements.
  BytecodeOffset lastNonJumpTargetOffset() const {
    return lastOpcodeIsJumpTarget() ? lastTarget_.offset : offset();
  }

  // ---- Stack ----

  int32_t stackDepth() const { return stackDepth_; }
  void setStackDepth(int32_t depth) { stackDepth_ = depth; }

  uint32_t maxStackDepth() const { return maxStackDepth_; }

  void updateDepth(BytecodeOffset target);

  // ---- Try notes ----

  CGTryNoteList& tryNoteList() { return tryNoteList_; };
  const CGTryNoteList& tryNoteList() const { return tryNoteList_; };

  // ---- Scope ----

  CGScopeNoteList& scopeNoteList() { return scopeNoteList_; };
  const CGScopeNoteList& scopeNoteList() const { return scopeNoteList_; };

  // ---- Generator ----

  CGResumeOffsetList& resumeOffsetList() { return resumeOffsetList_; }
  const CGResumeOffsetList& resumeOffsetList() const {
    return resumeOffsetList_;
  }

  uint32_t numYields() const { return numYields_; }
  void addNumYields() { numYields_++; }

  // ---- Line and column ----

  uint32_t currentLine() const { return currentLine_; }
  uint32_t lastColumn() const { return lastColumn_; }
  void setCurrentLine(uint32_t line, uint32_t sourceOffset) {
    currentLine_ = line;
    lastColumn_ = 0;
    lastSourceOffset_ = sourceOffset;
  }

  void setLastColumn(uint32_t column, uint32_t offset) {
    lastColumn_ = column;
    lastSourceOffset_ = offset;
  }

  void updateSeparatorPosition() {
    lastSeparatorCodeOffset_ = code().length();
    lastSeparatorSourceOffset_ = lastSourceOffset_;
    lastSeparatorLine_ = currentLine_;
    lastSeparatorColumn_ = lastColumn_;
  }

  void updateSeparatorPositionIfPresent() {
    if (lastSeparatorCodeOffset_ == code().length()) {
      lastSeparatorSourceOffset_ = lastSourceOffset_;
      lastSeparatorLine_ = currentLine_;
      lastSeparatorColumn_ = lastColumn_;
    }
  }

  bool isDuplicateLocation() const {
    return lastSeparatorLine_ == currentLine_ &&
           lastSeparatorColumn_ == lastColumn_;
  }

  bool atSeparator(uint32_t offset) const {
    return lastSeparatorSourceOffset_ == offset;
  }

  // ---- JIT ----

  uint32_t numICEntries() const { return numICEntries_; }
  void incrementNumICEntries() {
    MOZ_ASSERT(numICEntries_ != UINT32_MAX, "Shouldn't overflow");
    numICEntries_++;
  }
  void setNumICEntries(uint32_t entries) { numICEntries_ = entries; }

  uint32_t numTypeSets() const { return numTypeSets_; }
  void incrementNumTypeSets() {
    MOZ_ASSERT(numTypeSets_ != UINT32_MAX, "Shouldn't overflow");
    numTypeSets_++;
  }

 private:
  // ---- Bytecode ----

  // Bytecode.
  BytecodeVector code_;

  // ---- Source notes ----

  // Source notes
  SrcNotesVector notes_;

  // Code offset for last source note
  BytecodeOffset lastNoteOffset_;

  // ---- Jump ----

  // Last jump target emitted.
  JumpTarget lastTarget_;

  // ---- Stack ----

  // Maximum number of expression stack slots so far.
  uint32_t maxStackDepth_ = 0;

  // Current stack depth in script frame.
  int32_t stackDepth_ = 0;

  // ---- Try notes ----

  // List of emitted try notes.
  CGTryNoteList tryNoteList_;

  // ---- Scope ----

  // List of emitted block scope notes.
  CGScopeNoteList scopeNoteList_;

  // ---- Generator ----

  // Certain ops (yield, await, gosub) have an entry in the script's
  // resumeOffsets list. This can be used to map from the op's resumeIndex to
  // the bytecode offset of the next pc. This indirection makes it easy to
  // resume in the JIT (because BaselineScript stores a resumeIndex => native
  // code array).
  CGResumeOffsetList resumeOffsetList_;

  // Number of yield instructions emitted. Does not include JSOp::Await.
  uint32_t numYields_ = 0;

  // ---- Line and column ----

  // Line number for srcnotes.
  //
  // WARNING: If this becomes out of sync with already-emitted srcnotes,
  // we can get undefined behavior.
  uint32_t currentLine_;

  // Zero-based column index on currentLine_ of last
  // SrcNoteType::ColSpan-annotated opcode.
  //
  // WARNING: If this becomes out of sync with already-emitted srcnotes,
  // we can get undefined behavior.
  uint32_t lastColumn_ = 0;

  // The last code unit used for srcnotes.
  uint32_t lastSourceOffset_ = 0;

  // The offset, line and column numbers of the last opcode for the
  // breakpoint for step execution.
  uint32_t lastSeparatorCodeOffset_ = 0;
  uint32_t lastSeparatorSourceOffset_ = 0;
  uint32_t lastSeparatorLine_ = 0;
  uint32_t lastSeparatorColumn_ = 0;

  // ---- JIT ----

  // Number of ICEntries in the script. There's one ICEntry for each JOF_IC op
  // and, if the script is a function, for |this| and each formal argument.
  uint32_t numICEntries_ = 0;

  // Number of JOF_TYPESET opcodes generated.
  uint32_t numTypeSets_ = 0;
};

// Data that is not directly associated with specific opcode/index inside
// bytecode, but referred from bytecode is stored in this class.
class PerScriptData {
 public:
  explicit PerScriptData(JSContext* cx,
                         frontend::CompilationInfo& compilationInfo);

  MOZ_MUST_USE bool init(JSContext* cx);

  GCThingList& gcThingList() { return gcThingList_; }
  const GCThingList& gcThingList() const { return gcThingList_; }

  PooledMapPtr<AtomIndexMap>& atomIndices() { return atomIndices_; }
  const PooledMapPtr<AtomIndexMap>& atomIndices() const { return atomIndices_; }

 private:
  // List of emitted scopes/objects/bigints.
  GCThingList gcThingList_;

  // Map from atom to index.
  PooledMapPtr<AtomIndexMap> atomIndices_;
};

} /* namespace frontend */
} /* namespace js */

#endif /* frontend_BytecodeSection_h */
