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


#include "xp.h"
#include "net.h"
#include "pa_parse.h"
#include "layout.h"
#include "np.h"
#ifdef JAVA
#include "java.h"
#endif

#ifdef	ANTHRAX /* 9.23.97	amusil */
#include "prefapi.h"
#endif	/* ANTHRAX */

/* Internal prototypes */
void lo_FetchObjectData(MWContext*, lo_DocState*, lo_ObjectStack*);
void lo_ObjectURLExit(URL_Struct*, int, MWContext*);
void lo_ClearObjectBlock(MWContext*, LO_ObjectStruct*);
lo_ObjectStack* lo_PushObject(MWContext*, lo_DocState*, PA_Tag*);
void lo_PopObject(lo_DocState*);
Bool lo_CheckObjectBlockage(MWContext*, lo_DocState*, lo_ObjectStack*);

#ifdef	ANTHRAX		/* 9.23.97	amusil */
static void lo_SetClassID(PA_Tag* tag, char* appletName);
static char* lo_GetNextName(char** index);
static char* lo_GetNextValue(char** index);
static void lo_SetJavaArgs(char* tag, LO_JavaAppStruct* current_java);
static void lo_itoa(uint32 n, char* s);
static void lo_ReverseString(char* s);
#endif	/* ANTHRAX */

