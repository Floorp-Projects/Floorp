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
// Public interface and shared subsystem data.
//

#ifdef EDITOR

#include "editor.h"

#include "fsfile.h"
// For XP Strings
extern "C" {
#include "xpgetstr.h"
#define WANT_ENUM_STRING_IDS
#include "allxpstr.h"
#undef WANT_ENUM_STRING_IDS
}
#include "xp_str.h"
#include "prefapi.h"
#include "secnav.h"
#include "msgcom.h"
#include "intl_csi.h"

//extern "C" int XP_EDT_HEAD_FAILED;

CBitArray *edt_setNoEndTag = 0;
CBitArray *edt_setWriteEndTag = 0;
CBitArray *edt_setHeadTags = 0;
CBitArray *edt_setSoloTags = 0;
CBitArray *edt_setBlockFormat = 0;
CBitArray *edt_setCharFormat = 0;
CBitArray *edt_setList = 0;
CBitArray *edt_setUnsupported = 0;
CBitArray *edt_setAutoStartBody = 0;
CBitArray *edt_setTextContainer = 0;
CBitArray *edt_setListContainer = 0;
CBitArray *edt_setParagraphBreak = 0;
CBitArray *edt_setFormattedText = 0;
CBitArray *edt_setContainerSupportsAlign = 0;
CBitArray *edt_setIgnoreWhiteSpace = 0;
CBitArray *edt_setSuppressNewlineBefore = 0;
CBitArray *edt_setRequireNewlineAfter = 0;
CBitArray *edt_setContainerBreakConvert = 0;
CBitArray *edt_setContainerHasLineAfter = 0;
CBitArray *edt_setIgnoreBreakAfterClose = 0;

//-----------------------------------------------------------------------------
//  BitArrays
//-----------------------------------------------------------------------------

static XP_Bool bBitsInited = FALSE;

void edt_InitBitArrays() {

    if( bBitsInited ){
        return;
    }
    bBitsInited = TRUE;

    int size = P_MAX + 1; // +1 because CEditRootElements are of type P_MAX.

    // Note -- P_UNKNOWN can't be in these arrays because it has a value of -1.

    edt_setNoEndTag = new CBitArray(      size,
                                    P_TEXT,
                                    P_PARAGRAPH,
                                    P_IMAGE,
                                    P_NEW_IMAGE,
                                    P_INPUT,
                                    P_LIST_ITEM,
                                    P_HRULE,
                                    P_DESC_TITLE,
                                    P_NSDT,
                                    P_DESC_TEXT,
                                    P_LINEBREAK,
                                    P_WORDBREAK,
                                    P_INDEX,
                                    P_BASE,
                                    P_LINK,
                                    P_META,
                                    P_GRID_CELL,
                                    P_INPUT, //cmanske - forms End tag is "forbidden"
                                    BIT_ARRAY_END );

    edt_setWriteEndTag = new CBitArray(   size,
                                    P_HEADER_1,
                                    P_HEADER_2,
                                    P_HEADER_3,
                                    P_HEADER_4,
                                    P_HEADER_5,
                                    P_HEADER_6,
                                    P_PARAGRAPH,
                                    P_ADDRESS,
                                    P_PREFORMAT,
                                    P_LIST_ITEM,
                                    P_DESC_TITLE,
                                    P_DESC_TEXT,
                                    P_NSDT,
                                    BIT_ARRAY_END );


    edt_setHeadTags = new CBitArray(      size,
                                    P_INDEX,
                                    P_BASE,
                                    P_LINK,
                                    BIT_ARRAY_END );

    edt_setSoloTags = new CBitArray(      size,
                                    P_TEXT,
                                    P_IMAGE,
                                    P_NEW_IMAGE,
                                    P_HRULE,
                                    P_LINEBREAK,
                                    P_WORDBREAK,
                                    P_AREA,
                                    P_INPUT,
                                    BIT_ARRAY_END );


    edt_setBlockFormat = new CBitArray(   size,
                                    P_ANCHOR,
                                    P_CENTER,
                                    P_DIVISION,
                                    P_BLINK,
                                    BIT_ARRAY_END );


    //
    // Character formatting tags
    //
    edt_setCharFormat = new CBitArray(    size,
                                    P_BOLD,
                                    P_ITALIC,
                                    P_FONT,
                                    P_EMPHASIZED,
                                    P_STRONG,
                                    P_SAMPLE,
                                    P_KEYBOARD,
                                    P_VARIABLE,
                                    P_ANCHOR,
                                    P_STRIKEOUT,
                                    P_UNDERLINE,
                                    P_FIXED,
                                    P_EMPHASIZED,
                                    P_STRONG,
                                    P_CODE,
                                    P_SAMPLE,
                                    P_KEYBOARD,
                                    P_VARIABLE,
                                    P_CITATION,
                                    P_CENTER,
                                    P_BLINK,
                                    P_BIG,
                                    P_SMALL,
                                    P_SUPER,
                                    P_SUB,
                                    P_NOBREAK,
                                    P_SERVER,
                                    P_SCRIPT,
                                    P_STYLE,
                                    P_SPELL,
                                    P_INLINEINPUT,
                                    P_INLINEINPUTTHICK,
                                    P_INLINEINPUTDOTTED,
                                    BIT_ARRAY_END );

    edt_setList = new CBitArray( size,
                                    P_UNUM_LIST,
                                    P_NUM_LIST,
                                    P_DESC_LIST,
                                    P_MENU,
                                    P_DIRECTORY,
                                    P_BLOCKQUOTE,
                                    P_MQUOTE,
                                    BIT_ARRAY_END );

    // All the tags that are known to navigator, but not to us.
    edt_setUnsupported = new CBitArray( size,
        P_AREA,
        P_CELL,
        P_CERTIFICATE,
        P_ILAYER,
        P_LAYER,
        P_NOLAYER,     //cmanske - added 9/4/97
        P_COLORMAP,
        P_EMBED,
#ifdef SHACK
		P_BUILTIN,
#endif /* SHACK */
        P_FORM,
        P_INPUT,
        P_OPTION,
        P_SELECT,
        P_TEXTAREA,
        P_GRID,
        P_GRID_CELL,
        P_HYPE,
        P_INDEX,
        P_JAVA_APPLET,
        P_KEYGEN,
        P_MAP,
        P_MULTICOLUMN,
        P_NOEMBED,
        P_NOGRIDS,
        P_NOSCRIPT,
        P_OBJECT,
        P_PARAM,
        P_SPACER,
        P_SUBDOC,
        P_WORDBREAK,
        P_NOSCRIPT,
        P_SPAN,
        BIT_ARRAY_END );

    // Any of these tags, when they appear, automatically start the body of
    // the document.
    edt_setAutoStartBody = new CBitArray( size,
        P_TEXT,
        // P_TITLE,
        // P_INDEX,
        // P_BASE,
        // P_LINK,
        P_HEADER_1,
        P_HEADER_2,
        P_HEADER_3,
        P_HEADER_4,
        P_HEADER_5,
        P_HEADER_6,
        P_ANCHOR,
        P_PARAGRAPH,
        P_ADDRESS,
        P_IMAGE,
        P_PLAIN_TEXT,
        P_PLAIN_PIECE,
        P_PREFORMAT,
        P_LISTING_TEXT,
        P_UNUM_LIST,
        P_NUM_LIST,
        P_MENU,
        P_DIRECTORY,
        P_LIST_ITEM,
        P_DESC_LIST,
        P_DESC_TITLE,
        P_DESC_TEXT,
        P_STRIKEOUT,
        P_FIXED,
        P_BOLD,
        P_ITALIC,
        P_EMPHASIZED,
        P_STRONG,
        P_CODE,
        P_SAMPLE,
        P_KEYBOARD,
        P_VARIABLE,
        P_CITATION,
        P_BLOCKQUOTE,
        P_FORM,
        P_INPUT,
        P_SELECT,
        P_OPTION,
        P_TEXTAREA,
        P_HRULE,
        P_LINEBREAK,
        P_WORDBREAK,
        P_NOBREAK,
        P_BASEFONT,
        P_FONT,
        P_BLINK,
        P_NEW_IMAGE,
        P_CENTER,
        P_SUBDOC,
        P_CELL,
        P_TABLE,
        P_CAPTION,
        P_TABLE_ROW,
        P_TABLE_HEADER,
        P_TABLE_DATA,
        P_EMBED,
        // P_BODY,
        // P_META,
        P_COLORMAP,
        P_HYPE,
        P_BIG,
        P_SMALL,
        P_SUPER,
        P_SUB,
        // P_GRID,
        // P_GRID_CELL,
        // P_NOGRIDS,
        P_JAVA_APPLET,
        P_PARAM,
        P_MAP,
        P_AREA,
        P_DIVISION,
        P_KEYGEN,
        // P_SCRIPT,
        // P_NOSCRIPT,
        P_NOEMBED,
        // P_HEAD,
        // P_HTML,
        // P_SERVER,
        P_CERTIFICATE,
        P_ILAYER,
        P_STRIKE,
        P_UNDERLINE,
        P_SPACER,
        P_MULTICOLUMN,
        P_NSCP_CLOSE,
        P_NSCP_OPEN,
        P_BLOCK,
        P_LAYER, //cmanske - fix for bug
        // P_STYLE,
        P_MQUOTE,
        P_SPAN,
        P_SPELL,
        P_INLINEINPUT,
        P_NSCP_REBLOCK,
        P_OBJECT,
        P_NSDT,
        P_INLINEINPUTTHICK,
        P_INLINEINPUTDOTTED,
        P_NOLAYER,
#ifdef SHACK
		P_BUILTIN,
#endif /* SHACK */
        BIT_ARRAY_END );

    edt_setTextContainer = new CBitArray( size,
                                    P_TITLE,
                                    P_HEADER_1,
                                    P_HEADER_2,
                                    P_HEADER_3,
                                    P_HEADER_4,
                                    P_HEADER_5,
                                    P_HEADER_6,
                                    P_PARAGRAPH,
                                    P_ADDRESS,
                                    P_PREFORMAT,
                                    P_LIST_ITEM,
                                    P_DESC_TITLE,
                                    P_DESC_TEXT,
                                    P_NSDT,
                                    BIT_ARRAY_END );

    edt_setContainerSupportsAlign = new CBitArray( size,

                                    P_HEADER_1,
                                    P_HEADER_2,
                                    P_HEADER_3,
                                    P_HEADER_4,
                                    P_HEADER_5,
                                    P_HEADER_6,

    // unfortunately, HRules end paragraph containment, implictly.  So we
    //  either need to change the way we deal with <HR> or change the
    //  code.  until then, we can't generate "align=center" as part of a
    //  paragraph.
                                    //P_PARAGRAPH,

                                    //P_ADDRESS,
                                    //P_PREFORMAT,
                                    //P_LIST_ITEM,
                                    //P_DESC_TITLE,
                                    //P_DESC_TEXT,
                                    BIT_ARRAY_END );


    edt_setListContainer = new CBitArray( size,
                                    P_UNUM_LIST,
                                    P_NUM_LIST,
                                    P_MENU,
                                    P_DIRECTORY,
                                    P_BLOCKQUOTE,
                                    P_MQUOTE,
                                    BIT_ARRAY_END );

    edt_setParagraphBreak = new CBitArray( size,
                                    P_TITLE,
                                    P_HEADER_1,
                                    P_HEADER_2,
                                    P_HEADER_3,
                                    P_HEADER_4,
                                    P_HEADER_5,
                                    P_HEADER_6,
                                    P_PARAGRAPH,
                                    P_LINEBREAK,
                                    P_ADDRESS,
                                    P_PREFORMAT,
                                    P_LIST_ITEM,
                                    P_UNUM_LIST,
                                    P_NUM_LIST,
                                    P_LINEBREAK,
                                    P_HRULE,
                                    P_DESC_LIST,
                                    P_DESC_TITLE,
                                    P_DESC_TEXT,
                                    P_NSDT,
                                    P_MQUOTE,
                                    P_TABLE,
                                    BIT_ARRAY_END );

    edt_setContainerBreakConvert = new CBitArray( size,
                                    P_PARAGRAPH,
                                    P_LIST_ITEM,
                                    P_ADDRESS,
                                    P_LIST_ITEM,
                                    P_DESC_TITLE,
                                    P_DESC_TEXT,
                                    P_NSDT ,
                                    BIT_ARRAY_END );

    edt_setFormattedText = new CBitArray( size,
                                    P_PREFORMAT,
                                    P_PLAIN_PIECE,
                                    P_PLAIN_TEXT,
                                    P_LISTING_TEXT,
                                    BIT_ARRAY_END );

    edt_setIgnoreWhiteSpace = new CBitArray( size,
                                    P_TABLE,
                                    P_TABLE_ROW,
                                    BIT_ARRAY_END );

    //
    // Suppress printing a newline before these tags.
    //
    edt_setSuppressNewlineBefore = new CBitArray(    size,
                                    P_LINEBREAK,
                                    BIT_ARRAY_END );

    //
    // Print a newline after these tags.
    //

    edt_setRequireNewlineAfter = new CBitArray(    size,
                                    P_LINEBREAK,
                                    BIT_ARRAY_END );


    edt_setContainerHasLineAfter = new CBitArray( size, 
                                    P_HEADER_1,
                                    P_HEADER_2,
                                    P_HEADER_3,
                                    P_HEADER_4,
                                    P_HEADER_5,
                                    P_HEADER_6,
                                    P_PREFORMAT,
                                    BIT_ARRAY_END );

   edt_setIgnoreBreakAfterClose = new CBitArray( size,
                                    P_HEADER_1,
                                    P_HEADER_2,
                                    P_HEADER_3,
                                    P_HEADER_4,
                                    P_HEADER_5,
                                    P_HEADER_6,
                                    P_PARAGRAPH,
                                    P_ADDRESS,
                                    P_PREFORMAT,
                                    P_LIST_ITEM,
                                    P_DESC_TITLE,
                                    P_DESC_TEXT,
                                    P_NSDT,
                                    BIT_ARRAY_END );
}


