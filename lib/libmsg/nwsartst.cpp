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

#include "msg.h"
#include "nwsartst.h"
#include "newshdr.h"
#include "newsdb.h"
#include "xpgetstr.h"
#include "maildb.h"
#include "newspane.h"
#include "mailhdr.h"
#include "msgfinfo.h"
#include "pmsgsrch.h"
#include "prsembst.h"
#include "msgdbapi.h"

extern "C"
{
	extern int MK_OUT_OF_MEMORY;
	extern int MK_MSG_ERROR_WRITING_MAIL_FOLDER;
	extern int MK_MSG_DOWNLOAD_COUNT;
	extern int MK_MSG_RETRIEVING_ARTICLE;
	extern int MK_MSG_RETRIEVING_ARTICLE_OF;
	extern int MK_MSG_ARTICLES_TO_RETRIEVE;
	extern int MK_NNTP_SERVER_ERROR;
}


class MSG_FolderInfo;

// This file contains the news article download state machine.

extern "C" NET_StreamClass *
msg_MakeAppendToFolderStream (int /*format_out*/, void * /*closure*/,
							  URL_Struct *url, MWContext *context)
{
	NET_StreamClass *stream;

	TRACEMSG(("Setting up attachment stream. Have URL: %s\n", url->address));

	stream = XP_NEW (NET_StreamClass);
	if (!stream) return 0;
	XP_MEMSET (stream, 0, sizeof (NET_StreamClass));

	stream->name           = "Folder Append Stream";
	stream->complete       = DownloadArticleState::MSG_SaveToStreamComplete;
	stream->abort          = DownloadArticleState::MSG_SaveToStreamAbort;
	stream->put_block      = DownloadArticleState::MSG_SaveToStreamWrite;
	stream->is_write_ready = DownloadArticleState::MSG_SaveToStreamWriteReady;
	stream->data_object    = url->fe_data;
	stream->window_id      = context;

	TRACEMSG(("Returning stream from msg_MakeAppendToFolderStream"));

	return stream;
}

// if pIds is not null, download the articles whose id's are passed in. Otherwise,
// which articles to download is determined by DownloadArticleState object,
// or subclasses thereof. News can download marked objects, for example.
int DownloadArticleState::DownloadArticles(MSG_FolderInfo *folder, MWContext *context, IDArray *pIds)
{
	if (pIds != NULL)
		m_keysToDownload.InsertAt(0, pIds);

	if (m_keysToDownload.GetSize() > 0)
		m_downloadFromKeys = TRUE;

	m_folder = folder;
	m_context = context;

	if (GetNextHdrToRetrieve() != eSUCCESS)
		return MK_CONNECTED;

	return DownloadNext(TRUE, context);
}

/* Saving news messages
 */

DownloadArticleState::DownloadArticleState(int (*doneCB)(void *, int), void *state, MessageDB *msgDB)
{
	m_numwrote = 0;
	m_finalExit = 0;
	m_downloadFromKeys = FALSE;
	m_doneCB = doneCB;
	m_doneCBState = state;
	m_newsDB = msgDB;
	if (m_newsDB) // acquire our ref count so it can't go away on us..Bug #97825
		m_newsDB->AddUseCount();
}

DownloadArticleState::~DownloadArticleState()
{
	if (m_doneCB)
		(*m_doneCB)(m_doneCBState, m_status);
	if (m_newsDB)
	{
		m_newsDB->Close();
		m_newsDB = NULL;
	}
}

int DownloadArticleState::DownloadNext(XP_Bool firstTimeP, MWContext *context)
{
	URL_Struct *url_struct = NULL;

//	if (GetNextHdrToRetrieve() != eSUCCESS)
//		return 0;
	char *url = m_folder->BuildUrl(m_newsDB, m_keyToDownload);
	m_numwrote++;
	if (url)
		url_struct = NET_CreateURLStruct (url, NET_DONT_RELOAD);
	if (!url_struct || !url) return MK_OUT_OF_MEMORY;

	StartDownload();
	if (firstTimeP)
	{
		XP_ASSERT (!m_finalExit);
		FE_SetWindowLoading (context, url_struct, &m_finalExit);
		XP_ASSERT (m_finalExit);
	}
	url_struct->fe_data = (void *) this;
	m_wroteAnyP = FALSE;
#ifdef DEBUG_bienvenu
//	XP_Trace("downloading %s\n", url);
#endif
	NET_GetURL (url_struct, FO_CACHE_AND_APPEND_TO_FOLDER,
		context, DownloadArticleState::MSG_SaveNextExit);
	XP_FREE (url);

	return 0;
}

