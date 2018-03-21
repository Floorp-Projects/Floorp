/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * data structures passed to nsIStyleRuleProcessor methods (to pull loop
 * invariant computations out of the loop)
 */

#ifndef nsRuleProcessorData_h_
#define nsRuleProcessorData_h_


// Define this dummy class so there are fewer call sites to change when the old
// style system code is compiled out.
struct TreeMatchContext
{
public:
  class AutoAncestorPusher
  {
  public:
    explicit AutoAncestorPusher(TreeMatchContext* aTreeMatchContext) {}
    void PushAncestor(nsIContent* aContent) {}
  };

  class AutoParentDisplayBasedStyleFixupSkipper
  {
  public:
    explicit AutoParentDisplayBasedStyleFixupSkipper(
        TreeMatchContext& aTreeMatchContext,
        bool aSkipParentDisplayBasedStyleFixup = true) {}
  };

  enum ForFrameConstructionTag { ForFrameConstruction };

  TreeMatchContext(nsIDocument* aDocument, ForFrameConstructionTag) {}

  void InitAncestors(mozilla::dom::Element* aElement) {}
};


#endif /* !defined(nsRuleProcessorData_h_) */
