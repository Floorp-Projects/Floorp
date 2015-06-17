/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
    eUseCounter_##interface_##_##name_##_getter, \
    eUseCounter_##interface_##_##name_##_setter,
#define USE_COUNTER_CSS_PROPERTY(name_, id_) \
    eUseCounter_property_##id_,
#include "mozilla/dom/UseCounterList.h"
#undef USE_COUNTER_DOM_METHOD
#undef USE_COUNTER_DOM_ATTRIBUTE
#undef USE_COUNTER_CSS_PROPERTY

#define DEPRECATED_OPERATION(op_) \
  eUseCounter_##op_,
#include "nsDeprecatedOperationList.h"
#undef DEPRECATED_OPERATION

  eUseCounter_Count
};

}

#endif
