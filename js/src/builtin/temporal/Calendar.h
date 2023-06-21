/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef builtin_temporal_Calendar_h
#define builtin_temporal_Calendar_h

#include <stdint.h>

#include "js/TypeDecls.h"
#include "js/Value.h"
#include "vm/NativeObject.h"

namespace js {
struct ClassSpec;
}

namespace js::temporal {

class CalendarObject : public NativeObject {
 public:
  static const JSClass class_;
  static const JSClass& protoClass_;

  static constexpr uint32_t IDENTIFIER_SLOT = 0;
  static constexpr uint32_t SLOT_COUNT = 1;

  JSString* identifier() const {
    return getFixedSlot(IDENTIFIER_SLOT).toString();
  }

 private:
  static const ClassSpec classSpec_;
};

/**
 * ISODaysInYear ( year )
 */
int32_t ISODaysInYear(int32_t year);

/**
 * ISODaysInMonth ( year, month )
 */
int32_t ISODaysInMonth(int32_t year, int32_t month);

/**
 * ISODaysInMonth ( year, month )
 */
int32_t ISODaysInMonth(double year, int32_t month);

/**
 * Perform `ToString(calendar)` with an optimization when the built-in
 * Temporal.Calendar.prototype.toString method is called.
 */
JSString* CalendarToString(JSContext* cx, JS::Handle<JSObject*> calendar);

} /* namespace js::temporal */

#endif /* builtin_temporal_Calendar_h */
