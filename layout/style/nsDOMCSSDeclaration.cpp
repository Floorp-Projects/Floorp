/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* base class for DOM objects for element.style and cssStyleRule.style */

#include "nsDOMCSSDeclaration.h"

#include "nsCSSParser.h"
#include "mozilla/DeclarationBlockInlines.h"
#include "mozilla/StyleSheetInlines.h"
#include "mozilla/css/Rule.h"
#include "mozilla/DeclarationBlockInlines.h"
#include "mozilla/dom/CSS2PropertiesBinding.h"
#include "nsCSSProps.h"
#include "nsCOMPtr.h"
#include "mozAutoDocUpdate.h"
#include "nsIURI.h"
#include "mozilla/dom/BindingUtils.h"
#include "nsContentUtils.h"
#include "nsQueryObject.h"
#include "mozilla/layers/ScrollLinkedEffectDetector.h"

using namespace mozilla;

nsDOMCSSDeclaration::~nsDOMCSSDeclaration()
{
}

/* virtual */ JSObject*
nsDOMCSSDeclaration::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return dom::CSS2PropertiesBinding::Wrap(aCx, this, aGivenProto);
}

NS_INTERFACE_TABLE_HEAD(nsDOMCSSDeclaration)
  NS_INTERFACE_TABLE(nsDOMCSSDeclaration,
                     nsICSSDeclaration,
                     nsIDOMCSSStyleDeclaration)
  NS_INTERFACE_TABLE_TO_MAP_SEGUE
NS_INTERFACE_MAP_END

NS_IMETHODIMP
nsDOMCSSDeclaration::GetPropertyValue(const nsCSSPropertyID aPropID,
                                      nsAString& aValue)
{
  NS_PRECONDITION(aPropID != eCSSProperty_UNKNOWN,
                  "Should never pass eCSSProperty_UNKNOWN around");

  css::Declaration* decl = GetCSSDeclaration(eOperation_Read)->AsGecko();

  aValue.Truncate();
  if (decl) {
    decl->GetValue(aPropID, aValue);
  }
  return NS_OK;
}

