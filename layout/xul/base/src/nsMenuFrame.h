/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * Original Author: David W. Hyatt (hyatt@netscape.com)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
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
#include "nsITimerCallback.h"
#include "nsISupportsArray.h"
#include "nsIDOMText.h"
#include "nsIContent.h"
#include "nsIScrollableViewProvider.h"

nsresult NS_NewMenuFrame(nsIPresShell* aPresShell, nsIFrame** aResult, PRUint32 aFlags) ;

class nsMenuBarFrame;
class nsMenuPopupFrame;
class nsCSSFrameConstructor;
class nsIScrollableView;

#define NS_STATE_ACCELTEXT_IS_DERIVED  NS_STATE_BOX_CHILD_RESERVED

class nsMenuFrame : public nsBoxFrame, 
                    public nsIMenuFrame,
                    public nsITimerCallback,
                    public nsIScrollableViewProvider
{
public:
  nsMenuFrame(nsIPresShell* aShell);

  NS_DECL_ISUPPORTS

  // nsIBox
  NS_IMETHOD DoLayout(nsBoxLayoutState& aBoxLayoutState);
  NS_IMETHOD GetPrefSize(nsBoxLayoutState& aBoxLayoutState, nsSize& aSize);

  // The nsITimerCallback interface
  NS_IMETHOD_(void) Notify(nsITimer *timer);

  NS_IMETHOD Init(nsIPresContext*  aPresContext,
                  nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsIStyleContext* aContext,
                  nsIFrame*        aPrevInFlow);

  NS_IMETHOD SetDebug(nsBoxLayoutState& aState, PRBool aDebug);

  NS_IMETHOD IsActive(PRBool& aResult) { aResult = PR_TRUE; return NS_OK; };

  // The following four methods are all overridden so that the menu children
  // can be stored in a separate list (so that they don't impact reflow of the
  // actual menu item at all).
  NS_IMETHOD FirstChild(nsIPresContext* aPresContext,
                        nsIAtom*        aListName,
                        nsIFrame**      aFirstChild) const;
  NS_IMETHOD SetInitialChildList(nsIPresContext* aPresContext,
                                 nsIAtom*        aListName,
                                 nsIFrame*       aChildList);
  NS_IMETHOD GetAdditionalChildListName(PRInt32   aIndex,
                                        nsIAtom** aListName) const;
  NS_IMETHOD Destroy(nsIPresContext* aPresContext);

  // Overridden to prevent events from ever going to children of the menu.
  NS_IMETHOD GetFrameForPoint(nsIPresContext* aPresContext,
                              const nsPoint& aPoint,
                              nsFramePaintLayer aWhichLayer,    
                              nsIFrame**     aFrame);

  NS_IMETHOD HandleEvent(nsIPresContext* aPresContext, 
                         nsGUIEvent*     aEvent,
                         nsEventStatus*  aEventStatus);

  NS_IMETHOD  AppendFrames(nsIPresContext* aPresContext,
                           nsIPresShell&   aPresShell,
                           nsIAtom*        aListName,
                           nsIFrame*       aFrameList);

  NS_IMETHOD  InsertFrames(nsIPresContext* aPresContext,
                           nsIPresShell&   aPresShell,
                           nsIAtom*        aListName,
                           nsIFrame*       aPrevFrame,
                           nsIFrame*       aFrameList);

  NS_IMETHOD  RemoveFrame(nsIPresContext* aPresContext,
                          nsIPresShell&   aPresShell,
                          nsIAtom*        aListName,
                          nsIFrame*       aOldFrame);

  // nsIMenuFrame Interface

  NS_IMETHOD ActivateMenu(PRBool aActivateFlag);
  NS_IMETHOD SelectMenu(PRBool aActivateFlag);
  NS_IMETHOD OpenMenu(PRBool aActivateFlag);

  NS_IMETHOD MenuIsOpen(PRBool& aResult) { aResult = IsOpen(); return NS_OK; }
  NS_IMETHOD MenuIsContainer(PRBool& aResult) { aResult = IsMenu(); return NS_OK; }
  NS_IMETHOD MenuIsChecked(PRBool& aResult) { aResult = mChecked; return NS_OK; }
  NS_IMETHOD MenuIsDisabled(PRBool& aResult) { aResult = IsDisabled(); return NS_OK; }
  
  NS_IMETHOD GetActiveChild(nsIDOMElement** aResult);
  NS_IMETHOD SetActiveChild(nsIDOMElement* aChild);

  NS_IMETHOD SelectFirstItem();

  NS_IMETHOD Escape(PRBool& aHandledFlag);
  NS_IMETHOD Enter();
  NS_IMETHOD ShortcutNavigation(PRUint32 aLetter, PRBool& aHandledFlag);
  NS_IMETHOD KeyboardNavigation(PRUint32 aDirection, PRBool& aHandledFlag);

  NS_IMETHOD SetParent(const nsIFrame* aParent);

  NS_IMETHOD GetMenuParent(nsIMenuParent** aResult) { NS_IF_ADDREF(mMenuParent); *aResult = mMenuParent; return NS_OK; };
  NS_IMETHOD GetMenuChild(nsIFrame** aResult) { *aResult = mPopupFrames.FirstChild(); return NS_OK; }
  NS_IMETHOD GetRadioGroupName(nsString &aName) { aName = mGroupName; return NS_OK; };
  NS_IMETHOD GetMenuType(nsMenuType &aType) { aType = mType; return NS_OK; };
  NS_IMETHOD MarkChildrenStyleChange();
  NS_IMETHOD MarkAsGenerated();

  // nsIScrollableViewProvider methods

  NS_IMETHOD GetScrollableView(nsIScrollableView** aView);

  // nsMenuFrame methods 

  PRBool IsOpen() { return mMenuOpen; };
  PRBool IsMenu();
  PRBool IsDisabled();
  PRBool IsGenerated();
  NS_IMETHOD ToggleMenuState();

  void SetIsMenu(PRBool aIsMenu) { mIsMenu = aIsMenu; };

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsString& aResult) const
  {
      return MakeFrameName("Menu", aResult);
  }
