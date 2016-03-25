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
  NS_QUERYFRAME_ENTRY(nsIAnonymousContentCreator)
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
    HTMLDetailsElement* details = HTMLDetailsElement::FromContent(GetContent());
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
    for (nsIFrame* child : aChildList) {
      HTMLSummaryElement* summary =
        HTMLSummaryElement::FromContent(child->GetContent());

      if (child == aChildList.FirstChild()) {
        if (summary && summary->IsMainSummary()) {
          break;
        }
      } else {
        MOZ_ASSERT(!summary || !summary->IsMainSummary(),
                   "Rest of the children are neither summary elements nor"
                   "the main summary!");
      }
    }
#endif

  }

  nsBlockFrame::SetInitialChildList(aListID, aChildList);
}

void
DetailsFrame::DestroyFrom(nsIFrame* aDestructRoot)
{
  nsContentUtils::DestroyAnonymousContent(&mDefaultSummary);
  nsBlockFrame::DestroyFrom(aDestructRoot);
}

nsresult
DetailsFrame::CreateAnonymousContent(nsTArray<ContentInfo>& aElements)
{
  auto* details = HTMLDetailsElement::FromContent(GetContent());
  if (details->GetFirstSummary()) {
    return NS_OK;
  }

  // The <details> element lacks any direct <summary> child. Create a default
  // <summary> element as an anonymous content.
  nsNodeInfoManager* nodeInfoManager =
    GetContent()->NodeInfo()->NodeInfoManager();

  already_AddRefed<NodeInfo> nodeInfo =
    nodeInfoManager->GetNodeInfo(nsGkAtoms::summary, nullptr, kNameSpaceID_XHTML,
                                 nsIDOMNode::ELEMENT_NODE);
  mDefaultSummary = new HTMLSummaryElement(nodeInfo);

  nsXPIDLString defaultSummaryText;
  nsContentUtils::GetLocalizedString(nsContentUtils::eFORMS_PROPERTIES,
                                     "DefaultSummary", defaultSummaryText);
  RefPtr<nsTextNode> description = new nsTextNode(nodeInfoManager);
  description->SetText(defaultSummaryText, false);
  mDefaultSummary->AppendChildTo(description, false);

  aElements.AppendElement(mDefaultSummary);

  return NS_OK;
}

void
DetailsFrame::AppendAnonymousContentTo(nsTArray<nsIContent*>& aElements,
                                       uint32_t aFilter)
{
  if (mDefaultSummary) {
    aElements.AppendElement(mDefaultSummary);
  }
}
