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
/*mjudge*/

#include "xp_core.h"
#include "xp_file.h"
#include "xp_mem.h"
#include "xpassert.h"
#include "xp_mcom.h"
#include "mprdecod.h"
#include "memstrem.h"
#include "mimeenc.h"


#define CONTENT_TYPE_MPR_MIMEREL        "multipart/related;"
#define CONTENT_TYPE_TXT_MIMEREL        "text/html;"
#define CONENT_CHARSEL_MIMEREL          "charset="
#define CONTENT_TYPE_MIMEREL            "Content-Type:"
#define CONTENT_ID_MIMEREL              "Content-ID:"
#define CONTENT_TRANS_MIMEREL           "Content-Transfer-Encoding:"
#define BASE64_MIMEREL                  "base64"
#define UUENCODE_MIMEREL                "uuencode"
#define BIT7_MIMEREL                    "7bit"
#define BOUNDARYSTR_MIMEREL             "boundary"
#define XMOZSTATUS_MIMEREL              "X-Mozilla-Status: 8001"
#define CONTENT_DISPOSITION_MIMEREL     "Content-Disposition:"
#define IMAGE_GIF_MIMEREL               "image/gif"
#define IMAGE_JPG_MIMEREL               "image/jpeg"
#define IMAGE_PNG_MIMEREL               "image/png"
#define FILENAME_MIMEREL                "filename"


/*
SimpleMultiPart
SimpleMultiPart
SimpleMultiPart
*/


SimpleMultiPart::SimpleMultiPart()
{
    m_pFileName = NULL;
    m_pContentId = NULL;
    m_pCharset = NULL;
    m_pEncoding = NULL;
    m_iContentType = UNKNOWN_TYPE;
    m_pUrlFileName = NULL;
}



SimpleMultiPart::~SimpleMultiPart()
{
    XP_FREEIF(m_pFileName);
    XP_FREEIF(m_pContentId);
    XP_FREEIF(m_pCharset);
    XP_FREEIF(m_pEncoding);
}



void
SimpleMultiPart::setFileName(const char *p_pFileName)
{
    XP_FREEIF(m_pFileName);
    m_pFileName = XP_STRDUP(p_pFileName);
}



const char *
SimpleMultiPart::getUrlFileName()
{
    XP_FREEIF(m_pUrlFileName);
    m_pUrlFileName = XP_PlatformFileToURL(m_pFileName); 
    return m_pUrlFileName;
}



void
SimpleMultiPart::setCharset(const char *p_pCharset)
{
    XP_FREEIF(m_pCharset);
    m_pCharset = XP_STRDUP(p_pCharset);
}



void
SimpleMultiPart::setContentId(const char *p_pContentId)
{
    XP_FREEIF(m_pContentId);
    m_pContentId = XP_STRDUP(p_pContentId);
}



void
SimpleMultiPart::setEncoding(const char *p_pEncoding)
{
    XP_FREEIF(m_pEncoding);
    m_pEncoding = XP_STRDUP(p_pEncoding);
}



/*
SimpleMultipartRelatedMimeDecoder
SimpleMultipartRelatedMimeDecoder
SimpleMultipartRelatedMimeDecoder
*/
SimpleMultipartRelatedMimeDecoder::SimpleMultipartRelatedMimeDecoder(XP_File p_stream)
{
    m_pBoundaryName = NULL;
    m_ppParts = NULL;
    m_iNumParts = 0;
    m_pFileStream = p_stream;
    m_pMemStream = NULL;
    m_iMemStreamLoc = 0;
    m_iMemStreamLen = 0;
}



SimpleMultipartRelatedMimeDecoder::SimpleMultipartRelatedMimeDecoder(const char *p_stream, int32 p_len)
{
    m_pBoundaryName = NULL;
    m_ppParts = NULL;
    m_iNumParts = 0;
    m_pFileStream = NULL;
    m_pMemStream = p_stream;
    m_iMemStreamLoc = 0;
    m_iMemStreamLen = p_len;
    m_pHeaderFileName = NULL;
}



SimpleMultipartRelatedMimeDecoder::~SimpleMultipartRelatedMimeDecoder()
{
    clear_all();
    if (m_pHeaderFileName)
        XP_FileRemove(m_pHeaderFileName,xpFileToPost);
}



