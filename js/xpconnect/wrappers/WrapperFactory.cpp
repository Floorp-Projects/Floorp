/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
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
#include "mozilla/jsipc/CrossProcessObjectWrappers.h"
#include "mozilla/Likely.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/MaybeCrossOriginObject.h"
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

bool WrapperFactory::IsCOW(JSObject* obj) {
  return IsWrapper(obj) &&
         Wrapper::wrapperHandler(obj) == &ChromeObjectWrapper::singleton;
}

JSObject* WrapperFactory::GetXrayWaiver(HandleObject obj) {
  // Object should come fully unwrapped but outerized.
  MOZ_ASSERT(obj == UncheckedUnwrap(obj));
  MOZ_ASSERT(!js::IsWindow(obj));
  XPCWrappedNativeScope* scope = ObjectScope(obj);
  MOZ_ASSERT(scope);

  if (!scope->mWaiverWrapperMap) {
    return nullptr;
  }

  return scope->mWaiverWrapperMap->Find(obj);
}

JSObject* WrapperFactory::CreateXrayWaiver(JSContext* cx, HandleObject obj,
                                           bool allowExisting) {
  // The caller is required to have already done a lookup, unless it's
  // trying to replace an existing waiver.
  // NB: This implictly performs the assertions of GetXrayWaiver.
  MOZ_ASSERT(bool(GetXrayWaiver(obj)) == allowExisting);
  XPCWrappedNativeScope* scope = ObjectScope(obj);

  JSAutoRealm ar(cx, obj);
  JSObject* waiver = Wrapper::New(cx, obj, &XrayWaiver);
  if (!waiver) {
    return nullptr;
  }

  // Add the new waiver to the map. It's important that we only ever have
  // one waiver for the lifetime of the target object.
  if (!scope->mWaiverWrapperMap) {
    scope->mWaiverWrapperMap =
        JSObject2JSObjectMap::newMap(XPC_WRAPPER_MAP_LENGTH);
  }
  if (!scope->mWaiverWrapperMap->Add(cx, obj, waiver)) {
    return nullptr;
  }
  return waiver;
}

JSObject* WrapperFactory::WaiveXray(JSContext* cx, JSObject* objArg) {
  RootedObject obj(cx, objArg);
  obj = UncheckedUnwrap(obj);
  MOZ_ASSERT(!js::IsWindow(obj));

  JSObject* waiver = GetXrayWaiver(obj);
  if (!waiver) {
    waiver = CreateXrayWaiver(cx, obj);
  }
  JS::AssertObjectIsNotGray(waiver);
  return waiver;
}

/* static */
bool WrapperFactory::AllowWaiver(JS::Compartment* target,
                                 JS::Compartment* origin) {
  return CompartmentPrivate::Get(target)->allowWaivers &&
         CompartmentOriginInfo::Subsumes(target, origin);
}

/* static */
bool WrapperFactory::AllowWaiver(JSObject* wrapper) {
  MOZ_ASSERT(js::IsCrossCompartmentWrapper(wrapper));
  return AllowWaiver(js::GetObjectCompartment(wrapper),
                     js::GetObjectCompartment(js::UncheckedUnwrap(wrapper)));
}

inline bool ShouldWaiveXray(JSContext* cx, JSObject* originalObj) {
  unsigned flags;
  (void)js::UncheckedUnwrap(originalObj, /* stopAtWindowProxy = */ true,
                            &flags);

  // If the original object did not point through an Xray waiver, we're done.
  if (!(flags & WrapperFactory::WAIVE_XRAY_WRAPPER_FLAG)) {
    return false;
  }

  // If the original object was not a cross-compartment wrapper, that means
  // that the caller explicitly created a waiver. Preserve it so that things
  // like WaiveXrayAndWrap work.
  if (!(flags & Wrapper::CROSS_COMPARTMENT)) {
    return true;
  }

  // Otherwise, this is a case of explicitly passing a wrapper across a
  // compartment boundary. In that case, we only want to preserve waivers
  // in transactions between same-origin compartments.
  JS::Compartment* oldCompartment = js::GetObjectCompartment(originalObj);
  JS::Compartment* newCompartment = js::GetContextCompartment(cx);
  bool sameOrigin = false;
  if (OriginAttributes::IsRestrictOpenerAccessForFPI()) {
    sameOrigin =
        CompartmentOriginInfo::Subsumes(oldCompartment, newCompartment) &&
        CompartmentOriginInfo::Subsumes(newCompartment, oldCompartment);
  } else {
    sameOrigin = CompartmentOriginInfo::SubsumesIgnoringFPD(oldCompartment,
                                                            newCompartment) &&
                 CompartmentOriginInfo::SubsumesIgnoringFPD(newCompartment,
                                                            oldCompartment);
  }
  return sameOrigin;
}

