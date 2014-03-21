/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "XrayWrapper.h"
#include "AccessCheck.h"
#include "WrapperFactory.h"

#include "nsIContent.h"
#include "nsIControllers.h"
#include "nsContentUtils.h"

#include "XPCWrapper.h"
#include "xpcprivate.h"

#include "jsapi.h"
#include "jsprf.h"
#include "nsJSUtils.h"

#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/WindowBinding.h"
#include "nsGlobalWindow.h"

using namespace mozilla::dom;
using namespace JS;
using namespace mozilla;

using js::Wrapper;
using js::BaseProxyHandler;
using js::IsCrossCompartmentWrapper;
using js::UncheckedUnwrap;
using js::CheckedUnwrap;

namespace xpc {

using namespace XrayUtils;

// Whitelist for the standard ES classes we can Xray to.
static bool
IsJSXraySupported(JSProtoKey key)
{
    return false;
}

XrayType
GetXrayType(JSObject *obj)
{
    obj = js::UncheckedUnwrap(obj, /* stopAtOuter = */ false);
    if (mozilla::dom::UseDOMXray(obj))
        return XrayForDOMObject;

    const js::Class* clasp = js::GetObjectClass(obj);
    if (IS_WN_CLASS(clasp) || clasp->ext.innerObject)
        return XrayForWrappedNative;

    JSProtoKey standardProto = IdentifyStandardInstanceOrPrototype(obj);
    if (IsJSXraySupported(standardProto))
        return XrayForJSObject;

    return NotXray;
}

JSObject *
XrayAwareCalleeGlobal(JSObject *fun)
{
  MOZ_ASSERT(js::IsFunctionObject(fun));
  JSObject *scope = js::GetObjectParent(fun);
  if (IsXrayWrapper(scope))
    scope = js::UncheckedUnwrap(scope);
  return js::GetGlobalForObjectCrossCompartment(scope);
}

const uint32_t JSSLOT_RESOLVING = 0;
ResolvingId::ResolvingId(JSContext *cx, HandleObject wrapper, HandleId id)
  : mId(id),
    mHolder(cx, getHolderObject(wrapper)),
    mPrev(getResolvingId(mHolder)),
    mXrayShadowing(false)
{
    js::SetReservedSlot(mHolder, JSSLOT_RESOLVING, js::PrivateValue(this));
}

ResolvingId::~ResolvingId()
{
    MOZ_ASSERT(getResolvingId(mHolder) == this, "unbalanced ResolvingIds");
    js::SetReservedSlot(mHolder, JSSLOT_RESOLVING, js::PrivateValue(mPrev));
}

bool
ResolvingId::isXrayShadowing(jsid id)
{
    if (!mXrayShadowing)
        return false;

    return mId == id;
}

bool
ResolvingId::isResolving(jsid id)
{
    for (ResolvingId *cur = this; cur; cur = cur->mPrev) {
        if (cur->mId == id)
            return true;
    }

    return false;
}

ResolvingId *
ResolvingId::getResolvingId(JSObject *holder)
{
    MOZ_ASSERT(strcmp(JS_GetClass(holder)->name, "NativePropertyHolder") == 0);
    return (ResolvingId *)js::GetReservedSlot(holder, JSSLOT_RESOLVING).toPrivate();
}

JSObject *
ResolvingId::getHolderObject(JSObject *wrapper)
{
    return &js::GetProxyExtra(wrapper, 0).toObject();
}

ResolvingId *
ResolvingId::getResolvingIdFromWrapper(JSObject *wrapper)
{
    return getResolvingId(getHolderObject(wrapper));
}

class MOZ_STACK_CLASS ResolvingIdDummy
{
public:
    ResolvingIdDummy(JSContext *cx, HandleObject wrapper, HandleId id)
    {
    }
};

class XrayTraits
{
public:
    static JSObject* getTargetObject(JSObject *wrapper) {
        return js::UncheckedUnwrap(wrapper, /* stopAtOuter = */ false);
    }

    virtual bool resolveNativeProperty(JSContext *cx, HandleObject wrapper,
                                       HandleObject holder, HandleId id,
                                       MutableHandle<JSPropertyDescriptor> desc,
                                       unsigned flags) = 0;
    // NB: resolveOwnProperty may decide whether or not to cache what it finds
    // on the holder. If the result is not cached, the lookup will happen afresh
    // for each access, which is the right thing for things like dynamic NodeList
    // properties.
    virtual bool resolveOwnProperty(JSContext *cx, Wrapper &jsWrapper,
                                    HandleObject wrapper, HandleObject holder,
                                    HandleId id, MutableHandle<JSPropertyDescriptor> desc,
                                    unsigned flags);

    virtual void preserveWrapper(JSObject *target) = 0;

    JSObject* getExpandoObject(JSContext *cx, HandleObject target,
                               HandleObject consumer);
    JSObject* ensureExpandoObject(JSContext *cx, HandleObject wrapper,
                                  HandleObject target);

    JSObject* getHolder(JSObject *wrapper);
    JSObject* ensureHolder(JSContext *cx, HandleObject wrapper);
    virtual JSObject* createHolder(JSContext *cx, JSObject *wrapper) = 0;

    JSObject* getExpandoChain(JSObject *obj) {
      return GetObjectScope(obj)->GetExpandoChain(obj);
    }

    bool setExpandoChain(JSContext *cx, HandleObject obj, HandleObject chain) {
      return GetObjectScope(obj)->SetExpandoChain(cx, obj, chain);
    }
    bool cloneExpandoChain(JSContext *cx, HandleObject dst, HandleObject src);

private:
    bool expandoObjectMatchesConsumer(JSContext *cx, HandleObject expandoObject,
                                      nsIPrincipal *consumerOrigin,
                                      HandleObject exclusiveGlobal);
    JSObject* getExpandoObjectInternal(JSContext *cx, HandleObject target,
                                       nsIPrincipal *origin,
                                       JSObject *exclusiveGlobal);
    JSObject* attachExpandoObject(JSContext *cx, HandleObject target,
                                  nsIPrincipal *origin,
                                  HandleObject exclusiveGlobal);
};

class XPCWrappedNativeXrayTraits : public XrayTraits
{
public:
    enum {
        HasPrototype = 0
    };

    static const XrayType Type = XrayForWrappedNative;

    virtual bool resolveNativeProperty(JSContext *cx, HandleObject wrapper,
                                       HandleObject holder, HandleId id,
                                       MutableHandle<JSPropertyDescriptor> desc, unsigned flags);
    virtual bool resolveOwnProperty(JSContext *cx, Wrapper &jsWrapper, HandleObject wrapper,
                                    HandleObject holder, HandleId id,
                                    MutableHandle<JSPropertyDescriptor> desc, unsigned flags);
    static bool defineProperty(JSContext *cx, HandleObject wrapper, HandleId id,
                               MutableHandle<JSPropertyDescriptor> desc,
                               Handle<JSPropertyDescriptor> existingDesc, bool *defined);
    static bool enumerateNames(JSContext *cx, HandleObject wrapper, unsigned flags,
                               AutoIdVector &props);
    static bool call(JSContext *cx, HandleObject wrapper,
                     const JS::CallArgs &args, js::Wrapper& baseInstance);
    static bool construct(JSContext *cx, HandleObject wrapper,
                          const JS::CallArgs &args, js::Wrapper& baseInstance);

    static bool isResolving(JSContext *cx, JSObject *holder, jsid id);

    static bool resolveDOMCollectionProperty(JSContext *cx, HandleObject wrapper,
                                             HandleObject holder, HandleId id,
                                             MutableHandle<JSPropertyDescriptor> desc, unsigned flags);

    static XPCWrappedNative* getWN(JSObject *wrapper) {
        return XPCWrappedNative::Get(getTargetObject(wrapper));
    }

    virtual void preserveWrapper(JSObject *target);

    typedef ResolvingId ResolvingIdImpl;

    virtual JSObject* createHolder(JSContext *cx, JSObject *wrapper);

    static const JSClass HolderClass;
    static XPCWrappedNativeXrayTraits singleton;
};

const JSClass XPCWrappedNativeXrayTraits::HolderClass = {
    "NativePropertyHolder", JSCLASS_HAS_RESERVED_SLOTS(2),
    JS_PropertyStub, JS_DeletePropertyStub, holder_get, holder_set,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub
};

class DOMXrayTraits : public XrayTraits
{
public:
    enum {
        HasPrototype = 0
    };

    static const XrayType Type = XrayForDOMObject;

    virtual bool resolveNativeProperty(JSContext *cx, HandleObject wrapper,
                                       HandleObject holder, HandleId id,
                                       MutableHandle<JSPropertyDescriptor> desc, unsigned flags);
    virtual bool resolveOwnProperty(JSContext *cx, Wrapper &jsWrapper, HandleObject wrapper,
                                    HandleObject holder, HandleId id,
                                    MutableHandle<JSPropertyDescriptor> desc, unsigned flags);
    static bool defineProperty(JSContext *cx, HandleObject wrapper, HandleId id,
                               MutableHandle<JSPropertyDescriptor> desc,
                               Handle<JSPropertyDescriptor> existingDesc, bool *defined);
    static bool enumerateNames(JSContext *cx, HandleObject wrapper, unsigned flags,
                               AutoIdVector &props);
    static bool call(JSContext *cx, HandleObject wrapper,
                     const JS::CallArgs &args, js::Wrapper& baseInstance);
    static bool construct(JSContext *cx, HandleObject wrapper,
                          const JS::CallArgs &args, js::Wrapper& baseInstance);

    static bool isResolving(JSContext *cx, JSObject *holder, jsid id)
    {
        return false;
    }

    typedef ResolvingIdDummy ResolvingIdImpl;

    virtual void preserveWrapper(JSObject *target);

    virtual JSObject* createHolder(JSContext *cx, JSObject *wrapper);

    static DOMXrayTraits singleton;
};

class JSXrayTraits : public XrayTraits
{
public:
    enum {
        HasPrototype = 1
    };
    static const XrayType Type = XrayForJSObject;

