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
// Editor save stuff. LWT 06/01/95
//
// Added all ITapeFileSystem stuff, hardts 10/96

#ifdef EDITOR

#include "editor.h"
#include "streams.h"
#include "shist.h"
#include "xpgetstr.h"
#include "net.h"
#include "mime.h"
#include "prefapi.h"

// Other error values are created from ALLXPSTR.RC during compiling
//extern "C" int MK_UNABLE_TO_LOCATE_FILE;
//extern "C" int MK_MALFORMED_URL_ERROR;

//-----------------------------------------------------------------------------
// CEditImageLoader
//-----------------------------------------------------------------------------
CEditImageLoader::CEditImageLoader( CEditBuffer *pBuffer, EDT_ImageData *pImageData, 
                XP_Bool bReplaceImage ): 
        m_pBuffer( pBuffer ),
        m_pImageData( edt_DupImageData( pImageData ) ),
        m_bReplaceImage( bReplaceImage )
        {
    char *pStr;
    if( !pBuffer->m_pContext->is_new_document ){
        intn retVal = NET_MakeRelativeURL( LO_GetBaseURL( m_pBuffer->m_pContext ), 
                    m_pImageData->pSrc, 
                    &pStr );
        
        if( retVal != NET_URL_FAIL ){
            XP_FREE( m_pImageData->pSrc );
            m_pImageData->pSrc = pStr;
        }

        retVal = NET_MakeRelativeURL( LO_GetBaseURL( m_pBuffer->m_pContext ), 
                    m_pImageData->pLowSrc, 
                    &pStr );
        
        if( retVal != NET_URL_FAIL ){
            XP_FREE( m_pImageData->pLowSrc );
            m_pImageData->pLowSrc = pStr;
        }

    }
}

CEditImageLoader::~CEditImageLoader(){
    m_pBuffer->m_pLoadingImage = 0;
    FE_ImageLoadDialogDestroy( m_pBuffer->m_pContext );
    FE_EnableClicking( m_pBuffer->m_pContext );
    edt_FreeImageData( m_pImageData );
}

void CEditImageLoader::LoadImage(){
    char *pStr;
    XP_ObserverList image_obs_list; // List of image observers.

    m_pLoImage = LO_NewImageElement( m_pBuffer->m_pContext );
    
    // convert the URL to an absolute path
    pStr = NET_MakeAbsoluteURL( LO_GetBaseURL( m_pBuffer->m_pContext ), 
                m_pImageData->pSrc );
    m_pLoImage->image_url = PA_strdup( pStr );
    XP_FREE( pStr );

    // convert the URL to an absolute path
    pStr = NET_MakeAbsoluteURL( LO_GetBaseURL( m_pBuffer->m_pContext ), 
                m_pImageData->pLowSrc );
    m_pLoImage->lowres_image_url = PA_strdup( pStr );
    XP_FREE( pStr );

    // tell layout to  pass the size info back to us.
    m_pLoImage->ele_id = ED_IMAGE_LOAD_HACK_ID;  
    FE_ImageLoadDialog( m_pBuffer->m_pContext );

    image_obs_list = lo_NewImageObserverList(m_pBuffer->m_pContext,
                                             m_pLoImage);
    if (!image_obs_list) {
        XP_ASSERT(FALSE);        // Out of memory.
        return;
    }
    lo_GetImage(m_pBuffer->m_pContext, m_pBuffer->m_pContext->img_cx,
                m_pLoImage, image_obs_list, NET_DONT_RELOAD);
}


// The image library is telling us the size of the image.  Actually do the 
//  insert now.
void CEditImageLoader::SetImageInfo(int32 ele_id, int32 width, int32 height){
    if( m_pImageData->iHeight == 0 || m_pImageData->iWidth == 0 ){
        m_pImageData->iHeight = height;
        m_pImageData->iWidth = width;    
        m_pImageData->iOriginalHeight = height;
        m_pImageData->iOriginalWidth = width;    
    }
    if( m_bReplaceImage ){
        // whack this in here...
        m_pBuffer->m_pCurrent->Image()->SetImageData( m_pImageData );
        // we could be doing a better job here.  We probably repaint too much...
        CEditElement *pContainer = m_pBuffer->m_pCurrent->FindContainer();
        m_pBuffer->Relayout( pContainer, 0, pContainer->GetLastMostChild(), 
                    RELAYOUT_NOCARET );
        m_pBuffer->SelectCurrentElement();
    }
    else {
        m_pBuffer->InsertImage( m_pImageData );
    }
    delete this;
}

//-----------------------------------------------------------------------------
// CEditSaveObject
//-----------------------------------------------------------------------------
#if defined(XP_OS2)
extern "C"
#else
PRIVATE
#endif
unsigned int edt_file_save_stream_write_ready( NET_StreamClass *stream ){
    return MAX_WRITE_READY;
}

#if defined(XP_OS2)
extern "C"
#else
PRIVATE
#endif
int edt_FileSaveStreamWrite (NET_StreamClass *stream, const char *block, int32 length) {
    CFileSaveObject *pSaver = (CFileSaveObject*)stream->data_object;	
    return pSaver->NetStreamWrite( block, length );
}

#if defined(XP_OS2)
extern "C"
#else
PRIVATE
#endif
void edt_file_save_stream_complete( NET_StreamClass *stream ){	
}

#if defined(XP_OS2)
extern "C"
#else
PRIVATE
#endif
void edt_file_save_stream_abort (NET_StreamClass *stream, int status) {	
}


#if defined(XP_OS2)
extern "C"
#else
PRIVATE
#endif
void edt_UrlExit( URL_Struct *pURL, int status, MWContext *context )
{
    // hardts, changed from CEditSaveObject to CFileSaveObject
    ((CFileSaveObject*)(pURL->fe_data))->NetFetchDone(pURL, status, context );

    NET_FreeURLStruct(pURL);
}

#if defined(XP_OS2)
extern "C"
#else
PRIVATE
#endif
NET_StreamClass * edt_MakeFileSaveStream (int format_out, void *closure,
    URL_Struct *url, MWContext *context ) {

    NET_StreamClass *stream;
    CFileSaveObject *pSaver = (CFileSaveObject*)url->fe_data;
    
    // OpenOutputFile will return 0 if OK
    if( !pSaver || pSaver->OpenOutputFile() != 0  ){
        return (NULL);
    }
 
 	// ### mwelch Relay the information stored in this URL. We do it here instead of
 	//            in the url exit routine because edt_MakeFileSaveStream could
 	//            have been invoked by an inferior URL which represents a parsed
 	//            piece of a mail or news message. If this URL is what was originally
 	//            invoked, the correct mime type should still be present here.	
	if (url)
 		pSaver->CopyURLInfo(url);
   
    TRACEMSG(("Setting up attachment stream. Have URL: %s\n", url->address));

    stream = XP_NEW (NET_StreamClass);
    if (stream == NULL) 
        return (NULL);

    XP_MEMSET (stream, 0, sizeof (NET_StreamClass));

    stream->name           = "editor file save";
    stream->complete       = edt_file_save_stream_complete;
    stream->abort          = edt_file_save_stream_abort;
    stream->put_block      = edt_FileSaveStreamWrite;
    stream->is_write_ready = edt_file_save_stream_write_ready;
    stream->data_object    = url->fe_data;
    stream->window_id      = context;

    return stream;
}

static XP_Bool bNetRegistered = FALSE;

#ifdef MOZ_MAIL_NEWS
extern "C" 
{
	NET_StreamClass *MIME_MessageConverter (int format_out, void *closure,
                                               URL_Struct *url,
                                               MWContext *context);
}
#endif

//-----------------------------------------------------------------------------
// CFileSaveObject
//-----------------------------------------------------------------------------

