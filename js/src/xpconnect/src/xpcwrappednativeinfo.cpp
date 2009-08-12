/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
// vim:cindent:ts=8:et:sw=4:
/*
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
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
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/* Manage the shared info about interfaces for use by wrappedNatives. */

#include "xpcprivate.h"

/***************************************************************************/

/*
 * Helper that clones JS Function objects along with both of its
 * reserved slots.
 */

JSObject *
xpc_CloneJSFunction(XPCCallContext &ccx, JSObject *funobj, JSObject *parent)
{
    JSObject *clone = JS_CloneFunctionObject(ccx, funobj, parent);
    if(!clone)
        return nsnull;

    AUTO_MARK_JSVAL(ccx, OBJECT_TO_JSVAL(clone));

    XPCWrappedNativeScope *scope = 
        XPCWrappedNativeScope::FindInJSObjectScope(ccx, parent);

    if (!scope) {
        return nsnull;
    }

    // Make sure to break the prototype chain to the function object
    // we cloned to prevent its scope from leaking into the clones
    // scope.
    JS_SetPrototype(ccx, clone, scope->GetPrototypeJSFunction());

    // Copy the reserved slots to the clone.
    jsval ifaceVal, memberVal;
    if(!JS_GetReservedSlot(ccx, funobj, 0, &ifaceVal) ||
       !JS_GetReservedSlot(ccx, funobj, 1, &memberVal))
        return nsnull;

    if(!JS_SetReservedSlot(ccx, clone, 0, ifaceVal) ||
       !JS_SetReservedSlot(ccx, clone, 1, memberVal))
        return nsnull;

    return clone;
}

// XPCNativeMember

// static
JSBool
XPCNativeMember::GetCallInfo(XPCCallContext& ccx,
                             JSObject* funobj,
                             XPCNativeInterface** pInterface,
                             XPCNativeMember**    pMember)
{
    jsval ifaceVal;
    jsval memberVal;

    if(!JS_GetReservedSlot(ccx, funobj, 0, &ifaceVal) ||
       !JS_GetReservedSlot(ccx, funobj, 1, &memberVal) ||
       !JSVAL_IS_INT(ifaceVal) || !JSVAL_IS_INT(memberVal))
    {
        return JS_FALSE;
    }

    *pInterface = (XPCNativeInterface*) JSVAL_TO_PRIVATE(ifaceVal);
    *pMember = (XPCNativeMember*) JSVAL_TO_PRIVATE(memberVal);

    return JS_TRUE;
}

JSBool
XPCNativeMember::NewFunctionObject(XPCCallContext& ccx,
                                   XPCNativeInterface* iface, JSObject *parent,
                                   jsval* pval)
{
    NS_ASSERTION(!IsConstant(),
                 "Only call this if you're sure this is not a constant!");
    if(!IsResolved() && !Resolve(ccx, iface))
        return JS_FALSE;

    AUTO_MARK_JSVAL(ccx, &mVal);
    JSObject* funobj =
        xpc_CloneJSFunction(ccx, JSVAL_TO_OBJECT(mVal), parent);
    if(!funobj)
        return JS_FALSE;

    *pval = OBJECT_TO_JSVAL(funobj);

    return JS_TRUE;
}