    virtual bool resolveNativeProperty(JSContext *cx, HandleObject wrapper,
                                       HandleObject holder, HandleId id,
                                       MutableHandle<JSPropertyDescriptor> desc, unsigned flags)
    {
        MOZ_ASSUME_UNREACHABLE("resolveNativeProperty hook should never be called with HasPrototype = 1");
    }

    virtual bool resolveOwnProperty(JSContext *cx, Wrapper &jsWrapper, HandleObject wrapper,
                                    HandleObject holder, HandleId id,
                                    MutableHandle<JSPropertyDescriptor> desc, unsigned flags)
    {
        MOZ_ASSUME_UNREACHABLE("Not yet implemented");
    }

    static bool defineProperty(JSContext *cx, HandleObject wrapper, HandleId id,
                               MutableHandle<JSPropertyDescriptor> desc,
                               Handle<JSPropertyDescriptor> existingDesc, bool *defined)
    {
        // There's no useful per-trait work to do here. Punt back up to the common code.
        *defined = false;
        return true;
    }

    static bool enumerateNames(JSContext *cx, HandleObject wrapper, unsigned flags,
                               AutoIdVector &props)
    {
        MOZ_ASSUME_UNREACHABLE("Not yet implemented");
    }

    static bool call(JSContext *cx, HandleObject wrapper,
                     const JS::CallArgs &args, js::Wrapper& baseInstance)
    {
        // We'll handle this when we start supporting Functions.
        RootedValue v(cx, ObjectValue(*wrapper));
        js_ReportIsNotFunction(cx, v);
        return false;
    }

    static bool construct(JSContext *cx, HandleObject wrapper,
                          const JS::CallArgs &args, js::Wrapper& baseInstance)
    {
        // We'll handle this when we start supporting Functions.
        RootedValue v(cx, ObjectValue(*wrapper));
        js_ReportIsNotFunction(cx, v);
        return false;
    }

    static bool isResolving(JSContext *cx, JSObject *holder, jsid id)
    {
        return false;
    }

    typedef ResolvingIdDummy ResolvingIdImpl;

    bool getPrototypeOf(JSContext *cx, JS::HandleObject wrapper,
                               JS::HandleObject target,
                               JS::MutableHandleObject protop)
    {
        RootedObject holder(cx, ensureHolder(cx, wrapper));
        JSProtoKey key = isPrototype(holder) ? JSProto_Object
                                             : getProtoKey(holder);
        {
            JSAutoCompartment ac(cx, target);
            if (!JS_GetClassPrototype(cx, key, protop))
                return nullptr;
        }
        return JS_WrapObject(cx, protop);
    }

    virtual void preserveWrapper(JSObject *target) {
        // In the case of pure JS objects, there is no underlying object, and
        // the target is the canonical representation of state. If it gets
        // collected, then expandos and such should be collected too. So there's
        // nothing to do here.
    }

    enum {
        SLOT_PROTOKEY = 0,
        SLOT_ISPROTOTYPE,
        SLOT_COUNT
    };
    virtual JSObject* createHolder(JSContext *cx, JSObject *wrapper);

    static JSProtoKey getProtoKey(JSObject *holder) {
        int32_t key = js::GetReservedSlot(holder, SLOT_PROTOKEY).toInt32();
        return static_cast<JSProtoKey>(key);
    }

    static bool isPrototype(JSObject *holder) {
        return js::GetReservedSlot(holder, SLOT_ISPROTOTYPE).toBoolean();
    }

