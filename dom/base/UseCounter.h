/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef UseCounter_h_
#define UseCounter_h_

#include <stdint.h>

namespace mozilla {

enum UseCounter : int16_t {
  eUseCounter_UNKNOWN = -1,
#define USE_COUNTER_DOM_METHOD(interface_, name_) \
  eUseCounter_##interface_##_##name_,
#define USE_COUNTER_DOM_ATTRIBUTE(interface_, name_) \
  eUseCounter_##interface_##_##name_##_getter,       \
      eUseCounter_##interface_##_##name_##_setter,
#define USE_COUNTER_CUSTOM(name_, desc_) eUseCounter_custom_##name_,
#include "mozilla/dom/UseCounterList.h"
#undef USE_COUNTER_DOM_METHOD
#undef USE_COUNTER_DOM_ATTRIBUTE
#undef USE_COUNTER_CUSTOM

#define DEPRECATED_OPERATION(op_) eUseCounter_##op_,
#include "nsDeprecatedOperationList.h"
#undef DEPRECATED_OPERATION

  eUseCounter_FirstCSSProperty,
  __reset_hack = eUseCounter_FirstCSSProperty - 1,

// Need an extra level of macro nesting to force expansion of method_
// params before they get pasted.
#define CSS_PROP_USE_COUNTER(method_) eUseCounter_property_##method_,
#define CSS_PROP_PUBLIC_OR_PRIVATE(publicname_, privatename_) privatename_
#define CSS_PROP_LONGHAND(name_, id_, method_, ...) \
  CSS_PROP_USE_COUNTER(method_)
#define CSS_PROP_SHORTHAND(name_, id_, method_, ...) \
  CSS_PROP_USE_COUNTER(method_)
#define CSS_PROP_ALIAS(name_, aliasid_, id_, method_, ...) \
  CSS_PROP_USE_COUNTER(method_)
#include "mozilla/ServoCSSPropList.h"
#undef CSS_PROP_ALIAS
#undef CSS_PROP_SHORTHAND
#undef CSS_PROP_LONGHAND
#undef CSS_PROP_PUBLIC_OR_PRIVATE
#undef CSS_PROP_USE_COUNTER

  eUseCounter_EndCSSProperties,
  eUseCounter_FirstCountedUnknownProperty = eUseCounter_EndCSSProperties,
  __reset_hack_2 = eUseCounter_FirstCountedUnknownProperty - 1,

#define COUNTED_UNKNOWN_PROPERTY(name_, method_) eUseCounter_unknown_property_##method_,
#include "mozilla/CountedUnknownProperties.h"
#undef COUNTED_UNKNOWN_PROPERTY

  eUseCounter_Count
};

}  // namespace mozilla

#endif
