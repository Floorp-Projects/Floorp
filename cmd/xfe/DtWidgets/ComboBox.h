/*
 * ComboBox.h, Interleaf, 16aug93 2:37pm Version 1.1.
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
 *  (C) Copyright 1991,1992, 1993
 *  Interleaf, Inc.
 *  Nine Hillside Avenue, Waltham, MA  02154
 *
 * ComboBox.h: 
 * 
 * Public header file for DtComboBoxWidget.
 */
#ifndef _ComboBox_h
#define _ComboBox_h

#ifdef __cplusplus
extern "C" {
#endif

#ifndef AA
#define AA(args) ()
#endif

#include <Xm/Xm.h>

#ifndef XmNarrowSize
#define XmNarrowSize		"arrowSize"
#endif
#ifndef XmNarrowSpacing
#define XmNarrowSpacing		"arrowSpacing"
#endif
#define XmNarrowType		"arrowType"
#ifndef XmNlist
#define XmNlist			"list"
#endif
#define XmNlistFontList		"listFontList"
#define XmNpoppedUp		"poppedUp"
#ifndef XmNselectedPosition
#define XmNselectedPosition	"selectedPosition"
#endif
#ifndef XmNselectedItem
#define XmNselectedItem		"selectedItem"
#endif
#ifndef XmNtextField
#define XmNtextField		"textField"
#endif
#define XmNtype			"type"
#define XmNupdateLabel		"updateLabel"
#define XmNmoveSelectedItemUp	"moveSelectedItemUp"
#define XmNnoCallbackForArrow	"noCallbackForArrow"

#ifndef XmCArrowSize
#define XmCArrowSize		"ArrowSize"
#endif
#ifndef XmCArrowSpacing
#define XmCArrowSpacing		"ArrowSpacing"
#endif
#define XmCArrowType		"ArrowType"
#define XmCHorizontalSpacing	"HorizontalSpacing"
#ifndef XmCList
#define XmCList			"List"
#endif
#define XmCListFontList		"ListFontList"
#define XmCPoppedUp		"PoppedUp"
#ifndef XmCSelectedPosition
#define XmCSelectedPosition	"SelectedPosition"
#endif
#ifndef XmCSelectedItem
#define XmCSelectedItem		"SelectedItem"
#endif
#define XmCType			"Type"
#ifndef XmCTextField
#define XmCTextField		"TextField"
#endif
#define XmCUpdateLabel		"UpdateLabel"
#define XmCVerticalSpacing	"VerticalSpacing"
#define XmCMoveSelectedItemUp	"MoveSelectedItemUp"
#define XmCNoCallbackForArrow	"NoCallbackForArrow"

#define XmRArrowType		"ArrowType"
#define XmRType			"Type"

/* XmNorientation defines */
#define XmLEFT		0
#define XmRIGHT		1

/* ArrowType defines */
#define XmMOTIF		0
#define XmWINDOWS	1

/* XmNtype defines */
#define XmDROP_DOWN_LIST_BOX	0
#define XmDROP_DOWN_COMBO_BOX	1

/* ComboBox callback info */
#ifndef XmNselectionCallback
#define XmNselectionCallback	"selectionCallback"
#endif
#define XmNmenuPostCallback	"menuPostCallback"
#define XmCR_SELECT	    128 /* Large #, so no collisions with XM */
#define XmCR_MENU_POST      129 /* Large #, so no collisions with XM */

typedef struct {
   int 	     reason;
   XEvent    *event;
   XmString  item_or_text;
   int       item_position;
} DtComboBoxCallbackStruct;

extern WidgetClass dtComboBoxWidgetClass;

typedef struct _DtComboBoxClassRec *DtComboBoxWidgetClass;
typedef struct _DtComboBoxRec      *DtComboBoxWidget;

/*
 * External functions which manipulate the ComboBox list of items.
 */
extern Widget DtCreateComboBox (Widget parent, char *name,
				   Arg *arglist, int num_args);
extern void DtComboBoxAddItem (Widget combo, XmString item,
				  int pos, Boolean unique);
extern void DtComboBoxDeletePos (Widget combo, int pos);
extern void DtComboBoxDeleteAllItems(Widget combo);
extern void DtComboBoxSetItem (Widget combo, XmString item);
extern void DtComboBoxSelectItem (Widget combo, XmString item);

/* xfe additions */
extern void DtComboBoxAddItemSelected (Widget combo, XmString item, int pos, Boolean unique);
extern void DtComboBoxDeleteAllItems (Widget combo);


#ifdef __cplusplus
};
#endif

#endif	/* _ComboBox_h */
