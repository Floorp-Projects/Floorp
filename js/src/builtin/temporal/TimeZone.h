/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef builtin_temporal_TimeZone_h
#define builtin_temporal_TimeZone_h

#include <stddef.h>
#include <stdint.h>

#include "js/RootingAPI.h"
#include "js/TypeDecls.h"
#include "js/Value.h"
#include "vm/NativeObject.h"

struct JSClassOps;

namespace js {
struct ClassSpec;
}

namespace js::temporal {

class TimeZoneObject : public NativeObject {
 public:
  static const JSClass class_;
  static const JSClass& protoClass_;

  static constexpr uint32_t SLOT_COUNT = 0;

 private:
  static const JSClassOps classOps_;
  static const ClassSpec classSpec_;

  static void finalize(JS::GCContext* gcx, JSObject* obj);
};

/**
 * IsValidTimeZoneName ( timeZone )
 * IsAvailableTimeZoneName ( timeZone )
 */
bool IsValidTimeZoneName(JSContext* cx, JS::Handle<JSString*> timeZone,
                         JS::MutableHandle<JSAtom*> validatedTimeZone);

/**
 * CanonicalizeTimeZoneName ( timeZone )
 */
JSString* CanonicalizeTimeZoneName(JSContext* cx,
                                   JS::Handle<JSLinearString*> timeZone);

/**
 * IsValidTimeZoneName ( timeZone )
 * IsAvailableTimeZoneName ( timeZone )
 * CanonicalizeTimeZoneName ( timeZone )
 */
JSString* ValidateAndCanonicalizeTimeZoneName(JSContext* cx,
                                              JS::Handle<JSString*> timeZone);

} /* namespace js::temporal */

#endif /* builtin_temporal_TimeZone_h */
