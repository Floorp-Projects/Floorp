/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ServoStyleSet.h"

#include "mozilla/DocumentStyleRootIterator.h"
#include "mozilla/ServoRestyleManager.h"
#include "mozilla/dom/ChildIterator.h"
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
  , mRawSet(Servo_StyleSet_Init())
  , mBatching(0)
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
  // It's important to do this before mRawSet is released, since that will cause
  // a RuleTree GC, which needs to happen after we have dropped all of the
  // document's strong references to RuleNodes.  We also need to do it here,
  // in BeginShutdown, and not in Shutdown, since Shutdown happens after the
  // frame tree has been destroyed, but before the script runners that delete
  // native anonymous content (which also could be holding on the RuleNodes)
  // have run.  By clearing style here, before the frame tree is destroyed,
  // the AllChildrenIterator will find the anonymous content.
  //
  // Note that this is pretty bad for performance; we should find a way to
  // get by with the ServoNodeDatas being dropped as part of the document
  // going away.
  DocumentStyleRootIterator iter(mPresContext->Document());
  while (Element* root = iter.GetNextStyleRoot()) {
    ServoRestyleManager::ClearServoDataFromSubtree(root);
  }
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

  Servo_StyleSet_FlushStyleSheets(mRawSet.get());
  return NS_OK;
}

already_AddRefed<nsStyleContext>
ServoStyleSet::ResolveStyleFor(Element* aElement,
                               nsStyleContext* aParentContext,
                               ConsumeStyleBehavior aConsume,
                               LazyComputeBehavior aMayCompute)
{
  return GetContext(aElement, aParentContext, nullptr,
                    CSSPseudoElementType::NotPseudo, aConsume, aMayCompute);
}

already_AddRefed<nsStyleContext>
ServoStyleSet::GetContext(nsIContent* aContent,
                          nsStyleContext* aParentContext,
                          nsIAtom* aPseudoTag,
                          CSSPseudoElementType aPseudoType,
                          ConsumeStyleBehavior aConsume,
                          LazyComputeBehavior aMayCompute)
{
  MOZ_ASSERT(aContent->IsElement());
  Element* element = aContent->AsElement();

  RefPtr<ServoComputedValues> computedValues;
  if (aMayCompute == LazyComputeBehavior::Allow) {
    computedValues =
      Servo_ResolveStyleLazily(element, nullptr, aConsume, mRawSet.get()).Consume();
  } else {
    computedValues = Servo_ResolveStyle(element, aConsume).Consume();
  }

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
                            aPseudoType, Move(aComputedValues), skipFixup);
}

already_AddRefed<nsStyleContext>
ServoStyleSet::ResolveStyleFor(Element* aElement,
                               nsStyleContext* aParentContext,
                               ConsumeStyleBehavior aConsume,
                               LazyComputeBehavior aMayCompute,
                               TreeMatchContext& aTreeMatchContext)
{
  // aTreeMatchContext is used to speed up selector matching,
  // but if the element already has a ServoComputedValues computed in
  // advance, then we shouldn't need to use it.
  return ResolveStyleFor(aElement, aParentContext, aConsume, aMayCompute);
}

already_AddRefed<nsStyleContext>
ServoStyleSet::ResolveStyleForText(nsIContent* aTextNode,
                                   nsStyleContext* aParentContext)
{
  MOZ_ASSERT(aTextNode && aTextNode->IsNodeOfType(nsINode::eTEXT));
  MOZ_ASSERT(aTextNode->GetParent());
  MOZ_ASSERT(aParentContext);

  // Gecko expects text node style contexts to be like elements that match no
  // rules: inherit the inherit structs, reset the reset structs. This is cheap
  // enough to do on the main thread, which means that the parallel style system
  // can avoid worrying about text nodes.
  const ServoComputedValues* parentComputedValues =
    aParentContext->StyleSource().AsServoComputedValues();
  RefPtr<ServoComputedValues> computedValues =
    Servo_ComputedValues_Inherit(parentComputedValues).Consume();

  return GetContext(computedValues.forget(), aParentContext,
                    nsCSSAnonBoxes::mozText, CSSPseudoElementType::AnonBox);
}

