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
   PrefsApplEdit.cpp -- The more dialog for Mail server preference
   Created: Linda Wei <lwei@netscape.com>, 23-Oct-96.
 */

#include "felocale.h"
#include "structs.h"
#include "fonts.h"
#include "xpassert.h"
#include "np.h"
#include "xfe.h"
#include "e_kit.h"
#include "PrefsApplEdit.h"
#include "PrefsDialogAppl.h"
#include "PrefsDialog.h"

#include <Xm/Xm.h>
#include <Xm/Label.h>
#include <Xm/LabelG.h>
#include <Xm/PushB.h>
#include <Xm/PushBG.h>
#include <Xm/ToggleB.h>
#include <Xm/Form.h>
#include <Xm/Frame.h>
#include <Xm/TextF.h> 
#include <Xm/ToggleBG.h> 
#include <XmL/Grid.h>
#include <DtWidgets/ComboBox.h>
#include <Xfe/Xfe.h>

extern int XFE_HELPERS_PLUGIN_DESC_CHANGE;
extern int XFE_HELPERS_EMPTY_MIMETYPE;
extern int XFE_ERROR_SAVING_OPTIONS;
extern int XFE_HELPERS_EMPTY_APP;

extern "C"
{
	char     *XP_GetString(int i);
/* 	char     *XP_AppendStr(char *s1, char *s2); */
	void      fe_browse_file_of_text(MWContext *context, Widget text_field, Boolean dirp);
	char     *fe_GetPluginNameFromMailcapEntry(NET_mdataStruct *md_item);
	int       fe_GetMimetypeInfoFromPlugin(char *pluginName, char *mimetype,
										   char **r_desc, char **r_ext);
	XP_Bool   fe_IsMailcapEntryPlugin(NET_mdataStruct *md_item);
}

// ==================== Public Member Functions ====================

// Member:       XFE_PrefsApplEditDialog
// Description:  Constructor
// Inputs:
// Side effects: Creates the edit dialog for applications

