/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AddonWrapper_h
#define AddonWrapper_h

#include "mozilla/Attributes.h"

#include "nsID.h"

#include "jswrapper.h"

namespace xpc {

bool
InterposeProperty(JSContext* cx, JS::HandleObject target, const nsIID* iid, JS::HandleId id,
                  JS::MutableHandle<JSPropertyDescriptor> descriptor);

bool
InterposeCall(JSContext* cx, JS::HandleObject wrapper,
              const JS::CallArgs& args, bool& done);

template<typename Base>
class AddonWrapper : public Base {
  public:
    explicit MOZ_CONSTEXPR AddonWrapper(unsigned flags) : Base(flags) { }

    virtual bool getOwnPropertyDescriptor(JSContext* cx, JS::Handle<JSObject*> wrapper,
                                          JS::Handle<jsid> id,
                                          JS::MutableHandle<JSPropertyDescriptor> desc) const override;
    virtual bool defineProperty(JSContext* cx, JS::HandleObject proxy, JS::HandleId id,
                                JS::Handle<JSPropertyDescriptor> desc,
                                JS::ObjectOpResult& result) const override;
    virtual bool delete_(JSContext* cx, JS::HandleObject proxy, JS::HandleId id,
                         JS::ObjectOpResult& result) const override;
    virtual bool get(JSContext* cx, JS::Handle<JSObject*> wrapper, JS::Handle<JSObject*> receiver,
                     JS::Handle<jsid> id, JS::MutableHandle<JS::Value> vp) const override;
    virtual bool set(JSContext* cx, JS::HandleObject wrapper, JS::HandleId id, JS::HandleValue v,
                     JS::HandleValue receiver, JS::ObjectOpResult& result) const override;
    virtual bool call(JSContext* cx, JS::Handle<JSObject*> wrapper,
                      const JS::CallArgs& args) const override;

    virtual bool getPropertyDescriptor(JSContext* cx, JS::Handle<JSObject*> wrapper,
                                       JS::Handle<jsid> id,
                                       JS::MutableHandle<JSPropertyDescriptor> desc) const override;

    static const AddonWrapper singleton;
};

} // namespace xpc

#endif // AddonWrapper_h