already_AddRefed<nsStyleContext>
ServoStyleSet::ResolveStyleForOtherNonElement(nsStyleContext* aParentContext)
{
  // The parent context can be null if the non-element share a style context
  // with the root of an anonymous subtree.
  const ServoComputedValues* parent =
    aParentContext ? aParentContext->StyleSource().AsServoComputedValues() : nullptr;
  RefPtr<ServoComputedValues> computedValues =
    Servo_ComputedValues_Inherit(parent).Consume();
  MOZ_ASSERT(computedValues);

  return GetContext(computedValues.forget(), aParentContext,
                    nsCSSAnonBoxes::mozOtherNonElement,
                    CSSPseudoElementType::AnonBox);
}

already_AddRefed<nsStyleContext>
ServoStyleSet::ResolvePseudoElementStyle(Element* aOriginatingElement,
                                         CSSPseudoElementType aType,
                                         nsStyleContext* aParentContext,
                                         Element* aPseudoElement)
{
  if (aPseudoElement) {
    NS_ERROR("stylo: We don't support CSS_PSEUDO_ELEMENT_SUPPORTS_USER_ACTION_STATE yet");
  }

  // NB: We ignore aParentContext, on the assumption that pseudo element styles
  // should just inherit from aOriginatingElement's primary style, which Servo
  // already knows.
  MOZ_ASSERT(aType < CSSPseudoElementType::Count);
  nsIAtom* pseudoTag = nsCSSPseudoElements::GetPseudoAtom(aType);

  RefPtr<ServoComputedValues> computedValues =
    Servo_ResolvePseudoStyle(aOriginatingElement, pseudoTag,
                             /* is_probe = */ false, mRawSet.get()).Consume();
  MOZ_ASSERT(computedValues);

  return GetContext(computedValues.forget(), aParentContext, pseudoTag, aType);
}

already_AddRefed<nsStyleContext>
ServoStyleSet::ResolveTransientStyle(Element* aElement, CSSPseudoElementType aType)
{
  nsIAtom* pseudoTag = nullptr;
  if (aType != CSSPseudoElementType::NotPseudo) {
    pseudoTag = nsCSSPseudoElements::GetPseudoAtom(aType);
  }

  RefPtr<ServoComputedValues> computedValues =
    Servo_ResolveStyleLazily(aElement, pseudoTag, ConsumeStyleBehavior::DontConsume,
                             mRawSet.get()).Consume();

  return GetContext(computedValues.forget(), nullptr, pseudoTag, aType);
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

  const ServoComputedValues* parentStyle =
    aParentContext ? aParentContext->StyleSource().AsServoComputedValues()
                   : nullptr;
  RefPtr<ServoComputedValues> computedValues =
    Servo_ComputedValues_GetForAnonymousBox(parentStyle, aPseudoTag,
                                            mRawSet.get()).Consume();
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
  Servo_StyleSet_AppendStyleSheet(mRawSet.get(), aSheet->RawSheet(), !mBatching);

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
  Servo_StyleSet_PrependStyleSheet(mRawSet.get(), aSheet->RawSheet(), !mBatching);

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
  Servo_StyleSet_RemoveStyleSheet(mRawSet.get(), aSheet->RawSheet(), !mBatching);

  return NS_OK;
}

