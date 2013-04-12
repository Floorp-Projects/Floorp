/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
// vim:cindent:ts=8:et:sw=4:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Manage the shared info about interfaces for use by wrappedNatives. */

#include "xpcprivate.h"

/***************************************************************************/

// XPCNativeMember

// static
JSBool
XPCNativeMember::GetCallInfo(JSObject* funobj,
                             XPCNativeInterface** pInterface,
                             XPCNativeMember**    pMember)
{
    funobj = js::UncheckedUnwrap(funobj);
    jsval ifaceVal = js::GetFunctionNativeReserved(funobj, 0);
    jsval memberVal = js::GetFunctionNativeReserved(funobj, 1);

    *pInterface = (XPCNativeInterface*) JSVAL_TO_PRIVATE(ifaceVal);
    *pMember = (XPCNativeMember*) JSVAL_TO_PRIVATE(memberVal);

    return true;
}

JSBool
XPCNativeMember::NewFunctionObject(XPCCallContext& ccx,
                                   XPCNativeInterface* iface, JSObject *parent,
                                   jsval* pval)
{
    NS_ASSERTION(!IsConstant(),
                 "Only call this if you're sure this is not a constant!");

    return Resolve(ccx, iface, parent, pval);
}

JSBool
XPCNativeMember::Resolve(XPCCallContext& ccx, XPCNativeInterface* iface,
                         JSObject *parent, jsval *vp)
{
    if (IsConstant()) {
        const nsXPTConstant* constant;
        if (NS_FAILED(iface->GetInterfaceInfo()->GetConstant(mIndex, &constant)))
            return false;

        const nsXPTCMiniVariant& mv = *constant->GetValue();

        // XXX Big Hack!
        nsXPTCVariant v;
        v.flags = 0;
        v.type = constant->GetType();
        memcpy(&v.val, &mv.val, sizeof(mv.val));

        jsval resultVal;

        if (!XPCConvert::NativeData2JS(ccx, &resultVal, &v.val, v.type,
                                       nullptr, nullptr))
            return false;

        *vp = resultVal;

        return true;
    }
    // else...

    // This is a method or attribute - we'll be needing a function object

    int argc;
    JSNative callback;

    if (IsMethod()) {
        const nsXPTMethodInfo* info;
        if (NS_FAILED(iface->GetInterfaceInfo()->GetMethodInfo(mIndex, &info)))
            return false;

        // Note: ASSUMES that retval is last arg.
        argc = (int) info->GetParamCount();
        if (argc && info->GetParam((uint8_t)(argc-1)).IsRetval())
            argc-- ;

        callback = XPC_WN_CallMethod;
    } else {
        argc = 0;
        callback = XPC_WN_GetterSetter;
    }

    JSFunction *fun = js::NewFunctionByIdWithReserved(ccx, callback, argc, 0, parent, GetName());
    if (!fun)
        return false;

    JSObject* funobj = JS_GetFunctionObject(fun);
    if (!funobj)
        return false;

    js::SetFunctionNativeReserved(funobj, 0, PRIVATE_TO_JSVAL(iface));
    js::SetFunctionNativeReserved(funobj, 1, PRIVATE_TO_JSVAL(this));

    *vp = OBJECT_TO_JSVAL(funobj);

    return true;
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
    if (!map)
        return nullptr;

    {   // scoped lock
        XPCAutoLock lock(rt->GetMapLock());
        iface = map->Find(*iid);
    }

    if (iface)
        return iface;

    nsCOMPtr<nsIInterfaceInfo> info;
    ccx.GetXPConnect()->GetInfoForIID(iid, getter_AddRefs(info));
    if (!info)
        return nullptr;

    iface = NewInstance(ccx, info);
    if (!iface)
        return nullptr;

    {   // scoped lock
        XPCAutoLock lock(rt->GetMapLock());
        XPCNativeInterface* iface2 = map->Add(iface);
        if (!iface2) {
            NS_ERROR("failed to add our interface!");
            DestroyInstance(iface);
            iface = nullptr;
        } else if (iface2 != iface) {
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
    if (NS_FAILED(info->GetIIDShared(&iid)) || !iid)
        return nullptr;

    XPCJSRuntime* rt = ccx.GetRuntime();

    IID2NativeInterfaceMap* map = rt->GetIID2NativeInterfaceMap();
    if (!map)
        return nullptr;

    {   // scoped lock
        XPCAutoLock lock(rt->GetMapLock());
        iface = map->Find(*iid);
    }

    if (iface)
        return iface;

    iface = NewInstance(ccx, info);
    if (!iface)
        return nullptr;

    {   // scoped lock
        XPCAutoLock lock(rt->GetMapLock());
        XPCNativeInterface* iface2 = map->Add(iface);
        if (!iface2) {
            NS_ERROR("failed to add our interface!");
            DestroyInstance(iface);
            iface = nullptr;
        } else if (iface2 != iface) {
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
    return info ? GetNewOrUsed(ccx, info) : nullptr;
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
    static const uint16_t MAX_LOCAL_MEMBER_COUNT = 16;
    XPCNativeMember local_members[MAX_LOCAL_MEMBER_COUNT];
    XPCNativeInterface* obj = nullptr;
    XPCNativeMember* members = nullptr;

    int i;
    JSBool failed = false;
    uint16_t constCount;
    uint16_t methodCount;
    uint16_t totalCount;
    uint16_t realTotalCount = 0;
    XPCNativeMember* cur;
    JSString* str = NULL;
    jsid name;
    jsid interfaceName;

    // XXX Investigate lazy init? This is a problem given the
    // 'placement new' scheme - we need to at least know how big to make
    // the object. We might do a scan of methods to determine needed size,
    // then make our object, but avoid init'ing *any* members until asked?
    // Find out how often we create these objects w/o really looking at
    // (or using) the members.

    bool canScript;
    if (NS_FAILED(aInfo->IsScriptable(&canScript)) || !canScript)
        return nullptr;

    if (NS_FAILED(aInfo->GetMethodCount(&methodCount)) ||
        NS_FAILED(aInfo->GetConstantCount(&constCount)))
        return nullptr;

    // If the interface does not have nsISupports in its inheritance chain
    // then we know we can't reflect its methods. However, some interfaces that
    // are used just to reflect constants are declared this way. We need to
    // go ahead and build the thing. But, we'll ignore whatever methods it may
    // have.
    if (!nsXPConnect::IsISupportsDescendant(aInfo))
        methodCount = 0;

    totalCount = methodCount + constCount;

    if (totalCount > MAX_LOCAL_MEMBER_COUNT) {
        members = new XPCNativeMember[totalCount];
        if (!members)
            return nullptr;
    } else {
        members = local_members;
    }

    // NOTE: since getters and setters share a member, we might not use all
    // of the member objects.

    for (i = 0; i < methodCount; i++) {
        const nsXPTMethodInfo* info;
        if (NS_FAILED(aInfo->GetMethodInfo(i, &info))) {
            failed = true;
            break;
        }

        // don't reflect Addref or Release
        if (i == 1 || i == 2)
            continue;

        if (!XPCConvert::IsMethodReflectable(*info))
            continue;

        str = JS_InternString(ccx, info->GetName());
        if (!str) {
            NS_ERROR("bad method name");
            failed = true;
            break;
        }
        name = INTERNED_STRING_TO_JSID(ccx, str);

        if (info->IsSetter()) {
            NS_ASSERTION(realTotalCount,"bad setter");
            // Note: ASSUMES Getter/Setter pairs are next to each other
            // This is a rule of the typelib spec.
            cur = &members[realTotalCount-1];
            NS_ASSERTION(cur->GetName() == name,"bad setter");
            NS_ASSERTION(cur->IsReadOnlyAttribute(),"bad setter");
            NS_ASSERTION(cur->GetIndex() == i-1,"bad setter");
            cur->SetWritableAttribute();
        } else {
            // XXX need better way to find dups
            // NS_ASSERTION(!LookupMemberByID(name),"duplicate method name");
            cur = &members[realTotalCount++];
            cur->SetName(name);
            if (info->IsGetter())
                cur->SetReadOnlyAttribute(i);
            else
                cur->SetMethod(i);
        }
    }

    if (!failed) {
        for (i = 0; i < constCount; i++) {
            const nsXPTConstant* constant;
            if (NS_FAILED(aInfo->GetConstant(i, &constant))) {
                failed = true;
                break;
            }

            str = JS_InternString(ccx, constant->GetName());
            if (!str) {
                NS_ERROR("bad constant name");
                failed = true;
                break;
            }
            name = INTERNED_STRING_TO_JSID(ccx, str);

            // XXX need better way to find dups
            //NS_ASSERTION(!LookupMemberByID(name),"duplicate method/constant name");

            cur = &members[realTotalCount++];
            cur->SetName(name);
            cur->SetConstant(i);
        }
    }

    if (!failed) {
        const char* bytes;
        if (NS_FAILED(aInfo->GetNameShared(&bytes)) || !bytes ||
            nullptr == (str = JS_InternString(ccx, bytes))) {
            failed = true;
        }
        interfaceName = INTERNED_STRING_TO_JSID(ccx, str);
    }

    if (!failed) {
        // Use placement new to create an object with the right amount of space
        // to hold the members array
        int size = sizeof(XPCNativeInterface);
        if (realTotalCount > 1)
            size += (realTotalCount - 1) * sizeof(XPCNativeMember);
        void* place = new char[size];
        if (place)
            obj = new(place) XPCNativeInterface(aInfo, interfaceName);

        if (obj) {
            obj->mMemberCount = realTotalCount;
            // copy valid members
            if (realTotalCount)
                memcpy(obj->mMembers, members,
                       realTotalCount * sizeof(XPCNativeMember));
        }
    }

    if (members && members != local_members)
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

size_t
XPCNativeInterface::SizeOfIncludingThis(nsMallocSizeOfFun mallocSizeOf)
{
    return mallocSizeOf(this);
}

void
XPCNativeInterface::DebugDump(int16_t depth)
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
    if (!iface)
        return nullptr;

    XPCNativeSetKey key(nullptr, iface, 0);

    XPCJSRuntime* rt = ccx.GetRuntime();
    NativeSetMap* map = rt->GetNativeSetMap();
    if (!map)
        return nullptr;

    {   // scoped lock
        XPCAutoLock lock(rt->GetMapLock());
        set = map->Find(&key);
    }

    if (set)
        return set;

    // hacky way to get a XPCNativeInterface** using the AutoPtr
    XPCNativeInterface* temp[] = {iface};
    set = NewInstance(ccx, temp, 1);
    if (!set)
        return nullptr;

    {   // scoped lock
        XPCAutoLock lock(rt->GetMapLock());
        XPCNativeSet* set2 = map->Add(&key, set);
        if (!set2) {
            NS_ERROR("failed to add our set!");
            DestroyInstance(set);
            set = nullptr;
        } else if (set2 != set) {
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
    if (!map)
        return nullptr;

    {   // scoped lock
        XPCAutoLock lock(rt->GetMapLock());
        set = map->Find(classInfo);
    }

    if (set)
        return set;

    nsIID** iidArray = nullptr;
    AutoMarkingNativeInterfacePtrArrayPtr interfaceArray(ccx);
    uint32_t iidCount = 0;

    if (NS_FAILED(classInfo->GetInterfaces(&iidCount, &iidArray))) {
        // Note: I'm making it OK for this call to fail so that one can add
        // nsIClassInfo to classes implemented in script without requiring this
        // method to be implemented.

        // Make sure these are set correctly...
        iidArray = nullptr;
        iidCount = 0;
    }

    NS_ASSERTION((iidCount && iidArray) || !(iidCount || iidArray), "GetInterfaces returned bad array");

    // !!! from here on we only exit through the 'out' label !!!

    if (iidCount) {
        AutoMarkingNativeInterfacePtrArrayPtr
            arr(ccx, new XPCNativeInterface*[iidCount], iidCount, true);
        if (!arr)
            goto out;

        interfaceArray = arr;

        XPCNativeInterface** currentInterface = interfaceArray;
        nsIID**              currentIID = iidArray;
        uint16_t             interfaceCount = 0;

        for (uint32_t i = 0; i < iidCount; i++) {
            nsIID* iid = *(currentIID++);
            if (!iid) {
                NS_ERROR("Null found in classinfo interface list");
                continue;
            }

            XPCNativeInterface* iface =
                XPCNativeInterface::GetNewOrUsed(ccx, iid);

            if (!iface) {
                // XXX warn here
                continue;
            }

            *(currentInterface++) = iface;
            interfaceCount++;
        }

        if (interfaceCount) {
            set = NewInstance(ccx, interfaceArray, interfaceCount);
            if (set) {
                NativeSetMap* map2 = rt->GetNativeSetMap();
                if (!map2)
                    goto out;

                XPCNativeSetKey key(set, nullptr, 0);

                {   // scoped lock
                    XPCAutoLock lock(rt->GetMapLock());
                    XPCNativeSet* set2 = map2->Add(&key, set);
                    if (!set2) {
                        NS_ERROR("failed to add our set!");
                        DestroyInstance(set);
                        set = nullptr;
                        goto out;
                    }
                    if (set2 != set) {
                        DestroyInstance(set);
                        set = set2;
                    }
                }
            }
        } else
            set = GetNewOrUsed(ccx, &NS_GET_IID(nsISupports));
    } else
        set = GetNewOrUsed(ccx, &NS_GET_IID(nsISupports));

    if (set)
    {   // scoped lock
        XPCAutoLock lock(rt->GetMapLock());

#ifdef DEBUG
        XPCNativeSet* set2 =
#endif
          map->Add(classInfo, set);
        NS_ASSERTION(set2, "failed to add our set!");
        NS_ASSERTION(set2 == set, "hashtables inconsistent!");
    }

out:
    if (iidArray)
        NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(iidCount, iidArray);
    if (interfaceArray)
        delete [] interfaceArray.get();

    return set;
}

// static
void
XPCNativeSet::ClearCacheEntryForClassInfo(nsIClassInfo* classInfo)
{
    XPCJSRuntime* rt = nsXPConnect::GetRuntimeInstance();
    ClassInfo2NativeSetMap* map = rt->GetClassInfo2NativeSetMap();
    if (map)
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
                           uint16_t position)
{
    AutoMarkingNativeSetPtr set(ccx);
    XPCJSRuntime* rt = ccx.GetRuntime();
    NativeSetMap* map = rt->GetNativeSetMap();
    if (!map)
        return nullptr;

    XPCNativeSetKey key(otherSet, newInterface, position);

    {   // scoped lock
        XPCAutoLock lock(rt->GetMapLock());
        set = map->Find(&key);
    }

    if (set)
        return set;

    if (otherSet)
        set = NewInstanceMutate(otherSet, newInterface, position);
    else
        set = NewInstance(ccx, &newInterface, 1);

    if (!set)
        return nullptr;

    {   // scoped lock
        XPCAutoLock lock(rt->GetMapLock());
        XPCNativeSet* set2 = map->Add(&key, set);
        if (!set2) {
            NS_ERROR("failed to add our set!");
            DestroyInstance(set);
            set = nullptr;
        } else if (set2 != set) {
            DestroyInstance(set);
            set = set2;
        }
    }

    return set;
}

// static
XPCNativeSet*
XPCNativeSet::GetNewOrUsed(XPCCallContext& ccx,
                           XPCNativeSet* firstSet,
                           XPCNativeSet* secondSet,
                           bool preserveFirstSetOrder)
{
    // Figure out how many interfaces we'll need in the new set.
    uint32_t uniqueCount = firstSet->mInterfaceCount;
    for (uint32_t i = 0; i < secondSet->mInterfaceCount; ++i) {
        if (!firstSet->HasInterface(secondSet->mInterfaces[i]))
            uniqueCount++;
    }

    // If everything in secondSet was a duplicate, we can just use the first
    // set.
    if (uniqueCount == firstSet->mInterfaceCount)
        return firstSet;

    // If the secondSet is just a superset of the first, we can use it provided
    // that the caller doesn't care about ordering.
    if (!preserveFirstSetOrder && uniqueCount == secondSet->mInterfaceCount)
        return secondSet;

    // Ok, darn. Now we have to make a new set.
    //
    // It would be faster to just create the new set all at once, but that
    // would involve wrangling with some pretty hairy code - especially since
    // a lot of stuff assumes that sets are created by adding one interface to an
    // existing set. So let's just do the slow and easy thing and hope that the
    // above optimizations handle the common cases.
    XPCNativeSet* currentSet = firstSet;
    for (uint32_t i = 0; i < secondSet->mInterfaceCount; ++i) {
        XPCNativeInterface* iface = secondSet->mInterfaces[i];
        if (!currentSet->HasInterface(iface)) {
            // Create a new augmented set, inserting this interface at the end.
            uint32_t pos = currentSet->mInterfaceCount;
            currentSet = XPCNativeSet::GetNewOrUsed(ccx, currentSet, iface, pos);
            if (!currentSet)
                return nullptr;
        }
    }

    // We've got the union set. Hand it back to the caller.
    MOZ_ASSERT(currentSet->mInterfaceCount == uniqueCount);
    return currentSet;
}

// static
XPCNativeSet*
XPCNativeSet::NewInstance(XPCCallContext& ccx,
                          XPCNativeInterface** array,
                          uint16_t count)
{
    XPCNativeSet* obj = nullptr;

    if (!array || !count)
        return nullptr;

    // We impose the invariant:
    // "All sets have exactly one nsISupports interface and it comes first."
    // This is the place where we impose that rule - even if given inputs
    // that don't exactly follow the rule.

    XPCNativeInterface* isup = XPCNativeInterface::GetISupports(ccx);
    uint16_t slots = count+1;

    uint16_t i;
    XPCNativeInterface** pcur;

    for (i = 0, pcur = array; i < count; i++, pcur++) {
        if (*pcur == isup)
            slots--;
    }

    // Use placement new to create an object with the right amount of space
    // to hold the members array
    int size = sizeof(XPCNativeSet);
    if (slots > 1)
        size += (slots - 1) * sizeof(XPCNativeInterface*);
    void* place = new char[size];
    if (place)
        obj = new(place) XPCNativeSet();

    if (obj) {
        // Stick the nsISupports in front and skip additional nsISupport(s)
        XPCNativeInterface** inp = array;
        XPCNativeInterface** outp = (XPCNativeInterface**) &obj->mInterfaces;
        uint16_t memberCount = 1;   // for the one member in nsISupports

        *(outp++) = isup;

        for (i = 0; i < count; i++) {
            XPCNativeInterface* cur;

            if (isup == (cur = *(inp++)))
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
                                uint16_t            position)
{
    XPCNativeSet* obj = nullptr;

    if (!newInterface)
        return nullptr;
    if (otherSet && position > otherSet->mInterfaceCount)
        return nullptr;

    // Use placement new to create an object with the right amount of space
    // to hold the members array
    int size = sizeof(XPCNativeSet);
    if (otherSet)
        size += otherSet->mInterfaceCount * sizeof(XPCNativeInterface*);
    void* place = new char[size];
    if (place)
        obj = new(place) XPCNativeSet();

    if (obj) {
        if (otherSet) {
            obj->mMemberCount = otherSet->GetMemberCount() +
                                newInterface->GetMemberCount();
            obj->mInterfaceCount = otherSet->mInterfaceCount + 1;

            XPCNativeInterface** src = otherSet->mInterfaces;
            XPCNativeInterface** dest = obj->mInterfaces;
            for (uint16_t i = 0; i < obj->mInterfaceCount; i++) {
                if (i == position)
                    *dest++ = newInterface;
                else
                    *dest++ = *src++;
            }
        } else {
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

size_t
XPCNativeSet::SizeOfIncludingThis(nsMallocSizeOfFun mallocSizeOf)
{
    return mallocSizeOf(this);
}

void
XPCNativeSet::DebugDump(int16_t depth)
{
#ifdef DEBUG
    depth--;
    XPC_LOG_ALWAYS(("XPCNativeSet @ %x", this));
        XPC_LOG_INDENT();

        XPC_LOG_ALWAYS(("mInterfaceCount of %d", mInterfaceCount));
        if (depth) {
            for (uint16_t i = 0; i < mInterfaceCount; i++)
                mInterfaces[i]->DebugDump(depth);
        }
        XPC_LOG_ALWAYS(("mMemberCount of %d", mMemberCount));
        XPC_LOG_OUTDENT();
#endif
}
