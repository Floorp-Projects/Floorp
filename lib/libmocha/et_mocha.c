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
 * Messages passing from the mozilla thread to the mocha thread
 */
#include "lm.h"
#include "xp.h"
#include "fe_proto.h"
#include "proto.h"
#include "net.h"
#include "structs.h"
#include "prmon.h"
#include "prthread.h"
#ifdef XP_MAC
#include "prpriv.h"
#else
#include "private/prpriv.h" /* For PR_NewNamedMonitor */
#endif
#include "layout.h"	/* XXX for lo_ContextToCell and lo_FormData */
#include "libimg.h"
#include "java.h"
#include "pa_tags.h"
#include "css.h"
#include "pa_parse.h"
#include "jsjava.h"
#include "intl_csi.h"
#include "netcache.h"

QueueStackElement * et_TopQueue = NULL;

PRIVATE PRMonitor * lm_queue_monitor = NULL;
PRIVATE JSBool lm_InterruptCurrentOp = JS_FALSE;

#ifdef XP_WIN16
#define MOCHA_NORMAL_PRIORITY PR_PRIORITY_NORMAL
extern PRThread     *lm_InterpretThread;
#endif

/**********************************************************************/


#define MAKE_EAGER_INHERIT(e)						     \
                      if (et_TopQueue->inherit_parent &&		     \
			  e->ce.handle_eagerly == JS_FALSE &&		     \
			  e->ce.context->grid_parent) {			     \
			  e->ce.handle_eagerly = 			     \
			    (XP_DOCID(e->ce.context->grid_parent) ==         \
			     et_TopQueue->doc_id);			     \
		      }

#define MAKE_EAGER(e) if (et_TopQueue->doc_id == 0 && 			     \
			  et_TopQueue->context == e->ce.context) {	     \
			  et_TopQueue->doc_id = XP_DOCID(e->ce.context);     \
			  e->ce.handle_eagerly = JS_TRUE;		     \
		      } else {						     \
			  e->ce.handle_eagerly = 			     \
			    (XP_DOCID(e->ce.context) == et_TopQueue->doc_id);\
		      }							     \
                      MAKE_EAGER_INHERIT(e)
			  
/**********************************************************************/

static void
et_event_to_mocha(ETEvent * e)
{
    JSBool canDoJS;
    
    if(e->context) {
	e->doc_id = XP_DOCID(e->context);
	canDoJS = LM_CanDoJS(e->context);
    } else {
	/* source of event must be timeout or someone else who can do mocha*/
	canDoJS = JS_TRUE;
    }
    
    if (!lm_InterpretQueue || !canDoJS) {
	e->event.destructor((PREvent *) e);
	return;
    }

    /*
     * Decide which queue to put this event on.  The et_TopQueue may
     *   actually be the same as lm_InterpretQueue
     */
    if (e->handle_eagerly) 
	PR_PostEvent(et_TopQueue->queue, &e->event);
    else
	PR_PostEvent(lm_InterpretQueue, &e->event);

#ifdef XP_WIN16
    /* Raise the mocha thread priority, otherwise mocha may not get 
     *   time slices, because user's program java threads have higher
     *   priority ( 15 ). If mocha is starving events are not transfered 
     *   to the mozilla-event-queue and Navigator stops reacting to 
     *   the input events
     */
    PR_SetThreadPriority ( lm_InterpretThread, PR_PRIORITY_URGENT );
#endif

    if(lm_queue_monitor) {
        /* wake up the processing routine */
        PR_EnterMonitor(lm_queue_monitor);
        PR_Notify(lm_queue_monitor);
        PR_ExitMonitor(lm_queue_monitor);
    }

}

/**********************************************************************/

/* By wrapping the following macros around an event handler callback,
   we can verify that the MWContext to which the event was directed
   hasn't gone away. As a side effect, the local variable 'decoder'
   is established within the macros:

   void foo_handler(JSEvent* e) {
     ET_BEGIN_EVENT_HANDLER(e);
     ... decoder ...
     ET_END_EVENT_HANDLER(e);
   }

 */
#define ET_BEGIN_EVENT_HANDLER(jsevent)				  \
{								  \
    MWContext* _c = (jsevent)->ce.context;			  \
    MochaDecoder* decoder;					  \
    if (_c == NULL) {						  \
	goto _quit;						  \
    }								  \
    decoder = LM_GetMochaDecoder(_c);				  \
    if (decoder == NULL) {					  \
	goto _quit;						  \
    }								  \

#define ET_END_EVENT_HANDLER(e)					  \
  _quit:							  \
    if (decoder != NULL) {					  \
	LM_PutMochaDecoder(decoder);				  \
    }								  \
}								  \

/**********************************************************************/

/*
 * Trash our event when we get done with it
 */
PR_STATIC_CALLBACK(void)
et_event_destructor(JSEvent * pEvent)
{
    if (!pEvent->saved)
	XP_FREE(pEvent);
}

/*
 * Actual handler routine
 */
PR_STATIC_CALLBACK(void)
et_event_handler(JSEvent * e)
{

    LO_Element	    * lo_element = NULL;
    ETEventStatus     status = EVENT_OK;
    jsval        rval;

    ET_BEGIN_EVENT_HANDLER(e);

    LO_LockLayout();

    /* verify that this document is still valid */
    if(e->ce.doc_id != XP_DOCID(e->ce.context) && e->type != EVENT_UNLOAD) {
        status = EVENT_PANIC;
	LO_UnlockLayout();
        goto done;
    }

    decoder->doc_id = e->ce.doc_id;

    /* find the element that caused this event */
    lo_element = e->lo_element;

    /* figure out who to call */
    switch(e->type) {

    case EVENT_SCROLL:
    case EVENT_DRAGDROP:
    case EVENT_MOVE:
    case EVENT_RESIZE:
    case EVENT_HELP:
	 /* These are all window level events only, TRUE unless explicitly denied */
   	LO_UnlockLayout();
	    
	if (!(decoder->event_mask & e->type)) {
	    decoder->event_mask |= e->type;
	    if (lm_SendEvent(e->ce.context, decoder->window_object, e, &rval) &&
		rval == JSVAL_FALSE) {
		status = EVENT_CANCEL;
 	    }
	    decoder->event_mask &= ~e->type;
	}
 	break;

    /*BEGIN Events occurring in the following cases must LO_UnlockLayout themselves. */
    case EVENT_CLICK:
    case EVENT_MOUSEDOWN:
    case EVENT_MOUSEUP:
    case EVENT_DBLCLICK:
        /* TRUE unless explicitly denied */
	if (lm_MouseInputEvent(e->ce.context, lo_element, e, &rval) &&
	    rval == JSVAL_FALSE) {
            status = EVENT_CANCEL;
        }

        /* 
         * If this was the submit button we want to send a submit 
         *   event here
         */
        break;    
    case EVENT_KEYDOWN:
    case EVENT_KEYUP:
    case EVENT_KEYPRESS:
        /* TRUE unless explicitly denied */
        if (lm_KeyInputEvent(e->ce.context, lo_element, e, &rval) &&
	    rval == JSVAL_FALSE) {
            status = EVENT_CANCEL;
        }
        break;    
    case EVENT_BLUR:
    case EVENT_FOCUS:
    case EVENT_SELECT:
    case EVENT_MOUSEOUT:
    case EVENT_CHANGE:
        /* TRUE unless explicitly denied */
        if (lm_InputEvent(e->ce.context, lo_element, e, &rval) &&
	    rval == JSVAL_FALSE) {
            status = EVENT_CANCEL;
        }
        break;    
    case EVENT_MOUSEMOVE:
        /* There are two reasons this event could be here
	 * 1. The script is capturing this event.
	 * 2. The script has a handler defined for onmousemove and we
	 *    are assuming they intend the capure this event.  Since the 
	 *    time it takes to handle a mousedown event and start capturing
	 *    would cause lossage of mousemove messages we're sending all
	 *    mousemoves between mousedowns and ups so that if the scripts
	 *    starts capturing during the onmousedown handler, the mousemoves
	 *    we would have lost will be sitting on the JS queue.
	/* TRUE unless explicitly denied */
	if (LM_EventCaptureCheck(e->ce.context, EVENT_MOUSEMOVE)) {
	    if (lm_InputEvent(e->ce.context, lo_element, e, &rval) &&
		rval == JSVAL_FALSE) {
		status = EVENT_CANCEL;
	    }
	}
	else
	    LO_UnlockLayout();
        break;    
    case EVENT_MOUSEOVER:
        /* FALSE unless explicitly set */
        status = EVENT_CANCEL;
        if (lm_InputEvent(e->ce.context, lo_element, e, &rval) &&
	    rval == JSVAL_TRUE) {
            status = EVENT_OK;
        }
        break;    
   
    case EVENT_SUBMIT:
	if(lm_SendOnSubmit(e->ce.context, e,  lo_element))
	    status = EVENT_OK;
	else
	    status = EVENT_CANCEL;
	/*LO_UnlockLayout called in form_event;*/
	break;


    case EVENT_RESET:
	if(lm_SendOnReset(e->ce.context, e, lo_element))
	    status = EVENT_OK;
	else
	    status = EVENT_CANCEL;
	/*LO_UnlockLayout called in form_event;*/
	break;
   /*END asymmetric layout unlocking.*/

    default:
	LO_UnlockLayout();
	XP_TRACE(("Mocha thread: Got event %d that I didn't expect", e->type));
        break;
    }
  
    /* clear the mask */

    /* check again to make sure the document hasn't changed */
    if(e->ce.doc_id != XP_DOCID(e->ce.context))
        status = EVENT_PANIC;

done:
    /* Post an event to call the front-end closuer */
    if(e->fnClosure)
        ET_PostJsEventAck(e->ce.context, lo_element,
                          e->type, e->fnClosure,
                          e->whatever, status);

#ifdef XPWIN16
    /* Return to the normal mocha thread priority, after mocha thread
     *   has transfered event to the mozilla-event-queue ( if necessary, 
     *   i.e. if e->fnClosure != NULL 
     */
    PR_SetThreadPriority ( lm_InterpretThread, MOCHA_NORMAL_PRIORITY );
#endif

    ET_END_EVENT_HANDLER(e);
}