nsresult
ServoStyleSet::ReplaceSheets(SheetType aType,
                             const nsTArray<RefPtr<ServoStyleSheet>>& aNewSheets)
{
  // Gecko uses a two-dimensional array keyed by sheet type, whereas Servo
  // stores a flattened list. This makes ReplaceSheets a pretty clunky thing
  // to express. If the need ever arises, we can easily make this more efficent,
  // probably by aligning the representations better between engines.

  for (ServoStyleSheet* sheet : mSheets[aType]) {
    Servo_StyleSet_RemoveStyleSheet(mRawSet.get(), sheet->RawSheet(), false);
  }

  mSheets[aType].Clear();
  mSheets[aType].AppendElements(aNewSheets);

  for (ServoStyleSheet* sheet : mSheets[aType]) {
    Servo_StyleSet_AppendStyleSheet(mRawSet.get(), sheet->RawSheet(), false);
  }

  if (!mBatching) {
    Servo_StyleSet_FlushStyleSheets(mRawSet.get());
  }

  return NS_OK;
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
  Servo_StyleSet_InsertStyleSheetBefore(mRawSet.get(), aNewSheet->RawSheet(),
                                        aReferenceSheet->RawSheet(), !mBatching);

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
  RefPtr<StyleSheet> strong(aSheet);

  mSheets[SheetType::Doc].RemoveElement(aSheet);

  size_t index =
    aDocument->FindDocStyleSheetInsertionPoint(mSheets[SheetType::Doc], aSheet);
  mSheets[SheetType::Doc].InsertElementAt(index, aSheet);

  // Maintain a mirrored list of sheets on the servo side.
  ServoStyleSheet* followingSheet =
    mSheets[SheetType::Doc].SafeElementAt(index + 1);
  if (followingSheet) {
    Servo_StyleSet_InsertStyleSheetBefore(mRawSet.get(), aSheet->RawSheet(),
                                          followingSheet->RawSheet(), !mBatching);
  } else {
    Servo_StyleSet_AppendStyleSheet(mRawSet.get(), aSheet->RawSheet(), !mBatching);
  }

  return NS_OK;
}

already_AddRefed<nsStyleContext>
ServoStyleSet::ProbePseudoElementStyle(Element* aOriginatingElement,
                                       CSSPseudoElementType aType,
                                       nsStyleContext* aParentContext)
{
  // NB: We ignore aParentContext, on the assumption that pseudo element styles
  // should just inherit from aOriginatingElement's primary style, which Servo
  // already knows.
  MOZ_ASSERT(aType < CSSPseudoElementType::Count);
  nsIAtom* pseudoTag = nsCSSPseudoElements::GetPseudoAtom(aType);

  RefPtr<ServoComputedValues> computedValues =
    Servo_ResolvePseudoStyle(aOriginatingElement, pseudoTag,
                             /* is_probe = */ true, mRawSet.get()).Consume();
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
    if (display->mDisplay == StyleDisplay::None ||
        content->ContentCount() == 0) {
      return nullptr;
    }
  }

  return GetContext(computedValues.forget(), aParentContext, pseudoTag, aType);
}

already_AddRefed<nsStyleContext>
ServoStyleSet::ProbePseudoElementStyle(Element* aOriginatingElement,
                                       CSSPseudoElementType aType,
                                       nsStyleContext* aParentContext,
                                       TreeMatchContext& aTreeMatchContext,
                                       Element* aPseudoElement)
{
  if (aPseudoElement) {
    NS_ERROR("stylo: We don't support CSS_PSEUDO_ELEMENT_SUPPORTS_USER_ACTION_STATE yet");
  }
  return ProbePseudoElementStyle(aOriginatingElement, aType, aParentContext);
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

void
ServoStyleSet::StyleDocument()
{
  // Restyle the document from the root element and each of the document level
  // NAC subtree roots.
  DocumentStyleRootIterator iter(mPresContext->Document());
  while (Element* root = iter.GetNextStyleRoot()) {
    if (root->ShouldTraverseForServo()) {
      Servo_TraverseSubtree(root, mRawSet.get(), TraversalRootBehavior::Normal);
    }
  }
}

void
ServoStyleSet::StyleNewSubtree(Element* aRoot)
{
  MOZ_ASSERT(!aRoot->HasServoData());
  Servo_TraverseSubtree(aRoot, mRawSet.get(), TraversalRootBehavior::Normal);
}

void
ServoStyleSet::StyleNewChildren(Element* aParent)
{
  Servo_TraverseSubtree(aParent, mRawSet.get(),
                        TraversalRootBehavior::UnstyledChildrenOnly);
}

void
ServoStyleSet::NoteStyleSheetsChanged()
{
  Servo_StyleSet_NoteStyleSheetsChanged(mRawSet.get());
}

#ifdef DEBUG
void
ServoStyleSet::AssertTreeIsClean()
{
  if (Element* root = mPresContext->Document()->GetRootElement()) {
    Servo_AssertTreeIsClean(root);
  }
}
#endif
