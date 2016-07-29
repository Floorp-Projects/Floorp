/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Manage the shared info about interfaces for use by wrappedNatives. */

#include "xpcprivate.h"
#include "jswrapper.h"

#include "mozilla/MemoryReporting.h"
#include "mozilla/XPTInterfaceInfoManager.h"
#include "nsPrintfCString.h"

using namespace JS;
using namespace mozilla;

/***************************************************************************/

// XPCNativeMember

// static
bool
XPCNativeMember::GetCallInfo(JSObject* funobj,
                             XPCNativeInterface** pInterface,
                             XPCNativeMember**    pMember)
{
    funobj = js::UncheckedUnwrap(funobj);
    Value memberVal =
        js::GetFunctionNativeReserved(funobj,
                                      XPC_FUNCTION_NATIVE_MEMBER_SLOT);

    *pMember = static_cast<XPCNativeMember*>(memberVal.toPrivate());
    *pInterface = (*pMember)->GetInterface();

    return true;
}

bool
XPCNativeMember::NewFunctionObject(XPCCallContext& ccx,
                                   XPCNativeInterface* iface, HandleObject parent,
                                   Value* pval)
{
    MOZ_ASSERT(!IsConstant(), "Only call this if you're sure this is not a constant!");

    return Resolve(ccx, iface, parent, pval);
}