/*
 * Tell the backend about a new event.
 * fnClosure should be allowed to be NULL
 */
JSBool
ET_SendEvent(MWContext * pContext, LO_Element *pElement, JSEvent *pEvent, 
             ETClosureFunc fnClosure, void * whatever) 
{
    /* make sure we are able to process Mocha events before bothering */
    if (!LM_CanDoJS(pContext) || EDT_IS_EDITOR(pContext)) {
	ETEventStatus     status = EVENT_OK;
	if (pEvent->type == EVENT_MOUSEOVER)
	    status = EVENT_CANCEL;
	if (fnClosure)
	    fnClosure(pContext, pElement, pEvent->type, whatever, status);
	XP_FREE(pEvent);
	return(JS_TRUE);
    }

    PR_InitEvent(&pEvent->ce.event, pContext,
                 (PRHandleEventProc)et_event_handler, 
                 (PRDestroyEventProc)et_event_destructor);

    pEvent->ce.context = pContext;
    MAKE_EAGER_INHERIT(pEvent);
    if(pElement)
	pEvent->id = pElement->lo_any.ele_id;
    pEvent->lo_element = pElement;
    pEvent->fnClosure = fnClosure;
    pEvent->whatever = whatever;

    et_event_to_mocha(&pEvent->ce);

    return(JS_TRUE);

}

/**********************************************************************/

typedef struct {
    ETEvent	        ce;
    int32		type;
    ETVoidPtrFunc	fnClosure;
    void	      * data;
    int32               layer_id;
    JSBool              resize_reload;
} LoadEvent;

PR_STATIC_CALLBACK(void)
et_load_event_handler(LoadEvent * e)
{

    ETEventStatus     status = EVENT_OK;

    ET_BEGIN_EVENT_HANDLER(e);

    LO_LockLayout();

    /* verify that this document is still valid */
    if(e->ce.doc_id != XP_DOCID(e->ce.context) && e->type != EVENT_UNLOAD) {
        status = EVENT_PANIC;
	LO_UnlockLayout();
        goto done;
    }

    LO_UnlockLayout();

    decoder->doc_id = e->ce.doc_id;

    /* figure out who to call */
    if (e->layer_id == LO_DOCUMENT_LAYER_ID) {
        if (e->type == EVENT_LOAD)
            lm_ClearDecoderStream(decoder, JS_TRUE);

        lm_SendLoadEvent(e->ce.context, e->type, e->resize_reload);
    }
    else {
        if ((decoder->stream_owner == e->layer_id) && (e->type == EVENT_LOAD))
            lm_ClearDecoderStream(decoder, JS_TRUE);
            
        /* 
         * If the load event has already been sent, this is a layer whose
         * src has been dynamically changed. In that case, we want to fire
         * a load event irrespective of whether this context had been
         * resized.
         */
        lm_SendLayerLoadEvent(e->ce.context, e->type, e->layer_id, 
                     decoder->load_event_sent ? JS_FALSE : e->resize_reload);
    }

done:
    /* Post an event to call the front-end closure */
    if(e->fnClosure)
        ET_moz_CallFunctionAsync(e->fnClosure, e->data);

    ET_END_EVENT_HANDLER(e);
}

PR_STATIC_CALLBACK(void)
et_generic_destructor(void * event)
{
    XP_FREE(event);
}

/*
 * Tell the backend about a new load event.
 */
void
ET_SendLoadEvent(MWContext * pContext, int32 type, ETVoidPtrFunc fnClosure,
		 NET_StreamClass *stream, int32 layer_id, Bool resize_reload)
{

    LoadEvent * pEvent;

    /* 
     * Make sure we are allowed to do mocha stuff in this context
     *   before bothering to send the event
     */
    if (!LM_CanDoJS(pContext)) {
	if(fnClosure)
	    fnClosure(stream);
	return;
    }

    pEvent = (LoadEvent *) XP_NEW_ZAP(LoadEvent);
    if(!pEvent)
        return;

    PR_InitEvent(&pEvent->ce.event, pContext,
                 (PRHandleEventProc)et_load_event_handler, 
                 (PRDestroyEventProc)et_generic_destructor);

    pEvent->type = type;
    pEvent->ce.context = pContext;
    MAKE_EAGER(pEvent);
    pEvent->layer_id = layer_id;
    pEvent->fnClosure = fnClosure;
    pEvent->data = stream;
    pEvent->resize_reload = (JSBool)resize_reload;

    /* add the event to the event queue */
    et_event_to_mocha(&pEvent->ce);

}

/**********************************************************************/

PR_STATIC_CALLBACK(void)
et_setactiveform_handler(JSEvent * e)
{
    ET_BEGIN_EVENT_HANDLER(e);

    /* verify that this document is still valid */
    if(e->ce.doc_id != XP_DOCID(e->ce.context))
	return;

    lm_SetActiveForm(e->ce.context, e->type);

    ET_END_EVENT_HANDLER(e);
}

/*
 */
void
ET_SetActiveForm(MWContext * pContext, lo_FormData * form)
{

    JSEvent      * pEvent = (JSEvent *) XP_NEW_ZAP(JSEvent);
    if(!pEvent)
        return;

    PR_InitEvent(&pEvent->ce.event, pContext,
                 (PRHandleEventProc)et_setactiveform_handler, 
                 (PRDestroyEventProc)et_generic_destructor);

    pEvent->ce.context = pContext;
    MAKE_EAGER(pEvent);

    /* form can be NULL when there should be no active form */
    if(form)
	pEvent->type = form->id;
    else
	pEvent->type = 0;

    et_event_to_mocha(&pEvent->ce);

}



/**********************************************************************/

PR_STATIC_CALLBACK(void)
et_setactivelayer_handler(JSEvent * e)
{
    ET_BEGIN_EVENT_HANDLER(e);

    /* verify that this document is still valid */
    if(e->ce.doc_id != XP_DOCID(e->ce.context))
	return;

    LM_SetActiveLayer(e->ce.context, e->layer_id);

    ET_END_EVENT_HANDLER(e);
}

/*
 */
void
ET_SetActiveLayer(MWContext * pContext, int32 layer_id)
{

    JSEvent      * pEvent = (JSEvent *) XP_NEW_ZAP(JSEvent);
    if(!pEvent)
        return;

    PR_InitEvent(&pEvent->ce.event, pContext,
                 (PRHandleEventProc)et_setactivelayer_handler, 
                 (PRDestroyEventProc)et_generic_destructor);

    pEvent->ce.context = pContext;
    pEvent->layer_id = layer_id;
    et_event_to_mocha(&pEvent->ce);
}

/**********************************************************************/

/* 
 * Mocha is about to process or is processing an event for the given
 *   context.  Verify we haven't been asked to interrupt it
 */
JSBool
ET_ContinueProcessing(MWContext * context)
{
    return (JSBool)(lm_InterruptCurrentOp == JS_FALSE);
}

/**********************************************************************/

static void
et_RevokeEvents(MWContext * pContext)
{
    QueueStackElement *qse;

    for (qse = et_TopQueue; qse; qse = qse->down) {
        PR_RevokeEvents(qse->queue, pContext);
    }
    for (qse = et_TopQueue->up; qse; qse = qse->up) {
        PR_RevokeEvents(qse->queue, pContext);
    }
}

void
ET_InterruptContext(MWContext * pContext)
{

    /* make sure the context can do mocha before bothering */
    if (!lm_queue_monitor || !LM_CanDoJS(pContext))
	return;

    /* need to lock the JS-thread from starting new events */
    PR_EnterMonitor(lm_queue_monitor);

    /* Is our context currently running in mocha ? */
    if (LM_JSLockGetContext() == pContext) {
	/* 
	 * if the owner of the JSLock is the context we are
	 *   interrupting set a flag so it will stop soon
	 */
	lm_InterruptCurrentOp = JS_TRUE;

    }

    /* clear events for this context off of the interpret queue */
    et_RevokeEvents(pContext);

    /* need to unlock the JS-thread from starting new events */
    PR_ExitMonitor(lm_queue_monitor);
 
    /* Interrupt the JS image context. */
    if (pContext->mocha_context)
	ET_InterruptImgCX(pContext);
}

