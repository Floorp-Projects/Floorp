/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/AutoDetectInvalidation.h"

#include "jit/JitFrames.h"
#include "vm/JSContext.h"

#include "vm/JSScript-inl.h"

namespace js::jit {

AutoDetectInvalidation::AutoDetectInvalidation(JSContext* cx,
                                               MutableHandleValue rval)
    : cx_(cx),
      ionScript_(GetTopJitJSScript(cx)->ionScript()),
      rval_(rval),
      disabled_(false) {}

void AutoDetectInvalidation::setReturnOverride() {
  cx_->setIonReturnOverride(rval_.get());
}

}  // namespace js::jit
