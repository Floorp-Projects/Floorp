/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
/*
 * Event handling in the Navigator / libmocha
 *
 * This file contains common code for separate threads to send messages
 *   to the mozilla thread to get it to do stuff
 */
#include "lm.h"
#include "xp.h"
#include "fe_proto.h"
#include "net.h"
#include "structs.h"
#include "prthread.h"
#include "prmem.h"
#include "ds.h"		/* XXX required by htmldlgs.h */
#include "htmldlgs.h"
#include "layout.h"
#include "np.h"
#include "prefapi.h"
#include "pa_parse.h"
/* #include "netcache.h" */
#include "secnav.h"


#define IL_CLIENT
#include "libimg.h"             /* Image Library public API. */

/* pointer to the mocha thread */
extern PRThread		    *lm_InterpretThread;
extern PRThread		    *mozilla_thread;
extern QueueStackElement    *et_TopQueue;

/****************************************************************************/

PR_STATIC_CALLBACK(void*)
et_PostEvent(ETEvent * e, Bool sync)
{

    /* 
     * Guard against the mozilla_event_queue goes away before we do
     *   on exit.
     */
    if(!mozilla_event_queue)
        return(NULL);

    /*
     * If we are already in the mozilla thread and about to send
     *   a synchronous message to ourselves just process it right
     *   now instead of deadlocking
     */
    if(sync && PR_CurrentThread() == mozilla_thread) {
	void * val;
	val = e->event.handler(&e->event);
	e->event.destructor(&e->event);
        return(val);
    }

    if(sync)
        return(PR_PostSynchronousEvent(mozilla_event_queue, &e->event));

    PR_PostEvent(mozilla_event_queue, &e->event);
    return(NULL);

}


/* 
 * synchonous events can't be destroyed until after PR_PostSynchronousEvent()
 *   has returned.  This is a dummy destructor to make sure the event stays
 *   alive along enough
 */
PR_STATIC_CALLBACK(void)
et_DestroyEvent_WaitForIt(void * event)
{
}

/*
 * Generic destructor for event with no private data
 */
PR_STATIC_CALLBACK(void)
et_DestroyEvent_GenericEvent(void * event)
{
    XP_FREE(event);
}


/****************************************************************************/

typedef struct {
    ETEvent   ce;
    char*     szMessage;  /* message for dialog */
    JSBool    bConfirm;   /* TRUE if confirmation, FALSE if alert */
} MozillaEvent_MessageBox;


PR_STATIC_CALLBACK(void*)
et_HandleEvent_MessageBox(MozillaEvent_MessageBox* e)
{
    void *pRet;
    Bool bPriorJSCalling = FALSE;
    if( e->ce.context ) {
        bPriorJSCalling = e->ce.context->bJavaScriptCalling;
        e->ce.context->bJavaScriptCalling = TRUE;
    }
    
    pRet = (void *)JSTYPE_BOOLEAN;
    
    if( e->bConfirm )
        pRet = (void *)FE_Confirm(e->ce.context, e->szMessage);
    else
        FE_Alert(e->ce.context, e->szMessage);

    if( e->ce.context ) 
        e->ce.context->bJavaScriptCalling = bPriorJSCalling;
            
    return pRet;
}

PR_STATIC_CALLBACK(void)
et_DestroyEvent_MessageBox(MozillaEvent_MessageBox* event)
{
    XP_FREE((char*)event->szMessage);
    XP_FREE(event);
}

JSBool
ET_PostMessageBox(MWContext* context, char* szMessage, JSBool bConfirm)
{
    JSBool ok = JS_TRUE;
    MozillaEvent_MessageBox* event = PR_NEW(MozillaEvent_MessageBox);
    if (event == NULL) 
        return(JS_FALSE);
    event->ce.context = context;
    event->szMessage = strdup(szMessage);
    event->bConfirm = bConfirm;
    PR_InitEvent(&event->ce.event, context,
		 (PRHandleEventProc)et_HandleEvent_MessageBox,
		 (PRDestroyEventProc)et_DestroyEvent_MessageBox);
    ok = (JSBool) et_PostEvent(&event->ce, TRUE);

    return(ok);

}

/****************************************************************************/

typedef struct {
    ETEvent 	   ce;
    const char   * szMessage;	/* progress message */
} MozillaEvent_Progress;


PR_STATIC_CALLBACK(void)
et_HandleEvent_Progress(MozillaEvent_Progress* e)
{
    FE_Progress(e->ce.context, e->szMessage);
}

PR_STATIC_CALLBACK(void)
et_DestroyEvent_Progress(MozillaEvent_MessageBox* event)
{
    XP_FREE((char*)event->szMessage);
    XP_FREE(event);
}

void
ET_PostProgress(MWContext* context, const char* szMessage)
{
    MozillaEvent_Progress* event = PR_NEW(MozillaEvent_Progress);
	if (event == NULL) 
        return;
    PR_InitEvent(&event->ce.event, context,
		 (PRHandleEventProc)et_HandleEvent_Progress,
		 (PRDestroyEventProc)et_DestroyEvent_Progress);
    event->ce.context = context;
    if(szMessage)
        event->szMessage = strdup(szMessage);
    else
        event->szMessage = NULL;
    et_PostEvent(&event->ce, FALSE);

}

/****************************************************************************/

typedef struct {
    ETEvent	    ce;
    void          * pStuff;    
} MozillaEvent_Void;


PR_STATIC_CALLBACK(void)
et_HandleEvent_ClearTimeout(MozillaEvent_Void* e)
{
    FE_ClearTimeout(e->pStuff);
}

void
ET_PostClearTimeout(void * pStuff)
{
    MozillaEvent_Void* event = PR_NEW(MozillaEvent_Void);
    if (event == NULL) 
        return;
    PR_InitEvent(&event->ce.event, NULL,
		 (PRHandleEventProc)et_HandleEvent_ClearTimeout,
		 (PRDestroyEventProc)et_DestroyEvent_GenericEvent);
    event->ce.context = NULL;
    event->pStuff = pStuff;
    et_PostEvent(&event->ce, FALSE);

}

/****************************************************************************/

/*
 * OK, timeouts are messy.  We are in a separate thread, we want to
 *   call the mozilla thread to fire a timeout.  When the timeout
 *   fires we need to get a message sent back to our thread to call
 *   our timeout since, presumably, our timeout needs to run in our
 *   thread and not the mozilla thread.
 * Make a duplicate of the current event structure in case the 
 *   timeout gets fired and the 'run the closure' event gets sent
 *   before we have returned from PR_PostSynchronousEvent().
 */
PR_STATIC_CALLBACK(void*)
et_HandleEvent_SetTimeout(MozillaEvent_Timeout* e)
{
    MozillaEvent_Timeout* event = XP_NEW_ZAP(MozillaEvent_Timeout);
    if (event == NULL) 
        return(NULL);

    event->fnCallback = e->fnCallback;
    event->pClosure = e->pClosure;
    event->ulTime = e->ulTime;
    event->ce.doc_id = e->ce.doc_id;
    event->pTimerId = FE_SetTimeout(ET_FireTimeoutCallBack, event, e->ulTime);
    
    return(event->pTimerId);
}

void *
ET_PostSetTimeout(TimeoutCallbackFunction fnCallback, void * pClosure, 
                  uint32 ulTime, int32 doc_id)
{
    MozillaEvent_Timeout* event = XP_NEW_ZAP(MozillaEvent_Timeout);
    void * ret;
    if (event == NULL) 
        return(NULL);
    PR_InitEvent(&event->ce.event, NULL,
		 (PRHandleEventProc)et_HandleEvent_SetTimeout,
		 (PRDestroyEventProc)et_DestroyEvent_GenericEvent);
    event->ce.doc_id = doc_id;
    event->fnCallback = fnCallback;
    event->pClosure = pClosure;
    event->ulTime = ulTime;

    ret = et_PostEvent(&event->ce, TRUE);
    return(ret);

}

typedef struct {
    ETEvent	    ce;
    int32           wrap_width;
    int32           parent_layer_id;
} MozillaEvent_CreateLayer;

PR_STATIC_CALLBACK(void*)
et_HandleEvent_CreateLayer(MozillaEvent_CreateLayer * e)
{
    int32 layer_id;

    /* See if the document has changed since this event was sent. */
    if(e->ce.doc_id != XP_DOCID(e->ce.context))
	return NULL;

    layer_id = LO_CreateNewLayer(e->ce.context, e->wrap_width, e->parent_layer_id);
    return (void*)layer_id;
}

int32
ET_PostCreateLayer(MWContext *context, int32 wrap_width, int32 parent_layer_id)
{
    int32 ret;
    MozillaEvent_CreateLayer * event = XP_NEW_ZAP(MozillaEvent_CreateLayer);
    if (event == NULL) 
        return(0);
    PR_InitEvent(&event->ce.event, NULL,
		 (PRHandleEventProc)et_HandleEvent_CreateLayer,
		 (PRDestroyEventProc)et_DestroyEvent_GenericEvent);
    event->ce.context = context;
    event->ce.doc_id = XP_DOCID(context);
    event->wrap_width = wrap_width;
    event->parent_layer_id = parent_layer_id;

    ret = (int32)et_PostEvent(&event->ce, TRUE);
    return(ret);
}

/****************************************************************************/

PR_STATIC_CALLBACK(void)
et_HandleEvent_DestroyWindow(ETEvent* e)
{
    e->context->waitingMode = FALSE;
    FE_DestroyWindow(e->context);
}

void
ET_PostDestroyWindow(MWContext * context)
{
    ETEvent * event = PR_NEW(ETEvent);
    if (event == NULL) 
        return;
    PR_InitEvent(&event->event, context,
		 (PRHandleEventProc)et_HandleEvent_DestroyWindow,
		 (PRDestroyEventProc)et_DestroyEvent_GenericEvent);
    event->context = context;
    et_PostEvent(event, FALSE);

}

/****************************************************************************/

typedef struct {
    ETEvent	      ce;
    LO_Element      * pForm;
    int32             action;
    int32    	      docID;
    PRPackedBool      duplicate;
} MozillaEvent_ManipulateForm;


PR_STATIC_CALLBACK(void)
et_HandleEvent_ManipulateForm(MozillaEvent_ManipulateForm* e)
{

    /* see if the document has changed since this event was sent */
    if(e->docID != XP_DOCID(e->ce.context))
	return;

    switch(e->action) {
    case EVENT_BLUR:
        FE_BlurInputElement(e->ce.context, e->pForm);
        break;
    case EVENT_FOCUS:
        FE_FocusInputElement(e->ce.context, e->pForm);
        break;
    case EVENT_SELECT:
        FE_SelectInputElement(e->ce.context, e->pForm);
        break;
    case EVENT_CLICK:
        FE_ClickInputElement(e->ce.context, e->pForm);
        break;
    case EVENT_CHANGE:
        FE_ChangeInputElement(e->ce.context, e->pForm);
        break;
    default:
        XP_ASSERT(0);
    }
}

PR_STATIC_CALLBACK(void)
et_check_already_pending(MozillaEvent_ManipulateForm* event, 
			 MozillaEvent_ManipulateForm* data, 
			 PREventQueue* queue)
{
    if (event->ce.event.owner == data->ce.event.owner) {
	if (event->action == data->action)
	    data->duplicate = TRUE;
    }
}

void
ET_PostManipulateForm(MWContext * context, LO_Element * pForm, int32 action)
{
    MozillaEvent_ManipulateForm* event;

    event = PR_NEW(MozillaEvent_ManipulateForm);
    if (event == NULL) 
        return;

    PR_InitEvent(&event->ce.event, pForm,
		 (PRHandleEventProc)et_HandleEvent_ManipulateForm,
		 (PRDestroyEventProc)et_DestroyEvent_GenericEvent);
    event->ce.context = context;
    event->docID = XP_DOCID(context);
    event->pForm = pForm;
    event->action = action;
    event->duplicate = FALSE;

    /*
     * Now that we have built the event use it to see if a similar
     *   event already exists.  If one does, the owner field will
     *   get cleared in the event we just created
     */
    PR_MapEvents(mozilla_event_queue, 
		 (PREventFunProc)et_check_already_pending, 
		 event);
    if (event->duplicate) {
	XP_FREE(event);
	return;
    }

    et_PostEvent(&event->ce, FALSE);

}

