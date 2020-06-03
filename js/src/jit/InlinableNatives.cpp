/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/InlinableNatives.h"

#ifdef JS_HAS_INTL_API
#  include "builtin/intl/Collator.h"
#  include "builtin/intl/DateTimeFormat.h"
#  include "builtin/intl/DisplayNames.h"
#  include "builtin/intl/ListFormat.h"
#  include "builtin/intl/NumberFormat.h"
#  include "builtin/intl/PluralRules.h"
#  include "builtin/intl/RelativeTimeFormat.h"
#endif
#include "builtin/MapObject.h"
#include "vm/ArrayBufferObject.h"
#include "vm/Iteration.h"
#include "vm/SharedArrayObject.h"

using namespace js;
using namespace js::jit;

const JSClass* InlinableNativeGuardToClass(InlinableNative native) {
  switch (native) {
#ifdef JS_HAS_INTL_API
    // Intl natives.
    case InlinableNative::IntlGuardToCollator:
      return &CollatorObject::class_;
    case InlinableNative::IntlGuardToDateTimeFormat:
      return &DateTimeFormatObject::class_;
    case InlinableNative::IntlGuardToDisplayNames:
      return &DisplayNamesObject::class_;
    case InlinableNative::IntlGuardToListFormat:
      return &ListFormatObject::class_;
    case InlinableNative::IntlGuardToNumberFormat:
      return &NumberFormatObject::class_;
    case InlinableNative::IntlGuardToPluralRules:
      return &PluralRulesObject::class_;
    case InlinableNative::IntlGuardToRelativeTimeFormat:
      return &RelativeTimeFormatObject::class_;
#else
    case InlinableNative::IntlGuardToCollator:
    case InlinableNative::IntlGuardToDateTimeFormat:
    case InlinableNative::IntlGuardToDisplayNames:
    case InlinableNative::IntlGuardToListFormat:
    case InlinableNative::IntlGuardToNumberFormat:
    case InlinableNative::IntlGuardToPluralRules:
    case InlinableNative::IntlGuardToRelativeTimeFormat:
      MOZ_CRASH("Intl API disabled");
#endif

    // Utility intrinsics.
    case InlinableNative::IntrinsicGuardToArrayIterator:
      return &ArrayIteratorObject::class_;
    case InlinableNative::IntrinsicGuardToMapIterator:
      return &MapIteratorObject::class_;
    case InlinableNative::IntrinsicGuardToSetIterator:
      return &SetIteratorObject::class_;
    case InlinableNative::IntrinsicGuardToStringIterator:
      return &StringIteratorObject::class_;
    case InlinableNative::IntrinsicGuardToRegExpStringIterator:
      return &RegExpStringIteratorObject::class_;
    case InlinableNative::IntrinsicGuardToWrapForValidIterator:
      return &WrapForValidIteratorObject::class_;

    case InlinableNative::IntrinsicGuardToMapObject:
      return &MapObject::class_;
    case InlinableNative::IntrinsicGuardToSetObject:
      return &SetObject::class_;
    case InlinableNative::IntrinsicGuardToArrayBuffer:
      return &ArrayBufferObject::class_;
    case InlinableNative::IntrinsicGuardToSharedArrayBuffer:
      return &SharedArrayBufferObject::class_;

    default:
      MOZ_CRASH("Not a GuardTo instruction");
  }
}