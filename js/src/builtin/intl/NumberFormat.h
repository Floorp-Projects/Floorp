/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef builtin_intl_NumberFormat_h
#define builtin_intl_NumberFormat_h

#include "mozilla/Attributes.h"

#include <stdint.h>

#include "builtin/SelfHostingDefines.h"
#include "gc/Barrier.h"
#include "js/Class.h"
#include "js/Vector.h"
#include "vm/NativeObject.h"
#include "vm/Runtime.h"

struct UFormattedNumber;
struct UNumberFormatter;

namespace js {

class ArrayObject;
class FreeOp;

class NumberFormatObject : public NativeObject {
 public:
  static const Class class_;

  static constexpr uint32_t INTERNALS_SLOT = 0;
  static constexpr uint32_t UNUMBER_FORMATTER_SLOT = 1;
  static constexpr uint32_t UFORMATTED_NUMBER_SLOT = 2;
  static constexpr uint32_t SLOT_COUNT = 3;

  static_assert(INTERNALS_SLOT == INTL_INTERNALS_OBJECT_SLOT,
                "INTERNALS_SLOT must match self-hosting define for internals "
                "object slot");

  UNumberFormatter* getNumberFormatter() const {
    const auto& slot = getFixedSlot(UNUMBER_FORMATTER_SLOT);
    if (slot.isUndefined()) {
      return nullptr;
    }
    return static_cast<UNumberFormatter*>(slot.toPrivate());
  }

  void setNumberFormatter(UNumberFormatter* formatter) {
    setFixedSlot(UNUMBER_FORMATTER_SLOT, PrivateValue(formatter));
  }

  UFormattedNumber* getFormattedNumber() const {
    const auto& slot = getFixedSlot(UFORMATTED_NUMBER_SLOT);
    if (slot.isUndefined()) {
      return nullptr;
    }
    return static_cast<UFormattedNumber*>(slot.toPrivate());
  }

  void setFormattedNumber(UFormattedNumber* formatted) {
    setFixedSlot(UFORMATTED_NUMBER_SLOT, PrivateValue(formatted));
  }

 private:
  static const ClassOps classOps_;

  static void finalize(FreeOp* fop, JSObject* obj);
};

extern JSObject* CreateNumberFormatPrototype(JSContext* cx, HandleObject Intl,
                                             Handle<GlobalObject*> global,
                                             MutableHandleObject constructor);

/**
 * Returns a new instance of the standard built-in NumberFormat constructor.
 * Self-hosted code cannot cache this constructor (as it does for others in
 * Utilities.js) because it is initialized after self-hosted code is compiled.
 *
 * Usage: numberFormat = intl_NumberFormat(locales, options)
 */
extern MOZ_MUST_USE bool intl_NumberFormat(JSContext* cx, unsigned argc,
                                           Value* vp);

/**
 * Returns an object indicating the supported locales for number formatting
 * by having a true-valued property for each such locale with the
 * canonicalized language tag as the property name. The object has no
 * prototype.
 *
 * Usage: availableLocales = intl_NumberFormat_availableLocales()
 */
extern MOZ_MUST_USE bool intl_NumberFormat_availableLocales(JSContext* cx,
                                                            unsigned argc,
                                                            Value* vp);

/**
 * Returns the numbering system type identifier per Unicode
 * Technical Standard 35, Unicode Locale Data Markup Language, for the
 * default numbering system for the given locale.
 *
 * Usage: defaultNumberingSystem = intl_numberingSystem(locale)
 */
extern MOZ_MUST_USE bool intl_numberingSystem(JSContext* cx, unsigned argc,
                                              Value* vp);

/**
 * Returns a string representing the number x according to the effective
 * locale and the formatting options of the given NumberFormat.
 *
 * Spec: ECMAScript Internationalization API Specification, 11.3.2.
 *
 * Usage: formatted = intl_FormatNumber(numberFormat, x, formatToParts)
 */
extern MOZ_MUST_USE bool intl_FormatNumber(JSContext* cx, unsigned argc,
                                           Value* vp);

namespace intl {

/**
 * Class to create a number formatter skeleton.
 *
 * The skeleton syntax is documented at:
 * https://github.com/unicode-org/icu/blob/master/docs/userguide/format_parse/numbers/skeletons.md
 */
class MOZ_STACK_CLASS NumberFormatterSkeleton final {
  static constexpr size_t DefaultVectorSize = 128;
  using SkeletonVector = Vector<char16_t, DefaultVectorSize>;

