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
   EditorFrame.cpp -- class definition for the editor frame class
   Created: Richard Hess <rhess@netscape.com>, 11-Nov-96
 */


#include "rosetta.h"
#include "EditorFrame.h"
#include "EditorView.h"
#include "Command.h"
#include "ViewGlue.h" // remove with fe_HackEditorNotifyToolbarUpdated()
#include "PrefsDialog.h"
#include "xpassert.h"

#include "DtWidgets/ComboBox.h"

#include "xeditor.h"
#include "edt.h"
#include "EditRecentMenu.h"

/* for XP_GetString() */
#include <xpgetstr.h>
extern int XFE_DISABLED_BY_ADMIN;
extern int XFE_EDITOR_ALERT_ABOUT_DOCUMENT;

#ifdef DEBUG_spence
#define D(x) x
#else
#define D(x)
#endif

extern "C"   void xfe2_EditorInit(MWContext* context);
extern "C"   void fe_set_scrolled_default_size(MWContext *context);
extern "C"   void fe_HackTranslations (MWContext *, Widget);

MenuSpec XFE_EditorFrame::file_menu_spec[] = {
  { "newSubmenu",		CASCADEBUTTON,
	(MenuSpec*)&XFE_Frame::new_menu_spec },
  { xfeCmdOpenPage,		PUSHBUTTON },
  { "recentSubmenu",	DYNA_CASCADEBUTTON, 
	NULL, NULL, False, NULL,
	XFE_EditRecentMenu::generate 
  },
  MENU_SEPARATOR,
  { xfeCmdSave,         PUSHBUTTON },
  { xfeCmdSaveAs,	    PUSHBUTTON },
  { xfeCmdPublish,      PUSHBUTTON },
  MENU_SEPARATOR,
  { xfeCmdSendPage,		PUSHBUTTON },
  { xfeCmdBrowsePage,	PUSHBUTTON },
  MENU_SEPARATOR,
  //{ xfeCmdPrintSetup,	PUSHBUTTON },
  //{ xfeCmdPrintPreview,	PUSHBUTTON },
  { xfeCmdPrint,		PUSHBUTTON },
  MENU_SEPARATOR,
  { xfeCmdClose,		PUSHBUTTON },
  { xfeCmdExit,			PUSHBUTTON },
  { NULL }
};

MenuSpec XFE_EditorFrame::delete_table_menu_spec[] = {
	{ xfeCmdDeleteTable,       PUSHBUTTON },
	{ xfeCmdDeleteTableRow,    PUSHBUTTON },
	{ xfeCmdDeleteTableColumn, PUSHBUTTON },
	{ xfeCmdDeleteTableCell,   PUSHBUTTON },
	{ NULL }
};

MenuSpec XFE_EditorFrame::edit_menu_spec[] = {
  { xfeCmdUndo,			PUSHBUTTON },
  { xfeCmdRedo,			PUSHBUTTON },
  MENU_SEPARATOR,
  { xfeCmdCut,			PUSHBUTTON },
  { xfeCmdCopy,			PUSHBUTTON },
  { xfeCmdPaste,		PUSHBUTTON },
  { xfeCmdDeleteItem,	PUSHBUTTON },
  MENU_SEPARATOR,
  { "deleteTableMenu",	CASCADEBUTTON,
	(MenuSpec*)&delete_table_menu_spec },
  { xfeCmdRemoveLink,	PUSHBUTTON },
  MENU_SEPARATOR,
  { xfeCmdSelectAll,	PUSHBUTTON },
  { xfeCmdSelectTable,	PUSHBUTTON },
  MENU_SEPARATOR,
  { xfeCmdFindInObject,	PUSHBUTTON },
  { xfeCmdFindAgain,	PUSHBUTTON },
  { xfeCmdSearchAddress,	PUSHBUTTON },
  MENU_SEPARATOR,
  { xfeCmdEditPageSource,PUSHBUTTON },
  MENU_SEPARATOR,
  { xfeCmdEditPreferences,PUSHBUTTON },
  { NULL }
};

MenuSpec XFE_EditorFrame::view_menu_spec[] = {
  { xfeCmdToggleNavigationToolbar, PUSHBUTTON },
  { xfeCmdToggleFormatToolbar, PUSHBUTTON },
  MENU_SEPARATOR,
  { xfeCmdToggleParagraphMarks, PUSHBUTTON }, /* with toggling label */
  MENU_SEPARATOR,
  { xfeCmdReload,		    PUSHBUTTON },
  { xfeCmdShowImages,		PUSHBUTTON },
  { xfeCmdRefresh,		    PUSHBUTTON },
  { xfeCmdStopLoading,		PUSHBUTTON },
  //{ xfeCmdStopAnimations,	PUSHBUTTON },
  MENU_SEPARATOR,
  { xfeCmdViewPageSource,	PUSHBUTTON },
  { xfeCmdViewPageInfo,		PUSHBUTTON },
  //xxxAdd Page Services
  MENU_SEPARATOR,
  { "encodingSubmenu",		CASCADEBUTTON,
	(MenuSpec*)&XFE_Frame::encoding_menu_spec },
  // fix me..
  { NULL }
};

static MenuSpec insert_table_menu_spec[] = {
	{ xfeCmdInsertTable, PUSHBUTTON },
	{ xfeCmdInsertTableRow, PUSHBUTTON },
	{ xfeCmdInsertTableColumn, PUSHBUTTON },
	{ xfeCmdInsertTableCell, PUSHBUTTON },
	{ NULL }
};

MenuSpec XFE_EditorFrame::insert_menu_spec[] = {
	{ xfeCmdInsertLink, PUSHBUTTON },
	{ xfeCmdInsertTarget, PUSHBUTTON },
	{ xfeCmdInsertImage, PUSHBUTTON }, 
	{ xfeCmdInsertHorizontalLine, PUSHBUTTON },
	{ "insertTableMenu",		CASCADEBUTTON,
	  (MenuSpec*)&insert_table_menu_spec },
	{ xfeCmdInsertHtml, PUSHBUTTON },
	MENU_SEPARATOR,
	{ xfeCmdInsertLineBreak, PUSHBUTTON },
	{ xfeCmdInsertBreakBelowImage, PUSHBUTTON },
	{ NULL }
};

