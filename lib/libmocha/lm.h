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

#ifndef lm_h___
#define lm_h___
/*
 * JS in the Navigator library-private interface.
 */
#include "xp.h"         /* for uint and PA_Block */
#include "prlong.h"	/* for int64 time type used below */
#include "libevent.h"	/* until its a stand-alone */
#include "libmocha.h"

/*
 * Shared string constants for common property names.
 */
extern char lm_argc_err_str[];          /* "incorrect number of arguments" */

extern char lm_unknown_origin_str[];    /* "[unknown origin]" */

extern char lm_onLoad_str[];            /* "onLoad" */
extern char lm_onUnload_str[];          /* "onUnload" */
extern char lm_onAbort_str[];           /* "onAbort" */
extern char lm_onError_str[];           /* "onError" */
extern char lm_onScroll_str[];          /* "onScroll" */
extern char lm_onFocus_str[];           /* "onFocus" */
extern char lm_onBlur_str[];            /* "onBlur" */
extern char lm_onSelect_str[];          /* "onSelect" */
extern char lm_onChange_str[];          /* "onChange" */
extern char lm_onReset_str[];           /* "onReset" */
extern char lm_onSubmit_str[];          /* "onSubmit" */
extern char lm_onClick_str[];           /* "onClick" */
extern char lm_onMouseDown_str[];       /* "onMouseDown" */
extern char lm_onMouseOver_str[];       /* "onMouseOver" */
extern char lm_onMouseOut_str[];        /* "onMouseOut" */
extern char lm_onMouseUp_str[];         /* "onMouseUp" */
extern char lm_onLocate_str[];          /* "onLocate" */
extern char lm_onHelp_str[];            /* "onHelp" [EA] */

extern char lm_focus_str[];             /* "focus" */
extern char lm_blur_str[];              /* "blur" */
extern char lm_select_str[];            /* "select" */
extern char lm_click_str[];             /* "click" */
extern char lm_scroll_str[];            /* "scroll" */
extern char lm_enable_str[];            /* "enable" */
extern char lm_disable_str[];           /* "disable" */

extern char lm_toString_str[];          /* "toString" */
extern char lm_length_str[];            /* "length" */
extern char lm_document_str[];          /* "document" */
extern char lm_forms_str[];             /* "forms" */
extern char lm_links_str[];             /* "links" */
extern char lm_anchors_str[];           /* "anchors" */
extern char lm_applets_str[];           /* "applets" */
extern char lm_embeds_str[];            /* "embeds" */
extern char lm_plugins_str[];           /* "plugins" */
extern char lm_images_str[];            /* "images" */
extern char lm_layers_str[];            /* "layers" */
#ifdef DOM
extern char lm_spans_str[];				/* "spans" */
extern char lm_transclusions_str[];		/* "transclusions" */
#endif

extern char lm_location_str[];          /* "location" */
extern char lm_navigator_str[];         /* "navigator" */
extern char lm_netcaster_str[];         /* "netcaster" */
extern char lm_components_str[];        /* "components" */

extern char lm_parentLayer_str[];       /* "parentLayer" */
extern char lm_opener_str[];            /* "opener" */
extern char lm_closed_str[];            /* "closed" */
extern char lm_assign_str[];            /* "assign" */
extern char lm_reload_str[];            /* "reload" */
extern char lm_replace_str[];           /* "replace" */
extern char lm_event_str[];             /* "event" */
extern char lm_methodPrefix_str[];      /* "#method" */
extern char lm_methodArgc_str[];      /* "#method" */
extern char lm_methodArgv_str[];      /* "#method" */
extern char lm_getPrefix_str[];         /* "#get_" */
extern char lm_setPrefix_str[];         /* "#set_" */
extern char lm_typePrefix_str[];        /* "#type_" */
extern const char *lm_event_argv[];     /* {lm_event_str} */

extern PRThread		    *lm_InterpretThread;
extern PRThread		    *mozilla_thread;
extern PRThread		    *lm_js_lock_previous_owner;

