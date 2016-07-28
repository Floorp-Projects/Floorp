/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ServoStyleSet.h"

#include "mozilla/ServoRestyleManager.h"
#include "nsCSSAnonBoxes.h"
#include "nsCSSPseudoElements.h"
#include "nsIDocumentInlines.h"
#include "nsPrintfCString.h"
#include "nsStyleContext.h"
#include "nsStyleSet.h"

using namespace mozilla;
using namespace mozilla::dom;

ServoStyleSet::ServoStyleSet()
  : mPresContext(nullptr)
  , mRawSet(Servo_InitStyleSet())
  , mBatching(0)
  , mStylingStarted(false)
{
}

void
ServoStyleSet::Init(nsPresContext* aPresContext)
{
  mPresContext = aPresContext;
}

void
ServoStyleSet::BeginShutdown()
{
}

void
ServoStyleSet::Shutdown()
{
  mRawSet = nullptr;
}

bool
ServoStyleSet::GetAuthorStyleDisabled() const
{
  return false;
}

nsresult
ServoStyleSet::SetAuthorStyleDisabled(bool aStyleDisabled)
{
  MOZ_CRASH("stylo: not implemented");
}

void
ServoStyleSet::BeginUpdate()
{
  ++mBatching;
}

nsresult
ServoStyleSet::EndUpdate()
{
  MOZ_ASSERT(mBatching > 0);
  if (--mBatching > 0) {
    return NS_OK;
  }

  // ... do something ...
  return NS_OK;
}

void
ServoStyleSet::StartStyling(nsPresContext* aPresContext)
{
  Element* root = aPresContext->Document()->GetRootElement();
  if (root) {
    RestyleSubtree(root);
  }
  mStylingStarted = true;
}

already_AddRefed<nsStyleContext>
ServoStyleSet::ResolveStyleFor(Element* aElement,
                               nsStyleContext* aParentContext)
{
  return GetContext(aElement, aParentContext, nullptr,
                    CSSPseudoElementType::NotPseudo);
}

already_AddRefed<nsStyleContext>
ServoStyleSet::GetContext(nsIContent* aContent,
                          nsStyleContext* aParentContext,
                          nsIAtom* aPseudoTag,
                          CSSPseudoElementType aPseudoType)
{
  RefPtr<ServoComputedValues> computedValues = dont_AddRef(Servo_GetComputedValues(aContent));
  MOZ_ASSERT(computedValues);
  return GetContext(computedValues.forget(), aParentContext, aPseudoTag, aPseudoType);
}

already_AddRefed<nsStyleContext>
ServoStyleSet::GetContext(already_AddRefed<ServoComputedValues> aComputedValues,
                          nsStyleContext* aParentContext,
                          nsIAtom* aPseudoTag,
                          CSSPseudoElementType aPseudoType)
{
  // XXXbholley: nsStyleSet does visited handling here.

  // XXXbholley: Figure out the correct thing to pass here. Does this fixup
  // duplicate something that servo already does?
  bool skipFixup = false;

  return NS_NewStyleContext(aParentContext, mPresContext, aPseudoTag,
                            aPseudoType,
                            Move(aComputedValues), skipFixup);
}

already_AddRefed<nsStyleContext>
ServoStyleSet::ResolveStyleFor(Element* aElement,
                               nsStyleContext* aParentContext,
                               TreeMatchContext& aTreeMatchContext)
{
  // aTreeMatchContext is used to speed up selector matching,
  // but if the element already has a ServoComputedValues computed in
  // advance, then we shouldn't need to use it.
  return ResolveStyleFor(aElement, aParentContext);
}

already_AddRefed<nsStyleContext>
ServoStyleSet::ResolveStyleForText(nsIContent* aTextNode,
                                   nsStyleContext* aParentContext)
{
  MOZ_ASSERT(aTextNode && aTextNode->IsNodeOfType(nsINode::eTEXT));
  return GetContext(aTextNode, aParentContext, nsCSSAnonBoxes::mozText,
                    CSSPseudoElementType::AnonBox);
}

already_AddRefed<nsStyleContext>
ServoStyleSet::ResolveStyleForOtherNonElement(nsStyleContext* aParentContext)
{
  RefPtr<ServoComputedValues> computedValues =
    Servo_InheritComputedValues(
      aParentContext->StyleSource().AsServoComputedValues());
  MOZ_ASSERT(computedValues);

  return GetContext(computedValues.forget(), aParentContext,
                    nsCSSAnonBoxes::mozOtherNonElement,
                    CSSPseudoElementType::AnonBox);
}

