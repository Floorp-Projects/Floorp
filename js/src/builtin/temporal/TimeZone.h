/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef builtin_temporal_TimeZone_h
#define builtin_temporal_TimeZone_h

#include "mozilla/Assertions.h"

#include <stddef.h>
#include <stdint.h>

#include "builtin/temporal/Wrapped.h"
#include "js/GCVector.h"
#include "js/RootingAPI.h"
#include "js/TypeDecls.h"
#include "js/Value.h"
#include "vm/JSObject.h"
#include "vm/NativeObject.h"

class JSLinearString;
class JS_PUBLIC_API JSTracer;
struct JSClassOps;

namespace js {
struct ClassSpec;
}

namespace mozilla::intl {
class TimeZone;
}

namespace js::temporal {

class TimeZoneObjectMaybeBuiltin : public NativeObject {
 public:
  static constexpr uint32_t IDENTIFIER_SLOT = 0;
  static constexpr uint32_t OFFSET_MINUTES_SLOT = 1;
  static constexpr uint32_t INTL_TIMEZONE_SLOT = 2;
  static constexpr uint32_t SLOT_COUNT = 3;

  // Estimated memory use for intl::TimeZone (see IcuMemoryUsage).
  static constexpr size_t EstimatedMemoryUse = 6840;

  JSString* identifier() const {
    return getFixedSlot(IDENTIFIER_SLOT).toString();
  }

  const auto& offsetMinutes() const {
    return getFixedSlot(OFFSET_MINUTES_SLOT);
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

 protected:
  static void finalize(JS::GCContext* gcx, JSObject* obj);
};

class TimeZoneObject : public TimeZoneObjectMaybeBuiltin {
 public:
  static const JSClass class_;
  static const JSClass& protoClass_;

 private:
  static const JSClassOps classOps_;
  static const ClassSpec classSpec_;
};

class BuiltinTimeZoneObject : public TimeZoneObjectMaybeBuiltin {
 public:
  static const JSClass class_;

 private:
  static const JSClassOps classOps_;
};

/**
 * Temporal time zones can be either objects or strings. Objects are either
 * instances of `Temporal.TimeZone` or user-defined time zones. Strings are
 * either canonical time zone identifiers or time zone offset strings.
 *
 * Examples of valid Temporal time zones:
 * - Any object
 * - "UTC"
 * - "America/New_York"
 * - "+00:00"
 *
 * Examples of invalid Temporal time zones:
 * - Number values
 * - "utc" (wrong case)
 * - "Etc/UTC" (canonical name is "UTC")
 * - "+00" (missing minutes part)
 * - "+00:00:00" (sub-minute precision)
 * - "+00:00:01" (sub-minute precision)
 * - "-00:00" (wrong sign for zero offset)
 *
 * String-valued Temporal time zones are an optimization to avoid allocating
 * `Temporal.TimeZone` objects when creating `Temporal.ZonedDateTime` objects.
 * For example `Temporal.ZonedDateTime.from("1970-01-01[UTC]")` doesn't require
 * to allocate a fresh `Temporal.TimeZone` object for the "UTC" time zone.
 *
 * The specification creates new `Temporal.TimeZone` objects whenever any
 * operation is performed on a string-valued Temporal time zone. This newly
 * created object can't be accessed by the user and implementations are expected
 * to optimize away the allocation.
 *
 * The following two implementation approaches are possible:
 *
 * 1. Represent string-valued time zones as JSStrings. Additionally keep a
 *    mapping from JSString to `mozilla::intl::TimeZone` to avoid repeatedly
 *    creating new `mozilla::intl::TimeZone` for time zone operations. Offset
 *    string time zones have to be special cased, because they don't use
 *    `mozilla::intl::TimeZone`. Either detect offset strings by checking the
 *    time zone identifier or store offset strings as the offset in minutes
 *    value to avoid reparsing the offset string again and again.
 * 2. Represent string-valued time zones as `Temporal.TimeZone`-like objects.
 *    These internal `Temporal.TimeZone`-like objects must not be exposed to
 *    user-code.
 *
 * Option 2 is a bit easier to implement, so we use this approach for now.
 */
class TimeZoneValue final {
  JSObject* object_ = nullptr;

 public:
  /**
   * Default initialize this TimeZoneValue.
   */
  TimeZoneValue() = default;

  /**
   * Initialize this TimeZoneValue with a "string" time zone object.
   */
  explicit TimeZoneValue(BuiltinTimeZoneObject* timeZone) : object_(timeZone) {
    MOZ_ASSERT(isString());
  }

  /**
   * Initialize this TimeZoneValue with an "object" time zone object.
   */
  explicit TimeZoneValue(JSObject* timeZone) : object_(timeZone) {
    MOZ_ASSERT(isObject());
  }

