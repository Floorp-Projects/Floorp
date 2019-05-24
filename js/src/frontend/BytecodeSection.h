/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef frontend_BytecodeSection_h
#define frontend_BytecodeSection_h

#include "mozilla/Attributes.h"  // MOZ_MUST_USE, MOZ_STACK_CLASS
#include "mozilla/Span.h"        // mozilla::Span

#include <stddef.h>  // ptrdiff_t, size_t
#include <stdint.h>  // uint16_t, int32_t, uint32_t

#include "NamespaceImports.h"          // ValueVector
#include "frontend/JumpList.h"         // JumpTarget
#include "frontend/NameCollections.h"  // AtomIndexMap, PooledMapPtr
#include "frontend/SourceNotes.h"      // jssrcnote
#include "gc/Barrier.h"                // GCPtrObject, GCPtrScope, GCPtrValue
#include "gc/Rooting.h"                // JS::Rooted
#include "js/GCVector.h"               // GCVector
#include "js/TypeDecls.h"              // jsbytecode
#include "js/Value.h"                  // JS::Vector
#include "js/Vector.h"                 // Vector
#include "vm/JSScript.h"               // JSTryNote, JSTryNoteKind, ScopeNote
#include "vm/Opcodes.h"                // JSOP_*

struct JSContext;

namespace js {

class Scope;

namespace frontend {

class ObjectBox;

class CGNumberList {
  JS::Rooted<ValueVector> vector;

 public:
  explicit CGNumberList(JSContext* cx) : vector(cx, ValueVector(cx)) {}
  MOZ_MUST_USE bool append(const JS::Value& v) { return vector.append(v); }
  size_t length() const { return vector.length(); }
  void finish(mozilla::Span<GCPtrValue> array);
};

struct CGObjectList {
  // Number of emitted so far objects.
  uint32_t length;
  // Last emitted object.
  ObjectBox* lastbox;

  CGObjectList() : length(0), lastbox(nullptr) {}

  unsigned add(ObjectBox* objbox);
  void finish(mozilla::Span<GCPtrObject> array);
  void finishInnerFunctions();
};

struct MOZ_STACK_CLASS CGScopeList {
  JS::Rooted<GCVector<Scope*>> vector;

  explicit CGScopeList(JSContext* cx) : vector(cx, GCVector<Scope*>(cx)) {}

  bool append(Scope* scope) { return vector.append(scope); }
  uint32_t length() const { return vector.length(); }
  void finish(mozilla::Span<GCPtrScope> array);
};

struct CGTryNoteList {
  Vector<JSTryNote> list;
  explicit CGTryNoteList(JSContext* cx) : list(cx) {}

  MOZ_MUST_USE bool append(JSTryNoteKind kind, uint32_t stackDepth,
                           size_t start, size_t end);
  size_t length() const { return list.length(); }
  void finish(mozilla::Span<JSTryNote> array);
};

struct CGScopeNote : public ScopeNote {
  // The end offset. Used to compute the length.
  uint32_t end;
};

struct CGScopeNoteList {
  Vector<CGScopeNote> list;
  explicit CGScopeNoteList(JSContext* cx) : list(cx) {}

  MOZ_MUST_USE bool append(uint32_t scopeIndex, uint32_t offset,
                           uint32_t parent);
  void recordEnd(uint32_t index, uint32_t offse);
  size_t length() const { return list.length(); }
  void finish(mozilla::Span<ScopeNote> array);
};

struct CGResumeOffsetList {
  Vector<uint32_t> list;
  explicit CGResumeOffsetList(JSContext* cx) : list(cx) {}

  MOZ_MUST_USE bool append(uint32_t offset) { return list.append(offset); }
  size_t length() const { return list.length(); }
  void finish(mozilla::Span<uint32_t> array);
};


static constexpr size_t MaxBytecodeLength = INT32_MAX;
static constexpr size_t MaxSrcNotesLength = INT32_MAX;

// Have a few inline elements, so as to avoid heap allocation for tiny
// sequences.  See bug 1390526.
typedef Vector<jsbytecode, 64> BytecodeVector;
typedef Vector<jssrcnote, 64> SrcNotesVector;

// Bytecode and all data directly associated with specific opcode/index inside
// bytecode is stored in this class.
class BytecodeSection {
 public:
  BytecodeSection(JSContext* cx, uint32_t lineNum);

  // ---- Bytecode ----

  BytecodeVector& code() { return code_; }
  const BytecodeVector& code() const { return code_; }

