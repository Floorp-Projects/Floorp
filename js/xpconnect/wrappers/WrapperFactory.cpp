/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WaiveXrayWrapper.h"
#include "FilteringWrapper.h"
#include "AddonWrapper.h"
#include "XrayWrapper.h"
#include "AccessCheck.h"
#include "XPCWrapper.h"
#include "ChromeObjectWrapper.h"
#include "WrapperFactory.h"

#include "xpcprivate.h"
#include "XPCMaps.h"
#include "mozilla/dom/BindingUtils.h"
#include "jsfriendapi.h"
#include "mozilla/jsipc/CrossProcessObjectWrappers.h"
#include "mozilla/Likely.h"
#include "mozilla/dom/ScriptSettings.h"
#include "nsContentUtils.h"
#include "nsXULAppAPI.h"

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
const Wrapper XrayWaiver(WrapperFactory::WAIVE_XRAY_WRAPPER_FLAG);

// When objects for which we waived the X-ray wrapper cross into
// chrome, we wrap them into a special cross-compartment wrapper
// that transitively extends the waiver to all properties we get
// off it.
const WaiveXrayWrapper WaiveXrayWrapper::singleton(0);

bool
WrapperFactory::IsCOW(JSObject* obj)
{
    return IsWrapper(obj) &&
           Wrapper::wrapperHandler(obj) == &ChromeObjectWrapper::singleton;
}

JSObject*
WrapperFactory::GetXrayWaiver(HandleObject obj)
{
    // Object should come fully unwrapped but outerized.
    MOZ_ASSERT(obj == UncheckedUnwrap(obj));
    MOZ_ASSERT(!js::IsWindow(obj));
    XPCWrappedNativeScope* scope = ObjectScope(obj);
    MOZ_ASSERT(scope);

    if (!scope->mWaiverWrapperMap)
        return nullptr;

    JSObject* xrayWaiver = scope->mWaiverWrapperMap->Find(obj);
    if (xrayWaiver)
        JS::ExposeObjectToActiveJS(xrayWaiver);

    return xrayWaiver;
}

JSObject*
WrapperFactory::CreateXrayWaiver(JSContext* cx, HandleObject obj)
{
    // The caller is required to have already done a lookup.
    // NB: This implictly performs the assertions of GetXrayWaiver.
    MOZ_ASSERT(!GetXrayWaiver(obj));
    XPCWrappedNativeScope* scope = ObjectScope(obj);

    JSAutoCompartment ac(cx, obj);
    JSObject* waiver = Wrapper::New(cx, obj, &XrayWaiver);
    if (!waiver)
        return nullptr;

    // Add the new waiver to the map. It's important that we only ever have
    // one waiver for the lifetime of the target object.
    if (!scope->mWaiverWrapperMap) {
        scope->mWaiverWrapperMap =
          JSObject2JSObjectMap::newMap(XPC_WRAPPER_MAP_LENGTH);
    }
    if (!scope->mWaiverWrapperMap->Add(cx, obj, waiver))
        return nullptr;
    return waiver;
}

JSObject*
WrapperFactory::WaiveXray(JSContext* cx, JSObject* objArg)
{
    RootedObject obj(cx, objArg);
    obj = UncheckedUnwrap(obj);
    MOZ_ASSERT(!js::IsWindow(obj));

    JSObject* waiver = GetXrayWaiver(obj);
    if (!waiver) {
        waiver = CreateXrayWaiver(cx, obj);
    }
    MOZ_ASSERT(!ObjectIsMarkedGray(waiver));
    return waiver;
}

/* static */ bool
WrapperFactory::AllowWaiver(JSCompartment* target, JSCompartment* origin)
{
    return CompartmentPrivate::Get(target)->allowWaivers &&
           AccessCheck::subsumes(target, origin);
}

/* static */ bool
WrapperFactory::AllowWaiver(JSObject* wrapper) {
    MOZ_ASSERT(js::IsCrossCompartmentWrapper(wrapper));
    return AllowWaiver(js::GetObjectCompartment(wrapper),
                       js::GetObjectCompartment(js::UncheckedUnwrap(wrapper)));
}

