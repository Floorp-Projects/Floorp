/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef dbg_Script_inl_h
#define dbg_Script_inl_h

#include "dbg/Script.h"

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

#endif /* dbg_Script_inl_h */
