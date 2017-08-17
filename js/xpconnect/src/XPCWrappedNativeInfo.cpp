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
#include "nsIScriptError.h"
#include "nsPrintfCString.h"
#include "nsPointerHashKeys.h"

using namespace JS;
using namespace mozilla;

/***************************************************************************/

// XPCNativeMember

// static
bool
XPCNativeMember::GetCallInfo(JSObject* funobj,
                             RefPtr<XPCNativeInterface>* pInterface,
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
        nsCString name;
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

XPCNativeInterface::~XPCNativeInterface()
{
    XPCJSRuntime::Get()->GetIID2NativeInterfaceMap()->Remove(this);
}

// static
already_AddRefed<XPCNativeInterface>
XPCNativeInterface::GetNewOrUsed(const nsIID* iid)
{
    RefPtr<XPCNativeInterface> iface;
    XPCJSRuntime* rt = XPCJSRuntime::Get();

    IID2NativeInterfaceMap* map = rt->GetIID2NativeInterfaceMap();
    if (!map)
        return nullptr;

    iface = map->Find(*iid);

    if (iface)
        return iface.forget();

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
        iface = nullptr;
    } else if (iface2 != iface) {
        iface = iface2;
    }

    return iface.forget();
}

// static
already_AddRefed<XPCNativeInterface>
XPCNativeInterface::GetNewOrUsed(nsIInterfaceInfo* info)
{
    RefPtr<XPCNativeInterface> iface;

    const nsIID* iid;
    if (NS_FAILED(info->GetIIDShared(&iid)) || !iid)
        return nullptr;

    XPCJSRuntime* rt = XPCJSRuntime::Get();

    IID2NativeInterfaceMap* map = rt->GetIID2NativeInterfaceMap();
    if (!map)
        return nullptr;

    iface = map->Find(*iid);

    if (iface)
        return iface.forget();

    iface = NewInstance(info);
    if (!iface)
        return nullptr;

    RefPtr<XPCNativeInterface> iface2 = map->Add(iface);
    if (!iface2) {
        NS_ERROR("failed to add our interface!");
        iface = nullptr;
    } else if (iface2 != iface) {
        iface = iface2;
    }

    return iface.forget();
}

// static
already_AddRefed<XPCNativeInterface>
XPCNativeInterface::GetNewOrUsed(const char* name)
{
    nsCOMPtr<nsIInterfaceInfo> info;
    XPTInterfaceInfoManager::GetSingleton()->GetInfoForName(name, getter_AddRefs(info));
    return info ? GetNewOrUsed(info) : nullptr;
}

// static
already_AddRefed<XPCNativeInterface>
XPCNativeInterface::GetISupports()
{
    // XXX We should optimize this to cache this common XPCNativeInterface.
    return GetNewOrUsed(&NS_GET_IID(nsISupports));
}

// static
already_AddRefed<XPCNativeInterface>
XPCNativeInterface::NewInstance(nsIInterfaceInfo* aInfo)
{
    AutoJSContext cx;
    static const uint16_t MAX_LOCAL_MEMBER_COUNT = 16;
    XPCNativeMember local_members[MAX_LOCAL_MEMBER_COUNT];
    RefPtr<XPCNativeInterface> obj;
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

    return obj.forget();
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
    XPC_LOG_ALWAYS(("XPCNativeInterface @ %p", this));
        XPC_LOG_INDENT();
        XPC_LOG_ALWAYS(("name is %s", GetNameString()));
        XPC_LOG_ALWAYS(("mMemberCount is %d", mMemberCount));
        XPC_LOG_ALWAYS(("mInfo @ %p", mInfo.get()));
        XPC_LOG_OUTDENT();
#endif
}

/***************************************************************************/
// XPCNativeSetKey

static PLDHashNumber
HashPointer(const void* ptr)
{
    return nsPtrHashKey<const void>::HashKey(ptr);
}

PLDHashNumber
XPCNativeSetKey::Hash() const
{
    PLDHashNumber h = 0;

    if (mBaseSet) {
        XPCNativeInterface** current = mBaseSet->GetInterfaceArray();
        uint16_t count = mBaseSet->GetInterfaceCount();
        for (uint16_t i = 0; i < count; i++) {
            h ^= HashPointer(*(current++));
        }
    } else {
        // A newly created set will contain nsISupports first...
        RefPtr<XPCNativeInterface> isupp = XPCNativeInterface::GetISupports();
        h ^= HashPointer(isupp);

        // ...but no more than once.
        if (isupp == mAddition)
            return h;
    }

    if (mAddition) {
        h ^= HashPointer(mAddition);
    }

    return h;
}

