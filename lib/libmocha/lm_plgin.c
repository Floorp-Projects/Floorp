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
 * JS reflection of Navigator Plugins.
 *
 * This file contains implementations of several JS objects:
 *
 *     + JSPluginList, a lazily-filled array of all installed
 *       plug-ins, based on the information exported from lib/plugin
 *       by the functions in np.h.  The plug-in properties in the
 *       array are named by plug-in name, so you can do an array
 *       lookup by name, e.g. plugins["Shockwave"].
 *
 *     + JSMIMETypeList, a lazily-filled array of all handled
 *       MIME types, based in the information exported from libnet
 *       by the functions in net.h.  The type properties in the
 *       array are named by type, so you can do an array lookup by
 *       type, e.g. mimetypes["video/quicktime"].
 *
 *     + JSPlugin, the reflection of an installed plug-in, with
 *       static properties for the plug-in's name, file name, etc.
 *       and dynamic properties for the MIME types supported by
 *       the plug-in.
 *
 *     + JSMIMEType, the reflection of an individual MIME type,
 *       with properties for the type, file extensions, platform-
 *       specific file type, description of the type, and enabled
 *		 plug-in.  The enabled plug-in property is a weak object
 *		 reference marked as JSPROP_BACKEDGE to break the cycle of
 *		 references from plug-in to mime type to plug-in.
 *
 */

#include "lm.h"
#include "prmem.h"
#include "np.h"
#include "net.h"


/*
 * -----------------------------------------------------------------------
 *
 * Data types
 *
 * -----------------------------------------------------------------------
 */

typedef struct JSMIMEType
{
    MochaDecoder	       *decoder;
    JSObject			   *obj;
    JSString	*type;				/* MIME type itself, e.g. "text/html" */
    JSString    *description;		/* English description of MIME type */
    JSString    *suffixes;			/* Comma-separated list of file suffixes */
    JSObject			   *enabledPluginObj;	/* Plug-in enabled for this MIME type */
    void				   *fileType;			/* Platform-specific file type */
} JSMIMEType;

typedef struct JSPlugin
{
    MochaDecoder    *decoder;
    JSObject	*obj;
    JSString    *name;           	/* Name of plug-in */
    JSString    *filename;       	/* Name of file on disk */
    JSString    *description;    	/* Descriptive HTML (version, etc.) */
    NPReference	plugref;			/* Opaque reference to private plugin object */
    uint32	length;         	/* Total number of mime types we have */
    XP_Bool	reentrant;			/* Flag to halt recursion of getProperty */
} JSPlugin;


typedef struct JSPluginList
{
    MochaDecoder       *decoder;
    JSObject	       *obj;
    uint32		length;				/* Total number of plug-ins */
    XP_Bool		reentrant;			/* Flag to halt recursion of getProperty */
} JSPluginList;


typedef struct JSMIMETypeList
{
    MochaDecoder       *decoder;
    JSObject	       *obj;
    uint32 		length;	    /* Total number of mime types */
    PRPackedBool	reentrant;  /* Flag to halt recursion of getProperty */
} JSMIMETypeList;



/*
 * -----------------------------------------------------------------------
 *
 * Function protos (all private to this file)
 *
 * -----------------------------------------------------------------------
 */

static JSMIMEType*	mimetype_create_self(JSContext *cx, MochaDecoder* decoder, char* type,
					    char* description, char** suffixes, uint32 numExtents, void* fileType);
JSBool PR_CALLBACK	mimetype_getProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp);
void PR_CALLBACK	mimetype_finalize(JSContext *cx, JSObject *obj);

static JSPlugin* 	plugin_create_self(JSContext *cx, MochaDecoder* decoder, char* name,
					   char* filename, char* description, NPReference plugref);
static JSMIMEType*	plugin_create_mimetype(JSContext* cx, JSPlugin* plugin,
					       XP_Bool byName, const char* targetName, jsint targetSlot);
JSBool PR_CALLBACK	plugin_getProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp);
JSBool PR_CALLBACK	plugin_resolve_name(JSContext *cx, JSObject *obj, jsval id);
void PR_CALLBACK 	plugin_finalize(JSContext *cx, JSObject *obj);

static JSPluginList*	pluginlist_create_self(JSContext *cx, MochaDecoder* decoder, JSObject* parent_obj);
static JSPlugin*	pluginlist_create_plugin(JSContext *cx, JSPluginList *pluginlist,
						 XP_Bool byName, const char* targetName, jsint targetSlot);
JSBool PR_CALLBACK	pluginlist_getProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp);
JSBool PR_CALLBACK	pluginlist_resolve_name(JSContext *cx, JSObject *obj, jsval id);
JSBool PR_CALLBACK      pluginlist_enumerate(JSContext *cx, JSObject *obj);
void PR_CALLBACK	pluginlist_finalize(JSContext *cx, JSObject *obj);
JSBool PR_CALLBACK	pluginlist_refresh(JSContext *cx, JSObject *obj,
					   uint argc, jsval *argv, jsval *rval);