JSBool
XPCNativeMember::Resolve(XPCCallContext& ccx, XPCNativeInterface* iface)
{
    if(IsConstant())
    {
        const nsXPTConstant* constant;
        if(NS_FAILED(iface->GetInterfaceInfo()->GetConstant(mIndex, &constant)))
            return JS_FALSE;

        const nsXPTCMiniVariant& mv = *constant->GetValue();

        // XXX Big Hack!
        nsXPTCVariant v;
        v.flags = 0;
        v.type = constant->GetType();
        memcpy(&v.val, &mv.val, sizeof(mv.val));

        jsval resultVal;

        if(!XPCConvert::NativeData2JS(ccx, &resultVal, &v.val, v.type,
                                      nsnull, nsnull, nsnull))
            return JS_FALSE;

        {   // scoped lock
            XPCAutoLock lock(ccx.GetRuntime()->GetMapLock());
            mVal = resultVal;
            mFlags |= RESOLVED;
        }

        return JS_TRUE;
    }
    // else...

    // This is a method or attribute - we'll be needing a function object

    intN argc;
    intN flags;
    JSNative callback;

    if(IsMethod())
    {
        const nsXPTMethodInfo* info;
        if(NS_FAILED(iface->GetInterfaceInfo()->GetMethodInfo(mIndex, &info)))
            return JS_FALSE;

        // Note: ASSUMES that retval is last arg.
        argc = (intN) info->GetParamCount();
        if(argc && info->GetParam((uint8)(argc-1)).IsRetval())
            argc-- ;

        flags = 0;
        callback = XPC_WN_CallMethod;
    }
    else
    {
        if(IsWritableAttribute())
            flags = JSFUN_GETTER | JSFUN_SETTER;
        else
            flags = JSFUN_GETTER;
        argc = 0;
        callback = XPC_WN_GetterSetter;
    }

    // We need to use the safe context for this thread because we don't want
    // to parent the new (and cached forever!) function object to the current
    // JSContext's global object. That would be bad!

    JSContext* cx = ccx.GetSafeJSContext();
    if(!cx)
        return JS_FALSE;

    const char *memberName = iface->GetMemberName(ccx, this);

    jsrefcount suspendDepth = 0;
    if(cx != ccx) {
        // Switching contexts, suspend the old and enter the new request.
        suspendDepth = JS_SuspendRequest(ccx);
        JS_BeginRequest(cx);
    }

    JSFunction *fun = JS_NewFunction(cx, callback, argc, flags, nsnull,
                                     memberName);

    if(suspendDepth) {
        JS_EndRequest(cx);
        JS_ResumeRequest(ccx, suspendDepth);
    }

    if(!fun)
        return JS_FALSE;

    JSObject* funobj = JS_GetFunctionObject(fun);
    if(!funobj)
        return JS_FALSE;

    AUTO_MARK_JSVAL(ccx, OBJECT_TO_JSVAL(funobj));

    STOBJ_CLEAR_PARENT(funobj);
    STOBJ_CLEAR_PROTO(funobj);

    if(!JS_SetReservedSlot(ccx, funobj, 0, PRIVATE_TO_JSVAL(iface))||
       !JS_SetReservedSlot(ccx, funobj, 1, PRIVATE_TO_JSVAL(this)))
        return JS_FALSE;

    {   // scoped lock
        XPCAutoLock lock(ccx.GetRuntime()->GetMapLock());
        mVal = OBJECT_TO_JSVAL(funobj);
        mFlags |= RESOLVED;
    }

    return JS_TRUE;
}

/***************************************************************************/
// XPCNativeInterface

// static
XPCNativeInterface*
XPCNativeInterface::GetNewOrUsed(XPCCallContext& ccx, const nsIID* iid)
{
    AutoMarkingNativeInterfacePtr iface(ccx);
    XPCJSRuntime* rt = ccx.GetRuntime();

    IID2NativeInterfaceMap* map = rt->GetIID2NativeInterfaceMap();
    if(!map)
        return nsnull;

    {   // scoped lock
        XPCAutoLock lock(rt->GetMapLock());
        iface = map->Find(*iid);
    }

    if(iface)
        return iface;

    nsCOMPtr<nsIInterfaceInfo> info;
    ccx.GetXPConnect()->GetInfoForIID(iid, getter_AddRefs(info));
    if(!info)
        return nsnull;

    iface = NewInstance(ccx, info);
    if(!iface)
        return nsnull;

    {   // scoped lock
        XPCAutoLock lock(rt->GetMapLock());
        XPCNativeInterface* iface2 = map->Add(iface);
        if(!iface2)
        {
            NS_ERROR("failed to add our interface!");
            DestroyInstance(iface);
            iface = nsnull;
        }
        else if(iface2 != iface)
        {
            DestroyInstance(iface);
            iface = iface2;
        }
    }

    return iface;
}

// static
XPCNativeInterface*
XPCNativeInterface::GetNewOrUsed(XPCCallContext& ccx, nsIInterfaceInfo* info)
{
    AutoMarkingNativeInterfacePtr iface(ccx);

    const nsIID* iid;
    if(NS_FAILED(info->GetIIDShared(&iid)) || !iid)
        return nsnull;

    XPCJSRuntime* rt = ccx.GetRuntime();

    IID2NativeInterfaceMap* map = rt->GetIID2NativeInterfaceMap();
    if(!map)
        return nsnull;

    {   // scoped lock
        XPCAutoLock lock(rt->GetMapLock());
        iface = map->Find(*iid);
    }

    if(iface)
        return iface;

    iface = NewInstance(ccx, info);
    if(!iface)
        return nsnull;

    {   // scoped lock
        XPCAutoLock lock(rt->GetMapLock());
        XPCNativeInterface* iface2 = map->Add(iface);
        if(!iface2)
        {
            NS_ERROR("failed to add our interface!");
            DestroyInstance(iface);
            iface = nsnull;
        }
        else if(iface2 != iface)
        {
            DestroyInstance(iface);
            iface = iface2;
        }
    }

    return iface;
}

