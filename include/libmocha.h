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
 * Header file for Mocha in the Navigator (libmocha).
 */

#ifndef libmocha_h___
#define libmocha_h___

#include "ntypes.h"
#include "il_types.h"
#include "prtypes.h"
#include "plhash.h"
#include "prthread.h"
#include "jsapi.h"

/* enable JavaScript Debugger support */
#if defined (_WIN32) || defined(XP_UNIX) || defined(powerc) || defined(__powerc) || defined(XP_OS2)
#ifdef JAVA
#define JSDEBUGGER 1 
#endif
#endif

NSPR_BEGIN_EXTERN_C

typedef struct JSTimeout JSTimeout;
typedef struct JSPrincipalsList JSPrincipalsList;
typedef struct JSNestingUrl JSNestingUrl;

/*
 * There exists one MochaDecoder per top-level MWContext that decodes Mocha,
 * either from an HTML page or from a "mocha:[expr]" URL.
 */
typedef struct MochaDecoder {
    int32           forw_count;		    /* forward reference count */
    int32           back_count;		    /* back (up the tree) count */
    JSContext	    *js_context;
    MWContext       *window_context;
    JSObject	    *window_object;
    NET_StreamClass *stream;
    int32           stream_owner;   /* id of layer that's loading the stream */
    URL_Struct      *url_struct;
    JSTimeout	    *timeouts;
    JSTimeout       *saved_timeouts;
    uint16          signature_ordinal;
    PRPackedBool    replace_location;
    PRPackedBool    resize_reload;
    PRPackedBool    load_event_sent;
    PRPackedBool    visited;
    PRPackedBool    writing_input;
    PRPackedBool    free_stream_on_close;
    PRPackedBool    in_window_quota;
    PRPackedBool    called_win_close;
    PRPackedBool    principals_compromised;
    const char      *source_url;
    JSNestingUrl    *nesting_url;
    uint32          error_count;
    uint32          event_mask;
    int32           active_layer_id;
    uint32          active_form_id;
    uint32	    event_bit;
    int32	    doc_id;

    /*
     * Class prototype objects, in alphabetical order.  Must be CLEARed (set
     * to null) in LM_PutMochaDecoder, HELD (GC roots added) in lm_NewWindow,
     * and DROPped (removed as GC roots) in lm_DestroyWindow.
     * XXXbe clean up, clear via bzero, using a sub-structure.
     */
    JSObject	    *anchor_prototype;
    JSObject	    *bar_prototype;
    JSObject        *document_prototype;
    JSObject	    *event_prototype;
    JSObject	    *event_capturer_prototype;
    JSObject	    *event_receiver_prototype;
    JSObject	    *form_prototype;
    JSObject	    *image_prototype;
    JSObject	    *input_prototype;
    JSObject	    *layer_prototype;
    JSObject	    *option_prototype;
    JSObject	    *rect_prototype;
    JSObject	    *url_prototype;

    /*
     * Window sub-objects.  These must also follow the CLEAR/HOLD/DROP
     * protocol mentioned above.
     */
    JSObject	    *document;
    JSObject	    *history;
    JSObject	    *location;
    JSObject	    *navigator;
    JSObject	    *components;
    JSObject	    *screen;
    JSObject	    *hardware;
    JSObject	    *crypto;
    JSObject        *pkcs11;

    /*
     * Ad-hoc GC roots.
     */
    JSObject	    *event_receiver;
    JSObject	    *opener;

    JSVersion	    firstVersion;   /* First JS script tag version. */

    /*
     * Security info for all of this decoder's scripts, except those
     * contained in layers.
     */
    JSPrincipals    *principals;
    JSPrincipalsList*early_access_list;

    IL_GroupContext *image_context; /* Image context for anonymous images */

    /* 
     * Table that maintains an id to JS object mapping for reflected
     * elements. This is used during resize to reestablish the connection
     * between layout elements and their corresponding JS object. 
     * Form elements are special, since they can't use the same keying
     */
    PRHashTable     *id_to_object_map;
} MochaDecoder;

/* 
 * Number of buckets for the id-to-object hash table.
 */
#define LM_ID_TO_OBJ_MAP_SIZE 20
#define LM_FORM_ELEMENT_MAP_SIZE 10

/*
 * Types of objects reflected into Mocha
 */
