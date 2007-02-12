/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Original Author: David W. Hyatt (hyatt@netscape.com)
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   Dean Tessman <dean_tessman@hotmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsGkAtoms.h"
#include "nsPopupSetFrame.h"
#include "nsIMenuParent.h"
#include "nsMenuFrame.h"
#include "nsBoxFrame.h"
#include "nsIContent.h"
#include "prtypes.h"
#include "nsIAtom.h"
#include "nsPresContext.h"
#include "nsStyleContext.h"
#include "nsCSSRendering.h"
#include "nsINameSpaceManager.h"
#include "nsMenuPopupFrame.h"
#include "nsMenuBarFrame.h"
#include "nsIView.h"
#include "nsIWidget.h"
#include "nsIDocument.h"
#include "nsIDOMNSDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMXULDocument.h"
#include "nsIDOMElement.h"
#include "nsISupportsArray.h"
#include "nsIDOMText.h"
#include "nsBoxLayoutState.h"
#include "nsIScrollableFrame.h"
#include "nsCSSFrameConstructor.h"
#include "nsGUIEvent.h"
#include "nsIRootBox.h"
#include "nsIFocusController.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocShell.h"
#include "nsPIDOMWindow.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIBaseWindow.h"
#include "nsIViewManager.h"

#define NS_MENU_POPUP_LIST_INDEX   0

nsPopupFrameList::nsPopupFrameList(nsIContent* aPopupContent, nsPopupFrameList* aNext)
:mNextPopup(aNext), 
 mPopupFrame(nsnull),
 mPopupContent(aPopupContent),
 mElementContent(nsnull), 
 mCreateHandlerSucceeded(PR_FALSE),
 mIsOpen(PR_FALSE),
 mLastPref(-1,-1)
{
}

nsPopupFrameList* nsPopupFrameList::GetEntry(nsIContent* aPopupContent) {
  if (aPopupContent == mPopupContent)
    return this;

  if (mNextPopup)
    return mNextPopup->GetEntry(aPopupContent);

  return nsnull;
}

nsPopupFrameList* nsPopupFrameList::GetEntryByFrame(nsIFrame* aPopupFrame) {
  if (aPopupFrame == mPopupFrame)
    return this;

  if (mNextPopup)
    return mNextPopup->GetEntryByFrame(aPopupFrame);

  return nsnull;
}

//
// NS_NewPopupSetFrame
//
// Wrapper for creating a new menu popup container
//
nsIFrame*
NS_NewPopupSetFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsPopupSetFrame (aPresShell, aContext);
}

NS_IMETHODIMP_(nsrefcnt) 
nsPopupSetFrame::AddRef(void)
{
  return NS_OK;
}

NS_IMETHODIMP_(nsrefcnt) 
nsPopupSetFrame::Release(void)
{
    return NS_OK;
}

//
// QueryInterface
//
NS_INTERFACE_MAP_BEGIN(nsPopupSetFrame)
  NS_INTERFACE_MAP_ENTRY(nsIPopupSetFrame)
NS_INTERFACE_MAP_END_INHERITING(nsBoxFrame)

NS_IMETHODIMP
nsPopupSetFrame::Init(nsIContent*      aContent,
                      nsIFrame*        aParent,
                      nsIFrame*        aPrevInFlow)
{
  nsresult  rv = nsBoxFrame::Init(aContent, aParent, aPrevInFlow);

  nsIRootBox *rootBox;
  nsresult res = CallQueryInterface(aParent->GetParent(), &rootBox);
  NS_ASSERTION(NS_SUCCEEDED(res), "grandparent should be root box");
  if (NS_SUCCEEDED(res)) {
    rootBox->SetPopupSetFrame(this);
  }

  return rv;
}

