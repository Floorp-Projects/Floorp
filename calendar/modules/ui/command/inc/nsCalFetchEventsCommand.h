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

#ifndef nsCalFetchEventsCommand_h___
#define nsCalFetchEventsCommand_h___

#include "nsXPFCCommand.h"
#include "nsCalUtilCIID.h"
#include "nsIDateTime.h"

class nsCalFetchEventsCommand : public nsXPFCCommand
{
public:
  nsCalFetchEventsCommand();

  NS_DECL_ISUPPORTS

  NS_IMETHOD Init() ;

protected:
  ~nsCalFetchEventsCommand();

public:
  nsIDateTime * mStartDate;
  nsIDateTime * mEndDate;

};

#endif /* nsCalFetchEventsCommand_h___ */
