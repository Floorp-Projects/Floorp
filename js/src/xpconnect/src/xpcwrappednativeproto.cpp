/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is .
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   John Bandhauer <jband@netscape.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

/* Possibly shared proto object for XPCWrappedNative. */

#include "xpcprivate.h"

#ifdef DEBUG
PRInt32 XPCWrappedNativeProto::gDEBUG_LiveProtoCount = 0;
#endif

MOZ_DECL_CTOR_COUNTER(XPCWrappedNativeProto)

XPCWrappedNativeProto::XPCWrappedNativeProto(XPCWrappedNativeScope* Scope,
                                             nsIClassInfo* ClassInfo,
                                             PRUint32 ClassInfoFlags,
                                             XPCNativeSet* Set)
    : mScope(Scope),
      mJSProtoObject(nsnull),
      mClassInfo(ClassInfo),
      mClassInfoFlags(0),
      mSet(Set),
      mSecurityInfo(nsnull),
      mScriptableInfo(nsnull)
{
    // This native object lives as long as its associated JSObject - killed
    // by finalization of the JSObject (or explicitly if Init fails).

    MOZ_COUNT_CTOR(XPCWrappedNativeProto);

#ifdef DEBUG
    PR_AtomicIncrement(&gDEBUG_LiveProtoCount);
#endif
}

XPCWrappedNativeProto::~XPCWrappedNativeProto()
{
    NS_ASSERTION(!mJSProtoObject, "JSProtoObject still alive");

    MOZ_COUNT_DTOR(XPCWrappedNativeProto);

#ifdef DEBUG
    PR_AtomicDecrement(&gDEBUG_LiveProtoCount);
#endif
    
    // Note that our weak ref to mScope is not to be trusted at this point.

    XPCNativeSet::ClearCacheEntryForClassInfo(mClassInfo);

    delete mScriptableInfo;
}

JSBool
XPCWrappedNativeProto::Init(
                XPCCallContext& ccx,
                const XPCNativeScriptableCreateInfo* scriptableCreateInfo)
{
    if(scriptableCreateInfo && scriptableCreateInfo->GetCallback())
    {
        mScriptableInfo =
            XPCNativeScriptableInfo::Construct(ccx, scriptableCreateInfo);
        if(!mScriptableInfo)
            return JS_FALSE;
    }

    JSClass* jsclazz = mScriptableInfo &&
                       mScriptableInfo->GetFlags().AllowPropModsToPrototype() ?
                            &XPC_WN_ModsAllowed_Proto_JSClass :
                            &XPC_WN_NoMods_Proto_JSClass;

    mJSProtoObject = JS_NewObject(ccx, jsclazz,
                                  mScope->GetPrototypeJSObject(),
                                  mScope->GetGlobalJSObject());

    JSBool retval = mJSProtoObject && JS_SetPrivate(ccx, mJSProtoObject, this);

    DEBUG_ReportShadowedMembers(mSet, nsnull, this);

    return retval;
}

void
XPCWrappedNativeProto::JSProtoObjectFinalized(JSContext *cx, JSObject *obj)
{
    NS_ASSERTION(obj == mJSProtoObject, "huh?");

    // Map locking is not necessary since we are running gc.

    if(IsShared())
    {
        // Only remove this proto from the map if it is the one in the map.
        ClassInfo2WrappedNativeProtoMap* map = 
            GetScope()->GetWrappedNativeProtoMap();
        if(map->Find(mClassInfo) == this)
            map->Remove(mClassInfo);
    }

    GetRuntime()->GetDetachedWrappedNativeProtoMap()->Remove(this);
    GetRuntime()->GetDyingWrappedNativeProtoMap()->Add(this);

    mJSProtoObject = nsnull;
}

void
XPCWrappedNativeProto::SystemIsBeingShutDown(XPCCallContext& ccx)
{
    // Note that the instance might receive this call multiple times
    // as we walk to here from various places.

#ifdef XPC_TRACK_PROTO_STATS
    static PRBool DEBUG_DumpedStats = PR_FALSE;
    if(!DEBUG_DumpedStats)
    {
        printf("%d XPCWrappedNativeProto(s) alive at shutdown\n",
               gDEBUG_LiveProtoCount);
        DEBUG_DumpedStats = PR_TRUE;
    }
#endif

    if(mJSProtoObject)
    {
        // short circuit future finalization
        JS_SetPrivate(ccx, mJSProtoObject, nsnull);
        mJSProtoObject = nsnull;
    }
}

// static
XPCWrappedNativeProto*
XPCWrappedNativeProto::GetNewOrUsed(XPCCallContext& ccx,
                                    XPCWrappedNativeScope* Scope,
                                    nsIClassInfo* ClassInfo,
                                    const XPCNativeScriptableCreateInfo* ScriptableCreateInfo,
                                    JSBool ForceNoSharing)
{
    NS_ASSERTION(Scope, "bad param");
    NS_ASSERTION(ClassInfo, "bad param");

    AutoMarkingWrappedNativeProtoPtr proto(ccx);
    ClassInfo2WrappedNativeProtoMap* map;
    XPCLock* lock;
    JSBool shared;

    JSUint32 ciFlags;
    if(NS_FAILED(ClassInfo->GetFlags(&ciFlags)))
        ciFlags = 0;

    if(ciFlags & XPC_PROTO_DONT_SHARE)
    {
        NS_ERROR("reserved flag set!");
        ciFlags &= ~XPC_PROTO_DONT_SHARE;
    }

    if(ForceNoSharing || (ciFlags & nsIClassInfo::PLUGIN_OBJECT) ||
       (ScriptableCreateInfo &&
        ScriptableCreateInfo->GetFlags().DontSharePrototype()))
    {
        ciFlags |= XPC_PROTO_DONT_SHARE;
        shared = JS_FALSE;
    }
    else
    {
        shared = JS_TRUE;
    }

    if(shared)
    {
        map = Scope->GetWrappedNativeProtoMap();
        lock = Scope->GetRuntime()->GetMapLock();
        {   // scoped lock
            XPCAutoLock al(lock);
            proto = map->Find(ClassInfo);
            if(proto)
                return proto;
        }
    }

    AutoMarkingNativeSetPtr set(ccx);
    set = XPCNativeSet::GetNewOrUsed(ccx, ClassInfo);
    if(!set)
        return nsnull;

    proto = new XPCWrappedNativeProto(Scope, ClassInfo, ciFlags, set);

    if(!proto || !proto->Init(ccx, ScriptableCreateInfo))
    {
        delete proto.get();
        return nsnull;
    }

    if(shared)
    {   // scoped lock
        XPCAutoLock al(lock);
        map->Add(ClassInfo, proto);
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
        XPC_LOG_ALWAYS(("mJSProtoObject @ %x", mJSProtoObject));
        XPC_LOG_ALWAYS(("mSet @ %x", mSet));
        XPC_LOG_ALWAYS(("mSecurityInfo of %x", mSecurityInfo));
        XPC_LOG_ALWAYS(("mScriptableInfo @ %x", mScriptableInfo));
        if(depth && mScriptableInfo)
        {
            XPC_LOG_INDENT();
            XPC_LOG_ALWAYS(("mScriptable @ %x", mScriptableInfo->GetCallback()));
            XPC_LOG_ALWAYS(("mFlags of %x", (PRUint32)mScriptableInfo->GetFlags()));
            XPC_LOG_ALWAYS(("mJSClass @ %x", mScriptableInfo->GetJSClass()));
            XPC_LOG_OUTDENT();
        }
    XPC_LOG_OUTDENT();
#endif
}


