/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *   Michael Lowe <michael.lowe@bigfoot.com>
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
#include "nsHTMLParts.h"
#include "nsMenuFrame.h"
#include "nsBoxFrame.h"
#include "nsIContent.h"
#include "prtypes.h"
#include "nsIAtom.h"
#include "nsPresContext.h"
#include "nsIPresShell.h"
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
#include "nsIDOMElement.h"
#include "nsISupportsArray.h"
#include "nsIDOMText.h"
#include "nsILookAndFeel.h"
#include "nsIComponentManager.h"
#include "nsWidgetsCID.h"
#include "nsBoxLayoutState.h"
#include "nsIScrollableFrame.h"
#include "nsIViewManager.h"
#include "nsBindingManager.h"
#include "nsIServiceManager.h"
#include "nsCSSFrameConstructor.h"
#include "nsIDOMKeyEvent.h"
#include "nsIScrollableView.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"
#include "nsIStringBundle.h"
#include "nsGUIEvent.h"
#include "nsIEventStateManager.h"
#include "nsContentUtils.h"
#include "nsDisplayList.h"
#include "nsIReflowCallback.h"

#define NS_MENU_POPUP_LIST_INDEX   0

#if defined(XP_WIN) || defined(XP_OS2)
#define NSCONTEXTMENUISMOUSEUP 1
#endif

static PRInt32 gEatMouseMove = PR_FALSE;

static NS_DEFINE_IID(kLookAndFeelCID, NS_LOOKANDFEEL_CID);

nsrefcnt nsMenuFrame::gRefCnt = 0;
nsString *nsMenuFrame::gShiftText = nsnull;
nsString *nsMenuFrame::gControlText = nsnull;
nsString *nsMenuFrame::gMetaText = nsnull;
nsString *nsMenuFrame::gAltText = nsnull;
nsString *nsMenuFrame::gModifierSeparator = nsnull;

//
// NS_NewMenuFrame
//
// Wrapper for creating a new menu popup container
//
nsIFrame*
NS_NewMenuFrame(nsIPresShell* aPresShell, nsStyleContext* aContext, PRUint32 aFlags)
{
  nsMenuFrame* it = new (aPresShell) nsMenuFrame (aPresShell, aContext);
  
  if ((it != nsnull) && aFlags)
    it->SetIsMenu(PR_TRUE);

  return it;
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
  NS_INTERFACE_MAP_ENTRY(nsIMenuFrame)
  NS_INTERFACE_MAP_ENTRY(nsIScrollableViewProvider)
NS_INTERFACE_MAP_END_INHERITING(nsBoxFrame)

//
// nsMenuFrame cntr
//
nsMenuFrame::nsMenuFrame(nsIPresShell* aShell, nsStyleContext* aContext):
  nsBoxFrame(aShell, aContext),
    mIsMenu(PR_FALSE),
    mMenuOpen(PR_FALSE),
    mCreateHandlerSucceeded(PR_FALSE),
    mChecked(PR_FALSE),
    mType(eMenuType_Normal),
    mMenuParent(nsnull),
    mLastPref(-1,-1)
{

} // cntr

NS_IMETHODIMP
nsMenuFrame::SetParent(const nsIFrame* aParent)
{
  nsBoxFrame::SetParent(aParent);
  const nsIFrame* currFrame = aParent;
  while (!mMenuParent && currFrame) {
    // Set our menu parent.
    CallQueryInterface(NS_CONST_CAST(nsIFrame*, currFrame), &mMenuParent);

    currFrame = currFrame->GetParent();
  }

  return NS_OK;
}

class nsASyncMenuInitialization : public nsIReflowCallback
{
public:
  nsASyncMenuInitialization(nsIFrame* aFrame)
    : mWeakFrame(aFrame)
  {
  }

  virtual PRBool ReflowFinished() {
    PRBool shouldFlush = PR_FALSE;
    if (mWeakFrame.IsAlive()) {
      nsIMenuFrame* imenu = nsnull;
      CallQueryInterface(mWeakFrame.GetFrame(), &imenu);
      if (imenu) {
        nsMenuFrame* menu = NS_STATIC_CAST(nsMenuFrame*, imenu);
        menu->UpdateMenuType(menu->GetPresContext());
        shouldFlush = PR_TRUE;
      }
    }
    delete this;
    return shouldFlush;
  }

  nsWeakFrame mWeakFrame;
};

NS_IMETHODIMP
nsMenuFrame::Init(nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsIFrame*        aPrevInFlow)
{
  nsresult  rv = nsBoxFrame::Init(aContent, aParent, aPrevInFlow);

  // Set up a mediator which can be used for callbacks on this frame.
  mTimerMediator = new nsMenuTimerMediator(this);
  if (NS_UNLIKELY(!mTimerMediator))
    return NS_ERROR_OUT_OF_MEMORY;

  nsIFrame* currFrame = aParent;
  while (!mMenuParent && currFrame) {
    // Set our menu parent.
    CallQueryInterface(currFrame, &mMenuParent);

    currFrame = currFrame->GetParent();
  }

  //load the display strings for the keyboard accelerators, but only once
  if (gRefCnt++ == 0) {
    
    nsCOMPtr<nsIStringBundleService> bundleService(do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv));
    nsCOMPtr<nsIStringBundle> bundle;
    if (NS_SUCCEEDED(rv) && bundleService) {
      rv = bundleService->CreateBundle( "chrome://global-platform/locale/platformKeys.properties",
                                        getter_AddRefs(bundle));
    }    
    
    NS_ASSERTION(NS_SUCCEEDED(rv) && bundle, "chrome://global/locale/platformKeys.properties could not be loaded");
    nsXPIDLString shiftModifier;
    nsXPIDLString metaModifier;
    nsXPIDLString altModifier;
    nsXPIDLString controlModifier;
    nsXPIDLString modifierSeparator;
    if (NS_SUCCEEDED(rv) && bundle) {
      //macs use symbols for each modifier key, so fetch each from the bundle, which also covers i18n
      rv = bundle->GetStringFromName(NS_LITERAL_STRING("VK_SHIFT").get(), getter_Copies(shiftModifier));
      rv = bundle->GetStringFromName(NS_LITERAL_STRING("VK_META").get(), getter_Copies(metaModifier));
      rv = bundle->GetStringFromName(NS_LITERAL_STRING("VK_ALT").get(), getter_Copies(altModifier));
      rv = bundle->GetStringFromName(NS_LITERAL_STRING("VK_CONTROL").get(), getter_Copies(controlModifier));
      rv = bundle->GetStringFromName(NS_LITERAL_STRING("MODIFIER_SEPARATOR").get(), getter_Copies(modifierSeparator));
    } else {
      rv = NS_ERROR_NOT_AVAILABLE;
    }
    //if any of these don't exist, we get  an empty string
    gShiftText = new nsString(shiftModifier);
    gMetaText = new nsString(metaModifier);
    gAltText = new nsString(altModifier);
    gControlText = new nsString(controlModifier);
    gModifierSeparator = new nsString(modifierSeparator);    
  }

  BuildAcceleratorText();
  nsIReflowCallback* cb = new nsASyncMenuInitialization(this);
  NS_ENSURE_TRUE(cb, NS_ERROR_OUT_OF_MEMORY);
  GetPresContext()->PresShell()->PostReflowCallback(cb);
  return rv;
}

nsMenuFrame::~nsMenuFrame()
{
  // Clean up shared statics
  if (--gRefCnt == 0) {
    delete gShiftText;
    gShiftText = nsnull;
    delete gControlText;  
    gControlText = nsnull;
    delete gMetaText;  
    gMetaText = nsnull;
    delete gAltText;  
    gAltText = nsnull;
    delete gModifierSeparator;
    gModifierSeparator = nsnull;
  }
}

// The following methods are all overridden to ensure that the menupopup frame
// is placed in the appropriate list.
nsIFrame*
nsMenuFrame::GetFirstChild(nsIAtom* aListName) const
{
  if (nsGkAtoms::popupList == aListName) {
    return mPopupFrames.FirstChild();
  }
  return nsBoxFrame::GetFirstChild(aListName);
}

