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
	NS_IMETHOD NamedItem(const nsString& aName, nsIDOMMimeType** aReturn);

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

	NS_IMETHOD GetDescription(nsString& aDescription);
	NS_IMETHOD GetEnabledPlugin(nsIDOMPlugin** aEnabledPlugin);
	NS_IMETHOD GetSuffixes(nsString& aSuffixes);
	NS_IMETHOD GetType(nsString& aType);

protected:
	void *mScriptObject;
	nsIDOMPlugin* mPlugin;
	nsIDOMMimeType* mMimeType;
};

#endif /* nsMimeTypeArray_h___ */