char *edt_GetPathURL( char* pURL ){
    char *pRet = XP_STRDUP(pURL);
    char * pLastSlash = XP_STRRCHR( pRet, '/' );
    if ( pLastSlash == NULL ) {
        XP_FREE(pRet);
        return NULL;
    }
    pLastSlash++;
    *pLastSlash = 0;

    // Actually returning a string with unused space after the /0.
    return pRet;
}

//-----------------------------------------------------------------------------
// External Interface
//-----------------------------------------------------------------------------

char* EDT_GetEmptyDocumentString(){
    return "<html></html>";
}

ED_Buffer* EDT_MakeEditBuffer(MWContext *pContext, XP_Bool bImportText){
    return new CEditBuffer(pContext, bImportText);
}

XP_Bool EDT_HaveEditBuffer(MWContext * pContext){
  return (LO_GetEDBuffer( pContext ) != NULL);
}

ED_FileError EDT_SaveFile( MWContext * pContext,
                           char * pSourceURL,
                           char * pDestURL,
                           XP_Bool   bSaveAs,
                           XP_Bool   bKeepImagesWithDoc,
                           XP_Bool   bAutoAdjustLinks ) {
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer) ED_ERROR_BLOCKED;

    // Create Abstract file system to write to disk.
    char *pDestPathURL = edt_GetPathURL(pDestURL);
    XP_ASSERT(pDestPathURL);
    ITapeFileSystem *tapeFS = new CTapeFSFile(pDestPathURL,pDestURL);
    XP_ASSERT(tapeFS);
    XP_FREE(pDestPathURL);
    // tapeFS freed by CEditSaveObject.

    // go to the newly saved document.
    return pEditBuffer->SaveFile( ED_FINISHED_GOTO_NEW, pSourceURL, tapeFS, bSaveAs,
                                  bKeepImagesWithDoc, bAutoAdjustLinks, 
                                  FALSE, // not AutoSave
                                  NULL ); // default files to send with doc
}

// Declared here since it is only used by edt_publish_head_done and EDT_PublishFile.
// Takes care of cleaning itself up.
class CEditPublishData {
    public:
        CEditPublishData(CEditBuffer *b, ED_SaveFinishedOption finOpt, 
                          char *src, char **inclFiles,
                          char *loc, char *user, char *pass,
                          XP_Bool keepIm, XP_Bool autoAdj)
        :pBuf(b), finishedOpt(finOpt), pSourceURL(XP_STRDUP(src)), ppIncludedFiles(inclFiles),
         pLocation(XP_STRDUP(loc)), pUsername(XP_STRDUP(user)), pPassword(XP_STRDUP(pass)), 
         bKeepImagesWithDoc(keepIm),bAutoAdjustLinks(autoAdj) {} 

        ~CEditPublishData() {XP_FREEIF(pSourceURL); XP_FREEIF(pLocation); XP_FREE(pUsername); XP_FREEIF(pPassword);}
    
    CEditBuffer *pBuf;
    ED_SaveFinishedOption finishedOpt;
    char *pSourceURL;
    char **ppIncludedFiles;

//    char *pDestFullURL;
    char *pLocation;  // destination.
    char *pUsername;
    char *pPassword;

    XP_Bool bKeepImagesWithDoc;
    XP_Bool bAutoAdjustLinks;
};



// Characters that are not allowed in the destination URL for publishing.
// RFC 1808 and RFC 2068, we are erring on the safe side and allowing
// some potentially illegal URLs.
#define ED_BAD_PUBLISH_URL_CHARS "# <>\""

// Return a STATIC string, don't free.
PRIVATE int edt_CheckPublishURL(char *pURL) {
  // NULL or empty string.
  if (!pURL || !(*pURL)) {
    return XP_EDT_PUBLISH_BAD_URL;
  }

  // Bad characters.
  char *p = pURL;
  while (*p) {
    if (XP_IS_CNTRL(*p) || XP_STRCHR(ED_BAD_PUBLISH_URL_CHARS,*p)) {
      return XP_EDT_PUBLISH_BAD_CHAR;
    }      
    p++;
  }

  // Must be ftp:// or http://
  int result = NET_URL_Type(pURL);
  if (result != FTP_TYPE_URL && 
      result != HTTP_TYPE_URL && 
      result != SECURE_HTTP_TYPE_URL) {
    return XP_EDT_PUBLISH_BAD_PROTOCOL;
  }

  // Must not end in a slash, i.e. no filename
  if (pURL[XP_STRLEN(pURL)-1] == '/') {
    return XP_EDT_PUBLISH_NO_FILE;
  }
  
  // The part of the URL after the last slash must have a file extension.
  // Maybe we should be more strict and require that the extension 
  // be .html, .htm, or .shtml.
  char *pLastSlash = XP_STRRCHR(pURL,'/');
  if (!pLastSlash) {
    return XP_EDT_PUBLISH_NO_EXTENSION;
  }

  // Search for '.'.
  if (!XP_STRCHR(pLastSlash,'.')) {
    return XP_EDT_PUBLISH_NO_EXTENSION;
  }

  return 0;
}


/* Check the URL that will be passed into EDT_PublishFile. */
XP_Bool EDT_CheckPublishURL( MWContext *pContext, char *pURL) {
  XP_Bool bRetVal = TRUE;

  int xpStrId = edt_CheckPublishURL(pURL);

  if (xpStrId) {
    char *message = XP_GetString(xpStrId);
    if (message) {
      // Allocate memory for message.
      message = XP_STRDUP(message);

      // Report the error, but give the user the option of trying to publish anyway.
      StrAllocCat(message,"\n\n");
      StrAllocCat(message,XP_GetString(XP_EDT_PUBLISH_ERROR_BODY));
      bRetVal = FE_Confirm(pContext,message);
      XP_FREEIF(message);
    }
    else {
      XP_ASSERT(0);
    }
  }    

  return bRetVal;
}

ED_FileError EDT_PublishFile( MWContext * pContext,
                            ED_SaveFinishedOption finishedOpt,
                           char * pSourceURL,
                           char ** ppIncludedFiles,
                           char * pDestFullURL, /* may not have trailing slash */
                           XP_Bool   bKeepImagesWithDoc,
                           XP_Bool   bAutoAdjustLinks,
                           XP_Bool   bSavePassword ) {
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer) ED_ERROR_BLOCKED;

    int type = NET_URL_Type(pDestFullURL);
    ED_FileError retVal;
    char *pLocation = NULL;
    char *pUsername = NULL;
    char *pPassword = NULL;
    if (!NET_ParseUploadURL( pDestFullURL, &pLocation, 
                             &pUsername, &pPassword )) {
        // This also checks if the URL is complete garbage.
        return ED_ERROR_BAD_URL;
    }                
    
    // Assemble a URL that includes username, but not password and filename
    char * pDestDirectory = EDT_ReplaceFilename(pLocation, NULL, TRUE);
    if( pDestDirectory ){
        char * pPrefLocation = NULL;
        if (NET_MakeUploadURL(&pPrefLocation,pDestDirectory,pUsername,NULL)) {
            // Save as the "last location" in preferences
       		PREF_SetCharPref("editor.publish_last_loc", pPrefLocation);
            
            //  Save the password if user wants us to (and we have one!)
            //  Also set the preference we use for initial state of "save password" checkbox
            if( pPassword && *pPassword ){
                if( bSavePassword ){    
                    // PROBLEM: If password was wrong, user may have
                    // enterred it in prompted dialog, but we dont know that one!
                    PREF_SetCharPref("editor.publish_last_pass",SECNAV_MungeString(pPassword));
			        PREF_SetBoolPref("editor.publish_save_password",TRUE);
                } else {
			        PREF_SetBoolPref("editor.publish_save_password",FALSE);
                }
            }
        }
    }
    
    // FTP  
    if (type == FTP_TYPE_URL) {
        // For ftp, set dest URL to ftp://username@path, so will edit
        // properly.  Explicitly put the username in the URL.
       if (pUsername) {
          char *pUsernameLocation = NULL;
          if (NET_MakeUploadURL(&pUsernameLocation,pLocation,pUsername,NULL)) {
              XP_FREEIF(pLocation);
              pLocation = pUsernameLocation;
          }
       }

       retVal = pEditBuffer->PublishFile(finishedOpt,pSourceURL,ppIncludedFiles,
                    pLocation,pUsername,pPassword,
                    bKeepImagesWithDoc,bAutoAdjustLinks);
    }

    // HTTP
    else if (type == HTTP_TYPE_URL || type == SECURE_HTTP_TYPE_URL) {
        retVal = pEditBuffer->PublishFile(finishedOpt,pSourceURL,ppIncludedFiles,
                    pLocation,pUsername,pPassword,
                    bKeepImagesWithDoc,bAutoAdjustLinks);
    }
    else {
        // We should have already made sure pDestURL is ftp or http before getting here.
        XP_ASSERT(0);
        retVal = ED_ERROR_BLOCKED;
    }

  XP_FREEIF(pLocation);
  XP_FREEIF(pUsername);
  XP_FREEIF(pPassword);
  return retVal;
}

ED_FileError EDT_SaveFileTo( MWContext * pContext,
                           ED_SaveFinishedOption finishedOpt,
                           char * pSourceURL,
                           //ITapeFileSystem *tapeFS,
                           void *_tapeFS,
                           XP_Bool   bKeepImagesWithDoc,
                           XP_Bool   bAutoAdjustLinks ) {
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer) ED_ERROR_BLOCKED;

    ITapeFileSystem *tapeFS = (ITapeFileSystem *)_tapeFS;    
    XP_ASSERT(tapeFS);
    // tapeFS freed by CEditSaveObject.

    // saveAs set to TRUE, why not?
    return pEditBuffer->SaveFile( finishedOpt, pSourceURL, tapeFS, TRUE,
                                  bKeepImagesWithDoc, bAutoAdjustLinks, 
                                  FALSE, // not auto-saving
                                  NULL ); // default files to send with doc
}

