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

#include "xp.h"
#include "jsapi.h"
#include "libstyle.h"
#include "xp_mcom.h"
#include "jsspriv.h"
#include "jssrules.h"
#include "jsscope.h"
#include "jsatom.h"

#ifndef NSPR20
#include "prhash.h"
#else
#include "plhash.h"
#endif

extern void LO_SetStyleObjectRefs(MWContext *context, void *tags, void *classes, void *ids);

/**** Declaration of JavaScript classes ****/

extern JSClass Tags_class;
extern JSClass Classes_class;
extern JSClass Tag_class;

static JSBool
is_valid_jsss_prop(char *prop)
{
	if(!prop)
		return FALSE;

	switch(XP_TO_UPPER(*prop))
	{
		case 'A':
			if(!strcasecomp(prop, "absolute"))
				return TRUE;
			else if(!strcasecomp(prop, "activeColor"))
				return TRUE;
			else if(!strcasecomp(prop, "align"))
				return TRUE;
			break;
		case 'B':
			if(!strcasecomp(prop, "borderWidth"))
				return TRUE;
			else if(!strcasecomp(prop, "borderStyle"))
				return TRUE;
			else if(!strcasecomp(prop, "borderColor"))
				return TRUE;
			else if(!strcasecomp(prop, "borderRightWidth"))
				return TRUE;
			else if(!strcasecomp(prop, "borderLeftWidth"))
				return TRUE;
			else if(!strcasecomp(prop, "borderTopWidth"))
				return TRUE;
			else if(!strcasecomp(prop, "borderBottomWidth"))
				return TRUE;
			else if(!strcasecomp(prop, "backgroundColor"))
				return TRUE;
			else if(!strcasecomp(prop, "backgroundImage"))
				return TRUE;
			else if(!strcasecomp(prop, "backgroundRepeat"))
				return TRUE;
			break;
		case 'C':
			if(!strcasecomp(prop, "clear"))
				return TRUE;
			else if(!strcasecomp(prop, "color"))
				return TRUE;
			else if(!strcasecomp(prop, "clip"))
				return TRUE;
			break;
		case 'D':
			if(!strcasecomp(prop, "display"))
				return TRUE;
			break;
		case 'F':
			if(!strcasecomp(prop, "fontSize"))
				return TRUE;
			else if(!strcasecomp(prop, "fontFamily"))
				return TRUE;
			else if(!strcasecomp(prop, "fontWeight"))
				return TRUE;
			else if(!strcasecomp(prop, "fontStyle"))
				return TRUE;
			break;
		case 'H':
			if(!strcasecomp(prop, "height"))
				return TRUE;
			break;
		case 'I':
			if(!strcasecomp(prop, "includeSource"))
				return TRUE;
			break;
		case 'L':
			if(!strcasecomp(prop, "lineHeight"))
				return TRUE;
			else if(!strcasecomp(prop, "listStyleType"))
				return TRUE;
			else if(!strcasecomp(prop, "layerBackgroundColor"))
				return TRUE;
			else if(!strcasecomp(prop, "layerBackgroundImage"))
				return TRUE;
			else if(!strcasecomp(prop, "linkColor"))
				return TRUE;
			else if(!strcasecomp(prop, "linkBorder"))
				return TRUE;
			else if(!strcasecomp(prop, "_layer_width"))
				return TRUE;
			else if(!strcasecomp(prop, "left"))
				return TRUE;
			break;
		case 'M':
			if(!strcasecomp(prop, "marginLeft"))
				return TRUE;
			else if(!strcasecomp(prop, "marginRight"))
				return TRUE;
			else if(!strcasecomp(prop, "marginTop"))
				return TRUE;
			else if(!strcasecomp(prop, "marginBottom"))
				return TRUE;
			break;
		case 'O':
			if(!strcasecomp(prop, "overflow"))
				return TRUE;
			break;
		case 'P':
			if(!strcasecomp(prop, "padding"))
				return TRUE;
			else if(!strcasecomp(prop, "paddingLeft"))
				return TRUE;
			else if(!strcasecomp(prop, "paddingRight"))
				return TRUE;
			else if(!strcasecomp(prop, "paddingTop"))
				return TRUE;
			else if(!strcasecomp(prop, "paddingBottom"))
				return TRUE;
			else if(!strcasecomp(prop, "position"))
				return TRUE;
			else if(!strcasecomp(prop, "pageBreakBefore"))
				return TRUE;
			else if(!strcasecomp(prop, "pageBreakAfter"))
				return TRUE;
			break;
		case 'R':
			if(!strcasecomp(prop, "relative"))
				return TRUE;
			break;
		case 'T':
			if(!strcasecomp(prop, "top"))
				return TRUE;
			else if(!strcasecomp(prop, "textTransform"))
				return TRUE;
			else if(!strcasecomp(prop, "textAlign"))
				return TRUE;
			else if(!strcasecomp(prop, "textIndent"))
				return TRUE;
			else if(!strcasecomp(prop, "textDecoration"))
				return TRUE;
			break;
		case 'V':
			if(!strcasecomp(prop, "visitedColor"))
				return TRUE;
			else if(!strcasecomp(prop, "verticalAlign"))
				return TRUE;
			else if(!strcasecomp(prop, "visibility"))
				return TRUE;
			break;
		case 'W':
			if(!strcasecomp(prop, "width"))
				return TRUE;
			if(!strcasecomp(prop, "whiteSpace"))
				return TRUE;
			break;
		case 'Z':
			if(!strcasecomp(prop, "zIndex"))
				return TRUE;
		default:
			XP_ASSERT(0);
			break;
	}

	return FALSE;
};

