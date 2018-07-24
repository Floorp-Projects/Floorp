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

    RecordReplayRegisterDeferredFinalizeThing(nullptr, nullptr, mClassInfo);
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

    DeferredFinalize(mClassInfo.forget().take());
}

bool
XPCWrappedNativeProto::Init(nsIXPCScriptable* scriptable)
{
    AutoJSContext cx;
    mScriptable = scriptable;

    JS::RootedObject proto(cx, JS::GetRealmObjectPrototype(cx));
    mJSProtoObject = JS_NewObjectWithUniqueType(cx, js::Jsvalify(&XPC_WN_Proto_JSClass),
                                                proto);

    bool success = !!mJSProtoObject;
    if (success) {
        JS_SetPrivate(mJSProtoObject, this);
    }

    return success;
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
                                    nsIXPCScriptable* scriptable)
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

    if (!proto || !proto->Init(scriptable)) {
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


