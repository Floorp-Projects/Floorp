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

#include "nsID.h"

using namespace js;
using namespace JS;

namespace xpc {

bool
Interpose(JSContext *cx, HandleObject target, const nsIID *iid, HandleId id,
          MutableHandle<JSPropertyDescriptor> descriptor)
{
    XPCWrappedNativeScope *scope = ObjectScope(CurrentGlobalOrNull(cx));
    MOZ_ASSERT(scope->HasInterposition());

    nsCOMPtr<nsIAddonInterposition> interp = scope->GetInterposition();
    JSAddonId *addonId = AddonIdOfObject(target);
    RootedValue addonIdValue(cx, StringValue(StringOfAddonId(addonId)));
    RootedValue prop(cx, IdToValue(id));
    RootedValue targetValue(cx, ObjectValue(*target));
    RootedValue descriptorVal(cx);
    nsresult rv = interp->Interpose(addonIdValue, targetValue,
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
        if (!JS::ParsePropertyDescriptorObject(cx, target, descriptorVal, descriptor))
            return false;
    }

    // Always make the property non-configurable regardless of what the
    // interposition wants.
    descriptor.setAttributes(descriptor.attributes() | JSPROP_PERMANENT);

    if (!JS_WrapPropertyDescriptor(cx, descriptor))
        return false;

    return true;
}

template<typename Base>
bool
AddonWrapper<Base>::getPropertyDescriptor(JSContext *cx, HandleObject wrapper,
                                          HandleId id, MutableHandle<JSPropertyDescriptor> desc) const
{
    if (!Interpose(cx, wrapper, nullptr, id, desc))
        return false;

    if (desc.object())
        return true;

    return Base::getPropertyDescriptor(cx, wrapper, id, desc);
}

template<typename Base>
bool
AddonWrapper<Base>::getOwnPropertyDescriptor(JSContext *cx, HandleObject wrapper,
                                             HandleId id, MutableHandle<JSPropertyDescriptor> desc) const
{
    if (!Interpose(cx, wrapper, nullptr, id, desc))
        return false;

    if (desc.object())
        return true;

    return Base::getOwnPropertyDescriptor(cx, wrapper, id, desc);
}

template<typename Base>
bool
AddonWrapper<Base>::get(JSContext *cx, JS::Handle<JSObject*> wrapper, JS::Handle<JSObject*> receiver,
                        JS::Handle<jsid> id, JS::MutableHandle<JS::Value> vp) const
{
    Rooted<JSPropertyDescriptor> desc(cx);
    if (!Interpose(cx, wrapper, nullptr, id, &desc))
        return false;

    if (!desc.object())
        return Base::get(cx, wrapper, receiver, id, vp);

    if (desc.getter()) {
        MOZ_ASSERT(desc.hasGetterObject());
        AutoValueVector args(cx);
        RootedValue fval(cx, ObjectValue(*desc.getterObject()));
        return JS_CallFunctionValue(cx, receiver, fval, args, vp);
    } else {
        vp.set(desc.value());
        return true;
    }
}

template<typename Base>
bool
AddonWrapper<Base>::set(JSContext *cx, JS::HandleObject wrapper, JS::HandleObject receiver,
                        JS::HandleId id, bool strict, JS::MutableHandleValue vp) const
{
    Rooted<JSPropertyDescriptor> desc(cx);
    if (!Interpose(cx, wrapper, nullptr, id, &desc))
        return false;

    if (!desc.object())
        return Base::set(cx, wrapper, receiver, id, strict, vp);

    if (desc.setter()) {
        MOZ_ASSERT(desc.hasSetterObject());
        MOZ_ASSERT(!desc.isReadonly());
        JS::AutoValueVector args(cx);
        args.append(vp);
        RootedValue fval(cx, ObjectValue(*desc.setterObject()));
        return JS_CallFunctionValue(cx, receiver, fval, args, vp);
    } else {
        if (!strict)
            return true;

        js::ReportErrorWithId(cx, "unable to set interposed data property %s", id);
        return false;
    }
}

template<typename Base>
bool
AddonWrapper<Base>::defineProperty(JSContext *cx, HandleObject wrapper, HandleId id,
                                   MutableHandle<JSPropertyDescriptor> desc) const
{
    Rooted<JSPropertyDescriptor> interpDesc(cx);
    if (!Interpose(cx, wrapper, nullptr, id, &interpDesc))
        return false;

    if (!interpDesc.object())
        return Base::defineProperty(cx, wrapper, id, desc);

    js::ReportErrorWithId(cx, "unable to modify interposed property %s", id);
    return false;
}

template<typename Base>
bool
AddonWrapper<Base>::delete_(JSContext *cx, HandleObject wrapper, HandleId id, bool *bp) const
{
    Rooted<JSPropertyDescriptor> desc(cx);
    if (!Interpose(cx, wrapper, nullptr, id, &desc))
        return false;

    if (!desc.object())
        return Base::delete_(cx, wrapper, id, bp);

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
