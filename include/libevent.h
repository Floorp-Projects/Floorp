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
 * Header file for event passing between the mozilla thread and
 *   the mocha thread
 */

#ifndef libevent_h___
#define libevent_h___

#include "libmocha.h"
#include "prtypes.h"
#ifndef NSPR20
#include "prevent.h"
#else
#include "plevent.h"
#endif
#include "shist.h"
#include "fe_proto.h"
#include "lo_ele.h"

NSPR_BEGIN_EXTERN_C

extern PREventQueue * mozilla_event_queue;

/* 
 * XXX - should we use the same event values as layer events?
 */

/* Event bits stored in the low end of decoder->event_mask. */
#define EVENT_MOUSEDOWN 0x00000001
#define EVENT_MOUSEUP   0x00000002
#define EVENT_MOUSEOVER 0x00000004      /* user is mousing over a link */
#define EVENT_MOUSEOUT  0x00000008      /* user is mousing out of a link */
#define EVENT_MOUSEMOVE 0x00000010  
#define EVENT_MOUSEDRAG 0x00000020
#define EVENT_CLICK     0x00000040      /* input element click in progress */
#define EVENT_DBLCLICK  0x00000080
#define EVENT_KEYDOWN   0x00000100
#define EVENT_KEYUP     0x00000200
#define EVENT_KEYPRESS	0x00000400
#define EVENT_DRAGDROP  0x00000800      /* not yet implemented */
#define EVENT_FOCUS     0x00001000      /* input focus event in progress */
#define EVENT_BLUR      0x00002000      /* loss of focus event in progress */
#define EVENT_SELECT    0x00004000      /* input field selection in progress */
#define EVENT_CHANGE    0x00008000      /* field value change in progress */
#define EVENT_RESET     0x00010000      /* form submit in progress */
#define EVENT_SUBMIT    0x00020000      /* form submit in progress */
#define EVENT_SCROLL    0x00040000      /* window is being scrolled */
#define EVENT_LOAD      0x00080000      /* layout parsed a loaded document */
#define EVENT_UNLOAD    0x00100000
#define EVENT_XFER_DONE	0x00200000      /* document has loaded */
#define EVENT_ABORT     0x00400000
#define EVENT_ERROR     0x00800000
#define EVENT_LOCATE	0x01000000
#define EVENT_MOVE	0x02000000
#define EVENT_RESIZE    0x04000000
#define EVENT_FORWARD   0x08000000
#define EVENT_HELP	0x10000000		/* for handling of help events */
#define EVENT_BACK      0x20000000

/* #define EVENT_PRINT     0x20000000	*//* To be removed per joki */

#define STATUS_STOP      0x00000001      /* stop processing */
#define STATUS_IGNORE    0x00000002      /* no new messages */

#define	EVENT_ALT_MASK	    0x00000001
#define	EVENT_CONTROL_MASK  0x00000002
#define	EVENT_SHIFT_MASK    0x00000004
#define	EVENT_META_MASK	    0x00000008

#define ARGTYPE_NULL	0x00000001
#define ARGTYPE_INT32	0x00000002
#define ARGTYPE_BOOL	0x00000004
#define ARGTYPE_STRING	0x00000008

#define SIZE_MAX	0x00000001
#define SIZE_MIN        0X00000002
/*
 * When the event has been processed by the backend, there will be
 *   a front-end callback that gets called.  If the event processed
 *   successfully, the callback will be passed EVENT_OK.  If the
 *   event wasn't successful (i.e. the user canceled it) the return
 *   status will be EVENT_CANCEL.  If something radical happened
 *   and the front-end should do nothing (i.e. mocha changed the
 *   underlying context) the status will be EVENT_PANIC and the
 *   front end should treat the context and element passed to the
 *   exit routine as bogus
 */
typedef enum {
    EVENT_OK,
    EVENT_CANCEL,
    EVENT_PANIC
} ETEventStatus;