void WrapperFactory::PrepareForWrapping(JSContext* cx, HandleObject scope,
                                        HandleObject objArg,
                                        HandleObject objectPassedToWrap,
                                        MutableHandleObject retObj) {
  // The JS engine calls ToWindowProxyIfWindow and deals with dead wrappers.
  MOZ_ASSERT(!js::IsWindow(objArg));
  MOZ_ASSERT(!JS_IsDeadWrapper(objArg));

  bool waive = ShouldWaiveXray(cx, objectPassedToWrap);
  RootedObject obj(cx, objArg);
  retObj.set(nullptr);

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

  JSAutoRealm ar(cx, obj);
  XPCCallContext ccx(cx, obj);
  RootedObject wrapScope(cx, scope);

  {
    if (ccx.GetScriptable() && ccx.GetScriptable()->WantPreCreate()) {
      // We have a precreate hook. This object might enforce that we only
      // ever create JS object for it.

      // Note: this penalizes objects that only have one wrapper, but are
      // being accessed across compartments. We would really prefer to
      // replace the above code with a test that says "do you only have one
      // wrapper?"
      nsresult rv = wn->GetScriptable()->PreCreate(wn->Native(), cx, scope,
                                                   wrapScope.address());
      if (NS_FAILED(rv)) {
        retObj.set(waive ? WaiveXray(cx, obj) : obj);
        return;
      }

      // If the handed back scope differs from the passed-in scope and is in
      // a separate compartment, then this object is explicitly requesting
      // that we don't create a second JS object for it: create a security
      // wrapper.
      if (js::GetObjectCompartment(scope) !=
          js::GetObjectCompartment(wrapScope)) {
        retObj.set(waive ? WaiveXray(cx, obj) : obj);
        return;
      }

      RootedObject currentScope(cx, JS::GetNonCCWObjectGlobal(obj));
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
        rv = wn->GetScriptable()->PreCreate(wn->Native(), cx, currentScope,
                                            probe.address());

        // Check for case (2).
        if (probe != currentScope) {
          MOZ_ASSERT(probe == wrapScope);
          retObj.set(waive ? WaiveXray(cx, obj) : obj);
          return;
        }

        // Ok, must be case (1). Fall through and create a new wrapper.
      }

      // Nasty hack for late-breaking bug 781476. This will confuse identity
      // checks, but it's probably better than any of our alternatives.
      //
      // Note: We have to ignore domain here. The JS engine assumes that, given
      // a compartment c, if c->wrap(x) returns a cross-compartment wrapper at
      // time t0, it will also return a cross-compartment wrapper for any time
      // t1 > t0 unless an explicit transplant is performed. In particular,
      // wrapper recomputation assumes that recomputing a wrapper will always
      // result in a wrapper.
      //
      // This doesn't actually pose a security issue, because we'll still
      // compute the correct (opaque) wrapper for the object below given the
      // security characteristics of the two compartments.
      if (!AccessCheck::isChrome(js::GetObjectCompartment(wrapScope)) &&
          CompartmentOriginInfo::Subsumes(js::GetObjectCompartment(wrapScope),
                                          js::GetObjectCompartment(obj))) {
        retObj.set(waive ? WaiveXray(cx, obj) : obj);
        return;
      }
    }
  }

  // This public WrapNativeToJSVal API enters the compartment of 'wrapScope'
  // so we don't have to.
  RootedValue v(cx);
  nsresult rv = nsXPConnect::XPConnect()->WrapNativeToJSVal(
      cx, wrapScope, wn->Native(), nullptr, &NS_GET_IID(nsISupports), false,
      &v);
  if (NS_FAILED(rv)) {
    return;
  }

  obj.set(&v.toObject());
  MOZ_ASSERT(IS_WN_REFLECTOR(obj), "bad object");
  JS::AssertObjectIsNotGray(obj);  // We should never return gray reflectors.

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
  RefPtr<XPCNativeSet> unionSet =
      XPCNativeSet::GetNewOrUsed(cx, newwn->GetSet(), wn->GetSet(), false);
  if (!unionSet) {
    return;
  }
  newwn->SetSet(unionSet.forget());

  retObj.set(waive ? WaiveXray(cx, obj) : obj);
}

