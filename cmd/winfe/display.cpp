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
//
// OK, so this module is horribly misnamed.  It contains all of the external
//   streams defined in the front end
//
#include "stdafx.h"

#include "display.h"

#include "helper.h"
#include "dialog.h"
#include "ngdwtrst.h"
#include "mainfrm.h"
#include "cxsave.h"
#include "errno.h"
#include "prefapi.h"
#include "extgen.h"
#include "viewerse.h"

extern "C" int MK_DISK_FULL;  // defined in allxpstr.h
extern "C" int MK_OUT_OF_MEMORY;  // defined in allxpstr.h

/*      Stream Object
**      ------------
*/                   

extern NET_StreamClass *OLE_ViewStream(int format_out, void *pDataObj, URL_Struct *urls,
              MWContext *pContext);


extern char *FE_FindFileExt(char * path);
extern void FE_LongNameToDosName(char* dest, char* source);

#define DEFAULT_BUFFER_SZ 30000

struct DataObject {
    /* generic */
    char      * address;
    char      * format_in;
    MWContext * context;
  int         format_out;

    /* file stuff */
    char * filename;
    FILE * fp;

    /* memory stuff */
    int32  sz;               /* size of buffer */
    char * start;         /* start of buffer */
    char * loc;           /* current posn in buffer */
    int32  content_length; /* size if known */
    int32  cur_loc;

    /* external object stuff */
    char * command;       /* command to execute */
    char * params;
    int how_handle;
};


/****************************************************************************
DISK STREAM DECLARATION
*****************************************************************************/

/*  Buffer write.  
**  ------------
*/
extern "C"  int 
disk_stream_write (NET_StreamClass *stream, CONST char* s, int32 len)
{

    int32 status;

    DataObject * data = (DataObject *)stream->data_object;	

    if(!data || !data->fp || !len || !s)
        return(0);

    status = XP_FileWrite(s, len, data->fp); 
    if(len != status) {
        if(errno == ENOSPC)
            return(MK_DISK_FULL);
        else
            return(-1);
    }

    data->cur_loc += len;

    float fPercent = 0.0f;

    if (data->content_length) 
        fPercent = (float)data->cur_loc / (float)data->content_length;
    if (data->cur_loc) 
        FE_SetProgressBarPercent(data->context, (int)(fPercent * 100));

    return((int) status);

}



/*  Free the object
**  ---------------
**
*/
extern "C"  void 
disk_stream_complete (NET_StreamClass *stream)
{
    DataObject * data = (DataObject *)stream->data_object;	

    if(!data)
        return;

    if(data->fp) {
        fclose(data->fp);
        data->fp = NULL;
    }

    FE_Progress(data->context, szLoadString(IDS_DOC_LOAD_COMPLETE));

    if(FEU_Execute(data->context, data->command, data->params)) {
        FE_Progress(data->context, szLoadString(IDS_SPAWNING_EXTERNAL_VIEWER));
    }

    if(data->address) {
        XP_FREE(data->address);
        data->address = NULL;
    }

    if(data->filename) {
        XP_FREE(data->filename);
        data->filename = NULL;
    }

    if(data->params) {
        XP_FREE(data->params);
        data->params = NULL;
    }

    delete data;
    return;
}

/*  End writing
*/
       
extern "C"  void 
disk_stream_abort (NET_StreamClass *stream, int status)
{
    DataObject * data = (DataObject *)stream->data_object;	

    char * error_string = "<title>Error</title><H1>Error</h1>Unable to load requested item.";

    if (data->fp)
        XP_FileWrite(error_string, XP_STRLEN(error_string), data->fp); 

    disk_stream_complete(stream);

    return;
}


//
// Always assume we can write no matter what type of stream we are dealing with
//
static unsigned int 
write_ready(NET_StreamClass *stream)
{	
  return((unsigned int) MAX_WRITE_READY);
}

