/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

/*----------------------------------------------------------------------*/
/*																		*/
/* Name:		<Xfe/ComboBox.h>										*/
/* Description:	XfeComboBox widget public header file.					*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/

#ifndef _XfeComboBox_h_							/* start ComboBox.h		*/
#define _XfeComboBox_h_

#include <Xfe/Manager.h>

XFE_BEGIN_CPLUSPLUS_PROTECTION

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeComboBox resource names											*/
/*																		*/
/*----------------------------------------------------------------------*/
#define XmNtextActivateCallback			"textActivateCallback"

#define XmNarrow						"arrow"
#define XmNgetTextFunc					"getTextFunc"
#define XmNlistFontList					"listFontList"
#define XmNsetTextProc					"setTextProc"
#define XmNshareShell					"shareShell"
#define XmNshell						"shell"
#define XmNtitleShadowThickness			"titleShadowThickness"
#define XmNtitleShadowType				"titleShadowType"

#define XmCGetTextFunc					"GetTextFunc"
#define XmCListFontList					"ListFontList"
#define XmCSetTextProc					"SetTextProc"
#define XmCShareShell					"ShareShell"

/* Things that conflict with Motif 2.x */
#if XmVersion < 2000
#define XmNcomboBoxType					"comboBoxType"
#define XmCComboBoxType					"ComboBoxType"
#define XmRComboBoxType					"ComboBoxType"
#endif

/* Things that conflict elsewhere */
#ifndef XmNlist
#define XmNlist							"list"
#endif

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeComboBox reasonable defaults for some resources					*/
/*																		*/
/*----------------------------------------------------------------------*/
#define XfeDEFAULT_COMBO_BOX_LIST_MARGIN_HEIGHT			2
#define XfeDEFAULT_COMBO_BOX_LIST_MARGIN_WIDTH			2
#define XfeDEFAULT_COMBO_BOX_LIST_SPACING				2
#define XfeDEFAULT_COMBO_BOX_LIST_VISIBLE_ITEM_COUNT	10

/*----------------------------------------------------------------------*/
/*																		*/
/* XmRComboBoxType														*/
/*																		*/
/*----------------------------------------------------------------------*/
enum
{
	XmCOMBO_BOX_EDITABLE,						/*						*/
	XmCOMBO_BOX_READ_ONLY						/*						*/
};
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Callback Reasons														*/
/*																		*/
/*----------------------------------------------------------------------*/
enum
{
	XmCR_COMBO_BOX_TEXT_ACTIVATE = XmCR_XFE_LAST_REASON + 199, /* Text activate */
	XmCR_COMBO_BOX_TEXT_COMPLETE,				/* Text complete		*/
	XmCR_COMBO_BOX_TEXT_SELECT					/* Text select			*/
};

/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeComboBoxSetTextProc												*/
/*																		*/
/* Type for XmNsetTextProc resource.  If set, this procedure will be	*/
/* called instead of XmTextFieldSetString().  You can use this to 		*/
/* work around Motif bugs or locale customizations.           			*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef void	(*XfeComboBoxSetTextProc)		(Widget			w,
												 char *			text);
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeComboBoxGetTextFunc												*/
/*																		*/
/* Type for XmNgetTextFunc resource.  If set, this function will be		*/
/* called instead of XmTextFieldGetString().  You can use this to 		*/
/* work around Motif bugs or locale customizations.           			*/
/*																		*/
/* Use XtFree() on the returned string.									*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef char *	(*XfeComboBoxGetTextFunc)		(Widget			w);
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeBox class names													*/
/*																		*/
/*----------------------------------------------------------------------*/
externalref WidgetClass xfeComboBoxWidgetClass;

typedef struct _XfeComboBoxClassRec *			XfeComboBoxWidgetClass;
typedef struct _XfeComboBoxRec *				XfeComboBoxWidget;

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeBox subclass test macro											*/
/*																		*/
/*----------------------------------------------------------------------*/
#define XfeIsComboBox(w)	XtIsSubclass(w,xfeComboBoxWidgetClass)

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeComboBox public methods											*/
/*																		*/
/*----------------------------------------------------------------------*/
extern Widget
XfeCreateComboBox				(Widget			pw,
								 String			name,
								 Arg *			av,
								 Cardinal		ac);
/*----------------------------------------------------------------------*/
extern void
XfeComboBoxAddItem				(Widget			w,
								 XmString		item,
								 int			position);
/*----------------------------------------------------------------------*/
extern void
XfeComboBoxAddItemUnique		(Widget			w,
								 XmString		item,
								 int			position);
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XfeComboBox - XmCOMBO_BOX_EDITABLE methods							*/
/*																		*/
/*----------------------------------------------------------------------*/
extern String
XfeComboBoxGetTextString		(Widget			w);
/*----------------------------------------------------------------------*/
extern void
XfeComboBoxSetTextString		(Widget			w,
								 String			string);
/*----------------------------------------------------------------------*/

XFE_END_CPLUSPLUS_PROTECTION

#endif											/* end ComboBox.h		*/
