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
#include "xp.h"
#include "prsembst.h"
#include "maildb.h"
#include "mailhdr.h"
#include "xpgetstr.h"
#include "grpinfo.h"
#include "msgfinfo.h"
#include "msgcmfld.h"
#include "msgtpane.h"
extern "C"
{
	extern int MK_MSG_COMPRESS_FOLDER_ETC;
	extern int MK_OUT_OF_MEMORY;
	extern int MK_UNABLE_TO_OPEN_FILE;
	extern int MK_UNABLE_TO_DELETE_FILE;
	extern int MK_MSG_ERROR_WRITING_MAIL_FOLDER;
	extern int MK_MSG_FOLDER_UNREADABLE;
	extern int MK_MSG_COMPRESS_FOLDER_ETC;
	extern int MK_MSG_COMPRESS_FAILED;
}

/* Folder compression. We inherit from ParseMailboxState so we can
   reparse the folder if needed, and share its parsing code in 
   general.
 */

 /* The algorithm is as follows:
	
	Open the database for the mail folder. If this fails, we can't tell if the
	folder needs compressing or not. We could reparse the folder to determine
	if any messages have been expunged. Let's do that, for fun.

    Let's pretend we could open the database. Check if DBFolderInfo->m_expungedBytes
	== 0. If so, no messages have been expunged, so we're done.
	
	Otherwise, we need to compress. We could either use the database, or use
	the msg flags in the folder to determine if a message should be copied over.
	Ideally, the database is cheaper, since we can skip over expunged messages.

    
    So let's try that road. Open a temp output folder and temp output database.
	Iterate through the source db, check each header, if not expunged, read the 
	message and write it out to the temp output folder and db. When done, rename
	the temp folder to the original folder, and the temp database to the
	original database (after closing each). 

  */


/*
----------------------------------------------------
class MSG_CompressFolderState
----------------------------------------------------
*/

MSG_CompressFolderState::MSG_CompressFolderState(MSG_Master *master,
												 MWContext *context,
												 URL_Struct *url,
												 MSG_FolderInfoMail *mailFolderInfo,
												 int32 progressBytesDoneOffset,
												 int32 progressBytesTotal)
 : MSG_CompressState(master, context, url), ParseMailboxState(mailFolderInfo->GetPathname())
{
	m_compressState = CFStart;
	m_listContext = NULL;
	m_outPosition = 0;
	m_folderInfo = NULL;
	m_folder = mailFolderInfo;
//	m_newMailHdr = NULL;
	m_newMailDB = NULL;
	m_srcMailDB = NULL;
	m_infile = NULL;
	m_outfile = NULL;
	m_pHeader = NULL;
	m_bytesLeftInMessage = 0;
	m_bytesInMessage = 0;
	m_progressBytesDoneOffset = progressBytesDoneOffset;
	m_progressBytesTotalOffset = progressBytesTotal;
	m_context = context;
	m_mailboxName = mailFolderInfo->GetPathname();
	m_ignoreNonMailFolder = TRUE;
	SetMaster(master);
}

MSG_CompressFolderState::~MSG_CompressFolderState()
{
	if (m_srcMailDB != NULL)
		m_srcMailDB->ListDone (m_listContext);
//	if (m_newMailHdr != NULL)
//		delete m_newMailHdr;
	FREEIF(m_obuffer);
}

int MSG_CompressFolderState::BeginCompression(void)
{
	int bufSize = 10240;
#ifdef DEBUG_bienvenu
	XP_Trace("begin compression\n");
#endif
	m_obuffer = (char *) XP_ALLOC(bufSize);	// what would be a good size?
	while (m_obuffer == NULL)
	{
		bufSize /= 2;
		m_obuffer = (char *) XP_ALLOC(bufSize);
		if (!m_obuffer && (bufSize < 1024))
		{
			m_obuffer_size = 0;
			return MK_OUT_OF_MEMORY;
		}
	}

	m_obuffer_size = bufSize;
	m_master->BroadcastFolderKeysAreInvalid (m_folder);

	return 0;
}

int MSG_CompressFolderState::FinishCompression()
{
#ifdef DEBUG_bienvenu
	XP_Trace("finish compression\n");
#endif
	// does this always get called, even if an error occurs or we get interrupted?
	CloseFiles(FALSE);	// do we care what return value is?
	return 0;
}