/**********************************************************************/

/*
 * Actual handler routine for image events
 */
PR_STATIC_CALLBACK(void)
et_image_event_handler(JSEvent * e)
{

    ETEventStatus     status = EVENT_OK;
    LO_ImageStruct  * image;
    JSObject	    * obj;

    ET_BEGIN_EVENT_HANDLER(e);

    LO_LockLayout();

    /* verify that this document is still valid */
    if(e->ce.doc_id != XP_DOCID(e->ce.context)) {
        status = EVENT_PANIC;
	LO_UnlockLayout();
        goto done;
    }

    /* 
     * Remember the ID of the document that spaned this call stack
     */
    if(decoder) 
	decoder->doc_id = e->ce.doc_id;

    /* XXX chouck - do we need to set a mask so we don't loop infinitely? */

    /* find the element that caused this event */
    if(e->id)
	image = LO_GetImageByIndex(e->ce.context, e->layer_id, e->id);
    else
	image = (LO_ImageStruct *) e->lo_element;

    if (!image || (image->type != LO_IMAGE)) {
	LO_UnlockLayout();
	goto done;
    }

    obj = image->mocha_object;

    /* OK, we've gotten our pointer, let layout be happy again */
    LO_UnlockLayout();

    /* If we actually had an object send the event for it */
    if(obj) 
	lm_ProcessImageEvent(e->ce.context, obj, (LM_ImageEvent) e->type);

    /* clear the mask */

    /* check again to make sure the document hasn't changed */
    if(e->ce.doc_id != XP_DOCID(e->ce.context))
        status = EVENT_PANIC;

done:
    ET_END_EVENT_HANDLER(e);

    return;
}


void
ET_SendImageEvent(MWContext * pContext, LO_ImageStruct *image_data,
                  LM_ImageEvent event)
{
    JSEvent      * pEvent;

    if (!LM_CanDoJS(pContext))
	return;
    
    pEvent = (JSEvent *) XP_NEW_ZAP(JSEvent);
    if(!pEvent)
        return;

    PR_InitEvent(&pEvent->ce.event, pContext,
		 (PRHandleEventProc)et_image_event_handler, 
		 (PRDestroyEventProc)et_event_destructor);

    pEvent->type = event;
    pEvent->ce.context = pContext;

    if(image_data) {
	pEvent->id = image_data->seq_num;
        pEvent->layer_id = image_data->layer_id;
    }
    pEvent->lo_element = (LO_Element *) image_data;

    et_event_to_mocha(&pEvent->ce);

}

/**********************************************************************/

typedef struct {
    ETEvent	        ce;
    int32               layer_id;
    void              * lo_ele;
    void              * pa_tag;
    ReflectedObject	type;
    uint		index;
} Reflect_Event;

/*
 * Make the appropriate LM_Reflect() call.  Since we store a pointer
 *   to the newly created JSObject as part of the layout object it
 *   represents, we need to lock layout while doing this so layout
 *   doesn't take our object away out from under us.
 * If any of the reflection routines take us back into the mozilla 
 *   thread we run the risk of deadlocking on the LO_LockLayout()
 *   call.
 */
PR_STATIC_CALLBACK(void)
et_reflect_handler(Reflect_Event * e)
{
    ETEventStatus     status = EVENT_OK;

    ET_BEGIN_EVENT_HANDLER(e);

    /*
     * Make sure the layout objects don't go away while we are 
     *   reflecting them
     */
    LO_LockLayout();

    /* verify that this document is still valid */
    if(e->ce.doc_id != XP_DOCID(e->ce.context)) {
        status = EVENT_PANIC;
        goto done;
    }

    switch(e->type) {
    case LM_APPLETS:
#ifdef JAVA
	LM_ReflectApplet(e->ce.context, (LO_JavaAppStruct *) e->lo_ele, 
	                 e->pa_tag, e->layer_id, e->index);
#endif
	    break;
    case LM_EMBEDS:
#ifdef JAVA
	LM_ReflectEmbed(e->ce.context, e->lo_ele, e->pa_tag, 
                        e->layer_id, e->index);
#endif
	break;
    case LM_IMAGES:
	LM_ReflectImage(e->ce.context, e->lo_ele, e->pa_tag, 
                        e->layer_id, e->index);
	break;
    case LM_LINKS:
	LM_ReflectLink(e->ce.context, (LO_AnchorData *) e->lo_ele, 
		       e->pa_tag, e->layer_id, e->index);
	    break;
    case LM_FORMS:
	LM_ReflectForm(e->ce.context, (lo_FormData *) e->lo_ele, 
		       e->pa_tag, e->layer_id, e->index);
	    break;
    case LM_NAMEDANCHORS:
        LM_ReflectNamedAnchor(e->ce.context, (lo_NameList *) e->lo_ele,
                              e->pa_tag, e->layer_id, e->index);
        break;
    case LM_FORMELEMENTS:
	XP_ASSERT(0);
	break;
    case LM_LAYERS:
        LM_ReflectLayer(e->ce.context, e->index, e->layer_id, e->pa_tag);
        break;
#ifdef DOM
	case LM_SPANS:
			LM_ReflectSpan(e->ce.context, e->lo_ele, e->pa_tag, 
				e->layer_id, e->index);
		break;
	case LM_TRANSCLUSIONS:
			LM_ReflectTransclusion(e->ce.context, e->lo_ele, e->layer_id, e->index);
		break;
#endif
    default:
	XP_ASSERT(0);
	break;
    }

done:

    /*
     * We are done with the reflection so unlock layout 
     */ 
    LO_UnlockLayout();

    ET_END_EVENT_HANDLER(e);

    return;
}

/*
 * Free our memory
 */
PR_STATIC_CALLBACK(void)
et_reflect_destructor(Reflect_Event * e)
{
    /* we explictly duplicated our tag when we created this event
     *   object so make sure to get rid of it now
     */
    if (e->pa_tag)
        PA_FreeTag(e->pa_tag);

    XP_FREE(e);
}

/*
 * Reflect the given object.
 */
void
ET_ReflectObject(MWContext * pContext, void * lo_ele, void * tag, 
                 int32 layer_id, uint index, ReflectedObject type)
{
    /* create our event object */
    Reflect_Event * pEvent = (Reflect_Event *) XP_NEW_ZAP(Reflect_Event);
    if(!pEvent)
        return;

#ifdef JAVA
    /* before we can call java on the mocha thread we need
     * to initialize moja.  this isn't safe to do with the
     * layout lock held, so we do it here. */
    if (type == LM_APPLETS || type == LM_EMBEDS)
        ET_InitMoja(pContext);
#endif

    /* do a PR_InitEvent on the event structure */
    PR_InitEvent(&pEvent->ce.event, pContext,
		 (PRHandleEventProc)et_reflect_handler, 
		 (PRDestroyEventProc)et_reflect_destructor);

    /* fill in the non-PR fields we care about */
    pEvent->type = type;
    pEvent->ce.context = pContext;
    pEvent->lo_ele = lo_ele;
    if(tag)
        pEvent->pa_tag = (void *) PA_CloneMDLTag(tag);
    else
        pEvent->pa_tag = NULL;

    pEvent->index = index;
    pEvent->layer_id = layer_id;
    MAKE_EAGER(pEvent);

    /* add the event to the event queue */
    et_event_to_mocha(&pEvent->ce);

}

/**********************************************************************/

typedef struct {
    ETEvent	        ce;
    int32               layer_id;
    void              * pa_tag;
    uint		element_index;
    uint		form_index;
} ReflectForm_Event;


/*
 * Make the appropriate LM_Reflect() call.  Since we store a pointer
 *   to the newly created JSObject as part of the layout object it
 *   represents, we need to lock layout while doing this so layout
 *   doesn't take our object away out from under us.
 * If any of the reflection routines take us back into the mozilla 
 *   thread we run the risk of deadlocking on the LO_LockLayout()
 *   call.
 */
PR_STATIC_CALLBACK(void)
et_reflectElement_handler(ReflectForm_Event * e)
{

    ET_BEGIN_EVENT_HANDLER(e);

    /*
     * Make sure the layout objects don't go away while we are 
     *   reflecting them
     */
    LO_LockLayout();

    if (!decoder)
	goto done;
    
    /* verify that this document is still valid */
    if (e->ce.doc_id != XP_DOCID(e->ce.context))
        goto done;

    /* reflect the form element */
    LM_ReflectFormElement(e->ce.context, e->layer_id,
			  e->form_index, e->element_index, e->pa_tag);

done:

    LO_UnlockLayout();
    ET_END_EVENT_HANDLER(e);

}

PR_STATIC_CALLBACK(void)
et_reflectElement_destructor(ReflectForm_Event * e)
{
    /* 
     * We explictly duplicated our tag when we created this event
     *   object so make sure to get rid of it now
     */
    if(e->pa_tag) {
	PA_Tag * tag = (PA_Tag *) e->pa_tag;
	if(tag->data)
		PA_FREE(tag->data);
	PA_FREE(tag);
    }

    XP_FREE(e);
}

