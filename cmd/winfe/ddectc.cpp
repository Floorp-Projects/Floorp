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
#include "stdafx.h"

#include "wfedde.h"
#include "ddectc.h"
#include "cxsave.h"
#include "extgen.h"

extern "C" int MK_DISK_FULL;	 // defined in allxpstr.h

CDDEStreamData::CDDEStreamData(const char *pServer, const char *pMimeType,
	DWORD dwSDIFlags, BOOL bQueryViewer) : CStreamData(CStreamData::m_DDE)	{
//	Purpose:	Construct the data stream object.
//	Arguments:	pServer	The name of the server that we should contact
//							in our data stream.
//				pMimeType	The mime type fo the stream.
//				dwSDIFlags	How we should contact the server.  This will
//								converted into an internal type.
//				bQueryViewer	Wether or not we should query a remote applciation
//									for the file in which we should save our data.
//	Returns:	none
//	Comments:	Note that we initialize the base class to know we are a DDE
//					viewer.
//	Revision History:
//		01-06-95	created GAB
//

	//	Simply assign the members over for now.
	m_csServerName = pServer;
	m_csMimeType = pMimeType;
	m_bQueryViewer = bQueryViewer;
	
	switch(dwSDIFlags)	{
	case 0x1L:
		m_dwHowHandle = m_OpenDocument;
		break;
	case 0x4L:
		m_dwHowHandle = m_ViewDocFile;
		break;
	case 0x8L:
		m_dwHowHandle = m_ViewDocData;
		break;
	}
}

CDDEDownloadData::CDDEDownloadData(CDDEStreamData *pCData,
	const char *pAddress, DWORD dwFrameID)	{
//	Purpose:	Create an instance of the download data.
//	Arguments:	pCData	A global object representing our DDE registered
//							viewer.
//				pAddress	The URL we're loading, we'll use this to
//								construct a file name, and open it.
//				dwFrameID	The frame performing the download.
//	Returns:	none
//	Comments:	Download instance specific member.
//	Revision History:
//		01-06-95	created GAB
//

	//	Assign over our data.
	m_pCData = pCData;
	
	//	Mark so that we know to delete any files that we create at
	//		exit.
	m_bDelete = TRUE;
	
	//	Save the URL/address
	m_csURL = pAddress;
	
	//	Save the frame performing the download.
	m_dwFrameID = dwFrameID;
	
	//	Create the file name.
	//	Attempt to save as much of the file name as possible.
	char *cpFullName = ::fe_URLtoLocalName(pAddress, NULL);
	char *cpTempDir = theApp.m_pTempDir;
	
	if(cpFullName != NULL && cpTempDir != NULL)	{
		char caNameBuffer[_MAX_PATH];
		
		::sprintf(caNameBuffer, "%s\\%s", cpTempDir, cpFullName);
		if(::_access(caNameBuffer, 0) == -1)	{
			m_csFileName = caNameBuffer;
		}
	}
	
	if(cpFullName != NULL)	{
		::free(cpFullName);
	}
	
	//	If our file name is still empty, then we must create it another
	//		way, by using only the extension and some random name.
	//  We leave as dot three, as we may be 32 bits yet talking to a 16
	//      bit DDE app.
	if(m_csFileName.IsEmpty())	{
		char caExt[_MAX_EXT];
		DWORD dwFlags = EXT_DOT_THREE;
		size_t stExt = 0;
		
		caExt[0] = '\0';
		stExt = EXT_Invent(caExt, sizeof(caExt), dwFlags, pAddress, NULL);
		
		{
			char* filename = WH_TempFileName(xpTemporary, "M", caExt);
			if (filename) {
				m_csFileName = filename;
				XP_FREE(filename);
			}
		}
	}
	    
	//	Okay, we've got the file name that will suite our needs.
	TRY	{
        //  Leave as shared readable for DDE apps looking into the file early.
		m_pStream = new CFile(m_csFileName, CFile::modeCreate |
            CFile::modeWrite | CFile::shareDenyWrite);
	}
	CATCH(CFileException, e)	{
		THROW_LAST();		
	}
	END_CATCH
}