int PR_CALLBACK
jss_CompareStringsNoCase(const void *str1, const void *str2)
{
    return XP_STRCASECMP(str1, str2) == 0;
}

PRHashNumber PR_CALLBACK
jss_HashStringNoCase(const void *key)
{
    PRHashNumber h;
    const unsigned char *s;

    h = 0;
    for (s = key; *s; s++)
        h = (h >> 28) ^ (h << 4) ^ toupper(*s);
    return h;
}

/**** Finalizer for JSSTags and JSSClasses ****/

int PR_CALLBACK
jss_DestroyTags(PRHashEntry *he, int i, void *arg)
{
	XP_ASSERT(he->value);
	if (he->value)
		jss_DestroyTag((StyleTag *)he->value);

	return HT_ENUMERATE_NEXT;  /* keep enumerating */
}

void PR_CALLBACK
jss_FinalizeStyleObject(JSContext *mc, JSObject *obj)
{
	StyleObject *tags;

	XP_ASSERT(JS_InstanceOf(mc, obj, &Tags_class, 0) || JS_InstanceOf(mc, obj, &Classes_class, 0));

	/* Note: the prototype objects won't have any private data */
	tags = JS_GetPrivate(mc, obj);
	if (tags) {
		XP_ASSERT(tags->type >= JSSTags && tags->type <= JSSClass);
			
		if (tags->table) {
			if (tags->type != JSSClasses)
				PR_HashTableEnumerateEntries(tags->table, jss_DestroyTags, 0);
			PR_HashTableDestroy(tags->table);
		}
	
		if (tags->name)
			XP_FREE(tags->name);

		XP_FREE(tags);
	}
}

/**** Implementation of JavaScript JSSTags classes ****/

/* Constructor for JSSTags class */
JSBool PR_CALLBACK
TagsConstructor (JSContext *mc, JSObject *obj, unsigned argc, jsval *argv, jsval *rval)
{
    /* Check arguments first */
    XP_ASSERT(JS_InstanceOf(mc, obj, &Tags_class, 0));

    /* We don't expect any arguments */
    XP_ASSERT(argc == 0);

    return JS_TRUE;
}

/* Helper routine to determine the specificity of a tag */
static uint32
jss_TagSpecificity(StyleObject *obj)
{
	/* Compute the specificity for a tag of this type */
	switch (obj->type) {
		case JSSTags:
			return JSS_SPECIFICITY(1, 0, 0);

		case JSSIds:
			return JSS_SPECIFICITY(0, 0, 1);

		case JSSClasses:
			XP_ASSERT(FALSE);
			break;

		case JSSClass:
			XP_ASSERT(obj->name);
			if (obj->name && XP_STRCMP(obj->name, "all") == 0)
				return JSS_SPECIFICITY(0, 1, 0);
			else
				return JSS_SPECIFICITY(1, 1, 0);
			break;
	}

	return 0;
}

