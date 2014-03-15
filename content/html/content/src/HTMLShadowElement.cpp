/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ShadowRoot.h"

#include "nsContentUtils.h"
#include "mozilla/dom/HTMLShadowElement.h"
#include "mozilla/dom/HTMLShadowElementBinding.h"

NS_IMPL_NS_NEW_HTML_ELEMENT(Shadow)

using namespace mozilla::dom;

HTMLShadowElement::HTMLShadowElement(already_AddRefed<nsINodeInfo>& aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo), mIsInsertionPoint(false)
{
  SetIsDOMBinding();
}

HTMLShadowElement::~HTMLShadowElement()
{
  if (mProjectedShadow) {
    mProjectedShadow->RemoveMutationObserver(this);
  }
}

NS_IMPL_CYCLE_COLLECTION_CLASS(HTMLShadowElement)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(HTMLShadowElement,
                                                  nsGenericHTMLElement)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mProjectedShadow)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(HTMLShadowElement,
                                                nsGenericHTMLElement)
  if (tmp->mProjectedShadow) {
    tmp->mProjectedShadow->RemoveMutationObserver(tmp);
    tmp->mProjectedShadow = nullptr;
  }
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_ADDREF_INHERITED(HTMLShadowElement, Element)
NS_IMPL_RELEASE_INHERITED(HTMLShadowElement, Element)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(HTMLShadowElement)
NS_INTERFACE_MAP_END_INHERITING(nsGenericHTMLElement)

NS_IMPL_ELEMENT_CLONE(HTMLShadowElement)

JSObject*
HTMLShadowElement::WrapNode(JSContext *aCx, JS::Handle<JSObject*> aScope)
{
  return HTMLShadowElementBinding::Wrap(aCx, aScope, this);
}

void
HTMLShadowElement::SetProjectedShadow(ShadowRoot* aProjectedShadow)
{
  if (mProjectedShadow) {
    mProjectedShadow->RemoveMutationObserver(this);
  }

  mProjectedShadow = aProjectedShadow;
  if (mProjectedShadow) {
    // Watch for mutations on the projected shadow because
    // it affects the nodes that are distributed to this shadow
    // insertion point.
    mProjectedShadow->AddMutationObserver(this);
  }
}

static bool
IsInFallbackContent(nsIContent* aContent)
{
  nsINode* parentNode = aContent->GetParentNode();
  while (parentNode) {
    if (parentNode->IsElement() &&
        parentNode->AsElement()->IsHTML(nsGkAtoms::content)) {
      return true;
    }
    parentNode = parentNode->GetParentNode();
  }

  return false;
}

nsresult
HTMLShadowElement::BindToTree(nsIDocument* aDocument,
                              nsIContent* aParent,
                              nsIContent* aBindingParent,
                              bool aCompileEventHandlers)
{
  nsresult rv = nsGenericHTMLElement::BindToTree(aDocument, aParent,
                                                 aBindingParent,
                                                 aCompileEventHandlers);
  NS_ENSURE_SUCCESS(rv, rv);

  ShadowRoot* containingShadow = GetContainingShadow();
  if (containingShadow) {
    // Keep track of all descendant <shadow> elements in tree order so
    // that when the current shadow insertion point is removed, the next
    // one can be found quickly.
    TreeOrderComparator comparator;
    containingShadow->ShadowDescendants().InsertElementSorted(this, comparator);

    if (containingShadow->ShadowDescendants()[0] != this) {
      // Only the first <shadow> (in tree order) of a ShadowRoot can be an insertion point.
      return NS_OK;
    }

    if (IsInFallbackContent(this)) {
      // If the first shadow element in tree order is invalid (in fallback content),
      // the containing ShadowRoot will not have a shadow insertion point.
      containingShadow->SetShadowElement(nullptr);
    } else {
      mIsInsertionPoint = true;
      containingShadow->SetShadowElement(this);
    }

    containingShadow->SetInsertionPointChanged();
  }

  return NS_OK;
}

void
HTMLShadowElement::UnbindFromTree(bool aDeep, bool aNullParent)
{
  if (mIsInsertionPoint) {
    ShadowRoot* containingShadow = GetContainingShadow();
    // Make sure that containingShadow exists, it may have been nulled
    // during unlinking in which case the ShadowRoot is going away.
    if (containingShadow) {
      nsTArray<HTMLShadowElement*>& shadowDescendants =
        containingShadow->ShadowDescendants();
      shadowDescendants.RemoveElement(this);
      containingShadow->SetShadowElement(nullptr);

      // Find the next shadow insertion point.
      if (shadowDescendants.Length() > 0 &&
          !IsInFallbackContent(shadowDescendants[0])) {
        containingShadow->SetShadowElement(shadowDescendants[0]);
      }

      containingShadow->SetInsertionPointChanged();
    }

    mIsInsertionPoint = false;
  }

  nsGenericHTMLElement::UnbindFromTree(aDeep, aNullParent);
}