/****************************************************************************/

PR_STATIC_CALLBACK(void)
et_HandleEvent_ClearView(ETEvent * e)
{
    FE_ClearView(e->context, FE_VIEW);
}

void
ET_PostClearView(MWContext * context)
{
    ETEvent * event = PR_NEW(ETEvent);
    if (event == NULL) 
        return;
    PR_InitEvent(&event->event, context,
		 (PRHandleEventProc)et_HandleEvent_ClearView,
		 (PRDestroyEventProc)et_DestroyEvent_GenericEvent);
    event->context = context;
    et_PostEvent(event, FALSE);

}

/****************************************************************************/

PR_STATIC_CALLBACK(void)
et_HandleEvent_FreeImageElement(MozillaEvent_Void* e)
{
    LO_ImageStruct *lo_image = (LO_ImageStruct *) e->pStuff;

    IL_DestroyImage(lo_image->image_req);
    lo_image->image_req = NULL;
}

/*
 * From the code flow it looked like some people were depending
 *   on this getting done before continuing.  Not sure if that
 *   is really the case or not.  Do we need to be synchronous?
 */
void
ET_PostFreeImageElement(MWContext * context, void * pStuff)
{
    MozillaEvent_Void* event = PR_NEW(MozillaEvent_Void);
	if (event == NULL) 
        return;
    PR_InitEvent(&event->ce.event, context,
		 (PRHandleEventProc)et_HandleEvent_FreeImageElement,
		 (PRDestroyEventProc)et_DestroyEvent_GenericEvent);
    event->ce.context = context;
    event->pStuff = pStuff;

    et_PostEvent(&event->ce, TRUE);

}

/****************************************************************************/

/* Free all images in the mocha image context. */
PR_STATIC_CALLBACK(void)
et_HandleEvent_FreeAnonImages(MozillaEvent_Void* e)
{
    IL_DestroyImageGroup((IL_GroupContext *)e->pStuff);
}

void
ET_PostFreeAnonImages(MWContext *context, IL_GroupContext *img_cx)
{
    MozillaEvent_Void* event = PR_NEW(MozillaEvent_Void);
	if (event == NULL) 
        return;
    PR_InitEvent(&event->ce.event, context,
		 (PRHandleEventProc)et_HandleEvent_FreeAnonImages,
		 (PRDestroyEventProc)et_DestroyEvent_GenericEvent);
    event->ce.context = context;
    event->pStuff = img_cx;

    et_PostEvent(&event->ce, FALSE);
}

/****************************************************************************/

/* 
 * Destroy the mocha image context in the Mozilla thread since 
 *   its destruction must succeed the actual destruction of 
 *   anonymous images. 
 */
PR_STATIC_CALLBACK(void)
et_HandleEvent_FreeImageContext(MozillaEvent_Void* e)
{
    IL_DestroyGroupContext((IL_GroupContext *)e->pStuff);
}

void
ET_PostFreeImageContext(MWContext *context, IL_GroupContext *img_cx)
{
    MozillaEvent_Void* event = PR_NEW(MozillaEvent_Void);
	if (event == NULL) 
        return;
    PR_InitEvent(&event->ce.event, context,
		 (PRHandleEventProc)et_HandleEvent_FreeImageContext,
		 (PRDestroyEventProc)et_DestroyEvent_GenericEvent);
    event->ce.context = context;
    event->pStuff = img_cx;

    et_PostEvent(&event->ce, FALSE);
}

/****************************************************************************/

PR_STATIC_CALLBACK(void)
et_HandleEvent_GetUrl(MozillaEvent_Void* e)
{
    FE_GetURL(e->ce.context, (URL_Struct *) e->pStuff);
}

void
ET_PostGetUrl(MWContext * context, URL_Struct * pUrl)
{
    MozillaEvent_Void* event = PR_NEW(MozillaEvent_Void);
	if (event == NULL) 
        return;
    PR_InitEvent(&event->ce.event, context,
		 (PRHandleEventProc)et_HandleEvent_GetUrl,
		 (PRDestroyEventProc)et_DestroyEvent_GenericEvent);
    event->ce.context = context;
    event->pStuff = pUrl;
    et_PostEvent(&event->ce, FALSE);

}

/****************************************************************************/

typedef struct {
    ETEvent	    ce;
    char*	    szMessage;	/* message for dialog */
    char*           szDefault;
} MozillaEvent_Prompt;


PR_STATIC_CALLBACK(void *)
et_HandleEvent_Prompt(MozillaEvent_Prompt* e)
{
    void *pRet;
    Bool bPriorJSCalling = FALSE;
    if( e->ce.context ) {
        bPriorJSCalling = e->ce.context->bJavaScriptCalling;
        e->ce.context->bJavaScriptCalling = TRUE;
    }
    
    pRet = (void *)FE_Prompt(e->ce.context, e->szMessage, e->szDefault);
    
    if( e->ce.context ) 
        e->ce.context->bJavaScriptCalling = bPriorJSCalling;
            
    return pRet;
}

PR_STATIC_CALLBACK(void)
et_DestroyEvent_Prompt(MozillaEvent_Prompt* event)
{
    XP_FREE((char*)event->szMessage);
    XP_FREE((char*)event->szDefault);
    XP_FREE(event);
}

char *
ET_PostPrompt(MWContext* context, const char* szMessage, const char * szDefault)
{
    char * ret;
    MozillaEvent_Prompt* event = PR_NEW(MozillaEvent_Prompt);
	if (event == NULL) 
        return(NULL);
    PR_InitEvent(&event->ce.event, context,
		 (PRHandleEventProc)et_HandleEvent_Prompt,
		 (PRDestroyEventProc)et_DestroyEvent_Prompt);
    event->ce.context = context;
    event->szMessage = strdup(szMessage);
    if(szDefault)
        event->szDefault = strdup(szDefault);
    else
        event->szDefault = NULL;

    ret = (char *) et_PostEvent(&event->ce, TRUE);
    return(ret);

}

/****************************************************************************/

typedef struct {
    ETEvent	      ce;
    URL_Struct      * pUrl;
    char            * szName;
    Chrome          * pChrome;
} MozillaEvent_NewWindow;


PR_STATIC_CALLBACK(void*)
et_HandleEvent_NewWindow(MozillaEvent_NewWindow* e)
{
    return((void *)FE_MakeNewWindow(e->ce.context, e->pUrl, e->szName, 
                                    e->pChrome));
}

PR_STATIC_CALLBACK(void)
et_DestroyEvent_NewWindow(MozillaEvent_NewWindow* event)
{
    NET_DropURLStruct(event->pUrl);
    if (event->szName)
	XP_FREE((char*)event->szName);
    XP_FREE(event);
}

MWContext *
ET_PostNewWindow(MWContext* context, URL_Struct * pUrl, 
                 char * szName, Chrome * pChrome)
{
    MWContext * ret;
    MozillaEvent_NewWindow* event = PR_NEW(MozillaEvent_NewWindow);
    if (event == NULL) 
        return(NULL);
    PR_InitEvent(&event->ce.event, context,
		 (PRHandleEventProc)et_HandleEvent_NewWindow,
		 (PRDestroyEventProc)et_DestroyEvent_NewWindow);
    event->ce.context = context;
    event->pUrl = NET_HoldURLStruct(pUrl);
    if(szName)
	event->szName = strdup(szName);
    else
	event->szName = NULL;
    event->pChrome = pChrome;

    ret = (MWContext *) et_PostEvent(&event->ce, TRUE);
    return(ret);

}

/****************************************************************************/

typedef struct {
    ETEvent	      ce;
    Chrome          * pChrome;
} MozillaEvent_UpdateWindow;


PR_STATIC_CALLBACK(void)
et_HandleEvent_UpdateWindow(MozillaEvent_UpdateWindow* e)
{
    FE_UpdateChrome(e->ce.context, e->pChrome);
}

void
ET_PostUpdateChrome(MWContext* context, Chrome * pChrome)
{
    MozillaEvent_UpdateWindow* event = PR_NEW(MozillaEvent_UpdateWindow);
    if (event == NULL) 
        return;
    PR_InitEvent(&event->ce.event, context,
		 (PRHandleEventProc)et_HandleEvent_UpdateWindow,
		 (PRDestroyEventProc)et_DestroyEvent_GenericEvent);
    event->ce.context = context;
    event->pChrome = pChrome;

    et_PostEvent(&event->ce, TRUE);

}

/****************************************************************************/

typedef struct {
    ETEvent	      ce;
    Chrome          * pChrome;
} MozillaEvent_QueryWindow;


PR_STATIC_CALLBACK(void)
et_HandleEvent_QueryWindow(MozillaEvent_QueryWindow* e)
{
    FE_QueryChrome(e->ce.context, e->pChrome);
}

void
ET_PostQueryChrome(MWContext* context, Chrome * pChrome)
{
    MozillaEvent_QueryWindow* event = PR_NEW(MozillaEvent_QueryWindow);
    if (event == NULL) 
        return;
    PR_InitEvent(&event->ce.event, context,
		 (PRHandleEventProc)et_HandleEvent_QueryWindow,
		 (PRDestroyEventProc)et_DestroyEvent_GenericEvent);
    event->ce.context = context;
    event->pChrome = pChrome;

    et_PostEvent(&event->ce, TRUE);

}

/****************************************************************************/

typedef struct {
    ETEvent	      ce;
    int32	     *pPixel;
    int32	     *pPallette;
} MozillaEvent_GetColorDepth;


PR_STATIC_CALLBACK(void)
et_HandleEvent_GetColorDepth(MozillaEvent_GetColorDepth* e)
{
    FE_GetPixelAndColorDepth(e->ce.context, e->pPixel, e->pPallette);
}

void
ET_PostGetColorDepth(MWContext* context, int32 *pPixel, int32 *pPallette)
{
    int32 *pMPixel = NULL, *pMPallette = NULL;
    
    MozillaEvent_GetColorDepth* event = PR_NEW(MozillaEvent_GetColorDepth);
    if (event == NULL) 
        return;

    pMPixel = XP_ALLOC(sizeof(int32));
    if (pMPixel == NULL)
	goto fail;

    pMPallette = XP_ALLOC(sizeof(int32));
    if (pMPallette == NULL)
	goto fail;

    PR_InitEvent(&event->ce.event, context,
		 (PRHandleEventProc)et_HandleEvent_GetColorDepth,
		 (PRDestroyEventProc)et_DestroyEvent_WaitForIt);
    event->ce.context = context;
    event->pPixel = pMPixel;
    event->pPallette = pMPallette;

    et_PostEvent(&event->ce, TRUE);
    *pPixel = *event->pPixel;
    *pPallette = *event->pPallette;

fail:
    if (pMPixel)
	XP_FREE(pMPixel);
    if (pMPallette)
	XP_FREE(pMPallette);
    if (event)
	XP_FREE(event);
}

/****************************************************************************/

typedef struct {
    ETEvent	      ce;
    int32	     *pX;
    int32	     *pY;
} MozillaEvent_GetScreenSize;


PR_STATIC_CALLBACK(void)
et_HandleEvent_GetScreenSize(MozillaEvent_GetScreenSize* e)
{
    FE_GetScreenSize(e->ce.context, e->pX, e->pY);
}