NS_IMETHODIMP
nsPopupSetFrame::AppendFrames(nsIAtom*        aListName,
                              nsIFrame*       aFrameList)
{
  if (aListName == nsGkAtoms::popupList) {
    NS_ASSERTION(!aFrameList->GetNextSibling(), "Append one popup at a time!");
    return AddPopupFrame(aFrameList);
  }

  return nsBoxFrame::AppendFrames(aListName, aFrameList);
}

NS_IMETHODIMP
nsPopupSetFrame::RemoveFrame(nsIAtom*        aListName,
                             nsIFrame*       aOldFrame)
{
  if (aListName == nsGkAtoms::popupList) {
    return RemovePopupFrame(aOldFrame);
  }

  return nsBoxFrame::RemoveFrame(aListName, aOldFrame);
}

#ifdef DEBUG
NS_IMETHODIMP
nsPopupSetFrame::InsertFrames(nsIAtom*        aListName,
                              nsIFrame*       aPrevFrame,
                              nsIFrame*       aFrameList)
{
  NS_PRECONDITION(aListName != nsGkAtoms::popupList,
               "Shouldn't be inserting popups");

  return nsBoxFrame::InsertFrames(aListName, aPrevFrame, aFrameList);
}

NS_IMETHODIMP
nsPopupSetFrame::SetInitialChildList(nsIAtom*        aListName,
                                     nsIFrame*       aChildList)
{
  NS_PRECONDITION(aListName != nsGkAtoms::popupList,
                  "Shouldn't be setting initial popup child list");

  return nsBoxFrame::SetInitialChildList(aListName, aChildList);

}
#endif


void
nsPopupSetFrame::Destroy()
{
  // Remove our frame list.
  if (mPopupList) {
    // Try to hide any active popups
    if (nsMenuDismissalListener::sInstance) {
      nsIMenuParent *menuParent =
        nsMenuDismissalListener::sInstance->GetCurrentMenuParent();
      nsIFrame* frame;
      CallQueryInterface(menuParent, &frame);
      // Rollup popups, but only if they're ours
      if (frame && mPopupList->GetEntryByFrame(frame)) {
        nsMenuDismissalListener::sInstance->Rollup();
      }
    }

    // Actually remove each popup from the list as we go. This
    // keeps things consistent so reentering won't crash us
    while (mPopupList) {
      if (mPopupList->mPopupFrame) {
        mPopupList->mPopupFrame->Destroy();
      }

      nsPopupFrameList* temp = mPopupList;
      mPopupList = mPopupList->mNextPopup;
      delete temp;
    }
  }

  nsIRootBox *rootBox;
  nsresult res = CallQueryInterface(mParent->GetParent(), &rootBox);
  NS_ASSERTION(NS_SUCCEEDED(res), "grandparent should be root box");
  if (NS_SUCCEEDED(res)) {
    rootBox->SetPopupSetFrame(nsnull);
  }

  nsBoxFrame::Destroy();
}