void
HTMLShadowElement::DistributeSingleNode(nsIContent* aContent)
{
  // Handle the case where the shadow element is a child of
  // a node with a ShadowRoot. The nodes that have been distributed to
  // this shadow insertion point will need to be reprojected into the
  // insertion points of the parent's ShadowRoot.
  ShadowRoot* parentShadowRoot = GetParent()->GetShadowRoot();
  if (parentShadowRoot) {
    parentShadowRoot->DistributeSingleNode(aContent);
    return;
  }

  // Handle the case where the parent of this shadow element is a ShadowRoot
  // that is projected into a shadow insertion point in the younger ShadowRoot.
  ShadowRoot* containingShadow = GetContainingShadow();
  ShadowRoot* youngerShadow = containingShadow->GetYoungerShadow();
  if (youngerShadow && GetParent() == containingShadow) {
    HTMLShadowElement* youngerShadowElement = youngerShadow->GetShadowElement();
    if (youngerShadowElement) {
      youngerShadowElement->DistributeSingleNode(aContent);
    }
  }
}

void
HTMLShadowElement::RemoveDistributedNode(nsIContent* aContent)
{
  // Handle the case where the shadow element is a child of
  // a node with a ShadowRoot. The nodes that have been distributed to
  // this shadow insertion point will need to be removed from the
  // insertion points of the parent's ShadowRoot.
  ShadowRoot* parentShadowRoot = GetParent()->GetShadowRoot();
  if (parentShadowRoot) {
    parentShadowRoot->RemoveDistributedNode(aContent);
    return;
  }

  // Handle the case where the parent of this shadow element is a ShadowRoot
  // that is projected into a shadow insertion point in the younger ShadowRoot.
  ShadowRoot* containingShadow = GetContainingShadow();
  ShadowRoot* youngerShadow = containingShadow->GetYoungerShadow();
  if (youngerShadow && GetParent() == containingShadow) {
    HTMLShadowElement* youngerShadowElement = youngerShadow->GetShadowElement();
    if (youngerShadowElement) {
      youngerShadowElement->RemoveDistributedNode(aContent);
    }
  }
}

void
HTMLShadowElement::DistributeAllNodes()
{
  // Handle the case where the shadow element is a child of
  // a node with a ShadowRoot. The nodes that have been distributed to
  // this shadow insertion point will need to be reprojected into the
  // insertion points of the parent's ShadowRoot.
  ShadowRoot* parentShadowRoot = GetParent()->GetShadowRoot();
  if (parentShadowRoot) {
    parentShadowRoot->DistributeAllNodes();
    return;
  }

  // Handle the case where the parent of this shadow element is a ShadowRoot
  // that is projected into a shadow insertion point in the younger ShadowRoot.
  ShadowRoot* containingShadow = GetContainingShadow();
  ShadowRoot* youngerShadow = containingShadow->GetYoungerShadow();
  if (youngerShadow && GetParent() == containingShadow) {
    HTMLShadowElement* youngerShadowElement = youngerShadow->GetShadowElement();
    if (youngerShadowElement) {
      youngerShadowElement->DistributeAllNodes();
    }
  }
}

void
HTMLShadowElement::ContentAppended(nsIDocument* aDocument,
                                   nsIContent* aContainer,
                                   nsIContent* aFirstNewContent,
                                   int32_t aNewIndexInContainer)
{
  // Watch for content appended to the projected shadow (the ShadowRoot that
  // will be rendered in place of this shadow insertion point) because the
  // nodes may need to be distributed into other insertion points.
  nsIContent* currentChild = aFirstNewContent;
  while (currentChild) {
    if (ShadowRoot::IsPooledNode(currentChild, aContainer, mProjectedShadow)) {
      DistributeSingleNode(currentChild);
    }
    currentChild = currentChild->GetNextSibling();
  }
}

void
HTMLShadowElement::ContentInserted(nsIDocument* aDocument,
                                   nsIContent* aContainer,
                                   nsIContent* aChild,
                                   int32_t aIndexInContainer)
{
  // Watch for content appended to the projected shadow (the ShadowRoot that
  // will be rendered in place of this shadow insertion point) because the
  // nodes may need to be distributed into other insertion points.
  if (!ShadowRoot::IsPooledNode(aChild, aContainer, mProjectedShadow)) {
    return;
  }

  DistributeSingleNode(aChild);
}

void
HTMLShadowElement::ContentRemoved(nsIDocument* aDocument,
                                  nsIContent* aContainer,
                                  nsIContent* aChild,
                                  int32_t aIndexInContainer,
                                  nsIContent* aPreviousSibling)
{
  // Watch for content removed to the projected shadow (the ShadowRoot that
  // will be rendered in place of this shadow insertion point) because the
  // nodes may need to be removed from other insertion points.
  if (!ShadowRoot::IsPooledNode(aChild, aContainer, mProjectedShadow)) {
    return;
  }

  RemoveDistributedNode(aChild);
}

