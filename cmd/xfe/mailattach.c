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
   mailattach.c --- Mail Attachment stuff goes here.
   Created: Dora Hsu<dora@netscape.com>, 6-May-96.
 */


#include "mozilla.h"
#include "xfe.h"
#include <Xfe/XfeP.h> /* for _XfeHeight() */
/* for XP_GetString() */
#include <xpgetstr.h>
#include "felocale.h"


extern int XFE_INVALID_FILE_ATTACHMENT_IS_A_DIRECTORY;
extern int XFE_INVALID_FILE_ATTACHMENT_NOT_READABLE;
extern int XFE_INVALID_FILE_ATTACHMENT_DOESNT_EXIST;

extern int XFE_DLG_OK;
extern int XFE_DLG_CLEAR;
extern int XFE_DLG_CANCEL;

static char* fe_last_attach_type = NULL;

static void
fe_attachmentUpdate(struct fe_mail_attach_data* mad )
{
  const char* ptr;
  char *str = NULL;

  if ( mad && mad->comppane )
  {
  mad->attachments[mad->nattachments].url = NULL;
  MSG_SetAttachmentList(mad->comppane, mad->attachments);
  ptr = MSG_GetCompHeader(mad->comppane, MSG_ATTACHMENTS_HEADER_MASK);
  
  if (ptr)
  {
    str = malloc(sizeof(char)*(strlen(ptr)+1));
    strcpy(str, ptr);
  }

  if (str) free(str);
  }
}


static void
fe_del_attachment(struct fe_mail_attach_data* mad, int pos)
{
  if (pos > mad->nattachments) return;
  else XP_ASSERT(pos <= mad->nattachments);

  pos--;
  if (mad->attachments[pos].url)
    XP_FREE((char *)mad->attachments[pos].url);
  pos++;
  while (pos < mad->nattachments) {
    mad->attachments[pos-1] = mad->attachments[pos];
    pos++;
  }
  mad->nattachments--;
  fe_attachmentUpdate(mad);
}

static void
fe_add_attachmentData(struct fe_mail_attach_data *mad,
			const struct MSG_AttachmentData *data)
{
  struct MSG_AttachmentData *m;
  char *name = (char *)data->url;
  XmString xmstr;

  if (!name || !*name) return;

  if (mad->nattachments >= XFE_MAX_ATTACHMENTS) return;
  else XP_ASSERT(mad->nattachments < XFE_MAX_ATTACHMENTS);

  xmstr = XmStringCreate(name, XmSTRING_DEFAULT_CHARSET);
  XmListAddItem(mad->list, xmstr, 0);

  m = &mad->attachments[mad->nattachments];
  *m = *data;
  m->url = XP_STRDUP(data->url);

  mad->nattachments++;
  if (mad->nattachments == 1)
    XmListSelectPos(mad->list, 1, True);

  XmListSelectItem(mad->list, xmstr, TRUE);
  XmStringFree(xmstr);
}


static void
fe_add_attachment(struct fe_mail_attach_data *mad, char *name)
{
  struct MSG_AttachmentData m = {0};
  Boolean b;
  XmString xmstr;

  if (!name || !*name) return;

  if(mad->nattachments >= XFE_MAX_ATTACHMENTS) return;
  else XP_ASSERT(mad->nattachments < XFE_MAX_ATTACHMENTS);

  xmstr = XmStringCreate(name, XmSTRING_DEFAULT_CHARSET);
  XmListAddItem(mad->list, xmstr, 0);

  XtVaGetValues (mad->text_p, XmNset, &b, 0);
  if (b)
    m.desired_type = TEXT_PLAIN;
  else {
    XtVaGetValues (mad->postscript_p, XmNset, &b, 0);
    if (b)
      m.desired_type = APPLICATION_POSTSCRIPT;
  }
  m.url = name;

  mad->attachments[mad->nattachments] = m;
  mad->nattachments++;
  fe_attachmentUpdate(mad);

  XmListSelectItem(mad->list, xmstr, TRUE);
  XmStringFree(xmstr);
}
/***********************************
 * Location popup related routines *
 ***********************************/

static void
fe_locationOk_cb (Widget widget, XtPointer closure, XtPointer call_data)
{
  struct fe_mail_attach_data *mad = (struct fe_mail_attach_data  *) closure;
  char *url = fe_GetTextField(mad->location_text);

  if (url && *url)
    fe_add_attachment(mad, url);
  else
    if (url) XtFree(url);
  XtUnmanageChild(mad->location_shell);
}
static void
fe_locationClear_cb (Widget widget, XtPointer closure, XtPointer call_data)
{
  struct fe_mail_attach_data *mad = (struct fe_mail_attach_data  *) closure;
  fe_SetTextField (mad->location_text, "");
  /* Focus on the text widget after this, since otherwise you have to
     click again. */
  XmProcessTraversal (mad->location_text, XmTRAVERSE_CURRENT);
}

