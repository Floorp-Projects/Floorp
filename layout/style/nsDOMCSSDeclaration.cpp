/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* base class for DOM objects for element.style and cssStyleRule.style */

#include "nsDOMCSSDeclaration.h"

#include "mozilla/DeclarationBlock.h"
#include "mozilla/StyleSheetInlines.h"
#include "mozilla/css/Rule.h"
#include "mozilla/dom/CSS2PropertiesBinding.h"
#include "mozilla/dom/MutationEventBinding.h"
#include "nsCSSProps.h"
#include "nsCOMPtr.h"
#include "mozAutoDocUpdate.h"
#include "nsIURI.h"
#include "mozilla/dom/BindingUtils.h"
#include "nsContentUtils.h"
#include "nsQueryObject.h"
#include "mozilla/layers/ScrollLinkedEffectDetector.h"

using namespace mozilla;

nsDOMCSSDeclaration::~nsDOMCSSDeclaration() = default;

/* virtual */ JSObject*
nsDOMCSSDeclaration::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return dom::CSS2Properties_Binding::Wrap(aCx, this, aGivenProto);
}

NS_IMPL_QUERY_INTERFACE(nsDOMCSSDeclaration,
                        nsICSSDeclaration)

nsresult
nsDOMCSSDeclaration::GetPropertyValue(const nsCSSPropertyID aPropID,
                                      nsAString& aValue)
{
  MOZ_ASSERT(aPropID != eCSSProperty_UNKNOWN,
             "Should never pass eCSSProperty_UNKNOWN around");

  aValue.Truncate();
  if (DeclarationBlock* decl =
        GetOrCreateCSSDeclaration(eOperation_Read, nullptr)) {
    decl->GetPropertyValueByID(aPropID, aValue);
  }
  return NS_OK;
}

nsresult
nsDOMCSSDeclaration::SetPropertyValue(const nsCSSPropertyID aPropID,
                                      const nsAString& aValue,
                                      nsIPrincipal* aSubjectPrincipal)
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

  return ParsePropertyValue(aPropID, aValue, false, aSubjectPrincipal);
}


void
nsDOMCSSDeclaration::GetCssText(nsAString& aCssText)
{
  DeclarationBlock* decl = GetOrCreateCSSDeclaration(eOperation_Read, nullptr);
  aCssText.Truncate();

  if (decl) {
    decl->ToString(aCssText);
  }
}

void
nsDOMCSSDeclaration::SetCssText(const nsAString& aCssText,
                                nsIPrincipal* aSubjectPrincipal,
                                ErrorResult& aRv)
{
  // We don't need to *do* anything with the old declaration, but we need
  // to ensure that it exists, or else SetCSSDeclaration may crash.
  RefPtr<DeclarationBlock> created;
  DeclarationBlock* olddecl =
    GetOrCreateCSSDeclaration(eOperation_Modify, getter_AddRefs(created));
  if (!olddecl) {
    aRv.Throw(NS_ERROR_NOT_AVAILABLE);
    return;
  }

  // For nsDOMCSSAttributeDeclaration, SetCSSDeclaration will lead to
  // Attribute setting code, which leads in turn to BeginUpdate.  We
  // need to start the update now so that the old rule doesn't get used
  // between when we mutate the declaration and when we set the new
  // rule (see stack in bug 209575).
  mozAutoDocUpdate autoUpdate(DocToUpdate(), true);
  DeclarationBlockMutationClosure closure = {};
  MutationClosureData closureData;
  GetPropertyChangeClosure(&closure, &closureData);

  ParsingEnvironment servoEnv =
    GetParsingEnvironment(aSubjectPrincipal);
  if (!servoEnv.mUrlExtraData) {
    if (created) {
      // In case we can't set a new declaration, but one was
      // created for the old one, we need to set the old declaration to
      // get right style attribute handling.
      SetCSSDeclaration(olddecl, &closureData);
    }
    aRv.Throw(NS_ERROR_NOT_AVAILABLE);
    return;
  }

  // Need to special case closure calling here, since parsing css text
  // doesn't modify any existing declaration and that is why the callback isn't
  // called implicitly.
  if (closureData.mClosure) {
    closureData.mClosure(&closureData);
  }

  RefPtr<DeclarationBlock> newdecl =
    DeclarationBlock::FromCssText(aCssText, servoEnv.mUrlExtraData,
                                  servoEnv.mCompatMode, servoEnv.mLoader);

  aRv = SetCSSDeclaration(newdecl, &closureData);
}

