/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsScriptNameSpaceManager.h"
#include "nsCOMPtr.h"
#include "nsIComponentManager.h"
#include "nsIComponentRegistrar.h"
#include "nsICategoryManager.h"
#include "nsIServiceManager.h"
#include "nsXPCOM.h"
#include "nsISupportsPrimitives.h"
#include "nsIScriptExternalNameSet.h"
#include "nsIScriptNameSpaceManager.h"
#include "nsIScriptContext.h"
#include "nsIInterfaceInfoManager.h"
#include "nsIInterfaceInfo.h"
#include "xptinfo.h"
#include "nsXPIDLString.h"
#include "nsPrintfCString.h"
#include "nsReadableUtils.h"
#include "nsHashKeys.h"
#include "nsDOMClassInfo.h"
#include "nsCRT.h"
#include "nsIObserverService.h"
#include "nsISimpleEnumerator.h"

#include "mozilla/MemoryReporting.h"
#include "mozilla/Preferences.h"
#include "mozilla/Services.h"

#define NS_INTERFACE_PREFIX "nsI"
#define NS_DOM_INTERFACE_PREFIX "nsIDOM"

using namespace mozilla;

// Our extended PLDHashEntryHdr
class GlobalNameMapEntry : public PLDHashEntryHdr
{
public:
  // Our hash table ops don't care about the order of these members
  nsString mKey;
  nsGlobalNameStruct mGlobalName;

  size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) {
    // Measurement of the following members may be added later if DMD finds it
    // is worthwhile:
    // - mGlobalName
    return mKey.SizeOfExcludingThisMustBeUnshared(aMallocSizeOf);
  }
};


static PLDHashNumber
GlobalNameHashHashKey(PLDHashTable *table, const void *key)
{
  const nsAString *str = static_cast<const nsAString *>(key);
  return HashString(*str);
}

static bool
GlobalNameHashMatchEntry(PLDHashTable *table, const PLDHashEntryHdr *entry,
                         const void *key)
{
  const GlobalNameMapEntry *e =
    static_cast<const GlobalNameMapEntry *>(entry);
  const nsAString *str = static_cast<const nsAString *>(key);

  return str->Equals(e->mKey);
}

static void
GlobalNameHashClearEntry(PLDHashTable *table, PLDHashEntryHdr *entry)
{
  GlobalNameMapEntry *e = static_cast<GlobalNameMapEntry *>(entry);

  // An entry is being cleared, let the key (nsString) do its own
  // cleanup.
  e->mKey.~nsString();
  if (e->mGlobalName.mType == nsGlobalNameStruct::eTypeExternalClassInfo) {
    nsIClassInfo* ci = GET_CLEAN_CI_PTR(e->mGlobalName.mData->mCachedClassInfo);

    // If we constructed an internal helper, we'll let the helper delete 
    // the nsDOMClassInfoData structure, if not we do it here.
    if (!ci || e->mGlobalName.mData->u.mExternalConstructorFptr) {
      delete e->mGlobalName.mData;
    }

    // Release our pointer to the helper.
    NS_IF_RELEASE(ci);
  }
  else if (e->mGlobalName.mType == nsGlobalNameStruct::eTypeExternalConstructorAlias) {
    delete e->mGlobalName.mAlias;
  }

  // This will set e->mGlobalName.mType to
  // nsGlobalNameStruct::eTypeNotInitialized
  memset(&e->mGlobalName, 0, sizeof(nsGlobalNameStruct));
}

static bool
GlobalNameHashInitEntry(PLDHashTable *table, PLDHashEntryHdr *entry,
                        const void *key)
{
  GlobalNameMapEntry *e = static_cast<GlobalNameMapEntry *>(entry);
  const nsAString *keyStr = static_cast<const nsAString *>(key);

  // Initialize the key in the entry with placement new
  new (&e->mKey) nsString(*keyStr);

  // This will set e->mGlobalName.mType to
  // nsGlobalNameStruct::eTypeNotInitialized
  memset(&e->mGlobalName, 0, sizeof(nsGlobalNameStruct));
  return true;
}

NS_IMPL_ISUPPORTS3(
  nsScriptNameSpaceManager,
  nsIObserver,
  nsISupportsWeakReference,
  nsIMemoryReporter)

