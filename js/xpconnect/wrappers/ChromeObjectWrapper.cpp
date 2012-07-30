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

bool
ChromeObjectWrapper::getPropertyDescriptor(JSContext *cx, JSObject *wrapper,
                                           jsid id, bool set,
                                           js::PropertyDescriptor *desc)
{
    // First, try the lookup on the base wrapper. This can throw for various
    // reasons, including sets (gets fail silently). There's nothing we can really
    // do for sets, so we can conveniently propagate any exception we hit here.
    desc->obj = NULL;
    if (!ChromeObjectWrapperBase::getPropertyDescriptor(cx, wrapper, id,
                                                        set, desc)) {
        return false;
    }

    // If we found something, were doing a set, or have no proto, we're done.
    JSObject *wrapperProto = JS_GetPrototype(wrapper);
    if (desc->obj || set || !wrapperProto)
        return true;

    // If not, try doing the lookup on the prototype.
    JSAutoEnterCompartment ac;
    return ac.enter(cx, wrapper) &&
           JS_GetPropertyDescriptorById(cx, wrapperProto, id, 0, desc);
}

bool
ChromeObjectWrapper::has(JSContext *cx, JSObject *wrapper, jsid id, bool *bp)
{
    // Try the lookup on the base wrapper.
    if (!ChromeObjectWrapperBase::has(cx, wrapper, id, bp))
        return false;

    // If we found something or have no prototype, we're done.
    JSObject *wrapperProto = JS_GetPrototype(wrapper);
    if (*bp || !wrapperProto)
        return true;

    // Try the prototype if that failed.
    JSAutoEnterCompartment ac;
    JSPropertyDescriptor desc;
    if (!ac.enter(cx, wrapper) ||
        !JS_GetPropertyDescriptorById(cx, wrapperProto, id, 0, &desc))
    {
        return false;
    }
    *bp = !!desc.obj;
    return true;
}

bool
ChromeObjectWrapper::get(JSContext *cx, JSObject *wrapper, JSObject *receiver,
                         jsid id, js::Value *vp)
{
    // Try the lookup on the base wrapper.
    if (!ChromeObjectWrapperBase::get(cx, wrapper, receiver, id, vp))
        return false;

    // If we found something or have no proto, we're done.
    JSObject *wrapperProto = JS_GetPrototype(wrapper);
    if (!vp->isUndefined() || !wrapperProto)
        return true;

    // Try the prototype.
    JSAutoEnterCompartment ac;
    if (!ac.enter(cx, wrapper))
        return false;
    return js::GetGeneric(cx, wrapperProto, receiver, id, vp);
}

}
