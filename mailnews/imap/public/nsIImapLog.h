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
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsIImapLog_h___
#define nsIImapLog_h___

/////////////////////////////////////////////////////////////////////////////
// An Imap Log is used to record imap log information. If a url has a log
// sink set on it, then the imap protocol will call through this interface
// with the data returned from the server. 
//
// Why not just use PR_Log? We'll still use PR_Log, but we wanted to use this
// sink as a simple interface to test our event notification design for imap.
//////////////////////////////////////////////////////////////////////////////


#include "nscore.h"
#include "nsISupports.h"

/* B08502B0-E0B1-11d2-806D-006008128C4E */

#define NS_IIMAPLOG_IID								\
{ 0xb08502b0, 0xe0b1, 0x11d2,						\
    { 0x80, 0x6d, 0x0, 0x60, 0x8, 0x12, 0x8c, 0x4e } }


class nsIImapLog : public nsISupports
{
public:
	static const nsIID& GetIID() 
	{
		static nsIID iid = NS_IIMAPLOG_IID;
		return iid;
	}

	NS_IMETHOD HandleImapLogData (const char * aLogData) = 0;
	
};

#endif /* nsIImapLog_h___ */
