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
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

/********************************************************************************************************
 
   Interface for representing Local Mail folders.
 
*********************************************************************************************************/

#ifndef nsLocalMessage_h__
#define nsLocalMessage_h__

#include "nsMessage.h" /* include the interface we are going to support */

class nsLocalMessage : public nsMessage
{
public:
	nsLocalMessage(void);
	virtual ~nsLocalMessage(void);

	NS_DECL_ISUPPORTS_INHERITED
	NS_IMETHOD GetMsgFolder(nsIMsgFolder **folder);
	NS_IMETHOD GetMessageType(PRUint32 *aMessageType);

protected:
	nsresult GetFolderFromURI(nsIMsgFolder **folder);

};

#endif //nsLocalMessage_h__
