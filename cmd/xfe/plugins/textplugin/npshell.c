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
 * Example usage of this plugin is:
 * <EMBED SRC="a.txt" WIDTH=100 HEIGHT=100>
 *	Other attributes specific to the TEXT_PLUGIN are
 * MODE=AsFile
 * MODE=AsFileOnly
 *	This will work in StreamAsFile mode and print the name of the file.
 * MODE=Seek
 *	This will work in seek mode on seekable streames and display
 *	20 bytes from the 10th byte.
 * NODATA
 *	This will make the plugin take no data. WriteReady will always return
 *	0.
 * The above modes are provided for test purposes.
 *
 * dp Suresh <dp@netscape.com>
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
#include "npapi.h"

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

    uint16 opmode;		/* Reflects the MODE attribute in the embed */
    NPBool nodata;		/* Does not accept data */
#ifdef TEXT_PLUGIN
    Widget form;		/* Form container widget */
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
	/* The registering of this plugin for text/postscript also is
	 * more intended towards showing how to register a plugin for
	 * multiple mime/types.
	 */
	return("text/plain:txt,text:Plain text;text/postscript:ps:Postscript");
#else
	return("text/plain:txt,text:Plain text;text/postscript:ps:Postscript");
#endif
}

NPError
NPP_GetValue(void *future, NPPVariable variable, void *value)
{
	NPError err = NPERR_NO_ERROR;

	switch (variable) {
		case NPPVpluginNameString:
			*((char **)value) = "Sample Text plugin";
			break;
		case NPPVpluginDescriptionString:
			*((char **)value) =
				"This plugins handles text files and displays "
				"them in a scrolled window.";
			break;
		default:
			err = NPERR_GENERIC_ERROR;
	}
	return err;
}

NPError
NPP_Initialize(void)
{
    return NPERR_NO_ERROR;
}