NS_IMETHODIMP
nsMenuFrame::SetInitialChildList(nsIAtom*        aListName,
                                 nsIFrame*       aChildList)
{
  nsresult rv = NS_OK;
  if (nsGkAtoms::popupList == aListName) {
    mPopupFrames.SetFrames(aChildList);
  } else {

    nsFrameList frames(aChildList);

    // We may have a menupopup in here. Get it out, and move it into
    // the popup frame list.
    nsIFrame* frame = frames.FirstChild();
    while (frame) {
      nsIMenuParent *menuPar;
      CallQueryInterface(frame, &menuPar);
      if (menuPar) {
        PRBool isMenuBar;
        menuPar->IsMenuBar(isMenuBar);
        if (!isMenuBar) {
          // Remove this frame from the list and place it in the other list.
          frames.RemoveFrame(frame);
          mPopupFrames.AppendFrame(this, frame);
          nsIFrame* first = frames.FirstChild();
          rv = nsBoxFrame::SetInitialChildList(aListName, first);
          return rv;
        }
      }
      frame = frame->GetNextSibling();
    }

    // Didn't find it.
    rv = nsBoxFrame::SetInitialChildList(aListName, aChildList);
  }
  return rv;
}

nsIAtom*
nsMenuFrame::GetAdditionalChildListName(PRInt32 aIndex) const
{
  if (NS_MENU_POPUP_LIST_INDEX == aIndex) {
    return nsGkAtoms::popupList;
  }

  return nsnull;
}

nsresult
nsMenuFrame::DestroyPopupFrames(nsPresContext* aPresContext)
{
  // Remove our frame mappings
  nsCSSFrameConstructor* frameConstructor =
    aPresContext->PresShell()->FrameConstructor();
  nsIFrame* curFrame = mPopupFrames.FirstChild();
  while (curFrame) {
    frameConstructor->RemoveMappingsForFrameSubtree(curFrame);
    curFrame = curFrame->GetNextSibling();
  }

   // Cleanup frames in popup child list
  mPopupFrames.DestroyFrames();
  return NS_OK;
}

void
nsMenuFrame::Destroy()
{
  // Kill our timer if one is active. This is not strictly necessary as
  // the pointer to this frame will be cleared from the mediator, but
  // this is done for added safety.
  if (mOpenTimer) {
    mOpenTimer->Cancel();
  }

  // Null out the pointer to this frame in the mediator wrapper so that it 
  // doesn't try to interact with a deallocated frame.
  mTimerMediator->ClearFrame();

  nsWeakFrame weakFrame(this);
  // are we our menu parent's current menu item?
  if (mMenuParent) {
    nsIMenuFrame *curItem = mMenuParent->GetCurrentMenuItem();
    if (curItem == this) {
      // yes; tell it that we're going away
      mMenuParent->SetCurrentMenuItem(nsnull);
      ENSURE_TRUE(weakFrame.IsAlive());
    }
  }

  UngenerateMenu();
  ENSURE_TRUE(weakFrame.IsAlive());
  DestroyPopupFrames(GetPresContext());
  nsBoxFrame::Destroy();
}

NS_IMETHODIMP
nsMenuFrame::BuildDisplayListForChildren(nsDisplayListBuilder*   aBuilder,
                                         const nsRect&           aDirtyRect,
                                         const nsDisplayListSet& aLists)
{
  if (!aBuilder->IsForEventDelivery())
    return nsBoxFrame::BuildDisplayListForChildren(aBuilder, aDirtyRect, aLists);
    
  nsDisplayListCollection set;
  nsresult rv = nsBoxFrame::BuildDisplayListForChildren(aBuilder, aDirtyRect, set);
  NS_ENSURE_SUCCESS(rv, rv);
  
  return WrapListsInRedirector(aBuilder, set, aLists);
}