// static
XPCNativeInterface*
XPCNativeInterface::GetNewOrUsed(XPCCallContext& ccx, const char* name)
{
    nsCOMPtr<nsIInterfaceInfo> info;
    ccx.GetXPConnect()->GetInfoForName(name, getter_AddRefs(info));
    return info ? GetNewOrUsed(ccx, info) : nsnull;
}

// static
XPCNativeInterface*
XPCNativeInterface::GetISupports(XPCCallContext& ccx)
{
    // XXX We should optimize this to cache this common XPCNativeInterface.
    return GetNewOrUsed(ccx, &NS_GET_IID(nsISupports));
}

// static
XPCNativeInterface*
XPCNativeInterface::NewInstance(XPCCallContext& ccx,
                                nsIInterfaceInfo* aInfo)
{
    static const PRUint16 MAX_LOCAL_MEMBER_COUNT = 16;
    XPCNativeMember local_members[MAX_LOCAL_MEMBER_COUNT];
    XPCNativeInterface* obj = nsnull;
    XPCNativeMember* members = nsnull;

    int i;
    JSBool failed = JS_FALSE;
    PRUint16 constCount;
    PRUint16 methodCount;
    PRUint16 totalCount;
    PRUint16 realTotalCount = 0;
    XPCNativeMember* cur;
    JSString*  str;
    jsval name;
    jsval interfaceName;

    // XXX Investigate lazy init? This is a problem given the
    // 'placement new' scheme - we need to at least know how big to make
    // the object. We might do a scan of methods to determine needed size,
    // then make our object, but avoid init'ing *any* members until asked?
    // Find out how often we create these objects w/o really looking at
    // (or using) the members.

    PRBool canScript;
    if(NS_FAILED(aInfo->IsScriptable(&canScript)) || !canScript)
        return nsnull;

    if(NS_FAILED(aInfo->GetMethodCount(&methodCount)) ||
       NS_FAILED(aInfo->GetConstantCount(&constCount)))
        return nsnull;

    // If the interface does not have nsISupports in its inheritance chain
    // then we know we can't reflect its methods. However, some interfaces that
    // are used just to reflect constants are declared this way. We need to
    // go ahead and build the thing. But, we'll ignore whatever methods it may
    // have.
    if(!nsXPConnect::IsISupportsDescendant(aInfo))
        methodCount = 0;

    totalCount = methodCount + constCount;

    if(totalCount > MAX_LOCAL_MEMBER_COUNT)
    {
        members = new XPCNativeMember[totalCount];
        if(!members)
            return nsnull;
    }
    else
    {
        members = local_members;
    }

    // NOTE: since getters and setters share a member, we might not use all
    // of the member objects.

    for(i = 0; i < methodCount; i++)
    {
        const nsXPTMethodInfo* info;
        if(NS_FAILED(aInfo->GetMethodInfo(i, &info)))
        {
            failed = JS_TRUE;
            break;
        }

        // don't reflect Addref or Release
        if(i == 1 || i == 2)
            continue;

        if(!XPCConvert::IsMethodReflectable(*info))
            continue;

        str = JS_InternString(ccx, info->GetName());
        if(!str)
        {
            NS_ASSERTION(0,"bad method name");
            failed = JS_TRUE;
            break;
        }
        name = STRING_TO_JSVAL(str);

        if(info->IsSetter())
        {
            NS_ASSERTION(realTotalCount,"bad setter");
            // Note: ASSUMES Getter/Setter pairs are next to each other
            // This is a rule of the typelib spec.
            cur = &members[realTotalCount-1];
            NS_ASSERTION(cur->GetName() == name,"bad setter");
            NS_ASSERTION(cur->IsReadOnlyAttribute(),"bad setter");
            NS_ASSERTION(cur->GetIndex() == i-1,"bad setter");
            cur->SetWritableAttribute();
        }
        else
        {
            // XXX need better way to find dups
            // NS_ASSERTION(!LookupMemberByID(name),"duplicate method name");
            cur = &members[realTotalCount++];
            cur->SetName(name);
            if(info->IsGetter())
                cur->SetReadOnlyAttribute(i);
            else
                cur->SetMethod(i);
        }
    }

    if(!failed)
    {
        for(i = 0; i < constCount; i++)
        {
            const nsXPTConstant* constant;
            if(NS_FAILED(aInfo->GetConstant(i, &constant)))
            {
                failed = JS_TRUE;
                break;
            }

            str = JS_InternString(ccx, constant->GetName());
            if(!str)
            {
                NS_ASSERTION(0,"bad constant name");
                failed = JS_TRUE;
                break;
            }
            name = STRING_TO_JSVAL(str);

            // XXX need better way to find dups
            //NS_ASSERTION(!LookupMemberByID(name),"duplicate method/constant name");

            cur = &members[realTotalCount++];
            cur->SetName(name);
            cur->SetConstant(i);
        }
    }

    if(!failed)
    {
        const char* bytes;
        if(NS_FAILED(aInfo->GetNameShared(&bytes)) || !bytes ||
           nsnull == (str = JS_InternString(ccx, bytes)))
        {
            failed = JS_TRUE;
        }
        interfaceName = STRING_TO_JSVAL(str);
    }

    if(!failed)
    {
        // Use placement new to create an object with the right amount of space
        // to hold the members array
        int size = sizeof(XPCNativeInterface);
        if(realTotalCount > 1)
            size += (realTotalCount - 1) * sizeof(XPCNativeMember);
        void* place = new char[size];
        if(place)
            obj = new(place) XPCNativeInterface(aInfo, interfaceName);

        if(obj)
        {
            obj->mMemberCount = realTotalCount;
            // copy valid members
            if(realTotalCount)
                memcpy(obj->mMembers, members,
                       realTotalCount * sizeof(XPCNativeMember));
        }
    }

    if(members && members != local_members)
        delete [] members;

    return obj;
}

