/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsScriptNameSpaceManager.h"
#include "nsCOMPtr.h"
#include "nsIComponentManager.h"
#include "nsICategoryManager.h"
#include "nsIServiceManager.h"
#include "nsISupportsPrimitives.h"
#include "nsIScriptExternalNameSet.h"
#include "nsIScriptNameSpaceManager.h"
#include "nsIInterfaceInfoManager.h"
#include "nsIInterfaceInfo.h"
#include "xptinfo.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"


#define NS_DOM_INTERFACE_PREFIX "nsIDOM"

nsScriptNameSpaceManager::nsScriptNameSpaceManager()
{
}

static PRBool PR_CALLBACK
NameStructCleanupCallback(nsHashKey *aKey, void *aData, void* closure)
{
  nsGlobalNameStruct *s = (nsGlobalNameStruct *)aData;

  delete s;

  return PR_TRUE;
}

nsScriptNameSpaceManager::~nsScriptNameSpaceManager()
{
  mGlobalNames.Reset(NameStructCleanupCallback);
}

nsresult
nsScriptNameSpaceManager::FillHash(nsICategoryManager *aCategoryManager,
                                   const char *aCategory,
                                   nsGlobalNameStruct::nametype aType)
{
  nsCOMPtr<nsISimpleEnumerator> e;
  nsresult rv = aCategoryManager->EnumerateCategory(aCategory,
                                                    getter_AddRefs(e));
  NS_ENSURE_SUCCESS(rv, rv);

  nsXPIDLCString categoryEntry;
  nsXPIDLCString contractId;
  nsCOMPtr<nsISupports> entry;

  while (NS_SUCCEEDED(e->GetNext(getter_AddRefs(entry)))) {
    nsCOMPtr<nsISupportsString> category(do_QueryInterface(entry));

    if (!category) {
      NS_WARNING("Category entry not an nsISupportsString!");

      continue;
    }

    rv = category->GetData(getter_Copies(categoryEntry));

    aCategoryManager->GetCategoryEntry(aCategory, categoryEntry,
                                       getter_Copies(contractId));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCID cid;

    rv = nsComponentManager::ContractIDToClassID(contractId, &cid);

    if (NS_FAILED(rv)) {
      NS_WARNING("Bad contract id registed with the script namespace manager");

      continue;
    }

    // XXX Mac chokes _later_ if we put the |NS_Conv...| right in
    // the |nsStringKey| ctor, so make a temp
    NS_ConvertASCIItoUCS2 temp(categoryEntry);

    nsStringKey key(temp);

    if (!mGlobalNames.Get(&key)) {
      nsGlobalNameStruct *s = new nsGlobalNameStruct;
      NS_ENSURE_TRUE(s, NS_ERROR_OUT_OF_MEMORY);

      s->mType = aType;
      s->mCID = cid;

      nsGlobalNameStruct *old =
        (nsGlobalNameStruct *)mGlobalNames.Put(&key, s);

      NS_WARN_IF_FALSE(!old, "Redefining name in global namespace hash!");

      delete old;
    } else {
      NS_WARNING("Global script name not overwritten!");
    }
  }

  return rv;
}


// This method enumerates over all installed interfaces (in .xpt
// files) and finds ones that start with "nsIDOM" and has constants
// defined in the interface itself (inherited constants doesn't
// count), once such an interface is found the "nsIDOM" prefix is cut
// off the name and the rest of the name is added into the hash for
// global names. This makes things like 'Node.ELEMENT_NODE' work in
// JS. See nsWindowSH::GlobalResolve() for detais on how this is used.

nsresult
nsScriptNameSpaceManager::FillHashWithDOMInterfaces()
{
  nsCOMPtr<nsIInterfaceInfoManager> iim =
    dont_AddRef(XPTI_GetInterfaceInfoManager());
  NS_ENSURE_TRUE(iim, NS_ERROR_UNEXPECTED);

  nsCOMPtr<nsIEnumerator> e;
  nsresult rv = iim->EnumerateInterfaces(getter_AddRefs(e));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsISupports> entry;

  rv = e->First();

  if (NS_FAILED(rv)) {
    // Empty interface list?

    NS_WARNING("What, no interfaces installed?");

    return NS_OK;
  }

  for ( ; e->IsDone() == NS_COMFALSE; e->Next()) {
    rv = e->CurrentItem(getter_AddRefs(entry));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIInterfaceInfo> if_info(do_QueryInterface(entry));

    NS_ASSERTION(if_info, "Interface info not an nsIInterfaceInfo!");

    // With the InterfaceInfo system it is actually cheaper to get the 
    // interface name than to get the count of constants. The former is 
    // always cached. The latter might require loading an xpt file!

    nsXPIDLCString if_name;

    rv = if_info->GetName(getter_Copies(if_name));
    NS_ENSURE_SUCCESS(rv, rv);

    if (nsCRT::strncmp(if_name.get(), NS_DOM_INTERFACE_PREFIX,
                       nsCRT::strlen(NS_DOM_INTERFACE_PREFIX))) {
      continue;
    }

    PRUint16 constant_count = 0;

    rv = if_info->GetConstantCount(&constant_count);
    NS_ENSURE_SUCCESS(rv, rv);

    if (constant_count) {
      PRUint16 parent_constant_count = 0;

      nsCOMPtr<nsIInterfaceInfo> parent_info;

      if_info->GetParent(getter_AddRefs(parent_info));

      if (parent_info) {
        rv = parent_info->GetConstantCount(&parent_constant_count);
        NS_ENSURE_SUCCESS(rv, rv);
      }

      if (constant_count != parent_constant_count) {
        nsGlobalNameStruct *s = new nsGlobalNameStruct;
        NS_ENSURE_TRUE(s, NS_ERROR_OUT_OF_MEMORY);

        s->mType = nsGlobalNameStruct::eTypeInterface;

        nsStringKey key(NS_ConvertASCIItoUCS2(if_name.get() + strlen(NS_DOM_INTERFACE_PREFIX)));

        nsGlobalNameStruct *old =
          (nsGlobalNameStruct *)mGlobalNames.Put(&key, s);

        delete old;
      }
    }
  }

  return rv;
}

