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

#ifndef nsPluginArray_h___
#define nsPluginArray_h___

#include "nsIDOMPluginArray.h"
#include "nsIDOMPlugin.h"
#include "nsIPluginHost.h"

class NavigatorImpl;
class nsIDocShell;
struct nsIPluginHost;

class PluginArrayImpl : public nsIDOMPluginArray,
                        public nsIDOMJSPluginArray
{
public:
  PluginArrayImpl(NavigatorImpl* navigator, nsIDocShell *aDocShell);
  virtual ~PluginArrayImpl();

  NS_DECL_ISUPPORTS

  // nsIDOMPluginArray
  NS_DECL_NSIDOMPLUGINARRAY

  // nsIDOMJSPluginArray
  NS_DECL_NSIDOMJSPLUGINARRAY

  nsresult GetPluginHost(nsIPluginHost** aPluginHost);

private:
  nsresult GetPlugins();

public:
  void SetDocShell(nsIDocShell* aDocShell);

protected:
  NavigatorImpl* mNavigator;
  nsCOMPtr<nsIPluginHost> mPluginHost;
  PRUint32 mPluginCount;
  nsIDOMPlugin** mPluginArray;
  nsIDocShell* mDocShell; // weak reference
};

class PluginElementImpl : public nsIDOMPlugin
{
public:
  PluginElementImpl(nsIDOMPlugin* plugin);
  virtual ~PluginElementImpl();

  NS_DECL_ISUPPORTS

  NS_IMETHOD GetDescription(nsAWritableString& aDescription);
  NS_IMETHOD GetFilename(nsAWritableString& aFilename);
  NS_IMETHOD GetName(nsAWritableString& aName);
  NS_IMETHOD GetLength(PRUint32* aLength);
  NS_IMETHOD Item(PRUint32 aIndex, nsIDOMMimeType** aReturn);
  NS_IMETHOD NamedItem(const nsAReadableString& aName, nsIDOMMimeType** aReturn);

private:
  nsresult GetMimeTypes();

protected:
  nsIDOMPlugin* mPlugin;
  PRUint32 mMimeTypeCount;
  nsIDOMMimeType** mMimeTypeArray;
};

#endif /* nsPluginArray_h___ */
