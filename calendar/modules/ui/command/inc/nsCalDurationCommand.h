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
