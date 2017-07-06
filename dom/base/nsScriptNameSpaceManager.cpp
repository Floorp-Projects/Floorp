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
#include "nsIScriptNameSpaceManager.h"
#include "nsIScriptContext.h"
#include "nsIInterfaceInfoManager.h"
#include "nsIInterfaceInfo.h"
#include "xptinfo.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsHashKeys.h"
#include "nsDOMClassInfo.h"
#include "nsCRT.h"
#include "nsIObserverService.h"
#include "nsISimpleEnumerator.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/WebIDLGlobalNameHash.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/Preferences.h"
#include "mozilla/Services.h"

#define NS_INTERFACE_PREFIX "nsI"
#define NS_DOM_INTERFACE_PREFIX "nsIDOM"

using namespace mozilla;
using namespace mozilla::dom;

static PLDHashNumber
GlobalNameHashHashKey(const void *key)
{
  const nsAString *str = static_cast<const nsAString *>(key);
  return HashString(*str);
}

static bool
GlobalNameHashMatchEntry(const PLDHashEntryHdr *entry, const void *key)
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

  // This will set e->mGlobalName.mType to
  // nsGlobalNameStruct::eTypeNotInitialized
  memset(&e->mGlobalName, 0, sizeof(nsGlobalNameStruct));
}

static void
GlobalNameHashInitEntry(PLDHashEntryHdr *entry, const void *key)
{
  GlobalNameMapEntry *e = static_cast<GlobalNameMapEntry *>(entry);
  const nsAString *keyStr = static_cast<const nsAString *>(key);

  // Initialize the key in the entry with placement new
  new (&e->mKey) nsString(*keyStr);

  // This will set e->mGlobalName.mType to
  // nsGlobalNameStruct::eTypeNotInitialized
  memset(&e->mGlobalName, 0, sizeof(nsGlobalNameStruct));
}

NS_IMPL_ISUPPORTS(
  nsScriptNameSpaceManager,
  nsIObserver,
  nsISupportsWeakReference,
  nsIMemoryReporter)

static const PLDHashTableOps hash_table_ops =
{
  GlobalNameHashHashKey,
  GlobalNameHashMatchEntry,
  PLDHashTable::MoveEntryStub,
  GlobalNameHashClearEntry,
  GlobalNameHashInitEntry
};

#define GLOBALNAME_HASHTABLE_INITIAL_LENGTH          32

nsScriptNameSpaceManager::nsScriptNameSpaceManager()
  : mGlobalNames(&hash_table_ops, sizeof(GlobalNameMapEntry),
                 GLOBALNAME_HASHTABLE_INITIAL_LENGTH)
{
}

nsScriptNameSpaceManager::~nsScriptNameSpaceManager()
{
  UnregisterWeakMemoryReporter(this);
}

nsGlobalNameStruct *
nsScriptNameSpaceManager::AddToHash(const char *aKey,
                                    const char16_t **aClassName)
{
  NS_ConvertASCIItoUTF16 key(aKey);
  auto entry = static_cast<GlobalNameMapEntry*>(mGlobalNames.Add(&key, fallible));
  if (!entry) {
    return nullptr;
  }

  WebIDLGlobalNameHash::Remove(aKey, key.Length());

  if (aClassName) {
    *aClassName = entry->mKey.get();
  }

  return &entry->mGlobalName;
}