extern JSContext *lm_writing_context;

/*
 * Timeout structure threaded on MochaDecoder.timeouts for cleanup.
 */
struct JSTimeout {
    int32               ref_count;      /* reference count to shared usage */
    char                *expr;          /* the JS expression to evaluate */
    JSObject            *funobj;        /* or function to call, if !expr */
    jsval               *argv;          /* function actual arguments */
    void                *toid;          /* Identifier, used internally only */
    uint32              public_id;      /* Returned as value of setTimeout() */
    uint16              argc;           /* and argument count */
    uint16              spare;          /* alignment padding */
    int32		doc_id;       	/* document this is for */
    int32               interval;       /* Non-zero if repetitive timeout */
    int64               when;           /* nominal time to run this timeout */
    JSVersion		version;	/* Version of JavaScript to execute */
    JSPrincipals        *principals;    /* principals with which to execute */
    char                *filename;      /* filename of setTimeout call */
    uint32              lineno;         /* line number of setTimeout call */
    JSTimeout           *next;          /* next timeout in list */
};

extern void             lm_ClearWindowTimeouts(MochaDecoder *decoder);

struct JSNestingUrl {
    JSNestingUrl 	*next;
    char                *str;
};

/*
 * Event queue stack madness to handle doc.write("<script>doc.write...").
 */
typedef struct QueueStackElement {
    PREventQueue		* queue;
    MWContext		        * context;
    int32			  doc_id;
    struct QueueStackElement	* up;
    struct QueueStackElement	* down;
    PRPackedBool		  done;
    PRPackedBool		  discarding;
    PRPackedBool		  inherit_parent;
    void                        * retval;
} QueueStackElement;

extern void
et_SubEventLoop(QueueStackElement * qse);

/*
 * Stack size per window context, plus one for the navigator.
 */
#define LM_STACK_SIZE   8192

extern JSRuntime        *lm_runtime;
extern MochaDecoder     *lm_crippled_decoder;
extern JSClass		lm_window_class;
extern JSClass		lm_layer_class;
extern JSClass		lm_document_class;
extern JSClass		lm_event_class;

extern JSBool           lm_SaveParamString(JSContext *cx, PA_Block *bp,
                                           const char *str);
extern MochaDecoder     *lm_NewWindow(MWContext *context);
extern void             lm_DestroyWindow(MochaDecoder *decoder);

/*
 * Hold and drop the reference count for tree back-edges that go from object
 * private data to the containing decoder.  These refs do not keep the object
 * tree under decoder alive from the GC, but they do keep decoder from being
 * destroyed and some out of order finalizer tripping over its freed memory.
 */
#ifdef DEBUG
extern MochaDecoder     *lm_HoldBackCount(MochaDecoder *decoder);
extern void             lm_DropBackCount(MochaDecoder *decoder);

#define HOLD_BACK_COUNT(decoder) lm_HoldBackCount(decoder)
#define DROP_BACK_COUNT(decoder) lm_DropBackCount(decoder)
#else
#define HOLD_BACK_COUNT(decoder)                                             \
    (((decoder) ? (decoder)->back_count++ : 0), (decoder))
#define DROP_BACK_COUNT(decoder)                                             \
    (((decoder) && --(decoder)->back_count <= 0 && !(decoder)->forw_count)   \
     ? lm_DestroyWindow(decoder)					     \
     : (void)0)
#endif

extern JSBool           lm_InitWindowContent(MochaDecoder *decoder);
extern JSBool           lm_DefineWindowProps(JSContext *cx,
                                             MochaDecoder *decoder);
extern JSBool           lm_ResolveWindowProps(JSContext *cx,
                                              MochaDecoder *decoder,
                                              JSObject *obj,
                                              jsval id);
extern void             lm_FreeWindowContent(MochaDecoder *decoder,
					     JSBool fromDiscard);
