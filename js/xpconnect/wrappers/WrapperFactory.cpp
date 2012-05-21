/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CrossOriginWrapper.h"
#include "FilteringWrapper.h"
#include "XrayWrapper.h"
#include "AccessCheck.h"
#include "XPCWrapper.h"

#include "xpcprivate.h"
#include "dombindings.h"
#include "XPCMaps.h"
#include "mozilla/dom/BindingUtils.h"
#include "jsfriendapi.h"

using namespace js;

namespace xpc {

// When chrome pulls a naked property across the membrane using
// .wrappedJSObject, we want it to cross the membrane into the
// chrome compartment without automatically being wrapped into an
// X-ray wrapper. We achieve this by wrapping it into a special
// transparent wrapper in the origin (non-chrome) compartment. When
// an object with that special wrapper applied crosses into chrome,
// we know to not apply an X-ray wrapper.
Wrapper WaiveXrayWrapperWrapper(WrapperFactory::WAIVE_XRAY_WRAPPER_FLAG);

// Objects that haven't been explicitly waived, but have been exposed
// to chrome don't want a CrossOriginWrapper, since that deeply-waives
// but need the transparent behavior of a CrossOriginWrapper. The
// NoWaiverWrapper is like a CrossOriginWrapper that can also hand out
// XrayWrappers as return values.
NoWaiverWrapper NoWaiverWrapper::singleton(0);

// When objects for which we waived the X-ray wrapper cross into
// chrome, we wrap them into a special cross-compartment wrapper
// that transitively extends the waiver to all properties we get
// off it.
CrossOriginWrapper CrossOriginWrapper::singleton(0);

static JSObject *
GetCurrentOuter(JSContext *cx, JSObject *obj)
{
    obj = JS_ObjectToOuterObject(cx, obj);
    if (!obj)
        return nsnull;

    if (IsWrapper(obj) && !js::GetObjectClass(obj)->ext.innerObject) {
        obj = UnwrapObject(obj);
        NS_ASSERTION(js::GetObjectClass(obj)->ext.innerObject,
                     "weird object, expecting an outer window proxy");
    }

    return obj;
}

JSObject *
WrapperFactory::WaiveXray(JSContext *cx, JSObject *obj)
{
    obj = UnwrapObject(obj);

    // We have to make sure that if we're wrapping an outer window, that
    // the .wrappedJSObject also wraps the outer window.
    obj = GetCurrentOuter(cx, obj);

    {
        // See if we already have a waiver wrapper for this object.
        CompartmentPrivate *priv =
            (CompartmentPrivate *)JS_GetCompartmentPrivate(js::GetObjectCompartment(obj));
        JSObject *wobj = nsnull;
        if (priv && priv->waiverWrapperMap) {
            wobj = priv->waiverWrapperMap->Find(obj);
            xpc_UnmarkGrayObject(wobj);
        }

        // No wrapper yet, make one.
        if (!wobj) {
            JSObject *proto = js::GetObjectProto(obj);
            if (proto && !(proto = WaiveXray(cx, proto)))
                return nsnull;

            JSAutoEnterCompartment ac;
            if (!ac.enter(cx, obj) || !JS_WrapObject(cx, &proto))
                return nsnull;
            wobj = Wrapper::New(cx, obj, proto, JS_GetGlobalForObject(cx, obj),
                                &WaiveXrayWrapperWrapper);
            if (!wobj)
                return nsnull;

            // Add the new wrapper so we find it next time.
            if (priv) {
                if (!priv->waiverWrapperMap) {
                    priv->waiverWrapperMap = JSObject2JSObjectMap::newMap(XPC_WRAPPER_MAP_SIZE);
                    if (!priv->waiverWrapperMap)
                        return nsnull;
                }
                if (!priv->waiverWrapperMap->Add(obj, wobj))
                    return nsnull;
            }
        }

        obj = wobj;
    }

    return obj;
}

// DoubleWrap is called from PrepareForWrapping to maintain the state that
// we're supposed to waive Xray wrappers for the given on. On entrance, it
// expects |cx->compartment != obj->compartment()|. The returned object will
// be in the same compartment as |obj|.
JSObject *
WrapperFactory::DoubleWrap(JSContext *cx, JSObject *obj, unsigned flags)
{
    if (flags & WrapperFactory::WAIVE_XRAY_WRAPPER_FLAG) {
        JSAutoEnterCompartment ac;
        if (!ac.enter(cx, obj))
            return nsnull;

        return WaiveXray(cx, obj);
    }
    return obj;
}

JSObject *
WrapperFactory::PrepareForWrapping(JSContext *cx, JSObject *scope, JSObject *obj, unsigned flags)
{
    // Don't unwrap an outer window, just double wrap it if needed.
    if (js::GetObjectClass(obj)->ext.innerObject)
        return DoubleWrap(cx, obj, flags);

    // Here are the rules for wrapping:
    // We should never get a proxy here (the JS engine unwraps those for us).
    JS_ASSERT(!IsWrapper(obj));

    // As soon as an object is wrapped in a security wrapper, it morphs to be
    // a fat wrapper. (see also: bug XXX).
    if (IS_SLIM_WRAPPER(obj) && !MorphSlimWrapper(cx, obj))
        return nsnull;

    // We only hand out outer objects to script.
    obj = GetCurrentOuter(cx, obj);
    if (!obj)
        return nsnull;

    if (js::GetObjectClass(obj)->ext.innerObject)
        return DoubleWrap(cx, obj, flags);

    // Now, our object is ready to be wrapped, but several objects (notably
    // nsJSIIDs) have a wrapper per scope. If we are about to wrap one of
    // those objects in a security wrapper, then we need to hand back the
    // wrapper for the new scope instead. Also, global objects don't move
    // between scopes so for those we also want to return the wrapper. So...
    if (!IS_WN_WRAPPER(obj) || !js::GetObjectParent(obj))
        return DoubleWrap(cx, obj, flags);

    XPCWrappedNative *wn = static_cast<XPCWrappedNative *>(xpc_GetJSPrivate(obj));

    JSAutoEnterCompartment ac;
    if (!ac.enter(cx, obj))
        return nsnull;
    XPCCallContext ccx(JS_CALLER, cx, obj);

    {
        if (NATIVE_HAS_FLAG(&ccx, WantPreCreate)) {
            // We have a precreate hook. This object might enforce that we only
            // ever create JS object for it.
            JSObject *originalScope = scope;
            nsresult rv = wn->GetScriptableInfo()->GetCallback()->
                PreCreate(wn->Native(), cx, scope, &scope);
            NS_ENSURE_SUCCESS(rv, DoubleWrap(cx, obj, flags));

            // If the handed back scope differs from the passed-in scope and is in
            // a separate compartment, then this object is explicitly requesting
            // that we don't create a second JS object for it: create a security
            // wrapper.
            if (js::GetObjectCompartment(originalScope) != js::GetObjectCompartment(scope))
                return DoubleWrap(cx, obj, flags);

            // Note: this penalizes objects that only have one wrapper, but are
            // being accessed across compartments. We would really prefer to
            // replace the above code with a test that says "do you only have one
            // wrapper?"
        }
    }

    // NB: Passing a holder here inhibits slim wrappers under
    // WrapNativeToJSVal.
    nsCOMPtr<nsIXPConnectJSObjectHolder> holder;

    // This public WrapNativeToJSVal API enters the compartment of 'scope'
    // so we don't have to.
    jsval v;
    nsresult rv =
        nsXPConnect::FastGetXPConnect()->WrapNativeToJSVal(cx, scope, wn->Native(), nsnull,
                                                           &NS_GET_IID(nsISupports), false,
                                                           &v, getter_AddRefs(holder));
    if (NS_SUCCEEDED(rv)) {
        obj = JSVAL_TO_OBJECT(v);
        NS_ASSERTION(IS_WN_WRAPPER(obj), "bad object");

        // Because the underlying native didn't have a PreCreate hook, we had
        // to a new (or possibly pre-existing) XPCWN in our compartment.
        // This could be a problem for chrome code that passes XPCOM objects
        // across compartments, because the effects of QI would disappear across
        // compartments.
        //
        // So whenever we pull an XPCWN across compartments in this manner, we
        // give the destination object the union of the two native sets. We try
        // to do this cleverly in the common case to avoid too much overhead.
        XPCWrappedNative *newwn = static_cast<XPCWrappedNative *>(xpc_GetJSPrivate(obj));
        XPCNativeSet *unionSet = XPCNativeSet::GetNewOrUsed(ccx, newwn->GetSet(),
                                                            wn->GetSet(), false);
        if (!unionSet)
            return nsnull;
        newwn->SetSet(unionSet);
    }

    return DoubleWrap(cx, obj, flags);
}

static XPCWrappedNative *
GetWrappedNative(JSContext *cx, JSObject *obj)
{
    obj = JS_ObjectToInnerObject(cx, obj);
    return IS_WN_WRAPPER(obj)
           ? static_cast<XPCWrappedNative *>(js::GetObjectPrivate(obj))
           : nsnull;
}

enum XrayType {
    XrayForDOMObject,
    XrayForDOMProxyObject,
    XrayForWrappedNative,
    NotXray
};

static XrayType
GetXrayType(JSObject *obj)
{
    js::Class* clasp = js::GetObjectClass(obj);
    if (mozilla::dom::IsDOMClass(Jsvalify(clasp))) {
        return XrayForDOMObject;
    }
    if (mozilla::dom::binding::instanceIsProxy(obj)) {
        return XrayForDOMProxyObject;
    }
    if (IS_WRAPPER_CLASS(clasp) || clasp->ext.innerObject) {
        NS_ASSERTION(clasp->ext.innerObject || IS_WN_WRAPPER_OBJECT(obj),
                     "We forgot to Morph a slim wrapper!");
        return XrayForWrappedNative;
    }
    return NotXray;
}

JSObject *
WrapperFactory::Rewrap(JSContext *cx, JSObject *obj, JSObject *wrappedProto, JSObject *parent,
                       unsigned flags)
{
    NS_ASSERTION(!IsWrapper(obj) ||
                 GetProxyHandler(obj) == &WaiveXrayWrapperWrapper ||
                 js::GetObjectClass(obj)->ext.innerObject,
                 "wrapped object passed to rewrap");
    NS_ASSERTION(JS_GetClass(obj) != &XrayUtils::HolderClass, "trying to wrap a holder");

    JSCompartment *origin = js::GetObjectCompartment(obj);
    JSCompartment *target = js::GetContextCompartment(cx);
    bool usingXray = false;

    Wrapper *wrapper;
    CompartmentPrivate *targetdata =
        static_cast<CompartmentPrivate *>(JS_GetCompartmentPrivate(target));
    if (AccessCheck::isChrome(target)) {
        if (AccessCheck::isChrome(origin)) {
            wrapper = &CrossCompartmentWrapper::singleton;
        } else {
            bool isSystem;
            {
                JSAutoEnterCompartment ac;
                if (!ac.enter(cx, obj))
                    return nsnull;
                JSObject *globalObj = JS_GetGlobalForObject(cx, obj);
                JS_ASSERT(globalObj);
                isSystem = JS_IsSystemObject(cx, globalObj);
            }

            if (isSystem) {
                wrapper = &CrossCompartmentWrapper::singleton;
            } else if (flags & WAIVE_XRAY_WRAPPER_FLAG) {
                // If we waived the X-ray wrapper for this object, wrap it into a
                // special wrapper to transitively maintain the X-ray waiver.
                wrapper = &CrossOriginWrapper::singleton;
            } else {
                // Native objects must be wrapped into an X-ray wrapper.
                XrayType type = GetXrayType(obj);
                if (type == XrayForDOMObject) {
                    wrapper = &XrayDOM::singleton;
                } else if (type == XrayForDOMProxyObject) {
                    wrapper = &XrayProxy::singleton;
                } else if (type == XrayForWrappedNative) {
                    typedef XrayWrapper<CrossCompartmentWrapper> Xray;
                    usingXray = true;
                    wrapper = &Xray::singleton;
                } else {
                    wrapper = &NoWaiverWrapper::singleton;
                }
            }
        }
    } else if (AccessCheck::isChrome(origin)) {
        JSFunction *fun = JS_GetObjectFunction(obj);
        if (fun) {
            if (JS_IsBuiltinEvalFunction(fun) || JS_IsBuiltinFunctionConstructor(fun)) {
                JS_ReportError(cx, "Not allowed to access chrome eval or Function from content");
                return nsnull;
            }
        }

        XPCWrappedNative *wn;
        if (targetdata &&
            (wn = GetWrappedNative(cx, obj)) &&
            wn->HasProto() && wn->GetProto()->ClassIsDOMObject()) {
            typedef XrayWrapper<CrossCompartmentSecurityWrapper> Xray;
            usingXray = true;
            if (IsLocationObject(obj))
                wrapper = &FilteringWrapper<Xray, LocationPolicy>::singleton;
            else
                wrapper = &FilteringWrapper<Xray, CrossOriginAccessiblePropertiesOnly>::singleton;
        } else if (mozilla::dom::binding::instanceIsProxy(obj)) {
            wrapper = &FilteringWrapper<XrayProxy, CrossOriginAccessiblePropertiesOnly>::singleton;
        } else if (mozilla::dom::IsDOMClass(JS_GetClass(obj))) {
            wrapper = &FilteringWrapper<XrayDOM, CrossOriginAccessiblePropertiesOnly>::singleton;
        } else if (IsComponentsObject(obj)) {
            wrapper = &FilteringWrapper<CrossCompartmentSecurityWrapper,
                                        ComponentsObjectPolicy>::singleton;
        } else {
            wrapper = &FilteringWrapper<CrossCompartmentSecurityWrapper,
                                        ExposedPropertiesOnly>::singleton;
        }
    } else if (AccessCheck::isSameOrigin(origin, target)) {
        // For the same-origin case we use a transparent wrapper, unless one
        // of the following is true:
        // * The object is flagged as needing a SOW.
        // * The object is a location object.
        // * The context compartment specifically requested Xray vision into
        //   same-origin compartments.
        //
        // The first two cases always require a security wrapper for non-chrome
        // access, regardless of the origin of the object.
        //
        // The Location case is a bit tricky. Because the security characteristics
        // depend on the current outer window, we always have a security wrapper
        // around locations, same-compartment or cross-compartment. We would
        // normally just use an identical security policy and just switch between
        // Wrapper and CrossCompartmentWrapper to differentiate the cases (LW/XLW).
        // However, there's an added wrinkle that same-origin-but-cross-compartment
        // scripts expect to be able to see expandos on each others' location
        // objects. So if all cross-compartment access used XLWs, then the expandos
        // would live on the per-compartment XrayWrapper expando object, and would
        // not be shared. So to make sure that expandos work correctly in the
        // same-origin case, we need to use a transparent CrossCompartmentWrapper
        // to the LW in the host compartment, rather than an XLW directly to the
        // Location object. This still doesn't share expandos in the
        // document.domain case, but that's probably fine. Double-wrapping sucks,
        // but it's kind of unavoidable here.
        XrayType type;
        if (AccessCheck::needsSystemOnlyWrapper(obj)) {
            wrapper = &FilteringWrapper<CrossCompartmentSecurityWrapper,
                                        OnlyIfSubjectIsSystem>::singleton;
        } else if (IsComponentsObject(obj)) {
            wrapper = &FilteringWrapper<CrossCompartmentSecurityWrapper,
                                        ComponentsObjectPolicy>::singleton;
        } else if (!targetdata || !targetdata->wantXrays ||
                   (type = GetXrayType(obj)) == NotXray) {
            // Do the double-wrapping if need be.
            if (IsLocationObject(obj)) {
                JSAutoEnterCompartment ac;
                if (!ac.enter(cx, obj))
                    return nsnull;
                XPCWrappedNative *wn = GetWrappedNative(cx, obj);
                if (!wn)
                    return nsnull;
                obj = wn->GetSameCompartmentSecurityWrapper(cx);
            }
            wrapper = &CrossCompartmentWrapper::singleton;
        } else if (type == XrayForDOMObject) {
            wrapper = &XrayDOM::singleton;
        } else if (type == XrayForDOMProxyObject) {
            wrapper = &XrayProxy::singleton;
        } else {
            typedef XrayWrapper<CrossCompartmentWrapper> Xray;
            usingXray = true;
            wrapper = &Xray::singleton;
        }
    } else {
        NS_ASSERTION(!AccessCheck::needsSystemOnlyWrapper(obj),
                     "bad object exposed across origins");

        // Cross origin we want to disallow scripting and limit access to
        // a predefined set of properties. XrayWrapper adds a property
        // (.wrappedJSObject) which allows bypassing the XrayWrapper, but
        // we filter out access to that property.
        XrayType type = GetXrayType(obj);
        if (type == NotXray) {
            wrapper = &FilteringWrapper<CrossCompartmentSecurityWrapper,
                                        CrossOriginAccessiblePropertiesOnly>::singleton;
        } else if (type == XrayForDOMObject) {
            wrapper = &FilteringWrapper<XrayDOM,
                                        CrossOriginAccessiblePropertiesOnly>::singleton;
        } else if (type == XrayForDOMProxyObject) {
            wrapper = &FilteringWrapper<XrayProxy,
                                        CrossOriginAccessiblePropertiesOnly>::singleton;
        } else {
            typedef XrayWrapper<CrossCompartmentSecurityWrapper> Xray;
            usingXray = true;

            // Location objects can become same origin after navigation, so we might
            // have to grant transparent access later on.
            if (IsLocationObject(obj)) {
                wrapper = &FilteringWrapper<Xray, LocationPolicy>::singleton;
            } else {
                wrapper = &FilteringWrapper<Xray,
                    CrossOriginAccessiblePropertiesOnly>::singleton;
            }
        }
    }

    JSObject *wrapperObj = Wrapper::New(cx, obj, wrappedProto, parent, wrapper);
    if (!wrapperObj || !usingXray)
        return wrapperObj;

    JSObject *xrayHolder = XrayUtils::createHolder(cx, obj, parent);
    if (!xrayHolder)
        return nsnull;
    js::SetProxyExtra(wrapperObj, 0, js::ObjectValue(*xrayHolder));
    return wrapperObj;
}

JSObject *
WrapperFactory::WrapForSameCompartment(JSContext *cx, JSObject *obj)
{
    // Only WNs have same-compartment wrappers.
    //
    // NB: The contract of WrapForSameCompartment says that |obj| may or may not
    // be a security wrapper. This check implicitly handles the security wrapper
    // case.
    if (!IS_WN_WRAPPER(obj))
        return obj;

    // Extract the WN. It should exist.
    XPCWrappedNative *wn = static_cast<XPCWrappedNative *>(xpc_GetJSPrivate(obj));
    MOZ_ASSERT(wn, "Trying to wrap a dead WN!");

    // The WN knows what to do.
    return wn->GetSameCompartmentSecurityWrapper(cx);
}

typedef FilteringWrapper<XrayWrapper<SameCompartmentSecurityWrapper>, LocationPolicy> LW;

bool
WrapperFactory::IsLocationObject(JSObject *obj)
{
    const char *name = js::GetObjectClass(obj)->name;
    return name[0] == 'L' && !strcmp(name, "Location");
}

JSObject *
WrapperFactory::WrapLocationObject(JSContext *cx, JSObject *obj)
{
    JSObject *xrayHolder = XrayUtils::createHolder(cx, obj, js::GetObjectParent(obj));
    if (!xrayHolder)
        return nsnull;
    JSObject *wrapperObj = Wrapper::New(cx, obj, js::GetObjectProto(obj), js::GetObjectParent(obj),
                                        &LW::singleton);
    if (!wrapperObj)
        return nsnull;
    js::SetProxyExtra(wrapperObj, 0, js::ObjectValue(*xrayHolder));
    return wrapperObj;
}

// Call WaiveXrayAndWrap when you have a JS object that you don't want to be
// wrapped in an Xray wrapper. cx->compartment is the compartment that will be
// using the returned object. If the object to be wrapped is already in the
// correct compartment, then this returns the unwrapped object.
bool
WrapperFactory::WaiveXrayAndWrap(JSContext *cx, jsval *vp)
{
    if (JSVAL_IS_PRIMITIVE(*vp))
        return JS_WrapValue(cx, vp);

    JSObject *obj = js::UnwrapObject(JSVAL_TO_OBJECT(*vp));
    obj = GetCurrentOuter(cx, obj);
    if (js::IsObjectInContextCompartment(obj, cx)) {
        *vp = OBJECT_TO_JSVAL(obj);
        return true;
    }

    obj = WaiveXray(cx, obj);
    if (!obj)
        return false;

    *vp = OBJECT_TO_JSVAL(obj);
    return JS_WrapValue(cx, vp);
}

JSObject *
WrapperFactory::WrapSOWObject(JSContext *cx, JSObject *obj)
{
    JSObject *wrapperObj =
        Wrapper::New(cx, obj, JS_GetPrototype(obj), JS_GetGlobalForObject(cx, obj),
                     &FilteringWrapper<SameCompartmentSecurityWrapper,
                     OnlyIfSubjectIsSystem>::singleton);
    return wrapperObj;
}

bool
WrapperFactory::IsComponentsObject(JSObject *obj)
{
    const char *name = js::GetObjectClass(obj)->name;
    return name[0] == 'n' && !strcmp(name, "nsXPCComponents");
}

JSObject *
WrapperFactory::WrapComponentsObject(JSContext *cx, JSObject *obj)
{
    JSObject *wrapperObj =
        Wrapper::New(cx, obj, JS_GetPrototype(obj), JS_GetGlobalForObject(cx, obj),
                     &FilteringWrapper<SameCompartmentSecurityWrapper, ComponentsObjectPolicy>::singleton);

    return wrapperObj;
}

}