static void
fe_locationCancel_cb (Widget widget, XtPointer closure, XtPointer call_data)
{
  struct fe_mail_attach_data *mad = (struct fe_mail_attach_data  *) closure;
  XtUnmanageChild(mad->location_shell);
}

static void
fe_attach_make_location(struct fe_mail_attach_data *mad)
{
  Arg av [20];
  int ac;
  Visual *v = 0;
  Colormap cmap = 0;
  Cardinal depth = 0;

  Widget shell;
  Widget parent;
  Widget form;
  Widget label, location_label, location_text;
  Widget ok_button, clear_button, cancel_button;

  Widget kids [20];
  int i;

  if (mad->location_shell) return;

#ifdef dp_DEBUG
  fprintf(stderr, "Making attach_location widgets : fe_attach_make_location().\n");
#endif

  parent = CONTEXT_WIDGET(mad->context);

  XtVaGetValues (parent, XtNvisual, &v, XtNcolormap, &cmap,
		 XtNdepth, &depth, 0);

  ac = 0;
  XtSetArg (av[ac], XmNvisual, v); ac++;
  XtSetArg (av[ac], XmNdepth, depth); ac++;
  XtSetArg (av[ac], XmNcolormap, cmap); ac++;
/*  XtSetArg (av[ac], XmNallowShellResize, True); ac++; */
/*  XtSetArg (av[ac], XmNtransientFor, parent); ac++; */
  XtSetArg (av[ac], XmNautoUnmanage, False); ac++;
  XtSetArg (av[ac], XmNdeleteResponse, XmUNMAP); ac++;
  XtSetArg (av[ac], XmNdialogStyle, XmDIALOG_MODELESS); ac++;
  shell = XmCreateTemplateDialog (parent, "location_popup", av, ac);

  fe_UnmanageChild_safe (XmMessageBoxGetChild (shell, XmDIALOG_OK_BUTTON));
  fe_UnmanageChild_safe (XmMessageBoxGetChild (shell, XmDIALOG_CANCEL_BUTTON));
  fe_UnmanageChild_safe (XmMessageBoxGetChild (shell, XmDIALOG_DEFAULT_BUTTON));
  fe_UnmanageChild_safe (XmMessageBoxGetChild (shell, XmDIALOG_HELP_BUTTON));

  ac = 0;
  ok_button = XmCreatePushButtonGadget (shell,
										XP_GetString(XFE_DLG_OK),
										av, ac);
  clear_button = XmCreatePushButtonGadget (shell, 
										   XP_GetString(XFE_DLG_CLEAR), 
										   av, ac);
  cancel_button = XmCreatePushButtonGadget (shell, 
											XP_GetString(XFE_DLG_CANCEL),
														 av, ac);

  ac = 0;
  form = XmCreateForm(shell, "form", av, ac);
  label = XmCreateLabelGadget( form, "label", av, ac);
  location_label = XmCreateLabelGadget( form, "locationLabel", av, ac);
  location_text = fe_CreateTextField( form, "locationText", av, ac);

  if (fe_globalData.nonterminal_text_translations)
    XtOverrideTranslations (location_text, fe_globalData.
				nonterminal_text_translations);

  XtVaSetValues (label,
		 XmNtopAttachment, XmATTACH_FORM,
		 XmNbottomAttachment, XmATTACH_NONE,
		 XmNleftAttachment, XmATTACH_FORM,
		 XmNrightAttachment, XmATTACH_FORM,
		 0);
  XtVaSetValues (location_label,
		 XmNtopAttachment, XmATTACH_WIDGET,
		 XmNtopWidget, label,
		 XmNbottomAttachment, XmATTACH_NONE,
		 XmNleftAttachment, XmATTACH_FORM,
		 XmNrightAttachment, XmATTACH_NONE,
		 0);
  XtVaSetValues (location_text,
		 XmNtopAttachment, XmATTACH_WIDGET,
		 XmNtopWidget, label,
		 XmNbottomAttachment, XmATTACH_NONE,
		 XmNleftAttachment, XmATTACH_WIDGET,
		 XmNleftWidget, location_label,
		 XmNrightAttachment, XmATTACH_FORM,
		 0);

  _XfeHeight(location_label) = _XfeHeight(location_text);

  XtAddCallback(ok_button, XmNactivateCallback, fe_locationOk_cb, mad);
  XtAddCallback(clear_button, XmNactivateCallback, fe_locationClear_cb, mad);
  XtAddCallback(cancel_button, XmNactivateCallback, fe_locationCancel_cb, mad);

  fe_HackDialogTranslations (form);

  mad->location_shell = shell;
  i = 0;
  kids[i++] = mad->location_text = location_text;
  kids[i++] = location_label;
  kids[i++] = label;
  XtManageChildren(kids, i);
  i = 0;
  kids[i++] = ok_button;
  kids[i++] = clear_button;
  kids[i++] = cancel_button;
  XtManageChildren(kids, i);

  XtManageChild(form);
  XtManageChild(shell);
}