char *EDT_GetDocTempDir(MWContext *pContext) {
  GET_EDIT_BUF_OR_RETURN(pContext,pEditBuffer) NULL;
  CEditCommandLog *pLog = CGlobalHistoryGroup::GetGlobalHistoryGroup()->GetLog(pEditBuffer);
  return pLog ? pLog->GetDocTempDir() : 0;
}

char *EDT_CreateDocTempFilename(MWContext *pContext,char *pPrefix,char *pExtension) {
  GET_EDIT_BUF_OR_RETURN(pContext,pEditBuffer) NULL;
  CEditCommandLog *pLog = CGlobalHistoryGroup::GetGlobalHistoryGroup()->GetLog(pEditBuffer);
  return pLog ? pLog->CreateDocTempFilename(pPrefix,pExtension) : 0;
}


void EDT_SaveToTempFile(MWContext *pContext,EDT_SaveToTempCallbackFn doneFn, void *hook) {
  GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);

  // Get a temporary file name.
  CEditCommandLog *pLog = CGlobalHistoryGroup::GetGlobalHistoryGroup()->GetLog(pEditBuffer);
  if (!pLog) {
    return;
  }

// For printing, ".TMP" doesn't work on Windows.
#if defined(XP_WIN) || defined(XP_OS2)
  char *pTemp = pLog->CreateDocTempFilename(NULL,"htm");
#else
  char *pTemp = pLog->CreateDocTempFilename(NULL,"html");
#endif

  if (!pTemp) {
    XP_ASSERT(0);
    (*doneFn)(NULL,hook);
    return;
  }
   
  // pTemp is in xpURL format, so just prepend file://
  char *pFileURL = PR_smprintf("file://%s",pTemp);
  XP_FREE(pTemp);
  if (!pFileURL) {
    XP_ASSERT(0);
    (*doneFn)(NULL,hook);
    return;
  }

  CEditSaveToTempData *pData = new CEditSaveToTempData;    
  if (!pData) {
    XP_ASSERT(0);
    XP_FREE(pFileURL);
    (*doneFn)(NULL,hook);
    return;
  }


  // Fill CEditSaveToTempData structure.  
  pData->doneFn = doneFn;
  pData->hook = hook;
  pData->pFileURL = pFileURL;  // Takes responsibility for this memory.

  History_entry * hist_entry = SHIST_GetCurrent(&(pContext->hist));
	if(!hist_entry || !hist_entry->address) {
    XP_ASSERT(0);
    delete pData;
    (*doneFn)(NULL,hook);
    return;
  }

  /////// ! Duplicating some code from EDT_SaveFile, but I didn't want to change
  /////// ! the interface for EDT_SaveFile(), but we still need to pass in the
  /////// ! CEditSaveToTempData.  We really need a better interface for these
  /////// ! saving functions.  Need some sort of CEditSaveOptions structure.
  // Create Abstract file system to write to disk.
  char *pDestPathURL = edt_GetPathURL(pFileURL);
  XP_ASSERT(pDestPathURL);
  ITapeFileSystem *tapeFS = new CTapeFSFile(pDestPathURL,pFileURL);
  XP_ASSERT(tapeFS);
  XP_FREEIF(pDestPathURL);
  // tapeFS freed by CEditSaveObject.

  if (!tapeFS) {
    XP_ASSERT(0);
    delete pData;
    (*doneFn)(NULL,hook);
    return;
  }  

  char **ppEmptyList = (char **)XP_ALLOC(sizeof(char *));
  XP_ASSERT(ppEmptyList);
  ppEmptyList[0] = NULL;

  pEditBuffer->SaveFile(ED_FINISHED_REVERT_BUFFER, // Don't want to actually change the buffer.
                        hist_entry->address,tapeFS,
                        TRUE, // saveAs is true, probably doesn't matter.
                        FALSE, // Don't move any images.
                        TRUE, // Adjust the links
                        FALSE, // not auto-saving
                        ppEmptyList, // Don't send along any files, even LOCALDATA.
                        pData); // So will callback.
}

void EDT_RemoveTempFile(MWContext *pContext,char *pFileURL) {
  GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);

  char *pFileURLPath = NET_ParseURL(pFileURL,GET_PATH_PART);
  if (pFileURLPath) {
    XP_FileRemove(pFileURLPath,xpURL);
  }
}

#if defined(MOZ_MAIL_COMPOSE) || defined(MOZ_MAIL_NEWS)
PRIVATE void edt_MailDocumentCB(char *pFileURL,void *hook) {
  if (!hook) {
    XP_ASSERT(0);
    return;
  }
  if (!pFileURL) {
    // Failed to save temp file.
    return;
  }
  MWContext *pContext = (MWContext *)hook;
  MSG_MailDocumentURL(pContext,pFileURL);
}

void EDT_MailDocument(MWContext *pContext) {
  GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
  if (EDT_IS_NEW_DOCUMENT(pContext) || EDT_DirtyFlag(pContext)) {
    // Write to temp file and mail that instead.
    EDT_SaveToTempFile(pContext,(EDT_SaveToTempCallbackFn) edt_MailDocumentCB,(void *)pContext);
  }
  else {
    MSG_MailDocumentURL(pContext,NULL);
  }
}
#endif

void EDT_SaveToBuffer(MWContext* pContext, XP_HUGE_CHAR_PTR* pBuffer )
{
      GET_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
      pEditBuffer->WriteToBuffer(pBuffer, FALSE);
}

void EDT_ReadFromBuffer(MWContext* pContext, XP_HUGE_CHAR_PTR pBuffer )
{
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    pEditBuffer->ReadFromBuffer(pBuffer);
}



void EDT_SaveCancel( MWContext *pContext ){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    if( pEditBuffer->m_pSaveObject){
        pEditBuffer->m_pSaveObject->Cancel();
    }
}

void EDT_SetAutoSavePeriod(MWContext *pContext, int32 minutes){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    pEditBuffer->SetAutoSavePeriod( minutes );
}

int32 EDT_GetAutoSavePeriod(MWContext *pContext){
    GET_EDIT_BUF_OR_RETURN(pContext, pEditBuffer) -1;
    return pEditBuffer->GetAutoSavePeriod();
}

//
// we are blocked if we are blocked or saving an object.
//
XP_Bool EDT_IsBlocked( MWContext *pContext ){
    CEditBuffer *pEditBuffer = LO_GetEDBuffer( pContext );
    return (! pEditBuffer || pEditBuffer->IsBlocked() || pEditBuffer->m_pSaveObject != 0);
}

//
// Streams the edit buffer to a big buffer.
//
void EDT_DisplaySource( MWContext *pContext ){
    GET_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);

    //char *pSource = 0;

    pEditBuffer->DisplaySource();

    //pEditBuffer->WriteToBuffer( &pSource );
    //FE_DisplaySource( pContext, pSource );
}


void EDT_DestroyEditBuffer(MWContext * pContext){
    if ( pContext ) {
	    int32 doc_id;
	    lo_TopState *top_state;

	    /*
	     * Get the unique document ID, and retreive this
	     * documents layout state.
	     */
	    doc_id = XP_DOCID(pContext);
	    top_state = lo_FetchTopState(doc_id);
	    if ((top_state == NULL)||(top_state->doc_state == NULL))
	    {
		    return;
	    }
	    CEditBuffer *pEditBuffer = top_state->edit_buffer;
        delete pEditBuffer;
        top_state->edit_buffer = NULL;
    }
}


//
// Hooked into the GetURL state machine.  We do intermitent processing to
//  let the layout engine to the initial processing and fetch all the nested
//  images.
//
// Returns: 1 - Done ok, continuing.
//    0 - Done ok, stopping.
//   -1 - not done, error.
//
intn EDT_ProcessTag(void *data_object, PA_Tag *tag, intn status){
    pa_DocData *pDocData = (pa_DocData*)data_object;
    int parseRet = OK_CONTINUE;
    intn retVal;
    XP_Bool bCreatedEditor = FALSE;

    if( !EDT_IS_EDITOR(pDocData->window_id) ){
        return LO_ProcessTag( data_object, tag, status );
    }

    if( pDocData->edit_buffer == 0 ){
        // HACK ALERT
        // Libnet does not seem to be able to supply us with a reliable
        //  "content_type" in the URL_Struct passed around to the front ends,
        //  so we need to figure out when we are converting a text file into
        //  HTML here.
        // Since we just created our edit buffer for the first tag encountered,
        //  if it is the PLAIN_TEXT type, then we are probably importing a text file
        pDocData->edit_buffer = EDT_MakeEditBuffer( pDocData->window_id, tag->type == P_PLAIN_TEXT );
        bCreatedEditor = pDocData->edit_buffer != NULL;
    }

#if 0
    // if parsing is complete and we didn't end up with anything, force a NOBR
    //  space.
    if( tag == NULL && status == PA_COMPLETE ) {
        CEditRootDocElement* pRoot = pDocData->edit_buffer->m_pRoot;
        XP_Bool bNoData = ! pRoot || ! pRoot->GetChild();
        if ( bNoData ){
            pDocData->edit_buffer->DummyCharacterAddedDuringLoad();
            PA_Tag *pNewTag = XP_NEW( PA_Tag );
            XP_BZERO( pNewTag, sizeof( PA_Tag ) );

            // Non-breaking-spaces cause a memory leak when they are laid out in 3.0b5.
            // See bug 23404.
            // And anyway, we strip out this character.
            // So it can be any character.
            // edt_SetTagData( pNewTag, NON_BREAKING_SPACE_STRING );
            edt_SetTagData( pNewTag, "$" );

            pDocData->edit_buffer->ParseTag( pDocData, pNewTag, status );

            PA_FreeTag( pNewTag );
        }
    }
#endif

    if( tag ){
        // Fix bug 67427 - if the MWContext doesn't have an editor at this point, it
        // means that the editor has been deleted.
        CEditBuffer* pEditBuffer = (CEditBuffer*) LO_GetEDBuffer(pDocData->window_id);
        if ( pEditBuffer != pDocData->edit_buffer && ! bCreatedEditor ){
            return LO_ProcessTag( data_object, tag, status );
        }
        parseRet = pDocData->edit_buffer->ParseTag( pDocData, tag, status );
    }

    if( parseRet != OK_IGNORE ){

#if 0
        //
        // Check to see if the tag went away.  Text without an Edit Element
        //
        // LTNOTE: this case should now return OK_IGNORE, don't check.
        //
        if( tag
                && tag->type == P_TEXT
                && tag->data
                && tag->edit_element == 0 )
            {
                PA_FREE( tag->data );
                return 1;
            }
#endif
        retVal = LO_ProcessTag( data_object, tag, status );
        if( tag == 0 ){
            //pDocData->edit_buffer->FinishedLoad();
        }
        return retVal;
    }
    else{
    	PA_FreeTag(tag);
        return 1;
    }
}

#if 0

static char *loTypeNames[] = {
    "LO_NONE",
    "LO_TEXT",
    "LO_LINEFEED",
    "LO_HRULE",
    "LO_IMAGE",
    "LO_BULLET",
    "LO_FORM_ELE",
    "LO_SUBDOC",
    "LO_TABLE",
    "LO_CELL",
    "LO_EMBED",
#ifdef SHACK
	"LO_BUILTIN",
#endif /* SHACK */
};