static JSMIMETypeList*	mimetypelist_create_self(JSContext *cx, MochaDecoder* decoder);
static JSMIMEType*	mimetypelist_create_mimetype(JSContext* cx, JSMIMETypeList* mimetypelist,
						     XP_Bool byName, const char* targetName, jsint targetSlot);
JSBool PR_CALLBACK	mimetypelist_getProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp);
JSBool PR_CALLBACK	mimetypelist_resolve_name(JSContext *cx, JSObject *obj, jsval id);
JSBool PR_CALLBACK      mimetypelist_enumerate(JSContext *cx, JSObject *obj);
void PR_CALLBACK	mimetypelist_finalize(JSContext *cx, JSObject *obj);

/*
 * -----------------------------------------------------------------------
 *
 * Reflection of a MIME type.
 *
 * -----------------------------------------------------------------------
 */

enum mimetype_slot
{
    MIMETYPE_TYPE           = -1,
    MIMETYPE_DESCRIPTION    = -2,
    MIMETYPE_SUFFIXES       = -3,
    MIMETYPE_ENABLEDPLUGIN  = -4
};


static JSPropertySpec mimetype_props[] =
{
    {"type",          MIMETYPE_TYPE,	      JSPROP_ENUMERATE|JSPROP_READONLY},
    {"description",   MIMETYPE_DESCRIPTION,   JSPROP_ENUMERATE|JSPROP_READONLY},
    {"suffixes",      MIMETYPE_SUFFIXES,      JSPROP_ENUMERATE|JSPROP_READONLY},
    {"enabledPlugin", MIMETYPE_ENABLEDPLUGIN, JSPROP_ENUMERATE|JSPROP_READONLY},
    {0}
};


static JSClass mimetype_class =
{
    "MimeType",	JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub,
    mimetype_getProperty, mimetype_getProperty, JS_EnumerateStub,
    JS_ResolveStub, JS_ConvertStub, mimetype_finalize
};

PR_STATIC_CALLBACK(JSBool)
MimeType(JSContext *cx, JSObject *obj, uint argc, jsval *argv, jsval *rval)
{
    return JS_TRUE;
}

/* Constructor method for a JSMIMEType object */
static JSMIMEType*
mimetype_create_self(JSContext *cx, MochaDecoder* decoder,
		     char* type, char* description, char ** suffixes,
                     uint32 numExtents, void* fileType)
{
    JSObject *obj;
    JSMIMEType *mimetype;

    mimetype = JS_malloc(cx, sizeof(JSMIMEType));
    if (!mimetype)
        return NULL;
    XP_BZERO(mimetype, sizeof *mimetype);

    obj = JS_NewObject(cx, &mimetype_class, NULL, NULL);
    if (!obj || !JS_SetPrivate(cx, obj, mimetype)) {
	JS_free(cx, mimetype);
        return NULL;
    }

    if (!JS_DefineProperties(cx, obj, mimetype_props))
	return NULL;

    mimetype->decoder = HOLD_BACK_COUNT(decoder);
    mimetype->obj = obj;
    mimetype->fileType = fileType;

    mimetype->type = JS_NewStringCopyZ(cx, type);
    if (!mimetype->type || !JS_LockGCThing(cx, mimetype->type))
        return NULL;

    mimetype->description = JS_NewStringCopyZ(cx, description);
    if (!mimetype->description || !JS_LockGCThing(cx, mimetype->description))
        return NULL;


    /*
     * Assemble the list of file extensions into a string.
     * The extensions are stored in an array of individual strings, so we
     * first traverse the array to see how big the concatenated string will
     * be, then allocate the memory and re-traverse the array to build the
     * string.  Each extension is seperated by trailing comma and space.
     */
    {
        uint32 index;
        uint32 totalSize = 0;
        char *extStr;

        if (numExtents > 0)
        {
            for (index = 0; index < numExtents; index++)
                totalSize += XP_STRLEN(suffixes[index]);

            /* Add space for ', ' after each extension */
            totalSize += (2 * (numExtents - 1));
        }

        /* Total size plus terminator */
        extStr = XP_ALLOC(totalSize + 1);
        if (! extStr)
            return NULL;

        extStr[0] = 0;

        for (index = 0; index < numExtents; index++)
        {
            extStr = strcat(extStr, suffixes[index]);
            if (index < numExtents - 1)
                extStr = strcat(extStr, ", ");
        }
        mimetype->suffixes = JS_NewStringCopyZ(cx, extStr);
        XP_FREE(extStr);
        if (!mimetype->suffixes || !JS_LockGCThing(cx, mimetype->suffixes))
            return NULL;
    }

    /*
     * Conjure up the JS object for the plug-in enabled for this
     * MIME type.  First we get the name of the plug-in from libplugin;
     * then we look up the plugin object by name in the global plug-in
     * list (it's actually a "resolve", not a "lookup", so that the
     * plug-in object will be created if it doesn't already exist).
     */
    if (type) {
	char* pluginName = NPL_FindPluginEnabledForType(type);
	if (pluginName)
	{
	    /* Look up the global plugin list object */
	    jsval val;
	    if (JS_LookupProperty(cx, decoder->navigator, "plugins", &val) &&
		JSVAL_IS_OBJECT(val))
	    {
		/* Look up the desired plugin by name in the list */
		JSObject* pluginListObj = JSVAL_TO_OBJECT(val);
		if (JS_GetProperty(cx, pluginListObj, pluginName, &val) &&
		    JSVAL_IS_OBJECT(val))
		{
		    mimetype->enabledPluginObj = JSVAL_TO_OBJECT(val);
		}
	    }

	    XP_FREE(pluginName);
	}
    }

    return mimetype;
}


