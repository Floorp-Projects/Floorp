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
#include "PrefsMserverMore.h"
#include "PrefsMsgMore.h"
#include "ColorDialog.h"
#include "PrefsLdapProp.h"
#include "dirprefs.h"
#include "addrbk.h"
#include "dirprefs.h"
#include "xp_list.h"
#endif
#include "prefapi.h"

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
extern int MK_MSG_MAIL_DIRECTORY_CHANGED;

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
	void      fe_installMailNewsComposition();
	void      fe_installMailNewsMserver();
	void      fe_installMailNewsNserver();
	void      fe_installMailNewsAddrBook();
	XP_List  *FE_GetDirServers();
	ABook    *fe_GetABook(MWContext *context);
	void      fe_showABCardPropertyDlg(Widget parent,
									   MWContext *context,
									   ABID entry,
									   XP_Bool newuser);
	void      fe_SwatchSetColor(Widget widget, LO_Color* color);

	static void fe_set_quoted_text_styles(PrefsDataMailNews *);
	static void fe_set_quoted_text_sizes(PrefsDataMailNews *);
	static void fe_fcc_add_folders(XP_List *paths, XP_List *folders, 
								   MSG_FolderInfo *info, 
								   char *default_folder_path,
								   int default_folder_index, int depth);
	static char *fe_GetFolderPathFromInfo(MSG_FolderInfo *f);
	static char *fe_GetFolderNameFromInfo(MSG_FolderInfo *f);
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
    sprintf(buf, "#%02x%02x%02x", color.red, color.green, color.blue);

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
// **********************  Mail News/Composition   ************************
// ************************************************************************

// Member:       XFE_PrefsPageMailNewsComposition
// Description:  Constructor
// Inputs:
// Side effects: 

XFE_PrefsPageMailNewsComposition::XFE_PrefsPageMailNewsComposition(XFE_PrefsDialog *dialog)
	: XFE_PrefsPage(dialog),
	  m_prefsDataMailNewsComposition(0)
{
}

// Member:       ~XFE_PrefsPageMailNewsComposition
// Description:  Destructor
// Inputs:
// Side effects: 

XFE_PrefsPageMailNewsComposition::~XFE_PrefsPageMailNewsComposition()
{
	delete m_prefsDataMailNewsComposition;
}

// Member:       create
// Description:  Creates page for MailNews/Composition
// Inputs:
// Side effects: 

