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

#ifndef nsCalToolkit_h___
#define nsCalToolkit_h___

#include "nsICalToolkit.h"
#include "nsICalendarShell.h"
#include "nsIXPFCObserverManager.h"
#include "nsIXPFCCanvasManager.h"
#include "nsXPFCToolkit.h"

class nsCalToolkit : public nsICalToolkit,
                     public nsXPFCToolkit
{
public:
  nsCalToolkit();

  NS_DECL_ISUPPORTS

  NS_IMETHOD Init(nsIApplicationShell * aApplicationShell) ;
  NS_IMETHOD_(CAPISession) GetCAPISession();
  NS_IMETHOD_(CAPIHandle) GetCAPIHandle();

  NS_IMETHOD_(char *) GetCAPIPassword() ;
  NS_IMETHOD_(NSCalendar *) GetNSCalendar();

protected:
  ~nsCalToolkit();

private:
  nsICalendarShell * mCalendarShell;

};

// XXX: Need a SessionManager to manage various toolkits across
//      different applications.  For now, this is a convenient
//      way to access the global application shell
extern nsCalToolkit * gCalToolkit;


#endif /* nsCalToolkit_h___ */
