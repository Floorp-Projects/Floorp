/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef nsImapUrl_h___
#define nsImapUrl_h___

#include "nsIImapUrl.h"
#include "nsCOMPtr.h"
#include "nsMsgMailNewsUrl.h"
#include "nsIMsgIncomingServer.h"

class nsImapUrl : public nsIImapUrl, public nsMsgMailNewsUrl, public nsIMsgUriUrl
{
public:

	NS_DECL_ISUPPORTS_INHERITED

	// nsIURI override
	NS_IMETHOD SetSpec(char * aSpec);

	/////////////////////////////////////////////////////////////////////////////// 
	// we support the nsIImapUrl interface
	///////////////////////////////////////////////////////////////////////////////

	NS_IMETHOD Initialize(const char * aUserName);

	NS_IMETHOD GetServer(nsIMsgIncomingServer ** aServer);

	NS_IMETHOD GetImapLog(nsIImapLog ** aImapLog);
	NS_IMETHOD SetImapLog(nsIImapLog  * aImapLog);

    NS_IMETHOD GetImapMailFolderSink(nsIImapMailFolderSink** aImapMailFolderSink);
    NS_IMETHOD SetImapMailFolderSink(nsIImapMailFolderSink* aImapMailFolderSink);

    NS_IMETHOD GetImapMessageSink(nsIImapMessageSink** aImapMessageSink);
    NS_IMETHOD SetImapMessageSink(nsIImapMessageSink* aImapMessageSink);

    NS_IMETHOD GetImapExtensionSink(nsIImapExtensionSink** aImapExtensionSink);
    NS_IMETHOD SetImapExtensionSink(nsIImapExtensionSink* aImapExtensionSink);

    NS_IMETHOD GetImapMiscellaneousSink(nsIImapMiscellaneousSink** aImapMiscellaneousSink);
    NS_IMETHOD SetImapMiscellaneousSink(nsIImapMiscellaneousSink* aImapMiscellaneousSink);

	NS_IMPL_CLASS_GETSET(ImapAction, nsImapAction, m_imapAction);
	NS_IMETHOD GetRequiredImapState(nsImapState * aImapUrlState);

	NS_IMETHOD AddOnlineDirectoryIfNecessary(const char *onlineMailboxName, char ** directory);
	
	NS_IMETHOD GetImapPartToFetch(char **result);
	NS_IMETHOD AllocateCanonicalPath(const char *serverPath, char onlineDelimiter, char **allocatedPath ) ;
	NS_IMETHOD AllocateServerPath(const char * aCanonicalPath, char aOnlineDelimiter, char ** aAllocatedPath);
	NS_IMETHOD CreateCanonicalSourceFolderPathString(char **result);
	NS_IMETHOD CreateServerSourceFolderPathString(char **result) ;
	NS_IMETHOD CreateServerDestinationFolderPathString(char **result);

	NS_IMETHOD	CreateSearchCriteriaString(nsString2 *aResult);
	NS_IMETHOD	CreateListOfMessageIdsString(nsString2 *result) ;
	NS_IMETHOD	MessageIdsAreUids(PRBool *result);
	NS_IMETHOD	GetMsgFlags(imapMessageFlagsType *result);	// kAddMsgFlags or kSubtractMsgFlags only
    NS_IMETHOD GetChildDiscoveryDepth(PRInt32* result);
    NS_IMETHOD GetOnlineSubDirSeparator(char* separator);
	NS_IMETHOD SetOnlineSubDirSeparator(char onlineDirSeparator);

	// for enabling or disabling mime parts on demand. Setting this to TRUE says we
	// can use mime parts on demand, if we chose.
	NS_IMETHOD	SetAllowContentChange(PRBool allowContentChange);
	NS_IMETHOD  GetAllowContentChange(PRBool *results);

    NS_IMETHOD SetCopyState(nsISupports* copyState);
    NS_IMETHOD GetCopyState(nsISupports** copyState);
    
    NS_IMETHOD SetMsgFileSpec(nsIFileSpec* fileSpec);
    NS_IMETHOD GetMsgFileSpec(nsIFileSpec** fileSpec);

    // nsIMsgUriUrl
    NS_IMETHOD GetURI(char **aURI);

	// nsImapUrl
	nsImapUrl();
	virtual ~nsImapUrl();

protected:

	virtual nsresult ParseUrl();
	char		*m_listOfMessageIds;

	// handle the imap specific parsing
	void		ParseImapPart(char *imapPartOfUrl);

	char *		ReplaceCharsInCopiedString(const char *stringToCopy, char oldChar, char newChar);
	void		ParseFolderPath(char **resultingCanonicalPath);
	void		ParseSearchCriteriaString();
	void		ParseChildDiscoveryDepth();
	void		ParseUidChoice();
	void		ParseMsgFlags();
	void		ParseListOfMessageIds();

    char        *m_sourceCanonicalFolderPathSubString;
    char        *m_destinationCanonicalFolderPathSubString;
    char		*m_tokenPlaceHolder;
	char		*m_urlidSubString;
    char		m_onlineSubDirSeparator;
	char		*m_searchCriteriaString;	// should we use m_search, or is this special?

	PRBool					m_validUrl;
	PRBool					m_runningUrl;
	PRBool					m_idsAreUids;
	PRBool					m_mimePartSelectorDetected;
	PRBool					m_allowContentChange;	// if FALSE, we can't use Mime parts on demand
	PRInt32					m_discoveryDepth;

	char *		m_userName;

	// event sinks
	imapMessageFlagsType	m_flags;
	nsImapAction			m_imapAction;
	nsCOMPtr<nsIImapLog>	m_imapLog;
    nsCOMPtr<nsIImapMailFolderSink> m_imapMailFolderSink;
    nsCOMPtr<nsIImapMessageSink>	m_imapMessageSink;
    nsCOMPtr<nsIImapExtensionSink>	m_imapExtensionSink;
    nsCOMPtr<nsIImapMiscellaneousSink> m_imapMiscellaneousSink;

	nsCOMPtr<nsIMsgIncomingServer>  m_server;
  
    // online message copy support; i don't have a better solution yet
    nsCOMPtr<nsISupports> m_copyState;
    nsCOMPtr<nsIFileSpec> m_fileSpec;
};

#endif /* nsImapUrl_h___ */