  jsbytecode* code(ptrdiff_t offset) { return code_.begin() + offset; }
  ptrdiff_t offset() const { return code_.end() - code_.begin(); }

  // ---- Source notes ----

  SrcNotesVector& notes() { return notes_; }
  const SrcNotesVector& notes() const { return notes_; }

  ptrdiff_t lastNoteOffset() const { return lastNoteOffset_; }
  void setLastNoteOffset(ptrdiff_t offset) { lastNoteOffset_ = offset; }

  // ---- Jump ----

  ptrdiff_t lastTargetOffset() const { return lastTarget_.offset; }
  void setLastTargetOffset(ptrdiff_t offset) { lastTarget_.offset = offset; }

  // Check if the last emitted opcode is a jump target.
  bool lastOpcodeIsJumpTarget() const {
    return offset() - lastTarget_.offset == ptrdiff_t(JSOP_JUMPTARGET_LENGTH);
  }

  // JumpTarget should not be part of the emitted statement, as they can be
  // aliased by multiple statements. If we included the jump target as part of
  // the statement we might have issues where the enclosing statement might
  // not contain all the opcodes of the enclosed statements.
  ptrdiff_t lastNonJumpTargetOffset() const {
    return lastOpcodeIsJumpTarget() ? lastTarget_.offset : offset();
  }

  // ---- Stack ----

  int32_t stackDepth() const { return stackDepth_; }
  void setStackDepth(int32_t depth) { stackDepth_ = depth; }

  uint32_t maxStackDepth() const { return maxStackDepth_; }

  void updateDepth(ptrdiff_t target);

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
  void setCurrentLine(uint32_t line) {
    currentLine_ = line;
    lastColumn_ = 0;
  }
  void setLastColumn(uint32_t column) { lastColumn_ = column; }

  void updateSeparatorPosition() {
    lastSeparatorOffet_ = code().length();
    lastSeparatorLine_ = currentLine_;
    lastSeparatorColumn_ = lastColumn_;
  }

  void updateSeparatorPositionIfPresent() {
    if (lastSeparatorOffet_ == code().length()) {
      lastSeparatorLine_ = currentLine_;
      lastSeparatorColumn_ = lastColumn_;
    }
  }

  bool isDuplicateLocation() const {
    return lastSeparatorLine_ == currentLine_ &&
           lastSeparatorColumn_ == lastColumn_;
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
  ptrdiff_t lastNoteOffset_ = 0;

  // ---- Jump ----

  // Last jump target emitted.
  JumpTarget lastTarget_ = {-1 - ptrdiff_t(JSOP_JUMPTARGET_LENGTH)};

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

  // Number of yield instructions emitted. Does not include JSOP_AWAIT.
  uint32_t numYields_ = 0;

  // ---- Line and column ----

  // Line number for srcnotes.
  //
  // WARNING: If this becomes out of sync with already-emitted srcnotes,
  // we can get undefined behavior.
  uint32_t currentLine_;

  // Zero-based column index on currentLine_ of last SRC_COLSPAN-annotated
  // opcode.
  //
  // WARNING: If this becomes out of sync with already-emitted srcnotes,
  // we can get undefined behavior.
  uint32_t lastColumn_ = 0;

  // The offset, line and column numbers of the last opcode for the
  // breakpoint for step execution.
  uint32_t lastSeparatorOffet_ = 0;
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
  explicit PerScriptData(JSContext* cx);

  MOZ_MUST_USE bool init(JSContext* cx);

  // ---- Scope ----

  CGScopeList& scopeList() { return scopeList_; }
  const CGScopeList& scopeList() const { return scopeList_; }

  // ---- Literals ----

  CGNumberList& numberList() { return numberList_; }
  const CGNumberList& numberList() const { return numberList_; }

  CGObjectList& objectList() { return objectList_; }
  const CGObjectList& objectList() const { return objectList_; }

  PooledMapPtr<AtomIndexMap>& atomIndices() { return atomIndices_; }
  const PooledMapPtr<AtomIndexMap>& atomIndices() const { return atomIndices_; }

 private:
  // ---- Scope ----

  // List of emitted scopes.
  CGScopeList scopeList_;

  // ---- Literals ----

  // List of double and bigint values used by script.
  CGNumberList numberList_;

  // List of emitted objects.
  CGObjectList objectList_;

  // Map from atom to index.
  PooledMapPtr<AtomIndexMap> atomIndices_;
};

} /* namespace frontend */
} /* namespace js */

#endif /* frontend_BytecodeSection_h */
