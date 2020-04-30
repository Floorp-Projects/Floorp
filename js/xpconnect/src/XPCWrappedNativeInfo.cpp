/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Manage the shared info about interfaces for use by wrappedNatives. */

#include "xpcprivate.h"
#include "js/Wrapper.h"

#include "mozilla/MemoryReporting.h"
#include "nsIScriptError.h"
#include "nsPrintfCString.h"
#include "nsPointerHashKeys.h"

using namespace JS;
using namespace mozilla;

/***************************************************************************/

// XPCNativeMember

// static
bool XPCNativeMember::GetCallInfo(JSObject* funobj,
                                  RefPtr<XPCNativeInterface>* pInterface,
                                  XPCNativeMember** pMember) {
  funobj = js::UncheckedUnwrap(funobj);
  Value memberVal =
      js::GetFunctionNativeReserved(funobj, XPC_FUNCTION_NATIVE_MEMBER_SLOT);

  *pMember = static_cast<XPCNativeMember*>(memberVal.toPrivate());
  *pInterface = (*pMember)->GetInterface();

  return true;
}

bool XPCNativeMember::NewFunctionObject(XPCCallContext& ccx,
                                        XPCNativeInterface* iface,
                                        HandleObject parent, Value* pval) {
  MOZ_ASSERT(!IsConstant(),
             "Only call this if you're sure this is not a constant!");

  return Resolve(ccx, iface, parent, pval);
}

bool XPCNativeMember::Resolve(XPCCallContext& ccx, XPCNativeInterface* iface,
                              HandleObject parent, Value* vp) {
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
    if (NS_FAILED(iface->GetInterfaceInfo()->GetMethodInfo(mIndex, &info))) {
      return false;
    }

    // Note: ASSUMES that retval is last arg.
    argc = (int)info->ParamCount();
    if (info->HasRetval()) {
      argc--;
    }

    callback = XPC_WN_CallMethod;
  } else {
    argc = 0;
    callback = XPC_WN_GetterSetter;
  }

  JSFunction* fun;
  jsid name = GetName();
  if (JSID_IS_STRING(name)) {
    fun = js::NewFunctionByIdWithReserved(ccx, callback, argc, 0, GetName());
  } else {
    fun = js::NewFunctionWithReserved(ccx, callback, argc, 0, nullptr);
  }
  if (!fun) {
    return false;
  }

  JSObject* funobj = JS_GetFunctionObject(fun);
  if (!funobj) {
    return false;
  }

  js::SetFunctionNativeReserved(funobj, XPC_FUNCTION_NATIVE_MEMBER_SLOT,
                                PrivateValue(this));
  js::SetFunctionNativeReserved(funobj, XPC_FUNCTION_PARENT_OBJECT_SLOT,
                                ObjectValue(*parent));

  vp->setObject(*funobj);

  return true;
}

/***************************************************************************/
// XPCNativeInterface

XPCNativeInterface::~XPCNativeInterface() {
  XPCJSRuntime::Get()->GetIID2NativeInterfaceMap()->Remove(this);
}

