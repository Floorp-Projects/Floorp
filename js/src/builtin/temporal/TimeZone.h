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

class JSLinearString;
struct JSClassOps;

namespace js {
struct ClassSpec;
}

namespace mozilla::intl {
class TimeZone;
}

namespace js::temporal {

class TimeZoneObject : public NativeObject {
 public:
  static const JSClass class_;
  static const JSClass& protoClass_;

  static constexpr uint32_t IDENTIFIER_SLOT = 0;
  static constexpr uint32_t OFFSET_NANOSECONDS_SLOT = 1;
  static constexpr uint32_t INTL_TIMEZONE_SLOT = 2;
  static constexpr uint32_t SLOT_COUNT = 3;

  // Estimated memory use for intl::TimeZone (see IcuMemoryUsage).
  static constexpr size_t EstimatedMemoryUse = 6840;

  JSString* identifier() const {
    return getFixedSlot(IDENTIFIER_SLOT).toString();
  }

  const auto& offsetNanoseconds() const {
    return getFixedSlot(OFFSET_NANOSECONDS_SLOT);
  }

  mozilla::intl::TimeZone* getTimeZone() const {
    const auto& slot = getFixedSlot(INTL_TIMEZONE_SLOT);
    if (slot.isUndefined()) {
      return nullptr;
    }
    return static_cast<mozilla::intl::TimeZone*>(slot.toPrivate());
  }

  void setTimeZone(mozilla::intl::TimeZone* timeZone) {
    setFixedSlot(INTL_TIMEZONE_SLOT, JS::PrivateValue(timeZone));
  }

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

/**
 * CreateTemporalTimeZone ( identifier [ , newTarget ] )
 */
TimeZoneObject* CreateTemporalTimeZone(JSContext* cx,
                                       JS::Handle<JSString*> identifier);

/**
 * CreateTemporalTimeZone ( identifier [ , newTarget ] )
 */
TimeZoneObject* CreateTemporalTimeZoneUTC(JSContext* cx);

/**
 * FormatTimeZoneOffsetString ( offsetNanoseconds )
 */
JSString* FormatTimeZoneOffsetString(JSContext* cx, int64_t offsetNanoseconds);


} /* namespace js::temporal */

#endif /* builtin_temporal_TimeZone_h */