/*
 * Reflect a form element.  This is enough of a special case that
 *   its been pulled out of the generic reflect object
 */
void
ET_ReflectFormElement(MWContext * pContext, void * form,
		      LO_FormElementStruct * form_element, PA_Tag * tag)
{
    /* create our event object */
    ReflectForm_Event * pEvent;

    if (!form || !form_element)
	return;

    pEvent = (ReflectForm_Event *) XP_NEW_ZAP(ReflectForm_Event);
    if (!pEvent)
        return;

    PR_InitEvent(&pEvent->ce.event, pContext,
		 (PRHandleEventProc)et_reflectElement_handler, 
		 (PRDestroyEventProc)et_reflectElement_destructor);

    pEvent->ce.context = pContext;
    if(tag)
        pEvent->pa_tag = (void *) PA_CloneMDLTag(tag);
    else
        pEvent->pa_tag = NULL;

    pEvent->element_index = form_element->element_index;
    pEvent->form_index = ((lo_FormData *)form)->id;
    pEvent->layer_id = form_element->layer_id;
    MAKE_EAGER(pEvent);

    et_event_to_mocha(&pEvent->ce);

}

/**********************************************************************/

typedef struct {
    ETEvent	         ce;
    void               * buffer;
    ETEvalStuff        * stuff;
    ETEvalAckFunc        fn;
} EvalStruct;


PR_STATIC_CALLBACK(void)
et_evalbuffer_handler(EvalStruct * e)
{
    JSContext	 * cx;
    JSPrincipals * principals = NULL;
    JSPrincipals * event_principals = NULL;
    jsval	   rv;
    JSBool	   ok;
    size_t         result_len;
    char         * result_str;
    char         * wysiwyg_url;
    char	 * base_href;
    uint	   len, line_no;

    ET_BEGIN_EVENT_HANDLER(e);

    LO_LockLayout();

    /* 
     * If the current document is the same as the document that sent
     *   the evaluate event we want to continue to evaluate and remember
     *   the doc_id so we can see if the document changes out from under
     *   us
     */     
    if(e->ce.doc_id != XP_DOCID(e->ce.context)) {
	LO_UnlockLayout();
        goto done;
    }

    decoder->doc_id = e->ce.doc_id;

    LO_UnlockLayout();

    len = e->stuff->len;
    line_no = e->stuff->line_no;
    lm_SetVersion(decoder, e->stuff->version);
    event_principals = e->stuff->principals;

    cx = decoder->js_context;
    if (event_principals) {
        /* First appearance on this thread. Create a root. */
        JSPRINCIPALS_HOLD(cx, event_principals);
    }
    principals = lm_GetCompilationPrincipals(decoder, event_principals);
    if (principals) {
        JSPRINCIPALS_HOLD(cx, principals);
	ok = LM_EvaluateBuffer(decoder, e->buffer, len, line_no, 
			       e->stuff->scope_to, principals, 
			       e->stuff->unicode, &rv);

    } 
    else {
        ok = JS_FALSE;
    }
    if (event_principals) {
        /* We're done with e->principals */
        JSPRINCIPALS_DROP(cx, event_principals);
    }

    /* make sure the document hasn't changed out from under us */
    if(e->ce.doc_id != XP_DOCID(e->ce.context))
        goto done;

    if(!ok) {
        ET_PostEvalAck(e->ce.context, e->ce.doc_id, e->stuff->data, 
		       NULL, 0, NULL, NULL, FALSE, e->fn);
        goto done;
    }

    if (!e->stuff->want_result)
	rv = JSVAL_VOID;
    if (rv == JSVAL_VOID || 
	!JS_ConvertValue(cx, rv, JSTYPE_STRING, &rv)) {
        ET_PostEvalAck(e->ce.context, e->ce.doc_id, e->stuff->data, 
		       NULL, 0, NULL, NULL, JS_TRUE, e->fn);
        goto done;
    }

    result_len = JS_GetStringLength(JSVAL_TO_STRING(rv));
    result_str = JS_malloc(cx, result_len + 1);
    if (result_str) {
	/* XXXunicode or is this binary data going to imagelib ? */
	XP_MEMCPY(result_str, JS_GetStringBytes(JSVAL_TO_STRING(rv)),
		  result_len);
	result_str[result_len] = '\0';
    }

    wysiwyg_url = lm_MakeWysiwygUrl(cx, decoder, decoder->active_layer_id, 
                                    principals);
    base_href = LM_GetBaseHrefTag(cx, principals);

    ET_PostEvalAck(e->ce.context, 
		   e->ce.doc_id, 
		   e->stuff->data, 
		   result_str,
		   result_len,
                   wysiwyg_url, 
		   base_href, 
		   JS_TRUE, 
		   e->fn);
done:
    if (principals)
        JSPRINCIPALS_DROP(cx, principals);
    ET_END_EVENT_HANDLER(e);
}

PR_STATIC_CALLBACK(void)
et_evalbuffer_destructor(EvalStruct * e)
{
    XP_FREEIF(e->stuff->scope_to);
    XP_FREE(e->buffer);
    XP_FREE(e->stuff);
    XP_FREE(e);
}

/*
 * This sucks a lot.  The ET_EvaluateBuffer() API needed to change
 *   but the security code is on a tagged release and can't be changed.  
 */
void
ET_EvaluateBuffer(MWContext * context, char * buffer, uint buflen,
		  uint line_no, char * scope_to, JSBool want_result,
		  ETEvalAckFunc fn, void * data,
		  JSVersion ver, struct JSPrincipals * hi)
{
    /* call ET_EvaluateScript(), please */
    XP_ASSERT(0);
}

/*
 * Evaluate the given script.  I'm sure this is going to need a
 *   callback or compeletion routine
 */
void
ET_EvaluateScript(MWContext * pContext, char * buffer, ETEvalStuff * stuff,
		  ETEvalAckFunc fn)
{

    EvalStruct * pEvent;
    int len;
    int16 charset;

    /*
     * make sure this context can do mocha, if not, don't bother
     *   sending the event over, just call the closure function and
     *   go home
     */
    if (!LM_CanDoJS(pContext)) {
	fn(stuff->data, NULL, 0, NULL, NULL, JS_FALSE);
	XP_FREE(stuff);
	return;
    }
    
    /* create our event object */
    pEvent = (EvalStruct *) XP_NEW_ZAP(EvalStruct);
    if (!pEvent) {
	XP_FREE(stuff);
        return;
    }

    /* do a PR_InitEvent on the event structure */
    PR_InitEvent(&pEvent->ce.event, pContext,
		 (PRHandleEventProc)et_evalbuffer_handler, 
		 (PRDestroyEventProc)et_evalbuffer_destructor);

    pEvent->ce.context = pContext;
    MAKE_EAGER(pEvent);

    /*
     * We are going to make our own copy of the buffer in order
     *   to be safe.  If we are on an non-ascii page just do the
     *   conversion to unicode here.  Since the JS engine is all
     *   unicode in 5.x maybe we should always just do the 
     *   transformation here.
     */
    len = stuff->len;
    charset = INTL_GetCSIWinCSID(LO_GetDocumentCharacterSetInfo(pContext));

    if (charset == CS_DEFAULT || charset == CS_ASCII || charset == CS_LATIN1) {
	char * buf;
	buf = XP_ALLOC(sizeof(char)* (len + 1));
	if (!buf) {
	    XP_FREE(stuff);
	    return;
	}
	strncpy(buf, buffer, len);
	buf[len] = '\0';
	pEvent->buffer = buf;
	stuff->unicode = JS_FALSE;
    }
    else {
	uint32 unicodeLen;

	/* find out how many unicode characters we'll end up with */
	unicodeLen = INTL_TextToUnicodeLen(charset, 
				    (unsigned char *) buffer,
				    len);
	pEvent->buffer = XP_ALLOC(sizeof(INTL_Unicode) * unicodeLen);
	if (!pEvent->buffer) {
	    XP_FREE(stuff);
	    return;
	}

	/* do the conversion */
	stuff->len = INTL_TextToUnicode(charset,
				        (unsigned char *) buffer,
				        len,
				        pEvent->buffer,
				        unicodeLen);

	stuff->unicode = JS_TRUE;

    }
    pEvent->stuff = stuff;
    pEvent->fn = fn;

    /* add the event to the event queue */
    et_event_to_mocha(&pEvent->ce);

}

/**********************************************************************/

PR_STATIC_CALLBACK(void)
et_firetimeout_handler(MozillaEvent_Timeout * e)
{
/*    ET_BEGIN_EVENT_HANDLER(e);*/	/* since we don't use the MWContext */
 
    (e->fnCallback) ((void *)e);

/*    ET_END_EVENT_HANDLER(e);*/
}

void
ET_FireTimeoutCallBack(void * obj)
{
    /*
     * our closure is actually the original event that we sent to 
     *   the mozilla thread to get this whole party started.
     * we own the freeing of this storage so macke sure we have a 
     *   valid destructor function
     */
    MozillaEvent_Timeout * pEvent = (MozillaEvent_Timeout *) obj;

    /* reuse our event */    
    PR_InitEvent(&pEvent->ce.event, NULL,
		 (PRHandleEventProc)et_firetimeout_handler, 
		 (PRDestroyEventProc)et_generic_destructor);

    /* add the event to the event queue */
    et_event_to_mocha(&pEvent->ce);

}

