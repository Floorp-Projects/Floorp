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

#include "msgCore.h"
#include "nsMsgSearchCore.h"
#include "nsMsgUtils.h"
#include "nsMsgBodyHandler.h"

nsMsgBodyHandler::nsMsgBodyHandler (nsMsgSearchScopeTerm * scope, PRUint32 offset, PRUint32 numLines, nsIMsgDBHdr* msg, nsIMsgDatabase * db)
{
	m_scope = scope;
	m_localFileOffset = offset;
	m_numLocalLines = numLines;
	m_msgHdr = msg;
	m_db = db;

	// the following are variables used when the body handler is handling stuff from filters....through this constructor, that is not the
	// case so we set them to NULL.
	m_headers = NULL;
	m_headersSize = 0;
	m_Filtering = PR_FALSE; // make sure we set this before we call initialize...

#ifdef DO_BODY
	Initialize();  // common initialization stuff
	if (m_scope->IsOfflineIMAPMail() || m_scope->m_folder->GetType() == FOLDER_IMAPMAIL) // if we are here with an IMAP folder, assume offline! 
		m_OfflineIMAP = PR_TRUE;
	else
   	   OpenLocalFolder();	    // POP so open the mail folder file
#endif
}

nsMsgBodyHandler::nsMsgBodyHandler(nsMsgSearchScopeTerm * scope,
                                   PRUint32 offset, PRUint32 numLines,
                                   nsIMsgDBHdr* msg, nsIMsgDatabase* db,
                                   const char * headers, PRUint32 headersSize,
                                   PRBool Filtering)
{
	m_scope = scope;
	m_localFileOffset = offset;
	m_numLocalLines = numLines;
	m_msgHdr = msg;
	m_db = db;
	m_headersSize = headersSize;
	m_Filtering = Filtering;

#ifdef DO_BODY
	Initialize();

	if (m_Filtering)
		m_headers = headers;
	else
		if (m_scope->IsOfflineIMAPMail() || m_scope->m_folder->GetType() == FOLDER_IMAPMAIL)
			m_OfflineIMAP = PR_TRUE;
		else
			OpenLocalFolder();  // if nothing else applies, then we must be a POP folder file
#endif
}

void nsMsgBodyHandler::Initialize()
// common initialization code regardless of what body type we are handling...
{
	// Default transformations for local message search and MAPI access
	m_stripHeaders = PR_TRUE;
	m_stripHtml = PR_TRUE;
	m_messageIsHtml = PR_FALSE;
	m_passedHeaders = PR_FALSE;

	// set our offsets to 0 since we haven't handled any bytes yet...
	m_IMAPMessageOffset = 0;
	m_NewsArticleOffset = 0;
	m_headerBytesRead = 0;

	m_OfflineIMAP = PR_FALSE;
}

nsMsgBodyHandler::~nsMsgBodyHandler()
{
#ifdef DO_BODY
	if (m_scope->m_file)
	{
		XP_FileClose (m_scope->m_file);
		m_scope->m_file = NULL;
	}
#endif
}

		

PRInt32 nsMsgBodyHandler::GetNextLine (char * buf, int bufSize)
{
	PRInt32 length = 0;
#ifdef DO_BODY
	PRBool eatThisLine = PR_FALSE;

	do {
		// first, handle the filtering case...this is easy....
		if (m_Filtering)
			length = GetNextFilterLine(buf, bufSize);
		else
		{
			// 3 cases: Offline IMAP, POP, or we are dealing with a news message....
			if (m_db)
			{
				MailDB * mailDB = m_db->GetMailDB();
				if (mailDB)    // a mail data base?
				{
					if (m_OfflineIMAP)
						length = GetNextIMAPLine (buf, bufSize);  // (1) IMAP Offline
					else
						length = GetNextLocalLine (buf, bufSize); // (2) POP
				}
				else
				{
					NewsGroupDB * newsDB = m_db->GetNewsDB();
					if (newsDB)
						length = GetNextNewsLine (newsDB, buf, bufSize);  // (3) News
				}
			}
		}

		if (length > 0)
			length = ApplyTransformations (buf, length, eatThisLine);
	} while (eatThisLine && length);  // if we hit eof, make sure we break out of this loop. Bug #:
#endif // DO_BODY
	return length;  
}
#ifdef DO_BODY
void nsMsgBodyHandler::OpenLocalFolder()
{
	if (!m_scope->m_file)
	{
		const char *path = m_scope->GetMailPath();
		if (path)
			m_scope->m_file = XP_FileOpen (path, xpMailFolder, XP_FILE_READ_BIN);    // open the folder
	}
	if (m_scope->m_file)
		XP_FileSeek (m_scope->m_file, m_localFileOffset, SEEK_SET); 
}


