/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Shared proto object for XPCWrappedNative. */

#include "xpcprivate.h"
#include "pratom.h"

using namespace mozilla;

#ifdef DEBUG
int32_t XPCWrappedNativeProto::gDEBUG_LiveProtoCount = 0;
#endif

XPCWrappedNativeProto::XPCWrappedNativeProto(XPCWrappedNativeScope* Scope,
                                             nsIClassInfo* ClassInfo,
                                             already_AddRefed<XPCNativeSet>&& Set)
    : mScope(Scope),
      mJSProtoObject(nullptr),
      mClassInfo(ClassInfo),
      mSet(Set)
{
    // This native object lives as long as its associated JSObject - killed
    // by finalization of the JSObject (or explicitly if Init fails).

    MOZ_COUNT_CTOR(XPCWrappedNativeProto);
    MOZ_ASSERT(mScope);

#ifdef DEBUG
    gDEBUG_LiveProtoCount++;
#endif
}

XPCWrappedNativeProto::~XPCWrappedNativeProto()
{
    MOZ_ASSERT(!mJSProtoObject, "JSProtoObject still alive");

    MOZ_COUNT_DTOR(XPCWrappedNativeProto);

#ifdef DEBUG
    gDEBUG_LiveProtoCount--;
#endif

    // Note that our weak ref to mScope is not to be trusted at this point.

    XPCNativeSet::ClearCacheEntryForClassInfo(mClassInfo);
}

bool
XPCWrappedNativeProto::Init(nsIXPCScriptable* scriptable,
                            bool callPostCreatePrototype)
{
    AutoJSContext cx;
    mScriptable = scriptable;

    JS::RootedObject global(cx, mScope->GetGlobalJSObject());
    JS::RootedObject proto(cx, JS_GetObjectPrototype(cx, global));
    mJSProtoObject = JS_NewObjectWithUniqueType(cx, js::Jsvalify(&XPC_WN_Proto_JSClass),
                                                proto);

    bool success = !!mJSProtoObject;
    if (success) {
        JS_SetPrivate(mJSProtoObject, this);
        if (callPostCreatePrototype)
            success = CallPostCreatePrototype();
    }

    return success;
}

bool
XPCWrappedNativeProto::CallPostCreatePrototype()
{
    AutoJSContext cx;

    // Nothing to do if we don't have a scriptable callback.
    if (!mScriptable)
        return true;

    // Call the helper. This can handle being called if it's not implemented,
    // so we don't have to check any sort of "want" here. See xpc_map_end.h.
    nsresult rv = mScriptable->PostCreatePrototype(cx, mJSProtoObject);
    if (NS_FAILED(rv)) {
        JS_SetPrivate(mJSProtoObject, nullptr);
        mJSProtoObject = nullptr;
        XPCThrower::Throw(rv, cx);
        return false;
    }

    return true;
}

void
XPCWrappedNativeProto::JSProtoObjectFinalized(js::FreeOp* fop, JSObject* obj)
{
    MOZ_ASSERT(obj == mJSProtoObject, "huh?");

#ifdef DEBUG
    // Check that this object has already been swept from the map.
    ClassInfo2WrappedNativeProtoMap* map = GetScope()->GetWrappedNativeProtoMap();
    MOZ_ASSERT(map->Find(mClassInfo) != this);
#endif

    GetRuntime()->GetDyingWrappedNativeProtoMap()->Add(this);

    mJSProtoObject.finalize(js::CastToJSFreeOp(fop)->runtime());
}

void
XPCWrappedNativeProto::JSProtoObjectMoved(JSObject* obj, const JSObject* old)
{
    MOZ_ASSERT(mJSProtoObject == old);
    mJSProtoObject.init(obj); // Update without triggering barriers.
}

void
XPCWrappedNativeProto::SystemIsBeingShutDown()
{
    // Note that the instance might receive this call multiple times
    // as we walk to here from various places.

    if (mJSProtoObject) {
        // short circuit future finalization
        JS_SetPrivate(mJSProtoObject, nullptr);
        mJSProtoObject = nullptr;
    }
}

// static
XPCWrappedNativeProto*
XPCWrappedNativeProto::GetNewOrUsed(XPCWrappedNativeScope* scope,
                                    nsIClassInfo* classInfo,
                                    nsIXPCScriptable* scriptable,
                                    bool callPostCreatePrototype)
{
    AutoJSContext cx;
    MOZ_ASSERT(scope, "bad param");
    MOZ_ASSERT(classInfo, "bad param");

    AutoMarkingWrappedNativeProtoPtr proto(cx);
    ClassInfo2WrappedNativeProtoMap* map = nullptr;

    map = scope->GetWrappedNativeProtoMap();
    proto = map->Find(classInfo);
    if (proto)
        return proto;

    RefPtr<XPCNativeSet> set = XPCNativeSet::GetNewOrUsed(classInfo);
    if (!set)
        return nullptr;

    proto = new XPCWrappedNativeProto(scope, classInfo, set.forget());

    if (!proto || !proto->Init(scriptable, callPostCreatePrototype)) {
        delete proto.get();
        return nullptr;
    }

    map->Add(classInfo, proto);

    return proto;
}

void
XPCWrappedNativeProto::DebugDump(int16_t depth)
{
#ifdef DEBUG
    depth-- ;
    XPC_LOG_ALWAYS(("XPCWrappedNativeProto @ %p", this));
    XPC_LOG_INDENT();
        XPC_LOG_ALWAYS(("gDEBUG_LiveProtoCount is %d", gDEBUG_LiveProtoCount));
        XPC_LOG_ALWAYS(("mScope @ %p", mScope));
        XPC_LOG_ALWAYS(("mJSProtoObject @ %p", mJSProtoObject.get()));
        XPC_LOG_ALWAYS(("mSet @ %p", mSet.get()));
        XPC_LOG_ALWAYS(("mScriptable @ %p", mScriptable.get()));
        if (depth && mScriptable) {
            XPC_LOG_INDENT();
            XPC_LOG_ALWAYS(("mFlags of %x", mScriptable->GetScriptableFlags()));
            XPC_LOG_ALWAYS(("mJSClass @ %p", mScriptable->GetJSClass()));
            XPC_LOG_OUTDENT();
        }
    XPC_LOG_OUTDENT();
#endif
}


