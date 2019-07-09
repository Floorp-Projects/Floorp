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

namespace js {

class ArrayObject;
class FreeOp;

class NumberFormatObject : public NativeObject {
 public:
  static const Class class_;

  static constexpr uint32_t INTERNALS_SLOT = 0;
  static constexpr uint32_t UNUMBER_FORMAT_SLOT = 1;
  static constexpr uint32_t SLOT_COUNT = 2;

  static_assert(INTERNALS_SLOT == INTL_INTERNALS_OBJECT_SLOT,
                "INTERNALS_SLOT must match self-hosting define for internals "
                "object slot");

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
