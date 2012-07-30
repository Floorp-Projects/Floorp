/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPluginArray.h"
#include "nsMimeTypeArray.h"
#include "Navigator.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDOMNavigator.h"
#include "nsIDOMMimeType.h"
#include "nsIPluginHost.h"
#include "nsIDocShell.h"
#include "nsIWebNavigation.h"
#include "nsDOMClassInfoID.h"
#include "nsPluginError.h"
#include "nsPluginHost.h"

using namespace mozilla;
using namespace mozilla::dom;

nsPluginArray::nsPluginArray(Navigator* navigator,
                             nsIDocShell *aDocShell)
  : mNavigator(navigator),
    mPluginHost(do_GetService(MOZ_PLUGIN_HOST_CONTRACTID)),
    mPluginCount(0),
    mPluginArray(nullptr),
    mDocShell(do_GetWeakReference(aDocShell))
{
}

nsPluginArray::~nsPluginArray()
{
  if (mPluginArray != nullptr) {
    for (PRUint32 i = 0; i < mPluginCount; i++) {
      NS_IF_RELEASE(mPluginArray[i]);
    }
    delete[] mPluginArray;
  }
}

DOMCI_DATA(PluginArray, nsPluginArray)

// QueryInterface implementation for nsPluginArray
NS_INTERFACE_MAP_BEGIN(nsPluginArray)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMPluginArray)
  NS_INTERFACE_MAP_ENTRY(nsIDOMPluginArray)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(PluginArray)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(nsPluginArray)
NS_IMPL_RELEASE(nsPluginArray)

NS_IMETHODIMP
nsPluginArray::GetLength(PRUint32* aLength)
{
  nsPluginHost *pluginHost = static_cast<nsPluginHost*>(mPluginHost.get());
  if (AllowPlugins() && pluginHost)
    return pluginHost->GetPluginCount(aLength);
  
  *aLength = 0;
  return NS_OK;
}

bool
nsPluginArray::AllowPlugins()
{
  bool allowPlugins = false;
  nsCOMPtr<nsIDocShell> docShell = do_QueryReferent(mDocShell);

  if (docShell)
    if (NS_FAILED(docShell->GetAllowPlugins(&allowPlugins)))
      allowPlugins = false;

  return allowPlugins;
}

nsIDOMPlugin*
nsPluginArray::GetItemAt(PRUint32 aIndex, nsresult* aResult)
{
  *aResult = NS_OK;

  if (!AllowPlugins())
    return nullptr;

  if (mPluginArray == nullptr) {
    *aResult = GetPlugins();
    if (*aResult != NS_OK)
      return nullptr;
  }

  return aIndex < mPluginCount ? mPluginArray[aIndex] : nullptr;
}

NS_IMETHODIMP
nsPluginArray::Item(PRUint32 aIndex, nsIDOMPlugin** aReturn)
{
  nsresult rv;

  NS_IF_ADDREF(*aReturn = GetItemAt(aIndex, &rv));

  return rv;
}

nsIDOMPlugin*
nsPluginArray::GetNamedItem(const nsAString& aName, nsresult* aResult)
{
  *aResult = NS_OK;

  if (!AllowPlugins())
    return nullptr;

  if (mPluginArray == nullptr) {
    *aResult = GetPlugins();
    if (*aResult != NS_OK)
      return nullptr;
  }

  for (PRUint32 i = 0; i < mPluginCount; i++) {
    nsAutoString pluginName;
    nsIDOMPlugin* plugin = mPluginArray[i];
    if (plugin->GetName(pluginName) == NS_OK && pluginName.Equals(aName)) {
      return plugin;
    }
  }

  return nullptr;
}

NS_IMETHODIMP
nsPluginArray::NamedItem(const nsAString& aName, nsIDOMPlugin** aReturn)
{
  NS_PRECONDITION(nullptr != aReturn, "null arg");

  nsresult rv;

  NS_IF_ADDREF(*aReturn = GetNamedItem(aName, &rv));

  return rv;
}

