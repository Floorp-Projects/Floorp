/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ChildIterator_h
#define ChildIterator_h

/**
 * Iterates over the children on a node. If a child is an insertion point,
 * iterates over the children inserted there instead, or the default content
 * if no children are inserted there.
 *
 * The FlattenedChildIterator expands any anonymous content bound from an XBL
 * binding's <xbl:content> element.
 */

#include "nsIContent.h"

namespace mozilla {
namespace dom {

// This class iterates normal DOM child nodes of a given DOM node with
// <xbl:children> nodes replaced by the elements that have been filtered into that
// insertion point. Any bindings on the given element are ignored for purposes
// of determining which insertion point children are filtered into.
class ExplicitChildIterator
{
public:
  ExplicitChildIterator(nsIContent* aParent)
    : mParent(aParent),
      mChild(nullptr),
      mDefaultChild(nullptr),
      mIndexInInserted(0),
      mIsFirst(true)
  {
  }

  nsIContent* GetNextChild();

  // Looks for aChildToFind respecting insertion points until aChildToFind
  // or aBound is found. If aBound is nullptr then the seek is unbounded. Returns
  // whether aChildToFind was found as an explicit child prior to encountering
  // aBound.
  bool Seek(nsIContent* aChildToFind, nsIContent* aBound = nullptr)
  {
    // It would be nice to assert that we find aChildToFind, but bz thinks that
    // we might not find aChildToFind when called from ContentInserted
    // if first-letter frames are about.

    nsIContent* child;
    do {
      child = GetNextChild();
    } while (child && child != aChildToFind && child != aBound);

    return child == aChildToFind;
  }

protected:
  // The parent of the children being iterated. For the FlattenedChildIterator,
  // if there is a binding attached to the original parent, mParent points to
  // the <xbl:content> element for the binding.
  nsIContent* mParent;

  // The current child. When we encounter an <xbl:children> insertion point,
  // mChild remains as the insertion point whose content we're iterating (and
  // our state is controled by mDefaultChild or mIndexInInserted depending on
  // whether the insertion point expands to its default content or not).
  nsIContent* mChild;

  // If non-null, this points to the current default content for the current
  // insertion point that we're iterating (i.e. mChild, which must be an
  // nsXBLChildrenElement). Once this transitions back to null,
  // we continue iterating at mChild's next sibling.
  nsIContent* mDefaultChild;

  // If not zero, we're iterating inserted children for an insertion point. This
  // is an index into mChild's inserted children array (mChild must be an
  // nsXBLChildrenElement). The index is one past the "current" child (as
  // opposed to mChild which represents the "current" child).
  uint32_t mIndexInInserted;

  // A flag to let us know that we haven't started iterating yet.
  bool mIsFirst;
};

// Iterates over the flattened children of a node, which accounts for anonymous
// children and nodes moved by insertion points. If a node has anonymous
// children, those are iterated over.
class FlattenedChildIterator : public ExplicitChildIterator
{
public:
  FlattenedChildIterator(nsIContent* aParent);

  // Returns the current target of this iterator (which might be an explicit
  // child of the node, default content for an <xbl:children> element or
  // an inserted child for an <xbl:children> element.
  nsIContent* Get();

  // The inverse of GetNextChild. Properly steps in and out of <xbl:children>
  // elements.
  nsIContent* GetPreviousChild();

  bool XBLInvolved() { return mXBLInvolved; }

private:
  // For certain optimizations, nsCSSFrameConstructor needs to know if the
  // child list of the element that we're iterating matches its .childNodes.
  bool mXBLInvolved;
};

} // namespace dom
} // namespace mozilla

#endif