MenuSpec XFE_EditorFrame::character_style_menu_spec[] = {
	{ xfeCmdToggleCharacterStyleBold, TOGGLEBUTTON },
	{ xfeCmdToggleCharacterStyleItalic, TOGGLEBUTTON },
	{ xfeCmdToggleCharacterStyleUnderline, TOGGLEBUTTON },
	{ xfeCmdToggleCharacterStyleStrikethrough, TOGGLEBUTTON },
	{ xfeCmdToggleCharacterStyleSuperscript, TOGGLEBUTTON },
	{ xfeCmdToggleCharacterStyleSubscript, TOGGLEBUTTON },
	{ xfeCmdToggleCharacterStyleBlink, TOGGLEBUTTON },
	{ NULL }
};

static char editor_radio_group_name[] = "editorRadiogroup";

#define RADIOBUTTON TOGGLEBUTTON, NULL, editor_radio_group_name

MenuSpec XFE_EditorFrame::character_size_menu_spec[] = {
	{ xfeCmdSetFontSizeMinusTwo, RADIOBUTTON },
	{ xfeCmdSetFontSizeMinusOne, RADIOBUTTON },
	{ xfeCmdSetFontSizeZero, RADIOBUTTON },
	{ xfeCmdSetFontSizePlusOne, RADIOBUTTON },
	{ xfeCmdSetFontSizePlusTwo, RADIOBUTTON },
	{ xfeCmdSetFontSizePlusThree, RADIOBUTTON },
	{ xfeCmdSetFontSizePlusFour, RADIOBUTTON },
	MENU_SEPARATOR,
	{ xfeCmdSetFontSize, PUSHBUTTON },
	{ NULL }
};

MenuSpec XFE_EditorFrame::heading_style_menu_spec[] = {
	{ xfeCmdSetParagraphStyleNormal, RADIOBUTTON },
	{ xfeCmdSetParagraphStyleHeadingOne, RADIOBUTTON },
	{ xfeCmdSetParagraphStyleHeadingTwo, RADIOBUTTON },
	{ xfeCmdSetParagraphStyleHeadingThree, RADIOBUTTON },
	{ xfeCmdSetParagraphStyleHeadingFour, RADIOBUTTON },
	{ xfeCmdSetParagraphStyleHeadingFive, RADIOBUTTON },
	{ xfeCmdSetParagraphStyleHeadingSix, RADIOBUTTON },
	{ NULL }
};

MenuSpec XFE_EditorFrame::paragraph_style_menu_spec[] = {
	{ xfeCmdSetParagraphStyleNormal, RADIOBUTTON },
	{ xfeCmdSetParagraphStyleAddress, RADIOBUTTON },
	{ xfeCmdSetParagraphStyleFormatted, RADIOBUTTON },
	{ xfeCmdSetParagraphStyleDescriptionTitle, RADIOBUTTON },
	{ xfeCmdSetParagraphStyleDescriptionText, RADIOBUTTON },
	{ NULL }
};

MenuSpec XFE_EditorFrame::list_style_menu_spec[] = {
	{ xfeCmdSetListStyleNone, RADIOBUTTON },
	{ xfeCmdSetListStyleBulleted, RADIOBUTTON },
	{ xfeCmdSetListStyleNumbered, RADIOBUTTON },
    //xxxAdd Description
    //xxxAdd Menu
	{ NULL }
};

MenuSpec XFE_EditorFrame::alignment_style_menu_spec[] = {
	{ xfeCmdSetAlignmentStyleLeft, RADIOBUTTON },
	{ xfeCmdSetAlignmentStyleCenter, RADIOBUTTON },
	{ xfeCmdSetAlignmentStyleRight, RADIOBUTTON },
	{ NULL }
};

MenuSpec XFE_EditorFrame::format_menu_spec[] = {
	{ "fontStyleMenu",	      CASCADEBUTTON },
	{ "characterSizeMenu",	  CASCADEBUTTON,
	  (MenuSpec*)&XFE_EditorFrame::character_size_menu_spec },
	{ "characterStyleMenu",	  CASCADEBUTTON,
	  (MenuSpec*)&XFE_EditorFrame::character_style_menu_spec },
	{ xfeCmdSetCharacterColor,	  PUSHBUTTON },
	{ xfeCmdClearAllStyles,   PUSHBUTTON },
	MENU_SEPARATOR,
	{ "headingStyleMenu",	  CASCADEBUTTON,
	  (MenuSpec*)&XFE_EditorFrame::heading_style_menu_spec },
	{ "paragraphStyleMenu",	  CASCADEBUTTON,
	  (MenuSpec*)&XFE_EditorFrame::paragraph_style_menu_spec },
	{ "listStyleMenu",	  CASCADEBUTTON,
	  (MenuSpec*)&XFE_EditorFrame::list_style_menu_spec },
	{ "alignmentStyleMenu",	  CASCADEBUTTON,
	  (MenuSpec*)&XFE_EditorFrame::alignment_style_menu_spec },
	MENU_SEPARATOR,
	{ xfeCmdIndent,   PUSHBUTTON },
	{ xfeCmdOutdent,   PUSHBUTTON },
	MENU_SEPARATOR,
	{ xfeCmdSetObjectProperties,   PUSHBUTTON },
	{ xfeCmdSetTableProperties,   PUSHBUTTON },
	{ xfeCmdSetPageProperties,   PUSHBUTTON },
	HG10287
	{ NULL }
};

MenuSpec XFE_EditorFrame::tools_menu_spec[] = {
	{ xfeCmdSpellCheck,	PUSHBUTTON },
	{ NULL }
};