// static
void
XPCNativeInterface::DestroyInstance(XPCNativeInterface* inst)
{
    inst->~XPCNativeInterface();
    delete [] (char*) inst;
}

const char*
XPCNativeInterface::GetMemberName(XPCCallContext& ccx,
                                  const XPCNativeMember* member) const
{
    return JS_GetStringBytes(JSVAL_TO_STRING(member->GetName()));
}

void
XPCNativeInterface::DebugDump(PRInt16 depth)
{
#ifdef DEBUG
    depth--;
    XPC_LOG_ALWAYS(("XPCNativeInterface @ %x", this));
        XPC_LOG_INDENT();
        XPC_LOG_ALWAYS(("name is %s", GetNameString()));
        XPC_LOG_ALWAYS(("mMemberCount is %d", mMemberCount));
        XPC_LOG_ALWAYS(("mInfo @ %x", mInfo.get()));
        XPC_LOG_OUTDENT();
#endif
}

/***************************************************************************/
// XPCNativeSet

// static
XPCNativeSet*
XPCNativeSet::GetNewOrUsed(XPCCallContext& ccx, const nsIID* iid)
{
    AutoMarkingNativeSetPtr set(ccx);

    AutoMarkingNativeInterfacePtr iface(ccx);
    iface = XPCNativeInterface::GetNewOrUsed(ccx, iid);
    if(!iface)
        return nsnull;

    XPCNativeSetKey key(nsnull, iface, 0);

    XPCJSRuntime* rt = ccx.GetRuntime();
    NativeSetMap* map = rt->GetNativeSetMap();
    if(!map)
        return nsnull;

    {   // scoped lock
        XPCAutoLock lock(rt->GetMapLock());
        set = map->Find(&key);
    }

    if(set)
        return set;

    // hacky way to get a XPCNativeInterface** using the AutoPtr
    XPCNativeInterface* temp[] = {iface}; 
    set = NewInstance(ccx, temp, 1);
    if(!set)
        return nsnull;

    {   // scoped lock
        XPCAutoLock lock(rt->GetMapLock());
        XPCNativeSet* set2 = map->Add(&key, set);
        if(!set2)
        {
            NS_ERROR("failed to add our set!");
            DestroyInstance(set);
            set = nsnull;
        }
        else if(set2 != set)
        {
            DestroyInstance(set);
            set = set2;
        }
    }

    return set;
}

