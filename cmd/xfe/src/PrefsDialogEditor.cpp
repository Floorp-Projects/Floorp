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
   PrefsDialogEditor.cpp -- the Editor page in XFE preferences dialogs
   Created: Linda Wei <lwei@netscape.com>, 20-Nov-96.
 */

#include "felocale.h"
#include "structs.h"
#include "fonts.h"
#include "xpassert.h"
#include "xfe.h"
#include "e_kit.h"
#include "prefapi.h"
#include "PrefsDialog.h"
#include "edttypes.h"
#include "edt.h"

#include <Xm/Label.h>
#include <Xm/LabelG.h>
#include <Xm/CascadeBG.h>
#include <Xm/PushB.h>
#include <Xm/PushBG.h>
#include <Xm/ToggleB.h>
#include <Xm/ArrowBG.h>
#include <Xm/Form.h>
#include <Xm/Frame.h>
#include <Xm/RowColumn.h>
#include <Xm/LabelG.h> 
#include <Xm/TextF.h> 
#include <Xm/ToggleBG.h> 
#include <Xfe/Xfe.h>

#define SWATCH_SIZE 60

#define RANGE_CHECK(o, a, b) ((o) < (a) || (o) > (b))

#define AUTOSAVE_MIN_PERIOD 0
#define AUTOSAVE_MAX_PERIOD 600

extern int XFE_EDITOR_AUTOSAVE_PERIOD_RANGE;
extern int XFE_EDITOR_PUBLISH_LOCATION_INVALID;

#define EDITOR_GENERAL_AUTHOR          (0x1<<0)
#define EDITOR_GENERAL_HTML_EDITOR     (0x1<<1)
#define EDITOR_GENERAL_IMAGE_EDITOR    (0x1<<2)
#define EDITOR_GENERAL_TEMPLATE        (0x1<<3)
#define EDITOR_GENERAL_AUTOSAVE        (0x1<<4)

#define EDITOR_PUBLISH_LINKS           (0x1<<0)
#define EDITOR_PUBLISH_IMAGES          (0x1<<1)
#define EDITOR_PUBLISH_PUBLISH         (0x1<<2)
#define EDITOR_PUBLISH_BROWSE          (0x1<<3)
#define EDITOR_PUBLISH_USERNAME        (0x1<<4)
#define EDITOR_PUBLISH_PASSWORD        (0x1<<5)
#define EDITOR_PUBLISH_PASSWORD_SAVE   (0x1<<6)

extern char* fe_PreviewPanelCreate_names[];

extern "C"
{
	char     *XP_GetString(int i);
	Widget    fe_CreateFrame(Widget parent, char* name,  Arg* p_args, Cardinal p_n);
	char*     fe_EditorDefaultGetTemplate();
	XtPointer fe_GetUserData(Widget);
	void      fe_TextFieldSetEditable(MWContext* context, Widget widget, Boolean editable);
	void      fe_browse_file_of_text(MWContext *context, Widget text_field, Boolean dirp);
	void      fe_browse_file_of_text_in_url(MWContext *context, Widget text_field, Boolean dirp);
	void      fe_TextFieldSetString(Widget widget, char* value, Boolean notify);
	void      fe_document_appearance_use_image_button_update_cb(Widget w, XtPointer closure,
																XtPointer callData);
	void      fe_document_appearance_use_image_cb(Widget w, XtPointer closure, XtPointer callData);
	void      fe_document_appearance_use_image_update_cb(Widget w, XtPointer closure, XtPointer callData);
	void      fe_document_appearance_image_text_cb(Widget w, XtPointer closure, XtPointer callData);
	void      fe_document_appearance_image_text_update_cb(Widget w, XtPointer closure, XtPointer callData);
	void      fe_document_appearance_image_text_browse_cb(Widget w, XtPointer closure, XtPointer callData);
	void      fe_document_appearance_use_custom_cb(Widget w, XtPointer closure, XtPointer callData);
	void      fe_document_appearance_use_custom_update_cb(Widget w, XtPointer closure,
														  XtPointer callData);
	void      fe_document_appearance_sensitized_update_cb(Widget w, XtPointer closure,
														  XtPointer callData);
	void      fe_document_appearance_swatch_update_cb(Widget w, XtPointer closure,
													  XtPointer callData);
	void      fe_document_appearance_preview_update_cb(Widget w, XtPointer closure,
													   XtPointer callData);
	void      fe_document_appearance_color_cb(Widget w, XtPointer closure, XtPointer callData);
	void      fe_preview_panel_click_cb(Widget w, XtPointer closure, XtPointer callData);
	void      fe_document_appearance_init(MWContext* context,
										  fe_EditorDocumentAppearancePropertiesStruct* w_data);
	void      fe_document_appearance_set(MWContext* context,
										 fe_EditorDocumentAppearancePropertiesStruct* w_data);
	void      fe_DependentListAddDependent(fe_DependentList** list_a,
										   Widget widget, fe_Dependency mask,
										   XtCallbackProc proc, XtPointer closure);
	Widget    fe_CreateSwatch(Widget parent, char* name, Arg* p_args, Cardinal p_n);
	Widget    fe_PreviewPanelCreate(Widget parent, char* name, Arg* p_args, Cardinal p_n);
	Widget    fe_CreatePasswordField(Widget parent, char* name, Arg* args, Cardinal n);

	static void      fe_set_numeric_text_field(Widget widget, unsigned value);
	static Widget    fe_GetBiggestWidget(Boolean horizontal, Widget* widgets, Cardinal n);
	static int       fe_get_numeric_text_field(Widget widget);
	static void      fe_error_dialog(MWContext* context, Widget parent, char* s);
}

// ************************************************************************
// *************************       Editor        *************************
// ************************************************************************

// Member:       XFE_PrefsPageEditor
// Description:  Constructor
// Inputs:
// Side effects: 

XFE_PrefsPageEditor::XFE_PrefsPageEditor(XFE_PrefsDialog *dialog)
	: XFE_PrefsPage(dialog),
	  m_prefsDataEditor(0)
{
}

// Member:       ~XFE_PrefsPageEditor
// Description:  Destructor
// Inputs:
// Side effects: 

XFE_PrefsPageEditor::~XFE_PrefsPageEditor()
{
	delete m_prefsDataEditor;
}

// Member:       create
// Description:  Creates page for Editor
// Inputs:
// Side effects: 