void
ET_PostGetScreenSize(MWContext* context, int32 *pX, int32 *pY)
{
    int32 *pMX = NULL, *pMY = NULL;
    
    MozillaEvent_GetScreenSize* event = PR_NEW(MozillaEvent_GetScreenSize);
    if (event == NULL) 
        return;

    pMX = XP_ALLOC(sizeof(int32));
    if (pMX == NULL)
	goto fail;

    pMY = XP_ALLOC(sizeof(int32));
    if (pMY == NULL)
	goto fail;

    PR_InitEvent(&event->ce.event, context,
		 (PRHandleEventProc)et_HandleEvent_GetScreenSize,
		 (PRDestroyEventProc)et_DestroyEvent_WaitForIt);
    event->ce.context = context;
    event->pX = pMX;
    event->pY = pMY;

    et_PostEvent(&event->ce, TRUE);
    *pX = *event->pX;
    *pY = *event->pY;

fail:
    if (pMX)
	XP_FREE(pMX);
    if (pMY)
	XP_FREE(pMY);
    if (event)
	XP_FREE(event);
}

/****************************************************************************/

typedef struct {
    ETEvent	      ce;
    int32	     *pX;
    int32	     *pY;
    int32	     *pLeft;
    int32	     *pTop;
} MozillaEvent_GetAvailScreenRect;


PR_STATIC_CALLBACK(void)
et_HandleEvent_GetAvailScreenRect(MozillaEvent_GetAvailScreenRect* e)
{
    FE_GetAvailScreenRect(e->ce.context, e->pX, e->pY, e->pLeft, e->pTop);
}

void
ET_PostGetAvailScreenRect(MWContext* context, int32 *pX, int32 *pY, int32 *pLeft, 
								    int32 *pTop)
{
    int32 *pMX = NULL, *pMY = NULL, *pMLeft = NULL, *pMTop = NULL;
    
    MozillaEvent_GetAvailScreenRect* event = PR_NEW(MozillaEvent_GetAvailScreenRect);
    if (event == NULL) 
        return;

    pMX = XP_ALLOC(sizeof(int32));
    if (pMX == NULL)
	goto fail;
    pMY = XP_ALLOC(sizeof(int32));
    if (pMY == NULL)
	goto fail;
    pMLeft = XP_ALLOC(sizeof(int32));
    if (pMLeft == NULL)
	goto fail;
    pMTop = XP_ALLOC(sizeof(int32));
    if (pMTop == NULL)
	goto fail;


    PR_InitEvent(&event->ce.event, context,
		 (PRHandleEventProc)et_HandleEvent_GetAvailScreenRect,
		 (PRDestroyEventProc)et_DestroyEvent_WaitForIt);
    event->ce.context = context;
    event->pX = pMX;
    event->pY = pMY;
    event->pLeft = pMLeft;
    event->pTop = pMTop;

    et_PostEvent(&event->ce, TRUE);
    *pX = *event->pX;
    *pY = *event->pY;
    *pLeft = *event->pLeft;
    *pTop = *event->pTop;

fail:
    if (pMX)
	XP_FREE(pMX);
    if (pMY)
	XP_FREE(pMY);
    if (pMLeft)
	XP_FREE(pMLeft);
    if (pMTop)
	XP_FREE(pMTop);
    if (event)
	XP_FREE(event);
}

/****************************************************************************/

typedef struct {
    ETEvent	      ce;
    char            * szText;
} MozillaEvent_GetSelectedText;


PR_STATIC_CALLBACK(void*)
et_HandleEvent_GetSelectedText(MozillaEvent_GetSelectedText* e)
{
    char * rv;

    rv = (char *)LO_GetSelectionText(e->ce.context);

    if (rv)
	rv = XP_STRDUP(rv);

    return rv;
}

char *
ET_PostGetSelectedText(MWContext* context)
{
    char * ret;
    MozillaEvent_GetSelectedText* event = XP_NEW_ZAP(MozillaEvent_GetSelectedText);
    if (event == NULL) 
        return(NULL);
    PR_InitEvent(&event->ce.event, context,
		 (PRHandleEventProc)et_HandleEvent_GetSelectedText,
		 (PRDestroyEventProc)et_DestroyEvent_GenericEvent);
    event->ce.context = context;

    ret = (char *) et_PostEvent(&event->ce, TRUE);

    return(ret);

}

/****************************************************************************/

typedef struct {
    ETEvent	      ce;
    int               location;
    int32             x, y;
} MozillaEvent_ScrollTo;


PR_STATIC_CALLBACK(void)
et_HandleEvent_ScrollTo(MozillaEvent_ScrollTo* e)
{
    FE_ScrollDocTo(e->ce.context, e->location, e->x, e->y);    
}

void
ET_PostScrollDocTo(MWContext* context, int loc, int32 x, int32 y)
{
    MozillaEvent_ScrollTo* event = PR_NEW(MozillaEvent_ScrollTo);
	if (event == NULL) 
        return;
    PR_InitEvent(&event->ce.event, context,
		 (PRHandleEventProc)et_HandleEvent_ScrollTo,
		 (PRDestroyEventProc)et_DestroyEvent_GenericEvent);
    event->ce.context = context;
    event->location = loc;
    event->x = x;
    event->y = y;

    et_PostEvent(&event->ce, FALSE);

}

/****************************************************************************/

typedef struct {
    ETEvent	      ce;
    int               location;
    int32             x, y;
} MozillaEvent_ScrollBy;


PR_STATIC_CALLBACK(void)
et_HandleEvent_ScrollBy(MozillaEvent_ScrollBy* e)
{
    FE_ScrollDocBy(e->ce.context, e->location, e->x, e->y);    
}

void
ET_PostScrollDocBy(MWContext* context, int loc, int32 x, int32 y)
{
    MozillaEvent_ScrollBy* event = PR_NEW(MozillaEvent_ScrollBy);
	if (event == NULL) 
        return;
    PR_InitEvent(&event->ce.event, context,
		 (PRHandleEventProc)et_HandleEvent_ScrollBy,
		 (PRDestroyEventProc)et_DestroyEvent_GenericEvent);
    event->ce.context = context;
    event->location = loc;
    event->x = x;
    event->y = y;

    et_PostEvent(&event->ce, FALSE);

}


/****************************************************************************/

PR_STATIC_CALLBACK(void)
et_HandleEvent_BackCommand(ETEvent* e)
{
    FE_BackCommand(e->context);    
}

void
ET_PostBackCommand(MWContext* context)
{
    ETEvent* event = PR_NEW(ETEvent);
    if (event == NULL) 
        return;
    PR_InitEvent(&event->event, context,
		 (PRHandleEventProc)et_HandleEvent_BackCommand,
		 (PRDestroyEventProc)et_DestroyEvent_GenericEvent);
    event->context = context;

    et_PostEvent(event, FALSE);

}

/****************************************************************************/

typedef struct {
    ETEvent	      ce;
} MozillaEvent_ForwardCommand;


PR_STATIC_CALLBACK(void)
et_HandleEvent_ForwardCommand(ETEvent* e)
{
    FE_ForwardCommand(e->context);    
}

void
ET_PostForwardCommand(MWContext* context)
{
    ETEvent* event = PR_NEW(ETEvent);
    if (event == NULL) 
        return;
    PR_InitEvent(&event->event, context,
		 (PRHandleEventProc)et_HandleEvent_ForwardCommand,
		 (PRDestroyEventProc)et_DestroyEvent_GenericEvent);
    event->context = context;

    et_PostEvent(event, FALSE);

}

/****************************************************************************/

PR_STATIC_CALLBACK(void)
et_HandleEvent_HomeCommand(ETEvent* e)
{
    FE_HomeCommand(e->context);    
}

void
ET_PostHomeCommand(MWContext* context)
{
    ETEvent* event = PR_NEW(ETEvent);
    if (event == NULL) 
        return;
    PR_InitEvent(&event->event, context,
		 (PRHandleEventProc)et_HandleEvent_HomeCommand,
		 (PRDestroyEventProc)et_DestroyEvent_GenericEvent);
    event->context = context;

    et_PostEvent(event, FALSE);

}

/****************************************************************************/

typedef struct {
    ETEvent	      ce;
    char *	      szName;
    XP_Bool	      matchCase;
    XP_Bool	      searchBackward;
} MozillaEvent_FindCommand;


PR_STATIC_CALLBACK(void*)
et_HandleEvent_FindCommand(MozillaEvent_FindCommand* e)
{
    return(void *)FE_FindCommand(e->ce.context, e->szName, e->matchCase, e->searchBackward, FALSE);    
}

PR_STATIC_CALLBACK(void)
et_DestroyEvent_FindCommand(MozillaEvent_FindCommand* event)
{
    if(event->szName)
	XP_FREE((char*)event->szName);
    XP_FREE(event);
}

JSBool
ET_PostFindCommand(MWContext* context, char *szName, JSBool matchCase,
		   JSBool searchBackward)
{
    JSBool ret;
    
    MozillaEvent_FindCommand* event = PR_NEW(MozillaEvent_FindCommand);
	if (event == NULL) 
        return JS_FALSE;
    PR_InitEvent(&event->ce.event, context,
		 (PRHandleEventProc)et_HandleEvent_FindCommand,
		 (PRDestroyEventProc)et_DestroyEvent_FindCommand);
    event->ce.context = context;
    if(szName)
	event->szName = strdup(szName);
    else
	event->szName = NULL;
    event->matchCase = (XP_Bool)matchCase;
    event->searchBackward = (XP_Bool)searchBackward;

    ret = (JSBool)et_PostEvent(&event->ce, TRUE);
    return(ret);

}

/****************************************************************************/

typedef struct {
    ETEvent	      ce;
} MozillaEvent_PrintCommand;

PR_STATIC_CALLBACK(void)
et_HandleEvent_PrintCommand(ETEvent* e)
{
    FE_PrintCommand(e->context);    
}

void
ET_PostPrintCommand(MWContext* context)
{
    ETEvent* event = PR_NEW(ETEvent);
    if (event == NULL) 
        return;
    PR_InitEvent(&event->event, context,
		 (PRHandleEventProc)et_HandleEvent_PrintCommand,
		 (PRDestroyEventProc)et_DestroyEvent_GenericEvent);
    event->context = context;

    et_PostEvent(event, FALSE);

}

/****************************************************************************/

PR_STATIC_CALLBACK(void)
et_HandleEvent_OpenFileCommand(ETEvent* e)
{
}

void
ET_PostOpenFileCommand(MWContext* context)
{
    ETEvent* event = PR_NEW(ETEvent);
    if (event == NULL) 
        return;
    PR_InitEvent(&event->event, context,
		 (PRHandleEventProc)et_HandleEvent_OpenFileCommand,
		 (PRDestroyEventProc)et_DestroyEvent_GenericEvent);
    event->context = context;

    et_PostEvent(event, FALSE);

}

/****************************************************************************/

typedef struct {
    ETEvent	ce;
    char*	szMessage;	/* message for dialog */
} MozillaEvent_HtmlAlert;

PR_STATIC_CALLBACK(void)
et_HandleEvent_HtmlAlert(MozillaEvent_HtmlAlert* e)
{
    XP_MakeHTMLAlert(e->ce.context, e->szMessage);
}

PR_STATIC_CALLBACK(void)
et_DestroyEvent_HtmlAlert(MozillaEvent_HtmlAlert* event)
{
    XP_FREE((char*)event->szMessage);
    XP_FREE(event);
}

void
ET_MakeHTMLAlert(MWContext* context, const char* szMessage)
{
    MozillaEvent_HtmlAlert* event = PR_NEW(MozillaEvent_HtmlAlert);
	if (event == NULL) 
        return;
    PR_InitEvent(&event->ce.event, context,
		 (PRHandleEventProc)et_HandleEvent_HtmlAlert,
		 (PRDestroyEventProc)et_DestroyEvent_HtmlAlert);
    event->ce.context = context;
    event->szMessage = strdup(szMessage);

    et_PostEvent(&event->ce, FALSE);

}

/****************************************************************************/

typedef struct {
    ETEvent	      ce;
    LO_Element      * pEle;
    int               type;
    ETClosureFunc     fnClosure;
    void            * pStuff;
    ETEventStatus     status;
} MozillaEvent_JsEventAck;


