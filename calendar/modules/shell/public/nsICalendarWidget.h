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
#ifndef nsICalendarWidget_h___
#define nsICalendarWidget_h___

#include "nsIContentViewer.h"
#include "nsIParser.h"
#include "nsIXPFCCanvas.h"
#include "nsIMenuBar.h"

class nsIPostData;
class nsICalendarShell;


// Interface to the calendar widget. This object defines 
// a rectangular region in the ICalendarApp object which
// contains CalendarPanels

//2a5b8ac0-ea8e-11d1-9244-00805f8a7ab6
#define NS_ICALENDAR_WIDGET_IID   \
{ 0x2a5b8ac0, 0xea8e, 0x11d1,    \
{ 0x92, 0x44, 0x00, 0x80, 0x5f, 0x8a, 0x7a, 0xb6 } }

class nsICalendarWidget : public nsIContentViewer 
{

public:

  // Create a native window for this calendar widget
  NS_IMETHOD Init(nsNativeWidget aNativeParent,
                  nsIDeviceContext* aDeviceContext,
                  nsIPref* aPrefs,
                  const nsRect& aBounds,
                  nsScrollPreference aScrolling = nsScrollPreference_kAuto) = 0;
  NS_IMETHOD Init(nsIView * aParent,
                  const nsRect& aBounds,
                  nsICalendarShell * aCalendarShell) = 0;


  NS_IMETHOD_(nsEventStatus) HandleEvent(nsGUIEvent *aEvent) = 0 ;

  NS_IMETHOD SetContainer(nsIContentViewerContainer* aContainer) = 0;

  NS_IMETHOD GetContainer(nsIContentViewerContainer*& aContainerResult) = 0;

  NS_IMETHOD BindToDocument(nsISupports *aDoc, const char *aCommand) = 0;

  NS_IMETHOD LoadURL(const nsString& aURLSpec,
                     nsIStreamObserver* aListener,
                     nsIPostData* aPostData = 0) = 0;

};


#endif /* nsCalendarWidget_h___ */