/* Called by JS to resolve names */
JSBool PR_CALLBACK
Tags_ResolveName(JSContext *mc, JSObject *obj, jsval id)
{
	if (JSVAL_IS_STRING(id)) {
		char	    *name = JS_GetStringBytes(JSVAL_TO_STRING(id));
 		JSObject    *tag_obj;
		StyleObject *tags;
		StyleTag    *tag;

        XP_ASSERT(JS_InstanceOf(mc, obj, &Tags_class, 0));

   		/* Get the pointer to the hash table */
		tags = JS_GetPrivate(mc, obj);
        if (!tags)
			return JS_TRUE;  /* we must be getting called before we've been initialized */

		/*
		 * See if there is an existing StyleTag object with this name (all names
		 * are case insensitive like in CSS)
		 */
		if (tags->table) {
			PRHashEntry *he, **hep;
			
			hep = PR_HashTableRawLookup(tags->table, tags->table->keyHash(name), name);
			if ((he = *hep) != 0) {
				/* Alias this name with the original name */
				JS_AliasProperty(mc, obj, (const char *)he->key, name);
				return JS_TRUE;
			}
		}
        
		/* Lazily create tag objects when needed */
		tag_obj = JS_NewObject(mc, &Tag_class, 0, obj);
		if (!tag_obj)
			return JS_FALSE;
		
		/* Create a new StyleTag structure */
		tag = jss_NewTag(name);
		if (!tag) {
			JS_ReportOutOfMemory(mc);
			return JS_FALSE;
		}
		JS_SetPrivate(mc, tag_obj, tag);

		/* We don't create the hash table until it's actually needed */
		if (!tags->table) {
            /* All lookups must be case insensitive */
    		tags->table = PR_NewHashTable(8, (PRHashFunction)jss_HashStringNoCase,
				(PRHashComparator)jss_CompareStringsNoCase, PR_CompareValues, 0, 0);

			if (!tags->table) {
				jss_DestroyTag(tag);
				JS_ReportOutOfMemory(mc);
				return JS_FALSE;
			}
		}

		/* Add this tag to the hash table */
		PR_HashTableAdd(tags->table, (const void *)tag->name, tag);
		tag->specificity = jss_TagSpecificity(tags);

		/* Give the object a name */
		return JS_DefineProperty(mc, obj, name, OBJECT_TO_JSVAL(tag_obj), 0, 0,
			JSPROP_READONLY | JSPROP_PERMANENT);
	}

	return JS_TRUE;	
}

/**** Implementation of JavaScript JSSClasses class ****/

/* Constructor for JSSClasses class */
JSBool PR_CALLBACK
ClassesConstructor (JSContext *mc, JSObject *obj, unsigned argc, jsval *argv, jsval *rval)
{
    /* Check arguments first */
    XP_ASSERT(JS_InstanceOf(mc, obj, &Classes_class, 0));

    /* We don't expect any arguments */
    XP_ASSERT(argc == 0);

    return JS_TRUE;
}

/* Called by JS to resolve names */
JSBool PR_CALLBACK
Classes_ResolveName(JSContext *mc, JSObject *obj, jsval id)
{
  	if (JSVAL_IS_STRING(id)) {
	 	char*		 name = JS_GetStringBytes(JSVAL_TO_STRING(id));
		StyleObject *classes;
		JSObject*	 tags_obj;
		StyleObject *tags;

		/* Get the pointer to the hash table */
		classes = JS_GetPrivate(mc, obj);
        if (!classes)
			return JS_TRUE;  /* we must be getting called before we've been initialized */

		/*
		 * See if there is an existing StyleObject object with this name (all names
		 * are case insensitive like in CSS)
		 */
		if (classes->table) {
			PRHashEntry *he, **hep;
			
			hep = PR_HashTableRawLookup(classes->table, classes->table->keyHash(name), name);
			if ((he = *hep) != 0) {
				/* Alias this name with the original name */
				JS_AliasProperty(mc, obj, (const char *)he->key, name);
				return JS_TRUE;
			}
		}
        
		/* Create an instance of the "Tags" class for the specified class name */
		tags_obj = JS_NewObject(mc, &Tags_class, 0, obj);
		if (!tags_obj) {
			JS_ReportOutOfMemory(mc);
			return JS_FALSE;
		}

		/* Create a StyleObject data structure */
		tags = (StyleObject *)XP_CALLOC(1, sizeof(StyleObject));
		if (!tags) {
			JS_ReportOutOfMemory(mc);
			return JS_FALSE;
		}

		/* Make a copy of the name */
		tags->name = XP_STRDUP(name);
		if (!tags->name) {
			XP_FREE(tags);
			JS_ReportOutOfMemory(mc);
			return JS_FALSE;
		}

		/* We don't create the hash table until it's actually needed */
		if (!classes->table) {
            /* All lookups must be case insensitive */
			classes->table = PR_NewHashTable(8, (PRHashFunction)jss_HashStringNoCase,
				(PRHashComparator)jss_CompareStringsNoCase, PR_CompareValues, 0, 0);

			if (!classes->table) {
				if (tags->name)
					XP_FREE(tags->name);
				XP_FREE(tags);
				JS_ReportOutOfMemory(mc);
				return JS_FALSE;
			}
		}
		
		/* Add it to the hash table */
		PR_HashTableAdd(classes->table, (const void *)tags->name, tags);
		tags->type = JSSClass;
		JS_SetPrivate(mc, tags_obj, tags);

		return JS_DefineProperty(mc, obj, name, OBJECT_TO_JSVAL(tags_obj), 0, 0,
			JSPROP_READONLY | JSPROP_PERMANENT);
	}

    return JS_TRUE;	
}

