/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DocumentOrShadowRoot.h"
#include "mozilla/AnimationComparator.h"
#include "mozilla/EventStateManager.h"
#include "mozilla/PresShell.h"
#include "mozilla/dom/AnimatableBinding.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/HTMLInputElement.h"
#include "mozilla/dom/ShadowRoot.h"
#include "mozilla/dom/StyleSheetList.h"
#include "nsTHashtable.h"
#include "nsFocusManager.h"
#include "nsIRadioVisitor.h"
#include "nsIFormControl.h"
#include "nsLayoutUtils.h"
#include "nsSVGUtils.h"
#include "nsWindowSizes.h"

namespace mozilla {
namespace dom {

DocumentOrShadowRoot::DocumentOrShadowRoot(ShadowRoot* aShadowRoot)
    : mAsNode(aShadowRoot), mKind(Kind::ShadowRoot) {
  MOZ_ASSERT(mAsNode);
}

DocumentOrShadowRoot::DocumentOrShadowRoot(Document* aDoc)
    : mAsNode(aDoc), mKind(Kind::Document) {
  MOZ_ASSERT(mAsNode);
}

void DocumentOrShadowRoot::AddSizeOfOwnedSheetArrayExcludingThis(
    nsWindowSizes& aSizes, const nsTArray<RefPtr<StyleSheet>>& aSheets) const {
  size_t n = 0;
  n += aSheets.ShallowSizeOfExcludingThis(aSizes.mState.mMallocSizeOf);
  for (StyleSheet* sheet : aSheets) {
    if (!sheet->GetAssociatedDocumentOrShadowRoot()) {
      // Avoid over-reporting shared sheets.
      continue;
    }
    n += sheet->SizeOfIncludingThis(aSizes.mState.mMallocSizeOf);
  }

  if (mKind == Kind::ShadowRoot) {
    aSizes.mLayoutShadowDomStyleSheetsSize += n;
  } else {
    aSizes.mLayoutStyleSheetsSize += n;
  }
}

void DocumentOrShadowRoot::AddSizeOfExcludingThis(nsWindowSizes& aSizes) const {
  AddSizeOfOwnedSheetArrayExcludingThis(aSizes, mStyleSheets);
  aSizes.mDOMOtherSize +=
      mIdentifierMap.SizeOfExcludingThis(aSizes.mState.mMallocSizeOf);
}

DocumentOrShadowRoot::~DocumentOrShadowRoot() {
  for (StyleSheet* sheet : mStyleSheets) {
    sheet->ClearAssociatedDocumentOrShadowRoot();
  }
}

StyleSheetList* DocumentOrShadowRoot::StyleSheets() {
  if (!mDOMStyleSheets) {
    mDOMStyleSheets = new StyleSheetList(*this);
  }
  return mDOMStyleSheets;
}

void DocumentOrShadowRoot::InsertSheetAt(size_t aIndex, StyleSheet& aSheet) {
  aSheet.SetAssociatedDocumentOrShadowRoot(
      this, StyleSheet::OwnedByDocumentOrShadowRoot);
  mStyleSheets.InsertElementAt(aIndex, &aSheet);
}

already_AddRefed<StyleSheet> DocumentOrShadowRoot::RemoveSheet(
    StyleSheet& aSheet) {
  auto index = mStyleSheets.IndexOf(&aSheet);
  if (index == mStyleSheets.NoIndex) {
    return nullptr;
  }
  RefPtr<StyleSheet> sheet = std::move(mStyleSheets[index]);
  mStyleSheets.RemoveElementAt(index);
  sheet->ClearAssociatedDocumentOrShadowRoot();
  return sheet.forget();
}

void DocumentOrShadowRoot::RemoveSheetFromStylesIfApplicable(
    StyleSheet& aSheet) {
  if (!aSheet.IsApplicable()) {
    return;
  }
  if (mKind == Kind::Document) {
    AsNode().AsDocument()->RemoveStyleSheetFromStyleSets(aSheet);
  } else {
    MOZ_ASSERT(AsNode().IsShadowRoot());
    static_cast<ShadowRoot&>(AsNode()).RemoveSheetFromStyles(aSheet);
  }
}

// https://wicg.github.io/construct-stylesheets/#dom-documentorshadowroot-adoptedstylesheets
void DocumentOrShadowRoot::SetAdoptedStyleSheets(
    const Sequence<OwningNonNull<StyleSheet>>& aAdoptedStyleSheets,
    ErrorResult& aRv) {
  Document& doc = *AsNode().OwnerDoc();
  for (const OwningNonNull<StyleSheet>& sheet : aAdoptedStyleSheets) {
    // 2.1 Check if all sheets are constructed, else throw NotAllowedError
    if (!sheet->IsConstructed()) {
      return aRv.ThrowNotAllowedError(
          "Each adopted style sheet must be created through the Constructable "
          "StyleSheets API");
    }
    // 2.2 Check if all sheets' constructor documents match the
    // DocumentOrShadowRoot's node document, else throw NotAlloweError
    if (!sheet->ConstructorDocumentMatches(doc)) {
      return aRv.ThrowNotAllowedError(
          "Each adopted style sheet's constructor document must match the "
          "document or shadow root's node document");
    }
  }

  auto* shadow = ShadowRoot::FromNode(AsNode());
  MOZ_ASSERT((mKind == Kind::ShadowRoot) == !!shadow);

  StyleSheetSet set(aAdoptedStyleSheets.Length());
  size_t commonPrefix = 0;

  // Find the index at which the new array differs from the old array.
  // We don't want to do extra work for the sheets that both arrays have.
  size_t min =
      std::min(aAdoptedStyleSheets.Length(), mAdoptedStyleSheets.Length());
  for (size_t i = 0; i < min; ++i) {
    if (aAdoptedStyleSheets[i] != mAdoptedStyleSheets[i]) {
      break;
    }
    ++commonPrefix;
    set.PutEntry(mAdoptedStyleSheets[i]);
  }

  // Try to truncate the sheets to a common prefix.
  // If the prefix contains duplicates of sheets that we are removing,
  // we are just going to re-build everything from scratch.
  if (commonPrefix != mAdoptedStyleSheets.Length()) {
    StyleSheetSet removedSet(mAdoptedStyleSheets.Length() - commonPrefix);
    for (size_t i = mAdoptedStyleSheets.Length(); i != commonPrefix; --i) {
      RefPtr<StyleSheet> sheetToRemove = mAdoptedStyleSheets.PopLastElement();
      if (MOZ_UNLIKELY(set.Contains(sheetToRemove))) {
        // Fixing duplicate sheets would require insertions/removals from the
        // style set. We may as well just rebuild the whole thing from scratch.
        set.Clear();
        // Note that setting this to zero means we'll continue the loop until
        // all the sheets are cleared.
        commonPrefix = 0;
      }
      if (MOZ_LIKELY(removedSet.EnsureInserted(sheetToRemove))) {
        RemoveSheetFromStylesIfApplicable(*sheetToRemove);
        sheetToRemove->RemoveAdopter(*this);
      }
    }
    mAdoptedStyleSheets.TruncateLength(commonPrefix);
  }

  // 3. Set the adopted style sheets to the new sheets
  mAdoptedStyleSheets.SetCapacity(aAdoptedStyleSheets.Length());

  // Only add sheets that are not already in the common prefix.
  for (const auto& sheet : MakeSpan(aAdoptedStyleSheets).From(commonPrefix)) {
    if (MOZ_UNLIKELY(!set.EnsureInserted(sheet))) {
      // The idea is that this case is rare, so we pay the price of removing the
      // old sheet from the styles and append it later rather than the other way
      // around.
      RemoveSheetFromStylesIfApplicable(*sheet);
    } else {
      sheet->AddAdopter(*this);
    }
    mAdoptedStyleSheets.AppendElement(sheet);
    if (sheet->IsApplicable()) {
      if (mKind == Kind::Document) {
        doc.AddStyleSheetToStyleSets(*sheet);
      } else {
        shadow->InsertSheetIntoAuthorData(mAdoptedStyleSheets.Length() - 1,
                                          *sheet, mAdoptedStyleSheets);
      }
    }
  }
}

void DocumentOrShadowRoot::ClearAdoptedStyleSheets() {
  EnumerateUniqueAdoptedStyleSheetsBackToFront([&](StyleSheet& aSheet) {
    RemoveSheetFromStylesIfApplicable(aSheet);
    aSheet.RemoveAdopter(*this);
  });
  mAdoptedStyleSheets.Clear();
}

void DocumentOrShadowRoot::CloneAdoptedSheetsFrom(
    DocumentOrShadowRoot& aSource) {
  Document& ownerDoc = *AsNode().OwnerDoc();
  Sequence<OwningNonNull<StyleSheet>> list;
  if (!list.SetCapacity(mAdoptedStyleSheets.Length(), fallible)) {
    return;
  }

  // TODO(nordzilla, bug 1621415) We can improve the performance here.
  // https://bugzilla.mozilla.org/show_bug.cgi?id=1622322
  for (auto& sheet : aSource.mAdoptedStyleSheets) {
    DebugOnly<bool> succeeded =
        list.AppendElement(sheet->CloneAdoptedSheet(ownerDoc), fallible);
    MOZ_ASSERT(succeeded);
  }

  ErrorResult rv;
  SetAdoptedStyleSheets(list, rv);
  MOZ_ASSERT(!rv.Failed());
}

Element* DocumentOrShadowRoot::GetElementById(const nsAString& aElementId) {
  if (MOZ_UNLIKELY(aElementId.IsEmpty())) {
    nsContentUtils::ReportEmptyGetElementByIdArg(AsNode().OwnerDoc());
    return nullptr;
  }

  if (IdentifierMapEntry* entry = mIdentifierMap.GetEntry(aElementId)) {
    if (Element* el = entry->GetIdElement()) {
      return el;
    }
  }

  return nullptr;
}

already_AddRefed<nsContentList> DocumentOrShadowRoot::GetElementsByTagNameNS(
    const nsAString& aNamespaceURI, const nsAString& aLocalName) {
  ErrorResult rv;
  RefPtr<nsContentList> list =
      GetElementsByTagNameNS(aNamespaceURI, aLocalName, rv);
  if (rv.Failed()) {
    return nullptr;
  }
  return list.forget();
}

already_AddRefed<nsContentList> DocumentOrShadowRoot::GetElementsByTagNameNS(
    const nsAString& aNamespaceURI, const nsAString& aLocalName,
    ErrorResult& aResult) {
  int32_t nameSpaceId = kNameSpaceID_Wildcard;

  if (!aNamespaceURI.EqualsLiteral("*")) {
    aResult = nsContentUtils::NameSpaceManager()->RegisterNameSpace(
        aNamespaceURI, nameSpaceId);
    if (aResult.Failed()) {
      return nullptr;
    }
  }

  NS_ASSERTION(nameSpaceId != kNameSpaceID_Unknown, "Unexpected namespace ID!");
  return NS_GetContentList(&AsNode(), nameSpaceId, aLocalName);
}

already_AddRefed<nsContentList> DocumentOrShadowRoot::GetElementsByClassName(
    const nsAString& aClasses) {
  return nsContentUtils::GetElementsByClassName(&AsNode(), aClasses);
}

nsIContent* DocumentOrShadowRoot::Retarget(nsIContent* aContent) const {
  for (nsIContent* cur = aContent; cur; cur = cur->GetContainingShadowHost()) {
    if (cur->SubtreeRoot() == &AsNode()) {
      return cur;
    }
  }
  return nullptr;
}

Element* DocumentOrShadowRoot::GetRetargetedFocusedElement() {
  auto* content = AsNode().OwnerDoc()->GetUnretargetedFocusedContent();
  if (!content) {
    return nullptr;
  }
  if (nsIContent* retarget = Retarget(content)) {
    return retarget->AsElement();
  }
  return nullptr;
}

Element* DocumentOrShadowRoot::GetPointerLockElement() {
  nsCOMPtr<Element> pointerLockedElement =
      do_QueryReferent(EventStateManager::sPointerLockedElement);
  if (!pointerLockedElement) {
    return nullptr;
  }

  nsIContent* retargetedPointerLockedElement = Retarget(pointerLockedElement);
  return retargetedPointerLockedElement &&
                 retargetedPointerLockedElement->IsElement()
             ? retargetedPointerLockedElement->AsElement()
             : nullptr;
}

Element* DocumentOrShadowRoot::GetFullscreenElement() {
  if (!AsNode().IsInComposedDoc()) {
    return nullptr;
  }

  Element* element = AsNode().OwnerDoc()->FullscreenStackTop();
  NS_ASSERTION(!element || element->State().HasState(NS_EVENT_STATE_FULLSCREEN),
               "Fullscreen element should have fullscreen styles applied");

  nsIContent* retargeted = Retarget(element);
  if (retargeted && retargeted->IsElement()) {
    return retargeted->AsElement();
  }

  return nullptr;
}

namespace {

using FrameForPointOption = nsLayoutUtils::FrameForPointOption;

// Whether only one node or multiple nodes is requested.
enum class Multiple {
  No,
  Yes,
};

// Whether we should flush layout or not.
enum class FlushLayout {
  No,
  Yes,
};

template <typename NodeOrElement>
NodeOrElement* CastTo(nsIContent* aContent);

template <>
Element* CastTo<Element>(nsIContent* aContent) {
  return aContent->AsElement();
}

template <>
nsINode* CastTo<nsINode>(nsIContent* aContent) {
  return aContent;
}

template <typename NodeOrElement>
static void QueryNodesFromRect(DocumentOrShadowRoot& aRoot, const nsRect& aRect,
                               EnumSet<FrameForPointOption> aOptions,
                               FlushLayout aShouldFlushLayout,
                               Multiple aMultiple,
                               nsTArray<RefPtr<NodeOrElement>>& aNodes) {
  static_assert(std::is_same<nsINode, NodeOrElement>::value ||
                    std::is_same<Element, NodeOrElement>::value,
                "Should returning nodes or elements");

  constexpr bool returningElements =
      std::is_same<Element, NodeOrElement>::value;

  nsCOMPtr<Document> doc = aRoot.AsNode().OwnerDoc();

  // Make sure the layout information we get is up-to-date, and
  // ensure we get a root frame (for everything but XUL)
  if (aShouldFlushLayout == FlushLayout::Yes) {
    doc->FlushPendingNotifications(FlushType::Layout);
  }

  PresShell* presShell = doc->GetPresShell();
  if (!presShell) {
    return;
  }

  nsIFrame* rootFrame = presShell->GetRootFrame();
  // XUL docs, unlike HTML, have no frame tree until everything's done loading
  if (!rootFrame) {
    return;  // return null to premature XUL callers as a reminder to wait
  }

  aOptions += FrameForPointOption::IgnorePaintSuppression;
  aOptions += FrameForPointOption::IgnoreCrossDoc;

  AutoTArray<nsIFrame*, 8> frames;
  nsLayoutUtils::GetFramesForArea(rootFrame, aRect, frames, aOptions);

  for (nsIFrame* frame : frames) {
    nsIContent* content = doc->GetContentInThisDocument(frame);
    if (!content) {
      continue;
    }

    if (returningElements && !content->IsElement()) {
      // If this helper is called via ElementsFromPoint, we need to make sure
      // our frame is an element. Otherwise return whatever the top frame is
      // even if it isn't the top-painted element.
      // SVG 'text' element's SVGTextFrame doesn't respond to hit-testing, so
      // if 'content' is a child of such an element then we need to manually
      // defer to the parent here.
      if (aMultiple == Multiple::Yes &&
          !nsSVGUtils::IsInSVGTextSubtree(frame)) {
        continue;
      }

      content = content->GetParent();
      if (ShadowRoot* shadow = ShadowRoot::FromNodeOrNull(content)) {
        content = shadow->Host();
      }
    }

    // XXXsmaug There is plenty of unspec'ed behavior here
    //         https://github.com/w3c/webcomponents/issues/735
    //         https://github.com/w3c/webcomponents/issues/736
    content = aRoot.Retarget(content);

    if (content && content != aNodes.SafeLastElement(nullptr)) {
      aNodes.AppendElement(CastTo<NodeOrElement>(content));
      if (aMultiple == Multiple::No) {
        return;
      }
    }
  }
}

template <typename NodeOrElement>
static void QueryNodesFromPoint(DocumentOrShadowRoot& aRoot, float aX, float aY,
                                EnumSet<FrameForPointOption> aOptions,
                                FlushLayout aShouldFlushLayout,
                                Multiple aMultiple,
                                nsTArray<RefPtr<NodeOrElement>>& aNodes) {
  // As per the spec, we return null if either coord is negative.
  if (!aOptions.contains(FrameForPointOption::IgnoreRootScrollFrame) &&
      (aX < 0 || aY < 0)) {
    return;
  }

  nscoord x = nsPresContext::CSSPixelsToAppUnits(aX);
  nscoord y = nsPresContext::CSSPixelsToAppUnits(aY);
  nsPoint pt(x, y);
  QueryNodesFromRect(aRoot, nsRect(pt, nsSize(1, 1)), aOptions,
                     aShouldFlushLayout, aMultiple, aNodes);
}

}  // namespace

Element* DocumentOrShadowRoot::ElementFromPoint(float aX, float aY) {
  return ElementFromPointHelper(aX, aY, false, true);
}

void DocumentOrShadowRoot::ElementsFromPoint(
    float aX, float aY, nsTArray<RefPtr<Element>>& aElements) {
  QueryNodesFromPoint(*this, aX, aY, {}, FlushLayout::Yes, Multiple::Yes,
                      aElements);
}

void DocumentOrShadowRoot::NodesFromPoint(float aX, float aY,
                                          nsTArray<RefPtr<nsINode>>& aNodes) {
  QueryNodesFromPoint(*this, aX, aY, {}, FlushLayout::Yes, Multiple::Yes,
                      aNodes);
}

nsINode* DocumentOrShadowRoot::NodeFromPoint(float aX, float aY) {
  AutoTArray<RefPtr<nsINode>, 1> nodes;
  QueryNodesFromPoint(*this, aX, aY, {}, FlushLayout::Yes, Multiple::No, nodes);
  return nodes.SafeElementAt(0);
}

Element* DocumentOrShadowRoot::ElementFromPointHelper(
    float aX, float aY, bool aIgnoreRootScrollFrame, bool aFlushLayout) {
  EnumSet<FrameForPointOption> options;
  if (aIgnoreRootScrollFrame) {
    options += FrameForPointOption::IgnoreRootScrollFrame;
  }

  auto flush = aFlushLayout ? FlushLayout::Yes : FlushLayout::No;

  AutoTArray<RefPtr<Element>, 1> elements;
  QueryNodesFromPoint(*this, aX, aY, options, flush, Multiple::No, elements);
  return elements.SafeElementAt(0);
}

void DocumentOrShadowRoot::NodesFromRect(float aX, float aY, float aTopSize,
                                         float aRightSize, float aBottomSize,
                                         float aLeftSize,
                                         bool aIgnoreRootScrollFrame,
                                         bool aFlushLayout, bool aOnlyVisible,
                                         nsTArray<RefPtr<nsINode>>& aReturn) {
  // Following the same behavior of elementFromPoint,
  // we don't return anything if either coord is negative
  if (!aIgnoreRootScrollFrame && (aX < 0 || aY < 0)) {
    return;
  }

  nscoord x = nsPresContext::CSSPixelsToAppUnits(aX - aLeftSize);
  nscoord y = nsPresContext::CSSPixelsToAppUnits(aY - aTopSize);
  nscoord w = nsPresContext::CSSPixelsToAppUnits(aLeftSize + aRightSize) + 1;
  nscoord h = nsPresContext::CSSPixelsToAppUnits(aTopSize + aBottomSize) + 1;

  nsRect rect(x, y, w, h);

  EnumSet<FrameForPointOption> options;
  if (aIgnoreRootScrollFrame) {
    options += FrameForPointOption::IgnoreRootScrollFrame;
  }
  if (aOnlyVisible) {
    options += FrameForPointOption::OnlyVisible;
  }

  auto flush = aFlushLayout ? FlushLayout::Yes : FlushLayout::No;
  QueryNodesFromRect(*this, rect, options, flush, Multiple::Yes, aReturn);
}

Element* DocumentOrShadowRoot::AddIDTargetObserver(nsAtom* aID,
                                                   IDTargetObserver aObserver,
                                                   void* aData,
                                                   bool aForImage) {
  nsDependentAtomString id(aID);

  if (!CheckGetElementByIdArg(id)) {
    return nullptr;
  }

  IdentifierMapEntry* entry = mIdentifierMap.PutEntry(aID);
  NS_ENSURE_TRUE(entry, nullptr);

  entry->AddContentChangeCallback(aObserver, aData, aForImage);
  return aForImage ? entry->GetImageIdElement() : entry->GetIdElement();
}

void DocumentOrShadowRoot::RemoveIDTargetObserver(nsAtom* aID,
                                                  IDTargetObserver aObserver,
                                                  void* aData, bool aForImage) {
  nsDependentAtomString id(aID);

  if (!CheckGetElementByIdArg(id)) {
    return;
  }

  IdentifierMapEntry* entry = mIdentifierMap.GetEntry(aID);
  if (!entry) {
    return;
  }

  entry->RemoveContentChangeCallback(aObserver, aData, aForImage);
}

Element* DocumentOrShadowRoot::LookupImageElement(const nsAString& aId) {
  if (aId.IsEmpty()) {
    return nullptr;
  }

  IdentifierMapEntry* entry = mIdentifierMap.GetEntry(aId);
  return entry ? entry->GetImageIdElement() : nullptr;
}

void DocumentOrShadowRoot::ReportEmptyGetElementByIdArg() {
  nsContentUtils::ReportEmptyGetElementByIdArg(AsNode().OwnerDoc());
}

/**
 * A struct that holds all the information about a radio group.
 */
struct nsRadioGroupStruct {
  nsRadioGroupStruct()
      : mRequiredRadioCount(0), mGroupSuffersFromValueMissing(false) {}

