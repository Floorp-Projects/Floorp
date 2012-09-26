/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TextComposition.h"
#include "nsContentUtils.h"
#include "nsEventDispatcher.h"
#include "nsGUIEvent.h"
#include "nsIPresShell.h"
#include "nsIWidget.h"
#include "nsPresContext.h"

namespace mozilla {

/******************************************************************************
 * TextComposition
 ******************************************************************************/

TextComposition::TextComposition(nsPresContext* aPresContext,
                                 nsINode* aNode,
                                 nsGUIEvent* aEvent) :
  mPresContext(aPresContext), mNode(aNode),
  // temporarily, we should assume that one native IME context is per native
  // widget.
  mNativeContext(aEvent->widget)
{
}

TextComposition::TextComposition(const TextComposition& aOther)
{
  mNativeContext = aOther.mNativeContext;
  mPresContext = aOther.mPresContext;
  mNode = aOther.mNode;
}

bool
TextComposition::MatchesNativeContext(nsIWidget* aWidget) const
{
  // temporarily, we should assume that one native IME context is per one
  // native widget.
  return mNativeContext == static_cast<void*>(aWidget);
}

bool
TextComposition::MatchesEventTarget(nsPresContext* aPresContext,
                                    nsINode* aNode) const
{
  return mPresContext == aPresContext && mNode == aNode;
}

void
TextComposition::DispatchEvent(nsGUIEvent* aEvent,
                               nsEventStatus* aStatus,
                               nsDispatchingCallback* aCallBack)
{
  nsEventDispatcher::Dispatch(mNode, mPresContext,
                              aEvent, nullptr, aStatus, aCallBack);
}

/******************************************************************************
 * TextCompositionArray
 ******************************************************************************/

TextCompositionArray::index_type
TextCompositionArray::IndexOf(nsIWidget* aWidget)
{
  for (index_type i = Length(); i > 0; --i) {
    if (ElementAt(i - 1).MatchesNativeContext(aWidget)) {
      return i - 1;
    }
  }
  return NoIndex;
}

TextCompositionArray::index_type
TextCompositionArray::IndexOf(nsPresContext* aPresContext)
{
  for (index_type i = Length(); i > 0; --i) {
    if (ElementAt(i - 1).GetPresContext() == aPresContext) {
      return i - 1;
    }
  }
  return NoIndex;
}

TextCompositionArray::index_type
TextCompositionArray::IndexOf(nsPresContext* aPresContext,
                              nsINode* aNode)
{
  index_type index = IndexOf(aPresContext);
  if (index == NoIndex) {
    return NoIndex;
  }
  nsINode* node = ElementAt(index).GetEventTargetNode();
  return node == aNode ? index : NoIndex;
}

TextComposition*
TextCompositionArray::GetCompositionFor(nsIWidget* aWidget)
{
  index_type i = IndexOf(aWidget);
  return i != NoIndex ? &ElementAt(i) : nullptr;
}

TextComposition*
TextCompositionArray::GetCompositionFor(nsPresContext* aPresContext)
{
  index_type i = IndexOf(aPresContext);
  return i != NoIndex ? &ElementAt(i) : nullptr;
}

TextComposition*
TextCompositionArray::GetCompositionFor(nsPresContext* aPresContext,
                                           nsINode* aNode)
{
  index_type i = IndexOf(aPresContext, aNode);
  return i != NoIndex ? &ElementAt(i) : nullptr;
}

TextComposition*
TextCompositionArray::GetCompositionInContent(nsPresContext* aPresContext,
                                              nsIContent* aContent)
{
  // There should be only one composition per content object.
  for (index_type i = Length(); i > 0; --i) {
    nsINode* node = ElementAt(i - 1).GetEventTargetNode();
    if (node && nsContentUtils::ContentIsDescendantOf(node, aContent)) {
      return &ElementAt(i - 1);
    }
  }
  return nullptr;
}

} // namespace mozilla