void
SimpleMultipartRelatedMimeDecoder::clear_all()
{
    XP_FREEIF(m_pBoundaryName);
    XP_FREEIF(m_pHeaderFileName);

    for (int i= 0; i< m_iNumParts; i++)
    {
        delete m_ppParts[i];
    }
    XP_FREEIF(m_ppParts);
    m_ppParts = NULL;
    m_iNumParts = 0;
}



SimpleMultiPart *
SimpleMultipartRelatedMimeDecoder::getPart(int32 p_index)
{
    XP_ASSERT(p_index < m_iNumParts);
    if (p_index < m_iNumParts)
        return m_ppParts[p_index];
    return NULL;
}



XP_Bool 
SimpleMultipartRelatedMimeDecoder::begin()
{
    char t_tempbuffer[MAX_BUFFER_LEN + 1];//no parameter allowed longer than MAX_BUFFER_LEN. +1 for the NULL termination
    int t_index = 0; //used to index t_tempbuffer
    XP_FREEIF(m_pHeaderFileName);
    m_pHeaderFileName = WH_TempName (xpFileToPost, m_pPrefix);
    XP_File t_pHeaderFile;
    if (m_pHeaderFileName)
        t_pHeaderFile = XP_FileOpen(m_pHeaderFileName, xpFileToPost, XP_FILE_WRITE);//write text only
    else
        return FALSE;
    if (!m_pHeaderFileName || !t_pHeaderFile)
        return FALSE;

    
    // find first string.  take all data and throw it into the header file if necessary
    XP_Bool t_foundstr = FALSE;

    t_foundstr = searchForString(CONTENT_TYPE_MIMEREL, t_pHeaderFile);
    XP_FileClose(t_pHeaderFile);
    if (!t_foundstr)
        return TRUE; /*just data, no multipartrelated*/

    t_foundstr = searchForString(CONTENT_TYPE_MPR_MIMEREL, NULL);
    if (!t_foundstr)
        return FALSE;
    //find boundary marker.
    t_foundstr = searchForString(BOUNDARYSTR_MIMEREL, NULL);
    if (!t_foundstr)
        return FALSE;
    if (!eatWhite())
        return FALSE;
    if (getCh() != '=')
        return FALSE;
    if (!eatWhite())
        return FALSE;
    if (getCh() != '\"')
        return FALSE;
    char t_char;
    while( (t_char = getCh()) )
    {
        if (t_index >= MAX_BUFFER_LEN || isspace(t_char) || t_char == '\"')
            break; //done
        t_tempbuffer[t_index++] = t_char;
    }
    if (t_index >= MAX_BUFFER_LEN)
        return FALSE;
    t_tempbuffer[t_index] = 0; //nul terminate the string
    m_pBoundaryName = XP_STRDUP(t_tempbuffer);
    if (!isspace(getCh()))
        return FALSE;
    //fun part
    XP_Bool t_done = !searchForString(m_pBoundaryName, NULL);

    //found 1st boundary. keep going until we reach the last
    while(!t_done)
    {
        if ( getCh() == '-')
            if (getCh() == '-')
            {
                t_done = TRUE; //reached the end
                break; //just break here. if we need to check t_done someother time thats ok
            }
            else
                backUp();
        backUp();
        if (!getNextString(t_tempbuffer, MAX_BUFFER_LEN))
            return FALSE;
        if (XP_STRCASECMP(CONTENT_TYPE_MIMEREL,t_tempbuffer))
            return FALSE;
        if (!eatWhite())
            return FALSE;
        if (!getNextString(t_tempbuffer, MAX_BUFFER_LEN))
            return FALSE;
        if (!XP_STRCASECMP(CONTENT_TYPE_TXT_MIMEREL,t_tempbuffer))
        {//text/html
            if (!readTextHtml(t_tempbuffer))
                return FALSE;
        }
        else 
        {//image ect
            if (!readEncoded(t_tempbuffer))
                return FALSE;
        }
    }
    return TRUE;
}



