/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#include "msgCore.h"  // for pre-compiled headers
#include "net.h" /* should be defined into msgCore.h? - need MESSAGE_RFC822 */

#include "nsImapCore.h"
#include "nsImapProtocol.h"
#include "nsImapServerResponseParser.h"
#include "nsIMAPBodyShell.h"
#include "nsImapFlagAndUidState.h"
#include "nsIMAPNamespace.h"
////////////////// nsImapServerResponseParser /////////////////////////

extern PRLogModuleInfo* IMAP;

nsImapServerResponseParser::nsImapServerResponseParser(nsImapProtocol &imapProtocolConnection) :
	nsIMAPGenericParser(),
	fServerConnection(imapProtocolConnection),
	fCurrentCommandTag(nil),
	fCurrentFolderReadOnly(PR_FALSE),
	fNumberOfExistingMessages(0),
	fNumberOfRecentMessages(0),
	fNumberOfUnseenMessages(0),
	fSizeOfMostRecentMessage(0),
	fTotalDownloadSize(0),
	fIMAPstate(kNonAuthenticated),
	fSelectedMailboxName(nil),
	fFlagState(nil),
	fCurrentLineContainedFlagInfo(PR_FALSE),
	fReportingErrors(PR_TRUE),
	fLastChunk(PR_FALSE),
	fServerIsNetscape3xServer(PR_FALSE),
	m_shell(nsnull),
	fHostSessionList(nsnull)
{
	fSearchResults = nsImapSearchResultSequence::CreateSearchResultSequence();
	fMailAccountUrl = nsnull;
	fManageFiltersUrl = nsnull;
	fManageListsUrl = nsnull;
	fFolderAdminUrl = nsnull;
	fNetscapeServerVersionString = nsnull;
	fXSenderInfo = nsnull;
	fSupportsUserDefinedFlags = 0;
	fSettablePermanentFlags = 0;
	fCapabilityFlag = kCapabilityUndefined; 
	fLastAlert = nsnull;
}

nsImapServerResponseParser::~nsImapServerResponseParser()
{
	PR_FREEIF( fCurrentCommandTag );
	//delete fFlagState;	// not our object
	delete fSearchResults; 
	PR_FREEIF( fMailAccountUrl );
	PR_FREEIF( fFolderAdminUrl );
	PR_FREEIF( fNetscapeServerVersionString );
	PR_FREEIF( fXSenderInfo );
	PR_FREEIF( fLastAlert );
	PR_FREEIF( fManageListsUrl );
	PR_FREEIF( fManageFiltersUrl );
	PR_FREEIF( fSelectedMailboxName );

    NS_IF_RELEASE (fHostSessionList);
}

PRBool nsImapServerResponseParser::LastCommandSuccessful()
{
	return (!CommandFailed() && 
		!fServerConnection.DeathSignalReceived() &&
		nsIMAPGenericParser::LastCommandSuccessful());
}

// returns PR_TRUE if things look ok to continue
PRBool nsImapServerResponseParser::GetNextLineForParser(char **nextLine)
{
	PRBool rv = PR_TRUE;
	*nextLine = fServerConnection.CreateNewLineFromSocket();
	if (fServerConnection.DeathSignalReceived() || (fServerConnection.GetConnectionStatus() <= 0))
		rv = PR_FALSE;
	// we'd really like to try to silently reconnect, but we shouldn't put this
	// message up just in the interrupt case
	if (fServerConnection.GetConnectionStatus() <= 0 && !fServerConnection.DeathSignalReceived())
		fServerConnection.AlertUserEventUsingId(kImapServerDisconnected);
	return rv;
}

// This should probably be in the base class...
void nsImapServerResponseParser::end_of_line()
{
	// if current commands failed, don't reset the lex analyzer
	// we need the info for the user
	if (!at_end_of_line())
		SetSyntaxError(PR_TRUE);
	else if (fProcessingTaggedResponse && !fCurrentCommandFailed)
		ResetLexAnalyzer();	// no more tokens until we send a command
	else if (!fCurrentCommandFailed)
		fNextToken = GetNextToken();
}

PRBool	nsImapServerResponseParser::CommandFailed()
{
	return fCurrentCommandFailed;
}

void nsImapServerResponseParser::SetFlagState(nsImapFlagAndUidState *state)
{
	fFlagState = state;
}

PRInt32 nsImapServerResponseParser::SizeOfMostRecentMessage()
{
	return fSizeOfMostRecentMessage;
}

// Call this when adding a pipelined command to the session
void nsImapServerResponseParser::IncrementNumberOfTaggedResponsesExpected(const char *newExpectedTag)
{
	fNumberOfTaggedResponsesExpected++;
	PR_FREEIF( fCurrentCommandTag );
	fCurrentCommandTag = PL_strdup(newExpectedTag);
	if (!fCurrentCommandTag)
		HandleMemoryFailure();
}
/* 
 response        ::= *response_data response_done
*/


void nsImapServerResponseParser::InitializeState()
{
	fProcessingTaggedResponse = PR_FALSE;
	fCurrentCommandFailed 	  = PR_FALSE;
}

void nsImapServerResponseParser::ParseIMAPServerResponse(const char *currentCommand)
{

	// Reinitialize the parser
	SetConnected(PR_TRUE);
	SetSyntaxError(PR_FALSE);

	// Reinitialize our state
	InitializeState();
	
	// the default is to not pipeline
	fNumberOfTaggedResponsesExpected = 1;
	int numberOfTaggedResponsesReceived = 0;

	char *copyCurrentCommand = PL_strdup(currentCommand);
	if (copyCurrentCommand && !fServerConnection.DeathSignalReceived())
	{
		char *placeInTokenString = nsnull;
		char *tagToken           = Imapstrtok_r(copyCurrentCommand, WHITESPACE,&placeInTokenString);
		char *commandToken       = Imapstrtok_r(nil, WHITESPACE,&placeInTokenString);
		if (tagToken)
		{
			PR_FREEIF( fCurrentCommandTag );
			fCurrentCommandTag = PL_strdup(tagToken);
			if (!fCurrentCommandTag)
				HandleMemoryFailure();
		}
		
		if (commandToken && ContinueParse())
			PreProcessCommandToken(commandToken, currentCommand);
		
		if (ContinueParse())
		{
			// Clears any syntax error lines
			SetSyntaxError(PR_FALSE);
			ResetLexAnalyzer();
			
			do {
				fNextToken = GetNextToken();
				while (ContinueParse() && !PL_strcmp(fNextToken, "*") )
				{
					response_data();
				}
				
				if (*fNextToken == '+')	// never pipeline APPEND or AUTHENTICATE
				{
					NS_ASSERTION((fNumberOfTaggedResponsesExpected - numberOfTaggedResponsesReceived) == 1, 
						" didn't get the number of tagged responses we expected");
					numberOfTaggedResponsesReceived = fNumberOfTaggedResponsesExpected;
				}
				else
					numberOfTaggedResponsesReceived++;
					
				if (numberOfTaggedResponsesReceived < fNumberOfTaggedResponsesExpected)
				{
					response_tagged();
					fProcessingTaggedResponse = PR_FALSE;
				}
				
			} while (ContinueParse() && (numberOfTaggedResponsesReceived < fNumberOfTaggedResponsesExpected));
			
			// check and see if the server is waiting for more input
			if (*fNextToken == '+')
			{
				fWaitingForMoreClientInput = PR_TRUE;
			}
			else
			{
				if (ContinueParse())
					response_done();
				
				if (ContinueParse() && !CommandFailed())
				{
					// a sucessful command may change the eIMAPstate
					ProcessOkCommand(commandToken);
				}
				else if (CommandFailed())
				{
					// a failed command may change the eIMAPstate
					ProcessBadCommand(commandToken);
					if (fReportingErrors)
						fServerConnection.AlertUserEventFromServer(fCurrentLine);
				}
			}
		}
	}
	else if (!fServerConnection.DeathSignalReceived())
		HandleMemoryFailure();

	PR_FREEIF(copyCurrentCommand);
}

void nsImapServerResponseParser::HandleMemoryFailure()
{
#ifdef DEBUG_chrisf
	XP_ASSERT(PR_FALSE);
#endif
	fServerConnection.AlertUserEventUsingId(kImapOutOfMemory);
	nsIMAPGenericParser::HandleMemoryFailure();
}


// SEARCH is the only command that requires pre-processing for now.
// others will be added here.
void nsImapServerResponseParser::PreProcessCommandToken(const char *commandToken,
										      const char *currentCommand)
{
	fCurrentCommandIsSingleMessageFetch = PR_FALSE;
	fWaitingForMoreClientInput = PR_FALSE;
	
	if (!PL_strcasecmp(commandToken, "SEARCH"))
		fSearchResults->ResetSequence();
	else if (!PL_strcasecmp(commandToken, "SELECT") && currentCommand)
	{
		// the mailbox name must be quoted, so strip the quotes
		const char *openQuote = PL_strstr(currentCommand, "\"");
		NS_ASSERTION(openQuote, "expected open quote in imap server response");
		if (!openQuote)
        { // ill formed select command
            openQuote = PL_strstr(currentCommand, " ");
        }
		PR_FREEIF( fSelectedMailboxName);
		fSelectedMailboxName = PL_strdup(openQuote + 1);
		if (fSelectedMailboxName)
		{
			// strip the escape chars and the ending quote
			char *currentChar = fSelectedMailboxName;
			while (*currentChar)
			{
				if (*currentChar == '\\')
				{
					PL_strcpy(currentChar, currentChar+1);
					currentChar++;	// skip what we are escaping
				}
				else if (*currentChar == '\"')
					*currentChar = 0;	// end quote
				else
					currentChar++;
			}
		}
		else
			HandleMemoryFailure();
		
		// we don't want bogus info for this new box
		//delete fFlagState;	// not our object
		//fFlagState = nsnull;
	}
	else if (!PL_strcasecmp(commandToken, "CLOSE"))
	{
		return;	// just for debugging
		// we don't want bogus info outside the selected state
		//delete fFlagState;	// not our object
		//fFlagState = nsnull;
	}
	else if (!PL_strcasecmp(commandToken, "UID"))
	{
	
		char *copyCurrentCommand = PL_strdup(currentCommand);
		if (copyCurrentCommand && !fServerConnection.DeathSignalReceived())
		{
			char *placeInTokenString = nsnull;
			char *tagToken           = Imapstrtok_r(copyCurrentCommand, WHITESPACE,&placeInTokenString);
			char *uidToken           = Imapstrtok_r(nil, WHITESPACE,&placeInTokenString);
			char *fetchToken         = Imapstrtok_r(nil, WHITESPACE,&placeInTokenString);
			
			if (!PL_strcasecmp(fetchToken, "FETCH") )
			{
				char *uidStringToken = Imapstrtok_r(nil, WHITESPACE, &placeInTokenString);
				if (!PL_strchr(uidStringToken, ',') && !PL_strchr(uidStringToken, ':'))	// , and : are uid delimiters
				{
					fCurrentCommandIsSingleMessageFetch = PR_TRUE;
					fUidOfSingleMessageFetch = atoi(uidStringToken);
				}
			}
			PR_Free(copyCurrentCommand);
		}
	}
}

