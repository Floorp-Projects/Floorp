/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsIDOMScriptObjectFactory_h__
#define nsIDOMScriptObjectFactory_h__

#include "nsISupports.h"
#include "nsIDOMClassInfo.h"
#include "nsStringGlue.h"

#define NS_IDOM_SCRIPT_OBJECT_FACTORY_IID   \
{ 0xd5a4f935, 0xe428, 0x47ec, \
  { 0x8f, 0x36, 0x44, 0x23, 0xfa, 0xa2, 0x21, 0x90 } }

class nsIScriptContext;
class nsIScriptGlobalObject;
class nsIScriptRuntime;
class nsIDOMEventListener;

typedef nsIClassInfo* (*nsDOMClassInfoExternalConstructorFnc)
  (const char* aName);

class nsIDOMScriptObjectFactory : public nsISupports {
public:  
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IDOM_SCRIPT_OBJECT_FACTORY_IID)

  // Get a script language given its "name" (ie, the mime-type)
  // Note that to fetch javascript from this function, you must currently
  // use the name "application/javascript" (but also note that all existing
  // callers of this function optimize the detection of JS, so do not
  // ask this function for JS)
  NS_IMETHOD GetScriptRuntime(const nsAString &aLanguageName,
                              nsIScriptRuntime **aLanguage) = 0;

  // Get a script language given its nsIProgrammingLanguage ID.
  NS_IMETHOD GetScriptRuntimeByID(PRUint32 aScriptTypeID, 
                                  nsIScriptRuntime **aLanguage) = 0;

  // Get the ID for a language given its name - but like GetScriptRuntime,
  // only "application/javascript" is currently supported for JS.
  NS_IMETHOD GetIDForScriptType(const nsAString &aLanguageName,
                                PRUint32 *aScriptTypeID) = 0;

  NS_IMETHOD NewScriptGlobalObject(PRBool aIsChrome,
                                   PRBool aIsModalContentWindow,
                                   nsIScriptGlobalObject **aGlobal) = 0;

  NS_IMETHOD_(nsISupports *) GetClassInfoInstance(nsDOMClassInfoID aID) = 0;
  NS_IMETHOD_(nsISupports *) GetExternalClassInfoInstance(const nsAString& aName) = 0;

  // Register the info for an external class. aName must be static
  // data, it will not be deleted by the DOM code. aProtoChainInterface
  // must be registered in the JAVASCRIPT_DOM_INTERFACE category, or
  // prototypes for this class won't work (except if the interface
  // name starts with nsIDOM).
  NS_IMETHOD RegisterDOMClassInfo(const char *aName,
                                  nsDOMClassInfoExternalConstructorFnc aConstructorFptr,
                                  const nsIID *aProtoChainInterface,
                                  const nsIID **aInterfaces,
                                  PRUint32 aScriptableFlags,
                                  PRBool aHasClassInterface,
                                  const nsCID *aConstructorCID) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIDOMScriptObjectFactory,
                              NS_IDOM_SCRIPT_OBJECT_FACTORY_IID)

#endif /* nsIDOMScriptObjectFactory_h__ */