already_AddRefed<nsStyleContext>
ServoStyleSet::ResolvePseudoElementStyle(Element* aParentElement,
                                         CSSPseudoElementType aType,
                                         nsStyleContext* aParentContext,
                                         Element* aPseudoElement)
{
  if (aPseudoElement) {
    NS_ERROR("stylo: We don't support CSS_PSEUDO_ELEMENT_SUPPORTS_USER_ACTION_STATE yet");
  }
  MOZ_ASSERT(aParentContext);
  MOZ_ASSERT(aType < CSSPseudoElementType::Count);
  nsIAtom* pseudoTag = nsCSSPseudoElements::GetPseudoAtom(aType);

  RefPtr<ServoComputedValues> computedValues =
    Servo_GetComputedValuesForPseudoElement(
      aParentContext->StyleSource().AsServoComputedValues(),
      aParentElement, pseudoTag, mRawSet.get(), /* is_probe = */ false);
  MOZ_ASSERT(computedValues);

  return GetContext(computedValues.forget(), aParentContext, pseudoTag, aType);
}

// aFlags is an nsStyleSet flags bitfield
already_AddRefed<nsStyleContext>
ServoStyleSet::ResolveAnonymousBoxStyle(nsIAtom* aPseudoTag,
                                        nsStyleContext* aParentContext,
                                        uint32_t aFlags)
{
  MOZ_ASSERT(nsCSSAnonBoxes::IsAnonBox(aPseudoTag));

  MOZ_ASSERT(aFlags == 0 ||
             aFlags == nsStyleSet::eSkipParentDisplayBasedStyleFixup);
  bool skipFixup = aFlags & nsStyleSet::eSkipParentDisplayBasedStyleFixup;

  ServoComputedValues* parentStyle =
    aParentContext ? aParentContext->StyleSource().AsServoComputedValues()
                   : nullptr;
  RefPtr<ServoComputedValues> computedValues =
    dont_AddRef(Servo_GetComputedValuesForAnonymousBox(parentStyle, aPseudoTag,
                                                       mRawSet.get()));
#ifdef DEBUG
  if (!computedValues) {
    nsString pseudo;
    aPseudoTag->ToString(pseudo);
    NS_ERROR(nsPrintfCString("stylo: could not get anon-box: %s",
             NS_ConvertUTF16toUTF8(pseudo).get()).get());
    MOZ_CRASH();
  }
#endif

  return NS_NewStyleContext(aParentContext, mPresContext, aPseudoTag,
                            CSSPseudoElementType::AnonBox,
                            computedValues.forget(), skipFixup);
}

// manage the set of style sheets in the style set
nsresult
ServoStyleSet::AppendStyleSheet(SheetType aType,
                                ServoStyleSheet* aSheet)
{
  MOZ_ASSERT(aSheet);
  MOZ_ASSERT(aSheet->IsApplicable());
  MOZ_ASSERT(nsStyleSet::IsCSSSheetType(aType));

  mSheets[aType].RemoveElement(aSheet);
  mSheets[aType].AppendElement(aSheet);

  // Maintain a mirrored list of sheets on the servo side.
  Servo_AppendStyleSheet(aSheet->RawSheet(), mRawSet.get());

  return NS_OK;
}

nsresult
ServoStyleSet::PrependStyleSheet(SheetType aType,
                                 ServoStyleSheet* aSheet)
{
  MOZ_ASSERT(aSheet);
  MOZ_ASSERT(aSheet->IsApplicable());
  MOZ_ASSERT(nsStyleSet::IsCSSSheetType(aType));

  mSheets[aType].RemoveElement(aSheet);
  mSheets[aType].InsertElementAt(0, aSheet);

  // Maintain a mirrored list of sheets on the servo side.
  Servo_PrependStyleSheet(aSheet->RawSheet(), mRawSet.get());

  return NS_OK;
}

nsresult
ServoStyleSet::RemoveStyleSheet(SheetType aType,
                                ServoStyleSheet* aSheet)
{
  MOZ_ASSERT(aSheet);
  MOZ_ASSERT(aSheet->IsApplicable());
  MOZ_ASSERT(nsStyleSet::IsCSSSheetType(aType));

  mSheets[aType].RemoveElement(aSheet);

  // Maintain a mirrored list of sheets on the servo side.
  Servo_RemoveStyleSheet(aSheet->RawSheet(), mRawSet.get());

  return NS_OK;
}

nsresult
ServoStyleSet::ReplaceSheets(SheetType aType,
                             const nsTArray<RefPtr<ServoStyleSheet>>& aNewSheets)
{
  MOZ_CRASH("stylo: not implemented");
}

nsresult
ServoStyleSet::InsertStyleSheetBefore(SheetType aType,
                                      ServoStyleSheet* aNewSheet,
                                      ServoStyleSheet* aReferenceSheet)
{
  MOZ_ASSERT(aNewSheet);
  MOZ_ASSERT(aReferenceSheet);
  MOZ_ASSERT(aNewSheet->IsApplicable());

  mSheets[aType].RemoveElement(aNewSheet);
  size_t idx = mSheets[aType].IndexOf(aReferenceSheet);
  if (idx == mSheets[aType].NoIndex) {
    return NS_ERROR_INVALID_ARG;
  }

  mSheets[aType].InsertElementAt(idx, aNewSheet);

  // Maintain a mirrored list of sheets on the servo side.
  Servo_InsertStyleSheetBefore(aNewSheet->RawSheet(),
                               aReferenceSheet->RawSheet(), mRawSet.get());

  return NS_OK;
}