void
lo_FormatObject(MWContext* context, lo_DocState* state, PA_Tag* tag)
{
	lo_TopState* top_state = state->top_state;
	lo_ObjectStack* top;
	LO_ObjectStruct* object;
	PA_Block buff;
	int16 type = LO_NONE;
	char* str;
	char* pluginName;

#ifdef	ANTHRAX
	XP_Bool javaMimetypeHandler = FALSE;
	char* appletName;
	NET_cinfo* fileInfo;
#endif /* ANTHRAX */
	
	/*
	 * Make a new default object.  Passing LO_OBJECT will create an
	 * LO_Element, which being a union of all other layout element types
	 * is guaranteed to be big enough to transmogrify into one of these
	 * specific types later if necessary.
	 */
	object = (LO_ObjectStruct*) lo_NewElement(context, state, LO_OBJECT, NULL, 0);
	if (object == NULL)
	{
		state->top_state->out_of_memory = TRUE;
		return;
	}
	
	top = top_state->object_stack;
	top->object = object;

	/*
	 * Set up default fields for this object that are common
	 * to all possible object types.
	 */
	object->lo_element.lo_plugin.type = LO_NONE;
	object->lo_element.lo_plugin.ele_id = NEXT_ELEMENT;
	object->lo_element.lo_plugin.x = state->x;
	object->lo_element.lo_plugin.x_offset = 0;
	object->lo_element.lo_plugin.y = state->y;
	object->lo_element.lo_plugin.y_offset = 0;
	object->lo_element.lo_plugin.width = 0;
	object->lo_element.lo_plugin.height = 0;
	object->lo_element.lo_plugin.next = NULL;
	object->lo_element.lo_plugin.prev = NULL;

	
	/*
	 * Now attempt to figure out what type of object we have.
	 * If the type can be determined here, great; otherwise
	 * we have to block until the type can be determined by
	 * reading in additional data.
	 *
	 * Initially the type of the object is LO_NONE.  When
	 * we figure out enough to know the type, we set it to
	 * LO_EMBED, LO_JAVA, or LO_IMAGE.  If the type had
	 * already been changed to a different incompatible type,
	 * then the tag is malformed and we should ignore it, so
	 * set the type to LO_UNKNOWN.
	 */
	
#if 0 
	/*
	 * Check the "codetype" attribute, which optionally determines
	 * the MIME type of the object code itself (as opposed to its
	 * data).  The only code type we know about right now is
	 * application/java-vm for Java applets.
	 */
	buff = lo_FetchParamValue(context, tag, PARAM_CODETYPE);
	if (buff != NULL)
	{
		PA_LOCK(str, char *, buff);
		if (pa_TagEqual(APPLICATION_JAVAVM, str))
		{
			/* It's a Java applet */
			if (type == LO_NONE)
				type = LO_JAVA;
			else if (type != LO_JAVA)
				type = LO_UNKNOWN;
		}
		else if (pa_TagEqual(APPLICATION_OLEOBJECT, str) ||
				 pa_TagEqual(APPLICATION_OLEOBJECT2, str))
		{
			/* It's an OLE object */
			if (type == LO_NONE)
				type = LO_EMBED;
			else if (type != LO_EMBED)
				type = LO_UNKNOWN;
		}
		PA_UNLOCK(buff);
		XP_FREE(buff);
	}
#endif

	/*
	 * Check the "classid" attribute, which optionally determines
	 * the specific implementation of the object.  The classid
	 * could be a normal URL, in which case we have to retrieve
	 * that URL and match it against a known code type (see above).
	 * There are also two "magic" URL types supported for classid:
	 * "clsid:", which indicates a COM 376-hex-digit class ID,
	 * and "java:", which indicates a specific java class to run.
	 * Note that the "java:" URL is different from the APPLET
	 * "code" attribute: the "java:" URL specifies a particular
	 * method (e.g. "java:program.run"), while the APPLET CODE
	 * attribute specifies an applet subclass (e.g. "MyApplet.class").
     *
     * Further notes about "java:"
     * We are adding two related "magic" protocol selectors to
     * augment "java:". These are "javaprogram:" and "javabean:".
     * They are used with embedded applications and application
     * objects. "javaprogram:" identifies an object as being a
     * subclass of netscape.application.Application, and is used
     * to start an instance of such application. "javabean:" is
     * used to add an embedded object to an application.
	 */
	buff = lo_FetchParamValue(context, tag, PARAM_CLASSID);
	if (buff != NULL)
	{
		PA_LOCK(str, char *, buff);
		if (XP_STRNCASECMP(str, "clsid:", 6) == 0)
		{
			/*
			 * It's a COM class ID, so make sure we have an
			 * appropriate plug-in to handle ActiveX controls.
			 */
			if ((pluginName = NPL_FindPluginEnabledForType(APPLICATION_OLEOBJECT)) != NULL)
			{
				XP_FREE(pluginName);
				if (type == LO_NONE)
					type = LO_EMBED;
				else if (type != LO_EMBED)
					type = LO_UNKNOWN;
			}
		}
		else if ( (XP_STRNCASECMP(str, "java:", 5) == 0) ||
                  (XP_STRNCASECMP(str, "javaprogram:", 12) == 0) ||
                  (XP_STRNCASECMP(str, "javabean:", 9) == 0) )
		{
			/* It's a Java class */
#ifdef OJI
			if (type == LO_NONE)
				type = LO_EMBED;
			else if (type != LO_EMBED) /* XXX */
#else
			if (type == LO_NONE)
				type = LO_JAVA;
			else if (type != LO_JAVA)
#endif
				type = LO_UNKNOWN;
		}
		else
		{
			/*
			 * Must be a URL to the code; we'll need to fetch it to
			 * determine the type.  bing: How should we do this?
			 */
		}
		PA_UNLOCK(buff);
		XP_FREE(buff);
	}

	/*
	 * Check the "type" attribute, which optionally determines
	 * the type of the data for the object.  The data type
	 * can be used to infer the object implementation type if
	 * the implementation hasn't been specified via "classid"
	 * or "codetype" (see above).  The two kinds of objects
	 * we currently support with typed data are plug-ins and
	 * images; for plug-ins we can ask libplug if the type is
	 * currently handled by a plug-in; for images we just check
	 * against a hard-coded list of image types we natively
	 * support (yuck).
	 */
	buff = lo_FetchParamValue(context, tag, PARAM_TYPE);
	if (buff != NULL)
	{
		PA_LOCK(str, char *, buff);
		if ((pluginName = NPL_FindPluginEnabledForType(str)) != NULL)
		{
			XP_FREE(pluginName);
			/* It's a plug-in */
			if (type == LO_NONE)
				type = LO_EMBED;
			else if (type != LO_EMBED)
				type = LO_UNKNOWN;
		}

		/*  
			Adding a check for applets that handle mimetypes.
			The pref is stored based on the particular mimetype.
			We do a lookup and if there is an association, the name
			of the applet is placed into "appletName".  
			
			NOTE: PREF_CopyCharPref() allocates memory for appletName
					and we must free it.
					
			9.23.97		amusil
		*/
#ifdef	ANTHRAX
		if((appletName = NPL_FindAppletEnabledForMimetype(str)) != NULL)
			{
			/* Set the type */
			type = LO_JAVA;
			
			/* set the CLASSID to whatever was put into "appletName" */
			lo_SetClassID(tag, appletName);
			
			/* set this so that we know later to translate the DATA/SRC param to a Java arg */
			javaMimetypeHandler = TRUE;
			XP_FREE(appletName);
			}
#endif	/* ANTHRAX */	
		
#if 0
		else if (XP_STRNCASECMP(str, "image/", 6) == 0)
		{
			if (XP_STRCASECMP(str, IMAGE_GIF) ||
				XP_STRCASECMP(str, IMAGE_JPG) ||
				XP_STRCASECMP(str, IMAGE_PJPG) ||
				XP_STRCASECMP(str, IMAGE_XBM) ||
				XP_STRCASECMP(str, IMAGE_XBM2) ||
				XP_STRCASECMP(str, IMAGE_XBM3))
			{
				/* It's an image */
				if (type == LO_NONE)
					type = LO_IMAGE;
				else if (type != LO_IMAGE)
					type = LO_UNKNOWN;
			}
		}
#endif /* if 0 */

		if (XP_STRNCASECMP(str, "builtin", 7) == 0)
		{
			if (type == LO_NONE)
				type = LO_BUILTIN;
			else if (type != LO_BUILTIN)
				type = LO_UNKNOWN;
		}

		PA_UNLOCK(buff);
		XP_FREE(buff);
	}
#ifdef	ANTHRAX
	else /* we didn't find a TYPE param, so check the filename to get the type - amusil */
		{
		buff = lo_FetchParamValue(context, tag, PARAM_SRC);
		/* if no src, check if there's a DATA param */
		if(buff == NULL)
			buff = lo_FetchParamValue(context, tag, PARAM_DATA);
			
		
		/* extract the mimetype info */
		PA_LOCK(str, char *, buff);
		fileInfo = NET_cinfo_find_type(str);
		
		str = fileInfo->type;
		if((appletName = NPL_FindAppletEnabledForMimetype(str)) != NULL)
			{
			/* Set the type */
			type = LO_JAVA;
			
			/* set the CLASSID to whatever was put into "appletName" */
			lo_SetClassID(tag, appletName);
			
			/* set this so that we know later to translate the DATA/SRC param to a Java arg */
			javaMimetypeHandler = TRUE;
			XP_FREE(appletName);			/* do we need to free this regardless? */	
			}
		if(buff)
			XP_FREE(buff);
		}
#endif	/* ANTRHAX */


	if (type == LO_EMBED)
	{
		object->lo_element.lo_plugin.type = LO_EMBED;
	}
	else if (type == LO_BUILTIN)
	{
		object->lo_element.type = LO_BUILTIN;
	}
#ifdef JAVA
	else if (type == LO_JAVA)
	{
		if (LJ_GetJavaEnabled() != FALSE)
		{
			/*
			 * Close out the current applet if necessary
			 * (people tend to forget "</APPLET>").
			 */
			if (state->current_java != NULL)
				lo_CloseJavaApp(context, state, state->current_java);
				
			object->lo_element.lo_plugin.type = LO_JAVA;
			lo_FormatJavaObject(context, state, tag, (LO_JavaAppStruct*) object);
			
			/* 
				If we determined previously that this is an applet to mimetype
				association, we must set up the SRC or DATA as an arg for the 
				applet.
				
				9.8.97		amusil
			*/
#ifdef	ANTHRAX
			if(javaMimetypeHandler)
				lo_SetJavaArgs((char*)tag->data, state->current_java);
#endif	/* ANTHRAX */
		}
	}
#endif /* JAVA */
#if 0
	else if (type == LO_IMAGE)
	{
		object->lo_element.lo_plugin.type = LO_IMAGE;
		lo_FormatImageObject(context, state, tag, (LO_ImageStruct*) object);
	}
#endif /* if 0 */
	else
	{
		/*
		 * Check for a "data" attribute; if it exists, we can get
		 * the URL later to see what the type of the object should be.
		 */
		buff = lo_FetchParamValue(context, tag, PARAM_DATA);
		if (buff != NULL)
		{
			PA_LOCK(str, char *, buff);
			if (XP_STRNCASECMP(str, "data:", 5) == 0)
			{
				/* bing: deal with magic data URLs here */
				PA_UNLOCK(buff);
				XP_FREE(buff);
			}
			else
			{
				
				/*
				 * Block layout until we can read the PARAM tags
				 * and closing </OBJECT> tag, go get the data URL,
				 * and determine its type.  At that point (in
				 * either LO_NewObjectStream, or lo_ObjectURLExit),
				 * we know the object type and can unblock.
				 */
				top->data_url = str;
				state->top_state->layout_blocking_element = (LO_Element*) object;
				PA_UNLOCK(buff);
				/* Don't free buff; we stored it in the object stack */
			}
		}
		else
		{
			/*
			 * Otherwise we just don't know what to do with this!
			 */
			object->lo_element.lo_plugin.type = LO_UNKNOWN;
		}
	}
}




