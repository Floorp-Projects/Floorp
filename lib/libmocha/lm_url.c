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
 * JS reflection of the current Navigator URL (Location) object.
 *
 * Brendan Eich, 9/8/95
 */
#include "lm.h"
#include "xp.h"
#include "net.h"		/* for URL_Struct */
#include "shist.h"		/* for session history stuff */
#include "structs.h"		/* for MWContext */
#include "layout.h"		/* included via -I../layout */
#include "mkparse.h"		/* included via -I../libnet */

/* NB: these named properties use non-negative slots; code below knows that. */
/* Non-negative slots are protected by a permissions check; others aren't. */
enum url_slot {
    URL_HREF,			/* the entire URL as a string */
    URL_PROTOCOL,		/* protocol:... */
    URL_HOST,			/* protocol://host/... */
    URL_HOSTNAME,		/* protocol://hostname:... */
    URL_PORT,			/* protocol://hostname:port/... */
    URL_PATHNAME,		/* protocol://host/pathname[#?...] */
    URL_HASH,			/* protocol://host/pathname#hash */
    URL_SEARCH,			/* protocol://host/pathname?search */
    URL_TARGET,			/* target window or null */
    URL_TEXT,			/* text within A container */
    URL_X       = -1,           /* layout X coordinate */
    URL_Y       = -2            /* layout Y coordinate */
};

static JSPropertySpec url_props[] = {
    {"href",            URL_HREF,       JSPROP_ENUMERATE},
    {"protocol",        URL_PROTOCOL,   JSPROP_ENUMERATE},
    {"host",            URL_HOST,       JSPROP_ENUMERATE},
    {"hostname",        URL_HOSTNAME,   JSPROP_ENUMERATE},
    {"port",            URL_PORT,       JSPROP_ENUMERATE},
    {"pathname",        URL_PATHNAME,   JSPROP_ENUMERATE},
    {"hash",            URL_HASH,       JSPROP_ENUMERATE},
    {"search",          URL_SEARCH,     JSPROP_ENUMERATE},
    {"target",          URL_TARGET,     JSPROP_ENUMERATE},
    {"text",            URL_TEXT,       JSPROP_ENUMERATE | JSPROP_READONLY},
    {"x",               URL_X,          JSPROP_ENUMERATE | JSPROP_READONLY},
    {"y",               URL_Y,          JSPROP_ENUMERATE | JSPROP_READONLY},
    {0}
};

#define ParseURL(url,part)	((url) ? NET_ParseURL(url,part) : 0)

static JSBool
url_get_coord(JSContext *cx, JSURL *url, jsint slot, jsval *vp)
{
    int32 coord;
    LO_AnchorData *anchor_data;
    MWContext *context;

    LO_LockLayout();
    context = url->url_decoder->window_context;
    anchor_data = LO_GetLinkByIndex(context, url->layer_id, url->index);
    if (anchor_data && anchor_data->element) {
        switch (slot) {
        case URL_X:
            coord = anchor_data->element->lo_any.x;
            break;
            
        case URL_Y:
            coord = anchor_data->element->lo_any.y;
            break;

        default:
            LO_UnlockLayout();
            return JS_FALSE;
        }

	*vp = INT_TO_JSVAL(coord);
	LO_UnlockLayout();
	return JS_TRUE;
    }
    *vp = JSVAL_NULL;
    LO_UnlockLayout();
    return JS_TRUE;
}

extern JSClass lm_url_class;
extern JSClass lm_location_class;

