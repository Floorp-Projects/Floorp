/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __FilteringWrapper_h__
#define __FilteringWrapper_h__

#include "XrayWrapper.h"
#include "mozilla/Attributes.h"
#include "jswrapper.h"
#include "js/CallNonGenericMethod.h"

struct JSPropertyDescriptor;

namespace JS {
class AutoIdVector;
}

namespace xpc {

template <typename Base, typename Policy>
class FilteringWrapper : public Base {
  public:
    MOZ_CONSTEXPR explicit FilteringWrapper(unsigned flags) : Base(flags) {}

    virtual bool enter(JSContext *cx, JS::Handle<JSObject*> wrapper, JS::Handle<jsid> id,
                       js::Wrapper::Action act, bool *bp) const MOZ_OVERRIDE;

    virtual bool getOwnPropertyDescriptor(JSContext *cx, JS::Handle<JSObject*> wrapper,
                                          JS::Handle<jsid> id,
                                          JS::MutableHandle<JSPropertyDescriptor> desc) const MOZ_OVERRIDE;
    virtual bool ownPropertyKeys(JSContext *cx, JS::Handle<JSObject*> wrapper,
                                 JS::AutoIdVector &props) const MOZ_OVERRIDE;
    virtual bool enumerate(JSContext *cx, JS::Handle<JSObject*> wrapper,
                           JS::AutoIdVector &props) const MOZ_OVERRIDE;

    virtual bool getPropertyDescriptor(JSContext *cx, JS::Handle<JSObject*> wrapper,
                                       JS::Handle<jsid> id,
                                       JS::MutableHandle<JSPropertyDescriptor> desc) const MOZ_OVERRIDE;
    virtual bool getOwnEnumerablePropertyKeys(JSContext *cx, JS::Handle<JSObject*> wrapper,
                                              JS::AutoIdVector &props) const MOZ_OVERRIDE;
    virtual bool iterate(JSContext *cx, JS::Handle<JSObject*> wrapper, unsigned flags,
                         JS::MutableHandle<JS::Value> vp) const MOZ_OVERRIDE;
    virtual bool nativeCall(JSContext *cx, JS::IsAcceptableThis test, JS::NativeImpl impl,
                            JS::CallArgs args) const MOZ_OVERRIDE;

    virtual bool defaultValue(JSContext *cx, JS::Handle<JSObject*> obj, JSType hint,
                              JS::MutableHandleValue vp) const MOZ_OVERRIDE;

    static const FilteringWrapper singleton;
};

/*
 * The HTML5 spec mandates very particular object behavior for cross-origin DOM
 * objects (Window and Location), some of which runs contrary to the way that
 * other XrayWrappers behave. We use this class to implement those semantics.
 */
class CrossOriginXrayWrapper : public SecurityXrayDOM {
  public:
    explicit CrossOriginXrayWrapper(unsigned flags);

    virtual bool getOwnPropertyDescriptor(JSContext *cx, JS::Handle<JSObject*> wrapper,
                                          JS::Handle<jsid> id,
                                          JS::MutableHandle<JSPropertyDescriptor> desc) const MOZ_OVERRIDE;
    virtual bool defineProperty(JSContext *cx, JS::Handle<JSObject*> wrapper,
                                JS::Handle<jsid> id,
                                JS::MutableHandle<JSPropertyDescriptor> desc) const MOZ_OVERRIDE;
    virtual bool ownPropertyKeys(JSContext *cx, JS::Handle<JSObject*> wrapper,
                                 JS::AutoIdVector &props) const MOZ_OVERRIDE;
    virtual bool delete_(JSContext *cx, JS::Handle<JSObject*> wrapper,
                         JS::Handle<jsid> id, bool *bp) const MOZ_OVERRIDE;
    virtual bool enumerate(JSContext *cx, JS::Handle<JSObject*> wrapper,
                           JS::AutoIdVector &props) const MOZ_OVERRIDE;
    virtual bool getPrototypeOf(JSContext *cx, JS::HandleObject wrapper,
                                JS::MutableHandleObject protop) const MOZ_OVERRIDE;

    virtual bool getPropertyDescriptor(JSContext *cx, JS::Handle<JSObject*> wrapper,
                                       JS::Handle<jsid> id,
                                       JS::MutableHandle<JSPropertyDescriptor> desc) const MOZ_OVERRIDE;
};

}

#endif /* __FilteringWrapper_h__ */