int MSG_CompressFolderState::StartOutput()
{
	/* Print a status message, and open up the output file. */

	const char *fmt = XP_GetString(MK_MSG_COMPRESS_FOLDER_ETC); /* #### i18n */
	const char *f = m_mailboxName;
	char *s = XP_STRRCHR (f, '/');
	if (s)
		f = s+1;
	char *unEscapedF = XP_STRDUP(f);
	if (unEscapedF)
	{
		NET_UnEscape(unEscapedF); // Mac apparently stores as "foo%20bar"
		s = PR_smprintf (fmt, unEscapedF);
		if (s)
		{
			FE_Progress(m_context, s);
			XP_FREE(s);
		}
		XP_FREE(unEscapedF);
	}
	m_graph_progress_total =
	  m_folderInfo->m_folderSize - m_folderInfo->m_expunged_bytes;

	if (m_graph_progress_total < 1) 
	{
		/* Pure paranoia. */
		m_graph_progress_total = 1;
	}
	m_graph_progress_received = 0;
	FE_SetProgressBarPercent(m_context, 0);


	m_tmpName = FE_GetTempFileFor(m_context, m_mailboxName, xpMailFolder,
									   &tmptype);
	if (!m_tmpName) return MK_OUT_OF_MEMORY;	/* ### */
	m_outfile = XP_FileOpen(m_tmpName, xpMailFolder,
								 XP_FILE_WRITE_BIN);
	if (!m_outfile) return MK_UNABLE_TO_OPEN_FILE;

	// setup outgoing parser to make sure mozilla-status gets written out.
	ParseOutgoingMessage *outgoingParser = new ParseOutgoingMessage;
	if (!outgoingParser)
		return MK_OUT_OF_MEMORY;
	outgoingParser->SetOutFile(m_outfile);
	SetMailMessageParseState(outgoingParser);
	
	m_tmpdbName = FE_GetTempFileFor(m_context, m_mailboxName, xpMailFolderSummary,
		&tmptype);
	if (!m_tmpdbName)
		return MK_OUT_OF_MEMORY;	/* ### do we need to free other stuff? */

	if (MailDB::Open(m_tmpdbName, TRUE, &m_newMailDB, TRUE) != eSUCCESS)
		return MK_UNABLE_TO_OPEN_FILE;
	/* #### We should check here that the folder is writable, and warn.
	   (open it for append, then close it right away?)  Just checking
	   S_IWUSR is not enough.
	 */
	 
	// transfer any needed DBFolderInfo to the new db
	DBFolderInfo *destinationInfo = m_newMailDB->GetDBFolderInfo();
	if (m_folderInfo && destinationInfo)
	{
		TDBFolderInfoTransfer transferInfo(*m_folderInfo);
		transferInfo.TransferFolderInfo(*destinationInfo);
	}
	
	 
	// thanks to use of Multiple Inheritance, we need to change the maildb of the
	// base class to the db we're writing to.
	ParseMailboxState::SetDB(m_newMailDB);
	outgoingParser->SetMailDB(m_newMailDB);	// make sure new header goes to correct DB.

#ifdef XP_UNIX
	/* Clone the permissions of the "real" folder file into the temp file,
	   so that when we rename the finished temp file to the real file, the
	   preferences don't appear to change. */
	{
	  XP_StatStruct st;
	  if (XP_Stat (m_mailboxName, &st, xpMailFolder) == 0)
		/* Ignore errors; if it fails, bummer. */
		/* This is unbelievable.  SCO doesn't define fchmod at all.  no big deal.
		   AIX3, however, defines it to take a char* as the first parameter.  The
		   man page sez it takes an int, though.  ... */
#if defined(SCO_SV) || defined(AIXV3)
	  {
	    		char *really_tmp_file = WH_FileName(m_tmpName, xpMailFolder);

			chmod  (really_tmp_file, (st.st_mode | S_IRUSR | S_IWUSR));

			XP_FREE(really_tmp_file);
	  }
#else
		fchmod (fileno(m_outfile), (st.st_mode | S_IRUSR | S_IWUSR));
#endif
	}
#endif /* XP_UNIX */
	return 0;
}

int MSG_CompressFolderState::OpenFolder()
{
	m_compressState = CFDBOpened;
	m_infile = XP_FileOpen(m_mailboxName, xpMailFolder, XP_FILE_READ_BIN);
	if (!m_infile)
		return MK_MSG_FOLDER_UNREADABLE;
	else
		return MK_WAITING_FOR_CONNECTION;
}