CFileSaveObject::CFileSaveObject( MWContext *pContext, char *pSrcURL, 
                                  ITapeFileSystem *tapeFS, XP_Bool bAutoSave,
                                  CEditSaveToTempData *pSaveToTempData) : 
        m_pContext( pContext ) ,
        m_bOverwriteAll( FALSE ),
        m_bDontOverwriteAll( FALSE ),
        m_bDontOverwrite( FALSE ),
        m_iCurFile(0),
        m_pOutStream(0),
        m_bOpenOutputHandledError(FALSE),
        m_tapeFS(tapeFS),
        m_status( ED_ERROR_NONE ),
        m_bFromAutoSave(bAutoSave),
        m_pSaveToTempData(pSaveToTempData)
{
    if( !bNetRegistered ){
#ifdef MOZ_MAIL_NEWS
    	// ### mwelch Need to pass mail/news messages through
    	//     MIME_MessageConverter since we're likely to be
    	//     extracting a part of the message.
    	NET_RegisterContentTypeConverter( MESSAGE_RFC822, FO_EDT_SAVE_IMAGE,
    										NULL, MIME_MessageConverter);
    	NET_RegisterContentTypeConverter( MESSAGE_NEWS, FO_EDT_SAVE_IMAGE,
    										NULL, MIME_MessageConverter);
#endif

    	// For everything else, just write to the tapefs.
        NET_RegisterContentTypeConverter( "*", FO_EDT_SAVE_IMAGE,
                        NULL, edt_MakeFileSaveStream );
//// Why is this duplicated? hardts
#if defined(XP_WIN) || defined(XP_OS2)
        NET_RegisterContentTypeConverter( "*", FO_EDT_SAVE_IMAGE,
                        NULL, edt_MakeFileSaveStream );
#else
NET_RegisterContentTypeConverter( "*", FO_EDT_SAVE_IMAGE,
				NULL, edt_MakeFileSaveStream );
#endif /* XP_WIN */
        bNetRegistered = TRUE;
    }

    // MUST DISABLE WINDOW INTERACTION HERE!
    // (It will always be cleared in the GetURLExitRoutine)
    // (This is normally set TRUE by front end before calling NET_GetURL)
    m_pContext->waitingMode = TRUE;

    // Set a flag so we know when we are saving a file
    // (also set in CEditBuffer::SaveFile() - may not be needed here, but its safe to set again)
    m_pContext->edit_saving_url = TRUE;

    // set base URL for the tape file system.
    char *pBaseURL = edt_GetDocRelativeBaseURL(m_pContext);
    if (pBaseURL) {
      m_tapeFS->SetSourceBaseURL(pBaseURL);
      XP_FREE(pBaseURL);
    }
}

CFileSaveObject::~CFileSaveObject(){
    // Success message for publishing.  Moved from WinFE to back end.
    if (m_tapeFS->GetType() == ITapeFileSystem::Publish && 
        m_status == ED_ERROR_NONE) {
      char *msg = NULL;
      // Different message for 1 or multiple files.
      if (m_tapeFS->GetNumFiles() > 1) {
        char *tmplate = XP_GetString(XP_EDT_PUBLISH_REPORT_MSG);
        if (tmplate) {
          msg = PR_smprintf(tmplate,m_tapeFS->GetNumFiles());
        }
      }
      else {
        msg = XP_STRDUP(XP_GetString(XP_EDT_PUBLISH_REPORT_ONE_FILE));
      }
      if (msg) {
        char * pBrowse = NULL, *pPub = NULL, *pLastPub = NULL;
        PREF_CopyCharPref("editor.publish_browse_location",&pBrowse);
        PREF_CopyCharPref("editor.publish_location",&pPub);
        PREF_CopyCharPref("editor.publish_last_loc",&pLastPub);

#if defined(XP_WIN) || defined(XP_OS2)
        /* FE_LoadUrl needs to be implemented in X */
    
        if( EDT_IsSameURL(pPub, pLastPub,0,0) && pBrowse && *pBrowse ){
          // We just saved to the default home location
          StrAllocCat(msg, XP_GetString(XP_EDT_BROWSE_TO_DEFAULT));
          if( FE_Confirm(m_pContext,msg) ){
            // User said Yes - load location into a BROWSER window (FALSE param)
            FE_LoadUrl(pBrowse, FALSE );
          }
        } else {
          FE_Alert(m_pContext,msg);
        }
        XP_FREEIF(pBrowse);
        XP_FREEIF(pPub);
#else
		// mac and unix
		FE_Message(m_pContext,msg);
#endif
        XP_FREE(msg);
      }
    }

    // LOU: What should I do here.  There appear to be connections on the 
    //  window that are still open....
    if( NET_AreThereActiveConnectionsForWindow( m_pContext ) ){
        //XP_ASSERT(FALSE);
        //NET_SilentInterruptWindow( m_pContext );
        XP_TRACE(("NET_AreThereActiveConnectionsForWindow is TRUE in ~CFileSaveObject"));
    }

    ////XP_FREE( m_pDestPathURL );

    // Allow front end interaction
    m_pContext->waitingMode = FALSE;
    // Clear flag (also done in CEditBuffer::FinishedLoad2)
    m_pContext->edit_saving_url = FALSE;
    
#ifdef XP_WIN    
    // Tell front end we finished autosave
    //   so it can gray the Save toolbar button
    if( m_bFromAutoSave )
        FE_UpdateEnableStates(m_pContext);
#endif

    // Tape File system should not be deleted yet.
    if (m_tapeFS && 
        m_tapeFS->GetType() == ITapeFileSystem::Publish &&
        m_status == ED_ERROR_NONE) {
      // Call publishComplete event.
      char *pDestURL = m_tapeFS->GetDestAbsURL();
      EDT_PerformEvent(m_pContext,"publishComplete",pDestURL,TRUE,FALSE,0,0);
      XP_FREEIF(pDestURL);
    }

    ///// This code will have to change if we implement a "saveComplete" event for
    ///// composer plugins.
    // Call callback for saving file to temporary location.
    if (m_tapeFS &&
        m_tapeFS->GetType() == ITapeFileSystem::File &&
        m_pSaveToTempData && m_pSaveToTempData->doneFn) {
      if (m_status == ED_ERROR_NONE) {
        (*m_pSaveToTempData->doneFn)(m_pSaveToTempData->pFileURL,m_pSaveToTempData->hook);
      }
      else {
        // Failed, pass in NULL as the temporary file name.
        (*m_pSaveToTempData->doneFn)(NULL,m_pSaveToTempData->hook);
      }
    }
    delete m_pSaveToTempData;
}

// Callback from tape file system.
//
//** Don't make this function static because it is a friend of CFileSaveObject
//** so it must be extern.  (at least on MAC)
void edt_CFileSaveObjectDone(XP_Bool success,void *arg) {
    CFileSaveObject *saveObject = (CFileSaveObject *)arg;
    if (!saveObject) {
        XP_ASSERT(0);
        return;
    }        
    
    if (!success) {
        saveObject->m_status = ED_ERROR_TAPEFS_COMPLETION;
    }
    // Tape File system should not be deleted yet.
    delete saveObject;
}

void
CFileSaveObject::CopyURLInfo(const URL_Struct *pURL)
{
	XP_ASSERT((m_tapeFS) && (m_iCurFile <= m_tapeFS->GetNumFiles()));
	
	if ((m_tapeFS) && (m_iCurFile <= m_tapeFS->GetNumFiles()))
		m_tapeFS->CopyURLInfo(m_iCurFile-1, pURL);
}

ED_FileError CFileSaveObject::FileFetchComplete() {
    XP_ASSERT(m_tapeFS->GetNumFiles());

    if( !m_bFromAutoSave && // Don't want dialogs from autosave, bug 43878
        m_tapeFS->GetType() != ITapeFileSystem::MailSend ){  // Suppress dialog for mail send.
        FE_SaveDialogDestroy( m_pContext, m_status, 0);
    }
    if (!m_tapeFS) {
        XP_ASSERT(0);
        return ED_ERROR_BLOCKED;  // is this the right thing to return?
    }

    ED_FileError statusResult = m_status;

    m_tapeFS->Complete( m_status == ED_ERROR_NONE, 
                                        edt_CFileSaveObjectDone, (void *)this );

    // "this" may already be deleted at this point, so we can't just return m_status.
    return statusResult;
}

ED_FileError CFileSaveObject::SaveFiles(){
    XP_ASSERT(m_tapeFS && m_tapeFS->GetNumFiles());

    //     Next param indicates if we are downloading (FALSE)
    //     or uploading (TRUE), so this can be used for publishing
    if( !m_bFromAutoSave && // Don't want dialogs from autosave, bug 43878
        m_tapeFS->GetType() != ITapeFileSystem::MailSend ){ // Suppress dialog for mail send.
      ED_SaveDialogType saveType;
      switch (m_tapeFS->GetType()) {
        case ITapeFileSystem::File:
          saveType = ED_SAVE_DLG_SAVE_LOCAL;
          break;
        case ITapeFileSystem::Publish:
          saveType = ED_SAVE_DLG_PREPARE_PUBLISH;
          break;
        default:
          XP_ASSERT(0);
      }
      FE_SaveDialogCreate( m_pContext, m_tapeFS->GetNumFiles(), saveType );
    }

    if (SaveFirstFile()) {
        // Child of CFileSaveObject took care of the first file.
        m_iCurFile++;
    }

    // loop through the tree getting all the image elements...
    return FetchNextFile();
}