MenuSpec XFE_EditorFrame::go_menu_spec[] = {
  { xfeCmdBack,			PUSHBUTTON },
  { xfeCmdForward,		PUSHBUTTON },
  { xfeCmdHome,			PUSHBUTTON },
  // history goes here
  { NULL }
};

MenuSpec XFE_EditorFrame::bookmark_menu_spec[] = {
  { xfeCmdAddBookmark,		PUSHBUTTON },
  { xfeCmdFileBookmark,		PUSHBUTTON },
  { xfeCmdEditBookmark,		PUSHBUTTON },
  MENU_SEPARATOR,
  { xfeCmdNetscapeDirectory,	PUSHBUTTON },
  MENU_SEPARATOR,
  { NULL }
};

MenuSpec XFE_EditorFrame::menu_bar_spec[] = {
  { xfeMenuFile, 	CASCADEBUTTON,
	(MenuSpec*)&XFE_EditorFrame::file_menu_spec },
  { xfeMenuEdit, 	CASCADEBUTTON,
	(MenuSpec*)&XFE_EditorFrame::edit_menu_spec },
  { xfeMenuView, 	CASCADEBUTTON,
	(MenuSpec*)&XFE_EditorFrame::view_menu_spec },
  { xfeMenuInsert,  CASCADEBUTTON,
	(MenuSpec*)&XFE_EditorFrame::insert_menu_spec },
  { xfeMenuFormat,  CASCADEBUTTON,
	(MenuSpec*)&XFE_EditorFrame::format_menu_spec },
  { xfeMenuTools, 	CASCADEBUTTON,
	(MenuSpec*)&XFE_EditorFrame::tools_menu_spec },
  { xfeMenuWindow, 	CASCADEBUTTON, (MenuSpec*)&XFE_Frame::window_menu_spec },
  { xfeMenuHelp, 	CASCADEBUTTON, (MenuSpec*)&XFE_Frame::help_menu_spec },
  { NULL }
};


MenuSpec XFE_EditorFrame::new_submenu_spec[] = {
	MENU_PUSHBUTTON(xfeCmdNewBlank),
	MENU_SEPARATOR,
	MENU_PUSHBUTTON(xfeCmdNewTemplate),
	MENU_PUSHBUTTON(xfeCmdNewWizard),
	{ NULL }
};

MenuSpec XFE_EditorFrame::save_submenu_spec[] = {
	MENU_PUSHBUTTON(xfeCmdSave),
	MENU_PUSHBUTTON(xfeCmdSaveAs),
	{ NULL }
};

MenuSpec XFE_EditorFrame::publish_submenu_spec[] = {
	MENU_PUSHBUTTON(xfeCmdPublish),
	MENU_PUSHBUTTON(xfeCmdSendPage),
	{ NULL }
};

static ToolbarSpec editor_file_toolbar_spec[] = {
	{ 
		xfeCmdNewBlank,
		CASCADEBUTTON,
		&ed_new_group, NULL, NULL, NULL,					// Icons
		(MenuSpec*) &XFE_EditorFrame::new_submenu_spec,		// Submenu spec
		NULL , NULL, 										// Generate proc
		XFE_TOOLBAR_DELAY_LONG								// Popup delay
	},
	{ 
		xfeCmdOpenPage,
		DYNA_CASCADEBUTTON,
		&ed_open_group, NULL, NULL, NULL,					// Icons
		NULL,												// Submenu spec
		XFE_EditRecentMenu::generate, (void *) True,		// Generate proc
		XFE_TOOLBAR_DELAY_LONG								// Popup delay
	},
	{ 
		xfeCmdSave,
		CASCADEBUTTON,
		&ed_save_group, NULL, NULL, NULL,					// Icons
		(MenuSpec*) &XFE_EditorFrame::save_submenu_spec,	// Submenu spec
		NULL , NULL, 										// Generate proc
		XFE_TOOLBAR_DELAY_LONG								// Popup delay
	},
	{ xfeCmdBrowsePage,	PUSHBUTTON,           &ed_browse_group  },
	{ 
		xfeCmdPublish,
		CASCADEBUTTON,
		&ed_publish_group, NULL, NULL, NULL,				// Icons
		(MenuSpec*) &XFE_EditorFrame::publish_submenu_spec,	// Submenu spec
		NULL , NULL, 										// Generate proc
		XFE_TOOLBAR_DELAY_LONG								// Popup delay
	},
	TOOLBAR_SEPARATOR,
	{ xfeCmdPrint,      PUSHBUTTON,           &ed_print_group   }, 
	{ xfeCmdSpellCheck,	PUSHBUTTON,           &ed_spellcheck_group },
	TOOLBAR_SEPARATOR,
	{ xfeCmdInsertLink, PUSHBUTTON,           &ed_link_group    },
	{ xfeCmdInsertTarget, PUSHBUTTON,         &ed_target_group  },
	{ xfeCmdInsertImage, PUSHBUTTON,          &ed_image_group    },
	{ xfeCmdInsertHorizontalLine, PUSHBUTTON, &ed_hrule_group   },
	{ xfeCmdInsertTable, PUSHBUTTON,          &ed_table_group   },

	{ NULL }
};

static ToolbarSpec alignment_menu_spec[] = {
	{ xfeCmdSetAlignmentStyleLeft,	 PUSHBUTTON, &ed_left_group },
	{ xfeCmdSetAlignmentStyleCenter, PUSHBUTTON, &ed_center_group },
	{ xfeCmdSetAlignmentStyleRight,	 PUSHBUTTON, &ed_right_group },
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
	{ xfeCmdClearAllStyles,                PUSHBUTTON  , &ed_clear_group },
	TOOLBAR_SEPARATOR,

	{ xfeCmdInsertBulletedList,	TOGGLEBUTTON, &ed_bullet_group },
	{ xfeCmdInsertNumberedList,	TOGGLEBUTTON, &ed_number_group },
	TOOLBAR_SEPARATOR,

	{ xfeCmdOutdent,	PUSHBUTTON, &ed_outdent_group },
	{ xfeCmdIndent,	PUSHBUTTON, &ed_indent_group },
	{ xfeCmdSetAlignmentStyle, CASCADEBUTTON, &ed_left_group, 0, 0, 0,
	  (MenuSpec*)&alignment_menu_spec },
	{ NULL }
};

