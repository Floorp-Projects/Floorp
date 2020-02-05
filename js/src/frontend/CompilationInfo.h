/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef frontend_CompilationInfo_h
#define frontend_CompilationInfo_h

#include "mozilla/Attributes.h"
#include "mozilla/Variant.h"

#include "ds/LifoAlloc.h"
#include "frontend/FunctionTree.h"
#include "frontend/SharedContext.h"
#include "frontend/Stencil.h"
#include "frontend/UsedNameTracker.h"
#include "js/RealmOptions.h"
#include "js/SourceText.h"
#include "js/Vector.h"
#include "vm/JSContext.h"
#include "vm/Realm.h"

namespace js {
namespace frontend {

// CompilationInfo owns a number of pieces of information about script
// compilation as well as controls the lifetime of parse nodes and other data by
// controling the mark and reset of the LifoAlloc.
struct MOZ_RAII CompilationInfo {
  JSContext* cx;
  const JS::ReadOnlyCompileOptions& options;

  // Until we have dealt with Atoms in the front end, we need to hold
  // onto them.
  AutoKeepAtoms keepAtoms;

  Directives directives;

  // The resulting outermost script for the compilation powered
  // by this CompilationInfo.
  JS::Rooted<JSScript*> script;

  UsedNameTracker usedNames;
  LifoAllocScope& allocScope;
  FunctionTreeHolder treeHolder;
  // Hold onto the RegExpCreationData and BigIntCreationDatas that are
  // allocated during parse to ensure correct destruction.
  Vector<RegExpCreationData> regExpData;
  Vector<BigIntCreationData> bigIntData;

  // A rooted list of scopes created during this parse.
  //
  // To ensure that ScopeCreationData's destructors fire, and thus our HeapPtr
  // barriers, we store the scopeCreationData at this level so that they
  // can be safely destroyed, rather than LifoAllocing them with the rest of
  // the parser data structures.
  //
  // References to scopes are controlled via AbstractScope, which holds onto
  // an index (and CompilationInfo reference).
  JS::RootedVector<ScopeCreationData> scopeCreationData;

  JS::Rooted<ScriptSourceObject*> sourceObject;

  // Construct a CompilationInfo
  CompilationInfo(JSContext* cx, LifoAllocScope& alloc,
                  const JS::ReadOnlyCompileOptions& options)
      : cx(cx),
        options(options),
        keepAtoms(cx),
        directives(options.forceStrictMode()),
        script(cx),
        usedNames(cx),
        allocScope(alloc),
        treeHolder(cx),
        regExpData(cx),
        bigIntData(cx),
        scopeCreationData(cx),
        sourceObject(cx) {}

  bool init(JSContext* cx);

  void initFromSourceObject(ScriptSourceObject* sso) { sourceObject = sso; }

  template <typename Unit>
  MOZ_MUST_USE bool assignSource(JS::SourceText<Unit>& sourceBuffer) {
    return sourceObject->source()->assignSource(cx, options, sourceBuffer);
  }

  // To avoid any misuses, make sure this is neither copyable,
  // movable or assignable.
  CompilationInfo(const CompilationInfo&) = delete;
  CompilationInfo(CompilationInfo&&) = delete;
  CompilationInfo& operator=(const CompilationInfo&) = delete;
  CompilationInfo& operator=(CompilationInfo&&) = delete;
};

}  // namespace frontend
}  // namespace js

#endif