NS_IMETHODIMP
nsPopupSetFrame::DoLayout(nsBoxLayoutState& aState)
{
  // lay us out
  nsresult rv = nsBoxFrame::DoLayout(aState);

  // lay out all of our currently open popups.
  nsPopupFrameList* currEntry = mPopupList;
  while (currEntry) {
    nsIFrame* popupChild = currEntry->mPopupFrame;
    if (popupChild) {
      NS_ASSERTION(popupChild->IsBoxFrame(), "popupChild is not box!!");

      // then get its preferred size
      nsSize prefSize = popupChild->GetPrefSize(aState);
      nsSize minSize = popupChild->GetMinSize(aState);
      nsSize maxSize = popupChild->GetMaxSize(aState);

      BoundsCheck(minSize, prefSize, maxSize);

      // if the pref size changed then set bounds to be the pref size
      // and sync the view. Also set new pref size.
     // if (currEntry->mLastPref != prefSize) {
        popupChild->SetBounds(aState, nsRect(0,0,prefSize.width, prefSize.height));
        RepositionPopup(currEntry, aState);
        currEntry->mLastPref = prefSize;
     // }

      // is the new size too small? Make sure we handle scrollbars correctly
      nsIBox* child;
      popupChild->GetChildBox(&child);

      nsRect bounds(popupChild->GetRect());

      nsCOMPtr<nsIScrollableFrame> scrollframe = do_QueryInterface(child);
      if (scrollframe &&
          scrollframe->GetScrollbarStyles().mVertical == NS_STYLE_OVERFLOW_AUTO) {
        // if our pref height
        if (bounds.height < prefSize.height) {
          // layout the child
          popupChild->Layout(aState);

          nsMargin scrollbars = scrollframe->GetActualScrollbarSizes();
          if (bounds.width < prefSize.width + scrollbars.left + scrollbars.right)
          {
            bounds.width += scrollbars.left + scrollbars.right;
            //printf("Width=%d\n",width);
            popupChild->SetBounds(aState, bounds);
          }
        }
      }
    
      // layout the child
      popupChild->Layout(aState);

      // only size popup if open
      if (currEntry->mCreateHandlerSucceeded) {
        nsIView* view = popupChild->GetView();
        nsIViewManager* viewManager = view->GetViewManager();
        nsRect r(0, 0, bounds.width, bounds.height);
        viewManager->ResizeView(view, r);
        viewManager->SetViewVisibility(view, nsViewVisibility_kShow);
      }
    }

    currEntry = currEntry->mNextPopup;
  }

  SyncLayout(aState);

  return rv;
}


#ifdef DEBUG_LAYOUT
NS_IMETHODIMP
nsPopupSetFrame::SetDebug(nsBoxLayoutState& aState, PRBool aDebug)
{
  // see if our state matches the given debug state
  PRBool debugSet = mState & NS_STATE_CURRENTLY_IN_DEBUG;
  PRBool debugChanged = (!aDebug && debugSet) || (aDebug && !debugSet);

  // if it doesn't then tell each child below us the new debug state
  if (debugChanged)
  {
    // XXXdwh fix later.  nobody uses this anymore anyway.
  }

  return NS_OK;
}

nsresult
nsPopupSetFrame::SetDebug(nsBoxLayoutState& aState, nsIFrame* aList, PRBool aDebug)
{
      if (!aList)
          return NS_OK;

      while (aList) {
        if (aList->IsBoxFrame())
          aList->SetDebug(aState, aDebug);

        aList = aList->GetNextSibling();
      }

      return NS_OK;
}
#endif


void
nsPopupSetFrame::RepositionPopup(nsPopupFrameList* aEntry, nsBoxLayoutState& aState)
{
  // Sync up the view.
  if (aEntry && aEntry->mElementContent) {
    nsPresContext* presContext = aState.PresContext();
    nsIFrame* frameToSyncTo = presContext->PresShell()->
      GetPrimaryFrameFor(aEntry->mElementContent);
    ((nsMenuPopupFrame*)(aEntry->mPopupFrame))->SyncViewWithFrame(presContext, 
          aEntry->mPopupAnchor, aEntry->mPopupAlign, frameToSyncTo, aEntry->mXPos, aEntry->mYPos);
  }
}

