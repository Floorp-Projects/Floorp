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

  aValue.Truncate();
  if (DeclarationBlock* decl = GetCSSDeclaration(eOperation_Read)) {
    decl->GetPropertyValueByID(aPropID, aValue);
  }
  return NS_OK;
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
    return RemovePropertyInternal(aPropID);
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

  // For nsDOMCSSAttributeDeclaration, SetCSSDeclaration will lead to
  // Attribute setting code, which leads in turn to BeginUpdate.  We
  // need to start the update now so that the old rule doesn't get used
  // between when we mutate the declaration and when we set the new
  // rule (see stack in bug 209575).
  mozAutoDocConditionalContentUpdateBatch autoUpdate(DocToUpdate(), true);

  RefPtr<DeclarationBlock> newdecl;
  if (olddecl->IsServo()) {
    ServoCSSParsingEnvironment servoEnv = GetServoCSSParsingEnvironment();
    if (!servoEnv.mUrlExtraData) {
      return NS_ERROR_NOT_AVAILABLE;
    }

    newdecl = ServoDeclarationBlock::FromCssText(aCssText, servoEnv.mUrlExtraData,
                                                 servoEnv.mCompatMode);
  } else {
    CSSParsingEnvironment geckoEnv;
    GetCSSParsingEnvironment(geckoEnv);
    if (!geckoEnv.mPrincipal) {
      return NS_ERROR_NOT_AVAILABLE;
    }

    RefPtr<css::Declaration> decl(new css::Declaration());
    decl->InitializeEmpty();
    nsCSSParser cssParser(geckoEnv.mCSSLoader);
    bool changed;
    nsresult result = cssParser.ParseDeclarations(aCssText, geckoEnv.mSheetURI,
                                                  geckoEnv.mBaseURI, geckoEnv.mPrincipal,
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
  aReturn.Truncate();
  if (DeclarationBlock* decl = GetCSSDeclaration(eOperation_Read)) {
    decl->GetPropertyValue(aPropertyName, aReturn);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetAuthoredPropertyValue(const nsAString& aPropertyName,
                                              nsAString& aReturn)
{
  if (DeclarationBlock* decl = GetCSSDeclaration(eOperation_Read)) {
    decl->GetAuthoredPropertyValue(aPropertyName, aReturn);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetPropertyPriority(const nsAString& aPropertyName,
                                         nsAString& aReturn)
{
  DeclarationBlock* decl = GetCSSDeclaration(eOperation_Read);

  aReturn.Truncate();
  if (decl && decl->GetPropertyIsImportant(aPropertyName)) {
    aReturn.AssignLiteral("important");
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetProperty(const nsAString& aPropertyName,
                                 const nsAString& aValue,
                                 const nsAString& aPriority)
{
  if (aValue.IsEmpty()) {
    // If the new value of the property is an empty string we remove the
    // property.
    // XXX this ignores the priority string, should it?
    return RemovePropertyInternal(aPropertyName);
  }

  // In the common (and fast) cases we can use the property id
  nsCSSPropertyID propID =
    nsCSSProps::LookupProperty(aPropertyName, CSSEnabledState::eForAllContent);
  if (propID == eCSSProperty_UNKNOWN) {
    return NS_OK;
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
  nsresult rv = GetPropertyValue(aPropertyName, aReturn);
  NS_ENSURE_SUCCESS(rv, rv);
  return RemovePropertyInternal(aPropertyName);
}

/* static */ void
nsDOMCSSDeclaration::GetCSSParsingEnvironmentForRule(css::Rule* aRule,
                                                     CSSParsingEnvironment& aCSSParseEnv)
{
  StyleSheet* sheet = aRule ? aRule->GetStyleSheet() : nullptr;
  if (!sheet) {
    aCSSParseEnv.mPrincipal = nullptr;
    return;
  }

  nsIDocument* document = sheet->GetAssociatedDocument();
  aCSSParseEnv.mSheetURI = sheet->GetSheetURI();
  aCSSParseEnv.mBaseURI = sheet->GetBaseURI();
  aCSSParseEnv.mPrincipal = sheet->Principal();
  aCSSParseEnv.mCSSLoader = document ? document->CSSLoader() : nullptr;
}

/* static */ nsDOMCSSDeclaration::ServoCSSParsingEnvironment
nsDOMCSSDeclaration::GetServoCSSParsingEnvironmentForRule(const css::Rule* aRule)
{
  StyleSheet* sheet = aRule ? aRule->GetStyleSheet() : nullptr;
  if (!sheet) {
    return { nullptr, eCompatibility_FullStandards };
  }

  if (nsIDocument* document = aRule->GetDocument()) {
    return {
      sheet->AsServo()->URLData(),
      document->GetCompatibilityMode(),
    };
  }

  return {
    sheet->AsServo()->URLData(),
    eCompatibility_FullStandards,
  };
}

template<typename GeckoFunc, typename ServoFunc>
nsresult
nsDOMCSSDeclaration::ModifyDeclaration(GeckoFunc aGeckoFunc,
                                       ServoFunc aServoFunc)
{
  DeclarationBlock* olddecl = GetCSSDeclaration(eOperation_Modify);
  if (!olddecl) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // For nsDOMCSSAttributeDeclaration, SetCSSDeclaration will lead to
  // Attribute setting code, which leads in turn to BeginUpdate.  We
  // need to start the update now so that the old rule doesn't get used
  // between when we mutate the declaration and when we set the new
  // rule (see stack in bug 209575).
  mozAutoDocConditionalContentUpdateBatch autoUpdate(DocToUpdate(), true);
  RefPtr<DeclarationBlock> decl;
  if (olddecl->IsServo() && !olddecl->IsDirty()) {
    // In stylo, the old DeclarationBlock is stored in element's rule node tree
    // directly, to avoid new values replacing the DeclarationBlock in the tree
    // directly, we need to copy the old one here if we haven't yet copied.
    // As a result the new value does not replace rule node tree until traversal
    // happens.
    decl = olddecl->Clone();
  } else {
    decl = olddecl->EnsureMutable();
  }

  bool changed;
  if (decl->IsGecko()) {
    CSSParsingEnvironment geckoEnv;
    GetCSSParsingEnvironment(geckoEnv);
    if (!geckoEnv.mPrincipal) {
      return NS_ERROR_NOT_AVAILABLE;
    }

    aGeckoFunc(decl->AsGecko(), geckoEnv, &changed);
  } else {
    ServoCSSParsingEnvironment servoEnv = GetServoCSSParsingEnvironment();
    if (!servoEnv.mUrlExtraData) {
      return NS_ERROR_NOT_AVAILABLE;
    }

    changed = aServoFunc(decl->AsServo(), servoEnv);
  }
  if (!changed) {
    // Parsing failed -- but we don't throw an exception for that.
    return NS_OK;
  }

  return SetCSSDeclaration(decl);
}

nsresult
nsDOMCSSDeclaration::ParsePropertyValue(const nsCSSPropertyID aPropID,
                                        const nsAString& aPropValue,
                                        bool aIsImportant)
{
  return ModifyDeclaration(
    [&](css::Declaration* decl, CSSParsingEnvironment& env, bool* changed) {
      nsCSSParser cssParser(env.mCSSLoader);
      cssParser.ParseProperty(aPropID, aPropValue,
                              env.mSheetURI, env.mBaseURI, env.mPrincipal,
                              decl, changed, aIsImportant);
    },
    [&](ServoDeclarationBlock* decl, ServoCSSParsingEnvironment& env) {
      NS_ConvertUTF16toUTF8 value(aPropValue);
      return Servo_DeclarationBlock_SetPropertyById(
        decl->Raw(), aPropID, &value, aIsImportant, env.mUrlExtraData,
        ParsingMode::Default, env.mCompatMode);
    });
}

nsresult
nsDOMCSSDeclaration::ParseCustomPropertyValue(const nsAString& aPropertyName,
                                              const nsAString& aPropValue,
                                              bool aIsImportant)
{
  MOZ_ASSERT(nsCSSProps::IsCustomPropertyName(aPropertyName));
  return ModifyDeclaration(
    [&](css::Declaration* decl, CSSParsingEnvironment& env, bool* changed) {
      nsCSSParser cssParser(env.mCSSLoader);
      auto propName = Substring(aPropertyName, CSS_CUSTOM_NAME_PREFIX_LENGTH);
      cssParser.ParseVariable(propName, aPropValue, env.mSheetURI,
                              env.mBaseURI, env.mPrincipal, decl,
                              changed, aIsImportant);
    },
    [&](ServoDeclarationBlock* decl, ServoCSSParsingEnvironment& env) {
      NS_ConvertUTF16toUTF8 property(aPropertyName);
      NS_ConvertUTF16toUTF8 value(aPropValue);
      return Servo_DeclarationBlock_SetProperty(
        decl->Raw(), &property, &value, aIsImportant, env.mUrlExtraData,
        ParsingMode::Default, env.mCompatMode);
    });
}

nsresult
nsDOMCSSDeclaration::RemovePropertyInternal(nsCSSPropertyID aPropID)
{
  DeclarationBlock* olddecl = GetCSSDeclaration(eOperation_RemoveProperty);
  if (!olddecl) {
    return NS_OK; // no decl, so nothing to remove
  }

  // For nsDOMCSSAttributeDeclaration, SetCSSDeclaration will lead to
  // Attribute setting code, which leads in turn to BeginUpdate.  We
  // need to start the update now so that the old rule doesn't get used
  // between when we mutate the declaration and when we set the new
  // rule (see stack in bug 209575).
  mozAutoDocConditionalContentUpdateBatch autoUpdate(DocToUpdate(), true);

  RefPtr<DeclarationBlock> decl = olddecl->EnsureMutable();
  decl->RemovePropertyByID(aPropID);
  return SetCSSDeclaration(decl);
}

nsresult
nsDOMCSSDeclaration::RemovePropertyInternal(const nsAString& aPropertyName)
{
  DeclarationBlock* olddecl = GetCSSDeclaration(eOperation_RemoveProperty);
  if (!olddecl) {
    return NS_OK; // no decl, so nothing to remove
  }

  // For nsDOMCSSAttributeDeclaration, SetCSSDeclaration will lead to
  // Attribute setting code, which leads in turn to BeginUpdate.  We
  // need to start the update now so that the old rule doesn't get used
  // between when we mutate the declaration and when we set the new
  // rule (see stack in bug 209575).
  mozAutoDocConditionalContentUpdateBatch autoUpdate(DocToUpdate(), true);

  RefPtr<DeclarationBlock> decl = olddecl->EnsureMutable();
  decl->RemoveProperty(aPropertyName);
  return SetCSSDeclaration(decl);
}
