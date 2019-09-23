/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Class WritableStream. */

#ifndef builtin_streams_WritableStream_h
#define builtin_streams_WritableStream_h

#include "mozilla/Attributes.h"  // MOZ_MUST_USE

#include "js/Class.h"         // JSClass, js::ClassSpec
#include "js/RootingAPI.h"    // JS::Handle
#include "js/Value.h"         // JS::Value
#include "vm/NativeObject.h"  // js::NativeObject

struct JSContext;

namespace js {

class WritableStream : public NativeObject {
 public:
  /**
   * Memory layout of WritableStream instances.
   *
   * See https://streams.spec.whatwg.org/#ws-internal-slots for details on
   * the stored state.
   *
   * XXX jwalden needs fleshin' out
   */
  enum Slots { SlotCount };

 public:
  static MOZ_MUST_USE WritableStream* create(
      JSContext* cx, void* nsISupportsObject_alreadyAddreffed = nullptr,
      JS::Handle<JSObject*> proto = nullptr);

  static bool constructor(JSContext* cx, unsigned argc, JS::Value* vp);

  static const ClassSpec classSpec_;
  static const JSClass class_;
  static const ClassSpec protoClassSpec_;
  static const JSClass protoClass_;
};

}  // namespace js

#endif  // builtin_streams_WritableStream_h
