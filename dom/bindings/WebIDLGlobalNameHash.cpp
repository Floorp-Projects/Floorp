/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebIDLGlobalNameHash.h"
#include "js/Class.h"
#include "js/GCAPI.h"
#include "js/Id.h"
#include "js/Wrapper.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/HashFunctions.h"
#include "mozilla/Maybe.h"
#include "mozilla/dom/DOMJSClass.h"
#include "mozilla/dom/DOMJSProxyHandler.h"
#include "mozilla/dom/JSSlots.h"
#include "mozilla/dom/PrototypeList.h"
#include "mozilla/dom/RegisterBindings.h"
#include "nsGlobalWindow.h"
#include "nsIMemoryReporter.h"
#include "nsTHashtable.h"
#include "WrapperFactory.h"

namespace mozilla {
namespace dom {

struct MOZ_STACK_CLASS WebIDLNameTableKey
{
  explicit WebIDLNameTableKey(JSFlatString* aJSString)
    : mLength(js::GetFlatStringLength(aJSString))
  {
    mNogc.emplace();
    JSLinearString* jsString = js::FlatStringToLinearString(aJSString);
    if (js::LinearStringHasLatin1Chars(jsString)) {
      mLatin1String = reinterpret_cast<const char*>(
        js::GetLatin1LinearStringChars(*mNogc, jsString));
      mTwoBytesString = nullptr;
      mHash = mLatin1String ? HashString(mLatin1String, mLength) : 0;
    } else {
      mLatin1String = nullptr;
      mTwoBytesString = js::GetTwoByteLinearStringChars(*mNogc, jsString);
      mHash = mTwoBytesString ? HashString(mTwoBytesString, mLength) : 0;
    }
  }
  explicit WebIDLNameTableKey(const char* aString, size_t aLength)
    : mLatin1String(aString),
      mTwoBytesString(nullptr),
      mLength(aLength),
      mHash(HashString(aString, aLength))
  {
    MOZ_ASSERT(aString[aLength] == '\0');
  }

  Maybe<JS::AutoCheckCannotGC> mNogc;
  const char* mLatin1String;
  const char16_t* mTwoBytesString;
  size_t mLength;
  PLDHashNumber mHash;
};

struct WebIDLNameTableEntry : public PLDHashEntryHdr
{
  typedef const WebIDLNameTableKey& KeyType;
  typedef const WebIDLNameTableKey* KeyTypePointer;

  explicit WebIDLNameTableEntry(KeyTypePointer aKey)
    : mNameOffset(0),
      mNameLength(0),
      mConstructorId(constructors::id::_ID_Count),
      mCreate(nullptr),
      mEnabled(nullptr)
  {}
  WebIDLNameTableEntry(WebIDLNameTableEntry&& aEntry)
    : mNameOffset(aEntry.mNameOffset),
      mNameLength(aEntry.mNameLength),
      mConstructorId(aEntry.mConstructorId),
      mCreate(aEntry.mCreate),
      mEnabled(aEntry.mEnabled)
  {}
  ~WebIDLNameTableEntry()
  {}

  bool KeyEquals(KeyTypePointer aKey) const
  {
    if (mNameLength != aKey->mLength) {
      return false;
    }

    const char* name = WebIDLGlobalNameHash::sNames + mNameOffset;

    if (aKey->mLatin1String) {
      return PodEqual(aKey->mLatin1String, name, aKey->mLength);
    }

    return nsCharTraits<char16_t>::compareASCII(aKey->mTwoBytesString, name,
                                                aKey->mLength) == 0;
  }

  static KeyTypePointer KeyToPointer(KeyType aKey)
  {
    return &aKey;
  }

  static PLDHashNumber HashKey(KeyTypePointer aKey)
  {
    return aKey->mHash;
  }

  enum { ALLOW_MEMMOVE = true };

  uint16_t mNameOffset;
  uint16_t mNameLength;
  constructors::id::ID mConstructorId;
  CreateInterfaceObjectsMethod mCreate;
  // May be null if enabled unconditionally
  WebIDLGlobalNameHash::ConstructorEnabled mEnabled;
};

static nsTHashtable<WebIDLNameTableEntry>* sWebIDLGlobalNames;

class WebIDLGlobalNamesHashReporter final : public nsIMemoryReporter
{
  MOZ_DEFINE_MALLOC_SIZE_OF(MallocSizeOf)

  ~WebIDLGlobalNamesHashReporter() {}

public:
  NS_DECL_ISUPPORTS