  SkeletonVector vector_;

  bool append(char16_t c) { return vector_.append(c); }

  bool appendN(char16_t c, size_t times) { return vector_.appendN(c, times); }

  template <size_t N>
  bool append(const char16_t (&chars)[N]) {
    static_assert(N > 0,
                  "should only be used with string literals or properly "
                  "null-terminated arrays");
    MOZ_ASSERT(chars[N - 1] == '\0',
               "should only be used with string literals or properly "
               "null-terminated arrays");
    return vector_.append(chars, N - 1);  // Without trailing \0.
  }

  template <size_t N>
  bool appendToken(const char16_t (&token)[N]) {
    return append(token) && append(' ');
  }

 public:
  explicit NumberFormatterSkeleton(JSContext* cx) : vector_(cx) {}

  /**
   * Return a new UNumberFormatter based on this skeleton.
   */
  UNumberFormatter* toFormatter(JSContext* cx, const char* locale);

  enum class CurrencyDisplay { Code, Name, Symbol };

  /**
   * Set this skeleton to display a currency amount. |currency| must be a
   * three-letter currency code.
   *
   * https://github.com/unicode-org/icu/blob/master/docs/userguide/format_parse/numbers/skeletons.md#unit
   */
  MOZ_MUST_USE bool currency(CurrencyDisplay display, JSLinearString* currency);

  /**
   * Set this skeleton to display a percent number.
   *
   * https://github.com/unicode-org/icu/blob/master/docs/userguide/format_parse/numbers/skeletons.md#unit
   * https://github.com/unicode-org/icu/blob/master/docs/userguide/format_parse/numbers/skeletons.md#scale
   */
  MOZ_MUST_USE bool percent();

  /**
   * Set the fraction digits settings for this skeleton. |min| can be zero,
   * |max| must be larger-or-equal to |min|.
   *
   * https://github.com/unicode-org/icu/blob/master/docs/userguide/format_parse/numbers/skeletons.md#fraction-precision
   */
  MOZ_MUST_USE bool fractionDigits(uint32_t min, uint32_t max);

  /**
   * Set the integer-width settings for this skeleton. |min| must be a non-zero
   * number.
   *
   * https://github.com/unicode-org/icu/blob/master/docs/userguide/format_parse/numbers/skeletons.md#integer-width
   */
  MOZ_MUST_USE bool integerWidth(uint32_t min);

  /**
   * Set the significant digits settings for this skeleton. |min| must be a
   * non-zero number, |max| must be larger-or-equal to |min|.
   *
   * https://github.com/unicode-org/icu/blob/master/docs/userguide/format_parse/numbers/skeletons.md#significant-digits-precision
   */
  MOZ_MUST_USE bool significantDigits(uint32_t min, uint32_t max);

  /**
   * Enable or disable grouping for this skeleton.
   *
   * https://github.com/unicode-org/icu/blob/master/docs/userguide/format_parse/numbers/skeletons.md#grouping
   */
  MOZ_MUST_USE bool useGrouping(bool on);

  /**
   * Set the rounding mode to 'half-up' for this skeleton.
   *
   * https://github.com/unicode-org/icu/blob/master/docs/userguide/format_parse/numbers/skeletons.md#rounding-mode
   */
  MOZ_MUST_USE bool roundingModeHalfUp();
};

using FieldType = js::ImmutablePropertyNamePtr JSAtomState::*;

struct Field {
  uint32_t begin;
  uint32_t end;
  FieldType type;

  // Needed for vector-resizing scratch space.
  Field() = default;

  Field(uint32_t begin, uint32_t end, FieldType type)
      : begin(begin), end(end), type(type) {}
};

class NumberFormatFields {
  using FieldsVector = Vector<Field, 16>;

  FieldsVector fields_;
  double number_;

 public:
  NumberFormatFields(JSContext* cx, double number)
      : fields_(cx), number_(number) {}

  MOZ_MUST_USE bool append(int32_t field, int32_t begin, int32_t end);

  MOZ_MUST_USE ArrayObject* toArray(JSContext* cx,
                                    JS::HandleString overallResult,
                                    FieldType unitType);
};

}  // namespace intl
}  // namespace js

#endif /* builtin_intl_NumberFormat_h */