/**** Implementation of JavaScript JSSTag class ****/

/* Constructor for JSSTag class */
JSBool PR_CALLBACK
TagConstructor (JSContext *mc, JSObject *obj, unsigned argc, jsval *argv, jsval *rval)
{
    /* Check arguments first */
    XP_ASSERT(JS_InstanceOf(mc, obj, &Tag_class, 0));

    /* We don't expect any arguments */
    XP_ASSERT(argc == 0);

    return JS_TRUE;
}

/* Destroys a list of properties */
void
jss_DestroyProperties(StyleProperty *p)
{
	StyleProperty *next;

	while (p) {
		next = p->next;

		/* Free the name */
		if (p->name)
			XP_FREE(p->name);

		/* If the value is a string then free it, too */
		if (p->tag == JSVAL_STRING) {
			XP_ASSERT(p->u.strVal);
			if (p->u.strVal)
				XP_FREE(p->u.strVal);
		}

		XP_FREE(p);
		p = next;
	}
}

static JSBool
jss_PropertySetValue(JSContext *mc, StyleProperty *prop, jsval *vp)
{
	JSString *str;

	if (JSVAL_IS_BOOLEAN(*vp)) {
		prop->u.bVal = JSVAL_TO_BOOLEAN(*vp);

	} else if (JSVAL_IS_INT(*vp)) {
		prop->u.nVal = JSVAL_TO_INT(*vp);
		/* XXX - JSVAL_TAG doesn't do the right thing for ints */
		prop->tag = JSVAL_INT;
		return JS_TRUE;

	} else if (JSVAL_IS_DOUBLE(*vp)) {
		prop->u.dVal = *JSVAL_TO_DOUBLE(*vp);

	} else {
		XP_ASSERT(JSVAL_IS_STRING(*vp));
		str = JSVAL_TO_STRING(*vp);
		prop->u.strVal = XP_STRDUP(JS_GetStringBytes(str));
	}
	
	prop->tag = JSVAL_TAG(*vp);
	return JS_TRUE;
}

static JSBool
jss_PropertyGetValue(JSContext *mc, StyleProperty *prop, jsval *vp)
{
	switch (prop->tag) {
		case JSVAL_BOOLEAN:
			*vp = BOOLEAN_TO_JSVAL(prop->u.bVal);
			break;

		case JSVAL_STRING:
			*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(mc, prop->u.strVal));
			break;

		case JSVAL_INT:
			*vp = INT_TO_JSVAL(prop->u.nVal);
			break;

		case JSVAL_DOUBLE:
			JS_NewDoubleValue(mc, prop->u.dVal, vp);
			break;

		default:
			XP_ASSERT(FALSE);
			break;
	}

	return JS_TRUE;
}

static StyleProperty *
jss_TagGetProperty(JSContext *mc, StyleTag *tag, char *propname)
{
	StyleProperty *prop;

	/* Look for the property in our list of properties */
	for (prop = tag->properties; prop; prop = prop->next) {
		if (XP_STRCASECMP(prop->name, propname) == 0)
			return prop;
	}

	return 0;
}

static JSBool
jss_TagAddProperty(JSContext *mc, StyleTag *tag, char *name, jsval *vp)
{
	StyleProperty *prop;

	/* Create a new property */
	prop = (StyleProperty *)XP_CALLOC(1, sizeof(StyleProperty));
	if (!prop) {
		JS_ReportOutOfMemory(mc);
		return JS_FALSE;
	}
	
	/* Set the name and property value */
	prop->name = XP_STRDUP(name);
	if (!prop->name) {
		XP_FREE(prop);
		JS_ReportOutOfMemory(mc);
		return JS_FALSE;
	}
	jss_PropertySetValue(mc, prop, vp);

	/* Add it to the list */
	prop->next = tag->properties;
	tag->properties = prop;

	return JS_TRUE;
}

/* Creates a new property if necessary */
static JSBool
jss_TagSetProperty(JSContext *mc, StyleTag *tag, char *name, jsval *vp)
{
	StyleProperty *prop;

	/* See if we already have the property defined */
	prop = jss_TagGetProperty(mc, tag, name);

	if (prop)
		jss_PropertySetValue(mc, prop, vp);
	else
		jss_TagAddProperty(mc, tag, name, vp);
	
	return JS_TRUE;
}