PRInt32 nsMsgBodyHandler::GetNextFilterLine(char * buf, int bufSize)
{
	// m_nextHdr always points to the next header in the list....the list is NULL terminated...
	int numBytesCopied = 0;
	if (m_headersSize > 0)
	{
		// #mscott. Ugly hack! filter headers list have CRs & LFs inside the NULL delimited list of header
		// strings. It is possible to have: To NULL CR LF From. We want to skip over these CR/LFs if they start
		// at the beginning of what we think is another header.
		while ((m_headers[0] == CR || m_headers[0] == LF || m_headers[0] == ' ' || m_headers[0] == '\0') && m_headersSize > 0)
		{
			m_headers++;  // skip over these chars...
			m_headersSize--;
		}

		if (m_headersSize > 0)
		{
			numBytesCopied = nsCRT::strlen(m_headers)+1 /* + 1 to include NULL */ < bufSize ? nsCRT::strlen(m_headers)+1 : bufSize;
			XP_MEMCPY(buf, m_headers, numBytesCopied);
			m_headers += numBytesCopied;  
			// be careful...m_headersSize is unsigned. Don't let it go negative or we overflow to 2^32....*yikes*	
			if (m_headersSize < numBytesCopied)
				m_headersSize = 0;
			else
				m_headersSize -= numBytesCopied;  // update # bytes we have read from the headers list

			return numBytesCopied;
		}
	}
	return 0;
}


PRInt32 nsMsgBodyHandler::GetNextNewsLine (NewsGroupDB * /* newsDB */, char * buf, int bufSize)
{
	// we know we have a safe downcasting on m_msgHdr to a NewsMessageHdr because we checked that
	// m_db is a news data base b4 calling this routine
	PRInt32 msgLength = ((NewsMessageHdr *)m_msgHdr)->GetOfflineMessageLength (m_db->GetDB()) - m_NewsArticleOffset;
	if (buf && msgLength != 0) // make sure the news article exists....
	{
		PRInt32 bytesToCopy = (msgLength < bufSize-2) ? msgLength : bufSize - 2; // this -2 is a small hack
		PRInt32 bytesCopied = ((NewsMessageHdr *)m_msgHdr)->ReadFromArticle (buf, bytesToCopy, m_NewsArticleOffset, m_db->GetDB());
		if (bytesCopied == 0) // reached end of message?
			return bytesCopied;

		// now determine the location of the nearest CR/LF pairing...
		char * tmp = buf;
		while (tmp < buf + bytesCopied && *tmp != 0 && *tmp != CR && *tmp != LF)
			tmp++;

		if (tmp && (*tmp == CR || *tmp == LF) )
		{
			// a line is contained within the buffer. Null terminate 2 positions past the CR/LF pair, update new offset value
			// we know we have at least 2 bytes leftover in the buffer
			if (*tmp == CR && *(tmp+1) == LF) // if it is a CR LF pair then null terminate after the pair
				*(tmp+2) = '\0';
			else		// otherwise, null terminate string after the CR or LF
				*(tmp+1) = '\0'; 
		}
		else
		    buf[bytesCopied] = '\0';
		m_NewsArticleOffset += nsCRT::strlen (buf);
		return nsCRT::strlen (buf);      // return num bytes stored in the buf
	}
	return 0;
}