// Just a wrapper around the Tape File System.
intn CFileSaveObject::AddFile( char *pSrcURL, char *pMIMEType, int16 iDocCharSetID){
    XP_ASSERT(m_tapeFS);
    return m_tapeFS->AddFile(pSrcURL,pMIMEType,iDocCharSetID);
}

//
// open the file and get the Netlib to fetch it from the cache.
//
ED_FileError CFileSaveObject::FetchNextFile(){
    XP_ASSERT(m_tapeFS);    

    if( m_status != ED_ERROR_NONE || m_iCurFile >= m_tapeFS->GetNumFiles() ){
        return FileFetchComplete();
    }    
    // m_iCurFile is 1-based, so i is 0-based.
    int i = m_iCurFile++;
    
    char *srcURL = m_tapeFS->GetSourceURL(i);
    if (!srcURL) {
        XP_ASSERT(0);
        return ED_ERROR_FILE_READ;
    }
    URL_Struct *pUrl;
    pUrl= NET_CreateURLStruct( srcURL, NET_DONT_RELOAD );
    if (!pUrl) {
        XP_ASSERT(0);
        XP_FREE(srcURL);
        return ED_ERROR_FILE_READ;    
    }    
    XP_FREE(srcURL);    
    // Store file save object so we can access OpenOutputFile etc.
    pUrl->fe_data = this;
    NET_GetURL( pUrl, FO_EDT_SAVE_IMAGE, m_pContext, edt_UrlExit );
    return m_status;
}

void CFileSaveObject::CheckFinishedSave(intn oneBased,ED_FileError iError) // one-based index.
{
  if (m_tapeFS->IsLocalPersistentFile(oneBased - 1)) {
    char *pAbsURL = GetDestAbsoluteURL(oneBased - 1);
    if (pAbsURL) {
      // Tell the front end that a new local file has been created.
      // We use number to tell between Main image (1) and Low-Res image (2)
      FE_FinishedSave( m_pContext, iError, pAbsURL, oneBased );
      XP_FREE(pAbsURL);
    }
  }
}

void CFileSaveObject::NetFetchDone( URL_Struct *pUrl, int status, MWContext *pContext ){
    // close the file in any case.
    if( m_pOutStream ){
        m_tapeFS->CloseStream(m_iCurFile-1);
        m_pOutStream = 0;
    }
    // Default: No error, show destination name for most errors
    ED_FileError iError = ED_ERROR_NONE;

    // NOTE: This is the NETLIB error code: (see merrors.h) 
    //   < 0 is an error
    if( status < 0 ){
        // As we become familiar with typical errors,
        //  add specialized error types here
        //  Use " extern MK_..." to access the error value
        if( status == MK_UNABLE_TO_LOCATE_FILE ||
            status == MK_MALFORMED_URL_ERROR ) {
            // Couldn't find source -- use source filename
            //   for error report
            iError = ED_ERROR_SRC_NOT_FOUND;
        } else if( m_bDontOverwrite ) {
            // We get status = MK_UNABLE_TO_CONVERT 
            //  if user said NO to overwriting an existing file:
            //  edt_MakeFileSaveStream returns 0,
            //  making NET_GetURL think it failed,
            //   but its not really an error
            iError = ED_ERROR_NONE;
        } else if( m_status == ED_ERROR_CANCEL ) {
            // User selected CANCEL at overwrite warning dialog,
            //  thus stream was not created and returns error status,
            //  but its not really an error
            iError = ED_ERROR_CANCEL;
        } else {
            // "Unknown" error default
            iError = ED_ERROR_FILE_READ;
        }
    }

    // ED_ERROR_FILE_OPEN was already handled in CFileSaveObject::OpenOutputFile()
    if (!m_bOpenOutputHandledError) {
      // No longer uses FE_SaveErrorContinueDialog()
      if (!SaveErrorContinueDialog(iError) ) {
        Cancel();
      }
      CheckFinishedSave(m_iCurFile,iError);
    }

    FetchNextFile();
}

intn CFileSaveObject::OpenOutputFile(){
    m_bOpenOutputHandledError = FALSE;

    // Return if we already have an open stream
    if( m_pOutStream != NULL){
        return 0;
    }
    // Current number incremented before comming here,
    //  or think of it as 1-based counting
    int i = m_iCurFile-1;
    
    XP_Bool bWriteThis = TRUE;
    char *pHumanName = m_tapeFS->GetHumanName(i);

    // Tell front end the filename even if we are not writing the file
    //
    if (!m_bDontOverwriteAll && 
        !m_bFromAutoSave && // Don't want dialogs from autosave, bug 43878
        m_tapeFS->GetType() != ITapeFileSystem::MailSend) { // Suppress dialog for mail send.
        FE_SaveDialogSetFilename( m_pContext, pHumanName );
    } 

    m_bDontOverwrite = m_bDontOverwriteAll;

    if( !m_bOverwriteAll){
        if( m_tapeFS->FileExists(i) ){
            // file exists.  
            if( m_bDontOverwriteAll ){
                bWriteThis = FALSE;
                CheckFinishedSave(m_iCurFile,ED_ERROR_FILE_EXISTS);
            }
            else {
                ED_SaveOption e;
                e = FE_SaveFileExistsDialog( m_pContext, pHumanName);

                switch( e ){
                case ED_SAVE_OVERWRITE_THIS:
                    break;
                case ED_SAVE_OVERWRITE_ALL:
                    m_bOverwriteAll = TRUE;
                    break;
                case ED_SAVE_DONT_OVERWRITE_ALL:
                    m_bDontOverwriteAll = TRUE;
                    // intentionally fall through
                case ED_SAVE_DONT_OVERWRITE_THIS:
                    // This flag prevents us from reporting
                    //  "failed" file stream creation as an error
                    m_bDontOverwrite = TRUE;
                    bWriteThis = FALSE;

                    CheckFinishedSave(m_iCurFile,ED_ERROR_FILE_EXISTS);
                    break;
                case ED_SAVE_CANCEL:
                    Cancel();
                    bWriteThis = FALSE;
                }
            }
        }
    }

    if( bWriteThis ){
       m_pOutStream = m_tapeFS->OpenStream( i );
    }
    else {
        // We are returning, not doing the save, but not an error condition
        XP_FREE(pHumanName);
        return -1;
    }

    XP_TRACE(("Opening stream number %d gives %d.", i, (int)m_pOutStream));
    
    if ( m_pOutStream == NULL ) {
        // No longer uses FE_SaveErrorContinueDialog()
        if(!SaveErrorContinueDialog( ED_ERROR_FILE_OPEN ) ){
            Cancel();
        }
        XP_FREE(pHumanName);

        CheckFinishedSave(m_iCurFile,ED_ERROR_FILE_OPEN);

        // So we don't report error twice, once on opening file and once
        // when netlib is unable to write to it.
        m_bOpenOutputHandledError = TRUE;

        return -1;
    }
    XP_FREE(pHumanName);
    return 0; // success
}

int CFileSaveObject::NetStreamWrite( const char *block, int32 length ){
    XP_TRACE(("NetStreamWrite: length = %lu", length));
    XP_ASSERT(m_pOutStream);
    m_pOutStream->Write((char *)block,length);
    return m_pOutStream->Status() == IStreamOut::EOS_NoError;
}

void CFileSaveObject::Cancel(){
    // tell net URL to stop...
    m_status = ED_ERROR_CANCEL;
    // Above alone does seem to work! Don't we need this?
    // NET_InterruptWindow causes a "reentrant interrupt" error message now!
    NET_SilentInterruptWindow( m_pContext );
}

char *CFileSaveObject::GetDestName( intn index ){ 
    if (!m_tapeFS) {
        XP_ASSERT(0);
        return NULL;
    }    
    return m_tapeFS->GetDestURL(index);
}

char *CFileSaveObject::GetDestAbsoluteURL( intn index ){ 
    if (!m_tapeFS) {
        XP_ASSERT(0);
        return NULL;
    }    
    char *pDestPathURL = m_tapeFS->GetDestPathURL();
    char *pDestRelativeURL = m_tapeFS->GetDestURL(index);
    if (!pDestRelativeURL) {
      XP_ASSERT(0);
      XP_FREEIF(pDestPathURL);
      return NULL;
    }

    // Maybe should use NET_MakeAbsolute() instead of concatenation.
    char *pDestAbsURL = pDestPathURL ? 
        PR_smprintf( "%s%s", pDestPathURL,  pDestRelativeURL ) :
        XP_STRDUP(pDestRelativeURL);
    XP_FREEIF(pDestPathURL);
    XP_FREE(pDestRelativeURL);

    return pDestAbsURL;
}