// static
XPCNativeSet*
XPCNativeSet::GetNewOrUsed(XPCCallContext& ccx, nsIClassInfo* classInfo)
{
    AutoMarkingNativeSetPtr set(ccx);
    XPCJSRuntime* rt = ccx.GetRuntime();

    ClassInfo2NativeSetMap* map = rt->GetClassInfo2NativeSetMap();
    if(!map)
        return nsnull;

    {   // scoped lock
        XPCAutoLock lock(rt->GetMapLock());
        set = map->Find(classInfo);
    }

    if(set)
        return set;

    nsIID** iidArray = nsnull;
    AutoMarkingNativeInterfacePtrArrayPtr interfaceArray(ccx);
    PRUint32 iidCount = 0;

    if(NS_FAILED(classInfo->GetInterfaces(&iidCount, &iidArray)))
    {
        // Note: I'm making it OK for this call to fail so that one can add
        // nsIClassInfo to classes implemented in script without requiring this
        // method to be implemented.

        // Make sure these are set correctly...
        iidArray = nsnull;
        iidCount = 0;
    }

    NS_ASSERTION((iidCount && iidArray) || !(iidCount || iidArray), "GetInterfaces returned bad array");

    // !!! from here on we only exit through the 'out' label !!!

    if(iidCount)
    {
        AutoMarkingNativeInterfacePtrArrayPtr
            arr(ccx, new XPCNativeInterface*[iidCount], iidCount, PR_TRUE);
        if (!arr)
            goto out;

        interfaceArray = arr;

        XPCNativeInterface** currentInterface = interfaceArray;
        nsIID**              currentIID = iidArray;
        PRUint16             interfaceCount = 0;

        for(PRUint32 i = 0; i < iidCount; i++)
        {
            nsIID* iid = *(currentIID++);
            if (!iid) {
                NS_ERROR("Null found in classinfo interface list");
                continue;
            }

            XPCNativeInterface* iface =
                XPCNativeInterface::GetNewOrUsed(ccx, iid);

            if(!iface)
            {
                // XXX warn here
                continue;
            }

            *(currentInterface++) = iface;
            interfaceCount++;
        }

        if(interfaceCount)
        {
            set = NewInstance(ccx, interfaceArray, interfaceCount);
            if(set)
            {
                NativeSetMap* map2 = rt->GetNativeSetMap();
                if(!map2)
                    goto out;

                XPCNativeSetKey key(set, nsnull, 0);

                {   // scoped lock
                    XPCAutoLock lock(rt->GetMapLock());
                    XPCNativeSet* set2 = map2->Add(&key, set);
                    if(!set2)
                    {
                        NS_ERROR("failed to add our set!");
                        DestroyInstance(set);
                        set = nsnull;
                        goto out;
                    }
                    if(set2 != set)
                    {
                        DestroyInstance(set);
                        set = set2;
                    }
                }
            }
        }
        else
            set = GetNewOrUsed(ccx, &NS_GET_IID(nsISupports));
    }
    else
        set = GetNewOrUsed(ccx, &NS_GET_IID(nsISupports));

    if(set)
    {   // scoped lock
        XPCAutoLock lock(rt->GetMapLock());
        XPCNativeSet* set2 = map->Add(classInfo, set);
        NS_ASSERTION(set2, "failed to add our set!");
        NS_ASSERTION(set2 == set, "hashtables inconsistent!");
    }

out:
    if(iidArray)
        NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(iidCount, iidArray);
    if(interfaceArray)
        delete [] interfaceArray.get();

    return set;
}

// static 
void 
XPCNativeSet::ClearCacheEntryForClassInfo(nsIClassInfo* classInfo)
{
    XPCJSRuntime* rt = nsXPConnect::GetRuntimeInstance();
    ClassInfo2NativeSetMap* map = rt->GetClassInfo2NativeSetMap();
    if(map)
    {   // scoped lock
        XPCAutoLock lock(rt->GetMapLock());
        map->Remove(classInfo);
    }
}

// static
XPCNativeSet*
XPCNativeSet::GetNewOrUsed(XPCCallContext& ccx,
                           XPCNativeSet* otherSet,
                           XPCNativeInterface* newInterface,
                           PRUint16 position)
{
    AutoMarkingNativeSetPtr set(ccx);
    XPCJSRuntime* rt = ccx.GetRuntime();
    NativeSetMap* map = rt->GetNativeSetMap();
    if(!map)
        return nsnull;

    XPCNativeSetKey key(otherSet, newInterface, position);

    {   // scoped lock
        XPCAutoLock lock(rt->GetMapLock());
        set = map->Find(&key);
    }

    if(set)
        return set;

    if(otherSet)
        set = NewInstanceMutate(otherSet, newInterface, position);
    else
        set = NewInstance(ccx, &newInterface, 1);

    if(!set)
        return nsnull;

    {   // scoped lock
        XPCAutoLock lock(rt->GetMapLock());
        XPCNativeSet* set2 = map->Add(&key, set);
        if(!set2)
        {
            NS_ERROR("failed to add our set!");
            DestroyInstance(set);
            set = nsnull;
        }
        else if(set2 != set)
        {
            DestroyInstance(set);
            set = set2;
        }
    }

    return set;
}

