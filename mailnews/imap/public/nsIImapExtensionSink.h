/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#ifndef nsIImapExtensionSink_h__
#define nsIImapExtensionSink_h__

#include "nscore.h"
#include "nsISupports.h"
#include "nsImapCore.h"
#include "nsIImapProtocol.h"
#include "nsMsgKeyArray.h"

class nsIImapUrl;

/* 44ede08e-e77f-11d2-af83-001083002da8 */

#define NS_IIMAPEXTENSIONSINK_IID \
{ 0x44ede08e, 0xe77f, 0x11d2, \
  { 0xaf, 0x83, 0x00, 0x10, 0x83, 0x00, 0x2d, 0xa8 } }

class nsIImapExtensionSink : public nsISupports
{
public:
  static const nsIID& GetIID()
  {
	static nsIID iid = NS_IIMAPEXTENSIONSINK_IID;
	return iid;
  }
  
  NS_IMETHOD ClearFolderRights(nsIImapProtocol* aProtocol,
							   nsIMAPACLRightsInfo* aclRights) = 0;
  NS_IMETHOD AddFolderRights(nsIImapProtocol* aProtocol,
							 nsIMAPACLRightsInfo* aclRights) = 0;
  NS_IMETHOD RefreshFolderRights(nsIImapProtocol* aProtocol,
								 nsIMAPACLRightsInfo* aclRights) = 0;
  NS_IMETHOD FolderNeedsACLInitialized(nsIImapProtocol* aProtocol,
									   nsIMAPACLRightsInfo* aclRights) = 0;
  NS_IMETHOD SetCopyResponseUid(nsIImapProtocol* aProtocol,
                                nsMsgKeyArray* keyArray, 
                                const char *msgIdString,
                                nsIImapUrl * aUrl) = 0;
  NS_IMETHOD SetAppendMsgUid(nsIImapProtocol* aProtocol,
                             nsMsgKey newKey,
                             nsIImapUrl * aUrl) = 0;
  NS_IMETHOD GetMessageId(nsIImapProtocol* aProtocol,
                          nsCString* messageId,
                          nsIImapUrl * aUrl) = 0;
};

#endif