MsgERR DownloadNewsArticlesToNewsDB::GetNextHdrToRetrieve()
{
	MsgERR	dbErr;

	if (m_downloadFromKeys)
		return DownloadArticleState::GetNextHdrToRetrieve();

	while (TRUE)
	{
		if (m_listContext == NULL)
			dbErr = m_newsDB->ListFirst (&m_listContext, (DBMessageHdr **) &m_newsHeader);
		else
			dbErr = m_newsDB->ListNext(m_listContext, (DBMessageHdr **) &m_newsHeader);

		if (dbErr == eDBEndOfList)
			break;
		if (m_newsHeader->GetFlags() & kMsgMarked)
		{
			m_keyToDownload = m_newsHeader->GetMessageKey();
			char *statusTemplate = XP_GetString (MK_MSG_RETRIEVING_ARTICLE);
			char *statusString = PR_smprintf (statusTemplate,  m_numwrote);
			if (statusString)
			{
				FE_Progress (m_context, statusString);
				XP_FREE(statusString);
			}
			break;
		}
		else
		{
			delete m_newsHeader;
			m_newsHeader = NULL;
		}
	}
	if (m_newsHeader && m_dbWriteDocument)
		MSG_OfflineMsgDocumentHandle_SetMsgHeaderHandle(m_dbWriteDocument, m_newsHeader, m_newsDB->GetDB());
	return dbErr;
}

void DownloadArticleState::Abort() {}
void DownloadArticleState::Complete() {}

/* static */ void DownloadArticleState::MSG_SaveNextExit(URL_Struct *url , int status, MWContext *context)
{
	DownloadArticleState *downloadState =
		(DownloadArticleState *) url->fe_data;

	XP_ASSERT (downloadState);
	if (!downloadState) return;

	if (!downloadState->SaveExit(url, status, context))
		delete downloadState;
}

MsgERR DownloadArticleState::GetNextHdrToRetrieve()
{
	if (m_downloadFromKeys)
	{
		if (m_numwrote >= (int32) m_keysToDownload.GetSize())
			return eDBEndOfList;
		m_keyToDownload = m_keysToDownload.GetAt(m_numwrote);
#ifdef DEBUG_bienvenu
//		XP_Trace("downloading %ld index = %ld\n", m_keyToDownload, m_numwrote);
#endif
		char *statusTemplate = XP_GetString (MK_MSG_RETRIEVING_ARTICLE_OF);
		char *statusString = PR_smprintf (statusTemplate,  m_numwrote, (long) m_keysToDownload.GetSize(), m_folder->GetPrettiestName());
		if (statusString)
		{
			FE_Progress (m_context, statusString);
			XP_FREE(statusString);
		}

		int32 percent;
		percent = (100L * m_numwrote) / (uint32) m_keysToDownload.GetSize();
		FE_SetProgressBarPercent (m_context, percent);
		return eSUCCESS;
	}
	XP_ASSERT(FALSE);
	return eBAD_PARAMETER;	// shouldn't get here if we're not downloading from keys.
}

XP_Bool DownloadArticleState::SaveExit(URL_Struct *url, int status, MWContext *context)
{

	char *statusTemplate = XP_GetString (MK_MSG_DOWNLOAD_COUNT);
	char *statusString = PR_smprintf (statusTemplate,  (long)m_numwrote, (long)m_keysToDownload.GetSize());

	FE_Progress (context, statusString);
	XP_FREE(statusString);

	m_status = status;
	// we should try to go on from server error in case it's an article not found error.
	if ((status < 0 && status != MK_NNTP_SERVER_ERROR) || (GetNextHdrToRetrieve() != eSUCCESS))
	{
	FAIL:
		status = FinishDownload();
		if (m_finalExit)	// call exit routine
			m_finalExit (url, status, context);

		return FALSE;
	}
	else
	{
		status = DownloadNext(FALSE, context);
		if (status < 0) goto FAIL;
		return TRUE;
	}
}