extern "C"	{

 NET_StreamClass *dde_stream(int iFormatOut, void *vpDataObj,
	URL_Struct *pURL, MWContext *pContext)	{
//	Purpose:	Return the stream class for our DDE stream.
//	Arguments:	iFormatOut	The representation we are outputting.
//				vpDataObj	Our CDDEStreamData structure.
//				pURL		The URL we're loading.
//				pContext	The current context we're loading in.
//	Returns:	NET_StreamClass	A group of functions that will handle
//									the details of the download.
//	Comments:	Set up the data stream.
//				The stream will be lost on completion if the app failed
//					to unregister itself with us and exited.
//					Can't really handle.
//	Revision History:
//		01-06-95	created GAB
//		10-20-95	Hacked it up to use a secondary stream through CSaveCX if possible.
//

	NET_StreamClass *pRetval = NULL;

	//	Convert our data object.
	CDDEStreamData *pCData = (CDDEStreamData *)vpDataObj;
	
	//	Let's not be romantic about this, create our download data
	//		and then the stream class.
	TRY	{
		CDDEDownloadData *pDData =
			new CDDEDownloadData(pCData, pURL->address, FE_GetContextID(pContext));

		pRetval = NET_NewStream("PaperbackWriter",
			dde_StreamWrite,
			dde_StreamComplete,
			dde_StreamAbort,
			dde_StreamReady,
			(void *)pDData,
			pContext);
	}
	CATCH(CException, e)	{	//	Any exception will do
		pRetval = NULL;
	}
	END_CATCH

	if(pRetval != NULL)	{
		//	Attempt to go beyond, and get a secondary stream going
		//		to split this off into a new context.
		//	I know this says OLE, but it will work anyhow....
		NET_StreamClass *pShunt = CSaveCX::OleStreamObject(pRetval, pURL, pCData->m_csServerName);
		if(pShunt != NULL)	{
			pRetval = pShunt;
		}
	}
	
	return(pRetval);
}

int dde_StreamWrite(NET_StreamClass *stream, const char *cpWriteData,
	int32 lLength)	{
//	Purpose:	Write data out to the stream.
//	Arguments:	vpDataObj	Our download instance object.
//				cpWriteData	The data to write.
//				lLength		The amount of data to write.
//	Returns:	int		Return one of the infamous MK_* codes.
//	Comments:	Hacking for a return value, it would seem.
//	Revision History:
//		01-06-95	created GAB
//

	//	Obtain our data object.
	CDDEDownloadData *pDData = (CDDEDownloadData *)stream->data_object;	
	
	TRY	{
		ASSERT(lLength < 0x0000FFFFL);
		pDData->m_pStream->Write((const void *)cpWriteData,
			CASTINT(lLength));
	}
	CATCH(CException, e)	{
		//	Just return out of disk space, any exception.
		return(MK_DISK_FULL);
	}
	END_CATCH
		
	return(MK_DATA_LOADED);
}

void dde_StreamComplete(NET_StreamClass *stream)	{
//	Purpose:	Called when a stream comes to its successful completion.
//	Arguments:	vpDataObj	Our download instance object
//	Returns:	void
//	Comments:	Just unitialize mainly.
//	Revision History:
//		01-06-95	created
//

	//	Get our object.
	CDDEDownloadData *pDData = (CDDEDownloadData *)stream->data_object;	
	
	//	First off, close and free our file object.
	delete(pDData->m_pStream);
	pDData->m_pStream = NULL;
		
	//	Now, see what we're supposed to do with this closed file.
	if(pDData->m_pCData->m_bQueryViewer == TRUE)	{
		//	We're to move the file to a new locale, and then
		//		send whatever open message....
		CDDEWrapper::QueryViewer(pDData);
	}

	//	Okay, see if we're supposed to delete this file on exit.
	if(pDData->m_bDelete == TRUE)	{
		FE_DeleteFileOnExit(pDData->m_csFileName, pDData->m_csURL);
	}
		
	//	Switch on how to open.
	switch(pDData->m_pCData->m_dwHowHandle)	{
	case CDDEStreamData::m_OpenDocument:
		//	We're to use a platform specific open, this means
		//		Shell Open to windows.
		CDDEWrapper::OpenDocument(pDData);
		break;
	case CDDEStreamData::m_ViewDocFile:
		//	We're to simply tell the viewer what file to take a
		//		look at.
		//	Have our DDE member handle all the contingencies.
		CDDEWrapper::ViewDocFile(pDData);
		break;
	case CDDEStreamData::m_ViewDocData:
	default:
		//	Not supported.
		ASSERT(0);
		break;
	}
	
	//	We're basically done.
	//	Free off the download specific data.
	delete(pDData);
}

void dde_StreamAbort(NET_StreamClass *stream, int iStatus)	{
//	Purpose:	Abort a streaming download.
//	Arguments:	vpDataObj	Our download instance information.
//				iStatus		Our abort status.
//	Returns:	void
//	Comments:	Abort the stream, just as we close a connection.
//				We should send any streaming DDE clients a special
//					message.
//	Revision History:
//		01-06-95	created GAB
//

	//	Get our object.
	CDDEDownloadData *pDData = (CDDEDownloadData *)stream->data_object;	
	
	//	Handle just like a normal file close.
	dde_StreamComplete(stream);
}

unsigned int dde_StreamReady(NET_StreamClass *stream)	{
//	Purpose:	Tell the nework library how much data every one is
//					ready to receive.
//	Arguments:	vpDataObj	Our download instance informational fucntion.
//	Returns:	int	The amount of data the stream is ready for.
//	Comments:	On files, like what we are normally handline, simply
//					return the maximum amount.
//				On streaming clients, just ask them how much they can
//					now take.
//	Revision History:
//		01-06-95	created GAB
//

	//	Get our object.
	CDDEDownloadData *pDData = (CDDEDownloadData *)stream->data_object;	
	
	//	Return our maximum write value for local files.
	return((unsigned int)MAX_WRITE_READY);
}

};