static NET_StreamClass *
ExternalFileSave(int iFormatOut, URL_Struct *pUrl, MWContext *pContext)
{
	//
	// While we have a pointer to the original frame window query
	//   for a filename
	//
	char 			*pDestination;
	char 			*pSuggested = NULL;
	NET_StreamClass *pRetval = NULL;

	//	Prompt the user for a file name.
	//  Security rist to let path information in externally provided
	//      filename (content disposition, filename =)
	if (pUrl->content_name != NULL &&
		strstr(pUrl->content_name, "../") == NULL &&
		strstr(pUrl->content_name, "..\\") == NULL) {
		// Make a copy of the name, because that's what fe_URLtoLocalName() does
		// and the code below is going to XP_FREE pSuggested
		pSuggested = XP_STRDUP(pUrl->content_name);
	}
	if (!pSuggested)
		pSuggested = fe_URLtoLocalName(pUrl->address, pUrl->content_type);
#ifdef XP_WIN16
	char dosName[13];
	FE_LongNameToDosName(dosName, pSuggested);
	pDestination = wfe_GetSaveFileName(GetFrame(pContext)->GetFrameWnd()->m_hWnd, szLoadString(IDS_SAVE_AS), dosName, NULL);
#else
	pDestination = wfe_GetSaveFileName(GetFrame(pContext)->GetFrameWnd()->m_hWnd, szLoadString(IDS_SAVE_AS), pSuggested, NULL);
#endif
	if(pSuggested != NULL)	{
		XP_FREE(pSuggested);
	}
	if(pDestination == NULL)	{
		return(NULL);
	}

	//	We're going to be saving this stream.
	//	Check to see if we should attempt to save the file in the
	//		old fashion, or in the new external contexts.
	if(NET_IsSafeForNewContext(pUrl) != FALSE)	{
		//	Attempt to split it off into a new context.
		pRetval = CSaveCX::SaveUrlObject(pUrl, NULL, pDestination);

	} else {
		//	We need to continue to use the old method of saving.

		//	We have a destination file name.
		FILE *pSink = XP_FileOpen(pDestination, xpTemporary, "wb");
		if(pSink == NULL)	{
			FE_Alert(pContext, szLoadString(IDS_FAILED_CREATE_TEMP_FILE));
			XP_FREE(pDestination);
			return(NULL);
		}

		//	Create the data object that will be passed along down
		//		the stream.
		DataObject *pMe = new DataObject;
		if(!pMe)
			return(NULL);

		memset(pMe, 0, sizeof(DataObject));
		pMe->how_handle = HANDLE_SAVE;
		pMe->fp = pSink;
		pMe->context = pContext;
		pMe->format_out = iFormatOut;
		pMe->filename = pDestination;
		pMe->content_length = pUrl->content_length < 0 ? 0 : pUrl->content_length;
		pMe->cur_loc = 0;
		StrAllocCopy(pMe->address, pUrl->address);
		StrAllocCopy(pMe->format_in, pUrl->content_type);

		//	Progress.
		FE_SetProgressBarPercent(pContext, 0);

		//	Set the waiting mode???
		FE_EnableClicking(pContext);

		//	Create the stream.
		pRetval = NET_NewStream("ServeAndSave",
								disk_stream_write,
								disk_stream_complete,
								disk_stream_abort,
								write_ready,
								pMe,
								pContext);

		if(!pRetval)
			return(NULL);
	}

	return pRetval;
}

/*  Subclass-specific Methods
**  -------------------------
*/

/* this is the main converter for external viewers.
 * it returns the stream object as well
 * as a data object that it can reference 
 * internally to save the state of the document
 */
