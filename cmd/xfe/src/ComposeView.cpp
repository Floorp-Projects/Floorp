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
   ComposeView.cpp -- presents view for composing a mail messages.
   Created: Dora Hsu <dora@netscape.com>, 23-Sept-96.
   */



#include "xfe2_extern.h"
#include "ComposeView.h"
#include "ComposeFolderView.h"
#include "AddressOutliner.h"
#include "TextEditorView.h"
#include "EditorView.h"
#include "Command.h"
#include "new_manage.h"
#include "xp_mem.h"
#include "ViewGlue.h"
#include <Xm/Frame.h>
#include <Xfe/ToolBox.h>
#include "xfe.h"
#include "edt.h"
#include "felocale.h"
#include "intl_csi.h"
#include "secnav.h"

#include "xeditor.h"

/* for XP_GetString() */
#include <xpgetstr.h>
extern int MK_MSG_EMPTY_MESSAGE;
extern int MK_MSG_DOUBLE_INCLUDE;
extern int MK_MSG_MIXED_SECURITY;
extern int MK_MSG_MISSING_SUBJECT;
extern int XFE_NO_SUBJECT;

#include "prefs.h"

#if defined(DEBUG_dora) || defined(DEBUG_sgidev)
#define XDEBUG(x) x
#else
#define XDEBUG(x)
#endif

static Atom XFE_TEXT_ATOM = 0;

extern "C" {
  void           xfe2_EditorInit(MWContext* context);
  void           xfe2_EditorImSetCoords(MWContext* context);
  void           xfe2_EditorCaretHide(MWContext* context);
  void           xfe2_EditorCaretShow(MWContext* context);
  void           xfe2_EditorWaitWhileContextBusy(MWContext* context);
  char          *xfe2_GetClipboardText(Widget focus, int32 *internal);
  CL_Compositor *fe_create_compositor(MWContext *context);
  Boolean        fe_EditorCanPasteCache(MWContext* context, int32* r_length);
  Boolean        fe_EditorCanPasteLocal(MWContext* context, int32* r_length);
  Boolean        fe_EditorCanPasteText(MWContext* context, int32* r_length);
  Boolean        fe_EditorCut(MWContext* context, Time timestamp);
  Boolean        fe_EditorCopy(MWContext* context, Time timestamp);
  Boolean        fe_EditorPaste(MWContext* context, Time timestamp);
  Widget         fe_EditorCreateComposeToolbar(MWContext*, Widget, char*);
}


extern "C" XtPointer fe_GetFont(MWContext *context, int sizeNum, int fontmask);
extern "C" void
fe_set_compose_wrap_state(MWContext *context, XP_Bool wrap_p);

extern "C" void fe_addToContextList(MWContext *context);

extern "C" void fe_cut_cb (Widget widget, XtPointer closure,
                               XtPointer call_data);
extern "C" void fe_paste_cb(Widget widget, XtPointer closure,
                               XtPointer call_data);
extern "C" void fe_copy_cb (Widget widget, XtPointer closure,
                               XtPointer call_data);
extern "C" void fe_paste_quoted_cb (Widget widget, XtPointer closure,
                               XtPointer call_data);
extern "C" void fe_mailNew_cb (Widget widget, XtPointer closure,
                               XtPointer call_data);
extern "C" void fe_load_default_font (MWContext *context);
extern "C" void fe_find_scrollbar_sizes (MWContext *context);
extern "C" void fe_get_final_context_resources (MWContext *context);
extern "C" void fe_InitScrolling (MWContext *context);
extern "C" void fe_InitRemoteServerWindow (MWContext *context);
extern "C" void fe_SensitizeMenus (MWContext *context);
extern "C" int FE_GetMessageBody (MSG_Pane* comppane,
                   char **body,
                   uint32 *body_size,
                   MSG_FontCode **font_changes);
extern XtResource fe_Resources[];
extern Cardinal fe_ResourcesSize;

const char *XFE_ComposeView::tabBeforeSubject = "XFE_ComposeView::tabBeforeSubject";
const char *XFE_ComposeView::tabAfterSubject = "XFE_ComposeView::tabAfterSubject";
const char *XFE_ComposeView::tabPrev   = "XFE_ComposeView::tabPrev";
const char *XFE_ComposeView::tabNext   = "XFE_ComposeView::tabNext";
const char *XFE_ComposeView::updateSecurityOption= "XFE_ComposeView::updateSecurityOption";

static void TabBeforeSubjectTraverse(Widget w, XEvent *, String *, Cardinal *)
{
   XtPointer client;
   XFE_ComposeView *obj ;

   XtVaGetValues( w, XmNuserData, &client, 0);

   XDEBUG(printf("Inside ... TabBeforeSubjectTraverse to notify...\n");)

   obj = (XFE_ComposeView *) client;
   obj->getToplevel()->notifyInterested(XFE_ComposeView::tabBeforeSubject, client);
}

static void TabAfterSubjectTraverse(Widget w, XEvent *, String *, Cardinal *)
{
   XtPointer client;
   XFE_ComposeView *obj ;

   XtVaGetValues( w, XmNuserData, &client, 0);
   XDEBUG(printf("Inside ... TabAfterSubjectTraverse to notify...\n");)

   obj = (XFE_ComposeView *) client;
   obj->getToplevel()->notifyInterested(XFE_ComposeView::tabAfterSubject, client);
}

extern XtAppContext fe_XtAppContext;

static XtActionsRec actions[] =
  {{"PrevTabTraverse",     TabBeforeSubjectTraverse     },
   {"NextTabTraverse",     TabAfterSubjectTraverse     }};


static char prevtraverseTranslations[] =
"~Ctrl Shift <Key>Tab:  PrevTabTraverse()\n\
  Ctrl Shift <Key>Tab:  NextTabTraverse()\n\
      ~Shift <Key>Tab:  NextTabTraverse()";



static void PrevTabTraverse(Widget w, XEvent *, String *, Cardinal *)
{
   XtPointer client;
   XFE_ComposeView *obj ;
 
#ifdef DEBUG_rhess
   fprintf(stderr, "Editor::[ PrevTabTraverse ]\n");
#endif

   XtVaGetValues( w, XmNuserData, &client, 0);
 
   obj = (XFE_ComposeView *) client;
   obj->getToplevel()->notifyInterested(XFE_ComposeView::tabPrev, client);
}

static void NextTabTraverse(Widget w, XEvent *, String *, Cardinal *)
{
   XtPointer client;
   XFE_ComposeView *obj ;
 
#ifdef DEBUG_rhess
   fprintf(stderr, "Editor::[ NextTabTraverse ]\n");
#endif

   XtVaGetValues( w, XmNuserData, &client, 0);
 
   obj = (XFE_ComposeView *) client;
   obj->getToplevel()->notifyInterested(XFE_ComposeView::tabNext, client);
}
 
extern XtAppContext fe_XtAppContext;
 
static XtActionsRec cv_actions[] =
{{"CvTabTraversePrev",    PrevTabTraverse     },
 {"CvTabTraverseNext",    NextTabTraverse     }};
 
 
static char cvTraverseTranslations[] =
"~Meta ~Alt Ctrl ~Shift <Key>Tab:  CvTabTraversePrev()\n\
 ~Meta ~Alt Ctrl  Shift <Key>Tab:  CvTabTraverseNext()";



XFE_ComposeView::XFE_ComposeView(XFE_Component *toplevel_component,
				 Widget parent,
				 XFE_View *parent_view,
				 MWContext *context,
				 MWContext *context_to_copy, 
				 MSG_CompositionFields * fields,
				 MSG_Pane *p,
				 const char *pInitialText,
				 XP_Bool preferToUseHtml)
  : XFE_MNView(toplevel_component, parent_view, context, p)
{
  INTL_CharSetInfo c;
  INTL_CharSetInfo c_to_copy;
	 
  c = LO_GetDocumentCharacterSetInfo(context);
  if (context_to_copy)
  c_to_copy = LO_GetDocumentCharacterSetInfo(context_to_copy);
  else
        c_to_copy = c;

  MSG_Pane *pane = NULL;

  // XXX should really be done in the frame..
  if (context_to_copy)
  CONTEXT_DATA(m_contextData)->xfe_doc_csid = 
		CONTEXT_DATA(context_to_copy)->xfe_doc_csid;
  else
        CONTEXT_DATA(m_contextData)->xfe_doc_csid = fe_globalPrefs.doc_csid;

  INTL_SetCSIWinCSID(c, INTL_GetCSIWinCSID(c_to_copy));

  mailComposer = NULL;
  mailAttachment = NULL;
  m_expanded = True;
  m_toolbarW = NULL;
  m_format_toolbar = NULL;
  m_editorFormW = NULL;
  m_subjectFormW = NULL;
  m_editorFocusP = False;
  m_subjectW = NULL;
  m_addrTextW = NULL;
  m_addrTypeW = NULL;
  m_focusW = NULL;
  m_nullW  = NULL;
  m_daW    = NULL;
  m_topW   = NULL;

  m_folderViewAlias = NULL; // These are just pointer aliases to views
  m_textViewAlias = NULL;   // this class does not own these views
  m_htmlViewAlias = NULL;   // It's the parent class who owns these views
  m_delayedSent = False;
  m_plaintextP = !fe_globalPrefs.send_html_msg;
  m_pInitialText = NULL;
  if ( pInitialText && strlen(pInitialText) )
  	m_pInitialText = XP_STRDUP(pInitialText);

  m_requestHTML = preferToUseHtml;
  m_initEditorP = False;
  m_postEditorP = False;
  m_puntEditorP = False;
  m_delayEditorP = False;
  m_dontQuoteP = False;

  if ( m_requestHTML ) {
	  m_plaintextP = False;
  }
  else {
	  m_plaintextP = True;
  }

#ifdef DEBUG_rhess
    if (m_plaintextP)
		fprintf(stderr, "ComposeView::[ plaintext ]\n");
	else
		fprintf(stderr, "ComposeView::[ HTML ]\n");
#endif

  getToplevel()->registerInterest(XFE_Component::afterRealizeCallback,
             this,
             (XFE_FunctionNotification)afterRealizeWidget_cb); 

  { // Create base widgets
    Widget w;
    /*w = XmCreateForm(parent, "composeViewBaseWidget", NULL, 0);*/

    w = XtVaCreateWidget("panedw",
    			xmPanedWindowWidgetClass,
                        parent,
			XmNnavigationType, XmTAB_GROUP,
                        NULL);
    setBaseWidget(w);
  }



  if (!p)
  {
    pane = MSG_CreateCompositionPane(m_contextData, 
				     //context_to_copy required by xp
				     context_to_copy, 
				     fe_mailNewsPrefs, fields,  XFE_MNView::m_master );
    setPane(pane);
    CONTEXT_DATA(m_contextData)->comppane = (MSG_Pane*)pane;

    CONTEXT_DATA(m_contextData)->mcCitedAndUnedited = True;
    CONTEXT_DATA(m_contextData)->mcEdited = False;
  }

  CONTEXT_DATA (m_contextData)->compose_wrap_lines_p = True;

  createWidget(getBaseWidget());
}