/*************************
 * File Attachment popup *
 *************************/
static void 
fe_attachFile_cb (Widget widget, XtPointer closure, XtPointer call_data)
{
  struct fe_mail_attach_data *mad = (struct fe_mail_attach_data *) closure;
  XmFileSelectionBoxCallbackStruct *sbc =
    (XmFileSelectionBoxCallbackStruct *) call_data;
  char *file;
  char *msg;

  switch (sbc->reason) {
    case XmCR_NO_MATCH:
	XBell (XtDisplay (widget), 0);
	break;

    case XmCR_OK:
	XmStringGetLtoR (sbc->value, XmSTRING_DEFAULT_CHARSET, &file);
	if (!fe_isFileExist(file)) {
	    msg = PR_smprintf( XP_GetString( XFE_INVALID_FILE_ATTACHMENT_DOESNT_EXIST ), file);
	    if (msg) {
		fe_Alert_2(XtParent(mad->file_shell), msg);
		XP_FREE(msg);
	    }
	}
	else if (!fe_isFileReadable(file)) {
	    msg = PR_smprintf( XP_GetString( XFE_INVALID_FILE_ATTACHMENT_NOT_READABLE ) , file);
	    if (msg) {
		fe_Alert_2(XtParent(mad->file_shell), msg);
		XP_FREE(msg);
	    }
	}
	else if (fe_isDir(file)) {
	    msg = PR_smprintf( XP_GetString( XFE_INVALID_FILE_ATTACHMENT_IS_A_DIRECTORY ), file);
	    if (msg) {
		fe_Alert_2(XtParent(mad->file_shell), msg);
		if (msg) XP_FREE(msg);
	    }
	}
	else {
	  fe_add_attachment(mad, file);
	  XtUnmanageChild(mad->file_shell);
	}
	break;

    case XmCR_CANCEL:
	XtUnmanageChild(mad->file_shell);
	break;
    default:
      abort ();
  }
}