PR_STATIC_CALLBACK(void)
et_HandleEvent_JsEventAck(MozillaEvent_JsEventAck* e)
{
    /* make sure we haven't gone away somehow */
    if(e->ce.doc_id != XP_DOCID(e->ce.context))
	e->status = EVENT_PANIC;

    (*e->fnClosure) (e->ce.context, e->pEle, e->type, e->pStuff, e->status);
}

void
ET_PostJsEventAck(MWContext* context, LO_Element * pEle, int type, 
                  ETClosureFunc fnClosure, void * pStuff, 
                  ETEventStatus status)
{
    MozillaEvent_JsEventAck* event = PR_NEW(MozillaEvent_JsEventAck);
    if (event == NULL) 
        return;
    PR_InitEvent(&event->ce.event, context,
		 (PRHandleEventProc)et_HandleEvent_JsEventAck,
		 (PRDestroyEventProc)et_DestroyEvent_GenericEvent);
    event->ce.context = context;
    event->ce.doc_id = XP_DOCID(context);
    event->pEle = pEle;
    event->type = type;
    event->fnClosure = fnClosure;
    event->pStuff = pStuff;
    event->status = status;

    et_PostEvent(&event->ce, FALSE);

}


/****************************************************************************/

/****************************************************************************/
/****************************************************************************/

typedef struct {
    ETEvent		  ce;
    FO_Present_Types	  format;
    URL_Struct		* pUrl;
    void		* pStuff;
    uint32		  ulBytes;
} MozillaEvent_GenericNetLib;


PR_STATIC_CALLBACK(void*)
et_HandleEvent_CacheConverter(MozillaEvent_GenericNetLib* e)
{
    void *retval = (void *)NET_CacheConverter(e->format, e->pStuff, e->pUrl,
					      e->ce.context);
    return retval;
}

PR_STATIC_CALLBACK(void)
et_DestroyEvent_NetlibWithUrl(MozillaEvent_GenericNetLib* e)
{
    NET_DropURLStruct(e->pUrl);
    XP_FREE(e);
}


NET_StreamClass	*
ET_net_CacheConverter(FO_Present_Types format, void * obj,
                      URL_Struct *pUrl, MWContext * context)
{
    NET_StreamClass * ret;
    MozillaEvent_GenericNetLib* event = PR_NEW(MozillaEvent_GenericNetLib);
    if (event == NULL) 
        return(NULL);
    PR_InitEvent(&event->ce.event, context,
		 (PRHandleEventProc)et_HandleEvent_CacheConverter,
		 (PRDestroyEventProc)et_DestroyEvent_NetlibWithUrl);
    event->ce.context = context;
    event->format = format;
    event->pStuff = obj;
    event->pUrl = NET_HoldURLStruct(pUrl);

    ret = (NET_StreamClass *) et_PostEvent(&event->ce, TRUE);
    return(ret);

}

/****************************************************************************/

PR_STATIC_CALLBACK(void)
et_HandleEvent_FindURLInCache(MozillaEvent_GenericNetLib* e)
{
    NET_FindURLInCache(e->pUrl, e->ce.context);
}

/* NOTE: as far as libmocha is concerned, we just need this routine
   to complete.  We don't care about the return value
 */
void
ET_net_FindURLInCache(URL_Struct * pUrl, MWContext * pContext)
{
    MozillaEvent_GenericNetLib* event = PR_NEW(MozillaEvent_GenericNetLib);
    if (event == NULL) 
        return;
    PR_InitEvent(&event->ce.event, pContext,
		 (PRHandleEventProc)et_HandleEvent_FindURLInCache,
		 (PRDestroyEventProc)et_DestroyEvent_NetlibWithUrl);
    event->ce.context = pContext;
    event->pUrl = NET_HoldURLStruct(pUrl);

    et_PostEvent(&event->ce, TRUE);
}

/****************************************************************************/

PR_STATIC_CALLBACK(void*)
et_HandleEvent_StreamBuilder(MozillaEvent_GenericNetLib* e)
{
    NET_StreamClass *rv;
	
    NET_SetActiveEntryBusyStatus(e->pUrl, TRUE);
    rv = NET_StreamBuilder(e->format, e->pUrl, e->ce.context);
    NET_SetActiveEntryBusyStatus(e->pUrl, FALSE);
    return((void *)rv);
}

NET_StreamClass	*
ET_net_StreamBuilder(FO_Present_Types format, URL_Struct *pUrl, 
		     MWContext * pContext)
{
    NET_StreamClass * ret;
    MozillaEvent_GenericNetLib* event = PR_NEW(MozillaEvent_GenericNetLib);
    if (event == NULL) 
        return(NULL);
    PR_InitEvent(&event->ce.event, pContext,
		 (PRHandleEventProc)et_HandleEvent_StreamBuilder,
		 (PRDestroyEventProc)et_DestroyEvent_NetlibWithUrl);
    event->ce.context = pContext;
    event->format = format;
    event->pUrl = NET_HoldURLStruct(pUrl);

    ret = (NET_StreamClass *) et_PostEvent(&event->ce, TRUE);
    return(ret);

}

/****************************************************************************/

typedef struct {
    ETEvent		  ce;
    NET_StreamClass	* stream;
    void		* data;
    size_t		  len;
    JSBool		  self_modifying;
    JSBool		  processed;
} MozillaEvent_DocWrite;

/*
 * A lot of this should get moved out to the layout module
 * I don't think we need to bother locking layout in this function
 *   since we are running in the mozilla thread	and the mocha
 *   thread is blocked waiting for our return value.
 */
PR_STATIC_CALLBACK(void*)
et_HandleEvent_DocWrite(MozillaEvent_DocWrite* e)
{
    lo_TopState *top_state;
    int32 pre_doc_id;
    LO_Element * save_blocking = NULL;
    LO_Element * current_script = NULL;
    Bool bumped_no_newline_count = FALSE;
    uint save_overflow;
    int status;

    e->processed = JS_TRUE;

    /*
     * If the context has a doc_id of -1 it means that its being destroyed
     * If the context's doc_id has changed only process the event if the
     *   original doc_id was zero (i.e. this is the first write and it will
     *   create a new doc with a new doc_id)
     */
    pre_doc_id = XP_DOCID(e->ce.context);
    if ((e->ce.doc_id && e->ce.doc_id != pre_doc_id) || (pre_doc_id == -1)) {
#ifdef DEBUG_chouck
	XP_TRACE(("Ignoring doc.write since doc_id changed"));
#endif
	ET_DocWriteAck(e->ce.context, -1);
	return((void *) -1);
    }

    if (!ET_ContinueProcessing(e->ce.context)) {
#ifdef DEBUG_chouck
	XP_TRACE(("Ignoring doc.write since was interrupted"));
#endif
	ET_DocWriteAck(e->ce.context, -1);
	return((void *) -1);
    }

    /* make sure top_state doesn't go away while we are holding onto it */
    LO_LockLayout();

    top_state = lo_GetMochaTopState(e->ce.context);

    /* tell layout that we are writing */
    if (top_state) {
        if (top_state->input_write_level >= MAX_INPUT_WRITE_LEVEL-1) {
	    LO_UnlockLayout();
	    ET_DocWriteAck(e->ce.context, -1);
            return ((void *) -1); 
        }
        top_state->input_write_level++;

        if (top_state->doc_data) {
	    /* Suppress generated line counting if self-modifying. */
	    if (e->self_modifying) {
		top_state->doc_data->no_newline_count++;
		bumped_no_newline_count = TRUE;
	    }

	    /* Save the overflow counter and XXX */
	    save_overflow = top_state->doc_data->overflow_depth;
	    top_state->doc_data->overflow_depth = 0;
	}
	else {
	    save_overflow = 0;
	}

	current_script = top_state->current_script;

        /* 
         * if we are currently blocked by a <script> tag unblock
         *   layout for this putblock since this is some of the
         *   script data coming through.  If we are blocked on
         *   something other than a script tag don't unblock, this
         *   stuff will just get shoved onto the list to be
         *   processed after we get unblocked
         */
        save_blocking = top_state->layout_blocking_element;
        if (save_blocking && save_blocking->type == LO_SCRIPT)
            top_state->layout_blocking_element = NULL;

    }

    LO_UnlockLayout();

    /* shove the data out */
    status = (*e->stream->put_block)(e->stream, e->data, e->len);

    LO_LockLayout();

    /* I doubt top_state could have changed but try to be safe */
    top_state = lo_GetMochaTopState(e->ce.context);

    /* if the doc is still around we are done so clean up */
    if (top_state) {
	/* Stop suppressing generated line counting if self-modifying. */
	if (bumped_no_newline_count) {
	    XP_ASSERT(top_state->doc_data);
	    top_state->doc_data->no_newline_count--;
	}

	if (XP_DOCID(e->ce.context) == pre_doc_id) {
	    if (top_state->doc_data) {
		top_state->doc_data->overflow_depth += save_overflow;
		XP_ASSERT(top_state->doc_data->overflow_depth >= 0);
	    }
	    if(top_state->layout_blocking_element == NULL) {
		/* 
		 * the stuff we wrote didn't block us.  continue to block
		 *   on the script tag
		 */
		top_state->layout_blocking_element = save_blocking;
		top_state->input_write_level--;
		ET_DocWriteAck(e->ce.context, status);
	    }
	    else {
		/* 
		 * If we had been blocked on the script tag but now we are
		 *   blocked by something else make sure we reblock on
		 *   the script tag when the new thing is done
		 */
		LO_CreateReblockTag(e->ce.context, current_script);
	    }
	}
	else {
	    /* 
	     * we just created a new top_state so its input_write_level
	     *   value should be OK.  Don't mess with it.
	     */
	    ET_DocWriteAck(e->ce.context, status);
	}
    } 

    LO_UnlockLayout();

    return((void *) status);

}

PR_STATIC_CALLBACK(void)
et_DestroyEvent_DocWrite(MozillaEvent_DocWrite* e)
{
    if (!e->processed)
	ET_DocWriteAck(e->ce.context, -1);
    XP_FREE(e);
}

static uint et_EventQueueDepth;	/* number of active entries on stack */
static uint et_EventQueueCount;	/* allocated entries, including inactive */

static QueueStackElement *
et_PushEventQueue(MWContext * context)
{
    QueueStackElement * qse;
    char name[32];

    /* catch script src= tags that generate themselves. */
    if (et_EventQueueDepth >= 5)
	return NULL;

    /* see if we've already got one */
    qse = et_TopQueue->up;
    if (!qse) {
	qse = XP_NEW_ZAP(QueueStackElement);
	if (!qse)
	    return NULL;

	PR_snprintf(name, sizeof name, "mocha-stack-queue-%d", et_EventQueueDepth + 1);
	qse->queue  = PR_CreateEventQueue(name, lm_InterpretThread);

	if (!qse->queue) {
	    XP_DELETE(qse);
	    return NULL;
	}

	et_EventQueueCount++;
	qse->down = et_TopQueue;
	et_TopQueue->up = qse;
    }
    et_EventQueueDepth++;
    qse->context = context;
    qse->done = FALSE;

    /*
     * This should get set by our caller
     */
    qse->doc_id = -1;  

    et_TopQueue = qse;
    return qse;

}

static void *		      
et_PopEventQueue(void)
{
    QueueStackElement * qse;
    void * ret;
    
    qse = et_TopQueue;
    ret = qse->retval;
    et_TopQueue = qse->down;
    et_EventQueueDepth--;
    if (et_EventQueueCount > 2) {
	/* free the entry we're popping */
	et_TopQueue->up = NULL;
	PR_DestroyEventQueue(qse->queue);
	XP_DELETE(qse);
	et_EventQueueCount--;
    }
    return ret;
}