/* 
 * Takes a "PARAM" tag and pointers to the object's attribute
 * count, attribute name array, and attribute value array.
 * Parses the name and value of the PARAM and adjusts the
 * attribute count, names, and values accordingly.
 */
void
lo_ObjectParam(MWContext* context, lo_DocState* state, PA_Tag* tag,
			   uint32* count, char*** names, char*** values)
{
	PA_Block buff;
	char *str;
	char *param_name = NULL;
	char *param_value = NULL;
	char **name_list;
	char **value_list;
	
	/*
	 * Get the name of this parameter.
	 */
	buff = lo_FetchParamValue(context, tag, PARAM_NAME);
	if (buff != NULL)
	{
		PA_LOCK(str, char *, buff);
		if (str != NULL)
		{
			int32 len;
			char *new_str;

			len = lo_StripTextWhitespace(str, XP_STRLEN(str));
			new_str = (char *)XP_ALLOC(len + 1);
			if (new_str != NULL)
			{
				XP_STRCPY(new_str, str);
			}
			param_name = new_str;
		}
		PA_UNLOCK(buff);
		PA_FREE(buff);
	}
	else
	{
		/* If we don't have a parameter name, it's useless */
		return;
	}

	/*
	 * Get the value of this parameter.
	 */
	buff = lo_FetchParamValue(context, tag, PARAM_VALUE);
	if (buff != NULL)
	{
		PA_LOCK(str, char *, buff);
		if (str != NULL)
		{
			int32 len;
			char *new_str;

			len = lo_StripTextWhitespace(str, XP_STRLEN(str));
			new_str = (char *)XP_ALLOC(len + 1);
			if (new_str != NULL)
			{
				XP_STRCPY(new_str, str);
			}
			param_value = new_str;
		}
		PA_UNLOCK(buff);
		PA_FREE(buff);
	}

	/*
	 * Add the parameter to the name/value list.
	 */
	if (*names == NULL)
		name_list = (char **)XP_ALLOC(sizeof(char *));
	else
		name_list = (char **)XP_REALLOC(*names,
						((*count + 1) * sizeof(char *)));
	
	/*
	 * If we run out of memory here, free up what
	 * we can and return.
	 */
	if (name_list == NULL)
	{
		state->top_state->out_of_memory = TRUE;
		XP_FREE(param_name);
		if (param_value != NULL)
			XP_FREE(param_value);
		return;
	}

	if (*values == NULL)
		value_list = (char **)XP_ALLOC(sizeof(char *));
	else
		value_list = (char **)XP_REALLOC(*values,
						((*count + 1) * sizeof(char *)));

	/*
	 * If we run out of memory here, free up what
	 * we can and return.
	 */
	if (value_list == NULL)
	{
		state->top_state->out_of_memory = TRUE;
		XP_FREE(param_name);
		if (param_value != NULL)
			XP_FREE(param_value);
		return;
	}
	
	*names = name_list;
	*values = value_list;
	(*names)[*count] = param_name;
	(*values)[*count] = param_value;
	(*count)++;

}