void
nsDOMCSSDeclaration::GetCustomPropertyValue(const nsAString& aPropertyName,
                                            nsAString& aValue)
{
  MOZ_ASSERT(Substring(aPropertyName, 0,
                       CSS_CUSTOM_NAME_PREFIX_LENGTH).EqualsLiteral("--"));

  css::Declaration* decl = GetCSSDeclaration(eOperation_Read)->AsGecko();
  if (!decl) {
    aValue.Truncate();
    return;
  }

  decl->GetVariableDeclaration(Substring(aPropertyName,
                                         CSS_CUSTOM_NAME_PREFIX_LENGTH),
                               aValue);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetPropertyValue(const nsCSSPropertyID aPropID,
                                      const nsAString& aValue)
{
  switch (aPropID) {
    case eCSSProperty_background_position:
    case eCSSProperty_background_position_x:
    case eCSSProperty_background_position_y:
    case eCSSProperty_transform:
    case eCSSProperty_top:
    case eCSSProperty_left:
    case eCSSProperty_bottom:
    case eCSSProperty_right:
    case eCSSProperty_margin:
    case eCSSProperty_margin_top:
    case eCSSProperty_margin_left:
    case eCSSProperty_margin_bottom:
    case eCSSProperty_margin_right:
    case eCSSProperty_margin_inline_start:
    case eCSSProperty_margin_inline_end:
    case eCSSProperty_margin_block_start:
    case eCSSProperty_margin_block_end:
      mozilla::layers::ScrollLinkedEffectDetector::PositioningPropertyMutated();
      break;
    default:
      break;
  }

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
  DeclarationBlock* decl = GetCSSDeclaration(eOperation_Read);
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
  DeclarationBlock* olddecl = GetCSSDeclaration(eOperation_Modify);
  if (!olddecl) {
    return NS_ERROR_NOT_AVAILABLE;
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

  RefPtr<DeclarationBlock> newdecl;
  if (olddecl->IsServo()) {
    newdecl = ServoDeclarationBlock::FromCssText(aCssText);
  } else {
    RefPtr<css::Declaration> decl(new css::Declaration());
    decl->InitializeEmpty();
    nsCSSParser cssParser(env.mCSSLoader);
    bool changed;
    nsresult result = cssParser.ParseDeclarations(aCssText, env.mSheetURI,
                                                  env.mBaseURI, env.mPrincipal,
                                                  decl, &changed);
    if (NS_FAILED(result) || !changed) {
      return result;
    }
    newdecl = decl.forget();
  }

  return SetCSSDeclaration(newdecl);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetLength(uint32_t* aLength)
{
  DeclarationBlock* decl = GetCSSDeclaration(eOperation_Read);

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
  DeclarationBlock* decl = GetCSSDeclaration(eOperation_Read);
  aFound = decl && decl->GetNthProperty(aIndex, aPropName);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetPropertyValue(const nsAString& aPropertyName,
                                      nsAString& aReturn)
{
  const nsCSSPropertyID propID =
    nsCSSProps::LookupProperty(aPropertyName, CSSEnabledState::eForAllContent);
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
nsDOMCSSDeclaration::GetAuthoredPropertyValue(const nsAString& aPropertyName,
                                              nsAString& aReturn)
{
  const nsCSSPropertyID propID =
    nsCSSProps::LookupProperty(aPropertyName, CSSEnabledState::eForAllContent);
  if (propID == eCSSProperty_UNKNOWN) {
    aReturn.Truncate();
    return NS_OK;
  }

  if (propID == eCSSPropertyExtra_variable) {
    GetCustomPropertyValue(aPropertyName, aReturn);
    return NS_OK;
  }

  css::Declaration* decl = GetCSSDeclaration(eOperation_Read)->AsGecko();
  if (!decl) {
    return NS_ERROR_FAILURE;
  }

  decl->GetAuthoredValue(propID, aReturn);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetPropertyPriority(const nsAString& aPropertyName,
                                         nsAString& aReturn)
{
  css::Declaration* decl = GetCSSDeclaration(eOperation_Read)->AsGecko();

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
  nsCSSPropertyID propID =
    nsCSSProps::LookupProperty(aPropertyName, CSSEnabledState::eForAllContent);
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
  const nsCSSPropertyID propID =
    nsCSSProps::LookupProperty(aPropertyName, CSSEnabledState::eForAllContent);
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
  CSSStyleSheet* sheet = aRule ? aRule->GetStyleSheet() : nullptr;
  if (!sheet) {
    aCSSParseEnv.mPrincipal = nullptr;
    return;
  }

  nsIDocument* document = sheet->GetOwningDocument();
  aCSSParseEnv.mSheetURI = sheet->GetSheetURI();
  aCSSParseEnv.mBaseURI = sheet->GetBaseURI();
  aCSSParseEnv.mPrincipal = sheet->Principal();
  aCSSParseEnv.mCSSLoader = document ? document->CSSLoader() : nullptr;
}

nsresult
nsDOMCSSDeclaration::ParsePropertyValue(const nsCSSPropertyID aPropID,
                                        const nsAString& aPropValue,
                                        bool aIsImportant)
{
  css::Declaration* olddecl = GetCSSDeclaration(eOperation_Modify)->AsGecko();
  if (!olddecl) {
    return NS_ERROR_NOT_AVAILABLE;
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
  RefPtr<css::Declaration> decl = olddecl->EnsureMutable();

  nsCSSParser cssParser(env.mCSSLoader);
  bool changed;
  cssParser.ParseProperty(aPropID, aPropValue, env.mSheetURI, env.mBaseURI,
                          env.mPrincipal, decl, &changed, aIsImportant);
  if (!changed) {
    // Parsing failed -- but we don't throw an exception for that.
    return NS_OK;
  }

  return SetCSSDeclaration(decl);
}

nsresult
nsDOMCSSDeclaration::ParseCustomPropertyValue(const nsAString& aPropertyName,
                                              const nsAString& aPropValue,
                                              bool aIsImportant)
{
  MOZ_ASSERT(nsCSSProps::IsCustomPropertyName(aPropertyName));

  css::Declaration* olddecl = GetCSSDeclaration(eOperation_Modify)->AsGecko();
  if (!olddecl) {
    return NS_ERROR_NOT_AVAILABLE;
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
  RefPtr<css::Declaration> decl = olddecl->EnsureMutable();

  nsCSSParser cssParser(env.mCSSLoader);
  bool changed;
  cssParser.ParseVariable(Substring(aPropertyName,
                                    CSS_CUSTOM_NAME_PREFIX_LENGTH),
                          aPropValue, env.mSheetURI,
                          env.mBaseURI, env.mPrincipal, decl,
                          &changed, aIsImportant);
  if (!changed) {
    // Parsing failed -- but we don't throw an exception for that.
    return NS_OK;
  }

  return SetCSSDeclaration(decl);
}

nsresult
nsDOMCSSDeclaration::RemoveProperty(const nsCSSPropertyID aPropID)
{
  css::Declaration* olddecl =
    GetCSSDeclaration(eOperation_RemoveProperty)->AsGecko();
  if (!olddecl) {
    return NS_OK; // no decl, so nothing to remove
  }

  // For nsDOMCSSAttributeDeclaration, SetCSSDeclaration will lead to
  // Attribute setting code, which leads in turn to BeginUpdate.  We
  // need to start the update now so that the old rule doesn't get used
  // between when we mutate the declaration and when we set the new
  // rule (see stack in bug 209575).
  mozAutoDocConditionalContentUpdateBatch autoUpdate(DocToUpdate(), true);

  RefPtr<css::Declaration> decl = olddecl->EnsureMutable();
  decl->RemoveProperty(aPropID);
  return SetCSSDeclaration(decl);
}

nsresult
nsDOMCSSDeclaration::RemoveCustomProperty(const nsAString& aPropertyName)
{
  MOZ_ASSERT(Substring(aPropertyName, 0,
                       CSS_CUSTOM_NAME_PREFIX_LENGTH).EqualsLiteral("--"));

  css::Declaration* olddecl =
    GetCSSDeclaration(eOperation_RemoveProperty)->AsGecko();
  if (!olddecl) {
    return NS_OK; // no decl, so nothing to remove
  }

  // For nsDOMCSSAttributeDeclaration, SetCSSDeclaration will lead to
  // Attribute setting code, which leads in turn to BeginUpdate.  We
  // need to start the update now so that the old rule doesn't get used
  // between when we mutate the declaration and when we set the new
  // rule (see stack in bug 209575).
  mozAutoDocConditionalContentUpdateBatch autoUpdate(DocToUpdate(), true);

  RefPtr<css::Declaration> decl = olddecl->EnsureMutable();
  decl->RemoveVariableDeclaration(Substring(aPropertyName,
                                            CSS_CUSTOM_NAME_PREFIX_LENGTH));
  return SetCSSDeclaration(decl);
}
