/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 1999
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

#ifndef nsParseMailbox_H
#define nsParseMailbox_H

#include "nsIURI.h"
#include "nsIMsgParseMailMsgState.h"
#include "nsMsgKeyArray.h"
#include "nsVoidArray.h"
#include "nsIStreamListener.h"
#include "nsMsgLineBuffer.h"
#include "nsIMsgHeaderParser.h"
#include "nsIMsgDatabase.h"
#include "nsIMsgHdr.h"
#include "nsIMsgStatusFeedback.h"
#include "nsXPIDLString.h"
#include "nsLocalStringBundle.h"
#include "nsCOMPtr.h"
#include "nsTime.h"
#include "nsFileSpec.h"
#include "nsIDBChangeListener.h"

#include "nsIMsgFilterList.h"
#include "nsIMsgFilterHitNotify.h"

class nsFileSpec;
class nsByteArray;
class nsOutputFileStream;
class nsIOFileStream;
class nsInputFileStream;
class nsIMsgFilter;
class MSG_FolderInfoMail;
class nsIMsgFilterList;
class nsIFolder;

/* Used for the various things that parse RFC822 headers...
 */
typedef struct message_header
{
  const char *value; /* The contents of a header (after ": ") */
  PRInt32 length;      /* The length of the data (it is not NULL-terminated.) */
} message_header;

// This object maintains the parse state for a single mail message.
class nsParseMailMessageState : public nsIMsgParseMailMsgState
{
public:
	NS_DECL_ISUPPORTS
  NS_DECL_NSIMSGPARSEMAILMSGSTATE
	
  nsParseMailMessageState();
	virtual			~nsParseMailMessageState();
	
	void			Init(PRUint32 fileposition);
	virtual PRInt32	ParseFolderLine(const char *line, PRUint32 lineLength);
	virtual int		StartNewEnvelope(const char *line, PRUint32 lineLength);
	int				ParseHeaders();
	int				FinalizeHeaders();
	int				ParseEnvelope (const char *line, PRUint32 line_size);
	int				InternSubject (struct message_header *header);
	nsresult		InternRfc822 (struct message_header *header, 
								char **ret_name);

	static PRBool	IsEnvelopeLine(const char *buf, PRInt32 buf_size);
	static int		msg_UnHex(char C); 

	nsCOMPtr<nsIMsgHeaderParser> m_HeaderAddressParser;

	nsCOMPtr<nsIMsgDBHdr>		m_newMsgHdr;		/* current message header we're building */
	nsCOMPtr<nsIMsgDatabase>	m_mailDB;

	nsMailboxParseState m_state;
	PRUint32			m_position;
	PRUint32			m_envelope_pos;
	PRUint32			m_headerstartpos;

	nsByteArray		m_headers;

	nsByteArray		m_envelope;

	struct message_header m_message_id;
	struct message_header m_references;
	struct message_header m_date;
	struct message_header m_from;
	struct message_header m_sender;
	struct message_header m_newsgroups;
	struct message_header m_subject;
	struct message_header m_status;
	struct message_header m_mozstatus;
	struct message_header m_mozstatus2;
	struct message_header m_in_reply_to;
	struct message_header m_content_type;

	// Support for having multiple To or Cc header lines in a message
	nsVoidArray m_toList;
	nsVoidArray m_ccList;
	struct message_header *GetNextHeaderInAggregate (nsVoidArray &list);
	void GetAggregateHeader (nsVoidArray &list, struct message_header *);
	void ClearAggregateHeader (nsVoidArray &list);

	struct message_header m_envelope_from;
	struct message_header m_envelope_date;
	struct message_header m_priority;
	// Mdn support
	struct message_header m_mdn_original_recipient;
	struct message_header m_return_path;
	struct message_header m_mdn_dnt; /* MDN Disposition-Notification-To: header */

	PRUint16			m_body_lines;
	
	PRBool			m_IgnoreXMozillaStatus;
protected:
  nsCOMPtr<nsIMsgStringService> mStringService;

};

// this should go in some utility class.
inline int	nsParseMailMessageState::msg_UnHex(char C)
{
	return ((C >= '0' && C <= '9') ? C - '0' :
		((C >= 'A' && C <= 'F') ? C - 'A' + 10 :
		 ((C >= 'a' && C <= 'f') ? C - 'a' + 10 : 0)));
}

// This class is part of the mailbox parsing state machine 
class nsMsgMailboxParser : public nsIStreamListener, public nsParseMailMessageState, public nsMsgLineBuffer, public nsMsgLineBufferHandler, public nsIDBChangeListener
{
public:
	nsMsgMailboxParser();
	virtual ~nsMsgMailboxParser();

