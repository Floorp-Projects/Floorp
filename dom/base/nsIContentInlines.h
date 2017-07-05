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
#include "nsIAtom.h"
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
static inline bool FlattenedTreeParentIsParent(const nsINode* aNode)
{
  // Try to short-circuit past the complicated and not-exactly-fast logic for
  // computing the flattened parent.
  //
  // WARNING! This logic is replicated in Servo to avoid out of line calls.
  // If you change the cases here, you need to change them in Servo as well!

  // Check if the node is an explicit child of an XBL-bound element, re-bound to
  // an XBL insertion point, or is a top-level element in a shadow tree, whose
  // flattened parent is the host element (as opposed to the actual parent which
  // is the shadow root).
  if (aNode->HasFlag(NODE_MAY_BE_IN_BINDING_MNGR | NODE_IS_IN_SHADOW_TREE)) {
    return false;
  }

  // Check if we want the flattened parent for style, and the node is the root
  // of a native anonymous content subtree parented to the document's root element.
  if (Type == nsIContent::eForStyle && aNode->HasFlag(NODE_IS_NATIVE_ANONYMOUS_ROOT) &&
      aNode->OwnerDoc()->GetRootElement() == aNode->GetParentNode())
  {
    return false;
  }

  // Check if the node is an explicit child of an element with a shadow root,
  // re-bound to an insertion point.
  nsIContent* parent = aNode->GetParent();
  if (parent && parent->GetShadowRoot()) {
    return false;
  }

  // Common case.
  return true;
}

template<nsIContent::FlattenedParentType Type>
static inline nsINode*
GetFlattenedTreeParentNode(const nsINode* aNode)
{
  if (MOZ_LIKELY(FlattenedTreeParentIsParent<Type>(aNode))) {
    return aNode->GetParentNode();
  }

  MOZ_ASSERT(aNode->IsContent());
  return aNode->AsContent()->GetFlattenedTreeParentNodeInternal(Type);
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

inline bool
nsIContent::IsEventAttributeName(nsIAtom* aName)
{
  const char16_t* name = aName->GetUTF16String();
  if (name[0] != 'o' || name[1] != 'n') {
    return false;
  }

  return IsEventAttributeNameInternal(aName);
}

inline nsINode*
nsINode::GetFlattenedTreeParentNodeForStyle() const
{
  return ::GetFlattenedTreeParentNode<nsIContent::eForStyle>(this);
}

inline bool
nsINode::NodeOrAncestorHasDirAuto() const
{
  return AncestorHasDirAuto() || (IsElement() && AsElement()->HasDirAuto());
}

#endif // nsIContentInlines_h