char *CFileSaveObject::GetSrcName( intn index ){ 
    if (!m_tapeFS) {
        XP_ASSERT(0);
        return NULL;
    }    
    return m_tapeFS->GetSourceURL(index);

    /*if( index >= m_srcImageURLs.Size() ){
        return 0;
    }
    else {
        return XP_STRDUP( m_srcImageURLs[index] );
    } */
}

// Convert file:// URLs into local file names for display to user.
PRIVATE void edt_url_make_human(char **pURL) {
  if (NET_IsLocalFileURL(*pURL)) {
    char *pTemp;
    XP_ConvertUrlToLocalFile(*pURL,&pTemp);
    XP_FREEIF(*pURL);
    *pURL = pTemp;
  }
}

XP_Bool CFileSaveObject::SaveErrorContinueDialog(ED_FileError iError) {
  // No user interaction for these conditions.
  if (iError == ED_ERROR_NONE ||
      iError == ED_ERROR_CANCEL ||
      iError == ED_ERROR_FILE_EXISTS) {
    return TRUE;
  }

  char *tmplate = NULL;
  char *msg = NULL;
  // m_iCurFile is 1-based.
  char *pAbsDest = GetDestAbsoluteURL(m_iCurFile-1);
  char *pRelDest = m_tapeFS->GetHumanName(m_iCurFile-1);
  char *pAbsSrc = GetSrcName(m_iCurFile-1);
  char *pRootAbsDest = GetDestAbsoluteURL(0);

  edt_url_make_human(&pAbsDest);
  edt_url_make_human(&pAbsSrc);
  edt_url_make_human(&pRootAbsDest);

  // Two level switch, first on different types of file systems,
  // second on different classes of errors.
  switch (m_tapeFS->GetType()) {
    case ITapeFileSystem::File:
      switch (iError) {
        case ED_ERROR_FILE_OPEN:
        case ED_ERROR_FILE_WRITE:
          tmplate = XP_STRDUP(XP_GetString(XP_EDT_ERR_SAVE_FILE_WRITE));
          StrAllocCat(tmplate,XP_GetString(XP_EDT_ERR_SAVE_CONTINUE));
          msg = PR_smprintf(tmplate,pRelDest,pRootAbsDest,pRelDest);
          break;
        case ED_ERROR_SRC_NOT_FOUND:
          tmplate = XP_STRDUP(XP_GetString(XP_EDT_ERR_SAVE_SRC_NOT_FOUND));
          StrAllocCat(tmplate,XP_GetString(XP_EDT_ERR_SAVE_CONTINUE));
          msg = PR_smprintf(tmplate,pAbsSrc,pAbsSrc);
          break;
        case ED_ERROR_FILE_READ:
          tmplate = XP_STRDUP(XP_GetString(XP_EDT_ERR_SAVE_FILE_READ));
          StrAllocCat(tmplate,XP_GetString(XP_EDT_ERR_SAVE_CONTINUE));
          msg = PR_smprintf(tmplate,pRelDest,pRelDest);
          break;
      }
      break;

    case ITapeFileSystem::Publish:
      switch (iError) {
        case ED_ERROR_FILE_OPEN:
        case ED_ERROR_FILE_WRITE:
          tmplate = XP_STRDUP(XP_GetString(XP_EDT_ERR_PUBLISH_FILE_WRITE));
          StrAllocCat(tmplate,XP_GetString(XP_EDT_ERR_PUBLISH_CONTINUE));
          msg = PR_smprintf(tmplate,pRelDest,pRelDest);
          break;
        case ED_ERROR_SRC_NOT_FOUND:
          tmplate = XP_STRDUP(XP_GetString(XP_EDT_ERR_PUBLISH_SRC_NOT_FOUND));
          StrAllocCat(tmplate,XP_GetString(XP_EDT_ERR_PUBLISH_CONTINUE));
          msg = PR_smprintf(tmplate,pAbsSrc,pAbsSrc);
          break;
        case ED_ERROR_FILE_READ:
          tmplate = XP_STRDUP(XP_GetString(XP_EDT_ERR_PUBLISH_FILE_READ));
          StrAllocCat(tmplate,XP_GetString(XP_EDT_ERR_PUBLISH_CONTINUE));
          msg = PR_smprintf(tmplate,pRelDest,pRelDest);
          break;
      }
      break;

    case ITapeFileSystem::MailSend:
      switch (iError) {
        case ED_ERROR_FILE_OPEN:
        case ED_ERROR_FILE_WRITE:
          tmplate = XP_STRDUP(XP_GetString(XP_EDT_ERR_MAIL_FILE_WRITE));
          StrAllocCat(tmplate,XP_GetString(XP_EDT_ERR_MAIL_CONTINUE));
          msg = PR_smprintf(tmplate,pRelDest,pRelDest);
          break;
        case ED_ERROR_SRC_NOT_FOUND:
          tmplate = XP_STRDUP(XP_GetString(XP_EDT_ERR_MAIL_SRC_NOT_FOUND));
          StrAllocCat(tmplate,XP_GetString(XP_EDT_ERR_MAIL_CONTINUE));
          msg = PR_smprintf(tmplate,pAbsSrc,pAbsSrc);
          break;
        case ED_ERROR_FILE_READ:
          tmplate = XP_STRDUP(XP_GetString(XP_EDT_ERR_MAIL_FILE_READ));
          StrAllocCat(tmplate,XP_GetString(XP_EDT_ERR_MAIL_CONTINUE));
          msg = PR_smprintf(tmplate,pRelDest,pRelDest);
          break;
      }
      break;
  }

  XP_Bool retVal = FALSE;
  if (msg) {
    retVal = FE_Confirm(m_pContext,msg);
  }
  else {
    XP_ASSERT(0);
  }

  XP_FREEIF(tmplate);
  XP_FREEIF(msg);
  XP_FREEIF(pAbsDest);
  XP_FREEIF(pRelDest);
  XP_FREEIF(pAbsSrc);
  XP_FREEIF(pRootAbsDest);

  return retVal;
}


char * CEditSaveObject::m_pFailedPublishUrl = NULL;

//-----------------------------------------------------------------------------
// CEditSaveObject
//-----------------------------------------------------------------------------

CEditSaveObject::CEditSaveObject( CEditBuffer *pBuffer, 
                                    ED_SaveFinishedOption finishedOpt,
                                    char *pSrcURL, 
                                    ITapeFileSystem *tapeFS,
                                    XP_Bool, // bSaveAs is ignored. 
                                    XP_Bool bKeepImagesWithDoc, 
                                    XP_Bool bAutoAdjustLinks,
                                    XP_Bool bAutoSave,
                                    CEditSaveToTempData *pSaveToTempData ) :
        CFileSaveObject( pBuffer->m_pContext,pSrcURL,tapeFS,bAutoSave, pSaveToTempData ),
        m_pBuffer( pBuffer ),
        m_pDocStateSave( 0 ),
        m_pSrcURL( XP_STRDUP( pSrcURL ) ),
        m_finishedOpt( finishedOpt ),
        m_bKeepImagesWithDoc( bKeepImagesWithDoc ),
        m_bAutoAdjustLinks( bAutoAdjustLinks ),
        m_backgroundIndex(IgnoreImage) {
}