extern JSBool           lm_SetInputStream(JSContext *cx,
					  MochaDecoder *decoder,
					  NET_StreamClass *stream,
					  URL_Struct *url_struct,
                                          JSBool free_stream_on_close);
extern JSObject         *lm_DefineDocument(MochaDecoder *decoder,
                                           int32 layer_id);
extern JSObject         *lm_DefineHistory(MochaDecoder *decoder);
extern JSObject         *lm_DefineLocation(MochaDecoder *decoder);
extern JSObject         *lm_DefineNavigator(MochaDecoder *decoder);
extern JSObject         *lm_DefineComponents(MochaDecoder *decoder);
extern JSObject         *lm_DefineCrypto(MochaDecoder *decoder);
extern JSObject         *lm_DefineScreen(MochaDecoder *decoder,
                                         JSObject *parent);
extern JSObject         *lm_DefineHardware(MochaDecoder *decoder,
                                         JSObject *parent);
extern JSBool           lm_DefinePluginClasses(MochaDecoder *decoder);
extern JSBool           lm_DefineBarClasses(MochaDecoder *decoder);
extern JSBool           lm_ResolveBar(JSContext *cx, MochaDecoder *decoder,
				      const char *name);
extern JSBool           lm_DefineTriggers(void);
extern JSObject         *lm_NewPluginList(JSContext *cx, JSObject *parent_obj);
extern JSObject         *lm_NewMIMETypeList(JSContext *cx);
extern JSObject         *lm_GetDocumentFromLayerId(MochaDecoder *decoder,
                                                   int32 layer_id);
extern JSObject         *lm_DefinePkcs11(MochaDecoder *decoder);
/*
 * Get (create if needed) document's form, link, and named anchor arrays.
 */
extern JSObject         *lm_GetFormArray(MochaDecoder *decoder,
                                         JSObject *document);
extern JSObject         *lm_GetLinkArray(MochaDecoder *decoder,
                                         JSObject *document);
extern JSObject         *lm_GetNameArray(MochaDecoder *decoder,
                                         JSObject *document);
#ifdef DOM
extern JSObject         *lm_GetSpanArray(MochaDecoder *decoder,
                                         JSObject *document);
extern JSObject         *lm_GetTransclusionArray(MochaDecoder *decoder,
                                         JSObject *document);
#endif
extern JSObject         *lm_GetAppletArray(MochaDecoder *decoder,
                                           JSObject *document);
extern JSObject         *lm_GetEmbedArray(MochaDecoder *decoder,
                                          JSObject *document);
extern JSObject         *lm_GetImageArray(MochaDecoder *decoder,
                                          JSObject *document);
extern JSObject         *lm_GetDocumentLayerArray(MochaDecoder *decoder,
                                                  JSObject *document);


/*
 * dummy object for applets and embeds that can't be reflected
 */
extern JSObject *lm_DummyObject;
extern void lm_InitDummyObject(JSContext *cx);

/* bit vector for handlers */
typedef enum  {
	HANDLER_ONCLICK = 1 << 0,
	HANDLER_ONFOCUS = 1 << 1,
	HANDLER_ONBLUR = 1 << 2,
	HANDLER_ONCHANGE = 1 << 3,
	HANDLER_ONSELECT = 1 << 4,
	HANDLER_ONSCROLL = 1 << 5,
	HANDLER_ONMOUSEDOWN = 1 << 6,
	HANDLER_ONMOUSEUP = 1 << 7,
	HANDLER_ONKEYDOWN = 1 << 8,
	HANDLER_ONKEYUP = 1 << 9,
	HANDLER_ONKEYPRESS = 1 << 10,
	HANDLER_ONDBLCLICK = 1 << 11
} JSHandlersBitVector;

/*
 * Base class of all JS input private object data structures.
 */
typedef struct JSInputBase {
    MochaDecoder        *decoder;       /* this window's JS decoder */
    int32               type;           /* layout form element type */
    JSHandlersBitVector	handlers;	/* bit vector for handlers */
} JSInputBase;

