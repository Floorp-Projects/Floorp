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
   dialogs.c --- General UI functions used elsewhere.
   Created: Jamie Zawinski <jwz@netscape.com>, 23-Jun-94.
 */


/* #define DOCINFO_SOURCE_TEXT */
#define DOCINFO_VISUAL_TEXT

#include "mozilla.h"
#include "net.h"             /* for fe_makeSecureTitle */
#include "xlate.h"
#include "xfe.h"
#include "felocale.h"
#include "outline.h"
#include "mailnews.h"

#include <Xm/FileSBP.h>      /* for hacking FS lossage */

#include <Xm/XmAll.h>
#include <Xm/CascadeB.h>

#include <Xfe/XfeP.h>			/* for xfe widgets and utilities */

#include "libi18n.h"
#include "intl_csi.h"

#include "np.h"
#include "xp_trace.h"
#include <layers.h>
#include "xeditor.h"
#include "xp_qsort.h"

/* for XP_GetString() */
#include <xpgetstr.h>
extern int XFE_OPEN_FILE;
extern int XFE_ERROR_OPENING_FILE;
extern int XFE_ERROR_OPENING_PIPE;
extern int XFE_NO_SUBJECT;
extern int XFE_UNKNOWN_ERROR_CODE;
extern int XFE_INVALID_FILE_ATTACHMENT_DOESNT_EXIST;
extern int XFE_INVALID_FILE_ATTACHMENT_NOT_READABLE;
extern int XFE_INVALID_FILE_ATTACHMENT_IS_A_DIRECTORY;
extern int XFE_ERROR_SAVING_OPTIONS;
extern int XFE_X_RESOURCES_NOT_INSTALLED_CORRECTLY;
extern int XFE_OUTBOX_CONTAINS_MSG;
extern int XFE_OUTBOX_CONTAINS_MSGS;
extern int XFE_CONTINUE_MOVEMAIL;
extern int XFE_CANCEL_MOVEMAIL;
extern int XFE_MOVEMAIL_EXPLANATION;
extern int XFE_SHOW_NEXT_TIME;
extern int XFE_MAIL_SPOOL_UNKNOWN;
extern int XFE_CANT_SAVE_PREFS;
extern int XFE_JAVASCRIPT_APP;
extern int XFE_DIALOGS_PRINTING;
extern int XFE_DIALOGS_DEFAULT_VISUAL_AND_COLORMAP;
extern int XFE_DIALOGS_DEFAULT_VISUAL_AND_PRIVATE_COLORMAP;
extern int XFE_DIALOGS_NON_DEFAULT_VISUAL;
extern int XFE_DIALOGS_FROM_NETWORK;
extern int XFE_DIALOGS_FROM_DISK_CACHE;
extern int XFE_DIALOGS_FROM_MEMORY_CACHE;
extern int XFE_DIALOGS_DEFAULT;

#if XmVersion >= 2000
extern void _XmOSBuildFileList(String,String,unsigned char,String * *,
			       unsigned int *,unsigned int *);

extern char * _XmStringGetTextConcat(XmString);

extern int _XmOSFileCompare(XmConst void *,XmConst void *);

extern String _XmOSFindPatternPart(String);

extern void _XmOSQualifyFileSpec(String,String,String *,String *);

extern XmGeoMatrix _XmGeoMatrixAlloc(unsigned int,unsigned int,unsigned int);

extern Boolean _XmGeoSetupKid(XmKidGeometry,Widget);

extern void _XmMenuBarFix(XmGeoMatrix,int,XmGeoMajorLayout,XmKidGeometry);

extern void _XmSeparatorFix(XmGeoMatrix,int,XmGeoMajorLayout,XmKidGeometry);

extern void _XmDestroyParentCallback(Widget,XtPointer,XtPointer);

#endif /* XmVersion >= 2000 */


#define DOCINFO_CHARSET_TEXT

/* Kludge around conflicts between Motif and xp_core.h... */
#undef Bool
#define Bool char

typedef enum {
  Answer_Invalid = -1,
  Answer_Cancel = 0,
  Answer_OK,
  Answer_Apply,
  Answer_Destroy } Answers;

struct fe_confirm_data {
  MWContext *context;
  Answers answer;
  void *return_value;
  void *return_value_2;
  Widget widget;
  Widget text, text2;
  Boolean must_match;
};

#ifdef MOZ_MAIL_NEWS
extern void fe_getMessageBody(MWContext *context, char **pBody, uint32 *body_size, MSG_FontCode **font_changes);
extern void fe_doneWithMessageBody(MWContext *context, char *pBody, uint32 body_size);
#endif

extern const char* FE_GetFolderDirectory(MWContext* context);

/*static void fe_confirm_cb (Widget, XtPointer, XtPointer);*/
static void fe_clear_text_cb (Widget, XtPointer, XtPointer);
static void fe_destroy_cb (Widget, XtPointer, XtPointer);
static void fe_destroy_ok_cb (Widget, XtPointer, XtPointer);
static void fe_destroy_apply_cb (Widget, XtPointer, XtPointer);
static void fe_destroy_cancel_cb (Widget, XtPointer, XtPointer);
static void fe_destroy_finish_cb (Widget, XtPointer, XtPointer);

static void fe_destroy_snarf_text_cb (Widget, XtPointer, XtPointer);
static void fe_destroy_snarf_pw_cb (Widget, XtPointer, XtPointer);
static void fe_destroy_snarf_pw2_cb (Widget, XtPointer, XtPointer);

void fe_browse_file_of_text (MWContext *context, Widget text_field, Boolean dirp);

/*
 *    A real Info dialog - with ! instead of error icon.
 *    Now we don't have to have "error: no new messages on server"
 */
void
FE_Message(MWContext * context, const char* message)
{
	if (context && context->type != MWContextBiff)
		fe_Message(context, message);
}

/* FE_Alert is no longer defined in fe_proto.h */
void
FE_Alert (MWContext *context, const char *message)
{
  if (context && context->type == MWContextBiff)
	return;

  if (context)
      XFE_Alert (context, message);
  else
    {
      Widget toplevel = FE_GetToplevelWidget();
      if ( toplevel ) {
	      fe_Alert_2(toplevel, message);
      } else {
	  /* So that didn't even work. Write to stdout and
	   * exit.
	   */
	  XP_ABORT((message));
	}
    }
}

/* Lame hack to get the right title for javascript dialogs.
 * This should be instead added to the call arguments.
 */
static MWContext *javaScriptCallingContextHack = 0;

/* Display a message, and wait for the user to click "Ok".
   A pointer to the string is not retained.
   The call to FE_Alert() returns immediately - it does not
   wait for the user to click OK.
 */
void
XFE_Alert (MWContext *context, const char *message)
{
  /* Keep the context around, so we can pull the domain name
   * full the dialog title.
   */
  if (context->bJavaScriptCalling)
	javaScriptCallingContextHack = context;

  fe_Alert_2 (CONTEXT_WIDGET (context), message);
}

/* Just like XFE_Alert, but with a different dialog title. */
void
fe_stderr (MWContext *context, const char *message)
{
  (void) fe_dialog (CONTEXT_WIDGET (context),
		    (fe_globalData.stderr_dialog_p &&
		     fe_globalData.stdout_dialog_p
		     ? "stdout_stderr"
		     : fe_globalData.stderr_dialog_p ? "stderr"
		     : fe_globalData.stdout_dialog_p ?  "stdout"  : "???" ),
		    message, FALSE, 0, FALSE, FALSE, 0);
}


/* Let the user confirm or deny assertion `message' (returns True or False.)
   A pointer to the prompt-string is not retained.
 */
XP_Bool
XFE_Confirm (MWContext *context, const char *message)
{
  return fe_Confirm_2 (CONTEXT_WIDGET (context), message);
}

Boolean
fe_Confirm_2 (Widget parent, const char *message)
{
  return (Bool) ((int) fe_dialog (parent, "question", message,
				  TRUE, 0, TRUE, FALSE, 0));
}

void
fe_Alert_2 (Widget parent, const char *message)
{
  (void) fe_dialog (parent, "error", message, FALSE, 0, FALSE, FALSE, 0);
}

#if !defined(__FreeBSD__) && !defined(MKLINUX) && !defined(LINUX_GLIBC_2)
#include <sys/errno.h>
extern char *sys_errlist[];
extern int sys_nerr;
#endif

/* Like perror, but with a dialog.
 */
void
fe_perror (MWContext *context, const char *message)
{
  fe_perror_2 (CONTEXT_WIDGET (context), message);
}

void
fe_perror_2 (Widget parent, const char *message)
{
  int e = errno;
  char *es = 0;
  char buf1 [2048];
  char buf2 [512];
  char *b = buf1;
  if (e >= 0 && e < sys_nerr)
    {
      es = sys_errlist [e];
    }
  else
    {
      PR_snprintf (buf2, sizeof (buf2), XP_GetString( XFE_UNKNOWN_ERROR_CODE ),
		errno);
      es = buf2;
    }
  if (message)
    PR_snprintf (buf1, sizeof (buf1), "%.900s\n%.900s", message, es);
  else
    b = buf2;
  fe_Alert_2 (parent, b);
}

void
fe_UnmanageChild_safe (Widget w)
{
  if (w) XtUnmanageChild (w);
}


void
fe_NukeBackingStore (Widget widget)
{
  XSetWindowAttributes attrs;
  unsigned long attrmask;

  if (!XtIsTopLevelShell (widget))
    widget = XtParent (widget);
  XtRealizeWidget (widget);

  attrmask = CWBackingStore | CWSaveUnder;
  attrs.backing_store = NotUseful;
  attrs.save_under = False;
  XChangeWindowAttributes (XtDisplay (widget), XtWindow (widget),
			   attrmask, &attrs);
}


Widget

#ifdef OSF1
fe_CreateTextField (Widget parent, char *name, Arg *av, int ac)
#else
fe_CreateTextField (Widget parent, const char *name, Arg *av, int ac)
#endif
{
  Widget w;

#if 1
  w = XmCreateTextField (parent, (char *) name, av, ac);
#else
  XtSetArg (av[ac], XmNeditMode, XmSINGLE_LINE_EDIT); ac++;
  w = XmCreateTextField (parent, (char *) name, av, ac);
#endif

  fe_HackTextTranslations (w);
  return w;
}

Widget
fe_CreateText (Widget parent, const char *name, Arg *av, int ac)
{
  Widget w = XmCreateText (parent, (char *) name, av, ac);
  fe_HackTextTranslations (w);
  return w;
}


static void
fe_select_text(Widget text)
{
  XmTextPosition first = 0;
  XmTextPosition last = XmTextGetLastPosition (text);
  XP_ASSERT (XtIsRealized(text));
  XmTextSetSelection (text, first, last,
		      XtLastTimestampProcessed (XtDisplay (text)));
}


/* The purpose of this function is try disable all grabs and settle
 * focus issues. This will be called before we popup a dialog
 * (modal or non-modal). This needs to do all these:
 *	- if a menu was posted, unpost it.
 *	- if a popup menu was up, pop it down.
 *	- if an option menu, pop it down.
 */
void fe_fixFocusAndGrab(MWContext *context)
{
  Widget focus_w;
  Widget mainw;
  XEvent event;
  int i;

  mainw = CONTEXT_WIDGET(context);
  focus_w = XmGetFocusWidget(mainw);

  /* Unpost Menubar */
  if (focus_w && XmIsCascadeButton(focus_w) &&
      XmIsRowColumn(XtParent(focus_w))) {
    /* 1. Found the menubar. Unpost it.
     *		To do that, we makeup a dummy event and use it with the
     *		CleanupMenuBar() action.
     * WARNING: if focus_w was a XmCascadeButtonGadget we wont be able to
     *		do this.
     */
    event.xany.type = 0;
    event.xany.serial = 0;
    event.xany.send_event = 0;
    event.xany.display = fe_display;
    event.xany.window = XtWindow(focus_w);
    XtCallActionProc(focus_w, "CleanupMenuBar", &event, NULL, 0);
  }

  /* Identify and Popdown any OptionMenu that was active */
  if (focus_w && XmIsRowColumn(XtParent(focus_w))) {
    unsigned char type;
    Widget w;
    XtVaGetValues(XtParent(focus_w), XmNrowColumnType, &type, 0);
    if (type == XmMENU_OPTION) {
      XtVaGetValues(focus_w, XmNsubMenuId, &w, 0);
      if (w) XtUnmanageChild(w);
    }
  }
  
  /* Identify and Popdown any popup menus that were active */
  for (i=0; i < XfeNumPopups(mainw); i++) {
    Widget popup = XfePopupListIndex(mainw,i);
    if (XtIsShell(popup) && XmIsMenuShell(popup))
      if (XfeShellIsPoppedUp(popup)) {
#ifdef DEBUG_dora
	  printf("popdown... name %s shell is popped up\n", XtName(popup));
#endif
	XtPopdown(popup);
      }
  }
}

static char *fe_makeSecureTitle( MWContext *context )
{
  History_entry *h;
  char *domain = 0;
  char *title = 0;

  if( !context )
	{
	  return title;
	}
  
  h = SHIST_GetCurrent (&context->hist);
  
  if (!h || !h->address) return title;
  
  domain = NET_ParseURL(h->address, GET_HOST_PART);

  if (domain) {
	title = PR_smprintf("%s - %s", domain, XP_GetString(XFE_JAVASCRIPT_APP));
	XP_FREE(domain);
  } else {
	title = PR_smprintf("%s", XP_GetString(XFE_JAVASCRIPT_APP));
  }
  
  return title;
}

void *
fe_prompt (MWContext *context, Widget mainw,
	   const char *title, const char *message,
	   XP_Bool question_p, const char *default_text,
	   XP_Bool wait_p, XP_Bool select_p,
	   char **passwd)
{
  /* Keep the context around, so we can pull the domain name
   * full the dialog title.
   */
  if (context->bJavaScriptCalling)
	javaScriptCallingContextHack = context;

  return fe_dialog(mainw, title, message, question_p,
				   default_text, wait_p, select_p, passwd);
}

/* This function is complete madness - it takes a billion flags
   and does everything in the world because somehow I thought it would save
   me a bunch of lines of code but it's just a MESS!
 */
void *
fe_dialog (Widget mainw,
	   const char *title, const char *message,
	   XP_Bool question_p, const char *default_text,
	   XP_Bool wait_p, XP_Bool select_p,
	   char **passwd)
{
  Widget shell, dialog;
  Widget text = 0, pwtext = 0;
  XmString xm_message = 0;
  Arg av [20];
  int ac;
  Visual *v = 0;
  Colormap cmap = 0;
  Cardinal depth = 0;
  char title2 [255];
  struct fe_MWContext_cons *cons;
  int i;
  char *javaScriptTitle = 0;  /* dialog title when javascript calls */
  XP_Bool javaScriptCalling = FALSE;
    
  strcpy (title2, title);
  strcat (title2, "_popup");

  /* Use this global variable hackery to get the context */
  if (javaScriptCallingContextHack) {
	javaScriptTitle = fe_makeSecureTitle(javaScriptCallingContextHack);
	javaScriptCallingContextHack = NULL;
    javaScriptCalling = TRUE;
  }

  if (!mainw) {
    /* Yikes.  Well, this can happen if someone calls FE_Alert() on a biff
       context or some other context without a window.  We could ignore it,
       but I think it better to try and put the dialog up *someplace*.  So,
       look for some context with a reasonable window. */
    for (cons = fe_all_MWContexts ; cons && !mainw; cons = cons->next) {
      mainw = CONTEXT_WIDGET(cons->context);
    }
    if (!mainw) return NULL;
  }

  while(!XtIsWMShell(mainw) && (XtParent(mainw)!=0))
    mainw = XtParent(mainw);

  /* so, if the context that is popping up the dialog is hidden, we need
	 to pop it up _somewhere_, so we run down the list of contexts *again*,
	 and pop it up over one of them.  This should work.  I think. */
  if (!XfeIsViewable(mainw))
	  {
		  Widget attempt = mainw;
		  /* instead of popping up the dialog at (0,0) pop it up over
			 the active context */
		  for (cons = fe_all_MWContexts ; 
			   cons && !XfeIsViewable(attempt); 
			   cons = cons->next) 
			  {
				  attempt = CONTEXT_WIDGET(cons->context);

				  while(!XtIsWMShell(attempt) && (XtParent(attempt)!=0))
					  attempt = XtParent(attempt);
			  }

		  /* oh well, we tried. */
		  if (attempt)
			  mainw = attempt;
	  }

  /* If any dialog is already up we will cascade these dialogs. Thus this
   * dialog will be the child of the last popped up dialog.
   */
  i = XfeNumPopups(mainw);
  while (i>0)
    if (XmIsDialogShell(XfePopupListIndex(mainw,i-1)) &&
		(XfeIsViewable(XfePopupListIndex(mainw,i-1)))) {
      /* Got a new mainw */
      Widget newmainw = XfePopupListIndex(mainw,i-1);
#ifdef DEBUG_dp
      fprintf(stderr, "Using mainw: %s[%x] (%s[%x] num_popup=%d)\n",
	      XtName(newmainw), newmainw,
	      XtName(mainw), mainw, i);
#endif /* DEBUG_dp */
      mainw = newmainw;
      i = XfeNumPopups(mainw);
    }
  else i--;

  /* Popdown any popup menu that was active. If not, this could get motif
   * motif really confused as to who has focus as the new dialog that we
   * are going to popup will undo a grab that the popup did without the
   * popup's knowledge.
   */
  for (cons = fe_all_MWContexts ; cons; cons = cons->next)
    fe_fixFocusAndGrab(cons->context);

  XtVaGetValues (mainw, XtNvisual, &v, XtNcolormap, &cmap,
		 XtNdepth, &depth, 0);

  ac = 0;
  XtSetArg (av[ac], XmNvisual, v); ac++;
  XtSetArg (av[ac], XmNcolormap, cmap); ac++;
  XtSetArg (av[ac], XmNdepth, depth); ac++;
  XtSetArg (av[ac], XmNallowShellResize, TRUE); ac++;
  XtSetArg (av[ac], XmNtransientFor, mainw); ac++;
  shell = XmCreateDialogShell (mainw, title2, av, ac);

  /* If it is java script that is posting this, use this special title */
  if (javaScriptTitle) {
	XtVaSetValues (shell, XmNtitle, javaScriptTitle, 0);
	free(javaScriptTitle);
  }

  ac = 0;
  if (message)
    xm_message = XmStringCreateLtoR ((char *) message,
				     XmFONTLIST_DEFAULT_TAG);
  XtSetArg (av[ac], XmNdialogStyle, (wait_p
				     ? XmDIALOG_FULL_APPLICATION_MODAL
				     : XmDIALOG_MODELESS)); ac++;
  XtSetArg (av[ac], XmNdialogType,
            (default_text
             ? XmDIALOG_MESSAGE
             : (question_p
                ? XmDIALOG_QUESTION
                : (javaScriptCalling
                   ? XmDIALOG_WARNING
                   : XmDIALOG_ERROR)))); ac++;
  if (xm_message) XtSetArg (av[ac], XmNmessageString, xm_message), ac++;
  XtSetArg (av[ac], XmNautoUnmanage, False); ac++;
  dialog = XmCreateMessageBox (shell, "dialog", av, ac);
  if (xm_message) XmStringFree (xm_message);

#ifdef NO_HELP
  fe_UnmanageChild_safe (XtNameToWidget (dialog, "*Help"));
#endif


  if (! question_p )
    {
      Widget cancel = 0;
      XtVaGetValues (dialog, XmNcancelButton, &cancel, 0);
      if (! cancel) abort ();
      XtUnmanageChild (cancel);
    }

  if (default_text)
    {
      Widget clear;
      Widget text_parent = dialog;
      Widget ulabel = 0, plabel = 0;

      text = 0;
      pwtext = 0;

      if (passwd && passwd != (char **) 1)
	{
	  ac = 0;
	  text_parent = XmCreateForm (dialog, "dialogform", av, ac);
	  ulabel = XmCreateLabelGadget (text_parent, "userLabel", av, ac);
	  plabel = XmCreateLabelGadget (text_parent, "passwdLabel", av, ac);
	  XtVaSetValues (ulabel,
			 XmNtopAttachment, XmATTACH_FORM,
			 XmNbottomAttachment, XmATTACH_WIDGET,
			 XmNbottomWidget, plabel,
			 XmNleftAttachment, XmATTACH_FORM,
			 XmNrightAttachment, XmATTACH_OPPOSITE_WIDGET,
			 XmNrightWidget, plabel,
			 0);
	  XtVaSetValues (plabel,
			 XmNtopAttachment, XmATTACH_NONE,
			 XmNbottomAttachment, XmATTACH_FORM,
			 XmNleftAttachment, XmATTACH_FORM,
			 XmNrightAttachment, XmATTACH_NONE,
			 0);
	}

      if (passwd != (char **) 1)
	{
	  ac = 0;
	  text = fe_CreateTextField (text_parent, "text", av, ac);
	  fe_SetTextField(text, default_text);
	  XtVaSetValues (text, XmNcursorPosition, 0, 0);
	}

      if (passwd)
	{
	  ac = 0;
	  pwtext = fe_CreateTextField (text_parent, "pwtext", av, ac);
	}

      if (text && pwtext)
	{
	  if (fe_globalData.nonterminal_text_translations)
	    XtOverrideTranslations (text, fe_globalData.
				    nonterminal_text_translations);
				    
	  XtVaSetValues (text,
			 XmNtopAttachment, XmATTACH_FORM,
			 XmNbottomAttachment, XmATTACH_WIDGET,
			 XmNbottomWidget, pwtext,
			 XmNleftAttachment, XmATTACH_WIDGET,
			 XmNleftWidget, ulabel,
			 XmNrightAttachment, XmATTACH_FORM,
			 0);
	  XtVaSetValues (pwtext,
			 XmNtopAttachment, XmATTACH_NONE,
			 XmNbottomAttachment, XmATTACH_FORM,
			 XmNleftAttachment, XmATTACH_WIDGET,
			 XmNleftWidget, plabel,
			 XmNrightAttachment, XmATTACH_FORM,
			 0);
	  XtManageChild (ulabel);
	  XtManageChild (plabel);
	}

      if (text)
	XtManageChild (text);
      if (pwtext)
	XtManageChild (pwtext);

      if (text && pwtext)
	XtManageChild (text_parent);

      XtVaSetValues (text_parent, XmNinitialFocus, (text ? text : pwtext), 0);
      XtVaSetValues (dialog, XmNinitialFocus, (text ? text : pwtext), 0);

      ac = 0;
      clear = XmCreatePushButtonGadget (dialog, "clear", av, ac);
      if (pwtext)
	XtAddCallback (clear, XmNactivateCallback, fe_clear_text_cb, pwtext);
      if (text)
	XtAddCallback (clear, XmNactivateCallback, fe_clear_text_cb, text);
      XtManageChild (clear);
    }

  if (! wait_p)
    {
      XtAddCallback (dialog, XmNokCallback, fe_destroy_cb, shell);
      XtAddCallback (dialog, XmNcancelCallback, fe_destroy_cb, shell);
      XtManageChild (dialog);
      if (text && select_p)
	fe_select_text (text);
      return False;
    }
  else
    {
      struct fe_confirm_data data;
      void *ret_val = 0;
      
      /*      data.context = context; */
      data.answer = Answer_Invalid;
      data.widget = shell;
      data.text  = (text ? text : pwtext);
      data.text2 = ((text && pwtext) ? pwtext : 0);
      data.return_value = 0;
      data.return_value_2 = 0;

      XtVaSetValues (shell, XmNdeleteResponse, XmDESTROY, 0);
      XtAddCallback (dialog, XmNokCallback, fe_destroy_ok_cb, &data);
      XtAddCallback (dialog, XmNcancelCallback, fe_destroy_cancel_cb, &data);
      XtAddCallback (dialog, XmNdestroyCallback, fe_destroy_finish_cb, &data);

      if (text)
	XtAddCallback (text, XmNdestroyCallback, fe_destroy_snarf_text_cb,
		       &data);

      if (text && pwtext)
	XtAddCallback (pwtext, XmNdestroyCallback, fe_destroy_snarf_pw2_cb,
		       &data);
      else if (pwtext)
	XtAddCallback (pwtext, XmNdestroyCallback, fe_destroy_snarf_pw_cb,
		       &data);

      if (pwtext)
	fe_SetupPasswdText (pwtext, 1024);

      fe_NukeBackingStore (dialog);
      XtManageChild (dialog);

      if (text)
	{
	  XtVaSetValues (text, XmNcursorPosition, 0, 0);
	  XtVaSetValues (text, XmNcursorPositionVisible, True, 0);
	  if (select_p)
	    fe_select_text (text);
	}

      while (data.answer == Answer_Invalid)
	fe_EventLoop ();

      if (default_text)
	{
	  if (data.answer == Answer_OK)	/* user clicked OK at the text */
	    {
	      if (text && pwtext)
		{
		  if (!data.return_value || !data.return_value_2) abort ();
		  *passwd = data.return_value_2;
		  ret_val = data.return_value;
		}
	      else if (text || pwtext)
		{
		  if (! data.return_value) abort ();
		  ret_val = data.return_value;
		}
	      else
		abort ();
	    }
	  else			/* user clicked cancel */
	    {
	      if (data.return_value)
		free (data.return_value);
	      if (data.return_value_2)
		free (data.return_value_2);
	      ret_val = 0;
	    }
	}
      else
	{
	  ret_val = (data.answer == Answer_OK
		     ? (void *) True
		     : (void *) False);
	}

      /* We need to suck the values out of the widgets before destroying them.
       */
      /* Ok, with the new way, it got destroyed by the callbacks on the
	 OK and Cancel buttons. */
      /* XtDestroyWidget (shell); */
      return ret_val;
    }
}