JSBool PR_CALLBACK
mimetype_getProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    JSMIMEType *mimetype;
    JSString *str;
    jsint slot;

    if (!JSVAL_IS_INT(id))
	return JS_TRUE;

    slot = JSVAL_TO_INT(id);

    mimetype = JS_GetInstancePrivate(cx, obj, &mimetype_class, NULL);
    if (!mimetype)
	return JS_TRUE;

    switch (slot)
    {
        case MIMETYPE_TYPE:
            str = mimetype->type;
            break;

        case MIMETYPE_DESCRIPTION:
            str = mimetype->description;
            break;

        case MIMETYPE_SUFFIXES:
            str = mimetype->suffixes;
            break;

        case MIMETYPE_ENABLEDPLUGIN:
            *vp = OBJECT_TO_JSVAL(mimetype->enabledPluginObj);
            return JS_TRUE;

        default:
            /* don't mess with user-defined props. */
            return JS_TRUE;
    }

    if (str)
	*vp = STRING_TO_JSVAL(str);
    else
        *vp = JS_GetEmptyStringValue(cx);
    return JS_TRUE;
}


void PR_CALLBACK
mimetype_finalize(JSContext *cx, JSObject *obj)
{
    JSMIMEType *mimetype;

    mimetype = JS_GetPrivate(cx, obj);
    if (!mimetype)
	return;

    DROP_BACK_COUNT(mimetype->decoder);
    JS_UnlockGCThing(cx, mimetype->type);
    JS_UnlockGCThing(cx, mimetype->description);
    JS_UnlockGCThing(cx, mimetype->suffixes);
    JS_free(cx, mimetype);
}






/*
 * -----------------------------------------------------------------------
 *
 * Reflection of an installed plug-in.
 *
 * -----------------------------------------------------------------------
 */

enum plugin_slot
{
    PLUGIN_NAME         = -1,
    PLUGIN_FILENAME     = -2,
    PLUGIN_DESCRIPTION  = -3,
    PLUGIN_ARRAY_LENGTH = -4
};


static JSPropertySpec plugin_props[] =
{
    {"name",        PLUGIN_NAME,            JSPROP_ENUMERATE | JSPROP_READONLY},
    {"filename",    PLUGIN_FILENAME,        JSPROP_ENUMERATE | JSPROP_READONLY},
    {"description", PLUGIN_DESCRIPTION,     JSPROP_ENUMERATE | JSPROP_READONLY},
    {"length",      PLUGIN_ARRAY_LENGTH,    JSPROP_ENUMERATE | JSPROP_READONLY},
    {0}
};

static JSClass plugin_class =
{
    "Plugin", JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub,
    plugin_getProperty, plugin_getProperty, JS_EnumerateStub,
    plugin_resolve_name, JS_ConvertStub, plugin_finalize
};

PR_STATIC_CALLBACK(JSBool)
Plugin(JSContext *cx, JSObject *obj, uint argc, jsval *argv, jsval *rval)
{
    return JS_TRUE;
}

/* Constructor method for a JSPlugin object */
static JSPlugin*
plugin_create_self(JSContext *cx, MochaDecoder* decoder, char* name,
				   char* filename, char* description, NPReference plugref)
{
    JSObject *obj;
    JSPlugin *plugin;

    plugin = JS_malloc(cx, sizeof(JSPlugin));
    if (!plugin)
        return NULL;
    XP_BZERO(plugin, sizeof *plugin);

    obj = JS_NewObject(cx, &plugin_class, NULL, NULL);
    if (!obj || !JS_SetPrivate(cx, obj, plugin)) {
	JS_free(cx, plugin);
	return NULL;
    }

    if (!JS_DefineProperties(cx, obj, plugin_props))
	return NULL;

    /* Fill out static property fields */
    plugin->decoder = HOLD_BACK_COUNT(decoder);
    plugin->obj = obj;
    plugin->plugref = plugref;

    plugin->name = JS_NewStringCopyZ(cx, name);
    if (!plugin->name || !JS_LockGCThing(cx, plugin->name))
	return NULL;

    plugin->filename = JS_NewStringCopyZ(cx, filename);
    if (!plugin->filename || !JS_LockGCThing(cx, plugin->filename))
	return NULL;

    plugin->description = JS_NewStringCopyZ(cx, description);
    if (!plugin->description || !JS_LockGCThing(cx, plugin->description))
	return NULL;

    /* Count how many MIME types we have */
    {
        NPReference typeref = NPRefFromStart;
        while (NPL_IteratePluginTypes(&typeref, plugref, NULL, NULL, NULL, NULL))
        	plugin->length++;
    }

    return plugin;
}


