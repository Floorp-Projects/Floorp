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
/*
 * JS reflection of the current Navigator Document.
 *
 * Brendan Eich, 9/8/95
 */
#include "lm.h"
#include "prtypes.h"
#include "plhash.h"
#include "prmem.h"
#include "prtime.h"
#include "xp.h"
#include "lo_ele.h"
#include "shist.h"
#include "cookies.h"
#include "structs.h"            /* for MWContext */
#include "layout.h"             /* included via -I../layout */
#include "pa_parse.h"           /* included via -I../libparse */
#include "pa_tags.h"            /* included via -I../libparse */
#include "secnav.h"
#include "libstyle.h"
#include "prthread.h"
#include "np.h"
#ifdef JAVA
#include "jsjava.h"
#endif

#ifndef DOM
enum doc_slot {
    DOC_LENGTH          = -1,
    DOC_ELEMENTS        = -2,
    DOC_FORMS           = -3,
    DOC_LINKS           = -4,
    DOC_ANCHORS         = -5,
    DOC_APPLETS         = -6,
    DOC_EMBEDS          = -7,
    DOC_TITLE           = -8,
    DOC_URL             = -9,
    DOC_REFERRER        = -10,
    DOC_LAST_MODIFIED   = -11,
    DOC_COOKIE          = -12,
    DOC_DOMAIN          = -13,
    /* slots below this line are not secured */
    DOC_IMAGES          = -14,
    DOC_LAYERS          = -15,
    DOC_LOADED_DATE     = -16,
    DOC_BG_COLOR        = -17,
    DOC_FG_COLOR        = -18,
    DOC_LINK_COLOR      = -19,
    DOC_VLINK_COLOR     = -20,
    DOC_ALINK_COLOR     = -21,
    DOC_WIDTH           = -22,
    DOC_HEIGHT          = -23
};
#else
enum doc_slot {
    DOC_LENGTH          = -1,
    DOC_ELEMENTS        = -2,
    DOC_FORMS           = -3,
    DOC_LINKS           = -4,
    DOC_ANCHORS         = -5,
    DOC_APPLETS         = -6,
    DOC_EMBEDS          = -7,
	DOC_SPANS			= -8,	/* Added for HTML SPAN DOM stuff */
	DOC_TRANSCLUSIONS   = -9,	/* Added for XML Transclusion DOM stuff */
    DOC_TITLE           = -10,
    DOC_URL             = -11,
    DOC_REFERRER        = -12,
    DOC_LAST_MODIFIED   = -13,
    DOC_COOKIE          = -14,
    DOC_DOMAIN          = -15,
    /* slots below this line are not secured */
    DOC_IMAGES          = -16,
    DOC_LAYERS          = -17,
    DOC_LOADED_DATE     = -18,
    DOC_BG_COLOR        = -19,
    DOC_FG_COLOR        = -20,
    DOC_LINK_COLOR      = -21,
    DOC_VLINK_COLOR     = -22,
    DOC_ALINK_COLOR     = -23,
    DOC_WIDTH           = -24,
    DOC_HEIGHT          = -25
};
	
#endif
#define IS_SECURE_DOC_SLOT(s) (DOC_DOMAIN <= (s) && (s) <= DOC_LENGTH) 

static JSPropertySpec doc_props[] = {
    {lm_length_str,  DOC_LENGTH,        JSPROP_READONLY},
    {"elements",     DOC_ELEMENTS,      JSPROP_READONLY},
    {lm_forms_str,   DOC_FORMS,         JSPROP_ENUMERATE|JSPROP_READONLY},
    {lm_links_str,   DOC_LINKS,         JSPROP_ENUMERATE|JSPROP_READONLY},
    {lm_anchors_str, DOC_ANCHORS,       JSPROP_ENUMERATE|JSPROP_READONLY},
    {lm_applets_str, DOC_APPLETS,       JSPROP_ENUMERATE|JSPROP_READONLY},
    {lm_embeds_str,  DOC_EMBEDS,        JSPROP_ENUMERATE|JSPROP_READONLY},
    {lm_plugins_str, DOC_EMBEDS,        JSPROP_READONLY},
    {lm_images_str,  DOC_IMAGES,        JSPROP_ENUMERATE|JSPROP_READONLY},
    {lm_layers_str,  DOC_LAYERS,        JSPROP_ENUMERATE|JSPROP_READONLY},
#ifdef DOM
	{lm_spans_str,   DOC_SPANS,         JSPROP_ENUMERATE|JSPROP_READONLY},
	{lm_transclusions_str,   DOC_TRANSCLUSIONS,         JSPROP_ENUMERATE|JSPROP_READONLY},
#endif
    {"title",        DOC_TITLE,         JSPROP_ENUMERATE|JSPROP_READONLY},
    {"URL",          DOC_URL,           JSPROP_ENUMERATE|JSPROP_READONLY},
    {"referrer",     DOC_REFERRER,      JSPROP_ENUMERATE|JSPROP_READONLY},
    {"lastModified", DOC_LAST_MODIFIED, JSPROP_ENUMERATE|JSPROP_READONLY},
    {"loadedDate",   DOC_LOADED_DATE,   JSPROP_READONLY},
    {"cookie",       DOC_COOKIE,        JSPROP_ENUMERATE},
    {"domain",       DOC_DOMAIN,        JSPROP_ENUMERATE},
    {"bgColor",      DOC_BG_COLOR,      JSPROP_ENUMERATE},
    {"fgColor",      DOC_FG_COLOR,      JSPROP_ENUMERATE},
    {"linkColor",    DOC_LINK_COLOR,    JSPROP_ENUMERATE},
    {"vlinkColor",   DOC_VLINK_COLOR,   JSPROP_ENUMERATE},
    {"alinkColor",   DOC_ALINK_COLOR,   JSPROP_ENUMERATE},
    {"width",        DOC_WIDTH,         JSPROP_ENUMERATE},
    {"height",       DOC_HEIGHT,        JSPROP_ENUMERATE},
    {0}
};

#define LO_COLOR_TYPE(slot)                             \
    (((slot) == DOC_BG_COLOR) ? LO_COLOR_BG             \
     : ((slot) == DOC_FG_COLOR) ? LO_COLOR_FG           \
     : ((slot) == DOC_LINK_COLOR) ? LO_COLOR_LINK       \
     : ((slot) == DOC_VLINK_COLOR) ? LO_COLOR_VLINK     \
     : ((slot) == DOC_ALINK_COLOR) ? LO_COLOR_ALINK     \
     : -1)


extern PRThread *mozilla_thread;
extern JSClass lm_document_class;
extern JSClass lm_form_class;
extern JSClass *lm_java_clasp;

JSContext *lm_writing_context;
static int32 writing_context_counter;