XP_Bool DownloadNewsArticlesToNewsDB::SaveExit(URL_Struct *url, int status, MWContext *context)
{
	m_status = status;
	if (m_newsHeader != NULL)
	{
#ifdef DEBUG_bienvenu
//		XP_Trace("finished retrieving %ld\n", m_newsHeader->GetMessageKey());
#endif
		if (m_newsDB)
		{
			m_newsDB->MarkOffline(m_newsHeader->GetMessageKey(), TRUE, NULL);
			m_newsDB->MarkMarked(m_newsHeader->GetMessageKey(), FALSE, NULL);
			m_newsDB->Commit();
		}
		delete m_newsHeader;
	}	
	m_newsHeader = NULL;
	return DownloadArticleState::SaveExit(url, status, context);
}

int DownloadArticlesToFolder::FinishDownload()
{
	int status = 0;
	MsgERR err = eSUCCESS;
	if (XP_FileClose(m_outFp) != 0) 
	{
		if (status >= 0) 
			status = MK_MSG_ERROR_WRITING_MAIL_FOLDER;
	}

	if (m_dstDatabase)
	{
		if (m_summaryValidP)
			m_dstDatabase->SetFolderInfoValid(m_fileName, 0, 0);
		err = m_dstDatabase->Close();
		m_dstDatabase = NULL;
	}

	FREEIF (m_fileName);

	err = m_newsDB->Close(); /* Don't close DB, because creator is responsible for it. */
	m_newsDB = NULL;
	if (m_dstFolder)
		m_dstFolder->ReleaseSemaphore(this);

	return ConvertMsgErrToMKErr(err);
}

int DownloadNewsArticlesToNewsDB::FinishDownload()
{
/*	MsgERR	err = m_newsDB->Close(); Don't close DB, because creator is responsible for it. */
	return 0;
}


/*static*/ unsigned int
DownloadArticleState::MSG_SaveToStreamWriteReady(NET_StreamClass* /*stream*/)
{	
  return MAX_WRITE_READY;
}

/*static*/ void DownloadArticleState::AddKeyToDownload(void *state, MessageKey key)
{
	DownloadArticleState *newsArticleState = (DownloadArticleState *) state;
	if (newsArticleState != NULL)
	{
		// only need to download articles we don't already have...
		if (!newsArticleState->m_newsDB->GetNewsDB()->IsArticleOffline(key))
		{
			newsArticleState->m_keysToDownload.Add(key);
			char *statusTemplate = XP_GetString (MK_MSG_ARTICLES_TO_RETRIEVE);
			char *statusString = PR_smprintf (statusTemplate,  (long) newsArticleState->m_keysToDownload.GetSize());
			if (statusString)
			{
				FE_Progress (newsArticleState->m_context, statusString);
				XP_FREE(statusString);
			}
		}
	}
}

/*static*/ void DownloadArticleState::MSG_SaveToStreamComplete(NET_StreamClass *stream)
{
	DownloadArticleState *newsArticleState = (DownloadArticleState *) stream->data_object;	
	newsArticleState->Complete();
}

/*static*/ void DownloadArticleState::MSG_SaveToStreamAbort (NET_StreamClass *stream, int /*status*/)
{
	DownloadArticleState *newsArticleState = (DownloadArticleState *) stream->data_object;	
	if (newsArticleState)
	{
		newsArticleState->Abort();
	}
}

/*static*/ int DownloadArticleState::MSG_SaveToStreamWrite (NET_StreamClass *stream, const char *block, int32 length)
{
	DownloadArticleState *newsArticleState = (DownloadArticleState *) stream->data_object;	

	if (newsArticleState != 0)
		return newsArticleState->Write(block, length);

	return 0;
}

