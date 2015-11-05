/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code is subject to the terms of the Mozilla Public License
 * version 2.0 (the "License"). You can obtain a copy of the License at
 * http://mozilla.org/MPL/2.0/. */

/* base class for ruby rendering objects that directly contain content */

#include "nsRubyContentFrame.h"
#include "nsPresContext.h"
#include "nsStyleContext.h"
#include "nsCSSAnonBoxes.h"

using namespace mozilla;

//----------------------------------------------------------------------

// nsRubyContentFrame Method Implementations
// ======================================

/* virtual */ bool
nsRubyContentFrame::IsFrameOfType(uint32_t aFlags) const
{
  if (aFlags & eBidiInlineContainer) {
    return false;
  }
  return nsRubyContentFrameSuper::IsFrameOfType(aFlags);
}

bool
nsRubyContentFrame::IsIntraLevelWhitespace() const
{
  nsIAtom* pseudoType = StyleContext()->GetPseudo();
  if (pseudoType != nsCSSAnonBoxes::rubyBase &&
      pseudoType != nsCSSAnonBoxes::rubyText) {
    return false;
  }

  nsIFrame* child = mFrames.OnlyChild();
  return child && child->GetContent()->TextIsOnlyWhitespace();
}
