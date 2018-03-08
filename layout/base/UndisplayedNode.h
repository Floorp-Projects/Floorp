/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Linked list node for undisplayed element */

#ifndef mozilla_UndisplayedNode_h
#define mozilla_UndisplayedNode_h

#include "nsIContent.h"
#include "nsStyleContext.h"

namespace mozilla {

/**
 * Node in a linked list, containing the style for an element that
 * does not have a frame but whose parent does have a frame.
 */
struct UndisplayedNode : public LinkedListElement<UndisplayedNode>
{
  UndisplayedNode(nsIContent* aContent, nsStyleContext* aStyle)
    : mContent(aContent)
    , mStyle(aStyle)
  {
    MOZ_COUNT_CTOR(mozilla::UndisplayedNode);
  }

  ~UndisplayedNode() { MOZ_COUNT_DTOR(mozilla::UndisplayedNode); }

  nsCOMPtr<nsIContent> mContent;
  RefPtr<nsStyleContext> mStyle;
};

} // namespace mozilla

#endif // mozilla_UndisplayedNode_h