//
// Debug routine
//
PRIVATE
void EDT_ShowElement( LO_Element* le ){
    char buff[512];
    CEditTextElement *pText;
    XP_TRACE(("  Type:          %s", loTypeNames[le->type] ));
    XP_TRACE(("  ele_id:        %d", le->lo_any.ele_id ));
    XP_TRACE(("  edit_element   0x%x", le->lo_any.edit_element));
    XP_TRACE(("  edit_offset    %d", le->lo_any.edit_offset));
    if( le->type == LO_TEXT && le->lo_any.edit_element->IsA( P_TEXT ) ){
        pText = le->lo_any.edit_element->Text();
        XP_TRACE(("  lo_element(text)     '%s'", le->lo_text.text ));
        XP_TRACE(("  edit_element(text)   '%s'", pText->GetText()));
        strncpy( buff, pText->GetText()+le->lo_any.edit_offset,
                    le->lo_text.text_len );
        buff[le->lo_text.text_len] = '\0';
        XP_TRACE(("  edit_element text( %d, %d ) = '%s' ",
                le->lo_any.edit_offset, le->lo_text.text_len, buff));
    }
}

#endif

void EDT_SetInsertPoint( ED_Buffer *pBuffer, ED_Element* pElement, int iPosition, XP_Bool bStickyAfter ){
    if( pBuffer->IsBlocked() ){ return; }
    if ( ! pElement->IsLeaf() ){
        XP_ASSERT(FALSE);
        return;
    }
    pBuffer->SetInsertPoint( pElement->Leaf(), iPosition, bStickyAfter );
}


void EDT_SetLayoutElement( ED_Element* pElement, intn iEditOffset,
        intn lo_type, LO_Element *pLayoutElement ){
    // XP_TRACE(("EDT_SetLayoutElement pElement(0x%08x) iEditOffset(%d) pLayoutElement(0x%08x)", pElement, iEditOffset, pLayoutElement));
    if( pElement != 0 && pElement->IsLeaf() ) {
         pElement->Leaf()->SetLayoutElement( iEditOffset, lo_type, pLayoutElement );
    }
}

void EDT_ResetLayoutElement( ED_Element* /* pElement */, intn /* iEditOffset */,
        LO_Element * /*pLayoutElement */ ){
    /* The lifetime of a layout element can be greater than that of the
       corresponding edit element. An example is when you browse to the home page,
       hit edit, then cancel the save.
       When EDT_ResetLayoutElement happens, pElement
       may already have been deleted.

       We were only doing this book keeping to be neat. So it's OK to disable it.
     */
#if 0
    XP_TRACE(("EDT_ResetLayoutElement pElement(0x%08x) iEditOffset(%d) pLayoutElement(0x%08x)", pElement, iEditOffset, pLayoutElement));
    if( pElement != 0 && pElement->IsLeaf()) {
         pElement->Leaf()->ResetLayoutElement( iEditOffset, pLayoutElement );
    }
#endif
}

#ifdef DEBUG
void EDT_VerifyLayoutElement( MWContext *pContext, LO_Element *pLoElement,
        XP_Bool bPrint ){
    CEditElement *pElement = pLoElement->lo_any.edit_element;
    if( pElement ){
        if( !pElement->IsLeaf() ){
            // die here.
        }

        LO_Element *pEle;
        intn iLayoutOffset;
        if( !pElement->Leaf()->GetLOElementAndOffset(
                    pLoElement->lo_any.edit_offset, TRUE,
                    pEle,
                    iLayoutOffset ) ) {
            XP_TRACE((" couldn't find layout element %8x(%d) from edit_element %8x",
                    pLoElement, pLoElement->lo_any.ele_id, pElement ));
            XP_ASSERT(FALSE);
            return;
        }

        if( pEle != pLoElement ){
            XP_TRACE(("Found wrong layout element\n"
                        "    Original Element %8x(%d)\n"
                        "    Found Element %8x(%d)\n",
                    pLoElement, pLoElement->lo_any.ele_id,
                    pEle, pEle != NULL ? pEle->lo_any.ele_id : 0));
            XP_ASSERT(FALSE);
        }
    }
}
#endif //DEBUG

void EDT_DeleteTagChain( PA_Tag* pTag ){
    while( pTag ){
        PA_Tag* pNext = pTag->next;
        PA_FreeTag( pTag );
        pTag = pNext;
    }
}

PA_Tag* EDT_TagCursorGetNext( ED_TagCursor* pCursor ){
    return pCursor->GetNextTag();
}

PA_Tag* EDT_TagCursorGetNextState( ED_TagCursor* pCursor ){
    return pCursor->GetNextTagState();
}

XP_Bool EDT_TagCursorAtBreak( ED_TagCursor* pCursor, XP_Bool* pEndTag ){
    return pCursor->AtBreak(pEndTag);
}

int32 EDT_TagCursorCurrentLine( ED_TagCursor* pCursor ){
    return pCursor->CurrentLine();
}

XP_Bool EDT_TagCursorClearRelayoutState( ED_TagCursor* pCursor ){
    return pCursor->ClearRelayoutState();
}


ED_TagCursor* EDT_TagCursorClone( ED_TagCursor *pCursor ){
    return pCursor->Clone();
}

void EDT_TagCursorDelete( ED_TagCursor* pCursor ){
    delete pCursor;
}

//-----------------------------------------------------------------------------
// FE Keyboard interface.
//-----------------------------------------------------------------------------

EDT_ClipboardResult EDT_KeyDown( MWContext *pContext, uint16 nChar, uint16 /* b */, uint16 /* c */ ){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer) EDT_COP_DOCUMENT_BUSY;
    if( pEditBuffer && nChar >= ' ' ){
        return pEditBuffer->InsertChar(nChar, TRUE);
    }
    return EDT_COP_OK;
}

EDT_ClipboardResult EDT_InsertNonbreakingSpace( MWContext *pContext ){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer) EDT_COP_DOCUMENT_BUSY;
    EDT_ClipboardResult result = EDT_COP_OK;
    const char *p;
    INTL_CharSetInfo c = LO_GetDocumentCharacterSetInfo(pContext);
    // Nonbreaking space can be string for multibyte
    p = INTL_NonBreakingSpace(INTL_GetCSIWinCSID(c));
    for (; *p; p++){
        result = pEditBuffer->InsertChar(*p, TRUE);
        if ( result != EDT_COP_OK) break;
    }
    return result;
}

EDT_ClipboardResult EDT_DeletePreviousChar( MWContext *pContext ){
    GET_EDIT_BUF_OR_RETURN(pContext, pEditBuffer) EDT_COP_DOCUMENT_BUSY;
    return pEditBuffer->DeletePreviousChar();
}

EDT_ClipboardResult EDT_DeleteChar( MWContext *pContext ){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer) EDT_COP_DOCUMENT_BUSY;
    return pEditBuffer->DeleteNextChar();
}

//---------------------------------------------------------------------------
//  Navigation
//---------------------------------------------------------------------------
void EDT_PreviousChar( MWContext *pContext, XP_Bool bSelect ){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    // pEditBuffer->PreviousChar( bSelect );
    pEditBuffer->NavigateChunk( bSelect, LO_NA_CHARACTER, FALSE );
}

void EDT_NextChar( MWContext *pContext, XP_Bool bSelect ){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    pEditBuffer->NavigateChunk( bSelect, LO_NA_CHARACTER, TRUE );
}

void EDT_Up( MWContext *pContext, XP_Bool bSelect ){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    pEditBuffer->UpDown( bSelect, FALSE );
}

void EDT_Down( MWContext *pContext, XP_Bool bSelect ){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    pEditBuffer->UpDown( bSelect, TRUE );
}

#endif //EDITOR

#ifdef EDITOR

void EDT_BeginOfLine( MWContext *pContext, XP_Bool bSelect ){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    pEditBuffer->NavigateChunk( bSelect, LO_NA_LINEEDGE, FALSE );
}

void EDT_EndOfLine( MWContext *pContext, XP_Bool bSelect ){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    pEditBuffer->NavigateChunk( bSelect, LO_NA_LINEEDGE, TRUE );
}

void EDT_BeginOfDocument( MWContext *pContext, XP_Bool bSelect ){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    pEditBuffer->NavigateDocument( bSelect, FALSE );
}

void EDT_EndOfDocument( MWContext *pContext, XP_Bool bSelect ){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    pEditBuffer->NavigateDocument( bSelect, TRUE );
}

void EDT_PageUp( MWContext *pContext, XP_Bool bSelect ){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    pEditBuffer->PageUpDown( bSelect, FALSE);
}

void EDT_PageDown( MWContext *pContext, XP_Bool bSelect ){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    pEditBuffer->PageUpDown( bSelect ,TRUE);
}

void EDT_PreviousWord( MWContext *pContext, XP_Bool bSelect ){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    pEditBuffer->NavigateChunk( bSelect, LO_NA_WORD, FALSE );
}

void EDT_NextWord( MWContext *pContext, XP_Bool bSelect ){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    pEditBuffer->NavigateChunk( bSelect, LO_NA_WORD, TRUE );
}

//CLM: Navigate from cell to cell
void EDT_NextTableCell( MWContext *pContext, XP_Bool bSelect ){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    pEditBuffer->NextTableCell(bSelect, TRUE);
}

void EDT_PreviousTableCell( MWContext *pContext, XP_Bool bSelect ){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    pEditBuffer->NextTableCell(bSelect, FALSE);
}

//
// Kind of a bizarre routine, Order of calls is as follows:
//  FE_OnMouseDown      - determines x and y in layout coords. calls:
//  EDT_PositionCaret   - clears movement and calls
//  LO_PositionCaret    - figures out which layout element is under caret and calls
//  EDT_SetInsertPoint  - which sets the edit element and offset and then calls
//  FE_DisplayTextCaret - which actually positions the caret.
//
// There are some more levels in there, but from an external view, this is how
//  it occurs.
//
void EDT_PositionCaret( MWContext *pContext, int32 x, int32 y ){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    pEditBuffer->PositionCaret(x, y);
}

void EDT_DeleteSelectionAndPositionCaret( MWContext *pContext, int32 x, int32 y ){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    pEditBuffer->DeleteSelectionAndPositionCaret(x, y);
}

XP_Bool EDT_PositionDropCaret( MWContext *pContext, int32 x, int32 y ){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer) FALSE;
    return pEditBuffer->PositionDropCaret(x, y);
}

XP_Bool EDT_StartDragTable( MWContext *pContext, int32 x, int32 y ){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer) FALSE;
    return pEditBuffer->StartDragTable(x, y);
}

void EDT_StopDragTable( MWContext *pContext )
{
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    pEditBuffer->StopDragTable();
}

XP_Bool EDT_IsDraggingTable( MWContext *pContext )
{
    GET_EDIT_BUF_OR_RETURN(pContext, pEditBuffer) FALSE;
    return pEditBuffer->IsDraggingTable();
}

void EDT_DoubleClick( MWContext *pContext, int32 x, int32 y ){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    pEditBuffer->StartSelection(x,y, TRUE);
}

void EDT_SelectObject( MWContext *pContext, int32 x, int32 y ){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    pEditBuffer->SelectObject(x,y);
}

void EDT_WindowScrolled( MWContext *pContext ){
    GET_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    pEditBuffer->WindowScrolled();
}

EDT_ClipboardResult EDT_ReturnKey( MWContext *pContext ){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer) EDT_COP_DOCUMENT_BUSY;
    // TRUE = user is typing, FALSE = don't indent after return action
    return pEditBuffer->ReturnKey(TRUE, FALSE);
}

/* Do normal Return (Enter) key processing, then indent one level */
EDT_ClipboardResult EDT_ReturnKeyAndIndent( MWContext *pContext ){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer) EDT_COP_DOCUMENT_BUSY;
    // 2nd TRUE = indent after return action
    return pEditBuffer->ReturnKey(TRUE, TRUE);
}

EDT_ClipboardResult EDT_TabKey( MWContext *pContext, XP_Bool bForward, XP_Bool bForceTabChar ){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer) EDT_COP_DOCUMENT_BUSY;
    return pEditBuffer->TabKey(bForward, bForceTabChar);
}

void EDT_Indent( MWContext *pContext ){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    {
        CEditSelection selection;
        pEditBuffer->GetSelection(selection);
        if ( selection.CrossesSubDocBoundary() ) {
            return;
        }
    }
    pEditBuffer->BeginBatchChanges(kIndentCommandID);
    pEditBuffer->Indent();
    pEditBuffer->EndBatchChanges();
}

