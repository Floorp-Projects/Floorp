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

#ifndef __DDEContentTypeConverter_H
//	Avoid include redundancy
//
#define __DDEContentTypeConverter_H

//	Purpose:	DDE content type converter
//	Comments:	Handles all DDE server's that have registered themselves
//					with Netscape.
//	Revision History:
//		01-06-95	created GAB
//

//	Required Includes
//
#include "strmdata.h"

//	Constants
//

//	Structures
//
struct CDDEStreamData : public CStreamData	{
	enum	{
		m_OpenDocument = 0,
		m_ViewDocFile,
		m_ViewDocData
	};

	CString m_csServerName;	//	DDE server name
	DWORD m_dwHowHandle;	//	What services the content type converter should handle the request.
	BOOL m_bQueryViewer;	//	Should we query the viewer first?
	CString m_csMimeType;	//	Our mime type
	
	CDDEStreamData(const char *pServer, const char *pMimeType,
		DWORD dwSDIFlags, BOOL bQueryViewer);
};

struct CDDEDownloadData	{
	CFile *m_pStream;
	CString m_csFileName;
	CString m_csURL;
	CDDEStreamData *m_pCData;
	DWORD m_dwFrameID;
	BOOL m_bDelete;
	
	CDDEDownloadData(CDDEStreamData *pCData, const char *pAddress,
		DWORD dwFrameID);
};

//	Global variables
//

//	Macros
//

//	Function declarations
//
extern "C"	{
NET_StreamClass *dde_stream(int format_out, void *data_obj,
	URL_Struct *URL_s, MWContext  *context);
int dde_StreamWrite(NET_StreamClass *stream, const char *cpWriteData, int32 lLength);
void dde_StreamComplete(NET_StreamClass *stream);
void dde_StreamAbort(NET_StreamClass *stream, int iStatus);
unsigned int dde_StreamReady(NET_StreamClass *stream);
};

#endif // __DDEContentTypeConverter_H
