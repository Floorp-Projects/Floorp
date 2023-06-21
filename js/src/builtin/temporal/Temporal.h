/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef builtin_temporal_Temporal_h
#define builtin_temporal_Temporal_h

#include <stdint.h>

#include "jstypes.h"

#include "js/TypeDecls.h"
#include "vm/NativeObject.h"

namespace js {
struct ClassSpec;
class PropertyName;
}  // namespace js

namespace js::temporal {

class TemporalObject : public NativeObject {
 public:
  static const JSClass class_;

 private:
  static const ClassSpec classSpec_;
};

/**
 * ToPositiveIntegerWithTruncation ( argument )
 */
bool ToPositiveIntegerWithTruncation(JSContext* cx, JS::Handle<JS::Value> value,
                                     const char* name, double* result);

/**
 * ToIntegerWithTruncation ( argument )
 */
bool ToIntegerWithTruncation(JSContext* cx, JS::Handle<JS::Value> value,
                             const char* name, double* result);

/**
 * GetMethod ( V, P )
 */
bool GetMethod(JSContext* cx, JS::Handle<JSObject*> object,
               JS::Handle<PropertyName*> name,
               JS::MutableHandle<JS::Value> result);

/**
 * GetMethod ( V, P )
 */
bool GetMethodForCall(JSContext* cx, JS::Handle<JSObject*> object,
                      JS::Handle<PropertyName*> name,
                      JS::MutableHandle<JS::Value> result);

} /* namespace js::temporal */

#endif /* builtin_temporal_Temporal_h */
