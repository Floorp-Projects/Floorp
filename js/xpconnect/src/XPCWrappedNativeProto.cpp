/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Shared proto object for XPCWrappedNative. */

#include "xpcprivate.h"

#if defined(DEBUG_xpc_hacker) || defined(DEBUG)
PRInt32 XPCWrappedNativeProto::gDEBUG_LiveProtoCount = 0;
#endif

XPCWrappedNativeProto::XPCWrappedNativeProto(XPCWrappedNativeScope* Scope,
                                             nsIClassInfo* ClassInfo,
                                             PRUint32 ClassInfoFlags,
                                             XPCNativeSet* Set,
                                             QITableEntry* offsets)
    : mScope(Scope),
      mJSProtoObject(nsnull),
      mClassInfo(ClassInfo),
      mClassInfoFlags(ClassInfoFlags),
      mSet(Set),
      mSecurityInfo(nsnull),
      mScriptableInfo(nsnull),
      mOffsets(offsets)
{
    // This native object lives as long as its associated JSObject - killed
    // by finalization of the JSObject (or explicitly if Init fails).

    MOZ_COUNT_CTOR(XPCWrappedNativeProto);

#ifdef DEBUG
    PR_ATOMIC_INCREMENT(&gDEBUG_LiveProtoCount);
#endif
}

XPCWrappedNativeProto::~XPCWrappedNativeProto()
{
    NS_ASSERTION(!mJSProtoObject, "JSProtoObject still alive");

    MOZ_COUNT_DTOR(XPCWrappedNativeProto);

#ifdef DEBUG
    PR_ATOMIC_DECREMENT(&gDEBUG_LiveProtoCount);
#endif

    // Note that our weak ref to mScope is not to be trusted at this point.

    XPCNativeSet::ClearCacheEntryForClassInfo(mClassInfo);

    delete mScriptableInfo;
}

JSBool
XPCWrappedNativeProto::Init(XPCCallContext& ccx,
                            const XPCNativeScriptableCreateInfo* scriptableCreateInfo,
                            bool callPostCreatePrototype)
{
    nsIXPCScriptable *callback = scriptableCreateInfo ?
                                 scriptableCreateInfo->GetCallback() :
                                 nsnull;
    if (callback) {
        mScriptableInfo =
            XPCNativeScriptableInfo::Construct(ccx, scriptableCreateInfo);
        if (!mScriptableInfo)
            return false;
    }

    js::Class* jsclazz;


    if (mScriptableInfo) {
        const XPCNativeScriptableFlags& flags(mScriptableInfo->GetFlags());

        if (flags.AllowPropModsToPrototype()) {
            jsclazz = flags.WantCall() ?
                &XPC_WN_ModsAllowed_WithCall_Proto_JSClass :
                &XPC_WN_ModsAllowed_NoCall_Proto_JSClass;
        } else {
            jsclazz = flags.WantCall() ?
                &XPC_WN_NoMods_WithCall_Proto_JSClass :
                &XPC_WN_NoMods_NoCall_Proto_JSClass;
        }
    } else {
        jsclazz = &XPC_WN_NoMods_NoCall_Proto_JSClass;
    }

    JSObject *parent = mScope->GetGlobalJSObject();

    mJSProtoObject =
        xpc_NewSystemInheritingJSObject(ccx, js::Jsvalify(jsclazz),
                                        mScope->GetPrototypeJSObject(),
                                        true, parent);

    bool success = !!mJSProtoObject;
    if (success) {
        JS_SetPrivate(mJSProtoObject, this);
        if (callPostCreatePrototype)
            success = CallPostCreatePrototype(ccx);
    }

    DEBUG_ReportShadowedMembers(mSet, nsnull, this);

    return success;
}

bool
XPCWrappedNativeProto::CallPostCreatePrototype(XPCCallContext& ccx)
{
    // Nothing to do if we don't have a scriptable callback.
    nsIXPCScriptable *callback = mScriptableInfo ? mScriptableInfo->GetCallback()
                                                 : nsnull;
    if (!callback)
        return true;

    // Call the helper. This can handle being called if it's not implemented,
    // so we don't have to check any sort of "want" here. See xpc_map_end.h.
    nsresult rv = callback->PostCreatePrototype(ccx, mJSProtoObject);
    if (NS_FAILED(rv)) {
        JS_SetPrivate(mJSProtoObject, nsnull);
        mJSProtoObject = nsnull;
        XPCThrower::Throw(rv, ccx);
        return false;
    }

    return true;
}

