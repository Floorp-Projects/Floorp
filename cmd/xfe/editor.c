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
   editor.c --- XFE editor functions
   Created: Spencer Murray <spence@netscape.com>, 11-Jan-96.
 */


#include "mozilla.h"
#include "xfe.h"
#include "selection.h"

#include "net.h"

#ifdef EDITOR

#include <X11/keysym.h>   /* for editor key translation */
#include <Xm/CutPaste.h>   /* for editor key translation */


#include <Xfe/Xfe.h>		/* for xfe widgets and utilities */

#include <xpgetstr.h>     /* for XP_GetString() */
#include <edttypes.h>
#include <edt.h>
#include <layout.h>
#include <layers.h>
#include <secnav.h>
#include <prefapi.h>

#include "menu.h"
#include "fonts.h"
#include "felocale.h"
#include "intl_csi.h"

#endif  /* EDITOR */

#include "xeditor.h"

#ifdef EDITOR

#include "il_icons.h"           /* Image icon enumeration. */

#include "edtplug.h"

#define CB_STATIC static      /* let commands.c see it */

#define XFE_EDITOR_IM_SUPPORT 1

/*  Our event loop doesn't have workprocs, so we can't have two modal
 *  dialog conditions (one from frontend, one from backend)
#define EVENT_LOOP_HAS_WORKPROC
*/
extern int XFE_EDITOR_NEW_DOCNAME;
extern int XFE_WARNING_AUTO_SAVE_NEW_MSG;
extern int XFE_FILE_OPEN;
extern int XFE_EDITOR_ALERT_FRAME_DOCUMENT;
extern int XFE_EDITOR_ALERT_ABOUT_DOCUMENT;
extern int XFE_ALERT_SAVE_CHANGES;
extern int XFE_WARNING_SAVE_CHANGES;
extern int XFE_ERROR_GENERIC_ERROR_CODE;
extern int XFE_EDITOR_COPY_DOCUMENT_BUSY;
extern int XFE_EDITOR_COPY_SELECTION_EMPTY;
extern int XFE_EDITOR_COPY_SELECTION_CROSSES_TABLE_DATA_CELL;
extern int XFE_EDITOR_COPY_DOCUMENT_BUSY;
extern int XFE_COMMAND_EMPTY;
extern int XFE_EDITOR_HTML_EDITOR_COMMAND_EMPTY;
extern int XFE_EDITOR_IMAGE_EDITOR_COMMAND_EMPTY;
extern int XFE_ACTION_SYNTAX_ERROR;
extern int XFE_ACTION_WRONG_CONTEXT;
extern int XFE_EDITOR_WARNING_REMOVE_LINK;
extern int XFE_UNKNOWN;
extern int XFE_ERROR_COPYRIGHT_HINT;
extern int XFE_ERROR_SAVING_OPTIONS;
extern int XFE_ERROR_READ_ONLY;
extern int XFE_ERROR_BLOCKED;
extern int XFE_ERROR_BAD_URL;
extern int XFE_ERROR_FILE_OPEN;
extern int XFE_ERROR_FILE_WRITE;
extern int XFE_ERROR_CREATE_BAKNAME;
extern int XFE_ERROR_DELETE_BAKFILE;
extern int XFE_ERROR_WRITING_FILE;
extern int XFE_ERROR_SRC_NOT_FOUND;
extern int XFE_WARNING_SAVE_CONTINUE;
extern int XFE_EDITOR_DOCUMENT_TEMPLATE_EMPTY;
extern int XFE_EDITOR_BROWSE_LOCATION_EMPTY;
extern int XFE_UPLOADING_FILE;
extern int XFE_SAVING_FILE;
extern int XFE_LOADING_IMAGE_FILE;
extern int XFE_FILE_N_OF_N;
extern int XFE_PREPARE_UPLOAD;

extern void fe_MailComposeDocumentLoaded(MWContext*);
extern void fe_HackEditorNotifyToolbarUpdated(MWContext* context);

static void
fe_Bell(MWContext* context)
{
    XBell(XtDisplay(CONTEXT_WIDGET(context)), 0);
}

/*
 *    Utility stuff that should be somewhere else.
 */
static Widget
fe_CreateFormDialog(MWContext* context, char* name)
{
  Widget mainw = CONTEXT_WIDGET(context);
  Widget form;
  Arg    av[20];
  int    ac;
  Visual* v = 0;
  Colormap cmap = 0;
  Cardinal depth = 0;

  /*
   *    Inherit MainW attributes.
   */
  XtVaGetValues(mainw, XtNvisual, &v, XtNcolormap, &cmap,
		XtNdepth, &depth, 0);
  ac = 0;
  XtSetArg(av[ac], XmNvisual, v); ac++;
  XtSetArg(av[ac], XmNdepth, depth); ac++;
  XtSetArg(av[ac], XmNcolormap, cmap); ac++;
  XtSetArg(av[ac], XmNtransientFor, mainw); ac++;

  XtSetArg(av[ac], XmNallowShellResize, TRUE); ac++;
  XtSetArg(av[ac], XmNdialogStyle, XmDIALOG_FULL_APPLICATION_MODAL); ac++;
  XtSetArg(av[ac], XmNautoUnmanage, False); ac++;

  form = XmCreateFormDialog(mainw, name, av, ac);

  return form;
}

static int fe_GetStandardPixmap_pixmapsInitialized;

static Pixmap
fe_GetStandardPixmap(MWContext* context, char type)
{
  Widget mainw = CONTEXT_WIDGET(context);
  Arg    av[20];
  int    ac;
  Cardinal depth;
  Screen* screen;
  Pixel   fg;
  Pixel   bg;
  char* name;
  Pixmap pixmap;

  switch (type) {
  case XmDIALOG_ERROR:       name = "xm_error";       break;
  case XmDIALOG_INFORMATION: name = "xm_information"; break;
  case XmDIALOG_QUESTION:    name = "xm_question";    break;
  case XmDIALOG_WARNING:     name = "xm_warning";     break;
  case XmDIALOG_WORKING:     name = "xm_working";     break;
  default:
    return XmUNSPECIFIED_PIXMAP;
  }

  /*
   *    This is so broken. Init MesageBox class, so the Motif
   *    standard icons get installed. YUCK..djw
   */
  if (!fe_GetStandardPixmap_pixmapsInitialized) {
    XtInitializeWidgetClass(xmMessageBoxWidgetClass);
    fe_GetStandardPixmap_pixmapsInitialized++;
  }

  /*
   *    Inherit MainW attributes.
   */
  ac = 0;
  XtSetArg(av[ac], XmNbackground, &bg); ac++;
  XtSetArg(av[ac], XmNforeground, &fg); ac++;
  XtSetArg(av[ac], XmNdepth, &depth); ac++;
  XtSetArg(av[ac], XmNscreen, &screen); ac++;
  XtGetValues(mainw, av, ac);

  pixmap = XmGetPixmapByDepth(screen, name, fg, bg, depth);
  if (pixmap == XmUNSPECIFIED_PIXMAP) {
    char default_name[256];

    strcpy(default_name, "default_");
    strcat(default_name, name);

    pixmap = XmGetPixmapByDepth(screen, default_name, fg, bg, depth);
  }

  return pixmap;
}

XtPointer
fe_GetUserData(Widget widget)
{
    void* rv;
    XtVaGetValues(widget, XmNuserData, &rv, 0);
    return rv;
}

static struct {
  unsigned type;
  char*    name;
} fe_CreateYesToAllDialog_button_data[] = {
  { XFE_DIALOG_YES_BUTTON,      "yes"      }, 
  { XFE_DAILOG_YESTOALL_BUTTON, "yesToAll" },
  { XFE_DIALOG_NO_BUTTON,       "no"       },
  { XFE_DIALOG_NOTOALL_BUTTON,  "noToAll"  },
  { XFE_DIALOG_CANCEL_BUTTON,   "cancel"   },
  { 0, NULL }
};



#define XFE_YESTOALL_YES_BUTTON      (fe_CreateYesToAllDialog_button_data[0].name)
#define XFE_YESTOALL_YESTOALL_BUTTON (fe_CreateYesToAllDialog_button_data[1].name)
#define XFE_YESTOALL_NO_BUTTON       (fe_CreateYesToAllDialog_button_data[2].name)
#define XFE_YESTOALL_NOTOALL_BUTTON  (fe_CreateYesToAllDialog_button_data[3].name)
#define XFE_YESTOALL_CANCEL_BUTTON   (fe_CreateYesToAllDialog_button_data[4].name)

typedef struct {
    Widget widget;
    char*  name;
} GetChildInfo;

static XtPointer
fe_YesToAllDialogGetChildMappee(Widget widget, XtPointer data)
{
    GetChildInfo* info = (GetChildInfo*)data;
    char*         name = XtName(widget);
    
    if (strcmp(info->name, name) == 0) {
	info->widget = widget;
	return (XtPointer)1; /* don't look any more */
    }
    return 0;
}

Widget
fe_YesToAllDialogGetChild(Widget parent, unsigned char type)
{
  char*        name;
  GetChildInfo info;

  switch (type) {
  case XFE_DIALOG_YES_BUTTON:
    name = XFE_YESTOALL_YES_BUTTON;
    break;
  case XFE_DAILOG_YESTOALL_BUTTON:
    name = XFE_YESTOALL_YESTOALL_BUTTON;
    break;
  case XFE_DIALOG_NO_BUTTON:
    name = XFE_YESTOALL_NO_BUTTON;
    break;
  case XFE_DIALOG_NOTOALL_BUTTON:
    name = XFE_YESTOALL_NOTOALL_BUTTON;
    break;
  case XFE_DIALOG_CANCEL_BUTTON:
    name = XFE_YESTOALL_CANCEL_BUTTON;
    break;
  default:
    return NULL; /* kill, kill, kill */
  }

  info.name = name;
  info.widget = NULL;
#ifdef OSF1
  fe_WidgetTreeWalk(parent, fe_YesToAllDialogGetChildMappee, (void *)&info);
#else
  fe_WidgetTreeWalk(parent, fe_YesToAllDialogGetChildMappee, &info);
#endif

  return info.widget;
}

