/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WaiveXrayWrapper.h"
#include "FilteringWrapper.h"
#include "XrayWrapper.h"
#include "AccessCheck.h"
#include "XPCWrapper.h"
#include "ChromeObjectWrapper.h"

#include "xpcprivate.h"
#include "XPCMaps.h"
#include "mozilla/dom/BindingUtils.h"
#include "jsfriendapi.h"
#include "mozilla/Likely.h"
#include "nsContentUtils.h"

using namespace js;
using namespace mozilla;

namespace xpc {

// When chrome pulls a naked property across the membrane using
// .wrappedJSObject, we want it to cross the membrane into the
// chrome compartment without automatically being wrapped into an
// X-ray wrapper. We achieve this by wrapping it into a special
// transparent wrapper in the origin (non-chrome) compartment. When
// an object with that special wrapper applied crosses into chrome,
// we know to not apply an X-ray wrapper.
Wrapper XrayWaiver(WrapperFactory::WAIVE_XRAY_WRAPPER_FLAG);

// When objects for which we waived the X-ray wrapper cross into
// chrome, we wrap them into a special cross-compartment wrapper
// that transitively extends the waiver to all properties we get
// off it.
WaiveXrayWrapper WaiveXrayWrapper::singleton(0);

bool
WrapperFactory::IsCOW(JSObject *obj)
{
    return IsWrapper(obj) &&
           Wrapper::wrapperHandler(obj) == &ChromeObjectWrapper::singleton;
}

JSObject *
WrapperFactory::GetXrayWaiver(JSObject *obj)
{
    // Object should come fully unwrapped but outerized.
    MOZ_ASSERT(obj == UnwrapObject(obj));
    MOZ_ASSERT(!js::GetObjectClass(obj)->ext.outerObject);
    XPCWrappedNativeScope *scope = GetObjectScope(obj);
    MOZ_ASSERT(scope);

    if (!scope->mWaiverWrapperMap)
        return NULL;
    return xpc_UnmarkGrayObject(scope->mWaiverWrapperMap->Find(obj));
}

JSObject *
WrapperFactory::CreateXrayWaiver(JSContext *cx, JSObject *obj)
{
    // The caller is required to have already done a lookup.
    // NB: This implictly performs the assertions of GetXrayWaiver.
    MOZ_ASSERT(!GetXrayWaiver(obj));
    XPCWrappedNativeScope *scope = GetObjectScope(obj);

    // Get a waiver for the proto.
    JSObject *proto;
    if (!js::GetObjectProto(cx, obj, &proto))
        return nullptr;
    if (proto && !(proto = WaiveXray(cx, proto)))
        return nullptr;

    // Create the waiver.
    JSAutoCompartment ac(cx, obj);
    if (!JS_WrapObject(cx, &proto))
        return nullptr;
    JSObject *waiver = Wrapper::New(cx, obj, proto,
                                    JS_GetGlobalForObject(cx, obj),
                                    &XrayWaiver);
    if (!waiver)
        return nullptr;

    // Add the new waiver to the map. It's important that we only ever have
    // one waiver for the lifetime of the target object.
    if (!scope->mWaiverWrapperMap) {
        scope->mWaiverWrapperMap =
          JSObject2JSObjectMap::newMap(XPC_WRAPPER_MAP_SIZE);
        MOZ_ASSERT(scope->mWaiverWrapperMap);
    }
    if (!scope->mWaiverWrapperMap->Add(obj, waiver))
        return nullptr;
    return waiver;
}

JSObject *
WrapperFactory::WaiveXray(JSContext *cx, JSObject *obj)
{
    obj = UnwrapObject(obj);
    MOZ_ASSERT(!js::IsInnerObject(obj));

    JSObject *waiver = GetXrayWaiver(obj);
    if (waiver)
        return waiver;
    return CreateXrayWaiver(cx, obj);
}

// DoubleWrap is called from PrepareForWrapping to maintain the state that
// we're supposed to waive Xray wrappers for the given on. On entrance, it
// expects |cx->compartment != obj->compartment()|. The returned object will
// be in the same compartment as |obj|.
JSObject *
WrapperFactory::DoubleWrap(JSContext *cx, JSObject *obj, unsigned flags)
{
    if (flags & WrapperFactory::WAIVE_XRAY_WRAPPER_FLAG) {
        JSAutoCompartment ac(cx, obj);
        return WaiveXray(cx, obj);
    }
    return obj;
}

JSObject *
WrapperFactory::PrepareForWrapping(JSContext *cx, JSObject *scope, JSObject *obj, unsigned flags)
{
    // Outerize any raw inner objects at the entry point here, so that we don't
    // have to worry about them for the rest of the wrapping code.
    if (js::IsInnerObject(obj)) {
        JSAutoCompartment ac(cx, obj);
        obj = JS_ObjectToOuterObject(cx, obj);
        NS_ENSURE_TRUE(obj, nullptr);
        // The outerization hook wraps, which means that we can end up with a
        // CCW here if |obj| was a navigated-away-from inner. Strip any CCWs.
        obj = js::UnwrapObject(obj);
        MOZ_ASSERT(js::IsOuterObject(obj));
    }

    // If we've got an outer window, there's nothing special that needs to be
    // done here, and we can move on to the next phase of wrapping. We handle
    // this case first to allow us to assert against wrappers below.
    if (js::IsOuterObject(obj))
        return DoubleWrap(cx, obj, flags);

    // Here are the rules for wrapping:
    // We should never get a proxy here (the JS engine unwraps those for us).
    MOZ_ASSERT(!IsWrapper(obj));

    // As soon as an object is wrapped in a security wrapper, it morphs to be
    // a fat wrapper. (see also: bug XXX).
    if (IS_SLIM_WRAPPER(obj) && !MorphSlimWrapper(cx, obj))
        return nullptr;

    // Now, our object is ready to be wrapped, but several objects (notably
    // nsJSIIDs) have a wrapper per scope. If we are about to wrap one of
    // those objects in a security wrapper, then we need to hand back the
    // wrapper for the new scope instead. Also, global objects don't move
    // between scopes so for those we also want to return the wrapper. So...
    if (!IS_WN_WRAPPER(obj) || !js::GetObjectParent(obj))
        return DoubleWrap(cx, obj, flags);

    XPCWrappedNative *wn = static_cast<XPCWrappedNative *>(xpc_GetJSPrivate(obj));

    JSAutoCompartment ac(cx, obj);
    XPCCallContext ccx(JS_CALLER, cx, obj);

    {
        if (NATIVE_HAS_FLAG(&ccx, WantPreCreate)) {
            // We have a precreate hook. This object might enforce that we only
            // ever create JS object for it.

            // Note: this penalizes objects that only have one wrapper, but are
            // being accessed across compartments. We would really prefer to
            // replace the above code with a test that says "do you only have one
            // wrapper?"
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

            JSObject *currentScope = JS_GetGlobalForObject(cx, obj);
            if (MOZ_UNLIKELY(scope != currentScope)) {
                // The wrapper claims it wants to be in the new scope, but
                // currently has a reflection that lives in the old scope. This
                // can mean one of two things, both of which are rare:
                //
                // 1 - The object has a PreCreate hook (we checked for it above),
                // but is deciding to request one-wrapper-per-scope (rather than
                // one-wrapper-per-native) for some reason. Usually, a PreCreate
                // hook indicates one-wrapper-per-native. In this case we want to
                // make a new wrapper in the new scope.
                //
                // 2 - We're midway through wrapper reparenting. The document has
                // moved to a new scope, but |wn| hasn't been moved yet, and
                // we ended up calling JS_WrapObject() on its JS object. In this
                // case, we want to return the existing wrapper.
                //
                // So we do a trick: call PreCreate _again_, but say that we're
                // wrapping for the old scope, rather than the new one. If (1) is
                // the case, then PreCreate will return the scope we pass to it
                // (the old scope). If (2) is the case, PreCreate will return the
                // scope of the document (the new scope).
                JSObject *probe;
                rv = wn->GetScriptableInfo()->GetCallback()->
                    PreCreate(wn->Native(), cx, currentScope, &probe);

                // Check for case (2).
                if (probe != currentScope) {
                    MOZ_ASSERT(probe == scope);
                    return DoubleWrap(cx, obj, flags);
                }

                // Ok, must be case (1). Fall through and create a new wrapper.
            }

            // Nasty hack for late-breaking bug 781476. This will confuse identity checks,
            // but it's probably better than any of our alternatives.
            //
            // Note: We have to ignore domain here. The JS engine assumes that, given a
            // compartment c, if c->wrap(x) returns a cross-compartment wrapper at time t0,
            // it will also return a cross-compartment wrapper for any time t1 > t0 unless
            // an explicit transplant is performed. In particular, wrapper recomputation
            // assumes that recomputing a wrapper will always result in a wrapper.
            //
            // This doesn't actually pose a security issue, because we'll still compute
            // the correct (opaque) wrapper for the object below given the security
            // characteristics of the two compartments.
            if (!AccessCheck::isChrome(js::GetObjectCompartment(scope)) &&
                 AccessCheck::subsumesIgnoringDomain(js::GetObjectCompartment(scope),
                                                     js::GetObjectCompartment(obj)))
            {
                return DoubleWrap(cx, obj, flags);
            }
        }
    }

    // NB: Passing a holder here inhibits slim wrappers under
    // WrapNativeToJSVal.
    nsCOMPtr<nsIXPConnectJSObjectHolder> holder;

    // This public WrapNativeToJSVal API enters the compartment of 'scope'
    // so we don't have to.
    jsval v;
    nsresult rv =
        nsXPConnect::FastGetXPConnect()->WrapNativeToJSVal(cx, scope, wn->Native(), nullptr,
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
            return nullptr;
        newwn->SetSet(unionSet);
    }

    return DoubleWrap(cx, obj, flags);
}

#ifdef DEBUG
static void
DEBUG_CheckUnwrapSafety(JSObject *obj, js::Wrapper *handler,
                        JSCompartment *origin, JSCompartment *target)
{
    if (AccessCheck::isChrome(target) || xpc::IsUniversalXPConnectEnabled(target)) {
        // If the caller is chrome (or effectively so), unwrap should always be allowed.
        MOZ_ASSERT(handler->isSafeToUnwrap());
    } else if (WrapperFactory::IsComponentsObject(obj))
    {
        // The Components object that is restricted regardless of origin.
        MOZ_ASSERT(!handler->isSafeToUnwrap());
    } else if (AccessCheck::needsSystemOnlyWrapper(obj)) {
        // SOWs have a dynamic unwrap check, so we can't really say anything useful
        // about them here :-(
    } else {
        // Otherwise, it should depend on whether the target subsumes the origin.
        MOZ_ASSERT(handler->isSafeToUnwrap() == AccessCheck::subsumes(target, origin));
    }
}
#else
#define DEBUG_CheckUnwrapSafety(obj, handler, origin, target) {}
#endif

static Wrapper *
SelectWrapper(bool securityWrapper, bool wantXrays, XrayType xrayType,
              bool waiveXrays)
{
    // Waived Xray uses a modified CCW that has transparent behavior but
    // transitively waives Xrays on arguments.
    if (waiveXrays) {
        MOZ_ASSERT(!securityWrapper);
        return &WaiveXrayWrapper::singleton;
    }

    // If we don't want or can't use Xrays, select a wrapper that's either
    // entirely transparent or entirely opaque.
    if (!wantXrays || xrayType == NotXray) {
        if (!securityWrapper)
            return &CrossCompartmentWrapper::singleton;
        return &FilteringWrapper<CrossCompartmentSecurityWrapper, Opaque>::singleton;
    }

    // Ok, we're using Xray. If this isn't a security wrapper, use the permissive
    // version and skip the filter.
    if (!securityWrapper) {
        if (xrayType == XrayForWrappedNative)
            return &PermissiveXrayXPCWN::singleton;
        return &PermissiveXrayDOM::singleton;
    }

    // This is a security wrapper. Use the security versions and filter.
    if (xrayType == XrayForWrappedNative)
        return &FilteringWrapper<SecurityXrayXPCWN,
                                 CrossOriginAccessiblePropertiesOnly>::singleton;
    return &FilteringWrapper<SecurityXrayDOM,
                             CrossOriginAccessiblePropertiesOnly>::singleton;
}

JSObject *
WrapperFactory::Rewrap(JSContext *cx, JSObject *existing, JSObject *obj,
                       JSObject *wrappedProto, JSObject *parent,
                       unsigned flags)
{
    MOZ_ASSERT(!IsWrapper(obj) ||
               GetProxyHandler(obj) == &XrayWaiver ||
               js::GetObjectClass(obj)->ext.innerObject,
               "wrapped object passed to rewrap");
    MOZ_ASSERT(JS_GetClass(obj) != &XrayUtils::HolderClass, "trying to wrap a holder");
    MOZ_ASSERT(!js::IsInnerObject(obj));
    // We sometimes end up here after nsContentUtils has been shut down but before
    // XPConnect has been shut down, so check the context stack the roundabout way.
    MOZ_ASSERT(XPCJSRuntime::Get()->GetJSContextStack()->Peek() == cx);

    // Compute the information we need to select the right wrapper.
    JSCompartment *origin = js::GetObjectCompartment(obj);
    JSCompartment *target = js::GetContextCompartment(cx);
    bool originIsChrome = AccessCheck::isChrome(origin);
    bool targetIsChrome = AccessCheck::isChrome(target);
    bool originSubsumesTarget = AccessCheck::subsumes(origin, target);
    bool targetSubsumesOrigin = AccessCheck::subsumes(target, origin);
    bool sameOrigin = targetSubsumesOrigin && originSubsumesTarget;
    XrayType xrayType = GetXrayType(obj);

    // By default we use the wrapped proto of the underlying object as the
    // prototype for our wrapper, but we may select something different below.
    JSObject *proxyProto = wrappedProto;

    Wrapper *wrapper;
    CompartmentPrivate *targetdata = EnsureCompartmentPrivate(target);

    //
    // First, handle the special cases.
    //

    // If UniversalXPConnect is enabled, this is just some dumb mochitest. Use
    // a vanilla CCW.
    if (xpc::IsUniversalXPConnectEnabled(target)) {
        wrapper = &CrossCompartmentWrapper::singleton;

    // If this is a chrome object being exposed to content without Xrays, use
    // a COW.
    } else if (originIsChrome && !targetIsChrome && xrayType == NotXray) {
        wrapper = &ChromeObjectWrapper::singleton;

    // If content is accessing a Components object or NAC, we need a special filter,
    // even if the object is same origin.
    } else if (IsComponentsObject(obj) && !AccessCheck::isChrome(target)) {
        wrapper = &FilteringWrapper<CrossCompartmentSecurityWrapper,
                                    ComponentsObjectPolicy>::singleton;
    } else if (AccessCheck::needsSystemOnlyWrapper(obj) &&
               !(targetIsChrome || (targetSubsumesOrigin && nsContentUtils::IsCallerXBL())))
    {
        wrapper = &FilteringWrapper<CrossCompartmentSecurityWrapper,
                                    OnlyIfSubjectIsSystem>::singleton;
    }

    //
    // Now, handle the regular cases.
    //
    // These are wrappers we can compute using a rule-based approach. In order
    // to do so, we need to compute some parameters.
    //
    else {

        // The wrapper is a security wrapper (protecting the wrappee) if and
        // only if the target does not subsume the origin.
        bool securityWrapper = !targetSubsumesOrigin;

        // Xrays are warranted if either the target or the origin don't trust
        // each other. This is generally the case, unless the two are same-origin
        // and the caller has not requested same-origin Xrays.
        //
        // Xrays are a bidirectional protection, since it affords clarity to the
        // caller and privacy to the callee.
        bool wantXrays = !(sameOrigin && !targetdata->wantXrays);

        // If Xrays are warranted, the caller may waive them for non-security
        // wrappers.
        bool waiveXrays = wantXrays && !securityWrapper &&
                          (flags & WAIVE_XRAY_WRAPPER_FLAG);

        wrapper = SelectWrapper(securityWrapper, wantXrays, xrayType, waiveXrays);
    }



    // If the prototype of a chrome object being wrapped in content is a prototype
    // for a standard class, use the one from the content compartment so
    // that we can safely take advantage of things like .forEach().
    //
    // If the prototype chain of chrome object |obj| looks like this:
    //
    // obj => foo => bar => chromeWin.StandardClass.prototype
    //
    // The prototype chain of COW(obj) looks lke this:
    //
    // COW(obj) => COW(foo) => COW(bar) => contentWin.StandardClass.prototype
    if (wrapper == &ChromeObjectWrapper::singleton) {
        JSProtoKey key = JSProto_Null;
        {
            JSAutoCompartment ac(cx, obj);
            JSObject *unwrappedProto;
            if (!js::GetObjectProto(cx, obj, &unwrappedProto))
                return NULL;
            if (unwrappedProto && IsCrossCompartmentWrapper(unwrappedProto))
                unwrappedProto = Wrapper::wrappedObject(unwrappedProto);
            if (unwrappedProto) {
                JSAutoCompartment ac2(cx, unwrappedProto);
                key = JS_IdentifyClassPrototype(cx, unwrappedProto);
            }
        }
        if (key != JSProto_Null) {
            JSObject *homeProto;
            if (!JS_GetClassPrototype(cx, key, &homeProto))
                return NULL;
            MOZ_ASSERT(homeProto);
            proxyProto = homeProto;
        }

        // This shouldn't happen, but do a quick check to make some dumb addon
        // doesn't expose chrome eval or Function().
        JSFunction *fun = JS_GetObjectFunction(obj);
        if (fun) {
            if (JS_IsBuiltinEvalFunction(fun) || JS_IsBuiltinFunctionConstructor(fun)) {
                JS_ReportError(cx, "Not allowed to access chrome eval or Function from content");
                return nullptr;
            }
        }
    }

    DEBUG_CheckUnwrapSafety(obj, wrapper, origin, target);

    if (existing && proxyProto == wrappedProto)
        return Wrapper::Renew(cx, existing, obj, wrapper);

    return Wrapper::New(cx, obj, proxyProto, parent, wrapper);
}

JSObject *
WrapperFactory::WrapForSameCompartment(JSContext *cx, JSObject *obj)
{
    MOZ_ASSERT(js::IsObjectInContextCompartment(obj, cx));

    // NB: The contract of WrapForSameCompartment says that |obj| may or may not
    // be a security wrapper. These checks implicitly handle the security
    // wrapper case.

    // Outerize if necessary. This, in combination with the check in
    // PrepareForUnwrapping, means that calling JS_Wrap* always outerizes.
    obj = JS_ObjectToOuterObject(cx, obj);
    NS_ENSURE_TRUE(obj, nullptr);

    if (dom::GetSameCompartmentWrapperForDOMBinding(obj)) {
        return obj;
    }

    MOZ_ASSERT(!dom::IsDOMObject(obj));

    if (!IS_WN_WRAPPER(obj))
        return obj;

    // Extract the WN. It should exist.
    XPCWrappedNative *wn = static_cast<XPCWrappedNative *>(xpc_GetJSPrivate(obj));
    MOZ_ASSERT(wn, "Trying to wrap a dead WN!");

    // The WN knows what to do.
    JSObject *wrapper = wn->GetSameCompartmentSecurityWrapper(cx);
    MOZ_ASSERT_IF(wrapper != obj && IsComponentsObject(js::UnwrapObject(obj)),
                  !Wrapper::wrapperHandler(wrapper)->isSafeToUnwrap());
    return wrapper;
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
    MOZ_ASSERT(!js::IsInnerObject(obj));
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
    JSObject *proto;
    if (!JS_GetPrototype(cx, obj, &proto))
        return NULL;
    JSObject *wrapperObj =
        Wrapper::New(cx, obj, proto, JS_GetGlobalForObject(cx, obj),
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
    JSObject *proto;
    if (!JS_GetPrototype(cx, obj, &proto))
        return NULL;
    JSObject *wrapperObj =
        Wrapper::New(cx, obj, proto, JS_GetGlobalForObject(cx, obj),
                     &FilteringWrapper<SameCompartmentSecurityWrapper, ComponentsObjectPolicy>::singleton);

    return wrapperObj;
}

JSObject *
WrapperFactory::WrapForSameCompartmentXray(JSContext *cx, JSObject *obj)
{
    // We should be same-compartment here.
    MOZ_ASSERT(js::IsObjectInContextCompartment(obj, cx));

    // Sort out what kind of Xray we can do. If we can't Xray, bail.
    XrayType type = GetXrayType(obj);
    if (type == NotXray)
        return NULL;

    // Select the appropriate proxy handler.
    Wrapper *wrapper = NULL;
    if (type == XrayForWrappedNative)
        wrapper = &SCPermissiveXrayXPCWN::singleton;
    else if (type == XrayForDOMObject)
        wrapper = &SCPermissiveXrayDOM::singleton;
    else
        MOZ_NOT_REACHED("Bad Xray type");

    // Make the Xray.
    JSObject *parent = JS_GetGlobalForObject(cx, obj);
    return Wrapper::New(cx, obj, NULL, parent, wrapper);
}


bool
WrapperFactory::XrayWrapperNotShadowing(JSObject *wrapper, jsid id)
{
    ResolvingId *rid = ResolvingId::getResolvingIdFromWrapper(wrapper);
    return rid->isXrayShadowing(id);
}

/*
 * Calls to JS_TransplantObject* should go through these helpers here so that
 * waivers get fixed up properly.
 */

static bool
FixWaiverAfterTransplant(JSContext *cx, JSObject *oldWaiver, JSObject *newobj)
{
    MOZ_ASSERT(Wrapper::wrapperHandler(oldWaiver) == &XrayWaiver);
    MOZ_ASSERT(!js::IsCrossCompartmentWrapper(newobj));

    // Create a waiver in the new compartment. We know there's not one already
    // because we _just_ transplanted, which means that |newobj| was either
    // created from scratch, or was previously cross-compartment wrapper (which
    // should have no waiver). CreateXrayWaiver asserts this.
    JSObject *newWaiver = WrapperFactory::CreateXrayWaiver(cx, newobj);
    if (!newWaiver)
        return false;

    // Update all the cross-compartment references to oldWaiver to point to
    // newWaiver.
    if (!js::RemapAllWrappersForObject(cx, oldWaiver, newWaiver))
        return false;

    // There should be no same-compartment references to oldWaiver, and we
    // just remapped all cross-compartment references. It's dead, so we can
    // remove it from the map.
    XPCWrappedNativeScope *scope = GetObjectScope(oldWaiver);
    JSObject *key = Wrapper::wrappedObject(oldWaiver);
    MOZ_ASSERT(scope->mWaiverWrapperMap->Find(key));
    scope->mWaiverWrapperMap->Remove(key);
    return true;
}

JSObject *
TransplantObject(JSContext *cx, JSObject *origobj, JSObject *target)
{
    JSObject *oldWaiver = WrapperFactory::GetXrayWaiver(origobj);
    JSObject *newIdentity = JS_TransplantObject(cx, origobj, target);
    if (!newIdentity || !oldWaiver)
       return newIdentity;

    if (!FixWaiverAfterTransplant(cx, oldWaiver, newIdentity))
        return NULL;
    return newIdentity;
}

JSObject *
TransplantObjectWithWrapper(JSContext *cx,
                            JSObject *origobj, JSObject *origwrapper,
                            JSObject *targetobj, JSObject *targetwrapper)
{
    JSObject *oldWaiver = WrapperFactory::GetXrayWaiver(origobj);
    JSObject *newSameCompartmentWrapper =
      js_TransplantObjectWithWrapper(cx, origobj, origwrapper, targetobj,
                                     targetwrapper);
    if (!newSameCompartmentWrapper || !oldWaiver)
        return newSameCompartmentWrapper;

    JSObject *newIdentity = Wrapper::wrappedObject(newSameCompartmentWrapper);
    MOZ_ASSERT(js::IsWrapper(newIdentity));
    if (!FixWaiverAfterTransplant(cx, oldWaiver, newIdentity))
        return NULL;
    return newSameCompartmentWrapper;
}

}
