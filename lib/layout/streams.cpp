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


// Created 10/14/96 hardts
// input and output streams as well as ITapeFileSystem

#ifdef EDITOR
// Work-around for Win16 precompiled header bug -- all .cpp files in
// lib/layout have to include editor.h first. This file
// doesn't even need editor.h.
#include "editor.h"
#include "rosetta.h"

#include "streams.h"
#include "secnav.h"

//-----------------------------------------------------------------------------
//  Streams, Implementation
//-----------------------------------------------------------------------------

//
// LTNOTE:
// There is some uglyness on different platforms to make this work.  We'll 
//  cross that bridge when we come to it.
//

IStreamOut::IStreamOut(){
    stream_buffer = 0;
}

IStreamOut::~IStreamOut(){
    XP_FREE(stream_buffer);
}

int IStreamOut::Printf( char * pFormat, ... ){
    va_list stack;
    int32 len;

    // if the buffer has been allocated, resize it.
    if( stream_buffer ){
        stream_buffer[0] = 0;
    }
    va_start (stack, pFormat);
    stream_buffer = PR_vsprintf_append( stream_buffer, pFormat, stack );
    va_end (stack);

    len = XP_STRLEN( stream_buffer ); 
    Write( stream_buffer, len );
    return len;
}

void IStreamOut::WriteZString(char* pString){
    if( pString ){
        int32 iLen = XP_STRLEN( pString )+1;
        WriteInt( iLen );
        Write( pString, iLen );
    }
    else {
        WriteInt(0);
    }
}

void IStreamOut::WritePartialZString(char* pString, int32 start, int32 end){
    int32 iLen = end - start;
    XP_ASSERT(iLen >= 0);
    if( pString && iLen > 0 ){
        WriteInt( iLen + 1 ); // Account for the '\0'
        Write( pString + start, iLen );
        Write( "", 1 ); // write the '\0'
    }
    else {
        WriteInt(0);
    }
}

char* IStreamIn::ReadZString(){
    char *pRet = 0;
    int32 iLen = ReadInt();
    if( iLen ){
        pRet = (char*)XP_ALLOC(iLen);
        Read( pRet, iLen );
    }
    return pRet;
}


//-----------------------------------------------------------------------------
// File Stream
//-----------------------------------------------------------------------------
//CM: For better cross-platform use, call with file handle already open
CStreamOutFile::CStreamOutFile( XP_File hFile, XP_Bool bBinary ){
    m_status = EOS_NoError;
    m_outputFile = hFile;
    m_bBinary = bBinary;
}

CStreamOutFile::~CStreamOutFile(){
    XP_FileClose( m_outputFile );
}

void CStreamOutFile::Write( char *pBuffer, int32 iCount ){

    if( m_status != EOS_NoError ){
        return;
    }

    // this code doesn't work and it probably should be done at the other end
    //  it is designed to fix CR's showing up in the text.

    int iWrote;

    if( !m_bBinary ){
        int i = 0;
        int iLast = 0;
        int iWrite;
        while( i < iCount ){
            if( pBuffer[i] == '\n' ){
                iWrite = i - iLast;
                if( iWrite ){
                    iWrote = XP_FileWrite( &pBuffer[iLast], iWrite, m_outputFile );
                    if( iWrote != iWrite ){ m_status = EOS_DeviceFull; }
                }
#ifdef XP_MAC
				iWrote = XP_FileWrite("\r", 1, m_outputFile );
#else
				iWrote = XP_FileWrite("\n", 1, m_outputFile );
#endif
                if( iWrote != 1 ){ m_status = EOS_DeviceFull; }
                iLast = i+1;
            }
            i++;
        }
        iWrite = i - iLast;
        if( iWrite ){
            iWrote = XP_FileWrite( &pBuffer[iLast], iWrite, m_outputFile );
            if( iWrote != iWrite ){ m_status = EOS_DeviceFull; }
        }
        return;
    }
    else {
        iWrote = XP_FileWrite( pBuffer, iCount, m_outputFile );
        if( iWrote != iCount ){ m_status = EOS_DeviceFull; }
    }
}

