/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "msgCore.h"
#include "prlog.h"
#include "prmem.h"
#include "nsMsgLineBuffer.h"

#include "nsIInputStream.h" // used by nsMsgLineStreamBuffer

MOZ_DECL_CTOR_COUNTER(nsByteArray)

nsByteArray::nsByteArray()
{
  MOZ_COUNT_CTOR(nsByteArray);
  m_buffer = NULL;
  m_bufferSize = 0;
  m_bufferPos = 0;
}

nsByteArray::~nsByteArray()
{
  MOZ_COUNT_DTOR(nsByteArray);
  PR_FREEIF(m_buffer);
}

nsresult nsByteArray::GrowBuffer(PRUint32 desired_size, PRUint32 quantum)
{
  if (m_bufferSize < desired_size)
  {
    char *new_buf;
    PRUint32 increment = desired_size - m_bufferSize;
    if (increment < quantum) /* always grow by a minimum of N bytes */
      increment = quantum;
    
    
    new_buf = (m_buffer
      ? (char *) PR_REALLOC (m_buffer, (m_bufferSize + increment))
      : (char *) PR_MALLOC (m_bufferSize + increment));
    if (! new_buf)
      return NS_ERROR_OUT_OF_MEMORY;
    m_buffer = new_buf;
    m_bufferSize += increment;
  }
  return 0;
}

nsresult nsByteArray::AppendString(const char *string)
{
  PRUint32 strLength = (string) ? PL_strlen(string) : 0;
  return AppendBuffer(string, strLength);
  
}

nsresult nsByteArray::AppendBuffer(const char *buffer, PRUint32 length)
{
  nsresult ret = NS_OK;
  if (m_bufferPos + length > m_bufferSize)
    ret = GrowBuffer(m_bufferPos + length, 1024);
  if (ret == NS_OK)
  {
    memcpy(m_buffer + m_bufferPos, buffer, length);
    m_bufferPos += length;
  }
  return ret;
}

MOZ_DECL_CTOR_COUNTER(nsMsgLineBuffer)

nsMsgLineBuffer::nsMsgLineBuffer(nsMsgLineBufferHandler *handler, PRBool convertNewlinesP)
{
  MOZ_COUNT_CTOR(nsMsgLineBuffer);
  m_handler = handler;
  m_convertNewlinesP = convertNewlinesP;
  m_lookingForCRLF = PR_TRUE;
  m_ignoreCRLFs = PR_FALSE;
}

nsMsgLineBuffer::~nsMsgLineBuffer()
{
  MOZ_COUNT_DTOR(nsMsgLineBuffer);
}

void
nsMsgLineBuffer::SetLookingForCRLF(PRBool b)
{
  m_lookingForCRLF = b;
}