// This check is completely symmetric, so we don't need to keep track of origin
// vs target here.  Two compartments may have had transparent CCWs between them
// only if they are same-origin (ignoring document.domain) or have both had
// document.domain set at some point and are same-site.  In either case they
// will have the same SiteIdentifier, so check that first.
static bool CompartmentsMayHaveHadTransparentCCWs(
    CompartmentPrivate* private1, CompartmentPrivate* private2) {
  auto& info1 = private1->originInfo;
  auto& info2 = private2->originInfo;

  if (!info1.SiteRef().Equals(info2.SiteRef())) {
    return false;
  }

  return info1.GetPrincipalIgnoringDocumentDomain()->FastEquals(
             info2.GetPrincipalIgnoringDocumentDomain()) ||
         (info1.HasChangedDocumentDomain() && info2.HasChangedDocumentDomain());
}

#ifdef DEBUG
static void DEBUG_CheckUnwrapSafety(HandleObject obj,
                                    const js::Wrapper* handler,
                                    JS::Realm* origin, JS::Realm* target) {
  JS::Compartment* targetCompartment = JS::GetCompartmentForRealm(target);
  if (!js::AllowNewWrapper(targetCompartment, obj)) {
    // The JS engine should have returned a dead wrapper in this case and we
    // shouldn't even get here.
    MOZ_ASSERT_UNREACHABLE("CheckUnwrapSafety called for a dead wrapper");
  } else if (AccessCheck::isChrome(targetCompartment) ||
             xpc::IsUniversalXPConnectEnabled(targetCompartment)) {
    // If the caller is chrome (or effectively so), unwrap should always be
    // allowed, but we might have a CrossOriginObjectWrapper here which allows
    // it dynamically.
    MOZ_ASSERT(!handler->hasSecurityPolicy() ||
               handler == &CrossOriginObjectWrapper::singleton);
  } else if (RealmPrivate::Get(origin)->forcePermissiveCOWs) {
    // Similarly, if this is a privileged scope that has opted to make itself
    // accessible to the world (allowed only during automation), unwrap should
    // be allowed.  Again, it might be allowed dynamically.
    MOZ_ASSERT(!handler->hasSecurityPolicy() ||
               handler == &CrossOriginObjectWrapper::singleton);
  } else {
    // Otherwise, it should depend on whether the target subsumes the origin.
    bool subsumes =
        (OriginAttributes::IsRestrictOpenerAccessForFPI()
             ? AccessCheck::subsumesConsideringDomain(target, origin)
             : AccessCheck::subsumesConsideringDomainIgnoringFPD(target,
                                                                 origin));
    if (!subsumes) {
      // If the target (which is where the wrapper lives) does not subsume the
      // origin (which is where the wrapped object lives), then we should
      // generally have a security check on the wrapper here.  There is one
      // exception, though: things that used to be same-origin and then stopped
      // due to document.domain changes.  In that case we will have a
      // transparent cross-compartment wrapper here even though "subsumes" is no
      // longer true.
      CompartmentPrivate* originCompartmentPrivate =
          CompartmentPrivate::Get(origin);
      CompartmentPrivate* targetCompartmentPrivate =
          CompartmentPrivate::Get(target);
      if (!originCompartmentPrivate->wantXrays &&
          !targetCompartmentPrivate->wantXrays &&
          CompartmentsMayHaveHadTransparentCCWs(originCompartmentPrivate,
                                                targetCompartmentPrivate)) {
        // We should have a transparent CCW, unless we have a cross-origin
        // object, in which case it will be a CrossOriginObjectWrapper.
        MOZ_ASSERT(handler == &CrossCompartmentWrapper::singleton ||
                   handler == &CrossOriginObjectWrapper::singleton);
      } else {
        MOZ_ASSERT(handler->hasSecurityPolicy());
      }
    } else {
      // Even if target subsumes origin, we might have a wrapper with a security
      // policy here, if it happens to be a CrossOriginObjectWrapper.
      MOZ_ASSERT(!handler->hasSecurityPolicy() ||
                 handler == &CrossOriginObjectWrapper::singleton);
    }
  }
}
#else
#  define DEBUG_CheckUnwrapSafety(obj, handler, origin, target) \
    {}
