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
   PrefsDialogMailnews.cpp -- the Mail/News page in XFE preferences dialogs
   Created: Linda Wei <lwei@netscape.com>, 17-Sep-96.
 */

#include "rosetta.h"
#include "MozillaApp.h"
#include "felocale.h"
#include "structs.h"
#include "fonts.h"
#include "xpassert.h"
#include "xfe.h"
#include "msgcom.h"
#include "e_kit.h"
#include "prprf.h"
extern "C" {
#include "prnetdb.h"
}
#include "PrefsDialog.h"
#ifdef MOZ_MAIL_NEWS
#include "ColorDialog.h"
#include "dirprefs.h"
#include "addrbk.h"
#include "xp_list.h"
#endif
#include "prefapi.h"
#include "xp_mem.h"

#include <Xm/DrawnB.h>
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
#include <DtWidgets/ComboBox.h>
#include <Xfe/Xfe.h>

#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN 64
#endif

#define COLOR_STRING_BLACK "black"

extern int XFE_EMPTY_DIR;
extern int XFE_PREFS_MAIL_FOLDER_SENT;
extern int XFE_WARNING;
extern int XFE_HOST_IS_UNKNOWN;
extern int XFE_NEWS_DIR;
extern int XFE_MAIL_HOST;
extern int XFE_NEWS_HOST;
extern int XFE_DIR_DOES_NOT_EXIST;
extern int XFE_PREF_STYLE;
extern int XFE_PREF_SIZE;
extern int XFE_OUT_OF_MEMORY_URL;
extern int XFE_PREFS_CR_CARD;
extern int XFE_PREFS_LABEL_STYLE_PLAIN;
extern int XFE_PREFS_LABEL_STYLE_BOLD;
extern int XFE_PREFS_LABEL_STYLE_ITALIC;
extern int XFE_PREFS_LABEL_STYLE_BOLDITALIC;
extern int XFE_PREFS_LABEL_SIZE_NORMAL;
extern int XFE_PREFS_LABEL_SIZE_BIGGER;
extern int XFE_PREFS_LABEL_SIZE_SMALLER;
extern int XFE_EMPTY_EMAIL_ADDR;
extern int XFE_FOLDER_ON_SERVER_FORMAT;
extern int XFE_DRAFTS_ON_SERVER;
extern int XFE_TEMPLATES_ON_SERVER;
extern int MK_MSG_MAIL_DIRECTORY_CHANGED;
extern int XFE_MAIL_SELF_FORMAT;
extern int MK_MSG_SENT_L10N_NAME;
extern int MK_MSG_DRAFTS_L10N_NAME;
extern int MK_MSG_TEMPLATES_L10N_NAME;
extern int MK_POP3_ONLY_ONE;
extern int MK_MSG_REMOVE_MAILHOST_CONFIRM;

#define OUTLINER_GEOMETRY_PREF "preferences.dir.outliner_geometry"

extern "C"
{
  
#ifdef MOZ_MAIL_NEWS
	void      MIME_ConformToStandard(XP_Bool conform_p);
	void      MSG_SetBiffStatFile(const char* filename);
	void      MSG_SetFolderDirectory(MSG_Prefs* prefs, const char* directory);
	void      NET_SetMailRelayHost(char * host);
	void      NET_SetNewsHost(const char * hostname);
	void      NET_SetPopUsername(const char *username);
	void      fe_browse_file_of_text(MWContext *context, Widget text_field, Boolean dirp);
	void      fe_installMailNews();
	void      fe_installMailNewsMserver();
	XP_List  *FE_GetDirServers();
	ABook    *fe_GetABook(MWContext *context);
	void      fe_showABCardPropertyDlg(Widget parent,
									   MWContext *context,
									   ABID entry,
									   XP_Bool newuser);
	void      fe_SwatchSetColor(Widget widget, LO_Color* color);

	static void fe_set_quoted_text_styles(PrefsDataMailNews *);
	static void fe_set_quoted_text_sizes(PrefsDataMailNews *);
#endif // MOZ_MAIL_NEWS
	char     *XP_GetString(int i);
	void      fe_installMailNewsIdentity();
}

#ifdef MOZ_MAIL_NEWS
struct _quotedTextStyle {
	MSG_FONT               style;
	int                   *defaultLabelId;
} quotedTextStyles[] = {
	{
		MSG_PlainFont,
		&XFE_PREFS_LABEL_STYLE_PLAIN,
	},
	{
		MSG_BoldFont,
		&XFE_PREFS_LABEL_STYLE_BOLD,
	},
	{
		MSG_ItalicFont,
		&XFE_PREFS_LABEL_STYLE_ITALIC,
	},
	{
		MSG_BoldItalicFont,
		&XFE_PREFS_LABEL_STYLE_BOLDITALIC,
	},
};

struct _quotedTextSize {
	MSG_CITATION_SIZE      size;
	int                   *defaultLabelId;
} quotedTextSizes[] = {
	{
		MSG_NormalSize,
		&XFE_PREFS_LABEL_SIZE_NORMAL,
	},
	{
		MSG_Bigger,
		&XFE_PREFS_LABEL_SIZE_BIGGER,
	},
	{
		MSG_Smaller,
		&XFE_PREFS_LABEL_SIZE_SMALLER,
	},
};
#endif // MOZ_MAIL_NEWS

#ifdef MOZ_MAIL_NEWS
// ************************************************************************
// ************************      Mail News        *************************
// ************************************************************************

// Member:       XFE_PrefsPageMailNews
// Description:  Constructor
// Inputs:
// Side effects: 

XFE_PrefsPageMailNews::XFE_PrefsPageMailNews(XFE_PrefsDialog *dialog)
	: XFE_PrefsPage(dialog),
	  m_prefsDataMailNews(0),
	  m_refresh_needed(FALSE)
{
}

// Member:       ~XFE_PrefsPageMailNews
// Description:  Destructor
// Inputs:
// Side effects: 

XFE_PrefsPageMailNews::~XFE_PrefsPageMailNews()
{
	PrefsDataMailNews     *fep = m_prefsDataMailNews;
	char                  *orig_color_name;

	XtVaGetValues(fep->quoted_text_color_button,
				  XmNuserData, &orig_color_name,
				  NULL);
	if (orig_color_name) XtFree(orig_color_name);

	delete m_prefsDataMailNews;
}

// Member:       create
// Description:  Creates page for MailNews/
// Inputs:
// Side effects: 

void XFE_PrefsPageMailNews::create()
{
	Widget            kids[100];
	Arg               av[50];
	int               ac;
	int               i;

	PrefsDataMailNews *fep = NULL;

	fep = new PrefsDataMailNews;
	memset(fep, 0, sizeof(PrefsDataMailNews));
	m_prefsDataMailNews = fep;

	fep->context = getContext();
	fep->prompt_dialog = getPrefsDialog()->getDialogChrome();

	Widget form;

	ac = 0;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	form = XmCreateForm (m_wPageForm, "mailnews", av, ac);
	XtManageChild (form);
	m_wPage = fep->page = form;

	// plain quoted text

	Widget frame1;

	ac = 0;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_NONE); ac++;
	frame1 = XmCreateFrame (form, "quotedFrame", av, ac);

	Widget form1;

	ac = 0;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
	form1 = XmCreateForm (frame1, "quotedBox", av, ac);

	Widget label1;

	ac = 0;
	XtSetArg (av [ac], XmNchildType, XmFRAME_TITLE_CHILD); ac++;
	label1 = XmCreateLabelGadget (frame1, "quotedTextLabel", av, ac);

	Visual    *v = 0;
	Colormap   cmap = 0;
	Cardinal   depth = 0;

	i = 0;
	ac = 0;

	XtVaGetValues (getPrefsDialog()->getPrefsParent(),
				   XtNvisual, &v,
				   XtNcolormap, &cmap,
				   XtNdepth, &depth, 
				   0);

	// Quoted text - style, size, color

	Widget quoted_text_style_label;

	ac = 0;
	kids[i++] = quoted_text_style_label =
		XmCreateLabelGadget(form1, "quotedTextStyleLabel", av, ac);

	ac = 0;
	XtSetArg(av[ac], XmNvisual, v); ac++;
	XtSetArg(av[ac], XmNdepth, depth); ac++;
	XtSetArg(av[ac], XmNcolormap, cmap); ac++;
	fep->quoted_text_style_pulldown = 
		XmCreatePulldownMenu (form1, "quotedTextStylePullDown", av, ac);

	ac = 0;
	XtSetArg (av [ac], XmNsubMenuId, fep->quoted_text_style_pulldown); ac++;
	kids[i++] = fep->quoted_text_style_option = 
		fe_CreateOptionMenu (form1, "quotedTextStyleMenu", av, ac);
	fe_set_quoted_text_styles(fep);
	fe_UnmanageChild_safe(XmOptionLabelGadget(fep->quoted_text_style_option));

	Widget quoted_text_size_label;

	ac = 0;
	kids[i++] = quoted_text_size_label = 
		XmCreateLabelGadget (form1, "quotedTextSizeLabel", av, ac);

	ac = 0;
	XtSetArg (av[ac], XmNvisual, v); ac++;
	XtSetArg (av[ac], XmNdepth, depth); ac++;
	XtSetArg (av[ac], XmNcolormap, cmap); ac++;
	fep->quoted_text_size_pulldown = 
		XmCreatePulldownMenu (form1, "quotedTextSizePullDown", av, ac);

	ac = 0;
	XtSetArg (av [ac], XmNsubMenuId, fep->quoted_text_size_pulldown); ac++;
	kids[i++] = fep->quoted_text_size_option =
		fe_CreateOptionMenu (form1, "quotedTextSizeMenu", av, ac);
	fe_set_quoted_text_sizes(fep);
	fe_UnmanageChild_safe(XmOptionLabelGadget(fep->quoted_text_size_option));

	// color button

	Widget quoted_text_color_label;

	ac = 0;
	kids[i++] = quoted_text_color_label =
		XmCreateLabelGadget(form1, "quotedTextColorLabel", av, ac);

	ac = 0;
	kids[i++] = fep->quoted_text_color_button =
		XmCreateDrawnButton(form1, "quotedTextColorButton", av, ac);

	int labels_width;
	labels_width = XfeVaGetWidestWidget(quoted_text_style_label,
										quoted_text_size_label,
										quoted_text_color_label,
										NULL);
	int labels_height;
	labels_height = XfeVaGetTallestWidget(quoted_text_style_label,
										  fep->quoted_text_style_option,
										  fep->quoted_text_color_button,
										  NULL);

	XtVaSetValues (quoted_text_style_label,
				   XmNheight, labels_height,
				   XmNleftAttachment, XmATTACH_NONE,
				   RIGHT_JUSTIFY_VA_ARGS(quoted_text_style_label,labels_width),
				   XmNtopAttachment, XmATTACH_FORM,
				   XmNbottomAttachment, XmATTACH_NONE,
				   XmNrightAttachment, XmATTACH_NONE,
				   0);

	XtVaSetValues (fep->quoted_text_style_option,
				   XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				   XmNtopWidget, quoted_text_style_label,
				   XmNtopOffset, (-5),
				   XmNbottomAttachment, XmATTACH_NONE,
				   XmNleftAttachment, XmATTACH_WIDGET,
				   XmNleftWidget, quoted_text_style_label,
				   XmNrightAttachment, XmATTACH_NONE,
				   0);

	XtVaSetValues (quoted_text_size_label,
				   XmNheight, labels_height,
				   XmNtopAttachment, XmATTACH_WIDGET,
				   XmNtopWidget, fep->quoted_text_style_option,
				   XmNtopOffset, 4,
				   XmNbottomAttachment, XmATTACH_NONE,
				   XmNleftAttachment, XmATTACH_NONE,
				   XmNrightAttachment, XmATTACH_OPPOSITE_WIDGET,
				   XmNrightWidget, quoted_text_style_label,
				   0);

	XtVaSetValues (fep->quoted_text_size_option,
				   XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				   XmNtopWidget, quoted_text_size_label,
				   XmNtopOffset, (-5),
				   XmNbottomAttachment, XmATTACH_NONE,
				   XmNleftAttachment, XmATTACH_WIDGET,
				   XmNleftWidget, quoted_text_size_label,
				   XmNrightAttachment, XmATTACH_NONE,
				   0);

	XtVaSetValues (quoted_text_color_label,
				   XmNheight, labels_height,
				   XmNtopAttachment, XmATTACH_WIDGET,
				   XmNtopWidget, fep->quoted_text_size_option,
				   XmNtopOffset, 4,
				   XmNbottomAttachment, XmATTACH_NONE,
				   XmNleftAttachment, XmATTACH_NONE,
				   XmNrightAttachment, XmATTACH_OPPOSITE_WIDGET,
				   XmNrightWidget, quoted_text_style_label,
				   0);

	XtVaSetValues (fep->quoted_text_color_button,
				   XmNwidth, 40,
				   XmNheight, 20,
				   XmNshadowThickness, 2,
				   XmNuserData, 0,
				   XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				   XmNtopWidget, quoted_text_color_label,
				   XmNtopOffset, 2,
				   XmNbottomAttachment, XmATTACH_NONE,
				   XmNleftAttachment, XmATTACH_WIDGET,
				   XmNleftWidget, quoted_text_color_label,
				   XmNrightAttachment, XmATTACH_NONE,
				   0);

	XtManageChildren(kids, i);
	XtManageChild(label1);
	XtManageChild(form1);
	XtManageChild(frame1);

	// Display messages/articles with fixed/variable width font

	Widget frame2;

	ac = 0;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
	XtSetArg (av [ac], XmNtopWidget, frame1); ac++;
	XtSetArg (av [ac], XmNtopOffset, 8); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_NONE); ac++;
	frame2 = XmCreateFrame (form, "dpyMsgFrame", av, ac);

	Widget form2;

	ac = 0;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
	form2 = XmCreateForm (frame2, "dpyMsgBox", av, ac);

	Widget label2;

	ac = 0;
	XtSetArg (av [ac], XmNchildType, XmFRAME_TITLE_CHILD); ac++;
	label2 = XmCreateLabelGadget (frame2, "msgArticleLabel", av, ac);

	i = 0;
	ac = 0;

	kids[i++] = fep->fixed_width_font_toggle =
		XmCreateToggleButtonGadget(form2, "fixedWidthFont", av, ac);

	ac = 0;
	kids[i++] = fep->var_width_font_toggle =
		XmCreateToggleButtonGadget(form2, "varWidthFont", av, ac);

	XtVaSetValues (fep->fixed_width_font_toggle,
				   XmNindicatorType, XmONE_OF_MANY,
				   XmNtopAttachment, XmATTACH_FORM,
				   XmNbottomAttachment, XmATTACH_NONE,
				   XmNleftAttachment, XmATTACH_FORM,
				   XmNleftOffset, 16,
				   XmNrightAttachment, XmATTACH_NONE,
				   0);

	XtVaSetValues (fep->var_width_font_toggle,
				   XmNindicatorType, XmONE_OF_MANY,
				   XmNtopAttachment, XmATTACH_WIDGET,
				   XmNtopWidget, fep->fixed_width_font_toggle,
				   XmNbottomAttachment, XmATTACH_NONE,
				   XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
				   XmNleftWidget, fep->fixed_width_font_toggle,
				   XmNrightAttachment, XmATTACH_NONE,
				   0);

	XtManageChildren(kids, i);
	XtManageChild(label2);
	XtManageChild(form2);
	XtManageChild(frame2);

	// Frame3

	Widget frame3;

	ac = 0;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
	XtSetArg (av [ac], XmNtopWidget, frame2); ac++;
	XtSetArg (av [ac], XmNtopOffset, 8); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_NONE); ac++;
	frame3 = XmCreateFrame (form, "frame3", av, ac);

	Widget form3;

	ac = 0;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
	form3 = XmCreateForm (frame3, "frame3Box", av, ac);

	ac = 0;
	i = 0;

	kids[i++] = fep->reuse_thread_window_toggle =
		XmCreateToggleButtonGadget(form3, "reuseThread", av, ac);

	kids[i++] = fep->reuse_message_window_toggle =
		XmCreateToggleButtonGadget(form3, "reuseMsg", av, ac);

	XtVaSetValues (fep->reuse_thread_window_toggle,
				   XmNindicatorType, XmN_OF_MANY,
				   XmNalignment, XmALIGNMENT_BEGINNING,
				   XmNtopAttachment, XmATTACH_FORM,
				   XmNbottomAttachment, XmATTACH_NONE,
				   XmNleftAttachment, XmATTACH_FORM,
				   XmNrightAttachment, XmATTACH_NONE,
				   0);

	XtVaSetValues (fep->reuse_message_window_toggle,
				   XmNindicatorType, XmN_OF_MANY,
				   XmNalignment, XmALIGNMENT_BEGINNING,
				   XmNtopAttachment, XmATTACH_WIDGET,
				   XmNtopWidget, fep->reuse_thread_window_toggle,
				   XmNbottomAttachment, XmATTACH_NONE,
				   XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
				   XmNleftWidget, fep->reuse_thread_window_toggle,
				   XmNrightAttachment, XmATTACH_NONE,
				   0);

	XtManageChildren(kids, i);
	XtManageChild(form3);
	XtManageChild(frame3);

	XtAddCallback(fep->quoted_text_color_button, XmNactivateCallback, 
				  cb_quotedTextColor, this);

	XtAddCallback(fep->fixed_width_font_toggle, XmNvalueChangedCallback,
				  cb_toggleMsgFont, fep);
	XtAddCallback(fep->var_width_font_toggle, XmNvalueChangedCallback,
				  cb_toggleMsgFont, fep);

	setCreated(TRUE);
}