	PRBool  IsRunningUrl() { return m_urlInProgress;} // returns true if we are currently running a url and false otherwise...
	NS_DECL_ISUPPORTS_INHERITED

	////////////////////////////////////////////////////////////////////////////////////////
	// we suppport the nsIStreamListener interface 
	////////////////////////////////////////////////////////////////////////////////////////
    NS_DECL_NSIREQUESTOBSERVER
    NS_DECL_NSISTREAMLISTENER
    NS_DECL_NSIDBCHANGELISTENER


	void			SetDB (nsIMsgDatabase *mailDB) {m_mailDB = dont_QueryInterface(mailDB); }

	void			SetIncrementalUpdate(PRBool update) {m_updateAsWeGo = update;}
	void			SetIgnoreNonMailFolder(PRBool ignoreNonMailFolder) {m_ignoreNonMailFolder = ignoreNonMailFolder;}
	PRBool			GetIsRealMailFolder() {return m_isRealMailFolder;}

	// message socket libnet callbacks, which come through folder pane
	virtual int ProcessMailboxInputStream(nsIURI* aURL, nsIInputStream *aIStream, PRUint32 aLength);

	virtual void	DoneParsingFolder(nsresult status);
	virtual void	AbortNewHeader();

	// for nsMsgLineBuffer
	virtual PRInt32 HandleLine(char *line, PRUint32 line_length);

	void			UpdateDBFolderInfo();
	void			UpdateDBFolderInfo(nsIMsgDatabase *mailDB);
	void			UpdateStatusText (PRUint32 stringID);

	// Update the progress bar based on what we know.
	virtual void    UpdateProgressPercent ();

protected:
	nsCOMPtr<nsIMsgStatusFeedback> m_statusFeedback;

	virtual PRInt32			PublishMsgHeader(nsIMsgWindow *msgWindow);
	virtual void			FolderTypeSpecificTweakMsgHeader(nsIMsgDBHdr *tweakMe);
	void					FreeBuffers();

	// data
    nsString        m_folderName;
	nsXPIDLCString		m_inboxUri;
	nsByteArray		m_inputStream;
	PRInt32			m_obuffer_size;
	char			*m_obuffer;
	PRInt32			m_graph_progress_total;
	PRInt32			m_graph_progress_received;
	PRBool			m_updateAsWeGo;
	PRBool			m_parsingDone;
	PRBool			m_ignoreNonMailFolder;
	PRBool			m_isRealMailFolder;
	nsTime			m_startTime;
private:
		// the following flag is used to determine when a url is currently being run. It is cleared on calls
	// to ::StopBinding and it is set whenever we call Load on a url
	PRBool	m_urlInProgress;	

};

class nsParseNewMailState : public nsMsgMailboxParser 
, public nsIMsgFilterHitNotify
{
public:
	nsParseNewMailState();
	virtual ~nsParseNewMailState();
	NS_DECL_ISUPPORTS_INHERITED
    nsresult Init(nsIFolder *rootFolder, nsIMsgFolder *downloadFolder, nsFileSpec &folder, nsIOFileStream *inboxFileStream);

	virtual void	DoneParsingFolder(nsresult status);
	virtual void	SetUsingTempDB(PRBool usingTempDB, char *tmpDBName);

	void DisableFilters() {m_disableFilters = PR_TRUE;}

#ifdef DOING_JSFILTERS
	// from jsmsg.cpp
	friend void JSMailFilter_MoveMessage(ParseNewMailState *state, 
										 nsIMsgDBHdr *msgHdr, 
										 nsMailDatabase *mailDB, 
										 const char *folder, 
										 nsIMsgFilter *filter,
										 PRBool *pMoved);
#endif
	// nsIMsgFilterHitNotification method(s)
	NS_IMETHOD ApplyFilterHit(nsIMsgFilter *filter, nsIMsgWindow *msgWindow, PRBool *applyMore);

	nsOutputFileStream *GetLogFile();
	virtual PRInt32	PublishMsgHeader(nsIMsgWindow *msgWindow);
protected:
	char				*m_tmpdbName;				// Temporary filename of new database
	PRBool				m_usingTempDB;
	virtual void	ApplyFilters(PRBool *pMoved, nsIMsgWindow *msgWindow);
	virtual nsresult GetTrashFolder(nsIMsgFolder **pTrashFolder);
	virtual nsresult	MoveIncorporatedMessage(nsIMsgDBHdr *mailHdr, 
											   nsIMsgDatabase *sourceDB, 
                                                const char *destFolder,
                                            nsIMsgFilter *filter,
                                            nsIMsgWindow *msgWindow);
	virtual	int			MarkFilteredMessageRead(nsIMsgDBHdr *msgHdr);
			void		LogRuleHit(nsIMsgFilter *filter, nsIMsgDBHdr *msgHdr);
	nsCOMPtr <nsIMsgFilterList> m_filterList;
	nsCOMPtr <nsIFolder> m_rootFolder;
	nsOutputFileStream	*m_logFile;
	nsIOFileStream		*m_inboxFileStream;
	nsFileSpec			m_inboxFileSpec;
	PRBool				m_disableFilters;
	PRBool				m_msgMovedByFilter;
	PRUint32			m_ibuffer_fp;
	char			*m_ibuffer;
	PRUint32			m_ibuffer_size;
};