XFE_ComposeView::~XFE_ComposeView()
{
	//  destroyPane();	
	// Don't call this here.  Calling MSG_MailCompositionAllConnections- 
	//  complete will destroy the composition pane.

	if (!m_plaintextP) {
		XtRemoveEventHandler(m_subjectW, 
							 ButtonPressMask, FALSE, unGrabEditorProc, this);
		XtRemoveEventHandler(m_daW,  
							 FocusChangeMask, FALSE, daFocusProc, this);
		XtRemoveEventHandler(m_topW, 
							 FocusChangeMask, FALSE, topFocusProc, this);
		if (m_nullW)
			XtRemoveEventHandler(m_nullW, FocusChangeMask, FALSE, 
								 nullFocusProc, this);
	}
}

static ToolbarSpec alignment_menu_spec[] = {
	{ xfeCmdSetAlignmentStyleLeft,	 PUSHBUTTON, &ed_left_group },
	{ xfeCmdSetAlignmentStyleCenter, PUSHBUTTON, &ed_center_group },
	{ xfeCmdSetAlignmentStyleRight,	 PUSHBUTTON, &ed_right_group },
	{ NULL }
};

static ToolbarSpec goodies_menu_spec[] = {
	{ xfeCmdInsertLink,           PUSHBUTTON, &ed_link_group   },
	{ xfeCmdInsertTarget,         PUSHBUTTON, &ed_target_group },
	{ xfeCmdInsertImage,          PUSHBUTTON, &ed_image_group  },
	{ xfeCmdInsertHorizontalLine, PUSHBUTTON, &ed_hrule_group  },
	{ xfeCmdInsertTable,          PUSHBUTTON, &ed_table_group  },
	{ NULL }
};

static ToolbarSpec editor_style_toolbar_spec[] = {

	{ xfeCmdSetParagraphStyle, COMBOBOX },
	{ xfeCmdSetFontFace,       COMBOBOX },
	{ xfeCmdSetFontSize,       COMBOBOX },
	{ xfeCmdSetFontColor,      COMBOBOX },
	TOOLBAR_SEPARATOR,

	{ xfeCmdToggleCharacterStyleBold,	   TOGGLEBUTTON, &ed_bold_group },
	{ xfeCmdToggleCharacterStyleItalic,	   TOGGLEBUTTON, &ed_italic_group },
	{ xfeCmdToggleCharacterStyleUnderline, TOGGLEBUTTON, &ed_underline_group },
	{ xfeCmdClearAllStyles,                PUSHBUTTON  , &ed_clear_group },
	TOOLBAR_SEPARATOR,

	{ xfeCmdInsertBulletedList,	TOGGLEBUTTON, &ed_bullet_group },
	{ xfeCmdInsertNumberedList,	TOGGLEBUTTON, &ed_number_group },
	TOOLBAR_SEPARATOR,

	{ xfeCmdOutdent,	PUSHBUTTON, &ed_outdent_group },
	{ xfeCmdIndent,	PUSHBUTTON, &ed_indent_group },
	{ xfeCmdSetAlignmentStyle, CASCADEBUTTON, &ed_left_group, 0, 0, 0,
	  (MenuSpec*)&alignment_menu_spec },
	{ "editorGoodiesMenu", CASCADEBUTTON, &ed_insert_group, 0, 0, 0,
	  (MenuSpec*)&goodies_menu_spec },
	{ NULL }
};

