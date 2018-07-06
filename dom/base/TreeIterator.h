/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TreeIterator_h
#define TreeIterator_h

#include "mozilla/Attributes.h"
#include "nsIContent.h"

/**
 * A generic pre-order tree iterator on top of ChildIterator.
 *
 * See ChildIterator.h for the kind of iterators you can use as the template
 * argument for this class.
 */

namespace mozilla {
namespace dom {

template<typename ChildIterator>
class MOZ_STACK_CLASS TreeIterator
{
  enum class Direction
  {
    Forward,
    Backwards,
  };

  template<Direction aDirection>
  nsIContent* GetNextChild(ChildIterator& aIter)
  {
    return aDirection == Direction::Forward
      ? aIter.GetNextChild()
      : aIter.GetPreviousChild();
  }

  template<Direction> inline void Advance();
  template<Direction> inline void AdvanceSkippingChildren();

public:
  explicit TreeIterator(nsIContent& aRoot)
    : mRoot(aRoot)
    , mCurrent(&aRoot)
  {
  }

  nsIContent* GetCurrent() const
  {
    return mCurrent;
  }

  // Note that this keeps the iterator state consistent in case of failure.
  inline bool Seek(nsIContent&);
  inline nsIContent* GetNext();
  inline nsIContent* GetNextSkippingChildren();
  inline nsIContent* GetPrev();
  inline nsIContent* GetPrevSkippingChildren();

private:
  using IteratorArray = AutoTArray<ChildIterator, 30>;

  nsIContent& mRoot;
  nsIContent* mCurrent;
  IteratorArray mParentIterators;
};

template<typename ChildIterator>
template<typename TreeIterator<ChildIterator>::Direction aDirection>
inline void
TreeIterator<ChildIterator>::AdvanceSkippingChildren()
{
  while (true) {
    if (MOZ_UNLIKELY(mParentIterators.IsEmpty())) {
      mCurrent = nullptr;
      return;
    }

    if (nsIContent* nextSibling =
          GetNextChild<aDirection>(mParentIterators.LastElement())) {
      mCurrent = nextSibling;
      return;
    }
    mParentIterators.RemoveLastElement();
  }
}

template<typename ChildIterator>
inline bool
TreeIterator<ChildIterator>::Seek(nsIContent& aContent)
{
  IteratorArray parentIterators;
  nsIContent* current = &aContent;
  while (current != &mRoot) {
    nsIContent* parent = ChildIterator::GetParent(*current);
    if (!parent) {
      return false;
    }

    ChildIterator children(parent);
    if (!children.Seek(current)) {
      return false;
    }

    parentIterators.AppendElement(std::move(children));
    current = parent;
  }

  parentIterators.Reverse();

  mParentIterators.Clear();
  mParentIterators.SwapElements(parentIterators);
  mCurrent = &aContent;
  return true;
}

template<typename ChildIterator>
template<typename TreeIterator<ChildIterator>::Direction aDirection>
inline void
TreeIterator<ChildIterator>::Advance()
{
  MOZ_ASSERT(mCurrent);
  const bool startAtBeginning = aDirection == Direction::Forward;
  ChildIterator children(mCurrent, startAtBeginning);
  if (nsIContent* first = GetNextChild<aDirection>(children)) {
    mCurrent = first;
    mParentIterators.AppendElement(std::move(children));
    return;
  }

  AdvanceSkippingChildren<aDirection>();
}

template<typename ChildIterator>
inline nsIContent*
TreeIterator<ChildIterator>::GetNext()
{
  Advance<Direction::Forward>();
  return GetCurrent();
}

template<typename ChildIterator>
inline nsIContent*
TreeIterator<ChildIterator>::GetPrev()
{
  Advance<Direction::Backwards>();
  return GetCurrent();
}

template<typename ChildIterator>
inline nsIContent*
TreeIterator<ChildIterator>::GetNextSkippingChildren()
{
  AdvanceSkippingChildren<Direction::Forward>();
  return GetCurrent();
}

template<typename ChildIterator>
inline nsIContent*
TreeIterator<ChildIterator>::GetPrevSkippingChildren()
{
  AdvanceSkippingChildren<Direction::Backwards>();
  return GetCurrent();
}

} // namespace dom
} // namespace mozilla

#endif