inline bool
ShouldWaiveXray(JSContext* cx, JSObject* originalObj)
{
    unsigned flags;
    (void) js::UncheckedUnwrap(originalObj, /* stopAtWindowProxy = */ true, &flags);

    // If the original object did not point through an Xray waiver, we're done.
    if (!(flags & WrapperFactory::WAIVE_XRAY_WRAPPER_FLAG))
        return false;

    // If the original object was not a cross-compartment wrapper, that means
    // that the caller explicitly created a waiver. Preserve it so that things
    // like WaiveXrayAndWrap work.
    if (!(flags & Wrapper::CROSS_COMPARTMENT))
        return true;

    // Otherwise, this is a case of explicitly passing a wrapper across a
    // compartment boundary. In that case, we only want to preserve waivers
    // in transactions between same-origin compartments.
    JSCompartment* oldCompartment = js::GetObjectCompartment(originalObj);
    JSCompartment* newCompartment = js::GetContextCompartment(cx);
    bool sameOrigin =
        AccessCheck::subsumesConsideringDomain(oldCompartment, newCompartment) &&
        AccessCheck::subsumesConsideringDomain(newCompartment, oldCompartment);
    return sameOrigin;
}

void
WrapperFactory::PrepareForWrapping(JSContext* cx, HandleObject scope,
                                   HandleObject objArg, HandleObject objectPassedToWrap,
                                   MutableHandleObject retObj)
{
    bool waive = ShouldWaiveXray(cx, objectPassedToWrap);
    RootedObject obj(cx, objArg);
    retObj.set(nullptr);
    // Outerize any raw inner objects at the entry point here, so that we don't
    // have to worry about them for the rest of the wrapping code.
    if (js::IsWindow(obj)) {
        JSAutoCompartment ac(cx, obj);
        obj = js::ToWindowProxyIfWindow(obj);
        MOZ_ASSERT(obj);
        // ToWindowProxyIfWindow can return a CCW if |obj| was a
        // navigated-away-from Window. Strip any CCWs.
        obj = js::UncheckedUnwrap(obj);
        if (JS_IsDeadWrapper(obj)) {
            JS_ReportErrorASCII(cx, "Can't wrap dead object");
            return;
        }
        MOZ_ASSERT(js::IsWindowProxy(obj));
        // We crossed a compartment boundary there, so may now have a gray
        // object.  This function is not allowed to return gray objects, so
        // don't do that.
        ExposeObjectToActiveJS(obj);
    }

    // If we've got a WindowProxy, there's nothing special that needs to be
    // done here, and we can move on to the next phase of wrapping. We handle
    // this case first to allow us to assert against wrappers below.
    if (js::IsWindowProxy(obj)) {
        retObj.set(waive ? WaiveXray(cx, obj) : obj);
        return;
    }

    // Here are the rules for wrapping:
    // We should never get a proxy here (the JS engine unwraps those for us).
    MOZ_ASSERT(!IsWrapper(obj));

    // Now, our object is ready to be wrapped, but several objects (notably
    // nsJSIIDs) have a wrapper per scope. If we are about to wrap one of
    // those objects in a security wrapper, then we need to hand back the
    // wrapper for the new scope instead. Also, global objects don't move
    // between scopes so for those we also want to return the wrapper. So...
    if (!IS_WN_REFLECTOR(obj) || JS_IsGlobalObject(obj)) {
        retObj.set(waive ? WaiveXray(cx, obj) : obj);
        return;
    }

    XPCWrappedNative* wn = XPCWrappedNative::Get(obj);

    JSAutoCompartment ac(cx, obj);
    XPCCallContext ccx(cx, obj);
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
            if (NS_FAILED(rv)) {
                retObj.set(waive ? WaiveXray(cx, obj) : obj);
                return;
            }

            // If the handed back scope differs from the passed-in scope and is in
            // a separate compartment, then this object is explicitly requesting
            // that we don't create a second JS object for it: create a security
            // wrapper.
            if (js::GetObjectCompartment(scope) != js::GetObjectCompartment(wrapScope)) {
                retObj.set(waive ? WaiveXray(cx, obj) : obj);
                return;
            }

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
                    retObj.set(waive ? WaiveXray(cx, obj) : obj);
                    return;
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
                retObj.set(waive ? WaiveXray(cx, obj) : obj);
                return;
            }
        }
    }

    // This public WrapNativeToJSVal API enters the compartment of 'wrapScope'
    // so we don't have to.
    RootedValue v(cx);
    nsresult rv =
        nsXPConnect::XPConnect()->WrapNativeToJSVal(cx, wrapScope, wn->Native(), nullptr,
                                                    &NS_GET_IID(nsISupports), false, &v);
    if (NS_FAILED(rv)) {
        return;
    }

    obj.set(&v.toObject());
    MOZ_ASSERT(IS_WN_REFLECTOR(obj), "bad object");
    MOZ_ASSERT(!ObjectIsMarkedGray(obj), "Should never return gray reflectors");

    // Because the underlying native didn't have a PreCreate hook, we had
    // to a new (or possibly pre-existing) XPCWN in our compartment.
    // This could be a problem for chrome code that passes XPCOM objects
    // across compartments, because the effects of QI would disappear across
    // compartments.
    //
    // So whenever we pull an XPCWN across compartments in this manner, we
    // give the destination object the union of the two native sets. We try
    // to do this cleverly in the common case to avoid too much overhead.
    XPCWrappedNative* newwn = XPCWrappedNative::Get(obj);
    RefPtr<XPCNativeSet> unionSet = XPCNativeSet::GetNewOrUsed(newwn->GetSet(),
                                                               wn->GetSet(), false);
    if (!unionSet) {
        return;
    }
    newwn->SetSet(unionSet.forget());

    retObj.set(waive ? WaiveXray(cx, obj) : obj);
}