PR_STATIC_CALLBACK(JSBool)
url_getProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    jsval slot;
    JSURL *url;
    JSString * str;
    char *port;
    const char *cstr, *tmp;

    if (!JSVAL_IS_INT(id))
	return JS_TRUE;

    slot = JSVAL_TO_INT(id);
    url = JS_GetInstancePrivate(cx, obj, &lm_url_class, NULL);
    if (!url) {
	url = JS_GetInstancePrivate(cx, obj, &lm_location_class, NULL);
	if (!url)
	    return JS_TRUE;
    }

    str = 0;
    cstr = 0;

    switch (slot) {
      case URL_HREF:
	str = url->href;
	break;

      case URL_PROTOCOL:
        if (url->href)
	    cstr = ParseURL(JS_GetStringBytes(url->href), GET_PROTOCOL_PART);
	break;

      case URL_HOST:
        if (url->href)
	    cstr = ParseURL(JS_GetStringBytes(url->href), GET_HOST_PART);
	break;

      case URL_HOSTNAME:
        if (url->href)
	    cstr = ParseURL(JS_GetStringBytes(url->href), GET_HOST_PART);
	if (cstr && (port = XP_STRCHR(cstr, ':')) != 0)
	    *port = '\0';
	break;

      case URL_PORT:
	if (url->href)
	    cstr = ParseURL(JS_GetStringBytes(url->href), GET_HOST_PART);
	if (cstr && (port = XP_STRCHR(cstr, ':')) != 0)
	    port++;
	else
	    port = "";
	tmp = cstr;
	cstr = JS_strdup(cx, port);
	XP_FREE((void *)tmp);
	break;

      case URL_PATHNAME:
        if (url->href)
	    cstr = ParseURL(JS_GetStringBytes(url->href), GET_PATH_PART);
	break;

      case URL_HASH:
        if (url->href)
	    cstr = ParseURL(JS_GetStringBytes(url->href), GET_HASH_PART);
	break;

      case URL_SEARCH:
	if (url->href)
	    cstr = ParseURL(JS_GetStringBytes(url->href), GET_SEARCH_PART);
	break;

      case URL_TARGET:
	if (!url->target) {
	    *vp = JSVAL_NULL;
	    return JS_TRUE;
	}
	str = url->target;
	break;

      case URL_TEXT:
	if (!url->text) {
	    *vp = JSVAL_NULL;
	    return JS_TRUE;
	}
	str = url->text;
	break;

    case URL_X:
    case URL_Y:
        return url_get_coord(cx, url, slot, vp);

      default:
	/* Don't mess with user-defined or method properties. */
          return JS_TRUE;
    }

    if (!str && cstr)
	str = JS_NewStringCopyZ(cx, cstr);
    if (cstr)
	XP_FREE((char *)cstr);
    if (!str)        
        return JS_FALSE;
    *vp = STRING_TO_JSVAL(str);
    return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
url_setProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    JSURL *url;
    const char *href, *name, *checked_href;
    char *new_href, *prop_name;
    JSBool free_href;
    jsval tmp;
    JSString *str;
    MWContext *context;
    LO_AnchorData *anchor_data;
    JSBool ok;
    jsint slot;

    url = JS_GetInstancePrivate(cx, obj, &lm_url_class, NULL);
    if (!url) {
	url = JS_GetInstancePrivate(cx, obj, &lm_location_class, NULL);
	if (!url)
	    return JS_TRUE;
    }

    /* If the property is setting an event handler we find out now so
     * that we can tell the front end to send the event.
     */
    if (JSVAL_IS_STRING(id)) {
	prop_name = JS_GetStringBytes(JSVAL_TO_STRING(id));
	if (XP_STRCASECMP(prop_name, lm_onClick_str) == 0 ||
	    XP_STRCASECMP(prop_name, lm_onMouseDown_str) == 0 ||
	    XP_STRCASECMP(prop_name, lm_onMouseOver_str) == 0 ||
	    XP_STRCASECMP(prop_name, lm_onMouseOut_str) == 0 ||
	    XP_STRCASECMP(prop_name, lm_onMouseUp_str) == 0) {
	    context = url->url_decoder->window_context;
	    if (context) {
		anchor_data = LO_GetLinkByIndex(context, url->layer_id,
						url->index);
		if (anchor_data)
		    anchor_data->event_handler_present = TRUE;
	    }
	}
	return JS_TRUE;
    }
	
    XP_ASSERT(JSVAL_IS_INT(id));
    slot = JSVAL_TO_INT(id);

    if (slot < 0) {
	/* Don't mess with user-defined or method properties. */
	return JS_TRUE;
    }

    if (!JSVAL_IS_STRING(*vp) &&
	!JS_ConvertValue(cx, *vp, JSTYPE_STRING, vp)) {
	return JS_FALSE;
    }

    ok = JS_TRUE;

    switch (slot) {
      case URL_HREF:
	url->href = JSVAL_TO_STRING(*vp);
	href = JS_GetStringBytes(url->href);
	free_href = JS_FALSE;
	break;

      case URL_PROTOCOL:
      case URL_HOST:
      case URL_HOSTNAME:
      case URL_PORT:
      case URL_PATHNAME:
      case URL_HASH:
      case URL_SEARCH:
	/* a component property changed -- recompute href. */
	new_href = NULL;

#define GET_SLOT(aslot, ptmp) {                                               \
    if (aslot == slot) {                                                      \
	*(ptmp) = *vp;                                                        \
    } else {                                                                  \
	if (!JS_GetElement(cx, obj, aslot, ptmp)) {                           \
	    if (new_href) XP_FREE(new_href);                                  \
	    return JS_FALSE;                                               \
	}                                                                     \
    }                                                                         \
}

