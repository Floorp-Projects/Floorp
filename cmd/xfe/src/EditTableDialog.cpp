/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
   EditTableDialog.cpp -- create and modify tables in Composer.
   Contains a lot of code moved from editordialogs.c,
   which needs to be cleaned up asap.
   Created: Akkana Peck <akkana@netscape.com>, 25-Jun-98.
 */

/* These come from xfe_err.h */
extern int XFE_EDITOR_TABLE_ROW_RANGE;
extern int XFE_EDITOR_TABLE_COLUMN_RANGE;
extern int XFE_EDITOR_TABLE_BORDER_RANGE;
extern int XFE_EDITOR_TABLE_SPACING_RANGE;
extern int XFE_EDITOR_TABLE_PADDING_RANGE;
extern int XFE_EDITOR_TABLE_WIDTH_RANGE;
extern int XFE_EDITOR_TABLE_HEIGHT_RANGE;
extern int XFE_EDITOR_TABLE_IMAGE_WIDTH_RANGE;
extern int XFE_EDITOR_TABLE_IMAGE_HEIGHT_RANGE;
extern int XFE_EDITOR_TABLE_IMAGE_SPACE_RANGE;

#define TABLE_MAX_ROWS 100
#define TABLE_MIN_ROWS 1
#define TABLE_MAX_COLUMNS 100
#define TABLE_MIN_COLUMNS 1
#define TABLE_MAX_BORDER 10000
#define TABLE_MIN_BORDER 0
#define TABLE_MAX_SPACING 10000
#define TABLE_MIN_SPACING 0
#define TABLE_MAX_PADDING 10000
#define TABLE_MIN_PADDING 0
#define TABLE_MAX_PIXEL_WIDTH  10000
#define TABLE_MIN_PIXEL_WIDTH  1
#define TABLE_MAX_PIXEL_HEIGHT  10000
#define TABLE_MIN_PIXEL_HEIGHT  1
#define TABLE_MAX_CELL_NROWS 100
#define TABLE_MIN_CELL_NROWS 1
#define TABLE_MAX_CELL_NCOLUMNS 100
#define TABLE_MIN_CELL_NCOLUMNS 1

/* BWEEP BWEEP BWEEP!  These must match defs in editordialogs.c! */
#define TABLE_MAX_PERCENT_WIDTH 100
#define TABLE_MIN_PERCENT_WIDTH 1
#define TABLE_MAX_PERCENT_HEIGHT 100
#define TABLE_MIN_PERCENT_HEIGHT 1

#define XFE_INVALID_TABLE_NROWS     1
#define XFE_INVALID_TABLE_NCOLUMNS  2
#define XFE_INVALID_TABLE_BORDER    3
#define XFE_INVALID_TABLE_SPACING   4
#define XFE_INVALID_TABLE_PADDING   5
#define XFE_INVALID_TABLE_WIDTH     6
#define XFE_INVALID_TABLE_HEIGHT    7
#define XFE_INVALID_CELL_NROWS      8
#define XFE_INVALID_CELL_NCOLUMNS   9
#define XFE_INVALID_CELL_WIDTH      10
#define XFE_INVALID_CELL_HEIGHT     11

#define XFE_INVALID_CELL_NROWS      8
#define XFE_INVALID_CELL_NCOLUMNS   9
#define XFE_INVALID_CELL_WIDTH      10
#define XFE_INVALID_CELL_HEIGHT     11

#define SWATCH_SIZE 60
/* End of BWEEP! BWEEP! section */

#define RANGE_CHECK(o, a, b) ((o) < (a) || (o) > (b))

#include "EditTableDialog.h"

#include "xfe.h"
#include "xeditor.h"    // for fe_EditorPropertiesDialogType
#include "edt.h"
#include "felocale.h"    // for fe_GetTextField
#include <Xfe/Xfe.h>    // for XfeSubResourceGetXmStringValue etc.

#include <XmL/Folder.h>   /* tab folder stuff */
#include <Xm/Form.h>
#include <Xm/Frame.h>
#include <Xm/LabelG.h>
#include <Xm/PushBG.h>

// we will need this in the post() method.
extern "C" void fe_EventLoop();

extern "C" {

/* some forward definitions: */
static void fe_bg_group_set(fe_bgGroup* group, LO_Color* color,
                            char* bg_image, XP_Bool leave_image);
static void fe_bg_group_get(fe_bgGroup* group, LO_Color** color_r,
                     char** bg_image_r, XP_Bool* leave_image_r);

static Widget fe_CreateFolder(Widget parent, char* name,
                              Arg* args, Cardinal n);
static void fe_table_tbr_set(MWContext* context, Widget toggle,
                             Widget text, Widget radio, Boolean enabled,
                             unsigned numeric, Boolean second_one);
static void fe_create_background_group(Widget parent, char* name,
                                         Arg* p_args, Cardinal p_n,
                                         fe_bgGroup* group);
/* End Forward Definitions */

/* A routine that comes from Mail/News SearchRuleView.cpp, of all places: */
extern Widget fe_make_option_menu(Widget toplevel, Widget parent,
                                  char* widgetName, Widget *popup);
Widget fe_get_current_attribute_option(Widget parent);

/* Some defs of stuff in editordialogs.c */
extern char* fe_SimpleTableAlignment[];
extern char* fe_SimpleOptionHorizontalAlignment[];
extern char* fe_SimpleOptionVerticalAlignment[];
extern char* fe_SimpleOptionPixelPercent[];
extern char* fe_SimpleOptionAboveBelow[];

extern Widget fe_CreateSimpleRadioGroup(Widget parent, char* name,
                                        Arg* p_args, Cardinal p_n);
extern void fe_SimpleRadioGroupSetWhich(Widget widget, unsigned which);
extern int fe_SimpleRadioGroupGetWhich(Widget widget);
extern void fe_SimpleRadioGroupSetSensitive(Widget widget, Boolean sensitive);
extern Widget fe_SimpleRadioGroupGetChild(Widget widget, unsigned n);
extern unsigned fe_ED_Alignment_to_index(ED_Alignment type);
extern void fe_editor_range_error_dialog(MWContext* context, Widget parent,
                                         unsigned* errors, unsigned nerrors);
extern Widget fe_CreateFrame(Widget parent, char* name,
                             Arg* p_args, Cardinal p_n);
extern Dimension fe_get_offset_from_widest(Widget* children,
                                           Cardinal nchildren);
extern int fe_get_numeric_text_field(Widget widget);
extern void fe_set_numeric_text_field(Widget widget, unsigned value);
extern void fe_TextFieldSetEditable(MWContext* context, Widget, Boolean);
extern Widget fe_CreateSwatchButton(Widget parent, char* name,
                                    Arg* p_args, Cardinal p_n);
extern void fe_bg_group_use_color_cb(Widget widget, XtPointer, XtPointer);
extern void fe_bg_group_swatch_cb(Widget widget, XtPointer, XtPointer);
extern void fe_bg_use_image_cb(Widget widget, XtPointer closure, XtPointer);
extern void fe_bg_image_browse_cb(Widget widget, XtPointer closure, XtPointer);
extern void fe_NewSwatchSetColor(Widget widget, LO_Color* color);
extern LO_Color* fe_NewSwatchGetColor(Widget widget, LO_Color* color);
extern void fe_table_percent_label_set(Widget widget, Boolean nested);
/* End editordialogs.c definitions */
};  // end extern "C"

#define TABLE_DIALOG_TAB_TABLE 0
#define TABLE_DIALOG_TAB_CELL  1

