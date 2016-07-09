/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __CrossOriginWrapper_h__
#define __CrossOriginWrapper_h__

#include "mozilla/Attributes.h"

#include "jswrapper.h"

namespace xpc {

class WaiveXrayWrapper : public js::CrossCompartmentWrapper {
  public:
    explicit constexpr WaiveXrayWrapper(unsigned flags) : js::CrossCompartmentWrapper(flags) { }

    virtual bool getOwnPropertyDescriptor(JSContext* cx, JS::Handle<JSObject*> wrapper,
                                          JS::Handle<jsid> id,
                                          JS::MutableHandle<JS::PropertyDescriptor> desc) const override;
    virtual bool getPrototype(JSContext* cx, JS::Handle<JSObject*> wrapper,
                              JS::MutableHandle<JSObject*> protop) const override;
    virtual bool getPrototypeIfOrdinary(JSContext* cx, JS::Handle<JSObject*> wrapper,
                                        bool* isOrdinary,
                                        JS::MutableHandle<JSObject*> protop) const override;
    virtual bool get(JSContext* cx, JS::Handle<JSObject*> wrapper, JS::Handle<JS::Value> receiver,
                     JS::Handle<jsid> id, JS::MutableHandle<JS::Value> vp) const override;
    virtual bool call(JSContext* cx, JS::Handle<JSObject*> wrapper,
                      const JS::CallArgs& args) const override;
    virtual bool construct(JSContext* cx, JS::Handle<JSObject*> wrapper,
                           const JS::CallArgs& args) const override;

    virtual bool enumerate(JSContext* cx, JS::Handle<JSObject*> proxy,
                           JS::MutableHandle<JSObject*> objp) const override;
    virtual bool nativeCall(JSContext* cx, JS::IsAcceptableThis test,
                            JS::NativeImpl impl, const JS::CallArgs& args) const override;
    virtual bool getPropertyDescriptor(JSContext* cx, JS::Handle<JSObject*> wrapper,
                                       JS::Handle<jsid> id,
                                       JS::MutableHandle<JS::PropertyDescriptor> desc) const override;

    static const WaiveXrayWrapper singleton;
};

} // namespace xpc

#endif