//why waste space on a temp buffer?
XP_Bool 
SimpleMultipartRelatedMimeDecoder::readTextHtml(char p_buffer[MAX_BUFFER_LEN])
{
    SimpleMultiPart *t_part = new SimpleMultiPart();
    if (!t_part)
        return FALSE;

    t_part->setType(SimpleMultiPart::TEXTHTML);
    if (!getNextString(p_buffer, MAX_BUFFER_LEN))
        {   delete t_part;  return FALSE; }
    if (!XP_STRCASECMP(CONENT_CHARSEL_MIMEREL,p_buffer))
    {//we have a charset!
        if (!getNextString(p_buffer, MAX_BUFFER_LEN))
            {   delete t_part;  return FALSE; }
        t_part->setCharset(p_buffer);
        if (!getNextString(p_buffer, MAX_BUFFER_LEN))
            {   delete t_part;  return FALSE; }
    }
    if (XP_STRCASECMP(CONTENT_TRANS_MIMEREL,p_buffer))
        if (!searchForString(CONTENT_TRANS_MIMEREL, NULL))
            {   delete t_part;  return FALSE; }
    if (!getNextString(p_buffer, MAX_BUFFER_LEN))
        {   delete t_part;  return FALSE; }
    t_part->setEncoding(p_buffer);
    /*
    now it gets interesting. we need a new file to be created. we will search
    for the boundary string.  until we find it, we will throw each char into the file.
    perhaps this isnt the fastest.
    */
    eatWhite();
    t_part->setFileName(WH_TempName (xpFileToPost, m_pPrefix));
    if (!t_part->getFileName())
        {   delete t_part;  return FALSE; }
    XP_File t_file;
    //write text only
    t_file = XP_FileOpen(t_part->getFileName(), xpFileToPost, XP_FILE_WRITE);
    if (!t_file)
        {   delete t_part;  return FALSE; }
    if (!searchForString(m_pBoundaryName, t_file))//writes to file
        {   delete t_part;  return FALSE; }
    XP_FileClose(t_file);
    m_iNumParts++;
    if (m_ppParts)
        m_ppParts = (SimpleMultiPart **)XP_REALLOC(m_ppParts, m_iNumParts * sizeof (SimpleMultiPart *));
    else
        m_ppParts = (SimpleMultiPart **)XP_ALLOC(m_iNumParts * sizeof (SimpleMultiPart *));
    m_ppParts[m_iNumParts -1] = t_part;
    return TRUE;
}



//why waste space on a temp buffer?
XP_Bool 
SimpleMultipartRelatedMimeDecoder::readEncoded(char p_buffer[MAX_BUFFER_LEN])
{
    SimpleMultiPart *t_part = new SimpleMultiPart();
    if (!t_part)
        return FALSE;

    if (!XP_STRCASECMP(IMAGE_GIF_MIMEREL,p_buffer))
        t_part->setType(SimpleMultiPart::IMAGEGIF);
    else if (!XP_STRCASECMP(IMAGE_JPG_MIMEREL,p_buffer))
        t_part->setType(SimpleMultiPart::IMAGEJPG);
    else if (!XP_STRCASECMP(IMAGE_PNG_MIMEREL,p_buffer))
        t_part->setType(SimpleMultiPart::IMAGEPNG);
    else    
        {   delete t_part;  return FALSE; }

    /*
    we must find a content id or this part is useless. and all bets are off.
    */    
    if (!getNextString(p_buffer, MAX_BUFFER_LEN))
        {   delete t_part;  return FALSE; }
    if (XP_STRCASECMP(CONTENT_ID_MIMEREL,p_buffer))
        {   delete t_part;  return FALSE; }
    eatWhite();
    if (getCh() != '<')
        {   delete t_part;  return FALSE; }
    if (!getNextString(p_buffer, MAX_BUFFER_LEN))
        {   delete t_part;  return FALSE; }
    if (getCh() != '>')
        {   delete t_part;  return FALSE; }
    //we have a content-id
    t_part->setContentId(p_buffer);

    if (!getNextString(p_buffer, MAX_BUFFER_LEN))
        {   delete t_part;  return FALSE; }
    if (XP_STRCASECMP(CONTENT_TRANS_MIMEREL,p_buffer))
        {   delete t_part;  return FALSE; }

    if (!getNextString(p_buffer, MAX_BUFFER_LEN))
        {   delete t_part;  return FALSE; }
    t_part->setEncoding(p_buffer);

    if (!getNextString(p_buffer, MAX_BUFFER_LEN))
        {   delete t_part;  return FALSE; }
    if (XP_STRCASECMP(CONTENT_DISPOSITION_MIMEREL,p_buffer))
        {   delete t_part;  return FALSE; }
    if (!searchForString(FILENAME_MIMEREL, NULL))
        {   delete t_part;  return FALSE; }
    eatWhite();
    if (getCh()!='=')
        {   delete t_part;  return FALSE; }
    eatWhite();
    char t_char; //eat the disposition filename
    while ((t_char = getCh()) && !isspace(t_char));
    backUp();
    /*we didnt care about the disposition. we need to make a completely unknown file for
    security purposes*/


    /*
    now it gets interesting. we need a new file to be created. we will search
    for the boundary string.  until we find it, we will throw each char into the file.
    perhaps this isnt the fastest.
    */
    eatWhite();
    t_part->setFileName(WH_TempName (xpFileToPost, m_pPrefix));
    if (!t_part->getFileName())
        {   delete t_part;  return FALSE; }
    XP_File t_file;
    //write text only
    t_file = XP_FileOpen(t_part->getFileName(), xpFileToPost, XP_FILE_WRITE);
    if (!t_file)
        {   delete t_part;  return FALSE; }
    if (!searchForString(m_pBoundaryName, t_file))//writes to file
    {
        XP_FileClose(t_file);
        delete t_part;
        return FALSE; 
    }
    XP_FileClose(t_file);
    m_iNumParts++;
    if (m_ppParts)
        m_ppParts = (SimpleMultiPart **)XP_REALLOC(m_ppParts, m_iNumParts * sizeof (SimpleMultiPart *));
    else
        m_ppParts = (SimpleMultiPart **)XP_ALLOC(m_iNumParts * sizeof (SimpleMultiPart *));
    m_ppParts[m_iNumParts -1] = t_part;
    return TRUE;
}