/*
 * Send the string str over to the mozilla thread and push it through
 *   layout.  This is an asynchronous event as far as NSPR is concerned
 *   but incase str pushes up any script or style tags we will enter a
 *   sub event loop to handle reflections and evaluations generated by
 *   this write but otherwise block the mocha thread until they have 
 *   all been proceed.
 */
int
ET_lo_DoDocWrite(JSContext *cx, MWContext * context, NET_StreamClass * stream,
                 char * str, size_t len, int32 doc_id)
{
    MochaDecoder * decoder;
    MozillaEvent_DocWrite * event;
    QueueStackElement * qse;
    int ret = -1;

    decoder = LM_GetMochaDecoder(context);
    if (!decoder)
	return ret;

#ifdef XP_UNIX
	/*
	 * NOTE:  we need to toss the string out if it's doc_id doesn't match
	 *        the current XP_DOCID...
	 *
	 *        the problem could be reproduced as follows on an IRIX 6.3
	 *        O2 workstation with "lots" of plugins;
	 *
	 *        1. load a "plain" HTML page...
	 *        2. select "about:plugins" from the help menu...
	 *        3. select "about:communicator" from the help menu...
	 *        4. hit the "back" button quickly to move across the
	 *            "about:plugins" page before it's had a chance to
	 *            finish loading...
	 *        5. crashes trying to execute events from the "about:plugins"
	 *            page in the context of the "plain" HTML page...
	 *
	 * WARNING:  there seems to be a memory corruption problem somewhere
	 *           in the "event" system.  Follow the same procedure I 
	 *           outlined above, except instead of loading "about:plugins"
	 *           from the menu, load the "aboutplg.html" source file 
	 *           directly.  If you switch rapidly across this page 
	 *           you will eventually see errors like "freeing free 
	 *           chunk" and "passing junk pointer to realloc" in the 
	 *           course of freeing ETEvents.  After a while, this will 
	 *           result in a variety of random crashes due to memory 
	 *           corruption errors... [ filing a new bug for this one ]
	 *
	 */
	if (doc_id && (doc_id != XP_DOCID(context))) {
		LM_PutMochaDecoder(decoder);
		return ret;
	}
#endif

    event = XP_NEW_ZAP(MozillaEvent_DocWrite);
    if (event == NULL) {
	LM_PutMochaDecoder(decoder);
        return ret;
    }

    qse = et_PushEventQueue(context);
    if (!qse) {
	XP_FREEIF(event);
	LM_PutMochaDecoder(decoder);
        return ret;
    }

    /*
     * Set the reciever doc_id to the one that gets passed in
     */
    et_TopQueue->doc_id = doc_id;

    PR_InitEvent(&event->ce.event, context,
		 (PRHandleEventProc)et_HandleEvent_DocWrite,
		 (PRDestroyEventProc)et_DestroyEvent_DocWrite);
    event->ce.context = context;
    event->ce.doc_id = doc_id;
    event->data = str;
    event->len = len;
    event->stream = stream;
    event->self_modifying = (JSBool)(cx == decoder->js_context);

    et_PostEvent(&event->ce, FALSE);

    /* spin here until we get our DocWriteAck */
    et_SubEventLoop(qse);
    ret = (int) et_PopEventQueue();

    /* Sample the doc_id, since we know that it's good */
    /* XXX do this in InitWindowContent only, not here and in DefineDocument */
    decoder->doc_id = XP_DOCID(context);

    LM_PutMochaDecoder(decoder);

    return ret;

}

/****************************************************************************/

PR_STATIC_CALLBACK(Bool)
et_HandleEvent_PrepareLayerForWriting(JSEvent* e)
{
    CL_Layer *layer;
    Bool rv;

    if (e->ce.doc_id != XP_DOCID(e->ce.context))
	return FALSE;

    LO_LockLayout();
    layer = LO_GetLayerFromId(e->ce.context, e->layer_id);
    
    /* e->data points to the referer string, or null */
    rv =  LO_PrepareLayerForWriting(e->ce.context, e->layer_id,
				    (const char *)e->data,
                                    LO_GetLayerWrapWidth(layer));
    LO_UnlockLayout();
    
    return rv;
}

PR_STATIC_CALLBACK(void)
et_DestroyEvent_PrepareLayerForWriting(JSEvent* e)
{
    XP_FREEIF(e->data);
    XP_FREE(e);
}

Bool
ET_lo_PrepareLayerForWriting(MWContext *pContext, int32 layer_id,
			     const char *referer)
{
    JSEvent      * pEvent = (JSEvent *) XP_NEW_ZAP(JSEvent);
    if(!pEvent)
        return FALSE;

    PR_InitEvent(&pEvent->ce.event, pContext,
                 (PRHandleEventProc)et_HandleEvent_PrepareLayerForWriting,
                 (PRDestroyEventProc)et_DestroyEvent_PrepareLayerForWriting);

    /* fill in the non-PR fields we care about */
    pEvent->ce.context = pContext;
    pEvent->layer_id = layer_id;
    pEvent->ce.doc_id = XP_DOCID(pContext);
    pEvent->data = referer ? XP_STRDUP(referer) : NULL;

    return (Bool)et_PostEvent(&pEvent->ce, TRUE);
}


/****************************************************************************/

typedef struct {
    ETEvent              ce;
    ETEvalAckFunc        fn;
    void               * data;
    char               * str;
    size_t               len;
    char	       * wysiwyg_url;
    char	       * base_href;
    Bool                 valid;
} MozillaEvent_EvalAck;


PR_STATIC_CALLBACK(void)
et_HandleEvent_EvalAck(MozillaEvent_EvalAck* e)
{
    if (e->ce.doc_id == XP_DOCID(e->ce.context)) {
	(e->fn) (e->data, e->str, e->len, e->wysiwyg_url, 
		 e->base_href, e->valid);
    }
    else {
#ifdef DEBUG_chouck
	XP_TRACE(("et_HandleEvent_EvalAck failed doc_id"));
#endif
    }
}

void
ET_PostEvalAck(MWContext * context, int doc_id,
	       void * data, char * str, size_t len,
	       char * wysiwyg_url, char * base_href,
	       Bool valid, ETEvalAckFunc fn)
{

    MozillaEvent_EvalAck* event;
    if (fn == NULL)
        return;

    event = XP_NEW_ZAP(MozillaEvent_EvalAck);
    if (event == NULL) 
        return;
    PR_InitEvent(&event->ce.event, NULL,
		 (PRHandleEventProc)et_HandleEvent_EvalAck,
		 (PRDestroyEventProc)et_DestroyEvent_GenericEvent);
    event->ce.context = context;
    event->ce.doc_id = doc_id;
    event->data = data;
    event->str = str;
    event->len = len;
    event->valid = valid;
    event->fn = fn;
    event->wysiwyg_url = wysiwyg_url;
    event->base_href = base_href;

    (void) et_PostEvent(&event->ce, FALSE);

}

/****************************************************************************/

typedef struct {
    ETEvent              ce;
    ETRestoreAckFunc     fn;
    void               * data;
    LO_BlockInitializeStruct *param;
} MozillaEvent_RestoreAck;


PR_STATIC_CALLBACK(void)
et_HandleEvent_RestoreAck(MozillaEvent_RestoreAck* e)
{
    /* XXX Need to do a doc_id check to see if this is still valid */
    (e->fn) (e->data, e->param);
}

void
ET_PostRestoreAck(void * data, LO_BlockInitializeStruct *param, 
                  ETRestoreAckFunc fn)
{

    MozillaEvent_RestoreAck* event;
    if (fn == NULL)
        return;

    event = XP_NEW_ZAP(MozillaEvent_RestoreAck);
    if (event == NULL) 
        return;
    PR_InitEvent(&event->ce.event, NULL,
		 (PRHandleEventProc)et_HandleEvent_RestoreAck,
		 (PRDestroyEventProc)et_DestroyEvent_GenericEvent);
    event->data = data;
    event->param = param;
    event->fn = fn;

    (void) et_PostEvent(&event->ce, FALSE);

}

/****************************************************************************/

typedef struct MozillaEvent_DiscardStruct {
    ETEvent	ce;
    JSBool	processed;
} MozillaEvent_DiscardStruct;

PR_STATIC_CALLBACK(void)
et_HandleEvent_DiscardDocument(MozillaEvent_DiscardStruct * e)
{
    e->processed = JS_TRUE;
    LO_DiscardDocument(e->ce.context);
    ET_DocWriteAck(e->ce.context, 0);
}

PR_STATIC_CALLBACK(void)
et_DestroyEvent_DiscardDocument(MozillaEvent_DiscardStruct * e)
{
    if (!e->processed)
        ET_DocWriteAck(e->ce.context, 0);
}

void
ET_lo_DiscardDocument(MWContext * pContext)
{
    MozillaEvent_DiscardStruct * event;
    QueueStackElement * qse;
    
    event = PR_NEW(MozillaEvent_DiscardStruct);
    if (event == NULL) 
        return;

    qse = et_PushEventQueue(pContext);
    if (!qse) {
	PR_DELETE(event);
	return;
    }

    et_TopQueue->doc_id = XP_DOCID(pContext);

    PR_InitEvent(&event->ce.event, pContext,
		 (PRHandleEventProc)et_HandleEvent_DiscardDocument,
		 (PRDestroyEventProc)et_DestroyEvent_DiscardDocument);
    event->ce.context = pContext;

    qse->discarding = TRUE;
    et_PostEvent(&event->ce, FALSE);

    /*
     * Spin here until we get a ReleaseDocument (if needed) followed by a
     * WriteAck send by our handler.
     */
    et_SubEventLoop(qse);

    qse->discarding = FALSE;
    et_PopEventQueue();
}

/****************************************************************************/

typedef struct {
    ETEvent              ce;
    ETVoidPtrFunc        fn;
    void               * data;
} MozillaEvent_CallFunc;

PR_STATIC_CALLBACK(void)
et_HandleEvent_CallFunction(MozillaEvent_CallFunc* e)
{
    (e->fn) (e->data);
}

void
ET_moz_CallFunction(ETVoidPtrFunc fn, void * data)
{
    MozillaEvent_CallFunc* event = PR_NEW(MozillaEvent_CallFunc);
    if (event == NULL) 
	return;
    PR_InitEvent(&event->ce.event, NULL,
		 (PRHandleEventProc)et_HandleEvent_CallFunction,
		 (PRDestroyEventProc)et_DestroyEvent_GenericEvent);
    event->fn = fn;
    event->data = data;

    et_PostEvent(&event->ce, TRUE);
}

void
ET_moz_CallFunctionAsync(ETVoidPtrFunc fn, void * data)
{
    MozillaEvent_CallFunc* event = PR_NEW(MozillaEvent_CallFunc);
    if (event == NULL) 
	return;
    PR_InitEvent(&event->ce.event, NULL,
		 (PRHandleEventProc)et_HandleEvent_CallFunction,
		 (PRDestroyEventProc)et_DestroyEvent_GenericEvent);
    event->fn = fn;
    event->data = data;

    (void) et_PostEvent(&event->ce, FALSE);
}

/****************************************************************************/

typedef struct {
    ETEvent              ce;
    ETBoolPtrFunc        fn;
    void               * data;
} MozillaEvent_CallFuncBool;

PR_STATIC_CALLBACK(PRBool)
et_HandleEvent_CallFunctionBool(MozillaEvent_CallFuncBool* e)
{
    return (e->fn) (e->data);
}

PRBool
ET_moz_CallFunctionBool(ETBoolPtrFunc fn, void * data)
{
    PRBool ret;
    MozillaEvent_CallFuncBool* event = PR_NEW(MozillaEvent_CallFuncBool);
    if (event == NULL) 
	return PR_FALSE;
    PR_InitEvent(&event->ce.event, NULL,
		 (PRHandleEventProc)et_HandleEvent_CallFunctionBool,
		 (PRDestroyEventProc)et_DestroyEvent_GenericEvent);
    event->fn = fn;
    event->data = data;

    ret = (PRBool) et_PostEvent(&event->ce, TRUE);
    return ret;
}