int MSG_CompressFolderState::OpenFolderAndDB()
{
	MsgERR err = MailDB::Open(m_mailboxName, FALSE /* create? */, &m_srcMailDB, FALSE);
	if (err != eSUCCESS)
	{
		err = MailDB::CloneInvalidDBInfoIntoNewDB(m_mailboxName, &m_srcMailDB);
		if (err != eSUCCESS)
			return MK_UNABLE_TO_OPEN_FILE;
		m_compressState = CFParsingSource;
		m_mailDB = m_srcMailDB;
		return BeginOpenFolderSock(m_mailboxName, NULL, 0, NULL);
	}
	else
	{
		return OpenFolder();
	}
}

int MSG_CompressFolderState::StartFirstMessage()
{
	int status;
	status = StartOutput();
	if (status != 0)
		return status;
	m_outPosition = 0;
	return AdvanceToNextNonExpungedMessage();
}

int MSG_CompressFolderState::AdvanceToNextNonExpungedMessage()
{
	int	status = MK_WAITING_FOR_CONNECTION;
	MsgERR dbErr;

	while (TRUE)
	{
		if (m_listContext == NULL)
			dbErr = m_srcMailDB->ListFirst (&m_listContext, (DBMessageHdr **) &m_pHeader);
		else
			dbErr = m_srcMailDB->ListNext(m_listContext, (DBMessageHdr **) &m_pHeader);

		if (dbErr == eDBEndOfList)
		{
			m_compressState = CFFinishedCopyingToTemp;
			break;
		}
		// this currently doesn't happen since ListNext doesn't return errors
		// other than eDBEndOfList.
		else if (dbErr != eSUCCESS)	 
		{
			m_compressState = CFAbortCompress;
			break;
		}
		if (! (m_pHeader->GetFlags() & kExpunged))
		{
			if (XP_FileSeek(m_infile, m_pHeader->GetMessageOffset(), SEEK_SET) != 0)
			{
				m_compressState = CFAbortCompress;
			}
			else
			{
				m_compressState = CFCopyingMsgToTemp;
				m_bytesInMessage = m_bytesLeftInMessage = m_pHeader->GetByteLength();
			}
			if (m_pHeader != NULL)	// dmb - find way to rearrange this code
			{
				delete m_pHeader;
				m_pHeader = NULL;
			}
			break;
		}
		if (m_pHeader != NULL)
		{
			delete m_pHeader;
			m_pHeader = NULL;
		}
	}
	return status;
}

// Copy the next block of a message
int MSG_CompressFolderState::CopyNextBlock()
{
	int status;

	int32 length = m_bytesLeftInMessage;
	// try to get a buffer if we don't have one.
	if (m_obuffer == NULL)
		BeginCompression();

	if (length > (int32) m_obuffer_size)
		length = m_obuffer_size;

	status = XP_FileRead(m_obuffer, length, m_infile);
	if (status < 0) 
		return status;
	if (status == 0) // at end of file? unexpected - Abort
	{
		m_compressState = CFAbortCompress;
	}
	else
	{
		if (m_bytesLeftInMessage == m_bytesInMessage)	// at start of message.
		{
			if (strncmp("From ", m_obuffer, 5))	// should be From - at start of msg.
			{
				m_compressState = CFAbortCompress;
				return MK_WAITING_FOR_CONNECTION;
			}
		}
		// do progress
		m_graph_progress_received += status;
		UpdateProgressPercent();

		m_bytesLeftInMessage -= status;	
		ParseBlock(m_obuffer, status);	// this gets written to m_outfile using the outgoing parser
		m_outPosition += status;
	}
	if (m_bytesLeftInMessage == 0)
	{
		/* End of message.  Flush out any partial line remaining in the buffer. */
		if (m_ibuffer_fp > 0) 
		{
			m_parseMsgState->ParseFolderLine(m_ibuffer, m_ibuffer_fp);
			m_ibuffer_fp = 0;
		}
		status = XP_FileRead(m_obuffer, 7, m_infile);
		if (status > 0)	// if we got any bytes (i.e., not at eof)
		{
			// make sure buffer is null terminated
			if (status < m_obuffer_size)
				m_obuffer[status] = '\0';
			// First, advance past any trailing CRLF's, because sometimes the db header
			// message length does not include trailing CRLF's (in particular, fcc folders
			// write an extra blank line at the end of the previous message before adding
			// a new message.)
			int crlfLen = 0;
			for (crlfLen = 0; crlfLen < status; crlfLen++)
			{
				char curChar = m_obuffer[crlfLen];
				if (curChar != CR && curChar != LF)
					break;
			}
			// Check if we're positioned at the start of the next message,
			// since we're not at eof. If not, DB is probably wrong, so it's risky
			// to keep compressing. It's OK to read from the file here
			// because we're going to seek to the start of the next message later.
			if (XP_STRNCMP("From ", m_obuffer + crlfLen, 5))
			{
				m_compressState = CFAbortCompress;
				return MK_WAITING_FOR_CONNECTION;
			}
		}
		m_compressState = CFGetNextMessage;
	}

	return MK_WAITING_FOR_CONNECTION;
}