void EDT_Outdent( MWContext *pContext ){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    pEditBuffer->BeginBatchChanges(kIndentCommandID);
    pEditBuffer->Outdent();
    pEditBuffer->EndBatchChanges();
}

void EDT_RemoveList( MWContext *pContext ){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    // Repeat removing indent until last Unnumbered list is gone
    pEditBuffer->BeginBatchChanges(kParagraphAlignCommandID);
    pEditBuffer->MorphContainer( P_NSDT );

    EDT_ListData * pListData;
    while ( (pListData = pEditBuffer->GetListData()) != NULL ){
        EDT_FreeListData(pListData);
        pEditBuffer->Outdent();
    }
    pEditBuffer->EndBatchChanges();
}

void EDT_ToggleList(MWContext *pContext, intn iTagType)
{
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    EDT_BeginBatchChanges(pContext);
    pEditBuffer->ToggleList(iTagType);
    EDT_EndBatchChanges(pContext);
}

XP_Bool EDT_GetToggleListState(MWContext *pContext, intn iTagType)
{
    GET_EDIT_BUF_OR_RETURN(pContext, pEditBuffer) FALSE;
   	TagType nParagraphFormat = EDT_GetParagraphFormatting( pContext );
    EDT_ListData * pData = NULL;
    XP_Bool bIsMyList = FALSE;
    // Description list is a special case - it doesn't (and shouldn't)
    //   have <LI> items. It should contain <DT> (Desc.Title) and 
    //   <DD> (Desc. text) items.
    if ( nParagraphFormat == P_LIST_ITEM || iTagType == P_DESC_LIST ) {
        pData = EDT_GetListData(pContext);
        bIsMyList = ( pData && pData->iTagType == iTagType );
    }
    if ( pData ) {
        EDT_FreeListData( pData );
    }
    return bIsMyList;
}

void EDT_SetParagraphAlign( MWContext *pContext, ED_Alignment eAlign ){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    pEditBuffer->BeginBatchChanges(kParagraphAlignCommandID);
    pEditBuffer->SetParagraphAlign( eAlign );
    pEditBuffer->EndBatchChanges();
}

ED_Alignment EDT_GetParagraphAlign( MWContext *pContext){
    GET_EDIT_BUF_OR_RETURN(pContext, pEditBuffer) ED_ALIGN_LEFT;
    return pEditBuffer->GetParagraphAlign();
}

void EDT_SetTableAlign( MWContext *pContext, ED_Alignment eAlign ){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    pEditBuffer->BeginBatchChanges(kParagraphAlignCommandID);
    pEditBuffer->SetTableAlign( eAlign );
    pEditBuffer->EndBatchChanges();
}

void EDT_MorphContainer( MWContext *pContext, TagType t ){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    pEditBuffer->BeginBatchChanges(kParagraphAlignCommandID);
	if( t == P_BLOCKQUOTE ||
        t == P_DIRECTORY ||
        t == P_MENU ||
        t == P_DESC_LIST){
        pEditBuffer->MorphListContainer( t );
    } else {
        pEditBuffer->MorphContainer( t );
    }
    pEditBuffer->EndBatchChanges();
}

void EDT_FormatCharacter( MWContext *pContext, ED_TextFormat tf ){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    pEditBuffer->FormatCharacter( tf );
}

EDT_CharacterData* EDT_GetCharacterData( MWContext *pContext ){
    GET_EDIT_BUF_OR_RETURN(pContext, pEditBuffer) 0;
    return pEditBuffer->GetCharacterData();
}

void EDT_SetCharacterData( MWContext *pContext, EDT_CharacterData *pData ){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    pEditBuffer->SetCharacterData( pData );
}

EDT_CharacterData* EDT_NewCharacterData(){
    EDT_CharacterData* pData = XP_NEW( EDT_CharacterData );
    XP_BZERO( pData, sizeof( EDT_CharacterData ));
    return pData;
}

void EDT_FreeCharacterData( EDT_CharacterData *pData ){
    if( pData->pHREFData ){
        EDT_FreeHREFData( pData->pHREFData );
    }
    if( pData->pColor ){
        XP_FREE( pData->pColor );
    }
    if( pData->pFontFace ){
        XP_FREE( pData->pFontFace );
    }
    XP_FREE( pData );
}


#if 0
PRIVATE
ED_BitArray* EDT_BitArrayCreate(){
    return new CBitArray(P_MAX, 0);
}

PRIVATE
void EDT_BitArrayDestroy( ED_BitArray* pBitArray){
    delete pBitArray;
}

PRIVATE
XP_Bool EDT_BitArrayTest( ED_BitArray* pBitArray, TagType t ){
    return (*pBitArray)[t];
}
#endif

ED_TextFormat EDT_GetCharacterFormatting( MWContext *pContext){
    GET_EDIT_BUF_OR_RETURN(pContext, pEditBuffer) 0;
    return pEditBuffer->GetCharacterFormatting();
}

/* CM: "Current" is superfluous! */
ED_ElementType EDT_GetCurrentElementType( MWContext *pContext){
    GET_EDIT_BUF_OR_RETURN(pContext, pEditBuffer) ED_ELEMENT_TEXT;
    return pEditBuffer->GetCurrentElementType();
}

TagType EDT_GetParagraphFormatting( MWContext *pContext ){
    GET_EDIT_BUF_OR_RETURN(pContext, pEditBuffer) P_UNKNOWN;
    return pEditBuffer->GetParagraphFormatting( );
}

int EDT_GetFontSize( MWContext *pContext ){
    GET_EDIT_BUF_OR_RETURN(pContext, pEditBuffer) 0;
    return pEditBuffer->GetFontSize();
}

void EDT_DecreaseFontSize( MWContext *pContext ){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    // Size is a relative change, signaled by TRUE param
    pEditBuffer->SetFontSize(-1, TRUE);
}
void EDT_IncreaseFontSize( MWContext *pContext ){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    // Size is a relative change, signalled by TRUE param
    pEditBuffer->SetFontSize(1, TRUE);
}
void EDT_SetFontSize( MWContext *pContext, int iSize ){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    pEditBuffer->SetFontSize(iSize, FALSE);
}

int EDT_GetFontPointSize( MWContext *pContext ){
    GET_EDIT_BUF_OR_RETURN(pContext, pEditBuffer) 0;
    return pEditBuffer->GetFontPointSize();
}

void EDT_SetFontPointSize( MWContext *pContext, int iPoints ){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    pEditBuffer->SetFontPointSize(iPoints);
}

int EDT_GetFontFaceIndex(MWContext *pContext){
    GET_EDIT_BUF_OR_RETURN(pContext, pEditBuffer) -1;
    return pEditBuffer->GetFontFaceIndex();
}

char * EDT_GetFontFace(MWContext *pContext){
    GET_EDIT_BUF_OR_RETURN(pContext, pEditBuffer) NULL;
    return pEditBuffer->GetFontFace();
}

//
// Some macros for packing and unpacking colors.
//

XP_Bool EDT_GetFontColor( MWContext *pContext, LO_Color* pDestColor ){
    GET_EDIT_BUF_OR_RETURN(pContext, pEditBuffer) FALSE;
    ED_Color i = pEditBuffer->GetFontColor();
    if( i.IsDefined() ){
        pDestColor->red = EDT_RED(i);
        pDestColor->green = EDT_GREEN(i);
        pDestColor->blue = EDT_BLUE(i);
        return TRUE;
    }
    else {
        return FALSE;
    }
}

void EDT_SetFontColor( MWContext *pContext, LO_Color *pColor){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    if( pColor ){
        pEditBuffer->SetFontColor( EDT_LO_COLOR( pColor ) );
    }
    else {
        pEditBuffer->SetFontColor(ED_Color::GetUndefined());
    }
}

ED_ElementType EDT_GetBackgroundColor( MWContext *pContext, LO_Color *pColor ){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer) ED_ELEMENT_NONE;
    return pEditBuffer->GetBackgroundColor(pColor);
}

void EDT_SetBackgroundColor( MWContext *pContext, LO_Color *pColor){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    pEditBuffer->SetBackgroundColor(pColor);
}

void EDT_StartSelection(MWContext *pContext, int32 x, int32 y){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    pEditBuffer->StartSelection(x,y, FALSE);
}

void EDT_ExtendSelection(MWContext *pContext, int32 x, int32 y){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    pEditBuffer->ExtendSelection(x,y);
}

void EDT_EndSelection(MWContext *pContext, int32 x, int32 y){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    pEditBuffer->EndSelection(x, y);
}

void EDT_SelectAll(MWContext *pContext){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    pEditBuffer->SelectAll();
}

void EDT_SelectTable(MWContext *pContext){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    pEditBuffer->SelectTable();
}

void EDT_SelectTableCell(MWContext *pContext){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    pEditBuffer->SelectTableCell();
}

void EDT_ClearTableAndCellSelection(MWContext *pContext)
{
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    pEditBuffer->ClearTableAndCellSelection();
}

XP_Bool EDT_IsTableSelected(MWContext *pContext)
{
    GET_EDIT_BUF_OR_RETURN(pContext, pEditBuffer) FALSE;
    return pEditBuffer->IsTableSelected();
}

int EDT_GetSelectedCellCount(MWContext *pContext)
{
    GET_EDIT_BUF_OR_RETURN(pContext, pEditBuffer) 0;
    return pEditBuffer->GetSelectedCellCount();
}

void EDT_ClearCellSelectionIfNotInside(MWContext *pContext)
{
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    pEditBuffer->ClearCellSelectionIfNotInside();
}

void EDT_StartSpecialCellSelection(MWContext *pContext, EDT_TableCellData *pCellData)
{
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    pEditBuffer->StartSpecialCellSelection(pCellData);
}

void EDT_ClearSpecialCellSelection(MWContext *pContext)
{
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    pEditBuffer->ClearSpecialCellSelection();
}

void EDT_ClearSelection(MWContext *pContext){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    CEditSelection selection;
    pEditBuffer->GetSelection(selection);
    selection.m_end = selection.m_start; // Leave cursor at left, so table cell change data works.
    CSetSelectionCommand* pSelectCommand = new CSetSelectionCommand(pEditBuffer, selection);
    pEditBuffer->AdoptAndDo(pSelectCommand);
}

EDT_ClipboardResult EDT_InsertText( MWContext *pContext, char *pText ) {
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer) EDT_COP_DOCUMENT_BUSY;
    pEditBuffer->DeleteSelection();
    pEditBuffer->StartTyping(TRUE);
    EDT_ClipboardResult result = pEditBuffer->PasteText( pText, FALSE, TRUE, TRUE ,TRUE);
    return result;
}

EDT_ClipboardResult EDT_PasteText( MWContext *pContext, char *pText ) {
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer) EDT_COP_DOCUMENT_BUSY;
    pEditBuffer->BeginBatchChanges(kPasteTextCommandID);
    EDT_ClipboardResult result;

    XP_Bool bPasteAsTable = FALSE;

    // Determine format of paste text - use tabs as cell delimiters, <CR><LF> as end of row
    intn iCols;
    intn iRows;
    XP_Bool bEqualCols = pEditBuffer->CountRowsAndColsInPasteText(pText, &iRows, &iCols);
    // Ask user for approval only if we have a well-formed table with > 1 column to paste
    if( bEqualCols && iCols > 1 && iRows > 0 )
    {
        char *pMessage = NULL;
        pMessage = PR_sprintf_append( pMessage, XP_GetString(XP_EDT_CAN_PASTE_AS_TABLE), iRows, iCols);
        if( pEditBuffer->IsInsertPointInTableCell() )
        {
            // Give message about pasting into an existing table
            pMessage = PR_sprintf_append( pMessage,  XP_GetString(XP_EDT_REPLACE_CELLS));

            if( FE_Confirm(pEditBuffer->GetContext(), pMessage) )
            {
                //TODO: Implement PasteIntoTable(TRUE); // TRUE = delete existing cells
                bPasteAsTable = TRUE;
            } else {
                //TODO: Implement PasteIntoTable(FALSE); // FALSE = append to existing cells
                // For now, just paste into one cell
                //bPasteAsTable = TRUE;
            }
            bPasteAsTable = TRUE;
        } else {
            // Give message about pasting as a new table
            pMessage = PR_sprintf_append( pMessage,  XP_GetString(XP_EDT_PASTE_AS_TABLE));
            if( FE_Confirm(pEditBuffer->GetContext(), pMessage) )
            {
                pEditBuffer->PasteTextAsNewTable(pText, iRows, iCols);
                bPasteAsTable = TRUE;
            }
        }
        XP_FREEIF(pMessage);
    }
    else 
    {
        // No selected table cells Delete any existing selection so paste replaces it
        pEditBuffer->DeleteSelection();
    }

    // If we didn't do any special table pasting, do regular paste at caret location
    if( !bPasteAsTable )
        result = pEditBuffer->PasteText( pText, FALSE, FALSE, TRUE ,TRUE);
    pEditBuffer->EndBatchChanges();
    return result;
}

