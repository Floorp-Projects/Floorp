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
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef _nsIMAPServerResponseParser_H_
#define _nsIMAPServerResponseParser_H_

#include "nsImapCore.h"
#include "nsIMAPHostSessionList.h"
#include "nsImapSearchResults.h"
#include "nsString2.h"

class nsIMAPNamespace;
class nsIMAPNamespaceList;
class nsIMAPBodyShell;
class nsImapSearchResultIterator;
class nsImapFlagAndUidState;
class nsString2;

#include "nsIMAPGenericParser.h"

class nsImapServerResponseParser : public nsIMAPGenericParser 
{
public:
    nsImapServerResponseParser(nsImapProtocol &imapConnection);
    virtual ~nsImapServerResponseParser();

	// Overridden from the base parser class
	virtual PRBool     LastCommandSuccessful();
    virtual void		HandleMemoryFailure();

    virtual void		ParseIMAPServerResponse(const char *currentCommand);
	virtual void		InitializeState();
    PRBool				CommandFailed();
    
    enum eIMAPstate {
        kNonAuthenticated,
        kAuthenticated,
#if 0
        kFolderSelected,
        kWaitingForMoreClientInput	// This shouldn't be a server state. It should only be a status.
#else
		kFolderSelected
#endif
    } ;

    virtual eIMAPstate GetIMAPstate();
	virtual PRBool WaitingForMoreClientInput() { return fWaitingForMoreClientInput; };
    
    const char *GetSelectedMailboxName();   // can be NULL

	// if we get a PREAUTH greeting from the server, initialize the parser to begin in
	// the kAuthenticated state
    void		PreauthSetAuthenticatedState();

    // these functions represent the state of the currently selected
    // folder
    PRBool     CurrentFolderReadOnly();
    PRInt32         NumberOfMessages();
    PRInt32         NumberOfRecentMessages();
    PRInt32         NumberOfUnseenMessages();
    PRInt32       FolderUID();
    PRUint32      CurrentResponseUID();
    PRUint32      HighestRecordedUID();
	PRBool		IsNumericString(const char *string);
   PRInt32       SizeOfMostRecentMessage();
	void		SetTotalDownloadSize(PRInt32 newSize) { fTotalDownloadSize = newSize; }
    
    nsImapSearchResultIterator *CreateSearchResultIterator();
    void ResetSearchResultSequence() {fSearchResults->ResetSequence();}
    
    // create a struct mailbox_spec from our info, used in
    // libmsg c interface
    struct mailbox_spec *CreateCurrentMailboxSpec(const char *mailboxName = NULL);
    
    // zero stops a list recording of flags and causes the flags for
    // each individual message to be sent back to libmsg 
    void ResetFlagInfo(int numberOfInterestingMessages);
    
    // set this to false if you don't want to alert the user to server 
    // error messages
    void SetReportingErrors(PRBool reportThem) { fReportingErrors=reportThem;}
	PRBool GetReportingErrors() { return fReportingErrors; }

	PRUint32 GetCapabilityFlag() { return fCapabilityFlag; }
	void   SetCapabilityFlag(PRUint32 capability) {fCapabilityFlag = capability;}
	PRBool ServerHasIMAP4Rev1Capability() { return ((fCapabilityFlag & kIMAP4rev1Capability) != 0); }
	PRBool ServerHasACLCapability() { return ((fCapabilityFlag & kACLCapability) != 0); }
	PRBool ServerHasNamespaceCapability() { return ((fCapabilityFlag & kNamespaceCapability) != 0); }
	PRBool ServerIsNetscape3xServer() { return fServerIsNetscape3xServer; }
	PRBool ServerHasServerInfo() {return ((fCapabilityFlag & kXServerInfoCapability) != 0); }
	void ResetCapabilityFlag() ;

	const char *GetMailAccountUrl() { return fMailAccountUrl; }
	const char *GetXSenderInfo() { return fXSenderInfo; }
	void FreeXSenderInfo() { PR_FREEIF(fXSenderInfo); }
	const char *GetManageListsUrl() { return fManageListsUrl; }
	const char *GetManageFiltersUrl() {return fManageFiltersUrl;}
	const char *GetManageFolderUrl() {return fFolderAdminUrl;}


	nsImapFlagAndUidState     *GetCurrentFlagState() { return fFlagState; }

    // Call this when adding a pipelined command to the session
    void IncrementNumberOfTaggedResponsesExpected(const char *newExpectedTag);
    
	// Interrupt a Fetch, without really Interrupting (through netlib)
	PRBool GetLastFetchChunkReceived(); 
	void ClearLastFetchChunkReceived(); 
	virtual PRUint16	SupportsUserFlags() { return fSupportsUserDefinedFlags; };
    virtual PRUint16  SettablePermanentFlags() { return fSettablePermanentFlags;};
	void SetFlagState(nsImapFlagAndUidState *state);

