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

//
// nsMenuFrame
//

#ifndef nsMenuFrame_h__
#define nsMenuFrame_h__

#include "prtypes.h"
#include "nsIAtom.h"
#include "nsCOMPtr.h"

#include "nsBoxFrame.h"
#include "nsFrameList.h"
#include "nsGkAtoms.h"
#include "nsMenuParent.h"
#include "nsXULPopupManager.h"
#include "nsITimer.h"
#include "nsIContent.h"

nsIFrame* NS_NewMenuFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
nsIFrame* NS_NewMenuItemFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);

class nsMenuBarFrame;

#define NS_STATE_ACCELTEXT_IS_DERIVED  NS_STATE_BOX_CHILD_RESERVED

// the type of menuitem
enum nsMenuType {
  // a normal menuitem where a command is carried out when activated
  eMenuType_Normal = 0,
  // a menuitem with a checkmark that toggles when activated
  eMenuType_Checkbox = 1,
  // a radio menuitem where only one of it and its siblings with the same
  // name attribute can be checked at a time
  eMenuType_Radio = 2
};

enum nsMenuListType {
  eNotMenuList,      // not a menulist
  eReadonlyMenuList, // <menulist/>
  eEditableMenuList  // <menulist editable="true"/>
};

class nsMenuFrame;

/**
 * nsMenuTimerMediator is a wrapper around an nsMenuFrame which can be safely
 * passed to timers. The class is reference counted unlike the underlying
 * nsMenuFrame, so that it will exist as long as the timer holds a reference
 * to it. The callback is delegated to the contained nsMenuFrame as long as
 * the contained nsMenuFrame has not been destroyed.
 */
class nsMenuTimerMediator : public nsITimerCallback
{
public:
  nsMenuTimerMediator(nsMenuFrame* aFrame);
  ~nsMenuTimerMediator();

  NS_DECL_ISUPPORTS
  NS_DECL_NSITIMERCALLBACK

  void ClearFrame();

private:

  // Pointer to the wrapped frame.
  nsMenuFrame* mFrame;
};

class nsMenuFrame : public nsBoxFrame
{
public:
  nsMenuFrame(nsIPresShell* aShell, nsStyleContext* aContext);

  NS_DECL_QUERYFRAME_TARGET(nsMenuFrame)
  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS

  // nsIBox
  NS_IMETHOD DoLayout(nsBoxLayoutState& aBoxLayoutState);
  virtual nsSize GetMinSize(nsBoxLayoutState& aBoxLayoutState);
  virtual nsSize GetPrefSize(nsBoxLayoutState& aBoxLayoutState);

  NS_IMETHOD Init(nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsIFrame*        aPrevInFlow);

#ifdef DEBUG_LAYOUT
  NS_IMETHOD SetDebug(nsBoxLayoutState& aState, bool aDebug);
#endif

  // The following methods are all overridden so that the menupopup
  // can be stored in a separate list, so that it doesn't impact reflow of the
  // actual menu item at all.
  virtual nsFrameList GetChildList(ChildListID aList) const;
  virtual void GetChildLists(nsTArray<ChildList>* aLists) const;
  NS_IMETHOD SetInitialChildList(ChildListID     aListID,
                                 nsFrameList&    aChildList);
  virtual void DestroyFrom(nsIFrame* aDestructRoot);

  // Overridden to prevent events from going to children of the menu.
  NS_IMETHOD BuildDisplayListForChildren(nsDisplayListBuilder*   aBuilder,
                                         const nsRect&           aDirtyRect,
                                         const nsDisplayListSet& aLists);
                                         
  // this method can destroy the frame
  NS_IMETHOD HandleEvent(nsPresContext* aPresContext, 
                         nsGUIEvent*     aEvent,
                         nsEventStatus*  aEventStatus);

  NS_IMETHOD  AppendFrames(ChildListID     aListID,
                           nsFrameList&    aFrameList);

  NS_IMETHOD  InsertFrames(ChildListID     aListID,
                           nsIFrame*       aPrevFrame,
                           nsFrameList&    aFrameList);

  NS_IMETHOD  RemoveFrame(ChildListID     aListID,
                          nsIFrame*       aOldFrame);

  virtual nsIAtom* GetType() const { return nsGkAtoms::menuFrame; }

  NS_IMETHOD SelectMenu(bool aActivateFlag);

  virtual nsIScrollableFrame* GetScrollTargetFrame();

  /**
   * NOTE: OpenMenu will open the menu asynchronously.
   */
  void OpenMenu(bool aSelectFirstItem);
  // CloseMenu closes the menu asynchronously
  void CloseMenu(bool aDeselectMenu);

  bool IsChecked() { return mChecked; }

  NS_IMETHOD GetActiveChild(nsIDOMElement** aResult);
  NS_IMETHOD SetActiveChild(nsIDOMElement* aChild);

