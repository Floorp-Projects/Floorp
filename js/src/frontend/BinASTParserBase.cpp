/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "frontend/BinASTParserBase.h"

#include "vm/JSContext-inl.h"

namespace js {
namespace frontend {

BinASTParserBase::BinASTParserBase(JSContext* cx, LifoAlloc& alloc,
                                   UsedNameTracker& usedNames,
                                   HandleScriptSourceObject sourceObject,
                                   Handle<LazyScript*> lazyScript)
    : ParserSharedBase(cx, alloc, usedNames, sourceObject),
      nodeAlloc_(cx, alloc),
      lazyScript_(cx, lazyScript),
      handler_(cx, alloc, nullptr, SourceKind::Binary) {
  MOZ_ASSERT_IF(lazyScript, lazyScript->isBinAST());
}

}  // namespace frontend
}  // namespace js