PR_STATIC_CALLBACK(JSBool)
doc_getProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    JSDocument *doc;
    MochaDecoder *decoder;
    MWContext *context;
    History_entry *he = NULL;
    JSString *str = NULL;
    int64 prsec, scale, prusec;
    PRExplodedTime prtime;
    char *cookie, buf[100];
    const char *domain, *origin;
    uint32 type;
    LO_Color rgb, *bg_color;
    jsint slot;
    CL_Layer *layer;
    int32 active_layer_id;

    doc = JS_GetInstancePrivate(cx, obj, &lm_document_class, NULL);
    if (!doc)
        return JS_TRUE;
    decoder = doc->decoder;

    slot = JSVAL_IS_INT(id) ? JSVAL_TO_INT(id) : 0;

    if (IS_SECURE_DOC_SLOT(slot) ||
        (slot == 0 &&
         JSVAL_IS_OBJECT(*vp) && 
         JSVAL_TO_OBJECT(*vp) && 
         (JS_InstanceOf(cx, JSVAL_TO_OBJECT(*vp), &lm_form_class, NULL) ||
          (lm_java_clasp &&
	   JS_InstanceOf(cx, JSVAL_TO_OBJECT(*vp), lm_java_clasp, NULL)))))
    {
        if (!lm_CheckPermissions(cx, obj, JSTARGET_UNIVERSAL_BROWSER_READ))
            return JS_FALSE;
    }

    if (!JSVAL_IS_INT(id))
        return JS_TRUE;

    context = decoder->window_context;
    if (!context)
        return JS_TRUE;

    LO_LockLayout();
    if(decoder->doc_id != XP_DOCID(context)) {
        LO_UnlockLayout();
#ifdef DEBUG_chouck
        XP_TRACE(("DOCID: in doc_getProperty"));
#endif
        return JS_FALSE;
    }

    switch (slot) {
      case DOC_FORMS:
        *vp = OBJECT_TO_JSVAL(lm_GetFormArray(decoder, obj));
        active_layer_id = LM_GetActiveLayer(context);
        LM_SetActiveLayer(context, doc->layer_id);
        (void) LO_EnumerateForms(context, doc->layer_id);
        LM_SetActiveLayer(context, active_layer_id);
        LO_UnlockLayout();
        return JS_TRUE;

      case DOC_LINKS:
        *vp = OBJECT_TO_JSVAL(lm_GetLinkArray(decoder, obj));
        active_layer_id = LM_GetActiveLayer(context);
        LM_SetActiveLayer(context, doc->layer_id);
        (void) LO_EnumerateLinks(context, doc->layer_id);
        LM_SetActiveLayer(context, active_layer_id);
        LO_UnlockLayout();
        return JS_TRUE;

      case DOC_ANCHORS:
        *vp = OBJECT_TO_JSVAL(lm_GetNameArray(decoder, obj));
        active_layer_id = LM_GetActiveLayer(context);
        LM_SetActiveLayer(context, doc->layer_id);
        (void) LO_EnumerateNamedAnchors(context,doc->layer_id);
        LM_SetActiveLayer(context, active_layer_id);
        LO_UnlockLayout();
        return JS_TRUE;

      case DOC_APPLETS:
#ifdef JAVA
        if (LM_MOJA_OK != ET_InitMoja(context)) {
            LO_UnlockLayout();
            return JS_FALSE;
        }
        
        *vp = OBJECT_TO_JSVAL(lm_GetAppletArray(decoder, obj));
        active_layer_id = LM_GetActiveLayer(context);
        LM_SetActiveLayer(context, doc->layer_id);
        (void) LO_EnumerateApplets(context,doc->layer_id);
        LM_SetActiveLayer(context, active_layer_id);
        LO_UnlockLayout();
        return JS_TRUE;
#else
        LO_UnlockLayout();
        break;
#endif

      case DOC_EMBEDS:
#ifdef JAVA
        if (LM_MOJA_OK != ET_InitMoja(context)) {
            LO_UnlockLayout();
            return JS_FALSE;
        }

        *vp = OBJECT_TO_JSVAL(lm_GetEmbedArray(decoder, obj));
        active_layer_id = LM_GetActiveLayer(context);
        LM_SetActiveLayer(context, doc->layer_id);
        (void) LO_EnumerateEmbeds(context,doc->layer_id);
        LM_SetActiveLayer(context, active_layer_id);
        LO_UnlockLayout();
        return JS_TRUE;
#else
        LO_UnlockLayout();
        break;
#endif

      case DOC_IMAGES:
        *vp = OBJECT_TO_JSVAL(lm_GetImageArray(decoder, obj));
        active_layer_id = LM_GetActiveLayer(context);
        LM_SetActiveLayer(context, doc->layer_id);
        (void) LO_EnumerateImages(context,doc->layer_id);
        LM_SetActiveLayer(context, active_layer_id);
        LO_UnlockLayout();
        return JS_TRUE;

      case DOC_LAYERS:
        *vp = OBJECT_TO_JSVAL(lm_GetDocumentLayerArray(decoder, obj));
        LO_UnlockLayout();
        return JS_TRUE;

#ifdef DOM
      case DOC_SPANS:
        *vp = OBJECT_TO_JSVAL(lm_GetSpanArray(decoder, obj));
        active_layer_id = LM_GetActiveLayer(context);
        LM_SetActiveLayer(context, doc->layer_id);
        (void) LO_EnumerateSpans(context,doc->layer_id);
        LM_SetActiveLayer(context, active_layer_id);
        LO_UnlockLayout();
        return JS_TRUE;

	  case DOC_TRANSCLUSIONS:
		/* We are assuming that by the time any JS sees document.transclusions[]
		   all the transclusions have been reflected into JS.  So
		   there is no need for the call to XMLEnumerateTransclusions that
		   reflects all Transclusions into JS.
		*/		
        *vp = OBJECT_TO_JSVAL(lm_GetTransclusionArray(decoder, obj));
        active_layer_id = LM_GetActiveLayer(context);
        LM_SetActiveLayer(context, doc->layer_id);
		/*
        (void) XMLEnumerateTransclusions(context,doc->layer_id);
		*/
        LM_SetActiveLayer(context, active_layer_id); 
        LO_UnlockLayout();
        return JS_TRUE;
#endif

        /* XXX BUGBUG Need a story for some of these for a layer's document */
      case DOC_TITLE:
        he = SHIST_GetCurrent(&context->hist);
        str = lm_LocalEncodingToStr(context, he ? he->title : "");
        LO_UnlockLayout();
        break;

      case DOC_URL:
        he = SHIST_GetCurrent(&context->hist);
        str = JS_NewStringCopyZ(cx, he ? he->address : "");
        LO_UnlockLayout();
        break;

      case DOC_REFERRER:
        he = SHIST_GetCurrent(&context->hist);
        str = JS_NewStringCopyZ(cx, he ? he->referer : "");
        LO_UnlockLayout();
        break;

      case DOC_LAST_MODIFIED:
        he = SHIST_GetCurrent(&context->hist);
        if (he) {
            LL_I2L(prsec, he->last_modified);
            LL_I2L(scale, 1000000);
            LL_MUL(prusec, prsec, scale);
            PR_ExplodeTime(prusec, PR_LocalTimeParameters, &prtime);
            PR_FormatTime(buf, sizeof buf, "%c", &prtime);
        } else {
            buf[0] = '\0';
        }
        str = JS_NewStringCopyZ(cx, buf);
        LO_UnlockLayout();
        break;

      case DOC_LOADED_DATE:
        LO_UnlockLayout();
        /* XXX todo */
        str = JSVAL_TO_STRING(JS_GetEmptyStringValue(cx));
        break;

      case DOC_COOKIE:
        /* XXX overloaded return - can't distinguish "none" from "error" */
        he = SHIST_GetCurrent(&context->hist);
	cookie = NET_GetCookie(context, he ? he->address : "");
        str = JS_NewStringCopyZ(cx, cookie);
	XP_FREEIF(cookie);
        LO_UnlockLayout();
        break;

      case DOC_DOMAIN:
        LO_UnlockLayout();
        origin = lm_GetObjectOriginURL(cx, obj);
        if (!origin)
            return JS_FALSE;
        domain = NET_ParseURL(origin, GET_HOST_PART);
        if (!domain) {
            JS_ReportOutOfMemory(cx);
            return JS_FALSE;
        }
        str = JS_NewStringCopyZ(cx, domain);
        XP_FREE((char *)domain);
        break;

      case DOC_WIDTH:
        layer = LO_GetLayerFromId(context, doc->layer_id);
        *vp = INT_TO_JSVAL(LO_GetLayerScrollWidth(layer));
        LO_UnlockLayout();
        return JS_TRUE;
          
      case DOC_HEIGHT:
        layer = LO_GetLayerFromId(context, doc->layer_id);
        *vp = INT_TO_JSVAL(LO_GetLayerScrollHeight(layer));
        LO_UnlockLayout();
        return JS_TRUE;

      case DOC_BG_COLOR:
        layer = LO_GetLayerFromId(context, doc->layer_id);
        bg_color = LO_GetLayerBgColor(layer);
        LO_UnlockLayout();
        if (bg_color) {
            XP_SPRINTF(buf, "#%02x%02x%02x", bg_color->red, bg_color->green, 
                       bg_color->blue);
            str = JS_NewStringCopyZ(cx, buf);
        } 
        else {
            *vp = JSVAL_NULL;
            return JS_TRUE;
        }
        break;
         
      default:
        type = LO_COLOR_TYPE(slot);
        if (type >= LO_NCOLORS) {
            LO_UnlockLayout();
            /* Don't mess with a user-defined or method property. */
            return JS_TRUE;
        }
        /* Note: background color handled above */
        LO_GetDocumentColor(context, type, &rgb);
        XP_SPRINTF(buf, "#%02x%02x%02x", rgb.red, rgb.green, rgb.blue);
        str = JS_NewStringCopyZ(cx, buf);
        LO_UnlockLayout();
        break;
    }

    /* Common tail code for string-type properties. */
    if (!str)
        return JS_FALSE;
    *vp = STRING_TO_JSVAL(str);
    return JS_TRUE;
}


