/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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


#ifndef STREAMS_H
#define STREAMS_H

#ifdef EDITOR

#include "xp.h"
#include "xp_file.h"
#include "itapefs.h"
#include "libi18n.h"

//-----------------------------------------------------------------------------
//  Streams
//-----------------------------------------------------------------------------

// IStreamOut is in "itapefs.h"

class IStreamIn {
public:
    IStreamIn() {}
    virtual ~IStreamIn() {}

    virtual void Read( char *pBuffer, int32 iCount )=0;
    int32 ReadInt(){ int32 i; Read((char*)&i, sizeof(int32)); return i; }
    char *ReadZString();
};


//
// Output Stream as file.
//
class CStreamOutFile: public IStreamOut {
private:
    EOutStreamStatus m_status;
    XP_File m_outputFile;  
    XP_Bool m_bBinary;
public:
    CStreamOutFile(  XP_File hFile, XP_Bool bBinary  );
    virtual ~CStreamOutFile();
    virtual void Write( char *pBuffer, int32 iCount );
    virtual EOutStreamStatus Status(){ return m_status; }
};

//
// Stream out to net.
//
class CStreamOutNet: public IStreamOut {
private:
    NET_StreamClass * m_pStream;
    EOutStreamStatus m_status;
protected:
    CStreamOutNet();
    void SetStream(NET_StreamClass *m_pStream);
public:
    CStreamOutNet( MWContext* pContext );
    virtual ~CStreamOutNet();
    virtual void Write( char *pBuffer, int32 iCount );
    virtual EOutStreamStatus Status(){ return m_status; }
};

class CStreamOutAnyNet: public CStreamOutNet {
public:
    CStreamOutAnyNet(MWContext* pContext,URL_Struct *URL_s,FO_Present_Types type);
};

class CNetStreamToTapeFS: public CStreamOutNet {
public:
    CNetStreamToTapeFS(MWContext* pContext,ITapeFileSystem *);
};

//
// Output Stream as memory
//
class CStreamOutMemory: public IStreamOut {
private:
    int32 m_bufferSize;
    int32 m_bufferEnd;
    XP_HUGE_CHAR_PTR m_pBuffer;
public:
    CStreamOutMemory();
    virtual ~CStreamOutMemory();
    virtual void Write( char *pBuffer, int32 iCount );
    XP_HUGE_CHAR_PTR GetText(){ return m_pBuffer; }
    int32 GetLen(){ return m_bufferEnd; }
};

// Convert character set encodings while streaming out.
// Adopts an existing IStreamOut* object.
// Deletes the existing IStreamOut* object when
// it itself is deleted. (Unless ForgetStream() is called.)

class CConvertCSIDStreamOut : public IStreamOut {
private:
    IStreamOut* m_pStream;
    CCCDataObject m_Converter;
    XP_Bool m_bNullConversion;
    int16 m_oldCSID;
    int16 m_newCSID;
public:
    CConvertCSIDStreamOut(int16 oldCSID, int16 newCSID, IStreamOut* pStream);
    virtual ~CConvertCSIDStreamOut();
    virtual void Write( char* pBuffer, int32 iCount );
    IStreamOut* ForgetStream();
    int16 GetOldCSID();
    int16 GetNewCSID();
};

class CStreamInMemory: public IStreamIn {
private:
    XP_HUGE_CHAR_PTR m_pBuffer;
    int32 m_iCurrentPos;
public:
    virtual void Read( char *pBuffer, int32 iCount );
    CStreamInMemory(char *pMem): m_pBuffer(pMem), m_iCurrentPos(0){}
};



#endif // EDITOR
#endif