/* Create a mimetype property for a plugin for a specified slot or name */
static JSMIMEType*
plugin_create_mimetype(JSContext* cx, JSPlugin* plugin, XP_Bool byName,
					  const char* targetName, jsint targetSlot)
{
    NPMIMEType type = NULL;
    char** suffixes = NULL;
    char* description = NULL;
    void* fileType = NULL;
    NPReference typeref = NPRefFromStart;
    jsint slot = 0;
    XP_Bool found = FALSE;

	/* Search for the type (by type name or slot number) */
    while (NPL_IteratePluginTypes(&typeref, plugin->plugref, &type,
    				  &suffixes, &description, &fileType))
	{
		if (byName)
			found = (type && (XP_STRCMP(targetName, type) == 0));
		else
			found = (targetSlot == slot);

		if (found)
			break;

        slot++;
	}

    /* Found the mime type, so create an object and property */
    if (found) {
    	JSMIMEType* mimetype;
    	uint32 numExtents = 0;

    	while (suffixes[numExtents])
    	    numExtents++;

        mimetype = mimetype_create_self(cx, plugin->decoder, type, description,
        				suffixes, numExtents, fileType);
	if (mimetype) {
	    char *name;
	    jsval val;

	    name = JS_GetStringBytes(mimetype->type);
	    val = OBJECT_TO_JSVAL(mimetype->obj);
	    JS_DefineProperty(cx, plugin->obj, name, val, NULL, NULL,
			      JSPROP_ENUMERATE | JSPROP_READONLY);
	    JS_AliasElement(cx, plugin->obj, name, slot);
	}

        return mimetype;
    }
    return NULL;
}


JSBool PR_CALLBACK
plugin_getProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    JSPlugin *plugin;
    JSString *str;
    jsint slot;

    if (!JSVAL_IS_INT(id))
	return JS_TRUE;

    slot = JSVAL_TO_INT(id);

    plugin = JS_GetInstancePrivate(cx, obj, &plugin_class, NULL);
    if (!plugin)
	return JS_TRUE;

    /* Prevent infinite recursion through call to GetSlot below */
    if (plugin->reentrant)
	return JS_TRUE;

    switch (slot)
    {
        case PLUGIN_NAME:
            str = plugin->name;
            break;

        case PLUGIN_FILENAME:
            str = plugin->filename;
            break;

        case PLUGIN_DESCRIPTION:
            str = plugin->description;
            break;

        case PLUGIN_ARRAY_LENGTH:
            *vp = INT_TO_JSVAL(plugin->length);
            return JS_TRUE;

        default:
            /* Don't mess with a user-defined property. */
            if (slot >= 0 && slot < (jsint) plugin->length) {
		/* Search for an existing JSMIMEType for this slot */
		JSObject* mimetypeObj = NULL;
		jsval val;

		plugin->reentrant = TRUE;
		if (JS_GetElement(cx, obj, slot, &val) && JSVAL_IS_OBJECT(val))
		    mimetypeObj = JSVAL_TO_OBJECT(val);
		else {
		    JSMIMEType* mimetype;
		    mimetype = plugin_create_mimetype(cx, plugin, FALSE, NULL, slot);
		    if (mimetype)
			mimetypeObj = mimetype->obj;
                }
		plugin->reentrant = FALSE;

		*vp = OBJECT_TO_JSVAL(mimetypeObj);
		return JS_TRUE;
            }
      	    return JS_FALSE;
    }

    if (str)
	*vp = STRING_TO_JSVAL(str);
    else
	*vp = JS_GetEmptyStringValue(cx);
    return JS_TRUE;
}


JSBool PR_CALLBACK
plugin_resolve_name(JSContext *cx, JSObject *obj, jsval id)
{
    JSPlugin* plugin;
    jsval val;
    const char * name;

    if (!JSVAL_IS_STRING(id))
	return JS_TRUE;

    name = JS_GetStringBytes(JSVAL_TO_STRING(id));

    plugin = JS_GetPrivate(cx, obj);
    if (!plugin)
	return JS_TRUE;

    /* Prevent infinite recursion through call to LookupProperty below */
    if (plugin->reentrant)
	return JS_TRUE;

    /* Look for a JSMIMEType object by this name already in our list */
    plugin->reentrant = TRUE;
    if (JS_LookupProperty(cx, obj, name, &val) && JSVAL_IS_OBJECT(val)) {
	plugin->reentrant = FALSE;
    	return JS_TRUE;
    }
    plugin->reentrant = FALSE;

    /* We don't already have the object, so make a new one */
    if (plugin_create_mimetype(cx, plugin, TRUE, name, 0))
	return JS_TRUE;

    /* Still no match for the name?  Must be some other property, or invalid. */
    return JS_TRUE;
}