// static
already_AddRefed<XPCNativeInterface> XPCNativeInterface::GetNewOrUsed(
    JSContext* cx, const nsIID* iid) {
  RefPtr<XPCNativeInterface> iface;
  XPCJSRuntime* rt = XPCJSRuntime::Get();

  IID2NativeInterfaceMap* map = rt->GetIID2NativeInterfaceMap();
  if (!map) {
    return nullptr;
  }

  iface = map->Find(*iid);

  if (iface) {
    return iface.forget();
  }

  const nsXPTInterfaceInfo* info = nsXPTInterfaceInfo::ByIID(*iid);
  if (!info) {
    return nullptr;
  }

  iface = NewInstance(cx, info);
  if (!iface) {
    return nullptr;
  }

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
already_AddRefed<XPCNativeInterface> XPCNativeInterface::GetNewOrUsed(
    JSContext* cx, const nsXPTInterfaceInfo* info) {
  RefPtr<XPCNativeInterface> iface;

  XPCJSRuntime* rt = XPCJSRuntime::Get();

  IID2NativeInterfaceMap* map = rt->GetIID2NativeInterfaceMap();
  if (!map) {
    return nullptr;
  }

  iface = map->Find(info->IID());

  if (iface) {
    return iface.forget();
  }

  iface = NewInstance(cx, info);
  if (!iface) {
    return nullptr;
  }

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
already_AddRefed<XPCNativeInterface> XPCNativeInterface::GetNewOrUsed(
    JSContext* cx, const char* name) {
  const nsXPTInterfaceInfo* info = nsXPTInterfaceInfo::ByName(name);
  return info ? GetNewOrUsed(cx, info) : nullptr;
}

// static
already_AddRefed<XPCNativeInterface> XPCNativeInterface::GetISupports(
    JSContext* cx) {
  // XXX We should optimize this to cache this common XPCNativeInterface.
  return GetNewOrUsed(cx, &NS_GET_IID(nsISupports));
}

// static
already_AddRefed<XPCNativeInterface> XPCNativeInterface::NewInstance(
    JSContext* cx, const nsXPTInterfaceInfo* aInfo) {
  static const uint16_t MAX_LOCAL_MEMBER_COUNT = 16;
  XPCNativeMember local_members[MAX_LOCAL_MEMBER_COUNT];
  RefPtr<XPCNativeInterface> obj;
  XPCNativeMember* members = nullptr;

  int i;
  bool failed = false;
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

  if (aInfo->IsMainProcessScriptableOnly() && !XRE_IsParentProcess()) {
    nsCOMPtr<nsIConsoleService> console(
        do_GetService(NS_CONSOLESERVICE_CONTRACTID));
    if (console) {
      const char* intfNameChars = aInfo->Name();
      nsPrintfCString errorMsg("Use of %s in content process is deprecated.",
                               intfNameChars);

      nsAutoString filename;
      uint32_t lineno = 0, column = 0;
      nsJSUtils::GetCallingLocation(cx, filename, &lineno, &column);
      nsCOMPtr<nsIScriptError> error(
          do_CreateInstance(NS_SCRIPTERROR_CONTRACTID));
      error->Init(NS_ConvertUTF8toUTF16(errorMsg), filename, EmptyString(),
                  lineno, column, nsIScriptError::warningFlag,
                  "chrome javascript", false /* from private window */,
                  true /* from chrome context */);
      console->LogMessage(error);
    }
  }

  uint16_t methodCount = aInfo->MethodCount();
  uint16_t constCount = aInfo->ConstantCount();
  totalCount = methodCount + constCount;

  if (totalCount > MAX_LOCAL_MEMBER_COUNT) {
    members = new XPCNativeMember[totalCount];
  } else {
    members = local_members;
  }

  // NOTE: since getters and setters share a member, we might not use all
  // of the member objects.

  for (i = 0; i < methodCount; i++) {
    const nsXPTMethodInfo& info = aInfo->Method(i);

    // don't reflect Addref or Release
    if (i == 1 || i == 2) {
      continue;
    }

    if (!info.IsReflectable()) {
      continue;
    }

    jsid name;
    if (!info.GetId(cx, name)) {
      NS_ERROR("bad method name");
      failed = true;
      break;
    }

    if (info.IsSetter()) {
      MOZ_ASSERT(realTotalCount, "bad setter");
      // Note: ASSUMES Getter/Setter pairs are next to each other
      // This is a rule of the typelib spec.
      cur = &members[realTotalCount - 1];
      MOZ_ASSERT(cur->GetName() == name, "bad setter");
      MOZ_ASSERT(cur->IsReadOnlyAttribute(), "bad setter");
      MOZ_ASSERT(cur->GetIndex() == i - 1, "bad setter");
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
      if (info.IsGetter()) {
        cur->SetReadOnlyAttribute(i);
      } else {
        cur->SetMethod(i);
      }
      cur->SetIndexInInterface(realTotalCount);
      ++realTotalCount;
    }
  }

  if (!failed) {
    for (i = 0; i < constCount; i++) {
      RootedValue constant(cx);
      nsCString namestr;
      if (NS_FAILED(aInfo->GetConstant(i, &constant, getter_Copies(namestr)))) {
        failed = true;
        break;
      }

      str = JS_AtomizeAndPinString(cx, namestr.get());
      if (!str) {
        NS_ERROR("bad constant name");
        failed = true;
        break;
      }
      jsid name = PropertyKey::fromPinnedString(str);

      // XXX need better way to find dups
      // MOZ_ASSERT(!LookupMemberByID(name),"duplicate method/constant name");
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
    const char* bytes = aInfo->Name();
    if (nullptr == bytes ||
        nullptr == (str = JS_AtomizeAndPinString(cx, bytes))) {
      failed = true;
    }
    interfaceName = PropertyKey::fromPinnedString(str);
  }

  if (!failed) {
    // Use placement new to create an object with the right amount of space
    // to hold the members array
    int size = sizeof(XPCNativeInterface);
    if (realTotalCount > 1) {
      size += (realTotalCount - 1) * sizeof(XPCNativeMember);
    }
    void* place = new char[size];
    if (place) {
      obj = new (place) XPCNativeInterface(aInfo, interfaceName);
    }

    if (obj) {
      obj->mMemberCount = realTotalCount;
      // copy valid members
      if (realTotalCount) {
        memcpy(obj->mMembers, members,
               realTotalCount * sizeof(XPCNativeMember));
      }
    }
  }

  if (members && members != local_members) {
    delete[] members;
  }

  return obj.forget();
}

// static
void XPCNativeInterface::DestroyInstance(XPCNativeInterface* inst) {
  inst->~XPCNativeInterface();
  delete[](char*) inst;
}

size_t XPCNativeInterface::SizeOfIncludingThis(MallocSizeOf mallocSizeOf) {
  return mallocSizeOf(this);
}

void XPCNativeInterface::DebugDump(int16_t depth) {
#ifdef DEBUG
  depth--;
  XPC_LOG_ALWAYS(("XPCNativeInterface @ %p", this));
  XPC_LOG_INDENT();
  XPC_LOG_ALWAYS(("name is %s", GetNameString()));
  XPC_LOG_ALWAYS(("mInfo @ %p", mInfo));
  XPC_LOG_OUTDENT();
#endif
}

/***************************************************************************/
// XPCNativeSetKey

static PLDHashNumber HashPointer(const void* ptr) {
  return nsPtrHashKey<const void>::HashKey(ptr);
}

PLDHashNumber XPCNativeSetKey::Hash() const {
  PLDHashNumber h = 0;

  if (mBaseSet) {
    // If we ever start using mCx here, adjust the constructors accordingly.
    XPCNativeInterface** current = mBaseSet->GetInterfaceArray();
    uint16_t count = mBaseSet->GetInterfaceCount();
    for (uint16_t i = 0; i < count; i++) {
      h ^= HashPointer(*(current++));
    }
  } else {
    // A newly created set will contain nsISupports first...
    RefPtr<XPCNativeInterface> isupp = XPCNativeInterface::GetISupports(mCx);
    h ^= HashPointer(isupp);

    // ...but no more than once.
    if (isupp == mAddition) {
      return h;
    }
  }

  if (mAddition) {
    h ^= HashPointer(mAddition);
  }

  return h;
}

/***************************************************************************/
// XPCNativeSet

XPCNativeSet::~XPCNativeSet() {
  // Remove |this| before we clear the interfaces to ensure that the
  // hashtable look up is correct.

  XPCJSRuntime::Get()->GetNativeSetMap()->Remove(this);

  for (int i = 0; i < mInterfaceCount; i++) {
    NS_RELEASE(mInterfaces[i]);
  }
}

// static
already_AddRefed<XPCNativeSet> XPCNativeSet::GetNewOrUsed(JSContext* cx,
                                                          const nsIID* iid) {
  RefPtr<XPCNativeInterface> iface = XPCNativeInterface::GetNewOrUsed(cx, iid);
  if (!iface) {
    return nullptr;
  }

  XPCNativeSetKey key(cx, iface);

  XPCJSRuntime* xpcrt = XPCJSRuntime::Get();
  NativeSetMap* map = xpcrt->GetNativeSetMap();
  if (!map) {
    return nullptr;
  }

  RefPtr<XPCNativeSet> set = map->Find(&key);

  if (set) {
    return set.forget();
  }

  set = NewInstance(cx, {std::move(iface)});
  if (!set) {
    return nullptr;
  }

  if (!map->AddNew(&key, set)) {
    NS_ERROR("failed to add our set!");
    set = nullptr;
  }

  return set.forget();
}

// static
already_AddRefed<XPCNativeSet> XPCNativeSet::GetNewOrUsed(
    JSContext* cx, nsIClassInfo* classInfo) {
  XPCJSRuntime* xpcrt = XPCJSRuntime::Get();
  ClassInfo2NativeSetMap* map = xpcrt->GetClassInfo2NativeSetMap();
  if (!map) {
    return nullptr;
  }

  RefPtr<XPCNativeSet> set = map->Find(classInfo);

  if (set) {
    return set.forget();
  }

  AutoTArray<nsIID, 4> iids;
  if (NS_FAILED(classInfo->GetInterfaces(iids))) {
    // Note: I'm making it OK for this call to fail so that one can add
    // nsIClassInfo to classes implemented in script without requiring this
    // method to be implemented.

    // Make sure these are set correctly...
    iids.Clear();
  }

  // Try to look up each IID's XPCNativeInterface object.
  nsTArray<RefPtr<XPCNativeInterface>> interfaces(iids.Length());
  for (auto& iid : iids) {
    RefPtr<XPCNativeInterface> iface =
        XPCNativeInterface::GetNewOrUsed(cx, &iid);
    if (iface) {
      interfaces.AppendElement(iface.forget());
    }
  }

  // Build a set from the interfaces specified here.
  if (interfaces.Length() > 0) {
    set = NewInstance(cx, std::move(interfaces));
    if (set) {
      NativeSetMap* map2 = xpcrt->GetNativeSetMap();
      if (!map2) {
        return set.forget();
      }

      XPCNativeSetKey key(set);
      XPCNativeSet* set2 = map2->Add(&key, set);
      if (!set2) {
        NS_ERROR("failed to add our set");
        return nullptr;
      }

      // It is okay to find an existing entry here because
      // we did not look for one before we called Add().
      if (set2 != set) {
        set = set2;
      }
    }
  } else {
    set = GetNewOrUsed(cx, &NS_GET_IID(nsISupports));
  }

  if (set) {
#ifdef DEBUG
    XPCNativeSet* set2 =
#endif
        map->Add(classInfo, set);
    MOZ_ASSERT(set2, "failed to add our set!");
    MOZ_ASSERT(set2 == set, "hashtables inconsistent!");
  }

  return set.forget();
}

// static
void XPCNativeSet::ClearCacheEntryForClassInfo(nsIClassInfo* classInfo) {
  XPCJSRuntime* xpcrt = nsXPConnect::GetRuntimeInstance();
  ClassInfo2NativeSetMap* map = xpcrt->GetClassInfo2NativeSetMap();
  if (map) {
    map->Remove(classInfo);
  }
}

// static
already_AddRefed<XPCNativeSet> XPCNativeSet::GetNewOrUsed(
    JSContext* cx, XPCNativeSetKey* key) {
  NativeSetMap* map = XPCJSRuntime::Get()->GetNativeSetMap();
  if (!map) {
    return nullptr;
  }

  RefPtr<XPCNativeSet> set = map->Find(key);

  if (set) {
    return set.forget();
  }

  if (key->GetBaseSet()) {
    set = NewInstanceMutate(key);
  } else {
    set = NewInstance(cx, {key->GetAddition()});
  }

  if (!set) {
    return nullptr;
  }

  if (!map->AddNew(key, set)) {
    NS_ERROR("failed to add our set!");
    set = nullptr;
  }

  return set.forget();
}

// static
already_AddRefed<XPCNativeSet> XPCNativeSet::GetNewOrUsed(
    JSContext* cx, XPCNativeSet* firstSet, XPCNativeSet* secondSet,
    bool preserveFirstSetOrder) {
  // Figure out how many interfaces we'll need in the new set.
  uint32_t uniqueCount = firstSet->mInterfaceCount;
  for (uint32_t i = 0; i < secondSet->mInterfaceCount; ++i) {
    if (!firstSet->HasInterface(secondSet->mInterfaces[i])) {
      uniqueCount++;
    }
  }

  // If everything in secondSet was a duplicate, we can just use the first
  // set.
  if (uniqueCount == firstSet->mInterfaceCount) {
    return RefPtr<XPCNativeSet>(firstSet).forget();
  }

  // If the secondSet is just a superset of the first, we can use it provided
  // that the caller doesn't care about ordering.
  if (!preserveFirstSetOrder && uniqueCount == secondSet->mInterfaceCount) {
    return RefPtr<XPCNativeSet>(secondSet).forget();
  }

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
      currentSet = XPCNativeSet::GetNewOrUsed(cx, &key);
      if (!currentSet) {
        return nullptr;
      }
    }
  }

  // We've got the union set. Hand it back to the caller.
  MOZ_ASSERT(currentSet->mInterfaceCount == uniqueCount);
  return currentSet.forget();
}

// static
already_AddRefed<XPCNativeSet> XPCNativeSet::NewInstance(
    JSContext* cx, nsTArray<RefPtr<XPCNativeInterface>>&& array) {
  if (array.Length() == 0) {
    return nullptr;
  }

  // We impose the invariant:
  // "All sets have exactly one nsISupports interface and it comes first."
  // This is the place where we impose that rule - even if given inputs
  // that don't exactly follow the rule.

  RefPtr<XPCNativeInterface> isup = XPCNativeInterface::GetISupports(cx);
  uint16_t slots = array.Length() + 1;

  for (auto key = array.begin(); key != array.end(); key++) {
    if (*key == isup) {
      slots--;
    }
  }

  // Use placement new to create an object with the right amount of space
  // to hold the members array
  int size = sizeof(XPCNativeSet);
  if (slots > 1) {
    size += (slots - 1) * sizeof(XPCNativeInterface*);
  }
  void* place = new char[size];
  RefPtr<XPCNativeSet> obj = new (place) XPCNativeSet();

  // Stick the nsISupports in front and skip additional nsISupport(s)
  XPCNativeInterface** outp = (XPCNativeInterface**)&obj->mInterfaces;

  NS_ADDREF(*(outp++) = isup);

  for (auto key = array.begin(); key != array.end(); key++) {
    RefPtr<XPCNativeInterface> cur = std::move(*key);
    if (isup == cur) {
      continue;
    }
    *(outp++) = cur.forget().take();
  }
  obj->mInterfaceCount = slots;

  return obj.forget();
}

// static
already_AddRefed<XPCNativeSet> XPCNativeSet::NewInstanceMutate(
    XPCNativeSetKey* key) {
  XPCNativeSet* otherSet = key->GetBaseSet();
  XPCNativeInterface* newInterface = key->GetAddition();

  MOZ_ASSERT(otherSet);

  if (!newInterface) {
    return nullptr;
  }

  // Use placement new to create an object with the right amount of space
  // to hold the members array
  int size = sizeof(XPCNativeSet);
  size += otherSet->mInterfaceCount * sizeof(XPCNativeInterface*);
  void* place = new char[size];
  RefPtr<XPCNativeSet> obj = new (place) XPCNativeSet();

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
void XPCNativeSet::DestroyInstance(XPCNativeSet* inst) {
  inst->~XPCNativeSet();
  delete[](char*) inst;
}

size_t XPCNativeSet::SizeOfIncludingThis(MallocSizeOf mallocSizeOf) {
  return mallocSizeOf(this);
}

void XPCNativeSet::DebugDump(int16_t depth) {
#ifdef DEBUG
  depth--;
  XPC_LOG_ALWAYS(("XPCNativeSet @ %p", this));
  XPC_LOG_INDENT();

  XPC_LOG_ALWAYS(("mInterfaceCount of %d", mInterfaceCount));
  if (depth) {
    for (uint16_t i = 0; i < mInterfaceCount; i++) {
      mInterfaces[i]->DebugDump(depth);
    }
  }
  XPC_LOG_OUTDENT();
#endif
}
