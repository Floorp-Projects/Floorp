/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * nullplugin.c
 *
 * Implementation of the null plugins for Unix.
 *
 * dp <dp@netscape.com>
 * updated 5/1998 <pollmann@netscape.com>
 *
 */

#include <stdio.h>
#include <Xm/Form.h>
#include <Xm/PushB.h>
#include <Xm/MessageB.h>
#include "npapi.h"
#include "nullplugin.h"

/* Global data */
MimeTypeElement *head = NULL;

static void
UnmanageChild_safe (Widget w)
{
  if (w) XtUnmanageChild (w);
}

static void
nullPlugin_cb(Widget widget, XtPointer closure, XtPointer call_data)
{
	XmAnyCallbackStruct *cb = (XmAnyCallbackStruct *) call_data;
	PluginInstance* This = (PluginInstance*) closure;
	char *url;

	switch (cb->reason) {
	    case XmCR_OK:
		XtDestroyWidget(This->dialog);
		This->dialog = NULL;  /* Don't reuse deleted dialog */

		if (This->pluginsFileUrl != NULL)
		{
		    /* Get the JavaScript command string */
		    char* buf = "javascript:netscape.softupdate.Trigger.StartSoftwareUpdate(\"%s\")";
		    url = NPN_MemAlloc(strlen(This->pluginsFileUrl) + 1 + strlen(buf) + 1);
		    if (url != NULL)
		    {
			/* Insert the file URL into the JavaScript command */
			sprintf(url, buf, This->pluginsFileUrl);
			NPN_GetURL(This->instance, url, TARGET);
			NPN_MemFree(url);
		    }
		}
		else
		{
		    /* If necessary, get the default plug-ins page resource */
		    char* address = This->pluginsPageUrl;
		    if (address == NULL)
		    {
			address = PLUGINSPAGE_URL;
		    }
		    url = NPN_MemAlloc(strlen(address) + 1 + strlen(This->type) + 1);
		    if (url != NULL)
		    {
			/* Append the MIME type to the URL */
			sprintf(url, "%s?%s", address, This->type);
			NPN_GetURL(This->instance, url, TARGET);
			NPN_MemFree(url);
		    }
		    /* Update the button label and callbacks */
		    setAction(This, REFRESH);
		}
		break;

	    case XmCR_CANCEL:
			XtDestroyWidget(This->dialog);
			This->dialog = NULL;  /* Don't reuse deleted dialog */
			break;
	}
}

void
refreshPluginList(Widget widget, XtPointer closure, XtPointer call_data)
{
	PluginInstance* This = (PluginInstance*) closure;
	if (!This) return;
	This->refreshing = TRUE;
	NPN_ReloadPlugins(TRUE);
/*
 *	NPN_GetURL(This->instance, REFRESH_PLUGIN_LIST, "_self");
 */
}

void
showPluginDialog(Widget widget, XtPointer closure, XtPointer call_data)
{
	PluginInstance* This = (PluginInstance*) closure;
	Widget dialog;
	Arg av[20];
	int ac;
	XmString xmstr;
	char message[1024];

	if (!This) return;

	dialog = This->dialog;

	if (dialog) {
		/* The dialog is already available */
		XtManageChild(dialog);
		XMapRaised(XtDisplay(dialog), XtWindow(dialog));
		return;
	}

	/* Create the dialog */

	sprintf(message, MESSAGE, This->type);

	xmstr = XmStringCreateLtoR(message, XmSTRING_DEFAULT_CHARSET);

	ac = 0;
	/* Copy button attributes unless hidden (no attributes) */
        if (This->visual)
	{
	    XtSetArg (av[ac], XmNvisual, This->visual); ac++;
	    XtSetArg (av[ac], XmNdepth, This->depth); ac++;
	    XtSetArg (av[ac], XmNcolormap, This->colormap); ac++;
	}
	XtSetArg (av[ac], XmNallowShellResize, TRUE); ac++;
	XtSetArg (av[ac], XmNdialogStyle, XmDIALOG_MODELESS); ac++;
	XtSetArg (av[ac], XmNdialogType, XmDIALOG_QUESTION); ac++;
	XtSetArg (av[ac], XmNdeleteResponse, XmUNMAP); ac++;
	XtSetArg (av[ac], XmNautoUnmanage, False); ac++;
	XtSetArg (av[ac], XmNmessageString, xmstr); ac++;

	if (!widget)
        {
	    widget = FE_GetToplevelWidget();
        }
	if (!widget) return;		/* This should never happen! */

	dialog = XmCreateMessageDialog(widget, "nullpluginDialog", av, ac);

	UnmanageChild_safe (XmMessageBoxGetChild (dialog,
						XmDIALOG_HELP_BUTTON));

	XtAddCallback(dialog, XmNokCallback, nullPlugin_cb, This);
	XtAddCallback(dialog, XmNcancelCallback, nullPlugin_cb, This);

	XmStringFree(xmstr);

	This->dialog = dialog;
	XtManageChild(dialog);
}