/**********************************************************************/

typedef struct {
    ETEvent	           ce;
    NET_StreamClass      * stream;
    NET_StreamClass	 * old_stream;
    URL_Struct	         * url_struct;
    JSBool                 free_stream_on_close;
} DecoderStreamStruct;

PR_STATIC_CALLBACK(void)
et_setstream_handler(DecoderStreamStruct * e)
{
    ET_BEGIN_EVENT_HANDLER(e);

    /* This will hold a ref on e->url_struct from the context's decoder. */
    LM_SetDecoderStream(e->ce.context, e->stream, e->url_struct,
                        e->free_stream_on_close);
    
    /* Drop the reference held below when e was constructed. */
    NET_DropURLStruct(e->url_struct);

    ET_END_EVENT_HANDLER(e);
}

/*
 */
void
ET_SetDecoderStream(MWContext * pContext, NET_StreamClass *stream,
	            URL_Struct *url_struct, JSBool free_stream_on_close)
{
    /* create our event object */
    DecoderStreamStruct * pEvent = XP_NEW_ZAP(DecoderStreamStruct);
    if(!pEvent)
        return;

    /* do a PR_InitEvent on the event structure */
    PR_InitEvent(&pEvent->ce.event, pContext,
		         (PRHandleEventProc)et_setstream_handler, 
		         (PRDestroyEventProc)et_generic_destructor);

    /* fill in the non-PR fields we care about */
    pEvent->ce.context = pContext;
    pEvent->stream = stream;
    pEvent->url_struct = url_struct;
    pEvent->free_stream_on_close = free_stream_on_close;

    /* we are holding a copy of the URL_struct across thread boundaries */
    NET_HoldURLStruct(url_struct);

    /* add the event to the event queue */
    et_event_to_mocha(&pEvent->ce);

}

/**********************************************************************/

PR_STATIC_CALLBACK(void)
et_clearstream_handler(DecoderStreamStruct * e)
{
    ET_BEGIN_EVENT_HANDLER(e);

    lm_ClearDecoderStream(decoder, JS_TRUE);
    if (e->old_stream)
        XP_FREE(e->old_stream);

    ET_END_EVENT_HANDLER(e);
}

/*
 */
void
ET_ClearDecoderStream(MWContext * pContext, NET_StreamClass * old_stream)
{
    /* create our event object */
    DecoderStreamStruct * pEvent = XP_NEW_ZAP(DecoderStreamStruct);
    if(!pEvent)
        return;

    /* do a PR_InitEvent on the event structure */
    PR_InitEvent(&pEvent->ce.event, pContext,
		(PRHandleEventProc)et_clearstream_handler, 
		(PRDestroyEventProc)et_generic_destructor);

    /* fill in the non-PR fields we care about */
    pEvent->ce.context = pContext;
    pEvent->old_stream = old_stream;

    /* add the event to the event queue */
    et_event_to_mocha(&pEvent->ce);

}

/**********************************************************************/

typedef struct {
    ETEvent	ce;
    JSObject   *layer_obj;
} DestroyLayerStruct;

PR_STATIC_CALLBACK(void)
et_destroylayer_handler(DestroyLayerStruct * e)
{
    ET_BEGIN_EVENT_HANDLER(e);

    lm_DestroyLayer(e->ce.context, e->layer_obj);

    ET_END_EVENT_HANDLER(e);
}

void
ET_DestroyLayer(MWContext * pContext, JSObject *layer_obj)
{
    DestroyLayerStruct * pEvent = XP_NEW_ZAP(DestroyLayerStruct);
    if(!pEvent)
        return;

    PR_InitEvent(&pEvent->ce.event, pContext,
		 (PRHandleEventProc)et_destroylayer_handler, 
		 (PRDestroyEventProc)et_generic_destructor);

    pEvent->ce.context = pContext;
    MAKE_EAGER(pEvent);
    pEvent->layer_obj = layer_obj;
    et_event_to_mocha(&pEvent->ce);
}

/**********************************************************************/

typedef struct {
    ETEvent	ce;
    JSBool	resize_reload;
} ReleaseDocStruct;

PR_STATIC_CALLBACK(void)
et_releasedocument_handler(ReleaseDocStruct * e)
{
    ET_BEGIN_EVENT_HANDLER(e);

    LM_ReleaseDocument(e->ce.context, e->resize_reload);

    ET_END_EVENT_HANDLER(e);
}

void
ET_ReleaseDocument(MWContext * pContext, JSBool resize_reload)
{
    /* create our event object */
    ReleaseDocStruct * pEvent = XP_NEW_ZAP(ReleaseDocStruct);
    if(!pEvent)
        return;

    /* 
     * give this event a NULL owner so it can't get revoked by an
     *   errant intertupt
     */
    PR_InitEvent(&pEvent->ce.event, NULL,
		 (PRHandleEventProc)et_releasedocument_handler, 
		 (PRDestroyEventProc)et_generic_destructor);

    pEvent->ce.context = pContext;
    MAKE_EAGER(pEvent);
    pEvent->resize_reload = resize_reload;
    et_event_to_mocha(&pEvent->ce);

}

/**********************************************************************/

typedef struct {
    ETEvent	          ce;
   IL_GroupContext *img_cx;
} InterruptImgCXEvent;


PR_STATIC_CALLBACK(void)
et_moz_interruptimgcx_func(void *data)
{
    InterruptImgCXEvent *e = data;

	IL_InterruptContext(e->img_cx);    
}


PR_STATIC_CALLBACK(void)
et_interruptimgcx_handler(InterruptImgCXEvent *e)
{
    ET_BEGIN_EVENT_HANDLER(e);

	e->img_cx = decoder->image_context;
	ET_moz_CallFunction(et_moz_interruptimgcx_func, e);

    ET_END_EVENT_HANDLER(e);
}

void
ET_InterruptImgCX(MWContext * pContext)
{
    InterruptImgCXEvent * pEvent = XP_NEW_ZAP(InterruptImgCXEvent);
    if(!pEvent)
        return;

    PR_InitEvent(&pEvent->ce.event, pContext,
		 (PRHandleEventProc)et_interruptimgcx_handler, 
		 (PRDestroyEventProc)et_generic_destructor);

    pEvent->ce.context = pContext;
    et_event_to_mocha(&pEvent->ce);

}

/**********************************************************************/

typedef struct {
    ETEvent		  ce;
    char		* szUrl;
} NestingUrlEvent;

PR_STATIC_CALLBACK(void)
et_nestingurl_handler(NestingUrlEvent * e)
{
    JSNestingUrl * url;
    ET_BEGIN_EVENT_HANDLER(e);

    if (e->szUrl) {
	/* push a new url */
	url = XP_NEW(JSNestingUrl);
	url->str = e->szUrl;
	url->next = decoder->nesting_url;
	decoder->nesting_url = url;
	e->szUrl = NULL; /* don't free below */
    }
    else {
	/* pop an old url */
	url = decoder->nesting_url;
	if (url) {
	    decoder->nesting_url = url->next;
	    XP_FREE(url->str);
	    XP_FREE(url);
	}
    }

    ET_END_EVENT_HANDLER(e);
}

PR_STATIC_CALLBACK(void)
et_nestingurl_destructor(NestingUrlEvent * e)
{
    XP_FREEIF(e->szUrl);
    XP_FREE(e);
}

void
ET_SetNestingUrl(MWContext * pContext, char * url)
{
    NestingUrlEvent * pEvent = XP_NEW_ZAP(NestingUrlEvent);
    if(!pEvent)
        return;

    PR_InitEvent(&pEvent->ce.event, pContext,
		 (PRHandleEventProc)et_nestingurl_handler, 
		 (PRDestroyEventProc)et_nestingurl_destructor);

    pEvent->ce.context = pContext;
    MAKE_EAGER(pEvent);
    if(url)
	pEvent->szUrl = XP_STRDUP(url);
    else
	pEvent->szUrl = NULL;

    et_event_to_mocha(&pEvent->ce);

}


/**********************************************************************/

typedef struct {
    ETEvent		  ce;
    JSVersion             version;
} VersionEvent;

PR_STATIC_CALLBACK(void)
et_version_handler(VersionEvent * e)
{
    ET_BEGIN_EVENT_HANDLER(e);

    JS_SetVersion(decoder->js_context, e->version);

    ET_END_EVENT_HANDLER(e);
}

void
ET_SetVersion(MWContext * pContext, JSVersion version)
{
    VersionEvent * pEvent = XP_NEW_ZAP(VersionEvent);
    if (pEvent == NULL)
        return;

    PR_InitEvent(&pEvent->ce.event, pContext,
		 (PRHandleEventProc)et_version_handler, 
		 (PRDestroyEventProc)et_generic_destructor);

    pEvent->ce.context = pContext;
    pEvent->version = version;
    et_event_to_mocha(&pEvent->ce);

}


/**********************************************************************/

