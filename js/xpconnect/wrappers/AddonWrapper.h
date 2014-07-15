/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AddonWrapper_h
#define AddonWrapper_h

#include "mozilla/Attributes.h"

#include "jswrapper.h"

namespace xpc {

bool
Interpose(JSContext *cx, HandleObject target, const nsIID *iid, HandleId id,
          MutableHandle<JSPropertyDescriptor> descriptor);

template<typename Base>
class AddonWrapper : public Base {
  public:
    AddonWrapper(unsigned flags);
    virtual ~AddonWrapper();

    virtual bool getPropertyDescriptor(JSContext *cx, JS::Handle<JSObject*> wrapper,
                                       JS::Handle<jsid> id,
                                       JS::MutableHandle<JSPropertyDescriptor> desc) const MOZ_OVERRIDE;
    virtual bool getOwnPropertyDescriptor(JSContext *cx, JS::Handle<JSObject*> wrapper,
                                          JS::Handle<jsid> id,
                                          JS::MutableHandle<JSPropertyDescriptor> desc) const MOZ_OVERRIDE;

    virtual bool get(JSContext *cx, JS::Handle<JSObject*> wrapper, JS::Handle<JSObject*> receiver,
                     JS::Handle<jsid> id, JS::MutableHandle<JS::Value> vp) const MOZ_OVERRIDE;
    virtual bool set(JSContext *cx, JS::HandleObject wrapper, JS::HandleObject receiver,
                     JS::HandleId id, bool strict, JS::MutableHandleValue vp) const MOZ_OVERRIDE;

    virtual bool defineProperty(JSContext *cx, HandleObject proxy, HandleId id,
                                MutableHandle<JSPropertyDescriptor> desc) const MOZ_OVERRIDE;
    virtual bool delete_(JSContext *cx, HandleObject proxy, HandleId id, bool *bp) const MOZ_OVERRIDE;

    static AddonWrapper singleton;
};

} // namespace xpc

#endif // AddonWrapper_h
