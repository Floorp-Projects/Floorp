/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef xpc_DocGroupValidationWrapper_h
#define xpc_DocGroupValidationWrapper_h

#include "mozilla/Attributes.h"

#include "jswrapper.h"
#include "WaiveXrayWrapper.h"
#include "XrayWrapper.h"

namespace xpc {

template<typename Base>
class DocGroupValidationWrapper : public Base {
  public:
    explicit constexpr DocGroupValidationWrapper(unsigned flags) : Base(flags) { }

    /* Standard internal methods. */
    virtual bool getOwnPropertyDescriptor(JSContext* cx, HandleObject wrapper, HandleId id,
                                          MutableHandle<PropertyDescriptor> desc) const override;
    virtual bool defineProperty(JSContext* cx, HandleObject wrapper, HandleId id,
                                Handle<PropertyDescriptor> desc,
                                ObjectOpResult& result) const override;
    virtual bool ownPropertyKeys(JSContext* cx, HandleObject wrapper,
                                 AutoIdVector& props) const override;
    virtual bool delete_(JSContext* cx, HandleObject wrapper, HandleId id,
                         ObjectOpResult& result) const override;
    virtual bool enumerate(JSContext* cx, HandleObject wrapper, MutableHandleObject objp) const override;
    virtual bool getPrototype(JSContext* cx, HandleObject proxy,
                              MutableHandleObject protop) const override;
    virtual bool setPrototype(JSContext* cx, HandleObject proxy, HandleObject proto,
                              ObjectOpResult& result) const override;

    virtual bool getPrototypeIfOrdinary(JSContext* cx, HandleObject proxy, bool* isOrdinary,
                                        MutableHandleObject protop) const override;
    virtual bool setImmutablePrototype(JSContext* cx, HandleObject proxy,
                                       bool* succeeded) const override;
    virtual bool preventExtensions(JSContext* cx, HandleObject wrapper,
                                   ObjectOpResult& result) const override;
    virtual bool isExtensible(JSContext* cx, HandleObject wrapper, bool* extensible) const override;
    virtual bool has(JSContext* cx, HandleObject wrapper, HandleId id, bool* bp) const override;
    virtual bool get(JSContext* cx, HandleObject wrapper, HandleValue receiver,
                     HandleId id, MutableHandleValue vp) const override;
    virtual bool set(JSContext* cx, HandleObject wrapper, HandleId id, HandleValue v,
                     HandleValue receiver, ObjectOpResult& result) const override;
    virtual bool call(JSContext* cx, HandleObject wrapper, const CallArgs& args) const override;
    virtual bool construct(JSContext* cx, HandleObject wrapper, const CallArgs& args) const override;

    /* SpiderMonkey extensions. */
    virtual bool getPropertyDescriptor(JSContext* cx, HandleObject wrapper, HandleId id,
                                       MutableHandle<PropertyDescriptor> desc) const override;
    virtual bool hasOwn(JSContext* cx, HandleObject wrapper, HandleId id, bool* bp) const override;
    virtual bool getOwnEnumerablePropertyKeys(JSContext* cx, HandleObject wrapper,
                                              AutoIdVector& props) const override;
    virtual bool hasInstance(JSContext* cx, HandleObject wrapper, MutableHandleValue v,
                             bool* bp) const override;
    virtual const char* className(JSContext* cx, HandleObject proxy) const override;
    virtual JSString* fun_toString(JSContext* cx, HandleObject wrapper,
                                   unsigned indent) const override;
    virtual bool regexp_toShared(JSContext* cx, HandleObject proxy, MutableHandle<RegExpShared*> g) const override;
    virtual bool boxedValue_unbox(JSContext* cx, HandleObject proxy, MutableHandleValue vp) const override;

    static const DocGroupValidationWrapper singleton;
};

#define DECLARE_SINGLETON(Base) \
    extern template class DocGroupValidationWrapper<Base>

DECLARE_SINGLETON(CrossCompartmentWrapper);
DECLARE_SINGLETON(PermissiveXrayXPCWN);
DECLARE_SINGLETON(PermissiveXrayDOM);
DECLARE_SINGLETON(PermissiveXrayJS);

DECLARE_SINGLETON(AddonWrapper<CrossCompartmentWrapper>);
DECLARE_SINGLETON(AddonWrapper<PermissiveXrayXPCWN>);
DECLARE_SINGLETON(AddonWrapper<PermissiveXrayDOM>);

DECLARE_SINGLETON(WaiveXrayWrapper);

} // namespace xpc

#endif // xpc_DocGroupValidationWrapper_h
