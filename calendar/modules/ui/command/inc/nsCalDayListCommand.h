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

#ifndef nsCalDayListCommand_h___
#define nsCalDayListCommand_h___

#include "nsXPFCCommand.h"
#include "nsCalUtilCIID.h"
#include "nsIArray.h"
#include "nsIIterator.h"
#include "nsIDateTime.h"


class nsCalDayListCommand : public nsXPFCCommand
{
public:
  nsCalDayListCommand();

  NS_DECL_ISUPPORTS

  NS_IMETHOD Init() ;
  NS_IMETHOD CreateIterator(nsIIterator ** aIterator) ;

  NS_IMETHOD AddDateTime(nsIDateTime * aDateTime);
  NS_IMETHOD AddDateVector(nsIArray * aDateVector);

protected:
  ~nsCalDayListCommand();

private:
  nsIArray *     mDateTimes ;


};

#endif /* nsCalDayListCommand_h___ */
