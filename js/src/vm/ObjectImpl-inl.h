/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_ObjectImpl_inl_h
#define vm_ObjectImpl_inl_h

#include "vm/ObjectImpl.h"

#include "jscntxt.h"
#include "jsproxy.h"

#include "vm/ProxyObject.h"

/* static */ inline bool
js::ObjectImpl::isExtensible(ExclusiveContext *cx, js::Handle<ObjectImpl*> obj, bool *extensible)
{
    if (obj->asObjectPtr()->is<ProxyObject>()) {
        if (!cx->shouldBeJSContext())
            return false;
        HandleObject h =
            HandleObject::fromMarkedLocation(reinterpret_cast<JSObject* const*>(obj.address()));
        return Proxy::isExtensible(cx->asJSContext(), h, extensible);
    }

    *extensible = obj->nonProxyIsExtensible();
    return true;
}

#endif /* vm_ObjectImpl_inl_h */