/*
 * Extern routine called from EditorView.cpp
 */
void
fe_EditorTableCreateDialogDo(MWContext* context)
{
    XFE_EditTableDialog* dialog
        = new XFE_EditTableDialog(CONTEXT_WIDGET(context), "tableCreateDialog",
                                  context, TRUE, TABLE_DIALOG_TAB_TABLE);
    dialog->post();
}

/*
 * Extern routine called from editordialogs.c and EditorView.cpp
 */
void
fe_EditorTablePropertiesDialogDo(MWContext* context,
                                 fe_EditorPropertiesDialogType type)
{
	unsigned tab_number;

	if (type == XFE_EDITOR_PROPERTIES_TABLE_CELL)
		tab_number = TABLE_DIALOG_TAB_CELL;
	else
		tab_number = TABLE_DIALOG_TAB_TABLE;

    XFE_EditTableDialog* dialog = 
        new XFE_EditTableDialog(CONTEXT_WIDGET(context),
                                "tablePropertiesDialog",
                                context, FALSE, tab_number);
    dialog->post();
}

XFE_EditTableDialog::XFE_EditTableDialog(Widget parent, char *name,
                                         MWContext* context,
                                         Boolean createIt,
                                         int tab_number)
    : XFE_Dialog(parent, 
                 name, 
                 TRUE, // ok
                 TRUE, // cancel
                 FALSE, // help
                 TRUE, // apply
                 TRUE, // separator
                 TRUE // modal
                )
{
    int n;
    Arg args[1];
    Widget folder;
    Widget tab_form;

    m_context = context;
    /* If we're creating, insert a new minimal table */
    if (m_newTable = createIt)
    {
        EDT_TableData* tableData = EDT_NewTableData();
        tableData->iRows = tableData->iColumns = 1;
        tableData->bBorderWidthDefined = True;
        tableData->iBorderWidth = 1;
        EDT_InsertTable(context, tableData);
    }

	n = 0;
	folder = fe_CreateFolder(m_chrome, "folder", args, n);
    XtManageChild(folder);

	n = 0;
	tab_form = fe_CreateTabForm(folder, "tableTab", args, n);
    XtManageChild(tab_form);
	tablePropertiesCreate(tab_form);
	tablePropertiesInit();
	
	n = 0;
	tab_form = fe_CreateTabForm(folder, "cellTab", args, n);
    XtManageChild(tab_form);
	cellPropertiesCreate(tab_form);
	cellPropertiesInit();

	XmLFolderSetActiveTab(folder, tab_number, True);

    XtAddCallback(m_chrome, XmNokCallback, ok_cb, this);
    XtAddCallback(m_chrome, XmNapplyCallback, apply_cb, this);
    XtAddCallback(m_chrome, XmNcancelCallback, cancel_cb, this);
}

XFE_EditTableDialog::~XFE_EditTableDialog()
{
}

void
XFE_EditTableDialog::post()
{
    m_doneWithLoop = False;

    show();

    while(!m_doneWithLoop)
        fe_EventLoop();
}

void
XFE_EditTableDialog::ok()
{
    /*
     *    We can only handle one lot of errors at a time.
     *    This a little dumb, but it shouldn't happen often.
     */
    if (tablePropertiesValidate()) {
        tablePropertiesSet();
    }

    if (cellPropertiesValidate()) {
        cellPropertiesSet();
    }

    if (m_doneWithLoop)
        hide();
}

void
XFE_EditTableDialog::cancel()
{
    m_doneWithLoop = True;

    // If the table is new, we need to delete the one that was created;
    // if it was already there, then don't do anything.
    if (m_newTable)
    {
        EDT_DeleteTable(m_context);
    }

    hide();
}

void
XFE_EditTableDialog::table_toggle_cb(Widget widget, XtPointer cd, XtPointer)
{
    XFE_EditTableDialog* edt = (XFE_EditTableDialog*)cd;
	Boolean enabled = XmToggleButtonGetState(widget);
	MWContext* context = fe_WidgetToMWContext(widget);
	Widget  text = NULL; /* keep -O happy */
	Widget  radio = NULL; /* keep -O happy */

	if (widget == edt->m_table.line_width_toggle) {
		text = edt->m_table.line_width_text;
		radio = NULL;
	} else if (widget == edt->m_table.width_toggle) {
		text = edt->m_table.width_text;
		radio = edt->m_table.width_units;
	} else if (widget == edt->m_table.height_toggle) {
		text = edt->m_table.height_text;
		radio = edt->m_table.height_units;
	} else if (widget == edt->m_table.caption_toggle) {
		fe_SimpleRadioGroupSetSensitive(edt->m_table.caption_type, enabled);
		return;
	}
    
	if (text != NULL)
		fe_TextFieldSetEditable(context, text, enabled);
	if (radio != NULL)
		fe_SimpleRadioGroupSetSensitive(radio, enabled);
}

void
XFE_EditTableDialog::cell_toggle_cb(Widget widget,
                                    XtPointer clientData, XtPointer)
{
    XFE_EditTableDialog* edt = (XFE_EditTableDialog*)clientData;
	fe_EditorTablesCellStruct* w_data = &(edt->m_cell);
	Boolean enabled = XmToggleButtonGetState(widget);
	MWContext* context = fe_WidgetToMWContext(widget);
	Widget  text = NULL;
	Widget  radio = NULL;

	if (widget == w_data->width_toggle) {
		text = w_data->width_text;
		radio = w_data->width_units;
	} else if (widget == w_data->height_toggle) {
		text = w_data->height_text;
		radio = w_data->height_units;
	}

	if (text != NULL)
		fe_TextFieldSetEditable(context, text, enabled);
	if (radio != NULL)
		fe_SimpleRadioGroupSetSensitive(radio, enabled);
}

void
XFE_EditTableDialog::changeSelection(Boolean nextP)
{
    ED_MoveSelType moveType;
    if (nextP)
        moveType = ED_MOVE_NEXT;
    else
        moveType = ED_MOVE_PREV;
    EDT_TableCellData* cellData = EDT_GetTableCellData(m_context);
    Widget which = fe_get_current_attribute_option(m_cell.option_menu);
    if (!strncmp(XtName(which), "cell", 4))
    {
        EDT_ChangeTableSelection(m_context, ED_HIT_SEL_CELL,
                                 moveType, cellData);
    }
    else if (!strncmp(XtName(which), "row", 3))
    {
        EDT_ChangeTableSelection(m_context, ED_HIT_SEL_ROW,
                                 moveType, cellData);
    }
    else if (!strncmp(XtName(which), "col", 3))
    {
        EDT_ChangeTableSelection(m_context, ED_HIT_SEL_COL,
                                 moveType, cellData);
    }
}

void
XFE_EditTableDialog::cell_selection_cb(Widget w, XtPointer clientData,
                                       XtPointer)
{
    XFE_EditTableDialog* edt = (XFE_EditTableDialog*)clientData;
    edt->changeSelection(strncmp(XtName(w), "next", 4) == 0);
}

void 
XFE_EditTableDialog::ok_cb(Widget, XtPointer clientData, XtPointer)
{
    XFE_EditTableDialog *obj = (XFE_EditTableDialog*)clientData;

    obj->m_doneWithLoop = True;
    obj->ok();
}

void 
XFE_EditTableDialog::apply_cb(Widget, XtPointer clientData, XtPointer)
{
    XFE_EditTableDialog *obj = (XFE_EditTableDialog*)clientData;

    obj->m_doneWithLoop = False;
    obj->ok();
}

