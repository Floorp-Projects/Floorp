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
   ComposeView.h -- class definition for ComposeView
   Created: Dora Hsu<dora@netscape.com>, 23-Sept-96.
 */



#ifndef _xfe_composeview_h
#define _xfe_composeview_h

#include "rosetta.h"
#include "MNView.h"  /* Has to be the first include because of xp_xx.h */
#include "msgcom.h"
#include "Command.h" /* For compose window commands */
#include "EditorToolbar.h"
#include <Xm/Xm.h>


/* Class Definition */

class XFE_ComposeView : public XFE_MNView
{
public:
  XFE_ComposeView(XFE_Component *toplevel_component, Widget parent, XFE_View *parent_view, MWContext *context, 
		  MWContext *context_to_copy, MSG_CompositionFields* fields, MSG_Pane* p = NULL,
	const char *pInitalText = NULL,
	XP_Bool useHtml = False);

  virtual ~XFE_ComposeView();

  // Public functions
  XFE_View* getComposeFolderView() { 
    return m_folderViewAlias;}

  virtual Boolean isCommandEnabled(CommandType command, void *calldata = NULL,
								   XFE_CommandInfo* i = NULL);
  virtual Boolean isCommandSelected(CommandType command, void *calldata = NULL,
								   XFE_CommandInfo* i = NULL);
  virtual Boolean handlesCommand(CommandType command, void *calldata = NULL,
								   XFE_CommandInfo* i = NULL);
  virtual void doCommand(CommandType command, void *calldata = NULL,
								   XFE_CommandInfo* i = NULL);
  virtual char* commandToString(CommandType, void*, XFE_CommandInfo*);
  XFE_Command* getCommand(CommandType cmd);
  virtual XFE_View* getCommandView(XFE_Command*);
  virtual char*	  className() {return "XFE_ComposeView";}

	virtual int  initEditor();
	virtual void initEditorContents();
	virtual void updateCompToolbar();

	static const char* tabBeforeSubject;
	static const char* tabAfterSubject;
	static const char* tabPrev;
	static const char* tabNext;
	static const char* eFocusIn;
	static const char* eFocusOut;
	HG12172

	static void    daFocusProc(Widget,XtPointer,XEvent*,Boolean*);
	static void   topFocusProc(Widget,XtPointer,XEvent*,Boolean*);
	static void  nullFocusProc(Widget,XtPointer,XEvent*,Boolean*);

	void saveFocus();
	void focusIn();
	void focusOut();

  void*   getComposerData();
  void    setComposerData(void* data);
 
  void*   getAttachmentData();
  void    setAttachmentData(void* data);
  XFE_CALLBACK_DECL(afterRealizeWidget)

  void    insertMessageCompositionText(
                const char* text, XP_Bool leaveCursorBeginning);

  void 	  getMessageBody(char **pBody, uint32 *body_size, 
		MSG_FontCode **font_changes);
  void    doneWithMessageBody(char *pBody);
  void    setComposeWrapState(XP_Bool wrap_p);
  static int doQuoteMessage(void *closure, const char *data);
  XP_Bool isDelayedSent();
  XP_Bool isHTML();
  static const char *newSubject;
  static void selectPriorityCallback(Widget, XtPointer, XtPointer);

  static void unGrabEditorProc(Widget,XtPointer,XEvent*,Boolean*);
  void unGrabEditor();
  void editorFocus(Boolean);
  void show();
  Boolean isModified();

  XP_Bool hasFocus();

protected:
  void	  initialize();
  void    updateHeaderInfo();
  void  createSubjectBar(Widget);
  XP_Bool   continueAfterSanityCheck();

  XP_Bool   handleStealthPasteQuote(CommandType, XFE_CommandInfo*);
  void      quoteText(char *);
 
private:
  void* mailComposer;
  void* mailAttachment;
  void  createWidget(Widget);

  XFE_View *m_folderViewAlias; // These are just pointer aliases to views
  XFE_View *m_textViewAlias;   // this class does not own these views
  XFE_View *m_htmlViewAlias;   // It's the parent class who owns these views

  Widget m_editorFormW;
  Widget m_subjectFormW;
  Widget m_subjectW;
  Widget m_toolbarW;
  Widget m_focusW;
  Widget m_nullW;

  Widget m_addrTextW;
  Widget m_addrTypeW;

  Widget m_daW;
  Widget m_topW;
  Widget m_expand_arrowW;

  XFE_EditorToolbar* m_format_toolbar;
  Boolean m_expanded;

        XP_Bool m_editorFocusP;

        XP_Bool m_plaintextP;
        XP_Bool m_requestHTML;
        XP_Bool m_initEditorP;
        XP_Bool m_postEditorP;
        XP_Bool m_puntEditorP;
        XP_Bool m_delayEditorP;
        XP_Bool m_dontQuoteP;

  int quoteMessage(const char *data);
  void displayDefaultTextBody();

  XFE_CALLBACK_DECL(tabToEditor)

  XP_Bool m_delayedSent;
  char *m_pInitialText;

  void	 subjectFocus();
  void	 textEditorFocus(Widget);
  void	 addressTextFocus(Widget);
  void	 addressTypeFocus(Widget);
  void	 subjectChange(Widget, XtPointer);
  void	 subjectUpdate(Widget, XtPointer);
  void	 toggleAddressArea();
  void   setDefaultSubjectHeader();
  XFE_CALLBACK_DECL(tabToSubject)
  XFE_CALLBACK_DECL(textEditorFocusIn)
  XFE_CALLBACK_DECL(addressTextFocusIn)
  XFE_CALLBACK_DECL(addressTypeFocusIn)
  static void subjectChangeCallback(Widget, XtPointer, XtPointer);
  static void subjectFocusIn_CB(Widget, XtPointer, XtPointer);

  static void quotePrimarySel_cb(Widget, XtPointer, 
								 Atom *, Atom *, XtPointer, 
								 unsigned long *, int *);

  static void expandCallback(Widget, XtPointer, XtPointer);

};



#endif /* _xfe_composeview_h */
