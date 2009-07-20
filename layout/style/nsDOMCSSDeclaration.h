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
 * The Original Code is Mozilla Communicator client code.
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

/* base class for DOM objects for element.style and cssStyleRule.style */

#ifndef nsDOMCSSDeclaration_h___
#define nsDOMCSSDeclaration_h___

#include "nsICSSDeclaration.h"
#include "nsIDOMNSCSS2Properties.h"

class nsCSSDeclaration;
class nsICSSParser;
class nsICSSLoader;
class nsIURI;
class nsIPrincipal;

class CSS2PropertiesTearoff : public nsIDOMNSCSS2Properties
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_NSIDOMCSS2PROPERTIES
  NS_DECL_NSIDOMNSCSS2PROPERTIES

  CSS2PropertiesTearoff(nsICSSDeclaration *aOuter);
  virtual ~CSS2PropertiesTearoff();

private:
  nsICSSDeclaration* mOuter;
};

class nsDOMCSSDeclaration : public nsICSSDeclaration
{
public:
  nsDOMCSSDeclaration();

  // Only implement QueryInterface; subclasses have the responsibility
  // of implementing AddRef/Release.
  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr);

  NS_DECL_NSICSSDECLARATION

  // Require subclasses to implement |GetParentRule|.
  //NS_DECL_NSIDOMCSSSTYLEDECLARATION
  NS_IMETHOD GetCssText(nsAString & aCssText);
  NS_IMETHOD SetCssText(const nsAString & aCssText);
  NS_IMETHOD GetPropertyValue(const nsAString & propertyName,
                              nsAString & _retval);
  NS_IMETHOD GetPropertyCSSValue(const nsAString & propertyName,
                                 nsIDOMCSSValue **_retval);
  NS_IMETHOD RemoveProperty(const nsAString & propertyName,
                            nsAString & _retval);
  NS_IMETHOD GetPropertyPriority(const nsAString & propertyName,
                                 nsAString & _retval);
  NS_IMETHOD SetProperty(const nsAString & propertyName,
                         const nsAString & value, const nsAString & priority);
  NS_IMETHOD GetLength(PRUint32 *aLength);
  NS_IMETHOD Item(PRUint32 index, nsAString & _retval);
  NS_IMETHOD GetParentRule(nsIDOMCSSRule * *aParentRule) = 0; 

protected:
  // Always fills in the out parameter, even on failure, and if the out
  // parameter is null the nsresult will be the correct thing to
  // propagate.
  virtual nsresult GetCSSDeclaration(nsCSSDeclaration **aDecl,
                                     PRBool aAllocate) = 0;
  virtual nsresult DeclarationChanged() = 0;
  
  // This will only fail if it can't get a parser or a principal.
  // This means it can return NS_OK without aURI or aCSSLoader being
  // initialized.
  virtual nsresult GetCSSParsingEnvironment(nsIURI** aSheetURI,
                                            nsIURI** aBaseURI,
                                            nsIPrincipal** aSheetPrincipal,
                                            nsICSSLoader** aCSSLoader,
                                            nsICSSParser** aCSSParser) = 0;

  nsresult ParsePropertyValue(const nsCSSProperty aPropID,
                              const nsAString& aPropValue);
  nsresult ParseDeclaration(const nsAString& aDecl,
                            PRBool aParseOnlyOneDecl, PRBool aClearOldDecl);

  // Prop-id based version of RemoveProperty.  Note that this does not
  // return the old value; it just does a straight removal.
  nsresult RemoveProperty(const nsCSSProperty aPropID);
  
  
protected:
  virtual ~nsDOMCSSDeclaration();

private:
  CSS2PropertiesTearoff mInner;
};

#endif // nsDOMCSSDeclaration_h___