CEditSaveObject::~CEditSaveObject(){
    XP_Bool bRevertBuffer = FALSE;
    XP_Bool bRelayout = FALSE;

    /* 
     * possible values for m_finishedOpt:
     * ED_FINISHED_GOTO_NEW,       Point the editor to the location of the 
     *                             newly saved document if successful, otherwise revert buffer.
     * ED_FINISHED_REVERT_BUFFER,  Revert the buffer to the state before
     *                             the save operation began, regardless.
     * ED_FINISHED_SAVE_DRAFT      Like ED_FINISHED_REVERT_BUFFER, except clears the dirty flag
     *                             on success.
     * ED_FINISHED_MAIL_SEND       If we succeed we're going to throw the buffer
     *                             away, so don't revert it, but don't bother gotoNew.  
     *                             If failure, revert the buffer.
     *                             Used for mail compose, we don't 
     *                             want the editor to start any operation that 
     *                             causes problems when libmsg destroys the editor
     *                             context. 
     */


    if ( m_finishedOpt == ED_FINISHED_GOTO_NEW && m_status == ED_ERROR_NONE )
    {
        //// Go to new location.

        m_pBuffer->m_pContext->is_new_document = FALSE;

        // Set the document title and history entry to the new location.

        char *pDestFileURL = GetDestAbsoluteURL(0);
        if ( !pDestFileURL )
        {        
            XP_ASSERT( 0 );
            m_status = ED_ERROR_FILE_WRITE; // Not really the right thing to set.
            return;        
        }


        // We used to do the ftp://username@path here, but it's moved to earlier in
        // the save process, in EDT_PublishFile.

        // Set this first - FE_SetDocTitle will get URL from it
        LO_SetBaseURL( m_pBuffer->m_pContext, pDestFileURL );
      	History_entry * hist_ent =
    		SHIST_GetCurrent(&(m_pBuffer->m_pContext->hist));
        char * pTitle = NULL;
	      if(hist_ent)
          {
            if ( hist_ent->title && XP_STRLEN(hist_ent->title) > 0 ){
                // Note: hist_ent->title should always = m_pContext->title?
                // Change "file:///Untitled" into URL
                // Use the new address if old title == old address
                if( 0 != XP_STRCMP(hist_ent->title, XP_GetString(XP_EDIT_NEW_DOC_NAME) ) && 
                    0 != XP_STRCMP(hist_ent->title, hist_ent->address) )
                {
                    pTitle = XP_STRDUP(hist_ent->title);
                } else {
                    pTitle = XP_STRDUP(pDestFileURL);
                    XP_FREE(hist_ent->title);
                    hist_ent->title = XP_STRDUP(pDestFileURL);
                }

            } else {
                // Use the URL if no Document title
                pTitle = XP_STRDUP(pDestFileURL);
                XP_FREEIF(hist_ent->title);
                hist_ent->title = XP_STRDUP(pDestFileURL);
            }
            // Test if new URL is same as current before we replace the latter
            XP_Bool bSameURL = EDT_IsSameURL( hist_ent->address, pDestFileURL, NULL, NULL);
            
            // Set history entry address to new URL
            XP_FREEIF(hist_ent->address);
            hist_ent->address = XP_STRDUP(pDestFileURL);

            // We may inherit this URL (used for faster reloading in browser)
            //  when saving a remote URL to local disk,
            //  so always clear it since Editor never uses it.
            XP_FREEIF(hist_ent->wysiwyg_url);

            // This changes doc title, window caption, history title,
            ///  but not history address
            if (pTitle)
            {
              FE_SetDocTitle(m_pBuffer->m_pContext, pTitle);
              XP_FREE(pTitle);
            }  
            
            if( !bSameURL )
            {
                // Make the new URL (now in current history entry) 
                //   as the most-recently-used URL in prefs. list
                //   (Note: We should never be here in mail message composer)
                EDT_SyncEditHistory(m_pBuffer->m_pContext);
            }

        } // if (hist_ent)
        XP_FREE(pDestFileURL);

        // Saved successfully.
        m_pBuffer->DocumentStored();

        // This flag must be cleared before GetFileWriteTime,
        //  else the write time will not be recorded
        m_pContext->edit_saving_url = FALSE;
        // Get the current file time
        m_pBuffer->GetFileWriteTime();
        // Some of the links may have changed, so relayout
        bRelayout = TRUE;
    } 
    else if ( m_finishedOpt == ED_FINISHED_MAIL_SEND && m_status == ED_ERROR_NONE )
    {
      //// Do nothing.
      // Don't relayout, we're throwing the buffer away.
    }
    else if (m_pDocStateSave) {
      //// Don't go to the new document, revert to m_pDocStateSave.
      bRevertBuffer = TRUE;        
      // Don't relayout, reverting the buffer will take care of it.
    }

    if ( m_finishedOpt == ED_FINISHED_SAVE_DRAFT && m_status == ED_ERROR_NONE )
    {
        // Note that we are still reverting the buffer.
        
        // Saved successfully.
        m_pBuffer->DocumentStored();
    }


    // Originally, this was only called if m_status == ED_ERROR_NONE.
    m_pBuffer->FileFetchComplete(m_status);
    if (bRelayout)
    {
      // Don't always relayout because we may have "cid:" links which cause an error.
      // reverting the buffer will take care of it.
      m_pBuffer->RefreshLayout();
    }
    
    
    // Already cleared above if ED_FINISHED_GOTO_NEW and success,
    // but it must be cleared eventually in all cases.
    m_pContext->edit_saving_url = FALSE;

    m_pBuffer->m_pSaveObject = 0;

    if (bRevertBuffer) {
        // Must be after CEditBuffer::FileFetchComplete(), because it deletes CEditBuffer.
        m_pBuffer->RestoreState(m_pDocStateSave);
    }
    
    if( m_tapeFS->GetType() == ITapeFileSystem::Publish ){
        XP_FREEIF(m_pFailedPublishUrl);
    
        if( m_status == ED_ERROR_NONE ){
            // Save the last-used location into the history list
            EDT_SyncPublishingHistory();
        } else {
            // Save this URL so we can supply the "bad" location
            //   to user if they try to publish that URL again
            m_pFailedPublishUrl = XP_STRDUP(m_pSrcURL);
        }
    }
    XP_FREE( m_pSrcURL );
    
    delete m_pDocStateSave;
}

