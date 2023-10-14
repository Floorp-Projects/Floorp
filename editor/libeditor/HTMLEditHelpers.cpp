/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HTMLEditHelpers.h"

#include "CSSEditUtils.h"
#include "EditorDOMPoint.h"
#include "HTMLEditor.h"
#include "PendingStyles.h"
#include "WSRunObject.h"

#include "mozilla/ContentIterator.h"
#include "mozilla/OwningNonNull.h"
#include "mozilla/dom/HTMLBRElement.h"
#include "mozilla/dom/Text.h"
#include "nsIContent.h"
#include "nsINode.h"
#include "nsRange.h"

class nsISupports;

namespace mozilla {

using namespace dom;

template void DOMIterator::AppendAllNodesToArray(
    nsTArray<OwningNonNull<nsIContent>>& aArrayOfNodes) const;
template void DOMIterator::AppendAllNodesToArray(
    nsTArray<OwningNonNull<HTMLBRElement>>& aArrayOfNodes) const;
template void DOMIterator::AppendNodesToArray(
    BoolFunctor aFunctor, nsTArray<OwningNonNull<nsIContent>>& aArrayOfNodes,
    void* aClosure) const;
template void DOMIterator::AppendNodesToArray(
    BoolFunctor aFunctor, nsTArray<OwningNonNull<Element>>& aArrayOfNodes,
    void* aClosure) const;
template void DOMIterator::AppendNodesToArray(
    BoolFunctor aFunctor, nsTArray<OwningNonNull<Text>>& aArrayOfNodes,
    void* aClosure) const;

/******************************************************************************
 * some helper classes for iterating the dom tree
 *****************************************************************************/

DOMIterator::DOMIterator(nsINode& aNode) : mIter(&mPostOrderIter) {
  DebugOnly<nsresult> rv = mIter->Init(&aNode);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
}

nsresult DOMIterator::Init(nsRange& aRange) { return mIter->Init(&aRange); }

nsresult DOMIterator::Init(const RawRangeBoundary& aStartRef,
                           const RawRangeBoundary& aEndRef) {
  return mIter->Init(aStartRef, aEndRef);
}

DOMIterator::DOMIterator() : mIter(&mPostOrderIter) {}

template <class NodeClass>
void DOMIterator::AppendAllNodesToArray(
    nsTArray<OwningNonNull<NodeClass>>& aArrayOfNodes) const {
  for (; !mIter->IsDone(); mIter->Next()) {
    if (NodeClass* node = NodeClass::FromNode(mIter->GetCurrentNode())) {
      aArrayOfNodes.AppendElement(*node);
    }
  }
}

template <class NodeClass>
void DOMIterator::AppendNodesToArray(
    BoolFunctor aFunctor, nsTArray<OwningNonNull<NodeClass>>& aArrayOfNodes,
    void* aClosure /* = nullptr */) const {
  for (; !mIter->IsDone(); mIter->Next()) {
    NodeClass* node = NodeClass::FromNode(mIter->GetCurrentNode());
    if (node && aFunctor(*node, aClosure)) {
      aArrayOfNodes.AppendElement(*node);
    }
  }
}

DOMSubtreeIterator::DOMSubtreeIterator() { mIter = &mSubtreeIter; }

nsresult DOMSubtreeIterator::Init(nsRange& aRange) {
  return mIter->Init(&aRange);
}

/******************************************************************************
 * mozilla::EditorElementStyle
 *****************************************************************************/

bool EditorElementStyle::IsCSSSettable(const nsStaticAtom& aTagName) const {
  return CSSEditUtils::IsCSSEditableStyle(aTagName, *this);
}

bool EditorElementStyle::IsCSSSettable(const Element& aElement) const {
  return CSSEditUtils::IsCSSEditableStyle(aElement, *this);
}

bool EditorElementStyle::IsCSSRemovable(const nsStaticAtom& aTagName) const {
  // <font size> cannot be applied with CSS font-size for now, but it should be
  // removable.
  return EditorElementStyle::IsCSSSettable(aTagName) ||
         (IsInlineStyle() && AsInlineStyle().IsStyleOfFontSize());
}

bool EditorElementStyle::IsCSSRemovable(const Element& aElement) const {
  // <font size> cannot be applied with CSS font-size for now, but it should be
  // removable.
  return EditorElementStyle::IsCSSSettable(aElement) ||
         (IsInlineStyle() && AsInlineStyle().IsStyleOfFontSize());
}

/******************************************************************************
 * mozilla::EditorInlineStyle
 *****************************************************************************/

PendingStyleCache EditorInlineStyle::ToPendingStyleCache(
    nsAString&& aValue) const {
  return PendingStyleCache(*mHTMLProperty,
                           mAttribute ? mAttribute->AsStatic() : nullptr,
                           std::move(aValue));
}

bool EditorInlineStyle::IsRepresentedBy(const nsIContent& aContent) const {
  MOZ_ASSERT(!IsStyleToClearAllInlineStyles());

  if (!aContent.IsHTMLElement()) {
    return false;
  }
  const Element& element = *aContent.AsElement();
  if (mHTMLProperty == element.NodeInfo()->NameAtom() ||
      mHTMLProperty == GetSimilarElementNameAtom()) {
    // <a> cannot be nested.  Therefore, if we're the style of <a>, we should
    // treat existing it even if the attribute does not match.
    if (mHTMLProperty == nsGkAtoms::a) {
      return true;
    }
    return !mAttribute || element.HasAttr(mAttribute);
  }
  // Special case for linking or naming an <a> element.
  if ((mHTMLProperty == nsGkAtoms::href && HTMLEditUtils::IsLink(&element)) ||
      (mHTMLProperty == nsGkAtoms::name &&
       HTMLEditUtils::IsNamedAnchor(&element))) {
    return true;
  }
  // If the style is font size, it's also represented by <big> or <small>.
  if (mHTMLProperty == nsGkAtoms::font && mAttribute == nsGkAtoms::size &&
      aContent.IsAnyOfHTMLElements(nsGkAtoms::big, nsGkAtoms::small)) {
    return true;
  }
  return false;
}

Result<bool, nsresult> EditorInlineStyle::IsSpecifiedBy(
    const HTMLEditor& aHTMLEditor, Element& aElement) const {
  MOZ_ASSERT(!IsStyleToClearAllInlineStyles());
  if (!IsCSSSettable(aElement) && !IsCSSRemovable(aElement)) {
    return false;
  }
  // Special case in the CSS mode.  We should treat <u>, <s>, <strike>, <ins>
  // and <del> specifies text-decoration (bug 1802668).
  if (aHTMLEditor.IsCSSEnabled() &&
      IsStyleOfTextDecoration(IgnoreSElement::No) &&
      aElement.IsAnyOfHTMLElements(nsGkAtoms::u, nsGkAtoms::s,
                                   nsGkAtoms::strike, nsGkAtoms::ins,
                                   nsGkAtoms::del)) {
    return true;
  }
  return CSSEditUtils::HaveSpecifiedCSSEquivalentStyles(aHTMLEditor, aElement,
                                                        *this);
}

}  // namespace mozilla
