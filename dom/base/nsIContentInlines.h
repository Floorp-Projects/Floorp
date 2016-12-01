/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIContentInlines_h
#define nsIContentInlines_h

#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsContentUtils.h"
#include "mozilla/dom/Element.h"

inline bool
nsIContent::IsInHTMLDocument() const
{
  return OwnerDoc()->IsHTMLDocument();
}

inline bool
nsIContent::IsInChromeDocument() const
{
  return nsContentUtils::IsChromeDoc(OwnerDoc());
}

inline mozilla::dom::ShadowRoot* nsIContent::GetShadowRoot() const
{
  if (!IsElement()) {
    return nullptr;
  }

  return AsElement()->FastGetShadowRoot();
}

template<nsIContent::FlattenedParentType Type>
static inline nsINode*
GetFlattenedTreeParentNode(const nsINode* aNode)
{
  nsINode* parent = aNode->GetParentNode();
  // Try to short-circuit past the complicated and not-exactly-fast logic for
  // computing the flattened parent.
  //
  // There are four cases where we need might something other than parentNode:
  //   (1) The node is an explicit child of an XBL-bound element, re-bound
  //       to an XBL insertion point.
  //   (2) The node is a top-level element in a shadow tree, whose flattened
  //       parent is the host element (as opposed to the actual parent which
  //       is the shadow root).
  //   (3) The node is an explicit child of an element with a shadow root,
  //       re-bound to an insertion point.
  //   (4) We want the flattened parent for style, and the node is the root
  //       of a native anonymous content subtree parented to the document's
  //       root element.
  bool needSlowCall = aNode->HasFlag(NODE_MAY_BE_IN_BINDING_MNGR) ||
                      aNode->IsInShadowTree() ||
                      (parent &&
                       parent->IsContent() &&
                       parent->AsContent()->GetShadowRoot()) ||
                      (Type == nsIContent::eForStyle &&
                       aNode->IsContent() &&
                       aNode->AsContent()->IsRootOfNativeAnonymousSubtree() &&
                       aNode->OwnerDoc()->GetRootElement() == parent);
  if (MOZ_UNLIKELY(needSlowCall)) {
    MOZ_ASSERT(aNode->IsContent());
    return aNode->AsContent()->GetFlattenedTreeParentNodeInternal(Type);
  }
  return parent;
}

inline nsINode*
nsINode::GetFlattenedTreeParentNode() const
{
  return ::GetFlattenedTreeParentNode<nsIContent::eNotForStyle>(this);
}

inline nsIContent*
nsIContent::GetFlattenedTreeParent() const
{
  nsINode* parent = GetFlattenedTreeParentNode();
  return (parent && parent->IsContent()) ? parent->AsContent() : nullptr;
}

inline nsINode*
nsINode::GetFlattenedTreeParentNodeForStyle() const
{
  return ::GetFlattenedTreeParentNode<nsIContent::eForStyle>(this);
}

#endif // nsIContentInlines_h
