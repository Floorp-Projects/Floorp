/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "frontend/BCEScriptStencil.h"

#include "frontend/AbstractScope.h"    // AbstractScope
#include "frontend/BytecodeEmitter.h"  // BytecodeEmitter
#include "frontend/BytecodeSection.h"  // BytecodeSection, PerScriptData

using namespace js;
using namespace js::frontend;

BCEScriptStencil::BCEScriptStencil(BytecodeEmitter& bce, uint32_t nslots)
    : bce_(bce) {
  init(nslots);
}

void BCEScriptStencil::init(uint32_t nslots) {
  lineno = bce_.firstLine;
  column = bce_.firstColumn;

  natoms = bce_.perScriptData().atomIndices()->count();

  ngcthings = bce_.perScriptData().gcThingList().length();

  numResumeOffsets = bce_.bytecodeSection().resumeOffsetList().length();
  numScopeNotes = bce_.bytecodeSection().scopeNoteList().length();
  numTryNotes = bce_.bytecodeSection().tryNoteList().length();

  mainOffset = bce_.mainOffset();
  nfixed = bce_.maxFixedSlots;
  this->nslots = nslots;
  bodyScopeIndex = bce_.bodyScopeIndex;
  numICEntries = bce_.bytecodeSection().numICEntries();
  numBytecodeTypeSets = bce_.bytecodeSection().numTypeSets();

  strict = bce_.sc->strict();
  bindingsAccessedDynamically = bce_.sc->bindingsAccessedDynamically();
  hasCallSiteObj = bce_.sc->hasCallSiteObj();
  isForEval = bce_.sc->isEvalContext();
  isModule = bce_.sc->isModuleContext();
  isFunction = bce_.sc->isFunctionBox();
  hasNonSyntacticScope =
      bce_.outermostScope().hasOnChain(ScopeKind::NonSyntactic);
  needsFunctionEnvironmentObjects = getNeedsFunctionEnvironmentObjects();
  hasModuleGoal = bce_.sc->hasModuleGoal();

  code = bce_.bytecodeSection().code();
  notes = bce_.bytecodeSection().notes();

  if (isFunction) {
    functionBox = bce_.sc->asFunctionBox();
  }
}

bool BCEScriptStencil::getNeedsFunctionEnvironmentObjects() const {
  // See JSFunction::needsCallObject()
  js::AbstractScope bodyScope = bce_.bodyScope();
  if (bodyScope.kind() == js::ScopeKind::Function) {
    if (bodyScope.hasEnvironment()) {
      return true;
    }
  }

  // See JSScript::maybeNamedLambdaScope()
  js::AbstractScope outerScope = bce_.outermostScope();
  if (outerScope.kind() == js::ScopeKind::NamedLambda ||
      outerScope.kind() == js::ScopeKind::StrictNamedLambda) {
    MOZ_ASSERT(bce_.sc->asFunctionBox()->isNamedLambda());

    if (outerScope.hasEnvironment()) {
      return true;
    }
  }

  return false;
}

bool BCEScriptStencil::finishGCThings(
    JSContext* cx, mozilla::Span<JS::GCCellPtr> gcthings) const {
  return bce_.perScriptData().gcThingList().finish(cx, bce_.compilationInfo,
                                                   gcthings);
}

void BCEScriptStencil::initAtomMap(GCPtrAtom* atoms) const {
  const AtomIndexMap& indices = *bce_.perScriptData().atomIndices();

  for (AtomIndexMap::Range r = indices.all(); !r.empty(); r.popFront()) {
    JSAtom* atom = r.front().key();
    uint32_t index = r.front().value();
    MOZ_ASSERT(index < indices.count());
    atoms[index].init(atom);
  }
}

void BCEScriptStencil::finishResumeOffsets(
    mozilla::Span<uint32_t> resumeOffsets) const {
  bce_.bytecodeSection().resumeOffsetList().finish(resumeOffsets);
}

void BCEScriptStencil::finishScopeNotes(
    mozilla::Span<ScopeNote> scopeNotes) const {
  bce_.bytecodeSection().scopeNoteList().finish(scopeNotes);
}

void BCEScriptStencil::finishTryNotes(mozilla::Span<JSTryNote> tryNotes) const {
  bce_.bytecodeSection().tryNoteList().finish(tryNotes);
}

void BCEScriptStencil::finishInnerFunctions() const {
  bce_.perScriptData().gcThingList().finishInnerFunctions();
}