/* Called by JS so we can handle the get operation */
JSBool PR_CALLBACK
Tag_GetProperty(JSContext *mc, JSObject *obj, jsval id, jsval *vp)
{
	if (JSVAL_IS_STRING(id)) {
		char		  *name = JS_GetStringBytes(JSVAL_TO_STRING(id));
		StyleTag 	  *tag = JS_GetPrivate(mc, obj);
		StyleProperty *prop;
	
		XP_ASSERT(tag);
		
		LO_LockLayout();
		prop = jss_TagGetProperty(mc, tag, name);
		if (prop)
			jss_PropertyGetValue(mc, prop, vp);
		LO_UnlockLayout();
	}
	
	return JS_TRUE;
}

/* Called by JS so we can handle the set operation */
JSBool PR_CALLBACK
Tag_SetProperty(JSContext *mc, JSObject *obj, jsval id, jsval *vp)
{
	/* We only support number, strings, and booleans */
	if (!JSVAL_IS_NUMBER(*vp) && !JSVAL_IS_BOOLEAN(*vp) && !JSVAL_IS_STRING(*vp))
		return JS_TRUE;

	if (JSVAL_IS_STRING(id)) {
		char		  *name = JS_GetStringBytes(JSVAL_TO_STRING(id));
		StyleTag 	  *tag = JS_GetPrivate(mc, obj);

		XP_ASSERT(tag);
	
		if (tag) {
			LO_LockLayout();
			jss_TagSetProperty(mc, tag, name, vp);
			LO_UnlockLayout();
		}
	}

	return JS_TRUE;
}

/* Called by JS to resolve names */
JSBool PR_CALLBACK
Tag_ResolveName(JSContext *mc, JSObject *obj, jsval id)
{
      if (JSVAL_IS_STRING(id)) {
              char   *name = JS_GetStringBytes(JSVAL_TO_STRING(id));

              /*
               * We need to have a resolve function for the case where there's a "with" clause, e.g.
               * "with (tags.H1) {color = 'red'}". In this case we need to resolve the property or
               * our setter function won't get called
               */
			  if(is_valid_jsss_prop(name))
              	  return JS_DefineProperty(mc, obj, name, JSVAL_VOID, 0, 0,
                      JSPROP_ENUMERATE | JSPROP_PERMANENT);
      }

      return JS_TRUE; 
}

/* Grouping syntax method for setting all the margins at once */
JSBool PR_CALLBACK
Tag_Margin(JSContext *mc, JSObject *obj, unsigned argc, jsval *argv, jsval *rval)
{
	StyleTag *tag = JS_GetPrivate(mc, obj);
	
	/*
	 * We expect between 1 and 4 arguments. We'll fail if there isn't at least
	 * one and silently ignore anything more than 4
	 */
	if (argc == 0) {
		JS_ReportError(mc, "Function margin() requires at least one argument.");
		return JS_FALSE;
	}

	XP_ASSERT(tag);

	/*
	 * The arguments apply to top, right, bottom, and left in that order. If there is
	 * only one argument it applies to all sides, if there are two or three, the missing
	 * values are taken from the opposite side
	 */
	if (tag) {
        jsval *vp;
        		
        LO_LockLayout();
		jss_TagSetProperty(mc, tag, "marginTop", argv);
		jss_TagSetProperty(mc, tag, "marginRight", argc >= 2 ? &argv[1] : argv);
		jss_TagSetProperty(mc, tag, "marginBottom", argc >= 3 ? &argv[2] : argv);

        if (argc == 4)
            vp = &argv[3];
        else if (argc >= 2)
            vp = &argv[1];
        else
            vp = argv;
		jss_TagSetProperty(mc, tag, "marginLeft", vp);
		LO_UnlockLayout();
	}

	return JS_TRUE;
}

/* Grouping syntax method for setting all the paddings at once */
JSBool PR_CALLBACK
Tag_Padding(JSContext *mc, JSObject *obj, unsigned argc, jsval *argv, jsval *rval)
{
	StyleTag *tag = JS_GetPrivate(mc, obj);
	
	/*
	 * We expect between 1 and 4 arguments. We'll fail if there isn't at least
	 * one and silently ignore anything more than 4
	 */
	if (argc == 0) {
		JS_ReportError(mc, "Function padding() requires at least one argument.");
		return JS_FALSE;
	}

	XP_ASSERT(tag);

	/*
	 * The arguments apply to top, right, bottom, and left in that order. If there is
	 * only one argument it applies to all sides, if there are two or three, the missing
	 * values are taken from the opposite side
	 */
	if (tag) {
        jsval *vp;
		
        LO_LockLayout();
		jss_TagSetProperty(mc, tag, "paddingTop", argv);
		jss_TagSetProperty(mc, tag, "paddingRight", argc >= 2 ? &argv[1] : argv);
		jss_TagSetProperty(mc, tag, "paddingBottom", argc >= 3 ? &argv[2] : argv);
        
        if (argc == 4)
            vp = &argv[3];
        else if (argc >= 2)
            vp = &argv[1];
        else
            vp = argv;
		jss_TagSetProperty(mc, tag, "paddingLeft", vp);
		LO_UnlockLayout();
	}

	return JS_TRUE;
}

