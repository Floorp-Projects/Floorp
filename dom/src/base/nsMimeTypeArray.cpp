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

#include "nsMimeTypeArray.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDOMNavigator.h"
#include "nsIDOMPluginArray.h"
#include "nsIDOMPlugin.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

MimeTypeArrayImpl::MimeTypeArrayImpl(nsIDOMNavigator* navigator)
{
	NS_INIT_ISUPPORTS();
	mScriptObject = nsnull;
	mNavigator = navigator;
	mMimeTypeCount = 0;
	mMimeTypeArray = nsnull;
}

MimeTypeArrayImpl::~MimeTypeArrayImpl()
{
	if (mMimeTypeArray != nsnull) {
		for (int i = 0; i < mMimeTypeCount; i++) {
			NS_IF_RELEASE(mMimeTypeArray[i]);
		}
		delete[] mMimeTypeArray;
	}
}

NS_IMPL_ADDREF(MimeTypeArrayImpl)
NS_IMPL_RELEASE(MimeTypeArrayImpl)

NS_IMETHODIMP MimeTypeArrayImpl::QueryInterface(const nsIID& aIID,
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
	if (aIID.Equals(nsIDOMMimeTypeArray::GetIID())) {
		*aInstancePtrResult = (void*) ((nsIDOMMimeTypeArray*)this);
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

NS_IMETHODIMP MimeTypeArrayImpl::SetScriptObject(void *aScriptObject)
{
	mScriptObject = aScriptObject;
	return NS_OK;
}

NS_IMETHODIMP MimeTypeArrayImpl::GetScriptObject(nsIScriptContext *aContext, void** aScriptObject)
{
	NS_PRECONDITION(nsnull != aScriptObject, "null arg");
	nsresult res = NS_OK;
	if (nsnull == mScriptObject) {
		res = NS_NewScriptMimeTypeArray(aContext, (nsISupports*)(nsIDOMMimeTypeArray*)this, mNavigator, &mScriptObject);
	}

	*aScriptObject = mScriptObject;
	return res;
}

NS_IMETHODIMP MimeTypeArrayImpl::GetLength(PRUint32* aLength)
{
	if (mMimeTypeArray == nsnull) {
		nsresult rv = GetMimeTypes();
		if (rv != NS_OK)
			return rv;
	}
	*aLength = mMimeTypeCount;
	return NS_OK;
}

NS_IMETHODIMP MimeTypeArrayImpl::Item(PRUint32 aIndex, nsIDOMMimeType** aReturn)
{
	if (mMimeTypeArray == nsnull) {
		nsresult rv = GetMimeTypes();
		if (rv != NS_OK)
			return rv;
	}
	if (aIndex >= 0 && aIndex < mMimeTypeCount) {
		*aReturn = mMimeTypeArray[aIndex];
		NS_IF_ADDREF(*aReturn);
		return NS_OK;
	}
	return NS_ERROR_FAILURE;
}

NS_IMETHODIMP MimeTypeArrayImpl::NamedItem(const nsString& aName, nsIDOMMimeType** aReturn)
{
	*aReturn = nsnull;
	return NS_OK;
}

nsresult MimeTypeArrayImpl::GetMimeTypes()
{
	nsIDOMPluginArray* pluginArray = nsnull;
	nsresult rv = mNavigator->GetPlugins(&pluginArray);
	if (rv == NS_OK) {
		// count up all possible MimeTypes, and collect them here. Later, we'll remove
		// duplicates.
		mMimeTypeCount = 0;
		PRUint32 pluginCount = 0;
		rv = pluginArray->GetLength(&pluginCount);
		if (rv == NS_OK) {
			PRUint32 i;
			for (i = 0; i < pluginCount; i++) {
				nsIDOMPlugin* plugin = nsnull;
				if (pluginArray->Item(i, &plugin) == NS_OK) {
					PRUint32 mimeTypeCount = 0;
					if (plugin->GetLength(&mimeTypeCount) == NS_OK)
						mMimeTypeCount += mimeTypeCount;
					NS_RELEASE(plugin);
				}
			}
			// now we know how many there are, start gathering them.
			mMimeTypeArray = new nsIDOMMimeType*[mMimeTypeCount];
			if (mMimeTypeArray == nsnull)
				return NS_ERROR_OUT_OF_MEMORY;
			PRUint32 mimeTypeIndex = 0;
			PRUint32 k;
			for (k = 0; k < pluginCount; k++) {
				nsIDOMPlugin* plugin = nsnull;
				if (pluginArray->Item(k, &plugin) == NS_OK) {
					PRUint32 mimeTypeCount = 0;
					if (plugin->GetLength(&mimeTypeCount) == NS_OK) {
						for (PRUint32 j = 0; j < mimeTypeCount; j++)
							plugin->Item(j, &mMimeTypeArray[mimeTypeIndex++]);
					}
					NS_RELEASE(plugin);
				}
			}
		}
		NS_RELEASE(pluginArray);
	}
	return rv;
}

MimeTypeElementImpl::MimeTypeElementImpl(nsIDOMPlugin* aPlugin, nsIDOMMimeType* aMimeType)
{
	NS_INIT_ISUPPORTS();
	mScriptObject = nsnull;
	mPlugin = aPlugin;
	mMimeType = aMimeType;
}

MimeTypeElementImpl::~MimeTypeElementImpl()
{
	NS_IF_RELEASE(mMimeType);
}

NS_IMPL_ADDREF(MimeTypeElementImpl)
NS_IMPL_RELEASE(MimeTypeElementImpl)

NS_IMETHODIMP MimeTypeElementImpl::QueryInterface(const nsIID& aIID,
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
	if (aIID.Equals(nsIDOMMimeType::GetIID())) {
		*aInstancePtrResult = (void*) ((nsIDOMMimeType*)this);
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

NS_IMETHODIMP MimeTypeElementImpl::SetScriptObject(void *aScriptObject)
{
	mScriptObject = aScriptObject;
	return NS_OK;
}

NS_IMETHODIMP MimeTypeElementImpl::GetScriptObject(nsIScriptContext *aContext, void** aScriptObject)
{
	NS_PRECONDITION(nsnull != aScriptObject, "null arg");
	nsresult res = NS_OK;
	if (nsnull == mScriptObject) {
		nsIScriptGlobalObject *global = aContext->GetGlobalObject();
		res = NS_NewScriptMimeType(aContext, (nsISupports*)(nsIDOMMimeType*)this, global, &mScriptObject);
		NS_IF_RELEASE(global);
	}

	*aScriptObject = mScriptObject;
	return res;
}

NS_IMETHODIMP MimeTypeElementImpl::GetDescription(nsString& aDescription)
{
	return mMimeType->GetDescription(aDescription);
}

NS_IMETHODIMP MimeTypeElementImpl::GetEnabledPlugin(nsIDOMPlugin** aEnabledPlugin)
{	
	*aEnabledPlugin = mPlugin;
	NS_IF_ADDREF(mPlugin);
	return NS_OK;
}

NS_IMETHODIMP MimeTypeElementImpl::GetSuffixes(nsString& aSuffixes)
{
	return mMimeType->GetSuffixes(aSuffixes);
}

NS_IMETHODIMP MimeTypeElementImpl::GetType(nsString& aType)
{
	return mMimeType->GetType(aType);
}