  /**
   * A strong pointer to the currently selected radio button.
   */
  RefPtr<HTMLInputElement> mSelectedRadioButton;
  nsCOMArray<nsIFormControl> mRadioButtons;
  uint32_t mRequiredRadioCount;
  bool mGroupSuffersFromValueMissing;
};

void DocumentOrShadowRoot::GetAnimations(
    nsTArray<RefPtr<Animation>>& aAnimations) {
  // As with Element::GetAnimations we initially flush style here.
  // This should ensure that there are no subsequent changes to the tree
  // structure while iterating over the children below.
  if (Document* doc = AsNode().GetComposedDoc()) {
    doc->FlushPendingNotifications(
        ChangesToFlush(FlushType::Style, false /* flush animations */));
  }

  GetAnimationsOptions options;
  options.mSubtree = true;

  for (RefPtr<nsIContent> child = AsNode().GetFirstChild(); child;
       child = child->GetNextSibling()) {
    if (RefPtr<Element> element = Element::FromNode(child)) {
      nsTArray<RefPtr<Animation>> result;
      element->GetAnimations(options, result, Element::Flush::No);
      aAnimations.AppendElements(std::move(result));
    }
  }

  aAnimations.Sort(AnimationPtrComparator<RefPtr<Animation>>());
}

nsresult DocumentOrShadowRoot::WalkRadioGroup(const nsAString& aName,
                                              nsIRadioVisitor* aVisitor,
                                              bool aFlushContent) {
  nsRadioGroupStruct* radioGroup = GetOrCreateRadioGroup(aName);

  for (int i = 0; i < radioGroup->mRadioButtons.Count(); i++) {
    if (!aVisitor->Visit(radioGroup->mRadioButtons[i])) {
      return NS_OK;
    }
  }

  return NS_OK;
}

void DocumentOrShadowRoot::SetCurrentRadioButton(const nsAString& aName,
                                                 HTMLInputElement* aRadio) {
  nsRadioGroupStruct* radioGroup = GetOrCreateRadioGroup(aName);
  radioGroup->mSelectedRadioButton = aRadio;
}

HTMLInputElement* DocumentOrShadowRoot::GetCurrentRadioButton(
    const nsAString& aName) {
  return GetOrCreateRadioGroup(aName)->mSelectedRadioButton;
}

nsresult DocumentOrShadowRoot::GetNextRadioButton(
    const nsAString& aName, const bool aPrevious,
    HTMLInputElement* aFocusedRadio, HTMLInputElement** aRadioOut) {
  // XXX Can we combine the HTML radio button method impls of
  //     Document and nsHTMLFormControl?
  // XXX Why is HTML radio button stuff in Document, as
  //     opposed to nsHTMLDocument?
  *aRadioOut = nullptr;

  nsRadioGroupStruct* radioGroup = GetOrCreateRadioGroup(aName);

  // Return the radio button relative to the focused radio button.
  // If no radio is focused, get the radio relative to the selected one.
  RefPtr<HTMLInputElement> currentRadio;
  if (aFocusedRadio) {
    currentRadio = aFocusedRadio;
  } else {
    currentRadio = radioGroup->mSelectedRadioButton;
    if (!currentRadio) {
      return NS_ERROR_FAILURE;
    }
  }
  int32_t index = radioGroup->mRadioButtons.IndexOf(currentRadio);
  if (index < 0) {
    return NS_ERROR_FAILURE;
  }

  int32_t numRadios = radioGroup->mRadioButtons.Count();
  RefPtr<HTMLInputElement> radio;
  do {
    if (aPrevious) {
      if (--index < 0) {
        index = numRadios - 1;
      }
    } else if (++index >= numRadios) {
      index = 0;
    }
    NS_ASSERTION(
        static_cast<nsGenericHTMLFormElement*>(radioGroup->mRadioButtons[index])
            ->IsHTMLElement(nsGkAtoms::input),
        "mRadioButtons holding a non-radio button");
    radio = static_cast<HTMLInputElement*>(radioGroup->mRadioButtons[index]);
  } while (radio->Disabled() && radio != currentRadio);

  radio.forget(aRadioOut);
  return NS_OK;
}

void DocumentOrShadowRoot::AddToRadioGroup(const nsAString& aName,
                                           HTMLInputElement* aRadio) {
  nsRadioGroupStruct* radioGroup = GetOrCreateRadioGroup(aName);
  radioGroup->mRadioButtons.AppendObject(aRadio);

  if (aRadio->IsRequired()) {
    radioGroup->mRequiredRadioCount++;
  }
}

void DocumentOrShadowRoot::RemoveFromRadioGroup(const nsAString& aName,
                                                HTMLInputElement* aRadio) {
  nsRadioGroupStruct* radioGroup = GetOrCreateRadioGroup(aName);
  radioGroup->mRadioButtons.RemoveObject(aRadio);

  if (aRadio->IsRequired()) {
    NS_ASSERTION(radioGroup->mRequiredRadioCount != 0,
                 "mRequiredRadioCount about to wrap below 0!");
    radioGroup->mRequiredRadioCount--;
  }
}

uint32_t DocumentOrShadowRoot::GetRequiredRadioCount(
    const nsAString& aName) const {
  nsRadioGroupStruct* radioGroup = GetRadioGroup(aName);
  return radioGroup ? radioGroup->mRequiredRadioCount : 0;
}

void DocumentOrShadowRoot::RadioRequiredWillChange(const nsAString& aName,
                                                   bool aRequiredAdded) {
  nsRadioGroupStruct* radioGroup = GetOrCreateRadioGroup(aName);

  if (aRequiredAdded) {
    radioGroup->mRequiredRadioCount++;
  } else {
    NS_ASSERTION(radioGroup->mRequiredRadioCount != 0,
                 "mRequiredRadioCount about to wrap below 0!");
    radioGroup->mRequiredRadioCount--;
  }
}

bool DocumentOrShadowRoot::GetValueMissingState(const nsAString& aName) const {
  nsRadioGroupStruct* radioGroup = GetRadioGroup(aName);
  return radioGroup && radioGroup->mGroupSuffersFromValueMissing;
}

void DocumentOrShadowRoot::SetValueMissingState(const nsAString& aName,
                                                bool aValue) {
  nsRadioGroupStruct* radioGroup = GetOrCreateRadioGroup(aName);
  radioGroup->mGroupSuffersFromValueMissing = aValue;
}

nsRadioGroupStruct* DocumentOrShadowRoot::GetRadioGroup(
    const nsAString& aName) const {
  nsRadioGroupStruct* radioGroup = nullptr;
  mRadioGroups.Get(aName, &radioGroup);
  return radioGroup;
}

nsRadioGroupStruct* DocumentOrShadowRoot::GetOrCreateRadioGroup(
    const nsAString& aName) {
  return mRadioGroups.LookupForAdd(aName)
      .OrInsert([]() { return new nsRadioGroupStruct(); })
      .get();
}

int32_t DocumentOrShadowRoot::StyleOrderIndexOfSheet(
    const StyleSheet& aSheet) const {
  if (aSheet.IsConstructed()) {
    // NOTE: constructable sheets can have duplicates, so we need to start
    // looking from behind.
    int32_t index = mAdoptedStyleSheets.LastIndexOf(&aSheet);
    return (index < 0) ? index : index + SheetCount();
  }
  return mStyleSheets.IndexOf(&aSheet);
}

void DocumentOrShadowRoot::GetAdoptedStyleSheets(
    nsTArray<RefPtr<StyleSheet>>& aAdoptedStyleSheets) const {
  aAdoptedStyleSheets = mAdoptedStyleSheets;
}

void DocumentOrShadowRoot::Traverse(DocumentOrShadowRoot* tmp,
                                    nsCycleCollectionTraversalCallback& cb) {
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mStyleSheets)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDOMStyleSheets)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mAdoptedStyleSheets)

  auto NoteSheetIfApplicable = [&](StyleSheet& aSheet) {
    if (!aSheet.IsApplicable()) {
      return;
    }
    // The style set or mServoStyles keep more references to it if the sheet
    // is applicable.
    if (tmp->mKind == Kind::ShadowRoot) {
      NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mServoStyles->sheets[i]");
      cb.NoteXPCOMChild(&aSheet);
    } else if (tmp->AsNode().AsDocument()->StyleSetFilled()) {
      NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(
          cb, "mStyleSet->mRawSet.stylist.stylesheets.author[i]");
      cb.NoteXPCOMChild(&aSheet);
    }
  };

  for (auto& sheet : tmp->mStyleSheets) {
    NoteSheetIfApplicable(*sheet);
  }

  tmp->EnumerateUniqueAdoptedStyleSheetsBackToFront(NoteSheetIfApplicable);

  for (auto iter = tmp->mIdentifierMap.ConstIter(); !iter.Done(); iter.Next()) {
    iter.Get()->Traverse(&cb);
  }

  for (auto iter = tmp->mRadioGroups.Iter(); !iter.Done(); iter.Next()) {
    nsRadioGroupStruct* radioGroup = iter.UserData();
    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(
        cb, "mRadioGroups entry->mSelectedRadioButton");
    cb.NoteXPCOMChild(ToSupports(radioGroup->mSelectedRadioButton));

    uint32_t i, count = radioGroup->mRadioButtons.Count();
    for (i = 0; i < count; ++i) {
      NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(
          cb, "mRadioGroups entry->mRadioButtons[i]");
      cb.NoteXPCOMChild(radioGroup->mRadioButtons[i]);
    }
  }
}

void DocumentOrShadowRoot::Unlink(DocumentOrShadowRoot* tmp) {
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDOMStyleSheets);
  for (RefPtr<StyleSheet>& sheet : tmp->mAdoptedStyleSheets) {
    sheet->RemoveAdopter(*tmp);
  }
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mAdoptedStyleSheets);
  tmp->mIdentifierMap.Clear();
  tmp->mRadioGroups.Clear();
}

}  // namespace dom
}  // namespace mozilla