void
XFE_EditTableDialog::cancel_cb(Widget, XtPointer clientData, XtPointer)
{
    XFE_EditTableDialog *obj = (XFE_EditTableDialog*)clientData;

    obj->cancel();
}

static ED_Alignment
fe_index_to_ED_Alignment(unsigned index, Boolean horizontal)
{
  switch (index) {
  case 1: return (horizontal)? ED_ALIGN_LEFT: ED_ALIGN_ABSTOP;
  case 2: return ED_ALIGN_ABSCENTER;
  case 3: return (horizontal)? ED_ALIGN_RIGHT: ED_ALIGN_ABSBOTTOM;
  case 4: if (!horizontal) return ED_ALIGN_BASELINE; /*FALLTHRU*/
  case 0:
  default: return ED_ALIGN_DEFAULT;
  }
}

static Widget
fe_create_table_text_alignment_group(Widget parent, char* name,
									 Arg* p_args, Cardinal p_n,
									 Widget* h, Widget* v)
{
	Widget frame;
	Widget rc;
	Widget sub_rc;
	Widget horizontal_label;
	Widget horizontal_alignment;
	Widget vertical_label;
	Widget vertical_alignment;
	Arg args[16];
	Cardinal n;
	Widget children[8];
	Cardinal nchildren;

	frame = fe_CreateFrame(parent, name, p_args, p_n);
	XtManageChild(frame);

	n = 0;
	XtSetArg(args[n], XmNorientation, XmHORIZONTAL); n++;
	rc = XmCreateRowColumn(frame, "textAlignment", args, n);
	XtManageChild(rc);

	n = 0;
	XtSetArg(args[n], XmNorientation, XmVERTICAL); n++;
	sub_rc = XmCreateRowColumn(rc, "horizontal", args, n);
	XtManageChild(sub_rc);

	nchildren = 0;
	n = 0;
	horizontal_label = XmCreateLabelGadget(sub_rc, "horizontalLabel", args, n);
	children[nchildren++] = horizontal_label;

	n = 0;
	XtSetArg(args[n], XmNentryAlignment, XmALIGNMENT_BEGINNING); n++;
	XtSetArg(args[n], XmNpacking, XmPACK_COLUMN); n++;
	XtSetArg(args[n], XmNorientation, XmVERTICAL); n++;
	XtSetArg(args[n], XmNbuttons, fe_SimpleOptionHorizontalAlignment); n++;
	horizontal_alignment = fe_CreateSimpleRadioGroup(sub_rc,
											 "horizontalAlignment", args, n);
	children[nchildren++] = horizontal_alignment;

	XtManageChildren(children, nchildren);

	n = 0;
	XtSetArg(args[n], XmNorientation, XmVERTICAL); n++;
	sub_rc = XmCreateRowColumn(rc, "vertical", args, n);
	XtManageChild(sub_rc);

	nchildren = 0;
	n = 0;
	vertical_label = XmCreateLabelGadget(sub_rc, "verticalLabel", args, n);
	children[nchildren++] = vertical_label;

	n = 0;
	XtSetArg(args[n], XmNentryAlignment, XmALIGNMENT_BEGINNING); n++;
	XtSetArg(args[n], XmNpacking, XmPACK_COLUMN); n++;
	XtSetArg(args[n], XmNorientation, XmVERTICAL); n++;
	XtSetArg(args[n], XmNbuttons, fe_SimpleOptionVerticalAlignment); n++;
	vertical_alignment = fe_CreateSimpleRadioGroup(sub_rc,
											 "verticalAlignment", args, n);
	children[nchildren++] = vertical_alignment;

	XtManageChildren(children, nchildren);

	*v = vertical_alignment;
	*h = horizontal_alignment;

	return frame;
}