void
XPCWrappedNativeProto::JSProtoObjectFinalized(js::FreeOp *fop, JSObject *obj)
{
    NS_ASSERTION(obj == mJSProtoObject, "huh?");

    // Map locking is not necessary since we are running gc.

    // Only remove this proto from the map if it is the one in the map.
    ClassInfo2WrappedNativeProtoMap* map =
        GetScope()->GetWrappedNativeProtoMap(ClassIsMainThreadOnly());
    if (map->Find(mClassInfo) == this)
        map->Remove(mClassInfo);

    GetRuntime()->GetDetachedWrappedNativeProtoMap()->Remove(this);
    GetRuntime()->GetDyingWrappedNativeProtoMap()->Add(this);

    mJSProtoObject.finalize(js::CastToJSFreeOp(fop)->runtime());
}

void
XPCWrappedNativeProto::SystemIsBeingShutDown()
{
    // Note that the instance might receive this call multiple times
    // as we walk to here from various places.

#ifdef XPC_TRACK_PROTO_STATS
    static bool DEBUG_DumpedStats = false;
    if (!DEBUG_DumpedStats) {
        printf("%d XPCWrappedNativeProto(s) alive at shutdown\n",
               gDEBUG_LiveProtoCount);
        DEBUG_DumpedStats = true;
    }
#endif

    if (mJSProtoObject) {
        // short circuit future finalization
        JS_SetPrivate(mJSProtoObject, nsnull);
        mJSProtoObject = nsnull;
    }
}

// static
XPCWrappedNativeProto*
XPCWrappedNativeProto::GetNewOrUsed(XPCCallContext& ccx,
                                    XPCWrappedNativeScope* scope,
                                    nsIClassInfo* classInfo,
                                    const XPCNativeScriptableCreateInfo* scriptableCreateInfo,
                                    QITableEntry* offsets,
                                    bool callPostCreatePrototype)
{
    NS_ASSERTION(scope, "bad param");
    NS_ASSERTION(classInfo, "bad param");

    AutoMarkingWrappedNativeProtoPtr proto(ccx);
    ClassInfo2WrappedNativeProtoMap* map = nsnull;
    XPCLock* lock = nsnull;

    uint32_t ciFlags;
    if (NS_FAILED(classInfo->GetFlags(&ciFlags)))
        ciFlags = 0;

    JSBool mainThreadOnly = !!(ciFlags & nsIClassInfo::MAIN_THREAD_ONLY);
    map = scope->GetWrappedNativeProtoMap(mainThreadOnly);
    lock = mainThreadOnly ? nsnull : scope->GetRuntime()->GetMapLock();
    {   // scoped lock
        XPCAutoLock al(lock);
        proto = map->Find(classInfo);
        if (proto)
            return proto;
    }

    AutoMarkingNativeSetPtr set(ccx);
    set = XPCNativeSet::GetNewOrUsed(ccx, classInfo);
    if (!set)
        return nsnull;

    proto = new XPCWrappedNativeProto(scope, classInfo, ciFlags, set, offsets);

    if (!proto || !proto->Init(ccx, scriptableCreateInfo, callPostCreatePrototype)) {
        delete proto.get();
        return nsnull;
    }

    {   // scoped lock
        XPCAutoLock al(lock);
        map->Add(classInfo, proto);
    }

    return proto;
}

void
XPCWrappedNativeProto::DebugDump(PRInt16 depth)
{
#ifdef DEBUG
    depth-- ;
    XPC_LOG_ALWAYS(("XPCWrappedNativeProto @ %x", this));
    XPC_LOG_INDENT();
        XPC_LOG_ALWAYS(("gDEBUG_LiveProtoCount is %d", gDEBUG_LiveProtoCount));
        XPC_LOG_ALWAYS(("mScope @ %x", mScope));
        XPC_LOG_ALWAYS(("mJSProtoObject @ %x", mJSProtoObject.get()));
        XPC_LOG_ALWAYS(("mSet @ %x", mSet));
        XPC_LOG_ALWAYS(("mSecurityInfo of %x", mSecurityInfo));
        XPC_LOG_ALWAYS(("mScriptableInfo @ %x", mScriptableInfo));
        if (depth && mScriptableInfo) {
            XPC_LOG_INDENT();
            XPC_LOG_ALWAYS(("mScriptable @ %x", mScriptableInfo->GetCallback()));
            XPC_LOG_ALWAYS(("mFlags of %x", (PRUint32)mScriptableInfo->GetFlags()));
            XPC_LOG_ALWAYS(("mJSClass @ %x", mScriptableInfo->GetJSClass()));
            XPC_LOG_OUTDENT();
        }
    XPC_LOG_OUTDENT();
#endif
}