int DownloadNewsArticlesToNewsDB::StartDownload()
{
	delete m_newsHeader;
	m_newsHeader = (NewsMessageHdr *) m_newsDB->GetDBHdrForKey(m_keyToDownload);
	if (m_dbWriteDocument)
		MSG_OfflineMsgDocumentHandle_SetMsgHeaderHandle(m_dbWriteDocument, m_newsHeader, m_newsDB->GetDB());
	return 0;
}

int DownloadNewsArticlesToNewsDB::Write(const char *block, int32 length)
{
	int32 ret = 0;

	if (m_newsHeader != NULL)
	{

		ret = MSG_OfflineMsgDocumentHandle_AddToOfflineMessage(m_dbWriteDocument, block, length);
//		ret = m_newsHeader->AddToArticle(block, length, m_newsDB->GetDB());
		if (ret > 0)
			ret = 0;
		else
			ret = MK_OUT_OF_MEMORY;
	}
	return ret;
}

void DownloadNewsArticlesToNewsDB::Complete()
{
	MSG_OfflineMsgDocumentHandle_Complete(m_dbWriteDocument);
}

DownloadArticlesToFolder::DownloadArticlesToFolder(int (*doneCB)(void *, int), void *state, MessageDB *msgDB)
	: DownloadArticleState(doneCB, state, msgDB)
{
	m_dstFolder = NULL;
	m_dstDatabase = NULL;
	m_lastOffset = 0;
	m_outgoingParser = NULL;
}

DownloadArticlesToFolder::~DownloadArticlesToFolder()
{
	if (m_dstDatabase)
		m_dstDatabase->Close ();
}

XP_Bool DownloadArticlesToFolder::SaveExit(URL_Struct *url, int status, MWContext *context)
{
	// One message has just been copied into the mail folder, and its URL is done. 
	// Before we go on to the next message, get the DB header out of the news
	// database and add it into the mail database

	m_status = status;
	if (m_dstDatabase)
	{
		MailMessageHdr *dstHdr = NULL;
		DBMessageHdr *srcHdr = m_newsDB->GetDBHdrForKey (m_keyToDownload);
		if (m_outgoingParser)
		{
			m_outgoingParser->FlushOutputBuffer();
			dstHdr = m_outgoingParser->m_newMsgHdr;
			if (srcHdr && dstHdr)
				dstHdr->SetFlags(srcHdr->GetFlags());
		}
		else
		{
			if (srcHdr)
				dstHdr = new MailMessageHdr ();
			if (dstHdr)
				dstHdr->CopyFromMsgHdr (srcHdr, m_newsDB->GetDB(), m_dstDatabase->GetDB());
		}
		if (dstHdr)
		{
			dstHdr->SetMessageKey (m_lastOffset);
			int32 curPosition = ftell (m_outFp);
			dstHdr->SetByteLength(curPosition - m_lastOffset);
			dstHdr->SetMessageSize(dstHdr->GetByteLength());
			m_lastOffset = curPosition;
			m_dstDatabase->AddHdrToDB (dstHdr, NULL, TRUE);
		}
		delete dstHdr;
	}
	return DownloadArticleState::SaveExit(url, status, context);
}

DownloadNewsArticlesToNewsDB::DownloadNewsArticlesToNewsDB(int (*doneCB)(void *, int), void *state, MessageDB *msgDB)
	: DownloadArticleState(doneCB, state, msgDB)
{
	m_listContext = NULL;
	m_dbWriteDocument = MSG_OfflineMsgDocumentHandle_Create(msgDB->GetDB(), NULL);
	m_newsDB = msgDB;
}

DownloadNewsArticlesToNewsDB::~DownloadNewsArticlesToNewsDB()
{
	MSG_OfflineMsgDocumentHandle_Destroy(m_dbWriteDocument);
}

int DownloadArticlesToFolder::DownloadNext(XP_Bool firstTimeP, MWContext *context)
{
	if (m_outgoingParser)
		m_outgoingParser->Clear();
	return DownloadArticleState::DownloadNext(firstTimeP, context);
}

