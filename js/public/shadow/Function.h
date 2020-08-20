/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Shadow definition of |JSFunction| innards.  Do not use this directly! */

#ifndef js_shadow_Function_h
#define js_shadow_Function_h

#include <stdint.h>  // uint16_t

#include "js/CallArgs.h"       // JSNative
#include "js/shadow/Object.h"  // JS::shadow::Object

struct JSJitInfo;

namespace JS {

namespace shadow {

struct Function {
  shadow::Object base;
  uint16_t nargs;
  uint16_t flags;
  /* Used only for natives */
  JSNative native;
  const JSJitInfo* jitinfo;
  void* _1;
};

}  // namespace shadow

}  // namespace JS

#endif  // js_shadow_Function_h