const char *nsImapServerResponseParser::GetSelectedMailboxName()
{
	return fSelectedMailboxName;
}

nsImapSearchResultIterator *nsImapServerResponseParser::CreateSearchResultIterator()
{
	return new nsImapSearchResultIterator(*fSearchResults);
}

nsImapServerResponseParser::eIMAPstate nsImapServerResponseParser::GetIMAPstate()
{
	return fIMAPstate;
}

void nsImapServerResponseParser::PreauthSetAuthenticatedState()
{
	fIMAPstate = kAuthenticated;
}

void nsImapServerResponseParser::ProcessOkCommand(const char *commandToken)
{
	if (!PL_strcasecmp(commandToken, "LOGIN") ||
		!PL_strcasecmp(commandToken, "AUTHENTICATE"))
		fIMAPstate = kAuthenticated;
	else if (!PL_strcasecmp(commandToken, "LOGOUT"))
		fIMAPstate = kNonAuthenticated;
	else if (!PL_strcasecmp(commandToken, "SELECT") ||
	         !PL_strcasecmp(commandToken, "EXAMINE"))
		fIMAPstate = kFolderSelected;
	else if (!PL_strcasecmp(commandToken, "CLOSE"))
		fIMAPstate = kAuthenticated;
	else if ((!PL_strcasecmp(commandToken, "LIST")) ||
			 (!PL_strcasecmp(commandToken, "LSUB")))
	{
		//fServerConnection.MailboxDiscoveryFinished();
		// This used to be reporting that we were finished
		// discovering folders for each time we issued a
		// LIST or LSUB.  So if we explicitly listed the
		// INBOX, or Trash, or namespaces, we would get multiple
		// "done" states, even though we hadn't finished.
		// Move this to be called from the connection object
		// itself.
	}
	else if (!PL_strcasecmp(commandToken, "FETCH"))
	{
		if (fZeroLengthMessageUidString.Length())
		{
			// "Deleting zero length message");
			fServerConnection.Store(fZeroLengthMessageUidString, "+Flags (\\Deleted)", PR_TRUE);
			if (LastCommandSuccessful())
				fServerConnection.Expunge();

			fZeroLengthMessageUidString.Truncate();
		}
	}
	if (GetFillingInShell())
	{
		// There is a BODYSTRUCTURE response.  Now let's generate the stream...
		// that is, if we're not doing it already
		if (!m_shell->IsBeingGenerated())
		{
			nsImapProtocol *navCon = &fServerConnection;

			char *imapPart = nsnull;

			fServerConnection.GetCurrentUrl()->GetImapPartToFetch(&imapPart);
			if (imapPart)
				m_shell->Generate(imapPart);
			PR_FREEIF(imapPart);

			if ((navCon && navCon->GetPseudoInterrupted())
				|| fServerConnection.DeathSignalReceived())
			{
				// we were pseudointerrupted or interrupted
				if (!m_shell->IsShellCached())
				{
					// if it's not in the cache, then we were (pseudo)interrupted while generating
					// for the first time.  Delete it.
					delete m_shell;
				}
				navCon->PseudoInterrupt(PR_FALSE);
			}
			else if (m_shell->GetIsValid())
			{
				// If we have a valid shell that has not already been cached, then cache it.
				if (!m_shell->IsShellCached() && fHostSessionList)	// cache is responsible for destroying it
				{
					PR_LOG(IMAP, PR_LOG_ALWAYS, 
                           ("BODYSHELL:  Adding shell to cache."));
                    char *userName = fServerConnection.GetImapUserName();
					fHostSessionList->AddShellToCacheForHost(
                        fServerConnection.GetImapHostName(), userName,
                        m_shell);
                    PR_FREEIF(userName);
				}
			}
			else
			{
				// The shell isn't valid, so we don't cache it.
				// Therefore, we have to destroy it here.
				delete m_shell;
			}
			m_shell = nsnull;
		}
	}
}

void nsImapServerResponseParser::ProcessBadCommand(const char *commandToken)
{
	if (!PL_strcasecmp(commandToken, "LOGIN") ||
		!PL_strcasecmp(commandToken, "AUTHENTICATE"))
		fIMAPstate = kNonAuthenticated;
	else if (!PL_strcasecmp(commandToken, "LOGOUT"))
		fIMAPstate = kNonAuthenticated;	// ??
	else if (!PL_strcasecmp(commandToken, "SELECT") ||
	         !PL_strcasecmp(commandToken, "EXAMINE"))
		fIMAPstate = kAuthenticated;	// nothing selected
	else if (!PL_strcasecmp(commandToken, "CLOSE"))
		fIMAPstate = kAuthenticated;	// nothing selected
	if (GetFillingInShell())
	{
		if (!m_shell->IsBeingGenerated())
		{
			delete m_shell;
			m_shell = nsnull;
		}
	}
}
/*
 response_data   ::= "*" SPACE (resp_cond_state / resp_cond_bye /
                              mailbox_data / message_data / capability_data)
                              CRLF
 
 The RFC1730 grammar spec did not allow one symbol look ahead to determine
 between mailbox_data / message_data so I combined the numeric possibilities
 of mailbox_data and all of message_data into numeric_mailbox_data.
 
 The production implemented here is 

 response_data   ::= "*" SPACE (resp_cond_state / resp_cond_bye /
                              mailbox_data / numeric_mailbox_data / 
                              capability_data)
                              CRLF

Instead of comparing lots of strings and make function calls, try to pre-flight
the possibilities based on the first letter of the token.

*/
void nsImapServerResponseParser::response_data()
{
	fNextToken = GetNextToken();
	
	if (ContinueParse())
	{
		switch (toupper(fNextToken[0]))
		{
		case 'O':			// OK
			if (toupper(fNextToken[1]) == 'K')
				resp_cond_state();
			else SetSyntaxError(PR_TRUE);
			break;
		case 'N':			// NO
			if (toupper(fNextToken[1]) == 'O')
				resp_cond_state();
			else if (!PL_strcasecmp(fNextToken, "NAMESPACE"))
				namespace_data();
			else SetSyntaxError(PR_TRUE);
			break;
		case 'B':			// BAD
			if (!PL_strcasecmp(fNextToken, "BAD"))
				resp_cond_state();
			else if (!PL_strcasecmp(fNextToken, "BYE"))
				resp_cond_bye();
			else SetSyntaxError(PR_TRUE);
			break;
		case 'F':
			if (!PL_strcasecmp(fNextToken, "FLAGS"))
				mailbox_data();
			else SetSyntaxError(PR_TRUE);
			break;
		case 'P':
			if (PL_strcasecmp(fNextToken, "PERMANENTFLAGS"))
				mailbox_data();
			else SetSyntaxError(PR_TRUE);
			break;
		case 'L':
			if (!PL_strcasecmp(fNextToken, "LIST")  || !PL_strcasecmp(fNextToken, "LSUB"))
				mailbox_data();
			else SetSyntaxError(PR_TRUE);
			break;
		case 'M':
			if (!PL_strcasecmp(fNextToken, "MAILBOX"))
				mailbox_data();
			else if (!PL_strcasecmp(fNextToken, "MYRIGHTS"))
				myrights_data();
			else SetSyntaxError(PR_TRUE);
			break;
		case 'S':
			if (!PL_strcasecmp(fNextToken, "SEARCH"))
				mailbox_data();
			else if (!PL_strcasecmp(fNextToken, "STATUS"))
			{
				PRBool gotMailboxName = PR_FALSE;
				while (	ContinueParse() &&
						!at_end_of_line() )
				{
					fNextToken = GetNextToken();
					if (!fNextToken)
						break;

					if (!gotMailboxName)	// if we haven't got the mailbox name, get it
					{
						// this couldn't be more bogus, but I don't know how to parse the status response.
						// I need to find the next open parenthesis, and it looks like folder names with
						// parentheses are quoted...but not ones with spaces...
						fCurrentTokenPlaceHolder = PL_strchr(fCurrentTokenPlaceHolder, '(');

						gotMailboxName = PR_TRUE;
						continue;
					}

					if (*fNextToken == '(') fNextToken++;
					if (!PL_strcasecmp(fNextToken, "UIDNEXT"))
					{
						fNextToken = GetNextToken();
						if (fNextToken)
						{
							fCurrentResponseUID = atoi(fNextToken);
							// if this token ends in ')', then it is the last token
							// else we advance
							if ( *(fNextToken + nsCRT::strlen(fNextToken) - 1) == ')')
								fNextToken += nsCRT::strlen(fNextToken) - 1;
						}
					}
					else if (!PL_strcasecmp(fNextToken, "MESSAGES"))
					{
						fNextToken = GetNextToken();
						if (fNextToken)
						{
							fNumberOfExistingMessages = atoi(fNextToken);
							// if this token ends in ')', then it is the last token
							// else we advance
							if ( *(fNextToken + nsCRT::strlen(fNextToken) - 1) == ')')
								fNextToken += nsCRT::strlen(fNextToken) - 1;
						}
					}
					else if (!PL_strcasecmp(fNextToken, "UNSEEN"))
					{
						fNextToken = GetNextToken();
						if (fNextToken)
						{
							fNumberOfUnseenMessages = atoi(fNextToken);
							// if this token ends in ')', then it is the last token
							// else we advance
							if ( *(fNextToken + nsCRT::strlen(fNextToken) - 1) == ')')
								fNextToken += nsCRT::strlen(fNextToken) - 1;
						}
					}
					else if (*fNextToken == ')')
						break;
					else if (!at_end_of_line())
						SetSyntaxError(PR_TRUE);
				} 
			} else SetSyntaxError(PR_TRUE);
			break;
		case 'C':
			if (!PL_strcasecmp(fNextToken, "CAPABILITY"))
				capability_data();
			else SetSyntaxError(PR_TRUE);
			break;
		case 'V':
			if (!PL_strcasecmp(fNextToken, "VERSION"))
			{
				// figure out the version of the Netscape server here
				PR_FREEIF(fNetscapeServerVersionString);
				fNextToken = GetNextToken();
				if (! fNextToken) 
					SetSyntaxError(PR_TRUE);
				else
				{
					fNetscapeServerVersionString = CreateAstring();
					fNextToken = GetNextToken();
					if (fNetscapeServerVersionString)
					{
						fServerIsNetscape3xServer = (*fNetscapeServerVersionString == '3');
					}
				}
				skip_to_CRLF();
			}
			else SetSyntaxError(PR_TRUE);
			break;
		case 'A':
			if (!PL_strcasecmp(fNextToken, "ACL"))
			{
				acl_data();
			}
			else if (!PL_strcasecmp(fNextToken, "ACCOUNT-URL"))
			{
				PR_FREEIF(fMailAccountUrl);
				fNextToken = GetNextToken();
				if (! fNextToken) 
					SetSyntaxError(PR_TRUE);
				else
				{
					fMailAccountUrl = CreateAstring();
					fNextToken = GetNextToken();
				}
			} 
			else SetSyntaxError(PR_TRUE);
			break;
		case 'X':
			if (!PL_strcasecmp(fNextToken, "XSERVERINFO"))
				xserverinfo_data();
			else if (!PL_strcasecmp(fNextToken, "XMAILBOXINFO"))
				xmailboxinfo_data();
			else 
				SetSyntaxError(PR_TRUE);
			break;
		default:
			if (IsNumericString(fNextToken))
				numeric_mailbox_data();
			else
				SetSyntaxError(PR_TRUE);
			break;
		}
		
		if (ContinueParse())
		{
			PostProcessEndOfLine();
			end_of_line();
		}
	}
}


