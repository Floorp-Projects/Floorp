/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* DOM object for element.style */

#include "nsDOMCSSAttrDeclaration.h"

#include "mozilla/DeclarationBlock.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/MutationEventBinding.h"
#include "mozilla/InternalMutationEvent.h"
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
nsDOMCSSAttributeDeclaration::SetCSSDeclaration(DeclarationBlock* aDecl,
                                                MutationClosureData* aClosureData)
{
  NS_ASSERTION(mElement, "Must have Element to set the declaration!");

  // Whenever changing element.style values, aClosureData must be non-null.
  // SMIL doesn't update Element's attribute values, so closure data isn't
  // needed.
  MOZ_ASSERT_IF(!mIsSMILOverride, aClosureData);

  // If the closure hasn't been called because the declaration wasn't changed,
  // we need to explicitly call it now to get InlineStyleDeclarationWillChange
  // notification before SetInlineStyleDeclaration.
  if (aClosureData && aClosureData->mClosure) {
    aClosureData->mClosure(aClosureData);
  }

  aDecl->SetDirty();
  return mIsSMILOverride
    ? mElement->SetSMILOverrideStyleDeclaration(aDecl, true)
    : mElement->SetInlineStyleDeclaration(*aDecl, *aClosureData);
}

nsIDocument*
nsDOMCSSAttributeDeclaration::DocToUpdate()
{
  // We need OwnerDoc() rather than GetUncomposedDoc() because it might
  // be the BeginUpdate call that inserts mElement into the document.
  return mElement->OwnerDoc();
}

DeclarationBlock*
nsDOMCSSAttributeDeclaration::GetOrCreateCSSDeclaration(Operation aOperation,
                                                        DeclarationBlock** aCreated)
{
  MOZ_ASSERT(aOperation != eOperation_Modify || aCreated);

  if (!mElement)
    return nullptr;

  DeclarationBlock* declaration;
  if (mIsSMILOverride) {
    declaration = mElement->GetSMILOverrideStyleDeclaration();
  } else {
    declaration = mElement->GetInlineStyleDeclaration();
  }

  if (declaration) {
    return declaration;
  }

  if (aOperation != eOperation_Modify) {
    return nullptr;
  }

  // cannot fail
  RefPtr<DeclarationBlock> decl = new DeclarationBlock();
  // Mark the declaration dirty so that it can be reused by the caller.
  // Normally SetDirty is called later in SetCSSDeclaration.
  decl->SetDirty();
#ifdef DEBUG
  RefPtr<DeclarationBlock> mutableDecl = decl->EnsureMutable();
  MOZ_ASSERT(mutableDecl == decl);
#endif
  decl.swap(*aCreated);
  return *aCreated;
}

nsDOMCSSDeclaration::ParsingEnvironment
nsDOMCSSAttributeDeclaration::GetParsingEnvironment(
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
  RefPtr<DeclarationBlock> created;
  DeclarationBlock* olddecl =
    GetOrCreateCSSDeclaration(eOperation_Modify, getter_AddRefs(created));
  if (!olddecl) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  mozAutoDocUpdate autoUpdate(DocToUpdate(), true);
  RefPtr<DeclarationBlock> decl = olddecl->EnsureMutable();
  bool changed = nsSMILCSSValueType::SetPropertyValues(aValue, *decl);
  if (changed) {
    // We can pass nullptr as the latter param, since this is
    // mIsSMILOverride == true case.
    SetCSSDeclaration(decl, nullptr);
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

void
nsDOMCSSAttributeDeclaration::MutationClosureFunction(void* aData)
{
  MutationClosureData* data = static_cast<MutationClosureData*>(aData);
  // Clear mClosure pointer so that it doesn't get called again.
  data->mClosure = nullptr;
  data->mElement->InlineStyleDeclarationWillChange(*data);
}
