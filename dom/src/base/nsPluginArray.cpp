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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
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

#include "nsPluginArray.h"
#include "nsMimeTypeArray.h"
#include "nsGlobalWindow.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDOMNavigator.h"
#include "nsIDOMMimeType.h"
#include "nsIServiceManager.h"
#include "nsIPluginHost.h"
#include "nsIDocShell.h"
#include "nsIWebNavigation.h"
#include "nsDOMClassInfo.h"

static NS_DEFINE_CID(kPluginManagerCID, NS_PLUGINMANAGER_CID);

PluginArrayImpl::PluginArrayImpl(NavigatorImpl* navigator,
                                 nsIDocShell *aDocShell)
{
  nsresult rv;
  NS_INIT_ISUPPORTS();
  mNavigator = navigator; // don't ADDREF here, needed for parent of script object.
  mPluginHost = do_GetService(kPluginManagerCID, &rv);
  mPluginCount = 0;
  mPluginArray = nsnull;
  mDocShell = aDocShell;
}

PluginArrayImpl::~PluginArrayImpl()
{
  if (mPluginArray != nsnull) {
    for (PRUint32 i = 0; i < mPluginCount; i++) {
      NS_IF_RELEASE(mPluginArray[i]);
    }
    delete[] mPluginArray;
  }
}

// QueryInterface implementation for PluginArrayImpl
NS_INTERFACE_MAP_BEGIN(PluginArrayImpl)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMPluginArray)
  NS_INTERFACE_MAP_ENTRY(nsIDOMPluginArray)
  NS_INTERFACE_MAP_ENTRY(nsIDOMJSPluginArray)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(PluginArray)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(PluginArrayImpl)
NS_IMPL_RELEASE(PluginArrayImpl)