  /**
   * Initialize this TimeZoneValue from a slot Value, which must be either a
   * "string" or "object" time zone object.
   */
  explicit TimeZoneValue(const JS::Value& value) : object_(&value.toObject()) {}

  /**
   * Return true if this TimeZoneValue is not null.
   */
  explicit operator bool() const { return !!object_; }

  /**
   * Return true if this TimeZoneValue is a "string" time zone.
   */
  bool isString() const {
    return object_ && object_->is<BuiltinTimeZoneObject>();
  }

  /**
   * Return true if this TimeZoneValue is an "object" time zone.
   */
  bool isObject() const { return object_ && !isString(); }

  /**
   * Return this "string" time zone.
   */
  auto* toString() const {
    MOZ_ASSERT(isString());
    return &object_->as<BuiltinTimeZoneObject>();
  }

  /**
   * Return this "object" time zone.
   */
  JSObject* toObject() const {
    MOZ_ASSERT(isObject());
    return object_;
  }

  /**
   * Return the Value representation of this TimeZoneValue.
   */
  JS::Value toValue() const {
    if (isString()) {
      return JS::StringValue(toString()->identifier());
    }

    MOZ_ASSERT(object_);
    return JS::ObjectValue(*object_);
  }

  /**
   * Return the slot Value representation of this TimeZoneValue.
   */
  JS::Value toSlotValue() const {
    MOZ_ASSERT(object_);
    return JS::ObjectValue(*object_);
  }

  // Helper methods for (Mutable)WrappedPtrOperations.
  auto address() { return &object_; }
  auto address() const { return &object_; }

