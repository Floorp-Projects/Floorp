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

#include "nsDOMCSSDeclaration.h"
#include "nsIDOMCSSRule.h"
#include "nsICSSParser.h"
#include "nsIStyleRule.h"
#include "nsCSSDeclaration.h"
#include "nsCSSProps.h"
#include "nsCOMPtr.h"
#include "nsIURL.h"

#include "nsContentUtils.h"


nsDOMCSSDeclaration::nsDOMCSSDeclaration()
{
  NS_INIT_ISUPPORTS();
}

nsDOMCSSDeclaration::~nsDOMCSSDeclaration()
{
}


// QueryInterface implementation for CSSStyleSheetImpl
NS_INTERFACE_MAP_BEGIN(nsDOMCSSDeclaration)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCSSStyleDeclaration)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCSS2Properties)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNSCSS2Properties)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMCSS2Properties)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(CSSStyleDeclaration)
NS_INTERFACE_MAP_END


NS_IMPL_ADDREF(nsDOMCSSDeclaration);
NS_IMPL_RELEASE(nsDOMCSSDeclaration);


NS_IMETHODIMP
nsDOMCSSDeclaration::GetCssText(nsAString& aCssText)
{
  nsCSSDeclaration* decl;
  aCssText.Truncate();
  GetCSSDeclaration(&decl, PR_FALSE);
  NS_ASSERTION(decl, "null CSSDeclaration");

  if (decl) {
    decl->ToString(aCssText);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetCssText(const nsAString& aCssText)
{
  return ParseDeclaration(aCssText, PR_FALSE, PR_TRUE);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetLength(PRUint32* aLength)
{
  nsCSSDeclaration *decl;
  nsresult result = GetCSSDeclaration(&decl, PR_FALSE);
 
  *aLength = 0;
  if ((NS_OK == result) && (nsnull != decl)) {
    *aLength = decl->Count();
  }

  return result;
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetParentRule(nsIDOMCSSRule** aParentRule)
{
  NS_ENSURE_ARG_POINTER(aParentRule);
  *aParentRule = nsnull;

  nsCOMPtr<nsISupports> parent;

  GetParent(getter_AddRefs(parent));

  if (parent) {
    parent->QueryInterface(NS_GET_IID(nsIDOMCSSRule), (void **)aParentRule);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetPropertyCSSValue(const nsAString& aPropertyName,
                                         nsIDOMCSSValue** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);

  // We don't support CSSValue yet so we'll just return null...
  *aReturn = nsnull;

  return NS_OK;
}

NS_IMETHODIMP
nsDOMCSSDeclaration::Item(PRUint32 aIndex, nsAString& aReturn)
{
  nsCSSDeclaration *decl;
  nsresult result = GetCSSDeclaration(&decl, PR_FALSE);

  aReturn.SetLength(0);
  if ((NS_OK == result) && (nsnull != decl)) {
    result = decl->GetNthProperty(aIndex, aReturn);
  }

  return result;
}

NS_IMETHODIMP    
nsDOMCSSDeclaration::GetPropertyValue(const nsAString& aPropertyName, 
                                      nsAString& aReturn)
{
  nsCSSValue val;
  nsCSSDeclaration *decl;
  nsresult result = GetCSSDeclaration(&decl, PR_FALSE);

  aReturn.SetLength(0);
  if ((NS_OK == result) && (nsnull != decl)) {
    result = decl->GetValue(aPropertyName, aReturn);
  }

  return result;
}

NS_IMETHODIMP    
nsDOMCSSDeclaration::GetPropertyPriority(const nsAString& aPropertyName, 
                                         nsAString& aReturn)
{
  nsCSSDeclaration *decl;
  nsresult result = GetCSSDeclaration(&decl, PR_FALSE);
  PRBool isImportant = PR_FALSE;

  if ((NS_OK == result) && (nsnull != decl)) {
    isImportant = decl->GetValueIsImportant(aPropertyName);
  }

  if ((NS_OK == result) && isImportant) {
    aReturn.Assign(NS_LITERAL_STRING("important"));    
  }
  else {
    aReturn.SetLength(0);
  }

  return result;
}

NS_IMETHODIMP    
nsDOMCSSDeclaration::SetProperty(const nsAString& aPropertyName, 
                                 const nsAString& aValue, 
                                 const nsAString& aPriority)
{
  if (aValue.IsEmpty()) {
     // If the new value of the property is an empty string we remove the
     // property.
    nsAutoString tmp;
    return RemoveProperty(aPropertyName, tmp);
  }

  nsresult res;
  if (aPriority.IsEmpty()) {
    res = ParseDeclaration(aPropertyName + NS_LITERAL_STRING(":") +
                           aValue,
                           PR_TRUE, PR_FALSE);
  }
  else {
    res = ParseDeclaration(aPropertyName + NS_LITERAL_STRING(":") +
                           aValue + NS_LITERAL_STRING("!") + aPriority,
                           PR_TRUE, PR_FALSE);
  }
  return res;
}

/**
 * Helper function to reduce code size.
 */
static nsresult
CallSetProperty(nsDOMCSSDeclaration* aDecl,
                const nsAString& aPropName,
                const nsAString& aValue)
{
  if (aValue.IsEmpty()) {
    // If the new value of the property is an empty string we remove the
     // property.
    nsAutoString tmp;
    return aDecl->RemoveProperty(aPropName, tmp);
  }

  return aDecl->ParsePropertyValue(aPropName, aValue);
}


// nsIDOMCSS2Properties

#define CSS_PROP(name_, id_, method_, hint_)                                  \
  NS_IMETHODIMP                                                               \
  nsDOMCSSDeclaration::Get##method_(nsAString& aValue)                        \
  {                                                                           \
    return GetPropertyValue(NS_LITERAL_STRING(#name_), aValue);               \
  }                                                                           \
                                                                              \
  NS_IMETHODIMP                                                               \
  nsDOMCSSDeclaration::Set##method_(const nsAString& aValue)                  \
  {                                                                           \
    return CallSetProperty(this, NS_LITERAL_STRING(#name_), aValue);          \
  }

#define CSS_PROP_INTERNAL(name_, id_, method_, hint_) /* nothing */
#define CSS_PROP_NOTIMPLEMENTED(name_, id_, method_, hint_)                   \
  CSS_PROP(name_, id_, method_, hint_)

#include "nsCSSPropList.h"

#undef CSS_PROP_INTERNAL
#undef CSS_PROP
