/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "builtin/temporal/PlainDateTime.h"

#include "mozilla/Assertions.h"

#include <algorithm>
#include <cstdlib>
#include <initializer_list>
#include <stddef.h>
#include <type_traits>
#include <utility>

#include "jsnum.h"
#include "jspubtd.h"
#include "NamespaceImports.h"

#include "builtin/temporal/Calendar.h"
#include "builtin/temporal/PlainDate.h"
#include "builtin/temporal/PlainTime.h"
#include "ds/IdValuePair.h"
#include "gc/AllocKind.h"
#include "gc/Barrier.h"
#include "js/AllocPolicy.h"
#include "js/CallArgs.h"
#include "js/CallNonGenericMethod.h"
#include "js/Class.h"
#include "js/Conversions.h"
#include "js/ErrorReport.h"
#include "js/friend/ErrorMessages.h"
#include "js/GCVector.h"
#include "js/Id.h"
#include "js/PropertyDescriptor.h"
#include "js/PropertySpec.h"
#include "js/RootingAPI.h"
#include "js/TypeDecls.h"
#include "js/Value.h"
#include "util/StringBuffer.h"
#include "vm/Compartment.h"
#include "vm/GlobalObject.h"
#include "vm/JSAtomState.h"
#include "vm/JSContext.h"
#include "vm/JSObject.h"
#include "vm/ObjectOperations.h"
#include "vm/PlainObject.h"
#include "vm/StringType.h"

#include "vm/JSObject-inl.h"
#include "vm/NativeObject-inl.h"

using namespace js;
using namespace js::temporal;

#ifdef DEBUG
/**
 * IsValidISODateTime ( year, month, day, hour, minute, second, millisecond,
 * microsecond, nanosecond )
 */
bool js::temporal::IsValidISODateTime(const PlainDateTime& dateTime) {
  return IsValidISODate(dateTime.date) && IsValidTime(dateTime.time);
}
#endif

/**
 * ISODateTimeWithinLimits ( year, month, day, hour, minute, second,
 * millisecond, microsecond, nanosecond )
 */
template <typename T>
static bool ISODateTimeWithinLimits(T year, T month, T day, T hour, T minute,
                                    T second, T millisecond, T microsecond,
                                    T nanosecond) {
  static_assert(std::is_same_v<T, int32_t> || std::is_same_v<T, double>);

  // Step 1.
  MOZ_ASSERT(IsInteger(year));
  MOZ_ASSERT(IsInteger(month));
  MOZ_ASSERT(IsInteger(day));
  MOZ_ASSERT(IsInteger(hour));
  MOZ_ASSERT(IsInteger(minute));
  MOZ_ASSERT(IsInteger(second));
  MOZ_ASSERT(IsInteger(millisecond));
  MOZ_ASSERT(IsInteger(microsecond));
  MOZ_ASSERT(IsInteger(nanosecond));

  MOZ_ASSERT(IsValidISODate(year, month, day));
  MOZ_ASSERT(
      IsValidTime(hour, minute, second, millisecond, microsecond, nanosecond));

  // js> new Date(-8_64000_00000_00000).toISOString()
  // "-271821-04-20T00:00:00.000Z"
  //
  // js> new Date(+8_64000_00000_00000).toISOString()
  // "+275760-09-13T00:00:00.000Z"

  constexpr int32_t minYear = -271821;
  constexpr int32_t maxYear = 275760;

  // Definitely in range.
  if (minYear < year && year < maxYear) {
    return true;
  }

  // -271821 April, 20
  if (year < 0) {
    if (year != minYear) {
      return false;
    }
    if (month != 4) {
      return month > 4;
    }
    if (day != (20 - 1)) {
      return day > (20 - 1);
    }
    // Needs to be past midnight on April, 19.
    return !(hour == 0 && minute == 0 && second == 0 && millisecond == 0 &&
             microsecond == 0 && nanosecond == 0);
  }

  // 275760 September, 13
  if (year != maxYear) {
    return false;
  }
  if (month != 9) {
    return month < 9;
  }
  if (day > 13) {
    return false;
  }
  return true;
}