XFE_PrefsApplEditDialog::XFE_PrefsApplEditDialog
                        (XFE_PrefsDialog *prefsDialog,
						 Widget           parent,      // dialog parent
						 char             *name,        // dialog name
						 XFE_PrefsPageGeneralAppl *prefsPageGeneralAppl,
						 Boolean               modal)       // modal dialog?
	: XFE_Dialog(parent, 
				 name,
				 TRUE,     // ok
				 TRUE,     // cancel
				 FALSE,    // help
				 FALSE,    // apply
				 FALSE,    // separator
				 modal     // modal
				 ),
	  m_prefsDialog(prefsDialog),
	  m_prefsPageGeneralAppl(prefsPageGeneralAppl),
	  m_prefsDataApplEdit(0),
	  m_prefsDataGeneralAppl(0)
{
	m_prefsDataGeneralAppl = m_prefsPageGeneralAppl->getData();

	PrefsDataApplEdit *fep = NULL;

	fep = new PrefsDataApplEdit;
	memset(fep, 0, sizeof(PrefsDataApplEdit));
	m_prefsDataApplEdit = fep;
	fep->context = m_prefsDialog->getContext();
	
	Widget     kids[100];
	Arg        av[50];
	int        ac;
	int        i;

	Widget form;
	form = XtVaCreateWidget("form", xmFormWidgetClass, m_chrome,
							XmNmarginWidth, 8,
							XmNtopAttachment, XmATTACH_FORM,
							XmNleftAttachment, XmATTACH_FORM,
							XmNrightAttachment, XmATTACH_FORM,
							XmNbottomAttachment, XmATTACH_FORM,
							NULL);
	XtManageChild (form);

	Widget form1;
	ac = 0;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_NONE); ac++;
	form1 = XmCreateForm (form, "applEditForm", av, ac);
 
	Widget static_desc_label;
	Widget mime_types_desc_label;
	Widget mime_types_desc_text;
	Widget mime_types_label;
	Widget mime_types_text;
	Widget mime_types_suffix_label;
	Widget mime_types_suffix_text;

	ac = 0;
	i = 0;
	kids[i++] = mime_types_desc_label =
		XmCreateLabelGadget (form1, "mimeTypesDescriptionLabel", av, ac);

	kids[i++] = mime_types_desc_text = 
		fe_CreateTextField (form1, "mimeTypesDescriptionText",av,ac);

	kids[i++] = static_desc_label =
		XmCreateLabelGadget (form1, "staticDescriptionLabel", av, ac);

	kids[i++] = mime_types_label = 
		XmCreateLabelGadget (form1, "mimeTypesLabel", av, ac);

	kids[i++] = mime_types_text = 
		fe_CreateTextField (form1, "mimeTypesText",av,ac);

	kids[i++] = mime_types_suffix_label = 
		XmCreateLabelGadget (form1, "mimeTypesSuffixLabel", av, ac);

	kids[i++] = mime_types_suffix_text =
		fe_CreateTextField (form1, "mimeTypesSuffixText",av, ac);
 
	if (fe_globalData.nonterminal_text_translations) {
		XtOverrideTranslations (mime_types_desc_text,
								fe_globalData.nonterminal_text_translations);
		XtOverrideTranslations (mime_types_text,
								fe_globalData.nonterminal_text_translations);
	}

	XtAddCallback(mime_types_text, 
				  XmNvalueChangedCallback, prefsApplEditCb_setHandledBy, this);
 
	int labels_height;
	labels_height = XfeVaGetTallestWidget(mime_types_desc_label,
										  mime_types_desc_text,
										  NULL);

	int labels_width;
	labels_width = XfeVaGetWidestWidget(mime_types_desc_label,
										mime_types_label,
										mime_types_suffix_label,
										NULL);

	XtVaSetValues (mime_types_desc_label,
				   XmNheight, labels_height,
				   RIGHT_JUSTIFY_VA_ARGS(mime_types_desc_label,labels_width),
				   XmNtopAttachment, XmATTACH_FORM,
				   XmNbottomAttachment, XmATTACH_NONE,
				   0);

	XtVaSetValues (mime_types_desc_text,
				   XmNcolumns, 35,
				   XmNheight, labels_height,
				   XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				   XmNtopWidget, mime_types_desc_label,
				   XmNbottomAttachment, XmATTACH_NONE,
				   XmNleftAttachment, XmATTACH_WIDGET,
				   XmNleftWidget, mime_types_desc_label,
				   XmNrightAttachment, XmATTACH_NONE,
				   0);

	XtVaSetValues (static_desc_label,
				   XmNheight, labels_height,
				   XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				   XmNtopWidget, mime_types_desc_label,
				   XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
				   XmNbottomWidget, mime_types_desc_label,
				   XmNleftAttachment, XmATTACH_WIDGET,
				   XmNleftWidget, mime_types_desc_label,
				   XmNrightAttachment, XmATTACH_NONE,
				   0);

	XtVaSetValues (mime_types_label,
				   XmNheight, labels_height,
				   RIGHT_JUSTIFY_VA_ARGS(mime_types_label,labels_width),
				   XmNtopAttachment, XmATTACH_WIDGET,
				   XmNtopWidget, mime_types_desc_label,
				   XmNbottomAttachment, XmATTACH_NONE,
				   0);
 
	XtVaSetValues (mime_types_text,
				   XmNcolumns, 35,
				   XmNheight, labels_height,
				   XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				   XmNtopWidget,mime_types_label,
				   XmNbottomAttachment, XmATTACH_NONE,
				   XmNleftAttachment, XmATTACH_WIDGET,
				   XmNleftWidget, mime_types_label,
				   XmNrightAttachment, XmATTACH_NONE,
				   0);
 
	XtVaSetValues (mime_types_suffix_label,
				   XmNheight, labels_height,
				   RIGHT_JUSTIFY_VA_ARGS(mime_types_suffix_label,labels_width),
				   XmNtopAttachment, XmATTACH_WIDGET,
				   XmNtopWidget, mime_types_label,
				   XmNbottomAttachment, XmATTACH_NONE,
				   0);
 
	XtVaSetValues (mime_types_suffix_text,
				   XmNcolumns, 35,
				   XmNheight, labels_height,
				   XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				   XmNtopWidget, mime_types_suffix_label,
				   XmNbottomAttachment, XmATTACH_NONE,
				   XmNleftAttachment, XmATTACH_WIDGET,
				   XmNleftWidget, mime_types_suffix_label,
				   XmNrightAttachment, XmATTACH_NONE,
				   0);

	XtManageChildren(kids,i);
	XtManageChild(form1);


	Widget frame1;
	ac = 0;
	XtSetArg (av [ac], XmNmarginWidth, 8); ac++;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
	XtSetArg (av [ac], XmNtopWidget, form1); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
	frame1 = XmCreateFrame (form, "applEditFrame", av, ac);

	Widget label1;
	ac = 0;
	XtSetArg (av [ac], XmNchildType, XmFRAME_TITLE_CHILD); ac++;
	label1 = XmCreateLabelGadget (frame1, "applEditFrameLabel", av, ac);
 
	Widget form2;
	ac = 0;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
	form2 = XmCreateForm (frame1, "applEditHandleBox", av, ac);
 
	Widget navigator_toggle;
	Widget plugin_toggle;
	Widget app_toggle;
	Widget app_text;
	Widget app_browse;
	Widget plugin_combo;
	Widget save_toggle;
	Widget unknown_toggle;

	ac = 0;
	i = 0;

	kids [i++] = navigator_toggle = 
		XmCreateToggleButtonGadget (form2, "applEditNavigator", av,ac);

	kids [i++] = plugin_toggle = 
		XmCreateToggleButtonGadget (form2, "applEditPlugin", av,ac);
 
 	Visual *v = 0;
	Colormap cmap = 0;
	Cardinal depth = 0;

	XtVaGetValues (parent, 
				   XtNvisual, &v,
				   XtNcolormap, &cmap,
				   XtNdepth, &depth, 
				   0);

	ac = 0;
	XtSetArg(av[ac], XmNmoveSelectedItemUp, False); ac++;
	XtSetArg(av[ac], XmNtype, XmDROP_DOWN_LIST_BOX); ac++;
	XtSetArg(av[ac], XmNvisual, v); ac++;
	XtSetArg(av[ac], XmNdepth, depth); ac++;
	XtSetArg(av[ac], XmNcolormap, cmap); ac++;
	XtSetArg(av[ac], XmNuserData, 0); ac++;
	XtSetArg(av[ac], XmNarrowType, XmMOTIF); ac++;
	kids[i++] = plugin_combo = DtCreateComboBox(form2, "pluginCombo", av,ac);
	XtAddCallback(plugin_combo, XmNselectionCallback, prefsApplEditCb_selectPlugin, this);
 
	ac = 0;
	kids [i++] = app_toggle = 
		XmCreateToggleButtonGadget (form2, "applEditApp", av,ac);

	kids [i++] = app_text = 
		fe_CreateTextField (form2, "applEditAppText", av,ac);

	kids [i++] = app_browse = 
		XmCreatePushButton (form2, "applEditAppBrowse", av,ac);

	kids [i++] = save_toggle = 
		XmCreateToggleButtonGadget (form2, "applEditSave", av,ac);

	kids [i++] = unknown_toggle =
		XmCreateToggleButtonGadget (form2, "applEditUnknown", av,ac);
 
	fep->static_desc_label = static_desc_label;
	fep->mime_types_desc_text = mime_types_desc_text;
	fep->mime_types_text = mime_types_text;
	fep->mime_types_suffix_text = mime_types_suffix_text;
	fep->navigator_toggle = navigator_toggle;
	fep->plugin_toggle = plugin_toggle;
	fep->plugin_combo = plugin_combo;
	fep->app_toggle  = app_toggle;
	fep->app_text = app_text;
	fep->app_browse = app_browse;
	fep->save_toggle = save_toggle;
	fep->unknown_toggle = unknown_toggle;
  
	buildPluginList((char *)(fep->cd ? fep->cd->ci.type:NULL));
  
	labels_height = XfeVaGetTallestWidget(plugin_toggle,
										  plugin_combo,
										  app_browse,
										  app_text,
										  NULL);

	labels_width = XfeVaGetWidestWidget(plugin_toggle,
										app_toggle,
										NULL);

	XtVaSetValues (navigator_toggle,
				   XmNindicatorType, XmONE_OF_MANY,
				   XmNheight, labels_height,
				   XmNtopAttachment, XmATTACH_FORM,
				   XmNbottomAttachment, XmATTACH_NONE,
				   XmNleftAttachment, XmATTACH_FORM,
				   XmNrightAttachment, XmATTACH_NONE,
				   XmNsensitive, True,
				   0);

	XtVaSetValues (plugin_toggle,
				   XmNindicatorType, XmONE_OF_MANY,
				   XmNheight, labels_height,
				   XmNtopAttachment, XmATTACH_WIDGET,
				   XmNtopWidget, navigator_toggle,
				   XmNbottomAttachment, XmATTACH_NONE,
				   XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
				   XmNleftWidget, navigator_toggle,
				   XmNrightAttachment, XmATTACH_NONE,
				   0);
 
	XtVaSetValues (plugin_combo,
				   XmNheight, labels_height,
				   XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				   XmNtopWidget, plugin_toggle,
				   XmNbottomAttachment, XmATTACH_NONE,
				   XmNleftAttachment, XmATTACH_FORM,
				   XmNleftOffset, labels_width+8,
				   XmNrightAttachment, XmATTACH_NONE,
				   0);
 
	XtVaSetValues (app_toggle,
				   XmNindicatorType, XmONE_OF_MANY,
				   XmNheight, labels_height,
				   XmNtopAttachment, XmATTACH_WIDGET,
				   XmNtopWidget, plugin_toggle,
				   XmNbottomAttachment, XmATTACH_NONE,
				   XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
				   XmNleftWidget, plugin_toggle,
				   XmNrightAttachment, XmATTACH_NONE,
				   0);
 
	Dimension size;
	Dimension space;
	XtVaGetValues( app_toggle, 
				   XmNindicatorSize, &size, 
				   XmNspacing, &space, 
				   0 );

	XtVaSetValues (app_text,
				   XmNcolumns, 35,
				   XmNheight, labels_height,
				   XmNsensitive, False,
				   XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				   XmNtopWidget, app_toggle,
				   XmNbottomAttachment, XmATTACH_NONE,
				   XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
				   XmNleftWidget, plugin_combo,
				   XmNrightAttachment, XmATTACH_NONE,
				   0);
 
	XtVaSetValues (app_browse,
				   XmNmarginWidth, 8,
				   XmNheight, labels_height,
				   XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				   XmNtopWidget, app_text,
				   XmNbottomAttachment, XmATTACH_NONE,
				   XmNleftAttachment, XmATTACH_WIDGET,
				   XmNleftWidget, app_text,
				   XmNrightAttachment, XmATTACH_NONE,
				   0);
 
	XtVaSetValues (save_toggle,
				   XmNindicatorType, XmONE_OF_MANY,
				   XmNheight, labels_height,
				   XmNtopAttachment, XmATTACH_WIDGET,
				   XmNtopWidget, app_text,
				   XmNbottomAttachment, XmATTACH_NONE,
				   XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
				   XmNleftWidget, app_toggle,
				   XmNrightAttachment, XmATTACH_NONE,
				   0);
 
	XtVaSetValues (unknown_toggle,
				   XmNindicatorType, XmONE_OF_MANY,
				   XmNheight, labels_height,
				   XmNtopAttachment, XmATTACH_WIDGET,
				   XmNtopWidget, save_toggle,
				   XmNbottomAttachment, XmATTACH_NONE,
				   XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
				   XmNleftWidget, save_toggle,
				   XmNrightAttachment, XmATTACH_NONE,
				   0);
 
	XtManageChildren (kids, i);
	XtManageChild(form2);
	XtManageChild(label1);
	XtManageChild(frame1);
 
	XtAddCallback(navigator_toggle,  XmNvalueChangedCallback,
				  prefsApplEditCb_toggleHandledBy, this);
	XtAddCallback(plugin_toggle,  XmNvalueChangedCallback,
				  prefsApplEditCb_toggleHandledBy, this);
	XtAddCallback(app_toggle,  XmNvalueChangedCallback,
				  prefsApplEditCb_toggleHandledBy, this);
	XtAddCallback(save_toggle,  XmNvalueChangedCallback,
				  prefsApplEditCb_toggleHandledBy, this);
	XtAddCallback(unknown_toggle,  XmNvalueChangedCallback,
				  prefsApplEditCb_toggleHandledBy, this);
 
	XtAddCallback(app_browse,  XmNactivateCallback,
				  prefsApplEditCb_browseAppl, this);
 
	XtAddCallback(m_chrome, XmNokCallback, prefsApplEditCb_ok, this);
	XtAddCallback(m_chrome, XmNcancelCallback, prefsApplEditCb_cancel, this);
}

