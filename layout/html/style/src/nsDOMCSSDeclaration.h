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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#ifndef nsDOMCSSDeclaration_h___
#define nsDOMCSSSDeclaration_h___

#include "nsISupports.h"
#include "nsIDOMCSS2Properties.h"
#include "nsIScriptObjectOwner.h"

class nsICSSDeclaration;
class nsICSSParser;
class nsIURI;

class nsDOMCSSDeclaration : public nsIDOMCSS2Properties,
                            public nsIScriptObjectOwner
{
public:
  nsDOMCSSDeclaration();

  NS_DECL_ISUPPORTS

  // NS_DECL_IDOMCSSSTYLEDECLARATION
  NS_IMETHOD    GetCssText(nsAWritableString& aCssText);
  NS_IMETHOD    SetCssText(const nsAReadableString& aCssText);
  NS_IMETHOD    GetLength(PRUint32* aLength);
  NS_IMETHOD    GetParentRule(nsIDOMCSSRule** aParentRule);
  NS_IMETHOD    GetPropertyValue(const nsAReadableString& aPropertyName,
                                 nsAWritableString& aReturn);
  NS_IMETHOD    GetPropertyCSSValue(const nsAReadableString& aPropertyName,
                                    nsIDOMCSSValue** aReturn);
  NS_IMETHOD    RemoveProperty(const nsAReadableString& aPropertyName,
                               nsAWritableString& aReturn) = 0;
  NS_IMETHOD    GetPropertyPriority(const nsAReadableString& aPropertyName,
                                    nsAWritableString& aReturn);
  NS_IMETHOD    SetProperty(const nsAReadableString& aPropertyName,
                            const nsAReadableString& aValue, const nsAReadableString& aPriority);
  NS_IMETHOD    Item(PRUint32 aIndex, nsAWritableString& aReturn);


  NS_DECL_IDOMCSS2PROPERTIES
  
  // nsIScriptObjectOwner interface
  NS_IMETHOD GetScriptObject(nsIScriptContext *aContext, void** aScriptObject);
  NS_IMETHOD SetScriptObject(void* aScriptObject);

  virtual void DropReference() = 0;
  virtual nsresult GetCSSDeclaration(nsICSSDeclaration **aDecl,
                                     PRBool aAllocate) = 0;
  // Note! This will only set the declaration if a style rule already exists
  virtual nsresult SetCSSDeclaration(nsICSSDeclaration *aDecl) = 0;

  virtual nsresult ParseDeclaration(const nsAReadableString& aDecl,
                                    PRBool aParseOnlyOneDecl,
                                    PRBool aClearOldDecl) = 0;
  virtual nsresult GetParent(nsISupports **aParent) = 0;
  
protected:
  virtual ~nsDOMCSSDeclaration();

  void *mScriptObject;
};

#endif // nsDOMCSSDeclaration_h___
