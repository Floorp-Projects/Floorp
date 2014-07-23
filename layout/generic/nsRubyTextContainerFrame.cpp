/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code is subject to the terms of the Mozilla Public License
 * version 2.0 (the "License"). You can obtain a copy of the License at
 * http://mozilla.org/MPL/2.0/. */

/* rendering object for CSS "display: ruby-text-container" */

#include "nsRubyTextContainerFrame.h"
#include "nsPresContext.h"
#include "nsStyleContext.h"

//----------------------------------------------------------------------

// Frame class boilerplate
// =======================

NS_QUERYFRAME_HEAD(nsRubyTextContainerFrame)
  NS_QUERYFRAME_ENTRY(nsRubyTextContainerFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsBlockFrame)

NS_IMPL_FRAMEARENA_HELPERS(nsRubyTextContainerFrame)

nsContainerFrame*
NS_NewRubyTextContainerFrame(nsIPresShell* aPresShell,
                             nsStyleContext* aContext)
{
  return new (aPresShell) nsRubyTextContainerFrame(aContext);
}


//----------------------------------------------------------------------

// nsRubyTextContainerFrame Method Implementations
// ===============================================

nsIAtom*
nsRubyTextContainerFrame::GetType() const
{
  return nsGkAtoms::rubyTextContainerFrame;
}

#ifdef DEBUG_FRAME_DUMP
nsresult
nsRubyTextContainerFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("RubyTextContainer"), aResult);
}
#endif
