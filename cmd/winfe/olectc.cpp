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

#include "olectc.h"

#include "presentm.h"
#include "oleview1.h"
#include "cxsave.h"
#include "winproto.h"

extern "C" int MK_OUT_OF_MEMORY;   // defined in allxpstr.h


COLEStreamData::COLEStreamData(const char *pServer, const char *pMimeType) : CStreamData(CStreamData::m_OLE)  {
//	Purpose:    Construct the data stream object.
//	Arguments:  pServer The registry name of the automation server that we should contact in our data stream.
//              pMimeType   The Mime type of the stream.
//	Returns:    none
//	Comments:   Note that we initialize the base class to know that we are an OLE viewer.

    //  Just copy over the relevant members.
    m_csServerName = pServer;
    m_csMimeType = pMimeType;
}

COLEDownloadData::COLEDownloadData(COLEStreamData *pCData, const char *pAddress)  {
//	Purpose:    Create an instance of the download data.
//	Arguments:  pCData  A global object representing the OLE registered server.
//              pAddress    The URL that we are about to handle.
//	Returns:    none
//	Comments:   Download instance specific member.

    //  Assign over our data.
    m_pCData = pCData;
    m_pBuffer = NULL;
    m_bReadyCalled = FALSE;

    //  Attach ourselves to the OLE Automation server.
    //  If an exception is thrown, it's handled elsewhere.
    TRACE("%s:Initialize(%s, %s)\n", (const char *)m_pCData->m_csServerName, (const char *)m_pCData->m_csMimeType, pAddress);
    m_Viewer.CreateDispatch(m_pCData->m_csServerName);
    if(0 == m_Viewer.Initialize(m_pCData->m_csMimeType, pAddress)) {
        AfxThrowNotSupportedException();
    }

    //  Attempt to allocate a small buffer to start out with.
    //  This may grow later, we don't really care.
    if(NULL == (m_pBuffer = SysAllocStringLen(NULL, m_InitialBufferSize))) {
        AfxThrowMemoryException();
    }
}

COLEDownloadData::~COLEDownloadData()   {
    //  Use to free off our buffer if it exists.
    if(m_pBuffer != NULL)   {
        SysFreeString(m_pBuffer);
    }
}