static Widget
fe_CreateYesToAllDialog(MWContext* context, char* name, Arg* p_av, Cardinal p_ac)
{
  Arg      av[20];
  Cardinal ac;
  Widget   form;
  Widget   children[16];
  Cardinal nchildren = 0;
  Pixmap   pixmap;
  char* bname;
  Widget button;
  Widget icon;
  Widget text;
  Widget separator;
  Widget row;
  int i;
  XtCallbackRec* button_callback_rec = NULL;
  XmString msg_string = NULL;
  char namebuf[64];
  char pixmapType = XmDIALOG_WARNING;

  for (i = 0; i < p_ac; i++) {
      if (p_av[i].name == XmNarmCallback)
	  button_callback_rec = (XtCallbackRec*)p_av[i].value;
      else if (p_av[i].name == XmNmessageString)
	  msg_string = (XmString)p_av[i].value;
      else if (p_av[i].name == XmNdialogType)
	  pixmapType = (unsigned char)p_av[i].value;
  }

  form = fe_CreateFormDialog(context, name);

  strcpy(namebuf, name); strcat(namebuf, "Message");
  ac = 0;
  XtSetArg(av[ac], XmNtopAttachment, XmATTACH_FORM); ac++;
  XtSetArg(av[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
  XtSetArg(av[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
  XtSetArg(av[ac], XmNorientation, XmHORIZONTAL); ac++;
  XtSetArg(av[ac], XmNentryAlignment, XmALIGNMENT_CENTER); ac++;
  XtSetArg(av[ac], XmNisAligned, TRUE); ac++;
  XtSetArg(av[ac], XmNpacking, XmPACK_TIGHT); ac++;
  row = XmCreateRowColumn(form, namebuf, av, ac);

  nchildren = 0;

  /*
   *    Make pixmap label.
   */
  pixmap = fe_GetStandardPixmap(context, pixmapType);

  ac = 0;
  XtSetArg(av[ac], XmNlabelType, XmPIXMAP); ac++;
  XtSetArg(av[ac], XmNlabelPixmap, pixmap); ac++;
  icon = XmCreateLabelGadget(row, "icon", av, ac);
  children[nchildren++] = icon;

  /*
   *    Text.
   */
  ac = 0;
  if (msg_string)
    XtSetArg(av[ac], XmNlabelString, msg_string); ac++;
  XtSetArg(av[ac], XmNalignment, XmALIGNMENT_BEGINNING); ac++;
  XtSetArg(av[ac], XmNlabelType, XmSTRING); ac++;
  text = XmCreateLabelGadget(row, "text", av, ac);
  children[nchildren++] = text;

  XtManageChildren(children, nchildren); /* children of row */

  nchildren = 0;
  children[nchildren++] = row;

  /*
   *    Separator.
   */
  ac = 0;
  XtSetArg(av[ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
  XtSetArg(av[ac], XmNtopWidget, row); ac++;
  XtSetArg(av[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
  XtSetArg(av[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
  separator = XmCreateSeparatorGadget(form, "separator", av, ac);
  children[nchildren++] = separator;
 
  /*
   *    Yes, YesToAll, No, NoToAll, Cancel.
   */
  strcpy(namebuf, name); strcat(namebuf, "Buttons");
  ac = 0;
  XtSetArg(av[ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
  XtSetArg(av[ac], XmNtopWidget, separator); ac++;
  XtSetArg(av[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
  XtSetArg(av[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
  XtSetArg(av[ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
  XtSetArg(av[ac], XmNorientation, XmHORIZONTAL); ac++;
  XtSetArg(av[ac], XmNentryAlignment, XmALIGNMENT_CENTER); ac++;
  XtSetArg(av[ac], XmNisAligned, TRUE); ac++;
  XtSetArg(av[ac], XmNpacking, XmPACK_COLUMN); ac++;
  row = XmCreateRowColumn(form, namebuf, av, ac);
  children[nchildren++] = row;

  XtManageChildren(children, nchildren);

  nchildren = 0; /* now do buttons */
  button = NULL;
  for (i = 0; (bname = fe_CreateYesToAllDialog_button_data[i].name); i++) {

    ac = 0;
    XtSetArg(av[ac], XmNalignment, XmALIGNMENT_CENTER); ac++;
    XtSetArg(av[ac], XmNlabelType, XmSTRING); ac++;
    button = XmCreatePushButtonGadget(row, bname, av, ac);

    if (button_callback_rec)
      XtAddCallback(
		    button,
		    XmNactivateCallback,
		    button_callback_rec->callback,
		    button_callback_rec->closure
		    );
    children[nchildren++] = button;
  }

  XtManageChildren(children, nchildren);

  return form;
}

Boolean
fe_EditorDocumentIsSaved(MWContext* context)
{
    History_entry* hist_ent;

	if (!context)
		return False;

	if (EDT_IS_NEW_DOCUMENT(context))
		return False;

    hist_ent = SHIST_GetCurrent(&context->hist);

    if (!hist_ent)
		return False;

	if (hist_ent->address != NULL && NET_IsLocalFileURL(hist_ent->address))
		return True;
	else
		return False;
}


Boolean fe_SaveAsDialog(MWContext* context, char* buf, int type);

static Boolean
fe_editor_report_file_error(MWContext*   context,
			    ED_FileError code,
			    char*        filename, 
			    Boolean      ask_question)
{
    int   id;
    char* msg;
    char  buf[MAXPATHLEN];

    switch (code) {
    case ED_ERROR_READ_ONLY: /* File is marked read-only */
	id = XFE_ERROR_READ_ONLY;
	break;
    case ED_ERROR_BLOCKED: /* Can't write at this time, edit buffer blocked */
	id = XFE_ERROR_BLOCKED;
	break;
    case ED_ERROR_BAD_URL: /* URL was not a "file:" type or no string */
	id = XFE_ERROR_BAD_URL;
	break;
    case ED_ERROR_FILE_OPEN:
	id = XFE_ERROR_FILE_OPEN;
	break;
    case ED_ERROR_FILE_WRITE:
	id = XFE_ERROR_FILE_WRITE;
	break;
    case ED_ERROR_CREATE_BAKNAME:
    case ED_ERROR_FILE_RENAME_TO_BAK:
	id = XFE_ERROR_CREATE_BAKNAME;
	break;
    case ED_ERROR_DELETE_BAKFILE:
	id = XFE_ERROR_DELETE_BAKFILE;
	break;
    case ED_ERROR_SRC_NOT_FOUND:
	id = XFE_ERROR_SRC_NOT_FOUND;
	break;
    default:
	id = XFE_ERROR_GENERIC_ERROR_CODE; /* generic "...code = (%d)" */
	break;
    }

    msg = XP_GetString(XFE_ERROR_WRITING_FILE);
    sprintf(buf, msg, filename);
    strcat(buf, "\n\n");

    msg = XP_GetString(id);
    sprintf(&buf[strlen(buf)], msg, code);

    if (ask_question) {
	strcat(buf, "\n\n");
	strcat(buf, XP_GetString(XFE_WARNING_SAVE_CONTINUE));
	return XFE_Confirm(context, buf);
    } else {
	FE_Alert(context, buf);
	return TRUE;
    }
}

static Boolean
fe_editor_save_common(MWContext* context, char* target_url)
{
    History_entry* hist_ent;
    ED_FileError   code;
    Boolean        images;
    Boolean        links;
    Boolean        save_as;

    /* url->address is used for filename */
    hist_ent = SHIST_GetCurrent(&context->hist);

    if (!hist_ent)
	return FALSE;

    /*
     *    Has the title changed since it was last saved?
     */
    if (hist_ent->title && strcmp(hist_ent->title, hist_ent->address) == 0) {
	/*
	 *    BE will set this to the new file, if we don't zap it,
	 *    BE will retain the old name - yuck!
	 */
	XP_FREE(hist_ent->title);
	hist_ent->title = NULL;
    }

    if (target_url) { /* save as */
	save_as = TRUE;
	/* fe_EditorPreferencesGetLinksAndImages(context, &links, &images); */
    } else { /* save me */ 
	target_url = hist_ent->address;
	save_as = FALSE;
	/* images = FALSE;
	links = FALSE; */
    }
    /* hardts, now always use prefs for links and images, Save and Save As 
     do the same thing for adjusting links and images */
    fe_EditorPreferencesGetLinksAndImages(context, &links, &images);

	/*
	 *    Do this before the save starts, as save very quickly gets
	 *    asynchronous.
	 */
    CONTEXT_DATA(context)->is_file_saving = True;

    /*
     *    EDT_SaveFile returns true if it completed immediately.
     *    savesas uses source and dest, save just uses 
     */
    code = EDT_SaveFile(context, hist_ent->address, target_url,
			save_as, images, links);

    /* here we spin and spin until all the savings of images
     * and stuff are done so that we get a "real" status back from
     * the backend.  This makes saving semi-synchronous, good for
     * status reporting
     */
	if (code == ED_ERROR_NONE) { /* we didn't error straight away */
		while(CONTEXT_DATA(context)->is_file_saving) {
			/* do nothing, we wait here */
			fe_EventLoop();
		}
	}

    code = CONTEXT_DATA(context)->file_save_status;
    if (code != ED_ERROR_NONE) {
	/*
	 *    I'm sure if it is bad if the title is NULL, but
	 *    just in case..
	 */
	if ((hist_ent = SHIST_GetCurrent(&context->hist)) != NULL
	    && 
	    hist_ent->title == NULL) {
	    hist_ent->title = XP_STRDUP(hist_ent->address);
	}
	fe_editor_report_file_error(context, code, target_url, FALSE);
	return FALSE;
    }

    return TRUE;
}

Boolean
fe_EditorSaveAs(MWContext* context) /* launches dialog */
{
    char filebuf[MAXPATHLEN];

    if (fe_SaveAsDialog(context, filebuf, fe_FILE_TYPE_HTML)) {
	
	char urlbuf[MAXPATHLEN];
	    
	PR_snprintf(urlbuf, sizeof(urlbuf), "file:%.900s", filebuf);

	return fe_editor_save_common(context, urlbuf);
    }

    return FALSE;
}

Boolean
fe_EditorSave(MWContext* context)
{
    /* 
     *    Always ask the user for the filename once.
     *
     *    if (EDT_IS_NEW_DOCUMENT(context))
     */
    if (!fe_EditorDocumentIsSaved(context)) {
		return fe_EditorSaveAs(context);
    }

    return fe_editor_save_common(context, NULL);
}

/*
 *    I18N input method support.
 */
#ifdef XFE_EDITOR_IM_SUPPORT
static void
fe_editor_im_deregister(Widget widget, XtPointer closure, XtPointer cb_data)
{
	if (!fe_globalData.editor_im_input_enabled)
		return;

    XmImUnregister(widget);
}

static void
fe_editor_im_get_coords(MWContext* context, XPoint* point)
{
    fe_EditorCaretData* data = &EDITOR_CONTEXT_DATA(context)->caret_data;
    
	if (!fe_globalData.editor_im_input_enabled)
		return;

    if (data->running == FALSE && data->showing == FALSE) {
		point->x = 0;
		point->y = 16;
    } else {
		point->x = data->x + data->width - CONTEXT_DATA(context)->document_x;
		point->y = data->y - 2 - CONTEXT_DATA(context)->document_y +
			data->height;
    }
#ifdef DEBUG_rhess2
	fprintf(stderr, "im_point::[ %d, %d ]\n", point->x, point->y);
#endif
}

static void
fe_editor_im_init(MWContext* context)
{
    Widget     widget = CONTEXT_DATA(context)->drawing_area;
    Arg        args[8];
    Cardinal   n;
    XmFontList font_list;
    Pixel      bg_pixel;
    Pixel      fg_pixel;
    /* Pixmap     bg_pixmap; */
    XPoint     xmim_point;
    /* int        xmim_height; */
    
	if (!fe_globalData.editor_im_input_enabled)
#ifdef DEBUG_rhess
		{
			fprintf(stderr, "fe_editor_im_init::[ PUNT ]\n");
		}
	else
		{
			fprintf(stderr, "fe_editor_im_init::[ ]\n");
		}
#else
		return;
#endif

    XtVaSetValues(CONTEXT_WIDGET(context), XmNallowShellResize, TRUE, NULL);

    XmImRegister(widget, 0);
    XtAddCallback(widget, XmNdestroyCallback, fe_editor_im_deregister, NULL);

    /*
     *    Should these change dynamically with point?
     */
#ifdef OSF1
    font_list = (void *)fe_GetFont(context, 3, LO_FONT_FIXED);
#else
    font_list = fe_GetFont(context, 3, LO_FONT_FIXED);
#endif
    bg_pixel = CONTEXT_DATA(context)->bg_pixel;
    fg_pixel = CONTEXT_DATA(context)->fg_pixel;
    /* bg_pixmap = CONTEXT_DATA(context)->backdrop_pixmap; */

    fe_editor_im_get_coords(context, &xmim_point);

    n = 0;
    XtSetArg(args[n], XmNfontList, font_list); n++;
    XtSetArg(args[n], XmNbackground, bg_pixel); n++;
    XtSetArg(args[n], XmNforeground, fg_pixel); n++;
    /* XtSetArg(args[n], XmNbackgroundPixmap, bg_pixmap); n++; */
    XtSetArg(args[n], XmNspotLocation, &xmim_point); n++;
    /* XtSetArg(args[n], XmNlineSpace, xmim_height); n++; */
    XmImSetValues(widget, args, n);
}

static void
fe_editor_im_set_coords(MWContext* context)
{
    Widget   widget = CONTEXT_DATA(context)->drawing_area;
    Arg      args[8];
    Cardinal n;
    XPoint   xmim_point;
    /* int      xmim_height; */

	if (!fe_globalData.editor_im_input_enabled)
		return;

    fe_editor_im_get_coords(context, &xmim_point);

    n = 0;
    XtSetArg(args[n], XmNspotLocation, &xmim_point); n++;
    /* XtSetArg(args[n], XmNlineSpace, xmim_height); n++; */
    XmImSetFocusValues(widget, args, n);
}

static void
fe_editor_im_focus_in(MWContext* context)
{
	if (!fe_globalData.editor_im_input_enabled)
		return;

    fe_editor_im_set_coords(context);

    XtSetKeyboardFocus(CONTEXT_WIDGET(context),
		       CONTEXT_DATA(context)->drawing_area);
}

static void
fe_editor_im_focus_out(MWContext* context)
{
    Widget widget = CONTEXT_DATA(context)->drawing_area;

	if (!fe_globalData.editor_im_input_enabled)
		return;

    XmImUnsetFocus(widget);
}
#endif /*XFE_EDITOR_IM_SUPPORT*/

static void fe_caret_pause(MWContext* context);
static void fe_caret_unpause(MWContext* context);

void
xfe2_EditorImSetCoords(MWContext* context)
{
	fe_editor_im_set_coords(context);
}

void
xfe2_EditorCaretShow(MWContext* context)
{
	fe_caret_unpause(context);
}

void
xfe2_EditorCaretHide(MWContext* context)
{
	fe_caret_pause(context);
}

void
xfe2_EditorWaitWhileContextBusy(MWContext* context)
{
	int  n = 0;

	while (XP_IsContextBusy(context)) {
		NET_ProcessNet(0, NET_EVERYTIME_TYPE);
#ifdef MOZ_MAIL_NEWS
		MSG_OnIdle();
#endif
#ifdef DEBUG_rhess
		fprintf(stderr, "tic::[ %d ]\n", n);
#endif
		n++;
	}
}

static void
fe_change_focus_eh(Widget w, XtPointer closure, XEvent* event, Boolean* cont)
{
    MWContext* context = (MWContext *)closure;

    *cont = TRUE;
    
    if (event->type == FocusIn) 
		{
			fe_caret_unpause(context);
#ifdef XFE_EDITOR_IM_SUPPORT
			fe_editor_im_focus_in(context);
#else
			XtSetKeyboardFocus(CONTEXT_WIDGET(context),
							   CONTEXT_DATA(context)->drawing_area);

#endif /*XFE_EDITOR_IM_SUPPORT*/
		} 
    else
		{
			fe_caret_pause(context);
#ifdef XFE_EDITOR_IM_SUPPORT
			fe_editor_im_focus_out(context);
#else
			XtSetKeyboardFocus(CONTEXT_WIDGET(context), NULL);
#endif /*XFE_EDITOR_IM_SUPPORT*/
      }
}

/*
 *    Menu stuff.
 */
typedef MWContext MWEditorContext; /* place holder */

/*
 *  NOTE: look in EditorFrame.cpp for new implementation...
 */

void
xfe2_EditorInit(MWContext* context)
{
	fe_EditorCaretData* data = &EDITOR_CONTEXT_DATA(context)->caret_data;
#if 0
	/*
	 *    Just cannot get this to do what it is supposed to.
	 */
	XtAddEventHandler(CONTEXT_WIDGET(context), PropertyChangeMask,
					  FALSE, (XtEventHandler)fe_property_notify_eh,
					  context);
#endif

	if (context->type == MWContextEditor) {
		/*
		 * NOTE:  the MWContextMessageComposition has it's own ev handler...
		 *
		 */
#ifdef DEBUG_rhess
		fprintf(stderr, "tag1::[ %p ]\n", CONTEXT_WIDGET(context));
#endif
		XtAddEventHandler(CONTEXT_WIDGET(context), FocusChangeMask,
						  FALSE, (XtEventHandler)fe_change_focus_eh,
						  context);
	}

	/*
	 *    Register with input server.
	 */
#ifdef XFE_EDITOR_IM_SUPPORT
	fe_editor_im_init(context);
#endif /*XFE_EDITOR_IM_SUPPORT*/

	/*
	 *  NOTE: initialize the caret data structure... [ may be overkill ]
	 */
	data->x = 0;
	data->y = 0;
	data->width = 0;
	data->height = 0;
	data->time = 0;
	data->timer_id = 0;
	data->showing = FALSE;
	data->running = FALSE;
	data->backing_store = 0;
}

char*
fe_EditorGetTemplateURL(MWContext* context)
{
    char* template_url = fe_EditorPreferencesGetTemplate(context);
 
    if (template_url == NULL || template_url[0] == '\0') {
		char* msg = XP_GetString(XFE_EDITOR_DOCUMENT_TEMPLATE_EMPTY);

		/* The new document template location is not set. */
      	if (XFE_Confirm(context, msg)) {
			fe_EditorPreferencesDialogDo(context,
									 XFE_EDITOR_DOCUMENT_PROPERTIES_GENERAL);
		}
		return NULL;
    }

	return template_url;
}

char*
fe_EditorGetWizardURL(MWContext* context)
{
	return XFE_WIZARD_TEMPLATE_URL;
}

void
fe_EditorDisplaySource(MWContext* context)
{
    EDT_DisplaySource(context);
}

void
fe_NukeCaret(MWContext * context)
{
	fe_EditorCaretData* data = &EDITOR_CONTEXT_DATA(context)->caret_data;

#if DEBUG_rhess2
	fprintf(stderr, "fe_NukeCaret::[ ]\n");
#endif

	data->x = 0;
	data->y = 0;
	data->width = 0;
	data->height = 0;
	data->time = 0;
	data->timer_id = 0;
	data->showing = False;
	data->running = False;
	data->backing_store = 0;
}

void
fe_EditorCleanup(MWContext* context)
{
    Boolean  as_enabled;
    unsigned as_time;

	/*
	 *    If they have autosave on, try to do a save. Don't do it
	 *    for a new doc, as that'll mean dialogs, and .....
	 */
	if (!EDT_IS_NEW_DOCUMENT(context) &&
		(!EDT_IsBlocked(context) && EDT_DirtyFlag(context))) {
		fe_EditorPreferencesGetAutoSave(context, &as_enabled, &as_time);
		if (as_enabled)
			fe_EditorSave(context);
	}

    fe_NukeCaret(context);
    EDT_DestroyEditBuffer(context);
}

char*
fe_EditorMakeName(MWContext* context, char* buf, unsigned maxsize)
{
    History_entry* entry;
    char*          local_name = NULL;

    if (!EDT_IS_NEW_DOCUMENT(context)
	&&
	(entry = SHIST_GetCurrent(&context->hist)) && (entry->address)) {

	local_name = NULL;
	if (XP_ConvertUrlToLocalFile(entry->address, &local_name)) {
	    FE_CondenseURL(buf, local_name, maxsize);
	    XP_FREE(local_name);
	} else {
	    FE_CondenseURL(buf, entry->address, maxsize);
	}
    } else {
	strcpy(buf, XP_GetString(XFE_EDITOR_NEW_DOCNAME));
    }
    return buf;
}

static Boolean
fe_named_question(MWContext* context, char* name, char* message)
{
    return (Boolean)((int)fe_dialog(CONTEXT_WIDGET (context),
				    name, message,
				    TRUE, 0, TRUE, FALSE, 0));
}

Boolean
fe_save_file_check(MWContext* context, Boolean file_must_exist, Boolean autoSaveNew)
{
    int rv;
    char filename[MAXPATHLEN];
    char buf[MAXPATHLEN];
    char dialogmessages[MAXPATHLEN+64];
    Boolean ok_cancel = (file_must_exist && EDT_IS_NEW_DOCUMENT(context));

    if (ok_cancel || EDT_DirtyFlag(context)) {
	fe_EditorMakeName(context, filename, sizeof(filename));
	
	if (ok_cancel) {
	    /* You must save <filename> as a local file */
	    sprintf(buf, XP_GetString(XFE_ALERT_SAVE_CHANGES), filename);
	    rv = (fe_named_question(context, "saveNewFile", buf))?
		XmDIALOG_OK_BUTTON: XmDIALOG_CANCEL_BUTTON;
	} else {
	    /* Save changes to <filename>? */
	    sprintf(buf, XP_GetString(XFE_WARNING_SAVE_CHANGES), filename);
	    if (autoSaveNew) {
            sprintf(dialogmessages,"%s\n\n%s", buf,
                    XP_GetString(XFE_WARNING_AUTO_SAVE_NEW_MSG));
			rv = fe_YesNoCancelDialog(context, "autoSaveNew", dialogmessages);
		} else
			rv = fe_YesNoCancelDialog(context, "saveFile", buf);
	}
	
	if (rv == XmDIALOG_OK_BUTTON)
	    return fe_EditorSave(context);
	else if (rv == XmDIALOG_CANCEL_BUTTON)
	    return FALSE;
    }
    return TRUE;
}

XP_Bool
FE_CheckAndSaveDocument(MWContext* context)
{
    return fe_save_file_check(context, TRUE, FALSE);
}

XP_Bool 
FE_CheckAndAutoSaveDocument(MWContext *context)
{
    return fe_save_file_check(context, FALSE, TRUE);
}

/*
 *    NOTE: this routine is currently commented out, always returns TRUE.
 *    Call this to check if we need to force saving file
 *    Conditions are New, unsaved document and when editing a remote file
 *    Returns TRUE for all cases except CANCEL by the user in any dialog
 */
XP_Bool
FE_SaveNonLocalDocument(MWContext* context, XP_Bool save_new_document) 
{
#if 0
    History_entry* hist_entry;
    int type;
    Boolean links;
    Boolean images;
    Boolean save;
    char filebuf[MAXPATHLEN];
    char urlbuf[MAXPATHLEN];
    ED_FileError file_error;

    if (context == NULL || !EDT_IS_EDITOR(context)) {
        return TRUE;
    }
    
    hist_entry = SHIST_GetCurrent(&(context->hist));
    if (!hist_entry || !hist_entry->address)
        return TRUE;
    
    /*
     */
    type = NET_URL_Type(hist_entry->address);

    if ((type >  0 && type != FILE_TYPE_URL && type != FILE_CACHE_TYPE_URL &&
         type != MAILBOX_TYPE_URL && type != VIEW_SOURCE_TYPE_URL)
	||
	(save_new_document && EDT_IS_NEW_DOCUMENT(context)))
    {
	fe_EditorPreferencesGetLinksAndImages(context, &links, &images);
	fe_DoSaveRemoteDialog(context, &save, &links, &images);

	if (!save)
	    return FALSE;

#if 0
	if (!EDT_IS_NEW_DOCUMENT(context))
	    fe_EditorCopyrightWarningDialogDo();
#endif
	
	if (!fe_SaveAsDialog(context, filebuf, fe_FILE_TYPE_HTML))
	    return FALSE;

	/*
	 *    Grab the filename while we have it to form file: path.
	 */
	PR_snprintf(urlbuf, sizeof(urlbuf), "file:%.900s", filebuf);

	/* the c++ comments are here deliberately so that later
	 * on if we use this code we won't forget to modify it */

	/* this here is all disabled, we don't use it anymore but if later
	 * on we need to use it again, we should probably make it synchronous
	 */

	file_error = EDT_SaveFile(context,
				  hist_entry->address,
				  urlbuf,
				  TRUE,
				  images,
				  links);
	
    	if (file_error != ED_ERROR_NONE) {
	    fe_editor_report_file_error(context, file_error, urlbuf,
					FALSE);
	    return FALSE;
	}
    }
#endif
    return TRUE;
}


CB_STATIC void
fe_editor_delete_cb(Widget widget, XtPointer closure, XtPointer call_data)
{
    MWContext *context = (MWContext *)closure;

    fe_UserActivity (context);

    if (fe_WindowCount == 1) {
	fe_QuitCallback(widget, closure, call_data);
    }  else {
	if (fe_save_file_check(context, FALSE, FALSE))
	    fe_EditorDelete(context);
    }
}

void fe_editor_delete_response(Widget widget, XtPointer closure, XtPointer call_data)
{
	fe_editor_delete_cb(widget, closure, call_data);
}


Boolean
fe_EditorCheckUnsaved(MWContext* context)
{
    struct fe_MWContext_cons* rest;

    for (rest = fe_all_MWContexts; rest; rest = rest->next) {
	MWContext* context = rest->context;
	if (context->type == MWContextEditor) {
	    if (!fe_save_file_check(context, FALSE, FALSE)) {
		return FALSE;
	    }
	}
    }
    return TRUE;
}

void
fe_EditorReload(MWContext* context, Boolean super_reload)
{
    if (EDT_IS_NEW_DOCUMENT(context))
		return; /* only from action */

    if (!FE_CheckAndSaveDocument(context))
		return;

#if 0 /* this does not exist! */
    EDT_Reload(context);
#else
    if (super_reload)
		fe_ReLayout (context, NET_SUPER_RELOAD);
    else
		fe_ReLayout (context, NET_NORMAL_RELOAD);
#endif
}

void
fe_EditorFind(MWContext* context)
{
    MWContext* top_context = XP_GetNonGridContext(context);
    
    if (!top_context)
	top_context = context;

    fe_UserActivity(top_context);

    fe_FindDialog(top_context, False);
}

Boolean
fe_EditorCanFindAgain(MWContext* context)
{
    fe_FindData* find_data = CONTEXT_DATA(context)->find_data;
    
    return ((find_data != NULL)
	    &&
	    (find_data->string != NULL)
	    &&
	    (find_data->string[0] != '\0'));
}

void
fe_EditorFindAgain(MWContext* context)
{
    MWContext*   top_context = XP_GetNonGridContext(context);
    fe_FindData* find_data;

    if (!top_context)
	top_context = context;

    fe_UserActivity(top_context);

    find_data = CONTEXT_DATA(top_context)->find_data;

  if (fe_EditorCanFindAgain(top_context))
      fe_FindDialog(top_context, TRUE);
  else
      fe_Bell(context);
}

Boolean
fe_EditorCanRemoveLinks(MWContext* context)
{
    ED_ElementType type = EDT_GetCurrentElementType(context);
    Boolean        sensitive = FALSE;

	if ((type == ED_ELEMENT_TEXT ||
		 type == ED_ELEMENT_SELECTION ||
		 type == ED_ELEMENT_IMAGE)
		&&
		EDT_CanSetHREF(context)) {
		sensitive = TRUE;
    }
	return sensitive;
}

void
fe_EditorInsertLinkDialogDo(MWContext* context)
{
#if 0
    if (EDT_IS_NEW_DOCUMENT(context)) {
		if (!FE_CheckAndSaveDocument(context))
			return;
    }
#endif

    fe_EditorPropertiesDialogDo(context, XFE_EDITOR_PROPERTIES_LINK_INSERT);
}

/* -------------------------------------------------------------- */

/*
 *    Size Group.
 */

void
fe_EditorFontSizeSet(MWContext* context, ED_FontSize edt_size)
{
    if (edt_size >= EDT_FONTSIZE_MIN && edt_size <= EDT_FONTSIZE_MAX) {
#if 1
	EDT_SetFontSize(context, edt_size);
#else
	/* this does not work */
	EDT_CharacterData* edt_cdata = EDT_GetCharacterData(context);
	edt_cdata->mask = TF_FONT_SIZE;
	edt_cdata->values = TF_FONT_SIZE;
	edt_cdata->iSize = edt_size;
	EDT_SetCharacterData(context, edt_cdata);
	EDT_FreeCharacterData(edt_cdata);
#endif
    } else {
	fe_Bell(context);
    }

    fe_EditorUpdateToolbar(context, TF_FONT_SIZE);
}

ED_FontSize
fe_EditorFontSizeGet(MWContext* context)
{
    EDT_CharacterData* edt_cdata = EDT_GetCharacterData(context);
    intn               edt_size;

	if (edt_cdata != NULL) {
		edt_size = edt_cdata->iSize;
		if (edt_size < EDT_FONTSIZE_MIN)
			edt_size = ED_FONTSIZE_ZERO;
		EDT_FreeCharacterData(edt_cdata);
	} else {
		edt_size = ED_FONTSIZE_DEFAULT;
	}

    return (ED_FontSize)edt_size;
}

Widget
fe_OptionMenuSetHistory(Widget menu, unsigned index)
{
    Arg        args[4];
    Cardinal   n;
    Widget     cascade;
    Widget     popup_menu;
    WidgetList children;
    Cardinal   nchildren;

    /*
     *   Update the label, and set the position of the popup.
     */
    cascade = XmOptionButtonGadget(menu);

    /*
     *    Get the popup menu from the cascade.
     */
    n = 0;
    XtSetArg(args[n], XmNsubMenuId, &popup_menu); n++;
    XtGetValues(cascade, args, n);

    /*
     *    Get the children of the popup.
     */
    n = 0;
    XtSetArg(args[n], XmNchildren, &children); n++;
    XtSetArg(args[n], XmNnumChildren, &nchildren); n++;
    XtGetValues(popup_menu, args, n);
    if (index < nchildren) {
	/*
	 *    Finally, set the Nth button as history.
	 */
	n = 0;
	XtSetArg(args[n], XmNmenuHistory, children[index]); n++;
	/* NOTE: set it on the top level menu (strange) */
	XtSetValues(menu, args, n);
	
	return children[index];
    }
    return NULL;
}

/* -------------------------------------------------------------- */
/*
 *    Text Attribute set.
 */
void
fe_EditorCharacterPropertiesSet(MWContext* context, ED_TextFormat values)
{
    EDT_CharacterData cdata;

    memset(&cdata, 0, sizeof(EDT_CharacterData));

    cdata.mask = TF_ALL_MASK;
    if (values == TF_NONE) {   /* pop everything */
	cdata.values = 0;
    } else if (values == (TF_SERVER|TF_SCRIPT)) {
	cdata.mask = TF_SERVER|TF_SCRIPT;
	cdata.values = 0;
    } else if (values == TF_SERVER || values == TF_SCRIPT) {
	cdata.mask = TF_SERVER|TF_SCRIPT;
	cdata.values = values;
    } else {
	values &= TF_ALL_MASK; /* don't let them shoot themselves */
	cdata.values = values;
    }

    EDT_SetCharacterData(context, &cdata);

    fe_EditorUpdateToolbar(context, values);
}

ED_TextFormat
fe_EditorCharacterPropertiesGet(MWContext* context)
{
    EDT_CharacterData* edt_cdata = EDT_GetCharacterData(context);
	ED_TextFormat      values;

	if (edt_cdata != NULL) {
		values = edt_cdata->values;
		EDT_FreeCharacterData(edt_cdata);
	} else {
		values = TF_NONE;
	}
    
    return values;
}

void
fe_EditorDoPoof(MWContext* context)
{
    Boolean            clear_link = TRUE;
    EDT_CharacterData* pData;
	
    if (EDT_SelectionContainsLink(context)) {
	/*Do you want to remove the link?*/
	if (!XFE_Confirm(context,
			 XP_GetString(XFE_EDITOR_WARNING_REMOVE_LINK)))
	    clear_link = FALSE;
    }

    if (clear_link) {
	EDT_FormatCharacter(context, TF_NONE);
    } else {
	pData = EDT_GetCharacterData(context);
        if (pData) {
            pData->mask = ~TF_HREF;
            pData->values = TF_NONE;
            EDT_SetCharacterData(context, pData);
            EDT_FreeCharacterData(pData);
        }
    }
    fe_EditorUpdateToolbar(context, 0);
}

/* -------------------------------------------------------------- */
/*
 *    Horizontal Rule.
 */
void
fe_EditorInsertHorizontalRule(MWContext* context)
{
    /*
     *    Hrule is so simple, just use default values
     *   instead of bringing up properties dialog
     */
    EDT_HorizRuleData *pData = EDT_NewHorizRuleData();
    if (pData) {
        EDT_InsertHorizRule(context, pData);
	EDT_FreeHorizRuleData(pData);
    }
}

void
fe_EditorIndent(MWContext* context, Boolean is_indent)
{
    if (is_indent)
		EDT_Outdent(context);
    else
		EDT_Indent(context);
}

/* -------------------------------------------------------------- */
/*
 *    Align Set.
 */
void
fe_EditorAlignSet(MWContext* pMWContext, ED_Alignment align)
{
    ED_ElementType type = EDT_GetCurrentElementType(pMWContext);

    switch ( type ){
        case ED_ELEMENT_HRULE:
        {
            EDT_HorizRuleData* pData = EDT_GetHorizRuleData(pMWContext);
            if ( pData ){
                pData->align = align;
                EDT_SetHorizRuleData(pMWContext, pData);
            }
            break;
        }
       default: /* For Images, Text, or selection, this will do all: */
            EDT_SetParagraphAlign( pMWContext, align );
            break;
    }

    fe_EditorUpdateToolbar(pMWContext, 0);
}

ED_Alignment
fe_EditorAlignGet(MWContext* pMWContext)
{
   ED_ElementType type = EDT_GetCurrentElementType(pMWContext);
   EDT_HorizRuleData h_data;

   if (type == ED_ELEMENT_HRULE) {
       fe_EditorHorizontalRulePropertiesGet(pMWContext, &h_data);
       return h_data.align;
   } else { /* For Images, Text, or selection, this will do all: */
       return EDT_GetParagraphAlign(pMWContext);
   }
}

/* -------------------------------------------------------------- */
/*
 *    List Set.
 */
void
fe_EditorToggleList(MWContext* context, intn tag_type)
{
    EDT_ToggleList(context, tag_type);

    fe_EditorUpdateToolbar(context, 0);
}

/* -------------------------------------------------------------- */
/*
 *    Paragraph Styles Set.
 */
TagType
fe_EditorParagraphPropertiesGet(MWContext* context)
{
    return EDT_GetParagraphFormatting(context);
}

void
fe_EditorParagraphPropertiesSet(MWContext* context, TagType type)
{
#if 1
    TagType paragraph_style = EDT_GetParagraphFormatting(context);

    if (type != paragraph_style) {
	EDT_MorphContainer(context, type);

	fe_EditorUpdateToolbar(context, 0);
    }
#else
    /*
     *    This seems like the correct code, as it would mean the toolbar
     *    menu and the properties dialog have the same effect when you set
     *    a list. But this would be different from the Windows version.
     *    Do above, the same as Windows.
     */
    if (type == P_LIST_ITEM) {
        EDT_ListData list_data;

        list_data.iTagType = P_UNUM_LIST;
        list_data.eType = ED_LIST_TYPE_DISC;
        list_data.bCompact = FALSE;

        fe_EditorParagraphPropertiesSetAll(context, type, &list_data,
					   ED_ALIGN_DEFAULT);
    } else {
        fe_EditorParagraphPropertiesSetAll(context, type, NULL,
					   ED_ALIGN_DEFAULT);
    }
#endif
}

void
fe_EditorObjectPropertiesDialogDo(MWContext* context)
{
    fe_EditorPropertiesDialogType type;
    ED_ElementType e_type = EDT_GetCurrentElementType(context);

	if (e_type == ED_ELEMENT_HRULE)
		type = XFE_EDITOR_PROPERTIES_HRULE;
	else if (e_type == ED_ELEMENT_IMAGE)
		type = XFE_EDITOR_PROPERTIES_IMAGE;
	else if (e_type == ED_ELEMENT_TARGET)
		type = XFE_EDITOR_PROPERTIES_TARGET;
	else if (e_type == ED_ELEMENT_UNKNOWN_TAG)
		type = XFE_EDITOR_PROPERTIES_HTML_TAG;
	else { /* character */
		if (EDT_GetHREF(context))
			type = XFE_EDITOR_PROPERTIES_LINK;
		else
			type = XFE_EDITOR_PROPERTIES_CHARACTER;
    }

    /*
     *    Validate that we can do this kind of dialog right now.
     */
    if (fe_EditorPropertiesDialogCanDo(context, type)) {
		fe_EditorPropertiesDialogDo(context, type);
    } else {
		fe_Bell(context);
    }
}

/*
 *    Caret handling stuff. The caret is just a timed draw onto the
 *    screen, it doesn't exist in the image.
 */
#define FE_CARET_DEFAULT_TIME 500
#define FE_CARET_FLAGS_BLANK  0x1
#define FE_CARET_FLAGS_DRAW   0x2
#define FE_CARET_FLAGS_XOR    0x4
#define FE_CARET_DEFAULT_WIDTH 5

static void
fe_caret_draw(MWContext *context)
{
    fe_EditorCaretData* data = &EDITOR_CONTEXT_DATA(context)->caret_data;
    Widget    drawing_area = CONTEXT_DATA(context)->drawing_area;
    Display*  dpy = XtDisplay(drawing_area);
    Window    win = XtWindow(drawing_area);
    GC        gc;
    XGCValues gcv;
    int       x = data->x;
    int       y = data->y;
    unsigned  width = data->width;
    unsigned  height = data->height;

#ifdef DEBUG_rhess2
	fprintf(stderr, "fe_caret_draw::[ ]\n");
#endif

    memset(&gcv, ~0, sizeof(gcv));
#if 0
    {
    LO_Color  text_color;
    LO_Color  bg_color;
    fe_EditorDocumentGetColors(context, NULL, &bg_color, &text_color,
			       NULL, NULL, NULL);
    gcv.foreground = fe_GetPixel(context,
				 text_color.red,
				 text_color.green,
				 text_color.blue);
    }
#else
    gcv.foreground = CONTEXT_DATA(context)->fg_pixel;
#endif
    gc = fe_GetGC(drawing_area, GCForeground, &gcv);

    x -= CONTEXT_DATA(context)->document_x;
    y -= CONTEXT_DATA(context)->document_y;

    if ((width & 0x1) != 1)
	width++;
	
    /*
     *    Hack, hack, hack. Do something pretty david!
     */
#if 1
    XDrawLine(dpy, win, gc, x, y, x + width - 1, y);
    XDrawLine(dpy, win, gc, x + (width/2), y, x + (width/2), y + height - 1);
    XDrawLine(dpy, win, gc, x, y + height - 1, x + width - 1, y + height - 1);
#else
    XDrawRectangle(dpy, win, gc, x, y, width-1, height-1);
#endif

	XFlush(dpy);
}


static void
fe_caret_undraw(MWContext *context)
{
    fe_EditorCaretData* data = &EDITOR_CONTEXT_DATA(context)->caret_data;
    Widget    drawing_area = CONTEXT_DATA (context)->drawing_area;
    Display*  dpy = XtDisplay(drawing_area);
#ifdef DONT_rhess
    Window    win = XtWindow(drawing_area);
    GC        gc;
    XGCValues gcv;
    Visual*   visual;
    int       visual_depth;
#endif
    int       x = data->x;
    int       y = data->y;
    unsigned  width = data->width;
    unsigned  height = data->height;

#ifdef DEBUG_rhess2
	fprintf(stderr, "fe_caret_undraw::[ ]\n");
#endif
	{
		int32 ex = x;
		int32 ey = y;
		int32 ew = width + 1;
		int32 eh = height + 1;

		if (width > 0  && height > 0) {
			/*
			 *  NOTE:  don't refresh if it's a 0 pixel box...
			 */
#ifdef DEBUG_rhess2
			fprintf(stderr, "fe_caret_undraw::[ %d, %d ][ %d, %d ]\n",
					ex, ey, ew, eh);
#endif
			fe_RefreshArea(context, ex, ey, ew, eh);

			XFlush(dpy);
		}
	}
#ifdef DONT_rhess
    {
        XExposeEvent event;

		/* generate an expose */
		event.type = Expose;
		event.serial = 0;
		event.send_event = True;
		event.display = XtDisplay(CONTEXT_DATA(context)->drawing_area);
		event.window = XtWindow(CONTEXT_DATA(context)->drawing_area);
		event.x = x - CONTEXT_DATA(context)->document_x;
		event.y = y - CONTEXT_DATA(context)->document_y;
		event.width = width + 1;
		event.height = height + 1;
		event.count = 0;
		XSendEvent(event.display, event.window, 
				   False, ExposureMask, (XEvent*)&event);
    }
	XFlush(dpy);
#endif
}

static void
fe_caret_update(MWContext* context, XtIntervalId *id)
{
    fe_EditorCaretData* data = &EDITOR_CONTEXT_DATA(context)->caret_data;

#ifdef DONT_rhess
    if (data->showing) {
		fe_caret_undraw(context);
		data->showing = FALSE;
    } else {
		fe_caret_draw(context);
		data->showing = TRUE;
    }

    if (data->running) {
        data->timer_id = XtAppAddTimeOut(fe_XtAppContext,
                                         data->time,
                                         (XtTimerCallbackProc)fe_caret_update,
                                         context);
    }
#else
    if (data->showing) {
		fe_caret_draw(context);
    } else {
		fe_caret_undraw(context);
    }

    data->timer_id = 0;
#endif
}

static void
fe_caret_cancel(MWContext* context)
{
    fe_EditorCaretData* data = &EDITOR_CONTEXT_DATA(context)->caret_data;
  
#ifdef DEBUG_rhess2
	fprintf(stderr, "fe_caret_cancel::[ ]\n");
#endif

#ifdef DONT_rhess
    if (data->running)
		XtRemoveTimeOut(data->timer_id);

    data->running = FALSE;

    if (data->showing)
		fe_caret_undraw(context);
    data->showing = FALSE;
    
    if (data->backing_store) {
		XFreePixmap(XtDisplay(CONTEXT_DATA(context)->drawing_area),
					data->backing_store);
		data->backing_store = 0;
    }
#else
	data->running = False;
	if (data->showing) {
		data->showing = False;
		fe_caret_update(context, NULL);
	}
#endif
}

static void
fe_caret_pause(MWContext* context)
{
    fe_EditorCaretData* data = &EDITOR_CONTEXT_DATA(context)->caret_data;

#ifdef DEBUG_rhess2
	fprintf(stderr, "fe_caret_pause::[ ]\n");
#endif

    if (!data->running) {
		/* already paused or never started */
        return;
	}
#ifdef DONT_rhess
	else {
		XtRemoveTimeOut(data->timer_id);
	}

    data->running = FALSE;

    if (data->showing == FALSE) /* always pause showing */
        fe_caret_update(context, NULL);
#else
	data->running = False;
#ifdef HOT_CARET_rhess
	if (!data->showing) {
		data->showing = True;
#else
	if (data->showing) {
		data->showing = False;
#endif
		fe_caret_update(context, NULL);
	}
#endif /* DONT_rhess */
}

static void
fe_caret_unpause(MWContext* context)
{
    fe_EditorCaretData* data = &EDITOR_CONTEXT_DATA(context)->caret_data;

#ifdef DEBUG_rhess2
	fprintf(stderr, "fe_caret_unpause::[ ]\n");
#endif

#ifdef DONT_rhess
    if (data->running == TRUE || data->showing == FALSE) /* not paused */
        return;

    data->showing = FALSE;
    data->running = TRUE;

    /*
     *    Draw and set timer
     */
    fe_caret_update(context, NULL);
#else
    if ( data->running || data->showing ) /* not paused */
        return;

	data->running = True;

	if (!data->showing) {
		data->showing = True;
		fe_caret_update(context, NULL);
	}
#endif
}

static void
fe_caret_begin(MWContext* context)
{
    fe_EditorCaretData* data = &EDITOR_CONTEXT_DATA(context)->caret_data;

#ifdef DEBUG_rhess2
	fprintf(stderr, "fe_caret_begin::[ ]\n");
#endif

#ifdef DONT_rhess
    data->showing = FALSE;
    data->time = FE_CARET_DEFAULT_TIME;
    data->running = TRUE;

    fe_caret_update(context, NULL);
#else
	data->time    = 0;
    data->running = True;
	if (!data->showing) {
		data->showing = True;
		fe_caret_update(context, NULL);
	}
#endif

#ifdef XFE_EDITOR_IM_SUPPORT
    fe_editor_im_set_coords(context);
#endif /* XFE_EDITOR_IM_SUPPORT */
}

void
fe_EditorRefreshArea(MWContext* context, int x, int y, unsigned w, unsigned h)
{
    fe_EditorCaretData* data = &EDITOR_CONTEXT_DATA(context)->caret_data;

    if (
#if 0
		/*
		 *    Don't use x, as LO_RefreshArea tends to redraw full width
		 *    anyway.
		 */
		((x < data->x && x + w >= data->x) ||
		 (x < data->x + data->width && x + w >= data->x + data->width))
		&&
#endif
		((y < data->y && y + h >= data->y) ||
		 (y < data->y + data->height && y + h >= data->y + data->height))) {

#ifdef DONT_rhess
        if (data->backing_store) {
            XFreePixmap(XtDisplay(CONTEXT_DATA(context)->drawing_area),
                        data->backing_store);
            data->backing_store = 0;
        }

        if (data->running) {
            XtRemoveTimeOut(data->timer_id);
            data->showing = TRUE; /* force restart of sequence next */
        }
        if (data->showing) {
            data->showing = FALSE;
            fe_caret_update(context, NULL);
        }
#else
        if (data->showing) {
            fe_caret_update(context, NULL);
        }
#endif
    }
}

static int fe_LastCoffeeScroll_X = 0;
static int fe_LastCoffeeScroll_Y = 0;

static void
fe_editor_keep_cursor_visible(MWContext* context)
{
    fe_EditorCaretData* c_data = &EDITOR_CONTEXT_DATA(context)->caret_data;
    int x = CONTEXT_DATA(context)->document_x;
    int y = CONTEXT_DATA(context)->document_y;
    unsigned win_height = CONTEXT_DATA(context)->scrolled_height;
    unsigned win_width = CONTEXT_DATA(context)->scrolled_width;
    Boolean  coffee_scroll = FALSE;
	int fudge = 20;
	int pad = 10;

#ifdef DEBUG_rhess2
	fprintf(stderr, "fe_editor_keep_cursor_visible::[ ]\n");
#endif

    if (c_data->y + c_data->height > y + win_height - fudge) {
		y = c_data->y + c_data->height - win_height + fudge; /* fudge */
		if (y + win_height > CONTEXT_DATA(context)->document_height)
			y = CONTEXT_DATA(context)->document_height - win_height;
		coffee_scroll = TRUE;
    } else if (c_data->y < y + pad) {
		y = c_data->y - pad;
		if (y < 0)
			y = 0;
		coffee_scroll = TRUE;
    }

	if (c_data->x + c_data->width > x + win_width - fudge) {
		x = c_data->x + c_data->width - win_width + fudge;
		if (x + win_width > CONTEXT_DATA(context)->document_width)
			x = CONTEXT_DATA(context)->document_width - win_width;
		coffee_scroll = TRUE;
    } else if (c_data->x < x + pad) {
		x = c_data->x - pad;
		if (x < 0)
			x = 0;
		coffee_scroll = TRUE;
    }

	if (x == fe_LastCoffeeScroll_X && y == fe_LastCoffeeScroll_Y) {
		coffee_scroll = False;
	}

    if (coffee_scroll) {
		if (c_data->showing)
			fe_caret_cancel(context);
#ifdef DEBUG_rhess2
		fprintf(stderr, "coffee_scroll::[ %d, %d ][ %d, %d ][ %d, %d ]\n", 
				x, y, c_data->x, c_data->y, 
				CONTEXT_DATA(context)->document_x,
				CONTEXT_DATA(context)->document_y );
#endif
        /*
		 *    Collect all pending exposures before we scroll.
		 */
		fe_SyncExposures(context);

		fe_ScrollTo(context, x, y);
    }

	fe_LastCoffeeScroll_X = x;
	fe_LastCoffeeScroll_Y = y;
}

static void
fe_editor_keep_pointer_visible_autoscroll(XtPointer, XtIntervalId*);

static void
fe_editor_keep_pointer_visible(MWContext* context, int p_x, int p_y)
{
    fe_ContextData* data = CONTEXT_DATA(context);
    fe_EditorAscrollData* ascroll_data
	= &EDITOR_CONTEXT_DATA(context)->ascroll_data;
    Boolean coffee_scroll = FALSE;
    int x = data->document_x;
    int y = data->document_y;
    int delta = 0;

#ifdef DEBUG_rhess2
	fprintf(stderr, "fe_editor_keep_pointer_visible::[ ]\n");
#endif

    if (ascroll_data->timer_id)
      XtRemoveTimeOut(ascroll_data->timer_id);

    ascroll_data->delta_x = 0;
    ascroll_data->delta_y = 0;

    if (p_y < data->document_y) {
	coffee_scroll = TRUE;
	y = p_y;
	delta = (data->document_y - p_y);
	ascroll_data->y = p_y;
	ascroll_data->delta_y = -delta;
    }

    if (p_y > data->document_y + data->scrolled_height) {
	coffee_scroll = TRUE;
	y = p_y - data->scrolled_height;
	delta = p_y - (data->document_y + data->scrolled_height);
	ascroll_data->y = p_y;
	ascroll_data->delta_y = delta;
    }

    if (coffee_scroll) {
        /*
		 *    Collect all pending exposures before we scroll.
		 */
		fe_SyncExposures(context);

		fe_ScrollTo(context, x, y);

		ascroll_data->timer_id = 
			XtAppAddTimeOut(fe_XtAppContext,
							(100),
							fe_editor_keep_pointer_visible_autoscroll,
							(XtPointer)context);
    }
}

static void
fe_editor_keep_pointer_visible_autoscroll(XtPointer closure, XtIntervalId* id)
{
    MWContext* context = (MWContext*)closure;
    fe_EditorAscrollData* ascroll_data;

#if DEBUG_rhess2
	fprintf (stderr,"fe_editor_keep_pointer_visible_autoscroll::[ ]\n");
#endif

    ascroll_data = &EDITOR_CONTEXT_DATA(context)->ascroll_data;
    ascroll_data->timer_id = 0; /* because we won't pop again */
  
    ascroll_data->x += ascroll_data->delta_x;
    ascroll_data->y += ascroll_data->delta_y;

    EDT_ExtendSelection(context, ascroll_data->x, ascroll_data->y);

    fe_editor_keep_pointer_visible(context, ascroll_data->x, ascroll_data->y);
}

static void
fe_caret_set(MWContext* context, int x, int y, unsigned w, unsigned h)
{
    fe_EditorCaretData* data = &EDITOR_CONTEXT_DATA(context)->caret_data;

#ifdef DEBUG_rhess2
	fprintf(stderr, "fe_caret_set::[ ]\n");
#endif

    if ( x != data->x || 
		 y != data->y || 
		 w != data->width || 
		 h != data->height )
		{
			if (data->showing)
				fe_caret_cancel(context);
		}

#ifdef DONT_rhess
    if (data->running)
	XtRemoveTimeOut(data->timer_id);
#endif

    data->x = x;
    data->y = y;
    data->width = w;
    data->height = h;

    fe_editor_keep_cursor_visible(context);
}

Boolean 
FE_GetCaretPosition(
		    MWContext *context,
		    LO_Position* where,
		    int32* caretX, int32* caretYLow, int32* caretYHigh
) {
  int32 xVal;
  int32 yVal;
  int32 yValHigh;

#if DEBUG_rhess2
  fprintf(real_stderr, "FE_GetCaretPosition::[ ]\n");
#endif

  if (!context || !where->element)
    return FALSE;
  
  xVal = where->element->lo_any.x + where->element->lo_any.x_offset;
  yVal = where->element->lo_any.y;
  yValHigh = yVal + where->element->lo_any.height;

  switch (where->element->type) {
  case LO_TEXT: {
    LO_TextStruct* text_data = & where->element->lo_text;
    LO_TextInfo    text_info;
    int            len_save = text_data->text_len;

    if (!text_data->text_attr)
      return FALSE;

    if (where->position <= text_data->text_len) {
      text_data->text_len = where->position;
    }
    XFE_GetTextInfo(context, text_data, &text_info);
    text_data->text_len = len_save;
    
    xVal += text_info.max_width - 1;
  } break;

  case LO_IMAGE: {
    LO_ImageStruct *pLoImage = & where->element->lo_image;
    if (where->position == 0) {
      xVal -= 1;
    } else {
      xVal += pLoImage->width + 2 * pLoImage->border_width;
    }
  } break;
    
  default: {
    LO_Any *any = &where->element->lo_any;
    if (where->position == 0) {
      xVal -= 1;
    } else {
      xVal += any->width;
    }
  }
  }

  *caretX = xVal;
  *caretYLow = yVal;
  *caretYHigh = yValHigh;

  return TRUE;
}

/*
 *    char_offset is in characters!!!
 */
PUBLIC void
FE_DisplayTextCaret(MWContext* context, int iLocation, LO_TextStruct* text,
		    int char_offset)
{
  int       x;
  int       y;
  unsigned  width;
  unsigned  height;
  LO_TextInfo text_info;
  int16     save_len;

#if DEBUG_rhess2
  fprintf (stderr,"FE_DisplayTextCaret::[ ]\n");
#endif

  /*
   *    Get info extent info about the first <char_offset> characters of
   *    text.
   */
  save_len = text->text_len;
  text->text_len = char_offset;
  XFE_GetTextInfo(context, text, &text_info);
  x = text->x + text->x_offset + text_info.max_width;
  y = text->y + text->y_offset;
  height =  text_info.ascent + text_info.descent;
  
  text->text_len = save_len;

  width = FE_CARET_DEFAULT_WIDTH;
  x -= (FE_CARET_DEFAULT_WIDTH/2) + 1; /* middle of char and back */

  fe_caret_set(context, x, y, width, height);
  fe_caret_begin(context);
}

void 
FE_DisplayImageCaret(
		     MWContext*             context, 
		     LO_ImageStruct*        image,
		     ED_CaretObjectPosition caretPos)
{
  int       x;
  int       y;
  unsigned  width;
  unsigned  height;
  
#if DEBUG_rhess2
  fprintf (stderr,"FE_DisplayImageCaret::[ ]\n");
#endif

  /*
   *    Get info extent info about the first <char_offset> characters of
   *    text.
   */
  x = image->x + image->x_offset;
  y = image->y + image->y_offset;
  width = FE_CARET_DEFAULT_WIDTH;
  height = image->height + (2 * image->border_width);

  if (caretPos == ED_CARET_BEFORE) {
    x -= 1;
  } else if (caretPos == ED_CARET_AFTER) {
    x += image->width + (2 * image->border_width);
  } else {
    width = image->width + (2 * image->border_width);
  }

  fe_caret_set(context, x, y, width, height);
  fe_caret_begin(context);
}

void FE_DisplayGenericCaret(MWContext * context, LO_Any * image,
                        ED_CaretObjectPosition caretPos )
{
  int       x;
  int       y;
  unsigned  width;
  unsigned  height;
  
#if DEBUG_rhess2
  fprintf (stderr,"FE_DisplayGenericCaret::[ ]\n");
#endif

  /*
   *    Get info extent info about the first <char_offset> characters of
   *    text.
   */
  x = image->x + image->x_offset;
  y = image->y + image->y_offset;
  width = FE_CARET_DEFAULT_WIDTH;
  height = image->line_height;

  if (caretPos == ED_CARET_BEFORE) {
    x -= 1;
  } else if (caretPos == ED_CARET_AFTER) {
    x += image->width;
  } else {
    width = image->width;
  }

  fe_caret_set(context, x, y, width, height);
  fe_caret_begin(context);
}

PUBLIC void
FE_DestroyCaret(MWContext * context)
{
	fe_EditorCaretData* data = &EDITOR_CONTEXT_DATA(context)->caret_data;

#if DEBUG_rhess2
	fprintf(stderr, "FE_DestroyCaret::[ ]\n");
#endif

	if (data->showing)
		fe_caret_cancel(context);

	data->time = 0;
	data->timer_id = 0;
	data->showing = False;
	data->running = False;
	data->backing_store = 0;
}

PUBLIC void
FE_ShowCaret(MWContext * context)
{
#if DEBUG_rhess2
	fprintf(stderr, "FE_ShowCaret::[ ]\n");
#endif
	fe_caret_begin(context);
}

/*
 *    Note all x, y co-ordinates are document relative.
 */
PUBLIC void 
FE_DocumentChanged(MWContext* context, int32 p_y, int32 p_height)
{
    int32  redraw_x = 0;
    int32  redraw_y;
    uint32 redraw_width = CONTEXT_DATA(context)->document_width;
    uint32 redraw_height;
    int32  win_y;
    uint32 win_height;
    int32  win_x;
    int32  margin_height;
    int32  margin_width;

    win_y = CONTEXT_DATA(context)->document_y;
    win_height = CONTEXT_DATA(context)->scrolled_height;

    win_x = CONTEXT_DATA(context)->document_x;

    /*
     *    Ok, it seems like layout doesn't take into account margins.
     *    If there was stuff in the margin before (because we *were*
     *    displaying something big, which was scrolled up), but now we
     *    are not, we must clear the margin. This code attempts
     *    to detect when we are close to the margin, and say, clear
     *    the whole thing. This seems kinda bogus, the back-end
     *    should know about margins (no?), but... djw
     */
    fe_GetMargin(context, &margin_width, &margin_height);
    
    if (p_y <= margin_height) {
        p_y = 0;
		if (p_height != -1)
			p_height += margin_height;
    }

    redraw_y = p_y;

    if (p_height < 0) {
		redraw_height = CONTEXT_DATA(context)->document_height;
		/* make sure full repaint goes to end of window at least. */
		if (win_height > redraw_height)
			redraw_height = win_height;
    } else {
		redraw_height = p_height;
    }

    /*
     *    Is doc change area above or below displayed region?
     */
    if (redraw_y > win_y + win_height || redraw_y + redraw_height < win_y)
		return; /* nothing to do */

    /*
     *    Clip redraw area.
     */
    if (redraw_y < win_y) {
		redraw_height -= (win_y - redraw_y);
		redraw_y = win_y;
    }

    if (redraw_y + redraw_height > win_y + win_height) {
		redraw_height = win_y + win_height - redraw_y;
    }

    /*FIXME*/
    /*
     *    Probably need to call CL_RefreshRegion()/Rect()
     */
#ifdef DONT_rhess
    fe_ClearArea(context, redraw_x - win_x, redraw_y - win_y,
				 redraw_width, redraw_height);
#endif
	fe_caret_cancel(context);

	{
		int32 ex = redraw_x;
		int32 ey = redraw_y;
		int32 ew = redraw_width;
		int32 eh = redraw_height;

#ifdef DEBUG_rhess2
		fprintf(stderr, "FE_DocumentChanged::[ %d, %d ][ %d, %d ]\n",
				ex, ey, ew, eh);
#endif
		fe_RefreshArea(context, ex, ey, ew, eh);
	}
#ifdef DONT_rhess
    {
        XExposeEvent event;

		/* generate an expose */
		event.type = Expose;
		event.serial = 0;
		event.send_event = True;
		event.display = XtDisplay(CONTEXT_DATA(context)->drawing_area);
		event.window = XtWindow(CONTEXT_DATA(context)->drawing_area);
		event.x = redraw_x - win_x;
		event.y = redraw_y - win_y;
		event.width = redraw_width;
		event.height = redraw_height;
		event.count = 0;
		XSendEvent(event.display, event.window, False, ExposureMask, (XEvent*)&event);
    }
#endif
}

void FE_GetDocAndWindowPosition(MWContext * context, int32 *pX, int32 *pY, 
    int32 *pWidth, int32 *pHeight )
{
  *pX = CONTEXT_DATA (context)->document_x;
  *pY = CONTEXT_DATA (context)->document_y;
  *pWidth = CONTEXT_DATA (context)->scrolled_width;
  *pHeight = CONTEXT_DATA (context)->scrolled_height;
}

void
FE_SetNewDocumentProperties(MWContext* context)
{

    LO_Color bg_color;
    LO_Color normal_color;
    LO_Color link_color;
    LO_Color active_color;
    LO_Color followed_color;
    char background_image[MAXPATHLEN];
    XP_Bool use_custom;
    Boolean keep_images;

    /*
     *    Set keep images state from prefs. 
     */
    fe_EditorPreferencesGetLinksAndImages(context, NULL, &keep_images);
    fe_EditorDocumentSetImagesWithDocument(context, keep_images);

    /*
     *    Get editor defaults.
     */
    use_custom = fe_EditorPreferencesGetColors(context,
											   background_image,
											   &bg_color,
											   &normal_color,
											   &link_color,
											   &active_color,
											   &followed_color);

    /*
     *    Apply.
     */
    if (use_custom) {
		fe_EditorDocumentSetColors(context,
								   background_image,
								   FALSE,
								   &bg_color,
								   &normal_color,
								   &link_color,
								   &active_color,
								   &followed_color);
    } else {
		fe_EditorDocumentSetColors(context,
								   background_image, /*independent of colors*/
								   FALSE,
								   NULL,
								   NULL,
								   NULL,
								   NULL,
								   NULL);
    }

    /*
     *    Now do other stuff.
     */
    /* Title */
    fe_EditorDocumentSetTitle(context, NULL); /* will show Untitled */

    /* Add fixed MetaData items for author, others?? */
    fe_EditorDocumentSetMetaData(context,
								 "Author",
								 fe_EditorPreferencesGetAuthor(context));
    fe_EditorDocumentSetMetaData(context,
								 "GENERATOR",
								 "Mozilla/X");
}

void
fe_EditorNotifyBackgroundChanged(MWContext* context)
{
#ifdef DONT_rhess
    fe_EditorCaretData* data = &EDITOR_CONTEXT_DATA(context)->caret_data;
    
    if (data->backing_store) {
		XFreePixmap(XtDisplay(CONTEXT_DATA(context)->drawing_area),
					data->backing_store);
		data->backing_store = 0;
    }
#endif
}

char* 
FE_URLToLocalName(char *name)
{
  char* qm;
  char* hm;
  char* begin;
  char* end;
  char* rv;
  char* nameSave;
  int   len;
  int   nameLen;

  /*
   *    Save a copy of the name as we might drop characters after '?' or '#'
   */

  nameLen = strlen(name);
  nameSave = XP_ALLOC(nameLen + 1);
  memcpy(nameSave, name, nameLen);
  nameSave[nameLen] = '\0';

  /*
   *    Drop everything after a '?' or '#' character.
   */
  qm = strchr(nameSave, '?');
  hm = strchr(nameSave, '#');

  if (qm && hm) {
    end = (qm > hm)? qm: hm;
  } else if (qm) {
    end = qm;
  } else if (hm) {
    end = hm;
  } else {
    end = nameSave + nameLen;
  }

  *end = '\0';

  /*
   *    Then get the basename of what's left.
   */
  if ((begin = strrchr(nameSave, '/')) != NULL && begin[1] != '\0') {
    begin++;
  } else {
    begin = nameSave;
  }
    
  len = end - begin;
  rv = malloc(len + 1);
  memcpy(rv, begin, len);
  rv[len] = '\0';

  XP_FREE(nameSave);
  return rv;
}

Boolean FE_EditorPrefConvertFileCaseOnWrite(void)
{
  fprintf (real_stderr,"FE_EditorPrefConvertFileCaseOnWrite\n");
  return TRUE;
}

void 
FE_FinishedSave(MWContext* context, int status,
		char *pDestURL, int iFileNumber)
{
}

#if 0
static void
fe_editor_copyright_hint_dialog(MWContext* context)
{
    if (!fe_globalPrefs.editor_copyright_hint)
	return;

    if (fe_HintDialog(context, XP_GetString(XFE_ERROR_COPYRIGHT_HINT))) {
	fe_globalPrefs.editor_copyright_hint = FALSE;
	
	/*
	 *    Save options.
	 */
	if (!XFE_SavePrefs((char *)fe_globalData.user_prefs_file,
			   &fe_globalPrefs)) {
	    fe_perror(context, XP_GetString(XFE_ERROR_SAVING_OPTIONS));
	}
    }
}
#endif

/*
 *    Crock timeout callback that is used by fe_EditorGetUrlExitRoutine() to
 *    kill a failed Frame (you opened a non-html doc say).
 */
static void
fe_editor_exit_timeout(XtPointer closure, XtIntervalId* id)
{
    MWContext *context = (MWContext *)closure;

	fe_EditorKill(context);
}

void
fe_EditorGetUrlExitRoutine(MWContext* context, URL_Struct* url, int status)
{
    if (status < 0) {
		/*
		 *    Make sure that it's not a publish. This code is only
		 *    meant for fe_GetURL() calls. Make sure the user gets to see
		 *    the error dialog, then request a delete on the editor.
		 */
		if (url != NULL && url->files_to_post == NULL) { /* not publish */

			/*
			 *    Wait for popup menus to go away.
			 */
			/* we need to check context->type because under certain cases we 
			 * reach this loop after the editor has been killed so context is
			 * garbage and if we don't check then ContextHasPopups fail
			 */
			while (context && context->type == MWContextEditor && 
				   fe_ContextHasPopups(context))
				fe_EventLoop();
	    
			/*
			 *    Then request a dissapearing act.
			 */
			XtAppAddTimeOut(fe_XtAppContext, 0, fe_editor_exit_timeout,
							(XtPointer)context);
		}
    }
}


/*
 * Editor calls us when we are finished loading
 * We force saving document if not editing local file
 */
void
FE_EditorDocumentLoaded(MWContext* context)
{
    History_entry* hist_ent;
    Bool           do_save_dialog;
    unsigned as_time;
    Boolean  as_enable;
    
    if (!context) {
		return;
    }
    
#ifdef DEBUG_rhess2
	fprintf(stderr, "editor::[ FE_EditorDocumentLoaded ]\n");
#endif

    /*
     *    Windows does a lot more gymnastics here to determine which
     *    docs should or should not be saved (especially new ones).
     *    Please look into this and match up.
     */
    /*FIXME*/
    do_save_dialog = FALSE;
    if ((hist_ent = SHIST_GetCurrent(&context->hist)) && hist_ent->address) {
		do_save_dialog = !NET_IsLocalFileURL(hist_ent->address);
    }
    
#if 0
	/*
	 *    Seems we don't do this anymore.
	 */
    if (!EDT_IS_NEW_DOCUMENT(context) && do_save_dialog) {
		
		/*
		 *    Tongue in cheek....
		 */
		fe_editor_copyright_hint_dialog(context);
    }
#endif
	
#ifdef MOZ_MAIL_NEWS
	if (context->type == MWContextMessageComposition) {
		fe_MailComposeDocumentLoaded(context);
	}
	else 
#endif
		xfe2_EditorInit(context);
	
    /*
     *    Set autosave period.
     */
    fe_EditorPreferencesGetAutoSave(context, &as_enable, &as_time);
    EDT_SetAutoSavePeriod(context, as_time);
	
    fe_HackEditorNotifyToolbarUpdated(context);
}

static char fe_SaveDialogSaveName[] = "saveMessageDialog";
static char fe_SaveDialogUploadName[] = "uploadMessageDialog";
static char fe_SaveDialogPrepareName[] = "prepareMessageDialog";
static char fe_SaveDialogImageLoadName[] = "imageLoadMessageDialog";

typedef enum fe_SaveDialogType
{
    XFE_SAVE_DIALOG_SAVE,
    XFE_SAVE_DIALOG_UPLOAD,
    XFE_SAVE_DIALOG_PREPARE,
    XFE_SAVE_DIALOG_IMAGELOAD
} fe_SaveDialogType;

typedef struct fe_SaveDialogInfo
{
    unsigned          nfiles;
    unsigned          current;
    fe_SaveDialogType type;
} fe_SaveDialogInfo;

static void
fe_save_dialog_info_init(Widget msg_box, fe_SaveDialogType type, unsigned n)
{
    fe_SaveDialogInfo* info = (fe_SaveDialogInfo*)XtNew(fe_SaveDialogInfo);
    
    if (info == NULL)
	return;

    info->nfiles = n;
    info->type = type;
    info->current = 0;

    XtVaSetValues(msg_box, XmNuserData, (XtPointer)info, 0);
}

#define XFE_SAVE_DIALOG_NEXT (-1)

static void
fe_save_dialog_info_set(Widget msg_box, char* filename, int index)
{
    XtPointer          user_data;
    fe_SaveDialogInfo* info;
    char               buf[MAXPATHLEN];
    int                id;
    char*              string;
    XmString           xm_string;

    XtVaGetValues(msg_box, XmNuserData, &user_data, 0);
    if (user_data != 0) {
	info = (fe_SaveDialogInfo*)user_data;

	if (index == XFE_SAVE_DIALOG_NEXT)
	    info->current++;
	else
	    info->current = index;

	if (info->current > info->nfiles)
	    info->nfiles = info->current;

	switch (info->type) {
	case XFE_SAVE_DIALOG_IMAGELOAD:
	    id = XFE_LOADING_IMAGE_FILE;
		break;
	case XFE_SAVE_DIALOG_UPLOAD:
	    id = XFE_UPLOADING_FILE;
		break;
	case XFE_SAVE_DIALOG_PREPARE:
	    id = XFE_PREPARE_UPLOAD;
		break;
	default:
	    id = XFE_SAVING_FILE;
		break;
	}

	string = XP_GetString(id);

	if (!filename)
	    filename = "";
	sprintf(buf, string, filename);
	
	if (info->nfiles > 1) {
	    strcat(buf, "\n");
	    string = XP_GetString(XFE_FILE_N_OF_N);
	    sprintf(&buf[strlen(buf)], string, info->current, info->nfiles);
	}
	
	xm_string = XmStringCreateLocalized(buf);
    } else {
	xm_string = XmStringCreateLocalized(filename);
    }
    XtVaSetValues(msg_box, XmNmessageString, xm_string, 0);
    XmStringFree(xm_string);
}

static fe_SaveDialogType
fe_SaveDialogGetType(Widget msg_box)
{
    char* name = XtName(msg_box);

    if (strcmp(name, fe_SaveDialogSaveName) == 0)
		return XFE_SAVE_DIALOG_SAVE;
    else if (strcmp(name, fe_SaveDialogUploadName) == 0)
		return XFE_SAVE_DIALOG_UPLOAD;
    else if (strcmp(name, fe_SaveDialogPrepareName) == 0)
		return XFE_SAVE_DIALOG_PREPARE;
    else
		return XFE_SAVE_DIALOG_IMAGELOAD;
}

static void
fe_save_dialog_cancel_cb(Widget msg_box, XtPointer closure, XtPointer cb_data)
{
    MWContext* context = (MWContext*)closure;

    if (fe_SaveDialogGetType(msg_box) == XFE_SAVE_DIALOG_SAVE) {
        EDT_SaveCancel(context);
    } else {
        NET_InterruptWindow(context);
    }
}

static void
fe_save_dialog_destroy_cb(Widget msg_box, XtPointer closure, XtPointer cb_data)
{
    MWContext* context = (MWContext*)closure;
    XtPointer  user_data;
    
    if (CONTEXT_DATA(context)->posted_msg_box == msg_box)
		CONTEXT_DATA(context)->posted_msg_box = NULL;

    XtVaGetValues(msg_box, XmNuserData, &user_data, 0);
    if (user_data != NULL)
		XtFree(user_data);
}

static Widget
fe_save_dialog_create(MWContext* context, char* name)
{
    Widget mainw = CONTEXT_WIDGET(context);
    Widget msg_box;
    Visual *v = 0;
    Colormap cmap = 0;
    Cardinal depth = 0;
    Arg args [20];
    Cardinal n;

    if (CONTEXT_DATA(context)->posted_msg_box != NULL)
	return CONTEXT_DATA(context)->posted_msg_box;

    XtVaGetValues(mainw, XtNvisual, &v, XtNcolormap, &cmap,
		  XtNdepth, &depth, 0);

    n = 0;
    XtSetArg(args[n], XmNvisual, v); n++;
    XtSetArg(args[n], XmNdepth, depth); n++;
    XtSetArg(args[n], XmNcolormap, cmap); n++;
    XtSetArg(args[n], XmNallowShellResize, TRUE); n++;
    XtSetArg(args[n], XmNtransientFor, mainw); n++;
    XtSetArg(args[n], XmNdialogStyle, XmDIALOG_FULL_APPLICATION_MODAL); n++;
    XtSetArg(args[n], XmNdialogType, XmDIALOG_MESSAGE); n++;
    XtSetArg(args[n], XmNdeleteResponse, XmDO_NOTHING); n++;
    XtSetArg(args[n], XmNautoUnmanage, FALSE); n++;
    XtSetArg(args[n], XmNresizePolicy, XmRESIZE_GROW); n++;

    msg_box = XmCreateMessageDialog(mainw, name, args, n);

    fe_UnmanageChild_safe(XmMessageBoxGetChild(msg_box, XmDIALOG_OK_BUTTON));
    fe_UnmanageChild_safe(XmMessageBoxGetChild(msg_box, XmDIALOG_HELP_BUTTON));

    XtAddCallback(msg_box, XmNcancelCallback, fe_save_dialog_cancel_cb,
		  (XtPointer)context);
    XtAddCallback(msg_box, XmNdestroyCallback, fe_save_dialog_destroy_cb,
		  (XtPointer)context);

    CONTEXT_DATA(context)->posted_msg_box = msg_box;

    return msg_box;
}

static void
fe_destroy_parent_cb(Widget widget, XtPointer closure, XtPointer cbd)
{
	XtDestroyWidget(XtParent(widget));
}

static void 
fe_save_dialog_destroy(Widget msg_box)
{
	Widget   shell = XtParent(msg_box);
	Cardinal num_popups = XfeNumPopups(shell);
	Widget*  popups = XfePopupList(shell);

	/*
	 *    If we have a stacked popup on us, them wait for that
	 *    popup to destroy before we destroy ourselves. Do this by
	 *    setting a destroy callback on the popup that destroys
	 *    it's parent (us). Amongst other things we need to do this
	 *    because server side error messages get stacked on the save
	 *    progress dialog.
	 */
	if (num_popups > 0) { /* anyone who adds to me after this is a loser */
		XtAddCallback(popups[0], XmNdestroyCallback, fe_destroy_parent_cb, 0);
	} else {
		XtDestroyWidget(msg_box);
	}
}

/*
 *    Dialog to give feedback and allow canceling, overwrite protection
 *    when downloading remote files 
 *
 *    put up a save dialog that says "i'm saving foo right now", cancel?
 *    and return right away. 
 */
void 
FE_SaveDialogCreate(MWContext* context, int nfiles, ED_SaveDialogType saveType)
{
    Widget msg_box;
    char*  name;
    fe_SaveDialogType type;

    if (CONTEXT_DATA(context)->posted_msg_box != NULL)
		return;

	switch (saveType) {
	case ED_SAVE_DLG_PUBLISH:
		name = fe_SaveDialogUploadName;
		type = XFE_SAVE_DIALOG_UPLOAD;
		break;
	case ED_SAVE_DLG_PREPARE_PUBLISH:
		name = fe_SaveDialogPrepareName;
		type = XFE_SAVE_DIALOG_PREPARE;
		break;
	default:
		name = fe_SaveDialogSaveName;
		type = XFE_SAVE_DIALOG_SAVE;
		break;
	}

    msg_box = fe_save_dialog_create(context, name);

    fe_save_dialog_info_init(msg_box, type, nfiles);

    CONTEXT_DATA(context)->posted_msg_box = msg_box;

	/* this is here so that we know when the file saying is over */
    CONTEXT_DATA(context)->is_file_saving = True;

    /*
     *    Postpone until we have something to say..
    XtManageChild(msg_box);
     */
}

void 
FE_SaveDialogSetFilename(MWContext* context, char* filename) 
{
    Widget msg_box = CONTEXT_DATA(context)->posted_msg_box;
    
    if (!msg_box)
		return;
    
    fe_save_dialog_info_set(msg_box, filename, XFE_SAVE_DIALOG_NEXT);
    
    if (!XtIsManaged(msg_box))
		XtManageChild(msg_box);

	XfeUpdateDisplay(msg_box);
}

void 
FE_SaveDialogDestroy(MWContext* context, int status, char* file_url)
{
    Widget msg_box = CONTEXT_DATA(context)->posted_msg_box;
	fe_SaveDialogType type;
    
    if (msg_box == NULL)
		return;

	type = fe_SaveDialogGetType(msg_box);

    if (type == XFE_SAVE_DIALOG_SAVE) {
		CONTEXT_DATA(context)->save_file_url = file_url;
		CONTEXT_DATA(context)->file_save_status = status;
    }
    
    fe_save_dialog_destroy(msg_box);
    CONTEXT_DATA(context)->posted_msg_box = NULL;
	/* this is here so that we know when the file saying is over */
    CONTEXT_DATA(context)->is_file_saving = False;
}

void
FE_ImageLoadDialog(MWContext* context)
{
    Widget msg_box;

    if (CONTEXT_DATA(context)->posted_msg_box != NULL)
	return;

    msg_box = fe_save_dialog_create(context, fe_SaveDialogImageLoadName);

    fe_save_dialog_info_init(msg_box, XFE_SAVE_DIALOG_IMAGELOAD, 1);
    CONTEXT_DATA(context)->posted_msg_box = msg_box;

    fe_save_dialog_info_set(msg_box, "", 1);

    XtManageChild(msg_box);
}

void
FE_ImageLoadDialogDestroy(MWContext* context)
{
    Widget msg_box = CONTEXT_DATA(context)->posted_msg_box;

    if (msg_box == NULL)
	return;

    fe_save_dialog_destroy(msg_box);

    CONTEXT_DATA(context)->posted_msg_box = NULL;
}

typedef struct {
  Widget  form;
  Widget  widget;
  unsigned response;
  Boolean done;
} YesToAllInfo;

static void
fe_yes_to_all_cb(Widget widget, XtPointer closure, XtPointer call_data)
{
  YesToAllInfo* info = (YesToAllInfo*)closure;
  int i;
  char* name;
  char* widget_name = XtName(widget);

  for (i = 0; (name = fe_CreateYesToAllDialog_button_data[i].name); i++) {
    if (strcmp(name, widget_name) == 0) {
      info->response = fe_CreateYesToAllDialog_button_data[i].type;
      break;
    }
  }

  info->widget = widget;
  info->done = TRUE;

  XtUnmanageChild(info->form);
}

#if 0
static unsigned
fe_OverWriteFileQuestionDialog(MWContext* context, char* filename)
{
  char* msg;
  int   size;
  Bool  rv = FALSE;

  size = XP_STRLEN(filename) + 1 /* slash*/
       + XP_STRLEN(fe_globalData.overwrite_file_message) + 1; /*nul*/

  msg = (char *)XP_ALLOC(size);
  if (!msg)
    return False;

  PR_snprintf(msg, size, fe_globalData.overwrite_file_message, filename);

  if (FE_Confirm(context, msg))
    rv = TRUE;

  XP_FREE(msg);

  return rv;
}
#endif

static unsigned
fe_DoYesToAllDialog(MWContext* context, char* filename)
{
  YesToAllInfo info;
  Arg      av[20];
  Cardinal ac;
  XtCallbackRec cb_data;
  XmString msg_string;
  char* msg;
  int size;
  Widget form;
  Widget default_button;

  size = XP_STRLEN(filename) + 1 /* slash*/
       + XP_STRLEN(fe_globalData.overwrite_file_message) + 1; /*nul*/

  msg = (char *)XP_ALLOC(size);
  if (!msg)
    return XFE_DIALOG_CANCEL_BUTTON;

  PR_snprintf(msg, size, fe_globalData.overwrite_file_message, filename);
  msg_string = XmStringCreateLocalized(msg);
  XP_FREE(msg);

  cb_data.callback = fe_yes_to_all_cb;
#ifdef OSF1
  cb_data.closure = (void *)&info;
#else
  cb_data.closure = &info;
#endif

  ac = 0;
  XtSetArg(av[ac], XmNarmCallback, &cb_data); ac++;
  XtSetArg(av[ac], XmNmessageString, msg_string); ac++;
  form = fe_CreateYesToAllDialog(context, "confirmSaveFiles", av, ac);

  default_button = fe_YesToAllDialogGetChild(form, XFE_DAILOG_YESTOALL_BUTTON);
  ac = 0;
  XtSetArg(av[ac], XmNdefaultButton, default_button); ac++;
#if 0 /* why doesn't this work? */
  XtSetArg(av[ac], XmNinitialFocus, default_button); ac++;
#endif
  XtSetValues(form, av, ac);

  info.response = XFE_DIALOG_CANCEL_BUTTON;
  info.done = FALSE;
  info.widget = NULL;
  info.form = form;

  XmStringFree(msg_string);
    
  XtManageChild(form);

  fe_NukeBackingStore(form);
  while (!info.done)
    fe_EventLoop();

  XtDestroyWidget(form);

  return info.response;
}

ED_SaveOption
FE_SaveFileExistsDialog(MWContext* context, char* filename) 
{
  switch (fe_DoYesToAllDialog(context, filename)) {
  case XFE_DIALOG_YES_BUTTON:
    return ED_SAVE_OVERWRITE_THIS;
  case XFE_DAILOG_YESTOALL_BUTTON:
    return ED_SAVE_OVERWRITE_ALL;
  case XFE_DIALOG_NO_BUTTON:
    return ED_SAVE_DONT_OVERWRITE_THIS;
  case XFE_DIALOG_NOTOALL_BUTTON:
    return ED_SAVE_DONT_OVERWRITE_ALL;
  default:
    return ED_SAVE_CANCEL;
  }
}

Boolean
FE_SaveErrorContinueDialog(MWContext*   context,
			   char*        filename, 
			   ED_FileError error)
{
  if (error != ED_ERROR_NONE) {
      /* There was a an error while saving on:\n%s */
      /* The error code = (%d).*/
      return fe_editor_report_file_error(context, error, filename, TRUE);
  } else {
      return TRUE;
  }
}

Boolean XP_ConvertUrlToLocalFile (const char *url, char **localName)
{
  Boolean filefound = FALSE;
  char *path = NULL;
  char *file = NULL;
  XP_StatStruct statinfo;

  if (localName)
    {
      *localName = NULL;
    }

  /* Must have "file:" url type and at least 1 character after '//' */
  if (!url || !NET_IsLocalFileURL((char *) url))
    {
	return FALSE; 
    }

  /* Extract file path from url: e.g. "/cl/foo/file.html" */
  path = NET_ParseURL (url, GET_PATH_PART);
  if (!path || XP_STRLEN(path) < 1) {
    return FALSE;
  }

#if 0
  /* Get filename - skip over initial "/" */
  file = WH_FileName (path+1, xpURL);
  XP_FREE (path);
#endif
  file = XP_STRDUP(path);
 
  /* Test if file exists */
  if (XP_Stat (file, &statinfo, xpURL) >= 0 &&
      statinfo.st_mode & S_IFREG)
    {
      filefound = TRUE;
    }
 
  if (localName)
    {
      /* Pass string to caller; we didn't change it, so there's
         no need to strdup
         */
      *localName = file;
    }

  return filefound;
}

char *
XP_BackupFileName (const char *url)
{
  char *filename = NULL;
  char* path;

  /* Must have a "file:" url and at least 1 character after the "//" */
  if (!url || !NET_IsLocalFileURL((char *) url))
    {
      return FALSE;
    }

  /* Extract file path from url: e.g. "/cl/foo/file.html" */
  path = NET_ParseURL (url, GET_PATH_PART);
  if (!path || XP_STRLEN(path) < 1) {
    return FALSE;
  }

  filename = (char *) XP_ALLOC ((XP_STRLEN (path) + 5) * sizeof (char));
  if (!filename)
    {
      return NULL;
    }

  /* Get filename but ignore "file://" */
  {
      char* filename2 = WH_FileName (path, xpURL);
      if (!filename2) return NULL;
      XP_STRCPY (filename, filename2);
      XP_FREE(filename2);
  }

  /* Add extension to the filename */
  XP_STRCAT (filename, "~");
  return filename;
}

void
fe_EditorUpdateToolbar(MWEditorContext* context, ED_TextFormat unused_for_now)
{
}

Boolean
fe_EditorLineBreakCanInsert(MWContext* context)
{
   return (EDT_IsSelected(context) == FALSE);
}

/*
 *    type             description
 *    ED_BREAK_NORMAL  insert line break, ignore images.
 *    ED_BREAK_LEFT    Break so it passes the image on the left
 *    ED_BREAK_RIGHT   Break past the right image.
 *    ED_BREAK_BOTH    Break below image(s).
 */
void
fe_EditorLineBreak(MWContext* context, ED_BreakType type)
{
    if (!fe_EditorLineBreakCanInsert(context))
        return;
    EDT_InsertBreak(context, type);
    fe_EditorUpdateToolbar(context, 0);
}

void
fe_EditorNonBreakingSpace(MWContext* context)
{
    EDT_InsertNonbreakingSpace(context);
    fe_EditorUpdateToolbar(context, 0);
}

void
fe_EditorParagraphMarksSetState(MWContext* context, Boolean display)
{
    EDT_SetDisplayParagraphMarks(context, (Bool)display);
}

Boolean
fe_EditorParagraphMarksGetState(MWContext* context)
{
    return (Boolean)EDT_GetDisplayParagraphMarks(context);
}

char*
fe_EditorDefaultGetTemplate()
{
    return XFE_EDITOR_DEFAULT_DOCUMENT_TEMPLATE_URL;
}

void
fe_EditorDefaultGetColors(LO_Color*  bg_color,
			  LO_Color*  normal_color,
			  LO_Color*  link_color,
			  LO_Color*  active_color,
			  LO_Color*  followed_color)
{
    if (bg_color)
        *bg_color = lo_master_colors[LO_COLOR_BG];

    if (normal_color)
        *normal_color = lo_master_colors[LO_COLOR_FG];

    if (link_color)
        *link_color = lo_master_colors[LO_COLOR_LINK];

    if (active_color)
        *active_color = lo_master_colors[LO_COLOR_ALINK];

    if (followed_color)
        *followed_color = lo_master_colors[LO_COLOR_VLINK];
}

static char* fe_last_publish_location;
static char* fe_last_publish_username;
static char* fe_last_publish_password;

#define SAFE_STRDUP(x) ((x)? XP_STRDUP(x): NULL)

Boolean
fe_EditorDefaultGetLastPublishLocation(MWContext* context,
				       char** location,
				       char** username,
				       char** password)
{
    if (location)
        *location = SAFE_STRDUP(fe_last_publish_location);
    if (username)
        *username = SAFE_STRDUP(fe_last_publish_username);
    if (password)
        *password = SECNAV_UnMungeString(fe_last_publish_password);

    return fe_last_publish_password != NULL;
}

void
fe_EditorDefaultSetLastPublishLocation(MWContext* context,
				       char* location,
				       char* username,
				       char* password)
{
    /* NOTE: this function is not MT safe */
    if (fe_last_publish_location)
        XP_FREE(fe_last_publish_location);
    if (fe_last_publish_username)
        XP_FREE(fe_last_publish_username);
    if (fe_last_publish_password)
        XP_FREE(fe_last_publish_password);

    fe_last_publish_location = NULL;
    fe_last_publish_username = NULL;
    fe_last_publish_password = NULL;

    if (location) {
		char* full_location;
		int   rv;

        fe_last_publish_location = XP_STRDUP(location);

		if (username)
			fe_last_publish_username = XP_STRDUP(username);

		if (password)
			fe_last_publish_password = SECNAV_MungeString(password);

		full_location = NULL; /* having to do this is SO stupid */
		rv = NET_MakeUploadURL(&full_location, location, username, NULL);

		if (rv && full_location != NULL) {
			PREF_SetCharPref("editor.publish_last_loc", full_location);
			PREF_SetCharPref("editor.publish_last_pass",
							 fe_last_publish_password?
							 fe_last_publish_password: "");
		}
    }
}

void
fe_EditorPreferencesGetAutoSave(MWContext* context,
				Boolean*   enable,
				unsigned*  time)
{
    if (fe_globalPrefs.editor_autosave_period == 0) {
	*time = 0;
	*enable = FALSE;
    } else {
	*time = fe_globalPrefs.editor_autosave_period;
	*enable = TRUE;
    }
}

void
fe_EditorPreferencesSetAutoSave(MWContext* context,
				Boolean    enable,
				unsigned   time)
{
    struct fe_MWContext_cons *rest;

    if (enable) {
	fe_globalPrefs.editor_autosave_period = time;
    } else {
	fe_globalPrefs.editor_autosave_period = 0;
	time = 0;
    }

    /*
     *    Walk over the contexts, and tell the BE to reset the
     *    autosave.
     */
    for (rest = fe_all_MWContexts; rest; rest = rest->next) {
	if (rest->context->type == MWContextEditor) {
	    EDT_SetAutoSavePeriod(rest->context, time);
	}
    }
}

void
fe_EditorPreferencesGetLinksAndImages(MWContext* context,
				      Boolean*   links, 
				      Boolean*   images)
{
    if (links)
	*links = fe_globalPrefs.editor_maintain_links;
    if (images)
	*images = fe_globalPrefs.editor_keep_images;
}

void
fe_EditorPreferencesSetLinksAndImages(MWContext* context,
				      Boolean    links, 
				      Boolean    images)
{
    fe_globalPrefs.editor_maintain_links = links;
    fe_globalPrefs.editor_keep_images = images;
}

Boolean
fe_EditorPreferencesGetPublishLocation(MWContext* context,
				       char** location,
				       char** username,
				       char** password)
{
    if (location)
        *location = SAFE_STRDUP(fe_globalPrefs.editor_publish_location);
    if (username)
        *username = SAFE_STRDUP(fe_globalPrefs.editor_publish_username);
    if (password)
        *password =
	  SECNAV_UnMungeString(fe_globalPrefs.editor_publish_password);

    return (fe_globalPrefs.editor_publish_password != NULL);
}

void
fe_EditorPreferencesSetPublishLocation(MWContext* context,
				       char* location,
				       char* username,
				       char* password)
{
    if (fe_globalPrefs.editor_publish_location)
        XP_FREE(fe_globalPrefs.editor_publish_location);
    if (fe_globalPrefs.editor_publish_username)
        XP_FREE(fe_globalPrefs.editor_publish_username);
    if (fe_globalPrefs.editor_publish_password)
        XP_FREE(fe_globalPrefs.editor_publish_password);

    fe_globalPrefs.editor_publish_location = NULL;
    fe_globalPrefs.editor_publish_username = NULL;
    fe_globalPrefs.editor_publish_password = NULL;

    if (location) {
        fe_globalPrefs.editor_publish_location = XP_STRDUP(location);

	if (username)
	    fe_globalPrefs.editor_publish_username = XP_STRDUP(username);

	if (password)
	    fe_globalPrefs.editor_publish_password
	      = SECNAV_MungeString(password);
    }
    fe_globalPrefs.editor_save_publish_password = (password != NULL);
}

char*
fe_EditorPreferencesGetBrowseLocation(MWContext* context)
{
    return SAFE_STRDUP(fe_globalPrefs.editor_browse_location);
}

void
fe_EditorPreferencesSetBrowseLocation(MWContext* context, char* location)
{
    if (fe_globalPrefs.editor_browse_location)
        XP_FREE(fe_globalPrefs.editor_browse_location);
    fe_globalPrefs.editor_browse_location = NULL;

    if (location)
	fe_globalPrefs.editor_browse_location = XP_STRDUP(location);
}

char*
fe_EditorPreferencesGetAuthor(MWContext* context)
{
    if (fe_globalPrefs.editor_author_name)
	return fe_globalPrefs.editor_author_name;
    else
	return "";
}

void
fe_EditorPreferencesSetAuthor(MWContext* context, char* name)
{
    if (fe_globalPrefs.editor_author_name)
	XtFree(fe_globalPrefs.editor_author_name);
    fe_globalPrefs.editor_author_name = XtNewString(name);
}

char*
fe_EditorPreferencesGetTemplate(MWContext* context)
{
    if (fe_globalPrefs.editor_document_template)
	return fe_globalPrefs.editor_document_template;
    else
	return NULL;
}

void
fe_EditorPreferencesSetTemplate(MWContext* context, char* name)
{
    if (fe_globalPrefs.editor_document_template)
	XtFree(fe_globalPrefs.editor_document_template);
    fe_globalPrefs.editor_document_template = XtNewString(name);
}

void
fe_EditorPreferencesGetEditors(MWContext* context, char** html, char** image)
{
    if (html != NULL) {
	if (fe_globalPrefs.editor_html_editor)
	    *html = fe_globalPrefs.editor_html_editor;
	else
	    *html = NULL;
    }

    if (image != NULL) {
	if (fe_globalPrefs.editor_image_editor)
	    *image = fe_globalPrefs.editor_image_editor;
	else
	    *image = NULL;
    }
}

void
fe_EditorPreferencesSetEditors(MWContext* context, char* html, char* image)
{
    if (html != NULL) {
	if (fe_globalPrefs.editor_html_editor)
	    XtFree(fe_globalPrefs.editor_html_editor);
	fe_globalPrefs.editor_html_editor = XtNewString(html);
    }

    if (image != NULL) {
	if (fe_globalPrefs.editor_image_editor)
	    XtFree(fe_globalPrefs.editor_image_editor);
	fe_globalPrefs.editor_image_editor = XtNewString(image);
    }
}

Boolean
fe_EditorPreferencesGetColors(MWContext* context,
			      char*      background_image,
			      LO_Color*  bg_color,
			      LO_Color*  normal_color,
			      LO_Color*  link_color,
			      LO_Color*  active_color,
			      LO_Color*  followed_color)
{
    if (bg_color)
	*bg_color = fe_globalPrefs.editor_background_color;
    if (normal_color)
	*normal_color = fe_globalPrefs.editor_normal_color;
    if (link_color)
	*link_color = fe_globalPrefs.editor_link_color;
    if (active_color)
	*active_color = fe_globalPrefs.editor_active_color;
    if (followed_color)
	*followed_color = fe_globalPrefs.editor_followed_color;

    if (background_image) {
	if (fe_globalPrefs.editor_background_image)
	    strcpy(background_image,  fe_globalPrefs.editor_background_image);
	else
	    background_image[0] = '\0';
    }

    return fe_globalPrefs.editor_custom_colors;
}

void
fe_EditorPreferencesSetColors(MWContext* context,
							  char*      background_image,
							  LO_Color*  bg_color,
							  LO_Color*  normal_color,
							  LO_Color*  link_color,
							  LO_Color*  active_color,
							  LO_Color*  followed_color)
{
    XP_Bool set = FALSE;

    if (bg_color) {
		fe_globalPrefs.editor_background_color = *bg_color;
		set = TRUE;
    }
    if (normal_color) {
		fe_globalPrefs.editor_normal_color = *normal_color;
		set = TRUE;
    }
    if (link_color) {
		fe_globalPrefs.editor_link_color = *link_color;
		set = TRUE;
    }
    if (active_color) {
		fe_globalPrefs.editor_active_color = *active_color;
		set = TRUE;
    }
    if (followed_color) {
		fe_globalPrefs.editor_followed_color = *followed_color;
		set = TRUE;
    }

    fe_globalPrefs.editor_custom_colors = set;

    if (fe_globalPrefs.editor_background_image)
		XtFree(fe_globalPrefs.editor_background_image);
    if (background_image && background_image[0] != '\0')
		fe_globalPrefs.editor_background_image = XtNewString(background_image);
    else
		fe_globalPrefs.editor_background_image = NULL;
}

Boolean
fe_EditorDocumentGetImagesWithDocument(MWContext* context)
{
    Boolean keep;
    EDT_PageData* page_data = EDT_GetPageData(context);

    if (page_data == NULL) {
	fe_EditorPreferencesGetLinksAndImages(context, NULL, &keep);
    } else {
	keep = page_data->bKeepImagesWithDoc;
	EDT_FreePageData(page_data);
    }

    return keep;
}

void
fe_EditorDocumentSetImagesWithDocument(MWContext* context, Boolean keep)
{
    EDT_PageData* page_data = EDT_GetPageData(context);

    if (page_data != NULL) {
	page_data->bKeepImagesWithDoc = keep;
	EDT_SetPageData(context, page_data);
	EDT_FreePageData(page_data);
    }
}

char*
fe_EditorDocumentLocationGet(MWContext* context)
{
    History_entry* hist_ent;

    /* get the location */
    hist_ent = SHIST_GetCurrent(&context->hist);
    if (hist_ent)
	return hist_ent->address;
    else
	return NULL;
}

void
fe_EditorDocumentSetTitle(MWContext* context, char* name)
{
    EDT_PageData* page_data = EDT_GetPageData(context);

	if (!page_data)
		return;

	if (page_data->pTitle)
		XP_FREE(page_data->pTitle);

	if (name && name[0] != '\0')
		page_data->pTitle = XP_STRDUP(name);
	else
		page_data->pTitle = NULL;

	EDT_SetPageData(context, page_data);

	EDT_FreePageData(page_data);
}

char*
fe_EditorDocumentGetTitle(MWContext* context)
{
    EDT_PageData* page_data;
	char*         value = NULL;

    /* get the title */
    page_data = EDT_GetPageData(context);
	if (page_data != NULL) {
		if (page_data->pTitle != NULL && page_data->pTitle[0] != '\0')
			value = XP_STRDUP(page_data->pTitle);
	
		EDT_FreePageData(page_data);
	}

    return value;
}

#define BEGIN_DIRTY(xxx) XP_Bool dirty = FALSE

#define MARK_DIRTY(context)           \
if (!(dirty)) {                       \
	EDT_BeginBatchChanges((context)); \
	dirty = TRUE;                     \
}

#define END_DIRTY(context)            \
if ((dirty)) {                        \
	EDT_EndBatchChanges((context));   \
}

void
fe_EditorDocumentSetMetaData(MWContext* context, char* name, char* value)
{
    EDT_MetaData *pData = EDT_NewMetaData();

	BEGIN_DIRTY(context);

    if ( pData ) {

        pData->bHttpEquiv = FALSE;
        if ( name && XP_STRLEN(name) > 0 ) {

			MARK_DIRTY(context);

            pData->pName = XP_STRDUP(name);
            if ( value && XP_STRLEN(value) > 0 ) {
                pData->pContent = XP_STRDUP(value);
                EDT_SetMetaData(context, pData);
            } else {
                /* (Don't really need to do this) */
				pData->pContent = NULL; 
                /* Remove the item */
				EDT_DeleteMetaData(context, pData);
            }
		}

		EDT_FreeMetaData(pData);
    }

	END_DIRTY(context);
}

char*
fe_EditorDocumentGetMetaData(MWContext* context, char* name)
{
    EDT_MetaData* pData;
    int count;
    int i;

    /* lifted from winfe/edprops.cpp */
    count = EDT_MetaDataCount(context);
    for (i = 0; i < count; i++ ) {
        pData = EDT_GetMetaData(context, i);
		if (pData != NULL && !pData->bHttpEquiv) {
			if (XP_STRCASECMP(pData->pName, name) == 0) {
				return pData->pContent;
			}
	    }
    }
    return NULL;
}

static char* fe_editor_known_meta_data_names[] = {
    "Author",
    "Description",
#if 0 /* this one need to end up in user variables */
    "Generator",
#endif
    "Last-Modified",
    "Created",
    "Classification",
    "Keywords",
    NULL
};

static Boolean
fe_is_known_meta_data_name(char* name)
{
    unsigned i;

    for (i = 0; fe_editor_known_meta_data_names[i] != NULL; i++) {
	if (XP_STRCASECMP(fe_editor_known_meta_data_names[i], name) == 0)
	    return TRUE;
    }
    return FALSE;
}

fe_NameValueItem*
fe_EditorDocumentGetHttpEquivMetaDataList(MWContext* context)
{
    int count = EDT_MetaDataCount(context);
    int i;
    EDT_MetaData* pData;
    fe_NameValueItem* items;
    unsigned nitems = 0;

    items = (fe_NameValueItem*)XtMalloc(sizeof(fe_NameValueItem)*(count+1));

    for (i = 0; i < count; i++) {
		pData = EDT_GetMetaData(context, i);

		if (pData != NULL && pData->bHttpEquiv != 0) {
			items[nitems].name = XtNewString(pData->pName);
			items[nitems].value = XtNewString(pData->pContent);
			nitems++;
		}
    }
    items[nitems].name = NULL;
    items[nitems].value = NULL;

    items = (fe_NameValueItem*)XtRealloc((XtPointer)items,
					 sizeof(fe_NameValueItem)*(nitems+1));

    return items;
}

void
fe_EditorDocumentSetHttpEquivMetaDataList(MWContext* context,
					  fe_NameValueItem* list)
{
    fe_NameValueItem* old_list;
    int i;
    int j;
    EDT_MetaData *meta_data;
	BEGIN_DIRTY(context);

    old_list = fe_EditorDocumentGetHttpEquivMetaDataList(context);

    /* first delete items no longer in set */
    for (i = 0; old_list[i].name != NULL; i++) {

		for (j = 0; list[j].name != NULL; j++) {
			if (XP_STRCASECMP(old_list[i].name, list[j].name) == 0) /* match */
				break;
		}

		/* I'm sure all this thrashing of the heap can't be required ?? */
		if (list[j].name == NULL) { /* it's gone now */
			meta_data = EDT_NewMetaData();
			meta_data->pName = old_list[i].name;
			meta_data->pContent = NULL;
			MARK_DIRTY(context);
			EDT_DeleteMetaData(context, meta_data);
			EDT_FreeMetaData(meta_data);
		}
    }

    /* now set the ones that have changed */
    for (j = 0; list[j].name != NULL; j++) {
		meta_data = EDT_NewMetaData();
		meta_data->pName = list[j].name;
		meta_data->pContent = list[j].value;
		meta_data->bHttpEquiv = TRUE;
		MARK_DIRTY(context);
		EDT_SetMetaData(context, meta_data);
		EDT_FreeMetaData(meta_data);
    }

	END_DIRTY(context);
}

fe_NameValueItem*
fe_EditorDocumentGetAdvancedMetaDataList(MWContext* context)
{
    int count = EDT_MetaDataCount(context);
    int i;
    EDT_MetaData* pData;
    fe_NameValueItem* items;
    unsigned nitems = 0;

    items = (fe_NameValueItem*)XtMalloc(sizeof(fe_NameValueItem)*(count+1));

    for (i = 0; i < count; i++) {
		pData = EDT_GetMetaData(context, i);

		if (pData == NULL)
			continue;

		/* is it one of the ones handled in document general */
        if ((pData->bHttpEquiv) || fe_is_known_meta_data_name(pData->pName))
			continue;
	
		items[nitems].name = XtNewString(pData->pName);
		items[nitems].value = XtNewString(pData->pContent);
		nitems++;
    }
    items[nitems].name = NULL;
    items[nitems].value = NULL;

    items = (fe_NameValueItem*)XtRealloc((XtPointer)items,
					 sizeof(fe_NameValueItem)*(nitems+1));

    return items;
}

void
fe_EditorDocumentSetAdvancedMetaDataList(MWContext* context,
					 fe_NameValueItem* list)
{
    fe_NameValueItem* old_list;
    int i;
    int j;
    EDT_MetaData *meta_data;
	BEGIN_DIRTY(context);

    old_list = fe_EditorDocumentGetAdvancedMetaDataList(context);

    /* first delete items no longer in set */
    for (i = 0; old_list[i].name != NULL; i++) {

		for (j = 0; list[j].name != NULL; j++) {
			if (XP_STRCASECMP(old_list[i].name, list[j].name) == 0) /* match */
				break;
		}

		/* I'm sure all this thrashing of the heap can't be required ?? */
		if (list[j].name == NULL) { /* it's gone now */
			meta_data = EDT_NewMetaData();
			meta_data->pName = old_list[i].name;
			meta_data->pContent = NULL;
			MARK_DIRTY(context);
			EDT_DeleteMetaData(context, meta_data);
			EDT_FreeMetaData(meta_data);
		}
    }

    /* now set the ones that have changed */
    for (j = 0; list[j].name != NULL; j++) {
		meta_data = EDT_NewMetaData();
		meta_data->pName = list[j].name;
		meta_data->pContent = list[j].value;
		MARK_DIRTY(context);
		EDT_SetMetaData(context, meta_data);
		EDT_FreeMetaData(meta_data);
    }

	END_DIRTY(context);
}

Boolean
fe_EditorDocumentGetColors(MWContext* context,
						   char*      background_image,
						   Boolean*   leave_image,
						   LO_Color*  bg_color,
						   LO_Color*  normal_color,
						   LO_Color*  link_color,
						   LO_Color*  active_color,
						   LO_Color*  followed_color)
{
    EDT_PageData* page_data = EDT_GetPageData(context);
    Boolean set = FALSE;

    fe_EditorDefaultGetColors(bg_color,
							  normal_color,
							  link_color, 
							  active_color,
							  followed_color);

	if (!page_data)
		return FALSE;

    if (background_image != NULL) {
        if (page_data->pBackgroundImage != NULL) {
			XP_STRCPY(background_image, page_data->pBackgroundImage);
		} else {
			background_image[0] = '\0';
		}
    }

    if (bg_color) {
        if (page_data->pColorBackground) {
			*bg_color = *page_data->pColorBackground;
			set = TRUE;
		}
    }

    if (normal_color) {
        if (page_data->pColorText) {
			*normal_color = *page_data->pColorText;
			set = TRUE;
		}
    }

    if (link_color) {
        if (page_data->pColorLink) {
			*link_color = *page_data->pColorLink;
			set = TRUE;
		}
    }

    if (active_color) {
        if (page_data->pColorActiveLink) {
			*active_color = *page_data->pColorActiveLink;
			set = TRUE;
		}
    }

    if (followed_color) {
        if (page_data->pColorFollowedLink) {
			*followed_color = *page_data->pColorFollowedLink;
			set = TRUE;
		}
    }

	if (leave_image) {
		*leave_image = page_data->bBackgroundNoSave;
	}


    EDT_FreePageData(page_data);

    return set;
}

void
fe_EditorDocumentSetColors(MWContext* context,
						   char*      background_image,
						   Boolean    leave_image,
						   LO_Color*  bg_color,
						   LO_Color*  normal_color,
						   LO_Color*  link_color,
						   LO_Color*  active_color,
						   LO_Color*  followed_color)
{
    EDT_PageData  save_data;
    EDT_PageData* page_data;
    
    if (background_image != NULL && background_image[0] == '\0')
		background_image = NULL;
    
    page_data = EDT_GetPageData(context);

	if (!page_data)
		return;

    memcpy(&save_data, page_data, sizeof(EDT_PageData));
    
    page_data->pBackgroundImage = background_image;
    page_data->pColorBackground = bg_color;
    page_data->pColorText = normal_color;
    page_data->pColorLink = link_color;
    page_data->pColorActiveLink = active_color;
    page_data->pColorFollowedLink = followed_color;
    page_data->bBackgroundNoSave = leave_image;
    
    EDT_SetPageData(context, page_data);
    memcpy(page_data, &save_data, sizeof(EDT_PageData));
    EDT_FreePageData(page_data);
}

typedef struct fe_EditorCommandDescription {
    intn  id;
    char* name;
} fe_EditorCommandDescription;

/*
 *    Yuck lifted from editor.h
 */
#define kNullCommandID 0
#define kTypingCommandID 1
#define kAddTextCommandID 2
#define kDeleteTextCommandID 3
#define kCutCommandID 4
#define kPasteTextCommandID 5
#define kPasteHTMLCommandID 6
#define kPasteHREFCommandID 7
#define kChangeAttributesCommandID 8
#define kIndentCommandID 9
#define kParagraphAlignCommandID 10
#define kMorphContainerCommandID 11
#define kInsertHorizRuleCommandID 12
#define kSetHorizRuleDataCommandID 13
#define kInsertImageCommandID 14
#define kSetImageDataCommandID 15
#define kInsertBreakCommandID 16
#define kChangePageDataCommandID 17
#define kSetMetaDataCommandID 18
#define kDeleteMetaDataCommandID 19
#define kInsertTargetCommandID 20
#define kSetTargetDataCommandID 21
#define kInsertUnknownTagCommandID 22
#define kSetUnknownTagDataCommandID 23
#define kGroupOfChangesCommandID 24
#define kSetListDataCommandID 25

#define kInsertTableCommandID 26
#define kDeleteTableCommandID 27
#define kSetTableDataCommandID 28

#define kInsertTableCaptionCommandID 29
#define kSetTableCaptionDataCommandID 30
#define kDeleteTableCaptionCommandID 31

#define kInsertTableRowCommandID 32
#define kSetTableRowDataCommandID 33
#define kDeleteTableRowCommandID 34

#define kInsertTableColumnCommandID 35
#define kSetTableCellDataCommandID 36
#define kDeleteTableColumnCommandID 37

#define kInsertTableCellCommandID 38
#define kDeleteTableCellCommandID 39

static fe_EditorCommandDescription fe_editor_commands[] = {
    { kNullCommandID,             "Null"              },
    { kTypingCommandID,           "Typing"            },
    { kAddTextCommandID,          "AddText"           },
    { kDeleteTextCommandID,       "DeleteText"        },
    { kCutCommandID,              "Cut"               },
    { kPasteTextCommandID,        "PasteText"         },
    { kPasteHTMLCommandID,        "PasteHTML"         },
    { kPasteHREFCommandID,        "PasteHREF"         },
    { kChangeAttributesCommandID, "ChangeAttributes"  },
    { kIndentCommandID,           "Indent"            },
    { kParagraphAlignCommandID,   "ParagraphAlign"    },
    { kMorphContainerCommandID,   "MorphContainer"    },
    { kInsertHorizRuleCommandID,  "InsertHorizRule"   },
    { kSetHorizRuleDataCommandID, "SetHorizRuleData"  },
    { kInsertImageCommandID,      "InsertImage"       },
    { kSetImageDataCommandID,     "SetImageData"      },
    { kInsertBreakCommandID,      "InsertBreak"       },
    { kChangePageDataCommandID,   "ChangePageData"    },
    { kSetMetaDataCommandID,      "SetMetaData"       },
    { kDeleteMetaDataCommandID,   "DeleteMetaData"    },
    { kInsertTargetCommandID,     "InsertTarget"      },
    { kSetTargetDataCommandID,    "SetTargetData"     },
    { kInsertUnknownTagCommandID, "InsertUnknownTag"  },
    { kSetUnknownTagDataCommandID,"SetUnknownTagData" },
    { kGroupOfChangesCommandID,   "GroupOfChanges"    },
    { kSetListDataCommandID,      "SetListData"       },
    { kInsertTableCommandID,      "InsertTable"       },
    { kDeleteTableCommandID,      "DeleteTable"       },
    { kSetTableDataCommandID,     "SetTableData"      },
    { kInsertTableCaptionCommandID,  "InsertTableCaption"  },
    { kSetTableCaptionDataCommandID, "SetTableCaptionData" },
    { kDeleteTableCaptionCommandID,  "DeleteTableCaption"  },
    { kInsertTableRowCommandID,    "InsertTableRow"   },
    { kSetTableRowDataCommandID,   "SetTableRowData"  },
    { kDeleteTableRowCommandID,    "DeleteTableRow"   },
    { kInsertTableColumnCommandID, "InsertTableColumn" },
    { kSetTableCellDataCommandID,  "SetTableCellData"  },
    { kDeleteTableColumnCommandID, "DeleteTableColumn" },
    { kInsertTableCellCommandID,   "InsertTableCell"  },
    { kDeleteTableCellCommandID,   "DeleteTableCell"  },

    { -1, NULL }
};

static XmString
resource_motif_string(Widget widget, char* name)
{
  XmString result = XfeSubResourceGetXmStringValue(widget,
												   name,
												   "UndoItem",
												   XmNlabelString,
												   XmCXmString,
												   NULL);
  
  if (!result)
      result = XmStringCreateLocalized(name);

  return result;
}

static XmString string_cache;

static XmString
fe_editor_undo_get_label(MWContext* context, intn id, Boolean is_undo)
{
    char     buf[64];
    XmString string;
    char* name;
    int n;

    name = NULL;
    for (n = 0; fe_editor_commands[n].name != NULL; n++) {
	if (fe_editor_commands[n].id == id) {
	    name = fe_editor_commands[n].name;
	    break;
	}
    }

    /*
     *    For ids we don't know yet, just show "Undo" or "Redo"
     */
    if (!name)
	name = "";

    /*
     *    Look for it in the cache here!!!!
     */
    /*FIXME*/
    if (string_cache)
	XmStringFree(string_cache);

    if (is_undo)
	XP_STRCPY(buf, "undo");
    else
	XP_STRCPY(buf, "redo");
    XP_STRCAT(buf, name);

    /*
     *    Resource the string.
     */
    /*
     *    Why is it so hard to get a string! Like, what,
     *    I'd never want to do that.
     */
    string = resource_motif_string(CONTEXT_DATA(context)->menubar, buf);
    string_cache = string;

    return string;
}

Boolean
fe_EditorCanUndo(MWContext* context, XmString* label_return)
{
    intn command_id = EDT_GetUndoCommandID(context, 0);

    if (label_return) {
	*label_return = fe_editor_undo_get_label(context,
						 command_id,
						 True);
    }
    
    if (command_id != CEDITCOMMAND_ID_NULL)
	return True;
    else
	return FALSE;
}

Boolean
fe_EditorCanRedo(MWContext* context, XmString* label_return)
{
    intn command_id = EDT_GetRedoCommandID(context, 0);
    
    if (label_return) {
	*label_return = fe_editor_undo_get_label(context, 
						 command_id,
						 False);
    }
    
    if (command_id != CEDITCOMMAND_ID_NULL)
	return True;
    else
	return FALSE;
}

void
fe_EditorUndo(MWContext* context)
{
    if (EDT_GetUndoCommandID(context, 0) != CEDITCOMMAND_ID_NULL) {
	EDT_Undo(context);
	fe_EditorUpdateToolbar(context, 0);
    } else {
	fe_Bell(context);
    }
}

void
fe_EditorRedo(MWContext* context)
{
    if (EDT_GetRedoCommandID(context, 0) != CEDITCOMMAND_ID_NULL) {
	EDT_Redo(context);
	fe_EditorUpdateToolbar(context, 0);
    } else {
	fe_Bell(context);
    }
}

Boolean
fe_EditorCanCut(MWContext* context)
{
    return (EDT_COP_OK == EDT_CanCut(context, FALSE));
}

Boolean
fe_EditorCanCopy(MWContext* context)
{
    return (EDT_COP_OK == EDT_CanCopy(context, TRUE));
}

static char fe_editor_data_name[] = "NETSCAPE_HTML";
static char fe_editor_html_name[] = "HTML";
static char fe_editor_text_name[] = "STRING";
static char fe_editor_app_name[] = "MOZILLA";

static Atom XFE_TEXT_ATOM = 0;
static Atom XFE_HTML_ATOM = 0;
static Atom XFE_DATA_ATOM = 0;

#define XFE_CCP_TEXT (fe_editor_text_name)
#define XFE_CCP_HTML (fe_editor_html_name)
#define XFE_CCP_DATA (fe_editor_data_name)
#define XFE_CCP_NAME (fe_editor_app_name)

#if 0
/*
 *    Cannot get this to work.
 */
static void
fe_property_notify_eh(Widget w, XtPointer closure, XEvent *ev, Boolean *cont)
{
    MWContext* context = (MWContext *)closure;
    Display*   display = XtDisplay(CONTEXT_WIDGET(context));
    Atom       cb_atom = XmInternAtom(display, "CLIPBOARD", False);
    Atom       string_atom = XmInternAtom(display, XFE_CCP_TEXT, False);
    Atom       html_atom = XmInternAtom(display, XFE_CCP_HTML, False);
    Atom       data_atom = XmInternAtom(display, XFE_CCP_DATA, False);
    Atom       long_string_atom = XmInternAtom(display, "_MOTIF_CLIP_FORMAT_" "STRING", False);
    Atom       long_html_atom = XmInternAtom(display, "_MOTIF_CLIP_FORMAT_" "NETSCAPE_HTML", False);
    fe_EditorContextData* editor;

#if 0
    if (ev->xproperty.atom == XA_CUT_BUFFER0) {
	editor = EDITOR_CONTEXT_DATA(context);
	fe_editor_paste_update_cb(editor->toolbar_paste,
				  (XtPointer)context, NULL);
    }
#endif

#if 0
    if (ev->xproperty.atom ==  XmInternAtom (dpy, "CLIPBOARD", False)XA_CUT_BUFFER0)
#endif
}
#endif

Boolean
fe_EditorCanPasteData(MWContext* context, int32* r_length)
{
    Widget   mainw    = CONTEXT_WIDGET(context);
    Display* display  = XtDisplay(mainw);
    Window   window   = XtWindow(mainw);
    int      cb_result;
    unsigned long length;

	cb_result = XmClipboardInquireLength(display, window,
										 XFE_CCP_DATA, &length);

    if (cb_result ==  ClipboardSuccess && length > 0) {
		*r_length = length;
		return TRUE;
    } else {
		*r_length = 0;
		return FALSE;
    }
}

Boolean
fe_EditorCanPasteHtml(MWContext* context, int32* r_length)
{
    Widget   mainw    = CONTEXT_WIDGET(context);
    Display* display  = XtDisplay(mainw);
    Window   window   = XtWindow(mainw);
    int      cb_result;
    unsigned long length;

	cb_result = XmClipboardInquireLength(display, window,
										 XFE_CCP_HTML, &length);

    if (cb_result ==  ClipboardSuccess && length > 0) {
		*r_length = length;
		return TRUE;
    } else {
		*r_length = 0;
		return FALSE;
    }
}

Boolean
fe_EditorCanPasteText(MWContext* context, int32* r_length)
{
    Widget   mainw    = CONTEXT_WIDGET(context);
    Display* display  = XtDisplay(mainw);
    Window   window   = XtWindow(mainw);
    int      cb_result;
    unsigned long length;

	cb_result = XmClipboardInquireLength(display, window,
										 XFE_CCP_TEXT, &length);

    if (cb_result ==  ClipboardSuccess && length > 0) {
		*r_length = length;
		return TRUE;
    } else {
		*r_length = 0;
		return FALSE;
    }
}

Boolean
fe_EditorCanPasteLocal(MWContext* context, int32* r_length)
{
	*r_length = 0;
    return FALSE;
}

#ifdef OSF1
extern char *fe_GetInternalHtml();
extern char *fe_GetInternalText();
#endif

Boolean
fe_EditorCanPasteCache(MWContext* context, int32* r_length)
{
	unsigned long text_len = 0;
	unsigned long html_len = 0;
	unsigned long data_len = 0;
	XP_Bool  sel_p = False;

    /*
     *    We have an "new" unified internal clipboard....
     *    Check it.
     */
	sel_p = fe_InternalSelection(True, &text_len, &html_len, &data_len);

	if (sel_p) {
		if (data_len > 0) {
			*r_length = data_len;
			return TRUE;
		}
		else {
			if (html_len > 0) {
				*r_length = html_len;
				return TRUE;
			}
			else {
				if (text_len > 0) {
					*r_length = text_len;
					return TRUE;
				}
			}
		}
	}

	*r_length = 0;
    return FALSE;
}

Boolean
fe_EditorCanPaste(MWContext* context)
{
    int32 len;

    /*
     *    We have to check if we can do a local paste first,
     *    because we can hang the server if we try to re-own
     *    the selection. This is all very hokey. I need to
     *    talk to jamie.
     */
    if (
	fe_EditorCanPasteLocal(context, &len)
	||
	fe_EditorCanPasteCache(context, &len)
	||
	fe_EditorCanPasteData(context, &len)
	|| 
	fe_EditorCanPasteHtml(context, &len)
	|| 
	fe_EditorCanPasteText(context, &len))
    {
	return TRUE;
    } else {
	return FALSE;
    }
}

void
fe_EditorCopyToClipboard(MWContext* context, char* string, Time timestamp)
{
	fe_OwnEDTSelection (context, timestamp, True, string, NULL, NULL, 0);
}

static void
fe_editor_report_copy_error(MWContext* context, EDT_ClipboardResult code)
{
    char* msg;
    int id;

    switch (code) {
    case EDT_COP_DOCUMENT_BUSY: /* IDS_DOCUMENT_BUSY */
	id = XFE_EDITOR_COPY_DOCUMENT_BUSY;
	/* Cannot copy or cut at this time, try again later. */
	break;
    case EDT_COP_SELECTION_EMPTY: /* IDS_SELECTION_EMPTY */
	id = XFE_EDITOR_COPY_SELECTION_EMPTY;
	/* Nothing is selected. */
	break;
    case EDT_COP_SELECTION_CROSSES_TABLE_DATA_CELL:
	id = XFE_EDITOR_COPY_SELECTION_CROSSES_TABLE_DATA_CELL;
	/* The selection includes a table cell boundary. */
	/* Deleting and copying are not allowed.         */
	break;
    default:
	return;
    }

    msg = XP_GetString(id);
    FE_Alert(context, msg);
}

void
fe_EditorReportCopyError(MWContext* context, EDT_ClipboardResult code)
{
	fe_editor_report_copy_error(context, code);
} 

Boolean
fe_EditorOwnSelection(MWContext *context, Time time, 
					  Boolean clip_p, Boolean cut_p)
{
    char* text;
    int32 text_len;
    char* data;
    int32 data_len;
    EDT_ClipboardResult cut_result;

	if (cut_p && clip_p) {
		if (!fe_EditorCanCut(context)) {
#ifdef DEBUG_rhess
			fprintf(stderr, "fe_EditorOwnSelection::[ can't CUT ]\n");
#endif
			return False;
		}

		cut_result = EDT_CutSelection(context, 
									  &text, &text_len,
									  &data, &data_len);
	}
	else {
		if (!fe_EditorCanCopy(context)) {
#ifdef DEBUG_rhess
			fprintf(stderr, "fe_EditorOwnSelection::[ can't COPY ]\n");
#endif
			return False;
		}

		cut_result = EDT_CopySelection(context, 
									   &text, &text_len,
									   &data, &data_len);
	}

    if (cut_result != EDT_COP_OK) {
		fe_editor_report_copy_error(context, cut_result);
		return False;
    }

    if ((!text || text_len <= 0) && (!data || data_len <= 0))
		return False;
    
	fe_OwnEDTSelection (context, time, clip_p, text, NULL, data, data_len);

	return True;
}

void
fe_EditorDisownSelection(MWContext *context, Time time, Boolean clip_p)
{
	Widget mainw = CONTEXT_WIDGET(context);

	if (clip_p)
		{
			fe_DisownClipboard(mainw, time);
		}
	else
		{
			fe_DisownPrimarySelection(mainw, time);

#ifdef NOT_rhess
			if (!fe_ContextHasPopups(context))
				EDT_ClearSelection(context);
#endif
		}
}

Boolean
fe_EditorCut(MWContext* context, Time timestamp)
{
	Boolean own_p = False;

	own_p = fe_EditorOwnSelection(context, timestamp, True, True);

    fe_EditorUpdateToolbar(context, 0);

	return own_p;
}

Boolean
fe_EditorCopy(MWContext* context, Time timestamp)
{
	Boolean own_p = False;

	own_p = fe_EditorOwnSelection(context, timestamp, True, False);

    fe_EditorUpdateToolbar(context, 0);

	return own_p;
}

static char fe_editor_local_name[] = "LOCAL";

#define XFE_CCP_LOCAL (fe_editor_local_name)

typedef struct {
    MWContext* context;
    char*  text;
    char*  html;
    char*  data;
	unsigned long text_len;
	unsigned long html_len;
	unsigned long data_len;
	int    count;
	int    max_count;
} xfe_pdata;

static xfe_pdata*
fe_new_pdata(MWContext* context, int max_count)
{
	xfe_pdata* pData = (xfe_pdata*) XP_ALLOC(sizeof(xfe_pdata));

	pData->context = context;
	pData->text = NULL;
	pData->html = NULL;
	pData->data = NULL;
	pData->text_len = 0;
	pData->html_len = 0;
	pData->data_len = 0;
	pData->count = 0;
	pData->max_count = max_count;

	return( pData );
}

static void
fe_free_pdata(xfe_pdata* pData)
{
	if (pData->text) {
		XtFree(pData->text);
	}

	if (pData->html) {
		XtFree(pData->html);
	}

	if (pData->data) {
		XtFree(pData->data);
	}

	pData->context = NULL;
	pData->text = NULL;
	pData->html = NULL;
	pData->data = NULL;
	pData->text_len = 0;
	pData->html_len = 0;
	pData->data_len = 0;
	pData->count = 0;
	pData->max_count = 0;

	XP_FREE(pData);
}

static void
fe_primary_sel_cb(Widget widget, XtPointer cdata, Atom *sel, Atom *type, 
					   XtPointer value, unsigned long *len, int *format)
{
	xfe_pdata* pData = (xfe_pdata*) cdata;

	if (value) {
		char* clip = (char *) value;

		if (*type == XFE_DATA_ATOM) {
			/*
			 * WARNING... [ this is actually HTML data, not HTML text ]
			 */
#ifdef DEBUG_rhess
			fprintf(stderr, 
					"fe_primary_sel_cb::[ data ][ %d ]\n", *len);
#endif
			pData->count++;
			pData->data_len = *len;
			pData->data = clip;
		}
		else {
			if (*type == XFE_HTML_ATOM) {
				/*
				 * NOTE... [ this is HTML source text ]
				 */
#ifdef DEBUG_rhess
				fprintf(stderr, 
						"fe_primary_sel_cb::[ html ][ %d ]\n", *len);
#endif
				pData->count++;
				pData->html_len = *len;
				pData->html = clip;
			}
			else {
				if (*type == XFE_TEXT_ATOM) {
#ifdef DEBUG_rhess
					fprintf(stderr, 
							"fe_primary_sel_cb::[ text ][ %d ]\n", *len);
#endif
					pData->count++;
					pData->text_len = *len;
					pData->text = clip;
				}
				else {
#ifdef DEBUG_rhess
					fprintf(stderr, "WARNING... [ unknown type atom ]\n");
#endif
				}
			}
		}
	}
	else {
		pData->count++;

#ifdef DEBUG_rhess
		if (*type == XFE_DATA_ATOM) {
			fprintf(stderr, "fe_primary_sel_cb::[ NULL ] - DATA\n");
		}
		else {
			if (*type == XFE_HTML_ATOM) {
				fprintf(stderr, "fe_primary_sel_cb::[ NULL ] - HTML\n");
			}
			else {
				if (*type == XFE_TEXT_ATOM) {
					fprintf(stderr, "fe_primary_sel_cb::[ NULL ] - TEXT\n");
				}
				else {
					fprintf(stderr, "fe_primary_sel_cb::[ ERROR ]\n");
				}
			}
		}
#endif
	}

	if (pData->count == pData->max_count) {
		if (pData->data_len > 0) {
#ifdef DEBUG_rhess
			fprintf(stderr, 
					"fe_primary_sel_cb::[ PasteHTML ][ %d ]\n",
					pData->data_len);
#endif
			EDT_PasteHTML(pData->context, pData->data);
		}
		else {
			if (pData->html_len > 0) {
#ifdef DEBUG_rhess
				fprintf(stderr, 
						"fe_primary_sel_cb::[ PasteQuote ][ %d ]\n",
						pData->html_len);
#endif
				EDT_PasteQuoteBegin(pData->context, True);
				EDT_PasteQuote(pData->context, pData->html);
				EDT_PasteQuoteEnd(pData->context);
			}
			else {
				if (pData->text_len > 0) {
#ifdef DEBUG_rhess
					fprintf(stderr, 
							"fe_primary_sel_cb::[ PasteText ][ %d ]\n",
							pData->text_len);
#endif
					EDT_PasteText(pData->context, pData->text);
				}
#ifdef DEBUG_rhess
				else {
					fprintf(stderr, "fe_primary_sel_cb::[ EMPTY ]\n");
				}
#endif
			}
		}
		fe_free_pdata(pData);
	}
}

Boolean
fe_EditorPastePrimarySel(MWContext* context, Time timestamp)
{
    Widget         mainw = CONTEXT_WIDGET(context);
    Display*       dpy   = XtDisplay(mainw);
	char*          data  = NULL;
	unsigned long  len   = 0;
	unsigned long  text_len = 0;
	unsigned long  html_len = 0;
	unsigned long  data_len = 0;
	XP_Bool        sel_p = False;

	/*
	 *  NOTE:  this only works if we are talking to one server... [ oops ]
	 */
	if (XFE_TEXT_ATOM == 0)
		XFE_TEXT_ATOM = XmInternAtom(dpy, XFE_CCP_TEXT, False);

	if (XFE_HTML_ATOM == 0)
		XFE_HTML_ATOM = XmInternAtom(dpy, XFE_CCP_HTML, False);
	
	if (XFE_DATA_ATOM == 0)
		XFE_DATA_ATOM = XmInternAtom(dpy, XFE_CCP_DATA, False);
	
	/*
	 *  NOTE:  first, let's see if we have an internal selection...
	 *
	 */
	sel_p = fe_InternalSelection(False, &text_len, &html_len, &data_len);

	if (sel_p) {
		if (data_len > 0) {
			data = fe_GetPrimaryData(&len);

#ifdef DEBUG_rhess
			fprintf(stderr, "fe_EditorPastePrimarySel::[ DATA ]\n");
#endif
			EDT_PasteHTML(context, data);
		}
		else {
			if (html_len > 0) {
				data = fe_GetPrimaryHtml();
		
#ifdef DEBUG_rhess
				fprintf(stderr, "fe_EditorPastePrimarySel::[ HTML ]\n");
#endif
				EDT_PasteQuoteBegin( context, True);
				EDT_PasteQuote( context, data);
				EDT_PasteQuoteEnd( context );
			}
			else {
				if (text_len > 0) {
					data = fe_GetPrimaryText();
				
#ifdef DEBUG_rhess
					fprintf(stderr, "fe_EditorPastePrimarySel::[ TEXT ]\n");
#endif
					EDT_PasteText(context, data);
				}
			}
		}
	}
	else {
		xfe_pdata  *pData = fe_new_pdata(context, 3);
		Atom        atoms[3];
		XtPointer   tags[3];

		atoms[0] = XFE_TEXT_ATOM;
		atoms[1] = XFE_HTML_ATOM;
		atoms[2] = XFE_DATA_ATOM;

		tags[0] = (XtPointer) pData;
		tags[1] = (XtPointer) pData;
		tags[2] = (XtPointer) pData;
		
		XtGetSelectionValues(mainw, 
							 XA_PRIMARY, atoms, 3,
							 fe_primary_sel_cb, 
							 tags,
							 timestamp);
	}
	return False; /* keep compiler happy for now */
}

Boolean
fe_EditorPaste(MWContext* context, Time timestamp)
{
    int32   length;
	char*   tmp_data  = NULL;
    char*   clip_data = NULL;
    char*   clip_name = NULL; /* none */
    int     cb_result;
    Boolean rv = FALSE;
    Widget   mainw    = CONTEXT_WIDGET(context);
    Display* display  = XtDisplay(mainw);
    Window   window   = XtWindow(mainw);
	unsigned long len = 0;
	unsigned long text_len = 0;
	unsigned long html_len = 0;
	unsigned long data_len = 0;
	int      i;


	if (!clip_data) {
		XP_Bool sel_p = fe_InternalSelection(True, 
											 &text_len, &html_len, &data_len);

		if (sel_p) {
			if (data_len > 0) {
				tmp_data = fe_GetInternalData(&len);

				clip_data = XP_ALLOC(len+1);
				memcpy(clip_data, tmp_data, len);
				clip_name = XFE_CCP_DATA;
			}
			else {
				if (html_len > 0) {
					tmp_data = fe_GetInternalHtml();
				
					clip_data = XP_STRDUP(tmp_data);
					clip_name = XFE_CCP_HTML;
				}
				else {
					if (text_len > 0) {
						tmp_data = fe_GetInternalText();
				
						clip_data = XP_STRDUP(tmp_data);
						clip_name = XFE_CCP_TEXT;
					}
				}
			}
		}
	}

    if (!clip_data) { /* go look on real CLIPBOARD */

		i = 0;
		do {
			cb_result = XmClipboardLock(display, window);
#ifdef DEBUG_rhess
			fprintf(stderr, "lockEDT::[ %d ]\n", i);
#endif
			i++;
		} while (cb_result == ClipboardLocked);
		
		if (fe_EditorCanPasteHtml(context, &length)) {
			clip_data = XP_ALLOC(length+1);
			clip_name = XFE_CCP_HTML;
		} else if (fe_EditorCanPasteText(context, &length)) {
			clip_data = XP_ALLOC(length+1);
			clip_name = XFE_CCP_TEXT;
		}
		
		cb_result = XmClipboardUnlock(display, window, TRUE);
#ifdef DEBUG_rhess
		fprintf(stderr, "fe_EditorPaste::[ unlock ]\n");
#endif
		
		if (clip_data) {
			
#ifdef DEBUG_rhess
			fprintf(stderr, "fe_EditorPaste::[ retrieve ]\n");
#endif
			cb_result = xfe_clipboard_retrieve_work(mainw,
                                                                clip_name,
                                                                &clip_data,
                                                                &len,
                                                                timestamp);
#ifdef DEBUG_rhess
			fprintf(stderr, "fe_EditorPaste::[ gotcha ]\n");
#endif
			
			if (cb_result == ClipboardSuccess) {
				clip_data[len] = '\0';
			} else {
				XP_FREE(clip_data);
				clip_data = NULL;
				clip_name = NULL;
			}
		}
    } 

    if (clip_name == XFE_CCP_DATA) {
#ifdef DEBUG_rhess
		fprintf (stderr, "fe_EditorPaste::[ XFE_CCP_DATA ]\n");
#endif
		rv = (EDT_PasteHTML(context, clip_data) == EDT_COP_OK);
		XP_FREE(clip_data);
    } else if (clip_name == XFE_CCP_HTML) {
#ifdef DEBUG_rhess
		fprintf (stderr, "fe_EditorPaste::[ XFE_CCP_HTML ]\n");
#endif
		EDT_PasteQuoteBegin( context, True);
		EDT_PasteQuote( context, clip_data);
		EDT_PasteQuoteEnd( context );
		XP_FREE(clip_data);
	} else if (clip_name == XFE_CCP_TEXT) {
#ifdef DEBUG_rhess
		fprintf (stderr, "fe_EditorPaste::[ XFE_CCP_TEXT ]\n");
#endif
		rv = (EDT_PasteText(context, clip_data) == EDT_COP_OK);
		XP_FREE(clip_data);
    } else if (clip_name == XFE_CCP_LOCAL) {
#ifdef DEBUG_rhess
		fprintf (stderr, "fe_EditorPaste::[ XFE_CCP_LOCAL ]\n");
#endif
		rv = (EDT_PasteText(context, clip_data) == EDT_COP_OK);
    } else {
		fe_Bell(context);
    }

#ifdef DONT_rhess
    fe_EditorUpdateToolbar(context, 0);
#endif
    return rv;
}

void
fe_EditorSelectAll(MWContext* context)
{
	Display *dpy  = XtDisplay(CONTEXT_WIDGET(context));
	Time     time = XtLastTimestampProcessed(dpy);

    EDT_SelectAll(context);  

    fe_EditorOwnSelection (context, time, False, False);
    fe_EditorUpdateToolbar(context, 0);
}

void
fe_EditorDeleteItem(MWContext* context, Boolean previous)
{
    EDT_ClipboardResult result;

	if (previous)
		result = EDT_DeletePreviousChar(context);
	else
		result = EDT_DeleteChar(context);

    if (result != EDT_COP_OK)
		fe_editor_report_copy_error(context, result);

    fe_EditorUpdateToolbar(context, 0);
}

Boolean
fe_EditorHorizontalRulePropertiesCanDo(MWContext* context)
{
    ED_ElementType type = EDT_GetCurrentElementType(context);

    if  (type == ED_ELEMENT_HRULE)
	return TRUE;
    else
	return FALSE;
}

void
fe_EditorHorizontalRulePropertiesSet(MWContext* context, EDT_HorizRuleData* data)
{
    if (fe_EditorHorizontalRulePropertiesCanDo(context)) {
	EDT_SetHorizRuleData(context, data);
    } else {
	EDT_InsertHorizRule(context, data);
    }
}

void
fe_EditorHorizontalRulePropertiesGet(MWContext* context, EDT_HorizRuleData* data)
{
    EDT_HorizRuleData* foo;

    if (fe_EditorHorizontalRulePropertiesCanDo(context)
		&&
		(foo = EDT_GetHorizRuleData(context)) != NULL) {
		memcpy(data, foo, sizeof(EDT_HorizRuleData));
		EDT_FreeHorizRuleData(foo);
    } else {
		data->size = 2;
		data->iWidth = 100;
		data->bWidthPercent = TRUE;
		data->align = ED_ALIGN_CENTER;
		data->bNoShade = FALSE;
    }
}

void
fe_EditorRefresh(MWContext* context)
{
    EDT_RefreshLayout(context);    
}

void
fe_EditorHrefInsert(MWContext* context, char* p_text, char* p_url)
{
    char* text;
    char* url;
	EDT_ClipboardResult result;

    if (!EDT_CanSetHREF(context)) { /* should not exist already */

		if (p_url && p_url[0] != '\0')
			url = XP_STRDUP(p_url);
		else
			return; /* no url specified */

		if (p_text && p_text[0] != '\0')
			text = XP_STRDUP(p_text);
		else
			text = XP_STRDUP(p_url);

		result = EDT_PasteHREF(context, &url, &text, 1);
	
		if (result != EDT_COP_OK)
			fe_editor_report_copy_error(context, result);
    }
}

Boolean
fe_EditorGetHref(MWContext *context) {
    char*         p;

    p = (char *) LO_GetSelectionText(context);
    if (p == NULL) return False;
    else
	return NULL != EDT_GetHREF(context);
}


Boolean
fe_EditorHrefGet(MWContext* context, char** text, char** url)
{
    EDT_HREFData* href_data = EDT_GetHREFData(context);
    Bool          is_selected = EDT_IsSelected(context);
    char*         p;

    p = (char *) LO_GetSelectionText(context);

    if (is_selected && p != NULL) {
	*text = XtNewString(p);
	XP_FREE(p);
    } else if (p == NULL) {   /* this fixes #27762 */
	*text = NULL;
    } else if ((p = EDT_GetHREFText(context)) != NULL) { 
			/* charlie's new get the text of a link thing */
	*text = p;
    } else {
	*text = NULL;
    }
    
    if (href_data != NULL && (p = href_data->pURL) != NULL) {
	*url = XtNewString(p);
    } else {
	*url = NULL;
    }

    if (href_data != NULL)
	EDT_FreeHREFData(href_data);

    return EDT_CanSetHREF(context);
}

void
fe_EditorHrefSetUrl(MWContext* context, char* url)
{
    EDT_HREFData* href_data;

    if (EDT_CanSetHREF(context)) {

	href_data = EDT_GetHREFData(context);

	if (!href_data)
	    href_data = EDT_NewHREFData();

	if (href_data->pURL)
	    XP_FREE(href_data->pURL);
	
	if (url)
	    href_data->pURL = XP_STRDUP(url);
	else
	    href_data->pURL = NULL; /* clear the href */

	EDT_SetHREFData(context, href_data);

	EDT_FreeHREFData(href_data);
    }
}

void
fe_EditorHrefClearUrl(MWContext* context)
{
    fe_EditorHrefSetUrl(context, NULL);
}

void
fe_EditorRemoveLinks(MWContext* context)
{
    /*
     *    Remove a single link or all links within selected region.
     */
    if (EDT_CanSetHREF(context)) {
        EDT_SetHREF(context, NULL);
    }
}

static Time
fe_getTimeFromEvent(MWContext* context, XEvent* event)
{
  Time time;
  time = (event && (event->type == KeyPress ||
		    event->type == KeyRelease)
	       ? event->xkey.time :
	       event && (event->type == ButtonPress ||
			 event->type == ButtonRelease)
	       ? event->xbutton.time :
	       XtLastTimestampProcessed (XtDisplay(CONTEXT_WIDGET (context))));
  return time;
}

Boolean
fe_EditorPropertiesDialogCanDo(MWContext* context,
			       fe_EditorPropertiesDialogType d_type)
{
    ED_ElementType e_type;

    if (
	d_type == XFE_EDITOR_PROPERTIES_IMAGE_INSERT
	||
	d_type == XFE_EDITOR_PROPERTIES_LINK_INSERT
	||
	d_type == XFE_EDITOR_PROPERTIES_CHARACTER
	||
	d_type == XFE_EDITOR_PROPERTIES_PARAGRAPH
	||
	d_type == XFE_EDITOR_PROPERTIES_TABLE
	|| 
	d_type == XFE_EDITOR_PROPERTIES_DOCUMENT
    )
	return TRUE; /* always ok to do */

    e_type = EDT_GetCurrentElementType(context);

    if (d_type == XFE_EDITOR_PROPERTIES_IMAGE) {
	if (e_type == ED_ELEMENT_IMAGE)
	    return TRUE;
	else
	    return FALSE;

    } else if (d_type == XFE_EDITOR_PROPERTIES_HRULE) {
	if (e_type == ED_ELEMENT_HRULE)
	    return TRUE;
	else
	    return FALSE;

    } else if (d_type == XFE_EDITOR_PROPERTIES_HTML_TAG) {
	if (e_type == ED_ELEMENT_UNKNOWN_TAG)
	    return TRUE;
	else
	    return FALSE;

    } else if (d_type == XFE_EDITOR_PROPERTIES_TARGET) {
	if (e_type == ED_ELEMENT_TARGET)
	    return TRUE;
	else
	    return FALSE;

    } else if (d_type == XFE_EDITOR_PROPERTIES_LINK) {
	if (EDT_GetHREF(context))
	    return TRUE;
	else
	    return FALSE;
    } 
    return FALSE;
}

/*
 *    This stuff is so crazy.
 *
 */
void
fe_EditorParagraphPropertiesSetAll(MWContext*    context,
				   TagType       paragragh_type,
				   EDT_ListData* list_data,
				   ED_Alignment  align)
{
    TagType       old_paragraph_type = EDT_GetParagraphFormatting(context);
    EDT_ListData* old_list_data = EDT_GetListData(context);
    Boolean       do_set;

    EDT_BeginBatchChanges(context);

    /*
     *    Windows has this kludge, so...
     */
    if (align == ED_ALIGN_LEFT)
	align = ED_ALIGN_DEFAULT;

    EDT_SetParagraphAlign(context, align);

    /*
     *    If there was a list before, and now there is none,
     *    remove the list.
     */
    if (old_list_data && !list_data) {
	for ( ; old_list_data; old_list_data = EDT_GetListData(context)) {
	    EDT_FreeListData(old_list_data);
	    EDT_Outdent(context);
	}
	old_list_data = NULL;
    }
    
    /*
     *    If the style has changed.
     */
    if (paragragh_type != old_paragraph_type)
	EDT_MorphContainer(context, paragragh_type);

    /*
     *    If there was no list, and now there is..
     */
    if (!old_list_data && list_data) {
	EDT_Indent(context);
	old_list_data = EDT_GetListData(context);
    }

    /*
     *    The BE is losing. It cannot deal with lists in selections,
     *    it returns no list data. So, we protect oursleves and
     *    end up doing nothing, just like windows.
     */
    if (old_list_data != NULL) {

	do_set = TRUE;

	/*
	 *    If this is a block quote..
	 */
	if (list_data != NULL && list_data->iTagType == P_BLOCKQUOTE) {
	    old_list_data->iTagType = P_BLOCKQUOTE;
	    old_list_data->eType = ED_LIST_TYPE_DEFAULT;
	}

	/*
	 *    If it's a list.
	 */
	else if (list_data != NULL) {
	    old_list_data->iTagType = list_data->iTagType;
	    old_list_data->eType = ED_LIST_TYPE_DEFAULT;
#if 0
	    /*
	     *    We have no UI for this, but maybe it's in  the author's
	     *    HTML. Don't change it. Actually, I have no idea.
	     */
	    old_list_data->bCompact = FALSE;
#endif
	    switch (list_data->iTagType) {
	    case P_UNUM_LIST:
		if (list_data->eType == ED_LIST_TYPE_DISC   ||
		    list_data->eType == ED_LIST_TYPE_SQUARE	||
		    list_data->eType == ED_LIST_TYPE_CIRCLE)
		    old_list_data->eType = list_data->eType;
		break;
	    case P_NUM_LIST:
		if (list_data->eType == ED_LIST_TYPE_DIGIT       ||
		    list_data->eType == ED_LIST_TYPE_BIG_ROMAN   ||
		    list_data->eType == ED_LIST_TYPE_SMALL_ROMAN ||
		    list_data->eType == ED_LIST_TYPE_BIG_LETTERS ||
		    list_data->eType == ED_LIST_TYPE_SMALL_LETTERS)
		    old_list_data->eType = list_data->eType;
		if (list_data->iStart > 0)
		    old_list_data->iStart = list_data->iStart;
		else
		    old_list_data->iStart = 1;
		break;
	    case P_DIRECTORY:
	    case P_MENU:
	    case P_DESC_LIST:
		break;
	    default: /* garbage, don't set */
		do_set = FALSE;
		break;
	    }
	}

	if (do_set)
	    EDT_SetListData(context, old_list_data);
    }

    /*
     *    Else it's just an ordinary old jo.
     */
    /* nothing to do */
 
    if (old_list_data != NULL)
	EDT_FreeListData(old_list_data);

    EDT_EndBatchChanges(context);
    
    fe_EditorUpdateToolbar(context, 0);
}

Boolean
fe_EditorParagraphPropertiesGetAll(MWContext*    context,
				   TagType*      paragragh_type,
				   EDT_ListData* list_data,
				   ED_Alignment* align)
{
    TagType       old_paragraph_type = EDT_GetParagraphFormatting(context);
    EDT_ListData* old_list_data = EDT_GetListData(context);

    if (paragragh_type)
	*paragragh_type = old_paragraph_type;

    if (list_data && old_list_data)
	*list_data = *old_list_data;

    if (align)
	*align = EDT_GetParagraphAlign(context);

    if (old_list_data != NULL)
	EDT_FreeListData(old_list_data);

    return (old_list_data != NULL);
}

/*FIXME*/
/*
 *    I think the following routine could now be replaced
 *    by EDT_SetFontColor(context, &color);
 */
void
fe_EditorColorSet(MWContext* context, LO_Color* color)
{
    EDT_CharacterData cdata;

    memset(&cdata, 0, sizeof(EDT_CharacterData));

    cdata.mask = TF_FONT_COLOR;
    cdata.pColor = color;

    if (color)
	cdata.values = TF_FONT_COLOR;
    else
	cdata.values = 0;

    EDT_SetCharacterData(context, &cdata);
}

Boolean
fe_EditorColorGet(MWContext* context, LO_Color* color)
{
	EDT_CharacterData* cdata = EDT_GetCharacterData(context);
    Boolean   rv = FALSE;
	LO_Color* use_color = NULL;

    if (cdata != NULL && (cdata->values & TF_FONT_COLOR) != 0) { /* custom */
		rv = TRUE; /* custom */
		if ((cdata->mask & TF_FONT_COLOR) != 0 && cdata->pColor != NULL) {
			use_color = cdata->pColor; /* defined */
		}
	}

	if (use_color != NULL) {
		*color = *use_color;
    } else {
		fe_EditorDocumentGetColors(context,
								   NULL, /* bg image */
								   NULL, /* leave image */
								   NULL, /* bg color */
								   color,/* normal color */
								   NULL, /* link color */
								   NULL, /* active color */
								   NULL); /* followed color */
    }

	if (cdata != NULL)
		EDT_FreeCharacterData(cdata);

    return rv; /* custom */
}

extern int XFE_COULDNT_FORK;
extern int XFE_EXECVP_FAILED;

void
fe_ExecCommand(MWContext* context, char** argv)
{
    char buf[MAXPATHLEN];
    int forked;

    switch (forked = fork()) {
    case -1:
	fe_perror(context, XP_GetString(XFE_COULDNT_FORK));
	break;
    case 0: {
	Display *dpy = XtDisplay(CONTEXT_WIDGET(context));
	close(ConnectionNumber(dpy));

	execvp(argv[0], argv);

	PR_snprintf(buf, sizeof(buf), XP_GetString(XFE_EXECVP_FAILED),
			fe_progname, argv[0]);
	perror(buf);
	exit(3);	/* Note that this only exits a child fork.  */
	break;
    }
    default:
	/* block */
	break;
    }
}

static char**
fe_editor_parse_argv(char* string, char* filename)
{
    char     buf[MAXPATHLEN];
    unsigned size = 32;
    unsigned argc = 0;
    char**   argv = (char**)malloc(sizeof(char*) * size);
    char*    p;
    char*    q;
    char*    r;

    for (p = string; *p != '\0'; ) {

	q = buf;

	/* skip leading white */
	while (isspace(*p))
	    p++;

	while (!isspace(*p) && *p != '\0' && ((q - buf) < sizeof(buf))) {
	    if (*p == '%' && p[1] == 'f') {
		for (r = filename; *r != '\0'; ) {
		    *q++ = *r++;
		}
		p += 2;
	    } else {
		*q++ = *p++;
	    }
	}
	*q = '\0';

	if (q > buf)
	    argv[argc++] = strdup(buf);

	if (argc == size) {
	    size = argc + 32;
	    argv = (char**)realloc((void*)argv, sizeof(char*) * size);
	}
    }

    if (argc > 0) {
	argv[argc] = NULL;
	return argv;
    } else {
	free(argv);
	return NULL;
    }
}

void
fe_editor_free_argv(char** argv)
{
    unsigned i;
    for (i = 0; argv[i]; i++)
	free(argv[i]);
    free(argv);
}

void
fe_editor_exec_command(MWContext* context, char* command, char* filename)
{
    char** argv;

    if ((argv = fe_editor_parse_argv(command, filename)) != NULL) {

	fe_ExecCommand(context, argv);

	fe_editor_free_argv(argv);
    } else {
	/* Empty command specified! */
	FE_Alert(context, XP_GetString(XFE_COMMAND_EMPTY));
    }
}

void
fe_EditorEditSource(MWContext* context)
{
    char* html_editor;
    char* local_name;
    History_entry* hist_entry;

    if (!FE_CheckAndSaveDocument(context))
		return;
    fe_EditorPreferencesGetEditors(context, &html_editor, NULL);

    if (html_editor == NULL || XP_STRSTR(html_editor, "%f") == NULL) {
		/*
		 * "Please specify an editor in Editor Preferences.\n"
		 * "Specify the file argument with %f. Netscape will\n"
		 * "replace %f with the correct file name. Examples:\n"
		 * "             xterm -e vi %f\n"
		 * "             xgifedit %f\n"
		 */
		char* msg = XP_GetString(XFE_EDITOR_HTML_EDITOR_COMMAND_EMPTY);
	
      	if (XFE_Confirm(context, msg)) {
			fe_EditorShowNewPreferences(context); /* call C++ adaptor */
		}

		return;
    } 

    hist_entry = SHIST_GetCurrent(&context->hist);
    if (hist_entry && hist_entry->address) {
		if (XP_ConvertUrlToLocalFile(hist_entry->address, &local_name)) {
			fe_editor_exec_command(context, html_editor, local_name);
			XP_FREE(local_name);
		}
    }
}

void
fe_EditorEditImage(MWContext* context, char* file)
{
    char* image_editor;
    char* local_name = NULL;
    char* full_file;
    char* directory;
    char* p;
    History_entry* hist_entry;

    fe_EditorPreferencesGetEditors(context, NULL, &image_editor);

    if (image_editor == NULL || XP_STRSTR(image_editor, "%f") == NULL) {
		/*
		 * "Please specify an editor in Editor Preferences.\n"
		 * "Specify the file argument with %f. Netscape will\n"
		 * "replace %f with the correct file name. Examples:\n"
		 * "             xterm -e vi %f\n"
		 * "             xgifedit %f\n"
		 */
		char* msg = XP_GetString(XFE_EDITOR_IMAGE_EDITOR_COMMAND_EMPTY);

      	if (XFE_Confirm(context, msg)) {
			fe_EditorShowNewPreferences(context); /* call C++ adaptor */
		}

		return;
    }

    while (isspace(*file))
		file++;

    if (file[0] != '/') { /* not absolute */
		hist_entry = SHIST_GetCurrent(&context->hist);
		if (hist_entry && hist_entry->address) {
			if (XP_ConvertUrlToLocalFile(hist_entry->address, &local_name)) {
				p = strrchr(local_name, '/'); /* find last slash */
				if (p) {
					*p = '\0';
					directory = local_name;
				} else {
					directory = ".";
				}
				full_file = (char*)XtMalloc(strlen(directory) +
											strlen(file) + 2);
				sprintf(full_file, "%s/%s", directory, file);
				XP_FREE(local_name);

				fe_editor_exec_command(context, image_editor, full_file);
				XtFree(full_file);
			}
		}
    } else {
		fe_editor_exec_command(context, image_editor, file);
    }
}

#define COPY_SAVE_EXTRA(b, a) \
{ char* xx_extra = (b)->pExtra; *(b) = *(a); (b)->pExtra = xx_extra; }

#define COPY_SAVE_BGCOLOR(b, a) \
{ char* xx_bgcolor = (b)->pColorBackground; *(b) = *(a); (b)->pColorBackground = xx_bgcolor; }

static LO_Color*
fe_dup_lo_color(LO_Color* color)
{
    LO_Color* new_color;

    if (color != NULL && (new_color = (LO_Color*)XtNew(LO_Color)) != NULL) {
	*new_color = *color;
	return new_color;
    } else {
	return NULL;
    }
}

#define DUP_BGCOLOR(b, a) \
{ (b)->pColorBackground = fe_dup_lo_color((a)->pColorBackground); }

/*
 *    Table support.
 */
Boolean
fe_EditorTableCanInsert(MWContext* context)
{
    return !EDT_IsJavaScript(context);
}

void
fe_EditorTableInsert(MWContext* context, EDT_AllTableData* all_data)
{
    EDT_TableData* new_data = EDT_NewTableData();
    EDT_TableCaptionData* caption_data;
    
    if (all_data) {
		EDT_TableData* data = &all_data->td;
		COPY_SAVE_EXTRA(new_data, data);
		DUP_BGCOLOR(new_data, data);
    } else {
		new_data->iBorderWidth = 1;
		new_data->iRows = 2;
		new_data->iColumns = 2;
    }
    EDT_BeginBatchChanges(context);

    EDT_InsertTable(context, new_data);

    if (all_data != NULL && all_data->has_caption) {
		caption_data = EDT_NewTableCaptionData();
		caption_data->align = all_data->cd.align;
		EDT_InsertTableCaption(context, caption_data);
    }

    EDT_FreeTableData(new_data);

	EDT_EndBatchChanges(context);
}

Boolean
fe_EditorTableCanDelete(MWContext* context)
{
    return (EDT_IsInsertPointInTable(context) != 0);
}

void
fe_EditorTableDelete(MWContext* context)
{
    if (EDT_IsInsertPointInTable(context) != 0) {
	EDT_DeleteTable(context);
	fe_EditorUpdateToolbar(context, 0);
    }
}

Boolean
fe_EditorTableGetData(MWContext* context, EDT_AllTableData* all_data)
{
    Boolean in_table = FALSE;
    EDT_TableData* table_data = EDT_GetTableData(context);
    EDT_TableData* pt_data = &all_data->td;
    EDT_TableCaptionData* caption_data;

    if (table_data) {
		*pt_data = *table_data;
		if (table_data->pColorBackground != NULL) {
			static LO_Color color_buf;
			color_buf = *table_data->pColorBackground;
			pt_data->pColorBackground = &color_buf; /*caller cannot hang on*/
		}
	
		if (table_data->pBackgroundImage != NULL) {
			static char* bg_image;
			if (bg_image != NULL)
				XP_FREE(bg_image);
			bg_image = XP_STRDUP(table_data->pBackgroundImage);
			pt_data->pBackgroundImage = bg_image; /*caller cannot hang on*/
		}
	
		EDT_FreeTableData(table_data);
		in_table = TRUE;
    } else {
		return FALSE;
    }

    if (EDT_IsInsertPointInTableCaption(context)
		&&
		(caption_data = EDT_GetTableCaptionData(context)) != NULL) {
		
		all_data->has_caption = TRUE;
		all_data->cd.align = caption_data->align;
    } else {
		all_data->has_caption = FALSE;
		all_data->cd.align = ED_ALIGN_TOP;
    }
    return TRUE;
}

void
fe_EditorTableSetData(MWContext* context, EDT_AllTableData* all_data)
{
    Boolean has_caption;
    Boolean wants_caption;
    EDT_TableData* table_data = EDT_GetTableData(context);
    EDT_TableCaptionData* caption_data;

    if (table_data) {
		EDT_BeginBatchChanges(context);

		/* get the basic table data */
		COPY_SAVE_EXTRA(table_data, &all_data->td);
		DUP_BGCOLOR(table_data, &all_data->td);

		EDT_SetTableData(context, table_data);

		caption_data = NULL;
		has_caption = EDT_IsInsertPointInTableCaption(context);
		wants_caption = all_data->has_caption;

		if (has_caption == TRUE && wants_caption == FALSE) {
			EDT_DeleteTableCaption(context);
		} else if (has_caption == FALSE && wants_caption == TRUE) {
			caption_data = EDT_NewTableCaptionData();
			caption_data->align = all_data->cd.align;
			EDT_InsertTableCaption(context, caption_data);
		} else if (has_caption
				   &&
				   (caption_data = EDT_GetTableCaptionData(context)) != NULL) {
			if (caption_data->align != all_data->cd.align) {
				caption_data->align = all_data->cd.align;
				EDT_SetTableCaptionData(context, caption_data);
			}
		}

		EDT_BeginBatchChanges(context);

		if (caption_data)
			EDT_FreeTableCaptionData(caption_data);

		EDT_FreeTableData(table_data);
    }
}

Boolean
fe_EditorTableRowCanInsert(MWContext* context)
{
    return (EDT_IsInsertPointInTableRow(context) != 0);
}

void
fe_EditorTableRowInsert(MWContext* context, EDT_TableRowData* data)
{
    EDT_TableRowData* new_data;

    if (!fe_EditorTableRowCanInsert(context)) {
		fe_Bell(context);
		return;
    }

    new_data = EDT_GetTableRowData(context);

	if (!new_data)
		return;

    if (data) {
		new_data->align = data->align;
		new_data->valign = data->valign;
		DUP_BGCOLOR(new_data, data);
    } 
    EDT_InsertTableRows(context, new_data, TRUE, 1);
    EDT_FreeTableRowData(new_data);
    fe_EditorUpdateToolbar(context, 0);
}

Boolean
fe_EditorTableRowCanDelete(MWContext* context)
{
    return (EDT_IsInsertPointInTableRow(context) != 0);
}

void
fe_EditorTableRowDelete(MWContext* context)
{
    if (EDT_IsInsertPointInTable(context) != 0) {
	EDT_DeleteTableRows(context, 1);
	fe_EditorUpdateToolbar(context, 0);
    }
}

Boolean
fe_EditorTableRowGetData(MWContext* context, EDT_TableRowData* p_data)
{
    EDT_TableRowData* row_data = EDT_GetTableRowData(context);

    if (row_data) {
		*p_data = *row_data;
		if (row_data->pColorBackground != NULL) {
			static LO_Color color_buf;
			color_buf = *row_data->pColorBackground;
			p_data->pColorBackground = &color_buf; /*caller cannot hang on*/
		}
		EDT_FreeTableRowData(row_data);
		return TRUE;
    } else {
		return FALSE;
    }
}

void
fe_EditorTableRowSetData(MWContext* context, EDT_TableRowData* p_data)
{
    EDT_TableRowData* row_data = EDT_GetTableRowData(context);

    if (row_data) {
	COPY_SAVE_EXTRA(row_data, p_data);
	DUP_BGCOLOR(row_data, p_data);
	EDT_SetTableRowData(context, row_data);	
	EDT_FreeTableRowData(row_data);
	fe_EditorUpdateToolbar(context, 0);
    }
}

Boolean
fe_EditorTableCaptionCanInsert(MWContext* context)
{
    return (EDT_IsInsertPointInTable(context) != 0);
}

void
fe_EditorTableCaptionInsert(MWContext* context, EDT_TableCaptionData* data)
{
    EDT_TableCaptionData* new_data = EDT_NewTableCaptionData();
    EDT_InsertTableCaption(context, new_data);
    EDT_FreeTableCaptionData(new_data);
    fe_EditorUpdateToolbar(context, 0);
}

Boolean
fe_EditorTableCaptionCanDelete(MWContext* context)
{
    return (EDT_IsInsertPointInTableCaption(context) != 0);
}

void
fe_EditorTableCaptionDelete(MWContext* context)
{
    if (EDT_IsInsertPointInTableCaption(context) != 0) {
	EDT_DeleteTableCaption(context);
	fe_EditorUpdateToolbar(context, 0);
    }
}

Boolean
fe_EditorTableColumnCanInsert(MWContext* context)
{
    return (EDT_IsInsertPointInTableRow(context) != 0);
}

void
fe_EditorTableColumnInsert(MWContext* context, EDT_TableCellData* data)
{
    EDT_TableCellData* new_data = EDT_NewTableCellData();
    EDT_InsertTableColumns(context, new_data, TRUE, 1);
    EDT_FreeTableCellData(new_data);

    fe_EditorUpdateToolbar(context, 0);
}

Boolean
fe_EditorTableColumnCanDelete(MWContext* context)
{
    return (EDT_IsInsertPointInTableRow(context) != 0);
}

void
fe_EditorTableColumnDelete(MWContext* context)
{
    if (EDT_IsInsertPointInTableRow(context) != 0) {
	EDT_DeleteTableColumns(context, 1);
	fe_EditorUpdateToolbar(context, 0);
    }
}

Boolean
fe_EditorTableCellCanInsert(MWContext* context)
{
    return (EDT_IsInsertPointInTableRow(context) != 0);
}

void
fe_EditorTableCellInsert(MWContext* context, EDT_TableCellData* data)
{
    EDT_TableCellData* new_data = EDT_NewTableCellData();
    EDT_InsertTableCells(context, new_data, TRUE, 1);
    EDT_FreeTableCellData(new_data);

    fe_EditorUpdateToolbar(context, 0);
}

Boolean
fe_EditorTableCellCanDelete(MWContext* context)
{
    return (EDT_IsInsertPointInTableCell(context) != 0);
}

void
fe_EditorTableCellDelete(MWContext* context)
{
    if (EDT_IsInsertPointInTableRow(context) != 0) {
	EDT_DeleteTableCells(context, 1);
	fe_EditorUpdateToolbar(context, 0);
    }
}

Boolean
fe_EditorTableCellGetData(MWContext* context, EDT_TableCellData* p_data)
{
    EDT_TableCellData* cell_data = EDT_GetTableCellData(context);
    if (cell_data) {
	*p_data = *cell_data;
	if (cell_data->pColorBackground != NULL) {
	    static LO_Color color_buf;
	    color_buf = *cell_data->pColorBackground;
	    p_data->pColorBackground = &color_buf; /*caller cannot hang on*/
	}
	EDT_FreeTableCellData(cell_data);
	return TRUE;
    } else {
	return FALSE;
    }
}

void
fe_EditorTableCellSetData(MWContext* context, EDT_TableCellData* p_data)
{
    EDT_TableCellData* cell_data = EDT_GetTableCellData(context);
    if (cell_data) {
	COPY_SAVE_EXTRA(cell_data, p_data);
	DUP_BGCOLOR(cell_data, p_data);
	EDT_SetTableCellData(context, cell_data);
	EDT_FreeTableCellData(cell_data);
	fe_EditorUpdateToolbar(context, 0);
    }
}

void
fe_EditorPublish(MWContext* context)
{
    /* Get the destination and what files to include from the user:*/
    fe_EditorPublishDialogDo(context);
}

void
fe_EditorDisplayTablesSet(MWContext* context, Boolean display)
{
    EDT_SetDisplayTables(context, display == TRUE);
    fe_EditorUpdateToolbar(context, 0);
}

Boolean
fe_EditorDisplayTablesGet(MWContext* context)
{
    return (EDT_GetDisplayTables(context) == TRUE);
}

static char*
fe_action_name(char* buf,
	       XtActionsRec* action_list,
	       Cardinal      action_list_size,
	       XtActionProc  action_proc,
	       String *av, Cardinal *ac)
{
    int i;
    char* name = XP_GetString(XFE_UNKNOWN); /* "<unknown>" */

    if (action_list_size == 0)
	action_list_size = 100000; /* look for null termination */

    /*
     *    Look for action name in the action_list.
     */
    for (i = 0; i < action_list_size && action_list[i].string != NULL; i++) {
	if (action_list[i].proc == action_proc) {
	    name = action_list[i].string;
	    break;
	}
    }

    strcpy(buf, name);
    strcat(buf, "(");
    for (i = 0; i < *ac; i++) {
	if (i != 0)
	    strcat(buf, ",");
	strcat(buf, av[i]);
    }
    strcat(buf, ")");

    return buf;
}

void xfe_GetShiftAndCtrl(XEvent* event, Boolean* shiftP, Boolean* ctrlP)
{
    if (event->type == KeyPress || event->type == KeyRelease) {
        unsigned int mask = event->xkey.state;
        if (shiftP) *shiftP = ((mask & ShiftMask) != 0);
        if (ctrlP) *ctrlP = ((mask & ControlMask) != 0);
    }
    else if (event->type == ButtonPress || event->type == ButtonRelease) {
        unsigned int mask = event->xbutton.state;
        if (shiftP) *shiftP = ((mask & ShiftMask) != 0);
        if (ctrlP) *ctrlP = ((mask & ControlMask) != 0);
    }
}

/*
 * NOTE:  because of this routine and its explicit need to disown the
 *        primary selection, we can't select text in the editor and then
 *        middle mouse paste it into another location in the same window...
 *
 *        - any ideas on how we might get around this one?...
 *
 *        :-l
 *
 */
void
fe_EditorGrabFocus(MWContext* context, XEvent *event)
{
	Widget da = NULL;
	XEvent ev;
  unsigned long x, y;
  Time time;
    LO_Element*   leHit = 0;
    ED_HitType    iTableHit; 

#ifdef DREDD_EDITOR
  fe_UserActivity(context); /* tell the app who has focus */
#else

	da = CONTEXT_DATA(context)->drawing_area;

    ev.type              = FocusIn;
    ev.xfocus.type       = FocusIn;
	ev.xfocus.send_event = False;
	ev.xfocus.display    = XtDisplay(da);
	ev.xfocus.window     = XtWindow(da);
	ev.xfocus.mode       = NotifyNormal;
    ev.xfocus.detail     = NotifyDetailNone;

	XtDispatchEvent(&ev);

	XSync(XtDisplay(da), False);

	/*
	XtSetKeyboardFocus(CONTEXT_WIDGET(context), da);
	*/
#endif

  /* don't do this -- Japanese input method requires focus
  fe_NeutralizeFocus(context);
  */

  time = fe_getTimeFromEvent(context, event); /* get the time */

  fe_EventLOCoords(context, event, &x, &y); /* get the doc co-ords */

  /*
   *    Do this first, so the EDT_ClearSelection() in the call doesn't
   *    blow away what EDT_StartSelection() does!
   *
   */
  fe_EditorDisownSelection(context, time, False); /* give up selection */

  /* see if we're in a table first... */
  iTableHit = EDT_GetTableHitRegion(context, x, y, &leHit, False);
  if ( iTableHit != ED_HIT_NONE )
  {
      Boolean shift, ctrl;
      xfe_GetShiftAndCtrl(event, &shift, &ctrl);

      EDT_SelectTableElement(context, x, y,
                             leHit, iTableHit,
                             ctrl,      /* bModifierKeyPressed */
                             FALSE);    /*bExtendSelection*/
                           /*shift);    -*bExtendSelection*/
      return;
  }
  /* for some reason, EDT_GetTableHitRegion returns ED_HIT_NONE in a cell */
  else if (leHit && leHit->type == LO_CELL)
  {
      Boolean shift, ctrl;
      xfe_GetShiftAndCtrl(event, &shift, &ctrl);

      EDT_SelectTableElement(context, x, y,
                             leHit, ED_HIT_SEL_CELL,
                             ctrl,      /* bModifierKeyPressed */
                             shift);    /*bExtendSelection*/
      return;
  }

  EDT_StartSelection(context, x, y);
}

void
fe_EditorSelectionBegin(MWContext* context, XEvent *event)
{
    unsigned long x, y;
    Time time;
    LO_Element*   leHit = 0;
    ED_HitType    iTableHit; 

    /* don't do this -- Japanese input method requires focus
    fe_NeutralizeFocus (context);
     */

    if (CONTEXT_DATA(context)->editor.ascroll_data.timer_id) {
        XtRemoveTimeOut(CONTEXT_DATA(context)->editor.ascroll_data.timer_id);
        CONTEXT_DATA(context)->editor.ascroll_data.timer_id = 0;
    }

    if (CONTEXT_DATA(context)->clicking_blocked
        || CONTEXT_DATA (context)->synchronous_url_dialog)
    {
        fe_Bell(context);
        return;
    }

    time = fe_getTimeFromEvent(context, event); /* get the time */
  
    fe_EventLOCoords(context, event, &x, &y);

#ifdef DONT_rhess
    /* 
     *  WARNING... [ nuke the primary selection owner ]
     */
    {
        Display *dpy = XtDisplay(CONTEXT_WIDGET(context));
        Window  owner = XGetSelectionOwner(dpy, XA_PRIMARY);

        if (owner != None) {
            XSetSelectionOwner(dpy, XA_PRIMARY, None, time);
        }
    }
#endif

    /*
     *    Do this first, so the EDT_ClearSelection() in the call doesn't
     *    blow away what EDT_StartSelection() does!
     */
    fe_EditorDisownSelection(context, time, False);

    /* see if we're in a table first... */
    iTableHit = EDT_GetTableHitRegion(context, x, y, &leHit, False);
    if ( iTableHit != ED_HIT_NONE )
    {
        Boolean shift, ctrl;
        xfe_GetShiftAndCtrl(event, &shift, &ctrl);
        EDT_SelectTableElement(context, x, y,
                               leHit, iTableHit,
                               ctrl,    /* bModifierKeyPressed */
                               shift);  /*bExtendSelection*/
        return;
    }
    /* for some reason, EDT_GetTableHitRegion returns ED_HIT_NONE in a cell */
    else if (leHit && leHit->type == LO_CELL)
    {
        Boolean shift, ctrl;
        xfe_GetShiftAndCtrl(event, &shift, &ctrl);

        EDT_SelectTableElement(context, x, y,
                               leHit, ED_HIT_SEL_CELL,
                               ctrl,      /* bModifierKeyPressed */
                               shift);    /*bExtendSelection*/
        return;
    }

    else
        /* need to implement disown */
        EDT_StartSelection(context, x, y);
}

void
fe_EditorSelectionExtend(MWContext* context, XEvent *event)
{
    Time       time;
    unsigned long x;
    unsigned long y;
    LO_Element*   leHit = 0;
    ED_HitType    iTableHit; 

    time = fe_getTimeFromEvent(context, event); /* get the time */

    /* don't do this -- Japanese input method requires focus
     fe_NeutralizeFocus(context);
     */

    fe_EventLOCoords (context, event, &x, &y);

    if (CONTEXT_DATA(context)->editor.ascroll_data.timer_id) {
        XtRemoveTimeOut(CONTEXT_DATA(context)->editor.ascroll_data.timer_id);
        CONTEXT_DATA(context)->editor.ascroll_data.timer_id = 0;
    }

    fe_editor_keep_pointer_visible(context, x, y);

#ifdef DONT_rhess
    /* 
     *  WARNING... [ nuke the primary selection owner ]
     */
    {
        Display  *dpy = XtDisplay(CONTEXT_WIDGET(context));
        Window  owner = XGetSelectionOwner(dpy, XA_PRIMARY);

        if (owner != None) {
            XSetSelectionOwner(dpy, XA_PRIMARY, None, time);
        }
    }
#endif

    /* see if we're in a table first... */
    iTableHit = EDT_GetTableHitRegion(context, x, y, &leHit, False);

    if (leHit && leHit->type == LO_CELL)
    {
        Boolean shift, ctrl;
        xfe_GetShiftAndCtrl(event, &shift, &ctrl);

        EDT_SelectTableElement(context, x, y,
                               leHit, ED_HIT_SEL_CELL,
                               ctrl,      /* bModifierKeyPressed */
                               TRUE);    /*bExtendSelection*/
        return;
    }

    /* need to implement own */
    EDT_ExtendSelection(context, x, y);

#ifdef DONT_rhess
    fe_EditorOwnSelection (context, time, False, False);
#endif

#if 0
    /* Making a selection turns off "Save Next" mode. */
    if (CONTEXT_DATA (context)->save_next_mode_p)
		{
			XBell (XtDisplay (widget), 0);
			CONTEXT_DATA (context)->save_next_mode_p = False;
			fe_SetCursor (context, False);
			XFE_Progress (context, fe_globalData.click_to_save_cancelled_message);
		}
#endif
}

void
fe_EditorSelectionEnd(MWContext *context, XEvent *event)
{
    Time time;
    unsigned long x;
    unsigned long y;
    LO_Element*   leHit = 0;
    ED_HitType    iTableHit; 

    fe_UserActivity(context);
    time = fe_getTimeFromEvent(context, event); /* get the time */

    fe_EventLOCoords (context, event, &x, &y);

    if (CONTEXT_DATA(context)->editor.ascroll_data.timer_id) {
		XtRemoveTimeOut(CONTEXT_DATA(context)->editor.ascroll_data.timer_id);
		CONTEXT_DATA(context)->editor.ascroll_data.timer_id = 0;
	}

    /* see if we're in a table first... */
    iTableHit = EDT_GetTableHitRegion( context, x, y, &leHit, FALSE );
    if ( iTableHit == ED_HIT_SEL_TABLE
        || iTableHit == ED_HIT_SEL_ALL_CELLS
        || iTableHit == ED_HIT_SEL_COL
        || iTableHit == ED_HIT_SEL_ROW )
    {
        Boolean shift, ctrl;
        xfe_GetShiftAndCtrl(event, &shift, &ctrl);
        EDT_SelectTableElement(context, x, y,
                               leHit, iTableHit,
                               ctrl,    /* bModifierKeyPressed */
                               shift);  /*bExtendSelection*/
        return;
    }

    /* need to implement own */
    EDT_EndSelection(context, x, y);

    fe_EditorOwnSelection(context, time, False, False);
}

#define XFE_EDITOR_MAX_INSERT_SIZE 512

void
fe_EditorKeyInsert(MWContext* context, Widget widget, XEvent* event)
{
    char       buf[XFE_EDITOR_MAX_INSERT_SIZE + 1];
    KeySym     keysym;
    Status     status_return;
    int        len;
    EDT_ClipboardResult result = EDT_COP_OK;
	INTL_CharSetInfo c = LO_GetDocumentCharacterSetInfo(context);

    if (widget != CONTEXT_DATA(context)->drawing_area) {
#ifdef DEBUG
		(void) fprintf(real_stderr,
					   "fe_editor_key_input_action got wrong widget (%s)\n",
					   XtName(widget));
#endif /* DEBUG */
		XtSetKeyboardFocus(CONTEXT_WIDGET(context),
						   CONTEXT_DATA(context)->drawing_area);
		return;
    }
    len = XmImMbLookupString(widget,
							 (XKeyEvent *)event,
							 buf,
							 XFE_EDITOR_MAX_INSERT_SIZE, 
							 &keysym,
							 &status_return);
    
    if (status_return == XBufferOverflow || len > XFE_EDITOR_MAX_INSERT_SIZE)
        return;

    if (len > 0) {
		char *str;

		buf[len] = 0;
		str = (char *) fe_ConvertFromLocaleEncoding(INTL_GetCSIWinCSID(c),
													(unsigned char *) buf);
		if (str != buf) {
			len = strlen(str);
		}

		if (len == 1) {
			result = EDT_KeyDown(context, str[0], 0, 0);
		} else if (len > 1) {
			result = EDT_PasteText(context, str);
		}

		if (result != EDT_COP_OK) {
			fe_editor_report_copy_error(context, result);
		}

		if (str != buf && str != NULL) {
			XP_FREE(str);
		}
    }
}

void
fe_EditorTab(MWContext* context, Boolean forward, Boolean force)
{
	EDT_ClipboardResult result = EDT_TabKey(context, forward, force);
	
	if (result != EDT_COP_OK)
		fe_editor_report_copy_error(context, result);
}

/*
 *    Editor specific action handlers.
 */
static void
fe_ActionWrongContextAlert(MWContext*, XtActionProc, String*, Cardinal*);

#ifdef DEBUG
#define FE_ACTION_ALERT(c, p, a, n) fe_ActionWrongContextAlert((c),(p),(a),(n))
#else
#define FE_ACTION_ALERT(c, p, a, n)
#endif

#define FE_ACTION_VALIDATE(proc)                  \
if (!(context) || !EDT_IS_EDITOR(context)) {      \
    FE_ACTION_ALERT(context, (proc), av, ac);     \
    return;                                       \
}


#define FE_SYNTAX_ERROR(proc) fe_ActionSyntaxAlert(context, (proc), av, ac)

/*=========================== ACTIONS =================================*/
static void
fe_get_selected_text_rect(MWContext* context,
			  LO_TextStruct* text,
			  int32*         rect_x,
			  int32*         rect_y,
			  int32*         rect_width,
			  int32*         rect_height)
{
    PA_Block    text_save = text->text;
    int         len_save = text->text_len;
    LO_TextInfo info;
    
    text->text_len = text->sel_start;
    XFE_GetTextInfo(context, text, &info);

    *rect_x = text->x + text->x_offset + info.max_width;
    *rect_y = text->y + text->y_offset;

    text->text = (PA_Block)((char*)text->text + text->sel_start);
    text->text_len = text->sel_end - text->sel_start;
    
    XFE_GetTextInfo(context, text, &info);

    *rect_width = info.max_width;
    *rect_height = info.ascent + info.descent;

    text->text = text_save;
    text->text_len = len_save;
}

static Boolean
fe_editor_selection_contains_point(MWContext* context, int32 x, int32 y)
{
    int32 start_selection;
    int32 end_selection;
    LO_Element* start_element = NULL;
    LO_Element* end_element = NULL;
    LO_Element* lo_element;
    int32 rect_x;
    int32 rect_y;
    int32 rect_width;
    int32 rect_height;
    CL_Layer *layer;

    if (!EDT_IsSelected(context))
	return FALSE;

    LO_GetSelectionEndpoints(context,
			     &start_element,
			     &end_element,
			     &start_selection,
			     &end_selection,
                             &layer);

    if (start_element == NULL)
	return FALSE;

    for (lo_element = start_element;
	 lo_element != NULL;
	 lo_element = ((LO_Any *)lo_element)->next) {

	if (lo_element->type == LO_TEXT &&
	    (lo_element == start_element || lo_element == end_element)) {
	    LO_TextStruct* text = (LO_TextStruct*)lo_element;

	    if (text->text == NULL) {
		if (text->prev != NULL && text->prev->type == LO_TEXT) {
		    text = (LO_TextStruct*)text->prev;
		} else {
		    text = (LO_TextStruct*)text->next;
		}
	    }
	    
	    if (text->text == NULL)
		continue;
	    
	    fe_get_selected_text_rect(context, text,
				      &rect_x,
				      &rect_y,
				      &rect_width,
				      &rect_height);
	    
	} else if (lo_element->type == LO_LINEFEED) {
	    continue;
	    
	} else {
	    LO_Any* lo_any = (LO_Any*)lo_element;
	    rect_x = lo_any->x + lo_any->x_offset;
	    rect_y = lo_any->y + lo_any->y_offset;
	    rect_width = lo_any->width;
	    rect_height = lo_any->height;
	}
	
	if (x > rect_x && y > rect_y &&
	    x < (rect_x + rect_width) && y < (rect_y + rect_height))
	    return TRUE;

	if (lo_element == end_element)
	    break;
    }

    return FALSE;
}

Boolean
fe_EditorSelectionContainsPoint(MWContext* context, int32 x, int32 y)
{
	return fe_editor_selection_contains_point(context, x, y);
}

static char*
fe_lo_image_anchor(LO_ImageStruct* image,
		     long x, long y,
		     MWContext* context)
{
    if (image->is_icon &&
        image->icon_number == IL_IMAGE_DELAYED) { /* delayed image */ 
		long width, height;

		fe_IconSize(IL_IMAGE_DELAYED, &width, &height);

		if (image->alt &&
			image->alt_len &&
			(x > image->x + image->x_offset + 1 + 4 + width)) {
			if (image->anchor_href) {
				return (char *)image->anchor_href->anchor;
			}
		} /* else fall to end */
	} 

	/*
	 *    Internal editor image maybe?
	 */
	else if (image->is_icon &&
			 (image->icon_number == IL_EDIT_NAMED_ANCHOR ||
			  image->icon_number == IL_EDIT_FORM_ELEMENT ||
			  image->icon_number == IL_EDIT_UNSUPPORTED_TAG ||
			  image->icon_number == IL_EDIT_UNSUPPORTED_END_TAG)) {

		if (image->alt != NULL)
			return (char*)image->alt;
		else
			return "";
    }

    /*
     * This would be a client-side usemap image.
     */
    else if (image->image_attr->usemap_name != NULL) {
	
		LO_AnchorData *anchor_href;
		long ix = image->x + image->x_offset;
		long iy = image->y + image->y_offset;
		long mx = x - ix - image->border_width;
		long my = y - iy - image->border_width;
	
		anchor_href = LO_MapXYToAreaAnchor(context,
										   (LO_ImageStruct *)image,
										   mx, my);
		if (anchor_href) {
			return (char *)anchor_href->anchor;
		} /* else fall to end */
    } else { /* some old random image */
		if (image->anchor_href) {
			return (char *)image->anchor_href->anchor;
		}
    }

    return NULL;
}

static LO_Element*   fe_editor_motion_action_last_le;
static Boolean       fe_editor_motion_action_last_le_is_image;
static LO_Element*   fe_editor_motion_action_last_hit_le;
static ED_HitType    fe_editor_motion_action_last_hit_type;

static void
fe_editor_motion_action(Widget widget, XEvent *event,
			String *av, Cardinal *ac)
{
    MWContext*    context = fe_MotionWidgetToMWContext(widget);
    LO_Element*   le;
    LO_Element*   leHit;
    ED_HitType    iTableHit; 
    unsigned long x, y;
    Cursor        cursor = None;
    char*         progress_string = NULL;
    char buf[80];
    char num[32];

    FE_ACTION_VALIDATE(fe_editor_motion_action);

    fe_EventLOCoords(context, event, &x, &y); /* get the doc co-ords */

    /* NOTE:  let's see if we're in table stuff first... */
    iTableHit = EDT_GetTableHitRegion( context, 
                                       x, y, 
                                       &leHit,
                                       FALSE );
    if ( iTableHit ) { 
        if ((fe_editor_motion_action_last_hit_type == iTableHit) &&
            (fe_editor_motion_action_last_hit_le == leHit)) {
            return;  /* NOTE:  no change... */
        }

        fe_editor_motion_action_last_hit_le   = leHit;
        fe_editor_motion_action_last_hit_type = iTableHit;

        switch ( iTableHit ) { 
            case ED_HIT_SEL_TABLE:
#ifdef DEBUG_motion
                fprintf(stderr, "ED_HIT_SEL_TABLE:\n");
#endif
                cursor = CONTEXT_DATA(context)->tab_sel_cursor;
                break; 

            case ED_HIT_SEL_ALL_CELLS: 
#ifdef DEBUG_motion
                fprintf(stderr, "ED_HIT_SEL_ALL_CELLS:\n");
#endif
                cursor = None;
                break; 

            case ED_HIT_SEL_COL:  
#ifdef DEBUG_motion
                fprintf(stderr, "ED_HIT_SEL_COL:\n");
#endif
                cursor = CONTEXT_DATA(context)->col_sel_cursor;
                break; 

            case ED_HIT_SEL_ROW:  
#ifdef DEBUG_motion
                fprintf(stderr, "ED_HIT_SEL_ROW:\n");
#endif
                cursor = CONTEXT_DATA(context)->row_sel_cursor;
                break; 

            case ED_HIT_SEL_CELL:  
#ifdef DEBUG_motion
                fprintf(stderr, "ED_HIT_SEL_CELL:\n");
#endif
                cursor = CONTEXT_DATA(context)->cel_sel_cursor;
                break; 

#ifdef ED_HIT_SIZE_TABLE
            case ED_HIT_SIZE_TABLE:  
#ifdef DEBUG_motion
                fprintf(stderr, "ED_HIT_SIZE_TABLE:\n");
#endif
                cursor = CONTEXT_DATA(context)->resize_tab_cursor;
                break; 
#endif /* ED_HIT_SIZE_TABLE */

            case ED_HIT_SIZE_COL:  
#ifdef DEBUG_motion
                fprintf(stderr, "ED_HIT_SIZE_COL:\n");
#endif
                cursor = CONTEXT_DATA(context)->resize_col_cursor;
                break;

            case ED_HIT_ADD_ROWS:  
#ifdef DEBUG_motion
                fprintf(stderr, "ED_HIT_ADD_ROWS:\n");
#endif
                cursor = CONTEXT_DATA(context)->add_row_cursor;
                break; 

            case ED_HIT_ADD_COLS:  
#ifdef DEBUG_motion
                fprintf(stderr, "ED_HIT_ADD_COLS:\n");
#endif
                cursor = CONTEXT_DATA(context)->add_col_cursor;
                break; 

            default:  
                /* NOTE:  we shouldn't ever get here... */
#ifdef DEBUG_motion
                fprintf(stderr, "fe_editor_motion_action default:  WARNING ***************\n");
#endif
                cursor = None;
        } 
    } 
    else {
        /* NOTE:  let's check for the other elements... */

        le = LO_XYToElement(context, (int32)x, (int32)y, NULL);

        if (le == fe_editor_motion_action_last_le &&
            !fe_editor_motion_action_last_le_is_image &&
            (fe_editor_motion_action_last_hit_type == 0)) {
            return; /* NOTE:  no change... */
        }

        fe_editor_motion_action_last_hit_le   = NULL;
        fe_editor_motion_action_last_hit_type = 0;

        fe_editor_motion_action_last_le_is_image = FALSE;
        fe_editor_motion_action_last_le = le;
    
        if (le != NULL) {
            /*  Are we over text? Special cursor */
            if (le->type == LO_TEXT) {
                cursor = CONTEXT_DATA(context)->editable_text_cursor;
                if (le->lo_text.anchor_href) { /* link */
                    progress_string = (char *)le->lo_text.anchor_href->anchor;
                }
                /*  Over image, special progress */
            } 
            else {
                if (le->type == LO_IMAGE) {
                    progress_string = fe_lo_image_anchor(&le->lo_image, 
                                                         /* link ? */
                                                         x, y,
                                                         context);
                    if (progress_string != NULL && progress_string[0] == '\0')
                        progress_string = NULL;
					
                    if (progress_string == NULL) {
						
                        if (le->lo_image.image_url) {
                            progress_string = (char*)le->lo_image.image_url;
                        } else if (le->lo_image.lowres_image_url) {
                            progress_string = (char*)le->lo_image.lowres_image_url;
                        }
					
                        if (progress_string != NULL) {
                            sprintf(num, ",%d,%d",
                                    x - le->lo_image.x, y - le->lo_image.y);
                            FE_CondenseURL(buf, progress_string,
                                           sizeof(buf) - 1 - strlen(num));
                            strcat(buf, num);
                            progress_string = buf;
                            fe_editor_motion_action_last_le_is_image = TRUE;
                        }
                    }
                }
            }
        }
    }

    if (CONTEXT_DATA(context)->clicking_blocked ||
		CONTEXT_DATA(context)->synchronous_url_dialog) {
		cursor = CONTEXT_DATA(context)->busy_cursor;
    }

    if (CONTEXT_DATA(context)->drawing_area) {
		XDefineCursor(XtDisplay(CONTEXT_DATA(context)->drawing_area),
					  XtWindow(CONTEXT_DATA(context)->drawing_area),
					  cursor);
    }

    if (progress_string == NULL)
		progress_string = "";

    if (CONTEXT_DATA(context)->active_url_count == 0) {
		/* If there are transfers in progress, don't document the URL under
		   the mouse, since that message would interfere with the transfer
		   messages.  Do change the cursor, however. */
		fe_MidTruncatedProgress(context, progress_string);
    }
}

#define EA_PREFIX "editor-"

static XtActionsRec fe_editor_actions [] =
{
	{ EA_PREFIX "motion", fe_editor_motion_action },
};

static void
fe_ActionWrongContextAlert(MWContext*    context,
			   XtActionProc  action_proc,
			   String* av, Cardinal* ac)
{
    char buf[1024];
    char name[1024];

    fe_action_name(name,
		   fe_editor_actions, countof(fe_editor_actions),
		   action_proc, av, ac);
    sprintf(buf, XP_GetString(XFE_ACTION_WRONG_CONTEXT), name);

    if (context)
	FE_Alert(context, buf);
    else
	fprintf(real_stderr, buf);
}

void
fe_EditorInitActions()
{
	XtAppAddActions(fe_XtAppContext,
					fe_editor_actions,
					countof(fe_editor_actions)
					);
}

static void
fe_EditURLCallback(const char* urlToOpen)
{
	/*
	 *    Try to get editor first.  XP_FindSomeContext() does not look for
	 *    editor context.
	 */
	MWContext *context = XP_FindContextOfType(NULL, MWContextEditor);
	if (!context) {
		context = XP_FindSomeContext();
	}

	/* This shouldn't happen. */
	if (!context) {
		XP_ASSERT(0);
		return;
	}

	/*
	 *   If urlToOpen is already being edited, this will just pop it to
	 *   front.
	 */
	fe_EditorEdit(context, NULL, NULL, (char*)urlToOpen);
}

void
fe_EditorStaticInit()
{
	EDTPLUG_RegisterEditURLCallback(&fe_EditURLCallback);
}

/*
 * Note: this routine is untested.  It's called for dragging columns,
 * not for highlighting selected columns.
 */
void
FE_DisplayAddRowOrColBorder(MWContext* context, XP_Rect *pRect,
                            XP_Bool bErase)
{
    XGCValues gcv;
    unsigned long gc_flags;
    GC gc;
    fe_Drawable *fe_drawable = CONTEXT_DATA(context)->drawable;

#ifdef DEBUG_akkana
    printf("FE_DisplayAddRowOrColBorder\n");
#endif /* DEBUG_akkana */

    if (bErase)
    {
        gc_flags = GCForeground /* | GCLineWidth */ | GCLineStyle;
        gcv.foreground = CONTEXT_DATA(context)->select_bg_pixel;
    }
    else
    {
        gc_flags = GCForeground /* | GCLineWidth */ | GCLineStyle;
        gcv.foreground = CONTEXT_DATA(context)->select_bg_pixel;
        gcv.line_style = LineOnOffDash;
    }
    /*gcv.line_width = iSelectionBorderThickness;*/
    gc = fe_GetGCfromDW(fe_display, fe_drawable->xdrawable,
                        gc_flags, &gcv, fe_drawable->clip_region);
    XDrawRectangle(fe_display,
                   XtWindow(CONTEXT_DATA(context)->drawing_area), gc,
                   pRect->left, pRect->top,
                   pRect->left - pRect->right, pRect->bottom - pRect->top);
}

void 
FE_DisplayEntireTableOrCell(MWContext* context, LO_Element* pLoElement)
{
    LO_TableStruct* table;

    if (pLoElement->lo_any.type == LO_TABLE)
    {
        XFE_DisplayTable(context, 0, &(pLoElement->lo_table));
    }
    else if (pLoElement->lo_any.type == LO_CELL)
    {
        XFE_DisplayCell(context, 0, &(pLoElement->lo_cell));
    }
    else        /* shouldn't happen */
    {
#ifdef DEBUG_akkana
        printf("FE_DisplayEntireTableOrCell() with type %d\n",
               pLoElement->lo_any.type);
#endif /* DEBUG_akkana */
        table = lo_GetParentTable(context, pLoElement);
        XFE_DisplayTable(context, 0, table);
    }
}


#endif /* EDITOR */