/*static*/ int
DownloadArticlesToFolder::SaveMessages(IDArray *array, const char *file_name,
						 MSG_Pane *pane, MSG_FolderInfo *folder, MessageDB *msgDB, int (*doneCB)(void *, int), void *state)
{
	XP_File	outFp = XP_FileOpen (file_name, xpMailFolder, XP_FILE_APPEND_BIN);

	if (!outFp)
		return MK_MSG_ERROR_WRITING_MAIL_FOLDER;

	DownloadArticlesToFolder *downloadState = new DownloadArticlesToFolder(doneCB, state, msgDB);
	if (downloadState)
	{
		downloadState->m_outFp = outFp;
		downloadState->m_fileName = XP_STRDUP (file_name);
		XP_FileSeek(outFp, 0, SEEK_END);
		downloadState->m_lastOffset = XP_FileTell(outFp);

		// Find out whether the summary file for the destination is valid.
		// We need this to know whether to re-validate it after adding headers
		XP_StatStruct folderStat;
		XP_Stat (file_name, &folderStat, xpMailFolder);

		// Find the destination folderInfo so we can make sure it's locked
		// before we try to write anything into it.
		MSG_FolderInfoMail *tree = pane->GetMaster()->GetLocalMailFolderTree();
		downloadState->m_dstFolder = tree->FindPathname (file_name);

		// if we're saving to a file, not a folder, m_dstFolder will be null, so we don't
		// have to lock it.

		// Open the destination summary file so we know where to put 
		// the headers after the message has been copied. (if we have a folder)
		if (downloadState->m_dstFolder)
		{
			downloadState->m_dstFolder->AcquireSemaphore (downloadState);
			MsgERR err = MailDB::Open (file_name, FALSE, &downloadState->m_dstDatabase);
			downloadState->m_summaryValidP  = (err == eSUCCESS && downloadState->m_dstDatabase != NULL);
			downloadState->m_outgoingParser = new ParseOutgoingMessage;
			if (downloadState->m_outgoingParser)
			{
				downloadState->m_outgoingParser->SetOutFile(downloadState->m_outFp);
				downloadState->m_outgoingParser->SetMailDB(downloadState->m_dstDatabase);
				downloadState->m_outgoingParser->SetWriteToOutFile(TRUE);
			}
		}

		return downloadState->DownloadArticles(folder, pane->GetContext(), array);
	}
	else
		return MK_OUT_OF_MEMORY;
}

/*static*/ int	DownloadNewsArticlesToNewsDB::SaveMessages(MWContext *context, MSG_FolderInfo *folder, MessageDB *newsDB, IDArray *pKeys)
{
	DownloadNewsArticlesToNewsDB *downloadState = new DownloadNewsArticlesToNewsDB(NULL, NULL, newsDB);
	if (downloadState)
		return downloadState->DownloadArticles(folder, context, pKeys);
	else
		return MK_OUT_OF_MEMORY;
}
/* static */ int DownloadNewsArticlesToNewsDB::DoIt(void *closure)
{
	int	ret;
	DownloadNewsArticlesToNewsDB *downloadState = (DownloadNewsArticlesToNewsDB *) closure;
	ret = downloadState->DownloadArticles(downloadState->m_folder,  
				downloadState->m_context, NULL);
	if (ret != 0)
		delete downloadState;
	return ret;
}

