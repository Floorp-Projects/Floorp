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

#include "msgCore.h"
#include "nsMsgSearchCore.h"
#include "nsMsgUtils.h"
#include "nsMsgBodyHandler.h"
#include "nsMsgSearchTerm.h"
#include "nsFileStream.h"
#include "nsIFileStream.h"
#include "nsIFileSpec.h"

nsMsgBodyHandler::nsMsgBodyHandler (nsIMsgSearchScopeTerm * scope, PRUint32 offset, PRUint32 numLines, nsIMsgDBHdr* msg, nsIMsgDatabase * db)
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
  
  Initialize();  // common initialization stuff
  OpenLocalFolder();	    
}

nsMsgBodyHandler::nsMsgBodyHandler(nsIMsgSearchScopeTerm * scope,
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
  
  Initialize();
  
  if (m_Filtering)
    m_headers = headers;
  else
    OpenLocalFolder();  // if nothing else applies, then we must be a POP folder file
}

void nsMsgBodyHandler::Initialize()
// common initialization code regardless of what body type we are handling...
{
  // Default transformations for local message search and MAPI access
  m_stripHeaders = PR_TRUE;
  m_stripHtml = PR_TRUE;
  m_messageIsHtml = PR_FALSE;
  m_passedHeaders = PR_FALSE;
  m_headerBytesRead = 0;
  
}

nsMsgBodyHandler::~nsMsgBodyHandler()
{
  if (m_fileSpec)
  {
    PRBool isOpen;
    m_fileSpec->IsStreamOpen(&isOpen);
    if (isOpen) 
      m_fileSpec->CloseStream();
  }
}

		

PRInt32 nsMsgBodyHandler::GetNextLine (char * buf, int bufSize)
{
  PRInt32 length = 0;
  PRBool eatThisLine = PR_FALSE;
  
  do {
    // first, handle the filtering case...this is easy....
    if (m_Filtering)
      length = GetNextFilterLine(buf, bufSize);
    else
    {
      // 3 cases: Offline IMAP, POP, or we are dealing with a news message....
      // Offline cases should be same as local mail cases, since we're going
      // to store offline messages in berkeley format folders.
      if (m_db)
      {
         length = GetNextLocalLine (buf, bufSize); // (2) POP
      }
    }
    
    if (length >= 0)
      length = ApplyTransformations (buf, length, eatThisLine);
  } while (eatThisLine && length >= 0);  // if we hit eof, make sure we break out of this loop. Bug #:
  return length;  
}
void nsMsgBodyHandler::OpenLocalFolder()
{
  nsresult rv = m_scope->GetMailPath(getter_AddRefs(m_fileSpec));
  PRBool isOpen = PR_FALSE;
  if (NS_SUCCEEDED(rv) && m_fileSpec)
  {
    m_fileSpec->IsStreamOpen(&isOpen);
    if (!isOpen) 
      m_fileSpec->OpenStreamForReading();
    m_fileSpec->Seek(m_localFileOffset);
  }
  
}


PRInt32 nsMsgBodyHandler::GetNextFilterLine(char * buf, PRUint32 bufSize)
{
  // m_nextHdr always points to the next header in the list....the list is NULL terminated...
  PRUint32 numBytesCopied = 0;
  if (m_headersSize > 0)
  {
    // #mscott. Ugly hack! filter headers list have CRs & LFs inside the NULL delimited list of header
    // strings. It is possible to have: To NULL CR LF From. We want to skip over these CR/LFs if they start
    // at the beginning of what we think is another header.
    
    while ((m_headers[0] == nsCRT::CR || m_headers[0] == nsCRT::LF || m_headers[0] == ' ' || m_headers[0] == '\0') && m_headersSize > 0)
    {
      m_headers++;  // skip over these chars...
      m_headersSize--;
    }
    
    if (m_headersSize > 0)
    {
      numBytesCopied = nsCRT::strlen(m_headers)+1 /* + 1 to include NULL */ < bufSize ? nsCRT::strlen(m_headers)+1 : (PRInt32) bufSize;
      nsCRT::memcpy(buf, m_headers, numBytesCopied);
      m_headers += numBytesCopied;  
      // be careful...m_headersSize is unsigned. Don't let it go negative or we overflow to 2^32....*yikes*	
      if (m_headersSize < numBytesCopied)
        m_headersSize = 0;
      else
        m_headersSize -= numBytesCopied;  // update # bytes we have read from the headers list
      
      return (PRInt32) numBytesCopied;
    }
  }
  else if (m_headersSize == 0) {
    buf[0] = '\0';
  }
  return -1;
}

// return -1 if no more local lines, length of next line otherwise.

PRInt32 nsMsgBodyHandler::GetNextLocalLine(char * buf, int bufSize)
// returns number of bytes copied
{
  if (m_numLocalLines)
  {
    if (m_passedHeaders)
      m_numLocalLines--; // the line count is only for body lines
    // do we need to check the return value here?
    if (m_fileSpec)
    {
      PRBool isEof = PR_FALSE;
      nsresult rv = m_fileSpec->Eof(&isEof);
      if (NS_FAILED(rv) || isEof)
        return -1;
      
      PRBool wasTruncated = PR_FALSE;
      rv = m_fileSpec->ReadLine(&buf, bufSize, &wasTruncated);
      if (NS_SUCCEEDED(rv) && !wasTruncated)
        return nsCRT::strlen(buf);
    }
  }
  
  return -1;
}

PRInt32 nsMsgBodyHandler::ApplyTransformations (char *buf, PRInt32 length, PRBool &eatThisLine)
{
  PRInt32 newLength = length;
  eatThisLine = PR_FALSE;
  
  if (!m_passedHeaders)	// buf is a line from the message headers
  {
    if (m_stripHeaders)
      eatThisLine = PR_TRUE;
    
    if (!nsCRT::strncasecmp(buf, "Content-Type:", 13) && PL_strcasestr (buf, "text/html"))
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
    PR_Free (pBuf);
  }
}