void XFE_PrefsPageEditor::create()
{
	Arg               av[50];
	int               ac;

	PrefsDataEditor *fep = NULL;

	fep = new PrefsDataEditor;
	memset(fep, 0, sizeof(PrefsDataEditor));
	m_prefsDataEditor = fep;

	fep->context = getContext();
	fep->prompt_dialog = getPrefsDialog()->getDialogChrome();

	MWContext *context = fep->context;
	Widget     form;

	ac = 0;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	form = XmCreateForm (m_wPageForm, "editor", av, ac);
	XtManageChild (form);
	m_wPage = fep->page = form;

	Widget frame1;
	Widget form1;
	Widget author_label;
	Widget author_text;
	Widget external_frame;
	Widget external_form;
	Widget html_label;
	Widget html_text;
	Widget html_browse;
	Widget image_label;
	Widget image_text;
	Widget image_browse;
	Widget template_label;
	Widget template_text;
	Widget template_restore;
	Widget autosave_toggle;
	Widget autosave_text;
	Widget autosave_label;
	Widget template_browse;
	Widget children[16];
	Cardinal  nchildren;
	Arg       args[8];
	Cardinal  n;
	int labels_height;
	int labels_width;

	n = 0;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNbottomAttachment, XmATTACH_NONE); n++;
	frame1 = XmCreateFrame(form, "frame1", args, n);
	XtManageChild(frame1);

	n = 0;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	form1 = XmCreateForm(frame1, "form1", args, n);
	XtManageChild(form1);

	nchildren = 0;

	n = 0;
	author_label = XmCreateLabelGadget(form1, "authorLabel", args, n);
	children[nchildren++] = author_label;

	n = 0;
	XtSetArg(args[n], XmNuserData, EDITOR_GENERAL_AUTHOR); n++;
	author_text = fe_CreateTextField(form1, "authorText", args, n);
	fep->author = author_text;
	children[nchildren++] = author_text;
	XtAddCallback(author_text, XmNvalueChangedCallback,
				  cb_changed, (XtPointer)fep);


	n = 0;
	XtSetArg(args[n], XmNindicatorType, XmN_OF_MANY); n++;
	autosave_toggle = XmCreateToggleButtonGadget(form1,
				 "autosaveEnable", args, n);
	fep->autosave_toggle = autosave_toggle;
	children[nchildren++] = autosave_toggle;
	XtAddCallback(autosave_toggle, XmNvalueChangedCallback,
				  cb_autosaveToggle, (XtPointer)fep);


	n = 0;
	XtSetArg(args[n], XmNcolumns, 4); n++;
	XtSetArg(args[n], XmNuserData, EDITOR_GENERAL_AUTOSAVE); n++;
	autosave_text = fe_CreateTextField(form1, "autosaveText", args, n);
	fep->autosave_text = autosave_text;
	children[nchildren++] = autosave_text;
	XtAddCallback(autosave_text, XmNvalueChangedCallback,
				  cb_changed, (XtPointer)fep);

	n = 0;
	autosave_label = XmCreateLabelGadget(form1,	"minutes", args, n);
	children[nchildren++] = autosave_label;


	n = 0;
	template_label = XmCreateLabelGadget(form1, "locationLabel", args, n);
	children[nchildren++] = template_label;

	n = 0;
	XtSetArg(args[n], XmNuserData, EDITOR_GENERAL_TEMPLATE); n++;
	template_text = fe_CreateTextField(form1, "templateText", args, n);
	children[nchildren++] = template_text;
	fep->tmplate = template_text;
	XtAddCallback(template_text, XmNvalueChangedCallback,
				  cb_changed, (XtPointer)fep);

	n = 0;
	template_restore = XmCreatePushButtonGadget(form1,
					"restoreDefault", args, n);
	children[nchildren++] = template_restore;
	fep->template_restore = template_restore;
	XtAddCallback(template_restore, XmNactivateCallback,
				  cb_restoreTemplate, (XtPointer)fep);

	n = 0;
	template_browse = XmCreatePushButtonGadget(form1,
				   "browseTemplate", args, n);
	children[nchildren++] = template_browse;

	XtVaSetValues(template_browse, XmNuserData, template_text, 0);
	XtAddCallback(template_browse, XmNactivateCallback,
				  cb_browseTemplate, (XtPointer)fep);

	XtManageChildren(children, nchildren);

	labels_height = XfeVaGetTallestWidget(author_label,
					  author_text,
					  autosave_toggle,
					  NULL);
	
	XtVaSetValues(author_label,
				  XmNheight, labels_height,
				  XmNtopAttachment, XmATTACH_FORM,
				  XmNbottomAttachment, XmATTACH_NONE,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNrightAttachment, XmATTACH_NONE,
				  NULL);

	XtVaSetValues(author_text,
				  XmNheight, labels_height,
				  XmNtopAttachment, XmATTACH_FORM,
				  XmNbottomAttachment, XmATTACH_NONE,
				  XmNleftAttachment, XmATTACH_WIDGET,
				  XmNleftWidget, author_label,
				  XmNrightAttachment, XmATTACH_NONE,
				  NULL);

	XtVaSetValues(autosave_toggle,
				  XmNheight, labels_height,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, author_label,
				  XmNbottomAttachment, XmATTACH_NONE,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNrightAttachment, XmATTACH_NONE,
				  NULL);

	XtVaSetValues(autosave_text,
				  XmNheight, labels_height,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, autosave_toggle,
				  XmNbottomAttachment, XmATTACH_NONE,
				  XmNleftAttachment, XmATTACH_WIDGET,
				  XmNleftWidget, autosave_toggle,
				  XmNrightAttachment, XmATTACH_NONE,
				  NULL);

	XtVaSetValues(autosave_label,
				  XmNheight, labels_height,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, autosave_toggle,
				  XmNbottomAttachment, XmATTACH_NONE,
				  XmNleftAttachment, XmATTACH_WIDGET,
				  XmNleftWidget, autosave_text,
				  XmNrightAttachment, XmATTACH_NONE,
				  NULL);

	XtVaSetValues(template_label,
				  XmNheight, labels_height,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, autosave_toggle,
				  XmNbottomAttachment, XmATTACH_NONE,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNrightAttachment, XmATTACH_NONE,
				  NULL);

	XtVaSetValues(template_text,
				  XmNheight, labels_height,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, template_label,
				  XmNbottomAttachment, XmATTACH_NONE,
				  XmNleftAttachment, XmATTACH_WIDGET,
				  XmNleftWidget, template_label,
				  XmNrightAttachment, XmATTACH_FORM,
				  NULL);

	XtVaSetValues(template_browse,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, template_text,
				  XmNbottomAttachment, XmATTACH_NONE,
				  XmNleftAttachment, XmATTACH_NONE,
				  XmNrightAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNrightWidget, template_text,
				  NULL);

	XtVaSetValues(template_restore,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, template_browse,
				  XmNbottomAttachment, XmATTACH_NONE,
				  XmNleftAttachment, XmATTACH_NONE,
				  XmNrightAttachment, XmATTACH_WIDGET,
				  XmNrightWidget, template_browse,
				  XmNrightOffset, 8,
				  NULL);

	n = 0;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, frame1); n++;
	XtSetArg(args[n], XmNtopOffset, 8); n++;
	XtSetArg(args[n], XmNbottomAttachment, XmATTACH_NONE); n++;
	external_frame = fe_CreateFrame(form, "externalFrame", args, n);
	XtManageChild(external_frame);

	n = 0;
	external_form = XmCreateForm(external_frame, "external", args, n);
	XtManageChild(external_form);

	nchildren = 0;
	n = 0;
	html_label = XmCreateLabelGadget(external_form, "htmlLabel", args, n);
	children[nchildren++] = html_label;

	n = 0;
	image_label = XmCreateLabelGadget(external_form, "imageLabel", args, n);
	children[nchildren++] = image_label;

	n = 0;
	html_browse = XmCreatePushButtonGadget(external_form, "browse", args, n);
	children[nchildren++] = html_browse;
    fep->html_browse = html_browse;

	n = 0;
	image_browse = XmCreatePushButtonGadget(external_form, "browse", args, n);
	children[nchildren++] = image_browse;
    fep->image_browse = image_browse;

	n = 0;
	XtSetArg(args[n], XmNuserData, EDITOR_GENERAL_HTML_EDITOR); n++;
	html_text = fe_CreateTextField(external_form, "htmlText", args, n);
	children[nchildren++] = html_text;
	fep->html_editor = html_text;
	XtAddCallback(html_text, XmNvalueChangedCallback,
				  cb_changed, (XtPointer)fep);

	n = 0;
	XtSetArg(args[n], XmNuserData, EDITOR_GENERAL_IMAGE_EDITOR); n++;
	image_text = fe_CreateTextField(external_form, "imageText", args, n);
	children[nchildren++] = image_text;
	fep->image_editor = image_text;
	XtAddCallback(image_text, XmNvalueChangedCallback,
				  cb_changed, (XtPointer)fep);

	/*
	 *    Go back for browse callbacks
	 */
	XtVaSetValues(image_browse, XmNuserData, image_text, 0);

	XtAddCallback(image_browse, XmNactivateCallback,
				  cb_browseToTextField, (XtPointer)context);

	XtVaSetValues(html_browse, XmNuserData, html_text, 0);

	XtAddCallback(html_browse, XmNactivateCallback,
				  cb_browseToTextField, (XtPointer)context);

	XtManageChildren(children, nchildren);

	/*
	 *    Attachments
	 */

	labels_width = XfeVaGetWidestWidget(html_label,
					image_label,
					NULL);

	labels_height = XfeVaGetTallestWidget(html_label,
					  html_text,
					  html_browse,
					  NULL);
	
	XtVaSetValues(html_label,
				  XmNheight, labels_height,
				  RIGHT_JUSTIFY_VA_ARGS(html_label,labels_width),
				  XmNtopAttachment, XmATTACH_FORM,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtVaSetValues(html_browse,
				  XmNheight, labels_height,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, html_label,
				  XmNbottomAttachment, XmATTACH_NONE,
				  XmNleftAttachment, XmATTACH_NONE,
				  XmNrightAttachment, XmATTACH_FORM,
				  NULL);

	XtVaSetValues(html_text,
				  XmNheight, labels_height,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, html_label,
				  XmNbottomAttachment, XmATTACH_NONE,
				  XmNleftAttachment, XmATTACH_WIDGET,
				  XmNleftWidget, html_label,
				  XmNrightAttachment, XmATTACH_WIDGET,
				  XmNrightWidget, html_browse,
				  NULL);

	XtVaSetValues(image_label,
				  XmNheight, labels_height,
				  RIGHT_JUSTIFY_VA_ARGS(image_label,labels_width),
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, html_label,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtVaSetValues(image_browse,
				  XmNheight, labels_height,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, image_label,
				  XmNbottomAttachment, XmATTACH_NONE,
				  XmNleftAttachment, XmATTACH_NONE,
				  XmNrightAttachment, XmATTACH_FORM,
				  NULL);

	XtVaSetValues(image_text,
				  XmNheight, labels_height,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, image_label,
				  XmNbottomAttachment, XmATTACH_NONE,
				  XmNleftAttachment, XmATTACH_WIDGET,
				  XmNleftWidget, image_label,
				  XmNrightAttachment, XmATTACH_WIDGET,
				  XmNrightWidget, image_browse,
				  NULL);

	setCreated(TRUE);
}

// Member:       init
// Description:  Initializes page for Editor
// Inputs:
// Side effects: 

void XFE_PrefsPageEditor::init()
{
	XP_ASSERT(m_prefsDataEditor);
	PrefsDataEditor    *w_data = m_prefsDataEditor;
	
	char     *value;
	char     *value2;
	Boolean   as_enable;
	unsigned  as_time;
	MWContext *context = w_data->context;
    Boolean sensitive;

	/* get the author */
	if ((value = fe_EditorPreferencesGetAuthor(context)) != NULL)
		fe_SetTextFieldAndCallBack(w_data->author, value);
    sensitive = !PREF_PrefIsLocked("editor.author");
    XtSetSensitive(w_data->author, sensitive);

	/* get the editors */
	fe_EditorPreferencesGetEditors(context, &value, &value2);
	if (value != NULL)
		fe_SetTextFieldAndCallBack(w_data->html_editor, value);
    sensitive = !PREF_PrefIsLocked("editor.html_editor");
    XtSetSensitive(w_data->html_editor, sensitive);
    XtSetSensitive(w_data->html_browse, sensitive);

	if (value2 != NULL)
		fe_SetTextFieldAndCallBack(w_data->image_editor, value2);
    sensitive = !PREF_PrefIsLocked("editor.image_editor");
    XtSetSensitive(w_data->image_editor, sensitive);
    XtSetSensitive(w_data->image_browse, sensitive);

	/* get the template */
	if ((value = fe_EditorPreferencesGetTemplate(context)))
		fe_SetTextFieldAndCallBack(w_data->tmplate, value);
    sensitive = !PREF_PrefIsLocked("editor.template_location");
    XtSetSensitive(w_data->tmplate, sensitive);
    XtSetSensitive(w_data->template_restore, sensitive);

	/* get the autosave state */
	fe_EditorPreferencesGetAutoSave(context, &as_enable, &as_time);
	if (!as_enable)
		as_time = 10;
	
	fe_set_numeric_text_field(w_data->autosave_text, as_time);
	fe_TextFieldSetEditable(context, w_data->autosave_text, as_enable);
	XmToggleButtonGadgetSetState(w_data->autosave_toggle, as_enable, FALSE);

    XtSetSensitive(w_data->autosave_text, !PREF_PrefIsLocked("editor.auto_save_delay"));
    XtSetSensitive(w_data->autosave_toggle, !PREF_PrefIsLocked("editor.auto_save"));

	w_data->context = context;
	w_data->changed = 0;

	setInitialized(TRUE);
}

// Member:       verify
// Description:  
// Inputs:
// Side effects: 

Boolean XFE_PrefsPageEditor::verify()
{
	PrefsDataEditor  *w_data = m_prefsDataEditor;
	MWContext        *context = getContext();
	Boolean           as_enable;
	int               as_time;

	/* autosave */
	if ((w_data->changed & EDITOR_GENERAL_AUTOSAVE) != 0) {
		as_time = fe_get_numeric_text_field(w_data->autosave_text);
		as_enable = XmToggleButtonGadgetGetState(w_data->autosave_toggle);
		
		if (as_time == 0)
			as_enable = FALSE;
		
		if (as_enable) {
			if (RANGE_CHECK(as_time,AUTOSAVE_MIN_PERIOD,AUTOSAVE_MAX_PERIOD)) {
				char* msg = XP_GetString(XFE_EDITOR_AUTOSAVE_PERIOD_RANGE);
				fe_error_dialog(context, w_data->autosave_text, msg);
				return FALSE;
			}
		}
	}
	return TRUE;
}

// Member:       install
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageEditor::install()
{
	PrefsDataEditor  *w_data = m_prefsDataEditor;
	MWContext        *context = getContext();
	char             *value;
	Boolean           as_enable;
	unsigned          as_time;

	EDT_BeginBatchChanges(context);

	if (w_data->changed != 0) {

		/* author */
		if ((w_data->changed & EDITOR_GENERAL_AUTHOR) != 0) {
			value = fe_GetTextField(w_data->author);
			fe_EditorPreferencesSetAuthor(context, value);
			XtFree(value);
		}
	
		/* editors */
		value = NULL;
		if ((w_data->changed & EDITOR_GENERAL_HTML_EDITOR) != 0) {
			value = fe_GetTextField(w_data->html_editor);
			fe_EditorPreferencesSetEditors(context, value, NULL);
			XtFree(value);
		}

		if ((w_data->changed & EDITOR_GENERAL_IMAGE_EDITOR) != 0) {
			value = fe_GetTextField(w_data->image_editor);
			fe_EditorPreferencesSetEditors(context, NULL, value);
			XtFree(value);
		}

		/* template */
		if ((w_data->changed & EDITOR_GENERAL_TEMPLATE) != 0) {
			value = fe_GetTextField(w_data->tmplate);
			fe_EditorPreferencesSetTemplate(context, value);
			XtFree(value);
		}

		/* autosave */
		if ((w_data->changed & EDITOR_GENERAL_AUTOSAVE) != 0) {
			as_time = fe_get_numeric_text_field(w_data->autosave_text);
			as_enable = XmToggleButtonGadgetGetState(w_data->autosave_toggle);
			fe_EditorPreferencesSetAutoSave(context, as_enable, as_time);
		}
		w_data->changed = 0;
	}

	EDT_EndBatchChanges(context);
}

// Member:       save
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageEditor::save()
{
 	// XFE_PrefsDialog   *theDialog = getPrefsDialog();
	PrefsDataEditor   *fep = m_prefsDataEditor;
	// MWContext         *context = theDialog->getContext();

	XP_ASSERT(fep);

#define SOMETHING_CHANGED() (fep->changed != 0)

	if (SOMETHING_CHANGED()) {

		// Install preferences

		install();
	}

#undef SOMETHING_CHANGED

	fep->changed = 0;
}

// Member:       getData
// Description:  
// Inputs:
// Side effects: 

PrefsDataEditor *XFE_PrefsPageEditor::getData()
{
	return m_prefsDataEditor;
}

// Member:       cb_restoreTemplate
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageEditor::cb_restoreTemplate(Widget    /* widget */,
					     XtPointer closure, 
					     XtPointer /* call_data */)
{
    char* value;
	PrefsDataEditor *w_data	= (PrefsDataEditor *)closure;

	/* get the template */
	if ((value = fe_EditorDefaultGetTemplate()) == NULL)
	    value = "";

	fe_SetTextFieldAndCallBack(w_data->tmplate, value);

	w_data->changed |= EDITOR_GENERAL_TEMPLATE;
}

// Member:       cb_changed
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageEditor::cb_changed(Widget    widget,
				     XtPointer closure, 
				     XtPointer /* call_data */)
{
	PrefsDataEditor *w_data	= (PrefsDataEditor *)closure;
    unsigned mask = (unsigned)fe_GetUserData(widget);

	w_data->changed |= mask;
}

// Member:       cb_autosaveToggle
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageEditor::cb_autosaveToggle(Widget    /* widget */,
					    XtPointer closure,
					    XtPointer call_data)
{
	PrefsDataEditor              *w_data = (PrefsDataEditor *)closure;
	XmToggleButtonCallbackStruct *cb = (XmToggleButtonCallbackStruct*)call_data;

	fe_TextFieldSetEditable(w_data->context, w_data->autosave_text, cb->set);
	w_data->changed |= EDITOR_GENERAL_AUTOSAVE;
}

// Member:       cb_browseToTextField
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageEditor::cb_browseToTextField(Widget    widget,
											   XtPointer closure, 
											   XtPointer /* call_data */)
{
    MWContext* context = (MWContext*)closure;
	Widget     text_field = (Widget)fe_GetUserData(widget);

	fe_browse_file_of_text(context, text_field, FALSE);
}

// Member:       cb_browseTemplate
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageEditor::cb_browseTemplate(Widget    widget,
											XtPointer closure, 
											XtPointer /* call_data */)
{
    MWContext* context = (MWContext*)closure;
	Widget     text_field = (Widget)fe_GetUserData(widget);

	fe_browse_file_of_text_in_url(context, text_field, FALSE);
}

// ************************************************************************
// *************************   Editor/Appearance  *************************
// ************************************************************************

// Member:       XFE_PrefsPageEditorAppearance
// Description:  Constructor
// Inputs:
// Side effects: 

XFE_PrefsPageEditorAppearance::XFE_PrefsPageEditorAppearance(XFE_PrefsDialog *dialog)
	: XFE_PrefsPage(dialog),
	  m_prefsDataEditorAppearance(0)
{
}

// Member:       ~XFE_PrefsPageEditorAppearance
// Description:  Destructor
// Inputs:
// Side effects: 

XFE_PrefsPageEditorAppearance::~XFE_PrefsPageEditorAppearance()
{
	delete m_prefsDataEditorAppearance;
}

// Member:       create
// Description:  Creates page for EditorAppearance
// Inputs:
// Side effects: 

void XFE_PrefsPageEditorAppearance::create()
{
	Arg               av[50];
	int               ac;

	PrefsDataEditorAppearance *fep = NULL;

	fep = new PrefsDataEditorAppearance;
	memset(fep, 0, sizeof(PrefsDataEditorAppearance));
	m_prefsDataEditorAppearance = fep;

	fep->context = getContext();
	fep->prompt_dialog = getPrefsDialog()->getDialogChrome();

	MWContext *context = fep->context;
	Widget     form;

	ac = 0;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	form = XmCreateForm (m_wPageForm, "editorAppearance", av, ac);
	XtManageChild (form);
	m_wPage = fep->page = form;

	fe_EditorDocumentAppearancePropertiesStruct *w_data = &fep->appearance_data;
	w_data->context = context;

	Arg args[16];
	Cardinal n;
	Widget main_rc;

	Widget colors_frame;
	Widget colors_rc;

	Widget strategy_rc;
	Widget strategy_custom;
	Widget strategy_browser;

#ifdef DOCUMENT_COLOR_SCHEMES
	Widget schemes_frame;
	Widget schemes_form;
	Widget schemes_combo;
	Widget schemes_save;
	Widget schemes_remove;
#endif

	Widget custom_form;
	Widget custom_frame;
	Widget custom_preview_rc;
	Widget background_info;
	Widget fat_guy;
	Widget top_guy;
	Widget bottom_guy;
	Widget children[6];
	Widget swatches[6];
	int      nchildren;
	Cardinal nswatch;
	Dimension width;
	int i;

	Widget background_frame;
	Widget background_form;
	Widget background_image_toggle;
	Widget background_image_text;
	Widget background_image_button;

	Widget info_label;

	n = 0;
	XtSetArg(args[n], XmNentryAlignment, XmALIGNMENT_CENTER); n++;
	XtSetArg(args[n], XmNorientation, XmVERTICAL); n++;
	main_rc = XmCreateRowColumn(form, "appearance", args, n);
	XtManageChild(main_rc);

	/*
	 *    Custom colors.
	 */
	n = 0;
	colors_frame = fe_CreateFrame(main_rc, "documentColors", args, n);
	XtManageChild(colors_frame);

	n = 0;
	XtSetArg(args[n], XmNentryAlignment, XmALIGNMENT_CENTER); n++;
	XtSetArg(args[n], XmNorientation, XmVERTICAL); n++;
	colors_rc = XmCreateRowColumn(colors_frame, "colors", args, n);
	XtManageChild(colors_rc);

	/*
	 *    Top row.
	 */
	n = 0;
	XtSetArg(args[n], XmNorientation, XmHORIZONTAL); n++;
	XtSetArg(args[n], XmNpacking, XmPACK_TIGHT); n++;
	XtSetArg(args[n], XmNentryVerticalAlignment, XmALIGNMENT_BASELINE_TOP); n++;
	strategy_rc = XmCreateRowColumn(colors_rc, "strategy", args, n);
	XtManageChild(strategy_rc);

	n = 0;
	XtSetArg(args[n], XmNuserData, TRUE); n++;
	XtSetArg(args[n], XmNindicatorType, XmONE_OF_MANY); n++;
	strategy_custom = XmCreateToggleButtonGadget(strategy_rc,
												 "custom", args, n);
	XtManageChild(strategy_custom);

	XtAddCallback(strategy_custom, XmNvalueChangedCallback,
				  fe_document_appearance_use_custom_cb, 
				  (XtPointer)w_data);

	fe_DependentListAddDependent(&w_data->dependents,
				     strategy_custom,
				     DOCUMENT_USE_CUSTOM_MASK,
				     fe_document_appearance_use_custom_update_cb,
				     (XtPointer)w_data);

	n = 0;
	XtSetArg(args[n], XmNuserData, FALSE); n++;
	XtSetArg(args[n], XmNindicatorType, XmONE_OF_MANY); n++;
	strategy_browser = XmCreateToggleButtonGadget(strategy_rc,
												 "browser", args, n);
	XtManageChild(strategy_browser);
	
	XtAddCallback(strategy_browser, XmNvalueChangedCallback,
				  fe_document_appearance_use_custom_cb, 
				  (XtPointer)w_data);

	fe_DependentListAddDependent(&w_data->dependents,
				 strategy_browser,
				 DOCUMENT_USE_CUSTOM_MASK,
				 fe_document_appearance_use_custom_update_cb,
				 (XtPointer)w_data);

#ifdef DOCUMENT_COLOR_SCHEMES
	/*
	 *    Color Schemes.
	 */
	n = 0;
	schemes_frame = fe_CreateFrame(colors_rc, "schemes", args, n);
	XtManageChild(schemes_frame);

	n = 0;
	schemes_form = XmCreateForm(schemes_frame, "schemes", args, n);
	XtManageChild(schemes_form);

	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNsensitive, FALSE); n++;
	schemes_remove = XmCreatePushButtonGadget(schemes_form, "remove", args, n);
	XtManageChild(schemes_remove);

	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNrightAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNrightWidget, schemes_remove); n++;
	XtSetArg(args[n], XmNsensitive, FALSE); n++;
	schemes_save = XmCreatePushButtonGadget(schemes_form, "save", args, n);
	XtManageChild(schemes_save);

	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNrightAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNrightWidget, schemes_save); n++;
	XtSetArg(args[n], XmNsensitive, FALSE); n++;
	schemes_combo = fe_CreateCombo(schemes_form, "combo", args, n);
	XtManageChild(schemes_combo);