/* Grouping syntax method for setting all the border widths at once */
JSBool PR_CALLBACK
Tag_BorderWidth(JSContext *mc, JSObject *obj, unsigned argc, jsval *argv, jsval *rval)
{
	StyleTag *tag = JS_GetPrivate(mc, obj);
	
	/*
	 * We expect between 1 and 4 arguments. We'll fail if there isn't at least
	 * one and silently ignore anything more than 4
	 */
	if (argc == 0) {
		JS_ReportError(mc, "Function borderWidth() requires at least one argument.");
		return JS_FALSE;
	}

	XP_ASSERT(tag);

	/*
	 * The arguments apply to top, right, bottom, and left in that order. If there is
	 * only one argument it applies to all sides, if there are two or three, the missing
	 * values are taken from the opposite side
	 */
	if (tag) {
        jsval *vp;

		LO_LockLayout();
		jss_TagSetProperty(mc, tag, "borderTopWidth", argv);
		jss_TagSetProperty(mc, tag, "borderRightWidth", argc >= 2 ? &argv[1] : argv);
		jss_TagSetProperty(mc, tag, "borderBottomWidth", argc >= 3 ? &argv[2] : argv);

        if (argc == 4)
            vp = &argv[3];
        else if (argc >= 2)
            vp = &argv[1];
        else
            vp = argv;
		jss_TagSetProperty(mc, tag, "borderLeftWidth", vp);
		LO_UnlockLayout();
	}

	return JS_TRUE;
}

/* Creates a new StyleTag structure */
StyleTag *
jss_NewTag(char *name)
{
	StyleTag *tag = (StyleTag *)XP_CALLOC(1, sizeof(StyleTag));

	if (!tag)
		return 0;

	/* Make a copy of the name */
	if (name) {
		tag->name = XP_STRDUP(name);
		if (!tag->name) {
			XP_FREE(tag);
			return 0;
		}
	}

	return tag;
}

/* Destroys a StyleTag structure */
void
jss_DestroyTag(StyleTag *tag)
{
	if (tag->name)
		XP_FREE(tag->name);
	jss_DestroyProperties(tag->properties);
	jss_DestroyRules(tag->rules);
	XP_FREE(tag);
}

/* JS function for creating contextual selectors */
JSBool PR_CALLBACK
jss_Contextual(JSContext *mc, JSObject *obj, unsigned argc, jsval *argv, jsval *rval)
{
	/* We expect at least 1 argument */
	if (argc == 0) {
		JS_ReportError(mc, "Function contextual() requires at least one argument.");
		return JS_FALSE;
	}

	/* If there's just one argument, then this is really just a simple selector */
	if (argc == 1) {
		*rval = argv[0];  /* copy the argument to the result */
		
	} else {
		StyleTag	*tag;
		JSObject	*tag_obj;
		StyleRule	*rule;
		unsigned	 i;

		/*
		 * Each StyleTag has a "rules" member where we store all contextual selectors that
		 * have that instance as the last simple selector in the list, i.e. the contextual
		 * selector "contextual(tags.UL, tags.OL)" would be stored in the "tags.OL" object
		 *
		 * Validate each of the arguments, and make sure it's a JavaScript object of
		 * type JSSTag
		 */
		for (i = 0; i < argc; i++) {
			/* We expect each simple selector to be a JavaScript object */
			if (!JSVAL_IS_OBJECT(argv[i]) || !JS_InstanceOf(mc, (JSObject *)argv[i], &Tag_class, 0)) {
				/* XXX - don't report error! */
				JS_ReportError(mc, "Invalid argument number '%u' in call to contextual().", i);
				return JS_FALSE;
			}
		}

		tag = JS_GetPrivate(mc, (JSObject *)argv[argc - 1]);
		XP_ASSERT(tag);

		LO_LockLayout();
		
		/* Look and see if there's already a rule for this selector */
		rule = jss_LookupRule(mc, tag->rules, argc, argv);
		if (!rule) {
			/* Create a new rule */
			rule = jss_NewRule(mc, argc, argv);
			if (!rule) {
				LO_UnlockLayout();
				return JS_FALSE;
			}

			/* Add it to the list of existing rules */
			rule->next = tag->rules;
			tag->rules = rule;
		}
		
		LO_UnlockLayout();

		/* Create a new JavaScript object */
		tag_obj = JS_NewObject(mc, &Tag_class, 0, 0);
		if (!tag_obj) {
			JS_ReportOutOfMemory(mc);
			return JS_FALSE;
		}
		
		JS_SetPrivate(mc, tag_obj, rule);
		*rval = OBJECT_TO_JSVAL(tag_obj);
	}

	return JS_TRUE;
}

