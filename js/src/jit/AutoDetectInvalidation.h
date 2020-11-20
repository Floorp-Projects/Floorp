/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_AutoDetectInvalidation_h
#define jit_AutoDetectInvalidation_h

#include "mozilla/Assertions.h"
#include "mozilla/Likely.h"

#include "NamespaceImports.h"

#include "jit/IonScript.h"
#include "js/RootingAPI.h"
#include "js/TypeDecls.h"

namespace js::jit {

// TODO(no-TI): remove.
class AutoDetectInvalidation {
  JSContext* cx_;
  IonScript* ionScript_;
  MutableHandleValue rval_;
  bool disabled_;

  void setReturnOverride();

 public:
  AutoDetectInvalidation(JSContext* cx, MutableHandleValue rval,
                         IonScript* ionScript)
      : cx_(cx), ionScript_(ionScript), rval_(rval), disabled_(false) {
    MOZ_ASSERT(ionScript);
  }

  AutoDetectInvalidation(JSContext* cx, MutableHandleValue rval);

  void disable() {
    MOZ_ASSERT(!disabled_);
    disabled_ = true;
  }

  bool shouldSetReturnOverride() const {
    return !disabled_ && ionScript_->invalidated();
  }

  ~AutoDetectInvalidation() {
    if (MOZ_UNLIKELY(shouldSetReturnOverride())) {
      setReturnOverride();
    }
  }
};

}  // namespace js::jit

#endif /* jit_AutoDetectInvalidation_h */