  NS_IMETHOD CollectReports(nsIHandleReportCallback* aHandleReport,
                            nsISupports* aData, bool aAnonymize) override
  {
    int64_t amount =
      sWebIDLGlobalNames ?
      sWebIDLGlobalNames->ShallowSizeOfIncludingThis(MallocSizeOf) : 0;

    MOZ_COLLECT_REPORT(
      "explicit/dom/webidl-globalnames", KIND_HEAP, UNITS_BYTES, amount,
      "Memory used by the hash table for WebIDL's global names.");

    return NS_OK;
  }
};

NS_IMPL_ISUPPORTS(WebIDLGlobalNamesHashReporter, nsIMemoryReporter)

/* static */
void
WebIDLGlobalNameHash::Init()
{
  sWebIDLGlobalNames = new nsTHashtable<WebIDLNameTableEntry>(sCount);
  RegisterWebIDLGlobalNames();

  RegisterStrongMemoryReporter(new WebIDLGlobalNamesHashReporter());
}

/* static */
void
WebIDLGlobalNameHash::Shutdown()
{
  delete sWebIDLGlobalNames;
}

/* static */
void
WebIDLGlobalNameHash::Register(uint16_t aNameOffset, uint16_t aNameLength,
                               CreateInterfaceObjectsMethod aCreate,
                               ConstructorEnabled aEnabled,
                               constructors::id::ID aConstructorId)
{
  const char* name = sNames + aNameOffset;
  WebIDLNameTableKey key(name, aNameLength);
  WebIDLNameTableEntry* entry = sWebIDLGlobalNames->PutEntry(key);
  entry->mNameOffset = aNameOffset;
  entry->mNameLength = aNameLength;
  entry->mCreate = aCreate;
  entry->mEnabled = aEnabled;
  entry->mConstructorId = aConstructorId;
}

/* static */
void
WebIDLGlobalNameHash::Remove(const char* aName, uint32_t aLength)
{
  WebIDLNameTableKey key(aName, aLength);
  sWebIDLGlobalNames->RemoveEntry(key);
}

static JSObject*
FindNamedConstructorForXray(JSContext* aCx, JS::Handle<jsid> aId,
                            const WebIDLNameTableEntry* aEntry)
{
  JSObject* interfaceObject =
    GetPerInterfaceObjectHandle(aCx, aEntry->mConstructorId,
                                aEntry->mCreate,
                                /* aDefineOnGlobal = */ false);
  if (!interfaceObject) {
    return nullptr;
  }

  // This is a call over Xrays, so we will actually use the return value
  // (instead of just having it defined on the global now).  Check for named
  // constructors with this id, in case that's what the caller is asking for.
  for (unsigned slot = DOM_INTERFACE_SLOTS_BASE;
       slot < JSCLASS_RESERVED_SLOTS(js::GetObjectClass(interfaceObject));
       ++slot) {
    JSObject* constructor = &js::GetReservedSlot(interfaceObject, slot).toObject();
    if (JS_GetFunctionId(JS_GetObjectFunction(constructor)) == JSID_TO_STRING(aId)) {
      return constructor;
    }
  }

  // None of the named constructors match, so the caller must want the
  // interface object itself.
  return interfaceObject;
}

/* static */
bool
WebIDLGlobalNameHash::DefineIfEnabled(JSContext* aCx,
                                      JS::Handle<JSObject*> aObj,
                                      JS::Handle<jsid> aId,
                                      JS::MutableHandle<JS::PropertyDescriptor> aDesc,
                                      bool* aFound)
{
  MOZ_ASSERT(JSID_IS_STRING(aId), "Check for string id before calling this!");

  const WebIDLNameTableEntry* entry;
  {
    WebIDLNameTableKey key(JSID_TO_FLAT_STRING(aId));
    // Rooting analysis thinks nsTHashtable<...>::GetEntry may GC because it
    // ends up calling through PLDHashTableOps' matchEntry function pointer, but
    // we know WebIDLNameTableEntry::KeyEquals can't cause a GC.
    JS::AutoSuppressGCAnalysis suppress;
    entry = sWebIDLGlobalNames->GetEntry(key);
  }

  if (!entry) {
    *aFound = false;
    return true;
  }

  *aFound = true;

  ConstructorEnabled checkEnabledForScope = entry->mEnabled;
  // We do the enabled check on the current compartment of aCx, but for the
  // actual object we pass in the underlying object in the Xray case.  That
  // way the callee can decide whether to allow access based on the caller
  // or the window being touched.
  JS::Rooted<JSObject*> global(aCx,
    js::CheckedUnwrap(aObj, /* stopAtWindowProxy = */ false));
  if (!global) {
    return Throw(aCx, NS_ERROR_DOM_SECURITY_ERR);
  }

  {
    // It's safe to pass "&global" here, because we've already unwrapped it, but
    // for general sanity better to not have debug code even having the
    // appearance of mutating things that opt code uses.
#ifdef DEBUG
    JS::Rooted<JSObject*> temp(aCx, global);
    DebugOnly<nsGlobalWindowInner*> win;
    MOZ_ASSERT(NS_SUCCEEDED(UNWRAP_OBJECT(Window, &temp, win)));
#endif
  }

  if (checkEnabledForScope && !checkEnabledForScope(aCx, global)) {
    return true;
  }

  // The DOM constructor resolve machinery interacts with Xrays in tricky
  // ways, and there are some asymmetries that are important to understand.
  //
  // In the regular (non-Xray) case, we only want to resolve constructors
  // once (so that if they're deleted, they don't reappear). We do this by
  // stashing the constructor in a slot on the global, such that we can see
  // during resolve whether we've created it already. This is rather
  // memory-intensive, so we don't try to maintain these semantics when
  // manipulating a global over Xray (so the properties just re-resolve if
  // they've been deleted).
  //
  // Unfortunately, there's a bit of an impedance-mismatch between the Xray
  // and non-Xray machinery. The Xray machinery wants an API that returns a
  // JS::PropertyDescriptor, so that the resolve hook doesn't have to get
  // snared up with trying to define a property on the Xray holder. At the
  // same time, the DefineInterface callbacks are set up to define things
  // directly on the global.  And re-jiggering them to return property
  // descriptors is tricky, because some DefineInterface callbacks define
  // multiple things (like the Image() alias for HTMLImageElement).
  //
  // So the setup is as-follows:
  //
  // * The resolve function takes a JS::PropertyDescriptor, but in the
  //   non-Xray case, callees may define things directly on the global, and
  //   set the value on the property descriptor to |undefined| to indicate
  //   that there's nothing more for the caller to do. We assert against
  //   this behavior in the Xray case.
  //
  // * We make sure that we do a non-Xray resolve first, so that all the
  //   slots are set up. In the Xray case, this means unwrapping and doing
  //   a non-Xray resolve before doing the Xray resolve.
  //
  // This all could use some grand refactoring, but for now we just limp
  // along.
  if (xpc::WrapperFactory::IsXrayWrapper(aObj)) {
    JS::Rooted<JSObject*> constructor(aCx);
    {
      JSAutoRealm ar(aCx, global);
      constructor = FindNamedConstructorForXray(aCx, aId, entry);
    }
    if (NS_WARN_IF(!constructor)) {
      return Throw(aCx, NS_ERROR_FAILURE);
    }
    if (!JS_WrapObject(aCx, &constructor)) {
      return Throw(aCx, NS_ERROR_FAILURE);
    }

    FillPropertyDescriptor(aDesc, aObj, 0, JS::ObjectValue(*constructor));
    return true;
  }

  JS::Rooted<JSObject*> interfaceObject(aCx,
    GetPerInterfaceObjectHandle(aCx, entry->mConstructorId,
                                entry->mCreate,
                                /* aDefineOnGlobal = */ true));
  if (NS_WARN_IF(!interfaceObject)) {
    return Throw(aCx, NS_ERROR_FAILURE);
  }

  // We've already defined the property.  We indicate this to the caller
  // by filling a property descriptor with JS::UndefinedValue() as the
  // value.  We still have to fill in a property descriptor, though, so
  // that the caller knows the property is in fact on this object.  It
  // doesn't matter what we pass for the "readonly" argument here.
  FillPropertyDescriptor(aDesc, aObj, JS::UndefinedValue(), false);

  return true;
}

/* static */
bool
WebIDLGlobalNameHash::MayResolve(jsid aId)
{
  WebIDLNameTableKey key(JSID_TO_FLAT_STRING(aId));
  // Rooting analysis thinks nsTHashtable<...>::Contains may GC because it ends
  // up calling through PLDHashTableOps' matchEntry function pointer, but we
  // know WebIDLNameTableEntry::KeyEquals can't cause a GC.
  JS::AutoSuppressGCAnalysis suppress;
  return sWebIDLGlobalNames->Contains(key);
}

/* static */
bool
WebIDLGlobalNameHash::GetNames(JSContext* aCx, JS::Handle<JSObject*> aObj,
                               NameType aNameType, JS::AutoIdVector& aNames)
{
  // aObj is always a Window here, so GetProtoAndIfaceCache on it is safe.
  ProtoAndIfaceCache* cache = GetProtoAndIfaceCache(aObj);
  for (auto iter = sWebIDLGlobalNames->Iter(); !iter.Done(); iter.Next()) {
    const WebIDLNameTableEntry* entry = iter.Get();
    // If aNameType is not AllNames, only include things whose entry slot in the
    // ProtoAndIfaceCache is null.
    if ((aNameType == AllNames ||
         !cache->HasEntryInSlot(entry->mConstructorId)) &&
        (!entry->mEnabled || entry->mEnabled(aCx, aObj))) {
      JSString* str = JS_AtomizeStringN(aCx, sNames + entry->mNameOffset,
                                        entry->mNameLength);
      if (!str || !aNames.append(NON_INTEGER_ATOM_TO_JSID(str))) {
        return false;
      }
    }
  }

  return true;
}

} // namespace dom
} // namespace mozilla