/****************************************************************************/
typedef struct {
    ETEvent              ce;
    ETIntPtrFunc        fn;
    void               * data;
} MozillaEvent_CallFuncInt;

PR_STATIC_CALLBACK(int32)
et_HandleEvent_CallFunctionInt(MozillaEvent_CallFuncInt* e)
{
    return (e->fn) (e->data);
}

int32
ET_moz_CallFunctionInt(ETIntPtrFunc fn, void * data)
{
    PRBool ret;
    MozillaEvent_CallFuncInt* event = PR_NEW(MozillaEvent_CallFuncInt);
    if (event == NULL) 
	return PR_FALSE;
    PR_InitEvent(&event->ce.event, NULL,
		 (PRHandleEventProc)et_HandleEvent_CallFunctionInt,
		 (PRDestroyEventProc)et_DestroyEvent_GenericEvent);
    event->fn = fn;
    event->data = data;

    ret = (int32) et_PostEvent(&event->ce, TRUE);
    return ret;
}

/****************************************************************************/

typedef struct {
    ETEvent              ce;
    ETStringPtrFunc        fn;
    void               * data;
} MozillaEvent_CallFuncString;

PR_STATIC_CALLBACK(char *)
et_HandleEvent_CallFunctionString(MozillaEvent_CallFuncString* e)
{
    return (e->fn) (e->data);
}

char *
ET_moz_CallFunctionString(ETStringPtrFunc fn, void * data)
{
    char * ret;
    MozillaEvent_CallFuncString* event = PR_NEW(MozillaEvent_CallFuncString);
    if (event == NULL) 
	return PR_FALSE;
    PR_InitEvent(&event->ce.event, NULL,
		 (PRHandleEventProc)et_HandleEvent_CallFunctionString,
		 (PRDestroyEventProc)et_DestroyEvent_GenericEvent);
    event->fn = fn;
    event->data = data;

    ret = (char *) et_PostEvent(&event->ce, TRUE);
    return ret;
}

/****************************************************************************/

void
ET_moz_CallAsyncAndSubEventLoop(ETVoidPtrFunc fn, void *data,
				MWContext *context)
{
    QueueStackElement *qse;
    
    qse = et_PushEventQueue(context);
    if ( qse == NULL ) {
	return;
    }

    qse->inherit_parent = JS_TRUE;
    qse->doc_id = 0;

    ET_moz_CallFunctionAsync(fn, data);
    
    et_SubEventLoop(qse);
    (void)et_PopEventQueue();
    return;
}

/****************************************************************************/
typedef struct {
    ETEvent              ce;
    MKStreamAbortFunc    fn;
    void               * data;
    int			 status;
} MozillaEvent_CallAbort;

PR_STATIC_CALLBACK(void)
et_HandleEvent_Abort(MozillaEvent_CallAbort* e)
{
    (*e->fn)(e->data, e->status);
}

void
ET_moz_Abort(MKStreamAbortFunc fn, void * data, int status)
{

    MozillaEvent_CallAbort* event = PR_NEW(MozillaEvent_CallAbort);
    if (event == NULL) 
        return;
    PR_InitEvent(&event->ce.event, NULL,
		 (PRHandleEventProc)et_HandleEvent_Abort,
		 (PRDestroyEventProc)et_DestroyEvent_GenericEvent);
    event->fn = fn;
    event->data = data;
    event->status = status;

    (void) et_PostEvent(&event->ce, TRUE);
}


/****************************************************************************/

typedef struct {
    ETEvent		 ce;
    IL_GroupContext    * img_cx;
    LO_ImageStruct     * img;
    char               * str;
    NET_ReloadMethod     how;
} MozillaEvent_GetImage;

PR_STATIC_CALLBACK(void)
et_HandleEvent_GetImage(MozillaEvent_GetImage* e)
{
    IL_DisplayData dpy_data;
    MWContext *pContext = e->ce.context;
    IL_GroupContext *img_cx = e->img_cx;
    LO_ImageStruct *lo_image = e->img;
    const char *url = e->str;
    
    if (e->ce.doc_id != XP_DOCID(pContext))
	return;

    XP_ASSERT(pContext->color_space);
    if (!pContext->color_space)
        return;
    dpy_data.color_space = pContext->color_space;
    dpy_data.dither_mode = IL_Auto;
	IL_SetDisplayMode(img_cx, IL_COLOR_SPACE | IL_DITHER_MODE, &dpy_data);
	
    LO_SetImageURL(pContext, img_cx, lo_image, url, e->how);
    lo_image->pending_mocha_event = PR_FALSE;
}

PR_STATIC_CALLBACK(void)
et_DestroyEvent_GetImage(MozillaEvent_GetImage* e)
{
    if (e->str)
        XP_FREE(e->str);
    XP_FREE(e);
}

void
ET_il_GetImage(const char * str, MWContext * pContext, IL_GroupContext *img_cx,
               LO_ImageStruct * image_data, NET_ReloadMethod how)
{

    MozillaEvent_GetImage* event = PR_NEW(MozillaEvent_GetImage);
	if (event == NULL) 
        return;
    PR_InitEvent(&event->ce.event, pContext,
		 (PRHandleEventProc)et_HandleEvent_GetImage,
		 (PRDestroyEventProc)et_DestroyEvent_GetImage);
    event->ce.context = pContext;
    event->ce.doc_id = XP_DOCID(pContext);
    event->str = str ? strdup(str) : NULL;
    event->img_cx = img_cx;
    event->img = image_data;
    event->how = how;

   (void) et_PostEvent(&event->ce, FALSE);

}

/****************************************************************************/

typedef struct {
    ETEvent		ce;
    IL_GroupContext	*pImgCX;
    void		*pDpyCX;
    JSBool		bAddObserver;
} MozillaEvent_GroupObserver;

PR_STATIC_CALLBACK(void)
et_HandleEvent_SetGroupObserver(MozillaEvent_GroupObserver* e)
{
    MWContext *pContext = e->ce.context;
    IL_DisplayData dpy_data;
     
    if (e->bAddObserver)
	IL_AddGroupObserver(e->pImgCX, FE_MochaImageGroupObserver, pContext);
    else
	IL_RemoveGroupObserver(e->pImgCX, FE_MochaImageGroupObserver, pContext);

    dpy_data.display_context = e->pDpyCX;
    IL_SetDisplayMode(e->pImgCX, IL_DISPLAY_CONTEXT, &dpy_data);
}

void
ET_il_SetGroupObserver(MWContext * pContext, IL_GroupContext *pImgCX, void *pDpyCX,
		       JSBool bAddObserver)
{

    MozillaEvent_GroupObserver* event = PR_NEW(MozillaEvent_GroupObserver);
    if (event == NULL) 
        return;
    PR_InitEvent(&event->ce.event, pContext,
		 (PRHandleEventProc)et_HandleEvent_SetGroupObserver,
		 (PRDestroyEventProc)et_DestroyEvent_GenericEvent);
    event->ce.context = pContext;
    event->pImgCX = pImgCX;
    event->pDpyCX = pDpyCX;
    event->bAddObserver = bAddObserver;

    (void) et_PostEvent(&event->ce, TRUE);

}

/****************************************************************************/

typedef struct {
    ETEvent	      ce;
    LO_Element	    * ele;
} MozillaEvent_Form;

PR_STATIC_CALLBACK(void)
et_HandleEvent_ResetForm(MozillaEvent_Form* e)
{
    LO_ResetForm(e->ce.context, (LO_FormElementStruct *) e->ele);
}

void
ET_lo_ResetForm(MWContext * pContext, LO_Element * ele)
{

    MozillaEvent_Form* event = XP_NEW_ZAP(MozillaEvent_Form);
	if (event == NULL) 
        return;
    PR_InitEvent(&event->ce.event, pContext,
		 (PRHandleEventProc)et_HandleEvent_ResetForm,
		 (PRDestroyEventProc)et_DestroyEvent_GenericEvent);
    event->ce.context = pContext;
    event->ele = ele;

    (void) et_PostEvent(&event->ce, TRUE);
}

/****************************************************************************/

PR_STATIC_CALLBACK(void)
et_HandleEvent_SubmitForm(MozillaEvent_Form* e)
{
/* XXX - I think this code should be here, but chouck must 
   have final say - fur
    if (e->ce.doc_id != XP_DOCID(e->ce.context))
	return;
*/

    FE_SubmitInputElement(e->ce.context, e->ele);
}

void
ET_fe_SubmitInputElement(MWContext * pContext, LO_Element * ele)
{

    MozillaEvent_Form* event = XP_NEW_ZAP(MozillaEvent_Form);
    if (event == NULL) 
        return;
    PR_InitEvent(&event->ce.event, pContext,
		 (PRHandleEventProc)et_HandleEvent_SubmitForm,
		 (PRDestroyEventProc)et_DestroyEvent_GenericEvent);
    event->ce.context = pContext;
    event->ce.doc_id = XP_DOCID(pContext);
    event->ele = ele;

    (void) et_PostEvent(&event->ce, FALSE);

}

/****************************************************************************/
PR_STATIC_CALLBACK(void*)
et_HandleEvent_GetSecurityStatus(ETEvent * e)
{
    return (void *) XP_GetSecurityStatus(e->context);
}

int
ET_GetSecurityStatus(MWContext * pContext)
{
    int ret = 0;
    ETEvent * event = XP_NEW_ZAP(ETEvent);
    if (event == NULL) 
        return -1; /* SSL_SECURITY_STATUS_NOOPT */
    PR_InitEvent(&event->event, pContext,
		 (PRHandleEventProc)et_HandleEvent_GetSecurityStatus,
		 (PRDestroyEventProc)et_DestroyEvent_GenericEvent);
    event->context = pContext;

    ret = (int) et_PostEvent(event, TRUE);
    return ret;
}

/****************************************************************************/

typedef struct {
    ETEvent		  ce;
    URL_Struct		* pUrl;
    const char		* wysiwyg_url;
    const char		* base_href;
    int32                 layer_id;
} MozillaEvent_DocCacheConverter;


PR_STATIC_CALLBACK(void)
et_HandleEvent_SetWriteStream(MozillaEvent_DocCacheConverter * e)
{
    lo_TopState *top_state;

    top_state = lo_GetMochaTopState(e->ce.context);
    if (top_state && !top_state->mocha_write_stream) {
        top_state->mocha_write_stream
            = NET_CloneWysiwygCacheFile(e->ce.context, 
				        e->pUrl,
				        (uint32)top_state->script_bytes,
					e->wysiwyg_url,
					e->base_href);
    }
}

PR_STATIC_CALLBACK(void)
et_DestroyEvent_SetWriteStream(MozillaEvent_DocCacheConverter * e)
{
    NET_DropURLStruct(e->pUrl);
    if (e->wysiwyg_url)
	XP_FREE((char *) e->wysiwyg_url);
    if (e->base_href)
	XP_FREE((char *) e->base_href);
    XP_FREE(e);
}

void
ET_moz_SetMochaWriteStream(MochaDecoder * decoder)
{
    MozillaEvent_DocCacheConverter * event;
    JSPrincipals *principals;

    event = XP_NEW_ZAP(MozillaEvent_DocCacheConverter);
    if (!event) 
        return;

    PR_InitEvent(&event->ce.event, decoder->window_context,
		 (PRHandleEventProc)et_HandleEvent_SetWriteStream,
		 (PRDestroyEventProc)et_DestroyEvent_SetWriteStream);

    event->ce.context = decoder->window_context;
    event->pUrl = NET_HoldURLStruct(decoder->url_struct);
    principals = lm_GetPrincipalsFromStackFrame(decoder->js_context);
    event->wysiwyg_url = lm_MakeWysiwygUrl(decoder->js_context, decoder,
                                           LO_DOCUMENT_LAYER_ID, principals);
    event->base_href = LM_GetBaseHrefTag(decoder->js_context, principals);

    et_PostEvent(&event->ce, TRUE);
}


