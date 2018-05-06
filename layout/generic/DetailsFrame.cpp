/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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

nsBlockFrame*
NS_NewDetailsFrame(nsIPresShell* aPresShell, ComputedStyle* aStyle)
{
  return new (aPresShell) DetailsFrame(aStyle);
}

namespace mozilla {

DetailsFrame::DetailsFrame(ComputedStyle* aStyle)
  : nsBlockFrame(aStyle, kClassID)
{
}

DetailsFrame::~DetailsFrame()
{
}

void
DetailsFrame::SetInitialChildList(ChildListID aListID, nsFrameList& aChildList)
{
#ifdef DEBUG
  if (aListID == kPrincipalList) {
    CheckValidMainSummary(aChildList);
  }
#endif

  nsBlockFrame::SetInitialChildList(aListID, aChildList);
}

#ifdef DEBUG
bool
DetailsFrame::CheckValidMainSummary(const nsFrameList& aFrameList) const
{
  for (nsIFrame* child : aFrameList) {
    HTMLSummaryElement* summary =
      HTMLSummaryElement::FromNode(child->GetContent());

    if (child == aFrameList.FirstChild()) {
      if (summary && summary->IsMainSummary()) {
        return true;
      } else if (child->GetContent() == GetContent()) {
        // The child frame's content is the same as our content, which means
        // it's a kind of wrapper frame. Descend into its child list to find
        // main summary.
        if (CheckValidMainSummary(child->PrincipalChildList())) {
          return true;
        }
      }
    } else {
      NS_ASSERTION(!summary || !summary->IsMainSummary(),
                   "Rest of the children are either not summary element "
                   "or are not the main summary!");
    }
  }
  return false;
}
#endif

void
DetailsFrame::DestroyFrom(nsIFrame* aDestructRoot, PostDestroyData& aPostDestroyData)
{
  aPostDestroyData.AddAnonymousContent(mDefaultSummary.forget());
  nsBlockFrame::DestroyFrom(aDestructRoot, aPostDestroyData);
}

nsresult
DetailsFrame::CreateAnonymousContent(nsTArray<ContentInfo>& aElements)
{
  auto* details = HTMLDetailsElement::FromNode(GetContent());
  if (details->GetFirstSummary()) {
    return NS_OK;
  }

  // The <details> element lacks any direct <summary> child. Create a default
  // <summary> element as an anonymous content.
  nsNodeInfoManager* nodeInfoManager =
    GetContent()->NodeInfo()->NodeInfoManager();

  already_AddRefed<NodeInfo> nodeInfo =
    nodeInfoManager->GetNodeInfo(nsGkAtoms::summary, nullptr, kNameSpaceID_XHTML,
                                 nsINode::ELEMENT_NODE);
  mDefaultSummary = new HTMLSummaryElement(nodeInfo);

  nsAutoString defaultSummaryText;
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

bool
DetailsFrame::HasMainSummaryFrame(nsIFrame* aSummaryFrame)
{
  nsIFrame* firstChild = nullptr;
  for (nsIFrame* frag = this; frag; frag = frag->GetNextInFlow()) {
    firstChild = frag->PrincipalChildList().FirstChild();
    if (!firstChild) {
      nsFrameList* overflowFrames = GetOverflowFrames();
      if (overflowFrames) {
        firstChild = overflowFrames->FirstChild();
      }
    }
    if (firstChild) {
      firstChild = nsPlaceholderFrame::GetRealFrameFor(firstChild);
      MOZ_ASSERT(firstChild && firstChild->IsPrimaryFrame(),
                 "this is probably not the frame you were looking for");
      break;
    }
  }
  return aSummaryFrame == firstChild;
}

} // namespace mozilla
