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
 * npshell.c
 *
 * Netscape Client Plugin API
 * - Function that need to be implemented by plugin developers
 *
 * This file defines a "shell" plugin that plugin developers can use
 * as the basis for a real plugin.  This shell just provides empty
 * implementations of all functions that the plugin can implement
 * that will be called by Netscape (the NPP_xxx methods defined in 
 * npapi.h). 
 *
 * dp Suresh <dp@netscape.com>
 * Modified by Warren Harris to test Java stuff. <warren@netscape.com>
 *
 *----------------------------------------------------------------------
 * PLUGIN DEVELOPERS:
 *	Implement your plugins here.
 *	A sample text plugin is implemented here.
 *	All the sample plugin does is displays any file with a
 *	".txt" extension in a scrolled text window. It uses Motif.
 *----------------------------------------------------------------------
 */

#include <stdio.h>
#include "prosdep.h"	/* include first to get the right #defines */
#include "npapi.h"

#include "JavaTestPlugin.h"
#include "java_lang_Class.h"
#include "java_lang_String.h"
#ifdef EMBEDDED_FRAMES
#include "java_awt_EmbeddedFrame.h"
#include "java_awt_Component.h"
#include "java_awt_Color.h"
#include "java_awt_Window.h"
#endif /* EMBEDDED_FRAMES */

#define TEXT_PLUGIN

#ifdef TEXT_PLUGIN
#include <Xm/Xm.h>
#include <Xm/Form.h>
#include <Xm/Text.h>
#endif /* TEXT_PLUGIN */

/***********************************************************************
 * Instance state information about the plugin.
 *
 * PLUGIN DEVELOPERS:
 *	Use this struct to hold per-instance information that you'll
 *	need in the various functions in this file.
 ***********************************************************************/

typedef struct _PluginInstance
{
    uint16 mode;
    Window window;
    Display *display;
    uint32 x, y;
    uint32 width, height;

#ifdef TEXT_PLUGIN
    Widget text;		/* Text widget */
#endif /* TEXT_PLUGIN */
} PluginInstance;


/***********************************************************************
 *
 * Empty implementations of plugin API functions
 *
 * PLUGIN DEVELOPERS:
 *	You will need to implement these functions as required by your
 *	plugin.
 *
 ***********************************************************************/

char*
NPP_GetMIMEDescription(void)
{
#ifdef TEXT_PLUGIN
	return("text/plain:.txt");
#else
	return("application/x-data:.sample");
#endif
}

#define PLUGIN_NAME		"Java Test Plugin"
#define PLUGIN_DESCRIPTION	"Tests java natives and reflection stuff."

NPError
NPP_GetValue(void *future, NPPVariable variable, void *value)
{
    NPError err = NPERR_NO_ERROR;
    if (variable == NPPVpluginNameString)
		*((char **)value) = PLUGIN_NAME;
    else if (variable == NPPVpluginDescriptionString)
		*((char **)value) = PLUGIN_DESCRIPTION;
    else
		err = NPERR_GENERIC_ERROR;

    return err;
}

JRIEnv* env;
JRIGlobalRef classString;
JRIGlobalRef classColor;
JRIGlobalRef classFrame;
JRIGlobalRef classJavaTestPlugin;

NPError
NPP_Initialize(void)
{
    return NPERR_NO_ERROR;
}

jref
NPP_GetJavaClass(void)
{
    jref myClass;

    env = NPN_GetJavaEnv();
    myClass = init_JavaTestPlugin(env);

    /* other classes I'll need: */
    classString = JRI_NewGlobalRef(env, init_java_lang_String(env));
#ifdef EMBEDDED_FRAMES
    classColor = JRI_NewGlobalRef(env, init_java_awt_Color(env));
    classFrame = JRI_NewGlobalRef(env, java_awt_EmbeddedFrame__init(env));
#endif
    classJavaTestPlugin = JRI_NewGlobalRef(env, myClass);

    return myClass;
}

