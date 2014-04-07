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
#include "vm/TypedArrayObject.h"

namespace js {

/* static */ inline bool
ObjectImpl::isExtensible(ExclusiveContext *cx, Handle<ObjectImpl*> obj, bool *extensible)
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

inline bool
ClassCanHaveFixedData(const Class *clasp)
{
    // Normally, the number of fixed slots given an object is the maximum
    // permitted for its size class. For array buffers and typed arrays we only
    // use enough to cover the class reserved slots, so that the remaining
    // space in the object's allocation is available for the buffer's data.
    return clasp == &ArrayBufferObject::class_ || IsTypedArrayClass(clasp);
}

inline void *
ObjectImpl::fixedData(size_t nslots) const
{
    JS_ASSERT(ClassCanHaveFixedData(getClass()));
    JS_ASSERT(nslots == numFixedSlots() + (hasPrivate() ? 1 : 0));
    return &fixedSlots()[nslots];
}

} // namespace js

#endif /* vm_ObjectImpl_inl_h */
