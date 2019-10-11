/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Intl.Locale implementation. */

#include "builtin/intl/Locale.h"

#include "mozilla/Assertions.h"
#include "mozilla/Maybe.h"
#include "mozilla/Range.h"
#include "mozilla/TextUtils.h"

#include <algorithm>
#include <iterator>
#include <utility>

#include "jsapi.h"

#include "builtin/intl/CommonFunctions.h"
#include "js/TypeDecls.h"
#include "unicode/uloc.h"
#include "unicode/utypes.h"
#include "vm/GlobalObject.h"
#include "vm/JSContext.h"
#include "vm/StringType.h"

#include "vm/JSObject-inl.h"
#include "vm/NativeObject-inl.h"

using namespace js;

/* static */ bool js::GlobalObject::addLocaleConstructor(JSContext* cx,
                                                         HandleObject intl) {
  Handle<GlobalObject*> global = cx->global();

  {
    const Value& proto = global->getReservedSlot(NATIVE_LOCALE_PROTO);
    if (!proto.isUndefined()) {
      MOZ_ASSERT(proto.isObject());
      JS_ReportErrorASCII(
          cx,
          "the Locale constructor can't be added multiple times in the"
          "same global");
      return false;
    }
  }

  JSObject* localeProto = CreateNativeLocalePrototype(cx, intl, global);
  if (!localeProto) {
    return false;
  }

  global->setReservedSlot(NATIVE_LOCALE_PROTO, ObjectValue(*localeProto));
  return true;
}

bool js::AddLocaleConstructor(JSContext* cx, JS::Handle<JSObject*> intl) {
  return GlobalObject::addLocaleConstructor(cx, intl);
}