PR_STATIC_CALLBACK(JSBool)
doc_setProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    JSObject *layer_obj;
    JSDocument *doc;
    MochaDecoder *decoder;
    MWContext *context;
    char *protocol, *pathname, *new_origin_url, *str;
    const char *domain, *new_domain, *domain_suffix, *prop_name, *origin;
    size_t domain_len, new_domain_len;
    JSBool ok;
    uint32 type;
    LO_Color *rgb;
    jsint slot;
    int32 val;
    CL_Layer *layer;
    History_entry *he = NULL;

    doc = JS_GetInstancePrivate(cx, obj, &lm_document_class, NULL);
    if (!doc)
        return JS_TRUE;
    decoder = doc->decoder;

    if (!lm_CheckPermissions(cx, obj, JSTARGET_UNIVERSAL_BROWSER_WRITE))
        return JS_FALSE;

    if (!JSVAL_IS_INT(id)) {
        /*
         * Due to the wonderful world of threads we need to know ahead of
         * time if someone is setting an onMouseMove event handler here or
         * in document so that we don't lose messages.
         */
        if (JS_TypeOfValue(cx, *vp) == JSTYPE_FUNCTION) {
            if (JSVAL_IS_STRING(id)) {
                prop_name = JS_GetStringBytes(JSVAL_TO_STRING(id));
                /* XXX use lm_onMouseMove_str etc.*/
                if (XP_STRCMP(prop_name, "onmousemove") == 0 ||
                    XP_STRCMP(prop_name, "onMouseMove") == 0) {
                    decoder->window_context->js_drag_enabled = TRUE;
                }
            }
        }
        return JS_TRUE;
    }

    slot = JSVAL_TO_INT(id);

    context = decoder->window_context;
    if (!context)
        return JS_TRUE;

    LO_LockLayout();
    if(decoder->doc_id != XP_DOCID(context)) {
        LO_UnlockLayout();
#ifdef DEBUG_chouck
        XP_TRACE(("DOCID: in doc_setProperty"));
#endif
        return JS_FALSE;
    }

    switch (slot) {
      case DOC_COOKIE:
        if (!JSVAL_IS_STRING(*vp) &&
            !JS_ConvertValue(cx, *vp, JSTYPE_STRING, vp)) {
	    LO_UnlockLayout();
            return JS_FALSE;
        }

	he = SHIST_GetCurrent(&context->hist);
	str = JS_GetStringBytes(JSVAL_TO_STRING(*vp));

        /* will make a copy that netlib can munge */
	if (str)
	    str = strdup(str);

        NET_SetCookieString(context, he ? he->address : "", str);
        LO_UnlockLayout();
        break;

      case DOC_DOMAIN:
        LO_UnlockLayout();
        origin = lm_GetObjectOriginURL(cx, obj);
        if (!origin || XP_STRCMP(origin, lm_unknown_origin_str) == 0)
            return JS_FALSE;
        domain = NET_ParseURL(origin, GET_HOST_PART);
        if (!domain) {
            JS_ReportOutOfMemory(cx);
            return JS_FALSE;
        }
        if (!JSVAL_IS_STRING(*vp) &&
            !JS_ConvertValue(cx, *vp, JSTYPE_STRING, vp)) {
            XP_FREE((char *)domain);
            return JS_FALSE;
        }
        new_domain = JS_GetStringBytes(JSVAL_TO_STRING(*vp));
        new_domain_len = JS_GetStringLength(JSVAL_TO_STRING(*vp));
        domain_len = XP_STRLEN(domain);
        domain_suffix = domain + domain_len - new_domain_len;
        ok = (JSBool)(!XP_STRCASECMP(domain, new_domain) ||
             (domain_len > new_domain_len &&
              !XP_STRCASECMP(domain_suffix, new_domain) &&
              (domain_suffix[-1] == '.' || domain_suffix[-1] == '/')));
        if (!ok) {
            JS_ReportError(cx, "illegal document.domain value %s",
                              new_domain);
        } else {
            protocol = NET_ParseURL(origin, GET_PROTOCOL_PART);
            pathname = NET_ParseURL(origin, GET_PATH_PART | GET_HASH_PART |
                                            GET_SEARCH_PART);
            new_origin_url = PR_smprintf("%s//%s%s",
                                         protocol, new_domain, pathname);
            ok = (JSBool)(protocol && pathname && new_origin_url);
            if (!ok) {
                JS_ReportOutOfMemory(cx);
            } else {
                JSPrincipals *principals;
                principals = lm_GetInnermostPrincipals(cx, obj, NULL);
                if (principals) {
                    ok = lm_SetDocumentDomain(cx, principals, new_origin_url);
                } 
            }
            PR_FREEIF(protocol);
            PR_FREEIF(pathname);
            PR_FREEIF(new_origin_url);
        }
        XP_FREE((char *)domain);
        return ok;

      case DOC_BG_COLOR:
        if (doc->layer_id == LO_DOCUMENT_LAYER_ID) {
            layer = LO_GetLayerFromId(context, LO_DOCUMENT_LAYER_ID);
            LO_UnlockLayout();
            if (!lm_jsval_to_rgb(cx, vp, &rgb))
                return JS_FALSE;
            ET_TweakLayer(decoder->window_context, layer, 0, 0,
                          rgb, 0, CL_SetBgColor, NULL, decoder->doc_id);
        } else {
            layer_obj = LO_GetLayerMochaObjectFromId(context, doc->layer_id);
            LO_UnlockLayout();
            if (layer_obj)
                layer_setBgColorProperty(cx, layer_obj, vp);
        }
        break;

      case DOC_FG_COLOR:
      case DOC_LINK_COLOR:
      case DOC_VLINK_COLOR:
      case DOC_ALINK_COLOR:
        type = LO_COLOR_TYPE(slot);
        if (type >= LO_NCOLORS) {
            LO_UnlockLayout();
            break;
        }

        if (!lm_jsval_to_rgb(cx, vp, &rgb) || !rgb) {
            LO_UnlockLayout();
            return JS_FALSE;
        }
        
        LO_SetDocumentColor(context, type, rgb);
        LO_UnlockLayout();
        if (rgb)
            XP_FREE(rgb);
        break;

        /* 
         * XXX BUGBUG These don't do the right thing for a window's document.
         * Does any of this make sense for a layer's document?
         */
      case DOC_WIDTH:
        /* Front-ends can't cope with a document dimension less than one. */
        if (!JS_ValueToInt32(cx, *vp, &val)) {
            LO_UnlockLayout();
            return JS_FALSE;
        }
        layer = LO_GetLayerFromId(context, doc->layer_id);
        LO_SetLayerScrollWidth(layer, val <= 1 ? 1 : val);
        LO_UnlockLayout();
        break;
          
      case DOC_HEIGHT:
        /* Front-ends can't cope with a document dimension less than one. */
        if (!JS_ValueToInt32(cx, *vp, &val)) {
            LO_UnlockLayout();
            return JS_FALSE;
        }
        layer = LO_GetLayerFromId(context, doc->layer_id);
        LO_SetLayerScrollHeight(layer, val <= 1 ? 1 : val);
        LO_UnlockLayout();
        break;

      default:
        LO_UnlockLayout();
        break;
    }

    return doc_getProperty(cx, obj, id, vp);
}