static void
jss_TagAddStyleProperties(StyleTag *tag, StyleStruct *style)
{
	StyleProperty *prop;

	for (prop = tag->properties; prop; prop = prop->next) {
		SS_Number *num;

		switch (prop->tag) {
			case JSVAL_BOOLEAN:
				STYLESTRUCT_SetString(style, prop->name, prop->u.bVal ? "true" : "false",
					(int32)tag->specificity);
				break;

			case JSVAL_STRING:
				STYLESTRUCT_SetString(style, prop->name, prop->u.strVal,
					(int32)tag->specificity);
				break;

			case JSVAL_INT:
				num = STYLESTRUCT_NewSSNumber(style, (double)prop->u.nVal, "");
				STYLESTRUCT_SetNumber(style, prop->name, num, (int32)tag->specificity);
				STYLESTRUCT_FreeSSNumber(style, num);
				break;

			case JSVAL_DOUBLE:
				num = STYLESTRUCT_NewSSNumber(style, prop->u.dVal, "");
				STYLESTRUCT_SetNumber(style, prop->name, num, (int32)tag->specificity);
				STYLESTRUCT_FreeSSNumber(style, num);
				break;

			default:
				XP_ASSERT(FALSE);
				break;
		}
	}
}

/*
 * This routine will find all the selectors that apply and add their
 * properties to the style struct
 */
static void
jss_AddMatchingSelectors(JSSContext *jc, StyleTag *tag, StyleAndTagStack *styleStack)
{
	/*
	 * Iterate over each of the rules and for each one that applies add
	 * its properties
	 */
	if (tag->rules) {
		jss_EnumApplicableRules(jc, tag->rules, styleStack, (RULECALLBACK)jss_TagAddStyleProperties,
			STYLESTACK_GetStyleByIndex(styleStack, 0));
	}
}

/*
 * This routine is called to retrieve the list of style properties for the current
 * tag (tag at the top of the tag stack)
 */
XP_Bool
jss_GetStyleForTopTag(StyleAndTagStack *styleStack)
{
	TagStruct	*tag;
	StyleStruct	*style;
	JSSContext	 jc;

	tag = STYLESTACK_GetTagByIndex(styleStack, 0);
	style = STYLESTACK_GetStyleByIndex(styleStack, 0);
	XP_ASSERT(tag && style);

	/* Get the top-level hash tables */
	XP_MEMSET(&jc, 0, sizeof(JSSContext));
	sml_GetJSSContext(styleStack, &jc);

	/*
	 * Find all the rules that apply and add their declarations. We pass
	 * the specificity along and the style stack decides whether to use the
	 * declaration. This way we avoid having to sort the declarations
	 *
	 * Start with document.ids
	 */
	if (tag->id && jc.ids && jc.ids->table) {
		StyleTag *jsstag = (StyleTag *)PR_HashTableLookup(jc.ids->table, tag->id);

		if (jsstag) {
			jss_TagAddStyleProperties(jsstag, style);
			jss_AddMatchingSelectors(&jc, jsstag, styleStack);
		}
	}

	/* Now do document.classes */
	if (tag->class_name && jc.classes && jc.classes->table) {
		StyleObject *jsscls = (StyleObject *)PR_HashTableLookup(jc.classes->table, tag->class_name);
		
		if (jsscls && jsscls->table) {
			StyleTag *jsstag;

			/* First we check all elements of the class, e.g. classes.punk.all */
			jsstag = (StyleTag *)PR_HashTableLookup(jsscls->table, "all");
			if (jsstag) {
				jss_TagAddStyleProperties(jsstag, style);
				jss_AddMatchingSelectors(&jc, jsstag, styleStack);
			}

			/* Now check for the specified tag */
			jsstag = (StyleTag *)PR_HashTableLookup(jsscls->table, tag->name);
			if (jsstag) {
				jss_TagAddStyleProperties(jsstag, style);
				jss_AddMatchingSelectors(&jc, jsstag, styleStack);
			}
		}
	}

	/* Last we do document.tags */
	if (tag->name && jc.tags && jc.tags->table) {
		StyleTag *jsstag = (StyleTag *)PR_HashTableLookup(jc.tags->table, tag->name);

		if (jsstag) {
			jss_TagAddStyleProperties(jsstag, style);
			jss_AddMatchingSelectors(&jc, jsstag, styleStack);
		}
	}

	return JS_TRUE;
}

/* Grouping syntax methods */
static JSFunctionSpec tag_groupingMethods[] = {
	{"margins", Tag_Margin, 1},
	{"paddings", Tag_Padding, 1},
	{"borderWidths", Tag_BorderWidth, 1},
	{0}
};

