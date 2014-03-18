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
#include "WrapperFactory.h"

#include "xpcprivate.h"
#include "XPCMaps.h"
#include "mozilla/dom/BindingUtils.h"
#include "jsfriendapi.h"
#include "mozilla/Likely.h"
#include "nsContentUtils.h"

using namespace JS;
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
WrapperFactory::GetXrayWaiver(HandleObject obj)
{
    // Object should come fully unwrapped but outerized.
    MOZ_ASSERT(obj == UncheckedUnwrap(obj));
    MOZ_ASSERT(!js::GetObjectClass(obj)->ext.outerObject);
    XPCWrappedNativeScope *scope = GetObjectScope(obj);
    MOZ_ASSERT(scope);

    if (!scope->mWaiverWrapperMap)
        return nullptr;

    JSObject* xrayWaiver = scope->mWaiverWrapperMap->Find(obj);
    if (xrayWaiver)
        JS::ExposeObjectToActiveJS(xrayWaiver);

    return xrayWaiver;
}

JSObject *
WrapperFactory::CreateXrayWaiver(JSContext *cx, HandleObject obj)
{
    // The caller is required to have already done a lookup.
    // NB: This implictly performs the assertions of GetXrayWaiver.
    MOZ_ASSERT(!GetXrayWaiver(obj));
    XPCWrappedNativeScope *scope = GetObjectScope(obj);

    JSAutoCompartment ac(cx, obj);
    JSObject *waiver = Wrapper::New(cx, obj,
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
    if (!scope->mWaiverWrapperMap->Add(cx, obj, waiver))
        return nullptr;
    return waiver;
}

JSObject *
WrapperFactory::WaiveXray(JSContext *cx, JSObject *objArg)
{
    RootedObject obj(cx, objArg);
    obj = UncheckedUnwrap(obj);
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
WrapperFactory::DoubleWrap(JSContext *cx, HandleObject obj, unsigned flags)
{
    if (flags & WrapperFactory::WAIVE_XRAY_WRAPPER_FLAG) {
        JSAutoCompartment ac(cx, obj);
        return WaiveXray(cx, obj);
    }
    return obj;
}

JSObject *
WrapperFactory::PrepareForWrapping(JSContext *cx, HandleObject scope,
                                   HandleObject objArg, unsigned flags)
{
    RootedObject obj(cx, objArg);
    // Outerize any raw inner objects at the entry point here, so that we don't
    // have to worry about them for the rest of the wrapping code.
    if (js::IsInnerObject(obj)) {
        JSAutoCompartment ac(cx, obj);
        obj = JS_ObjectToOuterObject(cx, obj);
        NS_ENSURE_TRUE(obj, nullptr);
        // The outerization hook wraps, which means that we can end up with a
        // CCW here if |obj| was a navigated-away-from inner. Strip any CCWs.
        obj = js::UncheckedUnwrap(obj);
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

    // If the object being wrapped is a prototype for a standard class and the
    // wrapper does not subsumes the wrappee, use the one from the content
    // compartment. This is generally safer all-around, and in the COW case this
    // lets us safely take advantage of things like .forEach() via the
    // ChromeObjectWrapper machinery.
    //
    // If the prototype chain of chrome object |obj| looks like this:
    //
    // obj => foo => bar => chromeWin.StandardClass.prototype
    //
    // The prototype chain of COW(obj) looks lke this:
    //
    // COW(obj) => COW(foo) => COW(bar) => contentWin.StandardClass.prototype
    //
    // NB: We now remap all non-subsuming access of standard prototypes.
    //
    // NB: We need to ignore domain here so that the security relationship we
    // compute here can't change over time. See the comment above the other
    // subsumes call below.
    bool subsumes = AccessCheck::subsumes(js::GetContextCompartment(cx),
                                          js::GetObjectCompartment(obj));
    XrayType xrayType = GetXrayType(obj);
    if (!subsumes && xrayType == NotXray) {
        JSProtoKey key = JSProto_Null;
        {
            JSAutoCompartment ac(cx, obj);
            key = IdentifyStandardPrototype(obj);
        }
        if (key != JSProto_Null) {
            RootedObject homeProto(cx);
            if (!JS_GetClassPrototype(cx, key, &homeProto))
                return nullptr;
            MOZ_ASSERT(homeProto);
            // No need to double-wrap here. We should never have waivers to
            // COWs.
            return homeProto;
        }
    }

    // Now, our object is ready to be wrapped, but several objects (notably
    // nsJSIIDs) have a wrapper per scope. If we are about to wrap one of
    // those objects in a security wrapper, then we need to hand back the
    // wrapper for the new scope instead. Also, global objects don't move
    // between scopes so for those we also want to return the wrapper. So...
    if (!IS_WN_REFLECTOR(obj) || !js::GetObjectParent(obj))
        return DoubleWrap(cx, obj, flags);

    XPCWrappedNative *wn = XPCWrappedNative::Get(obj);

    JSAutoCompartment ac(cx, obj);
    XPCCallContext ccx(JS_CALLER, cx, obj);
    RootedObject wrapScope(cx, scope);

    {
        if (NATIVE_HAS_FLAG(&ccx, WantPreCreate)) {
            // We have a precreate hook. This object might enforce that we only
            // ever create JS object for it.

            // Note: this penalizes objects that only have one wrapper, but are
            // being accessed across compartments. We would really prefer to
            // replace the above code with a test that says "do you only have one
            // wrapper?"
            nsresult rv = wn->GetScriptableInfo()->GetCallback()->
                PreCreate(wn->Native(), cx, scope, wrapScope.address());
            NS_ENSURE_SUCCESS(rv, DoubleWrap(cx, obj, flags));

            // If the handed back scope differs from the passed-in scope and is in
            // a separate compartment, then this object is explicitly requesting
            // that we don't create a second JS object for it: create a security
            // wrapper.
            if (js::GetObjectCompartment(scope) != js::GetObjectCompartment(wrapScope))
                return DoubleWrap(cx, obj, flags);

            RootedObject currentScope(cx, JS_GetGlobalForObject(cx, obj));
            if (MOZ_UNLIKELY(wrapScope != currentScope)) {
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
                RootedObject probe(cx);
                rv = wn->GetScriptableInfo()->GetCallback()->
                    PreCreate(wn->Native(), cx, currentScope, probe.address());

                // Check for case (2).
                if (probe != currentScope) {
                    MOZ_ASSERT(probe == wrapScope);
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
            if (!AccessCheck::isChrome(js::GetObjectCompartment(wrapScope)) &&
                 AccessCheck::subsumes(js::GetObjectCompartment(wrapScope),
                                       js::GetObjectCompartment(obj)))
            {
                return DoubleWrap(cx, obj, flags);
            }
        }
    }

    // This public WrapNativeToJSVal API enters the compartment of 'wrapScope'
    // so we don't have to.
    RootedValue v(cx);
    nsresult rv =
        nsXPConnect::XPConnect()->WrapNativeToJSVal(cx, wrapScope, wn->Native(), nullptr,
                                                    &NS_GET_IID(nsISupports), false, &v);
    NS_ENSURE_SUCCESS(rv, nullptr);

    obj.set(&v.toObject());
    MOZ_ASSERT(IS_WN_REFLECTOR(obj), "bad object");

    // Because the underlying native didn't have a PreCreate hook, we had
    // to a new (or possibly pre-existing) XPCWN in our compartment.
    // This could be a problem for chrome code that passes XPCOM objects
    // across compartments, because the effects of QI would disappear across
    // compartments.
    //
    // So whenever we pull an XPCWN across compartments in this manner, we
    // give the destination object the union of the two native sets. We try
    // to do this cleverly in the common case to avoid too much overhead.
    XPCWrappedNative *newwn = XPCWrappedNative::Get(obj);
    XPCNativeSet *unionSet = XPCNativeSet::GetNewOrUsed(newwn->GetSet(),
                                                        wn->GetSet(), false);
    if (!unionSet)
        return nullptr;
    newwn->SetSet(unionSet);

    return DoubleWrap(cx, obj, flags);
}

#ifdef DEBUG
static void
DEBUG_CheckUnwrapSafety(HandleObject obj, js::Wrapper *handler,
                        JSCompartment *origin, JSCompartment *target)
{
    if (AccessCheck::isChrome(target) || xpc::IsUniversalXPConnectEnabled(target)) {
        // If the caller is chrome (or effectively so), unwrap should always be allowed.
        MOZ_ASSERT(!handler->hasSecurityPolicy());
    } else if (AccessCheck::needsSystemOnlyWrapper(obj)) {
        // The rules for SOWs are complicated enough. Just skip double-checking them here.
    } else if (handler == &FilteringWrapper<CrossCompartmentSecurityWrapper, GentlyOpaque>::singleton) {
        // We explicitly use a SecurityWrapper to protect privileged callers from
        // less-privileged objects that they should never see. Skip the check in
        // this case.
    } else {
        // Otherwise, it should depend on whether the target subsumes the origin.
        MOZ_ASSERT(handler->hasSecurityPolicy() == !AccessCheck::subsumesConsideringDomain(target, origin));
    }
}
#else
#define DEBUG_CheckUnwrapSafety(obj, handler, origin, target) {}
#endif

static Wrapper *
SelectWrapper(bool securityWrapper, bool wantXrays, XrayType xrayType,
              bool waiveXrays, bool originIsXBLScope)
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
        // In general, we don't want opaque function wrappers to be callable.
        // But in the case of XBL, we rely on content being able to invoke
        // functions exposed from the XBL scope. We could remove this exception,
        // if needed, by using ExportFunction to generate the content-side
        // representations of XBL methods.
        else if (originIsXBLScope)
            return &FilteringWrapper<CrossCompartmentSecurityWrapper, OpaqueWithCall>::singleton;
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
WrapperFactory::Rewrap(JSContext *cx, HandleObject existing, HandleObject obj,
                       HandleObject wrappedProto, HandleObject parent,
                       unsigned flags)
{
    MOZ_ASSERT(!IsWrapper(obj) ||
               GetProxyHandler(obj) == &XrayWaiver ||
               js::GetObjectClass(obj)->ext.innerObject,
               "wrapped object passed to rewrap");
    MOZ_ASSERT(!XrayUtils::IsXPCWNHolderClass(JS_GetClass(obj)), "trying to wrap a holder");
    MOZ_ASSERT(!js::IsInnerObject(obj));
    // We sometimes end up here after nsContentUtils has been shut down but before
    // XPConnect has been shut down, so check the context stack the roundabout way.
    MOZ_ASSERT(XPCJSRuntime::Get()->GetJSContextStack()->Peek() == cx);

    // Compute the information we need to select the right wrapper.
    JSCompartment *origin = js::GetObjectCompartment(obj);
    JSCompartment *target = js::GetContextCompartment(cx);
    bool originIsChrome = AccessCheck::isChrome(origin);
    bool targetIsChrome = AccessCheck::isChrome(target);
    bool originSubsumesTarget = AccessCheck::subsumesConsideringDomain(origin, target);
    bool targetSubsumesOrigin = AccessCheck::subsumesConsideringDomain(target, origin);
    bool sameOrigin = targetSubsumesOrigin && originSubsumesTarget;
    XrayType xrayType = GetXrayType(obj);
    bool waiveXrayFlag = flags & WAIVE_XRAY_WRAPPER_FLAG;

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

    // If content is accessing NAC, we need a special filter, even if the
    // object is same origin. Note that we allow access to NAC for remote-XUL
    // whitelisted domains, since they don't have XBL scopes.
    } else if (AccessCheck::needsSystemOnlyWrapper(obj) &&
               xpc::AllowXBLScope(target) &&
               !(targetIsChrome || (targetSubsumesOrigin && nsContentUtils::IsCallerXBL())))
    {
        wrapper = &FilteringWrapper<CrossCompartmentSecurityWrapper, Opaque>::singleton;
    }

    // Normally, a non-xrayable non-waived content object that finds itself in
    // a privileged scope is wrapped with a CrossCompartmentWrapper, even though
    // the lack of a waiver _really_ should give it an opaque wrapper. This is
    // a bit too entrenched to change for content-chrome, but we can at least fix
    // it for XBL scopes.
    //
    // See bug 843829.
    else if (targetSubsumesOrigin && !originSubsumesTarget &&
             !waiveXrayFlag && xrayType == NotXray &&
             IsXBLScope(target))
    {
        wrapper = &FilteringWrapper<CrossCompartmentSecurityWrapper, GentlyOpaque>::singleton;
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
        bool waiveXrays = wantXrays && !securityWrapper && waiveXrayFlag;

        // We have slightly different behavior for the case when the object
        // being wrapped is in an XBL scope.
        bool originIsXBLScope = IsXBLScope(origin);

        wrapper = SelectWrapper(securityWrapper, wantXrays, xrayType, waiveXrays,
                                originIsXBLScope);
    }

    if (!targetSubsumesOrigin) {
        // Do a belt-and-suspenders check against exposing eval()/Function() to
        // non-subsuming content.
        JSFunction *fun = JS_GetObjectFunction(obj);
        if (fun) {
            if (JS_IsBuiltinEvalFunction(fun) || JS_IsBuiltinFunctionConstructor(fun)) {
                JS_ReportError(cx, "Permission denied to expose eval or Function to non-subsuming content");
                return nullptr;
            }
        }
    }

    DEBUG_CheckUnwrapSafety(obj, wrapper, origin, target);

    if (existing)
        return Wrapper::Renew(cx, existing, obj, wrapper);

    return Wrapper::New(cx, obj, parent, wrapper);
}

JSObject *
WrapperFactory::WrapForSameCompartment(JSContext *cx, HandleObject objArg)
{
    RootedObject obj(cx, objArg);
    MOZ_ASSERT(js::IsObjectInContextCompartment(obj, cx));

    // NB: The contract of WrapForSameCompartment says that |obj| may or may not
    // be a security wrapper. These checks implicitly handle the security
    // wrapper case.

    // Outerize if necessary. This, in combination with the check in
    // PrepareForUnwrapping, means that calling JS_Wrap* always outerizes.
    obj = JS_ObjectToOuterObject(cx, obj);
    NS_ENSURE_TRUE(obj, nullptr);

    // The method below is a no-op for non-DOM objects.
    dom::GetSameCompartmentWrapperForDOMBinding(*obj.address());
    return obj;
}

// Call WaiveXrayAndWrap when you have a JS object that you don't want to be
// wrapped in an Xray wrapper. cx->compartment is the compartment that will be
// using the returned object. If the object to be wrapped is already in the
// correct compartment, then this returns the unwrapped object.
bool
WrapperFactory::WaiveXrayAndWrap(JSContext *cx, MutableHandleValue vp)
{
    if (vp.isPrimitive())
        return JS_WrapValue(cx, vp);

    RootedObject obj(cx, &vp.toObject());
    if (!WaiveXrayAndWrap(cx, &obj))
        return false;

    vp.setObject(*obj);
    return true;
}

bool
WrapperFactory::WaiveXrayAndWrap(JSContext *cx, MutableHandleObject argObj)
{
    MOZ_ASSERT(argObj);
    RootedObject obj(cx, js::UncheckedUnwrap(argObj));
    MOZ_ASSERT(!js::IsInnerObject(obj));
    if (js::IsObjectInContextCompartment(obj, cx)) {
        argObj.set(obj);
        return true;
    }

    // Even though waivers have no effect on access by scopes that don't subsume
    // the underlying object, good defense-in-depth dictates that we should avoid
    // handing out waivers to callers that can't use them. The transitive waiving
    // machinery unconditionally calls WaiveXrayAndWrap on return values from
    // waived functions, even though the return value might be not be same-origin
    // with the function. So if we find ourselves trying to create a waiver for
    // |cx|, we should check whether the caller has any business with waivers
    // to things in |obj|'s compartment.
    JSCompartment *target = js::GetContextCompartment(cx);
    JSCompartment *origin = js::GetObjectCompartment(obj);
    obj = AccessCheck::subsumes(target, origin) ? WaiveXray(cx, obj) : obj;
    if (!obj)
        return false;

    if (!JS_WrapObject(cx, &obj))
        return false;
    argObj.set(obj);
    return true;
}

JSObject *
WrapperFactory::WrapSOWObject(JSContext *cx, JSObject *objArg)
{
    RootedObject obj(cx, objArg);

    // If we're not allowing XBL scopes, that means we're running as a remote
    // XUL domain, in which we can't have SOWs. We should never be called in
    // that case.
    MOZ_ASSERT(xpc::AllowXBLScope(js::GetContextCompartment(cx)));
    JSObject *wrapperObj =
        Wrapper::New(cx, obj, JS_GetGlobalForObject(cx, obj),
                     &FilteringWrapper<SameCompartmentSecurityWrapper,
                     Opaque>::singleton);
    return wrapperObj;
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
FixWaiverAfterTransplant(JSContext *cx, HandleObject oldWaiver, HandleObject newobj)
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
TransplantObject(JSContext *cx, JS::HandleObject origobj, JS::HandleObject target)
{
    RootedObject oldWaiver(cx, WrapperFactory::GetXrayWaiver(origobj));
    RootedObject newIdentity(cx, JS_TransplantObject(cx, origobj, target));
    if (!newIdentity || !oldWaiver)
       return newIdentity;

    if (!FixWaiverAfterTransplant(cx, oldWaiver, newIdentity))
        return nullptr;
    return newIdentity;
}

nsIGlobalObject *
GetNativeForGlobal(JSObject *obj)
{
    MOZ_ASSERT(JS_IsGlobalObject(obj));
    if (!MaybeGetObjectScope(obj))
        return nullptr;

    // Every global needs to hold a native as its private or be a
    // WebIDL object with an nsISupports DOM object.
    MOZ_ASSERT((GetObjectClass(obj)->flags & (JSCLASS_PRIVATE_IS_NSISUPPORTS |
                                             JSCLASS_HAS_PRIVATE)) ||
               dom::UnwrapDOMObjectToISupports(obj));

    nsISupports *native = dom::UnwrapDOMObjectToISupports(obj);
    if (!native) {
        native = static_cast<nsISupports *>(js::GetObjectPrivate(obj));
        MOZ_ASSERT(native);

        // In some cases (like for windows) it is a wrapped native,
        // in other cases (sandboxes, backstage passes) it's just
        // a direct pointer to the native. If it's a wrapped native
        // let's unwrap it first.
        if (nsCOMPtr<nsIXPConnectWrappedNative> wn = do_QueryInterface(native)) {
            native = wn->Native();
        }
    }

    nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(native);
    MOZ_ASSERT(global, "Native held by global needs to implement nsIGlobalObject!");

    return global;
}

}