// Member:       ~XFE_PrefsApplEditDialog
// Description:  Destructor
// Inputs:
// Side effects: 

XFE_PrefsApplEditDialog::~XFE_PrefsApplEditDialog()
{
	// Clean up

	delete m_prefsDataApplEdit;
}

// Member:       show
// Description:  Pop up dialog
// Inputs:
// Side effects: 

void XFE_PrefsApplEditDialog::show()
{
	// TODO: Initialize the dialog

	// Manage the top level

	XFE_Dialog::show();

	// Set focus to the OK button

	XmProcessTraversal(m_okButton, XmTRAVERSE_CURRENT);
}

// Member:       initPage
// Description:  Initializes page for MailNewsApplEdit
// Inputs:
// Side effects: 

void XFE_PrefsApplEditDialog::initPage(Boolean newFlag,
									   int     pos)
{
	XP_ASSERT(m_prefsDataApplEdit);

	PrefsDataApplEdit    *fep = m_prefsDataApplEdit;
	PrefsDataGeneralAppl *appl_data = m_prefsDataGeneralAppl;

	fep->pos = pos;
	fep->static_app = False;

	if (newFlag) {
		fep->pos = 0;
		fep->cd = 0;
		XtUnmanageChild(fep->static_desc_label);
	}
	else if (pos < xfe_prefsAppl_get_static_app_count()) {

		// static applications
		fep->static_app = True;
		XtUnmanageChild(fep->mime_types_desc_text);

		XmString xms;
		xms = XmStringCreateLtoR(xfe_prefsAppl_get_static_app_desc(pos),
								 XmFONTLIST_DEFAULT_TAG);
		XtVaSetValues(fep->static_desc_label,
					  XmNlabelString, xms,
					  0 );
		XmStringFree(xms);

		XtSetSensitive(fep->mime_types_text, False);
		XtSetSensitive(fep->mime_types_suffix_text, False);
		XtSetSensitive(fep->navigator_toggle, False);
		XtSetSensitive(fep->unknown_toggle, False);
		XtSetSensitive(fep->plugin_toggle, False);
		XtSetSensitive(fep->save_toggle, False);
		XtSetSensitive(fep->navigator_toggle, False);
		XtVaSetValues(fep->app_toggle,
					  XmNset, True, 
					  XmNsensitive, True,
					  0);

		XtVaSetValues(fep->app_text, XmNsensitive, True, 0);
		fe_SetTextField(fep->app_text,
					  xfe_prefsAppl_get_static_app_command(pos));
	}
	else {
		/* mime types */
		/* Note that fep->pos is adjusted */
		XtUnmanageChild(fep->static_desc_label);
		int adjusted_pos = pos - appl_data->static_apps_count;
		fep->pos = adjusted_pos;
		fep->cd = xfe_prefsDialogAppl_get_mime_data_at_pos(adjusted_pos);

		NET_cdataStruct *cd = fep->cd;

		int i;
		char *extensions = 0; /* has to initialize bf. passing to StrAllocCopy() */

		if ( !fep || !cd->ci.type || !*cd->ci.type ) return;

		/* Found Type */
		XtVaSetValues(fep->mime_types_text, XmNcursorPosition, 0, 0);
		fe_SetTextField(fep->mime_types_text, cd->ci.type);

		/* MIME Description */
		if ( cd->ci.desc && *cd->ci.desc ) {
			XtVaSetValues(fep->mime_types_desc_text, XmNcursorPosition, 0, 0 );
			fe_SetTextField(fep->mime_types_desc_text, cd->ci.desc);
		}

		/* MIME Suffix */
		for ( i = 0; i < cd->num_exts; i++ ) {
			if ( i == 0 ) {
				StrAllocCopy(extensions, cd->exts[i] );
			}
			else {
				extensions = XP_AppendStr(extensions, ",");
				extensions = XP_AppendStr(extensions,  cd->exts[i]);
			}
		}

		if (cd->num_exts > 0 ) {
			XtVaSetValues(fep->mime_types_suffix_text, XmNcursorPosition, 0, 0);
			fe_SetTextField(fep->mime_types_suffix_text, extensions);
		}

		/* Handle By...this mail cap */

		NET_mdataStruct *md = NULL;
		XtVaSetValues(fep->navigator_toggle, XmNsensitive, False, 0);
		/* Netscape Type ?*/
		if (xfe_prefsDialogAppl_handle_by_netscape(cd->ci.type) ){
			XtVaSetValues(fep->navigator_toggle, XmNsensitive, True, 0);
		}

		md = xfe_prefsDialogAppl_get_mailcap_from_type(cd->ci.type);
		if ( md && md->command && *md->command ){
			XtVaSetValues(fep->app_text, XmNsensitive, False, 0 );
			fe_SetTextField(fep->app_text, md->command);
		}
	
		if (md && md->xmode && *md->xmode){
			md->xmode = fe_StringTrim(md->xmode);

			if (!strcmp(md->xmode, NET_COMMAND_UNKNOWN))
				XtVaSetValues(fep->unknown_toggle, XmNset, True, 0 );
			else if (fe_IsMailcapEntryPlugin(md))
				XtVaSetValues(fep->plugin_toggle, XmNset, True, 0 );
			else if (!strcmp(md->xmode, NET_COMMAND_SAVE_TO_DISK) ||
					 (!strcmp(md->xmode, NET_COMMAND_SAVE_BY_NETSCAPE)))
				XtVaSetValues(fep->save_toggle, XmNset, True, 0 );
			else if (!strcmp(md->xmode,NET_COMMAND_NETSCAPE))
				XtVaSetValues(fep->navigator_toggle, XmNset, True, 0 );
			else if (!strcmp(md->xmode,NET_COMMAND_DELETED))
				XtVaSetValues(fep->unknown_toggle, XmNset, True, 0 );
		}
		else if ( md  && md->command && *md->command){
			XtVaSetValues(fep->app_toggle, XmNset, True, 0 );
			XtVaSetValues(fep->app_text,
						  XmNsensitive, True, 0 );
		}
		else if (xfe_prefsDialogAppl_handle_by_netscape(cd->ci.type) )
			XtVaSetValues(fep->navigator_toggle, XmNset, True, 0 );
		else if (xfe_prefsDialogAppl_handle_by_saveToDisk(cd->ci.type) )
			XtVaSetValues(fep->save_toggle, XmNset, True, 0 );
		else
			XtVaSetValues(fep->unknown_toggle, XmNset, True, 0);
	}
}

