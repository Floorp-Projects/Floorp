/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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
#include "nsReadableUtils.h"
#include "nsHashKeys.h"
#include "nsDOMClassInfo.h"
#include "nsCRT.h"
#include "nsIObserverService.h"
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

NS_IMPL_ISUPPORTS2(nsScriptNameSpaceManager,
                   nsIObserver,
                   nsISupportsWeakReference)

nsScriptNameSpaceManager::nsScriptNameSpaceManager()
  : mIsInitialized(false)
{
  MOZ_COUNT_CTOR(nsScriptNameSpaceManager);
}

nsScriptNameSpaceManager::~nsScriptNameSpaceManager()
{
  if (mIsInitialized) {
    // Destroy the hash
    PL_DHashTableFinish(&mGlobalNames);
    PL_DHashTableFinish(&mNavigatorNames);
  }
  MOZ_COUNT_DTOR(nsScriptNameSpaceManager);
}

nsGlobalNameStruct *
nsScriptNameSpaceManager::AddToHash(PLDHashTable *aTable, const char *aKey,
                                    const PRUnichar **aClassName)
{
  NS_ConvertASCIItoUTF16 key(aKey);
  GlobalNameMapEntry *entry =
    static_cast<GlobalNameMapEntry *>
               (PL_DHashTableOperate(aTable, &key, PL_DHASH_ADD));

  if (!entry) {
    return nsnull;
  }

  if (aClassName) {
    *aClassName = entry->mKey.get();
  }

  return &entry->mGlobalName;
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


// This method enumerates over all installed interfaces (in .xpt
// files) and finds ones that start with "nsIDOM" and has constants
// defined in the interface itself (inherited constants doesn't
// count), once such an interface is found the "nsIDOM" prefix is cut
// off the name and the rest of the name is added into the hash for
// global names. This makes things like 'Node.ELEMENT_NODE' work in
// JS. See nsCommonWindowSH::GlobalResolve() for detais on how this is used.

nsresult
nsScriptNameSpaceManager::FillHashWithDOMInterfaces()
{
  nsCOMPtr<nsIInterfaceInfoManager>
    iim(do_GetService(NS_INTERFACEINFOMANAGER_SERVICE_CONTRACTID));
  NS_ENSURE_TRUE(iim, NS_ERROR_UNEXPECTED);

  // First look for all interfaces whose name starts with nsIDOM
  nsCOMPtr<nsIEnumerator> domInterfaces;
  nsresult rv =
    iim->EnumerateInterfacesWhoseNamesStartWith(NS_DOM_INTERFACE_PREFIX,
                                                getter_AddRefs(domInterfaces));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsISupports> entry;

  rv = domInterfaces->First();

  if (NS_FAILED(rv)) {
    // Empty interface list?

    NS_WARNING("What, no nsIDOM interfaces installed?");

    return NS_OK;
  }

  bool found_old;
  nsCOMPtr<nsIInterfaceInfo> if_info;
  const char *if_name = nsnull;
  const nsIID *iid;

  for ( ; domInterfaces->IsDone() == NS_ENUMERATOR_FALSE; domInterfaces->Next()) {
    rv = domInterfaces->CurrentItem(getter_AddRefs(entry));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIInterfaceInfo> if_info(do_QueryInterface(entry));
    if_info->GetNameShared(&if_name);
    if_info->GetIIDShared(&iid);
    rv = RegisterInterface(if_name + sizeof(NS_DOM_INTERFACE_PREFIX) - 1,
                           iid, &found_old);

#ifdef DEBUG
    NS_ASSERTION(!found_old,
                 "Whaaa, interface name already in hash!");
#endif
  }

  // Next, look for externally registered DOM interfaces
  rv = RegisterExternalInterfaces(false);

  return rv;
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
  nsCAutoString category_entry;
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
        if (!aAsProto) {
          // nsIDOM* interfaces have already been registered.
          break;
        }
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

  if (s->mType != nsGlobalNameStruct::eTypeNotInitialized) {
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
  static PLDHashTableOps hash_table_ops =
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

  mIsInitialized = PL_DHashTableInit(&mGlobalNames, &hash_table_ops, nsnull,
                                     sizeof(GlobalNameMapEntry), 
                                     GLOBALNAME_HASHTABLE_INITIAL_SIZE);
  NS_ENSURE_TRUE(mIsInitialized, NS_ERROR_OUT_OF_MEMORY);

  mIsInitialized = PL_DHashTableInit(&mNavigatorNames, &hash_table_ops, nsnull,
                                     sizeof(GlobalNameMapEntry), 
                                     GLOBALNAME_HASHTABLE_INITIAL_SIZE);
  if (!mIsInitialized) {
    PL_DHashTableFinish(&mGlobalNames);

    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsresult rv = NS_OK;

  rv = FillHashWithDOMInterfaces();
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

  rv = FillHash(cm, JAVASCRIPT_GLOBAL_DYNAMIC_NAMESET_CATEGORY);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = FillHash(cm, JAVASCRIPT_NAVIGATOR_PROPERTY_CATEGORY);
  NS_ENSURE_SUCCESS(rv, rv);

  // Initial filling of the has table has been done.
  // Now, listen for changes.
  nsCOMPtr<nsIObserverService> serv = 
    mozilla::services::GetObserverService();

  if (serv) {
    serv->AddObserver(this, NS_XPCOM_CATEGORY_ENTRY_ADDED_OBSERVER_ID, true);
  }

  return NS_OK;
}

struct NameSetClosure {
  nsIScriptContext* ctx;
  nsresult rv;
};

static PLDHashOperator
NameSetInitCallback(PLDHashTable *table, PLDHashEntryHdr *hdr,
                    PRUint32 number, void *arg)
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
                                             const PRUnichar **aClassName)
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
    *aClassName = nsnull;
  }
  return nsnull;
}