#define ADD_SLOT(aslot, separator) {                                          \
    GET_SLOT(aslot, &tmp);                                                    \
    name = JS_GetStringBytes(JSVAL_TO_STRING(tmp));                           \
    if (*name) {                                                              \
	if (separator) StrAllocCat(new_href, separator);                      \
	StrAllocCat(new_href, name);                                          \
    }                                                                         \
}

	GET_SLOT(URL_PROTOCOL, &tmp);
	StrAllocCopy(new_href, JS_GetStringBytes(JSVAL_TO_STRING(tmp)));
	if (slot == URL_HOST) {
	    ADD_SLOT(URL_HOST, "//");
	} else {
	    ADD_SLOT(URL_HOSTNAME, "//");
	    ADD_SLOT(URL_PORT, ":");
	}
	ADD_SLOT(URL_PATHNAME, NULL);
	ADD_SLOT(URL_HASH, NULL);
	ADD_SLOT(URL_SEARCH, NULL);

	if (!new_href) {
	    JS_ReportOutOfMemory(cx);
	    return JS_FALSE;
	}

	free_href = JS_TRUE;
	href = new_href;
	str = JS_NewStringCopyZ(cx, href);
	if (!str) {
	    ok = JS_FALSE;
	    goto out;
	}
	url->href = str;
	break;

      case URL_TARGET:
	url->target = JSVAL_TO_STRING(*vp);
	if (url->index != URL_NOT_INDEXED) {
	    context = url->url_decoder->window_context;
	    if (context) {
		anchor_data = LO_GetLinkByIndex(context, url->layer_id,
                                                url->index);
		if (anchor_data) {
		    name = JS_GetStringBytes(url->target);
		    if (!lm_CheckWindowName(cx, name))
			return JS_FALSE;
		    if (!lm_SaveParamString(cx, &anchor_data->target, name))
			return JS_FALSE;
		}
	    }
	}
	/* Note early return, to bypass href update and freeing. */
	return JS_TRUE;

      default:
	/* Don't mess with a user-defined property. */
	return ok;
    }

    if (url->index != URL_NOT_INDEXED) {
	context = url->url_decoder->window_context;
	if (context) {
	    anchor_data = LO_GetLinkByIndex(context, url->layer_id, 
                                            url->index);
	    if (anchor_data) {
		checked_href = lm_CheckURL(cx, href, JS_FALSE);
		if (!checked_href ||
		    !lm_SaveParamString(cx, &anchor_data->anchor,
					checked_href)) {
		    ok = JS_FALSE;
		    goto out;
		}
		XP_FREE((char *)checked_href);
	    }
	}
    }

out:
    if (free_href && href)
	XP_FREE((char *)href);
    return ok;
}

PR_STATIC_CALLBACK(void)
url_finalize(JSContext *cx, JSObject *obj)
{
    JSURL *url;
    MochaDecoder *decoder;
    MWContext *context;
    LO_AnchorData *anchor_data;

    url = JS_GetPrivate(cx, obj);
    if (!url)
	return;
    decoder = url->url_decoder;
    if (url->index != URL_NOT_INDEXED) {
	context = decoder->window_context;
	if (context) {
	    LO_LockLayout();
	    anchor_data = LO_GetLinkByIndex(context, url->layer_id, 
                                            url->index);
	    if (anchor_data && anchor_data->mocha_object == obj)
		anchor_data->mocha_object = NULL;
	    LO_UnlockLayout();
	}
    }
    DROP_BACK_COUNT(decoder);
    JS_RemoveRoot(cx, &url->href);
    JS_RemoveRoot(cx, &url->target);
    JS_RemoveRoot(cx, &url->text);
    XP_DELETE(url);
}