typedef enum {
	LM_APPLETS = 0,
	LM_FORMS,
	LM_LINKS,
	LM_NAMEDANCHORS,
	LM_EMBEDS,
	LM_IMAGES,
	LM_FORMELEMENTS,
	LM_LAYERS
} ReflectedObject;

/*
 * Generates an id-to-object mapping key from the ReflectedObject
 * type, the containing layer id and the id of the object itself.
 * The key is 4 bits type, 14 bits layer_id and 14 bits id.
 */
#define LM_GET_MAPPING_KEY(obj_type, layer_id, id)    \
     (void *)(((((uint32)obj_type) << 28) & 0xF0000000UL) |     \
              ((((uint32)layer_id) << 14) & 0x0FFFC000UL) |     \
              (((uint32)id) & 0x00003FFFUL))

/*
 * Public, well-known string constants.
 */
extern char js_language_name[];      /* "JavaScript" */
extern char js_content_type[];       /* "application/x-javascript" */

/*
 * Initialize and finalize Mocha-in-the-client.
 */
extern void LM_InitMocha(void);
extern void LM_FinishMocha(void);

/*
 * Force mocha on in the given context, even if the user pref is set to
 * disable mocha.
 */
extern void LM_ForceJSEnabled(MWContext *cx);

/*
 * Initialize and finalize Mocha-Java connection
 */
#define LM_MOJA_UNINITIALIZED   0
#define LM_MOJA_OK              1
#define LM_MOJA_JAVA_FAILED     2
#define LM_MOJA_OUT_OF_MEMORY   3
extern int LM_InitMoja(void);
extern void LM_FinishMoja(void);
extern int LM_IsMojaInitialized(void);

/*
 * Enter or leave the big mocha lock.  Any thread which wants to
 * preserve JavaScript run-to-completion semantics must bracket
 * JavaScript evaluation with these calls.
 */
typedef void
(PR_CALLBACK *JSLockReleaseFunc)(void * data);


extern void PR_CALLBACK LM_LockJS(void);
extern void PR_CALLBACK LM_UnlockJS(void);
extern JSBool PR_CALLBACK LM_AttemptLockJS(JSLockReleaseFunc fn, void * data);
extern JSBool PR_CALLBACK LM_ClearAttemptLockJS(JSLockReleaseFunc fn, void * data);
extern PRBool PR_CALLBACK
LM_HandOffJSLock(PRThread * oldOwner, PRThread *newOwner);

/*
 * For interruption purposes we will sometimes need to know the
 *   context who is holding the JS lock
 */
extern void LM_JSLockSetContext(MWContext * context);
extern MWContext * LM_JSLockGetContext(void);

/*
 * Enable/disable for Mocha-in-the-client.
 */
#define LM_SwitchMocha(toggle)	LM_SetMochaEnabled(toggle)

extern JSBool
LM_GetMochaEnabled(void);

/*
 * Get (create if necessary) a MochaDecoder for context, adding a reference
 * to its window_object.  Put drops the reference, destroying window_object
 * when the count reaches zero.  These functions should only be called in
 * the mocha thread or while holding the JS-lock
 */
extern MochaDecoder *
LM_GetMochaDecoder(MWContext *context);

extern void
LM_PutMochaDecoder(MochaDecoder *decoder);

/*
 * Get the source URL for script being loaded by document.  This URL will be
 * the document's URL for inline script, or the SRC= URL for included script.
 * The returned pointer is safe only within the extent of the function that
 * calls LM_GetSourceURL().
 */
extern const char *
LM_GetSourceURL(MochaDecoder *decoder);

/*
 * Set the current layer and hence the current scope for script evaluation.
 */
extern void
LM_SetActiveLayer(MWContext * context, int32 layer_id);

/*
 * Get the current layer and hence the current scope for script evaluation.
 */
extern int32
LM_GetActiveLayer(MWContext * context);

/*
 * Evaluate the contents of a SCRIPT tag. You can specify the JSObject
 * to use as the base scope. Pass NULL to use the default window_object
 */
extern JSBool
LM_EvaluateBuffer(MochaDecoder *decoder, void *base, size_t length,
		  uint lineno, char * scope_to, struct JSPrincipals *principals,
		  JSBool unicode, jsval *result);

/*
 * Evaluate an expression entity in an HTML attribute (WIDTH="&{height/2};").
 * Returns null on error, otherwise a pointer to the malloc'd string result.
 * The caller is responsible for freeing the string result.
 */