typedef struct {
    ETEvent	          ce;
    void                * data;
    ETVoidPtrFunc         fn;
    History_entry	* he;
} RemoveWindowEvent;

PR_STATIC_CALLBACK(void)
et_moz_removewindow_epilog(void *data)
{
    RemoveWindowEvent *e = data;

	/* Do this before calling e->fn, which nukes e->ce.context! */
    if (e->he)
    	SHIST_DropEntry(e->ce.context, e->he);

    /* Call the FE to destroy the context, finally. */
    e->fn(e->data);
}

/*
 * Don't try to get the MochaDecoder in here with the
 *   ET_BEGIN_EVENT_HANDLER() stuff since the decoder
 *   already partially destroyed
 */
PR_STATIC_CALLBACK(void)
et_removewindow_handler(RemoveWindowEvent * e)
{
    LM_RemoveWindowContext(e->ce.context, e->he);

    /* remove any messages for this context that are waiting for us */
    /* what prevents one from getting added while running this? 
       A: Being in the monitor of lm_InterpretQueue, both when you do
       this, and when you deliver the events. -Warren */
/*    XP_ASSERT(PR_InMonitor(lm_queue_monitor));*/
    et_RevokeEvents(e->ce.context);

    ET_moz_CallFunction(et_moz_removewindow_epilog, e);
}

void
ET_RemoveWindowContext(MWContext * pContext, ETVoidPtrFunc fn, void * data)
{
    /* create our event object */
    RemoveWindowEvent * pEvent;
    History_entry * he = NULL;

    /* 
     * If mocha is disabled or this is a non-JS aware context don't 
     *   bother creating an event, just call the closure directly
     */
    if (!LM_CanDoJS(pContext)) {
	ET_moz_CallFunctionAsync(fn, data);
	return;
    }    

    /*
     * Allocate an event before possibly holding a history entry, so we can
     * return early without possibly having to drop.
     */
    pEvent = XP_NEW_ZAP(RemoveWindowEvent);
    if (!pEvent)
        return;

    /*
     * Frames are special, because their contexts are destroyed and recreated
     * when they're reloaded, even when resizing.
     */
    if (pContext->is_grid_cell) {
	lo_GridRec *grid = 0;
	lo_GridCellRec *rec = lo_ContextToCell(pContext, FALSE, &grid);

	if (rec && rec->hist_indx > 0) {
	    he = (History_entry *)rec->hist_array[rec->hist_indx - 1].hist;
	    SHIST_HoldEntry(he);
	}
    }

    PR_InitEvent(&pEvent->ce.event, pContext,
		 (PRHandleEventProc)et_removewindow_handler, 
		 (PRDestroyEventProc)et_generic_destructor);

    pEvent->ce.context = pContext;
    pEvent->fn = fn;
    pEvent->data = data;
    pEvent->he = he;

    /* set the doc_id so that we don't try to reuse this context */
    pContext->doc_id = -1;

    et_event_to_mocha(&pEvent->ce);

}


/**********************************************************************/

typedef struct {
    ETEvent	          ce;
    MochaDecoder        * decoder;
} PutDecoderEvent;

PR_STATIC_CALLBACK(void)
et_putdecoder_handler(PutDecoderEvent * e)
{
    LM_PutMochaDecoder(e->decoder);
}

/*
 * The mozilla thread is, in general, not allowed into the world of
 *   MochaDecoders, but one could have been stashed in a session
 *   history object.  If so, provide a way for the mozilla thread to
 *   release the decoder when the history object goes away.
 */
void
et_PutMochaDecoder(MWContext *pContext, MochaDecoder *decoder)
{
    PutDecoderEvent * pEvent;
    pEvent = XP_NEW_ZAP(PutDecoderEvent);
    if (!pEvent)
        return;

    PR_InitEvent(&pEvent->ce.event, pContext,
		 (PRHandleEventProc)et_putdecoder_handler,
		 (PRDestroyEventProc)et_generic_destructor);

    pEvent->ce.context = pContext;
    pEvent->decoder = decoder;
    et_event_to_mocha(&pEvent->ce);

}

/**********************************************************************/

 typedef struct {
     ETEvent	          ce;
     void *       app;
 } SetPluginWindowEvent;
 
 PR_EXTERN(void) NPL_SetPluginWindow(void *);
 
 PR_STATIC_CALLBACK(void)
 et_SetPluginWindow_handler(SetPluginWindowEvent * e)
 {
     NPL_SetPluginWindow(e->app);
 }
 
 void
 ET_SetPluginWindow(MWContext *pContext, void* app)
 {
     SetPluginWindowEvent * pEvent;
     pEvent = XP_NEW_ZAP(SetPluginWindowEvent);
     if (!pEvent)
         return;
 
     PR_InitEvent(&pEvent->ce.event, pContext,
 		 (PRHandleEventProc)et_SetPluginWindow_handler,
 		 (PRDestroyEventProc)et_generic_destructor);
 
     pEvent->ce.context = pContext;
     pEvent->app = app;
     et_event_to_mocha(&pEvent->ce);
 }
/**********************************************************************/

typedef struct {
    ETEvent	ce;
    void      * buf;
    int		len;
    int		status;
    char      * content_type;
    Bool	isUnicode;
} StreamEvent;

PR_STATIC_CALLBACK(void)
et_streamcomplete_handler(StreamEvent * e)
{
    jsval result;
    JSContext * cx;
    char *scope = NULL;
    JSNestingUrl * url;

    ET_BEGIN_EVENT_HANDLER(e);

    cx = decoder->js_context;

    if(e->content_type) {
	if(!strcasecomp(e->content_type, TEXT_JSSS)) {
	    /* scope to document */
	    scope = lm_document_str;
	}
	else if(!strcasecomp(e->content_type, TEXT_CSS)) {
	    /* convert to JS and scope to document */
	    char *new_buffer;
	    int32 new_buffer_length;

	    CSS_ConvertToJS(e->buf, 
			    e->len,
			    &new_buffer,
			    &new_buffer_length);

            XP_FREE(e->buf);
	    e->buf = new_buffer;
	    e->len = new_buffer_length;

	    if(e->len)
		e->len--; /* hack: subtract one to remove final \n */

	    scope = lm_document_str;
	}
    }

#ifdef JSDEBUGGER
    if( LM_GetJSDebugActive() )
        LM_JamSourceIntoJSDebug( LM_GetSourceURL(decoder), 
                                 e->buf, e->len, e->ce.context );
#endif /* JSDEBUGGER */

    LM_EvaluateBuffer(decoder, e->buf, e->len, 1, scope, NULL, 
		      (JSBool) e->isUnicode, &result);

    url = decoder->nesting_url;
    if (decoder->stream && !url) {
	/* complete and remove the stream */
	ET_moz_CallFunction( (ETVoidPtrFunc) decoder->stream->complete, 
			    (void *)decoder->stream);
        XP_DELETE(decoder->stream); 
        decoder->stream = 0;
        decoder->stream_owner = LO_DOCUMENT_LAYER_ID;
        decoder->free_stream_on_close = JS_FALSE;
    }

    if (url) {
	decoder->nesting_url = url->next;
	XP_FREE(url->str);
	XP_FREE(url);
	ET_PostEvalAck(e->ce.context, XP_DOCID(e->ce.context),
		       e->ce.context, NULL, 0, NULL, NULL, FALSE,
		       lo_ScriptEvalExitFn);
    }
    
    ET_END_EVENT_HANDLER(e);
}

PR_STATIC_CALLBACK(void)
et_streamcomplete_destructor(StreamEvent * e)
{
    if(e->buf)
	XP_FREE(e->buf);
    XP_FREEIF(e->content_type);
    XP_FREE(e);
}

/*
 * A mocha stream from netlib has compeleted, eveluate the contents
 *   and pass them up our stream.  We will take ownership of the 
 *   buf argument and are responsible for freeing it
 */
void
ET_MochaStreamComplete(MWContext * pContext, void * buf, int len, 
		       char *content_type, Bool isUnicode)
{
    StreamEvent * pEvent;

    pEvent = XP_NEW_ZAP(StreamEvent);
    if(!pEvent) {
	XP_FREE(buf);
        return;
    }

    PR_InitEvent(&pEvent->ce.event, pContext,
		 (PRHandleEventProc)et_streamcomplete_handler, 
		 (PRDestroyEventProc)et_streamcomplete_destructor);

    pEvent->ce.context = pContext;    
    MAKE_EAGER(pEvent);
    pEvent->buf = buf;
    pEvent->len = len;
    pEvent->isUnicode = isUnicode;
    if (content_type)
	pEvent->content_type = XP_STRDUP(content_type);

    et_event_to_mocha(&pEvent->ce);

}

/**********************************************************************/

PR_STATIC_CALLBACK(void)
et_streamabort_handler(StreamEvent * e)
{
    ET_BEGIN_EVENT_HANDLER(e);

    if (decoder->stream && !decoder->nesting_url) {
        ET_moz_Abort(decoder->stream->abort, decoder->stream, e->status);
        XP_DELETE(decoder->stream);
        decoder->stream = 0;
        decoder->free_stream_on_close = JS_FALSE;
        decoder->stream_owner = LO_DOCUMENT_LAYER_ID;
    }

    ET_END_EVENT_HANDLER(e);
}

