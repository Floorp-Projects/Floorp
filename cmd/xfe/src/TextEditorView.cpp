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
/* 
   TextEditorView.h - anything that has a text editable area in it. 
   Crreated: Dora Hsu <dora@netscape.com>, Sept-30-96.
 */



#include "TextEditorView.h"
#include "SpellHandler.h"
#include <Xm/Text.h>
#include "libi18n.h"
#include "felocale.h"
#include "xfe.h" // for CONTEXT_DATA
#include "Xfe/Xfe.h"

#ifdef DEBUG_editor
#define XDEBUG(x) x
#else
#define XDEBUG(x)
#endif

extern "C" void fe_mail_text_modify_cb (Widget, XtPointer, XtPointer);
extern "C"  void fe_HackTranslations (MWContext *, Widget);

extern "C" XtPointer fe_GetFont(MWContext *context, int sizeNum, int fontmask);
extern "C"
unsigned char * fe_ConvertToLocaleEncoding(int16 charset, unsigned char *str);


MenuSpec XFE_TextEditorView::popupmenu_spec[] = {
  { xfeCmdCut,                  PUSHBUTTON },
  { xfeCmdCopy,                 PUSHBUTTON },
  { xfeCmdPaste,                PUSHBUTTON },
  { xfeCmdPasteAsQuoted,        PUSHBUTTON },
  MENU_SEPARATOR,
  { xfeCmdQuoteOriginalText,          PUSHBUTTON },
  {NULL}
};

const char *XFE_TextEditorView::textFocusIn = "XFE_TextEditorView::textFocusIn";

XFE_TextEditorView::XFE_TextEditorView(XFE_Component* toplevel_component, 
				       XFE_View *parent_view,
				       MSG_Pane *p,
				       MWContext *context)
  : XFE_MNView(toplevel_component, parent_view, context, p)
{
	m_popup = NULL;
}