int32_t
ServoStyleSet::SheetCount(SheetType aType) const
{
  MOZ_ASSERT(nsStyleSet::IsCSSSheetType(aType));
  return mSheets[aType].Length();
}

ServoStyleSheet*
ServoStyleSet::StyleSheetAt(SheetType aType,
                            int32_t aIndex) const
{
  MOZ_ASSERT(nsStyleSet::IsCSSSheetType(aType));
  return mSheets[aType][aIndex];
}

nsresult
ServoStyleSet::RemoveDocStyleSheet(ServoStyleSheet* aSheet)
{
  return RemoveStyleSheet(SheetType::Doc, aSheet);
}

nsresult
ServoStyleSet::AddDocStyleSheet(ServoStyleSheet* aSheet,
                                nsIDocument* aDocument)
{
  StyleSheetHandle::RefPtr strong(aSheet);

  mSheets[SheetType::Doc].RemoveElement(aSheet);

  size_t index =
    aDocument->FindDocStyleSheetInsertionPoint(mSheets[SheetType::Doc], aSheet);
  mSheets[SheetType::Doc].InsertElementAt(index, aSheet);

  // Maintain a mirrored list of sheets on the servo side.
  ServoStyleSheet* followingSheet =
    mSheets[SheetType::Doc].SafeElementAt(index + 1);
  if (followingSheet) {
    Servo_InsertStyleSheetBefore(aSheet->RawSheet(), followingSheet->RawSheet(),
                                 mRawSet.get());
  } else {
    Servo_AppendStyleSheet(aSheet->RawSheet(), mRawSet.get());
  }

  return NS_OK;
}

already_AddRefed<nsStyleContext>
ServoStyleSet::ProbePseudoElementStyle(Element* aParentElement,
                                       CSSPseudoElementType aType,
                                       nsStyleContext* aParentContext)
{
  MOZ_ASSERT(aParentContext);
  MOZ_ASSERT(aType < CSSPseudoElementType::Count);
  nsIAtom* pseudoTag = nsCSSPseudoElements::GetPseudoAtom(aType);

  RefPtr<ServoComputedValues> computedValues =
    Servo_GetComputedValuesForPseudoElement(
      aParentContext->StyleSource().AsServoComputedValues(),
      aParentElement, pseudoTag, mRawSet.get(), /* is_probe = */ true);

  if (!computedValues) {
    return nullptr;
  }

  // For :before and :after pseudo-elements, having display: none or no
  // 'content' property is equivalent to not having the pseudo-element
  // at all.
  if (computedValues &&
      (pseudoTag == nsCSSPseudoElements::before ||
       pseudoTag == nsCSSPseudoElements::after)) {
    const nsStyleDisplay *display = Servo_GetStyleDisplay(computedValues);
    const nsStyleContent *content = Servo_GetStyleContent(computedValues);
    // XXXldb What is contentCount for |content: ""|?
    if (display->mDisplay == NS_STYLE_DISPLAY_NONE ||
        content->ContentCount() == 0) {
      return nullptr;
    }
  }

  return GetContext(computedValues.forget(), aParentContext, pseudoTag, aType);
}

already_AddRefed<nsStyleContext>
ServoStyleSet::ProbePseudoElementStyle(Element* aParentElement,
                                       CSSPseudoElementType aType,
                                       nsStyleContext* aParentContext,
                                       TreeMatchContext& aTreeMatchContext,
                                       Element* aPseudoElement)
{
  if (aPseudoElement) {
    NS_ERROR("stylo: We don't support CSS_PSEUDO_ELEMENT_SUPPORTS_USER_ACTION_STATE yet");
  }
  return ProbePseudoElementStyle(aParentElement, aType, aParentContext);
}

nsRestyleHint
ServoStyleSet::HasStateDependentStyle(dom::Element* aElement,
                                      EventStates aStateMask)
{
  NS_WARNING("stylo: HasStateDependentStyle always returns zero!");
  return nsRestyleHint(0);
}

nsRestyleHint
ServoStyleSet::HasStateDependentStyle(dom::Element* aElement,
                                      CSSPseudoElementType aPseudoType,
                                     dom::Element* aPseudoElement,
                                     EventStates aStateMask)
{
  NS_WARNING("stylo: HasStateDependentStyle always returns zero!");
  return nsRestyleHint(0);
}

nsRestyleHint
ServoStyleSet::ComputeRestyleHint(dom::Element* aElement,
                                  ServoElementSnapshot* aSnapshot)
{
  return Servo_ComputeRestyleHint(aElement, aSnapshot, mRawSet.get());
}

void
ServoStyleSet::RestyleSubtree(nsINode* aNode)
{
  MOZ_ASSERT(aNode->IsDirtyForServo() || aNode->HasDirtyDescendantsForServo());
  Servo_RestyleSubtree(aNode, mRawSet.get());
}
