/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsPluginArray.h"
#include "nsMimeTypeArray.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDOMNavigator.h"
#include "nsIDOMMimeType.h"
#include "nsIServiceManager.h"
#include "nsIPluginHost.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_CID(kPluginManagerCID, NS_PLUGINMANAGER_CID);

PluginArrayImpl::PluginArrayImpl(nsIDOMNavigator* navigator)
{
	NS_INIT_ISUPPORTS();
	mScriptObject = nsnull;
	mNavigator = navigator;		// don't ADDREF here, needed for parent of script object.

	if (nsServiceManager::GetService(kPluginManagerCID, nsIPluginHost::GetIID(), (nsISupports**)&mPluginHost) != NS_OK)
		mPluginHost = nsnull;
	
	mPluginCount = 0;
	mPluginArray = nsnull;
}

PluginArrayImpl::~PluginArrayImpl()
{
	if (mPluginHost != nsnull)
		nsServiceManager::ReleaseService(kPluginManagerCID, mPluginHost);

	if (mPluginArray != nsnull) {
		for (int i = 0; i < mPluginCount; i++) {
			NS_IF_RELEASE(mPluginArray[i]);
		}
		delete[] mPluginArray;
	}
}

NS_IMPL_ADDREF(PluginArrayImpl)
NS_IMPL_RELEASE(PluginArrayImpl)