// Member:       verifyPage
// Description:  verify page for MailNewsApplEdit
// Inputs:
// Side effects: 

Boolean XFE_PrefsApplEditDialog::verifyPage()
{
	return TRUE;
}

// Member:       installChanges
// Description:  install changes for ApplEdit
// Inputs:
// Side effects: 

void XFE_PrefsApplEditDialog::installChanges()
{
}

// Member:       getContext
// Description:  returns context
// Inputs:
// Side effects: 

MWContext *XFE_PrefsApplEditDialog::getContext()
{
	return (m_prefsDialog->getContext());
}

// Member:       getPrefsDialog
// Description:  returns preferences dialog
// Inputs:
// Side effects: 

XFE_PrefsDialog *XFE_PrefsApplEditDialog::getPrefsDialog()
{
	return (m_prefsDialog);
}

// Friend:       prefsApplEditCb_ok
// Description:  
// Inputs:
// Side effects: 

void prefsApplEditCb_ok(Widget    w,
						XtPointer closure,
						XtPointer callData)
{
	XFE_PrefsApplEditDialog  *theDialog = (XFE_PrefsApplEditDialog *)closure;
	XFE_PrefsPageGeneralAppl *thePage = theDialog->m_prefsPageGeneralAppl;
	PrefsDataApplEdit        *fep = theDialog->m_prefsDataApplEdit;
	PrefsDataGeneralAppl     *applData = theDialog->m_prefsDataGeneralAppl;

	XP_ASSERT(fep);
	if (! theDialog->verifyPage()) return;

	char            *text = 0;
	char            *u = 0;
	int              i;
	int              old_pos;
	NET_cdataStruct *old_cd = NULL;
	NET_cdataStruct *new_cd = NULL;
	NET_mdataStruct *md = NULL;
	Boolean          add_new_cd = False;
	char            *src_string =0;
	int              real_pos;

	if (fep->static_app) {
		
		/* Update fe_globalPrefs for static apps */

		text = fe_GetTextField(fep->app_text);
		if (text && *text) {
			char *trimmed_text = fe_StringTrim(text);
			if (XP_STRLEN(trimmed_text) > 0) {

				/* Update the application command in fe_globalPrefs */
				xfe_prefsAppl_set_static_app_command(fep->pos, trimmed_text);

				/* Update the applications pane in prefs dialog */
				char *line = 0;
				line = PR_smprintf("%s|%s", 
								   xfe_prefsAppl_get_static_app_desc(fep->pos), 
								   trimmed_text); 
				XmLGridSetStringsPos(applData->helpers_list, XmCONTENT, fep->pos, 
									 XmCONTENT, 0, line);
				XP_FREEIF(line);
			}
			else {
				FE_Alert(fep->context, XP_GetString(XFE_HELPERS_EMPTY_APP));
			}
		}
		else {
			FE_Alert(fep->context, XP_GetString(XFE_HELPERS_EMPTY_APP));
		}
		XP_FREEIF(text);
	}

	else {

		/* Deal with helper apps handled by the XP */

		thePage->setModified(TRUE);

		/* Type  is critical. If this field is empty, then basically we don't
	   do a thing here */

		text = fe_GetTextField(fep->mime_types_text);
		if ( !text ) return;
		text = fe_StringTrim(text);
		if ( !strlen(text)) {
			FE_Alert(fep->context, XP_GetString(XFE_HELPERS_EMPTY_MIMETYPE));
			XtFree(text); 
			return;
		}

		if(text) XtFree(text);

		/* Empty type Business has been taken care of */
		/* Do some serious stuff here now...*/

		new_cd = NET_cdataCreate();
		new_cd->is_local = True;
		if ( fep->cd ) {
			new_cd->is_external = fep->cd->is_external;
			new_cd->is_modified = 1;
		}
		else {
			new_cd->is_external = 1;
			new_cd->is_modified = 1;

			/* If new entry was deleted before commit, we don't want to save the info out*/
			new_cd->is_new = 1; /* remember this for deletion*/

			add_new_cd = True;
		}

		/* Description */
		text = fe_GetTextField(fep->mime_types_desc_text);
		if ( text && *text ) {
			fe_StringTrim(text);
			StrAllocCopy(new_cd->ci.desc, text);
		}
		if(text) XtFree(text);

		/* Type */
		text = fe_GetTextField(fep->mime_types_text);
		if ( text && *text ){
			fe_StringTrim(text);
			StrAllocCopy( new_cd->ci.type, text);
		}
		if(text) XtFree(text);

		/* Suffix */
		text = fe_GetTextField(fep->mime_types_suffix_text);
		u = XP_STRTOK(text, ",");

		while (u) {
			u = fe_StringTrim(u);
			xfe_prefsDialogAppl_build_exts(u, new_cd);
			u = XP_STRTOK(NULL, ",");
		}
		if(text) XtFree(text);

		if ( (old_cd=xfe_prefsDialogAppl_can_combine (new_cd, &old_pos)) ) {
			if ( ! xfe_prefsDialogAppl_is_deleted_type(old_cd) ) { 
				/* Only cd's that aren't deleted are displayed in the list */
				/* Therefore, we need to make sure if the item is in the list */
				/* in order to delete */

				if ( xfe_prefsDialogAppl_get_mime_data_real_pos(old_cd, &real_pos) )
					old_pos = real_pos;

				if ( old_pos > fep->pos ) 
					XmLGridDeleteRows(applData->helpers_list, XmCONTENT, 
									  old_pos + xfe_prefsAppl_get_static_app_count(), 1);
				else if (fep->pos > old_pos ) {
					XmLGridDeleteRows(applData->helpers_list, XmCONTENT, 
									  old_pos + xfe_prefsAppl_get_static_app_count(), 1);
					fep->pos -= 1;
				}
			}

			/* CanCombine=True will remove the fep->cd from the list 
			 * Therefore, make the new one the current 
			 * If action = add_new_cd, and can_combine = true, we should
			 * remove the old one
			 */
			if ( add_new_cd ) {
				NET_cdataRemove(old_cd);
				old_cd = NULL;
			}
		}

		if ( fep->cd ) /* Then, this is not an action: NEW  */ {

			if ( !fep->cd->is_local && !fep->cd->is_external ) {
				/* read from Global mime.type in old mime format */
				/* convert it to the new format here */
				src_string = xfe_prefsDialogAppl_construct_new_mime_string( new_cd );
			}
			else if ( !fep->cd->is_local && !fep->cd->num_exts ) {
				/* This entry is added by an existing mailcap type */
				src_string = xfe_prefsDialogAppl_construct_new_mime_string(new_cd);
			}
			else	
				src_string = xfe_prefsDialogAppl_replace_new_mime_string( fep->cd, new_cd );

			XP_FREE(fep->cd->src_string);
			fep->cd->src_string = src_string;

			XP_FREE(fep->cd->ci.desc );
			XP_FREE(fep->cd->ci.type );
 
			fep->cd->ci.desc = 0;
			fep->cd->ci.type = 0;
 
			for ( i = 0; i < fep->cd->num_exts; i++ )
				XP_FREE (fep->cd->exts[i]);
			XP_FREE(fep->cd->exts);

			fep->cd->exts = 0;
			fep->cd->num_exts = 0;
			fep->cd->is_external = new_cd->is_external;
			fep->cd->is_modified = new_cd->is_modified;
			StrAllocCopy(fep->cd->ci.desc, new_cd->ci.desc);
			StrAllocCopy(fep->cd->ci.type, new_cd->ci.type);
			for ( i = 0; i < new_cd->num_exts; i++ )
				xfe_prefsDialogAppl_build_exts(new_cd->exts[i], fep->cd);

			NET_cdataFree(new_cd);
		}
		else {
			src_string = xfe_prefsDialogAppl_construct_new_mime_string( new_cd );
			if ( new_cd->src_string) XP_FREE(new_cd->src_string);
			new_cd->src_string = src_string;
			fep->cd = new_cd;
		}

		xfe_prefsDialogAppl_build_handle(fep);
		if ( add_new_cd && (fep->pos == 0)) {
			fep->pos = -1;
			NET_cdataAdd(fep->cd);
		}

		if ( ! xfe_prefsDialogAppl_is_deleted_type(fep->cd) ) {
			md = xfe_prefsDialogAppl_get_mailcap_from_type(fep->cd->ci.type);
			int  position;
			if (fep->pos == (-1)) {
				/* adding this to the list.
				 * This is kinda tricky. We need to add this to the
				 * beginning of mime type list, because the backend does
				 * that in the background.
				 * at the same time, we want to insert it after the static apps
				 */
				position = xfe_prefsAppl_get_static_app_count();
				xfe_prefsDialogAppl_insert_type_at_pos(applData, fep->cd, md, position);
			}
			else {
				/* replacing an existing entry */
				position = fep->pos + xfe_prefsAppl_get_static_app_count();
				xfe_prefsDialogAppl_append_type_to_list(applData, fep->cd, md, position);
			}
		}
	}

	// Simulate a cancel

	prefsApplEditCb_cancel(w, closure, callData);

	// Install preferences

	theDialog->installChanges();

	// Save the preferences at the end, so that if we've found some setting
	// that crashes, it won't get saved...

	if (!XFE_SavePrefs ((char *) fe_globalData.user_prefs_file, &fe_globalPrefs))
		fe_perror(theDialog->getContext(), XP_GetString( XFE_ERROR_SAVING_OPTIONS));
}