/*
 * Appends the name/value list in count2, names2, and values2 on
 * to the name/value list in count1, names1, and values1.
 */
void
lo_AppendParamList(uint32* count1, char*** names1, char*** values1,
				   uint32 count2, char** names2, char** values2)
{		
	char** names = NULL;
	char** values = NULL;
	
	if (*names1 != NULL)
	{
		if (names2 != NULL)
		{
			names = (char**) XP_REALLOC(*names1, ((*count1 + count2) * sizeof(char*)));
			if (names != NULL)
				XP_MEMCPY(&(names[*count1]), names2, count2 * sizeof(char*));
		}
		else
			names = *names1;
	}
	else
		names = names2;
		
	if (*values1 != NULL)
	{
		if (values2 != NULL)
		{
			values = (char**) XP_REALLOC(*values1, ((*count1 + count2) * sizeof(char*)));
			if (values != NULL)
				XP_MEMCPY(&(values[*count1]), values2, count2 * sizeof(char*));
		}
		else
			values = *values1;
	}
	else
		values = values2;
		
	if (names != NULL && values != NULL)
	{
		*count1 = *count1 + count2;
		*names1 = names;
		*values1 = values;
	}
}



lo_ObjectStack*
lo_PushObject(MWContext* context, lo_DocState* state, PA_Tag* tag)
{
	/*
	 * If possible, reuse an object stack entry created in
	 * a previous pass over this tag (i.e. while blocked, by
	 * lo_BlockObjectTag).  If there's no previously-created
	 * object, make a new one.
	 */
	lo_TopState* top_state = state->top_state;
	lo_ObjectStack* new_top = top_state->object_cache;

	if (new_top != NULL)
	{
		/* Find and remove the matching item, if any */
		if (new_top->real_tag == tag)
			top_state->object_cache = new_top->next;
		else
		{
			while (new_top->next != NULL)
			{
				if (new_top->next->real_tag == tag)
				{
					lo_ObjectStack* temp = new_top->next;
					new_top->next = new_top->next->next;
					new_top = temp;
					break;
				}
				new_top = new_top->next;
			}
		}
	}
	
	if (new_top == NULL || new_top->real_tag != tag)
	{
		new_top = XP_NEW_ZAP(lo_ObjectStack);
		if (new_top == NULL)
		{
			state->top_state->out_of_memory = TRUE;
			return NULL;
		}
	}
	
	new_top->next = top_state->object_stack;
	top_state->object_stack = new_top;

	new_top->context = context;
	new_top->state = state;
	
	/*
	 * Clone the tag since the tag passed in may be freed
	 * by the parser before we're ready for it.
	 */
	if (new_top->real_tag != tag)
	{
		new_top->real_tag = tag;

		if (new_top->clone_tag != NULL)
			PA_FreeTag(new_top->clone_tag);

		new_top->clone_tag = XP_NEW(PA_Tag);
		if (new_top->clone_tag != NULL)
		{
			XP_MEMCPY(new_top->clone_tag, tag, sizeof(PA_Tag));
			new_top->clone_tag->data = PA_ALLOC((tag->data_len + 1) * sizeof (char)); 
			if (new_top->clone_tag->data != NULL)
			{
				char* source;
				char* dest;
				PA_LOCK(source, char*, tag->data);
				PA_LOCK(dest, char*, new_top->clone_tag->data);
				XP_MEMCPY(dest, source, tag->data_len + 1);
				PA_UNLOCK(dest);
				PA_UNLOCK(source);
			}
		}
	}
	
	return new_top;
}


void
lo_PopObject(lo_DocState* state)
{
	lo_TopState* top_state = state->top_state;
	lo_ObjectStack* top = top_state->object_stack;
	if (top == NULL)
		return;

	/* Unlink from stack */
	top_state->object_stack = top->next;

	/* Add to cache */
	top->next = top_state->object_cache;
	top_state->object_cache = top;
}


