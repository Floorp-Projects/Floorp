/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

#ifndef nsImapService_h___
#define nsImapService_h___

#include "nsIImapService.h"
#include "nsIMsgMessageService.h"
#include "nsISupportsArray.h"
#include "nsCOMPtr.h"
#include "nsIFileSpec.h"
#include "nsIProtocolHandler.h"
#include "nsIMsgProtocolInfo.h"

#include "nsICacheSession.h"

class nsIImapHostSessionList; 
class nsCString;
class nsIImapUrl;
class nsIMsgFolder;
class nsIMsgStatusFeedback;

class nsImapService : public nsIImapService,
                      public nsIMsgMessageService,
                      public nsIMsgMessageFetchPartService,
                      public nsIProtocolHandler,
                      public nsIMsgProtocolInfo
{
public:

	nsImapService();
	virtual ~nsImapService();
	
	NS_DECL_ISUPPORTS
  NS_DECL_NSIMSGPROTOCOLINFO

	////////////////////////////////////////////////////////////////////////////////////////
	// we suppport the nsIImapService interface 
	////////////////////////////////////////////////////////////////////////////////////////
  NS_DECL_NSIIMAPSERVICE

	////////////////////////////////////////////////////////////////////////////////////////
	// we suppport the nsIMsgMessageService Interface 
	////////////////////////////////////////////////////////////////////////////////////////
  NS_DECL_NSIMSGMESSAGESERVICE

	////////////////////////////////////////////////////////////////////////////////////////
	// we suppport the nsIProtocolHandler interface 
	////////////////////////////////////////////////////////////////////////////////////////
  NS_DECL_NSIPROTOCOLHANDLER

  NS_DECL_NSIMSGMESSAGEFETCHPARTSERVICE

protected:

    PRUnichar GetHierarchyDelimiter(nsIMsgFolder* aMsgFolder);
    nsresult GetFolderName(nsIMsgFolder* aImapFolder,
                           char **folderName);
	  nsresult CreateStartOfImapUrl(const char * aImapURI /* a RDF URI for the current message / folder, can be null */,
                                  nsIImapUrl  **imapUrl,
                                  nsIMsgFolder* aImapFolder,
                                  nsIUrlListener * aUrlListener,
                                  nsCString & urlSpec,
								                  PRUnichar &hierarchyDelimiter);
    nsresult GetImapConnectionAndLoadUrl(nsIEventQueue* aClientEventQueue, 
                                         nsIImapUrl* aImapUrl,
                                         nsISupports* aConsumer,
                                         nsIURI** aURL);
    nsresult SetImapUrlSink(nsIMsgFolder* aMsgFolder,
                              nsIImapUrl* aImapUrl);
    nsresult FetchMimePart(nsIImapUrl * aImapUrl,
                            nsImapAction aImapAction,
                            nsIMsgFolder * aImapMailFolder, 
                            nsIImapMessageSink * aImapMessage,
                            nsIURI ** aURL,
							              nsISupports * aDisplayConsumer, 
                            const char *messageIdentifierList,
                            const char *mimePart);

	nsresult DiddleFlags(nsIEventQueue * aClientEventQueue,
                         nsIMsgFolder * aImapMailFolder, 
                         nsIUrlListener * aUrlListener, 
                         nsIURI ** aURL,
                         const char *messageIdentifierList,
                         const char *howToDiddle,
                         imapMessageFlagsType flags,
                         PRBool messageIdsAreUID);

  nsresult OfflineAppendFromFile(nsIFileSpec* aFileSpec,
                                     nsIURI *aUrl,
                                     nsIMsgFolder* aDstFolder,
                                     const char* messageId, // te be replaced
                                     PRBool inSelectedState, // needs to be in
                                     nsIUrlListener* aListener,
                                     nsIURI** aURL,
                                     nsISupports* aCopyState);
    // just a little helper method...maybe it should be a macro? which helps break down a imap message uri
    // into the folder and message key equivalents
    nsresult DecomposeImapURI(const char * aMessageURI, nsIMsgFolder ** aFolder,  char ** msgKey);
    nsresult DecomposeImapURI(const char * aMessageURI, nsIMsgFolder ** aFolder,  nsMsgKey *msgKey);

    PRBool                mPrintingOperation;  // Flag for printing operations

    // handle to the cache session for imap.....
    nsCOMPtr<nsICacheSession> mCacheSession;
};

#endif /* nsImapService_h___ */
