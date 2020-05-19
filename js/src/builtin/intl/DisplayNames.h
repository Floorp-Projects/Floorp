/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef builtin_intl_DisplayNames_h
#define builtin_intl_DisplayNames_h

#include <stdint.h>

#include "builtin/SelfHostingDefines.h"
#include "vm/NativeObject.h"

struct JSClass;

namespace js {
struct ClassSpec;

class DisplayNamesObject : public NativeObject {
 public:
  static const JSClass class_;
  static const JSClass& protoClass_;

  static constexpr uint32_t INTERNALS_SLOT = 0;
  static constexpr uint32_t SLOT_COUNT = 1;

  static_assert(INTERNALS_SLOT == INTL_INTERNALS_OBJECT_SLOT,
                "INTERNALS_SLOT must match self-hosting define for internals "
                "object slot");

 private:
  static const ClassSpec classSpec_;
};

}  // namespace js

#endif /* builtin_intl_DisplayNames_h */