#endif

	n = 0;
	custom_form = XmCreateForm(colors_rc, "custom", args, n);
	XtManageChild(custom_form);

	for (nchildren = DOCUMENT_NORMAL_TEXT_COLOR;
		 nchildren <= DOCUMENT_BACKGROUND_COLOR;
		 nchildren++) {
	    Widget button;
		char*  name = fe_PreviewPanelCreate_names[nchildren];
		unsigned flags;

		n = 0;
		XtSetArg(args[n], XmNuserData, nchildren); n++;
		button = XmCreatePushButtonGadget(custom_form, name, args, n);
		XtAddCallback(button, XmNactivateCallback,
					fe_document_appearance_color_cb, 
					(XtPointer)w_data);
		flags = DOCUMENT_USE_CUSTOM_MASK;
		if (nchildren == DOCUMENT_BACKGROUND_COLOR)
			flags |= DOCUMENT_USE_IMAGE_MASK;
		fe_DependentListAddDependent(&w_data->dependents,
				 button,
				 flags,
				 fe_document_appearance_sensitized_update_cb,
				 (XtPointer)w_data);
		children[nchildren] = button;
	}
	XtManageChildren(children, nchildren);

	fat_guy = fe_GetBiggestWidget(TRUE, children, nchildren);
	XtVaGetValues(fat_guy, XmNwidth, &width, 0);

	/* swatches */
	for (nswatch = DOCUMENT_NORMAL_TEXT_COLOR;
		 nswatch <= DOCUMENT_BACKGROUND_COLOR;
		 nswatch++) {
	    Widget foo;
		char   name[32];
		unsigned flags;

		sprintf(name, "%sSwatch", fe_PreviewPanelCreate_names[nswatch]);
		n = 0;
		XtSetArg(args[n], XmNuserData, nswatch); n++;
		XtSetArg(args[n], XmNwidth, SWATCH_SIZE); n++;
		foo = fe_CreateSwatch(custom_form, name, args, n);
		XtAddCallback(foo, XmNactivateCallback,
					fe_document_appearance_color_cb, 
					(XtPointer)w_data);

		flags = DOCUMENT_USE_CUSTOM_MASK|(1<<nswatch);
		if (nswatch == DOCUMENT_BACKGROUND_COLOR)
			flags |= DOCUMENT_USE_IMAGE_MASK;
		fe_DependentListAddDependent(&w_data->dependents,
				 foo,
				 flags,
				 fe_document_appearance_swatch_update_cb,
				 (XtPointer)w_data);
		swatches[nswatch] = foo;
	}
	XtManageChildren(swatches, nswatch);

	/*
	 *    Do attachments.
	 */
	for (top_guy = NULL, i = 0; i < nchildren; i++) {

		n = 0;
		if (top_guy) {
			XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
			XtSetArg(args[n], XmNtopWidget, top_guy); n++;
		} else {
			XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
		}
		XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;

		top_guy = children[i];

		if (top_guy != fat_guy) {
		    	/*
			 *    Have to do this to avoid circular dependency in
			 *    losing XmForm.
			 */
		    XtSetArg(args[n], XmNwidth, width); n++;
#if 0
		    XtSetArg(args[n], XmNrightAttachment,
				   XmATTACH_OPPOSITE_WIDGET); n++;
		    XtSetArg(args[n],XmNrightWidget, fat_guy); n++;
#endif
		}
		XtSetValues(top_guy, args, n);

		/* Do swatch. */
		n = 0;
		XtSetArg(args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
		XtSetArg(args[n], XmNleftWidget, top_guy); n++;
		XtSetArg(args[n], XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET); n++;
		XtSetArg(args[n], XmNtopWidget, top_guy); n++;
		XtSetArg(args[n], XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET); n++;
		XtSetArg(args[n], XmNbottomWidget, top_guy); n++;
		XtSetValues(swatches[i], args, n);
	}

	top_guy = swatches[DOCUMENT_NORMAL_TEXT_COLOR];
	bottom_guy = swatches[DOCUMENT_FOLLOWED_TEXT_COLOR];

	n = 0;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNleftWidget, top_guy); n++;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, top_guy); n++;
	XtSetArg(args[n], XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET); n++;
	XtSetArg(args[n], XmNbottomWidget, bottom_guy); n++;
	XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