    static const JSClass HolderClass;
    static JSXrayTraits singleton;
};

const JSClass JSXrayTraits::HolderClass = {
    "JSXrayHolder", JSCLASS_HAS_RESERVED_SLOTS(SLOT_COUNT),
    JS_PropertyStub, JS_DeletePropertyStub,
    JS_PropertyStub, JS_StrictPropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub
};

JSObject*
JSXrayTraits::createHolder(JSContext *cx, JSObject *wrapper)
{
    RootedObject global(cx, JS_GetGlobalForObject(cx, wrapper));
    RootedObject target(cx, getTargetObject(wrapper));
    RootedObject holder(cx, JS_NewObjectWithGivenProto(cx, &HolderClass,
                                                       JS::NullPtr(), global));
    if (!holder)
        return nullptr;

    // Compute information about the target.
    bool isPrototype = false;
    JSProtoKey key = IdentifyStandardInstance(target);
    if (key == JSProto_Null) {
        isPrototype = true;
        key = IdentifyStandardPrototype(target);
    }
    MOZ_ASSERT(key != JSProto_Null);

    // Store it on the holder.
    RootedValue v(cx);
    v.setNumber(static_cast<uint32_t>(key));
    js::SetReservedSlot(holder, SLOT_PROTOKEY, v);
    v.setBoolean(isPrototype);
    js::SetReservedSlot(holder, SLOT_ISPROTOTYPE, v);

    return holder;
}

XPCWrappedNativeXrayTraits XPCWrappedNativeXrayTraits::singleton;
DOMXrayTraits DOMXrayTraits::singleton;
JSXrayTraits JSXrayTraits::singleton;

XrayTraits*
GetXrayTraits(JSObject *obj)
{
    switch (GetXrayType(obj)) {
      case XrayForDOMObject:
        return &DOMXrayTraits::singleton;
      case XrayForWrappedNative:
        return &XPCWrappedNativeXrayTraits::singleton;
      case XrayForJSObject:
        return &JSXrayTraits::singleton;
      default:
        return nullptr;
    }
}

/*
 * Xray expando handling.
 *
 * We hang expandos for Xray wrappers off a reserved slot on the target object
 * so that same-origin compartments can share expandos for a given object. We
 * have a linked list of expando objects, one per origin. The properties on these
 * objects are generally wrappers pointing back to the compartment that applied
 * them.
 *
 * The expando objects should _never_ be exposed to script. The fact that they
 * live in the target compartment is a detail of the implementation, and does
 * not imply that code in the target compartment should be allowed to inspect
 * them. They are private to the origin that placed them.
 */

enum ExpandoSlots {
    JSSLOT_EXPANDO_NEXT = 0,
    JSSLOT_EXPANDO_ORIGIN,
    JSSLOT_EXPANDO_EXCLUSIVE_GLOBAL,
    JSSLOT_EXPANDO_PROTOTYPE,
    JSSLOT_EXPANDO_COUNT
};

static nsIPrincipal*
ObjectPrincipal(JSObject *obj)
{
    return GetCompartmentPrincipal(js::GetObjectCompartment(obj));
}

static nsIPrincipal*
GetExpandoObjectPrincipal(JSObject *expandoObject)
{
    Value v = JS_GetReservedSlot(expandoObject, JSSLOT_EXPANDO_ORIGIN);
    return static_cast<nsIPrincipal*>(v.toPrivate());
}

static void
ExpandoObjectFinalize(JSFreeOp *fop, JSObject *obj)
{
    // Release the principal.
    nsIPrincipal *principal = GetExpandoObjectPrincipal(obj);
    NS_RELEASE(principal);
}

const JSClass ExpandoObjectClass = {
    "XrayExpandoObject",
    JSCLASS_HAS_RESERVED_SLOTS(JSSLOT_EXPANDO_COUNT),
    JS_PropertyStub, JS_DeletePropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, ExpandoObjectFinalize
};

bool
XrayTraits::expandoObjectMatchesConsumer(JSContext *cx,
                                         HandleObject expandoObject,
                                         nsIPrincipal *consumerOrigin,
                                         HandleObject exclusiveGlobal)
{
    MOZ_ASSERT(js::IsObjectInContextCompartment(expandoObject, cx));

    // First, compare the principals.
    nsIPrincipal *o = GetExpandoObjectPrincipal(expandoObject);
    // Note that it's very important here to ignore document.domain. We
    // pull the principal for the expando object off of the first consumer
    // for a given origin, and freely share the expandos amongst multiple
    // same-origin consumers afterwards. However, this means that we have
    // no way to know whether _all_ consumers have opted in to collaboration
    // by explicitly setting document.domain. So we just mandate that expando
    // sharing is unaffected by it.
    if (!consumerOrigin->Equals(o))
      return false;

    // Sandboxes want exclusive expando objects.
    JSObject *owner = JS_GetReservedSlot(expandoObject,
                                         JSSLOT_EXPANDO_EXCLUSIVE_GLOBAL)
                                        .toObjectOrNull();
    if (!owner && !exclusiveGlobal)
        return true;

    // The exclusive global should always be wrapped in the target's compartment.
    MOZ_ASSERT(!exclusiveGlobal || js::IsObjectInContextCompartment(exclusiveGlobal, cx));
    MOZ_ASSERT(!owner || js::IsObjectInContextCompartment(owner, cx));
    return owner == exclusiveGlobal;
}

JSObject *
XrayTraits::getExpandoObjectInternal(JSContext *cx, HandleObject target,
                                     nsIPrincipal *origin,
                                     JSObject *exclusiveGlobalArg)
{
    // The expando object lives in the compartment of the target, so all our
    // work needs to happen there.
    RootedObject exclusiveGlobal(cx, exclusiveGlobalArg);
    JSAutoCompartment ac(cx, target);
    if (!JS_WrapObject(cx, &exclusiveGlobal))
        return nullptr;

    // Iterate through the chain, looking for a same-origin object.
    RootedObject head(cx, getExpandoChain(target));
    while (head) {
        if (expandoObjectMatchesConsumer(cx, head, origin, exclusiveGlobal))
            return head;
        head = JS_GetReservedSlot(head, JSSLOT_EXPANDO_NEXT).toObjectOrNull();
    }

    // Not found.
    return nullptr;
}

JSObject *
XrayTraits::getExpandoObject(JSContext *cx, HandleObject target, HandleObject consumer)
{
    JSObject *consumerGlobal = js::GetGlobalForObjectCrossCompartment(consumer);
    bool isSandbox = !strcmp(js::GetObjectJSClass(consumerGlobal)->name, "Sandbox");
    return getExpandoObjectInternal(cx, target, ObjectPrincipal(consumer),
                                    isSandbox ? consumerGlobal : nullptr);
}

JSObject *
XrayTraits::attachExpandoObject(JSContext *cx, HandleObject target,
                                nsIPrincipal *origin, HandleObject exclusiveGlobal)
{
    // Make sure the compartments are sane.
    MOZ_ASSERT(js::IsObjectInContextCompartment(target, cx));
    MOZ_ASSERT(!exclusiveGlobal || js::IsObjectInContextCompartment(exclusiveGlobal, cx));

    // No duplicates allowed.
    MOZ_ASSERT(!getExpandoObjectInternal(cx, target, origin, exclusiveGlobal));

    // Create the expando object. We parent it directly to the target object.
    RootedObject expandoObject(cx, JS_NewObjectWithGivenProto(cx, &ExpandoObjectClass,
                                                              JS::NullPtr(), target));
    if (!expandoObject)
        return nullptr;

    // AddRef and store the principal.
    NS_ADDREF(origin);
    JS_SetReservedSlot(expandoObject, JSSLOT_EXPANDO_ORIGIN, PRIVATE_TO_JSVAL(origin));

    // Note the exclusive global, if any.
    JS_SetReservedSlot(expandoObject, JSSLOT_EXPANDO_EXCLUSIVE_GLOBAL,
                       OBJECT_TO_JSVAL(exclusiveGlobal));

    // If this is our first expando object, take the opportunity to preserve
    // the wrapper. This keeps our expandos alive even if the Xray wrapper gets
    // collected.
    RootedObject chain(cx, getExpandoChain(target));
    if (!chain)
        preserveWrapper(target);

    // Insert it at the front of the chain.
    JS_SetReservedSlot(expandoObject, JSSLOT_EXPANDO_NEXT, OBJECT_TO_JSVAL(chain));
    setExpandoChain(cx, target, expandoObject);

    return expandoObject;
}

JSObject *
XrayTraits::ensureExpandoObject(JSContext *cx, HandleObject wrapper,
                                HandleObject target)
{
    // Expando objects live in the target compartment.
    JSAutoCompartment ac(cx, target);
    JSObject *expandoObject = getExpandoObject(cx, target, wrapper);
    if (!expandoObject) {
        // If the object is a sandbox, we don't want it to share expandos with
        // anyone else, so we tag it with the sandbox global.
        //
        // NB: We first need to check the class, _then_ wrap for the target's
        // compartment.
        RootedObject consumerGlobal(cx, js::GetGlobalForObjectCrossCompartment(wrapper));
        bool isSandbox = !strcmp(js::GetObjectJSClass(consumerGlobal)->name, "Sandbox");
        if (!JS_WrapObject(cx, &consumerGlobal))
            return nullptr;
        expandoObject = attachExpandoObject(cx, target, ObjectPrincipal(wrapper),
                                            isSandbox ? (HandleObject)consumerGlobal : NullPtr());
    }
    return expandoObject;
}

bool
XrayTraits::cloneExpandoChain(JSContext *cx, HandleObject dst, HandleObject src)
{
    MOZ_ASSERT(js::IsObjectInContextCompartment(dst, cx));
    MOZ_ASSERT(getExpandoChain(dst) == nullptr);

    RootedObject oldHead(cx, getExpandoChain(src));
    while (oldHead) {
        RootedObject exclusive(cx, JS_GetReservedSlot(oldHead,
                                                      JSSLOT_EXPANDO_EXCLUSIVE_GLOBAL)
                                                     .toObjectOrNull());
        if (!JS_WrapObject(cx, &exclusive))
            return false;
        RootedObject newHead(cx, attachExpandoObject(cx, dst, GetExpandoObjectPrincipal(oldHead),
                                                     exclusive));
        if (!JS_CopyPropertiesFrom(cx, newHead, oldHead))
            return false;
        oldHead = JS_GetReservedSlot(oldHead, JSSLOT_EXPANDO_NEXT).toObjectOrNull();
    }
    return true;
}

namespace XrayUtils {
bool CloneExpandoChain(JSContext *cx, JSObject *dstArg, JSObject *srcArg)
{
    RootedObject dst(cx, dstArg);
    RootedObject src(cx, srcArg);
    return GetXrayTraits(src)->cloneExpandoChain(cx, dst, src);
}
}

static JSObject *
GetHolder(JSObject *obj)
{
    return &js::GetProxyExtra(obj, 0).toObject();
}

JSObject*
XrayTraits::getHolder(JSObject *wrapper)
{
    MOZ_ASSERT(WrapperFactory::IsXrayWrapper(wrapper));
    js::Value v = js::GetProxyExtra(wrapper, 0);
    return v.isObject() ? &v.toObject() : nullptr;
}

JSObject*
XrayTraits::ensureHolder(JSContext *cx, HandleObject wrapper)
{
    RootedObject holder(cx, getHolder(wrapper));
    if (holder)
        return holder;
    holder = createHolder(cx, wrapper); // virtual trap.
    if (holder)
        js::SetProxyExtra(wrapper, 0, ObjectValue(*holder));
    return holder;
}

bool
XPCWrappedNativeXrayTraits::isResolving(JSContext *cx, JSObject *holder,
                                        jsid id)
{
    ResolvingId *cur = ResolvingId::getResolvingId(holder);
    if (!cur)
        return false;
    return cur->isResolving(id);
}

namespace XrayUtils {

bool
IsXPCWNHolderClass(const JSClass *clasp)
{
  return clasp == &XPCWrappedNativeXrayTraits::HolderClass;
}

}


// Some DOM objects have shared properties that don't have an explicit
// getter/setter and rely on the class getter/setter. We install a
// class getter/setter on the holder object to trigger them.
bool
holder_get(JSContext *cx, HandleObject wrapper, HandleId id, MutableHandleValue vp)
{
    // JSClass::getProperty is wacky enough that it's hard to be sure someone
    // can't inherit this getter by prototyping a random object to an
    // XrayWrapper. Be safe.
    NS_ENSURE_TRUE(WrapperFactory::IsXrayWrapper(wrapper), true);
    JSObject *holder = GetHolder(wrapper);

    XPCWrappedNative *wn = XPCWrappedNativeXrayTraits::getWN(wrapper);
    if (NATIVE_HAS_FLAG(wn, WantGetProperty)) {
        JSAutoCompartment ac(cx, holder);
        bool retval = true;
        nsresult rv = wn->GetScriptableCallback()->GetProperty(wn, cx, wrapper,
                                                               id, vp.address(), &retval);
        if (NS_FAILED(rv) || !retval) {
            if (retval)
                XPCThrower::Throw(rv, cx);
            return false;
        }
    }
    return true;
}

bool
holder_set(JSContext *cx, HandleObject wrapper, HandleId id, bool strict, MutableHandleValue vp)
{
    // JSClass::setProperty is wacky enough that it's hard to be sure someone
    // can't inherit this getter by prototyping a random object to an
    // XrayWrapper. Be safe.
    NS_ENSURE_TRUE(WrapperFactory::IsXrayWrapper(wrapper), true);
    JSObject *holder = GetHolder(wrapper);
    if (XPCWrappedNativeXrayTraits::isResolving(cx, holder, id)) {
        return true;
    }

    XPCWrappedNative *wn = XPCWrappedNativeXrayTraits::getWN(wrapper);
    if (NATIVE_HAS_FLAG(wn, WantSetProperty)) {
        JSAutoCompartment ac(cx, holder);
        bool retval = true;
        nsresult rv = wn->GetScriptableCallback()->SetProperty(wn, cx, wrapper,
                                                               id, vp.address(), &retval);
        if (NS_FAILED(rv) || !retval) {
            if (retval)
                XPCThrower::Throw(rv, cx);
            return false;
        }
    }
    return true;
}

class AutoSetWrapperNotShadowing
{
public:
    AutoSetWrapperNotShadowing(ResolvingId *resolvingId MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
    {
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;
        MOZ_ASSERT(resolvingId);
        mResolvingId = resolvingId;
        mResolvingId->mXrayShadowing = true;
    }

    ~AutoSetWrapperNotShadowing()
    {
        mResolvingId->mXrayShadowing = false;
    }

private:
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
    ResolvingId *mResolvingId;
};

// This is called after the resolveNativeProperty could not find any property
// with the given id. At this point we can check for DOM specific collections
// like document["formName"] because we already know that it is not shadowing
// any native property.
bool
XPCWrappedNativeXrayTraits::resolveDOMCollectionProperty(JSContext *cx, HandleObject wrapper,
                                                         HandleObject holder, HandleId id,
                                                         MutableHandle<JSPropertyDescriptor> desc,
                                                         unsigned flags)
{
    // If we are not currently resolving this id and resolveNative is called
    // we don't do anything. (see defineProperty in case of shadowing is forbidden).
    ResolvingId *rid = ResolvingId::getResolvingId(holder);
    if (!rid || rid->mId != id)
        return true;

    XPCWrappedNative *wn = getWN(wrapper);
    if (!wn) {
        // This should NEVER happen, but let's be extra careful here
        // because of the reported crashes (Bug 832091).
        XPCThrower::Throw(NS_ERROR_UNEXPECTED, cx);
        return false;
    }
    if (!NATIVE_HAS_FLAG(wn, WantNewResolve))
        return true;

    ResolvingId *resolvingId = ResolvingId::getResolvingIdFromWrapper(wrapper);
    if (!resolvingId) {
        // This should NEVER happen, but let's be extra careful here
        // becaue of the reported crashes (Bug 832091).
        XPCThrower::Throw(NS_ERROR_UNEXPECTED, cx);
        return false;
    }

    // Setting the current ResolvingId in non-shadowing mode. So for this id
    // Xray won't ignore DOM specific collection properties temporarily.
    AutoSetWrapperNotShadowing asw(resolvingId);

    bool retval = true;
    RootedObject pobj(cx);
    nsresult rv = wn->GetScriptableInfo()->GetCallback()->NewResolve(wn, cx, wrapper, id,
                                                                     flags, pobj.address(), &retval);
    if (NS_FAILED(rv)) {
        if (retval)
            XPCThrower::Throw(rv, cx);
        return false;
    }

    if (pobj && !JS_GetPropertyDescriptorById(cx, holder, id, 0, desc))
        return false;

    return true;
}

static nsGlobalWindow*
AsWindow(JSContext *cx, JSObject *wrapper)
{
  nsGlobalWindow* win;
  // We want to use our target object here, since we don't want to be
  // doing a security check while unwrapping.
  JSObject* target = XrayTraits::getTargetObject(wrapper);
  nsresult rv = UNWRAP_OBJECT(Window, target, win);
  if (NS_SUCCEEDED(rv))
      return win;

  nsCOMPtr<nsPIDOMWindow> piWin = do_QueryInterface(
      nsContentUtils::XPConnect()->GetNativeOfWrapper(cx, target));
  return static_cast<nsGlobalWindow*>(piWin.get());
}

static bool
IsWindow(JSContext *cx, JSObject *wrapper)
{
    return !!AsWindow(cx, wrapper);
}

static nsQueryInterface
do_QueryInterfaceNative(JSContext* cx, HandleObject wrapper);

void
XPCWrappedNativeXrayTraits::preserveWrapper(JSObject *target)
{
    XPCWrappedNative *wn = XPCWrappedNative::Get(target);
    nsRefPtr<nsXPCClassInfo> ci;
    CallQueryInterface(wn->Native(), getter_AddRefs(ci));
    if (ci)
        ci->PreserveWrapper(wn->Native());
}

bool
XPCWrappedNativeXrayTraits::resolveNativeProperty(JSContext *cx, HandleObject wrapper,
                                                  HandleObject holder, HandleId id,
                                                  MutableHandle<JSPropertyDescriptor> desc, unsigned flags)
{
    MOZ_ASSERT(js::GetObjectJSClass(holder) == &HolderClass);

    desc.object().set(nullptr);

    // This will do verification and the method lookup for us.
    RootedObject target(cx, getTargetObject(wrapper));
    XPCCallContext ccx(JS_CALLER, cx, target, NullPtr(), id);

    // There are no native numeric properties, so we can shortcut here. We will
    // not find the property. However we want to support non shadowing dom
    // specific collection properties like window.frames, so we still have to
    // check for those.
    if (!JSID_IS_STRING(id)) {
        /* Not found */
        return resolveDOMCollectionProperty(cx, wrapper, holder, id, desc, flags);
    }


    // The |controllers| property is accessible as a [ChromeOnly] property on
    // Window.WebIDL, and [noscript] in XPIDL. Chrome needs to see this over
    // Xray, so we need to special-case it until we move |Window| to WebIDL.
    nsGlobalWindow *win = nullptr;
    if (id == GetRTIdByIndex(cx, XPCJSRuntime::IDX_CONTROLLERS) &&
        AccessCheck::isChrome(wrapper) &&
        (win = AsWindow(cx, wrapper)))
    {
        nsCOMPtr<nsIControllers> c;
        nsresult rv = win->GetControllers(getter_AddRefs(c));
        if (NS_SUCCEEDED(rv) && c) {
            rv = nsXPConnect::XPConnect()->WrapNativeToJSVal(cx, CurrentGlobalOrNull(cx),
                                                             c, nullptr, nullptr, true,
                                                             desc.value());
        }

        if (NS_FAILED(rv) || !c) {
            JS_ReportError(cx, "Failed to invoke GetControllers via Xrays");
            return false;
        }

        desc.object().set(wrapper);
        return true;
    }

    XPCNativeInterface *iface;
    XPCNativeMember *member;
    XPCWrappedNative *wn = getWN(wrapper);

    if (ccx.GetWrapper() != wn || !wn->IsValid()) {
        // Something is wrong. If the wrapper is not even valid let's not risk
        // calling resolveDOMCollectionProperty.
        return true;
    } else if (!(iface = ccx.GetInterface()) ||
               !(member = ccx.GetMember())) {
        /* Not found */
        return resolveDOMCollectionProperty(cx, wrapper, holder, id, desc, flags);
    }

    desc.object().set(holder);
    desc.setAttributes(JSPROP_ENUMERATE);
    desc.setGetter(nullptr);
    desc.setSetter(nullptr);
    desc.value().set(JSVAL_VOID);

    RootedValue fval(cx, JSVAL_VOID);
    if (member->IsConstant()) {
        if (!member->GetConstantValue(ccx, iface, desc.value().address())) {
            JS_ReportError(cx, "Failed to convert constant native property to JS value");
            return false;
        }
    } else if (member->IsAttribute()) {
        // This is a getter/setter. Clone a function for it.
        if (!member->NewFunctionObject(ccx, iface, wrapper, fval.address())) {
            JS_ReportError(cx, "Failed to clone function object for native getter/setter");
            return false;
        }

        unsigned attrs = desc.attributes();
        attrs |= JSPROP_GETTER;
        if (member->IsWritableAttribute())
            attrs |= JSPROP_SETTER;

        // Make the property shared on the holder so no slot is allocated
        // for it. This avoids keeping garbage alive through that slot.
        attrs |= JSPROP_SHARED;
        desc.setAttributes(attrs);
    } else {
        // This is a method. Clone a function for it.
        if (!member->NewFunctionObject(ccx, iface, wrapper, desc.value().address())) {
            JS_ReportError(cx, "Failed to clone function object for native function");
            return false;
        }

        // Without a wrapper the function would live on the prototype. Since we
        // don't have one, we have to avoid calling the scriptable helper's
        // GetProperty method for this property, so stub out the getter and
        // setter here explicitly.
        desc.setGetter(JS_PropertyStub);
        desc.setSetter(JS_StrictPropertyStub);
    }

    if (!JS_WrapValue(cx, desc.value()) || !JS_WrapValue(cx, &fval))
        return false;

    if (desc.hasGetterObject())
        desc.setGetterObject(&fval.toObject());
    if (desc.hasSetterObject())
        desc.setSetterObject(&fval.toObject());

    // Define the property.
    return JS_DefinePropertyById(cx, holder, id, desc.value(),
                                 desc.getter(), desc.setter(), desc.attributes());
}

static bool
wrappedJSObject_getter(JSContext *cx, HandleObject wrapper, HandleId id, MutableHandleValue vp)
{
    if (!IsWrapper(wrapper) || !WrapperFactory::IsXrayWrapper(wrapper)) {
        JS_ReportError(cx, "Unexpected object");
        return false;
    }

    vp.set(OBJECT_TO_JSVAL(wrapper));

    return WrapperFactory::WaiveXrayAndWrap(cx, vp);
}

bool
XrayTraits::resolveOwnProperty(JSContext *cx, Wrapper &jsWrapper,
                               HandleObject wrapper, HandleObject holder, HandleId id,
                               MutableHandle<JSPropertyDescriptor> desc, unsigned flags)
{
    desc.object().set(nullptr);
    RootedObject target(cx, getTargetObject(wrapper));
    RootedObject expando(cx, getExpandoObject(cx, target, wrapper));

    // Check for expando properties first. Note that the expando object lives
    // in the target compartment.
    bool found = false;
    if (expando) {
        JSAutoCompartment ac(cx, expando);
        if (!JS_GetPropertyDescriptorById(cx, expando, id, 0, desc))
            return false;
        found = !!desc.object();
    }

    // Next, check for ES builtins.
    if (!found && JS_IsGlobalObject(target)) {
        JSProtoKey key = JS_IdToProtoKey(cx, id);
        JSAutoCompartment ac(cx, target);
        if (key != JSProto_Null) {
            MOZ_ASSERT(key < JSProto_LIMIT);
            RootedObject constructor(cx);
            if (!JS_GetClassObject(cx, key, &constructor))
                return false;
            MOZ_ASSERT(constructor);
            desc.value().set(ObjectValue(*constructor));
            found = true;
        } else if (id == GetRTIdByIndex(cx, XPCJSRuntime::IDX_EVAL)) {
            RootedObject eval(cx);
            if (!js::GetOriginalEval(cx, target, &eval))
                return false;
            desc.value().set(ObjectValue(*eval));
            found = true;
        }
    }

    if (found) {
        if (!JS_WrapPropertyDescriptor(cx, desc))
            return false;
        // Pretend the property lives on the wrapper.
        desc.object().set(wrapper);
        return true;
    }

    // Handle .wrappedJSObject for subsuming callers. This should move once we
    // sort out own-ness for the holder.
    if (id == GetRTIdByIndex(cx, XPCJSRuntime::IDX_WRAPPED_JSOBJECT) &&
        AccessCheck::wrapperSubsumes(wrapper))
    {
        if (!JS_AlreadyHasOwnPropertyById(cx, holder, id, &found))
            return false;
        if (!found && !JS_DefinePropertyById(cx, holder, id, UndefinedValue(),
                                             wrappedJSObject_getter, nullptr,
                                             JSPROP_ENUMERATE | JSPROP_SHARED)) {
            return false;
        }
        if (!JS_GetPropertyDescriptorById(cx, holder, id, 0, desc))
            return false;
        desc.object().set(wrapper);
        return true;
    }

    return true;
}

bool
XPCWrappedNativeXrayTraits::resolveOwnProperty(JSContext *cx, Wrapper &jsWrapper,
                                               HandleObject wrapper, HandleObject holder,
                                               HandleId id, MutableHandle<JSPropertyDescriptor> desc,
                                               unsigned flags)
{
    // Call the common code.
    bool ok = XrayTraits::resolveOwnProperty(cx, jsWrapper, wrapper, holder,
                                             id, desc, flags);
    if (!ok || desc.object())
        return ok;

    // Check for indexed access on a window.
    int32_t index = GetArrayIndexFromId(cx, id);
    if (IsArrayIndex(index)) {
        nsGlobalWindow* win = AsWindow(cx, wrapper);
        // Note: As() unwraps outer windows to get to the inner window.
        if (win) {
            bool unused;
            nsCOMPtr<nsIDOMWindow> subframe = win->IndexedGetter(index, unused);
            if (subframe) {
                nsGlobalWindow* global = static_cast<nsGlobalWindow*>(subframe.get());
                global->EnsureInnerWindow();
                JSObject* obj = global->FastGetGlobalJSObject();
                if (MOZ_UNLIKELY(!obj)) {
                    // It's gone?
                    return xpc::Throw(cx, NS_ERROR_FAILURE);
                }
                desc.value().setObject(*obj);
                FillPropertyDescriptor(desc, wrapper, true);
                return JS_WrapPropertyDescriptor(cx, desc);
            }
        }
    }

    // Xray wrappers don't use the regular wrapper hierarchy, so we should be
    // in the wrapper's compartment here, not the wrappee.
    MOZ_ASSERT(js::IsObjectInContextCompartment(wrapper, cx));

    bool hasProp;
    if (!JS_HasPropertyById(cx, holder, id, &hasProp)) {
        return false;
    }
    if (!hasProp) {
        XPCWrappedNative *wn = getWN(wrapper);

        // Run the resolve hook of the wrapped native.
        if (!NATIVE_HAS_FLAG(wn, WantNewResolve)) {
            return true;
        }

        bool retval = true;
        RootedObject pobj(cx);
        nsIXPCScriptable *callback = wn->GetScriptableInfo()->GetCallback();
        nsresult rv = callback->NewResolve(wn, cx, wrapper, id, flags,
                                           pobj.address(), &retval);
        if (NS_FAILED(rv)) {
            if (retval)
                XPCThrower::Throw(rv, cx);
            return false;
        }

        MOZ_ASSERT(!pobj || (JS_HasPropertyById(cx, holder, id, &hasProp) &&
                             hasProp), "id got defined somewhere else?");
    }

    // resolveOwnProperty must return a non-empty |desc| if and only if an |own|
    // property was found on the object. However, given how the NewResolve setup
    // works, we can't run the resolve hook if the holder already has a property
    // of the same name. So if there was a pre-existing property on the holder,
    // we have to use it. But we have no way of knowing if it corresponded to an
    // |own| or non-|own| property, since both get cached on the holder and the
    // |own|-ness information is lost.
    //
    // So we just over-zealously call things |own| here. This can cause us to
    // return non-|own| properties from Object.getOwnPropertyDescriptor if
    // lookups are performed in a certain order, but we can probably live with
    // that until XPCWN Xrays go away with the new DOM bindings.
    return JS_GetPropertyDescriptorById(cx, holder, id, 0, desc);
}

bool
XPCWrappedNativeXrayTraits::defineProperty(JSContext *cx, HandleObject wrapper, HandleId id,
                                           MutableHandle<JSPropertyDescriptor> desc,
                                           Handle<JSPropertyDescriptor> existingDesc, bool *defined)
{
    *defined = false;
    JSObject *holder = singleton.ensureHolder(cx, wrapper);
    if (isResolving(cx, holder, id)) {
        if (!desc.hasAttributes(JSPROP_GETTER | JSPROP_SETTER)) {
            if (!desc.getter())
                desc.setGetter(holder_get);
            if (!desc.setter())
                desc.setSetter(holder_set);
        }

        *defined = true;
        return JS_DefinePropertyById(cx, holder, id, desc.value(), desc.getter(), desc.setter(),
                                     desc.attributes());
    }

    // Check for an indexed property on a Window.  If that's happening, do
    // nothing but claim we defined it so it won't get added as an expando.
    int32_t index = GetArrayIndexFromId(cx, id);
    if (IsArrayIndex(index) && IsWindow(cx, wrapper)) {
        *defined = true;
        return true;
    }

    return true;
}

bool
XPCWrappedNativeXrayTraits::enumerateNames(JSContext *cx, HandleObject wrapper, unsigned flags,
                                           AutoIdVector &props)
{
    // Force all native properties to be materialized onto the wrapped native.
    AutoIdVector wnProps(cx);
    {
        RootedObject target(cx, singleton.getTargetObject(wrapper));
        JSAutoCompartment ac(cx, target);
        if (!js::GetPropertyNames(cx, target, flags, &wnProps))
            return false;
    }
    if (!JS_WrapAutoIdVector(cx, wnProps))
        return false;

    // Go through the properties we got and enumerate all native ones.
    for (size_t n = 0; n < wnProps.length(); ++n) {
        RootedId id(cx, wnProps[n]);
        bool hasProp;
        if (!JS_HasPropertyById(cx, wrapper, id, &hasProp))
            return false;
        if (hasProp)
            props.append(id);
    }
    return true;
}

JSObject *
XPCWrappedNativeXrayTraits::createHolder(JSContext *cx, JSObject *wrapper)
{
    RootedObject global(cx, JS_GetGlobalForObject(cx, wrapper));
    JSObject *holder = JS_NewObjectWithGivenProto(cx, &HolderClass, JS::NullPtr(),
                                                  global);
    if (!holder)
        return nullptr;

    js::SetReservedSlot(holder, JSSLOT_RESOLVING, PrivateValue(nullptr));
    return holder;
}

bool
XPCWrappedNativeXrayTraits::call(JSContext *cx, HandleObject wrapper,
                                 const JS::CallArgs &args,
                                 js::Wrapper& baseInstance)
{
    // Run the resolve hook of the wrapped native.
    XPCWrappedNative *wn = getWN(wrapper);
    if (NATIVE_HAS_FLAG(wn, WantCall)) {
        XPCCallContext ccx(JS_CALLER, cx, wrapper, NullPtr(), JSID_VOIDHANDLE, args.length(),
                           args.array(), args.rval().address());
        if (!ccx.IsValid())
            return false;
        bool ok = true;
        nsresult rv = wn->GetScriptableInfo()->GetCallback()->Call(
            wn, cx, wrapper, args, &ok);
        if (NS_FAILED(rv)) {
            if (ok)
                XPCThrower::Throw(rv, cx);
            return false;
        }
    }

    return true;

}

bool
XPCWrappedNativeXrayTraits::construct(JSContext *cx, HandleObject wrapper,
                                      const JS::CallArgs &args,
                                      js::Wrapper& baseInstance)
{
    // Run the resolve hook of the wrapped native.
    XPCWrappedNative *wn = getWN(wrapper);
    if (NATIVE_HAS_FLAG(wn, WantConstruct)) {
        XPCCallContext ccx(JS_CALLER, cx, wrapper, NullPtr(), JSID_VOIDHANDLE, args.length(),
                           args.array(), args.rval().address());
        if (!ccx.IsValid())
            return false;
        bool ok = true;
        nsresult rv = wn->GetScriptableInfo()->GetCallback()->Construct(
            wn, cx, wrapper, args, &ok);
        if (NS_FAILED(rv)) {
            if (ok)
                XPCThrower::Throw(rv, cx);
            return false;
        }
    }

    return true;

}

bool
DOMXrayTraits::resolveNativeProperty(JSContext *cx, HandleObject wrapper,
                                     HandleObject holder, HandleId id,
                                     MutableHandle<JSPropertyDescriptor> desc, unsigned flags)
{
    RootedObject obj(cx, getTargetObject(wrapper));
    if (!XrayResolveNativeProperty(cx, wrapper, obj, id, desc))
        return false;

    MOZ_ASSERT(!desc.object() || desc.object() == wrapper, "What did we resolve this on?");

    return true;
}

bool
DOMXrayTraits::resolveOwnProperty(JSContext *cx, Wrapper &jsWrapper, HandleObject wrapper,
                                  HandleObject holder, HandleId id,
                                  MutableHandle<JSPropertyDescriptor> desc, unsigned flags)
{
    // Call the common code.
    bool ok = XrayTraits::resolveOwnProperty(cx, jsWrapper, wrapper, holder,
                                             id, desc, flags);
    if (!ok || desc.object())
        return ok;

    // Check for indexed access on a window.
    int32_t index = GetArrayIndexFromId(cx, id);
    if (IsArrayIndex(index)) {
        nsGlobalWindow* win = AsWindow(cx, wrapper);
        // Note: As() unwraps outer windows to get to the inner window.
        if (win) {
            bool unused;
            nsCOMPtr<nsIDOMWindow> subframe = win->IndexedGetter(index, unused);
            if (subframe) {
                nsGlobalWindow* global = static_cast<nsGlobalWindow*>(subframe.get());
                global->EnsureInnerWindow();
                JSObject* obj = global->FastGetGlobalJSObject();
                if (MOZ_UNLIKELY(!obj)) {
                    // It's gone?
                    return xpc::Throw(cx, NS_ERROR_FAILURE);
                }
                desc.value().setObject(*obj);
                FillPropertyDescriptor(desc, wrapper, true);
                return JS_WrapPropertyDescriptor(cx, desc);
            }
        }
    }

    RootedObject obj(cx, getTargetObject(wrapper));
    if (!XrayResolveOwnProperty(cx, wrapper, obj, id, desc, flags))
        return false;

    MOZ_ASSERT(!desc.object() || desc.object() == wrapper, "What did we resolve this on?");

    return true;
}

bool
DOMXrayTraits::defineProperty(JSContext *cx, HandleObject wrapper, HandleId id,
                              MutableHandle<JSPropertyDescriptor> desc,
                              Handle<JSPropertyDescriptor> existingDesc, bool *defined)
{
    if (!existingDesc.object())
        return true;

    // Check for an indexed property on a Window.  If that's happening, do
    // nothing but claim we defined it so it won't get added as an expando.
    int32_t index = GetArrayIndexFromId(cx, id);
    if (IsArrayIndex(index) && IsWindow(cx, wrapper)) {
        *defined = true;
        return true;
    }

    JS::Rooted<JSObject*> obj(cx, getTargetObject(wrapper));
    return XrayDefineProperty(cx, wrapper, obj, id, desc, defined);
}

bool
DOMXrayTraits::enumerateNames(JSContext *cx, HandleObject wrapper, unsigned flags,
                              AutoIdVector &props)
{
    JS::Rooted<JSObject*> obj(cx, getTargetObject(wrapper));
    return XrayEnumerateProperties(cx, wrapper, obj, flags, props);
}

bool
DOMXrayTraits::call(JSContext *cx, HandleObject wrapper,
                    const JS::CallArgs &args, js::Wrapper& baseInstance)
{
    RootedObject obj(cx, getTargetObject(wrapper));
    const js::Class* clasp = js::GetObjectClass(obj);
    // What we have is either a WebIDL interface object, a WebIDL prototype
    // object, or a WebIDL instance object.  WebIDL prototype objects never have
    // a clasp->call.  WebIDL interface objects we want to invoke on the xray
    // compartment.  WebIDL instance objects either don't have a clasp->call or
    // are using "legacycaller", which basically means plug-ins.  We want to
    // call those on the content compartment.
    if (clasp->flags & JSCLASS_IS_DOMIFACEANDPROTOJSCLASS) {
        if (!clasp->call) {
            RootedValue v(cx, ObjectValue(*wrapper));
            js_ReportIsNotFunction(cx, v);
            return false;
        }
        // call it on the Xray compartment
        if (!clasp->call(cx, args.length(), args.base()))
            return false;
    } else {
        // This is only reached for WebIDL instance objects, and in practice
        // only for plugins.  Just call them on the content compartment.
        if (!baseInstance.call(cx, wrapper, args))
            return false;
    }
    return JS_WrapValue(cx, args.rval());
}

bool
DOMXrayTraits::construct(JSContext *cx, HandleObject wrapper,
                         const JS::CallArgs &args, js::Wrapper& baseInstance)
{
    RootedObject obj(cx, getTargetObject(wrapper));
    MOZ_ASSERT(mozilla::dom::HasConstructor(obj));
    const js::Class* clasp = js::GetObjectClass(obj);
    // See comments in DOMXrayTraits::call() explaining what's going on here.
    if (clasp->flags & JSCLASS_IS_DOMIFACEANDPROTOJSCLASS) {
        if (!clasp->construct) {
            RootedValue v(cx, ObjectValue(*wrapper));
            js_ReportIsNotFunction(cx, v);
            return false;
        }
        if (!clasp->construct(cx, args.length(), args.base()))
            return false;
    } else {
        if (!baseInstance.construct(cx, wrapper, args))
            return false;
    }
    if (!args.rval().isObject() || !JS_WrapValue(cx, args.rval()))
        return false;
    return true;
}

void
DOMXrayTraits::preserveWrapper(JSObject *target)
{
    nsISupports *identity = mozilla::dom::UnwrapDOMObjectToISupports(target);
    if (!identity)
        return;
    nsWrapperCache* cache = nullptr;
    CallQueryInterface(identity, &cache);
    if (cache)
        cache->PreserveWrapper(identity);
}

JSObject*
DOMXrayTraits::createHolder(JSContext *cx, JSObject *wrapper)
{
    RootedObject global(cx, JS_GetGlobalForObject(cx, wrapper));
    return JS_NewObjectWithGivenProto(cx, nullptr, JS::NullPtr(), global);
}

template <typename Base, typename Traits>
XrayWrapper<Base, Traits>::XrayWrapper(unsigned flags)
  : Base(flags | WrapperFactory::IS_XRAY_WRAPPER_FLAG)
{
    Base::setHasPrototype(Traits::HasPrototype);
}

template <typename Base, typename Traits>
XrayWrapper<Base, Traits>::~XrayWrapper()
{
}

namespace XrayUtils {

JSObject *
GetNativePropertiesObject(JSContext *cx, JSObject *wrapper)
{
    MOZ_ASSERT(js::IsWrapper(wrapper) && WrapperFactory::IsXrayWrapper(wrapper),
               "bad object passed in");

    JSObject *holder = GetHolder(wrapper);
    MOZ_ASSERT(holder, "uninitialized wrapper being used?");
    return holder;
}

bool
IsXrayResolving(JSContext *cx, HandleObject wrapper, HandleId id)
{
    if (!WrapperFactory::IsXrayWrapper(wrapper) ||
        GetXrayType(wrapper) != XrayForWrappedNative)
    {
        return false;
    }
    JSObject *holder =
      XPCWrappedNativeXrayTraits::singleton.ensureHolder(cx, wrapper);
    return XPCWrappedNativeXrayTraits::isResolving(cx, holder, id);
}

bool
HasNativeProperty(JSContext *cx, HandleObject wrapper, HandleId id, bool *hasProp)
{
    MOZ_ASSERT(WrapperFactory::IsXrayWrapper(wrapper));
    XrayTraits *traits = GetXrayTraits(wrapper);
    MOZ_ASSERT(traits);
    RootedObject holder(cx, traits->ensureHolder(cx, wrapper));
    NS_ENSURE_TRUE(holder, false);
    *hasProp = false;
    Rooted<JSPropertyDescriptor> desc(cx);
    Wrapper *handler = Wrapper::wrapperHandler(wrapper);

    // Try resolveOwnProperty.
    Maybe<ResolvingId> resolvingId;
    if (traits == &XPCWrappedNativeXrayTraits::singleton)
        resolvingId.construct(cx, wrapper, id);
    if (!traits->resolveOwnProperty(cx, *handler, wrapper, holder, id, &desc, 0))
        return false;
    if (desc.object()) {
        *hasProp = true;
        return true;
    }

    // Try the holder.
    bool found = false;
    if (!JS_AlreadyHasOwnPropertyById(cx, holder, id, &found))
        return false;
    if (found) {
        *hasProp = true;
        return true;
    }

    // Try resolveNativeProperty.
    if (!traits->resolveNativeProperty(cx, wrapper, holder, id, &desc, 0))
        return false;
    *hasProp = !!desc.object();
    return true;
}

} // namespace XrayUtils

static bool
XrayToString(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (!args.thisv().isObject()) {
        JS_ReportError(cx, "XrayToString called on an incompatible object");
        return false;
    }

    RootedObject wrapper(cx, &args.thisv().toObject());
    if (!wrapper)
        return false;
    if (IsWrapper(wrapper) &&
        GetProxyHandler(wrapper) == &sandboxCallableProxyHandler) {
        wrapper = xpc::SandboxCallableProxyHandler::wrappedObject(wrapper);
    }
    if (!IsWrapper(wrapper) || !WrapperFactory::IsXrayWrapper(wrapper)) {
        JS_ReportError(cx, "XrayToString called on an incompatible object");
        return false;
    }

    RootedObject obj(cx, XrayTraits::getTargetObject(wrapper));

    static const char start[] = "[object XrayWrapper ";
    static const char end[] = "]";
    if (UseDOMXray(obj))
        return NativeToString(cx, wrapper, obj, start, end, args.rval());

    nsAutoString result;
    result.AppendASCII(start);

    XPCCallContext ccx(JS_CALLER, cx, obj);
    XPCWrappedNative *wn = XPCWrappedNativeXrayTraits::getWN(wrapper);
    char *wrapperStr = wn->ToString();
    if (!wrapperStr) {
        JS_ReportOutOfMemory(cx);
        return false;
    }
    result.AppendASCII(wrapperStr);
    JS_smprintf_free(wrapperStr);

    result.AppendASCII(end);

    JSString *str = JS_NewUCStringCopyN(cx, result.get(), result.Length());
    if (!str)
        return false;

    args.rval().setString(str);
    return true;
}

#ifdef DEBUG

static void
DEBUG_CheckXBLCallable(JSContext *cx, JSObject *obj)
{
    // In general, we shouldn't have cross-compartment wrappers here, because
    // we should be running in an XBL scope, and the content prototype should
    // contain wrappers to functions defined in the XBL scope. But if the node
    // has been adopted into another compartment, those prototypes will now point
    // to a different XBL scope (which is ok).
    MOZ_ASSERT_IF(js::IsCrossCompartmentWrapper(obj),
                  xpc::IsXBLScope(js::GetObjectCompartment(js::UncheckedUnwrap(obj))));
    MOZ_ASSERT(JS_ObjectIsCallable(cx, obj));
}

static void
DEBUG_CheckXBLLookup(JSContext *cx, JSPropertyDescriptor *desc)
{
    if (!desc->obj)
        return;
    if (!desc->value.isUndefined()) {
        MOZ_ASSERT(desc->value.isObject());
        DEBUG_CheckXBLCallable(cx, &desc->value.toObject());
    }
    if (desc->getter) {
        MOZ_ASSERT(desc->attrs & JSPROP_GETTER);
        DEBUG_CheckXBLCallable(cx, JS_FUNC_TO_DATA_PTR(JSObject *, desc->getter));
    }
    if (desc->setter) {
        MOZ_ASSERT(desc->attrs & JSPROP_SETTER);
        DEBUG_CheckXBLCallable(cx, JS_FUNC_TO_DATA_PTR(JSObject *, desc->setter));
    }
}
#else
#define DEBUG_CheckXBLLookup(a, b) {}
#endif

template <typename Base, typename Traits>
bool
XrayWrapper<Base, Traits>::isExtensible(JSContext *cx, JS::Handle<JSObject*> wrapper, bool *extensible)
{
    // Xray wrappers are supposed to provide a clean view of the target
    // reflector, hiding any modifications by script in the target scope.  So
    // even if that script freezes the reflector, we don't want to make that
    // visible to the caller. DOM reflectors are always extensible by default,
    // so we can just return true here.
    *extensible = true;
    return true;
}

template <typename Base, typename Traits>
bool
XrayWrapper<Base, Traits>::preventExtensions(JSContext *cx, HandleObject wrapper)
{
    // See above.
    JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_CANT_CHANGE_EXTENSIBILITY);
    return false;
}

template <typename Base, typename Traits>
bool
XrayWrapper<Base, Traits>::getPropertyDescriptor(JSContext *cx, HandleObject wrapper, HandleId id,
                                                 JS::MutableHandle<JSPropertyDescriptor> desc,
                                                 unsigned flags)
{
    assertEnteredPolicy(cx, wrapper, id, BaseProxyHandler::GET | BaseProxyHandler::SET);
    RootedObject holder(cx, Traits::singleton.ensureHolder(cx, wrapper));
    if (Traits::isResolving(cx, holder, id)) {
        desc.object().set(nullptr);
        return true;
    }

    typename Traits::ResolvingIdImpl resolving(cx, wrapper, id);

    if (!holder)
        return false;

    // Ordering is important here.
    //
    // We first need to call resolveOwnProperty, even before checking the holder,
    // because there might be a new dynamic |own| property that appears and
    // shadows a previously-resolved non-own property that we cached on the
    // holder. This can happen with indexed properties on NodeLists, for example,
    // which are |own| value props.
    //
    // resolveOwnProperty may or may not cache what it finds on the holder,
    // depending on how ephemeral it decides the property is. XPCWN |own|
    // properties generally end up on the holder via NewResolve, whereas
    // NodeList |own| properties don't get defined on the holder, since they're
    // supposed to be dynamic. This means that we have to first check the result
    // of resolveOwnProperty, and _then_, if that comes up blank, check the
    // holder for any cached native properties.
    //
    // Finally, we call resolveNativeProperty, which checks non-own properties,
    // and unconditionally caches what it finds on the holder.

    // Check resolveOwnProperty.
    if (!Traits::singleton.resolveOwnProperty(cx, *this, wrapper, holder, id, desc, flags))
        return false;

    // Check the holder.
    if (!desc.object() && !JS_GetPropertyDescriptorById(cx, holder, id, 0, desc))
        return false;
    if (desc.object()) {
        desc.object().set(wrapper);
        return true;
    }

    // Nothing in the cache. Call through, and cache the result.
    if (!Traits::singleton.resolveNativeProperty(cx, wrapper, holder, id, desc, flags))
        return false;

    // We need to handle named access on the Window somewhere other than
    // Traits::resolveOwnProperty, because per spec it happens on the Global
    // Scope Polluter and thus the resulting properties are non-|own|. However,
    // we're set up (above) to cache (on the holder) anything that comes out of
    // resolveNativeProperty, which we don't want for something dynamic like
    // named access. So we just handle it separately here.
    nsGlobalWindow *win = nullptr;
    if (!desc.object() &&
        JSID_IS_STRING(id) &&
        (win = AsWindow(cx, wrapper)))
    {
        nsDependentJSString name(id);
        nsCOMPtr<nsIDOMWindow> childDOMWin = win->GetChildWindow(name);
        if (childDOMWin) {
            nsGlobalWindow *cwin = static_cast<nsGlobalWindow*>(childDOMWin.get());
            JSObject *childObj = cwin->FastGetGlobalJSObject();
            if (MOZ_UNLIKELY(!childObj))
                return xpc::Throw(cx, NS_ERROR_FAILURE);
            FillPropertyDescriptor(desc, wrapper, ObjectValue(*childObj),
                                   /* readOnly = */ true);
            return JS_WrapPropertyDescriptor(cx, desc);
        }
    }

    if (!desc.object() &&
        id == nsXPConnect::GetRuntimeInstance()->GetStringID(XPCJSRuntime::IDX_TO_STRING))
    {

        JSFunction *toString = JS_NewFunction(cx, XrayToString, 0, 0, wrapper, "toString");
        if (!toString)
            return false;

        desc.object().set(wrapper);
        desc.setAttributes(0);
        desc.setGetter(nullptr);
        desc.setSetter(nullptr);
        desc.value().setObject(*JS_GetFunctionObject(toString));
    }

    // If we're a special scope for in-content XBL, our script expects to see
    // the bound XBL methods and attributes when accessing content. However,
    // these members are implemented in content via custom-spliced prototypes,
    // and thus aren't visible through Xray wrappers unless we handle them
    // explicitly. So we check if we're running in such a scope, and if so,
    // whether the wrappee is a bound element. If it is, we do a lookup via
    // specialized XBL machinery.
    //
    // While we have to do some sketchy walking through content land, we should
    // be protected by read-only/non-configurable properties, and any functions
    // we end up with should _always_ be living in an XBL scope (usually ours,
    // but could be another if the node has been adopted).
    //
    // Make sure to assert this.
    nsCOMPtr<nsIContent> content;
    if (!desc.object() &&
        EnsureCompartmentPrivate(wrapper)->scope->IsXBLScope() &&
        (content = do_QueryInterfaceNative(cx, wrapper)))
    {
        if (!nsContentUtils::LookupBindingMember(cx, content, id, desc))
            return false;
        DEBUG_CheckXBLLookup(cx, desc.address());
    }

    // If we still have nothing, we're done.
    if (!desc.object())
        return true;

    if (!JS_DefinePropertyById(cx, holder, id, desc.value(), desc.getter(),
                               desc.setter(), desc.attributes()) ||
        !JS_GetPropertyDescriptorById(cx, holder, id, flags, desc))
    {
        return false;
    }
    MOZ_ASSERT(desc.object());
    desc.object().set(wrapper);
    return true;
}

template <typename Base, typename Traits>
bool
XrayWrapper<Base, Traits>::getOwnPropertyDescriptor(JSContext *cx, HandleObject wrapper, HandleId id,
                                                    JS::MutableHandle<JSPropertyDescriptor> desc,
                                                    unsigned flags)
{
    assertEnteredPolicy(cx, wrapper, id, BaseProxyHandler::GET | BaseProxyHandler::SET);
    RootedObject holder(cx, Traits::singleton.ensureHolder(cx, wrapper));
    if (Traits::isResolving(cx, holder, id)) {
        desc.object().set(nullptr);
        return true;
    }

    typename Traits::ResolvingIdImpl resolving(cx, wrapper, id);

    // NB: Nothing we do here acts on the wrapped native itself, so we don't
    // enter our policy.

    if (!Traits::singleton.resolveOwnProperty(cx, *this, wrapper, holder, id, desc, flags))
        return false;
    if (desc.object())
        desc.object().set(wrapper);
    return true;
}

// Consider what happens when chrome does |xray.expando = xray.wrappedJSObject|.
//
// Since the expando comes from the target compartment, wrapping it back into
// the target compartment to define it on the expando object ends up stripping
// off the Xray waiver that gives |xray| and |xray.wrappedJSObject| different
// identities. This is generally the right thing to do when wrapping across
// compartments, but is incorrect in the special case of the Xray expando
// object. Manually re-apply Xrays if necessary.
//
// NB: In order to satisfy the invariants of WaiveXray, we need to pass
// in an object sans security wrapper, which means we need to strip off any
// potential same-compartment security wrapper that may have been applied
// to the content object. This is ok, because the the expando object is only
// ever accessed by code across the compartment boundary.
static bool
RecreateLostWaivers(JSContext *cx, JSPropertyDescriptor *orig,
                    MutableHandle<JSPropertyDescriptor> wrapped)
{
    // Compute whether the original objects were waived, and implicitly, whether
    // they were objects at all.
    bool valueWasWaived =
        orig->value.isObject() &&
        WrapperFactory::HasWaiveXrayFlag(&orig->value.toObject());
    bool getterWasWaived =
        (orig->attrs & JSPROP_GETTER) &&
        WrapperFactory::HasWaiveXrayFlag(JS_FUNC_TO_DATA_PTR(JSObject*, orig->getter));
    bool setterWasWaived =
        (orig->attrs & JSPROP_SETTER) &&
        WrapperFactory::HasWaiveXrayFlag(JS_FUNC_TO_DATA_PTR(JSObject*, orig->setter));

    // Recreate waivers. Note that for value, we need an extra UncheckedUnwrap
    // to handle same-compartment security wrappers (see above). This should
    // never happen for getters/setters.

    RootedObject rewaived(cx);
    if (valueWasWaived && !IsCrossCompartmentWrapper(&wrapped.value().toObject())) {
        rewaived = &wrapped.value().toObject();
        rewaived = WrapperFactory::WaiveXray(cx, UncheckedUnwrap(rewaived));
        NS_ENSURE_TRUE(rewaived, false);
        wrapped.value().set(ObjectValue(*rewaived));
    }
    if (getterWasWaived && !IsCrossCompartmentWrapper(wrapped.getterObject())) {
        MOZ_ASSERT(CheckedUnwrap(wrapped.getterObject()));
        rewaived = WrapperFactory::WaiveXray(cx, wrapped.getterObject());
        NS_ENSURE_TRUE(rewaived, false);
        wrapped.setGetterObject(rewaived);
    }
    if (setterWasWaived && !IsCrossCompartmentWrapper(wrapped.setterObject())) {
        MOZ_ASSERT(CheckedUnwrap(wrapped.setterObject()));
        rewaived = WrapperFactory::WaiveXray(cx, wrapped.setterObject());
        NS_ENSURE_TRUE(rewaived, false);
        wrapped.setSetterObject(rewaived);
    }

    return true;
}

template <typename Base, typename Traits>
bool
XrayWrapper<Base, Traits>::defineProperty(JSContext *cx, HandleObject wrapper,
                                          HandleId id, MutableHandle<JSPropertyDescriptor> desc)
{
    assertEnteredPolicy(cx, wrapper, id, BaseProxyHandler::SET);

    // NB: We still need JSRESOLVE_ASSIGNING here for the time being, because it
    // tells things like nodelists whether they should create the property or not.
    Rooted<JSPropertyDescriptor> existing_desc(cx);
    if (!getOwnPropertyDescriptor(cx, wrapper, id, &existing_desc, JSRESOLVE_ASSIGNING))
        return false;

    if (existing_desc.object() && existing_desc.isPermanent())
        return true; // silently ignore attempt to overwrite native property

    bool defined = false;
    if (!Traits::defineProperty(cx, wrapper, id, desc, existing_desc, &defined))
        return false;
    if (defined)
        return true;

    // We're placing an expando. The expando objects live in the target
    // compartment, so we need to enter it.
    RootedObject target(cx, Traits::singleton.getTargetObject(wrapper));
    JSAutoCompartment ac(cx, target);

    // Grab the relevant expando object.
    RootedObject expandoObject(cx, Traits::singleton.ensureExpandoObject(cx, wrapper,
                                                                         target));
    if (!expandoObject)
        return false;

    // Wrap the property descriptor for the target compartment.
    Rooted<JSPropertyDescriptor> wrappedDesc(cx, desc);
    if (!JS_WrapPropertyDescriptor(cx, &wrappedDesc))
        return false;

    // Fix up Xray waivers.
    if (!RecreateLostWaivers(cx, desc.address(), &wrappedDesc))
        return false;

    return JS_DefinePropertyById(cx, expandoObject, id, wrappedDesc.value(),
                                 wrappedDesc.getter(), wrappedDesc.setter(),
                                 wrappedDesc.get().attrs);
}

template <typename Base, typename Traits>
bool
XrayWrapper<Base, Traits>::getOwnPropertyNames(JSContext *cx, HandleObject wrapper,
                                               AutoIdVector &props)
{
    assertEnteredPolicy(cx, wrapper, JSID_VOID, BaseProxyHandler::ENUMERATE);
    return enumerate(cx, wrapper, JSITER_OWNONLY | JSITER_HIDDEN, props);
}

template <typename Base, typename Traits>
bool
XrayWrapper<Base, Traits>::delete_(JSContext *cx, HandleObject wrapper,
                                   HandleId id, bool *bp)
{
    assertEnteredPolicy(cx, wrapper, id, BaseProxyHandler::SET);

    // Check the expando object.
    RootedObject target(cx, Traits::getTargetObject(wrapper));
    RootedObject expando(cx, Traits::singleton.getExpandoObject(cx, target, wrapper));
    if (expando) {
        JSAutoCompartment ac(cx, expando);
        return JS_DeletePropertyById2(cx, expando, id, bp);
    }
    *bp = true;
    return true;
}

template <typename Base, typename Traits>
bool
XrayWrapper<Base, Traits>::enumerate(JSContext *cx, HandleObject wrapper, unsigned flags,
                                     AutoIdVector &props)
{
    assertEnteredPolicy(cx, wrapper, JSID_VOID, BaseProxyHandler::ENUMERATE);
    if (!AccessCheck::wrapperSubsumes(wrapper)) {
        JS_ReportError(cx, "Not allowed to enumerate cross origin objects");
        return false;
    }

    // Enumerate expando properties first. Note that the expando object lives
    // in the target compartment.
    RootedObject target(cx, Traits::singleton.getTargetObject(wrapper));
    RootedObject expando(cx, Traits::singleton.getExpandoObject(cx, target, wrapper));
    if (expando) {
        JSAutoCompartment ac(cx, expando);
        if (!js::GetPropertyNames(cx, expando, flags, &props))
            return false;
    }
    if (!JS_WrapAutoIdVector(cx, props))
        return false;

    return Traits::enumerateNames(cx, wrapper, flags, props);
}

template <typename Base, typename Traits>
bool
XrayWrapper<Base, Traits>::enumerate(JSContext *cx, HandleObject wrapper,
                                    AutoIdVector &props)
{
    return enumerate(cx, wrapper, 0, props);
}

template <typename Base, typename Traits>
bool
XrayWrapper<Base, Traits>::get(JSContext *cx, HandleObject wrapper,
                               HandleObject receiver, HandleId id,
                               MutableHandleValue vp)
{
    // Skip our Base if it isn't already ProxyHandler.
    // NB: None of the functions we call are prepared for the receiver not
    // being the wrapper, so ignore the receiver here.
    return js::BaseProxyHandler::get(cx, wrapper, Traits::HasPrototype ? receiver : wrapper, id, vp);
}

template <typename Base, typename Traits>
bool
XrayWrapper<Base, Traits>::set(JSContext *cx, HandleObject wrapper,
                               HandleObject receiver, HandleId id,
                               bool strict, MutableHandleValue vp)
{
    // Skip our Base if it isn't already BaseProxyHandler.
    // NB: None of the functions we call are prepared for the receiver not
    // being the wrapper, so ignore the receiver here.
    return js::BaseProxyHandler::set(cx, wrapper, Traits::HasPrototype ? receiver : wrapper, id, strict, vp);
}

template <typename Base, typename Traits>
bool
XrayWrapper<Base, Traits>::has(JSContext *cx, HandleObject wrapper,
                               HandleId id, bool *bp)
{
    // Skip our Base if it isn't already ProxyHandler.
    return js::BaseProxyHandler::has(cx, wrapper, id, bp);
}

template <typename Base, typename Traits>
bool
XrayWrapper<Base, Traits>::hasOwn(JSContext *cx, HandleObject wrapper,
                                  HandleId id, bool *bp)
{
    // Skip our Base if it isn't already ProxyHandler.
    return js::BaseProxyHandler::hasOwn(cx, wrapper, id, bp);
}

template <typename Base, typename Traits>
bool
XrayWrapper<Base, Traits>::keys(JSContext *cx, HandleObject wrapper,
                                AutoIdVector &props)
{
    // Skip our Base if it isn't already ProxyHandler.
    return js::BaseProxyHandler::keys(cx, wrapper, props);
}

template <typename Base, typename Traits>
bool
XrayWrapper<Base, Traits>::iterate(JSContext *cx, HandleObject wrapper,
                                   unsigned flags, MutableHandleValue vp)
{
    // Skip our Base if it isn't already ProxyHandler.
    return js::BaseProxyHandler::iterate(cx, wrapper, flags, vp);
}

template <typename Base, typename Traits>
bool
XrayWrapper<Base, Traits>::call(JSContext *cx, HandleObject wrapper, const JS::CallArgs &args)
{
    assertEnteredPolicy(cx, wrapper, JSID_VOID, BaseProxyHandler::CALL);
    return Traits::call(cx, wrapper, args, Base::singleton);
}

template <typename Base, typename Traits>
bool
XrayWrapper<Base, Traits>::construct(JSContext *cx, HandleObject wrapper, const JS::CallArgs &args)
{
    assertEnteredPolicy(cx, wrapper, JSID_VOID, BaseProxyHandler::CALL);
    return Traits::construct(cx, wrapper, args, Base::singleton);
}

template <typename Base, typename Traits>
bool
XrayWrapper<Base, Traits>::defaultValue(JSContext *cx, HandleObject wrapper,
                                        JSType hint, MutableHandleValue vp)
{
    // Even if this isn't a security wrapper, Xray semantics dictate that we
    // run the DefaultValue algorithm directly on the Xray wrapper.
    //
    // NB: We don't have to worry about things with special [[DefaultValue]]
    // behavior like Date because we'll never have an XrayWrapper to them.
    return js::DefaultValue(cx, wrapper, hint, vp);
}

template <typename Base, typename Traits>
bool
XrayWrapper<Base, Traits>::getPrototypeOf(JSContext *cx, JS::HandleObject wrapper,
                                          JS::MutableHandleObject protop)
{
    // We really only want this override for non-SecurityWrapper-inheriting
    // |Base|. But doing that statically with templates requires partial method
    // specializations (and therefore a helper class), which is all more trouble
    // than it's worth. Do a dynamic check.
    if (Base::hasSecurityPolicy())
        return Base::getPrototypeOf(cx, wrapper, protop);

    RootedObject target(cx, Traits::getTargetObject(wrapper));
    RootedObject expando(cx, Traits::singleton.getExpandoObject(cx, target, wrapper));

    // We want to keep the Xray's prototype distinct from that of content, but
    // only if there's been a set. If there's not an expando, or the expando
    // slot is |undefined|, hand back the default proto, appropriately wrapped.

    RootedValue v(cx);
    if (expando) {
        JSAutoCompartment ac(cx, expando);
        v = JS_GetReservedSlot(expando, JSSLOT_EXPANDO_PROTOTYPE);
    }
    if (v.isUndefined())
        return getPrototypeOfHelper(cx, wrapper, target, protop);

    protop.set(v.toObjectOrNull());
    return JS_WrapObject(cx, protop);
}

template <typename Base, typename Traits>
bool
XrayWrapper<Base, Traits>::setPrototypeOf(JSContext *cx, JS::HandleObject wrapper,
                                          JS::HandleObject proto, bool *bp)
{
    // Do this only for non-SecurityWrapper-inheriting |Base|. See the comment
    // in getPrototypeOf().
    if (Base::hasSecurityPolicy())
        return Base::setPrototypeOf(cx, wrapper, proto, bp);

    RootedObject target(cx, Traits::getTargetObject(wrapper));
    RootedObject expando(cx, Traits::singleton.ensureExpandoObject(cx, wrapper, target));

    // The expando lives in the target's compartment, so do our installation there.
    JSAutoCompartment ac(cx, target);

    RootedValue v(cx, ObjectValue(*proto));
    if (!JS_WrapValue(cx, &v))
        return false;
    JS_SetReservedSlot(expando, JSSLOT_EXPANDO_PROTOTYPE, v);
    *bp = true;
    return true;
}


/*
 * The Permissive / Security variants should be used depending on whether the
 * compartment of the wrapper is guranteed to subsume the compartment of the
 * wrapped object (i.e. - whether it is safe from a security perspective to
 * unwrap the wrapper).
 */

template<>
PermissiveXrayXPCWN PermissiveXrayXPCWN::singleton(0);
template class PermissiveXrayXPCWN;

template<>
SecurityXrayXPCWN SecurityXrayXPCWN::singleton(0);
template class SecurityXrayXPCWN;

template<>
PermissiveXrayDOM PermissiveXrayDOM::singleton(0);
template class PermissiveXrayDOM;

template<>
SecurityXrayDOM SecurityXrayDOM::singleton(0);
template class SecurityXrayDOM;

template<>
PermissiveXrayJS PermissiveXrayJS::singleton(0);
template class PermissiveXrayJS;

template<>
SCSecurityXrayXPCWN SCSecurityXrayXPCWN::singleton(0);
template class SCSecurityXrayXPCWN;

static nsQueryInterface
do_QueryInterfaceNative(JSContext* cx, HandleObject wrapper)
{
    nsISupports* nativeSupports;
    if (IsWrapper(wrapper) && WrapperFactory::IsXrayWrapper(wrapper)) {
        RootedObject target(cx, XrayTraits::getTargetObject(wrapper));
        if (GetXrayType(target) == XrayForDOMObject) {
            nativeSupports = UnwrapDOMObjectToISupports(target);
        } else {
            XPCWrappedNative *wn = XPCWrappedNative::Get(target);
            nativeSupports = wn->Native();
        }
    } else {
        nsIXPConnect *xpc = nsXPConnect::XPConnect();
        nativeSupports = xpc->GetNativeOfWrapper(cx, wrapper);
    }

    return nsQueryInterface(nativeSupports);
}

}
