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
#include "nsCapiCallbackReader.h"
#include "nspr.h"

const t_int32 nsCapiCallbackReader::m_MAXBUFFERSIZE = 1024;
const t_int32 nsCapiCallbackReader::m_NOMORECHUNKS = 404;
//---------------------------------------------------------------------
nsCapiCallbackReader::nsCapiCallbackReader()
{
    PR_ASSERT(FALSE);
}
//---------------------------------------------------------------------

void
nsCapiCallbackReader::AddBuffer(nsCapiBufferStruct * cBuf)
{
    if (m_Chunks == 0)
        m_Chunks = new JulianPtrArray();
    PR_ASSERT(m_Chunks != 0);
    if (m_Chunks != 0)
    {
        m_Chunks->Add(cBuf);
    }
}

//---------------------------------------------------------------------

nsCapiCallbackReader::nsCapiCallbackReader(PRMonitor * monitor,
                               JulianUtility::MimeEncoding encoding)
{ 
    m_Monitor = monitor;
    m_bFinished = FALSE;
    
    m_ChunkIndex = 0;
    m_Chunks = 0;
    m_Init = TRUE;
    m_Pos = 0;
    m_Mark = -1;
    m_ChunkMark = -1;
    m_Encoding = encoding;
}

//---------------------------------------------------------------------

void nsCapiCallbackReader::deleteCapiBufferStructVector(JulianPtrArray * bufferVector)
{
    t_int32 i;
    if (bufferVector != 0) 
    {
        nsCapiBufferStruct * cbBuf;
        for (i = bufferVector->GetSize() - 1; i >= 0; i--)
        {
            cbBuf = (nsCapiBufferStruct *) bufferVector->GetAt(i);
            if (0 != cbBuf->m_pBuf)
            {
                delete [] (cbBuf->m_pBuf);
                cbBuf->m_pBuf = 0;
            }
            delete cbBuf; cbBuf = 0;
        }
    }
}

//---------------------------------------------------------------------

nsCapiCallbackReader::~nsCapiCallbackReader()
{
    if (m_Chunks != 0)
    {
        deleteCapiBufferStructVector(m_Chunks);
        delete m_Chunks; m_Chunks = 0;
    }
}

//---------------------------------------------------------------------