// Friend:       prefsApplEditCb_cancel
// Description:  
// Inputs:
// Side effects: 

void prefsApplEditCb_cancel(Widget    /* w */,
							XtPointer closure,
							XtPointer /* callData */)
{
	XFE_PrefsApplEditDialog *theDialog = (XFE_PrefsApplEditDialog *)closure;

	// Delete the dialog

	delete theDialog;
}

// Friend:       prefsApplEditCb_toggleHandledBy
// Description:  
// Inputs:
// Side effects: 

void prefsApplEditCb_toggleHandledBy(Widget    widget,
									 XtPointer closure,
									 XtPointer callData)
{
	XFE_PrefsApplEditDialog      *theDialog = (XFE_PrefsApplEditDialog *)closure;
	PrefsDataApplEdit            *fep = theDialog->m_prefsDataApplEdit;
	XmToggleButtonCallbackStruct *cb = (XmToggleButtonCallbackStruct *)callData;
 
 	if (!cb->set) {
		XtVaSetValues (widget, XmNset, True, 0);
	}
	else if (widget == fep->navigator_toggle) {
		XtVaSetValues (fep->plugin_toggle, XmNset, False, 0);
		XtVaSetValues (fep->app_toggle, XmNset, False, 0);
		XtVaSetValues (fep->save_toggle, XmNset, False, 0);
		XtVaSetValues (fep->unknown_toggle, XmNset, False, 0);
		XtVaSetValues (fep->plugin_combo, XmNsensitive, False, 0);
		XtVaSetValues (fep->app_text, XmNsensitive, False, 0);
	}
	else if (widget == fep->plugin_toggle) {
		if (fep->plugins) {
			int    n;
			char  *pluginName;
			XtVaGetValues(fep->plugin_combo, XmNuserData, &n, 0);
			pluginName = fep->plugins[n];
			theDialog->pluginSelected(pluginName);
		}
		XtVaSetValues (fep->navigator_toggle, XmNset, False, 0);
		XtVaSetValues (fep->app_toggle, XmNset, False, 0);
		XtVaSetValues (fep->save_toggle, XmNset, False, 0);
		XtVaSetValues (fep->unknown_toggle, XmNset, False, 0);
		XtVaSetValues (fep->plugin_combo, XmNsensitive, True, 0);
		XtVaSetValues (fep->app_text, XmNsensitive, False, 0);
	}
	else if (widget == fep->app_toggle) {
		XtVaSetValues (fep->navigator_toggle, XmNset, False, 0);
		XtVaSetValues (fep->plugin_toggle, XmNset, False, 0);
		XtVaSetValues (fep->save_toggle, XmNset, False, 0);
		XtVaSetValues (fep->unknown_toggle, XmNset, False, 0);
		XtVaSetValues (fep->plugin_combo, XmNsensitive, False, 0);
		XtVaSetValues (fep->app_text, XmNsensitive, True, 0);
	}
	else if (widget == fep->save_toggle) {
		XtVaSetValues (fep->navigator_toggle, XmNset, False, 0);
		XtVaSetValues (fep->plugin_toggle, XmNset, False, 0);
		XtVaSetValues (fep->app_toggle, XmNset, False, 0);
		XtVaSetValues (fep->unknown_toggle, XmNset, False, 0);
		XtVaSetValues (fep->plugin_combo, XmNsensitive, False, 0);
		XtVaSetValues (fep->app_text, XmNsensitive, False, 0);
	}
	else if (widget == fep->unknown_toggle) {
		XtVaSetValues (fep->navigator_toggle, XmNset, False, 0);
		XtVaSetValues (fep->plugin_toggle, XmNset, False, 0);
		XtVaSetValues (fep->app_toggle, XmNset, False, 0);
		XtVaSetValues (fep->save_toggle, XmNset, False, 0);
		XtVaSetValues (fep->plugin_combo, XmNsensitive, False, 0);
		XtVaSetValues (fep->app_text, XmNsensitive, False, 0);
	}
	else
		abort ();
}

