/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef builtin_temporal_TemporalFields_h
#define builtin_temporal_TemporalFields_h

#include "mozilla/FloatingPoint.h"

#include <initializer_list>

#include "jstypes.h"

#include "js/RootingAPI.h"
#include "js/TypeDecls.h"
#include "js/Value.h"

class JS_PUBLIC_API JSTracer;

namespace js {
class PlainObject;
}

namespace js::temporal {
enum class TemporalField {
  Year,
  Month,
  MonthCode,
  Day,
  Hour,
  Minute,
  Second,
  Millisecond,
  Microsecond,
  Nanosecond,
  Offset,
  Era,
  EraYear,
  TimeZone,
};

// Default values are specified in Table 15 [1]. `undefined` is replaced with
// an appropriate value based on the type, for example `double` fields use
// NaN whereas pointer fields use nullptr.
//
// [1] <https://tc39.es/proposal-temporal/#table-temporal-field-requirements>
struct TemporalFields final {
  double year = mozilla::UnspecifiedNaN<double>();
  double month = mozilla::UnspecifiedNaN<double>();
  JSString* monthCode = nullptr;
  double day = mozilla::UnspecifiedNaN<double>();
  double hour = 0;
  double minute = 0;
  double second = 0;
  double millisecond = 0;
  double microsecond = 0;
  double nanosecond = 0;
  JSString* offset = nullptr;
  JSString* era = nullptr;
  double eraYear = mozilla::UnspecifiedNaN<double>();
  JS::Value timeZone = JS::UndefinedValue();

  TemporalFields() = default;

  void trace(JSTracer* trc);
};
}  // namespace js::temporal

namespace js {

template <typename Wrapper>
class WrappedPtrOperations<temporal::TemporalFields, Wrapper> {
  const temporal::TemporalFields& fields() const {
    return static_cast<const Wrapper*>(this)->get();
  }

 public:
  double year() const { return fields().year; }
  double month() const { return fields().month; }
  double day() const { return fields().day; }
  double hour() const { return fields().hour; }
  double minute() const { return fields().minute; }
  double second() const { return fields().second; }
  double millisecond() const { return fields().millisecond; }
  double microsecond() const { return fields().microsecond; }
  double nanosecond() const { return fields().nanosecond; }
  double eraYear() const { return fields().eraYear; }

  JS::Handle<JSString*> monthCode() const {
    return JS::Handle<JSString*>::fromMarkedLocation(&fields().monthCode);
  }
  JS::Handle<JSString*> offset() const {
    return JS::Handle<JSString*>::fromMarkedLocation(&fields().offset);
  }
  JS::Handle<JSString*> era() const {
    return JS::Handle<JSString*>::fromMarkedLocation(&fields().era);
  }
  JS::Handle<JS::Value> timeZone() const {
    return JS::Handle<JS::Value>::fromMarkedLocation(&fields().timeZone);
  }
};

template <typename Wrapper>
class MutableWrappedPtrOperations<temporal::TemporalFields, Wrapper>
    : public WrappedPtrOperations<temporal::TemporalFields, Wrapper> {
  temporal::TemporalFields& fields() {
    return static_cast<Wrapper*>(this)->get();
  }

 public:
  double& year() { return fields().year; }
  double& month() { return fields().month; }
  double& day() { return fields().day; }
  double& hour() { return fields().hour; }
  double& minute() { return fields().minute; }
  double& second() { return fields().second; }
  double& millisecond() { return fields().millisecond; }
  double& microsecond() { return fields().microsecond; }
  double& nanosecond() { return fields().nanosecond; }
  double& eraYear() { return fields().eraYear; }

  JS::MutableHandle<JSString*> monthCode() {
    return JS::MutableHandle<JSString*>::fromMarkedLocation(
        &fields().monthCode);
  }
  JS::MutableHandle<JSString*> offset() {
    return JS::MutableHandle<JSString*>::fromMarkedLocation(&fields().offset);
  }
  JS::MutableHandle<JSString*> era() {
    return JS::MutableHandle<JSString*>::fromMarkedLocation(&fields().era);
  }
  JS::MutableHandle<JS::Value> timeZone() {
    return JS::MutableHandle<JS::Value>::fromMarkedLocation(&fields().timeZone);
  }
};

}  // namespace js

namespace js::temporal {

/**
 * PrepareTemporalFields ( fields, fieldNames, requiredFields )
 */
bool PrepareTemporalFields(JSContext* cx, JS::Handle<JSObject*> fields,
                           std::initializer_list<TemporalField> fieldNames,
                           std::initializer_list<TemporalField> requiredFields,
                           JS::MutableHandle<TemporalFields> result);

/**
 * PrepareTemporalFields ( fields, fieldNames, requiredFields )
 */
PlainObject* PrepareTemporalFields(
    JSContext* cx, JS::Handle<JSObject*> fields,
    JS::Handle<JS::StackGCVector<JS::PropertyKey>> fieldNames);

/**
 * PrepareTemporalFields ( fields, fieldNames, requiredFields )
 */
PlainObject* PrepareTemporalFields(
    JSContext* cx, JS::Handle<JSObject*> fields,
    JS::Handle<JS::StackGCVector<JS::PropertyKey>> fieldNames,
    std::initializer_list<TemporalField> requiredFields);

/**
 * PrepareTemporalFields ( fields, fieldNames, requiredFields )
 */
PlainObject* PreparePartialTemporalFields(
    JSContext* cx, JS::Handle<JSObject*> fields,
    JS::Handle<JS::StackGCVector<JS::PropertyKey>> fieldNames);

/**
 * MergeLists ( a, b )
 */
[[nodiscard]] bool MergeTemporalFieldNames(
    const JS::StackGCVector<JS::PropertyKey>& receiverFieldNames,
    const JS::StackGCVector<JS::PropertyKey>& inputFieldNames,
    JS::StackGCVector<JS::PropertyKey>& mergedFieldNames);

[[nodiscard]] bool AppendSorted(
    JSContext* cx, JS::StackGCVector<JS::PropertyKey>& fieldNames,
    std::initializer_list<TemporalField> additionalNames);

[[nodiscard]] bool SortTemporalFieldNames(
    JSContext* cx, JS::StackGCVector<JS::PropertyKey>& fieldNames);

} /* namespace js::temporal */

#endif /* builtin_temporal_TemporalFields_h */
