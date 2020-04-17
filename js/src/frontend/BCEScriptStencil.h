/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef frontend_BCEScriptStencil_h
#define frontend_BCEScriptStencil_h

#include "mozilla/Span.h"  // mozilla::Span

#include <stdint.h>  // int32_t

#include "frontend/Stencil.h"  // ScriptStencil
#include "gc/Barrier.h"        // GCPtrAtom
#include "js/HeapAPI.h"        // JS::GCCellPtr
#include "vm/JSScript.h"       // JSTryNote, ScopeNote

struct JSContext;

namespace js {

namespace frontend {

struct BytecodeEmitter;

class BCEScriptStencil : public ScriptStencil {
  BytecodeEmitter& bce_;

  bool getNeedsFunctionEnvironmentObjects(BytecodeEmitter& bce) const;

  void init(BytecodeEmitter& bce);

 public:
  BCEScriptStencil(BytecodeEmitter& bce,
                   UniquePtr<ImmutableScriptData> immutableScriptData);

  virtual bool finishGCThings(JSContext* cx,
                              mozilla::Span<JS::GCCellPtr> output) const;
  virtual void initAtomMap(GCPtrAtom* atoms) const;
  virtual void finishInnerFunctions() const;
};

} /* namespace frontend */
} /* namespace js */

#endif /* frontend_BCEScriptStencil_h */
