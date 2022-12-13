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

DOMSubtreeIterator::DOMSubtreeIterator() : DOMIterator() {
  mIter = &mSubtreeIter;
}

nsresult DOMSubtreeIterator::Init(nsRange& aRange) {
  return mIter->Init(&aRange);
}

/******************************************************************************
 * mozilla::EditorElementStyle
 *****************************************************************************/

bool EditorElementStyle::IsCSSEditable(const Element& aElement) const {
  return CSSEditUtils::IsCSSEditableStyle(aElement, *this);
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
  if (element.NodeInfo()->NameAtom() == mHTMLProperty) {
    return true;
  }
  // Special case for linking or naming an <a> element.
  if ((mHTMLProperty == nsGkAtoms::href && HTMLEditUtils::IsLink(&element)) ||
      (mHTMLProperty == nsGkAtoms::name &&
       HTMLEditUtils::IsNamedAnchor(&element))) {
    return true;
  }
  return false;
}

}  // namespace mozilla
