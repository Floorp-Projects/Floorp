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