void
lo_DeleteObjectStack(lo_ObjectStack* top)
{
	/* Delete parameters */
#ifdef OJI
	if (top->parameters.n > 0)
	{
		uint32 index;

		if (top->parameters.values != NULL)
		{
			for (index = 0; index < top->parameters.n; index++)
			{
				if (top->parameters.names[index] != NULL)
					XP_FREE(top->parameters.names[index]);
			}
			XP_FREE(top->parameters.names);
		}
		
		if (top->parameters.values != NULL)
		{
			for (index = 0; index < top->parameters.n; index++)
			{
				if (top->parameters.values[index] != NULL)
					XP_FREE(top->parameters.values[index]);
			}
			XP_FREE(top->parameters.values);
		}
	}
#else
 	if (top->param_count > 0)
  	{
  		uint32 index;
  		
 		if (top->param_values != NULL)
  		{
 			for (index = 0; index < top->param_count; index++)
  			{
 				if (top->param_names[index] != NULL)
 					XP_FREE(top->param_names[index]);
  			}
 			XP_FREE(top->param_names);
  		}
  		
 		if (top->param_values != NULL)
  		{
 			for (index = 0; index < top->param_count; index++)
  			{
 				if (top->param_values[index] != NULL)
 					XP_FREE(top->param_values[index]);
 			}
 			XP_FREE(top->param_values);
  		}
  	}
#endif /* OJI */
	
	/* Delete tag copy */
	if (top->clone_tag != NULL)
		PA_FreeTag(top->clone_tag);
		
	/* Delete object */
	XP_FREE(top);
}



Bool
lo_CheckObjectBlockage(MWContext* context, lo_DocState* state, lo_ObjectStack* top)
{
	LO_ObjectStruct* object = top->object;
	
	if (object != NULL &&
		state->top_state->layout_blocking_element == (LO_Element*) object)
	{
		/*
		 * We're blocked on this object, and we need to start
		 * a data stream to figure out the object type. Now
		 * that we've read the PARAMs, it's OK to start the
		 * stream.
		 */
		if (top->data_url != NULL)
		{
			XP_ASSERT(object->lo_element.lo_plugin.type == LO_NONE);
			lo_FetchObjectData(context, state, top);
		}
		else
		{
			XP_ASSERT(FALSE);
			object->lo_element.lo_plugin.type = LO_UNKNOWN;
		}
		
		return TRUE;	/* Yes, we're the cause of the blockage */
	}
	
	return FALSE;		/* No, some enclosing object is the block point */
}



void
lo_ProcessObjectTag(MWContext* context, lo_DocState* state, PA_Tag* tag, XP_Bool blocked)
{
	lo_TopState* top_state = state->top_state;
	lo_ObjectStack* top = top_state->object_stack;
	LO_ObjectStruct* object = top ? top->object : NULL;

	if (tag->is_end == FALSE)
	{
		lo_ObjectStack* new_top;

		/*
		 * If we have started loading an OBJECT we are
		 * out if the HEAD section of the HTML
		 * and into the BODY
		 */
		if (blocked == FALSE)
		{
			state->top_state->in_head = FALSE;
			state->top_state->in_body = TRUE;
		}

		/*
		 * Push a new entry onto the stack.  If blocked,
		 * also stash it in the tag's lo_data so we can
		 * reuse it later when not blocked.
		 */
		new_top = lo_PushObject(context, state, tag);
		if (blocked)
		{
			XP_ASSERT(tag->lo_data == NULL);
			tag->lo_data = (void*) new_top;
		}
		
		/*
		 * If we're not blocked, and not in the middle of a handled
		 * object, go process this object tag. Otherwise ignore the
		 * the tag (we don't just filter it out in lo_FilterTag
		 * because we need to keep track of the object nesting to
		 * match up with the right </OBJECT>).
		 */
		if (blocked == FALSE)
		{
			if (object == NULL || object->lo_element.lo_plugin.type == LO_UNKNOWN)
			{
				lo_FormatObject(context, state, tag);

				/*
				 * If we've already read the PARAMs, we can try deal
				 * with the blockage now (in fact, we must).  If we
				 * haven't read the params yet, we have to wait until
				 * our matching </OBJECT> tag (this will be handled
				 * by the code below).
				 */
				if (new_top->read_params)
					lo_CheckObjectBlockage(context, state, new_top);
			}
		}
	}
	else
	{
		if (top != NULL)
		{
			if (blocked)
			{
				/*
			     * Set a flag indicating that we've already read
			     * this object's params while blocked so we won't
			     * have to do it again later when unblocked.
			     */
				top->read_params = TRUE;
				
				if (lo_CheckObjectBlockage(context, state, top))
				{
					/*
					 * If we're blocked on this object, don't pop the
					 * stack. When the blockage is cleared we want to
					 * still be at the top of the stack, because layout
					 * will proceed immediately after the block point, 
					 * which was our opening <OBJECT> tag.
					 */
				}
				else
				{
					/*
					 * If blocked on some other (enclosing) object,
					 * pop the stack.
					 */
					lo_PopObject(state);
				}
			}
			else
			{
				/*
				 * If not blocked, just close out the object
				 * as appropriate.
				 */
				 if (object != NULL && top->formatted_object == FALSE)
				 {
					if (object->lo_element.lo_plugin.type == LO_EMBED)
					{
						lo_FormatEmbedObject(context,
                                                                     state,
                                                                     top->clone_tag,
                                                                     (LO_EmbedStruct*) object,
                                                                     FALSE, /* Stream not started */
#ifdef OJI
                                                                     top->parameters.n,
                                                                     top->parameters.names,
                                                                     top->parameters.values);
						top->formatted_object = TRUE;
                                                LO_NVList_Init( &top->parameters );
#else
                                                                     top->param_count,
                                                                     top->param_names,
                                                                     top->param_values);
						top->formatted_object = TRUE;
						top->param_count = 0;
						top->param_names = NULL;
						top->param_values = NULL;
#endif /* OJI */
					}
					else if (object->lo_element.type == LO_BUILTIN)
					{
						lo_FormatBuiltinObject(context,
                                                                       state,
                                                                       top->clone_tag,
                                                                       (LO_BuiltinStruct*) object,
                                                                       FALSE, /* Stream not started */
#ifdef OJI
                                                                       top->parameters.n,
                                                                       top->parameters.names,
                                                                       top->parameters.values);
						top->formatted_object = TRUE;
                                                LO_NVList_Init( &top->parameters );
#else
                                                                       top->param_count,
                                                                       top->param_names,
                                                                       top->param_values);
						top->formatted_object = TRUE;
						top->param_count = 0;
						top->param_names = NULL;
						top->param_values = NULL;
#endif /* OJI */
					}

#ifdef JAVA
					else if (object->lo_element.lo_plugin.type == LO_JAVA)
					{
						lo_CloseJavaApp(context, state, (LO_JavaAppStruct*) object);
					}
#endif
				 }

				/* Pop stack entry */
				lo_PopObject(state);
			}
		}
	}
}