void nsImapServerResponseParser::PostProcessEndOfLine()
{
	// for now we only have to do one thing here
	// a fetch response to a 'uid store' command might return the flags
	// before it returns the uid of the message.  So we need both before
	// we report the new flag info to the front end
	
	// also check and be sure that there was a UID in the current response
	if (fCurrentLineContainedFlagInfo && CurrentResponseUID())
	{
		fCurrentLineContainedFlagInfo = PR_FALSE;
		fServerConnection.NotifyMessageFlags(fSavedFlagInfo, CurrentResponseUID());
	}
}


/*
 mailbox_data    ::=  "FLAGS" SPACE flag_list /
                               "LIST" SPACE mailbox_list /
                               "LSUB" SPACE mailbox_list /
                               "MAILBOX" SPACE text /
                               "SEARCH" [SPACE 1#nz_number] /
                               number SPACE "EXISTS" / number SPACE "RECENT"

This production was changed to accomodate predictive parsing 

 mailbox_data    ::=  "FLAGS" SPACE flag_list /
                               "LIST" SPACE mailbox_list /
                               "LSUB" SPACE mailbox_list /
                               "MAILBOX" SPACE text /
                               "SEARCH" [SPACE 1#nz_number]
*/
void nsImapServerResponseParser::mailbox_data()
{
	PRBool userDefined = PR_FALSE;
	PRBool perm = PR_FALSE;
	
	if (!PL_strcasecmp(fNextToken, "FLAGS")) {
		skip_to_CRLF();
	}
	else if (!PL_strcasecmp(fNextToken, "LIST"))
	{
		fNextToken = GetNextToken();
		if (ContinueParse())
			mailbox_list(PR_FALSE);
	}
	else if (!PL_strcasecmp(fNextToken, "LSUB"))
	{
		fNextToken = GetNextToken();
		if (ContinueParse())
			mailbox_list(PR_TRUE);
	}
	else if (!PL_strcasecmp(fNextToken, "MAILBOX"))
		skip_to_CRLF();
	else if (!PL_strcasecmp(fNextToken, "SEARCH"))
	{
		fSearchResults->AddSearchResultLine(fCurrentLine);
		fServerConnection.NotifySearchHit(fCurrentLine);
		skip_to_CRLF();
	}
}

/*
 mailbox_list    ::= "(" #("\Marked" / "\Noinferiors" /
                              "\Noselect" / "\Unmarked" / flag_extension) ")"
                              SPACE (<"> QUOTED_CHAR <"> / nil) SPACE mailbox
*/

void nsImapServerResponseParser::mailbox_list(PRBool discoveredFromLsub)
{
	mailbox_spec *boxSpec = (mailbox_spec *)PR_CALLOC(sizeof(mailbox_spec));
	if (!boxSpec)
		HandleMemoryFailure();
	else
	{
		boxSpec->folderSelected = PR_FALSE;
		boxSpec->box_flags = kNoFlags;
		boxSpec->allocatedPathName = nsnull;
		boxSpec->hostName = nsnull;
		boxSpec->connection = &fServerConnection;
		boxSpec->flagState = nsnull;
		boxSpec->discoveredFromLsub = discoveredFromLsub;
		boxSpec->onlineVerified = PR_TRUE;
		boxSpec->folderIsNamespace = PR_FALSE;
		
		PRBool endOfFlags = PR_FALSE;
		fNextToken++;	// eat the first "("
		do {
			if (!PL_strncasecmp(fNextToken, "\\Marked", 7))
				boxSpec->box_flags |= kMarked;	
			else if (!PL_strncasecmp(fNextToken, "\\Unmarked", 9))
				boxSpec->box_flags |= kUnmarked;	
			else if (!PL_strncasecmp(fNextToken, "\\Noinferiors", 12))
				boxSpec->box_flags |= kNoinferiors;	
			else if (!PL_strncasecmp(fNextToken, "\\Noselect", 9))
				boxSpec->box_flags |= kNoselect;	
			// we ignore flag extensions
			
			endOfFlags = *(fNextToken + nsCRT::strlen(fNextToken) - 1) == ')';
			fNextToken = GetNextToken();
		} while (!endOfFlags && ContinueParse());
		
		if (ContinueParse())
		{
			if (*fNextToken == '"')
			{
				fNextToken++;
				if (*fNextToken == '\\')	// handle escaped char
					boxSpec->hierarchySeparator = *(fNextToken + 1);
				else
					boxSpec->hierarchySeparator = *fNextToken;
			}
			else	// likely NIL.  Discovered late in 4.02 that we do not handle literals here (e.g. {10} <10 chars>), although this is almost impossibly unlikely
				boxSpec->hierarchySeparator = kOnlineHierarchySeparatorNil;
			fNextToken = GetNextToken();	
			if (ContinueParse())
				mailbox(boxSpec);
		}
	}
}

/* mailbox         ::= "INBOX" / astring
*/
void nsImapServerResponseParser::mailbox(mailbox_spec *boxSpec)
{
	char *boxname = nsnull;
    char *userName = fServerConnection.GetImapUserName();
    const char* hostName = fServerConnection.GetImapHostName();
	
	if (!PL_strcasecmp(fNextToken, "INBOX"))
	{
		boxname = PL_strdup("INBOX");
		fNextToken = GetNextToken();
	}
	else 
	{
		boxname = CreateAstring();
		// handle a literal ending the line here
		if (fTokenizerAdvanced)
		{
			fTokenizerAdvanced = PR_FALSE;
			if (!PL_strcmp(fCurrentTokenPlaceHolder, CRLF))
				fAtEndOfLine = PR_FALSE;
		}
		fNextToken = GetNextToken();
	}
	
    if (boxname && fHostSessionList)
    {
		// should the namespace check go before or after the Utf7 conversion?
		fHostSessionList->SetNamespaceHierarchyDelimiterFromMailboxForHost(
            hostName, userName, boxname, boxSpec->hierarchySeparator);

		
		nsIMAPNamespace *ns = nsnull;
		fHostSessionList->GetNamespaceForMailboxForHost(hostName, userName
                                                        ,boxname, ns);
		if (ns)
		{
			switch (ns->GetType())
			{
			case kPersonalNamespace:
				boxSpec->box_flags |= kPersonalMailbox;
				break;
			case kPublicNamespace:
				boxSpec->box_flags |= kPublicMailbox;
				break;
			case kOtherUsersNamespace:
				boxSpec->box_flags |= kOtherUsersMailbox;
				break;
			default:	// (kUnknownNamespace)
				break;
			}
			boxSpec->namespaceForFolder = ns;
		}

    	char *convertedName =
            fServerConnection.CreateUtf7ConvertedString(boxname, PR_FALSE);
    	PR_Free(boxname);
    	boxname = convertedName;
    }

	if (!boxname)
	{
		if (!fServerConnection.DeathSignalReceived())
			HandleMemoryFailure();
	}
	else
	{
		NS_ASSERTION(boxSpec->connection, "box spec has null connection");
		NS_ASSERTION(boxSpec->connection->GetCurrentUrl(), "box spec has connection with null url");
		//boxSpec->hostName = nsnull;
		//if (boxSpec->connection && boxSpec->connection->GetCurrentUrl())
		boxSpec->connection->GetCurrentUrl()->AllocateCanonicalPath(boxname, boxSpec->hierarchySeparator, &boxSpec->allocatedPathName);
		boxSpec->connection->GetCurrentUrl()->GetHost(&boxSpec->hostName);
		PR_FREEIF( boxname);
		// storage for the boxSpec is now owned by server connection
		fServerConnection.DiscoverMailboxSpec(boxSpec);
		
		// if this was cancelled by the user,then we sure don't want to
		// send more mailboxes their way
		if (fServerConnection.GetConnectionStatus() < 0)
			SetConnected(PR_FALSE);
	}
    PR_FREEIF(userName);
}


