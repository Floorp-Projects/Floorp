/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef nsCalendarContainer_h___
#define nsCalendarContainer_h___

#include <stdio.h>
#include "nsICalendarContainer.h"
#include "nsIXPFCMenuBar.h"
#include "nsString.h"
#include "nsICalendarShell.h"
#include "nsICalendarWidget.h"
#include "nsIWidget.h"
#include "nsIStreamListener.h"
#include "nsIMenuManager.h"
#include "nsICalToolkit.h"
#include "nsIViewManager.h"

class nsCalendarContainer : public nsICalendarContainer {

public:
  nsCalendarContainer();
  ~nsCalendarContainer();

  NS_DECL_ISUPPORTS

  NS_IMETHOD Init(nsIWidget * aParent,
                  const nsRect& aBounds,
                  nsICalendarShell * aCalendarShell);
  NS_IMETHOD LoadURL(const nsString& aURLSpec,
                     nsIStreamObserver* aListener,
                     nsIXPFCCanvas * aParentCanvas = 0,
                     nsIPostData* aPostData = 0);
  NS_IMETHOD_(nsEventStatus) HandleEvent(nsGUIEvent *aEvent);

public:
  NS_IMETHOD SetTitle(const nsString& aTitle) ;
  NS_IMETHOD GetTitle(nsString& aResult) ;

  NS_IMETHOD SetMenuBar(nsIXPFCMenuBar * aMenuBar) ;
  NS_IMETHOD UpdateMenuBar();

  NS_IMETHOD_(nsICalendarWidget *) GetDocumentWidget();

  NS_IMETHOD SetMenuManager(nsIMenuManager * aMenuManager) ;
  NS_IMETHOD_(nsIMenuManager *) GetMenuManager() ;

  NS_IMETHOD QueryCapability(const nsIID &aIID, void** aResult);
  NS_IMETHOD Embed(nsIContentViewer* aDocViewer, 
                   const char* aCommand,
                   nsISupports* aExtraInfo);
  NS_IMETHOD SetContentViewer(nsIContentViewer* aViewer);
  NS_IMETHOD GetContentViewer(nsIContentViewer*& aResult);
  NS_IMETHOD SetApplicationShell(nsIApplicationShell* aShell);
  NS_IMETHOD GetApplicationShell(nsIApplicationShell*& aResult);

  NS_IMETHOD SetToolbarManager(nsIXPFCToolbarManager * aToolbarManager);
  NS_IMETHOD_(nsIXPFCToolbarManager *) GetToolbarManager();
  NS_IMETHOD AddToolbar(nsIXPFCToolbar * aToolbar);
  NS_IMETHOD RemoveToolbar(nsIXPFCToolbar * aToolbar);
  NS_IMETHOD UpdateToolbars();

  NS_IMETHOD ShowDialog(nsIXPFCDialog * aDialog);

  NS_IMETHOD_(nsEventStatus) ProcessCommand(nsIXPFCCommand * aCommand) ;

private:
  NS_METHOD ProcessActionCommand(nsString& aCommand);

public:
  nsICalendarShell * mCalendarShell;

private:
  nsIMenuManager * mMenuManager;
  nsIXPFCToolbarManager * mToolbarManager;
  nsICalendarWidget * mCalendarWidget;
  nsIXPFCCanvas * mRootCanvas;
  nsICalToolkit * mToolkit;

};

extern NS_CALENDAR nsresult NS_NewCalendarContainer(nsICalendarContainer** aInstancePtrResult);

#endif //nsCalendarContainer_h___