void PR_CALLBACK
plugin_finalize(JSContext *cx, JSObject *obj)
{
    JSPlugin* plugin;

    plugin = JS_GetPrivate(cx, obj);
    if (!plugin)
	    return;
    DROP_BACK_COUNT(plugin->decoder);
    JS_UnlockGCThing(cx, plugin->name);
    JS_UnlockGCThing(cx, plugin->filename);
    JS_UnlockGCThing(cx, plugin->description);
    JS_free(cx, plugin);
}






/*
 * -----------------------------------------------------------------------
 *
 * Reflection of the list of installed plug-ins.
 * The only static property is the array length;
 * the array elements (JSPlugins) are added
 * lazily when referenced.
 *
 * -----------------------------------------------------------------------
 */

enum pluginlist_slot
{
    PLUGINLIST_ARRAY_LENGTH = -1
};


static JSPropertySpec pluginlist_props[] =
{
    {"length",  PLUGINLIST_ARRAY_LENGTH,    JSPROP_READONLY},
    {0}
};


static JSClass pluginlist_class =
{
    "PluginArray", JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub,
    pluginlist_getProperty, pluginlist_getProperty, pluginlist_enumerate,
    pluginlist_resolve_name, JS_ConvertStub, pluginlist_finalize
};


static JSFunctionSpec pluginlist_methods[] =
{
    {"refresh", pluginlist_refresh, 0},
    {0}
};


PR_STATIC_CALLBACK(JSBool)
PluginList(JSContext *cx, JSObject *obj,
	 uint argc, jsval *argv, jsval *rval)
{
    return JS_TRUE;
}


/* Constructor method for a JSPluginList object */
static JSPluginList*
pluginlist_create_self(JSContext *cx, MochaDecoder* decoder, JSObject* parent_obj)
{
    JSObject *obj;
    JSPluginList *pluginlist;

    pluginlist = JS_malloc(cx, sizeof(JSPluginList));
    if (!pluginlist)
        return NULL;
    XP_BZERO(pluginlist, sizeof *pluginlist);

    obj = JS_InitClass(cx, parent_obj, NULL, &pluginlist_class,
    		       PluginList, 0, pluginlist_props, 
		       pluginlist_methods, NULL, NULL); 

    if (!obj || !JS_SetPrivate(cx, obj, pluginlist)) {
        JS_free(cx, pluginlist);
        return NULL;
    }

    pluginlist->decoder = HOLD_BACK_COUNT(decoder);
    pluginlist->obj = obj;

    {
        /* Compute total number of plug-ins (potential slots) */
        NPReference ref = NPRefFromStart;
        while (NPL_IteratePluginFiles(&ref, NULL, NULL, NULL))
            pluginlist->length++;
    }

    return pluginlist;
}


/* Look up the plugin for the specified slot of the plug-in list */
static JSPlugin*
pluginlist_create_plugin(JSContext *cx, JSPluginList *pluginlist, XP_Bool byName,
					  	 const char* targetName, jsint targetSlot)
{
    char* plugname = NULL;
    char* filename = NULL;
    char* description = NULL;
    NPReference plugref = NPRefFromStart;
    jsint slot = 0;
    XP_Bool found = FALSE;

	/* Search for the plug-in (by name or slot number) */
    while (NPL_IteratePluginFiles(&plugref, &plugname, &filename, &description)) {
    	if (byName)
	    found = (plugname && (XP_STRCMP(targetName, plugname) == 0));
	else
	    found = (targetSlot == slot);

	if (found)
	    break;

        slot++;
    }

	/* Found the plug-in, so create an object and property */
    if (found) {
	JSPlugin* plugin;
	plugin = plugin_create_self(cx, pluginlist->decoder, plugname,
				    filename, description, plugref);
        if (plugin) {
	    char *name;
	    jsval val;

	    name = JS_GetStringBytes(plugin->name);
	    val = OBJECT_TO_JSVAL(plugin->obj);
	    JS_DefineProperty(cx, pluginlist->obj, name, val, NULL, NULL,
			      JSPROP_ENUMERATE | JSPROP_READONLY);
	    JS_AliasElement(cx, pluginlist->obj, name, slot);
        }

        return plugin;
    }
    return NULL;
}


