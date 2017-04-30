/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/ProxyObject.h"

#include "jscompartment.h"

#include "proxy/DeadObjectProxy.h"

#include "jsobjinlines.h"

using namespace js;

static gc::AllocKind
GetProxyGCObjectKind(const Class* clasp, const BaseProxyHandler* handler, const Value& priv)
{
    MOZ_ASSERT(clasp->isProxy());

    uint32_t nreserved = JSCLASS_RESERVED_SLOTS(clasp);

    // For now assert each Proxy Class has at least 1 reserved slot. This is
    // not a hard requirement, but helps catch Classes that need an explicit
    // JSCLASS_HAS_RESERVED_SLOTS since bug 1360523.
    MOZ_ASSERT(nreserved > 0);

    MOZ_ASSERT(js::detail::ProxyValueArray::sizeOf(nreserved) % sizeof(Value) == 0,
               "ProxyValueArray must be a multiple of Value");

    uint32_t nslots = js::detail::ProxyValueArray::sizeOf(nreserved) / sizeof(Value);
    MOZ_ASSERT(nslots <= NativeObject::MAX_FIXED_SLOTS);

    gc::AllocKind kind = gc::GetGCObjectKind(nslots);
    if (handler->finalizeInBackground(priv))
        kind = GetBackgroundAllocKind(kind);

    return kind;
}

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
        MOZ_ASSERT(priv.isNull() || (priv.isGCThing() && priv.toGCThing()->isTenured()));
        newKind = SingletonObject;
    } else if ((priv.isGCThing() && priv.toGCThing()->isTenured()) ||
               !handler->canNurseryAllocate() ||
               !handler->finalizeInBackground(priv))
    {
        newKind = TenuredObject;
    }

    gc::AllocKind allocKind = GetProxyGCObjectKind(clasp, handler, priv);

    AutoSetNewObjectMetadata metadata(cx);
    // Note: this will initialize the object's |data| to strange values, but we
    // will immediately overwrite those below.
    ProxyObject* proxy;
    JS_TRY_VAR_OR_RETURN_NULL(cx, proxy, create(cx, clasp, proto, allocKind, newKind));

    proxy->setInlineValueArray();

    detail::ProxyValueArray* values = detail::GetProxyDataLayout(proxy)->values();
    values->init(proxy->numReservedSlots());

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
    MOZ_ASSERT(usingInlineValueArray());
    Value priv = const_cast<ProxyObject*>(this)->private_();
    return GetProxyGCObjectKind(getClass(), data.handler, priv);
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
ProxyObject::nuke()
{
    // When nuking scripted proxies, isCallable and isConstructor values for
    // the proxy needs to be preserved. Do this before clearing the target.
    uint32_t callable = handler()->isCallable(this);
    uint32_t constructor = handler()->isConstructor(this);

    // Clear the target reference.
    setSameCompartmentPrivate(NullValue());

    // Update the handler to make this a DeadObjectProxy.
    if (callable) {
        if (constructor)
            setHandler(DeadObjectProxy<DeadProxyIsCallableIsConstructor>::singleton());
        else
            setHandler(DeadObjectProxy<DeadProxyIsCallableNotConstructor>::singleton());
    } else {
        if (constructor)
            setHandler(DeadObjectProxy<DeadProxyNotCallableIsConstructor>::singleton());
        else
            setHandler(DeadObjectProxy<DeadProxyNotCallableNotConstructor>::singleton());
    }

    // The proxy's reserved slots are not cleared and will continue to be
    // traced. This avoids the possibility of triggering write barriers while
    // nuking proxies in dead compartments which could otherwise cause those
    // compartments to be kept alive. Note that these are slots cannot hold
    // cross compartment pointers, so this cannot cause the target compartment
    // to leak.
}

/* static */ JS::Result<ProxyObject*, JS::OOM&>
ProxyObject::create(JSContext* cx, const Class* clasp, Handle<TaggedProto> proto,
                    gc::AllocKind allocKind, NewObjectKind newKind)
{
    MOZ_ASSERT(clasp->isProxy());

    JSCompartment* comp = cx->compartment();
    RootedObjectGroup group(cx);
    RootedShape shape(cx);

    // Try to look up the group and shape in the NewProxyCache.
    if (!comp->newProxyCache.lookup(clasp, proto, group.address(), shape.address())) {
        group = ObjectGroup::defaultNewGroup(cx, clasp, proto, nullptr);
        if (!group)
            return cx->alreadyReportedOOM();

        shape = EmptyShape::getInitialShape(cx, clasp, proto, /* nfixed = */ 0);
        if (!shape)
            return cx->alreadyReportedOOM();

        comp->newProxyCache.add(group, shape);
    }

    gc::InitialHeap heap = GetInitialHeap(newKind, clasp);
    debugCheckNewObject(group, shape, allocKind, heap);

    JSObject* obj = js::Allocate<JSObject>(cx, allocKind, /* numDynamicSlots = */ 0, heap, clasp);
    if (!obj)
        return cx->alreadyReportedOOM();

    ProxyObject* pobj = static_cast<ProxyObject*>(obj);
    pobj->group_.init(group);
    pobj->initShape(shape);

    MOZ_ASSERT(clasp->shouldDelayMetadataBuilder());
    cx->compartment()->setObjectPendingMetadata(cx, pobj);

    js::gc::TraceCreateObject(pobj);

    if (newKind == SingletonObject) {
        Rooted<ProxyObject*> pobjRoot(cx, pobj);
        if (!JSObject::setSingleton(cx, pobjRoot))
            return cx->alreadyReportedOOM();
        pobj = pobjRoot;
    }

    return pobj;
}

JS_FRIEND_API(void)
js::SetValueInProxy(Value* slot, const Value& value)
{
    // Slots in proxies are not GCPtrValues, so do a cast whenever assigning
    // values to them which might trigger a barrier.
    *reinterpret_cast<GCPtrValue*>(slot) = value;
}