static MenuSpec fe_editor_document_popups[] =
{
	MENU_PUSHBUTTON(xfeCmdSetPageProperties),
	MENU_SEPARATOR,
	MENU_END
};

static MenuSpec fe_editor_character_popups[] =
{
	MENU_PUSHBUTTON_1ARG("textProperties", xfeCmdDialog, "text"),
	MENU_PUSHBUTTON_1ARG("paragraphProperties", xfeCmdDialog,
						 "paragraph"),
	MENU_SEPARATOR,
	MENU_END
};

static MenuSpec fe_editor_image_popups[] =
{
	MENU_PUSHBUTTON_1ARG("imageProperties", xfeCmdDialog, "image"),
	MENU_PUSHBUTTON_1ARG("paragraphProperties", xfeCmdDialog,
						 "paragraph"),
	MENU_SEPARATOR,
	MENU_END
};

static MenuSpec fe_editor_tag_popups[] =
{
	MENU_PUSHBUTTON_1ARG("tagProperties", xfeCmdDialog, "tag"),
	MENU_PUSHBUTTON_1ARG("paragraphProperties", xfeCmdDialog,
						 "paragraph"),
	MENU_SEPARATOR,
	MENU_END
};

static MenuSpec fe_editor_target_popups[] =
{
	MENU_PUSHBUTTON_1ARG("targetProperties", xfeCmdDialog, "target"),
	MENU_PUSHBUTTON_1ARG("paragraphProperties", xfeCmdDialog,
						 "paragraph"),
	MENU_SEPARATOR,
	MENU_END
};

static MenuSpec fe_editor_hrule_popups[] =
{
	MENU_PUSHBUTTON_1ARG("hruleProperties", xfeCmdDialog, "hrule"),
	MENU_PUSHBUTTON_1ARG("paragraphProperties", xfeCmdDialog,
						 "paragraph"),
	MENU_SEPARATOR,
	MENU_END
};

static MenuSpec fe_editor_table_insert_popups[] =
{
	MENU_PUSHBUTTON(xfeCmdInsertTable),
	MENU_PUSHBUTTON(xfeCmdInsertTableRow),
	MENU_PUSHBUTTON(xfeCmdInsertTableColumn),
	MENU_PUSHBUTTON(xfeCmdInsertTableCell),
	MENU_END
};

static MenuSpec fe_editor_table_delete_popups[] =
{
	MENU_PUSHBUTTON(xfeCmdDeleteTable),
	MENU_PUSHBUTTON(xfeCmdDeleteTableRow),
	MENU_PUSHBUTTON(xfeCmdDeleteTableColumn),
	MENU_PUSHBUTTON(xfeCmdDeleteTableCell),
	MENU_END
};

static MenuSpec fe_editor_table_popups[] =
{
	MENU_PUSHBUTTON_1ARG("tableProperties", xfeCmdDialog, "table"),
	MENU_MENU("insert" ,fe_editor_table_insert_popups),
	MENU_MENU("delete" ,fe_editor_table_delete_popups),
	MENU_SEPARATOR,
	MENU_END
};

static MenuSpec fe_editor_link_popups[] =
{
	MENU_PUSHBUTTON_1ARG("linkProperties", xfeCmdDialog, "link"),
	MENU_PUSHBUTTON(xfeCmdOpenLinkNew),
	MENU_PUSHBUTTON(xfeCmdOpenLinkEdit),
	MENU_PUSHBUTTON(xfeCmdAddLinkBookmark),
	MENU_PUSHBUTTON(xfeCmdCopyLink),
	MENU_PUSHBUTTON(xfeCmdRemoveLink),
	MENU_SEPARATOR,
	MENU_END
};

static MenuSpec fe_editor_insert_link_popups[] =
{
	MENU_PUSHBUTTON(xfeCmdInsertLink),
	MENU_SEPARATOR,
	MENU_END
};

static MenuSpec fe_editor_edit_popups[] =
{
	MENU_PUSHBUTTON(xfeCmdCut),
	MENU_PUSHBUTTON(xfeCmdCopy),
	MENU_PUSHBUTTON(xfeCmdPaste),
	MENU_END
};

#if 0
static MenuSpec fe_editor_title_popups[] =
{
    { "title", LABEL },
	MENU_SEPARATOR,
	MENU_END
};
#endif

static MenuSpec fe_editor_show_options_popups[] =
{
	MENU_SEPARATOR,
	MENU_PUSHBUTTON(xfeCmdToggleMenubar),
	MENU_PUSHBUTTON(xfeCmdToggleNavigationToolbar),
	MENU_PUSHBUTTON(xfeCmdToggleFormatToolbar),
	MENU_END
};

XFE_PopupMenu*
fe_EditorNewPopupMenu(XFE_Frame* frame, Widget parent)
{
	MWContext*     context = frame->getContext();
	XFE_PopupMenu* popup;
    ED_ElementType e_type;
    MenuSpec*      list;

	if (!EDT_IS_EDITOR(context))
		return NULL;

	popup = new XFE_PopupMenu("popup",frame, parent, NULL);

	e_type = EDT_GetCurrentElementType(context);

    switch (e_type) {
    case ED_ELEMENT_IMAGE:       list = fe_editor_image_popups;  break;
    case ED_ELEMENT_UNKNOWN_TAG: list = fe_editor_tag_popups;    break;
    case ED_ELEMENT_TARGET:      list = fe_editor_target_popups; break;
    case ED_ELEMENT_HRULE:       list = fe_editor_hrule_popups;  break;
    case ED_ELEMENT_TEXT:
    default:                     list = fe_editor_character_popups; break;
    }

	popup->addMenuSpec(list);

    /*
     *    Document Props..
     */
	popup->addMenuSpec(fe_editor_document_popups);

	/*
	 *    Table support.
	 */
	if (EDT_IsInsertPointInTable(context) != 0) {
		popup->addMenuSpec(fe_editor_table_popups);
	}
	
	/*
	 *    Link stuff.
	 */
	if (EDT_GetHREF(context)) { 
		popup->addMenuSpec(fe_editor_link_popups);
	} else {
		popup->addMenuSpec(fe_editor_insert_link_popups);
	}
	
	/*
	 *    Add cut&paste menu.
	 */
	popup->addMenuSpec(fe_editor_edit_popups);
	
	/*
	 *    If they accidently deleted the menubar, help them out.
	 */
	if (!CONTEXT_DATA(context)->show_menubar_p)
		popup->addMenuSpec(fe_editor_show_options_popups);

	return popup;
}