NS_IMETHODIMP
nsPopupSetFrame::ShowPopup(nsIContent* aElementContent, nsIContent* aPopupContent, 
                           PRInt32 aXPos, PRInt32 aYPos, 
                           const nsString& aPopupType, const nsString& anAnchorAlignment,
                           const nsString& aPopupAlignment)
{
  if (!MayOpenPopup(this))
    return NS_OK;

  nsWeakFrame weakFrame(this);
  // First fire the popupshowing event.
  if (!OnCreate(aXPos, aYPos, aPopupContent) || !weakFrame.IsAlive())
    return NS_OK;
        
  // See if we already have an entry in our list.  We must create a new one on a miss.
  nsPopupFrameList* entry = nsnull;
  if (mPopupList)
    entry = mPopupList->GetEntry(aPopupContent);
  if (!entry) {
    entry = new nsPopupFrameList(aPopupContent, mPopupList);
    if (!entry)
      return NS_ERROR_OUT_OF_MEMORY;
    mPopupList = entry;
  }

  // Cache the element content we're supposed to sync to
  entry->mPopupType = aPopupType;
  entry->mElementContent = aElementContent;
  entry->mPopupAlign = aPopupAlignment;
  entry->mPopupAnchor = anAnchorAlignment;
  entry->mXPos = aXPos;
  entry->mYPos = aYPos;

  // If a frame exists already, go ahead and use it.
  entry->mPopupFrame = GetPresContext()->PresShell()
    ->GetPrimaryFrameFor(aPopupContent);

#ifdef DEBUG_PINK
  printf("X Pos: %d\n", mXPos);
  printf("Y Pos: %d\n", mYPos);
#endif

  // Generate the popup.
  entry->mCreateHandlerSucceeded = PR_TRUE;
  entry->mIsOpen = PR_TRUE;
  // This may destroy or change entry->mPopupFrame or remove the entry from
  // mPopupList. |this| may also get deleted.
  MarkAsGenerated(aPopupContent);

  if (!weakFrame.IsAlive()) {
    return NS_OK;
  }

  nsPopupFrameList* newEntry =
    mPopupList ? mPopupList->GetEntry(aPopupContent) : nsnull;
  if (!newEntry || newEntry != entry) {
    NS_WARNING("The popup entry for aPopupContent has changed!");
    return NS_OK;
  }

  // determine if this menu is a context menu and flag it
  nsIMenuParent* childPopup = nsnull;
  if (entry->mPopupFrame)
    CallQueryInterface(entry->mPopupFrame, &childPopup);
  if ( childPopup && aPopupType.EqualsLiteral("context") )
    childPopup->SetIsContextMenu(PR_TRUE);

  // Now open the popup.
  OpenPopup(entry, PR_TRUE);
  
  if (!weakFrame.IsAlive()) {
    return NS_OK;
  }

  // Now fire the popupshown event.
  OnCreated(aXPos, aYPos, aPopupContent);

  return NS_OK;
}