#endif

const CrossOriginObjectWrapper CrossOriginObjectWrapper::singleton;

bool CrossOriginObjectWrapper::dynamicCheckedUnwrapAllowed(
    HandleObject obj, JSContext* cx) const {
  MOZ_ASSERT(js::GetProxyHandler(obj) == this,
             "Why are we getting called for some random object?");
  JSObject* target = wrappedObject(obj);
  return dom::MaybeCrossOriginObjectMixins::IsPlatformObjectSameOrigin(cx,
                                                                       target);
}

static const Wrapper* SelectWrapper(bool securityWrapper, XrayType xrayType,
                                    bool waiveXrays, JSObject* obj) {
  // Waived Xray uses a modified CCW that has transparent behavior but
  // transitively waives Xrays on arguments.
  if (waiveXrays) {
    MOZ_ASSERT(!securityWrapper);
    return &WaiveXrayWrapper::singleton;
  }

  // If we don't want or can't use Xrays, select a wrapper that's either
  // entirely transparent or entirely opaque.
  if (xrayType == NotXray) {
    if (!securityWrapper) {
      return &CrossCompartmentWrapper::singleton;
    }
    return &FilteringWrapper<CrossCompartmentSecurityWrapper,
                             Opaque>::singleton;
  }

  // Ok, we're using Xray. If this isn't a security wrapper, use the permissive
  // version and skip the filter.
  if (!securityWrapper) {
    if (xrayType == XrayForDOMObject) {
      return &PermissiveXrayDOM::singleton;
    } else if (xrayType == XrayForJSObject) {
      return &PermissiveXrayJS::singleton;
    }
    MOZ_ASSERT(xrayType == XrayForOpaqueObject);
    return &PermissiveXrayOpaque::singleton;
  }

  // There's never any reason to expose other objects to non-subsuming actors.
  // Just use an opaque wrapper in these cases.
  //
  // In general, we don't want opaque function wrappers to be callable.
  // But in the case of XBL, we rely on content being able to invoke
  // functions exposed from the XBL scope. We could remove this exception,
  // if needed, by using ExportFunction to generate the content-side
  // representations of XBL methods.
  if (xrayType == XrayForJSObject && IsInContentXBLScope(obj)) {
    return &FilteringWrapper<CrossCompartmentSecurityWrapper,
                             OpaqueWithCall>::singleton;
  }
  return &FilteringWrapper<CrossCompartmentSecurityWrapper, Opaque>::singleton;
}