nsScriptNameSpaceManager::nsScriptNameSpaceManager()
  : mIsInitialized(false)
{
  MOZ_COUNT_CTOR(nsScriptNameSpaceManager);
}

nsScriptNameSpaceManager::~nsScriptNameSpaceManager()
{
  if (mIsInitialized) {
    UnregisterWeakMemoryReporter(this);
    // Destroy the hash
    PL_DHashTableFinish(&mGlobalNames);
    PL_DHashTableFinish(&mNavigatorNames);
  }
  MOZ_COUNT_DTOR(nsScriptNameSpaceManager);
}

nsGlobalNameStruct *
nsScriptNameSpaceManager::AddToHash(PLDHashTable *aTable, const nsAString *aKey,
                                    const char16_t **aClassName)
{
  GlobalNameMapEntry *entry =
    static_cast<GlobalNameMapEntry *>
               (PL_DHashTableOperate(aTable, aKey, PL_DHASH_ADD));

  if (!entry) {
    return nullptr;
  }

  if (aClassName) {
    *aClassName = entry->mKey.get();
  }

  return &entry->mGlobalName;
}

void
nsScriptNameSpaceManager::RemoveFromHash(PLDHashTable *aTable,
                                         const nsAString *aKey)
{
  PL_DHashTableOperate(aTable, aKey, PL_DHASH_REMOVE);
}

nsGlobalNameStruct*
nsScriptNameSpaceManager::GetConstructorProto(const nsGlobalNameStruct* aStruct)
{
  NS_ASSERTION(aStruct->mType == nsGlobalNameStruct::eTypeExternalConstructorAlias,
               "This function only works on constructor aliases!");
  if (!aStruct->mAlias->mProto) {
    GlobalNameMapEntry *proto =
      static_cast<GlobalNameMapEntry *>
                 (PL_DHashTableOperate(&mGlobalNames,
                                          &aStruct->mAlias->mProtoName,
                                          PL_DHASH_LOOKUP));

    if (PL_DHASH_ENTRY_IS_BUSY(proto)) {
      aStruct->mAlias->mProto = &proto->mGlobalName;
    }
  }
  return aStruct->mAlias->mProto;
}

