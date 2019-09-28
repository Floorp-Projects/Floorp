/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Class WritableStreamDefaultController. */

#include "builtin/streams/WritableStreamDefaultController.h"

#include "builtin/streams/ClassSpecMacro.h"  // JS_STREAMS_CLASS_SPEC
#include "builtin/streams/WritableStream.h"  // js::WritableStream
#include "js/Class.h"                        // js::ClassSpec, JS_NULL_CLASS_OPS
#include "js/PropertySpec.h"  // JS{Function,Property}Spec, JS_{FS,PS}_END

using js::ClassSpec;
using js::WritableStreamDefaultController;

/*** 4.7. Class WritableStreamDefaultController *****************************/

/**
 * Streams spec, 4.7.3.
 * new WritableStreamDefaultController()
 */
bool WritableStreamDefaultController::constructor(JSContext* cx, unsigned argc,
                                                  Value* vp) {
  // Step 1: Throw a TypeError.
  JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                            JSMSG_BOGUS_CONSTRUCTOR,
                            "WritableStreamDefaultController");
  return false;
}

static const JSPropertySpec WritableStreamDefaultController_properties[] = {
    JS_PS_END};

static const JSFunctionSpec WritableStreamDefaultController_methods[] = {
    JS_FS_END};

JS_STREAMS_CLASS_SPEC(WritableStreamDefaultController, 0, SlotCount,
                      ClassSpec::DontDefineConstructor, 0, JS_NULL_CLASS_OPS);