XP_Bool 
SimpleMultipartRelatedMimeDecoder::getNextString(char *p_buffer, int32 p_maxbuflen)
{
    if (!eatWhite())
        return FALSE;
    int t_index = 0;
    char t_char = 0;
    while( t_index < p_maxbuflen )
    {
        t_char = getCh();
        if (!t_char || t_char ==':' || t_char ==';' || t_char == '=' )
        {
            p_buffer[t_index++] = t_char;
            if (t_char)
            {
                if (t_index < p_maxbuflen)
                    p_buffer[t_index] = 0;
                else
                    return FALSE;//no room for null termination
            }
            return TRUE;
        }
        if (isspace(t_char) || t_char == '\"' || t_char =='<' || t_char == '>')
        {
            backUp();
            p_buffer[t_index] = 0;
            return TRUE;
        }
        p_buffer[t_index++] = t_char;
    }
    return FALSE; //ran out of room 

}



XP_Bool
SimpleMultipartRelatedMimeDecoder::searchForString(const char *t_searchstring, XP_File p_output)
{
    char t_char;
    int32 t_strlen = XP_STRLEN(t_searchstring);
    int32 t_stringindex = 0;
    while ((t_char = getCh()) && t_stringindex < t_strlen)
    {
        if (t_searchstring[t_stringindex] == t_char)
        {
            t_stringindex++;
        }
        else
        {
            if (t_stringindex && p_output)
                XP_FileWrite(t_searchstring,t_stringindex,p_output);

            t_stringindex = 0;
            if (p_output)
                XP_FileWrite(&t_char,1,p_output);
        }
    }
    if (t_char)
    {
        backUp();
        return TRUE;
    }
    return FALSE;
}



char
SimpleMultipartRelatedMimeDecoder::getCh()
{
    if (m_pMemStream)
    {
        if (m_iMemStreamLoc < m_iMemStreamLen)
            return m_pMemStream[m_iMemStreamLoc++];
        else
        {
            m_iMemStreamLoc--;
            return 0;
        }
    }
    else if (m_pFileStream)
    {
        char t_char[2];
        if (1 == XP_FileRead(t_char,1,m_pFileStream))
            return t_char[0];
        else
            return 0;
    }
    else
    {
        XP_ASSERT(FALSE);
        return 0;
    }
}