/**
 * ISODateTimeWithinLimits ( year, month, day, hour, minute, second,
 * millisecond, microsecond, nanosecond )
 */
template <typename T>
static bool ISODateTimeWithinLimits(T year, T month, T day) {
  static_assert(std::is_same_v<T, int32_t> || std::is_same_v<T, double>);

  MOZ_ASSERT(IsValidISODate(year, month, day));

  // js> new Date(-8_64000_00000_00000).toISOString()
  // "-271821-04-20T00:00:00.000Z"
  //
  // js> new Date(+8_64000_00000_00000).toISOString()
  // "+275760-09-13T00:00:00.000Z"

  constexpr int32_t minYear = -271821;
  constexpr int32_t maxYear = 275760;

  // ISODateTimeWithinLimits is called with hour=12 and the remaining time
  // components set to zero. That means the maximum value is exclusive, whereas
  // the minimum value is inclusive.

  // FIXME: spec bug - GetUTCEpochNanoseconds when called with large |year| may
  // cause MakeDay to return NaN, which makes MakeDate return NaN, which is
  // unexpected in GetUTCEpochNanoseconds, step 4.
  // https://github.com/tc39/proposal-temporal/issues/2315

  // Definitely in range.
  if (minYear < year && year < maxYear) {
    return true;
  }

  // -271821 April, 20
  if (year < 0) {
    if (year != minYear) {
      return false;
    }
    if (month != 4) {
      return month > 4;
    }
    if (day < (20 - 1)) {
      return false;
    }
    return true;
  }

  // 275760 September, 13
  if (year != maxYear) {
    return false;
  }
  if (month != 9) {
    return month < 9;
  }
  if (day > 13) {
    return false;
  }
  return true;
}

/**
 * ISODateTimeWithinLimits ( year, month, day, hour, minute, second,
 * millisecond, microsecond, nanosecond )
 */
bool js::temporal::ISODateTimeWithinLimits(double year, double month,
                                           double day) {
  return ::ISODateTimeWithinLimits(year, month, day);
}

/**
 * ISODateTimeWithinLimits ( year, month, day, hour, minute, second,
 * millisecond, microsecond, nanosecond )
 */
bool js::temporal::ISODateTimeWithinLimits(const PlainDateTime& dateTime) {
  auto& [date, time] = dateTime;
  return ::ISODateTimeWithinLimits(date.year, date.month, date.day, time.hour,
                                   time.minute, time.second, time.millisecond,
                                   time.microsecond, time.nanosecond);
}

/**
 * ISODateTimeWithinLimits ( year, month, day, hour, minute, second,
 * millisecond, microsecond, nanosecond )
 */
bool js::temporal::ISODateTimeWithinLimits(const PlainDate& date) {
  return ::ISODateTimeWithinLimits(date.year, date.month, date.day);
}

static bool PlainDateTimeConstructor(JSContext* cx, unsigned argc, Value* vp) {
  MOZ_CRASH("NYI");
}

const JSClass PlainDateTimeObject::class_ = {
    "Temporal.PlainDateTime",
    JSCLASS_HAS_RESERVED_SLOTS(PlainDateTimeObject::SLOT_COUNT) |
        JSCLASS_HAS_CACHED_PROTO(JSProto_PlainDateTime),
    JS_NULL_CLASS_OPS,
    &PlainDateTimeObject::classSpec_,
};

const JSClass& PlainDateTimeObject::protoClass_ = PlainObject::class_;

static const JSFunctionSpec PlainDateTime_methods[] = {
    JS_FS_END,
};

static const JSFunctionSpec PlainDateTime_prototype_methods[] = {
    JS_FS_END,
};

static const JSPropertySpec PlainDateTime_prototype_properties[] = {
    JS_PS_END,
};

const ClassSpec PlainDateTimeObject::classSpec_ = {
    GenericCreateConstructor<PlainDateTimeConstructor, 3,
                             gc::AllocKind::FUNCTION>,
    GenericCreatePrototype<PlainDateTimeObject>,
    PlainDateTime_methods,
    nullptr,
    PlainDateTime_prototype_methods,
    PlainDateTime_prototype_properties,
    nullptr,
    ClassSpec::DontDefineConstructor,
};
