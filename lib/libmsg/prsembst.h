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
#ifndef ParseMboxSt_H
#define ParseMboxSt_H

#include "idarray.h"
#include "ptrarray.h"
/* Be sure to include msg.h before including this file. */

class MSG_Master;
class MailDB;
class MSG_Pane;
class MSG_FolderPane;
class MailMessageHdr;
struct MSG_FilterList;
struct MSG_Filter;
struct MSG_Rule;
class MailMessageHdr;
class MSG_UrlQueue;
class TImapFlagAndUidState;
class MSG_FolderInfoContainer;
class MSG_FolderInfoMail;
class MSG_IMAPHost;

typedef enum
{
  MBOX_PARSE_ENVELOPE,
  MBOX_PARSE_HEADERS,
  MBOX_PARSE_BODY
} MBOX_PARSE_STATE;



// This object maintains the parse state for a single mail message.
class ParseMailMessageState 
{
public:
					ParseMailMessageState();
					virtual ~ParseMailMessageState();
	void			Init(uint32 fileposition);
	virtual void	Clear();
	virtual void	FinishHeader();
	virtual int32	ParseFolderLine(const char *line, uint32 lineLength);
	virtual int		StartNewEnvelope(const char *line, uint32 lineLength);
	int				ParseHeaders();
	int				GrowHeaders(uint32 desired_size);
	int				GrowEnvelope(uint32 desired_size);
	int				FinalizeHeaders();
	int				ParseEnvelope (const char *line, uint32 line_size);
	int				InternSubject (struct message_header *header);
	int				InternRfc822 (struct message_header *header, 
								char **ret_name);
	void			SetMailDB(MailDB *mailDB);
	MailDB			*GetMailDB() {return m_mailDB;}
	MailMessageHdr *m_newMsgHdr;		/* current message header we're building */
	MailDB			*m_mailDB;

	MBOX_PARSE_STATE m_state;
	uint32			m_position;
	uint32			m_envelope_pos;
	uint32			m_headerstartpos;

	char			*m_headers;
	uint32			m_headers_fp;
	uint32			m_headers_size;

	char			*m_envelope;
	uint32			m_envelope_fp;
	uint32			m_envelope_size;

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

	// Support for having multiple To or CC header lines in a message
	XPPtrArray m_toList;
	XPPtrArray m_ccList;
	struct message_header *GetNextHeaderInAggregate (XPPtrArray &list);
	void GetAggregateHeader (XPPtrArray &list, struct message_header *);
	void ClearAggregateHeader (XPPtrArray &list);

	struct message_header m_envelope_from;
	struct message_header m_envelope_date;
	struct message_header m_priority;
	// Mdn support
	struct message_header m_mdn_original_recipient;
	struct message_header m_return_path;
	struct message_header m_mdn_dnt; /* MDN Disposition-Notification-To: header */

	uint16			m_body_lines;
	
	XP_Bool			m_IgnoreXMozillaStatus;
};

// This class should ultimately be part of a mailbox parsing
// state machine - either by inheritance or delegation.
// Currently, a folder pane owns one and libnet message socket
// related messages get passed to this object.

// This class has a few things in common with ListNewsGroupState.
// We might want to consider a common base class that stores the
// view and db and url and master objects, though there will
// be casting involved if we do that so it may not be worthwhile.
// All of the actual work functions are different.
class ParseMailboxState 
{
public:
	ParseMailboxState(const char *mailboxName);
	virtual ~ParseMailboxState();
	MSG_Master		*GetMaster() {return m_mailMaster;}
	void			SetMaster(MSG_Master *master) 
						{m_mailMaster = master;}
	void			SetView(MessageDBView *view) {m_msgDBView = view;}
	void			SetPane(MSG_Pane *pane) {m_pane = pane;}
	void			SetContext(MWContext *context) {m_context = context; }
	MWContext*		GetContext() { return m_context; }
	void			SetMailMessageParseState(ParseMailMessageState *mailMessageState) ;
	void			SetDB (MailDB *mailDB) {m_mailDB = mailDB; if (m_parseMsgState) m_parseMsgState->SetMailDB(mailDB);}
	MailDB			*GetDB() {return m_mailDB;}
	char			*GetMailboxName() {return m_mailboxName;}

	void			SetFolder(MSG_FolderInfo *folder) {m_folder = folder;}
	void			SetIncrementalUpdate(XP_Bool update) {m_updateAsWeGo = update;}
	void			SetIgnoreNonMailFolder(XP_Bool ignoreNonMailFolder) {m_ignoreNonMailFolder = ignoreNonMailFolder;}
	MailMessageHdr *GetCurrentMsg() {return m_parseMsgState->m_newMsgHdr;}
	XP_Bool			GetIsRealMailFolder() {return m_isRealMailFolder;}

	// message socket libnet callbacks, which come through folder pane
	virtual int BeginOpenFolderSock(const char *folder_name,
								  const char *message_id, int32 msgnum,
								  void **folder_ptr);
	virtual int ParseMoreFolderSock(const char* folder_name,
								   const char* message_id,
								   int32 msgnum, void** folder_ptr);
	virtual void CloseFolderSock(const char* folder_name, const char* message_id,
							   int32 msgnum, void* folder_ptr);

	virtual int		BeginParsingFolder(int32 startPos);
	virtual void	DoneParsingFolder();
	virtual void	AbortNewHeader();
	virtual int		ParseBlock(const char *block, int32 length);
	// msg_LineBuffer callback.
	static  int32 LineBufferCallback (char *line, uint32 line_length,
									  void *closure);
	// which in turn calls this.
	int32			ParseFolderLine(const char *line, uint32 lineLength);

