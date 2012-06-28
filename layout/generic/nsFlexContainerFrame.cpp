/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */

/* This Source Code is subject to the terms of the Mozilla Public License
 * version 2.0 (the "License"). You can obtain a copy of the License at
 * http://mozilla.org/MPL/2.0/. */

/* rendering object for CSS display: -moz-flex */

#include "nsFlexContainerFrame.h"
#include "nsPresContext.h"
#include "nsStyleContext.h"
#include "prlog.h"

#ifdef PR_LOGGING 
static PRLogModuleInfo* nsFlexContainerFrameLM = PR_NewLogModule("nsFlexContainerFrame");
#endif /* PR_LOGGING */

NS_IMPL_FRAMEARENA_HELPERS(nsFlexContainerFrame)

nsIFrame*
NS_NewFlexContainerFrame(nsIPresShell* aPresShell,
                         nsStyleContext* aContext)
{
  return new (aPresShell) nsFlexContainerFrame(aContext);
}

//----------------------------------------------------------------------

/* virtual */
nsFlexContainerFrame::~nsFlexContainerFrame()
{
}

NS_QUERYFRAME_HEAD(nsFlexContainerFrame)
  NS_QUERYFRAME_ENTRY(nsFlexContainerFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsFlexContainerFrameSuper)

void
nsFlexContainerFrame::DestroyFrom(nsIFrame* aDestructRoot)
{
  DestroyAbsoluteFrames(aDestructRoot);
  nsFlexContainerFrameSuper::DestroyFrom(aDestructRoot);
}

nsIAtom*
nsFlexContainerFrame::GetType() const
{
  return nsGkAtoms::flexContainerFrame;
}

#ifdef DEBUG
NS_IMETHODIMP
nsFlexContainerFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("FlexContainer"), aResult);
}
#endif // DEBUG