// Friend:       prefsApplEditCb_browseAppl
// Description:  
// Inputs:
// Side effects: 

void prefsApplEditCb_browseAppl(Widget    /* w */,
								XtPointer closure,
								XtPointer /* callData */)
{
	XFE_PrefsApplEditDialog   *theDialog = (XFE_PrefsApplEditDialog *)closure;
	PrefsDataApplEdit         *fep = theDialog->m_prefsDataApplEdit;

	fe_browse_file_of_text(theDialog->getContext(), fep->app_text, False);
}

// Friend:       prefsApplEditCb_setHandledBy
// Description:  
// Inputs:
// Side effects: 

void prefsApplEditCb_setHandledBy(Widget    /* w */,
								  XtPointer closure,
								  XtPointer /* callData */)
{
	XFE_PrefsApplEditDialog   *theDialog = (XFE_PrefsApplEditDialog *)closure;
	PrefsDataApplEdit *fep = theDialog->m_prefsDataApplEdit;
	char              *text = NULL;
	Bool               nav_p = False;
	Bool               plugin_p = False;

	text = fe_GetTextField(fep->mime_types_text);

	if (text && *text && xfe_prefsDialogAppl_handle_by_netscape(text)) {
		XtVaSetValues(fep->navigator_toggle, XmNsensitive, True, 0);
		nav_p = True;
	}
	else
		XtVaSetValues(fep->navigator_toggle, XmNsensitive, False, 0);

	if (text && *text && theDialog->handleByPlugin(text))
		/* fe_helpers_handle_by_plugin() will sensitize the plugin button. */
		plugin_p = True;
  
	/* if Navigator used to handle this but cannot anymore,
	 * unknown takes over 
	 */
	if (XmToggleButtonGetState(fep->navigator_toggle) && !nav_p)
		XmToggleButtonSetState(fep->unknown_toggle, True, True);

	/* if Plugin used to handle this but cannot anymore, unknown takes over */
	if (XmToggleButtonGetState(fep->plugin_toggle) && !plugin_p)
		XmToggleButtonSetState(fep->unknown_toggle, True, True);

	if (text) XtFree(text);
}