  // called when the Enter key is pressed while the menuitem is the current
  // one in its parent popup. This will carry out the command attached to
  // the menuitem. If the menu should be opened, this frame will be returned,
  // otherwise null will be returned.
  nsMenuFrame* Enter(nsGUIEvent* aEvent);

  virtual void SetParent(nsIFrame* aParent);

  virtual nsMenuParent *GetMenuParent() { return mMenuParent; }
  const nsAString& GetRadioGroupName() { return mGroupName; }
  nsMenuType GetMenuType() { return mType; }
  nsMenuPopupFrame* GetPopup() { return mPopupFrame; }

  // nsMenuFrame methods 

  bool IsOnMenuBar() { return mMenuParent && mMenuParent->IsMenuBar(); }
  bool IsOnActiveMenuBar() { return IsOnMenuBar() && mMenuParent->IsActive(); }
  virtual bool IsOpen();
  virtual bool IsMenu();
  nsMenuListType GetParentMenuListType();
  bool IsDisabled();
  void ToggleMenuState();

  // indiciate that the menu's popup has just been opened, so that the menu
  // can update its open state. This method modifies the open attribute on
  // the menu, so the frames could be gone after this call.
  void PopupOpened();
  // indiciate that the menu's popup has just been closed, so that the menu
  // can update its open state. The menu should be unhighlighted if
  // aDeselectedMenu is true. This method modifies the open attribute on
  // the menu, so the frames could be gone after this call.
  void PopupClosed(bool aDeselectMenu);

  // returns true if this is a menu on another menu popup. A menu is a submenu
  // if it has a parent popup or menupopup.
  bool IsOnMenu() { return mMenuParent && mMenuParent->IsMenu(); }
  void SetIsMenu(bool aIsMenu) { mIsMenu = aIsMenu; }

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const
  {
      return MakeFrameName(NS_LITERAL_STRING("Menu"), aResult);
  }
#endif

  static bool IsSizedToPopup(nsIContent* aContent, bool aRequireAlways);

protected:
  friend class nsMenuTimerMediator;
  friend class nsASyncMenuInitialization;
  friend class nsMenuAttributeChangedEvent;

  // initialize mPopupFrame to the first popup frame within
  // aChildList. Removes the popup, if any, from aChildList.
  void SetPopupFrame(nsFrameList& aChildList);

  // set mMenuParent to the nearest enclosing menu bar or menupopup frame of
  // aParent (or aParent itself). This is called when initializing the frame,
  // so aParent should be the expected parent of this frame.
  void InitMenuParent(nsIFrame* aParent);

  // Update the menu's type (normal, checkbox, radio).
  // This method can destroy the frame.
  void UpdateMenuType(nsPresContext* aPresContext);
  // Update the checked state of the menu, and for radios, clear any other
  // checked items. This method can destroy the frame.
  void UpdateMenuSpecialState(nsPresContext* aPresContext);

  // Examines the key node and builds the accelerator.
  void BuildAcceleratorText(bool aNotify);

  // Called to execute our command handler. This method can destroy the frame.
  void Execute(nsGUIEvent *aEvent);

  // This method can destroy the frame
  NS_IMETHOD AttributeChanged(PRInt32 aNameSpaceID,
                              nsIAtom* aAttribute,
                              PRInt32 aModType);
  virtual ~nsMenuFrame() { };

  bool SizeToPopup(nsBoxLayoutState& aState, nsSize& aSize);

  bool ShouldBlink();
  void StartBlinking(nsGUIEvent *aEvent, bool aFlipChecked);
  void StopBlinking();
  void CreateMenuCommandEvent(nsGUIEvent *aEvent, bool aFlipChecked);
  void PassMenuCommandEventToPopupManager();

protected:
#ifdef DEBUG_LAYOUT
  nsresult SetDebug(nsBoxLayoutState& aState, nsIFrame* aList, bool aDebug);
#endif
  NS_HIDDEN_(nsresult) Notify(nsITimer* aTimer);

  bool mIsMenu; // Whether or not we can even have children or not.
  bool mChecked;              // are we checked?
  bool mIgnoreAccelTextChange; // temporarily set while determining the accelerator key
  nsMenuType mType;

  nsMenuParent* mMenuParent; // Our parent menu.

  // the popup for this menu, owned
  nsMenuPopupFrame* mPopupFrame;

  // Reference to the mediator which wraps this frame.
  nsRefPtr<nsMenuTimerMediator> mTimerMediator;

  nsCOMPtr<nsITimer> mOpenTimer;
  nsCOMPtr<nsITimer> mBlinkTimer;

  PRUint8 mBlinkState; // 0: not blinking, 1: off, 2: on
  nsRefPtr<nsXULMenuCommandEvent> mDelayedMenuCommandEvent;

  nsString mGroupName;

}; // class nsMenuFrame

#endif