//-----------------------------------------------------------------------------
// NetStream
//-----------------------------------------------------------------------------
CStreamOutNet::CStreamOutNet( MWContext* pContext )
{
    URL_Struct * URL_s;
    Chrome chrome;

    XP_BZERO( &chrome, sizeof( Chrome ) );
    chrome.allow_close = TRUE;
    chrome.allow_resize = TRUE;
    chrome.show_scrollbar = TRUE;
#ifndef XP_WIN
	// NOTE:  need to verify this change on XP_WIN and remove the
	//        ifndef... [ works on XP_UNIX & XP_MAC ]
	//
	chrome.type = MWContextDialog;
#endif

    //
    // LTNOTE: Ownership of the 'chrome' struct isn't documented in the interface.
    //  The windows implementation doesn't appear to keep pointers to the struct.
    //
    MWContext *pNewContext = FE_MakeNewWindow(pContext, NULL, "view-source",
                    &chrome );
    pNewContext->edit_view_source_hack = TRUE;

    URL_s = NET_CreateURLStruct(XP_GetString(EDT_VIEW_SOURCE_WINDOW_TITLE), NET_DONT_RELOAD);  

    URL_s->content_type = XP_STRDUP(TEXT_PLAIN);

    m_pStream = NET_StreamBuilder(FO_PRESENT, URL_s, pNewContext);

    if(!m_pStream){
        XP_ASSERT( FALSE );
        m_status = EOS_FileError;
    } 
    m_status = EOS_NoError;
}

//
// constructor for children
//
CStreamOutNet::CStreamOutNet(void){
    m_pStream = NULL;
    m_status = EOS_NoError;
}

//-----------------------------------------------------------------------------
// GenNetStream
//
// This is what NetStream out should have been! A generalized net function.
// 
//-----------------------------------------------------------------------------
CStreamOutAnyNet::CStreamOutAnyNet(MWContext* pContext, URL_Struct *URL_s, FO_Present_Types type ){
    NET_StreamClass *stream;
    //URL_s->content_type = XP_STRDUP(TEXT_PLAIN);

    stream = NET_StreamBuilder(type, URL_s, pContext);

    if(!stream){
        XP_ASSERT( FALSE );
    }
    else {
	SetStream(stream);
    }
}

CNetStreamToTapeFS::CNetStreamToTapeFS(MWContext* pContext, ITapeFileSystem *tapeFS ){
    NET_StreamClass *stream = NULL;

    if(!stream){
        XP_ASSERT( FALSE );
    }
    else {
	SetStream(stream);
    }
}

//
// Do't forget to free the stream as well...
//
CStreamOutNet::~CStreamOutNet(){
    if (m_pStream == NULL) return;
    (*m_pStream->complete)(m_pStream);
    XP_FREE(m_pStream);
}

void CStreamOutNet::SetStream(NET_StreamClass *stream){
    m_pStream = stream;
}


void CStreamOutNet::Write( char *pBuffer, int32 iCount ){

    // Buffer the output.
    // pBuffer may be a const string, even a ROM string.
    // (e.g. string constants on a Mac with VM.)
    // But networking does in-place transformations on the
    // data to convert it into other character sets.
    
    const int32 kBufferSize = 128;
    char buffer[kBufferSize];

    if (m_pStream == NULL) {
	// if we're trying to write with a null stream, we have definately
	// tripped over an error
	m_status = EOS_FileError;
        return;
    }

    while ( iCount > 0 ) {
        int32 iChunkSize = iCount;
        if ( iChunkSize > kBufferSize ) {
            iChunkSize = kBufferSize;
        }
        XP_MEMCPY(buffer, pBuffer, iChunkSize);

        int status = (*m_pStream->put_block)(m_pStream, buffer, iChunkSize );
    
        if(status < 0){
	    m_status = EOS_FileError;
            (*m_pStream->abort)(m_pStream, status);
	    XP_FREE(m_pStream);
	    m_pStream = NULL;
	    break;
        }
        // status??

        iCount -= iChunkSize;
    }
}

//-----------------------------------------------------------------------------
// Memory Streams
//-----------------------------------------------------------------------------

#define MEMBUF_GROW 10
#define MEMBUF_START 32
//
CStreamOutMemory::CStreamOutMemory(): m_bufferSize(MEMBUF_START),
        m_bufferEnd(0), m_pBuffer(0) {
    m_pBuffer = (XP_HUGE_CHAR_PTR) XP_HUGE_ALLOC( m_bufferSize );
    m_pBuffer[m_bufferEnd] = '\0';
}
  