EDT_ClipboardResult EDT_PasteQuoteBegin( MWContext *pContext, XP_Bool bHTML ) {
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer) EDT_COP_DOCUMENT_BUSY;
    EDT_ClipboardResult result = pEditBuffer->PasteQuoteBegin( bHTML );
    return result;
}

EDT_ClipboardResult EDT_PasteQuote( MWContext *pContext, char *pText ) {
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer) EDT_COP_DOCUMENT_BUSY;
    EDT_ClipboardResult result = pEditBuffer->PasteQuote( pText );
    return result;
}

EDT_ClipboardResult EDT_PasteQuoteINTL( MWContext *pContext, char *pText, int16 csid ) {
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer) EDT_COP_DOCUMENT_BUSY;
    EDT_ClipboardResult result = pEditBuffer->PasteQuoteINTL( pText, csid );
    return result;
}

EDT_ClipboardResult EDT_PasteQuoteEnd( MWContext *pContext ) {
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer) EDT_COP_DOCUMENT_BUSY;
    EDT_ClipboardResult result = pEditBuffer->PasteQuoteEnd();
    return result;
}

EDT_ClipboardResult EDT_PasteHTML( MWContext *pContext, char *pText ){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer) EDT_COP_DOCUMENT_BUSY;
    pEditBuffer->BeginBatchChanges(kPasteHTMLCommandID);
    pEditBuffer->DeleteSelection();
    EDT_ClipboardResult result = pEditBuffer->PasteHTML( pText, FALSE );
    pEditBuffer->EndBatchChanges();
    return result;
}

EDT_ClipboardResult EDT_CopySelection( MWContext *pContext, char **ppText, int32* pTextLen,
            XP_HUGE_CHAR_PTR* ppHtml, int32* pHtmlLen){
    GET_EDIT_BUF_OR_RETURN(pContext, pEditBuffer) EDT_COP_DOCUMENT_BUSY;
    return pEditBuffer->CopySelection( ppText, pTextLen, (char **)ppHtml, pHtmlLen );
}

EDT_ClipboardResult EDT_CutSelection( MWContext *pContext, char **ppText, int32* pTextLen,
            XP_HUGE_CHAR_PTR* ppHtml, int32* pHtmlLen){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer) EDT_COP_DOCUMENT_BUSY;
    EDT_ClipboardResult result = pEditBuffer->CutSelection( ppText, pTextLen, (char **)ppHtml, pHtmlLen );
    return result;
}

char *EDT_GetTabDelimitedTextFromSelectedCells( MWContext *pContext )
{
    GET_EDIT_BUF_OR_RETURN(pContext, pEditBuffer) NULL;
    return pEditBuffer->GetTabDelimitedTextFromSelectedCells();
}

XP_Bool EDT_CanConvertTextToTable(MWContext *pMWContext)
{
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pMWContext, pEditBuffer) FALSE;
    return pEditBuffer->CanConvertTextToTable();
}

void EDT_ConvertTextToTable(MWContext *pMWContext, intn iColumns)
{
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pMWContext, pEditBuffer);
    pEditBuffer->ConvertTextToTable(iColumns);
}

/* Convert the table into text - unravel existing paragraphs in cells */
void EDT_ConvertTableToText(MWContext *pMWContext)
{
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pMWContext, pEditBuffer);
    pEditBuffer->ConvertTableToText();
}

/* Save the character and paragraph style of selection or at caret */
void EDT_CopyStyle(MWContext *pMWContext)
{
    GET_EDIT_BUF_OR_RETURN(pMWContext, pEditBuffer);
    pEditBuffer->CopyStyle();
}

/* TRUE if no mouse actions taken since last EDT_CopyStyle call */
XP_Bool EDT_CanPasteStyle(MWContext *pMWContext)
{
    GET_EDIT_BUF_OR_RETURN(pMWContext, pEditBuffer) FALSE;
    return pEditBuffer->CanPasteStyle();
}

/* Apply the style to selection or at caret */
void EDT_PasteStyle(MWContext *pMWContext, XP_Bool bApplyStyle)
{
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pMWContext, pEditBuffer);
    pEditBuffer->PasteStyle(bApplyStyle);
}


XP_Bool EDT_DirtyFlag( MWContext *pContext ){
    GET_EDIT_BUF_OR_RETURN(pContext, pEditBuffer) FALSE;
    return pEditBuffer->GetDirtyFlag();
}

void EDT_SetDirtyFlag( MWContext *pContext, XP_Bool bValue ){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    if ( bValue ) {
        XP_ASSERT(FALSE); // Use BeginBatchChanges instead.
    }
    else {
        pEditBuffer->DocumentStored();
    }
}

EDT_ClipboardResult EDT_CanCut(MWContext *pContext, XP_Bool bStrictChecking){
    GET_EDIT_BUF_OR_RETURN(pContext, pEditBuffer) EDT_COP_DOCUMENT_BUSY;
    return pEditBuffer->CanCut( bStrictChecking );
}

EDT_ClipboardResult EDT_CanCopy(MWContext *pContext, XP_Bool bStrictChecking){
    GET_EDIT_BUF_OR_RETURN(pContext, pEditBuffer) EDT_COP_DOCUMENT_BUSY;
    return pEditBuffer->CanCopy( bStrictChecking );
}

EDT_ClipboardResult EDT_CanPaste(MWContext *pContext, XP_Bool bStrictChecking){
    GET_EDIT_BUF_OR_RETURN(pContext, pEditBuffer) EDT_COP_DOCUMENT_BUSY;
    return pEditBuffer->CanPaste( bStrictChecking );
}

XP_Bool EDT_CanSetHREF( MWContext *pContext ){
    GET_EDIT_BUF_OR_RETURN(pContext, pEditBuffer) FALSE;
    return pEditBuffer->CanSetHREF( );
}

char *EDT_GetHREF( MWContext *pContext ){
    GET_EDIT_BUF_OR_RETURN(pContext, pEditBuffer) NULL;
    return pEditBuffer->GetHREF( );
}

char *EDT_GetHREFText(MWContext *pContext){
    GET_EDIT_BUF_OR_RETURN(pContext, pEditBuffer) NULL;
    return pEditBuffer->GetHREFText();
}

void EDT_SetHREF(MWContext *pContext, char *pHREF ){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    pEditBuffer->SetHREF( pHREF, 0 );
}

/*
 * Returns colors of all the different fonts.
 * Must call XP_FREE( pDest ) after use.
*/
int EDT_GetExtraColors( MWContext * /* pContext */, LO_Color ** /* pDest */ ){
    return 0;
}

void EDT_RefreshLayout( MWContext *pContext ){
    LO_RefetchWindowDimensions( pContext );
    GET_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    pEditBuffer->RefreshLayout();
}

XP_Bool EDT_IsSelected( MWContext *pContext ){
    GET_EDIT_BUF_OR_RETURN(pContext, pEditBuffer) FALSE;
    return pEditBuffer->IsSelected();
}

//  Check current file-update time and
//  return TRUE if it is different
XP_Bool EDT_IsFileModified( MWContext* pContext ){
    GET_EDIT_BUF_OR_RETURN(pContext, pEditBuffer) FALSE;
    return pEditBuffer->IsFileModified();
}

//   This should probably be moved into CEditBuffer,
//     Note that we return FALSE if we are text type
//     and not selected, even if caret is in a link,
//     so don't use this to test for link.
// TODO: Move this into CEditBuffer::SelectionContainsLink()
//       after CharacterData functions are implemented
XP_Bool EDT_SelectionContainsLink( MWContext *pContext ){
    if( EDT_IsSelected(pContext) ||
        EDT_GetSelectedTableElement(pContext, NULL) )
    {
        ED_ElementType type = EDT_GetCurrentElementType(pContext);
        // If we have a Table selected, then we ignore character actions,
        //  but we do look into Table Rows, columns, and cells
        if( type == ED_ELEMENT_SELECTION || type > ED_ELEMENT_TABLE)
        {
            EDT_CharacterData *pData = EDT_GetCharacterData(pContext);
            if( pData )
            {
                // We aren't certain about HREF state accross
                //  entire selection if mask bit is off
                XP_Bool bUncertain = ( (pData->mask & TF_HREF) == 0 ||
                                    ((pData->mask & TF_HREF) &&
                                     (pData->values & TF_HREF)) );
                EDT_FreeCharacterData(pData);
                return bUncertain;
            }
        }
        else if ( type == ED_ELEMENT_IMAGE )
        {
            LO_Element *pStart;
            //
            // Grab the current selection.
            //
            LO_GetSelectionEndPoints( pContext, &pStart, 0, 0, 0, 0, 0 );
            // Check if image has an HREF
            return (pStart && pStart->lo_image.anchor_href != NULL );
        }
    }
    return FALSE;
}

void EDT_DropHREF( MWContext *pContext, char *pHref, char* pTitle, int32 x,
            int32 y ){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    EDT_BeginBatchChanges(pContext);
    EDT_PositionCaret( pContext, x, y );
    EDT_PasteText( pContext, pHref );
    EDT_PasteText( pContext, pTitle );
    EDT_EndBatchChanges(pContext);
}

EDT_ClipboardResult EDT_PasteHREF( MWContext *pContext, char **ppHref, char **ppTitle, int iCount){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer) EDT_COP_DOCUMENT_BUSY;
    EDT_ClipboardResult result = EDT_COP_DOCUMENT_BUSY;
    pEditBuffer->BeginBatchChanges(kPasteHREFCommandID);
    pEditBuffer->DeleteSelection();
    result = pEditBuffer->PasteHREF( ppHref, ppTitle, iCount );
    pEditBuffer->EndBatchChanges();
    return result;
}

XP_Bool EDT_CanDropHREF( MWContext * pContext, int32 /* x */, int32 /* y */){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer) FALSE;
    return TRUE;
}

EDT_HREFData *EDT_GetHREFData( MWContext *pContext ){
    GET_EDIT_BUF_OR_RETURN(pContext, pEditBuffer) 0;
    EDT_HREFData* pData = EDT_NewHREFData();
    if(pData){
        pEditBuffer->GetHREFData(pData);
    }
    return pData;
}

void EDT_SetHREFData( MWContext *pContext, EDT_HREFData *pData ){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    /* Allow clearing existing HREF with NULL pURL */
    if(pData /*&& pData->pURL*/){
        pEditBuffer->SetHREFData(pData);
    }
}

EDT_HREFData *EDT_NewHREFData() {
    EDT_HREFData* pData = XP_NEW( EDT_HREFData );
    if( pData ){
        XP_BZERO( pData, sizeof( EDT_HREFData ) );
        return pData;
    }
    return NULL;
}