void
XFE_TextEditorView::createWidgets(Widget parentW, XP_Bool wrap_p)
{

  int ac;
  Arg av[20];
  Widget textW;
  MWContext *context = getParent()->getContext();
  XmFontList fontList;
  Widget form;

  fontList = (XmFontList)fe_GetFont (context, 3, LO_FONT_FIXED);
  
  form = XmCreateForm(parentW, "plainTextForm", NULL, 0 );

  ac = 0;
  XtSetArg(av[ac], XmNtopOffset, 5); ac++;
  XtSetArg (av[ac], XmNeditMode, XmMULTI_LINE_EDIT); ac++;
  XtSetArg (av[ac], XmNfontList, fontList); ac++;
  XtSetArg (av[ac], XmNscrollHorizontal, !wrap_p); ac++;
  XtSetArg (av[ac], XmNwordWrap, wrap_p); ac++;
  XtSetArg (av[ac], XmNtopAttachment, XmATTACH_FORM); ac++;
  XtSetArg (av[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
  XtSetArg (av[ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
  XtSetArg (av[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
  textW = XmCreateScrolledText(parentW, "mailto_bodyText", av, ac);
  XtAddCallback (textW, XmNmodifyVerifyCallback,
                   fe_mail_text_modify_cb, getParent()->getContext());

  // NOTE... [ we need this for the ComposeView focus mechanism...
  //
  XtAddCallback(textW, XmNfocusCallback, textFocusCallback, this);

  // 
  setBaseWidget(form);
  XtManageChild(textW);

  // This is a hack...NEED here...
  // Otherwise, we cannot swap the text widget upon wrap on/off.
  CONTEXT_DATA(m_contextData)->mcBodyText = textW;

}

void
XFE_TextEditorView::textFocus()
{
	Widget textW = CONTEXT_DATA(m_contextData)->mcBodyText;

	getToplevel()->notifyInterested(XFE_TextEditorView::textFocusIn, textW);
}

void
XFE_TextEditorView::textFocusCallback(Widget, XtPointer clientData, XtPointer)
{
  XFE_TextEditorView *obj = (XFE_TextEditorView*)clientData;

  obj->textFocus();
}

void
XFE_TextEditorView::setFont(XmFontList fontList)
{
  int ac;
  Arg av[20];
  Widget textW = CONTEXT_DATA(m_contextData)->mcBodyText;

  ac = 0;
  XtSetArg (av[ac], XmNfontList, fontList); ac++;
  XtSetValues(textW, av, ac);
  if (fe_LocaleCharSetID & MULTIBYTE) {
    Dimension columns;
    XtVaGetValues(textW, XmNcolumns, &columns, NULL);
    XtVaSetValues(textW, XmNcolumns, (columns + 1) / 2, NULL);
  }
}

void
XFE_TextEditorView::setComposeWrapState(XP_Bool wrap_p)
{
  Boolean old_wrap, new_wrap, old_scroll, new_scroll;
  Widget text, parent, gramp;
  MWContext *context = getParent()->getContext();

  XP_ASSERT(context->type == MWContextMessageComposition);
  XP_ASSERT(CONTEXT_DATA(context)->mcBodyText);
 
  text = CONTEXT_DATA(context)->mcBodyText;
  if (!text) return;
 
  parent = XtParent(text);
  gramp = XtParent(parent);
 
  XtVaGetValues(text,
                XmNwordWrap,         &old_wrap,
                XmNscrollHorizontal, &old_scroll,
                0);
 
  new_wrap   = wrap_p;
  new_scroll = !wrap_p;
  CONTEXT_DATA (context)->compose_wrap_lines_p = wrap_p;
 
  if (new_wrap == old_wrap &&
      new_scroll == old_scroll)
    return;
 
  {
    String body = 0;
    XmTextPosition cursor = 0;
    unsigned char top = 0, bot = 0, left = 0, right = 0;
    Widget topw = 0, botw = 0, leftw = 0, rightw = 0;
    Arg av [20];
    int ac = 0;
    XmFontList font_list = 0;
 
    /* #### warning much of this is cloned from mozilla.c
       (fe_create_composition_widgets) */
 
    XtVaGetValues (text,
                   XmNcursorPosition, &cursor,
                   XmNfontList, &font_list,
                   0);
	body = fe_GetTextField(text);
    XtVaGetValues (parent,
                   XmNtopAttachment, &top,      XmNtopWidget, &topw,
                   XmNbottomAttachment, &bot,   XmNbottomWidget, &botw,
                   XmNleftAttachment, &left,    XmNleftWidget, &leftw,
                   XmNrightAttachment, &right,  XmNrightWidget, &rightw,
                   0);
 
    XtUnmanageChild(parent);
    XtDestroyWidget(parent);
    CONTEXT_DATA(context)->mcBodyText = text = parent = 0;
 
    XtSetArg (av[ac], XmNeditable, True); ac++;
    XtSetArg (av[ac], XmNcursorPositionVisible, True); ac++;
    XtSetArg (av[ac], XmNeditMode, XmMULTI_LINE_EDIT); ac++;
 
    XtSetArg (av[ac], XmNscrollVertical,   TRUE); ac++;
    XtSetArg (av[ac], XmNscrollHorizontal, new_scroll); ac++;
    XtSetArg (av[ac], XmNwordWrap,         new_wrap); ac++;
 
    XtSetArg (av[ac], XmNfontList, font_list); ac++;
 
    text = XmCreateScrolledText (gramp, "mailto_bodyText", av, ac);
    parent = XtParent(text);
    // This is really a hack since we changed the widget in the 
    // middle of displaying
    fe_HackTranslations(m_contextData, text);
 
    if (fe_LocaleCharSetID & MULTIBYTE) {
      Dimension columns;
      XtVaGetValues(text, XmNcolumns, &columns, NULL);
      XtVaSetValues(text, XmNcolumns, (columns + 1) / 2, NULL);
    }
 
 
    /* Warning -- this assumes no widgets are attached to this one
       (which is the normal state of affairs, since this is the widget
       that should grow.) */
    XtVaSetValues (parent,
                   XmNtopAttachment,    top,   XmNtopWidget,    topw,
                   XmNbottomAttachment, bot,   XmNbottomWidget, botw,
                   XmNleftAttachment,   left,  XmNleftWidget,   leftw,
                   XmNrightAttachment,  right, XmNrightWidget,  rightw,
                   0);
    XtVaSetValues (text,
                   XmNfontList, font_list,
                   XmNcursorPosition, cursor,
                   XmNwordWrap, new_wrap,
                   0);
	fe_SetTextField(text, (body ? body : ""));
    if (body) free(body);
 
    /* Put this callback back so that the "you haven't typed anything do
       you really want to send?" hack still works with this new text widget
       #### cloned from dialogs.c (FE_InitializeMailCompositionContext)
     */
    XtAddCallback (text, XmNmodifyVerifyCallback,
                   fe_mail_text_modify_cb, context);
 
	// NOTE... [ we need this for the ComposeView focus mechanism...
	//
	XtAddCallback(text, XmNfocusCallback, textFocusCallback, this);

    CONTEXT_DATA(context)->mcBodyText = text;
    XtManageChild(text);
 
    /* just in case it didn't take... */
    XtVaSetValues (text, XmNcursorPosition, cursor, 0);
 
    /* Move focus back there. */
    XmProcessTraversal (text, XmTRAVERSE_CURRENT);
  }
}

void
XFE_TextEditorView::doCommand(CommandType cmd, void */*calldata*/, XFE_CommandInfo* info)
{
XDEBUG( printf ("in XFE_TextEditorView::doCommand(%s)\n", Command::getString(cmd));)

 Widget destW = XmGetDestination(XtDisplay(getBaseWidget()));
 Widget focusW= XmGetFocusWidget(getBaseWidget());

 if ( (destW == CONTEXT_DATA(m_contextData)->mcBodyText ||
       focusW == CONTEXT_DATA(m_contextData)->mcBodyText))
 {
   // Now we are sure the focus is in the text area, 
   // We should perform those commands that are operable in the area

   if  (cmd == xfeCmdDelete)
   {
    XEvent *event = info->event;
    XtCallActionProc (CONTEXT_DATA(m_contextData)->mcBodyText, "delete-next-character", event, NULL, 0 );
   }
   else if ( cmd == xfeCmdSelectAll )
   {
	Widget text = CONTEXT_DATA(m_contextData)->mcBodyText;
        XmTextSetSelection(text, 0, XmTextGetLastPosition(text),
                                        info->event->xkey.time);
   }
   else if ( cmd == xfeCmdSpellCheck ) {
	   xfe_TextSpellCheck(m_contextData);
   }
#if 0
   else if ( cmd == xfeCmdFindInObject ) 
   {
      // Future work here
      return;
    }
    else if ( cmd == xfeCmdFindAgain )
    {
       // Future work here
       return;
    }
#endif
  }

#if 0
//On XFE Compose Window, we don't do Popup anymore...as 4.x Spec indicated.
// There are problem to deal with popup when view swaps...
// This is a XFE framework problem...
  if ( cmd == xfeCmdShowPopup )
  {
        // May need to find out if it's an HTML view or not
       XEvent *event = info->event;

       if (m_popup)
       delete m_popup;

       m_popup = new XFE_PopupMenu( (XFE_Frame *) getToplevel(),
		XfeAncestorFindApplicationShell(getToplevel()->getBaseWidget()));

       m_popup->addMenuSpec(popupmenu_spec);
       m_popup->position (event);
       m_popup->show();
  }
#endif
}




Boolean
XFE_TextEditorView::isCommandEnabled(CommandType cmd, void */*calldata*/, XFE_CommandInfo*)
{
XDEBUG( printf ("in XFE_TextEditorView::isCommandEnabled(%s)\n", Command::getString(cmd));)

 Widget destW = XmGetDestination(XtDisplay(getBaseWidget()));
 Widget focusW= XmGetFocusWidget(getBaseWidget());

 if ( cmd == xfeCmdShowPopup )
     return True;

 else if (cmd == xfeCmdSpellCheck)
 {
     return xfe_SpellCheckerAvailable(m_contextData);
 }

 else if ( (destW == CONTEXT_DATA(m_contextData)->mcBodyText ||
            focusW == CONTEXT_DATA(m_contextData)->mcBodyText))
 {
     // Now we are sure the focus is in the mcBodyText Area, 
     // we should turn on/off commands accordingly for this area

     if ( (cmd == xfeCmdDelete) ||
          (cmd == xfeCmdSelectAll) 
	)
    return True;
 }

XDEBUG( printf ("leaving XFE_TextEditorView::isCommandEnabled()\n");)
  return False;
}
 
Boolean
XFE_TextEditorView::handlesCommand(CommandType command, void */*calldata*/, XFE_CommandInfo*)
{
XDEBUG( printf ("in XFE_TextEditorView::handlesCommand(%s)\n", Command::getString(command));)

 Widget destW = XmGetDestination(XtDisplay(getBaseWidget()));
 Widget focusW= XmGetFocusWidget(getBaseWidget());

  if (command == xfeCmdGetNewMessages
      || command == xfeCmdAddNewsgroup
      || command == xfeCmdDelete
      || command == xfeCmdShowPopup
      || command == xfeCmdSpellCheck
      || command == xfeCmdDelete
      || command == xfeCmdSelectAll )
 {
    return True;
 }
 
XDEBUG( printf ("leaving XFE_TextEditorView::handlesCommand(%s)\n", Command::getString(command));)
  return False;
}

char*
XFE_TextEditorView::getPlainText()
{
  // This method suppose to get the text message in the widget
  // and return in a char string block
  return NULL;
}

// ---- These methods might need to move up to be XFE_EDITABLE ----------

void
XFE_TextEditorView::insertMessageCompositionText(
                const char* text, XP_Bool leaveCursorAtBeginning, 
				XP_Bool /*isHTML*/ )
{

  MWContext* context = getParent()->getContext();
  XmTextPosition pos = 0, newpos = 0;
  unsigned char *loc;
  Widget bodyTextW= CONTEXT_DATA(m_contextData)->mcBodyText;

  XtRemoveCallback(bodyTextW, XmNmodifyVerifyCallback,
                   fe_mail_text_modify_cb, getParent()->getContext());
		

  XtVaGetValues(bodyTextW, XmNcursorPosition, &pos, 0);
  loc = fe_ConvertToLocaleEncoding(INTL_DefaultWinCharSetID(context),
                                   (unsigned char *) text);
  XmTextInsert(bodyTextW, pos, (char*) loc);
  if (((char *) loc) != text) {
    XP_FREE(loc);
  }
  XtVaGetValues(bodyTextW, XmNcursorPosition, &newpos, 0);
  if (leaveCursorAtBeginning) {
    XtVaSetValues(bodyTextW, XmNcursorPosition, pos, 0);
  }
  else if (pos == newpos) {
    /* On some motif (eg. AIX), text insertion point is not moved after
     * inserted text. We depend on that here.
     *
     * WARNING: XXX I18N watch. The strlen might not be the right i18n way.
     */
    newpos = pos+strlen(text);
    XtVaSetValues(bodyTextW, XmNcursorPosition, newpos, 0);
  }
  XtAddCallback (bodyTextW, XmNmodifyVerifyCallback,
                   fe_mail_text_modify_cb, getParent()->getContext());
}

void
XFE_TextEditorView::getMessageBody(
		char **pBody, uint32 *body_size, 
		MSG_FontCode **font_changes)
{
  MWContext *context = getParent()->getContext();
  Dimension columns = 0;
  char *loc;
  unsigned char *tmp;
  Widget bodyTextW= CONTEXT_DATA(m_contextData)->mcBodyText;

  *pBody = 0;
  loc = 0;
  tmp = 0;
  XtVaGetValues (bodyTextW, XmNvalue, &loc, XmNcolumns, &columns, 0);
  if (fe_LocaleCharSetID & MULTIBYTE) {
    columns *= 2;
  }
  if (loc) {
    tmp = fe_ConvertFromLocaleEncoding(INTL_DefaultWinCharSetID(context),
                                       (unsigned char *) loc);
  }
  if (tmp) {

#ifdef WRAP_EDITOR_TO_WINDOW_WIDTH  /* Old way: always wrap to window width */

    if (columns <= 0) columns = 79;
    *pBody = (char *) XP_WordWrap(INTL_DefaultWinCharSetID(context), tmp,
                                 columns, 1 /* look for '>' */);

#else /* New way: obey the ``Wrap Lines to the new length specified by user'' toggle. */

    if (CONTEXT_DATA(context)->compose_wrap_lines_p)
      {
	if (fe_globalPrefs.msg_wrap_length >0)
        columns = fe_globalPrefs.msg_wrap_length;
	else columns = 72;
        *pBody = (char *) XP_WordWrap(INTL_DefaultWinCharSetID(context), tmp,
                                     columns, 1 /* look for '>' */);
      }
    else
      {
        /* Else, don't wrap it at all. */
        *pBody = (char*)tmp;
        tmp = 0;
      }
#endif /* New way. */

    if (loc != (char *) tmp) {
      XP_FREE(tmp);
    }
  }
  *body_size = (*pBody ? strlen(*pBody) : 0);
  *font_changes = 0;
}

void
XFE_TextEditorView::doneWithMessageBody(char* pBody)
{
  Widget bodyTextW= CONTEXT_DATA(m_contextData)->mcBodyText;
  char *loc;

  XtVaGetValues(bodyTextW, XmNvalue, &loc, 0);
  if (pBody != loc) {
    XP_FREE(pBody);
  }
}

Boolean
XFE_TextEditorView::isModified()
{

  return (!(CONTEXT_DATA(m_contextData)->mcCitedAndUnedited) && 
	CONTEXT_DATA(m_contextData)->mcEdited);
}