#if 1
	custom_frame = /*fe_*/XmCreateFrame(custom_form, "previewFrame", args, n);
	XtManageChild(custom_frame);
	n = 0; 
	XtSetArg(args[n], XmNmarginWidth, 0); n++;
	XtSetArg(args[n], XmNmarginHeight, 0); n++;
	XtSetValues(custom_frame, args, n);
#else
	n = 0;
	XtSetArg(args[n], XmNorientation, XmVERTICAL); n++;
	XtSetArg(args[n], XmNborderWidth, 1); n++;
	XtSetArg(args[n], XmNborderColor,
			 CONTEXT_DATA(w_data->context)->default_fg_pixel); n++;
#endif
	custom_preview_rc = fe_PreviewPanelCreate(custom_frame, "preview",
											  args, n);
	XtManageChild(custom_preview_rc);

	XtAddCallback(custom_preview_rc, XmNinputCallback,
				  fe_preview_panel_click_cb, (XtPointer)w_data);

	fe_DependentListAddDependent(&w_data->dependents,
				 custom_preview_rc,
				 (DOCUMENT_USE_CUSTOM_MASK|DOCUMENT_ALL_APPEARANCE),
				 fe_document_appearance_preview_update_cb,
				 (XtPointer)w_data);
	fe_DependentListAddDependent(&w_data->dependents,
				 custom_preview_rc,
				 DOCUMENT_USE_CUSTOM_MASK,
				 fe_document_appearance_sensitized_update_cb,
				 (XtPointer)w_data);
	top_guy = swatches[DOCUMENT_BACKGROUND_COLOR];

	n = 0;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNleftWidget, top_guy); n++;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, top_guy); n++;
	XtSetArg(args[n], XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET); n++;
	XtSetArg(args[n], XmNbottomWidget, top_guy); n++;
	XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	background_info = XmCreateLabelGadget(custom_form, "backgroundInfo", args, n);
	XtManageChild(background_info);

	/*
	 *    Background.
	 */
	n = 0;
	background_frame = fe_CreateFrame(main_rc, "backgroundImage", args, n);
	XtManageChild(background_frame);

	n = 0;
	background_form = XmCreateForm(background_frame, "backgroundImage", args, n);
	XtManageChild(background_form);

	nchildren = 0;
	n = 0;
	XtSetArg(args[n], XmNindicatorType, XmN_OF_MANY); n++;
	background_image_toggle = XmCreateToggleButtonGadget(background_form,
							"useImage", args, n);
	children[nchildren++] = background_image_toggle;

	XtAddCallback(background_image_toggle, XmNvalueChangedCallback,
				  fe_document_appearance_use_image_cb, 
				  (XtPointer)w_data);
	fe_DependentListAddDependent(&w_data->dependents,
			 background_image_toggle,
			 (DOCUMENT_USE_IMAGE_MASK|DOCUMENT_USE_CUSTOM_MASK),
			 fe_document_appearance_use_image_update_cb,
			 (XtPointer)w_data);

	n = 0;
	background_image_text = fe_CreateTextField(background_form,
					  "imageText", args, n);
	XtAddCallback(background_image_text, XmNvalueChangedCallback,
				  fe_document_appearance_image_text_cb,
				  (XtPointer)w_data);
	fe_DependentListAddDependent(&w_data->dependents,
				 background_image_text,
				 (DOCUMENT_BACKGROUND_IMAGE_MASK|
				  DOCUMENT_USE_IMAGE_MASK|
				  DOCUMENT_USE_CUSTOM_MASK),
				 fe_document_appearance_image_text_update_cb,
				 (XtPointer)w_data);
	w_data->image_text = background_image_text;
	children[nchildren++] = background_image_text;

	n = 0;
	XtSetArg(args[n], XmNuserData, background_image_text); n++;
	background_image_button = XmCreatePushButtonGadget(background_form,
						   "browseImageFile", args, n);
	XtAddCallback(background_image_button, XmNactivateCallback,
				  fe_document_appearance_image_text_browse_cb,
				  (XtPointer)w_data);
	fe_DependentListAddDependent(&w_data->dependents,
			 background_image_button,
			 (DOCUMENT_USE_IMAGE_MASK|DOCUMENT_USE_CUSTOM_MASK),
			 fe_document_appearance_use_image_button_update_cb,
			 (XtPointer)w_data);
	children[nchildren++] = background_image_button;

	/*
	 *    Do background image group attachments.
	 */
	n = 0;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetValues(background_image_toggle, args, n);

	n = 0;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNleftWidget, background_image_toggle); n++;
	XtSetArg(args[n], XmNrightAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNrightWidget, background_image_button); n++;
	XtSetValues(background_image_text, args, n);

	n = 0;
	XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetValues(background_image_button, args, n);

	XtManageChildren(children, nchildren);

	/*
	 *    Bottom line info text.
	 */
	n = 0;
	info_label = XmCreateLabelGadget(main_rc, "infoLabel", args, n);
	XtManageChild(info_label);

	w_data->is_editor_preferences = TRUE;

	setCreated(TRUE);
}