//
// This implementation intenttionally does not destroy the buffer.  The buffer
//  is kept and destroyed by the stream user.
//   
CStreamOutMemory::~CStreamOutMemory(){
}

//
//
void CStreamOutMemory::Write( char *pSrc, int32 iCount ){
    int32 neededSize = iCount + m_bufferEnd + 1; /* Extra byte for '\0' */

    //
    // Grow the buffer if need be.
    //
    if( neededSize  > m_bufferSize ){
        int32 iNewSize = (neededSize * 5 / 4) + MEMBUF_GROW;
        XP_HUGE_CHAR_PTR pBuf = (XP_HUGE_CHAR_PTR) XP_HUGE_ALLOC(iNewSize);
        if( pBuf ){
            XP_HUGE_MEMCPY(pBuf, m_pBuffer, m_bufferSize );
            XP_HUGE_FREE(m_pBuffer);
            m_pBuffer = pBuf;
            m_bufferSize = iNewSize;
        }
        else {
            // LTNOTE: throw an out of memory exception
            XP_ASSERT(FALSE);
            return;
        }
    }

    XP_HUGE_MEMCPY( &m_pBuffer[m_bufferEnd], pSrc, iCount );
    m_bufferEnd += iCount;
    m_pBuffer[m_bufferEnd] = '\0';
}

// class CConvertCSIDStreamOut

CConvertCSIDStreamOut::CConvertCSIDStreamOut(int16 oldCSID, int16 newCSID, IStreamOut* pStream){
    m_oldCSID = oldCSID;
    m_newCSID = newCSID;
    m_bNullConversion = oldCSID == newCSID;
    m_pStream = pStream;
    if ( ! m_bNullConversion ){
        m_Converter = INTL_CreateCharCodeConverter();
        if ( ! INTL_GetCharCodeConverter(oldCSID, newCSID, m_Converter) ){
            INTL_DestroyCharCodeConverter(m_Converter);
            m_bNullConversion = TRUE;
        }
    }
}

CConvertCSIDStreamOut::~CConvertCSIDStreamOut(){
    if ( ! m_bNullConversion ) {
        INTL_DestroyCharCodeConverter(m_Converter);
    }
    delete m_pStream;
    m_pStream = 0;
}

int16 CConvertCSIDStreamOut::GetOldCSID() {
    return m_oldCSID;
}

int16 CConvertCSIDStreamOut::GetNewCSID() {
    return m_newCSID;
}

void CConvertCSIDStreamOut::Write( char* pBuffer, int32 iCount ){
    if ( ! m_pStream ) {
        return;
    }
    if ( m_bNullConversion ) {
        m_pStream->Write(pBuffer, iCount);
    }
    else {
        // INTL_CallCharaCodeConverter has a pecuilar calling convention.
        // if the converion is a no-op, the argument is returned.
    	// if pToData hasn't changed it won't be null-terminated
    	// else it **MUST** be NULL-terminated so we can get the new length
    	
    	// The character code converter will trash the input string under
        // some circumstances, as for example if a Mac is transcoding to
        // ISO-Latin-1. Therefore we must copy the input stream.
        char* pCopy = (char*) XP_ALLOC(iCount);
        if ( pCopy == NULL) {
        	XP_ASSERT(FALSE);
        	return;
        	}
        XP_MEMCPY(pCopy, pBuffer, iCount);
        char* pToData = (char*) INTL_CallCharCodeConverter(m_Converter, (const unsigned char*) pCopy, iCount);
        if ( pToData ) {
        	if ( pToData != pCopy )
        		 iCount = XP_STRLEN(pToData);
            m_pStream->Write(pToData, iCount);
            if ( pToData != pCopy ) {
                XP_FREE(pToData);
            }
        }
        else {
            // Some sort of error.
            XP_ASSERT(FALSE);
            m_pStream->Write(pCopy, iCount);
        }
        XP_FREE(pCopy);
    }
}

IStreamOut* CConvertCSIDStreamOut::ForgetStream(){
    IStreamOut* result = m_pStream;
    m_pStream = NULL;
    return result;
}

// class CStreamInMemory

void CStreamInMemory::Read( char *pBuffer, int32 iCount ){
    XP_HUGE_MEMCPY( pBuffer, &m_pBuffer[m_iCurrentPos], iCount );
    m_iCurrentPos += iCount;
}

#endif // EDITOR