void
nsScriptNameSpaceManager::RemoveFromHash(const nsAString *aKey)
{
  mGlobalNames.Remove(aKey);
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
nsScriptNameSpaceManager::Init()
{
  RegisterWeakMemoryReporter(this);

  nsresult rv = NS_OK;

  nsCOMPtr<nsICategoryManager> cm =
    do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = FillHash(cm, JAVASCRIPT_GLOBAL_CONSTRUCTOR_CATEGORY);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = FillHash(cm, JAVASCRIPT_GLOBAL_PROPERTY_CATEGORY);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = FillHash(cm, JAVASCRIPT_GLOBAL_PRIVILEGED_PROPERTY_CATEGORY);
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

const nsGlobalNameStruct*
nsScriptNameSpaceManager::LookupName(const nsAString& aName,
                                     const char16_t **aClassName)
{
  auto entry = static_cast<GlobalNameMapEntry*>(mGlobalNames.Search(&aName));

  if (entry) {
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

nsresult
nsScriptNameSpaceManager::RegisterClassName(const char *aClassName,
                                            int32_t aDOMClassInfoID,
                                            bool aPrivileged,
                                            bool aXBLAllowed,
                                            const char16_t **aResult)
{
  if (!nsCRT::IsAscii(aClassName)) {
    NS_ERROR("Trying to register a non-ASCII class name");
    return NS_OK;
  }
  nsGlobalNameStruct *s = AddToHash(aClassName, aResult);
  NS_ENSURE_TRUE(s, NS_ERROR_OUT_OF_MEMORY);

  if (s->mType == nsGlobalNameStruct::eTypeClassConstructor) {
    return NS_OK;
  }

  // If a external constructor is already defined with aClassName we
  // won't overwrite it.

  if (s->mType == nsGlobalNameStruct::eTypeExternalConstructor) {
    return NS_OK;
  }

  NS_ASSERTION(s->mType == nsGlobalNameStruct::eTypeNotInitialized,
               "Whaaa, JS environment name clash!");

  s->mType = nsGlobalNameStruct::eTypeClassConstructor;
  s->mDOMClassInfoID = aDOMClassInfoID;
  s->mChromeOnly = aPrivileged;
  s->mAllowXBL = aXBLAllowed;

  return NS_OK;
}

nsresult
nsScriptNameSpaceManager::RegisterClassProto(const char *aClassName,
                                             const nsIID *aConstructorProtoIID,
                                             bool *aFoundOld)
{
  NS_ENSURE_ARG_POINTER(aConstructorProtoIID);

  *aFoundOld = false;

  nsGlobalNameStruct *s = AddToHash(aClassName);
  NS_ENSURE_TRUE(s, NS_ERROR_OUT_OF_MEMORY);

  if (s->mType != nsGlobalNameStruct::eTypeNotInitialized) {
    *aFoundOld = true;

    return NS_OK;
  }

  s->mType = nsGlobalNameStruct::eTypeClassProto;
  s->mIID = *aConstructorProtoIID;

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

  // We need to handle removal before calling GetCategoryEntry
  // because the category entry is already removed before we are
  // notified.
  if (aRemove) {
    NS_ConvertASCIItoUTF16 entry(categoryEntry);
    const nsGlobalNameStruct *s = LookupName(entry);
    // Verify mType so that this API doesn't remove names
    // registered by others.
    if (!s || s->mType != type) {
      return NS_OK;
    }

    RemoveFromHash(&entry);
    return NS_OK;
  }

  nsXPIDLCString contractId;
  rv = aCategoryManager->GetCategoryEntry(aCategory, categoryEntry.get(),
                                          getter_Copies(contractId));
  NS_ENSURE_SUCCESS(rv, rv);

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
  free(cidPtr);

  nsGlobalNameStruct *s = AddToHash(categoryEntry.get());
  NS_ENSURE_TRUE(s, NS_ERROR_OUT_OF_MEMORY);

  if (s->mType == nsGlobalNameStruct::eTypeNotInitialized) {
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

MOZ_DEFINE_MALLOC_SIZE_OF(ScriptNameSpaceManagerMallocSizeOf)

NS_IMETHODIMP
nsScriptNameSpaceManager::CollectReports(
  nsIHandleReportCallback* aHandleReport, nsISupports* aData, bool aAnonymize)
{
  MOZ_COLLECT_REPORT(
    "explicit/script-namespace-manager", KIND_HEAP, UNITS_BYTES,
    SizeOfIncludingThis(ScriptNameSpaceManagerMallocSizeOf),
    "Memory used for the script namespace manager.");

  return NS_OK;
}

size_t
nsScriptNameSpaceManager::SizeOfIncludingThis(
    mozilla::MallocSizeOf aMallocSizeOf) const
{
  size_t n = 0;

  n += mGlobalNames.ShallowSizeOfExcludingThis(aMallocSizeOf);
  for (auto iter = mGlobalNames.ConstIter(); !iter.Done(); iter.Next()) {
    auto entry = static_cast<GlobalNameMapEntry*>(iter.Get());
    n += entry->SizeOfExcludingThis(aMallocSizeOf);
  }

  return n;
}