JSObject* WrapperFactory::Rewrap(JSContext* cx, HandleObject existing,
                                 HandleObject obj) {
  MOZ_ASSERT(!IsWrapper(obj) || GetProxyHandler(obj) == &XrayWaiver ||
                 js::IsWindowProxy(obj),
             "wrapped object passed to rewrap");
  MOZ_ASSERT(!js::IsWindow(obj));
  MOZ_ASSERT(dom::IsJSAPIActive());

  // Compute the information we need to select the right wrapper.
  JS::Realm* origin = js::GetNonCCWObjectRealm(obj);
  JS::Realm* target = js::GetContextRealm(cx);
  MOZ_ASSERT(target, "Why is our JSContext not in a Realm?");
  bool originIsChrome = AccessCheck::isChrome(origin);
  bool targetIsChrome = AccessCheck::isChrome(target);
  bool originSubsumesTarget =
      OriginAttributes::IsRestrictOpenerAccessForFPI()
          ? AccessCheck::subsumesConsideringDomain(origin, target)
          : AccessCheck::subsumesConsideringDomainIgnoringFPD(origin, target);
  bool targetSubsumesOrigin =
      OriginAttributes::IsRestrictOpenerAccessForFPI()
          ? AccessCheck::subsumesConsideringDomain(target, origin)
          : AccessCheck::subsumesConsideringDomainIgnoringFPD(target, origin);
  bool sameOrigin = targetSubsumesOrigin && originSubsumesTarget;

  const Wrapper* wrapper;

  CompartmentPrivate* originCompartmentPrivate =
      CompartmentPrivate::Get(origin);
  CompartmentPrivate* targetCompartmentPrivate =
      CompartmentPrivate::Get(target);

  RealmPrivate* originRealmPrivate = RealmPrivate::Get(origin);

  // Track whether we decided to use a transparent wrapper because of
  // document.domain usage, so we don't override that decision.
  bool isTransparentWrapperDueToDocumentDomain = false;

  //
  // First, handle the special cases.
  //

  // If UniversalXPConnect is enabled, this is just some dumb mochitest. Use
  // a vanilla CCW.
  if (targetCompartmentPrivate->universalXPConnectEnabled) {
    CrashIfNotInAutomation();
    wrapper = &CrossCompartmentWrapper::singleton;
  }

  // Let the SpecialPowers scope make its stuff easily accessible to content.
  else if (originRealmPrivate->forcePermissiveCOWs) {
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
          XRE_IsContentProcess()))) {
      wrapper = &FilteringWrapper<CrossCompartmentSecurityWrapper,
                                  OpaqueWithCall>::singleton;
    }

    // For vanilla JSObjects exposed from chrome to content, we use a wrapper
    // that fails silently in a few cases. We'd like to get rid of this
    // eventually, but in their current form they don't cause much trouble.
    else if (IdentifyStandardInstance(obj) == JSProto_Object) {
      wrapper = &ChromeObjectWrapper::singleton;
    }

    // Otherwise we get an opaque wrapper.
    else {
      wrapper =
          &FilteringWrapper<CrossCompartmentSecurityWrapper, Opaque>::singleton;
    }
  }

  // Special handling for the web's cross-origin objects (WindowProxy and
  // Location).  We only need or want to do this in web-like contexts, where all
  // security relationships are symmetric and there are no forced Xrays.
  else if (originSubsumesTarget == targetSubsumesOrigin &&
           // Check for the more rare case of cross-origin objects before doing
           // the more-likely-to-pass checks for wantXrays.
           IsCrossOriginAccessibleObject(obj) &&
           (!targetSubsumesOrigin || (!originCompartmentPrivate->wantXrays &&
                                      !targetCompartmentPrivate->wantXrays))) {
    wrapper = &CrossOriginObjectWrapper::singleton;
  }

  // Special handling for other web objects.  Again, we only want this in
  // web-like contexts (symmetric security relationships, no forced Xrays).  In
  // this situation, if the two compartments may ever have had transparent CCWs
  // between them, we want to keep using transparent CCWs.
  else if (originSubsumesTarget == targetSubsumesOrigin &&
           !originCompartmentPrivate->wantXrays &&
           !targetCompartmentPrivate->wantXrays &&
           CompartmentsMayHaveHadTransparentCCWs(originCompartmentPrivate,
                                                 targetCompartmentPrivate)) {
    isTransparentWrapperDueToDocumentDomain = true;
    wrapper = &CrossCompartmentWrapper::singleton;
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
    bool sameOriginXrays = originCompartmentPrivate->wantXrays ||
                           targetCompartmentPrivate->wantXrays;
    bool wantXrays = !sameOrigin || sameOriginXrays;

    XrayType xrayType = wantXrays ? GetXrayType(obj) : NotXray;

    // If Xrays are warranted, the caller may waive them for non-security
    // wrappers (unless explicitly forbidden from doing so).
    bool waiveXrays = wantXrays && !securityWrapper &&
                      targetCompartmentPrivate->allowWaivers &&
                      HasWaiveXrayFlag(obj);

    wrapper = SelectWrapper(securityWrapper, xrayType, waiveXrays, obj);
  }

  if (!targetSubsumesOrigin && !originRealmPrivate->forcePermissiveCOWs &&
      !isTransparentWrapperDueToDocumentDomain) {
    // Do a belt-and-suspenders check against exposing eval()/Function() to
    // non-subsuming content.  But don't worry about doing it in the
    // SpecialPowers case.
    if (JSFunction* fun = JS_GetObjectFunction(obj)) {
      if (JS_IsBuiltinEvalFunction(fun) ||
          JS_IsBuiltinFunctionConstructor(fun)) {
        NS_WARNING(
            "Trying to expose eval or Function to non-subsuming content!");
        wrapper = &FilteringWrapper<CrossCompartmentSecurityWrapper,
                                    Opaque>::singleton;
      }
    }
  }

  DEBUG_CheckUnwrapSafety(obj, wrapper, origin, target);

  if (existing) {
    return Wrapper::Renew(existing, obj, wrapper);
  }

  return Wrapper::New(cx, obj, wrapper);
}

