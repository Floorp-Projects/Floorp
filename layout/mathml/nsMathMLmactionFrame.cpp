/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsMathMLmactionFrame.h"
#include "nsCOMPtr.h"
#include "nsPresContext.h"
#include "nsNameSpaceManager.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIWebBrowserChrome.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsTextFragment.h"
#include "nsIDOMEvent.h"
#include "mozilla/gfx/2D.h"

//
// <maction> -- bind actions to a subexpression - implementation
//

enum nsMactionActionTypes {
  NS_MATHML_ACTION_TYPE_CLASS_ERROR            = 0x10,
  NS_MATHML_ACTION_TYPE_CLASS_USE_SELECTION    = 0x20,
  NS_MATHML_ACTION_TYPE_CLASS_IGNORE_SELECTION = 0x40,
  NS_MATHML_ACTION_TYPE_CLASS_BITMASK          = 0xF0,

  NS_MATHML_ACTION_TYPE_NONE       = NS_MATHML_ACTION_TYPE_CLASS_ERROR|0x01,

  NS_MATHML_ACTION_TYPE_TOGGLE     = NS_MATHML_ACTION_TYPE_CLASS_USE_SELECTION|0x01,
  NS_MATHML_ACTION_TYPE_UNKNOWN    = NS_MATHML_ACTION_TYPE_CLASS_USE_SELECTION|0x02,

  NS_MATHML_ACTION_TYPE_STATUSLINE = NS_MATHML_ACTION_TYPE_CLASS_IGNORE_SELECTION|0x01,
  NS_MATHML_ACTION_TYPE_TOOLTIP    = NS_MATHML_ACTION_TYPE_CLASS_IGNORE_SELECTION|0x02
};


// helper function to parse actiontype attribute
static int32_t
GetActionType(nsIContent* aContent)
{
  nsAutoString value;

  if (aContent) {
    if (!aContent->GetAttr(kNameSpaceID_None, nsGkAtoms::actiontype_, value))
      return NS_MATHML_ACTION_TYPE_NONE;
  }

  if (value.EqualsLiteral("toggle"))
    return NS_MATHML_ACTION_TYPE_TOGGLE;
  if (value.EqualsLiteral("statusline"))
    return NS_MATHML_ACTION_TYPE_STATUSLINE;
  if (value.EqualsLiteral("tooltip"))
    return NS_MATHML_ACTION_TYPE_TOOLTIP;

  return NS_MATHML_ACTION_TYPE_UNKNOWN;
}

nsIFrame*
NS_NewMathMLmactionFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsMathMLmactionFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsMathMLmactionFrame)

nsMathMLmactionFrame::~nsMathMLmactionFrame()
{
  // unregister us as a mouse event listener ...
  //  printf("maction:%p unregistering as mouse event listener ...\n", this);
  if (mListener) {
    mContent->RemoveSystemEventListener(NS_LITERAL_STRING("click"), mListener,
                                        false);
    mContent->RemoveSystemEventListener(NS_LITERAL_STRING("mouseover"), mListener,
                                        false);
    mContent->RemoveSystemEventListener(NS_LITERAL_STRING("mouseout"), mListener,
                                        false);
  }
}

void
nsMathMLmactionFrame::Init(nsIContent*       aContent,
                           nsContainerFrame* aParent,
                           nsIFrame*         aPrevInFlow)
{
  // Init our local attributes

  mChildCount = -1; // these will be updated in GetSelectedFrame()
  mActionType = GetActionType(aContent);

  // Let the base class do the rest
  return nsMathMLSelectedFrame::Init(aContent, aParent, aPrevInFlow);
}

nsresult
nsMathMLmactionFrame::ChildListChanged(int32_t aModType)
{
  // update cached values
  mChildCount = -1;
  mSelectedFrame = nullptr;

  return nsMathMLSelectedFrame::ChildListChanged(aModType);
}