/*
 * When a given event gets processed we may need to tell the front
 *   end about it so that they can update the UI / continue the 
 *   operation.  The context, lo_element, lType and whatever
 *   field are all supplied by the original ET_SendEvent() call.
 *   See ET_SendEvent() for a description of the values for
 *   the status parameter
 */
typedef void
(*ETClosureFunc)(MWContext * pContext, LO_Element * lo_element, 
                 int32 lType, void * whatever, ETEventStatus status);

/*
 * Someone has initiated a call to LM_EvaluateBuffer().  This function
 *   gets called back with the result
 */
 typedef void
(*ETEvalAckFunc)(void * data, char * result_string, size_t result_length,
		 char * wysiwyg_url, char * base_href, Bool valid);

/*
 * This function is called back after a layer's state has been restored
 * in a resize_relayout.
 */
 typedef void
 (*ETRestoreAckFunc)(void * data, LO_BlockInitializeStruct *param);

/*
 * Typedef for a function taking a void pointer and
 *   returning nothing
 */
 typedef void
(*ETVoidPtrFunc)(void * data);

/*
 * Typedef for a function taking a void pointer and
 *   returning a bool
 */
 typedef PRBool
(*ETBoolPtrFunc)(void * data);

/*
 * Typedef for a function taking a void pointer and
 *   returning a int32
 */
 typedef int32
(*ETIntPtrFunc)(void * data);

/*
 * Typedef for a function taking a void pointer and
 *   returning a char *
 */
 typedef char *
(*ETStringPtrFunc)(void * data);

/*
 * Struct for passing JS typed variable info through C interface calls
 */
typedef union ArgVal {
    int32		intArg;
    XP_Bool		boolArg;
    char *		stringArg;
} ArgVal;

typedef struct {
    uint8		type;	  /* arg type as defined at top of file */
    ArgVal		value;		
} JSCompArg;

/*
 * Typedef for a function used to verify installed components and 
 *   get back components utility functions.
 */
 typedef PRBool
(*ETVerifyComponentFunc)(void **active_callback, void **startup_callback);

/*
 * Generic function for JS setting values with native calls.
 */
 typedef void
(*ETCompPropSetterFunc)(char *name, void *value);

/*
 * Generic function for JS getting values from native calls.
 */
 typedef void*
(*ETCompPropGetterFunc)(char *name);

/*
 * Generic function for JS calling native methods.
 */
 typedef void*
(*ETCompMethodFunc)(int32 argc, JSCompArg *argv);

/* --------------------------------------------------------------------------
 * Common prologue for talking between the mocha thread and the mozilla 
 *   thread
 */
typedef struct {
    PREvent		event;	  /* the PREvent structure */
    MWContext*		context;  /* context */
    int32		doc_id;   /* doc id of context when event launched */
    PRPackedBool	handle_eagerly;
} ETEvent;

/*
 * Struct to send back from front end in order to get additional 
 *   event information without having to initialize event object 
 *   until necessary.  Yow, there is a lot of junk in here now
 *   can we make a union out of some of these or are they always
 *   needed?
 */
typedef struct {
    ETEvent	    ce;
    MochaDecoder  * decoder;
    JSObject	  * object;
    int32	    type;
    int32           layer_id;
    int32	    id;
    LO_Element	  * lo_element;
    ETClosureFunc   fnClosure;    /* event sender closure           */
    void          * whatever;     /* anything other state           */
    int32	    x,y;
    int32	    docx,docy;
    int32	    screenx,screeny;
    uint32	    which;
    uint32	    modifiers;
    void	  * data;
    uint32	    dataSize;
    PRPackedBool    saved;
    PRPackedBool    event_handled;
} JSEvent;

