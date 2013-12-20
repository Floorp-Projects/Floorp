/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* base class for DOM objects for element.style and cssStyleRule.style */

#include "nsDOMCSSDeclaration.h"

#include "nsCSSParser.h"
#include "nsCSSStyleSheet.h"
#include "mozilla/css/Rule.h"
#include "mozilla/css/Declaration.h"
#include "mozilla/dom/CSS2PropertiesBinding.h"
#include "nsCSSProps.h"
#include "nsCOMPtr.h"
#include "mozAutoDocUpdate.h"
#include "nsIURI.h"

using namespace mozilla;

nsDOMCSSDeclaration::~nsDOMCSSDeclaration()
{
}

/* virtual */ JSObject*
nsDOMCSSDeclaration::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return dom::CSS2PropertiesBinding::Wrap(aCx, aScope, this);
}

NS_INTERFACE_TABLE_HEAD(nsDOMCSSDeclaration)
  NS_INTERFACE_TABLE2(nsDOMCSSDeclaration,
                      nsICSSDeclaration,
                      nsIDOMCSSStyleDeclaration)
  NS_INTERFACE_TABLE_TO_MAP_SEGUE
NS_INTERFACE_MAP_END

NS_IMETHODIMP
nsDOMCSSDeclaration::GetPropertyValue(const nsCSSProperty aPropID,
                                      nsAString& aValue)
{
  NS_PRECONDITION(aPropID != eCSSProperty_UNKNOWN,
                  "Should never pass eCSSProperty_UNKNOWN around");

  css::Declaration* decl = GetCSSDeclaration(false);

  aValue.Truncate();
  if (decl) {
    decl->GetValue(aPropID, aValue);
  }
  return NS_OK;
}

// Length of the "var-" prefix of custom property names.
#define VAR_PREFIX_LENGTH 4

void
nsDOMCSSDeclaration::GetCustomPropertyValue(const nsAString& aPropertyName,
                                            nsAString& aValue)
{
  MOZ_ASSERT(Substring(aPropertyName,
                       0, VAR_PREFIX_LENGTH).EqualsLiteral("var-"));

  css::Declaration* decl = GetCSSDeclaration(false);
  if (!decl) {
    aValue.Truncate();
    return;
  }

  decl->GetVariableDeclaration(Substring(aPropertyName, VAR_PREFIX_LENGTH),
                               aValue);
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

  return ParsePropertyValue(aPropID, aValue, false);
}