nsresult
nsScriptNameSpaceManager::LookupNavigatorName(const nsAString& aName,
                                              const nsGlobalNameStruct **aNameStruct)
{
  GlobalNameMapEntry *entry =
    static_cast<GlobalNameMapEntry *>
               (PL_DHashTableOperate(&mNavigatorNames, &aName,
                                     PL_DHASH_LOOKUP));

  if (PL_DHASH_ENTRY_IS_BUSY(entry) &&
      !((&entry->mGlobalName)->mDisabled)) {
    *aNameStruct = &entry->mGlobalName;
  } else {
    *aNameStruct = nsnull;
  }

  return NS_OK;
}

nsresult
nsScriptNameSpaceManager::RegisterClassName(const char *aClassName,
                                            PRInt32 aDOMClassInfoID,
                                            bool aPrivileged,
                                            bool aDisabled,
                                            const PRUnichar **aResult)
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
               s->mType == nsGlobalNameStruct::eTypeInterface,
               "Whaaa, JS environment name clash!");

  s->mType = nsGlobalNameStruct::eTypeClassConstructor;
  s->mDOMClassInfoID = aDOMClassInfoID;
  s->mChromeOnly = aPrivileged;
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
                                            PRUint32 aScriptableFlags,
                                            bool aHasClassInterface,
                                            const nsCID *aConstructorCID)
{
  const PRUnichar* className;
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
    s->mData->u.mExternalConstructorFptr = nsnull;
  s->mData->mCachedClassInfo = nsnull;
  s->mData->mProtoChainInterface = aProtoChainInterface;
  s->mData->mInterfaces = aInterfaces;
  s->mData->mScriptableFlags = aScriptableFlags;
  s->mData->mHasClassInterface = aHasClassInterface;
  s->mData->mConstructorCID = aConstructorCID;

  return NS_OK;
}

nsresult
nsScriptNameSpaceManager::AddCategoryEntryToHash(nsICategoryManager* aCategoryManager,
                                                 const char* aCategory,
                                                 nsISupports* aEntry)
{
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
  } else if (strcmp(aCategory, JAVASCRIPT_GLOBAL_DYNAMIC_NAMESET_CATEGORY) == 0) {
    type = nsGlobalNameStruct::eTypeDynamicNameSet;
  } else {
    return NS_OK;
  }

  nsCOMPtr<nsISupportsCString> strWrapper = do_QueryInterface(aEntry);

  if (!strWrapper) {
    NS_WARNING("Category entry not an nsISupportsCString!");
    return NS_OK;
  }

  nsCAutoString categoryEntry;
  nsresult rv = strWrapper->GetData(categoryEntry);
  NS_ENSURE_SUCCESS(rv, rv);

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
  nsMemory::Free(cidPtr);

  if (type == nsGlobalNameStruct::eTypeExternalConstructor) {
    nsXPIDLCString constructorProto;
    rv = aCategoryManager->GetCategoryEntry(JAVASCRIPT_GLOBAL_CONSTRUCTOR_PROTO_ALIAS_CATEGORY,
                                            categoryEntry.get(),
                                            getter_Copies(constructorProto));
    if (NS_SUCCEEDED(rv)) {
      nsGlobalNameStruct *s = AddToHash(&mGlobalNames, categoryEntry.get());
      NS_ENSURE_TRUE(s, NS_ERROR_OUT_OF_MEMORY);

      if (s->mType == nsGlobalNameStruct::eTypeNotInitialized) {
        s->mAlias = new nsGlobalNameStruct::ConstructorAlias;
        s->mType = nsGlobalNameStruct::eTypeExternalConstructorAlias;
        s->mChromeOnly = false;
        s->mAlias->mCID = cid;
        AppendASCIItoUTF16(constructorProto, s->mAlias->mProtoName);
        s->mAlias->mProto = nsnull;
      } else {
        NS_WARNING("Global script name not overwritten!");
      }

      return NS_OK;
    }
  }

  PLDHashTable *table;
  if (type == nsGlobalNameStruct::eTypeNavigatorProperty) {
    table = &mNavigatorNames;
  } else {
    table = &mGlobalNames;
  }

  nsGlobalNameStruct *s = AddToHash(table, categoryEntry.get());
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

NS_IMETHODIMP
nsScriptNameSpaceManager::Observe(nsISupports* aSubject, const char* aTopic,
                                  const PRUnichar* aData)
{
  if (!aData) {
    return NS_OK;
  }

  if (strcmp(aTopic, NS_XPCOM_CATEGORY_ENTRY_ADDED_OBSERVER_ID) == 0) {
    nsCOMPtr<nsICategoryManager> cm =
      do_GetService(NS_CATEGORYMANAGER_CONTRACTID);
    if (!cm) {
      return NS_OK;
    }

    return AddCategoryEntryToHash(cm, NS_ConvertUTF16toUTF8(aData).get(),
                                  aSubject);
  }

  // TODO: we could observe NS_XPCOM_CATEGORY_ENTRY_REMOVED_OBSERVER_ID
  // and NS_XPCOM_CATEGORY_CLEARED_OBSERVER_ID but we are safe without it.
  // See bug 600460.

  return NS_OK;
}

void
nsScriptNameSpaceManager::RegisterDefineDOMInterface(const nsAString& aName,
    mozilla::dom::binding::DefineInterface aDefineDOMInterface)
{
  nsGlobalNameStruct* s = LookupNameInternal(aName);
  if (s) {
    s->mDefineDOMInterface = aDefineDOMInterface;
  }
}
