/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DocumentOrShadowRoot.h"
#include "mozilla/EventStateManager.h"
#include "mozilla/dom/StyleSheetList.h"
#include "nsDocument.h"
#include "nsFocusManager.h"
#include "nsLayoutUtils.h"
#include "nsSVGUtils.h"

namespace mozilla {
namespace dom {

DocumentOrShadowRoot::DocumentOrShadowRoot(mozilla::dom::ShadowRoot& aShadowRoot)
  : mAsNode(aShadowRoot)
  , mKind(Kind::ShadowRoot)
{}

DocumentOrShadowRoot::DocumentOrShadowRoot(nsIDocument& aDoc)
  : mAsNode(aDoc)
  , mKind(Kind::Document)
{}

StyleSheetList&
DocumentOrShadowRoot::EnsureDOMStyleSheets()
{
  if (!mDOMStyleSheets) {
    mDOMStyleSheets = new StyleSheetList(*this);
  }
  return *mDOMStyleSheets;
}

void
DocumentOrShadowRoot::AppendSheet(StyleSheet& aSheet)
{
  aSheet.SetAssociatedDocumentOrShadowRoot(
    this, StyleSheet::OwnedByDocumentOrShadowRoot);
  mStyleSheets.AppendElement(&aSheet);
}

void
DocumentOrShadowRoot::InsertSheetAt(size_t aIndex, StyleSheet& aSheet)
{
  aSheet.SetAssociatedDocumentOrShadowRoot(
    this, StyleSheet::OwnedByDocumentOrShadowRoot);
  mStyleSheets.InsertElementAt(aIndex, &aSheet);
}

already_AddRefed<StyleSheet>
DocumentOrShadowRoot::RemoveSheet(StyleSheet& aSheet)
{
  auto index = mStyleSheets.IndexOf(&aSheet);
  if (index == mStyleSheets.NoIndex) {
    return nullptr;
  }
  RefPtr<StyleSheet> sheet = Move(mStyleSheets[index]);
  mStyleSheets.RemoveElementAt(index);
  sheet->ClearAssociatedDocumentOrShadowRoot();
  return sheet.forget();
}

Element*
DocumentOrShadowRoot::GetElementById(const nsAString& aElementId)
{
  if (MOZ_UNLIKELY(aElementId.IsEmpty())) {
    nsContentUtils::ReportEmptyGetElementByIdArg(AsNode().OwnerDoc());
    return nullptr;
  }

  if (nsIdentifierMapEntry* entry = mIdentifierMap.GetEntry(aElementId)) {
    if (Element* el = entry->GetIdElement()) {
      return el;
    }
  }

  return nullptr;
}

already_AddRefed<nsContentList>
DocumentOrShadowRoot::GetElementsByTagNameNS(const nsAString& aNamespaceURI,
                                   const nsAString& aLocalName)
{
  ErrorResult rv;
  RefPtr<nsContentList> list =
    GetElementsByTagNameNS(aNamespaceURI, aLocalName, rv);
  if (rv.Failed()) {
    return nullptr;
  }
  return list.forget();
}

already_AddRefed<nsContentList>
DocumentOrShadowRoot::GetElementsByTagNameNS(const nsAString& aNamespaceURI,
                                   const nsAString& aLocalName,
                                   mozilla::ErrorResult& aResult)
{
  int32_t nameSpaceId = kNameSpaceID_Wildcard;

  if (!aNamespaceURI.EqualsLiteral("*")) {
    aResult =
      nsContentUtils::NameSpaceManager()->RegisterNameSpace(aNamespaceURI,
                                                            nameSpaceId);
    if (aResult.Failed()) {
      return nullptr;
    }
  }

  NS_ASSERTION(nameSpaceId != kNameSpaceID_Unknown, "Unexpected namespace ID!");
  return NS_GetContentList(&AsNode(), nameSpaceId, aLocalName);
}

already_AddRefed<nsContentList>
DocumentOrShadowRoot::GetElementsByClassName(const nsAString& aClasses)
{
  return nsContentUtils::GetElementsByClassName(&AsNode(), aClasses);
}

nsIContent*
DocumentOrShadowRoot::Retarget(nsIContent* aContent) const
{
  for (nsIContent* cur = aContent;
       cur;
       cur = cur->GetContainingShadowHost()) {
    if (cur->SubtreeRoot() == &AsNode()) {
      return cur;
    }
  }
  return nullptr;
}

Element*
DocumentOrShadowRoot::GetRetargetedFocusedElement()
{
  if (nsCOMPtr<nsPIDOMWindowOuter> window = AsNode().OwnerDoc()->GetWindow()) {
    nsCOMPtr<nsPIDOMWindowOuter> focusedWindow;
    nsIContent* focusedContent =
      nsFocusManager::GetFocusedDescendant(window,
                                           nsFocusManager::eOnlyCurrentWindow,
                                           getter_AddRefs(focusedWindow));
    // be safe and make sure the element is from this document
    if (focusedContent && focusedContent->OwnerDoc() == AsNode().OwnerDoc()) {
      if (focusedContent->ChromeOnlyAccess()) {
        focusedContent = focusedContent->FindFirstNonChromeOnlyAccessContent();
      }

      if (focusedContent) {
        if (!nsDocument::IsShadowDOMEnabled(focusedContent)) {
          return focusedContent->AsElement();
        }

        if (nsIContent* retarget = Retarget(focusedContent)) {
          return retarget->AsElement();
        }
      }
    }
  }

  return nullptr;
}

Element*
DocumentOrShadowRoot::GetPointerLockElement()
{
  nsCOMPtr<Element> pointerLockedElement =
    do_QueryReferent(EventStateManager::sPointerLockedElement);
  if (!pointerLockedElement) {
    return nullptr;
  }

  nsIContent* retargetedPointerLockedElement = Retarget(pointerLockedElement);
  return
    retargetedPointerLockedElement && retargetedPointerLockedElement->IsElement() ?
      retargetedPointerLockedElement->AsElement() : nullptr;
}

Element*
DocumentOrShadowRoot::GetFullscreenElement()
{
  if (!AsNode().IsInComposedDoc()) {
    return nullptr;
  }

  Element* element = AsNode().OwnerDoc()->FullScreenStackTop();
  NS_ASSERTION(!element ||
               element->State().HasState(NS_EVENT_STATE_FULL_SCREEN),
    "Fullscreen element should have fullscreen styles applied");

  nsIContent* retargeted = Retarget(element);
  if (retargeted && retargeted->IsElement()) {
    return retargeted->AsElement();
  }

  return nullptr;
}

Element*
DocumentOrShadowRoot::ElementFromPoint(float aX, float aY)
{
  return ElementFromPointHelper(aX, aY, false, true);
}

void
DocumentOrShadowRoot::ElementsFromPoint(float aX, float aY,
                                        nsTArray<RefPtr<Element>>& aElements)
{
  ElementsFromPointHelper(aX, aY, nsIDocument::FLUSH_LAYOUT, aElements);
}

Element*
DocumentOrShadowRoot::ElementFromPointHelper(float aX, float aY,
                                             bool aIgnoreRootScrollFrame,
                                             bool aFlushLayout)
{
  AutoTArray<RefPtr<Element>, 1> elementArray;
  ElementsFromPointHelper(aX, aY,
                          ((aIgnoreRootScrollFrame ? nsIDocument::IGNORE_ROOT_SCROLL_FRAME : 0) |
                           (aFlushLayout ? nsIDocument::FLUSH_LAYOUT : 0) |
                           nsIDocument::IS_ELEMENT_FROM_POINT),
                          elementArray);
  if (elementArray.IsEmpty()) {
    return nullptr;
  }
  return elementArray[0];
}

void
DocumentOrShadowRoot::ElementsFromPointHelper(float aX, float aY,
                                              uint32_t aFlags,
                                              nsTArray<RefPtr<mozilla::dom::Element>>& aElements)
{
  // As per the the spec, we return null if either coord is negative
  if (!(aFlags & nsIDocument::IGNORE_ROOT_SCROLL_FRAME) && (aX < 0 || aY < 0)) {
    return;
  }

  nscoord x = nsPresContext::CSSPixelsToAppUnits(aX);
  nscoord y = nsPresContext::CSSPixelsToAppUnits(aY);
  nsPoint pt(x, y);

  nsCOMPtr<nsIDocument> doc = AsNode().OwnerDoc();

  // Make sure the layout information we get is up-to-date, and
  // ensure we get a root frame (for everything but XUL)
  if (aFlags & nsIDocument::FLUSH_LAYOUT) {
    doc->FlushPendingNotifications(FlushType::Layout);
  }

  nsIPresShell* ps = doc->GetShell();
  if (!ps) {
    return;
  }
  nsIFrame* rootFrame = ps->GetRootFrame();

  // XUL docs, unlike HTML, have no frame tree until everything's done loading
  if (!rootFrame) {
    return; // return null to premature XUL callers as a reminder to wait
  }

  nsTArray<nsIFrame*> outFrames;
  // Emulate what GetFrameAtPoint does, since we want all the frames under our
  // point.
  nsLayoutUtils::GetFramesForArea(rootFrame, nsRect(pt, nsSize(1, 1)), outFrames,
    nsLayoutUtils::IGNORE_PAINT_SUPPRESSION | nsLayoutUtils::IGNORE_CROSS_DOC |
    ((aFlags & nsIDocument::IGNORE_ROOT_SCROLL_FRAME) ? nsLayoutUtils::IGNORE_ROOT_SCROLL_FRAME : 0));

  // Dunno when this would ever happen, as we should at least have a root frame under us?
  if (outFrames.IsEmpty()) {
    return;
  }

  // Used to filter out repeated elements in sequence.
  nsIContent* lastAdded = nullptr;

  for (uint32_t i = 0; i < outFrames.Length(); i++) {
    nsIContent* node = doc->GetContentInThisDocument(outFrames[i]);

    if (!node || !node->IsElement()) {
      // If this helper is called via ElementsFromPoint, we need to make sure
      // our frame is an element. Otherwise return whatever the top frame is
      // even if it isn't the top-painted element.
      // SVG 'text' element's SVGTextFrame doesn't respond to hit-testing, so
      // if 'node' is a child of such an element then we need to manually defer
      // to the parent here.
      if (!(aFlags & nsIDocument::IS_ELEMENT_FROM_POINT) &&
          !nsSVGUtils::IsInSVGTextSubtree(outFrames[i])) {
        continue;
      }
      node = node->GetParent();
      if (ShadowRoot* shadow = ShadowRoot::FromNodeOrNull(node)) {
        node = shadow->Host();
      }
    }

    //XXXsmaug There is plenty of unspec'ed behavior here
    //         https://github.com/w3c/webcomponents/issues/735
    //         https://github.com/w3c/webcomponents/issues/736
    node = Retarget(node);

    if (node && node != lastAdded) {
      aElements.AppendElement(node->AsElement());
      lastAdded = node;
      // If this helper is called via ElementFromPoint, just return the first
      // element we find.
      if (aFlags & nsIDocument::IS_ELEMENT_FROM_POINT) {
        return;
      }
    }
  }
}

Element*
DocumentOrShadowRoot::AddIDTargetObserver(nsAtom* aID,
                                          IDTargetObserver aObserver,
                                          void* aData, bool aForImage)
{
  nsDependentAtomString id(aID);

  if (!CheckGetElementByIdArg(id)) {
    return nullptr;
  }

  nsIdentifierMapEntry* entry = mIdentifierMap.PutEntry(aID);
  NS_ENSURE_TRUE(entry, nullptr);

  entry->AddContentChangeCallback(aObserver, aData, aForImage);
  return aForImage ? entry->GetImageIdElement() : entry->GetIdElement();
}

void
DocumentOrShadowRoot::RemoveIDTargetObserver(nsAtom* aID,
                                             IDTargetObserver aObserver,
                                             void* aData, bool aForImage)
{
  nsDependentAtomString id(aID);

  if (!CheckGetElementByIdArg(id)) {
    return;
  }

  nsIdentifierMapEntry* entry = mIdentifierMap.GetEntry(aID);
  if (!entry) {
    return;
  }

  entry->RemoveContentChangeCallback(aObserver, aData, aForImage);
}

void
DocumentOrShadowRoot::ReportEmptyGetElementByIdArg()
{
  nsContentUtils::ReportEmptyGetElementByIdArg(AsNode().OwnerDoc());
}

}
}