Widget
XFE_EditTableDialog::cellPropertiesCreate(Widget parent)
{
	Widget form;
	Widget rows_label;
	Widget rows_text;
	Widget columns_label;
	Widget columns_text;
	Widget columns_units;
	Widget horizontal_alignment;
	Widget vertical_alignment;
	Widget align_frame;
	Widget other_frame;
	Widget rc;
	Widget header_style;
	Widget wrap_text;
	Widget width_toggle;
	Widget width_text;
	Widget width_units;
	Widget height_toggle;
	Widget height_text;
	Widget height_units;
    Widget w;
    Widget prevnext_form;
	Arg args[16];
	Cardinal n;

	form = parent;

	n = 0;
    // Why doesn't the shadow type default correctly?
	XtSetArg(args[n], XmNshadowType, XmSHADOW_ETCHED_IN); n++;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	other_frame = XmCreateFrame(form, "_frame", args, n);
	XtManageChild(other_frame);

    n = 0;
	XtSetArg(args[n], XmNchildType, XmFRAME_TITLE_CHILD); n++;
    w = XmCreateLabelGadget(other_frame, "changeSelection", args, n);
    XtManageChild(w);

	n = 0;
	XtSetArg(args[n], XmNfractionBase, 4); n++;
	prevnext_form = XmCreateForm(other_frame, "_form", args, n);
	XtManageChild(prevnext_form);

    Widget pulldownParent, option_menu;
    option_menu = fe_make_option_menu(FE_GetToplevelWidget(),
                                      prevnext_form, "searchTypeOpt",
                                      &pulldownParent);
    XtVaSetValues(option_menu,
                  XmNtopAttachment, XmATTACH_FORM,
                  XmNleftAttachment, XmATTACH_FORM,
                  XmNbottomAttachment, XmATTACH_FORM,
                  0);
    w = XmCreatePushButtonGadget(pulldownParent, "cellSelect", NULL, 0);
    XtManageChild(w);
    w = XmCreatePushButtonGadget(pulldownParent, "rowSelect", NULL, 0);
    XtManageChild(w);
    w = XmCreatePushButtonGadget(pulldownParent, "columnSelect", NULL, 0);
    XtManageChild(w);
    XtManageChild(option_menu);

    // Set the menu appropriately:
    //XtVaSetValues(m_opt_valueField, XmNsubMenuId, popup, 0);

    n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	XtSetArg(args[n], XmNleftPosition, 1); n++;
	XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
    w = XmCreatePushButtonGadget(prevnext_form, "previous", args, n);
    XtManageChild(w);
	XtAddCallback(w, XmNactivateCallback, 
				  cell_selection_cb, (XtPointer)this);

    n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNleftWidget, w); n++;
	XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
    w = XmCreatePushButtonGadget(prevnext_form, "next", args, n);
    XtManageChild(w);
	XtAddCallback(w, XmNactivateCallback, 
				  cell_selection_cb, (XtPointer)this);

	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, other_frame); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	rows_label = XmCreateLabelGadget(form, "cellRowsLabel", args, n);
    XtManageChild(rows_label);

	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, other_frame); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNleftWidget, rows_label); n++;
	XtSetArg(args[n], XmNcolumns, 4); n++;
	rows_text = fe_CreateTextField(form, "cellRowsText", args, n);
    XtManageChild(rows_text);

	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, other_frame); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNleftWidget, rows_text); n++;
	columns_label = XmCreateLabelGadget(form, "cellColumnsLabel", args, n);
    XtManageChild(columns_label);

	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, other_frame); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNleftWidget, columns_label); n++;
	XtSetArg(args[n], XmNcolumns, 4); n++;
	columns_text = fe_CreateTextField(form, "cellColumnsText", args, n);
    XtManageChild(columns_text);

	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, other_frame); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNleftWidget, columns_text); n++;
	columns_units = XmCreateLabelGadget(form, "cellColumnsUnits", args, n);
    XtManageChild(columns_units);

	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, rows_text); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	align_frame = fe_create_table_text_alignment_group(form, "textAlignment",
													  args, n,
													  &horizontal_alignment,
													  &vertical_alignment);
    XtManageChild(align_frame);

	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, rows_text); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNleftWidget, align_frame); n++;
	XtSetArg(args[n], XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET); n++;
	XtSetArg(args[n], XmNbottomWidget, align_frame); n++;
	other_frame = fe_CreateFrame(form, "textOther", args, n);
    XtManageChild(other_frame);

	n = 0;
	XtSetArg(args[n], XmNorientation, XmVERTICAL); n++;
	rc = XmCreateRowColumn(other_frame, "textOther", args, n);
	XtManageChild(rc);

	n = 0;
	header_style = XmCreateToggleButtonGadget(rc, "headerStyle", args, n);
	XtManageChild(header_style);

	n = 0;
	wrap_text = XmCreateToggleButtonGadget(rc, "nonBreaking", args, n);
	XtManageChild(wrap_text);

	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, align_frame); n++;
	XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNentryAlignment, XmALIGNMENT_BEGINNING); n++;
	XtSetArg(args[n], XmNpacking, XmPACK_TIGHT); n++;
	XtSetArg(args[n], XmNorientation, XmHORIZONTAL); n++;
	XtSetArg(args[n], XmNbuttons, fe_SimpleOptionPixelPercent); n++;
	width_units = fe_CreateSimpleRadioGroup(form, "widthUnits", args, n);
	XtManageChild(width_units);

	n = 0;
	XtSetArg(args[n], XmNcolumns, 4); n++;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, align_frame); n++;
	XtSetArg(args[n], XmNrightAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNrightWidget, width_units); n++;
	width_text = fe_CreateTextField(form, "cellWidthText", args, n);
	XtManageChild(width_text);

	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, align_frame); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNrightAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNrightWidget, width_text); n++;
	width_toggle = XmCreateToggleButtonGadget(form, "cellWidthToggle",
                                              args, n);
	XtManageChild(width_toggle);

	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, width_text); n++;
	XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET); n++;
	XtSetArg(args[n], XmNleftWidget, width_units); n++;
	XtSetArg(args[n], XmNentryAlignment, XmALIGNMENT_BEGINNING); n++;
	XtSetArg(args[n], XmNpacking, XmPACK_TIGHT); n++;
	XtSetArg(args[n], XmNorientation, XmHORIZONTAL); n++;
	XtSetArg(args[n], XmNbuttons, fe_SimpleOptionPixelPercent); n++;
	height_units = fe_CreateSimpleRadioGroup(form, "heightUnits", args, n);
	XtManageChild(height_units);

	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, width_text); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET); n++;
	XtSetArg(args[n], XmNleftWidget, width_text); n++;
	XtSetArg(args[n], XmNrightAttachment, XmATTACH_OPPOSITE_WIDGET); n++;
	XtSetArg(args[n], XmNrightWidget, width_text); n++;
	height_text = fe_CreateTextField(form, "cellHeightText", args, n);
	XtManageChild(height_text);

	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, width_text); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNrightAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNrightWidget, height_text); n++;
	height_toggle = XmCreateToggleButtonGadget(form, "cellHeightToggle",
                                               args, n);
	XtManageChild(height_toggle);

	XtAddCallback(width_toggle, XmNvalueChangedCallback, 
				  cell_toggle_cb, (XtPointer)this);
	XtAddCallback(height_toggle, XmNvalueChangedCallback, 
				  cell_toggle_cb, (XtPointer)this);

	/*
	 *    Background..
	 */
	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, height_text); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	fe_create_background_group(form, "background", args, n,
                               &(m_cell.bg_group));

	m_cell.number_rows_text = rows_text;
	m_cell.number_columns_text = columns_text;
	m_cell.horizontal_alignment = horizontal_alignment;
	m_cell.vertical_alignment = vertical_alignment;
	m_cell.header_style = header_style;
	m_cell.wrap_text = wrap_text;
	m_cell.width_toggle = width_toggle;
	m_cell.width_text = width_text;
	m_cell.width_units = width_units;
	m_cell.height_toggle = height_toggle;
	m_cell.height_text = height_text;
	m_cell.height_units = height_units;
    m_cell.option_menu = option_menu;

	return form;
}

void
XFE_EditTableDialog::cellPropertiesInit()
{
	Boolean is_nested = EDT_IsInsertPointInNestedTable(m_context);
    EDT_TableCellData* cell_data = EDT_GetTableCellData(m_context);
	Boolean enabled = TRUE;
	Widget  widget;

	if (!cell_data) {
		cell_data = EDT_NewTableCellData();
		enabled = FALSE;
	}

	fe_set_numeric_text_field(m_cell.number_rows_text,
							  cell_data->iRowSpan);
	fe_TextFieldSetEditable(m_context, m_cell.number_rows_text, enabled);
	fe_set_numeric_text_field(m_cell.number_columns_text,
							  cell_data->iColSpan);
	fe_TextFieldSetEditable(m_context, m_cell.number_columns_text, enabled);

	fe_SimpleRadioGroupSetWhich(m_cell.horizontal_alignment,
								fe_ED_Alignment_to_index(cell_data->align));
	fe_SimpleRadioGroupSetSensitive(m_cell.horizontal_alignment, enabled);

	fe_SimpleRadioGroupSetWhich(m_cell.vertical_alignment,
								fe_ED_Alignment_to_index(cell_data->valign));
	fe_SimpleRadioGroupSetSensitive(m_cell.vertical_alignment, enabled);

	XmToggleButtonGadgetSetState(m_cell.header_style, cell_data->bHeader,
								 FALSE);
	XtVaSetValues(m_cell.header_style, XmNsensitive, enabled, 0);
	XmToggleButtonGadgetSetState(m_cell.wrap_text, cell_data->bNoWrap, FALSE);
	XtVaSetValues(m_cell.wrap_text, XmNsensitive, enabled, 0);

	fe_table_tbr_set(m_context, m_cell.width_toggle,
					 m_cell.width_text, m_cell.width_units,
					 cell_data->bWidthDefined,
					 cell_data->iWidth,
					 (cell_data->bWidthPercent));
	XtVaSetValues(m_cell.width_toggle, XmNsensitive, enabled, 0);

	widget = fe_SimpleRadioGroupGetChild(m_cell.width_units, 1);
	fe_table_percent_label_set(widget, is_nested);

	fe_table_tbr_set(m_context, m_cell.height_toggle,
					 m_cell.height_text, m_cell.height_units,
					 cell_data->bHeightDefined,
					 cell_data->iHeight,
					 (cell_data->bHeightPercent));
	XtVaSetValues(m_cell.height_toggle, XmNsensitive, enabled, 0);

	widget = fe_SimpleRadioGroupGetChild(m_cell.height_units, 1);
	fe_table_percent_label_set(widget, is_nested);
	
	/*
	 *    Background stuff
	 */
	fe_bg_group_set(&(m_cell.bg_group),
					cell_data->pColorBackground,
					cell_data->pBackgroundImage,
					cell_data->bBackgroundNoSave);

	EDT_FreeTableCellData(cell_data);
}