/*
 message_data    ::= nz_number SPACE ("EXPUNGE" /
                              ("FETCH" SPACE msg_fetch) / msg_obsolete)

was changed to

numeric_mailbox_data ::=  number SPACE "EXISTS" / number SPACE "RECENT"
 / nz_number SPACE ("EXPUNGE" /
                              ("FETCH" SPACE msg_fetch) / msg_obsolete)

*/
void nsImapServerResponseParser::numeric_mailbox_data()
{
	PRInt32 tokenNumber = atoi(fNextToken);
	fNextToken = GetNextToken();
	
	if (ContinueParse())
	{
		if (!PL_strcasecmp(fNextToken, "FETCH"))
		{
			fFetchResponseIndex = tokenNumber;
			fNextToken = GetNextToken();
			if (ContinueParse())
				msg_fetch(); 
		}
		else if (!PL_strcasecmp(fNextToken, "EXISTS"))
		{
			fNumberOfExistingMessages = tokenNumber;
			fNextToken = GetNextToken();
		}
		else if (!PL_strcasecmp(fNextToken, "RECENT"))
		{
			fNumberOfRecentMessages = tokenNumber;
			fNextToken = GetNextToken();
		}
		else if (!PL_strcasecmp(fNextToken, "EXPUNGE"))
		{
			fFlagState->ExpungeByIndex((PRUint32) tokenNumber);
			skip_to_CRLF();
		}
		else
			msg_obsolete();
	}
}

/*
 msg_fetch       ::= "(" 1#("BODY" SPACE body /
                              "BODYSTRUCTURE" SPACE body /
                              "BODY[" section "]" SPACE nstring /
                              "ENVELOPE" SPACE envelope /
                              "FLAGS" SPACE "(" #(flag / "\Recent") ")" /
                              "INTERNALDATE" SPACE date_time /
                              "RFC822" [".HEADER" / ".TEXT"] SPACE nstring /
                              "RFC822.SIZE" SPACE number /
                              "UID" SPACE uniqueid) ")"

*/

void nsImapServerResponseParser::msg_fetch()
{
	nsresult res;

	// we have not seen a uid response or flags for this fetch, yet
	fCurrentResponseUID = 0;
	fCurrentLineContainedFlagInfo = PR_FALSE;

	// show any incremental progress, for instance, for header downloading
	fServerConnection.ShowProgress();

	fNextToken++;	// eat the '(' character
	
	// some of these productions are ignored for now
	while (ContinueParse() && (*fNextToken != ')') )
	{
		if (!PL_strcasecmp(fNextToken, "FLAGS"))
		{
			if (fCurrentResponseUID == 0)
				res = fFlagState->GetUidOfMessage(fFetchResponseIndex - 1, &fCurrentResponseUID);

			fNextToken = GetNextToken();
			if (ContinueParse())
				flags();
			
			if (ContinueParse())
			{	// eat the closing ')'
				fNextToken++;
				// there may be another ')' to close out
				// msg_fetch.  If there is then don't advance
				if (*fNextToken != ')')
					fNextToken = GetNextToken();
			}
		}
		else if (!PL_strcasecmp(fNextToken, "UID"))
		{
			fNextToken = GetNextToken();
			if (ContinueParse())
			{
				fCurrentResponseUID = atoi(fNextToken);
				if (fCurrentResponseUID > fHighestRecordedUID)
					fHighestRecordedUID = fCurrentResponseUID;
				
				// if this token ends in ')', then it is the last token
				// else we advance
				if ( *(fNextToken + nsCRT::strlen(fNextToken) - 1) == ')')
					fNextToken += nsCRT::strlen(fNextToken) - 1;
				else
					fNextToken = GetNextToken();
			}
		}
		else if (!PL_strcasecmp(fNextToken, "RFC822") ||
				 !PL_strcasecmp(fNextToken, "RFC822.HEADER") ||
				 !PL_strncasecmp(fNextToken, "BODY[HEADER",11) ||
				 !PL_strncasecmp(fNextToken, "BODY[]", 6) ||
				 !PL_strcasecmp(fNextToken, "RFC822.TEXT") ||
				 (!PL_strncasecmp(fNextToken, "BODY[", 5) &&
				  PL_strstr(fNextToken, "HEADER"))
				 )
		{
			if (fCurrentResponseUID == 0)
				 fFlagState->GetUidOfMessage(fFetchResponseIndex - 1, &fCurrentResponseUID);

			if (!PL_strcasecmp(fNextToken, "RFC822.HEADER") ||
	 			!PL_strcasecmp(fNextToken, "BODY[HEADER]"))
			{
				// all of this message's headers
				fNextToken = GetNextToken();
				fDownloadingHeaders = PR_TRUE;
				if (ContinueParse())
					msg_fetch_headers(nsnull);
			}
			else if (!PL_strncasecmp(fNextToken, "BODY[HEADER.FIELDS",19))
			{
				fDownloadingHeaders = PR_TRUE;
				// specific message header fields
				while (ContinueParse() && fNextToken[nsCRT::strlen(fNextToken)-1] != ']')
					fNextToken = GetNextToken();
				if (ContinueParse())
				{
					fNextToken = GetNextToken();
					if (ContinueParse())
						msg_fetch_headers(nsnull);
				}
			}
			else
			{
				char *whereHeader = PL_strstr(fNextToken, "HEADER");
				if (whereHeader)
				{
					char *startPartNum = fNextToken + 5;
					if (whereHeader > startPartNum)
					{
						char *partNum = (char *)PR_CALLOC((whereHeader - startPartNum) * sizeof (char));
						if (partNum)
						{
							PL_strncpy(partNum, startPartNum, (whereHeader - startPartNum));
							if (ContinueParse())
							{
								if (PL_strstr(fNextToken, "FIELDS"))
								{
									while (ContinueParse() && fNextToken[nsCRT::strlen(fNextToken)-1] != ']')
										fNextToken = GetNextToken();
								}
								if (ContinueParse())
								{
									fNextToken = GetNextToken();
									if (ContinueParse())
										msg_fetch_headers(partNum);
								}
							}
							PR_Free(partNum);
						}
					}
					else
						SetSyntaxError(PR_TRUE);
				}
				else
				{
					fDownloadingHeaders = PR_FALSE;

					PRBool chunk = PR_FALSE;
					PRInt32 origin = 0;
					if (!PL_strncasecmp(fNextToken, "BODY[]<", 7))
					{
						char *tokenCopy = 0;
						tokenCopy = PL_strdup(fNextToken);
						if (tokenCopy)
						{
							char *originString = tokenCopy + 7;	// where the byte number starts
							char *closeBracket = PL_strchr(tokenCopy,'>');
							if (closeBracket && originString && *originString)
							{
								*closeBracket = 0;
								origin = atoi(originString);
								chunk = PR_TRUE;
							}
							PR_Free(tokenCopy);
						}
					}

					fNextToken = GetNextToken();
					if (ContinueParse())
					{
						msg_fetch_content(chunk, origin, MESSAGE_RFC822);
					}
				}
			}
		}
		else if (!PL_strcasecmp(fNextToken, "RFC822.SIZE"))
		{
			fNextToken = GetNextToken();
			if (ContinueParse())
			{
				fSizeOfMostRecentMessage = atoi(fNextToken);
				
				if (fSizeOfMostRecentMessage == 0 && CurrentResponseUID())
				{
					// on no, bogus Netscape 2.0 mail server bug
					char uidString[100];
					sprintf(uidString, "%ld", (long)CurrentResponseUID());
					
					if (!fZeroLengthMessageUidString.Length())
						fZeroLengthMessageUidString = uidString;
					else
					{
						fZeroLengthMessageUidString += ",";
						fZeroLengthMessageUidString += uidString;
					}
				}
				
				// if this token ends in ')', then it is the last token
				// else we advance
				if ( *(fNextToken + nsCRT::strlen(fNextToken) - 1) == ')')
					fNextToken += nsCRT::strlen(fNextToken) - 1;
				else
					fNextToken = GetNextToken();
			}
		}
		else if (!PL_strcasecmp(fNextToken, "XSENDER"))
		{
			PR_FREEIF(fXSenderInfo);
			fNextToken = GetNextToken();
			if (! fNextToken) 
				SetSyntaxError(PR_TRUE);
			else
			{
				fXSenderInfo = CreateAstring(); 
				fNextToken = GetNextToken();
			}
		}
		// I only fetch RFC822 so I should never see these BODY responses
		else if (!PL_strcasecmp(fNextToken, "BODY"))
			skip_to_CRLF(); // I never ask for this
		else if (!PL_strcasecmp(fNextToken, "BODYSTRUCTURE"))
		{
 			if (fCurrentResponseUID == 0)
 				fFlagState->GetUidOfMessage(fFetchResponseIndex - 1, &fCurrentResponseUID);
			bodystructure_data();
		}
		else if (!PL_strncasecmp(fNextToken, "BODY[", 5) && PL_strncasecmp(fNextToken, "BODY[]", 6))
		{
			fDownloadingHeaders = PR_FALSE;
			// A specific MIME part, or MIME part header
			mime_data();
		}
		else if (!PL_strcasecmp(fNextToken, "ENVELOPE"))
			skip_to_CRLF(); // I never ask for this
		else if (!PL_strcasecmp(fNextToken, "INTERNALDATE"))
			skip_to_CRLF(); // I will need to implement this
		else
			SetSyntaxError(PR_TRUE);

	}
	
	if (ContinueParse())
	{
		if (CurrentResponseUID() && fCurrentLineContainedFlagInfo && fFlagState)
		{
			fFlagState->AddUidFlagPair(CurrentResponseUID(), fSavedFlagInfo);
			fCurrentLineContainedFlagInfo = PR_FALSE;	// do not fire if in PostProcessEndOfLine
		}
		fCurrentLineContainedFlagInfo = PR_FALSE;	// do not fire if in PostProcessEndOfLine
			
		fNextToken = GetNextToken();	// eat the ')' ending token
										// should be at end of line
	}
}

