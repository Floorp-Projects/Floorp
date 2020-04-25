/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "mozilla/PresShell.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/FromParser.h"
#include "mozilla/dom/NodeInfo.h"
#include "nsHTMLParts.h"
#include "nsContainerFrame.h"
#include "nsCSSRendering.h"
#include "nsPageFrame.h"
#include "nsStyleConsts.h"
#include "nsGkAtoms.h"
#include "nsBoxFrame.h"
#include "nsStackLayout.h"
#include "nsIAnonymousContentCreator.h"
#include "nsNodeInfoManager.h"
#include "nsContentCreatorFunctions.h"

//#define DEBUG_REFLOW

using namespace mozilla;
using namespace mozilla::dom;

class nsDocElementBoxFrame final : public nsBoxFrame,
                                   public nsIAnonymousContentCreator {
 public:
  virtual void DestroyFrom(nsIFrame* aDestructRoot,
                           PostDestroyData& aPostDestroyData) override;

  friend nsIFrame* NS_NewBoxFrame(mozilla::PresShell* aPresShell,
                                  ComputedStyle* aStyle);

  explicit nsDocElementBoxFrame(ComputedStyle* aStyle,
                                nsPresContext* aPresContext)
      : nsBoxFrame(aStyle, aPresContext, kClassID, true) {}

  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS(nsDocElementBoxFrame)

  // nsIAnonymousContentCreator
  virtual nsresult CreateAnonymousContent(
      nsTArray<ContentInfo>& aElements) override;
  virtual void AppendAnonymousContentTo(nsTArray<nsIContent*>& aElements,
                                        uint32_t aFilter) override;

  virtual bool IsFrameOfType(uint32_t aFlags) const override {
    // Override nsBoxFrame.
    if (aFlags & (nsIFrame::eReplacedContainsBlock | nsIFrame::eReplaced))
      return false;
    return nsBoxFrame::IsFrameOfType(aFlags);
  }

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override;
#endif
 private:
  nsCOMPtr<Element> mPopupgroupContent;
  nsCOMPtr<Element> mTooltipContent;
};

//----------------------------------------------------------------------

nsContainerFrame* NS_NewDocElementBoxFrame(PresShell* aPresShell,
                                           ComputedStyle* aStyle) {
  return new (aPresShell)
      nsDocElementBoxFrame(aStyle, aPresShell->GetPresContext());
}

NS_IMPL_FRAMEARENA_HELPERS(nsDocElementBoxFrame)

void nsDocElementBoxFrame::DestroyFrom(nsIFrame* aDestructRoot,
                                       PostDestroyData& aPostDestroyData) {
  aPostDestroyData.AddAnonymousContent(mPopupgroupContent.forget());
  aPostDestroyData.AddAnonymousContent(mTooltipContent.forget());
  nsBoxFrame::DestroyFrom(aDestructRoot, aPostDestroyData);
}

nsresult nsDocElementBoxFrame::CreateAnonymousContent(
    nsTArray<ContentInfo>& aElements) {
  Document* doc = mContent->GetComposedDoc();
  if (!doc) {
    // The page is currently being torn down.  Why bother.
    return NS_ERROR_FAILURE;
  }
  nsNodeInfoManager* nodeInfoManager = doc->NodeInfoManager();

  // create the top-secret popupgroup node. shhhhh!
  RefPtr<NodeInfo> nodeInfo;
  nodeInfo = nodeInfoManager->GetNodeInfo(
      nsGkAtoms::popupgroup, nullptr, kNameSpaceID_XUL, nsINode::ELEMENT_NODE);
  NS_ENSURE_TRUE(nodeInfo, NS_ERROR_OUT_OF_MEMORY);

  nsresult rv = NS_NewXULElement(getter_AddRefs(mPopupgroupContent),
                                 nodeInfo.forget(), dom::NOT_FROM_PARSER);
  NS_ENSURE_SUCCESS(rv, rv);

  // XXX(Bug 1631371) Check if this should use a fallible operation as it
  // pretended earlier.
  aElements.AppendElement(mPopupgroupContent);

  // create the top-secret default tooltip node. shhhhh!
  nodeInfo = nodeInfoManager->GetNodeInfo(
      nsGkAtoms::tooltip, nullptr, kNameSpaceID_XUL, nsINode::ELEMENT_NODE);
  NS_ENSURE_TRUE(nodeInfo, NS_ERROR_OUT_OF_MEMORY);

  rv = NS_NewXULElement(getter_AddRefs(mTooltipContent), nodeInfo.forget(),
                        dom::NOT_FROM_PARSER);
  NS_ENSURE_SUCCESS(rv, rv);

  mTooltipContent->SetAttr(kNameSpaceID_None, nsGkAtoms::_default,
                           NS_LITERAL_STRING("true"), false);

  // XXX(Bug 1631371) Check if this should use a fallible operation as it
  // pretended earlier.
  aElements.AppendElement(mTooltipContent);

  return NS_OK;
}

void nsDocElementBoxFrame::AppendAnonymousContentTo(
    nsTArray<nsIContent*>& aElements, uint32_t aFilter) {
  if (mPopupgroupContent) {
    aElements.AppendElement(mPopupgroupContent);
  }

  if (mTooltipContent) {
    aElements.AppendElement(mTooltipContent);
  }
}

NS_QUERYFRAME_HEAD(nsDocElementBoxFrame)
  NS_QUERYFRAME_ENTRY(nsIAnonymousContentCreator)
NS_QUERYFRAME_TAIL_INHERITING(nsBoxFrame)

#ifdef DEBUG_FRAME_DUMP
nsresult nsDocElementBoxFrame::GetFrameName(nsAString& aResult) const {
  return MakeFrameName(NS_LITERAL_STRING("DocElementBox"), aResult);
}
#endif
