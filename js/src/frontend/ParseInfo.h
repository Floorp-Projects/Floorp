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

class ParserBase;

// ParseInfo owns a number of pieces of information about a parse,
// as well as controls the lifetime of parse nodes and other data
// by controling the mark and reset of the LifoAlloc.
struct MOZ_RAII ParseInfo {
  // ParseInfo's mode can be eager or deferred:
  //
  // - In Eager mode, allocation happens right away and the Function Tree is not
  //   constructed.
  // - In Deferred mode, allocation is deferred as late as possible.
  enum Mode { Eager, Deferred };

  UsedNameTracker usedNames;
  LifoAllocScope& allocScope;
  FunctionTreeHolder treeHolder;
  Mode mode;

  ParseInfo(JSContext* cx, LifoAllocScope& alloc)
      : usedNames(cx),
        allocScope(alloc),
        treeHolder(cx),
        mode(cx->realm()->behaviors().deferredParserAlloc()
                 ? ParseInfo::Mode::Deferred
                 : ParseInfo::Mode::Eager) {}

  bool isEager() { return mode == Mode::Eager; }
  bool isDeferred() { return mode == Mode::Deferred; }
};

}  // namespace frontend
}  // namespace js

#endif