#ifdef JAVA
static void 
lm_reflect_stuff_eagerly(MWContext * context, int32 layer_id)
{

    lo_TopState *top_state;
    int count, index;
    LO_JavaAppStruct *applet;
    LO_EmbedStruct *embed;
    lo_DocLists *doc_lists;

    LO_LockLayout();

    top_state = lo_GetMochaTopState(context);
    if (top_state == NULL) {
        LO_UnlockLayout();
        return;
    }
    doc_lists = lo_GetDocListsById(top_state->doc_state, layer_id);

    count = 0;
    applet = doc_lists->applet_list;
    while (applet) {
        applet = applet->nextApplet;
        count++;
    }

    /* reflect all applets in reverse order */
    applet = doc_lists->applet_list;
    for (index = count-1; index >= 0; index--) {
        if (applet->mocha_object == NULL) {
            LM_ReflectApplet(context, applet, NULL, layer_id, index);
        }
        applet = applet->nextApplet;
    }

    count = 0;
    embed = doc_lists->embed_list;
    while (embed) {
        embed = embed->nextEmbed;
        count++;
    }

    /* reflect all embeds in reverse order */
    embed = doc_lists->embed_list;
    for (index = count-1; index >= 0; index--) {
        if (embed->mocha_object == NULL) {
            LM_ReflectEmbed(context, embed, NULL, layer_id, index);
        }
        embed = embed->nextEmbed;
    }

    LO_UnlockLayout();
}
#endif /* JAVA */

