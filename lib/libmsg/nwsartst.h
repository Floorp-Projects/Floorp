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
#ifndef NewsArtState_H
#define NewsArtState_H

class MailDB;
class MessageDB;
class NewsMessageHdr;
class MSG_SearchTermArray;
class MSG_OfflineNewsSearchPane;
class ParseOutgoingMessage;

struct ListContext;
#include "idarray.h"
#include "msgzap.h"
#include "dberror.h"
#include "msgdbtyp.h"

class DownloadArticleState : public MSG_ZapIt
{
public:
	DownloadArticleState(int (*doneCB)(void *, int), void *state, MessageDB	*msgDB);
	virtual ~DownloadArticleState();

	virtual int DownloadArticles(MSG_FolderInfo *folder, MWContext *context, IDArray *pKeyArray);
	static unsigned int MSG_SaveToStreamWriteReady (NET_StreamClass *stream);
	static void MSG_SaveToStreamComplete(NET_StreamClass *stream);
	static void MSG_SaveToStreamAbort (NET_StreamClass *stream, int status);
	static int MSG_SaveToStreamWrite (NET_StreamClass *stream, const char *block, int32 length);
	static void MSG_SaveNextExit(URL_Struct *, int, MWContext *);
	static void AddKeyToDownload(void *state, MessageKey key);
protected:
	virtual int Write(const char * /*block*/, int32 length) {return length;}
	virtual void Abort();
	virtual void Complete();
	virtual XP_Bool SaveExit(URL_Struct *, int, MWContext *);
	virtual MsgERR GetNextHdrToRetrieve();
	virtual int DownloadNext(XP_Bool firstTimeP, MWContext *);
	virtual int FinishDownload() {return 0;}
	virtual int	StartDownload() {return 0;}

	IDArray			m_keysToDownload;
	XP_Bool			m_downloadFromKeys;
	MSG_FolderInfo	*m_folder;
	MessageDB		*m_newsDB;
	MWContext		*m_context;
	XP_Bool			m_existedP;
	XP_Bool			m_wroteAnyP;
	XP_Bool			m_summaryValidP;
	int32			m_numwrote;
	Net_GetUrlExitFunc *m_finalExit;
	MessageKey		m_keyToDownload;
	int				m_status;
	int				(*m_doneCB)(void *, int status);
	void			*m_doneCBState;
};

class DownloadArticlesToFolder: public DownloadArticleState
{
public:
	DownloadArticlesToFolder(int (*doneCB)(void *, int status), void *state, MessageDB	*msgDB);
	virtual ~DownloadArticlesToFolder();
	static 	int SaveMessages(IDArray*, const char *file_name, MSG_Pane *pane, 
					 MSG_FolderInfo *folder, MessageDB *newsDB, int (*doneCB)(void *, int status) = NULL, void *state = NULL);
protected:
	char			*m_fileName;
	XP_File			m_outFp;
	MSG_FolderInfo	*m_dstFolder;
	int				m_lastOffset;
	MailDB			*m_dstDatabase;
	ParseOutgoingMessage *m_outgoingParser;

	virtual int Write(const char *block, int32 length);
	virtual int FinishDownload();
	virtual int DownloadNext(XP_Bool firstTimeP, MWContext *context);
	virtual XP_Bool SaveExit(URL_Struct *, int, MWContext *);
};

class DownloadNewsArticlesToNewsDB : public DownloadArticleState
{
public:
	DownloadNewsArticlesToNewsDB(int (*doneCB)(void *, int status), void *state, MessageDB *newsDB);
	virtual ~DownloadNewsArticlesToNewsDB();

static int	SaveMessages(MWContext *context, MSG_FolderInfo *folder, MessageDB *newsDB);
static int	SaveMessages(MWContext *context, MSG_FolderInfo *folder, MessageDB *newsDB, IDArray *pKeys);
static int	DoIt(void *closure);
protected:
	virtual int Write(const char *block, int32 length);
	virtual void Complete();
	virtual XP_Bool SaveExit(URL_Struct *url, int status, MWContext *context);
	virtual int	StartDownload();
	virtual int FinishDownload();
	virtual MsgERR	GetNextHdrToRetrieve();

	ListContext		*m_listContext;
	NewsMessageHdr	*m_newsHeader;
	MSG_OfflineMsgDocumentHandle m_dbWriteDocument;
};

class DownloadMatchingNewsArticlesToNewsDB : public DownloadNewsArticlesToNewsDB
{
public:
	DownloadMatchingNewsArticlesToNewsDB(MWContext *context, MSG_FolderInfo *folder, MessageDB *newsDB, 
		MSG_OfflineNewsSearchPane *searchPane, MSG_SearchTermArray &termArray, int (*doneCB)(void *, int status), void *state);
	virtual ~DownloadMatchingNewsArticlesToNewsDB();
static int	SaveMatchingMessages(MWContext *context, MSG_FolderInfo *folder, MessageDB *newsDB, 
								 MSG_SearchTermArray &termArray, int (*doneCB)(void *, int status), void *state);
protected:
	MSG_OfflineNewsSearchPane	*m_searchPane;
};

#endif