uint32_t
nsDOMCSSDeclaration::Length()
{
  DeclarationBlock* decl = GetOrCreateCSSDeclaration(eOperation_Read, nullptr);

  if (decl) {
    return decl->Count();
  }

  return 0;
}

void
nsDOMCSSDeclaration::IndexedGetter(uint32_t aIndex, bool& aFound, nsAString& aPropName)
{
  DeclarationBlock* decl = GetOrCreateCSSDeclaration(eOperation_Read, nullptr);
  aFound = decl && decl->GetNthProperty(aIndex, aPropName);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetPropertyValue(const nsAString& aPropertyName,
                                      nsAString& aReturn)
{
  aReturn.Truncate();
  if (DeclarationBlock* decl =
        GetOrCreateCSSDeclaration(eOperation_Read, nullptr)) {
    decl->GetPropertyValue(aPropertyName, aReturn);
  }
  return NS_OK;
}

void
nsDOMCSSDeclaration::GetPropertyPriority(const nsAString& aPropertyName,
                                         nsAString& aPriority)
{
  DeclarationBlock* decl = GetOrCreateCSSDeclaration(eOperation_Read, nullptr);

  aPriority.Truncate();
  if (decl && decl->GetPropertyIsImportant(aPropertyName)) {
    aPriority.AssignLiteral("important");
  }
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetProperty(const nsAString& aPropertyName,
                                 const nsAString& aValue,
                                 const nsAString& aPriority,
                                 nsIPrincipal* aSubjectPrincipal)
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
    return ParseCustomPropertyValue(aPropertyName, aValue, important,
                                    aSubjectPrincipal);
  }
  return ParsePropertyValue(propID, aValue, important, aSubjectPrincipal);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::RemoveProperty(const nsAString& aPropertyName,
                                    nsAString& aReturn)
{
  nsresult rv = GetPropertyValue(aPropertyName, aReturn);
  NS_ENSURE_SUCCESS(rv, rv);
  return RemovePropertyInternal(aPropertyName);
}

/* static */ nsDOMCSSDeclaration::ParsingEnvironment
nsDOMCSSDeclaration::GetParsingEnvironmentForRule(const css::Rule* aRule)
{
  StyleSheet* sheet = aRule ? aRule->GetStyleSheet() : nullptr;
  if (!sheet) {
    return { nullptr, eCompatibility_FullStandards, nullptr };
  }

  if (nsIDocument* document = sheet->GetAssociatedDocument()) {
    return {
      sheet->URLData(),
      document->GetCompatibilityMode(),
      document->CSSLoader(),
    };
  }

  return {
    sheet->URLData(),
    eCompatibility_FullStandards,
    nullptr,
  };
}

template<typename Func>
nsresult
nsDOMCSSDeclaration::ModifyDeclaration(nsIPrincipal* aSubjectPrincipal,
                                       MutationClosureData* aClosureData,
                                       Func aFunc)
{
  RefPtr<DeclarationBlock> created;
  DeclarationBlock* olddecl =
    GetOrCreateCSSDeclaration(eOperation_Modify, getter_AddRefs(created));
  if (!olddecl) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // For nsDOMCSSAttributeDeclaration, SetCSSDeclaration will lead to
  // Attribute setting code, which leads in turn to BeginUpdate.  We
  // need to start the update now so that the old rule doesn't get used
  // between when we mutate the declaration and when we set the new
  // rule (see stack in bug 209575).
  mozAutoDocUpdate autoUpdate(DocToUpdate(), true);
  RefPtr<DeclarationBlock> decl = olddecl->EnsureMutable();

  bool changed;
  ParsingEnvironment servoEnv =
    GetParsingEnvironment(aSubjectPrincipal);
  if (!servoEnv.mUrlExtraData) {
    if (created) {
      // In case we can't set a new declaration, but one was
      // created for the old one, we need to set the old declaration to
      // get right style attribute handling.
      SetCSSDeclaration(olddecl, aClosureData);
    }
    return NS_ERROR_NOT_AVAILABLE;
  }

  changed = aFunc(decl, servoEnv);

  if (!changed) {
    if (created) {
      // See comment above about setting old declaration.
      SetCSSDeclaration(olddecl, aClosureData);
    }
    // Parsing failed -- but we don't throw an exception for that.
    return NS_OK;
  }

  return SetCSSDeclaration(decl, aClosureData);
}

nsresult
nsDOMCSSDeclaration::ParsePropertyValue(const nsCSSPropertyID aPropID,
                                        const nsAString& aPropValue,
                                        bool aIsImportant,
                                        nsIPrincipal* aSubjectPrincipal)
{
  DeclarationBlockMutationClosure closure = {};
  MutationClosureData closureData;
  GetPropertyChangeClosure(&closure, &closureData);

  return ModifyDeclaration(
    aSubjectPrincipal,
    &closureData,
    [&](DeclarationBlock* decl, ParsingEnvironment& env) {
      NS_ConvertUTF16toUTF8 value(aPropValue);
      return Servo_DeclarationBlock_SetPropertyById(
        decl->Raw(), aPropID, &value, aIsImportant, env.mUrlExtraData,
        ParsingMode::Default, env.mCompatMode, env.mLoader, closure);
    });
}

nsresult
nsDOMCSSDeclaration::ParseCustomPropertyValue(const nsAString& aPropertyName,
                                              const nsAString& aPropValue,
                                              bool aIsImportant,
                                              nsIPrincipal* aSubjectPrincipal)
{
  MOZ_ASSERT(nsCSSProps::IsCustomPropertyName(aPropertyName));

  DeclarationBlockMutationClosure closure = {};
  MutationClosureData closureData;
  GetPropertyChangeClosure(&closure, &closureData);

  return ModifyDeclaration(
    aSubjectPrincipal,
    &closureData,
    [&](DeclarationBlock* decl, ParsingEnvironment& env) {
      NS_ConvertUTF16toUTF8 property(aPropertyName);
      NS_ConvertUTF16toUTF8 value(aPropValue);
      return Servo_DeclarationBlock_SetProperty(
        decl->Raw(), &property, &value, aIsImportant, env.mUrlExtraData,
        ParsingMode::Default, env.mCompatMode, env.mLoader, closure);
    });
}

nsresult
nsDOMCSSDeclaration::RemovePropertyInternal(nsCSSPropertyID aPropID)
{
  DeclarationBlock* olddecl =
    GetOrCreateCSSDeclaration(eOperation_RemoveProperty, nullptr);
  if (!olddecl) {
    return NS_OK; // no decl, so nothing to remove
  }

  // For nsDOMCSSAttributeDeclaration, SetCSSDeclaration will lead to
  // Attribute setting code, which leads in turn to BeginUpdate.  We
  // need to start the update now so that the old rule doesn't get used
  // between when we mutate the declaration and when we set the new
  // rule (see stack in bug 209575).
  mozAutoDocUpdate autoUpdate(DocToUpdate(), true);

  DeclarationBlockMutationClosure closure = {};
  MutationClosureData closureData;
  GetPropertyChangeClosure(&closure, &closureData);

  RefPtr<DeclarationBlock> decl = olddecl->EnsureMutable();
  if (!decl->RemovePropertyByID(aPropID, closure)) {
    return NS_OK;
  }
  return SetCSSDeclaration(decl, &closureData);
}

nsresult
nsDOMCSSDeclaration::RemovePropertyInternal(const nsAString& aPropertyName)
{
  DeclarationBlock* olddecl =
    GetOrCreateCSSDeclaration(eOperation_RemoveProperty, nullptr);
  if (!olddecl) {
    return NS_OK; // no decl, so nothing to remove
  }

  // For nsDOMCSSAttributeDeclaration, SetCSSDeclaration will lead to
  // Attribute setting code, which leads in turn to BeginUpdate.  We
  // need to start the update now so that the old rule doesn't get used
  // between when we mutate the declaration and when we set the new
  // rule (see stack in bug 209575).
  mozAutoDocUpdate autoUpdate(DocToUpdate(), true);

  DeclarationBlockMutationClosure closure = {};
  MutationClosureData closureData;
  GetPropertyChangeClosure(&closure, &closureData);

  RefPtr<DeclarationBlock> decl = olddecl->EnsureMutable();
  if (!decl->RemoveProperty(aPropertyName, closure)) {
    return NS_OK;
  }
  return SetCSSDeclaration(decl, &closureData);
}
