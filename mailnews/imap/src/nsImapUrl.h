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
#include "nsIUrlListenerManager.h"
#include "nsINetlibURL.h" /* this should be temporary until nunet project lands */
#include "nsIMsgIncomingServer.h"

class nsImapUrl : public nsIImapUrl, public nsINetlibURL
{
public:

	NS_DECL_ISUPPORTS

	/////////////////////////////////////////////////////////////////////////////// 
	// we support the nsIMsgMailNewsUrl interface...
	///////////////////////////////////////////////////////////////////////////////
	NS_IMETHOD SetUrlState(PRBool aRunningUrl, nsresult aExitCode);
	NS_IMETHOD GetUrlState(PRBool * aRunningUrl);

	NS_IMETHOD SetErrorMessage (char * errorMessage);
	NS_IMETHOD GetErrorMessage (char ** errorMessage) const;  // caller must free using PR_Free
	NS_IMETHOD RegisterListener (nsIUrlListener * aUrlListener);
	NS_IMETHOD UnRegisterListener (nsIUrlListener * aUrlListener);

	/////////////////////////////////////////////////////////////////////////////// 
	// we support the nsIImapUrl interface
	///////////////////////////////////////////////////////////////////////////////
	NS_IMETHOD SetServer(nsIMsgIncomingServer * aServer);
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

	// for enabling or disabling mime parts on demand. Setting this to TRUE says we
	// can use mime parts on demand, if we chose.
	NS_IMETHOD	SetAllowContentChange(PRBool allowContentChange);
	NS_IMETHOD  GetAllowContentChange(PRBool *results);

	/////////////////////////////////////////////////////////////////////////////// 
	// we support the nsINetlibURL interface
	///////////////////////////////////////////////////////////////////////////////

    NS_IMETHOD GetURLInfo(URL_Struct_ **aResult) const;
    NS_IMETHOD SetURLInfo(URL_Struct_ *URL_s);

	/////////////////////////////////////////////////////////////////////////////// 
	// we support the nsIURL interface
	///////////////////////////////////////////////////////////////////////////////

	// mscott: some of these we won't need to implement..as part of the netlib re-write we'll be removing them
	// from nsIURL and then we can remove them from here as well....
    NS_IMETHOD_(PRBool) Equals(const nsIURL *aURL) const;
    NS_IMETHOD GetSpec(const char* *result) const;
    NS_IMETHOD SetSpec(const char* spec);
    NS_IMETHOD GetProtocol(const char* *result) const;
    NS_IMETHOD SetProtocol(const char* protocol);
    NS_IMETHOD GetHost(const char* *result) const;
    NS_IMETHOD SetHost(const char* host);
    NS_IMETHOD GetHostPort(PRUint32 *result) const;
    NS_IMETHOD SetHostPort(PRUint32 port);
    NS_IMETHOD GetFile(const char* *result) const;
    NS_IMETHOD SetFile(const char* file);
    NS_IMETHOD GetRef(const char* *result) const;
    NS_IMETHOD SetRef(const char* ref);
    NS_IMETHOD GetSearch(const char* *result) const;
    NS_IMETHOD SetSearch(const char* search);
    NS_IMETHOD GetContainer(nsISupports* *result) const;
    NS_IMETHOD SetContainer(nsISupports* container);	
    NS_IMETHOD GetLoadAttribs(nsILoadAttribs* *result) const;	// make obsolete
    NS_IMETHOD SetLoadAttribs(nsILoadAttribs* loadAttribs);	// make obsolete
    NS_IMETHOD GetURLGroup(nsIURLGroup* *result) const;	// make obsolete
    NS_IMETHOD SetURLGroup(nsIURLGroup* group);	// make obsolete
    NS_IMETHOD SetPostHeader(const char* name, const char* value);	// make obsolete
    NS_IMETHOD SetPostData(nsIInputStream* input);	// make obsolete
    NS_IMETHOD GetContentLength(PRInt32 *len);
    NS_IMETHOD GetServerStatus(PRInt32 *status);  // make obsolete
    NS_IMETHOD ToString(PRUnichar* *aString) const;

	// nsImapUrl
	nsImapUrl();
	virtual ~nsImapUrl();

protected:

	nsresult ParseURL(const nsString& aSpec, const nsIURL* aURL = nsnull);
	void ReconstructSpec(void);
	// manager of all of current url listeners....
	nsIUrlListenerManager * m_urlListeners;
	// Here's our link to the old netlib world....
    URL_Struct *m_URL_s;

	char		*m_spec;
    char		*m_protocol;
    char		*m_host;
	PRUint32	 m_port;
	char		*m_search;
	char		*m_file;
	char		*m_errorMessage;
	char		*m_listOfMessageIds;

	// handle the imap specific parsing
	void		ParseImapPart(char *imapPartOfUrl);

	char		GetOnlineSubDirSeparator();
	void		SetOnlineSubDirSeparator(char onlineDirSeparator);
	char *		ReplaceCharsInCopiedString(const char *stringToCopy, char oldChar, char newChar);
	void		ParseFolderPath(char **resultingCanonicalPath);
	void		ParseSearchCriteriaString();
	void		ParseChildDiscoveryDepth();
	void		ParseUidChoice();
	void		ParseMsgFlags();
	void		ParseListofMessageIds();

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

	// event sinks
	imapMessageFlagsType	m_flags;
	nsImapAction			m_imapAction;
	nsIImapLog  * m_imapLog;
    nsIImapMailFolderSink* m_imapMailFolderSink;
    nsIImapMessageSink* m_imapMessageSink;
    nsIImapExtensionSink* m_imapExtensionSink;
    nsIImapMiscellaneousSink* m_imapMiscellaneousSink;

	nsIMsgIncomingServer  *m_server;
};

#endif /* nsImapUrl_h___ */