t_int8 nsCapiCallbackReader::read(ErrorCode & status)
{
    t_int32 i = 0;
    status = ZERO_ERROR;
    
    while (TRUE)
    {
        if (m_Chunks == 0 || m_Chunks->GetSize() == 0 || 
            m_ChunkIndex >= m_Chunks->GetSize())
        {
            status = m_NOMORECHUNKS; // no more chunks, should block
            return -1;
        }
        else
        {
            // read from linked list of UnicodeString's
            // delete front string when finished reading from it
        
            nsCapiBufferStruct * cbBuf = (nsCapiBufferStruct *) m_Chunks->GetAt(m_ChunkIndex);
            char * buf = cbBuf->m_pBuf;
            char c;
            if ((size_t) m_Pos < cbBuf->m_pBufSize)
            {
                if (JulianUtility::MimeEncoding_QuotedPrintable == m_Encoding)
                {
                    char * buf = cbBuf->m_pBuf;            
                    c = buf[m_Pos];
                    if ('=' == c)
                    {
                        if (cbBuf->m_pBufSize >= (size_t) (m_Pos + 3))
                        {
                            if (ICalReader::isHex(buf[m_Pos + 1]) && ICalReader::isHex(buf[m_Pos + 2]))
                            {
                                c = ICalReader::convertHex(buf[m_Pos + 1], buf[m_Pos + 2]);
                                m_Pos += 3;
                                return c;
                            }
                            else
                            {
                                return (t_int8) buf[m_Pos++];
                            }
                        }
                        else
                        {
                            PRInt32 lenDiff = cbBuf->m_pBufSize - m_Pos;
                            char fToken, sToken;
                            PRBool bSetFToken = FALSE;
                            PRInt32 tempIndex = m_ChunkIndex;
                            UnicodeString token;

                            while (TRUE)
                            {
                                // lenDiff = 1, 2 always
                                // the =XX spans different chunks
                                // if last chunk, return out of chunks status
                                if (tempIndex == m_Chunks->GetSize() - 1)
                                {
                                    status = m_NOMORECHUNKS;
                                    return -1;
                                }
                                else
                                {
                                    nsCapiBufferStruct * cbNextBuf = (nsCapiBufferStruct *) m_Chunks->GetAt(tempIndex + 1);
                                    char * nextBuf = cbNextBuf->m_pBuf;
                                    tempIndex++;
                                    if (cbNextBuf->m_pBufSize >= 2)
                                    {
                                        if (lenDiff == 2)
                                        {
                                            fToken = buf[cbBuf->m_pBufSize - 1];
                                            sToken = nextBuf[0];
                                            if (ICalReader::isHex(fToken) && ICalReader::isHex(sToken))
                                            {
                                                c = ICalReader::convertHex(fToken, sToken);
                                
                                                m_Pos = 1;
                                                m_ChunkIndex = tempIndex;
                                                bSetFToken = FALSE;
                                                return c;
                                            }
                                            else
                                            {
                                                return (t_int8) buf[m_Pos++];
                                            }
                                        }
                                        else
                                        {
                                            // lenDiff = 1
                                            if (bSetFToken)
                                            {
                                                sToken = nextBuf[0];
                                            }
                                            else
                                            {
                                                fToken = nextBuf[0];
                                                sToken = nextBuf[1];
                                            }
                                            if (ICalReader::isHex(fToken) && ICalReader::isHex(sToken))
                                            {
                                                c = ICalReader::convertHex(fToken, sToken);
                                
                                                m_Pos = 2;
                                                m_ChunkIndex = tempIndex;
                                                bSetFToken = FALSE;
                                                return c;
                                            }
                                            else
                                            {
                                                return (t_int8) buf[m_Pos++];
                                            }
                                        }
                                    }
                                    else
                                    {
                                        if (cbNextBuf->m_pBufSize > 0)
                                        {
                                            if (!bSetFToken)
                                            {
                                                fToken = nextBuf[0];
                                                bSetFToken = TRUE;
                                            }
                                            else
                                            {
                                                sToken = nextBuf[0];
                                                if (ICalReader::isHex(fToken) && ICalReader::isHex(sToken))
                                                {
                                                    c = ICalReader::convertHex(fToken, sToken); 

                                                    m_Pos = 1;
                                                    m_ChunkIndex = tempIndex;
                                                    bSetFToken = FALSE;
                                                    return c;
                                                }
                                                else
                                                {
                                                    return (t_int8) buf[m_Pos++];
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                    else
                    {
                        return (t_int8) buf[m_Pos++];
                    }
                }
                else
                {
                    return (t_int8) buf[m_Pos++];
                }
            }
            else
            {
                m_ChunkIndex++;
                m_Pos = 0;
            }
        }
    }
    status = 1;
    return -1;
}

//---------------------------------------------------------------------

UnicodeString &
nsCapiCallbackReader::createLine(t_int32 oldPos, t_int32 oldChunkIndex,
                           t_int32 newPos, t_int32 newChunkIndex,
                           UnicodeString & aLine)
{
    PR_ASSERT(oldChunkIndex <= newChunkIndex);
    UnicodeString u;
    if (oldChunkIndex == newChunkIndex)
    {
        u = *((UnicodeString *) m_Chunks->GetAt(oldChunkIndex));
        u.extractBetween(oldPos, newPos, aLine);
        return aLine;
    }
    else {
        //(oldChunkIndex < newChunkIndex)
        
        t_int32 i;
        UnicodeString v, temp;
        u = *((UnicodeString *) m_Chunks->GetAt(oldChunkIndex));
        u.extractBetween(oldPos, u.size(), aLine);
        v = *((UnicodeString *) m_Chunks->GetAt(newChunkIndex));
        v.extractBetween(0, newPos, temp);
        i = oldChunkIndex + 1;
        while (i < newChunkIndex)
        {
            v = *((UnicodeString *) m_Chunks->GetAt(i));
            aLine += v;
            i++;
        }
        aLine += temp;
        return aLine;
    }
}
//---------------------------------------------------------------------
UnicodeString & 
nsCapiCallbackReader::readFullLine(UnicodeString & aLine, ErrorCode & status, t_int32 iTemp)
{
    status = ZERO_ERROR;
    t_int32 i;

    PR_EnterMonitor(m_Monitor);

    t_int32 oldpos = m_Pos;
    t_int32 oldChunkIndex = m_ChunkIndex;

    readLine(aLine, status);
    if (status == m_NOMORECHUNKS)
    {
      if (m_bFinished)
      {
        // do nothing.
      }
      else 
      {
        PR_Wait(m_Monitor,PR_INTERVAL_NO_TIMEOUT);
      }
    }

    UnicodeString aSubLine;
    while (TRUE)
    {
      mark();
      i = read(status);
      if (status == m_NOMORECHUNKS)
      {
        if (m_bFinished)
        {
          // do nothing
          break;
        }
        else
        {
          PR_Wait(m_Monitor,PR_INTERVAL_NO_TIMEOUT);
        }
      }
      else if (i == ' ')
      {
        aLine += readLine(aSubLine, status);
      }
      else
      {
        reset();
        break;
      }
    }
    
    PR_ExitMonitor(m_Monitor);

    return aLine;
}
//---------------------------------------------------------------------
UnicodeString & 
nsCapiCallbackReader::readLine(UnicodeString & aLine, ErrorCode & status)
{
    aLine = "";

    if (m_Chunks == 0 || m_Chunks->GetSize() == 0 || 
            m_ChunkIndex >= m_Chunks->GetSize())
    {
        status = m_NOMORECHUNKS; // no more chunks, should block
        return aLine;
    }    
    else
    {
        nsCapiBufferStruct * cbBuf = 
            (nsCapiBufferStruct *) m_Chunks->GetAt(m_ChunkIndex);
        char * currentBuf = cbBuf->m_pBuf;
        char * line = 0;
        PRInt32 i;
        t_bool bFoundNewLine = FALSE;
        for (i = m_Pos; 0 != currentBuf[i] && 
            ((size_t) i < cbBuf->m_pBufSize); i++)
        {
            if ('\n' == currentBuf[i])
            {
                currentBuf[i] = 0;
                bFoundNewLine = TRUE;
                break;
            }
        }
        if (!bFoundNewLine)
        {
            // todo: mark this chunk needs to be saved
            if (m_ChunkIndex < m_Chunks->GetSize() - 1)
            {
                m_ChunkIndex++;
                m_Pos = 0;
            }
            //else
            //{
             //   status = m_NOMORECHUNKS;
            //}
            return aLine;
        }
        line = currentBuf + m_Pos;
        m_Pos = i + 1;
        aLine = line;
        return aLine;
    }
}
//---------------------------------------------------------------------