#ifdef DEBUG
static void
DEBUG_CheckUnwrapSafety(HandleObject obj, const js::Wrapper* handler,
                        JSCompartment* origin, JSCompartment* target)
{
    if (AccessCheck::isChrome(target) || xpc::IsUniversalXPConnectEnabled(target)) {
        // If the caller is chrome (or effectively so), unwrap should always be allowed.
        MOZ_ASSERT(!handler->hasSecurityPolicy());
    } else if (CompartmentPrivate::Get(origin)->forcePermissiveCOWs) {
        // Similarly, if this is a privileged scope that has opted to make itself
        // accessible to the world (allowed only during automation), unwrap should
        // be allowed.
        MOZ_ASSERT(!handler->hasSecurityPolicy());
    } else {
        // Otherwise, it should depend on whether the target subsumes the origin.
        MOZ_ASSERT(handler->hasSecurityPolicy() == !AccessCheck::subsumesConsideringDomain(target, origin));
    }
}
#else
#define DEBUG_CheckUnwrapSafety(obj, handler, origin, target) {}
#endif

static const Wrapper*
SelectWrapper(bool securityWrapper, bool wantXrays, XrayType xrayType,
              bool waiveXrays, bool originIsXBLScope, JSObject* obj)
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
        else if (xrayType == XrayForDOMObject)
            return &PermissiveXrayDOM::singleton;
        else if (xrayType == XrayForJSObject)
            return &PermissiveXrayJS::singleton;
        MOZ_ASSERT(xrayType == XrayForOpaqueObject);
        return &PermissiveXrayOpaque::singleton;
    }

    // This is a security wrapper. Use the security versions and filter.
    if (xrayType == XrayForDOMObject && IdentifyCrossOriginObject(obj) != CrossOriginOpaque)
        return &FilteringWrapper<CrossOriginXrayWrapper,
                                 CrossOriginAccessiblePropertiesOnly>::singleton;

    // There's never any reason to expose other objects to non-subsuming actors.
    // Just use an opaque wrapper in these cases.
    //
    // In general, we don't want opaque function wrappers to be callable.
    // But in the case of XBL, we rely on content being able to invoke
    // functions exposed from the XBL scope. We could remove this exception,
    // if needed, by using ExportFunction to generate the content-side
    // representations of XBL methods.
    if (xrayType == XrayForJSObject && originIsXBLScope)
        return &FilteringWrapper<CrossCompartmentSecurityWrapper, OpaqueWithCall>::singleton;
    return &FilteringWrapper<CrossCompartmentSecurityWrapper, Opaque>::singleton;
}

static const Wrapper*
SelectAddonWrapper(JSContext* cx, HandleObject obj, const Wrapper* wrapper)
{
    JSAddonId* originAddon = JS::AddonIdOfObject(obj);
    JSAddonId* targetAddon = JS::AddonIdOfObject(JS::CurrentGlobalOrNull(cx));

    MOZ_ASSERT(AccessCheck::isChrome(JS::CurrentGlobalOrNull(cx)));
    MOZ_ASSERT(targetAddon);

    if (targetAddon == originAddon)
        return wrapper;

    // Add-on interposition only supports certain wrapper types, so we check if
    // we would have used one of the supported ones.
    if (wrapper == &CrossCompartmentWrapper::singleton)
        return &AddonWrapper<CrossCompartmentWrapper>::singleton;
    else if (wrapper == &PermissiveXrayXPCWN::singleton)
        return &AddonWrapper<PermissiveXrayXPCWN>::singleton;
    else if (wrapper == &PermissiveXrayDOM::singleton)
        return &AddonWrapper<PermissiveXrayDOM>::singleton;

    // |wrapper| is not supported for interposition, so we don't do it.
    return wrapper;
}