void prefsApplEditCb_selectPlugin(Widget    /* widget */,
								  XtPointer closure,
								  XtPointer call_data)
{
	XFE_PrefsApplEditDialog   *theDialog = (XFE_PrefsApplEditDialog *)closure;
	PrefsDataApplEdit         *fep = theDialog->m_prefsDataApplEdit;
	DtComboBoxCallbackStruct  *cbs = (DtComboBoxCallbackStruct *)call_data;
	char                      *pluginName;

	if (!XmToggleButtonGetState(fep->plugin_toggle)) return;

	pluginName = fep->plugins[cbs->item_position];
	XtVaSetValues(fep->plugin_combo, XmNuserData, cbs->item_position, 0);
	theDialog->pluginSelected(pluginName);
}

// ==================== Static Member Functions ====================

// Member:       buildPluginList
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsApplEditDialog::buildPluginList(char *mimeType)
{
	int               i;
	char            **plugins = NULL;
	char             *current_plugin = NULL;
	NET_mdataStruct  *md = NULL;
	XmString          xms;

	PrefsDataApplEdit *fep = m_prefsDataApplEdit;
	if (!fep) return;

	if (mimeType  && *mimeType)
		plugins = NPL_FindPluginsForType(mimeType);
  
	/* Destroy all existing children */
	if (fep->plugin_combo) {
		int   count;
		XtVaGetValues(fep->plugin_combo, XmNitemCount, &count, 0);
		if (count > 0) DtComboBoxDeleteAllItems(fep->plugin_combo);
	}
  
	if (plugins && plugins[0]) {
		md = xfe_prefsDialogAppl_get_mailcap_from_type(mimeType);
		if (fe_IsMailcapEntryPlugin(md))
			current_plugin = fe_GetPluginNameFromMailcapEntry(md);
    
		i = 0;
		while (plugins[i]) {
			xms = XmStringCreateLtoR(plugins[i], XmFONTLIST_DEFAULT_TAG);
			DtComboBoxAddItem(fep->plugin_combo, xms, 0, True);
			if ((i == 0) || 
				(!strcmp(current_plugin, plugins[i])))
				DtComboBoxSelectItem(fep->plugin_combo, xms);
			XmStringFree(xms);
			i++;
		}
		XtVaSetValues(fep->plugin_combo, XmNsensitive, True, 0);
		XtVaSetValues(fep->plugin_toggle, XmNsensitive, True, 0);
	}
	else {
		// No plugins installed for this mime type
		XmString xms;
		xms = XmStringCreateLtoR(" ", XmFONTLIST_DEFAULT_TAG);
		XtVaSetValues(fep->plugin_combo,
					  XmNlabelString, xms,
					  0);
		XmStringFree(xms);
		XtVaSetValues(fep->plugin_toggle, XmNsensitive, False, 0);
	}

	fep->plugins = plugins;
}

