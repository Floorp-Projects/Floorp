/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "js/Id.h"
#include "js/RootingAPI.h"

#include "vm/SymbolType.h"

#include "vm/JSAtom-inl.h"

static const jsid voidIdValue = JSID_VOID;
static const jsid emptyIdValue = JSID_EMPTY;
const JS::HandleId JSID_VOIDHANDLE =
    JS::HandleId::fromMarkedLocation(&voidIdValue);
const JS::HandleId JSID_EMPTYHANDLE =
    JS::HandleId::fromMarkedLocation(&emptyIdValue);

bool JS::PropertyKey::isWellKnownSymbol(JS::SymbolCode code) const {
  MOZ_ASSERT(uint32_t(code) < WellKnownSymbolLimit);
  if (!isSymbol()) {
    return false;
  }
  return toSymbol()->code() == code;
}

/* static */ JS::PropertyKey JS::PropertyKey::fromPinnedString(JSString* str) {
  MOZ_ASSERT(str->asAtom().isPinned());
  return js::AtomToId(&str->asAtom());
}

/* static */ bool JS::PropertyKey::isNonIntAtom(JSAtom* atom) {
  uint32_t index;
  if (!atom->isIndex(&index)) {
    return true;
  }
  static_assert(JSID_INT_MIN == 0);
  return index > JSID_INT_MAX;
}

/* static */ bool JS::PropertyKey::isNonIntAtom(JSString* str) {
  return JS::PropertyKey::isNonIntAtom(&str->asAtom());
}
