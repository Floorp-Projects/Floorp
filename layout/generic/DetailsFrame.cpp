/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DetailsFrame.h"

#include "mozilla/Attributes.h"
#include "mozilla/dom/HTMLDetailsElement.h"
#include "mozilla/dom/HTMLSummaryElement.h"
#include "nsContentUtils.h"
#include "nsPlaceholderFrame.h"
#include "nsTextNode.h"

using namespace mozilla;
using namespace mozilla::dom;

NS_IMPL_FRAMEARENA_HELPERS(DetailsFrame)

NS_QUERYFRAME_HEAD(DetailsFrame)
  NS_QUERYFRAME_ENTRY(DetailsFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsBlockFrame)

DetailsFrame*
NS_NewDetailsFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) DetailsFrame(aContext);
}

DetailsFrame::DetailsFrame(nsStyleContext* aContext)
  : nsBlockFrame(aContext)
{
}

DetailsFrame::~DetailsFrame()
{
}

nsIAtom*
DetailsFrame::GetType() const
{
  return nsGkAtoms::detailsFrame;
}

void
DetailsFrame::SetInitialChildList(ChildListID aListID, nsFrameList& aChildList)
{
  if (aListID == kPrincipalList) {
    auto* details = HTMLDetailsElement::FromContent(GetContent());
    bool isOpen = details->Open();

    if (isOpen) {
      // If details is open, the first summary needs to be rendered as if it is
      // the first child.
      for (nsIFrame* child : aChildList) {
        auto* realFrame = nsPlaceholderFrame::GetRealFrameFor(child);
        auto* cif = realFrame->GetContentInsertionFrame();
        if (cif && cif->GetType() == nsGkAtoms::summaryFrame) {
          // Take out the first summary frame and insert it to the beginning of
          // the list.
          aChildList.RemoveFrame(child);
          aChildList.InsertFrame(nullptr, nullptr, child);
          break;
        }
      }
    }

#ifdef DEBUG
    nsIFrame* realFrame =
      nsPlaceholderFrame::GetRealFrameFor(isOpen ?
                                          aChildList.FirstChild() :
                                          aChildList.OnlyChild());
    MOZ_ASSERT(realFrame, "Principal list of details should not be empty!");
    nsIFrame* summaryFrame = realFrame->GetContentInsertionFrame();
    MOZ_ASSERT(summaryFrame->GetType() == nsGkAtoms::summaryFrame,
               "The frame should be summary frame!");
#endif

  }

  nsBlockFrame::SetInitialChildList(aListID, aChildList);
}