void
SimpleMultipartRelatedMimeDecoder::backUp()
{
    if (m_pMemStream)
    {
        m_iMemStreamLoc--;
    }
    else if (m_pFileStream)
    {
        XP_FileSeek(m_pFileStream, -1, SEEK_CUR);
    }
    else
    {
        XP_ASSERT(FALSE);
    }
}



XP_Bool
SimpleMultipartRelatedMimeDecoder::eatWhite()
{
    char t_char;
    while( (t_char = getCh()) && isspace( t_char ) );
    if (t_char)
    {
        backUp();
        return TRUE;
    }
    return FALSE;
}



int32
SimpleMultipartRelatedMimeDecoder::lookUpByPartId(const char *p_id)
{
    for (int i = 0;i < getNumberOfParts(); i++ )
    {
        SimpleMultiPart *t_part = getPart(i);
        if (t_part->getContentId())
            if (! XP_STRNCMP(t_part->getContentId(), p_id, XP_STRLEN(t_part->getContentId())))
            {
                return i;
            }
    }
    return -1;
}



/*
OTHER API CALLS
*/



#define CHUNK_SIZE 255
char *ReadBufferFromFile(const char *p_pFileName)
{
    memstream t_stream(CHUNK_SIZE,CHUNK_SIZE);
    XP_File t_inputfile;
    char t_readbuffer[CHUNK_SIZE];
    if( (t_inputfile = XP_FileOpen(p_pFileName,xpFileToPost,XP_FILE_READ)) != NULL )
    {
        /* Attempt to read in READBUFLEN characters */
        while (!feof( t_inputfile ))
        {
            int numread = XP_FileRead( t_readbuffer, CHUNK_SIZE -1, t_inputfile );
            if (ferror(t_inputfile))
            {
                XP_ASSERT(FALSE);
                break;
            }
            t_stream.write(t_readbuffer,numread);
        }
        XP_FileClose( t_inputfile );
    }
    t_stream.write("",1); /*null terminate*/
    return t_stream.str();
}



extern "C" int mimer_outputfile_func(const char *p_buffer,int32 p_size,void *closure);


XP_Bool DecodeSimpleMime(SimpleMultipartRelatedMimeDecoder &p_rDecoder, const char *p_pPrefix)
{
    char *t_pNewFileName = NULL;
    for (int i = 0 ; i < p_rDecoder.getNumberOfParts(); i++)
    {
        SimpleMultiPart *t_part = p_rDecoder.getPart(i);
        XP_File t_file;
        XP_FREEIF(t_pNewFileName);
        t_pNewFileName = WH_TempName (xpFileToPost, p_pPrefix);
        if (!t_pNewFileName)
            return FALSE;
        char *t_suffix;
        t_suffix = XP_STRRCHR(t_pNewFileName,'.'); //searching for suffix .XXX
        if (t_suffix && !XP_STRNCASECMP(t_suffix,".TMP ",4))
        {
            if (t_part->getType() == SimpleMultiPart::IMAGEGIF)
                XP_STRNCPY_SAFE(t_suffix,".gif",5);
            if (t_part->getType() == SimpleMultiPart::IMAGEJPG)
                XP_STRNCPY_SAFE(t_suffix,".jpg",5);
            if (t_part->getType() == SimpleMultiPart::IMAGEPNG)
                XP_STRNCPY_SAFE(t_suffix,".png",5);
        }

        MimeDecoderData *t_data;
        if ( !XP_STRCMP(t_part->getEncoding() , BASE64_MIMEREL))
        {
            t_file = XP_FileOpen(t_pNewFileName, xpFileToPost, XP_FILE_WRITE_BIN);//write binary only
            if (!t_file)
            {
                XP_FREEIF(t_pNewFileName);
                return FALSE;
            }
            t_data = MimeB64DecoderInit(mimer_outputfile_func, t_file);
        }
        else if ( !XP_STRCMP(t_part->getEncoding() , UUENCODE_MIMEREL))
        {
            t_file = XP_FileOpen(t_pNewFileName, xpFileToPost, XP_FILE_WRITE_BIN);//write binary only
            if (!t_file)
            {
                XP_FREEIF(t_pNewFileName);
                return FALSE;
            }
            t_data = MimeUUDecoderInit(mimer_outputfile_func, t_file);
        }
        else if ( !XP_STRCMP(t_part->getEncoding() , BIT7_MIMEREL))
        {

            continue; /*dont do anything to this one*/
        }
        else
            continue;

        XP_File t_inputfile;
        char t_readbuffer[CHUNK_SIZE];
        if( (t_inputfile = XP_FileOpen(t_part->getFileName(),xpFileToPost,XP_FILE_READ)) != NULL )
        {
            /* Attempt to read in READBUFLEN characters */
            while (!feof( t_inputfile ))
            {
                int numread = XP_FileRead( t_readbuffer, CHUNK_SIZE -1, t_inputfile );
                if (ferror(t_inputfile))
                {
                    XP_ASSERT(FALSE);
                    break;
                }
                MimeDecoderWrite(t_data,t_readbuffer,numread);
            }
            XP_FileClose( t_inputfile );
        }
        XP_FileClose( t_file );
        XP_FileRemove(t_part->getFileName(),xpFileToPost);
        t_part->setFileName(t_pNewFileName);
    }
    return TRUE;
}



