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
#include "nsIMenuParent.h"
#include "nsIMenuFrame.h"
#include "nsMenuDismissalListener.h"
#include "nsITimer.h"
#include "nsISupportsArray.h"
#include "nsIDOMText.h"
#include "nsIContent.h"
#include "nsIScrollableViewProvider.h"

nsIFrame* NS_NewMenuFrame(nsIPresShell* aPresShell, nsStyleContext* aContext, PRUint32 aFlags);

class nsMenuBarFrame;
class nsMenuPopupFrame;
class nsIScrollableView;

#define NS_STATE_ACCELTEXT_IS_DERIVED  NS_STATE_BOX_CHILD_RESERVED

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

/**
 * @note *** Methods marked with '@see comment ***' may cause the frame to be
 *           deleted during the method call. Be careful whenever using those
 *           methods.
 */

class nsMenuFrame : public nsBoxFrame, 
                    public nsIMenuFrame,
                    public nsIScrollableViewProvider
{
public:
  nsMenuFrame(nsIPresShell* aShell, nsStyleContext* aContext);

  NS_DECL_ISUPPORTS

  // nsIBox
  NS_IMETHOD DoLayout(nsBoxLayoutState& aBoxLayoutState);
  virtual nsSize GetMinSize(nsBoxLayoutState& aBoxLayoutState);
  virtual nsSize GetPrefSize(nsBoxLayoutState& aBoxLayoutState);

  NS_IMETHOD Init(nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsIFrame*        aPrevInFlow);

#ifdef DEBUG_LAYOUT
  NS_IMETHOD SetDebug(nsBoxLayoutState& aState, PRBool aDebug);
#endif

  NS_IMETHOD IsActive(PRBool& aResult) { aResult = PR_TRUE; return NS_OK; }

  // The following four methods are all overridden so that the menu children
  // can be stored in a separate list (so that they don't impact reflow of the
  // actual menu item at all).
  virtual nsIFrame* GetFirstChild(nsIAtom* aListName) const;
  NS_IMETHOD SetInitialChildList(nsIAtom*        aListName,
                                 nsIFrame*       aChildList);
  virtual nsIAtom* GetAdditionalChildListName(PRInt32 aIndex) const;
  virtual void Destroy(); // @see comment ***

  // Overridden to prevent events from going to children of the menu.
  NS_IMETHOD BuildDisplayListForChildren(nsDisplayListBuilder*   aBuilder,
                                         const nsRect&           aDirtyRect,
                                         const nsDisplayListSet& aLists);
                                         
  NS_IMETHOD HandleEvent(nsPresContext* aPresContext, 
                         nsGUIEvent*     aEvent,
                         nsEventStatus*  aEventStatus); // @see comment ***

  NS_IMETHOD  AppendFrames(nsIAtom*        aListName,
                           nsIFrame*       aFrameList);

  NS_IMETHOD  InsertFrames(nsIAtom*        aListName,
                           nsIFrame*       aPrevFrame,
                           nsIFrame*       aFrameList);

  NS_IMETHOD  RemoveFrame(nsIAtom*        aListName,
                          nsIFrame*       aOldFrame);

  // nsIMenuFrame Interface

  NS_IMETHOD ActivateMenu(PRBool aActivateFlag); // @see comment ***
  NS_IMETHOD SelectMenu(PRBool aActivateFlag); // @see comment ***
  NS_IMETHOD OpenMenu(PRBool aActivateFlag); // @see comment ***

  NS_IMETHOD MenuIsOpen(PRBool& aResult) { aResult = IsOpen(); return NS_OK; }
  NS_IMETHOD MenuIsContainer(PRBool& aResult) { aResult = IsMenu(); return NS_OK; }
  NS_IMETHOD MenuIsChecked(PRBool& aResult) { aResult = mChecked; return NS_OK; }
  NS_IMETHOD MenuIsDisabled(PRBool& aResult) { aResult = IsDisabled(); return NS_OK; }
  
  NS_IMETHOD GetActiveChild(nsIDOMElement** aResult);
  NS_IMETHOD SetActiveChild(nsIDOMElement* aChild); // @see comment ***

  NS_IMETHOD UngenerateMenu(); // @see comment ***

  NS_IMETHOD SelectFirstItem(); // @see comment ***

  NS_IMETHOD Escape(PRBool& aHandledFlag); // @see comment ***
  NS_IMETHOD Enter(); // @see comment ***
  NS_IMETHOD ShortcutNavigation(nsIDOMKeyEvent* aKeyEvent, PRBool& aHandledFlag); // @see comment ***
  NS_IMETHOD KeyboardNavigation(PRUint32 aKeyCode, PRBool& aHandledFlag); // @see comment ***

  NS_IMETHOD SetParent(const nsIFrame* aParent);

  virtual nsIMenuParent *GetMenuParent() { return mMenuParent; }
  virtual nsIFrame *GetMenuChild() { return mPopupFrames.FirstChild(); }
  NS_IMETHOD GetRadioGroupName(nsString &aName) { aName = mGroupName; return NS_OK; }
  NS_IMETHOD GetMenuType(nsMenuType &aType) { aType = mType; return NS_OK; }
  NS_IMETHOD MarkAsGenerated();

  // nsIScrollableViewProvider methods

  virtual nsIScrollableView* GetScrollableView();

  // nsMenuFrame methods 

  nsresult DestroyPopupFrames(nsPresContext* aPresContext);

  PRBool IsOpen() { return mMenuOpen; }
  PRBool IsMenu();
  PRBool IsDisabled();
  PRBool IsGenerated();
  NS_IMETHOD ToggleMenuState(); // @see comment ***

  void SetIsMenu(PRBool aIsMenu) { mIsMenu = aIsMenu; }

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const
  {
      return MakeFrameName(NS_LITERAL_STRING("Menu"), aResult);
  }
#endif

  static PRBool IsSizedToPopup(nsIContent* aContent, PRBool aRequireAlways);

  static nsIMenuParent *GetContextMenu();

protected:
  friend class nsMenuTimerMediator;
  
  virtual void RePositionPopup(nsBoxLayoutState& aState);

  void
  ConvertPosition(nsIContent* aPopupElt, nsString& aAnchor, nsString& aAlign);

  friend class nsASyncMenuInitialization;
  void UpdateMenuType(nsPresContext* aPresContext); // @see comment ***
  void UpdateMenuSpecialState(nsPresContext* aPresContext); // @see comment ***

  void OpenMenuInternal(PRBool aActivateFlag); // @see comment ***
  void GetMenuChildrenElement(nsIContent** aResult);

  // Examines the key node and builds the accelerator.
  void BuildAcceleratorText();

  // Called to execute our command handler.
  void Execute(nsGUIEvent *aEvent); // @see comment ***

  // Called as a hook just before the menu gets opened.
  PRBool OnCreate(); // @see comment ***

  // Called as a hook just after the menu gets opened.
  PRBool OnCreated(); // @see comment ***

  // Called as a hook just before the menu goes away.
  PRBool OnDestroy(); // @see comment ***

  // Called as a hook just after the menu goes away.
  PRBool OnDestroyed(); // @see comment ***

  NS_IMETHOD AttributeChanged(PRInt32 aNameSpaceID,
                              nsIAtom* aAttribute,
                              PRInt32 aModType); // @see comment ***
  virtual ~nsMenuFrame();

  PRBool SizeToPopup(nsBoxLayoutState& aState, nsSize& aSize);

protected:
#ifdef DEBUG_LAYOUT
  nsresult SetDebug(nsBoxLayoutState& aState, nsIFrame* aList, PRBool aDebug);
#endif
  NS_HIDDEN_(nsresult) Notify(nsITimer* aTimer);

  nsFrameList mPopupFrames;
  PRPackedBool mIsMenu; // Whether or not we can even have children or not.
  PRPackedBool mMenuOpen;
  PRPackedBool mCreateHandlerSucceeded;  // Did the create handler succeed?
  PRPackedBool mChecked;              // are we checked?
  nsMenuType mType;

  nsIMenuParent* mMenuParent; // Our parent menu.

  // Reference to the mediator which wraps this frame.
  nsRefPtr<nsMenuTimerMediator> mTimerMediator;

  nsCOMPtr<nsITimer> mOpenTimer;

  nsString mGroupName;
  nsSize mLastPref;
  
  //we load some display strings from platformKeys.properties only once
  static nsrefcnt gRefCnt; 
  static nsString *gShiftText;
  static nsString *gControlText;
  static nsString *gMetaText;
  static nsString *gAltText;
  static nsString *gModifierSeparator;

}; // class nsMenuFrame

#endif
