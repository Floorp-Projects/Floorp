/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "frontend/BytecodeSection.h"

#include "mozilla/Assertions.h"       // MOZ_ASSERT
#include "mozilla/ReverseIterator.h"  // mozilla::Reversed

#include "frontend/CompilationInfo.h"
#include "frontend/SharedContext.h"  // FunctionBox
#include "frontend/Stencil.h"        // ScopeCreationData
#include "vm/BytecodeUtil.h"         // INDEX_LIMIT, StackUses, StackDefs
#include "vm/GlobalObject.h"
#include "vm/JSContext.h"     // JSContext
#include "vm/RegExpObject.h"  // RegexpObject

using namespace js;
using namespace js::frontend;

bool GCThingList::append(FunctionBox* funbox, uint32_t* index) {
  // Append the function to the vector and return the index in *index. Also add
  // the FunctionBox to the |lastbox| linked list for finishInnerFunctions
  // below.

  MOZ_ASSERT(!funbox->emitLink_);
  funbox->emitLink_ = lastbox;
  lastbox = funbox;

  *index = vector.length();
  // To avoid circular include issues, funbox can't return a FunctionIndex, so
  // instead it returns a size_t, which we wrap in FunctionIndex here to
  // disambiguate the variant.
  return vector.append(mozilla::AsVariant(FunctionIndex(funbox->index())));
}

void GCThingList::finishInnerFunctions() {
  FunctionBox* funbox = lastbox;
  while (funbox) {
    funbox->finish();
    funbox = funbox->emitLink_;
  }
}

AbstractScopePtr GCThingList::getScope(size_t index) const {
  const ScriptThingVariant& elem = vector[index];
  if (elem.is<EmptyGlobalScopeType>()) {
    return AbstractScopePtr(&compilationInfo.cx->global()->emptyGlobalScope());
  }
  return AbstractScopePtr(compilationInfo, elem.as<ScopeIndex>());
}

bool js::frontend::EmitScriptThingsVector(JSContext* cx,
                                          CompilationInfo& compilationInfo,
                                          const ScriptThingsVector& objects,
                                          mozilla::Span<JS::GCCellPtr> output) {
  MOZ_ASSERT(objects.length() <= INDEX_LIMIT);
  MOZ_ASSERT(objects.length() == output.size());

  struct Matcher {
    JSContext* cx;
    CompilationInfo& compilationInfo;
    uint32_t i;
    mozilla::Span<JS::GCCellPtr>& output;

    bool operator()(const BigIntIndex& index) {
      BigIntCreationData& data = compilationInfo.bigIntData[index];
      BigInt* bi = data.createBigInt(cx);
      if (!bi) {
        return false;
      }
      output[i] = JS::GCCellPtr(bi);
      return true;
    }

    bool operator()(const RegExpIndex& rindex) {
      RegExpCreationData& data = compilationInfo.regExpData[rindex];
      RegExpObject* regexp = data.createRegExp(cx);
      if (!regexp) {
        return false;
      }
      output[i] = JS::GCCellPtr(regexp);
      return true;
    }

    bool operator()(const ObjLiteralCreationData& data) {
      JSObject* obj = data.create(cx);
      if (!obj) {
        return false;
      }
      output[i] = JS::GCCellPtr(obj);
      return true;
    }

    bool operator()(const ScopeIndex& index) {
      MutableHandle<ScopeCreationData> data =
          compilationInfo.scopeCreationData[index];
      Scope* scope = data.get().createScope(cx);
      if (!scope) {
        return false;
      }

      output[i] = JS::GCCellPtr(scope);
      return true;
    }

    bool operator()(const FunctionIndex& index) {
      MutableHandle<FunctionType> data = compilationInfo.funcData[index];
      // We should have already converted this data to a JSFunction as part
      // of publishDeferredFunctions, which currently happens before BCE begins.
      // Once we can do LazyScriptCreationData::create without referencing the
      // functionbox, then we should be able to do JSFunction allocation here.
      MOZ_ASSERT(!data.is<FunctionCreationData>());
      output[i] = JS::GCCellPtr(data.as<JSFunction*>());
      return true;
    }

    bool operator()(const EmptyGlobalScopeType& emptyGlobalScope) {
      Scope* scope = &cx->global()->emptyGlobalScope();
      output[i] = JS::GCCellPtr(scope);
      return true;
    }
  };

  for (uint32_t i = 0; i < objects.length(); i++) {
    Matcher m{cx, compilationInfo, i, output};
    if (!objects[i].match(m)) {
      return false;
    }
  }
  return true;
}

bool CGTryNoteList::append(TryNoteKind kind, uint32_t stackDepth,
                           BytecodeOffset start, BytecodeOffset end) {
  MOZ_ASSERT(start <= end);

  // Offsets are given relative to sections, but we only expect main-section
  // to have TryNotes. In finish() we will fixup base offset.

  TryNote note(uint32_t(kind), stackDepth, start.toUint32(),
               (end - start).toUint32());

  return list.append(note);
}

bool CGScopeNoteList::append(uint32_t scopeIndex, BytecodeOffset offset,
                             uint32_t parent) {
  ScopeNote note;
  note.index = scopeIndex;
  note.start = offset.toUint32();
  note.length = 0;
  note.parent = parent;

  return list.append(note);
}

void CGScopeNoteList::recordEnd(uint32_t index, BytecodeOffset offset) {
  recordEndImpl(index, offset.toUint32());
}

void CGScopeNoteList::recordEndFunctionBodyVar(uint32_t index) {
  recordEndImpl(index, UINT32_MAX);
}

void CGScopeNoteList::recordEndImpl(uint32_t index, uint32_t offset) {
  MOZ_ASSERT(index < length());
  MOZ_ASSERT(list[index].length == 0);
  MOZ_ASSERT(offset >= list[index].start);
  list[index].length = offset - list[index].start;
}

JSObject* ObjLiteralCreationData::create(JSContext* cx) const {
  return InterpretObjLiteral(cx, atoms_, writer_);
}

BytecodeSection::BytecodeSection(JSContext* cx, uint32_t lineNum)
    : code_(cx),
      notes_(cx),
      lastNoteOffset_(0),
      tryNoteList_(cx),
      scopeNoteList_(cx),
      resumeOffsetList_(cx),
      currentLine_(lineNum) {}

void BytecodeSection::updateDepth(BytecodeOffset target) {
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

PerScriptData::PerScriptData(JSContext* cx,
                             frontend::CompilationInfo& compilationInfo)
    : gcThingList_(cx, compilationInfo),
      atomIndices_(cx->frontendCollectionPool()) {}

bool PerScriptData::init(JSContext* cx) { return atomIndices_.acquire(cx); }