JSClass lm_url_class = {
    "Url", JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub, url_getProperty, url_setProperty,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, url_finalize
};

PR_STATIC_CALLBACK(JSBool)
Url(JSContext *cx, JSObject *obj,
    uint argc, jsval *argv, jsval *rval)
{
    return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
url_toString(JSContext *cx, JSObject *obj,
	      uint argc, jsval *argv, jsval *rval)
{
    return url_getProperty(cx, obj, INT_TO_JSVAL(URL_HREF), rval);
}

static JSFunctionSpec url_methods[] = {
    {lm_toString_str,	url_toString,	0},
    {0}
};

JSURL *
lm_NewURL(JSContext *cx, MochaDecoder *decoder, LO_AnchorData *anchor_data,
          JSObject *document)
{
    JSObject *obj;
    JSURL *url;
    JSString *str;

    if (!decoder->url_prototype) {
	obj = JS_InitClass(cx, decoder->window_object, 
			   decoder->event_receiver_prototype, &lm_url_class, 
			   Url, 0, url_props, NULL, NULL, NULL);
	if (!obj)
	    return NULL;
	decoder->url_prototype = obj;
    }

    url = JS_malloc(cx, sizeof *url);
    if (!url)
	return NULL;
    XP_BZERO(url, sizeof *url);

    obj = JS_NewObject(cx, &lm_url_class, decoder->url_prototype, 
                       lm_GetOuterObject(decoder));
    if (!obj || !JS_SetPrivate(cx, obj, url)) {
	JS_free(cx, url);
	return NULL;
    }

    if (!JS_DefineFunctions(cx, obj, url_methods))
	return NULL;

    url->url_decoder = HOLD_BACK_COUNT(decoder);
    url->url_type = FORM_TYPE_NONE;
    url->index = URL_NOT_INDEXED;
    url->url_object = obj;

    str = JS_NewStringCopyZ(cx, (char *) anchor_data->anchor);
    if (!str)
	return NULL;
    url->href = str;
    if (!JS_AddNamedRoot(cx, &url->href, "url.href"))
	return NULL;

    if (anchor_data->target) {
	str = JS_NewStringCopyZ(cx, (char *) anchor_data->target);
	if (!str)
	    return NULL;
	url->target = str;
    }
    if (!JS_AddNamedRoot(cx, &url->target, "url.target"))
	return NULL;

    if (anchor_data->element && anchor_data->element->type == LO_TEXT) {
	str = lm_LocalEncodingToStr(decoder->window_context, 
		(char *) anchor_data->element->lo_text.text);
	if (!str)
	    return NULL;
	url->text = str;
    }
    if (!JS_AddNamedRoot(cx, &url->text, "url.text"))
	return NULL;

    return url;
}

static const char *
get_url_string(JSContext *cx, JSObject *obj)
{
    JSURL *url;
    MochaDecoder *decoder;
    MWContext *context;
    History_entry *he;
    const char *url_string;

    url = JS_GetInstancePrivate(cx, obj, &lm_location_class, NULL);
    if (!url)
	return NULL;
    decoder = url->url_decoder;

    context = decoder->window_context;
    if (!context)
	return NULL;
    he = SHIST_GetCurrent(&context->hist);
    if (he) {
	url_string = he->address;
	if (NET_URL_Type(url_string) == WYSIWYG_TYPE_URL &&
	    !(url_string = LM_SkipWysiwygURLPrefix(url_string))) {
	    url_string = he->address;
        }
        return url_string;
    }
    return NULL;
}

/*
 * Top-level window URL object, a reflection of the "Location:" GUI field.
 */
PR_STATIC_CALLBACK(JSBool)
loc_getProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    JSURL *url;
    MochaDecoder *decoder;
    MWContext *context;
    const char *url_string;
    JSString *str;

    if (!JSVAL_IS_INT(id) || JSVAL_TO_INT(id) < 0)
        return JS_TRUE;

    url = JS_GetInstancePrivate(cx, obj, &lm_location_class, NULL);
    if (!url)
	return JS_TRUE;
    decoder = url->url_decoder;

    context = decoder->window_context;
    if (!context)
	return JS_TRUE;
    if (context->type == MWContextMail || 
        context->type == MWContextNews ||
        context->type == MWContextMailMsg || 
        context->type == MWContextNewsMsg) {
        /*
	 * Don't give out the location of a the mail folder to a script
         * embedded in a mail message. Just return the empty string.
         */
	url->href = JSVAL_TO_STRING(JS_GetEmptyStringValue(cx));
    } else {
        /*
         * Only need to check permissions for native getters
         */
        if (!lm_CheckPermissions(cx, obj, JSTARGET_UNIVERSAL_BROWSER_READ))
	    return JS_FALSE;
        url_string = get_url_string(cx, obj);
        if (url_string && (!url->href || XP_STRCMP(JS_GetStringBytes(url->href), 
                                                   url_string) != 0))
        {
	    str = JS_NewStringCopyZ(cx, url_string);
	    if (!str)
		return JS_FALSE;
	    url->href = str;
        }
    }
    return url_getProperty(cx, obj, id, vp);
}

