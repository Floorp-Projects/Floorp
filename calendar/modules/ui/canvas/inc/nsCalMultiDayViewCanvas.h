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

#ifndef nsCalMultiDayViewCanvas_h___
#define nsCalMultiDayViewCanvas_h___

#include "nsCalMultiViewCanvas.h"
#include "nsCalTimebarComponentCanvas.h"
#include "nsCalTimebarCanvas.h"
#include "nsDateTime.h"

class nsCalMultiDayViewCanvas : public nsCalMultiViewCanvas
{

public:
  nsCalMultiDayViewCanvas(nsISupports* outer);

  NS_DECL_ISUPPORTS

  NS_IMETHOD Init();
  NS_IMETHOD_(nsEventStatus) PaintBackground(nsIRenderingContext& aRenderingContext,
                                             const nsRect& aDirtyRect);

  NS_IMETHOD_(nsIXPFCCanvas*) AddDayViewCanvas();
  NS_IMETHOD_(PRUint32) GetNumberViewableDays();
  NS_IMETHOD SetNumberViewableDays(PRUint32 aNumberViewableDays);

  NS_IMETHOD SetShowHeaders(PRBool aShowHeaders);
  NS_IMETHOD SetShowStatus(PRBool aShowHeaders);
  NS_IMETHOD_(PRBool) GetShowHeaders();
  NS_IMETHOD_(PRBool) GetShowStatus();
  NS_IMETHOD SetTimeContext(nsICalTimeContext * aContext);

  NS_IMETHOD SetParameter(nsString& aKey, nsString& aValue) ;

  // nsIXPFCCommandReceiver methods
  NS_IMETHOD Action(nsIXPFCCommand * aCommand);


  NS_IMETHOD SetChildTimeContext(nsCalTimebarCanvas * aCanvas,
                                 nsICalTimeContext * aContext,
                                 PRUint32 increment);
  NS_IMETHOD ChangeChildDateTime(nsCalTimebarCanvas * aCanvas,
                                 nsDateTime * aDateTime);
  NS_IMETHOD ChangeChildDateTime(PRUint32 aIndex, nsDateTime * aDateTime);




protected:
  ~nsCalMultiDayViewCanvas();

private:
  PRUint32 mNumberViewableDays;
  PRBool mShowHeaders;
  PRBool mShowStatus;
  PRUint32 mMaxRepeat;
  PRUint32 mMinRepeat;

};

#endif /* nsCalMultiDayViewCanvas_h___ */
