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