nsresult
nsPluginArray::GetPluginHost(nsIPluginHost** aPluginHost)
{
  NS_ENSURE_ARG_POINTER(aPluginHost);

  nsresult rv = NS_OK;

  if (!mPluginHost) {
    mPluginHost = do_GetService(MOZ_PLUGIN_HOST_CONTRACTID, &rv);

    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  *aPluginHost = mPluginHost;
  NS_IF_ADDREF(*aPluginHost);

  return rv;
}

void
nsPluginArray::Invalidate()
{
  mDocShell = nullptr;
  mNavigator = nullptr;
}

NS_IMETHODIMP
nsPluginArray::Refresh(bool aReloadDocuments)
{
  nsresult res = NS_OK;
  if (!AllowPlugins())
    return NS_SUCCESS_LOSS_OF_INSIGNIFICANT_DATA;

  if (!mPluginHost) {
    mPluginHost = do_GetService(MOZ_PLUGIN_HOST_CONTRACTID, &res);
  }

  if(NS_FAILED(res)) {
    return res;
  }

  // NS_ERROR_PLUGINS_PLUGINSNOTCHANGED on reloading plugins indicates
  // that plugins did not change and was not reloaded
  bool pluginsNotChanged = false;
  if(mPluginHost)
    pluginsNotChanged = (NS_ERROR_PLUGINS_PLUGINSNOTCHANGED == mPluginHost->ReloadPlugins(aReloadDocuments));

  // no need to reload the page if plugins have not been changed
  // in fact, if we do reload we can hit recursive load problem, see bug 93351
  if(pluginsNotChanged)
    return res;

  nsCOMPtr<nsIWebNavigation> webNav = do_QueryReferent(mDocShell);

  if (mPluginArray != nullptr) {
    for (PRUint32 i = 0; i < mPluginCount; i++) 
      NS_IF_RELEASE(mPluginArray[i]);

    delete[] mPluginArray;
  }

  mPluginCount = 0;
  mPluginArray = nullptr;

  if (mNavigator)
    mNavigator->RefreshMIMEArray();
  
  if (aReloadDocuments && webNav)
    webNav->Reload(nsIWebNavigation::LOAD_FLAGS_NONE);

  return res;
}

nsresult
nsPluginArray::GetPlugins()
{
  nsresult rv = GetLength(&mPluginCount);
  if (NS_SUCCEEDED(rv)) {
    mPluginArray = new nsIDOMPlugin*[mPluginCount];
    if (!mPluginArray)
      return NS_ERROR_OUT_OF_MEMORY;

    if (!mPluginCount)
      return NS_OK;

    nsPluginHost *pluginHost = static_cast<nsPluginHost*>(mPluginHost.get());
    rv = pluginHost->GetPlugins(mPluginCount, mPluginArray);
    if (NS_SUCCEEDED(rv)) {
      // need to wrap each of these with a nsPluginElement, which
      // is scriptable.
      for (PRUint32 i = 0; i < mPluginCount; i++) {
        nsIDOMPlugin* wrapper = new nsPluginElement(mPluginArray[i]);
        NS_IF_ADDREF(wrapper);
        mPluginArray[i] = wrapper;
      }
    } else {
      /* XXX this code is all broken. If GetPlugins fails, there's no contract
       *     explaining what should happen. Instead of deleting elements in an
       *     array of random pointers, we mark the array as 0 length.
       */
      mPluginCount = 0;
    }
  }
  return rv;
}

//

nsPluginElement::nsPluginElement(nsIDOMPlugin* plugin)
{
  mPlugin = plugin;  // don't AddRef, see nsPluginArray::Item.
  mMimeTypeCount = 0;
  mMimeTypeArray = nullptr;
}

nsPluginElement::~nsPluginElement()
{
  NS_IF_RELEASE(mPlugin);

  if (mMimeTypeArray != nullptr) {
    for (PRUint32 i = 0; i < mMimeTypeCount; i++) {
      nsMimeType* mt = static_cast<nsMimeType*>(mMimeTypeArray[i]);
      if (mt) {
        mt->DetachPlugin();
        NS_RELEASE(mt);
      }
    }
    delete[] mMimeTypeArray;
  }
}


DOMCI_DATA(Plugin, nsPluginElement)

// QueryInterface implementation for nsPluginElement
NS_INTERFACE_MAP_BEGIN(nsPluginElement)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsIDOMPlugin)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(Plugin)
NS_INTERFACE_MAP_END


