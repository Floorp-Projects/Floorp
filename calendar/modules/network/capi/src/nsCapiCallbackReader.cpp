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
nsCapiCallbackReader::AddChunk(UnicodeString * u)
{
    if (m_Chunks == 0)
        m_Chunks = new JulianPtrArray();
    PR_ASSERT(m_Chunks != 0);
    if (m_Chunks != 0)
    {
        m_Chunks->Add(u);
    }
}

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

void nsCapiCallbackReader::deleteUnicodeStringVector(JulianPtrArray * stringVector)
{
    t_int32 i;
    if (stringVector != 0) 
    {
        for (i = stringVector->GetSize() - 1; i >= 0; i--)
        {
            delete ((UnicodeString *) stringVector->GetAt(i));
        }
    }
}

//---------------------------------------------------------------------

nsCapiCallbackReader::~nsCapiCallbackReader()
{
    if (m_Chunks != 0)
    {
        deleteUnicodeStringVector(m_Chunks);
        delete m_Chunks; m_Chunks = 0;
    }
}

//---------------------------------------------------------------------

t_int8 nsCapiCallbackReader::read(ErrorCode & status)
{
#if 0
    t_int32 i = 0;
    
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
        
            UnicodeString string = *((UnicodeString *) m_Chunks->GetAt(m_ChunkIndex));
            if (m_Pos < string.size())
            {  
                // return index of this 
                status = ZERO_ERROR;

                if (m_Encoding == JulianUtility::MimeEncoding_QuotedPrintable)
                {
                    if ((string)[(TextOffset) m_Pos] == '=')
                    {
                        if (string.size() >= (t_int32)(m_Pos + 3))
                        {
                            if (ICalReader::isHex((t_int8) string[(TextOffset)(m_Pos + 1)]) && 
                                ICalReader::isHex((t_int8) string[(TextOffset)(m_Pos + 2)]))
                            {
                                t_int8 c;
                                c = ICalReader::convertHex(
                                      (char) string[(TextOffset) (m_Pos + 1)], 
                                      (char) string[(TextOffset) (m_Pos + 2)]
                                      );
                                m_Pos += 3;
                                return c;
                            }
                            else
                            {
                                return (t_int8) (string)[(TextOffset) m_Pos++];
                            }
                        }
                        else
                        {
                            t_int32 lenDiff = string.size() - m_Pos;
                            char fToken;
                            char sToken;
                            t_bool bSetFToken = FALSE;
                            t_int32 tempIndex = m_ChunkIndex;
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
                                    UnicodeString nextstring = *((UnicodeString *) m_Chunks->GetAt(tempIndex + 1));
                                    tempIndex++;

                                    if (nextstring.size() >= 2)
                                    {
                                        t_int8 c;
                                        if (lenDiff == 2)
                                        {
                                            
                                            fToken = (char) string[(TextOffset) (string.size() - 1)];
                                            sToken = (char) nextstring[(TextOffset) 0];                                            

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
                                                return (t_int8) (string)[(TextOffset) m_Pos++];
                                            }
                                        }
                                        else
                                        {
                                            // lenDiff = 1

                                            if (bSetFToken)
                                            {
                                                sToken = (char)nextstring[(TextOffset) 0];
                                            }
                                            else 
                                            {
                                                fToken = (char)nextstring[(TextOffset) 0];
                                                sToken = (char)nextstring[(TextOffset) 1];
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
                                                return (t_int8) (string)[(TextOffset) m_Pos++];
                                            }
                                        }
                                    }
                                    else
                                    {
                                        if (nextstring.size() > 0)
                                        {
                                            if (!bSetFToken)
                                            {
                                                fToken = (char)nextstring[(TextOffset) 0];
                                                bSetFToken = TRUE;

                                            }
                                            else
                                            {
                                                sToken = (char)nextstring[(TextOffset) 0];
                                            
                                                if (ICalReader::isHex(fToken) && ICalReader::isHex(sToken))
                                                {
                                                    char c;
                                                    c = ICalReader::convertHex(fToken, sToken); 
    
                                                    m_Pos = 1;
                                                    m_ChunkIndex = tempIndex;
                                                    bSetFToken = FALSE;
                                                    return c;
                                                }
                                                else
                                                {
                                                    return (t_int8) (string)[(TextOffset) m_Pos++];
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
                        return (t_int8) (string)[(TextOffset) m_Pos++];
                    }

                }
                else
                {
                    return (t_int8) (string)[(TextOffset) m_Pos++];
                }
            }
            else
            {
                // delete front string from list, try reading from next chunk
                m_Pos = 0;
                t_int32 j = m_ChunkIndex;
                m_ChunkIndex++;

                if (-1 == m_Mark)
                {
                    for (i =  m_ChunkIndex - 1; i >=0 ; i--)
                    {
                        delete ((UnicodeString *) m_Chunks->GetAt(i));
                        m_Chunks->RemoveAt(i);
                        m_ChunkIndex--;
                    }                
                }
            }
        }
    }
    status = 1;
    return -1;
#else 

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
            if ((size_t) m_Pos < cbBuf->m_pBufSize)
            {                
                return buf[m_Pos++];
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
#endif
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
#if 0
    status = ZERO_ERROR;
    t_int8 c = 0;
    t_int32 oldPos = m_Pos;
    t_int32 oldChunkIndex = m_ChunkIndex;

    aLine = "";

    c = read(status);
    while (!(FAILURE(status)))
    {
        if (status == m_NOMORECHUNKS)
        {
            // block
            break;
        }
        /* Break on '\n', '\r\n', and '\r' */
        else if (c == '\n')
        {
            break;
        }
        else if (c == '\r')
        {
            mark();
            c = read(status);
            if (FAILURE(status))
            {
                break;
            }
            else if (c == '\n')
            {
                break;
            }
            else
            {
                reset();
                break;
            }
        }
        aLine += c;
        c = read(status);

    }
    return aLine;
#else

    aLine = "";

    if (m_Chunks == 0 || m_Chunks->GetSize() == 0 || 
            m_ChunkIndex >= m_Chunks->GetSize())
    {
        status = m_NOMORECHUNKS; // no more chunks, should block
        return aLine;
    }    
    else
    {
        nsCapiBufferStruct * cbBuf = (nsCapiBufferStruct *) m_Chunks->GetAt(m_ChunkIndex);
        char * currentBuf = cbBuf->m_pBuf;
        char * line = 0;
        PRInt32 i;
        t_bool bFoundNewLine = FALSE;
        for (i = m_Pos; 0 != currentBuf[i] && ((size_t) i < cbBuf->m_pBufSize); i++)
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
#endif

}
//---------------------------------------------------------------------