// Add all images in document that are also in ppIncludedFiles.
//
// If ppIncludedFiles is NULL, this should add exactly those files that would be returned by
// CEditBuffer::GetAllDocumentFiles() with the selected flag set TRUE.
// i.e. Regular Saving should save exactly those files that are published by default in remote publishing.
XP_Bool CEditSaveObject::AddAllFiles(char **ppIncludedFiles){
//////// WARNING: if you change this function, fix CEditBuffer::GetAllDocumentFiles and CEditSaveObject::FixupLinks() also.
    EDT_ImageData *pData;
    EDT_PageData *pPageData;

    // Make sure all URLs in ppIncludedFiles are absolute
    if (ppIncludedFiles) {
      char **ppFile = ppIncludedFiles;
      while (*ppFile) {
        char *pAbs = NET_MakeAbsoluteURL(m_pSrcURL,*ppFile);
        if (pAbs) {
          // If making *ppFile absolute changed it, replace it with absolute URL.
          if (XP_STRCMP(pAbs,*ppFile)) {
            XP_FREE(*ppFile);
            *ppFile = pAbs;
          }
          else {
            XP_FREE(pAbs);
          }
        }
        
        ppFile++;
      }
    }

    // Add actual HTML document.    
    intn firstIndex = AddFile(m_pSrcURL,TEXT_HTML,m_pBuffer->GetDocCharSetID());
    if (firstIndex != 0) {
        m_status = ED_ERROR_BAD_URL;
        FreeList(ppIncludedFiles);
        return FALSE;
    }


    CEditElement *pImage = m_pBuffer->m_pRoot->FindNextElement( 
                                             &CEditElement::FindImage, 0 );

    // If there is a background Image, make it the first image.
    pPageData = m_pBuffer->GetPageData();
    if( pPageData ) {
        if ( pPageData->pBackgroundImage && *pPageData->pBackgroundImage){
            m_backgroundIndex = CheckAddFile( pPageData->pBackgroundImage, NULL, ppIncludedFiles, 
                m_bKeepImagesWithDoc && !pPageData->bBackgroundNoSave );
        }
        // We now support > 1 FontDefURL per page
        int iFontDefCount = m_pBuffer->m_FontDefURL.Size();
        for( int i = 0; i < iFontDefCount; i++ )
        {
            intn iIndex = CheckAddFile( m_pBuffer->m_FontDefURL[i], NULL, ppIncludedFiles, 
                                        m_bKeepImagesWithDoc && !m_pBuffer->m_FontDefNoSave[i] );
            m_FontDefIndex.Add(iIndex);
        }
#if 0
        if ( pPageData->pFontDefURL && *pPageData->pFontDefURL ){
            m_fontDefIndex = CheckAddFile( pPageData->pFontDefURL, NULL, ppIncludedFiles, 
                m_bKeepImagesWithDoc && !pPageData->bFontDefNoSave );
        }
#endif
    }
    m_pBuffer->FreePageData( pPageData );

    while( pImage )
    {
        // If this assert fails, it is probably because FinishedLoad
        // didn't get called on the image. And that's probably because
        // of some bug in the recursive FinishedLoad code.
        // If the size isn't known, the relayout after the save will block,
        // and the fourth .. nth images in the document will get zero size.
        // XP_ASSERT(pImage->Image()->SizeIsKnown());
        pImage->Image()->m_iSaveIndex = IgnoreImage;
        pImage->Image()->m_iSaveLowIndex = IgnoreImage;
        pData = pImage->Image()->GetImageData();
        if( pData )
        {
            // Only consider images that are not server-generated
            if( EDT_IsImageURL(pData->pSrc) )
            {
                pImage->Image()->m_iSaveIndex = CheckAddFile( pData->pSrc, NULL, ppIncludedFiles, 
                    m_bKeepImagesWithDoc && !pData->bNoSave );
            }
            if( EDT_IsImageURL(pData->pLowSrc) )
            {
                pImage->Image()->m_iSaveLowIndex = CheckAddFile( pData->pLowSrc, NULL,ppIncludedFiles, 
                    m_bKeepImagesWithDoc && !pData->bNoSave );
            }
            edt_FreeImageData( pData );
        }
        pImage = pImage->FindNextElement( &CEditElement::FindImage, 0 );
    }


    //// Sure would be nice to abstract all the different types of table
    //// backgrounds.
    // table backgrounds <table>
    CEditElement *pNext = m_pBuffer->m_pRoot;
    while(NULL != (pNext = pNext->FindNextElement( &CEditElement::FindTable, 0 )) ){
      EDT_TableData *pData = ((CEditTableElement *)pNext)->GetData();
      if (pData) {
        if ( pData->pBackgroundImage && *pData->pBackgroundImage) {
          ((CEditTableElement *)pNext)->m_iBackgroundSaveIndex = CheckAddFile( pData->pBackgroundImage, NULL, ppIncludedFiles, 
              m_bKeepImagesWithDoc && !pData->bBackgroundNoSave);
        }
        CEditTableElement::FreeData(pData);
      }
    }
    // table row backgrounds <tr>
    pNext = m_pBuffer->m_pRoot;
    while(NULL != (pNext = pNext->FindNextElement( &CEditElement::FindTableRow, 0 )) ){
      EDT_TableRowData *pData = ((CEditTableRowElement *)pNext)->GetData();
      if (pData) {
        if ( pData->pBackgroundImage && *pData->pBackgroundImage) {
          ((CEditTableRowElement *)pNext)->m_iBackgroundSaveIndex = CheckAddFile( pData->pBackgroundImage, NULL, ppIncludedFiles, 
              m_bKeepImagesWithDoc && !pData->bBackgroundNoSave);
        }
        CEditTableRowElement::FreeData(pData);
      }
    }
    // table cell backgrounds <td> <th>
    pNext = m_pBuffer->m_pRoot;
    while(NULL != (pNext = pNext->FindNextElement( &CEditElement::FindTableCell, 0 )) ){
      EDT_TableCellData *pData = ((CEditTableCellElement *)pNext)->GetData(0);
      if (pData) {
        if ( pData->pBackgroundImage && *pData->pBackgroundImage) {
          ((CEditTableCellElement *)pNext)->m_iBackgroundSaveIndex = CheckAddFile( pData->pBackgroundImage, NULL, ppIncludedFiles, 
              m_bKeepImagesWithDoc && !pData->bBackgroundNoSave);
        }
        CEditTableCellElement::FreeData(pData);
      }
    }


    // UnknownHTML tags with LOCALDATA attribute.
    pNext = m_pBuffer->m_pRoot;
    if ( pNext ) {
        while(NULL != (pNext = pNext->FindNextElement( &CEditElement::FindUnknownHTML, 0 )) ){
          CEditIconElement *pIcon = CEditIconElement::Cast(pNext);
          if (pIcon) {
            char **pMimeTypes;
            char **pURLs;
            int count = pIcon->ParseLocalData(&pMimeTypes,&pURLs);
            
            // Remember indices into save object.
            if (count) {
              XP_FREEIF(pIcon->m_piSaveIndices);
              // allocate memory.            
              pIcon->m_piSaveIndices = new intn[count];
            }

            // Maybe should make a check that pURLs[n] is a relative URL
            // in current directory.
            for (int n = 0; n < count; n++) {
              pIcon->m_piSaveIndices[n] = CheckAddFile(pURLs[n],pMimeTypes[n],ppIncludedFiles,TRUE);
            }
            CEditIconElement::FreeLocalDataLists(pMimeTypes,pURLs,count);
          }
        } // while
    }


    // Go through ppIncludedFiles once more to add anything we missed.
    //
    // This is for the "add all files in directory" button in publishing,
    // here ppIncludedFiles may contain files that CEditSaveObject knows
    // nothing about.
    //
    // Doesn't hurt to add files twice to tape file system, it'll just 
    // return the previous index.
    if (ppIncludedFiles) {
      char **pFile = ppIncludedFiles;
      while (*pFile) {
        // Ignore returned index, we're not going to update any links here.
        AddFile(*pFile,NULL,m_pBuffer->GetDocCharSetID());
        pFile++;
      }
    }


    FreeList(ppIncludedFiles);
    return TRUE;
}

// Returns the URL that should be used as the base for relative URLs in 
// the document.  Similar to just calling LO_GetBaseURL(), except returns
// the document temp directory for untitled documents.
// Must free returned memory.
char *edt_GetDocRelativeBaseURL(MWContext *pContext) {
  char *pDocURL = NULL;

  // Interpret URLs relative to temp directory if a new document.
  if (EDT_IS_NEW_DOCUMENT(pContext)) {
    char *pxpURL = EDT_GetDocTempDir(pContext);
    if (!pxpURL) {
      return pDocURL;
    }
    // prepend "file://" to make a URL.
    StrAllocCat(pDocURL,"file://");
    StrAllocCat(pDocURL,pxpURL);
    XP_FREE(pxpURL);
  }
  else {
    pDocURL = edt_StrDup(LO_GetBaseURL(pContext));
  }
  return pDocURL;
}


// Mostly a wrapper for CFileSaveObject::AddFile, except deals with adjusting links.
//
// Analogous to AddToBufferUnique() in edtbuf.cpp
intn  CEditSaveObject::CheckAddFile( char *pSrc, char *pMIMEType, 
                      char **ppIncludedFiles, XP_Bool bSaveDefault ){
    intn retVal = IgnoreImage;
    if( EDT_IS_LIVEWIRE_MACRO( pSrc ) ){
        return IgnoreImage;  
    }

    char *pDocURL = edt_GetDocRelativeBaseURL(m_pContext);
    if (!pDocURL) {
      return IgnoreImage;
    }

    char *pAbsoluteSrc = NET_MakeAbsoluteURL(pDocURL,pSrc);
    XP_FREE(pDocURL);
    if (!pAbsoluteSrc) {
      return IgnoreImage;
    }

    // If ppIncludedFiles is passed in, use it to decide whether to add file.
    // Otherwise, use bSaveDefault to decide.
    XP_Bool saveMe;
    if (ppIncludedFiles) {
      saveMe = URLInList(ppIncludedFiles,pAbsoluteSrc);
    }
    else {
      saveMe = bSaveDefault;
    }
    
    if (saveMe) {
      retVal = AddFile(pSrc,pMIMEType,m_pBuffer->GetDocCharSetID());        
      if (retVal == ITapeFileSystem::Error || retVal == ITapeFileSystem::SourceDestSame) {
          // couldn't make a good local name, so try to fix it.
          retVal = AttemptAdjust;
      }
    }
    else if ( m_bAutoAdjustLinks ) { 
        retVal = AttemptAdjust;
    }

    XP_FREE(pAbsoluteSrc);
    return retVal;
}

void CEditSaveObject::FreeList(char **ppList) {
  if (!ppList) 
    return;

  int n = 0;
  while (ppList[n]) {
    XP_FREE(ppList[n]);
    n++;
  }
  
  XP_FREE(ppList);  
}

// ppList and pURL must be absolute URLs.
XP_Bool CEditSaveObject::URLInList(char **ppList, char *pURL ) {
  for (int  n = 0; ppList[n]; n++) {
    if (EDT_IsSameURL(ppList[n],pURL,NULL,NULL))
      return TRUE;
  }
  return FALSE;
}

// Should be called no matter what, even if error or cancel occurs.
ED_FileError CEditSaveObject::FileFetchComplete(){
    XP_ASSERT(XP_IsContextInList(m_pBuffer->m_pContext));

    return CFileSaveObject::FileFetchComplete();
}