int MSG_CompressFolderState::CloseAndRenameFiles()
{
	int status = CloseFiles(TRUE); // pass TRUE to indicate success
	if (status >= 0)
		status = RenameFiles();

	// Flush the DBFolderInfo into the MSG_FolderInfo. Req'd esp. for expunged bytes
	MSG_FolderInfo *folder = m_mailMaster->FindMailFolder (m_mailboxName, FALSE /*create?*/);
	if (folder)
		folder->SummaryChanged();

	if (status == 0)
	{
		// reload thread pane because it's invalid now.
		MSG_ThreadPane* threadPane = NULL;
		if (m_mailMaster != NULL)	
			threadPane = m_mailMaster->FindThreadPaneNamed(m_mailboxName);
		if (threadPane != NULL)
			threadPane->ReloadFolder();
	}
	return (status == 0) ? MK_CONNECTED : status;
}

int MSG_CompressFolderState::CloseFiles(XP_Bool success)
{
	if (m_newMailDB && success)	// if no DB, we're probably not really doing anything.
		DoneParsingFolder();	// force out any remaining data. - might need to override.
	else
		FreeBuffers();
	/* We've finished up this folder. */
	if (m_infile != NULL)
	{
		XP_FileClose(m_infile);
		m_infile = NULL;
	}
	if (m_outfile) 
	{
		if (XP_FileClose(m_outfile) != 0) 
		{
			m_outfile = NULL;
			return MK_MSG_ERROR_WRITING_MAIL_FOLDER;
		}
		m_outfile = NULL;
	}

	// ### mwelch This is a hack designed to ensure that the uncompressed
	//            mail DB isn't closed when the compress ended unsuccessfully.
	//            The real solution to this (which we want to implement) is
	//            to move the (success) code into CloseAndRenameFiles, leaving
	//            CloseFiles to handle the unsuccessful case. 
	//            (Simpler paths instead of simpler fixes.)
	if (success)
	{
		if (m_srcMailDB != NULL)
		{
			if (m_newMailDB)	// if we have a new mail db, we're going to rename.
				m_srcMailDB->ForceClosed();
			else				// otherwise, just release our reference so we don't have to reload folder.
				m_srcMailDB->Close();
			m_srcMailDB = NULL;
		}
		if (m_newMailDB != NULL)
		{
			UpdateDBFolderInfo(m_newMailDB, m_tmpName);	// fix folder info in temp file.
			m_folder->SetExpungedBytesCount(0);
			m_newMailDB->Close();
			m_newMailDB = NULL;
		}
	}
	else
	{
		if (m_srcMailDB != NULL)
		{
			// Don't force the db closed in this case, because the thread pane
			// still refers to it.
			m_srcMailDB->Close();
			m_srcMailDB = NULL;
		}
		if (m_newMailDB != NULL)
		{
			// Force the new (compressed) mail db closed, because we won't use it.
			m_newMailDB->ForceClosed();
			m_newMailDB = NULL;

			// Delete the files that would have been the new mailbox and db.
			XP_FileRemove(m_tmpName, xpMailFolder);
			XP_FileRemove(m_tmpdbName, xpMailFolderSummary);
		}
	}
	
	FREEIF(m_obuffer);
	m_obuffer = NULL;
	m_obuffer_size = 0;
	return MK_CONNECTED;
}