void nsImapServerResponseParser::flags()
{
	imapMessageFlagsType messageFlags = kNoImapMsgFlag;
	
	// eat the opening '('
	fNextToken++;
						     
	while (ContinueParse() && (*fNextToken != ')'))
	{
		if (*fNextToken == '\\')
		{
			switch (toupper(fNextToken[1])) {
			case 'S':
				if (!PL_strncasecmp(fNextToken, "\\Seen",5))
					messageFlags |= kImapMsgSeenFlag;
				break;
			case 'A':
				if (!PL_strncasecmp(fNextToken, "\\Answered",9))
					messageFlags |= kImapMsgAnsweredFlag;
				break;
			case 'F':
				if (!PL_strncasecmp(fNextToken, "\\Flagged",8))
					messageFlags |= kImapMsgFlaggedFlag;
				break;
			case 'D':
				if (!PL_strncasecmp(fNextToken, "\\Deleted",8))
					messageFlags |= kImapMsgDeletedFlag;
				else if (!PL_strncasecmp(fNextToken, "\\Draft",6))
					messageFlags |= kImapMsgDraftFlag;
				break;
			case 'R':
				if (!PL_strncasecmp(fNextToken, "\\Recent",7))
					messageFlags |= kImapMsgRecentFlag;
				break;
			default:
				break;
			}
		}
		else if (*fNextToken == '$')
		{
			switch (toupper(fNextToken[1])) {
			case 'M':
				if ((fSupportsUserDefinedFlags & (kImapMsgSupportUserFlag |
												  kImapMsgSupportMDNSentFlag))
					&& !PL_strncasecmp(fNextToken, "$MDNSent",8))
					messageFlags |= kImapMsgMDNSentFlag;
				break;
			case 'F':
				if ((fSupportsUserDefinedFlags & (kImapMsgSupportUserFlag |
												  kImapMsgSupportForwardedFlag))
					 && !PL_strncasecmp(fNextToken, "$Forwarded",10))
				messageFlags |= kImapMsgForwardedFlag;
				break;
			default:
				break;
			}
		}
			
		if (PL_strcasestr(fNextToken, ")"))
		{
			// eat token chars until we get the ')'
			while (*fNextToken != ')')
				fNextToken++;
		}
		else
			fNextToken = GetNextToken();
	}
	
	if (ContinueParse())
		while(*fNextToken != ')')
			fNextToken++;
	
	fCurrentLineContainedFlagInfo = PR_TRUE;	// handled in PostProcessEndOfLine
	fSavedFlagInfo = messageFlags;
}
	
/* 
 resp_cond_state ::= ("OK" / "NO" / "BAD") SPACE resp_text
*/
void nsImapServerResponseParser::resp_cond_state()
{
	if ((!PL_strcasecmp(fNextToken, "NO") ||
	     !PL_strcasecmp(fNextToken, "BAD") ) &&
	    	fProcessingTaggedResponse)
		fCurrentCommandFailed = PR_TRUE;
	
	fNextToken = GetNextToken();
	if (ContinueParse())
		resp_text();
}

/*
 resp_text       ::= ["[" resp_text_code "]" SPACE] (text_mime2 / text)
 
 was changed to in order to enable a one symbol look ahead predictive
 parser.
 
 resp_text       ::= ["[" resp_text_code  SPACE] (text_mime2 / text)
 */
void nsImapServerResponseParser::resp_text()
{
	if (ContinueParse() && (*fNextToken == '['))
		resp_text_code();
		
	if (ContinueParse())
	{
		if (!PL_strcmp(fNextToken, "=?"))
			text_mime2();
		else
			text();
	}
}
/*
 text_mime2       ::= "=?" <charset> "?" <encoding> "?"
                               <encoded-text> "?="
                               ;; Syntax defined in [MIME-2]
*/
void nsImapServerResponseParser::text_mime2()
{
	skip_to_CRLF();
}

/*
 text            ::= 1*TEXT_CHAR

*/
void nsImapServerResponseParser::text()
{
	skip_to_CRLF();
}


/*
 resp_text_code  ::= "ALERT" / "PARSE" /
                              "PERMANENTFLAGS" SPACE "(" #(flag / "\*") ")" /
                              "READ-ONLY" / "READ-WRITE" / "TRYCREATE" /
                              "UIDVALIDITY" SPACE nz_number /
                              "UNSEEN" SPACE nz_number /
                              atom [SPACE 1*<any TEXT_CHAR except "]">]
                              
 was changed to in order to enable a one symbol look ahead predictive
 parser.
 
  resp_text_code  ::= ("ALERT" / "PARSE" /
                              "PERMANENTFLAGS" SPACE "(" #(flag / "\*") ")" /
                              "READ-ONLY" / "READ-WRITE" / "TRYCREATE" /
                              "UIDVALIDITY" SPACE nz_number /
                              "UNSEEN" SPACE nz_number /
                              atom [SPACE 1*<any TEXT_CHAR except "]">] )
                      "]"

 
*/
void nsImapServerResponseParser::resp_text_code()
{
	// this is a special case way of advancing the token
	// strtok won't break up "[ALERT]" into separate tokens
	if (nsCRT::strlen(fNextToken) > 1)
		fNextToken++;
	else 
		fNextToken = GetNextToken();
	
	if (ContinueParse())
	{
		if (!PL_strcasecmp(fNextToken,"ALERT]"))
		{
			char *alertMsg = fCurrentTokenPlaceHolder;	// advance past ALERT]
			if (alertMsg && *alertMsg && (!fLastAlert || PL_strcmp(fNextToken, fLastAlert)))
			{
				fServerConnection.AlertUserEvent(alertMsg);
				PR_FREEIF(fLastAlert);
				fLastAlert = PL_strdup(alertMsg);
			}
			fNextToken = GetNextToken();
		}
		else if (!PL_strcasecmp(fNextToken,"PARSE]"))
		{
			// do nothing for now
			fNextToken = GetNextToken();
		}
		else if (!PL_strcasecmp(fNextToken,"NETSCAPE]"))
		{
			skip_to_CRLF();
		}
		else if (!PL_strcasecmp(fNextToken,"PERMANENTFLAGS"))
		{
			// do nothing but eat tokens until we see the ] or CRLF
			// we should see the ] but we don't want to go into an
			// endless loop if the CRLF is not there

			fSupportsUserDefinedFlags = 0;		// assume no unless told
			do {
				fNextToken = GetNextToken();
				if (*fNextToken == '(') fNextToken++;
				if (!PL_strncasecmp(fNextToken, "$MDNSent", 8))
				{
					fSupportsUserDefinedFlags |= kImapMsgSupportMDNSentFlag;
				}
				else if (!PL_strncasecmp(fNextToken, "$Forwarded", 10))
				{
					fSupportsUserDefinedFlags |= kImapMsgSupportForwardedFlag;
				}
				else if (!PL_strncasecmp(fNextToken, "\\Seen", 5))
				{
					fSettablePermanentFlags |= kImapMsgSeenFlag;
				}
				else if (!PL_strncasecmp(fNextToken, "\\Answered", 9))
				{
					fSettablePermanentFlags |= kImapMsgAnsweredFlag;
				}
				else if (!PL_strncasecmp(fNextToken, "\\Flagged", 8))
				{
					fSettablePermanentFlags |= kImapMsgFlaggedFlag;
				}
				else if (!PL_strncasecmp(fNextToken, "\\Deleted", 8))
				{
					fSettablePermanentFlags |= kImapMsgDeletedFlag;
				}
				else if (!PL_strncasecmp(fNextToken, "\\Draft", 6))
				{
					fSettablePermanentFlags |= kImapMsgDraftFlag;
				}
				else if (!PL_strncasecmp(fNextToken, "\\*)]", 4))
				{
					 fSupportsUserDefinedFlags |= kImapMsgSupportUserFlag;
					 fSupportsUserDefinedFlags |= kImapMsgSupportForwardedFlag;
					 fSupportsUserDefinedFlags |= kImapMsgSupportMDNSentFlag;
				}
			} while (!at_end_of_line() && ContinueParse());
			if (GetCurrentFlagState())
				GetCurrentFlagState()->
					SetSupportedUserFlags(fSupportsUserDefinedFlags);

		}
		else if (!PL_strcasecmp(fNextToken,"READ-ONLY]"))
		{
			fCurrentFolderReadOnly = PR_TRUE;
			fNextToken = GetNextToken();
		}
		else if (!PL_strcasecmp(fNextToken,"READ-WRITE]"))
		{
			fCurrentFolderReadOnly = PR_FALSE;
			fNextToken = GetNextToken();
		}
		else if (!PL_strcasecmp(fNextToken,"TRYCREATE]"))
		{
			// do nothing for now
			fNextToken = GetNextToken();
		}
		else if (!PL_strcasecmp(fNextToken,"UIDVALIDITY"))
		{
			fNextToken = GetNextToken();
			if (ContinueParse())
			{
				fFolderUIDValidity = atoi(fNextToken);
				fHighestRecordedUID = 0;
				fNextToken = GetNextToken();
			}
		}
		else if (!PL_strcasecmp(fNextToken,"UNSEEN"))
		{
			fNextToken = GetNextToken();
			if (ContinueParse())
			{
				fNumberOfUnseenMessages = atoi(fNextToken);
				fNextToken = GetNextToken();
			}
		}
		else if (!PL_strcasecmp(fNextToken, "APPENDUID"))
		{
			fNextToken = GetNextToken();
			if (ContinueParse())
			{
				fFolderUIDValidity = atoi(fNextToken);
				fHighestRecordedUID = 0;
				fNextToken = GetNextToken();
				if (ContinueParse())
				{
					fCurrentResponseUID = atoi(fNextToken);
					fNextToken = GetNextToken();
				}
			}
		}
		else 	// just text
		{
			// do nothing but eat tokens until we see the ] or CRLF
			// we should see the ] but we don't want to go into an
			// endless loop if the CRLF is not there
			do {
				fNextToken = GetNextToken();
			} while (!PL_strcasestr(fNextToken, "]") && 
			         !at_end_of_line() &&
			         ContinueParse());
		}
	}
}

