/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Original Author: David W. Hyatt (hyatt@netscape.com)
 *
 * Contributor(s): 
 *   Michael Lowe <michael.lowe@bigfoot.com>
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   Dean Tessman <dean_tessman@hotmail.com>
 */

#include "nsXULAtoms.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLParts.h"
#include "nsMenuFrame.h"
#include "nsBoxFrame.h"
#include "nsIContent.h"
#include "prtypes.h"
#include "nsIAtom.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsIStyleContext.h"
#include "nsIReflowCommand.h"
#include "nsCSSRendering.h"
#include "nsINameSpaceManager.h"
#include "nsLayoutAtoms.h"
#include "nsMenuPopupFrame.h"
#include "nsMenuBarFrame.h"
#include "nsIView.h"
#include "nsIWidget.h"
#include "nsIDocument.h"
#include "nsIDOMNSDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMXULDocument.h"
#include "nsIDOMXULMenuListElement.h"
#include "nsIDOMElement.h"
#include "nsISupportsArray.h"
#include "nsIDOMText.h"
#include "nsILookAndFeel.h"
#include "nsIComponentManager.h"
#include "nsWidgetsCID.h"
#include "nsBoxLayoutState.h"
#include "nsIXBLBinding.h"
#include "nsIScrollableFrame.h"
#include "nsIViewManager.h"
#include "nsIBindingManager.h"
#include "nsIServiceManager.h"
#include "nsIXBLService.h"
#include "nsCSSFrameConstructor.h"

#define NS_MENU_POPUP_LIST_INDEX   0

static PRInt32 gEatMouseMove = PR_FALSE;

nsMenuDismissalListener* nsMenuFrame::mDismissalListener = nsnull;

static NS_DEFINE_IID(kLookAndFeelCID, NS_LOOKANDFEEL_CID);
static NS_DEFINE_IID(kILookAndFeelIID, NS_ILOOKANDFEEL_IID);


//
// NS_NewMenuFrame
//
// Wrapper for creating a new menu popup container
//
nsresult
NS_NewMenuFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame, PRUint32 aFlags)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsMenuFrame* it = new (aPresShell) nsMenuFrame (aPresShell);
  if ( !it )
    return NS_ERROR_OUT_OF_MEMORY;
  *aNewFrame = it;
  if (aFlags)
    it->SetIsMenu(PR_TRUE);
  return NS_OK;
}

NS_IMETHODIMP_(nsrefcnt) 
nsMenuFrame::AddRef(void)
{
  return NS_OK;
}

NS_IMETHODIMP_(nsrefcnt)
nsMenuFrame::Release(void)
{
    return NS_OK;
}

//
// QueryInterface
//
NS_INTERFACE_MAP_BEGIN(nsMenuFrame)
  NS_INTERFACE_MAP_ENTRY(nsITimerCallback)
  NS_INTERFACE_MAP_ENTRY(nsIMenuFrame)
NS_INTERFACE_MAP_END_INHERITING(nsBoxFrame)

//
// nsMenuFrame cntr
//
nsMenuFrame::nsMenuFrame(nsIPresShell* aShell):nsBoxFrame(aShell),
    mIsMenu(PR_FALSE),
    mMenuOpen(PR_FALSE),
    mCreateHandlerSucceeded(PR_FALSE),
    mChecked(PR_FALSE),
    mType(eMenuType_Normal),
    mMenuParent(nsnull),
    mPresContext(nsnull),
    mLastPref(-1,-1),
    mFrameConstructor(nsnull)
{

} // cntr