NS_IMPL_ADDREF(nsPluginElement)
NS_IMPL_RELEASE(nsPluginElement)


NS_IMETHODIMP
nsPluginElement::GetDescription(nsAString& aDescription)
{
  return mPlugin->GetDescription(aDescription);
}

NS_IMETHODIMP
nsPluginElement::GetFilename(nsAString& aFilename)
{
  return mPlugin->GetFilename(aFilename);
}

NS_IMETHODIMP
nsPluginElement::GetVersion(nsAString& aVersion)
{
  return mPlugin->GetVersion(aVersion);
}

NS_IMETHODIMP
nsPluginElement::GetName(nsAString& aName)
{
  return mPlugin->GetName(aName);
}

NS_IMETHODIMP
nsPluginElement::GetLength(PRUint32* aLength)
{
  return mPlugin->GetLength(aLength);
}

nsIDOMMimeType*
nsPluginElement::GetItemAt(PRUint32 aIndex, nsresult *aResult)
{
  if (mMimeTypeArray == nullptr) {
    *aResult = GetMimeTypes();
    if (*aResult != NS_OK)
      return nullptr;
  }

  if (aIndex >= mMimeTypeCount) {
    *aResult = NS_ERROR_FAILURE;

    return nullptr;
  }

  *aResult = NS_OK;

  return mMimeTypeArray[aIndex];
}

NS_IMETHODIMP
nsPluginElement::Item(PRUint32 aIndex, nsIDOMMimeType** aReturn)
{
  nsresult rv;

  NS_IF_ADDREF(*aReturn = GetItemAt(aIndex, &rv));

  return rv;
}

nsIDOMMimeType*
nsPluginElement::GetNamedItem(const nsAString& aName, nsresult *aResult)
{
  if (mMimeTypeArray == nullptr) {
    *aResult = GetMimeTypes();
    if (*aResult != NS_OK)
      return nullptr;
  }

  *aResult = NS_OK;
  for (PRUint32 i = 0; i < mMimeTypeCount; i++) {
    nsAutoString type;
    nsIDOMMimeType* mimeType = mMimeTypeArray[i];
    if (mimeType->GetType(type) == NS_OK && type.Equals(aName)) {
      return mimeType;
    }
  }

  return nullptr;
}

NS_IMETHODIMP
nsPluginElement::NamedItem(const nsAString& aName, nsIDOMMimeType** aReturn)
{
  nsresult rv;

  NS_IF_ADDREF(*aReturn = GetNamedItem(aName, &rv));

  return rv;
}

nsresult
nsPluginElement::GetMimeTypes()
{
  nsresult rv = mPlugin->GetLength(&mMimeTypeCount);
  if (rv == NS_OK) {
    mMimeTypeArray = new nsIDOMMimeType*[mMimeTypeCount];
    if (mMimeTypeArray == nullptr)
      return NS_ERROR_OUT_OF_MEMORY;
    for (PRUint32 i = 0; i < mMimeTypeCount; i++) {
      nsCOMPtr<nsIDOMMimeType> mimeType;
      rv = mPlugin->Item(i, getter_AddRefs(mimeType));
      if (rv != NS_OK)
        break;
      mimeType = new nsMimeType(this, mimeType);
      NS_IF_ADDREF(mMimeTypeArray[i] = mimeType);
    }
  }
  return rv;
}