void
lm_ReplaceURL(MWContext *context, URL_Struct *url_struct)
{
    History_entry *he;

    he = SHIST_GetCurrent(&context->hist);
    if (!he)
	return;
    he->history_num = SHIST_GetIndex(&context->hist, he);
    he->replace = TRUE;
    url_struct->history_num = he->history_num;
}

JSBool
lm_GetURL(JSContext *cx, MochaDecoder *decoder, URL_Struct *url_struct)
{
    MWContext *context;

    context = decoder->window_context;
    if (!context) {
        NET_FreeURLStruct(url_struct);
        return JS_TRUE;
    }

    if (decoder->replace_location) {
	decoder->replace_location = JS_FALSE;
	lm_ReplaceURL(context, url_struct);
    }
    ET_PostGetUrl(context, url_struct);
    return JS_TRUE;
}

const char *
lm_CheckURL(JSContext *cx, const char *url_string, JSBool checkFile)
{
    char *protocol, *absolute;
    JSObject *obj;
    MochaDecoder *decoder;

    protocol = NET_ParseURL(url_string, GET_PROTOCOL_PART);
    if (!protocol || *protocol == '\0') {
        lo_TopState *top_state;

	obj = JS_GetGlobalObject(cx);
	decoder = JS_GetPrivate(cx, obj);

	LO_LockLayout();
	top_state = lo_GetMochaTopState(decoder->window_context);
        if (top_state && top_state->base_url) {
	    absolute = NET_MakeAbsoluteURL(top_state->base_url,
				           (char *)url_string);	/*XXX*/
            /* 
	     * Temporarily unlock layout so that we don't hold the lock
	     * across a call (lm_CheckPermissions) that may result in 
	     * synchronous event handling.
	     */
	    LO_UnlockLayout();
            if (!lm_CheckPermissions(cx, obj, 
                                     JSTARGET_UNIVERSAL_BROWSER_READ))
            {
                /* Don't leak information about the url of this page. */
                XP_FREEIF(absolute);
                return NULL;
            }
	    LO_LockLayout();
	} else {
	    absolute = NULL;
	}
	if (absolute) {
	    if (protocol) XP_FREE(protocol);
	    protocol = NET_ParseURL(absolute, GET_PROTOCOL_PART);
	}
	LO_UnlockLayout();
    } else {
	absolute = JS_strdup(cx, url_string);
	if (!absolute) {
	    XP_FREE(protocol);
	    return NULL;
	}
	decoder = NULL;
    }

    if (absolute) {

	/* Make sure it's a safe URL type. */
	switch (NET_URL_Type(protocol)) {
	  case FILE_TYPE_URL:
            if (checkFile) {
                const char *subjectOrigin = lm_GetSubjectOriginURL(cx);
                if (subjectOrigin == NULL) {
	            XP_FREE(protocol);
	            return NULL;
                }
                if (NET_URL_Type(subjectOrigin) != FILE_TYPE_URL &&
                    !lm_CanAccessTarget(cx, JSTARGET_UNIVERSAL_FILE_READ)) 
                {
                    XP_FREE(absolute);
                    absolute = NULL;
                }
            }
            break;
	  case FTP_TYPE_URL:
	  case GOPHER_TYPE_URL:
	  case HTTP_TYPE_URL:
	  case MAILTO_TYPE_URL:
	  case NEWS_TYPE_URL:
	  case RLOGIN_TYPE_URL:
	  case TELNET_TYPE_URL:
	  case TN3270_TYPE_URL:
	  case WAIS_TYPE_URL:
	  case SECURE_HTTP_TYPE_URL:
	  case URN_TYPE_URL:
	  case NFS_TYPE_URL:
	  case MOCHA_TYPE_URL:
	  case VIEW_SOURCE_TYPE_URL:
	  case NETHELP_TYPE_URL:
	  case WYSIWYG_TYPE_URL:
	  case LDAP_TYPE_URL:
#ifdef JAVA
	  case MARIMBA_TYPE_URL:
#endif
	    /* These are "safe". */
	    break;
	  case ABOUT_TYPE_URL:
	    if (XP_STRCASECMP(absolute, "about:blank") == 0)
		break;
	    if (XP_STRNCASECMP(absolute, "about:pics", 10) == 0)
		break;
	    /* these are OK if we are signed */
	    if (lm_CanAccessTarget(cx, JSTARGET_UNIVERSAL_BROWSER_READ))
		break;
	    /* FALL THROUGH */
	  default:
	    /* All others are naughty. */
	    XP_FREE(absolute);
	    absolute = NULL;
	    break;
	}
    }

    if (!absolute) {
	JS_ReportError(cx, "illegal URL method '%s'",
		       protocol && *protocol ? protocol : url_string);
    }
    if (protocol)
	XP_FREE(protocol);
    return absolute;
}

