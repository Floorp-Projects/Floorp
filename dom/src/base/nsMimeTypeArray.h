/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsMimeTypeArray_h___
#define nsMimeTypeArray_h___

#include "nsIScriptObjectOwner.h"
#include "nsIDOMMimeTypeArray.h"
#include "nsIDOMMimeType.h"

class nsIDOMNavigator;

class MimeTypeArrayImpl : public nsIScriptObjectOwner, public nsIDOMMimeTypeArray {
public:
	MimeTypeArrayImpl(nsIDOMNavigator* navigator);
	virtual ~MimeTypeArrayImpl();

	NS_DECL_ISUPPORTS

	NS_IMETHOD GetScriptObject(nsIScriptContext *aContext, void** aScriptObject);
	NS_IMETHOD SetScriptObject(void* aScriptObject);

	NS_IMETHOD GetLength(PRUint32* aLength);
	NS_IMETHOD Item(PRUint32 aIndex, nsIDOMMimeType** aReturn);
	NS_IMETHOD NamedItem(const nsAReadableString& aName, nsIDOMMimeType** aReturn);

private:
	nsresult GetMimeTypes();

protected:
	void *mScriptObject;
	nsIDOMNavigator* mNavigator;
	PRUint32 mMimeTypeCount;
	nsIDOMMimeType** mMimeTypeArray;
};

class MimeTypeElementImpl : public nsIScriptObjectOwner, public nsIDOMMimeType {
public:
	MimeTypeElementImpl(nsIDOMPlugin* aPlugin, nsIDOMMimeType* aMimeType);
	virtual ~MimeTypeElementImpl();

	NS_DECL_ISUPPORTS

	NS_IMETHOD GetScriptObject(nsIScriptContext *aContext, void** aScriptObject);
	NS_IMETHOD SetScriptObject(void* aScriptObject);

	NS_IMETHOD GetDescription(nsAWritableString& aDescription);
	NS_IMETHOD GetEnabledPlugin(nsIDOMPlugin** aEnabledPlugin);
	NS_IMETHOD GetSuffixes(nsAWritableString& aSuffixes);
	NS_IMETHOD GetType(nsAWritableString& aType);

protected:
	void *mScriptObject;
	nsIDOMPlugin* mPlugin;
	nsIDOMMimeType* mMimeType;
};

#endif /* nsMimeTypeArray_h___ */
