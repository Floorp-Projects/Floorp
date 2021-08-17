/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_BINDINGS_PINNEDSTRINGID_H_
#define DOM_BINDINGS_PINNEDSTRINGID_H_

#include "js/GCAnnotations.h"
#include "js/Id.h"
#include "js/RootingAPI.h"
#include "js/String.h"
#include "jsapi.h"

class JSString;
struct JSContext;

namespace mozilla::dom {
/*
 * Holds a jsid that is initialized to a pinned string, with automatic
 * conversion to Handle<jsid>, as it is held live forever by pinning.
 */
class PinnedStringId {
  jsid id;

 public:
  constexpr PinnedStringId() : id(JSID_VOID) {}

  bool init(JSContext* cx, const char* string) {
    JSString* str = JS_AtomizeAndPinString(cx, string);
    if (!str) {
      return false;
    }
    id = JS::PropertyKey::fromPinnedString(str);
    return true;
  }

  operator const jsid&() const { return id; }

  operator JS::Handle<jsid>() const {
    /* This is safe because we have pinned the string. */
    return JS::Handle<jsid>::fromMarkedLocation(&id);
  }
} JS_HAZ_ROOTED;
}  // namespace mozilla::dom

#endif