NET_StreamClass *external_viewer_disk_stream(int iFormatOut, void *pDataObj, URL_Struct *pUrl, MWContext *pContext)	
{
    ASSERT(pUrl);
    ASSERT(pUrl->address);

	//	Lookup the helper app, if one exists.
	//	If not found, create one on the fly.
	CNetscapeApp *pNetscape = (CNetscapeApp *)AfxGetApp();
	CHelperApp *pHelper;
	XP_Bool isNewHelper = FALSE;

	if(0 == pNetscape->m_HelperListByType.Lookup(pUrl->content_type, (CObject *&)pHelper))	{
		//	couldn't find one.
		//	create the new mime type.
		CString csText = pUrl->content_type;

		//	If there's no slash, just send the type as the file type
		//		(this usually only happens on server error, but we
		//		should still behave ourselves).
		int iSlash = csText.Find('/');
		if(iSlash != -1)	{
			// this mess splits the string into the stuff before the slash and
			//   the stuff after the slash
			pHelper = fe_AddNewFileFormatType(csText.Left(iSlash),
				csText.Right(csText.GetLength() - iSlash - 1));
			isNewHelper = TRUE;
		}
		else	{
			pHelper = fe_AddNewFileFormatType(csText, "");
			isNewHelper = TRUE;
		}
	}

	//	The helper app is now defined for the mime type in any case.
	//	See how it is to be handled.
	BOOL bExternal = FALSE;
	BOOL bSave = FALSE;
	BOOL bMoreInfo = FALSE;

	switch(pHelper->how_handle)	{
	case HANDLE_UNKNOWN:	{
		//	See what this is supposed to do via user input.
        CFrameWnd *pFrame = NULL;
        CFrameGlue *pGlue = GetFrame(pContext);
        if(pGlue)   {
            pFrame = pGlue->GetFrameWnd();
        }
        else    {
            pFrame = FEU_GetLastActiveFrame(MWContextAny, TRUE);
        }
		CUnknownTypeDlg dlgUnknown(pFrame, pUrl->content_type, pHelper);
		int iDlg = dlgUnknown.DoModal();
		if(iDlg == IDCANCEL)	{
			//	User hit cancel.  Abort the load.
			if (pHelper && pHelper->cd_item && isNewHelper) {
				if (XP_ListRemoveObject(cinfo_MasterListPointer(), pHelper->cd_item)) {
					if (pHelper->cd_item) {
						if (pHelper->cd_item->ci.type) {
							theApp.m_HelperListByType.RemoveKey(pHelper->cd_item->ci.type);
							XP_FREE( pHelper->cd_item->ci.type );
						}
						XP_FREE (pHelper->cd_item);
					}
					delete pHelper;
				}
			}
			return(NULL);
		}
		else if(iDlg == HANDLE_EXTERNAL)	{
            char buf[256];

			bExternal = TRUE;

			// We need to indicate that this is a user-defined MIME type. If we
			// don't, then we won't remember it the next time the Navigator is run
            sprintf(buf,"TYPE%d",theApp.m_iNumTypesInINIFile);
            theApp.m_iNumTypesInINIFile++;
            theApp.WriteProfileString("Viewers", buf, pUrl->content_type);
            pHelper->bNewType = FALSE;

		}
		else if(iDlg == HANDLE_SAVE)	{
			bSave = TRUE;
		}
		else if(iDlg == HANDLE_MOREINFO)	{
			bMoreInfo = TRUE;
		}
		break;
	}
	case HANDLE_EXTERNAL:	
	case HANDLE_BY_OLE: {
		bExternal = TRUE;
		break;
	}
	case HANDLE_SHELLEXECUTE:	{
		bExternal = TRUE;
		break;
	}
	case HANDLE_SAVE:	{
		bSave = TRUE;
		break;
	}
	default:	{
		//	Shouldn't ever be other than the above types at this
		//		point!
		ASSERT(0);
		return(NULL);
	}
	}

	//	We know that we are either saving or spawning an external
	//		viewer at this point.
	NET_StreamClass *pRetval = NULL;
	if (bSave == TRUE)	{
		return ExternalFileSave(iFormatOut, pUrl, pContext);
	} else if (bExternal == TRUE)	{
		//	Prompt the user for a file name.
		//  Security rist to let path information in externally provided
		//      filename (content disposition, filename =)
		// XXX This code could be cleaned up -- eliminate aFileName 
		// and just use what was allocated by WH_TempFileName.

		char aFileName[_MAX_PATH];
		char *pSuggestedName = NULL;
		BOOL bUseContentName = FALSE;
        if (pUrl->content_name != NULL &&
            strstr(pUrl->content_name, "../") == NULL &&
            strstr(pUrl->content_name, "..\\") == NULL) {
			bUseContentName = TRUE;
		}
		else {
			pSuggestedName = fe_URLtoLocalName(pUrl->address, pUrl->content_type);
		}
		char *pDestination;

		ASSERT(pNetscape->m_pTempDir);
		if(pNetscape->m_pTempDir != NULL && pSuggestedName != NULL)	{
			sprintf(aFileName, "%s\\%s", pNetscape->m_pTempDir, pSuggestedName);
			XP_FREE(pSuggestedName);
			pSuggestedName = NULL;
			pDestination = aFileName;
		}
		else	{
            char aExt[_MAX_EXT];
            size_t stExt = 0;
            DWORD dwFlags = 0;
            const char *pName = pUrl->address;
            
            if(bUseContentName) {
                pName = pUrl->content_name;
            }
#ifdef XP_WIN16
            dwFlags |= EXT_DOT_THREE;
#endif
            aExt[0] = '\0';
            stExt = EXT_Invent(aExt, sizeof(aExt), dwFlags, pName, pUrl->content_type);
            char *pTemp = WH_TempFileName(xpTemporary, "M", aExt);
            if(pTemp) {
                strcpy(aFileName, pTemp);
                XP_FREE(pTemp);
                pTemp = NULL;
            }
            else {
                aFileName[0] = '\0';
            }
		}
		pDestination = aFileName;


		//	Figure out the application that we'll be spawning.
		//	Strip off odd things at the right hand side.
		CString csCommand;
        if(pHelper->how_handle == HANDLE_EXTERNAL)  {
            csCommand = pHelper->csCmd;
		    int iStrip = csCommand.ReverseFind('%');
		    if(iStrip > 0)	{
			    csCommand = csCommand.Left(iStrip - 1);
		    }
        }

		//	See if it's actually OK to spawn this application.
        CString csSpawn = csCommand;
        BOOL bShellExecute = FALSE;
        if(pHelper->how_handle == HANDLE_SHELLEXECUTE ||
			pHelper->how_handle == HANDLE_BY_OLE) {
            //  Shell execute type, figure out the exe.
            char aExe[_MAX_PATH];
            memset(aExe, 0, sizeof(aExe));
            if(FEU_FindExecutable(pDestination, aExe, FALSE)) {
                csSpawn = aExe;
                if(pHelper->how_handle == HANDLE_SHELLEXECUTE) {
                    bShellExecute = TRUE;
                }
            }
            else     {
                csSpawn.Empty();
            }
        }

		// See whether the user wants to be prompted before we open the file
		if (pContext->type != MWContextPrint && theApp.m_pSpawn->PromptBeforeOpening((LPCSTR)csSpawn)) {
			BOOL	bFree = FALSE;
			LPCSTR	lpszFilename = NULL;

			if (pUrl->content_name != NULL &&
				strstr(pUrl->content_name, "../") == NULL &&
				strstr(pUrl->content_name, "..\\") == NULL) {
				lpszFilename = pUrl->content_name;
			}

			if (!lpszFilename) {
				lpszFilename = fe_URLtoLocalName(pUrl->address, pUrl->content_type);
				bFree = TRUE;
			}
			char* docExt[1];
			const char * ptr1 = lpszFilename;
			int type = NET_URL_Type(pUrl->address);
			BOOL canHandleOLE = FALSE;

			if ((type != MAILBOX_TYPE_URL) && (type !=NEWS_TYPE_URL) && (type != IMAP_TYPE_URL) ) {
				docExt[0] = FE_FindFileExt((char*)ptr1);
				if (docExt[0])
					canHandleOLE = fe_CanHandleByOLE(docExt, 1);
			}
			CLaunchHelper	dlg(lpszFilename, (LPCSTR)csSpawn, canHandleOLE, GetFrame(pContext)->GetFrameWnd());
	
			if (bFree)
				XP_FREE((LPVOID)lpszFilename);

			// Initialize the dialog to some defaults.
			dlg.m_bAlwaysAsk = TRUE;
			//dlg.m_nAction = HELPER_SAVE_TO_DISK; //Old statement CRN_MIME
			dlg.m_nAction = (pHelper->how_handle == HANDLE_SHELLEXECUTE) ? HELPER_OPEN_IT : HELPER_SAVE_TO_DISK; //New Statement. Set m_nAction based on pHelper->how_handle... CRN_MIME
			dlg.m_bHandleByOLE = fe_IsHandleByOLE(pUrl->content_type);
	
			// Ask the user
			if (dlg.DoModal() == IDCANCEL)
				return NULL;
	
			// See if they no longer want to be asked
			if (!dlg.m_bAlwaysAsk) {
				if (dlg.m_nAction == HELPER_SAVE_TO_DISK) {
					// User wants to just save to disk
					pHelper->how_handle = HANDLE_SAVE;
					pHelper->csCmd = MIME_SAVE;
					pHelper->bChanged = TRUE;
				
				} else {
					ASSERT(dlg.m_nAction == HELPER_OPEN_IT);
					theApp.m_pSpawn->SetPromptBeforeOpening((LPCSTR)csSpawn, FALSE);
				}
			}

			// Check whether the user wants to launch the application or save it
			// do disk
			if (dlg.m_nAction == HELPER_SAVE_TO_DISK)
				return ExternalFileSave(iFormatOut, pUrl, pContext);
			else { // open it case.
				// user want to handle this by OLE.
				if (dlg.m_bHandleByOLE) {
					fe_SetHandleByOLE(pUrl->content_type, pHelper, TRUE);
				}
				// Since mail and new will not be able launch using OLE inplace server, so we should not try to change helper app
				// how_handle here.
				else if (pHelper->how_handle == HANDLE_BY_OLE) {
					fe_SetHandleByOLE(pUrl->content_type, pHelper, FALSE);
				}
			}
		}
		// MWH -- see could we handle this via OLE.
		if ((iFormatOut == FO_PRESENT || iFormatOut == FO_PRINT)  &&
			(pHelper->how_handle == HANDLE_BY_OLE) &&
				FE_FileType(pUrl->address, pUrl->content_type, pUrl->content_encoding)) {

			// can be handle by OLE.
				return OLE_ViewStream(iFormatOut, pDataObj, pUrl,pContext);
		}

		//	It's OK to spawn this application.
		//	Attempt to split it off into a seperate context.
		if(bShellExecute) {
		    pRetval = CSaveCX::ViewUrlObject(pUrl, NULL);
		}
		else {
		    pRetval = CSaveCX::ViewUrlObject(pUrl, csSpawn);
		}
		if(pRetval != NULL)	{
			return(pRetval);
		}
		//	Couldn't split off into a new context.
		//	Handle as was handled before.

		//	We have a destination file name.
		FILE *pSink = XP_FileOpen(pDestination, xpTemporary, "wb");
		if(pSink == NULL)	{
			FE_Alert(pContext, szLoadString(IDS_FAILED_CREATE_TEMP_FILE));
			XP_FREE(pDestination);
			return(NULL);
		}

		//	Create the data object that will be passed along down
		//		the stream.
		DataObject *pMe = new DataObject;
        if(!pMe)
            return(NULL);

		memset(pMe, 0, sizeof(DataObject));
		pMe->how_handle = pHelper->how_handle;
		pMe->fp = pSink;
		pMe->context = pContext;
		pMe->format_out = iFormatOut;
		pMe->filename = pDestination;
		pMe->content_length = pUrl->content_length < 0 ? 0 : pUrl->content_length;
		pMe->cur_loc = 0;
		StrAllocCopy(pMe->address, pUrl->address);
		StrAllocCopy(pMe->format_in, pUrl->content_type);

		//	The spawn command.
        if(pMe->how_handle == HANDLE_EXTERNAL)  {
            pMe->params = XP_STRDUP(pDestination);
        }
        else if(pMe->how_handle == HANDLE_SHELLEXECUTE)    {
		    csCommand += pDestination;
        }
        else if(pMe->how_handle == HANDLE_BY_OLE)    {
		    csCommand += pDestination;
        }
		pMe->command = XP_STRDUP(csCommand);

		//	Progress.
		FE_SetProgressBarPercent(pContext, 0);

		//	Delete the file on exit.
		FE_DeleteFileOnExit(pDestination, pUrl->address);

		//	Set the waiting mode???
		FE_EnableClicking(pContext);

		//	Create the stream.
		pRetval = NET_NewStream("ServeAndView",
                    			disk_stream_write,
                    			disk_stream_complete,
                    			disk_stream_abort,
                    			write_ready,
                    			pMe,
                    			pContext);

	}
	if(bMoreInfo == TRUE)	{
		char * url = NULL;
		PREF_CopyConfigString("internal_url.more_info_plugin.url",&url);
		if (url) {
			CString csUrlAddress = url;
			csUrlAddress += "?";
			csUrlAddress += pUrl->content_type;
			(ABSTRACTCX(pContext))->NormalGetUrl(csUrlAddress, pUrl->address, csUrlAddress);
			XP_FREE(url);
		}
    }

	//	Return the stream that was created.
	return(pRetval);
}