static JSBool
url_load(JSContext *cx, JSObject *obj, const char *url_string, 
         NET_ReloadMethod reload_how)
{
    JSURL *url;
    URL_Struct *url_struct;
    const char *referer;

    url = JS_GetPrivate(cx, obj);
    if (!url)
	return JS_TRUE;
    url_struct = NET_CreateURLStruct(url_string, reload_how);
    if (!url_struct) {
	JS_ReportOutOfMemory(cx);
	return JS_FALSE;
    }
    if (!(referer = lm_GetSubjectOriginURL(cx)) ||
	!(url_struct->referer = JS_strdup(cx, referer))) {
	NET_FreeURLStruct(url_struct);
	return JS_FALSE;
    }
    return lm_GetURL(cx, url->url_decoder, url_struct);
}

PR_STATIC_CALLBACK(JSBool)
loc_setProperty(JSContext *cx, JSObject *obj, jsval id,	jsval *vp)
{
    const char *url_string;
    JSString *str;
    jsval val;
    jsint slot;
    JSURL *url;

    if (!JSVAL_IS_INT(id))
	return JS_TRUE;

    slot = JSVAL_TO_INT(id);
    /* Setting these properties should not cause a FE_GetURL. */
    if (slot < 0 || slot == URL_TARGET)
        return url_setProperty(cx, obj, id, vp);

    /* Make sure vp is a string. */
    if (!JSVAL_IS_STRING(*vp) &&
	!JS_ConvertValue(cx, *vp, JSTYPE_STRING, vp)) {
	return JS_FALSE;
    }

    /* Two cases: setting href vs. setting a component property. */
    if (slot == URL_HREF || slot == URL_PROTOCOL) {
	/* Make sure the URL is absolute and sanity-check its protocol. */
	url_string = JS_GetStringBytes(JSVAL_TO_STRING(*vp));
	url_string = lm_CheckURL(cx, url_string, JS_TRUE);
	if (!url_string)
	    return JS_FALSE;
	str = JS_NewStringCopyZ(cx, url_string);
	XP_FREE((char *)url_string);
	if (!str)
	    return JS_FALSE;
	val = STRING_TO_JSVAL(str);
	vp = &val;
    } else {
	/* Get href from session history before setting a piece of it. */
	if (!loc_getProperty(cx, obj, INT_TO_JSVAL(URL_HREF), &val))
	    return JS_FALSE;
    }

    /* Set slot's property. */
    if (!url_setProperty(cx, obj, id, vp))
	return JS_FALSE;

    url = JS_GetPrivate(cx, obj);
    if (!url)
	return JS_TRUE;
    if (url->href)
        url_string = JS_GetStringBytes(url->href);
    else
	url_string = "";
    return url_load(cx, obj, url_string, NET_DONT_RELOAD);
}

JSClass lm_location_class = {
    "Location", JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub, loc_getProperty, loc_setProperty,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, url_finalize
};