NS_IMETHODIMP PluginArrayImpl::QueryInterface(const nsIID& aIID,
                              void** aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null pointer");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(nsIScriptObjectOwner::GetIID())) {
    *aInstancePtrResult = (void*) ((nsIScriptObjectOwner*)this);
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(nsIDOMPluginArray::GetIID())) {
    *aInstancePtrResult = (void*) ((nsIDOMPluginArray*)this);
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtrResult = (void*) ((nsIScriptObjectOwner*)this);
    AddRef();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

NS_IMETHODIMP PluginArrayImpl::SetScriptObject(void *aScriptObject)
{
  mScriptObject = aScriptObject;
  return NS_OK;
}

NS_IMETHODIMP PluginArrayImpl::GetScriptObject(nsIScriptContext *aContext, void** aScriptObject)
{
  NS_PRECONDITION(nsnull != aScriptObject, "null arg");
  nsresult res = NS_OK;
  if (nsnull == mScriptObject) {
    res = NS_NewScriptPluginArray(aContext, (nsISupports*)(nsIDOMPluginArray*)this, mNavigator, &mScriptObject);
  }
  
  *aScriptObject = mScriptObject;
  return res;
}

NS_IMETHODIMP PluginArrayImpl::GetLength(PRUint32* aLength)
{
	return mPluginHost->GetPluginCount(aLength);
}

NS_IMETHODIMP PluginArrayImpl::Item(PRUint32 aIndex, nsIDOMPlugin** aReturn)
{
	NS_PRECONDITION(nsnull != aReturn, "null arg");

	if (mPluginArray == nsnull) {
		nsresult rv = GetPlugins();
		if (rv != NS_OK)
			return rv;
	}
	
	if (aIndex >= 0 && aIndex < mPluginCount) {
		*aReturn = mPluginArray[aIndex];
		NS_IF_ADDREF(*aReturn);
		return NS_OK;
	}
	
	return NS_ERROR_FAILURE;
}

NS_IMETHODIMP PluginArrayImpl::NamedItem(const nsString& aName, nsIDOMPlugin** aReturn)
{
	NS_PRECONDITION(nsnull != aReturn, "null arg");

	if (mPluginArray == nsnull) {
		nsresult rv = GetPlugins();
		if (rv != NS_OK)
			return rv;
	}

	*aReturn = nsnull;

	for (int i = 0; i < mPluginCount; i++) {
		nsString pluginName;
		nsIDOMPlugin* plugin = mPluginArray[i];
		if (plugin->GetName(pluginName) == NS_OK) {
			if (pluginName == aName) {
				*aReturn = plugin;
				NS_IF_ADDREF(plugin);
				return NS_OK;
			}
		}
	}

	return NS_ERROR_FAILURE;
}

NS_IMETHODIMP PluginArrayImpl::Refresh(PRBool aReloadDocuments)
{
	return NS_OK;
}

nsresult PluginArrayImpl::GetPlugins()
{
	nsresult rv = mPluginHost->GetPluginCount(&mPluginCount);
	if (rv == NS_OK) {
		mPluginArray = new nsIDOMPlugin*[mPluginCount];
		if (mPluginArray != nsnull) {
			rv = mPluginHost->GetPlugins(mPluginCount, mPluginArray);
			if (rv == NS_OK) {
				// need to wrap each of these with a PluginElementImpl, which is scriptable.
				for (int i = 0; i < mPluginCount; i++) {
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
	mScriptObject = nsnull;
	mPlugin = plugin;	// don't AddRef, see PluginArrayImpl::Item.
	mMimeTypeCount = 0;
	mMimeTypeArray = nsnull;
}

PluginElementImpl::~PluginElementImpl()
{
	NS_IF_RELEASE(mPlugin);
	
	if (mMimeTypeArray != nsnull) {
		for (int i = 0; i < mMimeTypeCount; i++)
			NS_IF_RELEASE(mMimeTypeArray[i]);
		delete[] mMimeTypeArray;
	}
}

NS_IMPL_ADDREF(PluginElementImpl)
NS_IMPL_RELEASE(PluginElementImpl)

NS_IMETHODIMP PluginElementImpl::QueryInterface(const nsIID& aIID,
                              void** aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null pointer");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(nsIScriptObjectOwner::GetIID())) {
    *aInstancePtrResult = (void*) ((nsIScriptObjectOwner*)this);
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(nsIDOMPlugin::GetIID())) {
    *aInstancePtrResult = (void*) ((nsIDOMPlugin*)this);
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtrResult = (void*) ((nsIScriptObjectOwner*)this);
    AddRef();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

NS_IMETHODIMP PluginElementImpl::SetScriptObject(void *aScriptObject)
{
  mScriptObject = aScriptObject;
  return NS_OK;
}

NS_IMETHODIMP PluginElementImpl::GetScriptObject(nsIScriptContext *aContext, void** aScriptObject)
{
  NS_PRECONDITION(nsnull != aScriptObject, "null arg");
  nsresult res = NS_OK;
  if (nsnull == mScriptObject) {
    nsIScriptGlobalObject *global = aContext->GetGlobalObject();
    res = NS_NewScriptPlugin(aContext, (nsISupports*)(nsIDOMPlugin*)this, global, &mScriptObject);
    NS_IF_RELEASE(global);
  }
  
  *aScriptObject = mScriptObject;
  return res;
}

NS_IMETHODIMP PluginElementImpl::GetDescription(nsString& aDescription)
{
	return mPlugin->GetDescription(aDescription);
}

NS_IMETHODIMP PluginElementImpl::GetFilename(nsString& aFilename)
{
	return mPlugin->GetFilename(aFilename);
}

NS_IMETHODIMP PluginElementImpl::GetName(nsString& aName)
{
	return mPlugin->GetName(aName);
}

NS_IMETHODIMP PluginElementImpl::GetLength(PRUint32* aLength)
{
	return mPlugin->GetLength(aLength);
}

NS_IMETHODIMP PluginElementImpl::Item(PRUint32 aIndex, nsIDOMMimeType** aReturn)
{
	if (mMimeTypeArray == nsnull) {
		nsresult rv = GetMimeTypes();
		if (rv != NS_OK)
			return rv;
	}
	if (aIndex >= 0 && aIndex < mMimeTypeCount) {
		nsIDOMMimeType* mimeType = mMimeTypeArray[aIndex];
		NS_IF_ADDREF(mimeType);
		*aReturn = mimeType;
		return NS_OK;
	}
	return NS_ERROR_FAILURE;
}

NS_IMETHODIMP PluginElementImpl::NamedItem(const nsString& aName, nsIDOMMimeType** aReturn)
{
	if (mMimeTypeArray == nsnull) {
		nsresult rv = GetMimeTypes();
		if (rv != NS_OK)
			return rv;
	}
	for (int i = 0; i < mMimeTypeCount; i++) {
		nsString type;
		nsIDOMMimeType* mimeType = mMimeTypeArray[i];
		if (mimeType->GetType(type) == NS_OK) {
			if (type == aName) {
				*aReturn = mimeType;
				NS_ADDREF(mimeType);
				return NS_OK;
			}
		}
	}
	return NS_ERROR_FAILURE;
}

nsresult PluginElementImpl::GetMimeTypes()
{
	nsresult rv = mPlugin->GetLength(&mMimeTypeCount);
	if (rv == NS_OK) {
		mMimeTypeArray = new nsIDOMMimeType*[mMimeTypeCount];
		if (mMimeTypeArray == nsnull)
			return NS_ERROR_OUT_OF_MEMORY;
		for (int i = 0; i < mMimeTypeCount; i++) {
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
