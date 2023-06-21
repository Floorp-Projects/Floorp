/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "builtin/temporal/TemporalFields.h"

#include "mozilla/Assertions.h"
#include "mozilla/FloatingPoint.h"
#include "mozilla/Likely.h"
#include "mozilla/Maybe.h"
#include "mozilla/Range.h"
#include "mozilla/RangedPtr.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <stdint.h>
#include <utility>

#include "NamespaceImports.h"

#include "builtin/temporal/Temporal.h"
#include "ds/Sort.h"
#include "gc/Barrier.h"
#include "gc/Tracer.h"
#include "js/AllocPolicy.h"
#include "js/ComparisonOperators.h"
#include "js/Conversions.h"
#include "js/ErrorReport.h"
#include "js/friend/ErrorMessages.h"
#include "js/GCVector.h"
#include "js/Id.h"
#include "js/Printer.h"
#include "js/RootingAPI.h"
#include "js/TracingAPI.h"
#include "js/Utility.h"
#include "js/Value.h"
#include "vm/JSAtomState.h"
#include "vm/JSContext.h"
#include "vm/PlainObject.h"
#include "vm/StringType.h"

#include "vm/JSAtom-inl.h"
#include "vm/JSObject-inl.h"
#include "vm/ObjectOperations-inl.h"

using namespace js;
using namespace js::temporal;

static int32_t ComparePropertyKey(PropertyKey x, PropertyKey y) {
  MOZ_ASSERT(x.isAtom() || x.isInt());
  MOZ_ASSERT(y.isAtom() || y.isInt());

  if (MOZ_LIKELY(x.isAtom() && y.isAtom())) {
    return CompareStrings(x.toAtom(), y.toAtom());
  }

  if (x.isInt() && y.isInt()) {
    return x.toInt() - y.toInt();
  }

  uint32_t index = uint32_t(x.isInt() ? x.toInt() : y.toInt());
  JSAtom* str = x.isAtom() ? x.toAtom() : y.toAtom();

  char16_t buf[UINT32_CHAR_BUFFER_LENGTH];
  mozilla::RangedPtr<char16_t> end(std::end(buf), buf, std::end(buf));
  mozilla::RangedPtr<char16_t> start = BackfillIndexInCharBuffer(index, end);

  int32_t result = CompareChars(start.get(), end - start, str);
  return x.isInt() ? result : -result;
}

bool js::temporal::SortTemporalFieldNames(
    JSContext* cx, JS::StackGCVector<PropertyKey>& fieldNames) {
  // Create scratch space for MergeSort().
  JS::StackGCVector<PropertyKey> scratch(cx);
  if (!scratch.resize(fieldNames.length())) {
    return false;
  }

  // Sort all field names in alphabetical order.
  return MergeSort(fieldNames.begin(), fieldNames.length(), scratch.begin(),
                   [](const auto& x, const auto& y, bool* lessOrEqual) {
                     *lessOrEqual = ComparePropertyKey(x, y) <= 0;
                     return true;
                   });
}