void
NPP_Shutdown(void)
{
}


NPError 
NPP_New(NPMIMEType pluginType,
	NPP instance,
	uint16 mode,
	int16 argc,
	char* argn[],
	char* argv[],
	NPSavedData* saved)
{
        NPError result = NPERR_NO_ERROR;
        PluginInstance* This;

	if (instance == NULL)
		return NPERR_INVALID_INSTANCE_ERROR;
		
	instance->pdata = NPN_MemAlloc(sizeof(PluginInstance));
	
	This = (PluginInstance*) instance->pdata;

	if (This == NULL)
	    return NPERR_OUT_OF_MEMORY_ERROR;

	{
		/* mode is NP_EMBED, NP_FULL, or NP_BACKGROUND (see npapi.h) */
		This->mode = mode;
		This->window = (Window) 0;

		/* PLUGIN DEVELOPERS:
		 *	Initialize fields of your plugin
		 *	instance data here.  If the NPSavedData is non-
		 *	NULL, you can use that data (returned by you from
		 *	NPP_Destroy to set up the new plugin instance).
		 */
	}

	/* Java Test */
	{
		jref plugin;
	    jref fact;
		int val;

		plugin = JRI_GetGlobalRef(env, classJavaTestPlugin);
	    val = JavaTestPlugin_fact(env, plugin, 10);	/* static method */
	    if (JRI_ExceptionOccurred(env)) {
			fprintf(stderr, "fact failed\n");
			result = NPERR_INVALID_INSTANCE_ERROR;
			goto done;
	    }
	    printf("Plugin: Java sez: fact(10) = %d\n", val);

	    fact = NPN_GetJavaPeer(instance);
	    if (JRI_ExceptionOccurred(env)) {
			fprintf(stderr, "couldn't create a JavaTestPlugin object\n");
			result = NPERR_INVALID_INSTANCE_ERROR;
			goto done;
	    }
	    printf("Plugin: setting foo field to %d\n", 11111);
	    set_JavaTestPlugin_foo(env, fact, 11111);
	    printf("Plugin: reading foo field as %ld\n",
			   get_JavaTestPlugin_foo(env, fact));
	    printf("Plugin: calling into java to test plugin native methods\n");
	    JavaTestPlugin_testPluginCalls(env, fact);
	    printf("Plugin: reading foo field as %ld\n",
			   get_JavaTestPlugin_foo(env, fact));
	    if (JRI_ExceptionOccurred(env)) {
			fprintf(stderr, "test returned an error\n");
			result = NPERR_INVALID_INSTANCE_ERROR;
			goto done;
	    }

	  done:
	    if (JRI_ExceptionOccurred(env)) {
			JRI_ExceptionDescribe(env);
			JRI_ExceptionClear(env);
	    }
	}
	return result;
}


NPError 
NPP_Destroy(NPP instance, NPSavedData** save)
{
	PluginInstance* This;

	if (instance == NULL)
		return NPERR_INVALID_INSTANCE_ERROR;

	This = (PluginInstance*) instance->pdata;

	/* PLUGIN DEVELOPERS:
	 *	If desired, call NP_MemAlloc to create a
	 *	NPSavedDate structure containing any state information
	 *	that you want restored if this plugin instance is later
	 *	recreated.
	 */

	if (This != NULL) {
		NPN_MemFree(instance->pdata);
		instance->pdata = NULL;
	}

	return NPERR_NO_ERROR;
}



