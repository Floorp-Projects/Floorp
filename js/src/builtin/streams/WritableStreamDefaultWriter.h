/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Class WritableStreamDefaultWriter. */

#ifndef builtin_streams_WritableStreamDefaultWriter_h
#define builtin_streams_WritableStreamDefaultWriter_h

#include "mozilla/Attributes.h"  // MOZ_MUST_USE

#include "js/Class.h"         // JSClass, js::ClassSpec
#include "js/Value.h"         // JS::{,Object,Undefined}Value
#include "vm/NativeObject.h"  // js::NativeObject

struct JSContext;
class JSObject;

namespace js {

class WritableStream;

class WritableStreamDefaultWriter : public NativeObject {
 public:
  /**
   * Memory layout of Stream Writer instances.
   *
   * See https://streams.spec.whatwg.org/#default-writer-internal-slots for
   * details.
   */
  enum Slots {
    Slot_ClosedPromise,
    Slot_Stream,
    Slot_ReadyPromise,
    SlotCount,
  };

  JSObject* closedPromise() const {
    return &getFixedSlot(Slot_ClosedPromise).toObject();
  }
  void setClosedPromise(JSObject* wrappedPromise) {
    setFixedSlot(Slot_ClosedPromise, JS::ObjectValue(*wrappedPromise));
  }

  bool hasStream() const { return !getFixedSlot(Slot_Stream).isUndefined(); }
  void setStream(JSObject* stream) {
    setFixedSlot(Slot_Stream, JS::ObjectValue(*stream));
  }
  void clearStream() { setFixedSlot(Slot_Stream, JS::UndefinedValue()); }

  JSObject* readyPromise() const {
    return &getFixedSlot(Slot_ReadyPromise).toObject();
  }
  void setReadyPromise(JSObject* wrappedPromise) {
    setFixedSlot(Slot_ReadyPromise, JS::ObjectValue(*wrappedPromise));
  }

  static bool constructor(JSContext* cx, unsigned argc, JS::Value* vp);
  static const ClassSpec classSpec_;
  static const JSClass class_;
  static const ClassSpec protoClassSpec_;
  static const JSClass protoClass_;
};

extern MOZ_MUST_USE WritableStreamDefaultWriter*
CreateWritableStreamDefaultWriter(JSContext* cx,
                                  JS::Handle<WritableStream*> unwrappedStream);

}  // namespace js

#endif  // builtin_streams_WritableStreamDefaultWriter_h