/*
 * Tell the backend about a new event.
 * The event is placed onto an event queue, it is not processed
 *   immediately.  If the event is the type that can be cancelled
 *   by the backend (i.e. a button click or a submit) the front
 *   end must wait until the callback routine gets called before
 *   continuing with the operation.  The ETEventStatus will be
 *   EVENT_OK if the operation is to continue or EVENT_CANCEL
 *   if it got cancelled.
 * 
 * The processing of the event may cause the document to change 
 *   or even the whole window to close.  In those cases the callback 
 *   will still get called in case there is any front-end cleanup 
 *   to do but the ETEventStatus will be set to EVENT_PANIC
 *
 */

extern JSBool
ET_SendEvent(MWContext * pContext, LO_Element *pElement, JSEvent *pEvent, 
             ETClosureFunc fnClosure, void * whatever);

/*
 * Tell the backend about a new document load event.  We need a 
 *   closure so that libparse/layout knows when its safe to discard
 *   the old document when they were waiting for onunload events to
 *   finish processing
 */
extern void
ET_SendLoadEvent(MWContext * pContext, int32 type, ETVoidPtrFunc fnClosure,
		 NET_StreamClass *stream, int32 layer_id, Bool resize_reload);

/*
 * Tell the backend about a new image event.  Async.  No closure
 */
extern void
ET_SendImageEvent(MWContext * pContext, LO_ImageStruct *image_data,
                  LM_ImageEvent event);

/*
 * Send an interrupt event to the current context
 * Remove all pending events for the event queue of the given context.
 */
extern void
ET_InterruptContext(MWContext * pContext);

extern JSBool
ET_ContinueProcessing(MWContext * pContext);

/*
 * Tell mocha to destroy the given context's data.  The callback 
 *   function gets called when mocha is done with all of its data
 *   that was associated with the context
 */
extern void
ET_RemoveWindowContext(MWContext * context, ETVoidPtrFunc fn, 
                       void * data);

typedef struct {
    uint	   len, line_no;
    char	 * scope_to;
    void	 * data;
    JSVersion	   version;
    JSPrincipals * principals;
    JSBool         want_result;
    JSBool	   unicode;
} ETEvalStuff;

/*
 * Evaluate the mocha code in the given buffer
 */
extern void
ET_EvaluateBuffer(MWContext * context, char * buffer, uint buflen,
		  uint line_no, char * scope_to, JSBool want_result,
		  ETEvalAckFunc fn, void * data,
		  JSVersion ver, struct JSPrincipals *);

extern void
ET_EvaluateScript(MWContext * context, char * buffer, ETEvalStuff * stuff,
		  ETEvalAckFunc fn);

/*
 * Ask Mocha to reflect the given object into JavaScript
 */
extern void
ET_ReflectObject(MWContext * pContext, void * lo_ele, void * tag,
		 int32 layer_id, uint index, ReflectedObject type);

void
ET_ReflectFormElement(MWContext * pContext, void * form,
		      LO_FormElementStruct * form_element, PA_Tag * tag);

extern void
ET_ReflectWindow(MWContext * pContext, 
		 PA_Block onLoad, PA_Block onUnload, 
		 PA_Block onFocus, PA_Block onBlur, PA_Block onHelp,
		 PA_Block onMouseOver, PA_Block onMouseOut, PA_Block onDragDrop,
		 PA_Block onMove, PA_Block onResize,
                 PA_Block id, char *all,
		 Bool bDelete, int newline_count);

/*
 * Tell mocha we are processing a form
 */
extern void
ET_SetActiveForm(MWContext * pContext, struct lo_FormData_struct * loElement);

/*
 * Tell mocha which layer we are processing
 */
void
ET_SetActiveLayer(MWContext * pContext, int32 layer_id);

/*
** Tell mocha where to send its output
*/
extern void
ET_ClearDecoderStream(MWContext * context, NET_StreamClass * old_stream);

extern void
ET_SetDecoderStream(MWContext * context, NET_StreamClass *stream,
	            URL_Struct *url_struct, JSBool free_stream_on_close);