void
lo_ProcessParamTag(MWContext* context, lo_DocState* state, PA_Tag* tag, XP_Bool blocked)
{
	if (tag->is_end == FALSE)
	{
		lo_TopState* top_state = state->top_state;
		lo_ObjectStack* top = top_state->object_stack;

		/*
		 * PARAMs can be used inside both APPLETs and OBJECTs.
		 * Since applets can be inside objects but objects can't
		 * be inside applets, give the current applet priority
		 * for ownership of the parameter.
		 */
#ifdef JAVA

	    if (state->current_java != NULL && blocked == FALSE)
	    {
	    	LO_JavaAppStruct* java_app = state->current_java;
			lo_ObjectParam(context, state, tag,
#ifdef OJI
						   (uint32*) &(java_app->parameters.n),
						   &(java_app->parameters.names),
						   &(java_app->parameters.values));
#else 
						   (uint32*) &(java_app->param_cnt),
						   &(java_app->param_names),
						   &(java_app->param_values));
#endif /* OJI */
		}
		else 

#endif /* JAVA */

		if (top != NULL && top->read_params == FALSE)
		{
#ifdef OJI
			lo_ObjectParam(context, state, tag,
						   &(top->parameters.n),
						   &(top->parameters.names),
						   &(top->parameters.values));
#else 
			lo_ObjectParam(context, state, tag,
						   &(top->param_count),
						   &(top->param_names),
						   &(top->param_values));
#endif /* OJI */
		}
	}
}



/*
 * This function gets the URL for the "data" attribute
 * for an object, so we can determine the MIME type of
 * the data and thus the type of object.  If libnet can
 * get the data, LO_NewObjectStream will be called (see
 * below).  If it can't, lo_ObjectURLExit will be called
 * (also see below).
 */
void
lo_FetchObjectData(MWContext* context, lo_DocState* state,
				   lo_ObjectStack* top)
{
	char* address = NULL;
	URL_Struct* urls = NULL;
	
	address = NET_MakeAbsoluteURL(
				state->top_state->base_url, top->data_url);

	if (address != NULL)
	{
        urls = NET_CreateURLStruct(address, NET_DONT_RELOAD);
        if (urls != NULL)
        {
        	urls->fe_data = (void*) top;
	        (void) NET_GetURL(urls, FO_CACHE_AND_OBJECT,
	        				  context, lo_ObjectURLExit);
        }
	}
	
	if (address != NULL)
		XP_FREE(address);
		
	if (urls == NULL)
	{
		state->top_state->out_of_memory = TRUE;
		return;
	}
}



/*
 * Create a new stream handler for dealing with a stream of
 * object data.  We don't really want to do anything with
 * the data, we just need to check the type to see if this
 * is some kind of object we can handle.  If it is, we can
 * format the right kind of object, clear the layout blockage,
 * and connect this stream up to its rightful owner.
 * NOTE: Plug-ins are the only object type supported here now.
 */
