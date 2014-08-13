/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "nsHTMLParts.h"
#include "nsContainerFrame.h"
#include "nsCSSRendering.h"
#include "nsIDocument.h"
#include "nsPageFrame.h"
#include "nsIDOMEvent.h"
#include "nsStyleConsts.h"
#include "nsGkAtoms.h"
#include "nsIPresShell.h"
#include "nsBoxFrame.h"
#include "nsStackLayout.h"
#include "nsIAnonymousContentCreator.h"
#include "mozilla/dom/NodeInfo.h"
#include "nsIServiceManager.h"
#include "nsNodeInfoManager.h"
#include "nsContentCreatorFunctions.h"
#include "nsContentUtils.h"
#include "nsContentList.h"
#include "mozilla/dom/Element.h"

//#define DEBUG_REFLOW

using namespace mozilla::dom;

class nsDocElementBoxFrame : public nsBoxFrame,
                             public nsIAnonymousContentCreator
{
public:
  virtual void DestroyFrom(nsIFrame* aDestructRoot) MOZ_OVERRIDE;

  friend nsIFrame* NS_NewBoxFrame(nsIPresShell* aPresShell,
                                  nsStyleContext* aContext);

  nsDocElementBoxFrame(nsIPresShell* aShell, nsStyleContext* aContext)
    :nsBoxFrame(aShell, aContext, true) {}

  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS

  // nsIAnonymousContentCreator
  virtual nsresult CreateAnonymousContent(nsTArray<ContentInfo>& aElements) MOZ_OVERRIDE;
  virtual void AppendAnonymousContentTo(nsTArray<nsIContent*>& aElements,
                                        uint32_t aFilter) MOZ_OVERRIDE;

  virtual bool IsFrameOfType(uint32_t aFlags) const MOZ_OVERRIDE
  {
    // Override nsBoxFrame.
    if (aFlags & (nsIFrame::eReplacedContainsBlock | nsIFrame::eReplaced))
      return false;
    return nsBoxFrame::IsFrameOfType(aFlags);
  }

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const MOZ_OVERRIDE;
#endif
private:
  nsCOMPtr<Element> mPopupgroupContent;
  nsCOMPtr<Element> mTooltipContent;
};

//----------------------------------------------------------------------

nsContainerFrame*
NS_NewDocElementBoxFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsDocElementBoxFrame (aPresShell, aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsDocElementBoxFrame)

void
nsDocElementBoxFrame::DestroyFrom(nsIFrame* aDestructRoot)
{
  nsContentUtils::DestroyAnonymousContent(&mPopupgroupContent);
  nsContentUtils::DestroyAnonymousContent(&mTooltipContent);
  nsBoxFrame::DestroyFrom(aDestructRoot);
}

nsresult
nsDocElementBoxFrame::CreateAnonymousContent(nsTArray<ContentInfo>& aElements)
{
  nsIDocument* doc = mContent->GetDocument();
  if (!doc) {
    // The page is currently being torn down.  Why bother.
    return NS_ERROR_FAILURE;
  }
  nsNodeInfoManager *nodeInfoManager = doc->NodeInfoManager();

  // create the top-secret popupgroup node. shhhhh!
  nsRefPtr<NodeInfo> nodeInfo;
  nodeInfo = nodeInfoManager->GetNodeInfo(nsGkAtoms::popupgroup,
                                          nullptr, kNameSpaceID_XUL,
                                          nsIDOMNode::ELEMENT_NODE);
  NS_ENSURE_TRUE(nodeInfo, NS_ERROR_OUT_OF_MEMORY);

  nsresult rv = NS_NewXULElement(getter_AddRefs(mPopupgroupContent),
                                 nodeInfo.forget());
  NS_ENSURE_SUCCESS(rv, rv);

  if (!aElements.AppendElement(mPopupgroupContent))
    return NS_ERROR_OUT_OF_MEMORY;

  // create the top-secret default tooltip node. shhhhh!
  nodeInfo = nodeInfoManager->GetNodeInfo(nsGkAtoms::tooltip, nullptr,
                                          kNameSpaceID_XUL,
                                          nsIDOMNode::ELEMENT_NODE);
  NS_ENSURE_TRUE(nodeInfo, NS_ERROR_OUT_OF_MEMORY);

  rv = NS_NewXULElement(getter_AddRefs(mTooltipContent), nodeInfo.forget());
  NS_ENSURE_SUCCESS(rv, rv);

  mTooltipContent->SetAttr(kNameSpaceID_None, nsGkAtoms::_default,
                           NS_LITERAL_STRING("true"), false);

  if (!aElements.AppendElement(mTooltipContent))
    return NS_ERROR_OUT_OF_MEMORY;

  return NS_OK;
}

void
nsDocElementBoxFrame::AppendAnonymousContentTo(nsTArray<nsIContent*>& aElements,
                                               uint32_t aFilter)
{
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
nsresult
nsDocElementBoxFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("DocElementBox"), aResult);
}
#endif