JSBool PR_CALLBACK
pluginlist_getProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    JSPluginList *pluginlist;
    jsint slot;

    if (!JSVAL_IS_INT(id))
	return JS_TRUE;

    slot = JSVAL_TO_INT(id);

    pluginlist = JS_GetInstancePrivate(cx, obj, &pluginlist_class, NULL);
    if (!pluginlist)
	return JS_TRUE;

    /* Prevent infinite recursion through call to GetSlot below */
    if (pluginlist->reentrant)
	return JS_TRUE;

    switch (slot) {
        case PLUGINLIST_ARRAY_LENGTH:
            *vp = INT_TO_JSVAL(pluginlist->length);
            break;

        default:
            /* Don't mess with a user-defined property. */
            if (slot >= 0 && slot < (jsint) pluginlist->length) {

		/* Search for an existing JSPlugin for this slot */
		JSObject* pluginObj = NULL;
		jsval val;

		pluginlist->reentrant = TRUE;
		if (JS_GetElement(cx, obj, slot, &val) && JSVAL_IS_OBJECT(val)) {
    		    pluginObj = JSVAL_TO_OBJECT(val);
		}
		else {
		    JSPlugin* plugin;
		    plugin = pluginlist_create_plugin(cx, pluginlist, FALSE, NULL, slot);
		    if (plugin)
    			pluginObj = plugin->obj;
		}
		pluginlist->reentrant = FALSE;

		*vp = OBJECT_TO_JSVAL(pluginObj);
		return JS_TRUE;
	    }
    }

    return JS_TRUE;
}


JSBool PR_CALLBACK
pluginlist_resolve_name(JSContext *cx, JSObject *obj, jsval id)
{
    JSPluginList* pluginlist;
    jsval val;
    const char * name;

    if (!JSVAL_IS_STRING(id))
	return JS_TRUE;

    name = JS_GetStringBytes(JSVAL_TO_STRING(id));

    pluginlist = JS_GetPrivate(cx, obj);
    if (!pluginlist)
	return JS_TRUE;

    /* Prevent infinite recursion through call to LookupProperty below */
    if (pluginlist->reentrant)
	return JS_TRUE;

    /* Look for a JSMIMEType object by this name already in our list */
    pluginlist->reentrant = TRUE;
    if (JS_LookupProperty(cx, obj, name, &val) && JSVAL_IS_OBJECT(val))
    {
	pluginlist->reentrant = FALSE;
    	return JS_TRUE;
    }
    pluginlist->reentrant = FALSE;

    /* We don't already have the object, so make a new one */
    if (pluginlist_create_plugin(cx, pluginlist, TRUE, name, 0))
	return JS_TRUE;

    /* Still no match for the name?  Must be some other property, or invalid. */
    return JS_TRUE;
}

JSBool PR_CALLBACK
pluginlist_enumerate(JSContext *cx, JSObject *obj)
{
    return JS_TRUE;
}

void PR_CALLBACK
pluginlist_finalize(JSContext *cx, JSObject *obj)
{
    JSPluginList* pluginlist;

    pluginlist = JS_GetPrivate(cx, obj);
    if (!pluginlist)
	return;
    DROP_BACK_COUNT(pluginlist->decoder);
    JS_free(cx, pluginlist);
}


JSBool PR_CALLBACK
pluginlist_refresh(JSContext *cx, JSObject *obj,
	  uint argc, jsval *argv, jsval *rval)
{
    JSBool value = JS_FALSE;
    JSPluginList* pluginlist;
    JSObject* navigator;
    MochaDecoder* decoder;
    NPError error = NPERR_NO_ERROR;

    if (!(pluginlist = JS_GetInstancePrivate(cx, obj, &pluginlist_class, argv)))
        return JS_FALSE;
    decoder = pluginlist->decoder;

    /*
     * Evaluate the parameter (if any).  If the parameter
     * is missing or can't be evaluated, default to FALSE.
     */
    if (argc > 0)
	(void) JS_ValueToBoolean(cx, argv[0], &value);

    /* Synchronously execute this function on the Mozilla thread */
    error = (NPError) ET_npl_RefreshPluginList(decoder->window_context, value);
    if (error != NPERR_NO_ERROR)
    {
	/* XXX should JS_ReportError() here, but can't happen currently */
	return JS_FALSE;
    }

    /*
     * Remove references to the existing navigator object,
     * and make a new one.  If scripts have outstanding
     * references to the old objects, they'll still be
     * valid, but if they reference navigator.plugins
     * anew they'll see any new plug-ins registered by
     * NPL_RefreshPlugins.
     */
    navigator = decoder->navigator;
    decoder->navigator = NULL;  /* Prevent lm_DefineNavigator from short-circuiting */
  /*  lm_crippled_decoder = NULL; */
    lm_crippled_decoder->navigator = NULL; 
    lm_crippled_decoder->navigator = lm_DefineNavigator(decoder);
    if (!decoder->navigator)
    {
	/*
	 * We failed to create a new navigator object, so
	 * restore the old one (saved in a local variable).
	 */
	decoder->navigator = navigator;
	return JS_FALSE;
    }
    return JS_TRUE;
}



/*
 * -----------------------------------------------------------------------
 *
 * Reflection of the list of handled MIME types.
 *
 * -----------------------------------------------------------------------
 */

enum mimetypelist_slot
{
    MIMETYPELIST_ARRAY_LENGTH   = -1
};