  // Trace implementation.
  void trace(JSTracer* trc);
};

struct Instant;
struct ParsedTimeZone;
struct PlainDateTime;
class CalendarValue;
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
BuiltinTimeZoneObject* CreateTemporalTimeZone(JSContext* cx,
                                              JS::Handle<JSString*> identifier);

/**
 * CreateTemporalTimeZone ( identifier [ , newTarget ] )
 */
BuiltinTimeZoneObject* CreateTemporalTimeZoneUTC(JSContext* cx);

/**
 * ToTemporalTimeZoneSlotValue ( temporalTimeZoneLike )
 */
bool ToTemporalTimeZone(JSContext* cx,
                        JS::Handle<JS::Value> temporalTimeZoneLike,
                        JS::MutableHandle<TimeZoneValue> result);

/**
 * ToTemporalTimeZoneSlotValue ( temporalTimeZoneLike )
 */
bool ToTemporalTimeZone(JSContext* cx, JS::Handle<ParsedTimeZone> string,
                        JS::MutableHandle<TimeZoneValue> result);

/**
 * ToTemporalTimeZoneObject ( timeZoneSlotValue )
 */
JSObject* ToTemporalTimeZoneObject(JSContext* cx,
                                   JS::Handle<TimeZoneValue> timeZone);

/**
 * ToTemporalTimeZoneIdentifier ( timeZoneSlotValue )
 */
JSString* ToTemporalTimeZoneIdentifier(JSContext* cx,
                                       JS::Handle<TimeZoneValue> timeZone);

/**
 * TimeZoneEquals ( one, two )
 */
bool TimeZoneEquals(JSContext* cx, JS::Handle<JSString*> one,
                    JS::Handle<JSString*> two, bool* equals);

/**
 * TimeZoneEquals ( one, two )
 */
bool TimeZoneEquals(JSContext* cx, JS::Handle<TimeZoneValue> one,
                    JS::Handle<TimeZoneValue> two, bool* equals);

/**
 * GetPlainDateTimeFor ( timeZone, instant, calendar [ ,
 * precalculatedOffsetNanoseconds ] )
 */
PlainDateTimeObject* GetPlainDateTimeFor(JSContext* cx,
                                         JS::Handle<TimeZoneValue> timeZone,
                                         const Instant& instant,
                                         JS::Handle<CalendarValue> calendar);

/**
 * GetPlainDateTimeFor ( timeZone, instant, calendar [ ,
 * precalculatedOffsetNanoseconds ] )
 */
PlainDateTimeObject* GetPlainDateTimeFor(JSContext* cx, const Instant& instant,
                                         JS::Handle<CalendarValue> calendar,
                                         int64_t offsetNanoseconds);

/**
 * GetPlainDateTimeFor ( timeZone, instant, calendar [ ,
 * precalculatedOffsetNanoseconds ] )
 */
PlainDateTime GetPlainDateTimeFor(const Instant& instant,
                                  int64_t offsetNanoseconds);

/**
 * GetPlainDateTimeFor ( timeZone, instant, calendar [ ,
 * precalculatedOffsetNanoseconds ] )
 */
bool GetPlainDateTimeFor(JSContext* cx, JS::Handle<TimeZoneValue> timeZone,
                         const Instant& instant, PlainDateTime* result);

/**
 * GetInstantFor ( timeZone, dateTime, disambiguation )
 */
bool GetInstantFor(JSContext* cx, JS::Handle<TimeZoneValue> timeZone,
                   JS::Handle<Wrapped<PlainDateTimeObject*>> dateTime,
                   TemporalDisambiguation disambiguation, Instant* result);

/**
 * FormatUTCOffsetNanoseconds ( offsetNanoseconds )
 */
JSString* FormatUTCOffsetNanoseconds(JSContext* cx, int64_t offsetNanoseconds);

/**
 * GetOffsetStringFor ( timeZone, instant )
 */
JSString* GetOffsetStringFor(JSContext* cx, JS::Handle<TimeZoneValue> timeZone,
                             const Instant& instant);

/**
 * GetOffsetStringFor ( timeZone, instant )
 */
JSString* GetOffsetStringFor(JSContext* cx, JS::Handle<TimeZoneValue> timeZone,
                             JS::Handle<Wrapped<InstantObject*>> instant);

/**
 * GetOffsetNanosecondsFor ( timeZone, instant )
 */
bool GetOffsetNanosecondsFor(JSContext* cx, JS::Handle<TimeZoneValue> timeZone,
                             JS::Handle<Wrapped<InstantObject*>> instant,
                             int64_t* offsetNanoseconds);

/**
 * GetOffsetNanosecondsFor ( timeZone, instant )
 */
bool GetOffsetNanosecondsFor(JSContext* cx, JS::Handle<TimeZoneValue> timeZone,
                             const Instant& instant,
                             int64_t* offsetNanoseconds);

using InstantVector = JS::StackGCVector<Wrapped<InstantObject*>>;

/**
 * GetPossibleInstantsFor ( timeZone, dateTime )
 */
bool GetPossibleInstantsFor(JSContext* cx, JS::Handle<TimeZoneValue> timeZone,
                            JS::Handle<Wrapped<PlainDateTimeObject*>> dateTime,
                            JS::MutableHandle<InstantVector> list);

/**
 * DisambiguatePossibleInstants ( possibleInstants, timeZone, dateTime,
 * disambiguation )
 */
bool DisambiguatePossibleInstants(
    JSContext* cx, JS::Handle<InstantVector> possibleInstants,
    JS::Handle<TimeZoneValue> timeZone,
    JS::Handle<Wrapped<PlainDateTimeObject*>> dateTimeObj,
    TemporalDisambiguation disambiguation,
    JS::MutableHandle<Wrapped<InstantObject*>> result);

// Helper for MutableWrappedPtrOperations.
bool WrapTimeZoneValueObject(JSContext* cx,
                             JS::MutableHandle<JSObject*> timeZone);

} /* namespace js::temporal */

template <>
inline bool JSObject::is<js::temporal::TimeZoneObjectMaybeBuiltin>() const {
  return is<js::temporal::TimeZoneObject>() ||
         is<js::temporal::BuiltinTimeZoneObject>();
}

namespace js {

template <typename Wrapper>
class WrappedPtrOperations<temporal::TimeZoneValue, Wrapper> {
  const auto& container() const {
    return static_cast<const Wrapper*>(this)->get();
  }

 public:
  explicit operator bool() const { return !!container(); }

  bool isString() const { return container().isString(); }

  bool isObject() const { return container().isObject(); }

  JS::Handle<temporal::BuiltinTimeZoneObject*> toString() const {
    MOZ_ASSERT(container().isString());
    auto h = JS::Handle<JSObject*>::fromMarkedLocation(container().address());
    return h.template as<temporal::BuiltinTimeZoneObject>();
  }

  JS::Handle<JSObject*> toObject() const {
    MOZ_ASSERT(container().isObject());
    return JS::Handle<JSObject*>::fromMarkedLocation(container().address());
  }

  JS::Value toValue() const { return container().toValue(); }

  JS::Value toSlotValue() const { return container().toSlotValue(); }
};

template <typename Wrapper>
class MutableWrappedPtrOperations<temporal::TimeZoneValue, Wrapper>
    : public WrappedPtrOperations<temporal::TimeZoneValue, Wrapper> {
  auto& container() { return static_cast<Wrapper*>(this)->get(); }

 public:
  /**
   * Wrap the time zone value into the current compartment.
   */
  bool wrap(JSContext* cx) {
    MOZ_ASSERT(container().isString() || container().isObject());
    auto mh =
        JS::MutableHandle<JSObject*>::fromMarkedLocation(container().address());
    return temporal::WrapTimeZoneValueObject(cx, mh);
  }
};

} /* namespace js */

#endif /* builtin_temporal_TimeZone_h */