void
XFE_EditTableDialog::cellPropertiesSetValidateCommon(EDT_TableCellData* cell_data)
{
	unsigned index;

	cell_data->iRowSpan = 
		fe_get_numeric_text_field(m_cell.number_rows_text);
	cell_data->iColSpan = 
		fe_get_numeric_text_field(m_cell.number_columns_text);
	 
	index = fe_SimpleRadioGroupGetWhich(m_cell.horizontal_alignment);
	cell_data->align = fe_index_to_ED_Alignment(index, TRUE);

	index = fe_SimpleRadioGroupGetWhich(m_cell.vertical_alignment);
	cell_data->valign = fe_index_to_ED_Alignment(index, FALSE);

	cell_data->bHeader = XmToggleButtonGadgetGetState(m_cell.header_style);
	cell_data->bNoWrap = XmToggleButtonGadgetGetState(m_cell.wrap_text);

	cell_data->bWidthDefined = XmToggleButtonGetState(m_cell.width_toggle);
	cell_data->iWidth = fe_get_numeric_text_field(m_cell.width_text);
	cell_data->bWidthPercent =
	  (fe_SimpleRadioGroupGetWhich(m_cell.width_units) == 1);

	cell_data->bHeightDefined =
		XmToggleButtonGetState(m_cell.height_toggle);
	cell_data->iHeight =
		fe_get_numeric_text_field(m_cell.height_text);
	cell_data->bHeightPercent =
		(fe_SimpleRadioGroupGetWhich(m_cell.height_units) == 1);

}

Boolean
XFE_EditTableDialog::cellPropertiesValidate()
{
	EDT_TableCellData cell_data;
	EDT_TableCellData* c = &cell_data;
	unsigned         errors[16];
	unsigned         nerrors = 0;
	
	memset(&cell_data, 0, sizeof(EDT_TableCellData));

	cellPropertiesSetValidateCommon(&cell_data);

	if (RANGE_CHECK(c->iRowSpan, TABLE_MIN_ROWS, TABLE_MAX_ROWS))
		errors[nerrors++] = XFE_INVALID_CELL_NROWS;
	if (RANGE_CHECK(c->iColSpan, TABLE_MIN_COLUMNS, TABLE_MAX_COLUMNS))
		errors[nerrors++] = XFE_INVALID_CELL_NCOLUMNS;

	if (c->bWidthDefined) {
		if (c->bWidthPercent) {
			if (RANGE_CHECK(c->iWidth,
						TABLE_MIN_PERCENT_WIDTH, TABLE_MAX_PERCENT_WIDTH))
				errors[nerrors++] = XFE_INVALID_CELL_WIDTH;
		} else {
			if (RANGE_CHECK(c->iWidth,
							TABLE_MIN_PIXEL_WIDTH, TABLE_MAX_PIXEL_WIDTH))
				errors[nerrors++] = XFE_INVALID_CELL_WIDTH;
		}
	}
		
	if (c->bHeightDefined) {
		if (c->bHeightPercent) {
			if (RANGE_CHECK(c->iHeight,
						TABLE_MIN_PERCENT_HEIGHT, TABLE_MAX_PERCENT_HEIGHT))
				errors[nerrors++] = XFE_INVALID_CELL_HEIGHT;
		} else {
			if (RANGE_CHECK(c->iHeight,
							TABLE_MIN_PIXEL_HEIGHT, TABLE_MAX_PIXEL_HEIGHT))
				errors[nerrors++] = XFE_INVALID_CELL_HEIGHT;
		}
	}

	if (nerrors > 0) {
		fe_editor_range_error_dialog(m_context, m_cell.number_rows_text,
									 errors, nerrors);
		return FALSE;
	}

	return TRUE;
}

void
XFE_EditTableDialog::cellPropertiesSet()
{
    EDT_TableCellData cell_data;

	memset(&cell_data, 0, sizeof(EDT_TableCellData));

	cellPropertiesSetValidateCommon(&cell_data);

	/*
	 *    Background stuff
	 */
	fe_bg_group_get(&(m_cell.bg_group),
					&cell_data.pColorBackground,
					&cell_data.pBackgroundImage,
					&cell_data.bBackgroundNoSave);

#ifdef DEBUG_akkana
    printf("Setting color: %x %x %x\n", cell_data.pColorBackground->red,
           cell_data.pColorBackground->green,
           cell_data.pColorBackground->blue);
#endif
    fe_EditorTableCellSetData(m_context, &cell_data);

	if (cell_data.pColorBackground)
		XP_FREE(cell_data.pColorBackground);
	if (cell_data.pBackgroundImage)
		XP_FREE(cell_data.pBackgroundImage);
}


/* Create the table properties tab in the table dialog */
Widget
XFE_EditTableDialog::tablePropertiesCreate(Widget parent)
{
	Widget frame;
	Widget form;
	Widget line_width_toggle;
	Widget line_width_text;
	Widget line_width_units;
	Widget spacing_label;
	Widget spacing_text;
	Widget spacing_units;
	Widget padding_label;
	Widget padding_text;
	Widget padding_units;
	Widget width_toggle;
	Widget width_text;
	Widget width_units;
	Widget height_toggle;
	Widget height_text;
	Widget height_units;
	Widget caption_toggle;
	Widget caption_type;
	Widget alignframe;
	Widget alignBox;
    Widget w;
	Arg args[16];
	Cardinal n;

	/*
	 *    Number of rows and columns.
	 */
	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	frame = fe_CreateFrame(parent, "tableRowsColumns", args, n);
    XtManageChild(frame);

	n = 0;
    XtSetArg(args[n], XmNfractionBase, 2); n++;
	form = XmCreateForm(frame, "_form", args, n);
	XtManageChild(form);

	n = 0;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	w = XmCreateLabelGadget(form, "tableRowsLabel", args, n);
    XtManageChild(w);

	n = 0;
	XtSetArg(args[n], XmNcolumns, 6); n++;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNleftWidget, w); n++;
	XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	m_table.number_rows_text = XmCreateTextField(form, "numRows", args, n);
    XtManageChild(m_table.number_rows_text);

	n = 0;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	XtSetArg(args[n], XmNleftPosition, 1); n++;
	XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	w = XmCreateLabelGadget(form, "tableColumnsLabel", args, n);
    XtManageChild(w);

	n = 0;
	XtSetArg(args[n], XmNcolumns, 6); n++;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNleftWidget, w); n++;
    m_table.number_columns_text = XmCreateTextField(form, "numColumns",
                                                    args, n);
    XtManageChild(m_table.number_columns_text);

	/*
	 *    Alignment frame.
	 */
	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, frame); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	alignframe = fe_CreateFrame(parent, "tableAlignment", args, n);
    XtManageChild(alignframe);

	n = 0;
	XtSetArg(args[n], XmNbuttons, fe_SimpleTableAlignment); n++;
	XtSetArg(args[n], XmNorientation, XmHORIZONTAL); n++;
	alignBox = fe_CreateSimpleRadioGroup(alignframe, "tableAlignmentBox", 
										 args, n);
	XtManageChild(alignBox);
	fe_SimpleRadioGroupSetWhich(alignBox, 0);

	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, alignframe); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	frame = fe_CreateFrame(parent, "attributes", args, n);
    XtManageChild(frame);

	n = 0;
	form = XmCreateForm(frame, "attributes", args, n);
	XtManageChild(form);

	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;	
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	caption_toggle = XmCreateToggleButtonGadget(form, "captionToggle",
												args, n);
	XtManageChild(caption_toggle);
	XtAddCallback(caption_toggle, XmNvalueChangedCallback, 
				  table_toggle_cb, (XtPointer)this);

	n = 0;
	XtSetArg(args[n], XmNmarginWidth, 0); n++;
	XtSetArg(args[n], XmNmarginHeight, 0); n++;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNleftWidget, caption_toggle); n++;
	XtSetArg(args[n], XmNbuttons, fe_SimpleOptionAboveBelow); n++;