void
fe_Message (MWContext *context, const char *message)
{
  Widget mainw = CONTEXT_WIDGET (context);
  Widget shell, dialog;
  XmString xm_message;
  Arg av [20];
  int ac;
  Visual *v = 0;
  Colormap cmap = 0;
  Cardinal depth = 0;

  XtVaGetValues (mainw, XtNvisual, &v, XtNcolormap, &cmap,
		 XtNdepth, &depth, 0);

  ac = 0;
  XtSetArg (av[ac], XmNvisual, v); ac++;
  XtSetArg (av[ac], XmNcolormap, cmap); ac++;
  XtSetArg (av[ac], XmNdepth, depth); ac++;
  XtSetArg (av[ac], XmNallowShellResize, TRUE); ac++;
  XtSetArg (av[ac], XmNtransientFor, mainw); ac++;
  shell = XmCreateDialogShell (mainw, "message_popup", av, ac);
  ac = 0;
  xm_message = XmStringCreateLtoR ((char *) message, XmFONTLIST_DEFAULT_TAG);
  XtSetArg (av[ac], XmNdialogStyle, XmDIALOG_MODELESS); ac++;
  XtSetArg (av[ac], XmNdialogType, XmDIALOG_MESSAGE); ac++;
  XtSetArg (av[ac], XmNmessageString, xm_message); ac++;
  XtSetArg (av[ac], XmNautoUnmanage, False); ac++;
  dialog = XmCreateMessageBox (shell, "message", av, ac);
  XmStringFree (xm_message);

#ifdef NO_HELP
  fe_UnmanageChild_safe (XtNameToWidget (dialog, "*Help"));
#endif

  {
    Widget cancel = 0;
    XtVaGetValues (dialog, XmNcancelButton, &cancel, 0);
    if (! cancel) abort ();
    XtUnmanageChild (cancel);
  }
  XtAddCallback (dialog, XmNokCallback, fe_destroy_cb, shell);
  XtAddCallback (dialog, XmNcancelCallback, fe_destroy_cb, shell);
  fe_NukeBackingStore (dialog);
  XtManageChild (dialog);
}

static void
fe_destroy_ok_cb (Widget widget, XtPointer closure, XtPointer call_data)
{
  struct fe_confirm_data *data = (struct fe_confirm_data *) closure;
  data->answer = Answer_OK;
  XtDestroyWidget(data->widget);
}

static void
fe_destroy_cancel_cb (Widget widget, XtPointer closure, XtPointer call_data)
{
  struct fe_confirm_data *data = (struct fe_confirm_data *) closure;
  data->answer = Answer_Cancel;
  XtDestroyWidget(data->widget);
}

static void
fe_destroy_apply_cb (Widget widget, XtPointer closure, XtPointer call_data)
{
  struct fe_confirm_data *data = (struct fe_confirm_data *) closure;
  data->answer = Answer_Apply;
  XtDestroyWidget(data->widget);
}

static void
fe_destroy_finish_cb (Widget widget, XtPointer closure, XtPointer call_data)
{
  struct fe_confirm_data *data = (struct fe_confirm_data *) closure;
  if (data->answer == Answer_Invalid)
    data->answer = Answer_Destroy;
}

static void
fe_destroy_snarf_text_cb (Widget widget, XtPointer closure,XtPointer call_data)
{
  struct fe_confirm_data *data = (struct fe_confirm_data *) closure;
  Widget text = data->text;
  if (text)
    {
		char *plaintext = 0;
		data->return_value = 0;
		plaintext = fe_GetTextField(text);
		data->return_value = XP_STRDUP(plaintext ? plaintext : "");
    }
}

static void
fe_destroy_snarf_pw_cb (Widget widget, XtPointer closure, XtPointer call_data)
{
  struct fe_confirm_data *data = (struct fe_confirm_data *) closure;
  Widget text = data->text;
  if (text)
    data->return_value = fe_GetPasswdText (text);
}

static void
fe_destroy_snarf_pw2_cb (Widget widget, XtPointer closure, XtPointer call_data)
{
  struct fe_confirm_data *data = (struct fe_confirm_data *) closure;
  Widget text = data->text2;
  if (text)
    data->return_value_2 = fe_GetPasswdText (text);
}


/* Callback used by the clear button to nuke the contents of the text field. */
static void
fe_clear_text_cb (Widget widget, XtPointer closure, XtPointer call_data)
{
  Widget text = (Widget) closure;
  fe_SetTextField (text, "");
  /* Focus on the text widget after this, since otherwise you have to
     click again. */
  XmProcessTraversal (text, XmTRAVERSE_CURRENT);
}

/* When we are not blocking waiting for a response to a dialog, this is used
   by the buttons to make it get lost when no longer needed (the default would
   otherwise be to merely unmanage it instead of destroying it.) */
static void
fe_destroy_cb (Widget widget, XtPointer closure, XtPointer call_data)
{
  Widget shell = XtParent (widget);
  while (!XmIsDialogShell (shell))
    shell = XtParent (shell);
  XtDestroyWidget (shell);
}



/* Create a XmString that will render in the given width or less.  If
   the given string is too big, then chop out stuff by clipping out the
   center and replacing it with "...". */
XmString
fe_StringChopCreate(char* message, char* tag, XmFontList font_list,
		    int maxwidth)
{
  XmString label = XmStringCreate ((char *) message, tag);
  int string_width;
  uint16 csid;

  if (!font_list) return label;

  string_width = XmStringWidth(font_list, label);
  if (string_width >= maxwidth) {
    /* The string is bigger than the label.  Mid-truncate it. */

    XmString try;
    int length = 0;
    int maxlength = XP_STRLEN(message);
    char* text = XP_ALLOC(maxlength + 10);
    if (!text) return label;
      
    csid = INTL_CharSetNameToID(tag);
    if (csid==CS_UNKNOWN) {
      csid = INTL_CharSetNameToID(INTL_ResourceCharSet());
    }
    string_width = 0;
    while (string_width < maxwidth && length < maxlength) {
      length++;
      INTL_MidTruncateString(csid, message, text, length);
      try = XmStringCreate(text, tag);
      if (!try) break;
      string_width = XmStringWidth(font_list, try);
      if (string_width >= maxwidth) {
	XmStringFree(try);
      } else {
	XmStringFree(label);
	label = try;
      }
      try = 0;
    }

    free (text);
  }

  return label;
}

/* Dealing with field/value pairs.

   This takes a "value" widget and an arbitrary number of "label" widgets.
   It finds the widest label, and attaches the "value" widget to the left
   edge of its parent with an offset such that it falls just beyond the
   right edge of the widest label widget.

   The argument list is expected to be NULL-terminated.
 */

/* #ifdef AIXV3 */
/*    I don't understand - this seems not to be broken any more, and now
      the "hack" way doesn't compile either.  Did someone upgrade me?? */
/* # define BROKEN_VARARGS */
/* #endif */

#ifdef BROKEN_VARARGS
/* Sorry, we can't be bothered to implement varargs correctly. */
# undef va_list
# undef va_start
# undef va_arg
# undef va_end
# define va_list int
# define va_start(list,first_arg) list = 0
# define va_arg(list, type) (++list, (type) (list == 1 ? varg0 : \
				             list == 2 ? varg1 : \
				             list == 3 ? varg2 : \
				             list == 4 ? varg3 : \
				             list == 5 ? varg4 : \
				             list == 6 ? varg5 : \
				             list == 7 ? varg6 : \
				             list == 8 ? varg7 : \
				             list == 9 ? varg8 : 0))
# define va_end(list)
#endif


void
#ifdef BROKEN_VARARGS
fe_attach_field_to_labels (Widget value_field,
			   void *varg0, void *varg1, void *varg2, void *varg3,
			   void *varg4, void *varg5, void *varg6, void *varg7,
			   void *varg8)
#else
fe_attach_field_to_labels (Widget value_field, ...)
#endif
{
  va_list vargs;
  Widget current;
  Widget widest = 0;
  Position left = 0;
  long max_width = -1;

  va_start (vargs, value_field);
  while ((current = va_arg (vargs, Widget)))
    {
      Dimension width = 0;
      Position left = 0;
      Position right = 10;

#ifdef Motif_doesnt_suck
      /* Getting the leftOffset of a Gadget may randomly dump core */
      if (XmIsGadget (current))
	abort ();
      XtVaGetValues (current,
		     XmNwidth, &width,
		     XmNleftOffset, &left,
/*		     XmNrightOffset, &right,*/
		     0);

#else
      width = XfeWidth(current);
#endif

      width += (left + right);
      if (((long) width) > max_width)
	{
	  widest = current;
	  max_width = (long) width;
	}
    }
  va_end (vargs);

  if (! widest)
    abort ();
#ifdef Motif_doesnt_suck
  XtVaGetValues (value_field, XmNleftOffset, &left, 0);
#endif
  XtVaSetValues (value_field,
		 XmNleftAttachment, XmATTACH_FORM,
		 XmNleftOffset, max_width + left,
		 0);
}


/* Files
 */
static XmString fe_file_selection_directory;

/*
 * This code tried to make some minor improvements to the standard
 * Motif 1.2* File Selection Box dialog. The code here attempts
 * to use the existing Motif code for the bulk of the work, and to
 * replace some parts of Motif's behavior with something simpler.
 * I've also added some nice things conceptually lifted from the Mac
 * file picker, and at the request of users. In hindsight it might
 * have been easier to just write a new file picker widget, preferably
 * one that is semantically compatible with the Motif widget. Next time.
 *
 * Anyway, this stuff is conditionally compiled. Most of this is
 * controlled by the USE_WINNING_FILE_SELECTION define. When
 * you turn this guy on, it enables a bunch of hacks to the standard
 * Motif behavior. Best to take a look, but below is a snippet from
 * the resource file. Note that some of the hacks (the simple layout
 * hacks) require Motif source knowledge because they must attack the
 * FSB class record directly. We've moved that code out (see the
 * define FEATURE_MOTIF_FSB_CLASS_HACKING) because we can't make the
 * Motif code public.
 *
 * Related to these hacks, but somewhat orthogonal to them, we try to
 * save the last accessed directory. This makes life a little easier
 * than always going back to (say) HOME.
 *
 * From the resource file:
 *
 * This resource enables some hacks to the File Selection Box
 * which use a simpler layout algorithm than the standard Motif
 * code. The directory and file listings are always maintained
 * as equal size. We don't do this for SGI, as they have solved
 * the problem. See *useEnhancedFSB above.
 * @NOTSGI@*nsMotifFSBHacks: True
 *
 * This resource enables the CDE mode of the File Selection Box.
 * You must be running a CDE enhanced Motif library for this
 * option to work. This resource will apply pathMode=1 to the
 * File Selection Box. If you specify nsMotifFSBHacks and
 * nsMotifFSBCdeMode, some of the keyboard and filter enhancements
 * of nsMotifFSBHacks will also be applied, the layout hacks will not.
 * *nsMotifFSBCdeMode: False
 *
 * ...djw
 */
#define USE_WINNING_FILE_SELECTION win

#ifdef USE_WINNING_FILE_SELECTION

#include <Xm/XmP.h>
#include <Xm/XmosP.h>

static void
fe_force_configure(Widget widget)
{
    Dimension width = _XfeWidth(widget);
    Dimension height = _XfeHeight(widget);
    Dimension border_width = _XfeBorderWidth(widget);

    _XfeHeight(widget) += 1;
    XtResizeWidget(widget, width, height, border_width);
}

static void
fe_file_selection_get_dir_entries(String directory, String pattern,
								  unsigned type,
								  String** file_list_r, unsigned int* nfiles_r)
{
    unsigned int n;
    unsigned len = strlen(directory);

    *file_list_r = NULL;
#ifdef XM_GET_DIR_ENTRIES_OK
    _XmOSGetDirEntries(directory, pattern, type, FALSE, FALSE,
					   file_list_r, nfiles_r, &n);
#else
    _XmOSBuildFileList(directory, pattern, type,
					   file_list_r, nfiles_r, &n);

    for (n = 0; n < *nfiles_r; n++) {
        String old_s = (*file_list_r)[n];
		String new_s;

		if (strncmp(directory, old_s, len) == 0) {
			new_s = (String)XtNewString(&old_s[len]);
			XtFree(old_s);
			(*file_list_r)[n] = new_s;
		}
    }
#endif
}

static void 
fe_file_selection_file_search_proc(Widget widget, XtPointer call_data)
{
    XmFileSelectionBoxCallbackStruct* search_data
		= (XmFileSelectionBoxCallbackStruct *)call_data;
    String    directory;
    String    pattern;
    String*   file_list;
    unsigned int nfiles;
    Arg       args[10];
    Cardinal  n;
    Cardinal  nn;
    XmString* xm_string_file_list;
    XmString  xm_directory;

    if (!(directory = _XmStringGetTextConcat(search_data->dir))) {
		return;
    }

    if (!(pattern = _XmStringGetTextConcat(search_data->pattern))) {
		XtFree(directory);
		return;
    }

    /*
     *    _XmOSGetDirEntries() is being really wierd! Use this guy
     *    instead..djw
     */
    file_list = NULL;
    fe_file_selection_get_dir_entries(directory, pattern, XmFILE_REGULAR,
									  &file_list, &nfiles);

	for (n = 0, nn = 0; n < nfiles; n++) {
		if (file_list[n][0] == '.') /* no more dot files ! */
			continue;
		
		file_list[nn++] = file_list[n];
	}
	nfiles = nn;
	
    if (nfiles != 0) {
		if (nfiles > 1)
			XP_QSORT((void *)file_list, nfiles, sizeof(char *), _XmOSFileCompare);
		
		xm_string_file_list = (XmString*)XtMalloc(nfiles * sizeof(XmString));

		for (n = 0; n < nfiles; n++) {
			xm_string_file_list[n] = XmStringLtoRCreate(file_list[n],
													XmFONTLIST_DEFAULT_TAG);
		}
    } else {
		xm_string_file_list = NULL;
    }

    /* The Motif book says update the XmNdirectory */
    xm_directory = XmStringLtoRCreate(directory, XmFONTLIST_DEFAULT_TAG);

    /* Update the list widget */
    n = 0;
    XtSetArg(args[n], XmNfileListItems, xm_string_file_list); n++;
    XtSetArg(args[n], XmNfileListItemCount, nfiles); n++;
    XtSetArg(args[n], XmNlistUpdated, TRUE); n++;
    XtSetArg(args[n], XmNdirectory, xm_directory); n++;
#if 0
	XtSetArg(args[n], XmNdirectoryValid, TRUE); n++;
#endif
    XtSetValues(widget, args, n);

#if 1
    fe_force_configure(widget);
#endif

    /* And save directory default for next time */
    if (fe_file_selection_directory)
		XmStringFree(fe_file_selection_directory);
    fe_file_selection_directory = xm_directory;

    if (nfiles != 0) {
		/*
		 *    Cleanup.
		 */
		for (n = 0; n < nfiles; n++) {
			XmStringFree(xm_string_file_list[n]);
			XtFree(file_list[n]);
		}
		XtFree((char *)xm_string_file_list);
    }

    XtFree((char *)directory);
    XtFree((char *)pattern);
    
    if (file_list)
		XtFree((char *)file_list);
}

static void 
fe_file_selection_dir_search_proc(Widget widget, XtPointer call_data)
{
	XmFileSelectionBoxCallbackStruct* search_data
		= (XmFileSelectionBoxCallbackStruct *)call_data;
	String			directory;
    Arg             args[10];
    Cardinal        n;
    Cardinal        m;
    String *        dir_list;
    unsigned int    ndirs;
    unsigned int    mdirs;
    XmString *      xm_string_dir_list;
    XmString        xm_directory;
    int attempts;
    char* p;

    XtVaGetValues(widget, XmNdirectory, &xm_directory, 0);

    if (XmStringByteCompare(xm_directory, search_data->dir) == True) {
		/* Update the list widget */
		n = 0;
		XtSetArg(args[n], XmNlistUpdated, FALSE); n++;
		XtSetArg(args[n], XmNdirectoryValid, TRUE); n++;
		XtSetValues(widget, args, n);
		return;
    }

    directory = _XmStringGetTextConcat(search_data->dir);

    ndirs = 0;
    for (attempts = 0; ndirs == 0; attempts++) {
		dir_list = NULL;
		fe_file_selection_get_dir_entries(directory, "*", XmFILE_DIRECTORY,
										  &dir_list, &ndirs);
		if (ndirs) {
			if (attempts != 0) {
				XmFileSelectionBoxCallbackStruct new_data;
				XmQualifyProc q_proc;

				/*
				 *    Cleanup old stuff that won't be re-used.
				 */
				XmStringFree(search_data->dir);
				if (search_data->mask)
					XmStringFree(search_data->mask);

				memset(&new_data, 0, sizeof(XmFileSelectionBoxCallbackStruct));
				new_data.reason = search_data->reason;
				new_data.event = search_data->event;
				new_data.dir = XmStringLtoRCreate(directory,
												  XmFONTLIST_DEFAULT_TAG);
				new_data.dir_length = XmStringLength(new_data.dir);
				new_data.pattern = search_data->pattern;
				new_data.pattern_length = search_data->pattern_length;

				/*
				 *    Reset the spec, because we may get called 50M
				 *    times if this broken state.
				 */
				XtVaSetValues(widget,
							  XmNdirectory, new_data.dir,
							  XmNdirSpec, new_data.dir,
							  0);

				/*
				 *    Call qualify proc to install new directory
				 *    into widget.
				 */
				XtVaGetValues(widget, XmNqualifySearchDataProc, &q_proc, 0);
				(*q_proc)(widget, (XtPointer)&new_data,
						  (XtPointer)search_data);
			}
		} else {
			if (attempts == 0) {
#ifdef DEBUG
				char   buf[1024];
		
				sprintf(buf, "Unable to access directory:\n  %.900s\n",
						directory);
				fprintf(stderr, buf);
#endif
				XBell(XtDisplay(widget), 0); /* emulate Motif */
			}

			if (dir_list)
				XtFree((char *) dir_list);

			if (directory[0] == '\0' || strcmp(directory, "/") == 0)
				return; /* I give in! */

			while ((p = strrchr(directory, '/')) != NULL) {
				if (p[1] == '\0') { /* "/" at end */
					p[0] = '\0';
				} else {
					p[1] = '\0';
					break;
				}
			}
		}
    }

    if (ndirs > 1)
		XP_QSORT((void *)dir_list, ndirs, sizeof(char *), _XmOSFileCompare);
    
    xm_string_dir_list = (XmString *)XtMalloc(ndirs * sizeof(XmString));

    for (m = n = 0; n < ndirs; n++) {
		if (strcmp(dir_list[n], ".") == 0) /* don't want that */
			continue;
		xm_string_dir_list[m++] = XmStringLtoRCreate(dir_list[n],
													 XmFONTLIST_DEFAULT_TAG);
    } 
    mdirs = m;

    /* Update the list widget */
    n = 0;
    XtSetArg(args[n], XmNdirListItems, xm_string_dir_list); n++;
    XtSetArg(args[n], XmNdirListItemCount, mdirs); n++;
    XtSetArg(args[n], XmNlistUpdated, TRUE); n++;
    XtSetArg(args[n], XmNdirectoryValid, TRUE); n++;
    XtSetValues(widget, args, n);

    /* And save directory default for next time */
    xm_directory = XmStringLtoRCreate(directory, XmFONTLIST_DEFAULT_TAG);
    if (fe_file_selection_directory)
		XmStringFree(fe_file_selection_directory);
    fe_file_selection_directory = xm_directory;

    /*
     *    Cleanup.
     */
    for (n = 0; n < mdirs; n++) {
		XmStringFree(xm_string_dir_list[n]);
    }    
    for (n = 0; n < ndirs; n++) {
		XtFree(dir_list[n]);
    } 

    XtFree((char *)xm_string_dir_list);
    XtFree((char *)directory);
}

static void
fe_file_selection_qualify_search_data_proc(Widget widget,
					   XtPointer sd, XtPointer qsd)
{
    XmFileSelectionBoxCallbackStruct* s_data
		= (XmFileSelectionBoxCallbackStruct *)sd;
    XmFileSelectionBoxCallbackStruct* qs_data
		= (XmFileSelectionBoxCallbackStruct *)qsd;
    String mask_string = _XmStringGetTextConcat(s_data->mask);
    String dir_string = _XmStringGetTextConcat(s_data->dir);
    String pattern_string = _XmStringGetTextConcat(s_data->pattern);
    String dir_part = NULL;
    String pattern_part = NULL;
    String q_dir_string = NULL;
    String q_mask_string = NULL;
    String q_pattern_string = NULL;
    String dir_free_string = NULL;
    String pattern_free_string = NULL;
    String p;
    XmString xm_directory;
    XmString xm_pattern;
    XmString xm_dir_spec;

    if (dir_string != NULL) {
		if (dir_string[0] == '/') {
			dir_part = dir_string;
		} else {
			XtVaGetValues(widget, XmNdirectory, &xm_directory, 0);
			if (xm_directory != NULL) {
				dir_part = _XmStringGetTextConcat(xm_directory);
				p = (String)XtMalloc(strlen(dir_part) +
									 strlen(dir_string) + 2);
				strcpy(p, dir_part);
				strcat(p, dir_string);
				XtFree(dir_string);
				XtFree(dir_part);
				dir_part = dir_string = p;
			}
		}
    } else {
		if (mask_string != NULL) { /* use filter value */
			pattern_part = _XmOSFindPatternPart(mask_string);

			if (pattern_part != mask_string) {
				pattern_part[-1] =  '\0'; /* zap dir trailing '/' */
				/* "" or "/" */
				if (*mask_string == '\0') {
					dir_part = "/";
				} else if (*mask_string == '/' && mask_string[1] == '\0') {
					dir_part = "//";
				} else {
					dir_part = mask_string;
				}
			}
		} else { /* use XmNdirectory */
			XtVaGetValues(widget, XmNdirectory, &xm_directory, 0);
			if (xm_directory != NULL) {
				dir_part = _XmStringGetTextConcat(xm_directory);
				dir_free_string = dir_part; /* to force free */
			}
		}
    }

    if (pattern_string != NULL) {
		pattern_part = pattern_string;
    } else {
		if (mask_string != NULL) {
			if (!pattern_part)
				pattern_part = _XmOSFindPatternPart(mask_string);
		} else {
			XtVaGetValues(widget, XmNpattern, &xm_pattern, 0);
			if (xm_pattern != NULL) {
				pattern_part = _XmStringGetTextConcat(xm_pattern);
				pattern_free_string = pattern_part; /* so it gets freed */
			}
		}
    }

    /*
     *    It's ok for dir_part to be NULL, as _XmOSQualifyFileSpec
     *    will just use the cwd.
     */
    _XmOSQualifyFileSpec(dir_part, pattern_part,
						 &q_dir_string, &q_pattern_string);

    qs_data->reason = s_data->reason ;
    qs_data->event = s_data->event ;
    if (s_data->value) {
		qs_data->value = XmStringCopy(s_data->value);
    } else {
		XtVaGetValues(widget, XmNdirSpec, &xm_dir_spec, 0);
		qs_data->value = XmStringCopy(xm_dir_spec);
    }
    qs_data->length = XmStringLength(qs_data->value) ;
    q_mask_string = (String)XtMalloc(strlen(q_dir_string) +
									 strlen(q_pattern_string) + 1);
    strcpy(q_mask_string, q_dir_string);
    strcat(q_mask_string, q_pattern_string);
    qs_data->mask = XmStringLtoRCreate(q_mask_string, XmFONTLIST_DEFAULT_TAG);
    qs_data->mask_length = XmStringLength(qs_data->mask);
    qs_data->dir = XmStringLtoRCreate(q_dir_string, XmFONTLIST_DEFAULT_TAG);
    qs_data->dir_length = XmStringLength(qs_data->dir) ;
    qs_data->pattern = XmStringLtoRCreate(q_pattern_string,
										  XmFONTLIST_DEFAULT_TAG);
    qs_data->pattern_length = XmStringLength(qs_data->pattern);

    if (dir_free_string)
		XtFree(dir_free_string);
    if (pattern_free_string)
		XtFree(pattern_free_string);
    if (dir_string)
		XtFree(dir_string);
    if (pattern_string)
		XtFree(pattern_string);
    if (mask_string)
		XtFree(mask_string);
    if (q_dir_string)
		XtFree(q_dir_string);
    if (q_pattern_string)
		XtFree(q_pattern_string);
    if (q_mask_string)
		XtFree(q_mask_string);
}

static char* fe_file_selection_home = 0;

static char*
fe_file_selection_gethome()
{
	if (!fe_file_selection_home) {
		char* foo = getenv("HOME");
		char* p;

		if (!foo)
			foo = "/"; /* hah */

		fe_file_selection_home = XP_STRDUP(foo);
		
		p = strrchr(fe_file_selection_home, '/');
		if (p != NULL && p != fe_file_selection_home && p[1] == '\0')
			*p = '\0';
	}

	return fe_file_selection_home;
}

static void
fe_file_selection_dirspec_cb(Widget widget, XtPointer closure, XtPointer cb)
{
    XmTextVerifyCallbackStruct* vd = (XmTextVerifyCallbackStruct*)cb;
    Widget fsb = (Widget)closure;
    XmString xm_directory;
    String directory;
    String ptr;

    if (vd->startPos == 0 && vd->text->ptr != NULL) {
		if (vd->text->ptr[0] == '~') { /* expand $HOME */

			char* home = fe_file_selection_gethome();

			ptr = (String)XtMalloc(strlen(home) + strlen(vd->text->ptr) + 2);

			strcpy(ptr, home);
			strcat(ptr, "/");
			if (vd->text->length > 1)
				strcat(ptr, &vd->text->ptr[1]);

			XtFree(vd->text->ptr);
			vd->text->ptr = ptr;
			vd->text->length = strlen(ptr);
			
		} else if (vd->text->ptr[0] != '/') {
			XtVaGetValues(fsb, XmNdirectory, &xm_directory, 0);
			if (xm_directory != NULL) {
				directory = _XmStringGetTextConcat(xm_directory);
				
				ptr = (String)XtMalloc(strlen(directory) +
									   strlen(vd->text->ptr) + 2);
				strcpy(ptr, directory);
				strcat(ptr, vd->text->ptr);
				
				XtFree(vd->text->ptr);
				vd->text->ptr = ptr;
				vd->text->length = strlen(ptr);
			}
		}
    }
}

static void
fe_file_selection_filter_cb(Widget apply, XtPointer closure, XtPointer cbd)
{
	Widget fsb = (Widget)closure;
	Widget dir = XmFileSelectionBoxGetChild(fsb, XmDIALOG_DIR_LIST);
	int*   position_list;
	int    position_count;

	if (XmListGetSelectedPos(dir, &position_list, &position_count)) {

		if (position_count == 1 && position_list[0] == 1)
			XmListSelectPos(dir, 1, True);
			
		if (position_count > 0 && position_list != NULL)
			XtFree((char*)position_list);
	}
}