// Call WaiveXrayAndWrap when you have a JS object that you don't want to be
// wrapped in an Xray wrapper. cx->compartment is the compartment that will be
// using the returned object. If the object to be wrapped is already in the
// correct compartment, then this returns the unwrapped object.
bool WrapperFactory::WaiveXrayAndWrap(JSContext* cx, MutableHandleValue vp) {
  if (vp.isPrimitive()) {
    return JS_WrapValue(cx, vp);
  }

  RootedObject obj(cx, &vp.toObject());
  if (!WaiveXrayAndWrap(cx, &obj)) {
    return false;
  }

  vp.setObject(*obj);
  return true;
}

bool WrapperFactory::WaiveXrayAndWrap(JSContext* cx,
                                      MutableHandleObject argObj) {
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
  JS::Compartment* target = js::GetContextCompartment(cx);
  JS::Compartment* origin = js::GetObjectCompartment(obj);
  obj = AllowWaiver(target, origin) ? WaiveXray(cx, obj) : obj;
  if (!obj) {
    return false;
  }

  if (!JS_WrapObject(cx, &obj)) {
    return false;
  }
  argObj.set(obj);
  return true;
}

/*
 * Calls to JS_TransplantObject* should go through these helpers here so that
 * waivers get fixed up properly.
 */

static bool FixWaiverAfterTransplant(JSContext* cx, HandleObject oldWaiver,
                                     HandleObject newobj,
                                     bool crossCompartmentTransplant) {
  MOZ_ASSERT(Wrapper::wrapperHandler(oldWaiver) == &XrayWaiver);
  MOZ_ASSERT(!js::IsCrossCompartmentWrapper(newobj));

  if (crossCompartmentTransplant) {
    // If the new compartment has a CCW for oldWaiver, nuke this CCW. This
    // prevents confusing RemapAllWrappersForObject: it would call RemapWrapper
    // with two same-compartment objects (the CCW and the new waiver).
    //
    // This can happen when loading a chrome page in a content frame and there
    // exists a CCW from the chrome compartment to oldWaiver wrapping the window
    // we just transplanted:
    //
    // Compartment 1  |  Compartment 2
    // ----------------------------------------
    // CCW1 -----------> oldWaiver --> CCW2 --+
    // newWaiver                              |
    // WindowProxy <--------------------------+
    js::NukeCrossCompartmentWrapperIfExists(
        cx, js::GetObjectCompartment(newobj), oldWaiver);
  } else {
    // We kept the same object identity, so the waiver should be a
    // waiver for our object, just in the wrong Realm.
    MOZ_ASSERT(newobj == Wrapper::wrappedObject(oldWaiver));
  }

  // Create a waiver in the new compartment. We know there's not one already in
  // the crossCompartmentTransplant case because we _just_ transplanted, which
  // means that |newobj| was either created from scratch, or was previously
  // cross-compartment wrapper (which should have no waiver). On the other hand,
  // in the !crossCompartmentTransplant case we know one already exists.
  // CreateXrayWaiver asserts all this.
  JSObject* newWaiver = WrapperFactory::CreateXrayWaiver(
      cx, newobj, /* allowExisting = */ !crossCompartmentTransplant);
  if (!newWaiver) {
    return false;
  }

  if (!crossCompartmentTransplant) {
    // CreateXrayWaiver should have updated the map to point to the new waiver.
    MOZ_ASSERT(WrapperFactory::GetXrayWaiver(newobj) == newWaiver);
  }

  // Update all the cross-compartment references to oldWaiver to point to
  // newWaiver.
  if (!js::RemapAllWrappersForObject(cx, oldWaiver, newWaiver)) {
    return false;
  }

  if (crossCompartmentTransplant) {
    // There should be no same-compartment references to oldWaiver, and we
    // just remapped all cross-compartment references. It's dead, so we can
    // remove it from the map.
    XPCWrappedNativeScope* scope = ObjectScope(oldWaiver);
    JSObject* key = Wrapper::wrappedObject(oldWaiver);
    MOZ_ASSERT(scope->mWaiverWrapperMap->Find(key));
    scope->mWaiverWrapperMap->Remove(key);
  }

  return true;
}