/*
 * A mocha stream from netlib has aborted
 */
void
ET_MochaStreamAbort(MWContext * context, int status)
{

    StreamEvent * pEvent = XP_NEW_ZAP(StreamEvent);

    if(!pEvent)
        return;

    PR_InitEvent(&pEvent->ce.event, context,
                 (PRHandleEventProc)et_streamabort_handler, 
                 (PRDestroyEventProc)et_generic_destructor);

    pEvent->ce.context = context;
    pEvent->status = status;
    et_event_to_mocha(&pEvent->ce);

}

/**********************************************************************/

PR_STATIC_CALLBACK(void)
et_newlayerdoc_handler(JSEvent * e)
{
    ET_BEGIN_EVENT_HANDLER(e);

    lm_NewLayerDocument(decoder, e->layer_id);

    ET_END_EVENT_HANDLER(e);
}

/*
 * A mocha stream from netlib has aborted
 */
void
ET_NewLayerDocument(MWContext * context, int32 layer_id)
{

    JSEvent * pEvent = XP_NEW_ZAP(JSEvent);

    if(!pEvent)
        return;

    /* do a PR_InitEvent on the event structure */
    PR_InitEvent(&pEvent->ce.event, context,
		 (PRHandleEventProc)et_newlayerdoc_handler, 
		 (PRDestroyEventProc)et_generic_destructor);

    pEvent->ce.context = context;
    pEvent->layer_id = layer_id;
    et_event_to_mocha(&pEvent->ce);

}

/**********************************************************************/

typedef struct {
    ETEvent ce;
    int32 layer_id;
    LO_BlockInitializeStruct *param;
    ETRestoreAckFunc fn;
    void *data;
} RestoreStruct;


PR_STATIC_CALLBACK(void)
et_restorelayerstate_handler(RestoreStruct * e)
{
    ET_BEGIN_EVENT_HANDLER(e);

    lm_RestoreLayerState(e->ce.context, e->layer_id, 
                         e->param);

    ET_PostRestoreAck(e->data, e->param, e->fn);

    ET_END_EVENT_HANDLER(e);
}

void 
ET_RestoreLayerState(MWContext *context, int32 layer_id,
                     LO_BlockInitializeStruct *param, ETRestoreAckFunc fn,
                     void *data)
{

    RestoreStruct * pEvent = XP_NEW_ZAP(RestoreStruct);

    if(!pEvent)
        return;

    PR_InitEvent(&pEvent->ce.event, context,
		 (PRHandleEventProc)et_restorelayerstate_handler, 
		 (PRDestroyEventProc)et_generic_destructor);

    pEvent->ce.context = context;
    pEvent->layer_id = layer_id;
    pEvent->param = param;
    pEvent->fn = fn;
    pEvent->data = data;
    MAKE_EAGER(pEvent);
    et_event_to_mocha(&pEvent->ce);
}


/**********************************************************************/

typedef struct {
    ETEvent	ce;
    PA_Block	onload;
    PA_Block	onunload;
    PA_Block	onfocus;
    PA_Block	onblur;
    PA_Block	onhelp;
    PA_Block	onmouseover;
    PA_Block	onmouseout;
    PA_Block	ondragdrop;
    PA_Block	onmove;
    PA_Block	onresize;
    PA_Block    id;
    char        *all;
    Bool	bDelete;
    int		newline_count;
} ReflectWindowEvent;

PR_STATIC_CALLBACK(void)
et_reflectwindow_handler(ReflectWindowEvent * e)
{
    JSObject *obj;

    ET_BEGIN_EVENT_HANDLER(e);

    obj = decoder->window_object;

    if (e->onload) {
	(void) lm_CompileEventHandler(decoder, e->id, (PA_Block) e->all, 
				      e->newline_count, 
				      obj, PARAM_ONLOAD, e->onload);
    }
    if (e->onunload) {
	(void) lm_CompileEventHandler(decoder, e->id, (PA_Block) e->all, 
				      e->newline_count, 
				      obj, PARAM_ONUNLOAD, e->onunload);	
    }
    if (e->onfocus) {
	(void) lm_CompileEventHandler(decoder, e->id, (PA_Block) e->all, 
				      e->newline_count, 
				      obj, PARAM_ONFOCUS, e->onfocus);	
    }
    if (e->onblur) {
	(void) lm_CompileEventHandler(decoder, e->id, (PA_Block) e->all, 
				      e->newline_count, 
				      obj, PARAM_ONBLUR, e->onblur);	
    }

    if (e->onhelp) {
	(void) lm_CompileEventHandler(decoder, e->id, (PA_Block) e->all, 
				      e->newline_count, 
				      obj, PARAM_ONHELP, e->onhelp);	
    }

    if (e->onmouseover) {
	(void) lm_CompileEventHandler(decoder, e->id, (PA_Block) e->all, 
				      e->newline_count, 
				      obj, PARAM_ONMOUSEOVER, e->onmouseover);	
    }

    if (e->onmouseout) {
	(void) lm_CompileEventHandler(decoder, e->id, (PA_Block) e->all, 
				      e->newline_count, 
				      obj, PARAM_ONMOUSEOUT, e->onmouseout);	
    }

    if (e->ondragdrop) {
	(void) lm_CompileEventHandler(decoder, e->id, (PA_Block) e->all, 
				      e->newline_count, 
				      obj, PARAM_ONDRAGDROP, e->ondragdrop);	
    }

    if (e->onmove) {
	(void) lm_CompileEventHandler(decoder, e->id, (PA_Block) e->all, 
				      e->newline_count, 
				      obj, PARAM_ONMOVE, e->onmove);	
    }

    if (e->onresize) {
	(void) lm_CompileEventHandler(decoder, e->id, (PA_Block) e->all, 
				      e->newline_count, 
				      obj, PARAM_ONRESIZE, e->onresize);	
    }

    if (e->bDelete) {
	if(e->onload)
	    XP_FREE(e->onload);
	if(e->onunload)
	    XP_FREE(e->onunload);
	if(e->onfocus)
	    XP_FREE(e->onfocus);
	if(e->onblur)
	    XP_FREE(e->onblur);
	if(e->onhelp)
	    XP_FREE(e->onhelp);
	if(e->onmouseover)
	    XP_FREE(e->onmouseover);
	if(e->onmouseout)
	    XP_FREE(e->onmouseout);
	if(e->ondragdrop)
	    XP_FREE(e->ondragdrop);
	if(e->onmove)
	    XP_FREE(e->onmove);
	if(e->onresize)
	    XP_FREE(e->onresize);
	if(e->id)
	    XP_FREE(e->id);
	if(e->all)
	    XP_FREE(e->all);
    }

    ET_END_EVENT_HANDLER(e);
}

/*
 * Reflect window events
 */
void
ET_ReflectWindow(MWContext * pContext, 
		 PA_Block onLoad, PA_Block onUnload, 
		 PA_Block onFocus, PA_Block onBlur, PA_Block onHelp,
		 PA_Block onMouseOver, PA_Block onMouseOut, PA_Block onDragDrop,
		 PA_Block onMove, PA_Block onResize,
                 PA_Block id, char *all, Bool bDelete, 
                 int newline_count)
{

    ReflectWindowEvent * pEvent = XP_NEW_ZAP(ReflectWindowEvent);
    if(!pEvent)
        return;

    PR_InitEvent(&pEvent->ce.event, pContext,
		 (PRHandleEventProc)et_reflectwindow_handler, 
		 (PRDestroyEventProc)et_generic_destructor);

    pEvent->ce.context = pContext;
    MAKE_EAGER(pEvent);
    pEvent->onload = onLoad;
    pEvent->onunload = onUnload;
    pEvent->onfocus = onFocus;
    pEvent->onblur = onBlur;
    pEvent->onhelp = onHelp;
    pEvent->onmouseover = onMouseOver;
    pEvent->onmouseout = onMouseOut;
    pEvent->ondragdrop = onDragDrop;
    pEvent->onmove = onMove;
    pEvent->onresize = onResize;
    pEvent->id = id;
    pEvent->all = all;
    pEvent->bDelete = bDelete;
    pEvent->newline_count = newline_count;

    /* add the event to the event queue */
    et_event_to_mocha(&pEvent->ce);

}

/**********************************************************************/

#ifdef DEBUG
PR_STATIC_CALLBACK(void)
lm_dump_named_root(const char *name, void *rp, void *data)
{
    XP_TRACE(("Leaked named root \"%s\" at 0x%x", name, rp));
}
#endif

PR_STATIC_CALLBACK(void)
et_FinishMochaHandler(JSEvent * e)
{
    MochaDecoder *decoder;

    decoder = lm_crippled_decoder;
    if (decoder) {
	LM_PutMochaDecoder(decoder);
	lm_crippled_decoder = 0;
    }

#ifdef JAVA
    JSJ_Finish();
#endif
#ifdef DEBUG
    JS_DumpNamedRoots(lm_runtime, lm_dump_named_root, NULL);
#endif
    JS_Finish(lm_runtime);

    /* turn off the mocha thread here! */

}