/*
 * Base class of event-handling elements like layers and documents.
 */
typedef struct JSInputHandler {
    JSInputBase     base;           /* decoder and type */
    JSObject        *object;        /* this input handler's JS object */
    uint32          event_mask;     /* mask of events in progress */
} JSInputHandler;

/*
 * Base class of input event-capturing elements like layers and documents.
 */
typedef struct JSEventReceiver {
    JSObject        *object;        /* this event receiver's JS object */
    uint32          event_mask;     /* mask of events in progress */
} JSEventReceiver;

/*
 * Base class of input event-handling elements like anchors and form inputs.
 */
typedef struct JSEventCapturer {
    JSEventReceiver base;	    /* this event capturer's receiver base */
    uint32          event_bit;	    /* mask of events being captured */
} JSEventCapturer;

#define base_decoder    base.decoder
#define base_type       base.type
#define base_handlers	base.handlers

/*
 * JS URL object.
 *
 * Location is a special URL: when you set one of its properties, your client
 * window goes to the newly formed address.
 */
typedef struct JSURL {
    JSInputHandler  handler;
    int32           layer_id;
    uint32          index;
    JSString        *href;
    JSString        *target;
    JSString        *text;
} JSURL;

/* JS Document Object
 * Documents exist per-window and per-layer
 */
typedef struct JSDocument {
    JSEventCapturer	capturer;
    MochaDecoder        *decoder;
    JSObject            *object;
    int32               layer_id;     /* The containing layer's id */
    JSObject	        *forms;
    JSObject	        *links;
    JSObject	        *anchors;
    JSObject	        *applets;
    JSObject            *embeds;
    JSObject	        *images;
    JSObject	        *layers;
#ifdef DOM
	JSObject	        *spans;
	JSObject	        *transclusions;
#endif
} JSDocument;

#define URL_NOT_INDEXED ((uint32)-1)

#define url_decoder     handler.base_decoder
#define url_type        handler.base_type
#define url_object      handler.object
#define url_event_mask  handler.event_mask

extern JSURL *
lm_NewURL(JSContext *cx, MochaDecoder *decoder,
          LO_AnchorData *anchor_data, JSObject *document);

extern void
lm_ReplaceURL(MWContext *context, URL_Struct *url_struct);

extern JSBool
lm_GetURL(JSContext *cx, MochaDecoder *decoder, URL_Struct *url_struct);

extern const char *
lm_CheckURL(JSContext *cx, const char *url_string, JSBool checkFile);

extern JSBool
lm_CheckWindowName(JSContext *cx, const char *window_name);

extern PRHashTable *
lm_GetIdToObjectMap(MochaDecoder *decoder);

extern JSBool PR_CALLBACK
lm_BranchCallback(JSContext *cx, JSScript *script);

extern void PR_CALLBACK
lm_ErrorReporter(JSContext *cx, const char *message,
                 JSErrorReport *report);

extern JSObject *
lm_GetFormObjectByID(MWContext *context, int32 layer_id, uint form_id);

extern LO_FormElementStruct *
lm_GetFormElementByIndex(JSContext * cx, JSObject *form_obj, int32 index);

extern JSObject *
lm_GetFormElementFromMapping(JSContext *cx, JSObject *form_obj, uint32 index);

extern JSBool
lm_AddFormElement(JSContext *cx, JSObject *form, JSObject *obj,
                  char *name, uint index);

extern JSBool
lm_ReflectRadioButtonArray(MWContext *context, int32 layer_id, intn form_id,
                           const char *name, PA_Tag * tag);

extern JSBool
lm_SendEvent(MWContext *context, JSObject *obj, JSEvent *event,
             jsval *result);

extern JSBool
lm_HandleEvent(JSContext *cx, JSObject *obj, JSObject *eventObj,
	       jsval funval, jsval *result);

extern JSBool
lm_FindEventHandler(MWContext *context, JSObject *obj, JSObject *eventObj,
		     jsval funval, jsval *result);