NS_IMETHODIMP
nsDOMCSSDeclaration::GetCssText(nsAString& aCssText)
{
  css::Declaration* decl = GetCSSDeclaration(false);
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
  css::Declaration* olddecl = GetCSSDeclaration(true);
  if (!olddecl) {
    return NS_ERROR_FAILURE;
  }

  CSSParsingEnvironment env;
  GetCSSParsingEnvironment(env);
  if (!env.mPrincipal) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // For nsDOMCSSAttributeDeclaration, SetCSSDeclaration will lead to
  // Attribute setting code, which leads in turn to BeginUpdate.  We
  // need to start the update now so that the old rule doesn't get used
  // between when we mutate the declaration and when we set the new
  // rule (see stack in bug 209575).
  mozAutoDocConditionalContentUpdateBatch autoUpdate(DocToUpdate(), true);

  nsAutoPtr<css::Declaration> decl(new css::Declaration());
  decl->InitializeEmpty();
  nsCSSParser cssParser(env.mCSSLoader);
  bool changed;
  nsresult result = cssParser.ParseDeclarations(aCssText, env.mSheetURI,
                                                env.mBaseURI,
                                                env.mPrincipal, decl, &changed);
  if (NS_FAILED(result) || !changed) {
    return result;
  }

  return SetCSSDeclaration(decl.forget());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetLength(uint32_t* aLength)
{
  css::Declaration* decl = GetCSSDeclaration(false);

  if (decl) {
    *aLength = decl->Count();
  } else {
    *aLength = 0;
  }

  return NS_OK;
}

already_AddRefed<dom::CSSValue>
nsDOMCSSDeclaration::GetPropertyCSSValue(const nsAString& aPropertyName, ErrorResult& aRv)
{
  // We don't support CSSValue yet so we'll just return null...

  return nullptr;
}

void
nsDOMCSSDeclaration::IndexedGetter(uint32_t aIndex, bool& aFound, nsAString& aPropName)
{
  css::Declaration* decl = GetCSSDeclaration(false);
  aFound = decl && decl->GetNthProperty(aIndex, aPropName);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetPropertyValue(const nsAString& aPropertyName,
                                      nsAString& aReturn)
{
  const nsCSSProperty propID = nsCSSProps::LookupProperty(aPropertyName,
                                                          nsCSSProps::eEnabled);
  if (propID == eCSSProperty_UNKNOWN) {
    aReturn.Truncate();
    return NS_OK;
  }

  if (propID == eCSSPropertyExtra_variable) {
    GetCustomPropertyValue(aPropertyName, aReturn);
    return NS_OK;
  }

  return GetPropertyValue(propID, aReturn);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetPropertyPriority(const nsAString& aPropertyName,
                                         nsAString& aReturn)
{
  css::Declaration* decl = GetCSSDeclaration(false);

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
  nsCSSProperty propID = nsCSSProps::LookupProperty(aPropertyName,
                                                    nsCSSProps::eEnabled);
  if (propID == eCSSProperty_UNKNOWN) {
    return NS_OK;
  }

  if (aValue.IsEmpty()) {
    // If the new value of the property is an empty string we remove the
    // property.
    // XXX this ignores the priority string, should it?
    if (propID == eCSSPropertyExtra_variable) {
      return RemoveCustomProperty(aPropertyName);
    }
    return RemoveProperty(propID);
  }

  bool important;
  if (aPriority.IsEmpty()) {
    important = false;
  } else if (aPriority.EqualsLiteral("important")) {
    important = true;
  } else {
    // XXX silent failure?
    return NS_OK;
  }

  if (propID == eCSSPropertyExtra_variable) {
    return ParseCustomPropertyValue(aPropertyName, aValue, important);
  }
  return ParsePropertyValue(propID, aValue, important);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::RemoveProperty(const nsAString& aPropertyName,
                                    nsAString& aReturn)
{
  const nsCSSProperty propID = nsCSSProps::LookupProperty(aPropertyName,
                                                          nsCSSProps::eEnabled);
  if (propID == eCSSProperty_UNKNOWN) {
    aReturn.Truncate();
    return NS_OK;
  }

  if (propID == eCSSPropertyExtra_variable) {
    RemoveCustomProperty(aPropertyName);
    return NS_OK;
  }

  nsresult rv = GetPropertyValue(propID, aReturn);
  NS_ENSURE_SUCCESS(rv, rv);

  return RemoveProperty(propID);
}

/* static */ void
nsDOMCSSDeclaration::GetCSSParsingEnvironmentForRule(css::Rule* aRule,
                                                     CSSParsingEnvironment& aCSSParseEnv)
{
  nsIStyleSheet* sheet = aRule ? aRule->GetStyleSheet() : nullptr;
  nsRefPtr<nsCSSStyleSheet> cssSheet(do_QueryObject(sheet));
  if (!cssSheet) {
    aCSSParseEnv.mPrincipal = nullptr;
    return;
  }

  nsIDocument* document = sheet->GetOwningDocument();
  aCSSParseEnv.mSheetURI = sheet->GetSheetURI();
  aCSSParseEnv.mBaseURI = sheet->GetBaseURI();
  aCSSParseEnv.mPrincipal = cssSheet->Principal();
  aCSSParseEnv.mCSSLoader = document ? document->CSSLoader() : nullptr;
}

nsresult
nsDOMCSSDeclaration::ParsePropertyValue(const nsCSSProperty aPropID,
                                        const nsAString& aPropValue,
                                        bool aIsImportant)
{
  css::Declaration* olddecl = GetCSSDeclaration(true);
  if (!olddecl) {
    return NS_ERROR_FAILURE;
  }

  CSSParsingEnvironment env;
  GetCSSParsingEnvironment(env);
  if (!env.mPrincipal) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // For nsDOMCSSAttributeDeclaration, SetCSSDeclaration will lead to
  // Attribute setting code, which leads in turn to BeginUpdate.  We
  // need to start the update now so that the old rule doesn't get used
  // between when we mutate the declaration and when we set the new
  // rule (see stack in bug 209575).
  mozAutoDocConditionalContentUpdateBatch autoUpdate(DocToUpdate(), true);
  css::Declaration* decl = olddecl->EnsureMutable();

  nsCSSParser cssParser(env.mCSSLoader);
  bool changed;
  nsresult result = cssParser.ParseProperty(aPropID, aPropValue, env.mSheetURI,
                                            env.mBaseURI, env.mPrincipal, decl,
                                            &changed, aIsImportant);
  if (NS_FAILED(result) || !changed) {
    if (decl != olddecl) {
      delete decl;
    }
    return result;
  }

  return SetCSSDeclaration(decl);
}

nsresult
nsDOMCSSDeclaration::ParseCustomPropertyValue(const nsAString& aPropertyName,
                                              const nsAString& aPropValue,
                                              bool aIsImportant)
{
  MOZ_ASSERT(nsCSSProps::IsCustomPropertyName(aPropertyName));

  css::Declaration* olddecl = GetCSSDeclaration(true);
  if (!olddecl) {
    return NS_ERROR_FAILURE;
  }

  CSSParsingEnvironment env;
  GetCSSParsingEnvironment(env);
  if (!env.mPrincipal) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // For nsDOMCSSAttributeDeclaration, SetCSSDeclaration will lead to
  // Attribute setting code, which leads in turn to BeginUpdate.  We
  // need to start the update now so that the old rule doesn't get used
  // between when we mutate the declaration and when we set the new
  // rule (see stack in bug 209575).
  mozAutoDocConditionalContentUpdateBatch autoUpdate(DocToUpdate(), true);
  css::Declaration* decl = olddecl->EnsureMutable();

  nsCSSParser cssParser(env.mCSSLoader);
  bool changed;
  nsresult result = cssParser.ParseVariable(Substring(aPropertyName,
                                                      VAR_PREFIX_LENGTH),
                                            aPropValue, env.mSheetURI,
                                            env.mBaseURI, env.mPrincipal, decl,
                                            &changed, aIsImportant);
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
  css::Declaration* decl = GetCSSDeclaration(false);
  if (!decl) {
    return NS_OK; // no decl, so nothing to remove
  }

  // For nsDOMCSSAttributeDeclaration, SetCSSDeclaration will lead to
  // Attribute setting code, which leads in turn to BeginUpdate.  We
  // need to start the update now so that the old rule doesn't get used
  // between when we mutate the declaration and when we set the new
  // rule (see stack in bug 209575).
  mozAutoDocConditionalContentUpdateBatch autoUpdate(DocToUpdate(), true);

  decl = decl->EnsureMutable();
  decl->RemoveProperty(aPropID);
  return SetCSSDeclaration(decl);
}

nsresult
nsDOMCSSDeclaration::RemoveCustomProperty(const nsAString& aPropertyName)
{
  MOZ_ASSERT(Substring(aPropertyName,
                       0, VAR_PREFIX_LENGTH).EqualsLiteral("var-"));

  css::Declaration* decl = GetCSSDeclaration(false);
  if (!decl) {
    return NS_OK; // no decl, so nothing to remove
  }

  // For nsDOMCSSAttributeDeclaration, SetCSSDeclaration will lead to
  // Attribute setting code, which leads in turn to BeginUpdate.  We
  // need to start the update now so that the old rule doesn't get used
  // between when we mutate the declaration and when we set the new
  // rule (see stack in bug 209575).
  mozAutoDocConditionalContentUpdateBatch autoUpdate(DocToUpdate(), true);

  decl = decl->EnsureMutable();
  decl->RemoveVariableDeclaration(Substring(aPropertyName, VAR_PREFIX_LENGTH));
  return SetCSSDeclaration(decl);
}
