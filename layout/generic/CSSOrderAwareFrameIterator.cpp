/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Iterator class for frame lists that respect CSS "order" during layout */

#include "CSSOrderAwareFrameIterator.h"

namespace mozilla {

template<>
bool
CSSOrderAwareFrameIterator::CSSOrderComparator(nsIFrame* const& a,
                                               nsIFrame* const& b)
{ return a->StylePosition()->mOrder < b->StylePosition()->mOrder; }

template<>
bool
CSSOrderAwareFrameIterator::CSSBoxOrdinalGroupComparator(nsIFrame* const& a,
                                                         nsIFrame* const& b)
{ return a->StyleXUL()->mBoxOrdinal < b->StyleXUL()->mBoxOrdinal; }

template<>
bool
CSSOrderAwareFrameIterator::IsForward() const { return true; }

template<>
nsFrameList::iterator
CSSOrderAwareFrameIterator::begin(const nsFrameList& aList)
{ return aList.begin(); }

template<>
nsFrameList::iterator CSSOrderAwareFrameIterator::end(const nsFrameList& aList)
{ return aList.end(); }

template<>
bool
ReverseCSSOrderAwareFrameIterator::CSSOrderComparator(nsIFrame* const& a,
                                                      nsIFrame* const& b)
{ return a->StylePosition()->mOrder > b->StylePosition()->mOrder; }

template<>
bool
ReverseCSSOrderAwareFrameIterator::CSSBoxOrdinalGroupComparator(nsIFrame* const& a,
                                                                nsIFrame* const& b)
{ return a->StyleXUL()->mBoxOrdinal > b->StyleXUL()->mBoxOrdinal; }

template<>
bool
ReverseCSSOrderAwareFrameIterator::IsForward() const
{ return false; }

template<>
nsFrameList::reverse_iterator
ReverseCSSOrderAwareFrameIterator::begin(const nsFrameList& aList)
{ return aList.rbegin(); }

template<>
nsFrameList::reverse_iterator
ReverseCSSOrderAwareFrameIterator::end(const nsFrameList& aList)
{ return aList.rend(); }

} // namespace mozilla