int MSG_CompressFolderState::RenameFiles()
{
	int status = 0;
	if (m_tmpdbName) 
	{
		if (MessageDB::RenameDB(m_tmpdbName, m_mailboxName) != eSUCCESS)
			status = MK_UNABLE_TO_DELETE_FILE;
//		XP_FileRename(m_tmpdbName, tmptype, m_mailboxName, xpMailFolderSummary);
		XP_FREE(m_tmpdbName);
		m_tmpdbName = NULL;
	}
	if (status == 0 && m_tmpName) 
	{
#ifdef XP_MAC
		// FIX ME!  tmpType == xpMailFolderSummary at this point, and if you use it,
		// this call will not be very good to us... jrm 97/03/13
		status = XP_FileRename(m_tmpName, xpMailFolder, m_mailboxName, xpMailFolder);
#else
		status = XP_FileRename(m_tmpName, tmptype, m_mailboxName, xpMailFolder);
#endif
		XP_FREE(m_tmpName);
		m_tmpName = NULL;
	}

	return status;
}

int MSG_CompressFolderState::CompressSomeMore()
{
	int status;

	// m_context = context;
	switch (m_compressState)
	{
	case CFStart:
		return OpenFolderAndDB();
	case CFParsingSource:
		status = ParseMoreFolderSock(m_mailboxName, NULL, 0, NULL);
		// if it's not a mail folder, status will be MK_CONNECTED but
		// GetIsRealMailFolder() will be FALSE.
		if (status < 0 || (status == MK_CONNECTED && !GetIsRealMailFolder()))
		{
			CloseFolderSock (m_mailboxName, NULL, 0, NULL);
			m_srcMailDB = NULL; //###phil hack. too many cleanup mechanisms here
			CloseFiles(FALSE);
			return status;	
		}

		if (status == MK_CONNECTED)
		{
			CloseFolderSock(m_mailboxName, NULL, 0, NULL);
			if (m_folder)
				m_folder->SummaryChanged();
			return OpenFolder();
		}

		return MK_WAITING_FOR_CONNECTION;
	case CFDBOpened:
 
		m_folderInfo = m_srcMailDB->GetDBFolderInfo();
		if (m_folderInfo->m_expunged_bytes == 0)		// nothing to do!
			return CloseFiles(FALSE);
		return StartFirstMessage();
	case CFCopyingMsgToTemp:
		return CopyNextBlock();
	case CFGetNextMessage:
		return AdvanceToNextNonExpungedMessage();
	case CFAbortCompress:
		XP_Trace("aborting compress!\n");
		FE_Alert(m_context, XP_GetString(MK_MSG_COMPRESS_FAILED));
		m_srcMailDB->SetSummaryValid(FALSE);
		return CloseFiles(FALSE);
	case CFFinishedCopyingToTemp:
		return CloseAndRenameFiles();
	default:
		XP_ASSERT(FALSE);
		break;
	}
	return MK_CONNECTED;	// ### dmb or not - really an internal error
}


/*
----------------------------------------------------
class MSG_CompressAllFoldersState
----------------------------------------------------
*/

MSG_CompressAllFoldersState::MSG_CompressAllFoldersState(MSG_Master *master,
														 MWContext *context,
														 URL_Struct *url)
	: MSG_CompressState(master, context, url), 
	  m_bytesCompressed(0)
{
	m_folders = m_master->GetLocalMailFolderTree();

	// Count the total number of unexpunged bytes in all folders.
	// ### mw Can't do this until I know how to properly open a mail folder
	//        just to peek at its size.
// 	MSG_FolderInfo *i;
// 	m_bytesTotal = 0;

 	m_iter = new MSG_FolderIterator(m_folders);
 	XP_ASSERT(m_iter != NULL);	// ### mw How can we handle this failure gracefully?

// 	while((i = m_iter->Next()) != NULL)
// 	{
// 		m_bytesTotal += i->m_folderSize - i->m_expunged_bytes;
// 	}

	// Reset our iterator and get the first folder.
	m_iter->Init();
	m_currentFolder = NULL;
	NextMailFolder();


	// Initialize the first single-folder compressor.

	// ### mw Casting the current folder to MSG_FolderInfoMail *
	//        is safe because we checked for the folder type
	//        above. 
	if (m_currentFolder)
		m_currentFolderCompressor = 
			new MSG_CompressFolderState(master, context, url, 
										(MSG_FolderInfoMail *)m_currentFolder,
										m_bytesCompressed, 
										m_bytesTotal);
}