/* Create the X-Motif button widget */
void
makeWidget(PluginInstance *This)
{
	Widget netscape_widget;
	Widget form;  
	Arg av[20];
	int ac;

	if (!This) return;

	netscape_widget = XtWindowToWidget(This->display, This->window);

	ac = 0;
	XtSetArg(av[ac], XmNx, 0); ac++;
	XtSetArg(av[ac], XmNy, 0); ac++;
	XtSetArg(av[ac], XmNwidth, This->width); ac++;
	XtSetArg(av[ac], XmNheight, This->height); ac++;
	XtSetArg(av[ac], XmNborderWidth, 0); ac++;
	XtSetArg(av[ac], XmNnoResize, True); ac++;
	form = XmCreateForm(netscape_widget, "pluginForm", av, ac);

	ac = 0;
	XtSetArg(av[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg(av[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg(av[ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg(av[ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av[ac], XmNeditMode, XmMULTI_LINE_EDIT); ac++;
	/* XtSetArg (av[ac], XmNlabelString, xmstr); ac++; */
	This->button = XmCreatePushButton (form, "pluginButton", av, ac);

	if (!This->action) This->action = GET;
	setAction(This, This->action);

	XtManageChild(This->button);
	XtManageChild(form);

	/* Popup the pluginDialog if this is the first time	*
	 * we encounter this MimeType				*/
	if (addToList(&head, This->type))
		showPluginDialog(This->button, (XtPointer) This, NULL);
}

/* Update the button label and callbacks (unless hidden) */
void
setAction(PluginInstance *This, int action)
{
	int ac;
	Arg av[20];
	XmString xmstr;

	if (!This) return;

	This->action = action;

	if (action == GET)
		This->message = CLICK_TO_GET;
	else if (action == REFRESH)
		This->message = CLICK_WHEN_DONE;

	if (!This->button) return;

	xmstr = XmStringCreateLtoR(This->message, XmSTRING_DEFAULT_CHARSET);

	/* Update the message on the button */
	ac = 0;
	XtSetArg (av[ac], XmNlabelString, xmstr); ac++;
	XtSetValues(This->button, av, ac);

	/* Restore the original size of the button	*
	 * NOTE: This MUST be done in a separate call	*
	 *  after the message change or else it will	*
	 *  not work.  Also, note that width/height is	*
	 *  is used only because XmNXXXAtachment	*
	 *  parameters do not work			*/
	ac = 0;
	XtSetArg(av[ac], XmNheight, This->height); ac++;
	XtSetArg(av[ac], XmNwidth, This->width); ac++;
	XtSetValues(This->button, av, ac);

	if (action == GET)
	{
	    if (This->exists)
		XtRemoveCallback (This->button, XmNactivateCallback,
		    refreshPluginList, (XtPointer)This);
	    XtAddCallback (This->button, XmNactivateCallback,
		showPluginDialog, (XtPointer)This);
	}
	else if (action == REFRESH)
	{
	    if (This->exists)
		XtRemoveCallback (This->button, XmNactivateCallback,
		    showPluginDialog, (XtPointer)This);
	    XtAddCallback (This->button, XmNactivateCallback,
		refreshPluginList, (XtPointer)This);
	}

	if (!This->exists) {
	    XtAddEventHandler (This->button, EnterWindowMask, False,
		(XtEventHandler)showStatus, (XtPointer)This);
	    XtAddEventHandler (This->button, LeaveWindowMask, False,
		(XtEventHandler)clearStatus, (XtPointer)This);
	    This->exists = TRUE;
	}

	XmStringFree(xmstr);
}

void
showStatus(Widget widget, XtPointer closure, XtPointer call_data)
{
	PluginInstance *This = (PluginInstance *) closure;
	if (!This) return;
	if (!This->instance) return;

	NPN_Status(This->instance, This->message);
}

void
clearStatus(Widget widget, XtPointer closure, XtPointer call_data)
{
	PluginInstance *This = (PluginInstance *) closure;
	if (!This) return;
	if (!This->instance) return;

	NPN_Status(This->instance, "");
}


/* MIMETypeList maintenance routines */

static Boolean
isEqual(NPMIMEType t1, NPMIMEType t2)
{
	return(strcmp(t1, t2) == 0);
}

static Boolean
isExist(MimeTypeElement **typelist, NPMIMEType type)
{
	MimeTypeElement *ele;

	if (typelist == NULL) return False;

	ele = *typelist;
	while (ele != NULL) {
		if (isEqual(ele->value, type))
			return True;
		ele = ele->next;
	}
	return False;
}

NPMIMEType
dupMimeType(NPMIMEType type)
{
	NPMIMEType mimetype = NPN_MemAlloc(strlen(type)+1);
	if (mimetype)
		strcpy(mimetype, type);
	return(mimetype);
}

int
addToList(MimeTypeElement **typelist, NPMIMEType type)
{
	MimeTypeElement *ele;

	if (!typelist) return(0);

	if (isExist(typelist, type)) return(0);

	ele = (MimeTypeElement *) NPN_MemAlloc(sizeof(MimeTypeElement));
	ele->value = dupMimeType(type);
	ele->next = *typelist;
	*typelist = ele;
	return(1);
}