/*
** Remember the current nesting URL in the MochaDecoder
*/
extern void
ET_SetNestingUrl(MWContext * context, char * szUrl);

/*
** Remember the current language version in the MochaDecoder
*/
extern void
ET_SetVersion(MWContext * context, JSVersion version);

/*
 * Tell mocha to trash the current document.  around and around...
 */
extern void
ET_ReleaseDocument(MWContext * pContext, JSBool resize_reload);

/*
 * Tell mocha to trash the layer's document.
 */
extern void
ET_DestroyLayer(MWContext * pContext, JSObject *layer_obj);

extern void
ET_MochaStreamComplete(MWContext * context, void * buf, int len, 
		       char * content_type, Bool isUnicode);

extern void
ET_MochaStreamAbort(MWContext * context, int status);
			
/*
 * Called when a layer's contents are changing and we want to create
 * a new layer document.
 */
extern void
ET_NewLayerDocument(MWContext *pContext, int32 layer_id);

extern void
ET_DocWriteAck(MWContext *pContext, int status);

extern void
ET_RegisterComponent(char *name, void *active_callback, void *startup_callback);

extern void
ET_RegisterComponentProp(char *comp, char *name, uint8 retType, void *setter, 
			 void *getter);

extern void
ET_RegisterComponentMethod(char *comp, char *name, uint8 retType, void *method, 
			   int32 argc);

/* =============================================================== */

/*
 * This event can be sent to both the mozilla thread and the moacha thread
 */
typedef struct {
    ETEvent			ce;
    TimeoutCallbackFunction	fnCallback;
    void*			pClosure;
    uint32			ulTime;
    void*			pTimerId;
} MozillaEvent_Timeout;


/* =============================================================== */

/*
 * Busy loop waiting for events to come along
 */
extern void PR_CALLBACK
lm_wait_for_events(void *);

/* 
 * global mocha event queues.  It would be nice to not have these
 *   exported this globally
 */
extern PREventQueue *lm_InterpretQueue;
extern PREventQueue *lm_PriorityQueue;

/*
 * Ways to send events to the front end
 */
extern JSBool
ET_PostMessageBox(MWContext* context, char* szMessage, 
		  JSBool bConfirm);

extern void
ET_PostProgress(MWContext* context, const char* szMessage);

/* --- timeout routines --- */

/*
 * Set (or clear) a timeout to go off.  The timeout will go off in the
 *   mozilla thread so we will use the routine ET_FireTimeoutCallBack()
 *   to get back into our thread to actually run the closure
 */
extern void * 
ET_PostSetTimeout(TimeoutCallbackFunction fnCallback, 
		  void * pClosure, uint32 ulTime, int32 doc_id);

extern void
ET_PostClearTimeout(void * stuff);

extern void
ET_FireTimeoutCallBack(void *);

/* --- end of timeout routines --- */

extern void
ET_PostDestroyWindow(MWContext * context);

extern void
ET_PostManipulateForm(MWContext * context, LO_Element * pForm, int32 action);

extern void
ET_PostClearView(MWContext * context);

extern void
ET_PostFreeImageElement(MWContext * context, void * stuff);

extern void
ET_PostFreeImageContext(MWContext *context, IL_GroupContext *img_cx);

extern void
ET_PostFreeAnonImages(MWContext *context, IL_GroupContext *img_cx);

extern void
ET_PostDisplayImage(MWContext *, int, LO_ImageStruct *);

extern void
ET_PostGetUrl(MWContext *, URL_Struct * pUrl);

extern char *
ET_PostPrompt(MWContext* context, const char* szMessage, 
			  const char * szDefault);

extern MWContext *
ET_PostNewWindow(MWContext* context, URL_Struct * pUrl, 
                 char * szName, Chrome * pChrome);

extern void
ET_PostUpdateChrome(MWContext* context, Chrome * pChrome);

extern void
ET_PostQueryChrome(MWContext* context, Chrome * pChrome);

