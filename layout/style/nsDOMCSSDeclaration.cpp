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
 *   Mats Palmgren <mats.palmgren@bredband.net>
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

#include "nsDOMCSSDeclaration.h"
#include "nsIDOMCSSRule.h"
#include "nsCSSParser.h"
#include "mozilla/css/Loader.h"
#include "nsIStyleRule.h"
#include "mozilla/css/Declaration.h"
#include "nsCSSProps.h"
#include "nsCOMPtr.h"
#include "nsIURL.h"
#include "nsReadableUtils.h"
#include "nsIPrincipal.h"

#include "nsContentUtils.h"
#include "mozAutoDocUpdate.h"

namespace css = mozilla::css;

nsDOMCSSDeclaration::~nsDOMCSSDeclaration()
{
}

DOMCI_DATA(CSSStyleDeclaration, nsDOMCSSDeclaration)

NS_INTERFACE_TABLE_HEAD(nsDOMCSSDeclaration)
  NS_INTERFACE_TABLE3(nsDOMCSSDeclaration,
                      nsICSSDeclaration,
                      nsIDOMCSSStyleDeclaration,
                      nsIDOMCSS2Properties)
  NS_INTERFACE_TABLE_TO_MAP_SEGUE
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(CSSStyleDeclaration)
NS_INTERFACE_MAP_END