extern "C"  {

NET_StreamClass *ole_stream(int iFormatOut, void *vpDataObj, URL_Struct *pURL, MWContext *pContext) {
//	Purpose:    Return the stream class for our OLE automated stream.
//	Arguments:  iFormatOut  The representation we are outputting (should be FO_PRESENT only for now)
//              vpDataObj   Our COLEStreamData structure.
//              pURL        The URL we're loading.
//              pContext    The current context that we're loading in.
//	Returns:    NET_StreamClass A group of functions that will handle the intimate details
//                  of the download.
//	Comments:   Set up the data stream.
//              Initiate the conversation with the automation server.
//              The stream will be lost if unable to establish the connection.  Can't really handle well.

    //  Convert our data object.
    COLEStreamData *pCData = (COLEStreamData *)vpDataObj;
    COLEDownloadData *pOData = NULL;
    //  Create the stream data.
    TRY {
        pOData = new COLEDownloadData(pCData, pURL->address);

		NET_StreamClass *pOleStream = NET_NewStream("Speghetti(SP)",
            ole_StreamWrite,
            ole_StreamComplete,
            ole_StreamAbort,
            ole_StreamReady,
            (void *)pOData,
            pContext);

		//	See if we can't get this to load in a seperate context.
		//	It will correctly call the stream members, and also reset
		//		the context if able.
		NET_StreamClass *pRetval =
			CSaveCX::OleStreamObject(pOleStream, pURL, pCData->m_csServerName);
		if(pRetval == NULL)	{
			//	Couldn't switch
			pRetval = pOleStream;
		}

		return(pRetval);
    }
    CATCH(CException, e)    {   //  Any exception will do

        //  Tell the user that we were unable to complete the operation, ask them if they would like to unregister the OLE viewer.
        TRACE("Couldn't connect to the OLE automation server\n");
        CString csMessage = szLoadString(IDS_OLE_CANTUSE_VIEWER);
        csMessage += pCData->m_csServerName;
        csMessage += szLoadString(IDS_OLE_CANTUSE_VIEWER2);

        if(IDNO == AfxMessageBox(csMessage, MB_YESNO))   {
            //  They don't want to use the viewer in the future.
            //  Unregister it; assume FO_PRESENT.
            WPM_UnRegisterContentTypeConverter(pCData->m_csServerName, pCData->m_csMimeType, FO_PRESENT);

            //  Remove the registration from the INI file also.
            theApp.WriteProfileString("Automation Viewers", pCData->m_csMimeType, NULL);

            //  We know it's registered, and we know it's our current data, so remove it.
            delete(pCData);
        }
        if(pOData != NULL)  {
            delete(pOData);
        }

        return(NULL);
    }
    END_CATCH

    return(NULL);
}

int ole_StreamWrite(NET_StreamClass *stream, const char *cpWriteData, int32 lLength)    {
//	Purpose:    Write data to our automated object.
//	Arguments:  vpDataObject    The COLEDownloadObject for the load.
//              cpWriteData     The data to write.
//              lLength         The length of the string we're writing.
//	Returns:    int MK_DATA_LOADED always.
//	Comments:   Used to write data to the streaming viewer.
//              If the viewer doesn't want the data, it should report the error status in Ready.

	void *vpDataObj=stream->data_object;	
    //  Obtain the object.
    COLEDownloadData *pOData = (COLEDownloadData *)vpDataObj;

    //  Check to see if the netlib didn't check is_write_ready
    if(pOData->m_bReadyCalled == FALSE) {
        TRACE("Please call Ready first.  This is a serious hack.\n");

        //  Since the netlib is being harsh to us, we have to do some special handling here.
        //  We're going to do a tight loop, until the other viewer has gotten all the data
        //      that we just got handed, and then we'll return as normal.
        //  Enough OLE messages will be generated to keep the application running....
        long lReady = 0;
        long lOffset = 0;
        while(1)    {
            lReady = ole_StreamReady(stream);
            if(lReady == 0) {
                continue;
            }
            else if(lReady < 0)   {
                //  Hm, they said error.
                pOData->m_bReadyCalled = FALSE;
                return(CASTINT(lReady));
            }

            //  Okay, see if the ready amount is the amount we can send.
            if(lReady + lOffset >= lLength)  {
                lReady = lLength - lOffset;
                if(lReady == 0) {
                    pOData->m_bReadyCalled = FALSE;
                    return(MK_DATA_LOADED);
                }
            }

            //  Send the data.
            ole_StreamWrite(stream, cpWriteData + lOffset, lReady);

            //  Increment our offset into the buffer.
            lOffset += lReady;
        }
    }

    //  Clear this out for next time.
    pOData->m_bReadyCalled = FALSE;

    //  If the length is greater than our buffer, then it's time to resize.
    if((int32)SysStringLen(pOData->m_pBuffer) < lLength)    {
        if(FALSE == SysReAllocStringLen(&(pOData->m_pBuffer), NULL, CASTUINT(lLength)))   {
            //  Couldn't do it, return an error.
            return(MK_OUT_OF_MEMORY);
        }
    }

    //  Copy over the bytes.
    memcpy(pOData->m_pBuffer, cpWriteData, CASTSIZE_T(lLength));

    //  Write it to the viewer.
    pOData->m_Viewer.Write(&(pOData->m_pBuffer), lLength);
    return(MK_DATA_LOADED);
}

void ole_StreamComplete(NET_StreamClass *stream)    {
//	Purpose:    Normally complete the stream.
//	Arguments:  vpDataObj   The COLEDownloadData object, which we will simply destroy.
//	Returns:    void
//	Comments:   Return a normal status to the viewer.

    //  Obtain our download data.
    COLEDownloadData *pOData = (COLEDownloadData *)stream->data_object;	

    //  Close, with no error.
    pOData->m_Viewer.Close(0);

    //  Delete the object.
    delete(pOData);
}

void ole_StreamAbort(NET_StreamClass *stream, int iStatus) {
//	Purpose:    Abort the stream for miscellaneous reasons.
//	Arguments:  vpDataObj   The COLEDownloadData object, we'll destroy this.
//              iStatus The error status, which we pay no attention to.
//	Returns:    void
//	Comments:   Return an error status to the viewer.

    //  Obtain our download data.
    COLEDownloadData *pOData = (COLEDownloadData *)stream->data_object;	

    //  Close, with error.
    pOData->m_Viewer.Close(-1);

    //  Delete the object.
    delete(pOData);
}

unsigned int ole_StreamReady(NET_StreamClass *stream)   {
//	Purpose:    Return the number of bytes which we are ready to have written to us.
//	Arguments:  vpDataObj   The COLEDownloadData which handles the download.
//	Returns:    unsigned int    The number of bytes that we're ready for.
//	Comments:   We really simply ask the viewer how much they're ready to handle.

    //  Obtain our download data.
    COLEDownloadData *pOData = (COLEDownloadData *)stream->data_object;	

    //  Mark that the netlib actually called us.
    pOData->m_bReadyCalled = TRUE;    

    //  Return the amount that the viewer reports.
    return(CASTUINT(pOData->m_Viewer.Ready()));
}

};