void
MSG_CompressAllFoldersState::NextMailFolder(void)
{
	// Get the next folder to be compressed.
	XP_Bool haveMailFile = FALSE;
	do
	{
		m_currentFolder = m_iter->Next();
#ifdef DEBUG_bienvenu
		if (m_currentFolder)
			XP_Trace("in compress all folders iterator - %s\n", m_currentFolder->GetName());
#endif
		
		// Make sure the current folder is appropriate for compression.
		if (m_currentFolder && (m_currentFolder->GetType() == FOLDER_MAIL))
		{
			// See if the file represented by this folder is a directory.
			// If it isn't a directory, we have an eligible mail file.
			XP_StatStruct fileInfo;
			
			if (XP_Stat(((MSG_FolderInfoMail *) m_currentFolder)->
						GetPathname(), &fileInfo, xpMailFolder))
				continue; // not a mail folder or had problems statting
#if defined (XP_MAC) || defined(XP_UNIX) || defined(XP_OS2)
			if (fileInfo.st_mode & S_IFREG)
#else
			if (fileInfo.st_mode & _S_IFREG)
#endif
//			if (S_ISREG(fileInfo.st_mode))
				haveMailFile = TRUE; // regular file == probable mail folder
		}
	}
	while ((haveMailFile == FALSE) && (m_currentFolder));
}

int
MSG_CompressAllFoldersState::BeginCompression(void)
{
	// Begin compression on the current (first) folder compressor.
	return m_currentFolderCompressor->BeginCompression();
}

int
MSG_CompressAllFoldersState::CompressSomeMore(void)
{
	int result = MK_CONNECTED;

	XP_ASSERT(m_currentFolderCompressor != NULL);
	XP_ASSERT(m_iter != NULL);

	if (m_currentFolderCompressor)
	{
		// Call the single-folder compressor for the current folder.
		result = m_currentFolderCompressor->CompressSomeMore();

		// If we finished with the current folder, or if we managed
		// to compress a new folder in one pass, start on a new one.
		while ((result == MK_CONNECTED) && (m_currentFolderCompressor != NULL))
		{
			// Done with this folder. Time to pick up the next one.

			// Delete the pre-existing folder compressor.
			delete m_currentFolderCompressor;
			m_currentFolderCompressor = NULL;

			// Update the number of bytes already compressed
			// so that we can properly show progress. (not yet)
			//m_bytesCompressed += m_currentFolder->m_folderSize -
			//	m_currentFolder->m_expunged_bytes;

			NextMailFolder();

			if (m_currentFolder)
			{
				// Got a folder. Create a compressor for it.
				// ### mw Casting the current folder to MSG_FolderInfoMail
				//        is safe because we checked for the folder type
				//        above. 
				m_currentFolderCompressor = 
					new MSG_CompressFolderState(m_master, 
												m_compressContext, m_url, 
												((MSG_FolderInfoMail *) 
												 m_currentFolder),
												m_bytesCompressed, 
												m_bytesTotal);
				XP_ASSERT(m_currentFolderCompressor != NULL);
				
				if (m_currentFolderCompressor)
				{
					// Begin compression on the new folder.
					// If it completes in one pass, the loop will
					// pick up the next folder in line.
					result = m_currentFolderCompressor->BeginCompression();
					// result = m_currentFolderCompressor->CompressSomeMore();
				}
			}
		}
	}

	return result;
}

int
MSG_CompressAllFoldersState::FinishCompression(void)
{
	// *** jefft -- m_currentFolderCompressor can be null;
	// check before calling FinishCompression
	if (m_currentFolderCompressor)
		return m_currentFolderCompressor->FinishCompression();
	return 0;
}

/*
----------------------------------------------------
class MSG_CompressState
----------------------------------------------------
*/

MSG_CompressState::MSG_CompressState(MSG_Master *master,
									 MWContext *context, 
									 URL_Struct *url)
	: m_master(master), m_compressContext(context), m_url(url)
{
	XP_ASSERT(m_master != NULL);
	XP_ASSERT(m_compressContext != NULL);
}

MSG_CompressState *
MSG_CompressState::Create(MSG_Master *master,
						  MWContext *context, 
						  URL_Struct *url,
						  const char *mailboxName)
{
	if ((mailboxName == NULL) || (mailboxName[0] == '\0'))
	{
		return new MSG_CompressAllFoldersState(master, context, url);
	}
	else
	{
		MSG_CompressFolderState *compressFolderState 
			= new MSG_CompressFolderState(master, context, url, 
										   master->FindMailFolder(mailboxName, FALSE),
										   0 /* extra progress done */, 
										   0 /* actual progress total */);
		// by default, compress folder state objects ignore non-mail folders,
		// but if we're doing a compress of a single folder, we should whine.
		if (compressFolderState)
			compressFolderState->SetIgnoreNonMailFolder(FALSE);
		return compressFolderState;
	}
}