/****************************************************************************
MEMORY STREAM DECLARATION
*****************************************************************************/

/*  Buffer write.  
**  ------------
*/
extern "C"  int 
memory_stream_write (NET_StreamClass *stream, const char* s, int32 l)
{
    DataObject * data = (DataObject *)stream->data_object;	

    ASSERT(data);
    ASSERT(s);

    if(!data || !data->start || !data->loc)
        return(MK_OUT_OF_MEMORY);

    /* check the sizes - make bigger and recopy if needed */
    if(((unsigned long)(data->loc - data->start))  + l > (unsigned long)data->sz) {
    char * buffer;
        int oldcontents = data->loc - data->start;

        data->sz += DEFAULT_BUFFER_SZ;
        buffer = (char *) XP_REALLOC(data->start, data->sz);
        if(!buffer) {
          XP_FREE(data->start);
          data->start = NULL;
          data->loc = NULL;
          return(MK_OUT_OF_MEMORY);
        }
          
        data->start = buffer;
        data->loc = data->start + oldcontents;     
    }
    
    /* copy the stuff over and update the pointer */
    XP_BCOPY(s, data->loc, l);
    data->loc += l;

    data->cur_loc += l;

    float fPercent = 0.0f;

    if (data->content_length) 
        fPercent = (float)data->cur_loc / (float)data->content_length;
    if (data->cur_loc) 
        FE_SetProgressBarPercent(data->context, (int)(fPercent * 100));

    return((int) l);
}

