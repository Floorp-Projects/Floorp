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
