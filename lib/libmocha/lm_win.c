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
 * JS reflection of the current Navigator Window.
 *
 * Brendan Eich, 9/8/95
 */
#include "rosetta.h"
#include "lm.h"
#include "xp.h"
#include "xpgetstr.h"
#include "structs.h"
#include "layout.h"             /* included via -I../layout */
#include "prtime.h"
#include "shist.h"
#include "ssl.h"
#include "libi18n.h"
#include "jsdbgapi.h"
#include "intl_csi.h"
#include "layers.h"
#include "base64.h"

#define IL_CLIENT
#include "libimg.h"             /* Image Library public API. */
#include "prthread.h"

#ifdef JAVA
#include "jsjava.h"
#endif

#undef FREE_AND_CLEAR           /* XXX over-including Mac compiled headers */

extern int XP_MSG_JS_CLOSE_WINDOW;
extern int XP_MSG_JS_CLOSE_WINDOW_NAME;

enum window_slot {
    WIN_LENGTH              = -1,
    WIN_FRAMES              = -2,
    WIN_PARENT              = -3,
    WIN_TOP                 = -4,
    WIN_SELF                = -5,
    WIN_NAME                = -6,
    WIN_STATUS              = -7,
    WIN_DEFAULT_STATUS      = -8,
    WIN_OPENER              = -9,
    WIN_CLOSED              = -10,
    WIN_WIDTH               = -11,
    WIN_HEIGHT              = -12,
    WIN_OUTWIDTH            = -13,
    WIN_OUTHEIGHT           = -14,
    WIN_XPOS                = -15,
    WIN_YPOS                = -16,
    WIN_XOFFSET             = -17,
    WIN_YOFFSET             = -18,
    WIN_SECURE              = -19,
    WIN_LOADING             = -20,
    WIN_FRAMERATE           = -21,
    WIN_OFFSCREEN_BUFFERING = -22
};

#define IS_INSECURE_SLOT(s) (WIN_LOADING <= (s) && (s) <= WIN_CLOSED)

static JSPropertySpec window_props[] = {
    {"length",        WIN_LENGTH,     JSPROP_ENUMERATE|JSPROP_READONLY},
    {"frames",        WIN_FRAMES,     JSPROP_ENUMERATE|JSPROP_READONLY},
    {"parent",        WIN_PARENT,     JSPROP_ENUMERATE|JSPROP_READONLY},
    {"top",           WIN_TOP,        JSPROP_ENUMERATE|JSPROP_READONLY},
    {"self",          WIN_SELF,       JSPROP_ENUMERATE|JSPROP_READONLY},
    {"window",        WIN_SELF,       JSPROP_READONLY},
    {"name",          WIN_NAME,       JSPROP_ENUMERATE},
    {"status",        WIN_STATUS,     JSPROP_ENUMERATE},
    {"defaultStatus", WIN_DEFAULT_STATUS, JSPROP_ENUMERATE},
    {lm_opener_str,   WIN_OPENER,     JSPROP_ENUMERATE},
    {lm_closed_str,   WIN_CLOSED,     JSPROP_ENUMERATE|JSPROP_READONLY},
    {"innerWidth",    WIN_WIDTH,      JSPROP_ENUMERATE},
    {"innerHeight",   WIN_HEIGHT,     JSPROP_ENUMERATE},
    {"outerWidth",    WIN_OUTWIDTH,   JSPROP_ENUMERATE},
    {"outerHeight",   WIN_OUTHEIGHT,  JSPROP_ENUMERATE},
    {"screenX",       WIN_XPOS,       JSPROP_ENUMERATE},
    {"screenY",       WIN_YPOS,       JSPROP_ENUMERATE},
    {"pageXOffset",   WIN_XOFFSET,    JSPROP_ENUMERATE|JSPROP_READONLY},
    {"pageYOffset",   WIN_YOFFSET,    JSPROP_ENUMERATE|JSPROP_READONLY},
    {"secure",        WIN_SECURE,     JSPROP_ENUMERATE|JSPROP_READONLY},
    {"frameRate",     WIN_FRAMERATE,  JSPROP_ENUMERATE},
    {"offscreenBuffering",
                      WIN_OFFSCREEN_BUFFERING,JSPROP_ENUMERATE},
    {0}
};

static Chrome *
win_get_chrome(MWContext *context, Chrome *chrome)
{
    if (!context->mocha_context)
        return NULL;

    /* All calls to this function must free the Chrome struct themselves!!! */
    chrome = JS_malloc(context->mocha_context, sizeof *chrome);
    if (!chrome)
        return NULL;
    XP_BZERO(chrome, sizeof *chrome);

    ET_PostQueryChrome(context, chrome);

    return chrome;
}

PR_STATIC_CALLBACK(PRBool)
win_getProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    MochaDecoder *decoder;
    MWContext *context;
    jsint count;
    Chrome *chrome = NULL;
    JSString * str;
    jsint slot;
    CL_OffscreenMode offscreen_mode;
    int status;

    while (!(decoder = JS_GetInstancePrivate(cx, obj, &lm_window_class, NULL))) {
        obj = JS_GetPrototype(cx, obj);
        if (!obj)
            return JS_TRUE;
    }

    /*
     * Allow anyone who can address this window to refer to its "window" and
     * "self" properties, because they refer to the window already in hand by
     * the accessing script.  Useful for layer scripts that 'import window.*'.
     */
    if (JSVAL_IS_INT(id) && JSVAL_TO_INT(id) == WIN_SELF) {
        *vp = OBJECT_TO_JSVAL(decoder->window_object);
        return JS_TRUE;
    }

    slot = JSVAL_IS_INT(id) ? JSVAL_TO_INT(id) : 0;

    if (!IS_INSECURE_SLOT(slot) &&
        !lm_CheckContainerAccess(cx, obj, decoder,
                                 JSTARGET_UNIVERSAL_BROWSER_READ)) {
        return JS_FALSE;
    }

    context = decoder->window_context;
    if (!context) {
        if (JSVAL_IS_INT(id) && JSVAL_TO_INT(id) == WIN_CLOSED)
            *vp = JSVAL_TRUE;
        return JS_TRUE;
    }

    if (!JSVAL_IS_INT(id))
        return JS_TRUE;

    switch (slot) {
      case WIN_LENGTH:
        *vp = INT_TO_JSVAL(XP_ListCount(context->grid_children));
        break;

      case WIN_FRAMES:
        *vp = OBJECT_TO_JSVAL(decoder->window_object);
        break;

      case WIN_PARENT:
        *vp = OBJECT_TO_JSVAL(decoder->window_object);
        if (context->grid_parent) {
            decoder = LM_GetMochaDecoder(context->grid_parent);
            if (decoder) {
                *vp = OBJECT_TO_JSVAL(decoder->window_object);
                LM_PutMochaDecoder(decoder);
            }
        }
        break;

      case WIN_TOP:
        while (context->grid_parent)
            context = context->grid_parent;
        decoder = LM_GetMochaDecoder(context);
        *vp = OBJECT_TO_JSVAL(decoder ? decoder->window_object : NULL);
        if (decoder)
            LM_PutMochaDecoder(decoder);
        break;

      case WIN_NAME:
        str = lm_LocalEncodingToStr(context, context->name);
        if (!str)
            return JS_FALSE;
        *vp = STRING_TO_JSVAL(str);
        break;

      case WIN_STATUS:
        return JS_TRUE; /* XXX can't get yet, return last known */

      case WIN_DEFAULT_STATUS:
        str = JS_NewStringCopyZ(cx, context->defaultStatus);
        if (!str)
            return JS_FALSE;
        *vp = STRING_TO_JSVAL(str);
        break;

      case WIN_OPENER:
        if (!JSVAL_IS_OBJECT(*vp) &&
            !JS_ConvertValue(cx, *vp, JSTYPE_OBJECT, vp)) {
            return JS_FALSE;
        }
        break;

      case WIN_CLOSED:
        *vp = JSVAL_FALSE;
        break;

      case WIN_WIDTH:
        if (!(chrome = win_get_chrome(context, chrome)))
            return JS_FALSE;
        *vp = INT_TO_JSVAL(chrome->w_hint);
        break;

      case WIN_HEIGHT:
        if (!(chrome = win_get_chrome(context, chrome)))
            return JS_FALSE;
        *vp = INT_TO_JSVAL(chrome->h_hint);
        break;

      case WIN_OUTWIDTH:
        if (!(chrome = win_get_chrome(context, chrome)))
            return JS_FALSE;
        *vp = INT_TO_JSVAL(chrome->outw_hint);
        break;

      case WIN_OUTHEIGHT:
        if (!(chrome = win_get_chrome(context, chrome)))
            return JS_FALSE;
        *vp = INT_TO_JSVAL(chrome->outh_hint);
        break;

      case WIN_XPOS:
        if (!(chrome = win_get_chrome(context, chrome)))
            return JS_FALSE;
        *vp = INT_TO_JSVAL(chrome->l_hint);
        break;

      case WIN_YPOS:
        if (!(chrome = win_get_chrome(context, chrome)))
            return JS_FALSE;
        *vp = INT_TO_JSVAL(chrome->t_hint);
        break;

      case WIN_XOFFSET:
        *vp = INT_TO_JSVAL(CL_GetCompositorXOffset(context->compositor));
        break;

      case WIN_YOFFSET:
        *vp = INT_TO_JSVAL(CL_GetCompositorYOffset(context->compositor));
        break;

      case WIN_SECURE:
      	*vp = JSVAL_FALSE;
      	HG99882
        break;

      case WIN_LOADING:
        decoder = LM_GetMochaDecoder(context);
        if (decoder && !decoder->load_event_sent)
            *vp = JSVAL_TRUE;
        else
            *vp = JSVAL_FALSE;
        if (decoder)
            LM_PutMochaDecoder(decoder);
        break;

      case WIN_FRAMERATE:
        *vp = INT_TO_JSVAL(CL_GetCompositorFrameRate(context->compositor));
        break;

      case WIN_OFFSCREEN_BUFFERING:
        offscreen_mode = CL_GetCompositorOffscreenDrawing(context->compositor);

        switch (offscreen_mode) {
          case CL_OFFSCREEN_ENABLED:
            *vp = JSVAL_TRUE;
            break;
          case CL_OFFSCREEN_DISABLED:
            *vp = JSVAL_FALSE;
            break;
          case CL_OFFSCREEN_AUTO:
            str = JS_NewStringCopyZ(cx, "auto");
            if (!str)
                return JS_FALSE;
            *vp = STRING_TO_JSVAL(str);
            break;
        }
        break;

      default:
        if (slot < 0) {
            /* Don't mess with user-defined or method properties. */
            return JS_TRUE;
        }

        /* XXX silly xp_cntxt.c puts newer contexts at the front! fix. */
        count = XP_ListCount(context->grid_children);
        context = XP_ListGetObjectNum(context->grid_children, count - slot);
        if (context) {
            decoder = LM_GetMochaDecoder(context);
            if (decoder) {
                *vp = OBJECT_TO_JSVAL(decoder->window_object);
                LM_PutMochaDecoder(decoder);
            } else {
                *vp = JSVAL_NULL;
            }
        }
        break;
    }
    if (chrome)
        JS_free(cx, chrome);

    return JS_TRUE;
}