JSObject*
WrapperFactory::Rewrap(JSContext* cx, HandleObject existing, HandleObject obj)
{
    MOZ_ASSERT(!IsWrapper(obj) ||
               GetProxyHandler(obj) == &XrayWaiver ||
               js::IsWindowProxy(obj),
               "wrapped object passed to rewrap");
    MOZ_ASSERT(!XrayUtils::IsXPCWNHolderClass(JS_GetClass(obj)), "trying to wrap a holder");
    MOZ_ASSERT(!js::IsWindow(obj));
    MOZ_ASSERT(dom::IsJSAPIActive());

    // Compute the information we need to select the right wrapper.
    JSCompartment* origin = js::GetObjectCompartment(obj);
    JSCompartment* target = js::GetContextCompartment(cx);
    bool originIsChrome = AccessCheck::isChrome(origin);
    bool targetIsChrome = AccessCheck::isChrome(target);
    bool originSubsumesTarget = AccessCheck::subsumesConsideringDomain(origin, target);
    bool targetSubsumesOrigin = AccessCheck::subsumesConsideringDomain(target, origin);
    bool sameOrigin = targetSubsumesOrigin && originSubsumesTarget;
    XrayType xrayType = GetXrayType(obj);

    const Wrapper* wrapper;

    //
    // First, handle the special cases.
    //

    // If UniversalXPConnect is enabled, this is just some dumb mochitest. Use
    // a vanilla CCW.
    if (xpc::IsUniversalXPConnectEnabled(target)) {
        CrashIfNotInAutomation();
        wrapper = &CrossCompartmentWrapper::singleton;
    }

    // Let the SpecialPowers scope make its stuff easily accessible to content.
    else if (CompartmentPrivate::Get(origin)->forcePermissiveCOWs) {
        CrashIfNotInAutomation();
        wrapper = &CrossCompartmentWrapper::singleton;
    }

    // Special handling for chrome objects being exposed to content.
    else if (originIsChrome && !targetIsChrome) {
        // If this is a chrome function being exposed to content, we need to allow
        // call (but nothing else). We allow CPOWs that purport to be function's
        // here, but only in the content process.
        if ((IdentifyStandardInstance(obj) == JSProto_Function ||
            (jsipc::IsCPOW(obj) && JS::IsCallable(obj) &&
             XRE_IsContentProcess())))
        {
            wrapper = &FilteringWrapper<CrossCompartmentSecurityWrapper, OpaqueWithCall>::singleton;
        }

        // For Vanilla JSObjects exposed from chrome to content, we use a wrapper
        // that supports __exposedProps__. We'd like to get rid of these eventually,
        // but in their current form they don't cause much trouble.
        else if (IdentifyStandardInstance(obj) == JSProto_Object) {
            wrapper = &ChromeObjectWrapper::singleton;
        }

        // Otherwise we get an opaque wrapper.
        else {
            wrapper = &FilteringWrapper<CrossCompartmentSecurityWrapper, Opaque>::singleton;
        }
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
        bool sameOriginXrays = CompartmentPrivate::Get(origin)->wantXrays ||
                               CompartmentPrivate::Get(target)->wantXrays;
        bool wantXrays = !sameOrigin || sameOriginXrays;

        // If Xrays are warranted, the caller may waive them for non-security
        // wrappers (unless explicitly forbidden from doing so).
        bool waiveXrays = wantXrays && !securityWrapper &&
                          CompartmentPrivate::Get(target)->allowWaivers &&
                          HasWaiveXrayFlag(obj);

        // We have slightly different behavior for the case when the object
        // being wrapped is in an XBL scope.
        bool originIsContentXBLScope = IsContentXBLScope(origin);

        wrapper = SelectWrapper(securityWrapper, wantXrays, xrayType, waiveXrays,
                                originIsContentXBLScope, obj);

        // If we want to apply add-on interposition in the target compartment,
        // then we try to "upgrade" the wrapper to an interposing one.
        if (CompartmentPrivate::Get(target)->scope->HasInterposition())
            wrapper = SelectAddonWrapper(cx, obj, wrapper);
    }

    if (!targetSubsumesOrigin) {
        // Do a belt-and-suspenders check against exposing eval()/Function() to
        // non-subsuming content.
        if (JSFunction* fun = JS_GetObjectFunction(obj)) {
            if (JS_IsBuiltinEvalFunction(fun) || JS_IsBuiltinFunctionConstructor(fun)) {
                NS_WARNING("Trying to expose eval or Function to non-subsuming content!");
                wrapper = &FilteringWrapper<CrossCompartmentSecurityWrapper, Opaque>::singleton;
            }
        }
    }

    DEBUG_CheckUnwrapSafety(obj, wrapper, origin, target);

    if (existing)
        return Wrapper::Renew(cx, existing, obj, wrapper);

    return Wrapper::New(cx, obj, wrapper);
}