#endif

  void SetFrameConstructor(nsCSSFrameConstructor* aFC) {
    mFrameConstructor = aFC;
  }

protected:

  virtual void RePositionPopup(nsBoxLayoutState& aState);

  static void UpdateDismissalListener(nsIMenuParent* aMenuParent);
  void UpdateMenuType(nsIPresContext* aPresContext);
  void UpdateMenuSpecialState(nsIPresContext* aPresContext);

  void OpenMenuInternal(PRBool aActivateFlag);
  void GetMenuChildrenElement(nsIContent** aResult);

  // Examines the key node and builds the accelerator.
  void BuildAcceleratorText();

  // Called to execute our command handler.
  void Execute();

  // Called as a hook just before the menu gets opened.
  PRBool OnCreate();

  // Called as a hook just after the menu gets opened.
  PRBool OnCreated();

  // Called as a hook just before the menu goes away.
  PRBool OnDestroy();

  // Called as a hook just after the menu goes away.
  PRBool OnDestroyed();

  NS_IMETHOD AttributeChanged(nsIPresContext* aPresContext,
                              nsIContent* aChild,
                              PRInt32 aNameSpaceID,
                              nsIAtom* aAttribute,
                              PRInt32 aModType, 
                              PRInt32 aHint);
  virtual ~nsMenuFrame();

protected:
  nsresult SetDebug(nsBoxLayoutState& aState, nsIFrame* aList, PRBool aDebug);

  nsFrameList mPopupFrames;
  PRPackedBool mIsMenu; // Whether or not we can even have children or not.
  PRPackedBool mMenuOpen;
  PRPackedBool mCreateHandlerSucceeded;  // Did the create handler succeed?
  PRPackedBool mChecked;              // are we checked?
  nsMenuType mType;

  nsIMenuParent* mMenuParent; // Our parent menu.
  nsCOMPtr<nsITimer> mOpenTimer;
  nsIPresContext* mPresContext; // Our pres context.
  nsString mGroupName;
  nsSize mLastPref;
  
  //we load some display strings from platformKeys.properties only once
  static nsrefcnt gRefCnt; 
  static nsString *gShiftText;
  static nsString *gControlText;
  static nsString *gMetaText;
  static nsString *gAltText;
  static nsString *gModifierSeparator;

public:
  static nsMenuDismissalListener* sDismissalListener; // The listener that dismisses menus.
private:
  nsCSSFrameConstructor* mFrameConstructor;
}; // class nsMenuFrame

#endif