static JSPropertySpec mimetypelist_props[] =
{
    {"length",  MIMETYPELIST_ARRAY_LENGTH,  JSPROP_READONLY},
    {0}
};


static JSClass mimetypelist_class =
{
    "MimeTypeArray", JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub,
    mimetypelist_getProperty, mimetypelist_getProperty, mimetypelist_enumerate,
    mimetypelist_resolve_name, JS_ConvertStub, mimetypelist_finalize
};


/* Constructor method for a JSMIMETypeList object */
static JSMIMETypeList*
mimetypelist_create_self(JSContext *cx, MochaDecoder* decoder)
{
    JSObject *obj;
    JSMIMETypeList *mimetypelist;

    mimetypelist = JS_malloc(cx, sizeof(JSMIMETypeList));
    if (!mimetypelist)
        return NULL;
    XP_BZERO(mimetypelist, sizeof *mimetypelist);

    obj = JS_NewObject(cx, &mimetypelist_class, NULL, NULL);

    if (!obj || !JS_SetPrivate(cx, obj, mimetypelist)) {
        JS_free(cx, mimetypelist);
        return NULL;
    }

    if (!JS_DefineProperties(cx, obj, mimetypelist_props))
	return NULL;

    mimetypelist->decoder = HOLD_BACK_COUNT(decoder);
    mimetypelist->obj = obj;

    /*
     * Count the number of types in the list.
     * We can't just go by the number of items
     * in the list, since it contains encodings, too.
     */
    {
	XP_List* cinfoList = cinfo_MasterListPointer();
	NET_cdataStruct* cdata = NULL;
	mimetypelist->length = 0;
	while (cinfoList)
	{
    	    cdata = cinfoList->object;
    	    if (cdata && cdata->ci.type)
    		mimetypelist->length++;
	    cinfoList = cinfoList->next;
	}
    }

    return mimetypelist;
}

/*
 * Common code to take a cdata and create a mimetype
 */
static JSMIMEType*
define_mimetype(JSContext* cx, JSMIMETypeList* mimetypelist, 
		NET_cdataStruct* cdata, jsint targetSlot)
{

    JSMIMEType* mimetype;
    mimetype = mimetype_create_self(cx, mimetypelist->decoder, cdata->ci.type,
        			    cdata->ci.desc, cdata->exts, cdata->num_exts, NULL);
    if (mimetype) {
	char *name;
	name = JS_GetStringBytes(mimetype->type);
	JS_DefineProperty(cx, mimetypelist->obj, name, 
			  OBJECT_TO_JSVAL(mimetype->obj), NULL, NULL,
			  JSPROP_ENUMERATE | JSPROP_READONLY);
	JS_AliasElement(cx, mimetypelist->obj, name, targetSlot);
    }

    return mimetype;
}

/* Create a mimetype property for a specified slot or name */
static JSMIMEType*
mimetypelist_create_mimetype(JSContext* cx, JSMIMETypeList* mimetypelist,
			     XP_Bool byName, const char* targetName, jsint targetSlot)
{
    XP_List* cinfoList = cinfo_MasterListPointer();
    NET_cdataStruct* cdata = NULL;
    XP_Bool found = FALSE;

    if (byName) {
	/* Look up by name */
	targetSlot = 0;
	while (cinfoList) {
            cdata = cinfoList->object;
            if (cdata) {
		char* type = cdata->ci.type;
		if (type && (XP_STRCMP(type, targetName) == 0)) {
		    found = TRUE;
		    break;
		}
	    }
	    targetSlot++;
	    cinfoList = cinfoList->next;
	}
    }
    else {
    	/*
    	 * Look up by slot.
    	 * The slot number does not directly correspond to list index,
    	 * since not all items in the list correspond to properties
    	 * (encodings are in list list as well as types).
    	 */
    	uint32 count = targetSlot + 1;
    	while (cinfoList) {
    	    cdata = cinfoList->object;
    	    if (cdata && cdata->ci.type)
    		count--;
    		    
    	    if (count == 0) {
    		found = TRUE;
    		break;
    	    }

	    cinfoList = cinfoList->next;
	}
    }

    if (found) 
	return define_mimetype(cx, mimetypelist, cdata, targetSlot);

    return NULL;
}