/****************************************************************************/

PRIVATE NET_StreamClass *
lm_DocCacheConverterNoHistory(MWContext * context, URL_Struct * url,
			      const char * wysiwyg_url)
{
    lo_TopState *top_state;
    char *address;
    NET_StreamClass *cache_stream;

    top_state = lo_GetMochaTopState(context);
    if (!top_state)
	return NULL;

    /* Save a wysiwyg copy in the URL struct for resize-reloads. */
    url->wysiwyg_url = XP_STRDUP(wysiwyg_url);
    if (!url->wysiwyg_url)
	return NULL;
    
    /* Then pass it via url_struct to create a cache converter stream. */
    address = url->address;
    url->address = url->wysiwyg_url;
    cache_stream = NET_CacheConverter(FO_CACHE_ONLY,
				      (void *)1,    /* XXX don't hold url */
				      url,
				      context);
    url->address = address;

    top_state->mocha_write_stream = cache_stream;
    return cache_stream;
}

NET_StreamClass *
lm_DocCacheConverter(MWContext * context, URL_Struct * url,
		     const char * wysiwyg_url)
{
    History_entry *he;    
    NET_StreamClass *cache_stream;

    cache_stream = lm_DocCacheConverterNoHistory(context, url, wysiwyg_url);

    /*
     * Make a copy of wysiwyg_url in our history entry no matter what went
     * wrong building cache_stream.  This way, NET_GetURL can see the wysiwyg:
     * prefix, notice when it misses the cache, and clear URL_s->resize_reload
     * to make the reload from the original/generating URL destructive.
     */
    he = SHIST_GetCurrent(&context->hist);
    if (he) {
        PR_FREEIF(he->wysiwyg_url);
        he->wysiwyg_url = XP_STRDUP(wysiwyg_url);
    }

    return cache_stream;
}

PR_STATIC_CALLBACK(NET_StreamClass *)
et_HandleEvent_DocCacheConverter(MozillaEvent_DocCacheConverter * e)
{
    NET_StreamClass * stream;
    
    if (e->layer_id != LO_DOCUMENT_LAYER_ID) {
        return lm_DocCacheConverterNoHistory(e->ce.context, 
                                             e->pUrl, 
                                             e->wysiwyg_url);
    }
    
    stream = lm_DocCacheConverter(e->ce.context, 
                                  e->pUrl, 
                                  e->wysiwyg_url);
    return stream;
}

PR_STATIC_CALLBACK(void)
et_DestroyEvent_DocCacheConverter(MozillaEvent_DocCacheConverter * e)
{
    NET_DropURLStruct(e->pUrl);
    XP_FREE(e);
}

NET_StreamClass *
ET_moz_DocCacheConverter(MWContext * pContext, URL_Struct * pUrl, 
			 char * wysiwyg_url, int32 layer_id)
{
    NET_StreamClass * ret = NULL;
    MozillaEvent_DocCacheConverter * event;
    
    event = XP_NEW_ZAP(MozillaEvent_DocCacheConverter);
    if (!event) 
        return ret;
    PR_InitEvent(&event->ce.event, pContext,
		 (PRHandleEventProc)et_HandleEvent_DocCacheConverter,
		 (PRDestroyEventProc)et_DestroyEvent_DocCacheConverter);
    event->ce.context = pContext;
    event->pUrl = NET_HoldURLStruct(pUrl);
    event->wysiwyg_url = wysiwyg_url;
    event->layer_id = layer_id;

    ret = et_PostEvent(&event->ce, TRUE);
    return ret;
}

/****************************************************************************/

PR_STATIC_CALLBACK(void*)
et_HandleEvent_InitMoja(ETEvent* e)
{
    return (void *) LM_InitMoja();
}

int
ET_InitMoja(MWContext* context)
{
    int returnCode;
    ETEvent * event;

    /* XXX assert that we're on the mocha-thread */

    /* fast check before sending an event and waiting for it */
    returnCode = LM_IsMojaInitialized();
    if (returnCode != LM_MOJA_UNINITIALIZED)
        return returnCode;

    event = PR_NEW(ETEvent);
    if (event == NULL) 
        return LM_MOJA_OUT_OF_MEMORY;
    PR_InitEvent(&event->event, context,
		 (PRHandleEventProc)et_HandleEvent_InitMoja,
		 (PRDestroyEventProc)et_DestroyEvent_GenericEvent);
    event->context = context;

    returnCode = (int) et_PostEvent(event, TRUE);
    return returnCode;
}


/****************************************************************************/

typedef struct {
    ETEvent	    ce;
    CL_Layer	  * layer;
    int32	    x, y;
    void          * param_ptr;
    int32           param_val;
    ETLayerOp	    op;
    int32	    doc_id;
    char        * referer;
} MozillaEvent_TweakLayer; 

PR_STATIC_CALLBACK(int)
et_HandleEvent_TweakLayer(MozillaEvent_TweakLayer* e)
{
    CL_Layer *layer;
    Bool ret = TRUE;

    /* check that the doc_id is valid */
    if(XP_DOCID(e->ce.context) != e->ce.doc_id)
	return FALSE;

    switch(e->op) {
    case CL_SetSrcWidth:
        ret = LO_SetLayerSrc(e->ce.context, e->param_val, (char*)e->param_ptr,
			     e->referer, e->x);
        break;
    case CL_SetSrc:
        layer = LO_GetLayerFromId(e->ce.context, e->param_val);
        ret = LO_SetLayerSrc(e->ce.context, e->param_val, (char*)e->param_ptr, 
                             e->referer, LO_GetLayerWrapWidth(layer));
        break;
    case CL_SetBgColor:
        LO_SetLayerBgColor(e->layer, (LO_Color*)e->param_ptr);
        if (e->param_ptr)
            XP_FREE((void*)e->param_ptr);
        break;
    default:
        XP_ASSERT(0);
    }

    return (int)ret;
}

PR_STATIC_CALLBACK(void)
et_DestroyEvent_TweakLayer(MozillaEvent_TweakLayer * event)
{
    XP_FREEIF(event->referer);
    XP_FREE(event);
}

/*
 * These need to be synchronous so that if we set this and then
 *    immediately look at it we get the correct (new) value
 */
int
ET_TweakLayer(MWContext * context, CL_Layer* layer, int32 x, int32 y,
              void *param_ptr, int32 param_val, ETLayerOp op,
	      const char *referer, int32 doc_id)
{
    MozillaEvent_TweakLayer * event;
    event = PR_NEW(MozillaEvent_TweakLayer);
    if (event == NULL) 
        return NULL;

    PR_InitEvent(&event->ce.event, context,
		 (PRHandleEventProc)et_HandleEvent_TweakLayer,
		 (PRDestroyEventProc)et_DestroyEvent_TweakLayer);
    event->ce.context = context;
    event->ce.doc_id = doc_id;
    event->layer = layer;
    event->x = x;
    event->y = y;
    event->param_ptr = param_ptr;
    event->param_val = param_val;
    event->op = op;
    event->referer = referer ? XP_STRDUP(referer) : NULL;

    return (int)et_PostEvent(&event->ce, TRUE);
}

/****************************************************************************/

typedef struct {
    ETEvent	ce;
    XP_Bool	refreshInstances;
} MozillaEvent_RefreshPlugins; 

PR_STATIC_CALLBACK(void*)
et_HandleEvent_RefreshPlugins(MozillaEvent_RefreshPlugins* e)
{
    return ((void*) NPL_RefreshPluginList(e->refreshInstances));
}

int32
ET_npl_RefreshPluginList(MWContext* context, XP_Bool refreshInstances)
{
    int32 ret = -1;

    MozillaEvent_RefreshPlugins* event = XP_NEW_ZAP(MozillaEvent_RefreshPlugins);
    if (event == NULL) 
        return ret;

    PR_InitEvent(&event->ce.event, context,
		 (PRHandleEventProc)et_HandleEvent_RefreshPlugins,
		 (PRDestroyEventProc)et_DestroyEvent_GenericEvent);
    event->ce.context = context;
    event->refreshInstances = refreshInstances;

    ret = (int32) et_PostEvent(&event->ce, TRUE);
    return ret;
}

/****************************************************************************/

typedef struct {
    ETEvent	      ce;
    JSContext	    * cx;
    uint	      argc;
    jsval	    * argv;
    jsval	    * rval;
    char            * string;
} MozillaEvent_HandlePref; 

PR_STATIC_CALLBACK(JSBool)
et_HandleEvent_HandlePref(MozillaEvent_HandlePref* e)
{
    JSString	* str;
    char	* cstr;
    JSContext	* mochaContext=NULL;
    JSObject	* mochaPrefObject=NULL;

    str = JS_ValueToString(e->cx, e->argv[0]);
    if (!str)
	return JS_TRUE;

    cstr = JS_GetStringBytes(str);

    PREF_GetConfigContext(&mochaContext);
    if (mochaContext == NULL)	{
        return JS_FALSE;
    }
    PREF_GetGlobalConfigObject(&mochaPrefObject);
    if (mochaPrefObject == NULL)	{
        return JS_FALSE;
    }

    switch(e->argc) {

    case 1:
	{
		int iType = PREF_GetPrefType((const char *)cstr);
		int32 iRet;
		XP_Bool bRet;
		char * pRet;

		switch (iType) {
			/* these cases should be defines but...*/
			case 8:
				/* pref is a string*/
				PREF_CopyCharPref((const char *)cstr,&pRet);
				*e->rval = STRING_TO_JSVAL(pRet);
				e->string = pRet;
				break;
			case 16:
				/* pref is a int*/				
				PREF_GetIntPref((const char *)cstr,&iRet);
				*e->rval = INT_TO_JSVAL(iRet);
				break;
			case 32:
				/* pref is a bool*/
				PREF_GetBoolPref((const char *)cstr,&bRet);
				*e->rval = BOOLEAN_TO_JSVAL(bRet);
				break;
		}
	}
	break;
    case 2:
	{
	    if (JSVAL_IS_STRING(e->argv[1]))	{
		JSString * valueJSStr = JS_ValueToString(e->cx, e->argv[1]);
		if (valueJSStr)	{
		    char * valueStr = JS_GetStringBytes(valueJSStr);
		    if (valueStr)
			PREF_SetCharPref(cstr, valueStr);
		}
	    }
	    else if (JSVAL_IS_INT(e->argv[1]))	{
		jsint intVal = JSVAL_TO_INT(e->argv[1]);
		PREF_SetIntPref(cstr, (int32)intVal);
	    }
	    else if (JSVAL_IS_BOOLEAN(e->argv[1]))	{
		JSBool boolVal = JSVAL_TO_BOOLEAN(e->argv[1]);
		PREF_SetBoolPref(cstr, (XP_Bool) boolVal);
	    }
	    else if (JSVAL_IS_NULL(e->argv[1]))	{
	    	PREF_DeleteBranch(cstr);
	    }
        }
	break;
    }

    return JS_TRUE;

}

JSBool
ET_HandlePref(JSContext * cx, uint argc, jsval * argv, jsval * rval)
{

    JSBool ret;

    MozillaEvent_HandlePref* event = XP_NEW_ZAP(MozillaEvent_HandlePref);
    if (event == NULL) 
        return JS_TRUE;

    PR_InitEvent(&event->ce.event, NULL,
		 (PRHandleEventProc)et_HandleEvent_HandlePref,
		 (PRDestroyEventProc)et_DestroyEvent_WaitForIt);
    event->ce.context = NULL;
    event->cx = cx;
    event->argc = argc;
    event->argv = argv;
    event->rval = rval;

    ret = (JSBool) et_PostEvent(&event->ce, TRUE);

    /* if it was a string we need to convert the string to a JSString */
    /* do we need to free event->string or does JS own it now ? */
    if (ret == JS_TRUE && JSVAL_IS_STRING(*event->rval))
	*rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, event->string)); 

    XP_FREE(event);
    return ret;

}

