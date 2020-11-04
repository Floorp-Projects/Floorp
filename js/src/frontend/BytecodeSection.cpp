/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "frontend/BytecodeSection.h"

#include "mozilla/Assertions.h"       // MOZ_ASSERT
#include "mozilla/ReverseIterator.h"  // mozilla::Reversed

#include "frontend/AbstractScopePtr.h"  // ScopeIndex
#include "frontend/CompilationInfo.h"
#include "frontend/SharedContext.h"  // FunctionBox
#include "vm/BytecodeUtil.h"         // INDEX_LIMIT, StackUses, StackDefs
#include "vm/GlobalObject.h"
#include "vm/JSContext.h"     // JSContext
#include "vm/RegExpObject.h"  // RegexpObject
#include "vm/Scope.h"         // GlobalScope

using namespace js;
using namespace js::frontend;

bool GCThingList::append(FunctionBox* funbox, GCThingIndex* index) {
  // Append the function to the vector and return the index in *index.
  *index = GCThingIndex(vector.length());

  if (!vector.append(mozilla::AsVariant(funbox->index()))) {
    return false;
  }
  return true;
}

AbstractScopePtr GCThingList::getScope(size_t index) const {
  const ScriptThingVariant& elem = vector[index];
  if (elem.is<EmptyGlobalScopeType>()) {
    // The empty enclosing scope should be stored by
    // CompilationInput::initForSelfHostingGlobal.
    MOZ_ASSERT(compilationInfo.input.enclosingScope);
    MOZ_ASSERT(
        !compilationInfo.input.enclosingScope->as<GlobalScope>().hasBindings());
    return AbstractScopePtr(compilationInfo.input.enclosingScope);
  }
  return AbstractScopePtr(compilationInfo, elem.as<ScopeIndex>());
}

mozilla::Maybe<ScopeIndex> GCThingList::getScopeIndex(size_t index) const {
  const ScriptThingVariant& elem = vector[index];
  if (elem.is<EmptyGlobalScopeType>()) {
    return mozilla::Nothing();
  }
  return mozilla::Some(vector[index].as<ScopeIndex>());
}

bool js::frontend::EmitScriptThingsVector(
    JSContext* cx, CompilationInfo& compilationInfo,
    CompilationGCOutput& gcOutput,
    mozilla::Span<const ScriptThingVariant> things,
    mozilla::Span<JS::GCCellPtr> output) {
  MOZ_ASSERT(things.size() <= INDEX_LIMIT);
  MOZ_ASSERT(things.size() == output.size());

  struct Matcher {
    JSContext* cx;
    CompilationAtomCache& atomCache;
    CompilationStencil& stencil;
    CompilationGCOutput& gcOutput;
    uint32_t i;
    mozilla::Span<JS::GCCellPtr>& output;

    bool operator()(const ScriptAtom& data) {
      JSAtom* atom = data->toExistingJSAtom(cx, atomCache);
      MOZ_ASSERT(atom);
      output[i] = JS::GCCellPtr(atom);
      return true;
    }

    bool operator()(const NullScriptThing& data) {
      output[i] = JS::GCCellPtr(nullptr);
      return true;
    }

    bool operator()(const BigIntIndex& index) {
      BigIntStencil& data = stencil.bigIntData[index];
      BigInt* bi = data.createBigInt(cx);
      if (!bi) {
        return false;
      }
      output[i] = JS::GCCellPtr(bi);
      return true;
    }

    bool operator()(const RegExpIndex& rindex) {
      RegExpStencil& data = stencil.regExpData[rindex];
      RegExpObject* regexp = data.createRegExp(cx);
      if (!regexp) {
        return false;
      }
      output[i] = JS::GCCellPtr(regexp);
      return true;
    }

    bool operator()(const ObjLiteralIndex& index) {
      ObjLiteralStencil& data = stencil.objLiteralData[index];
      JSObject* obj = data.create(cx, atomCache);
      if (!obj) {
        return false;
      }
      output[i] = JS::GCCellPtr(obj);
      return true;
    }

    bool operator()(const ScopeIndex& index) {
      output[i] = JS::GCCellPtr(gcOutput.scopes[index]);
      return true;
    }

    bool operator()(const FunctionIndex& index) {
      output[i] = JS::GCCellPtr(gcOutput.functions[index]);
      return true;
    }

    bool operator()(const EmptyGlobalScopeType& emptyGlobalScope) {
      Scope* scope = &cx->global()->emptyGlobalScope();
      output[i] = JS::GCCellPtr(scope);
      return true;
    }
  };

  for (uint32_t i = 0; i < things.size(); i++) {
    Matcher m{cx,
              compilationInfo.input.atomCache,
              compilationInfo.stencil,
              gcOutput,
              i,
              output};
    if (!things[i].match(m)) {
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

bool CGScopeNoteList::append(GCThingIndex scopeIndex, BytecodeOffset offset,
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

JSObject* ObjLiteralStencil::create(JSContext* cx,
                                    CompilationAtomCache& atomCache) const {
  return InterpretObjLiteral(cx, atomCache, atoms_, writer_);
}

BytecodeSection::BytecodeSection(JSContext* cx, uint32_t lineNum,
                                 uint32_t column)
    : code_(cx),
      notes_(cx),
      lastNoteOffset_(0),
      tryNoteList_(cx),
      scopeNoteList_(cx),
      resumeOffsetList_(cx),
      currentLine_(lineNum),
      lastColumn_(column) {}

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
