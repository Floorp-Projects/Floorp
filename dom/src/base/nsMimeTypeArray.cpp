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

#include "nsMimeTypeArray.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDOMNavigator.h"
#include "nsIDOMPluginArray.h"
#include "nsIDOMPlugin.h"
#include "nsDOMClassInfo.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

MimeTypeArrayImpl::MimeTypeArrayImpl(nsIDOMNavigator* navigator)
{
	NS_INIT_ISUPPORTS();
	mNavigator = navigator;
	mMimeTypeCount = 0;
	mMimeTypeArray = nsnull;
}

MimeTypeArrayImpl::~MimeTypeArrayImpl()
{
  Clear();
}


// QueryInterface implementation for MimeTypeArrayImpl
NS_INTERFACE_MAP_BEGIN(MimeTypeArrayImpl)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMimeTypeArray)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(MimeTypeArray)
NS_INTERFACE_MAP_END


NS_IMPL_ADDREF(MimeTypeArrayImpl)
NS_IMPL_RELEASE(MimeTypeArrayImpl)


NS_IMETHODIMP
MimeTypeArrayImpl::GetLength(PRUint32* aLength)
{
	if (mMimeTypeArray == nsnull) {
		nsresult rv = GetMimeTypes();
		if (rv != NS_OK)
			return rv;
	}
	*aLength = mMimeTypeCount;
	return NS_OK;
}

NS_IMETHODIMP
MimeTypeArrayImpl::Item(PRUint32 aIndex, nsIDOMMimeType** aReturn)
{
	if (mMimeTypeArray == nsnull) {
		nsresult rv = GetMimeTypes();
		if (rv != NS_OK)
			return rv;
	}
	if (aIndex < mMimeTypeCount) {
		*aReturn = mMimeTypeArray[aIndex];
		NS_IF_ADDREF(*aReturn);
		return NS_OK;
	}
	return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
MimeTypeArrayImpl::NamedItem(const nsAReadableString& aName,
                             nsIDOMMimeType** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = nsnull;

  if (mMimeTypeArray == nsnull) {
    nsresult rv = GetMimeTypes();
    if (rv != NS_OK)
      return rv;
  }

  PRUint32 i;

  for (i = 0; i < mMimeTypeCount; i++) {
    nsIDOMMimeType *mtype = mMimeTypeArray[i];

    nsAutoString type;
    mtype->GetType(type);

    if (type.Equals(aName)) {
      *aReturn = mtype;

      NS_ADDREF(*aReturn);

      break;
    }
  }

  return NS_OK;
}

void  MimeTypeArrayImpl::Clear()
{
  if (mMimeTypeArray != nsnull) {
    for (PRUint32 i = 0; i < mMimeTypeCount; i++) {
      NS_IF_RELEASE(mMimeTypeArray[i]);
    }
    delete[] mMimeTypeArray;
    mMimeTypeArray = nsnull;
  }
  mMimeTypeCount = 0;
}

nsresult MimeTypeArrayImpl::Refresh()
{
  Clear();
  return GetMimeTypes();
}

nsresult MimeTypeArrayImpl::GetMimeTypes()
{
  NS_PRECONDITION(!mMimeTypeArray && mMimeTypeCount==0,
                      "already initialized");

	nsIDOMPluginArray* pluginArray = nsnull;
	nsresult rv = mNavigator->GetPlugins(&pluginArray);
	if (rv == NS_OK) {
		// count up all possible MimeTypes, and collect them here. Later,
		// we'll remove duplicates.
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

MimeTypeElementImpl::MimeTypeElementImpl(nsIDOMPlugin* aPlugin,
                                         nsIDOMMimeType* aMimeType)
{
	NS_INIT_ISUPPORTS();
	mPlugin = aPlugin;
	mMimeType = aMimeType;
}

MimeTypeElementImpl::~MimeTypeElementImpl()
{
	NS_IF_RELEASE(mMimeType);
}


// QueryInterface implementation for MimeTypeElementImpl
NS_INTERFACE_MAP_BEGIN(MimeTypeElementImpl)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMimeType)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(MimeType)
NS_INTERFACE_MAP_END


NS_IMPL_ADDREF(MimeTypeElementImpl)
NS_IMPL_RELEASE(MimeTypeElementImpl)


NS_IMETHODIMP
MimeTypeElementImpl::GetDescription(nsAWritableString& aDescription)
{
	return mMimeType->GetDescription(aDescription);
}

NS_IMETHODIMP
MimeTypeElementImpl::GetEnabledPlugin(nsIDOMPlugin** aEnabledPlugin)
{	
	*aEnabledPlugin = mPlugin;
	NS_IF_ADDREF(mPlugin);
	return NS_OK;
}

NS_IMETHODIMP
MimeTypeElementImpl::GetSuffixes(nsAWritableString& aSuffixes)
{
	return mMimeType->GetSuffixes(aSuffixes);
}

NS_IMETHODIMP
MimeTypeElementImpl::GetType(nsAWritableString& aType)
{
	return mMimeType->GetType(aType);
}
