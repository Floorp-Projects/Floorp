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
   AdvSearchDialog.h -- dialog for specifying options to message search
   Created: Akkana Peck <akkana@netscape.com>, 28-Jun-98.
 */



#ifndef _xfe_EditTableDialog_h
#define _xfe_EditTableDialog_h

#include "xp_core.h"
#include "Dialog.h"

#include "lo_ele.h"     // for LO_Color

class XFE_Frame;
class EDT_AllTableData;

typedef struct fe_bgGroupStruct
{
    Widget useColorToggle;
    Widget useColorSwatch;
    Widget useImageToggle;
    Widget useImageText;
    Widget leaveImageToggle;
} fe_bgGroup;

//
// I don't think we really need to save all these variables.
// 2DO: clean out the ones we don't need to save.
//
typedef struct fe_EditorTablesTableStruct
{
	Widget number_rows_text;
	Widget number_columns_text;
	Widget line_width_toggle;
	Widget line_width_text;
	Widget spacing_text;
	Widget padding_text;
	Widget width_toggle;
	Widget width_text;
	Widget width_units;
	Widget height_toggle;
	Widget height_text;
	Widget height_units;
	fe_bgGroup bg_group;
	Widget choose_color;
	LO_Color color_value;
	Widget caption_toggle;
	Widget caption_type;
	Widget alignBox;
    Boolean inserting;
} fe_EditorTablesTableStruct; 

typedef struct fe_EditorTablesRowStruct
{
	Widget horizontal_alignment;
	Widget vertical_alignment;
	Widget bg_group;
	LO_Color color_value;
} fe_EditorTablesRowStruct;

typedef struct fe_EditorTablesCellStruct
{
	Widget number_rows_text;
	Widget number_columns_text;
	Widget line_width_text;
	Widget horizontal_alignment;
	Widget vertical_alignment;
	Widget header_style;
	Widget wrap_text;
	Widget width_toggle;
	Widget width_text;
	Widget width_units;
	Widget height_toggle;
	Widget height_text;
	Widget height_units;
	fe_bgGroup bg_group;
    Widget use_color_toggle;
	LO_Color color_value;
    Widget option_menu;
} fe_EditorTablesCellStruct;

class XFE_EditTableDialog: public XFE_Dialog
{
public:
    XFE_EditTableDialog(Widget parent, char* name, MWContext* context,
                        Boolean createIt, int tab_number = 0);

    virtual ~XFE_EditTableDialog();

    void post();

private:
    MWContext* m_context;
    XP_Bool m_doneWithLoop;

    fe_EditorTablesTableStruct m_table;
    fe_EditorTablesCellStruct m_cell;

    Boolean m_newTable;

    void ok();
    void cancel();
    void changeSelection(Boolean);

    Widget tablePropertiesCreate(Widget parent);
    void tablePropertiesInit();
    Widget cellPropertiesCreate(Widget parent);
    void cellPropertiesInit();
    void cellPropertiesSet();
    Boolean cellPropertiesValidate();
    void tablePropertiesCommonSet(EDT_AllTableData* table_data);
    Boolean tablePropertiesValidate();
    void tablePropertiesSet();
    void cellPropertiesSetValidateCommon(EDT_TableCellData* cell_data);

    static void ok_cb(Widget, XtPointer, XtPointer);
    static void apply_cb(Widget, XtPointer, XtPointer);
    static void cancel_cb(Widget, XtPointer, XtPointer);
    static void table_toggle_cb(Widget, XtPointer, XtPointer);
    static void cell_toggle_cb(Widget, XtPointer, XtPointer);
    static void cell_selection_cb(Widget, XtPointer, XtPointer);
};

#endif /* _xfe_EditTableDialog_h */
