/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __CrossOriginWrapper_h__
#define __CrossOriginWrapper_h__

#include "mozilla/Attributes.h"

#include "jswrapper.h"

namespace xpc {

class WaiveXrayWrapper : public js::CrossCompartmentWrapper {
  public:
    WaiveXrayWrapper(unsigned flags);
    virtual ~WaiveXrayWrapper();

    virtual bool getPropertyDescriptor(JSContext *cx, JS::Handle<JSObject*> wrapper,
                                       JS::Handle<jsid> id,
                                       JS::MutableHandle<JSPropertyDescriptor> desc,
                                       unsigned flags) MOZ_OVERRIDE;
    virtual bool getOwnPropertyDescriptor(JSContext *cx, JS::Handle<JSObject*> wrapper,
                                          JS::Handle<jsid> id,
                                          JS::MutableHandle<JSPropertyDescriptor> desc,
                                          unsigned flags) MOZ_OVERRIDE;
    virtual bool get(JSContext *cx, JS::Handle<JSObject*> wrapper, JS::Handle<JSObject*> receiver,
                     JS::Handle<jsid> id, JS::MutableHandle<JS::Value> vp) MOZ_OVERRIDE;

    virtual bool call(JSContext *cx, JS::Handle<JSObject*> wrapper,
                      const JS::CallArgs &args) MOZ_OVERRIDE;
    virtual bool construct(JSContext *cx, JS::Handle<JSObject*> wrapper,
                           const JS::CallArgs &args) MOZ_OVERRIDE;

    virtual bool nativeCall(JSContext *cx, JS::IsAcceptableThis test,
                            JS::NativeImpl impl, JS::CallArgs args) MOZ_OVERRIDE;

    virtual bool getPrototypeOf(JSContext *cx, JS::Handle<JSObject*> wrapper,
                                JS::MutableHandle<JSObject*> protop) MOZ_OVERRIDE;

    static WaiveXrayWrapper singleton;
};

}

#endif