NET_StreamClass*
LO_NewObjectStream(FO_Present_Types format_out, void* type,
				   URL_Struct* urls, MWContext* context)
{
	NET_StreamClass* stream = NULL;
	lo_ObjectStack* top = (lo_ObjectStack*) urls->fe_data;
	char* pluginName;
	
	if (top != NULL && top->object != NULL)
	{	
		if ((pluginName = NPL_FindPluginEnabledForType(urls->content_type)) != NULL)
		{
			/* bing: Internal reference to libplug! */
			extern void NPL_EmbedURLExit(URL_Struct*, int, MWContext*);

			XP_FREE(pluginName);

			/* Now we know the object type */
			top->object->lo_element.lo_plugin.type = LO_EMBED;
			
#ifdef OJI
			lo_FormatEmbedObject(top->context,
								 top->state,
								 top->clone_tag,
								 (LO_EmbedStruct*) top->object,
								 TRUE,
								 top->parameters.n,
								 top->parameters.names,
								 top->parameters.values);
			top->parameters.n = 0;
			top->parameters.names = NULL;
			top->parameters.values = NULL;
#else
			lo_FormatEmbedObject(top->context,
								 top->state,
								 top->clone_tag,
								 (LO_EmbedStruct*) top->object,
								 TRUE,
								 top->param_count,
								 top->param_names,
								 top->param_values);
			top->param_count = 0;
			top->param_names = NULL;
			top->param_values = NULL;
#endif
			top->formatted_object = TRUE;

			/* Set the FE data that libplug expects */
		    urls->fe_data = top->object->lo_element.lo_plugin.FE_Data;

		    /* Set the exit routine that libplug expects */
		    NET_SetNewContext(urls, context, NPL_EmbedURLExit);

		    /* Get the stream handler that libplug registered */
			stream = NET_StreamBuilder(FO_CACHE_AND_EMBED, urls, context);

			lo_ClearObjectBlock(top->context, top->object);
		}
	}
	
	return stream;
}



/*
 * This routine is called when a data stream we requested 
 * failed (either because libnet couldn't get the data, or
 * because we returned a NULL stream in LO_NewObjectStream
 * because the MIME type of the stream didn't translate into
 * a handlable object type).  In this case we need to get
 * the object out of the URL, set its type to "unknown", and
 * unblock layout.  This routine is also called when we
 * successfully handed off the stream to its new owner and
 * reset the exit routine (in LO_NewObjectStream; the status
 * will be MK_CHANGING_CONTEXT in the latter case).  In this
 * case, we don't need to do anything here.
 */
void
lo_ObjectURLExit(URL_Struct* urls, int status, MWContext* context)
{
    if (urls != NULL && status != MK_CHANGING_CONTEXT)
    {
    	if (urls->fe_data != NULL)
    	{
    		lo_ObjectStack* stack = (lo_ObjectStack*) urls->fe_data;
    		LO_ObjectStruct* object = stack->object;
    		if (object != NULL)
    		{
				object->lo_element.lo_plugin.type = LO_UNKNOWN;
				lo_ClearObjectBlock(context, object);
			}
		}

		XP_FREE(urls);
	}
}



void
lo_ClearObjectBlock(MWContext* context, LO_ObjectStruct* object)
{
	int32 doc_id = XP_DOCID(context);
	lo_TopState* top_state = lo_FetchTopState(doc_id);

	if (top_state != NULL &&
		top_state->doc_state != NULL &&
		top_state->layout_blocking_element == (LO_Element*) object)
	{
		lo_DocState* main_doc_state = top_state->doc_state;
		lo_DocState* state = lo_CurrentSubState(main_doc_state);
		lo_FlushBlockage(context, state, main_doc_state);
	}
}

/* 	
	lo_SetClassID
	-------------
	This inserts a CLASSID attribute into the tag->data string with its value set
	to "appletName".

	9.8.97		created		amusil
*/

#ifdef	ANTHRAX

static void lo_SetClassID(PA_Tag* tag, char* appletName)
{
	uint32 appletNameLen, oldTagLen, newTagLen;
	char* tagData;
	char foo;
	
	appletNameLen = XP_STRLEN(appletName);
	oldTagLen = XP_STRLEN((char*)tag->data);
	newTagLen = oldTagLen + 15 + appletNameLen;
	
	tag->data = XP_REALLOC(tag->data, newTagLen+1);

	/* remove the '>' character */
	tagData = (char*) (tag->data);
	tagData[oldTagLen-1] = 0;

	/* add " CLASSID = appletName" */
	XP_STRCAT((char*)tag->data, " CLASSID=java:");
	XP_STRCAT((char*)tag->data, appletName);
	XP_STRCAT((char*)tag->data, " >");
	tag->data_len = newTagLen;
}

/*
	lo_GetNextName()
	----------------
	This function parses a tag and returns a newly allocated string containing the first encountered
	NAME.  An example of what would get passed to this function is (basically a tag without the preceding
	<OBJECT) :
		
		"foo = VALUE	bar = VALUE2 >"
	
	This would return foo.  The tag pointer also gets incremented past the just-read NAME.  The example above
	would exit with *tag equal to:
	
		" = VALUE		bar = VALUE2 >"
	
	NULL is returned if a '>' character is encountered.
	
	10.7.97		amusil
*/

static char* lo_GetNextName(char** tag)
{
	uint32 nameLength = 0;
	char* newName;
	char* start;

	XP_ASSERT(tag);
	XP_ASSERT(*tag);
	XP_ASSERT(**tag);	

	while(**tag == ' ')
		++(*tag);
		
	if(**tag == '>')
		return NULL;
		
	start = *tag;
	while((**tag != ' ') && (**tag != '='))
		{
		++nameLength;
		++(*tag);
		}
		
	newName = XP_ALLOC((nameLength+1)*sizeof(char));
	
	newName = strncpy(newName, start, nameLength);
	XP_ASSERT(newName);
	
	newName[nameLength] = 0;
	
	return newName;
}

