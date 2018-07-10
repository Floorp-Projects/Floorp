/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsStyledElement.h"
#include "mozAutoDocUpdate.h"
#include "nsGkAtoms.h"
#include "nsAttrValue.h"
#include "nsAttrValueInlines.h"
#include "mozilla/dom/ElementInlines.h"
#include "mozilla/dom/MutationEventBinding.h"
#include "mozilla/InternalMutationEvent.h"
#include "mozilla/StaticPrefs.h"
#include "nsDOMCSSDeclaration.h"
#include "nsDOMCSSAttrDeclaration.h"
#include "nsServiceManagerUtils.h"
#include "nsIDocument.h"
#include "mozilla/DeclarationBlock.h"
#include "mozilla/css/Loader.h"
#include "nsXULElement.h"
#include "nsContentUtils.h"
#include "nsStyleUtil.h"

using namespace mozilla;
using namespace mozilla::dom;

// Use the CC variant of this, even though this class does not define
// a new CC participant, to make QIing to the CC interfaces faster.
NS_IMPL_QUERY_INTERFACE_CYCLE_COLLECTION_INHERITED(nsStyledElement,
                                                   nsStyledElementBase,
                                                   nsStyledElement)

//----------------------------------------------------------------------
// nsIContent methods

bool
nsStyledElement::ParseAttribute(int32_t aNamespaceID,
                                nsAtom* aAttribute,
                                const nsAString& aValue,
                                nsIPrincipal* aMaybeScriptedPrincipal,
                                nsAttrValue& aResult)
{
  if (aAttribute == nsGkAtoms::style && aNamespaceID == kNameSpaceID_None) {
    ParseStyleAttribute(aValue, aMaybeScriptedPrincipal, aResult, false);
    return true;
  }

  return nsStyledElementBase::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                             aMaybeScriptedPrincipal, aResult);
}

nsresult
nsStyledElement::BeforeSetAttr(int32_t aNamespaceID, nsAtom* aName,
                               const nsAttrValueOrString* aValue, bool aNotify)
{
  if (aNamespaceID == kNameSpaceID_None) {
    if (aName == nsGkAtoms::style) {
      if (aValue) {
        SetMayHaveStyle();
      }
    }
  }

  return nsStyledElementBase::BeforeSetAttr(aNamespaceID, aName, aValue,
                                            aNotify);
}

void
nsStyledElement::InlineStyleDeclarationWillChange(MutationClosureData& aData)
{
  MOZ_ASSERT(OwnerDoc()->UpdateNestingLevel() > 0,
             "Should be inside document update!");
  bool modification = false;
  if (MayHaveStyle()) {
    bool needsOldValue =
      !StaticPrefs::dom_mutation_events_cssom_disabled() &&
      nsContentUtils::HasMutationListeners(this,
                                           NS_EVENT_BITS_MUTATION_ATTRMODIFIED,
                                           this);

    if (!needsOldValue) {
      CustomElementDefinition* definition = GetCustomElementDefinition();
      if (definition && definition->IsInObservedAttributeList(nsGkAtoms::style)) {
        needsOldValue = true;
      }
    }

    if (needsOldValue) {
      nsAutoString oldValueStr;
      modification = GetAttr(kNameSpaceID_None, nsGkAtoms::style,
                             oldValueStr);
      if (modification) {
        aData.mOldValue.emplace();
        aData.mOldValue->SetTo(oldValueStr);
      }
    } else {
      modification = HasAttr(kNameSpaceID_None, nsGkAtoms::style);
    }
  }

  aData.mModType = modification ?
    static_cast<uint8_t>(MutationEvent_Binding::MODIFICATION) :
    static_cast<uint8_t>(MutationEvent_Binding::ADDITION);
  nsNodeUtils::AttributeWillChange(this, kNameSpaceID_None,
                                   nsGkAtoms::style,
                                   aData.mModType, nullptr);

  //XXXsmaug In order to make attribute handling more consistent, consider to
  //         call BeforeSetAttr and pass kCallAfterSetAttr to
  //         SetAttrAndNotify in SetInlineStyleDeclaration.
  //         Handling of mozAutoDocUpdate may require changes in that case.
}

