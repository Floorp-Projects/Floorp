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

#ifndef nsPluginArray_h___
#define nsPluginArray_h___

#include "nsIScriptObjectOwner.h"
#include "nsIDOMPluginArray.h"
#include "nsIDOMPlugin.h"

class nsIDOMNavigator;
class nsIPluginHost;

class PluginArrayImpl : public nsIScriptObjectOwner, public nsIDOMPluginArray {
public:
  PluginArrayImpl(nsIDOMNavigator* navigator);
  virtual ~PluginArrayImpl();

  NS_DECL_ISUPPORTS

  NS_IMETHOD GetScriptObject(nsIScriptContext *aContext, void** aScriptObject);
  NS_IMETHOD SetScriptObject(void* aScriptObject);

  NS_IMETHOD GetLength(PRUint32* aLength);
  NS_IMETHOD Item(PRUint32 aIndex, nsIDOMPlugin** aReturn);
  NS_IMETHOD NamedItem(const nsString& aName, nsIDOMPlugin** aReturn);
  NS_IMETHOD Refresh(PRBool aReloadDocuments);

private:
  nsresult GetPlugins();

protected:
  void *mScriptObject;
  nsIDOMNavigator* mNavigator;
  nsIPluginHost* mPluginHost;
  PRUint32 mPluginCount;
  nsIDOMPlugin** mPluginArray;
};

class PluginElementImpl : public nsIScriptObjectOwner, public nsIDOMPlugin {
public:
  PluginElementImpl(nsIDOMPlugin* plugin);
  virtual ~PluginElementImpl();

  NS_DECL_ISUPPORTS

  NS_IMETHOD GetScriptObject(nsIScriptContext *aContext, void** aScriptObject);
  NS_IMETHOD SetScriptObject(void* aScriptObject);

  NS_IMETHOD GetDescription(nsString& aDescription);
  NS_IMETHOD GetFilename(nsString& aFilename);
  NS_IMETHOD GetName(nsString& aName);
  NS_IMETHOD GetLength(PRUint32* aLength);
  NS_IMETHOD Item(PRUint32 aIndex, nsIDOMMimeType** aReturn);
  NS_IMETHOD NamedItem(const nsString& aName, nsIDOMMimeType** aReturn);

private:
  nsresult GetMimeTypes();

protected:
  void *mScriptObject;
  nsIDOMPlugin* mPlugin;
  PRUint32 mMimeTypeCount;
  nsIDOMMimeType** mMimeTypeArray;
};

#endif /* nsPluginArray_h___ */