/*
 response_done   ::= response_tagged / response_fatal
 */
void nsImapServerResponseParser::response_done()
{
	if (ContinueParse())
	{
		if (!PL_strcmp(fCurrentCommandTag, fNextToken))
			response_tagged();
		else
			response_fatal();
	}
}

/*  response_tagged ::= tag SPACE resp_cond_state CRLF
*/
void nsImapServerResponseParser::response_tagged()
{
	// eat the tag
	fNextToken = GetNextToken();
	if (ContinueParse())
	{
		fProcessingTaggedResponse = PR_TRUE;
		resp_cond_state();
		if (ContinueParse())
			end_of_line();
	}
}

/* response_fatal  ::= "*" SPACE resp_cond_bye CRLF
*/
void nsImapServerResponseParser::response_fatal()
{
	// eat the "*"
	fNextToken = GetNextToken();
	if (ContinueParse())
	{
		resp_cond_bye();
		if (ContinueParse())
			end_of_line();
	}
}
/*
resp_cond_bye   ::= "BYE" SPACE resp_text
                              ;; Server will disconnect condition
                    */
void nsImapServerResponseParser::resp_cond_bye()
{
	SetConnected(PR_FALSE);
	fIMAPstate = kNonAuthenticated;
	skip_to_CRLF();
}


void nsImapServerResponseParser::msg_fetch_headers(const char *partNum)
{
	if (GetFillingInShell())
	{
		char *headerData = CreateAstring();
		fNextToken = GetNextToken();
		m_shell->AdoptMessageHeaders(headerData, partNum);
	}
	else
	{
		msg_fetch_content(PR_FALSE, 0, MESSAGE_RFC822);
	}
}


/* nstring         ::= string / nil
string          ::= quoted / literal
 nil             ::= "NIL"

*/
void nsImapServerResponseParser::msg_fetch_content(PRBool chunk, PRInt32 origin, const char *content_type)
{
	// setup the stream for downloading this message.
	// Don't do it if we are filling in a shell or downloading a part.
	// DO do it if we are downloading a whole message as a result of
	// an invalid shell trying to generate.
	if ((!chunk || (origin == 0)) && 
		(GetFillingInShell() ? m_shell->GetGeneratingWholeMessage() : PR_TRUE))
	{
		NS_ASSERTION(fSizeOfMostRecentMessage > 0, "most recent message has 0 or negative size");
		fServerConnection.BeginMessageDownLoad(fSizeOfMostRecentMessage, 
										   content_type);
	}

	if (PL_strcasecmp(fNextToken, "NIL"))
	{
		if (*fNextToken == '"')
			fLastChunk = msg_fetch_quoted(chunk, origin);
		else
			fLastChunk = msg_fetch_literal(chunk, origin);
	}
	else
		fNextToken = GetNextToken();	// eat "NIL"
	
	if (fLastChunk && (GetFillingInShell() ? m_shell->GetGeneratingWholeMessage() : PR_TRUE))
	{
		// complete the message download
		if (ContinueParse())
			fServerConnection.NormalMessageEndDownload();
		else
			fServerConnection.AbortMessageDownLoad();
	}
}


/*
 quoted          ::= <"> *QUOTED_CHAR <">

          QUOTED_CHAR     ::= <any TEXT_CHAR except quoted_specials> /
                              "\" quoted_specials

          quoted_specials ::= <"> / "\"
*/

PRBool nsImapServerResponseParser::msg_fetch_quoted(PRBool chunk, PRInt32 origin)
{

#ifdef DEBUG_chrisf
	 XP_ASSERT(!chunk);
#endif

	char *q = CreateQuoted();
	if (q)
	{
		fServerConnection.HandleMessageDownLoadLine(q, PR_FALSE);
		PRUint32 numberOfCharsInThisChunk = nsCRT::strlen(q);
		PR_Free(q);
	}

	fNextToken = GetNextToken();

	PRBool lastChunk = !chunk || ((origin + numberOfCharsInThisChunk) >= (unsigned int) fTotalDownloadSize);
	return lastChunk;
}
/* msg_obsolete    ::= "COPY" / ("STORE" SPACE msg_fetch)
                              ;; OBSOLETE untagged data responses */
void nsImapServerResponseParser::msg_obsolete()
{
	if (!PL_strcasecmp(fNextToken, "COPY"))
		fNextToken = GetNextToken();
	else if (!PL_strcasecmp(fNextToken, "STORE"))
	{
		fNextToken = GetNextToken();
		if (ContinueParse())
			msg_fetch();
	}
	else
		SetSyntaxError(PR_TRUE);
	
}
void nsImapServerResponseParser::capability_data()
{
	fCapabilityFlag = fCapabilityFlag | kCapabilityDefined;
	do {
		fNextToken = GetNextToken();
		// for now we only care about AUTH=LOGIN
		if (fNextToken) {
			if(! PL_strcasecmp(fNextToken, "AUTH=LOGIN"))
				fCapabilityFlag |= kHasAuthLoginCapability;
			else if (! PL_strcasecmp(fNextToken, "AUTH=PLAIN"))
				fCapabilityFlag |= kHasAuthPlainCapability;
			else if (! PL_strcasecmp(fNextToken, "X-NETSCAPE"))
				fCapabilityFlag |= kHasXNetscapeCapability;
			else if (! PL_strcasecmp(fNextToken, "XSENDER"))
				fCapabilityFlag |= kHasXSenderCapability;
			else if (! PL_strcasecmp(fNextToken, "IMAP4"))
				fCapabilityFlag |= kIMAP4Capability;
			else if (! PL_strcasecmp(fNextToken, "IMAP4rev1"))
				fCapabilityFlag |= kIMAP4rev1Capability;
			else if (! PL_strncasecmp(fNextToken, "IMAP4", 5))
				fCapabilityFlag |= kIMAP4other;
			else if (! PL_strcasecmp(fNextToken, "X-NO-ATOMIC-RENAME"))
				fCapabilityFlag |= kNoHierarchyRename;
			else if (! PL_strcasecmp(fNextToken, "X-NON-HIERARCHICAL-RENAME"))
				fCapabilityFlag |= kNoHierarchyRename;
			else if (! PL_strcasecmp(fNextToken, "NAMESPACE"))
				fCapabilityFlag |= kNamespaceCapability;
			else if (! PL_strcasecmp(fNextToken, "MAILBOXDATA"))
				fCapabilityFlag |= kMailboxDataCapability;
			else if (! PL_strcasecmp(fNextToken, "ACL"))
				fCapabilityFlag |= kACLCapability;
			else if (! PL_strcasecmp(fNextToken, "XSERVERINFO"))
				fCapabilityFlag |= kXServerInfoCapability;
			else if (! PL_strcasecmp(fNextToken, "UIDPLUS"))
				fCapabilityFlag |= kUidplusCapability;
			else if (! PL_strcasecmp(fNextToken, "LITERAL+"))
				fCapabilityFlag |= kLiteralPlusCapability;
		}
	} while (fNextToken && 
			 !at_end_of_line() &&
			 ContinueParse());

    char* userName = fServerConnection.GetImapUserName();

    if (fHostSessionList)
        fHostSessionList->SetCapabilityForHost(
            fServerConnection.GetImapHostName(), 
            userName,
            fCapabilityFlag);
    PR_FREEIF(userName);
	nsImapProtocol *navCon = &fServerConnection;
	NS_ASSERTION(navCon, "null imap protocol connection while parsing capability response");	// we should always have this
	if (navCon)
		navCon->CommitCapabilityForHostEvent();
	skip_to_CRLF();
}

void nsImapServerResponseParser::xmailboxinfo_data()
{
	fNextToken = GetNextToken();
	if (!fNextToken)
		return;

	char *mailboxName = CreateAstring(); // PL_strdup(fNextToken);
	if (mailboxName)
	{
		do 
		{
			fNextToken = GetNextToken();
			if (fNextToken) 
			{
				if (!PL_strcmp("MANAGEURL", fNextToken))
				{
					fNextToken = GetNextToken();
					fFolderAdminUrl = CreateAstring();
				}
				else if (!PL_strcmp("POSTURL", fNextToken))
				{
					fNextToken = GetNextToken();
					// ignore this for now...
				}
			}
		} while (fNextToken && !at_end_of_line() && ContinueParse());
	}
}

