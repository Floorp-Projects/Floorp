/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code is subject to the terms of the Mozilla Public License
 * version 2.0 (the "License"). You can obtain a copy of the License at
 * http://mozilla.org/MPL/2.0/. */

/* rendering object for CSS "display: ruby" */
#include "nsRubyFrame.h"
#include "nsPresContext.h"
#include "nsStyleContext.h"

//----------------------------------------------------------------------

// Frame class boilerplate
// =======================

NS_QUERYFRAME_HEAD(nsRubyFrame)
  NS_QUERYFRAME_ENTRY(nsRubyFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsContainerFrame)

NS_IMPL_FRAMEARENA_HELPERS(nsRubyFrame)

nsContainerFrame*
NS_NewRubyFrame(nsIPresShell* aPresShell,
                nsStyleContext* aContext)
{
  return new (aPresShell) nsRubyFrame(aContext);
}


//----------------------------------------------------------------------

// nsRubyFrame Method Implementations
// ==================================

nsIAtom*
nsRubyFrame::GetType() const
{
  return nsGkAtoms::rubyFrame;
}

bool
nsRubyFrame::IsFrameOfType(uint32_t aFlags) const
{
  return nsContainerFrame::IsFrameOfType(aFlags &
    ~(nsIFrame::eBidiInlineContainer | nsIFrame::eLineParticipant));
}

#ifdef DEBUG_FRAME_DUMP
nsresult
nsRubyFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("Ruby"), aResult);
}
#endif
