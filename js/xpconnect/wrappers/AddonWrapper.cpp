/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AddonWrapper.h"
#include "WrapperFactory.h"
#include "XrayWrapper.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "nsIAddonInterposition.h"
#include "xpcprivate.h"
#include "mozilla/dom/BindingUtils.h"
#include "nsGlobalWindow.h"

#include "nsID.h"

using namespace js;
using namespace JS;

namespace xpc {

bool
InterposeProperty(JSContext* cx, HandleObject target, const nsIID* iid, HandleId id,
                  MutableHandle<JSPropertyDescriptor> descriptor)
{
    // We only want to do interpostion on DOM instances and
    // wrapped natives.
    RootedObject unwrapped(cx, UncheckedUnwrap(target));
    const js::Class* clasp = js::GetObjectClass(unwrapped);
    bool isCPOW = jsipc::IsWrappedCPOW(unwrapped);
    if (!mozilla::dom::IsDOMClass(clasp) &&
        !IS_WN_CLASS(clasp) &&
        !IS_PROTO_CLASS(clasp) &&
        clasp != &OuterWindowProxyClass &&
        !isCPOW) {
        return true;
    }

    XPCWrappedNativeScope* scope = ObjectScope(CurrentGlobalOrNull(cx));
    MOZ_ASSERT(scope->HasInterposition());

    nsCOMPtr<nsIAddonInterposition> interp = scope->GetInterposition();
    InterpositionWhitelist* wl = XPCWrappedNativeScope::GetInterpositionWhitelist(interp);
    // We do InterposeProperty only if the id is on the whitelist of the interpostion
    // or if the target is a CPOW.
    if ((!wl || !wl->has(JSID_BITS(id.get()))) && !isCPOW)
        return true;

    JSAddonId* addonId = AddonIdOfObject(target);
    RootedValue addonIdValue(cx, StringValue(StringOfAddonId(addonId)));
    RootedValue prop(cx, IdToValue(id));
    RootedValue targetValue(cx, ObjectValue(*target));
    RootedValue descriptorVal(cx);
    nsresult rv = interp->InterposeProperty(addonIdValue, targetValue,
                                            iid, prop, &descriptorVal);
    if (NS_FAILED(rv)) {
        xpc::Throw(cx, rv);
        return false;
    }

    if (!descriptorVal.isObject())
        return true;

    // We need to be careful parsing descriptorVal. |cx| is in the compartment
    // of the add-on and the descriptor is in the compartment of the
    // interposition. We could wrap the descriptor in the add-on's compartment
    // and then parse it. However, parsing the descriptor fetches properties
    // from it, and we would try to interpose on those property accesses. So
    // instead we parse in the interposition's compartment and then wrap the
    // descriptor.

    {
        JSAutoCompartment ac(cx, &descriptorVal.toObject());
        if (!JS::ObjectToCompletePropertyDescriptor(cx, target, descriptorVal, descriptor))
            return false;
    }

    // Always make the property non-configurable regardless of what the
    // interposition wants.
    descriptor.setAttributes(descriptor.attributes() | JSPROP_PERMANENT);

    if (!JS_WrapPropertyDescriptor(cx, descriptor))
        return false;

    return true;
}

bool
InterposeCall(JSContext* cx, JS::HandleObject target, const JS::CallArgs& args, bool* done)
{
    *done = false;
    XPCWrappedNativeScope* scope = ObjectScope(CurrentGlobalOrNull(cx));
    MOZ_ASSERT(scope->HasInterposition());

    nsCOMPtr<nsIAddonInterposition> interp = scope->GetInterposition();

    RootedObject unwrappedTarget(cx, UncheckedUnwrap(target));
    XPCWrappedNativeScope* targetScope = ObjectScope(unwrappedTarget);
    bool hasInterpostion = targetScope->HasCallInterposition();

    if (!hasInterpostion)
        return true;

    // If there is a call interpostion, we don't want to propogate the
    // call to Base:
    *done = true; 

    JSAddonId* addonId = AddonIdOfObject(target);
    RootedValue addonIdValue(cx, StringValue(StringOfAddonId(addonId)));
    RootedValue targetValue(cx, ObjectValue(*target));
    RootedValue thisValue(cx, args.thisv());
    RootedObject argsArray(cx, ConvertArgsToArray(cx, args));
    if (!argsArray)
        return false;

    RootedValue argsVal(cx, ObjectValue(*argsArray));
    RootedValue returnVal(cx);

    nsresult rv = interp->InterposeCall(addonIdValue, targetValue,
                                        thisValue, argsVal, args.rval());
    if (NS_FAILED(rv)) {
        xpc::Throw(cx, rv);
        return false;
    }

    return true;
}

template<typename Base>
bool AddonWrapper<Base>::call(JSContext* cx, JS::Handle<JSObject*> wrapper,
                              const JS::CallArgs& args) const
{
    bool done = false;
    if (!InterposeCall(cx, wrapper, args, &done))
        return false;

    return done || Base::call(cx, wrapper, args);
}

template<typename Base>
bool
AddonWrapper<Base>::getPropertyDescriptor(JSContext* cx, HandleObject wrapper,
                                          HandleId id, MutableHandle<JSPropertyDescriptor> desc) const
{
    if (!InterposeProperty(cx, wrapper, nullptr, id, desc))
        return false;

    if (desc.object())
        return true;

    return Base::getPropertyDescriptor(cx, wrapper, id, desc);
}

template<typename Base>
bool
AddonWrapper<Base>::getOwnPropertyDescriptor(JSContext* cx, HandleObject wrapper,
                                             HandleId id, MutableHandle<JSPropertyDescriptor> desc) const
{
    if (!InterposeProperty(cx, wrapper, nullptr, id, desc))
        return false;

    if (desc.object())
        return true;

    return Base::getOwnPropertyDescriptor(cx, wrapper, id, desc);
}

template<typename Base>
bool
AddonWrapper<Base>::get(JSContext* cx, JS::Handle<JSObject*> wrapper, JS::Handle<Value> receiver,
                        JS::Handle<jsid> id, JS::MutableHandle<JS::Value> vp) const
{
    Rooted<JSPropertyDescriptor> desc(cx);
    if (!InterposeProperty(cx, wrapper, nullptr, id, &desc))
        return false;

    if (!desc.object())
        return Base::get(cx, wrapper, receiver, id, vp);

    if (desc.getter()) {
        return Call(cx, receiver, desc.getterObject(), HandleValueArray::empty(), vp);
    } else {
        vp.set(desc.value());
        return true;
    }
}

template<typename Base>
bool
AddonWrapper<Base>::set(JSContext* cx, JS::HandleObject wrapper, JS::HandleId id, JS::HandleValue v,
                        JS::HandleValue receiver, JS::ObjectOpResult& result) const
{
    Rooted<JSPropertyDescriptor> desc(cx);
    if (!InterposeProperty(cx, wrapper, nullptr, id, &desc))
        return false;

    if (!desc.object())
        return Base::set(cx, wrapper, id, v, receiver, result);

    if (desc.setter()) {
        MOZ_ASSERT(desc.hasSetterObject());
        JS::AutoValueVector args(cx);
        if (!args.append(v))
            return false;
        RootedValue fval(cx, ObjectValue(*desc.setterObject()));
        RootedValue ignored(cx);
        if (!JS::Call(cx, receiver, fval, args, &ignored))
            return false;
        return result.succeed();
    }

    return result.failCantSetInterposed();
}

template<typename Base>
bool
AddonWrapper<Base>::defineProperty(JSContext* cx, HandleObject wrapper, HandleId id,
                                   Handle<JSPropertyDescriptor> desc,
                                   ObjectOpResult& result) const
{
    Rooted<JSPropertyDescriptor> interpDesc(cx);
    if (!InterposeProperty(cx, wrapper, nullptr, id, &interpDesc))
        return false;

    if (!interpDesc.object())
        return Base::defineProperty(cx, wrapper, id, desc, result);

    js::ReportErrorWithId(cx, "unable to modify interposed property %s", id);
    return false;
}

template<typename Base>
bool
AddonWrapper<Base>::delete_(JSContext* cx, HandleObject wrapper, HandleId id,
                            ObjectOpResult& result) const
{
    Rooted<JSPropertyDescriptor> desc(cx);
    if (!InterposeProperty(cx, wrapper, nullptr, id, &desc))
        return false;

    if (!desc.object())
        return Base::delete_(cx, wrapper, id, result);

    js::ReportErrorWithId(cx, "unable to delete interposed property %s", id);
    return false;
}

#define AddonWrapperCC AddonWrapper<CrossCompartmentWrapper>
#define AddonWrapperXrayXPCWN AddonWrapper<PermissiveXrayXPCWN>
#define AddonWrapperXrayDOM AddonWrapper<PermissiveXrayDOM>

template<> const AddonWrapperCC AddonWrapperCC::singleton(0);
template<> const AddonWrapperXrayXPCWN AddonWrapperXrayXPCWN::singleton(0);
template<> const AddonWrapperXrayDOM AddonWrapperXrayDOM::singleton(0);

template class AddonWrapperCC;
template class AddonWrapperXrayXPCWN;
template class AddonWrapperXrayDOM;

#undef AddonWrapperCC
#undef AddonWrapperXrayXPCWN
#undef AddonWrapperXrayDOM

} // namespace xpc