EDT_HREFData *EDT_DupHREFData( EDT_HREFData *pOldData) {
    EDT_HREFData* pData = EDT_NewHREFData();
    if( pOldData ){
        if(pOldData->pURL) pData->pURL = XP_STRDUP( pOldData->pURL );
        if(pOldData->pExtra) pData->pExtra = XP_STRDUP( pOldData->pExtra );
    }
    return pData;
}


void EDT_FreeHREFData(  EDT_HREFData *pData ) {
    //Move this to an EditBuffer function?
    if(pData){
        if(pData->pURL) XP_FREE(pData->pURL);
        if(pData->pExtra) XP_FREE(pData->pExtra);
        XP_FREE(pData);
    }
}

void EDT_FinishedLayout( MWContext *pContext ){
	/* we know that the buffer is not ready */
	/* we're actually not "ready" here because FinishedLoad will call */
	/* the timer which will ensure that FinishedLoad2 gets called which */
	/* completes the initialization of pEditBuffer */
    GET_EDIT_BUF_OR_RETURN_READY_OR_NOT(pContext, pEditBuffer);
    pEditBuffer->FinishedLoad();
}

// Page

EDT_PageData *EDT_GetPageData( MWContext *pContext ){
    GET_EDIT_BUF_OR_RETURN_READY_OR_NOT(pContext, pEditBuffer) NULL;
    return pEditBuffer->GetPageData();
}

EDT_PageData *EDT_NewPageData(){
    EDT_PageData* pData = XP_NEW( EDT_PageData );
    if( pData ){
        XP_BZERO( pData, sizeof( EDT_PageData ) );
        pData->bKeepImagesWithDoc = TRUE;
        return pData;
    }
    return NULL;
}

void EDT_SetPageData( MWContext *pContext, EDT_PageData *pData ){
    GET_WRITABLE_EDIT_BUF_OR_RETURN_READY_OR_NOT(pContext, pEditBuffer);
    pEditBuffer->AdoptAndDo(new CChangePageDataCommand(pEditBuffer, kChangePageDataCommandID));
    pEditBuffer->SetPageData( pData );
}

void EDT_FreePageData( EDT_PageData *pData ){
    CEditBuffer::FreePageData( pData );
}

void EDT_SetImageAsBackground( MWContext *pContext ){
    GET_WRITABLE_EDIT_BUF_OR_RETURN_READY_OR_NOT(pContext, pEditBuffer);
    pEditBuffer->SetImageAsBackground();
}

// Meta

intn EDT_MetaDataCount( MWContext *pContext ){
    GET_EDIT_BUF_OR_RETURN(pContext, pEditBuffer) 0;
    return pEditBuffer->MetaDataCount();
}

EDT_MetaData* EDT_GetMetaData( MWContext *pContext, intn n ){
    GET_EDIT_BUF_OR_RETURN_READY_OR_NOT(pContext, pEditBuffer) NULL;
    return pEditBuffer->GetMetaData( n );
}

EDT_MetaData* EDT_NewMetaData(){
    EDT_MetaData *pData = XP_NEW( EDT_MetaData );
    if( pData ){
        XP_BZERO( pData, sizeof( EDT_MetaData ) );
        return pData;
    }
    return NULL;
}

void EDT_SetMetaData( MWContext *pContext, EDT_MetaData *pMetaData ){
    GET_WRITABLE_EDIT_BUF_OR_RETURN_READY_OR_NOT(pContext, pEditBuffer);
    // CSetMetaDataCommand actually performs the operation in the
    // constructor of the command. So in order for save-based undo to
    // work, we must wrap its constructor in BeginBatchChanges.
    pEditBuffer->BeginBatchChanges(kSetMetaDataCommandID);
    pEditBuffer->AdoptAndDo(new CSetMetaDataCommand(pEditBuffer, pMetaData, FALSE, kSetMetaDataCommandID));
    pEditBuffer->EndBatchChanges();
}

void EDT_DeleteMetaData( MWContext *pContext, EDT_MetaData *pMetaData ){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    // CSetMetaDataCommand actually performs the operation in the
    // constructor of the command. So in order for save-based undo to
    // work, we must wrap its constructor in BeginBatchChanges.
    pEditBuffer->BeginBatchChanges(kDeleteMetaDataCommandID);
    pEditBuffer->AdoptAndDo(new CSetMetaDataCommand(pEditBuffer, pMetaData, TRUE, kDeleteMetaDataCommandID));
    pEditBuffer->EndBatchChanges();
}

void EDT_FreeMetaData( EDT_MetaData *pMetaData ){
    CEditBuffer::FreeMetaData( pMetaData );
}

// Image

EDT_ImageData *EDT_GetImageData( MWContext *pContext ){
    GET_EDIT_BUF_OR_RETURN(pContext, pEditBuffer) NULL;
    return pEditBuffer->GetImageData();
}

int32 EDT_GetDefaultBorderWidth( MWContext *pContext ){
    GET_EDIT_BUF_OR_RETURN(pContext, pEditBuffer) NULL;
    return pEditBuffer->GetDefaultBorderWidth();
}

void EDT_SetImageData( MWContext *pContext, EDT_ImageData *pData, XP_Bool bKeepImagesWithDoc ){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    pEditBuffer->BeginBatchChanges(kSetImageDataCommandID);
    pEditBuffer->SetImageData( pData, bKeepImagesWithDoc );
    pEditBuffer->EndBatchChanges();
}


EDT_ImageData *EDT_NewImageData(){
    return edt_NewImageData( );
}

void EDT_FreeImageData( EDT_ImageData *pData ){
    edt_FreeImageData( pData );
}

void EDT_InsertImage( MWContext *pContext, EDT_ImageData *pData, XP_Bool bKeepImagesWithDoc ){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    pEditBuffer->BeginBatchChanges(kInsertImageCommandID);
    pEditBuffer->LoadImage( pData, bKeepImagesWithDoc, FALSE );
    pEditBuffer->EndBatchChanges();
}

void EDT_ImageLoadCancel( MWContext *pContext ){
    GET_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    if( pEditBuffer->m_pLoadingImage ){
        delete pEditBuffer->m_pLoadingImage;
    }
}

void EDT_SetImageInfo(MWContext *pContext, int32 ele_id, int32 width, int32 height){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    if( pEditBuffer->m_pLoadingImage ){
        pEditBuffer->m_pLoadingImage->SetImageInfo( ele_id, width, height );
    }
}

// HorizRule

EDT_HorizRuleData *EDT_GetHorizRuleData( MWContext *pContext ){
    GET_EDIT_BUF_OR_RETURN(pContext, pEditBuffer) NULL;
    return pEditBuffer->GetHorizRuleData();
}

void EDT_SetHorizRuleData( MWContext *pContext, EDT_HorizRuleData *pData ){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    pEditBuffer->BeginBatchChanges(kSetHorizRuleDataCommandID);
    pEditBuffer->SetHorizRuleData( pData );
    pEditBuffer->EndBatchChanges();
}

EDT_HorizRuleData* EDT_NewHorizRuleData(){
    return CEditHorizRuleElement::NewData( );
}

void EDT_FreeHorizRuleData( EDT_HorizRuleData *pData ){
    CEditHorizRuleElement::FreeData( pData );
}

void EDT_InsertHorizRule( MWContext *pContext, EDT_HorizRuleData *pData ){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    pEditBuffer->InsertHorizRule( pData );
}

// Targets
char *EDT_GetTargetData( MWContext *pContext ){
   GET_EDIT_BUF_OR_RETURN(pContext, pEditBuffer) NULL;
   return pEditBuffer->GetTargetData();
}

void EDT_SetTargetData( MWContext *pContext, char *pData ){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    pEditBuffer->BeginBatchChanges(kSetTargetDataCommandID);
    pEditBuffer->SetTargetData( pData );
    pEditBuffer->EndBatchChanges();
}

void EDT_InsertTarget( MWContext *pContext, char* pData ){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    pEditBuffer->BeginBatchChanges(kInsertTargetCommandID);
    pEditBuffer->InsertTarget( pData );
    pEditBuffer->EndBatchChanges();
}

char *EDT_GetAllDocumentTargets( MWContext *pContext ){
    GET_EDIT_BUF_OR_RETURN(pContext, pEditBuffer) NULL;
    return pEditBuffer->GetAllDocumentTargets();
}

char *EDT_GetAllDocumentTargetsInFile( MWContext *pContext, char *pHref ){
    GET_EDIT_BUF_OR_RETURN(pContext, pEditBuffer) NULL;
    return pEditBuffer->GetAllDocumentTargetsInFile(pHref);
}

// For backwards compatibility with non-Windows Front Ends.
char *EDT_GetAllDocumentFiles( MWContext *pContext ){
    GET_EDIT_BUF_OR_RETURN(pContext, pEditBuffer) NULL;
    return pEditBuffer->GetAllDocumentFiles(NULL,TRUE);
}
// New version.
char *EDT_GetAllDocumentFilesSelected( MWContext *pContext, XP_Bool **ppSelected, 
                                        XP_Bool bKeepImages ){
    GET_EDIT_BUF_OR_RETURN(pContext, pEditBuffer) NULL;
    return pEditBuffer->GetAllDocumentFiles(ppSelected,bKeepImages);
}


// Unknown Tags
char *EDT_GetUnknownTagData( MWContext *pContext ){
    GET_EDIT_BUF_OR_RETURN(pContext, pEditBuffer) NULL;
    return pEditBuffer->GetUnknownTagData();
}

void EDT_SetUnknownTagData( MWContext *pContext, char *pData ){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    pEditBuffer->BeginBatchChanges(kSetUnknownTagDataCommandID);
    pEditBuffer->SetUnknownTagData( pData );
    pEditBuffer->EndBatchChanges();
}

void EDT_InsertUnknownTag( MWContext *pContext, char* pData ){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    pEditBuffer->BeginBatchChanges(kInsertUnknownTagCommandID);
    pEditBuffer->InsertUnknownTag( pData );
    pEditBuffer->EndBatchChanges();
}

ED_TagValidateResult EDT_ValidateTag( char *pData, XP_Bool bNoBrackets ){
    return CEditIconElement::ValidateTag( pData, bNoBrackets );
}

// List

EDT_ListData *EDT_GetListData( MWContext *pContext ){
    GET_EDIT_BUF_OR_RETURN(pContext, pEditBuffer) NULL;
    return pEditBuffer->GetListData();
}

void EDT_SetListData( MWContext *pContext, EDT_ListData *pData ){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    CSetListDataCommand* pCommand = new CSetListDataCommand(pEditBuffer, *pData);
    pEditBuffer->AdoptAndDo(pCommand);
}

void EDT_FreeListData( EDT_ListData *pData ){
    CEditListElement::FreeData( pData );
}

// Break

void EDT_InsertBreak( MWContext *pContext, ED_BreakType eBreak ){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    // A break is treated like a character as far as the typing command goes.
    // So we don't do a command at this level.
    pEditBuffer->InsertBreak( eBreak );
}

// Tables

XP_Bool EDT_IsInsertPointInTable(MWContext *pContext ){
    GET_EDIT_BUF_OR_RETURN(pContext, pEditBuffer) FALSE;
    return pEditBuffer->IsInsertPointInTable();
}

XP_Bool EDT_IsInsertPointInNestedTable(MWContext *pContext ){
    GET_EDIT_BUF_OR_RETURN(pContext, pEditBuffer) FALSE;
    return pEditBuffer->IsInsertPointInNestedTable();
}

EDT_TableData* EDT_GetTableData( MWContext *pContext ){
    GET_EDIT_BUF_OR_RETURN(pContext, pEditBuffer) NULL;
    return pEditBuffer->GetTableData();
}

void EDT_GetTableParentSize( MWContext *pContext, XP_Bool bCell, int32 *pWidth, int32 *pHeight )
{
    GET_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    CEditInsertPoint ip;
    pEditBuffer->GetInsertPoint(ip);
    CEditTableElement* pTable = ip.m_pElement->GetTableIgnoreSubdoc();
    if ( pTable )
    {
         if( bCell )
         {
             CEditTableCellElement* pCell = ip.m_pElement->GetTableCellIgnoreSubdoc();
             if( pCell )
             {
                if( pWidth )
                    *pWidth = pCell->GetParentWidth();
                if( pHeight )
                    *pHeight = pCell->GetParentHeight();
            }
         } else {
            pTable->GetParentSize(pEditBuffer->GetContext(), pWidth, pHeight, NULL);
        }
    }
}

