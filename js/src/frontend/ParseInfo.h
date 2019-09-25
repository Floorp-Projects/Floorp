/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef frontend_ParseInfo_h
#define frontend_ParseInfo_h

#include "mozilla/Attributes.h"
#include "mozilla/Variant.h"

#include "ds/LifoAlloc.h"
#include "frontend/FunctionTree.h"
#include "frontend/UsedNameTracker.h"
#include "js/RealmOptions.h"
#include "js/Vector.h"
#include "vm/JSContext.h"
#include "vm/Realm.h"

namespace js {
namespace frontend {

class BigIntLiteral;

// ParseInfo owns a number of pieces of information about a parse,
// as well as controls the lifetime of parse nodes and other data
// by controling the mark and reset of the LifoAlloc.
struct MOZ_RAII ParseInfo {
  UsedNameTracker usedNames;
  LifoAllocScope& allocScope;
  FunctionTreeHolder treeHolder;

  using DeferredAllocationType = mozilla::Variant<BigIntLiteral*>;
  using DeferredAllocationVector = js::Vector<DeferredAllocationType>;

  // In Deferred mode, BigInts aren't allocated till after the parser has
  // completely parsed the parse tree; this is the list of BigIntLiteralNodes
  // that need to be published before those nodes can be inspected.
  DeferredAllocationVector deferredAllocations;

  ParseInfo(JSContext* cx, LifoAllocScope& alloc)
      : usedNames(cx),
        allocScope(alloc),
        treeHolder(cx, cx->realm()->behaviors().deferredParserAlloc()
                           ? FunctionTreeHolder::Mode::Deferred
                           : FunctionTreeHolder::Mode::Eager),
        deferredAllocations(cx) {}
};

}  // namespace frontend
}  // namespace js

#endif
