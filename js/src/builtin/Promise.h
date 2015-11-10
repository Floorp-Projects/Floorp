/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef builtin_Promise_h
#define builtin_Promise_h

#include "builtin/SelfHostingDefines.h"
#include "vm/NativeObject.h"

namespace js {

class AutoSetNewObjectMetadata;

class PromiseObject : public NativeObject
{
  public:
    static const unsigned RESERVED_SLOTS = 6;
    static const Class class_;
    static const Class protoClass_;
    static PromiseObject* create(JSContext* cx, HandleObject executor,
                                 HandleObject proto = nullptr);

    JS::PromiseState state() {
        int32_t state = getReservedSlot(PROMISE_STATE_SLOT).toInt32();
        MOZ_ASSERT(state >= 0 && state <= int32_t(JS::PromiseState::Rejected));
        return JS::PromiseState(state);
    }

    bool resolve(JSContext* cx, HandleValue resolutionValue);
    bool reject(JSContext* cx, HandleValue rejectionValue);
};

} // namespace js

#endif /* builtin_Promise_h */