void
ET_FinishMocha(void)
{

    JSEvent * pEvent;

    /*
     * Annoyingly, the winfe might call us without event actually
     *   initializing mocha (if an instance is already running)
     */
    if (!lm_InterpretQueue)
	return;

    pEvent = XP_NEW_ZAP(JSEvent);
    if (!pEvent)
        return;

    PR_InitEvent(&pEvent->ce.event, NULL,
		 (PRHandleEventProc)et_FinishMochaHandler, 
		 (PRDestroyEventProc)et_generic_destructor);

    et_event_to_mocha(&pEvent->ce);

}

/**********************************************************************/

typedef struct {
    ETEvent	ce;
    void      * data;
    JSBool	processed;
} DocWriteAckEvent;

PR_STATIC_CALLBACK(void)
et_docwriteack_handler(DocWriteAckEvent * e)
{
    e->processed = JS_TRUE;
    et_TopQueue->done = TRUE;
    et_TopQueue->retval = e->data;
}

PR_STATIC_CALLBACK(void)
et_docwriteack_destructor(DocWriteAckEvent * e)
{
    if (!e->processed)
	et_docwriteack_handler(e);
    XP_FREE(e);
}		      

void
ET_DocWriteAck(MWContext * context, int status)
{
    DocWriteAckEvent * pEvent;

    pEvent = XP_NEW_ZAP(DocWriteAckEvent);
    if (!pEvent)
        return;

    PR_InitEvent(&pEvent->ce.event, context,
		 (PRHandleEventProc)et_docwriteack_handler, 
		 (PRDestroyEventProc)et_docwriteack_destructor);

    pEvent->ce.context = context;
    pEvent->ce.handle_eagerly = TRUE;
    pEvent->data = (void *)status;
    et_event_to_mocha(&pEvent->ce);
}

/**********************************************************************/

void
et_SubEventLoop(QueueStackElement * qse)
{
    PREvent      * pEvent;

    /* while there are events process them */
    while (!qse->done) {

	/* can't be interrupted yet */
	lm_InterruptCurrentOp = JS_FALSE;

        LM_LockJS();
	/* need to interlock the getting of an event with ET_Interrupt */
        PR_EnterMonitor(lm_queue_monitor);
        pEvent = PR_GetEvent(qse->queue);

	/* if we got an event handle it else wait for something */
        if(pEvent) {
            PR_ExitMonitor(lm_queue_monitor);
	    LM_JSLockSetContext(((ETEvent *)pEvent)->context);
            PR_HandleEvent(pEvent);
            LM_UnlockJS();
#ifdef DEBUG
	    /* make sure we don't have the layout lock */
	    while(!LO_VerifyUnlockedLayout()) {
		XP_ASSERT(0);
		LO_UnlockLayout();
	    }
#endif

        }
        else {
            /* queue is empty, wait for something to show up */
            LM_UnlockJS();
            PR_Wait(lm_queue_monitor, PR_INTERVAL_NO_TIMEOUT);
            PR_ExitMonitor(lm_queue_monitor);
        }
         
    }
}

extern PRThread		    *lm_InterpretThread;
extern PRMonitor *lm_owner_mon;

/*
 * Sit around in the mocha thread waiting for events to show up
 */
void PR_CALLBACK
lm_wait_for_events(void * pB)
{
    XP_ASSERT(et_TopQueue);

    /*
     * In NSPR 2.0 this thread could get created and it could start
     *   running before our parent is done initializing our state.
     *   The mozilla thread will have done a PR_EnterMonitor() on
     *   the lm_owner_mon before creating the mocha thread and will
     *   not exit the monitor until all of the state is initialized.
     *   So we are assured that if we can get the monitor here the
     *   mozilla thread has released it and we are OK to run.
     */
    PR_EnterMonitor(lm_owner_mon);
    PR_ExitMonitor(lm_owner_mon);

    /* set up the initial queue stack pointers */
    et_TopQueue->queue = lm_InterpretQueue;

    /* create our monitor if it doesn't exist already */
    if(lm_queue_monitor == NULL) {
        lm_queue_monitor = PR_NewNamedMonitor("lm-queue-monitor");
        if(!lm_queue_monitor)
            return;
    }

    while (TRUE) {
	et_SubEventLoop(et_TopQueue);

	/* should never get here but just in case behave nicely */
	XP_ASSERT(0);
	et_TopQueue->done = FALSE;
    }

}

/**********************************************************************/

typedef struct {
    ETEvent	    ce;
    char	   *name;
    ETBoolPtrFunc   active_callback;
    ETVoidPtrFunc   startup_callback;
} RegComponentStruct;

PR_STATIC_CALLBACK(void)
et_registercomponent_handler(RegComponentStruct * e)
{
    lm_RegisterComponent(e->name, e->active_callback, e->startup_callback);
}

PR_STATIC_CALLBACK(void)
et_registercomponent_destructor(RegComponentStruct * e)
{
    if (e->name)
        XP_FREE(e->name);

    XP_FREE(e);
}

void
ET_RegisterComponent(char *name, void *active_callback, void *startup_callback)
{
    /* create our event object */
    RegComponentStruct * pEvent = XP_NEW_ZAP(RegComponentStruct);
    if(!pEvent)
        return;

    /* do a PR_InitEvent on the event structure */
    PR_InitEvent(&pEvent->ce.event, NULL,
		 (PRHandleEventProc)et_registercomponent_handler, 
		 (PRDestroyEventProc)et_registercomponent_destructor);

    /* fill in the non-PR fields we care about */
    if (name)
	pEvent->name = XP_STRDUP(name);
    else
	pEvent->name = NULL;
    pEvent->active_callback = (ETBoolPtrFunc)active_callback;
    pEvent->startup_callback = (ETVoidPtrFunc)startup_callback;

    /* add the event to the event queue */
    et_event_to_mocha(&pEvent->ce);
}

/**********************************************************************/

typedef struct {
    ETEvent	ce;
    char       *comp;
    char       *name;
    uint8	retType;
    void       *setter;
    void       *getter;
} RegComponentPropStruct;

PR_STATIC_CALLBACK(void)
et_registercomponentprop_handler(RegComponentPropStruct * e)
{
    lm_RegisterComponentProp(e->comp, e->name, e->retType, 
	(ETCompPropSetterFunc)e->setter, (ETCompPropGetterFunc)e->getter); 
}

PR_STATIC_CALLBACK(void)
et_registercomponentprop_destructor(RegComponentPropStruct * e)
{
    if (e->comp)
        XP_FREE(e->comp);
    if (e->name)
        XP_FREE(e->name);

    XP_FREE(e);
}

void
ET_RegisterComponentProp(char *comp, char *name, uint8 retType, void *setter, 
			 void *getter)
{
    /* create our event object */
    RegComponentPropStruct * pEvent = XP_NEW_ZAP(RegComponentPropStruct);
    if(!pEvent)
        return;

    /* this won't work without a component and property name. */
    if (!comp || !name)
	return;

    /* do a PR_InitEvent on the event structure */
    PR_InitEvent(&pEvent->ce.event, NULL,
		 (PRHandleEventProc)et_registercomponentprop_handler, 
		 (PRDestroyEventProc)et_registercomponentprop_destructor);

    /* fill in the non-PR fields we care about */
    pEvent->comp = XP_STRDUP(comp);
    pEvent->name = XP_STRDUP(name);
    pEvent->retType = retType;
    pEvent->setter = setter;
    pEvent->getter = getter;

    /* add the event to the event queue */
    et_event_to_mocha(&pEvent->ce);
}

/**********************************************************************/

typedef struct {
    ETEvent	ce;
    char       *comp;
    char       *name;
    uint8	retType;
    void       *method;
    int32	argc;
} RegComponentMethodStruct;

PR_STATIC_CALLBACK(void)
et_registercomponentmethod_handler(RegComponentMethodStruct * e)
{
    lm_RegisterComponentMethod(e->comp, e->name, e->retType, (ETCompMethodFunc)e->method, e->argc); 
}

PR_STATIC_CALLBACK(void)
et_registercomponentmethod_destructor(RegComponentMethodStruct * e)
{
    if (e->comp)
        XP_FREE(e->comp);
    if (e->name)
        XP_FREE(e->name);
    XP_FREE(e);
}

void
ET_RegisterComponentMethod(char *comp, char *name, uint8 retType, void *method, 
			   int32 argc)
{
    /* create our event object */
    RegComponentMethodStruct * pEvent = XP_NEW_ZAP(RegComponentMethodStruct);
    if(!pEvent)
        return;

    /* do a PR_InitEvent on the event structure */
    PR_InitEvent(&pEvent->ce.event, NULL,
		 (PRHandleEventProc)et_registercomponentmethod_handler, 
		 (PRDestroyEventProc)et_registercomponentmethod_destructor);

    /* fill in the non-PR fields we care about */
    pEvent->comp = XP_STRDUP(comp);
    pEvent->name = XP_STRDUP(name);
    pEvent->retType = retType;
    pEvent->method = method;
    pEvent->argc = argc;

    /* add the event to the event queue */
    et_event_to_mocha(&pEvent->ce);
}