extern JSObject *
lm_NewEventObject(MochaDecoder * decoder, JSEvent * pEvent);

typedef struct JSEventNames {
    const char *lowerName;
    const char *mixedName;
} JSEventNames;

extern const char *
lm_EventName(uint32 event_bit);

extern JSEventNames *
lm_GetEventNames(uint32 event_bit);

/*
 * Compile the given attribute and attach it to the JSObject
 */
extern JSBool
lm_CompileEventHandler(MochaDecoder * decoder, PA_Block id, PA_Block data, 
		       int newline_count, JSObject *object, const char *name, 
		       PA_Block block);

extern uint32
lm_FindEventInMWContext(MWContext *context);

/*
 * Remember the current form object that we are processing
 */
extern void
lm_SetActiveForm(MWContext * context, int32 id);

/*
 * Called when we want to get rid of the old contents of a layer
 * and create a new document.
 */
extern void
lm_NewLayerDocument(MochaDecoder *decoder, int32 layer_id);

/*
 * Called to set the source URL as a result of a document.open() on the
 * layer's document.
 */
JSBool
lm_SetLayerSourceURL(MochaDecoder *decoder, int32 layer_id, char *url);

/*
 * Get a layer obj from a parent id and a layer name. Used for lazy reflection.
 */
extern JSObject *
lm_GetNamedLayer(MochaDecoder *decoder, int32 parent_layer_id, 
                 const char *name);

extern const char *
lm_GetLayerOriginURL(JSContext *cx, JSObject *obj);


/* Clears out object references from the doc private data */
extern void
lm_CleanUpDocumentRoots(MochaDecoder *decoder, JSObject *obj);

/* Calls CleanUpDocumentRoots for a layer's document */
extern void
lm_DestroyLayer(MWContext *context, JSObject *obj);

/* 
 * Called when the content associated with a document is destroyed,
 * but the document itself may not be. Cleans out object references
 * from doc private data (so that the objects can be collected). Also
 * deals with correctly relinquishing event capture.
 */
extern void
lm_CleanUpDocument(MochaDecoder *decoder, JSObject *document);

extern NET_StreamClass *
lm_ClearDecoderStream(MochaDecoder *decoder, JSBool fromDiscard);

extern void
lm_ProcessImageEvent(MWContext *context, JSObject *obj,
                     LM_ImageEvent event);

extern JSObject *
lm_NewImage(JSContext *cx, LO_ImageStruct *image_data);

extern JSObject *
lm_NewSoftupObject(JSContext *cx, MochaDecoder *decoder, const char *jarargs);

extern void
lm_RegisterComponent(const char *targetName, ETBoolPtrFunc active_callback, 
		     ETVoidPtrFunc startup_callback);

extern void
lm_RegisterComponentProp(const char *comp, const char *targetName, 
			 uint8 retType, ETCompPropSetterFunc setter, 
			 ETCompPropGetterFunc getter);

extern void
lm_RegisterComponentMethod(const char *comp, const char *targetName, 
			   uint8 retType, ETCompMethodFunc method, int32 nargs);

/*
 * Class initializers (the wave of the future).
 */
extern JSBool
lm_InitDocumentClass(MochaDecoder *decoder);

extern JSBool
lm_InitImageClass(MochaDecoder *decoder);

extern JSBool
lm_InitAnchorClass(MochaDecoder *decoder);

#ifdef DOM
extern JSBool
lm_InitSpanClass(MochaDecoder *decoder);

extern JSBool
lm_InitTransclusionClass(MochaDecoder *decoder);
#endif

extern JSBool
lm_InitLayerClass(MochaDecoder *decoder);

extern JSBool
lm_InitInputClasses(MochaDecoder *decoder);

extern JSBool
lm_InitEventClasses(MochaDecoder *decoder);

extern JSBool
lm_InitRectClass(MochaDecoder *decoder);