JSObject* TransplantObject(JSContext* cx, JS::HandleObject origobj,
                           JS::HandleObject target) {
  RootedObject oldWaiver(cx, WrapperFactory::GetXrayWaiver(origobj));
  MOZ_ASSERT_IF(oldWaiver, GetNonCCWObjectRealm(oldWaiver) ==
                               GetNonCCWObjectRealm(origobj));
  RootedObject newIdentity(cx, JS_TransplantObject(cx, origobj, target));
  if (!newIdentity || !oldWaiver) {
    return newIdentity;
  }

  bool crossCompartmentTransplant = (newIdentity != origobj);
  if (!crossCompartmentTransplant) {
    // We might still have been transplanted across realms within a single
    // compartment.
    if (GetNonCCWObjectRealm(oldWaiver) == GetNonCCWObjectRealm(newIdentity)) {
      // The old waiver is same-realm with the new object; nothing else to do
      // here.
      return newIdentity;
    }
  }

  if (!FixWaiverAfterTransplant(cx, oldWaiver, newIdentity,
                                crossCompartmentTransplant)) {
    return nullptr;
  }
  return newIdentity;
}

JSObject* TransplantObjectRetainingXrayExpandos(JSContext* cx,
                                                JS::HandleObject origobj,
                                                JS::HandleObject target) {
  // Save the chain of objects that carry origobj's Xray expando properties
  // (from all compartments). TransplantObject will blow this away; we'll
  // restore it manually afterwards.
  RootedObject expandoChain(
      cx, GetXrayTraits(origobj)->detachExpandoChain(origobj));

  RootedObject newIdentity(cx, TransplantObject(cx, origobj, target));

  // Copy Xray expando properties to the new wrapper.
  if (!GetXrayTraits(newIdentity)
           ->cloneExpandoChain(cx, newIdentity, expandoChain)) {
    // Failure here means some expandos were not copied over. The object graph
    // and the Xray machinery are left in a consistent state, but mysteriously
    // losing these expandos is too weird to allow.
    MOZ_CRASH();
  }

  return newIdentity;
}

nsIGlobalObject* NativeGlobal(JSObject* obj) {
  obj = JS::GetNonCCWObjectGlobal(obj);

  // Every global needs to hold a native as its private or be a
  // WebIDL object with an nsISupports DOM object.
  MOZ_ASSERT((GetObjectClass(obj)->flags &
              (JSCLASS_PRIVATE_IS_NSISUPPORTS | JSCLASS_HAS_PRIVATE)) ||
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
  MOZ_ASSERT(global,
             "Native held by global needs to implement nsIGlobalObject!");

  return global;
}

nsIGlobalObject* CurrentNativeGlobal(JSContext* cx) {
  return xpc::NativeGlobal(JS::CurrentGlobalOrNull(cx));
}

}  // namespace xpc