nsresult
nsScriptNameSpaceManager::FillHash(nsICategoryManager *aCategoryManager,
                                   const char *aCategory)
{
  nsCOMPtr<nsISimpleEnumerator> e;
  nsresult rv = aCategoryManager->EnumerateCategory(aCategory,
                                                    getter_AddRefs(e));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsISupports> entry;
  while (NS_SUCCEEDED(e->GetNext(getter_AddRefs(entry)))) {
    rv = AddCategoryEntryToHash(aCategoryManager, aCategory, entry);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  return NS_OK;
}


nsresult
nsScriptNameSpaceManager::RegisterExternalInterfaces(bool aAsProto)
{
  nsresult rv;
  nsCOMPtr<nsICategoryManager> cm =
    do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIInterfaceInfoManager>
    iim(do_GetService(NS_INTERFACEINFOMANAGER_SERVICE_CONTRACTID));
  NS_ENSURE_TRUE(iim, NS_ERROR_NOT_AVAILABLE);

  nsCOMPtr<nsISimpleEnumerator> enumerator;
  rv = cm->EnumerateCategory(JAVASCRIPT_DOM_INTERFACE,
                             getter_AddRefs(enumerator));
  NS_ENSURE_SUCCESS(rv, rv);

  nsXPIDLCString IID_string;
  nsAutoCString category_entry;
  const char* if_name;
  nsCOMPtr<nsISupports> entry;
  nsCOMPtr<nsIInterfaceInfo> if_info;
  bool found_old, dom_prefix;

  while (NS_SUCCEEDED(enumerator->GetNext(getter_AddRefs(entry)))) {
    nsCOMPtr<nsISupportsCString> category(do_QueryInterface(entry));

    if (!category) {
      NS_WARNING("Category entry not an nsISupportsCString!");

      continue;
    }

    rv = category->GetData(category_entry);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = cm->GetCategoryEntry(JAVASCRIPT_DOM_INTERFACE, category_entry.get(),
                              getter_Copies(IID_string));
    NS_ENSURE_SUCCESS(rv, rv);

    nsIID primary_IID;
    if (!primary_IID.Parse(IID_string) ||
        primary_IID.Equals(NS_GET_IID(nsISupports))) {
      NS_ERROR("Invalid IID registered with the script namespace manager!");
      continue;
    }

    iim->GetInfoForIID(&primary_IID, getter_AddRefs(if_info));

    while (if_info) {
      const nsIID *iid;
      if_info->GetIIDShared(&iid);
      NS_ENSURE_TRUE(iid, NS_ERROR_UNEXPECTED);

      if (iid->Equals(NS_GET_IID(nsISupports))) {
        break;
      }

      if_info->GetNameShared(&if_name);
      dom_prefix = (strncmp(if_name, NS_DOM_INTERFACE_PREFIX,
                            sizeof(NS_DOM_INTERFACE_PREFIX) - 1) == 0);

      const char* name;
      if (dom_prefix) {
        name = if_name + sizeof(NS_DOM_INTERFACE_PREFIX) - 1;
      } else {
        name = if_name + sizeof(NS_INTERFACE_PREFIX) - 1;
      }

      if (aAsProto) {
        RegisterClassProto(name, iid, &found_old);
      } else {
        RegisterInterface(name, iid, &found_old);
      }

      if (found_old) {
        break;
      }

      nsCOMPtr<nsIInterfaceInfo> tmp(if_info);
      tmp->GetParent(getter_AddRefs(if_info));
    }
  }

  return NS_OK;
}

nsresult
nsScriptNameSpaceManager::RegisterInterface(const char* aIfName,
                                            const nsIID *aIfIID,
                                            bool* aFoundOld)
{
  *aFoundOld = false;

  nsGlobalNameStruct *s = AddToHash(&mGlobalNames, aIfName);
  NS_ENSURE_TRUE(s, NS_ERROR_OUT_OF_MEMORY);

  if (s->mType != nsGlobalNameStruct::eTypeNotInitialized &&
      s->mType != nsGlobalNameStruct::eTypeNewDOMBinding) {
    *aFoundOld = true;

    return NS_OK;
  }

  s->mType = nsGlobalNameStruct::eTypeInterface;
  s->mIID = *aIfIID;

  return NS_OK;
}

#define GLOBALNAME_HASHTABLE_INITIAL_SIZE	1024

nsresult
nsScriptNameSpaceManager::Init()
{
  static const PLDHashTableOps hash_table_ops =
  {
    PL_DHashAllocTable,
    PL_DHashFreeTable,
    GlobalNameHashHashKey,
    GlobalNameHashMatchEntry,
    PL_DHashMoveEntryStub,
    GlobalNameHashClearEntry,
    PL_DHashFinalizeStub,
    GlobalNameHashInitEntry
  };

  mIsInitialized = PL_DHashTableInit(&mGlobalNames, &hash_table_ops, nullptr,
                                     sizeof(GlobalNameMapEntry), 
                                     GLOBALNAME_HASHTABLE_INITIAL_SIZE);
  NS_ENSURE_TRUE(mIsInitialized, NS_ERROR_OUT_OF_MEMORY);

  mIsInitialized = PL_DHashTableInit(&mNavigatorNames, &hash_table_ops, nullptr,
                                     sizeof(GlobalNameMapEntry), 
                                     GLOBALNAME_HASHTABLE_INITIAL_SIZE);
  if (!mIsInitialized) {
    PL_DHashTableFinish(&mGlobalNames);

    return NS_ERROR_OUT_OF_MEMORY;
  }

  RegisterWeakMemoryReporter(this);

  nsresult rv = NS_OK;

  rv = RegisterExternalInterfaces(false);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsICategoryManager> cm =
    do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = FillHash(cm, JAVASCRIPT_GLOBAL_CONSTRUCTOR_CATEGORY);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = FillHash(cm, JAVASCRIPT_GLOBAL_PROPERTY_CATEGORY);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = FillHash(cm, JAVASCRIPT_GLOBAL_PRIVILEGED_PROPERTY_CATEGORY);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = FillHash(cm, JAVASCRIPT_GLOBAL_STATIC_NAMESET_CATEGORY);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = FillHash(cm, JAVASCRIPT_NAVIGATOR_PROPERTY_CATEGORY);
  NS_ENSURE_SUCCESS(rv, rv);

  // Initial filling of the has table has been done.
  // Now, listen for changes.
  nsCOMPtr<nsIObserverService> serv = 
    mozilla::services::GetObserverService();

  if (serv) {
    serv->AddObserver(this, NS_XPCOM_CATEGORY_ENTRY_ADDED_OBSERVER_ID, true);
    serv->AddObserver(this, NS_XPCOM_CATEGORY_ENTRY_REMOVED_OBSERVER_ID, true);
  }

  return NS_OK;
}

struct NameSetClosure {
  nsIScriptContext* ctx;
  nsresult rv;
};

static PLDHashOperator
NameSetInitCallback(PLDHashTable *table, PLDHashEntryHdr *hdr,
                    uint32_t number, void *arg)
{
  GlobalNameMapEntry *entry = static_cast<GlobalNameMapEntry *>(hdr);

  if (entry->mGlobalName.mType == nsGlobalNameStruct::eTypeStaticNameSet) {
    nsresult rv = NS_OK;
    nsCOMPtr<nsIScriptExternalNameSet> ns =
      do_CreateInstance(entry->mGlobalName.mCID, &rv);
    NS_ENSURE_SUCCESS(rv, PL_DHASH_NEXT);

    NameSetClosure *closure = static_cast<NameSetClosure *>(arg);
    closure->rv = ns->InitializeNameSet(closure->ctx);
    if (NS_FAILED(closure->rv)) {
      NS_ERROR("Initing external script classes failed!");
      return PL_DHASH_STOP;
    }
  }

  return PL_DHASH_NEXT;
}

nsresult
nsScriptNameSpaceManager::InitForContext(nsIScriptContext *aContext)
{
  NameSetClosure closure;
  closure.ctx = aContext;
  closure.rv = NS_OK;
  PL_DHashTableEnumerate(&mGlobalNames, NameSetInitCallback, &closure);

  return closure.rv;
}

nsGlobalNameStruct*
nsScriptNameSpaceManager::LookupNameInternal(const nsAString& aName,
                                             const char16_t **aClassName)
{
  GlobalNameMapEntry *entry =
    static_cast<GlobalNameMapEntry *>
               (PL_DHashTableOperate(&mGlobalNames, &aName,
                                        PL_DHASH_LOOKUP));

  if (PL_DHASH_ENTRY_IS_BUSY(entry) &&
      !((&entry->mGlobalName)->mDisabled)) {
    if (aClassName) {
      *aClassName = entry->mKey.get();
    }
    return &entry->mGlobalName;
  }

  if (aClassName) {
    *aClassName = nullptr;
  }
  return nullptr;
}

const nsGlobalNameStruct*
nsScriptNameSpaceManager::LookupNavigatorName(const nsAString& aName)
{
  GlobalNameMapEntry *entry =
    static_cast<GlobalNameMapEntry *>
               (PL_DHashTableOperate(&mNavigatorNames, &aName,
                                     PL_DHASH_LOOKUP));

  if (!PL_DHASH_ENTRY_IS_BUSY(entry) ||
      entry->mGlobalName.mDisabled) {
    return nullptr;
  }

  return &entry->mGlobalName;
}

nsresult
nsScriptNameSpaceManager::RegisterClassName(const char *aClassName,
                                            int32_t aDOMClassInfoID,
                                            bool aPrivileged,
                                            bool aXBLAllowed,
                                            bool aDisabled,
                                            const char16_t **aResult)
{
  if (!nsCRT::IsAscii(aClassName)) {
    NS_ERROR("Trying to register a non-ASCII class name");
    return NS_OK;
  }
  nsGlobalNameStruct *s = AddToHash(&mGlobalNames, aClassName, aResult);
  NS_ENSURE_TRUE(s, NS_ERROR_OUT_OF_MEMORY);

  if (s->mType == nsGlobalNameStruct::eTypeClassConstructor) {
    return NS_OK;
  }

  // If a external constructor is already defined with aClassName we
  // won't overwrite it.

  if (s->mType == nsGlobalNameStruct::eTypeExternalConstructor) {
    return NS_OK;
  }

  NS_ASSERTION(s->mType == nsGlobalNameStruct::eTypeNotInitialized ||
               s->mType == nsGlobalNameStruct::eTypeNewDOMBinding ||
               s->mType == nsGlobalNameStruct::eTypeInterface,
               "Whaaa, JS environment name clash!");

  s->mType = nsGlobalNameStruct::eTypeClassConstructor;
  s->mDOMClassInfoID = aDOMClassInfoID;
  s->mChromeOnly = aPrivileged;
  s->mAllowXBL = aXBLAllowed;
  s->mDisabled = aDisabled;

  return NS_OK;
}

nsresult
nsScriptNameSpaceManager::RegisterClassProto(const char *aClassName,
                                             const nsIID *aConstructorProtoIID,
                                             bool *aFoundOld)
{
  NS_ENSURE_ARG_POINTER(aConstructorProtoIID);

  *aFoundOld = false;

  nsGlobalNameStruct *s = AddToHash(&mGlobalNames, aClassName);
  NS_ENSURE_TRUE(s, NS_ERROR_OUT_OF_MEMORY);

  if (s->mType != nsGlobalNameStruct::eTypeNotInitialized &&
      s->mType != nsGlobalNameStruct::eTypeNewDOMBinding &&
      s->mType != nsGlobalNameStruct::eTypeInterface) {
    *aFoundOld = true;

    return NS_OK;
  }

  s->mType = nsGlobalNameStruct::eTypeClassProto;
  s->mIID = *aConstructorProtoIID;

  return NS_OK;
}

nsresult
nsScriptNameSpaceManager::RegisterExternalClassName(const char *aClassName,
                                                    nsCID& aCID)
{
  nsGlobalNameStruct *s = AddToHash(&mGlobalNames, aClassName);
  NS_ENSURE_TRUE(s, NS_ERROR_OUT_OF_MEMORY);

  // If an external constructor is already defined with aClassName we
  // won't overwrite it.

  if (s->mType == nsGlobalNameStruct::eTypeExternalConstructor) {
    return NS_OK;
  }

  NS_ASSERTION(s->mType == nsGlobalNameStruct::eTypeNotInitialized ||
               s->mType == nsGlobalNameStruct::eTypeNewDOMBinding ||
               s->mType == nsGlobalNameStruct::eTypeInterface,
               "Whaaa, JS environment name clash!");

  s->mType = nsGlobalNameStruct::eTypeExternalClassInfoCreator;
  s->mCID = aCID;

  return NS_OK;
}

nsresult
nsScriptNameSpaceManager::RegisterDOMCIData(const char *aName,
                                            nsDOMClassInfoExternalConstructorFnc aConstructorFptr,
                                            const nsIID *aProtoChainInterface,
                                            const nsIID **aInterfaces,
                                            uint32_t aScriptableFlags,
                                            bool aHasClassInterface,
                                            const nsCID *aConstructorCID)
{
  const char16_t* className;
  nsGlobalNameStruct *s = AddToHash(&mGlobalNames, aName, &className);
  NS_ENSURE_TRUE(s, NS_ERROR_OUT_OF_MEMORY);

  // If an external constructor is already defined with aClassName we
  // won't overwrite it.

  if (s->mType == nsGlobalNameStruct::eTypeClassConstructor ||
      s->mType == nsGlobalNameStruct::eTypeExternalClassInfo) {
    return NS_OK;
  }

  // XXX Should we bail out here?
  NS_ASSERTION(s->mType == nsGlobalNameStruct::eTypeNotInitialized ||
               s->mType == nsGlobalNameStruct::eTypeNewDOMBinding ||
               s->mType == nsGlobalNameStruct::eTypeExternalClassInfoCreator,
               "Someone tries to register classinfo data for a class that isn't new or external!");

  s->mData = new nsExternalDOMClassInfoData;
  NS_ENSURE_TRUE(s->mData, NS_ERROR_OUT_OF_MEMORY);

  s->mType = nsGlobalNameStruct::eTypeExternalClassInfo;
  s->mData->mName = aName;
  s->mData->mNameUTF16 = className;
  if (aConstructorFptr)
    s->mData->u.mExternalConstructorFptr = aConstructorFptr;
  else
    // null constructor will cause us to use nsDOMGenericSH::doCreate
    s->mData->u.mExternalConstructorFptr = nullptr;
  s->mData->mCachedClassInfo = nullptr;
  s->mData->mProtoChainInterface = aProtoChainInterface;
  s->mData->mInterfaces = aInterfaces;
  s->mData->mScriptableFlags = aScriptableFlags;
  s->mData->mHasClassInterface = aHasClassInterface;
  s->mData->mConstructorCID = aConstructorCID;

  return NS_OK;
}

nsresult
nsScriptNameSpaceManager::OperateCategoryEntryHash(nsICategoryManager* aCategoryManager,
                                                   const char* aCategory,
                                                   nsISupports* aEntry,
                                                   bool aRemove)
{
  MOZ_ASSERT(aCategoryManager);
  // Get the type from the category name.
  // NOTE: we could have passed the type in FillHash() and guessed it in
  // Observe() but this way, we have only one place to update and this is
  // not performance sensitive.
  nsGlobalNameStruct::nametype type;
  if (strcmp(aCategory, JAVASCRIPT_GLOBAL_CONSTRUCTOR_CATEGORY) == 0) {
    type = nsGlobalNameStruct::eTypeExternalConstructor;
  } else if (strcmp(aCategory, JAVASCRIPT_GLOBAL_PROPERTY_CATEGORY) == 0 ||
             strcmp(aCategory, JAVASCRIPT_GLOBAL_PRIVILEGED_PROPERTY_CATEGORY) == 0) {
    type = nsGlobalNameStruct::eTypeProperty;
  } else if (strcmp(aCategory, JAVASCRIPT_NAVIGATOR_PROPERTY_CATEGORY) == 0) {
    type = nsGlobalNameStruct::eTypeNavigatorProperty;
  } else if (strcmp(aCategory, JAVASCRIPT_GLOBAL_STATIC_NAMESET_CATEGORY) == 0) {
    type = nsGlobalNameStruct::eTypeStaticNameSet;
  } else {
    return NS_OK;
  }

  nsCOMPtr<nsISupportsCString> strWrapper = do_QueryInterface(aEntry);

  if (!strWrapper) {
    NS_WARNING("Category entry not an nsISupportsCString!");
    return NS_OK;
  }

  nsAutoCString categoryEntry;
  nsresult rv = strWrapper->GetData(categoryEntry);
  NS_ENSURE_SUCCESS(rv, rv);

  PLDHashTable *table;
  if (type == nsGlobalNameStruct::eTypeNavigatorProperty) {
    table = &mNavigatorNames;
  } else {
    table = &mGlobalNames;
  }

  // We need to handle removal before calling GetCategoryEntry
  // because the category entry is already removed before we are
  // notified.
  if (aRemove) {
    NS_ConvertASCIItoUTF16 entry(categoryEntry);
    const nsGlobalNameStruct *s =
      type == nsGlobalNameStruct::eTypeNavigatorProperty ?
      LookupNavigatorName(entry) : LookupNameInternal(entry);
    // Verify mType so that this API doesn't remove names
    // registered by others.
    if (!s || s->mType != type) {
      return NS_OK;
    }

    RemoveFromHash(table, &entry);
    return NS_OK;
  }

  nsXPIDLCString contractId;
  rv = aCategoryManager->GetCategoryEntry(aCategory, categoryEntry.get(),
                                          getter_Copies(contractId));
  NS_ENSURE_SUCCESS(rv, rv);

  if (type == nsGlobalNameStruct::eTypeNavigatorProperty ||
      type == nsGlobalNameStruct::eTypeExternalConstructor) {
    bool isNavProperty = type == nsGlobalNameStruct::eTypeNavigatorProperty;
    nsPrintfCString prefName("dom.%s.disable.%s",
                             isNavProperty ? "navigator-property" : "global-constructor",
                             categoryEntry.get());
    if (Preferences::GetType(prefName.get()) == nsIPrefBranch::PREF_BOOL &&
        Preferences::GetBool(prefName.get(), false)) {
        return NS_OK;
    }
  }

  nsCOMPtr<nsIComponentRegistrar> registrar;
  rv = NS_GetComponentRegistrar(getter_AddRefs(registrar));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCID *cidPtr;
  rv = registrar->ContractIDToCID(contractId, &cidPtr);

  if (NS_FAILED(rv)) {
    NS_WARNING("Bad contract id registed with the script namespace manager");
    return NS_OK;
  }

  // Copy CID onto the stack, so we can free it right away and avoid having
  // to add cleanup code at every exit point from this function.
  nsCID cid = *cidPtr;
  nsMemory::Free(cidPtr);

  if (type == nsGlobalNameStruct::eTypeExternalConstructor) {
    nsXPIDLCString constructorProto;
    rv = aCategoryManager->GetCategoryEntry(JAVASCRIPT_GLOBAL_CONSTRUCTOR_PROTO_ALIAS_CATEGORY,
                                            categoryEntry.get(),
                                            getter_Copies(constructorProto));
    if (NS_SUCCEEDED(rv)) {
      nsGlobalNameStruct *s = AddToHash(&mGlobalNames, categoryEntry.get());
      NS_ENSURE_TRUE(s, NS_ERROR_OUT_OF_MEMORY);

      if (s->mType == nsGlobalNameStruct::eTypeNotInitialized ||
          s->mType == nsGlobalNameStruct::eTypeNewDOMBinding) {
        s->mAlias = new nsGlobalNameStruct::ConstructorAlias;
        s->mType = nsGlobalNameStruct::eTypeExternalConstructorAlias;
        s->mChromeOnly = false;
        s->mAlias->mCID = cid;
        AppendASCIItoUTF16(constructorProto, s->mAlias->mProtoName);
        s->mAlias->mProto = nullptr;
      } else {
        NS_WARNING("Global script name not overwritten!");
      }

      return NS_OK;
    }
  }

  nsGlobalNameStruct *s = AddToHash(table, categoryEntry.get());
  NS_ENSURE_TRUE(s, NS_ERROR_OUT_OF_MEMORY);

  if (s->mType == nsGlobalNameStruct::eTypeNotInitialized ||
      s->mType == nsGlobalNameStruct::eTypeNewDOMBinding) {
    s->mType = type;
    s->mCID = cid;
    s->mChromeOnly =
      strcmp(aCategory, JAVASCRIPT_GLOBAL_PRIVILEGED_PROPERTY_CATEGORY) == 0;
  } else {
    NS_WARNING("Global script name not overwritten!");
  }

  return NS_OK;
}

nsresult
nsScriptNameSpaceManager::AddCategoryEntryToHash(nsICategoryManager* aCategoryManager,
                                                 const char* aCategory,
                                                 nsISupports* aEntry)
{
  return OperateCategoryEntryHash(aCategoryManager, aCategory, aEntry,
                                  /* aRemove = */ false);
}

nsresult
nsScriptNameSpaceManager::RemoveCategoryEntryFromHash(nsICategoryManager* aCategoryManager,
                                                      const char* aCategory,
                                                      nsISupports* aEntry)
{
  return OperateCategoryEntryHash(aCategoryManager, aCategory, aEntry,
                                  /* aRemove = */ true);
}

NS_IMETHODIMP
nsScriptNameSpaceManager::Observe(nsISupports* aSubject, const char* aTopic,
                                  const char16_t* aData)
{
  if (!aData) {
    return NS_OK;
  }

  if (!strcmp(aTopic, NS_XPCOM_CATEGORY_ENTRY_ADDED_OBSERVER_ID)) {
    nsCOMPtr<nsICategoryManager> cm =
      do_GetService(NS_CATEGORYMANAGER_CONTRACTID);
    if (!cm) {
      return NS_OK;
    }

    return AddCategoryEntryToHash(cm, NS_ConvertUTF16toUTF8(aData).get(),
                                  aSubject);
  } else if (!strcmp(aTopic, NS_XPCOM_CATEGORY_ENTRY_REMOVED_OBSERVER_ID)) {
    nsCOMPtr<nsICategoryManager> cm =
      do_GetService(NS_CATEGORYMANAGER_CONTRACTID);
    if (!cm) {
      return NS_OK;
    }

    return RemoveCategoryEntryFromHash(cm, NS_ConvertUTF16toUTF8(aData).get(),
                                       aSubject);
  }

  // TODO: we could observe NS_XPCOM_CATEGORY_CLEARED_OBSERVER_ID
  // but we are safe without it. See bug 600460.

  return NS_OK;
}

void
nsScriptNameSpaceManager::RegisterDefineDOMInterface(const nsAFlatString& aName,
    mozilla::dom::DefineInterface aDefineDOMInterface,
    mozilla::dom::ConstructorEnabled* aConstructorEnabled)
{
  nsGlobalNameStruct *s = AddToHash(&mGlobalNames, &aName);
  if (s) {
    if (s->mType == nsGlobalNameStruct::eTypeNotInitialized) {
      s->mType = nsGlobalNameStruct::eTypeNewDOMBinding;
    }
    s->mDefineDOMInterface = aDefineDOMInterface;
    s->mConstructorEnabled = aConstructorEnabled;
  }
}

void
nsScriptNameSpaceManager::RegisterNavigatorDOMConstructor(
    const nsAFlatString& aName,
    mozilla::dom::ConstructNavigatorProperty aNavConstructor,
    mozilla::dom::ConstructorEnabled* aConstructorEnabled)
{
  nsGlobalNameStruct *s = AddToHash(&mNavigatorNames, &aName);
  if (s) {
    if (s->mType == nsGlobalNameStruct::eTypeNotInitialized) {
      s->mType = nsGlobalNameStruct::eTypeNewDOMBinding;
    }
    s->mConstructNavigatorProperty = aNavConstructor;
    s->mConstructorEnabled = aConstructorEnabled;
  }
}

struct NameClosure
{
  nsScriptNameSpaceManager::NameEnumerator enumerator;
  void* closure;
};

static PLDHashOperator
EnumerateName(PLDHashTable*, PLDHashEntryHdr *hdr, uint32_t, void* aClosure)
{
  GlobalNameMapEntry *entry = static_cast<GlobalNameMapEntry *>(hdr);
  NameClosure* closure = static_cast<NameClosure*>(aClosure);
  return closure->enumerator(entry->mKey, closure->closure);
}

void
nsScriptNameSpaceManager::EnumerateGlobalNames(NameEnumerator aEnumerator,
                                               void* aClosure)
{
  NameClosure closure = { aEnumerator, aClosure };
  PL_DHashTableEnumerate(&mGlobalNames, EnumerateName, &closure);
}

void
nsScriptNameSpaceManager::EnumerateNavigatorNames(NameEnumerator aEnumerator,
                                                  void* aClosure)
{
  NameClosure closure = { aEnumerator, aClosure };
  PL_DHashTableEnumerate(&mNavigatorNames, EnumerateName, &closure);
}

static size_t
SizeOfEntryExcludingThis(PLDHashEntryHdr *aHdr, MallocSizeOf aMallocSizeOf,
                         void *aArg)
{
  GlobalNameMapEntry* entry = static_cast<GlobalNameMapEntry*>(aHdr);
  return entry->SizeOfExcludingThis(aMallocSizeOf);
}

MOZ_DEFINE_MALLOC_SIZE_OF(ScriptNameSpaceManagerMallocSizeOf)

NS_IMETHODIMP
nsScriptNameSpaceManager::CollectReports(
  nsIHandleReportCallback* aHandleReport, nsISupports* aData)
{
  return MOZ_COLLECT_REPORT(
    "explicit/script-namespace-manager", KIND_HEAP, UNITS_BYTES,
    SizeOfIncludingThis(ScriptNameSpaceManagerMallocSizeOf),
    "Memory used for the script namespace manager.");
}

size_t
nsScriptNameSpaceManager::SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf)
{
  size_t n = 0;
  n += PL_DHashTableSizeOfExcludingThis(&mGlobalNames,
         SizeOfEntryExcludingThis, aMallocSizeOf);
  n += PL_DHashTableSizeOfExcludingThis(&mNavigatorNames,
         SizeOfEntryExcludingThis, aMallocSizeOf);
  return n;
}
