/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "frontend/BinASTParserBase.h"

#include "vm/JSContext-inl.h"

namespace js {
namespace frontend {

using UsedNamePtr = UsedNameTracker::UsedNameMap::Ptr;

BinASTParserBase::BinASTParserBase(JSContext* cx, LifoAlloc& alloc, UsedNameTracker& usedNames,
                                   HandleScriptSourceObject sourceObject,
                                   Handle<LazyScript*> lazyScript)
  : AutoGCRooter(cx, AutoGCRooter::Tag::BinParser)
  , cx_(cx)
  , alloc_(alloc)
  , traceListHead_(nullptr)
  , usedNames_(usedNames)
  , nodeAlloc_(cx, alloc)
  , keepAtoms_(cx)
  , sourceObject_(cx, sourceObject)
  , lazyScript_(cx, lazyScript)
  , parseContext_(nullptr)
  , factory_(cx, alloc, nullptr, SourceKind::Binary)
{
    MOZ_ASSERT_IF(lazyScript, lazyScript->isBinAST());
    cx->frontendCollectionPool().addActiveCompilation();
    tempPoolMark_ = alloc.mark();
}

BinASTParserBase::~BinASTParserBase()
{
    alloc_.release(tempPoolMark_);

    /*
     * The parser can allocate enormous amounts of memory for large functions.
     * Eagerly free the memory now (which otherwise won't be freed until the
     * next GC) to avoid unnecessary OOMs.
     */
    alloc_.freeAllIfHugeAndUnused();

    cx_->frontendCollectionPool().removeActiveCompilation();
}

bool
BinASTParserBase::hasUsedName(HandlePropertyName name)
{
    if (UsedNamePtr p = usedNames_.lookup(name)) {
        return p->value().isUsedInScript(parseContext_->scriptId());
    }

    return false;
}

} // namespace frontend
} // namespace js
