/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/ProxyObject.h"

#include "jscompartment.h"
#include "jsobjinlines.h"

using namespace js;

/* static */ ProxyObject*
ProxyObject::New(JSContext* cx, const BaseProxyHandler* handler, HandleValue priv, TaggedProto proto_,
                 const ProxyOptions& options)
{
    Rooted<TaggedProto> proto(cx, proto_);

    const Class* clasp = options.clasp();

    MOZ_ASSERT(isValidProxyClass(clasp));
    MOZ_ASSERT(clasp->shouldDelayMetadataBuilder());
    MOZ_ASSERT_IF(proto.isObject(), cx->compartment() == proto.toObject()->compartment());
    MOZ_ASSERT(clasp->hasFinalize());

    /*
     * Eagerly mark properties unknown for proxies, so we don't try to track
     * their properties and so that we don't need to walk the compartment if
     * their prototype changes later.  But don't do this for DOM proxies,
     * because we want to be able to keep track of them in typesets in useful
     * ways.
     */
    if (proto.isObject() && !options.singleton() && !clasp->isDOMClass()) {
        RootedObject protoObj(cx, proto.toObject());
        if (!JSObject::setNewGroupUnknown(cx, clasp, protoObj))
            return nullptr;
    }

    // Ensure that the wrapper has the same lifetime assumptions as the
    // wrappee. Prefer to allocate in the nursery, when possible.
    NewObjectKind newKind = NurseryAllocatedProxy;
    if (options.singleton()) {
        MOZ_ASSERT(priv.isGCThing() && priv.toGCThing()->isTenured());
        newKind = SingletonObject;
    } else if ((priv.isGCThing() && priv.toGCThing()->isTenured()) ||
               !handler->canNurseryAllocate() ||
               !handler->finalizeInBackground(priv))
    {
        newKind = TenuredObject;
    }

    gc::AllocKind allocKind = gc::GetGCObjectKind(clasp);
    if (handler->finalizeInBackground(priv))
        allocKind = GetBackgroundAllocKind(allocKind);

    AutoSetNewObjectMetadata metadata(cx);
    // Note: this will initialize the object's |data| to strange values, but we
    // will immediately overwrite those below.
    RootedObject obj(cx, NewObjectWithGivenTaggedProto(cx, clasp, proto, allocKind,
                                                       newKind));
    if (!obj)
        return nullptr;

    Rooted<ProxyObject*> proxy(cx, &obj->as<ProxyObject>());
    new (proxy->data.values) detail::ProxyValueArray;
    proxy->data.handler = handler;
    proxy->setCrossCompartmentPrivate(priv);

    /* Don't track types of properties of non-DOM and non-singleton proxies. */
    if (newKind != SingletonObject && !clasp->isDOMClass())
        MarkObjectGroupUnknownProperties(cx, proxy->group());

    return proxy;
}

gc::AllocKind
ProxyObject::allocKindForTenure() const
{
    gc::AllocKind allocKind = gc::GetGCObjectKind(group()->clasp());
    if (data.handler->finalizeInBackground(const_cast<ProxyObject*>(this)->private_()))
        allocKind = GetBackgroundAllocKind(allocKind);
    return allocKind;
}

/* static */ size_t
ProxyObject::objectMovedDuringMinorGC(TenuringTracer* trc, JSObject* dst, JSObject* src)
{
    ProxyObject& psrc = src->as<ProxyObject>();
    ProxyObject& pdst = dst->as<ProxyObject>();

    // We're about to sweep the nursery heap, so migrate the inline
    // ProxyValueArray to the malloc heap if they were nursery allocated.
    if (trc->runtime()->gc.nursery.isInside(psrc.data.values))
        pdst.data.values = js_new<detail::ProxyValueArray>(*psrc.data.values);
    else
        trc->runtime()->gc.nursery.removeMallocedBuffer(psrc.data.values);
    return sizeof(detail::ProxyValueArray);
}

void
ProxyObject::setCrossCompartmentPrivate(const Value& priv)
{
    *slotOfPrivate() = priv;
}

void
ProxyObject::setSameCompartmentPrivate(const Value& priv)
{
    MOZ_ASSERT(IsObjectValueInCompartment(priv, compartment()));
    *slotOfPrivate() = priv;
}

void
ProxyObject::nuke(const BaseProxyHandler* handler)
{
    setSameCompartmentPrivate(NullValue());
    for (size_t i = 0; i < detail::PROXY_EXTRA_SLOTS; i++)
        SetProxyExtra(this, i, NullValue());

    /* Restore the handler as requested after nuking. */
    setHandler(handler);
}

JS_FRIEND_API(void)
js::SetValueInProxy(Value* slot, const Value& value)
{
    // Slots in proxies are not GCPtrValues, so do a cast whenever assigning
    // values to them which might trigger a barrier.
    *reinterpret_cast<GCPtrValue*>(slot) = value;
}