NS_IMETHODIMP 
nsMenuFrame::HandleEvent(nsPresContext* aPresContext, 
                             nsGUIEvent*     aEvent,
                             nsEventStatus*  aEventStatus)
{
  NS_ENSURE_ARG_POINTER(aEventStatus);
  nsWeakFrame weakFrame(this);
  if (*aEventStatus == nsEventStatus_eIgnore)
    *aEventStatus = nsEventStatus_eConsumeDoDefault;
  
  if (aEvent->message == NS_KEY_PRESS && !IsDisabled()) {
    nsKeyEvent* keyEvent = (nsKeyEvent*)aEvent;
    PRUint32 keyCode = keyEvent->keyCode;
#ifdef XP_MACOSX
    // On mac, open menulist on either up/down arrow or space (w/o Cmd pressed)
    if (!IsOpen() && ((keyEvent->charCode == NS_VK_SPACE && !keyEvent->isMeta) ||
        (keyCode == NS_VK_UP || keyCode == NS_VK_DOWN)))
      OpenMenu(PR_TRUE);
#else
    // On other platforms, toggle menulist on unmodified F4 or Alt arrow
    if ((keyCode == NS_VK_F4 && !keyEvent->isAlt) ||
        ((keyCode == NS_VK_UP || keyCode == NS_VK_DOWN) && keyEvent->isAlt))
      OpenMenu(!IsOpen());
#endif
  }
  else if (aEvent->eventStructType == NS_MOUSE_EVENT &&
           aEvent->message == NS_MOUSE_BUTTON_DOWN &&
           NS_STATIC_CAST(nsMouseEvent*, aEvent)->button == nsMouseEvent::eLeftButton &&
           !IsDisabled() && IsMenu()) {
    PRBool isMenuBar = PR_FALSE;
    if (mMenuParent)
      mMenuParent->IsMenuBar(isMenuBar);

    // The menu item was selected. Bring up the menu.
    // We have children.
    if ( isMenuBar || !mMenuParent ) {
      ToggleMenuState();
      NS_ENSURE_TRUE(weakFrame.IsAlive(), NS_OK);

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
  else if (
#ifndef NSCONTEXTMENUISMOUSEUP
           (aEvent->eventStructType == NS_MOUSE_EVENT &&
            aEvent->message == NS_MOUSE_BUTTON_UP &&
            NS_STATIC_CAST(nsMouseEvent*, aEvent)->button ==
              nsMouseEvent::eRightButton) &&
#else
            aEvent->message == NS_CONTEXTMENU &&
#endif
            mMenuParent && !IsMenu() && !IsDisabled()) {
    // if this menu is a context menu it accepts right-clicks...fire away!
    // Make sure we cancel default processing of the context menu event so
    // that it doesn't bubble and get seen again by the popuplistener and show
    // another context menu.
    //
    // Furthermore (there's always more, isn't there?), on some platforms (win32
    // being one of them) we get the context menu event on a mouse up while
    // on others we get it on a mouse down. For the ones where we get it on a
    // mouse down, we must continue listening for the right button up event to
    // dismiss the menu.
    PRBool isContextMenu = PR_FALSE;
    mMenuParent->GetIsContextMenu(isContextMenu);
    if ( isContextMenu ) {
      *aEventStatus = nsEventStatus_eConsumeNoDefault;
      Execute(aEvent);
    }
  }
  else if (aEvent->eventStructType == NS_MOUSE_EVENT &&
           aEvent->message == NS_MOUSE_BUTTON_UP &&
           NS_STATIC_CAST(nsMouseEvent*, aEvent)->button == nsMouseEvent::eLeftButton &&
           !IsMenu() && mMenuParent && !IsDisabled()) {
    // Execute the execute event handler.
    Execute(aEvent);
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
    NS_ENSURE_TRUE(weakFrame.IsAlive(), NS_OK);
    NS_ENSURE_TRUE(mMenuParent, NS_OK);
    
    // we need to check if we really became the current menu
    // item or not
    nsIMenuFrame *realCurrentItem = mMenuParent->GetCurrentMenuItem();
    if (realCurrentItem != this) {
      // we didn't (presumably because a context menu was active)
      return NS_OK;
    }

    // If we're a menu (and not a menu item),
    // kick off the timer.
    if (!IsDisabled() && !isMenuBar && IsMenu() && !mMenuOpen && !mOpenTimer) {

      PRInt32 menuDelay = 300;   // ms

      nsCOMPtr<nsILookAndFeel> lookAndFeel(do_GetService(kLookAndFeelCID));
      if (lookAndFeel)
        lookAndFeel->GetMetric(nsILookAndFeel::eMetric_SubmenuDelay, menuDelay);

      // We're a menu, we're built, we're closed, and no timer has been kicked off.
      mOpenTimer = do_CreateInstance("@mozilla.org/timer;1");
      mOpenTimer->InitWithCallback(mTimerMediator, menuDelay, nsITimer::TYPE_ONE_SHOT);

    }
  }
  
  return NS_OK;
}

NS_IMETHODIMP
nsMenuFrame::ToggleMenuState()
{
  nsWeakFrame weakFrame(this);
  if (mMenuOpen) {
    OpenMenu(PR_FALSE);
    NS_ENSURE_TRUE(weakFrame.IsAlive(), NS_OK);
  }
  else {
    PRBool justRolledUp = PR_FALSE;
    if (mMenuParent) {
      mMenuParent->RecentlyRolledUp(this, &justRolledUp);
    }
    if (justRolledUp) {
      // Don't let a click reopen a menu that was just rolled up
      // from the same click. Otherwise, the user can't click on
      // a menubar item to toggle its submenu closed.
      OpenMenu(PR_FALSE);
      NS_ENSURE_TRUE(weakFrame.IsAlive(), NS_OK);
      SelectMenu(PR_TRUE);
      NS_ENSURE_TRUE(weakFrame.IsAlive(), NS_OK);
      NS_ENSURE_TRUE(mMenuParent, NS_OK);
      mMenuParent->SetActive(PR_FALSE);
    }
    else {
      if (mMenuParent) {
        mMenuParent->SetActive(PR_TRUE);
        NS_ENSURE_TRUE(weakFrame.IsAlive(), NS_OK);
      }
      OpenMenu(PR_TRUE);
    }
  }
  NS_ENSURE_TRUE(weakFrame.IsAlive(), NS_OK);

  if (mMenuParent) {
    // Make sure the current menu which is being toggled on
    // the menubar is highlighted
    mMenuParent->SetCurrentMenuItem(this);
    NS_ENSURE_TRUE(weakFrame.IsAlive(), NS_OK);
    NS_ENSURE_TRUE(mMenuParent, NS_OK);
    // We've successfully prevented the same click from both
    // dismissing and reopening this menu. 
    // Clear the recent rollup state so we don't prevent
    // this menu from being opened by the next click.
    mMenuParent->ClearRecentlyRolledUp();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsMenuFrame::SelectMenu(PRBool aActivateFlag)
{
  if (!mContent) {
    return NS_OK;
  }

  nsAutoString domEventToFire;

  nsWeakFrame weakFrame(this);
  if (aActivateFlag) {
    if (mMenuParent) {
      nsIMenuParent* ancestor = nsnull;
      nsresult rv = mMenuParent->GetParentPopup(&ancestor);
      while (NS_SUCCEEDED(rv) && ancestor) {
        ancestor->CancelPendingTimers();
        rv = ancestor->GetParentPopup(&ancestor);
      }
    }
    // Highlight the menu.
    mContent->SetAttr(kNameSpaceID_None, nsGkAtoms::menuactive, NS_LITERAL_STRING("true"), PR_TRUE);
    // The menuactivated event is used by accessibility to track the user's movements through menus
    domEventToFire.AssignLiteral("DOMMenuItemActive");
  }
  else {
    // Unhighlight the menu.
    mContent->UnsetAttr(kNameSpaceID_None, nsGkAtoms::menuactive, PR_TRUE);
    domEventToFire.AssignLiteral("DOMMenuItemInactive");
  }

  if (weakFrame.IsAlive()) {
    FireDOMEventSynch(domEventToFire);
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
  if (child &&
      !nsContentUtils::HasNonEmptyAttr(child, kNameSpaceID_None,
                                       nsGkAtoms::menugenerated)) {
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
  if (child &&
      !nsContentUtils::HasNonEmptyAttr(child, kNameSpaceID_None,
                                       nsGkAtoms::menugenerated)) {
    child->SetAttr(kNameSpaceID_None, nsGkAtoms::menugenerated,
                   NS_LITERAL_STRING("true"), PR_TRUE);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsMenuFrame::UngenerateMenu()
{
  nsCOMPtr<nsIContent> child;
  GetMenuChildrenElement(getter_AddRefs(child));
  
  if (child &&
      nsContentUtils::HasNonEmptyAttr(child, kNameSpaceID_None,
                                      nsGkAtoms::menugenerated)) {
    child->UnsetAttr(kNameSpaceID_None, nsGkAtoms::menugenerated, PR_TRUE);
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
      nsRect rect = menuPopup->GetRect();
      nsIView* view = menuPopup->GetView();
      nsIViewManager* viewManager = view->GetViewManager();
      rect.x = rect.y = 0;
      viewManager->ResizeView(view, rect);

      // make sure the scrolled window is at 0,0
      if (mLastPref.height <= rect.height) {
        nsIBox* child = menuPopup->GetChildBox();

        nsCOMPtr<nsIScrollableFrame> scrollframe(do_QueryInterface(child));
        if (scrollframe) {
          scrollframe->ScrollTo(nsPoint(0,0));
        }
      }

      viewManager->UpdateView(view, rect, NS_VMREFRESH_IMMEDIATE);
      viewManager->SetViewVisibility(view, nsViewVisibility_kShow);
      GetPresContext()->RootPresContext()->NotifyAddedActivePopupToTop(menuPopup);
  } else {
    if (mMenuOpen) {
      nsWeakFrame weakFrame(this);
      nsWeakFrame weakPopup(menuPopup);
      FireDOMEventSynch(NS_LITERAL_STRING("DOMMenuInactive"), menuPopup->GetContent());
      NS_ENSURE_TRUE(weakFrame.IsAlive() && weakPopup.IsAlive(), NS_OK);
    }
    nsIView* view = menuPopup->GetView();
    NS_ASSERTION(view, "View is gone, looks like someone forgot to rollup the popup!");
    if (view) {
      nsIViewManager* viewManager = view->GetViewManager();
      if (viewManager) { // the view manager can be null during widget teardown
        viewManager->SetViewVisibility(view, nsViewVisibility_kHide);
        viewManager->ResizeView(view, nsRect(0, 0, 0, 0));
      }
    }
    // set here so hide chain can close the menu as well.
    mMenuOpen = PR_FALSE;
    GetPresContext()->RootPresContext()->NotifyRemovedActivePopup(menuPopup);
  }
  
  return NS_OK;
}  

NS_IMETHODIMP
nsMenuFrame::AttributeChanged(PRInt32 aNameSpaceID,
                              nsIAtom* aAttribute,
                              PRInt32 aModType)
{
  nsAutoString value;

  if (aAttribute == nsGkAtoms::checked) {
    if (mType != eMenuType_Normal)
        UpdateMenuSpecialState(GetPresContext());
  } else if (aAttribute == nsGkAtoms::acceltext) {
    // someone reset the accelText attribute, so clear the bit that says *we* set it
    AddStateBits(NS_STATE_ACCELTEXT_IS_DERIVED);
    BuildAcceleratorText();
  } else if (aAttribute == nsGkAtoms::key) {
    BuildAcceleratorText();
  } else if ( aAttribute == nsGkAtoms::type || aAttribute == nsGkAtoms::name )
    UpdateMenuType(GetPresContext());

  return NS_OK;
}

NS_IMETHODIMP
nsMenuFrame::OpenMenu(PRBool aActivateFlag)
{
  if (!mContent)
    return NS_OK;

  nsWeakFrame weakFrame(this);
  if (aActivateFlag) {
    // Now that the menu is opened, we should have a menupopup child built.
    // Mark it as generated, which ensures a frame gets built.
    MarkAsGenerated();
    NS_ENSURE_TRUE(weakFrame.IsAlive(), NS_OK);

    mContent->SetAttr(kNameSpaceID_None, nsGkAtoms::open, NS_LITERAL_STRING("true"), PR_TRUE);
    NS_ENSURE_TRUE(weakFrame.IsAlive(), NS_OK);
    FireDOMEventSynch(NS_LITERAL_STRING("DOMMenuItemActive"));
  }
  else {
    mContent->UnsetAttr(kNameSpaceID_None, nsGkAtoms::open, PR_TRUE);
  }

  NS_ENSURE_TRUE(weakFrame.IsAlive(), NS_OK);
  OpenMenuInternal(aActivateFlag);

  return NS_OK;
}

void 
nsMenuFrame::OpenMenuInternal(PRBool aActivateFlag) 
{
  gEatMouseMove = PR_TRUE;

  if (!mIsMenu)
    return;

  nsPresContext* presContext = GetPresContext();
  nsWeakFrame weakFrame(this);

  if (aActivateFlag) {
    // Execute the oncreate handler
    if (!OnCreate() || !weakFrame.IsAlive())
      return;

    mCreateHandlerSucceeded = PR_TRUE;
  
    // Set the focus back to our view's widget.
    if (nsMenuDismissalListener::sInstance)
      nsMenuDismissalListener::sInstance->EnableListener(PR_FALSE);
    
    // XXX Only have this here because of RDF-generated content.
    MarkAsGenerated();
    ENSURE_TRUE(weakFrame.IsAlive());

    nsIFrame* frame = mPopupFrames.FirstChild();
    nsMenuPopupFrame* menuPopup = (nsMenuPopupFrame*)frame;
    
    PRBool wasOpen = mMenuOpen;
    mMenuOpen = PR_TRUE;

    if (menuPopup) {
      nsWeakFrame weakMenuPopup(frame);
      // inherit whether or not we're a context menu from the parent
      if ( mMenuParent ) {
        PRBool parentIsContextMenu = PR_FALSE;
        mMenuParent->GetIsContextMenu(parentIsContextMenu);
        menuPopup->SetIsContextMenu(parentIsContextMenu);
        ENSURE_TRUE(weakFrame.IsAlive());
      }

      // Install a keyboard navigation listener if we're the root of the menu chain.
      PRBool onMenuBar = PR_TRUE;
      if (mMenuParent)
        mMenuParent->IsMenuBar(onMenuBar);

      if (mMenuParent && onMenuBar)
        mMenuParent->InstallKeyboardNavigator();
      else if (!mMenuParent) {
        ENSURE_TRUE(weakMenuPopup.IsAlive());
        menuPopup->InstallKeyboardNavigator();
      }
      
      // Tell the menu bar we're active.
      if (mMenuParent) {
        mMenuParent->SetActive(PR_TRUE);
        ENSURE_TRUE(weakFrame.IsAlive());
      }

      nsIContent* menuPopupContent = menuPopup->GetContent();

      // Sync up the view.
      nsAutoString popupAnchor, popupAlign;
      
      menuPopupContent->GetAttr(kNameSpaceID_None, nsGkAtoms::popupanchor, popupAnchor);
      menuPopupContent->GetAttr(kNameSpaceID_None, nsGkAtoms::popupalign, popupAlign);

      ConvertPosition(menuPopupContent, popupAnchor, popupAlign);

      if (onMenuBar) {
        if (popupAnchor.IsEmpty())
          popupAnchor.AssignLiteral("bottomleft");
        if (popupAlign.IsEmpty())
          popupAlign.AssignLiteral("topleft");
      }
      else {
        if (popupAnchor.IsEmpty())
          popupAnchor.AssignLiteral("topright");
        if (popupAlign.IsEmpty())
          popupAlign.AssignLiteral("topleft");
      }

      // If the menu popup was not open, do a reflow.  This is either the
      // initial reflow for a brand-new popup, or a subsequent reflow for
      // a menu that was deactivated and needs to be brought back to its
      // active dimensions.
      if (!wasOpen)
      {
         menuPopup->AddStateBits(NS_FRAME_IS_DIRTY);
         presContext->PresShell()->
           FrameNeedsReflow(menuPopup, nsIPresShell::eStyleChange);
         presContext->PresShell()->FlushPendingNotifications(Flush_OnlyReflow);
      }

      nsRect curRect(menuPopup->GetRect());
      nsBoxLayoutState state(presContext);
      menuPopup->SetBounds(state, nsRect(0,0,mLastPref.width, mLastPref.height));

      nsIView* view = menuPopup->GetView();
      nsIViewManager* vm = view->GetViewManager();
      if (vm) {
        vm->SetViewVisibility(view, nsViewVisibility_kHide);
      }
      menuPopup->SyncViewWithFrame(presContext, popupAnchor, popupAlign, this, -1, -1);
      nscoord newHeight = menuPopup->GetRect().height;

      // if the height is different then reflow. It might need scrollbars force a reflow
      if (curRect.height != newHeight || mLastPref.height != newHeight)
      {
         menuPopup->AddStateBits(NS_FRAME_IS_DIRTY);
         presContext->PresShell()->
           FrameNeedsReflow(menuPopup, nsIPresShell::eStyleChange);
         presContext->PresShell()->FlushPendingNotifications(Flush_OnlyReflow);
      }

      ActivateMenu(PR_TRUE);
      ENSURE_TRUE(weakFrame.IsAlive());

      nsIMenuParent *childPopup = nsnull;
      CallQueryInterface(frame, &childPopup);

      nsMenuDismissalListener* listener = nsMenuDismissalListener::GetInstance();
      if (listener)
        listener->SetCurrentMenuParent(childPopup);

      OnCreated();
      ENSURE_TRUE(weakFrame.IsAlive());
    }

    // Set the focus back to our view's widget.
    if (nsMenuDismissalListener::sInstance)
      nsMenuDismissalListener::sInstance->EnableListener(PR_TRUE);

  }
  else {

    // Close the menu. 
    // Execute the ondestroy handler, but only if we're actually open
    if ( !mCreateHandlerSucceeded || !OnDestroy() || !weakFrame.IsAlive())
      return;

    // Set the focus back to our view's widget.
    if (nsMenuDismissalListener::sInstance) {
      nsMenuDismissalListener::sInstance->EnableListener(PR_FALSE);
      nsMenuDismissalListener::sInstance->SetCurrentMenuParent(mMenuParent);
    }

    nsIFrame* frame = mPopupFrames.FirstChild();
    nsMenuPopupFrame* menuPopup = (nsMenuPopupFrame*)frame;
  
    // Make sure we clear out our own items.
    if (menuPopup) {
      menuPopup->SetCurrentMenuItem(nsnull);
      ENSURE_TRUE(weakFrame.IsAlive());
      menuPopup->KillCloseTimer();

      PRBool onMenuBar = PR_TRUE;
      if (mMenuParent)
        mMenuParent->IsMenuBar(onMenuBar);

      if (mMenuParent && onMenuBar)
        mMenuParent->RemoveKeyboardNavigator();
      else if (!mMenuParent)
        menuPopup->RemoveKeyboardNavigator();

      // XXX, bug 137033, In Windows, if mouse is outside the window when the menupopup closes, no
      // mouse_enter/mouse_exit event will be fired to clear current hover state, we should clear it manually.
      // This code may not the best solution, but we can leave it here until we find the better approach.

      nsIEventStateManager *esm = presContext->EventStateManager();

      PRInt32 state;
      esm->GetContentState(menuPopup->GetContent(), state);

      if (state & NS_EVENT_STATE_HOVER)
        esm->SetContentState(nsnull, NS_EVENT_STATE_HOVER);
    }

    ActivateMenu(PR_FALSE);
    ENSURE_TRUE(weakFrame.IsAlive());
    // XXX hack: ensure that mMenuOpen is set to false, in case where
    // there is actually no popup. because ActivateMenu() will return 
    // early without setting it. It could be that mMenuOpen is true
    // in that case, because OpenMenuInternal(true) gets called if
    // the attribute open="true", whether there is a popup or not.
    // We should not allow mMenuOpen unless there is a popup in the first place,
    // in which case this line would not be necessary.
    mMenuOpen = PR_FALSE;

    OnDestroyed();
    ENSURE_TRUE(weakFrame.IsAlive());

    if (nsMenuDismissalListener::sInstance)
      nsMenuDismissalListener::sInstance->EnableListener(PR_TRUE);

    mCreateHandlerSucceeded = PR_FALSE;
  }

}

void
nsMenuFrame::GetMenuChildrenElement(nsIContent** aResult)
{
  *aResult = nsContentUtils::FindFirstChildWithResolvedTag(mContent,
                                                           kNameSpaceID_XUL,
                                                           nsGkAtoms::menupopup);
  NS_IF_ADDREF(*aResult);
}

PRBool
nsMenuFrame::IsSizedToPopup(nsIContent* aContent, PRBool aRequireAlways)
{
  PRBool sizeToPopup;
  if (aContent->Tag() == nsGkAtoms::select)
    sizeToPopup = PR_TRUE;
  else {
    nsAutoString sizedToPopup;
    aContent->GetAttr(kNameSpaceID_None, nsGkAtoms::sizetopopup, sizedToPopup);
    sizeToPopup = sizedToPopup.EqualsLiteral("always") ||
                  !aRequireAlways && sizedToPopup.EqualsLiteral("pref");
  }
  
  return sizeToPopup;
}

nsSize
nsMenuFrame::GetMinSize(nsBoxLayoutState& aBoxLayoutState)
{
  nsSize size = nsBoxFrame::GetMinSize(aBoxLayoutState);
  DISPLAY_MIN_SIZE(this, size);

  if (IsSizedToPopup(mContent, PR_TRUE))
    SizeToPopup(aBoxLayoutState, size);

  return size;
}

NS_IMETHODIMP
nsMenuFrame::DoLayout(nsBoxLayoutState& aState)
{
  // lay us out
  nsresult rv = nsBoxFrame::DoLayout(aState);

  // layout the popup. First we need to get it.
  nsIFrame* popupChild = mPopupFrames.FirstChild();

  if (popupChild) {
    PRBool sizeToPopup = IsSizedToPopup(mContent, PR_FALSE);
    
    NS_ASSERTION(popupChild->IsBoxFrame(), "popupChild is not box!!");

    // then get its preferred size
    nsSize prefSize = popupChild->GetPrefSize(aState);
    nsSize minSize = popupChild->GetMinSize(aState); 
    nsSize maxSize = popupChild->GetMaxSize(aState);

    BoundsCheck(minSize, prefSize, maxSize);

    if (sizeToPopup)
        prefSize.width = mRect.width;

    // if the pref size changed then set bounds to be the pref size
    // and sync the view. And set new pref size.
    if (mLastPref != prefSize) {
      popupChild->SetBounds(aState, nsRect(0,0,prefSize.width, prefSize.height));
      RePositionPopup(aState);
      mLastPref = prefSize;
    }

    // is the new size too small? Make sure we handle scrollbars correctly
    nsIBox* child = popupChild->GetChildBox();

    nsRect bounds(popupChild->GetRect());

    nsCOMPtr<nsIScrollableFrame> scrollframe(do_QueryInterface(child));
    if (scrollframe &&
        scrollframe->GetScrollbarStyles().mVertical == NS_STYLE_OVERFLOW_AUTO) {
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

    // Only size the popups view if open.
    if (mMenuOpen) {
      nsIView* view = popupChild->GetView();
      nsRect r(0, 0, bounds.width, bounds.height);
      view->GetViewManager()->ResizeView(view, r);
    }

  }

  SyncLayout(aState);

  return rv;
}

#ifdef DEBUG_LAYOUT
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
        if (aList->IsBoxFrame())
          aList->SetDebug(aState, aDebug);

        aList = aList->GetNextSibling();
      }

      return NS_OK;
}
#endif

void
nsMenuFrame::ConvertPosition(nsIContent* aPopupElt, nsString& aAnchor, nsString& aAlign)
{
  static nsIContent::AttrValuesArray strings[] =
    {&nsGkAtoms::_empty, &nsGkAtoms::before_start, &nsGkAtoms::before_end,
     &nsGkAtoms::after_start, &nsGkAtoms::after_end, &nsGkAtoms::start_before,
     &nsGkAtoms::start_after, &nsGkAtoms::end_before, &nsGkAtoms::end_after,
     &nsGkAtoms::overlap, nsnull};

  switch (aPopupElt->FindAttrValueIn(kNameSpaceID_None, nsGkAtoms::position,
                                     strings, eCaseMatters)) {
    case nsIContent::ATTR_MISSING:
    case 0:
      return;
    case 1:
      aAnchor.AssignLiteral("topleft");
      aAlign.AssignLiteral("bottomleft");
      break;
    case 2:
      aAnchor.AssignLiteral("topright");
      aAlign.AssignLiteral("bottomright");
      break;
    case 3:
      aAnchor.AssignLiteral("bottomleft");
      aAlign.AssignLiteral("topleft");
      break;
    case 4:
      aAnchor.AssignLiteral("bottomright");
      aAlign.AssignLiteral("topright");
      break;
    case 5:
      aAnchor.AssignLiteral("topleft");
      aAlign.AssignLiteral("topright");
      break;
    case 6:
      aAnchor.AssignLiteral("bottomleft");
      aAlign.AssignLiteral("bottomright");
      break;
    case 7:
      aAnchor.AssignLiteral("topright");
      aAlign.AssignLiteral("topleft");
      break;
    case 8:
      aAnchor.AssignLiteral("bottomright");
      aAlign.AssignLiteral("bottomleft");
      break;
    case 9:
      aAnchor.AssignLiteral("topleft");
      aAlign.AssignLiteral("topleft");
      break;
  }
}

void
nsMenuFrame::RePositionPopup(nsBoxLayoutState& aState)
{  
  nsPresContext* presContext = aState.PresContext();

  // Sync up the view.
  nsIFrame* frame = mPopupFrames.FirstChild();
  nsMenuPopupFrame* menuPopup = (nsMenuPopupFrame*)frame;
  if (mMenuOpen && menuPopup) {
    nsIContent* menuPopupContent = menuPopup->GetContent();
    nsAutoString popupAnchor, popupAlign;
      
    menuPopupContent->GetAttr(kNameSpaceID_None, nsGkAtoms::popupanchor, popupAnchor);
    menuPopupContent->GetAttr(kNameSpaceID_None, nsGkAtoms::popupalign, popupAlign);

    ConvertPosition(menuPopupContent, popupAnchor, popupAlign);

    PRBool onMenuBar = PR_TRUE;
    if (mMenuParent)
      mMenuParent->IsMenuBar(onMenuBar);

    if (onMenuBar) {
      if (popupAnchor.IsEmpty())
          popupAnchor.AssignLiteral("bottomleft");
      if (popupAlign.IsEmpty())
          popupAlign.AssignLiteral("topleft");
    }
    else {
      if (popupAnchor.IsEmpty())
        popupAnchor.AssignLiteral("topright");
      if (popupAlign.IsEmpty())
        popupAlign.AssignLiteral("topleft");
    }

    menuPopup->SyncViewWithFrame(presContext, popupAnchor, popupAlign, this, -1, -1);
  }
}

NS_IMETHODIMP
nsMenuFrame::ShortcutNavigation(nsIDOMKeyEvent* aKeyEvent, PRBool& aHandledFlag)
{
  nsIFrame* frame = mPopupFrames.FirstChild();
  if (frame) {
    nsMenuPopupFrame* popup = (nsMenuPopupFrame*)frame;
    popup->ShortcutNavigation(aKeyEvent, aHandledFlag);
  } 

  return NS_OK;
}

NS_IMETHODIMP
nsMenuFrame::KeyboardNavigation(PRUint32 aKeyCode, PRBool& aHandledFlag)
{
  nsIFrame* frame = mPopupFrames.FirstChild();
  if (frame) {
    nsMenuPopupFrame* popup = (nsMenuPopupFrame*)frame;
    popup->KeyboardNavigation(aKeyCode, aHandledFlag);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsMenuFrame::Escape(PRBool& aHandledFlag)
{
  if (mMenuParent) {
    mMenuParent->ClearRecentlyRolledUp();
  }
  nsIFrame* frame = mPopupFrames.FirstChild();
  if (frame) {
    nsMenuPopupFrame* popup = (nsMenuPopupFrame*)frame;
    popup->Escape(aHandledFlag);
  }

  return NS_OK;
}


//
// Enter
//
// Called when the user hits the <Enter>/<Return> keys or presses the
// shortcut key. If this is a leaf item, the item's action will be executed.
// If it is a submenu parent, open the submenu and select the first time.
// In either case, do nothing if the item is disabled.
//
NS_IMETHODIMP
nsMenuFrame::Enter()
{
  if (IsDisabled()) {
#ifdef XP_WIN
    // behavior on Windows - close the popup chain
    if (mMenuParent)
      mMenuParent->DismissChain();
#endif   // #ifdef XP_WIN
    // this menu item was disabled - exit
    return NS_OK;
  }
    
  if (!mMenuOpen) {
    // The enter key press applies to us.
    if (!IsMenu() && mMenuParent)
      Execute(0);          // Execute our event handler
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
    popup->SetCurrentMenuItem(popup->GetNextMenuItem(nsnull));
  }

  return NS_OK;
}

PRBool
nsMenuFrame::IsMenu()
{
  return mIsMenu;
}

nsresult
nsMenuFrame::Notify(nsITimer* aTimer)
{
  // Our timer has fired.
  if (aTimer == mOpenTimer.get()) {
    if (!mMenuOpen && mMenuParent) {
      // make sure we didn't open a context menu in the meantime
      // (i.e. the user right-clicked while hovering over a submenu).
      // However, also make sure that we're not the context menu itself,
      // to allow context submenus to open.
      nsIMenuParent *ctxMenu = nsMenuFrame::GetContextMenu();
      PRBool parentIsContextMenu = PR_FALSE;

      if (ctxMenu)
        mMenuParent->GetIsContextMenu(parentIsContextMenu);

      if (ctxMenu == nsnull || parentIsContextMenu) {
        if (mContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::menuactive,
                                  nsGkAtoms::_true, eCaseMatters)) {
          // We're still the active menu. Make sure all submenus/timers are closed
          // before opening this one
          mMenuParent->KillPendingTimers();
          OpenMenu(PR_TRUE);
        }
      }
    }
    mOpenTimer->Cancel();
    mOpenTimer = nsnull;
  }
  
  mOpenTimer = nsnull;
  return NS_OK;
}

PRBool 
nsMenuFrame::IsDisabled()
{
  return mContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::disabled,
                               nsGkAtoms::_true, eCaseMatters);
}

void
nsMenuFrame::UpdateMenuType(nsPresContext* aPresContext)
{
  static nsIContent::AttrValuesArray strings[] =
    {&nsGkAtoms::checkbox, &nsGkAtoms::radio, nsnull};
  switch (mContent->FindAttrValueIn(kNameSpaceID_None, nsGkAtoms::type,
                                    strings, eCaseMatters)) {
    case 0: mType = eMenuType_Checkbox; break;
    case 1:
      mType = eMenuType_Radio;
      mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::name, mGroupName);
      break;

    default:
      if (mType != eMenuType_Normal) {
        nsWeakFrame weakFrame(this);
        mContent->UnsetAttr(kNameSpaceID_None, nsGkAtoms::checked,
                            PR_TRUE);
        ENSURE_TRUE(weakFrame.IsAlive());
      }
      mType = eMenuType_Normal;
      break;
  }
  UpdateMenuSpecialState(aPresContext);
}

/* update checked-ness for type="checkbox" and type="radio" */
void
nsMenuFrame::UpdateMenuSpecialState(nsPresContext* aPresContext) {
  PRBool newChecked =
    mContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::checked,
                          nsGkAtoms::_true, eCaseMatters); 

  if (newChecked == mChecked) {
    /* checked state didn't change */

    if (mType != eMenuType_Radio)
      return; // only Radio possibly cares about other kinds of change

    if (!mChecked || mGroupName.IsEmpty())
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
  nsIMenuFrame *sibMenu;
  nsMenuType sibType;
  nsAutoString sibGroup;
  PRBool sibChecked;
  
  // get the first sibling in this menu popup. This frame may be it, and if we're
  // being called at creation time, this frame isn't yet in the parent's child list.
  // All I'm saying is that this may fail, but it's most likely alright.
  nsIFrame* sib = GetParent()->GetFirstChild(nsnull);
  if ( !sib )
    return;

  // XXX - egcs 1.1.2 & gcc 2.95.x -Oy builds, where y > 1, 
  // are known to break if we declare nsCOMPtrs inside this loop.  
  // Moving the declaration out of the loop works around this problem.
  // http://bugzilla.mozilla.org/show_bug.cgi?id=80988

  do {
    if (NS_FAILED(sib->QueryInterface(NS_GET_IID(nsIMenuFrame),
                                      (void **)&sibMenu)))
        continue;
        
    if (sibMenu != (nsIMenuFrame *)this &&        // correct way to check?
        (sibMenu->GetMenuType(sibType), sibType == eMenuType_Radio) &&
        (sibMenu->MenuIsChecked(sibChecked), sibChecked) &&
        (sibMenu->GetRadioGroupName(sibGroup), sibGroup == mGroupName)) {
      
      /* uncheck the old item */
      sib->GetContent()->UnsetAttr(kNameSpaceID_None, nsGkAtoms::checked,
                                   PR_TRUE);

      /* XXX in DEBUG, check to make sure that there aren't two checked items */
      return;
    }

  } while ((sib = sib->GetNextSibling()) != nsnull);

}

void 
nsMenuFrame::BuildAcceleratorText()
{
  nsAutoString accelText;

  if ((GetStateBits() & NS_STATE_ACCELTEXT_IS_DERIVED) == 0) {
    mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::acceltext, accelText);
    if (!accelText.IsEmpty())
      return;
  }
  // accelText is definitely empty here.

  // Now we're going to compute the accelerator text, so remember that we did.
  AddStateBits(NS_STATE_ACCELTEXT_IS_DERIVED);

  // If anything below fails, just leave the accelerator text blank.
  nsWeakFrame weakFrame(this);
  mContent->UnsetAttr(kNameSpaceID_None, nsGkAtoms::acceltext, PR_FALSE);
  ENSURE_TRUE(weakFrame.IsAlive());

  // See if we have a key node and use that instead.
  nsAutoString keyValue;
  mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::key, keyValue);
  if (keyValue.IsEmpty())
    return;

  // Turn the document into a DOM document so we can use getElementById
  nsCOMPtr<nsIDOMDocument> domDocument(do_QueryInterface(mContent->GetDocument()));
  if (!domDocument)
    return;

  nsCOMPtr<nsIDOMElement> keyDOMElement;
  domDocument->GetElementById(keyValue, getter_AddRefs(keyDOMElement));
  if (!keyDOMElement) {
#ifdef DEBUG
    nsAutoString label;
    mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::label, label);
    nsAutoString msg = NS_LITERAL_STRING("Key '") +
                       keyValue +
                       NS_LITERAL_STRING("' of menu item '") +
                       label +
                       NS_LITERAL_STRING("' could not be found");
    NS_WARNING(NS_ConvertUTF16toUTF8(msg).get());
#endif
    return;
  }

  nsCOMPtr<nsIContent> keyElement(do_QueryInterface(keyDOMElement));
  if (!keyElement)
    return;

  // get the string to display as accelerator text
  // check the key element's attributes in this order:
  // |keytext|, |key|, |keycode|
  nsAutoString accelString;
  keyElement->GetAttr(kNameSpaceID_None, nsGkAtoms::keytext, accelString);

  if (accelString.IsEmpty()) {
    keyElement->GetAttr(kNameSpaceID_None, nsGkAtoms::key, accelString);

    if (!accelString.IsEmpty()) {
      ToUpperCase(accelString);
    } else {
      nsAutoString keyCode;
      keyElement->GetAttr(kNameSpaceID_None, nsGkAtoms::keycode, keyCode);
      ToUpperCase(keyCode);

      nsresult rv;
      nsCOMPtr<nsIStringBundleService> bundleService(do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv));
      if (NS_SUCCEEDED(rv) && bundleService) {
        nsCOMPtr<nsIStringBundle> bundle;
        rv = bundleService->CreateBundle("chrome://global/locale/keys.properties",
                                         getter_AddRefs(bundle));

        if (NS_SUCCEEDED(rv) && bundle) {
          nsXPIDLString keyName;
          rv = bundle->GetStringFromName(keyCode.get(), getter_Copies(keyName));
          if (keyName)
            accelString = keyName;
        }
      }

      // nothing usable found, bail
      if (accelString.IsEmpty())
        return;
    }
  }

  static PRInt32 accelKey = 0;

  if (!accelKey)
  {
    // Compiled-in defaults, in case we can't get LookAndFeel --
    // command for mac, control for all other platforms.
#if defined(XP_MAC) || defined(XP_MACOSX)
    accelKey = nsIDOMKeyEvent::DOM_VK_META;
#else
    accelKey = nsIDOMKeyEvent::DOM_VK_CONTROL;
#endif

    // Get the accelerator key value from prefs, overriding the default:
    accelKey = nsContentUtils::GetIntPref("ui.key.accelKey", accelKey);
  }

  nsAutoString modifiers;
  keyElement->GetAttr(kNameSpaceID_None, nsGkAtoms::modifiers, modifiers);
  
  char* str = ToNewCString(modifiers);
  char* newStr;
  char* token = nsCRT::strtok(str, ", \t", &newStr);
  while (token) {
      
    if (PL_strcmp(token, "shift") == 0)
      accelText += *gShiftText;
    else if (PL_strcmp(token, "alt") == 0) 
      accelText += *gAltText; 
    else if (PL_strcmp(token, "meta") == 0) 
      accelText += *gMetaText; 
    else if (PL_strcmp(token, "control") == 0) 
      accelText += *gControlText; 
    else if (PL_strcmp(token, "accel") == 0) {
      switch (accelKey)
      {
        case nsIDOMKeyEvent::DOM_VK_META:
          accelText += *gMetaText;
          break;

        case nsIDOMKeyEvent::DOM_VK_ALT:
          accelText += *gAltText;
          break;

        case nsIDOMKeyEvent::DOM_VK_CONTROL:
        default:
          accelText += *gControlText;
          break;
      }
    }
    
    accelText += *gModifierSeparator;

    token = nsCRT::strtok(newStr, ", \t", &newStr);
  }

  nsMemory::Free(str);

  accelText += accelString;
  
  mContent->SetAttr(kNameSpaceID_None, nsGkAtoms::acceltext, accelText, PR_FALSE);
}

void
nsMenuFrame::Execute(nsGUIEvent *aEvent)
{
  nsWeakFrame weakFrame(this);
  // flip "checked" state if we're a checkbox menu, or an un-checked radio menu
  if (mType == eMenuType_Checkbox || (mType == eMenuType_Radio && !mChecked)) {
    if (!mContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::autocheck,
                               nsGkAtoms::_false, eCaseMatters)) {
      if (mChecked) {
        mContent->UnsetAttr(kNameSpaceID_None, nsGkAtoms::checked,
                            PR_TRUE);
        ENSURE_TRUE(weakFrame.IsAlive());
      }
      else {
        mContent->SetAttr(kNameSpaceID_None, nsGkAtoms::checked, NS_LITERAL_STRING("true"),
                          PR_TRUE);
        ENSURE_TRUE(weakFrame.IsAlive());
      }        
      /* the AttributeChanged code will update all the internal state */
    }
  }

  // Temporarily disable rollup events on this menu.  This is
  // to suppress this menu getting removed in the case where
  // the oncommand handler opens a dialog, etc.
  if ( nsMenuDismissalListener::sInstance ) {
    nsMenuDismissalListener::sInstance->EnableListener(PR_FALSE);
  }

  // Get our own content node and hold on to it to keep it from going away.
  nsCOMPtr<nsIContent> content = mContent;

  // Deselect ourselves.
  SelectMenu(PR_FALSE);
  ENSURE_TRUE(weakFrame.IsAlive());

  // Now hide all of the open menus.
  if (mMenuParent) {
    mMenuParent->HideChain();

    // Since menu was not dismissed via click outside menu
    // we don't want to keep track of this rollup.
    // Otherwise, we keep track so that the same click 
    // won't both dismiss and then reopen a menu.
    mMenuParent->ClearRecentlyRolledUp();
  }


  nsEventStatus status = nsEventStatus_eIgnore;
  // Create a trusted event if the triggering event was trusted, or if
  // we're called from chrome code (since at least one of our caller
  // passes in a null event).
  nsXULCommandEvent event(aEvent ? NS_IS_TRUSTED_EVENT(aEvent) :
                          nsContentUtils::IsCallerChrome(),
                          NS_XUL_COMMAND, nsnull);
  if (aEvent && (aEvent->eventStructType == NS_MOUSE_EVENT ||
                 aEvent->eventStructType == NS_KEY_EVENT ||
                 aEvent->eventStructType == NS_ACCESSIBLE_EVENT)) {

    event.isShift = NS_STATIC_CAST(nsInputEvent *, aEvent)->isShift;
    event.isControl = NS_STATIC_CAST(nsInputEvent *, aEvent)->isControl;
    event.isAlt = NS_STATIC_CAST(nsInputEvent *, aEvent)->isAlt;
    event.isMeta = NS_STATIC_CAST(nsInputEvent *, aEvent)->isMeta;
  }

  // The order of the nsIViewManager and nsIPresShell COM pointers is
  // important below.  We want the pres shell to get released before the
  // associated view manager on exit from this function.
  // See bug 54233.
  nsPresContext* presContext = GetPresContext();
  nsCOMPtr<nsIViewManager> kungFuDeathGrip = presContext->GetViewManager();
  nsCOMPtr<nsIPresShell> shell = presContext->GetPresShell();
  if (shell) {
    shell->HandleDOMEventWithTarget(mContent, &event, &status);
    ENSURE_TRUE(weakFrame.IsAlive());
  }

  if (mMenuParent) {
    mMenuParent->DismissChain();
  }

  // Re-enable rollup events on this menu.
  if ( nsMenuDismissalListener::sInstance ) {
    nsMenuDismissalListener::sInstance->EnableListener(PR_TRUE);
  }
}

PRBool
nsMenuFrame::OnCreate()
{
  nsEventStatus status = nsEventStatus_eIgnore;
  nsMouseEvent event(PR_TRUE, NS_XUL_POPUP_SHOWING, nsnull,
                     nsMouseEvent::eReal);

  nsCOMPtr<nsIContent> child;
  GetMenuChildrenElement(getter_AddRefs(child));
  
  nsresult rv = NS_OK;

  nsCOMPtr<nsIPresShell> shell = GetPresContext()->GetPresShell();
  if (shell) {
    if (child) {
      rv = shell->HandleDOMEventWithTarget(child, &event, &status);
    }
    else {
      rv = shell->HandleDOMEventWithTarget(mContent, &event, &status);
    }
  }

  if ( NS_FAILED(rv) || status == nsEventStatus_eConsumeNoDefault )
    return PR_FALSE;

  // The menu is going to show, and the create handler has executed.
  // We should now walk all of our menu item children, checking to see if any
  // of them has a command attribute.  If so, then several attributes must
  // potentially be updated.
  if (child) {
    nsCOMPtr<nsIDOMDocument> domDoc(do_QueryInterface(child->GetDocument()));

    PRUint32 count = child->GetChildCount();
    for (PRUint32 i = 0; i < count; i++) {
      nsCOMPtr<nsIContent> grandChild = child->GetChildAt(i);

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
            nsAutoString commandAttr;
            // The menu's disabled state needs to be updated to match the command.
            if (commandContent->GetAttr(kNameSpaceID_None, nsGkAtoms::disabled, commandAttr))
              grandChild->SetAttr(kNameSpaceID_None, nsGkAtoms::disabled, commandAttr, PR_TRUE);
            else
              grandChild->UnsetAttr(kNameSpaceID_None, nsGkAtoms::disabled, PR_TRUE);

            // The menu's label, accesskey and checked states need to be updated
            // to match the command. Note that unlike the disabled state if the
            // command has *no* value, we assume the menu is supplying its own.
            if (commandContent->GetAttr(kNameSpaceID_None, nsGkAtoms::checked, commandAttr))
              grandChild->SetAttr(kNameSpaceID_None, nsGkAtoms::checked, commandAttr, PR_TRUE);

            if (commandContent->GetAttr(kNameSpaceID_None, nsGkAtoms::accesskey, commandAttr))
              grandChild->SetAttr(kNameSpaceID_None, nsGkAtoms::accesskey, commandAttr, PR_TRUE);

            if (commandContent->GetAttr(kNameSpaceID_None, nsGkAtoms::label, commandAttr))
              grandChild->SetAttr(kNameSpaceID_None, nsGkAtoms::label, commandAttr, PR_TRUE);
          }
        }
      }
    }
  }

  return PR_TRUE;
}

