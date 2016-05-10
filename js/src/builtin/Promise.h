/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef builtin_Promise_h
#define builtin_Promise_h

#include "builtin/SelfHostingDefines.h"
#include "js/Date.h"
#include "vm/NativeObject.h"
#include "vm/Time.h"

namespace js {

class AutoSetNewObjectMetadata;

class PromiseObject : public NativeObject
{
  public:
    static const unsigned RESERVED_SLOTS = 11;
    static const Class class_;
    static const Class protoClass_;
    static PromiseObject* create(JSContext* cx, HandleObject executor,
                                 HandleObject proto = nullptr);

    JS::PromiseState state() {
        int32_t state = getFixedSlot(PROMISE_STATE_SLOT).toInt32();
        MOZ_ASSERT(state >= 0 && state <= int32_t(JS::PromiseState::Rejected));
        return JS::PromiseState(state);
    }
    Value value()  {
        MOZ_ASSERT(state() == JS::PromiseState::Fulfilled);
        return getFixedSlot(PROMISE_RESULT_SLOT);
    }
    Value reason() {
        MOZ_ASSERT(state() == JS::PromiseState::Rejected);
        return getFixedSlot(PROMISE_RESULT_SLOT);
    }

    MOZ_MUST_USE bool resolve(JSContext* cx, HandleValue resolutionValue);
    MOZ_MUST_USE bool reject(JSContext* cx, HandleValue rejectionValue);

    double allocationTime() { return getFixedSlot(PROMISE_ALLOCATION_TIME_SLOT).toNumber(); }
    double resolutionTime() { return getFixedSlot(PROMISE_RESOLUTION_TIME_SLOT).toNumber(); }
    JSObject* allocationSite() { return &getFixedSlot(PROMISE_ALLOCATION_SITE_SLOT).toObject(); }
    JSObject* resolutionSite() { return &getFixedSlot(PROMISE_RESOLUTION_SITE_SLOT).toObject(); }
    double lifetime() {
        double now = JS::TimeClip(static_cast<double>(PRMJ_Now()) / PRMJ_USEC_PER_MSEC).toDouble();
        return now - allocationTime();
    }
    double timeToResolution() {
        MOZ_ASSERT(state() != JS::PromiseState::Pending);
        return resolutionTime() - allocationTime();
    }
    MOZ_MUST_USE bool dependentPromises(JSContext* cx, MutableHandle<GCVector<Value>> values);
    double getID();
};

} // namespace js

#endif /* builtin_Promise_h */
