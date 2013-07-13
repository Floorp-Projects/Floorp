/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_ProxyObject_h
#define vm_ProxyObject_h

#include "jsobj.h"
#include "jsproxy.h"

namespace js {

// This is the base class for the various kinds of proxy objects.  It's never
// instantiated.
class ProxyObject : public JSObject
{
    // These are just local renamings of the slot constants that are part of
    // the API in jsproxy.h.
    static const uint32_t PRIVATE_SLOT = PROXY_PRIVATE_SLOT;
    static const uint32_t HANDLER_SLOT = PROXY_HANDLER_SLOT;
    static const uint32_t EXTRA_SLOT   = PROXY_EXTRA_SLOT;

  public:
    static ProxyObject *New(JSContext *cx, BaseProxyHandler *handler, HandleValue priv,
                            TaggedProto proto_, JSObject *parent_, ProxyCallable callable);

    const Value &private_() {
        return GetReservedSlot(this, PRIVATE_SLOT);
    }

    void initCrossCompartmentPrivate(HandleValue priv);

    HeapSlot *slotOfPrivate() {
        return &getReservedSlotRef(PRIVATE_SLOT);
    }

    JSObject *target() const {
        return const_cast<ProxyObject*>(this)->private_().toObjectOrNull();
    }

    BaseProxyHandler *handler() {
        return static_cast<BaseProxyHandler*>(GetReservedSlot(this, HANDLER_SLOT).toPrivate());
    }

    void initHandler(BaseProxyHandler *handler);

    void setHandler(BaseProxyHandler *handler) {
        SetReservedSlot(this, HANDLER_SLOT, PrivateValue(handler));
    }

    static size_t offsetOfHandler() {
        return getFixedSlotOffset(HANDLER_SLOT);
    }

    const Value &extra(size_t n) const {
        JS_ASSERT(n == 0 || n == 1);
        return GetReservedSlot(const_cast<ProxyObject*>(this), EXTRA_SLOT + n);
    }

    void setExtra(size_t n, const Value &extra) {
        JS_ASSERT(n == 0 || n == 1);
        SetReservedSlot(this, EXTRA_SLOT + n, extra);
    }

  private:
    HeapSlot *slotOfExtra(size_t n) {
        JS_ASSERT(n == 0 || n == 1);
        return &getReservedSlotRef(EXTRA_SLOT + n);
    }

  public:
    static unsigned grayLinkSlot(JSObject *obj);

    void renew(JSContext *cx, BaseProxyHandler *handler, Value priv);

    static void trace(JSTracer *trc, JSObject *obj);

    void nuke(BaseProxyHandler *handler);
};

class FunctionProxyObject : public ProxyObject
{
    static const uint32_t CALL_SLOT      = 4;
    static const uint32_t CONSTRUCT_SLOT = 5;

  public:
    static Class class_;

    static FunctionProxyObject *New(JSContext *cx, BaseProxyHandler *handler, HandleValue priv,
                                    JSObject *proto, JSObject *parent, JSObject *call,
                                    JSObject *construct);

    HeapSlot &call() { return getSlotRef(CALL_SLOT); }

    inline HeapSlot &construct();
    inline Value constructOrUndefined() const;

    void nukeExtra();
};

class ObjectProxyObject : public ProxyObject
{
  public:
    static Class class_;
};

class OuterWindowProxyObject : public ObjectProxyObject
{
  public:
    static Class class_;
};

} // namespace js

// Note: the following |JSObject::is<T>| methods are implemented in terms of
// the Is*Proxy() friend API functions to ensure the implementations are tied
// together.  The exception is |JSObject::is<js::OuterWindowProxyObject>()
// const|, which uses the standard template definition, because there is no
// IsOuterWindowProxy() function in the friend API.

template<>
inline bool
JSObject::is<js::ProxyObject>() const
{
    return js::IsProxy(const_cast<JSObject*>(this));
}

template<>
inline bool
JSObject::is<js::FunctionProxyObject>() const
{
    return js::IsFunctionProxy(const_cast<JSObject*>(this));
}

// WARNING: This function succeeds for ObjectProxyObject *and*
// OuterWindowProxyObject (which is a sub-class).  If you want a test that only
// succeeds for ObjectProxyObject, use |hasClass(&ObjectProxyObject::class_)|.
template<>
inline bool
JSObject::is<js::ObjectProxyObject>() const
{
    return js::IsObjectProxy(const_cast<JSObject*>(this));
}

#endif /* vm_ProxyObject_h */