nsresult
nsScriptNameSpaceManager::Init()
{
  nsresult rv = NS_OK;

  rv = FillHashWithDOMInterfaces();
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsICategoryManager> cm =
    do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = FillHash(cm, JAVASCRIPT_GLOBAL_CONSTRUCTOR_CATEGORY,
                nsGlobalNameStruct::eTypeExternalConstructor);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = FillHash(cm, JAVASCRIPT_GLOBAL_PROPERTY_CATEGORY,
                nsGlobalNameStruct::eTypeProperty);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = FillHash(cm, JAVASCRIPT_GLOBAL_STATIC_NAMESET_CATEGORY,
                nsGlobalNameStruct::eTypeStaticNameSet);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = FillHash(cm, JAVASCRIPT_GLOBAL_DYNAMIC_NAMESET_CATEGORY,
                nsGlobalNameStruct::eTypeDynamicNameSet);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

static PRBool PR_CALLBACK
NameSetInitCallback(nsHashKey *aKey, void *aData, void* closure)
{
  nsGlobalNameStruct *s = (nsGlobalNameStruct *)aData;

  if (s->mType != nsGlobalNameStruct::eTypeStaticNameSet) {
    return PR_TRUE;
  }

  nsresult rv = NS_OK;
  nsCOMPtr<nsIScriptExternalNameSet> ns(do_CreateInstance(s->mCID, &rv));
  NS_ENSURE_SUCCESS(rv, PR_TRUE);

  rv = ns->InitializeNameSet((nsIScriptContext *)closure);
  NS_WARN_IF_FALSE(NS_SUCCEEDED(rv),
                   "Initing external script classes failed!");

  return PR_TRUE;
}

nsresult
nsScriptNameSpaceManager::InitForContext(nsIScriptContext *aContext)
{
  mGlobalNames.Enumerate(NameSetInitCallback, aContext);

  return NS_OK;
}

nsresult
nsScriptNameSpaceManager::LookupName(const nsAReadableString& aName,
                                     const nsGlobalNameStruct **aNameStruct)
{
  nsStringKey key(aName);

  *aNameStruct = (const nsGlobalNameStruct *)mGlobalNames.Get(&key);

  return NS_OK;
}

nsresult
nsScriptNameSpaceManager::RegisterClassName(const char *aClassName,
                                            PRInt32 aDOMClassInfoID)
{
  nsAutoString name;
  CopyASCIItoUCS2(nsDependentCString(aClassName), name);

  nsStringKey key(name);

  nsGlobalNameStruct *s = (nsGlobalNameStruct *)mGlobalNames.Get(&key);

  if (s && s->mType == nsGlobalNameStruct::eTypeClassConstructor) {
    return NS_OK;
  }

  // If a external constructor is already defined with aClassName we
  // won't overwrite it.

  if (s && s->mType == nsGlobalNameStruct::eTypeExternalConstructor) {
    return NS_OK;
  }

  NS_ASSERTION(!(s && s->mType != nsGlobalNameStruct::eTypeInterface),
               "Whaa, JS environment name clash!");

  if (!s) {
    s = new nsGlobalNameStruct;
    NS_ENSURE_TRUE(s, NS_ERROR_OUT_OF_MEMORY);
  }
  // else reuse the one already found in the hash

  s->mType = nsGlobalNameStruct::eTypeClassConstructor;
  s->mDOMClassInfoID = aDOMClassInfoID;

  mGlobalNames.Put(&key, s);

  return NS_OK;
}

nsresult
nsScriptNameSpaceManager::RegisterClassProto(const char *aClassName,
                                             const nsIID *aConstructorProtoIID,
                                             PRBool *aFoundOld)
{
  NS_ENSURE_ARG_POINTER(aConstructorProtoIID);

  *aFoundOld = PR_FALSE;

  nsAutoString name;
  CopyASCIItoUCS2(nsDependentCString(aClassName), name);

  nsStringKey key(name);

  nsGlobalNameStruct *s = (nsGlobalNameStruct *)mGlobalNames.Get(&key);

  if (s && s->mType != nsGlobalNameStruct::eTypeInterface) {
    *aFoundOld = PR_TRUE;

    return NS_OK;
  }

  if (!s) {
    s = new nsGlobalNameStruct;
    NS_ENSURE_TRUE(s, NS_ERROR_OUT_OF_MEMORY);
  }

  s->mType = nsGlobalNameStruct::eTypeClassProto;
  s->mIID = *aConstructorProtoIID;

  mGlobalNames.Put(&key, s);

  return NS_OK;
}
