/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Iterator class for frame lists that respect CSS "order" during layout */

#include "CSSOrderAwareFrameIterator.h"
#include "nsIFrameInlines.h"

static bool CanUse(const nsIFrame* aFrame) {
  return aFrame->IsFlexOrGridContainer() ||
         (aFrame->GetContent() && aFrame->GetContent()->IsAnyOfXULElements(
                                      nsGkAtoms::treecols, nsGkAtoms::treecol));
}

namespace mozilla {

template <>
bool CSSOrderAwareFrameIterator::CanUse(const nsIFrame* aFrame) {
  return ::CanUse(aFrame);
}

template <>
bool ReverseCSSOrderAwareFrameIterator::CanUse(const nsIFrame* aFrame) {
  return ::CanUse(aFrame);
}

template <>
int CSSOrderAwareFrameIterator::CSSOrderComparator(nsIFrame* const& a,
                                                   nsIFrame* const& b) {
  return a->StylePosition()->mOrder - b->StylePosition()->mOrder;
}

template <>
int CSSOrderAwareFrameIterator::CSSBoxOrdinalGroupComparator(
    nsIFrame* const& a, nsIFrame* const& b) {
  return a->StyleXUL()->mBoxOrdinal - b->StyleXUL()->mBoxOrdinal;
}

template <>
bool CSSOrderAwareFrameIterator::IsForward() const {
  return true;
}

template <>
nsFrameList::iterator CSSOrderAwareFrameIterator::begin(
    const nsFrameList& aList) {
  return aList.begin();
}

template <>
nsFrameList::iterator CSSOrderAwareFrameIterator::end(
    const nsFrameList& aList) {
  return aList.end();
}

template <>
int ReverseCSSOrderAwareFrameIterator::CSSOrderComparator(nsIFrame* const& a,
                                                          nsIFrame* const& b) {
  return b->StylePosition()->mOrder - a->StylePosition()->mOrder;
}

template <>
int ReverseCSSOrderAwareFrameIterator::CSSBoxOrdinalGroupComparator(
    nsIFrame* const& a, nsIFrame* const& b) {
  return b->StyleXUL()->mBoxOrdinal - a->StyleXUL()->mBoxOrdinal;
}

template <>
bool ReverseCSSOrderAwareFrameIterator::IsForward() const {
  return false;
}

template <>
nsFrameList::reverse_iterator ReverseCSSOrderAwareFrameIterator::begin(
    const nsFrameList& aList) {
  return aList.rbegin();
}

template <>
nsFrameList::reverse_iterator ReverseCSSOrderAwareFrameIterator::end(
    const nsFrameList& aList) {
  return aList.rend();
}

}  // namespace mozilla
