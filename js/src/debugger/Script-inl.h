/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef debugger_Script_inl_h
#define debugger_Script_inl_h

#include "debugger/Script.h"  // for DebuggerScript

#include "mozilla/Assertions.h"  // for AssertionConditionType, MOZ_ASSERT
#include "mozilla/Variant.h"     // for AsVariant

#include <utility>  // for move

#include "debugger/Debugger.h"  // for DebuggerScriptReferent
#include "gc/Cell.h"            // for Cell
#include "vm/JSScript.h"        // for BaseScript, JSScript, LazyScript
#include "vm/NativeObject.h"    // for NativeObject
#include "wasm/WasmJS.h"        // for WasmInstanceObject

class JSObject;

js::gc::Cell* js::DebuggerScript::getReferentCell() const {
  return static_cast<gc::Cell*>(getPrivate());
}

js::DebuggerScriptReferent js::DebuggerScript::getReferent() const {
  if (gc::Cell* cell = getReferentCell()) {
    if (cell->is<JSScript>()) {
      return mozilla::AsVariant(cell->as<JSScript>());
    }
    if (cell->is<LazyScript>()) {
      return mozilla::AsVariant(cell->as<LazyScript>());
    }
    MOZ_ASSERT(cell->is<JSObject>());
    return mozilla::AsVariant(
        &static_cast<NativeObject*>(cell)->as<WasmInstanceObject>());
  }
  return mozilla::AsVariant(static_cast<JSScript*>(nullptr));
}

js::BaseScript* js::DebuggerScript::getReferentScript() const {
  gc::Cell* cell = getReferentCell();

  MOZ_ASSERT(cell->is<JSScript>() || cell->is<LazyScript>());
  return static_cast<js::BaseScript*>(cell);
}

#endif /* debugger_Script_inl_h */