static void
fe_file_selection_directory_up_action(Widget widget,
									  XEvent* event,
									  String* av, Cardinal* ac)
{
    XmString xm_directory;
    String   directory;
    String   p;

	while (!XtIsSubclass(widget, xmFileSelectionBoxWidgetClass)) {
		widget = XtParent(widget);
		if (!widget)
			return;
	}

	XtVaGetValues(widget, XmNdirectory, &xm_directory, 0);

	if (xm_directory != NULL) {
		int len;

		directory = _XmStringGetTextConcat(xm_directory);

		len = XP_STRLEN(directory);

		if (len > 0 && directory[len-1] == '/')
			directory[len-1] = '\0';

		if ((p = XP_STRRCHR(directory, '/')) != NULL) {
			p[1] = '\0';

			xm_directory = XmStringCreateSimple(directory);

			XtVaSetValues(widget, XmNdirectory, xm_directory, 0);

			XmStringFree(xm_directory);
		}
		XtFree(directory);
	}
}

static XtActionsRec fe_file_selection_actions[] =
{
	{ "FileSelectionBoxCdDotDot", fe_file_selection_directory_up_action },
};

static _XmConst char fe_file_selection_accelerators[] =
"~Alt Meta<Key>osfUp:   FileSelectionBoxCdDotDot()\n"
"Alt ~Meta<Key>osfUp:   FileSelectionBoxCdDotDot()";

static unsigned fe_file_selection_add_actions_done;

static XtPointer
fe_file_selection_register_actions_mappee(Widget widget, XtPointer data)
{
	XtOverrideTranslations(widget, (XtTranslations)data);
	return 0;
}

static void
fe_file_selection_register_actions(Widget widget)
{
	if (!fe_file_selection_add_actions_done) {
		XtAppAddActions(fe_XtAppContext,
						fe_file_selection_actions,
						countof(fe_file_selection_actions));
		fe_file_selection_add_actions_done++;
	}

	fe_WidgetTreeWalk(widget,
					  fe_file_selection_register_actions_mappee,
					  (XtPointer) XtParseTranslationTable(fe_file_selection_accelerators));
}

typedef struct {
	Boolean hack;
	Boolean cde;
} fe_fsb_res_st;

static XtResource fe_fsb_resources[] =
{
	{
		"nsMotifFSBHacks", XtCBoolean, XtRBoolean, sizeof(Boolean),
		XtOffset(fe_fsb_res_st *, hack), XtRImmediate,
#ifdef IRIX	
		/* Irix has the nice enhanced FSB, so they don't need it */
		(XtPointer)False
#else
		(XtPointer)True
#endif
	},
	{
		"nsMotifFSBCdeMode", XtCBoolean, XtRBoolean, sizeof(Boolean),
		XtOffset(fe_fsb_res_st *, cde), XtRImmediate, (XtPointer)False
	}
};

typedef enum {
	FSB_LOSING,
	FSB_HACKS,
	FSB_CDE,
	FSB_CDE_PLUS
} fe_fsb_mode;

static fe_fsb_mode
fe_file_selection_box_get_resources(Widget parent)
{
	static Boolean done;
	static fe_fsb_res_st result;

	if (!done) {
		
		while (!XtIsTopLevelShell(parent))
			parent = XtParent(parent);

		XtGetApplicationResources(parent,
								  (XtPointer)&result,
								  fe_fsb_resources, XtNumber(fe_fsb_resources),
								  0, 0);
		done++;
	}

	if (result.hack && result.cde)
		return FSB_CDE_PLUS;
	else if (result.hack && !result.cde)
		return FSB_HACKS;
	else if (!result.hack && result.cde)
		return FSB_CDE;
	else
		return FSB_LOSING;
}
#endif /*USE_WINNING_FILE_SELECTION*/

static void
fe_file_selection_save_directory_cb(Widget widget, XtPointer a, XtPointer b)
{
    XmString  xm_directory;
	XmString  xm_dirspec;
    /* And save directory default for next time */
    XtVaGetValues(widget,
				  XmNdirectory, &xm_directory,
				  XmNdirSpec, &xm_dirspec,
				  0);

	if (xm_dirspec != 0) {
		char* directory = _XmStringGetTextConcat(xm_dirspec);

		if (directory != NULL) {
			if (directory[0] == '/') {
				
				char* end = strrchr(directory, '/');
				
				if (end != NULL) {
					if (end != directory)
						*end = '\0';
					
					if (xm_directory != NULL)
						XmStringFree(xm_directory);
					
					xm_directory = XmStringCreateLocalized(directory);
				}
			}
			XtFree(directory);
		}
	}

    if (xm_directory != 0) {
		if (fe_file_selection_directory)
			XmStringFree(fe_file_selection_directory);
		fe_file_selection_directory = xm_directory;
    }
}

Widget
fe_CreateFileSelectionBox(Widget parent, char* name,
						  Arg* p_args, Cardinal p_n)
{
    Arg      args[32];
    Cardinal n;
    Cardinal i;
    Widget   fsb;
    Boolean  directory_set = FALSE;
#ifdef USE_WINNING_FILE_SELECTION
    Boolean  dir_search_set = FALSE;
    Boolean  file_search_set = FALSE;
    Boolean  qualify_set = FALSE;
    Widget   dir_list;
    Widget   file_list;
    Widget   dirspec;
    Widget   filter;
	Widget   filter_button;
	fe_fsb_mode  mode = fe_file_selection_box_get_resources(parent);
#endif /*USE_WINNING_FILE_SELECTION*/

	for (n = 0, i = 0; i < p_n && i < 30; i++) {	
		if (p_args[i].name == XmNdirectory)
			directory_set = TRUE;

#ifdef USE_WINNING_FILE_SELECTION
		if (mode == FSB_HACKS) {
			if (p_args[i].name == XmNdirSearchProc)
				dir_search_set = TRUE;
			else if (p_args[i].name == XmNfileSearchProc)
				file_search_set = TRUE;
			else if (p_args[i].name == XmNqualifySearchDataProc)
				qualify_set = TRUE;
			else
				args[n++] = p_args[i];
		}
		else
#endif /*USE_WINNING_FILE_SELECTION*/
		{
			args[n++] = p_args[i];
		}
	}

	/*
	 *    Add last visited directory
	 */
    if (!directory_set) {
		XtSetArg(args[n], XmNdirectory, fe_file_selection_directory); n++;
    }

#ifdef USE_WINNING_FILE_SELECTION
    if (mode == FSB_CDE || mode == FSB_CDE_PLUS) {
		XtSetArg(args[n], "pathMode", 1); n++;
    }

	if (mode == FSB_HACKS) {

		if (!dir_search_set) {
			XtSetArg(args[n], XmNdirSearchProc,
					 fe_file_selection_dir_search_proc); n++;
		}
		
		if (!file_search_set) {
			XtSetArg(args[n], XmNfileSearchProc,
					 fe_file_selection_file_search_proc); n++;
		}
		
		if (!qualify_set) {
			XtSetArg(args[n], XmNqualifySearchDataProc,
					 fe_file_selection_qualify_search_data_proc); n++;
		}
    
#ifdef FEATURE_MOTIF_FSB_CLASS_HACKING
		/*
		 *    This routine is now in the Xm patches tree, as it requires
		 *    access to the Motif source...djw
		 */
		fe_FileSelectionBoxHackClassRecord(parent);
#endif /*FEATURE_MOTIF_FSB_CLASS_HACKING*/

		fsb = XmCreateFileSelectionBox(parent, name, args, n);
    
		/*
		 *    XmCreateFileSelectionBox() sets directory list
		 *    XmNscrollBarDisplayPolicy to STATIC which always shows
		 *    the horizontal scrollbar. Bye...
		 */
		dir_list = XmFileSelectionBoxGetChild(fsb, XmDIALOG_DIR_LIST);
		XtVaSetValues(dir_list, XmNscrollBarDisplayPolicy, XmAS_NEEDED, 0);
		file_list = XmFileSelectionBoxGetChild(fsb, XmDIALOG_LIST);
		XtVaSetValues(file_list, XmNscrollBarDisplayPolicy, XmAS_NEEDED, 0);
		dirspec = XmFileSelectionBoxGetChild(fsb, XmDIALOG_TEXT);
		XtAddCallback(dirspec, XmNmodifyVerifyCallback,
					  fe_file_selection_dirspec_cb, (XtPointer)fsb);
		filter = XmFileSelectionBoxGetChild(fsb, XmDIALOG_FILTER_TEXT);
		XtAddCallback(filter, XmNmodifyVerifyCallback,
					  fe_file_selection_dirspec_cb, (XtPointer)fsb);

	}
	else
#endif /*USE_WINNING_FILE_SELECTION*/
	{
		fsb = XmCreateFileSelectionBox(parent, name, args, n);
	}

#ifdef IRIX
#ifndef IRIX5_3
	/* FIX 98019
	 *
	 * For IRIX 6.2 and later:
	 *      Reset filetype mask to suit custom Sgm file dialog.
	 *      (otherwise directories don't show up in the list.)
	 */
	XtVaSetValues(fsb,XmNfileTypeMask,XmFILE_ANY_TYPE,NULL);
#endif
#endif


#ifdef USE_WINNING_FILE_SELECTION
	if (mode == FSB_HACKS || mode == FSB_CDE_PLUS) {
		filter_button = XmFileSelectionBoxGetChild(fsb, XmDIALOG_APPLY_BUTTON);
		XtAddCallback(filter_button, XmNactivateCallback,
					  fe_file_selection_filter_cb, fsb);
		
		fe_file_selection_register_actions(fsb);
	}
#endif /*USE_WINNING_FILE_SELECTION*/

    XtAddCallback(fsb, XmNokCallback,
				  fe_file_selection_save_directory_cb, NULL);

    return fsb;
}

Widget 
fe_CreateFileSelectionDialog(Widget   parent,
							 String   name,
							 Arg*     fsb_args,
							 Cardinal fsb_n)
{   
	Widget  fsb;       /*  new fsb widget      */
	Widget  ds;        /*  DialogShell         */
	ArgList ds_args;   /*  arglist for shell  */
	char    ds_name[256];

    /*
	 *    Create DialogShell parent.
	 */
    XP_STRCPY(ds_name, name);
    XP_STRCAT(ds_name, "_popup"); /* motif compatible */

    ds_args = (ArgList)XtMalloc(sizeof(Arg) * (fsb_n + 1));
    XP_MEMCPY(ds_args, fsb_args, (sizeof(Arg) * fsb_n));
    XtSetArg(ds_args[fsb_n], XmNallowShellResize, True); 
    ds = XmCreateDialogShell(parent, ds_name, ds_args, fsb_n + 1);

    XtFree((char*)ds_args);

    /*
	 *    Create FileSelectionBox.
	 */
    fsb = fe_CreateFileSelectionBox(ds, name, fsb_args, fsb_n);
    XtAddCallback(fsb, XmNdestroyCallback, _XmDestroyParentCallback, NULL);

    return(fsb) ;
}

static void fe_file_cb (Widget, XtPointer, XtPointer);
static void fe_file_destroy_cb (Widget, XtPointer, XtPointer);
static void fe_file_type_cb (Widget, XtPointer, XtPointer);

#ifdef NEW_DECODERS
static void fe_file_type_hack_extension (struct fe_file_type_data *ftd);
#endif /* NEW_DECODERS */

/*
 * fe_ReadFilename_2
 * context:
 *	If context is non-null, it uses context's data to display the fsb.
 * iparent, filebp, ftdp:
 *	If context is NULL, then this uses these to create the file
 *	selection box.
 *	'iparent' is the parent shell under which the FSB will be created.
 *	'filebp' is a pointer to the file selection box widget. [CANT BE NULL]
 *	'ftdp' is a pointer to file type data used by FSB code. [CANT BE NULL]
 *		We allocate one and return it back to the caller.
 *		Everytime we are called with the same filebp, we need to
 * 		be called with the same ftdp.
 *
 * WARNING:	1. Allocates *ftdp
 *		2. filebp and ftdp go together. For every filebp we need to
 *		   use the ftdp that was created with it.
 *
 * Just another form of fe_ReadFileName() so that other parts of fe
 * eg. bookmarks can use this as they dont have a context and I
 * think making a dummy context is bad.
 * - dp
 *
 * Akkana note: bug 82924 was fixed in FE_PromptForFileName()
 * (in src/context_funcs.cpp), but has not yet been fixed here.
 * The three filename prompting routines need to be integrated into one;
 * see comment under FE_PromptForFileName().
 */
char *
fe_ReadFileName_2 (	MWContext* context,
			Widget iparent, Widget* filebp,
			struct fe_file_type_data **ftdp,
			const char *title, const char *default_url,
			Boolean must_match, int *save_as_type)
{
  Widget fileb;
  struct fe_file_type_data *ftd;
  struct fe_confirm_data data;
  Widget sabox, samenu, sabutton;

  if (context) {
    fileb = CONTEXT_DATA (context)->file_dialog;
    ftd = CONTEXT_DATA (context)->ftd;
  }
  else {
    fileb = *filebp;
    ftd = *ftdp;
  }

  if (! title) title = XP_GetString(XFE_OPEN_FILE);

  if (! fileb)
    {
      Arg av [20];
      int ac, i;
      Widget shell;
      Widget parent;
      Visual *v = 0;
      Colormap cmap = 0;
      Cardinal depth = 0;

      if (context) parent = CONTEXT_WIDGET (context);
      else parent = iparent;

      XtVaGetValues (parent, XtNvisual, &v, XtNcolormap, &cmap,
		     XtNdepth, &depth, 0);

      ac = 0;
      XtSetArg (av[ac], XmNvisual, v); ac++;
      XtSetArg (av[ac], XmNdepth, depth); ac++;
      XtSetArg (av[ac], XmNcolormap, cmap); ac++;
      XtSetArg (av[ac], XmNdeleteResponse, XmDESTROY); ac++;
/*      XtSetArg (av[ac], XmNallowShellResize, True); ac++;*/
      shell = XmCreateDialogShell (parent, "fileSelector_popup", av, ac);
      ac = 0;
      XtSetArg (av[ac], XmNdialogStyle, XmDIALOG_PRIMARY_APPLICATION_MODAL);
      ac++;
      ftd = (struct fe_file_type_data *)
	calloc (sizeof (struct fe_file_type_data), 1);
      if (context) CONTEXT_DATA (context)->ftd = ftd;
      else *ftdp = ftd;
      XtSetArg (av[ac], XmNuserData, ftd); ac++;
      fileb = fe_CreateFileSelectionBox (shell, "fileSelector", av, ac);

#ifdef NO_HELP
      fe_UnmanageChild_safe (XtNameToWidget (fileb, "*Help"));
#endif

      ac = 0;
      XtSetArg (av[ac], XmNvisual, v); ac++;
      XtSetArg (av[ac], XmNdepth, depth); ac++;
      XtSetArg (av[ac], XmNcolormap, cmap); ac++;
      sabox = XmCreateFrame (fileb, "frame", av, ac);
      samenu = XmCreatePulldownMenu (sabox, "formatType", av, ac);
      ac = 0;

#ifdef NEW_DECODERS
      ftd->fileb = fileb;
#endif /* NEW_DECODERS */
      ftd->options [fe_FILE_TYPE_TEXT] =
	XmCreatePushButtonGadget (samenu, "text", av, ac);
      ftd->options [fe_FILE_TYPE_HTML] =
	XmCreatePushButtonGadget (samenu, "html", av, ac);
#ifdef SAVE_ALL
      ftd->options [fe_FILE_TYPE_HTML_AND_IMAGES] =
	XmCreatePushButtonGadget (samenu, "tree", av, ac);
#endif /* SAVE_ALL */
      ftd->options [fe_FILE_TYPE_PS] =
	XmCreatePushButtonGadget (samenu, "ps", av, ac);
/*       ftd->selected_option = fe_FILE_TYPE_TEXT; */
	  /* Make the default "save as" type html (ie, source) */
      ftd->selected_option = fe_FILE_TYPE_HTML;
      XtVaSetValues (samenu, XmNmenuHistory,
		     ftd->options [ftd->selected_option], 0);
      ac = 0;
      XtSetArg (av [ac], XmNsubMenuId, samenu); ac++;
      sabutton = fe_CreateOptionMenu (sabox, "formatType", av, ac);
      for (i = 0; i < countof (ftd->options); i++)
	if (ftd->options[i])
	  {
	    XtAddCallback (ftd->options[i], XmNactivateCallback,
			   fe_file_type_cb, ftd);
	    XtManageChild (ftd->options[i]);
	  }
      XtManageChild (sabutton);

      if (context) CONTEXT_DATA (context)->file_dialog = fileb;
      else *filebp = fileb;

      fe_HackDialogTranslations (fileb);
    }
  else
    {
      /* Without this the "*fileSelector.width:" resource doesn't work
	 on subsequent invocations of this box.  Don't ask me why.  I
	 tried permutations of allowShellResize as well with no effect.
	 (It's better to keep the widgets themselves around, if not the
	 windows, so that the default text is persistent.)
       */
#if 0
      XtUnrealizeWidget (fileb);
#endif
      sabox = XtNameToWidget (fileb, "*frame");

	  /*
	   *    Restore the values saved in fe_file_selection_save_directory_cb()
	   */
	  if (fe_file_selection_directory != 0) {
		  XtVaSetValues(fileb, XmNdirectory, fe_file_selection_directory, 0);
	  }
    }

#ifdef NEW_DECODERS
  ftd->orig_url = default_url;
  ftd->conversion_allowed_p = (save_as_type != 0);
#endif /* NEW_DECODERS */

#ifdef NO_HELP
  fe_UnmanageChild_safe (XmSelectionBoxGetChild (fileb, XmDIALOG_HELP_BUTTON));
#endif

  XtRemoveAllCallbacks (fileb, XmNnoMatchCallback);
  XtRemoveAllCallbacks (fileb, XmNokCallback);
  XtRemoveAllCallbacks (fileb, XmNapplyCallback);
  XtRemoveAllCallbacks (fileb, XmNcancelCallback);
  XtRemoveAllCallbacks (fileb, XmNdestroyCallback);

  XtAddCallback (fileb, XmNnoMatchCallback, fe_file_cb, &data);
  XtAddCallback (fileb, XmNokCallback,      fe_file_cb, &data);
  XtAddCallback (fileb, XmNcancelCallback,  fe_file_cb, &data);
  XtAddCallback (fileb, XmNdestroyCallback, fe_file_destroy_cb, &data);

  XtVaSetValues (XtParent (fileb), XmNtitle, title, 0);
#if 0 /* mustMatch doesn't work! */
  XtVaSetValues (fileb, XmNmustMatch, must_match, 0);
#else
  XtVaSetValues (fileb, XmNmustMatch, False, 0);
  data.must_match = must_match;
#endif

  {
    String dirmask = 0;
    XmString xms;
    char *s, *tail;
    char buf [2048];
    const char *def;

    if (! default_url)
      def = 0;
    else if ((def = strrchr (default_url, '/')))
      def++;
    else
      def = default_url;

    XtVaGetValues (fileb, XmNdirSpec, &xms, 0);
    XmStringGetLtoR (xms, XmFONTLIST_DEFAULT_TAG, &s); /* s - is XtMalloc  */
    XmStringFree (xms);

    /* Take the file name part off of `s', leaving only the dir. */
    tail = strrchr (s, '/');
    if (tail) *tail = 0;

    PR_snprintf (buf, sizeof (buf), "%.900s/%.900s", s, (def ? def : ""));
    if (def)
      {
	/* Grab the file name part (it's sitting right after the end of `s')
	   and map out the characters which ought not go into file names.
	   Also, terminate the file name at ? or #, since those aren't
	   really a part of the URL's file, but are modifiers to it.
	 */
	for (tail = buf+strlen(s)+1; *tail; tail++)
	  if (*tail == '?' || *tail == '#')
	    *tail = 0;
	  else if (*tail < '+' || *tail > 'z' ||
		   *tail == ':' || *tail == ';' ||
		   *tail == '<' || *tail == '>' ||
		   *tail == '\\' || *tail == '^' ||
		   *tail == '\'' || *tail == '`' ||
		   *tail == ',')
	    *tail = '_';
      }
    XtFree (s);

    /* If the dialog already existed, its data is out of date.  Resync. */
    XtVaGetValues (fileb, XmNdirMask, &dirmask, 0);
    XmFileSelectionDoSearch (fileb, dirmask);

    /* Change the default file name. */
    xms = XmStringCreate (buf, XmFONTLIST_DEFAULT_TAG);
    XtVaSetValues (fileb, XmNdirSpec, xms, 0);
    XmStringFree (xms);

#ifdef NEW_DECODERS
    fe_file_type_hack_extension (ftd);
#endif /* NEW_DECODERS */
  }

  data.context = context;
  data.answer = Answer_Invalid;
  data.return_value = 0;

  if (save_as_type)
    XtManageChild (sabox);
  else
    XtUnmanageChild (sabox);

  fe_NukeBackingStore (fileb);
  XtManageChild (fileb);
  /* #### check for unmanagement here */
  while (data.answer == Answer_Invalid)
    fe_EventLoop ();

  if (data.answer != Answer_Destroy) {
    /* Call the ok -> save directory callback because it's been removed */
    fe_file_selection_save_directory_cb(fileb, NULL, NULL);
    XtUnmanageChild(fileb);
    XtRemoveCallback(fileb, XmNdestroyCallback, fe_file_destroy_cb, &data);
  }


  if (save_as_type)
    *save_as_type = ftd->selected_option;

  if (data.answer == Answer_Destroy) {
    if (!data.context) {
      XP_ASSERT(filebp && ftdp);
      *filebp = 0;
      *ftdp = 0;
    }
    if (ftd) {
      /*
       * If the context got destroyed, this could have been freed by the
       * context. Our destroy routine checks for the parent context being
       * freed and sets the return_value to -1 if the context was destroyed.
       */
      if (data.return_value != (void *)-1)
	XP_FREE(ftd);
    }
    data.answer = Answer_Cancel; /* Simulate CANCEL */
    data.return_value = 0;
  }

  return (char *) data.return_value;
}

char *
fe_ReadFileName (MWContext *context, const char *title,
		 const char *default_url,
		 Boolean must_match,
		 int *save_as_type)
{
  char *ret = NULL;

  fe_ProtectContext(context);
  ret = fe_ReadFileName_2( context, NULL, NULL, NULL,
			   title, default_url, must_match, save_as_type);
  fe_UnProtectContext(context);
  if (fe_IsContextDestroyed(context)) {
    free(CONTEXT_DATA(context));
    free(context);
  }
  return ret;
}


static void 
fe_file_destroy_cb (Widget widget, XtPointer closure, XtPointer call_data)
{
  struct fe_confirm_data *data = (struct fe_confirm_data *) closure;
  if (data->answer == Answer_Invalid) {
    data->answer = Answer_Destroy;
    /* Reset the context file dialog storage. Be sure that this destroy isnt
     * a result of the context itself being destroyed.
     */
    if (data->context)
	if (!XfeIsAlive(CONTEXT_WIDGET(data->context))) {
	/* Indicates the context was destroyed too */
	data->return_value = (void *)-1;
      }
      else {
	CONTEXT_DATA(data->context)->file_dialog = 0;
	CONTEXT_DATA(data->context)->ftd = 0;
      }
  }
}

static void 
fe_file_cb (Widget widget, XtPointer closure, XtPointer call_data)
{
  struct fe_confirm_data *data = (struct fe_confirm_data *) closure;
  XmFileSelectionBoxCallbackStruct *sbc =
    (XmFileSelectionBoxCallbackStruct *) call_data;

  switch (sbc->reason)
    {
    case XmCR_NO_MATCH:
      {
    NOMATCH:
	XBell (XtDisplay (widget), 0);
	break;
      }
    case XmCR_OK:
      {
	XmStringGetLtoR (sbc->value, XmFONTLIST_DEFAULT_TAG,
			 (char **) &data->return_value);
#if 1
	/* mustMatch doesn't work!! */
	{
	  struct stat st;
	  if (data->must_match &&
	      data->return_value &&
	      stat (data->return_value, &st))
	    {
	      free (data->return_value);
	      data->return_value = 0;
	      goto NOMATCH;
	    }
	}
#endif
	data->answer = Answer_OK;
	break;
      }
    case XmCR_CANCEL:
      {
	data->answer = Answer_Cancel;
	data->return_value = 0;
	break;
      }
    default:
      abort ();
    }
}

static void
fe_file_type_cb (Widget widget, XtPointer closure, XtPointer call_data)
{
  struct fe_file_type_data *ftd = (struct fe_file_type_data *) closure;
  int i;
  ftd->selected_option = -1;
  for (i = 0; i < countof (ftd->options); i++)
    if (ftd->options [i] == widget)
      {
	ftd->selected_option = i;
	break;
      }
#ifdef NEW_DECODERS
  fe_file_type_hack_extension (ftd);
#endif /* NEW_DECODERS */
}