// Member:       init
// Description:  Initializes page for MailNews
// Inputs:
// Side effects: 

void XFE_PrefsPageMailNews::init()
{
	XP_ASSERT(m_prefsDataMailNews);
	
	PrefsDataMailNews  *fep = m_prefsDataMailNews;
	XFE_GlobalPrefs    *prefs = &fe_globalPrefs;
	
	// Plain quoted text

	// quoted_text_style_option, initialized in fe_set_quoted_text_styles()
	// quoted_text_size_option, initialized in fe_set_quoted_text_sizes()

	// quoted text color

	LO_Color  color;
	char     *color_string = 0;
	char     *orig_citation_color;
	char      buf[32];
    Boolean   sensitive;

	color_string = ((prefs->citation_color) && (XP_STRLEN(prefs->citation_color) > 0)) ?
		prefs->citation_color : COLOR_STRING_BLACK;
	if (! LO_ParseRGB(color_string, &color.red, &color.green, &color.blue))
		LO_ParseRGB(COLOR_STRING_BLACK, &color.red, &color.green, &color.blue);
    PR_snprintf(buf, sizeof(buf), "#%02x%02x%02x",
                color.red, color.green, color.blue);

	XtVaGetValues(fep->quoted_text_color_button, XmNuserData, &orig_citation_color, NULL);
	if (orig_citation_color) XtFree(orig_citation_color);
	XtVaSetValues(fep->quoted_text_color_button,
				  XmNuserData, XtNewString(buf),
				  NULL);
	fe_SwatchSetColor(fep->quoted_text_color_button, &color);

    // If pref is locked, grey it out.

    XtSetSensitive(fep->quoted_text_style_pulldown,
				   !PREF_PrefIsLocked("mail.quoted_style"));
    XtSetSensitive(fep->quoted_text_size_pulldown, 
                   !PREF_PrefIsLocked("mail.quoted_size"));
    XtSetSensitive(fep->quoted_text_color_button, 
                   !PREF_PrefIsLocked("mail.citation_color"));

	// fixed/var width font

    sensitive = !PREF_PrefIsLocked("mail.fixed_width_messages");
	XtVaSetValues (fep->fixed_width_font_toggle,
				   XmNset, prefs->fixed_message_font_p, 
				   XmNsensitive, sensitive,
                   0);
	XtVaSetValues (fep->var_width_font_toggle,
				   XmNset, !prefs->fixed_message_font_p, 
				   XmNsensitive, sensitive,
                   0);

	// Reuse thread/msg window

	XtVaSetValues(fep->reuse_thread_window_toggle, 
				  XmNset, prefs->reuse_thread_window,
				  XmNsensitive, !PREF_PrefIsLocked("mailnews.reuse_thread_window"),
				  0);

	XtVaSetValues(fep->reuse_message_window_toggle,
				  XmNset, prefs->reuse_msg_window,
				  XmNsensitive, !PREF_PrefIsLocked("mailnews.reuse_message_window"),
				  0);

	setInitialized(TRUE);
}

// Member:       install
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageMailNews::install()
{
	fe_installMailNews();

	if (m_refresh_needed) {
		// Need to refresh message window
		XFE_MozillaApp::theApp()->notifyInterested(XFE_MozillaApp::refreshMsgWindow);
		m_refresh_needed = FALSE;
	}
}

// Member:       save
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageMailNews::save()
{
	PrefsDataMailNews *fep = m_prefsDataMailNews;
	Boolean            b;
	
	XP_ASSERT(fep);
	if (! verify()) return;

	// quoted text style

	Widget     button;
	MSG_FONT   style;
	MSG_FONT   old_style;

	old_style = fe_globalPrefs.citation_font;
	XtVaGetValues(fep->quoted_text_style_option, XmNmenuHistory, &button, NULL);
	XtVaGetValues(button, XmNuserData, &style, NULL);
	fe_globalPrefs.citation_font = style;
	if (old_style != style) m_refresh_needed = TRUE;

	// quoted text size

	MSG_CITATION_SIZE   size;
	MSG_CITATION_SIZE   old_size;

	old_size = fe_globalPrefs.citation_size;
	XtVaGetValues(fep->quoted_text_size_option, XmNmenuHistory, &button, NULL);
	XtVaGetValues(button, XmNuserData, &size, NULL);
	fe_globalPrefs.citation_size = size;
	if (old_size != size) m_refresh_needed = TRUE;

	// quoted_text_color_button;

	char *citation_color;
	citation_color = fe_globalPrefs.citation_color;
	char *selected_citation_color;
	XtVaGetValues(fep->quoted_text_color_button, XmNuserData, &selected_citation_color, NULL);
	fe_globalPrefs.citation_color = (char *)XP_STRDUP(selected_citation_color);
	if (citation_color && fe_globalPrefs.citation_color &&
		XP_STRCASECMP(citation_color, fe_globalPrefs.citation_color))
		m_refresh_needed = TRUE;
	if (citation_color) XP_FREE(citation_color);

	// Fixed/var width font

	XtVaGetValues (fep->fixed_width_font_toggle, XmNset, &b, 0);
	if (b != fe_globalPrefs.fixed_message_font_p) m_refresh_needed = TRUE;
	fe_globalPrefs.fixed_message_font_p = b;

	// Reuse thread/msg window

	XtVaGetValues(fep->reuse_thread_window_toggle, XmNset, &b, 0);
	fe_globalPrefs.reuse_thread_window = b;

	XtVaGetValues(fep->reuse_message_window_toggle, XmNset, &b, 0);
	fe_globalPrefs.reuse_msg_window = b;

	// Install preferences

	install();
}

// Member:       getData
// Description:  
// Inputs:
// Side effects: 

PrefsDataMailNews *XFE_PrefsPageMailNews::getData()
{
	return m_prefsDataMailNews;
}

// Member:       prefsMailNewsCompositionCb_toggleMsgFont
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageMailNews::cb_toggleMsgFont(Widget    w,
											 XtPointer closure,
											 XtPointer callData)
{
	XmToggleButtonCallbackStruct *cb = (XmToggleButtonCallbackStruct *)callData;
	PrefsDataMailNews            *fep = (PrefsDataMailNews *)closure;

	if (! cb->set) {
		XtVaSetValues(w, XmNset, True, 0);
	}
	else if (w == fep->fixed_width_font_toggle) {
		XtVaSetValues(fep->var_width_font_toggle, XmNset, False, 0);
	}
	else if (w == fep->var_width_font_toggle) {
		XtVaSetValues(fep->fixed_width_font_toggle, XmNset, False, 0);
	}
	else
		abort();	
}

// Member:       cb_quotedTextColor
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageMailNews::cb_quotedTextColor(Widget    /* w */,
											   XtPointer closure,
											   XtPointer /* callData */)
{
	XFE_PrefsPageMailNews *thePage = (XFE_PrefsPageMailNews *)closure;
	XFE_PrefsDialog       *theDialog = thePage->getPrefsDialog();
	PrefsDataMailNews     *fep = thePage->getData();

	// LO_Color color;
	// if (fe_ColorPicker(theDialog->getContext(), &color)) {
	// fe_swatchSetColor(fep->quoted_text_color_button, &color);
	//	}

	char   selected_color_name[128];
	Pixel  selected_pixel;
	char  *orig_color_name;

	XtVaGetValues(fep->quoted_text_color_button,
				  XmNuserData, &orig_color_name,
				  NULL);
	if (orig_color_name != NULL)
		XP_STRCPY(selected_color_name, orig_color_name);
	else
		selected_color_name[0] = '\0';

	if (fe_colorDialog(theDialog->getBaseWidget(), theDialog->getContext(),
					   selected_color_name, &selected_pixel)) {
		if (XP_STRLEN(selected_color_name) > 0) {
			if (orig_color_name &&
				(! XP_STRCMP(orig_color_name, selected_color_name)))
				return;
			if (orig_color_name) XtFree(orig_color_name);
			XtVaSetValues(fep->quoted_text_color_button, 
						  XmNbackground, selected_pixel,
						  XmNuserData, XtNewString(selected_color_name),
						  NULL);
		}
	}
}

static void fe_set_quoted_text_styles(PrefsDataMailNews     *fep)
{
	int              ac;
	int              i;
	int              numStyles;
	Arg              av[50];
	char            *label;
	char             name[32];
	XmString         xms;
	Widget           button;
	XFE_GlobalPrefs *prefs = &fe_globalPrefs;

	numStyles = XtNumber(quotedTextStyles); 
	for (i = 0; i < numStyles; i++)	{
		label = XP_GetString(*quotedTextStyles[i].defaultLabelId);

		ac = 0;
		xms = XmStringCreateLtoR(label, XmFONTLIST_DEFAULT_TAG);
		XtSetArg(av[ac], XmNlabelString, xms); ac++;

		XP_SAFE_SPRINTF(name, sizeof(name), "style%d", i);
		button = XmCreatePushButtonGadget(fep->quoted_text_style_pulldown,
										  name, av, ac);
		XmStringFree(xms);

		XtVaSetValues(button, XmNuserData, quotedTextStyles[i].style, NULL);

		if (quotedTextStyles[i].style == prefs->citation_font)
			XtVaSetValues (fep->quoted_text_style_option, XmNmenuHistory, button, 0);

		XtManageChild(button);
	}
}

static void fe_set_quoted_text_sizes(PrefsDataMailNews     *fep)
{
	int              ac;
	int              i;
	int              numSizes;
	Arg              av[50];
	char            *label;
	char             name[32];
	XmString         xms;
	Widget           button;
	XFE_GlobalPrefs *prefs = &fe_globalPrefs;

	numSizes = XtNumber(quotedTextSizes); 
	for (i = 0; i < numSizes; i++)	{
		label = XP_GetString(*quotedTextSizes[i].defaultLabelId);

		ac = 0;
		xms = XmStringCreateLtoR(label, XmFONTLIST_DEFAULT_TAG);
		XtSetArg(av[ac], XmNlabelString, xms); ac++;

		XP_SAFE_SPRINTF(name, sizeof(name), "size%d", i);
		button = XmCreatePushButtonGadget(fep->quoted_text_size_pulldown,
										  name, av, ac);
		XmStringFree(xms);

		XtVaSetValues(button, XmNuserData, quotedTextSizes[i].size, NULL);

		if (quotedTextSizes[i].size == prefs->citation_size)
			XtVaSetValues (fep->quoted_text_size_option, XmNmenuHistory, button, 0);

		XtManageChild(button);
	}
}
#endif // MOZ_MAIL_NEWS

// ************************************************************************
// ************************  Mail News/Identity   *************************
// ************************************************************************

// Member:       XFE_PrefsPageMailNewsIdentity
// Description:  Constructor
// Inputs:
// Side effects: 

XFE_PrefsPageMailNewsIdentity::XFE_PrefsPageMailNewsIdentity(XFE_PrefsDialog *dialog)
	: XFE_PrefsPage(dialog),
	  m_prefsDataMailNewsIdentity(0)
{
}

// Member:       ~XFE_PrefsPageMailNewsIdentity
// Description:  Destructor
// Inputs:
// Side effects: 

XFE_PrefsPageMailNewsIdentity::~XFE_PrefsPageMailNewsIdentity()
{
	delete m_prefsDataMailNewsIdentity;
}

// Member:       create
// Description:  Creates page for MailNews/Identity
// Inputs:
// Side effects: 

void XFE_PrefsPageMailNewsIdentity::create()
{
	Widget            kids[100];
	Arg               av[50];
	int               ac;
	int               i;

	PrefsDataMailNewsIdentity *fep = NULL;

	fep = new PrefsDataMailNewsIdentity;
	memset(fep, 0, sizeof(PrefsDataMailNewsIdentity));
	m_prefsDataMailNewsIdentity = fep;

	fep->context = getContext();
	fep->prompt_dialog = getPrefsDialog()->getDialogChrome();

	Widget form;

	ac = 0;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	form = XmCreateForm (m_wPageForm, "mailnewsIdentity", av, ac);
	XtManageChild (form);
	m_wPage = fep->page = form;

	Widget frame1;

	ac = 0;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_NONE); ac++;
	frame1 = XmCreateFrame (form, "mnIdFrame", av, ac);

	Widget form1;

	ac = 0;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
	form1 = XmCreateForm (frame1, "mnIdBox", av, ac);

	Widget id_label;
	Widget name_label;
	Widget email_addr_label;
	Widget org_label;
#ifdef MOZ_MAIL_NEWS
	Widget reply_to_addr_label;
	Widget sig_file_label;
	Widget browse_button;
