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

/*
I am writing a multipart mime decoder because the original one is so tied into 
libmsg.  This one is very simple. It will take a multipart/related message and
split it into N files writing them to nswebXX.tmp in the temp directory.  It will
then have another mechanism for BASE64 bit decoding the necessary files and rewriting them
to the temp directory.  This should be very simple.  It will only handle 1 layer of parts.
It will search for "Content-Type: multipart/related;"
If it doesn't find it. then the whole thing is considered one part text/HTML.

*/

#ifndef _MPRDECOD_H
#define _MPRDECOD_H


/*I am in a hurry maybe this should be a class hierarchy of mime types, 
but this is supposed to be simple*/
class SimpleMultiPart
{
public:
    enum SIMPLE_CONTENT_TYPE {UNKNOWN_TYPE, TEXTHTML, IMAGEJPG, IMAGEGIF, IMAGEPNG};
private:
    char *m_pFileName;
    char *m_pUrlFileName;
    char *m_pContentId;
    char *m_pCharset;
    char *m_pEncoding;
    SIMPLE_CONTENT_TYPE m_iContentType;
public:

    
    SimpleMultiPart();
    ~SimpleMultiPart();


    /*gets*/
    const char *getFileName(){return m_pFileName;} /*this return pointer is not yours*/
    const char *getUrlFileName(); /*this return pointer is not yours*/
    const char *getCharset(){return m_pCharset;} /*this return pointer is not yours*/
    const char *getContentId(){return m_pContentId;} /*this return pointer is not yours*/
    const char *getEncoding(){return m_pEncoding;}
    SIMPLE_CONTENT_TYPE getType(){return m_iContentType;}
    
    /*sets*/
    void setFileName(const char *p_pFileName); /*this is copied*/
    void setCharset(const char *p_pCharset); /*this is copied*/
    void setContentId(const char *p_pContentId); /*this is copied*/
    void setEncoding(const char *p_pEncoding); /*this is copied*/
    void setType(SIMPLE_CONTENT_TYPE p_type){m_iContentType = p_type;}
};



class SimpleMultipartRelatedMimeDecoder
{
public:
    enum {MAX_BUFFER_LEN = 255};
private:
    const char *m_pPrefix;

    char *m_pBoundaryName;
    SimpleMultiPart **m_ppParts;
    int32 m_iNumParts;
/*write data before actual parts to header file incase this IS the only part*/
    char *m_pHeaderFileName;

    const char *m_pMemStream;
    int32 m_iMemStreamLoc;
    int32 m_iMemStreamLen;
    XP_File m_pFileStream; //filestream

/*stream manipulations*/
    XP_Bool eatWhite();
    char getCh();
    XP_Bool searchForString(const char *p_string, XP_File p_output);/* searches for string. when it cant find it writes data to p_output*/
    void backUp(); //went to far in the stream, back up one char
    XP_Bool getNextString(char *p_buffer, int32 p_maxbuflen);

/*helper funcs */
    XP_Bool readTextHtml(char p_buffer[MAX_BUFFER_LEN]);
    XP_Bool readEncoded(char p_buffer[MAX_BUFFER_LEN]);

public:
/*constructor destructor*/
    SimpleMultipartRelatedMimeDecoder(XP_File p_stream);/*this class will not close the input file stream */
    SimpleMultipartRelatedMimeDecoder(const char *p_stream, int32 p_len);
    ~SimpleMultipartRelatedMimeDecoder();

    XP_Bool begin();        
    void clear_all();//called by destructor also

    void setFilePrefix(const char *p_prefix){m_pPrefix = p_prefix;}

    SimpleMultiPart *getPart(int32 p_index); /*this return pointer is not yours*/
    int32 getNumberOfParts(){return m_iNumParts;}
    
    const char *getHeaderFileName(){return m_pHeaderFileName;}

    int32 lookUpByPartId(const char *);
};


/*
OTHER API CALLS
*/


char *ParseBuffer(char *p_pOldBuffer,SimpleMultipartRelatedMimeDecoder &p_rDecoder);
XP_Bool DecodeSimpleMime(SimpleMultipartRelatedMimeDecoder &p_rDecoder,const char *p_pPrefix);
char *ReadBufferFromFile(const char *p_pFileName);
#endif //_MPRDECOD_H