void EDT_SetTableData( MWContext *pContext, EDT_TableData *pData )
{
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);

    // Even though this is checked in SetTableData,
    //  doing it here preserves the UNDO buffer in case
    //  we are called and we're not inside a table
    if( pEditBuffer->IsInsertPointInTable() )
    {
        pEditBuffer->BeginBatchChanges(kSetTableDataCommandID);
        pEditBuffer->SetTableData(pData);
//            CSetTableDataCommand* pCommand = new CSetTableDataCommand(pEditBuffer, pData);
//            pEditBuffer->AdoptAndDo(pCommand);
        pEditBuffer->EndBatchChanges();
    }
}

EDT_TableData* EDT_NewTableData() {
    return CEditTableElement::NewData();
}

void EDT_FreeTableData(  EDT_TableData *pData ) {
    CEditTableElement::FreeData( pData );
}

void EDT_InsertTable( MWContext *pContext, EDT_TableData *pData){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    pEditBuffer->InsertTable( pData );
}

void EDT_DeleteTable( MWContext *pContext ){
    GET_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    pEditBuffer->DeleteTable();
}

ED_MergeType EDT_GetMergeTableCellsType( MWContext *pContext )
{
    GET_EDIT_BUF_OR_RETURN(pContext, pEditBuffer) ED_MERGE_NONE;
    return pEditBuffer->GetMergeTableCellsType();
}

XP_Bool EDT_CanSplitTableCell( MWContext *pContext )
{
    GET_EDIT_BUF_OR_RETURN(pContext, pEditBuffer) FALSE;
    return pEditBuffer->CanSplitTableCell();
}

void EDT_MergeTableCells( MWContext *pContext )
{
    GET_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    pEditBuffer->MergeTableCells();
}

void EDT_SplitTableCell( MWContext *pContext )
{
    GET_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    pEditBuffer->SplitTableCell();
}

// Table Caption

XP_Bool EDT_IsInsertPointInTableCaption(MWContext *pContext ){
    GET_EDIT_BUF_OR_RETURN(pContext, pEditBuffer) FALSE;
    return pEditBuffer->IsInsertPointInTableCaption();
}

EDT_TableCaptionData* EDT_GetTableCaptionData( MWContext *pContext ){
    GET_EDIT_BUF_OR_RETURN(pContext, pEditBuffer) NULL;
    return pEditBuffer->GetTableCaptionData();
}

void EDT_SetTableCaptionData( MWContext *pContext, EDT_TableCaptionData *pData ){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    // CSetTableCaptionDataCommand actually performs the operation in the
    // constructor of the command. So in order for save-based undo to
    // work, we must wrap its constructor in BeginBatchChanges.
    pEditBuffer->BeginBatchChanges(kSetTableCaptionDataCommandID);
    CSetTableCaptionDataCommand* pCommand = new CSetTableCaptionDataCommand(pEditBuffer, pData);
    pEditBuffer->AdoptAndDo(pCommand);
    pEditBuffer->EndBatchChanges();
}

EDT_TableCaptionData* EDT_NewTableCaptionData() {
    return CEditCaptionElement::NewData();
}

void EDT_FreeTableCaptionData(  EDT_TableCaptionData *pData ) {
    CEditCaptionElement::FreeData( pData );
}

void EDT_InsertTableCaption( MWContext *pContext, EDT_TableCaptionData *pData){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    pEditBuffer->InsertTableCaption( pData );
}

void EDT_DeleteTableCaption( MWContext *pContext){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    pEditBuffer->DeleteTableCaption();
}

// Table Row

XP_Bool EDT_IsInsertPointInTableRow(MWContext *pContext ){
    GET_EDIT_BUF_OR_RETURN(pContext, pEditBuffer) FALSE;
    return pEditBuffer->IsInsertPointInTableRow();
}

EDT_TableRowData* EDT_GetTableRowData( MWContext *pContext ){
    GET_EDIT_BUF_OR_RETURN(pContext, pEditBuffer) NULL;
    return pEditBuffer->GetTableRowData();
}

void EDT_SetTableRowData( MWContext *pContext, EDT_TableRowData *pData ){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    // CSetTableRowDataCommand actually performs the operation in the
    // constructor of the command. So in order for save-based undo to
    // work, we must wrap its constructor in BeginBatchChanges.
    pEditBuffer->BeginBatchChanges(kSetTableRowDataCommandID);
    CSetTableRowDataCommand* pCommand = new CSetTableRowDataCommand(pEditBuffer, pData);
    pEditBuffer->AdoptAndDo(pCommand);
    pEditBuffer->EndBatchChanges();
}

EDT_TableRowData* EDT_NewTableRowData() {
    return CEditTableRowElement::NewData();
}

void EDT_FreeTableRowData(  EDT_TableRowData *pData ) {
    CEditTableRowElement::FreeData( pData );
}

void EDT_InsertTableRows( MWContext *pContext, EDT_TableRowData *pData, XP_Bool bAfterCurrentRow, intn number){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    pEditBuffer->BeginBatchChanges(kInsertTableRowCommandID);
    pEditBuffer->InsertTableRows( pData, bAfterCurrentRow, number );
    pEditBuffer->EndBatchChanges();
}

void EDT_DeleteTableRows( MWContext *pContext, intn number){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    pEditBuffer->DeleteTableRows(number);
}

// Table Cell

XP_Bool EDT_IsInsertPointInTableCell(MWContext *pContext ){
    GET_EDIT_BUF_OR_RETURN(pContext, pEditBuffer) FALSE;
    return pEditBuffer->IsInsertPointInTableCell();
}

EDT_TableCellData* EDT_GetTableCellData( MWContext *pContext ){
    GET_EDIT_BUF_OR_RETURN(pContext, pEditBuffer) NULL;
    return pEditBuffer->GetTableCellData();
}

void EDT_ChangeTableSelection(MWContext *pContext, ED_HitType iHitType, ED_MoveSelType iMoveType, 
                              EDT_TableCellData *pData) {
    GET_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    pEditBuffer->ChangeTableSelection(iHitType, iMoveType, pData);
}

void EDT_SetTableCellData( MWContext *pContext, EDT_TableCellData *pData ){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    // CSetTableCellDataCommand actually performs the operation in the
    // constructor of the command. So in order for save-based undo to
    // work, we must wrap its constructor in BeginBatchChanges.
    pEditBuffer->BeginBatchChanges(kSetTableCellDataCommandID);
    pEditBuffer->SetTableCellData(pData);
//    CSetTableCellDataCommand* pCommand = new CSetTableCellDataCommand(pEditBuffer, pData);
//    pEditBuffer->AdoptAndDo(pCommand);
    pEditBuffer->EndBatchChanges();
}

EDT_TableCellData* EDT_NewTableCellData() {
    return CEditTableCellElement::NewData();
}

void EDT_FreeTableCellData(  EDT_TableCellData *pData ) {
    CEditTableCellElement::FreeData( pData );
}

void EDT_InsertTableCells( MWContext *pContext, EDT_TableCellData *pData, XP_Bool bAfterCurrentColumn, intn number){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    pEditBuffer->InsertTableCells( pData, bAfterCurrentColumn, number );
}

void EDT_DeleteTableCells( MWContext *pContext, intn number){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    pEditBuffer->DeleteTableCells(number);
}

void EDT_InsertTableColumns( MWContext *pContext, EDT_TableCellData *pData, XP_Bool bAfterCurrentColumn, intn number){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    pEditBuffer->BeginBatchChanges(kInsertTableColumnCommandID);
    pEditBuffer->InsertTableColumns( pData, bAfterCurrentColumn, number );
    pEditBuffer->EndBatchChanges();
}

void EDT_DeleteTableColumns( MWContext *pContext, intn number){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    pEditBuffer->DeleteTableColumns(number);
}

// Layer

XP_Bool EDT_IsInsertPointInLayer(MWContext *pContext ){
    GET_EDIT_BUF_OR_RETURN(pContext, pEditBuffer) FALSE;
    return pEditBuffer->IsInsertPointInLayer();
}

EDT_LayerData* EDT_GetLayerData( MWContext *pContext ){
    GET_EDIT_BUF_OR_RETURN(pContext, pEditBuffer) NULL;
    return pEditBuffer->GetLayerData();
}

void EDT_SetLayerData( MWContext *pContext, EDT_LayerData *pData ){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    CEditInsertPoint ip;
    pEditBuffer->GetInsertPoint(ip);
    CEditLayerElement* pLayer = ip.m_pElement->GetLayerIgnoreSubdoc();
    if ( pLayer ){
        pEditBuffer->BeginBatchChanges(kSetLayerDataCommandID);
        pLayer->SetData(pData);
        pEditBuffer->EndBatchChanges();
    }
}

EDT_LayerData* EDT_NewLayerData() {
    return CEditLayerElement::NewData();
}

void EDT_FreeLayerData(  EDT_LayerData *pData ) {
    CEditLayerElement::FreeData( pData );
}

void EDT_InsertLayer( MWContext *pContext, EDT_LayerData *pData){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    pEditBuffer->InsertLayer( pData );
}

void EDT_DeleteLayer( MWContext *pContext ){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    pEditBuffer->DeleteLayer();
}

// Undo/Redo

void EDT_Undo( MWContext *pContext ){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    pEditBuffer->Undo( );
}

void EDT_Redo( MWContext *pContext ){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    pEditBuffer->Redo( );
}
#if 0
PRIVATE
intn EDT_GetCommandHistoryLimit(MWContext *pContext){
    GET_EDIT_BUF_OR_RETURN(pContext, pEditBuffer) 0;
    return pEditBuffer->GetCommandHistoryLimit( );
}

PRIVATE
void EDT_SetCommandHistoryLimit(MWContext *pContext, intn newLimit){
    GET_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    pEditBuffer->SetCommandHistoryLimit( newLimit );
}
#endif

intn EDT_GetUndoCommandID( MWContext *pContext, intn index ){
    GET_EDIT_BUF_OR_RETURN(pContext, pEditBuffer) CEDITCOMMAND_ID_NULL;
    return pEditBuffer->GetUndoCommand( index );
}

intn EDT_GetRedoCommandID( MWContext *pContext, intn index ){
    GET_EDIT_BUF_OR_RETURN(pContext, pEditBuffer) CEDITCOMMAND_ID_NULL;
    return pEditBuffer->GetRedoCommand( index );
}

void EDT_BeginBatchChanges(MWContext *pContext){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    pEditBuffer->BeginBatchChanges(kGroupOfChangesCommandID);
}

void EDT_EndBatchChanges(MWContext *pContext){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    pEditBuffer->EndBatchChanges( );
}

void EDT_SetDisplayParagraphMarks(MWContext *pContext, XP_Bool display){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    pEditBuffer->SetDisplayParagraphMarks( display );
}

XP_Bool EDT_GetDisplayParagraphMarks(MWContext *pContext){
    GET_EDIT_BUF_OR_RETURN(pContext, pEditBuffer) FALSE;
    return pEditBuffer->GetDisplayParagraphMarks();
}

void EDT_SetDisplayTables(MWContext *pContext, XP_Bool display){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
    pEditBuffer->SetDisplayTables( display );
}

XP_Bool EDT_GetDisplayTables(MWContext *pContext){
    GET_EDIT_BUF_OR_RETURN(pContext, pEditBuffer) FALSE;
    return pEditBuffer->GetDisplayTables();
}

XP_Bool EDT_IsWritableBuffer(MWContext *pContext){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext,pEditBuffer) FALSE;
    return TRUE; 
}

#endif //EDITOR