static MenuSpec*
fe_editor_create_plugin_menu(MenuSpec* static_spec)
{
	//
	//    Count the number of static entries in the tools menu.
	//
	int32 nstatics = 0;
	if (static_spec != NULL) {
		while (static_spec[nstatics].menuItemName != NULL) {
			nstatics++;
		}
	}
	
	//
	//    Register Java Plugin Commands.
	//
    int32 ncategories = EDT_NumberOfPluginCategories();
	
	if (!ncategories)
		return static_spec; // the static menu will do fine
	
	MenuSpec* menu_spec = new MenuSpec[ncategories+nstatics+1];
	memset(menu_spec, 0, sizeof(MenuSpec) * (ncategories+nstatics+1));

	for (int n = 0; n < nstatics; n++)
		menu_spec[n] = static_spec[n];
	
	int32 category_index = 0;
	for (int32 category = 0; category < ncategories; category++) {
	
		char* category_name = XP_STRDUP(EDT_GetPluginCategoryName(category));

		if (!category_name)
			continue;

		int32 nplugins = EDT_NumberOfPlugins(category);

		if (!nplugins) // no plugins for this category.
			continue;

		MenuSpec* category_spec = new MenuSpec[nplugins+1];
		memset(category_spec, 0, sizeof(MenuSpec) * (nplugins+1));

		int32 plugin = 0;
		for (; plugin < nplugins; plugin++) {
			char buf[64];
			sprintf(buf, "javaPlugin_%d_%d", category, plugin);
			char* command_name = Command::intern(buf);

			category_spec[plugin].menuItemName = command_name;
			category_spec[plugin].tag = PUSHBUTTON;
		}
		category_spec[plugin].menuItemName = NULL; // delimit paranoia

		menu_spec[nstatics+category_index].menuItemName = category_name;
		menu_spec[nstatics+category_index].tag = CASCADEBUTTON;
		menu_spec[nstatics+category_index].submenu = category_spec;
		category_index++;
	}
	menu_spec[nstatics+category_index].menuItemName = NULL; // delimit paranoia

	return menu_spec;
}

static MenuSpec*
fe_editor_create_font_face_menu(XFE_Frame* frame)
{
	CommandType  command_id = xfeCmdSetFontFace;
	XFE_Command* handler = frame->getCommand(command_id);

	if (!handler)
		return NULL;

	XFE_CommandParameters* params = handler->getParameters(frame);
	unsigned n;

	for (n = 0; params[n].name != NULL; n++)
		;

	MenuSpec* spec = (MenuSpec*)XP_CALLOC((n+1), sizeof(MenuSpec));
	
	for (n = 0; params[n].name != NULL; n++) {
		spec[n].menuItemName = command_id;
		spec[n].tag = TOGGLEBUTTON;
		spec[n].radioGroupName = editor_radio_group_name;
		spec[n].cmd_name = command_id;
		spec[n].cmd_args[0] = params[n].name;
		spec[n].callData = params[n].data;
	}

	return spec;
}

static MenuSpec* i_tools_menu = 0;
static MenuSpec* i_font_menu = 0;

MenuSpec*
fe_EditorInstanciateMenu(XFE_Frame* f, MenuSpec* spec)
{
	//
	//    Find the Tools menu and install Composer Plugin menu.
	//
	for (unsigned n = 0; spec[n].menuItemName != NULL; n++) {

		if (XP_STRCMP(spec[n].menuItemName, xfeMenuTools) == 0) {
			
			if (!i_tools_menu)
				i_tools_menu = fe_editor_create_plugin_menu(spec[n].submenu);
			
			spec[n].submenu = i_tools_menu;
		}

		else if (XP_STRCMP(spec[n].menuItemName, xfeMenuFormat) == 0) {
			MenuSpec* fspec = spec[n].submenu;
			for (unsigned m = 0; fspec[m].menuItemName != NULL; m++) {
				if (XP_STRCMP(fspec[m].menuItemName, "fontStyleMenu") == 0) {

					if (!i_font_menu)
						i_font_menu = fe_editor_create_font_face_menu(f);

					fspec[m].submenu = i_font_menu; 
				}
			}
		}
	}
	
	return spec;
}

static MenuSpec* initialized_menu_bar_spec = 0;

