/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Contributor(s): 
 */

#ifndef nsCalendarWidget_h___
#define nsCalendarWidget_h___

#include <stdio.h>
#include "nsICalendarWidget.h"
#include "nsIWidget.h"
#include "nsString.h"
#include "nsIXPFCCanvas.h"
#include "nsFont.h"
#include "nsIURL.h"
#include "nsIStreamListener.h"
#include "nsIDTD.h"
#include "nsIParser.h"
#include "nscalexport.h"
#include "nsICalToolkit.h"
#include "nsXPFCCanvas.h"


// Machine independent implementation portion of the calendar widget
class nsCalendarWidget : public nsICalendarWidget
{
public:
  nsCalendarWidget();
  ~nsCalendarWidget();

  NS_DECL_ISUPPORTS

  NS_IMETHOD Init(nsNativeWidget aParent,
                  nsIDeviceContext* aDeviceContext,
                  nsIPref* aPrefs,
                  const nsRect& aBounds,
                  nsScrollPreference aScrolling = nsScrollPreference_kAuto);


  NS_IMETHOD Init(nsIView * aParent,
                  const nsRect& aBounds,
                  nsICalendarShell * aCalendarShell);

  virtual nsRect GetBounds();
  virtual void SetBounds(const nsRect& aBounds);
  virtual void Show();
  virtual void Hide();
  virtual void Move(PRInt32 aX, PRInt32 aY) ;

  NS_IMETHOD BindToDocument(nsISupports *aDoc, const char *aCommand);

  NS_IMETHOD SetContainer(nsIContentViewerContainer* aContainer);

  NS_IMETHOD GetContainer(nsIContentViewerContainer*& aContainerResult);

  NS_IMETHOD LoadURL(const nsString& aURLSpec,
                     nsIStreamObserver* aListener,
                     nsIXPFCCanvas * aParentCanvas = 0,
                     nsIPostData* aPostData = 0);

  virtual nsIWidget* GetWWWindow();

  NS_IMETHOD_(nsEventStatus) HandleEvent(nsGUIEvent *aEvent);

private:
  nsIWidget * mWindow;
  nsIStreamListener * mStreamListener;

public:
  nsICalendarShell * mCalendarShell;

};

// Create a new calendar widget that uses the default presentation
// context.
extern NS_CALENDAR nsresult NS_NewCalendarWidget(nsICalendarWidget** aInstancePtrResult);

#endif //nsCalendarWidget_h___
