/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi-tests/tests.h"

static bool Getter(JSContext* cx, unsigned argc, JS::Value* vp) {
  JS::CallArgs args = CallArgsFromVp(argc, vp);
  args.rval().setBoolean(true);
  return true;
}

enum PropertyDescriptorKind { DataDescriptor, AccessorDescriptor };

static bool CheckDescriptor(
    JS::Handle<mozilla::Maybe<JS::PropertyDescriptor>> desc,
    PropertyDescriptorKind kind, bool enumerable, bool writable,
    bool configurable) {
  if (desc.isNothing()) {
    return false;
  }
  if (!(kind == DataDescriptor ? desc->isDataDescriptor()
                               : desc->isAccessorDescriptor())) {
    return false;
  }
  if (desc->enumerable() != enumerable) {
    return false;
  }
  if (kind == DataDescriptor && desc->writable() != writable) {
    return false;
  }
  if (desc->configurable() != configurable) {
    return false;
  }
  return true;
}

BEGIN_TEST(testDefinePropertyIgnoredAttributes) {
  JS::RootedObject obj(cx, JS_NewPlainObject(cx));
  JS::Rooted<mozilla::Maybe<JS::PropertyDescriptor>> desc(cx);
  JS::RootedValue defineValue(cx);

  // Try a getter. Allow it to fill in the defaults. Because we're passing a
  // JSNative, JS_DefineProperty will infer JSPROP_GETTER even though we
  // aren't passing it.
  CHECK(JS_DefineProperty(cx, obj, "foo", Getter, nullptr,
                          JSPROP_IGNORE_ENUMERATE | JSPROP_IGNORE_PERMANENT));

  CHECK(JS_GetOwnPropertyDescriptor(cx, obj, "foo", &desc));

  // Note that JSPROP_READONLY is meaningless for accessor properties.
  CHECK(CheckDescriptor(desc, AccessorDescriptor, false, true, false));

  // Install another configurable property, so we can futz with it.
  CHECK(JS_DefineProperty(cx, obj, "bar", Getter, nullptr,
                          JSPROP_IGNORE_ENUMERATE));
  CHECK(JS_GetOwnPropertyDescriptor(cx, obj, "bar", &desc));
  CHECK(CheckDescriptor(desc, AccessorDescriptor, false, true, true));

  // Rewrite the descriptor to now be enumerable, leaving the configurability
  // unchanged.
  CHECK(JS_DefineProperty(cx, obj, "bar", Getter, nullptr,
                          JSPROP_IGNORE_PERMANENT | JSPROP_ENUMERATE));
  CHECK(JS_GetOwnPropertyDescriptor(cx, obj, "bar", &desc));
  CHECK(CheckDescriptor(desc, AccessorDescriptor, true, true, true));
  return true;
}
END_TEST(testDefinePropertyIgnoredAttributes)