NS_IMETHODIMP
PluginArrayImpl::GetLength(PRUint32* aLength)
{
  if (mPluginHost && NS_SUCCEEDED(mPluginHost->GetPluginCount(aLength)))
    return NS_OK;
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
PluginArrayImpl::Item(PRUint32 aIndex, nsIDOMPlugin** aReturn)
{
  NS_PRECONDITION(nsnull != aReturn, "null arg");

  if (mPluginArray == nsnull) {
    nsresult rv = GetPlugins();
    if (rv != NS_OK)
      return rv;
  }

  *aReturn = nsnull;

  if (aIndex < mPluginCount) {
    *aReturn = mPluginArray[aIndex];
    NS_IF_ADDREF(*aReturn);
  }

  return NS_OK;
}

NS_IMETHODIMP
PluginArrayImpl::NamedItem(const nsAReadableString& aName,
                           nsIDOMPlugin** aReturn)
{
  NS_PRECONDITION(nsnull != aReturn, "null arg");

  if (mPluginArray == nsnull) {
    nsresult rv = GetPlugins();
    if (rv != NS_OK)
      return rv;
  }

  *aReturn = nsnull;

  for (PRUint32 i = 0; i < mPluginCount; i++) {
    nsAutoString pluginName;
    nsIDOMPlugin* plugin = mPluginArray[i];
    if (plugin->GetName(pluginName) == NS_OK) {
      if (pluginName.Equals(aName)) {
        *aReturn = plugin;
        NS_IF_ADDREF(plugin);
        break;
      }
    }
  }

  return NS_OK;
}

nsresult
PluginArrayImpl::GetPluginHost(nsIPluginHost** aPluginHost)
{
  NS_ENSURE_ARG_POINTER(aPluginHost);

  nsresult rv = NS_OK;

  if (!mPluginHost) {
    mPluginHost = do_GetService(kPluginManagerCID, &rv);

    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  *aPluginHost = mPluginHost;
  NS_IF_ADDREF(*aPluginHost);

  return rv;
}

void
PluginArrayImpl::SetDocShell(nsIDocShell* aDocShell)
{
  mDocShell = aDocShell;
}

NS_IMETHODIMP
PluginArrayImpl::Refresh(PRBool aReloadDocuments)
{
  nsresult res = NS_OK;

  nsCOMPtr<nsIWebNavigation> webNav = do_QueryInterface(mDocShell);

  if(aReloadDocuments && webNav) {
    // we should take some measures to prevent recursive reload, 
    // check the URL and don't do anything if we just saw it.
    nsCOMPtr<nsIURI> uri;
    webNav->GetCurrentURI(getter_AddRefs(uri));
    if(uri) {
      PRBool sameURI = PR_FALSE;
      uri->Equals(mLastURI, &sameURI);
      if(sameURI) {
        mLastURI = nsnull;
        return res;
      }
    }
  }

  if (mPluginArray != nsnull) {
    for (PRUint32 i = 0; i < mPluginCount; i++) 
      NS_IF_RELEASE(mPluginArray[i]);

    delete[] mPluginArray;
  }

  mPluginCount = 0;
  mPluginArray = nsnull;

  if (!mPluginHost) {
    mPluginHost = do_GetService(kPluginManagerCID, &res);
  }

  if(NS_FAILED(res)) {
    return res;
  }

  nsCOMPtr<nsIPluginManager> pm(do_QueryInterface(mPluginHost));

  if(pm)
    pm->ReloadPlugins(aReloadDocuments);

  if (mNavigator)
    mNavigator->RefreshMIMEArray();
  
  if (aReloadDocuments && webNav) {
    webNav->GetCurrentURI(getter_AddRefs(mLastURI));
    webNav->Reload(nsIWebNavigation::LOAD_FLAGS_NONE);
  }

  return res;
}

NS_IMETHODIMP
PluginArrayImpl::Refresh()
{
  nsresult rv;
  nsCOMPtr<nsIXPConnect> xpc(do_GetService(nsIXPConnect::GetCID(), &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIXPCNativeCallContext> ncc;

  rv = xpc->GetCurrentNativeCallContext(getter_AddRefs(ncc));
  NS_ENSURE_SUCCESS(rv, rv);

  if (!ncc)
    return NS_ERROR_NOT_AVAILABLE;

  PRBool reload_doc = PR_FALSE;

  PRUint32 argc;

  ncc->GetArgc(&argc);

  if (argc > 0) {
    jsval *argv = nsnull;

    ncc->GetArgvPtr(&argv);
    NS_ENSURE_TRUE(argv, NS_ERROR_UNEXPECTED);

    JSContext *cx = nsnull;

    rv = ncc->GetJSContext(&cx);
    NS_ENSURE_SUCCESS(rv, rv);

    JS_ValueToBoolean(cx, argv[0], &reload_doc);
  }

  return Refresh(reload_doc);
}

nsresult
PluginArrayImpl::GetPlugins()
{
  nsresult rv = GetLength(&mPluginCount);
  if (rv == NS_OK) {
    mPluginArray = new nsIDOMPlugin*[mPluginCount];
    if (mPluginArray != nsnull) {
      rv = mPluginHost->GetPlugins(mPluginCount, mPluginArray);
      if (rv == NS_OK) {
        // need to wrap each of these with a PluginElementImpl, which is scriptable.
        for (PRUint32 i = 0; i < mPluginCount; i++) {
          nsIDOMPlugin* wrapper = new PluginElementImpl(mPluginArray[i]);
          NS_IF_ADDREF(wrapper);
          mPluginArray[i] = wrapper;
        }
      }
    } else {
      rv = NS_ERROR_OUT_OF_MEMORY;
    }
  }
  return rv;
}

//

PluginElementImpl::PluginElementImpl(nsIDOMPlugin* plugin)
{
  NS_INIT_ISUPPORTS();
  mPlugin = plugin;  // don't AddRef, see PluginArrayImpl::Item.
  mMimeTypeCount = 0;
  mMimeTypeArray = nsnull;
}

PluginElementImpl::~PluginElementImpl()
{
  NS_IF_RELEASE(mPlugin);

  if (mMimeTypeArray != nsnull) {
    for (PRUint32 i = 0; i < mMimeTypeCount; i++)
      NS_IF_RELEASE(mMimeTypeArray[i]);
    delete[] mMimeTypeArray;
  }
}


// QueryInterface implementation for PluginElementImpl
NS_INTERFACE_MAP_BEGIN(PluginElementImpl)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsIDOMPlugin)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(Plugin)
NS_INTERFACE_MAP_END


NS_IMPL_ADDREF(PluginElementImpl)
NS_IMPL_RELEASE(PluginElementImpl)


NS_IMETHODIMP
PluginElementImpl::GetDescription(nsAWritableString& aDescription)
{
  return mPlugin->GetDescription(aDescription);
}

NS_IMETHODIMP
PluginElementImpl::GetFilename(nsAWritableString& aFilename)
{
  return mPlugin->GetFilename(aFilename);
}

NS_IMETHODIMP
PluginElementImpl::GetName(nsAWritableString& aName)
{
  return mPlugin->GetName(aName);
}

NS_IMETHODIMP
PluginElementImpl::GetLength(PRUint32* aLength)
{
  return mPlugin->GetLength(aLength);
}

NS_IMETHODIMP
PluginElementImpl::Item(PRUint32 aIndex, nsIDOMMimeType** aReturn)
{
  if (mMimeTypeArray == nsnull) {
    nsresult rv = GetMimeTypes();
    if (rv != NS_OK)
      return rv;
  }
  if (aIndex < mMimeTypeCount) {
    nsIDOMMimeType* mimeType = mMimeTypeArray[aIndex];
    NS_IF_ADDREF(mimeType);
    *aReturn = mimeType;
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
PluginElementImpl::NamedItem(const nsAReadableString& aName,
                             nsIDOMMimeType** aReturn)
{
  if (mMimeTypeArray == nsnull) {
    nsresult rv = GetMimeTypes();
    if (rv != NS_OK)
      return rv;
  }

  *aReturn = nsnull;
  for (PRUint32 i = 0; i < mMimeTypeCount; i++) {
    nsAutoString type;
    nsIDOMMimeType* mimeType = mMimeTypeArray[i];
    if (mimeType->GetType(type) == NS_OK) {
      if (type.Equals(aName)) {
        *aReturn = mimeType;
        NS_ADDREF(mimeType);
        break;
      }
    }
  }

  return NS_OK;
}

nsresult
PluginElementImpl::GetMimeTypes()
{
  nsresult rv = mPlugin->GetLength(&mMimeTypeCount);
  if (rv == NS_OK) {
    mMimeTypeArray = new nsIDOMMimeType*[mMimeTypeCount];
    if (mMimeTypeArray == nsnull)
      return NS_ERROR_OUT_OF_MEMORY;
    for (PRUint32 i = 0; i < mMimeTypeCount; i++) {
      nsIDOMMimeType* mimeType = nsnull;
      rv = mPlugin->Item(i, &mimeType);
      if (rv != NS_OK)
        break;
      mimeType = new MimeTypeElementImpl(this, mimeType);
      NS_IF_ADDREF(mimeType);
      mMimeTypeArray[i] = mimeType;
    }
  }
  return rv;
}