/* 
 *  Do something interesting with the object and free it
 *  ----------------------------------------------------
 */
extern "C"  void 
memory_stream_complete (NET_StreamClass *stream)
{
    DataObject * data = (DataObject *)stream->data_object;	

    if(data->address)
        XP_FREE(data->address);

	// Make sure we are text and NULL terminated
	if(data->loc)
		*(data->loc) = '\0';
	
	if(data->start) { 
		XP_FREE(data->start);
		data->start = NULL;
	}
	    
    if(data->params) {
        XP_FREE(data->params);
        data->params = NULL;
    }

	delete data;
	return;
}

/*  End writing
*/

extern "C"  void 
memory_stream_abort (NET_StreamClass *stream, int status)
{	
    memory_stream_complete(stream);
}


/*  Subclass-specific Methods
**  -------------------------
*/

/* this is the main converter.
 * it returns the stream object as well
 * as a data object that it can reference 
 * internally to save the state of the document
 */

NET_StreamClass* 
memory_stream (int     format_out,
           void       *data_object,
           URL_Struct *URL_s,
           MWContext  *context)
{
    DataObject* me;
    NET_StreamClass* stream;

    //
    // If we are a view source stream and the user wants an external view source
    //   application send it there instead
    //
    const char * html = theApp.m_pHTML;
    if((format_out == FO_VIEW_SOURCE) && html && * html)	{
        return (view_source_disk_stream(format_out, data_object, URL_s, context));
	}
	else if(format_out & FO_VIEW_SOURCE)	{
		//	Otherwise, use our colored syntax HTML viewer for view source.
		return(INTL_ConvCharCode(format_out, URL_s->content_type, URL_s, context));
	}

    me = new DataObject;
    if(!me)
        return NULL;
    memset(me, 0, sizeof(DataObject));

    stream = NET_NewStream("MemoryWriter",
                            memory_stream_write,
                            memory_stream_complete,
                            memory_stream_abort,
                            write_ready,
                            me,
                            context);

    if(stream == NULL) {
		delete me;
		return NULL;
	}

    me->context = context;
    me->address = me->filename = me->format_in = NULL;
    me->format_out = format_out;
    StrAllocCopy(me->address, URL_s->address);
    StrAllocCopy(me->format_in, URL_s->content_type);

    /* set up the buffer fields */
    me->sz = DEFAULT_BUFFER_SZ;
    me->start = me->loc = (char *)XP_ALLOC(sizeof(char) * me->sz);

    if(!me->start) {
        delete me;
        XP_FREE(stream);
        return NULL;
    }

    if (URL_s->content_length > 0)
        me->content_length = URL_s->content_length;
    else 
        me->content_length = 0;

    me->cur_loc =0;    
    FE_SetProgressBarPercent(context, 0);

    return stream;
}