jref
NPP_GetJavaClass()
{
    return NULL;
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
        PluginInstance* This;

	if (instance == NULL)
		return NPERR_INVALID_INSTANCE_ERROR;
		
	instance->pdata = NPN_MemAlloc(sizeof(PluginInstance));
	
	This = (PluginInstance*) instance->pdata;

	if (This != NULL)
	{
		/* mode is NP_EMBED, NP_FULL, or NP_BACKGROUND (see npapi.h) */
		This->mode = mode;
		This->window = (Window) 0;
		This->opmode = NP_NORMAL;
		This->nodata = FALSE;
#ifdef TEXT_PLUGIN
		This->form = 0;
		This->text = 0;
#endif /* TEXT_PLUGIN */
		while (argc > 0) {
			if (!strcasecmp(argn[argc-1], "MODE") && argv[argc-1]) {
			 	if (!strcasecmp(argv[argc-1], "seek"))
					This->opmode = NP_SEEK;
			 	if (!strcasecmp(argv[argc-1], "asfile"))
					This->opmode = NP_ASFILE;
			 	if (!strcasecmp(argv[argc-1], "asfileonly"))
					This->opmode = NP_ASFILEONLY;
			}
			else if (!strcasecmp(argn[argc-1], "NODATA"))
				This->nodata = TRUE;
			argc --;
		}

		/* PLUGIN DEVELOPERS:
		 *	Initialize fields of your plugin
		 *	instance data here.  If the NPSavedData is non-
		 *	NULL, you can use that data (returned by you from
		 *	NPP_Destroy to set up the new plugin instance).
		 */
		
		return NPERR_NO_ERROR;
	}
	else
		return NPERR_OUT_OF_MEMORY_ERROR;
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
	PluginInstance* This;

	if (instance == NULL)
		return NPERR_INVALID_INSTANCE_ERROR;

	if (window == NULL)
		return NPERR_NO_ERROR;

	This = (PluginInstance*) instance->pdata;

	/*
	 * PLUGIN DEVELOPERS:
	 *	Before setting window to point to the
	 *	new window, you may wish to compare the new window
	 *	info to the previous window (if any) to note window
	 *	size changes, etc.
	 */

	if (This->window == (Window) window->window) {
		/* This could be the plugin changing size (or) just an
		 * extra SetWindow().
		 */
#ifdef TEXT_PLUGIN
		if (This->x != window->x || This->width != window->width ||
			This->y != window->y || This->height != window->height) {
			This->x = window->x;
			This->y = window->y;
			This->width = window->width;
			This->height = window->height;
			XtVaSetValues(This->form, XmNx, This->x, XmNy, This->y,
						XmNheight, This->height, XmNwidth, This->width, 0);
		}
#endif /* TEXT_PLUGIN */
	}
	else {
		This->x = window->x;
		This->y = window->y;
		This->width = window->width;
		This->height = window->height;
		This->window = (Window) window->window;
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

			This->form = XmCreateForm(netscape_widget, "pluginForm",
					av, ac);

			ac = 0;
			XtSetArg(av[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
			XtSetArg(av[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
			XtSetArg(av[ac], XmNtopAttachment, XmATTACH_FORM); ac++;
			XtSetArg(av[ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
			XtSetArg (av[ac], XmNeditMode, XmMULTI_LINE_EDIT); ac++;
			This->text = XmCreateScrolledText (This->form, "pluginText",
											   av, ac);

			XtManageChild(This->text);
			XtManageChild(This->form);
		}
#endif /* TEXT_PLUGIN */
	}

	return NPERR_NO_ERROR;
}


NPError 
NPP_NewStream(NPP instance,
			  NPMIMEType type,
			  NPStream *stream, 
			  NPBool seekable,
			  uint16 *stype)
{
	NPByteRange range;
	PluginInstance* This;

	if (instance == NULL)
		return NPERR_INVALID_INSTANCE_ERROR;

	This = (PluginInstance*) instance->pdata;

	if (seekable && This->opmode == NP_SEEK) {
		*stype = NP_SEEK;
		range.offset = 10;
		range.length = 20;
		range.next = NULL;
		NPN_RequestRead(stream, &range);
	}
	else if (This->opmode == NP_ASFILE || This->opmode == NP_ASFILEONLY)
		*stype = This->opmode;

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
	/* If in nodata mode, return 0 */
	if (This->nodata)
		return 0;

	return STREAMBUFSIZE;
}


int32 
NPP_Write(NPP instance, NPStream *stream, int32 offset, int32 len, void *buffer)
{
	if (instance != NULL)
	{
		PluginInstance* This = (PluginInstance*) instance->pdata;
		char *cbuf;
#ifdef TEXT_PLUGIN
		XmTextPosition pos = 0;
#endif /* TEXT_PLUGIN */

		/* If this was a NODATA plugin, then we should not be getting
		 * this write call.
		 */
		if (This->nodata)
			fprintf(stderr, "TextPlugin ERROR: Although NODATA attribute was used (NPP_WriteReady returned 0),\nwe got a NPP_Write call. Tell Netscape about this.\n");

		cbuf = (char *) NPN_MemAlloc(len + 1);
		if (!cbuf) return 0;
		memcpy(cbuf, (char *)buffer, len);
		cbuf[len] = '\0';
#ifdef TEXT_PLUGIN
		if (This->text) {
			XtVaGetValues(This->text, XmNcursorPosition, &pos, 0);
			XmTextInsert(This->text, pos, cbuf);
		}
		else {
			/* This must be a hidden plugin */
			fprintf(stderr, "%s", cbuf);
		}
#else /* TEXT_PLUGIN */
		fprintf(stderr, "%s", cbuf);
#endif /* TEXT_PLUGIN */
		NPN_MemFree(cbuf);
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
#ifdef TEXT_PLUGIN
	fprintf(stderr, "Got NPP_StreamAsFile: %s.\n",
			(fname == NULL)?"null":fname);
#endif /* TEXT_PLUGIN */
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

#ifdef TEXT_PLUGIN
			fprintf(stderr, "NPP_Print: FullPage print. platformPrint=%x, printOne=%s\n",
				  platformPrint, printOne ? "True":"False");
#endif /* TEXT_PLUGIN */
			
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
#ifdef TEXT_PLUGIN
			fprintf(stderr, "NPP_Print: EmbedPage print. platformPrint=%x,\n",
				  platformPrint);
			fprintf(stderr, "         : window(%d) : (%d, %d) [%d x %d]\n",
					printWindow->window, printWindow->x, printWindow->y,
					printWindow->width, printWindow->height);
#endif /* TEXT_PLUGIN */
		}
	}
}