/* Create an image context for anonymous images. */
extern JSBool
lm_NewImageContext(MWContext *context, MochaDecoder *decoder);

/* 
 * Called to save and restore timeouts across a resize_reload.
 */
void
lm_SaveWindowTimeouts(MochaDecoder *decoder);

void
lm_RestoreWindowTimeouts(MochaDecoder *decoder);

/*
 * Call this function from LO_ResetForm() to let the form's onReset attribute
 * event handler run and do specialized form element data reinitialization.
 * Returns JS_TRUE if the form reset should continue, JS_FALSE if it
 * should be aborted.
 */
extern JSBool
lm_SendOnReset(MWContext *context, JSEvent *event, LO_Element *element);

/*
 * See if our outer object is the current form or the document
 */
extern JSObject *
lm_GetOuterObject(MochaDecoder * decoder);

/*
 * Entry point for the netlib to notify JS of load and unload events.
 */
extern void
lm_SendLoadEvent(MWContext *context, int32 event, JSBool resize_reload);

/*
 * Load or unload event for a layer
 */
extern void
lm_SendLayerLoadEvent(MWContext *context, int32 event, int32 layer_id, JSBool resize_reload);

/*
 * This one should be called when a form is submitted.  If there is an
 * INPUT tag of type "submit", the JS author could write an "onClick"
 * handler property of the submit button, but for forms that auto-submit
 * we want an "onSubmit" form property handler.
 *
 * Returns JS_TRUE if the submit should continue, JS_FALSE if it
 * should be aborted.
 */
extern JSBool
lm_SendOnSubmit(MWContext *context, JSEvent *event, LO_Element *element);

extern JSBool
lm_InputEvent(MWContext *context, LO_Element *element, JSEvent *pEvent,
            jsval *rval);

extern JSBool
lm_KeyInputEvent(MWContext *context, LO_Element *element, JSEvent *pEvent,
            jsval *rval);

extern JSBool
lm_MouseInputEvent(MWContext *context, LO_Element *element, JSEvent *pEvent,
            jsval *rval);

extern NET_StreamClass *
lm_DocCacheConverter(MWContext * context, URL_Struct * url,
		     const char * wysiwyg_url);

extern char *
lm_MakeWysiwygUrl(JSContext *cx, MochaDecoder *decoder, int32 layer_id,
                  JSPrincipals *principals);

typedef struct JSObjectArray {
    MochaDecoder        *decoder;
    jsint		length;
    int32               layer_id;
} JSObjectArray;

extern JSBool
lm_AddObjectToArray(JSContext * cx, JSObject * array_obj,
		     const char * name, jsint index, JSObject * obj);

/* Version Information; sits on top of JS_SetVersion and JS_GetVersion */
extern void
lm_SetVersion(MochaDecoder *decoder, JSVersion version);

#define lm_GetVersion(decoder) JS_GetVersion((decoder)->js_context)

/* 
 * Security -----------------------------------
 */

typedef enum JSTarget {
    JSTARGET_UNIVERSAL_BROWSER_READ,
    JSTARGET_UNIVERSAL_BROWSER_WRITE,
    JSTARGET_UNIVERSAL_SEND_MAIL,
    JSTARGET_UNIVERSAL_FILE_READ,
    JSTARGET_UNIVERSAL_FILE_WRITE,
    JSTARGET_UNIVERSAL_PREFERENCES_READ,
    JSTARGET_UNIVERSAL_PREFERENCES_WRITE,
    JSTARGET_MAX
} JSTarget;

extern const char *
lm_GetSubjectOriginURL(JSContext *cx);

extern const char *
lm_GetObjectOriginURL(JSContext *cx, JSObject *object);

extern const char *
lm_SetObjectOriginURL(JSContext *cx, JSObject *object, const char *origin);

extern JSPrincipals *
lm_GetPrincipalsFromStackFrame(JSContext *cx);

extern JSPrincipals *
lm_GetCompilationPrincipals(MochaDecoder *decoder, 
                            JSPrincipals *layoutPrincipals);