XFE_EditorFrame::XFE_EditorFrame(Widget toplevel,
								 XFE_Frame *parent_frame,
								 Chrome *chromespec)
  : XFE_Frame("Editor", toplevel, parent_frame, FRAME_EDITOR, chromespec, True, True, True)
{
  geometryPrefName = "editor";

  if (parent_frame)
    fe_copy_context_settings(m_context, parent_frame->getContext());

  // create html view
  XFE_EditorView *editorview = new XFE_EditorView(this, 
												  getChromeParent(),
												  NULL,
												  m_context);

  editorview->registerInterest(XFE_EditorView::newURLLoading, 
		this,
		(XFE_FunctionNotification)newPageLoading_cb);

  /*
   *    Hacks to make old XFE code work. These should go away
   *    as old XFE code is sent to pasture.
   */
  CONTEXT_DATA (m_context)->show_character_toolbar_p = TRUE;
  CONTEXT_DATA (m_context)->show_paragraph_toolbar_p = TRUE;
  CONTEXT_DATA (m_context)->top_area = getChromeParent();
  CONTEXT_DATA (m_context)->menubar = m_menubar->getBaseWidget();

  setView(editorview);

  if (!initialized_menu_bar_spec)
	  initialized_menu_bar_spec = fe_EditorInstanciateMenu(this,
														   menu_bar_spec);

  setMenubar(initialized_menu_bar_spec);
  setToolbar((ToolbarSpec*)&editor_file_toolbar_spec);

  m_format_toolbar = new XFE_EditorToolbar(this,
										   m_toolbox,
										   "editorFormattingToolbar",
									(ToolbarSpec*)&editor_style_toolbar_spec,
										   True);

  fe_set_scrolled_default_size(m_context);

  m_toolbar->show();
//  m_format_toolbar->show();
  editorview->show();

  m_dashboard->setShowStatusBar(True);
  m_dashboard->setShowProgressBar(True);

  // Configure the toolbox for the first time
  configureToolbox();
}

int
XFE_EditorFrame::getURL(URL_Struct *url)
{
  XFE_EditorView *eview = (XFE_EditorView*)m_view;

  return eview->getURL(url);
}

XFE_CALLBACK_DEFN(XFE_EditorFrame, navigateToURL)(XFE_NotificationCenter*, void*, void* /*callData*/)
{
  // int status;
  // URL_Struct *url_struct = (URL_Struct*)callData;
  //
  // status = getURL(url_struct);
  //
  // if (status >= 0)
  //  {
  //     if (url_struct && url_struct->address)
  //       m_urlBar->recordURL(url_struct);
  //  }
}

XFE_CALLBACK_DEFN(XFE_EditorFrame, newPageLoading)(XFE_NotificationCenter*, void*, void* /*callData*/)
{
  // URL_Struct *url = (URL_Struct*)callData;
  //
  // m_urlBar->setURLString(url);
}

MWContext*
XFE_EditorFrame::getContext()
{
  if (m_view)
    return m_view->getContext();
  else
    return NULL;
}

#ifdef DEBUG_djw_NOT_RIGHT_NOW_THANK_YOU
#define TRACE(x) printf x
#else
#define TRACE(x)
#endif

Boolean
XFE_EditorFrame::isCommandEnabled(CommandType cmd,
								  void* calldata, XFE_CommandInfo* info)
{
	TRACE(("XFE_EditorFrame::isCommandEnabled(%d,0x%x)\n", cmd, calldata));
	
	if (cmd == xfeCmdToggleFormatToolbar) {
		return True;
	}
	
    return XFE_Frame::isCommandEnabled(cmd, calldata, info);
}


Boolean
XFE_EditorFrame::isCommandSelected(CommandType cmd, 
								  void* calldata, XFE_CommandInfo* info)
{
	TRACE(("XFE_EditorFrame::isCommandSelected(%s,0x%x)\n",
		   Command::getString(cmd), calldata));

	return XFE_Frame::isCommandSelected(cmd, calldata, info);
}

void
XFE_EditorFrame::doCommand(CommandType cmd,
						   void* calldata, XFE_CommandInfo* info)
{
	TRACE(("XFE_EditorFrame::doCommand(%d,0x%x)\n", cmd, calldata));

	if (cmd == xfeCmdToggleFormatToolbar) {
			// Toggle the showing state
			m_format_toolbar->toggleShowingState();

			//
			// The toolboxItemChangeShowing() call is needed to make the 
			// state of toolbars persistent.
			//
			
			// Update prefs
			toolboxItemChangeShowing(m_format_toolbar);

			//
			// Im not sure if the following invocation is needed, but 
			// every single other frame that that toggles toolbar has it.
			//

			// Update chrome
			notifyInterested(XFE_View::chromeNeedsUpdating);
	} else if (cmd == xfeCmdEditPreferences) { // override BrowserFrame
		fe_showEditorPreferences(this, getContext());
	} else {
		XFE_Frame::doCommand(cmd, calldata, info);
	}
}

Boolean
XFE_EditorFrame::handlesCommand(CommandType cmd,
								void* calldata, XFE_CommandInfo* info)
{
	TRACE(("XFE_EditorFrame::handlesCommand(%d,0x%x)\n", cmd, calldata));

	if (cmd == xfeCmdToggleFormatToolbar) {
		return True;
	}

    return XFE_Frame::handlesCommand(cmd, calldata, info);
}

char*
XFE_EditorFrame::commandToString(CommandType cmd,
								 void* calldata, XFE_CommandInfo* info)
{
	TRACE(("XFE_EditorFrame::commandToString(%d,0x%x)\n", cmd, calldata));
	if (cmd == xfeCmdToggleFormatToolbar) {

		Widget widget = NULL;
		Boolean show = !m_format_toolbar->isShown();
		
		if (info != NULL)
			widget = info->widget;
		
		return getShowHideLabelString(cmd, show, widget);

	} else {
		return XFE_Frame::commandToString(cmd, calldata, info);
	}
}

XFE_Command*
XFE_EditorFrame::getCommand(CommandType cmd)
{
	XFE_Command* handler = getView()->getCommand(cmd);

	if (handler != NULL)
		return handler;

	return XFE_Frame::getCommand(cmd);
}

extern "C" Boolean
fe_save_file_check(MWContext* context,
				   Boolean file_must_exist, Boolean auto_save);

XP_Bool
XFE_EditorFrame::isOkToClose()
{
	MWContext* context = getContext();

	if (fe_save_file_check(context, False, False)) {

		/*
		 *    We've already had this discussion....
		 */
		EDT_SetDirtyFlag(context, FALSE);
		
		return TRUE;
	} else {
		return FALSE;
	}
}

static void
editor_do_close_cb(void* closure)
{
	XFE_EditorFrame* frame = (XFE_EditorFrame *)closure;

	frame->XFE_Frame::doClose();
}