PRInt32	nsMsgLineBuffer::BufferInput(const char *net_buffer, PRInt32 net_buffer_size)
{
    int status = 0;
    if (m_bufferPos > 0 && m_buffer && m_buffer[m_bufferPos - 1] == nsCRT::CR &&
        net_buffer_size > 0 && net_buffer[0] != nsCRT::LF) {
        /* The last buffer ended with a CR.  The new buffer does not start
           with a LF.  This old buffer should be shipped out and discarded. */
        PR_ASSERT(m_bufferSize > m_bufferPos);
        if (m_bufferSize <= m_bufferPos) return -1;
        status = ConvertAndSendBuffer();
        if (status < 0) 
           return status;
        m_bufferPos = 0;
    }
    while (net_buffer_size > 0)
    {
        const char *net_buffer_end = net_buffer + net_buffer_size;
        const char *newline = 0;
        const char *s;
        
        if (!m_ignoreCRLFs)
        {
          for (s = net_buffer; s < net_buffer_end; s++)
          {
            if (m_lookingForCRLF) {
              /* Move forward in the buffer until the first newline.
                 Stop when we see CRLF, CR, or LF, or the end of the buffer.
                 *But*, if we see a lone CR at the *very end* of the buffer,
                 treat this as if we had reached the end of the buffer without
                 seeing a line terminator.  This is to catch the case of the
                 buffers splitting a CRLF pair, as in "FOO\r\nBAR\r" "\nBAZ\r\n".
              */
              if (*s == nsCRT::CR || *s == nsCRT::LF) {
                newline = s;
                if (newline[0] == nsCRT::CR) {
                  if (s == net_buffer_end - 1) {
                    /* CR at end - wait for the next character. */
                    newline = 0;
                    break;
                  }
                  else if (newline[1] == nsCRT::LF) {
                    /* CRLF seen; swallow both. */
                    newline++;
                  }
                }
                newline++;
                break;
              }
            }
            else {
              /* if not looking for a CRLF, stop at CR or LF.  (for example, when parsing the newsrc file).  this fixes #9896, where we'd lose the last line of anything we'd parse that used CR as the line break. */
              if (*s == nsCRT::CR || *s == nsCRT::LF) {
                newline = s;
                newline++;
                break;
              }
            }
          }
        }
        
        /* Ensure room in the net_buffer and append some or all of the current
           chunk of data to it. */
        {
            const char *end = (newline ? newline : net_buffer_end);
            PRUint32 desired_size = (end - net_buffer) + m_bufferPos + 1;
            
            if (desired_size >= m_bufferSize)
            {
                status = GrowBuffer (desired_size, 1024);
                if (status < 0) 
                    return status;
            }
            memcpy (m_buffer + m_bufferPos, net_buffer, (end - net_buffer));
            m_bufferPos += (end - net_buffer);
        }
        
        /* Now m_buffer contains either a complete line, or as complete
           a line as we have read so far.
           
           If we have a line, process it, and then remove it from `m_buffer'.
           Then go around the loop again, until we drain the incoming data.
           */
        if (!newline)
            return 0;
        
        status = ConvertAndSendBuffer();
        if (status < 0) return status;
        
        net_buffer_size -= (newline - net_buffer);
        net_buffer = newline;
        m_bufferPos = 0;
    }
    return 0;
}

PRInt32 nsMsgLineBuffer::HandleLine(char *line, PRUint32 line_length)
{
	NS_ASSERTION(PR_FALSE, "must override this method if you don't provide a handler");
	return 0;
}

PRInt32 nsMsgLineBuffer::ConvertAndSendBuffer()
{
    /* Convert the line terminator to the native form.
     */

    char *buf = m_buffer;
    PRInt32 length = m_bufferPos;

    char* newline;
    
    PR_ASSERT(buf && length > 0);
    if (!buf || length <= 0) 
        return -1;
    newline = buf + length;
    
    PR_ASSERT(newline[-1] == nsCRT::CR || newline[-1] == nsCRT::LF);
    if (newline[-1] != nsCRT::CR && newline[-1] != nsCRT::LF)
        return -1;
    
    if (m_convertNewlinesP)
    {
#if (MSG_LINEBREAK_LEN == 1)
      if ((newline - buf) >= 2 &&
           newline[-2] == nsCRT::CR &&
           newline[-1] == nsCRT::LF)
      {
        /* CRLF -> CR or LF */
        buf [length - 2] = MSG_LINEBREAK[0];
        length--;
      }
      else if (newline > buf + 1 &&
             newline[-1] != MSG_LINEBREAK[0])
      {
        /* CR -> LF or LF -> CR */
        buf [length - 1] = MSG_LINEBREAK[0];
      }
#else
      if (((newline - buf) >= 2 && newline[-2] != nsCRT::CR) ||
               ((newline - buf) >= 1 && newline[-1] != nsCRT::LF))
      {
        /* LF -> CRLF or CR -> CRLF */
        length++;
        buf[length - 2] = MSG_LINEBREAK[0];
        buf[length - 1] = MSG_LINEBREAK[1];
      }
#endif
    }    
    return (m_handler) ? m_handler->HandleLine(buf, length) : HandleLine(buf, length);
}

