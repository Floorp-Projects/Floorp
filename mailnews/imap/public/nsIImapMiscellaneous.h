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
#ifndef nsIImapMiscellaneous_h__
#define nsIImapMiscellaneous_h__

#include "nscore.h"
#include "nsISupports.h"
#include "nsImapCore.h"
#include "nsIImapProtocol.h"
#include "MailNewsTypes.h"

/* 22e3e664-e789-11d2-af83-001083002da8 */

#define NS_IIMAPMISCELLANEOUS_IID \
{ 0x22e3e664, 0xe789, 0x11d2, \
		{ 0xaf, 0x83, 0x00, 0x10, 0x83, 0x00, 0x2d, 0xa8 } }

class nsIImapMiscellaneous : public nsISupports
{
public:
	static const nsIID& GetIID()
	{
		static nsIID iid = NS_IIMAPMISCELLANEOUS_IID;
		return iid;
	}
	
	NS_IMETHOD AddSearchResult(nsIImapProtocol* aProtocol, 
														 const char* searchHitLine) = 0;
	NS_IMETHOD GetArbitraryHeaders(nsIImapProtocol* aProtocol,
																 GenericInfo* aInfo) = 0;
	NS_IMETHOD GetShouldDownloadArbitraryHeaders(nsIImapProtocol* aProtocol,
																							 GenericInfo* aInfo) = 0;
  NS_IMETHOD GetShowAttachmentsInline(nsIImapProtocol* aProtocol,
                                      PRBool* aBool) = 0;
	NS_IMETHOD HeaderFetchCompleted(nsIImapProtocol* aProtocol) = 0;
	NS_IMETHOD UpdateSecurityStatus(nsIImapProtocol* aProtocol) = 0;
	// ****
	NS_IMETHOD FinishImapConnection(nsIImapProtocol* aProtocol) = 0;
	NS_IMETHOD SetImapHostPassword(nsIImapProtocol* aProtocol,
																 GenericInfo* aInfo) = 0;
	NS_IMETHOD GetPasswordForUser(nsIImapProtocol* aProtocol,
																const char* userName) = 0;
	NS_IMETHOD SetBiffStateAndUpdate(nsIImapProtocol* aProtocol,
																	 nsMsgBiffState biffState) = 0;
	NS_IMETHOD GetStoredUIDValidity(nsIImapProtocol* aProtocol,
																	uid_validity_info* aInfo) = 0;
	NS_IMETHOD LiteSelectUIDValidity(nsIImapProtocol* aProtocol,
																	 PRUint32 uidValidity) = 0;
	NS_IMETHOD FEAlert(nsIImapProtocol* aProtocol,
										 const char* aString) = 0;
	NS_IMETHOD FEAlertFromServer(nsIImapProtocol* aProtocol,
															 const char* aString) = 0;
	NS_IMETHOD ProgressStatus(nsIImapProtocol* aProtocol,
														const char* statusMsg) = 0;
	NS_IMETHOD PercentProgress(nsIImapProtocol* aProtocol,
														 ProgressInfo* aInfo) = 0;
	NS_IMETHOD PastPasswordCheck(nsIImapProtocol* aProtocol) = 0;
	NS_IMETHOD CommitNamespaces(nsIImapProtocol* aProtocol,
															const char* hostName) = 0;
	NS_IMETHOD CommitCapabilityForHost(nsIImapProtocol* aProtocol,
																		 const char* hostName) = 0;
	NS_IMETHOD TunnelOutStream(nsIImapProtocol* aProtocol,
														 msg_line_info* aInfo) = 0;
	NS_IMETHOD ProcessTunnel(nsIImapProtocol* aProtocol,
													 TunnelInfo *aInfo) = 0;
};


#endif
