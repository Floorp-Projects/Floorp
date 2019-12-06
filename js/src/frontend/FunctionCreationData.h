/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef frontend_FunctionCreationData_h
#define frontend_FunctionCreationData_h

#include "frontend/ParseNode.h"
#include "gc/AllocKind.h"
#include "gc/Rooting.h"
#include "vm/JSFunction.h"
#include "vm/JSScript.h"

namespace js {
namespace frontend {

// Data used to instantiate the lazy script before script emission.
struct LazyScriptCreationData {
  frontend::AtomVector closedOverBindings;

  // This is traced by the functionbox which owns this LazyScriptCreationData
  FunctionBoxVector innerFunctionBoxes;
  bool strict = false;

  mozilla::Maybe<FieldInitializers> fieldInitializers;

  explicit LazyScriptCreationData(JSContext* cx) : innerFunctionBoxes(cx) {}

  bool init(JSContext* cx, const frontend::AtomVector& COB,
            FunctionBoxVector& innerBoxes, bool isStrict) {
    strict = isStrict;
    // Copy out of the stack allocated vectors.
    if (!innerFunctionBoxes.appendAll(innerBoxes)) {
      return false;
    }

    if (!closedOverBindings.appendAll(COB)) {
      ReportOutOfMemory(cx);  // closedOverBindings uses SystemAllocPolicy.
      return false;
    }
    return true;
  }
};

// Metadata that can be used to allocate a JSFunction object.
//
// Keeping metadata separate allows the parser to generate
// metadata without requiring immediate access to the garbage
// collector.
struct FunctionCreationData {
  // The Parser uses KeepAtoms to prevent GC from collecting atoms
  JSAtom* atom = nullptr;
  FunctionSyntaxKind kind = FunctionSyntaxKind::Expression;
  GeneratorKind generatorKind = GeneratorKind::NotGenerator;
  FunctionAsyncKind asyncKind = FunctionAsyncKind::SyncFunction;

  gc::AllocKind allocKind = gc::AllocKind::FUNCTION;
  FunctionFlags flags = {};

  bool isSelfHosting = false;

  HandleAtom getAtom(JSContext* cx) const;

  void trace(JSTracer* trc) {
    TraceNullableRoot(trc, &atom, "FunctionCreationData atom");
  }
};

} /* namespace frontend */
} /* namespace js */

#endif /* frontend_FunctionCreationData_h */