#ifdef NEW_DECODERS
static void
fe_file_type_hack_extension_1 (struct fe_file_type_data *ftd,
			       Boolean dirspec_p)
{
  int desired_type;
  const char *default_file_name = "index.html";
  XmString xm_file = 0;
  char *file = 0;
  char *name_part = 0;
  char *ext = 0;
  char *orig_ext = 0;
  const char *new_ext = 0;
  char *orig_url_copy = 0;

  if (!ftd || !ftd->fileb)
    return;

  if (ftd->conversion_allowed_p)
    desired_type = ftd->selected_option;
  else
    desired_type = fe_FILE_TYPE_HTML;

  /* Find a default file name.
     If this URL ends in a file part, use that, otherwise, assume "index.html".
     Then, once we've got a default file name, set the default extension to
     use for "Save As Source" from that.
   */
  if (ftd->orig_url)
    {
      const char *orig_name_part;
      char *s;

      orig_url_copy = strdup (ftd->orig_url);

      /* take off form and anchor data. */
      if ((s = strchr (orig_url_copy, '?')))
	*s = 0;
      if ((s = strchr (orig_url_copy, '#')))
	*s = 0;

      orig_name_part = strrchr (orig_url_copy, '/');
      if (! orig_name_part)
	orig_name_part = strrchr (orig_url_copy, ':');
      if (orig_name_part)
	orig_name_part++;
      else if (orig_url_copy && *orig_url_copy)
	orig_name_part = orig_url_copy;

      if (orig_name_part && *orig_name_part)
	default_file_name = orig_name_part;

      orig_ext = strrchr (default_file_name, '.');

      if (!orig_ext && !dirspec_p)
	{
	  /* If we're up in the filter area, and there is as yet no
	     orig_ext, then that means that there was file name without
	     an extension (as opposed to no file name, when we would
	     have defaulted to "index.html".)  So, for the purposes
	     of the filter, set the extension to ".*".
	   */
	  orig_ext = ".*";
	}
    }

  /* Get `file' from what's currently typed into the text field.
   */
  XtVaGetValues (ftd->fileb,
		 (dirspec_p ? XmNdirSpec : XmNdirMask), &xm_file, 0);
  if (! xm_file) return;
  XmStringGetLtoR (xm_file, XmFONTLIST_DEFAULT_TAG, &file); /* file- XtMalloc */
  XmStringFree (xm_file);

  /* If the file ends in "/" then stick the default file name on the end. */
  if (dirspec_p && file && *file && file [strlen (file) - 1] == '/')
    {
      char *file2 = (char *) malloc (strlen (file) +
				     strlen (default_file_name) + 1);
      strcpy (file2, file);
      strcat (file2, default_file_name);
      XtFree (file);
      file = file2;
    }

  name_part = strrchr (file, '/');
  if (! name_part)
    name_part = strrchr (file, ':');
  if (name_part)
    name_part++;

  if (!name_part || !*name_part)
    name_part = 0;

  if (name_part)
    ext = strrchr (name_part, '.');

  switch (desired_type)
    {
    case fe_FILE_TYPE_HTML:
    case fe_FILE_TYPE_HTML_AND_IMAGES:
      new_ext = orig_ext;
      break;
    case fe_FILE_TYPE_TEXT:
    case fe_FILE_TYPE_FORMATTED_TEXT:
      /* The user has selected "text".  Change the extension to "txt"
	 only if the original URL had extension "html" or "htm". */
      if (orig_ext &&
	  (!strcasecomp (orig_ext, ".html") ||
	   !strcasecomp (orig_ext, ".htm")))
	new_ext = ".txt";
      else
	new_ext = orig_ext;
      break;
    case fe_FILE_TYPE_PS:
      new_ext = ".ps";
      break;
    default:
      break;
    }

  if (ext && new_ext /* && !!strcasecomp (ext, new_ext) */ )
    {
      char *file2;
      *ext = 0;
      file2 = (char *) malloc (strlen (file) + strlen (new_ext) + 1);
      strcpy (file2, file);
      strcat (file2, new_ext);
      xm_file = XmStringCreateLtoR (file2, XmFONTLIST_DEFAULT_TAG);

      if (dirspec_p)
	{
	  XtVaSetValues (ftd->fileb, XmNdirSpec, xm_file, 0);
	}
      else
	{
	  XmString saved_value = 0;

	  /* Don't let what's currently typed into the `Selection' field
	     to change as a result of running this search again -- that's
	     totally bogus. */
	  XtVaGetValues (ftd->fileb, XmNdirSpec, &saved_value, 0);

	  XtVaSetValues (ftd->fileb, XmNdirMask, xm_file, 0);
	  XmFileSelectionDoSearch (ftd->fileb, xm_file);
	  if (saved_value)
	    {
	      XtVaSetValues (ftd->fileb, XmNdirSpec, saved_value, 0);
	      XmStringFree (saved_value);
	    }
	}

      XmStringFree (xm_file);
      free (file2);
    }

  if (orig_url_copy)
    free (orig_url_copy);

  XtFree (file);
}


static void
fe_file_type_hack_extension (struct fe_file_type_data *ftd)
{
  fe_file_type_hack_extension_1 (ftd, False);
  fe_file_type_hack_extension_1 (ftd, True);
}

#endif /* NEW_DECODERS */



/* Open URL - prompts asynchronously
 */

static char *last_open_url_text = 0;

struct fe_open_url_data {
  MWContext *context;
  Widget widget;
  Widget text;
#ifdef EDITOR
  Widget in_editor;
#endif
  Widget in_browser;
};

static char*
cleanup_selection(char* target, char* source, unsigned max_size)
{
	char* p;
	char* q;
	char* end;

	for (p = source; isspace(*p); p++) /* skip beginning whitespace */
		;

	end = &p[max_size-1];
	q = target;

	while (p < end) {
		/*
		 *    Stop if we detect an unprintable, or newline.
		 */
		if (!isprint(*p) || *p == '\n' || *p == '\r')
			break;

		if (isspace(*p))
			*q++ = ' ', p++;
		else
			*q++ = *p++;
	}
	/* strip trailing whitespace */
	while (q > target && isspace(q[-1]))
		q--;
	 
	*q = '\0';

	return target;
}

static void
file_dialog_get_url(MWContext* context, char* address, MWContextType ge_type)
{
	if (address != NULL) {
		if (ge_type == MWContextBrowser) {
			fe_BrowserGetURL(context, address);
		}
#ifdef EDITOR
		else if (ge_type == MWContextEditor) {
			fe_EditorEdit(context, NULL, NULL, address); /* try to re-use */
		}
#endif
	}
}

static void
fe_open_url_cb (Widget widget, XtPointer closure, XtPointer call_data)
{
	struct fe_open_url_data *data = (struct fe_open_url_data *) closure;
	char*  text = NULL;
	MWContext* context;
	MWContextType ge_type = MWContextAny;

	if (call_data == NULL ||    /* when it's a destroy callback */
		!XfeIsAlive(widget) || /* on it's way */
		data == NULL)                         /* data from hell */
		return;
	
	context = data->context;

	if (widget == data->in_browser
#ifdef EDITOR
        || widget == data->in_editor
#endif
        ) {

		text = fe_GetTextField(data->text);

		if (! text)
			return;
		
		cleanup_selection(text, text, strlen(text)+1);

		if (*text != '\0') {

			if (last_open_url_text)
				XtFree(last_open_url_text);

			last_open_url_text = text;
            
            if (widget == data->in_browser) 
              ge_type = MWContextBrowser;
#ifdef EDITOR
            else if (widget == data->in_editor)
              ge_type = MWContextEditor;
#endif
		}
    }

	XtUnmanageChild(data->widget);
	XtDestroyWidget(data->widget);
	free(data);

	/*
	 *    Call common routine to divert the get_url()
	 */
	file_dialog_get_url(context, text, ge_type);
}

static void
fe_open_url_browse_cb(Widget widget, XtPointer closure, XtPointer call_data)
{
	struct fe_open_url_data* data = (struct fe_open_url_data *)closure;

	fe_browse_file_of_text(data->context, data->text, False);
}

