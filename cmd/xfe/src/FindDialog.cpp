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
   FindDialog.cpp -- Find (-and-Replace) dialog.
   Created: Akkana Peck <akkana@netscape.com>, 22-Jul-98.
 */


#include "mozilla.h"
#include "xfe.h"
#include "felocale.h"
#include "intl_csi.h"

#include "edt.h"

/* for XP_GetString() */
#include <xpgetstr.h>

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

    /* Update the find string */
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
      XtFree ((char*)loc);
    }

    /* Update the replace string, if any */
    if (find_data->replace_string)
      XP_FREE (find_data->replace_string);
    find_data->replace_string = 0;
    if (find_data->replace_text != 0)
    {
      XtVaGetValues (find_data->replace_text, XmNvalue, &loc, 0);
      tmp = (char *) fe_ConvertFromLocaleEncoding (INTL_GetCSIWinCSID(c), loc);
      if (tmp) {
        int length;
        length = XP_STRLEN(tmp);
        find_data->replace_string = (char *)XP_ALLOC(length+1);
        XP_STRCPY(find_data->replace_string, tmp);
      }
      if (find_data->replace_string != (char *) loc) {
        XtFree ((char*)loc);
      }
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
fe_find_destroy_cb (Widget , XtPointer closure, XtPointer)
{
  MWContext *context = (MWContext *) closure;
  fe_FindData *find_data = CONTEXT_DATA(context)->find_data;
  if (find_data) {
    /* Before we destroy, load all the search parameters */ /* Why?? ...Akk */
    fe_find_refresh_data(find_data);
    if (find_data->string)
        XP_FREE(find_data->string);
    if (find_data->replace_string)
        XP_FREE(find_data->replace_string);
    XP_FREE(find_data);
    CONTEXT_DATA(context)->find_data = 0;
  }
  /* find_data will be freed when the context is deleted */
}

static void
fe_find_cb(Widget, XtPointer closure, XtPointer call_data)
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
fe_find_head_or_body_changed(Widget widget, XtPointer closure, XtPointer)
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
  
static void
fe_replace_cb(Widget w, XtPointer closure, XtPointer )
{
  MWContext* ctxt = (MWContext *) closure;
  if (ctxt == 0)
      return;
  fe_FindData* findData = CONTEXT_DATA(ctxt)->find_data;
  if (findData == 0)
    return;

  /* reload search parameters */
  fe_find_refresh_data(findData);

  if (findData->string == 0 || findData->string[0] == '\0'
      || findData->replace_string == 0 || findData->replace_string[0] == '\0')
      return;

  /* set replace_all based on the widget name */
  if (!strncmp(XtName(w), "replaceAll", 10))
      findData->replace_all = TRUE;
  else
      findData->replace_all = FALSE;

/* This should be replaced by something that doesn't require the editor -cls */
#ifdef EDITOR
  EDT_ReplaceText(ctxt,
                  findData->replace_string,
                  findData->replace_all,
                  findData->string,
                  findData->case_sensitive_p,
                  findData->backward_p,
                  FALSE /*bDoWrap*/); 
#endif

  /* Continue to search for next string unless we replaced all 
  if( bReplaceAll && find_data && find_data->shell)
    XtUnmanageChild(find_data->shell);
   */
}


static void
findOrReplaceDialog (MWContext *context, Boolean again, Boolean replaceP)
{
  Widget mainw = CONTEXT_WIDGET (context);
  Widget kids [50];
  Arg av [20];
  int ac;
  Widget shell, form, find_label, find_text;
  Widget replace_text = 0;
  Widget replace_label = 0, replaceBtn = 0, replaceAllBtn = 0;
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
  //XtSetArg (av[ac], XmNuserData, find_data); ac++;	/* this must go */
  shell = XmCreatePromptDialog (mainw, "findDialog", av, ac);

  fe_UnmanageChild_safe (XmSelectionBoxGetChild (shell,
						XmDIALOG_SELECTION_LABEL));
  fe_UnmanageChild_safe (XmSelectionBoxGetChild (shell, XmDIALOG_TEXT));
  XtManageChild (XmSelectionBoxGetChild (shell, XmDIALOG_APPLY_BUTTON));
#ifdef NO_HELP
  fe_UnmanageChild_safe (XmSelectionBoxGetChild (shell, XmDIALOG_HELP_BUTTON));
#endif

  ac = 0;
  XtSetArg (av[ac], XmNfractionBase, 100); ac++;
  form = XmCreateForm (shell, "form", av, ac);

#if 0
  /* find in headers has been replaced byy ldap search
   */
  if (context->type == MWContextMail || context->type == MWContextNews) {
    ac = 0;
    findin = XmCreateLabelGadget(form, "findInLabel", av, ac);
    XtVaSetValues(findin,
		  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
		  XmNtopWidget, msg_head,
		  XmNleftAttachment, XmATTACH_FORM,
		  XmNrightAttachment, XmATTACH_WIDGET,
		  XmNrightWidget, msg_head,
		  XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
		  XmNbottomWidget, msg_head,
		  0);
    ac = 0;
    msg_head = XmCreateToggleButtonGadget(form, "msgHeaders", av, ac);
    XtVaSetValues(msg_head,
		  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
		  XmNtopWidget, msg_body,
		  XmNrightAttachment, XmATTACH_WIDGET,
		  XmNrightWidget, msg_body,
		  XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
		  XmNbottomWidget, msg_body,
		  0);
    ac = 0;
    msg_body = XmCreateToggleButtonGadget(form, "msgBody", av, ac);
    XtVaSetValues(msg_body,
		  XmNtopAttachment, XmATTACH_FORM,
		  XmNrightAttachment, XmATTACH_FORM,
		  XmNbottomAttachment, XmATTACH_NONE,
		  0);
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
  XtVaSetValues (find_text,
		 XmNtopAttachment, msg_body ? XmATTACH_WIDGET : XmATTACH_FORM,
		 XmNtopWidget, msg_body,
                 XmNleftAttachment, XmATTACH_POSITION,
                 XmNleftPosition, 25,
		 0);
  XtVaSetValues (find_label,
                 XmNrightAttachment, XmATTACH_WIDGET,
                 XmNrightWidget, find_text,
		 XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
		 XmNbottomWidget, find_text,
		 0);

  if (replaceP)
  {
    ac = 0;
    XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
    XtSetArg (av [ac], XmNleftAttachment, XmATTACH_POSITION); ac++;
    XtSetArg (av [ac], XmNleftPosition, 80); ac++;
    XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
    replaceBtn = XmCreatePushButtonGadget(form, "replaceBtn", av, ac);
    XtAddCallback (replaceBtn, XmNactivateCallback, fe_replace_cb, context);

    ac = 0;
    XtSetArg (av [ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
    XtSetArg (av [ac], XmNtopWidget, replaceBtn); ac++;
    XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
    XtSetArg (av [ac], XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET); ac++;
    XtSetArg (av [ac], XmNleftWidget, replaceBtn); ac++;
    replaceAllBtn = XmCreatePushButtonGadget(form, "replaceAllBtn", av, ac);
    XtAddCallback (replaceAllBtn, XmNactivateCallback,
                   fe_replace_cb, context);

    ac = 0;
    loc = (char *) fe_ConvertToLocaleEncoding (INTL_GetCSIWinCSID(c),
                               (unsigned char *) find_data->replace_string);
    XtSetArg (av [ac], XmNvalue, loc ? loc : ""); ac++;
    replace_text = fe_CreateTextField (form, "replaceText", av, ac);
    if (loc && (loc != find_data->replace_string)) {
      free (loc);
    }
    XtVaSetValues (replace_text,
                   XmNtopAttachment, XmATTACH_WIDGET,
                   XmNtopWidget, find_text,
                   XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
                   XmNleftWidget, find_text,
                   XmNrightAttachment, XmATTACH_WIDGET,
                   XmNrightWidget, replaceAllBtn,
                   //XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
                   //XmNbottomWidget, replaceAllBtn,
                   0);

    ac = 0;
    replace_label = XmCreateLabelGadget (form, "replaceLabel", av, ac);
    XtVaSetValues (replace_label,
                   XmNrightAttachment, XmATTACH_WIDGET,
                   XmNrightWidget, replace_text,
                   XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
                   XmNbottomWidget, replace_text,
                   0);

    XtVaSetValues (find_text,
                   XmNrightAttachment, XmATTACH_WIDGET,
                   XmNrightWidget, replaceAllBtn,
                   0);
  }
  else  // not replaceP
  {
      XtVaSetValues (find_text,
                     XmNrightAttachment, XmATTACH_FORM,
                     0);
  }

  ac = 0;
  XtSetArg (av [ac], XmNset, find_data->case_sensitive_p); ac++;
  case_sensitive_toggle = XmCreateToggleButtonGadget (form, "caseSensitive",
                                                      av, ac);
  XtVaSetValues (case_sensitive_toggle,
		 XmNtopAttachment, XmATTACH_WIDGET,
		 XmNtopWidget, (replaceP ? replace_text : find_text),
		 XmNbottomAttachment, XmATTACH_FORM,
		 XmNrightAttachment, XmATTACH_POSITION,
                 XmNrightPosition, 50,
		 0);

  ac = 0;
  XtSetArg (av [ac], XmNset, find_data->backward_p); ac++;
  backwards_toggle = XmCreateToggleButtonGadget (form, "backwards", av, ac);
  XtVaSetValues (backwards_toggle,
		 XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
		 XmNtopWidget, case_sensitive_toggle,
		 XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
		 XmNbottomWidget, case_sensitive_toggle,
		 XmNleftAttachment, XmATTACH_WIDGET,
		 XmNleftWidget, case_sensitive_toggle,
		 0);

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
  find_data->replace_text = replace_text;
  find_data->case_sensitive = case_sensitive_toggle;
  find_data->backward = backwards_toggle;

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
  if (replaceP)
  {
      kids[ac++] = replace_label;
      kids[ac++] = replace_text;
      kids[ac++] = replaceBtn;
      kids[ac++] = replaceAllBtn;
  }
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

/*
 * The externally visible routines.
 */
void
fe_FindDialog (MWContext *context, Boolean again)
{
  findOrReplaceDialog(context, again, FALSE);
}

void
fe_FindAndReplaceDialog (MWContext *context, Boolean again)
{
  findOrReplaceDialog(context, again, TRUE);
}

