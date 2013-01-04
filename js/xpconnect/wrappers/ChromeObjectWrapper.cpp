#include "ChromeObjectWrapper.h"

namespace xpc {

// When creating wrappers for chrome objects in content, we detect if the
// prototype of the wrapped chrome object is a prototype for a standard class
// (like Array.prototype). If it is, we use the corresponding standard prototype
// from the wrapper's scope, rather than the wrapped standard prototype
// from the wrappee's scope.
//
// One of the reasons for doing this is to allow standard operations like
// chromeArray.forEach(..) to Just Work without explicitly listing them in
// __exposedProps__. Since proxies don't automatically inherit behavior from
// their prototype, we have to instrument the traps to do this manually.
ChromeObjectWrapper ChromeObjectWrapper::singleton;

static bool
PropIsFromStandardPrototype(JSContext *cx, JSPropertyDescriptor *desc)
{
    MOZ_ASSERT(desc->obj);
    JSObject *unwrapped = js::UnwrapObject(desc->obj);
    JSAutoCompartment ac(cx, unwrapped);
    return JS_IdentifyClassPrototype(cx, unwrapped) != JSProto_Null;
}

bool
ChromeObjectWrapper::getPropertyDescriptor(JSContext *cx, JSObject *wrapper,
                                           jsid id, js::PropertyDescriptor *desc,
                                           unsigned flags)
{
    // First, try the lookup on the base wrapper. This can throw for various
    // reasons, including sets (gets fail silently). There's nothing we can really
    // do for sets, so we can conveniently propagate any exception we hit here.
    desc->obj = NULL;
    if (!ChromeObjectWrapperBase::getPropertyDescriptor(cx, wrapper, id,
                                                        desc, flags)) {
        return false;
    }

    // If the property is something that can be found on a standard prototype,
    // prefer the one we'll get via the prototype chain in the content
    // compartment.
    if (desc->obj && PropIsFromStandardPrototype(cx, desc))
        desc->obj = NULL;

    // If we found something or have no proto, we're done.
    JSObject *wrapperProto;
    if (!JS_GetPrototype(cx, wrapper, &wrapperProto))
      return false;
    if (desc->obj || !wrapperProto)
        return true;

    // If not, try doing the lookup on the prototype.
    MOZ_ASSERT(js::IsObjectInContextCompartment(wrapper, cx));
    return JS_GetPropertyDescriptorById(cx, wrapperProto, id, 0, desc);
}

bool
ChromeObjectWrapper::has(JSContext *cx, JSObject *wrapper, jsid id, bool *bp)
{
    // Try the lookup on the base wrapper.
    if (!ChromeObjectWrapperBase::has(cx, wrapper, id, bp))
        return false;

    // If we found something or have no prototype, we're done.
    JSObject *wrapperProto;
    if (!JS_GetPrototype(cx, wrapper, &wrapperProto))
        return false;
    if (*bp || !wrapperProto)
        return true;

    // Try the prototype if that failed.
    MOZ_ASSERT(js::IsObjectInContextCompartment(wrapper, cx));
    JSPropertyDescriptor desc;
    if (!JS_GetPropertyDescriptorById(cx, wrapperProto, id, 0, &desc))
        return false;
    *bp = !!desc.obj;
    return true;
}

bool
ChromeObjectWrapper::get(JSContext *cx, JSObject *wrapper, JSObject *receiver,
                         jsid id, js::Value *vp)
{
    // Start with a call to getPropertyDescriptor. We unfortunately need to do
    // this because the call signature of ::get doesn't give us any way to
    // determine the object upon which the property was found.
    JSPropertyDescriptor desc;
    if (!ChromeObjectWrapperBase::getPropertyDescriptor(cx, wrapper, id, &desc,
                                                        0)) {
        return false;
    }

    // Only call through to the get trap on the underlying object if we'll find
    // something, and if what we'll find is not on a standard prototype.
    vp->setUndefined();
    if (desc.obj && !PropIsFromStandardPrototype(cx, &desc)) {
        // Call the get trap.
        if (!ChromeObjectWrapperBase::get(cx, wrapper, receiver, id, vp))
            return false;
        // If we found something, we're done.
        if (!vp->isUndefined())
            return true;
    }

    // If we have no proto, we're done.
    JSObject *wrapperProto;
    if (!JS_GetPrototype(cx, wrapper, &wrapperProto))
        return false;
    if (!wrapperProto)
        return true;

    // Try the prototype.
    MOZ_ASSERT(js::IsObjectInContextCompartment(wrapper, cx));
    return js::GetGeneric(cx, wrapperProto, receiver, id, vp);
}

}