// Member:       init
// Description:  Initializes page for EditorAppearance
// Inputs:
// Side effects: 

void XFE_PrefsPageEditorAppearance::init()
{
	XP_ASSERT(m_prefsDataEditorAppearance);
	PrefsDataEditorAppearance    *fep = m_prefsDataEditorAppearance;
	MWContext                    *context = fep->context;
	
	fe_document_appearance_init(context, &fep->appearance_data);

	setInitialized(TRUE);
}

// Member:       install
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageEditorAppearance::install()
{
	PrefsDataEditorAppearance  *fep = m_prefsDataEditorAppearance;
	MWContext                  *context = getContext();
	fe_EditorDocumentAppearancePropertiesStruct *w_data = &fep->appearance_data;

	EDT_BeginBatchChanges(context);

	if (w_data->changed != 0) {
		fe_document_appearance_set(context, w_data);
		w_data->changed = 0;
	}

	EDT_EndBatchChanges(context);
}

// Member:       save
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageEditorAppearance::save()
{
	// XFE_PrefsDialog            *theDialog = getPrefsDialog();
	PrefsDataEditorAppearance  *fep = m_prefsDataEditorAppearance;
	// MWContext                  *context = theDialog->getContext();
	fe_EditorDocumentAppearancePropertiesStruct *w_data = &fep->appearance_data;

	XP_ASSERT(fep);

#define SOMETHING_CHANGED() (w_data->changed != 0)

	if (SOMETHING_CHANGED()) {

		// Install preferences

		install();
	}

#undef SOMETHING_CHANGED

	w_data->changed = 0;
}

