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

#ifndef nsCalDurationCommand_h___
#define nsCalDurationCommand_h___

#include "nsXPFCCommand.h"
#include "nsDuration.h"
#include "nsCalPeriodFormat.h"

class nsCalDurationCommand : public nsXPFCCommand
{
public:
  nsCalDurationCommand();

  NS_DECL_ISUPPORTS

  NS_IMETHOD Init(nsDuration * aDuration) ;

  NS_IMETHOD_(nsDuration *) GetDuration();
  NS_IMETHOD SetDuration(nsDuration * aDuration);

  NS_IMETHOD_(nsCalPeriodFormat) GetPeriodFormat();
  NS_IMETHOD SetPeriodFormat(nsCalPeriodFormat aPeriodFormat);

private:
  nsCalPeriodFormat mPeriodFormat;
  nsDuration * mDuration;

protected:
  ~nsCalDurationCommand();

};

#endif /* nsCalDurationCommand_h___ */
