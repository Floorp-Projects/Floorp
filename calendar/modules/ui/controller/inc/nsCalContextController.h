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

#ifndef nsCalContextController_h___
#define nsCalContextController_h___

#include "nsICalContextController.h"
#include "nsCalCanvas.h"

#include "nsIXPFCSubject.h"
#include "nsIXPFCCommand.h"

#include "nsCalPeriodFormat.h"
#include "nsDuration.h"

class nsCalContextController : public nsICalContextController,
                               public nsIXPFCSubject,
                               public nsCalCanvas
{
public:
  nsCalContextController(nsISupports * aOuter);

  NS_DECL_ISUPPORTS

  // ContextController Methods
  NS_IMETHOD_(void)                           SetOrientation(nsContextControllerOrientation eOrientation);
  NS_IMETHOD_(nsContextControllerOrientation) GetOrientation();

  NS_IMETHOD_(nsDuration *) GetDuration();
  NS_IMETHOD SetDuration(nsDuration * aDuration);

  // nsIXPFCSubject methods
  NS_IMETHOD Init();
  NS_IMETHOD Attach(nsIXPFCObserver * aObserver);
  NS_IMETHOD Detach(nsIXPFCObserver * aObserver);
  NS_IMETHOD Notify(nsIXPFCCommand * aCommand);

  NS_IMETHOD_(nsCalPeriodFormat) GetPeriodFormat();
  NS_IMETHOD SetPeriodFormat(nsCalPeriodFormat aPeriodFormat);
  // nsIXMLParserObject methods
  NS_IMETHOD SetParameter(nsString& aKey, nsString& aValue) ;

private:
  nsCalPeriodFormat mPeriodFormat;

protected:
  ~nsCalContextController();

private:
  nsContextControllerOrientation mOrientation;
  nsDuration * mDuration;

};

#endif /* nsCalContextController_h___ */