extern JSBool
lm_CanAccessTarget(JSContext *cx, JSTarget target);

extern JSBool
lm_CheckPermissions(JSContext *cx, JSObject *obj, JSTarget target);

extern JSBool
lm_GetCrossOriginEnabled(void);

extern JSBool
lm_CheckContainerAccess(JSContext *cx, JSObject *obj, MochaDecoder *decoder,
                        JSTarget target);

/* 
 * Get the innermost (in the sense of the parent scope chain of "obj") 
 * nonnull principals. Will create principals at the window level if 
 * none exist. Will always return nonnull principals except in the case 
 * of memory failure. 
 * Argument "container" specifies the start of the search, the 
 * container where the principals were found will be stored into 
 * "foundIn" if "foundIn" is nonnull. 
 */
extern JSPrincipals *
lm_GetInnermostPrincipals(JSContext *cx, JSObject *container, 
                          JSObject **foundIn);

extern JSPrincipals *
lm_GetContainerPrincipals(JSContext *cx, JSObject *container);

extern void
lm_SetContainerPrincipals(JSContext *cx, JSObject *container, 
                          JSPrincipals *principals);

extern JSBool 
lm_CanCaptureEvent(JSContext *cx, JSFunction *fun, JSEvent *event);

extern void
lm_SetExternalCapture(JSContext *cx, JSPrincipals *principals, 
                      JSBool b);

extern JSBool
lm_CheckSetParentSlot(JSContext *cx, JSObject *obj, jsval id, jsval *vp);

extern JSBool
lm_SetDocumentDomain(JSContext *cx, JSPrincipals *principals, 
                     const char *newDomain);

extern void
lm_InvalidateCertPrincipals(MochaDecoder *decoder, JSPrincipals *principals);

extern void
lm_DestroyPrincipalsList(JSContext *cx, JSPrincipalsList *list);

/* 
 * --------------------------------------------
 */

/* For when mozilla needs to call LM_PutMochaDecoder in the mocha thread. */
extern void
et_PutMochaDecoder(MWContext *context, MochaDecoder *decoder);

/* This function is private to libmocha, so don't stuff it in libevent.h, chouck ! */
extern int32
ET_PostCreateLayer(MWContext *context, int32 wrap_width, int32 parent_layer_id);

extern void
lm_RestoreLayerState(MWContext *context, int32 layer_id, 
                     LO_BlockInitializeStruct *param);

extern PRHashNumber 
lm_KeyHash(const void *key);

extern JSBool
lm_jsval_to_rgb(JSContext *cx, jsval *vp, LO_Color **rgbp);

extern JSBool
layer_setBgColorProperty(JSContext *cx, JSObject *obj, jsval *vp);

extern JSObject *
lm_GetActiveContainer(MochaDecoder *decoder);

extern JSBool
lm_GetPrincipalsCompromise(JSContext *cx, JSObject *obj);

extern char *
lm_FixNewlines(JSContext *cx, const char *value, JSBool formElement);

extern JSBool PR_CALLBACK
win_open(JSContext *cx, JSObject *obj, uint argc, jsval *argv, jsval *rval);


/* defined in libmocha.h */
#ifdef JSDEBUGGER   

extern void
lm_InitJSDebug(JSRuntime *jsruntime);

extern void
lm_ExitJSDebug(JSRuntime *jsruntime);

#endif

#define IS_MESSAGE_WINDOW(context)                                           \
(((context)->type == MWContextMail)      ||                                  \
 ((context)->type == MWContextNews)      ||                                  \
 ((context)->type == MWContextMailMsg)   ||                                  \
 ((context)->type == MWContextNewsMsg))

/* INTL support */

extern char *
lm_StrToLocalEncoding(MWContext * context, JSString * str);

extern JSString *
lm_LocalEncodingToStr(MWContext * context, char * bytes);

/* end INTL support */

#endif /* lm_h___ */