void
XFE_EditorFrame::doClose()
{
	MWContext*     context = getContext();
	History_entry* hist_entry = SHIST_GetCurrent(&context->hist);
	char*          url = NULL;
	
	if (hist_entry != NULL)
		url = hist_entry->address;
	
	EDT_PreClose(context, url, editor_do_close_cb, (void*)this);
}

//
// These 2 are here cause the editor does not have all the fancy toolbox 
// and logo crap that the other frames do, so the configureLogo() method
// cuases the frame to do nothing and getLogo() always returns null so that
// the logo never appears
//
void
XFE_EditorFrame::configureLogo()
{
}
//////////////////////////////////////////////////////////////////////////
XFE_Logo *
XFE_EditorFrame::getLogo()
{
	return NULL;
}
//////////////////////////////////////////////////////////////////////////


//
//    Moved here from editor.c. I think (?) the only place this is called
//    now is for a cancelled remote save. See cmd/xfe/editor.c, rev 1.146
//    for audit trail.
//
extern "C" void
fe_EditorDelete(MWContext* context)
{

	XFE_Frame* frame = ViewGlue_getFrame(context);

	frame->delete_response();
}

//
//    This guy is called when we just want to get the hell out of here.
//    No discussions with the user, just go. This is used when we load a
//    bad url or something like that. We don't know it's bogus until after
//    we have made the window, loaded the url, and generaly got into a state
//    we dont' want to be in. So just go, skip the PreClose() stuff and go.
//
extern "C" void
fe_EditorKill(MWContext* context)
{
	XFE_Frame* frame = ViewGlue_getFrame(context);

	/*
	 *    Mark it is a new document, that is not dirty, so that we don't
	 *    bother with "do you really want to quit?" dialogs.
	 */
	context->is_new_document = TRUE;
	EDT_SetDirtyFlag(context, FALSE);

	frame->XFE_Frame::doClose(); // skip PreClose, call super.
}

// --------------------------------------------------------------------------
struct fe_editor_pre_open_info
{
	Widget      toplevel;
	XFE_Frame*  parent_frame;
	Chrome*     chromespec;
	char*       address;
};

static void 
fe_editor_pre_open_cb(XP_Bool cancelled, char* address, void* closure)
{
	fe_editor_pre_open_info* info = (fe_editor_pre_open_info*)closure;

	if (!cancelled) {

		MWContext*       context = NULL;
		XFE_EditorFrame* theFrame;

		theFrame = new XFE_EditorFrame(info->toplevel,
									   info->parent_frame,
									   info->chromespec);

		theFrame->show();

		context = theFrame->getContext();

		fe_UserActivity(context);
		xfe2_EditorInit(context); // im registration, focus event handlers, ...
    
		if (address == NULL) {
			//
			// NOTE:  create a blank address...
			//
			address = "about:editfilenew";
		}

		URL_Struct* url = NET_CreateURLStruct(address, NET_NORMAL_RELOAD);
		theFrame->getURL(url);
	}

	if (info->address)
		XP_FREE(info->address);

	delete info;
}

MWContext*
fe_showEditor(Widget      toplevel,
			  XFE_Frame*  parent_frame,
			  Chrome*     chromespec,
			  URL_Struct* url)
{
	// not a static global, since we can have multiple editors.
	MWContext*       context = NULL;

	if (parent_frame != NULL)
		context = parent_frame->getContext();

	if (fe_IsEditorDisabled()) {
		if (context != NULL)
			FE_Alert(context, XP_GetString(XFE_DISABLED_BY_ADMIN));
		return NULL;
	}

	/*
	 *    Should test for others we know are bogus. There is a BE call
	 *    for this.
	 */
	if (url != NULL && XP_STRNCMP(url->address, "about:", 6) == 0) {
		if (context != NULL)
			FE_Alert(context, XP_GetString(XFE_EDITOR_ALERT_ABOUT_DOCUMENT));
		return NULL;
	}

	fe_editor_pre_open_info* info = new fe_editor_pre_open_info;

	info->toplevel = toplevel;
	info->parent_frame = parent_frame;
	info->chromespec = chromespec;
	if (url != NULL && url->address != NULL)
		info->address = XP_STRDUP(url->address);
	else
		info->address = NULL;
		
	if (context != NULL && url != NULL && url->address != NULL) {
		EDT_PreOpen(context, info->address, fe_editor_pre_open_cb, info);
	} else {
		fe_editor_pre_open_cb(FALSE, info->address, info);
	}
	
	return NULL;
}

// --------------------------------------------------------------------------

MWContext*
fe_EditorNew(MWContext* context,
			 XFE_Frame* parent_frame,
			 Chrome*    chromespec,
			 char*      address)
{
	Widget      toplevel;
	MWContext*  new_context;
	URL_Struct* url = NULL;

	if (address != NULL)
		url = NET_CreateURLStruct(address, NET_NORMAL_RELOAD);
	
	toplevel = XtParent(CONTEXT_WIDGET(context));

	//
	//    If there is no parent_frame, try to derive one from the context.
	//    Otherwise the constructor doesn't copy the context bits!
	//
	if (parent_frame == NULL && context != NULL)
		parent_frame = ViewGlue_getFrame(context);
	
	new_context = fe_showEditor(toplevel, parent_frame, chromespec, url);

	return new_context;
}

static MWContext*
fe_editor_find_context(char* address, int type)
{
    struct fe_MWContext_cons* rest;
    MWContext* context;
    History_entry* hist_entry;

	if (!address)
		return NULL;

    for (rest = fe_all_MWContexts; rest; rest = rest->next) {
        context = rest->context;
		if (context->type == type) {

			char* hist_address = NULL;
			hist_entry = SHIST_GetCurrent(&context->hist);
			if (hist_entry != NULL)
				hist_address = hist_entry->address;

			//
			//    If the addresses match, just remap the existing one.
			//    Note the following hack, because it happens so much:
			//    "www.foo.com/file.html" == "http://www.foo.com/file.html"
			//    "/dir/file.html" == "file:/dir/file.html"
			//    "/dir/file.html" == "file:///dir/file.html"
			//
			if (hist_address != NULL && 
				(XP_STRCMP(hist_address, address) == 0 /* exact */

				 )
				)
				return context;
		}
    }
    return NULL;
}

