/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "frontend/BinASTParserBase.h"

#include "vm/JSContext-inl.h"

namespace js::frontend {

BinASTParserBase::BinASTParserBase(JSContext* cx,
                                   CompilationInfo& compilationInfo,
                                   HandleScriptSourceObject sourceObject)
    : ParserSharedBase(cx, compilationInfo, sourceObject,
                       ParserSharedBase::Kind::BinASTParser) {}

ObjectBox* BinASTParserBase::newObjectBox(JSObject* obj) {
  MOZ_ASSERT(obj);

  /*
   * We use JSContext.tempLifoAlloc to allocate parsed objects and place them
   * on a list in this Parser to ensure GC safety. Thus the tempLifoAlloc
   * arenas containing the entries must be alive until we are done with
   * scanning, parsing and code generation for the whole script or top-level
   * function.
   */

  auto* box = alloc_.new_<ObjectBox>(obj, traceListHead_);
  if (!box) {
    ReportOutOfMemory(cx_);
    return nullptr;
  }

  traceListHead_ = box;

  return box;
}

}  // namespace js::frontend