PR_STATIC_CALLBACK(JSBool)
Location(JSContext *cx, JSObject *obj,
	 uint argc, jsval *argv, jsval *rval)
{
    return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
loc_toString(JSContext *cx, JSObject *obj,
	      uint argc, jsval *argv, jsval *rval)
{
    return loc_getProperty(cx, obj, INT_TO_JSVAL(URL_HREF), rval);
}

PR_STATIC_CALLBACK(JSBool)
loc_assign(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    JSURL *url;
    jsval v;
    JSObject *locobj;

    if (!JS_InstanceOf(cx, obj, &lm_location_class, NULL))  {
        if(!JS_LookupProperty(cx, obj, "location", &v) ||
	   !JSVAL_IS_OBJECT(v))  {
	    return JS_FALSE;
	}
	locobj = JSVAL_TO_OBJECT(v);
    }  else  {
        locobj = obj;
    }
    if (!(url = JS_GetInstancePrivate(cx, locobj, &lm_location_class, NULL)))
        return JS_FALSE;
    if (!loc_setProperty(cx, locobj, INT_TO_JSVAL(URL_HREF), vp))
	return JS_FALSE;
    *vp = OBJECT_TO_JSVAL(url->url_object);
    return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
loc_reload(JSContext *cx, JSObject *obj,
	   uint argc, jsval *argv, jsval *rval)
{
    if (!JS_InstanceOf(cx, obj, &lm_location_class, argv))
        return JS_FALSE;
    return url_load(cx, obj, get_url_string(cx, obj),
		    (JSVAL_IS_BOOLEAN(argv[0]) && JSVAL_TO_BOOLEAN(argv[0]))
		    ? NET_SUPER_RELOAD
		    : NET_NORMAL_RELOAD);
}

PR_STATIC_CALLBACK(JSBool)
loc_replace(JSContext *cx, JSObject *obj,
	    uint argc, jsval *argv, jsval *rval)
{
    JSURL *url;
    jsid   jid;
    JSBool ans;

    if (!(url = JS_GetInstancePrivate(cx, obj, &lm_location_class, argv)))
        return JS_FALSE;
    url->url_decoder->replace_location = JS_TRUE;
    /* Note: loc_assign ignores the id in favor of its own id anyway */
    ans = loc_assign(cx, obj, 0, argv);
    *rval = *argv;
    return ans;
}

static JSFunctionSpec loc_methods[] = {
    {lm_toString_str,   loc_toString,  0},
    {lm_reload_str,     loc_reload,     1},
    {lm_replace_str,    loc_replace,    1},
    {0}
};

JSObject *
lm_DefineLocation(MochaDecoder *decoder)
{
    JSObject *obj;
    JSContext *cx;
    JSURL *url;

    obj = decoder->location;
    if (obj)
	return obj;

    cx = decoder->js_context;
    url = JS_malloc(cx, sizeof *url);
    if (!url)
	return NULL;
    XP_BZERO(url, sizeof *url);

    obj = JS_InitClass(cx, decoder->window_object, NULL, &lm_location_class,
		       Location, 0, url_props, loc_methods, NULL, NULL);
    if (!obj || !JS_SetPrivate(cx, obj, url)) {
	JS_free(cx, url);
	return NULL;
    }

    /* XXX common subroutine this and above with lm_NewURL */
    if (!JS_AddNamedRoot(cx, &url->href, "loc.text"))
	return NULL;
    if (!JS_AddNamedRoot(cx, &url->target, "loc.target"))
	return NULL;
    if (!JS_AddNamedRoot(cx, &url->text, "loc.text"))
	return NULL;

    if (!JS_DefineProperty(cx, decoder->window_object, lm_location_str,
			   OBJECT_TO_JSVAL(obj), NULL, loc_assign,
			   JSPROP_ENUMERATE)) {
	return NULL;
    }
    if (!JS_DefineProperty(cx, decoder->document, lm_location_str, 
                           OBJECT_TO_JSVAL(obj), NULL, loc_assign, 
                           JSPROP_ENUMERATE)) {
	return NULL;
    }

    /* Define the Location object (the current URL). */
    url->url_decoder = HOLD_BACK_COUNT(decoder);
    url->url_type = FORM_TYPE_NONE;
    url->url_object = obj;
    url->index = URL_NOT_INDEXED;
    decoder->location = obj;
    return obj;
}
