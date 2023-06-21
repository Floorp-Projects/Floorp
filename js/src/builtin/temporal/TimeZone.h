/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef builtin_temporal_TimeZone_h
#define builtin_temporal_TimeZone_h

#include <stddef.h>
#include <stdint.h>

#include "builtin/temporal/Wrapped.h"
#include "js/GCVector.h"
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

struct Instant;
struct PlainDateTime;
class InstantObject;
class PlainDateTimeObject;
enum class TemporalDisambiguation;

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

JSObject* ToTemporalTimeZone(JSContext* cx,
                             JS::Handle<JS::Value> temporalTimeZoneLike);

/**
 * ToTemporalTimeZone ( temporalTimeZoneLike )
 */
TimeZoneObject* ToTemporalTimeZone(JSContext* cx, JS::Handle<JSString*> string);

/**
 * GetPlainDateTimeFor ( timeZone, instant, calendar )
 */
PlainDateTimeObject* GetPlainDateTimeFor(JSContext* cx,
                                         JS::Handle<JSObject*> timeZone,
                                         const Instant& instant,
                                         JS::Handle<JSObject*> calendar);

/**
 * GetPlainDateTimeFor ( timeZone, instant, calendar )
 */
bool GetPlainDateTimeFor(JSContext* cx, JS::Handle<JSObject*> timeZone,
                         JS::Handle<InstantObject*> instant,
                         PlainDateTime* result);

/**
 * GetPlainDateTimeFor ( timeZone, instant, calendar )
 */
bool GetPlainDateTimeFor(JSContext* cx, JS::Handle<JSObject*> timeZone,
                         const Instant& instant, PlainDateTime* result);

/**
 * GetInstantFor ( timeZone, dateTime, disambiguation )
 */
bool GetInstantFor(JSContext* cx, JS::Handle<JSObject*> timeZone,
                   JS::Handle<Wrapped<PlainDateTimeObject*>> dateTime,
                   TemporalDisambiguation disambiguation, Instant* result);

/**
 * GetOffsetStringFor ( timeZone, instant )
 */
JSString* GetOffsetStringFor(JSContext* cx, JS::Handle<JSObject*> timeZone,
                             JS::Handle<Wrapped<InstantObject*>> instant);

/**
 * GetOffsetNanosecondsFor ( timeZone, instant )
 */
bool GetOffsetNanosecondsFor(JSContext* cx, JS::Handle<JSObject*> timeZone,
                             JS::Handle<Wrapped<InstantObject*>> instant,
                             int64_t* offsetNanoseconds);

/**
 * GetOffsetNanosecondsFor ( timeZone, instant )
 */
bool GetOffsetNanosecondsFor(JSContext* cx, JS::Handle<JSObject*> timeZone,
                             const Instant& instant,
                             int64_t* offsetNanoseconds);

using InstantVector = JS::StackGCVector<Wrapped<InstantObject*>>;

/**
 * GetPossibleInstantsFor ( timeZone, dateTime )
 */
bool GetPossibleInstantsFor(JSContext* cx, JS::Handle<JSObject*> timeZone,
                            JS::Handle<Wrapped<PlainDateTimeObject*>> dateTime,
                            JS::MutableHandle<InstantVector> list);

/**
 * DisambiguatePossibleInstants ( possibleInstants, timeZone, dateTime,
 * disambiguation )
 */
Wrapped<InstantObject*> DisambiguatePossibleInstants(
    JSContext* cx, JS::Handle<InstantVector> possibleInstants,
    JS::Handle<JSObject*> timeZone,
    JS::Handle<Wrapped<PlainDateTimeObject*>> dateTimeObj,
    TemporalDisambiguation disambiguation);

/**
 * FormatTimeZoneOffsetString ( offsetNanoseconds )
 */
JSString* FormatTimeZoneOffsetString(JSContext* cx, int64_t offsetNanoseconds);

/**
 * FormatISOTimeZoneOffsetString ( offsetNanoseconds )
 */
JSString* FormatISOTimeZoneOffsetString(JSContext* cx,
                                        int64_t offsetNanoseconds);
/**
 * Perform `ToString(timeZone)` with an optimization when the built-in
 * Temporal.TimeZone.prototype.toString method is called.
 */
JSString* TimeZoneToString(JSContext* cx, JS::Handle<JSObject*> timeZone);

} /* namespace js::temporal */

#endif /* builtin_temporal_TimeZone_h */