JSBool PR_CALLBACK
mimetypelist_getProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    JSMIMETypeList *mimetypelist;
    jsint slot;

    if (!JSVAL_IS_INT(id))
	return JS_TRUE;

    slot = JSVAL_TO_INT(id);

    mimetypelist = JS_GetInstancePrivate(cx, obj, &mimetypelist_class, NULL);
    if (!mimetypelist)
	return JS_TRUE;

    /* Prevent infinite recursion through call to GetSlot below */
    if (mimetypelist->reentrant)
	return JS_TRUE;

    switch (slot)
    {
        case MIMETYPELIST_ARRAY_LENGTH:
            *vp = INT_TO_JSVAL(mimetypelist->length);
            break;

        default:
            /* Don't mess with a user-defined property. */
            if (slot >= 0 && slot < (jsint) mimetypelist->length) {
		/* Search for an existing JSMIMEType for this slot */
		JSObject* mimetypeObj = NULL;
		jsval val = JSVAL_VOID;

		mimetypelist->reentrant = TRUE;
		if (JS_GetElement(cx, obj, slot, &val) && JSVAL_IS_OBJECT(val)) {
		    mimetypeObj = JSVAL_TO_OBJECT(val);
		}
		else {
		    JSMIMEType* mimetype;
		    mimetype = mimetypelist_create_mimetype(cx, mimetypelist, FALSE, NULL, slot);
		    if (mimetype)
			mimetypeObj = mimetype->obj;
		}
		mimetypelist->reentrant = FALSE;

		*vp = OBJECT_TO_JSVAL(mimetypeObj);
                return JS_TRUE;
            }
            break;
    }

    return JS_TRUE;
}


JSBool PR_CALLBACK
mimetypelist_resolve_name(JSContext *cx, JSObject *obj, jsval id)
{
    JSMIMETypeList* mimetypelist;
    jsval val;
    const char * name;

    if (!JSVAL_IS_STRING(id))
	return JS_TRUE;

    name = JS_GetStringBytes(JSVAL_TO_STRING(id));

    mimetypelist = JS_GetPrivate(cx, obj);
    if (!mimetypelist)
	return JS_TRUE;

    /* Prevent infinite recursion through call to LookupProperty below */
    if (mimetypelist->reentrant)
	return JS_TRUE;

    /* Look for a JSMIMEType object by this name already in our list */
    mimetypelist->reentrant = TRUE;
    if (JS_LookupProperty(cx, obj, name, &val) && JSVAL_IS_OBJECT(val))
    {
	mimetypelist->reentrant = FALSE;
    	return JS_TRUE;
    }
    mimetypelist->reentrant = FALSE;

    /* We don't already have the object, so make a new one */
    if (mimetypelist_create_mimetype(cx, mimetypelist, TRUE, name, 0))
	return JS_TRUE;

    /* Still no match for the name?  Must be some other property, or invalid. */
    return JS_TRUE;
}

JSBool PR_CALLBACK
mimetypelist_enumerate(JSContext *cx, JSObject *obj)
{
    JSMIMETypeList* mimetypelist;
    XP_List* cinfoList = cinfo_MasterListPointer();
    NET_cdataStruct* cdata = NULL;
    jsint targetSlot = 0;

    mimetypelist = JS_GetPrivate(cx, obj);
    if (!mimetypelist)
	return JS_TRUE;

    while (cinfoList) {
        cdata = cinfoList->object;
        if (cdata)
	    define_mimetype(cx, mimetypelist, cdata, targetSlot++);
	cinfoList = cinfoList->next;
    }

    return JS_TRUE;   
}

void PR_CALLBACK
mimetypelist_finalize(JSContext *cx, JSObject *obj)
{
    JSMIMETypeList *mimetypelist;

    mimetypelist = JS_GetPrivate(cx, obj);
    if (!mimetypelist)
	return;
    DROP_BACK_COUNT(mimetypelist->decoder);
    JS_free(cx, mimetypelist);
}




/*
 * Exported entry point, called from lm_nav.c.
 * This function creates the JSPluginList object.  The
 * properties for the installed plug-ins are instantiated
 * lazily in pluginlist_resolve_name.
 */
JSObject*
lm_NewPluginList(JSContext *cx, JSObject *parent_obj)
{
    MochaDecoder* decoder;
    JSPluginList* pluginlist;

    decoder = JS_GetPrivate(cx, JS_GetGlobalObject(cx));
    pluginlist = pluginlist_create_self(cx, decoder, parent_obj);
    return (pluginlist ? pluginlist->obj : NULL);
}




/*
 * Exported entry point to this file, called from lm_nav.c.
 * This function creates the JSMIMETypeList object.  The
 * properties for the MIME types are instantiated
 * lazily in mimetypelist_resolve_name.
 */
JSObject*
lm_NewMIMETypeList(JSContext *cx)
{
    MochaDecoder* decoder;
    JSMIMETypeList* mimetypelist;

    decoder = JS_GetPrivate(cx, JS_GetGlobalObject(cx));
    mimetypelist = mimetypelist_create_self(cx, decoder);
    return (mimetypelist ? mimetypelist->obj : NULL);
}

JSBool
lm_DefinePluginClasses(MochaDecoder *decoder)
{
    JSContext *cx = decoder->js_context;

    if (!JS_InitClass(cx, decoder->window_object, NULL, &mimetype_class,
		      MimeType, 0, mimetype_props, NULL, NULL, NULL))
	return JS_FALSE;

    if (!JS_InitClass(cx, decoder->window_object, NULL, &plugin_class,
		      Plugin, 0, plugin_props, NULL, NULL, NULL))
        return JS_FALSE;

    return JS_TRUE;
}
