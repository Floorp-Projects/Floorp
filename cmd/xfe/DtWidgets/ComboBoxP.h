/*
 * ComboBoxP.h, Interleaf, 16aug93 2:37pm Version 1.1.
 */
/***********************************************************
Copyright 1993 Interleaf, Inc.

Permission to use, copy, modify, and distribute this software
and its documentation for any purpose without fee is granted,
provided that the above copyright notice appear in all copies
and that both copyright notice and this permission notice appear
in supporting documentation, and that the name of Interleaf not
be used in advertising or publicly pertaining to distribution of
the software without specific written prior permission.

Interleaf makes no representation about the suitability of this
software for any purpose. It is provided "AS IS" without any
express or implied warranty. 
******************************************************************/

/*
 * (C) Copyright 1991,1992, 1993
 * Interleaf, Inc.
 * Nine Hillside Avenue, Waltham, MA  02154
 *
 * ComboBoxP.h:
 * 
 * Private header file for DtComboBoxWidget.
 */
#ifndef _ComboBoxP_h
#define _ComboBoxP_h

#ifndef AA
#define AA(args) ()
#endif

#include <X11/IntrinsicP.h>
#include <X11/ShellP.h>
#include <Xm/DrawnB.h>
#include <Xm/ArrowB.h>
#include <Xm/TextFP.h>
#include <Xm/Label.h>
#include <Xm/Frame.h>
#include <Xm/ListP.h>
#include <Xm/Separator.h>
#include <Xm/ScrolledWP.h>
#include <Xm/ScrollBarP.h>

#include <Xm/ManagerP.h>
#include "ComboBox.h"

/*
 * External definitions of syn_resources for our list widget.
 */
#define SYN_RESOURCE_AA AA((Widget w, int resource_offset, XtArgVal *value))
extern void _DtComboBoxGetArrowSize		SYN_RESOURCE_AA;
extern void _DtComboBoxGetLabelString		SYN_RESOURCE_AA;
extern void _DtComboBoxGetListItemCount		SYN_RESOURCE_AA;
extern void _DtComboBoxGetListItems		SYN_RESOURCE_AA;
extern void _DtComboBoxGetListFontList		SYN_RESOURCE_AA;
extern void _DtComboBoxGetListMarginHeight	SYN_RESOURCE_AA;
extern void _DtComboBoxGetListMarginWidth	SYN_RESOURCE_AA;
extern void _DtComboBoxGetListSpacing		SYN_RESOURCE_AA;
extern void _DtComboBoxGetListTopItemPosition	SYN_RESOURCE_AA;
extern void _DtComboBoxGetListVisibleItemCount	SYN_RESOURCE_AA;

#define ARROW_MULT	    .45
#define ARROW_MIN	    13
#define MARGIN		    2
#define LABEL_PADDING	    2
#define LABEL_SHADOW	    2
#define TEXT_FIELD_SHADOW   1

/* 
 * Class Records
 */
typedef struct {
    Boolean junk;  /* Need something */
} DtComboBoxClassPart;

typedef struct _DtComboBoxClassRec {
    CoreClassPart	    core_class;
    CompositeClassPart	    composite_class;
    ConstraintClassPart	    constraint_class;
    XmManagerClassPart	    manager_class;
    DtComboBoxClassPart	    combo_box_class;
} DtComboBoxClassRec;

extern DtComboBoxClassRec dtComboBoxClassRec;


/*
 * Instance Record.
 */
typedef struct _DtComboBoxPart {
    /* Private data */
    Widget arrow;
    Widget shell;
    Widget frame;
    Widget label;
    Widget sep;
    Dimension old_width;
    Dimension old_height;
    Dimension label_max_length;
    Dimension label_max_height;
    /* 
     * max_shell_width is the width that is needed to hold the
     * list if the longest item was visible.  We then use this
     * width to figure out if the shell is not wide enough,
     * when it gets popped on the screen.  This is needed in case the
     * combo-box resizes, or if items changes (list will resize).
     * Sometimes we change the size of the shell to fit on the screen, or
     * to make it at least as large as the combo_box.  The next time we pop
     * the shell up the size may be different; therefore, we set the shell
     * size to the maximum everytime it gets popped up, then we will
     * make adjustments, only if needed.
     * This value gets saved every time the user updates XmNitems.
     */
    Dimension max_shell_width;
    Dimension max_shell_height;

    /* ComboBox specific resources */
    XtCallbackList activate_callback;
    unsigned char alignment;
    Dimension arrow_size;
    Dimension arrow_spacing;
    unsigned char arrow_type;
    short text_columns;
    XtCallbackList focus_callback;
    Dimension horizontal_spacing;
    int item_count;
    XmStringTable items;
    XmString label_string;
    Widget list;
    XmFontList list_font_list;
    Dimension list_margin_height;
    Dimension list_margin_width;
    Dimension list_spacing;
    XtCallbackList losing_focus_callback;
    Dimension margin_height;
    Dimension margin_width;
    unsigned int text_max_length;
    XtCallbackList menu_post_callback;
    unsigned char orientation;
    Boolean popped_up;
    Boolean recompute_size;
    XmString selected_item;
    int selected_position;
    XtCallbackList selection_callback;
    Widget text;
    int top_item_position;
    unsigned char type;
    Boolean update_label;
    Dimension vertical_spacing;
    int visible_item_count;
    Boolean move_selecteditem_up;
    Boolean no_callback_for_arrow;
} DtComboBoxPart;


typedef struct _DtComboBoxRec {
    CorePart		core;
    CompositePart	composite;
    ConstraintPart	constraint;
    XmManagerPart	manager;
    DtComboBoxPart	combo_box;
} DtComboBoxRec;


/*
 * Error defines.
 */
#define COMBO_ALIGNMENT "DtComboButtonWidget: Invalid alignment resource (defaulting to XmALIGNMENT_CENTER)."
#define COMBO_MARGIN_HEIGHT "DtComboButtonWidget: Invalid marginHeight resource (defaulting to 2)."
#define COMBO_MARGIN_WIDTH "DtComboButtonWidget: Invalid marginHeight resource (defaulting to 2)."
#define COMBO_HORIZONTAL_SPACING "DtComboButtonWidget: Invalid horizontalSpacing resource (defaulting to 0)."
#define COMBO_VERTICAL_SPACING "DtComboButtonWidget: Invalid verticalSpacing resource (defaulting to 0)."
#define COMBO_ORIENTATION "DtComboButtonWidget: Invalid orientation resource (defaulting to XmRIGHT)."
#define COMBO_ITEM_COUNT "DtComboButtonWidget: Invalid itemCount resource (defaulting to 0)."
#define COMBO_VISIBLE_ITEM "DtComboButtonWidget: Invalid selectedPosition resource (defaulting to 0)."
#define COMBO_TEXT "DtComboButtonWidget: Unable to set textField resource."
#define COMBO_ITEM_COUNT "DtComboButtonWidget: Invalid itemCount resource (defaulting to 0)."
#define COMBO_SET_ITEM "DtComboBoxWidget: Unable to find item to set (DtComboBoxSetItem)."
#define COMBO_SELECT_ITEM "DtComboBoxWidget: Unable to find item to select (XmComboBoxSelectItem)."


#endif /* _XmComboBoxP_h */