// Always return TRUE.
XP_Bool CEditSaveObject::SaveFirstFile() {
    if ( m_status != ED_ERROR_NONE ) {
        XP_ASSERT(0);
        return TRUE;    
    }

    XP_ASSERT(!m_pDocStateSave);
    m_pDocStateSave = m_pBuffer->RecordState();

    char *pBadLinks = FixupLinks();
    PRBool needEncrypt = EDT_EncryptState( m_pContext );

    // Tell user some links may be broken by this operation, give choice
    // of continuing or not.
    if (pBadLinks) {
      XP_Bool bCancel = FALSE;
      char *tmplate = XP_GetString(XP_EDT_BREAKING_LINKS);
      char *msg = NULL;
      if (tmplate) {
        msg = PR_smprintf(tmplate,pBadLinks);
        bCancel = !FE_Confirm(m_pContext,msg);
      }

      XP_FREEIF(msg);
      XP_FREEIF(pBadLinks);
      if (bCancel) {
          Cancel();
          return TRUE;
      }
    }

    // Write root HTML document
    char *pHumanName = m_tapeFS->GetHumanName(0);
    if (pHumanName == NULL) {
        XP_ASSERT(0);
        return TRUE;
    }    
    
    // Tell front end the filename.
    if( !m_bFromAutoSave && // Don't want dialogs from autosave, bug 43878
        m_tapeFS->GetType() != ITapeFileSystem::MailSend ){// Suppress dialog for mail send.
      FE_SaveDialogSetFilename( m_pContext, pHumanName );
    } 

    IStreamOut *out = NULL;
    URL_Struct *URL_s = NULL;

    if (needEncrypt) { 
	    // by the time the file gets to the tapeFS, it will be binary.
	    m_tapeFS->SetFirstBinary();
	    out = new CNetStreamToTapeFS(m_pContext,m_tapeFS);
    }
    else {
        out = m_tapeFS->OpenStream(0);
    }
    
    if (out == NULL) {
        m_status = ED_ERROR_FILE_OPEN;
    }
    else {
      m_pBuffer->WriteToStream( out );
      if ( out->Status() != IStreamOut::EOS_NoError ) {
          m_status = ED_ERROR_FILE_WRITE;
      }
      if (!needEncrypt) {
          m_tapeFS->CloseStream(0);    
      }
      else {
         delete out;
      }
    }
    
    if( m_status != ED_ERROR_NONE ){
      char *tmplate = NULL;
      char *msg = NULL;
      char *pAbsDest = GetDestAbsoluteURL(0);
      char *pAbsSrc = GetSrcName(0);
      edt_url_make_human(&pAbsDest);
      edt_url_make_human(&pAbsSrc);

      switch (m_tapeFS->GetType()) {
        case ITapeFileSystem::File:
          tmplate = XP_GetString(XP_EDT_ERR_SAVE_WRITING_ROOT);
          if (tmplate) {
            char *pRelDest = m_tapeFS->GetHumanName(0);
            msg = PR_smprintf(tmplate,pRelDest,pAbsDest);
            XP_FREEIF(pRelDest);
          }
          break;
        case ITapeFileSystem::Publish:
          tmplate = XP_GetString(XP_EDT_ERR_PUBLISH_PREPARING_ROOT);
          if (tmplate) {
            msg = PR_smprintf(tmplate,pAbsSrc);
            StrAllocCat(msg,XP_GetString(XP_EDT_ERR_CHECK_DISK));
          }
          break;
        case ITapeFileSystem::MailSend:
          msg = XP_STRDUP(XP_GetString(XP_EDT_ERR_MAIL_PREPARING_ROOT));
          break;
      }
      if (msg) {
        FE_Alert(m_pContext,msg);
      }

      XP_FREEIF(msg);
      XP_FREEIF(pAbsDest);
      XP_FREEIF(pAbsSrc);
    }

    // We know this is file number 1,  CFileSaveObject::m_iCurFile is 1-based.
    CheckFinishedSave(1,m_status);

    XP_FREE(pHumanName);
    return TRUE;
}

void CEditSaveObject::FixupLink(intn iIndex, // index to tape file system
                                char **ppImageURL,   // Value to fixup.
                                char *pDestPathURL,
                                ED_HREFList *badLinks) {
  if( iIndex == IgnoreImage ){
    return;
  }

  if( iIndex == AttemptAdjust ){
    CEditLinkManager::AdjustLink(ppImageURL,m_pSrcURL,pDestPathURL,badLinks);
  }
  else {
    // Look it up from tape file system.
    char *pNewURL = GetDestName(iIndex);
    if (pNewURL) {
      XP_FREEIF(*ppImageURL);
      *ppImageURL = pNewURL;
    }
  }          
}

char *CEditSaveObject::FixupLinks(){
//////// WARNING: if you change this function, fix CEditSaveObject::AddAllFiles() and CEditBuffer::GetAllDocumentFiles() also.
    ED_HREFList badLinks;

    EDT_ImageData *pData;

    char *pDestPathURL = m_tapeFS->GetDestPathURL();
    // Note: pDestPathURL may legally be NULL.

    // If there is a background Image, make it the first one.
    FixupLink(m_backgroundIndex,&m_pBuffer->m_pBackgroundImage,pDestPathURL,&badLinks);

    // We now support > 1 FontDefURLs
    int iFontDefCount = m_pBuffer->m_FontDefURL.Size();
    for( int i; i < iFontDefCount; i++ )
        FixupLink(m_FontDefIndex[i], &m_pBuffer->m_FontDefURL[i], pDestPathURL, &badLinks);

    // regular images.
    CEditElement *pImage = m_pBuffer->m_pRoot->FindNextElement( &CEditElement::FindImage, 0 );
    while( pImage )
    {
        pData = pImage->Image()->GetImageData();
        if( pData )
        {        
            // Only consider images that are not server-generated
            if( EDT_IsImageURL(pData->pSrc) )
            {
                // do the normal image
                FixupLink(pImage->Image()->m_iSaveIndex,&pData->pSrc,pDestPathURL,&badLinks);
            }            
            if( EDT_IsImageURL(pData->pLowSrc) )
            {
                // do the lowres image
                FixupLink(pImage->Image()->m_iSaveLowIndex,&pData->pLowSrc,pDestPathURL,&badLinks);
            }
            pImage->Image()->SetImageData( pData );
            edt_FreeImageData( pData );
        }
        pImage = pImage->FindNextElement( &CEditElement::FindImage, 0 );
    }


    //// Sure would be nice to abstract all the different types of table
    //// backgrounds.
    // table backgrounds <table>
    CEditElement *pNext = m_pBuffer->m_pRoot;
    while(NULL != (pNext = pNext->FindNextElement( &CEditElement::FindTable, 0 )) ){
      CEditTableElement *pTable = (CEditTableElement *)pNext;
      EDT_TableData *pData = pTable->GetData();
      if (pData) {
        if ( pData->pBackgroundImage && *pData->pBackgroundImage) {
          FixupLink(pTable->m_iBackgroundSaveIndex,&pData->pBackgroundImage,pDestPathURL,&badLinks);
          pTable->SetData(pData);
        }
        CEditTableElement::FreeData(pData);
      }
    }
    // table row backgrounds <tr>
    pNext = m_pBuffer->m_pRoot;
    while(NULL != (pNext = pNext->FindNextElement( &CEditElement::FindTableRow, 0 )) ){
      CEditTableRowElement *pTable = (CEditTableRowElement *)pNext;
      EDT_TableRowData *pData = pTable->GetData();
      if (pData) {
        if ( pData->pBackgroundImage && *pData->pBackgroundImage) {
          FixupLink(pTable->m_iBackgroundSaveIndex,&pData->pBackgroundImage,pDestPathURL,&badLinks);
          pTable->SetData(pData);
        }
        CEditTableRowElement::FreeData(pData);
      }
    }
    // table cell backgrounds <td> <th>
    pNext = m_pBuffer->m_pRoot;
    while(NULL != (pNext = pNext->FindNextElement( &CEditElement::FindTableCell, 0 )) ){
      CEditTableCellElement *pTable = (CEditTableCellElement *)pNext;
      EDT_TableCellData *pData = pTable->GetData(0);
      if (pData) {
        if ( pData->pBackgroundImage && *pData->pBackgroundImage) {
          FixupLink(pTable->m_iBackgroundSaveIndex,&pData->pBackgroundImage,pDestPathURL,&badLinks);
          pTable->SetData(pData);
        }
        CEditTableCellElement::FreeData(pData);
      }
    }

    // UnknownHTML tags with LOCALDATA attribute.
    pNext = m_pBuffer->m_pRoot;
    while(NULL != (pNext = pNext->FindNextElement( &CEditElement::FindUnknownHTML, 0 )) ){
      CEditIconElement *pIcon = CEditIconElement::Cast(pNext);
      if (pIcon) {
        char **pMimeTypes;
        char **pURLs;
        int count = pIcon->ParseLocalData(&pMimeTypes,&pURLs);

        if (count) {
          if (pIcon->m_piSaveIndices) {
            // Change all occurances of pURLs[n] in the unknown tag to
            // take account of the new location.
            for (int n = 0; n < count; n++) {
              char *pPrev = XP_STRDUP(pURLs[n]); // Remember, so we can se if pURLs[n] changes.
              FixupLink(pIcon->m_piSaveIndices[n],&pURLs[n],pDestPathURL,&badLinks);
              // Note: ReplaceParamValue checks if the two are the same.
              // Also note: the LOCALDATA parameter itself will be updated.
              pIcon->ReplaceParamValues(pPrev,pURLs[n]);
              XP_FREEIF(pPrev);
            }
          } 
          else {
            XP_ASSERT(0);
          }
        }

        // kill and zero memory for saved indices.
        XP_FREEIF(pIcon->m_piSaveIndices);
        CEditIconElement::FreeLocalDataLists(pMimeTypes,pURLs,count);
      }
    } // while


    
    // Adjust the HREFs.
    if( m_bAutoAdjustLinks && pDestPathURL ){
       // No longer use CEditLinkManager::AdjustAllLinks because its ref counting is 
       // messed up and has more links than are currently in document.  It adds too 
       // many lines to badLinks (which is actually displayed to the user.)
       // m_pBuffer->linkManager.AdjustAllLinks( m_pSrcURL, pDestPathURL, &badLinks );

        // Walk the tree and find all HREFs.
        CEditElement *pLeaf = m_pBuffer->m_pRoot->FindNextElement( 
                                      &CEditElement::FindLeafAll,0 );
        // First sweep, mark all HREFs as not adjusted.
        while (pLeaf) {
          m_pBuffer->linkManager.SetAdjusted(pLeaf->Leaf()->GetHREF(),FALSE);
          pLeaf = pLeaf->FindNextElement(&CEditElement::FindLeafAll,0 );
        }
        // Second sweep, actually adjust the HREFs.
        pLeaf = m_pBuffer->m_pRoot->FindNextElement( 
                &CEditElement::FindLeafAll,0 );
        while (pLeaf) {
          ED_LinkId linkId = pLeaf->Leaf()->GetHREF();
          if (linkId && !m_pBuffer->linkManager.GetAdjusted(linkId)) {
            // linkManager can deal with NULL HREF.
            m_pBuffer->linkManager.AdjustLink(linkId,m_pSrcURL, pDestPathURL, &badLinks );          
            m_pBuffer->linkManager.SetAdjusted(linkId,TRUE);
          }
          pLeaf = pLeaf->FindNextElement(&CEditElement::FindLeafAll,0 );
        }
    }
    
    XP_FREEIF(pDestPathURL);

    // Package up a string of all the bad links found.
    char *pRet = NULL;
    if (badLinks.Size() > 0) {
      int n;
      for (n = 0; n < badLinks.Size(); n++) {
        StrAllocCat(pRet,badLinks[n]);
        StrAllocCat(pRet,"\n");
        XP_FREEIF(badLinks[n]);
      }
    }

    return pRet;
}