// static
XPCNativeSet*
XPCNativeSet::NewInstance(XPCCallContext& ccx,
                          XPCNativeInterface** array,
                          PRUint16 count)
{
    XPCNativeSet* obj = nsnull;

    if(!array || !count)
        return nsnull;

    // We impose the invariant:
    // "All sets have exactly one nsISupports interface and it comes first."
    // This is the place where we impose that rule - even if given inputs
    // that don't exactly follow the rule.

    XPCNativeInterface* isup = XPCNativeInterface::GetISupports(ccx);
    PRUint16 slots = count+1;

    PRUint16 i;
    XPCNativeInterface** pcur;

    for(i = 0, pcur = array; i < count; i++, pcur++)
    {
        if(*pcur == isup)
            slots--;
    }

    // Use placement new to create an object with the right amount of space
    // to hold the members array
    int size = sizeof(XPCNativeSet);
    if(slots > 1)
        size += (slots - 1) * sizeof(XPCNativeInterface*);
    void* place = new char[size];
    if(place)
        obj = new(place) XPCNativeSet();

    if(obj)
    {
        // Stick the nsISupports in front and skip additional nsISupport(s)
        XPCNativeInterface** inp = array;
        XPCNativeInterface** outp = (XPCNativeInterface**) &obj->mInterfaces;
        PRUint16 memberCount = 1;   // for the one member in nsISupports

        *(outp++) = isup;

        for(i = 0; i < count; i++)
        {
            XPCNativeInterface* cur;

            if(isup == (cur = *(inp++)))
                continue;
            *(outp++) = cur;
            memberCount += cur->GetMemberCount();
        }
        obj->mMemberCount = memberCount;
        obj->mInterfaceCount = slots;
    }

    return obj;
}

// static
XPCNativeSet*
XPCNativeSet::NewInstanceMutate(XPCNativeSet*       otherSet,
                                XPCNativeInterface* newInterface,
                                PRUint16            position)
{
    XPCNativeSet* obj = nsnull;

    if(!newInterface)
        return nsnull;
    if(otherSet && position > otherSet->mInterfaceCount)
        return nsnull;

    // Use placement new to create an object with the right amount of space
    // to hold the members array
    int size = sizeof(XPCNativeSet);
    if(otherSet)
        size += otherSet->mInterfaceCount * sizeof(XPCNativeInterface*);
    void* place = new char[size];
    if(place)
        obj = new(place) XPCNativeSet();

    if(obj)
    {
        if(otherSet)
        {
            obj->mMemberCount = otherSet->GetMemberCount() +
                                newInterface->GetMemberCount();
            obj->mInterfaceCount = otherSet->mInterfaceCount + 1;

            XPCNativeInterface** src = otherSet->mInterfaces;
            XPCNativeInterface** dest = obj->mInterfaces;
            for(PRUint16 i = 0; i < obj->mInterfaceCount; i++)
            {
                if(i == position)
                    *dest++ = newInterface;
                else
                    *dest++ = *src++;
            }
        }
        else
        {
            obj->mMemberCount = newInterface->GetMemberCount();
            obj->mInterfaceCount = 1;
            obj->mInterfaces[0] = newInterface;
        }
    }

    return obj;
}

// static
void
XPCNativeSet::DestroyInstance(XPCNativeSet* inst)
{
    inst->~XPCNativeSet();
    delete [] (char*) inst;
}

void
XPCNativeSet::DebugDump(PRInt16 depth)
{
#ifdef DEBUG
    depth--;
    XPC_LOG_ALWAYS(("XPCNativeSet @ %x", this));
        XPC_LOG_INDENT();

        XPC_LOG_ALWAYS(("mInterfaceCount of %d", mInterfaceCount));
        if(depth)
        {
            for(PRUint16 i = 0; i < mInterfaceCount; i++)
                mInterfaces[i]->DebugDump(depth);
        }
        XPC_LOG_ALWAYS(("mMemberCount of %d", mMemberCount));
        XPC_LOG_OUTDENT();
#endif
}