/* this is the main converter for saving files
 * it returns the stream object as well
 * as a data object that it can reference 
 * internally to save the state of the document
 */
PUBLIC NET_StreamClass*
view_source_disk_stream(int         format_out,
           void       *lous_new_stuff,
           URL_Struct *URL_s,
           MWContext  * context)
{
    DataObject* me;
    NET_StreamClass* stream;
    FILE * fp = NULL;
    BOOL bPromptUserForName =FALSE;
    BOOL bDeleteFile = TRUE;

    ASSERT(context);

    me = new DataObject;
    if (me == NULL) 
        return NULL;
    memset(me, 0, sizeof(DataObject));
                                                                            
    stream = NET_NewStream("FileWriter", 
                            disk_stream_write,        
                            disk_stream_complete,
                            disk_stream_abort,
                            write_ready,
                            me,
                            context);

    if(stream == NULL) {
        delete me;
        return NULL;
    }
    
    // user selected save to disk
    me->how_handle = HANDLE_EXTERNAL; 

    //
    // Get a filename with an extension cuz stupid old notepad assumes one
    //
    char* fname = WH_TempFileName(xpTemporary, NULL, ".txt");
    if(!fname)
        return(NULL);
    
    //
    // Open the file in text mode so that will get return translation
    // Why in the world are we doing this AnsiToOemBuff stuff???
    //  
#ifdef XP_WIN32
    fp = fopen(fname, "wt");
#else
	char oemBuff[254];
	AnsiToOemBuff(fname, oemBuff, strlen(fname)+1);
    fp = fopen(oemBuff, "wt");
#endif
    if(!fp) {
        XP_FREE(fname);
        return NULL;
    }

    // delete the file when we are done
    FE_DeleteFileOnExit(fname, URL_s->address);

    int len = theApp.m_pHTML.GetLength() + XP_STRLEN(fname) + 10;
    me->command = (char *) XP_ALLOC(len);
    if(!me->command) {
        fclose(fp);
        XP_FREE(fname);
        return NULL;
    }
    
    sprintf(me->command, "%s %s", theApp.m_pHTML, fname);

    me->fp = fp;
    me->address = me->filename = me->format_in = NULL;
    me->context = context;
    me->format_out = format_out;
    me->filename = fname;
    StrAllocCopy(me->address, URL_s->address);
    StrAllocCopy(me->format_in, URL_s->content_type);

    FE_EnableClicking(context);

    if (URL_s->content_length > 0)
        me->content_length = URL_s->content_length;
    else 
        me->content_length = 0;
    
    FE_SetProgressBarPercent(context, 0);
  
    return stream;

}
