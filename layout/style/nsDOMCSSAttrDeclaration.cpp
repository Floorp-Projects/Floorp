/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* DOM object for element.style */

#include "nsDOMCSSAttrDeclaration.h"

#include "mozilla/DeclarationBlock.h"
#include "mozilla/DeclarationBlockInlines.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/MutationEventBinding.h"
#include "mozilla/InternalMutationEvent.h"
#include "mozilla/ServoDeclarationBlock.h"
#include "mozAutoDocUpdate.h"
#include "nsContentUtils.h"
#include "nsIDocument.h"
#include "nsIURI.h"
#include "nsNodeUtils.h"
#include "nsSMILCSSValueType.h"
#include "nsWrapperCacheInlines.h"
#include "nsIFrame.h"
#include "ActiveLayerTracker.h"

using namespace mozilla;

nsDOMCSSAttributeDeclaration::nsDOMCSSAttributeDeclaration(dom::Element* aElement,
                                                           bool aIsSMILOverride)
  : mElement(aElement)
  , mIsSMILOverride(aIsSMILOverride)
{
  NS_ASSERTION(aElement, "Inline style for a NULL element?");
}

nsDOMCSSAttributeDeclaration::~nsDOMCSSAttributeDeclaration()
{
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(nsDOMCSSAttributeDeclaration, mElement)

// mElement holds a strong ref to us, so if it's going to be
// skipped, the attribute declaration can't be part of a garbage
// cycle.
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_BEGIN(nsDOMCSSAttributeDeclaration)
  if (tmp->mElement && Element::CanSkip(tmp->mElement, true)) {
    if (tmp->PreservingWrapper()) {
      tmp->MarkWrapperLive();
    }
    return true;
  }
  return tmp->HasKnownLiveWrapper();
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_IN_CC_BEGIN(nsDOMCSSAttributeDeclaration)
  return tmp->HasKnownLiveWrapper() ||
    (tmp->mElement && Element::CanSkipInCC(tmp->mElement));
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_IN_CC_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_THIS_BEGIN(nsDOMCSSAttributeDeclaration)
  return tmp->HasKnownLiveWrapper() ||
    (tmp->mElement && Element::CanSkipThis(tmp->mElement));
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_THIS_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsDOMCSSAttributeDeclaration)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
NS_IMPL_QUERY_TAIL_INHERITING(nsDOMCSSDeclaration)

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsDOMCSSAttributeDeclaration)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsDOMCSSAttributeDeclaration)

nsresult
nsDOMCSSAttributeDeclaration::SetCSSDeclaration(DeclarationBlock* aDecl)
{
  NS_ASSERTION(mElement, "Must have Element to set the declaration!");
  aDecl->SetDirty();
  return mIsSMILOverride
    ? mElement->SetSMILOverrideStyleDeclaration(aDecl, true)
    : mElement->SetInlineStyleDeclaration(aDecl, nullptr, true);
}

nsIDocument*
nsDOMCSSAttributeDeclaration::DocToUpdate()
{
  // We need OwnerDoc() rather than GetUncomposedDoc() because it might
  // be the BeginUpdate call that inserts mElement into the document.
  return mElement->OwnerDoc();
}