PRBool
nsMenuFrame::OnCreated()
{
  nsEventStatus status = nsEventStatus_eIgnore;
  nsMouseEvent event(PR_TRUE, NS_XUL_POPUP_SHOWN, nsnull,
                     nsMouseEvent::eReal);

  nsCOMPtr<nsIContent> child;
  GetMenuChildrenElement(getter_AddRefs(child));
  
  nsresult rv = NS_OK;
  nsCOMPtr<nsIPresShell> shell = GetPresContext()->GetPresShell();
  if (shell) {
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
  nsMouseEvent event(PR_TRUE, NS_XUL_POPUP_HIDING, nsnull,
                     nsMouseEvent::eReal);

  nsCOMPtr<nsIContent> child;
  GetMenuChildrenElement(getter_AddRefs(child));
  
  nsresult rv = NS_OK;
  nsCOMPtr<nsIPresShell> shell = GetPresContext()->GetPresShell();
  if (shell) {
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
nsMenuFrame::OnDestroyed()
{
  nsEventStatus status = nsEventStatus_eIgnore;
  nsMouseEvent event(PR_TRUE, NS_XUL_POPUP_HIDDEN, nsnull,
                     nsMouseEvent::eReal);

  nsCOMPtr<nsIContent> child;
  GetMenuChildrenElement(getter_AddRefs(child));
  
  nsresult rv = NS_OK;
  nsCOMPtr<nsIPresShell> shell = GetPresContext()->GetPresShell();
  if (shell) {
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
nsMenuFrame::RemoveFrame(nsIAtom*        aListName,
                         nsIFrame*       aOldFrame)
{
  nsresult  rv;

  if (mPopupFrames.ContainsFrame(aOldFrame)) {
    // Go ahead and remove this frame.
    mPopupFrames.DestroyFrame(aOldFrame);
    AddStateBits(NS_FRAME_HAS_DIRTY_CHILDREN);
    GetPresContext()->PresShell()->
      FrameNeedsReflow(this, nsIPresShell::eTreeChange);
    rv = NS_OK;
  } else {
    rv = nsBoxFrame::RemoveFrame(aListName, aOldFrame);
  }

  return rv;
}

NS_IMETHODIMP
nsMenuFrame::InsertFrames(nsIAtom*        aListName,
                          nsIFrame*       aPrevFrame,
                          nsIFrame*       aFrameList)
{
  nsresult          rv;

  nsIMenuParent *menuPar;
  if (aFrameList && NS_SUCCEEDED(CallQueryInterface(aFrameList, &menuPar))) {
    NS_ASSERTION(aFrameList->IsBoxFrame(),"Popup is not a box!!!");
    mPopupFrames.InsertFrames(nsnull, nsnull, aFrameList);

#ifdef DEBUG_LAYOUT
    nsBoxLayoutState state(GetPresContext());
    SetDebug(state, aFrameList, mState & NS_STATE_CURRENTLY_IN_DEBUG);
#endif
    AddStateBits(NS_FRAME_HAS_DIRTY_CHILDREN);
    GetPresContext()->PresShell()->
      FrameNeedsReflow(this, nsIPresShell::eTreeChange);
    rv = NS_OK;
  } else {
    rv = nsBoxFrame::InsertFrames(aListName, aPrevFrame, aFrameList);  
  }

  return rv;
}

NS_IMETHODIMP
nsMenuFrame::AppendFrames(nsIAtom*        aListName,
                          nsIFrame*       aFrameList)
{
  if (!aFrameList)
    return NS_OK;

  nsresult          rv;

  nsIMenuParent *menuPar;
  if (aFrameList && NS_SUCCEEDED(CallQueryInterface(aFrameList, &menuPar))) {
    NS_ASSERTION(aFrameList->IsBoxFrame(),"Popup is not a box!!!");

    mPopupFrames.AppendFrames(nsnull, aFrameList);
#ifdef DEBUG_LAYOUT
    nsBoxLayoutState state(GetPresContext());
    SetDebug(state, aFrameList, mState & NS_STATE_CURRENTLY_IN_DEBUG);
#endif
    AddStateBits(NS_FRAME_HAS_DIRTY_CHILDREN);
    GetPresContext()->PresShell()->
      FrameNeedsReflow(this, nsIPresShell::eTreeChange);
    rv = NS_OK;
  } else {
    rv = nsBoxFrame::AppendFrames(aListName, aFrameList); 
  }

  return rv;
}

class nsASyncMenuGeneration : public nsIReflowCallback
{
public:
  nsASyncMenuGeneration(nsIFrame* aFrame)
    : mWeakFrame(aFrame)
  {
    nsIContent* content = aFrame ? aFrame->GetContent() : nsnull;
    mDocument = content ? content->GetCurrentDoc() : nsnull;
    if (mDocument) {
      mDocument->BlockOnload();
    }
  }

  virtual PRBool ReflowFinished() {
    PRBool shouldFlush = PR_FALSE;
    nsIFrame* frame = mWeakFrame.GetFrame();
    if (frame) {
      nsBoxLayoutState state(frame->GetPresContext());
      if (!frame->IsCollapsed(state)) {
        nsIMenuFrame* imenu = nsnull;
        CallQueryInterface(frame, &imenu);
        if (imenu) {
          imenu->MarkAsGenerated();
          shouldFlush = PR_TRUE;
        }
      }
    }
    if (mDocument) {
      mDocument->UnblockOnload(PR_FALSE);
    }
    delete this;
    return shouldFlush;
  }

  nsWeakFrame           mWeakFrame;
  nsCOMPtr<nsIDocument> mDocument;
};

PRBool
nsMenuFrame::SizeToPopup(nsBoxLayoutState& aState, nsSize& aSize)
{
  if (!IsCollapsed(aState)) {
    nsSize tmpSize(-1, 0);
    nsIBox::AddCSSPrefSize(aState, this, tmpSize);
    if (tmpSize.width == -1 && GetFlex(aState) == 0) {
      nsIFrame* frame = mPopupFrames.FirstChild();
      if (!frame) {
        nsCOMPtr<nsIContent> child;
        GetMenuChildrenElement(getter_AddRefs(child));
        if (child &&
            !nsContentUtils::HasNonEmptyAttr(child, kNameSpaceID_None,
                                             nsGkAtoms::menugenerated)) {
          nsIReflowCallback* cb = new nsASyncMenuGeneration(this);
          if (cb) {
            GetPresContext()->PresShell()->PostReflowCallback(cb);
          }
        }
        return PR_FALSE;
      }

      NS_ASSERTION(frame->IsBoxFrame(), "popupChild is not box!!");

      tmpSize = frame->GetPrefSize(aState);
      aSize.width = tmpSize.width;
      return PR_TRUE;
    }
  }

  return PR_FALSE;
}

nsSize
nsMenuFrame::GetPrefSize(nsBoxLayoutState& aState)
{
  nsSize size = nsBoxFrame::GetPrefSize(aState);
  DISPLAY_PREF_SIZE(this, size);

  // If we are using sizetopopup="always" then
  // nsBoxFrame will already have enforced the minimum size
  if (!IsSizedToPopup(mContent, PR_TRUE) &&
      IsSizedToPopup(mContent, PR_FALSE) &&
      SizeToPopup(aState, size)) {
    // We now need to ensure that size is within the min - max range.
    nsSize minSize = nsBoxFrame::GetMinSize(aState);
    nsSize maxSize = GetMaxSize(aState);
    BoundsCheck(minSize, size, maxSize);
  }

  return size;
}

NS_IMETHODIMP
nsMenuFrame::GetActiveChild(nsIDOMElement** aResult)
{
  nsIFrame* frame = mPopupFrames.FirstChild();
  nsMenuPopupFrame* menuPopup = (nsMenuPopupFrame*)frame;
  if (!frame)
    return NS_ERROR_FAILURE;

  nsIMenuFrame* menuFrame = menuPopup->GetCurrentMenuItem();
  
  if (!menuFrame) {
    *aResult = nsnull;
  }
  else {
    nsIFrame* f;
    menuFrame->QueryInterface(NS_GET_IID(nsIFrame), (void**)&f);
    nsCOMPtr<nsIDOMElement> elt(do_QueryInterface(f->GetContent()));
    *aResult = elt;
    NS_IF_ADDREF(*aResult);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsMenuFrame::SetActiveChild(nsIDOMElement* aChild)
{
  nsIFrame* frame = mPopupFrames.FirstChild();
  nsMenuPopupFrame* menuPopup = (nsMenuPopupFrame*)frame;
  if (!frame)
    return NS_ERROR_FAILURE;

  if (!aChild) {
    // Remove the current selection
    menuPopup->SetCurrentMenuItem(nsnull);
    return NS_OK;
  }

  nsCOMPtr<nsIContent> child(do_QueryInterface(aChild));
  
  nsIFrame* kid = GetPresContext()->PresShell()->GetPrimaryFrameFor(child);
  if (!kid)
    return NS_ERROR_FAILURE;
  nsIMenuFrame *menuFrame;
  nsresult rv = CallQueryInterface(kid, &menuFrame);
  if (NS_FAILED(rv))
    return rv;
  menuPopup->SetCurrentMenuItem(menuFrame);
  return NS_OK;
}

nsIScrollableView* nsMenuFrame::GetScrollableView()
{
  if (!mPopupFrames.FirstChild())
    return nsnull;

  nsMenuPopupFrame* popup = (nsMenuPopupFrame*) mPopupFrames.FirstChild();
  nsIFrame* childFrame = popup->GetFirstChild(nsnull);
  if (childFrame) {
    return popup->GetScrollableView(childFrame);
  }
  return nsnull;
}

/* Need to figure out what this does.
NS_IMETHODIMP
nsMenuFrame::GetBoxInfo(nsPresContext* aPresContext, const nsHTMLReflowState& aReflowState, nsBoxInfo& aSize)
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
      
      nsCalculatedBoxInfo childInfo(frame);
      frame->GetBoxInfo(aPresContext, aReflowState, childInfo);
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

nsIMenuParent*
nsMenuFrame::GetContextMenu()
{
  if (!nsMenuDismissalListener::sInstance)
    return nsnull;

  nsIMenuParent *menuParent =
    nsMenuDismissalListener::sInstance->GetCurrentMenuParent();
  if (!menuParent)
    return nsnull;

  PRBool isContextMenu;
  menuParent->GetIsContextMenu(isContextMenu);
  if (isContextMenu)
    return menuParent;

  return nsnull;
}

// nsMenuTimerMediator implementation.
NS_IMPL_ISUPPORTS1(nsMenuTimerMediator, nsITimerCallback)

/**
 * Constructs a wrapper around an nsMenuFrame.
 * @param aFrame nsMenuFrame to create a wrapper around.
 */
nsMenuTimerMediator::nsMenuTimerMediator(nsMenuFrame *aFrame) :
  mFrame(aFrame)
{
  NS_ASSERTION(mFrame, "Must have frame");
}

nsMenuTimerMediator::~nsMenuTimerMediator()
{
}

/**
 * Delegates the notification to the contained frame if it has not been destroyed.
 * @param aTimer Timer which initiated the callback.
 * @return NS_ERROR_FAILURE if the frame has been destroyed.
 */
NS_IMETHODIMP nsMenuTimerMediator::Notify(nsITimer* aTimer)
{
  if (!mFrame)
    return NS_ERROR_FAILURE;

  return mFrame->Notify(aTimer);
}

/**
 * Clear the pointer to the contained nsMenuFrame. This should be called
 * when the contained nsMenuFrame is destroyed.
 */
void nsMenuTimerMediator::ClearFrame()
{
  mFrame = nsnull;
}
