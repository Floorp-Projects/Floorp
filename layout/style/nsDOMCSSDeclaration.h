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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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
#ifndef nsDOMCSSDeclaration_h___
#define nsDOMCSSSDeclaration_h___

#include "nsISupports.h"
#include "nsIDOMCSSStyleDeclaration.h"
#include "nsIDOMCSS2Properties.h"

class nsICSSDeclaration;
class nsICSSParser;
class nsIURI;

class nsDOMCSSDeclaration : public nsIDOMCSSStyleDeclaration,
                            public nsIDOMNSCSS2Properties
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
                            const nsAReadableString& aValue,
                            const nsAReadableString& aPriority);
  NS_IMETHOD    Item(PRUint32 aIndex, nsAWritableString& aReturn);


  NS_DECL_NSIDOMCSS2PROPERTIES
  NS_DECL_NSIDOMNSCSS2PROPERTIES

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
};

#endif // nsDOMCSSDeclaration_h___