void nsImapServerResponseParser::xserverinfo_data()
{
	do 
	{
		fNextToken = GetNextToken();
		if (!fNextToken)
			break;
		if (!PL_strcmp("MANAGEACCOUNTURL", fNextToken))
		{
			fNextToken = GetNextToken();
			fMailAccountUrl = CreateNilString();
		}
		else if (!PL_strcmp("MANAGELISTSURL", fNextToken))
		{
			fNextToken = GetNextToken();
			fManageListsUrl = CreateNilString();
		}
		else if (!PL_strcmp("MANAGEFILTERSURL", fNextToken))
		{
			fNextToken = GetNextToken();
			fManageFiltersUrl = CreateNilString();
		}
	} while (fNextToken && !at_end_of_line() && ContinueParse());
}


void nsImapServerResponseParser::namespace_data()
{
	EIMAPNamespaceType namespaceType = kPersonalNamespace;
	PRBool namespacesCommitted = PR_FALSE;
    const char* hostName = fServerConnection.GetImapHostName();
    char* userName = fServerConnection.GetImapUserName();
	while ((namespaceType != kUnknownNamespace) && ContinueParse())
	{
		fNextToken = GetNextToken();
		while (at_end_of_line() && ContinueParse())
			fNextToken = GetNextToken();
		if (!PL_strcasecmp(fNextToken,"NIL"))
		{
			// No namespace for this type.
			// Don't add anything to the Namespace object.
		}
		else if (fNextToken[0] == '(')
		{
			// There may be multiple namespaces of the same type.
			// Go through each of them and add them to our Namespace object.

			fNextToken++;
			while (fNextToken[0] == '(' && ContinueParse())
			{
				// we have another namespace for this namespace type
				fNextToken++;
				if (fNextToken[0] != '"')
				{
					SetSyntaxError(PR_TRUE);
				}
				else
				{
					char *namespacePrefix = CreateQuoted(PR_FALSE);

					fNextToken = GetNextToken();
					char *quotedDelimiter = fNextToken;
					char namespaceDelimiter = '\0';

					if (quotedDelimiter[0] == '"')
					{
						quotedDelimiter++;
						namespaceDelimiter = quotedDelimiter[0];
					}
					else if (!PL_strncasecmp(quotedDelimiter, "NIL", 3))
					{
						// NIL hierarchy delimiter.  Leave namespace delimiter nsnull.
					}
					else
					{
						// not quoted or NIL.
						SetSyntaxError(PR_TRUE);
					}
					if (ContinueParse())
					{
						nsIMAPNamespace *newNamespace = new nsIMAPNamespace(namespaceType, namespacePrefix, namespaceDelimiter, PR_FALSE);
						// add it to a temporary list in the host
						if (newNamespace && fHostSessionList)
							fHostSessionList->AddNewNamespaceForHost(
                                hostName, userName, newNamespace);

						skip_to_close_paren();	// Ignore any extension data
	
						PRBool endOfThisNamespaceType = (fNextToken[0] == ')');
						if (!endOfThisNamespaceType && fNextToken[0] != '(')	// no space between namespaces of the same type
						{
							SetSyntaxError(PR_TRUE);
						}
					}
					PR_FREEIF(namespacePrefix);
				}
			}
		}
		else
		{
			SetSyntaxError(PR_TRUE);
		}
		switch (namespaceType)
		{
		case kPersonalNamespace:
			namespaceType = kOtherUsersNamespace;
			break;
		case kOtherUsersNamespace:
			namespaceType = kPublicNamespace;
			break;
		default:
			namespaceType = kUnknownNamespace;
			break;
		}
	}
	if (ContinueParse())
	{
		nsImapProtocol *navCon = &fServerConnection;
		NS_ASSERTION(navCon, "null protocol connection while parsing namespace");	// we should always have this
		if (navCon)
		{
			navCon->CommitNamespacesForHostEvent();
			namespacesCommitted = PR_TRUE;
		}
	}
	skip_to_CRLF();

	if (!namespacesCommitted && fHostSessionList)
	{
		PRBool success;
		fHostSessionList->FlushUncommittedNamespacesForHost(hostName,
                                                            userName,
                                                            success);
	}

    PR_FREEIF(userName);
}

void nsImapServerResponseParser::myrights_data()
{
	fNextToken = GetNextToken();
	if (ContinueParse() && !at_end_of_line())
	{
		char *mailboxName = CreateAstring(); // PL_strdup(fNextToken);
		if (mailboxName)
		{
			fNextToken = GetNextToken();
			if (ContinueParse())
			{
				char *myrights = CreateAstring(); // PL_strdup(fNextToken);
				if (myrights)
				{
					nsImapProtocol *navCon = &fServerConnection;
					NS_ASSERTION(navCon, "null connection parsing my rights");	// we should always have this
					if (navCon)
						navCon->AddFolderRightsForUser(mailboxName, nsnull /* means "me" */, myrights);
					PR_Free(myrights);
				}
				else
				{
					HandleMemoryFailure();
				}
				if (ContinueParse())
					fNextToken = GetNextToken();
			}
			PR_Free(mailboxName);
		}
		else
		{
			HandleMemoryFailure();
		}
	}
	else
	{
		SetSyntaxError(PR_TRUE);
	}
}

void nsImapServerResponseParser::acl_data()
{
	fNextToken = GetNextToken();
	if (ContinueParse() && !at_end_of_line())
	{
		char *mailboxName = CreateAstring();	// PL_strdup(fNextToken);
		if (mailboxName && ContinueParse())
		{
			fNextToken = GetNextToken();
			while (ContinueParse() && !at_end_of_line())
			{
				char *userName = CreateAstring(); // PL_strdup(fNextToken);
				if (userName && ContinueParse())
				{
					fNextToken = GetNextToken();
					if (ContinueParse())
					{
						char *rights = CreateAstring(); // PL_strdup(fNextToken);
						if (rights)
						{
							fServerConnection.AddFolderRightsForUser(mailboxName, userName, rights);
							PR_Free(rights);
						}
						else
							HandleMemoryFailure();

						if (ContinueParse())
							fNextToken = GetNextToken();
					}
					PR_Free(userName);
				}
				else
					HandleMemoryFailure();
			}
			PR_Free(mailboxName);
		}
		else
			HandleMemoryFailure();
	}
}


void nsImapServerResponseParser::mime_data()
{
	if (PL_strstr(fNextToken, "MIME"))
		mime_header_data();
	else
		mime_part_data();
}

// mime_header_data should not be streamed out;  rather, it should be
// buffered in the nsIMAPBodyShell.
// This is because we are still in the process of generating enough
// information from the server (such as the MIME header's size) so that
// we can construct the final output stream.
void nsImapServerResponseParser::mime_header_data()
{
	char *partNumber = PL_strdup(fNextToken);
	if (partNumber)
	{
		char *start = partNumber+5, *end = partNumber+5;	// 5 == nsCRT::strlen("BODY[")
		while (ContinueParse() && end && *end != 'M' && *end != 'm')
		{
			end++;
		}
		if (end && (*end == 'M' || *end == 'm'))
		{
			*(end-1) = 0;
			fNextToken = GetNextToken();
			char *mimeHeaderData = CreateAstring();	// is it really this simple?
			fNextToken = GetNextToken();
			if (m_shell)
			{
				m_shell->AdoptMimeHeader(start, mimeHeaderData);
			}
		}
		else
		{
			SetSyntaxError(PR_TRUE);
		}
		PR_Free(partNumber);	// partNumber is not adopted by the body shell.
	}
	else
	{
		HandleMemoryFailure();
	}
}

// Actual mime parts are filled in on demand (either from shell generation
// or from explicit user download), so we need to stream these out.
void nsImapServerResponseParser::mime_part_data()
{
	char *checkOriginToken = PL_strdup(fNextToken);
	if (checkOriginToken)
	{
		PRUint32 origin = 0;
		PRBool originFound = PR_FALSE;
		char *whereStart = PL_strchr(checkOriginToken, '<');
		if (whereStart)
		{
			char *whereEnd = PL_strchr(whereStart, '>');
			if (whereEnd)
			{
				*whereEnd = 0;
				whereStart++;
				origin = atoi(whereStart);
				originFound = PR_TRUE;
			}
		}
		PR_Free(checkOriginToken);
		fNextToken = GetNextToken();
		msg_fetch_content(originFound, origin, MESSAGE_RFC822);	// keep content type as message/rfc822, even though the
																// MIME part might not be, because then libmime will
																// still handle and decode it.
	}
	else
		HandleMemoryFailure();
}

// FETCH BODYSTRUCTURE parser
// After exit, set fNextToken and fCurrentLine to the right things
void nsImapServerResponseParser::bodystructure_data()
{
	fNextToken = GetNextToken();

	// separate it out first
	if (fNextToken && *fNextToken == '(')	// It has to start with an open paren.
	{
		char *buf = CreateParenGroup();

		if (ContinueParse())
		{
			if (!buf)
				HandleMemoryFailure();
			else
			{
				// Looks like we have what might be a valid BODYSTRUCTURE response.
				// Try building the shell from it here.
				m_shell = new nsIMAPBodyShell(&fServerConnection, buf, CurrentResponseUID(), GetSelectedMailboxName());
				/*
				if (m_shell)
				{
					if (!m_shell->GetIsValid())
					{
						SetSyntaxError(PR_TRUE);
					}
				}
				*/
				PR_Free(buf);
			}
		}
	}
	else
		SetSyntaxError(PR_TRUE);
}

PRBool nsImapServerResponseParser::GetFillingInShell()
{
	return (m_shell != nsnull);
}

PRBool nsImapServerResponseParser::GetDownloadingHeaders()
{
	return fDownloadingHeaders;
}