/*static*/ int DownloadMatchingNewsArticlesToNewsDB::SaveMatchingMessages(MWContext *context, MSG_FolderInfo *folder, MessageDB *newsDB, 
							MSG_SearchTermArray &termArray, int (*doneCB)(void *, int), void *state)
{
	MSG_OfflineNewsSearchPane *searchPane = new MSG_OfflineNewsSearchPane(context, folder->GetMaster());
	DownloadMatchingNewsArticlesToNewsDB *downloadState = 
		new DownloadMatchingNewsArticlesToNewsDB(context, folder, newsDB, searchPane, termArray, doneCB, state);

	MSG_SearchAlloc(searchPane);
	MSG_AddScopeTerm(searchPane, scopeOfflineNewsgroup, folder);
	for (int i = 0; i < termArray.GetSize(); i++)
	{
		MSG_SearchTerm * term = (MSG_SearchTerm *) termArray.GetAt(i);
		MSG_AddSearchTerm(searchPane, term->m_attribute, term->m_operator, &term->m_value, term->m_booleanOp, term->m_arbitraryHeader);
	}

	MSG_SearchFrame *frame = MSG_SearchFrame::FromPane (searchPane);
	URL_Struct *urlStruct = NET_CreateURLStruct ("search-libmsg:", NET_DONT_RELOAD);
	frame->m_urlStruct = urlStruct;
	// Set the internal_url flag so just in case someone else happens to have
	// a search-libmsg URL, it won't fire my code, and surely crash.
	urlStruct->internal_url = TRUE;

	searchPane->SetExitFunc(DownloadNewsArticlesToNewsDB::DoIt, downloadState);
	searchPane->SetReportHitFunction(DownloadArticleState::AddKeyToDownload, downloadState);
	if (MSG_Search(searchPane) != SearchError_Success)	// we got a search error, but we should try to go on.
	{
		XP_ASSERT(FALSE);
		return 0;	
	}
	return 0;
}

DownloadMatchingNewsArticlesToNewsDB::DownloadMatchingNewsArticlesToNewsDB
	(MWContext *context, MSG_FolderInfo *folder, MessageDB *newsDB,
	 MSG_OfflineNewsSearchPane *searchPane,
	 MSG_SearchTermArray & /*termArray*/,
	 int (*doneCB)(void *, int), void *state)	: DownloadNewsArticlesToNewsDB(doneCB, state, newsDB)
{
	m_context = context;
	m_folder = folder;
	m_newsDB = newsDB;
	m_searchPane = searchPane;
	m_downloadFromKeys = TRUE;	// search term matching means downloadFromKeys.
}

DownloadMatchingNewsArticlesToNewsDB::~DownloadMatchingNewsArticlesToNewsDB()
{
	MSG_SearchFree (m_searchPane);
	delete m_searchPane;
}


int DownloadArticlesToFolder::Write(const char *block, int32 length)
{
	int32 len = 0;
	
	// Make sure we have the folder locked before we scribble on it
	// If it is a folder. If it's not (e.g., just a save as file), we can't
	// get a MSG_FolderInfo * in the first place...
	XP_Bool locked = (m_dstFolder) ? m_dstFolder->TestSemaphore(this) : TRUE;
	XP_ASSERT(locked);
	if (!locked)
		return -1;

	/* We implicitly assume that the data we're getting is already following
	 the local line termination conventions; the NNTP module has already
	 converted from CRLF to the local linebreak for us. */

	if (!m_wroteAnyP)
	{
		const char *envelope = msg_GetDummyEnvelope();

		m_existedP = TRUE;
		if (m_outgoingParser)
			m_outgoingParser->StartNewEnvelope(envelope, XP_STRLEN(envelope));
		else
		{
			len = XP_FileWrite (envelope, XP_STRLEN (envelope), m_outFp);
			if (len != XP_STRLEN(envelope))
				return len;
		}
		m_wroteAnyP = TRUE;
	}

#ifdef MANGLE_INTERNAL_ENVELOPE_LINES
  /* Since we're writing a mail folder, we must follow the Usual Mangling
	 Conventions.   #### Note: this assumes that `block' contains just
	 a single line, which is the case since this is only invoked on the
	 result of an NNTP stream, which does line buffering.

     Note: it is correct to mangle all lines beginning with "From ",
	 not just those that look like parsable message delimiters.
	 Other software expects this.
   */

	if (block[0] == 'F' && !XP_STRNCMP (block, "From ", 5))
	{
		len = XP_FileWrite (">", 1, m_outFp);
		if (len != 1)
			return len;
		m_outgoingParser->m_position += 1;
	}
#endif /* MANGLE_INTERNAL_ENVELOPE_LINES */

	if (m_outgoingParser)
		return m_outgoingParser->ParseBlock(block, length);
	else
		return XP_FileWrite(block, length, m_outFp);
}

