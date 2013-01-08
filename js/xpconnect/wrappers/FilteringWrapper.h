/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __FilteringWrapper_h__
#define __FilteringWrapper_h__

#include <jsapi.h>
#include <jswrapper.h>

namespace xpc {

template <typename Base, typename Policy>
class FilteringWrapper : public Base {
  public:
    FilteringWrapper(unsigned flags);
    virtual ~FilteringWrapper();

    virtual bool getPropertyDescriptor(JSContext *cx, JSObject *wrapper, jsid id, js::PropertyDescriptor *desc, unsigned flags) MOZ_OVERRIDE;
    virtual bool getOwnPropertyDescriptor(JSContext *cx, JSObject *wrapper, jsid id, js::PropertyDescriptor *desc, unsigned flags) MOZ_OVERRIDE;
    virtual bool getOwnPropertyNames(JSContext *cx, JSObject *wrapper, js::AutoIdVector &props) MOZ_OVERRIDE;
    virtual bool enumerate(JSContext *cx, JSObject *wrapper, js::AutoIdVector &props) MOZ_OVERRIDE;
    virtual bool keys(JSContext *cx, JSObject *wrapper, js::AutoIdVector &props) MOZ_OVERRIDE;
    virtual bool iterate(JSContext *cx, JSObject *proxy, unsigned flags, js::Value *vp) MOZ_OVERRIDE;
    virtual bool nativeCall(JSContext *cx, JS::IsAcceptableThis test, JS::NativeImpl impl,
                            JS::CallArgs args) MOZ_OVERRIDE;

    virtual bool enter(JSContext *cx, JSObject *wrapper, jsid id, js::Wrapper::Action act, bool *bp) MOZ_OVERRIDE;

    static FilteringWrapper singleton;
};

}

#endif /* __FilteringWrapper_h__ */