PR_STATIC_CALLBACK(JSBool)
doc_list_properties(JSContext *cx, JSObject *obj)
{
#ifdef JAVA

    /* reflect applets eagerly, anything else? */
    JSDocument *doc;
    MWContext *context;
    JSBool bDoInitMoja = JS_FALSE;
    lo_TopState *top_state;
    lo_DocLists *doc_lists;

    doc = JS_GetPrivate(cx, obj);
    if (!doc)
        return JS_TRUE;
    context = doc->decoder->window_context;
    if (!context)
        return JS_TRUE;

    LO_LockLayout();

    /* if no embeds or applets don't eagerly start java */
    top_state = lo_GetMochaTopState(context);
    if (top_state) {
        doc_lists = lo_GetDocListsById(top_state->doc_state, doc->layer_id);

        if(doc_lists){
			if (doc_lists->applet_list)
				bDoInitMoja = JS_TRUE;
			else if (doc_lists->embed_list){
				/* determine if any of these embeds actually require java, are liveconnected. */
				LO_EmbedStruct *embed;
				for (embed = doc_lists->embed_list; embed != NULL; embed=embed->nextEmbed){
					if (NPL_IsLiveConnected(embed)){
						bDoInitMoja = JS_TRUE;
						break; /* it only takes one  */
					}
				}
			}
		}

    }
    LO_UnlockLayout();

    if(bDoInitMoja) {
        if (LM_MOJA_OK != ET_InitMoja(context))
            return JS_FALSE;
        if (JSJ_IsEnabled())
            lm_reflect_stuff_eagerly(context, doc->layer_id);
    }

#endif
    return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
doc_resolve_name(JSContext *cx, JSObject *obj, jsval id)
{
    JSDocument *doc;
    const char * name;
    JSObject *layer_obj;

    if (!JSVAL_IS_STRING(id))
        return JS_TRUE;

    name = JS_GetStringBytes(JSVAL_TO_STRING(id));

    doc = JS_GetPrivate(cx, obj);
    if (!doc)
        return JS_TRUE;

    layer_obj = lm_GetNamedLayer(doc->decoder, doc->layer_id, name);
    if (layer_obj)
        return JS_DefineProperty(cx, obj, name,
                                 OBJECT_TO_JSVAL(layer_obj),
                                 NULL, NULL,
                                 JSPROP_ENUMERATE | JSPROP_READONLY);

    if (!JSS_ResolveDocName(cx, doc->decoder->window_context, obj, id))
        return JS_FALSE;

    return doc_list_properties(cx, obj);
}

PR_STATIC_CALLBACK(void)
doc_finalize(JSContext *cx, JSObject *obj)
{
    JSDocument *doc;

    doc = JS_GetPrivate(cx, obj);
    if (!doc)
        return;
    DROP_BACK_COUNT(doc->decoder);
    JS_RemoveRoot(cx, &doc->forms);
    JS_RemoveRoot(cx, &doc->links);
    JS_RemoveRoot(cx, &doc->anchors);
    JS_RemoveRoot(cx, &doc->applets);
    JS_RemoveRoot(cx, &doc->embeds);
    JS_RemoveRoot(cx, &doc->images);
    JS_RemoveRoot(cx, &doc->layers);
    JS_free(cx, doc);
}

JSClass lm_document_class = {
    "Document", JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub, doc_getProperty, doc_setProperty,
    doc_list_properties, doc_resolve_name, JS_ConvertStub, doc_finalize
};

PR_STATIC_CALLBACK(JSBool)
Document(JSContext *cx, JSObject *obj,
         uint argc, jsval *argv, jsval *rval)
{
    return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
doc_toString(JSContext *cx, JSObject *obj,
              uint argc, jsval *argv, jsval *rval)
{
    /* XXX make string of whole doc for trusted scripts */
    *rval = JS_GetEmptyStringValue(cx); 
    return JS_TRUE;
}

char *
LM_GetBaseHrefTag(JSContext *cx, JSPrincipals *principals)
{
    char *new_origin_url;
    const char *origin_url;
    const char *cp, *qp, *last_slash;
    char *tag;

    origin_url = principals ? principals->codebase : lm_unknown_origin_str;
    if (origin_url == NULL)
        return NULL;
    new_origin_url = 0;
    cp = origin_url;
    if ((qp = XP_STRCHR(cp, '"')) != 0) {
        do {
            new_origin_url = PR_sprintf_append(new_origin_url, "%.*s%%%x",
                                                qp - cp, cp, *qp);
            cp = qp + 1;
        } while ((qp = XP_STRCHR(cp, '"')) != 0);
        new_origin_url = PR_sprintf_append(new_origin_url, "%s", cp);
        if (!new_origin_url) {
            JS_ReportOutOfMemory(cx);
            return NULL;
        }
        origin_url = new_origin_url;
    }
    last_slash = XP_STRRCHR(origin_url, '/');
    if (last_slash) {
        tag = PR_smprintf("<BASE HREF=\"%.*s/\">\n",
                          last_slash - origin_url, origin_url);
    } else {
        tag = PR_smprintf("<BASE HREF=\"%s\">\n", origin_url);
    }
    PR_FREEIF(new_origin_url);
    if (!tag)
        JS_ReportOutOfMemory(cx);
    return tag;
}

static int
doc_write_stream(JSContext *cx, MochaDecoder *decoder,
                 NET_StreamClass *stream, char *str, size_t len)
{	
    ET_lo_DoDocWrite(cx, decoder->window_context, stream, str, len,
                     decoder->doc_id);

    return JS_TRUE;
}

static char *
context_pathname(JSContext *cx, MWContext *context, int32 layer_id)
{
    MWContext *parent;
    int count, index;
    char *name = 0;

    while ((parent = context->grid_parent) != 0) {
        if (context->name) {
            name = PR_sprintf_append(name, "%s.", context->name);
        } else {
            /* XXX silly xp_cntxt.c puts newer contexts at the front! */
            count = XP_ListCount(parent->grid_children);
            index = XP_ListGetNumFromObject(parent->grid_children, context);
            name = PR_sprintf_append(name, "%u.", count - index);
        }
        context = parent;
    }
    name = PR_sprintf_append(name, "%ld", (long)XP_DOCID(context));
    if (layer_id != LO_DOCUMENT_LAYER_ID)
        name = PR_sprintf_append(name, ".%ld", layer_id);
    if (!name)
        JS_ReportOutOfMemory(cx);
    return name;
}

/* Make a wysiwyg: URL containing the context and our origin. */
char *
lm_MakeWysiwygUrl(JSContext *cx, MochaDecoder *decoder, int32 layer_id,
                  JSPrincipals *principals)
{
    const char *origin;
    const char *pathname;
    char *url_string;
    char *slash;

    origin = principals ? principals->codebase : lm_unknown_origin_str;
    if (origin == NULL)
        return NULL;
    pathname = context_pathname(cx, decoder->window_context, layer_id);
    if (!pathname)
        return NULL;
    while ((slash = XP_STRCHR(pathname, '/')) != NULL) {
        /* Escape '/' in pathname as "%2f" */
        char *newPath;
        *slash = '\0';
        newPath = PR_smprintf("%s%%2f%s", pathname, slash+1);
        XP_FREE((char *)pathname);
        if (!newPath) {
            JS_ReportOutOfMemory(cx);
            return NULL;
        }
        pathname = newPath;
    }
    url_string = PR_smprintf("wysiwyg://%s/%s", pathname, origin);
    XP_FREE((char *)pathname);
    if (!url_string) {
        JS_ReportOutOfMemory(cx);
        return NULL;
    }
    return url_string;
}

static NET_StreamClass *
doc_cache_converter(JSContext *cx, MochaDecoder *decoder,
                    URL_Struct *url_struct, char *wysiwyg_url,
                    int32 layer_id)
{
    NET_StreamClass * rv;

    rv = ET_moz_DocCacheConverter(decoder->window_context, 
                                  url_struct, wysiwyg_url,
                                  layer_id);

    if (!rv)
        JS_ReportOutOfMemory(cx);

    return rv;

}

NET_StreamClass *
LM_WysiwygCacheConverter(MWContext *context, URL_Struct *url_struct,
                         const char * wysiwyg_url, const char * base_href)
{
    NET_StreamClass *cache_stream;

    if (!wysiwyg_url) {
        cache_stream = 0;
    } else {
        cache_stream = lm_DocCacheConverter(context, url_struct,
                                            wysiwyg_url);
        if (cache_stream) {
            /* Wysiwyg files need a base tag that removes that URL prefix. */
            if (base_href) {
                (void) cache_stream->put_block(cache_stream, 
                                               base_href,
                                               XP_STRLEN(base_href));
            }
        }
    }
    return cache_stream;
}

static char nscp_open_tag[] = "<" PT_NSCP_OPEN ">";

static NET_StreamClass *
doc_open_stream(JSContext *cx, MochaDecoder *decoder, JSObject *doc_obj,
                const char *mime_type, JSBool replace)
{
    JSDocument *doc;
    MWContext *context;
    char *wysiwyg_url, *tag;
    JSPrincipals *principals;
    URL_Struct *url_struct, *cached_url;
    MochaDecoder *running;
    History_entry *he;
    NET_StreamClass *stream;
    JSBool is_text_html;
    FO_Present_Types present_type;

    context = decoder->window_context;
    if (!context)
        return NULL;

    doc = JS_GetPrivate(cx, doc_obj);
    if (!doc)
        return NULL;
    
    /* Don't allow a script to replace a message window's contents
       with its own document.write'ed content, since that would allow
       them to spoof mail headers. */
    if ((doc->layer_id == LO_DOCUMENT_LAYER_ID) && IS_MESSAGE_WINDOW(context))
        return NULL;

    /* We must be called after any loading URL has finished. */
    XP_ASSERT(!decoder->url_struct);
    principals = lm_GetPrincipalsFromStackFrame(cx);
    wysiwyg_url = lm_MakeWysiwygUrl(cx, decoder, doc->layer_id, principals);
    if (!wysiwyg_url)
        return NULL;
    url_struct = NET_CreateURLStruct(wysiwyg_url, NET_DONT_RELOAD);
    if (!url_struct) {
        XP_FREE(wysiwyg_url);
        JS_ReportOutOfMemory(cx);
        return NULL;
    }

    /* Set content type so it can be cached. */
    StrAllocCopy(url_struct->content_type, mime_type);
    url_struct->preset_content_type = TRUE;
    url_struct->must_cache = TRUE;

    /* Set these to null so we can goto bad from here onward. */
    stream = 0;
    cached_url = 0;

    /* If the writer is secure, pass its security info into the cache. */
    running = JS_GetPrivate(cx, JS_GetGlobalObject(cx));
    he = SHIST_GetCurrent(&running->window_context->hist);
    if (he && he->security_on) {
        /* Copy security stuff (checking for malloc failure) */
        url_struct->security_on = he->security_on;
        url_struct->sec_info = SECNAV_CopySSLSocketStatus(he->sec_info);
        if (he->sec_info && !url_struct->sec_info)
            goto bad;
    }

    /* If we're opening a stream for the window's document */
    if (doc->layer_id == LO_DOCUMENT_LAYER_ID) {
        /* Free any old doc before stream building, which may discard too. */
        ET_lo_DiscardDocument(context);
        if (replace) {
            /* If replacing, flag current entry and url_struct specially. */
            lm_ReplaceURL(context, url_struct);
        }
        present_type = FO_PRESENT;
    }
    /* Otherwise, we're dealing with a layer's document */
    else {
	const char *referer = lm_GetSubjectOriginURL(cx);

	if (!referer ||
	    !ET_lo_PrepareLayerForWriting(context, doc->layer_id, referer)) {
	    goto bad;
	}
	lm_NewLayerDocument(decoder, doc->layer_id);
	LM_SetActiveLayer(context, doc->layer_id);
        present_type = FO_PRESENT_INLINE;
    }

    /* We must be called after any open stream has been closed. */
    XP_ASSERT(!decoder->stream);
    stream = ET_net_StreamBuilder(present_type, url_struct, context);
    if (!stream) {
        JS_ReportError(cx, "cannot open document for %s", mime_type);
        goto bad;
    }

    if (doc->layer_id == LO_DOCUMENT_LAYER_ID) {
        /* Layout discards documents lazily, but we are now eager. */
        if (!decoder->document && !lm_InitWindowContent(decoder))
            goto bad;

        /* 
         * Reset the doc_id since the context won't have a valid 
         * one set till we start writing out content.
         */
        decoder->doc_id = 0;
    }
    
    decoder->writing_input = JS_TRUE;
    if (!lm_SetInputStream(cx, decoder, stream, url_struct, JS_TRUE))
        goto bad;
    
    is_text_html =  (JSBool)(!XP_STRCMP(mime_type, TEXT_HTML));
    
    /* Only need to do this for a window's document */
    if (doc->layer_id == LO_DOCUMENT_LAYER_ID) {
        if (is_text_html) {
            /* Feed layout an internal tag so it will create a new top state.*/
            ET_lo_DoDocWrite(cx, context, stream, nscp_open_tag,
                             sizeof nscp_open_tag - 1, 0);
        }

        if (is_text_html || !XP_STRCMP(mime_type, TEXT_PLAIN)) {
            doc_cache_converter(cx, decoder, url_struct, 
                                wysiwyg_url, doc->layer_id);
        }

        /* Auto-generate a BASE HREF= tag for generated HTML documents. */
        if (is_text_html) {
            tag = LM_GetBaseHrefTag(cx, principals);
            if (!tag)
                goto bad;
            (void) doc_write_stream(cx, decoder, stream, tag, XP_STRLEN(tag));
            XP_FREE(tag);
        }
    }
    else {
        if (lm_SetLayerSourceURL(decoder, doc->layer_id, wysiwyg_url)) {
            if ((is_text_html || !XP_STRCMP(mime_type, TEXT_PLAIN)) &&
                !doc_cache_converter(cx, decoder, url_struct, 
                                     wysiwyg_url, doc->layer_id)) {
                lm_SetLayerSourceURL(decoder, doc->layer_id, NULL);
            }
        }
    }

    /* Drop our ref on url_struct -- decoder holds its own. */
    NET_DropURLStruct(url_struct);
    XP_FREE(wysiwyg_url);
    return stream;

bad:
    decoder->writing_input = JS_FALSE;
    decoder->free_stream_on_close = JS_FALSE;
    decoder->stream = 0;
    decoder->stream_owner = LO_DOCUMENT_LAYER_ID;
    decoder->url_struct = 0;
    if (stream) {
        (*stream->abort)(stream, MK_UNABLE_TO_CONVERT);
        XP_DELETE(stream);
    }
    NET_FreeURLStruct(url_struct);
    if (cached_url)
        NET_FreeURLStruct(cached_url);
    XP_FREE(wysiwyg_url);
    return NULL;
}

static JSBool
doc_stream_active(MochaDecoder *decoder)
{
    MWContext *context;
    lo_TopState *top_state;

    if (!decoder->writing_input)
        return JS_TRUE;

    context = decoder->window_context;
    if (context) {
        LO_LockLayout();
        top_state = lo_GetMochaTopState(context);
        if (top_state && top_state->input_write_level) {
            LO_UnlockLayout();
            return JS_TRUE;
        }
        LO_UnlockLayout();
    }
    return JS_FALSE;
}

/* XXX shared from lm_win.c for compatibility hack */
extern JSBool PR_CALLBACK
win_open(JSContext *cx, JSObject *obj, uint argc, jsval *argv, jsval *rval);

PR_STATIC_CALLBACK(JSBool)
doc_open(JSContext *cx, JSObject *obj, uint argc, jsval *argv, jsval *rval)
{
    JSDocument *doc;
    MochaDecoder *decoder;
    JSString *str, *str2;
    const char *mime_type, *options;
    JSBool replace;
    NET_StreamClass *stream;

    if (!JS_InstanceOf(cx, obj, &lm_document_class, argv))
        return JS_FALSE;
    doc = JS_GetPrivate(cx, obj);
    if (!doc)
        return JS_TRUE;
    decoder = doc->decoder;

    if (argc > 2 || argv[0] == JS_GetEmptyStringValue(cx)) {
        /* XXX hideous compatibility hack to call window.open */
        return win_open(cx, decoder->window_object, argc, argv, rval);
    }

    str = str2 = 0;
    mime_type = TEXT_HTML;
    replace = JS_FALSE;
    if (argc >= 1) {
        if (!(str = JS_ValueToString(cx, argv[0])))
            return JS_FALSE;
        mime_type = JS_GetStringBytes(str);
        if (argc >= 2) {
            if (!(str2 = JS_ValueToString(cx, argv[1])))
                return JS_FALSE;
            options = JS_GetStringBytes(str2);
            replace = (JSBool)(!XP_STRCASECMP(options, "replace"));
        }
    }
    stream = decoder->stream;
    if (stream) {
        if (doc_stream_active(decoder)) {
            /* Don't close a stream being fed by netlib. */
            *rval = JSVAL_NULL;
            return JS_TRUE;
        }
        lm_ClearDecoderStream(decoder, JS_FALSE);
    }
    stream = doc_open_stream(cx, decoder, obj, mime_type, replace);
    if (!stream)
        return JS_FALSE;
    *rval = OBJECT_TO_JSVAL(doc->object);
    return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
doc_close(JSContext *cx, JSObject *obj, uint argc, jsval *argv, jsval *rval)
{
    JSDocument *doc;
    MochaDecoder *decoder;

    if (!JS_InstanceOf(cx, obj, &lm_document_class, argv))
        return JS_FALSE;
    doc = JS_GetPrivate(cx, obj);
    if (!doc)
        return JS_TRUE;
    decoder = doc->decoder;
    if (!decoder->stream || doc_stream_active(decoder)) {
        /* Don't close a closed stream, or a stream being fed by netlib. */
        *rval = JSVAL_FALSE;
        return JS_TRUE;
    }
    lm_ClearDecoderStream(decoder, JS_FALSE);
    *rval = JSVAL_TRUE;
    return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
doc_clear(JSContext *cx, JSObject *obj, uint argc, jsval *argv, jsval *rval)
{
    JSDocument *doc;
    MochaDecoder *decoder;
    MWContext *context;

    if (!JS_InstanceOf(cx, obj, &lm_document_class, argv))
        return JS_FALSE;
    doc = JS_GetPrivate(cx, obj);
    if (!doc)
        return JS_TRUE;
    decoder = doc->decoder;
    context = decoder->window_context;
    if (!context)
        return JS_TRUE;
    if (!doc_close(cx, obj, argc, argv, rval))
        return JS_FALSE;
    /* XXX need to open and push a tag through layout? */
    ET_PostClearView(context);
    return JS_TRUE;
}

static NET_StreamClass *
doc_get_stream(JSContext *cx, MochaDecoder *decoder, JSObject *doc_obj)
{
    NET_StreamClass *stream;
    JSDocument *doc;

    doc = JS_GetPrivate(cx, doc_obj);
    if (!doc)
        return NULL;
    
    stream = decoder->stream;
    if (!stream) {
        stream = doc_open_stream(cx, decoder, doc_obj, TEXT_HTML, JS_FALSE);
    /*
     * If, at document load time, we're trying to get a stream to a layer
     * that isn't the currently loading layer, then disallow it.
     */
    } else if (doc->layer_id != decoder->active_layer_id &&
	       !decoder->load_event_sent) {
        return NULL;
    /*
     * If we're writing to the main document or before the main document
     * has completely loaded, make sure that we do all the wysiwyg
     * nonsense.
     */
    } else if (doc->layer_id == LO_DOCUMENT_LAYER_ID ||
               !decoder->load_event_sent) { 
        XP_ASSERT(decoder->url_struct);
        if (decoder->url_struct && !decoder->url_struct->wysiwyg_url)
            ET_moz_SetMochaWriteStream(decoder);
    }
    return stream;
}

static JSBool
do_doc_write(JSContext *cx, JSObject *obj,
             uint argc, jsval *argv, JSBool write_eol, 
             jsval *rval)
{
    JSDocument *doc;
    MochaDecoder *decoder;
    MochaDecoder *running;
    NET_StreamClass *stream;
    uint i;
    int status, total_len, len;
    static char eol[] = "\n";
    char * buf, * start;
    JSString * str;

    doc = JS_GetPrivate(cx, obj);
    if (!doc)
        return JS_TRUE;

    if (PR_CurrentThread() == mozilla_thread)
        return JS_FALSE;

    decoder = doc->decoder;
    stream = doc_get_stream(cx, decoder, obj);
    if (!stream)
        return JS_FALSE;

    /* see how much space we need */
    total_len = 0;
    for (i = 0; i < argc; i++) {
        if (JSVAL_IS_STRING(argv[i])) {
            str = JSVAL_TO_STRING(argv[i]);
        }
        else {
            str = JS_ValueToString(cx, argv[i]);
            if (!str)
                return JS_FALSE;
            argv[i] = STRING_TO_JSVAL(str);
        }

        total_len += JS_GetStringLength(str);

    }

    /* see if we need to allocate space for the end of line */
    if (write_eol)
        total_len += XP_STRLEN(eol);

    start = buf = XP_ALLOC((total_len + 1) * sizeof(char));
    if (!buf)
        return JS_FALSE;

    buf[0] = '\0';

    /* cache these so we don't need to look up each iteration */
    running = JS_GetPrivate(cx, JS_GetGlobalObject(cx));

    /* build the string for whatever is on the other end of the stream */
    for (i = 0; i < argc; i++) {

        str = JSVAL_TO_STRING(argv[i]);
        len = JS_GetStringLength(str);

        /* 
         * JSStrings can contain NUL, not sure how well layout will
         *   deal with that but pass them along in case they're going
         *   to the imglib.
         */
	/* XXXunicode */
        memcpy(buf, JS_GetStringBytes(str), len);
        buf += len;

    }

    /* generate a new line */
    if (write_eol)
        XP_STRCPY(buf, eol);
    else
        *buf = '\0';

    if (lm_writing_context == NULL)
        lm_writing_context = cx;
    writing_context_counter++;

    /* if we made it to here we must have a valid string */
    status = ET_lo_DoDocWrite(cx, decoder->window_context, stream, start,
                              total_len, decoder->doc_id);

    if (--writing_context_counter == 0)
        lm_writing_context = NULL;

    XP_FREE(start);

    if (status < 0) {
        *rval = JSVAL_FALSE;
        return JS_TRUE;
    }
    
    if (!decoder->document && !lm_InitWindowContent(decoder))
        return JS_FALSE;
    *rval = JSVAL_TRUE;
    return JS_TRUE;

}

PR_STATIC_CALLBACK(JSBool)
doc_write(JSContext *cx, JSObject *obj,
          uint argc, jsval *argv, jsval *rval)
{
    if (!JS_InstanceOf(cx, obj, &lm_document_class, argv))
        return JS_FALSE;

    return do_doc_write(cx, obj, argc, argv, JS_FALSE, rval);
}

PR_STATIC_CALLBACK(JSBool)
doc_writeln(JSContext *cx, JSObject *obj,
            uint argc, jsval *argv, jsval *rval)
{
    if (!JS_InstanceOf(cx, obj, &lm_document_class, argv))
        return JS_FALSE;

    return do_doc_write(cx, obj, argc, argv, JS_TRUE, rval);
}

PR_STATIC_CALLBACK(JSBool)
doc_capture_events(JSContext *cx, JSObject *obj,
            uint argc, jsval *argv, jsval *rval)
{
    JSDocument *doc;
    MochaDecoder *decoder;
    jsdouble d;

    if (!JS_InstanceOf(cx, obj, &lm_document_class, argv))
        return JS_FALSE;
    doc = JS_GetPrivate(cx, obj);
    if (!doc)
        return JS_FALSE;
    decoder=doc->decoder;

    if (!decoder->window_context)
        return JS_TRUE;

    if (argc != 1)
        return JS_TRUE;

    if (!JS_ValueToNumber(cx, argv[0], &d)) 
        return JS_FALSE;

    doc->capturer.event_bit |= (uint32)d;
    decoder->window_context->event_bit |= (uint32)d;

    return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
doc_release_events(JSContext *cx, JSObject *obj,
            uint argc, jsval *argv, jsval *rval)
{
    JSDocument *doc;
    JSEventCapturer *cap;
    MochaDecoder *decoder;
    jsdouble d;
    jsint layer_index;
    jsint max_layer_num;
    JSObject *cap_obj;

    if (!JS_InstanceOf(cx, obj, &lm_document_class, argv))
        return JS_FALSE;
    doc = JS_GetPrivate(cx, obj);
    if (!doc)
        return JS_FALSE;
    decoder=doc->decoder;

    if (!decoder->window_context)
        return JS_TRUE;

    if (argc != 1)
        return JS_TRUE;

    if (!JS_ValueToNumber(cx, argv[0], &d)) 
        return JS_FALSE;

    doc->capturer.event_bit &= ~(uint32)d;
    decoder->window_context->event_bit &= ~(uint32)d;

    /*Now we have to see if anyone else wanted that bit set.  Joy!*/
    /*First we check versus window */
    decoder->window_context->event_bit |= decoder->event_bit;

    /*Now we check versus layers */
    max_layer_num = LO_GetNumberOfLayers(decoder->window_context);
    
    for (layer_index=0; layer_index <= max_layer_num; layer_index++) {
        cap_obj = LO_GetLayerMochaObjectFromId(decoder->window_context, layer_index);
        if (cap_obj && (cap = JS_GetPrivate(cx, cap_obj)) != NULL)
            decoder->window_context->event_bit |= cap->event_bit;
        
        cap_obj = lm_GetDocumentFromLayerId(decoder, layer_index);
        if (cap_obj && (cap = JS_GetPrivate(cx, cap_obj)) != NULL)
            decoder->window_context->event_bit |= cap->event_bit;
    }
    
    return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
doc_get_selection(JSContext *cx, JSObject *obj,
            uint argc, jsval *argv, jsval *rval)
{
    JSDocument *doc;
    MWContext *context;
    JSString *str;
    char *text;
    
    if (!JS_InstanceOf(cx, obj, &lm_document_class, argv))
        return JS_FALSE;

    doc = JS_GetPrivate(cx, obj);
    if (!doc)
        return JS_FALSE;
    context = doc->decoder->window_context;

    text = ET_PostGetSelectedText(context);
    str = lm_LocalEncodingToStr(context, text);
    XP_FREE(text);
    if (!str)
        return JS_FALSE;

    *rval = STRING_TO_JSVAL(str);
    return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
doc_get_object_at(JSContext *cx, JSObject *obj,
            uint argc, jsval *argv, jsval *rval)
{
    JSDocument *doc;
    MochaDecoder *decoder;
    LO_Element *pElement;
    CL_Layer *layer;
    JSObject *xy_obj = 0;
    LO_AnchorData *anchor;
    int32 x, y;
    int16 type;

    if (!JS_InstanceOf(cx, obj, &lm_document_class, argv))
        return JS_FALSE;

    doc = JS_GetPrivate(cx, obj);
    if (!doc)
        return JS_FALSE;
    
    decoder = doc->decoder;

    if (argc != 2) {
        JS_ReportError(cx, lm_argc_err_str);
        return JS_FALSE;
    }

    if (!decoder->window_context)
        return JS_TRUE;

    if (!JS_ValueToInt32(cx, argv[0], &x) || !JS_ValueToInt32(cx, argv[1], &y))
        return JS_FALSE;    

    layer = LO_GetLayerFromId(decoder->window_context, doc->layer_id);
        
    LO_LockLayout();

    pElement = LO_XYToElement(decoder->window_context, x, y, layer);

    type = pElement ? pElement->type : LO_NONE;

    switch (type) {
      case LO_TEXT:
        anchor = pElement->lo_text.anchor_href;
        if (!anchor)
            *rval = JSVAL_VOID;
        else
            *rval = OBJECT_TO_JSVAL(anchor->mocha_object);
        break;
      case LO_IMAGE:
        anchor = pElement->lo_image.anchor_href;
        if (!anchor)
            *rval = OBJECT_TO_JSVAL(pElement->lo_image.mocha_object);
        else 
            *rval = OBJECT_TO_JSVAL(anchor->mocha_object);
        break;
      case LO_FORM_ELE:
        *rval = OBJECT_TO_JSVAL(pElement->lo_form.mocha_object);
        break;
      default:
        *rval = OBJECT_TO_JSVAL(obj);
        break;
    }

    LO_UnlockLayout();

    return JS_TRUE;
}

static JSFunctionSpec doc_methods[] = {
    {"clear",           doc_clear,              0},
    {"close",           doc_close,              0},
    {"open",            doc_open,               1},
    {lm_toString_str,   doc_toString,           0},
    {"write",           doc_write,              0},
    {"writeln",         doc_writeln,            0},
    {"captureEvents",   doc_capture_events,     1}, 
    {"releaseEvents",   doc_release_events,     1},
    {"getSelection",    doc_get_selection,      0},
    {"getObjectAt",     doc_get_object_at,      0},
    {0}
};

JSObject *
lm_DefineDocument(MochaDecoder *decoder, int32 layer_id)
{

    JSObject *obj;
    JSContext *cx;
    JSDocument *doc;
    JSObject *parent;

    cx = decoder->js_context;
    doc = JS_malloc(cx, sizeof *doc);
    if (!doc)
        return NULL;
    XP_BZERO(doc, sizeof *doc);

    obj = lm_GetDocumentFromLayerId(decoder, layer_id);
    if (obj)
        return obj;
    
    if (layer_id == LO_DOCUMENT_LAYER_ID) {
        parent = decoder->window_object;
    } else {
        parent = 
            (JSObject *)LO_GetLayerMochaObjectFromId(decoder->window_context, 
                                                     layer_id);    
	if (!parent)
	    parent = decoder->window_object;
    }
    obj = JS_DefineObject(cx, parent, lm_document_str, &lm_document_class, 
                          decoder->document_prototype, 
                          JSPROP_ENUMERATE|JSPROP_READONLY);

    if (!obj || !JS_SetPrivate(cx, obj, doc)) {
        JS_free(cx, doc);
        return NULL;
    }

    if (!JS_AddNamedRoot(cx, &doc->forms,   lm_forms_str) ||
        !JS_AddNamedRoot(cx, &doc->links,   lm_links_str) ||
        !JS_AddNamedRoot(cx, &doc->anchors, lm_anchors_str) ||
        !JS_AddNamedRoot(cx, &doc->applets, lm_applets_str) ||
        !JS_AddNamedRoot(cx, &doc->embeds,  lm_embeds_str) ||
        !JS_AddNamedRoot(cx, &doc->images,  lm_images_str) ||
        !JS_AddNamedRoot(cx, &doc->layers,  lm_layers_str)
#ifdef DOM
		|| !JS_AddNamedRoot(cx, &doc->spans,   lm_spans_str)
		|| !JS_AddNamedRoot(cx, &doc->transclusions,   lm_transclusions_str)
#endif
		) {
        /* doc_finalize will clean up the rest. */
        return NULL;
    }

    doc->object = obj;
    doc->decoder = HOLD_BACK_COUNT(decoder);
    doc->layer_id = layer_id;
    if (layer_id == LO_DOCUMENT_LAYER_ID) {
        decoder->document = obj;
        XP_ASSERT(decoder->window_context);
        decoder->doc_id = XP_DOCID(decoder->window_context);
    }

    return obj;
}

JSBool
lm_InitDocumentClass(MochaDecoder *decoder)
{
    JSContext *cx;
    JSObject *prototype;

    cx = decoder->js_context;
    prototype = JS_InitClass(cx, decoder->window_object, 
                             decoder->event_capturer_prototype,
                             &lm_document_class, Document, 0,
                             doc_props, doc_methods, NULL, NULL);
    if (!prototype)
        return JS_FALSE;
    decoder->document_prototype = prototype;
    return JS_TRUE;
}

JSObject *
lm_GetDocumentFromLayerId(MochaDecoder *decoder, int32 layer_id)
{
    JSObject *layer_obj;
    JSBool ok;
    jsval val;
    JSObject *rv;

    LO_LockLayout();

    if (layer_id == LO_DOCUMENT_LAYER_ID)
        rv = decoder->document;
    else {
        layer_obj = LO_GetLayerMochaObjectFromId(decoder->window_context, 
                                                 layer_id);
        if (!layer_obj) {
            LO_UnlockLayout();
            return NULL;
        }
        
        ok = JS_LookupProperty(decoder->js_context, layer_obj, 
                               lm_document_str, &val);
        if (!ok) {
            LO_UnlockLayout();
            return NULL;
        }

        rv = JSVAL_IS_OBJECT(val) ? JSVAL_TO_OBJECT(val) : NULL;
    }

    LO_UnlockLayout();
    return rv;
}

/* Clears out object references from the doc private data */
void
lm_CleanUpDocumentRoots(MochaDecoder *decoder, JSObject *obj)
{
    JSDocument *doc;

    doc = JS_GetPrivate(decoder->js_context, obj);

    if (!doc)
        return;

    doc->forms = NULL;
    doc->links = NULL;
    doc->anchors = NULL;
    doc->applets = NULL;
    doc->embeds = NULL;
    doc->images = NULL;
    doc->layers = NULL;
#ifdef DOM
	doc->spans = NULL;
	doc->transclusions = NULL;
#endif
}

/* 
 * Called when the content associated with a document is destroyed,
 * but the document itself may not be. Cleans out object references
 * from doc private data (so that the objects can be collected). Also
 * deals with correctly relinquishing event capture.
 */
void
lm_CleanUpDocument(MochaDecoder *decoder, JSObject *obj)
{
    JSDocument *doc;
    MWContext *context;
    jsint layer_index, max_layer_num;
    JSObject *cap_obj;
    JSEventCapturer *cap;

    lm_CleanUpDocumentRoots(decoder, obj);
    
    doc = JS_GetPrivate(decoder->js_context, obj);
    if (!doc)
        return;

    doc->capturer.event_bit = 0;
    context = decoder->window_context;
    if (!context)
        return;
    context->event_bit = 0;

    /* Now we have to see who still wants events */
    /* First we check versus window */
    context->event_bit |= decoder->event_bit;

    /*Now we check versus layers */
    max_layer_num = LO_GetNumberOfLayers(context);

    for (layer_index=0; layer_index <= max_layer_num; layer_index++) {
        if (doc->layer_id == layer_index)
            continue;

        cap_obj = LO_GetLayerMochaObjectFromId(decoder->window_context, layer_index);
        if (cap_obj && (cap = JS_GetPrivate(decoder->js_context, cap_obj)) != NULL)
            context->event_bit |= cap->event_bit;
        
        cap_obj = lm_GetDocumentFromLayerId(decoder, layer_index);
        if (cap_obj && (cap = JS_GetPrivate(decoder->js_context, cap_obj)) != NULL)
            context->event_bit |= cap->event_bit;
    }

}



/*
 * This routine needs to be callable from either the mocha or
 *   the mozilla threads.  The call to lm_FreeWindowContent()
 *   may be problematic since its going to try to post
 *   clearTimeout() messages back to the mozilla thread if any
 *   are pending.
 */
void
LM_ReleaseDocument(MWContext *context, JSBool resize_reload)
{
    MochaDecoder *decoder;
    JSContext *cx;

    if (resize_reload)
        return;

    /* Be defensive about JS-unaware contexts. */
    cx = context->mocha_context;
    if (!cx)
        return;

    /* Hold the context's decoder rather than calling LM_GetMochaDecoder. */
    decoder = JS_GetPrivate(cx, JS_GetGlobalObject(cx));
    decoder->forw_count++;

    /* Turn of the capture bits for the window and context */
    decoder->event_bit = 0;
    decoder->window_context->event_bit = 0;
    
    /* Set first language version back to unknown for the next document */
    decoder->firstVersion = JSVERSION_UNKNOWN;

    if (decoder->principals) {
        /* Drop reference to JSPrincipals object */
        JSPRINCIPALS_DROP(cx, decoder->principals);
        decoder->principals = NULL;
    }

    /* Free any anonymous images. */
    if (decoder->image_context)
        ET_PostFreeAnonImages(context, decoder->image_context);

    /* Free all the HTML-based objects and properties. */
    lm_FreeWindowContent(decoder, JS_TRUE);
    JS_GC(cx);

    LM_PutMochaDecoder(decoder);
}
