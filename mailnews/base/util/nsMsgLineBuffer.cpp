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
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "msgCore.h"
#include "nsMsgLineBuffer.h"

nsByteArray::nsByteArray()
{
	m_buffer = NULL;
	m_bufferSize = 0;
	m_bufferPos = 0;
}

nsByteArray::~nsByteArray()
{
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

nsMsgLineBuffer::nsMsgLineBuffer(nsMsgLineBufferHandler *handler, PRBool convertNewlinesP)
{
	m_handler = handler;
	m_convertNewlinesP = convertNewlinesP;
}

nsMsgLineBuffer::~nsMsgLineBuffer()
{
}

PRInt32	nsMsgLineBuffer::BufferInput(const char *net_buffer, PRInt32 net_buffer_size)
{
    int status = 0;
    if (m_bufferPos > 0 && m_buffer && m_buffer[m_bufferPos - 1] == CR &&
        net_buffer_size > 0 && net_buffer[0] != LF) {
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
        
        
        for (s = net_buffer; s < net_buffer_end; s++)
		{
            /* Move forward in the buffer until the first newline.
               Stop when we see CRLF, CR, or LF, or the end of the buffer.
               *But*, if we see a lone CR at the *very end* of the buffer,
               treat this as if we had reached the end of the buffer without
               seeing a line terminator.  This is to catch the case of the
               buffers splitting a CRLF pair, as in "FOO\r\nBAR\r" "\nBAZ\r\n".
		   */
            if (*s == CR || *s == LF)
			{
                newline = s;
                if (newline[0] == CR)
				{
                    if (s == net_buffer_end - 1)
					{
                        /* CR at end - wait for the next character. */
                        newline = 0;
                        break;
					}
                    else if (newline[1] == LF)
                        /* CRLF seen; swallow both. */
                        newline++;
				}
                newline++;
				break;
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
#ifdef DEBUG_bienvenu
	printf("returning from buffer input m_bufferPos = %ld\n", m_bufferPos);
#endif	
    return 0;
}

PRInt32 nsMsgLineBuffer::HandleLine(char *line, PRUint32 line_length)
{
	NS_ASSERTION(FALSE, "must override this method if you don't provide a handler");
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
    
    PR_ASSERT(newline[-1] == CR || newline[-1] == LF);
    if (newline[-1] != CR && newline[-1] != LF)
		return -1;
    
    if (!m_convertNewlinesP)
	{
	}
#if (LINEBREAK_LEN == 1)
    else if ((newline - buf) >= 2 &&
             newline[-2] == CR &&
             newline[-1] == LF)
	{
        /* CRLF -> CR or LF */
        buf [length - 2] = LINEBREAK[0];
        length--;
	}
    else if (newline > buf + 1 &&
             newline[-1] != LINEBREAK[0])
	{
        /* CR -> LF or LF -> CR */
        buf [length - 1] = LINEBREAK[0];
	}
#else
    else if (((newline - buf) >= 2 && newline[-2] != CR) ||
             ((newline - buf) >= 1 && newline[-1] != LF))
	{
        /* LF -> CRLF or CR -> CRLF */
        length++;
        buf[length - 2] = LINEBREAK[0];
        buf[length - 1] = LINEBREAK[1];
	}
#endif
    
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


