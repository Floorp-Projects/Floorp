/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* base class for ruby rendering objects that directly contain content */

#ifndef nsRubyContentFrame_h___
#define nsRubyContentFrame_h___

#include "nsInlineFrame.h"

class nsRubyContentFrame : public nsInlineFrame {
 public:
  NS_DECL_ABSTRACT_FRAME(nsRubyContentFrame)

  // Indicates whether this is an "intra-level whitespace" frame, i.e.
  // an anonymous frame that was created to contain non-droppable
  // whitespaces directly inside a ruby level container. This impacts
  // ruby pairing behavior.
  // See http://dev.w3.org/csswg/css-ruby/#anon-gen-interpret-space
  bool IsIntraLevelWhitespace() const;

 protected:
  nsRubyContentFrame(ComputedStyle* aStyle, nsPresContext* aPresContext,
                     ClassID aID)
      : nsInlineFrame(aStyle, aPresContext, aID) {}
};

#endif /* nsRubyContentFrame_h___ */