bool
XPCNativeMember::Resolve(XPCCallContext& ccx, XPCNativeInterface* iface,
                         HandleObject parent, Value* vp)
{
    MOZ_ASSERT(iface == GetInterface());
    if (IsConstant()) {
        RootedValue resultVal(ccx);
        nsXPIDLCString name;
        if (NS_FAILED(iface->GetInterfaceInfo()->GetConstant(mIndex, &resultVal,
                                                             getter_Copies(name))))
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

    JSFunction* fun = js::NewFunctionByIdWithReserved(ccx, callback, argc, 0, GetName());
    if (!fun)
        return false;

    JSObject* funobj = JS_GetFunctionObject(fun);
    if (!funobj)
        return false;

    js::SetFunctionNativeReserved(funobj, XPC_FUNCTION_NATIVE_MEMBER_SLOT,
                                  PrivateValue(this));
    js::SetFunctionNativeReserved(funobj, XPC_FUNCTION_PARENT_OBJECT_SLOT,
                                  ObjectValue(*parent));

    vp->setObject(*funobj);

    return true;
}

/***************************************************************************/
// XPCNativeInterface

// static
XPCNativeInterface*
XPCNativeInterface::GetNewOrUsed(const nsIID* iid)
{
    AutoJSContext cx;
    AutoMarkingNativeInterfacePtr iface(cx);
    XPCJSRuntime* rt = XPCJSRuntime::Get();

    IID2NativeInterfaceMap* map = rt->GetIID2NativeInterfaceMap();
    if (!map)
        return nullptr;

    iface = map->Find(*iid);

    if (iface)
        return iface;

    nsCOMPtr<nsIInterfaceInfo> info;
    XPTInterfaceInfoManager::GetSingleton()->GetInfoForIID(iid, getter_AddRefs(info));
    if (!info)
        return nullptr;

    iface = NewInstance(info);
    if (!iface)
        return nullptr;

    XPCNativeInterface* iface2 = map->Add(iface);
    if (!iface2) {
        NS_ERROR("failed to add our interface!");
        DestroyInstance(iface);
        iface = nullptr;
    } else if (iface2 != iface) {
        DestroyInstance(iface);
        iface = iface2;
    }

    return iface;
}

// static
XPCNativeInterface*
XPCNativeInterface::GetNewOrUsed(nsIInterfaceInfo* info)
{
    AutoJSContext cx;
    AutoMarkingNativeInterfacePtr iface(cx);

    const nsIID* iid;
    if (NS_FAILED(info->GetIIDShared(&iid)) || !iid)
        return nullptr;

    XPCJSRuntime* rt = XPCJSRuntime::Get();

    IID2NativeInterfaceMap* map = rt->GetIID2NativeInterfaceMap();
    if (!map)
        return nullptr;

    iface = map->Find(*iid);

    if (iface)
        return iface;

    iface = NewInstance(info);
    if (!iface)
        return nullptr;

    XPCNativeInterface* iface2 = map->Add(iface);
    if (!iface2) {
        NS_ERROR("failed to add our interface!");
        DestroyInstance(iface);
        iface = nullptr;
    } else if (iface2 != iface) {
        DestroyInstance(iface);
        iface = iface2;
    }

    return iface;
}

// static
XPCNativeInterface*
XPCNativeInterface::GetNewOrUsed(const char* name)
{
    nsCOMPtr<nsIInterfaceInfo> info;
    XPTInterfaceInfoManager::GetSingleton()->GetInfoForName(name, getter_AddRefs(info));
    return info ? GetNewOrUsed(info) : nullptr;
}

// static
XPCNativeInterface*
XPCNativeInterface::GetISupports()
{
    // XXX We should optimize this to cache this common XPCNativeInterface.
    return GetNewOrUsed(&NS_GET_IID(nsISupports));
}

// static
XPCNativeInterface*
XPCNativeInterface::NewInstance(nsIInterfaceInfo* aInfo)
{
    AutoJSContext cx;
    static const uint16_t MAX_LOCAL_MEMBER_COUNT = 16;
    XPCNativeMember local_members[MAX_LOCAL_MEMBER_COUNT];
    XPCNativeInterface* obj = nullptr;
    XPCNativeMember* members = nullptr;

    int i;
    bool failed = false;
    uint16_t constCount;
    uint16_t methodCount;
    uint16_t totalCount;
    uint16_t realTotalCount = 0;
    XPCNativeMember* cur;
    RootedString str(cx);
    RootedId interfaceName(cx);

    // XXX Investigate lazy init? This is a problem given the
    // 'placement new' scheme - we need to at least know how big to make
    // the object. We might do a scan of methods to determine needed size,
    // then make our object, but avoid init'ing *any* members until asked?
    // Find out how often we create these objects w/o really looking at
    // (or using) the members.

    bool canScript;
    if (NS_FAILED(aInfo->IsScriptable(&canScript)) || !canScript)
        return nullptr;

    bool mainProcessScriptableOnly;
    if (NS_FAILED(aInfo->IsMainProcessScriptableOnly(&mainProcessScriptableOnly)))
        return nullptr;
    if (mainProcessScriptableOnly && !XRE_IsParentProcess()) {
        nsCOMPtr<nsIConsoleService> console(do_GetService(NS_CONSOLESERVICE_CONTRACTID));
        if (console) {
            const char* intfNameChars;
            aInfo->GetNameShared(&intfNameChars);
            nsPrintfCString errorMsg("Use of %s in content process is deprecated.", intfNameChars);

            nsAutoString filename;
            uint32_t lineno = 0, column = 0;
            nsJSUtils::GetCallingLocation(cx, filename, &lineno, &column);
            nsCOMPtr<nsIScriptError> error(do_CreateInstance(NS_SCRIPTERROR_CONTRACTID));
            error->Init(NS_ConvertUTF8toUTF16(errorMsg),
                        filename, EmptyString(),
                        lineno, column, nsIScriptError::warningFlag, "chrome javascript");
            console->LogMessage(error);
        }
    }

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

        str = JS_AtomizeAndPinString(cx, info->GetName());
        if (!str) {
            NS_ERROR("bad method name");
            failed = true;
            break;
        }
        jsid name = INTERNED_STRING_TO_JSID(cx, str);

        if (info->IsSetter()) {
            MOZ_ASSERT(realTotalCount,"bad setter");
            // Note: ASSUMES Getter/Setter pairs are next to each other
            // This is a rule of the typelib spec.
            cur = &members[realTotalCount-1];
            MOZ_ASSERT(cur->GetName() == name,"bad setter");
            MOZ_ASSERT(cur->IsReadOnlyAttribute(),"bad setter");
            MOZ_ASSERT(cur->GetIndex() == i-1,"bad setter");
            cur->SetWritableAttribute();
        } else {
            // XXX need better way to find dups
            // MOZ_ASSERT(!LookupMemberByID(name),"duplicate method name");
            if (realTotalCount == XPCNativeMember::GetMaxIndexInInterface()) {
                NS_WARNING("Too many members in interface");
                failed = true;
                break;
            }
            cur = &members[realTotalCount];
            cur->SetName(name);
            if (info->IsGetter())
                cur->SetReadOnlyAttribute(i);
            else
                cur->SetMethod(i);
            cur->SetIndexInInterface(realTotalCount);
            ++realTotalCount;
        }
    }

    if (!failed) {
        for (i = 0; i < constCount; i++) {
            RootedValue constant(cx);
            nsXPIDLCString namestr;
            if (NS_FAILED(aInfo->GetConstant(i, &constant, getter_Copies(namestr)))) {
                failed = true;
                break;
            }

            str = JS_AtomizeAndPinString(cx, namestr);
            if (!str) {
                NS_ERROR("bad constant name");
                failed = true;
                break;
            }
            jsid name = INTERNED_STRING_TO_JSID(cx, str);

            // XXX need better way to find dups
            //MOZ_ASSERT(!LookupMemberByID(name),"duplicate method/constant name");
            if (realTotalCount == XPCNativeMember::GetMaxIndexInInterface()) {
                NS_WARNING("Too many members in interface");
                failed = true;
                break;
            }
            cur = &members[realTotalCount];
            cur->SetName(name);
            cur->SetConstant(i);
            cur->SetIndexInInterface(realTotalCount);
            ++realTotalCount;
        }
    }

    if (!failed) {
        const char* bytes;
        if (NS_FAILED(aInfo->GetNameShared(&bytes)) || !bytes ||
            nullptr == (str = JS_AtomizeAndPinString(cx, bytes))) {
            failed = true;
        }
        interfaceName = INTERNED_STRING_TO_JSID(cx, str);
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
XPCNativeInterface::SizeOfIncludingThis(MallocSizeOf mallocSizeOf)
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
// XPCNativeSetKey

static PLDHashNumber
HashPointer(const void* ptr)
{
    return NS_PTR_TO_UINT32(ptr) >> 2;
}

PLDHashNumber
XPCNativeSetKey::Hash() const
{
    PLDHashNumber h = 0;

    if (!mBaseSet) {
        MOZ_ASSERT(mAddition, "bad key");
        h ^= HashPointer(mAddition);
    } else {
        XPCNativeInterface** current = mBaseSet->GetInterfaceArray();
        uint16_t count = mBaseSet->GetInterfaceCount();
        if (mAddition) {
            count++;
            for (uint16_t i = 0; i < count; i++) {
                if (i == mPosition)
                    h ^= HashPointer(mAddition);
                else
                    h ^= HashPointer(*(current++));
            }
        } else {
            for (uint16_t i = 0; i < count; i++)
                h ^= HashPointer(*(current++));
        }
    }

    return h;
}

/***************************************************************************/
// XPCNativeSet

// static
XPCNativeSet*
XPCNativeSet::GetNewOrUsed(const nsIID* iid)
{
    AutoJSContext cx;
    AutoMarkingNativeSetPtr set(cx);

    AutoMarkingNativeInterfacePtr iface(cx);
    iface = XPCNativeInterface::GetNewOrUsed(iid);
    if (!iface)
        return nullptr;

    XPCNativeSetKey key(nullptr, iface, 0);

    XPCJSRuntime* rt = XPCJSRuntime::Get();
    NativeSetMap* map = rt->GetNativeSetMap();
    if (!map)
        return nullptr;

    set = map->Find(&key);

    if (set)
        return set;

    // hacky way to get a XPCNativeInterface** using the AutoPtr
    XPCNativeInterface* temp[] = {iface};
    set = NewInstance(temp, 1);
    if (!set)
        return nullptr;

    XPCNativeSet* set2 = map->Add(&key, set);
    if (!set2) {
        NS_ERROR("failed to add our set!");
        DestroyInstance(set);
        set = nullptr;
    } else if (set2 != set) {
        DestroyInstance(set);
        set = set2;
    }

    return set;
}

// static
XPCNativeSet*
XPCNativeSet::GetNewOrUsed(nsIClassInfo* classInfo)
{
    AutoJSContext cx;
    AutoMarkingNativeSetPtr set(cx);
    XPCJSRuntime* rt = XPCJSRuntime::Get();

    ClassInfo2NativeSetMap* map = rt->GetClassInfo2NativeSetMap();
    if (!map)
        return nullptr;

    set = map->Find(classInfo);

    if (set)
        return set;

    nsIID** iidArray = nullptr;
    AutoMarkingNativeInterfacePtrArrayPtr interfaceArray(cx);
    uint32_t iidCount = 0;

    if (NS_FAILED(classInfo->GetInterfaces(&iidCount, &iidArray))) {
        // Note: I'm making it OK for this call to fail so that one can add
        // nsIClassInfo to classes implemented in script without requiring this
        // method to be implemented.

        // Make sure these are set correctly...
        iidArray = nullptr;
        iidCount = 0;
    }

    MOZ_ASSERT((iidCount && iidArray) || !(iidCount || iidArray), "GetInterfaces returned bad array");

    // !!! from here on we only exit through the 'out' label !!!

    if (iidCount) {
        AutoMarkingNativeInterfacePtrArrayPtr
            arr(cx, new XPCNativeInterface*[iidCount], iidCount, true);

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
                XPCNativeInterface::GetNewOrUsed(iid);

            if (!iface) {
                // XXX warn here
                continue;
            }

            *(currentInterface++) = iface;
            interfaceCount++;
        }

        if (interfaceCount) {
            set = NewInstance(interfaceArray, interfaceCount);
            if (set) {
                NativeSetMap* map2 = rt->GetNativeSetMap();
                if (!map2)
                    goto out;

                XPCNativeSetKey key(set, nullptr, 0);

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
        } else
            set = GetNewOrUsed(&NS_GET_IID(nsISupports));
    } else
        set = GetNewOrUsed(&NS_GET_IID(nsISupports));

    if (set) {
#ifdef DEBUG
        XPCNativeSet* set2 =
#endif
          map->Add(classInfo, set);
        MOZ_ASSERT(set2, "failed to add our set!");
        MOZ_ASSERT(set2 == set, "hashtables inconsistent!");
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
        map->Remove(classInfo);
}

// static
XPCNativeSet*
XPCNativeSet::GetNewOrUsed(XPCNativeSet* otherSet,
                           XPCNativeInterface* newInterface,
                           uint16_t position)
{
    AutoJSContext cx;
    AutoMarkingNativeSetPtr set(cx);
    XPCJSRuntime* rt = XPCJSRuntime::Get();
    NativeSetMap* map = rt->GetNativeSetMap();
    if (!map)
        return nullptr;

    XPCNativeSetKey key(otherSet, newInterface, position);

    set = map->Find(&key);

    if (set)
        return set;

    if (otherSet)
        set = NewInstanceMutate(otherSet, newInterface, position);
    else
        set = NewInstance(&newInterface, 1);

    if (!set)
        return nullptr;

    XPCNativeSet* set2 = map->Add(&key, set);
    if (!set2) {
        NS_ERROR("failed to add our set!");
        DestroyInstance(set);
        set = nullptr;
    } else if (set2 != set) {
        DestroyInstance(set);
        set = set2;
    }

    return set;
}

// static
XPCNativeSet*
XPCNativeSet::GetNewOrUsed(XPCNativeSet* firstSet,
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
            currentSet = XPCNativeSet::GetNewOrUsed(currentSet, iface, pos);
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
XPCNativeSet::NewInstance(XPCNativeInterface** array,
                          uint16_t count)
{
    if (!array || !count)
        return nullptr;

    // We impose the invariant:
    // "All sets have exactly one nsISupports interface and it comes first."
    // This is the place where we impose that rule - even if given inputs
    // that don't exactly follow the rule.

    XPCNativeInterface* isup = XPCNativeInterface::GetISupports();
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
    XPCNativeSet* obj = new(place) XPCNativeSet();

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

    return obj;
}

// static
XPCNativeSet*
XPCNativeSet::NewInstanceMutate(XPCNativeSet* otherSet,
                                XPCNativeInterface* newInterface,
                                uint16_t position)
{
    MOZ_ASSERT(otherSet);

    if (!newInterface)
        return nullptr;
    if (position > otherSet->mInterfaceCount)
        return nullptr;

    // Use placement new to create an object with the right amount of space
    // to hold the members array
    int size = sizeof(XPCNativeSet);
    size += otherSet->mInterfaceCount * sizeof(XPCNativeInterface*);
    void* place = new char[size];
    XPCNativeSet* obj = new(place) XPCNativeSet();

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
XPCNativeSet::SizeOfIncludingThis(MallocSizeOf mallocSizeOf)
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