// If there's still some data (non CRLF terminated) flush it out
PRInt32 nsMsgLineBuffer::FlushLastLine()
{
	char *buf = m_buffer + m_bufferPos;
	PRInt32 length = m_bufferPos - 1;
	if (length > 0)
		return (m_handler) ? m_handler->HandleLine(buf, length) : HandleLine(buf, length);
	else
		return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// This is a utility class used to efficiently extract lines from an input stream by buffering
// read but unprocessed stream data in a buffer. 
///////////////////////////////////////////////////////////////////////////////////////////////////

nsMsgLineStreamBuffer::nsMsgLineStreamBuffer(PRUint32 aBufferSize, PRBool aAllocateNewLines, PRBool aEatCRLFs, char aLineToken) 
	         : m_eatCRLFs(aEatCRLFs), m_allocateNewLines(aAllocateNewLines), m_lineToken(aLineToken)
{
	NS_PRECONDITION(aBufferSize > 0, "invalid buffer size!!!");
	m_dataBuffer = nsnull;
	m_startPos = 0;
    m_numBytesInBuffer = 0;

	// used to buffer incoming data by ReadNextLineFromInput
	if (aBufferSize > 0)
	{
		m_dataBuffer = (char *) PR_CALLOC(sizeof(char) * aBufferSize);
	}

	m_dataBufferSize = aBufferSize;
}

nsMsgLineStreamBuffer::~nsMsgLineStreamBuffer()
{
  PR_FREEIF(m_dataBuffer); // release our buffer...
}


nsresult nsMsgLineStreamBuffer::GrowBuffer(PRInt32 desiredSize)
{
  m_dataBuffer = (char *) PR_REALLOC(m_dataBuffer, desiredSize);
  if (!m_dataBuffer)
    return NS_ERROR_OUT_OF_MEMORY;
  m_dataBufferSize = desiredSize;
  return NS_OK;
}

void nsMsgLineStreamBuffer::ClearBuffer()
{
  m_startPos = 0;
  m_numBytesInBuffer = 0;
}

// aInputStream - the input stream we want to read a line from
// aPauseForMoreData is returned as PR_TRUE if the stream does not yet contain a line and we must wait for more
// data to come into the stream.
// Note to people wishing to modify this function: Be *VERY CAREFUL* this is a critical function used by all of
// our mail protocols including imap, nntp, and pop. If you screw it up, you could break a lot of stuff.....

char * nsMsgLineStreamBuffer::ReadNextLine(nsIInputStream * aInputStream, PRUint32 &aNumBytesInLine, PRBool &aPauseForMoreData, nsresult *prv)
{
  // try to extract a line from m_inputBuffer. If we don't have an entire line, 
  // then read more bytes out from the stream. If the stream is empty then wait
  // on the monitor for more data to come in.
  
  NS_PRECONDITION(m_dataBuffer && m_dataBufferSize > 0, "invalid input arguments for read next line from input");

  if (prv)
    *prv = NS_OK;
  // initialize out values
  aPauseForMoreData = PR_FALSE;
  aNumBytesInLine = 0;
  char * endOfLine = nsnull;
  char * startOfLine = m_dataBuffer+m_startPos;
  
  if (m_numBytesInBuffer > 0) // any data in our internal buffer?
    endOfLine = PL_strchr(startOfLine, m_lineToken); // see if we already have a line ending...
  
  // it's possible that we got here before the first time we receive data from the server
  // so aInputStream will be nsnull...
  if (!endOfLine && aInputStream) // get some more data from the server
  {
    nsresult rv;
    PRUint32 numBytesInStream = 0;
    PRUint32 numBytesCopied = 0;
    PRBool nonBlockingStream;
    aInputStream->IsNonBlocking(&nonBlockingStream);
    rv = aInputStream->Available(&numBytesInStream);
    if (NS_FAILED(rv))
    {
      if (prv)
        *prv = rv;
      return nsnull;
    }
    if (!nonBlockingStream && numBytesInStream == 0) // if no data available,
      numBytesInStream = m_dataBufferSize / 2; // ask for half the data buffer size.

    // if the number of bytes we want to read from the stream, is greater than the number
    // of bytes left in our buffer, then we need to shift the start pos and its contents
    // down to the beginning of m_dataBuffer...
    PRUint32 numFreeBytesInBuffer = m_dataBufferSize - m_startPos - m_numBytesInBuffer;
    if (numBytesInStream >= numFreeBytesInBuffer)
    {
      if (m_startPos)
      {
        memmove(m_dataBuffer, startOfLine, m_numBytesInBuffer);
        // make sure the end of the buffer is terminated
        m_dataBuffer[m_numBytesInBuffer] = '\0';
        m_startPos = 0;
        startOfLine = m_dataBuffer;
        numFreeBytesInBuffer = m_dataBufferSize - m_numBytesInBuffer;
        //printf("moving data in read line around because buffer filling up\n");
      }
      else
      {
        PRInt32 growBy = (numBytesInStream - numFreeBytesInBuffer) * 2 + 1;
        // try growing buffer by twice as much as we need.
        nsresult rv = GrowBuffer(m_dataBufferSize + growBy);
        // if we can't grow the buffer, we have to bail.
        if (NS_FAILED(rv))
          return nsnull;
        startOfLine = m_dataBuffer;
        numFreeBytesInBuffer += growBy;
      }
      NS_ASSERTION(m_startPos == 0, "m_startPos should be 0 .....\n");
    }
    
    PRUint32 numBytesToCopy = PR_MIN(numFreeBytesInBuffer - 1 /* leave one for a null terminator */, numBytesInStream);
    if (numBytesToCopy > 0)
    {
      // read the data into the end of our data buffer
      rv = aInputStream->Read(startOfLine + m_numBytesInBuffer, numBytesToCopy,
                              &numBytesCopied);
      if (prv)
        *prv = rv;
      PRUint32 i;
      PRUint32 endBufPos = m_numBytesInBuffer + numBytesCopied;
      for (i = m_numBytesInBuffer; i < endBufPos; i++)  // replace nulls with spaces
      {
        if (!startOfLine[i])
          startOfLine[i] = ' ';
      }
      m_numBytesInBuffer += numBytesCopied;
      m_dataBuffer[m_startPos + m_numBytesInBuffer] = '\0';

      // okay, now that we've tried to read in more data from the stream,
      // look for another end of line character 
      endOfLine = PL_strchr(startOfLine, m_lineToken);
    }
  }
  
  // okay, now check again for endOfLine.
  if (endOfLine)
  {
    if (!m_eatCRLFs)
      endOfLine += 1; // count for LF or CR
    
    aNumBytesInLine = endOfLine - startOfLine;
    
    if (m_eatCRLFs && aNumBytesInLine > 0 && startOfLine[aNumBytesInLine-1] == '\r') // Remove the CR in a CRLF sequence
      aNumBytesInLine--;
    
    // PR_CALLOC zeros out the allocated line
    char* newLine = (char*) PR_CALLOC(aNumBytesInLine+1);
    if (!newLine)
    {
      aNumBytesInLine = 0;
      aPauseForMoreData = PR_TRUE;
      return nsnull;
    }
    
    memcpy(newLine, startOfLine, aNumBytesInLine); // copy the string into the new line buffer
    
    if (m_eatCRLFs)
      endOfLine += 1; // advance past LF or CR if we haven't already done so...
    
    // now we need to update the data buffer to go past the line we just read out. 
    m_numBytesInBuffer -= (endOfLine - startOfLine);
    if (m_numBytesInBuffer)
      m_startPos = endOfLine - m_dataBuffer;
    else
      m_startPos = 0;
    
    return newLine;
  }
  
  aPauseForMoreData = PR_TRUE;
  return nsnull; // if we somehow got here. we don't have another line in the buffer yet...need to wait for more data...
}

PRBool nsMsgLineStreamBuffer::NextLineAvailable()
{
  return (m_numBytesInBuffer > 0 && PL_strchr(m_dataBuffer+m_startPos, m_lineToken));
}