NS_IMETHODIMP
nsPopupSetFrame::HidePopup(nsIFrame* aPopup)
{
  if (!mPopupList)
    return NS_OK; // No active popups

  nsPopupFrameList* entry = mPopupList->GetEntryByFrame(aPopup);
  if (!entry)
    return NS_OK;

  if (entry->mCreateHandlerSucceeded)
    ActivatePopup(entry, PR_FALSE);

  if (entry->mElementContent && entry->mPopupType.EqualsLiteral("context")) {
    // If we are a context menu, and if we are attached to a
    // menupopup, then hiding us should also hide the parent menu
    // popup.
    if (entry->mElementContent->Tag() == nsGkAtoms::menupopup) {
      nsIFrame* popupFrame = GetPresContext()->PresShell()
        ->GetPrimaryFrameFor(entry->mElementContent);
      if (popupFrame) {
        nsIMenuParent *menuParent;
        if (NS_SUCCEEDED(CallQueryInterface(popupFrame, &menuParent))) {
          menuParent->HideChain();
        }
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsPopupSetFrame::DestroyPopup(nsIFrame* aPopup, PRBool aDestroyEntireChain)
{
  if (!mPopupList)
    return NS_OK; // No active popups

  nsPopupFrameList* entry = mPopupList->GetEntryByFrame(aPopup);

  if (entry && entry->mCreateHandlerSucceeded) {    // ensure the popup was created before we try to destroy it
    nsWeakFrame weakFrame(this);
    OpenPopup(entry, PR_FALSE);
    nsCOMPtr<nsIContent> popupContent = entry->mPopupContent;
    if (weakFrame.IsAlive()) {
      if (aDestroyEntireChain && entry->mElementContent && entry->mPopupType.EqualsLiteral("context")) {
        // If we are a context menu, and if we are attached to a
        // menupopup, then destroying us should also dismiss the parent
        // menu popup.
        if (entry->mElementContent->Tag() == nsGkAtoms::menupopup) {
          nsIFrame* popupFrame = GetPresContext()->PresShell()
            ->GetPrimaryFrameFor(entry->mElementContent);
          if (popupFrame) {
            nsIMenuParent *menuParent;
            if (NS_SUCCEEDED(CallQueryInterface(popupFrame, &menuParent))) {
              menuParent->DismissChain();
            }
          }
        }
      }
  
      // clear things out for next time
      entry->mPopupType.Truncate();
      entry->mCreateHandlerSucceeded = PR_FALSE;
      entry->mElementContent = nsnull;
      entry->mXPos = entry->mYPos = 0;
      entry->mLastPref.width = -1;
      entry->mLastPref.height = -1;
    }
    // ungenerate the popup.
    popupContent->UnsetAttr(kNameSpaceID_None, nsGkAtoms::menugenerated, PR_TRUE);
  }

  return NS_OK;
}

void
nsPopupSetFrame::MarkAsGenerated(nsIContent* aPopupContent)
{
  // Set our attribute, but only if we aren't already generated.
  // Retrieve the menugenerated attribute.
  if (!aPopupContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::menugenerated,
                                  nsGkAtoms::_true, eCaseMatters)) {
    // Generate this element.
    aPopupContent->SetAttr(kNameSpaceID_None, nsGkAtoms::menugenerated, NS_LITERAL_STRING("true"),
                           PR_TRUE);
  }
}

void
nsPopupSetFrame::OpenPopup(nsPopupFrameList* aEntry, PRBool aActivateFlag)
{
  nsWeakFrame weakFrame(this);
  nsIFrame* activeChild = aEntry->mPopupFrame;
  nsWeakFrame weakPopupFrame(activeChild);
  nsCOMPtr<nsIContent> popupContent = aEntry->mPopupContent;
  PRBool createHandlerSucceeded = aEntry->mCreateHandlerSucceeded;
  nsAutoString popupType = aEntry->mPopupType;
  if (aActivateFlag) {
    ActivatePopup(aEntry, PR_TRUE);

    // register the rollup listeners, etc, but not if we're a tooltip
    if (!popupType.EqualsLiteral("tooltip")) {
      nsIMenuParent* childPopup = nsnull;
      if (weakPopupFrame.IsAlive())
        CallQueryInterface(activeChild, &childPopup);

      // Tooltips don't get keyboard navigation
      if (childPopup && !nsMenuDismissalListener::sInstance) {
        // First check and make sure this popup wants keyboard navigation
        if (!popupContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::ignorekeys,
                                       nsGkAtoms::_true, eCaseMatters))
          childPopup->InstallKeyboardNavigator();
      }

      nsMenuDismissalListener* listener = nsMenuDismissalListener::GetInstance();
      if (listener)
        listener->SetCurrentMenuParent(childPopup);
    }
  }
  else {
    if (createHandlerSucceeded && !OnDestroy(aEntry->mPopupContent))
      return;

    // Unregister, but not if we're a tooltip
    if (!popupType.EqualsLiteral("tooltip") ) {
      nsMenuDismissalListener::Shutdown();
    }
    
    // Remove any keyboard navigators
    nsIMenuParent* childPopup = nsnull;
    if (weakPopupFrame.IsAlive())
      CallQueryInterface(activeChild, &childPopup);
    if (childPopup)
      childPopup->RemoveKeyboardNavigator();

    nsRefPtr<nsPresContext> presContext = GetPresContext();
    nsCOMPtr<nsIContent> content = aEntry->mPopupContent;
    ActivatePopup(aEntry, PR_FALSE);

    OnDestroyed(presContext, content);
  }

  if (weakFrame.IsAlive()) {
    AddStateBits(NS_FRAME_HAS_DIRTY_CHILDREN);
    GetPresContext()->PresShell()->FrameNeedsReflow(this, nsIPresShell::eTreeChange);
  }
}

void
nsPopupSetFrame::ActivatePopup(nsPopupFrameList* aEntry, PRBool aActivateFlag)
{
  if (aEntry->mPopupContent) {
    // When we sync the popup view with the frame, we'll show the popup if |menutobedisplayed|
    // is set by setting the |menuactive| attribute. This used to trip css into showing the menu
    // but now we do it ourselves. 
    if (aActivateFlag)
      // XXXben hook in |width| and |height| usage here? 
      aEntry->mPopupContent->SetAttr(kNameSpaceID_None, nsGkAtoms::menutobedisplayed, NS_LITERAL_STRING("true"), PR_TRUE);
    else {
      nsWeakFrame weakFrame(this);
      nsWeakFrame weakActiveChild(aEntry->mPopupFrame);
      nsCOMPtr<nsIContent> content = aEntry->mPopupContent;
      content->UnsetAttr(kNameSpaceID_None, nsGkAtoms::menuactive, PR_TRUE);
      content->UnsetAttr(kNameSpaceID_None, nsGkAtoms::menutobedisplayed, PR_TRUE);

      // get rid of the reflows we just created. If we leave them hanging around, we
      // can get into trouble if a dialog with a modal event loop comes along and
      // processes the reflows before we get to call DestroyChain(). Processing the
      // reflow will cause the popup to show itself again. (bug 71219)
      nsIDocument* doc = content->GetDocument();
      if (doc)
        doc->FlushPendingNotifications(Flush_OnlyReflow);

      // make sure we hide the popup. We can't assume that we'll have a view
      // since we could be cleaning up after someone that didn't correctly 
      // destroy the popup.
      if (weakFrame.IsAlive() && weakActiveChild.IsAlive()) {
        nsIView* view = weakActiveChild.GetFrame()->GetView();
        NS_ASSERTION(view, "View is gone, looks like someone forgot to roll up the popup!");
        if (view) {
          nsIViewManager* viewManager = view->GetViewManager();
          viewManager->SetViewVisibility(view, nsViewVisibility_kHide);
          nsRect r(0, 0, 0, 0);
          viewManager->ResizeView(view, r);
          if (aEntry->mIsOpen) {
            aEntry->mIsOpen = PR_FALSE;
            FireDOMEventSynch(NS_LITERAL_STRING("DOMMenuInactive"), content);
          }
        }
      }
    }
  }
}

PRBool
nsPopupSetFrame::OnCreate(PRInt32 aX, PRInt32 aY, nsIContent* aPopupContent)
{
  nsEventStatus status = nsEventStatus_eIgnore;
  nsMouseEvent event(PR_TRUE, NS_XUL_POPUP_SHOWING, nsnull,
                     nsMouseEvent::eReal);
  // XXX This is messed up: it needs to account for widgets.
  nsPoint dummy;
  event.widget = GetClosestView()->GetNearestWidget(&dummy);
  event.refPoint.x = aX;
  event.refPoint.y = aY;

  if (aPopupContent) {
    nsCOMPtr<nsIContent> kungFuDeathGrip(aPopupContent);
    nsIPresShell *shell = GetPresContext()->GetPresShell();
    if (shell) {
      nsresult rv = shell->HandleDOMEventWithTarget(aPopupContent, &event,
                                                    &status);
      // shell may no longer be alive, don't use it here unless you keep a ref
      if ( NS_FAILED(rv) || status == nsEventStatus_eConsumeNoDefault )
        return PR_FALSE;
    }

    nsCOMPtr<nsIDOMDocument> domDoc(do_QueryInterface(aPopupContent->GetDocument()));
    if (!domDoc) return PR_FALSE;

    // The menu is going to show, and the create handler has executed.
    // We should now walk all of our menu item children, checking to see if any
    // of them has a command attribute.  If so, then several attributes must
    // potentially be updated.

    PRUint32 count = aPopupContent->GetChildCount();
    for (PRUint32 i = 0; i < count; i++) {
      nsCOMPtr<nsIContent> grandChild = aPopupContent->GetChildAt(i);

      if (grandChild->Tag() == nsGkAtoms::menuitem) {
        // See if we have a command attribute.
        nsAutoString command;
        grandChild->GetAttr(kNameSpaceID_None, nsGkAtoms::command, command);
        if (!command.IsEmpty()) {
          // We do! Look it up in our document
          nsCOMPtr<nsIDOMElement> commandElt;
          domDoc->GetElementById(command, getter_AddRefs(commandElt));
          nsCOMPtr<nsIContent> commandContent(do_QueryInterface(commandElt));
          if ( commandContent ) {
            nsAutoString commandValue;
            // The menu's disabled state needs to be updated to match the command.
            if (commandContent->GetAttr(kNameSpaceID_None, nsGkAtoms::disabled, commandValue))
              grandChild->SetAttr(kNameSpaceID_None, nsGkAtoms::disabled, commandValue, PR_TRUE);
            else
              grandChild->UnsetAttr(kNameSpaceID_None, nsGkAtoms::disabled, PR_TRUE);

            // The menu's label, accesskey and checked states need to be updated
            // to match the command. Note that unlike the disabled state if the
            // command has *no* value, we assume the menu is supplying its own.
            if (commandContent->GetAttr(kNameSpaceID_None, nsGkAtoms::label, commandValue))
              grandChild->SetAttr(kNameSpaceID_None, nsGkAtoms::label, commandValue, PR_TRUE);

            if (commandContent->GetAttr(kNameSpaceID_None, nsGkAtoms::accesskey, commandValue))
              grandChild->SetAttr(kNameSpaceID_None, nsGkAtoms::accesskey, commandValue, PR_TRUE);

            if (commandContent->GetAttr(kNameSpaceID_None, nsGkAtoms::checked, commandValue))
              grandChild->SetAttr(kNameSpaceID_None, nsGkAtoms::checked, commandValue, PR_TRUE);
          }
        }
      }
    }
  }

  return PR_TRUE;
}

PRBool
nsPopupSetFrame::OnCreated(PRInt32 aX, PRInt32 aY, nsIContent* aPopupContent)
{
  nsEventStatus status = nsEventStatus_eIgnore;
  nsMouseEvent event(PR_TRUE, NS_XUL_POPUP_SHOWN, nsnull,
                     nsMouseEvent::eReal);
  // XXX See OnCreate above
  //event.point.x = aX;
  //event.point.y = aY;

  if (aPopupContent) {
    nsIPresShell *shell = GetPresContext()->GetPresShell();
    if (shell) {
      nsresult rv = shell->HandleDOMEventWithTarget(aPopupContent, &event,
                                                    &status);
      // shell may no longer be alive, don't use it here unless you keep a ref
      if ( NS_FAILED(rv) || status == nsEventStatus_eConsumeNoDefault )
        return PR_FALSE;
    }
  }

  return PR_TRUE;
}

PRBool
nsPopupSetFrame::OnDestroy(nsIContent* aPopupContent)
{
  nsEventStatus status = nsEventStatus_eIgnore;
  nsMouseEvent event(PR_TRUE, NS_XUL_POPUP_HIDING, nsnull,
                     nsMouseEvent::eReal);

  if (aPopupContent) {
    nsIPresShell *shell = GetPresContext()->GetPresShell();
    if (shell) {
      nsresult rv = shell->HandleDOMEventWithTarget(aPopupContent, &event,
                                                    &status);
      // shell may no longer be alive, don't use it here unless you keep a ref
      if ( NS_FAILED(rv) || status == nsEventStatus_eConsumeNoDefault )
        return PR_FALSE;
    }
  }
  return PR_TRUE;
}

PRBool
nsPopupSetFrame::OnDestroyed(nsPresContext* aPresContext,
                             nsIContent* aPopupContent)
{
  nsEventStatus status = nsEventStatus_eIgnore;
  nsMouseEvent event(PR_TRUE, NS_XUL_POPUP_HIDDEN, nsnull,
                     nsMouseEvent::eReal);

  if (aPopupContent && aPresContext) {
    nsIPresShell *shell = aPresContext->GetPresShell();
    if (shell) {
      nsresult rv = shell->HandleDOMEventWithTarget(aPopupContent, &event,
                                                    &status);
      // shell may no longer be alive, don't use it here unless you keep a ref
      if ( NS_FAILED(rv) || status == nsEventStatus_eConsumeNoDefault )
        return PR_FALSE;
    }
  }
  return PR_TRUE;
}

nsresult
nsPopupSetFrame::RemovePopupFrame(nsIFrame* aPopup)
{
  // This was called by the Destroy() method of the popup, so all we have to do is
  // get the popup out of our list, so we don't reflow it later.
  nsPopupFrameList* currEntry = mPopupList;
  nsPopupFrameList* temp = nsnull;
  while (currEntry) {
    if (currEntry->mPopupFrame == aPopup) {
      // Remove this entry.
      if (temp)
        temp->mNextPopup = currEntry->mNextPopup;
      else
        mPopupList = currEntry->mNextPopup;
      
      // Destroy the frame.
      currEntry->mPopupFrame->Destroy();

      // Delete the entry.
      currEntry->mNextPopup = nsnull;
      delete currEntry;

      // Break out of the loop.
      break;
    }

    temp = currEntry;
    currEntry = currEntry->mNextPopup;
  }

  return NS_OK;
}

nsresult
nsPopupSetFrame::AddPopupFrame(nsIFrame* aPopup)
{
  // The entry should already exist, but might not (if someone decided to make their
  // popup visible straightaway, e.g., the autocomplete widget).

  // First look for an entry by content.
  nsIContent* content = aPopup->GetContent();
  nsPopupFrameList* entry = nsnull;
  if (mPopupList)
    entry = mPopupList->GetEntry(content);
  if (!entry) {
    entry = new nsPopupFrameList(content, mPopupList);
    if (!entry)
      return NS_ERROR_OUT_OF_MEMORY;
    mPopupList = entry;
  }
  
  // Set the frame connection.
  entry->mPopupFrame = aPopup;
  
  // Now return.  The remaining entry values will be filled in if/when showPopup is
  // called for this popup.
  return NS_OK;
}

//static
PRBool
nsPopupSetFrame::MayOpenPopup(nsIFrame* aFrame)
{
  nsCOMPtr<nsISupports> cont = aFrame->GetPresContext()->GetContainer();
  nsCOMPtr<nsIDocShellTreeItem> dsti = do_QueryInterface(cont);
  if (!dsti)
    return PR_FALSE;

  // chrome shells can always open popups
  PRInt32 type = -1;
  if (NS_SUCCEEDED(dsti->GetItemType(&type)) && type == nsIDocShellTreeItem::typeChrome)
    return PR_TRUE;

  nsCOMPtr<nsIDocShell> shell = do_QueryInterface(dsti);
  if (!shell)
    return PR_FALSE;

  nsCOMPtr<nsPIDOMWindow> win = do_GetInterface(shell);
  if (!win)
    return PR_FALSE;

  // only allow popups in active windows
  PRBool active;
  nsIFocusController* focusController = win->GetRootFocusController();
  focusController->GetActive(&active);
  if (!active)
    return PR_FALSE;

  nsCOMPtr<nsIBaseWindow> baseWin = do_QueryInterface(shell);
  if (!baseWin)
    return PR_FALSE;

  // only allow popups in visible frames
  PRBool visible;
  baseWin->GetVisibility(&visible);
  return visible;
}

