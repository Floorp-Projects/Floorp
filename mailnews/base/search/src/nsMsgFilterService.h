/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef _nsMsgFilterService_H_
#define _nsMsgFilterService_H_

#include "nsIMsgFilterService.h"




// The filter service is used to acquire and manipulate filter lists.

class nsMsgFilterService : public nsIMsgFilterService
{

public:
	nsMsgFilterService();
	virtual ~nsMsgFilterService();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIMSGFILTERSERVICE
/* clients call OpenFilterList to get a handle to a FilterList, of existing nsMsgFilter *.
	These are manipulated by the front end as a result of user interaction
   with dialog boxes. To apply the new list call MSG_CloseFilterList.

*/
};

#endif  // _nsMsgFilterService_H_