#if 0 //// Now obsolete
//-----------------------------------------------------------------------------
// CEditImageSaveObject
//-----------------------------------------------------------------------------

CEditImageSaveObject::CEditImageSaveObject( CEditBuffer *pBuffer, 
                EDT_ImageData *pData, XP_Bool bReplaceImage ) 
    :
        CFileSaveObject( pBuffer->m_pContext ),
        m_pBuffer( pBuffer ),
        m_pData( edt_DupImageData( pData ) ),
        m_srcIndex(-1),
        m_lowSrcIndex(-1),
        m_bReplaceImage( bReplaceImage )
{
    // we are going to write the image files into
    SetDestPathURL( LO_GetBaseURL( pBuffer->m_pContext ) );
}

CEditImageSaveObject::~CEditImageSaveObject(){
    edt_FreeImageData( m_pData );
    m_pBuffer->m_pSaveObject = 0;
}

ED_FileError CEditImageSaveObject::FileFetchComplete(){

    if( m_status == ED_ERROR_NONE ){
        char *pName = 0;
        if( m_srcIndex != -1 && (pName = GetDestName(m_srcIndex)) != 0 ){
            if( m_pData->pSrc) XP_FREE( m_pData->pSrc );
            m_pData->pSrc = pName;
        }

        if( m_lowSrcIndex != -1 && (pName = GetDestName(m_lowSrcIndex)) != 0 ){
            XP_FREE( m_pData->pLowSrc );
            m_pData->pLowSrc = pName;
        }

        m_pBuffer->m_pLoadingImage = new CEditImageLoader( m_pBuffer, 
                    m_pData, m_bReplaceImage );
        m_pBuffer->m_pLoadingImage->LoadImage();
    }
    return CFileSaveObject::FileFetchComplete();
}
#endif

#if 0 // now obsolete
//-----------------------------------------------------------------------------
// CEditBackgroundImageSaveObject
//-----------------------------------------------------------------------------

CEditBackgroundImageSaveObject::CEditBackgroundImageSaveObject( CEditBuffer *pBuffer)
    :
        CFileSaveObject( pBuffer->m_pContext ),
        m_pBuffer( pBuffer )
{
    // we are going to write the image files into
    SetDestPathURL( LO_GetBaseURL( pBuffer->m_pContext ) );
}

CEditBackgroundImageSaveObject::~CEditBackgroundImageSaveObject(){
    m_pBuffer->m_pSaveObject = 0;
}

ED_FileError CEditBackgroundImageSaveObject::FileFetchComplete(){
    // Note: If user canceled in overwrite dialog, then we don't change the background
    if( m_status == ED_ERROR_NONE ){
        if( m_pBuffer->m_pBackgroundImage ) XP_FREE(m_pBuffer->m_pBackgroundImage);

        // Get the one and only image file (without local or URL path)
        m_pBuffer->m_pBackgroundImage = GetDestName(0);

        // This will set background image
        m_pBuffer->RefreshLayout();
        // LO_SetBackgroundImage( m_pBuffer->m_pContext, m_pBuffer->m_pBackgroundImage );
    }
    return CFileSaveObject::FileFetchComplete();
}
#endif

//
// Sigh, why did the editor invent a new streaming type? This function
// allows the preencrypted file standard streamer to point back to the
// Editor's stream without turning the C++ virus loose on the security
// library.
//

typedef struct edtPrivStructStr {
     ITapeFileSystem *tapeFS;
	 IStreamOut *out;
} edtPrivStruct;

static unsigned int
edt_TapeIsReady(NET_StreamClass *stream) {	
    return MAX_WRITE_READY;
}

static void
edt_TapeComplete(NET_StreamClass *stream) {
    edtPrivStruct *obj = (edtPrivStruct *)stream->data_object;	
    obj->tapeFS->CloseStream(0);
    XP_FREE(obj);
    return;
}

static void
edt_TapeAbort(NET_StreamClass *stream, int status) {	
    edt_TapeComplete(stream);
}

static int
edt_TapeWrite(NET_StreamClass *stream, char *str, int32 len) {
	edtPrivStruct *obj= (edtPrivStruct *)stream->data_object;	
    obj->out->Write(str,len);
    if ( obj->out->Status() != IStreamOut::EOS_NoError ) {
        return -1;
    }
    return 0;
}

extern "C" NET_StreamClass *EDT_NetToTape(void *data) {
    NET_StreamClass *stream;
    edtPrivStruct *obj;

    stream = (NET_StreamClass *) XP_ALLOC(sizeof(NET_StreamClass));
    if (stream == NULL) {
        return NULL;
    }

    obj = (edtPrivStruct *)XP_ALLOC(sizeof(edtPrivStruct));

    if (obj == NULL) {
        XP_FREE(stream);
        return NULL;
    }

    stream->name = "Editor Tape FS stream";
    stream->complete = edt_TapeComplete;
    stream->abort = edt_TapeAbort;
    stream->is_write_ready = edt_TapeIsReady;
    stream->data_object = obj;
    stream->window_id = NULL; // if this were a real stream we would need a 
                              // url and a window context.
    stream->put_block  = (MKStreamWriteFunc)edt_TapeWrite;

    obj->tapeFS = (ITapeFileSystem *)data;
    obj->out = obj->tapeFS->OpenStream(0);
    
    if (obj->out == NULL) {
        XP_FREE(obj);
        XP_FREE(stream);
        return NULL;
    }

    return stream;
}
#endif
