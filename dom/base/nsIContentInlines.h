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
nsIContent::IsInChromeDocument()
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

inline nsINode* nsINode::GetFlattenedTreeParentNode() const
{
  nsINode* parent = GetParentNode();

  // Try to short-circuit past the complicated and not-exactly-fast logic for
  // computing the flattened parent.
  //
  // There are three cases where we need might something other than parentNode:
  //   (1) The node is an explicit child of an XBL-bound element, re-bound
  //       to an XBL insertion point.
  //   (2) The node is a top-level element in a shadow tree, whose flattened
  //       parent is the host element (as opposed to the actual parent which
  //       is the shadow root).
  //   (3) The node is an explicit child of an element with a shadow root,
  //       re-bound to an insertion point.
  bool needSlowCall = HasFlag(NODE_MAY_BE_IN_BINDING_MNGR) ||
                      IsInShadowTree() ||
                      (parent && parent->IsContent() &&
                       parent->AsContent()->GetShadowRoot());
  if (MOZ_UNLIKELY(needSlowCall)) {
    MOZ_ASSERT(IsContent());
    return AsContent()->GetFlattenedTreeParentNodeInternal();
  }

  return parent;
}

inline nsIContent*
nsIContent::GetFlattenedTreeParent() const
{
  nsINode* parent = GetFlattenedTreeParentNode();
  return (parent && parent->IsContent()) ? parent->AsContent() : nullptr;
}


#endif // nsIContentInlines_h
