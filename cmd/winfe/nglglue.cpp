
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

// This file exists for code that lost its home during the process of trimming
// down the WinFE for NGLayout.  Hopefully, as layout integration proceeds
// most of this code will become irrelevant or will find a better home elsewhere.

#include "stdafx.h"
#include "cxabstra.h"


extern "C" void FE_RaiseWindow(MWContext *pContext)    {
    //  Make sure this is possible.
    if(pContext && ABSTRACTCX(pContext) && !ABSTRACTCX(pContext)->IsDestroyed()
        && ABSTRACTCX(pContext)->IsFrameContext() && PANECX(pContext)->GetPane()
        && WINCX(pContext)->GetFrame() && WINCX(pContext)->GetFrame()->GetFrameWnd()) {
        CWinCX *pWinCX = WINCX(pContext);
        CFrameWnd *pFrameWnd = pWinCX->GetFrame()->GetFrameWnd();

        //  Bring the frame to the top first.
		if(pFrameWnd->IsIconic())	{
			pFrameWnd->ShowWindow(SW_RESTORE);
		}
        ::SetWindowPos(pFrameWnd->GetSafeHwnd(), HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
#ifdef XP_WIN32
        pFrameWnd->SetForegroundWindow();
#endif

        //  Now, set focus to the view being raised,
        //      possibly a frame cell.
        pWinCX->
            GetFrame()->
            GetFrameWnd()->
            SetActiveView(pWinCX->GetView(), TRUE);
    }
}

extern "C" void FE_Print(const char *pUrl)  {
  XP_ASSERT(0);
}

PR_BEGIN_EXTERN_C
extern void
lo_view_title( MWContext *context, char *title_str ) {
  XP_ASSERT(0);
}
PR_END_EXTERN_C



//--------------------------------------------------------------------------
//--------------------------------------------------------------------------
//--------------------------------------------------------------------------
//  Everything from here down is copied straight from nsNetStubs.cpp,
//  It will all go away when netlib modularization is complete.
//  Not all these includes are needed.  Just copied them from nsNetStubs.cpp
//--------------------------------------------------------------------------
//--------------------------------------------------------------------------
//--------------------------------------------------------------------------
#include "nspr.h"
#include "xp_hash.h"
#include "xp_file.h"
#include "jsapi.h"
#include "libi18n.h"
#include "libevent.h"
#include "mkgeturl.h"
#include "net.h"

extern "C" {
#include "pwcacapi.h"
#include "xp_reg.h"
#include "secnav.h"
#include "preenc.h"
};

#define MOZ_FUNCTION_STUB XP_ASSERT(0)

extern "C" {

/*
 *---------------------------------------------------------------------------
 * From ns/lib/libparse/pa_hash.c
 *---------------------------------------------------------------------------
 */

/*************************************
 * Function: pa_tokenize_tag
 *
 * Description: This function maps the passed in string
 * 		to one of the valid tag element tokens, or to
 *		the UNKNOWN token.
 *
 * Params: Takes a \0 terminated string.
 *
 * Returns: a 32 bit token to describe this tag element.  On error,
 * 	    which means it was not passed an unknown tag element string,
 * 	    it returns the token P_UNKNOWN.
 *
 * Performance Notes:
 * Profiling on mac revealed this routine as a big (5%) time sink.
 * This function was stolen from pa_mdl.c and merged with the perfect
 * hashing code and the tag comparison code so it would be flatter (fewer
 * function calls) since those are still expensive on 68K and x86 machines.
 *************************************/

intn
pa_tokenize_tag(char *str)
{
    MOZ_FUNCTION_STUB;
    return -1; /* P_UNKNOWN */;
}


/*
 *---------------------------------------------------------------------------
 * From ns/lib/libparse/pa_parse.c
 *---------------------------------------------------------------------------
 */

/*************************************
 * Function: PA_BeginParseMDL
 *
 * Description: The outside world's main access to the parser.
 *      call this when you are going to start parsing
 *      a new document to set up the parsing stream.
 *      This function cannot be called successfully
 *      until PA_ParserInit() has been called.
 *
 * Params: Takes lots of document information that is all
 *     ignored right now, just used the window_id to create
 *     a unique document id.
 *
 * Returns: a pointer to a new NET_StreamClass structure, set up to
 *      give the caller a parsing stream into the parser.
 *      Returns NULL on error.
 *************************************/
NET_StreamClass *
PA_BeginParseMDL(FO_Present_Types format_out,
                 void *init_data, URL_Struct *anchor, MWContext *window_id)
{
    MOZ_FUNCTION_STUB;
    return NULL;
}

/*
 *---------------------------------------------------------------------------
 * From ns/lib/layout/layutil.c
 *---------------------------------------------------------------------------
 */

 /*
 * Return the width of the window for this context in chars in the default
 * fixed width font.  Return -1 on error.
 */
int16
LO_WindowWidthInFixedChars(MWContext *context)
{
    MOZ_FUNCTION_STUB;
    return -1;
}



/*
 *---------------------------------------------------------------------------
 * From ns/lib/layout/layinfo.c
 *---------------------------------------------------------------------------
 */

/*
 * Prepare a bunch of information about the content of this
 * document and prints the information as HTML down
 * the passed in stream.
 *
 * Returns:
 *      -1      If the context passed does not correspond to any currently
 *              laid out document.
 *      0       If the context passed corresponds to a document that is
 *              in the process of being laid out.
 *      1       If the context passed corresponds to a document that is
 *              completly laid out and info can be found.
 */
PUBLIC intn
LO_DocumentInfo(MWContext *context, NET_StreamClass *stream)
{
    MOZ_FUNCTION_STUB;
    return -1;
}


/*
 *---------------------------------------------------------------------------
 * From ns/lib/layout/layobj.c
 *---------------------------------------------------------------------------
 */

/*
 * Create a new stream handler for dealing with a stream of
 * object data.  We don't really want to do anything with
 * the data, we just need to check the type to see if this
 * is some kind of object we can handle.  If it is, we can
 * format the right kind of object, clear the layout blockage,
 * and connect this stream up to its rightful owner.
 * NOTE: Plug-ins are the only object type supported here now.
 */
NET_StreamClass*
LO_NewObjectStream(FO_Present_Types format_out, void* type,
                   URL_Struct* urls, MWContext* context)
{
    MOZ_FUNCTION_STUB;
    return NULL;
}


/*
 *---------------------------------------------------------------------------
 * From ns/lib/plugin/npglue.c
 *---------------------------------------------------------------------------
 */

XP_Bool
NPL_HandleURL(MWContext *cx, FO_Present_Types iFormatOut, URL_Struct *pURL, Net_GetUrlExitFunc *pExitFunc)
{
//    MOZ_FUNCTION_STUB;
    return FALSE;
}


/*
 *---------------------------------------------------------------------------
 * From ns/lib/libmocha/lm_taint.c
 *---------------------------------------------------------------------------
 */

const char *
LM_SkipWysiwygURLPrefix(const char *url_string)
{
    MOZ_FUNCTION_STUB;
    return NULL;
}

/*
 *---------------------------------------------------------------------------
 * From ns/lib/libmocha/et_moz.c
 *---------------------------------------------------------------------------
 */

JSBool
ET_PostMessageBox(MWContext* context, char* szMessage, JSBool bConfirm)
{
    MOZ_FUNCTION_STUB;
    return JS_FALSE;
}



/*
 *---------------------------------------------------------------------------
 * From ns/lib/libmocha/et_mocha.c
 *---------------------------------------------------------------------------
 */
/*
 * A mocha stream from netlib has compeleted, eveluate the contents
 *   and pass them up our stream.  We will take ownership of the 
 *   buf argument and are responsible for freeing it
 */
void
ET_MochaStreamComplete(MWContext * pContext, void * buf, int len, 
                       char *content_type, Bool isUnicode)
{
    MOZ_FUNCTION_STUB;
}


/*
 * A mocha stream from netlib has aborted
 */
void
ET_MochaStreamAbort(MWContext * context, int status)
{
    MOZ_FUNCTION_STUB;
}


/*
 * Evaluate the given script.  I'm sure this is going to need a
 *   callback or compeletion routine
 */
void
ET_EvaluateScript(MWContext * pContext, char * buffer, ETEvalStuff * stuff,
                  ETEvalAckFunc fn)
{
    MOZ_FUNCTION_STUB;
}


void
ET_SetDecoderStream(MWContext * pContext, NET_StreamClass *stream,
                    URL_Struct *url_struct, JSBool free_stream_on_close)
{
    MOZ_FUNCTION_STUB;
}


/*
 * Tell the backend about a new load event.
 */
void
ET_SendLoadEvent(MWContext * pContext, int32 type, ETVoidPtrFunc fnClosure,
                 NET_StreamClass *stream, int32 layer_id, Bool resize_reload)
{
//    MOZ_FUNCTION_STUB;
}


/*
 *---------------------------------------------------------------------------
 * From ns/lib/libmocha/lm_init.c
 *---------------------------------------------------------------------------
 */

void
LM_PutMochaDecoder(MochaDecoder *decoder)
{
    MOZ_FUNCTION_STUB;
}


MochaDecoder *
LM_GetMochaDecoder(MWContext *context)
{
    MOZ_FUNCTION_STUB;
    return NULL;
}


/*
 * Release the JSLock
 */
void PR_CALLBACK
LM_UnlockJS()
{
    MOZ_FUNCTION_STUB;
}


/*
 * Try to get the JSLock but just return JS_FALSE if we can't
 *   get it, don't wait since we could deadlock
 */
JSBool PR_CALLBACK
LM_AttemptLockJS(JSLockReleaseFunc fn, void * data)
{
    MOZ_FUNCTION_STUB;
    return JS_FALSE;
}


JSBool PR_CALLBACK
LM_ClearAttemptLockJS(JSLockReleaseFunc fn, void * data)
{
    MOZ_FUNCTION_STUB;
    return JS_FALSE;
}


/*
 *---------------------------------------------------------------------------
 * From ns/lib/libmocha/lm_doc.c
 *---------------------------------------------------------------------------
 */

NET_StreamClass *
LM_WysiwygCacheConverter(MWContext *context, URL_Struct *url_struct,
                         const char * wysiwyg_url, const char * base_href)
{
    MOZ_FUNCTION_STUB;
    return NULL;
}


/*
 *---------------------------------------------------------------------------
 * From ns/lib/libmocha/lm_win.c
 *---------------------------------------------------------------------------
 */
/*
 * Entry point for front-ends to notify JS code of help events.
 */
void
LM_SendOnHelp(MWContext *context)
{
    MOZ_FUNCTION_STUB;
}


/*
 *---------------------------------------------------------------------------
 * From ns/lib/libmocha/lm_taint.c
 *---------------------------------------------------------------------------
 */
char lm_unknown_origin_str[] = "[unknown origin]";

JSPrincipals *
LM_NewJSPrincipals(URL_Struct *archive, char *id, const char *codebase)
{
    MOZ_FUNCTION_STUB;
    return NULL;
}

/* 
 * This exit routine is used for all streams requested by the
 * plug-in: byterange request streams, NPN_GetURL streams, and 
 * NPN_PostURL streams.  NOTE: If the exit routine gets called
 * in the course of a context switch, we must NOT delete the
 * URL_Struct.  Example: FTP post with result from server
 * displayed in new window -- the exit routine will be called
 * when the upload completes, but before the new context to
 * display the result is created, since the display of the
 * results in the new context gets its own completion routine.
 */
void
NPL_URLExit(URL_Struct *urls, int status, MWContext *cx)
{
    MOZ_FUNCTION_STUB;
}

void RDF_AddCookieResource(char* name, char* path, char* host, char* expires)
{
	MOZ_FUNCTION_STUB;    
}

/*
 *---------------------------------------------------------------------------
 * From ns/lib/layout/laysel.c
 *---------------------------------------------------------------------------
 */
/*
    get first(last) if current element is NULL.
*/
Bool
LO_getNextTabableElement( MWContext *context, LO_TabFocusData *pCurrentFocus, int forward )
{
    MOZ_FUNCTION_STUB;
    return FALSE;
}


/*
 *---------------------------------------------------------------------------
 * From ns/lib/layout/laygrid.c
 *---------------------------------------------------------------------------
 */
lo_GridCellRec *
lo_ContextToCell(MWContext *context, Bool reconnect, lo_GridRec **grid_ptr)
{
    MOZ_FUNCTION_STUB;
    return NULL;
}

intn
LO_ProcessTag(void *data_object, PA_Tag *tag, intn status)
{
    MOZ_FUNCTION_STUB;
    return 0;
}

void
LM_ForceJSEnabled(MWContext *cx)
{
    MOZ_FUNCTION_STUB;
}

} // End extern "C"
