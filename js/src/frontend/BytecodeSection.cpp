/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "frontend/BytecodeSection.h"

#include "mozilla/Assertions.h"       // MOZ_ASSERT
#include "mozilla/PodOperations.h"    // PodZero
#include "mozilla/ReverseIterator.h"  // mozilla::Reversed

#include "frontend/ParseNode.h"      // ObjectBox
#include "frontend/SharedContext.h"  // FunctionBox
#include "vm/BytecodeUtil.h"         // INDEX_LIMIT, StackUses, StackDefs
#include "vm/JSContext.h"            // JSContext

using namespace js;
using namespace js::frontend;

void CGNumberList::finish(mozilla::Span<GCPtrValue> array) {
  MOZ_ASSERT(length() == array.size());

  for (unsigned i = 0; i < length(); i++) {
    array[i].init(vector[i]);
  }
}

/*
 * Find the index of the given object for code generator.
 *
 * Since the emitter refers to each parsed object only once, for the index we
 * use the number of already indexed objects. We also add the object to a list
 * to convert the list to a fixed-size array when we complete code generation,
 * see js::CGObjectList::finish below.
 */
unsigned CGObjectList::add(ObjectBox* objbox) {
  MOZ_ASSERT(objbox->isObjectBox());
  MOZ_ASSERT(!objbox->emitLink);
  objbox->emitLink = lastbox;
  lastbox = objbox;
  return length++;
}

void CGObjectList::finish(mozilla::Span<GCPtrObject> array) {
  MOZ_ASSERT(length <= INDEX_LIMIT);
  MOZ_ASSERT(length == array.size());

  ObjectBox* objbox = lastbox;
  for (GCPtrObject& obj : mozilla::Reversed(array)) {
    MOZ_ASSERT(obj == nullptr);
    MOZ_ASSERT(objbox->object()->isTenured());
    obj.init(objbox->object());
    objbox = objbox->emitLink;
  }
}

void CGObjectList::finishInnerFunctions() {
  ObjectBox* objbox = lastbox;
  while (objbox) {
    if (objbox->isFunctionBox()) {
      objbox->asFunctionBox()->finish();
    }
    objbox = objbox->emitLink;
  }
}

void CGScopeList::finish(mozilla::Span<GCPtrScope> array) {
  MOZ_ASSERT(length() <= INDEX_LIMIT);
  MOZ_ASSERT(length() == array.size());

  for (uint32_t i = 0; i < length(); i++) {
    array[i].init(vector[i]);
  }
}

bool CGTryNoteList::append(JSTryNoteKind kind, uint32_t stackDepth,
                           size_t start, size_t end) {
  MOZ_ASSERT(start <= end);
  MOZ_ASSERT(size_t(uint32_t(start)) == start);
  MOZ_ASSERT(size_t(uint32_t(end)) == end);

  // Offsets are given relative to sections, but we only expect main-section
  // to have TryNotes. In finish() we will fixup base offset.

  JSTryNote note;
  note.kind = kind;
  note.stackDepth = stackDepth;
  note.start = uint32_t(start);
  note.length = uint32_t(end - start);

  return list.append(note);
}

void CGTryNoteList::finish(mozilla::Span<JSTryNote> array) {
  MOZ_ASSERT(length() == array.size());

  for (unsigned i = 0; i < length(); i++) {
    array[i] = list[i];
  }
}

bool CGScopeNoteList::append(uint32_t scopeIndex, uint32_t offset,
                             uint32_t parent) {
  CGScopeNote note;
  mozilla::PodZero(&note);

  // Offsets are given relative to sections. In finish() we will fixup base
  // offset if needed.

  note.index = scopeIndex;
  note.start = offset;
  note.parent = parent;

  return list.append(note);
}

void CGScopeNoteList::recordEnd(uint32_t index, uint32_t offset) {
  MOZ_ASSERT(index < length());
  MOZ_ASSERT(list[index].length == 0);
  list[index].end = offset;
}

void CGScopeNoteList::finish(mozilla::Span<ScopeNote> array) {
  MOZ_ASSERT(length() == array.size());

  for (unsigned i = 0; i < length(); i++) {
    MOZ_ASSERT(list[i].end >= list[i].start);
    list[i].length = list[i].end - list[i].start;
    array[i] = list[i];
  }
}

void CGResumeOffsetList::finish(mozilla::Span<uint32_t> array) {
  MOZ_ASSERT(length() == array.size());

  for (unsigned i = 0; i < length(); i++) {
    array[i] = list[i];
  }
}

BytecodeSection::BytecodeSection(JSContext* cx, uint32_t lineNum)
    : code_(cx),
      notes_(cx),
      tryNoteList_(cx),
      scopeNoteList_(cx),
      resumeOffsetList_(cx),
      currentLine_(lineNum) {}

void BytecodeSection::updateDepth(ptrdiff_t target) {
  jsbytecode* pc = code(target);

  int nuses = StackUses(pc);
  int ndefs = StackDefs(pc);

  stackDepth_ -= nuses;
  MOZ_ASSERT(stackDepth_ >= 0);
  stackDepth_ += ndefs;

  if (uint32_t(stackDepth_) > maxStackDepth_) {
    maxStackDepth_ = stackDepth_;
  }
}

PerScriptData::PerScriptData(JSContext* cx)
    : scopeList_(cx),
      numberList_(cx),
      atomIndices_(cx->frontendCollectionPool()) {}

bool PerScriptData::init(JSContext* cx) { return atomIndices_.acquire(cx); }