extern char *
LM_EvaluateAttribute(MWContext *context, char *expr, uint lineno);

/*
 * Remove any MochaDecoder window_context pointer to an MWContext that's
 * being destroyed.
 */
extern void
LM_RemoveWindowContext(MWContext *context, History_entry * he);

extern void
LM_DropSavedWindow(MWContext *context, void *window);

/*
 * Set and clear the HTML stream and URL for the MochaDecoder
 *   associated with the given context
 */
extern JSBool
LM_SetDecoderStream(MWContext * context, NET_StreamClass *stream,
		            URL_Struct *url_struct, JSBool free_stream_on_close);

/*
 * Start caching HTML or plain text generated by document.write() where the
 * script is running on mc, the document is being generated into decoder's
 * window, and url_struct tells about the generator.
 */
extern NET_StreamClass *
LM_WysiwygCacheConverter(MWContext *context, URL_Struct *url_struct,
			 const char * wysiwyg_url, const char * base_href);

/*
 * Skip over the "wysiwyg://docid/" in url_string and return a pointer to the
 * real URL hidden after the prefix.  If url_string is not of "wysiwyg:" type,
 * just return url_string.  Never returns null.
 */
extern const char *
LM_StripWysiwygURLPrefix(const char *url_string);

/*
 * This function works only on "wysiwyg:" type URLs -- don't call it unless
 * you know that NET_URL_Type(url_string) is WYSIWYG_TYPE_URL.  It'll return
 * null if url_string seems too short, or if it can't find the third slash.
 */
extern const char *
LM_SkipWysiwygURLPrefix(const char *url_string);

/*
 * Return a pointer to a malloc'd string of the form "<BASE HREF=...>" where
 * the "..." URL is the directory of cx's origin URL.  Such a base URL is the
 * default base for relative URLs in generated HTML.
 */
extern char *
LM_GetBaseHrefTag(JSContext *cx, JSPrincipals *principals);

/*
 * XXX Make these public LO_... typedefs in lo_ele.h/ntypes.h?
 */
struct lo_FormData_struct;
struct lo_NameList_struct;

extern struct lo_FormData_struct *
LO_GetFormDataByID(MWContext *context, int32 layer_id, intn form_id);

extern uint
LO_EnumerateForms(MWContext *context, int32 layer_id);

extern struct LO_ImageStruct_struct *
LO_GetImageByIndex(MWContext *context, int32 layer_id, intn image_id);

extern uint
LO_EnumerateImages(MWContext *context, int32 layer_id);

/*
 * Reflect display layers into Mocha.
 */
extern JSObject *
LM_ReflectLayer(MWContext *context, int32 layer_id, int32 parent_layer_id,
                PA_Tag *tag);

extern LO_FormElementStruct *
LO_GetFormElementByIndex(struct lo_FormData_struct *form_data, int32 index);

extern uint
LO_EnumerateFormElements(MWContext *context,
			 struct lo_FormData_struct *form_data);

/*
 * Layout helper function to find a named anchor by its index in the
 * document.anchors[] array.
 */
extern struct lo_NameList_struct *
LO_GetNamedAnchorByIndex(MWContext *context, int32 layer_id, uint index);

extern uint
LO_EnumerateNamedAnchors(MWContext *context, int32 layer_id);

/*
 * Layout Mocha helper function to find an HREF Anchor by its index in the
 * document.links[] array.
 */
extern LO_AnchorData *
LO_GetLinkByIndex(MWContext *context, int32 layer_id, uint index);

extern uint
LO_EnumerateLinks(MWContext *context, int32 layer_id);

extern LO_JavaAppStruct *
LO_GetAppletByIndex(MWContext *context, int32 layer_id, uint index);

extern uint
LO_EnumerateApplets(MWContext *context, int32 layer_id);

extern LO_EmbedStruct *
LO_GetEmbedByIndex(MWContext *context, int32 layer_id, uint index);

extern uint
LO_EnumerateEmbeds(MWContext *context, int32 layer_id);

/*
 * Get and set a color attribute in the current document state.
 */
extern void
LO_GetDocumentColor(MWContext *context, int type, LO_Color *color);

extern void
LO_SetDocumentColor(MWContext *context, int type, LO_Color *color);