	void			UpdateDBFolderInfo();
	void			UpdateDBFolderInfo(MailDB *mailDB, const char *mailboxName);
	void			UpdateStatusText ();

	// Update the progress bar based on what we know.
	virtual void    UpdateProgressPercent ();

	ParseMailMessageState *GetMsgState();
protected:

	virtual int32			PublishMsgHeader();
	virtual void			FolderTypeSpecificTweakMsgHeader(MailMessageHdr *tweakMe);
	void					FreeBuffers();

	// data
	ParseMailMessageState	*m_parseMsgState;

	MailDB			*m_mailDB;
	MessageDBView	*m_msgDBView;	// open view on current download, if any
	MSG_FolderInfo	*m_folder;		// folder, if any, we are reparsing.
	char			*m_mailboxName;
	MSG_Master		*m_mailMaster;
	MSG_Pane		*m_pane;
	int32			m_obuffer_size;
	char			*m_obuffer;
	uint32			m_ibuffer_fp;
	char			*m_ibuffer;
	uint32			m_ibuffer_size;
	XP_File			m_file;
	int32			m_graph_progress_total;
	int32			m_graph_progress_received;
	MWContext		*m_context;
	XP_Bool			m_updateAsWeGo;
	XP_Bool			m_parsingDone;
	XP_Bool			m_ignoreNonMailFolder;
	XP_Bool			m_isRealMailFolder;
};


class ParseNewMailState : public ParseMailboxState 
{
public:
	ParseNewMailState(MSG_Master *master, MSG_FolderInfoMail *folder);
	virtual ~ParseNewMailState();
	virtual void	DoneParsingFolder();
	virtual void	SetUsingTempDB(XP_Bool usingTempDB, char *tmpDBName);

	void DisableFilters() {m_disableFilters = TRUE;}

	// from jsmsg.cpp
	friend void JSMailFilter_MoveMessage(ParseNewMailState *state, 
										 MailMessageHdr *msgHdr, 
										 MailDB *mailDB, 
										 const char *folder, 
										 MSG_Filter *filter,
										 XP_Bool *pMoved);


protected:
	virtual int32	PublishMsgHeader();
	virtual void	ApplyFilters(XP_Bool *pMoved);
	virtual MSG_FolderInfoMail *GetTrashFolder();
	virtual int		MoveIncorporatedMessage(MailMessageHdr *mailHdr, 
											   MailDB *sourceDB, 
											   char *destFolder,
											   MSG_Filter *filter);
	virtual	int		MarkFilteredMessageRead(MailMessageHdr *msgHdr);
			void	LogRuleHit(MSG_Filter *filter, MailMessageHdr *msgHdr);
	MSG_FilterList *m_filterList;
	XP_File			m_logFile;
	char			*m_tmpdbName;				// Temporary filename of new database
	XP_Bool			m_usingTempDB;
	XP_Bool			m_disableFilters;
};


class ParseIMAPMailboxState : public ParseNewMailState 
{
public:
	ParseIMAPMailboxState(MSG_Master *master, MSG_IMAPHost *host, MSG_FolderInfoMail *folder, MSG_UrlQueue *urlQueue, TImapFlagAndUidState *flagStateAdopted);
	virtual ~ParseIMAPMailboxState();
	
	// close the db
	virtual void			DoneParsingFolder();
	
	virtual void			SetPublishUID(int32 uid);
	virtual void			SetPublishByteLength(uint32 byteLength);
	virtual void			SetNextSequenceNum(int32 seqNum);
	
	const IDArray 			&GetBodyKeys() { return fFetchBodyKeys; }
	MSG_UrlQueue	*GetFilterUrlQueue() {return fUrlQueue;}
protected:
	virtual int32			PublishMsgHeader();
	virtual void			FolderTypeSpecificTweakMsgHeader(MailMessageHdr *tweakMe);
	virtual void			ApplyFilters(XP_Bool *pMoved);
	
	virtual MSG_FolderInfoMail *GetTrashFolder();
	virtual int		MoveIncorporatedMessage(MailMessageHdr *mailHdr, 
											   MailDB *sourceDB, 
											   char *destFolder,
											   MSG_Filter *filter);
	virtual	int				MarkFilteredMessageRead(MailMessageHdr *msgHdr);
private:
	XP_Bool fParsingInbox;
	XP_Bool	fB2HaveWarnedUserOfOfflineFiltertarget;
	int32 	fNextMessagePublishUID;
	uint32	fNextMessageByteLength;
	MSG_UrlQueue *fUrlQueue;
	TImapFlagAndUidState *fFlagState;
	IDArray fFetchBodyKeys;
	int32	fNextSequenceNum;
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
	virtual int		StartNewEnvelope(const char *line, uint32 lineLength);
	virtual void	FinishHeader();
	virtual int32	ParseFolderLine(const char *line, uint32 lineLength);
	virtual int32	ParseBlock(const char *block, uint32 lineLength);
	virtual void	Clear();
	void			AdvanceOutPosition(uint32 amountToAdvance);
	void			SetWriteToOutFile(XP_Bool writeToOutFile) {m_writeToOutFile = writeToOutFile;}

	void			FlushOutputBuffer();
	uint32			m_bytes_written;
protected:
	static int32	LineBufferCallback(char *line, uint32 lineLength, void *closure);
	XP_Bool			m_wroteXMozillaStatus;
	XP_Bool			m_writeToOutFile;
	XP_Bool			m_lastBodyLineEmpty;
	XP_File			m_out_file;
	uint32			m_ouputBufferSize;
	char			*m_outputBuffer;
	uint32			m_outputBufferIndex;
};



#endif