	PRBool GetFillingInShell();
	void	UseCachedShell(nsIMAPBodyShell *cachedShell);

protected:
    virtual void    flags();
    virtual void    response_data();
    virtual void    resp_text();
    virtual void    resp_cond_state();
    virtual void    text_mime2();
    virtual void    text();
    virtual void    resp_text_code();
    virtual void    response_done();
    virtual void    response_tagged();
    virtual void    response_fatal();
    virtual void    resp_cond_bye();
    virtual void    mailbox_data();
    virtual void    numeric_mailbox_data();
    virtual void    capability_data();
	virtual void	xserverinfo_data();
	virtual void	xmailboxinfo_data();
	virtual void	namespace_data();
	virtual void	myrights_data();
	virtual void	acl_data();
	virtual void	bodystructure_data();
	virtual void	mime_data();
	virtual void	mime_part_data();
	virtual void	mime_header_data();
    virtual void    msg_fetch();
    virtual void    msg_obsolete();
	virtual void	msg_fetch_headers(const char *partNum);
    virtual void    msg_fetch_content(PRBool chunk, PRInt32 origin, const char *content_type);
    virtual PRBool	msg_fetch_quoted(PRBool chunk, PRInt32 origin);
    virtual PRBool	msg_fetch_literal(PRBool chunk, PRInt32 origin);
    virtual void    mailbox_list(PRBool discoveredFromLsub);
    virtual void    mailbox(mailbox_spec *boxSpec);
    
    virtual void        ProcessOkCommand(const char *commandToken);
    virtual void        ProcessBadCommand(const char *commandToken);
    virtual void        PreProcessCommandToken(const char *commandToken,
                                               const char *currentCommand);
    virtual void		PostProcessEndOfLine();

	// Overridden from the nsIMAPGenericParser, to retrieve the next line
	// from the open socket.
	virtual PRBool		GetNextLineForParser(char **nextLine);
	virtual void    end_of_line();

	nsIMAPHostSessionList *GetHostSessionList();
private:
    PRBool                   fProcessingTaggedResponse;
    PRBool                   fCurrentCommandFailed;
    PRBool					  fReportingErrors;
    
    
    PRBool                   fCurrentFolderReadOnly;
    
    PRBool                   fCurrentLineContainedFlagInfo;
    imapMessageFlagsType	  fSavedFlagInfo;


	PRUint16					  fSupportsUserDefinedFlags;
    PRUint16                    fSettablePermanentFlags;

    PRInt32                     fFolderUIDValidity;
    PRInt32                     fNumberOfUnseenMessages;
    PRInt32                     fNumberOfExistingMessages;
    PRInt32                     fNumberOfRecentMessages;
    PRUint32                    fCurrentResponseUID;
    PRUint32                    fHighestRecordedUID;
    PRInt32                     fSizeOfMostRecentMessage;
	PRInt32					  fTotalDownloadSize;
    
    int						  fNumberOfTaggedResponsesExpected;
    
    char                     *fCurrentCommandTag;
    
    nsString2					 fZeroLengthMessageUidString;

    char                     *fSelectedMailboxName;
    
    nsImapSearchResultSequence    *fSearchResults;
    
    nsImapFlagAndUidState     *fFlagState;		// NOT owned by us, it's a copy, do not destroy
    
    eIMAPstate               fIMAPstate;
	PRBool					 fWaitingForMoreClientInput;

	PRUint32					  fCapabilityFlag; 
	char					 *fMailAccountUrl;
	char					 *fNetscapeServerVersionString;
	char					 *fXSenderInfo; /* changed per message download */
	char					 *fLastAlert;	/* used to avoid displaying the same alert over and over */
	char					 *fManageListsUrl;
	char					 *fManageFiltersUrl;
	char					 *fFolderAdminUrl;
	
	// used for index->uid mapping
	PRBool fCurrentCommandIsSingleMessageFetch;
	PRInt32	fUidOfSingleMessageFetch;
	PRInt32	fFetchResponseIndex;

	// used for aborting a fetch stream when we're pseudo-Interrupted
	PRBool fDownloadingHeaders;
	PRInt32 numberOfCharsInThisChunk;
	PRInt32 charsReadSoFar;
	PRBool fLastChunk;

	// Is the server a Netscape 3.x Messaging Server?
	PRBool	fServerIsNetscape3xServer;

	// points to the current body shell, if any
	nsIMAPBodyShell			*m_shell;

	// The connection object
    nsImapProtocol &fServerConnection;

	nsIMAPHostSessionList *fHostSessionList;
};

#endif