/***************************************************************************/
// XPCNativeSet

XPCNativeSet::~XPCNativeSet()
{
    // Remove |this| before we clear the interfaces to ensure that the
    // hashtable look up is correct.
    XPCJSRuntime::Get()->GetNativeSetMap()->Remove(this);

    for (int i = 0; i < mInterfaceCount; i++) {
        NS_RELEASE(mInterfaces[i]);
    }
}

// static
already_AddRefed<XPCNativeSet>
XPCNativeSet::GetNewOrUsed(const nsIID* iid)
{
    RefPtr<XPCNativeInterface> iface =
        XPCNativeInterface::GetNewOrUsed(iid);
    if (!iface)
        return nullptr;

    XPCNativeSetKey key(iface);

    XPCJSRuntime* xpcrt = XPCJSRuntime::Get();
    NativeSetMap* map = xpcrt->GetNativeSetMap();
    if (!map)
        return nullptr;

    RefPtr<XPCNativeSet> set = map->Find(&key);

    if (set)
        return set.forget();

    set = NewInstance({iface.forget()});
    if (!set)
        return nullptr;

    if (!map->AddNew(&key, set)) {
        NS_ERROR("failed to add our set!");
        set = nullptr;
    }

    return set.forget();
}

// static
already_AddRefed<XPCNativeSet>
XPCNativeSet::GetNewOrUsed(nsIClassInfo* classInfo)
{
    XPCJSRuntime* xpcrt = XPCJSRuntime::Get();
    ClassInfo2NativeSetMap* map = xpcrt->GetClassInfo2NativeSetMap();
    if (!map)
        return nullptr;

    RefPtr<XPCNativeSet> set = map->Find(classInfo);

    if (set)
        return set.forget();

    nsIID** iidArray = nullptr;
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
        nsTArray<RefPtr<XPCNativeInterface>> interfaceArray(iidCount);
        nsIID** currentIID = iidArray;

        for (uint32_t i = 0; i < iidCount; i++) {
            nsIID* iid = *(currentIID++);
            if (!iid) {
                NS_ERROR("Null found in classinfo interface list");
                continue;
            }

            RefPtr<XPCNativeInterface> iface =
                XPCNativeInterface::GetNewOrUsed(iid);

            if (!iface) {
                // XXX warn here
                continue;
            }

            interfaceArray.AppendElement(iface.forget());
        }

        if (interfaceArray.Length() > 0) {
            set = NewInstance(Move(interfaceArray));
            if (set) {
                NativeSetMap* map2 = xpcrt->GetNativeSetMap();
                if (!map2)
                    goto out;

                XPCNativeSetKey key(set);

                XPCNativeSet* set2 = map2->Add(&key, set);
                if (!set2) {
                    NS_ERROR("failed to add our set!");
                    set = nullptr;
                    goto out;
                }
                // It is okay to find an existing entry here because
                // we did not look for one before we called Add().
                if (set2 != set) {
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

    return set.forget();
}

// static
void
XPCNativeSet::ClearCacheEntryForClassInfo(nsIClassInfo* classInfo)
{
    XPCJSRuntime* xpcrt = nsXPConnect::GetRuntimeInstance();
    ClassInfo2NativeSetMap* map = xpcrt->GetClassInfo2NativeSetMap();
    if (map)
        map->Remove(classInfo);
}

// static
already_AddRefed<XPCNativeSet>
XPCNativeSet::GetNewOrUsed(XPCNativeSetKey* key)
{
    NativeSetMap* map = XPCJSRuntime::Get()->GetNativeSetMap();
    if (!map)
        return nullptr;

    RefPtr<XPCNativeSet> set = map->Find(key);

    if (set)
        return set.forget();

    if (key->GetBaseSet())
        set = NewInstanceMutate(key);
    else
        set = NewInstance({key->GetAddition()});

    if (!set)
        return nullptr;

    if (!map->AddNew(key, set)) {
        NS_ERROR("failed to add our set!");
        set = nullptr;
    }

    return set.forget();
}

// static
already_AddRefed<XPCNativeSet>
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
        return RefPtr<XPCNativeSet>(firstSet).forget();

    // If the secondSet is just a superset of the first, we can use it provided
    // that the caller doesn't care about ordering.
    if (!preserveFirstSetOrder && uniqueCount == secondSet->mInterfaceCount)
        return RefPtr<XPCNativeSet>(secondSet).forget();

    // Ok, darn. Now we have to make a new set.
    //
    // It would be faster to just create the new set all at once, but that
    // would involve wrangling with some pretty hairy code - especially since
    // a lot of stuff assumes that sets are created by adding one interface to an
    // existing set. So let's just do the slow and easy thing and hope that the
    // above optimizations handle the common cases.
    RefPtr<XPCNativeSet> currentSet = firstSet;
    for (uint32_t i = 0; i < secondSet->mInterfaceCount; ++i) {
        XPCNativeInterface* iface = secondSet->mInterfaces[i];
        if (!currentSet->HasInterface(iface)) {
            // Create a new augmented set, inserting this interface at the end.
            XPCNativeSetKey key(currentSet, iface);
            currentSet = XPCNativeSet::GetNewOrUsed(&key);
            if (!currentSet)
                return nullptr;
        }
    }

    // We've got the union set. Hand it back to the caller.
    MOZ_ASSERT(currentSet->mInterfaceCount == uniqueCount);
    return currentSet.forget();
}

// static
already_AddRefed<XPCNativeSet>
XPCNativeSet::NewInstance(nsTArray<RefPtr<XPCNativeInterface>>&& array)
{
    if (array.Length() == 0)
        return nullptr;

    // We impose the invariant:
    // "All sets have exactly one nsISupports interface and it comes first."
    // This is the place where we impose that rule - even if given inputs
    // that don't exactly follow the rule.

    RefPtr<XPCNativeInterface> isup = XPCNativeInterface::GetISupports();
    uint16_t slots = array.Length() + 1;

    for (auto key = array.begin(); key != array.end(); key++) {
        if (*key == isup)
            slots--;
    }

    // Use placement new to create an object with the right amount of space
    // to hold the members array
    int size = sizeof(XPCNativeSet);
    if (slots > 1)
        size += (slots - 1) * sizeof(XPCNativeInterface*);
    void* place = new char[size];
    RefPtr<XPCNativeSet> obj = new(place) XPCNativeSet();

    // Stick the nsISupports in front and skip additional nsISupport(s)
    XPCNativeInterface** outp = (XPCNativeInterface**) &obj->mInterfaces;
    uint16_t memberCount = 1;   // for the one member in nsISupports

    NS_ADDREF(*(outp++) = isup);

    for (auto key = array.begin(); key != array.end(); key++) {
        RefPtr<XPCNativeInterface> cur = key->forget();
        if (isup == cur)
            continue;
        memberCount += cur->GetMemberCount();
        *(outp++) = cur.forget().take();
    }
    obj->mMemberCount = memberCount;
    obj->mInterfaceCount = slots;

    return obj.forget();
}

// static
already_AddRefed<XPCNativeSet>
XPCNativeSet::NewInstanceMutate(XPCNativeSetKey* key)
{
    XPCNativeSet* otherSet = key->GetBaseSet();
    XPCNativeInterface* newInterface = key->GetAddition();

    MOZ_ASSERT(otherSet);

    if (!newInterface)
        return nullptr;

    // Use placement new to create an object with the right amount of space
    // to hold the members array
    int size = sizeof(XPCNativeSet);
    size += otherSet->mInterfaceCount * sizeof(XPCNativeInterface*);
    void* place = new char[size];
    RefPtr<XPCNativeSet> obj = new(place) XPCNativeSet();

    obj->mMemberCount = otherSet->GetMemberCount() +
        newInterface->GetMemberCount();
    obj->mInterfaceCount = otherSet->mInterfaceCount + 1;

    XPCNativeInterface** src = otherSet->mInterfaces;
    XPCNativeInterface** dest = obj->mInterfaces;
    for (uint16_t i = 0; i < otherSet->mInterfaceCount; i++) {
        NS_ADDREF(*dest++ = *src++);
    }
    NS_ADDREF(*dest++ = newInterface);

    return obj.forget();
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
    XPC_LOG_ALWAYS(("XPCNativeSet @ %p", this));
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