static void
fe_attach_make_file(struct fe_mail_attach_data *mad)
{
  char *text = 0;
  XmString xmpat, xmfile;
  char buf [1024];
  Arg av [20];
  int ac;
  Visual *v = 0;
  Colormap cmap = 0;
  Cardinal depth = 0;
  Widget shell;
  Widget parent;
  Widget fileb;
  Boolean dirp = False;

  if (mad->file_shell) return;

#ifdef dp_DEBUG
  fprintf(stderr, "Making attach_file widgets : fe_attach_make_file().\n");
#endif

  parent = CONTEXT_WIDGET (mad->context);

  XtVaGetValues (parent, XtNvisual, &v, XtNcolormap, &cmap,
		 XtNdepth, &depth, 0);
/***
  text = fe_GetTextField(text_field);
  text = fe_StringTrim (text);
***/

  if (!text || !*text) {
    xmpat = 0;
    xmfile = 0;
  }
  else if (dirp) {
    if (text [strlen (text) - 1] == '/')
	text [strlen (text) - 1] = 0;
    PR_snprintf (buf, sizeof (buf), "%.900s/*", text);
    xmpat = XmStringCreateLtoR (buf, XmSTRING_DEFAULT_CHARSET);
    xmfile = XmStringCreateLtoR (text, XmSTRING_DEFAULT_CHARSET);
  }
  else {
    char *f;
    if (text [strlen (text) - 1] == '/')
      PR_snprintf (buf, sizeof (buf), "%.900s/*", text);
    else
      PR_snprintf (buf, sizeof (buf), "%.900s", text);
    xmfile = XmStringCreateLtoR (buf, XmSTRING_DEFAULT_CHARSET);
    f = strrchr (text, '/');
    if (f && f != text)
      *f = 0;
    PR_snprintf (buf, sizeof (buf), "%.900s/*", text);
    xmpat = XmStringCreateLtoR (buf, XmSTRING_DEFAULT_CHARSET);
  }
  if (text) free (text);

  ac = 0;
  XtSetArg (av[ac], XmNvisual, v); ac++;
  XtSetArg (av[ac], XmNdepth, depth); ac++;
  XtSetArg (av[ac], XmNcolormap, cmap); ac++;
/*  XtSetArg (av[ac], XmNallowShellResize, True); ac++;*/
  XtSetArg (av[ac], XmNdeleteResponse, XmUNMAP); ac++;
  XtSetArg (av[ac], XmNdialogStyle, XmDIALOG_MODELESS); ac++;
  shell = XmCreateDialogShell (parent, "fileBrowser_popup", av, ac);

  ac = 0;
  XtSetArg (av[ac], XmNfileTypeMask,
	    (dirp ? XmFILE_DIRECTORY : XmFILE_REGULAR)); ac++;
  fileb = XmCreateFileSelectionBox (shell, "fileBrowser", av, ac);
#ifdef NO_HELP
  fe_UnmanageChild_safe (XmSelectionBoxGetChild (fileb, XmDIALOG_HELP_BUTTON));
#endif

  if (xmpat) {
    XtVaSetValues (fileb, XmNdirMask, xmpat, 0);
    XtVaSetValues (fileb, XmNpattern, xmpat, 0);
    XmFileSelectionDoSearch (fileb, xmpat);
    XtVaSetValues (fileb, XmNdirSpec, xmfile, 0);
    XmStringFree (xmpat);
    XmStringFree (xmfile);
  }

  XtAddCallback(fileb, XmNnoMatchCallback, fe_attachFile_cb, mad);
  XtAddCallback(fileb, XmNokCallback,      fe_attachFile_cb, mad);
  XtAddCallback(fileb, XmNcancelCallback,  fe_attachFile_cb, mad);

  mad->file_shell = fileb;

  fe_HackDialogTranslations (fileb);

  fe_NukeBackingStore (fileb);
  XtManageChild (fileb);
}

static void
fe_attach_doc_type_cb (Widget widget, XtPointer closure, XtPointer call_data)
{
  struct fe_mail_attach_data *mad = (struct fe_mail_attach_data  *) closure;
  int *poslist, npos;
  int attach_pos = -1;

  if (XmListGetSelectedPos(mad->list, &poslist, &npos)) {
    attach_pos = poslist[npos - 1] - 1;
    XP_FREE(poslist);
  }

  /*
   * my how intuitive, if the file is attach as siurce, desired type = NULL.
   */
  XtVaSetValues (widget, XmNset, True, 0);
  if (widget == mad->text_p) {
    XtVaSetValues (mad->source_p, XmNset, False, 0);
    XtVaSetValues (mad->postscript_p, XmNset, False, 0);
    if (attach_pos >= 0)
      mad->attachments[attach_pos].desired_type = TEXT_PLAIN;
  }
  else if (widget == mad->source_p) {
    XtVaSetValues (mad->text_p, XmNset, False, 0);
    XtVaSetValues (mad->postscript_p, XmNset, False, 0);
    if (attach_pos >= 0)
      mad->attachments[attach_pos].desired_type = NULL;
  }
  else if (widget == mad->postscript_p) {
    XtVaSetValues (mad->source_p, XmNset, False, 0);
    XtVaSetValues (mad->text_p, XmNset, False, 0);
    if (attach_pos >= 0)
      mad->attachments[attach_pos].desired_type = APPLICATION_POSTSCRIPT;
  }
  else
    abort ();
}

#if 0
static void
fe_attachDestroy_cb (Widget widget, XtPointer closure, XtPointer call_data)
{
  struct fe_mail_attach_data *mad = (struct fe_mail_attach_data  *) closure;
  int i;

#ifdef dp_DEBUG
  fprintf(stderr, "fe_attachDestroy_cb()\n");
  fprintf(stderr, "Destroying fe_mail_attach_data...\n");
#endif

  /* Free the list of attachments too */
  for(i=0; i<mad->nattachments; i++) {
    XP_FREE((char *)mad->attachments[i].url);
  }
  if (mad->location_shell)
    XtDestroyWidget(mad->location_shell);
  if (mad->file_shell)
    XtDestroyWidget(XtParent(mad->file_shell));
/* This should be an error ** CONTEXT_DATA(mad->context) ->mad = NULL; */
  free (mad);
}
#endif

