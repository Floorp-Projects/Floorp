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

    NS_IMETHOD GetImapMailfolder(nsIImapMailfolder** aImapMailfolder);
    NS_IMETHOD SetImapMailfolder(nsIImapMailfolder* aImapMailfolder);

    NS_IMETHOD GetImapMessage(nsIImapMessage** aImapMessage);
    NS_IMETHOD SetImapMessage(nsIImapMessage* aImapMessage);

    NS_IMETHOD GetImapExtension(nsIImapExtension** aImapExtension);
    NS_IMETHOD SetImapExtension(nsIImapExtension* aImapExtension);

    NS_IMETHOD GetImapMiscellaneous(nsIImapMiscellaneous** aImapMiscellaneous);
    NS_IMETHOD SetImapMiscellaneous(nsIImapMiscellaneous* aImapMiscellaneous);

	NS_IMPL_CLASS_GETSET(ImapAction, nsImapAction, m_imapAction);

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

	NS_IMETHOD GetImapPartToFetch(char **result) ;
	NS_IMETHOD AllocateCanonicalPath(const char *serverPath, char onlineDelimiter, char **allocatedPath ) ;
	NS_IMETHOD CreateServerSourceFolderPathString(char **result) ;

	NS_IMETHOD	CreateListOfMessageIdsString(char **result) ;

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
	char		*m_errorMessage;
	char		*m_listOfMessageIds;

	char		GetOnlineSubDirSeparator();
	char *		AllocateServerPath(const char *canonicalPath, 
									char onlineDelimiter = kOnlineHierarchySeparatorUnknown);
	char *		AddOnlineDirectoryIfNecessary(const char *onlineMailboxName);

	char *		ReplaceCharsInCopiedString(const char *stringToCopy, char oldChar, char newChar);
    char        *m_sourceCanonicalFolderPathSubString;
    char        *m_destinationCanonicalFolderPathSubString;
    char		m_onlineSubDirSeparator;	


	PRBool		m_runningUrl;

	nsImapAction m_imapAction;
	nsIImapLog  * m_imapLog;
    nsIImapMailfolder* m_imapMailfolder;
    nsIImapMessage* m_imapMessage;
    nsIImapExtension* m_imapExtension;
    nsIImapMiscellaneous* m_imapMiscellaneous;

	nsIMsgIncomingServer  *m_server;
};

#endif /* nsImapUrl_h___ */
