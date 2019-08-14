/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef builtin_intl_Locale_h
#define builtin_intl_Locale_h

#include <stdint.h>

#include "builtin/SelfHostingDefines.h"
#include "js/Class.h"
#include "vm/NativeObject.h"

namespace js {

class GlobalObject;

class LocaleObject : public NativeObject {
 public:
  static const JSClass class_;

  static constexpr uint32_t INTERNALS_SLOT = 0;
  static constexpr uint32_t SLOT_COUNT = 1;

  static_assert(INTERNALS_SLOT == INTL_INTERNALS_OBJECT_SLOT,
                "INTERNALS_SLOT must match self-hosting define for internals "
                "object slot");
};

extern JSObject* CreateLocalePrototype(JSContext* cx,
                                       JS::Handle<JSObject*> Intl,
                                       JS::Handle<GlobalObject*> global);

/**
 * Creates an uninitialized Intl.Locale object.
 */
extern MOZ_MUST_USE bool intl_CreateUninitializedLocale(JSContext* cx,
                                                        unsigned argc,
                                                        Value* vp);

/**
 * Adds likely subtags to the given canonicalized language BCP47 subtags per
 * the "Add Likely Subtags" algorithm from UTS #35.
 *
 * Usage: subtags = intl_AddLikelySubtags(language, script, region)
 */
extern MOZ_MUST_USE bool intl_AddLikelySubtags(JSContext* cx, unsigned argc,
                                               Value* vp);

/**
 * Removes likely subtags from the given canonicalized BCP47 subtags per
 * the "Remove Likely Subtags" algorithm from UTS #35.
 *
 * Usage: subtags = intl_RemoveLikelySubtags(language, script, region)
 */
extern MOZ_MUST_USE bool intl_RemoveLikelySubtags(JSContext* cx, unsigned argc,
                                                  Value* vp);

}  // namespace js

#endif /* builtin_intl_Locale_h */