// Tells the server state parser to use a previously cached shell.
void	nsImapServerResponseParser::UseCachedShell(nsIMAPBodyShell *cachedShell)
{
	// We shouldn't already have another shell we're dealing with.
	if (m_shell && cachedShell)
	{
		PR_LOG(IMAP, PR_LOG_ALWAYS, ("PARSER: Shell Collision"));
		NS_ASSERTION(PR_FALSE, "shell collision");
	}
	m_shell = cachedShell;
}


void nsImapServerResponseParser::ResetCapabilityFlag() 
{
    if (fHostSessionList)
    {
        char* userName = fServerConnection.GetImapUserName();

        fHostSessionList->SetCapabilityForHost(
            fServerConnection.GetImapHostName(), userName,
            kCapabilityUndefined);
        PR_FREEIF(userName);
    }
}

/*
 literal         ::= "{" number "}" CRLF *CHAR8
                              ;; Number represents the number of CHAR8 octets
*/
// returns PR_TRUE if this is the last chunk and we should close the stream
PRBool nsImapServerResponseParser::msg_fetch_literal(PRBool chunk, PRInt32 origin)
{
	numberOfCharsInThisChunk = atoi(fNextToken + 1); // might be the whole message
	charsReadSoFar = 0;
	static PRBool lastCRLFwasCRCRLF = PR_FALSE;

	PRBool lastChunk = !chunk || (origin + numberOfCharsInThisChunk >= fTotalDownloadSize);

	nsIImapUrl::nsImapAction imapAction; 
	fServerConnection.GetCurrentUrl()->GetImapAction(&imapAction);
	if (!lastCRLFwasCRCRLF && 
		fServerConnection.GetIOTunnellingEnabled() && 
		(numberOfCharsInThisChunk > fServerConnection.GetTunnellingThreshold()) &&
		(imapAction != nsIImapUrl::nsImapOnlineToOfflineCopy) &&
		(imapAction != nsIImapUrl::nsImapOnlineToOfflineMove))
	{
		// One day maybe we'll make this smarter and know how to handle CR/LF boundaries across tunnels.
		// For now, we won't, even though it might not be too hard, because it is very rare and will add
		// some complexity.
		charsReadSoFar = fServerConnection.OpenTunnel(numberOfCharsInThisChunk);
	}

	// If we opened a tunnel, finish everything off here.  Otherwise, get everything here.
	// (just like before)

	while (ContinueParse() && (charsReadSoFar < numberOfCharsInThisChunk))
	{
		AdvanceToNextLine();
		if (ContinueParse())
		{
			if (lastCRLFwasCRCRLF && (*fCurrentLine == CR))
			{
				char *usableCurrentLine = PL_strdup(fCurrentLine + 1);
				PR_FREEIF(fCurrentLine);
				if (usableCurrentLine)
					fCurrentLine = usableCurrentLine;
				else
					fCurrentLine = 0;
			}

			if (ContinueParse())
			{
				charsReadSoFar += nsCRT::strlen(fCurrentLine);
				if (!fDownloadingHeaders && fCurrentCommandIsSingleMessageFetch)
				{
					fServerConnection.ProgressEventFunctionUsingId(kImapDownloadingMessage);
					if (fTotalDownloadSize > 0)
						fServerConnection.PercentProgressUpdateEvent(0,(100*(charsReadSoFar + origin))/fTotalDownloadSize);
				}
				if (charsReadSoFar > numberOfCharsInThisChunk)
				{	// this is rare.  If this msg ends in the middle of a line then only display the actual message.
					char *displayEndOfLine = (fCurrentLine + nsCRT::strlen(fCurrentLine) - (charsReadSoFar - numberOfCharsInThisChunk));
					char saveit = *displayEndOfLine;
					*displayEndOfLine = 0;
					fServerConnection.HandleMessageDownLoadLine(fCurrentLine, !lastChunk);
					*displayEndOfLine = saveit;
					lastCRLFwasCRCRLF = (*(displayEndOfLine - 1) == CR);
				}
				else
				{
					lastCRLFwasCRCRLF = (*(fCurrentLine + nsCRT::strlen(fCurrentLine) - 1) == CR);
					fServerConnection.HandleMessageDownLoadLine(fCurrentLine, !lastChunk && (charsReadSoFar == numberOfCharsInThisChunk));
				}
			}
		}
	}

	// This would be a good thing to log.
	if (lastCRLFwasCRCRLF)
		PR_LOG(IMAP, PR_LOG_ALWAYS, ("PARSER: CR/LF fell on chunk boundary."));
	
	if (ContinueParse())
	{
		if (charsReadSoFar > numberOfCharsInThisChunk)
		{
			// move the lexical analyzer state to the end of this message because this message
			// fetch ends in the middle of this line.
			//fCurrentTokenPlaceHolder = fLineOfTokens + nsCRT::strlen(fCurrentLine) - (charsReadSoFar - numberOfCharsInThisChunk);
			AdvanceTokenizerStartingPoint(nsCRT::strlen(fCurrentLine) - (charsReadSoFar - numberOfCharsInThisChunk));
			fNextToken = GetNextToken();
		}
		else
		{
			skip_to_CRLF();
			fNextToken = GetNextToken();
		}
	}
	else
	{
		lastCRLFwasCRCRLF = PR_FALSE;
	}
	return lastChunk;
}

PRBool nsImapServerResponseParser::CurrentFolderReadOnly()
{
	return fCurrentFolderReadOnly;
}

PRInt32	nsImapServerResponseParser::NumberOfMessages()
{
	return fNumberOfExistingMessages;
}

PRInt32 	nsImapServerResponseParser::NumberOfRecentMessages()
{
	return fNumberOfRecentMessages;
}

PRInt32 	nsImapServerResponseParser::NumberOfUnseenMessages()
{
	return fNumberOfUnseenMessages;
}

PRInt32 nsImapServerResponseParser::FolderUID()
{
	return fFolderUIDValidity;
}

PRUint32 nsImapServerResponseParser::CurrentResponseUID()
{
	return fCurrentResponseUID;
}

PRUint32 nsImapServerResponseParser::HighestRecordedUID()
{
	return fHighestRecordedUID;
}

PRBool nsImapServerResponseParser::IsNumericString(const char *string)
{
	int i;
	for(i = 0; i < (int) PL_strlen(string); i++)
	{
		if (! isdigit(string[i]))
		{
			return FALSE;
		}
	}

	return TRUE;
}


struct mailbox_spec *nsImapServerResponseParser::CreateCurrentMailboxSpec(const char *mailboxName /* = nsnull */)
{
	mailbox_spec *returnSpec = (mailbox_spec *) PR_Calloc(1, sizeof(mailbox_spec) );
	if (returnSpec)
	{	
		char *convertedMailboxName = nsnull;
		const char *mailboxNameToConvert = (mailboxName) ? mailboxName : fSelectedMailboxName;
	    if (mailboxNameToConvert)
	    {
			const char *host = 				
                fServerConnection.GetImapHostName();
			nsIMAPNamespace *ns = nsnull;
			if (host != nsnull && fHostSessionList)
			{
                char* userName = fServerConnection.GetImapUserName();
				fHostSessionList->GetNamespaceForMailboxForHost(host, 
                           userName, mailboxNameToConvert, ns);	// for
                                                                // delimiter
                PR_FREEIF(userName);
			}

			if (ns)
				returnSpec->hierarchySeparator = ns->GetDelimiter();
			else
				returnSpec->hierarchySeparator = '/';	// a guess?

			char *convertedName = fServerConnection.CreateUtf7ConvertedString(mailboxNameToConvert, PR_FALSE);
			if (convertedName)
	    	{
	    		fServerConnection.GetCurrentUrl()->AllocateCanonicalPath(convertedName, returnSpec->hierarchySeparator, &convertedMailboxName);
	    		PR_Free(convertedName);
	    	}
	    }

		returnSpec->folderSelected = PR_TRUE;
		returnSpec->folder_UIDVALIDITY = fFolderUIDValidity;
		returnSpec->number_of_messages = fNumberOfExistingMessages;
		returnSpec->number_of_unseen_messages = fNumberOfUnseenMessages;
		returnSpec->number_of_recent_messages = fNumberOfRecentMessages;
		
		returnSpec->box_flags = kNoFlags;	// stub
		returnSpec->onlineVerified = PR_FALSE;	// we're fabricating this.  The flags aren't verified.
		returnSpec->allocatedPathName = convertedMailboxName;
		returnSpec->connection = &fServerConnection;
		if (returnSpec->connection)
			returnSpec->connection->GetCurrentUrl()->GetHost(&returnSpec->hostName);
		else
			returnSpec->hostName = nsnull;
		if (fFlagState)
			returnSpec->flagState = fFlagState; //copies flag state
		else
			returnSpec->flagState = nsnull;
	}
	else
		HandleMemoryFailure();
	
	return returnSpec;
	
}
// zero stops a list recording of flags and causes the flags for
// each individual message to be sent back to libmsg 
void nsImapServerResponseParser::ResetFlagInfo(int numberOfInterestingMessages)
{
	if (fFlagState)
		fFlagState->Reset(numberOfInterestingMessages);
}


PRBool nsImapServerResponseParser::GetLastFetchChunkReceived()
{
	return fLastChunk;
}

void nsImapServerResponseParser::ClearLastFetchChunkReceived()
{
	fLastChunk = PR_FALSE;
}

void nsImapServerResponseParser::SetHostSessionList(nsIImapHostSessionList*
                                               aHostSessionList)
{
    NS_IF_RELEASE (fHostSessionList);
    fHostSessionList = aHostSessionList;
    NS_IF_ADDREF (fHostSessionList);
}

nsIImapHostSessionList*
nsImapServerResponseParser::GetHostSessionList()
{
    NS_IF_ADDREF(fHostSessionList);
    return fHostSessionList;
}

        