// return the frame whose number is given by the attribute selection="number"
nsIFrame*
nsMathMLmactionFrame::GetSelectedFrame()
{
  nsAutoString value;
  int32_t selection;

  if ((mActionType & NS_MATHML_ACTION_TYPE_CLASS_BITMASK) ==
       NS_MATHML_ACTION_TYPE_CLASS_ERROR) {
    mSelection = -1;
    mInvalidMarkup = true;
    mSelectedFrame = nullptr;
    return mSelectedFrame;
  }

  // Selection is not applied to tooltip and statusline.
  // Thereby return the first child.
  if ((mActionType & NS_MATHML_ACTION_TYPE_CLASS_BITMASK) ==
       NS_MATHML_ACTION_TYPE_CLASS_IGNORE_SELECTION) {
    // We don't touch mChildCount here. It's incorrect to assign it 1,
    // and it's inefficient to count the children. It's fine to leave
    // it be equal -1 because it's not used with other actiontypes.
    mSelection = 1;
    mInvalidMarkup = false;
    mSelectedFrame = mFrames.FirstChild();
    return mSelectedFrame;
  }

  mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::selection_, value);
  if (!value.IsEmpty()) {
    nsresult errorCode;
    selection = value.ToInteger(&errorCode);
    if (NS_FAILED(errorCode))
      selection = 1;
  }
  else selection = 1; // default is first frame

  if (-1 != mChildCount) { // we have been in this function before...
    // cater for invalid user-supplied selection
    if (selection > mChildCount || selection < 1)
      selection = -1;
    // quick return if it is identical with our cache
    if (selection == mSelection)
      return mSelectedFrame;
  }

  // get the selected child and cache new values...
  int32_t count = 0;
  nsIFrame* childFrame = mFrames.FirstChild();
  while (childFrame) {
    if (!mSelectedFrame)
      mSelectedFrame = childFrame; // default is first child
    if (++count == selection)
      mSelectedFrame = childFrame;

    childFrame = childFrame->GetNextSibling();
  }
  // cater for invalid user-supplied selection
  if (selection > count || selection < 1)
    selection = -1;

  mChildCount = count;
  mSelection = selection;
  mInvalidMarkup = (mSelection == -1);
  TransmitAutomaticData();

  return mSelectedFrame;
}

void
nsMathMLmactionFrame::SetInitialChildList(ChildListID     aListID,
                                          nsFrameList&    aChildList)
{
  nsMathMLSelectedFrame::SetInitialChildList(aListID, aChildList);

  if (!mSelectedFrame) {
    mActionType = NS_MATHML_ACTION_TYPE_NONE;
  }
  else {
    // create mouse event listener and register it
    mListener = new nsMathMLmactionFrame::MouseListener(this);
    // printf("maction:%p registering as mouse event listener ...\n", this);
    mContent->AddSystemEventListener(NS_LITERAL_STRING("click"), mListener,
                                     false, false);
    mContent->AddSystemEventListener(NS_LITERAL_STRING("mouseover"), mListener,
                                     false, false);
    mContent->AddSystemEventListener(NS_LITERAL_STRING("mouseout"), mListener,
                                     false, false);
  }
}

nsresult
nsMathMLmactionFrame::AttributeChanged(int32_t  aNameSpaceID,
                                       nsAtom* aAttribute,
                                       int32_t  aModType)
{
  bool needsReflow = false;

  InvalidateFrame();

  if (aAttribute == nsGkAtoms::actiontype_) {
    // updating mActionType ...
    int32_t oldActionType = mActionType;
    mActionType = GetActionType(mContent);

    // Initiate a reflow when actiontype classes are different.
    if ((oldActionType & NS_MATHML_ACTION_TYPE_CLASS_BITMASK) !=
          (mActionType & NS_MATHML_ACTION_TYPE_CLASS_BITMASK)) {
      needsReflow = true;
    }
  } else if (aAttribute == nsGkAtoms::selection_) {
    if ((mActionType & NS_MATHML_ACTION_TYPE_CLASS_BITMASK) ==
         NS_MATHML_ACTION_TYPE_CLASS_USE_SELECTION) {
      needsReflow = true;
    }
  } else {
    // let the base class handle other attribute changes
    return
      nsMathMLContainerFrame::AttributeChanged(aNameSpaceID,
                                               aAttribute, aModType);
  }

  if (needsReflow) {
    PresShell()->
      FrameNeedsReflow(this, nsIPresShell::eTreeChange, NS_FRAME_IS_DIRTY);
  }

  return NS_OK;
}