extern void
ET_PostGetScreenSize(MWContext* context, int32 *pX, int32 *pY);

extern void
ET_PostGetAvailScreenRect(MWContext* context, int32 *pX, int32 *pY, 
					    int32 *pLeft, int32 *pTop);

extern void
ET_PostGetColorDepth(MWContext* context, int32 *pPixel, int32 *pPallette);

extern char *
ET_PostGetSelectedText(MWContext* context);

extern void
ET_PostScrollDocTo(MWContext* context, int loc, int32 x, int32 y);

extern void
ET_PostScrollDocBy(MWContext* context, int loc, int32 x, int32 y);

extern void
ET_PostBackCommand(MWContext* context);

extern void
ET_PostForwardCommand(MWContext* context);

extern void
ET_PostHomeCommand(MWContext* context);

extern JSBool
ET_PostFindCommand(MWContext* context, char * szName, JSBool matchCase, 
		   JSBool searchBackward);
extern void
ET_PostPrintCommand(MWContext* context);

extern void
ET_PostOpenFileCommand(MWContext* context);

extern void 
ET_MakeHTMLAlert(MWContext * context, const char * szString);

/* respond to events sent to the mocha thread by the mozilla thread */

extern void
ET_PostJsEventAck(MWContext* context, LO_Element * pEle, int type, 
                  ETClosureFunc fnClosure, void * pStuff, 
                  ETEventStatus status);



extern void
ET_PostEvalAck(MWContext * context, int doc_id, void * data, 
	       char * str, size_t len, char * wysiwyg_url, 
	       char * base_href, Bool valid, ETEvalAckFunc fn);

extern void
ET_PostRestoreAck(void *data, LO_BlockInitializeStruct *param, 
                  ETRestoreAckFunc fn);

/* netlib events */

extern char *
ET_net_GetCookie(MWContext* context, int32 doc_id);

extern char *
ET_net_SetCookieString(MWContext* context, char * szCookie, int32 doc_id);

extern NET_StreamClass *
ET_net_CacheConverter(FO_Present_Types format, void * obj,
                      URL_Struct *pUrl, MWContext * pContext);

extern void
ET_net_FindURLInCache(URL_Struct * pUrl, MWContext * pContext);

extern NET_StreamClass	*
ET_net_StreamBuilder(FO_Present_Types format, URL_Struct *pUrl, 
		     MWContext * pContext);

/* layout events */

extern  void
ET_lo_ResetForm(MWContext * pContext, LO_Element * ele);

void
ET_fe_SubmitInputElement(MWContext * pContext, LO_Element * ele);

/*
 * Synchronously shove the given text down the parser's processing
 *   queue.  If the currently loaded document is not equal to 
 *   doc_id, this message should be ignored since it arrived too
 *   late for the intended document
 */
extern int
ET_lo_DoDocWrite(JSContext *cx, MWContext * context, NET_StreamClass * stream, 
                 char * str, size_t len, int32 doc_id);


extern void
ET_il_GetImage(const char * str, MWContext * pContext, IL_GroupContext *img_cx,
               LO_ImageStruct * image_data, NET_ReloadMethod how);

extern void
ET_il_SetGroupObserver(MWContext * pContext, IL_GroupContext *pImgCX, void *pDpyCX,
					   JSBool bAddObserver);
			   
extern void
ET_InterruptImgCX(MWContext * pContext);

/*
 * Tell layout to trash the current document.
 */
extern void
ET_lo_DiscardDocument(MWContext * pContext);

/*
 * Tell layout to prepare a layer for writing.
 */
extern Bool
ET_lo_PrepareLayerForWriting(MWContext *context, int32 layer_id,
			     const char *referer);

/*
 * Return a copy of the current history element.  Caller must free
 */
extern History_entry * 
ET_shist_GetCurrent(MWContext * pContext);

/*
 * Return the current security status.
 */
extern int 
ET_GetSecurityStatus(MWContext * pContext);