PRInt32 nsMsgBodyHandler::GetNextLocalLine(char * buf, int bufSize)
// returns number of bytes copied
{
	char * line = NULL;
	if (m_numLocalLines)
	{
		if (m_passedHeaders)
			m_numLocalLines--; // the line count is only for body lines
		line = XP_FileReadLine (buf, bufSize, m_scope->m_file);
		if (line)
			return nsCRT::strlen(line);
	}

	return 0;
}


PRInt32 nsMsgBodyHandler::GetNextIMAPLine(char * buf, int bufSize)
{
	// we know we have safe downcasting on m_msgHdr because we checked that m_db is a mail data base b4 calling
	// this routine.
    PRInt32 msgLength = ((MailMessageHdr *) m_msgHdr)->GetOfflineMessageLength (m_db->GetDB()) - m_IMAPMessageOffset;
	if (buf && msgLength != 0)  // make sure message exists
	{
		PRInt32 bytesToCopy = (msgLength < bufSize-2) ? msgLength : bufSize-2; // the -2 is a small hack

		PRInt32 bytesCopied = ((MailMessageHdr *) m_msgHdr)->ReadFromOfflineMessage (buf, bytesToCopy, m_IMAPMessageOffset, m_db->GetDB());
		if (bytesCopied == 0)  // we reached end of message
			return bytesCopied;

		// now determine the location of the nearest CR/LF pairing...
		char * tmp = buf;
		while (tmp < buf + bytesCopied && *tmp != 0 && *tmp != CR && *tmp != LF)
			tmp++;
		
		if (tmp && (*tmp == CR || *tmp == LF) )
		{
			// a line is contained within the buffer. Null terminate 2 positions past the CR/LF pair, update new offset value
			// we know we have at least 2 bytes leftover in the buffer so it is safe to check tmp and tmp + 1...
			if (*tmp == CR && *(tmp+1) == LF) // if it is a CR LF pair then null terminate after the pair
				*(tmp+2) = '\0';
			else		// otherwise, null terminate string after the CR or LF
				*(tmp+1) = '\0'; 
		}
		else
		    buf[bytesCopied] = '\0';
		m_IMAPMessageOffset += nsCRT::strlen (buf);
		return nsCRT::strlen (buf);      // return num bytes stored in the buf
	}
	return 0;
}


PRInt32 nsMsgBodyHandler::ApplyTransformations (char *buf, PRInt32 length, PRBool &eatThisLine)
{
	PRInt32 newLength = length;
	eatThisLine = PR_FALSE;

	if (!m_passedHeaders)	// buf is a line from the message headers
	{
		if (m_stripHeaders)
			eatThisLine = PR_TRUE;

		if (!XP_STRNCASECMP(buf, "Content-Type:", 13) && strcasestr (buf, "text/html"))
			m_messageIsHtml = PR_TRUE;

		m_passedHeaders = EMPTY_MESSAGE_LINE(buf);
	}
	else	// buf is a line from the message body
	{
		if (m_stripHtml && m_messageIsHtml)
		{
			StripHtml (buf);
			newLength = nsCRT::strlen (buf);
		}
	}

	return newLength;
}


void nsMsgBodyHandler::StripHtml (char *pBufInOut)
{
	char *pBuf = (char*) PR_Malloc (nsCRT::strlen(pBufInOut) + 1);
	if (pBuf)
	{
		char *pWalk = pBuf;
		char *pWalkInOut = pBufInOut;
		PRBool inTag = PR_FALSE;
		while (*pWalkInOut) // throw away everything inside < >
		{
			if (!inTag)
				if (*pWalkInOut == '<')
					inTag = PR_TRUE;
				else
					*pWalk++ = *pWalkInOut;
			else
				if (*pWalkInOut == '>')
					inTag = PR_FALSE;
			pWalkInOut++;
		}
		*pWalk = 0; // null terminator

		// copy the temp buffer back to the real one
		pWalk = pBuf;
		pWalkInOut = pBufInOut;
		while (*pWalk)
			*pWalkInOut++ = *pWalk++;
		*pWalkInOut = *pWalk; // null terminator
		XP_FREE (pBuf);
	}
}

#endif // DO_BODY