/*
 * Layout function to reallocate the lo_FormElementOptionData array pointed at
 * by lo_FormElementSelectData's options member to include space for the number
 * of options given by selectData->option_cnt.
 */
extern XP_Bool
LO_ResizeSelectOptions(lo_FormElementSelectData *selectData);

/*
 * Discard the current document and all its subsidiary objects.
 */
extern void
LM_ReleaseDocument(MWContext *context, JSBool resize_reload);

/*
 * Search if a the event is being captured in the frame hierarchy.
 */
extern XP_Bool
LM_EventCaptureCheck(MWContext *context, uint32 current_event);

/*
 * Scroll a window to the given point.
 */
extern void LM_SendOnScroll(MWContext *context, int32 x, int32 y);

/*
 * Display a help topic.
 */
extern void LM_SendOnHelp(MWContext *context);

/*
 * Send a load or abort event for an image to a callback.
 */
typedef enum LM_ImageEvent {
    LM_IMGUNBLOCK   = 0,
    LM_IMGLOAD      = 1,
    LM_IMGABORT     = 2,
    LM_IMGERROR     = 3,
    LM_LASTEVENT    = 3
} LM_ImageEvent;

extern void
LM_ProcessImageEvent(MWContext *context, LO_ImageStruct *image_data,
		  LM_ImageEvent event);

/* This should be called when a named anchor is located. */
extern JSBool
LM_SendOnLocate(MWContext *context, struct lo_NameList_struct *name_rec);

extern JSObject *
LM_ReflectApplet(MWContext *context, LO_JavaAppStruct *applet_data,
                 PA_Tag * tag, int32 layer_id, uint index);

extern JSObject *
LM_ReflectEmbed(MWContext *context, LO_EmbedStruct *lo_embed,
                PA_Tag * tag, int32 layer_id, uint index);

struct lo_FormData_struct;
struct lo_NameList_struct;

extern JSObject *
LM_ReflectForm(MWContext *context, struct lo_FormData_struct *form_data, 
	       PA_Tag * tag, int32 layer_id, uint index);

extern JSObject *
LM_ReflectFormElement(MWContext *context, int32 layer_id, int32 form_id, 
                      int32 element_id, PA_Tag * tag);

extern JSObject *
LM_ReflectLink(MWContext *context, LO_AnchorData *anchor_data, PA_Tag * tag, 
               int32 layer_id, uint index);

extern JSObject *
LM_ReflectNamedAnchor(MWContext *context, struct lo_NameList_struct *name_rec, 
                      PA_Tag * tag, int32 layer_id, uint index);

extern JSObject *
LM_ReflectImage(MWContext *context, LO_ImageStruct *image_data, 
			    PA_Tag * tag, int32 layer_id, uint index);

extern JSBool
LM_CanDoJS(MWContext *context);

extern JSBool
LM_IsActive(MWContext *context);

/*
 * Security.
 */

extern JSPrincipals *
LM_NewJSPrincipals(URL_Struct *archive, char *name, const char *codebase);

extern char *
LM_ExtractFromPrincipalsArchive(JSPrincipals *principals, char *name, 
                                uint *length);

extern JSBool
LM_SetUntransformedSource(JSPrincipals *principals, char *original, 
                          char *transformed);

extern JSPrincipals * PR_CALLBACK
LM_GetJSPrincipalsFromJavaCaller(JSContext *cx, int callerDepth);

/*
 * LM_RegisterPrincipals will verify and register a set of principals 
 * in the decoder, modifying decoder->principals in the process. It
 * returns the modified decoder.
 * 
 * The "name" parameter may be NULL if "principals" was created with a name.
 */

extern JSPrincipals *
LM_RegisterPrincipals(MochaDecoder *decoder, JSPrincipals *principals, 
                      char *name, char *src);
/*
 * JavaScript Debugger support
 */
#ifdef JSDEBUGGER 

extern NET_StreamClass*
LM_StreamBuilder( int         format_out,
                  void        *data_obj,
                  URL_Struct  *URL_s,
                  MWContext   *mwcontext );

extern JSBool
LM_GetJSDebugActive(void);

extern void
LM_JamSourceIntoJSDebug( const char *filename,
                         const char *str, 
                         int32      len,
                         MWContext  *mwcontext );

#endif

NSPR_END_EXTERN_C

#endif /* libmocha_h___ */