// ################################################################
// Event handlers
// ################################################################

NS_IMPL_ISUPPORTS(nsMathMLmactionFrame::MouseListener,
                  nsIDOMEventListener)


// helper to show a msg on the status bar
// curled from nsPluginFrame.cpp ...
void
ShowStatus(nsPresContext* aPresContext, nsString& aStatusMsg)
{
  nsCOMPtr<nsIDocShellTreeItem> docShellItem(aPresContext->GetDocShell());
  if (docShellItem) {
    nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
    docShellItem->GetTreeOwner(getter_AddRefs(treeOwner));
    if (treeOwner) {
      nsCOMPtr<nsIWebBrowserChrome> browserChrome(do_GetInterface(treeOwner));
      if (browserChrome) {
        browserChrome->SetStatus(nsIWebBrowserChrome::STATUS_LINK, aStatusMsg.get());
      }
    }
  }
}

NS_IMETHODIMP
nsMathMLmactionFrame::MouseListener::HandleEvent(nsIDOMEvent* aEvent)
{
  nsAutoString eventType;
  aEvent->GetType(eventType);
  if (eventType.EqualsLiteral("mouseover")) {
    mOwner->MouseOver();
  }
  else if (eventType.EqualsLiteral("click")) {
    mOwner->MouseClick();
  }
  else if (eventType.EqualsLiteral("mouseout")) {
    mOwner->MouseOut();
  }
  else {
    MOZ_ASSERT_UNREACHABLE("Unexpected eventType");
  }

  return NS_OK;
}

void
nsMathMLmactionFrame::MouseOver()
{
  // see if we should display a status message
  if (NS_MATHML_ACTION_TYPE_STATUSLINE == mActionType) {
    // retrieve content from a second child if it exists
    nsIFrame* childFrame = mFrames.FrameAt(1);
    if (!childFrame) return;

    nsIContent* content = childFrame->GetContent();
    if (!content) return;

    // check whether the content is mtext or not
    if (content->IsMathMLElement(nsGkAtoms::mtext_)) {
      // get the text to be displayed
      content = content->GetFirstChild();
      if (!content) return;

      const nsTextFragment* textFrg = content->GetText();
      if (!textFrg) return;

      nsAutoString text;
      textFrg->AppendTo(text);
      // collapse whitespaces as listed in REC, section 3.2.6.1
      text.CompressWhitespace();
      ShowStatus(PresContext(), text);
    }
  }
}

void
nsMathMLmactionFrame::MouseOut()
{
  // see if we should remove the status message
  if (NS_MATHML_ACTION_TYPE_STATUSLINE == mActionType) {
    nsAutoString value;
    value.SetLength(0);
    ShowStatus(PresContext(), value);
  }
}

void
nsMathMLmactionFrame::MouseClick()
{
  if (NS_MATHML_ACTION_TYPE_TOGGLE == mActionType) {
    if (mChildCount > 1) {
      int32_t selection = (mSelection == mChildCount)? 1 : mSelection + 1;
      nsAutoString value;
      value.AppendInt(selection);
      bool notify = false; // don't yet notify the document
      mContent->SetAttr(kNameSpaceID_None, nsGkAtoms::selection_, value, notify);

      // Now trigger a content-changed reflow...
      PresShell()->
        FrameNeedsReflow(mSelectedFrame, nsIPresShell::eTreeChange,
                         NS_FRAME_IS_DIRTY);
    }
  }
}