DeclarationBlock*
nsDOMCSSAttributeDeclaration::GetCSSDeclaration(Operation aOperation)
{
  if (!mElement)
    return nullptr;

  DeclarationBlock* declaration;
  if (mIsSMILOverride) {
    declaration = mElement->GetSMILOverrideStyleDeclaration();
  } else {
    declaration = mElement->GetInlineStyleDeclaration();
  }

  // Notify observers that our style="" attribute is going to change
  // unless:
  //   * this is a declaration that holds SMIL animation values (which
  //     aren't reflected in the DOM style="" attribute), or
  //   * we're getting the declaration for reading, or
  //   * we're getting it for property removal but we don't currently have
  //     a declaration.

  // XXXbz this is a bit of a hack, especially doing it before the
  // BeginUpdate(), but this is a good chokepoint where we know we
  // plan to modify the CSSDeclaration, so need to notify
  // AttributeWillChange if this is inline style.
  if (!mIsSMILOverride &&
      ((aOperation == eOperation_Modify) ||
       (aOperation == eOperation_RemoveProperty && declaration))) {
    nsNodeUtils::AttributeWillChange(mElement, kNameSpaceID_None,
                                     nsGkAtoms::style,
                                     dom::MutationEventBinding::MODIFICATION,
                                     nullptr);
  }

  if (declaration) {
    if (aOperation != eOperation_Read &&
        nsContentUtils::HasMutationListeners(
          mElement, NS_EVENT_BITS_MUTATION_ATTRMODIFIED, mElement)) {
      // If there is any mutation listener on the element, we need to
      // ensure that any change would create a new declaration so that
      // nsStyledElement::SetInlineStyleDeclaration can generate the
      // correct old value.
      declaration->SetImmutable();
    }
    return declaration;
  }

  if (aOperation != eOperation_Modify) {
    return nullptr;
  }

  // cannot fail
  RefPtr<DeclarationBlock> decl = new ServoDeclarationBlock();

  // this *can* fail (inside SetAttrAndNotify, at least).
  nsresult rv;
  if (mIsSMILOverride) {
    rv = mElement->SetSMILOverrideStyleDeclaration(decl, false);
  } else {
    rv = mElement->SetInlineStyleDeclaration(decl, nullptr, false);
  }

  if (NS_FAILED(rv)) {
    return nullptr; // the decl will be destroyed along with the style rule
  }

  return decl;
}

nsDOMCSSDeclaration::ServoCSSParsingEnvironment
nsDOMCSSAttributeDeclaration::GetServoCSSParsingEnvironment(
    nsIPrincipal* aSubjectPrincipal) const
{
  return {
    mElement->GetURLDataForStyleAttr(aSubjectPrincipal),
    mElement->OwnerDoc()->GetCompatibilityMode(),
    mElement->OwnerDoc()->CSSLoader(),
  };
}

nsresult
nsDOMCSSAttributeDeclaration::SetSMILValue(const nsCSSPropertyID aPropID,
                                           const nsSMILValue& aValue)
{
  MOZ_ASSERT(mIsSMILOverride);
  // No need to do the ActiveLayerTracker / ScrollLinkedEffectDetector bits,
  // since we're in a SMIL animation anyway, no need to try to detect we're a
  // scripted animation.
  DeclarationBlock* olddecl = GetCSSDeclaration(eOperation_Modify);
  if (!olddecl) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  mozAutoDocConditionalContentUpdateBatch autoUpdate(DocToUpdate(), true);
  RefPtr<DeclarationBlock> decl = olddecl->EnsureMutable();
  bool changed = nsSMILCSSValueType::SetPropertyValues(aValue, *decl);
  if (changed) {
    SetCSSDeclaration(decl);
  }
  return NS_OK;
}

nsresult
nsDOMCSSAttributeDeclaration::SetPropertyValue(const nsCSSPropertyID aPropID,
                                               const nsAString& aValue,
                                               nsIPrincipal* aSubjectPrincipal)
{
  // Scripted modifications to style.opacity or style.transform
  // could immediately force us into the animated state if heuristics suggest
  // this is scripted animation.
  // FIXME: This is missing the margin shorthand and the logical versions of
  // the margin properties, see bug 1266287.
  if (aPropID == eCSSProperty_opacity || aPropID == eCSSProperty_transform ||
      aPropID == eCSSProperty_left || aPropID == eCSSProperty_top ||
      aPropID == eCSSProperty_right || aPropID == eCSSProperty_bottom ||
      aPropID == eCSSProperty_background_position_x ||
      aPropID == eCSSProperty_background_position_y ||
      aPropID == eCSSProperty_background_position) {
    nsIFrame* frame = mElement->GetPrimaryFrame();
    if (frame) {
      ActiveLayerTracker::NotifyInlineStyleRuleModified(frame, aPropID, aValue, this);
    }
  }
  return nsDOMCSSDeclaration::SetPropertyValue(aPropID, aValue, aSubjectPrincipal);
}