nsresult
nsStyledElement::SetInlineStyleDeclaration(DeclarationBlock& aDeclaration,
                                           MutationClosureData& aData)
{
  MOZ_ASSERT(OwnerDoc()->UpdateNestingLevel(),
             "Should be inside document update!");

  bool hasListeners =
    !StaticPrefs::dom_mutation_events_cssom_disabled() &&
    nsContentUtils::HasMutationListeners(this,
                                         NS_EVENT_BITS_MUTATION_ATTRMODIFIED,
                                         this);

  nsAttrValue attrValue(do_AddRef(&aDeclaration), nullptr);
  SetMayHaveStyle();

  nsIDocument* document = GetComposedDoc();
  mozAutoDocUpdate updateBatch(document, true);
  return SetAttrAndNotify(kNameSpaceID_None, nsGkAtoms::style, nullptr,
                          aData.mOldValue.isSome() ?
                            aData.mOldValue.ptr() : nullptr,
                          attrValue, nullptr, aData.mModType,
                          hasListeners, true, kDontCallAfterSetAttr,
                          document, updateBatch);
}

// ---------------------------------------------------------------
// Others and helpers

nsICSSDeclaration*
nsStyledElement::Style()
{
  Element::nsDOMSlots *slots = DOMSlots();

  if (!slots->mStyle) {
    // Just in case...
    ReparseStyleAttribute(true, false);

    slots->mStyle = new nsDOMCSSAttributeDeclaration(this, false);
    SetMayHaveStyle();
  }

  return slots->mStyle;
}

nsresult
nsStyledElement::ReparseStyleAttribute(bool aForceInDataDoc, bool aForceIfAlreadyParsed)
{
  if (!MayHaveStyle()) {
    return NS_OK;
  }
  const nsAttrValue* oldVal = mAttrsAndChildren.GetAttr(nsGkAtoms::style);
  if (oldVal && (aForceIfAlreadyParsed || oldVal->Type() != nsAttrValue::eCSSDeclaration)) {
    nsAttrValue attrValue;
    nsAutoString stringValue;
    oldVal->ToString(stringValue);
    ParseStyleAttribute(stringValue, nullptr, attrValue, aForceInDataDoc);
    // Don't bother going through SetInlineStyleDeclaration; we don't
    // want to fire off mutation events or document notifications anyway
    bool oldValueSet;
    nsresult rv = mAttrsAndChildren.SetAndSwapAttr(nsGkAtoms::style, attrValue,
                                                   &oldValueSet);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

void
nsStyledElement::NodeInfoChanged(nsIDocument* aOldDoc)
{
  nsStyledElementBase::NodeInfoChanged(aOldDoc);
}

nsICSSDeclaration*
nsStyledElement::GetExistingStyle()
{
  Element::nsDOMSlots* slots = GetExistingDOMSlots();
  if (!slots) {
    return nullptr;
  }

  return slots->mStyle;
}

void
nsStyledElement::ParseStyleAttribute(const nsAString& aValue,
                                     nsIPrincipal* aMaybeScriptedPrincipal,
                                     nsAttrValue& aResult,
                                     bool aForceInDataDoc)
{
  nsIDocument* doc = OwnerDoc();
  bool isNativeAnon = IsInNativeAnonymousSubtree();

  if (!isNativeAnon &&
      !nsStyleUtil::CSPAllowsInlineStyle(this, NodePrincipal(),
                                         aMaybeScriptedPrincipal,
                                         doc->GetDocumentURI(), 0, 0, aValue,
                                         nullptr))
    return;

  if (aForceInDataDoc ||
      !doc->IsLoadedAsData() ||
      GetExistingStyle() ||
      doc->IsStaticDocument()) {
    bool isCSS = true; // assume CSS until proven otherwise

    if (!isNativeAnon) {  // native anonymous content always assumes CSS
      nsAutoString styleType;
      doc->GetHeaderData(nsGkAtoms::headerContentStyleType, styleType);
      if (!styleType.IsEmpty()) {
        static const char textCssStr[] = "text/css";
        isCSS = (styleType.EqualsIgnoreCase(textCssStr, sizeof(textCssStr) - 1));
      }
    }

    if (isCSS && aResult.ParseStyleAttribute(aValue, aMaybeScriptedPrincipal,
                                             this)) {
      return;
    }
  }

  aResult.SetTo(aValue);
}