#if 0
static void
fe_attachCancel_cb (Widget widget, XtPointer closure, XtPointer call_data)
{
  struct fe_mail_attach_data *mad = (struct fe_mail_attach_data  *) closure;

  /* We dont need to delete all the attachments that we have as they will
     be deleted either the next time we show the attach window (or) when
     the message composition context gets destroyed */

  XtUnmanageChild(mad->shell);
  if (mad->location_shell)
    XtUnmanageChild(mad->location_shell);
  if (mad->file_shell)
    XtUnmanageChild(mad->file_shell);
}
#endif

#if 0
static void
fe_attachOk_cb (Widget widget, XtPointer closure, XtPointer call_data)
{
  struct fe_mail_attach_data *mad = (struct fe_mail_attach_data  *) closure;
  const char* ptr;
  char *str = NULL;

  mad->attachments[mad->nattachments].url = NULL;
  MSG_SetAttachmentList(mad->comppane, mad->attachments);
  ptr = MSG_GetCompHeader(mad->comppane, MSG_ATTACHMENTS_HEADER_MASK);
 
  if ( ptr )
  {
  str = malloc(sizeof(char)*(strlen(ptr)+1));
  strcpy(str, ptr);
  }

  if (str) free(str);

  /* We dont need to delete all the attachments that we have as they will
     be deleted either the next time we show the attach window (or) when
     the message composition context gets destroyed */

  XtUnmanageChild(mad->shell);
  if (mad->location_shell)
    XtUnmanageChild(mad->location_shell);
  if (mad->file_shell)
    XtUnmanageChild(mad->file_shell);
}
#endif

static void
fe_attach_location_cb (Widget widget, XtPointer closure, XtPointer call_data)
{
  struct fe_mail_attach_data *mad = (struct fe_mail_attach_data  *) closure;

  if (!mad->location_shell) fe_attach_make_location(mad);

  XtManageChild(mad->location_shell);
  XMapRaised(XtDisplay(mad->location_shell), XtWindow(mad->location_shell));
}

static void
fe_attach_file_cb (Widget widget, XtPointer closure, XtPointer call_data)
{
  struct fe_mail_attach_data *mad = (struct fe_mail_attach_data  *) closure;

  if (!mad->file_shell) fe_attach_make_file(mad);

  XtManageChild(mad->file_shell);
  XMapRaised(XtDisplay(mad->file_shell), XtWindow(mad->file_shell));
}

static void
fe_attach_delete_cb (Widget widget, XtPointer closure, XtPointer call_data)
{
  struct fe_mail_attach_data *mad = (struct fe_mail_attach_data  *) closure;
  int *poslist, npos;
  int i, pos;

  if (XmListGetSelectedPos(mad->list, &poslist, &npos)) {
    for(i=0; i<npos; i++) {
      XmListDeletePos(mad->list, poslist[i]);
      fe_del_attachment(mad, poslist[i]);
    }
    /*
     * After deleting an item from the list, select the
     * previous item in the list (or the last if it was
     * the first.)
     */
    pos = poslist[npos - 1] - 1;
    if (pos < 0)
      pos = 0;
    XmListSelectPos(mad->list, pos, TRUE);
    XP_FREE(poslist);
  }

  /*
   * If nothing left in the list selected, desensitize
   * the delete button.
   */
  if (!XmListGetSelectedPos(mad->list, &poslist, &npos)) {
    XtVaSetValues(mad->delete_attach, XmNsensitive, False, 0);
  }
  else XP_FREE(poslist);
}

static void
fe_attach_select_cb (Widget widget, XtPointer closure, XtPointer call_data)
{
  struct fe_mail_attach_data *mad = (struct fe_mail_attach_data  *) closure;
  XmListCallbackStruct *cbs = (XmListCallbackStruct *) call_data;
  const char *s;
  Widget which_w = 0;

  if (cbs->item_position > mad->nattachments) return;
  else XP_ASSERT(cbs->item_position <= mad->nattachments);

  s = mad->attachments[cbs->item_position-1].desired_type;

  if (!s || !*s)
    which_w = mad->source_p;
  else if (!XP_STRCMP(s, APPLICATION_POSTSCRIPT))
    which_w = mad->postscript_p;
  else if (!XP_STRCMP(s, TEXT_PLAIN))
    which_w = mad->text_p;

  if (which_w == 0) return;
  else XP_ASSERT (which_w != 0);
  fe_attach_doc_type_cb(which_w, (XtPointer)mad, (XtPointer)NULL);

  XtVaSetValues(mad->delete_attach, XmNsensitive, True, 0);
}