void
XFE_ComposeView::createSubjectBar(Widget parent)
{
   Cardinal ac =0;
   Arg av[20];
   m_subjectFormW = XtVaCreateManagedWidget("subjectFormW",
			xmFormWidgetClass,  parent,
			XmNtopAttachment, XmATTACH_FORM,
			XmNleftAttachment, XmATTACH_FORM,
			XmNrightAttachment, XmATTACH_FORM,
			XmNbottomAttachment, XmATTACH_NONE, 0);

   ac = 0;
   XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
   if (m_plaintextP) {
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
   }
   else {
	XtSetArg (av [ac], XmNtraversalOn, False); ac++;
   }
   XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
   XtSetArg (av [ac], XmNrightAttachment, XmATTACH_NONE); ac++;
   XtSetArg (av [ac], XmNarrowDirection, XmARROW_UP); ac++;
   XtSetArg (av [ac], XmNtraversalOn, False); ac++;
   XtSetArg (av [ac], XmNshadowThickness, 0); ac++;
   m_expand_arrowW = XmCreateArrowButton(m_subjectFormW, "toggleAddressArea",
										 av, ac);
   fe_WidgetAddToolTips(m_expand_arrowW);

   m_expanded = True;
   XtAddCallback(m_expand_arrowW, XmNactivateCallback, expandCallback, this);
   XtManageChild(m_expand_arrowW);


    /* Create the text field */
    char  name[30];
    char  buf[100];
    Widget subjectLabelW;

    PR_snprintf(name, sizeof (name), "%s", "subject");

    ac = 0;
    XtSetArg(av[ac], XmNleftAttachment, XmATTACH_WIDGET); ac++;
    XtSetArg(av[ac], XmNleftWidget, m_expand_arrowW); ac++;
    XtSetArg(av[ac], XmNtopAttachment, XmATTACH_FORM); ac++;
    if (m_plaintextP) {
	XtSetArg(av[ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
    }
    XtSetArg(av[ac], XmNrightAttachment, XmATTACH_NONE); ac++;
    PR_snprintf(buf, sizeof (buf), "%sLabel", name);
    subjectLabelW = XmCreateLabelGadget(m_subjectFormW, buf, av, ac);
    XtManageChild(subjectLabelW);

    Dimension height;

    ac =0;
    m_subjectW = fe_CreateTextField(m_subjectFormW, name, av, ac);

    XtVaGetValues(m_subjectW, XmNheight, &height, 0);
 
    // Set default priority...should be consistent with default menu priority toggle state
    ((XFE_ComposeFolderView*)m_folderViewAlias)->selectPriority(MSG_NormalPriority);

	Widget toolbox_widget = NULL;
	   
    if (!m_plaintextP) {
		XFE_Component* top_level = getToplevel();
		Widget         top_widget = top_level->getBaseWidget();

		//    I don't think we need this anymore...djw
		CONTEXT_DATA(getContext())->top_area = top_widget;
	
		XFE_Toolbox* toolbox = new XFE_Toolbox(top_level, m_subjectFormW);
		
		toolbox_widget = toolbox->getBaseWidget();

		// Make sure the toolbox is efficiently resized
		XtVaSetValues(toolbox_widget,XmNusePreferredHeight,True,NULL);

		toolbox->show(); // manage

		m_format_toolbar = new XFE_EditorToolbar(top_level,
												 toolbox,
												 "formattingToolbar",
									 (ToolbarSpec*)&editor_style_toolbar_spec,
												 False);
    }

    // Register Action

    XtAppAddActions( fe_XtAppContext, actions,  2);

    XtVaSetValues(m_subjectW, XmNuserData, this,  NULL);
    XtOverrideTranslations(m_subjectW, 
		XtParseTranslationTable(prevtraverseTranslations));


	if (!m_plaintextP) {
		XtInsertEventHandler(m_subjectW, ButtonPressMask, FALSE, 
							 unGrabEditorProc, this, XtListTail);
	}

    ac = 0;
    XtSetArg(av[ac], XmNleftAttachment, XmATTACH_WIDGET); ac++;
    XtSetArg(av[ac], XmNleftWidget, subjectLabelW); ac++;
    XtSetArg(av[ac], XmNtopAttachment, XmATTACH_FORM); ac++;
    if (m_plaintextP) {
	XtSetArg(av[ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
    }
    XtSetArg(av[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
    XtSetArg(av[ac], XmNrightOffset, 5); ac++;
    XtSetArg(av[ac], XmNtraversalOn, True); ac++;
    XtSetArg(av[ac], XmNhighlightOnEnter, True); ac++;
    XtSetValues(m_subjectW, av, ac);
    XtManageChild(m_subjectW);

    if (!m_plaintextP) {
	ac = 0;
	XtSetArg(av[ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
	XtSetArg(av[ac], XmNtopWidget, m_subjectW); ac++;
	XtSetArg(av[ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
	XtSetArg(av[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg(av[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg(av[ac], XmNtraversalOn, False); ac++;
	XtSetValues(toolbox_widget, av, ac);
	XtManageChild(toolbox_widget);
	}

    if (fe_globalData.nonterminal_text_translations) {
      XtOverrideTranslations(m_subjectW,
                       fe_globalData.nonterminal_text_translations);
     }

	
	if (m_plaintextP) {
		getToplevel()->registerInterest( XFE_TextEditorView::textFocusIn, 
					 this, (XFE_FunctionNotification)textEditorFocusIn_cb);
	}
	getToplevel()->registerInterest( XFE_AddressOutliner::textFocusIn, this,
				   (XFE_FunctionNotification)addressTextFocusIn_cb);
	getToplevel()->registerInterest( XFE_AddressOutliner::typeFocusIn, this,
				   (XFE_FunctionNotification)addressTypeFocusIn_cb);

   getToplevel()->registerInterest( XFE_AddressOutliner::tabNext, this,
				   (XFE_FunctionNotification)tabToSubject_cb);

   getToplevel()->registerInterest( XFE_ComposeView::tabPrev, this,
				   (XFE_FunctionNotification)tabToSubject_cb);

   setDefaultSubjectHeader();

   // Add callback after the default subject has been set to avoid
   // extra trip to valueChangeCallback
   XtAddCallback(m_subjectW, XmNvalueChangedCallback, 
				 subjectChangeCallback, (XtPointer)this);

   XtAddCallback(m_subjectW, XmNfocusCallback, 
				 subjectFocusIn_CB, (XtPointer)this);
}

void
XFE_ComposeView::setDefaultSubjectHeader()
{
  const char * subject = MSG_GetCompHeader(getPane(),MSG_SUBJECT_HEADER_MASK);
  char *pStr = XP_STRDUP(subject);


  if (pStr && strlen(pStr))
  {
     fe_SetTextFieldAndCallBack(m_subjectW, pStr);
  }
  XP_FREE(pStr);
}

void
XFE_ComposeView::show()
{
	if (!m_plaintextP && m_format_toolbar != NULL)
		m_format_toolbar->show();

	XFE_MNView::show(); // call super
}

void
XFE_ComposeView::createWidget(Widget )

{
  Boolean wrap_p ;
  {
    // Create subviews
    XFE_ComposeFolderView *view = 
		new XFE_ComposeFolderView(getToplevel(), (XFE_View*)this, 
			(MSG_Pane*)getPane(),getContext());

    view->createWidgets(getBaseWidget(), m_plaintextP);

    addView(view); // Parent class owns this view via "add" operation
    m_folderViewAlias = (XFE_View*) view;
    XtVaSetValues( view->getBaseWidget(), 
		XmNallowResize, True,  NULL);
    view->show(); 

  }
  {
    // Creating the subject bar....

    Widget lowerForm = XmCreateForm(getBaseWidget(), 
									"lowrPaneFormWidget", NULL, 0 );
    XtVaSetValues(lowerForm, 
				  XmNshadowThickness, 0,
				  XmNmarginWidth, 0,
				  XmNmarginHeight, 0,
				  XmNnavigationType, XmTAB_GROUP, NULL );

    createSubjectBar(lowerForm);

    m_editorFormW = XmCreateFrame(lowerForm, 
								  "composeViewEditFormWidget", NULL, 0);

    XtVaSetValues(m_editorFormW, 
				  XmNnavigationType, XmTAB_GROUP,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, m_subjectFormW,
				  XmNtopOffset, 5,
				  XmNbottomAttachment, XmATTACH_FORM,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNrightAttachment, XmATTACH_FORM, NULL);

    XtManageChild(m_editorFormW);
    XtManageChild(lowerForm);
  }
  if (m_plaintextP)
  {
    // Create Plain Editor View
    XFE_TextEditorView *view2 = 
		new XFE_TextEditorView(getToplevel(),(XFE_View*) this,
			(MSG_Pane*)getPane(), m_contextData);
			
    wrap_p = CONTEXT_DATA (m_contextData)->compose_wrap_lines_p;
    view2->createWidgets(m_editorFormW, wrap_p);
    addView(view2); // Parent class owns this view via "add" operation
    m_textViewAlias = (XFE_View*) view2;
    view2->show(); 

    // This is a hack...
    //CONTEXT_DATA(m_contextData)->mcBodyText = m_textViewAlias->getBaseWidget();

    getToplevel()->registerInterest(
	XFE_ComposeView::tabAfterSubject, this,
        (XFE_FunctionNotification)tabToEditor_cb);
   }
  else
  {
    m_contextData->is_editor = True;
	m_contextData->bIsComposeWindow = TRUE;

    // CONTEXT_DATA (m_contextData)->scrolled_width = 0;
    // CONTEXT_DATA (m_contextData)->scrolled_height = 0;

    XFE_EditorView *view3 = 
		new XFE_EditorView(getToplevel(), m_editorFormW,
				   (XFE_View*) this, m_contextData);
			
    m_daW  = CONTEXT_DATA(m_contextData)->drawing_area;
	m_topW = getToplevel()->getBaseWidget();

#ifdef DEBUG_rhess2
	// XmNpreeditType, "overthespot",
	// XmNpreeditType, "root",
	// XmNpreeditType, "none",

	XtVaSetValues (m_topW,
				   XmNpreeditType, "offthespot",
				   0);
#endif

    XtVaSetValues (XtParent(m_daW), 
				   XmNborderWidth, 2, 
				   XmNborderColor, 
				   CONTEXT_DATA (m_contextData)->default_bg_pixel,
				   0);

	XtVaSetValues (m_daW, 
				   XmNtraversalOn, True,
				   0);
    
    XtInsertEventHandler(m_daW,  FocusChangeMask, FALSE, 
						 daFocusProc, this, XtListTail);
    XtInsertEventHandler(m_topW, FocusChangeMask, FALSE, 
						 topFocusProc, this, XtListTail);

    // wrap_p = CONTEXT_DATA (m_contextData)->compose_wrap_lines_p;

    addView(view3); // Parent class owns this view via "add" operation

    m_htmlViewAlias = (XFE_View*) view3;
    view3->show(); 

    // This is a hack...
    CONTEXT_DATA (m_contextData)->mcBodyText = NULL;

    getToplevel()->registerInterest(
	XFE_ComposeView::tabAfterSubject, this,
        (XFE_FunctionNotification)tabToEditor_cb);
   
    // getToplevel()->registerInterest(
    //     XFE_AddressOutliner::tabPrev, this,
    //    (XFE_FunctionNotification)tabToEditor_cb);
  }


}

XFE_CALLBACK_DEFN(XFE_ComposeView,afterRealizeWidget)(XFE_NotificationCenter *,
                                 void *, void*)
{
  XmFontList fontList;
  // fe_MailComposeWin_Activate(context);
  fontList = (XmFontList)fe_GetFont (m_contextData, 3, LO_FONT_FIXED);
  // Set the default font in the text widget
  if (m_plaintextP)
	  ((XFE_TextEditorView*)m_textViewAlias)->setFont(fontList);
  else {
	  // Register Action

	  XtAppAddActions( fe_XtAppContext, cv_actions,  2);

#ifdef DEBUG_rhess
	  fprintf(stderr, "Editor::[ override ]\n");
#endif

	  XtVaSetValues(
					CONTEXT_DATA (m_contextData)->drawing_area, 
					XmNuserData, this, 0
					);
	  XtOverrideTranslations(
							 CONTEXT_DATA (m_contextData)->drawing_area, 
							 XtParseTranslationTable(cvTraverseTranslations)
							 );

  }

  // Initialize widget now
  initialize();

  // Get Font
  //fe_SensitizeMenus (m_contextData);

  // Set focus to the appropriate place depending on our mode:
  // If there's no recipient, the AddressOutliner will grab the focus.
  const char * subject = MSG_GetCompHeader(getPane(),MSG_SUBJECT_HEADER_MASK);

  // If there's a recipient but no subject, put the focus in the
  // subject field;
  if (!subject || strlen(subject) <= 0)
	  {
		  XmProcessTraversal(m_subjectW, XmTRAVERSE_CURRENT);
	  }
  else	// else shift keyboard focus into the editor
	  {
		  focusIn();
	  }

  MSG_SetCompBoolHeader(getPane(),MSG_ATTACH_VCARD_BOOL_HEADER_MASK, fe_globalPrefs.attach_address_card);
}

Boolean
XFE_ComposeView::isCommandEnabled(CommandType command, void *calldata,
								  XFE_CommandInfo* info)
{
  MSG_Pane *pane = getPane();

    if (pane && MSG_DeliveryInProgress(pane) && command != xfeCmdStopLoading)
        return False;

	Widget destW= XmGetDestination(XtDisplay(getBaseWidget()));
	Widget focusW= XmGetFocusWidget(getBaseWidget());
	if ((command == xfeCmdWrapLongLines && !m_plaintextP)
		)
		return False;

	if (command == xfeCmdAttachAddressBookCard) {
		XDEBUG(printf("XFE_ComposeView::isCommandEnabled(xfeCmdAttachAddressBookCard)\n"));
		return TRUE;
	}
	else if ( (command == xfeCmdNewMessage )
		 || (command == xfeCmdSaveDraft)
		 || (command == xfeCmdQuoteOriginalText)
		 || (command == xfeCmdQuote)
		 || (command == xfeCmdAttach)
		 || (command == xfeCmdWrapLongLines)
		 || (command == xfeCmdEditPreferences)
		 || (command == xfeCmdSendMessageNow) 
		 || (command == xfeCmdSendMessageLater) 
		 || (command == xfeCmdSearchAddress) 
		 || (command == xfeCmdSetPriorityHighest)
		 || (command == xfeCmdSetPriorityHigh)
		 || (command == xfeCmdSetPriorityNormal)
		 || (command == xfeCmdSetPriorityLow)
		 || (command == xfeCmdSetPriorityLowest)
		 || (command == xfeCmdToggleAddressArea)
		 || (command == xfeCmdToggleFormatToolbar) )
		return True;
	else if ( command == xfeCmdDeleteItem ) {
		  if ( m_plaintextP ) {
			  return True;
		  }
		  else {
			  if ( m_focusW ) {
				  if ( m_focusW != m_addrTypeW )
					  if ((m_focusW == m_subjectW) || 
						  (m_focusW == m_addrTextW)) {
						  return True;
					  }
					  else {
						  if (m_plaintextP) {
							  // NOTE:  textEditorView handles this one...
							  //
							  XFE_TextEditorView* tev = 
								  (XFE_TextEditorView*)m_textViewAlias;
							  return tev->isCommandEnabled(xfeCmdDelete,
														   calldata,info);
						  }
						  else {
							  return False;
						  }
					  }
				  else
					  return False;
			  }
			  else {
				  XFE_EditorView *html = (XFE_EditorView*) m_htmlViewAlias;
				  XFE_Command*    handler = html->getCommand(command);
			  
				  if (handler != NULL) {
					  return handler->isEnabled(html, info);
				  }
				  else {
					  return False;
				  }
			  }
		  }
	}
	else if ( command == xfeCmdUndo ) {
		if ( m_plaintextP ) {
			return False;
		}
		else {
			if ( m_focusW ) {
				return False;
			}
			else {
				XFE_EditorView *html = (XFE_EditorView*) m_htmlViewAlias;
				XFE_Command*    handler = html->getCommand(command);
			  
				if (handler != NULL) {
					return handler->isEnabled(html, info);
				}
				else {
					return False;
				}
			}
		}
	}
	else if ( command == xfeCmdCut   ||
			  command == xfeCmdCopy ) {
		// NOTE... [ this should only be enabled if there's a selection..
		//
		if ( m_focusW ) {
			if (XmIsTextField( m_focusW )) {
				XmTextPosition left;
				XmTextPosition right;
				Boolean gotcha = False;

				// NOTE: we disable Cut/Copy if we're in the type
				//       field of the AddressOutliner...
				//
				if (m_focusW != m_addrTypeW) {
					gotcha = XmTextFieldGetSelectionPosition(m_focusW, 
															 &left, &right);
				}

				if (gotcha) {
					return ((right - left) > 0);
				}
				else {
					return False;
				}
			}
			else {
				if (XmIsText ( m_focusW )) {
					XmTextPosition left;
					XmTextPosition right;
					  
					Boolean gotcha = False;
					  
					// NOTE: we disable Cut/Copy if we're in the type
					//       field of the AddressOutliner...
					//
					if (m_focusW != m_addrTypeW) {
						gotcha = XmTextGetSelectionPosition(m_focusW, 
															&left, &right);
					}

					if (gotcha) {
						return ((right - left) > 0);
					}
					else {
						return False;
					}
				}
				else {
					return False;
				}
			}
		}
		else {
			if (m_plaintextP) {
				// WARNING... [ the focus isn't being set correctly
				//
#ifdef DEBUG_rhess
				fprintf(stderr, "ERROR::[ m_focusW = NULL ]\n");
#endif
				return False;
			}
			else {
				XFE_EditorView *html = (XFE_EditorView*) m_htmlViewAlias;
				XFE_Command*    handler = html->getCommand(command);
			  
				if (handler != NULL) {
					return handler->isEnabled(html, info);
				}
				else {
					return False;
				}
			}
		}
	}
	else if ( command == xfeCmdPaste ||
			  command == xfeCmdPasteAsQuoted ) {
		if ( m_focusW ) {
			// NOTE... [ disable pasteAsQuoted for non-editor widgets
			//
			if ( command == xfeCmdPasteAsQuoted ) {
				if ( m_focusW == m_subjectW ||
					 m_focusW == m_addrTextW ||
					 m_focusW == m_addrTypeW ) {
					return False;
				}
			}
			else {
				// NOTE... [ disable paste for the address type widget
				//
				if (m_focusW == m_addrTypeW) {
					return False;
				}
			}
		}
		else {
			if (m_plaintextP) {
				// WARNING... [ the focus isn't being set correctly
				//
#ifdef DEBUG_rhess
				fprintf(stderr, "ERROR::[ m_focusW = NULL ]\n");
#endif
				return False;
			}
		}

		if (m_plaintextP) {
			// NOTE... [ we call the editor code to check the clipboard
			//
			int32 len;

			if (fe_EditorCanPasteCache(m_contextData, &len) ||
				fe_EditorCanPasteLocal(m_contextData, &len) ||
				fe_EditorCanPasteText (m_contextData, &len)) {
				return True;
			}
			else {
				return False;
			}
		}
		else {
			// NOTE... [ this should only be enabled if there's 
			//         [ something to PASTE in the clipboard
			//
			XFE_EditorView *html = (XFE_EditorView*) m_htmlViewAlias;
			XFE_Command*    handler = html->getCommand(xfeCmdPaste);
			  
			if (handler != NULL) {
				return handler->isEnabled(html, info);
			}
			else {
				return False;
			}
		}
	}
	else if ((command == xfeCmdSelectAll)) {

		//    If we are a PlainText editor, pass onto TextEditorView..
		if (m_plaintextP) {
			XFE_TextEditorView* tev =
				(XFE_TextEditorView*)m_textViewAlias;
			
			return tev->isCommandEnabled(command, calldata, info);
		}
		
		//    HTML.. SelectAll only works for the main work area.
		else if (!m_plaintextP && (m_focusW == NULL)) {
			
			XFE_EditorView* hev = (XFE_EditorView*)m_htmlViewAlias;
			XFE_Command*    handler = hev->getCommand(command);
			
			if (handler != NULL)
				return handler->isEnabled(hev, info);
		}

		// ignore any other.
		return False;
	}
	else if ( m_plaintextP && 
			  ((XFE_TextEditorView*)m_textViewAlias)->isCommandEnabled(command,calldata,info))
		{
			return True;
		}
	//    All other commands we pass to FolderView.
	else if (m_folderViewAlias->isCommandEnabled(command, calldata, info))
		return True;
	else 
		return XFE_MNView::isCommandEnabled(command, calldata, info);
}

Boolean
XFE_ComposeView::isCommandSelected(CommandType command, void * calldata, 
								   XFE_CommandInfo* info)
{
	if ( command == xfeCmdSetPriorityHighest)
		return	((XFE_ComposeFolderView*)m_folderViewAlias)
			->isPrioritySelected(MSG_HighestPriority);

	else if ( command == xfeCmdSetPriorityHigh)
		return ((XFE_ComposeFolderView*)m_folderViewAlias)
			->isPrioritySelected(MSG_HighPriority);

	else if ( command == xfeCmdSetPriorityNormal)
		return ((XFE_ComposeFolderView*)m_folderViewAlias)
			->isPrioritySelected(MSG_NormalPriority);

	else if ( command == xfeCmdSetPriorityLow)
		return ((XFE_ComposeFolderView*)m_folderViewAlias)
			->isPrioritySelected(MSG_LowPriority);

	else if ( command == xfeCmdSetPriorityLowest)
		return ((XFE_ComposeFolderView*)m_folderViewAlias)
			->isPrioritySelected(MSG_LowestPriority);

	else if ( command == xfeCmdAttachAddressBookCard)
            {
                XDEBUG(printf("XFE_ComposeView::isCommandEnabled(xfeCmdAttachAddressBookCard)\n"));
                return MSG_GetCompBoolHeader(getPane(),MSG_ATTACH_VCARD_BOOL_HEADER_MASK);
            }

	else if ( command == xfeCmdWrapLongLines)
 	{
		return CONTEXT_DATA (m_contextData)->compose_wrap_lines_p;
	}
	else 
		return XFE_MNView::isCommandSelected(command, calldata, info);
}


Boolean
XFE_ComposeView::handlesCommand(CommandType command, void* calldata, XFE_CommandInfo* info)
{
XDEBUG(	printf ("in XFE_ComposeView::handlesCommand(%s)\n", Command::getString(command));)

  if ((command == xfeCmdWrapLongLines && !m_plaintextP)
	  )
	  return False;
  if ( (command == xfeCmdNewMessage )
	|| (command == xfeCmdSaveDraft)
	|| (command == xfeCmdAddresseePicker)
	|| (command == xfeCmdQuoteOriginalText)
	|| (command == xfeCmdQuote)
	|| (command == xfeCmdAttach)
	|| (command == xfeCmdAttachAddressBookCard)
	|| (command == xfeCmdEditPreferences)
  	|| (command == xfeCmdSendMessageNow) 
        || (command == xfeCmdSendMessageLater)
	|| (command == xfeCmdWrapLongLines)
  	|| (command == xfeCmdUndo) 
  	|| (command == xfeCmdCut) 
  	|| (command == xfeCmdCopy) 
  	|| (command == xfeCmdPaste) 
	|| (command == xfeCmdDelete)
  	|| (command == xfeCmdPasteAsQuoted) 
  	|| (command == xfeCmdDeleteItem) 
  	|| (command == xfeCmdSearchAddress) 
  	|| (command == xfeCmdSetPriorityHighest)
	|| (command == xfeCmdSetPriorityHigh)
	|| (command == xfeCmdSetPriorityNormal)
	|| (command == xfeCmdSetPriorityLow)
	|| (command == xfeCmdSetPriorityLowest)
    || (command == xfeCmdToggleAddressArea)
    || (command == xfeCmdToggleFormatToolbar) )
     return True;
  else if ( m_plaintextP && 
        ((XFE_TextEditorView*)m_textViewAlias)->handlesCommand(command,calldata,info))
  {
        return True;
  }
  else if (m_folderViewAlias->handlesCommand(command, calldata, info))
     return True;
  else 
	return XFE_MNView::handlesCommand(command, calldata, info);
}

char*
XFE_ComposeView::commandToString(CommandType cmd,
								 void* calldata, XFE_CommandInfo* info)
{
	if (cmd == xfeCmdToggleFormatToolbar) {

		Widget widget = NULL;
		Boolean show = !m_format_toolbar->isShown();
		
		if (info != NULL)
			widget = info->widget;

		return getShowHideLabelString(cmd, show, widget);

	} else if (cmd == xfeCmdToggleAddressArea) {

		Widget widget = NULL;
		
		if (info != NULL)
			widget = info->widget;

		return getShowHideLabelString(cmd, !m_expanded, widget);

	} else 
	
	return XFE_MNView::commandToString(cmd, calldata, info);
}


XFE_Command*
XFE_ComposeView::getCommand(CommandType cmd)
{
	if (m_plaintextP) {
		return NULL;
	} else {
		if ( cmd == xfeCmdCut   ||
			 cmd == xfeCmdCopy  ||
			 cmd == xfeCmdPaste ||
			 cmd == xfeCmdDeleteItem ||
			 cmd == xfeCmdSelectAll ||
			 cmd == xfeCmdUndo ) {
			return NULL;
		}
		else
			return ((XFE_EditorView*)m_htmlViewAlias)->getCommand(cmd);
	}
}

XP_Bool
XFE_ComposeView::continueAfterSanityCheck()
{
  int errcode = 0;
  XP_Bool cont = True;

  errcode = MSG_SanityCheck(getPane(),errcode);

  XDEBUG( printf("Send Now error code is %d\n", errcode);)
  if ( errcode == MK_MSG_EMPTY_MESSAGE )
  {
        cont = XFE_Confirm(m_contextData,XP_GetString(errcode));
  }
  else if ( errcode == MK_MSG_DOUBLE_INCLUDE) 
  {
        cont = XFE_Confirm(m_contextData,XP_GetString(errcode));
  }
  else if ( errcode == MK_MSG_MIXED_SECURITY ) 
  {
        cont = XFE_Confirm(m_contextData,XP_GetString(errcode));
  }
  else if ( errcode == MK_MSG_MISSING_SUBJECT ) 
  {
        const char *def = XP_GetString(XFE_NO_SUBJECT);
        char *subject = (char *) fe_dialog (CONTEXT_WIDGET(m_contextData),
                             "promptSubject",
                             0, TRUE, def, TRUE, TRUE, 0);

        if ( subject && strlen(subject) )
        {
            fe_SetTextFieldAndCallBack(m_subjectW, subject);

            MSG_SetCompHeader(getPane(),MSG_SUBJECT_HEADER_MASK,subject);
            XtFree(subject);
        }
        else if ( !subject )
        {
	  cont = False;
        }
   }
   return cont;
}

void
XFE_ComposeView::doCommand(CommandType command, void * calldata, XFE_CommandInfo* info)
{
  char *pBody;
  uint32 bodySize;
  MSG_FontCode *font_changes;
  int errcode = 0;
  Widget destW= XmGetDestination(XtDisplay(getBaseWidget()));
  Widget focusW= XmGetFocusWidget(getBaseWidget());

XDEBUG(	printf ("in XFE_ComposeView::doCommand()\n");)
XDEBUG(	printf ("Do Command: %s \n", Command::getString(command));)

  if (command == xfeCmdSendMessageNow)
    {
      XDEBUG(	printf ("XFE_ComposeView::sendMessageNow()\n");)

      updateHeaderInfo ( );

      MSG_SetHTMLMarkup(getPane(), !m_plaintextP);

      FE_GetMessageBody(getPane(), &pBody, &bodySize, &font_changes);
      //if ( bodySize)
        MSG_SetCompBody(getPane(), pBody );
 
      XP_FREE(pBody);
 
      if ( !continueAfterSanityCheck()) return;

      MSG_CommandType msg_cmd = MSG_SendMessage;

#if 0
      // For now, this flag was always set to true which is incorrect
      if( fe_globalPrefs.queue_for_later_p )
             msg_cmd = MSG_SendMessageLater;
#endif

      /* ###tw  Should still probably do the commandstatus stuff. */
      MSG_Command(getPane(), msg_cmd, NULL, 0);
    }
  else if (command == xfeCmdSendMessageLater)
    {
      XDEBUG(	printf ("XFE_ComposeView::sendMessageLater()\n");)

      updateHeaderInfo ( );

      FE_GetMessageBody(getPane(), &pBody, &bodySize, &font_changes);
      //if ( bodySize)
        MSG_SetCompBody(getPane(), pBody );
 
      XP_FREE(pBody);
 
      while ((errcode = MSG_SanityCheck(getPane(),errcode))!=0)
      {
        XDEBUG( printf("Send Later error code is %d\n", errcode);)
        if (!XFE_Confirm(m_contextData,XP_GetString(errcode)))
            return;
      }
      // Mark this to be true so that when ComposeFame deletion
      // happened, it will delete the ComposeFrame accordingly
      m_delayedSent = True;
      /* ###tw  Should still probably do the commandstatus stuff. */
      MSG_Command(getPane(), MSG_SendMessageLater, NULL, 0);
    }
  else if (command == xfeCmdSaveDraft)
  {
      XDEBUG(   printf ("XFE_ComposeView::saveAsDraft()\n");)
 
      updateHeaderInfo ( );
 
      FE_GetMessageBody(getPane(), &pBody, &bodySize, &font_changes);
      //if ( bodySize)
        MSG_SetCompBody(getPane(), pBody );

      XP_FREE(pBody);
 
      /* ###tw  Should still probably do the commandstatus stuff. */
    
      MSG_Command(getPane(), MSG_SaveDraft, NULL, 0);

     // Reset these two flags so that if user close the window, they
     // will not be prompt to save again
     CONTEXT_DATA(m_contextData)->mcCitedAndUnedited = True;
     CONTEXT_DATA(m_contextData)->mcEdited = False;
  }
  else if (command == xfeCmdEditPreferences)
  {
      fe_showMailNewsPreferences(getToplevel(), (MWContext*)m_contextData);
  }
  else if (command == xfeCmdQuoteOriginalText||
	   command == xfeCmdQuote)
  {
	  if (m_plaintextP) {
		  Widget textW = CONTEXT_DATA(m_contextData)->mcBodyText;

		  XtSetSensitive(textW, False);
		  MSG_QuoteMessage (getPane(), &XFE_ComposeView::doQuoteMessage, this);
		  XtSetSensitive(textW, True);
	  }
	  else {
#ifdef DEBUG_rhess
		  if (command == xfeCmdQuote)
			  fprintf(stderr, "xfeCmdQuote\n");
		  else
			  fprintf(stderr, "xfeCmdQuoteOriginalText\n");
#endif
		  MSG_SetHTMLMarkup(getPane(), TRUE);
		  MSG_QuoteMessage (getPane(), &XFE_ComposeView::doQuoteMessage, this);
	  }
  }
  else if ( command == xfeCmdUndo )
  {
	  if (!m_plaintextP) {
		  XFE_EditorView* hev = (XFE_EditorView*)m_htmlViewAlias;
		  XFE_Command*    handler = hev->getCommand(command);
	  
		  if (handler != NULL) {
			  handler->doCommand(hev, info);
		  }
	  }
  }
  else if ( command == xfeCmdCut )
  {
	  if (m_plaintextP) {
		  fe_cut_cb(CONTEXT_WIDGET (m_contextData),
					(XtPointer) m_contextData, NULL);
	  }
	  else {
		  if (m_focusW == NULL) {
			  // NOTE:  pass this command to the EditorView...
			  //
#ifdef DEBUG_rhess
			  fprintf(stderr, "xfeCmdCut::[ HTML ]\n");
#endif
			  XFE_EditorView* hev = (XFE_EditorView*)m_htmlViewAlias;
			  XFE_Command*    handler = hev->getCommand(command);

			  if (handler != NULL) {
				  handler->doCommand(hev, info);
			  }
		  }
		  else {
#ifdef DEBUG_rhess
			  fprintf(stderr, "xfeCmdCut::[ plain ][ %p ]\n", focusW);
#endif
			  m_focusW = focusW;

			  fe_cut_cb(CONTEXT_WIDGET (m_contextData),
						(XtPointer) m_contextData, NULL);
		  }
	  }
  }
  else if ( command == xfeCmdCopy )
  {
	  if (m_plaintextP) {
		  fe_copy_cb(CONTEXT_WIDGET (m_contextData),
					 (XtPointer) m_contextData, NULL);
	  }
	  else {
		  if (m_focusW == NULL) {
			  // NOTE:  pass this command to the EditorView...
			  //
#ifdef DEBUG_rhess
			  fprintf(stderr, "xfeCmdCopy::[ HTML ]\n");
#endif
			  XFE_EditorView* hev = (XFE_EditorView*)m_htmlViewAlias;
			  XFE_Command*    handler = hev->getCommand(command);

			  if (handler != NULL) {
				  handler->doCommand(hev, info);
			  }
		  }
		  else {
#ifdef DEBUG_rhess
			  fprintf(stderr, "xfeCmdCopy::[ plain ][ %p ]\n", focusW);
#endif
			  m_focusW = focusW;

			  fe_copy_cb(CONTEXT_WIDGET (m_contextData),
						 (XtPointer) m_contextData, NULL);
		  }
	  }
  }
  else if ( command == xfeCmdPaste )
  {
	  if (m_plaintextP) {
		  fe_paste_cb(CONTEXT_WIDGET (m_contextData),
					  (XtPointer) m_contextData, NULL);
	  }
	  else {
		  if ((m_focusW == NULL) || (info->widget == m_daW)) {
			  Boolean handled = handleStealthPasteQuote(command, info);

			  if (!handled) {
				  // NOTE:  pass this command to the EditorView...
				  //
#ifdef DEBUG_rhess
				  fprintf(stderr, "xfeCmdPaste::[ HTML ]\n");
#endif
				  XFE_EditorView* hev = (XFE_EditorView*)m_htmlViewAlias;
				  XFE_Command*    handler = hev->getCommand(command);

				  if (handler != NULL) {
					  handler->doCommand(hev, info);
				  }
			  }
		  }
		  else {
#ifdef DEBUG_rhess
			  fprintf(stderr, "xfeCmdPaste::[ plain ][ %p ]\n", focusW);
#endif
			  m_focusW = focusW;

			  fe_paste_cb(CONTEXT_WIDGET (m_contextData),
						  (XtPointer) m_contextData, NULL);
		  }
	  }
  }
  else if ( command == xfeCmdPasteAsQuoted )
  {
	  if (m_plaintextP) {
		  // This command will always make the textWidget to be the focus, 
		  // per UI spec
		  Widget textW = CONTEXT_DATA(m_contextData)->mcBodyText;

		  XmProcessTraversal(textW, XmTRAVERSE_CURRENT);

		  fe_paste_quoted_cb(CONTEXT_WIDGET (m_contextData),
							 (XtPointer) m_contextData, NULL);
	  }
	  else {
		  Boolean handled = handleStealthPasteQuote(command, info);

		  if (!handled) {
			  int32   internal  = 0;
			  char*   clip_data = NULL;

			  clip_data = xfe2_GetClipboardText(focusW, &internal);

			  if (clip_data) {
				  MSG_PastePlaintextQuotation(getPane(), clip_data);
				  XP_FREE(clip_data);
			  }
		  }
	  }
  }
  else if ( command == xfeCmdSetPriorityHighest )
  {
	// Set Priority
	((XFE_ComposeFolderView*)m_folderViewAlias)
			->selectPriority(MSG_HighestPriority);
  }
  else if ( command == xfeCmdSetPriorityHigh )
  {
	// Set Priority
	((XFE_ComposeFolderView*)m_folderViewAlias)
		->selectPriority(MSG_HighPriority);
  }
  else if ( command == xfeCmdSetPriorityNormal )
  {
	// Set Priority
	((XFE_ComposeFolderView*)m_folderViewAlias)
		->selectPriority(MSG_NormalPriority);
  }
  else if ( command == xfeCmdSetPriorityLow )
  {
	// Set Priority
	((XFE_ComposeFolderView*)m_folderViewAlias)
		->selectPriority(MSG_LowPriority);
  }
  else if ( command == xfeCmdSetPriorityLowest )
  {
	// Set Priority
	((XFE_ComposeFolderView*)m_folderViewAlias)
		->selectPriority(MSG_LowestPriority);
  }
  else if ( command == xfeCmdWrapLongLines )
  {
        XP_Bool state = CONTEXT_DATA (m_contextData)->compose_wrap_lines_p;
	setComposeWrapState(!state);	
  }
  else if ( command == xfeCmdAttachAddressBookCard )
  {
      // toggle compose header flag
      XP_Bool state=MSG_GetCompBoolHeader(getPane(),MSG_ATTACH_VCARD_BOOL_HEADER_MASK);
      MSG_SetCompBoolHeader(getPane(),MSG_ATTACH_VCARD_BOOL_HEADER_MASK, state ? FALSE : TRUE);
  }
  else if ( command == xfeCmdViewAddresses ||
	command == xfeCmdAttachFile ||
	command == xfeCmdAttachWebPage ||
	command == xfeCmdDeleteAttachment ||
	command == xfeCmdViewOptions ||
	command == xfeCmdViewAttachments ||
	command == xfeCmdAddresseePicker	)
  {
	if ( m_folderViewAlias )
        m_folderViewAlias->doCommand(command, calldata, info);
  }
  else if (command == xfeCmdToggleFormatToolbar)
  {
	m_format_toolbar->toggle();
  }
  else if (command == xfeCmdToggleAddressArea)
  {
	  toggleAddressArea();
  }
  else if (command == xfeCmdViewSecurity ) 
  {
           updateHeaderInfo();
           fe_sec_logo_cb(NULL, getContext(), NULL);
  }
  else if (command == xfeCmdDeleteItem) {
	  if ( m_focusW ) {
		  if ( m_focusW != m_addrTypeW ) {
			  if ((m_focusW == m_subjectW) || 
				  (m_focusW == m_addrTextW)) {
				  // NOTE:  Xt handles the simple text widgets...
				  //
				  XEvent *event = info->event;
				  XtCallActionProc(m_focusW, 
								   "delete-next-character", 
								   event, NULL, 0 );
			  }
			  else {
				  if (m_plaintextP) {
					  // NOTE:  textEditorView handles this one...
					  //
					  XFE_TextEditorView* tev =
						  (XFE_TextEditorView*)m_textViewAlias;

					  tev->doCommand(xfeCmdDelete, calldata, info);
				  }
				  else {
					  // NOTE:  we shouldn't ever get here...
					  //
				  }
			  }
		  }
	  }
	  else {
		  XFE_EditorView* hev = (XFE_EditorView*)m_htmlViewAlias;
		  XFE_Command*    handler = hev->getCommand(command);

		  if (handler != NULL) {
			  handler->doCommand(hev, info);
		  }
	  }
  }
  else if ((command == xfeCmdSelectAll)) {
	  //    If we are a PlainText editor, pass onto TextEditorView..
	  if (m_plaintextP) {
		  XFE_TextEditorView* tev =
			  (XFE_TextEditorView*)m_textViewAlias;
		  
		  tev->doCommand(command, calldata,	info);
	  }

	  //    HTML.. SelectAll only works for the main work area.
	  else if (!m_plaintextP && m_focusW == NULL) {
		  
		  XFE_EditorView* hev = (XFE_EditorView*)m_htmlViewAlias;
		  XFE_Command*    handler = hev->getCommand(command);
		  
		  if (handler != NULL)
			  handler->doCommand(hev, info);

	  } // ignore any other.
  }
  else if ( (m_plaintextP ) && 
	((XFE_TextEditorView*)m_textViewAlias)->isCommandEnabled(command,calldata,info))

      ((XFE_TextEditorView*)m_textViewAlias)->doCommand(command,calldata,info);
  else if ( (m_folderViewAlias ) && 
	((XFE_ComposeFolderView*)m_folderViewAlias)->isCommandEnabled(command,calldata,info))

      ((XFE_ComposeFolderView*)m_folderViewAlias)->doCommand(command,calldata,info);
  else
  {
	XFE_MNView::doCommand(command, calldata, info);
  }
}

XP_Bool
XFE_ComposeView::handleStealthPasteQuote(CommandType cmd, XFE_CommandInfo* info)
{
	Widget   base = getBaseWidget();
	Display *dpy  = XtDisplay(base);
	Window   rat  = XGetSelectionOwner(dpy, XA_PRIMARY);
	XP_Bool  stealth = False;

	// NOTE... [ special handling required to support both menu and mouse
	//         [ click enabling of this feature...
	//
	if (rat != None) {
		if (info && 
			info->event &&
			(((info->event->type == ButtonRelease) && 
			  (cmd == xfeCmdPasteAsQuoted)) ||
			 ((info->event->type == ButtonPress) && 
			  (cmd == xfeCmdPaste))
			) &&
			(info->event->xkey.state & ShiftMask)) {
			stealth = True;
		}
	}

	if (stealth) {
		Time timestamp = info->event->xbutton.time;
		
		// NOTE... [ if it's triggered off a mouse click (Btn2), 
		//         [ set the location before you paste...
		//
		if ((info->event->type == ButtonPress) &&
			(cmd == xfeCmdPaste)
			) {
			fe_EditorGrabFocus(m_contextData, info->event);
		}

		/*
		 *  NOTE:  only works if we are talking to one server... [ oops ]
		 */
		if (XFE_TEXT_ATOM == 0)
			XFE_TEXT_ATOM = XmInternAtom(dpy, "STRING", False);
		
		/*
		 *  NOTE:  only paste text from primary selection...
		 */
		XtGetSelectionValue(base, 
							XA_PRIMARY, XFE_TEXT_ATOM, 
							quotePrimarySel_cb, 
							(XtPointer) this, 
							timestamp);
	}

	return stealth;
}

void*
XFE_ComposeView::getComposerData()
{
	return mailComposer;
}

void
XFE_ComposeView::setComposerData(void* data)
{
	mailComposer= data;
}

void*
XFE_ComposeView::getAttachmentData()
{
	return mailAttachment;
}

void
XFE_ComposeView::setAttachmentData(void* data)
{
	mailAttachment= data;
}



extern "C" void 
fe_MailComposeDocumentLoaded(MWContext* context)
{
	XtPointer     client = NULL;
	XFE_ComposeView *obj = NULL;
	Widget            da = NULL;
 
#ifdef DEBUG_rhess2
	fprintf(stderr, "ComposeView::[ fe_MailComposeGetURLExitRoutine ]\n");
#endif

	da = CONTEXT_DATA(context)->drawing_area;
	if (da) {
		XtVaGetValues(da, XmNuserData, &client, 0);
 
		obj = (XFE_ComposeView *) client;
		obj->initEditorContents();
	}
}

extern "C" void *
fe_compose_getData(MWContext *context)
{
  XFE_Frame *f = ViewGlue_getFrame(context);

  XP_ASSERT(f->getType() == FRAME_MAILNEWS_COMPOSE);
 
  return ((XFE_ComposeView *)f->getView())->getComposerData();
}
 
extern "C" void
fe_compose_setData(MWContext *context, void* data)
{
  XFE_Frame *f = ViewGlue_getFrame(context);
  
  XP_ASSERT(f->getType() == FRAME_MAILNEWS_COMPOSE);
  
  ((XFE_ComposeView *)f->getView())->setComposerData(data);
}
 
extern "C" void *
fe_compose_getAttachment(MWContext *context)
{
  XFE_Frame *f = ViewGlue_getFrame(context);
  
  XP_ASSERT(f->getType() == FRAME_MAILNEWS_COMPOSE);
  
  return ((XFE_ComposeView *)f->getView())->getAttachmentData();
}
 
extern "C" void
fe_compose_setAttachment(MWContext *context, void* data)
{
  XFE_Frame *f = ViewGlue_getFrame(context);
  
  XP_ASSERT(f->getType() == FRAME_MAILNEWS_COMPOSE);
  
  ((XFE_ComposeView *)f->getView())->setAttachmentData(data);
}


// ------------ NEW CODE ----------------------

int
XFE_ComposeView::quoteMessage(const char *textData)
{
  if (textData != NULL)
  {
#ifdef DEBUG_rhess
	  fprintf(stderr, "quoteMessage\n");
#endif
	  insertMessageCompositionText( textData, False);
  }
  return 0;
}

int
XFE_ComposeView::doQuoteMessage(void *closure, const char *textData)
{
 XFE_ComposeView *obj = (XFE_ComposeView *)closure;

 XP_ASSERT(obj != NULL);

#ifdef DEBUG_rhess
   fprintf(stderr, "doQuoteMessage\n");
#endif

 // Requires to return negative value on error
 return obj->quoteMessage(textData);
}

void
XFE_ComposeView::displayDefaultTextBody()
{
     MSG_Pane *pane = getPane(); // Get it from MNView
	 const char *pBody = MSG_GetCompBody(pane);

	 if ( m_pInitialText && XP_STRLEN(m_pInitialText) ) {
		 insertMessageCompositionText( m_pInitialText, False);

		 if (m_plaintextP)
			 insertMessageCompositionText( "\n", False);

		 XP_FREEIF(m_pInitialText);

		 m_dontQuoteP = True;
#ifdef DEBUG_rhess
		 fprintf(stderr, "  m_dontQuoteP... [ True ]\n");
#endif
	 }
	 else {
		 if ( pBody && strlen(pBody) ) {
			 insertMessageCompositionText( pBody, False);

			 m_dontQuoteP = False;
#ifdef DEBUG_rhess
			 fprintf(stderr, "  m_dontQuoteP... [ False ]\n");
#endif
		 }
		 else {
			 if (m_plaintextP) {
				 // NOTE:  skip over this for plaintext...
				 //
			 }
			 else {
				 // NOTE:  we need this to handle the special case where there 
				 //        isn't any default text for the message...
				 //
#ifdef DEBUG_rhess
				 fprintf(stderr, "displayDefaultTextBody::[ NULL ]\n");
#endif
				 xfe2_EditorInit(m_contextData);
			 }
		 }
	 }
}

// Inserting initial addressing field value
void
XFE_ComposeView::initialize()
{
	MSG_Pane *pane = getPane(); // Get it from MNView
	Widget textW = NULL;

	if (m_plaintextP) {
		XmTextPosition cPos = 0;

		textW = CONTEXT_DATA(m_contextData)->mcBodyText;

		XtSetSensitive(textW, False);

		MSG_SetHTMLMarkup(pane, !m_plaintextP);

		if ( m_pInitialText && XP_STRLEN(m_pInitialText) ) {
			m_dontQuoteP = True;
		}
		else {
			m_dontQuoteP = False;
		}

		if ( !m_dontQuoteP && MSG_ShouldAutoQuote (pane) ) {
			MSG_QuoteMessage (pane, &XFE_ComposeView::doQuoteMessage, this);
		}

		cPos = XmTextGetLastPosition(textW);

		XmTextSetCursorPosition(textW, cPos);

		displayDefaultTextBody();
		 
		XmTextSetCursorPosition(textW, cPos);

		XtSetSensitive(textW, True);
	}
	else {
		// NOTE: do all this work in XFE_ComposeView::initEditor()...
		//
	}
}

XP_Bool
XFE_ComposeView::hasFocus()
{
	return m_editorFocusP;
}

void 
XFE_ComposeView::daFocusProc(Widget,XtPointer cd,XEvent *event,Boolean*)
{
	// NOTE: event handler on the drawing area...
	//
	XFE_ComposeView *obj = (XFE_ComposeView*)cd;

#ifdef DEBUG_rhess_NOT
	if (!event->xfocus.send_event)
		{
			fprintf(stderr, "++---------------------------------[ synth ]\n");
		}
#endif

	if (event->type == FocusIn)
		{
			obj->focusIn();
		}
	else
		{
			obj->focusOut();
		}
}

void 
XFE_ComposeView::topFocusProc(Widget,XtPointer cd,XEvent *event,Boolean*)
{
	// NOTE: event handler on the toplevel shell...
	//
	XFE_ComposeView *obj = (XFE_ComposeView*)cd;

	if (event->type == FocusIn)
		{
			// NOTE:  we don't need to do anything here since the 
			//        nullFocusProc event handler on m_nullW will
			//        auto-magically set the focus to the editor...
			//
		}
	else
		{
			obj->saveFocus();
		}
}

void 
XFE_ComposeView::nullFocusProc(Widget,XtPointer cd,XEvent *event,Boolean*)
{
	// NOTE: event handler on the "null" widget...
	//
	XFE_ComposeView *obj = (XFE_ComposeView*)cd;

	if (event->type == FocusIn)
		{
#ifdef DEBUG_rhess2
			fprintf(stderr, "null::[ FocusIn ]\n");
#endif
			obj->focusIn();
		}
	else 
		{
#ifdef DEBUG_rhess2	
			fprintf(stderr, "null::[ FocusOut ]\n");
#endif
		}
}

void
XFE_ComposeView::saveFocus()
{
	Widget who = XmGetFocusWidget(m_topW);

	if (who) {
		// NOTE:  don't mess with m_focusW unless we've lost focus 
		//        in the editor...
		if (who == m_nullW) {
			m_focusW = NULL;
		}
	}
}

void
XFE_ComposeView::focusIn()
{
	if (m_plaintextP) {
		Widget textW = CONTEXT_DATA(m_contextData)->mcBodyText;
		XmProcessTraversal(textW, XmTRAVERSE_CURRENT);

		m_focusW = textW;
	}
	else {
		if (!m_editorFocusP) {
			m_editorFocusP = True;
			
			XtVaSetValues (XtParent(m_daW), XmNborderColor,
						   CONTEXT_DATA (m_contextData)->default_fg_pixel,
						   0);
				
			xfe2_EditorCaretShow(m_contextData);
			
			XmProcessTraversal(m_topW, XmTRAVERSE_CURRENT);
			
			// NOTE:  if there's any "magic" in this whole focus mechanism,
			//        it's right here...
			//
			Widget who = XmGetFocusWidget(m_topW);

			if (m_nullW) {
				if (m_nullW != who) {
#ifdef DEBUG_rhess
					fprintf(stderr, "yikes... [ new m_nullW value ]\n");
#endif
					XtRemoveEventHandler(m_nullW,  FocusChangeMask, 
										 FALSE, nullFocusProc, this);
					XtInsertEventHandler(who,  FocusChangeMask, FALSE, 
										 nullFocusProc, this, XtListTail);
				}
			}
			else {
				XtInsertEventHandler(who,  FocusChangeMask, FALSE, 
									 nullFocusProc, this, XtListTail);
			}

			m_nullW  = who;
			m_focusW = NULL;
			
			if (fe_globalData.editor_im_input_enabled) {
				xfe2_EditorImSetCoords(m_contextData);
			}

			XtSetKeyboardFocus(m_topW, m_daW);
		}
	}
}

void
XFE_ComposeView::focusOut()
{
	Widget who = XmGetFocusWidget(m_topW);

	if (m_plaintextP) {
		// YOH:  any idea if we need to do something here?...
		//
	}
	else {
		if (m_editorFocusP) {
			m_editorFocusP = False;

			xfe2_EditorCaretHide(m_contextData);
			
			if (fe_globalData.editor_im_input_enabled) {
				XmImUnsetFocus(m_daW);
			}

			XtSetKeyboardFocus(m_topW, None);

			XtVaSetValues (XtParent(m_daW), XmNborderColor,
						   CONTEXT_DATA (m_contextData)->default_bg_pixel,
						   0);
			
			// XFlush(XtDisplay(m_subjectW));
			XmProcessTraversal(m_topW, XmTRAVERSE_CURRENT);
			XmProcessTraversal(who, XmTRAVERSE_CURRENT);
		}
	}
}

void
XFE_ComposeView::updateCompToolbar()
{
    char*            address;
    URL_Struct*      n_url;

	if (m_delayEditorP) {
		m_delayEditorP = False;

#ifdef DEBUG_rhess
			fprintf(stderr, "initEditor... [ about:editfilenew ]\n");
#endif

		// NOTE:  create a blank address...
		address = "about:editfilenew";
		n_url = NET_CreateURLStruct(address, NET_DONT_RELOAD);

		((XFE_EditorView*)m_htmlViewAlias)->getURL(n_url, (n_url == NULL));
	}
}

int
XFE_ComposeView::initEditor()
{
    char*            address;
    URL_Struct*      n_url;
	int              rtn = 0;

	if ( m_plaintextP ) {
	}
	else {
		if (XP_IsContextBusy(m_contextData)) {
#ifdef DEBUG_rhess
			fprintf(stderr, "initEditor::[ delay ]\n");
#endif
			m_delayEditorP = True;
		}
		else {
#ifdef DEBUG_rhess
			fprintf(stderr, "initEditor\n");
#endif
			m_delayEditorP = False;

			// NOTE:  create a blank address...
			address = "about:editfilenew";
			n_url = NET_CreateURLStruct(address, NET_DONT_RELOAD);

			rtn = ((XFE_EditorView*)m_htmlViewAlias)->getURL(n_url, 
															 (n_url == NULL));
		}
	}

    return (rtn);
}


void
XFE_ComposeView::initEditorContents()
{
	MSG_Pane *pane = getPane(); // Get it from MNView

	if (m_puntEditorP) {
#ifdef DEBUG_rhess2
		fprintf(stderr, "initEditorContents::[ PUNT ]\n");
#endif
	}
	else {
		if (!m_initEditorP) {
			m_initEditorP = True;

			MSG_SetHTMLMarkup(pane, !m_plaintextP);

			// EDT_ReturnKey(m_contextData);

			// EDT_SelectAll(m_contextData);
			// EDT_ClearSelection(m_contextData);

			if ( m_pInitialText && XP_STRLEN(m_pInitialText) ) {
				m_dontQuoteP = True;
			}
			else {
				m_dontQuoteP = False;
			}

			EDT_BeginOfDocument(m_contextData, False);

			if ( !m_dontQuoteP && MSG_ShouldAutoQuote (pane) ) {
#ifdef DEBUG_rhess
				fprintf(stderr, "initEditorContents::[ quote ]\n");
#endif
				MSG_QuoteMessage (pane, 
								  &XFE_ComposeView::doQuoteMessage, this);
			}
			else {
#ifdef DEBUG_rhess
				fprintf(stderr, "initEditorContents::[ default ]\n");
#endif
				m_postEditorP = True;

				displayDefaultTextBody();
			}
		}
		else {
			if (!m_postEditorP) {
#ifdef DEBUG_rhess
				fprintf(stderr, "initEditorContents::[ default ]\n");
#endif
				m_postEditorP = True;

				EDT_EndOfDocument(m_contextData, False);

				displayDefaultTextBody();
			}
			else {
#ifdef DEBUG_rhess
				fprintf(stderr, "initEditorContents::[ top ]\n");
#endif
				m_puntEditorP = True;
				
				EDT_BeginOfDocument(m_contextData, False);
				// NOTE:  we need to set this so that we can determine
				//        if the user has changed the document from 
				//        what we just loaded...
				//
				EDT_SetDirtyFlag(m_contextData, False);

				fe_UserActivity(m_contextData);
		
				xfe2_EditorInit(m_contextData);
			}
		}
	}
}

XFE_CALLBACK_DEFN(XFE_ComposeView,tabToEditor)(XFE_NotificationCenter*, void*, void*)
{
#ifdef DEBUG_rhess
  fprintf(stderr, "Editor::[ tabToEditor ]\n");
#endif

  focusIn();
}

void
XFE_ComposeView::insertMessageCompositionText(
		const char* text, XP_Bool leaveCursorBeginning)
{
	if (m_plaintextP) {
		((XFE_TextEditorView*)m_textViewAlias)->
			insertMessageCompositionText(text, leaveCursorBeginning);
	}
	else {
#ifdef DEBUG_rhess2
		fprintf(stderr, "++\n%s\n++\n", text);
#endif
		((XFE_EditorView*)m_htmlViewAlias)->
			insertMessageCompositionText(text, leaveCursorBeginning, 
										 m_requestHTML);
	}
}

void 
XFE_ComposeView::getMessageBody(
		char **pBody, uint32 *body_size, 
		MSG_FontCode **font_changes)
{
	if (m_plaintextP) {
		((XFE_TextEditorView*)m_textViewAlias)->
			getMessageBody(pBody, body_size, font_changes);
	}
	else {
		((XFE_EditorView*)m_htmlViewAlias)->
			getMessageBody(pBody, body_size, font_changes);
	}
}

void
XFE_ComposeView::doneWithMessageBody(char *pBody)
{
	if (m_plaintextP) {
		((XFE_TextEditorView *)m_textViewAlias)->doneWithMessageBody(pBody);
	}
	else {
		((XFE_EditorView *)m_htmlViewAlias)->doneWithMessageBody(pBody);
	}
}

void
XFE_ComposeView::setComposeWrapState(XP_Bool wrap_p)
{
	if (m_plaintextP) {
		if ( m_textViewAlias )
			((XFE_TextEditorView*)m_textViewAlias)->setComposeWrapState(wrap_p);
	}
	else {
		// punt on this one for now... [ XFE_EditorView ]
	}
}

XP_Bool
XFE_ComposeView::isDelayedSent()
{
  return m_delayedSent;
}

XP_Bool
XFE_ComposeView::isHTML()
{
  return !m_plaintextP;
}


extern "C" void 
fe_InsertMessageCompositionText(
		MWContext *context,
                const char* text, XP_Bool leaveCursorBeginning)
{

  XFE_Frame *f = ViewGlue_getFrame(context);

  XP_ASSERT(f!=NULL && f->getType() == FRAME_MAILNEWS_COMPOSE);

#ifdef DEBUG_rhess
  fprintf(stderr, "fe_InsertMessageCompositionText::[ ]\n");
#endif  

  ((XFE_ComposeView*)f->getView())->insertMessageCompositionText(
  				       text, leaveCursorBeginning);
}

extern "C" void 
fe_getMessageBody(
		MWContext *context,
		char **pBody, uint32 *body_size, 
		MSG_FontCode **font_changes)
{

  XFE_Frame *f = ViewGlue_getFrame(context);

  XP_ASSERT(f!=NULL && f->getType() == FRAME_MAILNEWS_COMPOSE);

#ifdef DEBUG_rhess
  fprintf(stderr, "fe_getMessageBody::[ ]\n");
#endif  

  ((XFE_ComposeView*)f->getView())->getMessageBody(pBody, body_size,
		font_changes);
}
extern "C" void 
fe_doneWithMessageBody(MWContext *context, char *pBody, uint32 /*body_size*/)
{

  XFE_Frame *f = ViewGlue_getFrame(context);

  XP_ASSERT(f!=NULL && f->getType() == FRAME_MAILNEWS_COMPOSE);

#ifdef DEBUG_rhess
  fprintf(stderr, "fe_doneWithMessageBody::[ ]\n");
#endif  

  ((XFE_ComposeView*)f->getView())->doneWithMessageBody(pBody);
}

extern "C" void
fe_addToContextList(MWContext *context)
{
  struct fe_MWContext_cons *cons; 

  if ( context== NULL) return;

  cons =  XP_NEW_ZAP(struct fe_MWContext_cons);

  if ( cons == NULL ) return;

  cons->context = context;
  cons->next = fe_all_MWContexts;
  fe_all_MWContexts = cons;
  XP_AddContextToList (context);
}

extern "C" void
fe_set_compose_wrap_state(MWContext *context, XP_Bool wrap_p)
{
  XFE_Frame *frame = ViewGlue_getFrame(XP_GetNonGridContext(context));

  XP_ASSERT(frame->getType() == FRAME_MAILNEWS_COMPOSE);

  ((XFE_ComposeView*)frame->getView())->setComposeWrapState(wrap_p);
}

void
XFE_ComposeView::subjectUpdate(Widget w, XtPointer)
{
  MSG_HEADER_SET msgtype = MSG_SUBJECT_HEADER_MASK;
  char* value = 0;
  char *newvalue = 0;
  MSG_Pane *comppane = (MSG_Pane*)getPane(); 

  value = fe_GetTextField(w);
  newvalue = MSG_UpdateHeaderContents(comppane, msgtype, value);
  if (newvalue) {
    fe_SetTextField(w, newvalue);
    MSG_SetCompHeader(comppane, msgtype, newvalue);
    XP_FREE(newvalue);
  }
}

void
XFE_ComposeView::subjectChange(Widget w, XtPointer)
{
  MSG_HEADER_SET msgtype = MSG_SUBJECT_HEADER_MASK;
  char* value = 0;
  MSG_Pane *comppane = (MSG_Pane*)getPane(); 

  value = fe_GetTextField(w);
  MSG_SetCompHeader (comppane, msgtype, value);
}


void
XFE_ComposeView::subjectChangeCallback(Widget w, XtPointer clientData, XtPointer callData)
{
  XFE_ComposeView *obj = (XFE_ComposeView*)clientData;
 
  obj->subjectChange(w, callData);
}

void
XFE_ComposeView::textEditorFocus(Widget who)
{
#ifdef DEBUG_rhess2
	fprintf(stderr, "textEditor::[ %p ]\n", who);
#endif
	m_focusW    = who;
}

void
XFE_ComposeView::addressTextFocus(Widget who)
{
	unGrabEditor();

#ifdef DEBUG_rhess2
	fprintf(stderr, "addressText::[ %p ]\n", who);
#endif
	m_addrTextW = who;
	m_focusW    = who;
}

void
XFE_ComposeView::addressTypeFocus(Widget who)
{
	unGrabEditor();

#ifdef DEBUG_rhess2
	fprintf(stderr, "addressType::[ %p ]\n", who);
#endif
	m_addrTypeW = who;
	m_focusW    = who;
}

void
XFE_ComposeView::subjectFocus()
{
	unGrabEditor();

	m_focusW = m_subjectW;
}

void
XFE_ComposeView::quoteText(char * value)
{
	if (value) {
		MSG_PastePlaintextQuotation(getPane(), value);
	}
}

void
XFE_ComposeView::quotePrimarySel_cb(Widget, XtPointer cdata, 
									Atom*, Atom *type, XtPointer value, 
									unsigned long*, int*)
{
	XFE_ComposeView *obj = (XFE_ComposeView*)cdata;
 
	if (value) {
		if (*type == XFE_TEXT_ATOM) {
#ifdef DEBUG_rhess
			fprintf(stderr, "quotePrimarySel_cb::[ text ]\n");
#endif
			obj->quoteText( (char *) value );
		}
		else {
#ifdef DEBUG_rhess
			fprintf(stderr, "WARNING... [ unknown type atom ]\n");
#endif
		}
	}
#ifdef DEBUG_rhess
	else {
		fprintf(stderr, "quotePrimarySel_cb::[ NULL ]\n");
	}
#endif
}

void
XFE_ComposeView::subjectFocusIn_CB(Widget,
								   XtPointer clientData,
								   XtPointer)
{
	XFE_ComposeView *obj = (XFE_ComposeView*)clientData;
 
	obj->subjectFocus();
}

void
XFE_ComposeView::expandCallback(Widget, XtPointer clientData, XtPointer)
{
  XFE_ComposeView *obj = (XFE_ComposeView*)clientData;
 
  obj->toggleAddressArea();
}


void
XFE_ComposeView::toggleAddressArea()
{
	Widget widget = m_expand_arrowW;

    if (m_expanded)
    { // Current, it is expanded. we need to collapse it
      XtVaSetValues(widget, XmNarrowDirection, XmARROW_DOWN, 0);
      m_folderViewAlias->hide();
      m_expanded = False;
    }
    else  // Expand it
    {
      XtVaSetValues(widget, XmNarrowDirection, XmARROW_UP, 0);
      m_folderViewAlias->show();
      m_expanded = True;
    }
}


XFE_CALLBACK_DEFN(XFE_ComposeView, tabToSubject)(XFE_NotificationCenter*, void*, void*)
{
   XDEBUG(printf("About to tab to subject...\n");)
   unGrabEditor();
}

XFE_CALLBACK_DEFN(XFE_ComposeView, textEditorFocusIn)(XFE_NotificationCenter*, void*, void* calldata)
{
	Widget who = (Widget) calldata;

	textEditorFocus(who);
}

XFE_CALLBACK_DEFN(XFE_ComposeView, addressTextFocusIn)(XFE_NotificationCenter*, void*, void* calldata)
{
	Widget who = (Widget) calldata;

	addressTextFocus(who);
}

XFE_CALLBACK_DEFN(XFE_ComposeView, addressTypeFocusIn)(XFE_NotificationCenter*, void*, void* calldata)
{
	Widget who = (Widget) calldata;

	addressTypeFocus(who);
}

void 
XFE_ComposeView::unGrabEditorProc(Widget, XtPointer cd, XEvent*, Boolean*)
{
	XFE_ComposeView *obj = (XFE_ComposeView*)cd;

	obj->unGrabEditor();
}

void
XFE_ComposeView::unGrabEditor()
{
	if (m_editorFocusP) {
#ifdef DEBUG_rhess
		fprintf(stderr, "::[ unGrabEditor ]\n");
#endif
		focusOut();
	}
}

void
XFE_ComposeView::editorFocus(Boolean flag)
{
  m_editorFocusP = flag;
}

Boolean
XFE_ComposeView::isModified()
{
  Boolean modified = False;

  if ( m_plaintextP )
   modified = ((XFE_TextEditorView*)m_textViewAlias)->isModified();
  else 
   modified = ((XFE_EditorView*)m_htmlViewAlias)->isModified();
  return modified;
}

void
XFE_ComposeView::updateHeaderInfo(void)
{


  ((XFE_ComposeFolderView*)m_folderViewAlias)->updateHeaderInfo();
  if (m_subjectW)
  {
    char *string;

    string = fe_GetTextField(m_subjectW);

    MSG_SetCompHeader(getPane(),MSG_SUBJECT_HEADER_MASK,string);
    XtFree(string);
  }

}

XFE_View*
XFE_ComposeView::getCommandView(XFE_Command* )
{
	//
	//    We can do a much simpler (and faster) implimentation that
	//    the XFE_View version, because the only sub view that ever
	//    impliments XFE_Commands is the html editor view.
	//
	if (!m_plaintextP)
		return (XFE_TextEditorView*)m_htmlViewAlias;
	else
		return NULL;
}

extern "C" void
FE_SecurityOptionsChanged(MWContext * pContext)
{
   XFE_Frame * pFrame = ViewGlue_getFrame(XP_GetNonGridContext(pContext));
   if ( pFrame ) {
        ((XFE_ComposeView*)pFrame->getView())->getToplevel()->
		notifyInterested(XFE_ComposeView::updateSecurityOption, NULL);
   }
}