#ifdef IMAP_NEW_MAIL_HANDLED


class ParseIMAPMailboxState : public ParseNewMailState 
{
public:
	ParseIMAPMailboxState(MSG_IMAPHost *host, MSG_FolderInfoMail *folder, MSG_UrlQueue *urlQueue, TImapFlagAndUidState *flagStateAdopted);
	virtual ~ParseIMAPMailboxState();
	
	// close the db
	virtual void			DoneParsingFolder(nsresult status);
	
	virtual void			SetPublishUID(PRInt32 uid);
	virtual void			SetPublishByteLength(PRUint32 byteLength);
	virtual void			SetNextSequenceNum(PRInt32 seqNum);
	
	const IDArray 			&GetBodyKeys() { return fFetchBodyKeys; }
	MSG_UrlQueue	*GetFilterUrlQueue() {return fUrlQueue;}
protected:
	virtual PRInt32			PublishMsgHeader(nsIMsgWindow *msgWindow);
	virtual void			FolderTypeSpecificTweakMsgHeader(nsIMsgDBHdr *tweakMe);
	virtual void			ApplyFilters(PRBool *pMoved, nsIMsgWindow *msgWindow);
	
	virtual MSG_FolderInfoMail *GetTrashFolder();
	virtual nsresult		MoveIncorporatedMessage(nsIMsgDBHdr *mailHdr, 
											   nsIMsgDatabase *sourceDB, 
											   char *destFolder,
											   nsIMsgFilter *filter);
	virtual	int				MarkFilteredMessageRead(nsIMsgDBHdr *msgHdr);
private:
	PRBool fParsingInbox;
	PRBool	fB2HaveWarnedUserOfOfflineFiltertarget;
	PRInt32 	fNextMessagePublishUID;
	PRUint32	fNextMessageByteLength;
	MSG_UrlQueue *fUrlQueue;
	TImapFlagAndUidState *fFlagState;
	nsMsgKeyArray fFetchBodyKeys;
	PRInt32	fNextSequenceNum;
	MSG_FolderInfoContainer *m_imapContainer;
	MSG_IMAPHost	*m_host;
};

// If m_out_file is set, this parser will also write out the message,
// adding a mozilla status line if one is not present.
class ParseOutgoingMessage : public ParseMailMessageState
{
public:
	ParseOutgoingMessage();
	virtual ~ParseOutgoingMessage();
	void			SetOutFile(XP_File out_file) {m_out_file = out_file;}
	XP_File			GetOutFile() {return m_out_file;}
	void			SetWriteMozillaStatus(PRBool writeMozStatus) 
						{m_writeMozillaStatus = writeMozStatus;}
	virtual int		StartNewEnvelope(const char *line, PRUint32 lineLength);
	virtual void	FinishHeader();
	virtual PRInt32	ParseFolderLine(const char *line, PRUint32 lineLength);
	virtual PRInt32	ParseBlock(const char *block, PRUint32 lineLength);
	virtual void	Clear();
	void			AdvanceOutPosition(PRUint32 amountToAdvance);
	void			SetWriteToOutFile(PRBool writeToOutFile) {m_writeToOutFile = writeToOutFile;}

	void			FlushOutputBuffer();
	PRUint32			m_bytes_written;
protected:
	static PRInt32	LineBufferCallback(char *line, PRUint32 lineLength, void *closure);
	PRBool			m_wroteXMozillaStatus;
	PRBool			m_writeMozillaStatus;
	PRBool			m_writeToOutFile;
	PRBool			m_lastBodyLineEmpty;
	XP_File			m_out_file;
	PRUint32			m_ouputBufferSize;
	char			*m_outputBuffer;
	PRUint32			m_outputBufferIndex;
};

#endif /* #ifdef NEW_MAIL_HANDLED */
 

#endif