// Member:       getData
// Description:  
// Inputs:
// Side effects: 

PrefsDataEditorAppearance *XFE_PrefsPageEditorAppearance::getData()
{
	return m_prefsDataEditorAppearance;
}

// ************************************************************************
// *************************     Editor/Publish   *************************
// ************************************************************************

// Member:       XFE_PrefsPageEditorPublish
// Description:  Constructor
// Inputs:
// Side effects: 

XFE_PrefsPageEditorPublish::XFE_PrefsPageEditorPublish(XFE_PrefsDialog *dialog)
	: XFE_PrefsPage(dialog),
	  m_prefsDataEditorPublish(0)
{
}

// Member:       ~XFE_PrefsPageEditorPublish
// Description:  Destructor
// Inputs:
// Side effects: 

XFE_PrefsPageEditorPublish::~XFE_PrefsPageEditorPublish()
{
	delete m_prefsDataEditorPublish;
}

// Member:       create
// Description:  Creates page for EditorPublish
// Inputs:
// Side effects: 

void XFE_PrefsPageEditorPublish::create()
{
	Arg               av[50];
	int               ac;

	PrefsDataEditorPublish *w_data = NULL;

	w_data = new PrefsDataEditorPublish;
	memset(w_data, 0, sizeof(PrefsDataEditorPublish));
	m_prefsDataEditorPublish = w_data;

	w_data->context = getContext();
	w_data->prompt_dialog = getPrefsDialog()->getDialogChrome();

	Widget     form;

	ac = 0;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	form = XmCreateForm (m_wPageForm, "editorPublish", av, ac);
	XtManageChild (form);
	m_wPage = w_data->page = form;

	Widget links_frame;
	Widget links_main_rc;
	Widget links_main_info;
	Widget links_sub_rc;
	Widget links_toggle;
	Widget links_info;
	Widget images_toggle;
	Widget images_info;
	Widget links_main_tip;

	Widget publish_frame;
	Widget publish_form;
	Widget publish_label;
	Widget publish_text;
	Widget browse_label;
	Widget browse_text;
	Widget children[16];
	Cardinal nchildren;
	Arg    args[16];
	Cardinal n;

	n = 0;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNbottomAttachment, XmATTACH_NONE); n++;
	links_frame = fe_CreateFrame(form, "linksAndImages", args, n);
	XtManageChild(links_frame);

	n = 0;
	XtSetArg(args[n], XmNorientation, XmVERTICAL); n++;
	links_main_rc = XmCreateRowColumn(links_frame, "linksAndImages", args, n);
	XtManageChild(links_main_rc);

	nchildren = 0;
	n = 0;
	XtSetArg(args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	links_main_info = XmCreateLabelGadget(links_main_rc, "linksAndImagesLabel",
										  args, n);
	children[nchildren++] = links_main_info;

	n = 0;
	XtSetArg(args[n], XmNorientation, XmVERTICAL); n++;
	XtSetArg(args[n], XmNisAligned, TRUE); n++;
	XtSetArg(args[n], XmNentryAlignment, XmALIGNMENT_BEGINNING); n++;
	links_sub_rc = XmCreateRowColumn(links_main_rc, 
					"linksAndImagesToggles",
					 args, n);
	children[nchildren++] = links_sub_rc;

	n = 0;
	XtSetArg(args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	links_main_tip = XmCreateLabelGadget(links_main_rc, 
					"linksAndImagesTip",
					  args, n);
	children[nchildren++] = links_main_tip;

	XtManageChildren(children, nchildren);

	nchildren = 0;
	n = 0;
	links_toggle = XmCreateToggleButtonGadget(links_sub_rc, 
						"linksToggle",
						  args, n);
	children[nchildren++] = links_toggle;

	n = 0;
	XtSetArg(args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	links_info = XmCreateLabelGadget(links_sub_rc, "linksInfo", args, n);
	children[nchildren++] = links_info;

	n = 0;
	images_toggle = XmCreateToggleButtonGadget(links_sub_rc, 
						"imagesToggle",
						  args, n);
	children[nchildren++] = images_toggle;

	n = 0;
	XtSetArg(args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	images_info = XmCreateLabelGadget(links_sub_rc, "imagesInfo", args, n);
	children[nchildren++] = images_info;

	XtManageChildren(children, nchildren);

	n = 0;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, links_frame); n++;
	XtSetArg(args[n], XmNtopOffset, 8); n++;
	XtSetArg(args[n], XmNbottomAttachment, XmATTACH_NONE); n++;
	publish_frame = fe_CreateFrame(form, "publish", args, n);
	XtManageChild(publish_frame);

	n = 0;
	publish_form = XmCreateForm(publish_frame, "publish", args, n);
	XtManageChild(publish_form);

	nchildren = 0;
	n = 0;
	publish_label = XmCreateLabelGadget(publish_form, "publishLabel", args, n);
	children[nchildren++] = publish_label;

	n = 0;
	browse_label = XmCreateLabelGadget(publish_form, "browseLabel", args, n);
	children[nchildren++] = browse_label;

	n = 0;
	XtSetArg(args[n], XmNuserData, EDITOR_PUBLISH_PUBLISH); n++;
	publish_text = fe_CreateTextField(publish_form, "publishText", args, n);
	children[nchildren++] = publish_text;
	
	n = 0;
	XtSetArg(args[n], XmNuserData, EDITOR_PUBLISH_BROWSE); n++;
	browse_text = fe_CreateTextField(publish_form, "browseText", args, n);
	children[nchildren++] = browse_text;

	/*
	 *    Go back for attachments
	 */

	int labels_width;
	labels_width = XfeVaGetWidestWidget(publish_label,
					browse_label,
					NULL);

	int labels_height;
	labels_height = XfeVaGetTallestWidget(publish_label,
					  publish_text,
					  NULL);

	XtVaSetValues(publish_label,
		  XmNheight, labels_height,
		  RIGHT_JUSTIFY_VA_ARGS(publish_label,labels_width),
		  XmNtopAttachment, XmATTACH_FORM,
		  XmNbottomAttachment, XmATTACH_NONE,
		  NULL);

	XtVaSetValues(browse_label,
		  XmNheight, labels_height,
		  RIGHT_JUSTIFY_VA_ARGS(browse_label,labels_width),
		  XmNtopAttachment, XmATTACH_WIDGET,
		  XmNtopWidget, publish_label,
		  XmNbottomAttachment, XmATTACH_NONE,
		  NULL);

	XtVaSetValues(publish_text,
		  XmNheight, labels_height,
		  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
		  XmNtopWidget, publish_label,
		  XmNleftAttachment, XmATTACH_WIDGET,
		  XmNleftWidget, publish_label,
		  XmNrightAttachment, XmATTACH_NONE,
		  XmNbottomAttachment, XmATTACH_NONE,
		  NULL);

	XtVaSetValues(browse_text,
		  XmNheight, labels_height,
		  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
		  XmNtopWidget, browse_label,
		  XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
		  XmNleftWidget, publish_text,
		  XmNrightAttachment, XmATTACH_NONE,
		  XmNbottomAttachment, XmATTACH_NONE,
		  NULL);

	XtManageChildren(children, nchildren);

    w_data->maintain_links = links_toggle;
    w_data->keep_images = images_toggle;
    w_data->publish_text = publish_text;
    w_data->browse_text = browse_text;

	XtVaSetValues(links_toggle, XmNuserData, EDITOR_PUBLISH_LINKS, 0);
	XtAddCallback(links_toggle, XmNvalueChangedCallback,
				  cb_changed, (XtPointer)w_data);
	XtVaSetValues(images_toggle, XmNuserData, EDITOR_PUBLISH_IMAGES, 0);
	XtAddCallback(images_toggle, XmNvalueChangedCallback,
				  cb_changed, (XtPointer)w_data);
	XtVaSetValues(publish_text, XmNuserData, EDITOR_PUBLISH_PUBLISH, 0);
	XtAddCallback(publish_text, XmNvalueChangedCallback,
				  cb_changed, (XtPointer)w_data);
	XtVaSetValues(browse_text, XmNuserData, EDITOR_PUBLISH_BROWSE, 0);
	XtAddCallback(browse_text, XmNvalueChangedCallback,
				  cb_changed, (XtPointer)w_data);
	setCreated(TRUE);
}

// Member:       init
// Description:  Initializes page for EditorPublish
// Inputs:
// Side effects: 

void XFE_PrefsPageEditorPublish::init()
{
	XP_ASSERT(m_prefsDataEditorPublish);

	PrefsDataEditorPublish *w_data = m_prefsDataEditorPublish;
	MWContext              *context = w_data->context;

    char* location;
    char* browse_location;
	char* username;
	char* password;
	Boolean links;
	Boolean images;
	Boolean save_password;
	Boolean sensitive;

	fe_EditorPreferencesGetLinksAndImages(context, &links, &images);

	save_password = fe_EditorPreferencesGetPublishLocation(context,
														   &location,
														   &username,
														   &password);

	browse_location = fe_EditorPreferencesGetBrowseLocation(context);

	XmToggleButtonSetState(w_data->maintain_links, links, FALSE);
	XmToggleButtonSetState(w_data->keep_images, images, FALSE);
	/*XmToggleButtonSetState(w_data->save_password, save_password, FALSE);*/

    sensitive = !PREF_PrefIsLocked("editor.publish_keep_links");
    XtSetSensitive(w_data->maintain_links, sensitive);
    sensitive = !PREF_PrefIsLocked("editor.publish_keep_images");
    XtSetSensitive(w_data->keep_images, sensitive);

	if (location) {
	    fe_TextFieldSetString(w_data->publish_text, location, FALSE);

	}

	if (browse_location)
		fe_TextFieldSetString(w_data->browse_text, browse_location, FALSE);
	else
		fe_TextFieldSetString(w_data->browse_text, "", FALSE);

    sensitive = !PREF_PrefIsLocked("editor.publish_location");
    XtSetSensitive(w_data->publish_text, sensitive);
    sensitive = !PREF_PrefIsLocked("editor.publish_browse_location");
    XtSetSensitive(w_data->browse_text, sensitive);

	if (browse_location) {
	    free(browse_location);
	}
	if (location) {
	    free(location);
	}

	setInitialized(TRUE);
}

// Member:       install
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageEditorPublish::install()
{
	PrefsDataEditorPublish  *w_data = m_prefsDataEditorPublish;
	MWContext               *context = getContext();

    char* location = NULL;
    char* browse_location = NULL;
	char* username = NULL;
	char* password = NULL;
	Boolean new_links;
	Boolean new_images;
	Boolean old_links;
	Boolean old_images;

	EDT_BeginBatchChanges(context);

	if (w_data->changed != 0) {

		new_links = XmToggleButtonGetState(w_data->maintain_links);
		new_images = XmToggleButtonGetState(w_data->keep_images);

		fe_EditorPreferencesGetLinksAndImages(context, &old_links, &old_images);

		if (new_links != old_links || new_images != old_images) {
			fe_EditorPreferencesSetLinksAndImages(context, new_links, new_images);
		}

#define PUBLISH_MASK (EDITOR_PUBLISH_PUBLISH|  \
					  EDITOR_PUBLISH_BROWSE| \
					  EDITOR_PUBLISH_USERNAME| \
					  EDITOR_PUBLISH_PASSWORD|EDITOR_PUBLISH_PASSWORD_SAVE)


			if ((w_data->changed & PUBLISH_MASK) != 0) { 

				location = fe_GetTextField(w_data->publish_text);
				browse_location = fe_GetTextField(w_data->browse_text);
		
				fe_EditorPreferencesSetPublishLocation(context,
								   location,
								   username,
								   password);
				/*
				  new_save_password? password: 0);
				  */
				fe_EditorPreferencesSetBrowseLocation(context,
							 browse_location);
			}
#undef PUBLISH_MASK
			if (browse_location) {
				XtFree(browse_location);
			}
			if (location) {
				XtFree(location);
			}
			w_data->changed = 0;
	}
	
	EDT_EndBatchChanges(context);
}

// Member:       save
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageEditorPublish::save()
{
	// XFE_PrefsDialog         *theDialog = getPrefsDialog();
	PrefsDataEditorPublish  *fep = m_prefsDataEditorPublish;
	// MWContext               *context = theDialog->getContext();

	XP_ASSERT(fep);

#define SOMETHING_CHANGED() (fep->changed != 0)

	if (SOMETHING_CHANGED()) {

		// Install preferences

		install();
	}

#undef SOMETHING_CHANGED

	fep->changed = 0;
}

// Member:       getData
// Description:  
// Inputs:
// Side effects: 

PrefsDataEditorPublish *XFE_PrefsPageEditorPublish::getData()
{
	return m_prefsDataEditorPublish;
}

// Member:       cb_changed
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageEditorPublish::cb_changed(Widget    widget,
											XtPointer closure, 
											XtPointer /* call_data */)
{
	PrefsDataEditorPublish *w_data	= (PrefsDataEditorPublish *)closure;

	w_data->changed |= (unsigned)fe_GetUserData(widget);
}

// Member:       cb_changed
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageEditorPublish::cb_passwordChanged(Widget    /* widget */,
						XtPointer closure, 
						XtPointer /* call_data */)
{
	PrefsDataEditorPublish *w_data	= (PrefsDataEditorPublish *)closure;

	w_data->changed |= EDITOR_PUBLISH_PASSWORD;
}

// ************************************************************************
// *************************        Misc.         *************************
// ************************************************************************

static Widget
fe_GetBiggestWidget(Boolean horizontal, Widget* widgets, Cardinal n)
{
	Dimension max_width = 0;
	Dimension width;
	Widget max_widget = NULL;
	Widget widget;
	Cardinal i;
	
	if (!widgets)
		return NULL;

	for (i = 0; i < n; i++) {

		widget = widgets[i];

		if (horizontal) {
			XtVaGetValues(widget, XmNwidth, &width, 0);
		} else {
			XtVaGetValues(widget, XmNheight, &width, 0);
		}
		if (width > max_width) {
			max_width = width;
			max_widget = widget;
		}
	}
	
	return max_widget;
}

#if 0
static Dimension
fe_get_offset_from_widest(Widget* children, Cardinal nchildren)
{
	Dimension width;
	Dimension margin_width;

	Widget fat_guy = fe_GetBiggestWidget(TRUE, children, nchildren);
	XtVaGetValues(fat_guy, XmNwidth, &width, 0);
	XtVaGetValues(XtParent(fat_guy), XmNmarginWidth, &margin_width, 0);

	return width + margin_width;
}
#endif

static void
fe_set_numeric_text_field(Widget widget, unsigned value)
{
	char buf[32];

	sprintf(buf, "%d", value);

	fe_TextFieldSetString(widget, buf, FALSE);
}

static int
fe_get_numeric_text_field(Widget widget)
{
	char* p = fe_GetTextField(widget);
	char* q;
	int result = 0;

	if (!p)
		return -1;

	char* orig_p = p;
	while (isspace(*p))
		p++;
	if (*p == '\0') {
		XP_FREEIF(orig_p);
		return -1;
	}
	for (q = p; *q != '\0'; q++) {
		if (!(isdigit(*q) || isspace(*q))) {
			XP_FREEIF(orig_p);
			return -1; /* force error */
		}
	}

	result = atoi(p);
	
	XP_FREEIF(orig_p);

	return result;
}

static void
fe_error_dialog(MWContext* /* context */, Widget parent, char* s)
{
	while (!XtIsWMShell(parent) && (XtParent(parent)!=0))
		parent = XtParent(parent);

	fe_dialog(parent, "error", s, FALSE, 0, FALSE, FALSE, 0);
}

