/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SummaryFrame.h"

#include "mozilla/Attributes.h"
#include "mozilla/dom/HTMLSummaryElement.h"

NS_IMPL_FRAMEARENA_HELPERS(SummaryFrame)

SummaryFrame*
NS_NewSummaryFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) SummaryFrame(aContext);
}

SummaryFrame::SummaryFrame(nsStyleContext* aContext)
  : nsBlockFrame(aContext)
{
}

SummaryFrame::~SummaryFrame()
{
}

nsIAtom*
SummaryFrame::GetType() const
{
  return nsGkAtoms::summaryFrame;
}
