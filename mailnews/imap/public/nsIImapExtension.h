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
#ifndef nsIImapExtension_h__
#define nsIImapExtension_h__

#include "nscore.h"
#include "nsISupports.h"
#include "nsImapCore.h"
#include "nsIImapProtocol.h"

/* 44ede08e-e77f-11d2-af83-001083002da8 */

#define NS_IIMAPEXTENSION_IID \
{ 0x44ede08e, 0xe77f, 0x11d2, \
  { 0xaf, 0x83, 0x00, 0x10, 0x83, 0x00, 0x2d, 0xa8 } }

class nsIImapExtension : public nsISupports
{
public:
  static const nsIID& GetIID()
  {
	static nsIID iid = NS_IIMAPEXTENSION_IID;
	return iid;
  }
  
  NS_IMETHOD SetUserAuthenticated(nsIImapProtocol* aProtocol,
								  PRBool aBool) = 0;
  NS_IMETHOD SetMailServerUrls(nsIImapProtocol* aProtocol,
							   const char* hostName) = 0;
  NS_IMETHOD SetMailAccountUrl(nsIImapProtocol* aProtocol,
							   const char* hostName) = 0;
  NS_IMETHOD ClearFolderRights(nsIImapProtocol* aProtocol,
							   TIMAPACLRightsInfo* aclRights) = 0;
  NS_IMETHOD AddFolderRights(nsIImapProtocol* aProtocol,
							 TIMAPACLRightsInfo* aclRights) = 0;
  NS_IMETHOD RefreshFolderRights(nsIImapProtocol* aProtocol,
								 TIMAPACLRightsInfo* aclRights) = 0;
  NS_IMETHOD FolderNeedsACLInitialized(nsIImapProtocol* aProtocol,
									   TIMAPACLRightsInfo* aclRights) = 0;
  NS_IMETHOD SetFolderAdminURL(nsIImapProtocol* aProtocol,
							   FolderQueryInfo* aInfo) = 0;
};

#endif
