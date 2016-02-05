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

void
SummaryFrame::SetInitialChildList(ChildListID aListID, nsFrameList& aChildList)
{
  nsBlockFrame::SetInitialChildList(aListID, aChildList);

  // Construct the disclosure triangle if it's the main summary. We leverage the
  // list-item property and nsBulletFrame to draw the triangle. Need to set
  // list-style-type for :moz-list-bullet in html.css.
  // TODO: Bug 1221416 for styling the disclosure triangle.
  if (aListID == kPrincipalList) {
    auto* summary = HTMLSummaryElement::FromContent(GetContent());
    if (summary->IsMainSummary() &&
        StyleDisplay()->mDisplay != NS_STYLE_DISPLAY_LIST_ITEM) {
      CreateBulletFrameForListItem(true, true);
    }
  }
}