NPError 
NPP_SetWindow(NPP instance, NPWindow* window)
{
	NPError result = NPERR_NO_ERROR;
	PluginInstance* This;

	if (instance == NULL)
		return NPERR_INVALID_INSTANCE_ERROR;

	This = (PluginInstance*) instance->pdata;

	/*
	 * PLUGIN DEVELOPERS:
	 *	Before setting window to point to the
	 *	new window, you may wish to compare the new window
	 *	info to the previous window (if any) to note window
	 *	size changes, etc.
	 */
	
	This->window = (Window) window->window;
	This->x = window->x;
	This->y = window->y;
	This->width = window->width;
	This->height = window->height;
	This->display = ((NPSetWindowCallbackStruct *)window->ws_info)->display;

#ifdef TEXT_PLUGIN
	{
	    Widget netscape_widget;
	    Widget form;
	    Arg av[20];
	    int ac;

	    netscape_widget = XtWindowToWidget(This->display, This->window);

	    ac = 0;
	    XtSetArg(av[ac], XmNx, 0); ac++;
	    XtSetArg(av[ac], XmNy, 0); ac++;
	    XtSetArg(av[ac], XmNwidth, This->width); ac++;
	    XtSetArg(av[ac], XmNheight, This->height); ac++;
	    XtSetArg(av[ac], XmNborderWidth, 0); ac++;
	    XtSetArg(av[ac], XmNnoResize, True); ac++;
	    form = XmCreateForm(netscape_widget, "pluginForm",
					av, ac);

	    ac = 0;
	    XtSetArg(av[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	    XtSetArg(av[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	    XtSetArg(av[ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	    XtSetArg(av[ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
	    XtSetArg (av[ac], XmNeditMode, XmMULTI_LINE_EDIT); ac++;
	    This->text = XmCreateScrolledText (form, "pluginText", av, ac);

	    XtManageChild(This->text);
	    XtManageChild(form);

#ifdef EMBEDDED_FRAMES
	{
	    JavaStatus err;
	    jref frame;

	    /* Create a window! */
	    frame = java_awt_EmbeddedFrame_make_1(env, 
			JRI_GetGlobalRef(env, classFrame), form);
	    if ((err = JRI_GetError(env)) != JavaStatusOk) {
		fprintf(stderr, "couldn't create an EmbeddedFrame (%d)\n", err);
		result = NPERR_INVALID_INSTANCE_ERROR;
		goto done;
	    }
	    fprintf(stderr, "frame = %p\n", frame);

	    java_awt_Component_resize(env, frame, 30, 30);
	    java_awt_Component_setBackground(env, frame,
		java_awt_Color_red_get(env, JRI_GetGlobalRef(env, classColor)));
	    
	    java_awt_Window_show(env, frame);
	    if ((err = JRI_GetError(env)) != JavaStatusOk) {
		fprintf(stderr, "couldn't show EmbeddedFrame (%d)\n", err);
		result = NPERR_INVALID_INSTANCE_ERROR;
		goto done;
	    }
	    fprintf(stderr, "frame shown\n");

	  done:
	}
#endif
	}
#endif /* TEXT_PLUGIN */

	return result;
}


NPError 
NPP_NewStream(NPP instance,
	      NPMIMEType type,
	      NPStream *stream, 
	      NPBool seekable,
	      uint16 *stype)
{
	PluginInstance* This;

	if (instance == NULL)
		return NPERR_INVALID_INSTANCE_ERROR;

	This = (PluginInstance*) instance->pdata;

	return NPERR_NO_ERROR;
}


/* PLUGIN DEVELOPERS:
 *	These next 2 functions are directly relevant in a plug-in which
 *	handles the data in a streaming manner. If you want zero bytes
 *	because no buffer space is YET available, return 0. As long as
 *	the stream has not been written to the plugin, Navigator will
 *	continue trying to send bytes.  If the plugin doesn't want them,
 *	just return some large number from NPP_WriteReady(), and
 *	ignore them in NPP_Write().  For a NP_ASFILE stream, they are
 *	still called but can safely be ignored using this strategy.
 */

int32 STREAMBUFSIZE = 0X0FFFFFFF; /* If we are reading from a file in NPAsFile
				   * mode so we can take any size stream in our
				   * write call (since we ignore it) */

int32 
NPP_WriteReady(NPP instance, NPStream *stream)
{
	PluginInstance* This;
	if (instance != NULL)
		This = (PluginInstance*) instance->pdata;

	/* Number of bytes ready to accept in NPP_Write() */
	return STREAMBUFSIZE;
}


int32 
NPP_Write(NPP instance, NPStream *stream, int32 offset, int32 len, void *buffer)
{
	if (instance != NULL)
	{
		PluginInstance* This = (PluginInstance*) instance->pdata;
#ifdef TEXT_PLUGIN
		char *cbuf;
		XmTextPosition pos = 0;

		cbuf = (char *) NPN_MemAlloc(len + 1);
		if (!cbuf) return 0;
		memcpy(cbuf, ((char *)buffer)+offset, len);
		cbuf[len] = '\0';
		XtVaGetValues(This->text, XmNcursorPosition, &pos, 0);
		XmTextInsert(This->text, pos, cbuf);
		NPN_MemFree(cbuf);
#endif /* TEXT_PLUGIN */
	}

	return len;		/* The number of bytes accepted */
}


NPError 
NPP_DestroyStream(NPP instance, NPStream *stream, NPError reason)
{
	PluginInstance* This;

	if (instance == NULL)
		return NPERR_INVALID_INSTANCE_ERROR;
	This = (PluginInstance*) instance->pdata;

	return NPERR_NO_ERROR;
}


void 
NPP_StreamAsFile(NPP instance, NPStream *stream, const char* fname)
{
	PluginInstance* This;
	if (instance != NULL)
		This = (PluginInstance*) instance->pdata;
}


void 
NPP_Print(NPP instance, NPPrint* printInfo)
{
	if(printInfo == NULL)
		return;

	if (instance != NULL) {
		PluginInstance* This = (PluginInstance*) instance->pdata;
	
		if (printInfo->mode == NP_FULL) {
		    /*
		     * PLUGIN DEVELOPERS:
		     *	If your plugin would like to take over
		     *	printing completely when it is in full-screen mode,
		     *	set printInfo->pluginPrinted to TRUE and print your
		     *	plugin as you see fit.  If your plugin wants Netscape
		     *	to handle printing in this case, set
		     *	printInfo->pluginPrinted to FALSE (the default) and
		     *	do nothing.  If you do want to handle printing
		     *	yourself, printOne is true if the print button
		     *	(as opposed to the print menu) was clicked.
		     *	On the Macintosh, platformPrint is a THPrint; on
		     *	Windows, platformPrint is a structure
		     *	(defined in npapi.h) containing the printer name, port,
		     *	etc.
		     */

			void* platformPrint =
				printInfo->print.fullPrint.platformPrint;
			NPBool printOne =
				printInfo->print.fullPrint.printOne;
			
			/* Do the default*/
			printInfo->print.fullPrint.pluginPrinted = FALSE;
		}
		else {	/* If not fullscreen, we must be embedded */
		    /*
		     * PLUGIN DEVELOPERS:
		     *	If your plugin is embedded, or is full-screen
		     *	but you returned false in pluginPrinted above, NPP_Print
		     *	will be called with mode == NP_EMBED.  The NPWindow
		     *	in the printInfo gives the location and dimensions of
		     *	the embedded plugin on the printed page.  On the
		     *	Macintosh, platformPrint is the printer port; on
		     *	Windows, platformPrint is the handle to the printing
		     *	device context.
		     */

			NPWindow* printWindow =
				&(printInfo->print.embedPrint.window);
			void* platformPrint =
				printInfo->print.embedPrint.platformPrint;
		}
	}
}

/******************************************************************************/

/* public native doPluginStuff(Ljava/lang/String;I)C */
jchar
JavaTestPlugin_doPluginStuff(JRIEnv* env, struct JavaTestPlugin* self,
							 struct java_lang_String * s, jint i)
{
    const jchar* chars = JRI_GetStringChars(env, s);
    return chars[0];
}

/* public native native1()V */
void
JavaTestPlugin_native1(JRIEnv* env,struct JavaTestPlugin* self)
{
    fprintf(stderr, "Plugin: native1 called!\n");
}

/* public native native2(B)B */
jbyte
JavaTestPlugin_native2(JRIEnv* env,struct JavaTestPlugin* self,jbyte b)
{
    return b + 4;
}

/* public native native3(C)C */
jchar
JavaTestPlugin_native3(JRIEnv* env,struct JavaTestPlugin* self,jchar c)
{
    return c + 1;
}

/* public native native4(S)S */
jshort

JavaTestPlugin_native4(JRIEnv* env,struct JavaTestPlugin* self,jshort s)
{
    return s + 1;
}

/* public native native5(I)I */
jint
JavaTestPlugin_native5(JRIEnv* env,struct JavaTestPlugin* self,jint i)
{
    return i + 1;
}

/* public native native6(J)J */
jlong
JavaTestPlugin_native6(JRIEnv* env,struct JavaTestPlugin* self,jlong l)
{
    jlong result, tmp;
    jlong_I2L(tmp, 65536);
    jlong_ADD(result, l, tmp);
    return result;
}

/* public native native7(F)F */
jfloat
JavaTestPlugin_native7(JRIEnv* env,struct JavaTestPlugin* self,jfloat f)
{
    return f + 1.0;
}

/* public native native8(D)D */
jdouble
JavaTestPlugin_native8(JRIEnv* env,struct JavaTestPlugin* self,jdouble d)
{
    return d + 1e10;
}

/* public native native9(Ljava/lang/String;ISCB)Ljava/lang/String; */
struct java_lang_String *
JavaTestPlugin_native9(JRIEnv* env, struct JavaTestPlugin* self,
					   struct java_lang_String * s, jint j, jlong x,
					   jfloatArray floats, jbooleanArray bools)
{
	jref clazz, ba;
	jbool* b;
	char* c;
	jsize len = JRI_GetFloatArrayLength(env, floats);
	jfloat* f = JRI_GetFloatArrayData(env, floats);
	jsize i;
	for (i = 0; i < len; i++) {
		fprintf(stderr, "float %d: %f", i, f[i]);
		f[i] *= 2.2;
		fprintf(stderr, " => %f\n", f[i]);
	}
	len = JRI_GetBooleanArrayLength(env, bools);
	b = JRI_GetBooleanArrayData(env, bools);
	for (i = 0; i < len; i++) {
		fprintf(stderr, "boolean %d: %d\n", i, b[i]);
	}
	ba = JRI_FindClass(env, JRISigArray(JRISigBoolean));
	c = JRI_CopyStringUTF(env, java_lang_Class_getName(ba, env));
	fprintf(stderr, "class [b => %p (%s)\n",                                                                                                                                                                                                                   enamed NPN_GetJavaInstance & Plugin.getNativeInstance -> NPN_GetJavaPeer & Plugin.getPeer.
	free(c);
	
	clazz = JRI_GetObjectClass(env, shorts);
	c = JRI_CopyStringUTF(env, java_lang_Class_getName(env, clazz));
	fprintf(stderr, "class of shorts => %p (%s)\n", clazz, c);
	free(c);
	
	na = JRI_NewShortArray(env, 2, ss);
	clazz = JRI_GetObjectClass(env, na);
	c = JRI_CopyStringUTF(env, java_lang_Class_getName(env, clazz));
	fprintf(stderr, "class of the new short array => %p (%s)\n", clazz, c);

	/* can we call methods with double results properly */
	d = JavaTestPlugin_native8(env, self, 2e10);
	fprintf(stderr, "calling native8(2e10) => %lf %s\n", d, 
			(d == 3e10 ? "ok" : "error"));

	/* can we call a system function */
    return java_lang_String_substring(env, s, j);
}