static void
fe_map_raise_timeout(XtPointer closure, XtIntervalId*)
{
    MWContext* context = (MWContext *)closure;
    Widget     widget = CONTEXT_WIDGET(context);
    Display*   display = XtDisplay(widget);
    Window     window = XtWindow(widget);


    XMapWindow(display, window);
    XRaiseWindow(display, window);

}

void
fe_map_raise_window_when_idle(MWContext* context)
{
    /*
     *    Delay the raise until after any dialogs, etc have
     *    been unmanaged. Otherwise we bounce backwards and forwards,
     *    and sometimes the window is left unraised.
     */
    XtAppAddTimeOut(fe_XtAppContext, 0, fe_map_raise_timeout,
					(XtPointer)context);
}

MWContext* 
fe_EditorEdit(MWContext* context,
			 XFE_Frame*  parent_frame,
			 Chrome*     chromespec,
			 char*       address)
{
    History_entry* hist_ent;

	if (address == NULL) { /* edit this page */
		hist_ent = SHIST_GetCurrent(&context->hist);
		if (hist_ent != NULL)
			address = hist_ent->address;
	}

	MWContext* new_context = fe_editor_find_context(address, MWContextEditor);

	if (new_context) {
		fe_map_raise_window_when_idle(new_context);
	} else {
		new_context = fe_EditorNew(context, parent_frame, chromespec, address);
	}

	return new_context;
}

MWContext* 
fe_EditorOpen(MWContext* context,
			 XFE_Frame*  parent_frame,
			 Chrome*     chromespec)
{
	MWContext* new_context = XP_FindContextOfType(context, MWContextEditor);

	if (new_context) {
		fe_map_raise_window_when_idle(new_context);
	} else {
		new_context = fe_EditorNew(context, parent_frame, chromespec, NULL);
	}

	return new_context;
}

extern "C" void
fe_HackEditorNotifyToolbarUpdated(MWContext* context)
{
	XFE_Frame* frame = ViewGlue_getFrame(context);

	if (frame)
		frame->notifyInterested(XFE_View::chromeNeedsUpdating, 0);
}

//
//    This function exists because fe_showEditorPreferences(), though
//    defined as a C function, has C++ class as the first arg, and the
//    first arg can be derived from the second! Joy.
//
void
fe_EditorShowNewPreferences(MWContext* context)
{
	XFE_Frame* frame = ViewGlue_getFrame(context);

	if (frame)
		fe_showEditorPreferences(frame, context);
}

//////////////////////////////////////////////////////////////////////////
//
// Toolbox methods
//
//////////////////////////////////////////////////////////////////////////
void
XFE_EditorFrame::toolboxItemSnap(XFE_ToolboxItem * item)
{
	XP_ASSERT( item == m_toolbar || item == m_format_toolbar );

	// Composition
	fe_globalPrefs.composer_composition_toolbar_position = m_toolbar->getPosition();

	// Formatting
fe_globalPrefs.composer_formatting_toolbar_position = m_format_toolbar->getPosition();
}
//////////////////////////////////////////////////////////////////////////
void
XFE_EditorFrame::toolboxItemClose(XFE_ToolboxItem * item)
{
	XP_ASSERT( item != NULL );

	// Composition
	if (item == m_toolbar)
	{
		fe_globalPrefs.composer_composition_toolbar_open = False;
	}
	// Formatting
	else if (item == m_format_toolbar)
	{
		fe_globalPrefs.composer_formatting_toolbar_open = False;
	}
}
//////////////////////////////////////////////////////////////////////////
void
XFE_EditorFrame::toolboxItemOpen(XFE_ToolboxItem * item)
{
	XP_ASSERT( item != NULL );

	// Composition
	if (item == m_toolbar)
	{
		fe_globalPrefs.composer_composition_toolbar_open = True;
	}
	// Formatting
	else if (item == m_format_toolbar)
	{
		fe_globalPrefs.composer_formatting_toolbar_open = True;
	}
}
//////////////////////////////////////////////////////////////////////////
void
XFE_EditorFrame::toolboxItemChangeShowing(XFE_ToolboxItem * item)
{
	XP_ASSERT( item != NULL );

	// Composition
	if (item == m_toolbar)
	{
		fe_globalPrefs.composer_composition_toolbar_showing = item->isShown();
	}
	// Formatting
	else if (item == m_format_toolbar)
	{
		fe_globalPrefs.composer_formatting_toolbar_showing = item->isShown();
	}
}
//////////////////////////////////////////////////////////////////////////
void
XFE_EditorFrame::configureToolbox()
{
	// If a the frame was constructed with a chromespec, then we ignore
	// all the preference magic.
	if (m_chromespec_provided)
	{
		return;
	}

	// Make sure the toolbox is alive
	if (!m_toolbox || (m_toolbox && !m_toolbox->isAlive()))
	{
		return;
	}

	// Composition
	if (m_toolbar)
	{
		m_toolbar->setShowing(fe_globalPrefs.composer_composition_toolbar_showing);
		m_toolbar->setOpen(fe_globalPrefs.composer_composition_toolbar_open);
		m_toolbar->setPosition(fe_globalPrefs.composer_composition_toolbar_position);
	}

	// Formatting
	if (m_format_toolbar)
	{
		m_format_toolbar->setShowing(fe_globalPrefs.composer_formatting_toolbar_showing);
		m_format_toolbar->setOpen(fe_globalPrefs.composer_formatting_toolbar_open);
		m_format_toolbar->setPosition(fe_globalPrefs.composer_formatting_toolbar_position);
	}
}
//////////////////////////////////////////////////////////////////////////


// EOF
