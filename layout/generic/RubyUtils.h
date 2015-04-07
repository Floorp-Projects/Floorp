/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_RubyUtils_h_
#define mozilla_RubyUtils_h_

#include "nsGkAtoms.h"
#include "nsRubyTextContainerFrame.h"

class nsRubyBaseContainerFrame;

namespace mozilla {

/**
 * Reserved ISize
 *
 * With some exceptions, each ruby internal box has two isizes, which
 * are the reflowed isize and the final isize. The reflowed isize is
 * what a box itself needs. It is determined when the box gets reflowed.
 *
 * The final isize is what a box should be as the final result. For a
 * ruby base/text box, the final isize is the size of its ruby column.
 * For a ruby base/text container, the final isize is the size of its
 * ruby segment. The final isize is never smaller than the reflowed
 * isize. It is initially determined when a ruby column/segment gets
 * fully reflowed, and may be advanced when a box is expanded, e.g.
 * for justification.
 *
 * The difference between the reflowed isize and the final isize is
 * reserved in the line layout after reflowing a box, hence it is called
 * "Reserved ISize" here. It is used to expand the ruby boxes from their
 * reflowed isize to the final isize during alignment of the line.
 *
 * There are three exceptions for the final isize:
 * 1. A ruby text container has a larger final isize only if it is for
 *    a span or collapsed annotations.
 * 2. A ruby base container has a larger final isize only if at least
 *    one of its ruby text containers does.
 * 3. If a ruby text container has a larger final isize, its children
 *    must not have.
 */

class RubyUtils
{
public:
  static inline bool IsExpandableRubyBox(nsIFrame* aFrame)
  {
    nsIAtom* type = aFrame->GetType();
    return type == nsGkAtoms::rubyBaseFrame ||
           type == nsGkAtoms::rubyTextFrame ||
           type == nsGkAtoms::rubyBaseContainerFrame ||
           type == nsGkAtoms::rubyTextContainerFrame;
  }

  static void SetReservedISize(nsIFrame* aFrame, nscoord aISize);
  static void ClearReservedISize(nsIFrame* aFrame);
  static nscoord GetReservedISize(nsIFrame* aFrame);
};

/**
 * This class iterates all ruby text containers paired with
 * the given ruby base container.
 */
class MOZ_STACK_CLASS RubyTextContainerIterator
{
public:
  explicit RubyTextContainerIterator(nsRubyBaseContainerFrame* aBaseContainer);

  void Next();
  bool AtEnd() const { return !mFrame; }
  nsRubyTextContainerFrame* GetTextContainer() const
  {
    return static_cast<nsRubyTextContainerFrame*>(mFrame);
  }

private:
  nsIFrame* mFrame;
};

}

#endif /* !defined(mozilla_RubyUtils_h_) */
