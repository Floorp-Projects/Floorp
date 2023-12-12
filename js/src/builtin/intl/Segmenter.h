/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef builtin_intl_Segmenter_h
#define builtin_intl_Segmenter_h

#include <stdint.h>

#include "builtin/SelfHostingDefines.h"
#include "js/Class.h"
#include "js/Value.h"
#include "vm/NativeObject.h"

struct JS_PUBLIC_API JSContext;
class JSString;

namespace js {

enum class SegmenterGranularity : int8_t { Grapheme, Word, Sentence };

class SegmenterObject : public NativeObject {
 public:
  static const JSClass class_;
  static const JSClass& protoClass_;

  static constexpr uint32_t INTERNALS_SLOT = 0;
  static constexpr uint32_t LOCALE_SLOT = 1;
  static constexpr uint32_t GRANULARITY_SLOT = 2;
  static constexpr uint32_t SLOT_COUNT = 3;

  static_assert(INTERNALS_SLOT == INTL_INTERNALS_OBJECT_SLOT,
                "INTERNALS_SLOT must match self-hosting define for internals "
                "object slot");

  JSString* getLocale() const {
    const auto& slot = getFixedSlot(LOCALE_SLOT);
    if (slot.isUndefined()) {
      return nullptr;
    }
    return slot.toString();
  }

  void setLocale(JSString* locale) {
    setFixedSlot(LOCALE_SLOT, StringValue(locale));
  }

  SegmenterGranularity getGranularity() const {
    const auto& slot = getFixedSlot(GRANULARITY_SLOT);
    if (slot.isUndefined()) {
      return SegmenterGranularity::Grapheme;
    }
    return static_cast<SegmenterGranularity>(slot.toInt32());
  }

  void setGranularity(SegmenterGranularity granularity) {
    setFixedSlot(GRANULARITY_SLOT,
                 Int32Value(static_cast<int32_t>(granularity)));
  }

 private:
  static const ClassSpec classSpec_;
};

}  // namespace js

#endif /* builtin_intl_Segmenter_h */
