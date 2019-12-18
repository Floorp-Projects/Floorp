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

  bool create(JSContext* cx, FunctionBox* funbox,
              HandleScriptSourceObject sourceObject);
};

// Metadata that can be used to allocate a JSFunction object.
//
// Keeping metadata separate allows the parser to generate
// metadata without requiring immediate access to the garbage
// collector.
struct FunctionCreationData {
  FunctionCreationData(HandleAtom atom, FunctionSyntaxKind kind,
                       GeneratorKind generatorKind, FunctionAsyncKind asyncKind,
                       bool isSelfHosting = false, bool inFunctionBox = false);

  // Custom Copy constructor to ensure we never copy a FunctionCreationData
  // that has a LazyScriptData.
  //
  // We want to be able to copy FunctionCreationData into a Functionbox as part
  // of our attempts to syntax parse an inner function, however, because
  // trySyntaxParseInnerFunction is fallible, for example, if a new directive
  // like "use asmjs" is encountered, we don't want to -move- it into the
  // FunctionBox, because we may need it again if the syntax parse fails.
  //
  // To ensure that we never lose a lazyScriptData however, we guarantee that
  // when this copy constructor is run, it doesn't have any lazyScriptData.
  FunctionCreationData(const FunctionCreationData& data)
      : atom(data.atom),
        kind(data.kind),
        generatorKind(data.generatorKind),
        asyncKind(data.asyncKind),
        allocKind(data.allocKind),
        flags(data.flags),
        isSelfHosting(data.isSelfHosting),
        lazyScriptData(mozilla::Nothing()) {
    MOZ_RELEASE_ASSERT(!data.lazyScriptData);
  }

  FunctionCreationData(FunctionCreationData&& data) = default;

  // The Parser uses KeepAtoms to prevent GC from collecting atoms
  JSAtom* atom = nullptr;
  FunctionSyntaxKind kind = FunctionSyntaxKind::Expression;
  GeneratorKind generatorKind = GeneratorKind::NotGenerator;
  FunctionAsyncKind asyncKind = FunctionAsyncKind::SyncFunction;

  gc::AllocKind allocKind = gc::AllocKind::FUNCTION;
  FunctionFlags flags = {};

  bool isSelfHosting = false;

  mozilla::Maybe<LazyScriptCreationData> lazyScriptData;

  HandleAtom getAtom(JSContext* cx) const;

  void trace(JSTracer* trc) {
    TraceNullableRoot(trc, &atom, "FunctionCreationData atom");
  }
};

} /* namespace frontend */
} /* namespace js */

#endif /* frontend_FunctionCreationData_h */