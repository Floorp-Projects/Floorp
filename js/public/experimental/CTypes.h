/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef js_experimental_CTypes_h
#define js_experimental_CTypes_h

#include "mozilla/Attributes.h"  // MOZ_RAII

#include "jstypes.h"  // JS_FRIEND_API

struct JS_PUBLIC_API JSContext;

namespace JS {

/**
 * The type of ctypes activity that is occurring.
 */
enum class CTypesActivityType {
  BeginCall,
  EndCall,
  BeginCallback,
  EndCallback,
};

/**
 * The signature of a function invoked at the leading or trailing edge of ctypes
 * activity.
 */
using CTypesActivityCallback = void (*)(JSContext*, CTypesActivityType);

/**
 * Sets a callback that is run whenever js-ctypes is about to be used when
 * calling into C.
 */
extern JS_FRIEND_API void SetCTypesActivityCallback(JSContext* cx,
                                                    CTypesActivityCallback cb);

class MOZ_RAII JS_FRIEND_API AutoCTypesActivityCallback {
 private:
  JSContext* cx;
  CTypesActivityCallback callback;
  CTypesActivityType endType;

 public:
  AutoCTypesActivityCallback(JSContext* cx, CTypesActivityType beginType,
                             CTypesActivityType endType);

  ~AutoCTypesActivityCallback() { DoEndCallback(); }

  void DoEndCallback() {
    if (callback) {
      callback(cx, endType);
      callback = nullptr;
    }
  }
};

}  // namespace JS

#endif  // js_experimental_CTypes_h