#ifdef TABLES_USE_OPTION_MENU	
	caption_type = fe_CreateSimpleOptionMenu(form, "captionType", args, n);
#else
	XtSetArg(args[n], XmNpacking, XmPACK_TIGHT); n++;
	XtSetArg(args[n], XmNorientation, XmHORIZONTAL); n++;
	caption_type = fe_CreateSimpleRadioGroup(form, "captionType", args, n);
#endif
	XtManageChild(caption_type);
	
	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, caption_type); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	line_width_toggle = XmCreateToggleButtonGadget(form, "borderLineWidth",
												   args, n);
	XtManageChild(line_width_toggle);
	XtAddCallback(line_width_toggle, XmNvalueChangedCallback, 
				  table_toggle_cb, (XtPointer)this);

	n = 0;
	XtSetArg(args[n], XmNcolumns, 4); n++;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, caption_type); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNleftWidget, line_width_toggle); n++;
	line_width_text = fe_CreateTextField(form, "borderLineWidthText",
										  args, n);
	XtManageChild(line_width_text);
	
	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, caption_type); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNleftWidget, line_width_text); n++;
	line_width_units = XmCreateLabelGadget(form, "borderLineWidthUnits",
										   args, n);
	XtManageChild(line_width_units);

	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, line_width_text); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	spacing_label = XmCreateLabelGadget(form, "cellSpacingLabel", args, n);
	XtManageChild(spacing_label);

	n = 0;
	XtSetArg(args[n], XmNcolumns, 4); n++;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, line_width_text); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET); n++;
	XtSetArg(args[n], XmNleftWidget, line_width_text); n++;
	spacing_text = fe_CreateTextField(form, "cellSpacingText", args, n);
	XtManageChild(spacing_text);
	
	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, line_width_text); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNleftWidget, line_width_text); n++;
	spacing_units = XmCreateLabelGadget(form, "cellSpacingUnits", args, n);
	XtManageChild(spacing_units);

	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, spacing_text); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	padding_label = XmCreateLabelGadget(form, "cellPaddingLabel", args, n);
	XtManageChild(padding_label);

	n = 0;
	XtSetArg(args[n], XmNcolumns, 4); n++;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, spacing_text); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET); n++;
	XtSetArg(args[n], XmNleftWidget, line_width_text); n++;
	padding_text = fe_CreateTextField(form, "cellPaddingText", args, n);
	XtManageChild(padding_text);
	
	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, spacing_text); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNleftWidget, line_width_text); n++;
	padding_units = XmCreateLabelGadget(form, "cellPaddingUnits", args, n);
	XtManageChild(padding_units);

	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, padding_text); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	width_toggle = XmCreateToggleButtonGadget(form, "tableWidthToggle",
											  args, n);
	XtManageChild(width_toggle);
	XtAddCallback(width_toggle, XmNvalueChangedCallback, 
				  table_toggle_cb, (XtPointer)this);

	n = 0;
	XtSetArg(args[n], XmNcolumns, 4); n++;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, padding_text); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET); n++;
	XtSetArg(args[n], XmNleftWidget, line_width_text); n++;
	width_text = fe_CreateTextField(form, "tableWidthText", args, n);
	XtManageChild(width_text);
	
	n = 0;
	XtSetArg(args[n], XmNbuttons, fe_SimpleOptionPixelPercent); n++;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, padding_text); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNleftWidget, width_text); n++;
#ifdef TABLES_USE_OPTION_MENU	
	width_units = fe_CreateSimpleOptionMenu(form, "tableWidthUnits", args, n);
#else
	XtSetArg(args[n], XmNpacking, XmPACK_TIGHT); n++;
	XtSetArg(args[n], XmNorientation, XmHORIZONTAL); n++;
	width_units = fe_CreateSimpleRadioGroup(form, "tableWidthUnits", args, n);
#endif
	XtManageChild(width_units);

	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, width_text); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	height_toggle = XmCreateToggleButtonGadget(form, "tableHeightToggle",
											   args, n);
	XtManageChild(height_toggle);
    XtAddCallback(height_toggle, XmNvalueChangedCallback, 
                  table_toggle_cb, (XtPointer)this);

	n = 0;
	XtSetArg(args[n], XmNcolumns, 4); n++;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, width_text); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET); n++;
	XtSetArg(args[n], XmNleftWidget, line_width_text); n++;
	height_text = fe_CreateTextField(form, "tableHeightText", args, n);
	XtManageChild(height_text);

	n = 0;
	XtSetArg(args[n], XmNbuttons, fe_SimpleOptionPixelPercent); n++;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, width_text); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNleftWidget, width_text); n++;
#ifdef TABLES_USE_OPTION_MENU	
	height_units = fe_CreateSimpleOptionMenu(form, "tableHeightUnits",args, n);
#else
	XtSetArg(args[n], XmNpacking, XmPACK_TIGHT); n++;
	XtSetArg(args[n], XmNorientation, XmHORIZONTAL); n++;
	height_units = fe_CreateSimpleRadioGroup(form, "tableHeightUnits",args, n);
#endif
	XtManageChild(height_units);

	n = 0;
	XtSetArg(args[n], XmNsensitive, FALSE); n++;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, height_text); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	w = XmCreatePushButtonGadget(form, "extraHtml", args, n);
	XtManageChild(w);

	/*
	 *    Background..
	 */
	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, frame); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	fe_create_background_group(parent, "background", args, n,
                               &(m_table.bg_group));

    m_table.line_width_toggle = line_width_toggle;
    m_table.line_width_text = line_width_text;
    m_table.spacing_text = spacing_text;
    m_table.padding_text = padding_text;
    m_table.width_toggle = width_toggle;
    m_table.width_text = width_text;
    m_table.width_units = width_units;
    m_table.height_toggle = height_toggle;
    m_table.height_text = height_text;
    m_table.height_units = height_units;
    m_table.caption_toggle = caption_toggle;
    m_table.caption_type = caption_type;
    m_table.alignBox = alignBox;

	return alignframe;
}

void
XFE_EditTableDialog::tablePropertiesInit()
{
	Widget  widget;
	EDT_AllTableData table_data;
	Boolean is_nested = False;

	memset(&table_data, 0, sizeof(EDT_AllTableData));

	if (!fe_EditorTableGetData(m_context, &table_data))
		return;

    /* Set the number of rows and columns */
    if (m_table.number_rows_text != 0)
    {
        fe_set_numeric_text_field(m_table.number_rows_text,
                                  table_data.td.iRows);
        if (m_table.number_rows_text != NULL) /* inserting */
            is_nested = EDT_IsInsertPointInTable(m_context);
        else
            is_nested = EDT_IsInsertPointInNestedTable(m_context);
    }
    if (m_table.number_columns_text != 0)
        fe_set_numeric_text_field(m_table.number_columns_text,
                                  table_data.td.iColumns);

	/* set the border width parts */
	fe_table_tbr_set(m_context, m_table.line_width_toggle,
					 m_table.line_width_text, NULL,
					 table_data.td.bBorderWidthDefined,
					 table_data.td.iBorderWidth,
					 0);
	fe_set_numeric_text_field(m_table.spacing_text,
							  table_data.td.iCellSpacing);
	fe_set_numeric_text_field(m_table.padding_text,
							  table_data.td.iCellPadding);

	fe_table_tbr_set(m_context, m_table.width_toggle,
					 m_table.width_text, m_table.width_units,
					 table_data.td.bWidthDefined,
					 table_data.td.iWidth,
					 (table_data.td.bWidthPercent));

	widget = fe_SimpleRadioGroupGetChild(m_table.width_units, 1);
	fe_table_percent_label_set(widget, is_nested);
	
	fe_table_tbr_set(m_context, m_table.height_toggle,
					 m_table.height_text, m_table.height_units,
					 table_data.td.bHeightDefined,
					 table_data.td.iHeight,
					 (table_data.td.bHeightPercent));
	widget = fe_SimpleRadioGroupGetChild(m_table.height_units, 1);
	fe_table_percent_label_set(widget, is_nested);

	/*
	 *    Background stuff
	 */
	fe_bg_group_set(&(m_table.bg_group),
					table_data.td.pColorBackground,
					table_data.td.pBackgroundImage,
					table_data.td.bBackgroundNoSave);

	fe_table_tbr_set(m_context, m_table.caption_toggle,
					 NULL, m_table.caption_type,
					 table_data.has_caption,
					 0,
					 (table_data.cd.align != ED_ALIGN_ABSTOP));

	/* we don't have default for alignment yet, so we need to subtract one
	 * from index 
  	 */
	fe_SimpleRadioGroupSetWhich(m_table.alignBox,
								fe_ED_Alignment_to_index(table_data.td.align)-1);
}