/*
 * This routine is called to resolve names for the document object
 */
JSBool
JSS_ResolveDocName(JSContext *mc, MWContext *context, JSObject *obj, jsval id)
{
	if (JSVAL_IS_STRING(id)) {
		char *name = JS_GetStringBytes(JSVAL_TO_STRING(id));

		if (XP_STRCMP(name, "tags") == 0 ||
			XP_STRCMP(name, "classes") == 0 ||
			XP_STRCMP(name, "ids") == 0 ||
			XP_STRCMP(name, "contextual") == 0) {
			JSObject *winobj = JS_GetParent(mc, obj);

			JSObject    *tags_obj, *classes_obj, *ids_obj;
			StyleObject *tags, *classes, *ids;
	
			/* Define the JS "JSSTags" class which is a container for "JSSTag" objects */
			if (!JS_InitClass(mc, winobj, NULL, &Tags_class, TagsConstructor, 2, 0, 0, 0, 0))
				goto out_of_memory;
		
			/* Define an instances "tags" and bind it to the document */
			tags_obj = JS_DefineObject(mc, obj, "tags", &Tags_class, 0,
				JSPROP_READONLY | JSPROP_PERMANENT);
			if (!tags_obj)
				goto out_of_memory;

			/* Define an instances "ids" and bind it to the document */
			ids_obj = JS_DefineObject(mc, obj, "ids", &Tags_class, 0,
				JSPROP_READONLY | JSPROP_PERMANENT);
			if (!ids_obj)
				goto out_of_memory;
		
			/* Define the JS "JSSClasses" class which is a container for "JSSTags" objects */
			if (!JS_InitClass(mc, winobj, NULL, &Classes_class, ClassesConstructor, 2, 0, 0, 0, 0))
				goto out_of_memory;
		
			/* Define an instances "classes" and bind it to the document */
			classes_obj = JS_DefineObject(mc, obj, "classes", &Classes_class, 0,
				JSPROP_READONLY | JSPROP_PERMANENT);
			if (!classes_obj)
				goto out_of_memory;

			/* Define our C data structures */
			tags = (StyleObject *)XP_CALLOC(1, sizeof(StyleObject));
			if (!tags)
				goto out_of_memory;
			tags->type = JSSTags;

			classes = (StyleObject *)XP_CALLOC(1, sizeof(StyleObject));
			if (!classes)
				goto out_of_memory;
			classes->type = JSSClasses;

			ids = (StyleObject *)XP_CALLOC(1, sizeof(StyleObject));
			if (!ids)
				goto out_of_memory;
			ids->type = JSSIds;

			/* Add them to the style stack so we can get at them later */
			LO_SetStyleObjectRefs(context, tags, classes, ids);

			/* We need to be able to get to the C data structures from the JavaScript objects */
			JS_SetPrivate(mc, tags_obj, tags);
			JS_SetPrivate(mc, classes_obj, classes);
			JS_SetPrivate(mc, ids_obj, ids);
		
			/* Define the JS "JSSTag" class and some grouping syntax methods */
			if (!JS_InitClass(mc, winobj, NULL, &Tag_class, TagConstructor, 2, 0, tag_groupingMethods, 0, 0))
				goto out_of_memory;

			/* Define the function for creating contextual selectors */
			JS_DefineFunction(mc, obj, "contextual", jss_Contextual, 1, 0);
		}
	}

	return JS_TRUE;

  out_of_memory:
	JS_ReportOutOfMemory(mc);
	return JS_FALSE;
}

/**** Definition of JavaScript classes ****/

/*
 * This is the JS "JSSTags" class. This class is a container for "JSSTag"
 * objects (see below). There are two main instance of this class: "document.tags"
 * and "document.ids"
 */
JSClass Tags_class = {
    "JSSTags", JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
    JS_EnumerateStub, Tags_ResolveName, JS_ConvertStub, jss_FinalizeStyleObject
};

/*
 * This is the JS "JSSClasses" class. This class is a container for "JSSTags"
 * objects which in turn contain the actual tags. There is one instance of this
 * class: "document.classes"
 */
JSClass Classes_class = {
    "JSSClasses", JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
    JS_EnumerateStub, Classes_ResolveName, JS_ConvertStub, jss_FinalizeStyleObject
};

/*
 * This is the JS "JSSTag" class. There can be as many instances of this class
 * as there are HTML tags. JSS declarations are represented as properties
 */
JSClass Tag_class = {
    "JSSTag", JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub, Tag_GetProperty, Tag_SetProperty,
    JS_EnumerateStub, Tag_ResolveName, JS_ConvertStub, JS_FinalizeStub
};