#endif // MOZ_MAIL_NEWS

	ac = 0;
	i = 0;

	kids[i++] = id_label = 
		XmCreateLabelGadget(form1, "idLabel", av, ac);

	kids[i++] = name_label = 
		XmCreateLabelGadget(form1, "nameLabel", av, ac);

	kids[i++] = email_addr_label = 
		XmCreateLabelGadget(form1, "emailAddrLabel", av, ac);

#ifdef MOZ_MAIL_NEWS
	kids[i++] = reply_to_addr_label = 
		XmCreateLabelGadget(form1, "replyToAddrLabel", av, ac);
#endif // MOZ_MAIL_NEWS

	kids[i++] = org_label = 
		XmCreateLabelGadget(form1, "orgLabel", av, ac);

#ifdef MOZ_MAIL_NEWS
	kids[i++] = sig_file_label = 
		XmCreateLabelGadget(form1, "sigFileLabel", av, ac);
#endif // MOZ_MAIL_NEWS

	kids[i++] = fep->name_text =
		fe_CreateTextField(form1, "nameText", av, ac);

	kids[i++] = fep->email_addr_text =
		fe_CreateTextField(form1, "emailAddrText", av, ac);

#ifdef MOZ_MAIL_NEWS
	kids[i++] = fep->reply_to_addr_text =
		fe_CreateTextField(form1, "replyToAddrText", av, ac);
#endif // MOZ_MAIL_NEWS

	kids[i++] = fep->org_text =
		fe_CreateTextField(form1, "orgText", av, ac);

#ifdef MOZ_MAIL_NEWS
	kids[i++] = fep->sig_file_text =
		fe_CreateTextField(form1, "sigFileText", av, ac);

	kids[i++] = fep->browse_button = browse_button = 
		XmCreatePushButtonGadget(form1, "browse", av, ac);

	kids[i++] = fep->attach_card_toggle =
		XmCreateToggleButtonGadget(form1, "attachCard", av, ac);