NS_IMETHODIMP
nsDOMCSSDeclaration::GetPropertyValue(const nsCSSProperty aPropID,
                                      nsAString& aValue)
{
  NS_PRECONDITION(aPropID != eCSSProperty_UNKNOWN,
                  "Should never pass eCSSProperty_UNKNOWN around");

  css::Declaration* decl = GetCSSDeclaration(PR_FALSE);

  aValue.Truncate();
  if (decl) {
    decl->GetValue(aPropID, aValue);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetPropertyValue(const nsCSSProperty aPropID,
                                      const nsAString& aValue)
{
  if (aValue.IsEmpty()) {
    // If the new value of the property is an empty string we remove the
    // property.
    return RemoveProperty(aPropID);
  }

  return ParsePropertyValue(aPropID, aValue, PR_FALSE);
}


NS_IMETHODIMP
nsDOMCSSDeclaration::GetCssText(nsAString& aCssText)
{
  css::Declaration* decl = GetCSSDeclaration(PR_FALSE);
  aCssText.Truncate();

  if (decl) {
    decl->ToString(aCssText);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetCssText(const nsAString& aCssText)
{
  // We don't need to *do* anything with the old declaration, but we need
  // to ensure that it exists, or else SetCSSDeclaration may crash.
  css::Declaration* olddecl = GetCSSDeclaration(PR_TRUE);
  if (!olddecl) {
    return NS_ERROR_FAILURE;
  }

  nsresult result;
  nsRefPtr<css::Loader> cssLoader;
  nsCOMPtr<nsIURI> baseURI, sheetURI;
  nsCOMPtr<nsIPrincipal> sheetPrincipal;

  result = GetCSSParsingEnvironment(getter_AddRefs(sheetURI),
                                    getter_AddRefs(baseURI),
                                    getter_AddRefs(sheetPrincipal),
                                    getter_AddRefs(cssLoader));

  if (NS_FAILED(result)) {
    return result;
  }

  // For nsDOMCSSAttributeDeclaration, SetCSSDeclaration will lead to
  // Attribute setting code, which leads in turn to BeginUpdate.  We
  // need to start the update now so that the old rule doesn't get used
  // between when we mutate the declaration and when we set the new
  // rule (see stack in bug 209575).
  mozAutoDocConditionalContentUpdateBatch autoUpdate(DocToUpdate(), PR_TRUE);

  nsAutoPtr<css::Declaration> decl(new css::Declaration());
  decl->InitializeEmpty();
  nsCSSParser cssParser(cssLoader);
  PRBool changed;
  result = cssParser.ParseDeclarations(aCssText, sheetURI, baseURI,
                                       sheetPrincipal, decl, &changed);
  if (NS_FAILED(result) || !changed) {
    return result;
  }

  return SetCSSDeclaration(decl.forget());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetLength(PRUint32* aLength)
{
  css::Declaration* decl = GetCSSDeclaration(PR_FALSE);

  if (decl) {
    *aLength = decl->Count();
  } else {
    *aLength = 0;
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
  css::Declaration* decl = GetCSSDeclaration(PR_FALSE);

  aReturn.SetLength(0);
  if (decl) {
    decl->GetNthProperty(aIndex, aReturn);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetPropertyValue(const nsAString& aPropertyName,
                                      nsAString& aReturn)
{
  const nsCSSProperty propID = nsCSSProps::LookupProperty(aPropertyName);
  if (propID == eCSSProperty_UNKNOWN) {
    aReturn.Truncate();
    return NS_OK;
  }

  return GetPropertyValue(propID, aReturn);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetPropertyPriority(const nsAString& aPropertyName,
                                         nsAString& aReturn)
{
  css::Declaration* decl = GetCSSDeclaration(PR_FALSE);

  aReturn.Truncate();
  if (decl && decl->GetValueIsImportant(aPropertyName)) {
    aReturn.AssignLiteral("important");
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetProperty(const nsAString& aPropertyName,
                                 const nsAString& aValue,
                                 const nsAString& aPriority)
{
  // In the common (and fast) cases we can use the property id
  nsCSSProperty propID = nsCSSProps::LookupProperty(aPropertyName);
  if (propID == eCSSProperty_UNKNOWN) {
    return NS_OK;
  }

  if (aValue.IsEmpty()) {
    // If the new value of the property is an empty string we remove the
    // property.
    // XXX this ignores the priority string, should it?
    return RemoveProperty(propID);
  }

  if (aPriority.IsEmpty()) {
    return ParsePropertyValue(propID, aValue, PR_FALSE);
  }

  if (aPriority.EqualsLiteral("important")) {
    return ParsePropertyValue(propID, aValue, PR_TRUE);
  }

  // XXX silent failure?
  return NS_OK;
}

NS_IMETHODIMP
nsDOMCSSDeclaration::RemoveProperty(const nsAString& aPropertyName,
                                    nsAString& aReturn)
{
  const nsCSSProperty propID = nsCSSProps::LookupProperty(aPropertyName);
  if (propID == eCSSProperty_UNKNOWN) {
    aReturn.Truncate();
    return NS_OK;
  }

  nsresult rv = GetPropertyValue(propID, aReturn);
  NS_ENSURE_SUCCESS(rv, rv);

  return RemoveProperty(propID);
}

nsresult
nsDOMCSSDeclaration::ParsePropertyValue(const nsCSSProperty aPropID,
                                        const nsAString& aPropValue,
                                        PRBool aIsImportant)
{
  css::Declaration* olddecl = GetCSSDeclaration(PR_TRUE);
  if (!olddecl) {
    return NS_ERROR_FAILURE;
  }

  nsresult result;
  nsRefPtr<css::Loader> cssLoader;
  nsCOMPtr<nsIURI> baseURI, sheetURI;
  nsCOMPtr<nsIPrincipal> sheetPrincipal;

  result = GetCSSParsingEnvironment(getter_AddRefs(sheetURI),
                                    getter_AddRefs(baseURI),
                                    getter_AddRefs(sheetPrincipal),
                                    getter_AddRefs(cssLoader));
  if (NS_FAILED(result)) {
    return result;
  }

  // For nsDOMCSSAttributeDeclaration, SetCSSDeclaration will lead to
  // Attribute setting code, which leads in turn to BeginUpdate.  We
  // need to start the update now so that the old rule doesn't get used
  // between when we mutate the declaration and when we set the new
  // rule (see stack in bug 209575).
  mozAutoDocConditionalContentUpdateBatch autoUpdate(DocToUpdate(), PR_TRUE);
  css::Declaration* decl = olddecl->EnsureMutable();

  nsCSSParser cssParser(cssLoader);
  PRBool changed;
  result = cssParser.ParseProperty(aPropID, aPropValue, sheetURI, baseURI,
                                   sheetPrincipal, decl, &changed,
                                   aIsImportant);
  if (NS_FAILED(result) || !changed) {
    if (decl != olddecl) {
      delete decl;
    }
    return result;
  }

  return SetCSSDeclaration(decl);
}

nsresult
nsDOMCSSDeclaration::RemoveProperty(const nsCSSProperty aPropID)
{
  css::Declaration* decl = GetCSSDeclaration(PR_FALSE);
  if (!decl) {
    return NS_OK; // no decl, so nothing to remove
  }

  // For nsDOMCSSAttributeDeclaration, SetCSSDeclaration will lead to
  // Attribute setting code, which leads in turn to BeginUpdate.  We
  // need to start the update now so that the old rule doesn't get used
  // between when we mutate the declaration and when we set the new
  // rule (see stack in bug 209575).
  mozAutoDocConditionalContentUpdateBatch autoUpdate(DocToUpdate(), PR_TRUE);

  decl = decl->EnsureMutable();
  decl->RemoveProperty(aPropID);
  return SetCSSDeclaration(decl);
}

// nsIDOMCSS2Properties

#define CSS_PROP_DOMPROP_PREFIXED(prop_) Moz ## prop_
#define CSS_PROP(name_, id_, method_, flags_, datastruct_, member_,          \
                 parsevariant_, kwtable_, stylestruct_, stylestructoffset_,  \
                 animtype_)                                                  \
  NS_IMETHODIMP                                                              \
  nsDOMCSSDeclaration::Get##method_(nsAString& aValue)                       \
  {                                                                          \
    return GetPropertyValue(eCSSProperty_##id_, aValue);                     \
  }                                                                          \
                                                                             \
  NS_IMETHODIMP                                                              \
  nsDOMCSSDeclaration::Set##method_(const nsAString& aValue)                 \
  {                                                                          \
    return SetPropertyValue(eCSSProperty_##id_, aValue);                     \
  }

#define CSS_PROP_LIST_EXCLUDE_INTERNAL
#define CSS_PROP_SHORTHAND(name_, id_, method_, flags_) \
  CSS_PROP(name_, id_, method_, flags_, X, X, X, X, X, X, X)
#include "nsCSSPropList.h"

// Aliases
CSS_PROP(X, opacity, MozOpacity, X, X, X, X, X, X, X, X)
CSS_PROP(X, outline, MozOutline, X, X, X, X, X, X, X, X)
CSS_PROP(X, outline_color, MozOutlineColor, X, X, X, X, X, X, X, X)
CSS_PROP(X, outline_style, MozOutlineStyle, X, X, X, X, X, X, X, X)
CSS_PROP(X, outline_width, MozOutlineWidth, X, X, X, X, X, X, X, X)
CSS_PROP(X, outline_offset, MozOutlineOffset, X, X, X, X, X, X, X, X)

#undef CSS_PROP_SHORTHAND
#undef CSS_PROP_LIST_EXCLUDE_INTERNAL
#undef CSS_PROP
#undef CSS_PROP_DOMPROP_PREFIXED
