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

#ifndef __OLEContentTypeConverter_H
//	Avoid include redundancy
//
#define __OLEContentTypeConverter_H

//	Purpose:    OLE content type converter
//	Comments:   Handles all OLE automation servers that have registered themselves with Netscape.

#include "strmdata.h"
#include "oleview1.h"

//	Structures
//
struct COLEStreamData : public CStreamData  {
    CString m_csServerName; //  OLE Automation Server Name.
    CString m_csMimeType;   //  The mime type of the data to handle.
    COLEStreamData(const char *pServer, const char *pMimeType);
};

struct COLEDownloadData {
    IIViewer1 m_Viewer;
    COLEStreamData *m_pCData;
    enum    {
        m_InitialBufferSize = 4096
    };
    BSTR m_pBuffer;
    COLEDownloadData(COLEStreamData *pCData, const char *pAddress);
    ~COLEDownloadData();

    //  Special hackage to handle the netlib not calling Ready before Write.
    BOOL m_bReadyCalled;
};


//	Function declarations
//
extern "C"	{
NET_StreamClass *ole_stream(int format_out, void *data_obj,
	URL_Struct *URL_s, MWContext  *context);
int ole_StreamWrite(NET_StreamClass *stream, const char *cpWriteData, int32 lLength);
void ole_StreamComplete(NET_StreamClass *stream);
void ole_StreamAbort(NET_StreamClass *stream, int iStatus);
unsigned int ole_StreamReady(NET_StreamClass *stream);
};

#endif // __OLEContentTypeConverter_H
