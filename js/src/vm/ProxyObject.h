/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_ProxyObject_h
#define vm_ProxyObject_h

#include "jsproxy.h"

#include "vm/NativeObject.h"

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
    static ProxyObject *New(JSContext *cx, const BaseProxyHandler *handler, HandleValue priv,
                            TaggedProto proto_, JSObject *parent_,
                            const ProxyOptions &options);

    const Value &private_() {
        return GetReservedSlot(this, PRIVATE_SLOT);
    }

    void initCrossCompartmentPrivate(HandleValue priv);
    void setSameCompartmentPrivate(const Value &priv);

    HeapSlot *slotOfPrivate() {
        return &fakeNativeGetReservedSlotRef(PRIVATE_SLOT);
    }

    JSObject *target() const {
        return const_cast<ProxyObject*>(this)->private_().toObjectOrNull();
    }

    const BaseProxyHandler *handler() const {
        return static_cast<const BaseProxyHandler*>(
            GetReservedSlot(const_cast<ProxyObject*>(this), HANDLER_SLOT).toPrivate());
    }

    void initHandler(const BaseProxyHandler *handler);

    void setHandler(const BaseProxyHandler *handler) {
        SetReservedSlot(this, HANDLER_SLOT, PrivateValue(const_cast<BaseProxyHandler*>(handler)));
    }

    static size_t offsetOfHandler() {
        // FIXME Bug 1073842: this is temporary until non-native objects can
        // access non-slot storage.
        return NativeObject::getFixedSlotOffset(HANDLER_SLOT);
    }

    const Value &extra(size_t n) const {
        MOZ_ASSERT(n == 0 || n == 1);
        return GetReservedSlot(const_cast<ProxyObject*>(this), EXTRA_SLOT + n);
    }

    void setExtra(size_t n, const Value &extra) {
        MOZ_ASSERT(n == 0 || n == 1);
        SetReservedSlot(this, EXTRA_SLOT + n, extra);
    }

  private:
    HeapSlot *slotOfExtra(size_t n) {
        MOZ_ASSERT(n == 0 || n == 1);
        return &fakeNativeGetReservedSlotRef(EXTRA_SLOT + n);
    }

    HeapSlot *slotOfClassSpecific(size_t n) {
        MOZ_ASSERT(n >= PROXY_MINIMUM_SLOTS);
        MOZ_ASSERT(n < JSCLASS_RESERVED_SLOTS(getClass()));
        return &fakeNativeGetReservedSlotRef(n);
    }

    static bool isValidProxyClass(const Class *clasp) {
        // Since we can take classes from the outside, make sure that they
        // are "sane". They have to quack enough like proxies for us to belive
        // they should be treated as such.

        // proxy_Trace is just a trivial wrapper around ProxyObject::trace for
        // friend api exposure.

        // Proxy classes are not allowed to have call or construct hooks directly. Their
        // callability is instead decided by handler()->isCallable().
        return clasp->isProxy() &&
               (clasp->flags & JSCLASS_IMPLEMENTS_BARRIERS) &&
               clasp->trace == proxy_Trace &&
               !clasp->call && !clasp->construct &&
               JSCLASS_RESERVED_SLOTS(clasp) >= PROXY_MINIMUM_SLOTS;
    }

  public:
    static unsigned grayLinkSlot(JSObject *obj);

    void renew(JSContext *cx, const BaseProxyHandler *handler, Value priv);

    static void trace(JSTracer *trc, JSObject *obj);

    void nuke(const BaseProxyHandler *handler);

    static const Class class_;
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

#endif /* vm_ProxyObject_h */
