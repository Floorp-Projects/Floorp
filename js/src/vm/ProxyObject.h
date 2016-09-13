/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_ProxyObject_h
#define vm_ProxyObject_h

#include "js/Proxy.h"
#include "vm/ShapedObject.h"

namespace js {

/**
 * This is the base class for the various kinds of proxy objects.  It's never
 * instantiated.
 *
 * Proxy objects use ShapedObject::shape_ primarily to record flags.  Property
 * information, &c. is all dynamically computed.
 */
class ProxyObject : public ShapedObject
{
    // GetProxyDataLayout computes the address of this field.
    detail::ProxyDataLayout data;

    void static_asserts() {
        static_assert(sizeof(ProxyObject) == sizeof(JSObject_Slots0),
                      "proxy object size must match GC thing size");
        static_assert(offsetof(ProxyObject, data) == detail::ProxyDataOffset,
                      "proxy object layout must match shadow interface");
    }

  public:
    static ProxyObject* New(JSContext* cx, const BaseProxyHandler* handler, HandleValue priv,
                            TaggedProto proto_, const ProxyOptions& options);

    const Value& private_() {
        return GetProxyPrivate(this);
    }

    void setCrossCompartmentPrivate(const Value& priv);
    void setSameCompartmentPrivate(const Value& priv);

    GCPtrValue* slotOfPrivate() {
        return reinterpret_cast<GCPtrValue*>(&detail::GetProxyDataLayout(this)->values->privateSlot);
    }

    JSObject* target() const {
        return const_cast<ProxyObject*>(this)->private_().toObjectOrNull();
    }

    const BaseProxyHandler* handler() const {
        return GetProxyHandler(const_cast<ProxyObject*>(this));
    }

    void setHandler(const BaseProxyHandler* handler) {
        SetProxyHandler(this, handler);
    }

    static size_t offsetOfValues() {
        return offsetof(ProxyObject, data.values);
    }
    static size_t offsetOfHandler() {
        return offsetof(ProxyObject, data.handler);
    }
    static size_t offsetOfExtraSlotInValues(size_t slot) {
        MOZ_ASSERT(slot < detail::PROXY_EXTRA_SLOTS);
        return offsetof(detail::ProxyValueArray, extraSlots) + slot * sizeof(Value);
    }

    const Value& extra(size_t n) const {
        return GetProxyExtra(const_cast<ProxyObject*>(this), n);
    }

    void setExtra(size_t n, const Value& extra) {
        SetProxyExtra(this, n, extra);
    }

    gc::AllocKind allocKindForTenure() const;
    static size_t objectMovedDuringMinorGC(TenuringTracer* trc, JSObject* dst, JSObject* src);

  private:
    GCPtrValue* slotOfExtra(size_t n) {
        MOZ_ASSERT(n < detail::PROXY_EXTRA_SLOTS);
        return reinterpret_cast<GCPtrValue*>(&detail::GetProxyDataLayout(this)->values->extraSlots[n]);
    }

    static bool isValidProxyClass(const Class* clasp) {
        // Since we can take classes from the outside, make sure that they
        // are "sane". They have to quack enough like proxies for us to belive
        // they should be treated as such.

        // proxy_Trace is just a trivial wrapper around ProxyObject::trace for
        // friend api exposure.

        // Proxy classes are not allowed to have call or construct hooks directly. Their
        // callability is instead decided by handler()->isCallable().
        return clasp->isProxy() &&
               clasp->isTrace(proxy_Trace) &&
               !clasp->getCall() && !clasp->getConstruct();
    }

  public:
    static unsigned grayLinkExtraSlot(JSObject* obj);

    void renew(JSContext* cx, const BaseProxyHandler* handler, Value priv);

    static void trace(JSTracer* trc, JSObject* obj);

    void nuke(const BaseProxyHandler* handler);

    // There is no class_ member to force specialization of JSObject::is<T>().
    // The implementation in JSObject is incorrect for proxies since it doesn't
    // take account of the handler type.
    static const Class proxyClass;
};

inline bool
IsProxyClass(const Class* clasp)
{
    return clasp->isProxy();
}

bool IsDerivedProxyObject(const JSObject* obj, const js::BaseProxyHandler* handler);

} // namespace js

template<>
inline bool
JSObject::is<js::ProxyObject>() const
{
    // Note: this method is implemented in terms of the IsProxy() friend API
    // functions to ensure the implementations are tied together.
    // Note 2: this specialization isn't used for subclasses of ProxyObject
    // which must supply their own implementation.
    return js::IsProxy(const_cast<JSObject*>(this));
}

inline bool
js::IsDerivedProxyObject(const JSObject* obj, const js::BaseProxyHandler* handler) {
    return obj->is<js::ProxyObject>() && obj->as<js::ProxyObject>().handler() == handler;
}

#endif /* vm_ProxyObject_h */