void
XFE_EditTableDialog::tablePropertiesCommonSet(EDT_AllTableData* table_data)
{
	int alignment;

	/*FIXME*/
	/*
	 *    Don't know what this slot is, WinFE doesn't use it??
	 */
	table_data->td.malign = ED_ALIGN_DEFAULT;

    table_data->td.iRows = 
		fe_get_numeric_text_field(m_table.number_rows_text);
    table_data->td.iColumns = 
		fe_get_numeric_text_field(m_table.number_columns_text);

	table_data->td.bBorderWidthDefined =
		XmToggleButtonGetState(m_table.line_width_toggle);
	table_data->td.iBorderWidth = 
		fe_get_numeric_text_field(m_table.line_width_text);

	table_data->td.iCellSpacing = 
		fe_get_numeric_text_field(m_table.spacing_text);
	table_data->td.iCellPadding = 
		fe_get_numeric_text_field(m_table.padding_text);

	table_data->td.bWidthDefined =
		XmToggleButtonGetState(m_table.width_toggle);
	table_data->td.iWidth =
		fe_get_numeric_text_field(m_table.width_text);
	table_data->td.bWidthPercent =
		(fe_SimpleRadioGroupGetWhich(m_table.width_units) == 1);

	table_data->td.bHeightDefined =
		XmToggleButtonGetState(m_table.height_toggle);
	table_data->td.iHeight =
		fe_get_numeric_text_field(m_table.height_text);
	table_data->td.bHeightPercent =
		(fe_SimpleRadioGroupGetWhich(m_table.height_units) == 1);

	alignment = fe_SimpleRadioGroupGetWhich(m_table.alignBox);

	switch(alignment) {
		case 0:
			table_data->td.align = ED_ALIGN_LEFT;
			break;
		case 1:
			table_data->td.align = ED_ALIGN_ABSCENTER;
			break;
		case 2:
			table_data->td.align = ED_ALIGN_RIGHT;
			break;
		default:
			XP_ASSERT(0);
			break;
	}

	/*
	 *    Don't get the background stuff here, because none of it gets
	 *    validated, and we don't want to bother when we validate.
	 */

	table_data->has_caption = XmToggleButtonGetState(m_table.caption_toggle);

	if (fe_SimpleRadioGroupGetWhich(m_table.caption_type) == 1)
		table_data->cd.align = ED_ALIGN_ABSBOTTOM;
	else
		table_data->cd.align = ED_ALIGN_ABSTOP;
}

void
XFE_EditTableDialog::tablePropertiesSet()
{
	EDT_AllTableData table_data;
	EDT_TableData*   tmp = EDT_NewTableData();

	memset(&table_data, 0, sizeof(EDT_AllTableData));
	memcpy(&table_data.td, tmp, sizeof(EDT_TableData));

	if (!fe_EditorTableGetData(m_context, &table_data))
		return;

	tablePropertiesCommonSet(&table_data);

	/* Get the background stuff, because ..common_set() did not. */
	fe_bg_group_get(&(m_table.bg_group),
					&table_data.td.pColorBackground,
					&table_data.td.pBackgroundImage,
					&table_data.td.bBackgroundNoSave);

	fe_EditorTableSetData(m_context, &table_data);

	if (table_data.td.pColorBackground)
		XP_FREE(table_data.td.pColorBackground);
	if (table_data.td.pBackgroundImage)
		XP_FREE(table_data.td.pBackgroundImage);

	EDT_FreeTableData(tmp);
}

Boolean
XFE_EditTableDialog::tablePropertiesValidate()
{
	EDT_AllTableData table_data;
	EDT_TableData*   t = &table_data.td;
	unsigned         errors[16];
	unsigned         nerrors = 0;
	EDT_TableData*   tmp = EDT_NewTableData();

	memset(&table_data, 0, sizeof(EDT_AllTableData));
	memcpy(&table_data.td, tmp, sizeof(EDT_TableData));

	tablePropertiesCommonSet(&table_data);

	if (m_table.number_rows_text != NULL) {
		t->iRows = fe_get_numeric_text_field(m_table.number_rows_text);
		t->iColumns = 
			fe_get_numeric_text_field(m_table.number_columns_text);
	 
		if (RANGE_CHECK(t->iRows, TABLE_MIN_ROWS, TABLE_MAX_ROWS))
			errors[nerrors++] = XFE_INVALID_TABLE_NROWS;
		if (RANGE_CHECK(t->iColumns, TABLE_MIN_COLUMNS, TABLE_MAX_COLUMNS))
			errors[nerrors++] = XFE_INVALID_TABLE_NCOLUMNS;
	}

	if (RANGE_CHECK(t->iBorderWidth, TABLE_MIN_BORDER, TABLE_MAX_BORDER))
		errors[nerrors++] = XFE_INVALID_TABLE_BORDER;
		
	if (RANGE_CHECK(t->iCellSpacing, TABLE_MIN_SPACING, TABLE_MAX_SPACING))
		errors[nerrors++] = XFE_INVALID_TABLE_SPACING;
		
	if (RANGE_CHECK(t->iCellPadding, TABLE_MIN_PADDING, TABLE_MAX_PADDING))
		errors[nerrors++] = XFE_INVALID_TABLE_PADDING;

	if (t->bWidthDefined) {
		if (t->bWidthPercent) {
			if (RANGE_CHECK(t->iWidth,
						TABLE_MIN_PERCENT_WIDTH, TABLE_MAX_PERCENT_WIDTH))
				errors[nerrors++] = XFE_INVALID_TABLE_WIDTH;
		} else {
			if (RANGE_CHECK(t->iWidth,
							TABLE_MIN_PIXEL_WIDTH, TABLE_MAX_PIXEL_WIDTH))
				errors[nerrors++] = XFE_INVALID_TABLE_WIDTH;
		}
	}
		
	if (t->bHeightDefined) {
		if (t->bHeightPercent) {
			if (RANGE_CHECK(t->iHeight,
						TABLE_MIN_PERCENT_HEIGHT, TABLE_MAX_PERCENT_HEIGHT))
				errors[nerrors++] = XFE_INVALID_TABLE_HEIGHT;
		} else {
			if (RANGE_CHECK(t->iHeight,
							TABLE_MIN_PIXEL_HEIGHT, TABLE_MAX_PIXEL_HEIGHT))
				errors[nerrors++] = XFE_INVALID_TABLE_HEIGHT;
		}
	}

	EDT_FreeTableData(tmp);

	if (nerrors > 0) {
		fe_editor_range_error_dialog(m_context, m_table.spacing_text,
									 errors, nerrors);
		return FALSE;
	}

	return TRUE;
}