void
fe_make_new_attach_list (MWContext *context)
{
  fe_ContextData* data = CONTEXT_DATA(context);
  struct fe_mail_attach_data *mad = data->mad;
  const struct MSG_AttachmentData *list;
  int i;

  /* Free the existing list of attachments */
  XmListDeleteAllItems(mad->list);
  for(i=0; i<mad->nattachments; i++) {
    XP_FREE((char *)mad->attachments[i].url);
  }
  mad->comppane = data->comppane;
  mad->nattachments = 0;
    
  /* Refresh the list of attachments */
  list = MSG_GetAttachmentList(mad->comppane);
/*
  list = MSG_GetCompHeader(mad->comppane, MSG_ATTACHMENTS_HEADER_MASK);
*/
  while (list && list->url != NULL) {
    fe_add_attachmentData(mad, list);
    list++;
  }
}

Widget
fe_make_managed_attach_form(MWContext* context, Widget parent)
{
  fe_ContextData* data = CONTEXT_DATA(context);
  Widget list;
  Widget messb;
  Widget   attach_location, attach_file, delete_attach;
  Widget kids [50];
  Arg av [20];
  int ac;
  struct fe_mail_attach_data *mad = data->mad;
  Widget label;
  Widget   text_p, source_p, postscript_p;
  Widget form;


  XP_ASSERT(context->type == MWContextMessageComposition);

  if (mad)
  {
    fe_make_new_attach_list(context);
    return NULL;
  }

  ac = 0;
  XtSetArg (av[ac], XmNtopAttachment, XmATTACH_FORM); ac++;
  XtSetArg (av[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
  XtSetArg (av[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
  XtSetArg (av[ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
  form = XmCreateForm (parent, "attachForm", av, ac);

  ac = 0;
  XtSetArg (av[ac], XmNdialogStyle, XmDIALOG_WORK_AREA); ac++;
  XtSetArg (av[ac], XmNresizePolicy, XmRESIZE_GROW); ac++;
  messb = XmCreateMessageBox(form, "messagebox", av, ac);
  fe_UnmanageChild_safe(XmMessageBoxGetChild(messb, XmDIALOG_SEPARATOR));
  fe_UnmanageChild_safe(XmMessageBoxGetChild(messb, XmDIALOG_OK_BUTTON));
  fe_UnmanageChild_safe(XmMessageBoxGetChild(messb, XmDIALOG_CANCEL_BUTTON));
  fe_UnmanageChild_safe(XmMessageBoxGetChild(messb, XmDIALOG_HELP_BUTTON));
  ac = 0;
  list = XmCreateScrolledList(messb, "list", av, ac);
  attach_location = XmCreatePushButtonGadget(messb, "attachLocation", av, ac);
  attach_file = XmCreatePushButtonGadget(messb, "attachFile", av, ac);
  ac = 0;
  XtSetArg (av[ac], XmNsensitive, False); ac++;
  delete_attach = XmCreatePushButtonGadget(messb, "delete", av, ac);


  ac = 0;
  label = XmCreateLabelGadget (form, "label", av, ac);
  ac = 0;
  XtSetArg (av[ac], XmNset, False); ac++;
  source_p = XmCreateToggleButtonGadget (form, "sourceToggle", av, ac);
  text_p = XmCreateToggleButtonGadget (form, "textToggle", av, ac);
  postscript_p = XmCreateToggleButtonGadget (form, "postscriptToggle", av, ac);

  /* Making the attachments in such a way that the list would grow is
     the height of the dialog is increased */
  XtVaSetValues (messb,
		 XmNtopAttachment, XmATTACH_FORM,
		 XmNbottomAttachment, XmATTACH_FORM,
		 XmNleftAttachment, XmATTACH_FORM,
		 XmNrightAttachment, XmATTACH_FORM,
		 0);
  XtVaSetValues (label,
                 XmNtopAttachment, XmATTACH_NONE,
                 XmNbottomAttachment, XmATTACH_FORM,
                 XmNleftAttachment, XmATTACH_FORM,
                 XmNrightAttachment, XmATTACH_NONE,
                 0);
  XtVaSetValues (source_p,
                 XmNtopAttachment, XmATTACH_NONE,
                 XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
                 XmNbottomWidget, label,
                 XmNleftAttachment, XmATTACH_WIDGET,
                 XmNleftWidget, label,
                 XmNrightAttachment, XmATTACH_NONE,
                 0);
  XtVaSetValues (text_p,
                 XmNtopAttachment, XmATTACH_NONE,
                 XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
                 XmNbottomWidget, label,
                 XmNleftAttachment, XmATTACH_WIDGET,
                 XmNleftWidget, source_p,
                 XmNrightAttachment, XmATTACH_NONE,
                 0);
  XtVaSetValues (postscript_p,
                 XmNtopAttachment, XmATTACH_NONE,
                 XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
                 XmNbottomWidget, label,
                 XmNleftAttachment, XmATTACH_WIDGET,
                 XmNleftWidget, text_p,
                 XmNrightAttachment, XmATTACH_NONE,
                 0);


#ifdef dora_DEBUG
  fprintf(stderr, "Creating new fe_mail_attach_data...\n");
#endif
  data->mad = mad = (struct fe_mail_attach_data *)
    calloc (sizeof (struct fe_mail_attach_data), 1);
  mad->nattachments = 0;

  mad->context = context;
  mad->shell = 0;
  mad->list = list;

  XtManageChild(list);
  ac = 0;
  kids [ac++] = attach_location;
  kids [ac++] = mad->attach_file = attach_file;
  kids [ac++] = mad->delete_attach = delete_attach;
  XtManageChildren (kids, ac);
  ac = 0;
  kids [ac++] = messb;
  kids [ac++] = label;
  kids [ac++] = mad->source_p = source_p;
  kids [ac++] = mad->text_p = text_p;
  kids [ac++] = mad->postscript_p = postscript_p;
 
  XtManageChildren (kids, ac);
  XtManageChild (form);


#if 0
  XtAddCallback (ok_button, XmNactivateCallback, fe_attachOk_cb, mad);
  XtAddCallback (cancel_button, XmNactivateCallback, fe_attachCancel_cb, mad);
#endif
  XtAddCallback (attach_location, XmNactivateCallback,
					fe_attach_location_cb, mad);
  XtAddCallback (attach_file, XmNactivateCallback, fe_attach_file_cb, mad);
  XtAddCallback (delete_attach, XmNactivateCallback, fe_attach_delete_cb, mad);
  XtAddCallback (list, XmNbrowseSelectionCallback, fe_attach_select_cb, mad);

  XtAddCallback (text_p,   XmNvalueChangedCallback,
                 fe_attach_doc_type_cb, mad);
  XtAddCallback (source_p, XmNvalueChangedCallback,
                 fe_attach_doc_type_cb, mad);
  XtAddCallback (postscript_p, XmNvalueChangedCallback,
                 fe_attach_doc_type_cb, mad);
 
  /* Remember the last attachment typed used. */
  XtVaSetValues((fe_last_attach_type == NULL ? source_p :
                 strcmp(fe_last_attach_type, TEXT_PLAIN) == 0 ? text_p :
                 strcmp(fe_last_attach_type,
                        APPLICATION_POSTSCRIPT) == 0 ? postscript_p :
                 source_p),
                XmNset, True, 0);

  return form;
}

extern 
void
fe_MailComposeWin_ActivateFolder(MWContext *, int pos);

/* Prompts for attachment info.
 */
void
fe_mailto_attach_dialog(MWContext* context)
{
#if 0
  struct fe_mail_attach_data *mad = CONTEXT_DATA(context)->mad;
  const struct MSG_AttachmentData *list;
  int i;
#endif

  XP_ASSERT(context->type == MWContextMessageComposition);
  fe_MailComposeWin_ActivateFolder(context,1);

#if 0

  if (!mad || !mad->shell) {
/*
    fe_make_attach_dialog(context);
*/
    mad = CONTEXT_DATA(context)->mad;
  }

  /* Free the existing list of attachments */
  XmListDeleteAllItems(mad->list);
  for(i=0; i<mad->nattachments; i++) {
    XP_FREE((char *)mad->attachments[i].url);
  }
  mad->nattachments = 0;
    
  /* Refresh the list of attachments */
  list = MSG_GetAttachmentList(mad->comppane);
  while (list && list->url != NULL) {
    fe_add_attachmentData(mad, list);
    list++;
  }

  XtManageChild(mad->shell);
  XMapRaised(XtDisplay(mad->shell), XtWindow(mad->shell));
#endif
  return;
}



void
fe_attach_dropfunc(Widget dropw, void* closure, fe_dnd_Event type,
		   fe_dnd_Source* source, XEvent* event)
{
  MWContext *compose_context;
  MWContext *src_context;
  const struct MSG_AttachmentData *old_list, *a;
  struct MSG_AttachmentData *new_list;
  Boolean sensitive_p = False;
  char **urls, **ss;
  const char *s;
  char *str = NULL;
  int old_count = 0;
  int new_count = 0;
  int i;

  if (type != FE_DND_DROP)
    return;

  compose_context = (MWContext *) closure;
  if (!compose_context) return;
  XP_ASSERT(compose_context->type == MWContextMessageComposition);
  if (compose_context->type != MWContextMessageComposition)
    return;

  XtVaGetValues(CONTEXT_DATA(compose_context)->mcAttachments,
		XmNsensitive, &sensitive_p, 0);
  if (!sensitive_p)
    {
      /* If the Attachments field is not sensitive, then that means that
	 an attachment (or delivery?) is in progress, and bad things would
	 happen were we to try and attach things right now.  So just beep.
       */
      XBell (XtDisplay (CONTEXT_WIDGET(compose_context)), 0);
      return;
    }

  src_context = fe_WidgetToMWContext((Widget) source->closure);
  if (!src_context) return;
  switch (src_context->type)
    {
    case MWContextMail:
    case MWContextNews:
      /* ###tw  Should get a list of all the selected URLs from the mail/news
       window...*/
      urls = 0;
      break;
    case MWContextBookmarks:
      /* #### Get a list of all the selected URLs out of the bookmarks
	 window... */
      urls = 0;
      break;
    default:
      XP_ASSERT(0);
      urls = 0;
      break;
    }
  if (!urls)
    {
      XBell (XtDisplay (CONTEXT_WIDGET(compose_context)), 0);
      return;
    }

  new_count = 0;
  for (ss = urls; *ss; ss++)
    new_count++;
  XP_ASSERT(new_count > 0);
  if (new_count <= 0) return; /* #### leaks `urls'; but it already asserted. */

  old_list = MSG_GetAttachmentList(CONTEXT_DATA(compose_context)->comppane);
  old_count = 0;
  if (old_list)
    for (a = old_list; a->url; a++)
      old_count++;

  new_list = (struct MSG_AttachmentData *)
    XP_ALLOC(sizeof(struct MSG_AttachmentData) * (old_count + new_count + 1));

  for (i = 0; i < old_count; i++)
    {
      XP_MEMSET(&new_list[i], 0, sizeof(new_list[i]));
      if (old_list[i].url)
	new_list[i].url = XP_STRDUP(old_list[i].url);
      if (old_list[i].desired_type)
	new_list[i].desired_type = XP_STRDUP(old_list[i].desired_type);
      if (old_list[i].real_type)
	new_list[i].real_type = XP_STRDUP(old_list[i].real_type);
      if (old_list[i].real_encoding)
	new_list[i].real_encoding = XP_STRDUP(old_list[i].real_encoding);
      if (old_list[i].real_name)
	new_list[i].real_name = XP_STRDUP(old_list[i].real_name);
      if (old_list[i].description)
	new_list[i].description = XP_STRDUP(old_list[i].description);
      if (old_list[i].x_mac_type)
	new_list[i].x_mac_type = XP_STRDUP(old_list[i].x_mac_type);
      if (old_list[i].x_mac_creator)
	new_list[i].x_mac_creator = XP_STRDUP(old_list[i].x_mac_creator);
    }

  if (new_count > 0)
    XP_MEMSET(new_list + old_count, 0,
	      sizeof(struct MSG_AttachmentData) * (new_count + 1));

  i = old_count;
  for (ss = urls; *ss; ss++)
    new_list[i++].url = *ss;

  MSG_SetAttachmentList(CONTEXT_DATA(compose_context)->comppane, new_list);

  for (i = 0; i < old_count; i++)
    {
      if (new_list[i].url) XP_FREE((char*)new_list[i].url);
      if (new_list[i].desired_type) XP_FREE((char*)new_list[i].desired_type);
      if (new_list[i].real_type) XP_FREE((char*)new_list[i].real_type);
      if (new_list[i].real_encoding) XP_FREE((char*)new_list[i].real_encoding);
      if (new_list[i].real_name) XP_FREE((char*)new_list[i].real_name);
      if (new_list[i].description) XP_FREE((char*)new_list[i].description);
      if (new_list[i].x_mac_type) XP_FREE((char*)new_list[i].x_mac_type);
      if (new_list[i].x_mac_creator) XP_FREE((char*)new_list[i].x_mac_creator);
    }
  XP_FREE (new_list);
  for (ss = urls; *ss; ss++)
    XP_FREE(*ss);
  XP_FREE(urls);

  /* Now they're attached; update the display. */
  s = MSG_GetCompHeader(CONTEXT_DATA(compose_context)->comppane,
                        MSG_ATTACHMENTS_HEADER_MASK);

  if (s)
  {
    str = malloc(sizeof(char)*(strlen(s)+1));
    strcpy(str, s);
  }

  if (str) free(str);
}