/****************************************************************************/

typedef struct {
    ETEvent		    ce;
    ETVerifyComponentFunc   fn;
    ETBoolPtrFunc	   *pActive_callback;
    ETVoidPtrFunc	   *pStartup_callback;
} MozillaEvent_VerifyComponentFunc;

PR_STATIC_CALLBACK(PRBool)
et_HandleEvent_VerifyComponentFunction(MozillaEvent_VerifyComponentFunc* e)
{
   
    return (PRBool)(e->fn) ((void**)(e->pActive_callback), 
			    (void**)(e->pStartup_callback));
}

PRBool
ET_moz_VerifyComponentFunction(ETVerifyComponentFunc fn, ETBoolPtrFunc *pActive_callback, 
			       ETVoidPtrFunc *pStartup_callback)
{
    PRBool ret;
    
    MozillaEvent_VerifyComponentFunc* event = PR_NEW(MozillaEvent_VerifyComponentFunc);
    if (event == NULL) 
	return PR_FALSE;

    PR_InitEvent(&event->ce.event, NULL,
		 (PRHandleEventProc)et_HandleEvent_VerifyComponentFunction,
		 (PRDestroyEventProc)et_DestroyEvent_GenericEvent);
    event->fn = fn;
    /* Pointers must be malloc'd inside libmocha before this call */
    event->pActive_callback = pActive_callback;
    event->pStartup_callback = pStartup_callback;

    ret = (PRBool) et_PostEvent(&event->ce, TRUE);
    return ret;
}

/****************************************************************************/

typedef struct {
    ETEvent              ce;
    ETCompPropGetterFunc fn;
    char		*name;
} MozillaEvent_GetterFunc;

PR_STATIC_CALLBACK(void *)
et_HandleEvent_CompGetter(MozillaEvent_GetterFunc* e)
{
    return (void *)(e->fn)(e->name);
}

PR_STATIC_CALLBACK(void)
et_DestroyEvent_CompGetter(MozillaEvent_GetterFunc * event)
{
    if (event->name)
	XP_FREE(event->name);
    XP_FREE(event);
}

void *
ET_moz_CompGetterFunction(ETCompPropGetterFunc fn, char *name)
{
    MozillaEvent_GetterFunc* event = PR_NEW(MozillaEvent_GetterFunc);
    if (event == NULL) 
	return NULL;

    PR_InitEvent(&event->ce.event, NULL,
		 (PRHandleEventProc)et_HandleEvent_CompGetter,
		 (PRDestroyEventProc)et_DestroyEvent_CompGetter);
    event->fn = fn;
    event->name = XP_STRDUP(name);

    return (void *)et_PostEvent(&event->ce, TRUE);
}

/****************************************************************************/

typedef struct {
    ETEvent              ce;
    ETCompPropSetterFunc fn;
    char		*name;
    void		*data;
} MozillaEvent_SetterFunc;

PR_STATIC_CALLBACK(void)
et_HandleEvent_CompSetter(MozillaEvent_SetterFunc* e)
{
    (e->fn)(e->name, e->data);
}

PR_STATIC_CALLBACK(void)
et_DestroyEvent_CompSetter(MozillaEvent_SetterFunc * event)
{
    if (event->name)
	XP_FREE(event->name);
    XP_FREE(event);
}

void
ET_moz_CompSetterFunction(ETCompPropSetterFunc fn, char *name, void *data)
{
    MozillaEvent_SetterFunc* event = PR_NEW(MozillaEvent_SetterFunc);
    if (!event || !name) 
	return;

    PR_InitEvent(&event->ce.event, NULL,
		 (PRHandleEventProc)et_HandleEvent_CompSetter,
		 (PRDestroyEventProc)et_DestroyEvent_CompSetter);
    event->fn = fn;
    event->name = XP_STRDUP(name);
    event->data = data;

    (void) et_PostEvent(&event->ce, FALSE);
}

/****************************************************************************/

typedef struct {
    ETEvent              ce;
    ETCompMethodFunc	 fn;
    int32		 argc;
    JSCompArg		*argv;
} MozillaEvent_MethodFunc;

PR_STATIC_CALLBACK(void *)
et_HandleEvent_CompMethod(MozillaEvent_MethodFunc* e)
{
    return (void *)(e->fn)(e->argc, e->argv);
}

PR_STATIC_CALLBACK(void)
et_DestroyEvent_CompMethod(MozillaEvent_MethodFunc * event)
{
    if (event->argv)
	XP_FREE(event->argv);
    XP_FREE(event);
}

void *
ET_moz_CompMethodFunction(ETCompMethodFunc fn, int32 argc, JSCompArg *argv)
{
    MozillaEvent_MethodFunc* event = PR_NEW(MozillaEvent_MethodFunc);
    if (!event || !argv) 
	return NULL;

    PR_InitEvent(&event->ce.event, NULL,
		 (PRHandleEventProc)et_HandleEvent_CompMethod,
		 (PRDestroyEventProc)et_DestroyEvent_CompMethod);
    event->fn = fn;
    event->argc = argc;
    event->argv = XP_ALLOC(argc * sizeof(JSCompArg));
    if (event->argv)
	XP_MEMCPY(event->argv, argv, (argc * sizeof(JSCompArg)));

    return (void *)et_PostEvent(&event->ce, TRUE);
}

/****************************************************************************/

typedef struct {
    ETEvent ce;
    char* prin;
    char* target;
    char* risk;
    int isCert;
} MozillaEvent_signedAppletPrivileges;

PR_STATIC_CALLBACK(void)
et_HandleEvent_signedAppletPrivileges(MozillaEvent_signedAppletPrivileges* e)
{
    SECNAV_signedAppletPrivilegesOnMozillaThread
	(e->ce.context, e->prin, e->target, e->risk, e->isCert);
}

PR_STATIC_CALLBACK(void)
et_DestroyEvent_signedAppletPrivileges(MozillaEvent_signedAppletPrivileges* e)
{
    XP_FREE((char*)e->prin);
    XP_FREE((char*)e->target);
    XP_FREE((char*)e->risk);
    XP_FREE(e);
}

void
ET_PostSignedAppletPrivileges
    (MWContext* context, char* prin, char* target, char* risk, int isCert)
{
    MozillaEvent_signedAppletPrivileges* event =
	PR_NEW(MozillaEvent_signedAppletPrivileges);
    if (event == NULL)
	return;
    PR_InitEvent(&event->ce.event, context,
		 (PRHandleEventProc)et_HandleEvent_signedAppletPrivileges,
		 (PRDestroyEventProc)et_DestroyEvent_signedAppletPrivileges);
    event->ce.context = context;
    event->prin = strdup(prin);
    event->target = strdup(target);
    event->risk = strdup(risk);
    event->isCert = isCert;
    et_PostEvent(&event->ce, FALSE);
}

#ifdef DOM

/****************************************************************************/

typedef struct {
    ETEvent	    ce;
    lo_NameList *name_rec;
	ETSpanOp	op;
	void *param_ptr;
	int32 param_val;
} MozillaEvent_TweakSpan; 

PR_STATIC_CALLBACK(int)
et_HandleEvent_TweakSpan(MozillaEvent_TweakSpan* e)
{
    Bool ret = TRUE;

    /* check that the doc_id is valid */
    if(XP_DOCID(e->ce.context) != e->ce.doc_id)
	return FALSE;

    switch(e->op) {
    case SP_SetColor:
        LO_SetSpanColor(e->ce.context, e->name_rec, (LO_Color*)e->param_ptr);
		/* This call to reflow is just a temp hack */
		LO_RelayoutFromElement(e->ce.context, e->name_rec->element);
        if (e->param_ptr)
            XP_FREE((void*)e->param_ptr);
        break;
    case SP_SetBackground:
        LO_SetSpanBackground(e->ce.context, e->name_rec, (LO_Color*)e->param_ptr);
		/* This call to reflow is just a temp hack */
		LO_RelayoutFromElement(e->ce.context, e->name_rec->element);
        if (e->param_ptr)
            XP_FREE((void*)e->param_ptr);
        break;
    case SP_SetFontFamily:
      LO_SetSpanFontFamily(e->ce.context, e->name_rec, (char*)e->param_ptr);
      LO_RelayoutFromElement(e->ce.context, e->name_rec->element);
      break;
    case SP_SetFontWeight:
      LO_SetSpanFontWeight(e->ce.context, e->name_rec, (char*)e->param_ptr);
      LO_RelayoutFromElement(e->ce.context, e->name_rec->element);
      break;
    case SP_SetFontSize:
      LO_SetSpanFontSize(e->ce.context, e->name_rec, e->param_val);
      LO_RelayoutFromElement(e->ce.context, e->name_rec->element);
      break;
    case SP_SetFontSlant:
      LO_SetSpanFontSlant(e->ce.context, e->name_rec, (char*)e->param_ptr);
      LO_RelayoutFromElement(e->ce.context, e->name_rec->element);
      break;
    default:
        XP_ASSERT(0);
    }

    return (int)ret;
}

PR_STATIC_CALLBACK(void)
et_DestroyEvent_TweakSpan(MozillaEvent_TweakSpan * event)
{
    XP_FREE(event);
}

/*
 * These need to be synchronous so that if we set this and then
 *    immediately look at it we get the correct (new) value
 */
int
ET_TweakSpan(MWContext * context, void *name_rec, void *param_ptr,
	int32 param_val, ETSpanOp op, int32 doc_id)
{
    MozillaEvent_TweakSpan * event;
    event = PR_NEW(MozillaEvent_TweakSpan);
    if (event == NULL) 
        return NULL;

    PR_InitEvent(&event->ce.event, context,
		 (PRHandleEventProc)et_HandleEvent_TweakSpan,
		 (PRDestroyEventProc)et_DestroyEvent_TweakSpan);
    event->ce.context = context;
    event->ce.doc_id = doc_id;
    event->op = op;
    event->name_rec = name_rec;
	event->param_ptr = param_ptr;
    event->param_val = param_val;

    return (int)et_PostEvent(&event->ce, TRUE);
}

/* Tweak XML Transclusion stuff starts here */
typedef struct {
    ETEvent				ce;
	ETTransclusionOp	op;
	void *param_ptr;
	int32 param_val;
	void *xmlFile;
} MozillaEvent_TweakTransclusion; 

PR_STATIC_CALLBACK(int)
et_HandleEvent_TweakTransclusion(MozillaEvent_TweakTransclusion* e)
{
    Bool ret = TRUE;

    /* check that the doc_id is valid */
    if(XP_DOCID(e->ce.context) != e->ce.doc_id)
	return FALSE;

    switch(e->op) {
    case TR_SetHref:
        XMLSetTransclusionProperty(e->xmlFile, e->param_val, "href", e->param_ptr);
        if (e->param_ptr)
            XP_FREE((void*)e->param_ptr);
        break;
    default:
        XP_ASSERT(0);
    }

    return (int)ret;
}

PR_STATIC_CALLBACK(void)
et_DestroyEvent_TweakTransclusion(MozillaEvent_TweakTransclusion * event)
{
    XP_FREE(event);
}


int
ET_TweakTransclusion(MWContext * context, void *xmlFile, void *param_ptr,
	int32 param_val, ETTransclusionOp op, int32 doc_id)
{
    MozillaEvent_TweakTransclusion * event;
    event = PR_NEW(MozillaEvent_TweakTransclusion);
    if (event == NULL) 
        return NULL;

    PR_InitEvent(&event->ce.event, context,
		 (PRHandleEventProc)et_HandleEvent_TweakTransclusion,
		 (PRDestroyEventProc)et_DestroyEvent_TweakTransclusion);
    event->ce.context = context;
    event->ce.doc_id = doc_id;
    event->op = op;
	event->param_ptr = param_ptr;
    event->param_val = param_val;
	event->xmlFile = xmlFile;

    return (int)et_PostEvent(&event->ce, FALSE);
}

#endif