void XFE_PrefsPageMailNewsComposition::create()
{
	Widget            kids[100];
	Arg               av[50];
	int               ac;
	int               i;

	PrefsDataMailNewsComposition *fep = NULL;

	fep = new PrefsDataMailNewsComposition;
	memset(fep, 0, sizeof(PrefsDataMailNewsComposition));
	m_prefsDataMailNewsComposition = fep;

	fep->context = getContext();
	fep->prompt_dialog = getPrefsDialog()->getDialogChrome();

	Widget form;

	ac = 0;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	form = XmCreateForm (m_wPageForm, "mailnewsCompose", av, ac);
	XtManageChild (form);
	m_wPage = fep->page = form;

	// Message Properties

	Widget frame1;

	ac = 0;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_NONE); ac++;
	frame1 = XmCreateFrame (form, "mailnewsCompFrame", av, ac);

	Widget form1;

	ac = 0;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
	form1 = XmCreateForm (frame1, "mailnewsCompBox", av, ac);

	Widget label1;

	ac = 0;
	XtSetArg (av [ac], XmNchildType, XmFRAME_TITLE_CHILD); ac++;
	label1 = XmCreateLabelGadget (frame1, "msgPropLabel", av, ac);

	ac = 0;
	i = 0;

	// Auto quote original message

	Widget send_html_msg_toggle;
	Widget auto_quote_toggle;
	Widget wrap_label;
	Widget wrap_length_text;
	Widget char_label;

	kids[i++] = send_html_msg_toggle =
		XmCreateToggleButtonGadget(form1, "sendHtmlMsgToggle", av, ac);

	kids[i++] = auto_quote_toggle =
		XmCreateToggleButtonGadget(form1, "autoQuoteToggle", av, ac);

	kids[i++] = wrap_label =
		XmCreateLabelGadget(form1, "wrapLabel", av, ac);

	kids[i++] = char_label =
		XmCreateLabelGadget(form1, "charLabel", av, ac);

	kids[i++] = wrap_length_text =
		fe_CreateTextField(form1, "wrapLengthText", av, ac);

	fep->auto_quote_toggle = auto_quote_toggle;
	fep->send_html_msg_toggle = send_html_msg_toggle;
	fep->wrap_length_text = wrap_length_text;

	XtVaSetValues(send_html_msg_toggle,
				  XmNindicatorType, XmN_OF_MANY,
				  XmNtopAttachment, XmATTACH_FORM,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtVaSetValues(auto_quote_toggle,
				  XmNindicatorType, XmN_OF_MANY,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, send_html_msg_toggle,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	int labels_height;
	labels_height = XfeVaGetTallestWidget(wrap_label,
										  wrap_length_text,
										  NULL);

	XtVaSetValues(wrap_label,
				  XmNalignment, XmALIGNMENT_BEGINNING,
				  XmNheight, labels_height,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, auto_quote_toggle,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtVaSetValues(wrap_length_text,
				  XmNcolumns, 3,
				  XmNheight, labels_height,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, wrap_label,
				  XmNleftAttachment, XmATTACH_WIDGET,
				  XmNleftWidget, wrap_label,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtVaSetValues(char_label,
				  XmNalignment, XmALIGNMENT_BEGINNING,
				  XmNheight, labels_height,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, wrap_label,
				  XmNleftAttachment, XmATTACH_WIDGET,
				  XmNleftWidget, wrap_length_text,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtManageChildren(kids, i);
	XtManageChild(label1);
	XtManageChild(form1);
	XtManageChild(frame1);

	// Copies of outgoing messages

	Widget frame2;

	ac = 0;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
	XtSetArg (av [ac], XmNtopWidget, frame1); ac++;
	XtSetArg (av [ac], XmNtopOffset, 8); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_NONE); ac++;
	frame2 = XmCreateFrame (form, "ccFrame", av, ac);

	Widget form2;

	ac = 0;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
	form2 = XmCreateForm (frame2, "ccBox", av, ac);

	Widget label2;

	ac = 0;
	XtSetArg (av [ac], XmNchildType, XmFRAME_TITLE_CHILD); ac++;
	label2 = XmCreateLabelGadget (frame2, "ccLabel", av, ac);

	// Email a copy of outgoing message to

	Widget email_label;
	Widget mail_msg_label;
	Widget news_msg_label;
	Widget mail_email_self_toggle;
	Widget mail_email_other_label;
	Widget mail_email_other_text;
	Widget news_email_self_toggle;
	Widget news_email_other_label;
	Widget news_email_other_text;

	ac = 0;
	i = 0;

	kids[i++] = email_label = 
		XmCreateLabelGadget(form2, "emailLabel", av, ac);

	kids[i++] = mail_msg_label = 
		XmCreateLabelGadget(form2, "mailMsgLabel", av, ac);

	kids[i++] = news_msg_label = 
		XmCreateLabelGadget(form2, "newsMsgLabel", av, ac);

	kids[i++] = mail_email_self_toggle =
		XmCreateToggleButtonGadget(form2, "mEmailSelfToggle", av, ac);

	kids[i++] = news_email_self_toggle =
		XmCreateToggleButtonGadget(form2, "nEmailSelfToggle", av, ac);

	kids[i++] = mail_email_other_label = 
		XmCreateLabelGadget(form2, "mEmailOtherLabel", av, ac);

	kids[i++] = news_email_other_label = 
		XmCreateLabelGadget(form2, "nEmailOtherLabel", av, ac);

	kids[i++] = mail_email_other_text = 
		fe_CreateTextField(form2, "mEmailOtherText", av, ac);

	kids[i++] = news_email_other_text = 
		fe_CreateTextField(form2, "nEmailOtherText", av, ac);

	fep->mail_email_self_toggle = mail_email_self_toggle;
	fep->mail_email_other_text = mail_email_other_text;
	fep->news_email_self_toggle = news_email_self_toggle;
	fep->news_email_other_text = news_email_other_text;

	XtVaSetValues(email_label,
				  XmNalignment, XmALIGNMENT_BEGINNING,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, auto_quote_toggle,
				  XmNtopOffset, 8,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	int labels_width;
	labels_width = XfeVaGetWidestWidget(mail_msg_label,
										news_msg_label,
										NULL);

	labels_height = XfeVaGetTallestWidget(mail_msg_label,
										  mail_email_self_toggle,
										  mail_email_other_label,
										  mail_email_other_text,
										  NULL);

	XtVaSetValues(mail_msg_label,
				  XmNalignment, XmALIGNMENT_BEGINNING,
				  XmNwidth, labels_width,
				  XmNheight, labels_height,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, email_label,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNleftOffset, 26,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtVaSetValues(mail_email_self_toggle,
				  XmNindicatorType, XmN_OF_MANY,
				  XmNheight, labels_height,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, mail_msg_label,
				  XmNleftAttachment, XmATTACH_WIDGET,
				  XmNleftWidget, mail_msg_label,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtVaSetValues(mail_email_other_label,
				  XmNheight, labels_height,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, mail_msg_label,
				  XmNleftAttachment, XmATTACH_WIDGET,
				  XmNleftWidget, mail_email_self_toggle,
				  XmNleftOffset, 8,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtVaSetValues(mail_email_other_text,
				  XmNcolumns, 20,
				  XmNheight, labels_height,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, mail_msg_label,
				  XmNleftAttachment, XmATTACH_WIDGET,
				  XmNleftWidget, mail_email_other_label,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtVaSetValues(news_msg_label,
				  XmNalignment, XmALIGNMENT_BEGINNING,
				  XmNwidth, labels_width,
				  XmNheight, labels_height,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, mail_msg_label,
				  XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNleftWidget, mail_msg_label,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtVaSetValues(news_email_self_toggle,
				  XmNindicatorType, XmN_OF_MANY,
				  XmNheight, labels_height,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, news_msg_label,
				  XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNleftWidget, mail_email_self_toggle,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtVaSetValues(news_email_other_label,
				  XmNalignment, XmALIGNMENT_BEGINNING,
				  XmNheight, labels_height,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, news_msg_label,
				  XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNleftWidget, mail_email_other_label,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtVaSetValues(news_email_other_text,
				  XmNcolumns, 20,
				  XmNheight, labels_height,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, news_msg_label,
				  XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNleftWidget, mail_email_other_text,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	// FCC

	Visual    *v = 0;
	Colormap   cmap = 0;
	Cardinal   depth = 0;

	XtVaGetValues(getPrefsDialog()->getPrefsParent(),
				  XtNvisual, &v,
				  XtNcolormap, &cmap,
				  XtNdepth, &depth, 
				  0);

	Widget fcc_label;
	Widget mail_fcc_toggle;
	Widget mail_fcc_combo;
	Widget news_fcc_toggle;
	Widget news_fcc_combo;

	ac = 0;
	kids[i++] = fcc_label = 
		XmCreateLabelGadget(form2, "fccLabel", av, ac);

	kids[i++] = mail_fcc_toggle =
		XmCreateToggleButtonGadget(form2, "mailFolderToggle", av, ac);

	kids[i++] = news_fcc_toggle =
		XmCreateToggleButtonGadget(form2, "newsFolderToggle", av, ac);

	ac = 0;
	XtSetArg(av[ac], XmNvisual, v); ac++;
	XtSetArg(av[ac], XmNdepth, depth); ac++;
	XtSetArg(av[ac], XmNcolormap, cmap); ac++;
	XtSetArg(av[ac], XmNmoveSelectedItemUp, False); ac++;
	XtSetArg(av[ac], XmNtype, XmDROP_DOWN_LIST_BOX); ac++;
	XtSetArg(av[ac], XmNarrowType, XmMOTIF); ac++;
	kids[i++] = mail_fcc_combo = DtCreateComboBox(form2, "mailFccCombo", av, ac);

	ac = 0;
	XtSetArg(av[ac], XmNvisual, v); ac++;
	XtSetArg(av[ac], XmNdepth, depth); ac++;
	XtSetArg(av[ac], XmNcolormap, cmap); ac++;
	XtSetArg(av[ac], XmNmoveSelectedItemUp, False); ac++;
	XtSetArg(av[ac], XmNtype, XmDROP_DOWN_LIST_BOX); ac++;
	XtSetArg(av[ac], XmNarrowType, XmMOTIF); ac++;
	kids[i++] = news_fcc_combo = DtCreateComboBox(form2, "newsFccCombo", av, ac);

	fep->mail_fcc_toggle = mail_fcc_toggle;
	fep->mail_fcc_combo = mail_fcc_combo;
	fep->news_fcc_toggle = news_fcc_toggle;
	fep->news_fcc_combo = news_fcc_combo;

	XtVaSetValues(fcc_label,
				  XmNalignment, XmALIGNMENT_BEGINNING,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, news_msg_label,
				  XmNtopOffset, 8,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	int labels_width2;
	labels_width2 = XfeVaGetWidestWidget(mail_fcc_toggle,
										 news_fcc_toggle,
										 NULL);

	int labels_height2;
	labels_height2 = XfeVaGetTallestWidget(mail_fcc_toggle,
										   mail_fcc_combo,
										   NULL);

	XtVaSetValues(mail_fcc_toggle,
				  XmNindicatorType, XmN_OF_MANY,
				  XmNheight, labels_height2,
				  XmNwidth, labels_width2,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, fcc_label,
				  XmNtopOffset, 8,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNleftOffset, 16,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtVaSetValues(mail_fcc_combo,
				  XmNheight, labels_height2,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, mail_fcc_toggle,
				  XmNleftAttachment, XmATTACH_WIDGET,
				  XmNleftWidget, mail_fcc_toggle,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtVaSetValues(news_fcc_toggle,
				  XmNindicatorType, XmN_OF_MANY,
				  XmNwidth, labels_width2,
				  XmNheight, labels_height2,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, mail_fcc_toggle,
				  XmNtopOffset, 8,
				  XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNleftWidget, mail_fcc_toggle,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtVaSetValues(news_fcc_combo,
				  XmNheight, labels_height2,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, news_fcc_toggle,
				  XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNleftWidget, mail_fcc_combo,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtManageChildren(kids, i);
	XtManageChild(label2);
	XtManageChild(form2);
	XtManageChild(frame2);

	Widget more_button;

	ac = 0;
	i = 0;

	kids[i++] = more_button =
		XmCreatePushButtonGadget(form, "moreButton", av, ac);

	XtVaSetValues(more_button,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, frame2,
				  XmNtopOffset, 8,
				  XmNleftAttachment, XmATTACH_NONE,
				  XmNrightAttachment, XmATTACH_FORM,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);
	XtAddCallback(more_button, XmNactivateCallback, cb_more, this);

	XtManageChildren(kids, i);

	setCreated(TRUE);
}

// Member:       init
// Description:  Initializes page for MailNewsComposition
// Inputs:
// Side effects: 

void XFE_PrefsPageMailNewsComposition::init()
{
	XP_ASSERT(m_prefsDataMailNewsComposition);

	PrefsDataMailNewsComposition  *fep = m_prefsDataMailNewsComposition;
	XFE_GlobalPrefs               *prefs = &fe_globalPrefs;
	char                           buf[1024];

	// BCC

	XtVaSetValues(fep->mail_email_self_toggle, XmNset, prefs->mailbccself_p, 0);
	fe_SetTextField(fep->mail_email_other_text, prefs->mail_bcc);

	XtVaSetValues(fep->news_email_self_toggle, XmNset, prefs->newsbccself_p, 0);
	fe_SetTextField(fep->news_email_other_text, prefs->news_bcc);

	// FCC

	XtVaSetValues(fep->mail_fcc_toggle, XmNset, prefs->mailfcc_p, 0);
	setFccMenu(TRUE);

	XtVaSetValues(fep->news_fcc_toggle, XmNset, prefs->newsfcc_p, 0);
	setFccMenu(FALSE);

	XtVaSetValues(fep->auto_quote_toggle, XmNset, prefs->autoquote_reply, 0);

	// send_html_msg_toggle

	XtVaSetValues(fep->send_html_msg_toggle, XmNset, prefs->send_html_msg, 0);

	// wrap length

	PR_snprintf(buf, sizeof(buf), "%d", prefs->msg_wrap_length);
	XtVaSetValues(fep->wrap_length_text, XmNvalue, buf, 0);


    // If the preference is locked, grey it out.
    XtSetSensitive(fep->mail_email_self_toggle,
                   !PREF_PrefIsLocked("mail.cc_self"));
    XtSetSensitive(fep->mail_email_other_text,
                   !PREF_PrefIsLocked("mail.default_cc"));
    XtSetSensitive(fep->news_email_self_toggle,
                   !PREF_PrefIsLocked("news.cc_self"));
    XtSetSensitive(fep->news_email_other_text,
                   !PREF_PrefIsLocked("news.default_cc"));
    XtSetSensitive(fep->mail_fcc_toggle,
                   !PREF_PrefIsLocked("mail.use_fcc"));
    XtSetSensitive(fep->mail_fcc_combo,
                   !PREF_PrefIsLocked("mail.default_fcc"));
    XtSetSensitive(fep->news_fcc_toggle,
                   !PREF_PrefIsLocked("news.use_fcc"));
    XtSetSensitive(fep->news_fcc_combo,
                   !PREF_PrefIsLocked("news.default_fcc"));
    XtSetSensitive(fep->auto_quote_toggle,
                   !PREF_PrefIsLocked("mail.auto_quote"));
    XtSetSensitive(fep->send_html_msg_toggle,
                   !PREF_PrefIsLocked("mail.send_html"));
    XtSetSensitive(fep->wrap_length_text,
                   !PREF_PrefIsLocked("mailnews.wraplength"));

	setInitialized(TRUE);
}

// Member:       install
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageMailNewsComposition::install()
{
	fe_installMailNewsComposition();
}

// Member:       save
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageMailNewsComposition::save()
{
	PrefsDataMailNewsComposition *fep = m_prefsDataMailNewsComposition;

	XP_ASSERT(fep);

	// BCC

	PREFS_SET_GLOBALPREF_TEXT(mail_bcc, mail_email_other_text);
	PREFS_SET_GLOBALPREF_TEXT(news_bcc, news_email_other_text);
	XtVaGetValues(fep->mail_email_self_toggle, 
				  XmNset, &fe_globalPrefs.mailbccself_p, 
				  0);
	XtVaGetValues(fep->news_email_self_toggle,
				  XmNset, &fe_globalPrefs.newsbccself_p, 
				  0);

	// FCC

	if (fe_globalPrefs.mail_fcc) {
		free(fe_globalPrefs.mail_fcc); 
		fe_globalPrefs.mail_fcc = 0;
	}

	if (fe_globalPrefs.news_fcc) {
		free(fe_globalPrefs.news_fcc); 
		fe_globalPrefs.news_fcc = 0;
	}

	XtVaGetValues(fep->mail_fcc_toggle, 
				  XmNset, &fe_globalPrefs.mailfcc_p, 
				  0);
	XtVaGetValues(fep->news_fcc_toggle,
				  XmNset, &fe_globalPrefs.newsfcc_p, 
				  0);

	// If there is no folder, unselect the fcc toggle

	if ((fe_globalPrefs.mailfcc_p) &&
		(XP_ListCount(fep->mail_fcc_folders) == 0))
		fe_globalPrefs.mailfcc_p = FALSE;

	if ((fe_globalPrefs.newsfcc_p) &&
		(XP_ListCount(fep->news_fcc_folders) == 0))
		fe_globalPrefs.newsfcc_p = FALSE;
		
	// Find out which folder was selected

	int       pos;

	if ((fe_globalPrefs.mailfcc_p) &&
		(XP_ListCount(fep->mail_fcc_folders) > 0)){
		XtVaGetValues(fep->mail_fcc_combo, XmNselectedPosition, &pos, 0);
		fe_globalPrefs.mail_fcc = 
			XP_STRDUP((char *)XP_ListGetObjectNum(fep->mail_fcc_paths, pos+1));
		}
	else 
		fe_globalPrefs.mail_fcc = XP_STRDUP("");

	if ((fe_globalPrefs.newsfcc_p) &&
		(XP_ListCount(fep->news_fcc_folders) > 0)){
		XtVaGetValues(fep->news_fcc_combo, XmNselectedPosition, &pos, 0);
		fe_globalPrefs.news_fcc = 
			XP_STRDUP((char *)XP_ListGetObjectNum(fep->news_fcc_paths, pos+1));
	}
	else
		fe_globalPrefs.news_fcc = XP_STRDUP("");

	// Auto quote when replying

	XtVaGetValues(fep->auto_quote_toggle,
				  XmNset, &fe_globalPrefs.autoquote_reply, 
				  0);

	// Send HTML messages

	XtVaGetValues(fep->send_html_msg_toggle,
				  XmNset, &fe_globalPrefs.send_html_msg, 
				  0);

	// Wrap length

    char                    c;
	char                   *s = 0;
    int                     n;

	XtVaGetValues (fep->wrap_length_text, XmNvalue, &s, 0);
	if (1 == sscanf (s, " %d %c", &n, &c) &&
		n > 0)
		fe_globalPrefs.msg_wrap_length = n;
	if (s) XtFree(s);

	// Install preferences

	install();
}

// Member:       getData
// Description:  
// Inputs:
// Side effects: 

PrefsDataMailNewsComposition *XFE_PrefsPageMailNewsComposition::getData()
{
	return m_prefsDataMailNewsComposition;
}

// Member:       setFccMenu
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageMailNewsComposition::setFccMenu(Boolean isMail)
{
	PrefsDataMailNewsComposition *fep = m_prefsDataMailNewsComposition;
	XFE_GlobalPrefs              *prefs = &fe_globalPrefs;

	Widget   combo  = isMail ? fep->mail_fcc_combo : fep->news_fcc_combo;
	XP_List *folders = isMail ? fep->mail_fcc_folders : fep->news_fcc_folders;
	XP_List *paths = isMail ? fep->mail_fcc_paths : fep->news_fcc_paths;
	char    *pref_folder_path = isMail ? prefs->mail_fcc : prefs->news_fcc;

	// Remove existing children 

	XmString  xms;
	int       i;
	char     *path;
	char     *folder;

	if (folders) {
		int count = XP_ListCount(folders);
		if (count > 0) {
			DtComboBoxDeleteAllItems(combo);
			for (i = 0; i < count; i++) {
				folder = (char *)XP_ListGetObjectNum(folders, i+1);
				path = (char *)XP_ListGetObjectNum(paths, i+1);
				XP_FREEIF(folder);
				XP_FREEIF(path);
			}
		}
		XP_ListDestroy(folders);
		XP_ListDestroy(paths);
	}

	if (isMail) {
		fep->mail_fcc_folders = XP_ListNew();
		fep->mail_fcc_paths = XP_ListNew();
	}
	else {
		fep->news_fcc_folders = XP_ListNew();
		fep->news_fcc_paths = XP_ListNew();
	}
	folders = isMail ? fep->mail_fcc_folders : fep->news_fcc_folders;
	paths = isMail ? fep->mail_fcc_paths : fep->news_fcc_paths;

	// Put the preferred folder as the first one on the list if there is one

	char *default_folder_path = (pref_folder_path && XP_STRLEN(pref_folder_path) > 0) ?
		pref_folder_path : 0;
	int default_folder_index = (-1);

	if (default_folder_path) {
		XP_ListAddObjectToEnd(folders, XP_STRDUP(default_folder_path));
		XP_ListAddObjectToEnd(paths, XP_STRDUP(default_folder_path));
		default_folder_index = 0;
	}

	// Now read in all folders

	int              depth = 0;
	int              num_sub_folders = 0;
	MSG_FolderInfo **children = 0;
	MSG_FolderInfo  *localMailTree = MSG_GetLocalMailTree(fe_getMNMaster());

	num_sub_folders = MSG_GetFolderChildren(fe_getMNMaster(), localMailTree, NULL, 0);
	
	if (num_sub_folders > 0) {
		children = (MSG_FolderInfo **)XP_CALLOC(num_sub_folders, sizeof(MSG_FolderInfo*));
		num_sub_folders = 
			MSG_GetFolderChildren(fe_getMNMaster(), localMailTree, children, num_sub_folders);
	}

	for (i = 0; i < num_sub_folders; i++) {
		fe_fcc_add_folders(paths, folders, children[i], default_folder_path, 
						   default_folder_index, depth);
	}

	
	int count = XP_ListCount(folders);
	if (count > 0) {
		for (i = 0; i < count; i++) {
			char *folder_name = (char *)XP_ListGetObjectNum(folders, i+1);
			xms = XmStringCreateLtoR(folder_name, XmFONTLIST_DEFAULT_TAG);
			DtComboBoxAddItem(combo, xms, 0, True);
			if (i == 0) DtComboBoxSelectItem(combo, xms);
			XmStringFree(xms);
		}
	}
	else {
		xms = XmStringCreateLtoR("", XmFONTLIST_DEFAULT_TAG);
		XtVaSetValues(combo, XmNlabelString, xms, 0);
		XmStringFree(xms);
	}

	if (children) XP_FREE(children);
}

// Member:       cb_more
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageMailNewsComposition::cb_more(Widget    /* w */,
											   XtPointer closure,
											   XtPointer /* callData */)
{
	XFE_PrefsPageMailNewsComposition *thePage = (XFE_PrefsPageMailNewsComposition *)closure;
	XFE_PrefsDialog                  *theDialog = thePage->getPrefsDialog();
	Widget                            mainw = theDialog->getBaseWidget();

	// Instantiate a mail server more dialog

	XFE_PrefsMsgMoreDialog *theMoreDialog = 0;

	if ((theMoreDialog =
		 new XFE_PrefsMsgMoreDialog(theDialog, mainw, "prefsMsgMore")) == 0) {
	    fe_perror(thePage->getContext(), XP_GetString(XFE_OUT_OF_MEMORY_URL));
	    return;
	}

	// Initialize and pop up the more dialog

	theMoreDialog->initPage();
	theMoreDialog->show();
}

static void fe_fcc_add_folders(XP_List        *paths,
							   XP_List        *folders,
							   MSG_FolderInfo *info, 
							   char           *default_folder_path,
							   int             default_folder_index,
							   int             depth)
{
	int              num_sub_folders;
	int              i;
	MSG_FolderInfo **children = 0;
	char            *this_folder_path = fe_GetFolderPathFromInfo(info);
	char            *this_folder_name = fe_GetFolderNameFromInfo(info);
	char             folder_buf[256];

	// Create the button if this is different from the default folder

	*folder_buf = '\0';
	for (i = 0; i < depth; i++)
		XP_STRCAT(folder_buf, "  ");
	XP_STRCAT(folder_buf, this_folder_name);

	if ((default_folder_path != 0) &&
		((XP_STRCMP(default_folder_path, this_folder_path) == 0))) {
		// The default folder path is a folder in the mail directory.
		// Replace the label with folder name.
		
		char *folder_name = (char *)XP_ListGetObjectNum(folders, default_folder_index+1);
		XP_ListRemoveObject(folders, folder_name);
		char *folder_path = (char *)XP_ListGetObjectNum(paths, default_folder_index+1);
		XP_ListRemoveObject(paths, folder_path);

		// add the default to the front of the list
		XP_ListAddObject(folders, XP_STRDUP(folder_buf));
		XP_ListAddObject(paths, XP_STRDUP(this_folder_path));
	}
	else {
		// If there is no default folder path, make Sent as the one that gets displayed
		if ((default_folder_path == 0) &&
			(XP_STRCMP(this_folder_name, 
					   XP_GetString(XFE_PREFS_MAIL_FOLDER_SENT)) == 0)) {
			// add the Sent folder to the front of the list
			XP_ListAddObject(folders, XP_STRDUP(folder_buf));
			XP_ListAddObject(paths, XP_STRDUP(this_folder_path));
		}
		else {
			// add this folder to the end of the list
			XP_ListAddObjectToEnd(folders, XP_STRDUP(folder_buf));
			XP_ListAddObjectToEnd(paths, XP_STRDUP(this_folder_path));
		}
	}

	num_sub_folders = MSG_GetFolderChildren(fe_getMNMaster(), info, NULL, 0);

	if (num_sub_folders > 0) {
		children = (MSG_FolderInfo **)XP_CALLOC(num_sub_folders, sizeof(MSG_FolderInfo*));
		num_sub_folders = MSG_GetFolderChildren(fe_getMNMaster(), info, children, num_sub_folders);
	}

	for (i = 0; i < num_sub_folders; i ++) {
		fe_fcc_add_folders(paths, folders, children[i], default_folder_path,
						   default_folder_index, depth+1);
	}

	if (num_sub_folders > 0)
		XP_FREE(children);
}

static char *fe_GetFolderPathFromInfo(MSG_FolderInfo *f) {
	return (char*)MSG_GetFolderNameFromID(f);
}

static char *fe_GetFolderNameFromInfo(MSG_FolderInfo *f) {
	char *name = 0;
	char *path = (char*)MSG_GetFolderNameFromID(f);
	if (strrchr(path, '/'))
		name = strrchr(path, '/') + 1;
	else
		name = path;
	return name;
}

// ************************************************************************
// ************************ Mail News/Mail Server *************************
// ************************************************************************

// Member:       XFE_PrefsPageMailNewsMserver
// Description:  Constructor
// Inputs:
// Side effects: 

XFE_PrefsPageMailNewsMserver::XFE_PrefsPageMailNewsMserver(XFE_PrefsDialog *dialog)
	: XFE_PrefsPage(dialog),
	  m_prefsDataMailNewsMserver(0)
{
}

// Member:       ~XFE_PrefsPageMailNewsMserver
// Description:  Destructor
// Inputs:
// Side effects: 

XFE_PrefsPageMailNewsMserver::~XFE_PrefsPageMailNewsMserver()
{
	delete m_prefsDataMailNewsMserver;
}

// Member:       create
// Description:  Creates page for MailNews/Mail server
// Inputs:
// Side effects: 

void XFE_PrefsPageMailNewsMserver::create()
{
	Widget            kids[100];
	Arg               av[50];
	int               ac;
	int               i;

	PrefsDataMailNewsMserver *fep = NULL;

	fep = new PrefsDataMailNewsMserver;
	memset(fep, 0, sizeof(PrefsDataMailNewsMserver));
	m_prefsDataMailNewsMserver = fep;

	fep->context = getContext();
	fep->prompt_dialog = getPrefsDialog()->getDialogChrome();

	Widget form;

	ac = 0;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	form = XmCreateForm (m_wPageForm, "mailnewsMserver", av, ac);
	XtManageChild (form);
	m_wPage = fep->page = form;

	Widget frame1;

	ac = 0;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_NONE); ac++;
	frame1 = XmCreateFrame (form, "mServerFrame", av, ac);

	Widget form1;

	ac = 0;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
	form1 = XmCreateForm (frame1, "mServerBox", av, ac);

	Widget server_label;
	Widget pop_user_name_label;
	Widget pop_user_name_text;
	Widget incoming_mail_server_label;
	Widget incoming_mail_server_text;
	Widget outgoing_mail_server_label;
	Widget outgoing_mail_server_text;

	// POP user name and Incoming/outgoing mail server

	ac = 0;
	i = 0;

	kids[i++] = server_label = 
		XmCreateLabelGadget(form1, "serverLabel", av, ac);

	kids[i++] = pop_user_name_label = 
		XmCreateLabelGadget(form1, "popUserNameLabel", av, ac);

	kids[i++] = incoming_mail_server_label = 
		XmCreateLabelGadget(form1, "incomingMailServerLabel", av, ac);

	kids[i++] = outgoing_mail_server_label = 
		XmCreateLabelGadget(form1, "outgoingMailServerLabel", av, ac);

	kids[i++] = pop_user_name_text =
		fe_CreateTextField(form1, "popUserNameText", av, ac);

	kids[i++] = incoming_mail_server_text =
		fe_CreateTextField(form1, "incomingMailServerText", av, ac);

	kids[i++] = outgoing_mail_server_text =
		fe_CreateTextField(form1, "outgoingMailServerText", av, ac);

	fep->pop_user_name_text = pop_user_name_text;
	fep->incoming_mail_server_text = incoming_mail_server_text;
	fep->outgoing_mail_server_text = outgoing_mail_server_text;

	// Mail server type

	Widget server_type_label;
	Widget pop3_toggle;
	Widget movemail_toggle;
	Widget imap_toggle;


	kids[i++] = server_type_label = 
		XmCreateLabelGadget(form1, "serverTypeLabel", av, ac);

	kids[i++] = pop3_toggle =
		XmCreateToggleButtonGadget(form1, "pop3Toggle", av, ac);

	kids[i++] = movemail_toggle =
		XmCreateToggleButtonGadget(form1, "moveMailToggle", av, ac);

	kids[i++] = imap_toggle =
		XmCreateToggleButtonGadget(form1, "imapToggle", av, ac);

	fep->pop3_toggle = pop3_toggle;
	fep->movemail_toggle = movemail_toggle;
	fep->imap_toggle = imap_toggle;

	Widget leave_msg_on_server_toggle;
	//	Widget imap_local_copies_toggle;
	Widget imap_server_ssl_toggle;
	Widget delete_is_move_to_trash_toggle;

	kids[i++] = leave_msg_on_server_toggle =
		XmCreateToggleButtonGadget(form1, "leaveMsgToggle", av, ac);

	//	kids[i++] = imap_local_copies_toggle =
	//  XmCreateToggleButtonGadget(form1, "imapLocalCopiesToggle", av, ac);

	kids[i++] = imap_server_ssl_toggle =
		XmCreateToggleButtonGadget(form1, "imapServerSslToggle", av, ac);

	kids[i++] = delete_is_move_to_trash_toggle =
		XmCreateToggleButtonGadget(form1, "imapDeleteToggle", av, ac);

	fep->leave_msg_on_server_toggle = leave_msg_on_server_toggle;
	//	fep->imap_local_copies_toggle = imap_local_copies_toggle;
	fep->imap_server_ssl_toggle = imap_server_ssl_toggle;
	fep->delete_is_move_to_trash_toggle = delete_is_move_to_trash_toggle;

	Widget built_in_app_toggle;
	Widget external_app_toggle;
	Widget external_app_text;
	Widget external_app_browse_button;

	kids[i++] = built_in_app_toggle =
		XmCreateToggleButtonGadget(form1, "builtInAppToggle", av, ac);

	kids[i++] = external_app_toggle =
		XmCreateToggleButtonGadget(form1, "externalAppToggle", av, ac);

	kids[i++] = external_app_text =
		fe_CreateTextField(form1, "externalAppText", av, ac);

	kids[i++] = external_app_browse_button =
		XmCreatePushButtonGadget(form1, "externalAppBrowse", av, ac);

	fep->built_in_app_toggle = built_in_app_toggle;
	fep->external_app_toggle = external_app_toggle;
	fep->external_app_text = external_app_text;
	fep->external_app_browse_button = external_app_browse_button;


	// Attachments

	int labels_width;
	int labels_height;
	int labels_height2;

	labels_width = XfeVaGetWidestWidget(pop_user_name_label,
										incoming_mail_server_label,
										outgoing_mail_server_label,
										NULL);

	labels_height = XfeVaGetTallestWidget(incoming_mail_server_label,
										  incoming_mail_server_text,
										  NULL);

	XtVaSetValues(server_label,
				  XmNalignment, XmALIGNMENT_BEGINNING,
				  XmNtopAttachment, XmATTACH_FORM,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtVaSetValues(pop_user_name_label,
				  XmNheight, labels_height,
				  RIGHT_JUSTIFY_VA_ARGS(pop_user_name_label,labels_width),
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, server_label,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtVaSetValues(pop_user_name_text,
				  XmNheight, labels_height,
				  XmNcolumns, 25,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, pop_user_name_label,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNleftOffset, labels_width,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtVaSetValues(outgoing_mail_server_label,
				  RIGHT_JUSTIFY_VA_ARGS(outgoing_mail_server_label,labels_width),
				  XmNheight, labels_height,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, pop_user_name_label,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtVaSetValues(outgoing_mail_server_text,
				  XmNcolumns, 25,
				  XmNheight, labels_height,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, outgoing_mail_server_label,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNleftOffset, labels_width,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtVaSetValues(incoming_mail_server_label,
				  RIGHT_JUSTIFY_VA_ARGS(incoming_mail_server_label,labels_width),
				  XmNheight, labels_height,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, outgoing_mail_server_label,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtVaSetValues(incoming_mail_server_text,
				  XmNcolumns, 25,
				  XmNheight, labels_height,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, incoming_mail_server_label,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNleftOffset, labels_width,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtVaSetValues(server_type_label,
				  XmNalignment, XmALIGNMENT_BEGINNING,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, incoming_mail_server_text,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtVaSetValues(pop3_toggle,
				  XmNindicatorType, XmONE_OF_MANY,
				  XmNalignment, XmALIGNMENT_BEGINNING,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, server_type_label,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtVaSetValues(leave_msg_on_server_toggle,
				  XmNindicatorType, XmN_OF_MANY,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, pop3_toggle,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNleftOffset, 16,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtVaSetValues(movemail_toggle,
				  XmNalignment, XmALIGNMENT_BEGINNING,
				  XmNindicatorType, XmONE_OF_MANY,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, leave_msg_on_server_toggle,
				  XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNleftWidget, pop3_toggle,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	labels_height2 = XfeVaGetTallestWidget(external_app_toggle,
										   external_app_text,
										   external_app_browse_button,
										   NULL);

	XtVaSetValues(built_in_app_toggle,
				  XmNindicatorType, XmONE_OF_MANY,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, movemail_toggle,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNleftOffset, 16,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtVaSetValues(external_app_toggle,
				  XmNindicatorType, XmONE_OF_MANY,
				  XmNheight, labels_height2,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, built_in_app_toggle,
				  XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNleftWidget, built_in_app_toggle,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtVaSetValues(external_app_text,
				  XmNheight, labels_height2,
				  XmNcolumns, 20,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, external_app_toggle,
				  XmNleftAttachment, XmATTACH_WIDGET,
				  XmNleftWidget, external_app_toggle,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtVaSetValues(external_app_browse_button,
				  XmNheight, labels_height2,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, external_app_toggle,
				  XmNleftAttachment, XmATTACH_WIDGET,
				  XmNleftWidget, external_app_text,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtVaSetValues(imap_toggle,
				  XmNalignment, XmALIGNMENT_BEGINNING,
				  XmNindicatorType, XmONE_OF_MANY,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, external_app_toggle,
				  XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNleftWidget, movemail_toggle,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtVaSetValues(delete_is_move_to_trash_toggle,
				  XmNindicatorType, XmN_OF_MANY,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, imap_toggle,
				  XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNleftWidget, leave_msg_on_server_toggle,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtVaSetValues(imap_server_ssl_toggle,
				  XmNindicatorType, XmN_OF_MANY,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, delete_is_move_to_trash_toggle,
				  XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNleftWidget, leave_msg_on_server_toggle,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	// Add callbacks

	XtAddCallback(pop3_toggle, XmNvalueChangedCallback,
				  cb_toggleServerType, fep);
	XtAddCallback(movemail_toggle, XmNvalueChangedCallback,
				  cb_toggleServerType, fep);
	XtAddCallback(imap_toggle, XmNvalueChangedCallback,
				  cb_toggleServerType, fep);
	
	XtAddCallback(built_in_app_toggle, XmNvalueChangedCallback,
				  cb_toggleApplication, fep);
	XtAddCallback(external_app_toggle, XmNvalueChangedCallback,
				  cb_toggleApplication, fep);

	XtAddCallback(external_app_browse_button, XmNactivateCallback,
				  cb_browseApplication, this);

	XtManageChildren (kids, i);
	XtManageChild(form1);
	XtManageChild(frame1);

	Widget more_button;

	ac = 0;
	i = 0;

	kids[i++] = more_button =
		XmCreatePushButtonGadget(form, "moreButton", av, ac);

	XtVaSetValues(more_button,
				  XmNheight, labels_height,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, frame1,
				  XmNtopOffset, 8,
				  XmNleftAttachment, XmATTACH_NONE,
				  XmNrightAttachment, XmATTACH_FORM,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtAddCallback(more_button, XmNactivateCallback,
				  cb_more, this);

	XtManageChildren(kids, i);

	setCreated(TRUE);
}

// Member:       init
// Description:  Initializes page for MailNewsMserver
// Inputs:
// Side effects: 

void XFE_PrefsPageMailNewsMserver::init()
{
	XP_ASSERT(m_prefsDataMailNewsMserver);

	PrefsDataMailNewsMserver  *fep = m_prefsDataMailNewsMserver;
	XFE_GlobalPrefs           *prefs = &fe_globalPrefs;
	XP_Bool                    b;
    Boolean                    sensitive;
	
	fe_SetTextField(fep->incoming_mail_server_text, prefs->pop3_host);
	fe_SetTextField(fep->outgoing_mail_server_text, prefs->mailhost);
	fe_SetTextField(fep->pop_user_name_text, prefs->pop3_user_id);
	XtVaSetValues(fep->leave_msg_on_server_toggle, XmNset, prefs->pop3_leave_mail_on_server, 0);
	//	XtVaSetValues(fep->imap_local_copies_toggle, XmNset, prefs->imap_local_copies, 0);
	XtVaSetValues(fep->imap_server_ssl_toggle, XmNset, prefs->imap_server_ssl, 0);
	XtVaSetValues(fep->delete_is_move_to_trash_toggle, XmNset, prefs->imap_delete_is_move_to_trash, 0);
	XtVaSetValues (fep->built_in_app_toggle, XmNset, prefs->builtin_movemail_p, 0);
	XtVaSetValues (fep->external_app_toggle, XmNset, !prefs->builtin_movemail_p, 0);

    sensitive = !PREF_PrefIsLocked("mail.server_type");
    XtSetSensitive(fep->movemail_toggle, sensitive);
    XtSetSensitive(fep->pop3_toggle, sensitive);
    XtSetSensitive(fep->imap_toggle, sensitive);

	if (prefs->mail_server_type == MAIL_SERVER_MOVEMAIL) {
		XtVaSetValues(fep->movemail_toggle, XmNset, True, 0);
		XtVaSetValues(fep->pop3_toggle, XmNset, False, 0);
		XtVaSetValues(fep->imap_toggle, XmNset, False, 0);
		XtVaSetValues(fep->pop_user_name_text, XmNsensitive, False, 0);
		XtVaSetValues(fep->incoming_mail_server_text, XmNsensitive, False, 0);
		b = prefs->builtin_movemail_p;
        sensitive = !PREF_PrefIsLocked("mail.use_builtin_movemail");
		XtVaSetValues (fep->built_in_app_toggle, XmNset, b && sensitive, 0);
		XtVaSetValues (fep->external_app_toggle, XmNset, !b && sensitive, 0);
        sensitive = !PREF_PrefIsLocked("mail.movemail_program");
		XtVaSetValues (fep->external_app_text, XmNsensitive, !b && sensitive, 0);
		XtVaSetValues(fep->external_app_browse_button, XmNsensitive, !b && sensitive, 0);
	} 
	else if (prefs->mail_server_type == MAIL_SERVER_POP3) {
		// Pop 3
		XtVaSetValues(fep->pop3_toggle, XmNset, True, 0);
		XtVaSetValues(fep->movemail_toggle, XmNset, False, 0);
		XtVaSetValues(fep->imap_toggle, XmNset, False, 0);
        sensitive = !PREF_PrefIsLocked("network.hosts.pop_server");
		XtVaSetValues(fep->incoming_mail_server_text, XmNsensitive, sensitive, 0);
        sensitive = !PREF_PrefIsLocked("mail.pop_name");
		XtVaSetValues(fep->pop_user_name_text, XmNsensitive, sensitive, 0);
		XtVaSetValues(fep->built_in_app_toggle, XmNsensitive, False, 0);
		XtVaSetValues(fep->external_app_toggle, XmNsensitive, False, 0);
		XtVaSetValues(fep->external_app_text, XmNsensitive, False, 0);
		XtVaSetValues(fep->external_app_browse_button, XmNsensitive, False, 0);
	}
	else if (prefs->mail_server_type == MAIL_SERVER_IMAP) {
		XtVaSetValues(fep->movemail_toggle, XmNset, False, 0);
		XtVaSetValues(fep->pop3_toggle, XmNset, False, 0);
		XtVaSetValues(fep->imap_toggle, XmNset, True, 0);
		XtVaSetValues(fep->built_in_app_toggle, XmNsensitive, False, 0);
		XtVaSetValues(fep->external_app_toggle, XmNsensitive, False, 0);
		XtVaSetValues(fep->external_app_text, XmNsensitive, False, 0);
		XtVaSetValues(fep->external_app_browse_button, XmNsensitive, False, 0);
        sensitive = !PREF_PrefIsLocked("mail.pop_name");
		XtVaSetValues(fep->pop_user_name_text, XmNsensitive, sensitive, 0);
        sensitive = !PREF_PrefIsLocked("network.hosts.pop_server");
		XtVaSetValues(fep->incoming_mail_server_text, XmNsensitive, sensitive, 0);
	} 


    sensitive = ( prefs->mail_server_type == MAIL_SERVER_POP3 &&
                  !PREF_PrefIsLocked("mail.leave_on_server") );
    XtSetSensitive(fep->leave_msg_on_server_toggle, sensitive);

	//    sensitive = ( prefs->mail_server_type == MAIL_SERVER_IMAP &&
	//                  !PREF_PrefIsLocked("mail.imap.local_copies") );
	//    XtSetSensitive(fep->imap_local_copies_toggle, sensitive);

    sensitive = ( prefs->mail_server_type == MAIL_SERVER_IMAP &&
                  !PREF_PrefIsLocked("mail.imap.server_ssl") );
    XtSetSensitive(fep->imap_server_ssl_toggle, sensitive);

    sensitive = ( prefs->mail_server_type == MAIL_SERVER_IMAP &&
                  !PREF_PrefIsLocked("mail.imap.delete_is_move_to_trash") );
    XtSetSensitive(fep->delete_is_move_to_trash_toggle, sensitive);

    XtSetSensitive(fep->outgoing_mail_server_text, 
                   !PREF_PrefIsLocked("network.hosts.smtp_server"));

	fe_SetTextField(fep->external_app_text, prefs->movemail_program);

	setInitialized(TRUE);
}

// Member:       verify
// Description:  
// Inputs:
// Side effects: 

Boolean XFE_PrefsPageMailNewsMserver::verify()
{
	char         buf[10000];
	char        *buf2;
	char        *warning;
	int          size;
	int          orig_mail_server_type = fe_globalPrefs.mail_server_type;
	int          new_mail_server_type=0;  // Initialized to avoid warning - RK
	Boolean      b;

	XP_ASSERT(m_prefsDataMailNewsMserver);
	PrefsDataMailNewsMserver *fep = m_prefsDataMailNewsMserver;

	// Pop up a dialog warning the user that he has to exit
	// in order for mail server prefs to take effect.

	XtVaGetValues(fep->pop3_toggle, XmNset, &b, 0);
	if (b) new_mail_server_type = MAIL_SERVER_POP3;
	
	XtVaGetValues(fep->movemail_toggle, XmNset, &b, 0);
	if (b) new_mail_server_type = MAIL_SERVER_MOVEMAIL;

	XtVaGetValues(fep->imap_toggle, XmNset, &b, 0);
	if (b) new_mail_server_type = MAIL_SERVER_IMAP;

	if (new_mail_server_type != orig_mail_server_type) {
		if (! XFE_Confirm(getContext(), XP_GetString(MK_MSG_MAIL_DIRECTORY_CHANGED))) {
			// revert to old prefs if the user changes his mind
			init();
			return FALSE;
		}
	}

	buf2 = buf;
	strcpy (buf, XP_GetString(XFE_WARNING));
	buf2 = buf + XP_STRLEN(buf);
	warning = buf2;
	size = buf + sizeof (buf) - warning;

	PREFS_CHECK_HOST(fep->outgoing_mail_server_text, XP_GetString(XFE_MAIL_HOST), 
					 warning, size);

	if (*buf2) {
		FE_Alert (getContext(), fe_StringTrim (buf));
		return FALSE;
	}
	else {
		return TRUE;
	}
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
	PrefsDataMailNewsMserver *fep = m_prefsDataMailNewsMserver;
	Boolean                   b;

	XP_ASSERT(fep);

	// Mail server and type

	// POP3

	XtVaGetValues(fep->leave_msg_on_server_toggle, XmNset, &b, 0);
	fe_globalPrefs.pop3_leave_mail_on_server = b;

	XtVaGetValues(fep->pop3_toggle, XmNset, &b, 0);
	if (b) {
		fe_globalPrefs.mail_server_type = MAIL_SERVER_POP3;
	}

	// Movemail
	XtVaGetValues(fep->movemail_toggle, XmNset, &b, 0);
	fe_globalPrefs.use_movemail_p = b;
	if (b) {
		fe_globalPrefs.mail_server_type = MAIL_SERVER_MOVEMAIL;
	}

	// IMAP
	//	XtVaGetValues(fep->imap_local_copies_toggle, XmNset, &b, 0);
	//	fe_globalPrefs.imap_local_copies = b;
	XtVaGetValues(fep->imap_server_ssl_toggle, XmNset, &b, 0);
	fe_globalPrefs.imap_server_ssl = b;

	XtVaGetValues(fep->delete_is_move_to_trash_toggle, XmNset, &b, 0);
	fe_globalPrefs.imap_delete_is_move_to_trash = b;

	XtVaGetValues(fep->imap_toggle, XmNset, &b, 0);
	if (b) {
		fe_globalPrefs.mail_server_type = MAIL_SERVER_IMAP;
	}


	PREFS_SET_GLOBALPREF_TEXT(movemail_program, external_app_text);
	PREFS_SET_GLOBALPREF_TEXT(pop3_host, incoming_mail_server_text);
	PREFS_SET_GLOBALPREF_TEXT(pop3_user_id, pop_user_name_text);
	PREFS_SET_GLOBALPREF_TEXT(mailhost, outgoing_mail_server_text);

	if (fe_globalPrefs.use_movemail_p) {
		XtVaGetValues(fep->built_in_app_toggle, XmNset, &b, 0);
		fe_globalPrefs.builtin_movemail_p = b;
	}

	// Install preferences

	install();
}

// Member:       getData
// Description:  
// Inputs:
// Side effects: 

PrefsDataMailNewsMserver *XFE_PrefsPageMailNewsMserver::getData()
{
	return m_prefsDataMailNewsMserver;
}

// Member:       cb_browseApplication
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageMailNewsMserver::cb_browseApplication(Widget    /* w */,
														XtPointer closure,
														XtPointer /* callData */)
{
	XFE_PrefsPageMailNewsMserver *thePage = (XFE_PrefsPageMailNewsMserver *)closure;
	XFE_PrefsDialog              *theDialog = thePage->getPrefsDialog();
	PrefsDataMailNewsMserver     *fep = thePage->getData();

	fe_browse_file_of_text(theDialog->getContext(), fep->external_app_text, False);
}

// Member:       cb_more
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageMailNewsMserver::cb_more(Widget    /* w */,
										   XtPointer closure,
										   XtPointer /* callData */)
{
	XFE_PrefsPageMailNewsMserver *thePage = (XFE_PrefsPageMailNewsMserver *)closure;
	XFE_PrefsDialog              *theDialog = thePage->getPrefsDialog();
	Widget                        mainw = theDialog->getBaseWidget();

	//	while (!XtIsWMShell(mainw) && (XtParent(mainw)!=0))
	//		mainw = XtParent(mainw);

	// Instantiate a mail server more dialog

	XFE_PrefsMserverMoreDialog *theMoreDialog = 0;

	if ((theMoreDialog =
		 new XFE_PrefsMserverMoreDialog(theDialog, mainw, "prefsMserverMore")) == 0) {
	    fe_perror(thePage->getContext(), XP_GetString(XFE_OUT_OF_MEMORY_URL));
	    return;
	}

	// Initialize and pop up the more dialog

	theMoreDialog->initPage();
	theMoreDialog->show();
}

// Member:       cb_toggleServerType
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageMailNewsMserver::cb_toggleServerType(Widget    w,
													   XtPointer closure,
													   XtPointer callData)
{
	XmToggleButtonCallbackStruct *cb = (XmToggleButtonCallbackStruct *)callData;
	PrefsDataMailNewsMserver     *fep = (PrefsDataMailNewsMserver *)closure;
    Boolean                       sensitive;

	if (! cb->set) {
		XtVaSetValues(w, XmNset, True, 0);
	}
	else if (w == fep->pop3_toggle) {
		XtVaSetValues(fep->movemail_toggle, XmNset, False, 0);
		XtVaSetValues(fep->imap_toggle, XmNset, False, 0);
		XtVaSetValues(fep->built_in_app_toggle, XmNsensitive, False, 0);
		XtVaSetValues(fep->external_app_toggle, XmNsensitive, False, 0);
		XtVaSetValues(fep->external_app_text, XmNsensitive, False, 0);
		XtVaSetValues(fep->external_app_browse_button, XmNsensitive, False, 0);
		XtVaSetValues(fep->pop_user_name_text, 
                      XmNsensitive, !PREF_PrefIsLocked("mail.pop_name"), 0);
		XtVaSetValues(fep->incoming_mail_server_text, 
                      XmNsensitive, !PREF_PrefIsLocked("network.hosts.pop_server"), 0);
		XtVaSetValues(fep->leave_msg_on_server_toggle, 
                      XmNsensitive, !PREF_PrefIsLocked("mail.leave_on_server"),
                      0);

		XtVaSetValues(fep->imap_server_ssl_toggle, XmNsensitive, False, 0);
		XtVaSetValues(fep->delete_is_move_to_trash_toggle, XmNsensitive, False, 0);
	}
	else if (w == fep->movemail_toggle) {
		XtVaSetValues(fep->pop3_toggle, XmNset, False, 0);
		XtVaSetValues(fep->imap_toggle, XmNset, False, 0);
        sensitive = !PREF_PrefIsLocked("mail.use_builtin_movemail");
		XtVaSetValues(fep->built_in_app_toggle, XmNsensitive, sensitive, 0);
		XtVaSetValues(fep->external_app_toggle, XmNsensitive, sensitive, 0);
        sensitive = !PREF_PrefIsLocked("mail.movemail_program");
		XtVaSetValues(fep->external_app_text, XmNsensitive, sensitive, 0);
		XtVaSetValues(fep->external_app_browse_button, XmNsensitive, sensitive, 0);
		XtVaSetValues(fep->pop_user_name_text, XmNsensitive, False, 0);
		XtVaSetValues(fep->incoming_mail_server_text, XmNsensitive, False, 0);
		XtVaSetValues(fep->leave_msg_on_server_toggle, XmNsensitive, False, 0);
		//		XtVaSetValues(fep->imap_local_copies_toggle, XmNsensitive, False, 0);
		XtVaSetValues(fep->imap_server_ssl_toggle, XmNsensitive, False, 0);
		XtVaSetValues(fep->delete_is_move_to_trash_toggle, XmNsensitive, False, 0);
	}
	else if (w == fep->imap_toggle) {
		XtVaSetValues(fep->movemail_toggle, XmNset, False, 0);
		XtVaSetValues(fep->pop3_toggle, XmNset, False, 0);
		XtVaSetValues(fep->built_in_app_toggle, XmNsensitive, False, 0);
		XtVaSetValues(fep->external_app_toggle, XmNsensitive, False, 0);
		XtVaSetValues(fep->external_app_text, XmNsensitive, False, 0);
		XtVaSetValues(fep->external_app_browse_button, XmNsensitive, False, 0);
        sensitive = !PREF_PrefIsLocked("mail.pop_name");
		XtVaSetValues(fep->pop_user_name_text, XmNsensitive, sensitive, 0);
        sensitive = !PREF_PrefIsLocked("network.hosts.pop_server");
		XtVaSetValues(fep->incoming_mail_server_text, XmNsensitive, sensitive, 0);
		XtVaSetValues(fep->leave_msg_on_server_toggle, XmNsensitive, False, 0);
		//		XtVaSetValues(fep->imap_local_copies_toggle, 
		//                      XmNsensitive, !PREF_PrefIsLocked("mail.imap.local_copies"), 0);
		XtVaSetValues(fep->imap_server_ssl_toggle, 
                      XmNsensitive, !PREF_PrefIsLocked("mail.imap.server_ssl"), 0);
		XtVaSetValues(fep->delete_is_move_to_trash_toggle, 
                      XmNsensitive, !PREF_PrefIsLocked("mail.imap.delete_is_move_to_trash"), 0);
	}
	else
		abort();
}

// Member:       cb_toggleApplication
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageMailNewsMserver::cb_toggleApplication(Widget    w,
														XtPointer closure,
														XtPointer callData)
{
	XmToggleButtonCallbackStruct *cb = (XmToggleButtonCallbackStruct *)callData;
	PrefsDataMailNewsMserver     *fep = (PrefsDataMailNewsMserver *)closure;

	if (! cb->set) {
		XtVaSetValues(w, XmNset, True, 0);
	}
	else if (w == fep->built_in_app_toggle) {
		XtVaSetValues(fep->external_app_toggle, XmNset, False, 0);
		XtVaSetValues(fep->external_app_text, XmNsensitive, False, 0);
		XtVaSetValues(fep->external_app_browse_button, XmNsensitive, False, 0);
	}
	else if (w == fep->external_app_toggle) {
		XtVaSetValues(fep->built_in_app_toggle, XmNset, False, 0);
		XtVaSetValues(fep->external_app_text, XmNsensitive, True, 0);
		XtVaSetValues(fep->external_app_browse_button, XmNsensitive, True, 0);
	}
	else
		abort();	
}

// ************************************************************************
// ************************ Mail News/News Server *************************
// ************************************************************************

// Member:       XFE_PrefsPageMailNewsNserver
// Description:  Constructor
// Inputs:
// Side effects: 

XFE_PrefsPageMailNewsNserver::XFE_PrefsPageMailNewsNserver(XFE_PrefsDialog *dialog)
	: XFE_PrefsPage(dialog),
	  m_prefsDataMailNewsNserver(0)
{
}

// Member:       ~XFE_PrefsPageMailNewsNserver
// Description:  Destructor
// Inputs:
// Side effects: 

XFE_PrefsPageMailNewsNserver::~XFE_PrefsPageMailNewsNserver()
{
	delete m_prefsDataMailNewsNserver;
}

// Member:       create
// Description:  Creates page for MailNews/News server
// Inputs:
// Side effects: 

void XFE_PrefsPageMailNewsNserver::create()
{
	Widget            kids[100];
	Arg               av[50];
	int               ac;
	int               i;

	PrefsDataMailNewsNserver *fep = NULL;

	fep = new PrefsDataMailNewsNserver;
	memset(fep, 0, sizeof(PrefsDataMailNewsNserver));
	m_prefsDataMailNewsNserver = fep;

	fep->context = getContext();
	fep->prompt_dialog = getPrefsDialog()->getDialogChrome();

	Widget form;

	ac = 0;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	form = XmCreateForm (m_wPageForm, "mailnewsNserver", av, ac);
	XtManageChild (form);
	m_wPage = fep->page = form;

	Widget frame1;

	ac = 0;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_NONE); ac++;
	frame1 = XmCreateFrame (form, "nServerFrame", av, ac);

	Widget form1;

	ac = 0;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_NONE); ac++;
	form1 = XmCreateForm (frame1, "nServerBox", av, ac);

	Widget news_server_label;
	Widget nntp_server_label;
	Widget port_label;

	ac = 0;
	i = 0;

	kids[i++] = news_server_label = 
		XmCreateLabelGadget(form1, "newsServerLabel", av, ac);

	kids[i++] = port_label = 
		XmCreateLabelGadget(form1, "portLabel", av, ac);

	kids[i++] = nntp_server_label = 
		XmCreateLabelGadget(form1, "nntpServerLabel", av, ac);

	kids[i++] = fep->nntp_server_text =
		fe_CreateTextField(form1, "nntpServerText", av, ac);

	kids[i++] = fep->port_text =
		fe_CreateTextField(form1, "serverPortText", av, ac);

	kids[i++] = fep->secure_toggle =
		XmCreateToggleButtonGadget(form1, "secure", av, ac);

	XtVaSetValues(news_server_label,
				  XmNalignment, XmALIGNMENT_BEGINNING,
				  XmNtopAttachment, XmATTACH_FORM,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtVaSetValues(nntp_server_label,
				  XmNalignment, XmALIGNMENT_BEGINNING,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, news_server_label,
				  XmNtopOffset, 4,
				  XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNleftWidget, news_server_label,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtVaSetValues(fep->nntp_server_text,
				  XmNcolumns, 35,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, nntp_server_label,
				  XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNleftWidget, news_server_label,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	int labels_height;
	labels_height = XfeVaGetTallestWidget(port_label,
										  fep->port_text,
										  fep->secure_toggle,
										  NULL);

	XtVaSetValues(port_label,
				  XmNheight, labels_height,
				  XmNalignment, XmALIGNMENT_BEGINNING,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, fep->nntp_server_text,
				  XmNtopOffset, 4,
				  XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNleftWidget, news_server_label,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtVaSetValues(fep->port_text,
				  XmNheight, labels_height,
				  XmNcolumns, 5,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, port_label,
				  XmNleftAttachment, XmATTACH_WIDGET,
				  XmNleftWidget, port_label,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);
	
	XtVaSetValues(fep->secure_toggle,
				  XmNheight, labels_height,
				  XmNindicatorType, XmN_OF_MANY,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, port_label,
				  XmNleftAttachment, XmATTACH_WIDGET,
				  XmNleftWidget, fep->port_text,
				  XmNleftOffset, 24,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);


	XtManageChildren (kids, i);
	XtManageChild(form1);
	XtManageChild(frame1);

	Widget frame2;

	ac = 0;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
	XtSetArg (av [ac], XmNtopWidget, frame1); ac++;
	XtSetArg (av [ac], XmNtopOffset, 8); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_NONE); ac++;
	frame2 = XmCreateFrame(form, "newsDirFrame", av, ac);

	Widget form2;

	ac = 0;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
	form2 = XmCreateForm (frame2, "newsDirBox", av, ac);

	Widget news_dir_label;
	Widget browse_button;
	Widget msg_label;

	ac = 0;
	i = 0;

	kids[i++] = news_dir_label = 
		XmCreateLabelGadget(form2, "newsDirLabel", av, ac);

	kids[i++] = msg_label = 
		XmCreateLabelGadget(form2, "msgLabel", av, ac);

	kids[i++] = fep->news_dir_text =
		fe_CreateTextField(form2, "newsDirText", av, ac);

	kids[i++] = fep->msg_size_text =
		fe_CreateTextField(form2, "msgSizeText", av, ac);

	kids[i++] = fep->notify_toggle =
		XmCreateToggleButtonGadget(form2, "notify", av, ac);

	kids[i++] = fep->browse_button = browse_button =
		XmCreatePushButtonGadget(form2, "browse", av, ac);

	XtVaSetValues(news_dir_label,
				  XmNalignment, XmALIGNMENT_BEGINNING,
				  XmNtopAttachment, XmATTACH_FORM,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtVaSetValues(fep->news_dir_text,
				  XmNcolumns, 35,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, news_dir_label,
				  XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNleftWidget, news_dir_label,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtVaSetValues(browse_button,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, fep->news_dir_text,
				  XmNleftAttachment, XmATTACH_WIDGET,
				  XmNleftWidget, fep->news_dir_text,
				  XmNleftOffset, 16,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNbottomWidget, fep->news_dir_text,
				  NULL);

	labels_height = XfeVaGetTallestWidget(fep->notify_toggle,
										  fep->msg_size_text,
										  msg_label,
										  NULL);

	XtVaSetValues(fep->notify_toggle,
				  XmNtopOffset, 4,
				  XmNheight, labels_height,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, fep->news_dir_text,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtVaSetValues(fep->msg_size_text,
				  XmNcolumns, 6,
				  XmNheight, labels_height,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, fep->notify_toggle,
				  XmNleftAttachment, XmATTACH_WIDGET,
				  XmNleftWidget, fep->notify_toggle,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNbottomWidget, fep->notify_toggle,
				  NULL);

	XtVaSetValues(msg_label,
				  XmNheight, labels_height,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, fep->notify_toggle,
				  XmNleftAttachment, XmATTACH_WIDGET,
				  XmNleftWidget, fep->msg_size_text,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNbottomWidget, fep->msg_size_text,
				  NULL);

	XtAddCallback(fep->secure_toggle, XmNvalueChangedCallback, cb_toggleSecure, fep);
	XtAddCallback(browse_button, XmNactivateCallback, cb_browse, this);

	XtManageChildren(kids, i);
	XtManageChild(form2);
	XtManageChild(frame2);

	setCreated(TRUE);
}

// Member:       init
// Description:  Initializes page for MailNewsNserver
// Inputs:
// Side effects: 

void XFE_PrefsPageMailNewsNserver::init()
{
	XP_ASSERT(m_prefsDataMailNewsNserver);

	PrefsDataMailNewsNserver  *fep = m_prefsDataMailNewsNserver;
	XFE_GlobalPrefs           *prefs = &fe_globalPrefs;
    Boolean                    sensitive;
	char                       buf[256];

	// News server

	XtVaSetValues(fep->nntp_server_text, 
                  XmNsensitive, !PREF_PrefIsLocked("network.hosts.nntp_server"),
                  0);
	fe_SetTextField(fep->nntp_server_text, prefs->newshost);

	PR_snprintf(buf, sizeof(buf), "%d", prefs->news_server_port);
	XtVaSetValues(fep->port_text,
                  XmNsensitive, !PREF_PrefIsLocked("news.server_port"),
				  0);
	fe_SetTextField(fep->port_text, buf);

	XtVaSetValues(fep->secure_toggle,
				  XmNset, prefs->news_server_secure,
                  XmNsensitive, !PREF_PrefIsLocked("news.server_is_secure"),
				  0);

	// News folder and download

	fe_SetTextField(fep->news_dir_text, prefs->newsrc_directory);
    sensitive = !PREF_PrefIsLocked("news.directory");
    XtSetSensitive(fep->news_dir_text, sensitive);
    XtSetSensitive(fep->browse_button, sensitive);

	XtVaSetValues(fep->notify_toggle, XmNset, prefs->news_notify_on, 0);
	PR_snprintf(buf, sizeof(buf), "%d", prefs->news_max_articles);
	XtVaSetValues(fep->msg_size_text, XmNvalue, buf, 0);

    XtSetSensitive(fep->notify_toggle, !PREF_PrefIsLocked("news.notify.on"));
    XtSetSensitive(fep->msg_size_text, !PREF_PrefIsLocked("news.notify.size"));

	setInitialized(TRUE);
}

// Member:       verify
// Description:  
// Inputs:
// Side effects: 

Boolean XFE_PrefsPageMailNewsNserver::verify()
{
	char         buf[10000];
	char        *buf2;
	char        *warning;
	struct stat  st;
	int          size;

	buf2 = buf;
	strcpy (buf, XP_GetString(XFE_WARNING));
	buf2 = buf + XP_STRLEN(buf);
	warning = buf2;
	size = buf + sizeof (buf) - warning;

	XP_ASSERT(m_prefsDataMailNewsNserver);
	PrefsDataMailNewsNserver  *fep = m_prefsDataMailNewsNserver;

	PREFS_CHECK_HOST (fep->nntp_server_text,
					  XP_GetString(XFE_NEWS_HOST), warning, size);
	PREFS_CHECK_DIR  (fep->news_dir_text,
					  XP_GetString(XFE_NEWS_DIR),
					  warning, size);

	if (*buf2) {
		FE_Alert(getContext(), fe_StringTrim (buf));
		return FALSE;
	}
	else {
		return TRUE;
	}
}

// Member:       install
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageMailNewsNserver::install()
{
	fe_installMailNewsNserver();
}

// Member:       save
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageMailNewsNserver::save()
{
	PrefsDataMailNewsNserver *fep = m_prefsDataMailNewsNserver;

	XP_ASSERT(fep);

	PREFS_SET_GLOBALPREF_TEXT(newshost, nntp_server_text);

	XP_FREEIF(fe_globalPrefs.newsrc_directory);
	char *s = fe_GetTextField(fep->news_dir_text);
    fe_globalPrefs.newsrc_directory = s ? s : XP_STRDUP("");

	Boolean b;

	XtVaGetValues(fep->notify_toggle, XmNset, &b, 0);
	fe_globalPrefs.news_notify_on = b;

	XtVaGetValues(fep->secure_toggle, XmNset, &b, 0);
	fe_globalPrefs.news_server_secure = b;

    char *text;
	char  dummy;
    int   size = 0;
    int   port = 0;

    XtVaGetValues(fep->msg_size_text, XmNvalue, &text, 0);
    if (1 == sscanf(text, " %d %c", &size, &dummy) &&
		size >= 0)
		fe_globalPrefs.news_max_articles = size;
	if (text) XtFree(text);

    XtVaGetValues(fep->port_text, XmNvalue, &text, 0);
    if (1 == sscanf(text, " %d %c", &port, &dummy) &&
		port >= 0)
		fe_globalPrefs.news_server_port = port;
	if (text) XtFree(text);

	// Install preferences

	install();
}

// Member:       getData
// Description:  
// Inputs:
// Side effects: 

PrefsDataMailNewsNserver *XFE_PrefsPageMailNewsNserver::getData()
{
	return m_prefsDataMailNewsNserver;
}

// Member:       cb_browse
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageMailNewsNserver::cb_browse(Widget    /* w */,
											 XtPointer closure,
											 XtPointer /* callData */)
{
	XFE_PrefsPageMailNewsNserver *thePage = (XFE_PrefsPageMailNewsNserver *)closure;
	XFE_PrefsDialog              *theDialog = thePage->getPrefsDialog();
	PrefsDataMailNewsNserver     *fep = thePage->getData();

	fe_browse_file_of_text(theDialog->getContext(), fep->news_dir_text, True);
}

// Member:       cb_toggleSecure
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageMailNewsNserver::cb_toggleSecure(Widget    /*w*/,
												   XtPointer closure,
												   XtPointer callData)
{
	XmToggleButtonCallbackStruct *cb = (XmToggleButtonCallbackStruct *)callData;
	PrefsDataMailNewsNserver     *fep = (PrefsDataMailNewsNserver *)closure;
	char                         *port_text = 0;
	char                         *trimmed_port_text;
	int                           port_number;
	char                          buf[256];

	port_text = fe_GetTextField(fep->port_text);
	if (port_text) {
		trimmed_port_text = fe_StringTrim(port_text);
		if (XP_STRLEN(trimmed_port_text) == 0) {
			port_number = (cb->set) ? SECURE_NEWS_PORT : NEWS_PORT;
		}
		else {
			port_number = atoi(trimmed_port_text);
			if (cb->set && port_number == NEWS_PORT)
				port_number = SECURE_NEWS_PORT;
			else if (! cb->set && port_number == SECURE_NEWS_PORT)
				port_number = NEWS_PORT;
		}
		XtFree(port_text);
	}
	else {
		port_number = (cb->set) ? SECURE_NEWS_PORT : NEWS_PORT;
	}

	PR_snprintf(buf, sizeof(buf), "%d", port_number);
	fe_SetTextField(fep->port_text, buf);
}

// ************************************************************************
// *********************** Mail News/Address Book *************************
// ************************************************************************

const int XFE_PrefsPageMailNewsAddrBook::OUTLINER_COLUMN_NAME = 0;
const int XFE_PrefsPageMailNewsAddrBook::OUTLINER_COLUMN_MAX_LENGTH = 256;
const int XFE_PrefsPageMailNewsAddrBook::OUTLINER_INIT_POS = (-1);
#define STRING_COL_NAME    "Name"

// Member:       XFE_PrefsPageMailNewsAddrBook
// Description:  Constructor
// Inputs:
// Side effects: 

XFE_PrefsPageMailNewsAddrBook::XFE_PrefsPageMailNewsAddrBook(XFE_PrefsDialog *dialog)
	: XFE_PrefsPage(dialog),
	  m_prefsDataMailNewsAddrBook(0),
	  m_rowIndex(0)
{
}

// Member:       ~XFE_PrefsPageMailNewsAddrBook
// Description:  Destructor
// Inputs:
// Side effects: 

XFE_PrefsPageMailNewsAddrBook::~XFE_PrefsPageMailNewsAddrBook()
{
	PrefsDataMailNewsAddrBook   *fep = m_prefsDataMailNewsAddrBook;

	if (fep) {
		delete fep->dir_outliner;
		if (fep->directories) 
			XP_ListDestroy(fep->directories);
		// Note: do not destroy deleted_directories. It's taken care of by 
		// DIR_DeleteServerList, which is called by DIR_CleanUpServerPreferences
	}
	delete m_prefsDataMailNewsAddrBook;
}

// Member:       create
// Description:  Creates page for MailNews/Address Book
// Inputs:
// Side effects: 

void XFE_PrefsPageMailNewsAddrBook::create()
{
	Widget            kids[100];
	Arg               av[50];
	int               ac;
	int               i;

	PrefsDataMailNewsAddrBook *fep = NULL;

	fep = new PrefsDataMailNewsAddrBook;
	memset(fep, 0, sizeof(PrefsDataMailNewsAddrBook));
	m_prefsDataMailNewsAddrBook = fep;

	fep->context = getContext();
	fep->prompt_dialog = getPrefsDialog()->getDialogChrome();

	Widget form;

	ac = 0;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	form = XmCreateForm (m_wPageForm, "mailnewsAddrBook", av, ac);
	XtManageChild (form);
	m_wPage = fep->page = form;

	Widget frame1;

	ac = 0;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_NONE); ac++;
	frame1 = XmCreateFrame (form, "addrBookFrame", av, ac);

	Widget form1;

	ac = 0;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
	form1 = XmCreateForm (frame1, "addrBookBox", av, ac);

	Widget addr_book_label;
	Widget dir_list;
	Widget up_button;
	Widget down_button;
	Widget add_button;
	Widget edit_button;
	Widget delete_button;

	ac = 0;
	i = 0;

	kids[i++] = addr_book_label = 
		XmCreateLabelGadget(form1, "addrBookLabel", av, ac);

	kids[i++] = up_button = 
		XmCreateArrowButtonGadget(form1, "upButton", av, ac);

	kids[i++] = down_button = 
		XmCreateArrowButtonGadget(form1, "downButton", av, ac);

	kids[i++] = add_button =
		XmCreatePushButtonGadget(form1, "newButton", av, ac);

	kids[i++] = edit_button =
		XmCreatePushButtonGadget(form1, "editButton", av, ac);

	kids[i++] = delete_button =
		XmCreatePushButtonGadget(form1, "deleteButton", av, ac);

	// Outliner

	int           num_columns = 1;
	static int    default_column_widths[] = {40};
	XFE_Outliner *outliner;

	outliner = new XFE_Outliner("dirList",            // name
								this,                  // outlinable
								getPrefsDialog(),      // top level								  
								form1,                 // parent
								FALSE,                 // constant size
								FALSE,                 // has headings
								num_columns,           // number of columns
								num_columns,           // number of visible columns
								default_column_widths, // default column widths
								OUTLINER_GEOMETRY_PREF
								);

	dir_list = fep->dir_list = outliner->getBaseWidget();

	XtVaSetValues(outliner->getBaseWidget(),
				  XtVaTypedArg, XmNblankBackground, XmRString, "white", 6,
				  XmNvisibleRows, 10,
				  NULL);
	XtVaSetValues(outliner->getBaseWidget(),
				  XmNcellDefaults, True,
				  XtVaTypedArg, XmNcellBackground, XmRString, "white", 6,
				  NULL);

	outliner->setColumnResizable(OUTLINER_COLUMN_NAME, False);
	outliner->show();
	
	fep->add_button = add_button;
	fep->edit_button = edit_button;
	fep->delete_button = delete_button;
	fep->up_button = up_button;
	fep->down_button = down_button;
	fep->dir_outliner = outliner;

	XtVaSetValues(addr_book_label,
				  XmNalignment, XmALIGNMENT_BEGINNING,
				  XmNtopAttachment, XmATTACH_FORM,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	Pixel parent_bg;
	XtVaGetValues(getPrefsDialog()->getDialogChrome(), XmNbackground, &parent_bg, 0);

	XtVaSetValues(dir_list,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, addr_book_label,
				  XmNtopOffset, 4,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);
	
	Dimension list_height;
	Dimension up_button_height;
	int down_button_top_offset;
	int up_button_top_offset;

	XtVaGetValues(down_button,
				  XmNtopOffset, &down_button_top_offset,
				  NULL);

	list_height = XfeHeight(dir_list);
	up_button_height = XfeHeight(up_button);
	up_button_top_offset = list_height/2 - up_button_height - down_button_top_offset/2;

	XtVaSetValues(up_button,
				  XmNarrowDirection, XmARROW_UP,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, dir_list,
				  XmNtopOffset, up_button_top_offset,
				  XmNleftAttachment, XmATTACH_WIDGET,
				  XmNleftWidget, dir_list,
				  XmNleftOffset, 8,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);
				  
	XtVaSetValues(down_button,
				  XmNarrowDirection, XmARROW_DOWN,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, up_button,
				  XmNtopOffset, 4,
				  XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNleftWidget, up_button,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	int labels_width;
	labels_width = XfeVaGetWidestWidget(add_button,
										edit_button,
										delete_button,
										NULL);

	XtVaSetValues(add_button,
				  XmNwidth, labels_width,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, dir_list,
				  XmNleftAttachment, XmATTACH_WIDGET,
				  XmNleftWidget, up_button,
				  XmNleftOffset, 24,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtVaSetValues(edit_button,
				  XmNwidth, labels_width,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, add_button,
				  XmNtopOffset, 4,
				  XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNleftWidget, add_button,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtVaSetValues(delete_button,
				  XmNwidth, labels_width,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, edit_button,
				  XmNtopOffset, 4,
				  XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNleftWidget, add_button,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtManageChildren (kids, i);
	XtManageChild(form1);
	XtManageChild(frame1);

	// Show full names as

	Widget frame2;

	ac = 0;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
	XtSetArg (av [ac], XmNtopWidget, frame1); ac++;
	XtSetArg (av [ac], XmNtopOffset, 8); ac++;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_NONE); ac++;
	frame2 = XmCreateFrame (form, "frame2", av, ac);

	Widget form2;

	ac = 0;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
	form2 = XmCreateForm (frame2, "toolbarBox", av, ac);

	Widget label2;

	ac = 0;
	XtSetArg (av [ac], XmNchildType, XmFRAME_TITLE_CHILD); ac++;
	label2 = XmCreateLabelGadget (frame2, "fullNameLabel", av, ac);

	Widget first_last_toggle;
	Widget last_first_toggle;
	Widget first_last_label;
	Widget last_first_label;

	ac = 0;
	i = 0;

	kids[i++] = first_last_toggle = 
		XmCreateToggleButtonGadget(form2, "firstLastToggle", av, ac);

	kids[i++] = last_first_toggle = 
		XmCreateToggleButtonGadget(form2, "lastFirstToggle", av, ac);

	kids[i++] = first_last_label = 
		XmCreateLabelGadget(form2, "firstLastLabel", av, ac);

	kids[i++] = last_first_label = 
		XmCreateLabelGadget(form2, "lastFirstLabel", av, ac);

	fep->first_last_toggle = first_last_toggle;
	fep->last_first_toggle = last_first_toggle;

	int labels_width2;

	labels_width2 = XfeVaGetWidestWidget(first_last_toggle,
										 last_first_toggle,
										 NULL);

	int labels_height;

	labels_height = XfeVaGetTallestWidget(first_last_toggle,
										  first_last_label,
										  NULL);

	XtVaSetValues(first_last_toggle,
				  XmNalignment, XmALIGNMENT_BEGINNING,
				  XmNindicatorType, XmONE_OF_MANY,
				  XmNwidth, labels_width2,
				  XmNheight, labels_height,
				  XmNtopAttachment, XmATTACH_FORM,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNleftOffset, 16,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtVaSetValues(first_last_label,
				  XmNheight, labels_height,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, first_last_toggle,
				  XmNleftAttachment, XmATTACH_WIDGET,
				  XmNleftWidget, first_last_toggle,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtVaSetValues(last_first_toggle,
				  XmNalignment, XmALIGNMENT_BEGINNING,
				  XmNindicatorType, XmONE_OF_MANY,
				  XmNwidth, labels_width2,
				  XmNheight, labels_height,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, first_last_toggle,
				  XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNleftWidget, first_last_toggle,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtVaSetValues(last_first_label,
				  XmNheight, labels_height,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, last_first_toggle,
				  XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNleftWidget, first_last_label,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtManageChildren (kids, i);
	XtManageChild(label2);
	XtManageChild(form2);
	XtManageChild(frame2);

	// Add callbacks

	XtAddCallback(up_button, XmNactivateCallback, cb_promote, this);
	XtAddCallback(down_button, XmNactivateCallback, cb_demote, this);
	XtAddCallback(add_button, XmNactivateCallback, cb_add, this);
	XtAddCallback(edit_button, XmNactivateCallback, cb_edit, this);
	XtAddCallback(delete_button, XmNactivateCallback, cb_delete, this);

	XtAddCallback(first_last_toggle, XmNvalueChangedCallback, cb_toggleNameOrder, fep);
	XtAddCallback(last_first_toggle, XmNvalueChangedCallback, cb_toggleNameOrder, fep);

	setCreated(TRUE);
}

// Member:       init
// Description:  Initializes page for MailNewsAddrBook
// Inputs:
// Side effects: 

void XFE_PrefsPageMailNewsAddrBook::init()
{
	XP_ASSERT(m_prefsDataMailNewsAddrBook);

	PrefsDataMailNewsAddrBook     *fep = m_prefsDataMailNewsAddrBook;
	XFE_GlobalPrefs               *prefs = &fe_globalPrefs;
    Boolean                        sensitive;

	if (! fep->deleted_directories) fep->deleted_directories = XP_ListNew();
	fep->directories = FE_GetDirServers();
	fep->num_directories = XP_ListCount(fep->directories);

	fep->dir_outliner->change(0, fep->num_directories, fep->num_directories);
	setSelectionPos(OUTLINER_INIT_POS);

    sensitive = !PREF_PrefIsLocked("mail.addr_book.lastnamefirst");
	XtVaSetValues(fep->first_last_toggle, 
                  XmNset, !prefs->addr_book_lastname_first, 
                  XmNsensitive, sensitive,
                  0);
	XtVaSetValues(fep->last_first_toggle, 
                  XmNset, prefs->addr_book_lastname_first, 
                  XmNsensitive, sensitive,
                  0);

	setInitialized(TRUE);
}

// Member:       install
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageMailNewsAddrBook::install()
{
	XP_ASSERT(m_prefsDataMailNewsAddrBook);
	PrefsDataMailNewsAddrBook     *fep = m_prefsDataMailNewsAddrBook;

	// directory listing

	DIR_SaveServerPreferences(fep->directories);
	DIR_CleanUpServerPreferences(fep->deleted_directories);
	fe_installMailNewsAddrBook();
}

// Member:       save
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageMailNewsAddrBook::save()
{
	PrefsDataMailNewsAddrBook *fep = m_prefsDataMailNewsAddrBook;
	Boolean                    b;

	// XP_ASSERT(fep);

	XtVaGetValues(fep->last_first_toggle, XmNset, &b, 0);
	fe_globalPrefs.addr_book_lastname_first = b;

	// Install preferences

	install();
}

// Member:       insertDir
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageMailNewsAddrBook::insertDir(DIR_Server *dir)
{
	// First figure out where to insert it

	PrefsDataMailNewsAddrBook *fep = m_prefsDataMailNewsAddrBook;
	int                        pos = 0;
	int                        count = fep->num_directories;
	
	if (count > 0) {
		// Check if there is any selection
		uint32     sel_count = 0;
		const int *indices = 0;
		fep->dir_outliner->getSelection(&indices, (int *) &sel_count);
		if (sel_count > 0 && indices) {
			pos = indices[0];
		}
	}

	// Insert at pos

	insertDirAtPos(pos, dir);
}

// Member:       insertDirAtPos
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageMailNewsAddrBook::insertDirAtPos(int pos, DIR_Server *dir)
{
	PrefsDataMailNewsAddrBook *fep = m_prefsDataMailNewsAddrBook;
	int                        count = fep->num_directories;
	DIR_Server                *prev_dir;
	
	// Insert dir at position

	if (count > 0) {
		prev_dir = (DIR_Server*)XP_ListGetObjectNum(fep->directories,pos+1);
		XP_ListInsertObjectAfter(fep->directories, prev_dir, dir);
	}
	else {
		XP_ListAddObjectToEnd(fep->directories, dir);
	}
	fep->num_directories = XP_ListCount(fep->directories);

	// Repaint 

	fep->dir_outliner->change(0, fep->num_directories, fep->num_directories);

	// Set selection

	setSelectionPos(pos);
}

// Member:       deleteDirAtPos
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageMailNewsAddrBook::deleteDirAtPos(int pos)
{
	PrefsDataMailNewsAddrBook *fep = m_prefsDataMailNewsAddrBook;
	int                        count = fep->num_directories;

	if (pos >= count) return;

	// Remove the directory at pos

	if (pos >=  fep->num_directories) return;
	DIR_Server *dir = (DIR_Server*)XP_ListGetObjectNum(fep->directories,pos+1);
	
	DIR_Server *copy;
	DIR_CopyServer(dir, &copy);
	XP_ListAddObjectToEnd(fep->deleted_directories, copy);

	if (! XP_ListRemoveObject(fep->directories, dir)) return;
	fep->num_directories = XP_ListCount(fep->directories);

	// Repaint

	fep->dir_outliner->change(0, fep->num_directories, fep->num_directories);

	// Set selection if there is more than one entry 

	if (fep->num_directories > 0) {
		if (pos >= fep->num_directories) pos--;
		setSelectionPos(pos);
	}
	else {
		setSelectionPos(OUTLINER_INIT_POS);
	}
}

// Member:       swapDirs
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageMailNewsAddrBook::swapDirs(int pos1, int pos2, int sel_pos)
{
	PrefsDataMailNewsAddrBook *fep = m_prefsDataMailNewsAddrBook;
	int                        count = fep->num_directories;
	DIR_Server                *dir1;
	DIR_Server                *dir2;
	XP_List                   *ptr1;
	XP_List                   *ptr2;

	if ((pos1 >= count) ||
		(pos2 >= count) ||
		(sel_pos >= count))
		return;

	// Swap
	
	dir1 = (DIR_Server*)XP_ListGetObjectNum(fep->directories,pos1+1);
	ptr1 = XP_ListFindObject(fep->directories, dir1);

	dir2 = (DIR_Server*)XP_ListGetObjectNum(fep->directories,pos2+1);
	ptr2 = XP_ListFindObject(fep->directories, dir2);

	ptr1->object = (void *)dir2;
	ptr2->object = (void *)dir1;

	// Repaint

	fep->dir_outliner->change(0, fep->num_directories, fep->num_directories);

	// Set selection if there is more than one entry 

	setSelectionPos(sel_pos);
}

// Member:       setSelelctionPos
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageMailNewsAddrBook::setSelectionPos(int pos)
{
	PrefsDataMailNewsAddrBook *fep = m_prefsDataMailNewsAddrBook;
	int                        count = fep->num_directories;

	if (pos >= count) return;

	if (pos == OUTLINER_INIT_POS) {
		XtVaSetValues(fep->delete_button, XmNsensitive, False, NULL);
		XtVaSetValues(fep->up_button, XmNsensitive, False, NULL);
		XtVaSetValues(fep->down_button, XmNsensitive, False, NULL);
		XtVaSetValues(fep->edit_button, XmNsensitive, False, NULL);
	}
	else {
		// Grey out the Delete/Edit button if it is Personal Address Book
		DIR_Server *dir = (DIR_Server*)XP_ListGetObjectNum(fep->directories,pos+1);
		fep->dir_outliner->selectItemExclusive(pos);
		XtVaSetValues(fep->delete_button, XmNsensitive,
					  (count != 0 && dir->dirType != PABDirectory), NULL);
		XtVaSetValues(fep->edit_button, XmNsensitive,
					  (count != 0 && dir->dirType != PABDirectory), NULL);
		XtVaSetValues(fep->up_button, XmNsensitive, (pos != 0), NULL);
		XtVaSetValues(fep->down_button, XmNsensitive, (pos != (count-1)), NULL);
	}
}

// Member:       deselelctionPos
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageMailNewsAddrBook::deselectPos(int pos)
{
	PrefsDataMailNewsAddrBook *fep = m_prefsDataMailNewsAddrBook;
	int                        count = fep->num_directories;

	if (pos >= count) return;

	fep->dir_outliner->deselectItem(pos);
}

// Member:       getData
// Description:  
// Inputs:
// Side effects: 

PrefsDataMailNewsAddrBook *XFE_PrefsPageMailNewsAddrBook::getData()
{
	return m_prefsDataMailNewsAddrBook;
}

// Member:       cb_promote
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageMailNewsAddrBook::cb_promote(Widget    /* w */,
											   XtPointer closure,
											   XtPointer /* callData */)
{
	XFE_PrefsPageMailNewsAddrBook *thePage = (XFE_PrefsPageMailNewsAddrBook *)closure;
	PrefsDataMailNewsAddrBook     *fep = thePage->getData();
	int                            count = fep->num_directories;
	int                            pos = 0;
	uint32                         sel_count = 0;
	const int                     *indices = 0;

	if (count == 0) return;

	fep->dir_outliner->getSelection(&indices, (int *) &sel_count);
	if (sel_count > 0 && indices) {
		pos = indices[0];
		if (pos != 0) {
			thePage->deselectPos(pos);
			thePage->swapDirs(pos, pos-1, pos-1);
		}
	}
}

// Member:       cb_demote
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageMailNewsAddrBook::cb_demote(Widget    /* w */,
											  XtPointer closure,
											  XtPointer /* callData */)
{
	XFE_PrefsPageMailNewsAddrBook *thePage = (XFE_PrefsPageMailNewsAddrBook *)closure;
	PrefsDataMailNewsAddrBook     *fep = thePage->getData();
	int                            count = fep->num_directories;
	int                            pos = 0;
	uint32                         sel_count = 0;
	const int                     *indices = 0;

	if (count == 0) return;

	fep->dir_outliner->getSelection(&indices, (int *) &sel_count);
	if (sel_count > 0 && indices) {
		pos = indices[0];
		if (pos != (count-1)) {
			thePage->deselectPos(pos);
			thePage->swapDirs(pos, pos+1, pos+1);
		}
	}
}

// Member:       cb_add
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageMailNewsAddrBook::cb_add(Widget    /* w */,
										   XtPointer closure,
										   XtPointer /* callData */)
{
	XFE_PrefsPageMailNewsAddrBook *thePage = (XFE_PrefsPageMailNewsAddrBook *)closure;
	XFE_PrefsDialog               *theDialog = thePage->getPrefsDialog();
	Widget                         mainw = theDialog->getBaseWidget();

	//	while (!XtIsWMShell(mainw) && (XtParent(mainw)!=0))
	//		mainw = XtParent(mainw);

	// Instantiate an LDAP dialog

	XFE_PrefsLdapPropDialog *theLdapPropDialog = 0;

	if ((theLdapPropDialog =
		 new XFE_PrefsLdapPropDialog(theDialog, thePage, mainw, "prefsLdapProp")) == 0) {
	    fe_perror(thePage->getContext(), XP_GetString(XFE_OUT_OF_MEMORY_URL));
	    return;
	}

	// Initialize and pop up the LDAP dialog

	theLdapPropDialog->initPage(0);
	theLdapPropDialog->show();
}

// Member:       cb_edit
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageMailNewsAddrBook::cb_edit(Widget    /* w */,
											XtPointer closure,
											XtPointer /* callData */)
{
	XFE_PrefsPageMailNewsAddrBook *thePage = (XFE_PrefsPageMailNewsAddrBook *)closure;
	XFE_PrefsDialog               *theDialog = thePage->getPrefsDialog();
	PrefsDataMailNewsAddrBook     *fep = thePage->getData();
	Widget                         mainw = theDialog->getBaseWidget();
	int                            pos = 0;
	uint32                         sel_count = 0;
	const int                     *indices = 0;
	DIR_Server                    *server;

	// Find the entry that's selected

	fep->dir_outliner->getSelection(&indices, (int *) &sel_count);
	if (sel_count > 0 && indices) {
		pos = indices[0];
		if (pos >=  fep->num_directories) return;
		server = (DIR_Server*)XP_ListGetObjectNum(fep->directories,pos+1);
	}
	else 
		return;

	//	while (!XtIsWMShell(mainw) && (XtParent(mainw)!=0))
	//		mainw = XtParent(mainw);

	// Instantiate an LDAP dialog

	XFE_PrefsLdapPropDialog *theLdapPropDialog = 0;

	if ((theLdapPropDialog =
		 new XFE_PrefsLdapPropDialog(theDialog, thePage, mainw, "prefsLdapProp")) == 0) {
	    fe_perror(thePage->getContext(), XP_GetString(XFE_OUT_OF_MEMORY_URL));
	    return;
	}

	// Initialize and pop up the LDAP dialog

	theLdapPropDialog->initPage(server);
	theLdapPropDialog->show();
}

// Member:       cb_delete
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageMailNewsAddrBook::cb_delete(Widget    /* w */,
											  XtPointer closure,
											  XtPointer /* callData */)
{
	XFE_PrefsPageMailNewsAddrBook *thePage = (XFE_PrefsPageMailNewsAddrBook *)closure;
	PrefsDataMailNewsAddrBook     *fep = thePage->getData();
	int                            count = fep->num_directories;
	int                            pos = 0;
	uint32                         sel_count = 0;
	const int                     *indices = 0;

	if (count == 0) return;

	fep->dir_outliner->getSelection(&indices, (int *) &sel_count);
	if (sel_count > 0 && indices) {
		pos = indices[0];
		thePage->deleteDirAtPos(pos);
	}
}

// Member:       cb_toggleNameOrder
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageMailNewsAddrBook::cb_toggleNameOrder(Widget    w,
													   XtPointer closure,
													   XtPointer callData)
{
	XmToggleButtonCallbackStruct *cb = (XmToggleButtonCallbackStruct *)callData;
	PrefsDataMailNewsAddrBook    *fep = (PrefsDataMailNewsAddrBook *)closure;

	if (! cb->set) {
		XtVaSetValues(w, XmNset, True, 0);
	}
	else if (w == fep->first_last_toggle) {
		XtVaSetValues(fep->last_first_toggle, XmNset, False, 0);
	}
	else if (w == fep->last_first_toggle) {
		XtVaSetValues(fep->first_last_toggle, XmNset, False, 0);
	}
	else
		abort();	
}

/* Outlinable interface methods */

void *XFE_PrefsPageMailNewsAddrBook::ConvFromIndex(int /* index */)
{
	return (void *)NULL;
}

int XFE_PrefsPageMailNewsAddrBook::ConvToIndex(void * /* item */)
{
	return 0;
}

char*XFE_PrefsPageMailNewsAddrBook::getColumnName(int column)
{
	switch (column){
	case OUTLINER_COLUMN_NAME:
		return STRING_COL_NAME;
    default:
		XP_ASSERT(0); 
		return 0;
    }
}

char *XFE_PrefsPageMailNewsAddrBook::getColumnHeaderText(int column)
{
  switch (column) 
    {
    case OUTLINER_COLUMN_NAME:
      return "";
    default:
      XP_ASSERT(0);
      return 0;
    }
}

fe_icon *XFE_PrefsPageMailNewsAddrBook::getColumnHeaderIcon(int /* column */)
{
	return 0;

}

EOutlinerTextStyle XFE_PrefsPageMailNewsAddrBook::getColumnHeaderStyle(int /* column */)
{
	return OUTLINER_Default;
}

// This method acquires one line of data.
void *XFE_PrefsPageMailNewsAddrBook::acquireLineData(int line)
{
	m_rowIndex = line;
	return (void *)1;
}

void XFE_PrefsPageMailNewsAddrBook::getTreeInfo(Boolean * /* expandable */,
												Boolean * /* is_expanded */,
												int     * /* depth */,
												OutlinerAncestorInfo ** /* ancestor */)
{
	// No-op
}


EOutlinerTextStyle XFE_PrefsPageMailNewsAddrBook::getColumnStyle(int /*column*/)
{
    return OUTLINER_Default;
}

char *XFE_PrefsPageMailNewsAddrBook::getColumnText(int column)
{ 
	PrefsDataMailNewsAddrBook *fep = m_prefsDataMailNewsAddrBook;
	static char                line[OUTLINER_COLUMN_MAX_LENGTH+1];

	*line = 0;
    DIR_Server *dir = (DIR_Server*)XP_ListGetObjectNum(fep->directories,m_rowIndex+1);
	if (! dir) return line;

	switch (column) {
    case OUTLINER_COLUMN_NAME:
		sprintf(line, "%s", dir->description);
		break;

    default:
		break;
    }

	return line;
}

fe_icon *XFE_PrefsPageMailNewsAddrBook::getColumnIcon(int /* column */)
{
	return 0;
}

void XFE_PrefsPageMailNewsAddrBook::releaseLineData()
{
	// No-op
}

void XFE_PrefsPageMailNewsAddrBook::Buttonfunc(const OutlineButtonFuncData *data)
{
	int row = data->row;

	if (row < 0) {
		// header
		return;
	} 

	setSelectionPos(data->row);
}

void XFE_PrefsPageMailNewsAddrBook::Flippyfunc(const OutlineFlippyFuncData * /* data */)
{
	// No-op
}

XFE_Outliner *XFE_PrefsPageMailNewsAddrBook::getOutliner()
{
	return m_prefsDataMailNewsAddrBook->dir_outliner;
}
#endif // MOZ_MAIL_NEWS

// ************************************************************************
// ********************** Miscellaneous functions *************************
// ************************************************************************