/*
	lo_GetNextValue()
	----------------
	This function parses a tag and returns a newly allocated string containing the first encountered
	VALUE.  An example of what would get passed to this function is (basically a tag without the preceding
	<OBJECT) :
		
		"= foo	NAME = bar >"
	
	This would return foo.  The tag pointer also gets incremented past the just-read VALUE.  The example above
	would exit with *tag equal to:
	
		"	NAME = bar >"


	In the case that the value is enclosed in quotes, the entire string within the quotes, including spaces,
	is returned.

	NOTE: This function expects that lo_GetNextName() was called before it.
	
	10.7.97		amusil
*/

static char* lo_GetNextValue(char** tag)
{
	uint32 valueLength = 0;
	char* newValue;
	char* start;

	XP_ASSERT(tag);
	XP_ASSERT(*tag);
	XP_ASSERT(**tag);	

	while(**tag == ' ' || **tag == '=')
		++(*tag);
		
	if(**tag == '>')
		return NULL;
		
	/* if the value is enclosed in quotes, we capture the entire string up to the closing quote */
	if(**tag == '\"')
		{
			++(*tag);
			start = *tag;
			while(**tag != '\"')
				{
				++valueLength;
				++(*tag);
				}
			++(*tag);  /* skip over the enclosing " */
		}
	else	/* otherwise, we just get up to the first space */
		{
			start = *tag;
			while((**tag != ' '))
				{
				++valueLength;
				++(*tag);
				}
		}	
	
	newValue = XP_ALLOC((valueLength+1)*sizeof(char));
	
	newValue = strncpy(newValue, start, valueLength);
	XP_ASSERT(newValue);
	
	newValue[valueLength] = 0;
	
	return newValue;
}


/*	
	lo_SetJavaArgs
	--------------
	Extracts every param and passes them as arguements to the Java applet.  
	Here is the mapping: <... X=Y ...> becomes equivalent to <PARAM NAME=x VALUE=y>
	
	NOTE: lo_GetNextName() lo_GetNextValue() and allocates new memory for the return values.
	
	10.7.97		amusil
*/
#ifdef JAVA
static void lo_SetJavaArgs(char* tag, LO_JavaAppStruct* current_java)
{
	char* paramName;
	char* paramValue;
	char* index;
	Bool is_percent;
	int32 val;
		
	index = tag;
	while(TRUE)
		{
			paramName = lo_GetNextName(&index);
			if(paramName == NULL)
				break;

			/* always map data to src */
			if(!strcasecomp(paramName, "data"))
				{
				XP_FREE(paramName);
				paramName = XP_STRDUP("src");
				}				

			paramValue  = lo_GetNextValue(&index);

			/* Check if the user specified a % for height and width */
			if(!strcasecmp(paramName, "height"))
				{
				val = lo_ValueOrPercent(paramValue, &is_percent);
				if(is_percent)
					{
					XP_FREE(paramValue);
					/* int32 cannot exceed 10 digits */
					paramValue = XP_ALLOC(11 * sizeof(char));
					lo_itoa(current_java->height, paramValue);
					}
				}
		
			if(!strcasecmp(paramName, "width"))
				{
				val = lo_ValueOrPercent(paramValue, &is_percent);
				if(is_percent)
					{
					XP_FREE(paramValue);
					/* int32 cannot exceed 10 digits */
					paramValue = XP_ALLOC(11 * sizeof(char));
					lo_itoa(current_java->width, paramValue);
					}
				}
				
			/* increment and resize array */
#ifdef OJI
			++(current_java->parameters.n);
			current_java->parameters.names = XP_REALLOC(current_java->parameters.names, current_java->parameters.n*sizeof(char*));
			XP_ASSERT(current_java->parameters.names);
			current_java->parameters.values = XP_REALLOC(current_java->parameters.values, current_java->parameters.n*sizeof(char*));
			XP_ASSERT(current_java->parameters.values);
			
			/* point the new array elements to the newly allocated paramName and paramValue */
			current_java->parameters.names[current_java->parameters.n-1] = paramName;
			current_java->parameters.values[current_java->parameters.n-1] = paramValue;
#else
			++(current_java->param_count);
			current_java->param_names = XP_REALLOC(current_java->param_names, current_java->param_count*sizeof(char*));
			XP_ASSERT(current_java->param_names);
			current_java->param_values = XP_REALLOC(current_java->param_values, current_java->param_count*sizeof(char*));
			XP_ASSERT(current_java->param_values);
			
			/* point the new array elements to the newly allocated paramName and paramValue */
			current_java->param_names[current_java->param_count-1] = paramName;
			current_java->param_values[current_java->param_count-1] = paramValue;
#endif
		}
}
#endif

static void lo_itoa(uint32 n, char* s)
{
	int i, sign;
	if((sign = n) < 0)
		n = -n;
	i = 0;
	
	do
		{
		s[i++] = n%10 + '0';
		} while ((n/=10) > 0);
		
	if(sign < 0)
		s[i++] = '-';
	s[i]  = '\0';
	lo_ReverseString(s);
}

static void lo_ReverseString(char* s)
{
	int c, i, j;
	
	for(i=0, j=strlen(s)-1; i<j; i++, j--)
		{
		c = s[i];
		s[i] = s[j];
		s[j] = c;
		}
}

#endif	/* ANTHRAX */