NS_IMETHODIMP
nsMenuFrame::SetParent(const nsIFrame* aParent)
{
  nsresult rv = nsBoxFrame::SetParent(aParent);
  nsIFrame* currFrame = (nsIFrame*)aParent;
  while (!mMenuParent && currFrame) {
    // Set our menu parent.
    nsCOMPtr<nsIMenuParent> menuparent(do_QueryInterface(currFrame));
    mMenuParent = menuparent.get();

    currFrame->GetParent(&currFrame);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsMenuFrame::Init(nsIPresContext*  aPresContext,
                     nsIContent*      aContent,
                     nsIFrame*        aParent,
                     nsIStyleContext* aContext,
                     nsIFrame*        aPrevInFlow)
{
  mPresContext = aPresContext; // Don't addref it.  Our lifetime is shorter.

  nsresult  rv = nsBoxFrame::Init(aPresContext, aContent, aParent, aContext, aPrevInFlow);

  nsIFrame* currFrame = aParent;
  while (!mMenuParent && currFrame) {
    // Set our menu parent.
    nsCOMPtr<nsIMenuParent> menuparent(do_QueryInterface(currFrame));
    mMenuParent = menuparent.get();

    currFrame->GetParent(&currFrame);
  }

  // Do the type="checkbox" magic
  UpdateMenuType(aPresContext);

  nsAutoString accelString;
  BuildAcceleratorText(accelString);
  
  return rv;
}

// The following methods are all overridden to ensure that the menupopup frame
// is placed in the appropriate list.
NS_IMETHODIMP
nsMenuFrame::FirstChild(nsIPresContext* aPresContext,
                        nsIAtom*        aListName,
                        nsIFrame**      aFirstChild) const
{
  if (nsLayoutAtoms::popupList == aListName) {
    *aFirstChild = mPopupFrames.FirstChild();
  } else {
    nsBoxFrame::FirstChild(aPresContext, aListName, aFirstChild);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsMenuFrame::SetInitialChildList(nsIPresContext* aPresContext,
                                               nsIAtom*        aListName,
                                               nsIFrame*       aChildList)
{
  nsresult rv = NS_OK;
  if (nsLayoutAtoms::popupList == aListName) {
    mPopupFrames.SetFrames(aChildList);
  } else {

    nsFrameList frames(aChildList);

    // We may have a menupopup in here. Get it out, and move it into
    // the popup frame list.
    nsIFrame* frame = frames.FirstChild();
    while (frame) {
      nsCOMPtr<nsIMenuParent> menuPar(do_QueryInterface(frame));
      if (menuPar) {
        // Remove this frame from the list and place it in the other list.
        frames.RemoveFrame(frame);
        mPopupFrames.AppendFrame(this, frame);
        nsIFrame* first = frames.FirstChild();
        rv = nsBoxFrame::SetInitialChildList(aPresContext, aListName, first);
        return rv;
      }
      frame->GetNextSibling(&frame);
    }

    // Didn't find it.
    rv = nsBoxFrame::SetInitialChildList(aPresContext, aListName, aChildList);
  }
  return rv;
}

NS_IMETHODIMP
nsMenuFrame::GetAdditionalChildListName(PRInt32   aIndex,
                                        nsIAtom** aListName) const
{
  NS_PRECONDITION(nsnull != aListName, "null OUT parameter pointer");

  *aListName = nsnull;

  // don't expose the child frame list, it slows things down
#if 0
  if (NS_MENU_POPUP_LIST_INDEX == aIndex) {
    *aListName = nsLayoutAtoms::popupList;
    NS_ADDREF(*aListName);
  }
#endif

  return NS_OK;
}

NS_IMETHODIMP
nsMenuFrame::Destroy(nsIPresContext* aPresContext)
{
  // Remove our frame mappings
  if (mFrameConstructor) {
    nsIFrame* curFrame = mPopupFrames.FirstChild();
    while (curFrame) {
      mFrameConstructor->RemoveMappingsForFrameSubtree(aPresContext, curFrame, nsnull);
      curFrame->GetNextSibling(&curFrame);
    }
  }

   // Cleanup frames in popup child list
  mPopupFrames.DestroyFrames(aPresContext);

  return nsBoxFrame::Destroy(aPresContext);
}

// Called to prevent events from going to anything inside the menu.
NS_IMETHODIMP
nsMenuFrame::GetFrameForPoint(nsIPresContext* aPresContext,
                              const nsPoint& aPoint, 
                              nsFramePaintLayer aWhichLayer,    
                              nsIFrame**     aFrame)
{

  if ((aWhichLayer != NS_FRAME_PAINT_LAYER_FOREGROUND))
    return NS_ERROR_FAILURE;

 // if it is not inside us or not in the layer in which we paint, fail
  if (!mRect.Contains(aPoint)) 
      return NS_ERROR_FAILURE;
  
  nsresult result = nsBoxFrame::GetFrameForPoint(aPresContext, aPoint, aWhichLayer, aFrame);
  if ((result != NS_OK) || (*aFrame == this)) {
    return result;
  }
  nsCOMPtr<nsIContent> content;
  (*aFrame)->GetContent(getter_AddRefs(content));
  if (content) {
    // This allows selective overriding for subcontent.
    nsAutoString value;
    content->GetAttribute(kNameSpaceID_None, nsXULAtoms::allowevents, value);
    if (value.EqualsWithConversion("true"))
      return result;
  }
  const nsStyleDisplay* disp = (const nsStyleDisplay*)
    mStyleContext->GetStyleData(eStyleStruct_Display);
  if (disp->IsVisible()) {
    *aFrame = this; // Capture all events so that we can perform selection
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP 
nsMenuFrame::HandleEvent(nsIPresContext* aPresContext, 
                             nsGUIEvent*     aEvent,
                             nsEventStatus*  aEventStatus)
{
  NS_ENSURE_ARG_POINTER(aEventStatus);
  *aEventStatus = nsEventStatus_eConsumeDoDefault;
  
  if (aEvent->message == NS_KEY_PRESS && !IsDisabled()) {
    nsKeyEvent* keyEvent = (nsKeyEvent*)aEvent;
    PRUint32 keyCode = keyEvent->keyCode;
    if (keyCode == NS_VK_UP || keyCode == NS_VK_DOWN) {
      if (!IsOpen()) 
        OpenMenu(PR_TRUE);
    }
  }
  else if (aEvent->message == NS_MOUSE_LEFT_BUTTON_DOWN && !IsDisabled() && IsMenu() ) {
    PRBool isMenuBar = PR_FALSE;
    if (mMenuParent)
      mMenuParent->IsMenuBar(isMenuBar);

    // The menu item was selected. Bring up the menu.
    // We have children.
    if ( isMenuBar || !mMenuParent ) {
      ToggleMenuState();

      if (!IsOpen() && mMenuParent) {
        // We closed up. The menu bar should always be
        // deactivated when this happens.
        mMenuParent->SetActive(PR_FALSE);
      }
    }
    else
      if ( !IsOpen() ) {
        // one of our siblings is probably open and even possibly waiting
        // for its close timer to fire. Tell our parent to close it down. Not
        // doing this before its timer fires will cause the rollup state to
        // get very confused.
        if ( mMenuParent )
          mMenuParent->KillPendingTimers();

        // safe to open up
        OpenMenu(PR_TRUE);
      }
  }
  else if ( aEvent->message == NS_MOUSE_RIGHT_BUTTON_UP && mMenuParent && !IsDisabled()) {
    // if this menu is a context menu it accepts right-clicks...fire away!
    PRBool isContextMenu = PR_FALSE;
    mMenuParent->GetIsContextMenu(isContextMenu);
    if ( isContextMenu )
      Execute();
  }
  else if (aEvent->message == NS_MOUSE_LEFT_BUTTON_UP && !IsMenu() && mMenuParent && !IsDisabled()) {
    // First, flip "checked" state if we're a checkbox menu, or
    // an un-checked radio menu
    if (mType == eMenuType_Checkbox ||
        (mType == eMenuType_Radio && !mChecked)) {
      if (mChecked) {
        mContent->UnsetAttribute(kNameSpaceID_None, nsHTMLAtoms::checked,
                             PR_TRUE);
      }
      else {
        mContent->SetAttribute(kNameSpaceID_None, nsHTMLAtoms::checked, NS_ConvertASCIItoUCS2("true"),
                             PR_TRUE);
      }
        
      /* the AttributeChanged code will update all the internal state */
    }

    // Execute the execute event handler.
    Execute();
  }
  else if (aEvent->message == NS_MOUSE_EXIT_SYNTH) {
    // Kill our timer if one is active.
    if (mOpenTimer) {
      mOpenTimer->Cancel();
      mOpenTimer = nsnull;
    }

    // Deactivate the menu.
    PRBool isActive = PR_FALSE;
    PRBool isMenuBar = PR_FALSE;
    if (mMenuParent) {
      mMenuParent->IsMenuBar(isMenuBar);
      PRBool cancel = PR_TRUE;
      if (isMenuBar) {
        mMenuParent->GetIsActive(isActive);
        if (isActive) cancel = PR_FALSE;
      }
      
      if (cancel) {
        if (IsMenu() && !isMenuBar && mMenuOpen) {
          // Submenus don't get closed up immediately.
        }
        else mMenuParent->SetCurrentMenuItem(nsnull);
      }
    }
  }
  else if (aEvent->message == NS_MOUSE_MOVE && mMenuParent) {
    if (gEatMouseMove) {
      gEatMouseMove = PR_FALSE;
      return NS_OK;
    }

    // we checked for mMenuParent right above

    PRBool isMenuBar = PR_FALSE;
    mMenuParent->IsMenuBar(isMenuBar);

    // Let the menu parent know we're the new item.
    mMenuParent->SetCurrentMenuItem(this);
    
    // If we're a menu (and not a menu item),
    // kick off the timer.
    if (!IsDisabled() && !isMenuBar && IsMenu() && !mMenuOpen && !mOpenTimer) {

      PRInt32 menuDelay = 300;   // ms

      nsILookAndFeel * lookAndFeel;
      if (NS_OK == nsComponentManager::CreateInstance(kLookAndFeelCID, nsnull, 
                      kILookAndFeelIID, (void**)&lookAndFeel)) {
        lookAndFeel->GetMetric(nsILookAndFeel::eMetric_SubmenuDelay, menuDelay);
       NS_RELEASE(lookAndFeel);
      }

      // We're a menu, we're built, we're closed, and no timer has been kicked off.
      mOpenTimer = do_CreateInstance("component://netscape/timer");
      mOpenTimer->Init(this, menuDelay, NS_PRIORITY_HIGHEST);
    }
  }
  
  return NS_OK;
}

NS_IMETHODIMP
nsMenuFrame::ToggleMenuState()
{  
  if (mMenuOpen) {
    OpenMenu(PR_FALSE);
  }
  else {
    OpenMenu(PR_TRUE);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsMenuFrame::SelectMenu(PRBool aActivateFlag)
{
  if (aActivateFlag) {
    // Highlight the menu.
    mContent->SetAttribute(kNameSpaceID_None, nsXULAtoms::menuactive, NS_ConvertASCIItoUCS2("true"), PR_TRUE);
  }
  else {
    // Unhighlight the menu.
    mContent->UnsetAttribute(kNameSpaceID_None, nsXULAtoms::menuactive, PR_TRUE);
  }

  return NS_OK;
}

PRBool nsMenuFrame::IsGenerated()
{
  nsCOMPtr<nsIContent> child;
  GetMenuChildrenElement(getter_AddRefs(child));
  
  // Generate the menu if it hasn't been generated already.  This
  // takes it from display: none to display: block and gives us
  // a menu forevermore.
  if (child) {
    nsString genVal;
    child->GetAttribute(kNameSpaceID_None, nsXULAtoms::menugenerated, genVal);
    if (genVal.IsEmpty())
      return PR_FALSE;
  }

  return PR_TRUE;
}

NS_IMETHODIMP
nsMenuFrame::MarkAsGenerated()
{
  nsCOMPtr<nsIContent> child;
  GetMenuChildrenElement(getter_AddRefs(child));
  
  // Generate the menu if it hasn't been generated already.  This
  // takes it from display: none to display: block and gives us
  // a menu forevermore.
  if (child) {
    nsAutoString genVal;
    child->GetAttribute(kNameSpaceID_None, nsXULAtoms::menugenerated, genVal);
    if (genVal.IsEmpty())
      child->SetAttribute(kNameSpaceID_None, nsXULAtoms::menugenerated, NS_ConvertASCIItoUCS2("true"), PR_TRUE);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsMenuFrame::ActivateMenu(PRBool aActivateFlag)
{
  nsIFrame* frame = mPopupFrames.FirstChild();
  nsMenuPopupFrame* menuPopup = (nsMenuPopupFrame*)frame;
  
  if (!menuPopup) 
    return NS_OK;

  if (aActivateFlag) {
      nsRect rect;
      menuPopup->GetRect(rect);
      nsIView* view = nsnull;
      menuPopup->GetView(mPresContext, &view);
      nsCOMPtr<nsIViewManager> viewManager;
      view->GetViewManager(*getter_AddRefs(viewManager));
      viewManager->ResizeView(view, rect.width, rect.height);

      // make sure the scrolled window is at 0,0
      if (mLastPref.height <= rect.height) {
        nsIBox* child;
        menuPopup->GetChildBox(&child);

        nsCOMPtr<nsIScrollableFrame> scrollframe = do_QueryInterface(child);
        if (scrollframe) {
          scrollframe->ScrollTo(mPresContext, 0, 0);
        }
      }

      viewManager->UpdateView(view, nsRect(0,0, rect.width, rect.height), NS_VMREFRESH_IMMEDIATE);
      viewManager->SetViewVisibility(view, nsViewVisibility_kShow);

  } else {
    nsIView* view = nsnull;
    menuPopup->GetView(mPresContext, &view);
    nsCOMPtr<nsIViewManager> viewManager;
    view->GetViewManager(*getter_AddRefs(viewManager));
    viewManager->SetViewVisibility(view, nsViewVisibility_kHide);
    viewManager->ResizeView(view, 0, 0);
    // set here so hide chain can close the menu as well.
    mMenuOpen = PR_FALSE;
  }
  
  return NS_OK;
}  

NS_IMETHODIMP
nsMenuFrame::AttributeChanged(nsIPresContext* aPresContext,
                              nsIContent* aChild,
                              PRInt32 aNameSpaceID,
                              nsIAtom* aAttribute,
                              PRInt32 aHint)
{
  nsAutoString value;

  if (aAttribute == nsXULAtoms::open) {
    aChild->GetAttribute(kNameSpaceID_None, aAttribute, value);
    if (value.EqualsWithConversion("true"))
      OpenMenuInternal(PR_TRUE);
    else {
      OpenMenuInternal(PR_FALSE);
      mCreateHandlerSucceeded = PR_FALSE;
    }
  } else if (aAttribute == nsHTMLAtoms::checked) {
    if (mType != eMenuType_Normal)
        UpdateMenuSpecialState(aPresContext);
  } else if ( aAttribute == nsHTMLAtoms::type || aAttribute == nsHTMLAtoms::name )
    UpdateMenuType(aPresContext);

  return NS_OK;
}

NS_IMETHODIMP
nsMenuFrame::OpenMenu(PRBool aActivateFlag)
{
  nsCOMPtr<nsIDOMElement> domElement = do_QueryInterface(mContent);
  if (aActivateFlag) {
    // Now that the menu is opened, we should have a menupopup child built.
    // Mark it as generated, which ensures a frame gets built.
    MarkAsGenerated();

    domElement->SetAttribute(NS_ConvertASCIItoUCS2("open"), NS_ConvertASCIItoUCS2("true"));
  }
  else domElement->RemoveAttribute(NS_ConvertASCIItoUCS2("open"));

  return NS_OK;
}

void 
nsMenuFrame::OpenMenuInternal(PRBool aActivateFlag) 
{
  gEatMouseMove = PR_TRUE;

  if (!mIsMenu)
    return;

  if (aActivateFlag) {
    // Execute the oncreate handler
    if (!OnCreate())
      return;

    mCreateHandlerSucceeded = PR_TRUE;
  
    // Set the focus back to our view's widget.
    if (nsMenuFrame::mDismissalListener)
      nsMenuFrame::mDismissalListener->EnableListener(PR_FALSE);
    
    // XXX Only have this here because of RDF-generated content.
    MarkAsGenerated();

    nsIFrame* frame = mPopupFrames.FirstChild();
    nsMenuPopupFrame* menuPopup = (nsMenuPopupFrame*)frame;
    
    mMenuOpen = PR_TRUE;

    if (menuPopup) {
      // inherit whether or not we're a context menu from the parent
      if ( mMenuParent ) {
        PRBool parentIsContextMenu = PR_FALSE;
        mMenuParent->GetIsContextMenu(parentIsContextMenu);
        menuPopup->SetIsContextMenu(parentIsContextMenu);
      }

      // Install a keyboard navigation listener if we're the root of the menu chain.
      PRBool onMenuBar = PR_TRUE;
      if (mMenuParent)
        mMenuParent->IsMenuBar(onMenuBar);

      if (mMenuParent && onMenuBar)
        mMenuParent->InstallKeyboardNavigator();
      else if (!mMenuParent)
        menuPopup->InstallKeyboardNavigator();
      
      // Tell the menu bar we're active.
      if (mMenuParent)
        mMenuParent->SetActive(PR_TRUE);

      nsCOMPtr<nsIContent> menuPopupContent;
      menuPopup->GetContent(getter_AddRefs(menuPopupContent));

      // See if we're a menulist and set our selection accordingly.
      nsCOMPtr<nsIDOMXULMenuListElement> list = do_QueryInterface(mContent);
      if (list) {
        nsCOMPtr<nsIDOMElement> element;
        list->GetSelectedItem(getter_AddRefs(element));
        if (element) {
          nsCOMPtr<nsIContent> selectedContent = do_QueryInterface(element);
          nsIFrame* curr;
          menuPopup->FirstChild(mPresContext, nsnull, &curr);
          while (curr) {
            nsCOMPtr<nsIContent> child;
            curr->GetContent(getter_AddRefs(child));
            if (selectedContent == child) {
              nsCOMPtr<nsIMenuFrame> menuframe(do_QueryInterface(curr));
              if (menuframe)
                menuPopup->SetCurrentMenuItem(menuframe);
            }
            curr->GetNextSibling(&curr);
          }
        }
      }
      // End of menulist stuff.  We now return to our regularly scheduled
      // programming.

      // Sync up the view.
      nsAutoString popupAnchor, popupAlign;
      
      menuPopupContent->GetAttribute(kNameSpaceID_None, nsXULAtoms::popupanchor, popupAnchor);
      menuPopupContent->GetAttribute(kNameSpaceID_None, nsXULAtoms::popupalign, popupAlign);

      
      if (onMenuBar) {
        if (popupAnchor.IsEmpty())
          popupAnchor.AssignWithConversion("bottomleft");
        if (popupAlign.IsEmpty())
          popupAlign.AssignWithConversion("topleft");
      }
      else {
        if (popupAnchor.IsEmpty())
          popupAnchor.AssignWithConversion("topright");
        if (popupAlign.IsEmpty())
          popupAlign.AssignWithConversion("topleft");
      }

      // if height never set we need to do an initial reflow.
      if (mLastPref.height == -1)
      {
         nsBoxLayoutState state(mPresContext);
         menuPopup->MarkDirty(state);

         nsCOMPtr<nsIPresShell> shell;
         mPresContext->GetShell(getter_AddRefs(shell));
         shell->FlushPendingNotifications();
      }

      nsRect curRect;
      menuPopup->GetBounds(curRect);

      nsBoxLayoutState state(mPresContext);
      menuPopup->SetBounds(state, nsRect(0,0,mLastPref.width, mLastPref.height));

      nsIView* view = nsnull;
      menuPopup->GetView(mPresContext, &view);
      view->SetVisibility(nsViewVisibility_kHide);
      menuPopup->SyncViewWithFrame(mPresContext, popupAnchor, popupAlign, this, -1, -1);
      nsRect rect;
      menuPopup->GetBounds(rect);

      // if the height is different then reflow. It might need scrollbars force a reflow
      if (curRect.height != rect.height || mLastPref.height != rect.height)
      {
         nsBoxLayoutState state(mPresContext);
         menuPopup->MarkDirty(state);
         nsCOMPtr<nsIPresShell> shell;
         mPresContext->GetShell(getter_AddRefs(shell));
         shell->FlushPendingNotifications();
         menuPopup->GetRect(rect);
      }

      ActivateMenu(PR_TRUE);

      nsCOMPtr<nsIMenuParent> childPopup = do_QueryInterface(frame);
      UpdateDismissalListener(childPopup);

    }

    // Set the focus back to our view's widget.
    if (nsMenuFrame::mDismissalListener)
      nsMenuFrame::mDismissalListener->EnableListener(PR_TRUE);
    
  }
  else {

    // Close the menu. 
    // Execute the ondestroy handler, but only if we're actually open
    if ( !mCreateHandlerSucceeded || !OnDestroy() )
      return;

    mMenuOpen = PR_FALSE;

    // Set the focus back to our view's widget.
    if (nsMenuFrame::mDismissalListener) {
      nsMenuFrame::mDismissalListener->EnableListener(PR_FALSE);
      nsMenuFrame::mDismissalListener->SetCurrentMenuParent(mMenuParent);
    }

    nsIFrame* frame = mPopupFrames.FirstChild();
    nsMenuPopupFrame* menuPopup = (nsMenuPopupFrame*)frame;
  
    // Make sure we clear out our own items.
    if (menuPopup) {
      menuPopup->SetCurrentMenuItem(nsnull);
      menuPopup->KillCloseTimer();

      PRBool onMenuBar = PR_TRUE;
      if (mMenuParent)
        mMenuParent->IsMenuBar(onMenuBar);

      if (mMenuParent && onMenuBar)
        mMenuParent->RemoveKeyboardNavigator();
      else if (!mMenuParent)
        menuPopup->RemoveKeyboardNavigator();
    }

    // activate false will also set the mMenuOpen to false.
    ActivateMenu(PR_FALSE);

    if (nsMenuFrame::mDismissalListener)
      nsMenuFrame::mDismissalListener->EnableListener(PR_TRUE);

  }

}

void
nsMenuFrame::GetMenuChildrenElement(nsIContent** aResult)
{
  nsresult rv;
  NS_WITH_SERVICE(nsIXBLService, xblService, "component://netscape/xbl", &rv);
  PRInt32 dummy;
  PRInt32 count;
  mContent->ChildCount(count);

  for (PRInt32 i = 0; i < count; i++) {
    nsCOMPtr<nsIContent> child;
    mContent->ChildAt(i, *getter_AddRefs(child));
    nsCOMPtr<nsIAtom> tag;
    xblService->ResolveTag(child, &dummy, getter_AddRefs(tag));
    if (tag && tag.get() == nsXULAtoms::menupopup) {
      *aResult = child.get();
      NS_ADDREF(*aResult);
      return;
    }
  }
}

NS_IMETHODIMP
nsMenuFrame::DoLayout(nsBoxLayoutState& aState)
{
  nsRect contentRect;
  GetContentRect(contentRect);

  // lay us out
  nsresult rv = nsBoxFrame::DoLayout(aState);

  // layout the popup. First we need to get it.
  nsIFrame* popupChild = mPopupFrames.FirstChild();

  if (popupChild) {

    nsCOMPtr<nsIDOMXULMenuListElement> menulist = do_QueryInterface(mContent);

    nsIBox* ibox = nsnull;
    nsresult rv2 = popupChild->QueryInterface(NS_GET_IID(nsIBox), (void**)&ibox);
    NS_ASSERTION(NS_SUCCEEDED(rv2) && ibox,"popupChild is not box!!");

    // then get its preferred size
    nsSize prefSize(0,0);
    nsSize minSize(0,0);
    nsSize maxSize(0,0);

    ibox->GetPrefSize(aState, prefSize);
    ibox->GetMinSize(aState, minSize);
    ibox->GetMaxSize(aState, maxSize);

    BoundsCheck(minSize, prefSize, maxSize);

    if (menulist && prefSize.width < contentRect.width)
        prefSize.width = contentRect.width;

    // if the pref size changed then set bounds to be the pref size
    // and sync the view. And set new pref size.
    if (mLastPref != prefSize) {
      ibox->SetBounds(aState, nsRect(0,0,prefSize.width, prefSize.height));
      RePositionPopup(aState);
      mLastPref = prefSize;
    }

    // is the new size too small? Make sure we handle scrollbars correctly
    nsIBox* child;
    ibox->GetChildBox(&child);

    nsRect bounds(0,0,0,0);
    ibox->GetBounds(bounds);

    nsCOMPtr<nsIScrollableFrame> scrollframe = do_QueryInterface(child);
    if (scrollframe) {
      nsIScrollableFrame::nsScrollPref pref;
      scrollframe->GetScrollPreference(aState.GetPresContext(), &pref);

      if (pref == nsIScrollableFrame::Auto)  
      {
        // if our pref height
        if (bounds.height < prefSize.height) {
           // layout the child
           ibox->Layout(aState);

           nscoord width;
           nscoord height;
           scrollframe->GetScrollbarSizes(aState.GetPresContext(), &width, &height);
           if (bounds.width < prefSize.width + width)
           {
             bounds.width += width;
             //printf("Width=%d\n",width);
             ibox->SetBounds(aState, bounds);
           }
        }
      }
    }
    
    // layout the child
    ibox->Layout(aState);

    // Only size the popups view if open.
    if (mMenuOpen) {
      nsIView* view = nsnull;
      popupChild->GetView(aState.GetPresContext(), &view);
      nsCOMPtr<nsIViewManager> viewManager;
      view->GetViewManager(*getter_AddRefs(viewManager));
      viewManager->ResizeView(view, bounds.width, bounds.height);
    }

  }

  SyncLayout(aState);

  return rv;
}

NS_IMETHODIMP
nsMenuFrame::MarkChildrenStyleChange()
{
  nsresult rv = nsBoxFrame::MarkChildrenStyleChange();
  if (NS_FAILED(rv))
    return rv;
   
  nsIFrame* popupChild = mPopupFrames.FirstChild();

  if (popupChild) {
    nsIBox* ibox = nsnull;
    nsresult rv2 = popupChild->QueryInterface(NS_GET_IID(nsIBox), (void**)&ibox);
    NS_ASSERTION(NS_SUCCEEDED(rv2) && ibox,"popupChild is not box!!");

    return ibox->MarkChildrenStyleChange();
  }

  return rv;
}

/** Replaced by Layout
NS_IMETHODIMP
nsMenuFrame::Reflow(nsIPresContext*   aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus)
{
  //NS_ASSERTION(aReflowState.reason != eReflowReason_Incremental,"Incremental Reflow not supported!");

  nsIFrame* popupChild = mPopupFrames.FirstChild();

  nsHTMLReflowState boxState(aReflowState);

  if (aReflowState.reason == eReflowReason_Incremental) {
    nsIFrame* incrementalChild;

    // get the child but don't pull it off
    aReflowState.reflowCommand->GetNext(incrementalChild, PR_FALSE);
    
    // see if it is in the mPopupFrames list
    nsIFrame* child = mPopupFrames.FirstChild();
    popupChild = nsnull;

    while (nsnull != child) 
    { 
      // if it is then flow the popup incrementally then flow
      // us with a resize just to get our correct desired size.
      if (child == incrementalChild) {
        // pull it off now
        aReflowState.reflowCommand->GetNext(incrementalChild);

        // we know what child
        popupChild = child;

        // relow the box with resize just to get the
        // aDesiredSize set correctly
        boxState.reason = eReflowReason_Resize;
        break;
      }

      nsresult rv = child->GetNextSibling(&child);
      NS_ASSERTION(rv == NS_OK,"failed to get next child");
    }   
  } else if (aReflowState.reason == eReflowReason_Dirty) {
    // sometimes incrementals are converted to dirty. This is done in the case just above this. So lets check
    // to see if this was converted. If it was it will still have a reflow state.
    if (aReflowState.reflowCommand) {
        // it was converted so lets see if the next child is this one. If it is then convert it back and
        // pass it down.
        nsIFrame* incrementalChild = nsnull;
        aReflowState.reflowCommand->GetNext(incrementalChild, PR_FALSE);
        if (incrementalChild == popupChild) 
        {
            nsHTMLReflowState state(aReflowState);
            state.reason = eReflowReason_Incremental;
            return Reflow(aPresContext, aDesiredSize, state, aStatus);
            
        } 
    }
  }

  // Handle reflowing our subordinate popup
  nsHTMLReflowMetrics kidDesiredSize(aDesiredSize);  
  if (popupChild) {         
      // Constrain the child's width and height to aAvailableWidth and aAvailableHeight
      nsSize availSize(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE);
      nsHTMLReflowState kidReflowState(aPresContext, aReflowState, popupChild,
                                       availSize);
      kidReflowState.mComputedWidth = NS_UNCONSTRAINEDSIZE;
      kidReflowState.mComputedHeight = NS_UNCONSTRAINEDSIZE;
      
      nsRect rect;
      popupChild->GetRect(rect);
      ReflowChild(popupChild, aPresContext, kidDesiredSize, kidReflowState,
                                  rect.x, rect.y, NS_FRAME_NO_SIZE_VIEW | NS_FRAME_NO_MOVE_VIEW | NS_FRAME_NO_MOVE_CHILD_VIEWS, aStatus);

      // Set the child's width and height to its desired size
      // Note: don't position or size the view now, we'll do that in the
      // DidReflow() function
      FinishReflowChild(popupChild, aPresContext, kidDesiredSize, rect.x, rect.y, 
                           NS_FRAME_NO_SIZE_VIEW |NS_FRAME_NO_MOVE_VIEW | NS_FRAME_NO_MOVE_CHILD_VIEWS);
  }

  nsresult rv = nsBoxFrame::Reflow(aPresContext, aDesiredSize, boxState, aStatus);

  // If we're a menulist, then we might potentially flow the popup a second time
  // (its width may be too small).
  nsCOMPtr<nsIDOMXULMenuListElement> menulist = do_QueryInterface(mContent);
  if (menulist && popupChild) {
    if (kidDesiredSize.width < aDesiredSize.width) {
      // Flow the popup again.
      // Constrain the child's width and height to aAvailableWidth and aAvailableHeight
      nsSize availSize(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE);
      nsHTMLReflowState kidReflowState(aPresContext, aReflowState, popupChild,
                                       availSize);
      kidReflowState.mComputedWidth = aDesiredSize.width;
      kidReflowState.mComputedHeight = NS_UNCONSTRAINEDSIZE;
    
      nsRect rect;
      popupChild->GetRect(rect);
      ReflowChild(popupChild, aPresContext, kidDesiredSize, kidReflowState,
                                  rect.x, rect.y, 
                                  NS_FRAME_NO_SIZE_VIEW | NS_FRAME_NO_MOVE_VIEW | NS_FRAME_NO_MOVE_CHILD_VIEWS, aStatus);

      // Set the child's width and height to its desired size
      // Note: don't position or size the view now, we'll do that in the
      // DidReflow() function
      FinishReflowChild(popupChild, aPresContext, kidDesiredSize, rect.x, rect.y, 
                        NS_FRAME_NO_SIZE_VIEW | NS_FRAME_NO_MOVE_VIEW | NS_FRAME_NO_MOVE_CHILD_VIEWS);
    }
  }
  return rv;
}
*/

NS_IMETHODIMP
nsMenuFrame::SetDebug(nsBoxLayoutState& aState, PRBool aDebug)
{
  // see if our state matches the given debug state
  PRBool debugSet = mState & NS_STATE_CURRENTLY_IN_DEBUG;
  PRBool debugChanged = (!aDebug && debugSet) || (aDebug && !debugSet);

  // if it doesn't then tell each child below us the new debug state
  if (debugChanged)
  {
      nsBoxFrame::SetDebug(aState, aDebug);
      SetDebug(aState, mPopupFrames.FirstChild(), aDebug);
  }

  return NS_OK;
}

nsresult
nsMenuFrame::SetDebug(nsBoxLayoutState& aState, nsIFrame* aList, PRBool aDebug)
{
      if (!aList)
          return NS_OK;

      while (aList) {
          nsIBox* ibox = nsnull;
          if (NS_SUCCEEDED(aList->QueryInterface(NS_GET_IID(nsIBox), (void**)&ibox)) && ibox) {
              ibox->SetDebug(aState, aDebug);
          }

          aList->GetNextSibling(&aList);
      }

      return NS_OK;
}


void
nsMenuFrame::RePositionPopup(nsBoxLayoutState& aState)
{  
  nsIPresContext* presContext = aState.GetPresContext();

  // Sync up the view.
  nsIFrame* frame = mPopupFrames.FirstChild();
  nsMenuPopupFrame* menuPopup = (nsMenuPopupFrame*)frame;
  if (mMenuOpen && menuPopup) {
    nsCOMPtr<nsIContent> menuPopupContent;
    menuPopup->GetContent(getter_AddRefs(menuPopupContent));
    nsAutoString popupAnchor, popupAlign;
      
    menuPopupContent->GetAttribute(kNameSpaceID_None, nsXULAtoms::popupanchor, popupAnchor);
    menuPopupContent->GetAttribute(kNameSpaceID_None, nsXULAtoms::popupalign, popupAlign);

    PRBool onMenuBar = PR_TRUE;
    if (mMenuParent)
      mMenuParent->IsMenuBar(onMenuBar);

    if (onMenuBar) {
      if (popupAnchor.IsEmpty())
          popupAnchor.AssignWithConversion("bottomleft");
      if (popupAlign.IsEmpty())
          popupAlign.AssignWithConversion("topleft");
    }
    else {
      if (popupAnchor.IsEmpty())
        popupAnchor.AssignWithConversion("topright");
      if (popupAlign.IsEmpty())
        popupAlign.AssignWithConversion("topleft");
    }

    menuPopup->SyncViewWithFrame(presContext, popupAnchor, popupAlign, this, -1, -1);
  }
}

NS_IMETHODIMP
nsMenuFrame::ShortcutNavigation(PRUint32 aLetter, PRBool& aHandledFlag)
{
  nsIFrame* frame = mPopupFrames.FirstChild();
  if (frame) {
    nsMenuPopupFrame* popup = (nsMenuPopupFrame*)frame;
    popup->ShortcutNavigation(aLetter, aHandledFlag);
  } 

  return NS_OK;
}

NS_IMETHODIMP
nsMenuFrame::KeyboardNavigation(PRUint32 aDirection, PRBool& aHandledFlag)
{
  nsIFrame* frame = mPopupFrames.FirstChild();
  if (frame) {
    nsMenuPopupFrame* popup = (nsMenuPopupFrame*)frame;
    popup->KeyboardNavigation(aDirection, aHandledFlag);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsMenuFrame::Escape(PRBool& aHandledFlag)
{
  nsIFrame* frame = mPopupFrames.FirstChild();
  if (frame) {
    nsMenuPopupFrame* popup = (nsMenuPopupFrame*)frame;
    popup->Escape(aHandledFlag);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsMenuFrame::Enter()
{
  if (!mMenuOpen) {
    // The enter key press applies to us.
    if (!IsMenu() && mMenuParent) {
      // Execute our event handler
      Execute();
    }
    else {
      OpenMenu(PR_TRUE);
      SelectFirstItem();
    }

    return NS_OK;
  }

  nsIFrame* frame = mPopupFrames.FirstChild();
  if (frame) {
    nsMenuPopupFrame* popup = (nsMenuPopupFrame*)frame;
    popup->Enter();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsMenuFrame::SelectFirstItem()
{
  nsIFrame* frame = mPopupFrames.FirstChild();
  if (frame) {
    nsMenuPopupFrame* popup = (nsMenuPopupFrame*)frame;
    nsIMenuFrame* result;
    popup->GetNextMenuItem(nsnull, &result);
    popup->SetCurrentMenuItem(result);
  }

  return NS_OK;
}

PRBool
nsMenuFrame::IsMenu()
{
  return mIsMenu;
}

NS_IMETHODIMP_(void)
nsMenuFrame::Notify(nsITimer* aTimer)
{
  // Our timer has fired.
  if (aTimer == mOpenTimer.get()) {
    if (!mMenuOpen && mMenuParent) {
      nsAutoString active;
      mContent->GetAttribute(kNameSpaceID_None, nsXULAtoms::menuactive, active);
      if (active.EqualsWithConversion("true")) {
        // We're still the active menu.
        OpenMenu(PR_TRUE);
      }
    }
    mOpenTimer->Cancel();
    mOpenTimer = nsnull;
  }
  
  mOpenTimer = nsnull;
}

PRBool 
nsMenuFrame::IsDisabled()
{
  nsAutoString disabled;
  mContent->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::disabled, disabled);
  if (disabled.EqualsWithConversion("true"))
    return PR_TRUE;
  return PR_FALSE;
}

void
nsMenuFrame::UpdateMenuType(nsIPresContext* aPresContext)
{
  nsAutoString value;
  mContent->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::type, value);
  if (value.EqualsWithConversion("checkbox"))
    mType = eMenuType_Checkbox;
  else if (value.EqualsWithConversion("radio")) {
    mType = eMenuType_Radio;

    nsAutoString valueName;
    mContent->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::name, valueName);
    if ( mGroupName != valueName )
      mGroupName = valueName;
  } 
  else {
    if (mType != eMenuType_Normal)
      mContent->UnsetAttribute(kNameSpaceID_None, nsHTMLAtoms::checked,
                               PR_TRUE);
    mType = eMenuType_Normal;
  }
  UpdateMenuSpecialState(aPresContext);
}

/* update checked-ness for type="checkbox" and type="radio" */
void
nsMenuFrame::UpdateMenuSpecialState(nsIPresContext* aPresContext) {
  nsAutoString value;
  PRBool newChecked;

  mContent->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::checked,
                         value);
  newChecked = (value.EqualsWithConversion("true"));

  if (newChecked == mChecked) {
    /* checked state didn't change */

    if (mType != eMenuType_Radio)
      return; // only Radio possibly cares about other kinds of change

    mContent->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::name, value);
    if (value == mGroupName)
      return;                   // no interesting change
  } else { 
    mChecked = newChecked;
    if (mType != eMenuType_Radio || !mChecked)
      /*
       * Unchecking something requires no further changes, and only
       * menuRadio has to do additional work when checked.
       */
      return;
  }

  /*
   * If we get this far, we're type=radio, and:
   * - our name= changed, or
   * - we went from checked="false" to checked="true"
   */

  if (!mChecked)
    /*
     * If we're not checked, then it must have been a name change, and a name
     * change on an unchecked item doesn't require any magic.
     */
    return;
  
  /*
   * Behavioural note:
   * If we're checked and renamed _into_ an existing radio group, we are
   * made the new checked item, and we unselect the previous one.
   *
   * The only other reasonable behaviour would be to check for another selected
   * item in that group.  If found, unselect ourselves, otherwise we're the
   * selected item.  That, however, would be a lot more work, and I don't think
   * it's better at all.
   */

  /* walk siblings, looking for the other checked item with the same name */
  nsIFrame *parent, *sib;
  nsIMenuFrame *sibMenu;
  nsMenuType sibType;
  nsAutoString sibGroup;
  PRBool sibChecked;
  
  nsresult rv = GetParent(&parent);
  NS_ASSERTION(NS_SUCCEEDED(rv), "couldn't get parent of radio menu frame\n");
  if (NS_FAILED(rv)) return;
  
  // get the first sibling in this menu popup. This frame may be it, and if we're
  // being called at creation time, this frame isn't yet in the parent's child list.
  // All I'm saying is that this may fail, but it's most likely alright.
  rv = parent->FirstChild(aPresContext, NULL, &sib);
  if ( NS_FAILED(rv) || !sib )
    return;

  do {
    if (NS_FAILED(sib->QueryInterface(NS_GET_IID(nsIMenuFrame),
                                      (void **)&sibMenu)))
        continue;
        
    if (sibMenu != (nsIMenuFrame *)this &&        // correct way to check?
        (sibMenu->GetMenuType(sibType), sibType == eMenuType_Radio) &&
        (sibMenu->MenuIsChecked(sibChecked), sibChecked) &&
        (sibMenu->GetRadioGroupName(sibGroup), sibGroup == mGroupName)) {
      
      nsCOMPtr<nsIContent> content;
      if (NS_FAILED(sib->GetContent(getter_AddRefs(content))))
        continue;             // break?
      
      /* uncheck the old item */
      content->UnsetAttribute(kNameSpaceID_None, nsHTMLAtoms::checked,
                              PR_TRUE);

      /* XXX in DEBUG, check to make sure that there aren't two checked items */
      return;
    }

  } while(NS_SUCCEEDED(sib->GetNextSibling(&sib)) && sib);

}

void 
nsMenuFrame::BuildAcceleratorText(nsString& aAccelString)
{
  nsAutoString accelText;
  mContent->GetAttribute(kNameSpaceID_None, nsXULAtoms::acceltext, accelText);
  if (!accelText.IsEmpty()) {
    // Just use this.
    aAccelString = accelText;
    return;
  }

  // See if we have a key node and use that instead.
  nsAutoString keyValue;
  mContent->GetAttribute(kNameSpaceID_None, nsXULAtoms::key, keyValue);

  if (keyValue.IsEmpty())
    return;

  nsCOMPtr<nsIDocument> document;
  mContent->GetDocument(*getter_AddRefs(document));

  // Turn the document into a XUL document so we can use getElementById
  nsCOMPtr<nsIDOMXULDocument> xulDocument = do_QueryInterface(document);
  if (!xulDocument)
    return;

  nsCOMPtr<nsIDOMElement> keyElement;
  xulDocument->GetElementById(keyValue, getter_AddRefs(keyElement));
  
  if (!keyElement)
    return;
  
  nsAutoString keyAtom; keyAtom.AssignWithConversion("key");
  nsAutoString shiftAtom; shiftAtom.AssignWithConversion("shift");
	nsAutoString altAtom; altAtom.AssignWithConversion("alt");
	nsAutoString commandAtom; commandAtom.AssignWithConversion("command");
  nsAutoString controlAtom; controlAtom.AssignWithConversion("control");

	nsAutoString shiftValue;
	nsAutoString altValue;
	nsAutoString commandValue;
  nsAutoString controlValue;
	nsAutoString keyChar; keyChar.AssignWithConversion(" ");
	
	keyElement->GetAttribute(keyAtom, keyChar);
	keyElement->GetAttribute(shiftAtom, shiftValue);
	keyElement->GetAttribute(altAtom, altValue);
	keyElement->GetAttribute(commandAtom, commandValue);
  keyElement->GetAttribute(controlAtom, controlValue);
	  
  nsAutoString xulkey;
  keyElement->GetAttribute(NS_ConvertASCIItoUCS2("xulkey"), xulkey);
  if (xulkey.EqualsWithConversion("true")) {
    // Set the default for the xul key modifier
#ifdef XP_MAC
    commandValue.AssignWithConversion("true");
#elif defined(XP_PC) || defined(NTO)
    controlValue.AssignWithConversion("true");
#else 
    altValue.AssignWithConversion("true");
#endif
  }
  
  PRBool prependPlus = PR_FALSE;

  if(!commandValue.IsEmpty() && !commandValue.EqualsWithConversion("false")) {
    prependPlus = PR_TRUE;
	  aAccelString.AppendWithConversion("Ctrl"); // Hmmm. Kinda defeats the point of having an abstraction.
  }

  if(!controlValue.IsEmpty() && !controlValue.EqualsWithConversion("false")) {
    prependPlus = PR_TRUE;
	  aAccelString.AppendWithConversion("Ctrl");
  }

  if(!shiftValue.IsEmpty() && !shiftValue.EqualsWithConversion("false")) {
    if (prependPlus)
      aAccelString.AppendWithConversion("+");
    prependPlus = PR_TRUE;
    aAccelString.AppendWithConversion("Shift");
  }

  if (!altValue.IsEmpty() && !altValue.EqualsWithConversion("false")) {
	  if (prependPlus)
      aAccelString.AppendWithConversion("+");
    prependPlus = PR_TRUE;
    aAccelString.AppendWithConversion("Alt");
  }

  keyChar.ToUpperCase();
  if (!keyChar.IsEmpty()) {
    if (prependPlus)
      aAccelString.AppendWithConversion("+");
    prependPlus = PR_TRUE;
    aAccelString += keyChar;
  }

  if (!aAccelString.IsEmpty())
    mContent->SetAttribute(kNameSpaceID_None, nsXULAtoms::acceltext, aAccelString, PR_FALSE);
}

void
nsMenuFrame::Execute()
{
  // Temporarily disable rollup events on this menu.  This is
  // to suppress this menu getting removed in the case where
  // the oncommand handler opens a dialog, etc.
  if ( nsMenuFrame::mDismissalListener ) {
    nsMenuFrame::mDismissalListener->EnableListener(PR_FALSE);
  }

  // Get our own content node and hold on to it to keep it from going away.
  nsCOMPtr<nsIContent> content = dont_QueryInterface(mContent);

  // First hide all of the open menus.
  if (mMenuParent)
    mMenuParent->HideChain();

  nsEventStatus status = nsEventStatus_eIgnore;
  nsMouseEvent event;
  event.eventStructType = NS_EVENT;
  event.message = NS_MENU_ACTION;
  event.isShift = PR_FALSE;
  event.isControl = PR_FALSE;
  event.isAlt = PR_FALSE;
  event.isMeta = PR_FALSE;
  event.clickCount = 0;
  event.widget = nsnull;
  nsCOMPtr<nsIPresShell> shell;
  nsresult result = mPresContext->GetShell(getter_AddRefs(shell));
  if (NS_SUCCEEDED(result) && shell) {
    shell->HandleDOMEventWithTarget(mContent, &event, &status);
  }

  // XXX HACK. Just gracefully exit if the node has been removed, e.g., window.close()
  // was executed.
  nsCOMPtr<nsIDocument> doc;
  content->GetDocument(*getter_AddRefs(doc));

  // Now properly close them all up.
  if (doc && mMenuParent)
    mMenuParent->DismissChain();

  // Re-enable rollup events on this menu.
  if ( nsMenuFrame::mDismissalListener ) {
	nsMenuFrame::mDismissalListener->EnableListener(PR_TRUE);
  }
}

PRBool
nsMenuFrame::OnCreate()
{
  nsEventStatus status = nsEventStatus_eIgnore;
  nsMouseEvent event;
  event.eventStructType = NS_EVENT;
  event.message = NS_MENU_CREATE;
  event.isShift = PR_FALSE;
  event.isControl = PR_FALSE;
  event.isAlt = PR_FALSE;
  event.isMeta = PR_FALSE;
  event.clickCount = 0;
  event.widget = nsnull;
  
  nsCOMPtr<nsIContent> child;
  GetMenuChildrenElement(getter_AddRefs(child));
  
  nsresult rv;
  nsCOMPtr<nsIPresShell> shell;
  rv = mPresContext->GetShell(getter_AddRefs(shell));
  if (NS_SUCCEEDED(rv) && shell) {
    if (child) {
      rv = shell->HandleDOMEventWithTarget(child, &event, &status);
    }
    else {
      rv = shell->HandleDOMEventWithTarget(mContent, &event, &status);
    }
  }

  if ( NS_FAILED(rv) || status == nsEventStatus_eConsumeNoDefault )
    return PR_FALSE;
  return PR_TRUE;
}

PRBool
nsMenuFrame::OnDestroy()
{
  nsEventStatus status = nsEventStatus_eIgnore;
  nsMouseEvent event;
  event.eventStructType = NS_EVENT;
  event.message = NS_MENU_DESTROY;
  event.isShift = PR_FALSE;
  event.isControl = PR_FALSE;
  event.isAlt = PR_FALSE;
  event.isMeta = PR_FALSE;
  event.clickCount = 0;
  event.widget = nsnull;
  
  nsCOMPtr<nsIContent> child;
  GetMenuChildrenElement(getter_AddRefs(child));
  
  nsresult rv;
  nsCOMPtr<nsIPresShell> shell;
  rv = mPresContext->GetShell(getter_AddRefs(shell));
  if (NS_SUCCEEDED(rv) && shell) {
    if (child) {
      rv = shell->HandleDOMEventWithTarget(child, &event, &status);
    }
    else {
      rv = shell->HandleDOMEventWithTarget(mContent, &event, &status);
    }
  }

  if ( NS_FAILED(rv) || status == nsEventStatus_eConsumeNoDefault )
    return PR_FALSE;
  return PR_TRUE;
}

NS_IMETHODIMP
nsMenuFrame::RemoveFrame(nsIPresContext* aPresContext,
                           nsIPresShell& aPresShell,
                           nsIAtom* aListName,
                           nsIFrame* aOldFrame)
{
  nsresult  rv;

  if (mPopupFrames.ContainsFrame(aOldFrame)) {
    // Go ahead and remove this frame.
    mPopupFrames.DestroyFrame(aPresContext, aOldFrame);
    nsBoxLayoutState state(aPresContext);
    rv = MarkDirtyChildren(state);
  } else {
    rv = nsBoxFrame::RemoveFrame(aPresContext, aPresShell, aListName, aOldFrame);
  }

  return rv;
}

NS_IMETHODIMP
nsMenuFrame::InsertFrames(nsIPresContext* aPresContext,
                            nsIPresShell& aPresShell,
                            nsIAtom* aListName,
                            nsIFrame* aPrevFrame,
                            nsIFrame* aFrameList)
{
  nsCOMPtr<nsIAtom> tag;
  nsresult          rv;

  nsCOMPtr<nsIMenuParent> menuPar(do_QueryInterface(aFrameList));
  if (menuPar) {
    nsCOMPtr<nsIBox> menupopup = do_QueryInterface(aFrameList);
    NS_ASSERTION(menupopup,"Popup is not a box!!!");
    menupopup->SetParentBox(this);
    mPopupFrames.InsertFrames(nsnull, nsnull, aFrameList);

    nsBoxLayoutState state(aPresContext);
    SetDebug(state, aFrameList, mState & NS_STATE_CURRENTLY_IN_DEBUG);
    rv = MarkDirtyChildren(state);
  } else {
    rv = nsBoxFrame::InsertFrames(aPresContext, aPresShell, aListName, aPrevFrame, aFrameList);  
  }

  return rv;
}

NS_IMETHODIMP
nsMenuFrame::AppendFrames(nsIPresContext* aPresContext,
                           nsIPresShell&   aPresShell,
                           nsIAtom*        aListName,
                           nsIFrame*       aFrameList)
{
  if (!aFrameList)
    return NS_OK;

  nsCOMPtr<nsIAtom> tag;
  nsresult          rv;

  nsCOMPtr<nsIMenuParent> menuPar(do_QueryInterface(aFrameList));
  if (menuPar) {
    nsCOMPtr<nsIBox> menupopup = do_QueryInterface(aFrameList);
    NS_ASSERTION(menupopup,"Popup is not a box!!!");
    menupopup->SetParentBox(this);

    mPopupFrames.AppendFrames(nsnull, aFrameList);
    nsBoxLayoutState state(aPresContext);
    SetDebug(state, aFrameList, mState & NS_STATE_CURRENTLY_IN_DEBUG);
    rv = MarkDirtyChildren(state);
  } else {
    rv = nsBoxFrame::AppendFrames(aPresContext, aPresShell, aListName, aFrameList); 
  }

  return rv;
}

void
nsMenuFrame::UpdateDismissalListener(nsIMenuParent* aMenuParent)
{
  if (!nsMenuFrame::mDismissalListener) {
    if (!aMenuParent)
       return;
    // Create the listener and attach it to the outermost window.
    aMenuParent->CreateDismissalListener();
  }
  
  // Make sure the menu dismissal listener knows what the current
  // innermost menu popup frame is.
  nsMenuFrame::mDismissalListener->SetCurrentMenuParent(aMenuParent);
}

NS_IMETHODIMP
nsMenuFrame::GetPrefSize(nsBoxLayoutState& aState, nsSize& aSize)
{
  aSize.width = 0;
  aSize.height = 0;
  nsresult rv = nsBoxFrame::GetPrefSize(aState, aSize);

  nsCOMPtr<nsIDOMXULMenuListElement> menulist(do_QueryInterface(mContent));
  if (menulist) {
      nsCOMPtr<nsIDOMElement> element;
      menulist->GetSelectedItem(getter_AddRefs(element));
      if (!element) {
        nsAutoString value;
        menulist->GetValue(value);
        if (value.IsEmpty()) {
          nsCOMPtr<nsIContent> child;
          GetMenuChildrenElement(getter_AddRefs(child));
          if (child) {
            PRInt32 count;
            child->ChildCount(count);
            if (count > 0) {
              nsCOMPtr<nsIContent> item;
              child->ChildAt(0, *getter_AddRefs(item));
              nsCOMPtr<nsIDOMElement> selectedElement(do_QueryInterface(item));
              if (selectedElement) 
                menulist->SetSelectedItem(selectedElement);
            }
          }
        }
      }

     nsSize tmpSize(-1,0);
     nsIBox::AddCSSPrefSize(aState, this, tmpSize);
     nscoord flex;
     GetFlex(aState, flex);

     if (tmpSize.width == -1 && flex==0) {
        nsIFrame* frame = mPopupFrames.FirstChild();
        if (!frame) {
          MarkAsGenerated();
          frame = mPopupFrames.FirstChild();
          // No child - just return
          if (!frame) return NS_OK;
        }
      
        nsIBox* ibox = nsnull;
        nsresult rv2 = frame->QueryInterface(NS_GET_IID(nsIBox), (void**)&ibox);
        NS_ASSERTION(NS_SUCCEEDED(rv2) && ibox,"popupChild is not box!!");

        ibox->GetPrefSize(aState, tmpSize);
        aSize.width = tmpSize.width;
     }
  }

  return rv;
}

/* Need to figure out what this does.
NS_IMETHODIMP
nsMenuFrame::GetBoxInfo(nsIPresContext* aPresContext, const nsHTMLReflowState& aReflowState, nsBoxInfo& aSize)
{
  nsresult rv = nsBoxFrame::GetBoxInfo(aPresContext, aReflowState, aSize);
  nsCOMPtr<nsIDOMXULMenuListElement> menulist(do_QueryInterface(mContent));
  if (menulist) {
    nsCalculatedBoxInfo boxInfo(this);
    boxInfo.prefSize.width = NS_UNCONSTRAINEDSIZE;
    boxInfo.prefSize.height = NS_UNCONSTRAINEDSIZE;
    boxInfo.flex = 0;
    GetRedefinedMinPrefMax(aPresContext, this, boxInfo);
    if (boxInfo.prefSize.width == NS_UNCONSTRAINEDSIZE &&
        boxInfo.prefSize.height == NS_UNCONSTRAINEDSIZE &&
        boxInfo.flex == 0) {
      nsIFrame* frame = mPopupFrames.FirstChild();
      if (!frame) {
        MarkAsGenerated();
        frame = mPopupFrames.FirstChild();
      }
      
      nsCOMPtr<nsIBox> box(do_QueryInterface(frame));
      nsCalculatedBoxInfo childInfo(frame);
      box->GetBoxInfo(aPresContext, aReflowState, childInfo);
      GetRedefinedMinPrefMax(aPresContext, this, childInfo);
      aSize.prefSize.width = childInfo.prefSize.width;
    }

    // This retrieval guarantess that the selectedItem will
    // be set before we lay out.
    nsCOMPtr<nsIDOMElement> element;
    menulist->GetSelectedItem(getter_AddRefs(element));
  }
  return rv;
}
*/

