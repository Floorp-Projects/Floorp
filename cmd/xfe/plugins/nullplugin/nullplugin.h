/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * nullplugin.h
 *
 * Implementation of the null plugins for Unix.
 *
 * dp <dp@netscape.com>
 * updated 5/1998 <pollmann@netscape.com> 
 *
 */

#define TARGET			"_blank"
#define MIME_TYPES_HANDLED	"*:.*:All types"
#define PLUGIN_NAME		"Netscape Default Plugin"
#define PLUGIN_DESCRIPTION	"The default plugin handles plugin data for mimetypes and extensions that are not specified and facilitates downloading of new plugins."
#define CLICK_TO_GET		"Click here to get the plugin"       
#define CLICK_WHEN_DONE		"Click here after installing the plugin"

#define REFRESH_PLUGIN_LIST	"javascript:navigator.plugins.refresh(true)"
#define PLUGINSPAGE_URL		"http://cgi.netscape.com/cgi-bin/plug-in_finder.cgi"
#define MESSAGE	"\
This page contains information of a type (%s) that can\n\
only be viewed with the appropriate Plug-in.\n\
\n\
Click OK to download Plugin."

#define GET 1
#define REFRESH 2

typedef struct _PluginInstance
{
    uint16 mode;
    Window window;
    Display *display;
    uint32 x, y;
    uint32 width, height;
    NPMIMEType type;
    char *message;

    NPP instance;
    char *pluginsPageUrl;
    char *pluginsFileUrl;
    NPBool pluginsHidden;
    Visual* visual;
    Colormap colormap;
    unsigned int depth;
    Widget button;
    Widget dialog;

    NPBool exists;	/* Does the widget already exist? */
    NPBool refreshing;	/* Are we refreshing the plugin list? */
    int action;		/* What action should we take? (GET or REFRESH) */

} PluginInstance;


typedef struct _MimeTypeElement
{
    NPMIMEType value;
    struct _MimeTypeElement *next;
} MimeTypeElement;

/* Global data */
extern MimeTypeElement *head;

/* Extern functions */
extern void showPluginDialog(Widget, XtPointer, XtPointer);
extern void showStatus(Widget, XtPointer, XtPointer);
extern void clearStatus(Widget, XtPointer, XtPointer);
extern void refreshPluginList(Widget, XtPointer, XtPointer);
extern int addToList(MimeTypeElement **typelist, NPMIMEType type);
extern NPMIMEType dupMimeType(NPMIMEType type);
extern Widget FE_GetToplevelWidget(void);
extern void setAction(PluginInstance *This, int action);
extern void makeWidget(PluginInstance *This);