char *ParseBuffer(char *p_pOldBuffer,SimpleMultipartRelatedMimeDecoder &p_rDecoder)
{
    if (!p_pOldBuffer)
        return NULL;
    /*find occurrances if image SRC urls*/
    int t_index = 0;
    memstream t_stream(CHUNK_SIZE,CHUNK_SIZE);
    memstream t_urlstream;
    while (p_pOldBuffer[t_index])
    {
        char t_char = p_pOldBuffer[t_index++];
        if (t_char != '<')
            continue;
        while(p_pOldBuffer[t_index] && isspace(t_char))
            t_char = p_pOldBuffer[t_index++];
        if (!p_pOldBuffer[t_index] || p_pOldBuffer[t_index] != 'I')
            continue;
        if (XP_STRNCASECMP(p_pOldBuffer + t_index,"IMG ",4))
            continue;
        t_index+=4;
        XP_Bool t_found = TRUE;
        while( t_found && p_pOldBuffer[t_index])
        {
            if (t_char == '>')
            {
                t_found = FALSE;
                break;
            }

            if (!p_pOldBuffer[t_index] || p_pOldBuffer[t_index] != 'S')
            {
                t_index++;
                continue;
            }
            if (XP_STRNCASECMP(p_pOldBuffer + t_index,"SRC",3))
            {
                t_index++;
                continue;
            }
            t_index+=3;
            while(p_pOldBuffer[t_index] && isspace(t_char))
                t_char = p_pOldBuffer[t_index++];
            if (!p_pOldBuffer[t_index] || p_pOldBuffer[t_index++] != '=')
                continue;
            while(p_pOldBuffer[t_index] && isspace(t_char))
                t_char = p_pOldBuffer[t_index++];
            if (!p_pOldBuffer[t_index] || p_pOldBuffer[t_index++] != '\"')
                continue;
            while(p_pOldBuffer[t_index] && isspace(t_char))
                t_char = p_pOldBuffer[t_index++];
            int t_marker = t_index;
            if (XP_STRNCASECMP(p_pOldBuffer + t_index, "cid:", 4))
            {
                t_found = FALSE;
                continue;
            }
            t_index += 4;
            while(p_pOldBuffer[t_index] && p_pOldBuffer[t_index]!='\"')
                t_urlstream.write(p_pOldBuffer + t_index++,1);
            if (!p_pOldBuffer[t_index])
            {
                t_found = FALSE;
                continue;
            }
            //lookup t_urlstream for new filename
            int t_partidx = p_rDecoder.lookUpByPartId(t_urlstream.str());
            t_urlstream.clear();
            if (t_partidx < 0)
                continue;
            SimpleMultiPart *t_part = p_rDecoder.getPart(t_partidx);
            if (!t_part)
                continue;
            t_stream.write(p_pOldBuffer,t_marker);
            t_stream.write(t_part->getUrlFileName(),XP_STRLEN(t_part->getUrlFileName()));//gets new filename MUST MAKE THIS A URL
            p_pOldBuffer += t_index;
            t_index = 0;
            break;
        }
    }
    t_stream.write(p_pOldBuffer,t_index);    
    t_stream.write("",1); /*null terminate*/
    return t_stream.str();//does not free memory unless unfrozen.
}