void
fe_OpenURLDialog(MWContext* context)
{
	struct fe_open_url_data *data;
	Widget dialog;
	Widget label;
	Widget text;
	Widget in_browser;
#ifdef EDITOR
	Widget in_editor;
#endif
	Widget form;
	Widget browse;
	Arg    args[20];
	int    n;
	char*  string;

	dialog = fe_CreatePromptDialog(context, "openURLDialog",
								   FALSE, TRUE, TRUE, TRUE, TRUE);

	n = 0;
	form = XmCreateForm(dialog, "form", args, n);
	XtManageChild(form);

	n = 0;
	XtSetArg(args [n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg(args [n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg(args [n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	label = XmCreateLabelGadget(form, "label", args, n);
	XtManageChild(label);

	n = 0;
	XtSetArg(args [n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args [n], XmNtopWidget, label); n++;
	XtSetArg(args [n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg(args [n], XmNbottomAttachment, XmATTACH_FORM); n++;
	browse = XmCreatePushButtonGadget(form, "choose", args, n);
	XtManageChild(browse);

	string = last_open_url_text? last_open_url_text : "";

	n = 0;
	XtSetArg(args [n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg(args [n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args [n], XmNtopWidget, label); n++;
	XtSetArg(args [n], XmNrightAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args [n], XmNrightWidget, browse); n++;
	XtSetArg(args [n], XmNbottomAttachment, XmATTACH_FORM); n++;
    XtSetArg(args [n], XmNeditable, True); n++;
	XtSetArg(args [n], XmNcursorPositionVisible, True); n++;
	text = fe_CreateTextField(form, "openURLText", args, n);
	fe_SetTextField(text, string);
	XtManageChild(text);

	n = 0;
#ifdef MOZ_MAIL_NEWS
	in_browser = XmCreatePushButtonGadget(dialog, "openInBrowser", args, n);
#else
	in_browser = XmCreatePushButtonGadget(dialog, "OK", args, n);
#endif
	XtManageChild(in_browser);

#ifdef EDITOR
	n = 0;
	XtSetArg(args[n], XmNsensitive, !fe_IsEditorDisabled()); n++;
	in_editor = XmCreatePushButtonGadget(dialog, "openInEditor", args, n);
	XtManageChild(in_editor);
#endif

	data = (struct fe_open_url_data *)calloc(sizeof(struct fe_open_url_data), 1);
	data->context = context;
	data->widget = dialog;
	data->text = text;
	data->in_browser = in_browser;
#ifdef EDITOR
	data->in_editor = in_editor;

	if (context->type == MWContextEditor)
		XtVaSetValues(dialog, XmNdefaultButton, in_editor, 0);
	else
#endif
		XtVaSetValues(dialog, XmNdefaultButton, in_browser, 0);

	XtAddCallback(browse, XmNactivateCallback,fe_open_url_browse_cb, data);
#ifdef EDITOR
	XtAddCallback(in_editor, XmNactivateCallback, fe_open_url_cb, data);
#endif
	XtAddCallback(in_browser, XmNactivateCallback, fe_open_url_cb, data);
	XtAddCallback(dialog, XmNcancelCallback, fe_open_url_cb, data);
	XtAddCallback(dialog, XmNdestroyCallback, fe_open_url_cb, data);
	XtAddCallback(dialog, XmNapplyCallback, fe_clear_text_cb, text);
	
	fe_NukeBackingStore (dialog);
	XtManageChild (dialog);
}

void
fe_OpenURLChooseFileDialog (MWContext *context)
{
	char *text;

	if (!context) return;

	text = fe_ReadFileName (context, 		
							XP_GetString(XFE_OPEN_FILE),
							last_open_url_text,
							FALSE, /* must match */
							0); /* save_as_type */

	file_dialog_get_url(context, text, context->type);
}




/* Prompt the user for their password.  The message string is displayed
 * to the user so they know what they are giving their password for.
 * The characters of the password are not echoed.
 */
char *
XFE_PromptPassword (MWContext *context, const char *message)
{
  return (char *) fe_dialog (CONTEXT_WIDGET (context),
			     "password", message, TRUE, "", TRUE, FALSE,
			     ((char **) 1));
}

/* Prompt for a  username and password
 *
 * message is a prompt message.
 *
 * if username and password are not NULL they should be used
 * as default values and NOT MODIFIED.  New values should be malloc'd
 * and put in their place.
 *
 * If the User hit's cancel FALSE should be returned, otherwise
 * TRUE should be returned.
 */
XP_Bool
XFE_PromptUsernameAndPassword (MWContext *context, 
			      const char *message,
			      char **username,
			      char **password)
{ 
  char *pw = "";
  char *un = (char *) fe_dialog (CONTEXT_WIDGET (context),
				 "password", message, TRUE,
				 (*username ? *username : ""), TRUE, FALSE,
				 &pw);
  if (pw)
    {
      *username = un;
      *password = XP_STRDUP(pw);
      return(TRUE);
    }
  else
    {
      *username = 0;
      *password = 0;
      return(FALSE);
    }
}



/* Prompting for visuals
 */

static void fe_visual_cb (Widget, XtPointer, XtPointer);

Visual *
fe_ReadVisual (MWContext *context)
{
  Widget mainw = CONTEXT_WIDGET (context);
  Display *dpy = XtDisplay (mainw);
  Screen *screen = XtScreen (mainw);
  Arg av [20];
  int ac;
  int i;
  Widget shell;
  XmString *items;
  int item_count;
  int default_item = 0;
  XVisualInfo vi_in, *vi_out;
  struct fe_confirm_data data;
  Visual *v = 0;
  Colormap cmap = 0;
  Cardinal depth = 0;

  XtVaGetValues (mainw, XtNvisual, &v, XtNcolormap, &cmap,
		 XtNdepth, &depth, 0);

  vi_in.screen = fe_ScreenNumber (screen);
  vi_out = XGetVisualInfo (dpy, VisualScreenMask, &vi_in, &item_count);
  if (! vi_out) item_count = 0;
  items = (XmString *) calloc (sizeof (XmString), item_count + 1);
  for (i = 0; i < item_count; i++)
    {
      char *vdesc = fe_VisualDescription (screen, vi_out [i].visual);
      items[i] = XmStringCreate (vdesc, XmFONTLIST_DEFAULT_TAG);
      free (vdesc);
      if (vi_out [i].visual == v)
	default_item = i;
    }

  ac = 0;
  XtSetArg (av[ac], XmNvisual, v); ac++;
  XtSetArg (av[ac], XmNdepth, depth); ac++;
  XtSetArg (av[ac], XmNcolormap, cmap); ac++;
  XtSetArg (av[ac], XmNallowShellResize, TRUE); ac++;
  XtSetArg (av[ac], XmNtransientFor, mainw); ac++;
  XtSetArg (av[ac], XmNdialogStyle, XmDIALOG_PRIMARY_APPLICATION_MODAL); ac++;
  XtSetArg (av[ac], XmNdialogType, XmDIALOG_QUESTION); ac++;
/*  XtSetArg (av[ac], XmNautoUnmanage, False); ac++;*/
  XtSetArg (av[ac], XmNlistItems, items); ac++;
  XtSetArg (av[ac], XmNlistItemCount, item_count); ac++;
  shell = XmCreateSelectionDialog (mainw, "visual", av, ac);

  XtAddCallback (shell, XmNokCallback, fe_visual_cb, &data);
  XtAddCallback (shell, XmNcancelCallback, fe_visual_cb, &data);
  XtAddCallback (shell, XmNdestroyCallback, fe_destroy_finish_cb, &data);

  fe_UnmanageChild_safe (XmSelectionBoxGetChild (shell, XmDIALOG_APPLY_BUTTON));
  fe_UnmanageChild_safe (XmSelectionBoxGetChild (shell, XmDIALOG_SEPARATOR));
  fe_UnmanageChild_safe (XmSelectionBoxGetChild (shell, XmDIALOG_TEXT));
  fe_UnmanageChild_safe (XmSelectionBoxGetChild (shell,
						XmDIALOG_SELECTION_LABEL));

  {
    Widget list = XmSelectionBoxGetChild (shell, XmDIALOG_LIST);
    XtVaSetValues (list,
		   XmNselectedItems, (items + default_item),
		   XmNselectedItemCount, 1,
		   0);
  }

  data.context = context;
  data.widget = shell;
  data.answer = Answer_Invalid;
  data.return_value = 0;

  fe_NukeBackingStore (shell);
  XtManageChild (shell);

  /* #### check for destruction here */
  while (data.answer == Answer_Invalid)
    fe_EventLoop ();

  for (i = 0; i < item_count; i++)
    XmStringFree (items [i]);
  free (items);

  if (vi_out)
    {
      int index = (int) data.return_value;
      Visual *v = (data.answer == 1 ? vi_out [index].visual : 0);
      XFree ((char *) vi_out);
      return v;
    }
  else
    {
      return 0;
    }
}

static void fe_visual_cb (Widget widget, XtPointer closure,
			  XtPointer call_data)
{
  struct fe_confirm_data *data = (struct fe_confirm_data *) closure;
  XmSelectionBoxCallbackStruct *cb =
    (XmSelectionBoxCallbackStruct *) call_data;
  XmString *items;
  int count;
  int i;
  XtVaGetValues (data->widget,
		 XmNlistItems, &items,
		 XmNlistItemCount, &count,
		 0);
  switch (cb->reason)
    {
    case XmCR_OK:
    case XmCR_APPLY:
      {
	data->answer = 1;
	data->return_value = (void *) -1;
	for (i = 0; i < count; i++)
	  if (XmStringCompare (items[i], cb->value))
	    data->return_value = (void *) i;
	if (data->return_value == (void *) -1)
	  abort ();
	break;
      }
    case XmCR_CANCEL:
      data->answer = 0;
      break;
    default:
      abort ();
      break;
    }
}


/* Displaying source
 */

struct fe_source_data
{
  Widget dialog;
  Widget name, url, text;
};

#if 0
static void
fe_close_source_cb (Widget widget, XtPointer closure, XtPointer call_data)
{
  MWContext *context = (MWContext *) closure;
  if (! CONTEXT_DATA (context)->sd) return;
  XtUnmanageChild (CONTEXT_DATA (context)->sd->dialog);
}

static void
fe_source_save_as_cb (Widget widget, XtPointer closure, XtPointer call_data)
{
  MWContext *context = (MWContext *) closure;
  char *url = 0;
  URL_Struct *url_struct;
  if (! CONTEXT_DATA (context)->sd) return;
  url = fe_GetTextField (CONTEXT_DATA (context)->sd->url);
  if (! url) return;
  url_struct = NET_CreateURLStruct (url, FALSE);
  fe_SaveURL (context, url_struct);
}
#endif

#if 0	/* This is history. Should remove this code out later. */
Widget
fe_ViewSourceDialog (MWContext *context, const char *title, const char *url)
{
  Widget mainw = CONTEXT_WIDGET (context);
  struct fe_source_data *sd = CONTEXT_DATA (context)->sd;

  if (! sd)
    {
      Widget shell, form, text;
      Widget url_label, url_text, title_label, title_text;
      Widget ok_button, save_button;
      Arg av [20];
      int ac;
      Visual *v = 0;
      Colormap cmap = 0;
      Cardinal depth = 0;
      sd = (struct fe_source_data *) calloc(sizeof (struct fe_source_data), 1);

      XtVaGetValues (mainw, XtNvisual, &v, XtNcolormap, &cmap,
		     XtNdepth, &depth, 0);
      ac = 0;
      XtSetArg (av[ac], XmNvisual, v); ac++;
      XtSetArg (av[ac], XmNdepth, depth); ac++;
      XtSetArg (av[ac], XmNcolormap, cmap); ac++;
      XtSetArg (av[ac], XmNallowShellResize, TRUE); ac++;
      XtSetArg (av[ac], XmNtransientFor, mainw); ac++;

      XtSetArg (av[ac], XmNdialogStyle, XmDIALOG_MODELESS); ac++;

      XtSetArg (av[ac], XmNdialogType, XmDIALOG_INFORMATION); ac++;
      XtSetArg (av[ac], XmNautoUnmanage, False); ac++;
      shell = XmCreateTemplateDialog (mainw, "source", av, ac);

      fe_UnmanageChild_safe (XmMessageBoxGetChild (shell, XmDIALOG_SEPARATOR));
      fe_UnmanageChild_safe (XmMessageBoxGetChild (shell, XmDIALOG_OK_BUTTON));
      fe_UnmanageChild_safe (XmMessageBoxGetChild (shell,
						  XmDIALOG_CANCEL_BUTTON));
/*      fe_UnmanageChild_safe (XmMessageBoxGetChild (shell,
						  XmDIALOG_APPLY_BUTTON));*/
      fe_UnmanageChild_safe (XmMessageBoxGetChild (shell,
						  XmDIALOG_DEFAULT_BUTTON));
      fe_UnmanageChild_safe (XmMessageBoxGetChild (shell,
						  XmDIALOG_HELP_BUTTON));

      ac = 0;
      save_button = XmCreatePushButtonGadget (shell, "save", av, ac);
      ok_button = XmCreatePushButtonGadget (shell, "OK", av, ac);

      ac = 0;
      form = XmCreateForm (shell, "form", av, ac);

      ac = 0;
      title_label = XmCreateLabelGadget (form, "titleLabel", av, ac);
      url_label = XmCreateLabelGadget (form, "urlLabel", av, ac);
      ac = 0;
      XtSetArg (av [ac], XmNeditable, False); ac++;
      XtSetArg (av [ac], XmNcursorPositionVisible, False); ac++;
      title_text = fe_CreateTextField (form, "titleText", av, ac);
      url_text   = fe_CreateTextField (form, "urlText", av, ac);

      ac = 0;
      XtSetArg (av [ac], XmNeditable, False); ac++;
      XtSetArg (av [ac], XmNcursorPositionVisible, False); ac++;
      XtSetArg (av[ac], XmNeditMode, XmMULTI_LINE_EDIT); ac++;
      text = XmCreateScrolledText (form, "text", av, ac);
      fe_HackDialogTranslations (text);

      XtVaSetValues (title_label,
		     XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
		     XmNtopWidget, title_text,
		     XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
		     XmNbottomWidget, title_text,
		     XmNleftAttachment, XmATTACH_FORM,
		     XmNrightAttachment, XmATTACH_WIDGET,
		     XmNrightWidget, title_text,
		     0);
      XtVaSetValues (url_label,
		     XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
		     XmNtopWidget, url_text,
		     XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
		     XmNbottomWidget, url_text,
		     XmNleftAttachment, XmATTACH_FORM,
		     XmNrightAttachment, XmATTACH_WIDGET,
		     XmNrightWidget, url_text,
		     0);

      XtVaSetValues (title_text,
		     XmNtopAttachment, XmATTACH_FORM,
		     XmNbottomAttachment, XmATTACH_NONE,
		     XmNrightAttachment, XmATTACH_FORM,
		     0);
      XtVaSetValues (url_text,
		     XmNtopAttachment, XmATTACH_WIDGET,
		     XmNtopWidget, title_text,
		     XmNbottomAttachment, XmATTACH_NONE,
		     XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
		     XmNleftWidget, title_text,
		     XmNrightAttachment, XmATTACH_OPPOSITE_WIDGET,
		     XmNrightWidget, title_text,
		     0);
      XtVaSetValues (XtParent (text),
		     XmNtopAttachment, XmATTACH_WIDGET,
		     XmNtopWidget, url_text,
		     XmNbottomAttachment, XmATTACH_FORM,
		     XmNleftAttachment, XmATTACH_FORM,
		     XmNrightAttachment, XmATTACH_FORM,
		     0);

      fe_attach_field_to_labels (title_text, title_label, url_label, 0);

      XtAddCallback (save_button, XmNactivateCallback,
		     fe_source_save_as_cb, context);
      XtAddCallback (ok_button, XmNactivateCallback,
		     fe_close_source_cb, context);

      XtVaSetValues (shell, XmNdefaultButton, save_button, 0);

      XtManageChild (title_label);
      XtManageChild (title_text);
      XtManageChild (url_label);
      XtManageChild (url_text);
      XtManageChild (text);
      XtManageChild (form);
      XtManageChild (save_button);
      XtManageChild (ok_button);

      sd->dialog = shell;
      sd->text = text;
      sd->url = url_text;
      sd->name = title_text;
      CONTEXT_DATA (context)->sd = sd;
    }

  XtVaSetValues (sd->text, XmNcursorPosition, 0);
  fe_SetTextField (sd->text, "");
  XtVaSetValues (sd->name, XmNcursorPosition, 0, 0);
  fe_SetTextField (sd->name, (title ? title : ""));
  XtVaSetValues (sd->url, XmNcursorPosition, 0, 0);
  fe_SetTextField (sd->url, (url ? url : ""));

  fe_NukeBackingStore (sd->dialog);
  XtManageChild (sd->dialog);
  return sd->text;
}
#endif /* 0 */


/* User information
 */

#include <pwd.h>
#include <netdb.h>

void
fe_DefaultUserInfo (char **uid, char **name, Boolean really_default_p)
{
  struct passwd *pw = getpwuid (geteuid ());
  char *user_name, *tmp;
  char *real_uid = (pw ? pw->pw_name : "");
  char *user_uid = 0;

  if (really_default_p)
    {
      user_uid = real_uid;
    }
  else
    {
      user_uid = getenv ("LOGNAME");
      if (! user_uid) user_uid = getenv ("USER");
      if (! user_uid) user_uid = real_uid;

      /* If the env vars claim a different user, get the real name of
	 that user instead of the actual user. */
      if (strcmp (user_uid, real_uid))
	{
	  struct passwd *pw2 = getpwnam (user_uid);
	  if (pw2) pw = pw2;
	}
    }

  user_uid = strdup (user_uid);
  user_name = strdup ((pw && pw->pw_gecos ? pw->pw_gecos : "&"));

  /* Terminate the string at the first comma, to lose phone numbers and crap
     like that.  This may lose for "Jr."s, but who cares: Unix sucks. */
  if ((tmp = strchr (user_name, ',')))
    *tmp = 0;

  if ((tmp = strchr (user_name, '&')))
    {
      int i, j;
      char *new = (char *) malloc (strlen (user_name) + strlen (user_uid) + 1);
      for (i = 0, j = 0; user_name[i]; i++)
	if (tmp == user_name + i)
	  {
	    strcpy (new + j, user_uid);
	    new[j] = toupper (new[j]);
	    j += strlen (user_uid);
	    tmp = 0;
	  }
      else
	{
	  new[j++] = user_name[i];
	}
      free (user_name);
      user_name = new;
    }

  *uid = user_uid;
  *name = user_name;
}


/* Synchronous loading of a URL
   Internally, we use the URL mechanism to do some things during which time
   we want the interface to be blocked - for example, formatting a document
   for inclusion in a mail window, and printing, and so on.  This code lets
   us put up a dialog box with a status message and a cancel button, and
   bring it down when the event is over.
 */

/* #define SYNCHRONOUS_URL_DIALOG_WORKS */


#ifdef SYNCHRONOUS_URL_DIALOG_WORKS
static void
fe_synchronous_url_cancel_cb (Widget widget, XtPointer closure,
			      XtPointer call_data)
{
  MWContext *context = (MWContext *) closure;
  XP_InterruptContext (context);
}

/*
 * Make sure the the popped up modal dialog receives at least
 * on FocusIn event, because Motif seems to expect/require it.
 */
static void
fe_dialog_expose_eh (Widget widget, XtPointer closure, XEvent *event)
{
  MWContext *context = (MWContext *) closure;
  Widget dialog = CONTEXT_DATA (context)->synchronous_url_dialog;

  XtRemoveEventHandler(dialog, ExposureMask, FALSE,
        (XtEventHandler)fe_dialog_expose_eh, context);
  XSetInputFocus(XtDisplay (dialog), XtWindow(dialog),
        RevertToParent, CurrentTime);
  XSync (XtDisplay (dialog), False);
}
#endif /* SYNCHRONOUS_URL_DIALOG_WORKS */

void
fe_LowerSynchronousURLDialog (MWContext *context)
{
  if (CONTEXT_DATA (context)->synchronous_url_dialog)
    {
#ifdef SYNCHRONOUS_URL_DIALOG_WORKS
      Widget shell = XtParent (CONTEXT_DATA (context)->synchronous_url_dialog);

      /* Don't call XP_InterruptContext a (possibly) second time. */
      XtRemoveCallback (CONTEXT_DATA (context)->synchronous_url_dialog,
			XtNdestroyCallback,
			fe_synchronous_url_cancel_cb, context);
      XtUnmanageChild (CONTEXT_DATA (context)->synchronous_url_dialog);
      XSync (XtDisplay (shell), False);
      XtDestroyWidget (shell);
#endif /* SYNCHRONOUS_URL_DIALOG_WORKS */

      CONTEXT_DATA (context)->synchronous_url_dialog = 0;

      /* If this context was destroyed, do not proceed furthur */
      if (fe_IsContextDestroyed(context))
	return;

      assert (CONTEXT_DATA (context)->active_url_count > 0);
      if (CONTEXT_DATA (context)->active_url_count > 0)
	CONTEXT_DATA (context)->active_url_count--;
      if (CONTEXT_DATA (context)->active_url_count <= 0)
	XFE_AllConnectionsComplete (context);
      fe_SetCursor (context, False);
    }
}

static void
fe_synchronous_url_exit (URL_Struct *url, int status, MWContext *context)
{
  if (status == MK_CHANGING_CONTEXT)
    return;
  fe_LowerSynchronousURLDialog (context);
  if (status < 0 && url->error_msg)
    {
      FE_Alert (context, url->error_msg);
    }
  NET_FreeURLStruct (url);
  CONTEXT_DATA (context)->synchronous_url_exit_status = status;
}

void
fe_RaiseSynchronousURLDialog (MWContext *context, Widget parent,
			      const char *title)
{
#ifdef SYNCHRONOUS_URL_DIALOG_WORKS
  Widget shell, dialog;
  Arg av [20];
  int ac;
  Visual *v = 0;
  Colormap cmap = 0;
  Cardinal depth = 0;
  char title2 [255];
  Boolean popped_up;
#endif /* SYNCHRONOUS_URL_DIALOG_WORKS */

  CONTEXT_DATA (context)->active_url_count++;
  if (CONTEXT_DATA (context)->active_url_count == 1)
    {
      CONTEXT_DATA (context)->clicking_blocked = True;
      fe_StartProgressGraph (context);
      fe_SetCursor (context, False);
    }

#ifndef SYNCHRONOUS_URL_DIALOG_WORKS

  CONTEXT_DATA (context)->synchronous_url_dialog = (Widget) ~0;
  CONTEXT_DATA (context)->synchronous_url_exit_status = 0;

#else /* SYNCHRONOUS_URL_DIALOG_WORKS */

  XtVaGetValues (parent, XtNvisual, &v, XtNcolormap, &cmap,
		 XtNdepth, &depth, 0);

  strcpy (title2, title);
  strcat (title2, "_popup");

  ac = 0;
  XtSetArg (av[ac], XmNvisual, v); ac++;
  XtSetArg (av[ac], XmNcolormap, cmap); ac++;
  XtSetArg (av[ac], XmNdepth, depth); ac++;
  XtSetArg (av[ac], XmNallowShellResize, TRUE); ac++;
  XtSetArg (av[ac], XmNtransientFor, parent); ac++;
  XtSetArg (av[ac], XmNdeleteResponse, XmDESTROY); ac++;
  shell = XmCreateDialogShell (parent, title2, av, ac);
  ac = 0;
  XtSetArg (av[ac], XmNdialogStyle, XmDIALOG_PRIMARY_APPLICATION_MODAL); ac++;
  XtSetArg (av[ac], XmNdialogType, XmDIALOG_WORKING); ac++;
  XtSetArg (av[ac], XmNautoUnmanage, False); ac++;
  XtSetArg (av[ac], XmNdeleteResponse, XmDESTROY); ac++;
  dialog = XmCreateMessageBox (shell, (char *) title, av, ac);

  fe_UnmanageChild_safe (XmMessageBoxGetChild (dialog, XmDIALOG_OK_BUTTON));
#ifdef NO_HELP
  fe_UnmanageChild_safe (XmMessageBoxGetChild (dialog, XmDIALOG_HELP_BUTTON));
#endif

  XtAddCallback (dialog, XmNokCallback, fe_destroy_cb, shell);
  XtAddCallback (dialog, XmNcancelCallback, fe_destroy_cb, shell);
  XtAddCallback (dialog, XmNdestroyCallback, fe_synchronous_url_cancel_cb,
		 context);
  XtAddEventHandler(dialog, ExposureMask, FALSE,
		    (XtEventHandler)fe_dialog_expose_eh, context);

  CONTEXT_DATA (context)->synchronous_url_dialog = dialog;
  CONTEXT_DATA (context)->synchronous_url_exit_status = 0;

  fe_NukeBackingStore (dialog);
  XtManageChild (dialog);
  XSync (XtDisplay (dialog), False);

  /*
   * We wait here until we KNOW the dialog is popped up,
   * otherwise Motif will misbehave.
   */
  popped_up = FALSE;
  while (!popped_up)
    {
      XEvent event;

      XtAppNextEvent(XtWidgetToApplicationContext(dialog), &event);
      if ((event.type == Expose)&&(event.xexpose.window == XtWindow(dialog)))
	{
	  popped_up = TRUE;
	}
      XtDispatchEvent(&event);
    }
#endif /* SYNCHRONOUS_URL_DIALOG_WORKS */

}


int
fe_await_synchronous_url (MWContext *context)
{
  /* Loop dispatching X events until the dialog box goes down as a result
     of the exit routine being called (which may be a result of the Cancel
     button being hit. */
  int status;
  
  fe_ProtectContext(context);
  while (CONTEXT_DATA (context)->synchronous_url_dialog)
    fe_EventLoop ();
  fe_UnProtectContext(context);
  status = CONTEXT_DATA (context)->synchronous_url_exit_status;
  if (fe_IsContextDestroyed(context)) {
    free(CONTEXT_DATA(context));
    free(context);
  }
  return (status);
}


int
fe_GetSynchronousURL (MWContext *context,
		      Widget widget_to_grab,
		      const char *title,
		      URL_Struct *url,
		      int output_format,
		      void *call_data)
{
  int status;
  url->fe_data = call_data;
  fe_RaiseSynchronousURLDialog (context, widget_to_grab, title);
  status = NET_GetURL (url, output_format, context, fe_synchronous_url_exit);
  if (status < 0)
    return status;
  else
    return fe_await_synchronous_url (context);
}


#ifdef MOZ_MAIL_NEWS

/* Sending mail
 */

void fe_mail_text_modify_cb (Widget, XtPointer, XtPointer);

Boolean
fe_CheckDeferredMail (void)
{
  struct fe_MWContext_cons* rest;
  Boolean haveQueuedMail = False;
  int numMsgs = 0;
  MSG_ViewIndex row = 0;

  for (rest = fe_all_MWContexts; rest; rest = rest->next) {
    MWContext* context = rest->context;
    fe_MailNewsContextData* d = MAILNEWS_CONTEXT_DATA(context);
    if (context->type == MWContextMail) {
      MSG_CommandStatus (d->folderpane, MSG_DeliverQueuedMessages, NULL, 0,
			 &haveQueuedMail, NULL, NULL, NULL);
      if (haveQueuedMail) {
	MSG_FolderLine line;
	/* ###tw  This is just too vile for words.  */
	while (MSG_GetFolderLineByIndex(d->folderpane, row, 1, &line)) {
	  if (line.flags & MSG_FOLDER_FLAG_QUEUE) {
	    numMsgs = line.total;
	    break;
	  }
	  row++;
	}
	if (numMsgs) {
	  char buf [256];
	  void * sendNow = 0;
	  if (numMsgs == 1)
	    sendNow = fe_dialog (CONTEXT_WIDGET (fe_all_MWContexts->context),
				 "sendNow",
				 XP_GetString (XFE_OUTBOX_CONTAINS_MSG),
				 TRUE, 0, TRUE, FALSE, 0);
	  else {
	    PR_snprintf (buf, 256, XP_GetString (XFE_OUTBOX_CONTAINS_MSGS), numMsgs);
	    sendNow = fe_dialog (CONTEXT_WIDGET (fe_all_MWContexts->context),
				 "sendNow", buf, TRUE, 0, TRUE, FALSE, 0);
	  }
	  if (sendNow) {
	    MSG_Command (d->folderpane, MSG_DeliverQueuedMessages, NULL, 0);
	    return False;
	  }
	}
      }
    }
  }
  /* return True means no mail - weird, but consistent
   * with fe_CheckUnsentMail below. We can change them
   * later.
   */
  return True;
}

Boolean
fe_CheckUnsentMail (void)
{
  struct fe_MWContext_cons* rest;
  for (rest = fe_all_MWContexts; rest; rest = rest->next) {
    MWContext* context = rest->context;
    if (context->type == MWContextMessageComposition) {
      return XFE_Confirm(fe_all_MWContexts->context,
			 fe_globalData.unsent_mail_message);
    }
  }
  return TRUE;
}

#if 0
static char* fe_last_attach_type = NULL;

static void
fe_del_attachment(struct fe_mail_attach_data *mad, int pos)
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

  xmstr = XmStringCreate(name, XmFONTLIST_DEFAULT_TAG);
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

  xmstr = XmStringCreate(name, XmFONTLIST_DEFAULT_TAG);
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
  ok_button = XmCreatePushButtonGadget (shell, "OK", av, ac);
  clear_button = XmCreatePushButtonGadget (shell, "clear", av, ac);
  cancel_button = XmCreatePushButtonGadget (shell, "cancel", av, ac);

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

  {
    const char *str = MSG_GetAssociatedURL (mad->comppane);
    fe_SetTextFieldAndCallBack (location_text, (char *) (str ? str : ""));
  }

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
	XmStringGetLtoR (sbc->value, XmFONTLIST_DEFAULT_TAG, &file);
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
  fileb = fe_CreateFileSelectionBox (shell, "fileBrowser", av, ac);
#ifdef NO_HELP
  fe_UnmanageChild_safe (XmSelectionBoxGetChild (fileb, XmDIALOG_HELP_BUTTON));
#endif

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
   * my how intuitive, if the file is attach as source, desired type = NULL.
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
  CONTEXT_DATA(mad->context) ->mad = NULL;
  free (mad);
}

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

static void
fe_attachOk_cb (Widget widget, XtPointer closure, XtPointer call_data)
{
  struct fe_mail_attach_data *mad = (struct fe_mail_attach_data  *) closure;
  const char* ptr;

  mad->attachments[mad->nattachments].url = NULL;
  MSG_SetAttachmentList(mad->comppane, mad->attachments);
  ptr = MSG_GetCompHeader(mad->comppane, MSG_ATTACHMENTS_HEADER_MASK);
  fe_SetTextFieldAndCallBack(CONTEXT_DATA(mad->context)->mcAttachments,
		       ptr ? (char*) ptr : "");

  /* We dont need to delete all the attachments that we have as they will
     be deleted either the next time we show the attach window (or) when
     the message composition context gets destroyed */

  XtUnmanageChild(mad->shell);
  if (mad->location_shell)
    XtUnmanageChild(mad->location_shell);
  if (mad->file_shell)
    XtUnmanageChild(mad->file_shell);
}

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
    XtVaSetValues(mad->delete, XmNsensitive, False, 0);
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
  fe_attach_doc_type_cb(which_w, mad, NULL);

  XtVaSetValues(mad->delete, XmNsensitive, True, 0);
}

static void
fe_make_attach_dialog(MWContext* context)
{
  fe_ContextData* data = CONTEXT_DATA(context);
  Widget shell, form;
  Widget list;
  Widget messb;
  Widget   attach_location, attach_file, delete;
  Widget label;
  Widget   text_p, source_p, postscript_p;
  Widget ok_button, cancel_button;
  Widget kids [50];
  Arg av [20];
  int ac;
  Visual *v = 0;
  Colormap cmap = 0;
  Cardinal depth = 0;
  struct fe_mail_attach_data *mad = data->mad;

  XP_ASSERT(context->type == MWContextMessageComposition);

  if (mad && mad->shell)
    return;

  XtVaGetValues (CONTEXT_WIDGET(context), XtNvisual, &v, XtNcolormap, &cmap,
		 XtNdepth, &depth, 0);
  ac = 0;
  XtSetArg (av[ac], XmNvisual, v); ac++;
  XtSetArg (av[ac], XmNdepth, depth); ac++;
  XtSetArg (av[ac], XmNcolormap, cmap); ac++;
  XtSetArg (av[ac], XmNallowShellResize, TRUE); ac++;
  XtSetArg (av[ac], XmNtransientFor, CONTEXT_WIDGET(context)); ac++;
  XtSetArg (av[ac], XmNdialogStyle, XmDIALOG_MODELESS); ac++;
  XtSetArg (av[ac], XmNdialogType, XmDIALOG_QUESTION); ac++;
  XtSetArg (av[ac], XmNdeleteResponse, XmDESTROY); ac++;
  XtSetArg (av[ac], XmNautoUnmanage, False); ac++;
  shell = XmCreateTemplateDialog (CONTEXT_WIDGET(context), "attach", av, ac);
/*  fe_UnmanageChild_safe (XmMessageBoxGetChild (shell, XmDIALOG_SEPARATOR)); */
  fe_UnmanageChild_safe (XmMessageBoxGetChild (shell, XmDIALOG_OK_BUTTON));
  fe_UnmanageChild_safe (XmMessageBoxGetChild (shell, XmDIALOG_CANCEL_BUTTON));
/*  fe_UnmanageChild_safe (XmMessageBoxGetChild (shell, XmDIALOG_APPLY_BUTTON));*/
  fe_UnmanageChild_safe (XmMessageBoxGetChild (shell, XmDIALOG_DEFAULT_BUTTON));
  fe_UnmanageChild_safe (XmMessageBoxGetChild (shell, XmDIALOG_HELP_BUTTON));

  ac = 0;
  ok_button = XmCreatePushButtonGadget (shell, "OK", av, ac);
  cancel_button = XmCreatePushButtonGadget (shell, "cancel", av, ac);

  ac = 0;
  XtSetArg (av[ac], XmNtopAttachment, XmATTACH_FORM); ac++;
  XtSetArg (av[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
  XtSetArg (av[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
  XtSetArg (av[ac], XmNbottomAttachment, XmATTACH_NONE); ac++;
  form = XmCreateForm (shell, "form", av, ac);

  ac = 0;
  XtSetArg (av[ac], XmNdialogStyle, XmDIALOG_WORK_AREA); ac++;
  XtSetArg (av[ac], XmNresizePolicy, XmRESIZE_GROW); ac++;
  messb = XmCreateMessageBox(form, "messagebox", av, ac);
  fe_UnmanageChild_safe(XmMessageBoxGetChild(messb, XmDIALOG_SEPARATOR));
  fe_UnmanageChild_safe(XmMessageBoxGetChild(messb, XmDIALOG_OK_BUTTON));
  fe_UnmanageChild_safe(XmMessageBoxGetChild(messb, XmDIALOG_CANCEL_BUTTON));
  fe_UnmanageChild_safe(XmMessageBoxGetChild(messb, XmDIALOG_HELP_BUTTON));
  ac = 0;
  list = XmCreateList(messb, "list", av, ac);
  attach_location = XmCreatePushButtonGadget(messb, "attachLocation", av, ac);
  attach_file = XmCreatePushButtonGadget(messb, "attachFile", av, ac);
  ac = 0;
  XtSetArg (av[ac], XmNsensitive, False); ac++;
  delete = XmCreatePushButtonGadget(messb, "delete", av, ac);

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

#ifdef dp_DEBUG
  fprintf(stderr, "Creating new fe_mail_attach_data...\n");
#endif
  data->mad = mad = (struct fe_mail_attach_data *)
    calloc (sizeof (struct fe_mail_attach_data), 1);
  mad->nattachments = 0;

  mad->context = context;
  mad->shell = shell;
  mad->list = list;
  mad->comppane = data->comppane;

  XtManageChild(list);
  ac = 0;
  kids [ac++] = attach_location;
  kids [ac++] = mad->attach_file = attach_file;
  kids [ac++] = mad->delete = delete;
  XtManageChildren (kids, ac);
  ac = 0;
  kids [ac++] = messb;
  kids [ac++] = label;
  kids [ac++] = mad->source_p = source_p;
  kids [ac++] = mad->text_p = text_p;
  kids [ac++] = mad->postscript_p = postscript_p;

  XtManageChildren (kids, ac);

  XtManageChild (form);
  XtManageChild (ok_button);
  XtManageChild (cancel_button);

  XtAddCallback (ok_button, XmNactivateCallback, fe_attachOk_cb, mad);
  XtAddCallback (cancel_button, XmNactivateCallback, fe_attachCancel_cb, mad);
  XtAddCallback (shell, XtNdestroyCallback, fe_attachDestroy_cb, mad);

  XtAddCallback (attach_location, XmNactivateCallback,
					fe_attach_location_cb, mad);
  XtAddCallback (attach_file, XmNactivateCallback, fe_attach_file_cb, mad);
  XtAddCallback (delete, XmNactivateCallback, fe_attach_delete_cb, mad);
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

#if 0
  /* Decide whether to attach a file or a URL based on what the URL is.
     If there's something in the Attachement: field already, use that.
     Otherwise, use the document associated with this mail window.
   */
  {
    char *string = 0;
    const char *url = 0;
    const char *file;

    string = fe_GetTextField (data->mcAttachments);

    url = fe_StringTrim(string);
    if (url && !*url) {
      url = 0;
    }

    if (!url) {
      url = MSG_GetAssociatedURL(context);
    }

    if (! url)
      file = 0;
    else if (url[0] == '/')
      file = url;
    else if (!strncasecomp (url, "file://localhost/", 17))
      file = url + 16;
    else if (!strncasecomp (url, "file://", 7))
      file = 0;
    else if (!strncasecomp (url, "file:/", 6))
      file = url + 5;
    else
      file = 0;

    if (file)
      fe_SetTextField (file_text, file);

    XtVaSetValues (doc_p, XmNset, True, 0);
    fe_SetTextField (doc_text, url);
    if (string) free (string);
  }
#endif

  fe_NukeBackingStore (shell);
  XtManageChild (shell);
}


/* Prompts for attachment info.
 */
void
fe_mailto_attach_dialog(MWContext* context)
{
  struct fe_mail_attach_data *mad = CONTEXT_DATA(context)->mad;
  const struct MSG_AttachmentData *list;
  int i;

  XP_ASSERT(context->type == MWContextMessageComposition);

  if (!mad || !mad->shell) {
    fe_make_attach_dialog(context);
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
  const char* s;
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
  fe_SetTextFieldAndCallBack(CONTEXT_DATA(compose_context)->mcAttachments,
		       s ? (char*) s : "");
}

#endif  /* 0 */

int
FE_GetMessageBody (MSG_Pane* comppane,
		   char **body,
		   uint32 *body_size,
		   MSG_FontCode **font_changes)
{
  MWContext* context = MSG_GetContext(comppane);
  if (context->type != MWContextMessageComposition) return -1;
  fe_getMessageBody(context, body, body_size, font_changes);
  return 0;
}

void
FE_DoneWithMessageBody(MSG_Pane* comppane, char* body, uint32 body_size)
{
  MWContext* context = MSG_GetContext(comppane);
  fe_doneWithMessageBody(context, body, body_size);
}


void
fe_mail_text_modify_cb (Widget text, XtPointer client_data,
			XtPointer call_data)
{
  MWContext* context = (MWContext*) client_data;
  CONTEXT_DATA(context)->mcCitedAndUnedited = False;
  CONTEXT_DATA(context)->mcEdited = True;
  /*  MSG_MessageBodyEdited(CONTEXT_DATA(context)->comppane); */
}

#endif  /* MOZ_MAIL_NEWS */

#define cite_abort 0
#define cite_protect_me_from_myself 1
#define cite_let_me_be_a_loser 2

#if 0
static int
FE_BogusQuotationQuery (MWContext *context, Boolean double_p)
{
  struct fe_confirm_data data;
  Widget dialog;
  Arg av [20];
  int ac;
  Visual *v = 0;
  Colormap cmap = 0;
  Cardinal depth = 0;
  Widget parent = CONTEXT_WIDGET (context);
  XtVaGetValues (parent, XtNvisual, &v, XtNcolormap, &cmap,
		 XtNdepth, &depth, 0);
  ac = 0;
  XtSetArg (av[ac], XmNvisual, v); ac++;
  XtSetArg (av[ac], XmNdepth, depth); ac++;
  XtSetArg (av[ac], XmNcolormap, cmap); ac++;
  XtSetArg (av[ac], XmNallowShellResize, TRUE); ac++;
  XtSetArg (av[ac], XmNtransientFor, parent); ac++;
  XtSetArg (av[ac], XmNdialogStyle, XmDIALOG_PRIMARY_APPLICATION_MODAL); ac++;
  XtSetArg (av[ac], XmNdialogType, XmDIALOG_QUESTION); ac++;
  XtSetArg (av[ac], XmNdeleteResponse, XmDESTROY); ac++;
  XtSetArg (av[ac], XmNautoUnmanage, False); ac++;
  dialog = XmCreatePromptDialog (parent, (double_p ?
					  "citationQuery"
					  : "doubleCitationQuery")
				 , av, ac);

/*  fe_UnmanageChild_safe (XmSelectionBoxGetChild (dialog, XmDIALOG_SEPARATOR));*/
  fe_UnmanageChild_safe (XmSelectionBoxGetChild (dialog, XmDIALOG_TEXT));
  XtManageChild (XmSelectionBoxGetChild (dialog, XmDIALOG_SELECTION_LABEL));
  XtManageChild (XmSelectionBoxGetChild (dialog, XmDIALOG_APPLY_BUTTON));
#ifdef NO_HELP
  fe_UnmanageChild_safe (XmSelectionBoxGetChild (dialog, XmDIALOG_HELP_BUTTON));
#endif

  XtManageChild (dialog);

  XtAddCallback (dialog, XmNokCallback, fe_destroy_ok_cb, &data);
  XtAddCallback (dialog, XmNapplyCallback, fe_destroy_apply_cb, &data);
  XtAddCallback (dialog, XmNcancelCallback, fe_destroy_cancel_cb, &data);
  XtAddCallback (dialog, XmNdestroyCallback, fe_destroy_finish_cb, &data);

  data.context = context;
  data.widget = dialog;
  data.answer = Answer_Invalid;

  while (data.answer == Answer_Invalid)
    fe_EventLoop ();

  return (data.answer == 0 ? cite_abort :
	  data.answer == 1 ? cite_protect_me_from_myself :
	  data.answer == 2 ? cite_let_me_be_a_loser : 99);
}
#endif


/* Print setup
 */

struct fe_print_data
{
  MWContext *context;
  History_entry *hist;
  Widget shell;
  Widget printer, file, command_text, file_text, browse;
  Widget first, last;
  Widget portrait, landscape;
  Widget grey, color;
  Widget letter, legal, exec, a4;
};

static void
fe_print_to_toggle_cb (Widget widget, XtPointer closure, XtPointer call_data)
{
  struct fe_print_data *fpd = (struct fe_print_data *) closure;
  XmToggleButtonCallbackStruct *cb = (XmToggleButtonCallbackStruct *)call_data;
  if (!cb->set)
    XtVaSetValues (widget, XmNset, True, 0);
  else if (widget == fpd->printer)
    {
      XtVaSetValues (fpd->file, XmNset, False, 0);
      XtVaSetValues (fpd->browse, XmNsensitive, False, 0);
      XtVaSetValues (fpd->file_text, XmNsensitive, False, 0);
      XtVaSetValues (fpd->command_text, XmNsensitive, True, 0);
      XmProcessTraversal (fpd->command_text, XmTRAVERSE_CURRENT);
    }
  else if (widget == fpd->file)
    {
      XtVaSetValues (fpd->printer, XmNset, False, 0);
      XtVaSetValues (fpd->browse, XmNsensitive, True, 0);
      XtVaSetValues (fpd->file_text, XmNsensitive, True, 0);
      XtVaSetValues (fpd->command_text, XmNsensitive, False, 0);
      XmProcessTraversal (fpd->file_text, XmTRAVERSE_CURRENT);
    }
  else
    abort ();
}

static void
fe_print_order_toggle_cb (Widget widget, XtPointer closure,XtPointer call_data)
{
  struct fe_print_data *fpd = (struct fe_print_data *) closure;
  XmToggleButtonCallbackStruct *cb = (XmToggleButtonCallbackStruct *)call_data;
  if (!cb->set)
    XtVaSetValues (widget, XmNset, True, 0);
  else if (widget == fpd->first)
    XtVaSetValues (fpd->last, XmNset, False, 0);
  else if (widget == fpd->last)
    XtVaSetValues (fpd->first, XmNset, False, 0);
  else
    abort ();
}

static void
fe_print_orientation_toggle_cb (Widget widget, XtPointer closure,
				XtPointer call_data)
{
  struct fe_print_data *fpd = (struct fe_print_data *) closure;
  XmToggleButtonCallbackStruct *cb = (XmToggleButtonCallbackStruct *)call_data;
  if (!cb->set)
    XtVaSetValues (widget, XmNset, True, 0);
  else if (widget == fpd->portrait)
    XtVaSetValues (fpd->landscape, XmNset, False, 0);
  else if (widget == fpd->landscape)
    XtVaSetValues (fpd->portrait, XmNset, False, 0);
  else
    abort ();
}

static void
fe_print_color_toggle_cb (Widget widget, XtPointer closure,
			  XtPointer call_data)
{
  struct fe_print_data *fpd = (struct fe_print_data *) closure;
  XmToggleButtonCallbackStruct *cb = (XmToggleButtonCallbackStruct *)call_data;
  if (!cb->set)
    XtVaSetValues (widget, XmNset, True, 0);
  else if (widget == fpd->grey)
    XtVaSetValues (fpd->color, XmNset, False, 0);
  else if (widget == fpd->color)
    XtVaSetValues (fpd->grey, XmNset, False, 0);
  else
    abort ();
}

static void
fe_print_paper_toggle_cb (Widget widget, XtPointer closure,
			  XtPointer call_data)
{
  struct fe_print_data *fpd = (struct fe_print_data *) closure;
  XmToggleButtonCallbackStruct *cb = (XmToggleButtonCallbackStruct *)call_data;
  if (!cb->set)
    XtVaSetValues (widget, XmNset, True, 0);
  else if (widget == fpd->letter)
    {
      XtVaSetValues (fpd->legal,  XmNset, False, 0);
      XtVaSetValues (fpd->exec,   XmNset, False, 0);
      XtVaSetValues (fpd->a4,     XmNset, False, 0);
    }
  else if (widget == fpd->legal)
    {
      XtVaSetValues (fpd->letter, XmNset, False, 0);
      XtVaSetValues (fpd->exec,   XmNset, False, 0);
      XtVaSetValues (fpd->a4,     XmNset, False, 0);
    }
  else if (widget == fpd->exec)
    {
      XtVaSetValues (fpd->letter, XmNset, False, 0);
      XtVaSetValues (fpd->legal,  XmNset, False, 0);
      XtVaSetValues (fpd->a4,     XmNset, False, 0);
    }
  else if (widget == fpd->a4)
    {
      XtVaSetValues (fpd->letter, XmNset, False, 0);
      XtVaSetValues (fpd->legal,  XmNset, False, 0);
      XtVaSetValues (fpd->exec,   XmNset, False, 0);
    }
  else
    abort ();
}

void
fe_browse_file_of_text (MWContext *context, Widget text_field, Boolean dirp)
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
  struct fe_confirm_data data;
  data.context = context;

  /* Find the top level window of which this text field is a descendant,
     and make the file requester be a transient for that. */
  parent = text_field;
  while (parent && !XtIsShell (parent))
    parent = XtParent (parent);
  assert (parent);
  if (! parent)
    parent = CONTEXT_WIDGET (context);

  XtVaGetValues (parent, XtNvisual, &v, XtNcolormap, &cmap,
		 XtNdepth, &depth, 0);
  text = fe_GetTextField(text_field);
  text = fe_StringTrim (text);


  if ( text && *text )
  	text = XP_STRTOK(text, " ");
 
  if (!text || !*text)
    {
      xmpat = 0;
      xmfile = 0;
    }
  else if (dirp)
    {
      if (text [strlen (text) - 1] == '/')
	text [strlen (text) - 1] = 0;
      PR_snprintf (buf, sizeof (buf), "%.900s/*", text);
      xmpat = XmStringCreateLtoR (buf, XmFONTLIST_DEFAULT_TAG);
      xmfile = XmStringCreateLtoR (text, XmFONTLIST_DEFAULT_TAG);
    }
  else
    {
      char *f;
      if (text [strlen (text) - 1] == '/')
	PR_snprintf (buf, sizeof (buf), "%.900s/*", text);
      else
	PR_snprintf (buf, sizeof (buf), "%.900s", text);
      xmfile = XmStringCreateLtoR (buf, XmFONTLIST_DEFAULT_TAG);
      if (text[0] == '/') /* only do this for absolute path */
	  f = strrchr (text, '/');
      else
	  f = NULL;
      if (f && f != text)
	*f = 0;
      if (f) {
        PR_snprintf (buf, sizeof (buf), "%.900s/*", text);
        xmpat = XmStringCreateLtoR (buf, XmFONTLIST_DEFAULT_TAG);
	  }
      else {
			/* do not change dirmask and pattern if input is a file;
			 * otherwise, the text widget in the file selection box
			 * will insert the file name at the wrong position.
			 * Windows version has similar behavior if a relative file
			 * is entered.
			 */
		  buf[0] = '\0';		/* input was just a file. no '/' in it. */
		  xmpat = 0;
	  }
    }
  if (text) free (text);

  ac = 0;
  XtSetArg (av[ac], XmNvisual, v); ac++;
  XtSetArg (av[ac], XmNdepth, depth); ac++;
  XtSetArg (av[ac], XmNcolormap, cmap); ac++;
  XtSetArg (av[ac], XmNdeleteResponse, XmDESTROY); ac++;
/*  XtSetArg (av[ac], XmNallowShellResize, True); ac++;*/
  shell = XmCreateDialogShell (parent, "fileBrowser_popup", av, ac);
  ac = 0;
  XtSetArg (av[ac], XmNdialogStyle, XmDIALOG_PRIMARY_APPLICATION_MODAL); ac++;
  XtSetArg (av[ac], XmNfileTypeMask,
	    (dirp ? XmFILE_DIRECTORY : XmFILE_REGULAR)); ac++;
  fileb = fe_CreateFileSelectionBox (shell, "fileBrowser", av, ac);

#ifdef NO_HELP
  fe_UnmanageChild_safe (XmSelectionBoxGetChild (fileb, XmDIALOG_HELP_BUTTON));
#endif

  if (xmpat)
    {
      XtVaSetValues (fileb,
		     XmNdirMask, xmpat,
		     XmNpattern, xmpat, 0);
#if 0
      /*
       *    The XtVaSetValues on dirMask/pattern will cause this anyway.
       */
      XmFileSelectionDoSearch (fileb, xmpat);
#endif
      XtVaSetValues (fileb, XmNdirSpec, xmfile, 0);
      XmStringFree (xmpat);
      XmStringFree (xmfile);
    }

  XtAddCallback (fileb, XmNnoMatchCallback, fe_file_cb, &data);
  XtAddCallback (fileb, XmNokCallback,      fe_file_cb, &data);
  XtAddCallback (fileb, XmNcancelCallback,  fe_file_cb, &data);
  XtAddCallback (fileb, XmNdestroyCallback, fe_destroy_finish_cb, &data);

  data.answer = Answer_Invalid;
  data.return_value = 0;

  fe_HackDialogTranslations (fileb);

  fe_NukeBackingStore (fileb);
  XtManageChild (fileb);
  /* #### check for destruction here */
  while (data.answer == Answer_Invalid)
    fe_EventLoop ();

  if (data.answer == Answer_OK)
    if (data.return_value)
      {
	fe_SetTextField(text_field, data.return_value);
	free (data.return_value);
      }
  if (data.answer != Answer_Destroy)
    XtDestroyWidget(shell);
}

void
fe_browse_file_of_text_in_url (MWContext *context, Widget text_field, Boolean dirp)
{
	char *orig_text = 0;
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
	struct fe_confirm_data data;
	data.context = context;

	/* Find the top level window of which this text field is a descendant,
	   and make the file requester be a transient for that. */

	parent = text_field;
	while (parent && !XtIsShell (parent))
		parent = XtParent (parent);
	assert (parent);
	if (! parent)
		parent = CONTEXT_WIDGET (context);

	XtVaGetValues (parent, XtNvisual, &v, XtNcolormap, &cmap,
				   XtNdepth, &depth, 0);
	text = fe_GetTextField(text_field);
	orig_text = text;
	text = fe_StringTrim (text);

	if (!strncasecomp (text, "http://", 7)) {
		/* ignore url using http */
		free(orig_text);
		orig_text = 0;
		text = 0;
	}

	if (text) {
		if (!strncasecomp (text, "file://", 7)) {
			/* get to the absolute file path */
			text += 7;
		}
	}

	if ( text && *text )
		text = XP_STRTOK(text, " ");
 
	if (!text || !*text){
		xmpat = 0;
		xmfile = 0;
	}
	else if (dirp){
		if (text [strlen (text) - 1] == '/')
			text [strlen (text) - 1] = 0;
		PR_snprintf (buf, sizeof (buf), "%.900s/*", text);
		xmpat = XmStringCreateLtoR (buf, XmFONTLIST_DEFAULT_TAG);
		xmfile = XmStringCreateLtoR (text, XmFONTLIST_DEFAULT_TAG);
	}
	else {
		char *f;
		if (text [strlen (text) - 1] == '/')
			PR_snprintf (buf, sizeof (buf), "%.900s/*", text);
		else
			PR_snprintf (buf, sizeof (buf), "%.900s", text);
		xmfile = XmStringCreateLtoR (buf, XmFONTLIST_DEFAULT_TAG);
		if (text[0] == '/') /* only do this for absolute path */
			f = strrchr (text, '/');
		else
			f = NULL;
		if (f && f != text)
			*f = 0;
		if (f) {
			PR_snprintf (buf, sizeof (buf), "%.900s/*", text);
			xmpat = XmStringCreateLtoR (buf, XmFONTLIST_DEFAULT_TAG);
		}
		else {
			/* Do not change dirmask and pattern if input is a file;
			 * otherwise, the text widget in the file selection box
			 * will insert the file name at the wrong position.
			 * Windows version has similar behavior if a relative file
			 * is entered.
			 */
			buf[0] = '\0';		/* input was just a file. no '/' in it. */
			xmpat = 0;
		}
	}
	if (orig_text) free (orig_text);

	ac = 0;
	XtSetArg (av[ac], XmNvisual, v); ac++;
	XtSetArg (av[ac], XmNdepth, depth); ac++;
	XtSetArg (av[ac], XmNcolormap, cmap); ac++;
	XtSetArg (av[ac], XmNdeleteResponse, XmDESTROY); ac++;
	/*  XtSetArg (av[ac], XmNallowShellResize, True); ac++;*/
	shell = XmCreateDialogShell (parent, "fileBrowser_popup", av, ac);
	ac = 0;
	XtSetArg (av[ac], XmNdialogStyle, XmDIALOG_PRIMARY_APPLICATION_MODAL); ac++;
	XtSetArg (av[ac], XmNfileTypeMask, (dirp ? XmFILE_DIRECTORY : XmFILE_REGULAR)); ac++;
	fileb = fe_CreateFileSelectionBox (shell, "fileBrowser", av, ac);

#ifdef NO_HELP
	fe_UnmanageChild_safe (XmSelectionBoxGetChild (fileb, XmDIALOG_HELP_BUTTON));
#endif

	if (xmpat) {
		XtVaSetValues (fileb,
					   XmNdirMask, xmpat,
					   XmNpattern, xmpat, 0);
#if 0
		/*
		 *    The XtVaSetValues on dirMask/pattern will cause this anyway.
		 */
		XmFileSelectionDoSearch (fileb, xmpat);
#endif
		XtVaSetValues (fileb, XmNdirSpec, xmfile, 0);
		XmStringFree (xmpat);
		XmStringFree (xmfile);
	}

	XtAddCallback (fileb, XmNnoMatchCallback, fe_file_cb, &data);
	XtAddCallback (fileb, XmNokCallback,      fe_file_cb, &data);
	XtAddCallback (fileb, XmNcancelCallback,  fe_file_cb, &data);
	XtAddCallback (fileb, XmNdestroyCallback, fe_destroy_finish_cb, &data);

	data.answer = Answer_Invalid;
	data.return_value = 0;

	fe_HackDialogTranslations (fileb);

	fe_NukeBackingStore (fileb);
	XtManageChild (fileb);
	/* #### check for destruction here */
	while (data.answer == Answer_Invalid)
		fe_EventLoop ();

	if (data.answer == Answer_OK)
		if (data.return_value) {
			/* prepend the answer with file url */
			char path[1025];
			sprintf(path, "file://%s", data.return_value);
			fe_SetTextField (text_field, path);
			free (data.return_value);
		}
	if (data.answer != Answer_Destroy)
		XtDestroyWidget(shell);
}


static void
fe_print_browse_cb (Widget widget, XtPointer closure, XtPointer call_data)
{
  struct fe_print_data *fpd = (struct fe_print_data *) closure;
  fe_browse_file_of_text (fpd->context, fpd->file_text, False);
}


#define XFE_DEFAULT_PRINT_FILENAME	"netscape.ps"
static char *last_print_file_name = 0;
static Boolean last_to_file_p = False;

static void
ps_pipe_close (PrintSetup *p)
{
  fe_synchronous_url_exit (p->url, 0, (MWContext *) p->carg);
  pclose (p->out);
}

static void
ps_file_close (PrintSetup *p)
{
  fe_synchronous_url_exit (p->url, 0, (MWContext *) p->carg);
  fclose (p->out);
}

void
XFE_InitializePrintSetup (PrintSetup *p)
{
  XL_InitializePrintSetup (p);
  p->reverse = fe_globalPrefs.print_reversed;
  p->color = fe_globalPrefs.print_color;
  p->landscape = fe_globalPrefs.print_landscape;
/*  #### p->n_up = fe_globalPrefs.print_n_up;*/

  /* @@@ need to fix this -- erik */
  p->bigger = 0; /* -1 = small, 0 = medium, 1 = large, 2 = huge */

  if (fe_globalPrefs.print_paper_size == 0)
    {
      p->width = 8.5 * 72;
      p->height = 11  * 72;
    }
  else if (fe_globalPrefs.print_paper_size == 1)
    {
      p->width = 8.5 * 72;
      p->height = 14  * 72;
    }
  else if (fe_globalPrefs.print_paper_size == 2)
    {
      p->width = 7.5 * 72;
      p->height = 10  * 72;
    }
  else if (fe_globalPrefs.print_paper_size == 3)
    {
      p->width = 210 * 0.039 * 72;
      p->height = 297 * 0.039 * 72;
    }
}


void
fe_Print(MWContext *context, URL_Struct *url, Boolean last_to_file_p,
		char *last_print_file_name)
{
  SHIST_SavedData saved_data;
  PrintSetup p;
  char *type = NULL;
  char name[1024];
  char clas[1024];
  char mimecharset[48];
  XrmValue value;
  XrmDatabase db = XtDatabase(XtDisplay(CONTEXT_WIDGET(context)));
  INTL_CharSetInfo c = LO_GetDocumentCharacterSetInfo(context);

  XFE_InitializePrintSetup (&p);

  if (last_to_file_p)
    {
      if (! *last_print_file_name)
	{
	  FE_Alert (context, fe_globalData.no_file_message);
	  return;
	}

      /* If the file exists, confirm overwriting it. */
      {
	XP_StatStruct st;
	char buf [2048];
	if (!stat (last_print_file_name, &st))
	  {
	    PR_snprintf (buf, sizeof (buf),
			fe_globalData.overwrite_file_message,
			last_print_file_name);
	    if (!FE_Confirm (context, buf))
	      return;
	  }
      }

      p.out = fopen (last_print_file_name, "w");
      if (! p.out)
	{
	  char buf [2048];
	  PR_snprintf (buf, sizeof (buf),
			XP_GetString(XFE_ERROR_OPENING_FILE),
	   		last_print_file_name);
	  fe_perror (context, buf);
	  return;
	}
    }
  else
    {
      fe_globalPrefs.print_command =
	fe_StringTrim (fe_globalPrefs.print_command);
      if (! *fe_globalPrefs.print_command)
	{
	  FE_Alert (context, fe_globalData.no_print_command_message);
	  return;
	}
      p.out = popen (fe_globalPrefs.print_command, "w");
      if (! p.out)
	{
	  char buf [2048];
	  PR_snprintf (buf, sizeof (buf),
		    XP_GetString(XFE_ERROR_OPENING_PIPE),
		    fe_globalPrefs.print_command);
	  fe_perror (context, buf);
	  return;
	}
    }

  /* Make sure layout saves the current state. */
  LO_SaveFormData(context);

  /* Hold on to the saved data. */
  XP_MEMCPY(&saved_data, &url->savedData, sizeof(SHIST_SavedData));

  /* make damn sure the form_data slot is zero'd or else all
   * hell will break loose
   */
  XP_MEMSET (&url->savedData, 0, sizeof (SHIST_SavedData));
  NPL_PreparePrint(context, &url->savedData);

  INTL_CharSetIDToName(INTL_GetCSIWinCSID(c), mimecharset);

  PR_snprintf(clas, sizeof (clas),
		"%s.DocumentFonts.Charset.PSName", fe_progclass);
  PR_snprintf(name, sizeof (name),
		"%s.documentFonts.%s.psname", fe_progclass, mimecharset);
  if (XrmGetResource(db, name, clas, &type, &value))
      p.otherFontName = value.addr;
  else
      p.otherFontName = NULL;

  PR_snprintf(clas, sizeof (clas),
		"%s.DocumentFonts.Charset.PSCode", fe_progclass);
  PR_snprintf(name, sizeof (name),
		"%s.documentFonts.%s.pscode", fe_progclass, mimecharset);
  if (XrmGetResource(db, name, clas, &type, &value))
      p.otherFontCharSetID = INTL_CharSetNameToID(value.addr);

  PR_snprintf(clas, sizeof (clas),
		"%s.DocumentFonts.Charset.PSWidth", fe_progclass);
  PR_snprintf(name, sizeof (name),
		"%s.documentFonts.%s.pswidth", fe_progclass, mimecharset);
  if (XrmGetResource(db, name, clas, &type, &value))
      p.otherFontWidth = atoi(value.addr);

  PR_snprintf(clas, sizeof (clas),
		"%s.DocumentFonts.Charset.PSAscent", fe_progclass);
  PR_snprintf(name, sizeof (name),
		"%s.documentFonts.%s.psascent", fe_progclass, mimecharset);
  if (XrmGetResource(db, name, clas, &type, &value))
      p.otherFontAscent = atoi(value.addr);

  if (last_to_file_p)
      p.completion = ps_file_close;
  else
      p.completion = ps_pipe_close;
  
  p.carg = context;
  fe_RaiseSynchronousURLDialog (context, CONTEXT_WIDGET (context),
				XP_GetString(XFE_DIALOGS_PRINTING));
  XL_TranslatePostscript (context, url, &saved_data, &p);
  fe_await_synchronous_url (context);

  /* XXX do we need to delete the URL ? */
}


static void
fe_print_destroy_cb(Widget widget, XtPointer closure, XtPointer call_data)
{
  struct fe_print_data *fpd = (struct fe_print_data *) closure;
  if (!fpd) return;

  /* Remove this callback so that we make absolutely sure we wont
   * free the fpd again.
   */
  XtRemoveCallback(widget, XmNdestroyCallback, fe_print_destroy_cb, fpd);
  free(fpd);
}

static void
fe_print_cb (Widget widget, XtPointer closure, XtPointer call_data)
{
  struct fe_print_data *fpd = (struct fe_print_data *) closure;
  URL_Struct *url;
  Boolean b;

  /*
   * Pop down the print dialog immediately.
   */
  fe_UnmanageChild_safe (fpd->shell);

  XtVaGetValues (fpd->printer, XmNset, &b, 0);
  last_to_file_p = !b;
  if (fe_globalPrefs.print_command) free (fe_globalPrefs.print_command);
  XtVaGetValues (fpd->command_text, XmNvalue, &fe_globalPrefs.print_command,0);
  if (last_print_file_name) free (last_print_file_name);
  last_print_file_name = fe_GetTextField(fpd->file_text);
  XtVaGetValues (fpd->portrait, XmNset, &b, 0);
  fe_globalPrefs.print_landscape = !b;
  XtVaGetValues (fpd->last, XmNset, &b, 0);
  fe_globalPrefs.print_reversed = b;
  XtVaGetValues (fpd->grey, XmNset, &b, 0);
  fe_globalPrefs.print_color = !b;
  XtVaGetValues (fpd->letter, XmNset, &b, 0);
  if (b) fe_globalPrefs.print_paper_size = 0;
  XtVaGetValues (fpd->legal, XmNset, &b, 0);
  if (b) fe_globalPrefs.print_paper_size = 1;
  XtVaGetValues (fpd->exec, XmNset, &b, 0);
  if (b) fe_globalPrefs.print_paper_size = 2;
  XtVaGetValues (fpd->a4, XmNset, &b, 0);
  if (b) fe_globalPrefs.print_paper_size = 3;

  url = SHIST_CreateWysiwygURLStruct (fpd->context, fpd->hist);
  if (url) {
    last_print_file_name = fe_StringTrim (last_print_file_name);
    
    fe_Print(fpd->context, url, last_to_file_p, last_print_file_name);
  } else {
    FE_Alert(fpd->context, fe_globalData.no_url_loaded_message);
  }
  
  /* We need this check to see if this widget is being_destroyed
   * or not in its ok_callback because fe_Print() goes off and
   * calls fe_await_synchronous_url() while has a mini eventloop
   * in it. This could cause a destroy of this widget. Motif, in
   * all its smartness, keeps this widget's memory around. This
   * is the only think we can check. context, shell, fpd all might
   * have been destroyed and deallocated.
   */
  if (XfeIsAlive(widget)) {
    XtDestroyWidget (fpd->shell);
  }
}


void
fe_PrintDialog (MWContext *context)
{
  Widget mainw = CONTEXT_WIDGET (context);
  Widget kids [50];
  Arg av [20];
  int ac, i, labels_width;
  Widget shell, form;
  Widget print_to_label, print_command_label;
  Widget print_command_text, file_name_label, file_name_text;
  Widget browse_button, line, print_label;
  Widget first_first_toggle, last_first_toggle, orientation_label;
  Widget portrait_toggle, landscape_toggle;
  Widget print_color_label, greyscale_toggle;
  Widget color_toggle, paper_size_label, paper_size_radio_box, letter_toggle;
  Widget legal_toggle, executive_toggle, a4_toggle;
  Widget to_printer_toggle, to_file_toggle;

  Visual *v = 0;
  Colormap cmap = 0;
  Cardinal depth = 0;

  struct fe_print_data *fpd = (struct fe_print_data *)
    calloc (sizeof (struct fe_print_data), 1);

  XtVaGetValues (mainw, XtNvisual, &v, XtNcolormap, &cmap,
		 XtNdepth, &depth, 0);

  ac = 0;
  XtSetArg (av[ac], XmNvisual, v); ac++;
  XtSetArg (av[ac], XmNdepth, depth); ac++;
  XtSetArg (av[ac], XmNcolormap, cmap); ac++;
  XtSetArg (av[ac], XmNallowShellResize, TRUE); ac++;
  XtSetArg (av[ac], XmNtransientFor, mainw); ac++;
  XtSetArg (av[ac], XmNdialogStyle, XmDIALOG_PRIMARY_APPLICATION_MODAL); ac++;
  XtSetArg (av[ac], XmNdialogType, XmDIALOG_QUESTION); ac++;
  XtSetArg (av[ac], XmNautoUnmanage, False); ac++;
  XtSetArg (av[ac], XmNuserData, False); ac++;
  shell = XmCreatePromptDialog (mainw, "printSetup", av, ac);

  XtAddCallback (shell, XmNokCallback, fe_print_cb, fpd);
  XtAddCallback (shell, XmNcancelCallback, fe_destroy_cb, shell);

  fe_UnmanageChild_safe (XmSelectionBoxGetChild (shell,
						XmDIALOG_SELECTION_LABEL));
  fe_UnmanageChild_safe (XmSelectionBoxGetChild (shell, XmDIALOG_TEXT));
  fe_UnmanageChild_safe (XmSelectionBoxGetChild (shell, XmDIALOG_APPLY_BUTTON));
#ifdef NO_HELP
  fe_UnmanageChild_safe (XmSelectionBoxGetChild (shell, XmDIALOG_HELP_BUTTON));
#endif
  XtAddCallback (shell, XmNdestroyCallback, fe_print_destroy_cb, fpd);

  i = 0;
  ac = 0;
  form = XmCreateForm (shell, "form", av, ac);
  kids [i++] = print_to_label =
    XmCreateLabelGadget (form, "printToLabel", av, ac);
  kids [i++] = to_printer_toggle =
    XmCreateToggleButtonGadget (form, "toPrinterToggle", av, ac);
  kids [i++] = to_file_toggle =
    XmCreateToggleButtonGadget (form, "toFileToggle", av, ac);
  kids [i++] = print_command_label =
    XmCreateLabelGadget (form, "printCommandLabel", av, ac);
  kids [i++] = print_command_text =
    fe_CreateTextField (form, "printCommandText", av, ac);
  kids [i++] = file_name_label =
    XmCreateLabelGadget (form, "fileNameLabel", av, ac);
  kids [i++] = file_name_text =
    fe_CreateTextField (form, "fileNameText", av, ac);
  kids [i++] = browse_button =
    XmCreatePushButtonGadget (form, "browseButton", av, ac);
  kids [i++] = line = XmCreateSeparator (form, "line", av, ac);
  kids [i++] = print_label = XmCreateLabelGadget (form, "printLabel", av, ac);
  kids [i++] = first_first_toggle =
    XmCreateToggleButtonGadget (form, "firstFirstToggle", av, ac);
  kids [i++] = last_first_toggle =
    XmCreateToggleButtonGadget (form, "lastFirstToggle", av, ac);
  kids [i++] = orientation_label =
    XmCreateLabelGadget (form, "orientationLabel", av, ac);
  kids [i++] = portrait_toggle =
    XmCreateToggleButtonGadget (form, "portraitToggle", av, ac);
  kids [i++] = landscape_toggle =
    XmCreateToggleButtonGadget (form, "landscapeToggle", av, ac);
  kids [i++] = print_color_label =
    XmCreateLabelGadget (form, "printColorLabel", av, ac);
  kids [i++] = greyscale_toggle =
    XmCreateToggleButtonGadget (form, "greyscaleToggle", av, ac);
  kids [i++] = color_toggle =
    XmCreateToggleButtonGadget (form, "colorToggle", av, ac);
  kids [i++] = paper_size_label =
    XmCreateLabelGadget (form, "paperSizeLabel", av, ac);
  kids [i++] = paper_size_radio_box =
    XmCreateRadioBox (form, "paperSizeRadioBox", av, ac);
  kids [i++] = letter_toggle =
    XmCreateToggleButtonGadget (form, "letterToggle", av, ac);
  kids [i++] = legal_toggle =
    XmCreateToggleButtonGadget (form, "legalToggle", av, ac);
  kids [i++] = executive_toggle =
    XmCreateToggleButtonGadget (form, "executiveToggle", av, ac);
  kids [i++] = a4_toggle =
    XmCreateToggleButtonGadget (form, "a4Toggle", av, ac);

  labels_width = XfeVaGetWidestWidget(print_to_label, print_command_label,
			     file_name_label, 0);

  XtVaSetValues (print_to_label,
  		 XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
  		 XmNtopWidget, to_printer_toggle,
  		 XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
  		 XmNbottomWidget, to_printer_toggle,
		 RIGHT_JUSTIFY_VA_ARGS(print_to_label,labels_width),
  		 0);
  XtVaSetValues (to_printer_toggle,
  		 XmNtopAttachment, XmATTACH_FORM,
  		 XmNbottomAttachment, XmATTACH_NONE,
		 XmNleftAttachment, XmATTACH_FORM,
                 XmNleftOffset, labels_width,
  		 XmNrightAttachment, XmATTACH_NONE,
  		 0);
  XtVaSetValues (to_file_toggle,
		 XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
		 XmNtopWidget, to_printer_toggle,
		 XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
		 XmNbottomWidget, to_printer_toggle,
		 XmNleftAttachment, XmATTACH_WIDGET,
		 XmNleftWidget, to_printer_toggle,
		 XmNrightAttachment, XmATTACH_NONE,
		 0);
  XtVaSetValues (print_command_label,
		 XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
		 XmNtopWidget, print_command_text,
		 XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
		 XmNbottomWidget, print_command_text,
		 RIGHT_JUSTIFY_VA_ARGS(print_command_label,labels_width),
		 0);
  XtVaSetValues (print_command_text,
		 XmNtopAttachment, XmATTACH_WIDGET,
		 XmNtopWidget, to_printer_toggle,
		 XmNbottomAttachment, XmATTACH_NONE,
		 XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
		 XmNleftWidget, to_printer_toggle,
		 XmNrightAttachment, XmATTACH_FORM,
		 0);
  XtVaSetValues (file_name_label,
		 XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
		 XmNtopWidget, file_name_text,
		 XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
		 XmNbottomWidget, file_name_text,
		 RIGHT_JUSTIFY_VA_ARGS(file_name_label,labels_width),
		 0);
  XtVaSetValues (file_name_text,
		 XmNtopAttachment, XmATTACH_WIDGET,
		 XmNtopWidget, print_command_text,
		 XmNbottomAttachment, XmATTACH_NONE,
		 XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
		 XmNleftWidget, print_command_text,
		 XmNrightAttachment, XmATTACH_WIDGET,
		 XmNrightWidget, browse_button,
		 0);
  XtVaSetValues (browse_button,
		 XmNtopAttachment, XmATTACH_WIDGET,
		 XmNtopWidget, print_command_text,
		 XmNbottomAttachment, XmATTACH_NONE,
		 XmNleftAttachment, XmATTACH_NONE,
		 XmNrightAttachment, XmATTACH_FORM,
		 0);

  XtVaSetValues (browse_button, XmNheight, _XfeHeight(file_name_text), 0);

  XtVaSetValues (line,
		 XmNtopAttachment, XmATTACH_WIDGET,
		 XmNtopWidget, file_name_text,
		 XmNbottomAttachment, XmATTACH_NONE,
		 XmNleftAttachment, XmATTACH_FORM,
		 XmNrightAttachment, XmATTACH_FORM,
		 0);

  XtVaSetValues (print_label,
		 XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
		 XmNtopWidget, first_first_toggle,
		 XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
		 XmNbottomWidget, first_first_toggle,
		 XmNleftAttachment, XmATTACH_FORM,
		 XmNrightAttachment, XmATTACH_WIDGET,
		 XmNrightWidget, first_first_toggle,
		 0);
  XtVaSetValues (first_first_toggle,
		 XmNtopAttachment, XmATTACH_WIDGET,
		 XmNtopWidget, line,
		 XmNbottomAttachment, XmATTACH_NONE,
		 XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
		 XmNleftWidget, print_command_text,
		 XmNrightAttachment, XmATTACH_NONE,
		 0);
  XtVaSetValues (last_first_toggle,
		 XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
		 XmNtopWidget, first_first_toggle,
		 XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
		 XmNbottomWidget, first_first_toggle,
		 XmNleftAttachment, XmATTACH_WIDGET,
		 XmNleftWidget, first_first_toggle,
		 XmNrightAttachment, XmATTACH_NONE,
		 0);

  XtVaSetValues (orientation_label,
		 XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
		 XmNtopWidget, portrait_toggle,
		 XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
		 XmNbottomWidget, portrait_toggle,
		 XmNleftAttachment, XmATTACH_FORM,
		 XmNrightAttachment, XmATTACH_WIDGET,
		 XmNrightWidget, portrait_toggle,
		 0);
  XtVaSetValues (portrait_toggle,
		 XmNtopAttachment, XmATTACH_WIDGET,
		 XmNtopWidget, first_first_toggle,
		 XmNbottomAttachment, XmATTACH_NONE,
		 XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
		 XmNleftWidget, first_first_toggle,
		 XmNrightAttachment, XmATTACH_NONE,
		 0);
  XtVaSetValues (landscape_toggle,
		 XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
		 XmNtopWidget, portrait_toggle,
		 XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
		 XmNbottomWidget, portrait_toggle,
		 XmNleftAttachment, XmATTACH_WIDGET,
		 XmNleftWidget, portrait_toggle,
		 XmNrightAttachment, XmATTACH_NONE,
		 0);

  XtVaSetValues (print_color_label,
		 XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
		 XmNtopWidget, greyscale_toggle,
		 XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
		 XmNbottomWidget, greyscale_toggle,
		 XmNleftAttachment, XmATTACH_FORM,
		 XmNrightAttachment, XmATTACH_WIDGET,
		 XmNrightWidget, greyscale_toggle,
		 0);
  XtVaSetValues (greyscale_toggle,
		 XmNtopAttachment, XmATTACH_WIDGET,
		 XmNtopWidget, portrait_toggle,
		 XmNbottomAttachment, XmATTACH_NONE,
		 XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
		 XmNleftWidget, portrait_toggle,
		 XmNrightAttachment, XmATTACH_NONE,
		 0);
  XtVaSetValues (color_toggle,
		 XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
		 XmNtopWidget, greyscale_toggle,
		 XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
		 XmNbottomWidget, greyscale_toggle,
		 XmNleftAttachment, XmATTACH_WIDGET,
		 XmNleftWidget, greyscale_toggle,
		 XmNrightAttachment, XmATTACH_NONE,
		 0);

  XtVaSetValues (paper_size_label,
		 XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
		 XmNtopWidget, letter_toggle,
		 XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
		 XmNbottomWidget, letter_toggle,
		 XmNleftAttachment, XmATTACH_FORM,
		 XmNrightAttachment, XmATTACH_WIDGET,
		 XmNrightWidget, letter_toggle,
		 0);
  XtVaSetValues (letter_toggle,
		 XmNtopAttachment, XmATTACH_WIDGET,
		 XmNtopWidget, greyscale_toggle,
		 XmNbottomAttachment, XmATTACH_NONE,
		 XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
		 XmNleftWidget, greyscale_toggle,
		 XmNrightAttachment, XmATTACH_NONE,
		 0);
  XtVaSetValues (legal_toggle,
		 XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
		 XmNtopWidget, letter_toggle,
		 XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
		 XmNbottomWidget, letter_toggle,
		 XmNleftAttachment, XmATTACH_WIDGET,
		 XmNleftWidget, letter_toggle,
		 XmNrightAttachment, XmATTACH_NONE,
		 0);
  XtVaSetValues (executive_toggle,
		 XmNtopAttachment, XmATTACH_WIDGET,
		 XmNtopWidget, letter_toggle,
		 XmNbottomAttachment, XmATTACH_NONE,
		 XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
		 XmNleftWidget, letter_toggle,
		 XmNrightAttachment, XmATTACH_NONE,
		 0);
  XtVaSetValues (a4_toggle,
		 XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
		 XmNtopWidget, executive_toggle,
		 XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
		 XmNbottomWidget, executive_toggle,
		 XmNleftAttachment, XmATTACH_WIDGET,
		 XmNleftWidget, executive_toggle,
		 XmNrightAttachment, XmATTACH_NONE,
		 0);

  XtAddCallback (to_printer_toggle, XmNvalueChangedCallback,
		 fe_print_to_toggle_cb, fpd);
  XtAddCallback (to_file_toggle, XmNvalueChangedCallback,
		 fe_print_to_toggle_cb, fpd);

  XtAddCallback (browse_button, XmNactivateCallback,
		 fe_print_browse_cb, fpd);

  XtAddCallback (first_first_toggle, XmNvalueChangedCallback,
		 fe_print_order_toggle_cb, fpd);
  XtAddCallback (last_first_toggle, XmNvalueChangedCallback,
		 fe_print_order_toggle_cb, fpd);

  XtAddCallback (portrait_toggle, XmNvalueChangedCallback,
		 fe_print_orientation_toggle_cb, fpd);
  XtAddCallback (landscape_toggle, XmNvalueChangedCallback,
		 fe_print_orientation_toggle_cb, fpd);

  XtAddCallback (greyscale_toggle, XmNvalueChangedCallback,
		 fe_print_color_toggle_cb, fpd);
  XtAddCallback (color_toggle, XmNvalueChangedCallback,
		 fe_print_color_toggle_cb, fpd);

  XtAddCallback (letter_toggle, XmNvalueChangedCallback,
		 fe_print_paper_toggle_cb, fpd);
  XtAddCallback (legal_toggle, XmNvalueChangedCallback,
		 fe_print_paper_toggle_cb, fpd);
  XtAddCallback (executive_toggle, XmNvalueChangedCallback,
		 fe_print_paper_toggle_cb, fpd);
  XtAddCallback (a4_toggle, XmNvalueChangedCallback,
		 fe_print_paper_toggle_cb, fpd);

  XtVaSetValues (print_command_text, XmNvalue, fe_globalPrefs.print_command,0);
  if (!last_print_file_name || !*last_print_file_name) {
	/* Use a default file name. We need to strdup here as we free this
	   later. */
	last_print_file_name = strdup( XFE_DEFAULT_PRINT_FILENAME );
  }
  fe_SetTextField(file_name_text, last_print_file_name);
  if (last_to_file_p)
    {
      XtVaSetValues (to_file_toggle, XmNset, True, 0);
      XtVaSetValues (print_command_text, XmNsensitive, False, 0);
      XtVaSetValues (shell, XmNinitialFocus, file_name_text, 0);
    }
  else
    {
      XtVaSetValues (to_printer_toggle, XmNset, True, 0);
      XtVaSetValues (file_name_text, XmNsensitive, False, 0);
      XtVaSetValues (browse_button, XmNsensitive, False, 0);
      XtVaSetValues (shell, XmNinitialFocus, print_command_text, 0);
    }
  XtVaSetValues ((fe_globalPrefs.print_reversed
		  ? last_first_toggle : first_first_toggle),
		 XmNset, True, 0);
  XtVaSetValues ((fe_globalPrefs.print_landscape
		  ? landscape_toggle : portrait_toggle),
		 XmNset, True, 0);
  XtVaSetValues ((fe_globalPrefs.print_color
		  ? color_toggle : greyscale_toggle),
		 XmNset, True, 0);
  XtVaSetValues ((fe_globalPrefs.print_paper_size == 0 ? letter_toggle :
		  fe_globalPrefs.print_paper_size == 1 ? legal_toggle :
		  fe_globalPrefs.print_paper_size == 2 ? executive_toggle :
		  a4_toggle),
		 XmNset, True, 0);


  XtManageChildren (kids, i);
  XtManageChild (form);

  fpd->context = context;
  fpd->hist = SHIST_GetCurrent (&context->hist);
  fpd->shell = shell;
  fpd->printer = to_printer_toggle;
  fpd->file = to_file_toggle;
  fpd->command_text = print_command_text;
  fpd->file_text = file_name_text;
  fpd->browse = browse_button;
  fpd->first = first_first_toggle;
  fpd->last = last_first_toggle;
  fpd->portrait = portrait_toggle;
  fpd->landscape = landscape_toggle;
  fpd->grey = greyscale_toggle;
  fpd->color = color_toggle;
  fpd->letter = letter_toggle;
  fpd->legal = legal_toggle;
  fpd->exec = executive_toggle;
  fpd->a4 = a4_toggle;

  fe_NukeBackingStore (shell);
  XtManageChild (shell);
}



/* Find dialog. */

static void
fe_find_refresh_data(fe_FindData *find_data)
{
  MWContext *focus_context;
  INTL_CharSetInfo c;

  if (!find_data)
    return;

  /* Decide which context to search in */
  focus_context = fe_findcommand_context();
  if (!focus_context)
    focus_context = fe_GetFocusGridOfContext(find_data->context);
  
  c = LO_GetDocumentCharacterSetInfo(find_data->context);
  if (focus_context) {
    if (focus_context != find_data->context_to_find)
      fe_FindReset(find_data->context);
  }
  find_data->context_to_find = focus_context;

  if (find_data->shell) {
    unsigned char *loc;
    char * tmp;

    if (find_data->string)
      XP_FREE (find_data->string);
    find_data->string = 0;
    XtVaGetValues (find_data->text, XmNvalue, &loc, 0);
    tmp = (char *) fe_ConvertFromLocaleEncoding (INTL_GetCSIWinCSID(c), loc);
    if (tmp) {
      int length;
      /*
       * We don't use XP_STRDUP here because it maps to strdup
       * which implies using free() when we're through.  We want
       * to allocate a string which is freed by XP_FREE()
       */
      length = XP_STRLEN(tmp);
      find_data->string = (char *)XP_ALLOC(length+1);
      XP_STRCPY(find_data->string, tmp);
    } else {
      find_data->string = 0;
    }
    if (find_data->string != (char *) loc) {
      XtFree (loc);
    }
    XtVaGetValues (find_data->case_sensitive, XmNset,
		   &find_data->case_sensitive_p, 0);
    XtVaGetValues (find_data->backward,
		   XmNset, &find_data->backward_p, 0);
    /* For mail/news contexts, the Search in Header/Body is loaded into the
       fe_FindData in the valueChangeCallback */
  }
}

#define FE_FIND_FOUND			0
#define FE_FIND_NOTFOUND		1
#define FE_FIND_CANCEL			2
#define FE_FIND_HEADER_FOUND		3
#define FE_FIND_HEADER_NOTFOUND		4
#define FE_FIND_NOSTRING		5

static int
fe_find (fe_FindData *find_data)
{
  Widget mainw;
  MWContext *context_to_find;
  XP_Bool hasRetried = FALSE;
  CL_Layer *layer;

  XP_ASSERT(find_data);

  /* Find a shell to use with all subsequent dialogs */
  if (find_data->shell && XtIsManaged(find_data->shell))
    mainw = find_data->shell;
  else
    mainw = CONTEXT_WIDGET(find_data->context);
  while(!XtIsWMShell(mainw) && (XtParent(mainw)!=0))
    mainw = XtParent(mainw);

  /* reload search parameters */
  fe_find_refresh_data(find_data);

  context_to_find = find_data->context;
  if (find_data->context_to_find)
    context_to_find = find_data->context_to_find;

  if (!find_data->string || !find_data->string[0]) {
    fe_Alert_2(mainw, fe_globalData.no_search_string_message);
    return (FE_FIND_NOSTRING);
  }

  if (find_data->find_in_headers) {
    XP_ASSERT(find_data->context->type == MWContextMail ||
	      find_data->context->type == MWContextNews);
    if (find_data->context->type == MWContextMail ||
	find_data->context->type == MWContextNews) {
      int status = -1;		/* ###tw  Find needs to be hooked up in a brand
				   new way now### */
/*###tw      int status = MSG_DoFind(fj->context, fj->string, fj->case_sensitive_p); */
      if (status < 0) {
	/* mainw could be the find_data->shell. If status < 0 (find failed)
	 * backend will bring the find window down. So use the context to
	 * display the error message here.
	 */
	FE_Alert(find_data->context, XP_GetString(status));
	return(FE_FIND_HEADER_NOTFOUND);
      }
      return (FE_FIND_HEADER_FOUND);
    }
  }

#ifdef EDITOR
  /* but I think you will want this in non-Gold too! */
  /*
   *    Start the search from the current selection location. Bug #29854.
   */
  LO_GetSelectionEndpoints(context_to_find,
			   &find_data->start_element,
			   &find_data->end_element,
			   &find_data->start_pos,
			   &find_data->end_pos,
                           &layer);
#endif
  AGAIN:

  if (LO_FindText (context_to_find, find_data->string,
		   &find_data->start_element, &find_data->start_pos, 
		   &find_data->end_element, &find_data->end_pos,
		   find_data->case_sensitive_p, !find_data->backward_p))
    {
      int32 x, y;
      LO_SelectText (context_to_find,
		     find_data->start_element, find_data->start_pos,
		     find_data->end_element, find_data->end_pos,
		     &x, &y);

      /* If the found item is not visible on the screen, scroll to it.
	 If we need to scroll, attempt to position the destination
	 coordinate in the middle of the window.
	 */
      if (x >= CONTEXT_DATA (context_to_find)->document_x &&
	  x <= (CONTEXT_DATA (context_to_find)->document_x +
		CONTEXT_DATA (context_to_find)->scrolled_width))
	x = CONTEXT_DATA (context_to_find)->document_x;
      else
	x = x - (CONTEXT_DATA (context_to_find)->scrolled_width / 2);

      if (y >= CONTEXT_DATA (context_to_find)->document_y &&
	  y <= (CONTEXT_DATA (context_to_find)->document_y +
		CONTEXT_DATA (context_to_find)->scrolled_height))
	y = CONTEXT_DATA (context_to_find)->document_y;
      else
	y = y - (CONTEXT_DATA (context_to_find)->scrolled_height / 2);

      if (x + CONTEXT_DATA (context_to_find)->scrolled_width
	  > CONTEXT_DATA (context_to_find)->document_width)
	x = (CONTEXT_DATA (context_to_find)->document_width -
	     CONTEXT_DATA (context_to_find)->scrolled_width);

      if (y + CONTEXT_DATA (context_to_find)->scrolled_height
	  > CONTEXT_DATA (context_to_find)->document_height)
	y = (CONTEXT_DATA (context_to_find)->document_height -
	     CONTEXT_DATA (context_to_find)->scrolled_height);

      if (x < 0) x = 0;
      if (y < 0) y = 0;

      fe_ScrollTo (context_to_find, x, y);
    }
  else
    {
      if (hasRetried) {
	fe_Alert_2 (mainw, fe_globalData.wrap_search_not_found_message);
	return(FE_FIND_NOTFOUND);
      } else {
	if (fe_Confirm_2 (mainw,
			  (find_data->backward_p
			   ? fe_globalData.wrap_search_backward_message
			   : fe_globalData.wrap_search_message)))	{
	  find_data->start_element = 0;
	  find_data->start_pos = 0;
	  hasRetried = TRUE;
	  goto AGAIN;
	}
	else
	  return (FE_FIND_CANCEL);
      }
    }
  return(FE_FIND_FOUND);
}

static void
fe_find_destroy_cb (Widget widget, XtPointer closure, XtPointer call_data)
{
  MWContext *context = (MWContext *) closure;
  fe_FindData *find_data = CONTEXT_DATA(context)->find_data;
  if (find_data) {
    /* Before we destroy, load all the search parameters */
    fe_find_refresh_data(find_data);
    find_data->shell = 0;
    XP_FREE(find_data);
    find_data = 0;
  }
  /* find_data will be freed when the context is deleted */
}

static void
fe_find_cb(Widget widget, XtPointer closure, XtPointer call_data)
{
  XmAnyCallbackStruct *cb = (XmAnyCallbackStruct *) call_data;
  MWContext *context = (MWContext *) closure;
  fe_FindData *find_data = CONTEXT_DATA(context)->find_data;
  int result;

  XP_ASSERT(find_data && find_data->shell);

  switch (cb->reason) {
  case XmCR_OK:		/* ok */
    result = fe_find(find_data);
    if (result == FE_FIND_HEADER_FOUND ||
	result == FE_FIND_HEADER_NOTFOUND)
      XtUnmanageChild(find_data->shell);
    break;
  case XmCR_APPLY:	/* clear */
    XtVaSetValues (find_data->text, XmNcursorPosition, 0, 0);
    fe_SetTextField (find_data->text, "");
    XmProcessTraversal (find_data->text, XmTRAVERSE_CURRENT);
    break;
  case XmCR_CANCEL:	/* cancel */
  default:
    XtUnmanageChild(find_data->shell);
  }
}

static void
fe_find_head_or_body_changed(Widget widget, XtPointer closure,
			     XtPointer call_data)
{
  fe_FindData *find_data = (fe_FindData *) closure;
  XP_ASSERT(find_data);
  if (widget == find_data->msg_body) {
    find_data->find_in_headers = FALSE;
  } else {
    XP_ASSERT(widget == find_data->msg_head);
    find_data->find_in_headers = TRUE;
  }
  XmToggleButtonGadgetSetState(find_data->msg_body, !find_data->find_in_headers, FALSE);
  XmToggleButtonGadgetSetState(find_data->msg_head, find_data->find_in_headers, FALSE);
  XtVaSetValues(find_data->backward, XmNsensitive, !find_data->find_in_headers, 0);
}
  

void
fe_FindDialog (MWContext *context, Boolean again)
{
  Widget mainw = CONTEXT_WIDGET (context);
  Widget kids [50];
  Arg av [20];
  int ac;
  Widget shell, form, find_label, find_text;
  Widget findin = 0;
  Widget msg_head = 0;
  Widget msg_body = 0;
  Widget case_sensitive_toggle, backwards_toggle;
  Visual *v = 0;
  Colormap cmap = 0;
  Cardinal depth = 0;
  fe_FindData *find_data = CONTEXT_DATA(context)->find_data;
  char *loc;
  INTL_CharSetInfo c = LO_GetDocumentCharacterSetInfo(context);

  /* Force Find if this was the first time */
  if (!find_data && again)
      again = False;

  if (again) {
    if (find_data->find_in_headers) {
      XP_ASSERT(find_data->context->type == MWContextMail ||
		find_data->context->type == MWContextNews);
      if (find_data->context->type == MWContextMail ||
	  find_data->context->type == MWContextNews) {
#ifdef NOTDEF			/* ###tw */
	  MSG_Command(CONTEXT_DATA(last_find_junk->context)->messagepane,
		      MSG_FindAgain);
#endif /* NOTDEF */
	return;
      }
    }
    fe_find (find_data);
    return;
  }
  else if (find_data && find_data->shell) {
    /* The find dialog is already there. Use it. */
    XtManageChild(find_data->shell);
    XMapRaised(XtDisplay(find_data->shell),
	       XtWindow(find_data->shell));
    return;
  }

  /* Create the find dialog */
  if (!find_data) {
    find_data = (fe_FindData *) XP_NEW_ZAP (fe_FindData);
    CONTEXT_DATA(context)->find_data = find_data;
    find_data->case_sensitive_p = False;
    if (context->type == MWContextMail && context->type == MWContextNews)
      find_data->find_in_headers = True;
    else
      find_data->find_in_headers = False;
  }

  fe_getVisualOfContext(context, &v, &cmap, &depth);

  ac = 0;
  XtSetArg (av[ac], XmNvisual, v); ac++;
  XtSetArg (av[ac], XmNdepth, depth); ac++;
  XtSetArg (av[ac], XmNcolormap, cmap); ac++;
  XtSetArg (av[ac], XmNallowShellResize, TRUE); ac++;
  XtSetArg (av[ac], XmNdialogStyle, XmDIALOG_MODELESS); ac++;
  XtSetArg (av[ac], XmNdialogType, XmDIALOG_QUESTION); ac++;
  XtSetArg (av[ac], XmNdeleteResponse, XmUNMAP); ac++;
  XtSetArg (av[ac], XmNautoUnmanage, False); ac++;
  XtSetArg (av[ac], XmNuserData, find_data); ac++;	/* this must go */
  shell = XmCreatePromptDialog (mainw, "findDialog", av, ac);

  fe_UnmanageChild_safe (XmSelectionBoxGetChild (shell,
						XmDIALOG_SELECTION_LABEL));
  fe_UnmanageChild_safe (XmSelectionBoxGetChild (shell, XmDIALOG_TEXT));
  XtManageChild (XmSelectionBoxGetChild (shell, XmDIALOG_APPLY_BUTTON));
#ifdef NO_HELP
  fe_UnmanageChild_safe (XmSelectionBoxGetChild (shell, XmDIALOG_HELP_BUTTON));
#endif

  ac = 0;
  form = XmCreateForm (shell, "form", av, ac);

#if 0
  /* find in headers has been replaced byy ldap search
   */
  if (context->type == MWContextMail || context->type == MWContextNews) {
    ac = 0;
    findin = XmCreateLabelGadget(form, "findInLabel", av, ac);
    ac = 0;
    msg_head = XmCreateToggleButtonGadget(form, "msgHeaders", av, ac);
    ac = 0;
    msg_body = XmCreateToggleButtonGadget(form, "msgBody", av, ac);
    XtAddCallback(msg_body, XmNvalueChangedCallback,
		  fe_find_head_or_body_changed, find_data);
    XtAddCallback(msg_head, XmNvalueChangedCallback,
		  fe_find_head_or_body_changed, find_data);
  }
#endif

  ac = 0;
  find_label = XmCreateLabelGadget (form, "findLabel", av, ac);
  ac = 0;
  loc = (char *) fe_ConvertToLocaleEncoding (INTL_GetCSIWinCSID(c),
				    (unsigned char *) find_data->string);
  XtSetArg (av [ac], XmNvalue, loc ? loc : ""); ac++;
  find_text = fe_CreateTextField (form, "findText", av, ac);
  if (loc && (loc != find_data->string)) {
    free (loc);
  }
  ac = 0;
  XtSetArg (av [ac], XmNset, find_data->case_sensitive_p); ac++;
  case_sensitive_toggle = XmCreateToggleButtonGadget (form, "caseSensitive",
						      av, ac);
  ac = 0;
  XtSetArg (av [ac], XmNset, find_data->backward_p); ac++;
  backwards_toggle = XmCreateToggleButtonGadget (form, "backwards", av, ac);

  XtAddCallback (shell, XmNdestroyCallback, fe_find_destroy_cb, context);
  XtAddCallback (shell, XmNokCallback, fe_find_cb, context);
  XtAddCallback (shell, XmNcancelCallback, fe_find_cb, context);
  XtAddCallback (shell, XmNapplyCallback, fe_find_cb, context);

  find_data->msg_head = msg_head;
  find_data->msg_body = msg_body;
  find_data->context = context;
  find_data->context_to_find = fe_findcommand_context();
  if (!find_data->context_to_find)
    find_data->context_to_find = fe_GetFocusGridOfContext(context);
  find_data->shell = shell;
  find_data->text = find_text;
  find_data->case_sensitive = case_sensitive_toggle;
  find_data->backward = backwards_toggle;

  if (findin) {
    XtVaSetValues(findin,
		  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
		  XmNtopWidget, msg_head,
		  XmNleftAttachment, XmATTACH_FORM,
		  XmNrightAttachment, XmATTACH_WIDGET,
		  XmNrightWidget, msg_head,
		  XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
		  XmNbottomWidget, msg_head,
		  0);
    XtVaSetValues(msg_head,
		  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
		  XmNtopWidget, msg_body,
		  XmNrightAttachment, XmATTACH_WIDGET,
		  XmNrightWidget, msg_body,
		  XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
		  XmNbottomWidget, msg_body,
		  0);
    XtVaSetValues(msg_body,
		  XmNtopAttachment, XmATTACH_FORM,
		  XmNrightAttachment, XmATTACH_FORM,
		  XmNbottomAttachment, XmATTACH_NONE,
		  0);
  }

  XtVaSetValues (find_label,
		 XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
		 XmNtopWidget, find_text,
		 XmNleftAttachment, XmATTACH_FORM,
		 XmNrightAttachment, XmATTACH_WIDGET,
		 XmNrightWidget, find_text,
		 XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
		 XmNbottomWidget, find_text,
		 0);
  XtVaSetValues (find_text,
		 XmNtopAttachment, msg_body ? XmATTACH_WIDGET : XmATTACH_FORM,
		 XmNtopWidget, msg_body,
		 XmNleftAttachment, XmATTACH_POSITION,
		 XmNleftPosition, 10,
		 XmNrightAttachment, XmATTACH_FORM,
		 XmNbottomAttachment, XmATTACH_NONE,
		 0);
  XtVaSetValues (case_sensitive_toggle,
		 XmNtopAttachment, XmATTACH_WIDGET,
		 XmNtopWidget, find_text,
		 XmNbottomAttachment, XmATTACH_NONE,
		 XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
		 XmNleftWidget, find_text,
		 XmNrightAttachment, XmATTACH_NONE,
		 0);
  XtVaSetValues (backwards_toggle,
		 XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
		 XmNtopWidget, case_sensitive_toggle,
		 XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
		 XmNbottomWidget, case_sensitive_toggle,
		 XmNleftAttachment, XmATTACH_WIDGET,
		 XmNleftWidget, case_sensitive_toggle,
		 XmNrightAttachment, XmATTACH_NONE,
		 0);
  ac = 0;
  if (findin) {
    fe_find_head_or_body_changed(find_data->find_in_headers ? msg_head : msg_body,
				 (XtPointer)find_data, (XtPointer)0);
    kids[ac++] = findin;
    kids[ac++] = msg_head;
    kids[ac++] = msg_body;
  }
  kids[ac++] = find_label;
  kids[ac++] = find_text;
  kids[ac++] = case_sensitive_toggle;
  kids[ac++] = backwards_toggle;
  XtManageChildren (kids, ac);
  ac = 0;
  XtManageChild (form);
  XtVaSetValues (form, XmNinitialFocus, find_text, 0);

  fe_NukeBackingStore (shell);
  XtManageChild (shell);
}

/* When a new layout begins, the find data is invalid.
 */
void
fe_FindReset (MWContext *context)
{
  fe_FindData *find_data;
  MWContext *top_context = XP_GetNonGridContext(context);

  find_data = CONTEXT_DATA(top_context)->find_data;
  
  if (!find_data) return;

  if (find_data->context_to_find == context ||
      find_data->context == context) {
    find_data->start_element = 0;
    find_data->end_element = 0;
    find_data->start_pos = 0;
    find_data->end_pos = 0;
  }
}


/* Streaming audio
 */

int
XFE_AskStreamQuestion (MWContext *context)
{
  struct fe_confirm_data data;
  Widget mainw = CONTEXT_WIDGET (context);
  Widget dialog;
  Arg av [20];
  int ac;
  Visual *v = 0;
  Colormap cmap = 0;
  Cardinal depth = 0;
  XtVaGetValues (mainw, XtNvisual, &v, XtNcolormap, &cmap,
		 XtNdepth, &depth, 0);
  ac = 0;
  XtSetArg (av[ac], XmNvisual, v); ac++;
  XtSetArg (av[ac], XmNdepth, depth); ac++;
  XtSetArg (av[ac], XmNcolormap, cmap); ac++;
  XtSetArg (av[ac], XmNallowShellResize, TRUE); ac++;
  XtSetArg (av[ac], XmNtransientFor, mainw); ac++;
  XtSetArg (av[ac], XmNdialogStyle, XmDIALOG_FULL_APPLICATION_MODAL); ac++;
  XtSetArg (av[ac], XmNdialogType, XmDIALOG_QUESTION); ac++;
  XtSetArg (av[ac], XmNdeleteResponse, XmDESTROY); ac++;
  XtSetArg (av[ac], XmNautoUnmanage, False); ac++;
  dialog = XmCreatePromptDialog (mainw, "streamingAudioQuery", av, ac);

  fe_UnmanageChild_safe (XmSelectionBoxGetChild (dialog, XmDIALOG_SEPARATOR));
  fe_UnmanageChild_safe (XmSelectionBoxGetChild (dialog, XmDIALOG_TEXT));
  XtManageChild (XmSelectionBoxGetChild (dialog, XmDIALOG_SELECTION_LABEL));
  XtManageChild (XmSelectionBoxGetChild (dialog, XmDIALOG_APPLY_BUTTON));
#ifdef NO_HELP
  fe_UnmanageChild_safe (XmSelectionBoxGetChild (dialog, XmDIALOG_HELP_BUTTON));
#endif

  fe_NukeBackingStore (dialog);
  XtManageChild (dialog);

  XtAddCallback (dialog, XmNokCallback, fe_destroy_ok_cb, &data);
  XtAddCallback (dialog, XmNapplyCallback, fe_destroy_apply_cb, &data);
  XtAddCallback (dialog, XmNcancelCallback, fe_destroy_cancel_cb, &data);
  XtAddCallback (dialog, XmNdestroyCallback, fe_destroy_finish_cb, &data);

  data.context = context;
  data.widget = dialog;
  data.answer = Answer_Invalid;

  while (data.answer == Answer_Invalid)
    fe_EventLoop ();

  if (data.answer == Answer_Cancel || data.answer == Answer_Destroy)
    return -1;
  else if (data.answer == Answer_OK)
    return 1;
  else if (data.answer == Answer_Apply)
    return 0;
  else
    abort ();
}


/* Care and feeding of lawyers and other parasites */

static XtErrorHandler old_xt_warning_handler = NULL;

static void 
xt_warning_handler(String msg)
{
	if (!msg)
	{
		return;
	}

#ifndef DEBUG
	/* Ignore warnings that contain "Actions not found" (non debug only) */
	if (XP_STRSTR(msg,"Actions not found"))
	{
		return;
	}
#endif
	
	(*old_xt_warning_handler)(msg);
}

void
fe_LicenseDialog (MWContext *context)
{
  Widget mainw = CONTEXT_WIDGET (context);
  Widget dialog;
  Arg av [20];
  int ac;
  Visual *v = 0;
  Colormap cmap = 0;
  Cardinal depth = 0;
  Widget form, label1, text, label2;
  Widget accept, reject;

  char crockery [1024];
  PR_snprintf (crockery, sizeof (crockery), "%d ", getuid ());
  strcat (crockery, fe_version);

  if (fe_VendorAnim)
    return;

  if (fe_globalPrefs.license_accepted &&
      !strcmp (crockery, fe_globalPrefs.license_accepted))
    return;

  XtVaGetValues (mainw, XtNvisual, &v, XtNcolormap, &cmap,
		 XtNdepth, &depth, 0);
  ac = 0;
  XtSetArg (av[ac], XmNvisual, v); ac++;
  XtSetArg (av[ac], XmNdepth, depth); ac++;
  XtSetArg (av[ac], XmNcolormap, cmap); ac++;
  XtSetArg (av[ac], XmNallowShellResize, TRUE); ac++;
  XtSetArg (av[ac], XmNtransientFor, mainw); ac++;
  XtSetArg (av[ac], XmNdialogStyle, XmDIALOG_FULL_APPLICATION_MODAL); ac++;
  XtSetArg (av[ac], XmNdialogType, XmDIALOG_QUESTION); ac++;
  XtSetArg (av[ac], XmNdeleteResponse, XmDESTROY); ac++;
  XtSetArg (av[ac], XmNautoUnmanage, False); ac++;
  XtSetArg (av[ac], XmNdefaultButton, 0); ac++;
  dialog = XmCreateTemplateDialog (mainw, "licenseDialog", av, ac);

  fe_UnmanageChild_safe (XmMessageBoxGetChild (dialog, XmDIALOG_OK_BUTTON));
  fe_UnmanageChild_safe (XmMessageBoxGetChild (dialog, XmDIALOG_CANCEL_BUTTON));
/*  fe_UnmanageChild_safe (XmMessageBoxGetChild (dialog, XmDIALOG_APPLY_BUTTON));*/
  fe_UnmanageChild_safe (XmMessageBoxGetChild (dialog,XmDIALOG_DEFAULT_BUTTON));
  fe_UnmanageChild_safe (XmMessageBoxGetChild (dialog, XmDIALOG_HELP_BUTTON));

  ac = 0;
  accept = XmCreatePushButtonGadget (dialog, "accept", av, ac);
  reject = XmCreatePushButtonGadget (dialog, "reject", av, ac);
  form = XmCreateForm (dialog, "form", av, ac);
  label1 = XmCreateLabelGadget (form, "label1", av, ac);

  XtSetArg (av [ac], XmNeditable, False); ac++;
  XtSetArg (av [ac], XmNcursorPositionVisible, False); ac++;
  XtSetArg (av[ac], XmNeditMode, XmMULTI_LINE_EDIT); ac++;
  text = XmCreateScrolledText (form, "text", av, ac);
  fe_HackDialogTranslations (text);
  ac = 0;
  label2 = XmCreateLabelGadget (form, "label2", av, ac);

  XtVaSetValues (label1,
		 XmNtopAttachment, XmATTACH_FORM,
		 XmNbottomAttachment, XmATTACH_NONE,
		 XmNleftAttachment, XmATTACH_FORM,
		 XmNrightAttachment, XmATTACH_FORM,
		 0);
  XtVaSetValues (XtParent (text),
		 XmNtopAttachment, XmATTACH_WIDGET,
		 XmNtopWidget, label1,
		 XmNbottomAttachment, XmATTACH_WIDGET,
		 XmNbottomWidget, label2,
		 XmNleftAttachment, XmATTACH_FORM,
		 XmNrightAttachment, XmATTACH_FORM,
		 0);
  XtVaSetValues (label2,
		 XmNtopAttachment, XmATTACH_NONE,
		 XmNbottomAttachment, XmATTACH_FORM,
		 XmNleftAttachment, XmATTACH_FORM,
		 XmNrightAttachment, XmATTACH_FORM,
		 0);

  {
    char *license = 0;
    int32 length = 0;
    char *ctype = 0;
    void *data = FE_AboutData ("license", &license, &length, &ctype);
    if (!license || length < 300)
      {
        license = "Please fill in";
        /* abort (); */
      }
    XtVaSetValues (text, XmNvalue, license, 0);
    if (data) free (data);
  }

  XtManageChild (label1);
  XtManageChild (text);
  XtManageChild (XtParent (text));
  XtManageChild (label2);
  XtManageChild (form);
  XtManageChild (accept);
  XtManageChild (reject);

  {
    struct fe_confirm_data data;
    XtVaSetValues (dialog, XmNdefaultButton, accept, 0);
    XtAddCallback (accept, XmNactivateCallback, fe_destroy_ok_cb, &data);
    XtAddCallback (reject, XmNactivateCallback, fe_destroy_cancel_cb, &data);
    XtAddCallback (dialog, XmNdestroyCallback, fe_destroy_finish_cb, &data);

    data.context = context;
    data.widget = dialog;
    data.answer = Answer_Invalid;

    fe_NukeBackingStore (dialog);

	/* Install a warning handler that ignores translation warnings */
	old_xt_warning_handler = XtAppSetWarningHandler(fe_XtAppContext,
													xt_warning_handler);

    XtManageChild (dialog);

	/* Restore the original warning handler after the dialog is managed */
	XtAppSetWarningHandler(fe_XtAppContext,old_xt_warning_handler);

    while (data.answer == Answer_Invalid)
      fe_EventLoop ();

    if (data.answer == Answer_Cancel || data.answer == Answer_Destroy)
      {
	if (fe_pidlock) unlink (fe_pidlock);
	exit (0);
      }
    else if (data.answer == Answer_OK)
      {
	/* Store crockery into preferences, and save. */
	StrAllocCopy (fe_globalPrefs.license_accepted, crockery);
	if (!XFE_UpgradePrefs ((char *) fe_globalData.user_prefs_file,
			    &fe_globalPrefs))
	  fe_perror (context, XP_GetString(XFE_CANT_SAVE_PREFS));
	return;
      }
    else
      {
	abort ();
      }
  }
}

void
fe_UpdateDocInfoDialog (MWContext *context)
{
	/* nada for now */
}

static void
fe_security_dialog_toggle (Widget widget, XtPointer closure,
			   XtPointer call_data)
{
  XP_Bool *prefs_toggle = (XP_Bool *) closure;
  XmToggleButtonCallbackStruct *cb = (XmToggleButtonCallbackStruct *)call_data;
  *prefs_toggle = cb->set;
  return;
}

static void
fe_movemail_dialog_toggle (Widget widget, XtPointer closure,
			   XtPointer call_data)
{
  XP_Bool *prefs_toggle = (XP_Bool *) closure;
  XmToggleButtonCallbackStruct *cb = (XmToggleButtonCallbackStruct *)call_data;
  *prefs_toggle = cb->set;
  if (!XFE_SavePrefs ((char *) fe_globalData.user_prefs_file, &fe_globalPrefs))
    fe_perror (fe_all_MWContexts->context,
               XP_GetString( XFE_ERROR_SAVING_OPTIONS ) );
}

#ifdef MOZ_MAIL_NEWS
/*
 * fe_MovemailWarning
 * Stolen from FE_SecurityDialog
 */
extern Boolean
fe_MovemailWarning(MWContext *context)
{
  if ( fe_globalPrefs.movemail_warn == True ) {
    Widget mainw = CONTEXT_WIDGET (context);
    Widget dialog;
    Widget form;
    Widget toggle;
    Arg av [20];
    int ac;
    Visual *v = 0;
    Colormap cmap = 0;
    Cardinal depth = 0;
    XmString ok_label;
    XmString cancel_label;
    XmString selection_label;
    XmString toggle_label;
    struct fe_confirm_data data;
    char buf[256];
    char* from;
    const char* to;
 
    from = fe_mn_getmailbox();
    to   = FE_GetFolderDirectory(context);
 
    /*
     * This dialog should probably be popped up in fe_mn_getmailbox()
     * instead.  Otherwise, we have to check everywhere it's used.
     * (Only two places at this point, though.)
     */
    if ( from == NULL ) {
        FE_Alert(context, XP_GetString(XFE_MAIL_SPOOL_UNKNOWN));
        return False;
    }

    PR_snprintf(buf, sizeof(buf), XP_GetString(XFE_MOVEMAIL_EXPLANATION),
                from, to, from);
 
    ok_label = XmStringCreateLtoR(XP_GetString(XFE_CONTINUE_MOVEMAIL),
                                  XmFONTLIST_DEFAULT_TAG);
    cancel_label = XmStringCreateLtoR(XP_GetString(XFE_CANCEL_MOVEMAIL),
                                      XmFONTLIST_DEFAULT_TAG);
    selection_label = XmStringCreateLtoR(buf,
                                         XmFONTLIST_DEFAULT_TAG);
    toggle_label = XmStringCreateLtoR(XP_GetString(XFE_SHOW_NEXT_TIME),
                                      XmFONTLIST_DEFAULT_TAG);
 
    XtVaGetValues (mainw, XtNvisual, &v, XtNcolormap, &cmap,
                   XtNdepth, &depth, 0);
    ac = 0;
    XtSetArg (av[ac], XmNvisual, v); ac++;
    XtSetArg (av[ac], XmNdepth, depth); ac++;
    XtSetArg (av[ac], XmNcolormap, cmap); ac++;
    XtSetArg (av[ac], XmNallowShellResize, TRUE); ac++;
    XtSetArg (av[ac], XmNtransientFor, mainw); ac++;
    XtSetArg (av[ac], XmNdialogStyle,
              XmDIALOG_PRIMARY_APPLICATION_MODAL); ac++;
    XtSetArg (av[ac], XmNdialogType, XmDIALOG_QUESTION); ac++;
    XtSetArg (av[ac], XmNdeleteResponse, XmDESTROY); ac++;
    XtSetArg (av[ac], XmNautoUnmanage, False); ac++;
    XtSetArg (av[ac], XmNchildPlacement, XmPLACE_BELOW_SELECTION); ac++;
    XtSetArg (av[ac], XmNokLabelString, ok_label); ac++;
    XtSetArg (av[ac], XmNcancelLabelString, cancel_label); ac++;
    XtSetArg (av[ac], XmNselectionLabelString, selection_label); ac++;
    dialog = XmCreatePromptDialog (mainw, "movemailWarnDialog", av, ac);
 
    if ( ok_label ) XmStringFree(ok_label);
    if ( cancel_label ) XmStringFree(cancel_label);
    if ( selection_label ) XmStringFree(selection_label);
 
    XtManageChild (XmSelectionBoxGetChild (dialog, XmDIALOG_SELECTION_LABEL));
    fe_UnmanageChild_safe (XmSelectionBoxGetChild (dialog, XmDIALOG_SEPARATOR));
    fe_UnmanageChild_safe (XmSelectionBoxGetChild (dialog, XmDIALOG_TEXT));
    fe_UnmanageChild_safe (XmSelectionBoxGetChild (dialog,
                                                  XmDIALOG_APPLY_BUTTON));
#ifdef NO_HELP
    fe_UnmanageChild_safe (XmSelectionBoxGetChild (dialog,
                                                  XmDIALOG_HELP_BUTTON));
#endif
 
    ac = 0;
    form = XmCreateForm (dialog, "form", av, ac);
    ac = 0;
    XtSetArg (av[ac], XmNset, fe_globalPrefs.movemail_warn); ac++;
    XtSetArg (av[ac], XmNtopAttachment, XmATTACH_FORM); ac++;
    XtSetArg (av[ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
    XtSetArg (av[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
    XtSetArg (av[ac], XmNrightAttachment, XmATTACH_NONE); ac++;
    XtSetArg (av[ac], XmNlabelString, toggle_label); ac++;
    toggle = XmCreateToggleButtonGadget (form, "toggle", av, ac);
    if ( toggle_label ) XmStringFree(toggle_label);
 
    XtAddCallback (toggle, XmNvalueChangedCallback,
               fe_movemail_dialog_toggle, &fe_globalPrefs.movemail_warn);
    XtManageChild (toggle);
    XtManageChild (form);
 
    fe_NukeBackingStore (dialog);
    XtManageChild (dialog);
 
    XtAddCallback (dialog, XmNokCallback, fe_destroy_ok_cb, &data);
    XtAddCallback (dialog, XmNapplyCallback, fe_destroy_apply_cb, &data);
    XtAddCallback (dialog, XmNcancelCallback, fe_destroy_cancel_cb, &data);
    XtAddCallback (dialog, XmNdestroyCallback, fe_destroy_finish_cb,&data);
 
    data.context = context;
    data.widget = dialog;
    data.answer = Answer_Invalid;
 
    while (data.answer == Answer_Invalid)
      fe_EventLoop ();

    if (data.answer == Answer_Cancel || data.answer == Answer_Destroy)
      return False;
    else if (data.answer == Answer_OK)
      return True;
    else
      abort ();
  }
 
  return True;
}
#endif  /* MOZ_MAIL_NEWS */


extern Bool
FE_SecurityDialog (MWContext *context, int state, XP_Bool *prefs_toggle)
{
  char *name;
  Bool cancel_p = True;
  Bool cancel_default_p = False;
  switch (state)
    {
    case SD_INSECURE_POST_FROM_SECURE_DOC:
      name = "insecurePostFromSecureDocDialog";
      cancel_p = True;
      cancel_default_p = True;
      break;
    case SD_INSECURE_POST_FROM_INSECURE_DOC:
      name = "insecurePostFromInsecureDocDialog";
      cancel_p = True;
      break;
    case SD_ENTERING_SECURE_SPACE:
      name = "enteringSecureDialog";
      cancel_p = False;
      break;
    case SD_LEAVING_SECURE_SPACE:
      name = "leavingSecureDialog";
      cancel_p = True;
      break;
    case SD_INSECURE_DOCS_WITHIN_SECURE_DOCS_NOT_SHOWN:
      name = "mixedSecurityDialog";
      cancel_p = False;
      break;
    case SD_REDIRECTION_TO_INSECURE_DOC:
      name = "redirectionToInsecureDialog";
      cancel_p = True;
      break;
    case SD_REDIRECTION_TO_SECURE_SITE:
      name = "redirectionToSecureDialog";
      cancel_p = True;
      break;
    default:
      abort ();
    }

  if (prefs_toggle && !*prefs_toggle)
    return True;

  {
    Widget mainw = CONTEXT_WIDGET (context);
    Widget dialog;
    Arg av [20];
    int ac;
    Visual *v = 0;
    Colormap cmap = 0;
    Cardinal depth = 0;
    XtVaGetValues (mainw, XtNvisual, &v, XtNcolormap, &cmap,
		   XtNdepth, &depth, 0);
    ac = 0;
    XtSetArg (av[ac], XmNvisual, v); ac++;
    XtSetArg (av[ac], XmNdepth, depth); ac++;
    XtSetArg (av[ac], XmNcolormap, cmap); ac++;
    XtSetArg (av[ac], XmNallowShellResize, TRUE); ac++;
    XtSetArg (av[ac], XmNtransientFor, mainw); ac++;
	XtSetArg (av[ac], XmNdialogStyle,
			  XmDIALOG_PRIMARY_APPLICATION_MODAL); ac++;
    if (cancel_p)
      {
		  XtSetArg (av[ac], XmNdialogType, XmDIALOG_QUESTION); ac++;
      }
    else
      {
		  XtSetArg (av[ac], XmNdialogType, XmDIALOG_INFORMATION); ac++;
      }
    XtSetArg (av[ac], XmNdeleteResponse, XmDESTROY); ac++;
    XtSetArg (av[ac], XmNautoUnmanage, False); ac++;
    XtSetArg (av[ac], XmNchildPlacement, XmPLACE_BELOW_SELECTION); ac++;
    dialog = XmCreatePromptDialog (mainw, name, av, ac);

    XtManageChild (XmSelectionBoxGetChild (dialog, XmDIALOG_SELECTION_LABEL));
    fe_UnmanageChild_safe (XmSelectionBoxGetChild (dialog, XmDIALOG_SEPARATOR));
    fe_UnmanageChild_safe (XmSelectionBoxGetChild (dialog, XmDIALOG_TEXT));
    fe_UnmanageChild_safe (XmSelectionBoxGetChild (dialog,
						  XmDIALOG_APPLY_BUTTON));
#ifdef NO_HELP
    fe_UnmanageChild_safe (XmSelectionBoxGetChild (dialog,
						  XmDIALOG_HELP_BUTTON));
#endif
    if (! cancel_p)
      fe_UnmanageChild_safe (XmSelectionBoxGetChild (dialog,
						    XmDIALOG_CANCEL_BUTTON));

    if (prefs_toggle)
      {
	Widget form, toggle;
	ac = 0;
	form = XmCreateForm (dialog, "form", av, ac);
	ac = 0;
	XtSetArg (av[ac], XmNset, *prefs_toggle); ac++;
	XtSetArg (av[ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av[ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av[ac], XmNrightAttachment, XmATTACH_NONE); ac++;
	toggle = XmCreateToggleButtonGadget (form, "toggle", av, ac);
	XtAddCallback (toggle, XmNvalueChangedCallback,
		       fe_security_dialog_toggle, prefs_toggle);
	XtManageChild (toggle);
	XtManageChild (form);
      }

    if (cancel_default_p)
      {
	Widget c = XmSelectionBoxGetChild (dialog, XmDIALOG_CANCEL_BUTTON);
	if (!c) abort ();
	XtVaSetValues (dialog, XmNdefaultButton, c, XmNinitialFocus, c, 0);
      }

    fe_NukeBackingStore (dialog);
    XtManageChild (dialog);

      {
	struct fe_confirm_data data;
	XtAddCallback (dialog, XmNokCallback, fe_destroy_ok_cb, &data);
	if (cancel_p)
	  {
	    XtAddCallback (dialog, XmNapplyCallback, fe_destroy_apply_cb, &data);
	    XtAddCallback (dialog, XmNcancelCallback, fe_destroy_cancel_cb, &data);
	    XtAddCallback (dialog, XmNdestroyCallback, fe_destroy_finish_cb,&data);
	  }
	
	data.context = context;
	data.widget = dialog;
	data.answer = Answer_Invalid;

	while (data.answer == Answer_Invalid)
	  fe_EventLoop ();

	if (data.answer == Answer_Cancel || data.answer == Answer_Destroy)
	  return False;
	else if (data.answer == Answer_OK)
	  return True;
	else
	  abort ();
      }
  }
}