static void
fe_create_background_group(Widget parent, char* name,
						   Arg* p_args, Cardinal p_n,
                           fe_bgGroup* bgGroup)
{
	Widget frame;
	Widget form;
	Widget use_image_chooser;
	Arg args[16];
	Cardinal n;
	Dimension height;
	
	frame = fe_CreateFrame(parent, name, p_args, p_n);

	n = 0;
	form = XmCreateForm(frame, "backgroundAttributes", args, n);
	XtManageChild(form);

	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	bgGroup->useColorToggle = XmCreateToggleButtonGadget(form, "useColor",
                                                         args, n);
	XtManageChild(bgGroup->useColorToggle);

	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, bgGroup->useColorToggle); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	bgGroup->useImageToggle = XmCreateToggleButtonGadget(form, "useImage",
                                                         args, n);
	XtManageChild(bgGroup->useImageToggle);

	XtVaGetValues(bgGroup->useColorToggle, XmNheight, &height, 0);

	n = 0;
	XtSetArg(args[n], XmNheight, height); n++;
	XtSetArg(args[n], XmNwidth, SWATCH_SIZE); n++;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNleftWidget, bgGroup->useColorToggle); n++;
	bgGroup->useColorSwatch = fe_CreateSwatchButton(form, "useColorSwatch",
                                                    args, n);
	XtManageChild(bgGroup->useColorSwatch);

	XtAddCallback(bgGroup->useColorToggle, XmNvalueChangedCallback,
				  fe_bg_group_use_color_cb, bgGroup->useColorSwatch);
	XtAddCallback(bgGroup->useColorSwatch, XmNactivateCallback,
				  fe_bg_group_swatch_cb, bgGroup->useColorToggle);

	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, bgGroup->useColorToggle); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNleftWidget, bgGroup->useImageToggle); n++;
	XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	bgGroup->useImageText = fe_CreateTextField(form, "useImageText", args, n);
	XtManageChild(bgGroup->useImageText);

	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, bgGroup->useImageText); n++;
	XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	use_image_chooser = XmCreatePushButtonGadget(form, "chooseImage",
												  args, n);
	XtManageChild(use_image_chooser);

	XtAddCallback(bgGroup->useImageToggle, XmNvalueChangedCallback,
				  fe_bg_use_image_cb, bgGroup->useImageText);
	XtAddCallback(use_image_chooser, XmNactivateCallback,
				  fe_bg_image_browse_cb, bgGroup->useImageText);
	
	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, bgGroup->useImageText); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNrightAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNrightWidget, use_image_chooser); n++;
	bgGroup->leaveImageToggle = XmCreateToggleButtonGadget(form, "leaveImage",
                                                           args, n);
	XtManageChild(bgGroup->leaveImageToggle);
}

static void
fe_bg_group_set(fe_bgGroup* group,
				LO_Color* color, char* bg_image, XP_Bool /*leave_image*/)
{
	Widget  use_color_toggle = group->useColorToggle;
	Widget  use_color_swatch = group->useColorSwatch;
	Widget  use_image_toggle = group->useImageToggle;
	Widget  use_image_text = group->useImageText;
	Widget  leave_image_toggle = group->leaveImageToggle;
	
	if (use_color_toggle != NULL)
		XmToggleButtonGadgetSetState(use_color_toggle, (color != NULL), FALSE);

	if (use_color_swatch != NULL)
		fe_NewSwatchSetColor(use_color_swatch, color);

	if (use_image_toggle != NULL) {
		XmToggleButtonGadgetSetState(use_image_toggle, (bg_image != NULL),
									 FALSE);
	}

	if (bg_image == NULL)
		bg_image = "";

	if (use_image_text != NULL)
	    fe_TextFieldSetString(use_image_text, bg_image, FALSE);

	if (leave_image_toggle != NULL)
		XmToggleButtonGadgetSetState(leave_image_toggle, (leave_image_toggle != NULL),
									 FALSE);
}

static void
fe_bg_group_get(fe_bgGroup* group,
				LO_Color** color_r, char** bg_image_r, XP_Bool* leave_image_r)
{
	XP_Bool use_color = FALSE;
	XP_Bool use_image = FALSE;
	XP_Bool leave_image = FALSE;

    XP_ASSERT(group);
    if (group == 0)
        return;

	Widget  use_color_toggle = group->useColorToggle;
	Widget  use_color_swatch = group->useColorSwatch;
	Widget  use_image_toggle = group->useImageToggle;
	Widget  use_image_text = group->useImageText;
	Widget  leave_image_toggle = group->leaveImageToggle;
	
	if (use_color_toggle != NULL)
		use_color = (XP_Bool)XmToggleButtonGadgetGetState(use_color_toggle);

	if (use_image_toggle != NULL)
		use_image = (XP_Bool)XmToggleButtonGadgetGetState(use_image_toggle);

	if (leave_image_toggle != NULL)
		leave_image=(XP_Bool)XmToggleButtonGadgetGetState(leave_image_toggle);

	if (color_r != NULL) {
		LO_Color* foo = NULL;
		if (use_color && use_color_swatch != NULL) {
			foo = XP_NEW(LO_Color);
			fe_NewSwatchGetColor(use_color_swatch, foo);
		}
		*color_r = foo;
	}

	if (bg_image_r != NULL) {
		char* tmp = NULL;
		if (use_image && use_image_text != NULL) {
			tmp = fe_GetTextField(use_image_text);
			if (tmp != NULL) {
				fe_StringTrim(tmp);
				if (tmp[0] == '\0') {
					XP_FREE(tmp);
					tmp = NULL;
				}
			}
		}
			
		*bg_image_r = tmp;
	}

	if (leave_image_r != NULL)
		*leave_image_r = leave_image;
}

static Widget
fe_CreateFolder(Widget parent, char* name, Arg*, Cardinal)
{
	Widget folder;

	folder = XtVaCreateWidget(name, xmlFolderWidgetClass, parent,
							  XmNshadowThickness, 2,
							  XmNtopAttachment, XmATTACH_FORM,
							  XmNleftAttachment, XmATTACH_FORM,
							  XmNrightAttachment, XmATTACH_FORM,
							  XmNbottomAttachment, XmATTACH_FORM,
#ifdef ALLOW_TAB_ROTATE
							  XmNtabPlacement, XmFOLDER_LEFT,
							  XmNrotateWhenLeftRight, FALSE,
#endif /* ALLOW_TAB_ROTATE */
							  XmNbottomOffset, 3,
							  XmNspacing, 1,
							  NULL);

	return folder;
}

static void
fe_table_tbr_set(MWContext* context, Widget toggle, Widget text, Widget radio,
                 Boolean enabled, unsigned numeric, Boolean second_one)
{
	XmToggleButtonGadgetSetState(toggle, enabled, FALSE);
	if (text != NULL) {
	    char buf[32];

		sprintf(buf, "%d", numeric);

		fe_TextFieldSetString(text, buf, FALSE);
		fe_TextFieldSetEditable(context, text, enabled);
	}
	if (radio != NULL) {
		fe_SimpleRadioGroupSetWhich(radio, second_one);
		fe_SimpleRadioGroupSetSensitive(radio, enabled);
	}
}

