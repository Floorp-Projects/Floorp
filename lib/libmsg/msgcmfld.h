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
#ifndef MSG_COMPRESS_FOLDER_H
#define MSG_COMPRESS_FOLDER_H

#include "prsembst.h"

class MSG_FolderInfoMail;
class DBFolderInfo;
struct ListContext;
class MSG_FolderIterator;

enum CompressFolderState
{
	CFStart,
	CFParsingSource,
	CFDBOpened,
	CFCopyingMsgToTemp,
	CFGetNextMessage,
	CFAbortCompress,
	CFFinishedCopyingToTemp,
	CFCreateEmptyFolder
};

/*
  MSG_CompressState: An abstract class for objects which compress one or more
  mailbox files.
 */
class MSG_CompressState
{
public:
	static MSG_CompressState * 
					Create(MSG_Master *master,
						   MWContext *context,
						   URL_Struct *url,
						   const char *mailboxName);

					MSG_CompressState(MSG_Master *master,
									  MWContext *context,
									  URL_Struct *url);

	/*
	  The message pane calls these three methods in order to achieve
	  compression.
	  */
	virtual	int     BeginCompression(void) = 0;
	virtual	int	    CompressSomeMore(void) = 0;
	virtual int     FinishCompression(void) = 0;

	MSG_Master      *m_master;
	MWContext       *m_compressContext;
    URL_Struct      *m_url;
};

/*
  MSG_CompressFolderState: A class which compresses one mailbox file.
 */
class MSG_CompressFolderState : public MSG_CompressState, 
								public ParseMailboxState
{
public:
				    MSG_CompressFolderState(MSG_Master *master,
											MWContext *context, 
											URL_Struct *url,
											MSG_FolderInfoMail *mailFolderInfo,
											int32 progressBytesDoneOffset,
											int32 progressBytesTotal);
	virtual		    ~MSG_CompressFolderState();

	virtual	int	    BeginCompression(void);
	virtual	int	    CompressSomeMore(void);
	virtual int     FinishCompression(void);
protected:
	int				OpenFolderAndDB();
	int				OpenFolder();
	int				StartFirstMessage();
	int				CopyNextBlock();
	int				StartOutput();
	int				AdvanceToNextNonExpungedMessage();
	int				CloseAndRenameFiles();
	int				CloseFiles(XP_Bool success);
	int				RenameFiles();

	//virtual void UpdateProgressPercent();

	CompressFolderState		m_compressState;
	MSG_FolderInfoMail		*m_folder;			/* Folder being compressed. */
	DBFolderInfo			*m_folderInfo;
	ListContext				*m_listContext;
	MailMessageHdr			*m_pHeader;
	MailMessageHdr			*m_newMailHdr;
	int32					m_progressBytesDoneOffset;
	int32					m_progressBytesTotalOffset;
	const char *            m_folderName;

	XP_File m_infile;				/* The original folder we're parsing. */
	XP_File m_outfile;				/* The new compressed folder. */

	MailDB					*m_newMailDB;	// where the new headers are going
	MailDB					*m_srcMailDB;	// where the headers are coming from
	int32					m_bytesLeftInMessage;
	int32					m_bytesInMessage;	// total bytes in message.
	XP_FileType tmptype;
	char *m_tmpName;				// Temporary filename of new folder. 
	char *m_tmpdbName;				// Temporary filename of new database
	XP_StatStruct m_folderst;
	int32 m_outPosition;
	const char *            m_mailboxName; // Name of folder we're compressing
};

/*
  MSG_CompressAllFoldersState: A class which compresses all known mailbox 
  files.
 */
class MSG_CompressAllFoldersState : public MSG_CompressState
{
public:
					MSG_CompressAllFoldersState(MSG_Master *master,
												MWContext *context, 
												URL_Struct *url);

	/*
	  The message pane calls these three methods in order to achieve
	  compression.
	  */
	virtual	int     BeginCompression(void);
	virtual	int	    CompressSomeMore(void);
	virtual int     FinishCompression(void);
	
	void            NextMailFolder(void);

	MSG_FolderInfo          *m_currentFolder;
	MSG_CompressFolderState *m_currentFolderCompressor;
	MSG_FolderInfo          *m_folders;
	MSG_FolderIterator      *m_iter;
	int32                   m_bytesCompressed;
	int32                   m_bytesTotal;
};

#endif