// Member:       buildPluginList
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsApplEditDialog::pluginSelected(char *pluginName)
{
	PrefsDataApplEdit *fep = m_prefsDataApplEdit;

	/* Change the description and extensions to what the plugins says */
	char *mimetype, *desc=NULL, *ext=NULL;
	char *cur_desc=NULL, *cur_ext=NULL;
	XP_Bool differ = True;

	mimetype = fe_GetTextField(fep->mime_types_text);

	if (fe_GetMimetypeInfoFromPlugin(pluginName, mimetype, &desc, &ext) >= 0) {
		/* If either the description or the extension is different from what is
		 * already being displayed, ask the user whether to revert to what the
		 * plugin says.
		 */
		cur_desc = fe_GetTextField(fep->mime_types_desc_text);
		cur_ext = fe_GetTextField(fep->mime_types_suffix_text);
     
		if (desc) differ = strcmp(desc, cur_desc);
		else differ = *cur_desc;
     
		if (!differ)
			if (ext) differ = strcmp(ext, cur_ext);
			else differ = *cur_ext;
		
		if (differ) {
			char *buf;
			buf = PR_smprintf(XP_GetString(XFE_HELPERS_PLUGIN_DESC_CHANGE),
							  mimetype, desc?desc:"", ext?ext:"");
			if (FE_Confirm(fep->context, buf)) {
				XtVaSetValues(fep->mime_types_desc_text, XmNcursorPosition, 0,
							  0);
				fe_SetTextField(fep->mime_types_desc_text, desc);
				XtVaSetValues(fep->mime_types_suffix_text, XmNcursorPosition, 0,
							  0);
				fe_SetTextField(fep->mime_types_suffix_text, ext);
			}
			if (buf) XP_FREE(buf);
		}
	}

	if (mimetype) XP_FREE(mimetype);
	if (cur_desc) XP_FREE(cur_desc);
	if (cur_ext) XP_FREE(cur_ext);
}

XP_Bool XFE_PrefsApplEditDialog::handleByPlugin(char *type)
{
	PrefsDataApplEdit *fep = m_prefsDataApplEdit;
	Bool b;

	/* Create plugins options menu and sensitize the plugin option menu and
	   toggle button, depending on the mimetype passed in */

	buildPluginList(type);
	XtVaGetValues(fep->plugin_toggle, XmNsensitive, &b, 0);
	return(b);
}