#endif // MOZ_MAIL_NEWS


	XtVaSetValues(id_label,
				  XmNalignment, XmALIGNMENT_BEGINNING,
				  XmNtopAttachment, XmATTACH_FORM,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtVaSetValues(name_label,
				  XmNalignment, XmALIGNMENT_BEGINNING,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, id_label,
				  XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNleftWidget, id_label,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtVaSetValues(fep->name_text,
				  XmNcolumns, 35,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, name_label,
				  XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNleftWidget, id_label,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtVaSetValues(email_addr_label,
				  XmNalignment, XmALIGNMENT_BEGINNING,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, fep->name_text,
				  XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNleftWidget, id_label,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtVaSetValues(fep->email_addr_text,
				  XmNcolumns, 35,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, email_addr_label,
				  XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNleftWidget, id_label,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

#ifdef MOZ_MAIL_NEWS
	XtVaSetValues(reply_to_addr_label,
				  XmNalignment, XmALIGNMENT_BEGINNING,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, fep->email_addr_text,
				  XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNleftWidget, id_label,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtVaSetValues(fep->reply_to_addr_text,
				  XmNcolumns, 35,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, reply_to_addr_label,
				  XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNleftWidget, id_label,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);
#endif // MOZ_MAIL_NEWS

	XtVaSetValues(org_label,
				  XmNalignment, XmALIGNMENT_BEGINNING,
				  XmNtopAttachment, XmATTACH_WIDGET,
#ifdef MOZ_MAIL_NEWS
				  XmNtopWidget, fep->reply_to_addr_text,
#else
				  XmNtopWidget, fep->email_addr_text,
#endif // MOZ_MAIL_NEWS
				  XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNleftWidget, id_label,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtVaSetValues(fep->org_text,
				  XmNcolumns, 35,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, org_label,
				  XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNleftWidget, id_label,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

#ifdef MOZ_MAIL_NEWS
	XtVaSetValues(sig_file_label,
				  XmNcolumns, 35,
				  XmNalignment, XmALIGNMENT_BEGINNING,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, fep->org_text,
				  XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNleftWidget, id_label,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtVaSetValues(fep->sig_file_text,
				  XmNcolumns, 35,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, sig_file_label,
				  XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNleftWidget, id_label,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtVaSetValues(browse_button,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, fep->sig_file_text,
				  XmNleftAttachment, XmATTACH_WIDGET,
				  XmNleftWidget, fep->sig_file_text,
				  XmNleftOffset, 16,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNbottomWidget, fep->sig_file_text,
				  NULL);

	int labels_height;

	labels_height = XfeVaGetTallestWidget(fep->attach_card_toggle,
										  browse_button,
										  fep->sig_file_text,
										  NULL);

	XtVaSetValues(fep->attach_card_toggle,
				  XmNheight, labels_height,
				  XmNalignment, XmALIGNMENT_BEGINNING,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, fep->sig_file_text,
				  XmNtopOffset, 8,
				  XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNleftWidget, id_label,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);
#endif // MOZ_MAIL_NEWS

#ifdef MOZ_MAIL_NEWS
	XtAddCallback(browse_button, XmNactivateCallback,
				  cb_browse, this);
	XtAddCallback(fep->attach_card_toggle, XmNvalueChangedCallback,
				  cb_toggleAttachCard, this);
#endif // MOZ_MAIL_NEWS


	XtManageChildren(kids, i);
	XtManageChild(form1);
	XtManageChild(frame1);

	setCreated(TRUE);
}

// Member:       init
// Description:  Initializes page for MailNewsIdentity
// Inputs:
// Side effects: 

void XFE_PrefsPageMailNewsIdentity::init()
{
	XP_ASSERT(m_prefsDataMailNewsIdentity);

	PrefsDataMailNewsIdentity  *fep = m_prefsDataMailNewsIdentity;
	XFE_GlobalPrefs            *prefs = &fe_globalPrefs;
	
	fe_SetTextField(fep->name_text, prefs->real_name);
	fe_SetTextField(fep->email_addr_text, prefs->email_address);
	fe_SetTextField(fep->org_text, prefs->organization);
#ifdef MOZ_MAIL_NEWS
	fe_SetTextField(fep->reply_to_addr_text, prefs->reply_to_address);
	fe_SetTextField(fep->sig_file_text, prefs->signature_file);
	XtVaSetValues (fep->attach_card_toggle, XmNset, prefs->attach_address_card, 0);
#endif // MOZ_MAIL_NEWS

    // If the preference is locked, grey it out.
    XtSetSensitive(fep->name_text, 
                   !PREF_PrefIsLocked("mail.identity.username"));
    XtSetSensitive(fep->email_addr_text, 
                   !PREF_PrefIsLocked("mail.identity.useremail"));
    XtSetSensitive(fep->org_text, 
                   !PREF_PrefIsLocked("mail.identity.organization"));

#ifdef MOZ_MAIL_NEWS
    XtSetSensitive(fep->reply_to_addr_text, 
                   !PREF_PrefIsLocked("mail.identity.reply_to"));
    XtSetSensitive(fep->sig_file_text, 
                   !PREF_PrefIsLocked("mail.signature_file"));
    XtSetSensitive(fep->browse_button, 
                   !PREF_PrefIsLocked("mail.signature_file"));
    XtSetSensitive(fep->attach_card_toggle, 
                   !PREF_PrefIsLocked("mail.attach_vcard"));
#endif // MOZ_MAIL_NEWS

	setInitialized(TRUE);
}

// Member:       install
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageMailNewsIdentity::install()
{
#ifdef MOZ_MAIL_NEWS
	fe_installMailNewsIdentity();
#endif // MOZ_MAIL_NEWS
}

// Member:       verify
// Description:  
// Inputs:
// Side effects: 

Boolean XFE_PrefsPageMailNewsIdentity::verify()
{
	XP_ASSERT(m_prefsDataMailNewsIdentity);
	PrefsDataMailNewsIdentity  *fep = m_prefsDataMailNewsIdentity;

	// Make sure the user has specified a valid email address

	char *orig_email;
	char *email;
	orig_email = fe_GetTextField(fep->email_addr_text);
	email = fe_StringTrim(orig_email);

	if ((! email) ||
		(XP_STRLEN(email) == 0)) {
		FE_Alert(getContext(), XP_GetString(XFE_EMPTY_EMAIL_ADDR));
		return FALSE;
	}
	XtFree(orig_email);

#ifdef MOZ_MAIL_NEWS
	// Need to check if the user has created a card 

	PersonEntry      person;
	ABook           *addr_book = fe_GetABook(0);
	DIR_Server      *dir;
	ABID             entry_id;
	XFE_GlobalPrefs *prefs = &fe_globalPrefs;

	person.Initialize();
	person.pGivenName = ((prefs->real_name) && (XP_STRLEN(prefs->real_name) > 0)) ?
		XP_STRDUP(prefs->real_name) : 0;
	person.pEmailAddress = ((prefs->email_address) && (XP_STRLEN(prefs->email_address) > 0)) ?
		XP_STRDUP(prefs->email_address) : 0;
  
	DIR_GetPersonalAddressBook(FE_GetDirServers(), &dir);

	if (dir) {
		AB_GetEntryIDForPerson(dir, addr_book, &entry_id, &person);

		if (MSG_MESSAGEIDNONE == entry_id) {
			XtVaSetValues(fep->attach_card_toggle, XmNset, FALSE, 0);
		}
	}

	if (person.pGivenName) XP_FREE(person.pGivenName);
	if (person.pEmailAddress) XP_FREE(person.pEmailAddress);
#endif // MOZ_MAIL_NEWS

	return TRUE;
}

// Member:       save
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageMailNewsIdentity::save()
{
	PrefsDataMailNewsIdentity *fep = m_prefsDataMailNewsIdentity;

	XP_ASSERT(fep);

	if (fe_globalPrefs.real_name) {
		free(fe_globalPrefs.real_name);
	}
	fe_globalPrefs.real_name = fe_GetTextField(fep->name_text);

	PREFS_SET_GLOBALPREF_TEXT(email_address, email_addr_text);
#ifdef MOZ_MAIL_NEWS
	PREFS_SET_GLOBALPREF_TEXT(reply_to_address, reply_to_addr_text);
	PREFS_SET_GLOBALPREF_TEXT(signature_file, sig_file_text);
#endif // MOZ_MAIL_NEWS

	if (fe_globalPrefs.organization) {
		free (fe_globalPrefs.organization);
	}
	fe_globalPrefs.organization = fe_GetTextField (fep->org_text);

#ifdef MOZ_MAIL_NEWS
	Boolean b;
	XtVaGetValues(fep->attach_card_toggle, XmNset, &b, 0);
	fe_globalPrefs.attach_address_card = b;
#endif // MOZ_MAIL_NEWS

	// Install preferences

	install();
}

// Member:       getData
// Description:  
// Inputs:
// Side effects: 

PrefsDataMailNewsIdentity *XFE_PrefsPageMailNewsIdentity::getData()
{
	return m_prefsDataMailNewsIdentity;
}

#ifdef MOZ_MAIL_NEWS
// Member:       cb_browse
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageMailNewsIdentity::cb_browse(Widget    /* w */,
											  XtPointer closure,
											  XtPointer /* callData */)
{
	XFE_PrefsPageMailNewsIdentity *thePage = (XFE_PrefsPageMailNewsIdentity *)closure;
	XFE_PrefsDialog               *theDialog = thePage->getPrefsDialog();
	PrefsDataMailNewsIdentity     *fep = thePage->getData();

	fe_browse_file_of_text(theDialog->getContext(), fep->sig_file_text, False);
}
#endif // MOZ_MAIL_NEWS

#if 0
// Member:       cb_editCard
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageMailNewsIdentity::cb_editCard(Widget    /* w */,
												XtPointer closure,
												XtPointer /* callData */)
{
	XFE_PrefsPageMailNewsIdentity *thePage = (XFE_PrefsPageMailNewsIdentity *)closure;
	XFE_PrefsDialog               *theDialog = thePage->getPrefsDialog();
	Widget                         mainw = theDialog->getBaseWidget();
	XFE_GlobalPrefs               *prefs = &fe_globalPrefs;
	
	// Need to check if the user has created a card 

	PersonEntry  person;
	ABook       *addr_book = fe_GetABook(0);
	DIR_Server  *dir;
	ABID         entry_id;

	person.Initialize();
	person.pGivenName = ((prefs->real_name) && (XP_STRLEN(prefs->real_name) > 0)) ?
		XP_STRDUP(prefs->real_name) : 0;
	person.pEmailAddress = ((prefs->email_address) && (XP_STRLEN(prefs->email_address) > 0)) ?
		XP_STRDUP(prefs->email_address) : 0;
  
	DIR_GetPersonalAddressBook(FE_GetDirServers(), &dir);

	if (dir) {
		AB_GetEntryIDForPerson(dir, addr_book, &entry_id, &person);
		if (MSG_MESSAGEIDNONE != entry_id) {
			fe_showABCardPropertyDlg(mainw, 
									 thePage->getContext(),
									 entry_id,
									 FALSE);
		}
	}

	if (person.pGivenName) XP_FREE(person.pGivenName);
	if (person.pEmailAddress) XP_FREE(person.pEmailAddress);
}
#endif

// Member:       cb_toggleAttachCard
// Description:  
// Inputs:
// Side effects: 

#ifdef MOZ_MAIL_NEWS
void XFE_PrefsPageMailNewsIdentity::cb_toggleAttachCard(Widget    /* w */,
														XtPointer closure,
														XtPointer callData)
{
	XmToggleButtonCallbackStruct  *cb = (XmToggleButtonCallbackStruct *)callData;
	XFE_PrefsPageMailNewsIdentity *thePage = (XFE_PrefsPageMailNewsIdentity *)closure;
	PrefsDataMailNewsIdentity     *fep = thePage->getData();
	XFE_PrefsDialog               *theDialog = thePage->getPrefsDialog();
	MWContext                     *context = theDialog->getContext();
	Widget                         mainw = theDialog->getBaseWidget();
	XFE_GlobalPrefs               *prefs = &fe_globalPrefs;
	
	if (! cb->set) return;

	// Need to check if the user has created a card 

	PersonEntry  person;
	ABook       *addr_book = fe_GetABook(0);
	DIR_Server  *dir;
	ABID         entry_id;

	person.Initialize();
	person.pGivenName = ((prefs->real_name) && (XP_STRLEN(prefs->real_name) > 0)) ?
		XP_STRDUP(prefs->real_name) : 0;
	person.pEmailAddress = ((prefs->email_address) && (XP_STRLEN(prefs->email_address) > 0)) ?
		XP_STRDUP(prefs->email_address) : 0;
  
	DIR_GetPersonalAddressBook(FE_GetDirServers(), &dir);

	if (dir) {
		AB_GetEntryIDForPerson(dir, addr_book, &entry_id, &person);

		if (MSG_MESSAGEIDNONE == entry_id) {
			if (XFE_Confirm(context, XP_GetString(XFE_PREFS_CR_CARD))){
				fe_showABCardPropertyDlg(mainw, 
										 thePage->getContext(),
										 MSG_VIEWINDEXNONE, 
										 TRUE);
			}
			else {
				XtVaSetValues(fep->attach_card_toggle, XmNset, FALSE, 0);
			}
		}
	}

	if (person.pGivenName) XP_FREE(person.pGivenName);
	if (person.pEmailAddress) XP_FREE(person.pEmailAddress);
}
#endif // MOZ_MAIL_NEWS

#ifdef MOZ_MAIL_NEWS

// ************************************************************************
// ************************ Mail News/Mail Server *************************
// ************************************************************************

void
XFE_PrefsIncomingMServer::select_incoming()
{

    // Turn on neccessary buttons if something is selected
    if (m_edit_button) {
        int count = 0;
        XtVaGetValues(m_incoming, XmNselectedItemCount, &count, 0);

        if ( count > 0 ) {
            XtSetSensitive(m_edit_button, True);
            XtSetSensitive(m_delete_button, True);
        } else {
            XtSetSensitive(m_edit_button, False);
            XtSetSensitive(m_delete_button, False);
        }
    }
}

// When something is selected on the server list
void
XFE_PrefsIncomingMServer::cb_select_incoming(Widget /*w*/,
                                             XtPointer clientData,
                                             XtPointer)
{
    XFE_PrefsIncomingMServer *theClass = (XFE_PrefsIncomingMServer *) 
        clientData;

    // Refresh the list
    theClass->select_incoming();
}

// Add a new incoming mail server
void 
XFE_PrefsIncomingMServer::cb_new_incoming(Widget    /*w*/,
                                          XtPointer clientData,
                                          XtPointer)
{
    // Get the main window
    XFE_PrefsIncomingMServer *theClass = 
        (XFE_PrefsIncomingMServer *) clientData;
    theClass->newIncoming();
}

void XFE_PrefsIncomingMServer::newIncoming() {
    XFE_PrefsPage *thePage = getPage();
    XFE_PrefsDialog *theDialog = thePage->getPrefsDialog();
    Widget mainw = theDialog->getBaseWidget();

    int total_servers;

    // Check if a POP server has already been configured. If so, this is an
    // error. 
    if (has_pop()) {
        FE_Message(thePage->getContext(), 
                   XP_GetString(MK_POP3_ONLY_ONE));
        return;
    }

    XtVaGetValues(m_incoming,
                  XmNitemCount, &total_servers,
                  NULL);

    // Create the popup
    // why does this line keep crashing?!
    //    if (m_mserver_dialog) delete m_mserver_dialog;
    m_mserver_dialog =
        new XFE_PrefsMServerDialog(mainw, NULL,
                                   (total_servers==0),
                                   thePage->getContext());

    XtAddCallback(m_mserver_dialog->getChrome(), XmNokCallback,
                  cb_refresh, (XtPointer)this);
    m_mserver_dialog->show();
}

// Edit an existing incoming mail server
void
XFE_PrefsIncomingMServer::cb_edit_incoming(Widget    /*w*/,
                                           XtPointer clientData,
                                           XtPointer)
{

    // Get the main window
    XFE_PrefsIncomingMServer *theClass = 
        (XFE_PrefsIncomingMServer *) clientData;
    theClass->editIncoming();
}

void XFE_PrefsIncomingMServer::editIncoming() {
    XmString *selected_strings;
    char *name;
    int total_servers;

    XFE_PrefsPage *thePage = getPage();
    XFE_PrefsDialog *theDialog = thePage->getPrefsDialog();
    Widget mainw = theDialog->getBaseWidget();

    // Get the selected count
    int selected_count = 0;
    XtVaGetValues(m_incoming,
                  XmNitemCount, &total_servers,
                  XmNselectedItems, &selected_strings, 
                  XmNselectedItemCount, &selected_count,
                  NULL);

    // Only when there is a current hostname selected, we will allow EDITing
    if (selected_count <= 0) return;

    // Get the current hostname
    XmStringGetLtoR(selected_strings[0], 
                    XmSTRING_DEFAULT_CHARSET, 
                    &name);

    // Create the popup
    // Why does this line keep crashing?!
    // if (m_mserver_dialog) delete m_mserver_dialog;
    m_mserver_dialog =
        new XFE_PrefsMServerDialog(mainw, name,
                                   (total_servers==1),
                                   getPage()->getContext());

    XtAddCallback(m_mserver_dialog->getChrome(), XmNokCallback,
                  cb_refresh, (XtPointer)this);
    m_mserver_dialog->show();
}

// Delete an incoming mail server
void
XFE_PrefsIncomingMServer::cb_delete_incoming(Widget /*w*/,
                                             XtPointer clientData,
                                             XtPointer )
{
    // Get the main window
    XFE_PrefsIncomingMServer *theClass = (XFE_PrefsIncomingMServer *) 
        clientData;
    theClass->deleteIncoming();
}
 
void
XFE_PrefsIncomingMServer::deleteIncoming() {
     
    XmString 		*selected_strings;
    char 			*name;
    char			*pop_name;
    XFE_PrefsPage *thePage = getPage();
    XFE_PrefsDialog *theDialog = thePage->getPrefsDialog();
    Widget mainw = theDialog->getBaseWidget();
 
    // Get the selected count
    int selected_count = 0;
    XtVaGetValues(m_incoming, XmNselectedItems, &selected_strings, 
                  XmNselectedItemCount, &selected_count, 0);
 
    // Do nothing if nothing is selected
    if (!selected_count)
        return;
 
    int32 server_type;
    PREF_GetIntPref("mail.server_type", &server_type);
    if ((server_type==TYPE_IMAP) &&
        !fe_Confirm_2(mainw, XP_GetString(MK_MSG_REMOVE_MAILHOST_CONFIRM)))
        return;

    // Get the current hostname
    XmStringGetLtoR(selected_strings[0], 
                    XmSTRING_DEFAULT_CHARSET, 
                    &name);
    PREF_CopyCharPref("network.hosts.pop_server", &pop_name);
     
    if (strcmp(name, pop_name)) {
        MSG_Master* master = fe_getMNMaster();
         
        int total =  MSG_GetIMAPHosts(master, NULL, 0);
         
        if (total) {
            MSG_IMAPHost** hosts = NULL;
             
            hosts = new MSG_IMAPHost* [total];
             
            XP_ASSERT(hosts != NULL);
             
            // Get the list of IMAP hosts
            total =  MSG_GetIMAPHosts(master, hosts, total);
             
            // Loop over all the IMAP hosts
            for (int i = 0; i < total; i++) {
                MSG_Host *p = MSG_GetMSGHostFromIMAPHost(hosts[i]);
                if (strcmp(name, MSG_GetHostName(p)) == 0) {
                    MSG_DeleteIMAPHost(master, hosts[i]);
                }
            }	
             
            if (hosts)
                delete [] hosts;
        }             
    } else  {
        // What do I do here ?
        PREF_SetCharPref("network.hosts.pop_server",
                         "");
    }
     
    XP_FREE(pop_name);
     
    // Refresh the list
    refresh_incoming();
}
 
void
XFE_PrefsIncomingMServer::cb_refresh(Widget /*w*/,
                                     XtPointer clientData,
                                     XtPointer /* callData */)
{
    XFE_PrefsIncomingMServer *theClass = (XFE_PrefsIncomingMServer *)
        clientData;
    if (theClass->m_mserver_dialog)
        XtRemoveCallback(theClass->m_mserver_dialog->getChrome(),
                         XmNokCallback, cb_refresh, clientData);
    theClass->refresh_incoming();
}
 
// Refreshes the incoming server list
void
XFE_PrefsIncomingMServer::refresh_incoming()
{
    XmString str;
    char *server_ptr=NULL;
    // Clear the list
    XmListDeleteAllItems(m_incoming);
    if (has_pop()) {
 
        //	Get the POP server
        PREF_CopyCharPref("network.hosts.pop_server", &server_ptr);
 	
        // Create a string
        str = XmStringCreateLocalized(server_ptr);
 
        // Add it to the end of the list
        XmListAddItem(m_incoming, str, 0);
 		
        // Free the string
        XmStringFree(str);
 
        XP_FREEIF(server_ptr);      
 
        return;
    }
     
    // IMAP
    MSG_Master *master=fe_getMNMaster();
     
    int total = MSG_GetIMAPHosts(master, NULL, 0);
     
    if (!total) return;
    MSG_IMAPHost** hosts=NULL;
     
    hosts = new MSG_IMAPHost* [total];
     
    XP_ASSERT(hosts != NULL);
     
    // Get the list of IMAP hosts
    total =  MSG_GetIMAPHosts(master, hosts, total);
     
    for (int i=0;i<total; i++) {
        MSG_Host *p = MSG_GetMSGHostFromIMAPHost(hosts[i]);
         
        str = XmStringCreateLocalized((char *)MSG_GetHostName(p));
         
        // Add it to the end of the list
        XmListAddItem(m_incoming, str, 0);
         
        // Free the string
        XmStringFree(str);
    }
 
}
 
XP_Bool
XFE_PrefsIncomingMServer::has_pop()
{
    int32 server_type;
    char *server;
    XP_Bool pop;
 
    PREF_CopyCharPref("network.hosts.pop_server", &server);
    PREF_GetIntPref("mail.server_type",&server_type);
    if ((server_type==TYPE_POP) && server[0])
        pop=TRUE;
    else
        pop=FALSE;
 
    XP_FREEIF(server);
 	
    return pop;
 
}
 
 
XP_Bool
XFE_PrefsIncomingMServer::has_imap()
{
    int32 server_type;
    char *server;
    XP_Bool	imap;
 
    PREF_CopyCharPref("network.hosts.imap_servers", &server);
    PREF_GetIntPref("mail.server_type", &server_type);
     
    if ((server_type==TYPE_IMAP) && server[0])
        imap=TRUE;
    else
        imap=FALSE;
 
    XP_FREEIF(server);
 
    return imap;
}
 
 
XP_Bool
XFE_PrefsIncomingMServer::has_many_imap()
{
    int32 server_type;
    char *server;
    XP_Bool	many_imap;
 
    PREF_CopyCharPref("network.hosts.imap_servers", &server);
    PREF_GetIntPref("mail.server_type", &server_type);
     
    if ((server_type==TYPE_IMAP) && server[0] && XP_STRCHR(server,','))
        many_imap=TRUE;
    else
        many_imap=FALSE;
 
    XP_FREEIF(server);
 
    return many_imap;
}
 
 
// Incoming Servers Data
XFE_PrefsIncomingMServer::XFE_PrefsIncomingMServer(Widget incomingServerBox,
 												   XFE_PrefsPage *page) :
    // Store the page. This will be needed to get to the main window for
    // further pop ups.
    m_mserver_dialog(0),
    prefsPage(page)
{
    // Use position attachments
    XtVaSetValues(incomingServerBox, XmNfractionBase, 3, NULL);
 
 
    m_incoming = XmCreateScrolledList(incomingServerBox, "incomingServerList",
                                      NULL, 0);
 
 
    // Add callback for browse selection - when something is selected in the list
    // we should turn on action button accordingly
    XtAddCallback(m_incoming, XmNbrowseSelectionCallback, cb_select_incoming, this);
 
    refresh_incoming();
 
    XtVaSetValues(XtParent(m_incoming),
                  XmNtopAttachment, XmATTACH_FORM,
                  XmNleftAttachment, XmATTACH_FORM,
                  XmNrightAttachment, XmATTACH_POSITION,
                  XmNrightPosition, 2,
                  XmNbottomAttachment, XmATTACH_FORM,
                  XmNselectionPolicy, XmSINGLE_SELECT,
                  NULL);
 
    m_button_form = XtVaCreateWidget ("ButtonForm", xmFormWidgetClass,
                                      incomingServerBox,
                                      XmNtopAttachment, XmATTACH_FORM,
                                      XmNleftAttachment, XmATTACH_POSITION,
                                      XmNleftPosition, 2,
                                      XmNrightAttachment, XmATTACH_POSITION,
                                      XmNrightPosition, 3,
                                      XmNbottomAttachment, XmATTACH_FORM,
                                      NULL);
 									
 	
    m_new_button = XmCreatePushButtonGadget(m_button_form, "New", 
                                            NULL, 0);
 
    XtVaSetValues(m_new_button,
                  XmNtopAttachment, XmATTACH_FORM,
                  XmNrightAttachment, XmATTACH_FORM,
                  NULL);
    XtAddCallback(m_new_button, XmNactivateCallback, cb_new_incoming, this);
 
    m_edit_button = XmCreatePushButtonGadget(m_button_form, "Edit", 
                                             NULL, 0);
 
    XtVaSetValues(m_edit_button,
                  XmNtopAttachment, XmATTACH_WIDGET, 
                  XmNtopWidget, m_new_button,
                  XmNrightAttachment, XmATTACH_OPPOSITE_WIDGET,
                  XmNrightWidget, m_new_button,
                  XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
                  XmNleftWidget, m_new_button,
                  XmNsensitive, False, // Set to false to begin with
                  NULL);
    XtAddCallback(m_edit_button, XmNactivateCallback, cb_edit_incoming, this);
 
    m_delete_button = XmCreatePushButtonGadget(m_button_form, "Delete", 
                                               NULL, 0);
 
    XtVaSetValues(m_delete_button,
                  XmNtopAttachment, XmATTACH_WIDGET, 
                  XmNtopWidget, m_edit_button,
                  XmNrightAttachment, XmATTACH_OPPOSITE_WIDGET,
                  XmNrightWidget, m_new_button,
                  XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
                  XmNleftWidget, m_new_button,
                  XmNsensitive, False, // Set to FALSE until it's implemented
                  NULL);
    XtAddCallback(m_delete_button, XmNactivateCallback, cb_delete_incoming, this);
 
    XtManageChild(m_new_button);
    XtManageChild(m_edit_button);
    XtManageChild(m_delete_button);
    XtManageChild(m_button_form);
    XtManageChild(m_incoming);
 
 
}
 
XFE_PrefsIncomingMServer::~XFE_PrefsIncomingMServer()
{
    if (m_mserver_dialog) delete m_mserver_dialog;
}
 
// Outgoing Server Data
XFE_PrefsOutgoingServer::XFE_PrefsOutgoingServer(Widget outgoingServerBox)
{
    Widget          kids[6];
    int             i  = 0;
 
    Widget server_label;
    Widget server_user_name_label;
    HG11326
     
    kids[i++] = server_label = 
        XmCreateLabelGadget(outgoingServerBox, "outgoingServerLabel", NULL, 0);
 
    kids[i++] = m_servername_text =
        fe_CreateTextField(outgoingServerBox, "serverNameText", NULL, 0);
 
    kids[i++] = server_user_name_label = 
        XmCreateLabelGadget(outgoingServerBox, "serverUsernameLabel", NULL, 0);
 
    kids[i++] = m_username_text = 
        fe_CreateTextField(outgoingServerBox, "serverUsernameText", NULL, 0);
 
    HG72819
 
         
    XtVaSetValues(server_label,
                  XmNalignment, XmALIGNMENT_END,
                  XmNleftAttachment, XmATTACH_FORM,
                  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
                  XmNtopWidget, m_servername_text,
                  XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
                  XmNbottomWidget, m_servername_text,
                  NULL);
 
    XtVaSetValues(m_servername_text,
                  //XmNleftAttachment, XmATTACH_WIDGET,
                  // XmNleftWidget, server_label,
                  XmNrightAttachment, XmATTACH_FORM,
                  XmNtopAttachment, XmATTACH_FORM,
                  NULL);
 
    XtVaSetValues(server_user_name_label,
                  XmNalignment, XmALIGNMENT_END,
                  XmNleftAttachment, XmATTACH_FORM,
                  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
                  XmNtopWidget, m_username_text,
                  XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
                  XmNbottomWidget, m_username_text,
                  NULL);
 
    XtVaSetValues(m_username_text,
                  //XmNleftAttachment, XmATTACH_WIDGET,
                  //XmNleftWidget, m_username_label,
                  XmNrightAttachment, XmATTACH_FORM,
                  XmNtopAttachment, XmATTACH_WIDGET,
                  XmNtopWidget, m_servername_text,
                  NULL);
 
    HG71199
    XtManageChildren(kids, i);
}
 
int32
XFE_PrefsOutgoingServer::get_ssl() {
    HG18159
    return -1;
}
 
void
XFE_PrefsOutgoingServer::set_ssl(int32 val) {
 
    HG28688
}
 
void XFE_PrefsLocalMailDir::cb_choose(Widget    /*w*/,
 									  XtPointer clientData,
 									  XtPointer /* call data */)
{
    XFE_PrefsLocalMailDir *local= (XFE_PrefsLocalMailDir *) clientData;
 
    XFE_PrefsPage *thePage = (XFE_PrefsPageGeneralCache *) local->getPage();
    XFE_PrefsDialog              *theDialog = thePage->getPrefsDialog();
 
    fe_browse_file_of_text(theDialog->getContext(), local->m_local_dir_text, 
                           True);
}
 
// Local mail directory
XFE_PrefsLocalMailDir::XFE_PrefsLocalMailDir(Widget localMailDirBox, 
 											 XFE_PrefsPage *page)
    : prefsPage(page)
{
 
    Widget            kids[6];
    int               i  = 0;
 
    kids[i++] = XmCreateLabelGadget(localMailDirBox,
                                    "localMailDir", 
                                    NULL, 0);
 
    // Get the local_dir from the prefs
    kids[i++] = m_local_dir_text = fe_CreateTextField(localMailDirBox,
                                                      "localMailText",
                                                      NULL, 0);
 
    kids[i++] = XmCreatePushButtonGadget(localMailDirBox, "chooseButton", 
                                         NULL, 0);
 
 
 
    XtVaSetValues(kids[0],
                  XmNleftAttachment, XmATTACH_FORM,
                  NULL);
 
    XtVaSetValues(kids[1],
                  XmNleftAttachment, XmATTACH_WIDGET,
                  XmNleftWidget, kids[0],
                  XmNrightAttachment, XmATTACH_WIDGET,
                  XmNrightWidget, kids[2],
                  NULL);
 
    XtVaSetValues(kids[2],
                  XmNrightAttachment, XmATTACH_FORM,
                  XmNtopAttachment, XmATTACH_FORM,
                  XmNbottomAttachment, XmATTACH_FORM,
                  NULL);
 
    XtAddCallback(kids[2], XmNactivateCallback,
                  cb_choose, this);
 
    XtManageChildren(kids, i);
}
 
// Member:       XFE_PrefsPageMailNewsMserver
// Description:  Constructor
// Inputs:
// Side effects: 
 
XFE_PrefsPageMailNewsMserver::XFE_PrefsPageMailNewsMserver(XFE_PrefsDialog *dialog)
    : XFE_PrefsPage(dialog)
{
}
 
// Member:       ~XFE_PrefsPageMailNewsMserver
// Description:  Destructor
// Inputs:
// Side effects: 
 
XFE_PrefsPageMailNewsMserver::~XFE_PrefsPageMailNewsMserver()
{
}
 
// Member:       create
// Description:  Creates page for MailNews/Mail server
// Inputs:
// Side effects: 
 
void XFE_PrefsPageMailNewsMserver::create()
{
    Arg               av[50];
    int               ac;
 
 
    Widget form;
 
    ac = 0;
    XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
    XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
    XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
    XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
    form = XmCreateForm (m_wPageForm, "mailnewsMserver", av, ac);
    m_wPage = form;
 
    // Incoming mail servers
    Widget frame1;
 
    ac = 0;
    XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
    XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
    XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
    frame1 = XmCreateFrame (form, "iServerFrame", av, ac);
 
    Widget form1;
 
    ac = 0;
    XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
    XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
    XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
    XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
    form1 = XmCreateForm (frame1, "iServerBox", av, ac);
 
    Widget label1;
 
    ac = 0;
    XtSetArg (av [ac], XmNchildType, XmFRAME_TITLE_CHILD); ac++;
    label1 = XmCreateLabelGadget (frame1, "incomingServerLabel", av, ac);
 
    // Outgoing mail server
    Widget frame2;
 
    ac = 0;
    XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
    XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
    XtSetArg (av [ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
    XtSetArg (av [ac], XmNtopWidget, frame1); ac++;
 
    frame2 = XmCreateFrame (form, "oServerFrame", av, ac);
 
    Widget form2;
 
    ac = 0;
    XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
    XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
    XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
    XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
    form2 = XmCreateForm (frame2, "oServerBox", av, ac);
 
    Widget label2;
 
    ac = 0;
    XtSetArg (av [ac], XmNchildType, XmFRAME_TITLE_CHILD); ac++;
    label2 = XmCreateLabelGadget (frame2, "outgoingServerLabel", av, ac);
 
    // Local mail directory
    Widget frame3;
 
    ac = 0;
    XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
    XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
    XtSetArg (av [ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
    XtSetArg (av [ac], XmNtopWidget, frame2); ac++;
    frame3 = XmCreateFrame (form, "localFrame", av, ac);
 
    Widget form3;
 
    ac = 0;
    XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
    XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
    XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
    XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
    form3 = XmCreateForm (frame3, "localBox", av, ac);
 
    Widget label3;
 
    ac = 0;
    XtSetArg (av [ac], XmNchildType, XmFRAME_TITLE_CHILD); ac++;
    label3 = XmCreateLabelGadget (frame3, "localMailDirLabel", av, ac);
 
    // Create the objects corresponding to the frames
    xfe_incoming = new XFE_PrefsIncomingMServer(form1, this);
    xfe_outgoing = new XFE_PrefsOutgoingServer(form2);
    xfe_local_mail = new XFE_PrefsLocalMailDir(form3, this);
 
    XtManageChild(label1);
    XtManageChild(form1);
    XtManageChild(frame1);
 
    XtManageChild(label2);
    XtManageChild(form2);
    XtManageChild(frame2);
 
    XtManageChild(label3);
    XtManageChild(form3);
    XtManageChild(frame3);
 
    setCreated(TRUE);
}
 
// Member:       init
// Description:  Initializes page for MailNewsMserver
// Inputs:
// Side effects: 
 
void XFE_PrefsPageMailNewsMserver::init()
{
 
    char *charval;
    int32 intval;
     
    // Fill in the outgoing server and the user name field
    PREF_CopyCharPref("network.hosts.smtp_server", &charval);
    fe_SetTextField(xfe_outgoing->get_server_name(), charval);
    XP_FREE(charval);
 
    PREF_CopyCharPref("mail.smtp_name", &charval);
    fe_SetTextField(xfe_outgoing->get_user_name(), charval);
    XP_FREE(charval);
 
    HG82160
 
    // Fill in the local mail directory field
    PREF_CopyCharPref("mail.directory", &charval);
    fe_SetTextField(xfe_local_mail->get_local_dir_text(), charval);
    XP_FREE(charval);
 
    setInitialized(TRUE);
}
 
// Member:       verify
// Description:  
// Inputs:
// Side effects: 
 
Boolean XFE_PrefsPageMailNewsMserver::verify()
{
 
    // Don't verify for now.
    return TRUE;
}
 
// Member:       install
// Description:  
// Inputs:
// Side effects: 
 
void XFE_PrefsPageMailNewsMserver::install()
{
    fe_installMailNewsMserver();
}
 
// Member:       save
// Description:  
// Inputs:
// Side effects: 
 
void XFE_PrefsPageMailNewsMserver::save()
{
 	
    PREF_SetCharPref("network.hosts.smtp_server",
                     fe_GetTextField(xfe_outgoing->get_server_name()));
 
    PREF_SetCharPref("mail.smtp_name",
                     fe_GetTextField(xfe_outgoing->get_user_name()));
 
    PREF_SetCharPref("mail.directory",
                     fe_GetTextField(xfe_local_mail->get_local_dir_text()));
 
 
    HG92799
 	
    install();
}
 
#endif
 
#ifdef MOZ_MAIL_NEWS
// ************************************************************************
// ************************ Mail Message Copies ***************************
// ************************************************************************
 
 // Member:       XFE_PrefsPageMailNewsCopies
 // Description:  Constructor
 // Inputs:
 // Side effects: 
 
XFE_PrefsPageMailNewsCopies::XFE_PrefsPageMailNewsCopies(XFE_PrefsDialog *dialog)
    : XFE_PrefsPage(dialog)
{
    m_master = fe_getMNMaster();
    XP_ASSERT(m_master);
 
    m_chooseDialog = NULL;
}
 
// Member:       ~XFE_PrefsPageMailNewsCopies
// Description:  destructor
// Inputs:
// Side effects: 
 
XFE_PrefsPageMailNewsCopies::~XFE_PrefsPageMailNewsCopies()
{
    if (m_chooseDialog) delete m_chooseDialog;
}
 
// Member:       create
// Description:  Creates widgets
// Inputs:
// Side effects: 
 
void XFE_PrefsPageMailNewsCopies::create()
{
    Arg    av[50];
    int    ac;
 
    Widget copies_form;
 
 
    ac=0;
    XtSetArg(av[ac], XmNtopAttachment, XmATTACH_FORM); ac++;
    XtSetArg(av[ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
    XtSetArg(av[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
    XtSetArg(av[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
 
    m_wPage = copies_form =
        XmCreateForm(m_wPageForm, "mailnewsCopies", av,ac);
    XtManageChild(copies_form);
 
    Widget mail_frame = createMailFrame(copies_form, NULL);
    Widget news_frame = createNewsFrame(copies_form, mail_frame);
    /* Widget dt_frame   = */ createDTFrame(copies_form,news_frame);
 
    // callbacks
    XtAddCallback(m_mail_choose_button, XmNactivateCallback,
                  cb_chooseMailFcc, this);
    XtAddCallback(m_news_choose_button, XmNactivateCallback,
                  cb_chooseNewsFcc, this);
    XtAddCallback(m_drafts_choose_button, XmNactivateCallback,
                  cb_chooseDraftsFcc, this);
    XtAddCallback(m_templates_choose_button, XmNactivateCallback,
                  cb_chooseTemplatesFcc, this);
     
    setCreated(TRUE);
}  
   
// Member:       create
// Description:  Creates widgets
// Inputs:
// Side effects: 
 
Widget XFE_PrefsPageMailNewsCopies::createMailFrame(Widget parent,
                                                    Widget /* attachTo */)
{
    
    Widget kids[50];
    Arg    av[20];
    int    ac;
    int    i=0;
   
    XP_ASSERT(parent);
   
    // Mail copies frame
    // top frame, attached to form everywhere but bottom
    Widget mail_frame;
    ac=0;
    XtSetArg(av[ac], XmNtopAttachment, XmATTACH_FORM); ac++;
    XtSetArg(av[ac], XmNbottomAttachment, XmATTACH_NONE); ac++;
    XtSetArg(av[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
    XtSetArg(av[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
    mail_frame =
        XmCreateFrame(parent,"mailFrame",av,ac);
   
    Widget mail_form;
    ac=0;
    XtSetArg(av[ac], XmNtopAttachment, XmATTACH_FORM); ac++;
    XtSetArg(av[ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
    XtSetArg(av[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
    XtSetArg(av[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
    mail_form =
        XmCreateForm(mail_frame,"mailForm",av,ac);
 
    Widget mail_label;
    ac=0;
    XtSetArg(av[ac], XmNchildType, XmFRAME_TITLE_CHILD); ac++;
    mail_label =
        XmCreateLabelGadget(mail_frame, "mailCopies",av,ac);
 
    ac=0;
 
    kids[i++] = m_mail_self_toggle =
        XmCreateToggleButtonGadget(mail_form,"mailSelfToggle", av, ac);
 
    kids[i++] = m_mail_other_text =
        fe_CreateTextField(mail_form,"mailOtherText", av, ac);
 
    kids[i++] = m_mail_fcc_toggle =
        XmCreateToggleButtonGadget(mail_form,"mailFccToggle",av,ac);
 
    kids[i++] = m_mail_choose_button =
        XmCreatePushButtonGadget(mail_form,"mailChooseButton",av,ac);
 
    kids[i++] = m_mail_other_toggle =
        XmCreateToggleButtonGadget(mail_form,"mailOtherToggle",av,ac);
 
    // place the widgets
    int max_height1 = XfeVaGetTallestWidget(m_mail_fcc_toggle,
                                            m_mail_choose_button,
                                            NULL);
    int max_height2 = XfeVaGetTallestWidget(m_mail_other_text,
                                            m_mail_other_toggle,
                                            NULL);
 
 
 
    XtVaSetValues(m_mail_fcc_toggle,
                  XmNheight, max_height1,
                  XmNtopAttachment, XmATTACH_FORM,
                  XmNrightAttachment, XmATTACH_NONE,
                  XmNleftAttachment, XmATTACH_FORM,
                  XmNbottomAttachment, XmATTACH_NONE,
                  NULL);
 
    XtVaSetValues(m_mail_choose_button,
                  XmNheight, max_height1,
                  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
                  XmNtopWidget, m_mail_fcc_toggle,
                  XmNleftAttachment, XmATTACH_NONE,
                  XmNrightAttachment, XmATTACH_FORM,
                  XmNleftAttachment, XmATTACH_NONE,
                  NULL);
 
    XtVaSetValues(m_mail_self_toggle,
                  XmNheight, max_height1,
                  XmNtopAttachment, XmATTACH_WIDGET,
                  XmNtopWidget, m_mail_fcc_toggle,
                  XmNleftAttachment, XmATTACH_FORM,
                  XmNrightAttachment, XmATTACH_NONE,
                  XmNbottomAttachment, XmATTACH_NONE,
                  NULL);
 
    XtVaSetValues(m_mail_other_toggle,
                  XmNheight, max_height2,
                  XmNtopAttachment, XmATTACH_WIDGET,
                  XmNtopWidget, m_mail_self_toggle,
                  XmNrightAttachment, XmATTACH_NONE,
                  XmNleftAttachment, XmATTACH_FORM,
                  XmNbottomAttachment, XmATTACH_NONE,
                  NULL);
   
    XtVaSetValues(m_mail_other_text,
                  XmNheight, max_height2,
                  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
                  XmNtopWidget, m_mail_other_toggle,
                  XmNrightAttachment, XmATTACH_FORM,
                  XmNleftAttachment, XmATTACH_WIDGET,
                  XmNleftWidget, m_mail_other_toggle,
                  XmNbottomAttachment, XmATTACH_NONE,
                  NULL);
 
    XtManageChildren(kids,i);
    XtManageChild(mail_frame);
    XtManageChild(mail_form);
    XtManageChild(mail_label);
 
    return mail_frame;
}
 
 
 
// Member:       create
// Description:  Creates widgets
// Inputs:
// Side effects: 
 
Widget XFE_PrefsPageMailNewsCopies::createNewsFrame(Widget parent,
                                                    Widget attachTo)
{
 
    Widget kids[100];
    Arg    av[50];
    int    ac;
    int    i=0;
    XP_ASSERT(parent);
    // News copies frame 
    // middle frame, attached to mail copies frame and drafts frame
    Widget news_frame;
    ac=0;
    XtSetArg(av[ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
    XtSetArg(av[ac], XmNtopWidget, attachTo); ac++;
    XtSetArg(av[ac], XmNbottomAttachment, XmATTACH_NONE); ac++;
    XtSetArg(av[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
    XtSetArg(av[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
    news_frame = XmCreateFrame(parent,"newsFrame",av,ac);
   
    Widget news_form;
    ac=0;
    XtSetArg(av[ac], XmNtopAttachment, XmATTACH_FORM); ac++;
    XtSetArg(av[ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
    XtSetArg(av[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
    XtSetArg(av[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
    news_form = XmCreateForm(news_frame,"newsForm",av,ac);
 
    Widget news_label;
    ac=0;
    XtSetArg(av[ac], XmNchildType, XmFRAME_TITLE_CHILD); ac++;
    news_label = XmCreateLabelGadget(news_frame, "newsCopies",av,ac);
 
    ac=0;
 
    kids[i++] = m_news_self_toggle =
        XmCreateToggleButtonGadget(news_form,"newsSelfToggle", av, ac);
 
    kids[i++] = m_news_other_text =
        fe_CreateTextField(news_form,"newsOtherText", av, ac);
 
    kids[i++] = m_news_fcc_toggle =
        XmCreateToggleButtonGadget(news_form,"newsFccToggle",av,ac);
 
    kids[i++] = m_news_choose_button =
        XmCreatePushButtonGadget(news_form,"newsChooseButton",av,ac);
 
    kids[i++] = m_news_other_toggle =
        XmCreateToggleButtonGadget(news_form,"newsOtherToggle",av,ac);
 
    // place the widgets
    int max_height1 = XfeVaGetTallestWidget(m_news_fcc_toggle,
                                            m_news_choose_button,
                                            NULL);
    int max_height2 = XfeVaGetTallestWidget(m_news_other_text,
                                            m_news_other_toggle,
                                            NULL);
 
 
 
    XtVaSetValues(m_news_fcc_toggle,
                  XmNheight, max_height1,
                  XmNtopAttachment, XmATTACH_FORM,
                  XmNrightAttachment, XmATTACH_NONE,
                  XmNleftAttachment, XmATTACH_FORM,
                  XmNbottomAttachment, XmATTACH_NONE,
                  NULL);
 
    XtVaSetValues(m_news_choose_button,
                  XmNheight, max_height1,
                  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
                  XmNtopWidget, m_news_fcc_toggle,
                  XmNleftAttachment, XmATTACH_NONE,
                  XmNrightAttachment, XmATTACH_FORM,
                  XmNleftAttachment, XmATTACH_NONE,
                  NULL);
 
    XtVaSetValues(m_news_self_toggle,
                  XmNheight, max_height1,
                  XmNtopAttachment, XmATTACH_WIDGET,
                  XmNtopWidget, m_news_fcc_toggle,
                  XmNleftAttachment, XmATTACH_FORM,
                  XmNrightAttachment, XmATTACH_NONE,
                  XmNbottomAttachment, XmATTACH_NONE,
                  NULL);
 
    XtVaSetValues(m_news_other_toggle,
                  XmNheight, max_height2,
                  XmNtopAttachment, XmATTACH_WIDGET,
                  XmNtopWidget, m_news_self_toggle,
                  XmNrightAttachment, XmATTACH_NONE,
                  XmNleftAttachment, XmATTACH_FORM,
                  XmNbottomAttachment, XmATTACH_NONE,
                  NULL);
   
    XtVaSetValues(m_news_other_text,
                  XmNheight, max_height2,
                  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
                  XmNtopWidget, m_news_other_toggle,
                  XmNrightAttachment, XmATTACH_FORM,
                  XmNleftAttachment, XmATTACH_WIDGET,
                  XmNleftWidget, m_news_other_toggle,
                  XmNbottomAttachment, XmATTACH_NONE,
                  NULL);
     
    XtManageChildren(kids,i);
    XtManageChild(news_frame);
    XtManageChild(news_form);
    XtManageChild(news_label);
 
 
    return news_frame;
}
 
 
// Member:       create
// Description:  Creates widgets
// Inputs:
// Side effects: 
 
Widget XFE_PrefsPageMailNewsCopies::createDTFrame(Widget parent,
                                                  Widget attachTo)
{
 
    Widget kids[100];
    Arg    av[50];
    int    ac;
    int    i=0;
   
    XP_ASSERT(parent);
 
    // Drafts/Templates frame 
    // bottom frame, attached to news copies and bottom
    Widget dt_frame;
    ac=0;
    XtSetArg(av[ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
    XtSetArg(av[ac], XmNtopWidget, attachTo); ac++;
    XtSetArg(av[ac], XmNbottomAttachment, XmATTACH_NONE); ac++;
    XtSetArg(av[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
    XtSetArg(av[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
    dt_frame = XmCreateFrame(parent,"dtFrame",av,ac);
   
    Widget dt_form;
    ac=0;
    XtSetArg(av[ac], XmNtopAttachment, XmATTACH_FORM); ac++;
    XtSetArg(av[ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
    XtSetArg(av[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
    XtSetArg(av[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
    dt_form = XmCreateForm(dt_frame,"dtForm",av,ac);
 
    Widget dt_label;
    ac=0;
    XtSetArg(av[ac], XmNchildType, XmFRAME_TITLE_CHILD); ac++;
    dt_label = XmCreateLabelGadget(dt_frame, "dtCopies",av,ac);
 
    ac=0;
    kids[i++] = m_drafts_save_label =
        XmCreateLabelGadget(dt_form,"dSaveLabel",av,ac);
    kids[i++] = m_drafts_choose_button =
        XmCreatePushButtonGadget(dt_form, "dFccButton", av, ac);
 
    kids[i++] = m_templates_save_label =
        XmCreateLabelGadget(dt_form,"tSaveLabel",av,ac);
    kids[i++] = m_templates_choose_button =
        XmCreatePushButtonGadget(dt_form, "tFccButton", av, ac);
 
 
    int max_height1 = XfeVaGetTallestWidget(m_drafts_save_label,
                                            m_drafts_choose_button,
                                            NULL);
    int max_height2 = XfeVaGetTallestWidget(m_templates_save_label,
                                            m_templates_choose_button,
                                            NULL);
 
    XtVaSetValues(m_drafts_save_label,
                  XmNheight, max_height1,
                  XmNtopAttachment, XmATTACH_FORM,
                  XmNleftAttachment, XmATTACH_FORM,
                  XmNrightAttachment, XmATTACH_NONE,
                  XmNbottomAttachment, XmATTACH_NONE,
                  NULL);
 
    XtVaSetValues(m_drafts_choose_button,
                  XmNheight, max_height1,
                  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
                  XmNtopWidget, m_drafts_save_label,
                  XmNrightAttachment, XmATTACH_FORM,
                  XmNleftAttachment, XmATTACH_NONE,
                  XmNbottomAttachment, XmATTACH_NONE,
                  NULL);
 
    XtVaSetValues(m_templates_save_label,
                  XmNheight, max_height2,
                  XmNtopAttachment, XmATTACH_WIDGET,
                  XmNtopWidget, m_drafts_save_label,
                  XmNleftAttachment, XmATTACH_FORM,
                  XmNrightAttachment, XmATTACH_NONE,
                  XmNbottomAttachment, XmATTACH_NONE,
                  NULL);
 
    XtVaSetValues(m_templates_choose_button,
                  XmNheight, max_height2,
                  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
                  XmNtopWidget, m_templates_save_label,
                  XmNrightAttachment, XmATTACH_FORM,
                  XmNleftAttachment, XmATTACH_NONE,
                  XmNbottomAttachment, XmATTACH_NONE,
                  NULL);
   
    XtManageChildren(kids,i);
    XtManageChild(dt_frame);
    XtManageChild(dt_form);
    XtManageChild(dt_label);
 
    return dt_frame;
}
 
// Member:       init
// Description:  
// Inputs:
// Side effects: 
 
void XFE_PrefsPageMailNewsCopies::init()
{
 
    // values to grab from preference
    Bool boolval;
    Bool locked;
    char *charval;
    char *bcc, *email, *bcc_format;
    XmString xmstr;             // temp compound string for labels
 
    // get e-mail address for "BCC: userid@company.com" fields
    PREF_CopyCharPref("mail.identity.useremail", &email);
    bcc_format=XP_GetString(XFE_MAIL_SELF_FORMAT);
    bcc = PR_smprintf(bcc_format, email);
 
    // outgoing mail messages
    // 1. Toggle self
    xmstr = XmStringCreateLocalized(bcc);
    PREF_GetBoolPref("mail.cc_self",&boolval);
    locked = PREF_PrefIsLocked("mail.cc_self");
    XtVaSetValues(m_mail_self_toggle,
                  XmNset, boolval,
                  XmNlabelString, xmstr,
                  XmNsensitive,!locked,
                  NULL);
    XmStringFree(xmstr);
     
    // 2. toggle and value of fcc
    PREF_GetBoolPref("mail.use_imap_sentmail", &boolval);
    if (boolval)
        PREF_CopyCharPref("mail.imap_sentmail_path", &charval);
    else {
        // prepend "mailbox:" onto path
        PREF_CopyCharPref("mail.default_fcc", &charval);
        char *url=(char *)XP_CALLOC(strlen(charval)+9, sizeof(char));
        XP_STRCPY(url,"mailbox:");
        XP_STRCAT(url,charval);
        XP_FREE(charval);
        charval=url;
    }
 
    m_mailFcc = GetSpecialFolder(charval, MK_MSG_SENT_L10N_NAME);
    xmstr = VerboseFolderName(m_mailFcc);
     
    PREF_GetBoolPref("mail.use_fcc",&boolval);
    locked = PREF_PrefIsLocked("mail.use_fcc");
    XtVaSetValues(m_mail_fcc_toggle,
                  XmNset, boolval,
                  XmNlabelString, xmstr,
                  XmNsensitive, !locked,
                  NULL);
    XmStringFree(xmstr);
    XP_FREE(charval);
 
    // 2.1 fcc button
    locked = PREF_PrefIsLocked("mail.default_fcc");
    XtVaSetValues(m_mail_choose_button,
                  XmNsensitive, !locked,
                  NULL);
 
    // 3. CC
    PREF_CopyCharPref("mail.default_cc",&charval);
    locked = PREF_PrefIsLocked("mail.default_cc");
    XtVaSetValues(m_mail_other_text,
                  XmNvalue, charval,
                  XmNsensitive,!locked,
                  NULL);
    XP_FREE(charval);
 
    // outgoing news messages
    // 1. Toggle self
     
    xmstr = XmStringCreateLocalized(bcc);
    PREF_GetBoolPref("news.cc_self",&boolval);
    locked = PREF_PrefIsLocked("news.cc_self");
    XtVaSetValues(m_news_self_toggle,
                  XmNset, boolval,
                  XmNlabelString, xmstr,
                  XmNsensitive,!locked,
                  NULL);
    XmStringFree(xmstr);
 
    // TODO: format as "Fcc %s on %s"
    // 2. toggle and value of fcc
    PREF_GetBoolPref("news.use_imap_sentmail", &boolval);
    if (boolval)                // new style - use imap URL
        PREF_CopyCharPref("news.imap_sentmail_path", &charval);
    else {                       // old style - use local path
        PREF_CopyCharPref("news.default_fcc", &charval);
        char *url=(char *)XP_CALLOC(strlen(charval)+9, sizeof(char));
        XP_STRCPY(url,"mailbox:");
        XP_STRCAT(url,charval);
        XP_FREE(charval);
        charval=url;
    }
     
    m_newsFcc=GetSpecialFolder(charval, MK_MSG_SENT_L10N_NAME);
    xmstr = VerboseFolderName(m_newsFcc);
     
    PREF_GetBoolPref("news.use_fcc",&boolval);
    locked = PREF_PrefIsLocked("news.use_fcc");
    XtVaSetValues(m_news_fcc_toggle,
                  XmNset, boolval,
                  XmNlabelString, xmstr,
                  XmNsensitive, !locked,
                  NULL);
    XmStringFree(xmstr);
    XP_FREE(charval);
 
    // 2.1 fcc button
    locked = PREF_PrefIsLocked("news.default_fcc");
    XtVaSetValues(m_news_choose_button,
                  XmNsensitive,!locked,
                  NULL);
   
    // 3. CC
    PREF_CopyCharPref("news.default_cc",&charval);
    locked = PREF_PrefIsLocked("news.default_cc");
    fe_SetTextField(m_news_other_text, charval);
    XP_FREE(charval);
 
    // storage for drafts and templates
    PREF_CopyCharPref("mail.default_drafts",&charval);
     
    m_draftsFcc = GetSpecialFolder(charval, MK_MSG_DRAFTS_L10N_NAME);
    xmstr = VerboseFolderName(m_draftsFcc, XFE_DRAFTS_ON_SERVER);
     
    XtVaSetValues(m_drafts_save_label,
                  XmNlabelString, xmstr,
                  NULL);
    XmStringFree(xmstr);
    XP_FREE(charval);
 
    PREF_CopyCharPref("mail.default_templates",&charval);
    m_templatesFcc = GetSpecialFolder(charval, MK_MSG_TEMPLATES_L10N_NAME);
    xmstr = VerboseFolderName(m_templatesFcc, XFE_TEMPLATES_ON_SERVER);    
    XtVaSetValues(m_templates_save_label,
                  XmNlabelString, xmstr,
                  NULL);
    XmStringFree(xmstr);
    XP_FREE(charval);
 
    XP_FREE(email);
    XP_FREE(bcc);
     
    setInitialized(TRUE);  
}
 
 
// Member:       install
// Description:  
// Inputs:
// Side effects: 
 
void XFE_PrefsPageMailNewsCopies::install()
{
}
 
 
// Member:       save
// Description:  
// Inputs:
// Side effects: 
 
void XFE_PrefsPageMailNewsCopies::save()
{
    Bool boolval;
    char *charval;
    URL_Struct *url;
 
    // Mail 
    boolval = XmToggleButtonGadgetGetState(m_mail_self_toggle);
    PREF_SetBoolPref("mail.cc_self",boolval);
 
    boolval = XmToggleButtonGadgetGetState(m_mail_fcc_toggle);
    PREF_SetBoolPref("mail.use_fcc",boolval);
 
    charval = XmTextFieldGetString(m_mail_other_text);
    PREF_SetCharPref("mail.default_cc",charval);
    XtFree(charval);
 
    if (MSG_GetHostForFolder(m_mailFcc)) {
        url=MSG_ConstructUrlForFolder(NULL, m_mailFcc);
        XP_ASSERT(url);
        if (url) {
            PREF_SetBoolPref("mail.use_imap_sentmail",True);
            PREF_SetCharPref("mail.imap_sentmail_path",url->address);
            NET_FreeURLStruct(url);
        }
    } else {
        charval = (char *)MSG_GetFolderNameFromID(m_mailFcc);
        PREF_SetCharPref("mail.default_fcc", charval);
        // don't need to free charval
        PREF_SetBoolPref("mail.use_imap_sentmail",False);
    }
   
 
    // News
    boolval = XmToggleButtonGadgetGetState(m_news_self_toggle);
    PREF_SetBoolPref("news.cc_self",boolval);
 
    boolval = XmToggleButtonGadgetGetState(m_news_fcc_toggle);
    PREF_SetBoolPref("news.use_fcc",boolval);
 
    charval = XmTextFieldGetString(m_news_other_text);
    PREF_SetCharPref("news.default_cc",charval);
    XtFree(charval);
 
    if (MSG_GetHostForFolder(m_newsFcc)) {
        url=MSG_ConstructUrlForFolder(NULL, m_newsFcc);
        XP_ASSERT(url);
        if (url) {
            PREF_SetBoolPref("news.use_imap_sentmail",True);
            PREF_SetCharPref("news.imap_sentmail_path",url->address);
            NET_FreeURLStruct(url);
        }
    } else {
        charval = (char *)MSG_GetFolderNameFromID(m_newsFcc);
        PREF_SetCharPref("news.default_fcc", charval);
        // don't need to free charval
        PREF_SetBoolPref("news.use_imap_sentmail",False);
    }
 
    url=MSG_ConstructUrlForFolder(NULL, m_draftsFcc);
    PREF_SetCharPref("mail.default_drafts",url->address);
    NET_FreeURLStruct(url);
     
    url=MSG_ConstructUrlForFolder(NULL, m_templatesFcc);
    PREF_SetCharPref("mail.default_templates",url->address);
    NET_FreeURLStruct(url);
 
    install();
}
 
// Member:       verify
// Description:  
// Inputs:
// Side effects: 
 
Boolean XFE_PrefsPageMailNewsCopies::verify()
{
    return TRUE;
}
 
 
XmString XFE_PrefsPageMailNewsCopies::VerboseFolderName(MSG_FolderInfo *pFolder, int format_string_id)
{
    MSG_FolderLine folderLine, hostFolderLine;
    MSG_FolderInfo *hostFolder;
    MSG_Host *host;
    XmString xmstr;
    char *verbose_folder;
 
    XP_ASSERT(pFolder);
 
    host=MSG_GetHostForFolder(pFolder);
    if (host)
        hostFolder=MSG_GetFolderInfoForHost(host);
    else
        hostFolder=MSG_GetLocalMailTree(m_master);
 
    XP_ASSERT(hostFolder);
 
    MSG_GetFolderLineById(m_master, pFolder, &folderLine);
    MSG_GetFolderLineById(m_master, hostFolder, &hostFolderLine);
     
    if (format_string_id==0) format_string_id=XFE_FOLDER_ON_SERVER_FORMAT;
     
    char *folder_format=XP_GetString(format_string_id);
    verbose_folder=PR_smprintf(folder_format,
                               folderLine.prettyName,
                               hostFolderLine.name);
    xmstr = XmStringCreateLocalized(verbose_folder);
    XP_FREE(verbose_folder);
 
    return xmstr;
}
 
MSG_FolderInfo *XFE_PrefsPageMailNewsCopies::GetSpecialFolder(char *url, int l10n_name) {
 
    MSG_FolderInfo *folder;
    if (url) {
        folder = MSG_GetFolderInfoFromURL(m_master, url, TRUE);
        if (folder) return folder;
    }
     
    // the messaging library wouldn't give it to us,
    // so we'll build it ourselves.
     
    char *folder_name=XP_GetString(l10n_name);
    XP_ASSERT(folder_name);
 
    char *mail_dir;
    PREF_CopyCharPref("mail.directory",&mail_dir);
 
    // concatinate "mailbox:" + mail_dir + "/" + folder_name
    char *magic_url=(char *)XP_CALLOC(9+strlen(mail_dir)+1+
                                      strlen(folder_name)+1,
                                      sizeof(char));
 
    XP_STRCPY(magic_url,"mailbox:");
    XP_STRCAT(magic_url,mail_dir);
 
    // make sure there is exactly one '/'
    // MSG_GetFolderInfoFromURL doesn't like '//'
    char *slash=XP_STRRCHR(magic_url,'/');
    if (slash) {
        slash++;
        if (*slash!='\0')
            XP_STRCAT(magic_url,"/");
    }
    XP_STRCAT(magic_url,folder_name);
     
    folder = MSG_GetFolderInfoFromURL(m_master,magic_url, TRUE);
    XP_ASSERT(folder);
    XP_FREE(mail_dir);
    XP_FREE(magic_url);
    return folder;
 
}
 
 
// global callback for all of the Choose Folder.. buttons
MSG_FolderInfo *XFE_PrefsPageMailNewsCopies::chooseFcc(int l10n_name, MSG_FolderInfo *selected_folder) {
 
    XFE_PrefsDialog *theDialog = getPrefsDialog();
    Widget         mainw = theDialog->getBaseWidget();
     
    if (m_chooseDialog==NULL) {
        m_chooseDialog = new XFE_PrefsMailFolderDialog(mainw, m_master, getContext());
        m_chooseDialog->initPage();
    }
    m_chooseDialog->setFolder(selected_folder,l10n_name);
    return m_chooseDialog->prompt();
 
}
 
void XFE_PrefsPageMailNewsCopies::cb_chooseMailFcc(Widget, XtPointer closure,XtPointer)
{
    XFE_PrefsPageMailNewsCopies *prefsPage=
        (XFE_PrefsPageMailNewsCopies*)closure;
 
    prefsPage->m_mailFcc =
        prefsPage->chooseFcc(MK_MSG_SENT_L10N_NAME,prefsPage->m_mailFcc);
    XmString xmstr=prefsPage->VerboseFolderName(prefsPage->m_mailFcc);
    XtVaSetValues(prefsPage->m_mail_fcc_toggle,XmNlabelString,xmstr,NULL);
    XmStringFree(xmstr);
}
 
void XFE_PrefsPageMailNewsCopies::cb_chooseNewsFcc(Widget, XtPointer closure, XtPointer)
{
    XFE_PrefsPageMailNewsCopies *prefsPage=
        (XFE_PrefsPageMailNewsCopies*)closure;
     
    prefsPage->m_newsFcc =
        prefsPage->chooseFcc(MK_MSG_SENT_L10N_NAME,prefsPage->m_newsFcc);
    XmString xmstr=prefsPage->VerboseFolderName(prefsPage->m_newsFcc);
    XtVaSetValues(prefsPage->m_news_fcc_toggle,XmNlabelString,xmstr,NULL);
    XmStringFree(xmstr);
}
 
void XFE_PrefsPageMailNewsCopies::cb_chooseDraftsFcc(Widget, XtPointer closure, XtPointer)
{
    XFE_PrefsPageMailNewsCopies *prefsPage=
        (XFE_PrefsPageMailNewsCopies*)closure;
 
    prefsPage->m_draftsFcc =
        prefsPage->chooseFcc(MK_MSG_DRAFTS_L10N_NAME,prefsPage->m_draftsFcc);
    XmString xmstr=prefsPage->VerboseFolderName(prefsPage->m_draftsFcc, XFE_DRAFTS_ON_SERVER);
    XtVaSetValues(prefsPage->m_drafts_save_label,XmNlabelString,xmstr,NULL);
    XmStringFree(xmstr);
}
 
void XFE_PrefsPageMailNewsCopies::cb_chooseTemplatesFcc(Widget, XtPointer closure, XtPointer)
{
    XFE_PrefsPageMailNewsCopies *prefsPage=
        (XFE_PrefsPageMailNewsCopies*)closure;
     
    prefsPage->m_templatesFcc =
        prefsPage->chooseFcc(MK_MSG_TEMPLATES_L10N_NAME,prefsPage->m_templatesFcc);
    XmString xmstr=prefsPage->VerboseFolderName(prefsPage->m_templatesFcc,XFE_DRAFTS_ON_SERVER);
    XtVaSetValues(prefsPage->m_templates_save_label,XmNlabelString,xmstr,NULL);
    XmStringFree(xmstr);    
}
 
#endif  /* MOZ_MAIL_NEWS */
 
 
#ifdef MOZ_MAIL_NEWS
 
// ************************************************************************
// ************************ Mail Message HTML ***************************
// ************************************************************************
 
 // Member:       XFE_PrefsPageMailNewsHTML
 // Description:  Constructor
 // Inputs:
 // Side effects: 
 
XFE_PrefsPageMailNewsHTML::XFE_PrefsPageMailNewsHTML(XFE_PrefsDialog *dialog)
    : XFE_PrefsPage(dialog)
{
}
 
// Member:       ~XFE_PrefsPageMailNewsHTML
// Description:  destructor
// Inputs:
// Side effects: 
 
XFE_PrefsPageMailNewsHTML::~XFE_PrefsPageMailNewsHTML()
{
}
 
// Member:       create
// Description:  Creates widgets
// Inputs:
// Side effects: 
 
void XFE_PrefsPageMailNewsHTML::create()
{
    Arg    av[50];
    int    ac;
 
    Widget html_form;
 
    ac=0;
    XtSetArg(av[ac], XmNtopAttachment, XmATTACH_FORM); ac++;
    XtSetArg(av[ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
    XtSetArg(av[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
    XtSetArg(av[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
 
    m_wPage = html_form =
        XmCreateForm(m_wPageForm, "mailnewsHTML", av,ac);
    XtManageChild(html_form);
 
    Widget usehtml_frame=createUseHTMLFrame(html_form, NULL);
    /* Widget nohtml_frame= */ createNoHTMLFrame(html_form, usehtml_frame);
 
    setCreated(TRUE);
}
 
Widget XFE_PrefsPageMailNewsHTML::createUseHTMLFrame(Widget parent,
                                                     Widget attachTo)
{
    Widget kids[100];
    Arg    av[50];
    int ac;
    int i=0;
 
    XP_ASSERT(parent);
 
    Widget usehtml_frame;
    ac=0;
    XtSetArg(av[ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
    XtSetArg(av[ac], XmNtopWidget, attachTo); ac++;
    XtSetArg(av[ac], XmNbottomAttachment, XmATTACH_NONE); ac++;
    XtSetArg(av[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
    XtSetArg(av[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
    usehtml_frame =
        XmCreateFrame(parent,"useHTMLFrame",av,ac);
   
    Widget usehtml_form;
    ac=0;
    XtSetArg(av[ac], XmNtopAttachment, XmATTACH_FORM); ac++;
    XtSetArg(av[ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
    XtSetArg(av[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
    XtSetArg(av[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
    usehtml_form =
        XmCreateForm(usehtml_frame,"useHTMLForm",av,ac);
 
    Widget usehtml_label;
    ac=0;
    XtSetArg(av[ac], XmNchildType, XmFRAME_TITLE_CHILD); ac++;
    usehtml_label =
        XmCreateLabelGadget(usehtml_frame, "useHTML",av,ac);
 
    // child Widgets
 
    Widget usehtml_toggle;
    kids[i++] = usehtml_toggle =
        XmCreateToggleButtonGadget(usehtml_form, "useHTMLToggle",av,ac);
 
    m_usehtml_toggle=usehtml_toggle;
 
    XtVaSetValues(usehtml_toggle,
                  XmNtopAttachment, XmATTACH_FORM,
                  XmNleftAttachment, XmATTACH_FORM,
                  XmNrightAttachment, XmATTACH_NONE,
                  XmNbottomAttachment, XmATTACH_NONE,
                  NULL);
 
    XtManageChildren(kids,i);
    XtManageChild(usehtml_frame);
    XtManageChild(usehtml_form);
    XtManageChild(usehtml_label);
   
    return usehtml_frame;
}
 
Widget XFE_PrefsPageMailNewsHTML::createNoHTMLFrame(Widget parent,
                                                    Widget attachTo)
{
    Widget kids[100];
    Arg    av[50];
    int ac;
    int i=0;
 
    XP_ASSERT(parent);
 
    Widget nohtml_frame;
    ac=0;
    XtSetArg(av[ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
    XtSetArg(av[ac], XmNtopWidget, attachTo); ac++;
    XtSetArg(av[ac], XmNbottomAttachment, XmATTACH_NONE); ac++;
    XtSetArg(av[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
    XtSetArg(av[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
    nohtml_frame =
        XmCreateFrame(parent,"noHTMLFrame",av,ac);
   
    Widget nohtml_form;
    ac=0;
    XtSetArg(av[ac], XmNtopAttachment, XmATTACH_FORM); ac++;
    XtSetArg(av[ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
    XtSetArg(av[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
    XtSetArg(av[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
    nohtml_form =
        XmCreateForm(nohtml_frame,"noHTMLForm",av,ac);
 
 
    Widget nohtml_label;
   
    ac=0;
    XtSetArg(av[ac], XmNchildType, XmFRAME_TITLE_CHILD); ac++;
    nohtml_label =
        XmCreateLabelGadget(nohtml_frame, "noHTML",av,ac);
 
    Widget nohtml_radio_rb;
    Widget nohtml_override_label;
 
    ac=0;
    kids[i++] = nohtml_radio_rb =
        XmCreateRadioBox(nohtml_form, "noHTMLRB", av, ac);
    kids[i++] = nohtml_override_label =
        XmCreateLabelGadget(nohtml_form,"noHTMLoverride", av, ac);
   
    XtVaSetValues(nohtml_radio_rb,
                  XmNtopAttachment, XmATTACH_FORM,
                  XmNleftAttachment, XmATTACH_FORM,
                  XmNrightAttachment, XmATTACH_NONE,
                  XmNbottomAttachment, XmATTACH_NONE,
                  NULL);
   
    XtVaSetValues(nohtml_override_label,
                  XmNtopAttachment, XmATTACH_WIDGET,
                  XmNtopWidget, nohtml_radio_rb,
                  XmNleftAttachment, XmATTACH_FORM,
                  XmNrightAttachment, XmATTACH_NONE,
                  XmNbottomAttachment, XmATTACH_NONE,
                  NULL);
 
    XtManageChildren(kids,i);
 
    i=0;
    ac=0;
    kids[i++] = m_nohtml_ask_toggle =
        XmCreateToggleButtonGadget(nohtml_radio_rb, "noHTMLAsk", av, ac);
    kids[i++] = m_nohtml_text_toggle =
        XmCreateToggleButtonGadget(nohtml_radio_rb, "noHTMLText", av, ac);
    kids[i++] = m_nohtml_html_toggle =
        XmCreateToggleButtonGadget(nohtml_radio_rb, "noHTMLHTML", av, ac);
    kids[i++] = m_nohtml_both_toggle =
        XmCreateToggleButtonGadget(nohtml_radio_rb, "noHTMLBoth", av, ac);
 
    XtManageChildren(kids,i);
    XtManageChild(nohtml_radio_rb);
    XtManageChild(nohtml_frame);
    XtManageChild(nohtml_form);
    XtManageChild(nohtml_label);
 
    return nohtml_frame;
}
// Member:       init
// Description:  
// Inputs:
// Side effects: 
 
void XFE_PrefsPageMailNewsHTML::init()
{
  
    Bool boolval;
    Bool locked;
    int32 intval;
 
   
    PREF_GetBoolPref("mail.html_compose",&boolval);
    locked = PREF_PrefIsLocked("mail.html_compose");
    XtVaSetValues(m_usehtml_toggle,
                  XmNset, boolval,
                  XmNsensitive, !locked,
                  NULL);
                 
    PREF_GetIntPref("mail.default_html_action",&intval);
    locked = PREF_PrefIsLocked("mail.default_html_action");
    XtVaSetValues(m_nohtml_ask_toggle,
                  XmNset, (intval == HTML_ACTION_ASK),
                  XmNsensitive, !locked,
                  NULL);
    XtVaSetValues(m_nohtml_text_toggle,
                  XmNset, (intval == HTML_ACTION_TEXT),
                  XmNsensitive, !locked,
                  NULL);
    XtVaSetValues(m_nohtml_html_toggle,
                  XmNset, (intval == HTML_ACTION_HTML),
                  XmNsensitive, !locked,
                  NULL);
    XtVaSetValues(m_nohtml_both_toggle,
                  XmNset, (intval == HTML_ACTION_BOTH),
                  XmNsensitive, !locked,
                  NULL);
 
    setInitialized(TRUE);
}
 
// Member:       install
// Description:  
// Inputs:
// Side effects: 
 
void XFE_PrefsPageMailNewsHTML::install()
{
}
 
 
// Member:       save
// Description:  
// Inputs:
// Side effects: 
 
void XFE_PrefsPageMailNewsHTML::save()
{
    Bool boolval;
 
    boolval = XmToggleButtonGadgetGetState(m_usehtml_toggle);
    PREF_SetBoolPref("mail.html_compose",boolval);
 
    boolval = XmToggleButtonGadgetGetState(m_nohtml_ask_toggle);
    if (boolval) PREF_SetIntPref("mail.default_html_action",HTML_ACTION_ASK);
 
    boolval = XmToggleButtonGadgetGetState(m_nohtml_text_toggle);
    if (boolval) PREF_SetIntPref("mail.default_html_action",HTML_ACTION_TEXT);
 
    boolval = XmToggleButtonGadgetGetState(m_nohtml_html_toggle);
    if (boolval) PREF_SetIntPref("mail.default_html_action",HTML_ACTION_HTML);
 
    boolval = XmToggleButtonGadgetGetState(m_nohtml_both_toggle);
    if (boolval) PREF_SetIntPref("mail.default_html_action",HTML_ACTION_BOTH);
   
    install();
   
   
}
 
// Member:       verify
// Description:  
// Inputs:
// Side effects: 
 
Boolean XFE_PrefsPageMailNewsHTML::verify()
{
    return TRUE;
}
 
#endif  /* MOZ_MAIL_NEWS */
 
#ifdef MOZ_MAIL_NEWS
 
// ************************************************************************
// ************************ Mail News Return Receipts  ********************
// ************************************************************************
 
 // Member:       XFE_PrefsPageMailNewsReceipts
 // Description:  Constructor
 // Inputs:
 // Side effects: 
 
XFE_PrefsPageMailNewsReceipts::XFE_PrefsPageMailNewsReceipts(XFE_PrefsDialog *dialog)
    : XFE_PrefsPage(dialog)
{
}
 
// Member:       ~XFE_PrefsPageMailNewsReceipts
// Description:  destructor
// Inputs:
// Side effects: 
 
XFE_PrefsPageMailNewsReceipts::~XFE_PrefsPageMailNewsReceipts()
{
}
 
// Member:       create
// Description:  Creates widgets
// Inputs:
// Side effects: 
 
void XFE_PrefsPageMailNewsReceipts::create()
{
    Arg    av[10];
    int    ac;
 
 
    ac=0;
    XtSetArg(av[ac], XmNtopAttachment, XmATTACH_FORM); ac++;
    XtSetArg(av[ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
    XtSetArg(av[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
    XtSetArg(av[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
 
    Widget receipts_form;
    m_wPage = receipts_form=
        XmCreateForm(m_wPageForm, "mailnewsReceipts", av,ac);
 
    Widget frame1, frame2, frame3, kids[5];
    int i =0;
    kids[i++]=frame1=createRequestReceiptsFrame(receipts_form, NULL);
    XtVaSetValues(frame1, XmNtopAttachment, XmATTACH_FORM, 0);
 
    kids[i++]=frame2=createReceiptsArriveFrame(receipts_form, frame1);
    kids[i++]=frame3=createReceiveReceiptsFrame(receipts_form, frame2);
     
    XtManageChildren(kids,i);
    XtManageChild(receipts_form);
 
    setCreated(TRUE);
}
 
Widget XFE_PrefsPageMailNewsReceipts::createRequestReceiptsFrame(Widget parent,
                                                                 Widget attachTo)
{
    Widget kids[10];
    Arg    av[10];
    int ac;
    int i=0;
 
    XP_ASSERT(parent);
 
    ac=0;
    XtSetArg(av[ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
    XtSetArg(av[ac], XmNtopWidget, attachTo); ac++;
    XtSetArg(av[ac], XmNbottomAttachment, XmATTACH_NONE); ac++;
    XtSetArg(av[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
    XtSetArg(av[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
    Widget frame 
        = XmCreateFrame(parent,"requestReceiptsFrame",av,ac);
   
    ac=0;
    XtSetArg(av[ac], XmNchildType, XmFRAME_TITLE_CHILD); ac++;
    Widget label
        = XmCreateLabelGadget(frame, "requestReceiptsLabel",av,ac);
 
    ac=0;
    XtSetArg(av[ac], XmNtopAttachment, XmATTACH_FORM); ac++;
    XtSetArg(av[ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
    XtSetArg(av[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
    XtSetArg(av[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
    Widget form
        = XmCreateForm(frame,"requestReceiptsForm",av,ac);
 
    // child Widgets
 
    ac=0;
    Widget radioBox=
        XmCreateRadioBox(form, "requestReceiptsRB", av, ac);
   
    XtVaSetValues(radioBox,
                  XmNtopAttachment, XmATTACH_FORM,
                  XmNleftAttachment, XmATTACH_FORM,
                  XmNrightAttachment, XmATTACH_NONE,
                  XmNbottomAttachment, XmATTACH_NONE,
                  NULL);
   
 
    i=0;
    ac=0;
    kids[i++] = m_dsn_toggle =
        XmCreateToggleButtonGadget(radioBox, "dsn", av, ac);
    kids[i++] = m_mdn_toggle =
        XmCreateToggleButtonGadget(radioBox, "mdn", av, ac);
    kids[i++] = m_both_toggle =
        XmCreateToggleButtonGadget(radioBox, "both", av, ac);
 
    XtManageChildren(kids,i);
    XtManageChild(radioBox);
    XtManageChild(form);
    XtManageChild(label);
 
    return frame;
}
 
Widget XFE_PrefsPageMailNewsReceipts::createReceiptsArriveFrame(Widget parent,
                                                                Widget attachTo)
{
    Widget kids[10];
    Arg    av[50];
    int ac;
    int i=0;
 
    XP_ASSERT(parent);
 
    ac=0;
    XtSetArg(av[ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
    XtSetArg(av[ac], XmNtopWidget, attachTo); ac++;
    XtSetArg(av[ac], XmNbottomAttachment, XmATTACH_NONE); ac++;
    XtSetArg(av[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
    XtSetArg(av[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
    Widget frame 
        = XmCreateFrame(parent,"receiptsArriveFrame",av,ac);
   
    ac=0;
    XtSetArg(av[ac], XmNchildType, XmFRAME_TITLE_CHILD); ac++;
    Widget label
        = XmCreateLabelGadget(frame, "receiptsArriveLabel",av,ac);
 
    ac=0;
    XtSetArg(av[ac], XmNtopAttachment, XmATTACH_FORM); ac++;
    XtSetArg(av[ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
    XtSetArg(av[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
    XtSetArg(av[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
    Widget form
        = XmCreateForm(frame,"receiptsArriveForm",av,ac);
 
    // child Widgets
 
    ac=0;
    Widget radioBox=
        XmCreateRadioBox(form, "receiptsArriveRB", av, ac);
   
    XtVaSetValues(radioBox,
                  XmNtopAttachment, XmATTACH_FORM,
                  XmNleftAttachment, XmATTACH_FORM,
                  XmNrightAttachment, XmATTACH_NONE,
                  XmNbottomAttachment, XmATTACH_NONE,
                  NULL);
   
 
    i=0;
    ac=0;
    kids[i++] = m_inbox_toggle =
        XmCreateToggleButtonGadget(radioBox, "inbox", av, ac);
    kids[i++] = m_sentmail_toggle =
        XmCreateToggleButtonGadget(radioBox, "sentmail", av, ac);
 
    XtManageChildren(kids,i);
    XtManageChild(radioBox);
    XtManageChild(form);
    XtManageChild(label);
 
    return frame;
}
Widget XFE_PrefsPageMailNewsReceipts::createReceiveReceiptsFrame(Widget parent,
                                                                 Widget attachTo)
{
    Widget kids[10];
    Arg    av[50];
    int ac;
    int i=0;
 
    XP_ASSERT(parent);
 
    ac=0;
    XtSetArg(av[ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
    XtSetArg(av[ac], XmNtopWidget, attachTo); ac++;
    XtSetArg(av[ac], XmNbottomAttachment, XmATTACH_NONE); ac++;
    XtSetArg(av[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
    XtSetArg(av[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
    Widget frame 
        = XmCreateFrame(parent,"receiveReceiptsFrame",av,ac);
   
    ac=0;
    XtSetArg(av[ac], XmNchildType, XmFRAME_TITLE_CHILD); ac++;
    Widget label
        = XmCreateLabelGadget(frame, "receiveReceiptsLabel",av,ac);
 
    ac=0;
    XtSetArg(av[ac], XmNtopAttachment, XmATTACH_FORM); ac++;
    XtSetArg(av[ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
    XtSetArg(av[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
    XtSetArg(av[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
    Widget form
        = XmCreateForm(frame,"receiveReceiptsForm",av,ac);
 
    // child Widgets
 
    ac=0;
    Widget radioBox=
        XmCreateRadioBox(form, "receiveReceiptsRB", av, ac);
   
    XtVaSetValues(radioBox,
                  XmNtopAttachment, XmATTACH_FORM,
                  XmNleftAttachment, XmATTACH_FORM,
                  XmNrightAttachment, XmATTACH_NONE,
                  XmNbottomAttachment, XmATTACH_NONE,
                  NULL);
    i=0;
    ac=0;
    kids[i++] = m_never_toggle =
        XmCreateToggleButtonGadget(radioBox, "never", av, ac);
    kids[i++] = m_some_toggle =
        XmCreateToggleButtonGadget(radioBox, "some", av, ac);
 
    XtManageChildren(kids,i);
    XtManageChild(radioBox);
    XtManageChild(form);
    XtManageChild(label);
 
    return frame;
}
 
// Member:       init
// Description:  
// Inputs:
// Side effects: 
 
void XFE_PrefsPageMailNewsReceipts::init()
{
  
    Bool boolval;
    Bool locked;
    int32 intval;
 
    // Request Receipts 
 
    PREF_GetIntPref("mail.request.return_receipt",&intval);
    locked = PREF_PrefIsLocked("mail.request.return_receipt");
 
    XtVaSetValues(m_dsn_toggle,
                  XmNset, (intval == RETURN_RECEIPTS_DSN),
                  XmNsensitive, !locked,
                  NULL);
    XtVaSetValues(m_mdn_toggle,
                  XmNset, (intval == RETURN_RECEIPTS_MDN),
                  XmNsensitive, !locked,
                  NULL);
    XtVaSetValues(m_both_toggle,
                  XmNset, (intval == RETURN_RECEIPTS_BOTH),
                  XmNsensitive, !locked,
                  NULL);
 
   
    // Receipts Arrives
    PREF_GetIntPref("mail.incorporate.return_receipt",&intval);
    locked = PREF_PrefIsLocked("mail.incorporate.return_receipt");
 
    XtVaSetValues(m_inbox_toggle,
                  XmNset, (intval == RECEIPTS_ARRIVE_INBOX),
                  XmNsensitive, !locked,
                  NULL);
 
    XtVaSetValues(m_sentmail_toggle,
                  XmNset, (intval == RECEIPTS_ARRIVE_SENTMAIL),
                  XmNsensitive, !locked,
                  NULL);
 
    // Receive Receipts 
    PREF_GetBoolPref("mail.request.return_receipt_on",&boolval);
    locked = PREF_PrefIsLocked("mail.request.return_receipt_on");
 
    XtVaSetValues(m_never_toggle,
                  XmNset, boolval,
                  XmNsensitive, !locked,
                  NULL);
                 
    XtVaSetValues(m_some_toggle,
                  XmNset, !boolval,
                  XmNsensitive, !locked,
                  NULL);
     
    setInitialized(TRUE);
}
 
// Member:       install
// Description:  
// Inputs:
// Side effects: 
 
void XFE_PrefsPageMailNewsReceipts::install()
{
}
 
 
// Member:       save
// Description:  
// Inputs:
// Side effects: 
 
void XFE_PrefsPageMailNewsReceipts::save()
{
    Bool boolval;
 
    boolval = XmToggleButtonGadgetGetState(m_dsn_toggle);
    if (boolval) PREF_SetIntPref("mail.request.return_receipt", RETURN_RECEIPTS_DSN);
 
    boolval = XmToggleButtonGadgetGetState(m_mdn_toggle);
    if (boolval) PREF_SetIntPref("mail.request.return_receipt", RETURN_RECEIPTS_MDN);
 
    boolval = XmToggleButtonGadgetGetState(m_both_toggle);
    if (boolval) PREF_SetIntPref("mail.request.return_receipt", RETURN_RECEIPTS_BOTH);
 
    boolval = XmToggleButtonGadgetGetState(m_inbox_toggle);
    if (boolval) PREF_SetIntPref("mail.incorporate.return_receipt", RECEIPTS_ARRIVE_INBOX);
 
    boolval = XmToggleButtonGadgetGetState(m_sentmail_toggle);
    if (boolval) PREF_SetIntPref("mail.incorporate.return_receipt", RECEIPTS_ARRIVE_SENTMAIL);
 
    boolval = XmToggleButtonGadgetGetState(m_never_toggle);
    PREF_SetBoolPref("mail.request.return_receipt_on",boolval);
 
    install();
   
   
}
 
// Member:       verify
// Description:  
// Inputs:
// Side effects: 
 
Boolean XFE_PrefsPageMailNewsReceipts::verify()
{
    return TRUE;
}

#endif // MOZ_MAIL_NEWS

// ************************************************************************
// ********************** Miscellaneous functions *************************
// ************************************************************************