/*
 * Make sure Mocha/Java glue is ready.  Returns the same return code as
 * LM_InitMoja.
 */
extern int
ET_InitMoja(MWContext * pContext);

/*
 * Pack up toys and go home
 */
extern void
ET_FinishMocha(void);

/*
 * Used to call a stream completion function in the mozilla
 *   thread
 */
extern void
ET_moz_CallFunction(ETVoidPtrFunc fn, void * data);

extern void
ET_moz_CallFunctionAsync(ETVoidPtrFunc fn, void * data);

extern PRBool
ET_moz_CallFunctionBool(ETBoolPtrFunc fn, void * data);

extern int32
ET_moz_CallFunctionInt(ETIntPtrFunc fn, void * data);

extern char *
ET_moz_CallFunctionString(ETStringPtrFunc fn, void * data);

extern void
ET_moz_CallAsyncAndSubEventLoop(ETVoidPtrFunc fn, void *data,
				MWContext *context);

extern void
ET_moz_Abort(MKStreamAbortFunc fn, void * data, int status);

extern void
ET_moz_SetMochaWriteStream(MochaDecoder * decoder);

extern NET_StreamClass *
ET_moz_DocCacheConverter(MWContext * context, URL_Struct * pUrl, 
			 char * wysiwyg_url, int32 layer_id);

extern PRBool
ET_moz_VerifyComponentFunction(ETVerifyComponentFunc fn, ETBoolPtrFunc *pActive_callback, 
			       ETVoidPtrFunc *pStartup_callback);

extern void
ET_moz_CompSetterFunction(ETCompPropSetterFunc fn, char *name, void *data);

extern void *
ET_moz_CompGetterFunction(ETCompPropGetterFunc fn, char *name);

extern void *
ET_moz_CompMethodFunction(ETCompMethodFunc fn, int32 argc, JSCompArg *argv);

typedef enum { 
    CL_Move,
    CL_MoveX,
    CL_MoveY,
    CL_Offset,
    CL_Resize,
    CL_SetBboxWidth,
    CL_SetBboxHeight,
    CL_SetBboxTop,
    CL_SetBboxLeft,
    CL_SetBboxBottom,
    CL_SetBboxRight,
    CL_SetHidden,
    CL_MoveInZ,
    CL_SetSrc,
    CL_SetSrcWidth,
    CL_SetZ,
    CL_SetBgColor,
    CL_SetBackdrop
} ETLayerOp;

extern int
ET_TweakLayer(MWContext * context, CL_Layer * layer, int32 x, int32 y, 
              void *param_ptr, int32 param_val, ETLayerOp op,
	      const char *referer, int32 doc_id);

extern void
ET_RestoreLayerState(MWContext *context, int32 layer_id,
                     LO_BlockInitializeStruct *param, ETRestoreAckFunc fn,
                     void *data);

extern int32
ET_npl_RefreshPluginList(MWContext* context, XP_Bool refreshInstances);

extern JSBool
ET_HandlePref(JSContext * cx, uint argc, jsval * argv, jsval * rval);

extern void
ET_SetPluginWindow(MWContext * pContext, void * app);

#ifdef DOM
typedef enum { 
    SP_SetColor,
    SP_SetBackground,
	SP_SetFontWeight,
	SP_SetFontFamily,
    SP_SetFontSize,
    SP_SetFontSlant
} ETSpanOp;

extern int
ET_TweakSpan(MWContext * context, void *name_rec, void *param_ptr,
	int32 param_val, ETSpanOp op, int32 doc_id);

typedef enum { 
    TR_SetHref,
    TR_SetVisibility,
	TR_SetData
} ETTransclusionOp;

extern int
ET_TweakTransclusion(MWContext * context, void *xmlFile, void *param_ptr,
	int32 param_val, ETTransclusionOp op, int32 doc_id);
#endif

NSPR_END_EXTERN_C

#endif /* libevent_h___ */