// Call WaiveXrayAndWrap when you have a JS object that you don't want to be
// wrapped in an Xray wrapper. cx->compartment is the compartment that will be
// using the returned object. If the object to be wrapped is already in the
// correct compartment, then this returns the unwrapped object.
bool
WrapperFactory::WaiveXrayAndWrap(JSContext* cx, MutableHandleValue vp)
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
WrapperFactory::WaiveXrayAndWrap(JSContext* cx, MutableHandleObject argObj)
{
    MOZ_ASSERT(argObj);
    RootedObject obj(cx, js::UncheckedUnwrap(argObj));
    MOZ_ASSERT(!js::IsWindow(obj));
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
    JSCompartment* target = js::GetContextCompartment(cx);
    JSCompartment* origin = js::GetObjectCompartment(obj);
    obj = AllowWaiver(target, origin) ? WaiveXray(cx, obj) : obj;
    if (!obj)
        return false;

    if (!JS_WrapObject(cx, &obj))
        return false;
    argObj.set(obj);
    return true;
}

/*
 * Calls to JS_TransplantObject* should go through these helpers here so that
 * waivers get fixed up properly.
 */

static bool
FixWaiverAfterTransplant(JSContext* cx, HandleObject oldWaiver, HandleObject newobj)
{
    MOZ_ASSERT(Wrapper::wrapperHandler(oldWaiver) == &XrayWaiver);
    MOZ_ASSERT(!js::IsCrossCompartmentWrapper(newobj));

    // Create a waiver in the new compartment. We know there's not one already
    // because we _just_ transplanted, which means that |newobj| was either
    // created from scratch, or was previously cross-compartment wrapper (which
    // should have no waiver). CreateXrayWaiver asserts this.
    JSObject* newWaiver = WrapperFactory::CreateXrayWaiver(cx, newobj);
    if (!newWaiver)
        return false;

    // Update all the cross-compartment references to oldWaiver to point to
    // newWaiver.
    if (!js::RemapAllWrappersForObject(cx, oldWaiver, newWaiver))
        return false;

    // There should be no same-compartment references to oldWaiver, and we
    // just remapped all cross-compartment references. It's dead, so we can
    // remove it from the map.
    XPCWrappedNativeScope* scope = ObjectScope(oldWaiver);
    JSObject* key = Wrapper::wrappedObject(oldWaiver);
    MOZ_ASSERT(scope->mWaiverWrapperMap->Find(key));
    scope->mWaiverWrapperMap->Remove(key);
    return true;
}

JSObject*
TransplantObject(JSContext* cx, JS::HandleObject origobj, JS::HandleObject target)
{
    RootedObject oldWaiver(cx, WrapperFactory::GetXrayWaiver(origobj));
    RootedObject newIdentity(cx, JS_TransplantObject(cx, origobj, target));
    if (!newIdentity || !oldWaiver)
       return newIdentity;

    if (!FixWaiverAfterTransplant(cx, oldWaiver, newIdentity))
        return nullptr;
    return newIdentity;
}

nsIGlobalObject*
NativeGlobal(JSObject* obj)
{
    obj = js::GetGlobalForObjectCrossCompartment(obj);

    // Every global needs to hold a native as its private or be a
    // WebIDL object with an nsISupports DOM object.
    MOZ_ASSERT((GetObjectClass(obj)->flags & (JSCLASS_PRIVATE_IS_NSISUPPORTS |
                                             JSCLASS_HAS_PRIVATE)) ||
               dom::UnwrapDOMObjectToISupports(obj));

    nsISupports* native = dom::UnwrapDOMObjectToISupports(obj);
    if (!native) {
        native = static_cast<nsISupports*>(js::GetObjectPrivate(obj));
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

} // namespace xpc