PR_STATIC_CALLBACK(PRBool)
win_setProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    MochaDecoder *decoder, *parent_decoder;
    CL_OffscreenMode mode;
    JSBool enable_offscreen;
    jsdouble frame_rate, size;
    MWContext *context;
    Chrome *chrome = NULL;
    enum window_slot window_slot;
    const char *prop_name;
    char *str, *name, *old_name;
    jsint slot;
    int32 width, height;

    while (!(decoder = JS_GetInstancePrivate(cx, obj, &lm_window_class, NULL))) {
        obj = JS_GetPrototype(cx, obj);
        if (!obj)
            return JS_TRUE;
    }

    if (!lm_CheckContainerAccess(cx, obj, decoder,
                                 JSTARGET_UNIVERSAL_BROWSER_WRITE)) {
        return JS_FALSE;
    }

    context = decoder->window_context;
    if (!context)
        return JS_TRUE;

    if (!JSVAL_IS_INT(id)) {
        /* Due to the wonderful world of threads we need to know ahead of time if
         * someone is setting an onMouseMove event handler here or in document so
         * that we don't lose messages.*/
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

    window_slot = slot;
    switch (window_slot) {
      case WIN_NAME:
      case WIN_STATUS:
      case WIN_DEFAULT_STATUS:
        if (!JSVAL_IS_STRING(*vp) &&
            !JS_ConvertValue(cx, *vp, JSTYPE_STRING, vp)) {
            return JS_FALSE;
        }
        break;
      default:;
    }

    switch (window_slot) {
      case WIN_NAME:
        /* Don't let rogue JS name a mail or news window and then close it. */
        if (context->type != MWContextBrowser && context->type != MWContextPane)
            return JS_TRUE;
        name = lm_StrToLocalEncoding(context,  JSVAL_TO_STRING(*vp));
	if (!name)
	    return JS_FALSE;
        if (!lm_CheckWindowName(cx, name)) {
	    XP_FREE(name);
            return JS_FALSE;
	}
        old_name = context->name;
        if (old_name) {
            /* If context is a frame, change its name in its parent's scope. */
            if (context->grid_parent) {
                parent_decoder = LM_GetMochaDecoder(context->grid_parent);
                if (parent_decoder) {
                    JS_DeleteProperty(cx, parent_decoder->window_object,
                                         old_name);
                    LM_PutMochaDecoder(parent_decoder);
                }
            }
            XP_FREE(old_name);
        }
        context->name = name;
        break;

      case WIN_STATUS:
        ET_PostProgress(context, JS_GetStringBytes(JSVAL_TO_STRING(*vp)));
        break;

      case WIN_DEFAULT_STATUS:
        str = JS_strdup(cx, JS_GetStringBytes(JSVAL_TO_STRING(*vp)));
        if (!str)
            return JS_FALSE;
        if (context->defaultStatus)
            XP_FREE(context->defaultStatus);
        context->defaultStatus = str;
        ET_PostProgress(context, NULL);
        break;

      case WIN_OPENER:
        if (decoder->opener && !JSVAL_TO_OBJECT(*vp))
            decoder->opener = NULL;
        break;

      case WIN_WIDTH:
        if (context->grid_parent)   
            return JS_TRUE;
        if (!JS_ValueToNumber(cx, *vp, &size))
            return JS_FALSE;
        if (!(chrome = win_get_chrome(decoder->window_context, chrome)))
            return JS_FALSE;
        chrome->w_hint = (int32)size;
        chrome->outw_hint = chrome->outh_hint = 0;
        /* Minimum window size is 100 x 100 without security */
        if (chrome->w_hint < 100) {
            if (!lm_CanAccessTarget(cx, JSTARGET_UNIVERSAL_BROWSER_WRITE))
                chrome->w_hint = 100;
        }
        chrome->w_hint = (chrome->w_hint < 10) ? 10 : chrome->w_hint;
        ET_PostUpdateChrome(decoder->window_context, chrome);
        break;

      case WIN_HEIGHT:
        if (context->grid_parent)   
            return JS_TRUE;
        if (!JS_ValueToNumber(cx, *vp, &size))
            return JS_FALSE;
        if (!(chrome = win_get_chrome(decoder->window_context, chrome)))
            return JS_FALSE;
        chrome->h_hint = (int32)size;
        chrome->outw_hint = chrome->outh_hint = 0;
        /* Minimum window size is 100 x 100 without security */
        if (chrome->h_hint < 100) {
            if (!lm_CanAccessTarget(cx, JSTARGET_UNIVERSAL_BROWSER_WRITE))
                chrome->h_hint = 100;
        }
        chrome->h_hint = (chrome->h_hint < 10) ? 10 : chrome->h_hint;
        ET_PostUpdateChrome(decoder->window_context, chrome);
        break;

      case WIN_OUTWIDTH:
        if (!JS_ValueToNumber(cx, *vp, &size))
            return JS_FALSE;
        if (!(chrome = win_get_chrome(decoder->window_context, chrome)))
            return JS_FALSE;
        chrome->outw_hint = (int32)size;
        /* Minimum window size is 100 x 100 without security */
        if (chrome->outw_hint < 100) {
            if (!lm_CanAccessTarget(cx, JSTARGET_UNIVERSAL_BROWSER_WRITE))
                chrome->outw_hint = 100;
        }
        chrome->outw_hint = (chrome->outw_hint < 10) ? 10 : chrome->outw_hint;
        ET_PostUpdateChrome(decoder->window_context, chrome);
        break;

      case WIN_OUTHEIGHT:
        if (!JS_ValueToNumber(cx, *vp, &size))
            return JS_FALSE;
        if (!(chrome = win_get_chrome(decoder->window_context, chrome)))
            return JS_FALSE;
        chrome->outh_hint = (int32)size;
        /* Minimum window size is 100 x 100 without security */
        if (chrome->outh_hint < 100) { 
            if (!lm_CanAccessTarget(cx, JSTARGET_UNIVERSAL_BROWSER_WRITE))
                chrome->outh_hint = 100;
        }
        chrome->outh_hint = (chrome->outh_hint < 10) ? 10 : chrome->outh_hint;
        ET_PostUpdateChrome(decoder->window_context, chrome);
        break;

      case WIN_XPOS:
        if (!JS_ValueToNumber(cx, *vp, &size))
            return JS_FALSE;
        if (!(chrome = win_get_chrome(decoder->window_context, chrome)))
            return JS_FALSE;
        chrome->l_hint = (int32)size;
        /* Windows must be positioned on screen without security */
        ET_PostGetScreenSize(decoder->window_context, &width, &height);
        if ((width < chrome->l_hint + chrome->outw_hint)||(chrome->l_hint < 0)){
            if (!lm_CanAccessTarget(cx, JSTARGET_UNIVERSAL_BROWSER_WRITE)) {
                chrome->l_hint = (width < chrome->l_hint + chrome->outw_hint) ?
                                    width - chrome->outw_hint : chrome->l_hint;
                chrome->l_hint = (chrome->l_hint < 0) ? 0 : chrome->l_hint;
            }
        }
        ET_PostUpdateChrome(decoder->window_context, chrome);
        break;

      case WIN_YPOS:
        if (!JS_ValueToNumber(cx, *vp, &size))
            return JS_FALSE;
        if (!(chrome = win_get_chrome(decoder->window_context, chrome)))
            return JS_FALSE;
        chrome->t_hint = (int32)size;
        /* Windows must be positioned on screen without security */
        ET_PostGetScreenSize(decoder->window_context, &width, &height);
        if ((height < chrome->t_hint + chrome->outh_hint)||(chrome->t_hint < 0)){
            if (!lm_CanAccessTarget(cx, JSTARGET_UNIVERSAL_BROWSER_WRITE)) {
                chrome->t_hint = (height < chrome->t_hint + chrome->outh_hint) ?
                                    height - chrome->outh_hint : chrome->t_hint;
                chrome->t_hint = (chrome->t_hint < 0) ? 0 : chrome->t_hint;
            }
        }
        ET_PostUpdateChrome(decoder->window_context, chrome);
        break;

      case WIN_FRAMERATE:
        if (!JS_ValueToNumber(cx, *vp, &frame_rate))
            return JS_FALSE;
        CL_SetCompositorFrameRate(context->compositor, (uint32) frame_rate);
        break;

      case WIN_OFFSCREEN_BUFFERING:
        if (!JS_ValueToBoolean(cx, *vp, &enable_offscreen))
            return JS_FALSE;
        mode = enable_offscreen ? CL_OFFSCREEN_ENABLED : CL_OFFSCREEN_DISABLED;
        CL_SetCompositorOffscreenDrawing(context->compositor, mode);
        break;

      default:;
    }
    if (chrome)
        JS_free(cx, chrome);

    return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
win_list_properties(JSContext *cx, JSObject *obj)
{
    MochaDecoder *decoder;
    MWContext *context, *kid;
    XP_List *list;
    uint slot;

    decoder = JS_GetPrivate(cx, obj);
    if (!decoder)
        return JS_TRUE;
    context = decoder->window_context;
    if (!context)
        return JS_TRUE;

    /* xp_cntxt.c puts newer contexts at the front! deal. */
    list = context->grid_children;
    slot = XP_ListCount(list);
    while ((kid = XP_ListNextObject(list)) != NULL) {
        slot--;
        if (!JS_DefineProperty(cx, decoder->window_object, kid->name,
                               JSVAL_NULL, NULL, NULL,
                               JSPROP_ENUMERATE|JSPROP_READONLY))
            return JS_FALSE;
    }
    return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
win_resolve_name(JSContext *cx, JSObject *obj, jsval id)
{
    char * name = NULL;
    MochaDecoder *decoder;
    MWContext *context, *kid;
    XP_List *list;
    jsint slot;
    JSObject *window_obj;
    JSBool rv = JS_TRUE;

    /* Don't resolve any of names if id is on the left side of an = op. */
    if (JS_IsAssigning(cx))
        return JS_TRUE;

    if (!JSVAL_IS_STRING(id))
        return JS_TRUE;

    decoder = JS_GetPrivate(cx, obj);
    if (!decoder)
        return JS_TRUE;
    context = decoder->window_context;
    if (!context)
        return JS_TRUE;

    name = lm_StrToLocalEncoding(context, JSVAL_TO_STRING(id));
    if (!name)
	return JS_TRUE;

    /* xp_cntxt.c puts newer contexts at the front! deal. */
    list = context->grid_children;
    slot = XP_ListCount(list);
    while ((kid = XP_ListNextObject(list)) != NULL) {
        slot--;
        if (kid->name && XP_STRCMP(kid->name, name) == 0) {
            window_obj = decoder->window_object;
            if (!JS_DefinePropertyWithTinyId(cx, window_obj,
                                             kid->name, (int8)slot, JSVAL_NULL,
                                             NULL, NULL,
                                             JSPROP_ENUMERATE|JSPROP_READONLY))
		{
		    rv = JS_FALSE;
		    goto done;
		}
            if (!JS_AliasElement(cx, window_obj, kid->name, slot)) {
		rv = JS_FALSE;
		goto done;
	    }
            goto done;
        }
    }

    XP_FREE(name);
    return lm_ResolveWindowProps(cx, decoder, obj, id);

done:
    XP_FREE(name);
    return rv;
}

JSBool
lm_ResolveWindowProps(JSContext *cx, MochaDecoder *decoder, JSObject *obj,
                      jsval id)
{
    const char * name;

    if (!JSVAL_IS_STRING(id))
        return JS_TRUE;
    name = JS_GetStringBytes(JSVAL_TO_STRING(id));

    if (!XP_STRCMP(name, "screen"))
        return ((JSBool) (lm_DefineScreen(decoder, obj) != NULL));

    if (!XP_STRCMP(name, "hardware"))
        return ((JSBool) (lm_DefineHardware(decoder, obj) != NULL));

    if (!XP_STRCMP(name, "loading"))
        return (JS_DefinePropertyWithTinyId(cx, obj, name, WIN_LOADING, 
                                            JSVAL_VOID, NULL, NULL, 
                                            JSPROP_ENUMERATE));

    if (!XP_STRCMP(name, lm_navigator_str)) {
        /* see if there is a global navigator object yet */
        if (!lm_crippled_decoder->navigator) {
            lm_DefinePluginClasses(lm_crippled_decoder);
            lm_crippled_decoder->navigator = lm_DefineNavigator(lm_crippled_decoder);
            if (!lm_crippled_decoder->navigator)
                return JS_FALSE;
            if (!JS_AddRoot(cx, &lm_crippled_decoder->navigator))
                return JS_FALSE;
        }

        /* use the global navigator */
        decoder->navigator = lm_crippled_decoder->navigator;
        if (!JS_DefineProperty(cx, obj, lm_navigator_str,
                               OBJECT_TO_JSVAL(decoder->navigator),
                               NULL, NULL,
                               JSPROP_ENUMERATE | JSPROP_READONLY)) {
            return JS_FALSE;
        }

    }

    if (!XP_STRCMP(name, lm_components_str)) {
        /* see if there is a global components object yet */
        if (!lm_crippled_decoder->components) {
            lm_crippled_decoder->components =
                lm_DefineComponents(lm_crippled_decoder);
            if (!lm_crippled_decoder->components)
                return JS_FALSE;
            if (!JS_AddRoot(cx, &lm_crippled_decoder->components))
                return JS_FALSE;
        }

        /* use the global navigator */
        decoder->components = lm_crippled_decoder->components;
        if (!JS_DefineProperty(cx, obj, lm_components_str,
                               OBJECT_TO_JSVAL(decoder->components),
                               NULL, NULL,
                               JSPROP_ENUMERATE | JSPROP_READONLY)) {
            return JS_FALSE;
        }
    }

    return lm_ResolveBar(cx, decoder, name);
}

PR_STATIC_CALLBACK(void)
win_finalize(JSContext *cx, JSObject *obj)
{
    MochaDecoder *decoder;

    decoder = JS_GetPrivate(cx, obj);
    if (!decoder)
        return;
    decoder->window_object = NULL;

    /*
     * If the decoder is going down and no one has a ref to
     *   us run GC in LM_PutMochaDecoder() else destroy
     *   the context here because its not being used by
     *   whomever is finalizing us (i.e. a ref to a window
     *   from another)
     */
    DROP_BACK_COUNT(decoder);
}

JSBool win_check_access(JSContext *cx, JSObject *obj, jsval id,
                        JSAccessMode mode, jsval *vp)
{
    if(mode == JSACC_PARENT)  {
        return lm_CheckSetParentSlot(cx, obj, id, vp);
    }
    return JS_TRUE;
}

JSClass lm_window_class = {
    "Window", JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub, win_getProperty, win_setProperty,
    win_list_properties, win_resolve_name, JS_ConvertStub, win_finalize,
    NULL, win_check_access
};

/*
 * Alert and some simple dialogs.
 */
PR_STATIC_CALLBACK(PRBool)
win_alert(JSContext *cx, JSObject *obj, uint argc, jsval *argv, jsval *rval)
{
    MochaDecoder *decoder;
    MWContext *context;
    char *message, *platform_message;
    JSString * str;

    if (!(decoder = JS_GetInstancePrivate(cx, obj, &lm_window_class, argv)))
        return JS_FALSE;
    if (!(str = JS_ValueToString(cx, argv[0])))
        return JS_FALSE;

    /* if there is no context let the script continue */
    context = decoder->window_context;
    if (!context)
	return JS_TRUE;

    message = lm_StrToLocalEncoding(context, str);
    
    if (message) {
        platform_message = lm_FixNewlines(cx, message, JS_FALSE); 
        ET_PostMessageBox(context, platform_message, JS_FALSE);
        XP_FREEIF(platform_message);
	XP_FREE(message);
    }
    return JS_TRUE;
}

PR_STATIC_CALLBACK(PRBool)
win_confirm(JSContext *cx, JSObject *obj, uint argc, jsval *argv, jsval *rval)
{
    MochaDecoder *decoder;
    MWContext *context;
    JSString * str;
    char *message;
    JSBool ok;

    decoder = JS_GetInstancePrivate(cx, obj, &lm_window_class, argv);
    if (!decoder)
        return JS_FALSE;
    if (!(str = JS_ValueToString(cx, argv[0])))
        return JS_FALSE;

    context = decoder->window_context;
    if (context) {
	char *platform_message;
	message = lm_StrToLocalEncoding(context, str);
	if (!message) {
	    *rval = JSVAL_FALSE;
	    return JS_TRUE;
	}
        platform_message = lm_FixNewlines(cx, message, JS_FALSE);
        ok = ET_PostMessageBox(context, platform_message, JS_TRUE);
        XP_FREEIF(platform_message);
	XP_FREE(message);
    }
    else {
        ok = JS_FALSE;
    }
    *rval = BOOLEAN_TO_JSVAL(ok);
    return JS_TRUE;
}

PR_STATIC_CALLBACK(PRBool)
win_prompt(JSContext *cx, JSObject *obj, uint argc, jsval *argv, jsval *rval)
{
    MochaDecoder *decoder;
    MWContext *context;
    jsval arg;
    JSString *str;
    char *retval;

    if (!(decoder = JS_GetInstancePrivate(cx, obj, &lm_window_class, argv)))
        return JS_FALSE;
    arg = argv[0];

    context = decoder->window_context;
    if (context) {
	char *query, *defval, *platform_query;

	/*
	 * Build the query_string
	 */
	if (!(str = JS_ValueToString(cx, argv[0])))
	    return JS_FALSE;
	query = lm_StrToLocalEncoding(context, str);
	if (!query) {
	    JS_ReportOutOfMemory(cx);
	    return JS_FALSE;
	}
        platform_query = lm_FixNewlines(cx, query, JS_FALSE);

        /*
	 * Build the default value
	 */
	if (!(str = JS_ValueToString(cx, argv[1]))) {
            return JS_FALSE;
        }
	defval = lm_StrToLocalEncoding(context, str);

        retval = ET_PostPrompt(context, platform_query, defval);
	XP_FREEIF(query);
	XP_FREEIF(defval);
        XP_FREEIF(platform_query);
    } 
    else {
        retval = NULL;
    }

    if (!retval) {
        *rval = JSVAL_NULL;
        return JS_TRUE;
    }

    XP_ASSERT(context);
    str = lm_LocalEncodingToStr(context, retval);
    XP_FREE(retval);
    if (!str)
        return JS_FALSE;
    *rval = STRING_TO_JSVAL(str);
    return JS_TRUE;
}

/*
 * Open and close of a named window.
 */
JSBool
lm_CheckWindowName(JSContext *cx, const char *window_name)
{
    const char *cp;

    for (cp = window_name; *cp != '\0'; cp++) {
        if (!XP_IS_ALPHA(*cp) && !XP_IS_DIGIT(*cp) && *cp != '_') {
            JS_ReportError(cx,
                "illegal character '%c' ('\\%o') in window name %s",
                *cp, *cp, window_name);
            return JS_FALSE;
        }
    }
    return JS_TRUE;
}

static int32
win_has_option(char *options, char *name)
{
    char *comma, *equal;
    int32 found = 0;

    for (;;) {
        comma = XP_STRCHR(options, ',');
        if (comma) *comma = '\0';
        equal = XP_STRCHR(options, '=');
        if (equal) *equal = '\0';
        if (XP_STRCASECMP(options, name) == 0) {
            if (!equal || XP_STRCASECMP(equal + 1, "yes") == 0)
                found = 1;
            else
                found = XP_ATOI(equal + 1);
        }
        if (equal) *equal = '=';
        if (comma) *comma = ',';
        if (found || !comma)
            break;
        options = comma + 1;
    }
    return found;
}

/* These apply to top-level windows only, not to frames */
static uint lm_window_count = 0;
static uint lm_window_limit = 100;

/* XXX this can't be static yet, it's called by lm_doc.c/doc_open */
JSBool PR_CALLBACK
win_open(JSContext *cx, JSObject *obj,
         uint argc, jsval *argv, jsval *rval)
{
    MochaDecoder *decoder, *new_decoder;
    URL_Struct *url_struct;
    JSString *str, *window_name_str;
    const char *url_string;
    char *window_name = NULL;
    char *options;
    Chrome *chrome = NULL;
    MWContext *old_context, *context;
    int32 width, height;
    int32 win_width, win_height;

    if (!(decoder = JS_GetInstancePrivate(cx, obj, &lm_window_class, argv)))
        return JS_FALSE;

    /* Make url_string absolute based on current document's base URL. */
    url_struct = NULL;
    if (argc > 0) {
        if (!(str = JS_ValueToString(cx, argv[0])))
            return JS_FALSE;
        url_string = JS_GetStringBytes(str);
        if (*url_string != '\0') {
            url_string = lm_CheckURL(cx, url_string, JS_TRUE);
            if (url_string) {
                const char *referer;
                url_struct = NET_CreateURLStruct(url_string, NET_DONT_RELOAD);
                XP_FREE((char *)url_string);
                if (!url_struct) {
                    JS_ReportOutOfMemory(cx);
                    return JS_FALSE;
                }
                if (!(referer = lm_GetSubjectOriginURL(cx)) ||
		    !(url_struct->referer = JS_strdup(cx, referer))) {
		    NET_FreeURLStruct(url_struct);
		    return JS_FALSE;
		}
            }
        }
        if (!url_string)
            return JS_FALSE;
    }

    /* Set this to null so we can goto fail from here onward. */
    new_decoder = NULL;

    /* Sanity-check the optional window_name argument. */
    if (argc > 1) {
        if (!(window_name_str = JS_ValueToString(cx, argv[1])))
            goto fail;
        if (!JS_LockGCThing(cx, window_name_str))
            goto fail;
	/* XXXunicode ? */
        window_name = lm_StrToLocalEncoding(decoder->window_context, window_name_str);
        if (!lm_CheckWindowName(cx, window_name))
            goto fail;
    } else {
        window_name_str = NULL;
        window_name = NULL;
    }

    /* Check for window chrome options. */
    chrome = XP_NEW_ZAP(Chrome);
    if(chrome == NULL)
        goto fail;
    if (argc > 2) {
        if (!(str = JS_ValueToString(cx, argv[2])))
            goto fail;
        options = JS_GetStringBytes(str);

        chrome->show_button_bar        = win_has_option(options, "toolbar");
        chrome->show_url_bar           = win_has_option(options, "location");
        chrome->show_directory_buttons =
            win_has_option(options, "directories") | win_has_option(options, "personalbar");
        chrome->show_bottom_status_bar = win_has_option(options, "status");
        chrome->show_menu              = win_has_option(options, "menubar");
        chrome->show_security_bar      = FALSE;
        chrome->w_hint                 =
            win_has_option(options, "innerWidth") | win_has_option(options, "width");
        chrome->h_hint                 =
            win_has_option(options, "innerHeight") | win_has_option(options, "height");
        chrome->outw_hint              = win_has_option(options, "outerWidth");
        chrome->outh_hint              = win_has_option(options, "outerHeight");
        chrome->l_hint                 =
            win_has_option(options, "left") | win_has_option(options, "screenX");
        chrome->t_hint                 =
            win_has_option(options, "top") | win_has_option(options, "screenY");
        chrome->show_scrollbar         = win_has_option(options, "scrollbars");
        chrome->allow_resize           = win_has_option(options, "resizable");
        chrome->allow_close            = TRUE;
        chrome->dependent              = win_has_option(options, "dependent");
        chrome->copy_history           = FALSE; /* XXX need strong trust */
        chrome->topmost         = win_has_option(options, "alwaysRaised");
        chrome->bottommost              = win_has_option(options, "alwaysLowered");
        chrome->z_lock          = win_has_option(options, "z-lock");
        chrome->is_modal            = win_has_option(options, "modal");
        chrome->hide_title_bar  = !(win_has_option(options, "titlebar"));

        /* Allow disabling of commands only if there is no menubar */
        if (!chrome->show_menu) {
            chrome->disable_commands = !win_has_option(options, "hotkeys");
            if (XP_STRCASESTR(options,"hotkeys")==NULL)
                chrome->disable_commands = FALSE;
        }
        /* If titlebar condition not specified, default to shown */
        if (XP_STRCASESTR(options,"titlebar")==0)
            chrome->hide_title_bar=FALSE;

        if (chrome->topmost || chrome->bottommost || 
            chrome->z_lock || chrome->is_modal || 
            chrome->hide_title_bar || chrome->disable_commands) {
            if (!lm_CanAccessTarget(cx, JSTARGET_UNIVERSAL_BROWSER_WRITE)) {
                chrome->topmost = chrome->bottommost =
                chrome->z_lock = chrome->is_modal =
                chrome->hide_title_bar = chrome->disable_commands = 0;
            }
        }

        /* In order to not start Java for every single window open we 
	 * have to first check if we need to check, and then check.  
         * Start by getting width and height to use for positioning 
	 * calculations. Defaults to 100 if neither are specified.
	 * Then get screen size.
	 */
	win_width = chrome->w_hint ? chrome->w_hint : 
			(chrome->outw_hint ? chrome->outw_hint : 100);
	win_height = chrome->h_hint ? chrome->h_hint : 
			(chrome->outh_hint ? chrome->outh_hint : 100);
        ET_PostGetScreenSize(decoder->window_context, &width, &height);
        if ((chrome->w_hint && chrome->w_hint < 100) ||
            (chrome->h_hint && chrome->h_hint < 100) ||
            (chrome->outw_hint && chrome->outw_hint < 100) ||
            (chrome->outh_hint && chrome->outh_hint < 100) ||
            (width < chrome->l_hint + win_width) ||
            (chrome->l_hint < 0) ||
            (height < chrome->t_hint + win_height) ||
            (chrome->t_hint < 0)) {
            /* The window is trying to put a window offscreen or make it too
             * small.  We have to check the security permissions 
             */
            if (!lm_CanAccessTarget(cx, JSTARGET_UNIVERSAL_BROWSER_WRITE)) {
                /* Make sure windows are at least 100 by 100 pixels. */
                if (chrome->w_hint && chrome->w_hint < 100) {
		    chrome->w_hint = 100;
		    win_width = 100;
		}
                if (chrome->h_hint && chrome->h_hint < 100) {
		    chrome->h_hint = 100;
		    win_height = 100;
		}
                if (chrome->outw_hint && chrome->outw_hint < 100) {
		    chrome->outw_hint = 100;
		    win_width = 100;
		}
                if (chrome->outh_hint && chrome->outh_hint < 100) {
		    chrome->outh_hint = 100;
		    win_height = 100;
		}
                /* Windows must be positioned on screen without security */
                chrome->l_hint = (width < chrome->l_hint + win_width) ?
                                    width - win_width : chrome->l_hint;
                chrome->l_hint = (chrome->l_hint < 0) ? 0 : chrome->l_hint;
                chrome->t_hint = (height < chrome->t_hint + win_height) ?
                                    height - win_height : chrome->t_hint;
                chrome->t_hint = (chrome->t_hint < 0) ? 0 : chrome->t_hint;
            }
        } 
        /* Make sure they always at least 10 x 10 regardless of security. 1 x 1
         * windows are really hard to spot */
        if (chrome->w_hint && chrome->w_hint < 10) chrome->w_hint = 10;
        if (chrome->h_hint && chrome->h_hint < 10) chrome->h_hint = 10;
        if (chrome->outw_hint && chrome->outw_hint < 10) chrome->outw_hint = 10;
        if (chrome->outh_hint && chrome->outh_hint < 10) chrome->outh_hint = 10;

        /* You must specify both width and height to get either */
        if (chrome->w_hint == 0 || chrome->h_hint == 0)
            chrome->w_hint = chrome->h_hint = 0;
        if (chrome->outw_hint == 0 || chrome->outh_hint == 0)
            chrome->outw_hint = chrome->outh_hint = 0;

        /* Needed to allow positioning of windows at 0,0 */
        if ((XP_STRCASESTR(options,"top") || XP_STRCASESTR(options,"left") ||
                    XP_STRCASESTR(options,"screenX") || XP_STRCASESTR(options,"screenY")) != 0)
            chrome->location_is_chrome=TRUE;

        options = 0;
    } else {
        /* Make a fully chromed window, but don't copy history. */
        chrome->show_button_bar        = TRUE;
        chrome->show_url_bar           = TRUE;
        chrome->show_directory_buttons = TRUE;
        chrome->show_bottom_status_bar = TRUE;
        chrome->show_menu              = TRUE;
        chrome->show_security_bar      = FALSE;
        chrome->w_hint = chrome->h_hint = 0;
        chrome->is_modal               = FALSE;
        chrome->show_scrollbar         = TRUE;
        chrome->allow_resize           = TRUE;
        chrome->allow_close            = TRUE;
        chrome->copy_history           = FALSE; /* XXX need strong trust */
    }

    /* Windows created by JS cannot be randomly used by Mail/News */
    chrome->restricted_target = TRUE;
        
    old_context = decoder->window_context;
    if (!old_context)
        goto fail;
    if (window_name)
        context = XP_FindNamedContextInList(old_context, (char*)window_name);
    else
        context = NULL;

    if (context) {
        new_decoder = LM_GetMochaDecoder(context);
        if (!new_decoder)
            goto fail;
        if (url_struct && !lm_GetURL(cx, new_decoder, url_struct)) {
            url_struct = 0;
            goto fail;
        }
        /* lm_GetURL() stashed a url_struct pointer, and owns it now. */
        url_struct = 0;

	/* If specific options are specified we will update the named
	 * window to match those options.  If not, we won't change them */
	if (argc > 2)
	    ET_PostUpdateChrome(context, chrome);
    } else {
        if (lm_window_count >= lm_window_limit)
            goto fail;
        context = ET_PostNewWindow(old_context, url_struct,
                                   (char*)window_name,
                                   chrome);
        if (!context)
            goto fail;
        /* ET_PostNewWindow() stashed a url_struct pointer, and owns it now. */
        url_struct = 0;
        new_decoder = LM_GetMochaDecoder(context);
        if (!new_decoder) {
            (void) ET_PostDestroyWindow(context);
            goto fail;
        }
    }

    new_decoder->opener = obj;
    if (!JS_DefinePropertyWithTinyId(cx, new_decoder->window_object, 
                                     lm_opener_str, WIN_OPENER, 
                                     OBJECT_TO_JSVAL(obj),
                                     NULL, NULL, JSPROP_ENUMERATE)) {
        goto fail;
    }
    JS_UnlockGCThing(cx, window_name_str);
    new_decoder->in_window_quota = JS_TRUE;
    lm_window_count++;
    LM_PutMochaDecoder(new_decoder);
    *rval = OBJECT_TO_JSVAL(new_decoder->window_object);
    XP_FREE(chrome);
    XP_FREEIF(window_name);
    return JS_TRUE;

fail:
    if (window_name_str)
        JS_UnlockGCThing(cx, window_name_str);
    if (new_decoder)
        LM_PutMochaDecoder(new_decoder);
    if (url_struct)
        NET_FreeURLStruct(url_struct);
    XP_FREEIF(chrome);
    XP_FREEIF(window_name);
    *rval = JSVAL_NULL;
    return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
win_set_zoptions(JSContext *cx, JSObject *obj,
         uint argc, jsval *argv, jsval *rval)
{
    MochaDecoder *decoder;
    JSString *str;
    char *options;
    Chrome *chrome = NULL;

    if (!(decoder = JS_GetInstancePrivate(cx, obj, &lm_window_class, argv)))
        return JS_FALSE;
    if (argc != 1) {
        JS_ReportError(cx, lm_argc_err_str);
        return JS_FALSE;
    }
    if (!decoder->window_context)
        return JS_TRUE;

    /* Check for window chrome options. */
    chrome = win_get_chrome(decoder->window_context, chrome);
    if(chrome == NULL)
        return JS_FALSE;

    if (!(str = JS_ValueToString(cx, argv[0])))
        return JS_TRUE;
    options = JS_GetStringBytes(str);

    if (lm_CanAccessTarget(cx, JSTARGET_UNIVERSAL_BROWSER_WRITE)) {
        chrome->topmost         = win_has_option(options, "alwaysRaised");
        chrome->bottommost      = win_has_option(options, "alwaysLowered");
        chrome->z_lock          = win_has_option(options, "z-lock");
    }

    ET_PostUpdateChrome(decoder->window_context, chrome);

    options = 0;
    JS_free(cx, chrome);
    return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
win_set_hotkeys(JSContext *cx, JSObject *obj,
         uint argc, jsval *argv, jsval *rval)
{
    MochaDecoder *decoder;
    Chrome *chrome = NULL;

    if (!(decoder = JS_GetInstancePrivate(cx, obj, &lm_window_class, argv)))
        return JS_FALSE;
    if (argc != 1) {
        JS_ReportError(cx, lm_argc_err_str);
        return JS_FALSE;
    }
    if (!decoder->window_context)
        return JS_TRUE;

    /* Check for window chrome options. */
    chrome = win_get_chrome(decoder->window_context, chrome);
    if(chrome == NULL)
        return JS_FALSE;

    if(JSVAL_IS_BOOLEAN(argv[0])) {
        if (lm_CanAccessTarget(cx, JSTARGET_UNIVERSAL_BROWSER_WRITE)) {
            if (!chrome->show_menu) {
                chrome->disable_commands = !JSVAL_TO_BOOLEAN(argv[0]);
            }
        }
    }

    ET_PostUpdateChrome(decoder->window_context, chrome);

    JS_free(cx, chrome);
    return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
win_set_resizable(JSContext *cx, JSObject *obj,
         uint argc, jsval *argv, jsval *rval)
{
    MochaDecoder *decoder;
    Chrome *chrome = NULL;

    if (!(decoder = JS_GetInstancePrivate(cx, obj, &lm_window_class, argv)))
        return JS_FALSE;
    if (argc != 1) {
        JS_ReportError(cx, lm_argc_err_str);
        return JS_FALSE;
    }
    if (!decoder->window_context)
        return JS_TRUE;

    /* Check for window chrome options. */
    chrome = win_get_chrome(decoder->window_context, chrome);
    if(chrome == NULL)
        return JS_FALSE;

    if(JSVAL_IS_BOOLEAN(argv[0])) {
        chrome->allow_resize = JSVAL_TO_BOOLEAN(argv[0]);
    }

    ET_PostUpdateChrome(decoder->window_context, chrome);

    JS_free(cx, chrome);
    return JS_TRUE;
}

static void
destroy_window(void *closure)
{
    MWContext * context = (MWContext *)
		    ((MozillaEvent_Timeout *)closure)->pClosure;
    ET_PostDestroyWindow(context);
}

PR_STATIC_CALLBACK(PRBool)
win_close(JSContext *cx, JSObject *obj, uint argc, jsval *argv, jsval *rval)
{
    MochaDecoder *decoder;
    MochaDecoder *running_decoder;
    MWContext *context;
    char *message;
    JSBool ok;

    if (!(decoder = JS_GetInstancePrivate(cx, obj, &lm_window_class, argv)))
        return JS_FALSE;
    context = decoder->window_context;
    if (!context || context->grid_parent)
        return JS_TRUE;
    running_decoder = JS_GetPrivate(cx, JS_GetGlobalObject(cx));
    /* Bypass close window check if script is signed */
    if (context->type != MWContextBrowser && context->type != MWContextPane ||
        (!decoder->opener &&
         (XP_ListCount(SHIST_GetList(context)) > 1 ||
          decoder != running_decoder)) &&
          !lm_CanAccessTarget(cx, JSTARGET_UNIVERSAL_BROWSER_WRITE)) {
        /*
         * Prevent this window.close() call if this window
         * - is not a browser window, or
         * - was not opened by javascript, and
         * - has session history other than the current document, or
         * - does not contain the script that is closing this window.
         * - has no security privileges.
         */
        if (context->name)
            message = PR_smprintf(XP_GetString(XP_MSG_JS_CLOSE_WINDOW_NAME),
                                  context->name);
        else
            message = XP_GetString(XP_MSG_JS_CLOSE_WINDOW);
        ok = ET_PostMessageBox(context, message, JS_TRUE);
        if (context->name)
            XP_FREE(message);
        if (!ok)
            return JS_TRUE;
    }
    if(!decoder->called_win_close) {
        decoder->called_win_close = JS_TRUE;
	/* 
	 * If we are closing the window that the script is running in
	 *   wait until the script has finished running before sending
	 *   the close message
	 */
	if (decoder == running_decoder)
	    ET_PostSetTimeout(destroy_window, context, 0, XP_DOCID(context));
	else
            ET_PostDestroyWindow(context);

    }

    return JS_TRUE;
}

/*
 * Timeout support.
 */
static uint  lm_timeout_count = 0;
static uint  lm_timeout_limit = 1000;

#define HOLD_TIMEOUT(cx,to) ((to)->ref_count++)
#define DROP_TIMEOUT(decoder,to)                                             \
    NSPR_BEGIN_MACRO                                                         \
        XP_ASSERT((to)->ref_count > 0);                                      \
        if (--(to)->ref_count == 0)                                          \
            free_timeout(decoder,to);                                        \
    NSPR_END_MACRO

static void
free_timeout(MochaDecoder *decoder, JSTimeout *timeout)
{
    MWContext *context;
    JSContext *cx;

    cx = decoder->js_context;
    context = decoder->window_context;
    if (context) {
        XP_ASSERT(context->js_timeouts_pending > 0);
        context->js_timeouts_pending--;
    }

    if (timeout->expr)
        JS_free(cx, timeout->expr);
    else if (timeout->funobj) {
        JS_RemoveRoot(cx, &timeout->funobj);
        if (timeout->argv) {
            int i;
            for (i = 0; i < timeout->argc; i++)
                JS_RemoveRoot(cx, &timeout->argv[i]);
            JS_free(cx, timeout->argv);
        }
    }
    if (timeout->principals)
        JSPRINCIPALS_DROP(cx, timeout->principals);
    XP_FREEIF(timeout->filename);
    JS_free(cx, timeout);
    lm_timeout_count--;
}

/* Re-insert timeout before the first list item with greater deadline, or at
 * the head if the list is empty, or at the tail if there is no item with
 * greater deadline.
 * Through the righteousness of double-indirection we do this without two
 * ugly if-tests.
 */
static void
insert_timeout_into_list(JSTimeout **listp, JSTimeout *timeout)
{
    JSTimeout *to;

    while ((to = *listp) != NULL) {
        if (LL_CMP(to->when, >, timeout->when))
            break;
        listp = &to->next;
    }
    timeout->next = to;
    *listp = timeout;
}

static JSTimeout *js_timeout_running = NULL;
static JSTimeout **js_timeout_insertion_point = NULL;

static void
win_run_timeout(void *closure)
{
    JSTimeout *timeout, *next;
    JSTimeout *last_expired_timeout;
    JSTimeout dummy_timeout;
    MochaDecoder *decoder;
    JSContext *cx;
    int64 now;
    jsval result;
    MWContext * context = (MWContext *)((MozillaEvent_Timeout *)closure)->pClosure;
    void *timer_id;

    /* make sure the context hasn't disappeared out from under us */
    if (!XP_IsContextInList(context))
        return;

    timer_id = ((MozillaEvent_Timeout *)closure)->pTimerId;

    decoder = LM_GetMochaDecoder(context);
    if (!decoder)
        return;

    if (!decoder->timeouts) {
        LM_PutMochaDecoder(decoder);
        return;
    }

    cx = decoder->js_context;

    /*
     * A front end timer has gone off.  See which of our timeouts need
     *   servicing
     */
restart:
    LL_I2L(now, PR_IntervalNow());

    /* The timeout list is kept in deadline order.  Discover the
       latest timeout whose deadline has expired. On some platforms,
       front-end timeout events fire "early", so we need to test the
       timer_id as well as the deadline. */
    last_expired_timeout = NULL;
    for (timeout = decoder->timeouts; timeout; timeout = timeout->next) {
        if ((timer_id == timeout->toid) || !LL_CMP(timeout->when, >, now))
            last_expired_timeout = timeout;
    }

    /* Maybe the timeout that the event was fired for has been deleted
       and there are no others timeouts with deadlines that make them
       eligible for execution yet.  Go away. */
    if (!last_expired_timeout)
        return;

    /* Insert a dummy timeout into the list of timeouts between the portion
       of the list that we are about to process now and those timeouts that
       will be processed in a future call to win_run_timeout().  This dummy
       timeout serves as the head of the list for any timeouts inserted as
       a result of running a timeout. */
    dummy_timeout.toid = NULL;
    dummy_timeout.public_id = 0;
    dummy_timeout.next = last_expired_timeout->next;
    last_expired_timeout->next = &dummy_timeout;

    /* Don't let lm_ClearWindowTimeouts throw away our stack-allocated
       dummy timeout. */
    dummy_timeout.ref_count = 2;
   
    XP_ASSERT(!js_timeout_insertion_point);
    js_timeout_insertion_point = &dummy_timeout.next;
    
    for (timeout = decoder->timeouts; timeout != &dummy_timeout; timeout = next) {
        next = timeout->next;

        /*
         * make sure the document that asked for this timeout is still
         *   the active document
         */
        if (timeout->doc_id != XP_DOCID(decoder->window_context)) {
#ifdef DEBUG_chouck
            XP_TRACE(("DOCID: in win_run_timeout"));
#endif
            /* make sure this gets removed from the list */
            decoder->timeouts = next;
            DROP_TIMEOUT(decoder, timeout);
            continue;
        }

        /* Hold the timeout in case expr or funobj releases its doc. */
        HOLD_TIMEOUT(cx, timeout);
        js_timeout_running = timeout;

        if (timeout->expr) {
            /* Evaluate the timeout expression. */
            JSVersion save = JS_SetVersion(decoder->js_context, 
                                           timeout->version);
            JS_EvaluateScriptForPrincipals(decoder->js_context, 
                                           decoder->window_object,
                                           timeout->principals, 
                                           timeout->expr, 
                                           XP_STRLEN(timeout->expr),
                                           timeout->filename, timeout->lineno, 
                                           &result);
            JS_SetVersion(decoder->js_context, save);
        } else {
            int64 lateness64;
            int32 lateness;

            /* Add "secret" final argument that indicates timeout
               lateness in milliseconds */
            LL_SUB(lateness64, now, timeout->when);
            LL_L2I(lateness, lateness64);
            timeout->argv[timeout->argc] = INT_TO_JSVAL((jsint)lateness);
            JS_CallFunctionValue(cx, decoder->window_object,
                                 OBJECT_TO_JSVAL(timeout->funobj),
                                 timeout->argc + 1, timeout->argv, &result);
        }

        /* If timeout's reference count is now 1, its doc was released
         * and we should restart this function.
         */
        js_timeout_running = NULL;
        if (timeout->ref_count == 1) {
            free_timeout(decoder, timeout);
            goto restart;
        }
        DROP_TIMEOUT(decoder, timeout);

        /* If we have a regular interval timer, we re-fire the
         *  timeout, accounting for clock drift.
         */
        if (timeout->interval) {
            /* Compute time to next timeout for interval timer. */
            int32 delay32;
            int64 interval, delay;
            LL_I2L(interval, timeout->interval);
            LL_ADD(timeout->when, timeout->when, interval);
            LL_I2L(now, PR_IntervalNow());
            LL_SUB(delay, timeout->when, now);
            LL_L2I(delay32, delay);

            /* If the next interval timeout is already supposed to
             *  have happened then run the timeout immediately.
             */
            if (delay32 < 0)
                delay32 = 0;

            /* Reschedule timeout.  Account for possible error return in
               code below that checks for zero toid. */
            timeout->toid = ET_PostSetTimeout(win_run_timeout,
                                              decoder->window_context,
                                              (uint32)delay32,
                                              decoder->doc_id);
        }

        /* Running a timeout can cause another timeout to be deleted,
           so we need to reset the pointer to the following timeout. */
        next = timeout->next;
        decoder->timeouts = next;

        /* Free the timeout if this is not a repeating interval
         *  timeout (or if it was an interval timeout, but we were
         *  unsuccessful at rescheduling it.)
         */
        if (!timeout->interval || !timeout->toid) {
            DROP_TIMEOUT(decoder, timeout);
        } else {
            /* Reschedule an interval timeout */
            /* Insert interval timeout onto list sorted in deadline order. */
            insert_timeout_into_list(js_timeout_insertion_point, timeout);
        }
    }

    /* Take the dummy timeout off the head of the list */
    XP_ASSERT(decoder->timeouts == &dummy_timeout);
    decoder->timeouts = dummy_timeout.next;
    js_timeout_insertion_point = NULL;

    LM_PutMochaDecoder(decoder);
}

void
lm_ClearWindowTimeouts(MochaDecoder *decoder)
{
    JSTimeout *timeout, *next;

    for (timeout = decoder->timeouts; timeout; timeout = next) {
        /* If win_run_timeouts() is higher up on the stack for this
           decoder, e.g. as a result of document.write from a timeout,
           then we need to reset the list insertion point for
           newly-created timeouts in case the user adds a timeout,
           before we pop the stack back to win_run_timeouts(). */
        if (js_timeout_running == timeout)
            js_timeout_insertion_point = NULL;
        
        next = timeout->next;
        if (timeout->toid)
            ET_PostClearTimeout(timeout->toid);
        timeout->toid = NULL;
        DROP_TIMEOUT(decoder, timeout);
    }

    for (timeout = decoder->saved_timeouts; timeout; timeout = next) {
        next = timeout->next;
        DROP_TIMEOUT(decoder, timeout);
    }

    decoder->timeouts = NULL;
    decoder->saved_timeouts = NULL;
}

/*
 * Stop time temporarily till we restore the timeouts. The timeouts
 * are cancelled with the FE, the current time is recorded and
 * the timeout list is saved.
 */
void
lm_SaveWindowTimeouts(MochaDecoder *decoder)
{
    JSTimeout *timeout;
    int64 now, time_left;
 
    if (!decoder->timeouts)
        return;
    
    LL_I2L(now, PR_IntervalNow());

    /* 
     * Clear all the timeouts so that they don't fire till we're
     * ready to restart time.
     */
    for (timeout = decoder->timeouts; timeout; timeout = timeout->next) {
        ET_PostClearTimeout(timeout->toid);
        /* Figure out how much time was left before the timeout fired */
        LL_SUB(time_left, timeout->when, now);
        timeout->when = time_left;
    }

    decoder->saved_timeouts = decoder->timeouts;
    decoder->timeouts = NULL;
}

/*
 * Restore timeouts from the saved_timeouts list and restart time. 
 * These are timeouts that were suspended while we were reloading
 * the document.
 */
void
lm_RestoreWindowTimeouts(MochaDecoder *decoder)
{
    JSTimeout *timeout, *next;
    int64 now, when;
    int interval;
    JSBool fire_now = JS_FALSE;

    if (!decoder->saved_timeouts)
        return;
    
    LL_I2L(now, PR_IntervalNow());

    for (timeout = decoder->saved_timeouts; timeout; timeout = next) {
        next = timeout->next;
        
        LL_L2I(interval, timeout->when);
        /* Unclear if result can be one of the operands */
        LL_ADD(when, timeout->when, now);
        timeout->when = when;

        timeout->doc_id = decoder->doc_id;

        /* 
         * If we don't have to fire this timeout now, we tell the
         * FE to call us back.
         */
        if (interval > 0) {
            timeout->toid = ET_PostSetTimeout(win_run_timeout,
                                              decoder->window_context,
                                              (uint32)interval,
                                              decoder->doc_id);
        }
        else {
            timeout->toid = NULL;
            fire_now = JS_TRUE;
        }

        insert_timeout_into_list(&decoder->timeouts, timeout);
    }

    decoder->saved_timeouts = NULL;
    
    /*
     * If there are any timeouts that are ready to go, have them fired
     * off immediately.
     */
    if (fire_now) {
        MozillaEvent_Timeout e;
        
        e.ce.doc_id = decoder->doc_id;
        e.pClosure = decoder->window_context;
        e.pTimerId = NULL;
        e.ulTime = 0;
        
        win_run_timeout((void *)&e);
    }
}

static timeout_public_id_counter = 0;

static const char setTimeout_str[]  = "setTimeout";
static const char setInterval_str[] = "setInterval";
static const char *time_methods[2]  = { setTimeout_str, setInterval_str };

static JSBool
win_set_timeout_or_interval(JSContext *cx, JSObject *obj,
                            uint argc, jsval *argv, jsval *rval,
                            JSBool interval_timer)
{
    MWContext *context;
    MochaDecoder *decoder;
    jsdouble interval;
    JSString *str;
    char *expr;
    JSTimeout *timeout, **insertion_point;
    int64 now, delta;
    JSObject *funobj;
    JSStackFrame *fp;

    if (!(decoder = JS_GetInstancePrivate(cx, obj, &lm_window_class, argv)))
        return JS_FALSE;

    if (lm_timeout_count >= lm_timeout_limit) {
        JS_ReportError(cx, "too many timeouts and intervals");
        return JS_FALSE;
    }

    if (!JS_ValueToNumber(cx, argv[1], &interval))
        return JS_FALSE;

    switch (JS_TypeOfValue(cx, argv[0])) {
      case JSTYPE_FUNCTION:
        funobj = JSVAL_TO_OBJECT(argv[0]);
        expr = NULL;
        break;
      case JSTYPE_STRING:
      case JSTYPE_OBJECT:
        if (!(str = JS_ValueToString(cx, argv[0])))
            return JS_FALSE;
        expr = JS_strdup(cx, JS_GetStringBytes(str));
        if (!expr)
            return JS_FALSE;
        funobj = NULL;
        break;
      default:
        JS_ReportError(cx, "useless %s call (missing quotes around argument?)",
                       time_methods[interval_timer]);
        return JS_FALSE;
    }

    timeout = JS_malloc(cx, sizeof *timeout);
    if (!timeout) {
        JS_free(cx, expr);
        return JS_FALSE;
    }
    XP_BZERO(timeout, sizeof *timeout);
    timeout->ref_count = 1;
    if (interval_timer)
        timeout->interval = (int32)interval;
    timeout->expr = expr;
    timeout->funobj = funobj;
    timeout->version = JS_GetVersion(cx);
    timeout->principals = lm_GetPrincipalsFromStackFrame(cx);
    if (timeout->principals == NULL) {
        /* Result of out of memory error */
        DROP_TIMEOUT(decoder, timeout);
        return JS_FALSE;
    }
    JSPRINCIPALS_HOLD(cx, timeout->principals);

    fp = NULL;
    while ((fp = JS_FrameIterator(cx, &fp)) != NULL) {
        JSScript *script = JS_GetFrameScript(cx, fp);
        if (script) {
            /*
             * XXX - Disable error reporter; we may get an error
             * trying to find a line number. Two cases where we
             * get an error is in evaluating an expression as part
             * of the preferences code, and evaluating a javascript:
             * typein. In both cases there are no newlines and no
             * source annotations are generated, and the initial
             * linenumber is specified as zero. We need to clean
             * up all those cases and then remove the error reporter
             * workaround.
             */
            JSErrorReporter reporter = JS_SetErrorReporter(cx, NULL);
            const char *filename = JS_GetScriptFilename(cx, script);
            timeout->filename = filename ? XP_STRDUP(filename) : NULL;
            timeout->lineno = JS_PCToLineNumber(cx, script, 
                                                JS_GetFramePC(cx, fp));
            JS_SetErrorReporter(cx, reporter);
            break;
        }
    }

    /* Keep track of any pending timeouts so that FE can tell whether
       the stop button should be active. */
    context = decoder->window_context;
    if (context)
        context->js_timeouts_pending++;

    if (expr) {
        timeout->argv = 0;
        timeout->argc = 0;
    } else {
        int i;
        /* Leave an extra slot for a secret final argument that
           indicates to the called function how "late" the timeout is. */
        timeout->argv = JS_malloc(cx, (argc - 1) * sizeof(jsval));
        if (!timeout->argv) {
            DROP_TIMEOUT(decoder, timeout);
            return JS_FALSE;
        }
        if (!JS_AddNamedRoot(cx, &timeout->funobj, "timeout.funobj")) {
            DROP_TIMEOUT(decoder, timeout);
            return JS_FALSE;
        }

        timeout->argc = 0;
        for (i = 2; (uint)i < argc; i++) {
            timeout->argv[i - 2] = argv[i];
            if (!JS_AddNamedRoot(cx, &timeout->argv[i - 2], "timeout.argv[i]"))
	    {
                DROP_TIMEOUT(decoder, timeout);
                return JS_FALSE;
            }
            timeout->argc++;
        }
    }

    LL_I2L(now, PR_IntervalNow());
    LL_D2L(delta, interval);
    LL_ADD(timeout->when, now, delta);
    timeout->doc_id = decoder->doc_id;

    if (interval < 0)
	interval = 0;

    timeout->toid = ET_PostSetTimeout(win_run_timeout,
                                      decoder->window_context,
                                      (uint32)interval,
                                      decoder->doc_id);
    if (!timeout->toid) {
        DROP_TIMEOUT(decoder, timeout);
        return JS_FALSE;
    }

    if (js_timeout_insertion_point == NULL)
        insertion_point = &decoder->timeouts;
    else
        insertion_point = js_timeout_insertion_point;

    /* Insert interval timeout onto list sorted in deadline order */
    insert_timeout_into_list(insertion_point, timeout);

    lm_timeout_count++;

    timeout->public_id = ++timeout_public_id_counter;
    *rval = INT_TO_JSVAL((jsint)timeout->public_id);
    return JS_TRUE;
}

PR_STATIC_CALLBACK(PRBool)
win_set_timeout(JSContext *cx, JSObject *obj,
                uint argc, jsval *argv, jsval *rval)
{
    return win_set_timeout_or_interval(cx, obj, argc, argv, rval, JS_FALSE);
}

PR_STATIC_CALLBACK(PRBool)
win_set_interval(JSContext *cx, JSObject *obj,
                 uint argc, jsval *argv, jsval *rval)
{
    return win_set_timeout_or_interval(cx, obj, argc, argv, rval, JS_TRUE);
}

PR_STATIC_CALLBACK(PRBool)
win_clear_timeout(JSContext *cx, JSObject *obj,
                  uint argc, jsval *argv, jsval *rval)
{
    MochaDecoder *decoder;
    uint32 public_id;
    JSTimeout **top, *timeout;

    if (!(decoder = JS_GetInstancePrivate(cx, obj, &lm_window_class, argv)))
        return JS_FALSE;
    if (!JSVAL_IS_INT(argv[0]))
	return JS_TRUE;
    public_id = (uint32)JSVAL_TO_INT(argv[0]);
    if (!public_id)    /* id of zero is reserved for internal use */
	return JS_TRUE;
    for (top = &decoder->timeouts; ((timeout = *top) != NULL); top = &timeout->next) {
        if (timeout->public_id == public_id) {
            if (js_timeout_running == timeout) {
                /* We're running from inside the timeout.  Mark this
                   timeout for deferred deletion by the code in
                   win_run_timeout() */
                timeout->interval = 0;
            } else {
                /* Delete the timeout from the pending timeout list */
                *top = timeout->next;
                ET_PostClearTimeout(timeout->toid);
                DROP_TIMEOUT(decoder, timeout);
            }
            break;
        }
    }
    return JS_TRUE;
}

/* Sleep for the specified number of milliseconds */
PR_STATIC_CALLBACK(PRBool)
win_delay(JSContext *cx, JSObject *obj,
          uint argc, jsval *argv, jsval *rval)
{

/* XXX - Brendan and Scott decided not to provide a delay() method
   because of untoward side-effects of its use.  We might do so in the
   future, so I'm effectively reserving the name by leaving a
   do-nothing method in the Window object. */

#if DELAY_METHOD_ALLOWED

/* Arbitrary maximum delay to prevent near-infinit delays */
#define JS_MAX_DELAY 1000

    jsdouble ms;
    int64 ms64, c;

    if (argc < 1) {
        JS_ReportError(cx, lm_argc_err_str);
        return JS_FALSE;
    }

    if (!JS_ValueToNumber(cx, argv[0], &ms))
        return JS_FALSE;

    if (ms < 0) {
        JS_ReportError(cx, "milliseconds argument must be non-negative");
        return JS_FALSE;
    }

    /* Don't let someone hang the JS thread for too long. */
    if (ms > JS_MAX_DELAY)
        ms = JS_MAX_DELAY;

    LL_UI2L(ms64, (uint32)ms);
    LL_I2L(c, 1000);
    LL_MUL(ms64, ms64, c);
    {
        PRIntervalTime i;
        LL_L2I(i, ms64);
        PR_Sleep(i);
    }
#undef JS_MAX_DELAY

#endif
    return JS_TRUE;
}

PR_STATIC_CALLBACK(PRBool)
win_focus(JSContext *cx, JSObject *obj,
          uint argc, jsval *argv, jsval *rval)
{
    MochaDecoder *decoder;

    if (!(decoder = JS_GetInstancePrivate(cx, obj, &lm_window_class, argv)))
        return JS_FALSE;
    
    if (!decoder->window_context)
        return JS_TRUE;

    ET_PostManipulateForm(decoder->window_context, 0, EVENT_FOCUS);
    return JS_TRUE;
}

PR_STATIC_CALLBACK(PRBool)
win_blur(JSContext *cx, JSObject *obj,
         uint argc, jsval *argv, jsval *rval)
{
    MochaDecoder *decoder;

    if (!(decoder = JS_GetInstancePrivate(cx, obj, &lm_window_class, argv)))
        return JS_FALSE;
    
    if (!decoder->window_context)
        return JS_TRUE;

    ET_PostManipulateForm(decoder->window_context, 0, EVENT_BLUR);

    return JS_TRUE;
}

/*
 * Scrolling support.
 */
PR_STATIC_CALLBACK(PRBool)
win_scroll_to(JSContext *cx, JSObject *obj,
           uint argc, jsval *argv, jsval *rval)
{
    MochaDecoder *decoder;
    int32 x, y;

    if (!(decoder = JS_GetInstancePrivate(cx, obj, &lm_window_class, argv)))
        return JS_FALSE;

    if (argc != 2) {
        JS_ReportError(cx, lm_argc_err_str);
        return JS_FALSE;
    }

    if (!decoder->window_context)
        return JS_TRUE;

    if (!JS_ValueToInt32(cx, argv[0], &x) || !JS_ValueToInt32(cx, argv[1], &y))
        return JS_FALSE;    

    ET_PostScrollDocTo(decoder->window_context, 0, x<0?0:x, y<0?0:y);
    return JS_TRUE;
}

/*
 * Scrolling support.
 */
PR_STATIC_CALLBACK(PRBool)
win_scroll_by(JSContext *cx, JSObject *obj,
           uint argc, jsval *argv, jsval *rval)
{
    MochaDecoder *decoder;
    int32 x, y;

    if (!(decoder = JS_GetInstancePrivate(cx, obj, &lm_window_class, argv)))
        return JS_FALSE;

    if (argc != 2) {
        JS_ReportError(cx, lm_argc_err_str);
        return JS_FALSE;
    }

    if (!decoder->window_context)
        return JS_TRUE;

    if (!JS_ValueToInt32(cx, argv[0], &x) || !JS_ValueToInt32(cx, argv[1], &y))
        return JS_FALSE;    

    ET_PostScrollDocBy(decoder->window_context, 0, x, y);
    return JS_TRUE;
}

/*
 * Window resizing after initial creation.
 */
PR_STATIC_CALLBACK(PRBool)
win_resize_to(JSContext *cx, JSObject *obj,
           uint argc, jsval *argv, jsval *rval)
{
    MochaDecoder *decoder;
    Chrome *chrome = NULL;
    int32 width, height;
    JSBool outer_size;

    if (!(decoder = JS_GetInstancePrivate(cx, obj, &lm_window_class, argv)))
        return JS_FALSE;

    if (argc > 3) {
        JS_ReportError(cx, lm_argc_err_str);
        return JS_FALSE;
    }

    if (!decoder->window_context)
        return JS_TRUE;

    if (!JS_ValueToInt32(cx, argv[0], &width) || !JS_ValueToInt32(cx, argv[1], &height))
        return JS_FALSE;    

    if (!(chrome = win_get_chrome(decoder->window_context, chrome)))
        return JS_FALSE;

    if (argc==3 && JS_ValueToBoolean(cx, argv[2], &outer_size) && outer_size == JS_TRUE) {
        chrome->outw_hint = width;
        chrome->outh_hint = height;
        if (chrome->outw_hint <= 0 || chrome->outh_hint <= 0) {
            JS_free(cx, chrome);
            return JS_TRUE;
        }
    }
    else {
        chrome->w_hint = width;
        chrome->h_hint = height;
        chrome->outw_hint = 0;
        chrome->outh_hint = 0;
        if (chrome->w_hint <= 0 || chrome->h_hint <= 0 || 
                                    decoder->window_context->grid_parent) {
            JS_free(cx, chrome);
            return JS_TRUE;
        }
    }
    if ((chrome->w_hint && chrome->w_hint < 100) ||
        (chrome->h_hint && chrome->h_hint < 100) ||
        (chrome->outw_hint && chrome->outw_hint < 100) ||
        (chrome->outh_hint && chrome->outh_hint < 100)) {
        if (!lm_CanAccessTarget(cx, JSTARGET_UNIVERSAL_BROWSER_WRITE)) {
            /* Make sure windows are at least 100 by 100 pixels. */
            if (chrome->w_hint && chrome->w_hint < 100) chrome->w_hint = 100;
            if (chrome->h_hint && chrome->h_hint < 100) chrome->h_hint = 100;
            if (chrome->outw_hint && chrome->outw_hint < 100) chrome->outw_hint = 100;
            if (chrome->outh_hint && chrome->outh_hint < 100) chrome->outh_hint = 100;
        }
    } 
    /* Make sure they always at least 10 x 10 regardless of security. 1 x 1
     * windows are really hard to spot */
    if (chrome->w_hint && chrome->w_hint < 10) chrome->w_hint = 10;
    if (chrome->h_hint && chrome->h_hint < 10) chrome->h_hint = 10;
    if (chrome->outw_hint && chrome->outw_hint < 10) chrome->outw_hint = 10;
    if (chrome->outh_hint && chrome->outh_hint < 10) chrome->outh_hint = 10;

    ET_PostUpdateChrome(decoder->window_context, chrome);
    JS_free(cx, chrome);
    return JS_TRUE;
}

/*
 * Scrolling support.
 */
PR_STATIC_CALLBACK(PRBool)
win_resize_by(JSContext *cx, JSObject *obj,
           uint argc, jsval *argv, jsval *rval)
{
    MochaDecoder *decoder;
    Chrome *chrome = NULL;
    int32 width, height;
    JSBool outer_size;

    if (!(decoder = JS_GetInstancePrivate(cx, obj, &lm_window_class, argv)))
        return JS_FALSE;

    if (argc > 3) {
        JS_ReportError(cx, lm_argc_err_str);
        return JS_FALSE;
    }

    if (!decoder->window_context)
        return JS_TRUE;

    if (!JS_ValueToInt32(cx, argv[0], &width) || !JS_ValueToInt32(cx, argv[1], &height))
        return JS_FALSE;    

    if (!(chrome = win_get_chrome(decoder->window_context, chrome)))
        return JS_FALSE;

    if (argc==3 && JS_ValueToBoolean(cx, argv[2], &outer_size) && outer_size == JS_TRUE) {
        chrome->outw_hint += width;
        chrome->outh_hint += height;
        if (chrome->outw_hint <= 0 || chrome->outh_hint <= 0) {
            JS_free(cx, chrome);
            return JS_TRUE;
        }
    }
    else {
        chrome->w_hint += width;
        chrome->h_hint += height;
        chrome->outw_hint = 0;
        chrome->outh_hint = 0;
        if (chrome->w_hint <= 0 || chrome->h_hint <= 0 || 
                                    decoder->window_context->grid_parent) {
            JS_free(cx, chrome);
            return JS_TRUE;
        }
    }
    if ((chrome->w_hint && chrome->w_hint < 100) ||
        (chrome->h_hint && chrome->h_hint < 100) ||
        (chrome->outw_hint && chrome->outw_hint < 100) ||
        (chrome->outh_hint && chrome->outh_hint < 100)) {
        if (!lm_CanAccessTarget(cx, JSTARGET_UNIVERSAL_BROWSER_WRITE)) {
            /* Make sure windows are at least 100 by 100 pixels. */
            if (chrome->w_hint && chrome->w_hint < 100) chrome->w_hint = 100;
            if (chrome->h_hint && chrome->h_hint < 100) chrome->h_hint = 100;
            if (chrome->outw_hint && chrome->outw_hint < 100) chrome->outw_hint = 100;
            if (chrome->outh_hint && chrome->outh_hint < 100) chrome->outh_hint = 100;
        }
    }
    /* Make sure they always at least 10 x 10 regardless of security. 1 x 1
     * windows are really hard to spot */
    if (chrome->w_hint && chrome->w_hint < 10) chrome->w_hint = 10;
    if (chrome->h_hint && chrome->h_hint < 10) chrome->h_hint = 10;
    if (chrome->outw_hint && chrome->outw_hint < 10) chrome->outw_hint = 10;
    if (chrome->outh_hint && chrome->outh_hint < 10) chrome->outh_hint = 10;

    ET_PostUpdateChrome(decoder->window_context, chrome);
    JS_free(cx, chrome);
    return JS_TRUE;
}

/*
 * Scrolling support.
 */
PR_STATIC_CALLBACK(PRBool)
win_move_to(JSContext *cx, JSObject *obj,
           uint argc, jsval *argv, jsval *rval)
{
    MochaDecoder *decoder;
    Chrome *chrome = NULL;
    int32 width, height, x, y;

    if (!(decoder = JS_GetInstancePrivate(cx, obj, &lm_window_class, argv)))
        return JS_FALSE;

    if (argc != 2) {
        JS_ReportError(cx, lm_argc_err_str);
        return JS_FALSE;
    }

    if (!decoder->window_context)
        return JS_TRUE;

    if (!JS_ValueToInt32(cx, argv[0], &x) || !JS_ValueToInt32(cx, argv[1], &y))
        return JS_FALSE;    

    if (!(chrome = win_get_chrome(decoder->window_context, chrome)))
        return JS_FALSE;

    chrome->l_hint = x;
    chrome->t_hint = y;

    /* Windows must be positioned on screen without security */
    ET_PostGetScreenSize(decoder->window_context, &width, &height);
    if ((width < chrome->l_hint + chrome->outw_hint) ||
        (chrome->l_hint < 0) ||
        (height < chrome->t_hint + chrome->outh_hint) ||
        (chrome->t_hint < 0)) {
        if (!lm_CanAccessTarget(cx, JSTARGET_UNIVERSAL_BROWSER_WRITE)) {
            chrome->l_hint = (width < chrome->l_hint + chrome->outw_hint) ?
                                width - chrome->outw_hint : chrome->l_hint;
            chrome->l_hint = (chrome->l_hint < 0) ? 0 : chrome->l_hint;
            chrome->t_hint = (height < chrome->t_hint + chrome->outh_hint) ?
                                height - chrome->outh_hint : chrome->t_hint;
            chrome->t_hint = (chrome->t_hint < 0) ? 0 : chrome->t_hint;
        }
    }

    ET_PostUpdateChrome(decoder->window_context, chrome);
    JS_free(cx, chrome);
    return JS_TRUE;
}

/*
 * Scrolling support.
 */
PR_STATIC_CALLBACK(PRBool)
win_move_by(JSContext *cx, JSObject *obj,
           uint argc, jsval *argv, jsval *rval)
{
    MochaDecoder *decoder;
    Chrome *chrome = NULL;
    int32 x, y, width, height;

    if (!(decoder = JS_GetInstancePrivate(cx, obj, &lm_window_class, argv)))
        return JS_FALSE;

    if (argc != 2) {
        JS_ReportError(cx, lm_argc_err_str);
        return JS_FALSE;
    }

    if (!decoder->window_context)
        return JS_TRUE;

    if (!JS_ValueToInt32(cx, argv[0], &x) || !JS_ValueToInt32(cx, argv[1], &y))
        return JS_FALSE;    

    if (!(chrome = win_get_chrome(decoder->window_context, chrome)))
        return JS_FALSE;

    chrome->l_hint += x;
    chrome->t_hint += y;

    /* Windows must be positioned on screen without security */
    ET_PostGetScreenSize(decoder->window_context, &width, &height);
    if ((width < chrome->l_hint + chrome->outw_hint) ||
        (chrome->l_hint < 0) ||
        (height < chrome->t_hint + chrome->outh_hint) ||
        (chrome->t_hint < 0)) {
        if (!lm_CanAccessTarget(cx, JSTARGET_UNIVERSAL_BROWSER_WRITE)) {
            chrome->l_hint = (width < chrome->l_hint + chrome->outw_hint) ?
                                width - chrome->outw_hint : chrome->l_hint;
            chrome->l_hint = (chrome->l_hint < 0) ? 0 : chrome->l_hint;
            chrome->t_hint = (height < chrome->t_hint + chrome->outh_hint) ?
                                height - chrome->outh_hint : chrome->t_hint;
            chrome->t_hint = (chrome->t_hint < 0) ? 0 : chrome->t_hint;
        }
    }

    ET_PostUpdateChrome(decoder->window_context, chrome);
    JS_free(cx, chrome);
    return JS_TRUE;
}

PR_STATIC_CALLBACK(PRBool)
win_capture_events(JSContext *cx, JSObject *obj,
           uint argc, jsval *argv, jsval *rval)
{
    MochaDecoder *decoder;
    jsdouble d;

    if (!(decoder = JS_GetInstancePrivate(cx, obj, &lm_window_class, argv)))
        return JS_FALSE;

    if (!decoder->window_context)
        return JS_TRUE;

    if (argc != 1)
        return JS_TRUE;

    if (!JS_ValueToNumber(cx, argv[0], &d))
        return JS_FALSE;

    decoder->event_bit |= (uint32)d;
    decoder->window_context->event_bit |= (uint32)d;

    return JS_TRUE;
}

PR_STATIC_CALLBACK(PRBool)
win_release_events(JSContext *cx, JSObject *obj,
           uint argc, jsval *argv, jsval *rval)
{
    MochaDecoder *decoder;
    JSEventCapturer *cap;
    jsdouble d;
    jsint layer_index;
    jsint max_layer_num;
    JSObject *cap_obj;

    if (!(decoder = JS_GetInstancePrivate(cx, obj, &lm_window_class, argv)))
        return JS_FALSE;

    if (!decoder->window_context)
        return JS_TRUE;

    if (argc != 1)
        return JS_TRUE;

    if (!JS_ValueToNumber(cx, argv[0], &d))
        return JS_FALSE;

    decoder->event_bit &= ~(uint32)d;
    decoder->window_context->event_bit &= ~(uint32)d;

    /* Now we have to see if anyone else wanted that bit set.  Joy!*/
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

static PRBool
setExternalCapture(JSContext *cx, JSObject *obj, 
                   uint argc, jsval *argv, JSBool val)
{
    MochaDecoder *decoder;

    if (!(decoder = JS_GetInstancePrivate(cx, obj, &lm_window_class, argv)))
        return JS_FALSE;

    if (argc != 0)
        return JS_TRUE;

    if (lm_CanAccessTarget(cx, JSTARGET_UNIVERSAL_BROWSER_WRITE)) {
        if (decoder->principals)
            lm_SetExternalCapture(cx, decoder->principals, val);
    }

    return JS_TRUE;
}

PR_STATIC_CALLBACK(PRBool)
win_enable_external_capture(JSContext *cx, JSObject *obj,
                            uint argc, jsval *argv, jsval *rval)
{
    return setExternalCapture(cx, obj, argc, argv, JS_TRUE);
}

PR_STATIC_CALLBACK(PRBool)
win_disable_external_capture(JSContext *cx, JSObject *obj,
                             uint argc, jsval *argv, jsval *rval)
{
    return setExternalCapture(cx, obj, argc, argv, JS_FALSE);
}

PR_STATIC_CALLBACK(PRBool)
win_compromise_principals(JSContext *cx, JSObject *obj,
                          uint argc, jsval *argv, jsval *rval)
{
    MochaDecoder *decoder;

    if (!(decoder = JS_GetInstancePrivate(cx, obj, &lm_window_class, argv)))
        return JS_FALSE;

    decoder->principals_compromised = JS_TRUE;
    return JS_TRUE;
}

PR_STATIC_CALLBACK(PRBool)
win_downgrade_principals(JSContext *cx, JSObject *obj,
                         uint argc, jsval *argv, jsval *rval)
{
    MochaDecoder *decoder;

    if (!(decoder = JS_GetInstancePrivate(cx, obj, &lm_window_class, argv)))
        return JS_FALSE;

    if (decoder->principals)
        lm_InvalidateCertPrincipals(decoder, decoder->principals);

    return JS_TRUE;
}

PR_STATIC_CALLBACK(PRBool)
win_back(JSContext *cx, JSObject *obj,
           uint argc, jsval *argv, jsval *rval)
{
    MochaDecoder *decoder;

    if (!(decoder = JS_GetInstancePrivate(cx, obj, &lm_window_class, argv)))
        return JS_FALSE;
    if (!decoder->window_context)
        return JS_TRUE;

    ET_PostBackCommand(decoder->window_context);
    return JS_TRUE;
}

PR_STATIC_CALLBACK(PRBool)
win_forward(JSContext *cx, JSObject *obj,
           uint argc, jsval *argv, jsval *rval)
{
    MochaDecoder *decoder;

    if (!(decoder = JS_GetInstancePrivate(cx, obj, &lm_window_class, argv)))
        return JS_FALSE;
    if (!decoder->window_context)
        return JS_TRUE;

    ET_PostForwardCommand(decoder->window_context);
    return JS_TRUE;
}

PR_STATIC_CALLBACK(PRBool)
win_home(JSContext *cx, JSObject *obj,
           uint argc, jsval *argv, jsval *rval)
{
    MochaDecoder *decoder;

    if (!(decoder = JS_GetInstancePrivate(cx, obj, &lm_window_class, argv)))
        return JS_FALSE;
    if (!decoder->window_context)
        return JS_TRUE;

    ET_PostHomeCommand(decoder->window_context);
    return JS_TRUE;
}

PR_STATIC_CALLBACK(PRBool)
win_find(JSContext *cx, JSObject *obj,
           uint argc, jsval *argv, jsval *rval)
{
    MochaDecoder *decoder;
    MWContext *context;
    JSString *str;
    JSBool ret = JS_TRUE,
           matchCase = JS_FALSE,
           searchBackward = JS_FALSE;
    char * findStr;

    if (!(decoder = JS_GetInstancePrivate(cx, obj, &lm_window_class, argv)))
        return JS_FALSE;
    context = decoder->window_context;
    if (!context)
        return JS_TRUE;

    /* If no find argument set, just bring up find dialog */
    if (argc == 0) {
        ET_PostFindCommand(context, NULL, JS_FALSE, JS_FALSE);
        return JS_TRUE;
    }

    if (argc >3 )
        return JS_FALSE;

    if (!(str = JS_ValueToString(cx, argv[0])))
        return JS_TRUE;
    if (!lm_CheckPermissions(cx, decoder->window_object, 
                             JSTARGET_UNIVERSAL_BROWSER_READ))
        return JS_FALSE;

    if (argc == 3) {
        if (!JS_ValueToBoolean(cx, argv[1], &matchCase))
            matchCase = JS_FALSE;
        if (!JS_ValueToBoolean(cx, argv[2], &searchBackward))
            searchBackward = JS_FALSE;
    }

    findStr = lm_StrToLocalEncoding(context, str);
    ret = ET_PostFindCommand(context, findStr, matchCase, searchBackward);

    XP_FREEIF(findStr);
    *rval = BOOLEAN_TO_JSVAL(ret);

    return JS_TRUE;
}

PR_STATIC_CALLBACK(PRBool)
win_print(JSContext *cx, JSObject *obj,
           uint argc, jsval *argv, jsval *rval)
{
    MochaDecoder *decoder;

    if (!(decoder = JS_GetInstancePrivate(cx, obj, &lm_window_class, argv)))
        return JS_FALSE;
    if (!decoder->window_context)
        return JS_TRUE;

    ET_PostPrintCommand(decoder->window_context);
    return JS_TRUE;
}

PR_STATIC_CALLBACK(PRBool)
win_open_file(JSContext *cx, JSObject *obj,
           uint argc, jsval *argv, jsval *rval)
{
    MochaDecoder *decoder;

    if (!(decoder = JS_GetInstancePrivate(cx, obj, &lm_window_class, argv)))
        return JS_FALSE;
    if (!decoder->window_context)
        return JS_TRUE;

    ET_PostOpenFileCommand(decoder->window_context);
    return JS_TRUE;
}

PR_STATIC_CALLBACK(PRBool)
win_stop(JSContext *cx, JSObject *obj,
           uint argc, jsval *argv, jsval *rval)
{
    MochaDecoder *decoder;

    if (!(decoder = JS_GetInstancePrivate(cx, obj, &lm_window_class, argv)))
        return JS_FALSE;
    if (!decoder->window_context)
        return JS_TRUE;

    ET_moz_CallFunctionAsync((ETVoidPtrFunc) XP_InterruptContext, decoder->window_context);
    return JS_TRUE;
}

PR_STATIC_CALLBACK(PRBool)
win_atob(JSContext *cx, JSObject *obj, uint argc, jsval *argv, jsval *rval)
{
    JSString *str;
    char *srcStr, *destStr = NULL, *tmpStr;
    unsigned int len;

    if (argc != 1) {
        JS_ReportError(cx, lm_argc_err_str);
        return JS_FALSE;
    }
    if (!(str = JS_ValueToString(cx, argv[0])))
        return JS_FALSE;

    srcStr = JS_GetStringBytes(str);
    if (JS_GetStringLength(str) == 0) {
        *rval = JS_GetEmptyStringValue(cx);
        return JS_TRUE;
    }

    tmpStr = (char *)ATOB_AsciiToData(srcStr, &len);
    if (tmpStr == NULL) {
        JS_ReportError(cx, "base64 decoder failure");
        return JS_FALSE;
    }

    destStr = JS_malloc(cx, len + 1);
    if (destStr == NULL) {
        XP_FREE(tmpStr);
        return JS_FALSE;
    }
    destStr[len] = 0;

    XP_MEMCPY(destStr, tmpStr, len);
    XP_FREE(tmpStr);
    str = JS_NewString(cx, destStr, len);
    if (str == NULL) {
        JS_free(cx, destStr);
        return JS_FALSE;
    }

    *rval= STRING_TO_JSVAL(str);
    return JS_TRUE;
}

PR_STATIC_CALLBACK(PRBool)
win_btoa(JSContext *cx, JSObject *obj, uint argc, jsval *argv, jsval *rval)
{
    JSString *str;
    char *srcStr, *destStr;
    unsigned int len;

    if (argc != 1) {
        JS_ReportError(cx, lm_argc_err_str);
        return JS_FALSE;
    }

    if (!(str = JS_ValueToString(cx, argv[0])))
        return JS_FALSE;
    srcStr = JS_GetStringBytes(str);
    len = JS_GetStringLength(str);
    if (len == 0) {
        *rval = JS_GetEmptyStringValue(cx);
        return JS_TRUE;
    }

    destStr = BTOA_DataToAscii((unsigned char *)srcStr, len);
    if (destStr == NULL) {
        JS_ReportError(cx, "base64 encoder failure");
        return JS_FALSE;
    }

    len = XP_STRLEN(destStr);
    str = JS_NewString(cx, destStr, len);
    if (str == NULL)
        return JS_FALSE;

    *rval= STRING_TO_JSVAL(str);
    return JS_TRUE;
}

PR_STATIC_CALLBACK(PRBool)
win_taint_stub(JSContext *cx, JSObject *obj, uint argc, jsval *argv,
               jsval *rval)
{
    *rval = argv[0];
    return JS_TRUE;
}

static JSFunctionSpec lm_window_methods[] = {
    {"alert",           win_alert,              1},
    {"clearTimeout",    win_clear_timeout,      1},
    {"clearInterval",   win_clear_timeout,      1},
    {"close",           win_close,              0},
    {"confirm",         win_confirm,            1},
    {"open",            win_open,               1},
    {"setZOptions",     win_set_zoptions,       1},
    {"setHotkeys",      win_set_hotkeys,        1},
    {"setResizable",    win_set_resizable,      1},
    {"prompt",          win_prompt,             2},
    {setTimeout_str,    win_set_timeout,        2},
    {setInterval_str,   win_set_interval,       2},
    {"delay",           win_delay,              0},
    /* escape and unescape are now spec'ed by ECMA, and have been moved
     * into the core engine.
     */
    {lm_blur_str,       win_blur,               0},
    {lm_focus_str,      win_focus,              0},
    {lm_scroll_str,     win_scroll_to,          2},
    {"scrollTo",        win_scroll_to,          2},
    {"scrollBy",        win_scroll_by,          2},
    {"moveTo",          win_move_to,            2},
    {"moveBy",          win_move_by,            2},
    {"resizeTo",        win_resize_to,          2},
    {"resizeBy",        win_resize_by,          2},
    {"captureEvents",   win_capture_events,     1},
    {"releaseEvents",   win_release_events,     1},
    {"enableExternalCapture",   win_enable_external_capture,    0 },
    {"disableExternalCapture",  win_disable_external_capture,   0 },
    {"compromisePrincipals",    win_compromise_principals,      0 },
    {"downgradePrincipals",     win_downgrade_principals,       0 },
    {"back",            win_back,               0},
    {"forward",         win_forward,            0},
    {"home",            win_home,               0},
    {"find",            win_find,               0},
    {"print",           win_print,              0},
    {"openFile",        win_open_file,          0},
    {"stop",            win_stop,               0},
    {"atob",            win_atob,               1},
    {"btoa",            win_btoa,               1},
    {"taint",           win_taint_stub,         1},
    {"untaint",         win_taint_stub,         1},
    {0}
};

static JSBool
is_context_really_busy_or_loading(MWContext *context)
{
    lo_TopState *top_state;
    int i;
    XP_List *kids;
    MWContext *kid;

    if (!context)
        return JS_FALSE;

    LO_LockLayout();

    if ((top_state = lo_FetchTopState(XP_DOCID(context))) != NULL &&
        (top_state->mocha_loading_applets_count ||
         top_state->mocha_loading_embeds_count)) {
        LO_UnlockLayout();
        return JS_TRUE;
    }

    if ((kids = context->grid_children) != NULL) {
        for (i = 1; ((kid = XP_ListGetObjectNum(kids, i)) != NULL); i++) {

            /* see if this frame is loading */
            if (is_context_really_busy_or_loading(kid)) {
                LO_UnlockLayout();
                return JS_TRUE;
            }

            /* make sure the load event has fired for this frame */
            if (kid->mocha_context) {
                MochaDecoder * decoder = LM_GetMochaDecoder(kid);
                if (!decoder->load_event_sent) {
                    LM_PutMochaDecoder(decoder);
                    LO_UnlockLayout();
                    return JS_TRUE;
                }
                LM_PutMochaDecoder(decoder);
            }
        }
    }

    LO_UnlockLayout();
    return JS_FALSE;
}

static JSBool
is_context_busy(MWContext *context)
{
    if (XP_IsContextBusy(context))
        return JS_TRUE;
    return is_context_really_busy_or_loading(context);
}

/*
 * 3.0 ran load events from the timeout loop to avoid freeing contexts in use,
 * e.g., parent.document.write() in a frame's BODY onLoad= attribute.  That
 * hack ran afoul of WinFE's failure to FIFO-schedule same-deadline timeouts.
 * Now, with threading and no way for the mozilla thread to blow away its own
 * data structures without unwinding to its event loop, an event handler can
 * use document.write or start a new load without fear of free memory usage by
 * the front end code that sent the event.
 */
static void
try_load_event(MWContext *context, MochaDecoder *decoder)
{
    /*
     * Don't do anything if we are waiting for URLs to load, or if applets
     * have not been loaded and initialized.
     */
    if (is_context_busy(context))
        return;

    /*
     * Send a load event at most once.
     */
    if (!decoder->load_event_sent) {
        JSEvent *event;
        jsval rval;

        event = XP_NEW_ZAP(JSEvent);
        if (!event)
            return;

        event->type = decoder->resize_reload ? EVENT_RESIZE : EVENT_LOAD;
	if (context->compositor) {
	    XP_Rect rect;

	    CL_GetCompositorWindowSize(context->compositor, &rect);
	    event->x = rect.right;
	    event->y = rect.bottom;
	}

        decoder->load_event_sent = JS_TRUE;
        (void) lm_SendEvent(context, decoder->window_object, event, &rval);

        decoder->event_mask &= ~event->type;

        if (!event->saved)
            XP_FREE(event);

    }

    /*
     * Now that we may have scheduled a load event for this context, send
     *   a transfer-done event to our parent frameset, in case it was
     *   waiting for us to finish
     */
    if (context->grid_parent)
        lm_SendLoadEvent(context->grid_parent, EVENT_XFER_DONE, JS_FALSE);

}

/*
 * Entry point for the netlib to notify JS of load and unload events.
 * Note: we can only rely on the resize_reload parameter to be
 * correct in the load and unload (but not the xfer) cases.
 */
void
lm_SendLoadEvent(MWContext *context, int32 event, JSBool resize_reload)
{
    MochaDecoder *decoder;
    JSEvent *pEvent;
    jsval rval;

    decoder = context->mocha_context ? LM_GetMochaDecoder(context) : 0;
    if (!decoder)
        return;

    switch (event) {
      case EVENT_LOAD:
        decoder->event_mask |= EVENT_LOAD;
        if (resize_reload) {
            decoder->resize_reload = JS_TRUE;
            /* Restore any saved timeouts that we have for this window */
            lm_RestoreWindowTimeouts(decoder);
	}
	try_load_event(context, decoder);
        break;

      case EVENT_UNLOAD:
        decoder->load_event_sent = JS_FALSE;
        decoder->event_mask &= ~EVENT_LOAD;

        /* If we're resizing, just save timeouts but don't send an event */
        if (resize_reload) {
            lm_SaveWindowTimeouts(decoder);
        } else {
            pEvent = XP_NEW_ZAP(JSEvent);
            pEvent->type = EVENT_UNLOAD;

            (void) lm_SendEvent(context, decoder->window_object,
                                pEvent, &rval);
            if (!pEvent->saved)
                XP_FREE(pEvent);
        }

      break;

      case EVENT_XFER_DONE:
        if ((decoder->event_mask & EVENT_LOAD))
            try_load_event(context, decoder);
        if (context->grid_parent)
            lm_SendLoadEvent(context->grid_parent, EVENT_XFER_DONE, JS_FALSE);
        break;

      case EVENT_ABORT:
      default:
          break;
    }

    if (decoder)
        LM_PutMochaDecoder(decoder);
    return;
}

/*
 * Entry point for front-ends to notify JS code of help events.
 */
void
LM_SendOnHelp(MWContext *context)
{
    JSEvent                     *pEvent;

    pEvent = XP_NEW_ZAP(JSEvent);
    pEvent->type = EVENT_HELP;

    ET_SendEvent(context, NULL, pEvent, NULL, NULL);

}

/*
 * Entry point for front-ends to notify JS code of scroll events.
 */
void
LM_SendOnScroll(MWContext *context, int32 x, int32 y)
{
    MochaDecoder *decoder;
    JSEvent *event;
    jsval rval;

XP_ASSERT(0);

    if (!context->mocha_context)
                return;
    decoder = LM_GetMochaDecoder(context);
    if (!(decoder->event_mask & EVENT_SCROLL)) {
                decoder->event_mask |= EVENT_SCROLL;

                event = XP_NEW_ZAP(JSEvent);
                event->type = EVENT_SCROLL;

                (void) lm_SendEvent(context, decoder->window_object, event,
                                        &rval);
                decoder->event_mask &= ~EVENT_SCROLL;
    }
    LM_PutMochaDecoder(decoder);
}

PRHashNumber
lm_KeyHash(const void *key)
{
    return (PRHashNumber)key;
}

PRHashTable *
lm_GetIdToObjectMap(MochaDecoder *decoder)
{
    PRHashTable *map;

    map = decoder->id_to_object_map;
    if (map)
        return map;

    map = PR_NewHashTable(LM_ID_TO_OBJ_MAP_SIZE,
                          lm_KeyHash,
                          PR_CompareValues,
                          PR_CompareValues,
                          NULL, NULL);

    if (!map)
        return NULL;

    decoder->id_to_object_map = map;
    return map;
}


MochaDecoder *
lm_NewWindow(MWContext *context)
{
    History_entry *he;
    MochaDecoder *decoder;
    JSContext *cx;
    JSObject *obj;

    /* make sure its a JS-able context */
    if (!LM_CanDoJS(context))
        return NULL;

    /*
     * If this is a (resizing) frame, get its decoder from session history.
     * Using the layout lock to protect context->hist.cur_doc_ptr (which is
     * all SHIST_GetCurrent uses) is dicey.  XXXchouck
     */
    LO_LockLayout();
    he = context->is_grid_cell ? SHIST_GetCurrent(&context->hist) : NULL;
    if (he && he->savedData.Window) {
        decoder = he->savedData.Window;
        he->savedData.Window = NULL;
        cx = decoder->js_context;
        obj = decoder->window_object;
        decoder->window_context = context;
        context->mocha_context = cx;
        LO_UnlockLayout();
        
        /*
         * Allow the context to observe the decoder's image context.
         */
        ET_il_SetGroupObserver(context, decoder->image_context, context, JS_TRUE);

        return decoder;
    }
    LO_UnlockLayout();

    /* Otherwise, make a new decoder/context/object for this window. */
    decoder = XP_NEW_ZAP(MochaDecoder);
    if (!decoder)
        return NULL;
    cx = JS_NewContext(lm_runtime, LM_STACK_SIZE);
    if (!cx) {
        XP_DELETE(decoder);
        return NULL;
    }
    obj = JS_NewObject(cx, &lm_window_class, NULL, NULL);
    if (!obj || !JS_SetPrivate(cx, obj, decoder)) {
        JS_DestroyContext(cx);
        XP_DELETE(decoder);
        return NULL;
    }

    /* Set the backward refcount to 1 because obj now holds decoder. */
    decoder->back_count = 1;

    /* Add a root in decoder for obj early, in case the GC runs soon. */
    decoder->window_object = obj;
    if (!JS_AddNamedRoot(cx, &decoder->window_object, "window_object"))
        return NULL;

    /* Create a new image context for anonymous images. */
    if (!lm_NewImageContext(context, decoder))
        return NULL;

    /* Set the forward refcount to 1 because context holds decoder. */
    decoder->forw_count = 1;
    decoder->window_context = context;
    decoder->js_context = cx;
    context->mocha_context = cx;

    if (!lm_InitWindowContent(decoder)) {
        /* This will cause finalization and call lm_DestroyWindow(). */
        decoder->window_object = NULL;
        return NULL;
    }

#define HOLD(obj)       if (!JS_AddNamedRoot(cx, &(obj), #obj)) return NULL

    /* Drop all object prototype refs. */
    HOLD(decoder->anchor_prototype);
    HOLD(decoder->bar_prototype);
    HOLD(decoder->document_prototype);
    HOLD(decoder->event_prototype);
    HOLD(decoder->event_capturer_prototype);
    HOLD(decoder->event_receiver_prototype);
    HOLD(decoder->form_prototype);
    HOLD(decoder->image_prototype);
    HOLD(decoder->input_prototype);
    HOLD(decoder->layer_prototype);
    HOLD(decoder->option_prototype);
    HOLD(decoder->rect_prototype);
    HOLD(decoder->url_prototype);

    /* Drop window sub-object refs. */
    HOLD(decoder->document);
    HOLD(decoder->history);
    HOLD(decoder->location);
    HOLD(decoder->navigator);
    HOLD(decoder->components);
    HOLD(decoder->crypto);
    HOLD(decoder->screen);
    HOLD(decoder->hardware);
    HOLD(decoder->pkcs11);
    /* Drop ad-hoc GC roots. */
    HOLD(decoder->event_receiver);
    HOLD(decoder->opener);

#undef HOLD

    JS_SetBranchCallback(cx, lm_BranchCallback);
    JS_SetErrorReporter(cx, lm_ErrorReporter);
    return decoder;
}

void
lm_DestroyWindow(MochaDecoder *decoder)
{
    JSContext *cx;

    /* All reference counts must be 0 here. */
    XP_ASSERT(decoder->forw_count == 0);
    XP_ASSERT(decoder->back_count == 0);

    /* We must not have an MWContext at this point. */
    XP_ASSERT(!decoder->window_context);

    /* Drop decoder refs to window prototypes and sub-objects. */
    lm_FreeWindowContent(decoder, JS_FALSE);

    /* Set cx for use by DROP. */
    cx = decoder->js_context;

#define DROP(obj)       JS_RemoveRoot(cx, &(obj))

    /* Drop all object prototype refs. */
    DROP(decoder->anchor_prototype);
    DROP(decoder->bar_prototype);
    DROP(decoder->document_prototype);
    DROP(decoder->event_prototype);
    DROP(decoder->event_capturer_prototype);
    DROP(decoder->event_receiver_prototype);
    DROP(decoder->form_prototype);
    DROP(decoder->image_prototype);
    DROP(decoder->input_prototype);
    DROP(decoder->layer_prototype);
    DROP(decoder->option_prototype);
    DROP(decoder->rect_prototype);
    DROP(decoder->url_prototype);

    /* Drop window sub-object refs. */
    DROP(decoder->document);
    DROP(decoder->history);
    DROP(decoder->location);
    DROP(decoder->navigator);
    DROP(decoder->components);
    DROP(decoder->crypto);
    DROP(decoder->screen);
    DROP(decoder->hardware);
    DROP(decoder->pkcs11);
    /* Drop ad-hoc GC roots. */
    DROP(decoder->event_receiver);
    DROP(decoder->opener);

    if (decoder->in_window_quota)
        lm_window_count--;

    /* Remove the root that holds up the whole window in the decoder world */
    DROP(decoder->window_object);

#undef DROP

    /*
     * Destroy the mocha image context.  All mocha images need to have been
     * finalized *before* we get here, since image destruction requires a
     * valid MWContext.
     */
    ET_PostFreeImageContext(NULL, decoder->image_context);

    /* Free JS context and decoder struct, window is gone. */
    JS_DestroyContext(cx);

    XP_DELETE(decoder);
}

#ifdef DEBUG
extern MochaDecoder *
lm_HoldBackCount(MochaDecoder *decoder)
{
    if (decoder) {
        XP_ASSERT(decoder->back_count >= 0);
        decoder->back_count++;
    }
    return decoder;
}

extern void
lm_DropBackCount(MochaDecoder *decoder)
{
    if (!decoder)
        return;
    XP_ASSERT(decoder->back_count > 0);
    if (--decoder->back_count <= 0) {
        decoder->back_count = 0;
        XP_ASSERT(decoder->forw_count >= 0);
        if (!decoder->forw_count)
            lm_DestroyWindow(decoder);
    }
}
#endif /* DEBUG */

JSBool
lm_InitWindowContent(MochaDecoder *decoder)
{
    JSContext *cx;
    JSObject *obj;

    cx = decoder->js_context;
    obj = decoder->window_object;
    if (!JS_InitStandardClasses(cx, obj))
        return JS_FALSE;

#ifdef JAVA
    if (JSJ_IsEnabled() && !JSJ_InitContext(cx, obj))
        return JS_FALSE;
#endif
    if (!JS_DefineProperties(cx, obj, window_props))
        return JS_FALSE;

    if (decoder->opener) {
        if (!JS_DefinePropertyWithTinyId(cx, obj, lm_opener_str, WIN_OPENER,
                                         OBJECT_TO_JSVAL(decoder->opener),
                                         NULL, NULL, JSPROP_ENUMERATE)) {
            return JS_FALSE;
        }
    }

    return (JSBool)(lm_DefineWindowProps(cx, decoder) &&
           JS_SetPrototype(cx, obj, decoder->event_capturer_prototype));
}

JSBool
lm_DefineWindowProps(JSContext *cx, MochaDecoder *decoder)
{
    JSObject *obj;

    obj = decoder->window_object;
    return (JSBool)(JS_DefineFunctions(cx, obj, lm_window_methods) &&
           lm_InitEventClasses(decoder) &&
           lm_InitDocumentClass(decoder) &&
           lm_DefineDocument(decoder, LO_DOCUMENT_LAYER_ID) &&
           lm_DefineHistory(decoder) &&
           lm_DefineLocation(decoder) &&
           lm_DefineCrypto(decoder) &&
           lm_DefineBarClasses(decoder) &&
           lm_InitLayerClass(decoder) &&
           lm_InitRectClass(decoder) &&
           lm_InitImageClass(decoder) &&
           lm_InitAnchorClass(decoder) &&
#ifdef DOM
		   lm_InitSpanClass(decoder) &&
		   lm_InitTransclusionClass(decoder) &&
#endif
           lm_InitInputClasses(decoder) &&
           lm_DefinePkcs11(decoder));
}

void
lm_FreeWindowContent(MochaDecoder *decoder, JSBool fromDiscard)
{
    JSContext *cx;
    JSObject *obj;
    JSNestingUrl * url, * next_url;

    /* Clear any stream that the user forgot to close. */
    lm_ClearDecoderStream(decoder, fromDiscard);

    /* Clear any pending timeouts and URL fetches. */
    lm_ClearWindowTimeouts(decoder);

    /* These flags should never be set, but if any are, we'll cope. */
    decoder->replace_location = JS_FALSE;
    decoder->resize_reload    = JS_FALSE;
    decoder->load_event_sent  = JS_FALSE;
    decoder->visited          = JS_FALSE;
    decoder->writing_input    = JS_FALSE;

    /* This flag may have been set by a script; clear it now. */
    decoder->principals_compromised = JS_FALSE;

    /* need to inline this since source_url is 'const char *' and the Mac will whine */
    if (decoder->source_url) {
	XP_FREE((char *) decoder->source_url);
	decoder->source_url = NULL;
    }

    for (url = decoder->nesting_url; url; url = next_url) {
	next_url = url->next;
	XP_FREE(url->str);
	XP_FREE(url);
    }
    decoder->nesting_url = NULL;

    /* Forgive and forget all excessive errors. */
    decoder->error_count = 0;

    decoder->active_form_id = 0;
    decoder->signature_ordinal = 0;

    /* Clear the event mask. */
    decoder->event_mask = 0;

    if (decoder->id_to_object_map) {
        PR_HashTableDestroy(decoder->id_to_object_map);
        decoder->id_to_object_map = NULL;
    }

    cx = decoder->js_context;
    decoder->firstVersion = JSVERSION_UNKNOWN;
    decoder->principals = NULL;
    if (decoder->early_access_list)
        lm_DestroyPrincipalsList(cx, decoder->early_access_list);
    decoder->early_access_list = NULL;

#define CLEAR(obj)      obj = NULL

    /* Clear all object prototype refs. */
    CLEAR(decoder->anchor_prototype);
    CLEAR(decoder->bar_prototype);
    CLEAR(decoder->document_prototype);
    CLEAR(decoder->event_prototype);
    CLEAR(decoder->event_capturer_prototype);
    CLEAR(decoder->event_receiver_prototype);
    CLEAR(decoder->form_prototype);
    CLEAR(decoder->image_prototype);
    CLEAR(decoder->input_prototype);
    CLEAR(decoder->layer_prototype);
    CLEAR(decoder->option_prototype);
    CLEAR(decoder->rect_prototype);
    CLEAR(decoder->url_prototype);

    /* Clear window sub-object refs. */
    if (decoder->document)
        lm_CleanUpDocumentRoots(decoder, decoder->document);
    CLEAR(decoder->document);
    CLEAR(decoder->history);
    CLEAR(decoder->location);
    CLEAR(decoder->navigator);
    CLEAR(decoder->components);
    CLEAR(decoder->crypto);
    CLEAR(decoder->screen);
    CLEAR(decoder->hardware);
    CLEAR(decoder->pkcs11);

    /* Drop ad-hoc GC roots, but not opener -- it survives unloads. */
    CLEAR(decoder->event_receiver);

#undef CLEAR

#undef DROP_AND_CLEAR

    obj = decoder->window_object;
    if (obj) {
	JS_ClearWatchPointsForObject(cx, obj);
        JS_ClearScope(cx, obj);
        (void) JS_DefinePropertyWithTinyId(cx, obj, lm_closed_str, WIN_CLOSED,
                                           JSVAL_FALSE, NULL, NULL,
                                           JSPROP_ENUMERATE|JSPROP_READONLY);
    }
}

void
LM_RemoveWindowContext(MWContext *context, History_entry * he)
{
    MochaDecoder *decoder;
    JSContext *cx;

    /* Do work only if this context has a JS decoder. */
    cx = context->mocha_context;
    if (!cx)
        return;
    decoder = JS_GetPrivate(cx, JS_GetGlobalObject(cx));

    /* Sever the bidirectional connection between context and decoder. */
    XP_ASSERT(decoder->window_context == context);
    context->mocha_context = NULL;
    decoder->window_context = NULL;
    lm_ClearDecoderStream(decoder, JS_TRUE);

    ET_il_SetGroupObserver(context, decoder->image_context, NULL, JS_FALSE);
        
    if (he) {
        /*
         * Set current history entry's saved window from its layout cell.
         * We need to do this rather than SHIST_SetCurrentDocWindowData()
         * because FE_FreeGridWindow (who calls us indirectly) has already
         * "stolen" session history for return to layout, who saves it in
         * parent session history in lo_InternalDiscardDocument().
         */
        he->savedData.Window = decoder;
        return;
    }

    /* This might call lm_DestroyWindow(decoder), so do it last. */
    LM_PutMochaDecoder(decoder);
}

extern void
LM_DropSavedWindow(MWContext *context, void *window)
{
    MochaDecoder *decoder = window;
#ifdef DEBUG
    extern PRThread *mozilla_thread;

    /* Our caller, SHIST_FreeHistoryEntry, must run on the mozilla thread. */
    XP_ASSERT(PR_CurrentThread() == mozilla_thread);
#endif

    et_PutMochaDecoder(context, decoder);
}
